#pragma once

#include "CoreMinimal.h"
#include "Campaign/CampaignMiniatures.h"

/** Shared by the files that build the miniatures. */
namespace MiniatureParts
{
	/** A point of a figure, set out from where the figure stands and turned with it. */
	struct FPlacing
	{
		FVector Where;
		double Scale;
		double Cos, Sin;

		FPlacing(const FVector& InWhere, double InScale, double TurnDegrees)
			: Where(InWhere), Scale(InScale), Cos(FMath::Cos(FMath::DegreesToRadians(TurnDegrees))), Sin(FMath::Sin(FMath::DegreesToRadians(TurnDegrees)))
		{
		}

		/** Forward is toward the figure's front, Right to its right, Up above the ground. */
		FVector At(double Forward, double Right, double Up) const
		{
			// A figure that is not turned faces along -X, and so has -Y on its right hand.
			const double X = -Forward, Y = -Right;
			return Where + FVector(X * Cos - Y * Sin, X * Sin + Y * Cos, Up) * Scale;
		}

		/** A direction given the same way, as a unit vector. */
		FVector Toward(double Forward, double Right, double Up) const
		{
			const double X = -Forward, Y = -Right;
			return FVector(X * Cos - Y * Sin, X * Sin + Y * Cos, Up).GetSafeNormal();
		}
	};

	/** A point of a model built in metres: so far forward of its middle, so far to its right, so far up. */
	inline FVector At(double F, double R, double U)
	{
		return FVector(-F, -R, U);
	}

	/** How a skin is painted: given where a point is (forward, right, up), which way the skin faces there, and which ring it is on. */
	using FPaint = TFunctionRef<FLinearColor(const FVector& Place, const FVector& Facing, int32 Ring)>;

