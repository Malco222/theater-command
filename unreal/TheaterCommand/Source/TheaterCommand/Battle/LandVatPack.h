#pragma once

// The poses' picture of the men's crowd (SO1's draw: Battle/LandVat), packed with no engine, so the cloud can check it
// (Tools/land_show/land_vat_pack_test.cpp) against the bank the material reads it for.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace LandVatPack
{
	/** The most points a set may have: a point's number rides the mesh as a half float, exact to 2,048. */
	constexpr int MostPoints = 2048;

	/** A float as an IEEE half (round to nearest, ties to even; too large: infinity; too small: zero or subnormal). */
	inline uint16_t Half(float F)
	{
		uint32_t X = 0;
		std::memcpy(&X, &F, sizeof(X));
		const uint32_t Sign = (X >> 16) & 0x8000u;
		const uint32_t Abs = X & 0x7FFFFFFFu;
		if (Abs >= 0x7F800000u)
		{
			return static_cast<uint16_t>(Sign | 0x7C00u | (Abs > 0x7F800000u ? 0x200u : 0u));
		}
		if (Abs >= 0x477FF000u)
		{
			return static_cast<uint16_t>(Sign | 0x7C00u);
		}
		if (Abs < 0x38800000u)
		{
			// (subnormal: the bits shifted in by hand, rounded to nearest even)
			if (Abs < 0x33000000u)
			{
				return static_cast<uint16_t>(Sign);
			}
			const uint32_t Exp = Abs >> 23;
			const uint32_t Mant = (Abs & 0x7FFFFFu) | 0x800000u;
			const uint32_t Shift = 126u - Exp;
			uint32_t H = Mant >> Shift;
			const uint32_t Rest = Mant & ((1u << Shift) - 1u), HalfWay = 1u << (Shift - 1u);
			if (Rest > HalfWay || (Rest == HalfWay && (H & 1u)))
			{
				++H;
			}
			return static_cast<uint16_t>(Sign | H);
		}
		uint32_t H = ((Abs - 0x38000000u) >> 13);
		const uint32_t Rest = Abs & 0x1FFFu;
		if (Rest > 0x1000u || (Rest == 0x1000u && (H & 1u)))
		{
			++H;
		}
		return static_cast<uint16_t>(Sign | H);
	}

	/** A half back to a float. */
	inline float FromHalf(uint16_t H)
	{
		const uint32_t Sign = static_cast<uint32_t>(H & 0x8000u) << 16;
		const uint32_t Exp = (H >> 10) & 0x1Fu;
		const uint32_t Mant = H & 0x3FFu;
		uint32_t X = 0;
		if (Exp == 0)
		{
			const float V = std::ldexp(static_cast<float>(Mant), -24);
			return Sign ? -V : V;
		}
		if (Exp == 31)
		{
			X = Sign | 0x7F800000u | (Mant << 13);
		}
		else
		{
			X = Sign | ((Exp + 112u) << 23) | (Mant << 13);
		}
		float F = 0.f;
		std::memcpy(&F, &X, sizeof(F));
		return F;
	}

	/** One body set's poses at one level, as the card holds them. */
	struct Packed
	{
		/** Width a column a point; Height two rows a pose: rows 0 to Poses - 1 each point's way from its rest (Unreal's
		 * units, his own axes), rows Poses to 2 Poses - 1 its facing (unit long). */
		int Width = 0, Height = 0, Poses = 0;
		/** Red, green, blue, alpha halfs a texel, row by row (PF_FloatRGBA). */
		std::vector<uint16_t> Texels;
		/** The pose his mesh rests in (the first his file has). */
		int Rest = -1;
		/** The rest's box and every pose's he has, Unreal's units (the mesh's bounds are grown by the difference). */
		float RestLo[3] = { 0.f, 0.f, 0.f }, RestHi[3] = { 0.f, 0.f, 0.f }, AllLo[3] = { 0.f, 0.f, 0.f }, AllHi[3] = { 0.f, 0.f, 0.f };
	};

	/**
	 * Packs LandMen::Crowd::BankBody's buffers (pose-major, (k * Points + p) * 3, metres) for Poses poses, Has[k] whether
	 * the file has pose k (a pose it lacks is packed as his rest), U Unreal's units a metre. False (Out untouched) where
	 * the buffers do not fit, the set is over MostPoints, or it has no pose at all.
	 */
	inline bool Pack(const std::vector<float>& At, const std::vector<float>& Facing, const std::vector<bool>& Has, int Points, int Poses, double U, Packed& Out)
	{
		const size_t Each = static_cast<size_t>(Points) * 3;
		if (Points <= 0 || Points > MostPoints || Poses <= 0 || Has.size() != static_cast<size_t>(Poses) || At.size() != Each * static_cast<size_t>(Poses)
			|| Facing.size() != At.size())
		{
			return false;
		}
		int Rest = -1;
		for (int K = 0; K < Poses && Rest < 0; ++K)
		{
			if (Has[static_cast<size_t>(K)])
			{
				Rest = K;
			}
		}
		if (Rest < 0)
		{
			return false;
		}
		Packed P;
		P.Width = Points;
		P.Height = 2 * Poses;
		P.Poses = Poses;
		P.Rest = Rest;
		P.Texels.assign(static_cast<size_t>(P.Width) * static_cast<size_t>(P.Height) * 4, 0);
		const float Units = static_cast<float>(U);
		const float* RestAt = At.data() + static_cast<size_t>(Rest) * Each;
		const float* RestFacing = Facing.data() + static_cast<size_t>(Rest) * Each;
		for (int A = 0; A < 3; ++A)
		{
			P.RestLo[A] = P.AllLo[A] = 3.4e38f;
			P.RestHi[A] = P.AllHi[A] = -3.4e38f;
		}
		for (int Pt = 0; Pt < Points; ++Pt)
		{
			for (int A = 0; A < 3; ++A)
			{
				const float V = RestAt[static_cast<size_t>(Pt) * 3 + static_cast<size_t>(A)] * Units;
				P.RestLo[A] = std::min(P.RestLo[A], V);
				P.RestHi[A] = std::max(P.RestHi[A], V);
			}
		}
		auto Put = [&P](int Row, int Column, float R, float G, float B)
		{
			const size_t T = (static_cast<size_t>(Row) * static_cast<size_t>(P.Width) + static_cast<size_t>(Column)) * 4;
			P.Texels[T] = Half(R);
			P.Texels[T + 1] = Half(G);
			P.Texels[T + 2] = Half(B);
			P.Texels[T + 3] = 0;
		};
		for (int K = 0; K < Poses; ++K)
		{
			const bool bHas = Has[static_cast<size_t>(K)];
			const float* PoseAt = bHas ? At.data() + static_cast<size_t>(K) * Each : RestAt;
			const float* PoseFacing = bHas ? Facing.data() + static_cast<size_t>(K) * Each : RestFacing;
			for (int Pt = 0; Pt < Points; ++Pt)
			{
				const size_t Q = static_cast<size_t>(Pt) * 3;
				Put(K, Pt, (PoseAt[Q] - RestAt[Q]) * Units, (PoseAt[Q + 1] - RestAt[Q + 1]) * Units, (PoseAt[Q + 2] - RestAt[Q + 2]) * Units);
				float F[3] = { PoseFacing[Q], PoseFacing[Q + 1], PoseFacing[Q + 2] };
				const float L = std::sqrt(F[0] * F[0] + F[1] * F[1] + F[2] * F[2]);
				if (L > 1e-6f)
				{
					F[0] /= L;
					F[1] /= L;
					F[2] /= L;
				}
				else
				{
					F[0] = F[1] = 0.f;
					F[2] = 1.f;
				}
				Put(Poses + K, Pt, F[0], F[1], F[2]);
				if (bHas)
				{
					for (int A = 0; A < 3; ++A)
					{
						const float V = PoseAt[Q + static_cast<size_t>(A)] * Units;
						P.AllLo[A] = std::min(P.AllLo[A], V);
						P.AllHi[A] = std::max(P.AllHi[A], V);
					}
				}
			}
		}
		Out = std::move(P);
		return true;
	}

	/**
	 * The texel M_LandCrowd reads for the point whose number its mesh carries (a half float) in row Row, as the card
	 * reads it at the picture's first level with the nearest texel: u = (Number + 0.5) / Width, v = (Row + 0.5) / Height,
	 * in 32-bit floats as the vertex shader works. Column and row out; false if outside the picture.
	 */
	inline bool Read(const Packed& P, uint16_t NumberHalf, float Row, int& OutColumn, int& OutRow)
	{
		const float Number = FromHalf(NumberHalf);
		const float Uu = (Number + 0.5f) / static_cast<float>(P.Width);
		const float Vv = (Row + 0.5f) / static_cast<float>(P.Height);
		OutColumn = static_cast<int>(std::floor(Uu * static_cast<float>(P.Width)));
		OutRow = static_cast<int>(std::floor(Vv * static_cast<float>(P.Height)));
		return OutColumn >= 0 && OutColumn < P.Width && OutRow >= 0 && OutRow < P.Height;
	}
}
