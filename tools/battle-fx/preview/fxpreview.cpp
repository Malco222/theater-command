// The game kit's own preview: the battle effects code the game runs (unreal/.../Effects/TheaterFxCore.cpp), drawn
// here by a small software painter so that the art director can see what the code draws without the Mac.
//
//   python3 flatten.py ../fx.json > /tmp/recipe.txt
//   SRC=../../../unreal/TheaterCommand/Source/TheaterCommand
//   clang++ -O2 -std=c++20 -I $SRC fxpreview.cpp $SRC/Effects/TheaterFxCore.cpp -o fxpreview
//   ./fxpreview /tmp/recipe.txt <out folder> <size> <ground> <frames, e.g. 4,8,40> [WxH] [back up]
//     (size "mg": a machine gun's burst from 30 m away into the ground there, in place of a shell; FX_MG_VIEW=side|behind)
//   ./fxpreview /tmp/recipe.txt <out folder> air <rifle|hmg|cannon20> <frames> [WxH] [chase|side|close|far|target|hit|trail|wake|map|fall]
//   ./fxpreview /tmp/recipe.txt <out folder> oil <sunk|burning|horizon> <frames> [WxH]
//   ./fxpreview /tmp/recipe.txt <out folder> drive <sand|earth|...> <frames> [WxH] [eye|side|high|top|gun]
//   ./fxpreview /tmp/recipe.txt <out folder> fate <steam|out|cook|glance> <frames> [WxH]
//   ./fxpreview /tmp/recipe.txt <out folder> sea <a name for the frames> <frames> [WxH]
//   ./fxpreview /tmp/recipe.txt <out folder> raid <earth|town|water> <frames> [WxH]
//   ./fxpreview /tmp/recipe.txt <out folder> wreck <side|battle|far|tall> <frames> [WxH]
//   ./fxpreview /tmp/recipe.txt <out folder> seafx <screen...|splash...> <frames> [WxH]
//
// The sea shot (the sea battle's range): three ships in line ahead, FX_GAP m (500) apart and FX_SHIP_LONG m (200)
// long, steaming along +X at FX_KNOTS (25), seen broadside on from FX_DIST m (15000) by an eye FX_EYE_H m (24) up with
// an FX_LENS degree lens (55), looking at FX_LOOK_X m along the line and FX_LOOK_H m (30) up. The left ship fires four
// turrets of FX_GUN (howitzer) at the eye at FX_FIRE_AT s (0.98; again every FX_EVERY s); a heavy, a medium and a
// light shell fall short of the middle one at FX_FALL_AT s (1.18), and she is hit on her deck and on her armour at
// FX_HIT_AT s (2.48; FX_NO_HIT=1: not); the right one burns from 0 s, FX_BURN m across (20; 0: no fire), and the fire
// rides with her (BurnMove). FX_USE=sea (as a scene that calls UseSeaDistance), air (UseAirDistance) or none.
// FX_FUNNEL=<a trail kind>: each funnel smokes, FX_FUNNEL_SPAN m across, from FX_FUNNEL_UP m up (FX_POWER=share of
// her top speed, FX_POWER_AT=s and FX_POWER_TO=share: she rings up). FX_FOLLOW=1: the eye rides with the ships, from
// FX_EYE_X m along the line and FX_EYE_Y m toward it. FX_BG=light: a flat ground of that light, to measure opacity. FX_NO_SHIPS=1 leaves
// the hulls out. Each moment falls a little before a frame, as a game's does. The fate shots take a moving ship too:
// FX_SHIP=x,y,z is her velocity in m/s (12.9 is 25 knots), FX_SHIP_STOP=s stops her dead, FX_SHIP_STAY=1 holds the eye.
//
// The sea battle's own shots of release 2 (seafx), named screen... or splash...: "screen", FX_SCREENS escorts (1),
// FX_SHIP_LONG m (110) long, FX_GAP m (400) apart away from the eye and FX_STAGGER m (300) further along, steam along +X
// at FX_KNOTS (25) laying smoke screens from her stern from 0 s (ScreenLay; FX_BLACK=1 from the funnels, alt every
// other one) until FX_STOP s; the wind FX_WIND_X, FX_WIND_Y (0.8, 3: across her track, away from the eye); the eye
// FX_DIST m (5000) abeam, FX_EYE_H m (24) up, looking at the middle of what is laid by then; FX_FUNNEL=funnel: their
// funnels smoke too. "splash": FX_SPLASHES shells (1) of FX_CAL mm (406) dyed FX_DYE (none) fall FX_SHORT m (120)
// short of a ship FX_DIST m (1000) off, FX_SPREAD m (90) apart, from FX_FALL_AT s (1) and FX_EVERY s (0.3) apart
// (SeaSplash); FX_OLD=bomb500 (or another size): the call the scene made before, Shell on water. FX_SUN, FX_DUSK, FX_BG,
// FX_NO_SHIPS as in the sea shot; FX_STEP=n draws the frames between n at a time.
//
// The raid shot (bomb carpets from the air scene's wide view, BombStick): FX_STICKS bombers (22) in boxes of six,
// FX_BOX_GAP s (12) apart, each dropping a stick of FX_BOMBS (11) FX_SIZE (bomb500) FX_STICK_M m (300) long along +X,
// over FX_OVER s (under 0: the kit's own), the first at FX_DROP_AT s (0.5). The eye stands FX_EYE_H m (4700) up and
// FX_DIST m (11000) from the carpets' middle, from FX_FROM degrees round (0: the bombers cross the view), with an
// FX_LENS degree lens (55), in a light haze FX_HAZE_KM (60) on the ground only; the scene's draw range is FX_BEYOND
// (the recipe's air.draw_beyond_m). FX_BACK_MPS: the eye draws back along its line of sight that fast, so the
// carpets cross the draw range (the hand-over). FX_REF=1: the same bombs as plain Shell calls at the moments and
// places the kit gives them, for the proof that a recipe with no far row draws the same either way. It prints the
// most puffs laid in any frame.
//
// The drive shot (the desert dust): a column of FX_VEHICLES (4) tanks, FX_GAP m (30) apart, driving along +X at
// FX_SPEED m/s (10) on a stony desert ("sand"), FX_RUN s (12) before the first frame, each laying dust from its two
// rear sockets (FX_WHEELS=1: wheeled). "eye" is a man's eye, 1.7 m up, FX_AHEAD m (70) ahead of the lead and FX_OFF m
// (10) off the line; "side" FX_OFF m (70) abeam; "high" from 30 m up; "top" the overview, FX_DIST m (150) off at FX_DOWN
// degrees (50) down. FX_FIRE=1: the lead's gun fires at 0 s (again
// every FX_EVERY s). "gun": a towed anti-tank gun firing along +X at 0 s, seen at a man's eye FX_AHEAD m (45) down its
// line of fire and FX_OFF m (6) off it. The wind is FX_WIND_X, FX_WIND_Y m/s (1.6, 0.4, the game's own). Each frame prints how much of
// each vehicle (or the gun) still shows through the dust in front of it.
//
// The oil shot: one oil fire on a flat sea (FX_REACH metres of oil, 100; seen from FX_DIST metres off, 1200, at
// FX_EYE_H metres, 8, looking at FX_LOOK_H metres up its column, 150), or "horizon": three fires 3, 6 and 9 km off.
// Frames count from when it was lit; FX_SINCE=s opens with it already burning that long. The wind is FX_WIND_X,
// FX_WIND_Y metres a second (3, 0.8). FX_DUSK=1 dims the sea and the smoke's light, so the fire's lamp shows.
//
// The fate shots (the looks of a hit's outcome), each called on the frame its moment falls in. "steam": a vent on a
// ship's deck FX_UP m (8) over a flat sea lets go at FX_AT s (5): FX_STEAM_KIND (vent, funnel), FX_ACROSS m (2),
// FX_SECONDS (25), FX_WAY=x,y,z (up), beside a ship fire FX_BURN m across (12; 0: none) FX_ASIDE m (30) to the right.
// "out": a fire FX_BURN m across lit at 0 and put out from FX_AT s (20), FX_HOW water or extinguisher, over FX_OVER s
// (0: the recipe's); on the sea, or with FX_LAND=1 on the field (a tank's, 2 m). "cook": a tank's hatch FX_UP m (2.4)
// over the field: FX_POPS (7) over FX_OVER s (10), the jet FX_JET s (5) from FX_JET_FROM s (-1: after the last pop),
// the hatch FX_ACROSS m (0.6). "glance": an FX_MM (75) shot glancing off a plate FX_UP m (1.8) up; FX_IN its way in,
// FX_N the plate's way out; FX_GLANCES=n shots FX_GAP s apart. The eye: FX_DIST m off (300 at sea, 18.75 on land, the
// battle camera's), FX_EYE_H m up, looking FX_LOOK_H m up. Two men of 1.75 m stand by on land.
//
// The wreck shot: a knocked-out heavy tank (a box hull, 6.4 m long) burning, the kit called just as the ground scene
// calls it for the tank's knocked_out clip and then its burn loop: the hatch's cook-off at 0.35 s, a burst of smoke at
// 0.5 s, the engine's fire at 1.2 s and its smoke at 1.6 s, then from 3 s four fires of 2.4 m (engine deck, its left
// side, the turret roof, the front left) and the engine's smoke again (FX_SCRIPT=one: the engine's fire alone). The
// hull is lit by the sun and by the kit's lamps, plate by plate, so the fire's glow shows on it. "side" is a man's eye
// 16 m off her burning side, "battle" the battle camera's, "far" 60 m off, "tall" 160 m off for the whole column;
// FX_DIST, FX_EYE_H, FX_LOOK_H, FX_LENS (42) move it. FX_DUSK=1 as the others. Each frame prints the lamps lit.
//
// The air shot: a fighter at 1,500 m and 150 m/s in a banked turn (FX_TURN=degrees a second, 3 by default) fires its
// wing guns, laid to meet 300 m ahead, from the first frame; AirGun is called sixty times a second as the game would.
// FX_DUSK=1 darkens the sky and the smoke to dusk. The hit and trail shots turn it round: the aircraft is struck by
// that kind of gun along its wing and nose from 0.2 s to 1 s, and its engine trails FX_TRAIL (smoke, fire, glycol)
// from 0.3 s; hit looks from the side, trail from above and behind, wake at the whole trail from FX_DIST m off the beam
// (2500). FX_SPAN=metres gives TrailStart the aircraft's span (a fire's flame is sized by it); FX_FIRE_X=metres forward
// of the middle puts the trail's point (2.9, the cowl; 1, the wing root). map is the strategy
// camera: the guns firing seen from FX_DIST metres (1400) high abeam, where flashes are held at their pixel floor.
//
// It draws what the game's mesh would carry: the same puffs, in the same order, with the same colours and alphas,
// laid over a plain field (grass, a gentle swell, a sky) with five men of 1.75 m standing round the hit. The flash's
// lamp lifts the ground round it as the game's lamp would. Unreal's tone curve is not copied: the colours here are
// the recipe's picture colours put straight on the screen.

#include "Effects/TheaterFxCore.h"
#include "Battle/LandBurn.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using TheaterFx::Battle;
using TheaterFx::Rgb;
using TheaterFx::Vec;

namespace
{
	class FlatRecipe : public TheaterFx::Recipe
	{
	public:
		std::map<std::string, double> Numbers;
		std::map<std::string, std::string> Words;
		bool Find(const char* Path, double& Out) const override
		{
			auto It = Numbers.find(Path);
			if (It == Numbers.end())
			{
				return false;
			}
			Out = It->second;
			return true;
		}
		bool FindWord(const char* Path, std::string& Out) const override
		{
			auto It = Words.find(Path);
			if (It == Words.end())
			{
				return false;
			}
			Out = It->second;
			return true;
		}
	};

	double Swell(void*, double X, double Y)
	{
		return 0.35 * std::sin(X * 0.045) * std::cos(Y * 0.037) + 0.15 * std::sin((X + Y) * 0.11);
	}

	double ToLinear(double C255)
	{
		const double C = C255 / 255.0;
		return C <= 0.04045 ? C / 12.92 : std::pow((C + 0.055) / 1.055, 2.4);
	}

	double ToPicture(double L)
	{
		L = std::max(0.0, std::min(1.0, L));
		return 255.0 * (L <= 0.0031308 ? L * 12.92 : 1.055 * std::pow(L, 1.0 / 2.4) - 0.055);
	}

	struct Canvas
	{
		int W = 960, H = 540;
		std::vector<double> R, G, B, Depth;
		std::vector<double> Seen; // (how much of what is solid at a pixel still shows through the puffs in front of it)
		std::vector<int> Owner; // (which solid thing a pixel shows: a vehicle's number, or -1)
		void Init(int InW, int InH)
		{
			W = InW;
			H = InH;
			R.assign(static_cast<size_t>(W * H), 0.0);
			G = R;
			B = R;
			Depth.assign(static_cast<size_t>(W * H), 1e30);
			Seen.assign(static_cast<size_t>(W * H), 1.0);
			Owner.assign(static_cast<size_t>(W * H), -1);
		}
		void Save(const std::string& Path) const
		{
			std::ofstream F(Path, std::ios::binary);
			F << "P6\n" << W << " " << H << "\n255\n";
			for (size_t I = 0; I < R.size(); ++I)
			{
				const unsigned char Px[3] = { static_cast<unsigned char>(ToPicture(R[I])), static_cast<unsigned char>(ToPicture(G[I])), static_cast<unsigned char>(ToPicture(B[I])) };
				F.write(reinterpret_cast<const char*>(Px), 3);
			}
			if (std::getenv("FX_FLOAT"))
			{
				// (FX_FLOAT=1: the picture's linear light too, three 32-bit floats a pixel in <name>.f32, unclipped, so
				// that ../game_curve.py can show the frame as the game's own tone curve and grade would)
				std::ofstream Raw(Path + ".f32", std::ios::binary);
				for (size_t I = 0; I < R.size(); ++I)
				{
					const float Px[3] = { static_cast<float>(R[I]), static_cast<float>(G[I]), static_cast<float>(B[I]) };
					Raw.write(reinterpret_cast<const char*>(Px), sizeof(Px));
				}
			}
		}
	};

	struct Camera
	{
		Vec Eye, Right, Up, Forward;
		double Ppr = 1000.0;
		int W = 960, H = 540;
		bool Project(const Vec& P, double& Sx, double& Sy, double& Z) const
		{
			const Vec D = P - Eye;
			Z = D.Dot(Forward);
			if (Z < 0.2)
			{
				return false;
			}
			Sx = W * 0.5 + D.Dot(Right) / Z * Ppr;
			Sy = H * 0.5 - D.Dot(Up) / Z * Ppr;
			return true;
		}
		Vec Ray(double Sx, double Sy) const
		{
			return (Forward + Right * ((Sx - W * 0.5) / Ppr) + Up * ((H * 0.5 - Sy) / Ppr)).Unit();
		}
	};

	double Noise(double X, double Y)
	{
		const double A = std::sin(X * 0.73 + std::sin(Y * 0.31) * 2.0) + std::sin(Y * 0.57 + std::sin(X * 0.23) * 2.0);
		const double B = std::sin(X * 3.1 + Y * 1.7) * std::sin(Y * 2.9 - X * 1.3);
		return 0.5 + 0.3 * A * 0.5 + 0.2 * B;
	}

	// (FX_SKY=r,g,b: the sky or haze behind the smoke as the game's screen shows it, told to the kit (SetSky); without
	// it the kit is told nothing and old smoke pales to the recipe's own grey)
	void SkyFromEnv(Battle& Fx)
	{
		const char* Is = std::getenv("FX_SKY");
		double SkyR = 0.0, SkyG = 0.0, SkyB = 0.0;
		if (Is && std::sscanf(Is, "%lf,%lf,%lf", &SkyR, &SkyG, &SkyB) == 3)
		{
			Fx.SetSky(SkyR, SkyG, SkyB);
		}
	}

	// (FX_GROUND=r,g,b: the ground as the game's screen shows it, the same everywhere; without it the game cannot say)
	bool GroundPaint(void*, double, double, double& R, double& G, double& B)
	{
		const char* Is = std::getenv("FX_GROUND");
		return Is && std::sscanf(Is, "%lf,%lf,%lf", &R, &G, &B) == 3;
	}

	// (FX_DIM=k: the land and the sky that much darker, a dusk to judge a lamp on; the kit is told the same light)
	double DimK = 1.0;

	void Paint(Canvas& C, const Camera& Cam, const Battle::Lamp* Lamps, int NLamps, const std::vector<Vec>& Men, bool bSnow, bool bSea = false, bool bSand = false, size_t DarkFrom = static_cast<size_t>(-1))
	{
		// (grass, or a snowfield under a snow hit, so that a white skirt is judged on white; or the open sea, flat, for
		// the oil fires, its haze drawn out to kilometres; or a stony desert, for the desert dust)
		double GrassA[3] = { ToLinear(bSand ? 192 : bSea ? 58 : bSnow ? 226 : 124), ToLinear(bSand ? 166 : bSea ? 80 : bSnow ? 230 : 132), ToLinear(bSand ? 124 : bSea ? 98 : bSnow ? 236 : 74) };
		double GrassB[3] = { ToLinear(bSand ? 158 : bSea ? 44 : bSnow ? 196 : 98), ToLinear(bSand ? 132 : bSea ? 64 : bSnow ? 202 : 96), ToLinear(bSand ? 100 : bSea ? 82 : bSnow ? 212 : 60) };
		// (FX_GROUND=r,g,b: the land painted that colour, a little lighter and darker in patches; the kit is told the
		// same colour as the game's screen showing its ground, see GroundPaint)
		// (FX_GROUND_RAW=r,g,b: what the land is drawn as instead: with FX_FLOAT=1 and game_curve.py show, the colour
		// that lands on FX_GROUND, from game_curve.py write)
		double Painted[3];
		const char* ShownIs = std::getenv("FX_GROUND_RAW");
		const bool bShown = ShownIs && std::sscanf(ShownIs, "%lf,%lf,%lf", &Painted[0], &Painted[1], &Painted[2]) == 3;
		if (!bSea && (bShown || GroundPaint(nullptr, 0.0, 0.0, Painted[0], Painted[1], Painted[2])))
		{
			for (int K = 0; K < 3; ++K)
			{
				GrassA[K] = ToLinear(Painted[K]) * 1.12;
				GrassB[K] = ToLinear(Painted[K]) * 0.88;
			}
		}
		const double Haze[3] = { ToLinear(206), ToLinear(214), ToLinear(222) };
		// (FX_BACK=r,g,b: one flat colour behind everything, ground and sky, and no men: the same frame on black and on
		// white gives every pixel's opacity and the smoke's own colour, see tools/battle-fx/own_colour.py)
		double FlatBack[3];
		const char* BackIs = std::getenv("FX_BACK");
		const bool bFlatBack = BackIs && std::sscanf(BackIs, "%lf,%lf,%lf", &FlatBack[0], &FlatBack[1], &FlatBack[2]) == 3;
		for (int Y = 0; Y < C.H; ++Y)
		{
			for (int X = 0; X < C.W; ++X)
			{
				const size_t I = static_cast<size_t>(Y * C.W + X);
				const Vec D = Cam.Ray(X + 0.5, Y + 0.5);
				double Col[3];
				if (D.Z < -1e-4)
				{
					// march to the ground: plane first, then two corrections for the swell
					double T = -Cam.Eye.Z / D.Z;
					for (int K = 0; K < 3 && !bSea; ++K)
					{
						const Vec P = Cam.Eye + D * T;
						T = (Swell(nullptr, P.X, P.Y) - Cam.Eye.Z) / D.Z;
					}
					const Vec P = Cam.Eye + D * T;
					const double N = bSea ? Noise(P.X * 0.02, P.Y * 0.08) * 0.6 + Noise(P.X * 0.3, P.Y * 0.9) * 0.4 : Noise(P.X * 0.35, P.Y * 0.35) * 0.7 + Noise(P.X * 3.0, P.Y * 3.0) * 0.3;
					for (int K = 0; K < 3; ++K)
					{
						Col[K] = (GrassA[K] + (GrassB[K] - GrassA[K]) * N) * DimK;
					}
					double Lift[3] = { 1.0, 1.0, 1.0 };
					for (int L = 0; L < NLamps; ++L)
					{
						const double Dist = (P - Lamps[L].At).Length();
						if (Dist < Lamps[L].Radius)
						{
							// (as the game's lamp: inverse square, windowed to nothing at its radius; 64 puts the passed lift of
							// 1.24x on the ground 3 m from a 105, whose lamp hangs 16 m up)
							const double Window = 1.0 - std::pow(Dist / Lamps[L].Radius, 4.0);
							const double F = Lamps[L].Strength * 64.0 / std::max(Dist * Dist, 1.0) * Window * Window;
							Lift[0] += F * Lamps[L].Col.R / std::max(0.01f, Lamps[L].Col.G);
							Lift[1] += F;
							Lift[2] += F * Lamps[L].Col.B / std::max(0.01f, Lamps[L].Col.G);
						}
					}
					const double Fog = 1.0 - std::exp(-std::max(0.0, T - 60.0) / (bSea ? 6000.0 : 700.0));
					for (int K = 0; K < 3; ++K)
					{
						Col[K] = Col[K] * Lift[K] * (1.0 - Fog) + Haze[K] * DimK * Fog;
					}
					C.Depth[I] = (D * T).Dot(Cam.Forward);
				}
				else
				{
					const double Up = std::min(1.0, D.Z * 4.0);
					const double Top[3] = { ToLinear(150), ToLinear(178), ToLinear(214) };
					for (int K = 0; K < 3; ++K)
					{
						Col[K] = (Haze[K] + (Top[K] - Haze[K]) * Up) * DimK;
					}
				}
				C.R[I] = bFlatBack ? ToLinear(FlatBack[0]) : Col[0];
				C.G[I] = bFlatBack ? ToLinear(FlatBack[1]) : Col[1];
				C.B[I] = bFlatBack ? ToLinear(FlatBack[2]) : Col[2];
			}
		}
		// the men, 1.75 m: a pale figure, as the white soldiers in the Blender stills
		for (size_t Man = 0; Man < (bFlatBack ? static_cast<size_t>(0) : Men.size()); ++Man)
		{
			const Vec& M = Men[Man];
			const double Paint_ = Man >= DarkFrom ? 0.2 : 1.0; // (the gun's own crew in field grey, to read against pale ground)
			double Sx0, Sy0, Z0, Sx1, Sy1, Z1;
			if (!Cam.Project(M, Sx0, Sy0, Z0) || !Cam.Project(M + Vec(0, 0, 1.75), Sx1, Sy1, Z1))
			{
				continue;
			}
			const double Tall = Sy0 - Sy1;
			const double Wide = std::max(1.0, Tall * 0.26);
			for (int Y = static_cast<int>(Sy1); Y <= static_cast<int>(Sy0); ++Y)
			{
				const double F = (Y - Sy1) / std::max(Tall, 1.0);
				const double Half = F < 0.14 ? Wide * 0.32 : (F < 0.55 ? Wide * 0.5 : Wide * 0.36);
				for (int X = static_cast<int>(Sx0 - Half); X <= static_cast<int>(Sx0 + Half); ++X)
				{
					if (X < 0 || Y < 0 || X >= C.W || Y >= C.H)
					{
						continue;
					}
					const size_t I = static_cast<size_t>(Y * C.W + X);
					if (Z0 < C.Depth[I])
					{
						const double Shade = (X < Sx0 ? 0.62 : 0.78) * Paint_;
						C.R[I] = C.G[I] = C.B[I] = Shade;
						C.Depth[I] = Z0;
					}
				}
			}
		}
	}

