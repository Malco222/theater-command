#include "Battle/SeaFace.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr double Pi = 3.14159265358979323846;
	constexpr double Far = 1e20;

	double Smooth(double From, double To, double X)
	{
		const double T = std::clamp((X - From) / (To - From), 0.0, 1.0);
		return T * T * (3.0 - 2.0 * T);
	}

	uint8_t Byte(double Value)
	{
		return static_cast<uint8_t>(std::lround(std::clamp(Value, 0.0, 1.0) * 255.0));
	}

	/** A small steady run of numbers from 0 to 1 (xorshift), the same on every machine. */
	struct Dice
	{
		uint64_t State;
		double Next()
		{
			State ^= State >> 12;
			State ^= State << 25;
			State ^= State >> 27;
			return static_cast<double>((State * 2685821657736338717ull) >> 11) / 9007199254740992.0;
		}
	};

	/**
	 * Squared distances along one line to its nearest feature (Felzenszwalb and Huttenlocher's lower envelope of
	 * parabolas): In holds 0 at a feature and Far elsewhere, or a column's distances already found.
	 */
	void Line(const std::vector<double>& In, std::vector<double>& Out, int N, std::vector<int>& V, std::vector<double>& Z)
	{
		int K = 0;
		V[0] = 0;
		Z[0] = -Far;
		Z[1] = Far;
		for (int Q = 1; Q < N; ++Q)
		{
			// (S never reaches Z[0]: the In values are at most Far, so it stays above -Far / 2)
			double S = ((In[Q] + double(Q) * Q) - (In[V[K]] + double(V[K]) * V[K])) / (2.0 * Q - 2.0 * V[K]);
			while (S <= Z[K])
			{
				--K;
				S = ((In[Q] + double(Q) * Q) - (In[V[K]] + double(V[K]) * V[K])) / (2.0 * Q - 2.0 * V[K]);
			}
			++K;
			V[K] = Q;
			Z[K] = S;
			Z[K + 1] = Far;
		}
		K = 0;
		for (int Q = 0; Q < N; ++Q)
		{
			while (Z[K + 1] < Q)
			{
				++K;
			}
			Out[Q] = (double(Q) - V[K]) * (double(Q) - V[K]) + In[V[K]];
		}
	}

	/** Each pixel's squared distance, in pixels, to the nearest pixel where Feature is true (Far with none). */
	std::vector<double> Distances(const std::vector<uint8_t>& Feature, bool bWant, int N)
	{
		std::vector<double> Grid(static_cast<size_t>(N) * N);
		for (size_t K = 0; K < Grid.size(); ++K)
		{
			Grid[K] = (Feature[K] != 0) == bWant ? 0.0 : Far;
		}
		std::vector<double> In(N), Out(N), Z(N + 1);
		std::vector<int> V(N);
		for (int I = 0; I < N; ++I)
		{
			for (int J = 0; J < N; ++J)
			{
				In[J] = Grid[static_cast<size_t>(J) * N + I];
			}
			Line(In, Out, N, V, Z);
			for (int J = 0; J < N; ++J)
			{
				Grid[static_cast<size_t>(J) * N + I] = Out[J];
			}
		}
		for (int J = 0; J < N; ++J)
		{
			for (int I = 0; I < N; ++I)
			{
				In[I] = Grid[static_cast<size_t>(J) * N + I];
			}
			Line(In, Out, N, V, Z);
			for (int I = 0; I < N; ++I)
			{
				Grid[static_cast<size_t>(J) * N + I] = Out[I];
			}
		}
		return Grid;
	}
}

