// The effects kit's recipe for the lamps' cloud test (Tools/fx_lamps): Content/Effects/battle-fx.json read with the
// game's own small JSON reader, every number by its dotted path ("muzzle.naval_heavy.lamp_cd"), as the game's
// FBattleFxRecipe flattens it. Hide leaves one prefix out (fire.lamp.: the kit's plain lamps, Lamps without its
// steady ones). Only what the kit had before the lamp rule is used here, so fx_lamps_dump builds against either.
#pragma once

#include "Battle/LandJson.h"
#include "Effects/TheaterFxCore.h"

#include <cmath>
#include <cstdio>
#include <map>
#include <string>

namespace FxLamps
{
	using TheaterFx::Vec;

	class JsonRecipe : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double> Numbers;
		std::map<std::string, std::string> Words;
		std::string Hide;

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
		bool Find(const char* Path, double& Out) const override
		{
			if (!Hide.empty() && std::string(Path).rfind(Hide, 0) == 0)
			{
				return false;
			}
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
		double Get(const std::string& Path, double Otherwise) const
		{
			const auto It = Numbers.find(Path);
			return It == Numbers.end() ? Otherwise : It->second;
		}
	};

	/** The recipe at Path, or false (and why on stderr). */
	inline bool LoadRecipe(const char* Path, JsonRecipe& Into)
	{
		std::string Text, Error;
		LandJson::Json Root;
		if (!LandJson::ReadFile(Path, Text) || !LandJson::Parse(Text, Root, Error))
		{
			std::fprintf(stderr, "cannot read the recipe %s: %s\n", Path, Error.c_str());
			return false;
		}
		Into.Walk(Root, "");
		return true;
	}

	/** A flat sea (or field) at height 0. */
	inline double Flat(void*, double, double)
	{
		return 0.0;
	}

	/** An eye at Eye looking at At through a 55 degree lens 1,600 pixels across. */
	inline TheaterFx::Battle::View Looking(const Vec& Eye, const Vec& At)
	{
		TheaterFx::Battle::View V;
		V.Eye = Eye;
		V.Forward = (At - Eye).Unit();
		V.Right = V.Forward.Cross(Vec(0.0, 0.0, 1.0)).Unit(Vec(1.0, 0.0, 0.0));
		V.Up = V.Right.Cross(V.Forward).Unit();
		V.PixelsPerRadian = 1600.0 / (2.0 * std::tan(0.5 * 55.0 * 3.14159265358979323846 / 180.0));
		return V;
	}
}
