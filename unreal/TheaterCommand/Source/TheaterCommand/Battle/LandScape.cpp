#include "Battle/LandScape.h"

#include <algorithm>
#include <cstring>

namespace LandScape
{
	namespace
	{
		constexpr double Pi = 3.14159265358979323846;

		struct Box
		{
			const char* Look;
			double West, East, South, North;
		};
		/** The art director's climate table (art-director/world/world-climate-boxes.json, 14 November 2026): read in order, a
		 * later box painting over an earlier one. The campaign map paints its ground from the same table. */
		const Box Boxes[] = {
			{ "tundra", -180, 180, 66, 90 }, // the far north
			{ "taiga", -180, 180, 50, 66 }, // the northern forest belt
			{ "temperate", -180, 180, 30, 50 }, // the middle latitudes
			{ "savanna", -180, 180, 8, 30 }, // the northern tropics, where no other box says otherwise
			{ "jungle", -180, 180, -8, 8 }, // the equator
			{ "savanna", -180, 180, -25, -8 }, // the southern tropics
			{ "drysouth", -180, 180, -35, -25 }, // the southern dry belt
			{ "temperate", -180, 180, -47, -35 }, // the southern middle latitudes
			{ "steppe", -180, 180, -60, -47 }, // the far south
			{ "temperate", -11, 60, 50, 57.5 }, // Britain to central Russia
			{ "temperate", 4, 20, 55, 60 }, // southern Scandinavia
			{ "tundra", 14, 42, 67.5, 72 }, // Lapland and Kola
			{ "drysouth", -10, 45, 34, 44 }, // the Mediterranean
			{ "prairie", 28, 42, 45, 51 }, // the Ukraine
			{ "steppe", 42, 60, 44, 51 }, // the lower Volga
			{ "desert", -18, 40, 16, 33 }, // the Sahara
			{ "drysouth", -10, 12, 33.5, 37.5 }, // the Maghreb coast
			{ "desert", 34, 60, 12, 33 }, // Arabia and the Syrian desert
			{ "steppe", 44, 62, 26, 38 }, // the Iranian plateau
			{ "desert", 52, 61, 28, 35 }, // the Iranian salt deserts
			{ "jungle", -14, 10, 4, 8.5 }, // the Guinea coast
			{ "jungle", 8, 30, -6, 5.5 }, // the Congo basin
			{ "savanna", 30, 42, -12, 6 }, // East Africa
			{ "steppe", 40, 52, -1, 12 }, // the Horn of Africa
			{ "desert", 11, 20, -30, -16 }, // the Namib
			{ "steppe", 19, 27, -28, -19 }, // the Kalahari
			{ "prairie", 24, 31, -31, -25 }, // the veld
			{ "drysouth", 17, 26, -35, -31 }, // the Cape
			{ "monsoon", 29, 33, -31, -26 }, // the Natal coast
			{ "jungle", 47.5, 51, -25, -12 }, // eastern Madagascar
			{ "steppe", 47, 85, 46, 53 }, // the Kazakh steppe
			{ "desert", 52, 68, 37, 45 }, // the Turan deserts
			{ "steppe", 60, 70, 29, 37 }, // Afghanistan
			{ "desert", 58, 67, 24, 30 }, // Baluchistan
			{ "dryplain", 72, 88, 8, 30 }, // the plains of India
			{ "dryplain", 67, 76, 24, 33 }, // the Indus
			{ "desert", 68, 74, 24, 29.5 }, // the Thar
			{ "jungle", 73, 77.5, 8, 20 }, // the Malabar coast
			{ "jungle", 79, 82, 5.5, 10 }, // Ceylon
			{ "monsoon", 85, 95, 20, 28 }, // Bengal and Assam
			{ "alpine", 76, 102, 28.5, 37 }, // Tibet
			{ "desert", 76, 92, 36.5, 42 }, // the Taklamakan
			{ "steppe", 80, 92, 43, 48 }, // Dzungaria
			{ "steppe", 88, 120, 44, 50.5 }, // Mongolia
			{ "desert", 92, 112, 39, 45 }, // the Gobi
			{ "dryplain", 108, 122, 32, 41 }, // the North China plain
			{ "prairie", 120, 131, 41, 49 }, // Manchuria
			{ "temperate", 124.5, 131, 38, 42 }, // northern Korea and the hills east of Mukden (added 14 Nov...
			{ "temperate", 126.5, 131, 42, 44.5 }, // the Changbai hills (added 14 November; after Manchuria an...
			{ "taiga", 128, 142, 44, 56 }, // the Amur and the coast range
			{ "monsoon", 100, 123, 21, 32 }, // central and southern China
			{ "jungle", 92, 110, 8, 24 }, // Burma, Siam and Indochina
			{ "monsoon", 98, 106, 12, 18 }, // the plain of Siam
			{ "monsoon", 94.5, 97, 16, 23 }, // the Irrawaddy
			{ "monsoon", 104.5, 108, 19.5, 22 }, // Tonkin
			{ "monsoon", 104, 107, 9, 12 }, // the Mekong delta
			{ "jungle", 95, 160, -11, 20 }, // Malaya, the Indies, the Philippines and New Guinea
			{ "monsoon", 105, 115, -9, -6 }, // Java
			{ "taiga", 140, 146, 42, 46 }, // Hokkaido
			{ "monsoon", 129.5, 142.5, 30.5, 41.5 }, // Japan south of Hokkaido (added 14 November; keep the wood...
			{ "monsoon", 123, 131.5, 24, 30.5 }, // the Ryukyu islands
			{ "monsoon", 124.5, 129.5, 34, 39 }, // southern Korea
			{ "tundra", 158, 192, 58, 72 }, // Chukotka and northern Kamchatka
			{ "steppe", 114, 150, -36, -17 }, // the dry margin of Australia
			{ "desert", 117, 141, -31, -19.5 }, // the Australian desert
			{ "savanna", 120, 146, -18.5, -11 }, // the tropical north of Australia
			{ "jungle", 144.5, 147, -19, -10.5 }, // the Queensland coast
			{ "temperate", 148.5, 154, -38, -24 }, // the east coast of Australia
			{ "prairie", 140, 148.5, -37, -32 }, // the Murray
			{ "temperate", 143, 149, -44, -37 }, // Victoria and Tasmania
			{ "drysouth", 114.5, 119, -35.5, -30.5 }, // the south-west corner
			{ "temperate", 166, 179, -47.5, -34 }, // New Zealand
			{ "jungle", 150, 230, -25, 25 }, // the islands of the Pacific
			{ "tundra", -168, -141, 67.5, 72 }, // the north slope of Alaska
			{ "tundra", -168, -158, 58, 67 }, // the Bering coast
			{ "tundra", -112, -60, 62, 72 }, // the barren grounds
			{ "tundra", -96, -60, 57, 62 }, // Hudson Bay and Ungava
			{ "ice", -56, -18, 60, 72 }, // the Greenland ice cap
			{ "tundra", -25, -13, 63, 67 }, // Iceland
			{ "taiga", -136, -122, 48, 60 }, // the rain coast
			{ "prairie", -114, -96, 49, 54 }, // the prairie provinces
			{ "prairie", -104, -95, 30, 49 }, // the Great Plains
			{ "steppe", -120, -104, 38, 49 }, // the sagebrush country
			{ "desert", -118, -106, 28, 38 }, // the south-western deserts
			{ "desert", -116, -108, 23, 32 }, // Sonora and Lower California
			{ "steppe", -106, -99, 20, 29 }, // the Mexican plateau
			{ "drysouth", -124, -118, 32, 40 }, // California
			{ "temperate", -97, -75, 25, 31 }, // the Gulf coast and Florida
			{ "drysouth", -106, -97, 16, 22 }, // the Pacific slope of Mexico
			{ "jungle", -98, -77, 7, 19 }, // Yucatan and Central America
			{ "monsoon", -85, -59, 10, 24 }, // the Caribbean islands
			{ "jungle", -79, -47, -13, 7 }, // the Amazon
			{ "jungle", -62, -50, 2, 9 }, // the Guianas
			{ "savanna", -73, -61, 4, 10 }, // the Llanos
			{ "savanna", -45, -35, -16, -3 }, // the dry north-east of Brazil
			{ "monsoon", -48, -39, -26, -14 }, // the Atlantic coast of Brazil
			{ "alpine", -72, -65, -24, -14 }, // the Altiplano
			{ "desert", -81.5, -79.5, -7, -4 }, // the Sechura coast of Peru
			{ "desert", -80.5, -78, -10, -7 }, // the coast of Peru at Trujillo
			{ "desert", -79, -76.3, -13, -10 }, // the coast of Peru at Lima
			{ "desert", -77, -74.5, -15.5, -13 }, // the coast of Peru at Ica
			{ "desert", -75.5, -71.5, -17, -15.5 }, // the coast of Peru at Arequipa
			{ "desert", -72.5, -69.9, -18.5, -17 }, // the coast at Tacna and Arica
			{ "desert", -71, -68, -27, -18.5 }, // the Atacama
			{ "prairie", -66, -56, -40, -29 }, // the Pampas
			{ "prairie", -58, -49, -35, -27 }, // Uruguay and the far south of Brazil
			{ "steppe", -73, -62, -53, -39 }, // Patagonia
			{ "taiga", -76, -71.5, -55, -37 }, // the wet coast of southern Chile
			{ "drysouth", -73, -70, -37, -30 }, // central Chile
		};