SeaFace::Shore SeaFace::MakeShore(const std::vector<Part>& Parts, double WindFrom, int Size)
{
	Shore Made;
	double MinX = Far, MinY = Far, MaxX = -Far, MaxY = -Far;
	for (const Part& P : Parts)
	{
		for (size_t K = 0; K < P.X.size() && K < P.Y.size(); ++K)
		{
			MinX = std::min(MinX, P.X[K]);
			MaxX = std::max(MaxX, P.X[K]);
			MinY = std::min(MinY, P.Y[K]);
			MaxY = std::max(MaxY, P.Y[K]);
		}
	}
	if (MinX > MaxX || Size < 8)
	{
		return Made;
	}
	// The square: the parts' bounds and 10 km of sea round them.
	Size = Size / 4 * 4;
	const double Span = std::max(MaxX - MinX, MaxY - MinY) + 20000.0;
	Made.Size = Size;
	Made.Span = Span;
	Made.West = (MinX + MaxX) * 0.5 - Span * 0.5;
	Made.South = (MinY + MaxY) * 0.5 - Span * 0.5;

	// Where the sea breaks, at twice the picture's size (each pixel's middle inside a triangle or not).
	const int Fine = Size * 2;
	const double Step = Span / Fine;
	std::vector<uint8_t> Hard(static_cast<size_t>(Fine) * Fine, 0);
	for (const Part& P : Parts)
	{
		for (size_t T = 0; T + 2 < P.Triangles.size(); T += 3)
		{
			const int A = P.Triangles[T], B = P.Triangles[T + 1], C = P.Triangles[T + 2];
			const int Most = static_cast<int>(std::min(P.X.size(), P.Y.size()));
			if (A < 0 || B < 0 || C < 0 || A >= Most || B >= Most || C >= Most)
			{
				continue;
			}
			const double AX = (P.X[A] - Made.West) / Step, AY = (P.Y[A] - Made.South) / Step;
			const double BX = (P.X[B] - Made.West) / Step, BY = (P.Y[B] - Made.South) / Step;
			const double CX = (P.X[C] - Made.West) / Step, CY = (P.Y[C] - Made.South) / Step;
			const double Area = (BX - AX) * (CY - AY) - (BY - AY) * (CX - AX);
			if (std::abs(Area) < 1e-12)
			{
				continue;
			}
			const double Sign = Area > 0.0 ? 1.0 : -1.0;
			const int I0 = std::max(0, static_cast<int>(std::ceil(std::min({ AX, BX, CX }) - 0.5)));
			const int I1 = std::min(Fine - 1, static_cast<int>(std::floor(std::max({ AX, BX, CX }) - 0.5)));
			const int J0 = std::max(0, static_cast<int>(std::ceil(std::min({ AY, BY, CY }) - 0.5)));
			const int J1 = std::min(Fine - 1, static_cast<int>(std::floor(std::max({ AY, BY, CY }) - 0.5)));
			for (int J = J0; J <= J1; ++J)
			{
				const double PY = J + 0.5;
				for (int I = I0; I <= I1; ++I)
				{
					const double PX = I + 0.5;
					const double E0 = ((BX - AX) * (PY - AY) - (BY - AY) * (PX - AX)) * Sign;
					const double E1 = ((CX - BX) * (PY - BY) - (CY - BY) * (PX - BX)) * Sign;
					const double E2 = ((AX - CX) * (PY - CY) - (AY - CY) * (PX - CX)) * Sign;
					if (E0 >= 0.0 && E1 >= 0.0 && E2 >= 0.0)
					{
						Hard[static_cast<size_t>(J) * Fine + I] = 1;
					}
				}
			}
		}
	}

	// How far each point lies from the edge, in metres (out to sea positive), averaged four fine pixels to one.
	const std::vector<double> ToHard = Distances(Hard, true, Fine);
	const std::vector<double> ToWater = Distances(Hard, false, Fine);
	auto Edge = [&](size_t K)
	{
		return Hard[K] ? -(std::sqrt(ToWater[K]) - 0.5) * Step : (std::sqrt(ToHard[K]) - 0.5) * Step;
	};
	std::vector<double> Off(static_cast<size_t>(Size) * Size);
	for (int J = 0; J < Size; ++J)
	{
		for (int I = 0; I < Size; ++I)
		{
			const size_t K = static_cast<size_t>(J * 2) * Fine + I * 2;
			Off[static_cast<size_t>(J) * Size + I] = 0.25 * (Edge(K) + Edge(K + 1) + Edge(K + Fine) + Edge(K + Fine + 1));
		}
	}

	// The open water about each point, by rays, on a coarser grid: a quarter of the picture's size, marched over a mask
	// at half its size (a cell hard where any fine pixel in it is, so a narrow reef is not stepped over).
	const int Mask = Size / 2;
	const double MaskStep = Span / Mask;
	std::vector<uint8_t> Coarse(static_cast<size_t>(Mask) * Mask, 0);
	for (int J = 0; J < Fine; ++J)
	{
		for (int I = 0; I < Fine; ++I)
		{
			if (Hard[static_cast<size_t>(J) * Fine + I])
			{
				Coarse[static_cast<size_t>(J / 4) * Mask + I / 4] = 1;
			}
		}
	}
	auto HardAt = [&](double X, double Y, bool& bOut) -> bool
	{
		const int I = static_cast<int>(std::floor(X)), J = static_cast<int>(std::floor(Y));
		bOut = I < 0 || J < 0 || I >= Mask || J >= Mask;
		return !bOut && Coarse[static_cast<size_t>(J) * Mask + I] != 0;
	};
	// How far a ray from (X, Y) goes on the bearing before it meets land or reef (metres, at most Reach); from a point on
	// them, how far it goes from where it first comes out over water (Reach if it never does).
	auto Ray = [&](double X, double Y, double Bearing, double Reach, bool bFromHard) -> double
	{
		// (in steps of a mask cell, or of 120 m where the cells are smaller: a smaller stage's picture is finer)
		const double Stride = std::max(1.0, 120.0 / MaskStep);
		const double DX = std::sin(Bearing * Pi / 180.0) * Stride, DY = std::cos(Bearing * Pi / 180.0) * Stride;
		const int Steps = static_cast<int>(Reach / (MaskStep * Stride));
		bool bOut = false;
		int Start = 0;
		if (bFromHard)
		{
			while (Start < Steps && HardAt(X + DX * Start, Y + DY * Start, bOut))
			{
				++Start;
			}
			if (Start >= Steps)
			{
				return 0.0;
			}
		}
		for (int K = Start + 1; K <= Start + Steps; ++K)
		{
			if (HardAt(X + DX * K, Y + DY * K, bOut))
			{
				return (K - Start) * MaskStep * Stride;
			}
			if (bOut)
			{
				return Reach;
			}
		}
		return Reach;
	};
	const int Cells = Size / 4;
	std::vector<double> Shut(static_cast<size_t>(Cells) * Cells), Fetch(Shut.size()), Open(Shut.size());
	std::vector<uint8_t> CellHard(Shut.size());
	const double Around = 8000.0, Upwind = 16000.0;
	// (the wind's fetch: the open water it has crossed, upwind and up to 30 degrees either side, the nearer the wind's
	// own line the more it counts)
	static const double Spread[7][2] = { { -30.0, 0.25 }, { -20.0, 0.5 }, { -10.0, 0.8 }, { 0.0, 1.0 }, { 10.0, 0.8 }, { 20.0, 0.5 }, { 30.0, 0.25 } };
	for (int J = 0; J < Cells; ++J)
	{
		for (int I = 0; I < Cells; ++I)
		{
			const size_t K = static_cast<size_t>(J) * Cells + I;
			const double X = I * 2.0 + 1.0, Y = J * 2.0 + 1.0; // (the cell's middle on the mask)
			bool bOut = false;
			const bool bHard = HardAt(X, Y, bOut);
			CellHard[K] = bHard ? 1 : 0;
			// (no ray need be cast from far out at sea, where none can meet land, nor from deep inland or across a
			// lagoon, where only how open the shore lies is wanted, and only at its edge)
			const size_t Middle = static_cast<size_t>(J * 8 + 4) * Fine + I * 8 + 4;
			const double Clear = std::sqrt(ToHard[Middle]) * Step, Inland = std::sqrt(ToWater[Middle]) * Step;
			if (bHard && Inland > 1000.0)
			{
				Shut[K] = 1.0;
				continue;
			}
			double Closed = 0.0;
			if (bHard || Clear < Around + MaskStep * 2.0)
			{
				int Met = 0;
				for (int D = 0; D < 16; ++D)
				{
					Met += Ray(X, Y, D * 22.5, Around, bHard) < Around ? 1 : 0;
				}
				Closed = Met / 16.0;
			}
			Shut[K] = Smooth(0.62, 0.88, Closed);
			Open[K] = std::clamp(1.6 * (1.0 - Closed), 0.0, 1.0);
			if (!bHard)
			{
				double Run = Upwind;
				if (Clear < Upwind + MaskStep * 2.0)
				{
					double Sum = 0.0, Weights = 0.0;
					for (const auto& Way : Spread)
					{
						Sum += Way[1] * Ray(X, Y, WindFrom + Way[0], Upwind, false);
						Weights += Way[1];
					}
					Run = Sum / Weights;
				}
				Fetch[K] = Smooth(3000.0, 14000.0, Run) * (1.0 - Shut[K]);
			}
		}
	}

	// Into the picture: water from the water cells about it, land and reef from theirs (so no shore's calm or its
	// openness spills a cell's width across the edge); on land and reef the sea is shut in and raises no whitecaps.
	Made.Pixels.assign(static_cast<size_t>(Size) * Size * 4, 0);
	for (int J = 0; J < Size; ++J)
	{
		for (int I = 0; I < Size; ++I)
		{
			const size_t P = static_cast<size_t>(J) * Size + I;
			const double Out = Off[P];
			const bool bHard = Out < 0.0;
			const double GX = std::clamp((I + 0.5) / 4.0 - 0.5, 0.0, Cells - 1.0), GY = std::clamp((J + 0.5) / 4.0 - 0.5, 0.0, Cells - 1.0);
			const int X0 = std::min(static_cast<int>(GX), Cells - 2), Y0 = std::min(static_cast<int>(GY), Cells - 2);
			const double FX = GX - X0, FY = GY - Y0;
			double Weight = 0.0, SumShut = 0.0, SumFetch = 0.0, SumOpen = 0.0;
			for (int Q = 0; Q < 4; ++Q)
			{
				const int CX = std::min(X0 + (Q & 1), Cells - 1), CY = std::min(Y0 + (Q >> 1), Cells - 1);
				const size_t C = static_cast<size_t>(CY) * Cells + CX;
				if ((CellHard[C] != 0) != bHard)
				{
					continue;
				}
				const double W = ((Q & 1) ? FX : 1.0 - FX) * ((Q >> 1) ? FY : 1.0 - FY) + 1e-6;
				Weight += W;
				SumShut += W * Shut[C];
				SumFetch += W * Fetch[C];
				SumOpen += W * Open[C];
			}
			// (none of its kind among the four about it, as along a shore where the cells' middles fall ashore: the
			// nearest of its kind within three cells, by how near they are)
			for (int Ring = 1; Ring <= 3 && Weight <= 0.0; ++Ring)
			{
				for (int CY = Y0 - Ring; CY <= Y0 + 1 + Ring; ++CY)
				{
					for (int CX = X0 - Ring; CX <= X0 + 1 + Ring; ++CX)
					{
						if (CX < 0 || CY < 0 || CX >= Cells || CY >= Cells)
						{
							continue;
						}
						const size_t C = static_cast<size_t>(CY) * Cells + CX;
						if ((CellHard[C] != 0) != bHard)
						{
							continue;
						}
						const double W = 1.0 / (0.25 + (CX - GX) * (CX - GX) + (CY - GY) * (CY - GY));
						Weight += W;
						SumShut += W * Shut[C];
						SumFetch += W * Fetch[C];
						SumOpen += W * Open[C];
					}
				}
			}
			double ShutHere = 1.0, FetchHere = 0.0, OpenHere = 0.0;
			if (Weight > 0.0)
			{
				ShutHere = SumShut / Weight;
				FetchHere = SumFetch / Weight;
				OpenHere = SumOpen / Weight;
			}
			if (bHard)
			{
				ShutHere = 1.0;
				FetchHere = 0.0;
			}
			uint8_t* Pixel = &Made.Pixels[P * 4];
			Pixel[0] = Byte(0.5 + Out / (2.0 * EdgeReach));
			Pixel[1] = Byte(FetchHere);
			Pixel[2] = Byte(OpenHere);
			Pixel[3] = Byte(ShutHere);
		}
	}
	return Made;
}

