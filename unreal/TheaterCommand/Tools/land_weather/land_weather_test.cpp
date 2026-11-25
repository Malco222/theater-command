// The cloud test of rain and snow seen falling in the ground battle (release 2, the look bible's G10): the battle's pick
// of what falls (Battle/LandWeather), the drops' layers and meshes (Effects/WeatherFallCore) and the graphics chip's own
// arithmetic (Effects/WeatherFallChip.inl, built here as C++ by weather_fall_chip.h), checked against each other:
//   - what falls, for every word and sky the campaign draws, and the switch's stand-in (-TheaterLandFall=);
//   - Content/Effects/land-weather.json says what the code's standing numbers say;
//   - each layer comes round exactly in its period (the clock given within it never jumps a drop);
//   - every drop stays in its box about the eye, and stays put in the world as the mesh is set down box by box;
//   - drops fall at their speed and with the wind, streaks along their way, flakes square to the eye;
//   - none shows near the lens or at the box's edge, a strength shows that share of them, none is drawn under a pixel
//     and a quarter wide;
//   - how many show in a picture, and how many pixels they cover (the Mac's cost, before the Mac).
// Usage: land_weather_test <Content/Effects/land-weather.json>

#include "Battle/LandJson.h"
#include "Battle/LandWeather.h"
#include "Effects/WeatherFallCore.h"
#include "weather_fall_chip.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace
{
	namespace C = WeatherFallChipCpp;

	int Fails = 0;
	int Checks = 0;

	void Check(bool bGood, const std::string& What)
	{
		++Checks;
		if (!bGood)
		{
			++Fails;
			std::printf("  FAIL: %s\n", What.c_str());
		}
	}

	std::string Said(const char* Pattern, double A = 0.0, double B = 0.0, double D = 0.0)
	{
		char Buf[512];
		std::snprintf(Buf, sizeof(Buf), Pattern, A, B, D);
		return Buf;
	}

	C::float4 Four(const float (&V)[4])
	{
		return C::float4(V[0], V[1], V[2], V[3]);
	}

	/** One point of one drop as the chip places it (metres, the mesh's own frame). */
	C::FFallOut PointOf(const WeatherFall::Mesh& M, int Drop, int Corner, const WeatherFall::Params& P, const C::float3& Eye)
	{
		const size_t K = static_cast<size_t>(Drop) * 4 + static_cast<size_t>(Corner);
		const C::float3 Home(M.Points[K * 3], M.Points[K * 3 + 1], M.Points[K * 3 + 2]);
		const C::float2 Cor(M.Corners[K * 2], M.Corners[K * 2 + 1]);
		const C::float4 Own(M.Drops[K * 4], M.Drops[K * 4 + 1], M.Drops[K * 4 + 2], M.Drops[K * 4 + 3]);
		C::FFallChip Chip;
		Chip.Pad = 0.0f;
		return Chip.Vertex(Home, Cor, Own, Eye, Four(P.Box), Four(P.Move), Four(P.Look), Four(P.Lens));
	}

	struct Shown
	{
		C::float3 Corner[4];
		C::float3 Mid;
		float Seen = 0.0f;
	};

	Shown DropOf(const WeatherFall::Mesh& M, int Drop, const WeatherFall::Params& P, const C::float3& Eye)
	{
		Shown S;
		C::float3 Sum(0.0f, 0.0f, 0.0f);
		for (int K = 0; K < 4; ++K)
		{
			const C::FFallOut Out = PointOf(M, Drop, K, P, Eye);
			S.Corner[K] = Out.At;
			Sum = Sum + Out.At;
			S.Seen = Out.Look.x;
		}
		S.Mid = Sum * 0.25f;
		return S;
	}

	double Dist(const C::float3& A, const C::float3& B)
	{
		return C::length(A - B);
	}

	// ------------------------------------------------------------------ what falls

	void TestPick()
	{
		std::printf("What falls (LandWeather::Pick):\n");
		using LandWeather::Asked;
		auto Pick = [](const std::string& Word, double Cover, bool bDeck, double Snow = 0.0, const std::string& Look = "bocage", const std::string& Forced = "")
		{
			Asked A;
			A.Word = Word;
			A.Cover = Cover;
			A.bDeck = bDeck;
			A.SnowLying = Snow;
			A.Look = Look;
			if (!Forced.empty()) LandWeather::ReadForced(Forced, A.ForcedKind, A.ForcedStrength);
			return LandWeather::Pick(A, LandWeather::Book());
		};
		const LandWeather::Weather Clear = Pick("clear", 0.2, false);
		Check(Clear.Kind == WeatherFall::Kind::None, "a clear sky: none falls");
		Check(Pick("", 0.9, true).Kind == WeatherFall::Kind::None, "no campaign word: none falls");
		Check(Clear.HazeTimes == 1.0 && Clear.LightTimes == 1.0 && Clear.GroundRough < 0.0, "a clear sky: the day as it was");
		const LandWeather::Weather Light = Pick("rain", 0.75, true), Heavy = Pick("rain", 0.95, true);
		Check(Light.Kind == WeatherFall::Kind::Rain && Light.Strength >= 0.45 && Light.Strength < 0.7, Said("rain under the ticket's lightest deck, 0.75: rain a little over 0.45 (%.2f)", Light.Strength));
		Check(Heavy.Kind == WeatherFall::Kind::Rain && Heavy.Strength > 0.99, Said("rain under the heaviest, 0.95: a downpour (%.2f)", Heavy.Strength));
		Check(Heavy.Wet > Light.Wet && Light.Wet >= 0.55, Said("the harder it rains the wetter the ground (%.2f, %.2f)", Light.Wet, Heavy.Wet));
		Check(Heavy.HazeTimes > Light.HazeTimes && Light.HazeTimes > 1.0, "the harder it rains the thicker the haze");
		Check(Heavy.LightTimes < Light.LightTimes && Heavy.LightTimes >= 0.8, "and the duller the light, never much");
		Check(Heavy.GroundRough < 0.95 && Heavy.GroundSpecular > 0.15, "wet ground shines more than dry");
		Check(Heavy.Sound > Light.Sound && Light.Sound > 0.0, "the rain heard, louder as it falls harder");
		Check(Pick("rain", 0.85, true, 0.5).Kind == WeatherFall::Kind::Snow, "rain's sky over snow lying: it snows");
		Check(Pick("rain", 0.85, true, 0.5).Wet == 0.0, "and the ground is not wet");
		Check(Pick("rain", 0.85, true, 0.0, "jungle").Strength > Pick("rain", 0.85, true).Strength, "a tropical rain heavier");
		Check(Pick("rain", 0.85, true, 0.0, "desert").Strength < Pick("rain", 0.85, true).Strength, "rain over the desert lighter");
		Check(Pick("snow", 0.85, true, 0.8).Kind == WeatherFall::Kind::None, "the snow months under a 0.85 deck: the snow lies, none falls");
		Check(Pick("snow", 0.95, false, 0.8).Kind == WeatherFall::Kind::None, "the snow months under scattered cloud: none falls");
		const LandWeather::Weather Snow = Pick("snow", 0.95, true, 0.8);
		Check(Snow.Kind == WeatherFall::Kind::Snow && Snow.Strength > 0.35, Said("the snow months under a 0.95 deck: snow falls (%.2f)", Snow.Strength));
		Check(Snow.Wet == 0.0 && Snow.GroundRough < 0.0 && Snow.Sound == 0.0, "snow leaves things dry and quiet");
		Check(Snow.HazeTimes > 1.0 && Snow.HazeInk[2] > Snow.HazeInk[0], "snow's haze thicker and white-blue");
		Check(Pick("mud", 0.7, true).Kind == WeatherFall::Kind::None, "the thaw's mud under a 0.7 deck: none falls");
		const LandWeather::Weather Drizzle = Pick("mud", 0.79, true);
		Check(Drizzle.Kind == WeatherFall::Kind::Rain && Drizzle.Strength < 0.45, Said("the thaw's mud under its heaviest deck: a drizzle (%.2f)", Drizzle.Strength));
		// The switch's stand-in, whatever the day.
		const LandWeather::Weather Forced = Pick("clear", 0.1, false, 0.0, "bocage", "rain,0.8");
		Check(Forced.Kind == WeatherFall::Kind::Rain && std::fabs(Forced.Strength - 0.8) < 1e-9, "-TheaterLandFall=rain,0.8 on a clear day: rain at 0.8");
		Check(Pick("rain", 0.95, true, 0.0, "bocage", "none").Kind == WeatherFall::Kind::None, "-TheaterLandFall=none in a downpour: none");
		const LandWeather::Weather Alone = Pick("clear", 0.1, false, 0.6, "bocage", "0.6");
		Check(Alone.Kind == WeatherFall::Kind::Snow && std::fabs(Alone.Strength - 0.6) < 1e-9, "-TheaterLandFall=0.6 over snow lying: snow at 0.6");
		Check(Pick("clear", 0.1, false, 0.0, "bocage", "0.6").Kind == WeatherFall::Kind::Rain, "-TheaterLandFall=0.6 elsewhere: rain");
		Check(Pick("clear", 0.1, false, 0.0, "bocage", "Snow, 0.5").Kind == WeatherFall::Kind::Snow, "-TheaterLandFall=Snow, 0.5 read loosely");
		Check(Pick("clear", 0.1, false, 0.0, "bocage", "rain,0").Kind == WeatherFall::Kind::None, "-TheaterLandFall=rain,0: none");
		// The field's wet ground, before the field knows its snow.
		{
			Asked A;
			A.Word = "rain";
			A.Cover = 0.9;
			A.bDeck = true;
			A.SnowLying = 0.9;
			Check(LandWeather::RainOnField(A, LandWeather::Book()) > 0.55, "the field's wet ground under rain (its own snow lying is the field's to judge)");
			A.Word = "snow";
			Check(LandWeather::RainOnField(A, LandWeather::Book()) == 0.0, "no wet ground under the snow months' sky");
		}
		// The same battle, the same weather.
		Check(Pick("rain", 0.83, true).Strength == Pick("rain", 0.83, true).Strength, "the same battle has the same weather");
		// A number from the recipe is taken.
		{
			Asked A;
			A.Word = "rain";
			A.Cover = 0.7;
			A.bDeck = true;
			const LandWeather::Weather Own = LandWeather::Pick(A, [](const char* Path, double& Out)
			{
				if (std::string(Path) == "pick.rain.least") { Out = 0.3; return true; }
				return false;
			});
			Check(std::fabs(Own.Strength - 0.3) < 1e-9, Said("the recipe's pick.rain.least taken (%.2f)", Own.Strength));
		}
		std::printf("  rain at the ticket's decks 0.75, 0.85, 0.95: %.2f, %.2f, %.2f; the thaw's drizzle %.2f; snow under a 0.95 deck %.2f\n",
			Light.Strength, Pick("rain", 0.85, true).Strength, Heavy.Strength, Drizzle.Strength, Snow.Strength);
	}

	// ------------------------------------------------------------------ the recipe file

	void Flatten(const LandJson::Json& J, const std::string& Path, std::map<std::string, double>& Out)
	{
		if (J.IsObject())
		{
			for (size_t K = 0; K < J.Keys.size(); ++K)
			{
				Flatten(J.Values[K], Path.empty() ? J.Keys[K] : Path + "." + J.Keys[K], Out);
			}
		}
		else if (J.IsNumber())
		{
			Out[Path] = J.Value;
		}
	}

	bool SameLayer(const WeatherFall::Layer& A, const WeatherFall::Layer& B)
	{
		return A.Name == B.Name && A.Box == B.Box && A.Density == B.Density && A.Fall == B.Fall && A.Spread == B.Spread && A.WindShare == B.WindShare && A.Width == B.Width &&
			A.Length == B.Length && A.Alpha == B.Alpha && A.Tone == B.Tone && A.NearFrom == B.NearFrom && A.NearFull == B.NearFull && A.FarFade == B.FarFade &&
			A.Flutter == B.Flutter && A.Order == B.Order;
	}

	void TestRecipeFile(const std::string& Path)
	{
		std::printf("The recipe file (%s):\n", Path.c_str());
		std::string Text, Error;
		LandJson::Json Root;
		if (!LandJson::ReadFile(Path, Text) || !LandJson::Parse(Text, Root, Error))
		{
			Check(false, "the recipe file read: " + Error);
			return;
		}
		std::map<std::string, double> Numbers;
		Flatten(Root, "", Numbers);
		const LandWeather::Book Book = [&Numbers](const char* Key, double& Out)
		{
			auto It = Numbers.find(Key);
			if (It == Numbers.end()) return false;
			Out = It->second;
			return true;
		};
		// The drops: the file's layers are the standing ones.
		const WeatherFall::Recipe Standing = WeatherFall::Recipe::Standing();
		WeatherFall::Recipe Read = WeatherFall::Recipe::Standing();
		Read.ReadOver([&Book](const char* Key, double& Out) { return Book((std::string("fall.") + Key).c_str(), Out); });
		Check(Read.Rain.size() == Standing.Rain.size() && Read.Snow.size() == Standing.Snow.size(), "the file keeps every layer");
		for (size_t K = 0; K < std::min(Read.Rain.size(), Standing.Rain.size()); ++K)
			Check(SameLayer(Read.Rain[K], Standing.Rain[K]), "the file's rain layer " + Standing.Rain[K].Name + " is the standing one");
		for (size_t K = 0; K < std::min(Read.Snow.size(), Standing.Snow.size()); ++K)
			Check(SameLayer(Read.Snow[K], Standing.Snow[K]), "the file's snow layer " + Standing.Snow[K].Name + " is the standing one");
		Check(std::fabs(Read.RainStep - Standing.RainStep) < 1e-6 && std::fabs(Read.SnowStep - Standing.SnowStep) < 1e-6 && Read.LeastPx == Standing.LeastPx &&
			Read.StrengthAlpha == Standing.StrengthAlpha && Read.StrengthLength == Standing.StrengthLength && Read.MostDrops == Standing.MostDrops, "the file's steps, least pixels, strength alpha and length and most drops are the standing ones");
		// The pick and the day: the file's numbers give what the standing ones give, everywhere.
		int Differ = 0;
		for (const char* Word : { "clear", "rain", "snow", "mud", "" })
			for (double Cover = 0.0; Cover <= 1.0001; Cover += 0.05)
				for (const bool bDeck : { false, true })
					for (const double Snow : { 0.0, 0.29, 0.31, 0.8 })
						for (const char* Look : { "bocage", "jungle", "desert", "drysteppe" })
						{
							LandWeather::Asked A;
							A.Word = Word;
							A.Cover = Cover;
							A.bDeck = bDeck;
							A.SnowLying = Snow;
							A.Look = Look;
							const LandWeather::Weather X = LandWeather::Pick(A, Book), Y = LandWeather::Pick(A, LandWeather::Book());
							const bool bSame = X.Kind == Y.Kind && std::fabs(X.Strength - Y.Strength) < 1e-9 && std::fabs(X.Wet - Y.Wet) < 1e-9 && std::fabs(X.HazeTimes - Y.HazeTimes) < 1e-9 &&
								std::fabs(X.HazeToward - Y.HazeToward) < 1e-9 && std::fabs(X.LightTimes - Y.LightTimes) < 1e-9 && std::fabs(X.SkyTimes - Y.SkyTimes) < 1e-9 && std::fabs(X.GroundRough - Y.GroundRough) < 1e-9 &&
								std::fabs(X.GroundSpecular - Y.GroundSpecular) < 1e-9 && std::fabs(X.ThingRough - Y.ThingRough) < 1e-9 && std::fabs(X.SteelRough - Y.SteelRough) < 1e-9 &&
								std::fabs(X.InkOverHaze - Y.InkOverHaze) < 1e-9 && std::fabs(X.HazeInk[0] - Y.HazeInk[0]) < 1e-9 && std::fabs(X.HazeInk[2] - Y.HazeInk[2]) < 1e-9;
							Differ += bSame ? 0 : 1;
						}
		Check(Differ == 0, Said("the file's pick and day give what the standing numbers give (%.0f skies differ)", Differ));
		// Every number in the file is one the code reads (a misspelt path would be quietly left out).
		static const char* const Known[] = { "pick.snow_lying_from", "pick.rain.from_cover", "pick.rain.full_cover", "pick.rain.least", "pick.rain.tropic_more", "pick.rain.dry_times",
			"pick.snow.from_cover", "pick.snow.full_cover", "pick.snow.least", "pick.mud.from_cover", "pick.mud.least", "pick.mud.more", "day.rain.wet_least", "day.rain.haze_more",
			"day.rain.haze_toward", "day.rain.haze_curve", "day.rain.haze_r", "day.rain.haze_g", "day.rain.haze_b", "day.rain.dimmer", "day.rain.ground_rough_less", "day.rain.ground_specular_more",
			"day.rain.thing_rough_less", "day.rain.thing_specular_more", "day.rain.steel_rough", "day.rain.steel_specular", "day.rain.ink_over_haze", "day.rain.sky_dimmer", "day.snow.haze_more",
			"day.snow.haze_toward", "day.snow.haze_r", "day.snow.haze_g", "day.snow.haze_b", "day.snow.dimmer", "day.snow.sky_dimmer", "day.snow.ink_over_haze", "fall.rain_step", "fall.snow_step",
			"fall.least_px", "fall.strength_alpha", "fall.strength_length", "fall.most_drops" };
		static const char* const LayerKeys[] = { "box_m", "density", "fall", "spread", "wind_share", "width_m", "length_m", "alpha", "tone", "near_from_m", "near_full_m", "far_fade", "flutter_m", "order", "off" };
		int Unknown = 0;
		for (const auto& [Key, Value] : Numbers)
		{
			(void)Value;
			bool bKnown = std::find_if(std::begin(Known), std::end(Known), [&Key](const char* K) { return Key == K; }) != std::end(Known);
			for (const char* Kind : { "rain", "snow" })
				for (const char* Name : { "near", "middle", "far" })
					for (const char* L : LayerKeys)
						bKnown = bKnown || Key == std::string("fall.") + Kind + "." + Name + "." + L;
			if (!bKnown)
			{
				++Unknown;
				std::printf("  a number the code never reads: %s\n", Key.c_str());
			}
		}
		Check(Unknown == 0, "every number in the file is read");
	}

	// ------------------------------------------------------------------ the drops

	struct Kit
	{
		WeatherFall::Recipe R = WeatherFall::Recipe::Standing();
	};

	void TestLayer(const WeatherFall::Recipe& R, WeatherFall::Kind K, const WeatherFall::Layer& L)
	{
		const double Step = R.StepOf(K);
		const WeatherFall::Mesh M = WeatherFall::Make(L, Step, R.MostDrops, 0x5EEDull + static_cast<uint64_t>(L.Order + 7));
		const int N = M.DropCount;
		const double Side = L.Box, Half = 0.5 * Side, Period = WeatherFall::PeriodOf(L, Step);
		const std::string Name = std::string(WeatherFall::NameOf(K)) + " " + L.Name;
		std::printf("  %s: %d drops in a %.0f m box, period %.0f s\n", Name.c_str(), N, Side, Period);
		Check(N == WeatherFall::DropsIn(L, R.MostDrops) && N > 0, Name + ": the mesh holds the layer's drops");
		Check(M.Points.size() == static_cast<size_t>(N) * 12 + 6 && M.Corners.size() == static_cast<size_t>(N) * 8 + 4 && M.Drops.size() == static_cast<size_t>(N) * 16 + 8 &&
			M.Triangles.size() == static_cast<size_t>(N) * 6, Name + ": four points and two triangles a drop, two more points for the bounds");
		bool bHome = true, bTurn = true;
		for (int I = 0; I < N; ++I)
		{
			for (int Axis = 0; Axis < 3; ++Axis) bHome = bHome && std::fabs(M.Points[static_cast<size_t>(I) * 12 + Axis]) <= Half + 1e-4;
			const double Turn = M.Drops[static_cast<size_t>(I) * 16 + 3];
			bTurn = bTurn && Turn >= static_cast<double>(I) / N - 1e-6 && Turn < static_cast<double>(I + 1) / N + 1e-6;
		}
		Check(bHome, Name + ": every drop's home in its box");
		Check(bTurn, Name + ": the drops' turns to show spread evenly (a strength shows the same share wherever the eye is)");
		// Each drop's fall a whole number of steps, so in the period it falls a whole number of boxes.
		bool bWhole = true;
		for (int I = 0; I < N; ++I)
		{
			const double Steps = M.Drops[static_cast<size_t>(I) * 16];
			const double Boxes = Steps * Step * Period / Side;
			bWhole = bWhole && std::fabs(Boxes - std::round(Boxes)) < 1e-6 && Steps >= 1.0;
		}
		Check(bWhole, Name + ": every drop falls a whole number of boxes in a period");

		const double Wind[2] = { 2.6, 1.2 };
		const double PixelAngle = (70.0 * 3.14159265358979 / 180.0) / 1920.0;
		const WeatherFall::Params Full = WeatherFall::Now(R, L, K, 1.0, Wind[0], Wind[1], 1234.5, PixelAngle);
		Check(Full.Box[1] >= 0.0f && Full.Box[1] < Full.Box[2], Name + ": the clock given within the period");
		Check(std::fabs(Full.Move[0] * Period / Side - std::round(Full.Move[0] * Period / Side)) < 1e-4 && std::fabs(Full.Move[1] * Period / Side - std::round(Full.Move[1] * Period / Side)) < 1e-4,
			Name + ": the wind carries a whole number of boxes in a period");

		// Round the period: the drops where they began.
		{
			WeatherFall::Params A = Full, B = Full;
			A.Box[1] = 0.0f;
			B.Box[1] = static_cast<float>(Period);
			const C::float3 Eye(0.3f, -1.1f, 1.7f);
			double Worst = 0.0;
			for (int I = 0; I < N; ++I)
			{
				// (all shown, for the sums: the strength's turn and the fades are other checks)
				const Shown X = DropOf(M, I, A, Eye), Y = DropOf(M, I, B, Eye);
				const C::float3 Rel = X.Mid - Eye;
				if (std::fabs(Rel.x) > 0.49 * Side || std::fabs(Rel.y) > 0.49 * Side || std::fabs(Rel.z) > 0.49 * Side) continue;
				Worst = std::max(Worst, Dist(X.Mid, Y.Mid));
			}
			Check(Worst < 5e-3, Name + Said(": a period on, every drop where it began (%.4f m at worst)", Worst));
		}

		// In its box about the eye, wherever the eye is, and put in the world (the mesh set down box by box).
		{
			double WorstBox = 0.0, WorstWorld = 0.0;
			int Compared = 0;
			for (const double Along : { 0.47, 0.53, 1.49, 1.51, -0.51, 7.5 })
			{
				// The eye in the world and the mesh's place, as FWeatherFall::Tick sets it.
				const double E1[3] = { Along * Side, 0.21 * Side, 2.0 }, E2[3] = { (Along + 0.04) * Side, 0.23 * Side, 2.4 };
				auto Grid = [Side](double V) { return std::round(V / Side) * Side; };
				const double C1[3] = { Grid(E1[0]), Grid(E1[1]), Grid(E1[2]) }, C2[3] = { Grid(E2[0]), Grid(E2[1]), Grid(E2[2]) };
				const C::float3 L1(static_cast<float>(E1[0] - C1[0]), static_cast<float>(E1[1] - C1[1]), static_cast<float>(E1[2] - C1[2]));
				const C::float3 L2(static_cast<float>(E2[0] - C2[0]), static_cast<float>(E2[1] - C2[1]), static_cast<float>(E2[2] - C2[2]));
				for (int I = 0; I < N; I += 3)
				{
					const Shown X = DropOf(M, I, Full, L1), Y = DropOf(M, I, Full, L2);
					const C::float3 R1 = X.Mid - L1;
					WorstBox = std::max({ WorstBox, std::fabs(R1.x) - Half, std::fabs(R1.y) - Half, std::fabs(R1.z) - Half });
					// (the same drop in the world from both eyes, where it is well inside both boxes)
					const double W1[3] = { C1[0] + X.Mid.x, C1[1] + X.Mid.y, C1[2] + X.Mid.z }, W2[3] = { C2[0] + Y.Mid.x, C2[1] + Y.Mid.y, C2[2] + Y.Mid.z };
					bool bInside = true;
					for (int Axis = 0; Axis < 3; ++Axis)
						bInside = bInside && std::fabs(W1[Axis] - E1[Axis]) < 0.45 * Side && std::fabs(W1[Axis] - E2[Axis]) < 0.45 * Side;
					if (!bInside) continue;
					++Compared;
					WorstWorld = std::max(WorstWorld, std::sqrt((W1[0] - W2[0]) * (W1[0] - W2[0]) + (W1[1] - W2[1]) * (W1[1] - W2[1]) + (W1[2] - W2[2]) * (W1[2] - W2[2])));
				}
			}
			Check(WorstBox < 1e-3, Name + Said(": every drop within half a box of the eye (%.4f m past it at worst)", std::max(0.0, WorstBox)));
			Check(Compared > 0 && WorstWorld < 2e-3, Name + Said(": a drop stays put in the world as the eye moves and the mesh is set down again (%.0f drops, %.4f m at worst)", Compared, WorstWorld));
		}

		// Falling at its speed, with the wind; a streak along its way, a flake square to the eye.
		{
			const double Dt = 1.0 / 60.0;
			WeatherFall::Params A = Full, B = Full;
			A.Box[1] = 10.0f;
			B.Box[1] = static_cast<float>(10.0 + Dt);
			const C::float3 Eye(0.0f, 0.0f, 1.7f);
			bool bSpeed = true, bWind = true, bAlong = true, bSquare = true;
			int Seen = 0;
			for (int I = 0; I < N; ++I)
			{
				const Shown X = DropOf(M, I, A, Eye), Y = DropOf(M, I, B, Eye);
				if (X.Seen <= 0.0f || Y.Seen <= 0.0f) continue;
				const C::float3 V = (Y.Mid - X.Mid) / static_cast<float>(Dt);
				if (C::length(V) > 50.0f) continue; // (wrapped round the box between the two)
				++Seen;
				const double Down = -V.z;
				bSpeed = bSpeed && Down >= L.Fall * (1.0 - L.Spread) - R.StepOf(K) - 1e-3 && Down <= L.Fall * (1.0 + L.Spread) + R.StepOf(K) + 1e-3;
				if (K == WeatherFall::Kind::Rain)
				{
					bWind = bWind && std::fabs(V.x - Full.Move[0]) < 0.02 && std::fabs(V.y - Full.Move[1]) < 0.02;
					const C::float3 Long = X.Corner[3] - X.Corner[0];
					const C::float3 Way = C::float3(Full.Move[0], Full.Move[1], -static_cast<float>(Down));
					bAlong = bAlong && C::dot(Long, Way) / (C::length(Long) * C::length(Way)) > 0.999f;
				}
				else
				{
					const C::float3 Facing = X.Mid - Eye;
					const float F = C::length(Facing);
					bSquare = bSquare && std::fabs(C::dot(X.Corner[1] - X.Corner[0], Facing)) / (C::length(X.Corner[1] - X.Corner[0]) * F) < 1e-3f &&
						std::fabs(C::dot(X.Corner[3] - X.Corner[0], Facing)) / (C::length(X.Corner[3] - X.Corner[0]) * F) < 1e-3f;
				}
			}
			Check(Seen > 0 && bSpeed, Name + Said(": every drop falls at its speed, %.1f m/s within its spread (%.0f drops)", L.Fall, Seen));
			if (K == WeatherFall::Kind::Rain)
			{
				Check(bWind, Name + ": and drifts with the wind");
				Check(bAlong, Name + ": a streak along its way");
			}
			else
			{
				Check(bSquare, Name + ": a flake square to the eye");
			}
		}

		// None near the lens, none at the box's edge; a strength shows that share; none under a pixel and a quarter wide.
		{
			const C::float3 Eye(0.0f, 0.0f, 1.7f);
			bool bNear = true, bEdge = true, bWide = true;
			for (int I = 0; I < N; ++I)
			{
				const Shown X = DropOf(M, I, Full, Eye);
				const double D = Dist(X.Mid, Eye);
				if (D < L.NearFrom - 1e-3) bNear = bNear && X.Seen == 0.0f;
				if (D > Half + 1e-3) bEdge = bEdge && X.Seen == 0.0f;
				if (X.Seen > 0.0f)
				{
					const double Across = Dist(X.Corner[1], X.Corner[0]);
					bWide = bWide && Across >= R.LeastPx * std::max(D, 0.05) * PixelAngle * 0.999;
				}
			}
			Check(bNear, Name + Said(": none shows nearer the lens than %.1f m", L.NearFrom));
			Check(bEdge, Name + ": none shows past half the box (nothing seen to come or go as the box is carried)");
			Check(bWide, Name + ": none drawn under a pixel and a quarter wide");
			for (const double S : { 0.25, 0.5, 0.8 })
			{
				const WeatherFall::Params P = WeatherFall::Now(R, L, K, S, Wind[0], Wind[1], 1234.5, PixelAngle);
				int Turned = 0;
				for (int I = 0; I < N; ++I) Turned += M.Drops[static_cast<size_t>(I) * 16 + 3] < P.Box[3] ? 1 : 0;
				Check(std::fabs(static_cast<double>(Turned) / N - S) <= 1.0 / N + 1e-9, Name + Said(": strength %.2f shows %.3f of the drops", S, static_cast<double>(Turned) / N));
				Check(P.Opacity < Full.Opacity, Name + ": and each fainter than at full strength");
			}
			const WeatherFall::Params None = WeatherFall::Now(R, L, K, 0.0, Wind[0], Wind[1], 1234.5, PixelAngle);
			int Any = 0;
			for (int I = 0; I < N; ++I) Any += DropOf(M, I, None, Eye).Seen > 0.0f ? 1 : 0;
			Check(Any == 0, Name + ": none at no strength");
		}
	}

	void TestShape()
	{
		std::printf("The drops' shapes (Shape):\n");
		C::FFallChip Chip;
		Chip.Pad = 0.0f;
		Check(std::fabs(Chip.Shape(C::float2(0.0f, 0.5f), 1.0f) - 1.0f) < 1e-6f, "a flake whole at its middle");
		Check(Chip.Shape(C::float2(1.0f, 0.5f), 1.0f) == 0.0f && Chip.Shape(C::float2(0.0f, 0.0f), 1.0f) == 0.0f && Chip.Shape(C::float2(0.75f, 0.9f), 1.0f) == 0.0f, "and gone at its rim");
		Check(Chip.Shape(C::float2(0.0f, 0.0f), 0.0f) == 0.0f, "a streak gone at its tail");
		Check(Chip.Shape(C::float2(0.0f, 1.0f), 0.0f) == 0.0f, "and its head soft");
		Check(Chip.Shape(C::float2(1.0f, 0.8f), 0.0f) == 0.0f && Chip.Shape(C::float2(-1.0f, 0.8f), 0.0f) == 0.0f, "and its sides");
		Check(Chip.Shape(C::float2(0.0f, 0.85f), 0.0f) > Chip.Shape(C::float2(0.0f, 0.4f), 0.0f), "brightest toward its head");
	}

	/** How many drops show in a picture and how many pixels they cover: an eye 1.7 m up looking level, the lens 70
	 * degrees across, 1920 by 1080. */
	void Picture(const WeatherFall::Recipe& R, WeatherFall::Kind K, double Strength, int& OutShown, double& OutPixels, double& OutBright, std::string* OutLayers = nullptr)
	{
		OutShown = 0;
		OutPixels = 0.0;
		OutBright = 0.0;
		const double W = 1920.0, H = 1080.0, Lens = 70.0 * 3.14159265358979 / 180.0, PixelAngle = Lens / W;
		const double Focal = 0.5 * W / std::tan(0.5 * Lens);
		const C::float3 Eye(0.0f, 0.0f, 1.7f);
		for (const WeatherFall::Layer& L : R.Of(K))
		{
			const WeatherFall::Mesh M = WeatherFall::Make(L, R.StepOf(K), R.MostDrops, 77);
			const WeatherFall::Params P = WeatherFall::Now(R, L, K, Strength, 2.6, 1.2, 100.0, PixelAngle);
			const int ShownBefore = OutShown;
			const double PixelsBefore = OutPixels;
			for (int I = 0; I < M.DropCount; ++I)
			{
				const Shown X = DropOf(M, I, P, Eye);
				if (X.Seen <= 0.0f) continue;
				// (looking along +x: across is y, up is z)
				double Sx[4], Sy[4];
				bool bFront = true;
				for (int Q = 0; Q < 4; ++Q)
				{
					const C::float3 Rel = X.Corner[Q] - Eye;
					if (Rel.x < 0.05f) { bFront = false; break; }
					Sx[Q] = 0.5 * W - Focal * Rel.y / Rel.x;
					Sy[Q] = 0.5 * H - Focal * Rel.z / Rel.x;
				}
				if (!bFront) continue;
				const double Mx = 0.25 * (Sx[0] + Sx[1] + Sx[2] + Sx[3]), My = 0.25 * (Sy[0] + Sy[1] + Sy[2] + Sy[3]);
				if (Mx < 0.0 || Mx >= W || My < 0.0 || My >= H) continue;
				double Area = 0.0;
				for (int Q = 0; Q < 4; ++Q) Area += Sx[Q] * Sy[(Q + 1) % 4] - Sx[(Q + 1) % 4] * Sy[Q];
				Area = 0.5 * std::fabs(Area);
				OutPixels += Area;
				OutShown += X.Seen > 0.05f ? 1 : 0;
				OutBright += Area * X.Seen * P.Opacity;
			}
			if (OutLayers)
			{
				char Buf[160];
				std::snprintf(Buf, sizeof(Buf), "%s%s %d (%.0f px)", OutLayers->empty() ? "" : ", ", L.Name.c_str(), OutShown - ShownBefore, OutPixels - PixelsBefore);
				*OutLayers += Buf;
			}
		}
	}

	void TestPictures(const WeatherFall::Recipe& R)
	{
		std::printf("In the picture (1920 x 1080, a 70 degree lens, 1.7 m up, looking level):\n");
		struct Case { WeatherFall::Kind K; double S; const char* Name; };
		for (const Case& Each : { Case{ WeatherFall::Kind::Rain, 0.3, "light rain" }, Case{ WeatherFall::Kind::Rain, 0.6, "rain" }, Case{ WeatherFall::Kind::Rain, 1.0, "a downpour" },
				 Case{ WeatherFall::Kind::Snow, 0.4, "light snow" }, Case{ WeatherFall::Kind::Snow, 1.0, "heavy snow" } })
		{
			int Count = 0;
			double Pixels = 0.0, Bright = 0.0;
			std::string Layers;
			Picture(R, Each.K, Each.S, Count, Pixels, Bright, &Layers);
			std::printf("  %-11s %.2f: %5d drops seen, covering %7.0f pixels (%.2f of the picture's, drawn once more where they fall), their light %.4f of a picture at full\n              (%s)\n",
				Each.Name, Each.S, Count, Pixels, Pixels / (1920.0 * 1080.0), Bright / (1920.0 * 1080.0), Layers.c_str());
			Check(Pixels < 0.6 * 1920.0 * 1080.0, std::string(Each.Name) + ": the drops cover under 0.6 of the picture (the Mac's translucent cost)");
			if (Each.S >= 1.0) Check(Count > 1200 && Count < 4000, std::string(Each.Name) + Said(": some 1,200 to 4,000 drops in the picture (%.0f)", Count));
		}
	}
}

int main(int Argc, char** Argv)
{
	TestPick();
	if (Argc > 1) TestRecipeFile(Argv[1]);
	const WeatherFall::Recipe R = WeatherFall::Recipe::Standing();
	std::printf("The drops (WeatherFallCore and the chip's own arithmetic):\n");
	for (const WeatherFall::Kind K : { WeatherFall::Kind::Rain, WeatherFall::Kind::Snow })
		for (const WeatherFall::Layer& L : R.Of(K)) TestLayer(R, K, L);
	TestShape();
	TestPictures(R);
	std::printf("%d checks, %d failed\n", Checks, Fails);
	return Fails == 0 ? 0 : 1;
}
