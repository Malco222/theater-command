#include "Campaign/CampaignTextures.h"

#include "Engine/Texture2D.h"
#include "TextureResource.h"

namespace
{
	/** Writes one level of a picture into the texture's store. */
	void WriteLevel(FTexture2DMipMap& Target, const TArray<FColor>& Pixels)
	{
		Target.BulkData.Lock(LOCK_READ_WRITE);
		void* Out = Target.BulkData.Realloc(static_cast<int64>(Pixels.Num()) * sizeof(FColor));
		FMemory::Memcpy(Out, Pixels.GetData(), static_cast<int64>(Pixels.Num()) * sizeof(FColor));
		Target.BulkData.Unlock();
	}

	/** The next smaller copy of a picture: each pixel the average of the four it stands for. */
	void Halve(const TArray<FColor>& In, int32 Width, int32 Height, bool bColour, TArray<FColor>& Out, int32& OutWidth, int32& OutHeight)
	{
		OutWidth = FMath::Max(1, Width / 2);
		OutHeight = FMath::Max(1, Height / 2);
		Out.SetNumUninitialized(OutWidth * OutHeight);
		for (int32 J = 0; J < OutHeight; ++J)
		{
			const int32 J0 = FMath::Min(J * 2, Height - 1), J1 = FMath::Min(J * 2 + 1, Height - 1);
			for (int32 I = 0; I < OutWidth; ++I)
			{
				const int32 I0 = FMath::Min(I * 2, Width - 1), I1 = FMath::Min(I * 2 + 1, Width - 1);
				const FColor& A = In[J0 * Width + I0];
				const FColor& B = In[J0 * Width + I1];
				const FColor& C = In[J1 * Width + I0];
				const FColor& D = In[J1 * Width + I1];
				if (bColour)
				{
					// Colours are averaged as light, not as the numbers that stand for them, so a far-off patch
					// of light and dark comes out as bright as it should.
					FLinearColor Sum = FLinearColor(A) + FLinearColor(B) + FLinearColor(C) + FLinearColor(D);
					Sum *= 0.25f;
					FColor Mixed = Sum.ToFColor(true);
					Mixed.A = static_cast<uint8>((A.A + B.A + C.A + D.A + 2) / 4);
					Out[J * OutWidth + I] = Mixed;
				}
				else
				{
					Out[J * OutWidth + I] = FColor(
						static_cast<uint8>((A.R + B.R + C.R + D.R + 2) / 4),
						static_cast<uint8>((A.G + B.G + C.G + D.G + 2) / 4),
						static_cast<uint8>((A.B + B.B + C.B + D.B + 2) / 4),
						static_cast<uint8>((A.A + B.A + C.A + D.A + 2) / 4));
				}
			}
		}
	}

	/** Fills every level of a texture from its full-size pixels. Levels are added if the texture has none yet. */
	void Fill(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 Width, int32 Height, bool bColour)
	{
		FTexturePlatformData* Platform = Texture->GetPlatformData();
		TArray<FColor> Level = Pixels, Smaller;
		int32 W = Width, H = Height;
		for (int32 Mip = 0;; ++Mip)
		{
			if (!Platform->Mips.IsValidIndex(Mip))
			{
				Platform->Mips.Add(new FTexture2DMipMap(W, H, 1));
			}
			WriteLevel(Platform->Mips[Mip], Level);
			if (W == 1 && H == 1)
			{
				break;
			}
			int32 SmallerW = 0, SmallerH = 0;
			Halve(Level, W, H, bColour, Smaller, SmallerW, SmallerH);
			Level = MoveTemp(Smaller);
			W = SmallerW;
			H = SmallerH;
		}
		Texture->UpdateResource();
	}

	// ------------------------------------------------------------------ noise that repeats

	uint32 Hash(uint32 X, uint32 Y, uint32 Seed)
	{
		uint32 H = X * 374761393u + Y * 668265263u + Seed * 2246822519u;
		H = (H ^ (H >> 13)) * 1274126177u;
		return H ^ (H >> 16);
	}

	int32 Wrap(int32 Value, int32 Period)
	{
		const int32 M = Value % Period;
		return M < 0 ? M + Period : M;
	}