	double PictureGain = 1.0; // (what the game's material multiplies the kit's colours by: Battle::PictureGain)
	bool GammaBytes = false; // (while Puffs paints the kit's puffs whose bytes are raised to 1/2.2: Battle::SrgbBytes, as M_BattlePuffs reads them)
	// (FX_BYTES=srgb: the kit writes its colours as the game does when it draws with M_BattlePuffs, raised to 1/2.2 so a
	// dark smoke keeps its hue; unset or linear: plain bytes, as with M_Highlight, and every picture as it was)
	bool GammaWanted()
	{
		const char* Is = std::getenv("FX_BYTES");
		return Is && std::string(Is) == "srgb";
	}

	// (FX_LAND_GRADE=1: the kit's colours as written for the land battle's grade, Battle::SetLandGrade; see the frames
	// through that grade with land_grade.py, not game_curve.py)
	bool LandGradeWanted()
	{
		const char* Is = std::getenv("FX_LAND_GRADE");
		return Is && std::string(Is) == "1";
	}

	// (FX_SOFT_CONTACT=metres: the land battle's soft contact, FBattleEffects::SetSoftContact. The kit lays its lying
	// pieces nearer the eye, and here, as M_BattlePuffsSoft does, every piece fades where it comes within that depth in
	// front of the ground, a box or a man)
	double SoftWanted()
	{
		const char* Is = std::getenv("FX_SOFT_CONTACT");
		const double M = Is ? std::atof(Is) : 0.0;
		return M > 0.0 && M < 100.0 ? M : 0.0;
	}
	const double SoftFadeM = SoftWanted();

	void Triangle(Canvas& C, const Camera& Cam, const Vec P[3], const unsigned char* Rgba[3])
	{
		double Sx[3], Sy[3], Z[3];
		for (int K = 0; K < 3; ++K)
		{
			if (!Cam.Project(P[K], Sx[K], Sy[K], Z[K]))
			{
				return;
			}
		}
		const double Area = (Sx[1] - Sx[0]) * (Sy[2] - Sy[0]) - (Sx[2] - Sx[0]) * (Sy[1] - Sy[0]);
		if (!(std::fabs(Area) >= 1e-9) || !std::isfinite(Area))
		{
			return; // (no area, or a corner that is no number: an aircraft of a span without end draws nothing)
		}
		// (a corner a thousand million pixels off, a ship at a wild speed: held before it is made a whole number)
		auto Whole = [](double Is) { return static_cast<int>(std::clamp(Is, -1.0e9, 1.0e9)); };
		const int X0 = std::max(0, Whole(std::floor(std::min({ Sx[0], Sx[1], Sx[2] }))));
		const int X1 = std::min(C.W - 1, Whole(std::ceil(std::max({ Sx[0], Sx[1], Sx[2] }))));
		const int Y0 = std::max(0, Whole(std::floor(std::min({ Sy[0], Sy[1], Sy[2] }))));
		const int Y1 = std::min(C.H - 1, Whole(std::ceil(std::max({ Sy[0], Sy[1], Sy[2] }))));
		for (int Y = Y0; Y <= Y1; ++Y)
		{
			for (int X = X0; X <= X1; ++X)
			{
				const double Px = X + 0.5, Py = Y + 0.5;
				const double W0 = ((Sx[1] - Px) * (Sy[2] - Py) - (Sx[2] - Px) * (Sy[1] - Py)) / Area;
				const double W1 = ((Sx[2] - Px) * (Sy[0] - Py) - (Sx[0] - Px) * (Sy[2] - Py)) / Area;
				const double W2 = 1.0 - W0 - W1;
				if (W0 < 0 || W1 < 0 || W2 < 0)
				{
					continue;
				}
				const size_t I = static_cast<size_t>(Y * C.W + X);
				const double Deep = W0 * Z[0] + W1 * Z[1] + W2 * Z[2];
				if (Deep > C.Depth[I])
				{
					continue; // (behind the ground or a man: as Unreal's depth test for see-through things)
				}
				double A = (W0 * Rgba[0][3] + W1 * Rgba[1][3] + W2 * Rgba[2][3]) / 255.0;
				if (SoftFadeM > 0.0)
				{
					A *= std::clamp((C.Depth[I] - Deep) / SoftFadeM, 0.0, 1.0);
				}
				if (A <= 0.0)
				{
					continue;
				}
				// (the material reads the bytes as the GPU blends them across the triangle, and M_BattlePuffs then raises
				// that to 2.2)
				auto Read = [](double Blended) { return GammaBytes ? std::pow(Blended, 2.2) : Blended; };
				const double Cr = Read((W0 * Rgba[0][0] + W1 * Rgba[1][0] + W2 * Rgba[2][0]) / 255.0) * PictureGain;
				const double Cg = Read((W0 * Rgba[0][1] + W1 * Rgba[1][1] + W2 * Rgba[2][1]) / 255.0) * PictureGain;
				const double Cb = Read((W0 * Rgba[0][2] + W1 * Rgba[1][2] + W2 * Rgba[2][2]) / 255.0) * PictureGain;
				C.R[I] = C.R[I] * (1.0 - A) + Cr * A;
				C.G[I] = C.G[I] * (1.0 - A) + Cg * A;
				C.B[I] = C.B[I] * (1.0 - A) + Cb * A;
				C.Seen[I] *= 1.0 - std::min(1.0, A);
			}
		}
	}

	/** A flat-coloured triangle that hides what is behind it (the aircraft in the air shot). */
	void Solid(Canvas& C, const Camera& Cam, const Vec P[3], const double Col[3], int Who = -1)
	{
		double Sx[3], Sy[3], Z[3];
		for (int K = 0; K < 3; ++K)
		{
			if (!Cam.Project(P[K], Sx[K], Sy[K], Z[K]) || !std::isfinite(Sx[K]) || !std::isfinite(Sy[K]))
			{
				return; // (the preview's own shapes, under a dial that is not a number: nothing drawn. The kit's fans are not excused: see Triangle)
			}
		}
		const double Area = (Sx[1] - Sx[0]) * (Sy[2] - Sy[0]) - (Sx[2] - Sx[0]) * (Sy[1] - Sy[0]);
		if (!(std::fabs(Area) >= 1e-9) || !std::isfinite(Area))
		{
			return; // (no area, or a corner that is no number: an aircraft of a span without end draws nothing)
		}
		// (a corner a thousand million pixels off, a ship at a wild speed: held before it is made a whole number)
		auto Whole = [](double Is) { return static_cast<int>(std::clamp(Is, -1.0e9, 1.0e9)); };
		const int X0 = std::max(0, Whole(std::floor(std::min({ Sx[0], Sx[1], Sx[2] }))));
		const int X1 = std::min(C.W - 1, Whole(std::ceil(std::max({ Sx[0], Sx[1], Sx[2] }))));
		const int Y0 = std::max(0, Whole(std::floor(std::min({ Sy[0], Sy[1], Sy[2] }))));
		const int Y1 = std::min(C.H - 1, Whole(std::ceil(std::max({ Sy[0], Sy[1], Sy[2] }))));
		for (int Y = Y0; Y <= Y1; ++Y)
		{
			for (int X = X0; X <= X1; ++X)
			{
				const double Px = X + 0.5, Py = Y + 0.5;
				const double W0 = ((Sx[1] - Px) * (Sy[2] - Py) - (Sx[2] - Px) * (Sy[1] - Py)) / Area;
				const double W1 = ((Sx[2] - Px) * (Sy[0] - Py) - (Sx[0] - Px) * (Sy[2] - Py)) / Area;
				const double W2 = 1.0 - W0 - W1;
				if (W0 < 0 || W1 < 0 || W2 < 0)
				{
					continue;
				}
				const size_t I = static_cast<size_t>(Y * C.W + X);
				const double Deep = W0 * Z[0] + W1 * Z[1] + W2 * Z[2];
				if (Deep > C.Depth[I])
				{
					continue;
				}
				C.R[I] = Col[0];
				C.G[I] = Col[1];
				C.B[I] = Col[2];
				C.Depth[I] = Deep;
				C.Owner[I] = Who;
			}
		}
	}

	void Puffs(Canvas& C, const Camera& Cam, const Battle& Fx, int N)
	{
		const std::vector<Vec>& P = Fx.Points();
		const std::vector<uint8_t>& Rgba = Fx.Colours();
		const int Rim = Fx.Ring();
		PictureGain = Fx.PictureGain();
		GammaBytes = Fx.SrgbBytes();
		for (int Each = 0; Each < N; ++Each)
		{
			const int First = Each * Fx.PerPuff();
			auto Tri = [&](int A, int B, int Cc)
			{
				const Vec Pts[3] = { P[static_cast<size_t>(A)], P[static_cast<size_t>(B)], P[static_cast<size_t>(Cc)] };
				const unsigned char* Cols[3] = { &Rgba[static_cast<size_t>(A) * 4], &Rgba[static_cast<size_t>(B) * 4], &Rgba[static_cast<size_t>(Cc) * 4] };
				Triangle(C, Cam, Pts, Cols);
			};
			for (int K = 0; K < Rim; ++K)
			{
				const int Next = (K + 1) % Rim;
				const int In0 = First + 1 + K, In1 = First + 1 + Next, Out0 = First + 1 + Rim + K, Out1 = First + 1 + Rim + Next;
				Tri(First, In1, In0);
				Tri(In0, In1, Out1);
				Tri(In0, Out1, Out0);
			}
		}
		GammaBytes = false;
	}
}

namespace
{
	double FlatSea(void*, double, double) { return 0.0; }

