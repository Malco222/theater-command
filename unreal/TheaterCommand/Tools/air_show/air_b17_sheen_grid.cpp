// The B-17 paint sheen's law on a fixed grid (row flyB17Sheen): AirSheen::Law and AirSheen::GuardOf for a few tunes,
// inks, suns, eyes, normals and carried pairs, one line a case, for air_b17_sheen_material.py to compare with
// M_AirPaint's own nodes (Tools/build_content.py, its "if air:" block, run on plain numbers). So the material on the
// Mac and the law the cloud tests check are one law, wiring and all. Each line: the tune's seven launch-word numbers
// (gain floor spec metal metalfrom glare graze), the ink (rough, spec), the sun, the eye, the normal (x y z each), U,
// V, the guard, then rough, spec and metal. Usage: air_b17_sheen_grid > grid.txt
#include "Battle/AirSheen.h"

#include <cmath>
#include <cstdio>

int main()
{
	const double Us[] = { 0, 0.001, 0.01, 0.03, 0.049, 0.05, 0.1, 0.2, 0.39, 0.45, 0.51, 0.6, 0.7, 0.75, 0.77, 0.78, 0.8, 0.83, 0.9 };
	const double Vs[] = { 0, 0.18, 0.35, 0.4, 0.45, 0.5, 0.51, 0.55, 0.8, 1 };
	const double Inks[][2] = { { 0.9, 0.15 }, { 0.6, 0.35 } };
	AirSheen::Tune Tunes[4];
	Tunes[1].Gain = 0.6;
	Tunes[1].Floor = 0.55;
	Tunes[1].Spec = 0.35;
	Tunes[1].Metal = 0.7;
	Tunes[1].MetalFrom = 0.4;
	Tunes[2].Glare = 0.0;
	Tunes[2].Graze = 1.0;
	Tunes[2].Gain = 1.5;
	Tunes[3].Floor = 0.0;
	Tunes[3].Spec = 0.0;
	Tunes[3].MetalFrom = 0.0;
	Tunes[3].Graze = 0.5;
	const double D = 3.14159265358979323846 / 180.0;
	const double Suns[] = { 13, 10, 20, 31, 60 };
	const double Eyes[][2] = { { 13, 180 }, { 20, 180 }, { 35, 180 }, { 5, 90 }, { 60, 0 }, { 2, 0 } }; // (up, round from the sun)
	const AirSheen::Vec3 Normals[] = { { 0, 0, 1 }, { 0, 0.7071067811865476, 0.7071067811865476 } };
	for (const AirSheen::Tune& T : Tunes)
	{
		for (const auto& Ink : Inks)
		{
			for (const double Sun : Suns)
			{
				for (const auto& Eye : Eyes)
				{
					for (const AirSheen::Vec3& Normal : Normals)
					{
						const AirSheen::Vec3 S{ std::cos(Sun * D), 0, std::sin(Sun * D) };
						const AirSheen::Vec3 E{ std::cos(Eye[0] * D) * std::cos(Eye[1] * D), std::cos(Eye[0] * D) * std::sin(Eye[1] * D), std::sin(Eye[0] * D) };
						const double G = AirSheen::GuardOf(Normal, E, S, T);
						for (const double U : Us)
						{
							for (const double V : Vs)
							{
								const AirSheen::Finish F = AirSheen::Law(U, V, Ink[0], Ink[1], T, G);
								std::printf("%.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g %.17g\n",
									T.Gain, T.Floor, T.Spec, T.Metal, T.MetalFrom, T.Glare, T.Graze, Ink[0], Ink[1], S.X, S.Y, S.Z, E.X, E.Y, E.Z, Normal.X, Normal.Y,
									Normal.Z, U, V, G, F.Rough, F.Spec, F.Metal);
							}
						}
					}
				}
			}
		}
	}
	return 0;
}
