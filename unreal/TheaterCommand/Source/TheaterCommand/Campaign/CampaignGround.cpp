#include "Campaign/CampaignGround.h"

#include "Campaign/CampaignWorldRivers.h"

#include "Campaign/CampaignClimate.h"
#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignTerrain.h"
#include "Campaign/TheaterSpeed.h"
#include "TheaterCommand.h"
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
	/**
	 * How much of the land's small folds is kept in its light and shade, over the broad shape of the hills, which is
	 * kept whole: about a third (the art director's land and light brief). ("-TheaterFolds=1" tries them as they were.)
	 */
	float SmallFolds()
	{
		static float Kept = -1.0f;
		if (Kept < 0.0f)
		{
			Kept = 0.33f;
			FParse::Value(FCommandLine::Get(), TEXT("TheaterFolds="), Kept);
			Kept = FMath::Clamp(Kept, 0.0f, 1.0f);
		}
		return Kept;
	}

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
	float BeyondEuropeBy(float Latitude, float Longitude, float Degrees)
	{
		const float Out = FMath::Max(FMath::Max(-11.0f - Longitude, Longitude - 47.0f), FMath::Max(28.6f - Latitude, Latitude - 71.4f));
		return FMath::SmoothStep(0.0f, Degrees, Out);
	}

	float BeyondEurope(float Latitude, float Longitude)
	{
		return BeyondEuropeBy(Latitude, Longitude, 3.0f);
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
			default: Look = FSeen(ByHeight.R, ByHeight.G, ByHeight.B + 0.12f); break; // temperate: as Europe's own lowland by height, a little bluer, so that open ground in the sun is not lime
			}
			if (!bKeepsItsOwn)
			{
				Look = Mix(Look, ByHeight, High);
				// (between the tree line and the rock the ground is an alpine grey-brown, whatever lies below it)
				Look = Mix(Look, FSeen::Bytes(130, 124, 84), 0.85f * FMath::SmoothStep(TreeLine - 150.0f, TreeLine + 150.0f, Metres)); // (laid yellower than it is to be seen: the rock and the cool shade on it turned it mauve)
			}
			Sum = FSeen(Sum.R + Look.R * Share, Sum.G + Look.G * Share, Sum.B + Look.B * Share);
		}
		return Sum;
	}

	/**
	 * The tree line beyond Europe, in metres: 3,300 from the equator to 25 degrees, then 65 lower for each degree
	 * toward the pole (about 2,600 in central Japan, 700 at 65 north).
	 */
	float WorldTreeLine(float Latitude)
	{
		return FMath::Max(3300.0f - 65.0f * FMath::Max(FMath::Abs(Latitude) - 25.0f, 0.0f), 200.0f);
	}

	/**
	 * (World detail, item 3, the art director's order of 17 November 2026: mountains that read as mountains from
	 * playing height.) Beyond Europe, above the hills, the small folds of the ground come back into its light and
	 * shade and the shade is cut deeper, as the Alps' is; and the snow lies above her tree line. Five numbers: the
	 * height in metres where it begins (it is whole 900 above that), how much of the small folds comes back (0 to 1),
	 * how much deeper the shade is cut (0.5: half as deep again), how far above the tree line the snow begins, and over
	 * how many metres it comes in. ("-TheaterMountains=1400,1,0.5,0,500" tries another; without it the land is as it was.)
	 */
	struct FMountainLook
	{
		bool bOn = false;
		float From = 1400.0f, Folds = 1.0f, Deep = 0.5f, SnowOver = 0.0f, SnowBand = 500.0f;
	};
	const FMountainLook& MountainLook()
	{
		static FMountainLook Kept;
		static bool bRead = false;
		if (!bRead)
		{
			bRead = true;
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterMountains="), Asked, false))
			{
				Asked.ParseIntoArray(Parts, TEXT(","));
				Kept.bOn = !(Parts.Num() == 1 && FCString::Atof(*Parts[0]) <= 0.0f);
				float* const Numbers[] = { &Kept.From, &Kept.Folds, &Kept.Deep, &Kept.SnowOver, &Kept.SnowBand };
				for (int32 K = 0; K < Parts.Num() && K < 5; ++K)
				{
					*Numbers[K] = FCString::Atof(*Parts[K]);
				}
				Kept.SnowBand = FMath::Max(Kept.SnowBand, 50.0f);
			}
		}
		return Kept;
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

	/**
	 * How a water's dry bed is written into the picture of its ink: under a half, the share of its breadth that is
	 * bars which come and go along it; from a half up, the share that is dry the whole way along (a dry river's sand,
	 * a gully, a lake's pale shore).
	 */
	uint8 BedByte(float Share, bool bSteady)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32((bSteady ? 0.5f + 0.5f * Share : 0.49f * Share) * 255.0f), 0, 255));
	}

	/**
	 * How thick the noise must be before a clearing of red earth is cut in the jungle: away from any river, and on a
	 * river's bank. ("-TheaterClearings=1.0,0.62" tries others.)
	 */
	FVector2f ClearingFrom()
	{
		static FVector2f Kept(-1.0f, -1.0f);
		if (Kept.X < 0.0f)
		{
			Kept = FVector2f(0.76f, 0.16f);
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterClearings="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
			{
				Kept = FVector2f(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
		}
		return Kept;
	}

	/** How thickly the lakes of the northern forest lie, and the pools of the tundra, against what the rulings give: 1 is as ruled. ("-TheaterLakes=1.2,0.8" tries others.) */
	FVector2f LakesFrom()
	{
		static FVector2f Kept(-1.0f, -1.0f);
		if (Kept.X < 0.0f)
		{
			Kept = FVector2f(1.0f, 1.0f);
			FString Asked;
			TArray<FString> Parts;
			if (FParse::Value(FCommandLine::Get(), TEXT("TheaterLakes="), Asked, false) && Asked.ParseIntoArray(Parts, TEXT(",")) == 2)
			{
				Kept = FVector2f(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]));
			}
		}
		return Kept;
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

/** A blue or a violet: the paints that cancel to grey when they are mixed into warm ground (the art director's round 31). */
static bool IsCoolPaint(const FColor& Colour)
{
	const FLinearColor Hsv = FLinearColor(Colour.R / 255.0f, Colour.G / 255.0f, Colour.B / 255.0f).LinearRGBToHSV();
	return Hsv.R >= 195.0f && Hsv.R <= 285.0f && Hsv.G >= 0.2f;
}

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
	TArray<float> Outside, LookFields, LookBig, LookFir, LookSnow, LookTropic, LookNorth, LookFloor, LookRice;
	TArray<uint8> LookSquare; // where the fields are the New World's squares on the compass
	TArray<float> LookDryPlain; // the dry plain's share of the look, where the fields are squares: dry range is not farmed
	// (the waters of the world beyond Europe: see the header)
	const FCampaignWorldRivers& WorldRivers = FCampaignWorldRivers::Get();
	const bool bWaters = bBeyond && WorldRivers.IsLoaded();
	TArray<uint8> LookOf; // for the waters: the share of the prairie at each point (and room for two more)
	if (bWaters)
	{
		LookOf.Init(0, Count * 3);
	}
	if (bBeyond)
	{
		Outside.SetNumUninitialized(Count);
		LookTropic.SetNumUninitialized(Count);
		LookNorth.SetNumUninitialized(Count);
		LookFloor.Init(0.0f, Count);
		LookRice.Init(0.0f, Count);
		LookSquare.Init(0, Count);
		LookDryPlain.Init(0.0f, Count);
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
			// (A province of the wider chart keeps the look of its part of the world. Europe's recipe lays woods, marsh
			// and desert by each province's kind of country; laid on Kansas it put Europe's woods over the prairie.
			// Beyond Europe the country's look is the climate table's business, province or none.)
			const int16 Held = Terrain.Region[Idx];
			const int16 Id = Held >= Data.FirstBeyond && Data.FirstBeyond != INDEX_NONE ? FCampaignTerrain::UnclaimedLand : Held;
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
			if (MountainLook().bOn)
			{
				LookSnow[Idx] = WorldTreeLine(Latitude) + MountainLook().SnowOver; // (the art director: the snow lies above her tree line)
			}
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
			LookTropic[Idx] = FMath::Clamp(Shares[WorldLook::Jungle] + 0.9f * Shares[WorldLook::Monsoon] + 0.5f * Shares[WorldLook::Savanna], 0.0f, 1.0f) * Out;
			// (and the northern forest's is a lifted, cooler green than a fir wood of the Alps; the woods of the
			// temperate lands beyond Europe take a share of it, which keeps them from going olive under the lamp)
			LookNorth[Idx] = FMath::Clamp(Shares[WorldLook::Taiga] + 0.4f * Shares[WorldLook::Temperate], 0.0f, 1.0f) * Out
				* (1.0f - FMath::SmoothStep(0.15f, 0.4f, Shares[WorldLook::Monsoon])); // (rice country's woods are the warm canopy's green, not the temperate woods' blue-green: Japan)
			// (and under them the ground is a forest floor: see where the bare ground is laid into its picture)
			LookFloor[Idx] = Shares[WorldLook::Temperate] * Out;
			LookRice[Idx] = Shares[WorldLook::Monsoon] * Out; // (rice country: the plain is paddy, the ranges are wooded)
			if (bWaters)
			{
				LookOf[Idx * 3] = ToByte(Shares[WorldLook::Prairie]);
				LookOf[Idx * 3 + 1] = ToByte(Shares[WorldLook::Taiga]);
				LookOf[Idx * 3 + 2] = ToByte(Shares[WorldLook::Tundra]);
			}
			// (the prairie's fields, and those of temperate North America, are squares laid on the compass)
			const bool bNorthAmerica = Latitude > 24.0f && (Longitude < -52.0f || Longitude > 190.0f);
			// (The number is also how wholly the country is square-field country, from 1 at its edge to 255 well inside:
			// from high up its fields are drawn toward their own gold by that much, so that the gold comes in over a
			// distance and no ruled line shows where one manner of field ends.)
			// (The art director's world-detail order, item 2, 17 November 2026: "the strict square grid stays only on the
			// North American plains, where it is true". The square mile was surveyed over the Middle West and all the
			// West; the old colonies, the mountains and the old South were never ruled, and their fields lie crooked as
			// Europe's do. It comes in over a degree, and the land's paint brings the squares' gold in from that edge.)
			// (The art director's review of build 1608, 18 November 2026: the plan passes, but south of the Ohio the
			// crooked fields came out as soft strips six to ten dots wide and many times as long, "brushed cloth, not
			// farmland". They were made whole plots with an edge and a colour of their own, and she passed them at two
			// to one on 18 November at 08:13: the new lie is the game's. "-TheaterOldFields" lays the fields as before.)
			static const bool bOldFields = FParse::Param(FCommandLine::Get(), TEXT("TheaterOldFields"));
			// (the chart may count the New World's longitudes on past 180 east: they are brought back to west of Greenwich)
			const float WestLongitude = Longitude > 180.0f ? Longitude - 360.0f : Longitude;
			const float WestOfThePlains = 1.0f - FMath::SmoothStep(-101.0f, -99.0f, WestLongitude);
			// (The Middle West: north of the Ohio, which runs from Pittsburgh down to Cairo, and west of the Mississippi
			// north of Missouri's southern line; from Ohio's eastern line westward. Kentucky, Tennessee and the old
			// South below them were settled before the survey and were never ruled.)
			const float OhioRiver = FMath::Max(40.6f + 0.414f * (WestLongitude + 80.5f), 36.5f);
			const float MiddleWest = FMath::SmoothStep(OhioRiver - 0.5f, OhioRiver + 0.5f, Latitude) * (1.0f - FMath::SmoothStep(-81.2f, -80.0f, WestLongitude));
			const float Surveyed = bOldFields ? 1.0f : FMath::Max(WestOfThePlains, MiddleWest);
			const float SquareShare = Shares[WorldLook::Prairie] + (bNorthAmerica ? Shares[WorldLook::Temperate] * Surveyed : 0.0f);
			LookSquare[Idx] = Out > 0.5f && SquareShare > 0.5f ? static_cast<uint8>(1 + FMath::RoundToInt32(254.0f * FMath::SmoothStep(0.5f, 0.8f, SquareShare))) : 0;
			LookDryPlain[Idx] = bNorthAmerica ? Shares[WorldLook::DryPlain] + Shares[WorldLook::DrySouth] : 0.0f;
			if (Id < 0)
			{
				// (where the rules name the kind of country, that still leans the woods; where nobody has yet, the look does)
				const float Metres = Terrain.HasElevation() ? FMath::Max(Terrain.MetresAt(P.X, P.Y), 0.0f) : 0.0f;
				Woods[Idx] = FMath::Lerp(-0.07f, LookWoods(Shares, Metres), Out);
				// (The corn belt, from 96 to 82 west and 37.5 to 43.5 north: ploughed nearly from edge to edge. Its
				// woods are thinned to about a third of what the look would stand there, with soft edges; the South
				// keeps its own.)
				// (a sheet may count the Americas' longitudes eastward past 180: the far sheet did, and kept its woods)
				const float West = Longitude > 180.0f ? Longitude - 360.0f : Longitude;
				const float Belt = FMath::SmoothStep(-97.0f, -95.0f, West) * (1.0f - FMath::SmoothStep(-83.0f, -81.0f, West))
					* FMath::SmoothStep(36.5f, 38.5f, Latitude) * (1.0f - FMath::SmoothStep(42.5f, 44.5f, Latitude));
				// (the far sheet's woods answer the same lean far more sharply: there, half of it leaves a third standing)
				Woods[Idx] -= (Terrain.Cell > 3.0 ? 0.045f : 0.085f) * Belt;
				Dry[Idx] = Shares[WorldLook::Desert] * Out;
			}
		}
	}, TheaterSpeed::ModelLoop());
	const int32 Soften = FMath::RoundToInt32(4.0 / Terrain.Cell);
	FCampaignTerrain::Smooth(Woods, NX, NY, FMath::RoundToInt32(9.0 / Terrain.Cell), 2); // the edge of a wood does not follow a frontier
	FCampaignTerrain::Smooth(Dry, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(Marsh, NX, NY, Soften, 2);
	PlainsLightness = GroundColor(TEXT("plains")).GetLuminance();

	// The waters of the world beyond Europe, where they lie on this sheet: for every point near one, which way and how
	// far its own line lies (a river's course, a lake's long middle), how broad it is there and how much water it
	// carries (less than nothing for a lake).
	TArray<float> RiverAt, RiverDx, RiverDy, RiverHalf, RiverWater, Opened;
	WetReach = FMath::Max(4.0f, 2.5f * static_cast<float>(Terrain.Cell));
	Wet.Reset();
	WetInk.Reset();
	// (the waters' picture is ruled twice as fine as the ground on a fine sheet: see the header)
	WetScale = bWaters && Terrain.Cell <= 3.0 ? 2 : 1;
	WetNX = NX * WetScale;
	WetNY = NY * WetScale;
	TArray<float> FinerAt, FinerDx, FinerDy, FinerHalf; // the same as the four above, at the points of the waters' own picture
	TArray<float> HalfFloor; // the least half-breadth a water is drawn with at a point of the ground, where the country gives it one (a prairie creek, a gully of the steppe)
	if (bWaters)
	{
		HalfFloor.Init(0.0f, Count);
		if (WetScale > 1)
		{
			FinerAt.Init(WetReach, WetNX * WetNY);
			FinerDx.Init(WetReach, WetNX * WetNY);
			FinerDy.Init(WetReach, WetNX * WetNY);
			FinerHalf.Init(0.0f, WetNX * WetNY);
		}
		RiverAt.Init(WetReach, Count);
		RiverDx.Init(WetReach, Count);
		RiverDy.Init(WetReach, Count);
		RiverHalf.Init(0.0f, Count);
		RiverWater.Init(0.0f, Count);
		Opened.Init(0.0f, Count);
		WetInk.Init(FColor(0, 0, 0, 0), Count);
		const double Cell = Terrain.Cell, Reach = WetReach;
		const double BoxMinX = Terrain.OriginX - Reach, BoxMaxX = Terrain.OriginX + (NX - 1) * Cell + Reach;
		const double BoxMinY = Terrain.OriginY - Reach, BoxMaxY = Terrain.OriginY + (NY - 1) * Cell + Reach;
		// One stretch of water, from A to B, with half its breadth and its water at each end: every point near it that
		// is nearer its bank than any other water's takes it.
		bool bPhantomLay = false; // what is being laid is the place of a ruled river's ribbon, not painted water
		auto Lay = [&](const FVector2D& A, const FVector2D& B, float HalfA, float HalfB, float WaterA, float WaterB)
		{
			const double Lx = FMath::Min(A.X, B.X) - Reach, Hx = FMath::Max(A.X, B.X) + Reach, Ly = FMath::Min(A.Y, B.Y) - Reach, Hy = FMath::Max(A.Y, B.Y) + Reach;
			if (Hx < Terrain.OriginX || Lx > BoxMaxX - Reach || Hy < Terrain.OriginY || Ly > BoxMaxY - Reach)
			{
				return;
			}
			const FVector2D Along = B - A;
			const double Long2 = FMath::Max(Along.SizeSquared(), 1.0e-6);
			const int32 I0 = FMath::Clamp(FMath::FloorToInt32((Lx - Terrain.OriginX) / Cell), 0, NX - 1), I1 = FMath::Clamp(FMath::CeilToInt32((Hx - Terrain.OriginX) / Cell), 0, NX - 1);
			const int32 J0 = FMath::Clamp(FMath::FloorToInt32((Ly - Terrain.OriginY) / Cell), 0, NY - 1), J1 = FMath::Clamp(FMath::CeilToInt32((Hy - Terrain.OriginY) / Cell), 0, NY - 1);
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					const FVector2D P = Terrain.PointAt(I, J);
					const double T = FMath::Clamp(((P - A) | Along) / Long2, 0.0, 1.0);
					const FVector2D To = A + Along * T - P;
					const double Off = To.Size();
					const float Half = FMath::Lerp(HalfA, HalfB, static_cast<float>(T));
					const int32 Idx = Terrain.Index(I, J);
					if (Off < Reach && Off - Half < RiverAt[Idx] - RiverHalf[Idx])
					{
						RiverAt[Idx] = static_cast<float>(Off);
						RiverDx[Idx] = static_cast<float>(To.X);
						RiverDy[Idx] = static_cast<float>(To.Y);
						RiverHalf[Idx] = Half;
						RiverWater[Idx] = FMath::Lerp(WaterA, WaterB, static_cast<float>(T));
					}
				}
			}
			if (WetScale > 1 && !bPhantomLay)
			{
				// (and at the points of the waters' own finer picture)
				const double FinerCell = Cell / WetScale;
				const double FirstX = Terrain.OriginX + (0.5 / WetScale - 0.5) * Cell, FirstY = Terrain.OriginY + (0.5 / WetScale - 0.5) * Cell;
				const int32 K0 = FMath::Clamp(FMath::FloorToInt32((Lx - FirstX) / FinerCell), 0, WetNX - 1), K1 = FMath::Clamp(FMath::CeilToInt32((Hx - FirstX) / FinerCell), 0, WetNX - 1);
				const int32 L0 = FMath::Clamp(FMath::FloorToInt32((Ly - FirstY) / FinerCell), 0, WetNY - 1), L1 = FMath::Clamp(FMath::CeilToInt32((Hy - FirstY) / FinerCell), 0, WetNY - 1);
				for (int32 L = L0; L <= L1; ++L)
				{
					for (int32 K = K0; K <= K1; ++K)
					{
						const FVector2D P(FirstX + K * FinerCell, FirstY + L * FinerCell);
						const double T = FMath::Clamp(((P - A) | Along) / Long2, 0.0, 1.0);
						const FVector2D To = A + Along * T - P;
						const double Off = To.Size();
						const float Half = FMath::Lerp(HalfA, HalfB, static_cast<float>(T));
						const int32 At = L * WetNX + K;
						if (Off < Reach && Off - Half < FinerAt[At] - FinerHalf[At])
						{
							FinerAt[At] = static_cast<float>(Off);
							FinerDx[At] = static_cast<float>(To.X);
							FinerDy[At] = static_cast<float>(To.Y);
							FinerHalf[At] = Half;
						}
					}
				}
			}
		};
		const bool bFromHigh = Terrain.Cell > 3.0; // (the coarse sheet of the whole world, seen from high up)
		// The rules' own rivers of the wider chart are laid as ribbons of water, as Europe's are. Where one runs, the
		// painted course of the same river stands aside, and a painted tributary runs right up to the ribbon (the art
		// director's ruling of round 25). They are kept here in the world's own frame, as the courses are. (Not
		// from high up: there the ribbons are put away, and the painted great rivers stand for them.)
		struct FRuledStretch
		{
			FVector2D A, B;
			bool bSource = false, bMouth = false;
			float Half = 0.5f; // half the ribbon's breadth, about
		};
		TArray<FRuledStretch> Ruled;
		FBox2D RuledBox(ForceInit);
		if (!bFromHigh)
		{
			for (const TArray<FVector2D>& River : Data.RuledRivers)
			{
				for (int32 K = 0; K + 1 < River.Num(); ++K)
				{
					FRuledStretch& Stretch = Ruled.AddDefaulted_GetRef();
					Stretch.A = FVector2D(FMath::Fmod(River[K].X + Data.ShiftX + Data.WorldWidth, Data.WorldWidth), River[K].Y);
					Stretch.B = Stretch.A + (River[K + 1] - River[K]);
					Stretch.bSource = K == 0;
					Stretch.bMouth = K + 2 == River.Num();
					const int32 RiverIndex = static_cast<int32>(&River - Data.RuledRivers.GetData());
					Stretch.Half = static_cast<float>((Data.RuledBroad.IsValidIndex(RiverIndex) ? Data.RuledBroad[RiverIndex] : 0.5) * 0.8);
					RuledBox += Stretch.A;
					RuledBox += Stretch.B;
				}
			}
		}
		// (how far a place of a course lies from the nearest ruled river, and the place on that river nearest it: past
		// either end of a ruled river it counts as far off, unless it is all but on the end itself)
		auto ToRuled = [&Ruled](const FVector2D& P, FVector2D& OutNearest) -> double
		{
			double Best = 1.0e9;
			for (const FRuledStretch& Stretch : Ruled)
			{
				const FVector2D Along = Stretch.B - Stretch.A;
				const double T = ((P - Stretch.A) | Along) / FMath::Max(Along.SizeSquared(), 1.0e-9);
				const FVector2D Nearest = Stretch.A + Along * FMath::Clamp(T, 0.0, 1.0);
				double Off = FVector2D::Distance(P, Nearest);
				if (((T < 0.0 && Stretch.bSource) || (T > 1.0 && Stretch.bMouth)) && Off > 1.2)
				{
					Off += 100.0;
				}
				if (Off < Best)
				{
					Best = Off;
					OutNearest = Nearest;
				}
			}
			return Best;
		};
		// (which turn of the world brings a course over the ruled rivers, if any does)
		auto RuledTurn = [&](const FCampaignWorldRivers::FCourse& Of, double& OutOffset)
		{
			if (Ruled.Num() == 0)
			{
				return false;
			}
			for (const double Offset : { 0.0, -Data.WorldWidth, Data.WorldWidth })
			{
				if (Of.MaxX + Offset >= RuledBox.Min.X - 32.0 && Of.MinX + Offset <= RuledBox.Max.X + 32.0 && Of.MaxY >= RuledBox.Min.Y - 32.0 && Of.MinY <= RuledBox.Max.Y + 32.0)
				{
					OutOffset = Offset;
					return true;
				}
			}
			return false;
		};
		// (A painted course is the ruled river itself where half of it or more runs within twenty-five units of one.
		// The rules give a great river a score of points over a thousand units, and the ribbon drawn through them
		// lies tens of units from the course the heights give in places: at eight units the painted river came back
		// wherever the two parted, as fat blue scraps beside the ribbon.)
		TArray<int8> SameRiver;
		SameRiver.Init(-1, WorldRivers.Courses.Num());
		auto IsSameRiver = [&](int32 Index)
		{
			if (!WorldRivers.Courses.IsValidIndex(Index))
			{
				return false;
			}
			if (SameRiver[Index] < 0)
			{
				const FCampaignWorldRivers::FCourse& Of = WorldRivers.Courses[Index];
				double Offset = 0.0;
				int32 Near = 0;
				if (Of.Points.Num() >= 4 && RuledTurn(Of, Offset))
				{
					FVector2D Unused;
					for (const FVector2f& Point : Of.Points)
					{
						Near += ToRuled(FVector2D(Point.X + Offset, Point.Y), Unused) < 25.0 ? 1 : 0;
					}
				}
				SameRiver[Index] = Near * 2 >= Of.Points.Num() && Near > 0 ? 1 : 0;
			}
			return SameRiver[Index] == 1;
		};
		TArray<FVector2D> CourseLine, CourseLineBefore; // a course as it is laid on this sheet
		bool bMeetsRibbon = false; // the course being laid runs into a ruled river's ribbon at its end
		TArray<FVector2D> LaidLine;
		TArray<float> LaidHalf, LaidWater;
		for (const FCampaignWorldRivers::FCourse& Course : WorldRivers.Courses)
		{
			if (Course.MaxY < BoxMinY || Course.MinY > BoxMaxY)
			{
				continue;
			}
			const int32 Last = Course.Points.Num() - 1;
			if (Last < 1)
			{
				continue;
			}
			CourseLine.Reset();
			for (const FVector2f& Point : Course.Points)
			{
				CourseLine.Emplace(Point.X, Point.Y);
			}
			int32 LaidTo = Last; // the last of its points that is laid
			if (bFromHigh)
			{
				// (The art director's round 27. From the top a point of the waters' picture is six map units: a hairpin
				// tighter than that had both its limbs inside one point, which looked to both and drew neither, and the
				// line broke at the bend. The course is laid there without its smallest windings.)
				for (int32 Pass = 0; Pass < 2; ++Pass)
				{
					CourseLineBefore = CourseLine;
					for (int32 K = 1; K < Last; ++K)
					{
						const int32 Span = FMath::Min3(4, K, Last - K);
						FVector2D Sum(0.0, 0.0);
						for (int32 N = K - Span; N <= K + Span; ++N)
						{
							Sum += CourseLineBefore[N];
						}
						CourseLine[K] = Sum / (2.0 * Span + 1.0);
					}
				}
				// (and a tributary stops at the bank of the river it joins, not at its middle: where the two lines ran
				// into each other the picture drew a blob)
				if (Course.Joins >= 0)
				{
					double RunBack = 0.0;
					while (LaidTo > 1 && RunBack + FVector2D::Distance(CourseLine[LaidTo], CourseLine[LaidTo - 1]) < 3.0)
					{
						RunBack += FVector2D::Distance(CourseLine[LaidTo], CourseLine[LaidTo - 1]);
						--LaidTo;
					}
				}
			}
			// (three ranks: the river, its tributaries at no more than 0.36 of its breadth, the rest hairlines: see the courses)
			auto HalfHere = [&Course, bFromHigh](int32 Point, float& OutCarried)
			{
				float HalfLaid = Course.BreadthAt(Point, OutCarried);
				if (Course.bGreat)
				{
					// (The art director's rounds 25 and 27: a great river is a great river in every country. The Niger
					// takes most of its water far down its course, and by its water alone it was a thread at its great
					// bend, beside a Nile of ten pixels. Once it carries a river's water it has a river's breadth, and
					// it counts as running all the year: blue down the middle of its bed, with its strip of green.)
					// (it comes to that breadth by degrees from near its source: brought in over a short reach, the Niger
					// began as a hairline and was a river one confluence later)
					const float River = FMath::SmoothStep(8000.0f, 60000.0f, OutCarried);
					HalfLaid = FMath::Max(HalfLaid, River);
					// (The art director's round 28: above its join at Mopti the Niger's cream bed was three times its
					// water's width, a chalk road. A great river counts as running all the year from early in its
					// course: its bed is then no wider than its water.)
					OutCarried = FMath::Max(OutCarried, 160000.0f * FMath::SmoothStep(0.15f, 0.45f, River));
				}
				if (bFromHigh && HalfLaid < 1.0f)
				{
					// (from high up only the great rivers, and each comes in from nothing: the land's paint draws a
					// water from 0.8 of half-breadth up as a line of even width, and brings it in over the next eighth)
					// (The art director's rounds 31 and 33: from the top the Amazon is the one river to be picked out across
					// its basin. The very greatest, half-broad beyond a map unit by the water they carry, keep growing here,
					// and the land's paint draws them broader: see "WetGreatest".)
					HalfLaid = HalfLaid < 0.72f ? 0.0f : HalfLaid <= 1.0f ? 0.8f + 0.125f * FMath::SmoothStep(0.72f, 1.0f, HalfLaid) : 0.925f + 1.1f * (HalfLaid - 1.0f);
				}
				return HalfLaid;
			};
			bMeetsRibbon = false;
			{
				double Offset = 0.0;
				const int32 CourseIndex = static_cast<int32>(&Course - WorldRivers.Courses.GetData());
				if (!bFromHigh && RuledTurn(Course, Offset))
				{
					FVector2D Nearest;
					if (IsSameRiver(CourseIndex))
					{
						// (The ruled river itself. From where it first comes to the ribbon it stands aside whole: it is
						// the same water all the way down, however far the heights' course and the ribbon part. What
						// is left is its headwaters above the ribbon's source, which run into the ribbon.)
						int32 First = INDEX_NONE;
						for (int32 K = 0; K <= LaidTo && First == INDEX_NONE; ++K)
						{
							if (ToRuled(CourseLine[K] + FVector2D(Offset, 0.0), Nearest) < 25.0)
							{
								First = K;
							}
						}
						if (First != INDEX_NONE)
						{
							if (First < 2)
							{
								continue; // (none of it is left)
							}
							CourseLine[First] = Nearest - FVector2D(Offset, 0.0);
							LaidTo = First;
							bMeetsRibbon = true;
						}
					}
					else if ((IsSameRiver(Course.Joins) || Course.Joins < 0) && ToRuled(CourseLine[LaidTo] + FVector2D(Offset, 0.0), Nearest) < 30.0)
					{
						// (a tributary of it: the painted river it joined has stood aside, so it runs on to the ribbon.
						// Where its course crosses the ribbon on the way to where the painted river ran, it stops there.)
						bool bCrossed = false;
						for (int32 K = 0; K < LaidTo && !bCrossed; ++K)
						{
							const FVector2D P0 = CourseLine[K] + FVector2D(Offset, 0.0), P1 = CourseLine[K + 1] + FVector2D(Offset, 0.0);
							for (const FRuledStretch& Stretch : Ruled)
							{
								const FVector2D R = P1 - P0, Q = Stretch.B - Stretch.A;
								const double Cross = R.X * Q.Y - R.Y * Q.X;
								if (FMath::Abs(Cross) < 1.0e-9)
								{
									continue;
								}
								const FVector2D Between = Stretch.A - P0;
								const double T = (Between.X * Q.Y - Between.Y * Q.X) / Cross, U = (Between.X * R.Y - Between.Y * R.X) / Cross;
								if (T >= 0.0 && T <= 1.0 && U >= 0.0 && U <= 1.0)
								{
									CourseLine[K + 1] = P0 + R * T - FVector2D(Offset, 0.0);
									LaidTo = K + 1;
									bCrossed = true;
									break;
								}
							}
						}
						if (!bCrossed)
						{
							ToRuled(CourseLine[LaidTo] + FVector2D(Offset, 0.0), Nearest);
							CourseLine[LaidTo] = Nearest - FVector2D(Offset, 0.0);
						}
						bMeetsRibbon = true;
					}
				}
			}
			if (!bFromHigh && !Course.bGreat)
			{
				// (A course a few points long, the last trickle into a closed hollow, is no river to the eye: a gully that
				// short is a blob of its pale bed. One under seven map units long that is no great river's is not laid.)
				double Long = 0.0;
				for (int32 K = 0; K < LaidTo && Long < 7.0; ++K)
				{
					Long += FVector2D::Distance(CourseLine[K], CourseLine[K + 1]);
				}
				if (Long < 7.0)
				{
					continue;
				}
			}
			// The course as it is laid: its points, half its breadth and its water at each.
			LaidLine.Reset();
			LaidHalf.Reset();
			LaidWater.Reset();
			if (bFromHigh)
			{
				for (int32 K = 0; K <= LaidTo; ++K)
				{
					float Carried = 0.0f;
					const float HalfLaid = HalfHere(K, Carried);
					LaidLine.Add(CourseLine[K]);
					LaidHalf.Add(HalfLaid);
					LaidWater.Add(Carried);
				}
			}
			else
			{
				// (The art director's round 28: three hairlines ran dead straight for a hundred pixels and more, across
				// flats where the heights give water no reason to turn. A hairline is given a slow wander, the same at a
				// place for every course that passes it, so that two that meet still meet; where it joins a broader
				// river it comes to that river's own line.)
				float JoinsThin = 1.0f;
				if (WorldRivers.Courses.IsValidIndex(Course.Joins))
				{
					const FCampaignWorldRivers::FCourse& Joined = WorldRivers.Courses[Course.Joins];
					const float JoinedHalf = FMath::Max(Joined.BreadthFor(Course.JoinsWater), Joined.bGreat ? FMath::SmoothStep(8000.0f, 60000.0f, Course.JoinsWater) : 0.0f);
					JoinsThin = 1.0f - FMath::SmoothStep(0.15f, 0.35f, JoinedHalf);
				}
				double ToEnd = 0.0;
				for (int32 K = 0; K < LaidTo; ++K)
				{
					ToEnd += FVector2D::Distance(CourseLine[K], CourseLine[K + 1]);
				}
				auto Put = [&](const FVector2D& At, float InHalf, float Carried, double Left)
				{
					// (The art director's round 30: where a painted course met a ribbon it ended at its full breadth, a
					// grey smear against the ribbon's bank. It narrows to a hairline over its last twenty pixels.)
					const float HalfLaid = bMeetsRibbon ? FMath::Lerp(FMath::Min(InHalf, 0.1f), InHalf, FMath::SmoothStep(0.0f, 5.6f, static_cast<float>(Left))) : InHalf;
					const float Thin = FMath::Lerp(JoinsThin, 1.0f - FMath::SmoothStep(0.15f, 0.35f, HalfLaid), FMath::SmoothStep(0.0f, 5.0f, static_cast<float>(Left)));
					// (up to about two and a half units either way on a grain of eleven, and a third of that on a finer one:
					// the measure of the wander the heights give the other hairlines)
					const FVector2D Wandered = FVector2D(FMath::PerlinNoise2D(At * 0.09 + FVector2D(83.0, 19.0)), FMath::PerlinNoise2D(At * 0.09 + FVector2D(7.0, 61.0))) * 2.6
						+ FVector2D(FMath::PerlinNoise2D(At * 0.31 + FVector2D(41.0, 5.0)), FMath::PerlinNoise2D(At * 0.31 + FVector2D(13.0, 97.0))) * 0.8;
					LaidLine.Add(At + Wandered * Thin);
					LaidHalf.Add(HalfLaid);
					LaidWater.Add(Carried);
				};
				for (int32 K = 0; K < LaidTo; ++K)
				{
					float WaterA = 0.0f, WaterB = 0.0f;
					const float HalfA = HalfHere(K, WaterA), HalfB = HalfHere(K + 1, WaterB);
					const double Long = FVector2D::Distance(CourseLine[K], CourseLine[K + 1]);
					// (a long straight stretch of a hairline is laid in pieces of about two units, each with its own wander)
					const int32 Pieces = FMath::Max(HalfA, HalfB) < 0.35f ? FMath::Clamp(FMath::CeilToInt32(Long / 2.0), 1, 64) : 1;
					for (int32 Piece = 0; Piece < Pieces; ++Piece)
					{
						const float T = static_cast<float>(Piece) / Pieces;
						Put(FMath::Lerp(CourseLine[K], CourseLine[K + 1], static_cast<double>(T)), FMath::Lerp(HalfA, HalfB, T), FMath::Lerp(WaterA, WaterB, T), ToEnd - Long * T);
					}
					ToEnd -= Long;
				}
				float Carried = 0.0f;
				const float HalfLaid = HalfHere(LaidTo, Carried);
				Put(CourseLine[LaidTo], HalfLaid, Carried, 0.0);
			}
			for (int32 Turn = -2; Turn <= 2; ++Turn)
			{
				// (the course in the map's own measure: the world's own less Europe's place in it, and any turn of the world east or west)
				const double Shift = Turn * Data.WorldWidth - Data.ShiftX;
				if (Course.MaxX + Shift < BoxMinX || Course.MinX + Shift > BoxMaxX)
				{
					continue;
				}
				for (int32 K = 0; K + 1 < LaidLine.Num(); ++K)
				{
					if (bFromHigh && FMath::Max(LaidHalf[K], LaidHalf[K + 1]) <= 0.0f)
					{
						continue;
					}
					Lay(FVector2D(LaidLine[K].X + Shift, LaidLine[K].Y), FVector2D(LaidLine[K + 1].X + Shift, LaidLine[K + 1].Y), LaidHalf[K], LaidHalf[K + 1], LaidWater[K], LaidWater[K + 1]);
				}
			}
		}
		if (!bFromHigh && Ruled.Num() > 0)
		{
			// (Where a ruled river's ribbon runs, its place is laid as water that is not painted: the canopy opens
			// under the ribbon, the red clearings come to its banks as they came to the painted river's, no field
			// and no lake lies in it, and a painted water beside it gives way to it.)
			bPhantomLay = true;
			for (const FRuledStretch& Stretch : Ruled)
			{
				for (int32 Turn = -2; Turn <= 2; ++Turn)
				{
					const FVector2D Shift(Turn * Data.WorldWidth - Data.ShiftX, 0.0);
					Lay(Stretch.A + Shift, Stretch.B + Shift, Stretch.Half, Stretch.Half, -2.0f, -2.0f);
				}
			}
			bPhantomLay = false;
		}
		// The small lakes of the northern forest and the pools of the tundra (the art director's world look: "many
		// small lakes with pale shores", "pools over about 6%"; her ruling of round 25: rounded shapes only, and none
		// within a river's breadth of a river; and of round 27: lake country, not sprinkles). They come in every
		// size, a few of them many times the rest; they lie in clusters with empty ground between; and no two have
		// the same outline. Each is laid like short stretches of river, broader at one end than the other, which
		// draws a pebble; a second stretch at a bend makes it a bean, and a third to one side gives it a bulge.
		// Where one lies is settled by the place alone, so that every sheet that draws a place draws the same lake
		// there: the great ones first, on a ruling of their own; then the small ones on a finer ruling, each inside
		// its own square of it (a square that is itself pushed about, so that no row of them shows), and each clear
		// of every river and every lake laid before it. (From high up, on the coarse sheet, none: they are smaller
		// than its points.)
		if (!bFromHigh)
		{
			// (how near a river or another water runs to a place of this sheet, less its own half breadth: far away where none is known)
			auto WaterNear = [&](const FVector2D& At) -> float
			{
				const int32 I = FMath::RoundToInt32((At.X - Terrain.OriginX) / Cell), J = FMath::RoundToInt32((At.Y - Terrain.OriginY) / Cell);
				if (I < 0 || J < 0 || I >= NX || J >= NY)
				{
					return 0.0f; // (off this sheet: nothing can be said, and no lake is laid from there)
				}
				const int32 Idx = Terrain.Index(I, J);
				// (a lake may lie nearer another lake than a river: lakes come in groups; counted here as if it stood a unit further off)
				return RiverHalf[Idx] > 0.0f ? RiverAt[Idx] - RiverHalf[Idx] + (RiverWater[Idx] < 0.0f && RiverWater[Idx] > -1.5f ? 0.9f : 0.0f) : 1000.0f;
			};
			struct FLakeRuling
			{
				bool bPool, bGreatOne;
				double Pitch; // the ruling's squares, in map units
				float Chance; // of a square holding one, in the thick of lake country
				float Least, Most, Skew; // half its breadth at its middle: from, to, and how far the small outnumber the large
				float CountryFrom, CountryTo; // how much of the country is lake country
			};
			const FLakeRuling Rulings[] =
			{
				{ false, true, 60.0, 0.34f, 1.6f, 2.35f, 1.0f, -0.6f, 0.3f },
				// (The art director's round 28: the tundra's lakes were too even in size. Each view has three or four
				// thaw lakes two to three times the rest: long, at the most the picture can hold in breadth.)
				{ true, true, 45.0, 1.0f, 2.25f, 2.45f, 1.0f, -0.25f, 0.45f },
				{ false, false, 6.0, 1.0f, 0.4f, 1.7f, 1.3f, 0.0f, 0.3f },
				{ true, false, 6.5, 1.0f, 0.55f, 1.5f, 1.15f, 0.05f, 0.3f },
			};
			for (const FLakeRuling& Ruling : Rulings)
			{
				const bool bPool = Ruling.bPool;
				const double Pitch = Ruling.Pitch;
				const float Thickly = bPool ? LakesFrom().Y : LakesFrom().X;
				const int32 A0 = FMath::FloorToInt32(BoxMinX / Pitch) - 1, A1 = FMath::CeilToInt32(BoxMaxX / Pitch) + 1, B0 = FMath::FloorToInt32(BoxMinY / Pitch) - 1, B1 = FMath::CeilToInt32(BoxMaxY / Pitch) + 1;
				for (int32 B = B0; B <= B1; ++B)
				{
					for (int32 A = A0; A <= A1; ++A)
					{
						// (the same place is the same square whichever turn of the world it is counted in)
						const double Around = FMath::Fmod(FMath::Fmod(A * Pitch + Data.ShiftX, Data.WorldWidth) + Data.WorldWidth, Data.WorldWidth);
						uint32 Seed = HashCombineFast(GetTypeHash(FMath::RoundToInt32(Around / Pitch * 16.0)), GetTypeHash(B * 7919 + 13 + (bPool ? 104729 : 0) + (Ruling.bGreatOne ? 611953 : 0)));
						auto Draw = [&Seed]() { Seed = Seed * 1664525u + 1013904223u; return static_cast<float>((Seed >> 8) & 0xFFFF) / 65535.0f; };
						const float Luck = Draw(), Rx = Draw(), Ry = Draw(), Way = Draw(), LongA = Draw(), LongB = Draw(), Broad = Draw(), Bend = Draw(), Lean = Draw(), Lobe = Draw(), LobeWay = Draw(), LobeSize = Draw();
						// (its size: most are small, and a few are many times the rest)
						const float Radius = Ruling.Least * FMath::Pow(Ruling.Most / Ruling.Least, FMath::Pow(Broad, Ruling.Skew));
						// (its outline: broader at one end than the other, bent in the middle, and the larger with a bulge to one side)
						const float RadiusA = Radius * (0.6f + 0.4f * Lean), RadiusB = Radius * (1.0f - 0.4f * Lean);
						const float Room = static_cast<float>(Pitch) * 0.5f - 0.2f;
						// (a great pool of the tundra is a long thaw lake)
						const float LongLeast = bPool && Ruling.bGreatOne ? 0.75f : 0.3f;
						const float StretchA = FMath::Clamp(Radius * (LongLeast + 0.95f * LongA), 0.0f, FMath::Max(0.92f * Room - RadiusA, 0.0f));
						const float StretchB = FMath::Clamp(Radius * (LongLeast + 0.95f * LongB), 0.0f, FMath::Max(0.92f * Room - RadiusB, 0.0f));
						const bool bBulge = Ruling.bGreatOne || (Radius > 0.75f && Lobe < 0.6f);
						// (a bulge is broad and shallow: a narrow one stood out as a knob with a nick each side of it)
						const float RadiusC = Radius * (0.7f + 0.2f * LobeSize);
						const float StretchC = bBulge ? FMath::Clamp(Radius * (0.3f + 0.3f * LobeSize), 0.0f, FMath::Max(0.92f * Room - RadiusC, 0.0f)) : 0.0f;
						const float Extent = FMath::Max(FMath::Max3(Radius, StretchA + RadiusA, StretchB + RadiusB), bBulge ? StretchC + RadiusC : 0.0f);
						// (where in its square: anywhere it fits; and the square itself pushed about by a slow unevenness of
						// the country, the same for its neighbours, so that the ruling never shows as rows)
						// (On the open tundra, where nothing hides it, a pool kept inside its square still showed the ruling
						// faintly: a pool's nearest neighbours lay level or plumb with it too often. There a small pool
						// may lie anywhere in its square, over its edge too; one that then lies on another is not laid.)
						const double Free = bPool && !Ruling.bGreatOne ? Room : FMath::Max(Room - Extent, 0.0f);
						const FVector2D Kept(Around + 0.5 * Pitch, (B + 0.5) * Pitch); // (its square's middle, as the world keeps it)
						const FVector2D Pushed = Ruling.bGreatOne ? FVector2D::ZeroVector
							: FVector2D(FMath::PerlinNoise2D(Kept * 0.045 + FVector2D(23.0, 77.0)), FMath::PerlinNoise2D(Kept * 0.045 + FVector2D(59.0, 31.0))) * 2.2;
						const FVector2D Centre = FVector2D((A + 0.5) * Pitch + (2.0 * Rx - 1.0) * Free, (B + 0.5) * Pitch + (2.0 * Ry - 1.0) * Free) + Pushed;
						const float Latitude = static_cast<float>(Data.LatitudeAt(Centre.Y)), Longitude = static_cast<float>(Data.LongitudeAt(Centre.X));
						if (BeyondEurope(Latitude, Longitude) < 0.9f)
						{
							continue;
						}
						float Shares[WorldLook::Count];
						Climate.SharesAt(Latitude, Longitude, Shares);
						const float Taiga = Shares[WorldLook::Taiga], Tundra = Shares[WorldLook::Tundra];
						// (lake country lies in clusters, with empty ground between: a slow unevenness of the place says how thickly)
						// (at three sizes: the lake districts themselves, the groups within one, and the knots of two or three:
						// measured in the pictures, lakes sown by the first alone were still spaced more evenly than chance)
						const float Unevenness = 0.55f * FMath::PerlinNoise2D(Kept * 0.021 + FVector2D(5.0, 91.0)) + 0.8f * FMath::PerlinNoise2D(Kept * 0.06 + FVector2D(63.0, 17.0))
							+ 0.45f * FMath::PerlinNoise2D(Kept * 0.13 + FVector2D(29.0, 47.0));
						const float Country = FMath::SmoothStep(Ruling.CountryFrom, Ruling.CountryTo, Unevenness);
						if ((Tundra > Taiga) != bPool || Luck > Ruling.Chance * Thickly * Country * FMath::SmoothStep(0.4f, 0.8f, bPool ? Tundra : Taiga))
						{
							continue;
						}
						const double Turned = Way * 2.0 * UE_DOUBLE_PI, Bent = Turned + (Bend - 0.5) * 1.5;
						const FVector2D P0 = Centre - FVector2D(FMath::Cos(Turned), FMath::Sin(Turned)) * StretchA, P2 = Centre + FVector2D(FMath::Cos(Bent), FMath::Sin(Bent)) * StretchB;
						const double Aside = Turned + (LobeWay < 0.5f ? 1.0 : -1.0) * (0.5 * UE_DOUBLE_PI + (LobeSize - 0.5) * 0.7);
						const FVector2D P3 = Centre + FVector2D(FMath::Cos(Aside), FMath::Sin(Aside)) * StretchC;
						// (on level ground, well inland, below the tree line)
						const float Here = Terrain.MetresAt(Centre.X, Centre.Y);
						const float Slope = FMath::Max(FMath::Abs(Terrain.MetresAt(Centre.X + 2.0, Centre.Y) - Terrain.MetresAt(Centre.X - 2.0, Centre.Y)),
							FMath::Abs(Terrain.MetresAt(Centre.X, Centre.Y + 2.0) - Terrain.MetresAt(Centre.X, Centre.Y - 2.0)));
						const float Tilt = FMath::Max(FMath::Abs(Terrain.MetresAt(P0.X, P0.Y) - Here), FMath::Abs(Terrain.MetresAt(P2.X, P2.Y) - Here));
						if (Here <= 2.0f || Slope > 45.0f || Tilt > 60.0f || Terrain.ShoreAt(Centre.X, Centre.Y) < (bPool ? 5.5f : 3.5f) + (Ruling.bGreatOne ? Extent : 0.0f)
							|| (!bPool && Here > WorldTreeLine(Latitude) - 150.0f))
						{
							continue;
						}
						// (no lake within a river's breadth of a river, or a unit and a half: where a river widens it is the
						// river's own water. And none touching another lake: two that ran together made one with a nick in it.)
						if (WaterNear(Centre) < Radius + 1.5f || WaterNear(P0) < RadiusA + 1.5f || WaterNear(P2) < RadiusB + 1.5f || (bBulge && WaterNear(P3) < RadiusC + 1.5f))
						{
							continue;
						}
						Lay(P0, Centre, RadiusA, Radius, -1.0f, -1.0f);
						Lay(Centre, P2, Radius, RadiusB, -1.0f, -1.0f);
						if (bBulge)
						{
							Lay(Centre, P3, Radius * 0.9f, RadiusC, -1.0f, -1.0f);
						}
					}
				}
			}
		}
	}

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
				C = Mix(C, LookGround(Shares, Metres, P, WorldTreeLine(static_cast<float>(Data.LatitudeAt(P.Y)))), Outside[Idx]);
				if (bWaters)
				{
					const float Out = Outside[Idx], Off = RiverAt[Idx], Half = RiverHalf[Idx];
					const float Jungle = Shares[WorldLook::Jungle], Savanna = Shares[WorldLook::Savanna], Steppe = Shares[WorldLook::Steppe], Desert = Shares[WorldLook::Desert];
					if (Half > 0.0f && Off < WetReach && RiverWater[Idx] < -1.5f)
					{
						// (the place of a ruled river's ribbon: nothing is painted here)
					}
					else if (Half > 0.0f && Off < WetReach && RiverWater[Idx] < 0.0f)
					{
						// (a lake: deep blue-grey. Its pale shore lies along part of its edge only, the side the weather
						// drives the water to; which side that is turns slowly from one stretch of country to the next.
						// The art director's round 27: not an even ring.)
						const FVector3f Laid = Tone();
						const float WeatherWay = 2.0f * UE_PI * FMath::PerlinNoise2D(P * 0.011 + FVector2D(41.0, 13.0));
						const float Leeward = Off > 0.02f ? -(RiverDx[Idx] * FMath::Cos(WeatherWay) + RiverDy[Idx] * FMath::Sin(WeatherWay)) / Off : 0.0f;
						WetInk[Idx] = FColor(ToByte(40.0f / 255.0f * Laid.X), ToByte(70.0f / 255.0f * Laid.Y), ToByte(100.0f / 255.0f * Laid.Z), BedByte(FMath::Max(0.3f * FMath::SmoothStep(0.1f, 0.8f, Leeward), 0.03f), true)); // (never quite none: at none the picture's number sits on the line between a steady shore and a river's bars, and the bars showed as a comb along the edge)
					}
					else if (Half > 0.0f && Off < WetReach)
					{
						// The river's water by the country it runs through (the art director's world look). A great river
						// runs all the year; a lesser one in dry country is a bed of sand.
						const float Great = FMath::SmoothStep(40000.0f, 160000.0f, RiverWater[Idx]);
						const FSeen Blue = FSeen::Bytes(72, 104, 125); // (as Europe's rivers are laid)
						FSeen Ink = Blue;
						float Bed = 0.1f;
						// (The art director's round 33, ruling C: the jungle's rivers are a silted blue like the ribbon they run
						// into, to be seen at about (98, 146, 160). Brown read as a road beside every other river's blue.)
						// (laid (103, 118, 139) it was seen from far up at (119, 132, 152), a lavender: laid greener and less red)
						// (laid (82, 132, 147) it was seen at (85, 149, 160): a little more red)
						Ink = Mix(Ink, FSeen::Bytes(90, 130, 146), Jungle);
						// (a hairline has no bars: the art director's ruling of round 25)
						// (the art director's round 31: a jungle river's sand lay along its middle like the dashes of a road; less of it)
						Bed = FMath::Lerp(Bed, 0.16f * FMath::SmoothStep(0.2f, 0.45f, Half), Jungle);
						// (the savanna's are dry beds of sand; a great one keeps a stream down its middle)
						Ink = Mix(Ink, FSeen::Bytes(104, 122, 112), Savanna);
						Bed = FMath::Lerp(Bed, FMath::Lerp(1.0f, 0.28f, Great), Savanna);
						// (the steppe's are pale dry gullies with a thread of green)
						Ink = Mix(Ink, Mix(FSeen::Bytes(112, 132, 84), Blue, Great), Steppe);
						Bed = FMath::Lerp(Bed, FMath::Lerp(0.6f, 0.3f, Great), Steppe);
						// (and in the desert only the rivers that come from wetter country run: the Nile, the Indus)
						Bed = FMath::Lerp(Bed, FMath::Lerp(1.0f, 0.28f, Great), Desert);
						// (a dry bed and a gully are sand from bank to bank the whole way along; a running river's bars come and go)
						const bool bSteady = (Savanna + Desert) * (1.0f - Great) + Steppe > 0.5f;
						const FVector3f Laid = Tone();
						WetInk[Idx] = FColor(ToByte(Ink.R * Laid.X), ToByte(Ink.G * Laid.Y), ToByte(Ink.B * Laid.Z), BedByte(Bed, bSteady));
						// (a prairie creek is a thread of water to be seen between its trees)
						// (The art director's round 27: at working height a prairie creek is a hairline with its ribbon of
						// trees. At 0.15 the land's paint let it go from sight just under that height, with the jungle's hairlines.)
						if (Shares[WorldLook::Prairie] > 0.5f)
						{
							RiverHalf[Idx] = FMath::Max(RiverHalf[Idx], 0.25f);
							HalfFloor[Idx] = 0.25f;
						}
						// (the steppe's gully is a pale bed some way across, with its thread of green: a hairline of it was a dark scratch)
						if (Steppe > 0.5f && Great < 0.5f)
						{
							RiverHalf[Idx] = FMath::Max(RiverHalf[Idx], 0.38f * FMath::SmoothStep(0.5f, 0.8f, Steppe));
							HalfFloor[Idx] = FMath::Max(HalfFloor[Idx], 0.38f * FMath::SmoothStep(0.5f, 0.8f, Steppe));
						}
						// (a great river through dry country waters a strip of it: green, and farmed)
						// (in the savanna too, a little less: the green of the Niger's lower course begins above Mopti)
						const float Parched = FMath::Clamp(Desert + Steppe + Shares[WorldLook::DryPlain] + 0.6f * Shares[WorldLook::DrySouth] + 0.7f * Savanna, 0.0f, 1.0f);
						const float Strip = Out * Parched * Great * (1.0f - FMath::SmoothStep(Half + 1.0f, Half + 4.6f, Off));
						if (Strip > 0.0f)
						{
							C = Mix(C, FSeen::Bytes(112, 136, 80), 0.85f * Strip);
							Dry[Idx] *= 1.0f - 0.9f * Strip;
							LookFields[Idx] = FMath::Max(LookFields[Idx], Strip);
						}
					}
					if (Jungle > 0.55f && Terrain.Cell <= 3.0)
					{
						// (clearings of red earth: under a fiftieth of the jungle, and most of them on a river's bank)
						const float Patch = FMath::PerlinNoise2D(P * 0.23 + FVector2D(17.0, 71.0)) + 0.5f * FMath::PerlinNoise2D(P * 0.61 + FVector2D(3.0, 29.0));
						// (not at a river's very head, where it is a thread: a clearing there was a red match-head on every source)
						// (and only beside a course still drawn as a river, so that none floats alone in the canopy by a hairline)
						// (The art director's round 33: along a ruled river's ribbon the clearings showed as a neon fringe of raw
						// paint, magenta in Peru and orange in Brazil. The bank of a ribbon takes none.)
						// (And the same ruling finished: the small streams' banks carried the same fringe, clearing after clearing
						// in a row. No river's bank takes a clearing now: the few there are lie out in the forest.)
						const float OffBank = Half > 0.0f ? FMath::SmoothStep(Half + 1.0f, Half + 3.5f, Off) : 1.0f;
						const float Clear = FMath::SmoothStep(0.0f, 0.1f, Patch - ClearingFrom().X) * FMath::SmoothStep(0.55f, 0.8f, Jungle) * Out * OffBank;
						if (Clear > 0.0f)
						{
							C = Mix(C, FSeen::Bytes(152, 97, 86), Clear); // (laid bluer than it is to be seen: the lamp brings it to about (152, 94, 64))
							Opened[Idx] = FMath::Max(Opened[Idx], Clear);
						}
					}
				}
			}
			C = C * (0.95f + 0.10f * Fine);
			GroundR[Idx] = C.R;
			GroundG[Idx] = C.G;
			GroundB[Idx] = C.B;
		}
	}, TheaterSpeed::ModelLoop());

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
	// (SMOOTH HILLS. The broad shape of the land: the heights softened by about three of their own cells. The light
	// and shade is taken from this, with a third of the small folds laid back over it; and so is the lean the lamp sees.)
	TArray<float> Broad = Squeezed, BroadLie = Terrain.Height;
	FCampaignTerrain::Smooth(Broad, NX, NY, 3, 2);
	FCampaignTerrain::Smooth(BroadLie, NX, NY, 3, 2);
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
		const bool bFineBeyond = bBeyond && Terrain.Cell <= 3.0 && Terrain.HasElevation() && Outside.Num() == Count;
		ParallelFor(NY, [&](int32 J)
		{
			for (int32 I = 0; I < NX; ++I)
			{
				const int32 Idx = Terrain.Index(I, J);
				if (bFineBeyond && Outside[Idx] > 0.5f)
				{
					// (The art director's round 32: in West Texas the shading ran in steps about seven points long. A
					// shadow was thrown on a whole cell or on none of it, and a fine sheet's cell is five or six points
					// of the screen. Beyond Europe, where the heights are a smooth surface, it is thrown from nine points
					// of each cell, and the cell takes the share of them that lie in shadow: a step is a third as long.)
					const FVector2D Middle = Terrain.PointAt(I, J);
					int32 Dark = 0;
					for (int32 Sub = 0; Sub < 9; ++Sub)
					{
						const FVector2D From = Middle + FVector2D((Sub % 3 - 1) / 3.0, (Sub / 3 - 1) / 3.0) * Terrain.Cell;
						const float Mine = FMath::Max(Terrain.MetresAt(From.X, From.Y), 0.0f) * Tall;
						for (int32 K = 1; K <= Steps; ++K)
						{
							const FVector2D Over = From + FVector2D(Step.X, Step.Y) * (K * Terrain.Cell);
							if (FMath::Max(Terrain.MetresAt(Over.X, Over.Y), 0.0f) * Tall - K * Drop > Mine + 0.15f)
							{
								++Dark;
								break;
							}
						}
					}
					Thrown[Idx] = Dark / 9.0f;
					continue;
				}
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
		}, TheaterSpeed::ModelLoop());
		// (a thrown shadow's edge is spread over two cells each way: no stair step is to be counted along it)
		FCampaignTerrain::Smooth(Thrown, NX, NY, 2, 2);
	}

	Ground.SetNumUninitialized(Count);
	Relief.SetNumUninitialized(Count);
	Masks.SetNumUninitialized(Count);
	if (bWaters)
	{
		Wet.Init(FColor(255, 255, 255, 0), WetNX * WetNY);
	}
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
			// (SMOOTH HILLS, the art director's review of build 1448: the snow line stepped in bricks along a crest. It is
			// read from the softened height and the softened lean, as the shade is.)
			const int32 SnowW = FMath::Max(I - 1, 0), SnowE = FMath::Min(I + 1, NX - 1), SnowN = FMath::Max(J - 1, 0), SnowS = FMath::Min(J + 1, NY - 1);
			const float SnowLean = FMath::Clamp(FVector2f((BroadLie[Terrain.Index(SnowE, J)] - BroadLie[Terrain.Index(SnowW, J)]) / static_cast<float>((SnowE - SnowW) * Terrain.Cell),
				(BroadLie[Terrain.Index(I, SnowS)] - BroadLie[Terrain.Index(I, SnowN)]) / static_cast<float>((SnowS - SnowN) * Terrain.Cell)).Size() * 0.9f, 0.0f, 1.0f);
			float Snow = 0.9f * FMath::SmoothStep(5.0f, 6.6f, 0.5f * (H + BroadLie[Idx])) * (1.0f - 0.5f * SnowLean);
			const float Beyond = bBeyond ? Outside[Idx] : 0.0f;
			// (a jungle keeps its roof on the steepest range: no rock shows through it below about 3,300 metres)
			const float JungleRoof = bBeyond ? LookTropic[Idx] * (1.0f - FMath::SmoothStep(3000.0f, 3600.0f, Metres[Idx])) : 0.0f;
			if (Beyond > 0.0f)
			{
				// (one snow height for the whole world would whiten New Guinea and leave Alaska bare)
				Snow = FMath::Lerp(Snow, 0.9f * FMath::SmoothStep(LookSnow[Idx], LookSnow[Idx] + (MountainLook().bOn ? MountainLook().SnowBand : 800.0f), Metres[Idx]) * (1.0f - 0.5f * SnowLean), Beyond);
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
			// (Beyond Europe, in every look but the jungle, the openings in a wood are about a third the size and three
			// times as many, and they are strung along the low ground; and a wood there does not end at a firm line but
			// thins out, so that the land's paint can leave a fringe of single trees round every wood and opening.)
			const float Finer = bBeyond ? Beyond * (1.0f - LookTropic[Idx]) : 0.0f;
			const float Hollow = FMath::Clamp((Surround[Idx] - Metres[Idx]) / 90.0f, 0.0f, 1.0f);
			const float Thick = FMath::Lerp(Cloudy(1.0 / 34.0, FVector2D(31.0, -17.0)), Cloudy(1.0 / 11.5, FVector2D(13.0, 29.0)), Finer) * 0.62f + Cloudy(1.0 / 150.0, FVector2D(-7.0, 53.0)) * 0.38f + Woods[Idx]
				+ 0.11f * FMath::PerlinNoise2D(P * 0.13 + FVector2D(3.0, 41.0)) + 0.06f * FMath::PerlinNoise2D(P * 0.31 + FVector2D(19.0, 7.0))
				+ (FMath::SmoothStep(56.0f, 63.0f, Northing) * 0.2f - DryLand * 0.16f) * (1.0f - Beyond) + FMath::SmoothStep(250.0f, 900.0f, Metres[Idx]) * 0.08f
				// (Beyond Europe, in temperate and northern country, the plain is farmed and the hills are left to the
				// woods: the Appalachians, the Urals and Japan's ranges are wooded to the tree line.)
				+ (bBeyond ? FMath::Clamp(LookNorth[Idx] * 2.5f + LookRice[Idx], 0.0f, 1.0f) * (0.26f * FMath::SmoothStep(300.0f, 750.0f, Metres[Idx]) + 0.12f * FMath::SmoothStep(0.1f, 0.3f, Steepness)) : 0.0f);
			// (the tree line stands where the snow line is, less about 1,200 metres: at 1,750 in Europe)
			// (and in the jungle at about 3,300 metres)
			const float TreeLine = FMath::Lerp(1750.0f, WorldTreeLine(Northing), Beyond); // (Europe's own at 1,750 metres; beyond Europe it falls with the latitude, and the jungle's is 3,300)
			float Wooded = FMath::SmoothStep(WoodsFrom() - 0.09f * Finer, WoodsFrom() + 0.07f + 0.11f * Finer, Thick - 0.16f * Finer * Hollow) * FMath::SmoothStep(0.0f, 300.0f, TreeLine - Metres[Idx]);
			// (beyond Europe the woods climb the steep ridges of the lower ranges too, under about 1,800 metres)
			// (wholly: a share of rock showing through a dark canopy turns a wooded ridge purple)
			// (right up to the tree line, so that in Japan only Fuji and the crests of its Alps show rock)
			const float Roof = FMath::Max(JungleRoof, Beyond * (1.0f - FMath::SmoothStep(TreeLine - 250.0f, TreeLine + 50.0f, Metres[Idx])));
			Wooded *= (1.0f - Rock * (1.0f - Roof)) * (1.0f - Sand / 0.35f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);
			if (bWaters && Beyond > 0.0f)
			{
				// (a river comes in over the blend from Europe's own ground, which has the rules' rivers and none of these)
				const float Off = RiverAt[Idx], Half = RiverHalf[Idx] * FMath::SmoothStep(0.25f, 0.7f, Beyond);
				// (not on the coarse sheet, which is seen from high up: a point of it is wider than any river, and a wood
				// opened or planted by the point showed as a row of dashes down every course)
				if (Half > 0.0f && Terrain.Cell <= 3.0)
				{
					// (a ribbon of trees along each creek of the prairie, which has no other)
					const float Ribbon = RiverWater[Idx] >= 0.0f ? LookOf[Idx * 3] / 255.0f * (1.0f - FMath::SmoothStep(Half + 0.5f, Half + 1.6f, Off)) * FMath::SmoothStep(-0.25f, 0.1f, FMath::PerlinNoise2D(P * 0.37 + FVector2D(9.0, 57.0))) : 0.0f;
					Wooded = FMath::Max(Wooded, 0.8f * Ribbon * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]));
					// (and no canopy over the water itself: a jungle's roof is broken by its rivers)
					// (a hairline has no rim: the canopy closes to its very edge, and the water is drawn over it)
					// (The art director's round 33: not under a ruled river's ribbon. Opened there, the canopy showed the raw
					// paint of the ground beside the ribbon wherever the ribbon ran narrow: a neon fringe, magenta in Peru
					// and orange in Brazil. The ribbon lies over the canopy, and its banks are shaded instead: see below.)
					// (And the same ruling finished: nor beside a jungle's lesser rivers, where the opened rim showed as a
					// thread of raw paint down each bank, yellow in Brazil. There the canopy closes to the water's edge and
					// the water is drawn over it.)
					if (RiverWater[Idx] > -1.5f)
					{
						Wooded *= FMath::Lerp(1.0f, FMath::SmoothStep(Half + 0.05f, Half + 0.85f, Off), FMath::SmoothStep(0.2f, 0.45f, RiverHalf[Idx]) * (1.0f - FMath::SmoothStep(0.5f, 0.9f, JungleRoof)));
					}
				}
				Wooded *= 1.0f - Opened[Idx];
				if (WetScale == 1)
				{
					Wet[Idx] = FColor(ToByte(0.5f + 0.5f * FMath::Clamp(RiverDx[Idx] / WetReach, -1.0f, 1.0f)), ToByte(0.5f + 0.5f * FMath::Clamp(RiverDy[Idx] / WetReach, -1.0f, 1.0f)),
						ToByte(FMath::Min(Off / WetReach, 1.0f)), ToByte(Half / 2.5f));
				}
			}

			// Shaded relief, as on a printed map: how squarely the ground faces the sun (1 on the flat, up to 2.2
			// on a slope turned full to it, nothing on one turned away), less where a shadow is thrown across it,
			// and a little less on a valley floor than on a crest.
			const int32 IW = FMath::Max(I - 1, 0), IE = FMath::Min(I + 1, NX - 1), JN = FMath::Max(J - 1, 0), JS = FMath::Min(J + 1, NY - 1);
			const float Rough = 0.5f * FMath::PerlinNoise2D(P * 0.33) * FMath::Clamp(Steepness * 30.0f, 0.0f, 26.0f); // (broken ground on the steeps)
			// (Art fix 7: below about 300 metres the finest of the relief is cut by half. The plains are smooth and
			// the hills rough, not the other way about, and the fields can be seen.)
			const float Lowland = (1.0f - FMath::SmoothStep(200.0f, 400.0f, Metres[Idx])) * LowlandCalm();
			auto Fall = [&](const TArray<float>& Heights, int32 IA, int32 JA, int32 IB, int32 JB, int32 Steps) { return (Heights[Terrain.Index(IA, JA)] - Heights[Terrain.Index(IB, JB)]) / static_cast<float>(Steps * Terrain.Cell); };
			// (SMOOTH HILLS: the broad shape whole, and a third of the small folds over it; the broken ground of the steeps is a fold too)
			// (mountains beyond Europe: above the hills the small folds come back and the shade is cut deeper; see MountainLook)
			const float Mountain = MountainLook().bOn && bBeyond ? Beyond * FMath::SmoothStep(MountainLook().From, MountainLook().From + 900.0f, Metres[Idx]) : 0.0f;
			const float Folds = FMath::Lerp(SmallFolds(), 1.0f, Mountain * MountainLook().Folds);
			auto Hill = [&](int32 IA, int32 JA, int32 IB, int32 JB, int32 Steps)
			{
				const float Shape = Fall(Broad, IA, JA, IB, JB, Steps);
				return Shape + (Fall(Squeezed, IA, JA, IB, JB, Steps) - Shape) * Folds;
			};
			const float SlopeX = FMath::Lerp(Hill(IE, J, IW, J, IE - IW), Fall(Gentle, IE, J, IW, J, IE - IW), Lowland) + Rough * 0.13f * Folds * FMath::PerlinNoise2D(P * 0.7 + FVector2D(3.0, 9.0));
			const float SlopeY = FMath::Lerp(Hill(I, JS, I, JN, JS - JN), Fall(Gentle, I, JS, I, JN, JS - JN), Lowland) + Rough * 0.13f * Folds * FMath::PerlinNoise2D(P * 0.7 + FVector2D(17.0, 1.0));
			const float Cut = ReliefStrength() * (1.0f + MountainLook().Deep * Mountain);
			const FVector3f Leaning = FVector3f(-SlopeX * Cut, -SlopeY * Cut, 1.0f).GetSafeNormal();
			float Sun = FMath::Clamp(FVector3f::DotProduct(Leaning, ToSun) / ToSun.Z, 0.0f, 2.2f);
			Sun *= 1.0f - 0.42f * Thrown[Idx];
			if (bWaters && Terrain.Cell <= 3.0 && RiverHalf[Idx] > 0.0f && RiverWater[Idx] < -1.5f)
			{
				// (and the forest is a little darker along a ruled river's banks: the cut the river makes in the canopy)
				Sun *= FMath::Lerp(0.74f, 1.0f, FMath::SmoothStep(RiverHalf[Idx] * 0.4f, RiverHalf[Idx] + 1.6f, RiverAt[Idx]));
			}
			Sun -= 0.3f * FMath::Clamp((Surround[Idx] - Metres[Idx]) / 260.0f, -0.6f, 0.6f);
			// A little unevenness, so that large areas do not look printed.
			Sun *= 1.0f + 0.03f * (1.0f - 0.6f * Lowland) * FMath::PerlinNoise2D(FVector2D(I * 0.61, J * 0.61));
			// (Beyond Europe a canopy is one great roof: it takes half the relief's light and shade that bare ground
			// takes, so that a holder's colour reads on it in sun and in shadow alike.)
			// (and on the world's coarse sheet, which is what is seen from high up, low ground takes half the shading
			// too: a valley a cell wide, shaded whole, ran as a ruled band across New England)
			const float FarLow = bBeyond && Terrain.Cell > 3.0 ? 1.0f - 0.5f * Beyond * (1.0f - FMath::SmoothStep(450.0f, 800.0f, Metres[Idx])) : 1.0f;
			const float Roofed = (1.0f - 0.5f * Wooded * Beyond) * FarLow;
			Sun = 1.0f + (Sun - 1.0f) * Roofed;
			const float Shade = FMath::Clamp(Sun, 0.0f, 2.2f) / 2.2f * 2.0f; // (kept as half of it in the picture's blue, below)

			// Which way the ground leans, as the lamp sees it: north is +X and east is +Y in the world.
			// (on the low ground the lamp, too, sees the land leaning only half as much as it does)
			// (SMOOTH HILLS: and the lamp sees the broad lean whole and a third of the small folds, as the painted shade does)
			const FVector2f BroadLean(Fall(BroadLie, IE, J, IW, J, IE - IW), Fall(BroadLie, I, JS, I, JN, JS - JN));
			const FVector2f Lean = BroadLean + (Slope - BroadLean) * Folds;
			const FVector3f Normal = FVector3f(Lean.Y * VerticalScale * (1.0f - 0.5f * Lowland) * Roofed, -Lean.X * VerticalScale * (1.0f - 0.5f * Lowland) * Roofed, 1.0f).GetSafeNormal();

			const FVector3f Toned = Tone();
			// (A sixth of the ground shows through any canopy. Under the broadleaf woods of the temperate lands beyond
			// Europe it is a dark forest floor, or the pale upland showing through turns a wooded ridge grey-pink.)
			// (and under the canopy of the tropics too, whose own ground is a bright green that lifted the canopy an eighth)
			const float Shaded = bBeyond ? FMath::Clamp(LookFloor[Idx] + LookTropic[Idx], 0.0f, 1.0f) : 0.0f;
			const float Floor = Shaded * FMath::SmoothStep(0.5f, 0.85f, Wooded);
			const FSeen Leafmould = Mix(FSeen::Bytes(44, 66, 46), FSeen::Bytes(52, 78, 50), Shaded > 0.001f ? LookTropic[Idx] / (LookFloor[Idx] + LookTropic[Idx]) : 0.0f);
			FColor Bare(ToByte(FMath::Lerp(GroundR[Idx], Leafmould.R, Floor) * Toned.X), ToByte(FMath::Lerp(GroundG[Idx], Leafmould.G, Floor) * Toned.Y), ToByte(FMath::Lerp(GroundB[Idx], Leafmould.B, Floor) * Toned.Z), 255);
			Bare.A = ToByte(Wooded);
			Ground[Idx] = Bare;
			Relief[Idx] = FColor(ToByte(Normal.X * 0.5f + 0.5f), ToByte(Normal.Y * 0.5f + 0.5f), ToByte(Shade * 0.5f), 255);
			Masks[Idx] = FColor(ToByte(Rock * (1.0f - FMath::Max(JungleRoof, Roof * Wooded))), ToByte(Snow), ToByte(Sand), ToByte(Dry[Idx]));
		}
	}, TheaterSpeed::ModelLoop());

	{
		// ("-TheaterProbe=2056,840" writes what the painter made of the ground round a place to the log)
		FString ProbeAt, ProbeX, ProbeY;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterProbe="), ProbeAt, false) && ProbeAt.Split(TEXT(","), &ProbeX, &ProbeY))
		{
			const int32 ProbeI = FMath::RoundToInt32((FCString::Atod(*ProbeX) - Terrain.OriginX) / Terrain.Cell), ProbeJ = FMath::RoundToInt32((FCString::Atod(*ProbeY) - Terrain.OriginY) / Terrain.Cell);
			if (ProbeI >= 12 && ProbeJ >= 12 && ProbeI + 12 < NX && ProbeJ + 12 < NY)
			{
				for (const TCHAR* What : { TEXT("shade"), TEXT("wooded"), TEXT("metres"), TEXT("dry"), TEXT("sand") })
				{
					for (int32 J = ProbeJ - 9; J <= ProbeJ + 9; ++J)
					{
						FString Row;
						for (int32 I = ProbeI - 11; I <= ProbeI + 11; ++I)
						{
							const int32 Idx = Terrain.Index(I, J);
							const FString W(What);
							Row += FString::Printf(TEXT("%4d"), W == TEXT("shade") ? Relief[Idx].B : W == TEXT("wooded") ? Ground[Idx].A : W == TEXT("metres") ? FMath::RoundToInt32(Metres[Idx]) : W == TEXT("dry") ? Masks[Idx].A : Masks[Idx].B);
						}
						UE_LOG(LogTheater, Display, TEXT("Probe %s y %.0f: %s"), What, Terrain.OriginY + J * Terrain.Cell, *Row);
					}
				}
			}
		}
	}
	if (bWaters && WetScale > 1)
	{
		// (the waters' picture at its own finer points: each takes its country's floor for a water's breadth, and
		// how far it is beyond Europe, from the point of the ground it lies nearest)
		ParallelFor(WetNY, [&](int32 L)
		{
			const int32 J = FMath::Clamp(FMath::RoundToInt32((L + 0.5) / WetScale - 0.5), 0, NY - 1);
			for (int32 K = 0; K < WetNX; ++K)
			{
				const int32 I = FMath::Clamp(FMath::RoundToInt32((K + 0.5) / WetScale - 0.5), 0, NX - 1);
				const int32 Idx = Terrain.Index(I, J), At = L * WetNX + K;
				const float Beyond = Outside[Idx];
				if (Beyond <= 0.0f)
				{
					continue;
				}
				float Half = FinerHalf[At];
				if (Half > 0.0f && RiverWater[Idx] >= 0.0f)
				{
					Half = FMath::Max(Half, HalfFloor[Idx]);
				}
				Half *= FMath::SmoothStep(0.25f, 0.7f, Beyond);
				Wet[At] = FColor(ToByte(0.5f + 0.5f * FMath::Clamp(FinerDx[At] / WetReach, -1.0f, 1.0f)), ToByte(0.5f + 0.5f * FMath::Clamp(FinerDy[At] / WetReach, -1.0f, 1.0f)),
					ToByte(FMath::Min(FinerAt[At] / WetReach, 1.0f)), ToByte(Half / 2.5f));
			}
		}, TheaterSpeed::ModelLoop());
	}

	if (Terrain.Cell > 3.0)
	{
		// (On a coarse sheet a patch of rock is a few cells, and from high up showed as rectangles: the rock is
		// blended across its cells.)
		TArray<float> Stone;
		Stone.SetNumUninitialized(Count);
		for (int32 Idx = 0; Idx < Count; ++Idx)
		{
			Stone[Idx] = Masks[Idx].R;
		}
		FCampaignTerrain::Smooth(Stone, NX, NY, 1, 2);
		for (int32 Idx = 0; Idx < Count; ++Idx)
		{
			Masks[Idx].R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Stone[Idx]), 0, 255));
		}
		// (and so is the shading of the hills, which showed its cells as squares on the great ranges)
		for (int32 Part = 0; Part < 3; ++Part)
		{
			for (int32 Idx = 0; Idx < Count; ++Idx)
			{
				Stone[Idx] = Part == 0 ? Relief[Idx].R : Part == 1 ? Relief[Idx].G : Relief[Idx].B;
			}
			FCampaignTerrain::Smooth(Stone, NX, NY, 1, 1);
			for (int32 Idx = 0; Idx < Count; ++Idx)
			{
				const uint8 Value = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Stone[Idx]), 0, 255));
				(Part == 0 ? Relief[Idx].R : Part == 1 ? Relief[Idx].G : Relief[Idx].B) = Value;
			}
		}
	}

	if (bBeyond && Terrain.Cell <= 3.0)
	{
		// (On a fine sheet beyond Europe the heights are known only every two map units, less finely than the sheet
		// is painted: the shading of a range changed from lit to shaded within a cell, along edges that ran in
		// straight steps. It is softened by a cell, as far as the point lies beyond Europe.)
		TArray<float> Soft;
		Soft.SetNumUninitialized(Count);
		for (int32 Part = 0; Part < 3; ++Part)
		{
			for (int32 Idx = 0; Idx < Count; ++Idx)
			{
				Soft[Idx] = Part == 0 ? Relief[Idx].R : Part == 1 ? Relief[Idx].G : Relief[Idx].B;
			}
			FCampaignTerrain::Smooth(Soft, NX, NY, 1, 1);
			for (int32 Idx = 0; Idx < Count; ++Idx)
			{
				uint8& Kept = Part == 0 ? Relief[Idx].R : Part == 1 ? Relief[Idx].G : Relief[Idx].B;
				// (by half: softened wholly, the ranges lost some of their bite)
				Kept = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(FMath::Lerp(static_cast<float>(Kept), Soft[Idx], 0.5f * Outside[Idx])), 0, 255));
			}
		}
	}

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
			if (Beyond > 0.0f && LookDryPlain.IsValidIndex(Idx) && (LookDryPlain[Idx] > 0.0f || (LookSquare.IsValidIndex(Idx) && LookSquare[Idx] != 0)))
			{
				// (The art director's ruling of 19 November 2026 on American and Canadian farmland: "lay fields only on
				// flat or gentle farmland, never on hills, mountains, dry range, desert or forest. In Idaho that means
				// only the Snake River Plain; the hills stay bare range." How far the ground rises and falls among a
				// point and its eight neighbours says which it is: farmland to about 60 metres, hill past about 180. And
				// the dry plain's own share of the look keeps only a part of its fields.
				// "-TheaterFieldsRough=60,180,0.3" tries other numbers: gentle, hill, and the part dry country keeps.)
				static const FVector FieldsRough = []
				{
					FVector Asked(60.0, 180.0, 0.3);
					FString Text;
					TArray<FString> Parts;
					if (FParse::Value(FCommandLine::Get(), TEXT("TheaterFieldsRough="), Text, false) && Text.ParseIntoArray(Parts, TEXT(",")) >= 2)
					{
						Asked.X = FCString::Atod(*Parts[0]);
						Asked.Y = FMath::Max(FCString::Atod(*Parts[1]), Asked.X + 1.0);
						if (Parts.Num() >= 3)
						{
							Asked.Z = FMath::Clamp(FCString::Atod(*Parts[2]), 0.0, 1.0);
						}
					}
					return Asked;
				}();
				float Lowest = Metres[Idx], Highest = Metres[Idx];
				for (int32 Dj = -1; Dj <= 1; ++Dj)
				{
					for (int32 Di = -1; Di <= 1; ++Di)
					{
						const float There = Metres[Terrain.Index(FMath::Clamp(I + Di, 0, NX - 1), FMath::Clamp(J + Dj, 0, NY - 1))];
						Lowest = FMath::Min(Lowest, There);
						Highest = FMath::Max(Highest, There);
					}
				}
				const float Hilly = FMath::SmoothStep(static_cast<float>(FieldsRough.X), static_cast<float>(FieldsRough.Y), Highest - Lowest);
				Arable *= FMath::Lerp(1.0f, (1.0f - Hilly) * FMath::Lerp(1.0f, static_cast<float>(FieldsRough.Z), FMath::Clamp(LookDryPlain[Idx], 0.0f, 1.0f)), Beyond);
			}
			if (bWaters && Beyond > 0.0f)
			{
				// (no fields in a river, in a lake or on a clearing)
				Arable *= (1.0f - Opened[Idx]) * (RiverHalf[Idx] > 0.0f ? FMath::SmoothStep(RiverHalf[Idx], RiverHalf[Idx] + 0.8f, RiverAt[Idx]) : 1.0f);
			}
			Arable = FMath::Clamp(Arable, 0.0f, 1.0f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);
			// Small fields and heavy hedges in the Atlantic west, great open ones on the eastern plains.
			float Big = FMath::Clamp(FMath::SmoothStep(11.0f, 27.0f, Longitude) + 0.3f * FMath::PerlinNoise2D(P * 0.011 + FVector2D(5.0, 23.0)), 0.0f, 1.0f);
			if (Beyond > 0.0f)
			{
				Big = FMath::Lerp(Big, LookBig[Idx], Beyond); // (the largest on the prairie, the smallest in rice country)
			}
			// (The art director's review of build 1608: where the fields beyond Europe are not squares they were read
			// half on the small fields' grid and half on the great ones', two patterns laid over each other, and came
			// out as soft streaks. They are whole plots on one grid: the great one, about thirty-seven dots by twelve
			// from playing height; rice country keeps the small one, its paddies. ("-TheaterOldFields": as before.))
			static const bool bWholePlots = !FParse::Param(FCommandLine::Get(), TEXT("TheaterOldFields"));
			if (bWholePlots && Beyond > 0.5f && !(LookSquare.IsValidIndex(Idx) && LookSquare[Idx] != 0))
			{
				Big = LookRice[Idx] > 0.5f ? 0.0f : 1.0f;
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
	}, TheaterSpeed::ModelLoop());

	// How the fields lie in each province: turned by an angle of the province's own, and a little larger or smaller.
	Lie.SetNumUninitialized(Count);
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int32 Id = Terrain.Region[Idx] >= 0 ? Terrain.Region[Idx] : ShoreRegion[Idx];
		// (the picture's alpha: 1 where the woods are Europe's, less where their canopy is the tropics')
		// (One number carries both: from 1 down to a half it is how far the canopy is the tropics', from a half down to
		// nought how far it is the northern forest's. The two are never found together.)
		const uint8 NotTropic = !bBeyond ? 255 : LookNorth[Idx] > LookTropic[Idx] ? ToByte(0.49f - 0.49f * LookNorth[Idx]) : ToByte(1.0f - 0.49f * LookTropic[Idx]);
		if (Id < 0 || (Id >= Data.FirstBeyond && Data.FirstBeyond != INDEX_NONE))
		{
			// (and a province of the wider chart, whose fields lie as its country's do: the square mile of the prairie
			// is not turned to an angle of each state's own)
			// (Land nobody holds: its fields lie square on the compass where they are the New World's, and at a
			// slant of twenty degrees elsewhere. "No turn at all", to the last figure, is what tells the land's
			// paint to lay squares.)
			const bool bSquare = LookSquare.IsValidIndex(Idx) && LookSquare[Idx] != 0;
			// (Item 2 again: where the fields are not squares, a province beyond Europe turns its fields by an angle of
			// its own and makes them a little larger or smaller than its neighbour's, as every province of Europe does.
			// They all lay at one slant of twenty degrees, and the land read as one ruled board from Maine to Chile.)
			static const bool bOldLie = FParse::Param(FCommandLine::Get(), TEXT("TheaterOldFields"));
			if (!bSquare && Id >= 0 && bBeyond && !bOldLie)
			{
				const uint32 Own = static_cast<uint32>(Id) * 2654435761u;
				const float Turn = ((Own >> 8) & 0xFFFF) / 65535.0f * UE_PI;
				const float Size = 0.5f + 0.5f * ((((Own >> 3) & 0xFF) / 255.0f) * 2.0f - 1.0f) * 0.6f;
				Lie[Idx] = FColor(ToByte(0.5f + 0.5f * FMath::Cos(Turn)), ToByte(0.5f + 0.5f * FMath::Sin(Turn)), ToByte(Size), NotTropic);
				if (Lie[Idx].R == 255 && Lie[Idx].G == 128)
				{
					Lie[Idx].G = 129; // (exactly no turn is the sign for squares)
				}
				continue;
			}
			// (a square's blue is not its size, which is fixed: it is how wholly the country is square-field country)
			Lie[Idx] = bSquare ? FColor(255, 128, LookSquare[Idx], NotTropic) : (bBeyond ? FColor(247, 171, 128, NotTropic) : FColor(255, 129, 128, NotTropic));
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
			const float SeaBeyond = bBeyond ? BeyondEuropeBy(SeaLatitude, static_cast<float>(Data.LongitudeAt(X)), 6.0f) : 0.0f; // (the world's seas come in over six degrees from Europe's sheet: over three, the Barents Sea stepped from Europe's turquoise to the world's slate)
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
				// (The cold seas' own grey comes in over the last six degrees toward Europe's sheet, not three: Europe's
				// northern shelf is the turquoise it was accepted with, and beside it the world's steel was a step.)
				const float ColdBeyond = bBeyond ? BeyondEuropeBy(SeaLatitude, static_cast<float>(Data.LongitudeAt(X)), 6.0f) : 0.0f;
				C = Mix(C, FSeen::Bytes(62, 88, 100), 0.5f * FMath::SmoothStep(55.0f, 66.0f, SeaLatitude) * ColdBeyond);
				// (and the shelves of the far north are a steel blue-grey, not the south's turquoise: the Bering Sea)
				// (the same in the far south, from about 50 degrees: Cape Horn and the Falklands)
				const float ColdShelf = FMath::Max(FMath::SmoothStep(51.0f, 57.0f, SeaLatitude), FMath::SmoothStep(46.0f, 52.0f, -SeaLatitude));
				C = Mix(C, FSeen::Bytes(70, 102, 116), 0.85f * ColdShelf * (1.0f - FMath::SmoothStep(0.18f, 0.5f, Deep)) * ColdBeyond);
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
	}, TheaterSpeed::ModelLoop());
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
			}, TheaterSpeed::ModelLoop());
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------
// The ownership paint: how far two colours differ to the eye, the fronts a colour comes in by, and the stripes.

