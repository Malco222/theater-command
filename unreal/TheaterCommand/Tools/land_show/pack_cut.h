#pragma once
// A rider's look without his pack, or his helmet, or both (Effects, R2: Crowd::AddRider's bNoPack and bBareHead; the
// soldiers thread's pack_tris and helmet_tris), checked against the cut written out again here plainly: his soldier
// file's mesh at one level with those runs of its triangles left out, the points only the runs use dropped, the rest in
// the file's order. For the tests: Tools/land_show/land_pack_test.cpp, Tools/land_air, Tools/land_bail. Not for the game.
#include "Battle/LandMen.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace PackCut
{
	/** A run of a level's triangles, [First, First + Count): a file's helmet_tris or pack_tris at that level. */
	struct Run
	{
		int First = 0, Count = 0;
	};

	inline Run HelmetRun(const LandMen::Soldier& S, LandMen::Lod L) { return Run{ S.HelmetFirst[L], S.HelmetCount[L] }; }
	inline Run PackRun(const LandMen::Soldier& S, LandMen::Lod L) { return Run{ S.PackFirst[L], S.PackCount[L] }; }

	/** The level's mesh with the runs left out, as a rider's look should hold it. */
	struct Cut
	{
		std::vector<int> ToCompact;         // the file's point -> the cut's, or -1
		std::vector<uint32_t> Triangles;    // the cut's, in the file's order
		int Points = 0;
		int DroppedPoints = 0;              // only the runs' triangles used them
		int LeftOut = 0;                    // triangles of the runs
		int Orphans = 0;                    // a kept triangle's point dropped (never, if the cut is right)
	};

	inline Cut CutOf(const LandMen::Mesh& Full, const std::vector<Run>& Runs)
	{
		Cut Out;
		const size_t Tris = Full.Triangles.size() / 3, N = static_cast<size_t>(Full.Points);
		auto InRun = [&Runs](size_t T) {
			for (const Run& R : Runs)
			{
				if (R.Count > 0 && T >= static_cast<size_t>(R.First) && T < static_cast<size_t>(R.First) + static_cast<size_t>(R.Count)) return true;
			}
			return false;
		};
		// (a point stays unless only the runs' triangles use it: a point no triangle uses stays too, as in the file)
		std::vector<uint8_t> Used(N, 0);
		for (size_t T = 0; T < Tris; ++T)
		{
			const uint8_t Bit = InRun(T) ? 1 : 2;
			for (size_t C = 0; C < 3; ++C) Used[Full.Triangles[T * 3 + C]] = static_cast<uint8_t>(Used[Full.Triangles[T * 3 + C]] | Bit);
		}
		Out.ToCompact.assign(N, -1);
		for (size_t I = 0; I < N; ++I)
		{
			if (Used[I] == 1) { ++Out.DroppedPoints; continue; }
			Out.ToCompact[I] = Out.Points++;
		}
		for (size_t T = 0; T < Tris; ++T)
		{
			if (InRun(T)) { ++Out.LeftOut; continue; }
			for (size_t C = 0; C < 3; ++C)
			{
				const int P = Out.ToCompact[Full.Triangles[T * 3 + C]];
				Out.Orphans += P < 0 ? 1 : 0;
				Out.Triangles.push_back(static_cast<uint32_t>(P < 0 ? 0 : P));
			}
		}
		return Out;
	}

	/**
	 * Man's look and skin at level L against the cut he should be (Want), beside Whole: the same file's rider with his
	 * helmet and pack on, the same seed and ride (so the same face, pose and colours). His triangles are the cut's, to the
	 * last index; his points as many; each kept point skinned (place and facing) and coloured (paint and colour) as the
	 * whole man's own point, to the bit. Empty where all agree, else what first did not.
	 */
	inline std::string Compare(LandMen::Crowd& C, int Man, int Whole, LandMen::Lod L, const Cut& Want)
	{
		char Why[256];
		const int Pm = C.PointsOf(Man, L), Pw = C.PointsOf(Whole, L);
		if (Pm != Want.Points)
		{
			std::snprintf(Why, sizeof(Why), "%d points, the cut %d", Pm, Want.Points);
			return Why;
		}
		const LandMen::Look& Lm = C.LookOf(Man, L);
		const LandMen::Look& Lw = C.LookOf(Whole, L);
		if (Lm.Triangles != Want.Triangles)
		{
			size_t K = 0;
			while (K < Lm.Triangles.size() && K < Want.Triangles.size() && Lm.Triangles[K] == Want.Triangles[K]) ++K;
			std::snprintf(Why, sizeof(Why), "%zu triangles, the cut %zu; first apart at corner %zu", Lm.Triangles.size() / 3, Want.Triangles.size() / 3, K);
			return Why;
		}
		std::vector<float> Am(static_cast<size_t>(Pm) * 3), Fm(Am.size()), Aw(static_cast<size_t>(Pw) * 3), Fw(Aw.size());
		C.Skin(Man, L, Am.data(), Fm.data());
		C.Skin(Whole, L, Aw.data(), Fw.data());
		int Moved = 0, Recoloured = 0;
		for (size_t I = 0; I < Want.ToCompact.size(); ++I)
		{
			const int P = Want.ToCompact[I];
			if (P < 0) continue;
			const size_t A = static_cast<size_t>(P) * 3, B = I * 3;
			if (B + 2 >= Aw.size() || A + 2 >= Am.size()) { ++Moved; continue; }
			Moved += std::memcmp(&Am[A], &Aw[B], sizeof(float) * 3) != 0 || std::memcmp(&Fm[A], &Fw[B], sizeof(float) * 3) != 0 ? 1 : 0;
			Recoloured += Lm.Paint[static_cast<size_t>(P)] != Lw.Paint[I] || Lm.Rgb[A] != Lw.Rgb[B] || Lm.Rgb[A + 1] != Lw.Rgb[B + 1] || Lm.Rgb[A + 2] != Lw.Rgb[B + 2] ? 1 : 0;
		}
		if (Moved > 0 || Recoloured > 0)
		{
			std::snprintf(Why, sizeof(Why), "%d kept points skinned apart from the whole man's, %d coloured apart", Moved, Recoloured);
			return Why;
		}
		return std::string();
	}

	/** A ride for a rider who stands still in clip ClipIx at Time, at (300, 200) on flat ground facing east, unarmed. */
	inline LandMen::Crowd::Ride Standing(int ClipIx, float Time)
	{
		LandMen::Crowd::Ride R;
		R.bShown = true;
		R.Clip = ClipIx;
		R.Time = Time;
		R.bUnarmed = true;
		float* M = R.Seat.R;
		M[0] = 1.f; M[1] = 0.f; M[2] = 0.f;
		M[3] = 0.f; M[4] = -1.f; M[5] = 0.f;
		M[6] = 0.f; M[7] = 0.f; M[8] = 1.f;
		R.Seat.T[0] = 300.f;
		R.Seat.T[1] = 200.f;
		R.Seat.T[2] = 0.f;
		return R;
	}
}