	/** The oil fires (see the top of the file). */
	int Oil(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		Fx.Setup(&Book, &FlatSea, nullptr, 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 3.0), EnvNum("FX_WIND_Y", 0.8), 0.0));
		const double Since = EnvNum("FX_SINCE", 0.0);
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0)));
		if (Shot == "horizon")
		{
			// (three fires far off, as the sea battle's low camera sees the ships lost: two slicks and one ship afire)
			Fx.OilFire(0.0, 1, Vec(-900.0, 3000.0, 0.0), 110.0, "sunk", 0.0, Since);
			Fx.OilFire(0.0, 2, Vec(1400.0, 6000.0, 0.0), 80.0, "sunk", 0.0, Since);
			Fx.OilFire(0.0, 3, Vec(2900.0, 9000.0, 0.0), 70.0, "burning", 0.0, Since);
			Cam.Eye = Vec(0.0, 0.0, EnvNum("FX_EYE_H", 8.0));
			Cam.Forward = Vec(0.0, 1.0, 0.06).Unit();
		}
		else
		{
			// (one fire, Shot its kind, FX_REACH metres of oil, seen from FX_DIST metres off at FX_EYE_H metres)
			const double Reach = EnvNum("FX_REACH", 100.0), Dist = EnvNum("FX_DIST", 1200.0);
			const char* BurnIs = std::getenv("FX_BURN");
			if (BurnIs)
			{
				// (FX_BURN=metres across: a plain fire of that size in place of the oil (Burn, as a ship hit's in the air
				// battle), its base FX_BURN_UP m over the sea. It has no Since: ask for a frame late enough, 450 is 15 s.
				// FX_BURN_ASIDE=m: the oil fire burns too, the plain fire that far aside of it, as a ship on fire by her oil)
				// FX_BURN_ROW=n,m: n such fires in a row m metres apart, as a ship ablaze from end to end (FX_BURN_TURN=degrees
				// turns the row and the oil's place from the view's own line)
				const char* RowIs = std::getenv("FX_BURN_ROW");
				int RowN = 1;
				double RowStep = 0.0;
				if (RowIs)
				{
					std::sscanf(RowIs, "%d,%lf", &RowN, &RowStep);
					RowN = std::clamp(RowN, 1, 12);
				}
				const double RowTurn = EnvNum("FX_BURN_TURN", 0.0) * 3.14159265358979 / 180.0;
				const Vec RowWay(std::cos(RowTurn), std::sin(RowTurn), 0.0);
				for (int I = 0; I < RowN; ++I)
				{
					const Vec Spot = RowWay * (EnvNum("FX_BURN_ASIDE", 0.0) + (I - 0.5 * (RowN - 1)) * RowStep);
					Fx.Burn(0.0, Vec(Spot.X, Spot.Y, EnvNum("FX_BURN_UP", 0.0)), std::atof(BurnIs), 900.0);
				}
				Fx.SetDrawBeyond(Fx.AirBeyond()); // (as the air scene does: UseAirDistance)
			}
			if (!BurnIs || std::getenv("FX_BURN_ASIDE"))
			{
				Fx.OilFire(0.0, 1, Vec(0.0, 0.0, 0.0), Reach, Shot.c_str(), 0.0, Since);
			}
			Cam.Eye = Vec(-0.3, -1.0, 0.0).Unit() * Dist + Vec(0.0, 0.0, EnvNum("FX_EYE_H", 8.0));
			Cam.Forward = (Vec(0.0, 0.0, EnvNum("FX_LOOK_H", 150.0)) - Cam.Eye).Unit();
		}
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		// (FX_KIT_PX=k: the kit is told a picture k times as wide in pixels as the one drawn, as the air scene tells it
		// the window's pixels while Unreal draws fewer and fills the window out: 2.6875 is Brian's 2881 over 1072)
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
		if (!std::getenv("FX_NOCULL")) // (as the game: what is off the screen takes no place. FX_NOCULL: everything in range is laid)
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		if (const char* SunIs = std::getenv("FX_SUN")) // (FX_SUN=x,y,z: the way toward the sun)
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				View.Sun = Vec(X, Y, Z).Unit();
			}
		}
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		if (bDusk)
		{
			View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
			View.SunLight = Rgb(0.0f, 0.0f, 0.0f);
		}
		std::sort(Frames.begin(), Frames.end());
		int Drawn = 0;
		const std::vector<Vec> Nobody;
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; ++Drawn)
			{
				Fx.Draw(Drawn / 30.0, View);
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, Nobody, false, true);
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.22;
					C.G[I] *= 0.2;
					C.B[I] *= 0.26;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/oil-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps\n", Frame, N, NL);
		}
		return 0;
	}

	/** The outcome looks (see the top of the file): steam, a fire put out, a cook-off, a glance. */
	int Fate(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		auto EnvVec = [](const char* Name, const Vec& Default)
		{
			double X = 0.0, Y = 0.0, Z = 0.0;
			const char* Is = std::getenv(Name);
			return Is && std::sscanf(Is, "%lf,%lf,%lf", &X, &Y, &Z) == 3 ? Vec(X, Y, Z) : Default;
		};
		const bool bSea = Shot == "steam" || (Shot == "out" && !std::getenv("FX_LAND"));
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		Fx.Setup(&Book, bSea ? &FlatSea : &Swell, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetGroundColour(&GroundPaint, nullptr);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", bSea ? 3.0 : 1.6), EnvNum("FX_WIND_Y", bSea ? 0.8 : 0.4), 0.0));
		const double GroundZ = bSea ? 0.0 : Swell(nullptr, 0.0, 0.0);
		const double Up = EnvNum("FX_UP", bSea ? 8.0 : (Shot == "out" ? 1.2 : Shot == "glance" ? 1.8 : 2.4));
		const Vec At(0.0, 0.0, GroundZ + Up);
		const double Dist = EnvNum("FX_DIST", bSea ? 300.0 : 18.75), EyeH = EnvNum("FX_EYE_H", bSea ? 40.0 : 6.0), LookH = EnvNum("FX_LOOK_H", bSea ? 18.0 : 3.0);
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0)));
		if (const char* LensIs = std::getenv("FX_LENS"))
		{
			Cam.Ppr = W / (2.0 * std::tan(std::clamp(std::atof(LensIs), 5.0, 170.0) * 3.14159265358979 / 360.0));
		}
		Cam.Eye = Vec(-0.3, -1.0, 0.0).Unit() * Dist + Vec(0.0, 0.0, GroundZ + EyeH);
		Cam.Forward = (Vec(EnvNum("FX_LOOK_X", 0.0), 0.0, GroundZ + LookH) - Cam.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
		if (!std::getenv("FX_NOCULL"))
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		View.Sun = EnvVec("FX_SUN", View.Sun);
		if (std::getenv("FX_SUN"))
		{
			View.Sun = View.Sun.Unit();
		}
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		if (bDusk)
		{
			View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
			View.SunLight = Rgb(0.0f, 0.0f, 0.0f);
		}
		if (bSea)
		{
			Fx.SetDrawBeyond(Fx.AirBeyond());
		}
		// (what happens, and when: each is called on the frame its moment falls in, as a game would call it)
		struct Due
		{
			double At = 0.0;
			int What = 0;
			bool bDone = false;
		};
		std::vector<Due> Script;
		const double BurnAcross = EnvNum("FX_BURN", Shot == "steam" ? 12.0 : bSea ? 10.0 : 2.0);
		const double Aside = EnvNum("FX_ASIDE", Shot == "steam" ? 30.0 : 0.0);
		const Vec FireAt = At + Cam.Right * Aside;
		if (Shot == "steam")
		{
			// (a ship fire burning beside it, unless FX_BURN=0; the steam let go at FX_AT s, 5)
			Script.push_back({ 0.0, 1 });
			Script.push_back({ EnvNum("FX_AT", 5.0), 2 });
		}
		else if (Shot == "out")
		{
			// (a fire lit at 0 and put out from FX_AT s, 20)
			Script.push_back({ 0.0, 1 });
			Script.push_back({ EnvNum("FX_AT", 20.0), 3 });
		}
		else if (Shot == "cook")
		{
			Script.push_back({ EnvNum("FX_AT", 0.5), 4 });
			if (std::getenv("FX_BURN_AT"))
			{
				Script.push_back({ EnvNum("FX_BURN_AT", 0.0), 1 });
			}
		}
		else
		{
			// (FX_GLANCES=n: n shots, FX_GAP s apart)
			for (int I = 0; I < std::max(1, static_cast<int>(EnvNum("FX_GLANCES", 1.0))); ++I)
			{
				Script.push_back({ EnvNum("FX_AT", 0.1) + I * EnvNum("FX_GAP", 0.6), 5 });
			}
		}
		// (FX_SHIP=x,y,z: the thing burning is a ship under way at that velocity, metres a second (12.9 is 25 knots):
		// its fire, vent and cook-off are moved with it every frame, as the sea battle moves them, and the eye rides
		// along beside her. FX_SHIP_STOP=s: she stops dead at that moment. FX_SHIP_STAY=1: the eye stays where it was)
		const bool bShip = std::getenv("FX_SHIP") != nullptr;
		const Vec ShipVel = EnvVec("FX_SHIP", Vec());
		const double ShipStop = EnvNum("FX_SHIP_STOP", 1.0e9);
		auto ShipAt = [&](double Now) { return ShipVel * std::min(std::max(Now, 0.0), ShipStop); };
		auto Play = [&](double Now)
		{
			int Shots = 0;
			for (Due& D : Script)
			{
				if (D.bDone || D.At > Now)
				{
					Shots += D.What == 5 && D.bDone ? 1 : 0;
					continue;
				}
				D.bDone = true;
				switch (D.What)
				{
				case 1:
					if (BurnAcross > 0.0)
					{
						Fx.Burn(D.At, FireAt + ShipAt(D.At), BurnAcross, EnvNum("FX_BURN_S", 900.0), 7, true);
					}
					break;
				case 2:
				{
					const char* Kind = std::getenv("FX_STEAM_KIND");
					Fx.Steam(D.At, 1, At + ShipAt(D.At), EnvVec("FX_WAY", Vec(0.0, 0.0, 1.0)), EnvNum("FX_ACROSS", 2.0), EnvNum("FX_SECONDS", 25.0), Kind ? Kind : "vent");
					break;
				}
				case 3:
				{
					const char* How = std::getenv("FX_HOW");
					Fx.BurnOut(D.At, 7, EnvNum("FX_OVER", 0.0), How ? How : (bSea ? "water" : "extinguisher"));
					break;
				}
				case 4:
					if (bShip)
					{
						Fx.CookOff(D.At, At + ShipAt(D.At), EnvVec("FX_WAY", Vec(0.0, 0.0, 1.0)), EnvNum("FX_ACROSS", 0.6), static_cast<int>(EnvNum("FX_POPS", 7.0)), EnvNum("FX_OVER", 10.0), EnvNum("FX_JET", 5.0), EnvNum("FX_JET_FROM", -1.0), 9);
						break;
					}
					Fx.CookOff(D.At, At, EnvVec("FX_WAY", Vec(0.0, 0.0, 1.0)), EnvNum("FX_ACROSS", 0.6), static_cast<int>(EnvNum("FX_POPS", 7.0)), EnvNum("FX_OVER", 10.0), EnvNum("FX_JET", 5.0), EnvNum("FX_JET_FROM", -1.0));
					break;
				default:
					Fx.Glance(D.At, At + Cam.Right * (0.4 * Shots), EnvVec("FX_IN", Cam.Right * 0.9 + Cam.Forward * 0.3 - Vec(0.0, 0.0, 0.1)), EnvVec("FX_N", (Cam.Forward * -0.8 - Cam.Right * 0.5 + Vec(0.0, 0.0, 0.35)).Unit()), EnvNum("FX_MM", 75.0), "earth");
					++Shots;
					break;
				}
			}
		};
		// (men of 1.75 m for scale on land: one each side of the place, 3 m off)
		std::vector<Vec> Men;
		if (!bSea)
		{
			for (double Side : { -3.0, 3.5 })
			{
				const Vec M = Vec(0.0, 0.0, 0.0) + Cam.Right * Side - Vec(Cam.Forward.X, Cam.Forward.Y, 0.0).Unit() * 1.0;
				Men.push_back(Vec(M.X, M.Y, Swell(nullptr, M.X, M.Y)));
			}
		}
		Fx.Draw(-0.1, View);
		std::sort(Frames.begin(), Frames.end());
		const Vec Eye0 = Cam.Eye;
		const bool bEyeStays = std::getenv("FX_SHIP_STAY") != nullptr;
		auto Sail = [&](double Now)
		{
			if (!bShip)
			{
				return;
			}
			const Vec Moved = ShipAt(Now);
			const Vec Going = Now < ShipStop ? ShipVel : Vec();
			Fx.BurnMove(Now, 7, FireAt + Moved, Going);
			Fx.SteamMove(Now, 1, At + Moved, EnvVec("FX_WAY", Vec(0.0, 0.0, 1.0)), Going);
			Fx.CookOffMove(Now, 9, At + Moved, EnvVec("FX_WAY", Vec(0.0, 0.0, 1.0)), Going);
			if (!bEyeStays)
			{
				Cam.Eye = Eye0 + Moved;
				View.Eye = Cam.Eye;
			}
			// (two men on her deck for scale and to show where she is: 3 m ahead of the place and 3 m astern)
			const Vec Fore = ShipVel.Length() > 1e-6 ? ShipVel.Unit() : Vec(1.0, 0.0, 0.0);
			Men.clear();
			Men.push_back(At + Moved + Fore * 3.0 - Vec(0.0, 0.0, 0.5));
			Men.push_back(At + Moved - Fore * 3.0 - Vec(0.0, 0.0, 0.5));
		};
		int Drawn = 0;
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; ++Drawn)
			{
				Play(Drawn / 30.0);
				Sail(Drawn / 30.0);
				Fx.Draw(Drawn / 30.0, View);
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			Play(Now);
			Sail(Now);
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, Men, false, bSea);
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.22;
					C.G[I] *= 0.2;
					C.B[I] *= 0.26;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/fate-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps\n", Frame, N, NL);
		}
		return 0;
	}

	/** The air shot (see the top of the file). */
	int Air(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Kind, std::vector<int> Frames, int W, int H, const std::string& ShotName)
	{
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		Fx.Setup(&Book, &Swell, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		if (ShotName != "above")
		{
			Fx.SetDrawBeyond(Fx.AirBeyond()); // (as the air scene does: UseAirDistance)
		}
		Fx.SetWind(Vec(1.6, 0.4, 0.0));
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		const char* TurnIs = std::getenv("FX_TURN");
		const double Pi = 3.14159265358979;
		const char* SpeedIs = std::getenv("FX_SPEED"); // (FX_SPEED: metres a second, 150; a heavy bomber cruises at about 80)
		const double Speed = SpeedIs ? std::clamp(std::atof(SpeedIs), 20.0, 400.0) : 150.0, Turn = (TurnIs ? std::atof(TurnIs) : 3.0) * Pi / 180.0, Height = 1500.0;
		const char* PlaneIs = std::getenv("FX_PLANE"); // (FX_PLANE: the plain shape that many times as big, 2.8 about a heavy bomber)
		const double PlaneK = PlaneIs ? std::clamp(std::atof(PlaneIs), 0.2, 10.0) : 1.0;
		const Vec ZUp(0.0, 0.0, 1.0);
		struct Frame
		{
			Vec At, Fw, Up, Right;
		};
		auto Fly = [&](double T)
		{
			// (a level turn to the left: the nose swings at Turn, the wings banked as a true turn needs)
			Frame F;
			const double Psi = Turn * T;
			const double R = Turn > 1e-6 ? Speed / Turn : 0.0;
			F.At = Turn > 1e-6 ? Vec(R * std::sin(Psi), R * (1.0 - std::cos(Psi)), Height) : Vec(Speed * T, 0.0, Height);
			F.Fw = Vec(std::cos(Psi), std::sin(Psi), 0.0);
			const double Bank = std::atan(Speed * Turn / 9.81);
			const Vec Left = ZUp.Cross(F.Fw);
			F.Up = (ZUp * std::cos(Bank) + Left * std::sin(Bank)).Unit();
			F.Right = F.Fw.Cross(F.Up).Unit();
			return F;
		};
		// the guns, in the aircraft's own frame (forward, right, up), metres
		std::vector<Vec> Guns;
		auto Pair = [&](double Fwd, double Out, double Up_)
		{
			Guns.push_back(Vec(Fwd, Out, Up_));
			Guns.push_back(Vec(Fwd, -Out, Up_));
		};
		if (Kind == "cannon20")
		{
			Pair(1.7, 2.0, -0.32);
			Pair(1.6, 2.6, -0.32);
		}
		else if (Kind == "rifle")
		{
			for (double Out : { 2.4, 2.8, 3.2, 3.6 })
			{
				Pair(1.2, Out, -0.3);
			}
		}
		else
		{
			for (double Out : { 2.3, 2.75, 3.2 })
			{
				Pair(1.25, Out, -0.3);
			}
		}
		auto Place = [](const Frame& F, const Vec& L) { return F.At + F.Fw * L.X + F.Right * L.Y + F.Up * L.Z; };
		auto Level_ = [](const Frame& F) { return Vec(F.Fw.X, F.Fw.Y, 0.0).Unit(); };
		auto Right_ = [&](const Frame& F) { return Vec(F.Fw.X, F.Fw.Y, 0.0).Unit().Cross(ZUp).Unit(); };
		const double Meet = 300.0;
		// ("quarter" and "beam": an engine's trail and no hits, seen from the rear quarter above and from off the beam;
		// FX_ENGINE=x,y,z where the engine is on the aircraft (forward, right, up; metres), FX_TRAIL2 the kind it
		// changes to at FX_TRAIL2_AT seconds, FX_TRAIL3 and FX_TRAIL3_AT a second change)
		// "pass": the same trail seen by an eye that hangs still beside the path, where the aircraft was at FX_PASS_AT
		// seconds (0.6), FX_PASS_OFF metres off the path (4) and FX_PASS_UP above it (1), looking after the aircraft, so
		// its trail runs past the lens; FX_PASS_RIDE=1: the eye rides FX_PASS_BACK metres behind the aircraft instead (25)
		const bool bEngineShot = ShotName == "quarter" || ShotName == "beam" || ShotName == "pass";
		const bool bStruck = ShotName == "hit" || ShotName == "trail" || ShotName == "wake" || bEngineShot;
		// "fall": the aircraft falls from 0.5 s, FX_FALL its way (burn, spin, explode), shedding its debris row into a
		// deck FX_DECK metres below (400); seen from FX_DIST metres off the beam (140). FX_SPAN its span (11), FX_LOOK the
		// event's look (0 to 3), FX_BOMBS=1 bombs aboard, FX_STAY=1 the eye stays on an explosion's cloud
		const bool bFall = ShotName == "fall";
		const char* FallIs = std::getenv("FX_FALL");
		const std::string FallKind = FallIs ? FallIs : "burn";
		const double FallFrom = 0.5;
		auto Fall = [&](double T)
		{
			if (!bFall || T <= FallFrom)
			{
				return Fly(T);
			}
			const Frame F0 = Fly(FallFrom);
			const Vec L0 = Vec(F0.Fw.X, F0.Fw.Y, 0.0).Unit();
			const Vec R0 = L0.Cross(ZUp).Unit();
			const double U = T - FallFrom;
			Frame F;
			if (FallKind == "explode")
			{
				return Fly(T); // (gone, not drawn: its path only gives the moment's velocity)
			}
			if (FallKind == "spin")
			{
				// (a spin: nose well down, turning about a steep line through the air, sinking at about 45 m/s)
				const double Om = 2.0 * Pi / 2.8, Rr = 18.0, Settle = std::min(1.0, U / 2.0);
				const double A = Om * U;
				const Vec Mid = F0.At + L0 * (Speed * 1.2 * (1.0 - std::exp(-U))) - R0 * Rr;
				F.At = Mid + (R0 * std::cos(A) + L0 * std::sin(A)) * (Rr * Settle) - ZUp * (45.0 * U - 22.0 * (1.0 - std::exp(-U)));
				const Vec Round = (L0 * std::cos(A) - R0 * std::sin(A)).Unit();
				const double Nose = 1.05 * Settle;
				F.Fw = (Round * std::cos(Nose) - ZUp * std::sin(Nose)).Unit();
				F.Up = (ZUp * std::cos(Nose) + Round * std::sin(Nose)).Unit();
				const double Roll = 2.5 * A;
				const Vec Side = F.Fw.Cross(F.Up).Unit();
				F.Up = (F.Up * std::cos(Roll) + Side * std::sin(Roll)).Unit();
				F.Right = F.Fw.Cross(F.Up).Unit();
				return F;
			}
			// (burning: the nose drops into a steepening dive and the speed builds, rolling slowly)
			Vec At = F0.At;
			const double Dt = 1.0 / 120.0;
			double Th = 0.0, Sp = Speed;
			for (double Tau = 0.0; Tau < U; Tau += Dt)
			{
				Th = std::min(1.0, 0.22 * Tau);
				Sp = Speed + 9.0 * Tau;
				At = At + (L0 * std::cos(Th) - ZUp * std::sin(Th)) * (Sp * std::min(Dt, U - Tau));
			}
			F.At = At;
			F.Fw = (L0 * std::cos(Th) - ZUp * std::sin(Th)).Unit();
			F.Up = (ZUp * std::cos(Th) + L0 * std::sin(Th)).Unit();
			const double Roll = 0.35 * U;
			F.Up = (F.Up * std::cos(Roll) + F.Fw.Cross(F.Up).Unit() * std::sin(Roll)).Unit();
			F.Right = F.Fw.Cross(F.Up).Unit();
			return F;
		};
		const char* NationIs = std::getenv("FX_NATION");
		const char* TrailIs = std::getenv("FX_TRAIL");
		const std::string TrailKind = TrailIs ? TrailIs : "smoke";
		TheaterFx::Dice Spots(11);
		// flak: "burst" is one burst beside the aircraft's path, watched from a fixed eye; "flak" is heavy guns firing
		// round the aircraft as it flies on, bursts every 0.9 s or so (ragged), more behind than ahead
		// "above" is one burst 60 m over the fields seen as the campaign camera sees it: FX_DIST metres off (75), looking
		// down FX_PITCH degrees (50; the campaign camera never looks flatter than 22), facing FX_YAW degrees (90: the
		// game's test sun, 42 degrees up toward 249, then stands behind the eye's left)
		const bool bAbove = ShotName == "above";
		const bool bFlak = ShotName == "flak" || ShotName == "burst" || bAbove;
		const Frame Start = Fly(0.15);
		const Vec StartLevel = Vec(Start.Fw.X, Start.Fw.Y, 0.0).Unit();
		const Vec StartRight = StartLevel.Cross(ZUp).Unit();
		const Vec BurstAt = bAbove ? Vec(0.0, 0.0, 60.0) : Start.At + StartLevel * 60.0 - StartRight * 25.0 + ZUp * 4.0;
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Vec AirSun = Battle::View().Sun; // (FX_SUN=x,y,z: the way toward the sun)
		if (const char* SunIs = std::getenv("FX_SUN"))
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				AirSun = Vec(X, Y, Z).Unit();
			}
		}
		double NextBurst = 0.2;
		double Shown = 0.0; // (the time being drawn: the wake shot frames the whole trail laid so far)
		auto Shoot = [&](const Frame& F)
		{
			const Vec Level = Vec(F.Fw.X, F.Fw.Y, 0.0).Unit();
			const Vec LevelRight = Level.Cross(ZUp).Unit();
			Camera Cam;
			Cam.W = W;
			Cam.H = H;
			if (bAbove)
			{
				const double Pitch = EnvNum("FX_PITCH", 50.0) * Pi / 180.0, Yaw = EnvNum("FX_YAW", 90.0) * Pi / 180.0;
				Cam.Forward = Vec(std::cos(Pitch) * std::cos(Yaw), std::cos(Pitch) * std::sin(Yaw), -std::sin(Pitch));
				Cam.Eye = BurstAt - Cam.Forward * EnvNum("FX_DIST", 75.0);
			}
			else if (ShotName == "burst")
			{
				// (FX_FOLLOW=1: the eye goes with the wind and the burst's own drift, so an old burst stays in the picture;
				// FX_FAR=1 from 160 m, FX_FAR=2 and so on from that many times 160 m)
				const double Away = std::getenv("FX_FAR") ? 160.0 * std::clamp(EnvNum("FX_FAR", 1.0), 1.0, 20.0) : 95.0;
				const Vec Carried = EnvNum("FX_FOLLOW", 0.0) > 0.0 ? Vec(1.6, 0.4, 0.0) * ((1.0 + 0.6 / 1.65) * std::max(0.0, Shown - 0.15)) : Vec();
				Cam.Eye = BurstAt + Carried + StartRight * Away - StartLevel * 10.0 + ZUp * 8.0;
				Cam.Forward = (BurstAt + Carried + StartLevel * (std::getenv("FX_FAR") ? 22.0 : 8.0) - Cam.Eye).Unit();
			}
			else if (ShotName == "flak")
			{
				// (from well off the beam, so the wake of greying bursts the aircraft leaves behind is in the picture)
				Cam.Eye = F.At - Level * 650.0 + LevelRight * 950.0 + ZUp * 120.0;
				Cam.Forward = (F.At - Level * 520.0 - Cam.Eye).Unit();
			}
			else if (ShotName == "side")
			{
				// (off the outside wing, the aircraft left of the middle and its streams running off to the right)
				Cam.Eye = F.At + LevelRight * 45.0 - Level * 8.0 + ZUp * 3.0;
				Cam.Forward = ((F.At - Cam.Eye).Unit() + Level * 0.4).Unit();
			}
			else if (ShotName == "close")
			{
				// (the close view: off and under the right wing, about 12 m from its guns (FX_CLOSE_OUT metres out, 14),
				// looking across at the chutes and the spray of cases and links streaming aft; the licks and lamp show within
				// 30 m, cases within 60 m)
				if (std::getenv("FX_CLOSE_AFT"))
				{
					// (FX_CLOSE_AFT=1: behind and under the right wing, FX_CLOSE_BACK metres aft (14), looking up the stream)
					Cam.Eye = F.At - F.Fw * EnvNum("FX_CLOSE_BACK", 14.0) + F.Right * 3.6 - F.Up * 3.5;
					Cam.Forward = (F.At + F.Right * 2.8 - F.Up * 1.2 - F.Fw * 2.0 - Cam.Eye).Unit();
				}
				else
				{
					Cam.Eye = F.At + F.Right * EnvNum("FX_CLOSE_OUT", 14.0) - F.Fw * 3.0 - F.Up * 4.0;
					Cam.Forward = (F.At + F.Right * 2.8 - F.Fw * 1.0 - F.Up * 1.5 - Cam.Eye).Unit();
				}
			}
			else if (ShotName == "far")
			{
				Cam.Eye = F.At + Level * 520.0 + LevelRight * 260.0 + ZUp * 40.0;
				Cam.Forward = (F.At + Level * 260.0 - Cam.Eye).Unit();
			}
			else if (bFall)
			{
				// (from off the beam of where the fall began, following the fall; an explosion's pieces carried on along
				// the path, slowing and sinking)
				const Frame F0 = Fly(FallFrom);
				const Vec L0 = Vec(F0.Fw.X, F0.Fw.Y, 0.0).Unit();
				const Vec R0 = L0.Cross(ZUp).Unit();
				const double U = std::max(0.0, Shown - FallFrom);
				// (FX_STAY=1: the eye stays on the explosion's fire cloud and its smoke, which are carried on a little way and hang)
				const Vec Look = FallKind == "explode" ? (EnvNum("FX_STAY", 0.0) > 0.0 ? F0.At + L0 * (Speed * 0.2 * (1.0 - std::exp(-3.0 * U))) : F0.At + L0 * (Speed * 0.7 * (1.0 - std::exp(-U))) - ZUp * (10.0 * std::max(0.0, U - 1.0))) : F.At;
				const double Off = EnvNum("FX_DIST", 140.0);
				Cam.Eye = F0.At + R0 * Off + L0 * (0.6 * Off) + ZUp * (0.15 * Off);
				Cam.Forward = (Look - Cam.Eye).Unit();
			}
			else if (ShotName == "hit")
			{
				Cam.Eye = F.At + LevelRight * 26.0 - Level * 6.0 + ZUp * 7.0;
				Cam.Forward = ((F.At - Cam.Eye).Unit() - Level * 0.15).Unit();
			}
			else if (ShotName == "wake")
			{
				// (the whole trail laid so far, from far off the beam: FX_DIST metres, 2500)
				const Vec Mid = Fly(0.5 * Shown).At;
				Cam.Eye = Mid + StartRight * EnvNum("FX_DIST", 2500.0) + ZUp * EnvNum("FX_WAKE_UP", 250.0); // (FX_WAKE_UP: how far above it, 250)
				Cam.Forward = (Mid - Cam.Eye).Unit();
			}
			else if (ShotName == "map")
			{
				// (the strategy camera: FX_DIST metres off (1400), high above and abeam, the guns firing)
				const double Far = EnvNum("FX_DIST", 1400.0);
				Cam.Eye = F.At + LevelRight * (0.8 * Far) + ZUp * (0.55 * Far) - Level * (0.2 * Far);
				Cam.Forward = (F.At + Level * 150.0 - Cam.Eye).Unit();
			}
			else if (ShotName == "quarter")
			{
				// (from the rear quarter above: FX_DIST metres off (60), the trail streaming back past the tail toward the eye's side)
				const double Off = EnvNum("FX_DIST", 60.0);
				Cam.Eye = F.At - Level * (0.75 * Off) + LevelRight * (0.5 * Off) + ZUp * (0.42 * Off);
				Cam.Forward = (F.At - Level * (0.15 * Off) - Cam.Eye).Unit();
			}
			else if (ShotName == "beam")
			{
				// (from off the beam: FX_DIST metres (90), looking at a point FX_LOOK_BACK metres behind the aircraft (30))
				const double Off = EnvNum("FX_DIST", 90.0);
				Cam.Eye = F.At + LevelRight * Off - Level * EnvNum("FX_LOOK_BACK", 30.0) + ZUp * (0.06 * Off);
				Cam.Forward = (F.At - Level * EnvNum("FX_LOOK_BACK", 30.0) - Cam.Eye).Unit();
			}
			else if (ShotName == "pass")
			{
				if (EnvNum("FX_PASS_RIDE", 0.0) > 0.0)
				{
					Cam.Eye = F.At - Level * EnvNum("FX_PASS_BACK", 25.0) + LevelRight * EnvNum("FX_PASS_OFF", 4.0) + ZUp * EnvNum("FX_PASS_UP", 1.0);
					Cam.Forward = (F.At + Level * 60.0 - Cam.Eye).Unit();
				}
				else
				{
					const Frame FWas = Fly(EnvNum("FX_PASS_AT", 0.6));
					Cam.Eye = FWas.At + Right_(FWas) * EnvNum("FX_PASS_OFF", 4.0) + ZUp * EnvNum("FX_PASS_UP", 1.0);
					Cam.Forward = ((F.At - Cam.Eye).Length() > 5.0 ? (F.At - Cam.Eye).Unit() : Level_(FWas));
				}
			}
			else if (ShotName == "trail")
			{
				Cam.Eye = F.At - Level * 120.0 + LevelRight * 160.0 + ZUp * 60.0;
				Cam.Forward = (F.At - Level * 80.0 - Cam.Eye).Unit();
			}
			else if (ShotName == "target")
			{
				// (from just off the point the guns meet, looking back down the streams at the aircraft)
				Cam.Eye = F.At + Level * 140.0 + LevelRight * 9.0 + ZUp * 4.0;
				Cam.Forward = (F.At - Cam.Eye).Unit();
			}
			else
			{
				Cam.Eye = F.At - Level * 30.0 + ZUp * 9.0 + LevelRight * 4.0;
				Cam.Forward = (F.At + Level * 200.0 - Cam.Eye).Unit();
			}
			Cam.Right = Cam.Forward.Cross(ZUp).Unit();
			Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
			Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0)));
			return Cam;
		};
		// (every step between the pictures is drawn too, as the game draws every frame: a flash's lamp and the shortest
		// flickers go by the frames drawn)
		auto DrawBetween = [&](int StepNo, int Until, double T, const Frame& F)
		{
			if (StepNo == Until)
			{
				return;
			}
			const Camera C = Shoot(F);
			Battle::View V;
			V.Eye = C.Eye;
			V.Right = C.Right;
			V.Up = C.Up;
			V.Forward = C.Forward;
			V.PixelsPerRadian = C.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0); // (FX_KIT_PX: as the air scene tells the kit the window's pixels)
			if (!std::getenv("FX_NOCULL")) // (as the game: what is off the screen takes no place. FX_NOCULL: everything in range is laid)
			{
				V.HalfAcross = C.W / (2.0 * C.Ppr);
				V.HalfUp = C.H / (2.0 * C.Ppr);
			}
			V.Sun = AirSun;
			if (bDusk)
			{
				V.SmokeLight = Rgb(0.22f, 0.22f, 0.27f);
			}
			Fx.Draw(T, V);
		};
		std::sort(Frames.begin(), Frames.end());
		int Step = 0;
		Battle::View View;
		for (int FrameNo : Frames)
		{
			const int Until = FrameNo * 2; // (frames at 30 a second, calls at 60)
			for (; Step <= Until; ++Step)
			{
				const double T = Step / 60.0;
				Shown = T;
				const Frame F = Fall(T);
				if (bFall)
				{
					const Vec Vel = (Fall(T + 0.01).At - F.At) * 100.0;
					const Vec Engine = Place(F, Vec(EnvNum("FX_FIRE_X", 2.9), 0.75, -0.35));
					// (FX_FALL_TRAIL: the trail a fall is seen with, "none" for none; unset, smoke on a spin and fire on the rest)
					const char* FallTrail = std::getenv("FX_FALL_TRAIL");
					const std::string FallTrailIs = FallTrail ? FallTrail : (FallKind == "spin" ? "smoke" : "fire");
					const bool bFallTrail = FallKind != "explode" && FallTrailIs != "none";
					if (Step == 6 && bFallTrail)
					{
						Fx.TrailStart(T, 1, Engine, Vel, FallTrailIs.c_str(), EnvNum("FX_SPAN", 11.0), static_cast<int>(EnvNum("FX_LOOK", -1.0)));
					}
					if (Step >= 6 && bFallTrail)
					{
						Fx.TrailMove(T, 1, Engine, Vel);
					}
					if (Step == static_cast<int>(FallFrom * 60.0))
					{
						Fx.AirFall(T, 7, F.At, Vel, FallKind.c_str(), EnvNum("FX_SPAN", 11.0), Height - EnvNum("FX_DECK", 400.0), static_cast<int>(EnvNum("FX_LOOK", -1.0)), EnvNum("FX_BOMBS", 0.0) > 0.0);
					}
					else if (Step > static_cast<int>(FallFrom * 60.0))
					{
						Fx.AirFallMove(T, 7, F.At, Vel);
					}
					DrawBetween(Step, Until, T, F);
					continue;
				}
				if (bStruck)
				{
					// (struck: a hit every 0.07 s or so along the right wing and the nose; the engine trails from 0.3 s)
					if (!bEngineShot && T >= 0.2 && T <= 1.0 && Step % 4 == 0)
					{
						const Vec Spot(Spots.U(-1.0, 3.5), Spots.U(-0.5, 4.5), Spots.U(-0.2, 0.4));
						Fx.AirHit(T, Place(F, Spot), (F.Up + F.Right * 0.3).Unit(), F.Fw * Speed, Kind.c_str());
					}
					Vec EngineAt(EnvNum("FX_FIRE_X", 2.9), 0.75, -0.35); // (an exhaust on the cowling's near side; FX_FIRE_X=1 a wing root)
					if (const char* EngineIs = std::getenv("FX_ENGINE"))
					{
						std::sscanf(EngineIs, "%lf,%lf,%lf", &EngineAt.X, &EngineAt.Y, &EngineAt.Z);
					}
					const Vec Engine = Place(F, EngineAt);
					if (Step == 18)
					{
						Fx.TrailStart(T, 1, Engine, F.Fw * Speed, TrailKind.c_str(), EnvNum("FX_SPAN", 0.0), static_cast<int>(EnvNum("FX_LOOK", -1.0))); // (FX_SPAN: the aircraft's span, metres)
					}
					for (int K = 2; K <= 3; ++K)
					{
						const char* Next = std::getenv(K == 2 ? "FX_TRAIL2" : "FX_TRAIL3");
						if (Next && Step == static_cast<int>(std::lround(EnvNum(K == 2 ? "FX_TRAIL2_AT" : "FX_TRAIL3_AT", 1e9) * 60.0)))
						{
							Fx.TrailStart(T, 1, Engine, F.Fw * Speed, Next, EnvNum("FX_SPAN", 0.0), static_cast<int>(EnvNum("FX_LOOK", -1.0)));
						}
					}
					if (Step >= 18)
					{
						Fx.TrailMove(T, 1, Engine, F.Fw * Speed);
					}
					DrawBetween(Step, Until, T, F);
					continue;
				}
				if (bFlak)
				{
					if (ShotName == "burst" || bAbove)
					{
						if (Step == 9)
						{
							Fx.AirFlak(T, BurstAt, "heavy");
						}
					}
					else
					{
						for (int Guard = 0; NextBurst <= T && Guard < 4; ++Guard)
						{
							Vec Where;
							do
							{
								Where = F.At + Level_(F) * Spots.U(-80.0, 30.0) + Right_(F) * (Spots.Bell() * 35.0) + ZUp * (Spots.Bell() * 14.0);
							} while ((Where - F.At).Length() < 18.0);
							Fx.AirFlak(T, Where, "heavy");
							NextBurst += 0.9 * Spots.U(0.6, 1.4);
						}
					}
					DrawBetween(Step, Until, T, F);
					continue;
				}
				for (size_t G = 0; G < Guns.size(); ++G)
				{
					const Vec Muzzle = Place(F, Guns[G]);
					const Vec Aim = F.At + F.Fw * Meet; // (each gun laid on one point ahead: they converge there)
					Fx.AirGun(T, static_cast<int>(G), Muzzle, Aim, F.Fw * Speed, Kind.c_str(), 0.0, 0.0, NationIs ? NationIs : ""); // (FX_NATION: whose guns)
				}
				DrawBetween(Step, Until, T, F);
			}
			const double Now = FrameNo / 30.0;
			Shown = Now;
			const Frame F = Fall(Now);
			const Camera Cam = Shoot(F);
			View.Eye = Cam.Eye;
			View.Right = Cam.Right;
			View.Up = Cam.Up;
			View.Forward = Cam.Forward;
			View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
			if (!std::getenv("FX_NOCULL")) // (as the game: what is off the screen takes no place. FX_NOCULL: everything in range is laid)
			{
				View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
				View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
			}
			View.Sun = AirSun;
			if (bDusk)
			{
				View.SmokeLight = Rgb(0.22f, 0.22f, 0.27f);
			}
			const int N = Fx.Draw(Now, View);
			Canvas C;
			C.Init(W, H);
			Battle::Lamp AirLamps[2];
			const int NAirLamps = bAbove ? Fx.Lamps(Now, AirLamps, 2, Fx.DayOf(View.SmokeLight)) : 0; // (over the fields: the flash's lamp on the ground)
			Paint(C, Cam, AirLamps, NAirLamps, {}, false);
			if (const char* FlatIs = std::getenv("FX_FLAT_BACK"))
			{
				// (FX_FLAT_BACK=r,g,b: the whole picture behind the kit one flat colour, so that a trail drawn over two of
				// them shows its own opacity and colour: see trailmeasure.py)
				double Fr = 0.0, Fg = 0.0, Fb = 0.0;
				if (std::sscanf(FlatIs, "%lf,%lf,%lf", &Fr, &Fg, &Fb) == 3)
				{
					std::fill(C.R.begin(), C.R.end(), ToLinear(Fr));
					std::fill(C.G.begin(), C.G.end(), ToLinear(Fg));
					std::fill(C.B.begin(), C.B.end(), ToLinear(Fb));
				}
			}
			// the aircraft: a plain olive-drab shape, lit by the sun, so the flash and streams are judged on a wing
			if (!(bFall && FallKind == "explode" && Now > FallFrom))
			{
				const Vec Sun = View.Sun.Unit();
				const double Base[3] = { ToLinear(92), ToLinear(98), ToLinear(74) };
				auto Face = [&](const Vec& A, const Vec& B, const Vec& Cc)
				{
					const Vec Pa = Place(F, A * PlaneK), Pb = Place(F, B * PlaneK), Pc = Place(F, Cc * PlaneK);
					const Vec Nn = (Pb - Pa).Cross(Pc - Pa).Unit();
					const double Lit = 0.45 + 0.55 * std::fabs(Nn.Dot(Sun));
					const double Col[3] = { Base[0] * Lit, Base[1] * Lit, Base[2] * Lit };
					const Vec Tri[3] = { Pa, Pb, Pc };
					Solid(C, Cam, Tri, Col);
				};
				auto Quad = [&](const Vec& A, const Vec& B, const Vec& Cc, const Vec& D)
				{
					Face(A, B, Cc);
					Face(A, Cc, D);
				};
				// fuselage: three sections, nose to tail (forward, half width, half height)
				const double Sx[3] = { 4.5, 1.5, -5.0 }, Hw[3] = { 0.45, 0.62, 0.16 }, Hh[3] = { 0.5, 0.78, 0.3 };
				for (int K = 0; K < 2; ++K)
				{
					auto Ring = [&](int I, double Ay, double Az) { return Vec(Sx[I], Hw[I] * Ay, Hh[I] * Az); };
					const double Ys[5] = { 1, -1, -1, 1, 1 }, Zs[5] = { 1, 1, -1, -1, 1 };
					for (int E = 0; E < 4; ++E)
					{
						Quad(Ring(K, Ys[E], Zs[E]), Ring(K, Ys[E + 1], Zs[E + 1]), Ring(K + 1, Ys[E + 1], Zs[E + 1]), Ring(K + 1, Ys[E], Zs[E]));
					}
				}
				// wings, tailplane, fin
				for (double S : { 1.0, -1.0 })
				{
					Quad(Vec(1.35, 0.6 * S, -0.32), Vec(0.95, 5.6 * S, -0.1), Vec(-0.3, 5.6 * S, -0.1), Vec(-0.95, 0.6 * S, -0.32));
					Quad(Vec(-4.1, 0.15 * S, 0.05), Vec(-4.4, 2.1 * S, 0.1), Vec(-5.0, 2.1 * S, 0.1), Vec(-5.1, 0.15 * S, 0.05));
				}
				Quad(Vec(-3.8, 0.0, 0.3), Vec(-4.6, 0.0, 1.9), Vec(-5.2, 0.0, 1.9), Vec(-5.1, 0.0, 0.3));
				if (const char* NacelleIs = std::getenv("FX_ENGINE"))
				{
					// (FX_ENGINE: a plain nacelle about the engine given, as a wing engine has, so its smoke and flame begin
					// inside a cowling as they will on the model and never at a square start in the open; true metres)
					Vec E(0.0, 0.0, 0.0);
					if (std::sscanf(NacelleIs, "%lf,%lf,%lf", &E.X, &E.Y, &E.Z) == 3 && std::isfinite(E.X) && std::isfinite(E.Y) && std::isfinite(E.Z))
					{
						const double Nx[3] = { E.X + 0.9, E.X - 0.6, E.X - 3.4 }, Nr[3] = { 0.55, 0.72, 0.4 };
						for (int K = 0; K < 2; ++K)
						{
							auto Ring = [&](int I, double Ay, double Az) { return Vec(Nx[I], E.Y + Nr[I] * Ay, E.Z + Nr[I] * Az) * (1.0 / PlaneK); };
							const double Ys[5] = { 1, -1, -1, 1, 1 }, Zs[5] = { 1, 1, -1, -1, 1 };
							for (int Q = 0; Q < 4; ++Q)
							{
								Quad(Ring(K, Ys[Q], Zs[Q]), Ring(K, Ys[Q + 1], Zs[Q + 1]), Ring(K + 1, Ys[Q + 1], Zs[Q + 1]), Ring(K + 1, Ys[Q], Zs[Q]));
							}
						}
						Quad(Vec(Nx[0], E.Y + Nr[0], E.Z + Nr[0]) * (1.0 / PlaneK), Vec(Nx[0], E.Y - Nr[0], E.Z + Nr[0]) * (1.0 / PlaneK), Vec(Nx[0], E.Y - Nr[0], E.Z - Nr[0]) * (1.0 / PlaneK), Vec(Nx[0], E.Y + Nr[0], E.Z - Nr[0]) * (1.0 / PlaneK));
					}
				}
			}
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.16;
					C.G[I] *= 0.16;
					C.B[I] *= 0.2;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/air-%s-%s%s%s-f%03d.ppm", OutDir.c_str(), bFlak ? "flak" : Kind.c_str(), ShotName.c_str(), bStruck ? ("-" + TrailKind).c_str() : (bFall ? ("-" + FallKind).c_str() : ""), bDusk ? "-dusk" : "", FrameNo);
			C.Save(Name);
			std::printf("frame %d: %d puffs\n", FrameNo, N);
		}
		return 0;
	}
}