		double Lattice(int64_t I, int64_t J, uint64_t S)
		{
			return Unit(Mix(static_cast<uint64_t>(I) * 0x9E3779B97F4A7C15ull + static_cast<uint64_t>(J) * 0xC2B2AE3D27D4EB4Full + S * 0x165667B19E3779F9ull)) * 2.0 - 1.0;
		}
		double Fade(double T) { return T * T * T * (T * (T * 6.0 - 15.0) + 10.0); }

		bool Is(const char* A, const char* B) { return std::strcmp(A, B) == 0; }
	}

	double Noise(double X, double Y, uint64_t S)
	{
		const double FX = std::floor(X), FY = std::floor(Y);
		const int64_t I = static_cast<int64_t>(FX), J = static_cast<int64_t>(FY);
		const double U = Fade(X - FX), V = Fade(Y - FY);
		const double A = Lattice(I, J, S), B = Lattice(I + 1, J, S), C = Lattice(I, J + 1, S), D = Lattice(I + 1, J + 1, S);
		return Lerp(Lerp(A, B, U), Lerp(C, D, U), V);
	}

	double Fbm(double X, double Y, double Scale, int Octaves, uint64_t S)
	{
		double Sum = 0.0, Weight = 1.0, Total = 0.0, F = 1.0 / Scale;
		for (int O = 0; O < Octaves; ++O)
		{
			Sum += Weight * Noise(X * F + O * 17.31, Y * F - O * 9.13, S + static_cast<uint64_t>(O) * 101u);
			Total += Weight;
			Weight *= 0.5;
			F *= 2.03;
		}
		return Sum / Total;
	}

