#include "Campaign/CampaignGround.h"

#include "Campaign/CampaignMapData.h"
#include "Campaign/CampaignTerrain.h"
#include "Async/ParallelFor.h"

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

	uint8 ToByte(float Value)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Value * 255.0f), 0, 255));
	}
}

FLinearColor FCampaignGround::RockColour() { return FromHex(TEXT("857D70")); }
FLinearColor FCampaignGround::SnowColour() { return FromHex(TEXT("F2F0EA")); }
FLinearColor FCampaignGround::SandColour() { return FromHex(TEXT("D9CBA4")); }

void FCampaignGround::Build(const FCampaignMapData& Data, const FCampaignTerrain& Terrain, float VerticalScale)
{
	NX = Terrain.NX;
	NY = Terrain.NY;
	const int32 Count = NX * NY;

	// The ground colour of each kind of country, and how wooded and how dry it is, softened across region boundaries.
	TArray<FLinearColor> RegionGround;
	TArray<float> RegionWoods, RegionDry;
	RegionGround.SetNum(Data.Regions.Num());
	RegionWoods.SetNum(Data.Regions.Num());
	RegionDry.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		RegionGround[Region.Id] = GroundColor(Region.Terrain);
		RegionWoods[Region.Id] = WoodsFor(Region.Terrain);
		RegionDry[Region.Id] = Region.Terrain == TEXT("desert") ? 1.0f : 0.0f;
	}
	GroundR.SetNumUninitialized(Count);
	GroundG.SetNumUninitialized(Count);
	GroundB.SetNumUninitialized(Count);
	TArray<float> Woods, Dry;
	Woods.SetNumUninitialized(Count);
	Dry.SetNumUninitialized(Count);
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int16 Id = Terrain.Region[Idx];
		const FLinearColor& C = Id >= 0 ? RegionGround[Id] : Unclaimed;
		GroundR[Idx] = C.R;
		GroundG[Idx] = C.G;
		GroundB[Idx] = C.B;
		Woods[Idx] = Id >= 0 ? RegionWoods[Id] : 0.12f;
		Dry[Idx] = Id >= 0 ? RegionDry[Id] : 0.0f;
	}
	const int32 Soften = FMath::RoundToInt32(4.0 / Terrain.Cell);
	FCampaignTerrain::Smooth(GroundR, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(GroundG, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(GroundB, NX, NY, Soften, 2);
	FCampaignTerrain::Smooth(Woods, NX, NY, FMath::RoundToInt32(7.0 / Terrain.Cell), 2); // the edge of a wood does not follow a frontier
	FCampaignTerrain::Smooth(Dry, NX, NY, Soften, 2);
	PlainsLightness = GroundColor(TEXT("plains")).GetLuminance();

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

			FColor Bare = FLinearColor(GroundR[Idx], GroundG[Idx], GroundB[Idx]).ToFColor(true);
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
	// Each nation's colour, looked up once per region.
	TArray<FLinearColor> RegionTint;
	RegionTint.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& Region : Data.Regions)
	{
		const FCampaignNation* Nation = Data.FindNation(Region.Owner);
		RegionTint[Region.Id] = Nation ? FLinearColor(Nation->Color) : Unclaimed;
		if (Nation && Data.League.Contains(Region.Owner))
		{
			// A member of the League: its ground is the League's teal, with a trace of the country's own colour.
			RegionTint[Region.Id] = FMath::Lerp(FLinearColor(Nation->Color), FLinearColor(FColor::FromHex(TEXT("1FA8A6"))), 0.9f);
		}
	}
	for (int32 J = J0; J <= J1; ++J)
	{
		for (int32 I = I0; I <= I1; ++I)
		{
			const int32 Idx = Terrain.Index(I, J);
			const FLinearColor Bare(GroundR[Idx], GroundG[Idx], GroundB[Idx]);
			FLinearColor C = Bare;
			// The nation's colour is painted over the ground. The kind of country shows through as light
			// and dark (forest darker, desert paler) and as a trace of its own colour; along the frontier
			// the paint is laid on thick.
			const int16 Id = Terrain.Region[Idx];
			if (Id >= 0)
			{
				const FLinearColor& Colour = RegionTint[Id];
				const float Lightness = FMath::Lerp(1.0f, Bare.GetLuminance() / PlainsLightness, 0.55f);
				// (The trace of the ground's own colour is matched to the paint's lightness first: laid on as it
				// is, pale ground washed a dark or a grey paint out until the country looked like bare paper.)
				const float Match = FMath::Clamp(Colour.GetLuminance() * Lightness / FMath::Max(Bare.GetLuminance(), 0.02f), 0.0f, 1.0f);
				const FLinearColor Painted = FMath::Lerp(Colour * Lightness, Bare * Match, 0.2f);
				const float Edge = 1.0f - FMath::SmoothStep(0.6f, 4.5f, Terrain.BorderDistance[Idx] * static_cast<float>(Terrain.Cell));
				C = FMath::Lerp(Painted, Colour * 0.8f, 0.6f * Edge);
			}
			C.A = 1.0f;
			Tint[Idx] = C.GetClamped().ToFColor(true);
		}
	}
}
