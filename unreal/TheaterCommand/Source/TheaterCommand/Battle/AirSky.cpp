#include "Battle/AirSky.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iterator>

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
	// (the shaded side bluer and the base a little darker, the art director's brief on build 1768: through the battle's
	// grade, red 1.08 and blue 0.87 and the colour thinned, 162, 170, 184 came out a neutral grey, "no blue anywhere";
	// her marks a shade side of 125 to 160 at blue over red +8 to +18, an underside of 95 to 125)
	const Rgb SunSide = Rgb::FromPicture(252, 250, 245);
	const Rgb ShadeSide = Rgb::FromPicture(150, 164, 192);
	const Rgb BaseSide = Rgb::FromPicture(122, 130, 148);
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

		/** The nearest whole number, halves away from nought, as std::round, for the small numbers of the repeating
		 * square: one conversion, where std::round is a library call on a plain x86-64 build (the painter calls it for
		 * every cloud it draws, and it was a sixth of a paint's time in the cloud test). */
		double Nearest(double V) { return std::fabs(V) < 4.0e15 ? static_cast<double>(static_cast<int64_t>(V + (V >= 0.0 ? 0.5 : -0.5))) : V; }

		/** The shortest way across the repeating square (a difference of laid places, folded into half a side). */
		double Across(double D) { return D - Period * Nearest(D / Period); }

		std::atomic<uint64_t> Layings { 0 };
	}

	const Vec Wind(3.0, -2.0, 0.0);

	void WindLattice(const Vec& W, int& OutX, int& OutY)
	{
		OutX = 2;
		OutY = 3;
		const double Flat = std::sqrt(W.X * W.X + W.Y * W.Y);
		if (!(Flat > 1e-6))
		{
			return;
		}
		const double Long = std::sqrt(13.0) / Flat;
		const int AX = static_cast<int>(std::lround(-W.Y * Long)), AY = static_cast<int>(std::lround(W.X * Long));
		if (AX != 0 || AY != 0)
		{
			OutX = AX;
			OutY = AY;
		}
	}

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
		return this->Wind * std::max(0.0, AirSeconds);
	}

	Vec Field::ShiftNear(int CloudIndex, double X, double Y) const
	{
		const Cloud& C = Clouds[static_cast<size_t>(CloudIndex)];
		return Vec(Period * Nearest((X - C.At.X) / Period), Period * Nearest((Y - C.At.Y) / Period), 0.0);
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
		// (ground known to stand no higher than GroundTop lifts no cloud whose place is 310 m over it, as the lift below
		// works out: the lattice is not walked. Over the board's own flat land it was 200,000 asks a paint; the
		// pre-check on round 14)
		if (GroundTop >= 0.0 && GroundTop + 310.0 <= C.At.Z)
		{
			return 0.0;
		}
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
			// going first. Only small ones stay in it: a big one kept there, up to 3.5 km across since the clouds round,
			// covered a quarter of the clearing over Midway's fleet)
			const double Edge = std::max(0.0, R - 0.45 * C.W);
			const double Stays = 0.15 * Smooth((1500.0 - C.W) / 900.0);
			Keep = Stays + (1.0 - Stays) * Smooth((Edge - 1500.0) / 2500.0);
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
					// (a small one drawn out along its way: W / Long across it)
					if (C.Long > 1.001)
					{
						const double Lw = std::cos(C.LongWay), Lp = std::sin(C.LongWay);
						const double Along = Ex * Lw + Ey * Lp, Aside = (Ey * Lw - Ex * Lp) * C.Long;
						if (Along * Along + Aside * Aside > R * R)
						{
							continue;
						}
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

	void MakeFragments(std::vector<uint8_t>& OutRgba)
	{
		const int N = FragmentSize, Sheet = FragmentCells * FragmentSize, Cells = FragmentCells * FragmentCells;
		OutRgba.assign(static_cast<size_t>(Sheet) * static_cast<size_t>(Sheet) * 4, 0);
		TheaterFx::Dice Roll(0xC10D5EED5EEDull);
		const double Px = 2.0 / N; // (a pixel, in a cell's half-widths)
		struct Lump
		{
			double X = 0.0, Y = 0.0, R = 0.0;
		};
		const size_t Area = static_cast<size_t>(N) * N;
		std::vector<double> High(Area), Inside(Area), Soft(Area), Blur(Area), Pass(Area);
		// (smooth noise at three scales, 0 to 1, for the billow over the lumps and the torn edges)
		auto Billow = [](double X, double Y, uint64_t Seed)
		{
			return 0.55 * Noise(X + 4.0, Y + 4.0, 0.16, 1 << 20, Seed) + 0.3 * Noise(X + 4.0, Y + 4.0, 0.07, 1 << 20, Seed + 1) + 0.15 * Noise(X + 4.0, Y + 4.0, 0.03, 1 << 20, Seed + 2);
		};
		// (a box blur 2 Wide + 1 pixels wide, across and then down, by a running sum, the edge held)
		auto BoxBlur = [&](std::vector<double>& Grid, int Wide)
		{
			for (int Turn = 0; Turn < 2; ++Turn)
			{
				const bool bAcross = Turn == 0;
				std::vector<double>& From = bAcross ? Grid : Pass;
				std::vector<double>& To = bAcross ? Pass : Grid;
				auto Index = [&](int A, int B) { return bAcross ? static_cast<size_t>(A) * N + B : static_cast<size_t>(B) * N + A; };
				for (int A = 0; A < N; ++A)
				{
					double Sum = 0.0;
					for (int K = -Wide; K <= Wide; ++K)
					{
						Sum += From[Index(A, std::max(0, std::min(N - 1, K)))];
					}
					for (int B = 0; B < N; ++B)
					{
						To[Index(A, B)] = Sum / (2 * Wide + 1);
						Sum += From[Index(A, std::min(N - 1, B + Wide + 1))] - From[Index(A, std::max(0, B - Wide))];
					}
				}
			}
		};
		// (the shapes' lumps drawn in the order the sheet first had them, ten tops, four domes and two feet, so that each
		// shape kept is the one the art director passed on round 18c; the sheet keeps eight of the tops, two domes and the
		// two feet, and in place of the rest four lids, a deck's cells, each from its own seed: her round 18d steer)
		constexpr int Legacy = 16, LegacyTops = 10, LegacyDomes = 4;
		std::vector<std::vector<Lump>> LumpsOf(Legacy);
		for (int Was = 0; Was < Legacy; ++Was)
		{
			// (a dome, a far cloud's whole body on its flat underside, or a foot, a cloud's one base: both have the flat
			// underside; the rest are tops)
			const bool bFoot = Was >= LegacyTops + LegacyDomes;
			const bool bBase = Was >= LegacyTops;
			// (a base's flat underside, in half-widths from the cell's middle)
			const double Floor = -FragmentFloor;
			std::vector<Lump>& Lumps = LumpsOf[static_cast<size_t>(Was)];
			// (how far inside the lumps' outlines a point lies, the nearest outline; a base's underside cut straight)
			auto InsideOf = [&Lumps, bBase, Floor](double X, double Y)
			{
				double In = -1.0;
				for (const Lump& L : Lumps)
				{
					In = std::max(In, L.R - std::sqrt((X - L.X) * (X - L.X) + (Y - L.Y) * (Y - L.Y)));
				}
				return bBase ? std::min(In, Y - Floor) : In;
			};
			// (a lump on the body's edge the way A from the middle: its middle that far in from the edge, so that it
			// stands out of the body by the rest of it, never a ball apart from it)
			auto OnEdge = [&Lumps, &InsideOf](double A, double R, double In)
			{
				double D = 0.95;
				while (D > 0.0 && InsideOf(D * std::cos(A), D * std::sin(A)) < 0.0)
				{
					D -= 0.01;
				}
				D = std::min(D - In * R, 0.9 - R);
				Lumps.push_back({ D * std::cos(A), D * std::sin(A), R });
			};
			if (!bBase)
			{
				// (a body, lumps of middling size round it, larger and fewer on the side away from the top; then
				// smaller lumps along the top's edge and a few along its sides: a cauliflower, never a ball)
				Lumps.push_back({ Roll.U(-0.06, 0.06), Roll.U(-0.14, -0.02), Roll.U(0.38, 0.46) });
				const int Middling = 5 + static_cast<int>(Roll.U() * 4.0);
				const double Start = Roll.U(0.0, 2.0 * Pi);
				for (int J = 0; J < Middling; ++J)
				{
					const double A = Start + 2.0 * Pi * (J + Roll.U(-0.3, 0.3)) / Middling;
					const bool bUpper = std::sin(A) > 0.0;
					OnEdge(A, bUpper ? Roll.U(0.17, 0.27) : Roll.U(0.22, 0.32), Roll.U(0.45, 0.7));
				}
				const int Turrets = 10 + static_cast<int>(Roll.U() * 8.0);
				for (int J = 0; J < Turrets; ++J)
				{
					const double A = J < 3 ? Roll.U(0.0, 2.0 * Pi) : Roll.U(0.1 * Pi, 0.9 * Pi);
					OnEdge(A, Roll.U(0.05, 0.12), Roll.U(0.4, 0.6));
				}
			}
			else if (bFoot)
			{
				// (a foot: a solid slab on the flat underside, narrowing a little upward, its ends uneven (worked out with
				// the pixels), fading out upward into the body's lowest lumps, which overlap it; a row of lumps of mixed
				// sizes along it, the middle ones the largest; and at each end a few small ones strewn, so that the end is
				// torn, never rounded like a pill's)
				const int Row = 7 + static_cast<int>(Roll.U() * 3.0);
				for (int J = 0; J < Row; ++J)
				{
					const double X = -0.62 + 1.24 * (J + Roll.U(0.25, 0.75)) / Row;
					const double R = Roll.U(0.2, 0.28) * (1.0 - 0.4 * X * X);
					Lumps.push_back({ X, Floor + R * Roll.U(0.5, 0.85), R });
				}
				for (const double Way : { -1.0, 1.0 })
				{
					const int Strewn = 2 + static_cast<int>(Roll.U() * 3.0);
					for (int J = 0; J < Strewn; ++J)
					{
						Lumps.push_back({ Way * Roll.U(0.62, 0.8), Floor + Roll.U(0.03, 0.2), Roll.U(0.04, 0.08) });
					}
				}
			}
			else
			{
				// (a dome on a flat underside: a row of wide lumps sitting on it, a body over them reaching well up into
				// the cloud over it, so that the base is the cloud's own underside and no slab hung under it; turrets
				// along the top)
				const int Row = 3 + static_cast<int>(Roll.U() * 2.0);
				for (int J = 0; J < Row; ++J)
				{
					const double R = Roll.U(0.3, 0.4);
					const double X = Clamp(-0.6 + 1.2 * (J + Roll.U(0.3, 0.7)) / Row, -0.88 + R, 0.88 - R);
					Lumps.push_back({ X, Floor + Roll.U(0.2, 0.3), R });
				}
				Lumps.push_back({ Roll.U(-0.15, 0.15), Roll.U(0.0, 0.12), Roll.U(0.36, 0.44) });
				const int Over = 3 + static_cast<int>(Roll.U() * 3.0);
				for (int J = 0; J < Over; ++J)
				{
					OnEdge(Roll.U(0.2 * Pi, 0.8 * Pi), Roll.U(0.16, 0.26), Roll.U(0.45, 0.65));
				}
				const int Turrets = 6 + static_cast<int>(Roll.U() * 6.0);
				for (int J = 0; J < Turrets; ++J)
				{
					OnEdge(Roll.U(0.15 * Pi, 0.85 * Pi), Roll.U(0.05, 0.11), Roll.U(0.4, 0.6));
				}
			}
		}
		const int Tops = Cells - FragmentBases - FragmentFeet - FragmentLids;
		for (int Cell = 0; Cell < Cells; ++Cell)
		{
			const bool bLid = Cell >= Cells - FragmentLids;
			const bool bFoot = !bLid && Cell >= Tops + FragmentBases;
			const bool bBase = !bLid && Cell >= Tops;
			const int Was = Cell < Tops ? Cell : bLid ? Legacy + (Cell - (Cells - FragmentLids)) : bFoot ? LegacyTops + LegacyDomes + (Cell - Tops - FragmentBases) : LegacyTops + (Cell - Tops);
			const uint64_t Seed = 0xF1A9ull + 977ull * static_cast<uint64_t>(Was);
			const double Floor = -FragmentFloor;
			const std::vector<Lump>& Lumps = LumpsOf[static_cast<size_t>(bLid ? 0 : Was)];
			auto Byte = [](double V) { return static_cast<uint8_t>(Clamp(V, 0.0, 1.0) * 255.0 + 0.5); };
			if (bLid)
			{
				// A lid: one of a deck's cells (the art director's round 18d steer, Bremen's winter stratocumulus): a broad,
				// flat body whose outline is uneven and frays thin, never a wall; over it broad shallow bumps drawn out across
				// the picture in rolls, each some five times as wide as it stands, small bumps on them, and darker seams
				// between. Its light: red, the low winter sun across it toward the sheet's top, 13 degrees up: the faces
				// toward the sun bright, those away and the seams grey; green, the sun behind the eye, nearly even; blue,
				// seen from under it or against the sun, a mid grey, darker in each bump's middle, lighter where the cloud
				// thins, in the seams and at its frayed edge, so that from under the deck no lump is ringed in white.
				const int LidX = Cell % FragmentCells, LidY = Cell / FragmentCells;
				const double Sine = std::sin(13.0 * Pi / 180.0), Cosine = std::cos(13.0 * Pi / 180.0);
				// (the cell's height: a broad low dome, its slope two in five at most toward its edge, so that it is some
				// five times as wide as it stands; small bumps on it, a little drawn out across)
				auto Height = [Seed](double U, double V)
				{
					const double Dome = std::max(0.0, 1.0 - (U * U / (0.86 * 0.86) + V * V / (0.76 * 0.76)));
					return 0.28 * Dome + 0.07 * Noise(U * 0.85 + 4.0, V * 1.2 + 4.0, 0.24, 1 << 20, Seed + 21) + 0.025 * Noise(U + 4.0, V + 4.0, 0.1, 1 << 20, Seed + 23);
				};
				for (int Y = 0; Y < N; ++Y)
				{
					for (int X = 0; X < N; ++X)
					{
						const double CX = (X + 0.5) * Px - 1.0, CY = 1.0 - (Y + 0.5) * Px;
						// (its outline: an oval a little wider than tall, warped by a broad noise; frayed by a finer one over a
						// wide edge, and inside the cell's fan)
						const double Rho = std::sqrt(CX * CX / (0.86 * 0.86) + CY * CY / (0.76 * 0.76));
						const double In = 1.0 - Rho + 0.45 * (Billow(CX * 0.9, CY * 0.9, Seed + 3) - 0.5);
						const double Fray = Billow(CX * 1.2, CY * 1.2, Seed + 7) - 0.5;
						const double Alpha = Smooth((In + 0.5 * Fray) / 0.35) * Smooth((0.93 - std::hypot(CX, CY)) / 0.05);
						const double HX = (Height(CX + Px, CY) - Height(CX - Px, CY)) / (2.0 * Px), HY = (Height(CX, CY + Px) - Height(CX, CY - Px)) / (2.0 * Px);
						const double NL = std::sqrt(HX * HX + HY * HY + 1.0);
						const double Sun = std::max(0.0, (-HY * Cosine + Sine) / NL);
						// (the seams between the small bumps, and the thinning toward the edge, both darker from above)
						const double Bump = Noise(CX * 0.85 + 4.0, CY * 1.2 + 4.0, 0.24, 1 << 20, Seed + 21);
						const double Seam = Smooth((0.35 - Bump) / 0.2);
						const double Thins = Smooth(1.0 - In / 0.4);
						const double Shade = std::max(0.5 * Seam, 0.8 * Thins);
						const double Side = (0.66 + 0.95 * Sun) * (1.0 - 0.3 * Shade);
						const double Front = (0.96 + 0.04 / NL) * (1.0 - 0.12 * Shade);
						// (from under it: darker where it is thick, its middle, lighter toward its edge and in the seams)
						const double Deep = Smooth((0.85 - Rho) / 0.6) * (0.7 + 0.3 * Bump);
						const double Back = (0.52 - 0.12 * Deep + 0.24 * Shade) * (0.92 + 0.16 * Billow(CX * 2.0, CY * 2.0, Seed + 13));
						const size_t O = (static_cast<size_t>(LidY * N + Y) * Sheet + static_cast<size_t>(LidX * N + X)) * 4;
						OutRgba[O + 0] = Byte(0.5 * Side);
						OutRgba[O + 1] = Byte(0.5 * Front);
						OutRgba[O + 2] = Byte(0.5 * Back);
						OutRgba[O + 3] = Byte(Alpha);
					}
				}
				continue;
			}
			// The body's height over the picture (round lumps, a little billow over them) and how far inside its edge
			// each pixel lies, the edge a little uneven all round.
			for (int Y = 0; Y < N; ++Y)
			{
				for (int X = 0; X < N; ++X)
				{
					const double CX = (X + 0.5) * Px - 1.0, CY = 1.0 - (Y + 0.5) * Px;
					double H = 0.0, In = -1.0;
					for (const Lump& L : Lumps)
					{
						const double D2 = (CX - L.X) * (CX - L.X) + (CY - L.Y) * (CY - L.Y);
						In = std::max(In, L.R - std::sqrt(D2));
						if (D2 < L.R * L.R)
						{
							H = std::max(H, L.R * L.R - D2);
						}
					}
					H = H > 0.0 ? 0.6 * std::sqrt(H) : 0.0;
					if (bFoot)
					{
						// (the foot's slab: from the floor to 0.44 over it, as wide as 0.64 at the floor and a little narrower
						// up, its ends uneven)
						const double Rise = Clamp((CY - Floor) / 0.44, 0.0, 1.0);
						const double SlabWide = 0.64 - 0.2 * Rise * Rise + 0.12 * (Billow(CX * 1.5, CY * 3.0, Seed + 17) - 0.5);
						const double Slab = std::min(std::min(CY - Floor + 0.02, Floor + 0.44 - CY), SlabWide - std::fabs(CX));
						In = std::max(In, Slab);
						H = Slab > 0.0 ? std::max(H, 0.06) : H;
					}
					if (bBase)
					{
						// (the underside straight, but curling up raggedly at the ends, never a rounded end)
						const double Ends = Smooth((std::fabs(CX) - (bFoot ? 0.5 : 0.4)) / 0.35);
						In = std::min(In, CY - Floor - (Ends > 0.0 ? Ends * Ends * (0.05 + 0.12 * Billow(CX * 2.2, 0.3, Seed + 11)) : 0.0));
					}
					const size_t I = static_cast<size_t>(Y) * N + X;
					// (the edge a little uneven all round; worked out only near it)
					Inside[I] = std::fabs(In) < 0.03 ? In + 0.025 * (Billow(CX * 1.3, CY * 1.3, Seed + 3) - 0.5) : In;
					High[I] = H > 0.0 ? H + 0.03 * Billow(CX, CY, Seed) : 0.0;
				}
			}
			// (the surface for its light: the height smoothed over a few pixels, so the seams inside the body are gentle
			// dips and only its outline turns sharply away; and the lie of the land round each pixel, wider, a crease
			// lying under it)
			Soft = High;
			BoxBlur(Soft, 3);
			BoxBlur(Soft, 3);
			Blur = High;
			BoxBlur(Blur, 6);
			BoxBlur(Blur, 6);
			const int CellX = Cell % FragmentCells, CellY = Cell / FragmentCells;
			for (int Y = 0; Y < N; ++Y)
			{
				for (int X = 0; X < N; ++X)
				{
					const size_t I = static_cast<size_t>(Y) * N + X;
					const double CX = (X + 0.5) * Px - 1.0, CY = 1.0 - (Y + 0.5) * Px;
					const double Out = std::max(1e-6, std::hypot(CX, CY));
					// (how much of the way toward the sheet's top this pixel lies from the middle: the sun's side)
					const double Up = CY / Out;
					// Its edge: crisp toward the top (a pixel and a half), softer and torn toward the bottom (to some twenty
					// pixels); a base's underside crisp and straight, untorn.
					// (an underside's ends torn and soft, its straight edge only in the middle)
					const double Ends = bBase ? Smooth((std::fabs(CX) - (bFoot ? 0.5 : 0.4)) / 0.35) : 0.0;
					const double Low = bBase ? std::max(Ends, Smooth((CY - Floor - 0.04) / 0.2)) : 1.0;
					const double Edge = Px * (1.5 + 18.0 * Smooth((0.25 - Up) / 1.0) * Low + 8.0 * Ends);
					const double Tearing = std::min(1.0, Smooth((0.3 - Up) / 0.8) * Low + 0.6 * Ends);
					const double Torn = Tearing > 0.0 && std::fabs(Inside[I]) < 2.0 * Edge ? (Billow(CX * 1.7, CY * 1.7, Seed + 7) - 0.5) * 2.0 * Edge * 0.9 * Tearing : 0.0;
					// (a foot fades out upward into the body over it, from a little under its middle: no edge of its own there)
					const double Alpha = Smooth((Inside[I] + Torn) / Edge) * (bFoot ? Smooth((0.32 - CY) / 0.62) : 1.0);
					// Which way the surface faces (from the smoothed height's slope, steepest at the edge: there it faces
					// across, out from the middle).
					auto At = [&](int AX, int AY) { return Soft[static_cast<size_t>(std::max(0, std::min(N - 1, AY))) * N + static_cast<size_t>(std::max(0, std::min(N - 1, AX)))]; };
					double GX = (At(X + 1, Y) - At(X - 1, Y)) / (2.0 * Px), GY = (At(X, Y - 1) - At(X, Y + 1)) / (2.0 * Px);
					const double Edgeward = Smooth(1.0 - Inside[I] / 0.06);
					GX -= 3.0 * Edgeward * CX / Out;
					GY -= 3.0 * Edgeward * CY / Out;
					const double Steep = std::hypot(GX, GY);
					if (Steep > 5.0)
					{
						GX *= 5.0 / Steep;
						GY *= 5.0 / Steep;
					}
					const double NL = std::sqrt(GX * GX + GY * GY + 1.0);
					const double NY = -GY / NL, NZ = 1.0 / NL;
					// (a crease, where it lies under the lumps round it: darker; none past the edge, so that what the
					// picture's levels and its reading between pixels mix in from outside the shape is the edge's own light,
					// not a dark fringe)
					const double Crease = High[I] > 0.0 ? Clamp((Blur[I] - High[I]) / 0.06, 0.0, 1.0) : 0.0;
					const double Ao = 1.0 - 0.3 * Crease;
					// (the sun across the picture toward the top, a little toward the eye)
					const double Side = Ao * (0.6 + 0.5 * std::max(0.0, NY * 0.8 + NZ * 0.6));
					// (the sun behind the eye: the faces bright, the edges and creases less)
					const double Front = Ao * (0.8 + 0.25 * std::max(0.0, NY * 0.25 + NZ * 0.97));
					// (the sun ahead: the body darker where it is thick, mottled, its thin edges lit through, the most toward
					// the sun's side; and seen from under a cloud, its lumps so: the art director on round 18c, "a darker,
					// lumpy base, lighter where it thins")
					const double Thin = Smooth(1.0 - Inside[I] / 0.07);
					const double Mottle = Alpha > 0.0 ? 0.45 + 1.1 * Billow(CX * 2.5, CY * 2.5, Seed + 13) : 1.0;
					// (the light's way through a lump is longest at its middle: dark there, lighter toward its rim and in the
					// seams between lumps, so each lump reads on its own)
					const double Deep = Smooth((0.5 * Soft[I] + 0.5 * High[I]) / 0.2);
					const double Back = Mottle * (0.3 + 0.52 * (1.0 - Deep)) * (0.75 + 0.25 * (1.0 - Smooth(Inside[I] / 0.3))) + 1.05 * Thin * (0.4 + 0.6 * std::max(0.0, Up));
					// (a base's lower part darker toward its underside, a foot's more)
					const double Under = bFoot ? 0.72 + 0.28 * Smooth((CY - Floor) / 0.5) : bBase ? 0.8 + 0.2 * Smooth((CY - Floor) / 0.35) : 1.0;
					const size_t O = (static_cast<size_t>(CellY * N + Y) * Sheet + static_cast<size_t>(CellX * N + X)) * 4;
					OutRgba[O + 0] = Byte(0.5 * Side * Under);
					OutRgba[O + 1] = Byte(0.5 * Front * Under);
					OutRgba[O + 2] = Byte(0.5 * Back * Under);
					OutRgba[O + 3] = Byte(Alpha);
				}
			}
		}
	}

	double Day::CoverLaid() const
	{
		if (!(Cover >= 0.0))
		{
			return bDeck ? DefaultDeck : DefaultCumulus;
		}
		return bDeck ? Clamp(Cover, DeckLeast, 1.0) : Clamp(Cover, 0.0, CumulusMost);
	}

	Lighting LightOf(const Day& D, double Under)
	{
		Lighting L;
		const double C = D.CoverLaid();
		if (!D.bDeck)
		{
			// (the look the art director passed at three eighths kept as it is)
			L.Sky = 1.0 + 0.15 * (C - DefaultCumulus);
			return L;
		}
		const double U = Clamp(Under, 0.0, 1.0);
		const double Through = 0.55 - 0.5 * Smooth((C - DeckLeast) / (1.0 - DeckLeast));
		L.Sun = 1.0 - U * (1.0 - Through);
		L.Sky = 1.0 + U * 0.25 * C;
		return L;
	}

	DeckShade ShadeOf(const Field& F, int Side)
	{
		DeckShade S;
		if (!F.Sky.bDeck || Side <= 0 || F.Clouds.empty())
		{
			return S;
		}
		S.Side = Side;
		S.Origin = F.Origin;
		S.Period = Period;
		S.Z = 0.5 * (F.Base + F.Top);
		S.Top = F.Top;
		// (under a tile almost none of the sun: a sixth through a broken deck's thinner cells, a twentieth through a closed
		// overcast; with the holes, about a third of the whole sun under Bremen's 0.77, as LightOf has it)
		S.Through = 0.16 - 0.11 * Smooth((F.Sky.CoverLaid() - 0.8) / 0.2);
		const size_t N = static_cast<size_t>(Side) * static_cast<size_t>(Side);
		std::vector<double> Open(N), Soft(N);
		const double Step = Period / Side;
		for (int Y = 0; Y < Side; ++Y)
		{
			for (int X = 0; X < Side; ++X)
			{
				const double PX = F.Origin.X + (X + 0.5) * Step, PY = F.Origin.Y + (Y + 0.5) * Step;
				Open[static_cast<size_t>(Y) * Side + X] = F.DeckAt(PX, PY) > 0.0 ? 0.0 : 1.0;
			}
		}
		// (the edges softened over a few hundred metres, the field repeating: a cloud's edge is never a line on the ground)
		for (int Pass = 0; Pass < 2; ++Pass)
		{
			for (int Y = 0; Y < Side; ++Y)
			{
				for (int X = 0; X < Side; ++X)
				{
					double Sum = 0.0;
					for (int DY = -1; DY <= 1; ++DY)
					{
						for (int DX = -1; DX <= 1; ++DX)
						{
							Sum += Open[static_cast<size_t>((Y + DY + Side) % Side) * Side + static_cast<size_t>((X + DX + Side) % Side)];
						}
					}
					Soft[static_cast<size_t>(Y) * Side + X] = Sum / 9.0;
				}
			}
			Open.swap(Soft);
		}
		S.Light.resize(N);
		for (size_t K = 0; K < N; ++K)
		{
			const double Share = S.Through + (1.0 - S.Through) * Open[K];
			S.Light[K] = static_cast<uint8_t>(Clamp(Share * 255.0 + 0.5, 0.0, 255.0));
		}
		return S;
	}

	double DeckShade::At(const Vec& P, const Vec& Sun) const
	{
		if (Side <= 0 || Light.size() != static_cast<size_t>(Side) * static_cast<size_t>(Side) || Period <= 0.0)
		{
			return 1.0;
		}
		// (read where the ray to the sun crosses the deck's middle; over the tops nothing, faded over 100 m)
		const double Over = Smooth((P.Z - (Top - 100.0)) / 100.0);
		const double Along = std::max(0.0, (Z - P.Z) / std::max(0.05, Sun.Z));
		double U = (P.X + Sun.X * Along - Origin.X) / Period * Side - 0.5;
		double V = (P.Y + Sun.Y * Along - Origin.Y) / Period * Side - 0.5;
		U -= Side * std::floor(U / Side);
		V -= Side * std::floor(V / Side);
		const int X0 = std::min(Side - 1, static_cast<int>(U)), Y0 = std::min(Side - 1, static_cast<int>(V));
		const int X1 = (X0 + 1) % Side, Y1 = (Y0 + 1) % Side;
		const double FX = U - X0, FY = V - Y0;
		auto L = [this](int X, int Y) { return Light[static_cast<size_t>(Y) * Side + X] / 255.0; };
		const double Here = (L(X0, Y0) * (1.0 - FX) + L(X1, Y0) * FX) * (1.0 - FY) + (L(X0, Y1) * (1.0 - FX) + L(X1, Y1) * FX) * FY;
		return Here + (1.0 - Here) * Over;
	}

	double DeckShade::Mean() const
	{
		if (Light.empty())
		{
			return 1.0;
		}
		double Sum = 0.0;
		for (uint8_t B : Light)
		{
			Sum += B;
		}
		return Sum / (255.0 * Light.size());
	}

	double DeckHoleBelow(double Cover)
	{
		// (the share of the whole deck the kept tiles' discs cover, against the noise they are kept above, six seeds over
		// the laid square: the 3D builder's sweep of 20 November, Tools/air_show)
		static constexpr double Table[][2] = {
			{ 0.556, 0.50 }, { 0.675, 0.45 }, { 0.77, 0.40 }, { 0.857, 0.35 }, { 0.917, 0.30 }, { 0.958, 0.25 }, { 0.984, 0.20 }, { 0.995, 0.15 }, { 1.0, 0.0 },
		};
		const double C = Clamp(Cover, Table[0][0], 1.0);
		for (size_t K = 1; K < std::size(Table); ++K)
		{
			if (C <= Table[K][0])
			{
				const double U = (C - Table[K - 1][0]) / (Table[K][0] - Table[K - 1][0]);
				return Table[K - 1][1] + U * (Table[K][1] - Table[K - 1][1]);
			}
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
		F.GroundTop = P.GroundTop;
		F.Serial = ++Layings;
		F.Wind = P.Wind;
		WindLattice(F.Wind, F.AcrossX, F.AcrossY);
		const double LatX = F.AcrossX, LatY = F.AcrossY;
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
		F.WindWay = std::atan2(F.Wind.Y, F.Wind.X);
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
			// Scattered cumulus: the day's cover of the sky (none on a clear day, about three eighths as the art director
			// passed it, up to 0.55), thrown in at random over the laid square, kept apart enough to read as single clouds
			// (a few touch; apart across the square's edges too, as it repeats), until the cover is reached. They are laid at the day's base; the painter lifts each over the ground where it is drawn,
			// and thins the ones that drift into the clearing over the target.
			// (a trade-wind sky, the art director: many small, some middling, a few big; in loose clusters with gaps, in
			// rows along the wind. A place is taken more readily where the rows and the clusters crowd: the rows a wave
			// across the wind that repeats with the square, the wind (3, -2) running along (3, -2) and across (2, 3), three
			// waves to the square's step that way, about 4 km apart; the clusters a broad noise, 3 km a cell)
			// (the streets narrower than the lanes between them: the wave squared, the art director's brief on build 1768,
			// loose streets along the trade wind broken into clumps, clear lanes of open sea between them)
			const double RowTurn = Roll.U(0.0, 2.0 * Pi);
			auto Crowd = [&F, RowTurn, NoiseSeed, LatX, LatY](double X, double Y)
			{
				const double LX = X - F.Origin.X, LY = Y - F.Origin.Y;
				const double Wave = 0.5 + 0.5 * std::cos(2.0 * Pi * 3.0 * (LatX * LX + LatY * LY) / Period + RowTurn);
				const double Clump = Noise(LX, LY, Period / 14.0, 14, NoiseSeed + 2);
				return 0.6 * Wave * Wave + 0.4 * Clump;
			};
			// (downwind, level: the way the tops lean and the lee sides fray)
			const Vec Down = Vec(F.Wind.X, F.Wind.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
			const double Wanted = D.CoverLaid() * Period * Period;
			const double Fatter = Smooth((D.CoverLaid() - DefaultCumulus) / (CumulusMost - DefaultCumulus));
			// (each kind of day its own clouds, the art director, 20 November 22:37Z: a fair day's small flat fair-weather
			// cumulus, wider than tall and evenly spread; a heavy day's fewer, bigger, taller heaps, some towering, merging
			// into clusters with wide gaps between, their bases darker. Three eighths as she passed it, the same draws)
			const double Fair = Smooth((DefaultCumulus - D.CoverLaid()) / (DefaultCumulus - 0.12));
			// (sizes by the cover each takes, laid biggest first: the art director on 2b177386's frames, 22 November 04:16Z,
			// "popcorn: one size, even spacing ... wanted: a few big ones and many small". Drawn at random among sizes, the
			// big ones were the likeliest to find no room, and the middling took half the cover, 600 clouds of one size.
			// Now the big ones (1.5 to 3.5 km) are laid first and take half the sky's cover, a few middling (600 m to
			// 1.5 km) a quarter or so, and the small ones (100 to 600 m) the rest, many of them round the bigger; within each
			// even on a log scale. A cloudier day's cover goes more to the big ones, a fair day's more to the small)
			const double BigShare = 0.5 + 0.15 * Fatter - 0.3 * Fair, SmallShare = 0.22 - 0.08 * Fatter + 0.25 * Fair;
			// (the small ones are drawn one and a half to two and a half times as long as wide, and counted at the sky they
			// truly cover, so the day's cover holds. Laid in streets they now fill their share, where scattered one by one
			// they ran out of tries short of it: on a scattered or heavy day they take a third of their old share, about as
			// many clouds as a1acb552's sky and its paint's cost, and the big and middling the rest, shared as before; a
			// fair day's sky is still mostly small ones)
			const double SmallKept = 0.35 + 0.65 * Fair;
			const double SmallLaid = SmallShare * SmallKept;
			const double BigLaid = BigShare + (SmallShare - SmallLaid) * BigShare / std::max(0.05, 1.0 - SmallShare);
			const double UpTo[3] = { BigLaid, 1.0 - SmallLaid, 1.0 };
			double Covered = 0.0;
			int Pass = 0, PassTries = 0;
			// (the small ones laid in short streets and clumps: after one is laid in a new place, the next of its street is
			// tried where the street goes on, 1 to 5 more, each its own size, sea between them; the art director on
			// a1acb552's frames, 22 November 08:13Z, "evenly scattered, so the sea reads as a field of cotton balls. Gather
			// them into clumps and short streets with open sea between")
			struct StreetNext
			{
				double X = 0.0, Y = 0.0, W = 0.0, Way = 0.0, Sign = 1.0;
				int Left = 0;
			} Next;
			for (int Try = 0; Try < 120000 && Pass < 3; ++Try)
			{
				// (a size's pass ends when the cover is laid up to its share, or after 40,000 tries; a clear day, wanting none,
				// passes all three at once and lays no cloud)
				while (Pass < 3 && (Covered >= Wanted * UpTo[Pass] || PassTries >= 40000))
				{
					++Pass;
					PassTries = 0;
				}
				if (Pass >= 3)
				{
					break;
				}
				++PassTries;
				double X = F.Origin.X + Roll.U(0.0, Period), Y = F.Origin.Y + Roll.U(0.0, Period);
				const double Within = Roll.U();
				auto Between = [Within](double Least, double Most) { return Least * std::pow(Most / Least, Within); };
				double W = Pass == 0 ? Between(1500.0, 3500.0) : Pass == 1 ? Between(600.0, 1500.0) : Between(100.0, 600.0);
				// (the next of a street, where it goes on: its place taken as it is, the street's own)
				const bool bNext = Pass == 2 && Next.Left > 0;
				if (bNext)
				{
					X = Next.X;
					Y = Next.Y;
					W = Next.W;
				}
				const bool bSmall = W < 600.0;
				// (the small ones gather round the bigger, as a field of cumulus does, a few out in the open sea between:
				// none of them spread evenly over it)
				if (bSmall && !bNext)
				{
					double Gap = 1e9;
					for (const Cloud& O : F.Clouds)
					{
						if (O.W >= 600.0)
						{
							const double Dx = Across(O.At.X - X), Dy = Across(O.At.Y - Y);
							Gap = std::min(Gap, std::sqrt(Dx * Dx + Dy * Dy) - 0.5 * O.W);
						}
					}
					if (Roll.U() > 0.12 + 0.88 * std::exp(-std::max(0.0, Gap) / 900.0))
					{
						continue;
					}
				}
				// (the bigger ones keep to the streets; the small ones scatter in the lanes a little more)
				// (a fair day's evenly over the sea; a heavy day's crowding harder, the gaps wider)
				const double Street = (bSmall ? 0.1 : 0.06) + (bSmall ? 0.9 : 0.94) * Smooth((Crowd(X, Y) - 0.25 - 0.1 * Fatter) / (0.45 - 0.15 * Fatter));
				if (!bNext && Roll.U() > Street + (0.8 - Street) * Fair)
				{
					continue;
				}
				bool bClose = false;
				for (const Cloud& O : F.Clouds)
				{
					// (two small ones kept their own width apart, so that a street of them is a line of separate clouds
					// with sea between, not a chain of beads: the art director on round 18b, eyes 1 and 3)
					// (a heavy day's big ones closer, merging into clusters)
					const double Dx = Across(O.At.X - X), Dy = Across(O.At.Y - Y), Apart = (bSmall && O.W < 600.0 ? 1.0 : 0.55 - 0.25 * Fatter) * (O.W + W) * 0.5;
					if (Dx * Dx + Dy * Dy < Apart * Apart)
					{
						bClose = true;
						break;
					}
				}
				if (bClose)
				{
					// (a street that runs into another cloud ends there)
					Next.Left = 0;
					continue;
				}
				// Its height from its width, 1 to 2.5 times as wide as it is tall (never a ball), capped a little under the
				// day's top, where the inversion stops most of them in one broken level; one big one in about twenty a tower
				// to the day's top (the brief's flat bases and capped tops). A small one two fifths to a little over half as
				// deep as it is wide, 50 m at least (it was 150 m at least, and from above the 200 to 300 m ones stood as
				// round balls of one size: the art director on r2e's cloud sheets, 22 November 06:21Z, "flatter, height
				// about half their width ... some near 100 m")
				// (a small one drawn out along the wind, one and a half to twice as long as it is across, one in seven to two
				// and a half (W its length): from above, round, they stood as cotton balls, the art director on a1acb552's
				// frames; at two to three times they read as rice grains, her trim on r2g's sheets, 22 November 11:22Z; and
				// half to four fifths as deep as it is across)
				const double Aspect = !bSmall ? 1.0 : Roll.U() < 0.15 ? Roll.U(2.0, 2.5) : Roll.U(1.5, 2.0), Wide = W / Aspect;
				const double Least = bSmall ? std::max(45.0, Wide * Roll.U(0.55, 0.8)) : 250.0;
				// (a fair day's shallow, two to three and a half times as wide as tall; a heavy day's taller, a third of its big
				// ones towering to the day's top)
				const double Cap = Thick * Roll.U(0.8, 0.9) * (1.0 - 0.45 * Fair);
				const bool bTower = W >= 1500.0 && Roll.U() < 0.05 + 0.3 * Fatter;
				double H = bTower ? Thick : std::min(Cap, W / Roll.U(1.0 + 1.0 * Fair - 0.1 * Fatter, 2.5 + 1.0 * Fair - 0.7 * Fatter));
				H = std::max(H, std::min(Thick, Least));
				// (a small one no taller than two fifths of its width, or its own Least: in two layers, a taller one's top
				// lumps floated clear of its base, the art director on round 18c, oahu eye 7)
				// (what it would have stood, its draws for turrets made by that, so the day's other clouds lie where they did)
				const double Tall = H;
				H = bSmall ? std::min(H, Least) : H;
				const double Foot = Base;
				// (its top leaning 5 to 15 degrees downwind, a shift of so much for each metre up)
				const double Lean = std::tan(Roll.U(5.0, 15.0) * Pi / 180.0);
				auto Leaned = [&Down, Lean, Foot](double PX, double PY, double PZ) { return Vec(PX + Down.X * Lean * (PZ - Foot), PY + Down.Y * Lean * (PZ - Foot), PZ); };
				// (layers close enough that the puffs of one overlap the next: no gap between a cloud's base and its top; a
				// small cloud two layers at most, of three puffs)
				// (a small one's lumps in scale with it, down to 30 m: at 90 m at least, a 100 to 300 m one was two or three
				// lumps as big as itself, a ball)
				const double Rb = bSmall ? Clamp(0.55 * Wide, 25.0, 480.0) : Clamp(W * 0.25, 90.0, 480.0);
				const int Layers = static_cast<int>(Clamp(std::ceil(std::max(0.0, H - 0.5 * Rb - 0.55 * Rb) / (0.5 * Rb)) + 1.0, 2.0, bSmall ? 2.0 : 6.0));
				Cloud& C = F.Clouds.emplace_back();
				C.At = Vec(X, Y, Foot);
				C.W = W;
				C.H = H;
				C.First = static_cast<int>(F.Puffs.size());
				C.Fate = Roll.U(0.05, 0.85);
				C.Long = Aspect;
				double TopW = W * 0.5, TopR = Rb;
				// (a small one is a rag drawn out a little along the wind, give or take, its lumps strung along it at
				// mixed sizes: an irregular flat-based lump, never a marble, the art director on round 18b)
				const double RagWay = std::atan2(Down.Y, Down.X) + Roll.U(-0.6, 0.6);
				const double AlongX = std::cos(RagWay), AlongY = std::sin(RagWay);
				C.LongWay = RagWay;
				// (each of its lumps drawn out along it too, half as long again, the base's more: a rag, not a string of
				// beads)
				const double LumpLong[2] = { 1.6, 1.45 };
				for (int K = 0; K < Layers; ++K)
				{
					const double Hk = static_cast<double>(K) / (Layers - 1);
					// (a small one's top as broad as its base: narrowing, the 200 m ones of 22 November stood on their wider
					// grey bases as mushrooms seen from under them)
					const double Rk = Rb * (1.0 - (bSmall ? 0.15 : 0.35) * Hk);
					// (its base layer narrower than the body over it, so that from above the lit dome hides it and no grey
					// plate shows round the cloud)
					// (a small one's half breadth across it: its lumps on its middle line)
					const double Wk = bSmall ? 0.5 * Wide : W * 0.5 * (1.0 - 0.45 * std::pow(Hk, 1.5)) * (K == 0 ? 0.8 : 1.0);
					// (and how far along it its lumps' middles run either way: its top 0.9 of its length, its base 0.75, under it)
					const double Run = bSmall ? std::max(0.0, 0.5 * W * (K == 0 ? 0.75 : 0.9) - Rk * LumpLong[K == 0 ? 0 : 1]) : 0.0;
					// (a small one flatter all through: a rag of cloud wider than tall, not a ball, the brief's "dots")
					const double Flat = K == 0 ? 0.55 : bSmall ? 0.68 : 0.85;
					const double Low = Rk * Flat * 0.95, High = Rk * 0.85;
					const double Zk = Foot + Low + std::max(0.0, H - Low - High) * Hk;
					const int N = bSmall ? (K > 0 && Aspect > 1.75 ? 3 : 2) : static_cast<int>(Clamp(std::round((Wk / Rk) * (Wk / Rk) * 1.0), K + 1 < Layers ? 2.0 : 1.0, 10.0));
					for (int J = 0; J < N; ++J)
					{
						const double A = Roll.U(0.0, 2.0 * Pi), Far = std::sqrt(Roll.U()) * std::max(0.0, Wk - 0.85 * Rk);
						double PX = X + Far * std::cos(A), PY = Y + Far * std::sin(A);
						// (where along a small one's length: -1 at one end, 1 at the other)
						const double Spot = N > 1 ? (J - 0.5 * (N - 1)) / (0.5 * (N - 1)) : 0.0;
						if (bSmall)
						{
							const double Stretch = Spot * Run;
							PX = X + AlongX * Stretch + 0.35 * (PX - X);
							PY = Y + AlongY * Stretch + 0.35 * (PY - Y);
						}
						// (a shallow cloud's upper puffs kept within its height)
						const double PZ = std::min(Foot + H, Zk + (K > 0 ? Roll.U(-0.2, 0.2) * Rk : 0.0));
						const double Side = ((PX - X) * SunFlat.X + (PY - Y) * SunFlat.Y) / std::max(1.0, W * 0.5);
						const double Up = Clamp((PZ - Foot) / std::max(1.0, H), 0.0, 1.0);
						double Lit = 0.2 + 0.65 * Up + 0.25 * Side;
						// (a base's own light a little under the body's over it, no darker than the body's lower edge: from
						// under the cloud it is grey by the eye's look up at it (Painter::Paint's Below), and from the side
						// it is the body's flat underside, of a piece with it, not a dark lens hung under it: the art
						// director on round 18b, eye 2)
						if (K == 0)
						{
							Lit = std::min(Lit, 0.45 - 0.13 * Fatter);
						}
						// (far from the eye a layer is drawn with its first two puffs only, a cloud under 1.2 km across with its
						// first)
						const int Lod = J < (W < 1200.0 ? 1 : 2) ? 1 : 0;
						// (how far downwind of its middle: the upwind side firm, the lee side ragged, the brief)
						const double Lee = Clamp(((PX - X) * Down.X + (PY - Y) * Down.Y) / std::max(1.0, W * 0.5), 0.0, 1.0);
						// (its base soft at the sides with a straight, crisp underside, solid enough to stand darker than the
						// sky behind it; the lumps over it crisp where the sun catches their edges, softer and thinner to lee)
						// (a small one's end lumps smaller: it thins out to its ends, ragged, not a sausage)
						const double Taper = bSmall ? 1.0 - 0.22 * std::fabs(Spot) : 1.0;
						Puff& Lump = Add(Leaned(PX, PY, PZ), Rk * Taper * (bSmall && K > 0 ? Roll.U(0.75, 1.2) : Roll.U(0.7, 1.2)) * (K == 0 ? 1.0 : 1.0 - 0.15 * Lee), Flat, Lit, K == 0 ? 0.92 : 0.82 - 0.14 * Lee, Lod);
						// (a small one's lumps softer, its edges torn: the art director on r2e's sheets)
						Lump.Soft = K == 0 ? 0.45 : bSmall ? 0.32 + 0.3 * Lee : 0.2 + 0.28 * Lee;
						Lump.bFlatFoot = K == 0;
						if (bSmall)
						{
							Lump.Long = LumpLong[K == 0 ? 0 : 1];
							Lump.LongWay = RagWay;
						}
					}
					TopW = Wk;
					TopR = Rk;
				}
				// The cauliflower top: a few smaller lit turrets on a taller cloud.
				// (a small one none: drawn for, not laid)
				const int Turrets = Tall > 500.0 ? 1 + (Tall > 900.0 ? 1 : 0) + (Roll.U() < 0.5 ? 1 : 0) : 0;
				for (int J = 0; J < Turrets; ++J)
				{
					const double A = Roll.U(0.0, 2.0 * Pi), Far = Roll.U() * TopW * 0.6;
					const double PX = X + Far * std::cos(A), PY = Y + Far * std::sin(A);
					const double Rt = TopR * Roll.U(0.65, 0.85);
					if (bSmall)
					{
						continue;
					}
					const double Side = ((PX - X) * SunFlat.X + (PY - Y) * SunFlat.Y) / std::max(1.0, W * 0.5);
					Add(Leaned(PX, PY, Foot + H - 0.75 * Rt), Rt, 0.9, 0.9 + 0.1 * Side, 0.8, 1).Soft = 0.18;
				}
				// Fractus: on a big cloud, a few small ragged bits hanging under its base, toward its lee side (the brief:
				// 50 to 150 m, alpha 0.3 to 0.6; the art director on round 18c, from under it "a few torn wisps hanging
				// under it"), grey, drawn near only.
				const int Bits = W >= 1200.0 ? 1 + static_cast<int>(Roll.U() * 2.99) : 0;
				for (int J = 0; J < Bits; ++J)
				{
					const double Out = W * Roll.U(0.05, 0.42), Aside = W * Roll.U(-0.3, 0.3);
					const double Rf = Roll.U(45.0, 110.0);
					const double BX = X + Down.X * Out - Down.Y * Aside, BY = Y + Down.Y * Out + Down.X * Aside;
					// (its middle on the foot, as every puff's is within its cloud, half of it hanging under)
					Puff& Bit = Add(Vec(BX, BY, Foot), Rf, 0.6, 0.22, Roll.U(0.3, 0.6), 0);
					Bit.Soft = 0.6;
				}
				// Its veil: one broad, faint puff round the whole, so its lumps read as one body and not as separate balls
				// (the art director's sheet: never a few round puffs that read as cotton); kept within the body and faint,
				// so it lays no pale disc on the sea round the cloud (the art director: within 1.15 times the body, or half
				// as faint); a small cloud, a few lumps, needs none.
				// (no deeper than the cloud's flat base and 120 m more, at its faded rim: a flat base stays flat)
				if (!bSmall)
				{
					Add(Leaned(X, Y, Foot + 0.4 * H), 0.46 * W, Clamp((0.4 * H + 120.0) / (0.46 * W), 0.15, 0.9), 0.55, 0.1, 2).Soft = 0.5;
				}
				// Its far body: farther than its lumps can be told apart, one soft lit puff over most of it, in its veil
				// (its rim 60 m under the base at most).
				// (a small one's drawn out along it as the cloud is, 0.95 of its length and a little over its breadth: the
				// art director's elongated far sprite, 22 November 06:44Z and 08:13Z)
				const double FarR = bSmall ? 0.55 * Wide : 0.42 * W;
				Puff& Body = Add(Leaned(X, Y, Foot + 0.45 * H), FarR, Clamp(std::min(1.3 * H, 0.45 * H + 60.0) / FarR, 0.15, 0.9), 0.72, 0.72, 3);
				if (bSmall)
				{
					Body.Long = Clamp(0.475 * W / FarR, 1.0, 4.0);
					Body.LongWay = RagWay;
				}
				C.Count = static_cast<int>(F.Puffs.size()) - C.First;
				// How much of its own cloud lies between each of its puffs and the sun: the other lumps' depth along the
				// way to the sun, each by how near the way passes its middle (the brief's one sun on the cloud: a puff lit
				// from the whole cloud's form and what of it stands sunward, never as its own ball). The veil and the far
				// body are left out of the sum: they stand for the whole.
				for (int A = C.First; A < C.First + C.Count; ++A)
				{
					Puff& Me = F.Puffs[static_cast<size_t>(A)];
					double Depth = 0.0;
					for (int B = C.First; B < C.First + C.Count; ++B)
					{
						const Puff& O = F.Puffs[static_cast<size_t>(B)];
						if (B == A || O.Lod >= 2 || O.R >= 0.4 * W)
						{
							continue;
						}
						const Vec Gap = O.At - Me.At;
						const double Along = Gap.Dot(Sun);
						if (Along <= 0.25 * O.R)
						{
							continue;
						}
						const double Off = (Gap - Sun * Along).Length() / std::max(1.0, O.R);
						if (Off < 1.0)
						{
							Depth += 2.0 * O.R * (1.0 - Off * Off) * O.Alpha;
						}
					}
					Me.Shadowed = 1.0 - std::exp(-Depth / 700.0);
				}
				// (a small one counted as drawn, W long and Wide across: narrower than before, so the sea between them opens)
				Covered += Pi * (W * 0.5) * (Wide * 0.5) * 0.85;
				// (a small one's street: a new one begins 1 to 5 more, along the wind give or take a sixth of a right angle,
				// most of them downwind of it; each next one half to a little over the size of the last, its middle 1.1
				// to 1.5 of their mean length on, and up to 0.3 of its own length aside)
				if (bSmall)
				{
					if (!bNext)
					{
						Next.Left = 1 + static_cast<int>(Roll.U(0.0, 4.99));
						Next.Way = std::atan2(Down.Y, Down.X) + Roll.U(-0.26, 0.26);
						Next.Sign = Roll.U() < 0.75 ? 1.0 : -1.0;
					}
					else
					{
						--Next.Left;
					}
					if (Next.Left > 0)
					{
						const double Wn = Clamp(W * Roll.U(0.5, 1.1), 100.0, 599.0);
						const double On = 0.5 * (W + Wn) * Roll.U(1.1, 1.5) * Next.Sign, Aside = Wn * Roll.U(-0.3, 0.3);
						const double NX = X + std::cos(Next.Way) * On - std::sin(Next.Way) * Aside;
						const double NY = Y + std::sin(Next.Way) * On + std::cos(Next.Way) * Aside;
						Next.X = F.Origin.X + (NX - F.Origin.X) - Period * std::floor((NX - F.Origin.X) / Period);
						Next.Y = F.Origin.Y + (NY - F.Origin.Y) - Period * std::floor((NY - F.Origin.Y) / Period);
						Next.W = Wn;
					}
				}
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
			// (the day's cover: broken at 0.6, the breaks broad; closed at all of it, none; or the battle's own gaps)
			const double HoleBelow = P.DeckHoles >= 0.0 ? P.DeckHoles : DeckHoleBelow(D.CoverLaid());
			const double RollTurn = Roll.U(0.0, 2.0 * Pi), RiftTurn = Roll.U(0.0, 2.0 * Pi);
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
					// (the breaks: a few broad, more of them small, the art director's round 18d, 60 to 80 per cent cover; how many
					// the day's cover)
					// (and its cells lined up in rolls along the wind, about 2.4 km apart, the breaks lying in the troughs between
					// them; and a few long rifts along the rolls, broken into lengths: the art director's winter-deck steer. The
					// wind (3, -2) runs along the rolls, the waves across it, (2, 3), whole numbers of them to the square's side)
					const double Across = 2.0 * Pi * (LatX * LX + LatY * LY) / Period;
					const double Rolls = 0.5 + 0.5 * std::cos(5.0 * Across + RollTurn);
					const double Rift = Smooth((std::cos(2.0 * Across + RiftTurn) - 0.955) / 0.035) * Smooth((Noise(LX, LY, Period / 8.0, 8, NoiseSeed + 5) - 0.45) / 0.12);
					const double N = (Noise(LX, LY, Period / 9.0, 9, NoiseSeed) * 0.45 + Noise(LX, LY, Period / 27.0, 27, NoiseSeed + 1) * 0.55) * 0.72 + 0.28 * Rolls - 0.4 * Rift;
					const double Fate = Roll.U(0.1, 0.9);
					const double R = S * Roll.U(0.62, 0.9);
					// (as many numbers drawn for every place, kept or not, so the laying stays the same where it is kept)
					// (one tile in five with a cell standing 100 to 200 m proud of the lid, the rest flat: the art director's
					// round 18d steer, one lid under the inversion, not a field of heaps)
					const double LumpRoll = Roll.U();
					const int Lumps = LumpRoll < 0.2 ? 1 : 0;
					double LumpAt[3][3];
					for (double (&Each)[3] : LumpAt)
					{
						Each[0] = Roll.U(-0.42, 0.42);
						Each[1] = Roll.U(-0.42, 0.42);
						Each[2] = Roll.U(0.25, 0.6);
					}
					if (N < HoleBelow)
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
					// (the lid's top level under the inversion, give or take 60 m: as far under the day's tops as its proud cells
					// stand over it, so that the cells reach the tops and none goes past them; the lid that much flatter, its
					// underside where it was, as clear of high ground as before)
					const double Room = std::min(200.0, 0.2 * Th);
					const double FlatTop = std::max(0.15, std::min(0.6, 0.95 * Th / R) - Room / (1.495 * R));
					const double HalfTop = 0.55 * R * FlatTop;
					const double TopZ = std::min(Foot + Th - Room, Foot + Th - Room - 0.9 * HalfTop + (N - 0.5) * std::min(60.0, 0.1 * Th));
					Puff& Floor = Add(Vec(X, Y, TopZ), R, FlatTop, 0.9 + 0.1 * Clamp((N - 0.22) / 0.78, 0.0, 1.0), 0.85, 2);
					Floor.Soft = 0.3;
					Floor.Trough = 0.2;
					// (its proud cell, broad and low, its top 100 to 200 m over the lid's; drawn out to FarAll)
					for (int J = 0; J < Lumps; ++J)
					{
						const double M = std::min(R, 800.0) * (0.5 + 0.5 * LumpAt[J][2]);
						const double Flat = 0.35, Proud = Room * (0.5 + 0.5 * (LumpAt[J][2] - 0.25) / 0.35);
						const double LumpZ = std::min(Foot + Th - 1.0, TopZ + 0.55 * R * FlatTop * 0.9 + Proud - 0.55 * M * Flat);
						Puff& Lump = Add(Vec(X + LumpAt[J][0] * R, Y + LumpAt[J][1] * R, LumpZ), M, Flat, 0.95, 0.8, 1);
						Lump.Soft = 0.3;
					}
					if ((GX + GY) % 2 == 0)
					{
						// (its grey base: flat, its body's lower edge at the tile's foot, its underside straight)
						const double Rb = R * 1.1;
						// (hanging no more than 170 m under the foot, so that over high ground, the foot lifted 200 m over the ground
						// under the rims, its own rim stays 30 m clear: Bremen's deck laid over Oahu's Kaala)
						const double FlatBase = Clamp(std::min(0.3 * std::min(Th, R) / (0.55 * Rb), 170.0 / (0.45 * Rb)), 0.12, 0.5);
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
				C.HeartReach = std::max(C.HeartReach, Off + 0.55 * Each.R * Each.Long);
				C.RimReach = std::max(C.RimReach, Off + Each.R * Each.Long);
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
		std::vector<Vec>* OutShadows, std::vector<uint8_t>* OutShadowRgba, std::vector<float>* OutUvs)
	{
		const bool bApart = OutShadows && OutShadowRgba;
		const int Pool = RoomFor(F);
		const int Per = PerPuff();
		OutPoints.resize(static_cast<size_t>(Pool) * Per);
		OutRgba.resize(static_cast<size_t>(Pool) * Per * 4);
		// (the sheet's places, for the shapes: four numbers a point, the room not needed at the sheet's corner, outside
		// every shape)
		const bool bShapes = bSprites && OutUvs;
		if (OutUvs)
		{
			OutUvs->assign(static_cast<size_t>(Pool) * Per * 4, 0.0f);
		}
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
			// (a small cloud, under 600 m across, is drawn out to SmallSight times its width, thinning over the last fifth: a
			// 100 m one to 3.4 km, a 600 m one to the reach, each about 1.7 degrees across where it goes. Farther it is a few
			// pixels in the haze, and the many small ones the art director asked for on 22 November cost the painter a
			// puff each out to the reach)
			const double Haze = Smooth((Reach - Far) / (Reach / 3.0)) * (C.W < 600.0 ? Smooth((SmallSight * C.W - Far) / (0.2 * SmallSight * C.W)) : 1.0);
			if (Haze <= 0.0)
			{
				continue;
			}
			// (only a cloud that is drawn asks for the paints that ease it)
			bEasing = bEasing || (bMine && bStillEasing);
			const double NearShare = Smooth((NearAll + 1000.0 - Far) / 1000.0), FarShare = Smooth((FarAll + 1500.0 - Far) / 1500.0);
			// (a heap of cumulus begins with its base; a deck's tile with its broad top, a scrap with a bit)
			const bool bHeap = bShapes && C.Count > 1 && F.Puffs[static_cast<size_t>(C.First)].bFlatFoot;
			for (int K = C.First; bMine && K < C.First + C.Count; ++K)
			{
				const Puff& Each = F.Puffs[static_cast<size_t>(K)];
				// (a heap's one base, drawn from the sheet's feet, goes with its near lumps: farther, its far body's own flat
				// underside is its base, and the two together stood as a grey lens in the cloud)
				const double Fade = Haze * (bHeap && K == C.First ? NearShare : Each.Lod == 3 ? 1.0 - NearShare : Each.Lod == 2 ? 1.0 : Each.Lod == 1 ? FarShare : NearShare);
				// (drawn from the sheet's shapes, a heap needs no veil to hold its lumps together, and the veil's shape, as
				// broad as the cloud and up its windward side from the leaning lumps, hung by it as a grey ghost seen from
				// above; a deck's tile keeps its broad top)
				if (Fade * Here.Show * Each.Alpha < 0.002 || (bHeap && Each.Lod == 2))
				{
					continue;
				}
				const Vec At(Each.At.X + Shift.X, Each.At.Y + Shift.Y, C.At.Z + Here.Lift + (Each.At.Z - C.At.Z) * Here.Squash);
				const Vec D = At - Seen;
				Sorted.push_back({ K, Index, D.Dot(D), Fade, false, Vec() });
			}
			// (its shadow, where the sun's way through its middle meets the sea: a dark disc of its body's breadth, 1.5 m
			// over the water and laid over the land where it falls there (the art director, on build 1722: over Oahu the
			// clouds floated with nothing under them); out to ShadowReach from the eye)
			// (a painter of one room lays none unless given the shadows' room: the far room's lays them all)
			// (a small one's too, out to 20 times its length and a kilometre, 3 km for a 100 m one, and counted as if twice
			// as far, so that with no room for them all the small ones' go first: the art director on a1acb552's frames,
			// "if cheap, give each a soft shadow on the sea, offset down-sun, at about 0.75 of the sea's value". Until
			// then only a cloud 600 m across or more laid one)
			const bool bSmallShadow = C.W < 600.0;
			const double ShadowOut = bSmallShadow ? std::min(ShadowReach, 20.0 * C.W + 1000.0) : ShadowReach;
			if (bShadows && (bApart || Only.bAll) && !C.bScrap)
			{
				const double High = C.At.Z + Here.Lift + 0.45 * Here.Squash * C.H;
				const Vec Lies(LX - SunWay.X / SunWay.Z * High, LY - SunWay.Y / SunWay.Z * High, 1.5);
				const Vec D = Lies - Seen;
				const double Fade = Here.Show * Smooth((ShadowOut - D.Length()) / (bSmallShadow ? 0.25 * ShadowOut : 2000.0));
				if (Fade * ShadowDark >= 0.002)
				{
					(bApart ? Shaded : Sorted).push_back({ C.First, Index, D.Dot(D) * (bSmallShadow ? 4.0 : 1.0), Fade, true, Lies });
				}
			}
		}
		// (far to near, as see-through things are drawn; with no room for them all, the farthest go: only those drawn are
		// put in order, the rest set before them as they fall. The many small clouds of 22 November put twice the room's
		// puffs in reach, and ordering them all was a third of a paint)
		auto Farther = [](const Order& A, const Order& B) { return A.Deep > B.Deep; };
		const int Count = static_cast<int>(Sorted.size());
		LastDropped = std::max(0, Count - Pool);
		if (LastDropped > 0)
		{
			std::nth_element(Sorted.begin(), Sorted.begin() + LastDropped, Sorted.end(), Farther);
		}
		std::sort(Sorted.begin() + LastDropped, Sorted.end(), Farther);
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
		// (a puff's rings lit at every other point, each between two given the mean of its two neighbours' light: the
		// light round a puff is smooth, and the cloud's light, the body's face where the eye's line meets it, is the
		// dearest sum of a paint; the cloud test's budget. Coloured after, so that the ink's share holds on each)
		std::vector<double> HeartLight(static_cast<size_t>(SideCount)), RimLight(static_cast<size_t>(SideCount));
		// (and, drawn from the sheet's shapes, how near the cloud's outline each lit point lies, and each point's place on
		// the shape: the sheet's coordinates are set after the rings are lit, the unlit points' nearness the mean of
		// their neighbours', as their light is)
		std::vector<float> HeartOff(static_cast<size_t>(SideCount)), RimOff(static_cast<size_t>(SideCount));
		std::vector<double> RingX(static_cast<size_t>(SideCount)), RingY(static_cast<size_t>(SideCount));
		auto Mend = [&](size_t First)
		{
			for (int Ring = 0; Ring < 2; ++Ring)
			{
				const std::vector<double>& Lights = Ring == 0 ? HeartLight : RimLight;
				const size_t Start = First + 1 + static_cast<size_t>(Ring * SideCount);
				for (int K = 1; K < SideCount; K += 2)
				{
					const Rgb Col = Tone(0.5 * (Lights[static_cast<size_t>(K - 1)] + Lights[static_cast<size_t>((K + 1) % SideCount)]));
					uint8_t* Me = &OutRgba[(Start + static_cast<size_t>(K)) * 4];
					Me[0] = Byte(Col.R);
					Me[1] = Byte(Col.G);
					Me[2] = Byte(Col.B);
				}
			}
		};
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
			// 1.5 m over the water; over the land each of its points over the highest ground under it and under its
			// neighbours (by as much as they differ, below), so that the ground between them does not stand through it.
			// (The ground is asked once for each point: three times as many asks cost the painter about a millisecond
			// at Pearl Harbor.)
			// (a small one's as long and as narrow as it is, along its own way, and drawn out down-sun a third as much as a
			// big one's: a flat rag's shadow is its own shape, a heap's drawn out by its height)
			const bool bLong = Cl.Long > 1.001;
			const double SunLong = bLong ? 1.0 + (Stretch - 1.0) / 3.0 : Stretch;
			const Vec Ax = bLong ? Vec(std::cos(Cl.LongWay), std::sin(Cl.LongWay), 0.0) : SunFlat, Ay(-Ax.Y, Ax.X, 0.0);
			const double Broad = (bLong ? 0.47 : 0.42) * Cl.W, Narrow = Broad / std::max(1.0, Cl.Long);
			const double A = ShadowDark * O.Fade;
			Laid[0] = C;
			for (int K = 0; K < SideCount; ++K)
			{
				const Vec Flat = Ax * (Broad * Cos[static_cast<size_t>(K)]) + Ay * (Narrow * Sin[static_cast<size_t>(K)]);
				const Vec Way = (Flat + SunFlat * (Flat.Dot(SunFlat) * (SunLong - 1.0))) * (1.0 + 0.08 * Lumps3(K));
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
			// (over the highest ground under it by as much as the ground between the points asked might rise: half a
			// metre on flat land, so that a shadow passing over a flat field lies on it and not over the aircraft
			// waiting there, and 4 m for each metre the points differ, at most 25 m, to clear the hills between them
			// without lifting a whole shadow off a gentle slope: the pre-check on Bremen and Oahu, 20 November)
			auto Lay = [](double Highest, double Lowest) { return Highest > 0.5 ? Highest + std::min(25.0, 0.5 + 4.0 * (Highest - Lowest)) : 1.5; };
			double Middle = Under[0], MiddleLow = Under[0];
			for (int K = 0; K < SideCount; ++K)
			{
				Middle = std::max(Middle, Under[static_cast<size_t>(1 + K)]);
				MiddleLow = std::min(MiddleLow, Under[static_cast<size_t>(1 + K)]);
			}
			PutHere(First, Vec(C.X, C.Y, Lay(Middle, MiddleLow)), Shade, A);
			for (int K = 0; K < SideCount; ++K)
			{
				const size_t Heart = static_cast<size_t>(1 + K), Rim = static_cast<size_t>(1 + SideCount + K);
				const size_t HeartOn = static_cast<size_t>(1 + (K + 1) % SideCount), HeartBack = static_cast<size_t>(1 + (K + SideCount - 1) % SideCount);
				const size_t RimOn = static_cast<size_t>(1 + SideCount + (K + 1) % SideCount), RimBack = static_cast<size_t>(1 + SideCount + (K + SideCount - 1) % SideCount);
				const double HeartTop = std::max({ Under[Heart], Under[0], Under[HeartOn], Under[HeartBack], Under[Rim] });
				const double RimTop = std::max({ Under[Rim], Under[Heart], Under[RimOn], Under[RimBack] });
				const double HeartLow = std::min({ Under[Heart], Under[0], Under[HeartOn], Under[HeartBack], Under[Rim] });
				const double RimLow = std::min({ Under[Rim], Under[Heart], Under[RimOn], Under[RimBack] });
				PutHere(First + Heart, Vec(Laid[Heart].X, Laid[Heart].Y, Lay(HeartTop, HeartLow)), Shade, A * 0.8);
				PutHere(First + Rim, Vec(Laid[Rim].X, Laid[Rim].Y, Lay(RimTop, RimLow)), Shade, 0.0);
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
			// (a heap's one base, from the sheet's feet: in the cloud's middle at its foot, as wide as the body is there
			// and some two fifths of its height, the body over it overlapping into it; the art director on round 18c, "one
			// base per cloud, flat but ragged, the grey rising into the body over about a third of its height")
			// (its base layer's other puffs are drawn as the body's lumps: each a base of its own drew a grey pill, stacked
			// into shelves far off)
			const bool bHeapHere = bShapes && Cl.Count > 1 && F.Puffs[static_cast<size_t>(Cl.First)].bFlatFoot;
			const bool bFoot = bHeapHere && O.Index == Cl.First;
			const bool bFlatFoot = Each.bFlatFoot && (!bHeapHere || bFoot);
			// (and as round as the body's lumps and lit as the body is at their height: as flat as a base's, and as grey,
			// they stood as grey lenses in the cloud)
			const bool bLowLump = bHeapHere && Each.bFlatFoot && !bFoot;
			const double FootZ = Cl.At.Z + Here.Lift, HeapHigh = std::max(1.0, Here.Squash * Cl.H);
			// (a small one's across it: drawn out along it below)
			const double FootSpan = bFoot ? 0.4 * Cl.W / std::max(1.0, Cl.Long) : 0.0;
			Vec C = PlacedAt(O);
			if (bFoot)
			{
				C.X = Cl.At.X + Here.Shift.X;
				C.Y = Cl.At.Y + Here.Shift.Y;
			}
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
				// (a shadow drawn among the shapes, by a painter given no room for shadows apart: every point of it at the
				// middle of the first shape, solid cloud, so that it is its soft disc still)
				for (size_t K = 0; bShapes && K < static_cast<size_t>(Per); ++K)
				{
					float* Uv = &(*OutUvs)[(First + K) * 4];
					Uv[0] = Uv[1] = 0.5f / FragmentCells;
				}
				continue;
			}
			// (and a far body drawn as a dome, a whole cloud on its flat underside, as tall as the cloud: squashed to its
			// disc's flatness, the dome's top half went, and far clouds stood as stacked grey lenses, the art director's
			// "shelves in the distance" on round 18c)
			const bool bFarDome = bShapes && Each.Lod == 3 && Seen.Z < FootZ + HeapHigh;
			const double Flat = bFoot ? Clamp(0.5 * HeapHigh / (0.84 * FootSpan), 0.3, 1.0)
				: bFarDome ? Clamp(1.7 * HeapHigh / std::max(1.0, Cl.W), 0.4, 1.0) : std::max(0.08, (bLowLump ? 0.85 : Each.Flat) * Here.Squash);
			Vec Fwd = C - Seen;
			const double Dist = Fwd.Length();
			Near = std::min(Near, Dist - Each.R * Each.Long);
			if (Loose > 0.0)
			{
				const double Across = 1.15 * Each.R * Each.Long;
				Slackest = std::min(Slackest, std::min(Loose * Dist * Dist * Dist / (Across * Across), LooseTurn * Dist));
			}
			Fwd = Fwd.Unit(Vec(0.0, 0.0, -1.0));
			Vec Right = Fwd.Cross(Vec(0.0, 0.0, 1.0));
			Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
			Vec Up = Right.Cross(Fwd);
			Up = Up - Vec(0.0, 0.0, Up.Z * (1.0 - Flat)); // (a flat base stays flat seen from the side)
			// (a puff drawn out along its way (a small cloud's, Long), by as much as that way lies across the picture: all
			// of it looked down on, none looked along it. A point of the ring, so much right and up, drawn out along it)
			const double LongHere = bFoot ? Cl.Long : Each.Long;
			double LongR = 0.0, LongU = 0.0, LongMore = 0.0;
			if (LongHere > 1.001)
			{
				const double Way = bFoot ? Cl.LongWay : Each.LongWay;
				const double LR = std::cos(Way) * Right.X + std::sin(Way) * Right.Y, LU = std::cos(Way) * Up.X + std::sin(Way) * Up.Y;
				const double InPicture = std::sqrt(LR * LR + LU * LU);
				if (InPicture > 1e-6)
				{
					LongR = LR / InPicture;
					LongU = LU / InPicture;
					LongMore = (LongHere - 1.0) * std::min(1.0, InPicture);
				}
			}
			auto DrawnOut = [&Right, &Up, LongR, LongU, LongMore](double R, double U)
			{
				const double On = (R * LongR + U * LongU) * LongMore;
				return Right * (R + On * LongR) + Up * (U + On * LongU);
			};
			// (no disc across the lens as the eye passes through a cloud: it fades from 1.2 radii to 0.6; with a sweep, by
			// the nearest the eye comes to it along that way)
			double Lens = Dist;
			if (bSweep)
			{
				const Vec To = C - Seen;
				const double Along = Clamp((To.X * Sweep.X + To.Y * Sweep.Y + To.Z * Sweep.Z) * BySweep2, 0.0, 1.0);
				Lens = (To - Sweep * Along).Length();
			}
			const double Alpha = Here.Show * O.Fade * Each.Alpha * Clamp((Lens - 0.6 * Each.R * Each.Long) / (0.6 * Each.R * Each.Long), 0.0, 1.0);
			// Light and shade from the cloud's shape (the art director): each point of the puff lit as the point of the
			// cloud's rounded body it stands for, its way out from the body's middle to the sun; a deck's tile a flatter
			// body (a floor of cells), darker at its rim where it meets the next. Seen from under it, a cloud is its grey
			// base.
			const Vec Mid(Cl.At.X + Here.Shift.X, Cl.At.Y + Here.Shift.Y, Cl.At.Z + Here.Lift + 0.45 * Here.Squash * Cl.H);
			const double Wide = std::max(60.0, 0.5 * Cl.W * (F.Sky.bDeck && !Cl.bScrap ? 1.25 : 1.0)), Deep = std::max(60.0, 0.5 * Here.Squash * Cl.H);
			// (seen from under its foot and looking up steeply at it, its grey base; from the side, level with the base or
			// a little under it, its sides show, lit as they are: the art director on round 18b, eye 2, "grey all over,
			// darker than the sky beside them". By the puff's height over the eye, every puff above the eye was darkened,
			// the sunlit sides of a cloud seen across the sea with them)
			const double FootOver = Cl.At.Z + Here.Lift - Seen.Z;
			const double Below = Clamp(FootOver / 300.0, 0.0, 1.0) * Smooth((Fwd.Z - 0.2) / 0.5);
			const double ByWide = 1.0 / Wide, ByDeep = 1.0 / Deep;
			// (and, a third of it, as the point of the puff's own round lump it is: lit across the lump, the side toward
			// the sun brighter, so a small cloud's sunlit cap is a matte gradient, not a white pearl: the art director,
			// 15:17Z. Facing is how much the lump's middle, turned to the eye, faces the sun.)
			const double Facing = -(Fwd.X * SunWay.X + Fwd.Y * SunWay.Y + Fwd.Z * SunWay.Z);
			// (the cloud's body as the eye sees it: a point of the picture is the body's near face where the eye's line
			// through it meets the body, and that face's way out is what the sun lights, so the whole face is lit with the
			// sun behind the eye and dark with the sun ahead, lit at its edge only; not the way out of the point itself,
			// which is across the eye's line, half lit either way: the art director on round 18b, eye 2. Where asked,
			// how near the body's outline the point is, 0 at its middle, 1 on it or outside)
			// (in single precision, everything that does not change across the puff made once: the eye's way and the sun's in
			// the body's measure; a paint calls this some hundred thousand times, and in doubles it took a quarter of one)
			const float Bw = static_cast<float>(ByWide), Bd = static_cast<float>(ByDeep);
			const float MidX = static_cast<float>(Mid.X), MidY = static_cast<float>(Mid.Y), MidZ = static_cast<float>(Mid.Z);
			float Fx = static_cast<float>(Fwd.X) * Bw, Fy = static_cast<float>(Fwd.Y) * Bw, Fz = static_cast<float>(Fwd.Z) * Bd;
			{
				const float FwLong = std::max(1e-9f, std::sqrt(Fx * Fx + Fy * Fy + Fz * Fz));
				Fx /= FwLong;
				Fy /= FwLong;
				Fz /= FwLong;
			}
			const float Sx = static_cast<float>(SunWay.X) * Bw, Sy = static_cast<float>(SunWay.Y) * Bw, Sz = static_cast<float>(SunWay.Z) * Bd;
			const float Bw2 = Bw * Bw, Bd2 = Bd * Bd;
			auto Across2 = [&](const Vec& P, float& Ox, float& Oy, float& Oz, float& Along)
			{
				Ox = (static_cast<float>(P.X) - MidX) * Bw;
				Oy = (static_cast<float>(P.Y) - MidY) * Bw;
				Oz = (static_cast<float>(P.Z) - MidZ) * Bd;
				Along = Ox * Fx + Oy * Fy + Oz * Fz;
				return std::max(0.0f, Ox * Ox + Oy * Oy + Oz * Oz - Along * Along);
			};
			auto FaceAt = [&](const Vec& P, float& Off2, float& Depth)
			{
				float Ox, Oy, Oz, Along;
				Off2 = Across2(P, Ox, Oy, Oz, Along);
				// (on the outline or outside it, the way out across the eye's line; inside, toward the eye as well. Only the
				// way matters, not its length, so it is made a unit only at the end, in the board's measure: a flat body's
				// faces lean up more than a round one's)
				Depth = std::sqrt(std::max(0.0f, 1.0f - Off2));
				const float Toward = Depth + Along;
				const float Ax = Ox - Fx * Toward, Ay = Oy - Fy * Toward, Az = Oz - Fz * Toward;
				const float Long2 = (Ax * Ax + Ay * Ay) * Bw2 + Az * Az * Bd2;
				return Long2 > 1e-18f ? static_cast<double>((Ax * Sx + Ay * Sy + Az * Sz) / std::sqrt(Long2)) : SunWay.Z;
			};
			// (how far the sun stands ahead of the eye, past the cloud: 0 to 1; and how much the cloud is lit from behind
			// it as the eye sees it, the sun ahead or the eye under it, looking up at its shaded underside)
			const double SunAhead = bShapes ? Smooth((-Facing - 0.35) / 0.55) : 0.0;
			// (the eye under the cloud's foot looking up at it at all, the sun not behind the eye: its shaded underside)
			const double Beneath = bShapes ? Clamp(FootOver / 300.0, 0.0, 1.0) * Smooth((Fwd.Z - 0.03) / 0.25) * (1.0 - Smooth((Facing - 0.35) / 0.55)) : 0.0;
			const double Against = std::max(SunAhead, Beneath);
			// (and how thick the cloud is for the light coming through it: a small cloud, a few hundred metres, is lit
			// through nearly all over, a silver-grey; only a big one's body goes dark)
			const double Thick = Smooth((HeapHigh - 120.0) / 600.0);
			// (a deck's own puffs, its scraps apart: drawn from the lids where the sheet's shapes are drawn)
			const bool bLidTile = bShapes && F.Sky.bDeck && !Cl.bScrap;
			float LastOff2 = 0.0f;
			// (how near the body's outline a point is, from its squared distance across the body: 0 within 0.55 of the
			// way out, 1 from 0.95)
			auto NearOutline = [](float Off2) { return Smooth((static_cast<double>(Off2) - 0.3025) * (1.0 / 0.6)); };
			const double PerHigh = 1.0 / HeapHigh, PerLower = 1.0 / (0.38 * HeapHigh);
			auto LitAt = [&](const Vec& P, double Rim, double Own)
			{
				float Off2 = 0.0f, Depth = 0.0f;
				const double Body = FaceAt(P, Off2, Depth);
				LastOff2 = Off2;
				// (the cloud's own form leads, three parts in four, and what of it stands between the puff and the sun
				// darkens it: the brief's one sun on each cloud, the shade side grey-blue and the sunlit side bright)
				// (a deck's lid a broad flat body: its tile's own roundness only a touch of it, the sheet's bumps the rest)
				const double Shape = bLidTile ? 0.64 + 0.5 * (0.2 * Body + 0.1 * Own) : 0.5 + 0.5 * (0.75 * Body + 0.25 * Own);
				// (a heap's base lit as the body is at its height, so that the two are of a piece)
				const double OwnLit = bFoot || bLowLump ? 0.2 + 0.65 * Clamp((P.Z - FootZ) * PerHigh, 0.0, 1.0) : Each.Lit;
				// (what of the cloud stands between a puff and the sun darkens the faces turned from the sun; with the sun
				// behind the eye the faces seen are the sunward ones, and it fades)
				const double Lit = Clamp(0.25 * OwnLit + 0.75 * Shape - Each.Trough * Rim, 0.0, 1.0) * (1.0 - 0.55 * Each.Shadowed * Clamp(1.0 - Facing, 0.0, 1.0));
				if (!bShapes)
				{
					return Lit * (1.0 - 0.6 * Below);
				}
				// Drawn from the sheet's shapes (the art director on round 18c): the lower third of a cloud greying down to
				// its base; seen from under it, the grey less toward its outline, where the body's sides show past the base;
				// and with the sun ahead, the body darker where the eye's way through it is long, its thin edges lit through
				// (how near the body's outline the point is: 0 at its middle, 1 on it)
				const double Edge = bLidTile ? 0.0 : NearOutline(Off2);
				const double Graded = Lit * (1.0 - 0.6 * Below * (1.0 - 0.6 * Edge)) * (0.8 + 0.2 * Smooth((P.Z - FootZ) * PerLower));
				return Graded * (1.0 - 0.3 * Against * Thick * Depth) + 0.25 * Against * Edge + 0.12 * Against * (1.0 - Thick);
			};
			Put(First, C, Tone(LitAt(C, 0.0, Facing)), Alpha);
			const float MiddleOff = LastOff2;
			if (bShapes)
			{
				// One of the sheet's shapes (MakeFragments), a base's for a base: its edge, its lumps and their light are the
				// shape's; the points carry the cloud's light where they lie and the puff's alpha all over. Its top (the
				// crisp, sunlit edge) is turned toward the sun as the sun stands in the picture, give or take a fifth of a
				// right angle for its own look, a base upright with its underside level; with the sun behind the eye or
				// ahead, the shape's light is all round it and which way it is turned is no matter.
				const int Cells = FragmentCells * FragmentCells, Tops = Cells - FragmentBases - FragmentFeet - FragmentLids;
				// (a far body, the whole cloud in one shape, as a base's, a dome on a flat underside, upright, while the eye
				// is under the cloud's top; looked down on, a top's, turned to the sun: from above no flat underside shows)
				const double CloudTop = Cl.At.Z + Here.Lift + Here.Squash * Cl.H;
				// (a deck's every puff one of the lids, lying flat and turned to the sun as the tops are: the art director's
				// round 18d steer, one lid of cells, not heaps on a floor)
				const bool bUpright = !bLidTile && (bFlatFoot || (Each.Lod == 3 && Seen.Z < CloudTop));
				const int Pick = bLidTile ? Cells - FragmentLids + O.Index % FragmentLids : bFoot ? Tops + FragmentBases + O.Cloud % FragmentFeet : bUpright ? Tops + O.Index % FragmentBases : O.Index % Tops;
				const double Mirror = (O.Index / Cells) % 2 == 1 ? -1.0 : 1.0;
				const double SunAcross = Right.X * SunWay.X + Right.Y * SunWay.Y + Right.Z * SunWay.Z;
				const double SunUp = Up.X * SunWay.X + Up.Y * SunWay.Y + Up.Z * SunWay.Z;
				double Toward = 0.5 * Pi;
				if (!bUpright)
				{
					Toward = (SunAcross * SunAcross + SunUp * SunUp > 1e-8 ? std::atan2(SunUp, SunAcross) : 0.5 * Pi) + 0.3 * PhaseSines(false)[Phase];
				}
				const double Turn = 0.5 * Pi - Toward, CT = std::cos(Turn), ST = std::sin(Turn);
				const double CU = (Pick % FragmentCells + 0.5) / FragmentCells, CV = (Pick / FragmentCells + 0.5) / FragmentCells, Half = 0.49 / FragmentCells;
				const float Behind = static_cast<float>(Smooth((Facing - 0.35) / 0.55)), Ahead = static_cast<float>(std::min(Against, 1.0 - Smooth((Facing - 0.35) / 0.55)));
				const float Lumpy = bLidTile ? 0.85f : static_cast<float>((0.45 + 0.5 * Beneath) * (0.25 + 0.75 * Thick));
				// (a point's place on the sheet from its way out across the shape, x right and y up, a share of the rim. With
				// the sun ahead, the shape's bright thin edges only where it is the cloud's own outline: inside the cloud a
				// lump's edge is a fold of the dark body, not silver, so there the shape's even light, as with the sun behind)
				auto Sheet = [&](size_t At, float Off2, double SX, double SY)
				{
					// (a lid's light from under it is its own grey, no bright outline round each puff: the art director's round
					// 18d steer, "no white halo on every lump")
					const float Outline = Ahead > 0.0f && !bLidTile ? static_cast<float>(NearOutline(Off2)) : 0.0f;
					// (inside it, the shape's even light a little over half, its dark thick middle and lit thin lumps the rest,
					// more of them from under the cloud: seen from under it or against the sun, a lumpy body, never a flat grey
					// cushion. The art director on round 18c)
					float* Uv = &(*OutUvs)[At * 4];
					Uv[0] = static_cast<float>(CU + Half * SX);
					Uv[1] = static_cast<float>(CV - Half * SY);
					Uv[2] = Behind + Ahead * (1.0f - Lumpy) * (1.0f - Outline);
					Uv[3] = Ahead * (Lumpy + (1.0f - Lumpy) * Outline);
				};
				// (a deck's lid fills its cell, so its tile is drawn at its puff's own width: the lids meet the next tile's
				// in frayed seams, and the noise's breaks stay open)
				const double Span = bFoot ? FootSpan : SpriteSpan * Each.R;
				// (a base's and a far body's flat underside on the cloud's own foot, every one of a cloud's on the same
				// level, not stacked a little apart as their sizes put them: the art director on round 18b, "a base is
				// the cloud's own flat underside")
				Vec Middle = C;
				if (bUpright)
				{
					// (and, seen from under the foot, each base's underside where the cloud's foot is in the picture, not where
					// its own place puts it: one line under the cloud, not a stack of saucers with the near ones' lines over the
					// far ones'. The art director on round 18b, "bases continuous with the body, not saucers". Moved at most a
					// third of the shape's height, so a base stays under its own cloud)
					double Foot = Cl.At.Z + Here.Lift;
					const double CloudAway = std::hypot(Cl.At.X + Here.Shift.X - Seen.X, Cl.At.Y + Here.Shift.Y - Seen.Y);
					if (bFlatFoot && FootOver > 0.0 && CloudAway > Each.R)
					{
						const double Mine = std::hypot(C.X - Seen.X, C.Y - Seen.Y);
						Foot += Clamp(FootOver * (Mine / CloudAway - 1.0), -0.33 * Span, 0.33 * Span);
					}
					Middle.Z = Foot + FragmentFloor / 0.98 * Span * std::max(0.0, Up.Z);
					OutPoints[First] = Middle;
				}
				// (a base looked down on is under its dome: it fades as the eye rises over it, gone a radius and a half
				// above it, so that from above its flat underside never shows round the dome's edge)
				const double Shown = bFlatFoot ? Alpha * Smooth(Clamp(1.0 - (Seen.Z - C.Z) / (1.5 * Each.R), 0.0, 1.0)) : Alpha;
				OutRgba[First * 4 + 3] = static_cast<uint8_t>(Clamp(Shown, 0.0, 1.0) * 255.0 + 0.5);
				Sheet(First, MiddleOff, 0.0, 0.0);
				// (a base under a taller body melts upward into it: its dome's top is the body's lower part, not an edge of
				// its own, its sides softer, so only its flat underside is crisp; a small cloud's base keeps its whole outline.
				// The art director on round 18b, eye 2, "bases continuous with the body, not saucers")
				const double Melt = bFlatFoot && !bFoot && !bLidTile ? Smooth((CloudTop - (Middle.Z + 0.6 * Span)) / (0.6 * Span)) : 0.0;
				for (int K = 0; K < SideCount; ++K)
				{
					const double TurnedC = Cos[static_cast<size_t>(K)] * TurnC - Sin[static_cast<size_t>(K)] * TurnS;
					const double TurnedS = Sin[static_cast<size_t>(K)] * TurnC + Cos[static_cast<size_t>(K)] * TurnS;
					const Vec Way = DrawnOut(TurnedC, TurnedS);
					const Vec Heart = Middle + Way * (0.55 * Span), Rim = Middle + Way * Span;
					const double SX = Mirror * (TurnedC * CT - TurnedS * ST), SY = TurnedC * ST + TurnedS * CT;
					const double Upward = Melt * Smooth((TurnedS + 0.3) / 0.9);
					const bool bLit = K % 2 == 0;
					const size_t At = static_cast<size_t>(K);
					if (bLit)
					{
						HeartLight[At] = LitAt(Heart, 0.45, Facing);
						HeartOff[At] = LastOff2;
						RimLight[At] = LitAt(Rim, 1.0, Facing);
						RimOff[At] = LastOff2;
					}
					RingX[At] = SX;
					RingY[At] = SY;
					Put(First + 1 + K, Heart, Tone(HeartLight[At]), Shown * (1.0 - 0.35 * Upward));
					Put(First + 1 + SideCount + K, Rim, Tone(RimLight[At]), Shown * (1.0 - Upward));
				}
				for (int K = 0; K < SideCount; ++K)
				{
					const size_t At = static_cast<size_t>(K), Was = static_cast<size_t>((K + SideCount - 1) % SideCount), Next = static_cast<size_t>((K + 1) % SideCount);
					const bool bLit = K % 2 == 0;
					Sheet(First + 1 + At, bLit ? HeartOff[At] : 0.5f * (HeartOff[Was] + HeartOff[Next]), 0.55 * RingX[At], 0.55 * RingY[At]);
					Sheet(First + 1 + SideCount + At, bLit ? RimOff[At] : 0.5f * (RimOff[Was] + RimOff[Next]), RingX[At], RingY[At]);
				}
				Mend(First);
				continue;
			}
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
				Vec Way = DrawnOut(TurnedC, TurnedS);
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
				const bool bLit = K % 2 == 0;
				const size_t At = static_cast<size_t>(K);
				HeartLight[At] = bLit ? LitAt(Heart, 0.5, Out * WaySun + std::sqrt(1.0 - Out * Out) * Facing) : 0.0;
				RimLight[At] = bLit ? LitAt(Rim, 1.0, WaySun) : 0.0;
				Put(First + 1 + K, Heart, Tone(HeartLight[At]), Alpha * (1.0 - 0.45 * Soft));
				Put(First + 1 + SideCount + K, Rim, Tone(RimLight[At]), 0.0);
			}
			Mend(First);
		}
		LastShadows = 0;
		if (bApart)
		{
			// (the shadows' own room: far to near, the nearest MostShadows, the room not needed first)
			const int ShadowCount = static_cast<int>(Shaded.size());
			const int ShadowDropped = std::max(0, ShadowCount - MostShadows);
			if (ShadowDropped > 0)
			{
				std::nth_element(Shaded.begin(), Shaded.begin() + ShadowDropped, Shaded.end(), Farther);
			}
			std::sort(Shaded.begin() + ShadowDropped, Shaded.end(), Farther);
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

	void Rooms::SetSprites(bool bOn)
	{
		Near.SetSprites(bOn);
		Far.SetSprites(bOn);
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
			Near.Paint(F, Eye, Drift, Sun, Light, NearPoints, NearRgba, nullptr, nullptr, &NearUvs);
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
		Far.Paint(F, Eye, Drift, Sun, Light, FarPoints, FarRgba, &Shadows, &ShadowRgba, &FarUvs);
		Cut.bNear = true;
		Near.SetPart(Cut);
		Near.Paint(F, Eye, Drift, Sun, Light, NearPoints, NearRgba, nullptr, nullptr, &NearUvs);
		bBoth = true;
	}

	Chip::Chip(int InRoom, int InSides)
		: RoomSlots(std::max(1, InRoom)), SideCount(std::max(6, InSides))
	{
		// (each puff's ring turned by its own part of a side, as the painter's TurnCos and TurnSin)
		for (int K = 0; K < 97; ++K)
		{
			const double Golden = K * 0.6180339887;
			const double A = (Golden - std::floor(Golden)) * 2.0 * Pi / SideCount;
			TurnCos.push_back(std::cos(A));
			TurnSin.push_back(std::sin(A));
		}
		Cells.assign(static_cast<size_t>(Rows()) * Wide * 4, 0.0f);
	}

	std::vector<int> Chip::Triangles() const
	{
		return Painter(SideCount).Triangles(RoomSlots);
	}

	void Chip::PointPlaces(std::vector<float>& OutRing, std::vector<float>& OutSlot) const
	{
		const size_t Points = static_cast<size_t>(RoomSlots) * PerPuff();
		OutRing.assign(Points * 2, 0.0f);
		OutSlot.assign(Points * 2, 0.0f);
		for (int Slot = 0; Slot < RoomSlots; ++Slot)
		{
			const float Column = static_cast<float>((Slot % PuffsPerRow) * TexelsPerPuff), Row = static_cast<float>(1 + Slot / PuffsPerRow);
			for (int P = 0; P < PerPuff(); ++P)
			{
				const size_t At = (static_cast<size_t>(Slot) * PerPuff() + P) * 2;
				// (the middle 0; the heart ring's points 1 to Sides, the rim's after them, as Painter::Triangles lays them)
				const int Ring = P == 0 ? 0 : P <= SideCount ? 1 : 2;
				const int K = Ring == 0 ? 0 : Ring == 1 ? P - 1 : P - 1 - SideCount;
				OutRing[At] = static_cast<float>(K);
				OutRing[At + 1] = static_cast<float>(Ring);
				OutSlot[At] = Column;
				OutSlot[At + 1] = Row;
			}
		}
	}

	void Chip::SetOrderTurn(double InTurn)
	{
		Turn = std::isfinite(InTurn) ? Clamp(InTurn, 0.002, 0.2) : DefaultOrderTurn;
	}

	int Chip::SlotPuff(int Slot) const
	{
		return Slot >= 0 && Slot < LastLaid ? Order[static_cast<size_t>(LastDropped + Slot)].Puff : -1;
	}

	int Chip::SlotCloud(int Slot) const
	{
		return Slot >= 0 && Slot < LastLaid ? Order[static_cast<size_t>(LastDropped + Slot)].Cloud : -1;
	}

	bool Chip::Wants(const Vec& Eye, const Vec& Drift) const
	{
		if (!bLaid)
		{
			return true;
		}
		const double Carried = (Drift - LastDrift).Length();
		if ((bEasing && Carried > 3.0) || (bClearing && Carried > 15.0))
		{
			return true;
		}
		return ((Eye - Drift) - LastEye).Length() > LastSlack;
	}

	void Chip::Lay(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light)
	{
		// Painter::Paint's gathering, for the sheet's shapes and the whole sky (one painter, all of it), without its
		// points: which clouds, where (the copy nearest the eye, lifted, squashed, thinned by the clearing), and which of
		// their puffs, far to near; each out past the reach and its level's fade by the Margin, so that the chip, which
		// fades them itself, has every puff it could draw before the next lay.
		const Vec Seen = Eye - Drift;
		const size_t CloudCount = F.Clouds.size();
		if (Serial != F.Serial || Lifts.size() != CloudCount)
		{
			Lifts.assign(CloudCount, Kept());
			Serial = F.Serial;
		}
		Where.assign(CloudCount, Placed());
		Order.clear();
		bEasing = false;
		bClearing = false;
		const Vec Carry = F.DriftAt(1.0);
		const Vec Lead = Carry.Length() > 1e-9 ? Carry * (250.0 / Carry.Length()) : Vec();
		const Vec SunWay = Sun.Unit(Vec(-0.35, -0.45, 0.82));
		double Near = 1e18;
		int Drawn = 0;
		for (size_t I = 0; I < CloudCount; ++I)
		{
			const Cloud& C = F.Clouds[I];
			const int Index = static_cast<int>(I);
			const Vec Shift = F.ShiftNear(Index, Seen.X, Seen.Y);
			const double LX = C.At.X + Shift.X, LY = C.At.Y + Shift.Y;
			if (std::hypot(LX - Seen.X, LY - Seen.Y) > Reach + C.W + Margin)
			{
				continue;
			}
			// (its lift eased as the painter eases it)
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
			// (a cumulus whose near edge comes within 4 km of the target, where the clearing thins what the wind carries in
			// (Field::ShownAt): its share shown moves with the wind's carry, so the table is laid again as it does)
			if (!F.Sky.bDeck && std::hypot(WX - F.TargetX, WY - F.TargetY) - 0.45 * C.W < 4200.0)
			{
				bClearing = true;
			}
			Placed& Here = Where[I];
			Here = F.PlaceLifted(Index, Shift, Drift, Lift.Shown);
			if (Here.Show < 0.002)
			{
				continue;
			}
			const Vec Middle(LX, LY, C.At.Z + Here.Lift + 0.5 * Here.Squash * C.H);
			const double Far = (Middle - Seen).Length();
			// (a small cloud, under 600 m across, drawn out to SmallSight times its width, as the painter's)
			const double Sight = C.W < 600.0 ? SmallSight * C.W : 0.0;
			if (Smooth((Reach - (Far - Margin)) / (Reach / 3.0)) <= 0.0 || (Sight > 0.0 && Smooth((Sight - (Far - Margin)) / (0.2 * Sight)) <= 0.0))
			{
				continue;
			}
			bEasing = bEasing || bStillEasing;
			const double Haze = Smooth((Reach - Far) / (Reach / 3.0)) * (Sight > 0.0 ? Smooth((Sight - Far) / (0.2 * Sight)) : 1.0);
			const double NearShare = Smooth((NearAll + 1000.0 - Far) / 1000.0), FarShare = Smooth((FarAll + 1500.0 - Far) / 1500.0);
			const bool bHeap = C.Count > 1 && F.Puffs[static_cast<size_t>(C.First)].bFlatFoot;
			for (int K = C.First; K < C.First + C.Count; ++K)
			{
				const Puff& Each = F.Puffs[static_cast<size_t>(K)];
				if ((bHeap && Each.Lod == 2) || Here.Show * Each.Alpha < 0.002)
				{
					continue;
				}
				// (each level's share is nought only past its own distance: kept if the eye, within the Margin, could
				// bring it in)
				const bool bFirst = bHeap && K == C.First;
				const bool bMay = bFirst || Each.Lod == 0 ? Far - Margin < NearAll + 1000.0 : Each.Lod == 1 ? Far - Margin < FarAll + 1500.0 : Each.Lod == 3 ? Far + Margin > NearAll : true;
				if (!bMay)
				{
					continue;
				}
				const Vec At(Each.At.X + Shift.X, Each.At.Y + Shift.Y, C.At.Z + Here.Lift + (Each.At.Z - C.At.Z) * Here.Squash);
				const Vec D = At - Seen;
				Order.push_back({ K, Index, D.Dot(D) });
				// (drawn from here, as the painter would: its nearest, for the order's slack)
				const double Share = bFirst ? NearShare : Each.Lod == 3 ? 1.0 - NearShare : Each.Lod == 2 ? 1.0 : Each.Lod == 1 ? FarShare : NearShare;
				if (Haze > 0.0 && Haze * Share * Here.Show * Each.Alpha >= 0.002)
				{
					const Vec Mid = bFirst ? Vec(LX, LY, At.Z) : At;
					Near = std::min(Near, (Mid - Seen).Length() - Each.R * Each.Long);
					++Drawn;
				}
			}
		}
		// (far to near, as see-through things are drawn; with no room for them all, the farthest go)
		auto Farther = [](const Slotted& A, const Slotted& B) { return A.Deep > B.Deep; };
		const int Count = static_cast<int>(Order.size());
		LastDropped = std::max(0, Count - RoomSlots);
		LastLaid = Count - LastDropped;
		if (LastDropped > 0)
		{
			std::nth_element(Order.begin(), Order.begin() + LastDropped, Order.end(), Farther);
		}
		std::sort(Order.begin() + LastDropped, Order.end(), Farther);
		LastDrawn = Drawn;

		// The header: the sun's way and a sheet cell's half width; the paints as the painter's Tone gives them (the
		// shaded side and the base under the ink's share, by the hour's light); the floor's share, the sprite's span, the
		// reach, the levels' distances, the puffs laid and a puff's sides.
		auto Put = [this](int Row, int Texel, double A, double B, double C, double D)
		{
			float* At = &Cells[(static_cast<size_t>(Row) * Wide + static_cast<size_t>(Texel)) * 4];
			At[0] = static_cast<float>(A);
			At[1] = static_cast<float>(B);
			At[2] = static_cast<float>(C);
			At[3] = static_cast<float>(D);
		};
		const Rgb BaseInked = BaseSide * (1.0 / Ink), ShadeInked = ShadeSide * (1.0 / Ink);
		Put(0, 0, SunWay.X, SunWay.Y, SunWay.Z, 0.49 / FragmentCells);
		Put(0, 1, BaseInked.R * Light.R, BaseInked.G * Light.G, BaseInked.B * Light.B, FragmentFloor / 0.98);
		Put(0, 2, ShadeInked.R * Light.R, ShadeInked.G * Light.G, ShadeInked.B * Light.B, Painter::SpriteSpan);
		Put(0, 3, SunSide.R * Light.R, SunSide.G * Light.G, SunSide.B * Light.B, Reach);
		Put(0, 4, NearAll, FarAll, LastLaid, SideCount);

		// Each puff: eight texels (AirSkyChip.inl).
		const int SheetCells = FragmentCells * FragmentCells, Tops = SheetCells - FragmentBases - FragmentFeet - FragmentLids;
		auto SheetU = [](int Pick) { return (Pick % FragmentCells + 0.5) / FragmentCells; };
		auto SheetV = [](int Pick) { return (Pick / FragmentCells + 0.5) / FragmentCells; };
		for (int Slot = 0; Slot < LastLaid; ++Slot)
		{
			const Slotted& O = Order[static_cast<size_t>(LastDropped + Slot)];
			const Puff& Each = F.Puffs[static_cast<size_t>(O.Puff)];
			const Cloud& Cl = F.Clouds[static_cast<size_t>(O.Cloud)];
			const Placed& Here = Where[static_cast<size_t>(O.Cloud)];
			const bool bHeapHere = Cl.Count > 1 && F.Puffs[static_cast<size_t>(Cl.First)].bFlatFoot;
			const bool bFoot = bHeapHere && O.Puff == Cl.First;
			const bool bFlatFoot = Each.bFlatFoot && (!bHeapHere || bFoot);
			const bool bLowLump = bHeapHere && Each.bFlatFoot && !bFoot;
			const double CloudX = Cl.At.X + Here.Shift.X, CloudY = Cl.At.Y + Here.Shift.Y;
			const double PX = bFoot ? CloudX : Each.At.X + Here.Shift.X, PY = bFoot ? CloudY : Each.At.Y + Here.Shift.Y;
			const double PZ = Cl.At.Z + Here.Lift + (Each.At.Z - Cl.At.Z) * Here.Squash;
			const int Row = 1 + Slot / PuffsPerRow, First = (Slot % PuffsPerRow) * TexelsPerPuff;
			const size_t Phase = static_cast<size_t>(O.Puff % 97);
			const int FootPick = Tops + FragmentBases + O.Cloud % FragmentFeet;
			// (a deck's own puffs, its scraps apart, drawn from the lids, lying flat: the painter's bLidTile)
			const bool bLidTile = F.Sky.bDeck && !Cl.bScrap;
			const int UpPick = bFoot ? FootPick : Tops + O.Puff % FragmentBases;
			const int TopPick = bLidTile ? SheetCells - FragmentLids + O.Puff % FragmentLids : bFoot ? FootPick : O.Puff % Tops;
			const int Flags = (bFlatFoot ? 1 : 0) | (bFoot ? 2 : 0) | (bLowLump ? 4 : 0) | (bHeapHere ? 8 : 0) | (bLidTile ? 16 : 0);
			// (a small cloud's rag drawn out along its way: a base by its cloud's, a puff by its own)
			const double LongHere = bFoot ? Cl.Long : Each.Long, LongWay = bFoot ? Cl.LongWay : Each.LongWay;
			Put(Row, First + 0, PX, PY, PZ, Each.R);
			Put(Row, First + 1, CloudX, CloudY, Cl.At.Z + Here.Lift, Here.Squash * Cl.H);
			Put(Row, First + 2, Cl.W, 0.5 * Cl.W * (F.Sky.bDeck && !Cl.bScrap ? 1.25 : 1.0), Here.Squash, Each.Flat);
			Put(Row, First + 3, Here.Show * Each.Alpha, Each.Lit, Each.Trough, Each.Shadowed);
			Put(Row, First + 4, TurnCos[Phase], TurnSin[Phase], PhaseSines(false)[Phase], (O.Puff / SheetCells) % 2 == 1 ? -1.0 : 1.0);
			Put(Row, First + 5, SheetU(TopPick), SheetV(TopPick), SheetU(UpPick), SheetV(UpPick));
			Put(Row, First + 6, LongHere, std::cos(LongWay), std::sin(LongWay), Each.Long);
			Put(Row, First + 7, Each.Lod, Flags, Cl.W < 600.0 ? SmallSight * Cl.W : 0.0, bFoot ? 0.4 * Cl.W / std::max(1.0, Cl.Long) : 0.0);
		}
		bLaid = true;
		++LayCount;
		LastEye = Seen;
		LastDrift = Drift;
		LastNear = Drawn > 0 ? std::max(50.0, Near) : 50.0;
		LastSlack = Clamp(Turn * LastNear, 2.0, Margin);
	}
}