	const char* NameOf(Look L)
	{
		switch (L)
		{
		case Look::Bocage: return "bocage";
		case Look::Steppe: return "steppe";
		case Look::Desert: return "desert";
		case Look::Med: return "med";
		case Look::Jungle: return "jungle";
		case Look::Island: return "island";
		case Look::Farmland: return "farmland";
		case Look::Dry: return "drysteppe";
		case Look::Rice: return "rice";
		case Look::America: return "america";
		case Look::Plain: return "plain";
		default: return "";
		}
	}

	Look LookOf(const std::string& Word)
	{
		for (int K = 0; K < static_cast<int>(Look::Count); ++K)
			if (Word == NameOf(static_cast<Look>(K))) return static_cast<Look>(K);
		if (Word == "mediterranean" || Word == "drysouth" || Word == "italy") return Look::Med;
		if (Word == "pacific" || Word == "beach" || Word == "atoll") return Look::Island;
		if (Word == "dry" || Word == "mongolia" || Word == "khalkhin") return Look::Dry;
		if (Word == "paddy" || Word == "paddies" || Word == "china" || Word == "monsoon") return Look::Rice;
		if (Word == "usa" || Word == "midwest" || Word == "cornbelt" || Word == "northamerica") return Look::America;
		if (Word == "dryplain" || Word == "manchuria" || Word == "northchina" || Word == "india") return Look::Plain;
		return Look::Count;
	}

	const char* ClimateAt(double Lat, double Lon)
	{
		while (Lon > 180.0) Lon -= 360.0;
		while (Lon < -180.0) Lon += 360.0;
		const char* Out = "temperate";
		for (const Box& B : Boxes)
		{
			// (the Pacific's box runs on past the date line: its east is 230)
			const bool bLon = (Lon >= B.West && Lon <= B.East) || (B.East > 180.0 && Lon + 360.0 >= B.West && Lon + 360.0 <= B.East);
			if (bLon && Lat >= B.South && Lat <= B.North) Out = B.Look;
		}
		return Out;
	}

	Look Pick(const LandField::Field& F)
	{
		if (!F.Look.empty())
		{
			const Look L = LookOf(F.Look);
			if (L != Look::Count) return L;
		}
		if (F.Steppe.bOn) return Look::Steppe;
		const char* C = ClimateAt(F.Lat, F.Lon);
		if (F.Terrain == "desert" || Is(C, "desert")) return Look::Desert;
		// Rice country (release 2, on trial "rice" until the art director passes it and the Mac holds its frame rate):
		// central and southern China north of the Burma and Indochina boxes, Korea, Japan south of Hokkaido and the
		// Ryukyus; a beach or an islet there keeps the island's. Burma's, Siam's, Tonkin's and the Mekong's paddies stay
		// the jungle's mainland look.
		if (F.Tried("rice") && F.Terrain != "island" && F.Terrain != "beach" &&
			((Is(C, "monsoon") && F.Lat >= 22.0 && F.Lon >= 98.0) || (Is(C, "temperate") && F.Lon > 124.0 && F.Lon < 131.5 && F.Lat > 33.0 && F.Lat < 42.0)))
			return Look::Rice;
		// The dry plains (release 2, on trial "plain"): the North China plain, the Indus and the plains of India (the
		// climate's dry plain) and Manchuria's prairie; a beach or an islet keeps the island's.
		if (F.Tried("plain") && F.Terrain != "island" && F.Terrain != "beach" &&
			(Is(C, "dryplain") || (Is(C, "prairie") && F.Lon > 118.0 && F.Lon < 132.0 && F.Lat > 40.0 && F.Lat < 50.0)))
			return Look::Plain;
		// North America's farm country (release 2, on trial "america"): the temperate belt from the Pacific coast to
		// Newfoundland; a beach or an islet keeps the island's.
		if (F.Tried("america") && F.Lon > -125.0 && F.Lon < -52.0 && F.Lat > 28.0 && F.Lat < 56.0 &&
			(Is(C, "temperate") || Is(C, "prairie")) && F.Terrain != "island" && F.Terrain != "beach")
			return Look::America;
		if (Is(C, "drysouth")) return Look::Med;
		if (Is(C, "jungle") || Is(C, "monsoon") || Is(C, "savanna"))
		{
			// (a shore in the battle, or a coral islet: the island's beach and palms; inland, the jungle)
			if (F.Terrain == "coast" || F.Terrain == "island" || F.Terrain == "beach") return Look::Island;
			size_t Water = 0, All = 0;
			for (const int G : F.Ground)
			{
				++All;
				if (G >= 0 && static_cast<size_t>(G) < F.Grounds.size() && F.Grounds[static_cast<size_t>(G)] == "water") ++Water;
			}
			return All && Water * 12 > All ? Look::Island : Look::Jungle;
		}
		// The dry steppe east of the Volga (Mongolia, Khalkhin Gol, Kazakhstan, Iran, the Horn): short grass and sand, no
		// farms. The lower Don and the Volga by Stalingrad stay the east's farmland and its winter.
		if (Is(C, "steppe") && !(F.Lon > 38.0 && F.Lon < 47.0 && F.Lat > 43.0)) return Look::Dry;
		// Normandy, Brittany and Maine (the bocage proper) and the hedged fields of England keep the bocage.
		// (the bocage proper: Normandy, Maine, Brittany and the Vendée; Flanders, Picardy and Champagne east of it are open
		// farmland, and so is France in 1940 from Sedan to the Somme: the production lead, 22 November)
		if (Is(C, "temperate") && F.Lon < 0.6 && F.Lon > -5.5 && F.Lat > 46.5 && F.Lat < 49.9) return Look::Bocage;
		if (Is(C, "temperate") && F.Lon < 1.8 && F.Lon > -6.0 && F.Lat >= 49.9 && F.Lat < 55.5) return Look::Bocage; // (England's hedged fields)
		// The open country of central and eastern Europe, the steppe without a recording's lay, the northern forest.
		return Look::Farmland;
	}

