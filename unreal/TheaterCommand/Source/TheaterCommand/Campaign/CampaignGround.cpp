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

	/** How wooded each kind of country is: forest nearly all, open country a copse here and there. */
	float WoodsFor(const FString& Terrain)
	{
		if (Terrain == TEXT("forest")) return 0.92f;
		if (Terrain == TEXT("hills")) return 0.34f;
		if (Terrain == TEXT("mountain")) return 0.3f;
		if (Terrain == TEXT("marsh")) return 0.24f;
		if (Terrain == TEXT("urban")) return 0.04f;
		if (Terrain == TEXT("desert")) return 0.0f;
		return 0.16f; // plains
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
	FVector3f Tone()
	{
		static FVector3f Kept(0.0f);
		if (Kept.X <= 0.0f)
		{
			Kept = FVector3f(0.72f, 0.78f, 0.82f);
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
		Woods[Idx] = Id >= 0 ? RegionWoods[Id] : 0.12f;
		Dry[Idx] = Id >= 0 ? RegionDry[Id] : 0.0f;
		Marsh[Idx] = Id >= 0 && Data.Regions[Id].Terrain == TEXT("marsh") ? 1.0f : 0.0f;
	}
	const int32 Soften = FMath::RoundToInt32(4.0 / Terrain.Cell);
	FCampaignTerrain::Smooth(Woods, NX, NY, FMath::RoundToInt32(7.0 / Terrain.Cell), 2); // the edge of a wood does not follow a frontier
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

	// How each point stands against the ground around it: crests come out lighter, valley floors darker.
	TArray<float> Surround = Terrain.Height;
	FCampaignTerrain::Smooth(Surround, NX, NY, FMath::RoundToInt32(5.0 / Terrain.Cell), 2);

	// The direction the lamp shines from, in map terms (x east, y south, z up): low in the west-south-west.
	// The relief is shaded into the paint as well as lit by the lamp, the way a printed relief map is,
	// so that gentle country still reads as country.
	const FVector3f ToLamp = FVector3f(-0.80f, 0.30f, 0.52f).GetSafeNormal();
	const float Exaggerate = 1.8f;

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
			const float Sand = 0.45f * (1.0f - FMath::SmoothStep(0.2f, 1.8f, Shore));

			// Woods stand in patches: nearly everywhere in forest country, a copse here and there in the open,
			// none above the tree line, on bare rock, on the beach or in the sea.
			const float Patch = 0.5f + 0.5f * (0.65f * FMath::PerlinNoise2D(P * 0.045) + 0.35f * FMath::PerlinNoise2D(P * 0.13 + FVector2D(31.0, -17.0)));
			float Wooded = FMath::SmoothStep(0.42f, 0.58f, Patch + (Woods[Idx] - 0.5f) * 0.82f);
			Wooded *= (1.0f - FMath::SmoothStep(2.6f, 3.8f, H)) * (1.0f - Rock) * (1.0f - Sand / 0.45f) * FMath::SmoothStep(0.5f, 0.8f, Terrain.Coverage[Idx]);

			// Shaded relief, as on a printed map, and how the point stands among its neighbours.
			const FVector3f Leaning = FVector3f(-Slope.X * Exaggerate, -Slope.Y * Exaggerate, 1.0f).GetSafeNormal();
			const float Lit = FMath::Clamp(FVector3f::DotProduct(Leaning, ToLamp) / ToLamp.Z, 0.35f, 1.45f);
			const float Standing = FMath::Clamp(1.0f + (H - Surround[Idx]) * 0.16f, 0.72f, 1.18f);
			// A little unevenness, so that large areas do not look printed.
			const float Grain = 1.0f + 0.03f * FMath::PerlinNoise2D(FVector2D(I * 0.61, J * 0.61));
			const float Shade = FMath::Lerp(1.0f, Lit, 0.5f) * Standing * Grain;

			// Which way the ground leans, as the lamp sees it: north is +X and east is +Y in the world.
			const FVector3f Normal = FVector3f(Slope.Y * VerticalScale, -Slope.X * VerticalScale, 1.0f).GetSafeNormal();

			const FVector3f Toned = Tone();
			FColor Bare(ToByte(GroundR[Idx] * Toned.X), ToByte(GroundG[Idx] * Toned.Y), ToByte(GroundB[Idx] * Toned.Z), 255);
			Bare.A = ToByte(Wooded);
			Ground[Idx] = Bare;
			Relief[Idx] = FColor(ToByte(Normal.X * 0.5f + 0.5f), ToByte(Normal.Y * 0.5f + 0.5f), ToByte(Shade * 0.5f), 255);
			Masks[Idx] = FColor(ToByte(Rock), ToByte(Snow), ToByte(Sand), ToByte(Dry[Idx]));
		}
	});

	Tint.SetNumUninitialized(Count);
	PaintTint(Data, Terrain, 0, 0, NX - 1, NY - 1);

	// The sea. The water pales toward every coast, the way a chart shows shoal water. Where the real depth
	// is known the shading follows it: the North Sea and the Baltic lie pale on their shelf, the Atlantic
	// and the deep basins of the Mediterranean dark.
	WaterNX = FMath::CeilToInt32(Data.Width / WaterStep) + 1;
	WaterNY = FMath::CeilToInt32(Data.Height / WaterStep) + 1;
	Water.SetNumUninitialized(WaterNX * WaterNY);
	const FLinearColor Deep = FromHex(TEXT("15304A"));
	const FLinearColor Shallow = FromHex(TEXT("356E80"));
	ParallelFor(WaterNY, [&](int32 J)
	{
		for (int32 I = 0; I < WaterNX; ++I)
		{
			const double X = FMath::Min(I * WaterStep, Data.Width);
			const double Y = FMath::Min(J * WaterStep, Data.Height);
			const float OffShore = -Terrain.ShoreAt(X, Y);
			float Depth = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(OffShore / 9.0f, 0.0f, 1.0f));
			if (Terrain.HasElevation())
			{
				const float Fathomed = FMath::Clamp(FMath::Pow(FMath::Max(-Terrain.MetresAt(X, Y), 0.0f) / 3200.0f, 0.42f), 0.0f, 1.0f);
				Depth = FMath::Min(Depth, 0.25f + 0.75f * Fathomed) * FMath::Lerp(0.55f, 1.0f, Fathomed);
			}
			Water[J * WaterNX + I] = FMath::Lerp(Shallow, Deep, FMath::Pow(Depth, 0.7f)).ToFColor(true);
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
	// (the alpha here: 1 on the frontier, fading to nothing about ten map units inside it).
	const double Sigma = 3.0;
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
				Band = Terrain.Region[Idx] >= 0 ? FMath::Pow(FMath::Min(1.0f, 2.4f * static_cast<float>(FMath::Exp(-Far * Far / (2.0 * Sigma * Sigma)))), 0.75f) : 0.0f;
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