namespace
{
	/** A plain box standing on the ground (a vehicle, a towed gun), its faces shaded flat; Who marks its pixels. */
	void Box(Canvas& C, const Camera& Cam, const Vec& Foot, const Vec& Way, double Long, double Wide, double High, const double Col[3], int Who)
	{
		const Vec Fw = Vec(Way.X, Way.Y, 0.0).Unit(Vec(1.0, 0.0, 0.0));
		const Vec Rt(Fw.Y, -Fw.X, 0.0);
		const Vec Up(0.0, 0.0, 1.0);
		auto Corner = [&](int F, int R, int U) { return Foot + Fw * (Long * (F ? 0.5 : -0.5)) + Rt * (Wide * (R ? 0.5 : -0.5)) + Up * (U ? High : 0.0); };
		struct Face { int A[4][3]; double Shade; };
		const Face Faces[5] = {
			{ { { 0, 0, 1 }, { 1, 0, 1 }, { 1, 1, 1 }, { 0, 1, 1 } }, 1.15 }, // top
			{ { { 1, 0, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 1, 0, 1 } }, 0.9 }, // front
			{ { { 0, 0, 0 }, { 0, 1, 0 }, { 0, 1, 1 }, { 0, 0, 1 } }, 0.75 }, // back
			{ { { 0, 0, 0 }, { 1, 0, 0 }, { 1, 0, 1 }, { 0, 0, 1 } }, 0.85 }, // left
			{ { { 0, 1, 0 }, { 1, 1, 0 }, { 1, 1, 1 }, { 0, 1, 1 } }, 0.7 }, // right
		};
		for (const Face& F : Faces)
		{
			const double Shaded[3] = { Col[0] * F.Shade, Col[1] * F.Shade, Col[2] * F.Shade };
			Vec Q[4];
			for (int K = 0; K < 4; ++K)
			{
				Q[K] = Corner(F.A[K][0], F.A[K][1], F.A[K][2]);
			}
			const Vec T1[3] = { Q[0], Q[1], Q[2] };
			const Vec T2[3] = { Q[0], Q[2], Q[3] };
			Solid(C, Cam, T1, Shaded, Who);
			Solid(C, Cam, T2, Shaded, Who);
		}
	}

	/** A box lit by the sun and by the kit's lamps, patch by patch (Patch m a side), so that a fire's glow shows on the
	 * plates nearest it: Lo and Hi are two opposite corners, the box square to the world. Its floor is not drawn. */
	void LitBox(Canvas& C, const Camera& Cam, const Vec& Lo, const Vec& Hi, const double Base[3], const Battle::Lamp* Lamps, int NLamps, const Vec& Sun, double SunShare, double Patch = 0.2)
	{
		struct Face { int Axis; int Side; };
		const Face Faces[5] = { { 2, 1 }, { 0, 0 }, { 0, 1 }, { 1, 0 }, { 1, 1 } };
		for (const Face& F : Faces)
		{
			const int A = (F.Axis + 1) % 3, B = (F.Axis + 2) % 3;
			auto Get = [](const Vec& V, int K) { return K == 0 ? V.X : K == 1 ? V.Y : V.Z; };
			auto Make = [](int Axis, double On, int Ka, double Va, int Kb, double Vb)
			{
				double P[3] = { 0.0, 0.0, 0.0 };
				P[Axis] = On;
				P[Ka] = Va;
				P[Kb] = Vb;
				return Vec(P[0], P[1], P[2]);
			};
			const double On = F.Side ? Get(Hi, F.Axis) : Get(Lo, F.Axis);
			double N3[3] = { 0.0, 0.0, 0.0 };
			N3[F.Axis] = F.Side ? 1.0 : -1.0;
			const Vec N(N3[0], N3[1], N3[2]);
			const double A0 = Get(Lo, A), A1 = Get(Hi, A), B0 = Get(Lo, B), B1 = Get(Hi, B);
			const int Na = std::max(1, static_cast<int>(std::ceil((A1 - A0) / Patch))), Nb = std::max(1, static_cast<int>(std::ceil((B1 - B0) / Patch)));
			const double Lit = (1.0 - SunShare) * (0.55 + 0.45 * N.Z) + SunShare * std::max(0.0, N.Dot(Sun)) * 1.6;
			for (int I = 0; I < Na; ++I)
			{
				for (int J = 0; J < Nb; ++J)
				{
					const double Ua = A0 + (A1 - A0) * I / Na, Ub = A0 + (A1 - A0) * (I + 1) / Na;
					const double Va = B0 + (B1 - B0) * J / Nb, Vb = B0 + (B1 - B0) * (J + 1) / Nb;
					const Vec Mid = Make(F.Axis, On, A, 0.5 * (Ua + Ub), B, 0.5 * (Va + Vb));
					double Lift[3] = { 0.0, 0.0, 0.0 };
					for (int L = 0; L < NLamps; ++L)
					{
						const Vec To = Lamps[L].At - Mid;
						const double Dist = To.Length();
						if (Dist < Lamps[L].Radius && Dist > 1e-6)
						{
							// (the ground's own lamp in Paint, turned by how square the plate faces it)
							const double Window = 1.0 - std::pow(Dist / Lamps[L].Radius, 4.0);
							const double Fl = Lamps[L].Strength * 64.0 / std::max(Dist * Dist, 1.0) * Window * Window * std::max(0.0, N.Dot(To * (1.0 / Dist)));
							Lift[0] += Fl * Lamps[L].Col.R / std::max(0.01f, Lamps[L].Col.G);
							Lift[1] += Fl;
							Lift[2] += Fl * Lamps[L].Col.B / std::max(0.01f, Lamps[L].Col.G);
						}
					}
					const double Col[3] = { Base[0] * (Lit + Lift[0]), Base[1] * (Lit + Lift[1]), Base[2] * (Lit + Lift[2]) };
					const Vec Q[4] = { Make(F.Axis, On, A, Ua, B, Va), Make(F.Axis, On, A, Ub, B, Va), Make(F.Axis, On, A, Ub, B, Vb), Make(F.Axis, On, A, Ua, B, Vb) };
					const Vec T1[3] = { Q[0], Q[1], Q[2] };
					const Vec T2[3] = { Q[0], Q[2], Q[3] };
					Solid(C, Cam, T1, Col, 0);
					Solid(C, Cam, T2, Col, 0);
				}
			}
		}
	}

