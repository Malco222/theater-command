#include "Battle/AirSky.h"

#include <algorithm>
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

		/** Smooth value noise, 0 to 1, with lattice points Scale metres apart. */
		double Noise(double X, double Y, double Scale, uint64_t Seed)
		{
			const double U = X / Scale, V = Y / Scale;
			const double FU = std::floor(U), FV = std::floor(V);
			const int64_t IU = static_cast<int64_t>(FU), IV = static_cast<int64_t>(FV);
			const double A = Smooth(U - FU), B = Smooth(V - FV);
			const double N00 = Hash(IU, IV, Seed), N10 = Hash(IU + 1, IV, Seed), N01 = Hash(IU, IV + 1, Seed), N11 = Hash(IU + 1, IV + 1, Seed);
			return (N00 * (1.0 - A) + N10 * A) * (1.0 - B) + (N01 * (1.0 - A) + N11 * A) * B;
		}
	}

	double PlaceDeck(double& Base, double& Top, double Band, std::string& OutWhy)
	{
		OutWhy.clear();
		const double Thick = std::max(300.0, Top - Base);
		if (Band <= 2000.0)
		{
			// (the lowest band, 1,800 m: the deck over the bombers, its base at least 600 m above them)
			const double Least = Band + 600.0;
			if (Base < Least)
			{
				const double Moved = Least - Base;
				Base = Least;
				Top = Base + Thick;
				OutWhy = "above the band";
				return Moved;
			}
			return 0.0;
		}
		// (a deck 1.5 to 2.5 km below the bombers: its top no nearer than 1.5 km, its thickness kept while it fits over
		// 300 m of clear air above the sea, thinned to fit after that)
		const double Most = Band - 1500.0;
		if (Top <= Most)
		{
			return 0.0;
		}
		double NewTop = Most, NewBase = Most - Thick;
		if (NewBase < 300.0)
		{
			NewBase = 300.0;
			NewTop = std::max(NewBase + 300.0, Most);
		}
		const double Moved = NewTop - Top;
		Base = NewBase;
		Top = NewTop;
		OutWhy = "below the band";
		return Moved;
	}

	Field Lay(const Day& D, const Placing& P)
	{
		Field F;
		F.Sky = D;
		double Base = std::max(150.0, D.Base);
		double Top = std::max(D.Top, Base + 300.0);
		if (D.bDeck)
		{
			F.Moved = PlaceDeck(Base, Top, P.Band, F.MovedWhy);
		}
		F.Base = Base;
		F.Top = Top;
		F.DeckTop = D.bDeck ? Top : 0.0;
		const double Thick = Top - Base;
		const double Half = std::max(2000.0, P.Half);

		TheaterFx::Dice Roll(P.Seed * 0x9E3779B97F4A7C15ull + 0x5C1D);
		const uint64_t NoiseSeed = P.Seed * 0xD1B54A32D192ED03ull + 77;
		const Vec Sun = P.Sun.Unit(Vec(-0.35, -0.45, 0.82));
		const Vec SunFlat = Vec(Sun.X, Sun.Y, 0.0).Unit(Vec(0.0, -1.0, 0.0));
		auto GroundAt = [&P](double X, double Y)
		{
			return P.Ground ? std::max(0.0, P.Ground(X, Y)) : 0.0;
		};
		// How much of the day's cloud a place has: thinning out over the square's outer part, so no edge shows, and
		// over the target (cumulus: a third as many; a deck on a strike at ships: a hole over them).
		// (a deck reaches out further, in coarser tiles, so its far edge is lost in the haze and not seen as a bank)
		const double Reach = D.bDeck ? Half * 8.0 / 3.0 : Half;
		auto Want = [&P, &D, Reach](double X, double Y)
		{
			const double Dx = X - P.TargetX, Dy = Y - P.TargetY;
			const double R = std::sqrt(Dx * Dx + Dy * Dy);
			const double Edge = std::max(std::fabs(Dx), std::fabs(Dy));
			double K = Smooth((Reach - Edge) / (0.3 * Reach));
			if (!D.bDeck)
			{
				K *= 0.3 + 0.7 * Smooth((R - 1500.0) / 2500.0);
			}
			else if (P.bShips)
			{
				K *= Smooth((R - 2500.0) / 2000.0);
			}
			return K;
		};
		auto Add = [&F](const Vec& At, double R, double Flat, double Lit, double Alpha)
		{
			Puff& Each = F.Puffs.emplace_back();
			Each.At = At;
			Each.R = R;
			Each.Flat = Flat;
			Each.Lit = Clamp(Lit, 0.04, 1.0);
			Each.Alpha = Alpha;
			Each.Cloud = static_cast<int>(F.Clouds.size()) - 1;
		};

		if (!D.bDeck)
		{
			// Scattered cumulus: about three eighths of the sky where the field is whole. Clouds are thrown in at random,
			// thinned by Want, kept apart enough to read as single clouds (a few touch), until the cover is reached.
			double Room = 0.0; // the square's area as Want weighs it
			const int Grid = 48;
			for (int GX = 0; GX < Grid; ++GX)
				for (int GY = 0; GY < Grid; ++GY)
					Room += Want(P.TargetX - Half + (GX + 0.5) * 2.0 * Half / Grid, P.TargetY - Half + (GY + 0.5) * 2.0 * Half / Grid);
			Room *= (2.0 * Half / Grid) * (2.0 * Half / Grid);
			const double Wanted = 0.3 * Room;
			double Covered = 0.0;
			for (int Try = 0; Try < 20000 && Covered < Wanted; ++Try)
			{
				const double X = P.TargetX + Roll.U(-Half, Half), Y = P.TargetY + Roll.U(-Half, Half);
				const double W = 650.0 * std::pow(3.2, std::pow(Roll.U(), 1.4));
				if (Roll.U() > Want(X, Y))
				{
					continue;
				}
				bool bClose = false;
				for (const Cloud& O : F.Clouds)
				{
					const double Dx = O.At.X - X, Dy = O.At.Y - Y, Apart = 0.55 * (O.W + W) * 0.5;
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
				// Its foot: the day's base, or 300 m over the highest ground under it.
				double Foot = Base;
				const double Feel[5][2] = { { 0.0, 0.0 }, { 0.45, 0.0 }, { -0.45, 0.0 }, { 0.0, 0.45 }, { 0.0, -0.45 } };
				for (const auto& K : Feel)
				{
					Foot = std::max(Foot, GroundAt(X + K[0] * W, Y + K[1] * W) + 300.0);
				}
				// (layers close enough that the puffs of one overlap the next: no gap between a cloud's base and its top)
				const double Rb = Clamp(W * 0.25, 120.0, 480.0);
				const int Layers = static_cast<int>(Clamp(std::ceil(std::max(0.0, H - 0.5 * Rb - 0.55 * Rb) / (0.5 * Rb)) + 1.0, 2.0, 6.0));
				// (its puffs, bottom to top: needs at most 10 a layer, 3 turrets and its veil, so a cloud never runs past the room)
				if (static_cast<int>(F.Puffs.size()) + Layers * 10 + 4 > P.MostPuffs)
				{
					++F.Short;
					continue;
				}
				Cloud& C = F.Clouds.emplace_back();
				C.At = Vec(X, Y, Foot);
				C.W = W;
				C.H = H;
				C.First = static_cast<int>(F.Puffs.size());
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
						const double PZ = Zk + (K > 0 ? Roll.U(-0.2, 0.2) * Rk : 0.0);
						const double Side = ((PX - X) * SunFlat.X + (PY - Y) * SunFlat.Y) / std::max(1.0, W * 0.5);
						const double Up = Clamp((PZ - Foot) / std::max(1.0, H), 0.0, 1.0);
						double Lit = 0.2 + 0.65 * Up + 0.25 * Side;
						if (K == 0)
						{
							Lit = std::min(Lit, 0.32);
						}
						Add(Vec(PX, PY, PZ), Rk * Roll.U(0.7, 1.2), Flat, Lit, K == 0 ? 0.8 : 0.68);
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
					Add(Vec(PX, PY, Foot + H - 0.75 * Rt), Rt, 0.9, 0.9 + 0.1 * Side, 0.62);
				}
				// Its veil: one broad, faint puff round the whole, so its lumps read as one body and not as separate balls
				// (the art director's sheet: never a few round puffs that read as cotton).
				Add(Vec(X, Y, Foot + 0.4 * H), 0.5 * W, Clamp(H / W, 0.4, 0.9), 0.55, 0.18);
				C.Count = static_cast<int>(F.Puffs.size()) - C.First;
				Covered += Pi * (W * 0.5) * (W * 0.5) * 0.85;
			}
		}
		else
		{
			// A deck: tiles on a jittered grid, most of them kept (a few holes where a broad noise is low), each a broad
			// flattened puff for the lumpy lit top, now and then a lump above it, and on every other tile a grey flat
			// puff for its base, seen from under it.
			// (three rings of tiles: 800 m across out to two thirds of the field's half, then 1,600 m, then 3,200 m out to
			// the deck's reach)
			const double Rings[3][3] = { { 800.0, 0.0, Half * 2.0 / 3.0 }, { 1600.0, Half * 2.0 / 3.0, Half * 4.0 / 3.0 }, { 3200.0, Half * 4.0 / 3.0, Reach } };
			for (const auto& Ring : Rings)
			{
				const double S = Ring[0], Inner = Ring[1], Outer = Ring[2];
				const int Across = static_cast<int>(std::ceil(2.0 * Outer / S));
				for (int GX = 0; GX < Across; ++GX)
				{
					for (int GY = 0; GY < Across; ++GY)
					{
						const double CX = -Outer + (GX + 0.5) * S, CY = -Outer + (GY + 0.5) * S;
						if (std::max(std::fabs(CX), std::fabs(CY)) < Inner)
						{
							continue;
						}
						const double X = P.TargetX + CX + Roll.U(-0.35, 0.35) * S;
						const double Y = P.TargetY + CY + Roll.U(-0.35, 0.35) * S;
						const double N = Noise(X, Y, 4200.0, NoiseSeed) * 0.65 + Noise(X, Y, 1500.0, NoiseSeed + 1) * 0.35;
						const double Keep = Roll.U();
						if (N < 0.22 || Keep > Want(X, Y))
						{
							continue;
						}
						if (static_cast<int>(F.Puffs.size()) + 3 > P.MostPuffs)
						{
							++F.Short;
							continue;
						}
						const double Foot = std::max(Base, GroundAt(X, Y) + 300.0);
						const double R = S * Roll.U(0.62, 0.8);
						Cloud& C = F.Clouds.emplace_back();
						C.At = Vec(X, Y, Foot);
						C.W = 2.0 * R * 0.9;
						C.H = Thick;
						C.First = static_cast<int>(F.Puffs.size());
						const double TopZ = Foot + Thick - std::min(R * 0.3, Thick * 0.3) + (N - 0.5) * 160.0;
						Add(Vec(X, Y, TopZ), R, 0.6, 0.78 + 0.22 * Clamp((N - 0.22) / 0.78, 0.0, 1.0), 0.85);
						if (Roll.U() < 0.25)
						{
							Add(Vec(X + Roll.U(-0.3, 0.3) * R, Y + Roll.U(-0.3, 0.3) * R, TopZ + 0.45 * std::min(R, 800.0)), 0.55 * std::min(R, 800.0), 0.8, 0.95, 0.7);
						}
						if ((GX + GY) % 2 == 0)
						{
							Add(Vec(X, Y, Foot + 0.35 * std::min(R, Thick)), R * 1.1, 0.5, 0.12, 0.8);
						}
						C.Count = static_cast<int>(F.Puffs.size()) - C.First;
					}
				}
			}
		}

		// The cover where the field is whole: past the target's clearing (4 km for cumulus, 4.5 km for a deck's hole),
		// inside the thinning edge.
		int In = 0, Under = 0;
		const int Grid = 60;
		for (int GX = 0; GX < Grid; ++GX)
		{
			for (int GY = 0; GY < Grid; ++GY)
			{
				const double X = P.TargetX - Half + (GX + 0.5) * 2.0 * Half / Grid, Y = P.TargetY - Half + (GY + 0.5) * 2.0 * Half / Grid;
				const double Dx = X - P.TargetX, Dy = Y - P.TargetY;
				if (std::max(std::fabs(Dx), std::fabs(Dy)) > 0.7 * Half || Dx * Dx + Dy * Dy < 4500.0 * 4500.0)
				{
					continue;
				}
				++In;
				for (const Cloud& C : F.Clouds)
				{
					const double Ex = C.At.X - X, Ey = C.At.Y - Y, R = C.W * 0.5 * 0.9;
					if (Ex * Ex + Ey * Ey <= R * R)
					{
						++Under;
						break;
					}
				}
			}
		}
		F.Cover = In ? static_cast<double>(Under) / In : 0.0;
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
		Out.reserve(static_cast<size_t>(Count) * SideCount * 18);
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
		const double Moved = ((Eye - Drift) - LastEye).Length();
		return Moved > std::max(1.0, LastNear * 0.0045);
	}

	double Painter::Paint(const Field& F, const Vec& Eye, const Vec& Drift, const Vec& Sun, const Rgb& Light, std::vector<Vec>& OutPoints, std::vector<uint8_t>& OutRgba)
	{
		const int N = static_cast<int>(F.Puffs.size());
		const int Per = PerPuff();
		OutPoints.resize(static_cast<size_t>(N) * Per);
		OutRgba.resize(static_cast<size_t>(N) * Per * 4);
		Sorted.resize(static_cast<size_t>(N));
		const Vec Seen = Eye - Drift; // (the eye in the field's own frame)
		double Near = 1e18;
		for (int I = 0; I < N; ++I)
		{
			const Vec D = F.Puffs[static_cast<size_t>(I)].At - Seen;
			const double Deep = D.Dot(D);
			Sorted[static_cast<size_t>(I)] = { I, Deep };
			Near = std::min(Near, std::sqrt(Deep) - F.Puffs[static_cast<size_t>(I)].R);
		}
		std::sort(Sorted.begin(), Sorted.end(), [](const Order& A, const Order& B) { return A.Deep > B.Deep; });
		const Vec SunWay = Sun.Unit(Vec(-0.35, -0.45, 0.82));
		auto Byte = [](double V) { return static_cast<uint8_t>(Clamp(V, 0.0, 1.0) * 255.0 + 0.5); };
		for (int Slot = 0; Slot < N; ++Slot)
		{
			const Puff& Each = F.Puffs[static_cast<size_t>(Sorted[static_cast<size_t>(Slot)].Index)];
			const Vec& C = Each.At;
			Vec Fwd = C - Seen;
			const double Dist = Fwd.Length();
			Fwd = Fwd.Unit(Vec(0.0, 0.0, -1.0));
			Vec Right = Fwd.Cross(Vec(0.0, 0.0, 1.0));
			Right = Right.Length() < 1e-6 ? Vec(1.0, 0.0, 0.0) : Right.Unit();
			Vec Up = Right.Cross(Fwd);
			Up = Up - Vec(0.0, 0.0, Up.Z * (1.0 - Each.Flat)); // (a flat base stays flat seen from the side)
			// (no disc across the lens as the eye passes through a cloud: it fades from 1.2 radii to 0.6)
			const double Alpha = Each.Alpha * Clamp((Dist - 0.6 * Each.R) / (0.6 * Each.R), 0.0, 1.0);
			// (seen from under it, a cloud is its grey base; seen from above with the sun up, its tops all take the sun)
			const double Below = Clamp((C.Z - Seen.Z) / (2.0 * Each.R), 0.0, 1.0);
			const double Above = Clamp((Seen.Z - C.Z) / (3.0 * Each.R), 0.0, 1.0) * Clamp(SunWay.Z * 1.5, 0.0, 1.0);
			double Lit = Each.Lit * (1.0 - 0.6 * Below);
			Lit += (1.0 - Lit) * 0.45 * Above;
			Rgb Body = Lit < 0.35 ? BaseSide.Mix(ShadeSide, Lit / 0.35) : ShadeSide.Mix(SunSide, (Lit - 0.35) / 0.65);
			Body = Rgb(Body.R * Light.R, Body.G * Light.G, Body.B * Light.B);
			const size_t First = static_cast<size_t>(Slot) * Per;
			auto Put = [&OutPoints, &OutRgba, &Byte](size_t At, const Vec& Where, const Rgb& Col, double A)
			{
				OutPoints[At] = Where;
				OutRgba[At * 4 + 0] = Byte(Col.R);
				OutRgba[At * 4 + 1] = Byte(Col.G);
				OutRgba[At * 4 + 2] = Byte(Col.B);
				OutRgba[At * 4 + 3] = Byte(A);
			};
			Put(First, C, Body, Alpha);
			// (a puff's own lumpiness: its rim pushed in and out a little, differently for each puff)
			const double Phase = static_cast<double>(Sorted[static_cast<size_t>(Slot)].Index % 97) * 0.61;
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
		LastNear = std::max(50.0, Near);
		return Near;
	}
}
