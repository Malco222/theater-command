#include "Campaign/CampaignGround.h"

#include "Campaign/CampaignClimate.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignTerrain.h"
#include "Async/ParallelFor.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	FLinearColor FromHex(const TCHAR* Hex)
	{
		return FLinearColor(FColor::FromHex(Hex)); // sRGB in, linear out
	}

	/** The colour of the bare ground for each kind of country, before any nation is painted over it. */
	FLinearColor GroundColor(const FString& Terrain)
	{
		if (Terrain == TEXT("forest")) return FromHex(TEXT("6C7F55"));
		if (Terrain == TEXT("hills")) return FromHex(TEXT("A39670"));
		if (Terrain == TEXT("mountain")) return FromHex(TEXT("8D8676"));
		if (Terrain == TEXT("urban")) return FromHex(TEXT("9C9488"));
		if (Terrain == TEXT("desert")) return FromHex(TEXT("D3BC86"));
		if (Terrain == TEXT("marsh")) return FromHex(TEXT("7C8F78"));
		return FromHex(TEXT("A8A777")); // plains
	}

	/**
	 * How each kind of country leans toward woods (the art director's recipe): forest country is mostly wooded,
	 * hills, mountains and marsh carry more than the plains, towns little, the desert none. It is added to the
	 * noise that decides where the woods stand, so the woods come in standing masses and not as specks.
	 */
	float WoodsFor(const FString& Terrain)
	{
		if (Terrain == TEXT("forest")) return 0.26f;
		if (Terrain == TEXT("hills")) return 0.05f;
		if (Terrain == TEXT("mountain")) return 0.10f;
		if (Terrain == TEXT("marsh")) return 0.10f;
		if (Terrain == TEXT("urban")) return -0.14f;
		if (Terrain == TEXT("desert")) return -0.6f;
		return -0.07f; // plains
	}

	const FLinearColor Unclaimed = FLinearColor(FColor::FromHex(TEXT("A49C86")));

	/** A colour as it is seen, each part 0 to 1 (not linear light): the paint is mixed as a painter mixes it. */
	struct FSeen
	{
		float R = 0.0f, G = 0.0f, B = 0.0f;
		FSeen() = default;
		FSeen(float InR, float InG, float InB) : R(InR), G(InG), B(InB) {}
		static FSeen Bytes(int32 InR, int32 InG, int32 InB) { return FSeen(InR / 255.0f, InG / 255.0f, InB / 255.0f); }
		FSeen operator*(float K) const { return FSeen(R * K, G * K, B * K); }
		float Light() const { return 0.30f * R + 0.59f * G + 0.11f * B; }
	};
	FSeen Mix(const FSeen& A, const FSeen& B, float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return FSeen(A.R + (B.R - A.R) * T, A.G + (B.G - A.G) * T, A.B + (B.B - A.B) * T);
	}

	/** The colour of open country by its height in metres: green lowland, paler and browner upland, grey high ground. */
	FSeen GroundByHeight(float Metres)
	{
		static const float Heights[] = { 0.0f, 120.0f, 350.0f, 700.0f, 1200.0f, 1800.0f };
		static const FSeen Colours[] = { FSeen::Bytes(112, 136, 82), FSeen::Bytes(126, 144, 86), FSeen::Bytes(146, 150, 94), FSeen::Bytes(156, 142, 100), FSeen::Bytes(150, 134, 114), FSeen::Bytes(166, 158, 150) };
		if (Metres <= Heights[0])
		{
			return Colours[0];
		}
		for (int32 K = 1; K < 6; ++K)
		{
			if (Metres < Heights[K])
			{
				return Mix(Colours[K - 1], Colours[K], (Metres - Heights[K - 1]) / (Heights[K] - Heights[K - 1]));
			}
		}
		return Colours[5];
	}

	uint8 ToByte(float Value)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Value * 255.0f), 0, 255));
	}

	/**
	 * The room's lamp is warm and bright, and the lens lifts what it sees: paint mixed to be seen at one strength
	 * comes out half as bright again on the table, and yellower. The paint is therefore laid on darker and a
	 * little bluer than it is meant to look, by these shares of red, green and blue.
	 * ("-TheaterTone=0.6,0.64,0.7" tries others without a rebuild.)
	 */
	/** How strongly a slope is leaned for the shading of the relief. ("-TheaterRelief=0.08" tries another.) */
	float ReliefStrength()
	{
		static float Kept = -1.0f;
		if (Kept < 0.0f)
		{
			Kept = 0.22f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterRelief="), Kept);
		}
		return Kept;
	}

	/** How thick the noise must be before a wood stands: lower, and there are more woods. ("-TheaterWoods=0.58" tries another.) */
	float WoodsFrom()
	{
		static float Kept = -1.0f;
		if (Kept < 0.0f)
		{
			Kept = 0.575f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterWoods="), Kept);
		}
		return Kept;
	}

	/** How far the finest relief is calmed on the low ground: 1 halves it, 0 leaves it. ("-TheaterLowland=0" tries without.) */
	float LowlandCalm()
	{
		static float Kept = -1.0f;
		if (Kept < 0.0f)
		{
			Kept = 1.0f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterLowland="), Kept);
		}
		return Kept;
	}

	/**
	 * How far beyond Europe's own frame a place lies: 0 inside it (11 W to 47 E, 28.6 N to 71.4 N), 1 from three
	 * degrees out. Inside, the ground keeps the recipe it has always had; beyond, it takes its look from the
	 * world's climate table, and the two are blended across those three degrees (the art director's world look).
	 */
	float BeyondEurope(float Latitude, float Longitude)
	{
		const float Out = FMath::Max(FMath::Max(-11.0f - Longitude, Longitude - 47.0f), FMath::Max(28.6f - Latitude, Latitude - 71.4f));
		return FMath::SmoothStep(0.0f, 3.0f, Out);
	}

	/** The bare ground of the world's looks, mixed by the share each has of a place. Colours as seen. */
	FSeen LookGround(const float Shares[WorldLook::Count], float Metres, const FVector2D& P, float TreeLine)
	{
		const FSeen ByHeight = GroundByHeight(Metres);
		const float High = FMath::SmoothStep(900.0f, 2400.0f, Metres) * 0.85f; // (the looks of the low ground give way to the upland's own greys)
		FSeen Sum(0.0f, 0.0f, 0.0f);
		for (int32 K = 0; K < WorldLook::Count; ++K)
		{
			const float Share = Shares[K];
			if (Share < 0.004f)
			{
				continue;
			}
			FSeen Look = ByHeight;
			bool bKeepsItsOwn = false; // (ice and the high cold steppe are what they are at any height)
			switch (K)
			{
			case WorldLook::DrySouth: Look = Mix(ByHeight, FSeen::Bytes(186, 160, 108), 0.62f * FMath::SmoothStep(0.0f, 600.0f, 1600.0f - Metres)); break;
			case WorldLook::Prairie: Look = FSeen::Bytes(172, 160, 124); break; // (laid a little darker than its swatch: the lamp lifts it to the swatch's (170, 161, 102))
			case WorldLook::Steppe: Look = FSeen::Bytes(176, 164, 116); break;
			case WorldLook::Desert:
				// (a third of it is rock desert, in hard-edged patches)
				// (their outlines are broken by a finer noise, and are soft enough that a coarse sheet's cells do not show in them)
				Look = Mix(FSeen::Bytes(214, 178, 138), FSeen::Bytes(170, 140, 106), FMath::SmoothStep(0.17f, 0.31f,
					FMath::PerlinNoise2D(P * 0.021 + FVector2D(71.0, 13.0)) + 0.4f * FMath::PerlinNoise2D(P * 0.083 + FVector2D(5.0, 37.0)) + 0.16f * FMath::PerlinNoise2D(P * 0.27 + FVector2D(43.0, 3.0))));
				break;
			case WorldLook::Savanna: Look = FSeen::Bytes(171, 146, 99); break; // (laid darker than its swatch: the lamp lifts it to about (190, 166, 102))
			case WorldLook::Jungle: Look = FSeen::Bytes(104, 126, 76); break;
			case WorldLook::Monsoon: Look = FSeen::Bytes(134, 152, 98); break;
			case WorldLook::DryPlain: Look = FSeen::Bytes(184, 166, 110); break;
			case WorldLook::Taiga:
				// (What shows between the trees is bog; where the trees stop, on the high ground and toward the north,
				// the bog stops with them and the ground is the tundra's moss.)
				Look = Mix(FSeen::Bytes(118, 128, 108), FSeen::Bytes(112, 126, 100), FMath::SmoothStep(TreeLine - 250.0f, TreeLine + 50.0f, Metres));
				break; // (what shows between the trees is bog, seen at about (140, 134, 94), not meadow)
			case WorldLook::Tundra:
			{
				// (mottled with moss and with peat)
				const float Mottle = FMath::PerlinNoise2D(P * 0.09 + FVector2D(23.0, 61.0));
				// (sage, its green the highest of the three: seen at about (149, 152, 137))
				Look = Mix(FSeen::Bytes(130, 139, 131), Mottle > 0.0f ? FSeen::Bytes(120, 134, 118) : FSeen::Bytes(140, 136, 126), FMath::Min(FMath::Abs(Mottle) * 1.2f, 0.6f));
				break;
			}
			case WorldLook::Ice: Look = FSeen::Bytes(232, 236, 238); bKeepsItsOwn = true; break;
			case WorldLook::Alpine: Look = FSeen::Bytes(160, 148, 116); bKeepsItsOwn = true; break;
			default: Look = FSeen(ByHeight.R, ByHeight.G, ByHeight.B + 0.05f); break; // temperate: as Europe's own lowland by height, a little bluer, so that open ground in the sun is not lime
			}
			if (!bKeepsItsOwn)
			{
				Look = Mix(Look, ByHeight, High);
			}
			Sum = FSeen(Sum.R + Look.R * Share, Sum.G + Look.G * Share, Sum.B + Look.B * Share);
		}
		return Sum;
	}

	/** The snow line at a point, in metres: the world's where it is reckoned, Europe's own 2,950 where it is not. */
	float LookSnowOr(const TArray<float>& LookSnow, int32 Index)
	{
		return LookSnow.IsValidIndex(Index) ? LookSnow[Index] : 2950.0f;
	}

	/** How each look leans toward woods, on the scale the kinds of country use (forest country is 0.26, the desert -0.6). */
	float LookWoods(const float Shares[WorldLook::Count], float Metres)
	{
		const float OnTheHills = FMath::SmoothStep(120.0f, 480.0f, Metres);
		const float Lean[WorldLook::Count] = {
			0.0f, // temperate: woods here and there
			-0.13f, // the dry south: thin scrub
			-0.6f, // prairie: none
			-0.6f, // steppe: none
			-0.7f, // desert: none
			-0.1f, // savanna: single trees, never masses
			0.25f, // jungle: closed canopy inside it, and an edge that thins out over some three degrees, in clumps
			-0.16f + 0.34f * OnTheHills, // rice country: on the hills only
			-0.3f, // the dry plain
			0.19f, // the northern forest: about three quarters canopy, with bogs as windows in it
			-0.6f, // tundra
			-0.9f, // ice
			-0.7f, // the high cold steppe
		};
		float Sum = 0.0f;
		for (int32 K = 0; K < WorldLook::Count; ++K)
		{
			Sum += Shares[K] * Lean[K];
		}
		// (Away from its edge the jungle is whole: no openings but the rivers, the tree line and the sand rim. The
		// clumps belong to the blend zone alone.)
		Sum += 0.5f * FMath::SmoothStep(0.72f, 0.94f, Shares[WorldLook::Jungle]);
		return Sum;
	}

	FVector3f Tone()
	{
		static FVector3f Kept(0.0f);
		if (Kept.X <= 0.0f)
		{
			Kept = FVector3f(0.74f, 0.79f, 0.83f); // (a tenth lighter than it first was: the far view by day now sits at the brightness of the art director's target)
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterTone="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 3)
			{
				Kept = FVector3f(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			}
		}
		return Kept;
	}
}

