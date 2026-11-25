// The flashes' lamps as the kit hands them out with no scene's rule (SetLampRule never called): every lamp of every
// frame of a scripted fight on land and at sea, printed exactly (hex floats), with fire.lamp's steady lights and
// without them. It calls only what the kit had before the lamp rule, so the same file built against the kit before it
// printed what Tools/fx_lamps/golden-default-lamps.sha256 holds, and run.sh holds today's to that, byte for byte: with
// no scene opting in, not one lamp moves.
//
//     Tools/fx_lamps/run.sh                              (builds it with g++ and clang++, compares)
//     fx_lamps_dump <battle-fx.json> > lamps.txt
#include "fx_lamps_recipe.h"

#include <cstdio>
#include <string>

namespace
{
	using FxLamps::JsonRecipe;
	using FxLamps::Looking;
	using TheaterFx::Vec;

	/** One scripted fight, every frame's lamps printed. Day: 0 night, 1 day (Lamps' Day); Room: the lamps asked. */
	void Play(const JsonRecipe& Book, const char* Name, double Day, int Room)
	{
		TheaterFx::Battle Fx;
		Fx.Setup(&Book, &FxLamps::Flat, nullptr, 11u);
		Fx.SetWind(Vec(3.0, 0.8, 0.0));
		Fx.OilFire(0.0, 501, Vec(4000.0, 4500.0, 0.0), 150.0, "sunk", 0.0, 30.0);
		Fx.OilFire(0.0, 502, Vec(-900.0, 1400.0, 0.0), 60.0, "burning", 0.0, 10.0);
		Fx.Burn(0.5, Vec(-40.0, 80.0, 2.0), 3.0, 20.0, 7);
		const Vec Eye(0.0, -300.0, 30.0);
		const TheaterFx::Battle::View V = Looking(Eye, Vec(0.0, 1500.0, 0.0));
		std::printf("# %s: day %.2f, room %d\n", Name, Day, Room);
		const double Fps = 60.0;
		for (int Frame = 0; Frame < 900; ++Frame)
		{
			const double Now = Frame / Fps;
			// (a battle line's guns as the sea scene fires them today, the howitzer row at 180 mm and up and the tank gun's
			// below; field guns and tanks on land; a shell or two)
			if (Frame % 50 == 7)
			{
				for (int Ship = 0; Ship < 3; ++Ship)
				{
					for (int Turret = 0; Turret < 3; ++Turret)
					{
						const Vec At(-600.0 + 600.0 * Ship + 60.0 * Turret, 1500.0, 14.0);
						Fx.Muzzle(Now, At, Vec(0.2, 1.0, 0.15), Ship == 1 ? "tank_gun" : "howitzer", "water");
					}
				}
			}
			if (Frame % 37 == 3)
			{
				Fx.Muzzle(Now, Vec(-30.0 + Frame % 11, 40.0, 2.0), Vec(1.0, 0.3, 0.05), "tank_gun", "earth");
			}
			if (Frame % 73 == 11)
			{
				Fx.Muzzle(Now, Vec(25.0, 60.0, 1.5), Vec(0.6, 1.0, 0.3), "howitzer", "earth");
				Fx.Shell(Now, Vec(10.0, 140.0, 0.0), "how105", "earth");
			}
			if (Frame == 200)
			{
				Fx.FuelBall(Now, Vec(60.0, 120.0, 1.0), 6.0);
			}
			Fx.Draw(Now, V);
			TheaterFx::Battle::Lamp Lit[8];
			const int N = Fx.Lamps(Now, Lit, Room, Day);
			if (N <= 0)
			{
				continue;
			}
			std::printf("%d %d", Frame, N);
			for (int I = 0; I < N; ++I)
			{
				const TheaterFx::Battle::Lamp& L = Lit[I];
				std::printf(" | %a %a %a %a %a %a %a %a", L.At.X, L.At.Y, L.At.Z, L.Strength, L.Radius, static_cast<double>(L.Col.R), static_cast<double>(L.Col.G), static_cast<double>(L.Col.B));
			}
			std::printf("\n");
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "fx_lamps_dump <battle-fx.json>\n");
		return 2;
	}
	JsonRecipe Steady;
	if (!FxLamps::LoadRecipe(Argv[1], Steady))
	{
		return 2;
	}
	JsonRecipe Plain = Steady;
	Plain.Hide = "fire.lamp.";
	Play(Steady, "steady lamps (fire.lamp), night", 0.0, 2);
	Play(Steady, "steady lamps (fire.lamp), dusk", 0.5, 2);
	Play(Steady, "steady lamps (fire.lamp), day", 1.0, 2);
	Play(Steady, "steady lamps (fire.lamp), night, room 4", 0.0, 4);
	Play(Plain, "plain lamps (no fire.lamp), night", 0.0, 2);
	Play(Plain, "plain lamps (no fire.lamp), day", 1.0, 2);
	return 0;
}