	bool LeaflessOf(const LandField::Field& F, Look L)
	{
		if (L != Look::Bocage && L != Look::Steppe && L != Look::Farmland && L != Look::Rice && L != Look::America && L != Look::Plain) return false;
		if (F.Month < 1 || F.Month > 12) return false;
		const double Lat = std::fabs(F.Lat);
		if (Lat < 28.0) return false;
		const int M = F.Lat < 0.0 ? (F.Month + 5) % 12 + 1 : F.Month;
		if (M == 12 || M == 1 || M == 2) return true;
		if (M == 11 || M == 3) return Lat >= 44.0;
		return M == 4 && Lat >= 52.0;
	}

	double SnowOf(const LandField::Field& F, Look L)
	{
		if (F.Snow >= 0.0) return Clamp01(F.Snow);
		if (L == Look::Desert || L == Look::Jungle || L == Look::Island) return 0.0;
		const double Lat = F.Lat, Lon = F.Lon;
		// How hard the winter is here (0 the Atlantic's mild lowlands, 1 Russia's): north and east harder; the Ardennes'
		// and the Eifel's uplands as hard as Poland's; mountains harder.
		double W;
		if (Lat >= 58.0) W = 1.0;
		else if (Lon >= 22.0) W = 0.55 + 0.45 * Smooth(44.0, 55.0, Lat);
		else if (Lon >= 6.0) W = 0.3 + 0.4 * Smooth(45.0, 54.0, Lat);
		else W = 0.12 * Smooth(48.0, 53.0, Lat);
		if (Lat >= 49.4 && Lat <= 51.0 && Lon >= 4.6 && Lon <= 7.6) W = std::max(W, 0.78);
		if (F.Terrain == "mountain") W += 0.3;
		else if (F.Terrain == "hills") W += 0.1;
		if (L == Look::Med) W = F.Terrain == "mountain" ? 0.6 : F.Terrain == "hills" ? 0.3 : 0.0;
		if (L == Look::Dry) W *= 0.75; // (the dry steppe's winter is hard, but its snow thin and blown)
		// (rice country's winter by its latitude alone: none on the Yangtze's delta, a cold January in Korea and north Honshu)
		if (L == Look::Rice) W = Smooth(30.0, 44.0, Lat) + (F.Terrain == "mountain" ? 0.3 : F.Terrain == "hills" ? 0.1 : 0.0);
		// (the American farm belt's winter: continental, snow from Iowa and Ohio north, thinner and blown on the plains)
		if (L == Look::America)
			W = Smooth(35.0, 45.0, Lat) * (Lon < -96.5 ? 0.85 : 1.0) + (F.Terrain == "mountain" ? 0.3 : F.Terrain == "hills" ? 0.1 : 0.0);
		// (the dry plains: Manchuria's long hard winter; the North China plain's dry and mostly bare; none in India)
		if (L == Look::Plain) W = (Lon < 95.0 ? 0.0 : Smooth(37.0, 47.0, Lat)) + (F.Terrain == "mountain" ? 0.3 : F.Terrain == "hills" ? 0.1 : 0.0);
		const int M = F.Month;
		const double ByMonth = (M == 12 || M == 1 || M == 2) ? 1.0 : (M == 11 || M == 3) ? (W > 0.85 ? 0.9 : 0.5) : (M == 10 || M == 4) ? (W > 0.95 ? 0.4 : 0.0) : 0.0;
		const double Cold = W * ByMonth;
		if (Cold < 0.35) return 0.0;
		// (each battle's own: a fresh fall, an old crust thinned by thaw)
		const double Vary = 0.85 + 0.3 * Unit(Hash(F.Seed ^ 0x5A0Full, static_cast<uint64_t>(M)));
		return Clamp01((0.3 + 0.7 * Smooth(0.35, 0.9, Cold)) * Vary);
	}

	bool EastGold(Look L, double SnowLying, double Lat, double Lon, int Month, const std::string& Trial)
	{
		if (L != Look::Farmland || SnowLying > 0.3 || Month < 5 || Month > 9) return false;
		if (Lat < 44.0 || Lat > 62.0 || Lon < 23.5 || Lon > 60.0) return false;
		// (the northern forest belt keeps the farmland's own air: Leningrad, the Volkhov and the Svir, Karelia, where the
		// look picker sends the taiga to Farmland; no steppe dust over their swamps and spruce)
		if (Is(ClimateAt(Lat, Lon), "taiga")) return false;
		LandField::Field Words;
		Words.Trial = Trial;
		return Words.Tried("eastgold");
	}