std::vector<uint8_t> SeaFace::MakeFoam(int Size, double TileMetres, int Count)
{
	Size = std::max(Size, 16);
	const size_t Pixels = static_cast<size_t>(Size) * Size;
	std::vector<double> Cover(Pixels, 0.0), When(Pixels, 0.0), Along(Pixels, 0.0);
	const double PerMetre = Size / std::max(TileMetres, 1.0);
	const double Feather = std::max(0.5, 1.5 / PerMetre); // metres, and a pixel and a half at the least
	Dice Roll{ 0x5EAF0A3DC0FFEEull };
	// The rows: the chop's crests, across the wind about 20 m apart, each stronger in some stretches than others (a
	// whole number of waves across the picture, so it repeats).
	struct FRow
	{
		double U;
		int Waves[2];
		double Phase[2];
	};
	const int Rows = std::max(1, static_cast<int>(std::lround(TileMetres / 20.0)));
	std::vector<FRow> Crests;
	for (int R = 0; R < Rows; ++R)
	{
		FRow Row;
		Row.U = (R + 0.5 + (Roll.Next() - 0.5) * 0.4) * Size / Rows;
		for (int W = 0; W < 2; ++W)
		{
			Row.Waves[W] = 1 + static_cast<int>(Roll.Next() * 3.0);
			Row.Phase[W] = Roll.Next() * 2.0 * Pi;
		}
		Crests.push_back(Row);
	}
	auto Strength = [&](const FRow& Row, double V)
	{
		const double T = V / Size * 2.0 * Pi;
		return 0.5 + 0.5 * std::sin(Row.Waves[0] * T + Row.Phase[0]) * std::cos(Row.Waves[1] * T + Row.Phase[1]);
	};
	// Where a whitecap (and two pixels round it) already lies: two that overlapped would meet along a seam where a
	// read between pixels mixes their two times and lights a thin line.
	std::vector<uint8_t> Taken(Pixels, 0);
	const int Margin = 2;
	for (int K = 0; K < Count; ++K)
	{
		// How big: the head across the wind, most under 2 m, the largest 3 to 5 m; 2 to 4 times as long as it is deep;
		// the tail 2 to 4 times the head's length, ragged, a little wider as it ages.
		const double Across = 0.8 + 3.9 * std::pow(Roll.Next(), 2.5);
		const double Deep = Across / (2.0 + 2.0 * Roll.Next());
		const double Tail = Across * (2.0 + 2.0 * Roll.Next());
		const double Long = Deep + Tail;
		const double Phase = Roll.Next();
		const double Rag[4] = { Roll.Next() * 2.0 * Pi, Roll.Next() * 2.0 * Pi, 1.6 + Roll.Next() * 1.4, 0.9 + Roll.Next() * 0.6 };
		const double Half = (Across * 0.5 * 1.35 + Feather) * PerMetre + 2.0;
		// Its foam at a pixel, with its head's front at U, V: how much, and how far back from the front (metres).
		auto Foam = [&](double U, double V, int PU, int PV, double& A)
		{
			A = (U - (PU + 0.5)) / PerMetre;
			if (A < -Feather || A > Long + Feather)
			{
				return 0.0;
			}
			const double C = ((PV + 0.5) - V) / PerMetre;
			// The head: an ellipse, Across by Deep. (Each edge feathered over Feather metres, so a whitecap drawn close
			// has no hard rim.)
			const double HA = (A - Deep * 0.5) / (Deep * 0.5), HC = C / (Across * 0.5);
			const double Head = std::clamp((1.0 - std::sqrt(HA * HA + HC * HC)) * Deep * 0.5 / Feather + 0.5, 0.0, 1.0);
			// The tail: from the head's middle back, its edges ragged, its end rounded off.
			double Back = 0.0;
			if (A >= Deep * 0.5)
			{
				const double Share = (A - Deep * 0.5) / std::max(Long - Deep * 0.5, 0.01);
				const double Ragged = 1.0 + 0.16 * std::sin(A * 2.0 * Pi / Rag[2] + Rag[0]) + 0.09 * std::sin(A * 2.0 * Pi / Rag[3] + Rag[1]);
				const double Width = Across * 0.5 * (0.85 + 0.35 * Share) * Ragged;
				const double End = std::clamp((Long - A) / (0.25 * Tail), 0.0, 1.0);
				Back = std::clamp((Width * std::sqrt(End) - std::abs(C)) / Feather + 0.5, 0.0, 1.0);
			}
			return std::max(Head, Back);
		};
		auto Wrapped = [Size](int PU, int PV)
		{
			return static_cast<size_t>(((PV % Size) + Size) % Size) * Size + ((PU % Size) + Size) % Size;
		};
		// Where: most on a crest, in its stronger stretches; a few anywhere; never on another (tried again elsewhere,
		// and left out if the picture is too full for it).
		double U = 0.0, V = 0.0;
		bool bRoom = false;
		for (int Place = 0; Place < 40 && !bRoom; ++Place)
		{
			if (Roll.Next() < 0.8)
			{
				const FRow& Row = Crests[std::min(Rows - 1, static_cast<int>(Roll.Next() * Rows))];
				for (int Try = 0; Try < 16; ++Try)
				{
					V = Roll.Next() * Size;
					if (Roll.Next() < Strength(Row, V) * Strength(Row, V))
					{
						break;
					}
				}
				U = Row.U + (Roll.Next() + Roll.Next() - 1.0) * 1.5 * PerMetre;
			}
			else
			{
				U = Roll.Next() * Size;
				V = Roll.Next() * Size;
			}
			bRoom = true;
			const int UFrom = static_cast<int>(std::floor(U - (Long + Feather) * PerMetre)) - 2, UTo = static_cast<int>(std::ceil(U + Feather * PerMetre)) + 2;
			const int VFrom = static_cast<int>(std::floor(V - Half)), VTo = static_cast<int>(std::ceil(V + Half));
			for (int PU = UFrom; PU <= UTo && bRoom; ++PU)
			{
				for (int PV = VFrom; PV <= VTo; ++PV)
				{
					double A = 0.0;
					if (Foam(U, V, PU, PV, A) > 0.0 && Taken[Wrapped(PU, PV)])
					{
						bRoom = false;
						break;
					}
				}
			}
		}
		if (!bRoom)
		{
			continue;
		}
		const int UFrom = static_cast<int>(std::floor(U - (Long + Feather) * PerMetre)) - 2, UTo = static_cast<int>(std::ceil(U + Feather * PerMetre)) + 2;
		const int VFrom = static_cast<int>(std::floor(V - Half)), VTo = static_cast<int>(std::ceil(V + Half));
		for (int PU = UFrom; PU <= UTo; ++PU)
		{
			for (int PV = VFrom; PV <= VTo; ++PV)
			{
				double A = 0.0;
				const double Here = Foam(U, V, PU, PV, A);
				if (Here <= 0.0)
				{
					continue;
				}
				const size_t P = Wrapped(PU, PV);
				Cover[P] = Here;
				When[P] = Phase;
				Along[P] = std::clamp(1.0 - A / Long, 0.0, 1.0);
				for (int DU = -Margin; DU <= Margin; ++DU)
				{
					for (int DV = -Margin; DV <= Margin; ++DV)
					{
						Taken[Wrapped(PU + DU, PV + DV)] = 1;
					}
				}
			}
		}
	}
	// Round each whitecap, where it has no foam, its own when and along out to a metre or so, so that a picture read
	// between pixels (or from a smaller copy of itself, far off) never mixes a whitecap's time with the empty sea's
	// and lights a rim round it.
	{
		const int Reach = std::max(2, static_cast<int>(std::lround(PerMetre)));
		std::vector<double> WhenOut = When, AlongOut = Along;
		for (int J = 0; J < Size; ++J)
		{
			for (int I = 0; I < Size; ++I)
			{
				const size_t P = static_cast<size_t>(J) * Size + I;
				if (Cover[P] > 0.0)
				{
					continue;
				}
				double Best = 0.0;
				for (int DJ = -Reach; DJ <= Reach; ++DJ)
				{
					for (int DI = -Reach; DI <= Reach; ++DI)
					{
						const size_t Q = static_cast<size_t>((J + DJ + Size) % Size) * Size + (I + DI + Size) % Size;
						// (the nearest whitecap's, whatever its foam there: a far one's would meet the near one's foam)
						const double Weight = Cover[Q] > 0.0 ? 1000.0 - (DI * DI + DJ * DJ) + Cover[Q] : 0.0;
						if (Weight > Best)
						{
							Best = Weight;
							WhenOut[P] = When[Q];
							AlongOut[P] = Along[Q];
						}
					}
				}
			}
		}
		When.swap(WhenOut);
		Along.swap(AlongOut);
	}
	// The mottle: a fine value noise at two sizes, about a metre and half a metre, repeating with the picture.
	auto Noise = [&](int Cells, uint64_t Seed)
	{
		Dice Grain{ Seed };
		std::vector<double> Lattice(static_cast<size_t>(Cells) * Cells);
		for (double& L : Lattice)
		{
			L = Grain.Next();
		}
		std::vector<double> Field(Pixels);
		for (int J = 0; J < Size; ++J)
		{
			for (int I = 0; I < Size; ++I)
			{
				const double X = (I + 0.5) * Cells / Size, Y = (J + 0.5) * Cells / Size;
				const int X0 = static_cast<int>(std::floor(X)) % Cells, Y0 = static_cast<int>(std::floor(Y)) % Cells;
				const int X1 = (X0 + 1) % Cells, Y1 = (Y0 + 1) % Cells;
				double FX = X - std::floor(X), FY = Y - std::floor(Y);
				FX = FX * FX * (3.0 - 2.0 * FX);
				FY = FY * FY * (3.0 - 2.0 * FY);
				const double Top = Lattice[static_cast<size_t>(Y0) * Cells + X0] * (1.0 - FX) + Lattice[static_cast<size_t>(Y0) * Cells + X1] * FX;
				const double Bottom = Lattice[static_cast<size_t>(Y1) * Cells + X0] * (1.0 - FX) + Lattice[static_cast<size_t>(Y1) * Cells + X1] * FX;
				Field[static_cast<size_t>(J) * Size + I] = Top * (1.0 - FY) + Bottom * FY;
			}
		}
		return Field;
	};
	const int Coarse = std::max(2, static_cast<int>(std::lround(TileMetres / 1.2))), FineCells = std::max(2, static_cast<int>(std::lround(TileMetres / 0.55)));
	const std::vector<double> Big = Noise(std::min(Coarse, Size), 71), Small = Noise(std::min(FineCells, Size), 72);
	std::vector<uint8_t> Out(Pixels * 4);
	for (size_t P = 0; P < Pixels; ++P)
	{
		Out[P * 4 + 0] = Byte(Cover[P]);
		Out[P * 4 + 1] = Byte(When[P]);
		Out[P * 4 + 2] = Byte(Along[P]);
		Out[P * 4 + 3] = Byte((0.6 * Big[P] + 0.4 * Small[P] - 0.5) * 1.8 + 0.5);
	}
	return Out;
}

