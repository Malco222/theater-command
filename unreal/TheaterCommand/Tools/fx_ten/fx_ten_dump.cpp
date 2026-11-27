// Every piece the kit lays in five scripted fights, printed as a digest twice a second (and the pieces' count, what
// Draw laid and the lamps): the land guns (a tank gun, an anti-tank gun and a field howitzer firing 25, 120 and 280 m
// from the eye, a machine gun's bursts, shells, a burning wreck's steam and cook-off), a battleship's broadside close
// to the eye on the water, a flak barrage of every kind round the eye, a sea gun's turrets far off and the test
// barrage's tank gun on sand. It calls only what the kit had before this batch, so built against the kit before it
// prints what golden-default.sha256 holds, and run.sh holds today's to that, byte for byte: with no word given, not
// one piece moves.
//
//     Tools/fx_ten/run.sh                                    (builds it with g++ and clang++, compares)
//     fx_ten_dump <battle-fx.json> [overlay.json ...] > dump.txt
#include "fx_ten_common.h"

namespace
{
	using FxTen::Battle;
	using FxTen::Vec;

	void Sample(Battle& Fx, int Frame, int Drawn)
	{
		TheaterFx::Battle::Lamp Lit[4];
		const int N = Fx.Lamps(Frame / 30.0, Lit, 2, 1.0);
		std::printf("%5d %6zu %6d %d %016llx\n", Frame, Fx.Bits.size(), Drawn, N, static_cast<unsigned long long>(FxTen::Digest(Fx)));
	}