	/** The day's air over a look (AirOf), the east's summer its own where bEastGold (EastGold). */
	static Air AirFor(Look L, double SnowLying, bool bEastGold)
	{
		Air A;
		auto Ink = [](double* To, double R, double G, double B) { To[0] = R; To[1] = G; To[2] = B; };
		// (the art director's starting grades, one per ground, art-director/battles/theatres-sheet-rulings-1122.md item 1:
		// darks, middles, contrast, stops, toe and the gain's red and blue; Normandy's stays as passed; a recording's own
		// sky.grade stands over the first four)
		auto Grade = [&A](double D, double M, double C, double St, double T, double R, double B)
		{
			A.Grade[0] = D;
			A.Grade[1] = M;
			A.Grade[2] = R;
			A.Grade[3] = B;
			A.Contrast = C;
			A.Stops = St;
			A.Toe = T;
		};
		// (each haze's hue and sight from the art director's passed sets, battle-models/<region>/sets/<look>.json: the hue
		// taken at about the bocage's brightness, which she passed in the game; the thickness from its sight, the bocage's
		// 0.0069 seeing some 3,500 m)
		switch (L)
		{
		case Look::Desert:
			// (the desert's noon: dust hanging pale and warm low over the plain, the far escarpment a pale band; desert.json
			// D6C9B2 at 3,400 m)
			A.bSet = true;
			Ink(A.HazeInk, 0.85, 0.74, 0.56);
			Ink(A.HazeOvercast, 0.66, 0.61, 0.52);
			A.HazeDensity = 0.0055; // (pushed out from the set's 3,400 m to some 4,400 m on a clear day, so the escarpment's face reads at two kilometres: the art director's read, 18:30Z)
			A.SkyDust = 1.15;
			Grade(0.55, 0.66, 1.25, -0.6, 0.55, 1.02, 0.96);
			A.Cover = 0.05; // (the desert's own clear, deep sky and hard shadows: the art director's read, 18:30Z)
			break;
		case Look::Med:
			// (drysouth.json: a blue-grey haze, B3BFCC at 2,400 m; the hills behind fade in steps)
			A.bSet = true;
			Ink(A.HazeInk, 0.61, 0.70, 0.81);
			Ink(A.HazeOvercast, 0.52, 0.55, 0.58);
			A.HazeDensity = 0.0051; // (a clear day's, some 4,700 m, as the desert's: the art director's first read of 22 November; a hazy or sirocco day's 2,400 m is 0.0101)
			A.SkyDust = 0.85;
			Grade(0.50, 0.58, 1.35, -0.5, 0.55, 1.02, 0.97);
			break;
		case Look::Jungle:
			// (jungle.json: a grey-green steam, 8E9A8C at 2,500 m; its wet season's 1,400 m is the weather's to thicken)
			A.bSet = true;
			Ink(A.HazeInk, 0.54, 0.64, 0.52);
			Ink(A.HazeOvercast, 0.48, 0.54, 0.48);
			A.HazeDensity = 0.0097;
			A.SkyDust = 0.5;
			Grade(0.60, 0.66, 1.15, -0.2, 0.45, 1.0, 1.0);
			break;
		case Look::Island:
			A.bSet = true;
			Ink(A.HazeInk, 0.70, 0.76, 0.80);
			Ink(A.HazeOvercast, 0.56, 0.60, 0.62);
			A.HazeDensity = 0.0062;
			A.SkyDust = 0.5;
			Grade(0.60, 0.66, 1.15, -0.2, 0.45, 1.0, 1.0); // (the Pacific islands take the jungle's)
			break;
		case Look::Farmland:
			A.bSet = true;
			if (bEastGold)
			{
				// (the east's summer on trial "eastgold" (EastGold): Normandy's grade and haze washed its ripe strips to a
				// near-blown cream, the art director's Voronezh read (236,228,195 against her gold). The summer steppe's dust
				// hanging warm over the corn: the east set's steppe.json, C2B99E at 4,200 m, its own hue (linear red over
				// green 1.11, blue over green 0.71, by the 2.2 power the desert's and the Med's inks keep) at about the
				// bocage's brightness. In the cloud painter, blue over green: the far band 0.98 to 0.83, the near woods 0.94 to
				// 0.71, the sky 1.06 to 1.01; the yellower 0.41 first tried for blue took them to 0.79, an olive 0.66 and a
				// grey 0.99 (the art director's mend, 25 November), and the plain grey-warm 0.70,0.68,0.62 hardly warmed the
				// band (0.92). A little more dust in the sky; her T2 grade as ruled (the look bible's T2 row,
				// theatres-sheet-rulings-1122.md item 1: middles 0.55, darks 0.50, contrast 1.35, half a stop down, toe 0.55,
				// red 1.01 and blue 0.99), which keeps more of the gold in the middle tones than Normandy's 0.48)
				Ink(A.HazeInk, 0.73, 0.66, 0.47);
				A.HazeDensity = 0.0058;
				A.SkyDust = 0.8;
				Grade(0.50, 0.55, 1.35, -0.5, 0.55, 1.01, 0.99);
			}
			break;
		case Look::Rice:
			// (monsoon.json's A0A898 at 3,500 m: a damp grey-green haze over the paddies, the hills behind in steps; a
			// grade between the jungle's and the bocage's, a little more contrast than the jungle's steam)
			A.bSet = true;
			Ink(A.HazeInk, 0.62, 0.66, 0.60);
			Ink(A.HazeOvercast, 0.52, 0.54, 0.52);
			A.HazeDensity = 0.0072;
			A.SkyDust = 0.55;
			Grade(0.55, 0.62, 1.25, -0.35, 0.5, 1.0, 1.0);
			break;
		case Look::America:
			// (the farm belt's summer: a clear blue-grey continental haze over the corn, the sight long; more contrast
			// than the bocage's, the gain a touch warm)
			A.bSet = true;
			Ink(A.HazeInk, 0.66, 0.70, 0.76);
			Ink(A.HazeOvercast, 0.54, 0.56, 0.58);
			A.HazeDensity = 0.0045;
			A.SkyDust = 0.7;
			Grade(0.52, 0.60, 1.3, -0.45, 0.55, 1.01, 0.98);
			break;
		case Look::Plain:
			// (dryplain.json's C4B898 at 4,000 m: the loess's dust in the air, a warm yellow haze over the plain; a grade
			// a little warm, more contrast than the rice country's)
			A.bSet = true;
			Ink(A.HazeInk, 0.77, 0.72, 0.60);
			Ink(A.HazeOvercast, 0.62, 0.60, 0.55);
			A.HazeDensity = 0.006;
			A.SkyDust = 1.0;
			Grade(0.53, 0.62, 1.28, -0.5, 0.55, 1.02, 0.97);
			break;
		case Look::Dry:
			// (the high continental air of Mongolia in summer: clear and dry, a pale blue-grey haze and a little dust, the
			// sight long, some 8,500 m (the art director's Khalkhin Gol read: a third less haze, so the Khalkha bluffs carry);
			// her summer steppe grade)
			A.bSet = true;
			Ink(A.HazeInk, 0.68, 0.70, 0.72);
			Ink(A.HazeOvercast, 0.55, 0.56, 0.57);
			A.HazeDensity = 0.0028;
			A.SkyDust = 0.8;
			Grade(0.50, 0.55, 1.35, -0.5, 0.55, 1.01, 0.99);
			break;
		default: break;
		}
		if (SnowLying > 0.3)
		{
			// (cold air: the haze grey-blue and thicker, the colour held down, the blue a little up)
			A.bSet = true;
			const double K = Smooth(0.3, 0.8, SnowLying);
			// (the east sets' winter haze, C9CDD1 at 1,800 m)
			A.HazeInk[0] = Lerp(A.HazeInk[0], 0.75, K);
			A.HazeInk[1] = Lerp(A.HazeInk[1], 0.78, K);
			A.HazeInk[2] = Lerp(A.HazeInk[2], 0.82, K);
			A.HazeOvercast[0] = Lerp(A.HazeOvercast[0], 0.62, K);
			A.HazeOvercast[1] = Lerp(A.HazeOvercast[1], 0.64, K);
			A.HazeOvercast[2] = Lerp(A.HazeOvercast[2], 0.68, K);
			A.HazeDensity = Lerp(A.HazeDensity, 0.0134, K);
			A.SkyDust = Lerp(A.SkyDust, 0.45, K);
			// (the art director's winter grade: snow lit 220-235 with blue over green 1.02-1.05, its shade blue-grey)
			A.Grade[0] = Lerp(A.Grade[0], 0.45, K);
			A.Grade[1] = Lerp(A.Grade[1], 0.50, K);
			A.Grade[2] = Lerp(A.Grade[2], 0.99, K);
			A.Grade[3] = Lerp(A.Grade[3], 1.03, K);
			A.Contrast = Lerp(A.Contrast, 1.30, K);
			A.Stops = Lerp(A.Stops, -0.7, K);
			A.Toe = Lerp(A.Toe, 0.55, K);
		}
		// (the farm belt's winter air dry and clear: the windbreaks and the elevator carry over the snow)
		if (L == Look::America) A.HazeDensity = std::min(A.HazeDensity, 0.0072);
		return A;
	}