namespace
{
	/** A colour as it is seen (0 to 1), in the measure the difference between two colours is taken in. */
	FVector3f ToLab(const FSeen& C)
	{
		auto Linear = [](float U) { return U <= 0.04045f ? U / 12.92f : FMath::Pow((U + 0.055f) / 1.055f, 2.4f); };
		const float R = Linear(C.R), G = Linear(C.G), B = Linear(C.B);
		const float X = (0.4124f * R + 0.3576f * G + 0.1805f * B) / 0.95047f, Y = 0.2126f * R + 0.7152f * G + 0.0722f * B, Z = (0.0193f * R + 0.1192f * G + 0.9505f * B) / 1.08883f;
		auto Curve = [](float T) { return T > 0.008856f ? FMath::Pow(T, 1.0f / 3.0f) : 7.787f * T + 16.0f / 116.0f; };
		return FVector3f(116.0f * Curve(Y) - 16.0f, 500.0f * (Curve(X) - Curve(Y)), 200.0f * (Curve(Y) - Curve(Z)));
	}

	/** How far two colours differ to the eye (CIEDE2000): about 10 reads at a glance. */
	float Differs(const FSeen& One, const FSeen& Other)
	{
		const FVector3f A = ToLab(One), B = ToLab(Other);
		const float C1 = FMath::Sqrt(A.Y * A.Y + A.Z * A.Z), C2 = FMath::Sqrt(B.Y * B.Y + B.Z * B.Z), Cb = 0.5f * (C1 + C2);
		const float Cb7 = FMath::Pow(Cb, 7.0f), G = 0.5f * (1.0f - FMath::Sqrt(Cb7 / (Cb7 + 6103515625.0f)));
		const float A1 = (1.0f + G) * A.Y, A2 = (1.0f + G) * B.Y;
		const float Cp1 = FMath::Sqrt(A1 * A1 + A.Z * A.Z), Cp2 = FMath::Sqrt(A2 * A2 + B.Z * B.Z);
		auto Hue = [](float Bb, float Aa) { float H = FMath::RadiansToDegrees(FMath::Atan2(Bb, Aa)); return H < 0.0f ? H + 360.0f : H; };
		const float H1 = Hue(A.Z, A1), H2 = Hue(B.Z, A2);
		const float DL = B.X - A.X, DC = Cp2 - Cp1;
		float Dh = H2 - H1;
		if (Cp1 * Cp2 == 0.0f) { Dh = 0.0f; }
		else if (Dh > 180.0f) { Dh -= 360.0f; }
		else if (Dh < -180.0f) { Dh += 360.0f; }
		const float DH = 2.0f * FMath::Sqrt(Cp1 * Cp2) * FMath::Sin(FMath::DegreesToRadians(Dh * 0.5f));
		const float Lb = 0.5f * (A.X + B.X), Cpb = 0.5f * (Cp1 + Cp2);
		float Hb = H1 + H2;
		if (Cp1 * Cp2 != 0.0f) { Hb = FMath::Abs(H1 - H2) <= 180.0f ? 0.5f * (H1 + H2) : (H1 + H2 < 360.0f ? 0.5f * (H1 + H2 + 360.0f) : 0.5f * (H1 + H2 - 360.0f)); }
		const float T = 1.0f - 0.17f * FMath::Cos(FMath::DegreesToRadians(Hb - 30.0f)) + 0.24f * FMath::Cos(FMath::DegreesToRadians(2.0f * Hb))
			+ 0.32f * FMath::Cos(FMath::DegreesToRadians(3.0f * Hb + 6.0f)) - 0.2f * FMath::Cos(FMath::DegreesToRadians(4.0f * Hb - 63.0f));
		const float Turn = 30.0f * FMath::Exp(-FMath::Square((Hb - 275.0f) / 25.0f)), Cpb7 = FMath::Pow(Cpb, 7.0f), Rc = 2.0f * FMath::Sqrt(Cpb7 / (Cpb7 + 6103515625.0f));
		const float Sl = 1.0f + 0.015f * FMath::Square(Lb - 50.0f) / FMath::Sqrt(20.0f + FMath::Square(Lb - 50.0f)), Sc = 1.0f + 0.045f * Cpb, Sh = 1.0f + 0.015f * Cpb * T;
		const float Rt = -FMath::Sin(FMath::DegreesToRadians(2.0f * Turn)) * Rc;
		return FMath::Sqrt(FMath::Square(DL / Sl) + FMath::Square(DC / Sc) + FMath::Square(DH / Sh) + Rt * (DC / Sc) * (DH / Sh));
	}