	/** The wreck shot (see the top of the file): a knocked-out heavy tank burning, as the ground scene calls the kit. */
	int Wreck(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		Fx.Setup(&Book, &Swell, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetGroundColour(&GroundPaint, nullptr);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 1.6), EnvNum("FX_WIND_Y", 0.4), 0.0));
		// (the tank stands at the middle facing +X, on the ground there; its sockets as Tiger.json gives them)
		const double G = Swell(nullptr, 0.0, 0.0);
		const Vec Engine(-2.2, 0.0, G + 1.83), Fire1(-2.2, -1.0, G + 1.83), Fire2(-0.24, -0.5, G + 2.9), Fire3(1.55, -0.86, G + 1.82), Commander(-0.24, -0.5, G + 2.55);
		const Vec Upward(0.0, 0.0, 1.0);
		// (what happens, and when, as ALandBattle::PlayArmourEvents turns the tank's knocked_out clip and then its burn
		// loop into the kit's calls. FX_SCRIPT=one: the knocked_out clip's fire alone, one Burn of 2.4 m at the engine)
		struct Due
		{
			double At = 0.0;
			int What = 0; // (1 Burn, 2 the hatch's cook-off, 3 a fuel ball; release 2's film fire: 4 Vent, 5 Steam of Kind, 6 a cook-off of Pops over Over, jet Jet)
			Vec Where;
			double Across = 0.0, Seconds = 0.0;
			int Key = -1;
			bool bDone = false;
			std::string Kind;
			int Pops = 0;
			double Over = 0.0, Jet = 0.0;
		};
		const double Long = EnvNum("FX_BURN_S", 900.0);
		std::vector<Due> Script;
		const std::string Kind = std::getenv("FX_SCRIPT") ? std::getenv("FX_SCRIPT") : "ground";
		// (FX_SCRIPT=fuel: her fuel goes up at 0.5 s, a FuelBall FX_FUEL_M across (8) at the engine deck, and she burns
		// on as the ground script has it; crash: an aeroplane's fuel, FX_FUEL_M (20) across at the ground at 0.5 s,
		// coming in at 60 m/s, with no tank (FX_NOTANK too), its wreck burning on from 1.5 s; dump: a fuel dump's, the
		// same but standing still)
		const bool bNoTank = Kind == "crash" || Kind == "dump" || std::getenv("FX_NOTANK") != nullptr;
		if (Kind == "one")
		{
			Script.push_back({ 1.2, 1, Engine, 2.4, Long, 3 });
		}
		else if (Kind == "film")
		{
			// (release 2's film fire, -TheaterLandFilmFire: the scene's own plan, Battle/LandBurn. FX_FATE the fates
			// table's key (tank.engine_fire), FX_BREW=1 a brew-up (its clip's jets and fuel ball first), FX_CLASS heavy,
			// medium or light, FX_WRECK_SEED her dice. The kill's own clip beats the scene keeps are laid as it does)
			LandBurn::Wreck Wk;
			Wk.Fate = std::getenv("FX_FATE") ? std::getenv("FX_FATE") : "tank.engine_fire";
			Wk.bBrewUp = std::getenv("FX_BREW") != nullptr;
			Wk.bRing = true;
			Wk.Class = std::getenv("FX_CLASS") ? std::getenv("FX_CLASS") : "heavy";
			Wk.Seed = static_cast<uint64_t>(EnvNum("FX_WRECK_SEED", 7.0));
			Wk.Left = Long;
			const LandBurn::Plan Pl = LandBurn::Make(Wk);
			std::printf("film fire: %s (%zu calls)\n", Pl.Story.c_str(), Pl.Calls.size());
			const Vec Ring(0.12, 0.0, G + 1.8);
			if (Wk.bBrewUp)
			{
				// (the clip's flame jets, as the film fire plays them: a flash jetting up out of each hatch for half a second)
				for (const Vec& J : { Fire2, Fire1, Fire3 })
				{
					Due Jt;
					Jt.At = 0.05;
					Jt.What = 6;
					Jt.Where = J;
					Jt.Across = 0.8;
					Jt.Pops = 1;
					Jt.Over = 0.2;
					Jt.Jet = 0.5;
					Jt.Key = -1;
					Script.push_back(Jt);
				}
				Script.push_back({ 0.55, 3, Engine, 8.0, 0.0, -1 });
			}
			else
			{
				Script.push_back({ 0.35, 2, Commander, 0.8, 0.0, 1 }); // hatch_blown
				Script.push_back({ 0.5, 5, Engine, 0.7, 5.0, 100002, false, "smoke" }); // smoke_burst, as the film fire lays it
			}
			for (const LandBurn::Call& Cl : Pl.Calls)
			{
				const Vec At = Cl.Socket == "ring" ? Ring : Cl.Socket == "fire.1" ? Fire1 : Cl.Socket == "fire.2" ? Fire2 : Cl.Socket == "fire.3" ? Fire3 : Engine;
				Due Du;
				Du.At = Cl.At;
				Du.Where = At;
				Du.Across = Cl.Across;
				Du.Seconds = Cl.Seconds;
				Du.Key = 700000 + Cl.Slot;
				Du.Kind = Cl.Kind;
				Du.Pops = Cl.Pops;
				Du.Over = Cl.Over;
				Du.Jet = Cl.Jet;
				Du.What = Cl.What == LandBurn::Do::Fire ? 1 : Cl.What == LandBurn::Do::Vent ? 4 : Cl.What == LandBurn::Do::Wisp ? 5 : 6;
				std::printf("  %7.1f s %-5s %-7s %.2f m %6.1f s %s\n", Cl.At, Du.What == 1 ? "fire" : Du.What == 4 ? "vent" : Du.What == 5 ? "wisp" : "cook", Cl.Socket.c_str(), Cl.Across, Cl.What == LandBurn::Do::Cook ? Cl.Over : Cl.Seconds, Cl.Kind.c_str());
				Script.push_back(Du);
			}
		}
		else if (Kind == "crash" || Kind == "dump")
		{
			Script.push_back({ 0.5, 3, Vec(0.0, 0.0, G), EnvNum("FX_FUEL_M", 20.0), Kind == "crash" ? 60.0 : 0.0, -1 });
			// (and what is left burns on the ground from a second in, as the scene would light it: the wreck where it came
			// to rest, or the dump's spilt fuel; FX_GROUND_FIRE=0 for none)
			if (EnvNum("FX_GROUND_FIRE", 1.0) > 0.0)
			{
				Script.push_back({ 1.5, 1, Vec(Kind == "crash" ? 8.0 : 0.0, 0.0, G + 0.5), 0.3 * EnvNum("FX_FUEL_M", 20.0), Long, 11 });
			}
		}
		else
		{
			Script.push_back({ 0.35, 2, Commander, 0.8, 0.0, 1 }); // hatch_blown
			Script.push_back({ 0.5, 1, Engine, 0.8, 4.0, 100002 }); // smoke_burst
			Script.push_back({ 1.2, 1, Engine, 2.4, Long, 3 }); // fire_start
			Script.push_back({ 1.6, 1, Engine, 1.2, Long, 100004 }); // smoke_loop
			Script.push_back({ 3.0, 1, Engine, 2.4, Long, 5 }); // the burn loop: fire_loop at four sockets, smoke_loop
			Script.push_back({ 3.0, 1, Fire1, 2.4, Long, 6 });
			Script.push_back({ 3.0, 1, Fire2, 2.4, Long, 7 });
			Script.push_back({ 3.0, 1, Fire3, 2.4, Long, 8 });
			Script.push_back({ 3.0, 1, Engine, 1.2, Long, 100009 });
			if (Kind == "fuel")
			{
				Script.push_back({ 0.5, 3, Engine, EnvNum("FX_FUEL_M", 8.0), 0.0, -1 });
			}
		}
		auto Play = [&](double Now)
		{
			for (Due& D : Script)
			{
				if (D.bDone || D.At > Now)
				{
					continue;
				}
				D.bDone = true;
				if (D.What == 1)
				{
					Fx.Burn(D.At, D.Where, D.Across, D.Seconds, D.Key);
				}
				else if (D.What == 3)
				{
					Fx.FuelBall(D.At, D.Where, D.Across, Vec(D.Seconds, 0.0, -0.15 * D.Seconds)); // (Seconds: its speed in, along +X and a little down)
				}
				else if (D.What == 4)
				{
					Fx.Vent(D.At, D.Where, D.Across, D.Seconds, D.Key);
				}
				else if (D.What == 5)
				{
					Fx.Steam(D.At, D.Key, D.Where, Upward, D.Across, D.Seconds, D.Kind.c_str());
				}
				else if (D.What == 6)
				{
					Fx.CookOff(D.At, D.Where, Upward, D.Across, D.Pops, D.Over, D.Jet, -1.0, D.Key);
				}
				else
				{
					Fx.CookOff(D.At, D.Where, Upward, D.Across, 3, 2.5, 0.0, -1.0, D.Key);
				}
			}
		};
		// (the eyes: "side" a man's eye 16 m off her burning side, "battle" the battle camera's, "far" 60 m off to see
		// the column, "tall" 140 m off and low for the whole column. FX_DIST, FX_EYE_H, FX_LOOK_H move them; FX_LENS
		// is the lens's angle across, 42 degrees unless given)
		double Dist = 16.0, EyeH = 1.7, LookH = 3.0;
		Vec From(0.15, -1.0, 0.0);
		if (Shot == "battle")
		{
			Dist = 18.75, EyeH = 6.0, LookH = 3.0, From = Vec(-0.3, -1.0, 0.0);
		}
		else if (Shot == "far")
		{
			Dist = 60.0, EyeH = 4.0, LookH = 14.0, From = Vec(0.2, -1.0, 0.0);
		}
		else if (Shot == "tall")
		{
			Dist = 160.0, EyeH = 3.0, LookH = 60.0, From = Vec(0.2, -1.0, 0.0);
		}
		Dist = EnvNum("FX_DIST", Dist);
		EyeH = EnvNum("FX_EYE_H", EyeH);
		LookH = EnvNum("FX_LOOK_H", LookH);
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::clamp(EnvNum("FX_LENS", 42.0), 5.0, 170.0) * 3.14159265358979 / 360.0));
		Cam.Eye = From.Unit() * Dist + Vec(0.0, 0.0, G + EyeH);
		Cam.Forward = (Vec(EnvNum("FX_LOOK_X", -0.8), 0.0, G + LookH) - Cam.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
		if (!std::getenv("FX_NOCULL"))
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		if (const char* SunIs = std::getenv("FX_SUN")) // (FX_SUN=x,y,z: the way toward the sun)
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				View.Sun = Vec(X, Y, Z).Unit();
			}
		}
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		if (bDusk)
		{
			View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
			View.SunLight = Rgb(0.0f, 0.0f, 0.0f);
		}
		// (the hull, dark yellow gone dull with soot: tracks and lower hull, the upper hull, the turret, the gun)
		const double Paint3[3] = { ToLinear(112), ToLinear(102), ToLinear(80) };
		const double Track3[3] = { ToLinear(58), ToLinear(54), ToLinear(50) };
		std::vector<Vec> Men;
		for (const Vec& M : { Vec(4.5, -3.5, 0.0), Vec(-5.5, -4.0, 0.0) })
		{
			Men.push_back(Vec(M.X, M.Y, Swell(nullptr, M.X, M.Y)));
		}
		Fx.Draw(-0.1, View);
		std::sort(Frames.begin(), Frames.end());
		int Drawn = 0;
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; ++Drawn)
			{
				Play(Drawn / 30.0);
				Fx.Draw(Drawn / 30.0, View);
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			Play(Now);
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int Room = std::clamp(static_cast<int>(EnvNum("FX_LAMPS", 2.0)), 0, 2);
			const int NL = Fx.Lamps(Now, Lamps, Room, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, Men, false);
			const Vec Sun = View.Sun.Unit();
			const double SunShare = bDusk ? 0.0 : 0.6;
			if (!bNoTank)
			{
				LitBox(C, Cam, Vec(-3.2, -1.75, G), Vec(3.2, -1.15, G + 1.2), Track3, Lamps, NL, Sun, SunShare);
				LitBox(C, Cam, Vec(-3.2, 1.15, G), Vec(3.2, 1.75, G + 1.2), Track3, Lamps, NL, Sun, SunShare);
				LitBox(C, Cam, Vec(-3.0, -1.15, G + 0.4), Vec(2.6, 1.15, G + 1.2), Paint3, Lamps, NL, Sun, SunShare);
				LitBox(C, Cam, Vec(-3.2, -1.6, G + 1.2), Vec(2.2, 1.6, G + 1.83), Paint3, Lamps, NL, Sun, SunShare);
				LitBox(C, Cam, Vec(-1.6, -1.2, G + 1.83), Vec(1.2, 1.2, G + 2.85), Paint3, Lamps, NL, Sun, SunShare);
				LitBox(C, Cam, Vec(1.2, -0.12, G + 2.1), Vec(5.2, 0.12, G + 2.34), Paint3, Lamps, NL, Sun, SunShare, 0.4);
			}
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.22;
					C.G[I] *= 0.2;
					C.B[I] *= 0.26;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/wreck-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps", Frame, N, NL);
			for (int L = 0; L < NL; ++L)
			{
				std::printf(" | lamp %.3f at (%.2f, %.2f, %.2f) r %.1f", Lamps[L].Strength, Lamps[L].At.X, Lamps[L].At.Y, Lamps[L].At.Z, Lamps[L].Radius);
			}
			std::printf("\n");
		}
		return 0;
	}

	/** The sea with three ships' decks on it: a hit that is not on armour is laid on what is under it, as the game's
	 * ground function says (a deck there, the sea elsewhere). */
	struct SeaDecks
	{
		double Dist = 0.0, Gap = 0.0, Long = 0.0, Up = 0.0, Speed = 0.0, Now = 0.0;
	};
	double DeckOrSea(void* Context, double X, double Y)
	{
		const SeaDecks& S = *static_cast<const SeaDecks*>(Context);
		for (int Ship = 0; Ship < 3; ++Ship)
		{
			if (std::fabs(X - ((Ship - 1) * S.Gap + S.Speed * S.Now)) <= 0.5 * S.Long && std::fabs(Y - S.Dist) <= 0.07 * S.Long)
			{
				return S.Up;
			}
		}
		return 0.0;
	}

	/** The sea battle's range (see the top of the file): the enemy line seen from a ship's broadside. */
	int Sea(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		SeaDecks Decks;
		Fx.Setup(&Book, &DeckOrSea, &Decks, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 3.0), EnvNum("FX_WIND_Y", 0.8), 0.0));
		const char* UseIs = std::getenv("FX_USE"); // (FX_USE=air: as a scene that calls UseAirDistance; none: neither)
		const std::string Use = UseIs ? UseIs : "sea";
		if (Use == "sea")
		{
			Fx.UseSea();
		}
		else if (Use == "air")
		{
			Fx.SetDrawBeyond(Fx.AirBeyond());
		}
		const double Dist = EnvNum("FX_DIST", 15000.0), Speed = EnvNum("FX_KNOTS", 25.0) * 0.5144;
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::clamp(EnvNum("FX_LENS", 55.0), 5.0, 170.0) * 3.14159265358979 / 360.0));
		// (FX_EYE_X, FX_EYE_Y: the eye that far along the line and toward it (FX_EYE_Y=<FX_DIST> stands it in the line:
		// with FX_EYE_X under 0, astern of the middle ship). FX_FOLLOW=1: it rides along with the ships)
		const Vec Eye0 = Vec(EnvNum("FX_EYE_X", 0.0), EnvNum("FX_EYE_Y", 0.0), EnvNum("FX_EYE_H", 24.0));
		const bool bFollow = std::getenv("FX_FOLLOW") != nullptr;
		Cam.Eye = Eye0;
		Cam.Forward = (Vec(EnvNum("FX_LOOK_X", 0.0), Dist, EnvNum("FX_LOOK_H", 30.0)) - Cam.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
		if (!std::getenv("FX_NOCULL"))
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		if (bDusk)
		{
			View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
			View.SunLight = Rgb(0.0f, 0.0f, 0.0f);
		}
		// (three ships in line ahead, FX_GAP m (500) apart, steaming along +X: the left one fires her broadside at the
		// eye at FX_FIRE_AT s (0.98) and every FX_EVERY s (0: once); shells fall short of the middle one at FX_FALL_AT s
		// (1.18), a heavy, a medium and a light, and she is hit at FX_HIT_AT s (2.48; FX_NO_HIT=1: not); the right one
		// is on fire from 0 s, FX_BURN m across (20), and it rides with her)
		const double Gap = EnvNum("FX_GAP", 500.0), Long = EnvNum("FX_SHIP_LONG", 200.0), DeckUp = 10.0;
		Decks.Dist = Dist;
		Decks.Gap = Gap;
		Decks.Long = Long;
		Decks.Up = DeckUp;
		Decks.Speed = Speed;
		const char* GunIs = std::getenv("FX_GUN");
		const std::string Gun = GunIs ? GunIs : "howitzer";
		// (each moment falls a little before a frame, as a game's does: called on the very frame it is drawn, a flash is
		// not yet lit)
		const double FireAt = EnvNum("FX_FIRE_AT", 0.98), Every = EnvNum("FX_EVERY", 0.0), FallAt = EnvNum("FX_FALL_AT", 1.18), BurnAcross = EnvNum("FX_BURN", 20.0);
		auto ShipAt = [&](int Ship, double Now) { return Vec((Ship - 1) * Gap + Speed * Now, Dist, 0.0); };
		const Vec ToEye = Vec(0.0, -1.0, 0.0);
		double NextFire = FireAt;
		bool bFell = false, bLit = false, bHit = std::getenv("FX_NO_HIT") != nullptr;
		const double HitAt = EnvNum("FX_HIT_AT", 2.48);
		// (FX_FUNNEL=smoke: each ship's funnel smokes, a trail of that kind FX_FUNNEL_SPAN m across (0.07 of her length),
		// from FX_FUNNEL_UP m above the sea (0.125 of her length))
		// (FX_POWER=share: the share of her top speed each ship says she is making, 0 to 1 (not set: none is given);
		// FX_POWER_AT=s with FX_POWER_TO=share: it changes to that at s seconds, as when she rings up)
		const char* FunnelIs = std::getenv("FX_FUNNEL");
		const double FunnelSpan = EnvNum("FX_FUNNEL_SPAN", 0.07 * Long), FunnelUp = EnvNum("FX_FUNNEL_UP", 0.125 * Long);
		const double Power0 = EnvNum("FX_POWER", -1.0), PowerAt = EnvNum("FX_POWER_AT", 1.0e9), PowerTo = EnvNum("FX_POWER_TO", 1.0);
		bool bSmokes = false;
		auto Play = [&](double Now)
		{
			Decks.Now = Now;
			for (int Ship = 0; Ship < 3 && FunnelIs; ++Ship)
			{
				const Vec Top = ShipAt(Ship, Now) + Vec(0.02 * Long, 0.0, FunnelUp);
				if (!bSmokes)
				{
					Fx.TrailStart(Now, 50 + Ship, Top, Vec(Speed, 0.0, 0.0), FunnelIs, FunnelSpan);
				}
				Fx.TrailMove(Now, 50 + Ship, Top, Vec(Speed, 0.0, 0.0), Now >= PowerAt ? PowerTo : Power0);
			}
			bSmokes = true;
			if (!bLit && BurnAcross > 0.0)
			{
				Fx.Burn(0.0, ShipAt(2, 0.0) + Vec(-20.0, 0.0, DeckUp), BurnAcross, 900.0, 7, true);
				bLit = true;
			}
			if (bLit)
			{
				Fx.BurnMove(Now, 7, ShipAt(2, Now) + Vec(-20.0, 0.0, DeckUp), Vec(Speed, 0.0, 0.0));
			}
			while (Now >= NextFire)
			{
				// (four turrets, the after two a tenth of a second late)
				for (int T = 0; T < 4; ++T)
				{
					const double Along = (T < 2 ? 55.0 - 25.0 * T : -30.0 - 25.0 * (T - 2));
					Fx.Muzzle(NextFire + (T >= 2 ? 0.1 : 0.0), ShipAt(0, NextFire) + Vec(Along, -14.0, DeckUp + 3.0), (ToEye + Vec(0.0, 0.0, 0.25)).Unit(), Gun.c_str(), "water");
				}
				NextFire = Every > 0.0 ? NextFire + Every : 1.0e9;
			}
			if (!bFell && Now >= FallAt)
			{
				bFell = true;
				Fx.Shell(FallAt, ShipAt(1, FallAt) + Vec(-60.0, -150.0, 0.0), "bomb500", "water", false);
				Fx.Shell(FallAt + 0.15, ShipAt(1, FallAt) + Vec(40.0, -220.0, 0.0), "bomb250", "water", false);
				Fx.Shell(FallAt + 0.3, ShipAt(1, FallAt) + Vec(130.0, -120.0, 0.0), "how150", "water", false);
			}
			if (!bHit && Now >= HitAt)
			{
				// (and she is hit: a heavy shell on her deck forward, then a light one on her armour aft)
				bHit = true;
				Fx.Shell(HitAt, ShipAt(1, HitAt) + Vec(0.32 * Long, -0.05 * Long, DeckUp), "bomb250", "town", false);
				Fx.Shell(HitAt + 0.4, ShipAt(1, HitAt + 0.4) + Vec(-0.25 * Long, -0.072 * Long, 6.0), "light", "town", true);
			}
		};
		std::sort(Frames.begin(), Frames.end());
		int Drawn = 0;
		const std::vector<Vec> Nobody;
		const double Grey[3] = { 0.16, 0.18, 0.2 };
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; ++Drawn)
			{
				Play(Drawn / 30.0);
				if (bFollow)
				{
					View.Eye = Eye0 + Vec(Speed * (Drawn / 30.0), 0.0, 0.0);
				}
				Fx.Draw(Drawn / 30.0, View);
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			Play(Now);
			if (bFollow)
			{
				Cam.Eye = View.Eye = Eye0 + Vec(Speed * Now, 0.0, 0.0);
			}
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, Nobody, false, true);
			// (FX_BG=light: no sea, sky or ships, every pixel that much light (0 black, 1 white) behind the kit's pieces.
			// The same frame on black and on white gives a piece's own opacity and colour)
			const char* BgIs = std::getenv("FX_BG");
			if (BgIs)
			{
				const double Bg = std::atof(BgIs);
				std::fill(C.R.begin(), C.R.end(), Bg);
				C.G = C.R;
				C.B = C.R;
			}
			for (int Ship = 0; Ship < 3 && !std::getenv("FX_NO_SHIPS") && !BgIs; ++Ship)
			{
				// (a plain hull and a block of upperworks, so the eye has her size: FX_SHIP_LONG m (200) long)
				Box(C, Cam, ShipAt(Ship, Now), Vec(1.0, 0.0, 0.0), Long, 0.14 * Long, DeckUp, Grey, Ship);
				Box(C, Cam, ShipAt(Ship, Now) + Vec(0.05 * Long, 0.0, DeckUp), Vec(1.0, 0.0, 0.0), 0.3 * Long, 0.08 * Long, 14.0, Grey, Ship);
			}
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.22;
					C.G[I] *= 0.2;
					C.B[I] *= 0.26;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/sea-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps\n", Frame, N, NL);
		}
		return 0;
	}

	/** The sea battle's own looks of release 2 (see the top of the file): "screen", escorts laying smoke screens
	 * (ScreenLay); "splash", shells falling in the sea by their calibre and their ship's dye (SeaSplash). */
	int SeaFx(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		SeaDecks Decks; // (no decks: a flat sea)
		Fx.Setup(&Book, &DeckOrSea, &Decks, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		const bool bScreen = Shot.compare(0, 6, "screen") == 0;
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 0.8), EnvNum("FX_WIND_Y", bScreen ? 3.0 : 0.8), 0.0));
		Fx.UseSea();
		const double Dist = EnvNum("FX_DIST", bScreen ? 5000.0 : 1000.0), Speed = EnvNum("FX_KNOTS", 25.0) * 0.5144;
		const double Long = EnvNum("FX_SHIP_LONG", bScreen ? 110.0 : 200.0);
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::clamp(EnvNum("FX_LENS", 55.0), 5.0, 170.0) * 3.14159265358979 / 360.0));
		Battle::View View;
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
		if (!std::getenv("FX_NOCULL"))
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		if (const char* SunIs = std::getenv("FX_SUN"))
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				View.Sun = Vec(X, Y, Z).Unit();
			}
		}
		const bool bDusk = std::getenv("FX_DUSK") != nullptr;
		if (bDusk)
		{
			View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
			View.SunLight = Rgb(0.0f, 0.0f, 0.0f);
		}
		// (screens: FX_SCREENS escorts (1), FX_GAP m (400) apart away from the eye, each FX_STAGGER m (300) further along,
		// steaming along +X at FX_KNOTS and laying from 0 s until FX_STOP s; FX_BLACK=1 all from the funnels, alt every
		// other one; FX_FUNNEL=funnel: each funnel smokes too, FX_POWER of her power)
		const int Screens = std::clamp(static_cast<int>(EnvNum("FX_SCREENS", 1.0)), 0, 16);
		const double Gap = EnvNum("FX_GAP", 400.0), Stagger = EnvNum("FX_STAGGER", 300.0), StopAt = EnvNum("FX_STOP", 1.0e9);
		const char* BlackIs = std::getenv("FX_BLACK");
		const std::string Black = BlackIs ? BlackIs : "";
		const char* FunnelIs = std::getenv("FX_FUNNEL");
		auto EscortAt = [&](int I, double T) { return Vec(Speed * T + I * Stagger, Dist + I * Gap, 0.0); };
		// (splash: FX_SPLASHES shells (1) of FX_CAL mm (406) dyed FX_DYE, landing from FX_FALL_AT s (1) each FX_EVERY s
		// (0.3) after the last, FX_SPREAD m (90) apart along her, FX_SHORT m (120) short of a target ship FX_DIST m off;
		// FX_OLD=size: the call the scene made before, Shell on water with that size)
		const int Splashes = std::clamp(static_cast<int>(EnvNum("FX_SPLASHES", 1.0)), 0, 64);
		const double Cal = EnvNum("FX_CAL", 406.0), FallAt = EnvNum("FX_FALL_AT", 1.0), FallEvery = EnvNum("FX_EVERY", 0.3);
		const double Spread = EnvNum("FX_SPREAD", 90.0), Short = EnvNum("FX_SHORT", 120.0);
		const char* DyeIs = std::getenv("FX_DYE");
		const char* OldIs = std::getenv("FX_OLD");
		int Fallen = 0;
		bool bStopped = false;
		auto Play = [&](double Now)
		{
			if (bScreen)
			{
				for (int I = 0; I < Screens; ++I)
				{
					const Vec Ship = EscortAt(I, Now);
					const bool bBlack = Black == "1" || (Black == "alt" && I % 2 == 1);
					if (Now < StopAt)
					{
						Fx.ScreenLay(Now, 70 + I, Ship + Vec(-0.5 * Long, 0.0, 3.0), Vec(Speed, 0.0, 0.0), bBlack);
					}
					if (FunnelIs)
					{
						if (Now == 0.0)
						{
							Fx.TrailStart(Now, 90 + I, Ship + Vec(0.05 * Long, 0.0, 12.0), Vec(Speed, 0.0, 0.0), FunnelIs, 3.0);
						}
						Fx.TrailMove(Now, 90 + I, Ship + Vec(0.05 * Long, 0.0, 12.0), Vec(Speed, 0.0, 0.0), EnvNum("FX_POWER", 0.85));
					}
				}
				if (Now >= StopAt && !bStopped)
				{
					bStopped = true;
					for (int I = 0; I < Screens; ++I)
					{
						Fx.ScreenLayStop(Now, 70 + I);
					}
				}
				return;
			}
			while (Fallen < Splashes && Now >= FallAt + Fallen * FallEvery)
			{
				const double T = FallAt + Fallen * FallEvery;
				const Vec At(-0.5 * Spread * (Splashes - 1) + Spread * Fallen, Dist - Short * (Fallen % 2 == 0 ? 1.0 : -0.6), 0.0);
				if (OldIs)
				{
					Fx.Shell(T, At, OldIs, "water", false);
				}
				else
				{
					Fx.SeaSplash(T, At, Cal, DyeIs ? DyeIs : "");
				}
				++Fallen;
			}
		};
		// (the eye: FX_DIST m off, abeam, FX_EYE_H m (24) up, looking FX_LOOK_H m (screens 20, splash 25) up at the
		// middle of what the screens have laid by then, or at the target ship; FX_LOOK_X overrides)
		auto Aim = [&](double Now)
		{
			const double LookX = std::getenv("FX_LOOK_X") ? EnvNum("FX_LOOK_X", 0.0) : (bScreen ? 0.5 * Speed * std::min(Now, StopAt) - 0.5 * Long : 0.0);
			Cam.Eye = Vec(LookX, 0.0, EnvNum("FX_EYE_H", 24.0));
			Cam.Forward = (Vec(LookX, Dist, EnvNum("FX_LOOK_H", bScreen ? 20.0 : 25.0)) - Cam.Eye).Unit();
			Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
			Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
			View.Eye = Cam.Eye;
			View.Right = Cam.Right;
			View.Up = Cam.Up;
			View.Forward = Cam.Forward;
		};
		std::sort(Frames.begin(), Frames.end());
		int Drawn = 0;
		const std::vector<Vec> Nobody;
		const double Grey[3] = { 0.16, 0.18, 0.2 };
		const int Step = std::max(1, static_cast<int>(EnvNum("FX_STEP", 1.0))); // (FX_STEP=n: the frames between drawn n at a time, as a slow game)
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; Drawn += Step)
			{
				Play(Drawn / 30.0);
				Aim(Drawn / 30.0);
				Fx.Draw(Drawn / 30.0, View);
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			Play(Now);
			Aim(Now);
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, Nobody, false, true);
			const char* BgIs = std::getenv("FX_BG");
			if (BgIs)
			{
				const double Bg = std::atof(BgIs);
				std::fill(C.R.begin(), C.R.end(), Bg);
				C.G = C.R;
				C.B = C.R;
			}
			if (!std::getenv("FX_NO_SHIPS") && !BgIs)
			{
				if (bScreen)
				{
					for (int I = 0; I < Screens; ++I)
					{
						Box(C, Cam, EscortAt(I, Now), Vec(1.0, 0.0, 0.0), Long, 0.1 * Long, 6.0, Grey, I);
						Box(C, Cam, EscortAt(I, Now) + Vec(0.1 * Long, 0.0, 6.0), Vec(1.0, 0.0, 0.0), 0.3 * Long, 0.07 * Long, 7.0, Grey, I);
					}
				}
				else
				{
					Box(C, Cam, Vec(0.0, Dist, 0.0), Vec(1.0, 0.0, 0.0), Long, 0.14 * Long, 10.0, Grey, 0);
					Box(C, Cam, Vec(0.05 * Long, Dist, 10.0), Vec(1.0, 0.0, 0.0), 0.3 * Long, 0.08 * Long, 14.0, Grey, 0);
				}
			}
			if (bDusk)
			{
				for (size_t I = 0; I < C.R.size(); ++I)
				{
					C.R[I] *= 0.22;
					C.G[I] *= 0.2;
					C.B[I] *= 0.26;
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/seafx-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps\n", Frame, N, NL);
		}
		return 0;
	}

	/** A raid's bomb carpets from the air scene's wide view (see the top of the file). */
	int Raid(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Ground, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		Fx.Setup(&Book, +[](void*, double, double) { return 0.0; }, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetGroundColour(&GroundPaint, nullptr);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 4.0), EnvNum("FX_WIND_Y", 1.0), 0.0));
		Fx.SetDrawBeyond(EnvNum("FX_BEYOND", Fx.AirBeyond()));
		// (the eye: FX_EYE_H m up, FX_DIST m from the carpets' middle, from FX_FROM degrees round (0: from the south, the
		// bombers crossing the view), looking FX_LOOK_H m above it)
		const double Dist = std::max(EnvNum("FX_DIST", 11000.0), 10.0), EyeH = EnvNum("FX_EYE_H", 4700.0), From = EnvNum("FX_FROM", 0.0) * 3.14159265358979 / 180.0;
		const double Level = std::sqrt(std::max(Dist * Dist - EyeH * EyeH, 1.0));
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::clamp(EnvNum("FX_LENS", 55.0), 5.0, 170.0) * 3.14159265358979 / 360.0));
		Cam.Eye = Vec(-std::sin(From) * Level, -std::cos(From) * Level, EyeH);
		Cam.Forward = (Vec(0.0, 0.0, EnvNum("FX_LOOK_H", 100.0)) - Cam.Eye).Unit();
		// (FX_BACK_MPS: the eye draws back along its line of sight at that many metres a second, so the carpets cross the
		// scene's draw range: the hand-over from the near bursts to their far form)
		const Vec Eye0 = Cam.Eye;
		const double BackMps = EnvNum("FX_BACK_MPS", 0.0);
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0);
		if (!std::getenv("FX_NOCULL"))
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		if (const char* SunIs = std::getenv("FX_SUN"))
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				View.Sun = Vec(X, Y, Z).Unit();
			}
		}
		// (the raid: FX_STICKS bombers (22) in boxes of six, FX_BOX_GAP s (12) between boxes, each box's bombers a third of
		// a second apart, flying along +X; each drops FX_BOMBS of FX_SIZE (11 bomb500) in a stick FX_STICK_M m (300) long,
		// the first box's first bomb at FX_DROP_AT s (0.5))
		const int NSticks = std::clamp(static_cast<int>(EnvNum("FX_STICKS", 22.0)), 1, 200), NBombs = static_cast<int>(EnvNum("FX_BOMBS", 11.0));
		const char* SizeIs = std::getenv("FX_SIZE");
		const std::string Size = SizeIs ? SizeIs : "bomb500";
		const double StickM = EnvNum("FX_STICK_M", 300.0), BoxGap = EnvNum("FX_BOX_GAP", 12.0), DropAt = EnvNum("FX_DROP_AT", 0.5), Over = EnvNum("FX_OVER", -1.0);
		struct Drop
		{
			double T = 0.0;
			Vec First, Last;
		};
		std::vector<Drop> Drops;
		for (int I = 0; I < NSticks; ++I)
		{
			const int Box = I / 6, J = I % 6;
			Drop D;
			D.T = DropAt + Box * BoxGap + J * 0.35;
			const double X0 = NSticks == 1 ? -0.5 * StickM : (Box - 1.5) * 140.0 + ((J % 3) - 1) * 30.0 - 0.5 * StickM;
			const double Y = NSticks == 1 ? 0.0 : (J - 2.5) * 65.0 + (Box - 1.5) * 20.0;
			D.First = Vec(X0, Y, 0.0);
			D.Last = Vec(X0 + StickM, Y, 0.0);
			Drops.push_back(D);
		}
		// (FX_REF=shell: in place of each stick, the bombs as plain Shell calls at the moments and places the kit gives them
		// (BombStick's own sums), to show a recipe with no far row draws the same frames either way)
		const bool bRef = std::getenv("FX_REF") != nullptr;
		struct Strike
		{
			double T = 0.0;
			Vec At;
		};
		std::vector<Strike> Strikes;
		if (bRef)
		{
			double Walk = 85.0;
			Walk = Book.Find("far.bombs.walk_mps", Walk) && Walk == Walk ? std::clamp(Walk, 10.0, 400.0) : 85.0;
			const int Count = std::min(NBombs, 48);
			for (const Drop& D : Drops)
			{
				const Vec Run(D.Last.X - D.First.X, D.Last.Y - D.First.Y, 0.0);
				const double Length = std::min(Run.Length(), 5000.0);
				const Vec Way = Run.Unit(Vec(1.0, 0.0, 0.0));
				const double Gap = Count > 1 ? Length / (Count - 1) : 0.0;
				const double Takes = Over >= 0.0 && Over <= 1.0e9 ? std::min(Over, 30.0) : std::min(Length / Walk, 4.0);
				const double Dt = Count > 1 ? Takes / (Count - 1) : 0.0;
				for (int K = 0; K < Count; ++K)
				{
					Strikes.push_back(Strike{ D.T + Dt * K, D.First + Way * (Gap * K) });
				}
			}
			std::stable_sort(Strikes.begin(), Strikes.end(), [](const Strike& A, const Strike& B) { return A.T < B.T; });
		}
		size_t Called = 0, Struck = 0;
		auto Play = [&](double Now)
		{
			for (; Called < Drops.size() && Drops[Called].T <= Now; ++Called)
			{
				if (!bRef)
				{
					Fx.BombStick(Drops[Called].T, static_cast<int>(Called), Drops[Called].First, Drops[Called].Last, NBombs, Size.c_str(), Ground.c_str(), Over);
				}
			}
			for (; bRef && Struck < Strikes.size() && Strikes[Struck].T <= Now; ++Struck)
			{
				Fx.Shell(Strikes[Struck].T, Strikes[Struck].At, Size.c_str(), Ground.c_str(), false);
			}
		};
		const bool bSea = Ground == "water";
		std::sort(Frames.begin(), Frames.end());
		int Drawn = 0, Most = 0, MostAt = 0;
		const double HazeM = std::max(EnvNum("FX_HAZE_KM", 60.0), 0.1) * 1000.0;
		for (int Frame : Frames)
		{
			for (; Drawn < Frame; ++Drawn)
			{
				Play(Drawn / 30.0);
				View.Eye = Eye0 - Cam.Forward * (BackMps * (Drawn / 30.0));
				const int N = Fx.Draw(Drawn / 30.0, View);
				if (N > Most)
				{
					Most = N;
					MostAt = Drawn;
				}
			}
			Drawn = Frame + 1;
			const double Now = Frame / 30.0;
			Play(Now);
			Cam.Eye = View.Eye = Eye0 - Cam.Forward * (BackMps * Now);
			const int N = Fx.Draw(Now, View);
			if (N > Most)
			{
				Most = N;
				MostAt = Frame;
			}
			// (the ground from the air: fields, a town's roofs and streets, or the sea, in a light haze, FX_HAZE_KM (60))
			Canvas C;
			C.Init(W, H);
			const double Haze[3] = { ToLinear(196), ToLinear(206), ToLinear(218) }, Top[3] = { ToLinear(140), ToLinear(170), ToLinear(210) };
			for (int Y = 0; Y < H; ++Y)
			{
				for (int X = 0; X < W; ++X)
				{
					const size_t I = static_cast<size_t>(Y * W + X);
					const Vec Ray = Cam.Ray(X + 0.5, Y + 0.5);
					double Col[3];
					if (Ray.Z < -1e-5)
					{
						const double T = -Cam.Eye.Z / Ray.Z;
						const Vec P = Cam.Eye + Ray * T;
						const double N1 = Noise(P.X * 0.004, P.Y * 0.004), N2 = Noise(P.X * 0.03, P.Y * 0.03);
						double Base[3];
						if (bSea)
						{
							Base[0] = ToLinear(52 + 8 * N1);
							Base[1] = ToLinear(72 + 10 * N1);
							Base[2] = ToLinear(88 + 10 * N1);
						}
						else if (Ground == "town")
						{
							const bool bStreet = std::fmod(std::fabs(P.X) + 9.0, 70.0) < 9.0 || std::fmod(std::fabs(P.Y) + 7.0, 55.0) < 7.0;
							const double L = bStreet ? 108.0 : 128.0 + 30.0 * N2;
							Base[0] = ToLinear(L + 6.0);
							Base[1] = ToLinear(L);
							Base[2] = ToLinear(L - 6.0);
						}
						else
						{
							// (a patchwork of fields: grass, ploughland and stubble)
							const double Cell = std::fabs(std::sin(std::floor(P.X / 220.0) * 12.9898 + std::floor(P.Y / 150.0) * 78.233)) * 43758.5453;
							const int Kind = static_cast<int>((Cell - std::floor(Cell)) * 3.0);
							const double Fields[3][3] = { { 84, 104, 56 }, { 118, 98, 74 }, { 146, 136, 94 } };
							for (int K = 0; K < 3; ++K)
							{
								Base[K] = ToLinear(Fields[Kind][K] * (0.92 + 0.16 * N2));
							}
						}
						const double Fog = 1.0 - std::exp(-T / HazeM);
						for (int K = 0; K < 3; ++K)
						{
							Col[K] = Base[K] * (1.0 - Fog) + Haze[K] * Fog;
						}
						C.Depth[I] = (Ray * T).Dot(Cam.Forward);
					}
					else
					{
						const double Up = std::min(1.0, Ray.Z * 4.0);
						for (int K = 0; K < 3; ++K)
						{
							Col[K] = Haze[K] + (Top[K] - Haze[K]) * Up;
						}
					}
					C.R[I] = Col[0];
					C.G[I] = Col[1];
					C.B[I] = Col[2];
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/raid-%s-f%04d.ppm", OutDir.c_str(), Ground.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, the carpets' middle %.0f m from the eye\n", Frame, N, Cam.Eye.Length());
		}
		std::printf("most puffs laid in a frame: %d (frame %d)\n", Most, MostAt);
		return 0;
	}

	/** The desert dust (see the top of the file): a column driving, or a towed gun firing. */
	int Drive(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Ground, std::vector<int> Frames, int W, int H, const std::string& Shot)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		Battle Fx;
		Fx.Setup(&Book, &Swell, nullptr, 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetWind(Vec(EnvNum("FX_WIND_X", 1.6), EnvNum("FX_WIND_Y", 0.4), 0.0)); // (the game's own wind until a scene sets one)
		const double Speed = EnvNum("FX_SPEED", 10.0), Gap = EnvNum("FX_GAP", 30.0);
		const int NCars = Shot == "gun" ? 0 : std::max(1, static_cast<int>(EnvNum("FX_VEHICLES", 4.0)));
		const bool bTracked = std::getenv("FX_WHEELS") == nullptr;
		const double CarL = 6.0, CarW = 2.9, CarH = 2.4;
		auto Ground_ = [](double X, double Y) { return Vec(X, Y, Swell(nullptr, X, Y)); };
		auto CarAt = [&](int I, double T) { return Ground_(Speed * T - Gap * I, 0.0); };
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0)));
		Vec Look;
		const Vec GunAt = Ground_(0.0, 0.0);
		if (Shot == "gun")
		{
			// (a towed gun firing along +X, seen from FX_AHEAD m down its line of fire and FX_OFF m off it, at a man's eye)
			Cam.Eye = Ground_(EnvNum("FX_AHEAD", 45.0), EnvNum("FX_OFF", 6.0)) + Vec(0.0, 0.0, 1.7);
			Look = GunAt + Vec(0.0, 0.0, 0.9);
		}
		else if (Shot == "side")
		{
			Cam.Eye = Ground_(-Gap * 1.5, -EnvNum("FX_OFF", 70.0)) + Vec(0.0, 0.0, 1.7);
			Look = Ground_(-Gap * 1.5, 0.0) + Vec(0.0, 0.0, 2.0);
		}
		else if (Shot == "high")
		{
			Cam.Eye = Ground_(40.0, -40.0) + Vec(0.0, 0.0, 30.0);
			Look = Ground_(-Gap * 1.5, 0.0);
		}
		else if (Shot == "top")
		{
			// (the overview: FX_DIST m (150) off the column's middle, looking down at FX_DOWN degrees (50))
			const double Dist = EnvNum("FX_DIST", 150.0), Down = EnvNum("FX_DOWN", 50.0) * 3.14159265 / 180.0;
			Look = Ground_(-Gap * 1.5, 0.0);
			Cam.Eye = Look + Vec(0.35, -1.0, 0.0).Unit() * (Dist * std::cos(Down)) + Vec(0.0, 0.0, Dist * std::sin(Down));
		}
		else
		{
			// ("eye": a man standing FX_AHEAD m ahead of the lead and FX_OFF m off the column's line, the column coming on)
			Cam.Eye = Ground_(EnvNum("FX_AHEAD", 70.0), -EnvNum("FX_OFF", 10.0)) + Vec(0.0, 0.0, 1.7);
			Look = Ground_(-Gap, 0.0) + Vec(0.0, 0.0, 1.5);
		}
		Cam.Forward = (Look - Cam.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr;
		if (!std::getenv("FX_NOCULL")) // (as the game: what is off the screen takes no place. FX_NOCULL: everything in range is laid)
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		const double Every = EnvNum("FX_EVERY", 0.0); // (the gun fires again every this many seconds; 0: once)
		const bool bFire = Shot == "gun" || std::getenv("FX_FIRE") != nullptr;
		std::sort(Frames.begin(), Frames.end());
		const double T0 = Shot == "gun" ? -0.2 : -EnvNum("FX_RUN", 12.0);
		Fx.Draw(T0 - 0.1, View);
		double NextShot = 0.0;
		int Tick = 0;
		for (int Frame : Frames)
		{
			const double Now = Frame / 30.0;
			for (double T = T0 + Tick / 30.0; T <= Now + 1e-9; T = T0 + (++Tick) / 30.0)
			{
				for (int I = 0; I < NCars; ++I)
				{
					const Vec At = CarAt(I, T);
					for (int Side = 0; Side < 2; ++Side)
					{
						const Vec Socket = At + Vec(-CarL * 0.5, Side ? 1.2 : -1.2, 0.3);
						Fx.VehicleDust(T, I * 2 + Side, Socket, Vec(Speed, 0.0, 0.0), Ground.c_str(), bTracked);
					}
				}
				if (bFire && T >= NextShot && NextShot >= 0.0)
				{
					const Vec Muzzle = Shot == "gun" ? GunAt + Vec(2.2, 0.0, 1.0) : CarAt(0, T) + Vec(CarL * 0.5 + 2.0, 0.0, 2.0);
					Fx.Muzzle(T, Muzzle, Vec(1.0, 0.0, 0.0), std::getenv("FX_GUN_ROW") ? std::getenv("FX_GUN_ROW") : Shot == "gun" ? "at_gun" : "tank_gun", Ground.c_str()); // (FX_GUN_ROW: its row, or row:look)
					NextShot = Every > 0.0 ? NextShot + Every : -1.0;
				}
				Fx.Draw(T, View);
			}
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, std::vector<Vec>(), Ground == "snow", false, Ground == "sand");
			const double Hull[3] = { ToLinear(84), ToLinear(80), ToLinear(64) };
			for (int I = 0; I < NCars; ++I)
			{
				Box(C, Cam, CarAt(I, Now), Vec(1.0, 0.0, 0.0), CarL, CarW, CarH, Hull, I);
			}
			if (Shot == "gun")
			{
				Box(C, Cam, GunAt, Vec(1.0, 0.0, 0.0), 2.6, 1.9, 1.2, Hull, 0);
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/drive-%s-%s-f%03d.ppm", OutDir.c_str(), Shot.c_str(), Ground.c_str(), Frame);
			C.Save(Name);
			// (how much of each vehicle, or the gun, still shows through the dust in front of it)
			std::printf("frame %d: %d puffs", Frame, N);
			for (int I = 0; I < std::max(NCars, Shot == "gun" ? 1 : 0); ++I)
			{
				double Sum = 0.0;
				int Px = 0;
				for (size_t K = 0; K < C.Owner.size(); ++K)
				{
					if (C.Owner[K] == I)
					{
						Sum += C.Seen[K];
						++Px;
					}
				}
				std::printf(" | %s %d: %d px, %.0f%% seen", Shot == "gun" ? "gun" : "vehicle", I + 1, Px, Px ? 100.0 * Sum / Px : 0.0);
			}
			std::printf("\n");
		}
		return 0;
	}

	/** The air scene's ships (see the top of the file): a carrier ablaze as the scene lights her, or a carrier's light
	 * guns firing on a dive bomber, each seen as the scene's own camera sees it. */
	int Fleet(const TheaterFx::Recipe& Book, const std::string& OutDir, const std::string& Shot, std::vector<int> Frames, int W, int H)
	{
		auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
		const double Deg = 3.14159265358979 / 180.0;
		const Vec ZUp(0.0, 0.0, 1.0);
		Battle Fx;
		const char* SeedIs = std::getenv("FX_SEED");
		Fx.Setup(&Book, &FlatSea, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
		Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
		SkyFromEnv(Fx);
		Fx.SetDrawBeyond(Fx.AirBeyond()); // (as the air scene: UseAirDistance)
		const Vec Wind(EnvNum("FX_WIND_X", 3.0), EnvNum("FX_WIND_Y", -2.0), 0.0); // (the air scene's own, AirSky::Wind)
		Fx.SetWind(Wind);
		const bool bGuns = Shot == "guns";
		const double Long = EnvNum("FX_SHIP_LONG", bGuns ? 247.6 : 260.7), Beam = EnvNum("FX_SHIP_BEAM", bGuns ? 32.5 : 31.3);
		const double Turn = EnvNum("FX_SHIP_TURN", 20.0) * Deg;
		const Vec Bow(std::cos(Turn), std::sin(Turn), 0.0), Abeam(std::sin(Turn), -std::cos(Turn), 0.0);
		const Vec Ship(0.0, 0.0, 0.0);
		const double Lens = EnvNum("FX_LENS", bGuns ? 55.0 : 35.0);
		Camera Cam;
		Cam.W = W;
		Cam.H = H;
		Cam.Ppr = W / (2.0 * std::tan(std::clamp(Lens, 5.0, 170.0) * Deg * 0.5));
		const Vec Downwind = Vec(Wind.X, Wind.Y, 0.0).Unit(Abeam * -1.0);
		// (the dive bomber of the guns shot: FX_PLANE_OUT m out from her (900) and FX_PLANE_UP m up (1100) at 0 s, diving on
		// her at FX_PLANE_DIVE degrees (70) and FX_PLANE_MPS (110), from FX_PLANE_FROM degrees off her bow (60))
		const double PlaneFrom = EnvNum("FX_PLANE_FROM", 60.0) * Deg, Dive = EnvNum("FX_PLANE_DIVE", 70.0) * Deg, PlaneMps = EnvNum("FX_PLANE_MPS", 110.0);
		const Vec PlaneOut = (Bow * std::cos(PlaneFrom) + Abeam * std::sin(PlaneFrom)).Unit(Bow);
		const Vec Plane0 = Ship + PlaneOut * EnvNum("FX_PLANE_OUT", 900.0) + ZUp * EnvNum("FX_PLANE_UP", 1100.0);
		const Vec PlaneV = (PlaneOut * -std::cos(Dive) + ZUp * -std::sin(Dive)) * PlaneMps;
		auto PlaneAt = [&](double T) { return Plane0 + PlaneV * T; };
		Vec Look;
		double Away = EnvNum("FX_DIST", 0.0), Down = 0.0;
		Vec Out;
		if (bGuns)
		{
			// (AAirBattle::AimEye, the guns fire back picture: side-on to the line from her to him, the way upwind and with
			// the sun behind the eye; the look two fifths of the way to him, 4 degrees down; both in the frame with a quarter
			// to spare, never nearer than 450 m nor farther than 2.6 km)
			const Vec Line = PlaneAt(EnvNum("FX_PLANE_EYE_AT", 0.0)) - Ship;
			const Vec Across(Line.X, Line.Y, 0.0);
			const Vec Along = Across.Unit(Bow);
			const Vec Side = ZUp.Cross(Along).Unit(Abeam);
			Out = Side.Dot(Downwind * -1.0) >= 0.0 ? Side : Side * -1.0;
			const double Wide = Across.Length(), High = std::fabs(Line.Z);
			if (!(Away > 0.0))
			{
				Away = std::clamp(std::max(0.75 * Wide / std::tan(27.5 * Deg), 0.75 * High / std::tan(16.3 * Deg)), 450.0, 2600.0);
			}
			Look = Ship + Line * 0.4;
			Down = EnvNum("FX_DOWN", 4.0) * Deg;
		}
		else
		{
			// (AAirBattle::AimEye, the ships picture at sea: from upwind, the look FX_VIEW degrees (55) off the wind's way, of
			// the two such eyes the one nearer her beam; she fills about a third of the frame; 9 degrees down, the look 0.12
			// of her length over her waterline)
			const double View_ = EnvNum("FX_VIEW", 55.0) * Deg;
			double Best = -1e9;
			for (const double Sign : { 1.0, -1.0 })
			{
				const Vec Way = (Downwind * -std::cos(View_) + ZUp.Cross(Downwind) * (Sign * std::sin(View_))).Unit(Abeam);
				const double Score = std::fabs(Way.Dot(Abeam));
				if (Score > Best)
				{
					Best = Score;
					Out = Way;
				}
			}
			const double Shown = std::max(Out.Cross(Bow).Length(), 0.33) * Long;
			if (!(Away > 0.0))
			{
				Away = std::clamp(3.0 * Shown / (2.0 * std::tan(Lens * Deg * 0.5)), 450.0, 1600.0);
			}
			Look = Ship + ZUp * (0.12 * Long);
			Down = EnvNum("FX_DOWN", 9.0) * Deg;
		}
		Cam.Forward = (Out * -std::cos(Down) + ZUp * -std::sin(Down)).Unit();
		Cam.Eye = Look - Cam.Forward * Away;
		Cam.Eye.Z = std::max(Cam.Eye.Z, 40.0);
		Cam.Forward = (Look - Cam.Eye).Unit();
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
		std::printf("eye %.0f m off, %.0f m up\n", (Cam.Eye - Look).Length(), Cam.Eye.Z);
		Battle::View View;
		View.Eye = Cam.Eye;
		View.Right = Cam.Right;
		View.Up = Cam.Up;
		View.Forward = Cam.Forward;
		View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0); // (FX_KIT_PX: as the air scene tells the kit the window's pixels)
		if (!std::getenv("FX_NOCULL"))
		{
			View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
			View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
		}
		if (const char* SunIs = std::getenv("FX_SUN"))
		{
			double X = 0.0, Y = 0.0, Z = 1.0;
			if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
			{
				View.Sun = Vec(X, Y, Z).Unit();
			}
		}
		// (the carrier ablaze, AAirStage::Sink's "burn": FX_EARLY fires (2) her hits lit FX_EARLY_S s (90) before she is lost,
		// 0.45 of her beam across at her deck; then at FX_LOST_AT s (0) five fires 0.45 of her beam across along her length
		// for 1800 s (the hits' count toward the five), and her oil burning on the sea round her, 0.6 of her length
		// (OilFire "sunk", FX_OIL another kind, FX_NO_OIL=1 none))
		const double LostAt = EnvNum("FX_LOST_AT", 0.0), DeckFire = EnvNum("FX_DECK_FIRE", 9.0);
		const int Early = bGuns ? 0 : static_cast<int>(std::clamp(EnvNum("FX_EARLY", 2.0), 0.0, 5.0));
		const double EarlyS = EnvNum("FX_EARLY_S", 90.0);
		const double Shares[5] = { -0.4, 0.4, 0.0, -0.2, 0.2 };
		const double EarlyShares[5] = { 0.1, -0.3, 0.3, -0.1, 0.25 };
		const char* OilIs = std::getenv("FX_OIL");
		bool bLost = bGuns, bAblazeLit = false;
		int Lit = 0;
		// (the guns shot, ShipGuns::Gunnery: FX_MOUNTS (11) twin mounts of FX_GUN (aa25) along her decks 16 m up, port and
		// starboard by turns, each two streams of FX_RATE rounds a minute (220) at FX_MV m/s (900) in FX_NATION's colours
		// (jp), in bursts of 2.5 to 6 s with rests of 0.4 to 1.5 s and a 15-round magazine changed in 3.5 s, laid short of
		// the full lead and wandering about him, called sixty times a second)
		struct Crew { double Phase = 0.0, W1 = 0.0, W2 = 0.0, Lead = 0.0, Left = 0.0, Burst = 0.0, Rest = 0.0, Change = 0.0; bool bOn = false; };
		const bool bEscorts = std::getenv("FX_ESCORTS") != nullptr; // (in the carrier shot too: she and her escorts fire as she burns)
		const int Mounts = bGuns || bEscorts ? static_cast<int>(std::clamp(EnvNum("FX_MOUNTS", 11.0), 0.0, 30.0)) : 0;
		const char* KindIs = std::getenv("FX_GUN");
		const std::string Kind = KindIs ? KindIs : "aa25";
		const char* NationIs = std::getenv("FX_NATION");
		const std::string Nation = NationIs ? NationIs : "jp";
		const double Rate = EnvNum("FX_RATE", 220.0), Mv = EnvNum("FX_MV", 900.0), Load = EnvNum("FX_LOAD", 15.0), Change = EnvNum("FX_CHANGE", 3.5);
		const double Deck = 16.0;
		// (FX_ESCORTS=n: n more ships firing the same, 700 m apart abeam and 150 m along; FX_ABLAZE=m: a carrier ablaze m metres
		// astern of her from the first moment, five fires 0.45 of her beam across and her column (OilFire FX_OIL, ablaze, lit
		// FX_OIL_SINCE s before, 180): the room a burning carrier and her escorts' guns take together)
		const int Ships = bGuns || bEscorts ? 1 + static_cast<int>(std::clamp(EnvNum("FX_ESCORTS", 0.0), 0.0, 6.0)) : 1;
		auto ShipAt = [&](int Sh) { return Ship + Abeam * (Sh == 0 ? 0.0 : ((Sh + 1) / 2) * 700.0 * (Sh % 2 ? 1.0 : -1.0)) + Bow * (Sh * 150.0); };
		std::vector<Crew> Crews(static_cast<size_t>(Mounts * Ships));
		TheaterFx::Dice Roll(239u);
		for (Crew& C : Crews)
		{
			C.Phase = Roll.U(0.0, 6.2831853);
			C.W1 = Roll.U(0.25, 0.6);
			C.W2 = Roll.U(0.35, 0.9);
			C.Lead = Roll.U(0.5, 0.85);
			C.Left = Load;
			C.Burst = Roll.U(1.0, 4.0);
			C.Rest = Roll.U(0.0, 1.5);
		}
		const double FireFrom = EnvNum("FX_FIRE_FROM", -2.5);
		std::sort(Frames.begin(), Frames.end());
		const double Step = 1.0 / 60.0;
		double T = bGuns ? FireFrom - Step : std::min(LostAt - (Early > 0 ? EarlyS : 0.0), 0.0) - Step;
		Fx.Draw(T, View);
		for (int Frame : Frames)
		{
			const double Now = Frame / 30.0;
			while (T + Step <= Now + 1e-9)
			{
				T += Step;
				if (!bGuns)
				{
					for (; Lit < Early && T >= LostAt - EarlyS + Lit * 20.0; ++Lit)
					{
						Fx.Burn(T, Ship + Bow * (EarlyShares[Lit] * Long) + ZUp * DeckFire, Beam * 0.45, 1800.0, 239000 + Lit, true);
					}
					if (!bLost && T >= LostAt)
					{
						bLost = true;
						for (int K = 0; K < std::max(2, 5 - Early); ++K)
						{
							Fx.Burn(T, Ship + Bow * (Shares[K] * Long) + ZUp * DeckFire, Beam * 0.45, 1800.0, -1, true);
						}
						if (!std::getenv("FX_NO_OIL"))
						{
							Fx.OilFire(T, 238001, Ship, 0.6 * Long, OilIs ? OilIs : "sunk", 0.0, EnvNum("FX_OIL_SINCE", 0.0));
						}
					}
				}
				if (bGuns && !bAblazeLit && std::getenv("FX_ABLAZE"))
				{
					bAblazeLit = true;
					const Vec Burning = Ship - Bow * EnvNum("FX_ABLAZE", 1200.0);
					for (int K = 0; K < 5; ++K)
					{
						Fx.Burn(T, Burning + Bow * (Shares[K] * 260.7) + ZUp * DeckFire, 31.3 * 0.45, 1800.0, -1, true);
					}
					Fx.OilFire(T, 238001, Burning, 0.6 * 260.7, OilIs ? OilIs : "ablaze", 0.0, EnvNum("FX_OIL_SINCE", 180.0));
				}
				for (int MoAll = 0; MoAll < Mounts * Ships && T >= 0.0; ++MoAll)
				{
					const int Mo = MoAll % Mounts, Sh = MoAll / Mounts;
					Crew& C = Crews[static_cast<size_t>(MoAll)];
					bool bFiring = false;
					if (C.Change > 0.0) C.Change -= Step;
					else if (C.Rest > 0.0) C.Rest -= Step;
					else
					{
						bFiring = true;
						C.Burst -= Step;
						C.Left -= Rate / 60.0 * Step;
						if (C.Left <= 0.0)
						{
							C.Change = Change * Roll.U(0.8, 1.3);
							C.Left = Load;
							C.Burst = 0.0;
						}
						if (C.Burst <= 0.0)
						{
							C.Burst = Roll.U(2.5, 6.0);
							C.Rest = Roll.U(0.4, 1.5);
						}
					}
					const double SideK = Mo % 2 == 0 ? 1.0 : -1.0;
					const Vec Muzzle = ShipAt(Sh) + Bow * (Long * (-0.42 + 0.84 * (Mo + 0.5) / Mounts)) + Abeam * (SideK * Beam * 0.42) + ZUp * (Deck + 1.5);
					for (int K = 0; K < 2; ++K)
					{
						const int Key = (1 << 27) + (7 + Sh) * 64 + Mo * 2 + K;
						if (!bFiring)
						{
							if (C.bOn)
							{
								Fx.AirGunStop(Key);
							}
							continue;
						}
						const Vec P = PlaneAt(T);
						const double D = (P - Muzzle).Length();
						const double Tof = D / (Mv * 0.8);
						const double Lead = std::clamp(C.Lead + 0.22 * std::sin(T * C.W1 + C.Phase), 0.3, 1.1);
						const Vec Los = (P - Muzzle).Unit(ZUp);
						const Vec A1 = Los.Cross(ZUp).Unit(Abeam);
						const Vec A2 = A1.Cross(Los);
						const double Amp = std::max(12.7 * 1.3, D * 0.011);
						const Vec Off = A1 * (Amp * std::sin(T * C.W2 + C.Phase * 1.7)) + A2 * (Amp * 0.8 * std::sin(T * C.W2 * 1.37 + C.Phase * 0.6));
						const Vec Point = P + PlaneV * (Tof * Lead) + Off;
						const Vec Aim = Point + ZUp * (0.5 * 9.81 * Tof * Tof * 0.85);
						Fx.AirGun(T, Key, Muzzle + Bow * ((K - 0.5) * 0.6), Aim, Vec(), Kind.c_str(), Rate, Mv, Nation.c_str());
					}
					C.bOn = bFiring;
				}
				Fx.Draw(T, View);
			}
			const int N = Fx.Draw(Now, View);
			Battle::Lamp Lamps[2];
			const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight));
			Canvas C;
			C.Init(W, H);
			Paint(C, Cam, Lamps, NL, std::vector<Vec>(), false, true);
			if (!std::getenv("FX_NO_SHIPS"))
			{
				// (her hull to her flight deck, and the deck: so the eye has her size)
				const double HullCol[3] = { ToLinear(70), ToLinear(72), ToLinear(76) }, DeckCol[3] = { ToLinear(150), ToLinear(138), ToLinear(112) };
				Box(C, Cam, Ship, Bow, Long * 0.92, Beam * 0.8, 14.0, HullCol, 0);
				Box(C, Cam, Ship + ZUp * 14.0, Bow, Long, Beam, 2.0, DeckCol, 0);
				if (bGuns)
				{
					const double PlaneCol[3] = { ToLinear(60), ToLinear(64), ToLinear(66) };
					Box(C, Cam, PlaneAt(Now), Vec(PlaneV.X, PlaneV.Y, 0.0).Unit(Bow), 10.0, 12.7, 1.5, PlaneCol, 1);
				}
			}
			Puffs(C, Cam, Fx, N);
			char Name[256];
			std::snprintf(Name, sizeof(Name), "%s/fleet-%s-f%04d.ppm", OutDir.c_str(), Shot.c_str(), Frame);
			C.Save(Name);
			std::printf("frame %d: %d puffs, %d lamps\n", Frame, N, NL);
		}
		return 0;
	}
}