	Air AirOf(Look L, double SnowLying) { return AirFor(L, SnowLying, false); }

	Air AirOf(Look L, double SnowLying, double Lat, double Lon, int Month, const std::string& Trial)
	{
		Air A = AirFor(L, SnowLying, EastGold(L, SnowLying, Lat, Lon, Month, Trial));
		// (the desert in full sun stopped down, as a cameraman stops down for sand: on the Mac the key and the sky's light
		// put bright sand at the top of the film's curve, where it lost its colour (217,206,184 for a paint of 177,158,116,
		// and pale vehicles with it; the art director's gate fault 1 and her ask of 25 November: half a stop first, never
		// more than one in all). The desert's fields only, on trial "desertstop"; its target on the Mac in full sun: sand
		// L 180 to 190 with red over green 1.12 or more, the darkest tenth of the frame no lower than L 20)
		if (L == Look::Desert && SnowLying <= 0.3)
		{
			LandField::Field Words;
			Words.Trial = Trial;
			if (Words.Tried("desertstop"))
			{
				A.Stops -= 0.5;
			}
		}
		// (an island's beach the same way, and milder: its fields run at -0.2 stops under a brighter sky than the
		// desert's, so sunlit sand washes pale too, but its palms and jungle must not go dark. A third of a stop, to
		// -0.5 and never past -0.7 (the art director's ask of 25 November), on trial "islandstop"; its target on the
		// Mac: sunlit sand L 185 to 200, the darkest tenth of the frame L 20 or more, under 30% of the frame dark)
		if (L == Look::Island && SnowLying <= 0.3)
		{
			LandField::Field Words;
			Words.Trial = Trial;
			if (Words.Tried("islandstop"))
			{
				A.Stops -= 0.3;
			}
		}
		return A;
	}

	// ------------------------------------------------------------------ a region's plain answers

	double Region::Rolling(double X, double Y, double MeanZ) const
	{
		return MeanZ + 14.0 * Fbm(X, Y, 2600.0, 3, Seed ^ 0xB1) + 4.0 * Fbm(X, Y, 700.0, 2, Seed ^ 0xB2);
	}

	int Region::DrawStep() const
	{
		// (the budget's steps: the jungle a step nearer, the temperate woods two, the northern forest three, olives on a
		// mountain four; the desert, the dry steppe, the hills' olives and the islands GS9's own)
		switch (Which())
		{
		case Look::Jungle: return 1;
		case Look::Rice: return 1;
		case Look::America: return 2;
		case Look::Plain: return 1;
		case Look::Farmland: return Fd.Lat > 55.0 ? 3 : 2;
		case Look::Med: return Fd.Terrain.find("mount") != std::string::npos ? 4 : 0;
		default: return 0;
		}
	}

	void Region::KitPlain(int Slot, LandField::BaseMesh& M) const
	{
		// (a plain grey box a little taller than a man: the kit is what is meant to be seen, and a missing file is said)
		(void)Slot;
		M.Near = LandField::Mesh();
		Writer W(M.Near);
		W.Box(0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, Rgb(150, 146, 136));
		M.Far = M.Near;
		M.FarFrom = 200.0f;
		M.Radius = 1.4f;
		M.Height = 2.0f;
	}