FLinearColor FCampaignGround::RockColour() { return FromHex(TEXT("857D70")); }
FLinearColor FCampaignGround::SnowColour() { return FromHex(TEXT("F2F0EA")); }
FLinearColor FCampaignGround::SandColour() { return FromHex(TEXT("D9CBA4")); }

float FCampaignGround::WashBoost(const FColor& NationColour)
{
	// A nation whose colour has little colour in it (Germany's grey, the Baltic states' stone) is washed on
	// a fifth thicker, or its ground would read as nobody's.
	const float Most = FMath::Max3(NationColour.R, NationColour.G, NationColour.B), Least = FMath::Min3(NationColour.R, NationColour.G, NationColour.B);
	const float Colourful = Most > 0.0f ? (Most - Least) / Most : 0.0f;
	return 1.0f + 0.2f * (1.0f - FMath::SmoothStep(0.2f, 0.34f, Colourful));
}

void FCampaignGround::Build(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, float VerticalScale)
{
	NX = Terrain.NX;
	NY = Terrain.NY;
	const int32 Count = NX * NY;

	// The ground colour of each kind of country, and how wooded and how dry it is, softened across region boundaries.
	TArray<float> RegionWoods, RegionDry;
	RegionWoods.SetNum(Data.Regions.Num());
	RegionDry.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		RegionWoods[Region.Id] = WoodsFor(Region.Terrain);
		RegionDry[Region.Id] = Region.Terrain == TEXT("desert") ? 1.0f : 0.0f;
	}
	GroundR.SetNumUninitialized(Count);
	GroundG.SetNumUninitialized(Count);
	GroundB.SetNumUninitialized(Count);
	TArray<float> Woods, Dry;
	Woods.SetNumUninitialized(Count);
	Dry.SetNumUninitialized(Count);
	TArray<float> Marsh;
	Marsh.SetNumUninitialized(Count);
	// Beyond Europe's frame (the whole-world map) the ground takes its look from the world's climate table: how far
	// beyond it each point lies, and what the look there says of fields, of their size, of fir and of the snow line.
	const FCampaignClimate& Climate = FCampaignClimate::Get();
	const bool bBeyond = Data.bWorld && Climate.IsLoaded();
	TArray<float> Outside, LookFields, LookBig, LookFir, LookSnow, LookTropic, LookNorth, LookFloor;
	TArray<uint8> LookSquare; // where the fields are the New World's squares on the compass
	if (bBeyond)
	{
		Outside.SetNumUninitialized(Count);
		LookTropic.SetNumUninitialized(Count);
		LookNorth.SetNumUninitialized(Count);
		LookFloor.Init(0.0f, Count);
		LookSquare.Init(0, Count);
		LookFields.SetNumUninitialized(Count);
		LookBig.SetNumUninitialized(Count);
		LookFir.SetNumUninitialized(Count);
		LookSnow.SetNumUninitialized(Count);
	}
	ParallelFor(NY, [&](int32 J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const int16 Id = Terrain.Region[Idx];
			Woods[Idx] = Id >= 0 ? RegionWoods[Id] : -0.07f;
			Dry[Idx] = Id >= 0 ? RegionDry[Id] : 0.0f;
			Marsh[Idx] = Id >= 0 && Data.Regions[Id].Terrain == TEXT("marsh") ? 1.0f : 0.0f;
			if (!bBeyond)
			{
				continue;
			}
			const FVector2D P = Terrain.PointAt(I, J);
			const float Latitude = static_cast<float>(Data.LatitudeAt(P.Y)), Longitude = static_cast<float>(Data.LongitudeAt(P.X));
			const float Out = BeyondEurope(Latitude, Longitude);
			Outside[Idx] = Out;
			LookFields[Idx] = 1.0f;
			LookBig[Idx] = 0.5f;
			LookFir[Idx] = 0.0f;
			LookTropic[Idx] = 0.0f;
			LookNorth[Idx] = 0.0f;
			// (the snow line falls toward the poles: about 5,000 metres from the equator to 25 degrees, then 100 lower for each degree)
			LookSnow[Idx] = 5000.0f - 100.0f * FMath::Max(FMath::Abs(Latitude) - 25.0f, 0.0f);
			if (Out <= 0.0f)
			{
				continue;
			}
			float Shares[WorldLook::Count];
			Climate.SharesAt(Latitude, Longitude, Shares);
			LookFields[Idx] = Shares[WorldLook::Temperate] + Shares[WorldLook::Prairie] + Shares[WorldLook::DryPlain] + Shares[WorldLook::Monsoon] + 0.3f * Shares[WorldLook::DrySouth];
			LookBig[Idx] = FMath::Clamp(0.5f * Shares[WorldLook::Temperate] + Shares[WorldLook::Prairie] + 0.3f * Shares[WorldLook::DrySouth], 0.0f, 1.0f);
			// (the northern forest is fir and pine, but its canopy is not as black as a fir wood of the Alps: part of it is laid in the lighter green)
			LookFir[Idx] = 0.6f * Shares[WorldLook::Taiga];
			// (the canopy of the tropics is a lighter, fuller green than Europe's woods: the jungle's wholly, rice country's and the savanna's partly)
			LookTropic[Idx] = FMath::Clamp(Shares[WorldLook::Jungle] + 0.7f * Shares[WorldLook::Monsoon] + 0.5f * Shares[WorldLook::Savanna], 0.0f, 1.0f) * Out;
			// (and the northern forest's is a lifted, cooler green than a fir wood of the Alps; the woods of the
			// temperate lands beyond Europe take a share of it, which keeps them from going olive under the lamp)
			LookNorth[Idx] = FMath::Clamp(Shares[WorldLook::Taiga] + 0.4f * Shares[WorldLook::Temperate], 0.0f, 1.0f) * Out;
			// (and under them the ground is a forest floor: see where the bare ground is laid into its picture)
			LookFloor[Idx] = Shares[WorldLook::Temperate] * Out;
			// (the prairie's fields, and those of temperate North America, are squares laid on the compass)
			const bool bNorthAmerica = Latitude > 24.0f && (Longitude < -52.0f || Longitude > 190.0f);
			LookSquare[Idx] = Out > 0.5f && Shares[WorldLook::Prairie] + (bNorthAmerica ? Shares[WorldLook::Temperate] : 0.0f) > 0.5f ? 1 : 0;
			if (Id < 0)
			{
				// (where the rules name the kind of country, that still leans the woods; where nobody has yet, the look does)
				const float Metres = Terrain.HasElevation() ? FMath::Max(Terrain.MetresAt(P.X, P.Y), 0.0f) : 0.0f;
				Woods[Idx] = FMath::Lerp(-0.07f, LookWoods(Shares, Metres), Out);
				Dry[Idx] = Shares[WorldLook::Desert] * Out;
			}
		}
	});
	const int32 Soften = FMath::RoundToInt32(4.0 / Terrain.Cell);
	FCampaignTerrain::Smooth(Woods, NX, NY, FMath::RoundToInt32(9.0 / Terrain.Cell), 2); // the edge of a wood does not follow a frontier
	FCampaignTerrain::Smooth(Dry, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(Marsh, NX, NY, Soften, 2);
	PlainsLightness = GroundColor(TEXT("plains")).GetLuminance();

	// The bare ground as nature made it, before any nation's stain: green lowland rising through paler upland to
	// grey high ground, ochre where the country is dry (the south, and the steppe), sand in the desert, darker
	// and bluer in the far north, and dull in the marshes. The nations' colours are a stain over this.
	ParallelFor(NY, [&](int32 J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FVector2D P = Terrain.PointAt(I, J);
			const float Metres = Terrain.HasElevation() ? FMath::Max(Terrain.MetresAt(P.X, P.Y), 0.0f) : FMath::Max(Terrain.Height[Idx], 0.0f) * 380.0f;
			// (the map is laid out forty units to the degree of latitude, its top edge a little north of the North Cape)
			// (these two as Europe's recipe has always reckoned them, to the last figure: its ground is not repainted)
			const float Latitude = 71.4f - static_cast<float>(P.Y) / 40.0f, Longitude = -11.0f + static_cast<float>(P.X) / 25.7f;
			const float Broad = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.02), Middling = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.088 + FVector2D(7.0, 3.0));
			const float Fine = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.6 + FVector2D(11.0, 5.0));
			float DryCountry = FMath::Clamp(FMath::SmoothStep(0.0f, 8.0f, 46.0f - Latitude) * 0.8f + FMath::SmoothStep(28.0f, 44.0f, Longitude) * FMath::SmoothStep(0.0f, 6.0f, 52.0f - Latitude) * 0.45f, 0.0f, 1.0f);
			if (bBeyond)
			{
				DryCountry *= 1.0f - Outside[Idx]; // (Europe's reckoning of the dry south holds in Europe)
			}
			const float Boreal = FMath::SmoothStep(56.0f, 63.0f, Latitude) * (bBeyond ? 1.0f - Outside[Idx] : 1.0f);
			FSeen C = GroundByHeight(Metres);
			C = Mix(C, FSeen::Bytes(186, 160, 108), FMath::Clamp(DryCountry * (0.55f + 0.3f * Broad), 0.0f, 0.8f) * FMath::SmoothStep(0.0f, 600.0f, 1600.0f - Metres));
			C = Mix(C, FSeen::Bytes(216, 192, 142) * (0.94f + 0.12f * Middling), FMath::Clamp(Dry[Idx] * 1.1f, 0.0f, 0.92f));
			C = Mix(C, FSeen::Bytes(84, 108, 78), Boreal * 0.55f * FMath::SmoothStep(0.0f, 400.0f, 1100.0f - Metres));
			C = Mix(C, FSeen::Bytes(96, 112, 82), Marsh[Idx] * 0.6f);
			if (bBeyond && Outside[Idx] > 0.0f)
			{
				float Shares[WorldLook::Count];
				Climate.SharesAt(Data.LatitudeAt(P.Y), Data.LongitudeAt(P.X), Shares);
				C = Mix(C, LookGround(Shares, Metres, P, FMath::Max(LookSnow[Idx] - 1200.0f, 200.0f)), Outside[Idx]);
			}
			C = C * (0.95f + 0.10f * Fine);
			GroundR[Idx] = C.R;
			GroundG[Idx] = C.G;
			GroundB[Idx] = C.B;
		}
	});

	// Points just off the shore take the region of the land beside them, so the stain runs to the water's edge.
	ShoreRegion = Terrain.Region;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		const TArray<int16> Before = ShoreRegion;
		for (int32 J = 0; J < NY; ++J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const int32 Idx = Terrain.Index(I, J);
				if (Before[Idx] >= 0)
				{
					continue;
				}
				if (I > 0 && Before[Idx - 1] >= 0) { ShoreRegion[Idx] = Before[Idx - 1]; }
				else if (I + 1 < NX && Before[Idx + 1] >= 0) { ShoreRegion[Idx] = Before[Idx + 1]; }
				else if (J > 0 && Before[Idx - NX] >= 0) { ShoreRegion[Idx] = Before[Idx - NX]; }
				else if (J + 1 < NY && Before[Idx + NX] >= 0) { ShoreRegion[Idx] = Before[Idx + NX]; }
			}
		}
	}

	// The relief is shaded into the paint as well as lit by the lamp, the way a printed relief map is: a low sun
	// in the west-north-west (x east, y south, z up), about thirty-eight degrees up. (The art director's sun
	// stood in the north-west, as on a printed map. The room's lamp stands in the west-south-west so that the
	// pieces are lit from the front, and the two were sixty degrees apart: the hills' shadows fell one way and
	// the flags' another. This is between them.) Heights are squeezed first, so
	// that low hills show and the Alps do not burn out; high ground throws its shadow across the country behind
	// it; and valley floors lie a little darker than the crests. All of it is in the picture, so it costs
	// nothing while the game runs.
	const FVector3f ToSun = FVector3f(-0.75f, -0.22f, 0.62f).GetSafeNormal();
	TArray<float> Metres, Squeezed;
	Metres.SetNumUninitialized(Count);
	Squeezed.SetNumUninitialized(Count);
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FVector2D Here = Terrain.PointAt(I, J);
			const float M = Terrain.HasElevation() ? FMath::Max(Terrain.MetresAt(Here.X, Here.Y), 0.0f) : FMath::Max(Terrain.Height[Idx], 0.0f) * 380.0f;
			Metres[Idx] = M;
			Squeezed[Idx] = FMath::Pow(M, 0.82f);
		}
	}
	TArray<float> Surround = Metres;
	FCampaignTerrain::Smooth(Surround, NX, NY, FMath::RoundToInt32(5.0 / Terrain.Cell), 2);
	// (the same heights with their finest detail smoothed away, for the low ground: see where the sun is worked out)
	TArray<float> Gentle = Squeezed;
	FCampaignTerrain::Smooth(Gentle, NX, NY, FMath::Max(1, FMath::RoundToInt32(3.0 / Terrain.Cell)), 2);
	// Shadows thrown by the high ground: from each point a march toward the sun, over about twenty-six map
	// units, to see whether anything on the way stands above the line of its light.
	TArray<float> Thrown;
	Thrown.Init(0.0f, Count);
	{
		const float Tall = 0.014f; // map units of height to the metre, for the throwing of shadows
		const FVector2f Flat(ToSun.X, ToSun.Y);
		const FVector2f Step = Flat / FMath::Max(FMath::Abs(Flat.X), FMath::Abs(Flat.Y)); // one grid point a step along the longer way
		const float Drop = ToSun.Z / Flat.Size() * Step.Size() * static_cast<float>(Terrain.Cell); // how far the light's line falls each step
		const int32 Steps = FMath::CeilToInt32(26.0 / (Step.Size() * Terrain.Cell));
		ParallelFor(NY, [&](int32 J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const int32 Idx = Terrain.Index(I, J);
				const float Here = Metres[Idx] * Tall;
				for (int32 K = 1; K <= Steps; ++K)
				{
					const int32 SI = I + FMath::RoundToInt32(Step.X * K), SJ = J + FMath::RoundToInt32(Step.Y * K);
					if (SI < 0 || SJ < 0 || SI >= NX || SJ >= NY)
					{
						break;
					}
					if (Metres[Terrain.Index(SI, SJ)] * Tall - K * Drop > Here + 0.15f)
					{
						Thrown[Idx] = 1.0f;
						break;
					}
				}
			}
		});
		FCampaignTerrain::Smooth(Thrown, NX, NY, 1, 2);
	}

	Ground.SetNumUninitialized(Count);
	Relief.SetNumUninitialized(Count);
	Masks.SetNumUninitialized(Count);
	ParallelFor(NY, [&](int32 J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const float H = Terrain.Height[Idx];
			const FVector2f Slope = Terrain.GradientAt(I, J);
			const float Steepness = FMath::Clamp(Slope.Size() * 0.9f, 0.0f, 1.0f);
			const float Shore = Terrain.ShoreDistance[Idx];
			const FVector2D P = Terrain.PointAt(I, J);

			// Steep ground is rock whatever its height; high flat ground (a plateau) keeps its nation's colour.
			// The highest crests are snow, and a narrow strip along the shore is sand.
			const float Rock = FMath::Min(0.85f, 0.72f * FMath::SmoothStep(0.16f, 0.62f, Steepness) * FMath::SmoothStep(0.7f, 2.2f, H)
				+ 0.3f * FMath::SmoothStep(3.6f, 5.4f, H));
			float Snow = 0.9f * FMath::SmoothStep(5.0f, 6.6f, H) * (1.0f - 0.5f * Steepness);
			const float Beyond = bBeyond ? Outside[Idx] : 0.0f;
			// (a jungle keeps its roof on the steepest range: no rock shows through it below about 3,300 metres)
			const float JungleRoof = bBeyond ? LookTropic[Idx] * (1.0f - FMath::SmoothStep(3000.0f, 3600.0f, Metres[Idx])) : 0.0f;
			if (Beyond > 0.0f)
			{
				// (one snow height for the whole world would whiten New Guinea and leave Alaska bare)
				Snow = FMath::Lerp(Snow, 0.9f * FMath::SmoothStep(LookSnow[Idx], LookSnow[Idx] + 800.0f, Metres[Idx]) * (1.0f - 0.5f * Steepness), Beyond);
			}
			const float Sand = 0.35f * (1.0f - FMath::SmoothStep(0.1f, 0.75f, Shore)); // (a thin strand, not a halo round every coast)

			// Woods stand in patches: nearly everywhere in forest country, a copse here and there in the open,
			// none above the tree line, on bare rock, on the beach or in the sea.
			// (Art fix 7: the woods stand in masses. Two noises decide where, a middling one about thirty-four map
			// units across and a broad one about a hundred and fifty; the kind of country, the north, dry ground
			// and height lean it one way or the other.)
			auto Cloudy = [&P](double Scale, const FVector2D& Shift)
			{
				const float Sum = FMath::PerlinNoise2D(P * Scale + Shift) + 0.5f * FMath::PerlinNoise2D(P * (Scale * 2.03) + Shift * 1.7) + 0.25f * FMath::PerlinNoise2D(P * (Scale * 4.1) + Shift * 2.9);
				return FMath::Clamp(0.5f + 0.5f * Sum, 0.0f, 1.0f);
			};
			const float Northing = 71.4f - static_cast<float>(P.Y) / 40.0f, Easting = -11.0f + static_cast<float>(P.X) / 25.7f;
			const float DryLand = FMath::Clamp(FMath::SmoothStep(0.0f, 8.0f, 46.0f - Northing) * 0.8f + FMath::SmoothStep(28.0f, 44.0f, Easting) * FMath::SmoothStep(0.0f, 6.0f, 52.0f - Northing) * 0.45f, 0.0f, 1.0f);
			// (and a third, small one breaks the outline, cuts clearings and throws out a copse here and there)
			const float Thick = Cloudy(1.0 / 34.0, FVector2D(31.0, -17.0)) * 0.62f + Cloudy(1.0 / 150.0, FVector2D(-7.0, 53.0)) * 0.38f + Woods[Idx]
				+ 0.11f * FMath::PerlinNoise2D(P * 0.13 + FVector2D(3.0, 41.0)) + 0.06f * FMath::PerlinNoise2D(P * 0.31 + FVector2D(19.0, 7.0))
				+ (FMath::SmoothStep(56.0f, 63.0f, Northing) * 0.2f - DryLand * 0.16f) * (1.0f - Beyond) + FMath::SmoothStep(250.0f, 900.0f, Metres[Idx]) * 0.08f
				// (Beyond Europe, in temperate and northern country, the plain is farmed and the hills are left to the
				// woods: the Appalachians, the Urals and Japan's ranges are wooded to the tree line.)
				+ (bBeyond ? FMath::Clamp(LookNorth[Idx] * 2.5f, 0.0f, 1.0f) * (0.26f * FMath::SmoothStep(300.0f, 750.0f, Metres[Idx]) + 0.12f * FMath::SmoothStep(0.1f, 0.3f, Steepness)) : 0.0f);
			// (the tree line stands where the snow line is, less about 1,200 metres: at 1,750 in Europe)
			// (and in the jungle at about 3,300 metres)
			const float TreeLine = FMath::Lerp(FMath::Lerp(1750.0f, FMath::Max(LookSnowOr(LookSnow, Idx) - 1200.0f, 200.0f), Beyond), 3300.0f, bBeyond ? LookTropic[Idx] : 0.0f);
			float Wooded = FMath::SmoothStep(WoodsFrom(), WoodsFrom() + 0.07f, Thick) * FMath::SmoothStep(0.0f, 300.0f, TreeLine - Metres[Idx]);
			// (beyond Europe the woods climb the steep ridges of the lower ranges too, under about 1,800 metres)
			// (wholly: a share of rock showing through a dark canopy turns a wooded ridge purple)
			const float Roof = FMath::Max(JungleRoof, Beyond * (1.0f - FMath::SmoothStep(1500.0f, 2000.0f, Metres[Idx])));
			Wooded *= (1.0f - Rock * (1.0f - Roof)) * (1.0f - Sand / 0.35f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);

			// Shaded relief, as on a printed map: how squarely the ground faces the sun (1 on the flat, up to 2.2
			// on a slope turned full to it, nothing on one turned away), less where a shadow is thrown across it,
			// and a little less on a valley floor than on a crest.
			const int32 IW = FMath::Max(I - 1, 0), IE = FMath::Min(I + 1, NX - 1), JN = FMath::Max(J - 1, 0), JS = FMath::Min(J + 1, NY - 1);
			const float Rough = 0.5f * FMath::PerlinNoise2D(P * 0.33) * FMath::Clamp(Steepness * 30.0f, 0.0f, 26.0f); // (broken ground on the steeps)
			// (Art fix 7: below about 300 metres the finest of the relief is cut by half. The plains are smooth and
			// the hills rough, not the other way about, and the fields can be seen.)
			const float Lowland = (1.0f - FMath::SmoothStep(200.0f, 400.0f, Metres[Idx])) * LowlandCalm();
			auto Fall = [&](const TArray<float>& Heights, int32 IA, int32 JA, int32 IB, int32 JB, int32 Steps) { return (Heights[Terrain.Index(IA, JA)] - Heights[Terrain.Index(IB, JB)]) / static_cast<float>(Steps * Terrain.Cell); };
			const float SlopeX = FMath::Lerp(Fall(Squeezed, IE, J, IW, J, IE - IW), Fall(Gentle, IE, J, IW, J, IE - IW), Lowland) + Rough * 0.13f * FMath::PerlinNoise2D(P * 0.7 + FVector2D(3.0, 9.0));
			const float SlopeY = FMath::Lerp(Fall(Squeezed, I, JS, I, JN, JS - JN), Fall(Gentle, I, JS, I, JN, JS - JN), Lowland) + Rough * 0.13f * FMath::PerlinNoise2D(P * 0.7 + FVector2D(17.0, 1.0));
			const FVector3f Leaning = FVector3f(-SlopeX * ReliefStrength(), -SlopeY * ReliefStrength(), 1.0f).GetSafeNormal();
			float Sun = FMath::Clamp(FVector3f::DotProduct(Leaning, ToSun) / ToSun.Z, 0.0f, 2.2f);
			Sun *= 1.0f - 0.42f * Thrown[Idx];
			Sun -= 0.3f * FMath::Clamp((Surround[Idx] - Metres[Idx]) / 260.0f, -0.6f, 0.6f);
			// A little unevenness, so that large areas do not look printed.
			Sun *= 1.0f + 0.03f * (1.0f - 0.6f * Lowland) * FMath::PerlinNoise2D(FVector2D(I * 0.61, J * 0.61));
			// (Beyond Europe a canopy is one great roof: it takes half the relief's light and shade that bare ground
			// takes, so that a holder's colour reads on it in sun and in shadow alike.)
			const float Roofed = 1.0f - 0.5f * Wooded * Beyond;
			Sun = 1.0f + (Sun - 1.0f) * Roofed;
			const float Shade = FMath::Clamp(Sun, 0.0f, 2.2f) / 2.2f * 2.0f; // (kept as half of it in the picture's blue, below)

			// Which way the ground leans, as the lamp sees it: north is +X and east is +Y in the world.
			// (on the low ground the lamp, too, sees the land leaning only half as much as it does)
			const FVector3f Normal = FVector3f(Slope.Y * VerticalScale * (1.0f - 0.5f * Lowland) * Roofed, -Slope.X * VerticalScale * (1.0f - 0.5f * Lowland) * Roofed, 1.0f).GetSafeNormal();

			const FVector3f Toned = Tone();
			// (A sixth of the ground shows through any canopy. Under the broadleaf woods of the temperate lands beyond
			// Europe it is a dark forest floor, or the pale upland showing through turns a wooded ridge grey-pink.)
			const float Floor = bBeyond ? LookFloor[Idx] * FMath::SmoothStep(0.5f, 0.85f, Wooded) : 0.0f;
			const FSeen Leafmould = FSeen::Bytes(46, 58, 42);
			FColor Bare(ToByte(FMath::Lerp(GroundR[Idx], Leafmould.R, Floor) * Toned.X), ToByte(FMath::Lerp(GroundG[Idx], Leafmould.G, Floor) * Toned.Y), ToByte(FMath::Lerp(GroundB[Idx], Leafmould.B, Floor) * Toned.Z), 255);
			Bare.A = ToByte(Wooded);
			Ground[Idx] = Bare;
			Relief[Idx] = FColor(ToByte(Normal.X * 0.5f + 0.5f), ToByte(Normal.Y * 0.5f + 0.5f), ToByte(Shade * 0.5f), 255);
			Masks[Idx] = FColor(ToByte(Rock * (1.0f - FMath::Max(JungleRoof, Roof * Wooded))), ToByte(Snow), ToByte(Sand), ToByte(Dry[Idx]));
		}
	});

	// What covers the ground (art fix 7): farmland for the quilt of fields, and the rims of the woods.
	Cover.SetNumUninitialized(Count);
	ParallelFor(NY, [&](int32 J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FVector2D P = Terrain.PointAt(I, J);
			const float Latitude = 71.4f - static_cast<float>(P.Y) / 40.0f, Longitude = -11.0f + static_cast<float>(P.X) / 25.7f;
			const float Boreal = FMath::SmoothStep(56.0f, 63.0f, Latitude);
			const float Broad = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.02);
			const float Wooded = Ground[Idx].A / 255.0f;
			const FColor& Mask = Masks[Idx];
			// Farmland: the low ground, where it is not desert, marsh, wood, rock or beach; thinning in the far north.
			const float Beyond = bBeyond ? Outside[Idx] : 0.0f;
			// (Europe's farmland stops at about 500 metres; the prairie and the plains of Asia are farmed far higher)
			float Arable = FMath::SmoothStep(0.0f, 300.0f, FMath::Lerp(520.0f, 1700.0f, Beyond) - Metres[Idx]) * (1.0f - FMath::Clamp(Dry[Idx] * 1.4f, 0.0f, 1.0f)) * (1.0f - Boreal * 0.75f * (1.0f - Beyond))
				* (0.55f + 0.45f * Broad) * (1.0f - Marsh[Idx]) * (1.0f - Wooded) * (1.0f - Mask.R / 255.0f) * (1.0f - Mask.G / 255.0f) * (1.0f - FMath::Min(Mask.B / 90.0f, 1.0f));
			if (Beyond > 0.0f)
			{
				Arable *= FMath::Lerp(1.0f, LookFields[Idx], Beyond); // (no fields on the steppe, in the jungle, on the tundra)
			}
			Arable = FMath::Clamp(Arable, 0.0f, 1.0f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);
			// Small fields and heavy hedges in the Atlantic west, great open ones on the eastern plains.
			float Big = FMath::Clamp(FMath::SmoothStep(11.0f, 27.0f, Longitude) + 0.3f * FMath::PerlinNoise2D(P * 0.011 + FVector2D(5.0, 23.0)), 0.0f, 1.0f);
			if (Beyond > 0.0f)
			{
				Big = FMath::Lerp(Big, LookBig[Idx], Beyond); // (the largest on the prairie, the smallest in rice country)
			}
			// The rim of a wood: its sunward edge catches the light, and it throws a strip of shade on the open
			// ground behind it. (The sun stands in the west-north-west: the grid point toward it is the one to the west.)
			const float Sunward = Ground[Terrain.Index(FMath::Max(I - 1, 0), J)].A / 255.0f;
			const float Lit = FMath::Clamp(Wooded - Sunward, 0.0f, 1.0f), Shaded = FMath::Clamp(Sunward - Wooded, 0.0f, 1.0f);
			// (a rim should be about 0.6 map units wide; one point of this picture is 1.5, so the light on it is
			// cut to what the narrower rim would give, or a small wood would be all rim and go pale)
			const float Rim = 0.5f + 0.5f * FMath::Clamp(0.19f * Lit - Shaded, -1.0f, 1.0f);
			// Fir and pine in the north and on the high ground.
			const float Fir = FMath::Max(FMath::Lerp(Boreal, bBeyond ? LookFir[Idx] : 0.0f, Beyond), FMath::SmoothStep(700.0f, 1300.0f, Metres[Idx]) * (1.0f - 0.7f * Beyond));
			Cover[Idx] = FColor(ToByte(Arable), ToByte(Big), ToByte(Rim), ToByte(Fir));
		}
	});

	// How the fields lie in each province: turned by an angle of the province's own, and a little larger or smaller.
	Lie.SetNumUninitialized(Count);
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int32 Id = Terrain.Region[Idx] >= 0 ? Terrain.Region[Idx] : ShoreRegion[Idx];
		// (the picture's alpha: 1 where the woods are Europe's, less where their canopy is the tropics')
		// (One number carries both: from 1 down to a half it is how far the canopy is the tropics', from a half down to
		// nought how far it is the northern forest's. The two are never found together.)
		const uint8 NotTropic = !bBeyond ? 255 : LookNorth[Idx] > LookTropic[Idx] ? ToByte(0.49f - 0.49f * LookNorth[Idx]) : ToByte(1.0f - 0.49f * LookTropic[Idx]);
		if (Id < 0)
		{
			// (Land nobody holds: its fields lie square on the compass where they are the New World's, and at a
			// slant of twenty degrees elsewhere. "No turn at all", to the last figure, is what tells the land's
			// paint to lay squares.)
			const bool bSquare = LookSquare.IsValidIndex(Idx) && LookSquare[Idx] != 0;
			Lie[Idx] = bSquare ? FColor(255, 128, 128, NotTropic) : (bBeyond ? FColor(247, 171, 128, NotTropic) : FColor(255, 129, 128, NotTropic));
			continue;
		}
		const uint32 Seed = static_cast<uint32>(Id) * 2654435761u;
		const float Angle = ((Seed >> 8) & 0xFFFF) / 65535.0f * UE_PI; // anywhere in the half circle: a grid turned further is the same grid again
		const float Larger = 0.5f + 0.5f * ((((Seed >> 3) & 0xFF) / 255.0f) * 2.0f - 1.0f) * 0.6f; // 0.2 to 0.8
		Lie[Idx] = FColor(ToByte(0.5f + 0.5f * FMath::Cos(Angle)), ToByte(0.5f + 0.5f * FMath::Sin(Angle)), ToByte(Larger), NotTropic);
		if (Lie[Idx].R == 255 && Lie[Idx].G == 128)
		{
			Lie[Idx].G = 129; // (no province of Europe lies at exactly no turn: that is the sign for the New World's squares)
		}
	}

	Tint.SetNumUninitialized(Count);
	PaintTint(Data, Terrain, 0, 0, NX - 1, NY - 1);

	// The sea, by its depth: pale teal over the shelves (the North Sea and the Baltic), darkening through the
	// slopes to the abyss; the sea floor faintly shaded by the same sun as the land; a little unevenness for the
	// swell; pale turquoise in the shallows for about three map units out; and the land's shadow on the water
	// on the side away from the sun. The alpha is how far offshore the point is, for the line of surf.
	WaterStep = FMath::Max(1.5, Terrain.Cell); // (a coarse sheet's sea is painted as coarsely as its land)
	WaterNX = FMath::CeilToInt32(Terrain.ExtentX / WaterStep) + 1;
	WaterNY = FMath::CeilToInt32(Terrain.ExtentY / WaterStep) + 1;
	Water.SetNumUninitialized(WaterNX * WaterNY);
	Surf.SetNumUninitialized(WaterNX * WaterNY);
	const FVector2D SunOnMap = FVector2D(ToSun.X, ToSun.Y).GetSafeNormal();
	// (The lens crushes the darkest of a colour's three parts, which takes the red out of deep blue water and
	// turns it harsh; so the darks of the sea's paint are lifted a little before it goes on, and it comes out
	// the colour it was meant to be. "-TheaterSea=0.1,0.95" tries a greying and another strength.)
	float SeaGrey = 0.0f, SeaStrength = 0.95f;
	float SeaFloor = 0.18f; // (the palest the sea may be, as a place on its ramp: "-TheaterSeaFloor=0.1" tries another)
	FParse::Value(FCommandLine::Get(), TEXT("TheaterSeaFloor="), SeaFloor);
	{
		FString Asked;
		TArray<FString> Parts;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSea="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
		{
			SeaGrey = FCString::Atof(*Parts[0]);
			SeaStrength = FCString::Atof(*Parts[1]);
		}
	}
	ParallelFor(WaterNY, [&](int32 J)
	{
		static const float Stops[] = { 0.0f, 0.22f, 0.5f, 0.8f, 1.0f };
		static const FSeen Blues[] = { FSeen::Bytes(86, 160, 160), FSeen::Bytes(52, 126, 144), FSeen::Bytes(30, 86, 116), FSeen::Bytes(17, 52, 82), FSeen::Bytes(11, 34, 60) };
		for (int32 I = 0; I < WaterNX; ++I)
		{
			const double X = Terrain.OriginX + FMath::Min(I * WaterStep, Terrain.ExtentX);
			const double Y = Terrain.OriginY + FMath::Min(J * WaterStep, Terrain.ExtentY);
			const float OffShore = -Terrain.ShoreAt(X, Y);
			auto Bed = [&Terrain](double AtX, double AtY) { return FMath::Min(Terrain.MetresAt(AtX, AtY), 0.0f); };
			float Deep = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(OffShore / 40.0f, 0.0f, 1.0f));
			float Floor = 1.0f;
			// (the seas beyond Europe's frame, as the art director's world look has them)
			const float SeaLatitude = static_cast<float>(Data.LatitudeAt(Y));
			const float SeaBeyond = bBeyond ? BeyondEurope(SeaLatitude, static_cast<float>(Data.LongitudeAt(X))) : 0.0f;
			if (Terrain.HasElevation())
			{
				float Depth = -Bed(X, Y);
				if (SeaBeyond > 0.0f)
				{
					// The edge of a shelf is spread over about four times its true width, so that it reads as a
					// slope and not as a line: the depth is taken over the water round the point as well.
					float Round = Depth;
					for (int32 Ray = 0; Ray < 8; ++Ray)
					{
						const double Angle = Ray * UE_DOUBLE_PI / 4.0;
						Round += -Bed(X + FMath::Cos(Angle) * 16.0, Y + FMath::Sin(Angle) * 16.0);
					}
					Depth = FMath::Lerp(Depth, Round / 9.0f, SeaBeyond);
				}
				Deep = FMath::Max(FMath::Pow(FMath::Clamp(Depth / 3200.0f, 0.0f, 1.0f), 0.55f), SeaFloor);
				// (the open ocean is a deep blue with a faint swell, not the black of Europe's one deepest basin)
				Deep = FMath::Lerp(Deep, Deep * 0.6f, SeaBeyond);
				// (beyond Europe the sea floor is known only every two units and painted every six: its slope is taken over
				// a wider span there, or the floor's shading comes out as a weave)
				const double Span = FMath::Lerp(1.5, 7.0, static_cast<double>(SeaBeyond));
				const float SlopeX = (Bed(X + Span, Y) - Bed(X - Span, Y)) / static_cast<float>(2.0 * Span), SlopeY = (Bed(X, Y + Span) - Bed(X, Y - Span)) / static_cast<float>(2.0 * Span);
				Floor = FMath::Clamp(1.0f - 0.004f * (SlopeX * static_cast<float>(SunOnMap.X) + SlopeY * static_cast<float>(SunOnMap.Y)), 0.82f, 1.2f);
			}
			FSeen C = Blues[4];
			for (int32 K = 1; K < 5; ++K)
			{
				if (Deep <= Stops[K])
				{
					C = Mix(Blues[K - 1], Blues[K], (Deep - Stops[K - 1]) / (Stops[K] - Stops[K - 1]));
					break;
				}
			}
			const float Swell = 0.955f + 0.045f * (1.0f + FMath::PerlinNoise2D(FVector2D(X, Y) * 0.11));
			// (turquoise shallows belong to the tropics: between about 24 north and 24 south, gone by 32. Elsewhere the
			// shelves take the Channel's tone, and north of 55 the whole sea greys.)
			const float Tropic = 1.0f - FMath::SmoothStep(24.0f, 32.0f, FMath::Abs(SeaLatitude));
			if (SeaBeyond > 0.0f)
			{
				C = Mix(C, FSeen::Bytes(63, 120, 128), 0.8f * (1.0f - Tropic) * (1.0f - FMath::SmoothStep(0.18f, 0.42f, Deep)) * SeaBeyond);
				C = Mix(C, FSeen::Bytes(62, 88, 100), 0.5f * FMath::SmoothStep(55.0f, 66.0f, SeaLatitude) * SeaBeyond);
				// (and the shelves of the far north are a steel blue-grey, not the south's turquoise: the Bering Sea)
				C = Mix(C, FSeen::Bytes(70, 102, 116), 0.85f * FMath::SmoothStep(51.0f, 57.0f, SeaLatitude) * (1.0f - FMath::SmoothStep(0.18f, 0.5f, Deep)) * SeaBeyond);
				// (the lens takes the red out of deep blue water: the open ocean is given some back, so it is blue and not ink)
				C = Mix(C, FSeen::Bytes(58, 70, 104), 0.5f * FMath::SmoothStep(0.3f, 0.6f, Deep) * SeaBeyond);
			}
			C = C * (Floor * Swell);
			// (a narrow band, and less of it over water that is pale already: or a shallow enclosed sea turns to milk)
			C = Mix(C, FSeen::Bytes(120, 190, 184), 0.25f * FMath::Pow(FMath::Clamp(1.0f - OffShore / 2.0f, 0.0f, 1.0f), 1.2f) * FMath::SmoothStep(0.04f, 0.2f, Deep)
				* FMath::Lerp(1.0f, 0.3f + 0.7f * Tropic, SeaBeyond));
			// The surf comes and goes along a shore, and is strongest where the open ocean runs in: how deep the
			// water is a little way out says which shore that is.
			float Ocean = 0.0f;
			if (Terrain.HasElevation() && OffShore < 2.5f)
			{
				for (int32 Ray = 0; Ray < 8; ++Ray)
				{
					const double Angle = Ray * UE_DOUBLE_PI / 4.0;
					Ocean = FMath::Max(Ocean, -Bed(X + FMath::Cos(Angle) * 22.0, Y + FMath::Sin(Angle) * 22.0));
				}
				Ocean = FMath::SmoothStep(120.0f, 900.0f, Ocean);
			}
			const float Comes = FMath::SmoothStep(-0.25f, 0.45f, FMath::PerlinNoise2D(FVector2D(X, Y) * 0.045) + 0.5f * FMath::PerlinNoise2D(FVector2D(X, Y) * 0.13 + FVector2D(9.0, 4.0)));
			Surf[J * WaterNX + I] = FColor(ToByte(FMath::Lerp(0.3f, 1.0f, Ocean) * FMath::Lerp(0.25f, 1.0f, Comes)), 0, 0, 255);
			// The land's shadow: darker where the land stands between this water and the sun.
			const float Lee = Terrain.CoverageAt(X + SunOnMap.X * 1.3, Y + SunOnMap.Y * 1.3) * (1.0f - Terrain.CoverageAt(X, Y));
			// (on a coarse sheet one point of the picture is several times the width of that shadow: it is laid on as much the fainter)
			C = C * (1.0f - 0.3f * FMath::Min(1.0f, 1.5f / static_cast<float>(WaterStep)) * FMath::Clamp(Lee * 1.6f, 0.0f, 1.0f));
			const float Grey = C.Light();
			auto Lift = [](float Part) { return Part + 0.031f * FMath::Max(0.0f, 1.0f - Part / 0.5f); };
			C = FSeen(Lift(C.R), Lift(C.G) * 0.92f, Lift(C.B) * 0.98f);
			C = Mix(C, FSeen(Grey, Grey, Grey), SeaGrey) * SeaStrength;
			if (CoarseSea && CoarseShape && CoarseSea->WaterNX > 1 && OffShore > 3.0f)
			{
				// (the coarse sheet's sea at this place, between its four nearest points)
				const double Gx = FMath::Clamp((X - CoarseShape->OriginX) / CoarseSea->WaterStep, 0.0, static_cast<double>(CoarseSea->WaterNX - 1));
				const double Gy = FMath::Clamp((Y - CoarseShape->OriginY) / CoarseSea->WaterStep, 0.0, static_cast<double>(CoarseSea->WaterNY - 1));
				const int32 X0 = FMath::Min(FMath::FloorToInt32(Gx), CoarseSea->WaterNX - 2), Y0 = FMath::Min(FMath::FloorToInt32(Gy), CoarseSea->WaterNY - 2);
				const float Fx = static_cast<float>(Gx - X0), Fy = static_cast<float>(Gy - Y0);
				auto At = [this, X0, Y0](int32 DX, int32 DY) { const FColor& Pixel = CoarseSea->Water[(Y0 + DY) * CoarseSea->WaterNX + X0 + DX]; return FSeen::Bytes(Pixel.R, Pixel.G, Pixel.B); };
				const FSeen Far = Mix(Mix(At(0, 0), At(1, 0), Fx), Mix(At(0, 1), At(1, 1), Fx), Fy);
				C = Mix(C, Far, FMath::SmoothStep(3.0f, 12.0f, OffShore));
			}
			Water[J * WaterNX + I] = FColor(ToByte(C.R), ToByte(C.G), ToByte(C.B), ToByte(FMath::Clamp(OffShore / 6.0f, 0.0f, 1.0f)));
		}
	});
	if (WaterStep > 2.5)
	{
		// A coarse sheet's sea is smoothed once it is painted, so that the sea floor's relief shows as shading and
		// never as the steps of the sheet's own cells. (The alpha, how far offshore a point is, is left as it was.)
		TArray<FColor> Before;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			Before = Water;
			ParallelFor(WaterNY, [&](int32 J)
			{
				for (int32 I = 0; I < WaterNX; ++I)
				{
					int32 R = 0, G = 0, B = 0, Weight = 0;
					for (int32 DJ = -1; DJ <= 1; ++DJ)
					{
						for (int32 DI = -1; DI <= 1; ++DI)
						{
							const FColor& Near = Before[FMath::Clamp(J + DJ, 0, WaterNY - 1) * WaterNX + FMath::Clamp(I + DI, 0, WaterNX - 1)];
							const int32 W = (DI == 0 ? 2 : 1) * (DJ == 0 ? 2 : 1);
							R += Near.R * W;
							G += Near.G * W;
							B += Near.B * W;
							Weight += W;
						}
					}
					FColor& Here = Water[J * WaterNX + I];
					Here = FColor(static_cast<uint8>(R / Weight), static_cast<uint8>(G / Weight), static_cast<uint8>(B / Weight), Here.A);
				}
			});
		}
	}
}

