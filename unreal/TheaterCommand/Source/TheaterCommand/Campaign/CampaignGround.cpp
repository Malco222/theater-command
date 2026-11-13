#include "Campaign/CampaignGround.h"

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
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int16 Id = Terrain.Region[Idx];
		Woods[Idx] = Id >= 0 ? RegionWoods[Id] : -0.07f;
		Dry[Idx] = Id >= 0 ? RegionDry[Id] : 0.0f;
		Marsh[Idx] = Id >= 0 && Data.Regions[Id].Terrain == TEXT("marsh") ? 1.0f : 0.0f;
	}
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
			const FVector2D P(I * Terrain.Cell, J * Terrain.Cell);
			const float Metres = Terrain.HasElevation() ? FMath::Max(Terrain.MetresAt(P.X, P.Y), 0.0f) : FMath::Max(Terrain.Height[Idx], 0.0f) * 380.0f;
			// (the map is laid out forty units to the degree of latitude, its top edge a little north of the North Cape)
			const float Latitude = 71.4f - static_cast<float>(P.Y) / 40.0f, Longitude = -11.0f + static_cast<float>(P.X) / 25.7f;
			const float Broad = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.02), Middling = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.088 + FVector2D(7.0, 3.0));
			const float Fine = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.6 + FVector2D(11.0, 5.0));
			const float DryCountry = FMath::Clamp(FMath::SmoothStep(0.0f, 8.0f, 46.0f - Latitude) * 0.8f + FMath::SmoothStep(28.0f, 44.0f, Longitude) * FMath::SmoothStep(0.0f, 6.0f, 52.0f - Latitude) * 0.45f, 0.0f, 1.0f);
			const float Boreal = FMath::SmoothStep(56.0f, 63.0f, Latitude);
			FSeen C = GroundByHeight(Metres);
			C = Mix(C, FSeen::Bytes(186, 160, 108), FMath::Clamp(DryCountry * (0.55f + 0.3f * Broad), 0.0f, 0.8f) * FMath::SmoothStep(0.0f, 600.0f, 1600.0f - Metres));
			C = Mix(C, FSeen::Bytes(216, 192, 142) * (0.94f + 0.12f * Middling), FMath::Clamp(Dry[Idx] * 1.1f, 0.0f, 0.92f));
			C = Mix(C, FSeen::Bytes(84, 108, 78), Boreal * 0.55f * FMath::SmoothStep(0.0f, 400.0f, 1100.0f - Metres));
			C = Mix(C, FSeen::Bytes(96, 112, 82), Marsh[Idx] * 0.6f);
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
			const float M = Terrain.HasElevation() ? FMath::Max(Terrain.MetresAt(I * Terrain.Cell, J * Terrain.Cell), 0.0f) : FMath::Max(Terrain.Height[Idx], 0.0f) * 380.0f;
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
			const FVector2D P(I * Terrain.Cell, J * Terrain.Cell);

			// Steep ground is rock whatever its height; high flat ground (a plateau) keeps its nation's colour.
			// The highest crests are snow, and a narrow strip along the shore is sand.
			const float Rock = FMath::Min(0.85f, 0.72f * FMath::SmoothStep(0.16f, 0.62f, Steepness) * FMath::SmoothStep(0.7f, 2.2f, H)
				+ 0.3f * FMath::SmoothStep(3.6f, 5.4f, H));
			const float Snow = 0.9f * FMath::SmoothStep(5.0f, 6.6f, H) * (1.0f - 0.5f * Steepness);
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
				+ FMath::SmoothStep(56.0f, 63.0f, Northing) * 0.2f - DryLand * 0.16f + FMath::SmoothStep(250.0f, 900.0f, Metres[Idx]) * 0.08f;
			float Wooded = FMath::SmoothStep(WoodsFrom(), WoodsFrom() + 0.07f, Thick) * FMath::SmoothStep(0.0f, 300.0f, 1750.0f - Metres[Idx]);
			Wooded *= (1.0f - Rock) * (1.0f - Sand / 0.35f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);

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
			const float Shade = FMath::Clamp(Sun, 0.0f, 2.2f) / 2.2f * 2.0f; // (kept as half of it in the picture's blue, below)

			// Which way the ground leans, as the lamp sees it: north is +X and east is +Y in the world.
			// (on the low ground the lamp, too, sees the land leaning only half as much as it does)
			const FVector3f Normal = FVector3f(Slope.Y * VerticalScale * (1.0f - 0.5f * Lowland), -Slope.X * VerticalScale * (1.0f - 0.5f * Lowland), 1.0f).GetSafeNormal();

			const FVector3f Toned = Tone();
			FColor Bare(ToByte(GroundR[Idx] * Toned.X), ToByte(GroundG[Idx] * Toned.Y), ToByte(GroundB[Idx] * Toned.Z), 255);
			Bare.A = ToByte(Wooded);
			Ground[Idx] = Bare;
			Relief[Idx] = FColor(ToByte(Normal.X * 0.5f + 0.5f), ToByte(Normal.Y * 0.5f + 0.5f), ToByte(Shade * 0.5f), 255);
			Masks[Idx] = FColor(ToByte(Rock), ToByte(Snow), ToByte(Sand), ToByte(Dry[Idx]));
		}
	});

	// What covers the ground (art fix 7): farmland for the quilt of fields, and the rims of the woods.
	Cover.SetNumUninitialized(Count);
	ParallelFor(NY, [&](int32 J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FVector2D P(I * Terrain.Cell, J * Terrain.Cell);
			const float Latitude = 71.4f - static_cast<float>(P.Y) / 40.0f, Longitude = -11.0f + static_cast<float>(P.X) / 25.7f;
			const float Boreal = FMath::SmoothStep(56.0f, 63.0f, Latitude);
			const float Broad = 0.5f + 0.5f * FMath::PerlinNoise2D(P * 0.02);
			const float Wooded = Ground[Idx].A / 255.0f;
			const FColor& Mask = Masks[Idx];
			// Farmland: the low ground, where it is not desert, marsh, wood, rock or beach; thinning in the far north.
			float Arable = FMath::SmoothStep(0.0f, 300.0f, 520.0f - Metres[Idx]) * (1.0f - FMath::Clamp(Dry[Idx] * 1.4f, 0.0f, 1.0f)) * (1.0f - Boreal * 0.75f)
				* (0.55f + 0.45f * Broad) * (1.0f - Marsh[Idx]) * (1.0f - Wooded) * (1.0f - Mask.R / 255.0f) * (1.0f - Mask.G / 255.0f) * (1.0f - FMath::Min(Mask.B / 90.0f, 1.0f));
			Arable = FMath::Clamp(Arable, 0.0f, 1.0f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);
			// Small fields and heavy hedges in the Atlantic west, great open ones on the eastern plains.
			const float Big = FMath::Clamp(FMath::SmoothStep(11.0f, 27.0f, Longitude) + 0.3f * FMath::PerlinNoise2D(P * 0.011 + FVector2D(5.0, 23.0)), 0.0f, 1.0f);
			// The rim of a wood: its sunward edge catches the light, and it throws a strip of shade on the open
			// ground behind it. (The sun stands in the west-north-west: the grid point toward it is the one to the west.)
			const float Sunward = Ground[Terrain.Index(FMath::Max(I - 1, 0), J)].A / 255.0f;
			const float Lit = FMath::Clamp(Wooded - Sunward, 0.0f, 1.0f), Shaded = FMath::Clamp(Sunward - Wooded, 0.0f, 1.0f);
			// (a rim should be about 0.6 map units wide; one point of this picture is 1.5, so the light on it is
			// cut to what the narrower rim would give, or a small wood would be all rim and go pale)
			const float Rim = 0.5f + 0.5f * FMath::Clamp(0.19f * Lit - Shaded, -1.0f, 1.0f);
			// Fir and pine in the north and on the high ground.
			const float Fir = FMath::Max(Boreal, FMath::SmoothStep(700.0f, 1300.0f, Metres[Idx]));
			Cover[Idx] = FColor(ToByte(Arable), ToByte(Big), ToByte(Rim), ToByte(Fir));
		}
	});

	// How the fields lie in each province: turned by an angle of the province's own, and a little larger or smaller.
	Lie.SetNumUninitialized(Count);
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int32 Id = Terrain.Region[Idx] >= 0 ? Terrain.Region[Idx] : ShoreRegion[Idx];
		if (Id < 0)
		{
			Lie[Idx] = FColor(255, 128, 128, 255);
			continue;
		}
		const uint32 Seed = static_cast<uint32>(Id) * 2654435761u;
		const float Angle = ((Seed >> 8) & 0xFFFF) / 65535.0f * UE_PI; // anywhere in the half circle: a grid turned further is the same grid again
		const float Larger = 0.5f + 0.5f * ((((Seed >> 3) & 0xFF) / 255.0f) * 2.0f - 1.0f) * 0.6f; // 0.2 to 0.8
		Lie[Idx] = FColor(ToByte(0.5f + 0.5f * FMath::Cos(Angle)), ToByte(0.5f + 0.5f * FMath::Sin(Angle)), ToByte(Larger), 255);
	}

	Tint.SetNumUninitialized(Count);
	PaintTint(Data, Terrain, 0, 0, NX - 1, NY - 1);

	// The sea, by its depth: pale teal over the shelves (the North Sea and the Baltic), darkening through the
	// slopes to the abyss; the sea floor faintly shaded by the same sun as the land; a little unevenness for the
	// swell; pale turquoise in the shallows for about three map units out; and the land's shadow on the water
	// on the side away from the sun. The alpha is how far offshore the point is, for the line of surf.
	WaterNX = FMath::CeilToInt32(Data.Width / WaterStep) + 1;
	WaterNY = FMath::CeilToInt32(Data.Height / WaterStep) + 1;
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
			const double X = FMath::Min(I * WaterStep, Data.Width);
			const double Y = FMath::Min(J * WaterStep, Data.Height);
			const float OffShore = -Terrain.ShoreAt(X, Y);
			auto Bed = [&Terrain](double AtX, double AtY) { return FMath::Min(Terrain.MetresAt(AtX, AtY), 0.0f); };
			float Deep = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(OffShore / 40.0f, 0.0f, 1.0f));
			float Floor = 1.0f;
			if (Terrain.HasElevation())
			{
				Deep = FMath::Max(FMath::Pow(FMath::Clamp(-Bed(X, Y) / 3200.0f, 0.0f, 1.0f), 0.55f), SeaFloor);
				const float SlopeX = (Bed(X + 1.5, Y) - Bed(X - 1.5, Y)) / 3.0f, SlopeY = (Bed(X, Y + 1.5) - Bed(X, Y - 1.5)) / 3.0f;
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
			C = C * (Floor * Swell);
			// (a narrow band, and less of it over water that is pale already: or a shallow enclosed sea turns to milk)
			C = Mix(C, FSeen::Bytes(120, 190, 184), 0.25f * FMath::Pow(FMath::Clamp(1.0f - OffShore / 2.0f, 0.0f, 1.0f), 1.2f) * FMath::SmoothStep(0.04f, 0.2f, Deep));
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
			C = C * (1.0f - 0.3f * FMath::Clamp(Lee * 1.6f, 0.0f, 1.0f));
			const float Grey = C.Light();
			auto Lift = [](float Part) { return Part + 0.031f * FMath::Max(0.0f, 1.0f - Part / 0.5f); };
			C = FSeen(Lift(C.R), Lift(C.G) * 0.92f, Lift(C.B) * 0.98f);
			C = Mix(C, FSeen(Grey, Grey, Grey), SeaGrey) * SeaStrength;
			Water[J * WaterNX + I] = FColor(ToByte(C.R), ToByte(C.G), ToByte(C.B), ToByte(FMath::Clamp(OffShore / 6.0f, 0.0f, 1.0f)));
		}
	});
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