	Tone Snow::Over(const Tone& C, const Spot& S) const
	{
		(void)S;
		return Mixed(C, Rgb(226, 230, 236), Cover);
	}
	void Snow::Leafless(LandField::Base B, LandField::BaseMesh& M) const
	{
		(void)B;
		(void)M;
	}
	Tone Snow::OverFar(const Tone& C, const Spot& S, bool bWoods) const
	{
		(void)S;
		return Mixed(C, Rgb(220, 224, 232), bWoods ? 0.3 * Cover : Cover);
	}
	bool Snow::Clutter(LandField::Placed& P, const Spot& S) const
	{
		(void)P;
		(void)S;
		return Unit(Hash(Seed, static_cast<uint64_t>(std::lround(P.X * 7.0)), static_cast<uint64_t>(std::lround(P.Y * 7.0)))) > Cover;
	}
	void Snow::Base(LandField::Base B, LandField::BaseMesh& M) const
	{
		(void)B;
		(void)M;
	}
	void Snow::Built(LandField::Mesh& M) const
	{
		for (size_t K = 0; K < M.Colours.size(); ++K)
			if (M.Normals.size() > 3 * K + 2 && M.Normals[3 * K + 2] > 0.55f)
			{
				const Tone C{ double(M.Colours[K].R), double(M.Colours[K].G), double(M.Colours[K].B) };
				M.Colours[K] = Byte(Mixed(C, Rgb(228, 232, 238), Cover));
			}
	}

	bool KitWanted(const LandField::Field& F, std::string& OutFolder, std::vector<std::string>& OutNames)
	{
		OutFolder.clear();
		OutNames.clear();
		if (F.Steppe.bOn) return false;
		const std::unique_ptr<Region> R = Make(Pick(F), F, F.Seed);
		if (!R) return false;
		OutFolder = R->Folder();
		for (int K = 0; K < LandField::KitSlots; ++K)
			if (const char* Name = R->KitName(K); Name && !R->KitBuilt(K))
				if (std::find(OutNames.begin(), OutNames.end(), Name) == OutNames.end()) OutNames.push_back(Name);
		return !OutFolder.empty() && !OutNames.empty();
	}

	std::unique_ptr<Region> Make(Look L, const LandField::Field& F, uint64_t S)
	{
		switch (L)
		{
		case Look::Desert: return MakeDesert(F, S);
		case Look::Med: return MakeMed(F, S);
		case Look::Jungle: return MakeTropic(F, S, false);
		case Look::Island: return MakeTropic(F, S, true);
		case Look::Farmland: return MakeFarm(F, S);
		case Look::Dry: return MakeDry(F, S);
		case Look::Rice: return MakeAsia(F, S);
		case Look::America: return MakeAmerica(F, S);
		case Look::Plain: return MakePlain(F, S);
		default: return nullptr;
		}
	}

	std::unique_ptr<Snow> MakeSnow(double Cover, Look L, uint64_t S)
	{
		if (!(Cover > 0.02)) return nullptr;
		return MakeSnowCover(std::min(1.0, Cover), L, S);
	}

	std::unique_ptr<Snow> MakeSnow(double Cover, Look L, uint64_t S, const LandField::Field& F)
	{
		if (!(Cover > 0.02)) return nullptr;
		return MakeSnowCover(std::min(1.0, Cover), L, S, !F.Trial.empty() && F.Tried("wintergrass"));
	}

	// ------------------------------------------------------------------ the writer