int32 FCampaignGround::PaintedRegion(int32 Index, const FCampaignTerrain& Terrain) const
{
	return ShoreRegion.IsValidIndex(Index) ? ShoreRegion[Index] : (Terrain.Region.IsValidIndex(Index) ? Terrain.Region[Index] : -1);
}

void FCampaignGround::PaintTint(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, int32 I0, int32 J0, int32 I1, int32 J1)
{
	// Each nation's colour and how much wash it needs, looked up once per region.
	TArray<FSeen> RegionStain;
	TArray<float> RegionBoost;
	RegionStain.SetNum(Data.Regions.Num());
	RegionBoost.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const FCampaignNation* Nation = Data.FindNation(Region.Owner);
		FColor Colour = Nation ? Nation->Color : FColor(150, 150, 150);
		if (Nation && Data.League.Contains(Region.Owner))
		{
			// A member of the League: its ground is the League's teal, with a trace of the country's own colour.
			Colour = FColor(ToByte(FMath::Lerp(Colour.R / 255.0f, 0x1F / 255.0f, 0.9f)), ToByte(FMath::Lerp(Colour.G / 255.0f, 0xA8 / 255.0f, 0.9f)), ToByte(FMath::Lerp(Colour.B / 255.0f, 0xA6 / 255.0f, 0.9f)));
		}
		RegionStain[Region.Id] = FSeen::Bytes(Colour.R, Colour.G, Colour.B);
		RegionBoost[Region.Id] = WashBoost(Colour);
	}
	// The nation's colour is a stain over the ground, not a coat of paint: the colour a point takes under it is
	// the nation's own, lighter on light ground and darker on dark, so the lie of the land shows through. How
	// much of the stain lies there is the material's business: a wash everywhere, and a band along the frontier
	// (the alpha here: 1 on the frontier, falling away smoothly to nothing eight map units inside it, with no
	// shelf). The stain itself is a fifth darker at the line, so the band is always a deeper shade of the
	// country's own colour and never a brighter one.
	const FVector3f Toned = Tone();
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FSeen Bare(GroundR[Idx], GroundG[Idx], GroundB[Idx]);
			const int16 Id = ShoreRegion.IsValidIndex(Idx) ? ShoreRegion[Idx] : Terrain.Region[Idx];
			FSeen C = Bare;
			float Band = 0.0f, Boost = 0.0f;
			if (Id >= 0)
			{
				C = RegionStain[Id] * (0.42f + 1.05f * Bare.Light());
				const double Far = Terrain.BorderDistance[Idx] * Terrain.Cell;
				Band = Terrain.Region[Idx] >= 0 ? FMath::Pow(FMath::Clamp(1.0f - static_cast<float>(Far / BandReach), 0.0f, 1.0f), 1.7f) : 0.0f;
				C = C * (1.0f - 0.2f * Band);
				Boost = RegionBoost[Id] - 1.0f;
			}
			Tint[Idx] = FColor(ToByte(C.R * Toned.X), ToByte(C.G * Toned.Y), ToByte(C.B * Toned.Z), ToByte(Band));
			if (Relief.IsValidIndex(Idx))
			{
				Relief[Idx].A = ToByte(Boost);
			}
		}
	}
}