	/** One fight: Script called each frame, then Draw from Eye, sampled every 15 frames at 30 a second. */
	template <typename F>
	void Play(const FxTen::Book& Book, const char* Name, const Vec& Eye, const Vec& Look, int Frames, F Script)
	{
		Battle Fx;
		Fx.Setup(&Book, &FxTen::Flat, nullptr, 5u);
		Fx.SetWind(Vec(1.6, 0.4, 0.0));
		const Battle::View V = FxTen::Looking(Eye, Look);
		std::printf("# %s\n", Name);
		for (int Frame = 0; Frame < Frames; ++Frame)
		{
			const double Now = Frame / 30.0;
			Script(Fx, Frame, Now);
			const int Drawn = Fx.Draw(Now, V);
			if (Frame % 15 == 0)
			{
				Sample(Fx, Frame, Drawn);
			}
		}
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 2)
	{
		std::fprintf(stderr, "fx_ten_dump <battle-fx.json> [overlay.json ...]\n");
		return 2;
	}
	FxTen::Book Book;
	for (int I = 1; I < Argc; ++I)
	{
		if (!Book.Lay(Argv[I]))
		{
			return 2;
		}
	}
	// (the land guns: a tank gun 25 m off, an anti-tank gun at 120 m and a howitzer at 280 m, each every few seconds,
	// a stopped tank's machine gun, shells, a wreck burning, steaming and cooking off)
	Play(Book, "land guns", Vec(0.0, -25.0, 2.0), Vec(0.0, 40.0, 1.0), 1800, [](Battle& Fx, int Frame, double Now)
	{
		if (Frame % 180 == 10)
		{
			Fx.Muzzle(Now, Vec(-4.0, 0.0, 2.1), Vec(0.2, 1.0, 0.02), "tank_gun", "earth");
		}
		if (Frame % 210 == 40)
		{
			Fx.Muzzle(Now, Vec(30.0, 95.0, 1.0), Vec(0.0, 1.0, 0.05), "at_gun", "earth");
		}
		if (Frame % 240 == 70)
		{
			Fx.Muzzle(Now, Vec(-60.0, 250.0, 1.4), Vec(0.1, 1.0, 0.4), "howitzer", "earth");
		}
		if (Frame % 120 == 55)
		{
			Fx.Shots(Now, 3, Vec(6.0, 2.0, 0.6), Vec(20.0, 90.0, 0.4), "mg", 20, 15.0, "earth");
		}
		if (Frame % 150 == 20)
		{
			Fx.Shell(Now, Vec(-20.0 + Frame % 37, 160.0, 0.0), "how105", "earth");
		}
		if (Frame == 100)
		{
			Fx.Burn(Now, Vec(18.0, 60.0, 1.2), 2.4, 40.0, 11);
			Fx.Steam(Now, 12, Vec(18.0, 60.0, 2.4), Vec(0.0, 0.0, 1.0), 0.7, 5.0, "smoke");
			Fx.CookOff(Now, Vec(17.0, 61.0, 2.4), Vec(0.0, 0.0, 1.0), 0.8, 3, 2.5, 0.0, -1.0, 13);
		}
	});
	// (a battleship's broadside close to the eye on the water: the sea scene's rows today, the howitzer's at 180 mm
	// and over and the tank gun's under it; its brass thrown within 40 m)
	Play(Book, "broadside close", Vec(0.0, -30.0, 12.0), Vec(0.0, 20.0, 8.0), 900, [](Battle& Fx, int Frame, double Now)
	{
		if (Frame % 240 == 12)
		{
			for (int Turret = 0; Turret < 3; ++Turret)
			{
				Fx.Muzzle(Now, Vec(-60.0 + 55.0 * Turret, 0.0, 12.0), Vec(0.1, 1.0, 0.3), Turret == 1 ? "tank_gun" : "howitzer", "water");
			}
		}
	});
	// (flak round the eye: heavy every 0.9 s, medium every 1.4 s, light every 2.5 s, a mile up, 300 m to 1 km off)
	Play(Book, "flak barrage", Vec(0.0, 0.0, 1500.0), Vec(0.0, 600.0, 1500.0), 2700, [](Battle& Fx, int Frame, double Now)
	{
		const double Wobble = static_cast<double>((Frame * 37) % 23) - 11.0;
		if (Frame % 27 == 1)
		{
			Fx.AirFlak(Now, Vec(Wobble * 30.0, 700.0 + Wobble * 20.0, 1500.0 + Wobble * 4.0), "heavy");
		}
		if (Frame % 42 == 5)
		{
			Fx.AirFlak(Now, Vec(-Wobble * 25.0, 450.0 - Wobble * 10.0, 1490.0 - Wobble * 3.0), "medium");
		}
		if (Frame % 75 == 9)
		{
			Fx.AirFlak(Now, Vec(Wobble * 12.0, 320.0 + Wobble * 6.0, 1505.0), "light");
		}
	});
	// (the sea scene's turrets far off, as the lamps' dump has them)
	Play(Book, "broadside far", Vec(0.0, 500.0, 30.0), Vec(0.0, 1500.0, 0.0), 600, [](Battle& Fx, int Frame, double Now)
	{
		if (Frame % 50 == 7)
		{
			for (int Ship = 0; Ship < 3; ++Ship)
			{
				for (int Turret = 0; Turret < 3; ++Turret)
				{
					Fx.Muzzle(Now, Vec(-600.0 + 600.0 * Ship + 60.0 * Turret, 1500.0, 14.0), Vec(0.2, 1.0, 0.15), Ship == 1 ? "tank_gun" : "howitzer", "water");
				}
			}
		}
	});
	// (the test barrage's tank gun on sand, 4 m beside two riflemen, and an anti-tank gun's pile 6 m off)
	Play(Book, "guns on sand", Vec(-30.0, -14.0, 1.7), Vec(-36.0, -10.0, 1.0), 900, [](Battle& Fx, int Frame, double Now)
	{
		if (Frame % 120 == 3)
		{
			Fx.Muzzle(Now, Vec(-40.0, -6.0, 2.0), Vec(1.0, 0.25, 0.02), "tank_gun", "sand");
		}
		if (Frame % 150 == 33)
		{
			Fx.Muzzle(Now, Vec(-26.0, -8.0, 1.0), Vec(0.3, 1.0, 0.03), "at_gun", "sand");
		}
	});
	return 0;
}