	uint32_t Writer::Vert(double X, double Y, double Z, double NX, double NY, double NZ, const Tone& C)
	{
		M.Positions.insert(M.Positions.end(), { static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z) });
		M.Normals.insert(M.Normals.end(), { static_cast<float>(NX), static_cast<float>(NY), static_cast<float>(NZ) });
		M.Colours.push_back(Byte(C));
		return static_cast<uint32_t>(M.Colours.size() - 1);
	}

	void Writer::Tri(uint32_t A, uint32_t B, uint32_t C) { M.Triangles.insert(M.Triangles.end(), { A, B, C }); }

	void Writer::Box(double CX, double CY, double CZ, double HX, double HY, double HZ, double Yaw, const Tone& C, bool bUnder)
	{
		const double Ca = std::cos(Yaw), Sa = std::sin(Yaw);
		auto P = [&](double U, double V, double W, double Out[3]) {
			Out[0] = CX + U * HX * Ca - V * HY * Sa;
			Out[1] = CY + U * HX * Sa + V * HY * Ca;
			Out[2] = CZ + W * HZ;
		};
		// (each face its own four points, lit flat: x+, x-, y+, y-, top, and the underside if asked)
		const double Faces[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
		for (int F = 0; F < (bUnder ? 6 : 5); ++F)
		{
			const double* N = Faces[F];
			// two axes across the face, chosen so that (A x B) points along N
			double A[3] = { 0, 0, 0 }, B[3] = { 0, 0, 0 };
			if (N[0] != 0.0) { A[1] = N[0]; B[2] = 1.0; }
			else if (N[1] != 0.0) { A[2] = 1.0; B[0] = N[1]; }
			else { A[0] = 1.0; B[1] = N[2]; }
			uint32_t Id[4];
			const double Corners[4][2] = { { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } };
			const double WNX = N[0] * Ca - N[1] * Sa, WNY = N[0] * Sa + N[1] * Ca, WNZ = N[2];
			for (int K = 0; K < 4; ++K)
			{
				double Q[3];
				P(N[0] + A[0] * Corners[K][0] + B[0] * Corners[K][1], N[1] + A[1] * Corners[K][0] + B[1] * Corners[K][1], N[2] + A[2] * Corners[K][0] + B[2] * Corners[K][1], Q);
				Id[K] = Vert(Q[0], Q[1], Q[2], WNX, WNY, WNZ, C);
			}
			Tri(Id[0], Id[1], Id[2]);
			Tri(Id[0], Id[2], Id[3]);
		}
	}

	void Writer::Blade(double X0, double Y0, double Z0, double X1, double Y1, double Z1, double Width, const Tone& Foot, const Tone& Tip)
	{
		// across the blade: level, square to its run seen from above
		double AX = -(Y1 - Y0), AY = X1 - X0;
		const double L = std::hypot(AX, AY);
		if (L < 1e-9) { AX = 1.0; AY = 0.0; }
		else { AX /= L; AY /= L; }
		const double H = 0.5 * Width;
		// its face's normal: across x up the blade, leaning to the light
		const double UX = X1 - X0, UY = Y1 - Y0, UZ = Z1 - Z0;
		double NX = AY * UZ - 0.0 * UY, NY = 0.0 * UX - AX * UZ, NZ = AX * UY - AY * UX;
		const double NL = std::sqrt(NX * NX + NY * NY + NZ * NZ);
		if (NL > 1e-9) { NX /= NL; NY /= NL; NZ /= NL; }
		NZ = std::fabs(NZ) + 0.6; // (lit as grass is: from above, both faces)
		const double NN = std::sqrt(NX * NX + NY * NY + NZ * NZ);
		NX /= NN; NY /= NN; NZ /= NN;
		// Each face its own points, its normal leaning out of its own side: a face seen from behind is lit as its front
		// (its normal turned toward the eye), so the back face given the front's normal had it turned down, and drew
		// near black (the art director's black triangles on leaves, 22 November).
		const uint32_t A = Vert(X0 - AX * H, Y0 - AY * H, Z0, NX, NY, NZ, Foot);
		const uint32_t B = Vert(X0 + AX * H, Y0 + AY * H, Z0, NX, NY, NZ, Foot);
		const uint32_t C = Vert(X1, Y1, Z1, NX, NY, NZ, Tip);
		Tri(A, B, C);
		const uint32_t BA = Vert(X0 - AX * H, Y0 - AY * H, Z0, -NX, -NY, NZ, Foot);
		const uint32_t BB = Vert(X0 + AX * H, Y0 + AY * H, Z0, -NX, -NY, NZ, Foot);
		const uint32_t BC = Vert(X1, Y1, Z1, -NX, -NY, NZ, Tip);
		Tri(BB, BA, BC);
	}

	void Writer::Bead(double X, double Y, double Z, double Rad, double Tall, const Tone& Top, const Tone& Foot)
	{
		constexpr int Round = 6;
		const uint32_t T = Vert(X, Y, Z + 0.5 * Tall, 0.0, 0.0, 1.0, Top);
		const uint32_t B = Vert(X, Y, Z - 0.5 * Tall, 0.0, 0.0, -1.0, Foot);
		uint32_t Ring[Round];
		for (int K = 0; K < Round; ++K)
		{
			const double A = 2.0 * 3.14159265358979 * K / Round, CA = std::cos(A), SA = std::sin(A);
			Ring[K] = Vert(X + Rad * CA, Y + Rad * SA, Z, CA, SA, 0.0, Mixed(Foot, Top, 0.55));
		}
		for (int K = 0; K < Round; ++K)
		{
			Tri(T, Ring[K], Ring[(K + 1) % Round]);
			Tri(B, Ring[(K + 1) % Round], Ring[K]);
		}
	}

	void Writer::Rock(double CX, double CY, double Radius, double Height, uint64_t RockSeed, const Tone& Lit, const Tone& Under)
	{
		// a ball of 6 rings by 8 round, squashed to its height, its foot sunk a little, each point pushed in or out
		constexpr int Rings = 5, Round = 8;
		std::vector<uint32_t> Id;
		const uint32_t Top = Vert(CX, CY, Height, 0, 0, 1, Lit);
		for (int R = 1; R <= Rings; ++R)
		{
			const double Phi = 0.5 * Pi * R / Rings; // 0 at the top to pi/2 at the foot
			for (int K = 0; K < Round; ++K)
			{
				const double Th = 2.0 * Pi * K / Round;
				const double Push = 0.8 + 0.4 * Unit(Hash(RockSeed, static_cast<uint64_t>(R), static_cast<uint64_t>(K)));
				const double X = std::sin(Phi) * std::cos(Th), Y = std::sin(Phi) * std::sin(Th), Z = std::cos(Phi);
				const Tone C = Mixed(Under, Lit, Z);
				Id.push_back(Vert(CX + Radius * Push * X, CY + Radius * Push * Y, -0.15 * Height + 1.15 * Height * Z * Push, X, Y, Z, C));
			}
		}
		for (int K = 0; K < Round; ++K) Tri(Top, Id[static_cast<size_t>(K)], Id[static_cast<size_t>((K + 1) % Round)]);
		for (int R = 0; R + 1 < Rings; ++R)
			for (int K = 0; K < Round; ++K)
			{
				const uint32_t A = Id[static_cast<size_t>(R * Round + K)], B = Id[static_cast<size_t>(R * Round + (K + 1) % Round)];
				const uint32_t C = Id[static_cast<size_t>((R + 1) * Round + (K + 1) % Round)], D = Id[static_cast<size_t>((R + 1) * Round + K)];
				Tri(A, D, C);
				Tri(A, C, B);
			}
	}
}