	/**
	 * Smooth noise between -1 and 1 that repeats every Period steps each way. X and Y are in steps:
	 * across a picture that is to tile, they run from 0 to Period.
	 */
	float Noise(float X, float Y, int32 Period, uint32 Seed)
	{
		const int32 X0 = FMath::FloorToInt32(X), Y0 = FMath::FloorToInt32(Y);
		const float FX = X - X0, FY = Y - Y0;
		auto Corner = [&](int32 IX, int32 IY, float DX, float DY)
		{
			const uint32 H = Hash(static_cast<uint32>(Wrap(IX, Period)), static_cast<uint32>(Wrap(IY, Period)), Seed);
			const float Angle = (H & 0xFFFF) * (UE_TWO_PI / 65536.0f);
			return FMath::Cos(Angle) * DX + FMath::Sin(Angle) * DY;
		};
		const float U = FX * FX * FX * (FX * (FX * 6.0f - 15.0f) + 10.0f);
		const float V = FY * FY * FY * (FY * (FY * 6.0f - 15.0f) + 10.0f);
		const float Top = FMath::Lerp(Corner(X0, Y0, FX, FY), Corner(X0 + 1, Y0, FX - 1.0f, FY), U);
		const float Bottom = FMath::Lerp(Corner(X0, Y0 + 1, FX, FY - 1.0f), Corner(X0 + 1, Y0 + 1, FX - 1.0f, FY - 1.0f), U);
		return FMath::Clamp(FMath::Lerp(Top, Bottom, V) * 1.5f, -1.0f, 1.0f);
	}

	/** Several sizes of noise laid over one another, each half the size and half the strength of the last. U and V run 0 to 1 across the picture. */
	float Layered(float U, float V, int32 Period, int32 Octaves, uint32 Seed)
	{
		float Sum = 0.0f, Amp = 0.5f, Total = 0.0f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += Amp * Noise(U * Period, V * Period, Period, Seed + O * 101u);
			Total += Amp;
			Amp *= 0.5f;
			Period *= 2;
		}
		return Sum / Total;
	}

	/**
	 * Scattered points, so many cells each way: how far (in cells) the nearest point is, and a number
	 * between 0 and 1 that belongs to that point. Repeats across the picture.
	 */
	float Scatter(float U, float V, int32 Cells, uint32 Seed, float& OutOwn)
	{
		const float X = U * Cells, Y = V * Cells;
		const int32 CX = FMath::FloorToInt32(X), CY = FMath::FloorToInt32(Y);
		float Nearest = 10.0f;
		OutOwn = 0.0f;
		for (int32 J = -1; J <= 1; ++J)
		{
			for (int32 I = -1; I <= 1; ++I)
			{
				const uint32 H = Hash(static_cast<uint32>(Wrap(CX + I, Cells)), static_cast<uint32>(Wrap(CY + J, Cells)), Seed);
				const float PX = CX + I + ((H & 0xFF) / 255.0f) * 0.84f + 0.08f;
				const float PY = CY + J + (((H >> 8) & 0xFF) / 255.0f) * 0.84f + 0.08f;
				const float D = FMath::Sqrt((PX - X) * (PX - X) + (PY - Y) * (PY - Y));
				if (D < Nearest)
				{
					Nearest = D;
					OutOwn = ((H >> 16) & 0xFF) / 255.0f;
				}
			}
		}
		return Nearest;
	}

	uint8 ToByte(float Value)
	{
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Value * 255.0f), 0, 255));
	}

	constexpr int32 Size = CampaignTextures::DetailSize;

	/** Which way a height field leans at each point, as two numbers round mid-grey. The field repeats. */
	void Lean(const TArray<float>& Heights, float Strength, TArray<FVector2f>& Out)
	{
		Out.SetNumUninitialized(Size * Size);
		for (int32 J = 0; J < Size; ++J)
		{
			for (int32 I = 0; I < Size; ++I)
			{
				const float DX = Heights[J * Size + Wrap(I + 1, Size)] - Heights[J * Size + Wrap(I - 1, Size)];
				const float DY = Heights[Wrap(J + 1, Size) * Size + I] - Heights[Wrap(J - 1, Size) * Size + I];
				FVector3f Normal(-DX * Strength, -DY * Strength, 1.0f);
				Normal.Normalize();
				Out[J * Size + I] = FVector2f(Normal.X, Normal.Y);
			}
		}
	}

	/** Tree-tops seen from above: rounded crowns of several sizes packed together, the gaps between them dark. */
	void TreeTops(TArray<float>& OutHeights)
	{
		OutHeights.SetNumUninitialized(Size * Size);
		for (int32 J = 0; J < Size; ++J)
		{
			for (int32 I = 0; I < Size; ++I)
			{
				const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
				float OwnBig = 0.0f, OwnSmall = 0.0f;
				const float Big = Scatter(U, V, 13, 11u, OwnBig);
				const float Small = Scatter(U, V, 23, 23u, OwnSmall);
				// Each crown is a dome; where a big one and a small one overlap the taller shows.
				const float BigCrown = FMath::Sqrt(FMath::Clamp(1.0f - FMath::Square(Big / 0.62f), 0.0f, 1.0f)) * (0.72f + 0.28f * OwnBig);
				const float SmallCrown = FMath::Sqrt(FMath::Clamp(1.0f - FMath::Square(Small / 0.58f), 0.0f, 1.0f)) * (0.45f + 0.3f * OwnSmall);
				const float Ruffle = 0.06f * Noise(U * 128.0f, V * 128.0f, 128, 31u);
				OutHeights[J * Size + I] = FMath::Clamp(FMath::Max(BigCrown, SmallCrown) + Ruffle, 0.0f, 1.0f);
			}
		}
	}

	/** Bare rock: ridges and cracks, the big shapes carrying the small. */
	void Crags(TArray<float>& OutHeights)
	{
		OutHeights.SetNumUninitialized(Size * Size);
		for (int32 J = 0; J < Size; ++J)
		{
			for (int32 I = 0; I < Size; ++I)
			{
				const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
				float Sum = 0.0f, Amp = 0.5f, Total = 0.0f, Carry = 1.0f;
				int32 Period = 6;
				for (int32 O = 0; O < 5; ++O)
				{
					float Ridge = 1.0f - FMath::Abs(Noise(U * Period, V * Period, Period, 51u + O * 7u));
					Ridge *= Ridge;
					Sum += Amp * Ridge * Carry;
					Total += Amp;
					Carry = FMath::Clamp(Ridge * 1.5f, 0.0f, 1.0f);
					Amp *= 0.55f;
					Period *= 2;
				}
				OutHeights[J * Size + I] = Sum / Total;
			}
		}
	}
}

