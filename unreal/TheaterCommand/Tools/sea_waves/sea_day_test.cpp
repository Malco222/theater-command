// THE LIGHT CORE'S CLOUD TEST (the release 2 ocean plan, step 3; section 6's sea_day_test, its step-3 parts). Battle/
// SeaDayCore.{h,cpp} held to the plan: seed mode is today bit for bit, against the game's own text cut out of its source
// (Tools/sea_waves/day_extract.py, built against day_shim.h's stand-ins), the hour, the clamp, the lamps, the warmth, the
// sky's colours, the haze and the sea material's, under each of today's sky, grade, haze and sea switches; field mode's
// rules (the sun or the moon as the key, the warmth, the shares under overcast, the exposure, the night grade, the smoke's
// hour, the stars, the day under the water, the unlit and the self-lit pieces, a moon never darkening the night and
// lifting its sky by her own light only, the day's and the dusk's not at all); no step over a tenth for a minute of sun
// from -18 to 70 degrees (with vis, the fog outside the -6 to 0 twilight band, the plan's ruling; with none, everywhere);
// the engine's height fog (its line integral, as transcribed, against a numeric one), the vis fit from every planned eye,
// the vis never a fog below -6 degrees and wholly the fog from the horizon up at every sun, the material's own haze
// exactly nothing under it, and with no vis the air scene's 30 km at 1.2 times from +2 degrees up under a fair sky, the
// weather's own air added to it under a foul one, as by night; every lookout's sighting the show makes in
// every recording and stand-in kept at Cmin at its own sun and every sun the vis is a fog, and seen through light at night
// (judged again from the look alone); the day colours of 8 in the morning to 3 in the afternoon wherever the sun is 6
// degrees up; the water by the ocean and by place; no engine, no luck and no "dice" in the core.
// Nothing here is wired into the game: step 3 is cloud only. Plain C++20, no engine.
// Usage: sea_day_test <Source/TheaterCommand> <out dir> <recording.json ...>

#include "Battle/AirFeed.h"
#include "Battle/SeaDayCore.h"
#include "Battle/SeaFeed.h"
#include "Battle/SeaShow.h"
#include "Battle/SeaUnderCore.h"

#include "day_shim.h"