namespace
{
	/** A fine value noise repeating Cells times across a picture Size pixels a side (smoothstepped between its lattice). */
	std::vector<double> StreakNoise(int Size, int Cells, uint64_t Seed)
	{
		Dice Grain{ Seed };
		std::vector<double> Lattice(static_cast<size_t>(Cells) * Cells);
		for (double& L : Lattice)
		{
			L = Grain.Next();
		}
		std::vector<double> Field(static_cast<size_t>(Size) * Size);
		for (int J = 0; J < Size; ++J)
		{
			for (int I = 0; I < Size; ++I)
			{
				const double X = (I + 0.5) * Cells / Size, Y = (J + 0.5) * Cells / Size;
				const int X0 = static_cast<int>(std::floor(X)) % Cells, Y0 = static_cast<int>(std::floor(Y)) % Cells;
				const int X1 = (X0 + 1) % Cells, Y1 = (Y0 + 1) % Cells;
				const double FX = Smooth(0.0, 1.0, X - std::floor(X)), FY = Smooth(0.0, 1.0, Y - std::floor(Y));
				const double Top = Lattice[static_cast<size_t>(Y0) * Cells + X0] * (1.0 - FX) + Lattice[static_cast<size_t>(Y0) * Cells + X1] * FX;
				const double Bottom = Lattice[static_cast<size_t>(Y1) * Cells + X0] * (1.0 - FX) + Lattice[static_cast<size_t>(Y1) * Cells + X1] * FX;
				Field[static_cast<size_t>(J) * Size + I] = Top * (1.0 - FY) + Bottom * FY;
			}
		}
		return Field;
	}
}