UTexture2D* CampaignTextures::Create(int32 Width, int32 Height, const TArray<FColor>& Pixels, bool bColour, bool bTile, const FName& Name)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return nullptr;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, Name);
	if (!Texture)
	{
		return nullptr;
	}
	Texture->SRGB = bColour;
	Texture->Filter = TF_Trilinear;
	Texture->AddressX = bTile ? TA_Wrap : TA_Clamp;
	Texture->AddressY = bTile ? TA_Wrap : TA_Clamp;
	Texture->NeverStream = true;
	Texture->CompressionSettings = bColour ? TC_Default : TC_VectorDisplacementmap; // the second means: plain numbers, left alone
	Fill(Texture, Pixels, Width, Height, bColour);
	return Texture;
}

void CampaignTextures::Update(UTexture2D* Texture, const TArray<FColor>& Pixels)
{
	if (!Texture || !Texture->GetPlatformData() || Pixels.Num() != Texture->GetSizeX() * Texture->GetSizeY())
	{
		return;
	}
	Fill(Texture, Pixels, Texture->GetSizeX(), Texture->GetSizeY(), Texture->SRGB);
}

void CampaignTextures::MakeGroundDetail(TArray<FColor>& OutPixels)
{
	TArray<float> Trees, Rock;
	TreeTops(Trees);
	Crags(Rock);
	OutPixels.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
			// Grass: the fine speckle of flock, with soft patches of lighter and darker.
			const float Grass = 0.5f + 0.2f * Noise(U * 256.0f, V * 256.0f, 256, 3u) + 0.12f * Noise(U * 96.0f, V * 96.0f, 96, 5u) + 0.1f * Layered(U, V, 8, 3, 7u);
			// Tree-tops: lit crowns, dark between.
			const float Crown = Trees[J * Size + I];
			// The crowns are no brighter than open ground and the gaps between them a good deal darker, so a wood
			// reads as a dark, knobbly mass, the way it does from the air.
			const float Wood = 0.2f + 0.34f * FMath::Pow(Crown, 0.7f) + 0.03f * Noise(U * 200.0f, V * 200.0f, 200, 13u);
			// Rock: pale ridges, dark cracks.
			const float Stone = 0.22f + 0.72f * Rock[J * Size + I] + 0.08f * Noise(U * 180.0f, V * 180.0f, 180, 17u);
			// Sand: grain, and long low ripples that wander.
			const float Wander = 0.9f * Layered(U, V, 4, 3, 19u);
			const float Sand = 0.5f + 0.1f * FMath::Sin((V * 30.0f + Wander) * UE_TWO_PI) + 0.12f * Noise(U * 256.0f, V * 256.0f, 256, 29u);
			OutPixels[J * Size + I] = FColor(ToByte(Grass), ToByte(Wood), ToByte(Stone), ToByte(Sand));
		}
	}
}