namespace Today
{
#include "day_cut.inc"
}

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	using namespace SeaDay;
	int Checks = 0, Failed = 0;

	void Check(const std::string& What, bool bOk, const std::string& Detail = "")
	{
		++Checks;
		if (!bOk) ++Failed;
		std::printf("%s %s%s%s\n", bOk ? "ok  " : "FAIL", What.c_str(), Detail.empty() ? "" : ": ", Detail.c_str());
	}

	void Note(const std::string& What) { std::printf("     %s\n", What.c_str()); }

	std::string Num(double X, int Places = 3)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.*f", Places, X);
		return Text;
	}

	std::string Sci(double X)
	{
		char Text[64];
		std::snprintf(Text, sizeof(Text), "%.3g", X);
		return Text;
	}

	std::string ReadAll(const std::string& Path)
	{
		std::ifstream File(Path, std::ios::binary);
		std::stringstream Text;
		Text << File.rdbuf();
		return Text.str();
	}

	int CountOf(const std::string& Text, const std::string& What)
	{
		int N = 0;
		for (size_t At = Text.find(What); At != std::string::npos; At = Text.find(What, At + 1)) ++N;
		return N;
	}

	std::string Lower(std::string S)
	{
		for (char& C : S) C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
		return S;
	}

	constexpr double Unit = 100.0; // (ASeaBattle's UnitsPerMetre)

	/** A channel as the scene holds it (a float). */
	bool SameFloat(double Core, float Game) { return static_cast<float>(Core) == Game && Core == static_cast<double>(Game); }
	bool SameColour(const Rgb& Core, const FLinearColor& Game) { return SameFloat(Core.R, Game.R) && SameFloat(Core.G, Game.G) && SameFloat(Core.B, Game.B); }
	std::string Say3(const Rgb& C) { return Num(C.R, 4) + "/" + Num(C.G, 4) + "/" + Num(C.B, 4); }
	std::string Say3(const FLinearColor& C) { return Num(C.R, 4) + "/" + Num(C.G, 4) + "/" + Num(C.B, 4); }
	double Ratio(double A, double B) { return A > 0.0 && B > 0.0 ? std::max(A / B, B / A) : (A == B ? 1.0 : 1e9); }
	double RatioOf(const Rgb& A, const Rgb& B) { return std::max({ Ratio(A.R, B.R), Ratio(A.G, B.G), Ratio(A.B, B.B) }); }
	double Lum(const Rgb& C) { return 0.2126 * C.R + 0.7152 * C.G + 0.0722 * C.B; }
	Rgb Scale3(const Rgb& C, double S) { return { C.R * S, C.G * S, C.B * S }; }

	Fog FogOf(const Haze& H, double FogZ = 0.0)
	{
		Fog F;
		F.Density = H.Density;
		F.Falloff = H.Falloff;
		F.HeightZ = FogZ * Unit;
		F.Start = H.Start * Unit;
		F.MostOpacity = H.MostOpacity;
		return F;
	}

	std::vector<double> Planned() { return std::vector<double>(std::begin(PlannedEyes), std::end(PlannedEyes)); }

	Asked FieldAsk()
	{
		Asked A;
		A.Day = Mode::Field;
		return A;
	}

	/** A field lit by the field's own light: the sun Up high from From, the moon (below 0: down), the vis. */
	Field Lit(double Up, double From = 250.0, double MoonUp = -30.0, double MoonLit = 0.0, double Vis = 0.0, double Lat = 45.0, int Month = 6)
	{
		Field F;
		F.Lat = Lat;
		F.Lon = -30.0;
		F.Month = Month;
		F.bLight = true;
		F.SunUp = Up;
		F.bSunFrom = true;
		F.SunFrom = From;
		F.MoonUp = MoonUp;
		F.MoonFrom = 140.0;
		F.MoonLit = MoonLit;
		F.Seeing = Vis;
		return F;
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The recordings, played through the show: the field, and every sighting a lookout makes.

	struct Played
	{
		std::string Path, Name;
		SeaFeed::Recording Rec;
		uint64_t Seed = 239;
		bool bSunFrom = false;
		double SunFrom = 0.0, Wind = 0.0, Sees = 4000.0;
		std::vector<double> Sightings; // metres, flat, a boat up and a ship of the player's within the night's seeing
		int Frames = 0;
	};
	std::vector<Played> All;

	Field FieldOf(const Played& P)
	{
		const SeaShow::Field& B = P.Rec.Board;
		Field F;
		F.Lat = B.Lat;
		F.Lon = B.Lon;
		F.Month = B.Month;
		F.Seed = P.Seed;
		F.Wind = P.Wind;
		F.bLight = B.bLight;
		F.SunUp = B.SunUp;
		F.bSunFrom = P.bSunFrom;
		F.SunFrom = P.SunFrom;
		F.MoonUp = B.MoonUp;
		F.MoonFrom = B.MoonFrom;
		F.MoonLit = B.MoonLit;
		F.Seeing = B.Seeing;
		return F;
	}

	void Play(const std::vector<std::string>& Paths)
	{
		for (const std::string& Path : Paths)
		{
			Played P;
			P.Path = Path;
			P.Name = std::filesystem::path(Path).stem().string();
			std::string Error;
			const std::string Text = ReadAll(Path);
			if (!SeaFeed::ReadRecording(Text, P.Rec, Error) || P.Rec.Views.empty())
			{
				Check("recordings: " + P.Name + " read", false, Error);
				continue;
			}
			P.Seed = P.Rec.Seed ? P.Rec.Seed : 239;
			// (light.sun.from: step 4's SeaLookField reads it; the show does not)
			AirFeed::Json J;
			if (AirFeed::Parse(Text, J, Error))
			{
				const AirFeed::Json& Sun = J["field"]["light"]["sun"];
				if (Sun.IsObject() && Sun["from"].IsNumber())
				{
					P.bSunFrom = true;
					P.SunFrom = Sun["from"].Num();
				}
			}
			SeaShow::Show Show(P.Seed);
			Show.SetField(P.Rec.Board);
			const int Mine = SeaFeed::SideOf(P.Rec.Views.front().Head.Me);
			Show.SetMine(Mine);
			P.Sees = P.Rec.Board.Seeing > 0.0 ? P.Rec.Board.Seeing : 4000.0;
			size_t Taken = 0;
			auto TakeNext = [&]()
			{
				const SeaFeed::Frame& Fr = P.Rec.Views[Taken++];
				Show.Take(Fr.Head.T, Fr.Pieces, Fr.Events, Fr.bZones ? &Fr.Zones : nullptr, Fr.Head.Contact);
			};
			TakeNext();
			const double BeginT = P.Rec.Views.front().Head.T, EndT = P.Rec.Views.back().Head.T;
			// (Show::Hunt's lookout: a boat of the other side in the fight, in sight and up, within the night's seeing of a
			// ship of the player's in the line or limping, not a boat of his own dived)
			auto Under = [](const SeaShow::ShipOut& S) { return S.Dived > 1.0 && S.Dive != "surfaced"; };
			for (double T = BeginT; T <= EndT + 12.0; T += 0.5)
			{
				while (Taken < P.Rec.Views.size() && P.Rec.Views[Taken - 1].Head.T <= T + 1e-9) TakeNext();
				Show.Eye = SeaShow::Vec(0.0, -3000.0, 900.0);
				Show.Advance(T);
				++P.Frames;
				if (Mine < 0) continue;
				for (const SeaShow::ShipOut& Boat : Show.Ships())
				{
					if (Boat.Class != "ss" || Boat.Side == Mine || Boat.Doing != SeaShow::Fate::Line || !Boat.bShown || Under(Boat)) continue;
					for (const SeaShow::ShipOut& O : Show.Ships())
					{
						if (O.Side != Mine || (O.Doing != SeaShow::Fate::Line && O.Doing != SeaShow::Fate::Limping)) continue;
						if (O.Class == "ss" && Under(O)) continue;
						const double Flat = std::hypot(O.At.X - Boat.At.X, O.At.Y - Boat.At.Y);
						if (Flat <= P.Sees) P.Sightings.push_back(Flat);
					}
				}
			}
			P.Wind = Show.WindSpeed();
			All.push_back(std::move(P));
		}
		Check("recordings: every recording and stand-in read and played through the show", !All.empty() && All.size() == Paths.size(),
			std::to_string(All.size()) + " of " + std::to_string(Paths.size()));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Seed mode: today, bit for bit, against the game's own text.

	/** The scene's own sky, haze and sea material under the command line as it stands, read as step 11 reads them off the
	 * scene: SkyColour, SeaSkyLight, MovingSea's SkyHigh, HazeTop and HazeReach (after SetSeaWind gets SeaSkyLight and
	 * GradeNow, as OpenOn gives them), and the kept fog after AirHaze. */
	TodayScene SceneNow()
	{
		TodayScene S;
		S.bGiven = true;
		const FLinearColor Screen = Today::SeaParts::SkyColour(), SeaSky = Today::SeaParts::SeaSkyLight();
		S.SkyOnScreen = { Screen.R, Screen.G, Screen.B };
		S.SeaSkyLight = { SeaSky.R, SeaSky.G, SeaSky.B };
		Today::AAirStage Stage;
		Stage.UnitsPerMetre = Unit;
		Stage.SetSeaWind(0.4, 6.0, SeaSky, Today::SeaParts::GradeNow());
		FMaterialStandIn Moving;
		Stage.MovingSeaParams(&Moving);
		const FLinearColor High = Moving.Vectors["SkyHigh"];
		S.SkyHigh = { High.R, High.G, High.B };
		S.MaterialTop = Moving.Scalars["HazeTop"];
		S.MaterialReach = Moving.Scalars["HazeReach"] / Unit;
		UExponentialHeightFogComponent Room;
		Today::RoomFog(&Room, Unit);
		S.FogStart = Room.StartDistance / Unit;
		S.FogDensity = Room.FogDensity;
		S.FogFalloff = Room.FogHeightFalloff;
		S.FogMostOpacity = Room.FogMaxOpacity;
		S.FogColour = { Room.FogInscatteringColor.R, Room.FogInscatteringColor.G, Room.FogInscatteringColor.B };
		return S;
	}

	/** The command lines seed mode is held under: bare, and each switch today's sky, haze and sea material read. */
	const char* const SeedLines[] = { "", "-TheaterSeaSky=200,210,220", "-TheaterSeaSky=200,210,220 -TheaterSeaGrade=1.1,0.9", "-TheaterSeaGrade=1.1,0.9",
		"-TheaterSeaSky=190,200,215 -TheaterBattleGrade=1,1,1,1,1.12,0.84", "-TheaterNoBattleGrade -TheaterSeaSky=205,205,212", "-TheaterAirHaze=9000,,0.004",
		"-TheaterSeaHaze=0.4", "-TheaterSeaHigh=1.3", "-TheaterSeaSky=200,210,220 -TheaterSeaGrade=1.1,0.9 -TheaterAirHaze=9000,,0.004",
		"-TheaterSeaSky=200,210,220 -TheaterSeaGrade=1.1,0.9 -TheaterAirHaze=9000,,0.004 -TheaterSeaHaze=0.25 -TheaterSeaHigh=0.8" };

	void SeedIdentity()
	{
		DayShim::Line.clear();
		// The hour and the clamp: OpenOn's own lines (the seed's hour, SunAt, 12 to 70) against SeedHour and SeedSun.
		long Tried = 0, Differ = 0, Low = 0, High = 0;
		std::string First;
		for (uint64_t Seed = 0; Seed < 4000; Seed += 7)
		{
			for (double Lat = -75.0; Lat <= 75.0; Lat += 7.5)
			{
				for (int Month = 0; Month < 12; ++Month)
				{
					Today::FOpenOnRec S;
					S.Rec.Board.Lat = Lat;
					S.Rec.Board.Month = Month;
					double GameHour = 0.0, GameFrom = 0.0, GameUp = 0.0, From = 0.0, Up = 0.0;
					Today::OpenOnSun(S, Seed, GameHour, GameFrom, GameUp);
					SeaDay::SeedSun(Seed, Lat, Month, -1.0f, From, Up);
					++Tried;
					Low += GameUp == ClampLow;
					High += GameUp == ClampHigh;
					if (SeedHour(Seed) != GameHour || From != GameFrom || Up != GameUp)
					{
						if (!Differ++) First = "seed " + std::to_string(Seed) + " lat " + Num(Lat, 1) + " month " + std::to_string(Month);
					}
				}
			}
		}
		Check("seed: the hour (8 in the morning plus the seed's), the sun at it and the 12-70 degree clamp are OpenOn's own lines, bit for bit",
			Differ == 0 && Low > 0 && High > 0, std::to_string(Tried) + " seeds and places, " + std::to_string(Low) + " held at 12, " + std::to_string(High) + " at 70" + (Differ ? "; " + std::to_string(Differ) + " differ, first " + First : ""));
		// "-TheaterSeaHour=" as OpenOn takes it: from 5 to 19 only.
		struct HourAsk { const char* Line; float Asked; };
		const HourAsk Asks[] = { { "-TheaterSeaHour=6.5", 6.5f }, { "-TheaterSeaHour=4.5", 4.5f }, { "-TheaterSeaHour=19", 19.0f }, { "-TheaterSeaHour=19.25", 19.25f }, { "-TheaterSeaHour=5", 5.0f } };
		std::string Bad;
		for (const HourAsk& Ask : Asks)
		{
			DayShim::Line = Ask.Line;
			for (uint64_t Seed : { 1ull, 239ull, 4242ull })
			{
				Today::FOpenOnRec S;
				S.Rec.Board.Lat = 55.0;
				S.Rec.Board.Month = 11;
				double GameHour = 0.0, GameFrom = 0.0, GameUp = 0.0, From = 0.0, Up = 0.0;
				Today::OpenOnSun(S, Seed, GameHour, GameFrom, GameUp);
				SeaDay::SeedSun(Seed, 55.0, 11, Ask.Asked, From, Up);
				if (SeedHour(Seed, Ask.Asked) != GameHour || From != GameFrom || Up != GameUp) Bad += std::string(Ask.Line) + " ";
			}
		}
		DayShim::Line.clear();
		Check("seed: \"-TheaterSeaHour=\" taken as OpenOn takes it (5 to 19; 4.5 and 19.25 left to the seed)", Bad.empty(), Bad.empty() ? "5 asks, 3 seeds each" : Bad);

		// The look: every number today's, over every recording and a sweep of stage suns.
		struct Kept { Field F; StageSun Stage; std::string Name; };
		std::vector<Kept> Days;
		for (const Played& P : All)
		{
			Today::FOpenOnRec S;
			S.Rec.Board.Lat = P.Rec.Board.Lat;
			S.Rec.Board.Month = P.Rec.Board.Month;
			double H = 0.0;
			StageSun Stage;
			Today::OpenOnSun(S, P.Seed, H, Stage.From, Stage.Up);
			Days.push_back({ FieldOf(P), Stage, P.Name });
		}
		for (double Up = 12.0; Up <= 70.0; Up += 2.9)
		{
			Field F;
			F.Seed = static_cast<uint64_t>(Up * 13.0);
			Days.push_back({ F, StageSun{ Up, 37.0 * Up }, "stage sun " + Num(Up, 1) });
		}
		const FUnderStandIn* NoUnder = nullptr;
		std::map<std::string, std::string> Wrong;
		auto Must = [&](const std::string& What, bool bOk, const std::string& Name) { if (!bOk && !Wrong.count(What)) Wrong[What] = Name; };
		// (under each line: the scene's numbers passed through, as step 11 passes them; and, not passed, the built-ins,
		// which a switch moves off the scene's)
		int Lines = 0, Moved = 0, Switches = 0;
		std::string Unmoved;
		for (const char* Line : SeedLines)
		{
			DayShim::Line = Line;
			++Lines;
			const float Want = Today::WantOf(true, NoUnder), FillWant = Today::WantOf(false, NoUnder), SkyWant = Today::SkyWantOf(NoUnder);
			const FLinearColor GameScreen = Today::SeaParts::SkyColour(), GameSeaSky = Today::SeaParts::SeaSkyLight();
			Today::AAirStage Stage;
			Stage.UnitsPerMetre = Unit;
			Stage.SetSeaWind(0.4, 6.0, GameSeaSky, Today::SeaParts::GradeNow());
			FMaterialStandIn Moving;
			Stage.MovingSeaParams(&Moving);
			const TodayScene Scene = SceneNow();
			const std::string LineName = *Line ? std::string(" under ") + Line : std::string(" on a bare line");
			{
				// (the built-ins, not passed: the scene's on a bare line only)
				const Look Bare = LookOf(Field{}, StageSun{}, Asked{});
				UExponentialHeightFogComponent Room;
				Today::RoomFog(&Room, Unit);
				const bool bSame = SameColour(Bare.SkyOnScreen, GameScreen) && SameColour(Bare.SeaSkyLight, GameSeaSky) && SameColour(Bare.SkyHigh, Moving.Vectors["SkyHigh"]) &&
					static_cast<float>(Bare.Air.MaterialTop) == Moving.Scalars["HazeTop"] && static_cast<float>(Bare.Air.Start * Unit) == Room.StartDistance && SameFloat(Bare.Air.Density, Room.FogDensity) &&
					SameFloat(Bare.Air.Falloff, Room.FogHeightFalloff);
				if (!*Line) Must("the built-ins are the scene's on a bare line", bSame, "bare");
				else if (std::string(Line) != "-TheaterSeaGrade=1.1,0.9")
				{
					++Switches;
					Moved += !bSame;
					if (bSame) Unmoved += std::string(Line) + " ";
				}
			}
			for (const Kept& K : Days)
			{
				const std::string Name = K.Name + LineName;
				const Look L = LookOf(K.F, K.Stage, Asked{}, Scene);
				Must("mode", L.Day == Mode::Seed && L.Source == SunSource::Stage, Name);
				Must("the hour", L.Hour == SeedHour(K.F.Seed), Name);
				Must("the stage's sun, untouched", L.SunUp == K.Stage.Up && L.SunFrom == K.Stage.From && L.Key.Kind == BodyKind::Sun && L.Key.Up == K.Stage.Up && L.Key.From == K.Stage.From, Name);
				Must("the key, Want's 7.0", SameFloat(L.KeyLight, Want) && L.KeyShare == 1.0 && static_cast<float>(Want * L.KeyShare) == Want, Name);
				Must("the fills, Want's 0.75", SameFloat(L.FillLight, FillWant) && L.FillShare == 1.0 && static_cast<float>(FillWant * L.FillShare) == FillWant, Name);
				Must("the sky light, SkyWant's 0.72", static_cast<float>(L.SkyLight) == SkyWant && L.SkyShare == 1.0 && static_cast<float>(SkyWant * L.SkyShare) == SkyWant, Name);
				FLampStandIn Lamp;
				Lamp.Temperature = 6500.0f;
				const float Warm = Today::KelvinAfter(true, &Lamp);
				FLampStandIn Kept4900;
				Kept4900.Temperature = 4900.0f;
				Today::KelvinAfter(true, &Kept4900);
				Must("the key's warmth, 4,900 K", SameFloat(L.Kelvin, Warm) && Kept4900.Sets == 0 && !L.bPerPixelTransmittance, Name);
				Must("no exposure, grade or smoke hour of its own", L.Bias == 0.0 && L.Saturation == 1.0 && L.BlueGain == 1.0 &&
					SameColour(L.HourLight, FLinearColor::White), Name);
				Must("the unlit and the self-lit pieces at 1, no stars", L.UnlitShare == 1.0 && L.SelfLitShare == 1.0 && L.StarShare == 0.0 && L.Stars.Visible == 0.0, Name);
				Must("SkyOnScreen as SkyColour (221/221/223 bare, -TheaterSeaSky=)", SameColour(L.SkyOnScreen, GameScreen), Name);
				Must("SeaSkyLight as SeaParts' (229/230/231 under the fit's grade bare; the grade's switches)", SameColour(L.SeaSkyLight, GameSeaSky) && SameColour(L.SkyToward, GameSeaSky) && SameColour(L.SkyAway, GameSeaSky), Name);
				Must("the sea material's sky, MovingSea's SkyColour and SkyHigh", SameColour(L.SeaSkyLight, Moving.Vectors["SkyColour"]) && SameColour(L.SkyHigh, Moving.Vectors["SkyHigh"]), Name);
				Must("the sea material's own haze, MovingSea's HazeTop and HazeReach (0.6 and 5 km bare, -TheaterSeaHaze=)", L.Air.bMaterialToday && static_cast<float>(L.Air.MaterialTop) == Moving.Scalars["HazeTop"] &&
					static_cast<float>(L.Air.MaterialReach * Unit) == Moving.Scalars["HazeReach"], Name);
				for (const float Before : { 0.02f, 0.3f })
				{
					UExponentialHeightFogComponent Room;
					Room.FogDensity = Before;
					Today::RoomFog(&Room, Unit);
					Must("the haze, TakeTheRoom's then AirHaze's (bare: from 15 km, twice the room's 0.00045, falloff 0.006, wholly opaque at most; -TheaterAirHaze=), in HazeInk",
						L.Air.Kind == HazeKind::Today && static_cast<float>(L.Air.Start * Unit) == Room.StartDistance && SameFloat(L.Air.Density, Room.FogDensity) &&
						SameFloat(L.Air.Falloff, Room.FogHeightFalloff) && SameFloat(L.Air.MostOpacity, Room.FogMaxOpacity) && SameColour(L.Air.Colour, Room.FogInscatteringColor) && !L.Air.bSun, Name);
				}
				Today::FKeyStandIn Key;
				Must("the day under the water, ReadSun's", L.Daylight == Today::ReadSunDaylight(K.Stage.Up, &Key) && L.Daylight == SeaUnder::DaylightOf(K.Stage.Up), Name);
				const std::string Said = Say(L);
				Must("said, as seed, with no dice", Said.find("seed (today's)") != std::string::npos && Lower(Said).find("dice") == std::string::npos, Name);
			}
		}
		DayShim::Line.clear();
		const char* Order[] = { "mode", "the hour", "the stage's sun, untouched", "the key, Want's 7.0", "the fills, Want's 0.75", "the sky light, SkyWant's 0.72", "the key's warmth, 4,900 K",
			"no exposure, grade or smoke hour of its own", "the unlit and the self-lit pieces at 1, no stars", "SkyOnScreen as SkyColour (221/221/223 bare, -TheaterSeaSky=)", "SeaSkyLight as SeaParts' (229/230/231 under the fit's grade bare; the grade's switches)",
			"the sea material's sky, MovingSea's SkyColour and SkyHigh", "the sea material's own haze, MovingSea's HazeTop and HazeReach (0.6 and 5 km bare, -TheaterSeaHaze=)",
			"the haze, TakeTheRoom's then AirHaze's (bare: from 15 km, twice the room's 0.00045, falloff 0.006, wholly opaque at most; -TheaterAirHaze=), in HazeInk", "the day under the water, ReadSun's",
			"said, as seed, with no dice" };
		for (const char* What : Order)
		{
			const auto It = Wrong.find(What);
			Check(std::string("seed: ") + What + " (the game's own lines)", It == Wrong.end(), It == Wrong.end() ? std::to_string(Days.size()) + " days, every recording and stand-in among them, under " +
				std::to_string(Lines) + " command lines (bare and each of today's sky, grade, haze and sea switches), the scene's numbers passed as step 11 passes them" : "first wrong: " + It->second);
		}
		{
			const auto It = Wrong.find("the built-ins are the scene's on a bare line");
			Check("seed: not passed, the built-in sky, haze and sea material are the scene's on a bare line, and every switch moves the scene off them (so step 11 passes the scene's own)",
				It == Wrong.end() && Moved == Switches && Switches >= 9, std::to_string(Moved) + " of " + std::to_string(Switches) + " switched lines" + (Unmoved.empty() ? "" : "; unmoved: " + Unmoved));
		}
		const Look Once = LookOf(Days.front().F, Days.front().Stage, Asked{});
		Note("seed: the key " + Num(Once.KeyLight, 2) + ", fills " + Num(Once.FillLight, 2) + ", " + Num(Once.Kelvin, 0) + " K, sky light " + Num(Once.SkyLight, 2) + "; SkyOnScreen " +
			Num(Once.SkyOnScreen.R * 255.0, 1) + "/" + Num(Once.SkyOnScreen.G * 255.0, 1) + "/" + Num(Once.SkyOnScreen.B * 255.0, 1) + "; SeaSkyLight " + Say3(Once.SeaSkyLight) +
			"; the haze from " + Num(Once.Air.Start, 0) + " m at " + Sci(Once.Air.Density) + ", in " + Say3(Once.Air.Colour));
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The constants' sources, and the lines that are only text.

	void Pins(const std::string& Src)
	{
		DayShim::Line.clear();
		const FLinearColor Ink = Today::SeaParts::HazeInk, Screen = Today::SeaParts::SkyOnScreen, SeaInk = Today::AirParts::SeaHazeInk();
		Check("pins: HazeInk, SkyOnScreen, the fit's grade and SeaHazeInk are the game's",
			Ink.R == HazeInkToday[0] && Ink.G == HazeInkToday[1] && Ink.B == HazeInkToday[2] &&
			Screen.R == static_cast<float>(SkyOnScreenPixel[0]) / 255.0f && Screen.G == static_cast<float>(SkyOnScreenPixel[1]) / 255.0f && Screen.B == static_cast<float>(SkyOnScreenPixel[2]) / 255.0f &&
			Today::SeaParts::GradeOfTheFit.X == FitRed && Today::SeaParts::GradeOfTheFit.Y == FitBlue &&
			SeaInk.R == static_cast<float>(SeaHazeInkToday[0]) && SeaInk.G == static_cast<float>(SeaHazeInkToday[1]) && SeaInk.B == static_cast<float>(SeaHazeInkToday[2]),
			"HazeInk " + Say3(Ink) + ", SeaHazeInk " + Say3(SeaInk));
		const Rgb Back = LightOfStill(Rgb{ 229.0f / 255.0f, 230.0f / 255.0f, 231.0f / 255.0f }, FitRed, FitBlue);
		const FLinearColor GameBack = Today::SeaParts::LightOfStill(FLinearColor(229.0f / 255.0f, 230.0f / 255.0f, 231.0f / 255.0f), Today::SeaParts::GradeOfTheFit);
		const Rgb Again = StillOf(Back, FitRed, FitBlue);
		Check("pins: LightOfStill is the game's, and StillOf its inverse (to a thousandth of a level)", SameColour(Back, GameBack) &&
			std::fabs(Again.R * 255.0 - 229.0) < 1e-3 && std::fabs(Again.G * 255.0 - 230.0) < 1e-3 && std::fabs(Again.B * 255.0 - 231.0) < 1e-3,
			Say3(Back) + " back to " + Num(Again.R * 255.0, 4) + "/" + Num(Again.G * 255.0, 4) + "/" + Num(Again.B * 255.0, 4));
		// (the campaign map's smoke at night; the show's lookout; the kit's smoke hour today)
		const std::string Map = ReadAll(Src + "/Campaign/CampaignMapActor.cpp");
		const std::regex Smoke("const FLinearColor SmokeHour = FMath::Lerp\\(FLinearColor\\(1\\.0f, 1\\.0f, 1\\.0f\\), FLinearColor\\(([0-9.]+)f, ([0-9.]+)f, ([0-9.]+)f\\), GetMoonlight\\(\\)\\);");
		std::smatch M;
		const bool bSmoke = std::regex_search(Map, M, Smoke) && static_cast<float>(SmokeNight[0]) == std::stof(M[1].str()) && static_cast<float>(SmokeNight[1]) == std::stof(M[2].str()) &&
			static_cast<float>(SmokeNight[2]) == std::stof(M[3].str());
		Check("pins: the smoke's night (HourLight's) is CampaignMapActor's SmokeHour at full moonlight", bSmoke, bSmoke ? M[1].str() + "/" + M[2].str() + "/" + M[3].str() : "not found");
		const std::string ShowText = ReadAll(Src + "/Battle/SeaShow.cpp");
		Check("pins: NightSight's 4 km with no vis is Show::Hunt's lookout", CountOf(ShowText, "const double Sees = F.Seeing > 0.0 ? F.Seeing : 4000.0;") == 1 &&
			CountOf(ShowText, "if (Flat(O.P - Boat.P) <= Sees)") == 1, "SeaShow.cpp");
		const std::string Sea = ReadAll(Src + "/Battle/SeaBattle.cpp");
		const std::regex TickLine("Effects->Tick\\([^\\n]*\\);");
		std::string Ticks;
		int TickN = 0, White = 0;
		for (std::sregex_iterator It(Sea.begin(), Sea.end(), TickLine), End; It != End; ++It)
		{
			++TickN;
			White += It->str().find("FLinearColor::White") != std::string::npos;
		}
		Check("pins: today's smoke hour is white (the kit's Tick given FLinearColor::White; step 11 passes Day.HourLight there)", TickN == 1 && White == 1,
			std::to_string(TickN) + " Tick line(s)");
		// (every eye height the shots name, against PlannedEyes)
		std::set<double> Named;
		const std::regex Eyes[] = { std::regex("OutEye = World\\([^,\\n]+,[^,\\n]+,\\s*([0-9]+\\.[0-9]+)\\)"), std::regex("const double EyeUp = ([0-9]+\\.[0-9]+);"),
			std::regex("for \\(const double Up : \\{ ([0-9.]+), ([0-9.]+), ([0-9.]+) \\}\\)"), std::regex("Sea\\.Spot = FVector2D\\(K < 2 \\? ([0-9.]+) : ([0-9.]+),") };
		for (const std::regex& R : Eyes)
		{
			for (std::sregex_iterator It(Sea.begin(), Sea.end(), R), End; It != End; ++It)
			{
				for (size_t K = 1; K < It->size(); ++K) Named.insert(std::stod((*It)[K].str()));
			}
		}
		const std::set<double> Ours(std::begin(PlannedEyes), std::end(PlannedEyes));
		std::string Heights;
		for (const double H : Named) Heights += Num(H, 0) + " ";
		Check("pins: PlannedEyes are every eye height AimEye and PlanShots name over the sea (the lookout to the overview)", Named == Ours, Heights + "m");
		// (the engine's fog as transcribed, until the Mac's check 1 reads 5.8's HeightFogCommon.ush; and the material's haze)
		const std::string Core = ReadAll(Src + "/Battle/SeaDayCore.cpp");
		const char* Formula[] = { "const double DensityU = F.Density / 1000.0, FalloffU = F.Falloff / 1000.0;",
			"Origin = DensityU * std::exp2(Clamp(-FalloffU * (EyeZ - F.HeightZ), -125.0, 126.0));",
			"Origin = DensityU * std::exp2(-std::max(-127.0, FalloffU * (CutZ - F.HeightZ)));",
			"const double Line = std::fabs(Rise) > 0.01 ? (1.0 - std::exp2(-Rise)) / Rise : Ln2 - 0.5 * Ln2 * Ln2 * Rise;",
			"const double Seen = Clamp(std::exp2(-(Origin * Line * RayLength)), 0.0, 1.0);", "return std::max(Seen, 1.0 - F.MostOpacity);" };
		std::string Lost;
		for (const char* Line : Formula) if (CountOf(Core, Line) != 1) Lost += std::string(Line) + " ";
		Check("pins: the height fog's line integral as transcribed (HeightFogCommon.ush; the Mac's check 1 confirms it against 5.8)", Lost.empty(), Lost.empty() ? "6 lines" : "changed: " + Lost);
		const std::string Build = ReadAll(Src + "/../../Tools/build_content.py");
		const bool bMaterial = CountOf(Build, "reach_sq = times(m, reach, reach,") == 1 && CountOf(Build, "connect(plus(m, reach_sq, 1.0,") == 1 &&
			CountOf(Build, "lit = lerp(m, seen, times(m, sky_colour, scalar(m, \"HazeTop\"") == 1;
		Check("pins: the sea material's own haze is lerp(seen, HazeTop x sky, (d/reach)^2 / (1 + (d/reach)^2)), as MaterialHazed works it", bMaterial, "Tools/build_content.py");
		// (nothing wired: no game file but the core reads it)
		std::string Readers;
		for (const auto& E : std::filesystem::recursive_directory_iterator(Src))
		{
			if (!E.is_regular_file()) continue;
			const std::string Path = E.path().string();
			if (Path.find("SeaDayCore.") != std::string::npos) continue;
			const std::string Ext = E.path().extension().string();
			if (Ext != ".h" && Ext != ".cpp" && Ext != ".inl") continue;
			if (ReadAll(Path).find("SeaDayCore") != std::string::npos) Readers += E.path().filename().string() + " ";
		}
		Check("pins: nothing is wired yet (no game file but the core's own names SeaDayCore)", Readers.empty(), Readers.empty() ? "none" : Readers);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The engine's height fog: the transcribed line integral against a numeric one.

	void FogLine()
	{
		uint64_t State = 0x9E3779B97F4A7C15ull;
		auto Next = [&State]()
		{
			State = State * 6364136223846793005ull + 1442695040888963407ull;
			return static_cast<double>(State >> 11) / 9007199254740992.0;
		};
		const double Ln2 = std::log(2.0);
		double Worst = 0.0;
		int Tried = 0, Starts = 0, Taylor = 0;
		std::string First;
		for (int K = 0; K < 4000; ++K)
		{
			Fog F;
			F.Density = std::exp(std::log(1e-4) + Next() * std::log(500.0));
			F.Falloff = K % 5 == 0 ? 0.0 : std::exp(std::log(1e-4) + Next() * std::log(5000.0));
			F.HeightZ = -5000.0 + Next() * 50000.0;
			F.Start = K % 3 == 0 ? Next() * 2.0e6 : 0.0;
			F.MostOpacity = K % 7 == 0 ? 0.8 : 1.0;
			const double Eye = Next() * 130000.0, Target = K % 4 == 0 ? Eye + (Next() - 0.5) * 4.0 : Next() * 5000.0, Across = std::exp(std::log(100.0) + Next() * std::log(30000.0));
			const double Got = FogSeen(F, Eye, Target, Across);
			// (numerically: the density 2^-(falloff (z - h)), a thousandth of the component's each, from the start along the
			// ray; what is let through e^-((ln 2)^2 of its integral), as the shader's 2^-(ln 2 of it))
			const double Length = std::hypot(Across, Target - Eye);
			const double From = std::min(F.Start, Length);
			const int Steps = 4000;
			const double Ds = (Length - From) / Steps;
			double Sum = 0.0;
			for (int S = 0; S <= Steps; ++S)
			{
				const double At = From + S * Ds, Z = Eye + (Target - Eye) * At / Length;
				const double Rho = F.Density / 1000.0 * std::exp2(-F.Falloff / 1000.0 * (Z - F.HeightZ));
				Sum += Rho * (S == 0 || S == Steps ? 1.0 : S % 2 ? 4.0 : 2.0);
			}
			const double Depth = Sum * Ds / 3.0 * Ln2 * Ln2;
			const double Want = std::max(std::exp(-Depth), 1.0 - F.MostOpacity);
			const double Rise = F.Falloff / 1000.0 * (Target - (Eye + (Target - Eye) * From / Length));
			Taylor += std::fabs(Rise) <= 0.01 && F.Falloff > 0.0;
			Starts += F.Start > 0.0 && From < Length;
			++Tried;
			// (in optical depth, where it lets anything through; the Taylor branch is the engine's own, within (ln 2)^2 r^2 / 6)
			const double Off = Want > 1e-200 && Got > 1e-200 ? std::fabs(std::log(Got) - std::log(Want)) / std::max(Depth, 1e-9) : std::fabs(Got - Want);
			if (Off > Worst)
			{
				Worst = Off;
				First = "fog " + Sci(F.Density) + "/" + Sci(F.Falloff) + " eye " + Num(Eye / Unit, 0) + " m, " + Num(Across / Unit, 0) + " m across: " + Sci(Got) + " against " + Sci(Want);
			}
		}
		Check("fog: FogSeen's line integral (the start's exclusion, the Taylor branch, the least opacity) against a numeric integral along the ray, within 2e-5 of the optical depth",
			Worst <= 2e-5, std::to_string(Tried) + " rays, " + std::to_string(Starts) + " from a start, " + std::to_string(Taylor) + " on the Taylor branch; worst " + Sci(Worst) + (Worst > 2e-5 ? ", " + First : ""));
		// (a level ray through a uniform fog: e^-(beta x metres), with beta DensityOfBeta's)
		double Level = 0.0;
		for (const double Beta : { 1e-5, 2.6e-5, 1.2e-3, 1e-2 })
		{
			Fog F;
			F.Density = DensityOfBeta(Beta, Unit);
			for (const double R : { 100.0, 2500.0, 30000.0 })
			{
				Level = std::max(Level, std::fabs(std::log(FogSeen(F, 1400.0, 1400.0, R * Unit)) + Beta * R) / (Beta * R));
			}
			Level = std::max(Level, std::fabs(BetaOfDensity(DensityOfBeta(Beta, Unit), Unit) - Beta) / Beta);
		}
		Check("fog: a uniform fog's extinction is DensityOfBeta's (a thousandth of FogDensity x (ln 2)^2 a world unit)", Level < 1e-9, "worst " + Sci(Level));
		Fog Started;
		Started.Density = 5.0;
		Started.Start = 300000.0;
		Fog Most;
		Most.Density = 50.0;
		Most.MostOpacity = 0.8;
		Check("fog: nothing is fogged inside the start, and never more than FogMaxOpacity", FogSeen(Started, 1400.0, 0.0, 299000.0) == 1.0 &&
			std::fabs(FogSeen(Most, 1400.0, 0.0, 5.0e6) - 0.2) < 1e-12, "1 inside 3 km's start; " + Num(FogSeen(Most, 1400.0, 0.0, 5.0e6), 6) + " at most 0.8");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The vis fit: every planned eye's ray to a ship at vis keeps Cmin.

	void Fits()
	{
		const std::vector<double> Eyes = Planned();
		int Tried = 0, Bands = 0, Thinned = 0;
		double WorstEye = 1e9, WorstLeast = 0.0, LookoutMost = 0.0;
		std::string Bad;
		for (const double Cmin : { 0.05, 0.02, 0.1 })
		{
			for (const double Vis : { 300.0, 600.0, 1000.0, 1500.0, 2500.0, 2900.0, 3500.0, 5000.0, 8000.0, 12000.0, 20000.0, 30000.0 })
			{
				for (const double FogZ : { 0.0, 50.0 })
				{
					const double Beta = -std::log(Cmin) / Vis;
					const FogFit Fit = FogFor(Beta, Eyes, Vis, FogZ, Unit, Cmin);
					++Tried;
					Bands += Fit.bBand;
					Fog F;
					F.Density = Fit.Density;
					F.Falloff = Fit.Falloff;
					F.HeightZ = FogZ * Unit;
					double Worst = 1.0;
					for (const double Eye : Eyes) Worst = std::min(Worst, FogSeen(F, Eye * Unit, 0.0, Vis * Unit));
					WorstEye = std::min(WorstEye, Worst / Cmin);
					if (Worst < Cmin * (1.0 - 1e-9)) Bad += "vis " + Num(Vis, 0) + " Cmin " + Num(Cmin, 2) + " ";
					// (the sea's own density is beta's, unless a ray could not be kept and it was thinned)
					const double AtSea = BetaOfDensity(Fit.Density * std::exp2(Fit.Falloff / 1000.0 * FogZ * Unit), Unit);
					if (std::fabs(AtSea - Beta) > 1e-9 * Beta) ++Thinned;
					// (the least falloff: a little less lets some ray under Cmin)
					if (Fit.bBand)
					{
						Fog Less = F;
						Less.Falloff = Fit.Falloff * 0.98;
						Less.Density = DensityOfBeta(Beta, Unit) * std::exp2(-Less.Falloff / 1000.0 * FogZ * Unit);
						double W = 1.0;
						for (const double Eye : Eyes) W = std::min(W, FogSeen(Less, Eye * Unit, 0.0, Vis * Unit));
						WorstLeast = std::max(WorstLeast, W / Cmin);
					}
					// (still the rules' vis from the lookout: a ship there keeps little more than Cmin, and fades beyond)
					const double Lookout = FogSeen(F, PlannedEyes[0] * Unit, 0.0, Vis * Unit);
					LookoutMost = std::max(LookoutMost, Lookout / Cmin);
					if (FogSeen(F, PlannedEyes[0] * Unit, 0.0, 1.5 * Vis * Unit) >= Cmin) Bad += "fades-not vis " + Num(Vis, 0) + " ";
				}
			}
		}
		Check("vis: FogFor keeps every planned eye's ray (14 to 1,300 m up) to a ship at the rules' vis at Cmin or more, by the engine's line integral",
			Bad.empty() && WorstEye >= 1.0 - 1e-9, std::to_string(Tried) + " fits (vis 300 m to 30 km, Cmin 0.02 to 0.1, the fog at 0 and 50 m), " + std::to_string(Bands) +
			" with a falloff; worst " + Num(WorstEye, 4) + " Cmin" + (Bad.empty() ? "" : "; " + Bad));
		Check("vis: the falloff is the least that does it (2% less lets a ray under Cmin), the sea's density beta's", WorstLeast < 1.0 && Thinned == 0,
			"best ray at 98% of it " + Num(WorstLeast, 4) + " Cmin; " + std::to_string(Thinned) + " thinned");
		Check("vis: from the lookout a ship at vis keeps under 1.25 Cmin, and at 1.5 vis under Cmin (the vis is still the rules')", LookoutMost < 1.25 && Bad.find("fades-not") == std::string::npos,
			"most " + Num(LookoutMost, 3) + " Cmin at vis");
	}

	// ---------------------------------------------------------------------------------------------------------------
	// Field mode's rules.

	void FieldRules()
	{
		const Asked A = FieldAsk();
		// The sun: the field's light, its hour, or the seed's, unclamped.
		{
			Field F = Lit(20.0, 250.0);
			const Look L1 = LookOf(F, StageSun{}, A);
			F.bSunFrom = false;
			F.bHour = true;
			F.Hour = 17.0;
			const Look L2 = LookOf(F, StageSun{}, A);
			double From17 = 0.0, Up17 = 0.0;
			SunAt(F.Lat, F.Month, 17.0, From17, Up17);
			F.bHour = false;
			F.SunUp = -24.0;
			F.Lat = 55.0;
			F.Month = 11;
			const Look L3 = LookOf(F, StageSun{}, A);
			// (her evening bearing: the hour that day she stands -24 degrees, after noon)
			double Lo = 12.0, Hi = 24.0;
			for (int K = 0; K < 80; ++K)
			{
				const double Mid = 0.5 * (Lo + Hi);
				double MidFrom = 0.0, MidUp = 0.0;
				SunAt(55.0, 11, Mid, MidFrom, MidUp);
				(MidUp > -24.0 ? Lo : Hi) = Mid;
			}
			double Ev = 0.0, EvUp = 0.0;
			SunAt(55.0, 11, 0.5 * (Lo + Hi), Ev, EvUp);
			Check("field: the sun from light.sun {up, from}; with no from, from the field's hour, else her evening bearing that day",
				L1.Source == SunSource::FieldLight && L1.SunUp == 20.0 && L1.SunFrom == 250.0 && L2.SunFrom == From17 && L2.SunUp == 20.0 && L2.Hour == 17.0 &&
				std::fabs(L3.SunFrom - Ev) < 1e-6 && L3.SunFrom > 180.0 && L3.SunUp == -24.0,
				"from " + Num(L1.SunFrom, 1) + "; at 17:00 " + Num(L2.SunFrom, 2) + "; -24 at 55N in December: " + Num(L3.SunFrom, 2) + " (evening " + Num(Ev, 2) + ")");
		}
		{
			Field F;
			F.Lat = 45.0;
			F.Month = 5;
			F.bHour = true;
			F.Hour = 5.5;
			const Look L1 = LookOf(F, StageSun{}, A);
			double From = 0.0, Up = 0.0;
			SunAt(45.0, 5, 5.5, From, Up);
			F.Hour = 23.0;
			const Look L2 = LookOf(F, StageSun{}, A);
			double From23 = 0.0, Up23 = 0.0;
			SunAt(45.0, 5, 23.0, From23, Up23);
			Field G;
			G.Lat = 62.0;
			G.Month = 11;
			int Unclamped = 0, Seeds = 0;
			bool bSame = true;
			for (uint64_t Seed = 1; Seed < 400; Seed += 3)
			{
				G.Seed = Seed;
				const Look L = LookOf(G, StageSun{}, A);
				double SF = 0.0, SU = 0.0, CF = 0.0, CU = 0.0;
				SunAt(62.0, 11, SeedHour(Seed), SF, SU);
				SeedSun(Seed, 62.0, 11, -1.0f, CF, CU);
				bSame = bSame && L.Source == SunSource::SeedHour && L.Hour == SeedHour(Seed) && L.SunUp == SU && L.SunFrom == SF;
				Unclamped += L.SunUp < ClampLow && CU == ClampLow;
				++Seeds;
			}
			Asked AH = A;
			AH.AskedHour = 18.5f;
			const Look L4 = LookOf(F, StageSun{}, AH);
			Check("field: the field's hour and the seed's through SunAt with no clamp, down to astronomical night; -TheaterSeaHour= first",
				L1.Source == SunSource::FieldHour && L1.SunUp == Up && L1.SunFrom == From && Up < ClampLow && L2.SunUp == Up23 && Up23 < -18.0 && bSame && Unclamped > 0 &&
				L4.Hour == 18.5 && L4.Source == SunSource::SeedHour,
				"5:30 at 45N in June " + Num(Up, 2) + " deg (the stage's 12), 23:00 " + Num(Up23, 1) + "; " + std::to_string(Unclamped) + " of " + std::to_string(Seeds) +
				" seeded days at 62N in December under the clamp");
		}
		// The key body.
		{
			int Wrong = 0, Suns = 0, Moons = 0, Glows = 0;
			for (double Up = -20.0; Up <= 70.0; Up += 0.25)
			{
				for (const double MoonUp : { -10.0, 3.0, 30.0 })
				{
					for (const double MoonLit : { 0.0, 0.5 })
					{
						const Look L = LookOf(Lit(Up, 250.0, MoonUp, MoonLit), StageSun{}, A);
						const BodyKind Want = Up > SunKeyLowest ? BodyKind::Sun : MoonUp > 0.0 && MoonLit > 0.0 ? BodyKind::Moon : BodyKind::Glow;
						Wrong += L.Key.Kind != Want;
						Suns += L.Key.Kind == BodyKind::Sun;
						Moons += L.Key.Kind == BodyKind::Moon;
						Glows += L.Key.Kind == BodyKind::Glow;
						if (L.Key.Kind == BodyKind::Moon) Wrong += L.Key.Up != MoonUp || L.Key.From != 140.0;
						if (L.Key.Kind == BodyKind::Glow) Wrong += L.Key.Up != 90.0 || L.Key.Toward.Z != 1.0;
						if (L.Key.Kind != BodyKind::Sun && Up <= KeyRampLow) Wrong += L.Kelvin != NightKelvin;
					}
				}
			}
			Check("field: the sun keys the scene above -4 degrees, then the moon (where she is up and lit) at 8,000 K, else the glow overhead", Wrong == 0,
				std::to_string(Suns) + " sun, " + std::to_string(Moons) + " moon, " + std::to_string(Glows) + " glow" + (Wrong ? "; " + std::to_string(Wrong) + " wrong" : ""));
		}
		// The moon's key, and the night's without her.
		{
			// (her key in proportion to her lit share: nothing from an unlit moon, the full moon's 0.06 of the day's at the
			// zenith; the lamp LookOf keys with)
			double Worst = 0.0, PerLit = 0.0, Off = 0.0, Unlit = 0.0, MostDark = 0.0;
			for (const double MoonUp : { 3.0, 15.0, 30.0, 60.0, 85.0 })
			{
				Unlit = std::max(Unlit, MoonKeyOf(MoonUp, 0.0));
				for (const double Lit_ : { 0.1, 0.25, 0.5, 0.75, 1.0 })
				{
					const double K = MoonKeyOf(MoonUp, Lit_);
					const double Per = K / Lit_ / std::sqrt(std::sin(MoonUp * Pi / 180.0));
					if (PerLit == 0.0) PerLit = Per;
					Off = std::max(Off, std::fabs(Per - PerLit) / PerLit);
					const Look L = LookOf(Lit(-20.0, 250.0, MoonUp, Lit_), StageSun{}, A);
					Worst = std::max(Worst, L.Key.Kind == BodyKind::Moon ? std::fabs(L.KeyLight - K) / K : 1.0);
				}
			}
			Check("field: the moon's key is in proportion to her lit share, 7.0 x 0.06 x lit x the root of her height's sine: nothing from an unlit moon, and LookOf keys with it",
				Unlit == 0.0 && Off < 1e-12 && std::fabs(PerLit - KeyDay * MoonFull) < 1e-12 && Worst < 1e-12 && MoonKeyOf(-5.0, 1.0) == 0.0,
				"K(0) " + Num(Unlit, 3) + ", " + Num(PerLit, 4) + " a lit share at the zenith (" + Sci(Off) + " off); LookOf " + Sci(Worst) + " off");
			// (a light{} with no moon in it, as SeaFeed reads it: an unlit moon 30 degrees up, keyed by the glow, no disc)
			AirFeed::Json J;
			std::string Error;
			SeaShow::Field Board;
			const bool bRead = AirFeed::Parse("{\"light\": {\"sun\": -20, \"vis\": 3000}, \"lat\": 50, \"lon\": -25}", J, Error) && SeaFeed::ReadField(J, Board);
			Field F;
			F.Lat = Board.Lat;
			F.Lon = Board.Lon;
			F.bLight = Board.bLight;
			F.SunUp = Board.SunUp;
			F.MoonUp = Board.MoonUp;
			F.MoonFrom = Board.MoonFrom;
			F.MoonLit = Board.MoonLit;
			F.Seeing = Board.Seeing;
			const Look NoMoon = LookOf(F, StageSun{}, A), Glow = LookOf(Lit(-20.0, 250.0, -30.0, 0.0, 3000.0, 50.0), StageSun{}, A);
			Check("field: a light{} that names no moon (SeaFeed's unlit moon 30 degrees up) is keyed by the glow, with no disc, as a moonless night",
				bRead && Board.bLight && Board.MoonUp == 30.0 && Board.MoonLit == 0.0 && NoMoon.Key.Kind == BodyKind::Glow && !NoMoon.Moon.bUp && NoMoon.KeyLight == Glow.KeyLight &&
				NoMoon.Bias == Glow.Bias && NoMoon.StarShare == Glow.StarShare,
				"SeaFeed's moon " + Num(Board.MoonUp, 0) + " deg, " + Num(Board.MoonLit, 2) + " lit: keyed by " + (NoMoon.Key.Kind == BodyKind::Glow ? "the glow" : "not the glow") + (NoMoon.Moon.bUp ? ", a disc" : ", no disc"));
			// (with no moon, from -4 degrees down the night's key is the glow's, faded in from nothing)
			for (double Up = SunKeyLowest; Up >= -18.0; Up -= 0.25)
			{
				MostDark = std::max(MostDark, LookOf(Lit(Up, 250.0, -20.0, 0.9), StageSun{}, A).KeyLight / KeyDay);
				Field H;
				H.bHour = true;
				H.Hour = 0.5;
				H.Lat = 30.0;
				MostDark = std::max(MostDark, LookOf(H, StageSun{}, A).KeyLight / KeyDay);
			}
			const Look AtFour = LookOf(Lit(SunKeyLowest, 250.0, -20.0), StageSun{}, A), Above = LookOf(Lit(SunKeyLowest + 0.01, 250.0, -20.0), StageSun{}, A);
			Check("field: with no moon the night's key is at most 3% of the day's, from -4 degrees down; it comes in from nothing at -4, where the sun under the horizon puts nothing on the water",
				MostDark <= 0.03 && AtFour.KeyLight == 0.0 && !std::signbit(AtFour.KeyLight) && Above.Key.Kind == BodyKind::Sun && Above.Key.Toward.Z < 0.0,
				Num(MostDark * 100.0, 2) + "% (the glow); at -4 " + Num(AtFour.KeyLight, 3) + ", the sun's lamp just above " + Num(Above.KeyLight, 3) + " from under the horizon");
		}
		// The light on the level sea, and a moon never darkening the night.
		{
			double Worst = 0.0;
			for (double Up = -20.0; Up <= 70.0; Up += 0.5)
			{
				for (const double MoonUp : { -10.0, 4.0, 40.0 })
				{
					const Look L = LookOf(Lit(Up, 250.0, MoonUp, 0.6), StageSun{}, A);
					const double Mine = L.KeyLight * std::max(0.0, L.Key.Toward.Z) + L.SkyLight + 2.0 * L.FillLight;
					Worst = std::max(Worst, std::fabs(L.OnSea - Mine) / Mine);
				}
			}
			Check("field: the light on the level sea is the key's lamp times the sine of its height (nothing from under the horizon), the sky light and two fills",
				Worst < 1e-12, "-20 to 70 degrees, three moons; worst " + Sci(Worst));
			int Tried = 0, Darker = 0, ShownDarker = 0;
			double LeastGain = 1e9, LeastShown = 1e9;
			std::string First;
			for (int Sky = 0; Sky < 4; ++Sky)
			{
				for (const double Vis : { 0.0, 1000.0, 3500.0, 8000.0 })
				{
					for (double Up = SunKeyLowest - 0.25; Up >= -24.0; Up -= 1.25)
					{
						Field F = Lit(Up, 250.0, -20.0, 0.0, Vis, 50.0, 0);
						F.Wind = Sky == 3 ? 22.0 : 8.0;
						if (Sky > 0)
						{
							F.Sky.bGiven = true;
							F.Sky.Kind = Sky == 1 ? "cumulus" : "squalls";
							F.Sky.Cover = Sky == 1 ? 0.5 : 1.0;
							F.Sky.Rain = Sky == 2 ? 0.8 : Sky == 3 ? 0.3 : 0.0;
						}
						const Look Dark = LookOf(F, StageSun{}, A);
						const double DarkLight = Dark.KeyLight * std::max(0.0, Dark.Key.Toward.Z) + Dark.SkyLight + 2.0 * Dark.FillLight;
						for (const double MoonUp : { 0.5, 2.0, 5.0, 10.0, 20.0, 35.0, 60.0, 89.0 })
						{
							for (const double MoonLit : { 0.01, 0.1, 0.3, 0.6, 1.0 })
							{
								F.MoonUp = MoonUp;
								F.MoonLit = MoonLit;
								const Look L = LookOf(F, StageSun{}, A);
								const double Light = L.KeyLight * std::max(0.0, L.Key.Toward.Z) + L.SkyLight + 2.0 * L.FillLight;
								++Tried;
								const double Gain = Light / DarkLight, Shown = Gain * std::exp2(L.Bias - Dark.Bias);
								LeastGain = std::min(LeastGain, Gain);
								LeastShown = std::min(LeastShown, Shown);
								const bool bDarker = Gain < 1.0 - 1e-12, bShown = Shown < 1.0 - 1e-12;
								Darker += bDarker;
								ShownDarker += bShown;
								if ((bDarker || bShown) && First.empty())
									First = "sun " + Num(Up, 2) + " vis " + Num(Vis, 0) + " sky " + std::to_string(Sky) + " moon " + Num(MoonUp, 1) + "/" + Num(MoonLit, 2) + ": " + Num(Gain, 4) + " of the light, " + Num(Shown, 4) + " shown";
							}
						}
						F.MoonUp = -20.0;
						F.MoonLit = 0.0;
					}
				}
			}
			Check("field: adding any moon to a night (from -4 degrees down, low or high, thin or full, clear to squalls) never lowers the light on the level sea or its shown level (at the exposure)",
				Darker == 0 && ShownDarker == 0, std::to_string(Tried) + " moons; least " + Num(LeastGain, 4) + " of the moonless night's light, " + Num(LeastShown, 4) + " of its shown level" + (First.empty() ? "" : "; first " + First));
		}
		// A moon lights the night by her own light: the dusk's and the day's sky and lamps are the moonless ones (from -4
		// degrees up nothing moves at all), and the night's sky only by her own light (her key's share of the day's over the
		// moonless sky's share of the day's); the glow's key she replaces lifts the lamps, never the sky's colours.
		{
			int Days = 0, DayMoved = 0, Nights = 0, Over = 0, Under = 0;
			double DayWorst = 0.0, NightWorst = 0.0, ThinLevels = 0.0, ThinOnSea = 0.0, MostOwn = 0.0;
			std::string DayAt, NightAt, ThinAt;
			for (int Sky = 0; Sky < 2; ++Sky)
			{
				for (double Up = -24.0; Up <= 70.0; Up += 0.5)
				{
					auto FieldOf_ = [&](double MoonUp, double MoonLit)
					{
						Field F = Lit(Up, 250.0, MoonUp, MoonLit, Sky ? 0.0 : 3000.0, 50.0, 0);
						F.Wind = Sky ? 22.0 : 8.0;
						if (Sky)
						{
							F.Sky.bGiven = true;
							F.Sky.Kind = "squalls";
							F.Sky.Cover = 1.0;
							F.Sky.Rain = 0.8;
						}
						return F;
					};
					const Look Dark = LookOf(FieldOf_(-20.0, 0.0), StageSun{}, A);
					const double SkyLift = Sky ? (1.0 + OvercastSky) * (1.0 - RainSky * 0.8) : 1.0;
					const double Dusk = Dark.SkyShare / SkyLift;
					const double Fade = 1.0 - std::clamp((Up - KeyRampLow) / (SunKeyLowest - KeyRampLow), 0.0, 1.0);
					for (const double MoonUp : { 0.5, 3.0, 15.0, 40.0, 75.0 })
					{
						for (const double MoonLit : { 0.01, 0.3, 1.0 })
						{
							const Look M = LookOf(FieldOf_(MoonUp, MoonLit), StageSun{}, A);
							const Rgb Colours[][2] = { { M.SeaSkyLight, Dark.SeaSkyLight }, { M.SkyHigh, Dark.SkyHigh }, { M.SkyToward, Dark.SkyToward }, { M.SkyAway, Dark.SkyAway },
								{ M.Air.Colour, Dark.Air.Colour } };
							const std::string At = "sun " + Num(Up, 1) + " moon " + Num(MoonUp, 1) + "/" + Num(MoonLit, 2) + (Sky ? " in squalls" : " clear");
							if (Up >= 0.0)
							{
								// (by day and in the dusk from the horizon up: under 1%, the finding's bar)
								++Days;
								double Moved = std::max({ Ratio(M.SkyLight, Dark.SkyLight), Ratio(M.OnSea, Dark.OnSea), Ratio(M.UnlitShare, Dark.UnlitShare), RatioOf(M.SkyOnScreen, Dark.SkyOnScreen) }) - 1.0;
								for (const auto& C : Colours) Moved = std::max(Moved, RatioOf(C[0], C[1]) - 1.0);
								if (Moved > DayWorst)
								{
									DayWorst = Moved;
									DayAt = At;
								}
							}
							if (Up >= SunKeyLowest)
							{
								// (from -4 up, where her key is nothing, nothing at all)
								bool bSame = M.SkyLight == Dark.SkyLight && M.FillLight == Dark.FillLight && M.OnSea == Dark.OnSea && M.UnlitShare == Dark.UnlitShare && M.Bias == Dark.Bias &&
									M.SkyOnScreen.R == Dark.SkyOnScreen.R && M.SkyOnScreen.G == Dark.SkyOnScreen.G && M.SkyOnScreen.B == Dark.SkyOnScreen.B;
								for (const auto& C : Colours) bSame = bSame && C[0].R == C[1].R && C[0].G == C[1].G && C[0].B == C[1].B;
								DayMoved += !bSame;
								continue;
							}
							// (at night: each colour by at most her own light over the moonless sky's, never darker; on the screen within
							// the levels that gives)
							++Nights;
							const double Own = Fade * MoonKeyOf(MoonUp, MoonLit) / KeyDay / Dusk;
							MostOwn = std::max(MostOwn, Own);
							for (const auto& C : Colours)
							{
								for (const double Share : { C[0].R / C[1].R, C[0].G / C[1].G, C[0].B / C[1].B })
								{
									Over += Share - 1.0 > Own * (1.0 + 1e-9) + 1e-12;
									Under += Share < 1.0 - 1e-12;
									if (Share - 1.0 > NightWorst)
									{
										NightWorst = Share - 1.0;
										NightAt = At;
									}
								}
							}
							const Rgb Bound = StillOf(Scale3(LightOfStill(Dark.SkyOnScreen, FitRed, FitBlue), 1.0 + Own), FitRed, FitBlue);
							for (const auto& Ch : { std::pair<double, double>{ M.SkyOnScreen.R, Bound.R }, { M.SkyOnScreen.G, Bound.G }, { M.SkyOnScreen.B, Bound.B } }) Over += Ch.first > Ch.second + 0.01 / 255.0;
							Under += M.SkyOnScreen.R < Dark.SkyOnScreen.R || M.SkyOnScreen.G < Dark.SkyOnScreen.G || M.SkyOnScreen.B < Dark.SkyOnScreen.B;
							if (MoonUp == 0.5 && MoonLit == 0.01)
							{
								const double Levels = 255.0 * std::max({ M.SkyOnScreen.R - Dark.SkyOnScreen.R, M.SkyOnScreen.G - Dark.SkyOnScreen.G, M.SkyOnScreen.B - Dark.SkyOnScreen.B });
								if (Levels > ThinLevels)
								{
									ThinLevels = Levels;
									ThinAt = At;
								}
								ThinOnSea = std::max(ThinOnSea, Ratio(M.OnSea, Dark.OnSea) - 1.0);
							}
						}
					}
				}
			}
			Check("field: a moon moves nothing of the day's or the dusk's sky and lamps: from the horizon up the sky light, the light on the level sea, the unlit pieces and every sky colour under 1% "
				"(and from -4 degrees up, where her key is nothing, exactly the moonless look's)",
				DayWorst < 0.01 && DayMoved == 0 && Days > 1000, std::to_string(Days) + " moons from the horizon up, clear with vis and in squalls; worst " + Sci(DayWorst) + (DayAt.empty() ? "" : " at " + DayAt) +
					(DayMoved ? "; " + std::to_string(DayMoved) + " moved from -4 up" : ""));
			Check("field: at night a moon lifts every sky colour by no more than her own light (her key's share of the day's over the moonless sky's) and never darkens it, on the screen within the "
				"levels that gives; a 1%-lit moon half a degree up moves the horizon on the screen by under 1 level at every sun (the glow's key she replaces lifts the lamps, not the sky)",
				Over == 0 && Under == 0 && ThinLevels < 1.0 && Nights > 1000,
				std::to_string(Nights) + " moonlit nights and dusks under -4 degrees; most " + Num(NightWorst * 100.0, 1) + "% (" + NightAt + ", her own light up to " + Num(MostOwn * 100.0, 1) +
					"% of the moonless sky's); the thin low moon at most " + Num(ThinLevels, 3) + " levels (" + ThinAt + "), the light on the level sea " + Sci(ThinOnSea) + " off the moonless" +
					(Over + Under ? "; " + std::to_string(Over) + " over, " + std::to_string(Under) + " darker" : ""));
		}
		// Warmth.
		{
			bool bExact = true, bDown = true;
			double Last = 0.0;
			for (double Up = 12.0; Up <= 90.0; Up += 0.25) bExact = bExact && LookOf(Lit(Up), StageSun{}, A).Kelvin == KelvinDay;
			for (double Up = 12.0; Up >= 6.0; Up -= 0.125)
			{
				const double K = LookOf(Lit(Up), StageSun{}, A).Kelvin;
				bDown = bDown && K >= Last;
				Last = K;
			}
			const Look Six = LookOf(Lit(6.0), StageSun{}, A), Five = LookOf(Lit(2.0), StageSun{}, A);
			Check("field: the key exactly 4,900 K above 12 degrees, easing to 6,500 K by 6, with the atmosphere's per-pixel transmittance on",
				bExact && bDown && Six.Kelvin == TwilightKelvin && Five.Kelvin == TwilightKelvin && Six.bPerPixelTransmittance, "6 deg " + Num(Six.Kelvin, 0) + " K");
		}
		// Overcast and rain.
		{
			Field F = Lit(30.0);
			F.Sky.bGiven = true;
			F.Sky.Kind = "deck";
			F.Sky.Cover = 1.0;
			const Look Whole = LookOf(F, StageSun{}, A);
			F.Sky.Cover = 0.5;
			const Look Half = LookOf(F, StageSun{}, A);
			F.Sky.Cover = 1.0;
			F.Sky.Rain = 1.0;
			const Look Wet = LookOf(F, StageSun{}, A);
			const Look Clear = LookOf(Lit(30.0), StageSun{}, A);
			Check("field: overcast dims the key to 0.35 and lifts the sky (and the fills, its bounce); rain dims both",
				Whole.KeyShare == OvercastKey && Whole.SkyShare == 1.0 + OvercastSky && Whole.FillShare == Whole.SkyShare && std::fabs(Half.KeyShare - 0.675) < 1e-15 &&
				Clear.KeyShare == 1.0 && Clear.SkyShare == 1.0 && Wet.KeyShare < Whole.KeyShare && Wet.SkyShare < Whole.SkyShare && Whole.SkyHigh.B != Clear.SkyHigh.B,
				"key " + Num(Whole.KeyShare, 3) + " and sky " + Num(Whole.SkyShare, 3) + " under a whole deck; " + Num(Wet.KeyShare, 3) + "/" + Num(Wet.SkyShare, 3) + " in rain");
		}
		// Exposure, the night grade, the smoke's hour, the stars, the day under the water, the unlit pieces.
		{
			bool bDay = true, bGrade = true, bSmoke = true, bStars = true, bUnder = true, bUnlit = true, bSelf = true;
			double WorstUnder = 0.0, NightUnlit = 0.0;
			for (double Up = -20.0; Up <= 70.0; Up += 0.25)
			{
				const Look L = LookOf(Lit(Up, 250.0, 30.0, 0.5), StageSun{}, A);
				if (Up >= KeyRampHigh) bDay = bDay && L.Bias == 0.0;
				if (Up <= KeyRampLow) bDay = bDay && L.Bias == L.Night.Bias;
				if (Up <= -10.0) bGrade = bGrade && L.Saturation == NightSaturation && L.BlueGain == NightBlueGain;
				if (Up >= 2.0) bGrade = bGrade && L.Saturation == 1.0 && L.BlueGain == 1.0;
				if (Up >= 10.0) bSmoke = bSmoke && L.HourLight.R == 1.0 && L.HourLight.G == 1.0 && L.HourLight.B == 1.0;
				if (Up <= -6.0) bSmoke = bSmoke && L.HourLight.R == SmokeNight[0] && L.HourLight.G == SmokeNight[1] && L.HourLight.B == SmokeNight[2];
				if (Up >= -4.0) bStars = bStars && L.StarShare == 0.0;
				if (Up <= -12.0) bStars = bStars && L.StarShare == 1.0 - 0.5 * 0.5;
				Today::FKeyStandIn Key;
				Key.Intensity = static_cast<float>(L.KeyLight);
				const double Game = Today::ReadSunDaylight(Up, &Key);
				WorstUnder = std::max(WorstUnder, std::fabs(L.Daylight - Game));
				bUnder = bUnder && L.Daylight >= 0.0 && L.Daylight <= 1.0;
				if (Up >= 6.0) bUnlit = bUnlit && L.UnlitShare == 1.0;
				if (Up <= -16.0) NightUnlit = std::max(NightUnlit, L.UnlitShare);
				bSelf = bSelf && L.SelfLitShare == 1.0;
			}
			const Look AtFour = LookOf(Lit(-4.0, 250.0, -30.0), StageSun{}, A);
			Check("field: the exposure 0 by day (above 4 degrees), +1.2 stops at -4, the night's (NightSight's) by -10", bDay && AtFour.Bias == TwilightBias, "+" + Num(AtFour.Bias, 2) + " at -4");
			const Look Dark = LookOf(Lit(-20.0, 250.0, -30.0, 0.0, 3000.0), StageSun{}, A), Moonlit = LookOf(Lit(-20.0, 250.0, 30.0, 0.5, 3000.0), StageSun{}, A),
				Full = LookOf(Lit(-20.0, 250.0, 60.0, 1.0, 3000.0), StageSun{}, A);
			Check("field: +1.8 stops on a moonlit night (a half moon 30 degrees up and brighter), +2.4 on a dark one, where the silhouette holds there",
				Moonlit.Bias == MoonlitBias && Full.Bias == MoonlitBias && Dark.Bias >= DarkBias && Dark.Bias <= MostBias && (Dark.Bias == DarkBias || Dark.Night.Bias > DarkBias),
				"moonlit +" + Num(Moonlit.Bias, 2) + ", full +" + Num(Full.Bias, 2) + ", dark +" + Num(Dark.Bias, 2));
			Check("field: the night grade, saturation 0.45 and blue 1.06 by -10 degrees (none above 2)", bGrade, "SetSeaWind's grade argument (step 11)");
			Check("field: the smoke's hour white by day and the campaign map's night from -6 degrees", bSmoke, Say3(LookOf(Lit(-20.0), StageSun{}, A).HourLight));
			Check("field: the stars faded in from -4 to -12 degrees, half as many under a full moon", bStars, "under a half moon " + Num(LookOf(Lit(-20.0, 250.0, 30.0, 0.5), StageSun{}, A).StarShare, 2));
			Check("field: the day under the water is ReadSun's (SeaUnder::DaylightOf by the key lamp's share)", bUnder && WorstUnder < 1e-6, "worst " + Sci(WorstUnder) + " (the lamp's float)");
			Check("field: the unlit pieces at 1 by day, and the night's share of the day's light at night", bUnlit && NightUnlit < 0.05, "at most " + Num(NightUnlit, 4) + " by night under a half moon");
			Check("field: the self-lit pieces (the burst flashes, the star shells' flares) at 1 by day and by night: their own light, never the day's", bSelf,
				"SelfLitShare 1 from -20 to 70 degrees under a half moon (the unlit pieces " + Num(NightUnlit, 4) + " at night)");
			double DayWorst = 0.0;
			for (double Up = -40.0; Up <= 90.0; Up += 0.01) DayWorst = std::max(DayWorst, std::fabs(DaylightOf(Up) - SeaUnder::DaylightOf(Up)));
			Check("field: DaylightOf is SeaUnder's, bit for bit", DayWorst == 0.0, "-40 to 90 degrees");
		}
		// The moon's disc and the stars.
		{
			const Look Half = LookOf(Lit(-20.0, 250.0, 30.0, 0.5), StageSun{}, A), Full = LookOf(Lit(-20.0, 250.0, 30.0, 1.0), StageSun{}, A),
				New = LookOf(Lit(-20.0, 250.0, 30.0, 0.0), StageSun{}, A), Down = LookOf(Lit(-20.0, 250.0, -5.0, 0.5), StageSun{}, A);
			const Way S = TowardOf(-20.0, 250.0), M = Half.Moon.Toward, B = Half.Moon.BrightToward;
			const double Across = B.X * M.X + B.Y * M.Y + B.Z * M.Z, Sunward = B.X * S.X + B.Y * S.Y + B.Z * S.Z;
			Check("field: the moon's disc: up, her phase by her lit share, her bright limb across her toward the sun",
				Half.Moon.bUp && !Down.Moon.bUp && Full.Moon.Phase == 0.0 && std::fabs(New.Moon.Phase - Pi) < 1e-12 && std::fabs(Half.Moon.Phase - Pi / 2.0) < 1e-12 &&
				std::fabs(Across) < 1e-12 && Sunward > 0.0 && Half.Moon.Radius == 0.26,
				"phase " + Num(Half.Moon.Phase, 4) + " at half; limb " + Num(Sunward, 3) + " sunward");
			Field F = Lit(-20.0);
			F.Seed = 11;
			const Look S1 = LookOf(F, StageSun{}, A), S2 = LookOf(F, StageSun{}, A);
			F.Seed = 12;
			const Look S3 = LookOf(F, StageSun{}, A);
			F.Sky.bGiven = true;
			F.Sky.Cover = 0.75;
			const Look Covered = LookOf(F, StageSun{}, A);
			Check("field: 2,500 seeded stars at least 1.5 pixels, the same for the same battle, hidden by the cover",
				S1.Stars.Count == 2500 && S1.Stars.LeastPixels == 1.5 && S1.Stars.Seed == S2.Stars.Seed && S1.Stars.Seed != S3.Stars.Seed && Covered.Stars.Visible == 0.25 * Covered.Stars.Share,
				"seed " + std::to_string(S1.Stars.Seed));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// No step over a tenth for a minute of sun (a quarter degree at most).

	struct Steps
	{
		std::map<std::string, double> Most; // the worst step, as a share (or stops, or absolute) by what
		std::map<std::string, std::string> Where;
		void Take(const std::string& What, double Step, const std::string& At)
		{
			if (Step > Most[What])
			{
				Most[What] = Step;
				Where[What] = At;
			}
		}
	};

	/** The light on the level sea (the test's own: the key's lamp times the sine of its height, the sky light, two fills). */
	double OnTheSea(const Look& L) { return L.KeyLight * std::max(0.0, L.Key.Toward.Z) + L.SkyLight + 2.0 * L.FillLight; }

	/** A sun in the twilight band, where the vis fog comes in (the plan's ruling: not held to the tenth there, and only
	 * where the field gives vis). */
	bool InTwilight(double Up) { return Up > VisFogLowest && Up < VisFogFull; }
	/** A sun in the no-vis haze's band, from the night's air at -18 degrees to the day's at +2 (held to the tenth). */
	bool InNoVisBand(double Up) { return Up > AirSeaFrom && Up < AirSeaFull; }

	void StepOf(Steps& S, const Look& A, const Look& B, double Cmin, const std::string& At)
	{
		// (the key by what it delivers: all the light on the level sea, and the key's own there with its way, the lamp's
		// light on the water as a vector, of all the light there)
		const double SeaA = OnTheSea(A), SeaB = OnTheSea(B);
		S.Take("the light on the level sea", Ratio(SeaA, SeaB) - 1.0, At);
		{
			const double KA = A.KeyLight * std::max(0.0, A.Key.Toward.Z), KB = B.KeyLight * std::max(0.0, B.Key.Toward.Z);
			const double DX = KA * A.Key.Toward.X - KB * B.Key.Toward.X, DY = KA * A.Key.Toward.Y - KB * B.Key.Toward.Y, DZ = KA * A.Key.Toward.Z - KB * B.Key.Toward.Z;
			S.Take("the key on the water, with its way", std::sqrt(DX * DX + DY * DY + DZ * DZ) / std::max(SeaA, SeaB), At);
		}
		S.Take("fills", Ratio(A.FillLight, B.FillLight) - 1.0, At);
		S.Take("sky light", Ratio(A.SkyLight, B.SkyLight) - 1.0, At);
		S.Take("unlit pieces", Ratio(A.UnlitShare, B.UnlitShare) - 1.0, At);
		S.Take("the key's colour", RatioOf(KelvinColour(A.Kelvin), KelvinColour(B.Kelvin)) - 1.0, At);
		S.Take("sky colours", std::max({ RatioOf(A.SeaSkyLight, B.SeaSkyLight), RatioOf(A.SkyHigh, B.SkyHigh), RatioOf(A.SkyToward, B.SkyToward), RatioOf(A.SkyAway, B.SkyAway),
			RatioOf(A.SkyOnScreen, B.SkyOnScreen) }) - 1.0, At);
		S.Take("smoke's hour", RatioOf(A.HourLight, B.HourLight) - 1.0, At);
		S.Take("exposure (as light)", std::exp2(std::fabs(A.Bias - B.Bias)) - 1.0, At);
		S.Take("grade", std::max(Ratio(A.Saturation, B.Saturation), Ratio(A.BlueGain, B.BlueGain)) - 1.0, At);
		S.Take("fog colour", RatioOf(A.Air.Colour, B.Air.Colour) - 1.0, At);
		S.Take("sun-side haze (of its full 0.3)", std::max({ std::fabs(A.Air.SunColour.R - B.Air.SunColour.R), std::fabs(A.Air.SunColour.G - B.Air.SunColour.G),
			std::fabs(A.Air.SunColour.B - B.Air.SunColour.B) }) / InscatterShare, At);
		// (the haze: held to the tenth everywhere but in the twilight band of a field that gives vis, where the night's air
		// gives way to the rules' vis over six degrees, reported apart: the plan's ruling, which has nothing to give way to
		// with no vis. Outside the band it moves in, exactly unmoved: with vis -6 to 0 degrees, with none -18 to +2.)
		const bool bVis = A.Air.Vis > 0.0 || B.Air.Vis > 0.0;
		const bool bBand = bVis && (InTwilight(A.SunUp) || InTwilight(B.SunUp));
		const bool bMoves = bVis ? bBand : InNoVisBand(A.SunUp) || InNoVisBand(B.SunUp);
		const std::string Fogged = bBand ? "fog in the twilight band with vis (not held: the ruling)" : "fog (what it lets through, of at least Cmin)";
		const std::string Material = bBand ? "the sea material's own haze in the twilight band with vis (not held: the ruling)" : "the sea material's own haze (absolute)";
		const Fog FA = FogOf(A.Air), FB = FogOf(B.Air);
		double Moved = 0.0;
		for (const double Eye : { 14.0, 260.0, 1300.0 })
		{
			for (const double R : { 500.0, 1000.0, 2000.0, 2500.0, 3500.0, 5000.0, 10000.0, 20000.0, 40000.0 })
			{
				const double TA = FogSeen(FA, Eye * Unit, 0.0, R * Unit), TB = FogSeen(FB, Eye * Unit, 0.0, R * Unit);
				S.Take(Fogged, std::fabs(TA - TB) / std::max({ TA, TB, Cmin }), At + ", eye " + Num(Eye, 0) + " m, " + Num(R, 0) + " m");
				Moved = std::max(Moved, std::fabs(TA - TB));
			}
		}
		for (const double D : { 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0, 50000.0, 100000.0, 200000.0 })
		{
			const float HA = MaterialHazed(static_cast<float>(D * Unit), static_cast<float>(A.Air.MaterialReach * Unit));
			const float HB = MaterialHazed(static_cast<float>(D * Unit), static_cast<float>(B.Air.MaterialReach * Unit));
			const double Step = std::max(static_cast<double>(std::fabs(HA - HB)), std::fabs(HA * A.Air.MaterialTop - HB * B.Air.MaterialTop));
			S.Take(Material, Step, At + ", " + Num(D / 1000.0, 1) + " km");
			Moved = std::max(Moved, Step);
		}
		if (!bMoves) S.Take(bVis ? "the haze outside the twilight band, by the sun, with vis (unmoved)" : "the haze outside the no-vis band, by the sun (unmoved)", Moved, At);
	}

	void Continuity()
	{
		Asked A = FieldAsk();
		Steps S;
		int Sweeps = 0;
		struct Place { double Lat; int Month; };
		const Place Places[] = { { 55.0, 11 }, { 33.0, 6 }, { 11.7, 9 }, { 57.5, 4 } };
		struct Night { double MoonUp, MoonLit; };
		const Night Nights[] = { { -20.0, 0.0 }, { 30.0, 0.5 }, { 60.0, 1.0 }, { 4.0, 0.3 } };
		// (at the default Cmin over every place, and at -TheaterSeaVisContrast=0.02 and 0.1 over the first)
		for (const double Cmin : { VisContrast, 0.02, 0.1 })
		{
		A.VisContrast = Cmin;
		for (const Place& P : Places)
		{
			if (Cmin != VisContrast && &P != &Places[0]) continue;
			for (const Night& N : Nights)
			{
				for (const double Vis : { 0.0, 300.0, 800.0, 1500.0, 2500.0, 3500.0, 6000.0, 20000.0 })
				{
					for (int Sky = 0; Sky < 4; ++Sky)
					{
						Look Last;
						for (int K = 0; K <= 352; ++K)
						{
							const double Up = -18.0 + 0.25 * K;
							Field F = Lit(Up, 200.0 + Up, N.MoonUp, N.MoonLit, Vis, P.Lat, P.Month);
							F.Wind = Sky == 2 ? 18.0 : Sky == 3 ? 26.0 : 7.0;
							if (Sky > 0)
							{
								// (a low deck; squalls in rain and a strong wind; a gale's whole deck in heavy rain)
								F.Sky.bGiven = true;
								F.Sky.Kind = Sky == 1 ? "deck" : Sky == 2 ? "squalls" : "deck";
								F.Sky.Cover = Sky == 1 ? 0.8 : Sky == 2 ? 0.6 : 1.0;
								F.Sky.Rain = Sky == 1 ? 0.0 : Sky == 2 ? 0.5 : 1.0;
							}
							const Look L = LookOf(F, StageSun{}, A);
							if (K > 0) StepOf(S, Last, L, A.VisContrast, "sun " + Num(Up, 2) + ", vis " + Num(Vis, 0) + ", moon " + Num(N.MoonUp, 0) + "/" + Num(N.MoonLit, 1) + ", sky " + std::to_string(Sky) + ", Cmin " + Num(Cmin, 2));
							Last = L;
						}
						++Sweeps;
					}
				}
			}
		}
		}
		A.VisContrast = VisContrast;
		// (and real minutes of real days: the field's hour, round the clock)
		int Minutes = 0;
		const Place Days[] = { { 55.0, 11 }, { 33.0, 6 }, { 70.0, 5 }, { 0.0, 2 }, { -40.0, 6 } };
		for (const Place& P : Days)
		{
			Look Last;
			for (int Minute = 0; Minute <= 24 * 60; ++Minute)
			{
				Field F;
				F.Lat = P.Lat;
				F.Month = P.Month;
				F.bHour = true;
				F.Hour = Minute / 60.0;
				const Look L = LookOf(F, StageSun{}, A);
				if (Minute > 0) StepOf(S, Last, L, A.VisContrast, "minute " + std::to_string(Minute) + " at " + Num(P.Lat, 0) + " month " + std::to_string(P.Month));
				Last = L;
				++Minutes;
			}
		}
		std::string Said, Band;
		bool bOk = true;
		for (const auto& [What, Most] : S.Most)
		{
			if (What.find("twilight band with vis (not held") != std::string::npos)
			{
				Band += What + (Most > 0.0 ? " " + Num(Most * 100.0, 1) + "% at " + S.Where[What] : std::string(": unmoved (made nothing with vis)")) + "; ";
				continue;
			}
			if (What.find("(unmoved)") != std::string::npos) continue;
			bOk = bOk && Most <= 0.1;
			Said += What + " " + Num(Most * 100.0, 1) + "%; ";
		}
		Check("continuity: from -18 to 70 degrees of sun, no quarter degree (a minute's sun at most) and no real minute moves the light on the level sea, the key's there with its way, the "
			"fills, the sky, the colours, the exposure, the grade or the fog (with no vis everywhere; with vis outside the -6 to 0 twilight band, the ruling's) by more than a tenth",
			bOk && Sweeps > 500, std::to_string(Sweeps) + " sweeps (Cmin 0.05, and 0.02 and 0.1) and " + std::to_string(Minutes) + " minutes; worst " + Said);
		for (const auto& [What, Most] : S.Most)
		{
			if (Most > 0.1 && What.find("twilight band with vis (not held") == std::string::npos && What.find("(unmoved)") == std::string::npos) Note("  " + What + ": " + Num(Most * 100.0, 1) + "% at " + S.Where[What]);
		}
		// (and the no-vis fields' own fog, held to the tenth through their band, said apart)
		{
			Steps NoVis;
			for (int Sky = 0; Sky < 4; ++Sky)
			{
				for (const double Cmin : { VisContrast, 0.02, 0.1 })
				{
					Asked AN = FieldAsk();
					AN.VisContrast = Cmin;
					Look Last;
					for (int K = 0; K <= 352; ++K)
					{
						const double Up = -18.0 + 0.25 * K;
						Field F = Lit(Up, 200.0 + Up, -20.0, 0.0, 0.0, 55.0, 11);
						F.Wind = Sky == 2 ? 18.0 : Sky == 3 ? 26.0 : 7.0;
						if (Sky > 0)
						{
							F.Sky.bGiven = true;
							F.Sky.Kind = Sky == 1 ? "deck" : Sky == 2 ? "squalls" : "deck";
							F.Sky.Cover = Sky == 1 ? 0.8 : Sky == 2 ? 0.6 : 1.0;
							F.Sky.Rain = Sky == 1 ? 0.0 : Sky == 2 ? 0.5 : 1.0;
						}
						const Look L = LookOf(F, StageSun{}, AN);
						if (K > 0) StepOf(NoVis, Last, L, Cmin, "sun " + Num(Up, 2) + ", sky " + std::to_string(Sky) + ", Cmin " + Num(Cmin, 2));
						Last = L;
					}
				}
			}
			const double Fog = NoVis.Most["fog (what it lets through, of at least Cmin)"], Material = NoVis.Most["the sea material's own haze (absolute)"];
			Check("continuity: with no vis the haze is held to the tenth through its band too (the night's air at -18 degrees to the day's at +2, clear to a gale's rain, Cmin 0.02 to 0.1)",
				Fog > 0.0 && Fog <= 0.1 && Material > 0.0 && Material <= 0.1 && NoVis.Most.count("fog in the twilight band with vis (not held: the ruling)") == 0,
				"the fog at most " + Num(Fog * 100.0, 1) + "% (" + NoVis.Where["fog (what it lets through, of at least Cmin)"] + "), the sea material's own " + Num(Material * 100.0, 1) +
					"% (" + NoVis.Where["the sea material's own haze (absolute)"] + ")");
		}
		const auto Unmoved = S.Most.find("the haze outside the twilight band, by the sun, with vis (unmoved)"), UnmovedNoVis = S.Most.find("the haze outside the no-vis band, by the sun (unmoved)");
		Check("continuity: outside its band the haze (the fog, the sea material's own) does not go by the sun at all: with vis the night's air at -6 and below and the rules' from the horizon "
			"up; with none the night's air at -18 and below and the day's from +2 up",
			Unmoved != S.Most.end() && Unmoved->second == 0.0 && UnmovedNoVis != S.Most.end() && UnmovedNoVis->second == 0.0,
			(Unmoved == S.Most.end() ? std::string("with vis not swept") : "with vis most " + Sci(Unmoved->second)) + "; " +
				(UnmovedNoVis == S.Most.end() ? std::string("with none not swept") : "with none most " + Sci(UnmovedNoVis->second)));
		Note("continuity: in the twilight band with vis (the ruling: the rules' vis by day before the tenth a minute there; LookOf runs once a battle) " + Band);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The haze's rules.

	void HazeRules()
	{
		DayShim::Line.clear();
		const std::vector<double> Eyes = Planned();
		// Never a vis fog at -6 degrees and below.
		{
			int Tried = 0, Wrong = 0, Same = 0;
			for (const double Vis : { 500.0, 1000.0, 2500.0, 3500.0, 8000.0, 20000.0 })
			{
				for (double Up = -24.0; Up <= VisFogLowest; Up += 0.25)
				{
					for (const double Rain : { 0.0, 0.6 })
					{
						HazeAsk H;
						H.Vis = Vis;
						H.SunUp = Up;
						H.Rain = Rain;
						H.Cover = Rain > 0.0 ? 1.0 : 0.0;
						H.Kind = Rain > 0.0 ? "squalls" : "";
						H.Eyes = Eyes;
						const Haze Z = HazeFor(H);
						// (as with no vis at night: the no-vis haze is wholly the night's own air at -18 degrees and below)
						HazeAsk NoVis = H;
						NoVis.Vis = 0.0;
						NoVis.SunUp = std::min(Up, AirSeaFrom);
						const Haze Plain = HazeFor(NoVis);
						++Tried;
						Wrong += Z.VisShare != 0.0 || Z.Kind != HazeKind::Night || Z.Start != 0.0 || std::fabs(Z.Beta - Z.NightBeta) > 1e-12 * Z.NightBeta;
						if (Z.NightBeta == Plain.NightBeta)
						{
							++Same;
							Wrong += Z.Density != Plain.Density || Z.Falloff != Plain.Falloff;
						}
					}
				}
			}
			Check("haze: the vis is never a fog with the sun at -6 degrees or below: the night's own air from the eye, as with no vis at night (unless it would hide a ship the rules see)",
				Wrong == 0, std::to_string(Tried) + " nights, " + std::to_string(Same) + " the same air as with no vis" + (Wrong ? "; " + std::to_string(Wrong) + " wrong" : ""));
		}
		// By day with vis: every vis the rules may give and every sun from the horizon up (a winter noon on the North Sea is
		// 10.6 degrees), through LookOf, at the default Cmin and at -TheaterSeaVisContrast=0.02 and 0.1, clear and in squalls.
		{
			const TodayScene Scene = BuiltInScene(Unit);
			int Suns = 0, Wrong = 0, Tinted = 0;
			double WorstEye = 1e9, LookoutMost = 0.0, FarMost = 0.0;
			std::string Bad;
			for (const double Cmin : { VisContrast, 0.02, 0.1 })
			{
				Asked A = FieldAsk();
				A.VisContrast = Cmin;
				for (const double Vis : { 300.0, 500.0, 800.0, 1200.0, 1500.0, 2000.0, 2500.0, 2900.0, 3500.0, 5000.0, 8000.0, 12000.0, 20000.0, 30000.0 })
				{
					for (int Wet = 0; Wet < 2; ++Wet)
					{
						for (double Up = VisFogFull; Up <= 70.0; Up += 0.25)
						{
							Field F = Lit(Up, 200.0, -30.0, 0.0, Vis, 55.0, 11);
							if (Wet)
							{
								F.Sky.bGiven = true;
								F.Sky.Kind = "squalls";
								F.Sky.Cover = 0.7;
								F.Sky.Rain = 0.6;
								F.Wind = 18.0;
							}
							const Look L = LookOf(F, StageSun{}, A);
							const Fog G = FogOf(L.Air);
							++Suns;
							const double Beta = -std::log(Cmin) / Vis;
							// (the fog's colour HazeInk under the sky's own tint: HazeInk itself wherever the sky is today's)
							const Rgb Tint{ L.SeaSkyLight.R / Scene.SeaSkyLight.R, L.SeaSkyLight.G / Scene.SeaSkyLight.G, L.SeaSkyLight.B / Scene.SeaSkyLight.B };
							const bool bToday = Tint.R == 1.0 && Tint.G == 1.0 && Tint.B == 1.0;
							Tinted += !bToday;
							const bool bInk = bToday ? SameColour(L.Air.Colour, FLinearColor(HazeInkToday[0], HazeInkToday[1], HazeInkToday[2]))
													 : RatioOf(L.Air.Colour, Rgb{ HazeInkToday[0] * Tint.R, HazeInkToday[1] * Tint.G, HazeInkToday[2] * Tint.B }) - 1.0 < 1e-12;
							const Rgb Sun = Scale3(KelvinColour(L.Kelvin), InscatterShare * std::min(1.0, L.KeyShare));
							const bool bOk = L.Air.Kind == HazeKind::Vis && L.Air.VisShare == 1.0 && L.Air.Start == 0.0 && std::fabs(L.Air.Beta - Beta) < 1e-9 * Beta && L.Air.bSun &&
								L.Air.SunExponent == InscatterExponent && L.Air.SunStart == InscatterStart && std::fabs(L.Air.SunColour.R - Sun.R) < 1e-15 && std::fabs(L.Air.SunColour.G - Sun.G) < 1e-15 &&
								std::fabs(L.Air.SunColour.B - Sun.B) < 1e-15 && !L.Air.bMaterialToday && L.Air.MaterialTop == 0.0 && L.Air.MaterialReach * Unit == MaterialReachNever && bInk;
							// (every planned eye sees a ship at vis; from the lookout she keeps little more than Cmin, and at 1.5 vis less)
							double Worst = 1.0;
							for (const double Eye : Eyes) Worst = std::min(Worst, FogSeen(G, Eye * Unit, 0.0, Vis * Unit));
							const double AtVis = FogSeen(G, PlannedEyes[0] * Unit, 0.0, Vis * Unit), Beyond = FogSeen(G, PlannedEyes[0] * Unit, 0.0, 1.5 * Vis * Unit);
							WorstEye = std::min(WorstEye, Worst / Cmin);
							LookoutMost = std::max(LookoutMost, AtVis / Cmin);
							FarMost = std::max(FarMost, Beyond / Cmin);
							if (!bOk || Worst < Cmin * (1.0 - 1e-9) || AtVis > 1.25 * Cmin || Beyond >= Cmin)
							{
								if (!Wrong++) Bad = "first at sun " + Num(Up, 2) + ", vis " + Num(Vis, 0) + ", Cmin " + Num(Cmin, 2) + (Wet ? ", squalls" : "");
							}
						}
					}
				}
			}
			Check("haze: from the horizon up the rules' vis is the fog, at every sun: beta -ln(Cmin)/vis from the eye (2,500 m: 1.2e-3 a metre), the sun-side haze at 0.3 of her colour (by her key's "
				"share), exponent 8, from 2 km, in HazeInk (under the sky's own tint at a low sun); every planned eye sees a ship at vis, the lookout under 1.25 Cmin there and under Cmin at 1.5 vis",
				Wrong == 0 && Suns > 20000 && Tinted > 0,
				std::to_string(Suns) + " days (vis 300 m to 30 km, every quarter degree from 0 to 70, Cmin 0.02 to 0.1, clear and squalls; " + std::to_string(Tinted) + " under a low sun's tint); worst eye " +
					Num(WorstEye, 4) + " Cmin, the lookout at most " + Num(LookoutMost, 3) + " Cmin at vis and " + Num(FarMost, 3) + " at 1.5 vis" + (Wrong ? "; " + std::to_string(Wrong) + " wrong, " + Bad : ""));
			// (where the vis fog comes in: none at -6 degrees, wholly by the horizon, on a clear night's air and in squalls)
			int Bands = 0, BandWrong = 0;
			for (const double Vis : { 300.0, 800.0, 2500.0, 3500.0, 12000.0, 30000.0 })
			{
				for (int Wet = 0; Wet < 2; ++Wet)
				{
					HazeAsk H;
					H.Vis = Vis;
					H.Eyes = Eyes;
					H.Rain = Wet ? 0.5 : 0.0;
					H.Cover = Wet ? 0.6 : 0.0;
					H.Kind = Wet ? "squalls" : "";
					auto At = [&](double Up)
					{
						H.SunUp = Up;
						return HazeFor(H);
					};
					const Haze Low = At(VisFogLowest), Under = At(VisFogFull - 0.25), Full = At(VisFogFull);
					++Bands;
					BandWrong += !(Low.VisShare == 0.0 && Low.Kind == HazeKind::Night && Under.VisShare > 0.9 && Under.VisShare < 1.0 && Under.Kind == HazeKind::Vis && Full.VisShare == 1.0);
				}
			}
			Check("haze: the vis fog comes in over the twilight from -6 degrees (none there) and is wholly in by the horizon, whatever the night's air", BandWrong == 0,
				std::to_string(Bands) + " vis and skies" + (BandWrong ? "; " + std::to_string(BandWrong) + " wrong" : ""));
		}
		// MaterialHaze: the sea material's own haze term exactly nothing.
		{
			int Tried = 0, Moved = 0;
			const float Reach = static_cast<float>(MaterialReachNever);
			for (double D = 1.0; D <= 1.0e8; D *= 1.07)
			{
				const float H = MaterialHazed(static_cast<float>(D), Reach);
				for (const float Seen : { 0.0007f, 0.013f, 0.08f, 0.31f, 0.57f, 1.0f, 1.9f })
				{
					for (const float Sky : { 0.4f, 0.62f, 0.9f })
					{
						const float Lit1 = Seen + H * (Sky * 0.0f - Seen);
						const float Lit2 = Seen * (1.0f - H) + Sky * 0.0f * H;
						const float Lit3 = std::fma(H, Sky * 0.0f - Seen, Seen);
						const float Dim = 1.0f - H * 1.0f;
						++Tried;
						Moved += Lit1 != Seen || Lit2 != Seen || Lit3 != Seen || Dim != 1.0f;
					}
				}
			}
			const float Today5 = MaterialHazed(static_cast<float>(10000.0 * Unit), static_cast<float>(MaterialReachToday * Unit));
			Check("haze: MaterialHaze (HazeTop 0, HazeReach 1e12 units) makes the sea material's own haze term exactly nothing in float, to 1,000 km (five times the sea's edge; HazeTop 0 alone would darken the far sea)",
				Moved == 0, std::to_string(Tried) + " pixels, every lerp form; HazeTop 0 with today's reach would take " + Num(Today5 * 100.0, 0) + "% of the sea's light at 10 km");
		}
		// With no vis: the air scene's sea haze by day, with the weather's own air where the field's sky thickens it.
		{
			UExponentialHeightFogComponent Air;
			FAirStateStandIn OverSea;
			OverSea.Held.Board.StageKey = "open-sea";
			Today::AirFog(&Air, Unit, OverSea);
			UExponentialHeightFogComponent Land;
			FAirStateStandIn OverLand;
			OverLand.Held.Board.Land = { 1 };
			Today::AirFog(&Land, Unit, OverLand);
			const FLinearColor SeaInk = Today::AirParts::SeaHazeInk();
			const TodayScene Scene = BuiltInScene(Unit);
			// (the skies: none, fair-weather cumulus that adds nothing to clear air; then squalls in rain and a strong wind, a
			// low deck with no rain, a gale's spray alone, a whole deck in heavy rain)
			struct Weathered { const char* Name; const char* Kind; double Cover, Rain, Wind; };
			const Weathered Skies[] = { { "clear", "", 0.0, 0.0, 7.0 }, { "cumulus", "cumulus", 0.5, 0.0, 8.0 }, { "squalls", "squalls", 0.7, 0.6, 18.0 },
				{ "a low deck", "deck", 0.8, 0.0, 7.0 }, { "spray", "", 0.0, 0.0, 24.0 }, { "a deck in heavy rain", "deck", 1.0, 1.0, 26.0 } };
			int Wrong = 0, Tried = 0, Tinted = 0, Under = 0, WetWrong = 0, WetTried = 0, WetThinned = 0;
			double Top = 1e9, WetTop = 1e9, LeastByDay = 1e9, MostOverLeast = 0.0, MostTimes = 0.0, LeastTimes = 1e9, MostNight = 0.0;
			std::string Bad, WetBad, Times;
			for (const double Cmin : { VisContrast, 0.02, 0.1 })
			{
				Asked AC = FieldAsk();
				AC.VisContrast = Cmin;
				const double Gap = std::fabs(SeaOverSky - HullOverSkyDay);
				const double Least = std::max(Cmin / (Gap + Cmin * (1.0 - SeaOverSky)), Cmin / (1.0 - HullOverSkyDay));
				for (const Weathered& W : Skies)
				{
					const bool bWet = std::string(W.Name) != "clear" && std::string(W.Name) != "cumulus";
					auto FieldAt = [&](double Up)
					{
						Field F = Lit(Up, 200.0, -30.0, 0.0, 0.0, 55.0, 11);
						F.Wind = W.Wind;
						if (*W.Kind || W.Cover > 0.0 || W.Rain > 0.0)
						{
							F.Sky.bGiven = true;
							F.Sky.Kind = W.Kind;
							F.Sky.Cover = W.Cover;
							F.Sky.Rain = W.Rain;
						}
						return F;
					};
					// (the night's own air under this sky, and under a clear one: the weather's own is the difference)
					const Look Night = LookOf(FieldAt(-24.0), StageSun{}, AC), ClearNight = LookOf(Lit(-24.0, 200.0, -30.0, 0.0, 0.0, 55.0, 11), StageSun{}, AC);
					const double WeatherOwn = Night.Air.NightOwnBeta - ClearNight.Air.NightOwnBeta;
					for (double Up = AirSeaFrom; Up <= 70.0; Up += 0.25)
					{
						const Look L = LookOf(FieldAt(Up), StageSun{}, AC);
						if (Up < AirSeaFull)
						{
							// (under +2 degrees the night's air comes in: not yet the day's)
							++Under;
							Wrong += L.Air.Kind == HazeKind::AirSea || L.Air.bMaterialToday;
							continue;
						}
						// (its colour SeaHazeInk under the sky's own tint at a low sun: SeaHazeInk itself wherever the sky is today's)
						const Rgb Tint{ L.SeaSkyLight.R / Scene.SeaSkyLight.R, L.SeaSkyLight.G / Scene.SeaSkyLight.G, L.SeaSkyLight.B / Scene.SeaSkyLight.B };
						const bool bToday = Tint.R == 1.0 && Tint.G == 1.0 && Tint.B == 1.0;
						const bool bInk = bToday ? SameColour(L.Air.Colour, Air.FogInscatteringColor) && SameColour(L.Air.Colour, SeaInk)
												 : RatioOf(L.Air.Colour, Rgb{ SeaInk.R * Tint.R, SeaInk.G * Tint.G, SeaInk.B * Tint.B }) - 1.0 < 1e-12;
						const bool bMaterial = L.Air.bMaterialToday && L.Air.MaterialTop == MaterialTopToday && L.Air.MaterialReach == MaterialReachToday && !L.Air.bSun;
						if (!bWet)
						{
							Top = std::min(Top, Up);
							++Tried;
							Tinted += !bToday;
							const bool bOk = L.Air.Kind == HazeKind::AirSea && static_cast<float>(L.Air.Start * Unit) == Air.StartDistance && SameFloat(L.Air.Density, Air.FogDensity) &&
								SameFloat(L.Air.Falloff, Air.FogHeightFalloff) && SameFloat(L.Air.MostOpacity, Air.FogMaxOpacity) && bInk && bMaterial && L.Air.WeatherBeta == 0.0;
							if (!bOk && !Wrong++) Bad = "first at sun " + Num(Up, 2) + " " + W.Name + ", Cmin " + Num(Cmin, 2);
							continue;
						}
						// (the weather's own air added to the air scene's: the density the two's at the sea, the start the air scene's
						// 30 km times the square of its share of the air; the weather's the night's own less clear air's, never
						// thicker, its part thinned only so a hull lit by day at Hunt's 4 km keeps her silhouette from every planned
						// eye, and only as far as it must)
						WetTop = std::min(WetTop, Up);
						++WetTried;
						const double WeatherDensity = DensityOfBeta(L.Air.WeatherBeta, Unit);
						const double Density = static_cast<double>(Air.FogDensity) + WeatherDensity;
						const Fog G = FogOf(L.Air);
						double Worst = 1.0;
						for (const double Eye : PlannedEyes) Worst = std::min(Worst, FogSeen(G, Eye * Unit, 0.0, ClearNightSees * Unit));
						double Edge = 1.0;
						for (const double Eye : PlannedEyes) Edge = std::min(Edge, FogSeen(G, Eye * Unit, 0.0, 1.02 * ClearNightSees * Unit));
						LeastByDay = std::min(LeastByDay, Worst / Least);
						if (L.Air.bWeatherThinned)
						{
							++WetThinned;
							MostOverLeast = std::max(MostOverLeast, Edge / Least);
						}
						const double Share = static_cast<double>(Air.FogDensity) / Density;
						const double DayTimes = L.Air.Beta / BetaOfDensity(Air.FogDensity, Unit), NightTimes = L.Air.Beta / Night.Air.NightBeta;
						MostTimes = std::max(MostTimes, DayTimes);
						LeastTimes = std::min(LeastTimes, DayTimes);
						// (the same air by day as by night where the weather is most of the night's own: squalls and heavy rain, not a
						// dry deck's or a moderate spray's, where the clear air under them is half of it or more)
						if (WeatherOwn >= 0.5 * Night.Air.NightOwnBeta) MostNight = std::max(MostNight, std::max(NightTimes, 1.0 / NightTimes));
						if (Up == AirSeaFull && Cmin == VisContrast) Times += std::string(W.Name) + " " + Num(DayTimes, 1) + " times, from " + Num(L.Air.Start, 0) + " m (its night's " + Num(1.0 / NightTimes, 2) + " of it); ";
						const bool bOk = L.Air.Kind == HazeKind::AirSea && L.Air.WeatherBeta > 0.0 && std::fabs(L.Air.Density - Density) <= 1e-12 * Density &&
							std::fabs(L.Air.Start - AirSeaStart * Share * Share) <= 1e-9 * AirSeaStart && SameFloat(L.Air.Falloff, Air.FogHeightFalloff) &&
							SameFloat(L.Air.MostOpacity, Air.FogMaxOpacity) && bInk && bMaterial && std::fabs(L.Air.WeatherOwnBeta - WeatherOwn) <= 1e-12 * WeatherOwn &&
							L.Air.WeatherBeta <= L.Air.WeatherOwnBeta * (1.0 + 1e-12) && (L.Air.bWeatherThinned || std::fabs(L.Air.WeatherBeta - WeatherOwn) <= 1e-12 * WeatherOwn) &&
							Worst >= Least * (1.0 - 1e-9) && Edge >= Least * (1.0 - 1e-9) && (!L.Air.bWeatherThinned || Edge <= Least * (1.0 + 1e-6));
						if (!bOk && !WetWrong++)
							WetBad = "first at sun " + Num(Up, 2) + " " + W.Name + ", Cmin " + Num(Cmin, 2) + ": " + Sci(L.Air.Beta) + " a metre from " + Num(L.Air.Start, 0) + " m, the worst eye at 4 km " + Num(Worst, 3) +
								" (least " + Num(Least, 3) + ")";
					}
				}
			}
			Check("haze: with no vis, at every sun from +2 degrees up under a clear sky or fair-weather cumulus the air scene's sea haze (AAirBattle's own lines: from 30 km at 1.2 times, in SeaHazeInk, "
				"under the sky's own tint at a low sun) and today's material haze; the night's air coming in under +2",
				Wrong == 0 && Tried > 1000 && Top == AirSeaFull && Tinted > 0 && Land.StartDistance == static_cast<float>(7500.0 * Unit),
				std::to_string(Tried) + " suns from " + Num(Top, 2) + " degrees (" + std::to_string(Tinted) + " under a low sun's tint; the night's air coming in at " + std::to_string(Under) +
					" below, every sky); " + Num(Air.StartDistance / Unit, 0) + " m at " + Sci(Air.FogDensity) + (Wrong ? "; " + std::to_string(Wrong) + " wrong, " + Bad : ""));
			Check("haze: with no vis, by day the weather's own air (rain, a low deck, squalls, spray: the night's own air less clear air's) is added to the air scene's haze, its start drawn in "
				"by the square of the air scene's share, never thicker than its own and thinned only as far as a hull lit by day at Hunt's 4 km needs to keep her silhouette from every planned eye: "
				"where the weather is most of the night's air, the same air by day as by night within a factor of 2 (the two thinnings' difference)",
				WetWrong == 0 && WetTried > 1000 && WetTop == AirSeaFull && WetThinned > 0 && LeastByDay >= 1.0 - 1e-9 && MostNight < 2.0,
				std::to_string(WetTried) + " suns from " + Num(WetTop, 2) + " degrees (Cmin 0.02 to 0.1), " + std::to_string(WetThinned) + " thinned (the worst eye just past 4 km then at most " +
					Num(MostOverLeast, 6) + " of the least it must keep); the day's air " + Num(LeastTimes, 1) + " to " + Num(MostTimes, 1) + " times the air scene's, within " + Num(MostNight, 2) +
					" times of its night's where the weather is most of it; at Cmin 0.05 and +2 degrees: " + Times + "the worst eye at 4 km " + Num(LeastByDay, 3) + " of the least" +
					(WetWrong ? "; " + std::to_string(WetWrong) + " wrong, " + WetBad : ""));
		}
		// The night's own air.
		{
			HazeAsk H;
			H.SunUp = -20.0;
			const double Clear = HazeFor(H).NightBeta;
			H.Rain = 0.5;
			const double Rain = HazeFor(H).NightBeta;
			H.Rain = 0.0;
			H.Kind = "deck";
			H.Cover = 1.0;
			const double Deck = HazeFor(H).NightBeta;
			H.Kind = "";
			H.Cover = 0.0;
			H.Wind = 20.0;
			const double Spray = HazeFor(H).NightBeta;
			Check("haze: the night's air a clear 25 km (2% contrast) with no sky given, thicker in rain, under a low deck and in a gale's spray",
				std::fabs(Clear - 3.912023005428146 / ClearAirRange) < 1e-12 && Rain > Clear && Deck > Clear && Spray > Clear,
				Sci(Clear) + " a metre; rain 0.5 " + Sci(Rain) + ", a whole deck " + Sci(Deck) + ", 20 m/s " + Sci(Spray));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The sightings: by day the fog keeps each at Cmin; at night they are seen through light.

	/** A hull's silhouette at Range, from the look alone (the test's own sums, not NightSight's): the fog the engine is
	 * given (FogOf(L.Air)) from every planned eye; the horizon the look shows (its SeaSkyLight) behind her and in the air
	 * between; the sea at SeaOverSky of it; her lit side the day's HullOverSkyDay of the day's horizon, by the look's own key
	 * and sky light against the day's (or a dark hull, nothing); on the screen at the look's own exposure. */
	struct Silhouette
	{
		double Seen = 1.0, Sky = 0.0, Sea = 0.0, LevelsSky = 0.0, LevelsSea = 0.0;
	};
	Silhouette SilhouetteOf(const Look& L, double Range, bool bDark)
	{
		Silhouette S;
		const Fog G = FogOf(L.Air);
		for (const double Eye : PlannedEyes) S.Seen = std::min(S.Seen, FogSeen(G, Eye * Unit, 0.0, Range * Unit));
		const double Day = Lum(BuiltInScene(Unit).SeaSkyLight), Horizon = Lum(L.SeaSkyLight);
		const double Hull = bDark ? 0.0 : HullOverSkyDay * Day * (L.KeyLight + L.SkyLight) / (KeyDay + SkyDay);
		const double HullFar = Hull * S.Seen + Horizon * (1.0 - S.Seen), SeaFar = SeaOverSky * Horizon * S.Seen + Horizon * (1.0 - S.Seen);
		S.Sky = std::fabs(Horizon - HullFar) / Horizon;
		S.Sea = std::fabs(SeaFar - HullFar) / SeaFar;
		auto Pixel = [&](double V) { return 255.0 * StillOf(Scale3(Rgb{ 1.0, 1.0, 1.0 }, V * std::exp2(L.Bias)), FitRed, FitBlue).G; };
		S.LevelsSky = std::fabs(Pixel(Horizon) - Pixel(HullFar));
		S.LevelsSea = std::fabs(Pixel(SeaFar) - Pixel(HullFar));
		return S;
	}

	void Sightings()
	{
		const std::vector<double> Eyes = Planned();
		long Seen = 0;
		int Suns = 0;
		double Worst = 1e9, OwnWorst = 1e9;
		std::string Each, Bad;
		for (const Played& P : All)
		{
			if (P.Sightings.empty()) continue;
			double Far = 0.0;
			std::set<long> Ranges;
			for (const double R : P.Sightings) Ranges.insert(static_cast<long>(std::ceil(R)));
			for (const long R : Ranges) Far = std::max(Far, static_cast<double>(R));
			auto LeastOf = [&](const Fog& F)
			{
				double Least = 1.0;
				for (const long R : Ranges)
				{
					for (const double Eye : Eyes) Least = std::min(Least, FogSeen(F, Eye * Unit, 0.0, static_cast<double>(R) * Unit));
				}
				return Least;
			};
			// (at the recording's own day, as LookOf makes it in field mode, by day or by night; and at every sun from the
			// twilight's -6 degrees, where the vis begins to be a fog, to 70, under the recording's own vis and moon)
			const Look Own = LookOf(FieldOf(P), StageSun{}, FieldAsk());
			const double AtOwn = LeastOf(FogOf(Own.Air));
			double Least = AtOwn;
			std::string At = "its own sun " + Num(Own.SunUp, 1);
			for (double Up = VisFogLowest + 0.25; Up <= 70.0; Up += 0.25)
			{
				Field F = FieldOf(P);
				F.bLight = true;
				F.SunUp = Up;
				F.bSunFrom = true;
				F.SunFrom = 200.0 + Up;
				const Look L = LookOf(F, StageSun{}, FieldAsk());
				const double Got = LeastOf(FogOf(L.Air));
				++Suns;
				if (Got < Least)
				{
					Least = Got;
					At = "sun " + Num(Up, 2);
				}
			}
			Seen += static_cast<long>(P.Sightings.size());
			Worst = std::min(Worst, Least / VisContrast);
			OwnWorst = std::min(OwnWorst, AtOwn / VisContrast);
			if (Least < VisContrast * (1.0 - 1e-9) || Far > P.Sees + 1.0) Bad += P.Name + " ";
			Each += P.Name + " " + std::to_string(P.Sightings.size()) + " (to " + Num(Far, 0) + " m; at its own sun " + Num(Own.SunUp, 1) + " least " + Num(AtOwn, 3) + ", least of all " + Num(Least, 3) +
				" at " + At + "); ";
		}
		Check("sightings: every sighting Show::Hunt's lookout makes in every recording and stand-in keeps Cmin through the fog from every planned eye (the engine's line integral), at the "
			"recording's own sun and at every sun from -6 degrees up (the vis fog, coming in and wholly in)",
			Seen > 0 && Suns > 1000 && Bad.empty(), std::to_string(Seen) + " sightings over " + std::to_string(Suns) + " suns, worst " + Num(Worst, 3) + " Cmin (at their own suns " + Num(OwnWorst, 3) +
				"): " + Each + (Bad.empty() ? "" : "FAILED " + Bad));
		// At night, each stand-in at its own light.
		std::string Nights;
		int NightsSeen = 0;
		bool bAll = true;
		for (const Played& P : All)
		{
			if (!P.Rec.Board.bLight || P.Rec.Board.SunUp > VisFogLowest) continue;
			const Look L = LookOf(FieldOf(P), StageSun{}, FieldAsk());
			++NightsSeen;
			const bool bOk = L.Night.bHolds && L.Night.ContrastSky >= VisContrast && L.Night.ContrastSea >= VisContrast && L.Night.LevelsSky >= LeastLevels &&
				L.Night.LevelsSea >= LeastLevels && L.Air.Kind == HazeKind::Night && L.Bias == L.Night.Bias && L.Bias <= MostBias;
			bAll = bAll && bOk;
			Nights += P.Name + (bOk ? "" : " FAILED") + ": " + (L.Key.Kind == BodyKind::Moon ? "moon" : "glow") + ", +" + Num(L.Bias, 1) + " stops, against the sky " + Num(L.Night.ContrastSky, 3) +
				" and the sea " + Num(L.Night.ContrastSea, 3) + " (" + Num(L.Night.LevelsSky, 1) + "/" + Num(L.Night.LevelsSea, 1) + " levels) at " + Num(L.Night.Vis, 0) + " m; ";
		}
		Check("sightings: at night the rules' vis is met by light, not fog: a hull's silhouette at vis keeps Cmin against the sky and the sea, and 2 screen levels, at each stand-in's own light",
			NightsSeen > 0 && bAll, Nights);
		// And over the nights the rules may give.
		int Tried = 0, Held = 0;
		std::string First;
		double MostBiasSeen = 0.0;
		for (const double Vis : { 0.0, 1000.0, 2500.0, 4000.0, 8000.0 })
		{
			for (const double MoonUp : { -10.0, 10.0, 35.0, 70.0 })
			{
				for (const double MoonLit : { 0.0, 0.3, 1.0 })
				{
					for (int Sky = 0; Sky < 4; ++Sky)
					{
						Field F = Lit(-20.0, 250.0, MoonUp, MoonLit, Vis, 50.0, 0);
						F.Wind = Sky == 3 ? 22.0 : 8.0;
						if (Sky > 0)
						{
							F.Sky.bGiven = true;
							F.Sky.Kind = Sky == 1 ? "cumulus" : "squalls";
							F.Sky.Cover = Sky == 1 ? 0.5 : 1.0;
							F.Sky.Rain = Sky == 2 ? 0.8 : Sky == 3 ? 0.3 : 0.0;
						}
						const Look L = LookOf(F, StageSun{}, FieldAsk());
						++Tried;
						MostBiasSeen = std::max(MostBiasSeen, L.Night.Bias);
						if (L.Night.bHolds) ++Held;
						else if (First.empty()) First = "vis " + Num(Vis, 0) + " moon " + Num(MoonUp, 0) + "/" + Num(MoonLit, 1) + " sky " + std::to_string(Sky) + ": " +
							Num(L.Night.ContrastSky, 3) + "/" + Num(L.Night.ContrastSea, 3) + ", " + Num(L.Night.LevelsSky, 1) + "/" + Num(L.Night.LevelsSea, 1) + " levels at +" + Num(L.Night.Bias, 1);
					}
				}
			}
		}
		Check("sightings: NightSight holds over every night the rules may give (no moon to a full one high, clear to squalls and rain, a gale's spray)", Held == Tried,
			std::to_string(Held) + " of " + std::to_string(Tried) + ", the exposure at most +" + Num(MostBiasSeen, 1) + (First.empty() ? "" : "; first " + First));
		// The silhouette again, judged from the look alone and not from NightSight's own sums: the fog the engine is given
		// (FogOf(Look.Air)) from every planned eye, the horizon the look shows, her lit side by the look's own lamps (and a
		// dark hull), on the screen at the look's own exposure; from the twilight's -6 degrees down. And the night's air:
		// the sky's and the rain's own, never thicker, thinned only where the rules' vis (or Hunt's 4 km) would be hidden.
		{
			int Judged = 0, Hidden = 0, Thinned = 0, Thicker = 0, Unequal = 0;
			double LeastSky = 1e9, LeastSea = 1e9, LeastLevels = 1e9, MostThinned = 1.0;
			std::string FirstBad, MostAt;
			for (const double Up : { VisFogLowest, -8.0, -10.0, -14.0, -20.0 })
			{
				for (const double Vis : { 0.0, 1000.0, 2500.0, 4000.0, 8000.0 })
				{
					for (const double MoonUp : { -10.0, 4.0, 10.0, 35.0, 70.0 })
					{
						for (const double MoonLit : { 0.0, 0.3, 1.0 })
						{
							for (int Sky = 0; Sky < 4; ++Sky)
							{
								Field F = Lit(Up, 250.0, MoonUp, MoonLit, Vis, 50.0, 0);
								F.Wind = Sky == 3 ? 22.0 : 8.0;
								if (Sky > 0)
								{
									F.Sky.bGiven = true;
									F.Sky.Kind = Sky == 1 ? "cumulus" : "squalls";
									F.Sky.Cover = Sky == 1 ? 0.5 : 1.0;
									F.Sky.Rain = Sky == 2 ? 0.8 : Sky == 3 ? 0.3 : 0.0;
								}
								const Look L = LookOf(F, StageSun{}, FieldAsk());
								const double Range = Vis > 0.0 ? Vis : ClearNightSees;
								++Judged;
								std::string Why;
								for (const bool bDark : { false, true })
								{
									const Silhouette S = SilhouetteOf(L, Range, bDark);
									LeastSky = std::min(LeastSky, S.Sky / VisContrast);
									LeastSea = std::min(LeastSea, S.Sea / VisContrast);
									LeastLevels = std::min({ LeastLevels, S.LevelsSky, S.LevelsSea });
									if (S.Sky < VisContrast || S.Sea < VisContrast || S.LevelsSky < SeaDay::LeastLevels || S.LevelsSea < SeaDay::LeastLevels)
										Why = std::string(bDark ? "a dark hull " : "her lit side ") + Num(S.Sky, 3) + "/" + Num(S.Sea, 3) + ", " + Num(S.LevelsSky, 1) + "/" + Num(S.LevelsSea, 1) + " levels, " + Num(S.Seen, 3) + " seen";
								}
								if (!Why.empty() && !Hidden++)
									FirstBad = "sun " + Num(Up, 0) + " vis " + Num(Vis, 0) + " moon " + Num(MoonUp, 0) + "/" + Num(MoonLit, 1) + " sky " + std::to_string(Sky) + ": " + Why;
								// (the night's air: its own, or thinned; never thicker than its own)
								Thicker += L.Air.NightBeta > L.Air.NightOwnBeta * (1.0 + 1e-12);
								if (L.Air.bNightThinned)
								{
									++Thinned;
									const double By = L.Air.NightOwnBeta / L.Air.NightBeta;
									if (By > MostThinned)
									{
										MostThinned = By;
										MostAt = "vis " + Num(Vis, 0) + " sky " + std::to_string(Sky);
									}
								}
								else Unequal += std::fabs(L.Air.NightBeta - L.Air.NightOwnBeta) > 1e-12 * L.Air.NightOwnBeta;
							}
						}
					}
				}
			}
			Check("sightings: at night a hull's silhouette at the rules' vis (Hunt's 4 km with none), judged from the look alone (the engine's fog from every planned eye, the look's horizon, "
				"lamps and exposure), keeps Cmin against the sky and the sea and 2 screen levels, her lit side and a dark hull, from -6 degrees down",
				Hidden == 0 && Judged >= 1500, std::to_string(Judged) + " nights; least " + Num(LeastSky, 2) + " Cmin against the sky, " + Num(LeastSea, 2) + " against the sea, " + Num(LeastLevels, 1) +
					" levels" + (Hidden ? "; " + std::to_string(Hidden) + " fail, first " + FirstBad : ""));
			Check("sightings: the night's air is the sky's and the rain's own (a clear 25 km else), never thicker; thinned only where it would hide what the rules see (the rules' vis bounds it, "
				"never makes it)", Thicker == 0 && Unequal == 0,
				std::to_string(Thinned) + " of " + std::to_string(Judged) + " nights thinned, by at most " + Num(MostThinned, 1) + " times (" + MostAt + ")" +
					(Thicker + Unequal ? "; " + std::to_string(Thicker) + " thicker, " + std::to_string(Unequal) + " moved unthinned" : ""));
		}
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The day's colours, and the water.

	void DayColours()
	{
		const Look Seed = LookOf(Field{}, StageSun{}, Asked{});
		auto Pixels = [](const Look& L)
		{
			std::vector<double> P;
			for (const Rgb& C : { StillOf(L.SeaSkyLight, FitRed, FitBlue), StillOf(L.SkyHigh, FitRed, FitBlue), StillOf(L.SkyToward, FitRed, FitBlue), StillOf(L.SkyAway, FitRed, FitBlue), L.SkyOnScreen,
					 StillOf(L.Air.Colour, FitRed, FitBlue) })
			{
				P.push_back(C.R * 255.0);
				P.push_back(C.G * 255.0);
				P.push_back(C.B * 255.0);
			}
			return P;
		};
		const std::vector<double> Want = Pixels(Seed);
		int Days = 0, Tinted = 0, Under = 0;
		double Worst = 0.0, LowWorst = 0.0, TintedWorst = 0.0;
		std::string WorstAt;
		for (double Lat = -60.0; Lat <= 70.0; Lat += 2.5)
		{
			for (int Month = 0; Month < 12; ++Month)
			{
				for (double Hour = 8.0; Hour <= 15.0; Hour += 0.25)
				{
					Field F;
					F.Lat = Lat;
					F.Month = Month;
					F.bHour = true;
					F.Hour = Hour;
					Look L = LookOf(F, StageSun{}, FieldAsk());
					// (the haze's colour as today's: HazeInk with vis; the air scene's ink with none, set apart)
					L.Air.Colour = Seed.Air.Colour;
					const std::vector<double> Got = Pixels(L);
					double Off = 0.0;
					for (size_t K = 0; K < Got.size(); ++K) Off = std::max(Off, std::fabs(Got[K] - Want[K]));
					// (the colours' floor: the sun at SkyRampHigh, 6 degrees, where the sky light is the day's; below it the dawn's
					// and the dusk's own colours, which today's clamped stage never showed)
					if (L.SunUp >= SkyRampHigh)
					{
						++Days;
						if (L.SunUp < SkyGoldFrom)
						{
							++Tinted;
							TintedWorst = std::max(TintedWorst, Off);
						}
						if (Off > Worst)
						{
							Worst = Off;
							WorstAt = Num(Lat, 1) + " deg, month " + std::to_string(Month) + ", " + Num(Hour, 2) + " h, the sun " + Num(L.SunUp, 2);
						}
					}
					else
					{
						++Under;
						LowWorst = std::max(LowWorst, Off);
					}
				}
			}
		}
		Check("colours: the sky's (the horizon toward and away from the sun, the sea's sky, higher up, on the screen) at 8:00 to 15:00, wherever the sun is 6 degrees up or more (the colours' floor), "
			"within 2 levels of today's, the low sun's warm tints among them",
			Worst <= LeastLevels && Days > 1000 && Tinted > 100 && TintedWorst > 0.0,
			std::to_string(Days) + " hours (" + std::to_string(Tinted) + " with the sun 6 to 7 degrees up, under the low sun's tints: worst " + Num(TintedWorst, 2) + "), worst " + Num(Worst, 2) +
				" levels at " + WorstAt);
		Note("colours: " + std::to_string(Under) + " of those hours have the sun under 6 degrees (winter mornings and the high north's winter, where today's stage held her at 12): the dawn's own "
			"colours, up to " + Num(LowWorst, 1) + " levels off today's");
	}

	void Waters()
	{
		const std::map<std::string, WaterKind> Want = { { "samar", WaterKind::Tropic }, { "two-lines", WaterKind::Temperate }, { "light-ships", WaterKind::Med },
			{ "convoy-night", WaterKind::North }, { "convoy-night-air", WaterKind::North }, { "wolf-pack-abandon", WaterKind::Med }, { "wolf-pack-abeam", WaterKind::Med },
			{ "wolf-pack-surrender", WaterKind::Med }, { "sub-air-mediterranean-1942", WaterKind::Med } };
		std::string Said, Bad;
		int Known = 0;
		for (const Played& P : All)
		{
			const Look L = LookOf(FieldOf(P), StageSun{}, Asked{});
			Said += P.Name + " " + L.Sea.Name + "; ";
			const auto It = Want.find(P.Name);
			if (It == Want.end()) continue;
			++Known;
			if (L.Sea.Kind != It->second || L.Sea.bAsked) Bad += P.Name + " ";
		}
		Check("water: tropic for samar, temperate (the North Sea) for two-lines, med for light-ships; north for the convoy, med for the wolf packs", Bad.empty() && Known >= 3, Said + (Bad.empty() ? "" : "WRONG " + Bad));
		WaterKind K = WaterKind::Tropic;
		Asked A;
		A.Water = static_cast<int>(WaterKind::North);
		const Look Asked1 = LookOf(Field{}, StageSun{}, A);
		const bool bNames = WaterKindOf("tropic", K) && K == WaterKind::Tropic && WaterKindOf("med", K) && K == WaterKind::Med && WaterKindOf("north", K) && K == WaterKind::North &&
			WaterKindOf("temperate", K) && K == WaterKind::Temperate && !WaterKindOf("arctic", K);
		double Bright = 0.0;
		for (const WaterKind W : { WaterKind::Tropic, WaterKind::Temperate, WaterKind::North, WaterKind::Med }) Bright = std::max(Bright, std::fabs(Lum(WaterNamed(W).Body) - 1.0));
		Check("water: -TheaterSeaWater= overrides the place; each paint a tint of today's, its brightness kept", bNames && Asked1.Sea.Kind == WaterKind::North && Asked1.Sea.bAsked && Bright < 1e-12,
			"the brightness within " + Sci(Bright));
		Check("water: by the winter half year's line in either hemisphere (north from 45 degrees in winter, 50 in summer)",
			WaterOf(-50.0, 0.0, 6).Kind == WaterKind::North && WaterOf(-47.0, 0.0, 0).Kind == WaterKind::Temperate && WaterOf(47.0, -30.0, 0).Kind == WaterKind::North &&
			WaterOf(47.0, -30.0, 6).Kind == WaterKind::Temperate && WaterOf(55.0, 3.0, 0).Kind == WaterKind::Temperate && WaterOf(-10.0, 150.0, 0).Kind == WaterKind::Tropic,
			"south 50 in July north, 47 in January temperate; the North Sea temperate in winter");
		// (the places: the Mediterranean's box leaves the Bay of Biscay out and takes the Black Sea's north-west and Azov in;
		// the subtropical open ocean and its warm seas the clear blue; the shelf seas north of them green-grey)
		struct Place { const char* Name; double Lat, Lon; int Month; WaterKind Kind; };
		const Place Places[] = {
			{ "the Bay of Biscay (45N 4W, a U-boat's transit)", 45.0, -4.0, 6, WaterKind::Temperate },
			{ "the Gironde's mouth (44N 2W)", 44.0, -2.0, 6, WaterKind::Temperate },
			{ "the Biscay coast in winter (43.5N 3W)", 43.5, -3.0, 0, WaterKind::Temperate },
			{ "the Alboran Sea (36N 3W)", 36.0, -3.0, 6, WaterKind::Med },
			{ "Gibraltar (36N 5.5W)", 36.0, -5.5, 0, WaterKind::Med },
			{ "off Toulon (43N 6E)", 43.0, 6.0, 0, WaterKind::Med },
			{ "the Adriatic's head (45.5N 13E)", 45.5, 13.0, 0, WaterKind::Med },
			{ "Odessa (46.5N 31E)", 46.5, 31.0, 0, WaterKind::Med },
			{ "the Sea of Azov (46.5N 36E)", 46.5, 36.0, 0, WaterKind::Med },
			{ "the Black Sea (43.5N 34E)", 43.5, 34.0, 0, WaterKind::Med },
			{ "the Red Sea (20N 38.5E)", 20.0, 38.5, 6, WaterKind::Tropic },
			{ "the Persian Gulf (27N 51E)", 27.0, 51.0, 0, WaterKind::Tropic },
			{ "the Gulf of Mexico (25N 90W)", 25.0, -90.0, 0, WaterKind::Tropic },
			{ "Okinawa (26.5N 127.5E)", 26.5, 127.5, 3, WaterKind::Tropic },
			{ "the East China Sea (30N 125E)", 30.0, 125.0, 0, WaterKind::Tropic },
			{ "the Sargasso Sea (30N 60W)", 30.0, -60.0, 0, WaterKind::Tropic },
			{ "off Cape Hatteras (35N 75W)", 35.0, -75.0, 0, WaterKind::Tropic },
			{ "the Yellow Sea (36N 123E)", 36.0, 123.0, 0, WaterKind::Temperate },
			{ "the North Sea (56N 3E)", 56.0, 3.0, 0, WaterKind::Temperate },
			{ "the Norwegian Sea in winter (66N 5E)", 66.0, 5.0, 0, WaterKind::North },
			{ "south of Iceland in winter (60N 20W)", 60.0, -20.0, 1, WaterKind::North },
		};
		std::string Wrongs;
		for (const Place& P : Places)
		{
			const Water W = WaterOf(P.Lat, P.Lon, P.Month);
			if (W.Kind != P.Kind) Wrongs += std::string(P.Name) + " " + W.Name + "; ";
		}
		Check("water: by place: the Bay of Biscay temperate (the Mediterranean west of 0 only south of 40N), Gibraltar and the Alboran Sea med, the Black Sea's north-west and Azov med; "
			"the Red Sea, the Gulf, the Gulf of Mexico, Okinawa, the East China Sea and the Sargasso the clear blue; the Yellow Sea and the North Sea green-grey",
			Wrongs.empty(), Wrongs.empty() ? std::to_string(std::size(Places)) + " places" : "WRONG " + Wrongs);
	}

	// ---------------------------------------------------------------------------------------------------------------
	// The core's source and what it says.

	void Scan(const std::string& Src)
	{
		const char* Files[] = { "Battle/SeaDayCore.h", "Battle/SeaDayCore.cpp" };
		const std::regex Banned[] = { std::regex("FMath"), std::regex("#include\\s*[\"<](CoreMinimal|Engine|UObject|Kismet|Math/|HAL/|Misc/|Components/|GameFramework/)"),
			std::regex("\\brand\\s*\\("), std::regex("\\bsrand\\b"), std::regex("<random>"), std::regex("\\bDice\\s*\\("), std::regex("\\bRoll\\s*\\("), std::regex("\\bUE_LOG\\b"),
			std::regex("\\bTEXT\\s*\\("), std::regex("\\bstatic\\s+(?!constexpr|const\\b)[A-Za-z_:<>]+\\s+[A-Za-z_]+\\s*(=|;)") };
		std::string Found, Dice;
		size_t Bytes = 0;
		const std::regex Strings("\"((?:[^\"\\\\\\n]|\\\\.)*)\"");
		for (const char* F : Files)
		{
			const std::string Text = ReadAll(Src + "/" + F);
			Bytes += Text.size();
			for (const std::regex& R : Banned)
			{
				if (std::regex_search(Text, R)) Found += std::string(F) + " ";
			}
			for (std::sregex_iterator It(Text.begin(), Text.end(), Strings), End; It != End; ++It)
			{
				if (Lower((*It)[1].str()).find("dice") != std::string::npos) Dice += (*It)[1].str() + " ";
			}
		}
		Check("scan: no FMath, engine include, rand, Dice or Roll, log or kept state in the core: the same field and switches give the same day", Found.empty() && Bytes > 20000,
			Found.empty() ? std::to_string(Bytes) + " bytes read" : Found);
		// (everything it says: every string in its source, and its line over many days)
		int Said = 0;
		for (double Up = -20.0; Up <= 70.0; Up += 5.0)
		{
			for (const double Vis : { 0.0, 2500.0 })
			{
				for (int W = -1; W <= 3; ++W)
				{
					Asked A = FieldAsk();
					A.Water = W;
					const std::string Line = Say(LookOf(Lit(Up, 250.0, 30.0, 0.5, Vis), StageSun{}, A));
					++Said;
					if (Lower(Line).find("dice") != std::string::npos) Dice += Line + " ";
				}
			}
		}
		const Look Again1 = LookOf(Lit(-12.0, 250.0, 30.0, 0.5, 2500.0), StageSun{}, FieldAsk()), Again2 = LookOf(Lit(-12.0, 250.0, 30.0, 0.5, 2500.0), StageSun{}, FieldAsk());
		Check("scan: no string the core makes says dice (its source's strings and its line, over " + std::to_string(Said) + " days)", Dice.empty(), Dice.empty() ? Say(Again1) : Dice);
		Check("scan: the same field gives the same day, bit for bit", Say(Again1) == Say(Again2) && Again1.Air.Density == Again2.Air.Density && Again1.Night.Bias == Again2.Night.Bias &&
			Again1.Stars.Seed == Again2.Stars.Seed, "twice over");
	}
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::printf("usage: sea_day_test <Source/TheaterCommand> <out dir> <recording.json ...>\n");
		return 2;
	}
	const auto T0 = std::chrono::steady_clock::now();
	const std::string Src = argv[1], Out = argv[2];
	std::vector<std::string> Recordings;
	for (int K = 3; K < argc; ++K) Recordings.push_back(argv[K]);
	struct Part { const char* Name; std::function<void()> Run; };
	const Part Parts[] = {
		{ "recordings", [&] { Play(Recordings); } },
		{ "seed", [] { SeedIdentity(); } },
		{ "pins", [&] { Pins(Src); } },
		{ "fog", [] { FogLine(); } },
		{ "vis", [] { Fits(); } },
		{ "field", [] { FieldRules(); } },
		{ "continuity", [] { Continuity(); } },
		{ "haze", [] { HazeRules(); } },
		{ "sightings", [] { Sightings(); } },
		{ "colours", [] { DayColours(); } },
		{ "water", [] { Waters(); } },
		{ "scan", [&] { Scan(Src); } },
	};
	for (const Part& P : Parts)
	{
		const auto T = std::chrono::steady_clock::now();
		std::printf("-- %s\n", P.Name);
		P.Run();
		std::printf("     (%.1f s)\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - T).count());
	}
	std::ofstream Said(Out + "/sea_day_lines.txt");
	for (const Played& P : All) Said << P.Name << ": " << Say(LookOf(FieldOf(P), StageSun{}, FieldAsk())) << "\n";
	std::printf("sea_day_test: %d checks, %d failed (%.0f s)\n", Checks, Failed, std::chrono::duration<double>(std::chrono::steady_clock::now() - T0).count());
	return Failed == 0 ? 0 : 1;
}
