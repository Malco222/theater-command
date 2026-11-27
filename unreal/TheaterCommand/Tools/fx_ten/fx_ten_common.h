// The effects kit's first batch of the owner's film details (26 November, "do 2-11": flak by its size, no false brass
// at sea, a barrel's wisp, a breech's puff, a closed tank's hatch fumes and the odd case, cases over an open gun's
// side), held in the cloud: the recipe read as the game reads it, with its overlays laid over it in order
// (FBattleEffects::LayOver and Begin), a digest of everything the kit has laid, and the kit's own pieces compared
// piece by piece. Only what the kit had before this batch is used here, so fx_ten_dump builds against either kit.
#pragma once

#include "Battle/LandJson.h"
#define private public // (the pieces themselves, as tools/battle-fx/preview/barrage.cpp reads them)
#include "Effects/TheaterFxCore.h"
#undef private

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace FxTen
{
	using TheaterFx::Battle;
	using TheaterFx::Bit;
	using TheaterFx::Vec;

	/** The recipe and its overlays, each number by its dotted path, a later file's over an earlier's (as the game's
	 * FBattleFxRecipe walks them). */
	class Book : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double> Numbers;
		std::map<std::string, std::string> Words;

		void Walk(const LandJson::Json& J, const std::string& Path)
		{
			using T = LandJson::Json::Type;
			if (J.Kind == T::Object)
			{
				for (size_t I = 0; I < J.Keys.size(); ++I)
				{
					Walk(J.Values[I], Path.empty() ? J.Keys[I] : Path + "." + J.Keys[I]);
				}
			}
			else if (J.Kind == T::Array)
			{
				for (size_t I = 0; I < J.Items.size(); ++I)
				{
					Walk(J.Items[I], Path + "." + std::to_string(I));
				}
			}
			else if (J.Kind == T::Number || J.Kind == T::Bool)
			{
				Numbers[Path] = J.Num();
			}
			else if (J.Kind == T::String)
			{
				Words[Path] = J.Text;
			}
		}
		bool Lay(const std::string& Path)
		{
			std::string Text, Error;
			LandJson::Json Root;
			if (!LandJson::ReadFile(Path.c_str(), Text) || !LandJson::Parse(Text, Root, Error))
			{
				std::fprintf(stderr, "cannot read the recipe %s: %s\n", Path.c_str(), Error.c_str());
				return false;
			}
			Walk(Root, "");
			return true;
		}
		bool Find(const char* Path, double& Out) const override
		{
			const auto It = Numbers.find(Path);
			if (It == Numbers.end())
			{
				return false;
			}
			Out = It->second;
			return true;
		}
		bool FindWord(const char* Path, std::string& Out) const override
		{
			const auto It = Words.find(Path);
			if (It == Words.end())
			{
				return false;
			}
			Out = It->second;
			return true;
		}
	};

	inline double Flat(void*, double, double)
	{
		return 0.0;
	}

	/** An eye at Eye looking at At through the game's 42 degree lens, 1,600 pixels across. */
	inline Battle::View Looking(const Vec& Eye, const Vec& At, double Lens = 42.0)
	{
		Battle::View V;
		V.Eye = Eye;
		V.Forward = (At - Eye).Unit();
		V.Right = V.Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit(Vec(1.0, 0.0, 0.0));
		V.Up = V.Right.Cross(V.Forward).Unit();
		V.PixelsPerRadian = 1600.0 / (2.0 * std::tan(0.5 * Lens * 3.14159265358979323846 / 180.0));
		V.HalfAcross = std::tan(0.5 * Lens * 3.14159265358979323846 / 180.0);
		return V;
	}

	inline uint64_t Mix(uint64_t H, uint64_t V)
	{
		H ^= V + 0x9E3779B97F4A7C15ull + (H << 6) + (H >> 2);
		return H * 0xBF58476D1CE4E5B9ull;
	}
	inline uint64_t MixD(uint64_t H, double D)
	{
		uint64_t U = 0;
		std::memcpy(&U, &D, sizeof(U));
		return Mix(H, U);
	}
	inline uint64_t MixV(uint64_t H, const Vec& V)
	{
		return MixD(MixD(MixD(H, V.X), V.Y), V.Z);
	}
	inline uint64_t MixC(uint64_t H, const TheaterFx::Rgb& C)
	{
		return MixD(MixD(MixD(H, static_cast<double>(C.R)), static_cast<double>(C.G)), static_cast<double>(C.B));
	}

	/** One piece, by what makes it what it is: where and when it is born, how it moves, its size, look and life, its
	 * dice (its lumps' seed and its turn) and its tail. Two pieces alike in all of it are the same piece. */
	inline uint64_t PieceHash(const Bit& B)
	{
		uint64_t H = 1469598103934665603ull;
		H = Mix(H, static_cast<uint64_t>(B.Form));
		H = MixD(MixD(H, B.Born), B.Life);
		H = MixV(MixV(MixV(H, B.From), B.Vel), B.Along);
		H = MixD(MixD(MixD(MixD(H, B.Drag), B.Gravity), B.Rise), B.Windage);
		H = MixD(MixD(MixD(MixD(H, B.R0), B.R1), B.Grow), B.Stretch);
		H = MixD(MixD(MixD(MixD(H, B.Alpha), B.FadeIn), B.Hold), B.Heart);
		H = MixC(MixC(H, B.Col), B.Col2);
		H = Mix(H, B.LumpSeed);
		H = MixD(MixD(MixD(H, B.Turn), B.TurnRate), B.Rough);
		H = MixD(MixD(H, B.Land), B.Land2);
		H = MixV(MixV(H, B.At1), B.At2);
		H = MixD(MixD(H, B.Tumble), B.Glint);
		H = Mix(H, B.bTail ? 1u : 0u);
		H = MixD(MixD(MixD(H, B.TailThird), B.TailWide), B.TailLift);
		H = MixV(H, B.TailMid);
		H = MixD(MixD(H, B.LeastPx), B.HazeBy);
		H = Mix(H, (B.bThrown ? 1u : 0u) | (B.bLight ? 2u : 0u) | (B.bKeep ? 4u : 0u));
		return H;
	}

	/** Everything the kit holds now, in its order: how many pieces and their digest. */
	inline uint64_t Digest(const Battle& Fx)
	{
		uint64_t H = 7;
		for (const Bit& B : Fx.Bits)
		{
			H = Mix(H, PieceHash(B));
		}
		return H;
	}

	/** The pieces as a sorted list of their hashes (a heap of them: their order aside). */
	inline std::vector<uint64_t> Heap(const Battle& Fx)
	{
		std::vector<uint64_t> Out;
		Out.reserve(Fx.Bits.size());
		for (const Bit& B : Fx.Bits)
		{
			Out.push_back(PieceHash(B));
		}
		std::sort(Out.begin(), Out.end());
		return Out;
	}

	/** Of Small's pieces, how many Big has not (Small inside Big: none). */
	inline size_t Missing(const std::vector<uint64_t>& Small, const std::vector<uint64_t>& Big)
	{
		size_t Miss = 0, J = 0;
		for (const uint64_t H : Small)
		{
			while (J < Big.size() && Big[J] < H)
			{
				++J;
			}
			if (J < Big.size() && Big[J] == H)
			{
				++J;
			}
			else
			{
				++Miss;
			}
		}
		return Miss;
	}
}