	/**
	 * The stripe's colour for a holder over a lawful owner, as a share of the way from the holder's stain to the
	 * owner's (the art director's brief: 0.55 to begin with; less for a pair that comes out louder than 12 on open
	 * lowland by day; and where the two are too alike to reach 8 even at 0.6, the answer is less than nothing, which
	 * says "the holder's own stain at four fifths of its brightness").
	 */
	float StripeMix(const FSeen& Holder, const FSeen& Owner, float PlainsLight, float Snow)
	{
		// (Open lowland by day, as the game's own pictures show it. Measured with the stripes and without, on
		// Germany's field grey over France, Belgium and the Netherlands: the ground between stripes is about the
		// holder's stain laid 0.45 over a green of (100, 128, 96), and a stripe differs from it by the difference
		// of the two stains, at about two thirds of it in the red and the green and 0.85 of it in the blue. A
		// reckoning from the paint alone, as this first was, came out a third too quiet for a warm owner and not
		// far off half too quiet for a blue one.)
		const FSeen Between = Mix(FSeen::Bytes(100, 128, 96), Holder, 0.45f);
		// (The art director's round 28: under snow a stripe went quiet. Measured, Germany over Poland fell from 9.8 in
		// October to 7.2 in January at the same mix, and nothing showed at Smolensk in January 1942, where what could
		// be liberated matters most. Snow takes the stripe and the ground between both toward its own white, by
		// about three tenths in deep winter: the reckoning is made as the ground will be seen.)
		const FSeen SnowSeen = FSeen::Bytes(236, 232, 220);
		auto AtUnder = [&](float Share, float Lying)
		{
			const FSeen Stripe(FMath::Clamp(Between.R + 0.67f * Share * (Owner.R - Holder.R), 0.0f, 1.0f), FMath::Clamp(Between.G + 0.62f * Share * (Owner.G - Holder.G), 0.0f, 1.0f),
				FMath::Clamp(Between.B + 0.85f * Share * (Owner.B - Holder.B), 0.0f, 1.0f));
			return Differs(Mix(Stripe, SnowSeen, Lying), Mix(Between, SnowSeen, Lying));
		};
		auto At = [&](float Share) { return AtUnder(Share, Snow); };
		// (Two nations of one hue are too alike however the numbers fall: the art director names the Soviet Union over
		// Poland, Britain over the Soviet Union. In the game's pictures a stripe of Poland's red on the Soviet Union's
		// red measured under 4, where the reckoning gave it 10.)
		const FVector3f HolderLab = ToLab(Holder), OwnerLab = ToLab(Owner);
		const float HolderChroma = FMath::Sqrt(HolderLab.Y * HolderLab.Y + HolderLab.Z * HolderLab.Z), OwnerChroma = FMath::Sqrt(OwnerLab.Y * OwnerLab.Y + OwnerLab.Z * OwnerLab.Z);
		const float HueApart = FMath::Abs(FMath::FindDeltaAngleDegrees(FMath::RadiansToDegrees(FMath::Atan2(HolderLab.Z, HolderLab.Y)), FMath::RadiansToDegrees(FMath::Atan2(OwnerLab.Z, OwnerLab.Y))));
		// (whether two nations are too alike is asked of the bare ground: it is not for the season to decide)
		if (AtUnder(0.6f, 0.0f) < 8.0f || (HolderChroma > 18.0f && OwnerChroma > 18.0f && HueApart < 30.0f))
		{
			return -1.0f;
		}
		// (The art director's round 29: under full snow a stripe at the cap read like a venetian blind: flat pale
		// snow shows a stripe more than a quilt of fields does. She asked for about 8.5 there by the pictures, never
		// above 9, and no more than 0.06 darker than the snow beside it: warmer, not darker. That is done in the
		// land's paint, point by point where the snow lies whole: the snow lies less thin on a stripe there, and
		// the stripe's colour is carried further from its holder's. This reckoning has little hold on it: under
		// whole snow most of a point's colour is the snow's, and the mix moved the measure by a few tenths.)
		const float Cap = 10.5f;
		float Share = 0.55f;
		if (Snow > 0.0f && At(Share) < Cap)
		{
			// (under snow it may take more of the old owner's colour than the 0.55 the brief begins with, up to
			// 0.85: the stripe is two fifths of the ground, so the holder's shade is still the main colour)
			float Low = Share, High = 0.85f;
			for (int32 Pass = 0; Pass < 12; ++Pass)
			{
				const float Middle = 0.5f * (Low + High);
				(At(Middle) > Cap ? High : Low) = Middle;
			}
			return High;
		}
		if (At(Share) > 12.0f)
		{
			// (down to where it is about the cap)
			float Low = 0.05f, High = Share;
			for (int32 Pass = 0; Pass < 12; ++Pass)
			{
				const float Middle = 0.5f * (Low + High);
				(At(Middle) > Cap ? High : Low) = Middle;
			}
			Share = Low;
		}
		else if (At(Share) < 8.0f)
		{
			Share = 0.6f;
		}
		return Share;
	}