void CampaignTextures::MakeFields(TArray<FColor>& OutPixels)
{
	// Six tones from pasture through wheat to plough, and everything between them: a field takes one by the throw
	// of its own number. What is kept is how the tone stands to the average of them all, as light (half is no change).
	struct FStop { float At; FVector3f Tone; };
	static const FStop Stops[] =
	{
		{ 0.0f, FVector3f(120.0f, 146.0f, 84.0f) }, { 0.3f, FVector3f(140.0f, 150.0f, 86.0f) }, { 0.5f, FVector3f(184.0f, 172.0f, 104.0f) },
		{ 0.68f, FVector3f(150.0f, 132.0f, 92.0f) }, { 0.84f, FVector3f(104.0f, 134.0f, 78.0f) }, { 1.0f, FVector3f(170.0f, 166.0f, 110.0f) },
	};
	const FVector3f Average(143.0f, 150.0f, 91.0f);
	OutPixels.SetNumUninitialized(FieldsSize * FieldsSize);
	for (int32 J = 0; J < FieldsSize; ++J)
	{
		for (int32 I = 0; I < FieldsSize; ++I)
		{
			const uint32 H = Hash(static_cast<uint32>(I), static_cast<uint32>(J), 7u);
			const float Throw = (H & 0xFFFF) / 65535.0f;
			FVector3f Tone = Stops[0].Tone;
			for (int32 K = 0; K + 1 < UE_ARRAY_COUNT(Stops); ++K)
			{
				if (Throw >= Stops[K].At)
				{
					Tone = FMath::Lerp(Stops[K].Tone, Stops[K + 1].Tone, FMath::Clamp((Throw - Stops[K].At) / (Stops[K + 1].At - Stops[K].At), 0.0f, 1.0f));
				}
			}
			auto Share = [](float Seen, float Mean) { return ToByte(FMath::Clamp(FMath::Pow(Seen / Mean, 2.2f), 0.0f, 2.0f) * 0.5f); };
			OutPixels[J * FieldsSize + I] = FColor(Share(Tone.X, Average.X), Share(Tone.Y, Average.Y), Share(Tone.Z, Average.Z), static_cast<uint8>((H >> 16) & 0xFF));
		}
	}
}

void CampaignTextures::MakeGroundBumps(TArray<FColor>& OutPixels)
{
	TArray<float> Trees, Rock;
	TreeTops(Trees);
	Crags(Rock);
	TArray<FVector2f> TreeLean, RockLean;
	Lean(Trees, 3.2f, TreeLean);
	Lean(Rock, 5.0f, RockLean);
	OutPixels.SetNumUninitialized(Size * Size);
	for (int32 Index = 0; Index < Size * Size; ++Index)
	{
		OutPixels[Index] = FColor(ToByte(TreeLean[Index].X * 0.5f + 0.5f), ToByte(TreeLean[Index].Y * 0.5f + 0.5f),
			ToByte(RockLean[Index].X * 0.5f + 0.5f), ToByte(RockLean[Index].Y * 0.5f + 0.5f));
	}
}

void CampaignTextures::MakeWaterBumps(TArray<FColor>& OutPixels)
{
	TArray<float> First, Second;
	First.SetNumUninitialized(Size * Size);
	Second.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
			First[J * Size + I] = Layered(U, V, 7, 4, 61u);
			Second[J * Size + I] = Layered(U, V, 11, 4, 67u);
		}
	}
	TArray<FVector2f> FirstLean, SecondLean;
	Lean(First, 9.0f, FirstLean);
	Lean(Second, 9.0f, SecondLean);
	OutPixels.SetNumUninitialized(Size * Size);
	for (int32 Index = 0; Index < Size * Size; ++Index)
	{
		OutPixels[Index] = FColor(ToByte(FirstLean[Index].X * 0.5f + 0.5f), ToByte(FirstLean[Index].Y * 0.5f + 0.5f),
			ToByte(SecondLean[Index].X * 0.5f + 0.5f), ToByte(SecondLean[Index].Y * 0.5f + 0.5f));
	}
}