	/**
	 * A skin drawn over a row of rings, each ring a loop of the same number of points. Lit as one smooth
	 * surface. The first and last rings are closed with flat ends if asked.
	 */
	inline void Skin(::FMiniatureMesh& M, const TArray<TArray<FVector>>& Rings, FPaint Paint, bool bCloseFirst = true, bool bCloseLast = true)
	{
		const int32 Count = Rings.Num(), Round = Rings[0].Num();
		TArray<FVector> Middles;
		for (const TArray<FVector>& Ring : Rings)
		{
			FVector Middle = FVector::ZeroVector;
			for (const FVector& Point : Ring)
			{
				Middle += Point / Round;
			}
			Middles.Add(Middle);
		}
		const int32 First = M.Vertices.Num();
		for (int32 J = 0; J < Count; ++J)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const FVector Here = Rings[J][K];
				const FVector Around = Rings[J][(K + 1) % Round] - Rings[J][(K + Round - 1) % Round];
				const FVector Along = Rings[FMath::Min(J + 1, Count - 1)][K] - Rings[FMath::Max(J - 1, 0)][K];
				FVector Normal = (Around ^ Along).GetSafeNormal();
				if (Normal.IsNearlyZero())
				{
					// A ring drawn to a point (a nose, a wingtip): it faces along the skin.
					Normal = (Middles[J] - Middles[J == 0 ? 1 : J - 1]).GetSafeNormal();
				}
				else if ((Normal | (Here - Middles[J])) < 0.0)
				{
					Normal = -Normal;
				}
				const FVector Place(-Here.X, -Here.Y, Here.Z);
				const FVector Facing(-Normal.X, -Normal.Y, Normal.Z);
				M.Add(Here, Normal, Paint(Place, Facing, J).ToFColor(true));
			}
		}
		for (int32 J = 0; J + 1 < Count; ++J)
		{
			for (int32 K = 0; K < Round; ++K)
			{
				const int32 A = First + J * Round + K, B = First + J * Round + (K + 1) % Round, C = A + Round, D = B + Round;
				const FVector Out = M.Normals[A] + M.Normals[B] + M.Normals[C] + M.Normals[D];
				M.Face(A, B, C, Out);
				M.Face(B, D, C, Out);
			}
		}
		for (int32 End = 0; End < 2; ++End)
		{
			if (!(End ? bCloseLast : bCloseFirst))
			{
				continue;
			}
			const int32 J = End ? Count - 1 : 0;
			const FVector Out = (Middles[J] - Middles[End ? Count - 2 : 1]).GetSafeNormal();
			const FColor Colour = Paint(FVector(-Middles[J].X, -Middles[J].Y, Middles[J].Z), FVector(-Out.X, -Out.Y, Out.Z), J).ToFColor(true);
			const int32 Hub = M.Add(Middles[J], Out, Colour);
			TArray<int32> Rim;
			for (int32 K = 0; K < Round; ++K)
			{
				Rim.Add(M.Add(Rings[J][K], Out, Colour));
			}
			for (int32 K = 0; K < Round; ++K)
			{
				M.Face(Hub, Rim[K], Rim[(K + 1) % Round], Out);
			}
		}
	}

	/** One cross-section of a body: how far forward it is, how high its middle is, half its width, half its depth. */
	struct FStation
	{
		double X, Z, W, H;
	};

	/**
	 * More stations between the ones given, following a curve through them, so a body swells and tapers
	 * smoothly from nose to tail and is not a row of cones. Where two stations are close together (the lip of
	 * a cowling, the back of a spinner) the step between them is kept as it is.
	 */
	inline TArray<FStation> Flowing(const TArray<FStation>& Stations)
	{
		TArray<FStation> Fine;
		const int32 Count = Stations.Num();
		for (int32 K = 0; K + 1 < Count; ++K)
		{
			const FStation& P0 = Stations[FMath::Max(K - 1, 0)];
			const FStation& P1 = Stations[K];
			const FStation& P2 = Stations[K + 1];
			const FStation& P3 = Stations[FMath::Min(K + 2, Count - 1)];
			const double Length = FMath::Abs(P2.X - P1.X);
			const int32 Parts = Length < 0.2 ? 1 : FMath::Clamp(FMath::CeilToInt32(Length / 0.3), 2, 8);
			for (int32 Part = 0; Part < Parts; ++Part)
			{
				const double T = static_cast<double>(Part) / Parts;
				auto Curve = [T](double A, double B, double C, double D)
				{
					// (a curve through B and C that heads for A before and D after, held between B and C so it cannot bulge past them)
					const double V = 0.5 * ((2.0 * B) + (-A + C) * T + (2.0 * A - 5.0 * B + 4.0 * C - D) * T * T + (-A + 3.0 * B - 3.0 * C + D) * T * T * T);
					return FMath::Clamp(V, FMath::Min(B, C), FMath::Max(B, C));
				};
				Fine.Add({ FMath::Lerp(P1.X, P2.X, T), Curve(P0.Z, P1.Z, P2.Z, P3.Z), Curve(P0.W, P1.W, P2.W, P3.W), Curve(P0.H, P1.H, P2.H, P3.H) });
			}
		}
		Fine.Add(Stations.Last());
		return Fine;
	}

	/** The rings of a body (a fuselage, a nacelle, a canopy) over its stations. Square above 2 makes the section boxier than an oval. Side moves the whole body to one side. */
	inline TArray<TArray<FVector>> Body(const TArray<FStation>& Given, double Square = 2.0, int32 Round = 20, double Side = 0.0)
	{
		const TArray<FStation> Stations = Flowing(Given);
		TArray<TArray<FVector>> Rings;
		for (const FStation& S : Stations)
		{
			TArray<FVector>& Ring = Rings.AddDefaulted_GetRef();
			for (int32 K = 0; K < Round; ++K)
			{
				const double Angle = UE_DOUBLE_TWO_PI * K / Round, C = FMath::Cos(Angle), Sn = FMath::Sin(Angle);
				const double Power = 2.0 / Square;
				const double Y = FMath::Sign(C) * FMath::Pow(FMath::Abs(C), Power), Z = FMath::Sign(Sn) * FMath::Pow(FMath::Abs(Sn), Power);
				Ring.Add(At(S.X, Side + Y * S.W, S.Z + Z * S.H));
			}
		}
		return Rings;
	}

	/** A model that can be asked for by name. */
	struct FModel
	{
		const TCHAR* Name;
		void (*Build)(struct ::FMiniatureMesh&);
		/** A line about it, with its real measurements; and whose it is. */
		const TCHAR* Title;
		const TCHAR* Nation;
		/** Whether it is built in the paint of whichever nation asks ("Truck:SOV"). */
		bool bAnyNation = false;
		/** How much room it needs in a row, in metres; and how high it flies, for an aircraft (0 for anything that stands on the ground). */
		double Room = 12.0;
		double Aloft = 0.0;
	};

	/** The nation a model that is the same for several nations is being built for ("Truck:SOV", "Bomber:ITA"). */
	FString& NationWanted();

	/** The aircraft (CampaignAircraft.cpp) and the warships (CampaignShips.cpp). */
	TConstArrayView<FModel> AircraftModels();
	TConstArrayView<FModel> ShipModels();
}