	/** How the edge of a siege's colour or of spreading ink wanders, in map units either way: the same for a place every time. */
	float Wander(const FVector2D& P)
	{
		// (up to nine pixels either way at working height, on a slow grain of about forty: 2.5 map units on eleven)
		return 2.5f * FMath::Clamp(1.6f * FMath::PerlinNoise2D(P / 11.0 + FVector2D(37.0, 91.0)) + 0.5f * FMath::PerlinNoise2D(P / 4.3 + FVector2D(5.0, 13.0)), -1.0f, 1.0f);
	}
}

const FCampaignGround::FFront& FCampaignGround::FrontOf(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, int32 Region, const TArray<int32>& From, bool bEverySide)
{
	uint32 Key = HashCombineFast(GetTypeHash(Region), GetTypeHash(bEverySide));
	for (const int32 Side : From)
	{
		Key = HashCombineFast(Key, GetTypeHash(Side));
	}
	if (const TSharedPtr<FFront>* Kept = Fronts.Find(Key))
	{
		return **Kept;
	}
	FFront& Front = *Fronts.Add(Key, MakeShared<FFront>());
	if (!Data.Regions.IsValidIndex(Region) || !Data.Regions[Region].Bounds.bIsValid)
	{
		return Front;
	}
	const FBox2D& Box = Data.Regions[Region].Bounds;
	const double Cell = Terrain.Cell;
	Front.I0 = FMath::Clamp(FMath::FloorToInt32((Box.Min.X - Terrain.OriginX) / Cell) - 1, 0, Terrain.NX - 1);
	Front.J0 = FMath::Clamp(FMath::FloorToInt32((Box.Min.Y - Terrain.OriginY) / Cell) - 1, 0, Terrain.NY - 1);
	Front.NI = FMath::Clamp(FMath::CeilToInt32((Box.Max.X - Terrain.OriginX) / Cell) + 1, 0, Terrain.NX - 1) - Front.I0 + 1;
	Front.NJ = FMath::Clamp(FMath::CeilToInt32((Box.Max.Y - Terrain.OriginY) / Cell) + 1, 0, Terrain.NY - 1) - Front.J0 + 1;
	Front.Far.Init(-1.0f, Front.NI * Front.NJ);
	// The province's own points, and those of them the colour starts from: the ones beside a point of a province it
	// comes from; or beside any ground that is not this province's; or, with no such side, beside the sea; or, with
	// no coast either, the point nearest the town.
	auto Own = [&](int32 I, int32 J) { return I >= 0 && J >= 0 && I < Terrain.NX && J < Terrain.NY && Terrain.Region[Terrain.Index(I, J)] == Region; };
	const TSet<int32> Sides(From);
	TArray<int32> Start;
	for (int32 Way = 0; Way < 3 && Start.Num() == 0; ++Way)
	{
		for (int32 J = 0; J < Front.NJ; ++J)
		{
			for (int32 I = 0; I < Front.NI; ++I)
			{
				const int32 GI = Front.I0 + I, GJ = Front.J0 + J;
				if (!Own(GI, GJ))
				{
					continue;
				}
				bool bStarts = false;
				static const int32 Dx[4] = { 1, -1, 0, 0 }, Dy[4] = { 0, 0, 1, -1 };
				for (int32 K = 0; K < 4 && !bStarts; ++K)
				{
					const int32 NI = GI + Dx[K], NJ = GJ + Dy[K];
					if (NI < 0 || NJ < 0 || NI >= Terrain.NX || NJ >= Terrain.NY)
					{
						continue;
					}
					const int16 There = Terrain.Region[Terrain.Index(NI, NJ)];
					bStarts = Way == 0 ? (bEverySide ? There != Region : Sides.Contains(There)) : Way == 1 ? There == FCampaignTerrain::Sea : false;
				}
				if (bStarts)
				{
					Start.Add(J * Front.NI + I);
				}
			}
		}
		if (Way == 0 && !bEverySide && Sides.Num() == 0)
		{
			continue; // (no side named: the coast, then the town)
		}
	}
	if (Start.Num() == 0)
	{
		// (the town: the province's point nearest its middle)
		const FVector2D Town = Data.Regions[Region].Center;
		int32 Best = INDEX_NONE;
		double Nearest = TNumericLimits<double>::Max();
		for (int32 J = 0; J < Front.NJ; ++J)
		{
			for (int32 I = 0; I < Front.NI; ++I)
			{
				if (Own(Front.I0 + I, Front.J0 + J))
				{
					const double There = FVector2D::DistSquared(Terrain.PointAt(Front.I0 + I, Front.J0 + J), Town);
					if (There < Nearest)
					{
						Nearest = There;
						Best = J * Front.NI + I;
					}
				}
			}
		}
		if (Best != INDEX_NONE)
		{
			Start.Add(Best);
		}
	}
	// Outward from those points through the province, by the shortest way: each point's distance in, in map units.
	TArray<TPair<float, int32>> Heap;
	auto Less = [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; };
	for (const int32 At : Start)
	{
		Front.Far[At] = 0.0f;
		Heap.HeapPush(TPair<float, int32>(0.0f, At), Less);
	}
	float Furthest = 0.0f;
	while (Heap.Num() > 0)
	{
		TPair<float, int32> Top;
		Heap.HeapPop(Top, Less, EAllowShrinking::No);
		if (Top.Key > Front.Far[Top.Value] + 1.0e-4f)
		{
			continue;
		}
		Furthest = FMath::Max(Furthest, Top.Key);
		const int32 I = Top.Value % Front.NI, J = Top.Value / Front.NI;
		for (int32 Dj = -1; Dj <= 1; ++Dj)
		{
			for (int32 Di = -1; Di <= 1; ++Di)
			{
				const int32 NI = I + Di, NJ = J + Dj;
				if ((Di == 0 && Dj == 0) || NI < 0 || NJ < 0 || NI >= Front.NI || NJ >= Front.NJ || !Own(Front.I0 + NI, Front.J0 + NJ))
				{
					continue;
				}
				const float Step = static_cast<float>(Cell) * (Di != 0 && Dj != 0 ? 1.41421f : 1.0f), There = Top.Key + Step;
				float& Known = Front.Far[NJ * Front.NI + NI];
				if (Known < 0.0f || There < Known - 1.0e-4f)
				{
					Known = There;
					Heap.HeapPush(TPair<float, int32>(There, NJ * Front.NI + NI), Less);
				}
			}
		}
	}
	Front.Reach = FMath::Max(Furthest, static_cast<float>(Cell));
	return Front;
}