int main(int Argc, char** Argv)
{
	if (Argc < 6)
	{
		std::fprintf(stderr, "fxpreview recipe.txt out size ground frames [WxH] [back up]\n");
		return 1;
	}
	FlatRecipe Book;
	{
		std::ifstream F(Argv[1]);
		std::string Line;
		while (std::getline(F, Line))
		{
			const size_t Tab = Line.find('\t');
			if (Tab == std::string::npos)
			{
				continue;
			}
			const std::string Key = Line.substr(0, Tab), Val = Line.substr(Tab + 1);
			if (!Val.empty() && Val[0] == '=')
			{
				Book.Words[Key] = Val.substr(1);
			}
			else
			{
				Book.Numbers[Key] = std::stod(Val);
			}
		}
	}
	const std::string OutDir = Argv[2], Size = Argv[3], Ground = Argv[4];
	std::vector<int> Frames;
	{
		std::stringstream S(Argv[5]);
		std::string Part;
		while (std::getline(S, Part, ','))
		{
			Frames.push_back(std::stoi(Part));
		}
	}
	int W = 960, H = 540;
	if (Argc > 6)
	{
		std::sscanf(Argv[6], "%dx%d", &W, &H);
	}
	if (Size == "oil")
	{
		return Oil(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "fate")
	{
		return Fate(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "sea")
	{
		return Sea(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "seafx")
	{
		return SeaFx(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "raid")
	{
		return Raid(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "drive")
	{
		return Drive(Book, OutDir, Ground, Frames, W, H, Argc > 7 ? Argv[7] : "eye");
	}
	if (Size == "wreck")
	{
		return Wreck(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "fleet")
	{
		return Fleet(Book, OutDir, Ground, Frames, W, H);
	}
	if (Size == "air")
	{
		return Air(Book, OutDir, Ground, Frames, W, H, Argc > 7 ? Argv[7] : "chase");
	}
	const double Back = Argc > 7 ? std::stod(Argv[7]) : 40.0, UpBy = Argc > 8 ? std::stod(Argv[8]) : 14.0;

	Battle Fx;
	const char* SeedIs = std::getenv("FX_SEED"); // (FX_SEED=n: another throw of the dice, for the same hit seen several ways)
	Fx.Setup(&Book, &Swell, nullptr, SeedIs ? static_cast<uint32_t>(std::strtoul(SeedIs, nullptr, 10)) : 3u);
	Fx.SetSrgbBytes(GammaWanted()); // (FX_BYTES=srgb: as the game draws with M_BattlePuffs)
		Fx.SetLandGrade(LandGradeWanted());
		Fx.SetSoftContact(SoftWanted());
		Fx.SetFromAbove(std::getenv("FX_FROM_ABOVE") != nullptr); // (FX_FROM_ABOVE=1: as a scene seen from high above asks, game.from_above)
	SkyFromEnv(Fx);
	Fx.SetGroundColour(&GroundPaint, nullptr);
	auto EnvNum = [](const char* Name, double Default) { const char* Is = std::getenv(Name); return Is ? std::atof(Is) : Default; };
	Fx.SetWind(Vec(EnvNum("FX_WIND_X", 1.6), EnvNum("FX_WIND_Y", 0.4), 0.0)); // (the game's own wind; FX_WIND_X, FX_WIND_Y m/s another)
	auto Sized = [](double V) { return V == V ? std::min(25.0, std::max(0.01, V)) : 1.0; }; // (a wild scale in the recipe still gets a camera and its men)
	const double Scale = Sized(Book.Numbers.count("sizes." + Size + ".scale") ? Book.Numbers["sizes." + Size + ".scale"] : 1.0);
	const char* GrowIs = std::getenv("FX_FRAME_AS"); // (FX_FRAME_AS=size: frame the shot as for that size, so two sizes can be seen from one camera)
	const double FrameScale = GrowIs && Book.Numbers.count(std::string("sizes.") + GrowIs + ".scale") ? Sized(Book.Numbers[std::string("sizes.") + GrowIs + ".scale"]) : Scale;
	const double Grow = std::pow(std::max(0.6, FrameScale), 0.75);
	const Vec Hit(0.0, 0.0, Swell(nullptr, 0.0, 0.0));
	const Vec Away = Vec(-0.35, -1.0, 0.0).Unit();
	Camera Cam;
	Cam.W = W;
	Cam.H = H;
	Cam.Eye = Hit + Away * (Back * Grow) + Vec(0, 0, UpBy * Grow);
	// (FX_LOOK_HIT=m: the eye looks at the hit itself, that many metres above it, as the game's battle camera looks at its ground)
	const Vec Look = std::getenv("FX_LOOK_HIT") ? Hit + Vec(0, 0, EnvNum("FX_LOOK_HIT", 0.0)) : Hit + (Vec(0, 0, 5.0) - Away * 6.0) * Grow;
	Cam.Forward = (Look - Cam.Eye).Unit();
	Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
	Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
	Cam.Ppr = W / (2.0 * std::tan(std::atan(18.0 / 35.0))); // (a 35 mm lens, as the stills)
	if (const char* LensIs = std::getenv("FX_LENS")) // (FX_LENS=degrees: the view's angle across, as the game's 42)
	{
		Cam.Ppr = W / (2.0 * std::tan(std::clamp(std::atof(LensIs), 5.0, 170.0) * 3.14159265358979 / 360.0));
	}
	const Vec MgGun = Vec(-28.0, 12.0, Swell(nullptr, -28.0, 12.0) + EnvNum("FX_MG_UP", 0.6)); // (the "mg" shot's gun; FX_MG_UP: its muzzle that high, 0.6 m)
	if (const char* MgView = std::getenv("FX_MG_VIEW"); Size == "mg" && MgView)
	{
		// (FX_MG_VIEW=side: square to the line of fire, 30 m off it; =behind: 10 m behind the gun and 3 m up, along it)
		const Vec Fire = (Hit - MgGun).Unit();
		const Vec Mid = (MgGun + Hit) * 0.5;
		const Vec Square = Fire.Cross(Vec(0, 0, 1)).Unit();
		Cam.Eye = std::string(MgView) == "behind" ? MgGun - Fire * 10.0 + Vec(0, 0, 3.0) : Mid - Square * 30.0 + Vec(0, 0, 6.0);
		Cam.Forward = ((std::string(MgView) == "behind" ? Hit : Mid) - Cam.Eye).Unit();
		if (std::string(MgView) == "gunner")
		{
			// (=gunner: a man kneeling beside the gun, 3 m behind it and 1.5 m to its right, 1.6 m up, looking at the ground
			// under the gun's receiver: the heap of brass and the flash close to)
			const Vec Under = MgGun - Fire * 0.6 - Vec(0, 0, 0.6);
			Cam.Eye = MgGun - Fire * 3.0 - Square * 1.5 + Vec(0, 0, 1.0);
			Cam.Forward = (Under + Fire * 0.7 + Vec(0, 0, 0.25) - Cam.Eye).Unit();
		}
		if (std::string(MgView) == "flash")
		{
			// (=flash: square to the line of fire FX_FLASH_OFF m (3.5) off the muzzle and a little above it, looking just
			// ahead of it: the flash's own shape, its spikes, cone or tongue, and the dust under it)
			// (FX_FLASH_AHEAD m (0.5) along the line of fire: 2.5 with FX_FLASH_OFF=2.5 is a three-quarter view from ahead,
			// where the MG 42's star shows as a star)
			const double Off = EnvNum("FX_FLASH_OFF", 3.5);
			const double Ahead = EnvNum("FX_FLASH_AHEAD", 0.5);
			Cam.Eye = MgGun + Fire * Ahead - Square * Off + Vec(0, 0, EnvNum("FX_FLASH_UP", 0.35)); // (FX_FLASH_UP: the eye that much above the muzzle)
			Cam.Forward = (MgGun + Fire * (Ahead > 1.0 ? 0.25 : 0.55) - Cam.Eye).Unit();
		}
		if (std::string(MgView) == "game")
		{
			// (=game: the game's own battle camera as build 1720 photographed it: the eye 18.75 m from the gun and pitched
			// 22 degrees down, looking at the ground under it from square to the line of fire. Use with FX_LENS=42 and a
			// picture 1072x670, the size the game draws at. FX_GAME_FROM=degrees turns the eye round the gun;
			// FX_GAME_BACK=m sets it that far off instead, 60 for the far view)
			const double Turn = (std::getenv("FX_GAME_FROM") ? std::atof(std::getenv("FX_GAME_FROM")) : 0.0) * 3.14159265358979 / 180.0;
			const Vec Level = (Square * -std::cos(Turn) + Fire * -std::sin(Turn)).Unit();
			const Vec Foot = MgGun - Vec(0, 0, 0.6);
			const double Down = 22.0 * 3.14159265358979 / 180.0;
			const double EyeOff = std::getenv("FX_GAME_BACK") ? std::min(2000.0, std::max(2.0, std::atof(std::getenv("FX_GAME_BACK")))) : 18.75;
			// (FX_GAME_LOOK=toward,right: the eye looks at the ground that many metres nearer to it and to its right of the
			// gun's, as build 1758's west still has it: 3.6,0 with FX_GAME_BACK=13.1 puts the two riflemen where it does)
			double LookNear = 0.0, LookRight = 0.0;
			if (const char* LookIs = std::getenv("FX_GAME_LOOK"))
			{
				std::sscanf(LookIs, "%lf,%lf", &LookNear, &LookRight);
				LookNear = LookNear == LookNear ? std::min(200.0, std::max(-200.0, LookNear)) : 0.0;
				LookRight = LookRight == LookRight ? std::min(200.0, std::max(-200.0, LookRight)) : 0.0;
			}
			const Vec LookAt = Foot + Level * LookNear + Vec(0, 0, 1).Cross(Level) * LookRight;
			Cam.Eye = LookAt + Level * (EyeOff * std::cos(Down)) + Vec(0, 0, EyeOff * std::sin(Down));
			Cam.Forward = (LookAt - Cam.Eye).Unit();
		}
		Cam.Right = Cam.Forward.Cross(Vec(0, 0, 1)).Unit();
		Cam.Up = Cam.Right.Cross(Cam.Forward).Unit();
	}
	std::vector<Vec> Men;
	const double MenAt = 16.0 * std::max(1.0, std::pow(Scale, 0.6));
	for (int I = 0; I < 5; ++I)
	{
		const double A = (200.0 + 28.0 * I) * 3.14159265 / 180.0;
		const double X = std::cos(A) * MenAt, Y = std::sin(A) * MenAt;
		Men.push_back(Vec(X, Y, Swell(nullptr, X, Y)));
	}
	// a man at the hit's foot too, just off the crater's lip on the camera's side
	{
		const double Crater = Book.Numbers["sizes." + Size + ".crater_m"];
		const double Off = (Crater == Crater ? std::min(200.0, std::max(0.0, Crater)) : 1.5) * 0.6 + 2.0; // (and a man, whatever the crater's number)
		const Vec M = Hit + Away * Off + Cam.Right * 1.0;
		Men.push_back(Vec(M.X, M.Y, Swell(nullptr, M.X, M.Y)));
	}

	// (FX_CREW=1, the "mg" shot: two riflemen in field grey standing at the gun as the game's test has them, 0.7 m and
	// 1.1 m behind it, 0.45 m to its left and 0.55 m to its right)
	size_t DarkFrom = static_cast<size_t>(-1);
	if (Size == "mg" && std::getenv("FX_CREW"))
	{
		const Vec Fire = (Hit - MgGun).Unit(), Square = Fire.Cross(Vec(0, 0, 1)).Unit();
		DarkFrom = Men.size();
		for (const Vec& Back_Side : { Vec(0.7, -0.45, 0.0), Vec(1.1, 0.55, 0.0) })
		{
			const Vec M = MgGun - Fire * Back_Side.X + Square * Back_Side.Y;
			Men.push_back(Vec(M.X, M.Y, Swell(nullptr, M.X, M.Y)));
		}
	}

	Battle::View View;
	View.Eye = Cam.Eye;
	View.Right = Cam.Right;
	View.Up = Cam.Up;
	View.Forward = Cam.Forward;
	View.PixelsPerRadian = Cam.Ppr * std::clamp(EnvNum("FX_KIT_PX", 1.0), 0.1, 16.0); // (FX_KIT_PX: as a scene tells the kit its window's pixels)
	if (!std::getenv("FX_NOCULL")) // (as the game: what is off the screen takes no place. FX_NOCULL: everything in range is laid)
	{
		View.HalfAcross = Cam.W / (2.0 * Cam.Ppr);
		View.HalfUp = Cam.H / (2.0 * Cam.Ppr);
	}
	if (const char* SunIs = std::getenv("FX_SUN")) // (FX_SUN=x,y,z: the way toward the sun; by default high behind the eye's left)
	{
		double X = 0.0, Y = 0.0, Z = 1.0;
		if (std::sscanf(SunIs, "%lf,%lf,%lf", &X, &Y, &Z) == 3)
		{
			View.Sun = Vec(X, Y, Z).Unit();
		}
	}
	if (const char* LightIs = std::getenv("FX_SUNLIGHT")) // (FX_SUNLIGHT=r,g,b: the sun's own light, linear; white by default)
	{
		float R = 1.0f, G = 1.0f, B = 1.0f;
		if (std::sscanf(LightIs, "%f,%f,%f", &R, &G, &B) == 3)
		{
			View.SunLight = Rgb(R, G, B);
		}
	}
	if (const char* DimIs = std::getenv("FX_DIM")) // (FX_DIM=k: a dusk, the land and sky k as light, the kit told it is dusk)
	{
		DimK = std::clamp(std::atof(DimIs), 0.02, 1.0);
		View.SmokeLight = Rgb(0.22f, 0.2f, 0.24f);
	}
	// (FX_TEST_TANK=seconds, the "mg" shot: the game's own test barrage (BattleEffects.cpp, RunTest) round the machine
	// gun. Its tank gun fires that long before the machine gun's burst (0.5 s in the test): the muzzle 4 m south and 4 m
	// east of the machine gun's and 1.4 m higher, firing on a bearing of 10 degrees where the machine gun fires on 28;
	// and the test's wind, 3 m/s toward bearing 74. FX_TANK_ROW: its muzzle row (tank_gun). The test's four eyes with
	// FX_MG_VIEW=game: FX_GAME_FROM=208 looks west (the eye east of the gun), 118 north, 28 east, -62 south)
	if (const char* TankIs = std::getenv("FX_TEST_TANK"); Size == "mg" && TankIs)
	{
		const double Lead = std::max(0.0, std::atof(TankIs));
		const Vec Fire = (Hit - MgGun).Unit(), Square = Fire.Cross(Vec(0, 0, 1)).Unit();
		const double C28 = std::cos(28.0 * 3.14159265358979 / 180.0), S28 = std::sin(28.0 * 3.14159265358979 / 180.0);
		const Vec North = Fire * C28 - Square * S28, East = Fire * S28 + Square * C28;
		// (FX_TANK_UP: that much higher, as on a rise. FX_TANK_NORTH, FX_TANK_EAST: that many metres off its place, which
		// throws its dice anew: a shot's dice go by its place to a tenth of a metre)
		const Vec Muzzle = MgGun - North * (4.0 - EnvNum("FX_TANK_NORTH", 0.0)) + East * (4.0 + EnvNum("FX_TANK_EAST", 0.0)) + Vec(0, 0, 1.4 + EnvNum("FX_TANK_UP", 0.0));
		const Vec Way = (North * 52.0 + East * 9.0 - Vec(0, 0, 1.0)).Unit();
		const char* Row = std::getenv("FX_TANK_ROW");
		Fx.SetWind((North * 0.268 + East * 0.963) * 3.0);
		Fx.Draw(-0.12 - Lead, View);
		Fx.Muzzle(-0.02 - Lead, Muzzle, Way, Row ? Row : "tank_gun", Ground.c_str());
	}
	Fx.Draw(-0.1, View); // (the eye is known before the hit, as in the game)
	if (Size == "mg")
	{
		// (a machine gun's burst from 30 m off to the left into the ground by the hit: close tracer and strikes)
		const Vec Gun = MgGun;
		// (FX_MG_GUN=row or row:look, the gun named as the game names it, mg by default; FX_MG_ROUNDS, FX_MG_RATE its burst)
		const char* MgGunIs = std::getenv("FX_MG_GUN");
		const std::string MgRow = MgGunIs ? MgGunIs : "mg";
		const int MgRounds = static_cast<int>(EnvNum("FX_MG_ROUNDS", 25.0));
		Fx.Muzzle(-0.02, Gun, (Hit - Gun).Unit(), MgRow.c_str(), Ground.c_str()); // (on its ground, as the game's: a prone kick off snow is snow)
		Fx.Shots(-0.02, 13, Gun, Hit + Vec(0.0, 0.0, 0.1), MgRow.c_str(), MgRounds, EnvNum("FX_MG_RATE", 15.0), Ground.c_str());
	}
	// (FX_BARRAGE=n: n shells in a row across the view, FX_BARRAGE_STEP m apart (5) and FX_BARRAGE_GAP s apart (1.5), the
	// first at the hit's own moment: a barrage's dust hanging and drifting, as the game's test fires one. With FX_WIND_X,
	// FX_WIND_Y toward the eye, the eye stands downwind in it)
	const int Barrage = Size == "mg" ? 0 : std::max(0, static_cast<int>(EnvNum("FX_BARRAGE", 0.0)));
	const double BarrageStep = EnvNum("FX_BARRAGE_STEP", 5.0), BarrageGap = std::max(0.05, EnvNum("FX_BARRAGE_GAP", 1.5));
	int BarrageFired = 0;
	auto FireDue = [&](double T)
	{
		for (; BarrageFired < Barrage && BarrageFired * BarrageGap - 0.02 <= T; ++BarrageFired)
		{
			Vec At = Hit + Cam.Right * (BarrageStep * (BarrageFired - (Barrage - 1) * 0.5));
			At.Z = Swell(nullptr, At.X, At.Y);
			Fx.Shell(BarrageFired * BarrageGap - 0.02, At, Size.c_str(), Ground.c_str(), false);
		}
	};
	if (Size != "mg" && Barrage == 0 && !std::getenv("FX_NOHIT")) // (FX_NOHIT: the same shot with no hit, the ground alone, to measure a burst against)
	{
		// (FX_SHELL_AT: the shell's own moment, -0.02 s by default, so frame 0 can be set at any age of the burst.
		// FX_SHELL_ROUND=n: the hit n tenths of a metre aside, which is another round by the kit's dice)
		Fx.Shell(EnvNum("FX_SHELL_AT", -0.02), Hit + Vec(0.1, 0.0, 0.0) * EnvNum("FX_SHELL_ROUND", 0.0), Size.c_str(), Ground.c_str(), false);
	}
	std::sort(Frames.begin(), Frames.end());
	int Drawn = 0;
	for (int Frame : Frames)
	{
		// (every frame before the picture is drawn too, as the game draws every frame: a flash's lamp goes by frames)
		for (; Drawn < Frame; ++Drawn)
		{
			FireDue(Drawn / 30.0);
			Fx.Draw(Drawn / 30.0, View);
		}
		Drawn = Frame + 1;
		const double Now = Frame / 30.0;
		FireDue(Now);
		const Battle::Jolt J = Fx.Shake(Now, Cam.Eye);
		Camera Shot = Cam;
		Shot.Eye = Cam.Eye + J.Offset;
		View.Eye = Shot.Eye;
		const int N = Fx.Draw(Now, View);
		Battle::Lamp Lamps[2];
		const int NL = Fx.Lamps(Now, Lamps, 2, Fx.DayOf(View.SmokeLight)); // (a plain day: the lamp at its day share)
		Canvas C;
		C.Init(W, H);
		Paint(C, Shot, Lamps, NL, Men, Ground == "snow", false, std::getenv("FX_PALE") != nullptr, DarkFrom); // (FX_PALE: pale dry ground, as Idaho's in build 1720)
		Puffs(C, Shot, Fx, N);
		char Name[256];
		std::snprintf(Name, sizeof(Name), "%s/%s-%s-f%03d.ppm", OutDir.c_str(), Size.c_str(), Ground.c_str(), Frame);
		C.Save(Name);
		std::printf("frame %d: %d puffs, %d lamps, jolt %.3f m\n", Frame, N, NL, J.Offset.Length());
	}
	return 0;
}