std::vector<uint8_t> SeaFace::MakeStreaks(int Size, double TileMetres, int Count)
{
	Size = std::max(Size, 16);
	const size_t Pixels = static_cast<size_t>(Size) * Size;
	std::vector<double> Cover(Pixels, 0.0), Rank(Pixels, 0.0), Along(Pixels, 0.0);
	const double PerMetre = Size / std::max(TileMetres, 1.0);
	const double Feather = std::max(0.3, 1.5 / PerMetre); // metres, and a pixel and a half at the least
	Dice Roll{ 0x57AEA4D5EAF0B1ull };
	// The rows the wind's drift gathers the foam into: along the wind, [EST] 8 to 25 m apart (a whole number across the
	// picture, so it repeats), each a little off its even place.
	const int Rows = std::max(1, static_cast<int>(std::lround(TileMetres / 14.0)));
	std::vector<double> RowV;
	for (int R = 0; R < Rows; ++R)
	{
		RowV.push_back((R + 0.5 + (Roll.Next() - 0.5) * 0.6) * Size / Rows);
	}
	auto Wrapped = [Size](int PU, int PV)
	{
		return static_cast<size_t>(((PV % Size) + Size) % Size) * Size + ((PU % Size) + Size) % Size;
	};
	std::vector<uint8_t> Taken(Pixels, 0);
	const int Margin = 2;
	for (int K = 0; K < Count; ++K)
	{
		// How big: [EST] 15 to 120 m long (most short), 0.4 to 2 m wide (most narrow); wandering 0.3 to 1.2 m across over
		// 20 to 60 m; beaded every 3 to 8 m and broken every 9 to 25 m; thinning over the last 15% at each end.
		const double Long = 15.0 + 105.0 * std::pow(Roll.Next(), 2.0);
		const double Wide = 0.4 + 1.6 * std::pow(Roll.Next(), 3.0);
		const double Wander = 0.3 + 0.9 * Roll.Next(), WanderLength = 20.0 + 40.0 * Roll.Next(), WanderPhase = Roll.Next() * 2.0 * Pi;
		const double Bead[4] = { 3.0 + 5.0 * Roll.Next(), 9.0 + 16.0 * Roll.Next(), Roll.Next() * 2.0 * Pi, Roll.Next() * 2.0 * Pi };
		const double Mine = Roll.Next();
		// Its foam at a pixel, its upwind end at U (pixels) and its line at V: how much, and how far along it (0 to 1).
		auto Foam = [&](double U, double V, int PU, int PV, double& Share)
		{
			const double A = ((PU + 0.5) - U) / PerMetre;
			Share = std::clamp(A / Long, 0.0, 1.0);
			if (A < -Feather || A > Long + Feather)
			{
				return 0.0;
			}
			const double Ends = std::clamp(std::min(A, Long - A) / (0.15 * Long), 0.0, 1.0);
			const double Half = 0.5 * Wide * std::sqrt(Ends);
			const double Line = V / PerMetre + Wander * std::sin(A * 2.0 * Pi / WanderLength + WanderPhase);
			const double C = std::abs((PV + 0.5) / PerMetre - Line);
			const double Body = std::clamp((Half - C) / Feather + 0.5, 0.0, 1.0);
			const double Beads = 0.55 + 0.45 * std::sin(A * 2.0 * Pi / Bead[0] + Bead[2]) * std::cos(A * 2.0 * Pi / Bead[1] + Bead[3]);
			const double Broken = std::clamp((Beads - 0.25) / 0.2, 0.0, 1.0);
			return Body * Broken * (0.35 + 0.65 * Ends);
		};
		const double Reach = (Wander + 0.5 * Wide + Feather) * PerMetre + 2.0;
		// Where: most on a row, a little off it; a few anywhere; never on another (tried again elsewhere, and left out if the
		// picture is too full for it).
		double U = 0.0, V = 0.0;
		bool bRoom = false;
		for (int Place = 0; Place < 40 && !bRoom; ++Place)
		{
			U = Roll.Next() * Size;
			if (Roll.Next() < 0.7)
			{
				V = RowV[std::min(Rows - 1, static_cast<int>(Roll.Next() * Rows))] + (Roll.Next() + Roll.Next() - 1.0) * 0.8 * PerMetre;
			}
			else
			{
				V = Roll.Next() * Size;
			}
			bRoom = true;
			const int UFrom = static_cast<int>(std::floor(U - Feather * PerMetre)) - 2, UTo = static_cast<int>(std::ceil(U + (Long + Feather) * PerMetre)) + 2;
			const int VFrom = static_cast<int>(std::floor(V - Reach)), VTo = static_cast<int>(std::ceil(V + Reach));
			for (int PU = UFrom; PU <= UTo && bRoom; ++PU)
			{
				for (int PV = VFrom; PV <= VTo; ++PV)
				{
					double Share = 0.0;
					if (Taken[Wrapped(PU, PV)] && Foam(U, V, PU, PV, Share) > 0.0)
					{
						bRoom = false;
						break;
					}
				}
			}
		}
		if (!bRoom)
		{
			continue;
		}
		const int UFrom = static_cast<int>(std::floor(U - Feather * PerMetre)) - 2, UTo = static_cast<int>(std::ceil(U + (Long + Feather) * PerMetre)) + 2;
		const int VFrom = static_cast<int>(std::floor(V - Reach)), VTo = static_cast<int>(std::ceil(V + Reach));
		for (int PU = UFrom; PU <= UTo; ++PU)
		{
			for (int PV = VFrom; PV <= VTo; ++PV)
			{
				double Share = 0.0;
				const double Here = Foam(U, V, PU, PV, Share);
				if (Here <= 0.0)
				{
					continue;
				}
				const size_t P = Wrapped(PU, PV);
				Cover[P] = Here;
				Rank[P] = Mine;
				Along[P] = Share;
				for (int DU = -Margin; DU <= Margin; ++DU)
				{
					for (int DV = -Margin; DV <= Margin; ++DV)
					{
						Taken[Wrapped(PU + DU, PV + DV)] = 1;
					}
				}
			}
		}
	}
	// Round each streak, where it has no foam, its own rank and along out to half a metre or so, so a read between pixels
	// (or from a smaller copy, far off) never mixes a streak's rank with the empty sea's and shows a rim round it.
	{
		const int Reach = std::max(2, static_cast<int>(std::lround(0.5 * PerMetre)));
		std::vector<double> RankOut = Rank, AlongOut = Along;
		for (int J = 0; J < Size; ++J)
		{
			for (int I = 0; I < Size; ++I)
			{
				const size_t P = static_cast<size_t>(J) * Size + I;
				if (Cover[P] > 0.0)
				{
					continue;
				}
				double Best = 0.0;
				for (int DJ = -Reach; DJ <= Reach; ++DJ)
				{
					for (int DI = -Reach; DI <= Reach; ++DI)
					{
						const size_t Q = static_cast<size_t>((J + DJ + Size) % Size) * Size + (I + DI + Size) % Size;
						const double Weight = Cover[Q] > 0.0 ? 1000.0 - (DI * DI + DJ * DJ) + Cover[Q] : 0.0;
						if (Weight > Best)
						{
							Best = Weight;
							RankOut[P] = Rank[Q];
							AlongOut[P] = Along[Q];
						}
					}
				}
			}
		}
		Rank.swap(RankOut);
		Along.swap(AlongOut);
	}
	// The grain: a value noise at two sizes, about a metre and a third of one, repeating with the picture.
	const int Coarse = std::max(2, static_cast<int>(std::lround(TileMetres / 1.0))), Fine = std::max(2, static_cast<int>(std::lround(TileMetres / 0.35)));
	const std::vector<double> Big = StreakNoise(Size, std::min(Coarse, Size), 81), Small = StreakNoise(Size, std::min(Fine, Size), 82);
	std::vector<uint8_t> Out(Pixels * 4);
	for (size_t P = 0; P < Pixels; ++P)
	{
		Out[P * 4 + 0] = Byte(Cover[P]);
		Out[P * 4 + 1] = Byte(Rank[P]);
		Out[P * 4 + 2] = Byte(Along[P]);
		Out[P * 4 + 3] = Byte((0.6 * Big[P] + 0.4 * Small[P] - 0.5) * 1.8 + 0.5);
	}
	return Out;
}