float FCampaignGround::BehindFront(const FFront& Front, const FCampaignTerrain& Terrain, int32 I, int32 J, float Share)
{
	const int32 Li = I - Front.I0, Lj = J - Front.J0;
	if (Li < 0 || Lj < 0 || Li >= Front.NI || Lj >= Front.NJ)
	{
		return -1000.0f;
	}
	const float Far = Front.Far[Lj * Front.NI + Li];
	if (Far < 0.0f)
	{
		return -1000.0f;
	}
	// (the front stands a little beyond the far side when it has come the whole way, so that its wandering edge leaves nothing uncovered)
	return Share * (Front.Reach + 6.0f) - 3.0f * Share - (Far + Wander(Terrain.PointAt(I, J)) * FMath::Min(1.0f, Far / 2.5f));
}

float FCampaignGround::FrontCover(const FFront& Front, const FCampaignTerrain& Terrain, int32 I, int32 J, float Share)
{
	// (soft over ten pixels at working height: 2.8 map units)
	return Share <= 0.0f ? 0.0f : FMath::SmoothStep(-1.4f, 1.4f, BehindFront(Front, Terrain, I, J, Share));
}

int32 FCampaignGround::PaintedRegion(int32 Index, const FCampaignTerrain& Terrain) const
{
	return ShoreRegion.IsValidIndex(Index) ? ShoreRegion[Index] : (Terrain.Region.IsValidIndex(Index) ? Terrain.Region[Index] : -1);
}

void FCampaignGround::PaintTint(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, int32 I0, int32 J0, int32 I1, int32 J1)
{
	// (Speed: a fine sheet's thread reads whose each province is here while ground may change hands: held until the
	// two tables of it below are made, and no longer.)
	TheaterSpeed::FHoldersRead Reading;
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
		// (The art director's round 31, "blue beside gold".) Beyond Europe, ground held under a blue or violet paint
		// is marked for the material by a half added to its boost (see the land's material: its ripe wheat, its trees
		// and its palest ground take no wash). Argentina's and Uruguay's wash is 1.35 times the common one, by her ruling.
		if (Data.FirstBeyond != INDEX_NONE && Region.Id >= Data.FirstBeyond && IsCoolPaint(Colour))
		{
			const bool bThick = Region.Owner == TEXT("ARG") || Region.Owner == TEXT("URU");
			RegionBoost[Region.Id] = 1.0f + (bThick ? 0.35f : RegionBoost[Region.Id] - 1.0f) + 0.5f;
		}
	}
	// (The ownership paint.) Occupied ground: the stripe's colour, for each province whose holder is not its lawful
	// owner. And the sieges: the besieger's stain, and how far each point of the field lies from the side it comes by.
	auto StainOf = [&Data](const FString& Tag)
	{
		const FCampaignNation* Nation = Data.FindNation(Tag);
		const FColor Colour = Nation ? Nation->Color : FColor(150, 150, 150);
		return FSeen::Bytes(Colour.R, Colour.G, Colour.B);
	};
	TArray<FSeen> RegionStripe;
	TArray<uint8> RegionOccupied;
	RegionStripe.SetNum(Data.Regions.Num());
	RegionOccupied.Init(0, Data.Regions.Num());
	TMap<FString, float> Mixes;
	for (const FCampaignRegion& Region : Data.Regions)
	{
		if (Region.Lawful.IsEmpty() || Region.Lawful == Region.Owner)
		{
			continue;
		}
		const FSeen Owner = StainOf(Region.Lawful);
		const float Snow = RegionSnow.IsValidIndex(Region.Id) ? FMath::RoundToFloat(RegionSnow[Region.Id] * 20.0f) / 20.0f : 0.0f;
		const FString Pair = FString::Printf(TEXT("%s>%s:%d"), *Region.Owner, *Region.Lawful, FMath::RoundToInt32(Snow * 20.0f));
		float* Known = Mixes.Find(Pair);
		const float Share = Known ? *Known : Mixes.Add(Pair, StripeMix(RegionStain[Region.Id], Owner, PlainsLightness, Snow));
		if (!Known)
		{
			const FSeen& Held = RegionStain[Region.Id];
			UE_LOG(LogTheater, Log, TEXT("Stripes: %s holding %s: the holder's stain (%.0f, %.0f, %.0f), the owner's (%.0f, %.0f, %.0f), mixed %.2f toward the owner (snow %.2f)"), *Region.Owner, *Region.Lawful,
				Held.R * 255.0f, Held.G * 255.0f, Held.B * 255.0f, Owner.R * 255.0f, Owner.G * 255.0f, Owner.B * 255.0f, Share, Snow);
		}
		// (two nations too alike for a stripe of the one to show on the other: the holder's own stain, a fifth darker)
		// (too alike to tell apart: the holder's own stain, darker. At four fifths of its brightness, as the brief
		// began, the Soviet Union over Poland measured 5.4 in the game's pictures, under the 8 asked: seven tenths.)
		// (and darker still under snow, which lifts it)
		RegionStripe[Region.Id] = Share < 0.0f ? RegionStain[Region.Id] * (0.68f - 0.2f * Snow) : Mix(RegionStain[Region.Id], Owner, Share);
		RegionOccupied[Region.Id] = 1;
		// ("-TheaterStripeLog" says what each occupied province was given)
		static const bool bSayEach = FParse::Param(FCommandLine::Get(), TEXT("TheaterStripeLog"));
		if (bSayEach)
		{
			UE_LOG(LogTheater, Display, TEXT("Stripe of %s (%s holding %s): snow %.2f, mixed %.2f"), *Region.Name, *Region.Owner, *Region.Lawful, Snow, Share);
		}
	}
	Reading.Release();
	if (Occupied.Num() != Tint.Num())
	{
		Occupied.Init(FColor(0, 0, 0, 0), Tint.Num());
	}
	TArray<int32> SiegeOf;
	SiegeOf.Init(INDEX_NONE, Data.Regions.Num());
	TArray<const FFront*> SiegeFront;
	TArray<FSeen> SiegeStain;
	for (int32 K = 0; K < Sieges.Num(); ++K)
	{
		if (Data.Regions.IsValidIndex(Sieges[K].Region))
		{
			SiegeOf[Sieges[K].Region] = K;
		}
		SiegeFront.Add(&FrontOf(Data, Terrain, Sieges[K].Region, Sieges[K].From, Sieges[K].bEverySide));
		SiegeStain.Add(StainOf(Sieges[K].By));
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
				const float Shade = 0.42f + 1.05f * Bare.Light();
				C = RegionStain[Id] * Shade;
				const double Far = Terrain.BorderDistance[Idx] * Terrain.Cell;
				Band = Terrain.Region[Idx] >= 0 ? FMath::Pow(FMath::Clamp(1.0f - static_cast<float>(Far / BandReach), 0.0f, 1.0f), 1.7f) : 0.0f;
				C = C * (1.0f - 0.2f * Band);
				Boost = RegionBoost[Id] - 1.0f;
				// (A siege: the besieger's plain stain over the field, from the sides his side holds, as deep as the
				// siege has gone. The town keeps an island of its holder's colour to the end. Soft-edged, and no line.)
				float Besieged = 0.0f;
				if (SiegeOf[Id] != INDEX_NONE && Terrain.Region[Idx] == Id)
				{
					const FSiege& Siege = Sieges[SiegeOf[Id]];
					const float Out = static_cast<float>(FVector2D::Distance(Terrain.PointAt(I, J), Siege.Town));
					Besieged = FrontCover(*SiegeFront[SiegeOf[Id]], Terrain, I, J, Siege.Depth) * FMath::SmoothStep(Siege.Island - 1.4f, Siege.Island + 1.4f, Out + 0.6f * Wander(Terrain.PointAt(I, J)));
					C = Mix(C, SiegeStain[SiegeOf[Id]] * Shade * (1.0f - 0.2f * Band), Besieged);
					Boost = FMath::Lerp(Boost, WashBoost(FColor(ToByte(SiegeStain[SiegeOf[Id]].R), ToByte(SiegeStain[SiegeOf[Id]].G), ToByte(SiegeStain[SiegeOf[Id]].B))) - 1.0f, Besieged);
				}
				// (Occupied ground: the stripe's colour, made as the stain is; none within about three map units of a
				// national frontier, eleven pixels at working height, so that the frontier's band stays clean; and none
				// under a besieger's colour, which is plain.)
				if (RegionOccupied[Id])
				{
					const FSeen Stripe = RegionStripe[Id] * Shade * (1.0f - 0.2f * Band);
					const float Clear = Terrain.Region[Idx] >= 0 ? FMath::SmoothStep(0.4f, 3.5f, static_cast<float>(Far)) : 0.0f;
					Occupied[Idx] = FColor(ToByte(Stripe.R * Toned.X), ToByte(Stripe.G * Toned.Y), ToByte(Stripe.B * Toned.Z), ToByte(Clear * (1.0f - Besieged)));
				}
				else
				{
					Occupied[Idx] = FColor(ToByte(C.R * Toned.X), ToByte(C.G * Toned.Y), ToByte(C.B * Toned.Z), 0);
				}
			}
			else
			{
				Occupied[Idx] = FColor(0, 0, 0, 0);
			}
			Tint[Idx] = FColor(ToByte(C.R * Toned.X), ToByte(C.G * Toned.Y), ToByte(C.B * Toned.Z), ToByte(Band));
			if (Relief.IsValidIndex(Idx))
			{
				Relief[Idx].A = ToByte(Boost);
			}
		}
	}
}
