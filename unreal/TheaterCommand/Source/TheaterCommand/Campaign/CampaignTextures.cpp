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

namespace
{
	/** A new picture of this size and kind, with no pixels in it yet. */
	UTexture2D* NewPicture(int32 Width, int32 Height, bool bColour, bool bTile, const FName& Name, bool bSharp = false, bool bSlant = false)
	{
		UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8, Name);
		if (!Texture)
		{
			return nullptr;
		}
		Texture->SRGB = bColour;
		// (bSlant: the World group's own filter, anisotropic, in place of plain trilinear; see CreateFromLevels)
		Texture->Filter = bSharp ? TF_Nearest : bSlant ? TF_Default : TF_Trilinear;
		if (bSlant)
		{
			Texture->LODGroup = TEXTUREGROUP_World;
		}
		Texture->AddressX = bTile ? TA_Wrap : TA_Clamp;
		Texture->AddressY = bTile ? TA_Wrap : TA_Clamp;
		Texture->NeverStream = true;
		Texture->CompressionSettings = bColour ? TC_Default : TC_VectorDisplacementmap; // the second means: plain numbers, left alone
		return Texture;
	}
}

UTexture2D* CampaignTextures::Create(int32 Width, int32 Height, const TArray<FColor>& Pixels, bool bColour, bool bTile, const FName& Name, bool bSharp)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return nullptr;
	}
	UTexture2D* Texture = NewPicture(Width, Height, bColour, bTile, Name, bSharp);
	if (!Texture)
	{
		return nullptr;
	}
	Fill(Texture, Pixels, Width, Height, bColour);
	return Texture;
}

bool CampaignTextures::FLevels::Fits(int32 InWidth, int32 InHeight, bool bInColour, const TArray<FColor>& Pixels) const
{
	return Width == InWidth && Height == InHeight && bColour == bInColour && Each.Num() > 0 && Each.Num() == Sizes.Num()
		&& Each[0].Num() == Pixels.Num() && FMemory::Memcmp(Each[0].GetData(), Pixels.GetData(), static_cast<SIZE_T>(Pixels.Num()) * sizeof(FColor)) == 0;
}

void CampaignTextures::MakeLevels(int32 Width, int32 Height, const TArray<FColor>& Pixels, bool bColour, FLevels& Out)
{
	// (Level by level as Fill makes them: the same halving, from the same full-size pixels.)
	Out = FLevels();
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return;
	}
	Out.Width = Width;
	Out.Height = Height;
	Out.bColour = bColour;
	TArray<FColor> Level = Pixels, Smaller;
	int32 W = Width, H = Height;
	for (;;)
	{
		Out.Sizes.Add(FIntPoint(W, H));
		if (W == 1 && H == 1)
		{
			Out.Each.Add(MoveTemp(Level));
			break;
		}
		int32 SmallerW = 0, SmallerH = 0;
		Halve(Level, W, H, bColour, Smaller, SmallerW, SmallerH);
		Out.Each.Add(MoveTemp(Level));
		Level = MoveTemp(Smaller);
		W = SmallerW;
		H = SmallerH;
	}
}

UTexture2D* CampaignTextures::CreateFromLevels(const FLevels& Levels, bool bTile, const FName& Name, bool bSharp, bool bSlant)
{
	if (Levels.Each.Num() == 0 || Levels.Each.Num() != Levels.Sizes.Num())
	{
		return nullptr;
	}
	UTexture2D* Texture = NewPicture(Levels.Width, Levels.Height, Levels.bColour, bTile, Name, bSharp, bSlant);
	if (!Texture)
	{
		return nullptr;
	}
	FTexturePlatformData* Platform = Texture->GetPlatformData();
	for (int32 Mip = 0; Mip < Levels.Each.Num(); ++Mip)
	{
		if (!Platform->Mips.IsValidIndex(Mip))
		{
			Platform->Mips.Add(new FTexture2DMipMap(Levels.Sizes[Mip].X, Levels.Sizes[Mip].Y, 1));
		}
		WriteLevel(Platform->Mips[Mip], Levels.Each[Mip]);
	}
	Texture->UpdateResource();
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

void CampaignTextures::UpdateBox(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1)
{
	if (!Texture || !Texture->GetPlatformData() || !Texture->GetResource() || Pixels.Num() != Texture->GetSizeX() * Texture->GetSizeY())
	{
		return;
	}
	const int32 Wide = Texture->GetSizeX(), High = Texture->GetSizeY();
	X0 = FMath::Clamp(X0, 0, Wide - 1);
	X1 = FMath::Clamp(X1, X0, Wide - 1);
	Y0 = FMath::Clamp(Y0, 0, High - 1);
	Y1 = FMath::Clamp(Y1, Y0, High - 1);
	const int32 BoxWide = X1 - X0 + 1, BoxHigh = Y1 - Y0 + 1;
	// (a copy of the box, which the renderer reads when it comes to it and then throws away)
	FColor* Copy = static_cast<FColor*>(FMemory::Malloc(static_cast<SIZE_T>(BoxWide) * BoxHigh * sizeof(FColor)));
	for (int32 Row = 0; Row < BoxHigh; ++Row)
	{
		FMemory::Memcpy(Copy + Row * BoxWide, Pixels.GetData() + (Y0 + Row) * Wide + X0, BoxWide * sizeof(FColor));
	}
	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(X0, Y0, 0, 0, BoxWide, BoxHigh);
	Texture->UpdateTextureRegions(0, 1, Region, BoxWide * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(Copy),
		[](uint8* Data, const FUpdateTextureRegion2D* Regions)
		{
			FMemory::Free(Data);
			delete Regions;
		});
}

void CampaignTextures::UpdateBoxAll(UTexture2D* Texture, const TArray<FColor>& Pixels, int32 X0, int32 Y0, int32 X1, int32 Y1, int32 MaxLevel)
{
	if (!Texture || !Texture->GetPlatformData() || !Texture->GetResource() || Pixels.Num() != Texture->GetSizeX() * Texture->GetSizeY())
	{
		return;
	}
	UpdateBox(Texture, Pixels, X0, Y0, X1, Y1); // (the picture itself)
	const int32 Wide = Texture->GetSizeX(), High = Texture->GetSizeY();
	X0 = FMath::Clamp(X0, 0, Wide - 1);
	X1 = FMath::Clamp(X1, X0, Wide - 1);
	Y0 = FMath::Clamp(Y0, 0, High - 1);
	Y1 = FMath::Clamp(Y1, Y0, High - 1);
	const bool bColour = Texture->SRGB;
	// (colours are averaged as light, as Halve does it: a table gives each number its light)
	static float Light[256];
	static bool bLight = false;
	if (!bLight)
	{
		bLight = true;
		for (int32 Value = 0; Value < 256; ++Value)
		{
			Light[Value] = FLinearColor(FColor(static_cast<uint8>(Value), 0, 0)).R;
		}
	}
	const int32 Levels = FMath::Min(Texture->GetPlatformData()->Mips.Num(), MaxLevel < MAX_int32 ? MaxLevel + 1 : MAX_int32);
	for (int32 Level = 1; Level < Levels; ++Level)
	{
		// Each pixel of a smaller copy stands for a square of the picture: the squares the box touches are made again.
		const int32 LevelWide = FMath::Max(1, Wide >> Level), LevelHigh = FMath::Max(1, High >> Level), Square = 1 << Level;
		const int32 A0 = FMath::Clamp(X0 >> Level, 0, LevelWide - 1), A1 = FMath::Clamp(X1 >> Level, A0, LevelWide - 1);
		const int32 B0 = FMath::Clamp(Y0 >> Level, 0, LevelHigh - 1), B1 = FMath::Clamp(Y1 >> Level, B0, LevelHigh - 1);
		const int32 BoxWide = A1 - A0 + 1, BoxHigh = B1 - B0 + 1;
		FColor* Copy = static_cast<FColor*>(FMemory::Malloc(static_cast<SIZE_T>(BoxWide) * BoxHigh * sizeof(FColor)));
		for (int32 J = B0; J <= B1; ++J)
		{
			const int32 V0 = FMath::Min(J * Square, High - 1), V1 = FMath::Min(J * Square + Square - 1, High - 1);
			for (int32 I = A0; I <= A1; ++I)
			{
				const int32 U0 = FMath::Min(I * Square, Wide - 1), U1 = FMath::Min(I * Square + Square - 1, Wide - 1);
				double R = 0.0, G = 0.0, B = 0.0, A = 0.0;
				for (int32 V = V0; V <= V1; ++V)
				{
					const FColor* Row = Pixels.GetData() + V * Wide;
					for (int32 U = U0; U <= U1; ++U)
					{
						const FColor& Each = Row[U];
						if (bColour)
						{
							R += Light[Each.R];
							G += Light[Each.G];
							B += Light[Each.B];
						}
						else
						{
							R += Each.R;
							G += Each.G;
							B += Each.B;
						}
						A += Each.A;
					}
				}
				const double Count = static_cast<double>(U1 - U0 + 1) * (V1 - V0 + 1);
				FColor Mixed;
				if (bColour)
				{
					Mixed = FLinearColor(static_cast<float>(R / Count), static_cast<float>(G / Count), static_cast<float>(B / Count), 1.0f).ToFColor(true);
				}
				else
				{
					Mixed = FColor(static_cast<uint8>(FMath::RoundToInt32(R / Count)), static_cast<uint8>(FMath::RoundToInt32(G / Count)), static_cast<uint8>(FMath::RoundToInt32(B / Count)));
				}
				Mixed.A = static_cast<uint8>(FMath::RoundToInt32(A / Count));
				Copy[(J - B0) * BoxWide + (I - A0)] = Mixed;
			}
		}
		FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(A0, B0, 0, 0, BoxWide, BoxHigh);
		Texture->UpdateTextureRegions(Level, 1, Region, BoxWide * sizeof(FColor), sizeof(FColor), reinterpret_cast<uint8*>(Copy),
			[](uint8* Data, const FUpdateTextureRegion2D* Regions)
			{
				FMemory::Free(Data);
				delete Regions;
			});
	}
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
	// Fields are not all of a size: the picture is laid out in blocks of one to three pixels by one or two, each
	// block a single field, so that the quilt is not a chessboard.
	OutPixels.Init(FColor(0, 0, 0, 0), FieldsSize * FieldsSize);
	TArray<bool> Laid;
	Laid.Init(false, FieldsSize * FieldsSize);
	for (int32 J = 0; J < FieldsSize; ++J)
	{
		for (int32 I = 0; I < FieldsSize; ++I)
		{
			if (Laid[J * FieldsSize + I])
			{
				continue;
			}
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
			const FColor Field(Share(Tone.X, Average.X), Share(Tone.Y, Average.Y), Share(Tone.Z, Average.Z), static_cast<uint8>((H >> 16) & 0xFF));
			static const int32 Longs[] = { 1, 1, 1, 2 }, Deeps[] = { 1, 1, 1, 1, 2 }; // (strips two to four times as long as wide; a few are merged with a neighbour)
			const int32 Long = Longs[(H >> 20) % UE_ARRAY_COUNT(Longs)], Deep = Deeps[(H >> 26) % UE_ARRAY_COUNT(Deeps)];
			for (int32 DJ = 0; DJ < Deep && J + DJ < FieldsSize; ++DJ)
			{
				for (int32 DI = 0; DI < Long && I + DI < FieldsSize; ++DI)
				{
					const int32 At = (J + DJ) * FieldsSize + I + DI;
					if (Laid[At])
					{
						break; // (another field is already in the way: this one is the shorter for it)
					}
					Laid[At] = true;
					OutPixels[At] = Field;
				}
			}
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

void CampaignTextures::MakeWaves(TArray<FColor>& OutPixels)
{
	// A field of waves is several trains of them laid over one another, all running nearly the same way: where
	// they agree a crest stands, and where they disagree it dies, so that crests are short and broken and no ridge
	// runs across the sea. Each train fits the picture a whole number of times each way, so the picture repeats.
	struct FWaveTrain
	{
		int32 Lengthwise, Crosswise;
		float Phase, Height;
	};
	FRandomStream Dice(1947);
	auto Field = [&Dice](std::initializer_list<FIntPoint> Fits, TArray<float>& Out)
	{
		TArray<FWaveTrain> Trains;
		for (const FIntPoint& Fit : Fits)
		{
			Trains.Add({ Fit.X, Fit.Y, Dice.FRandRange(0.0f, UE_TWO_PI), Dice.FRandRange(0.7f, 1.0f) });
		}
		Out.SetNumUninitialized(Size * Size);
		double Squares = 0.0;
		for (int32 J = 0; J < Size; ++J)
		{
			for (int32 I = 0; I < Size; ++I)
			{
				const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
				float Sum = 0.0f;
				for (const FWaveTrain& Train : Trains)
				{
					Sum += Train.Height * FMath::Sin(UE_TWO_PI * (Train.Lengthwise * U + Train.Crosswise * V) + Train.Phase);
				}
				Out[J * Size + I] = Sum;
				Squares += Sum * Sum;
			}
		}
		// (its usual height is about a third of the most the picture can hold)
		const float Usual = FMath::Max(static_cast<float>(FMath::Sqrt(Squares / (Size * Size))), 0.0001f);
		for (float& Each : Out)
		{
			Each = FMath::Clamp(Each / Usual * 0.35f, -1.0f, 1.0f);
		}
	};
	TArray<float> Swell, Chop;
	Field({ FIntPoint(6, 0), FIntPoint(5, 2), FIntPoint(7, -2), FIntPoint(6, 3), FIntPoint(5, -3), FIntPoint(7, 1), FIntPoint(4, -1) }, Swell);
	Field({ FIntPoint(12, 0), FIntPoint(11, 4), FIntPoint(13, -4), FIntPoint(12, 6), FIntPoint(10, -5), FIntPoint(14, 2), FIntPoint(9, -2), FIntPoint(13, 5) }, Chop);
	// The flecks: soft round spots scattered over the picture, covering about a fifth of it.
	TArray<float> Flecks;
	Flecks.Init(0.0f, Size * Size);
	// (The review of build 1432: no glint could be found from playing height. They are larger now, two to four
	// pixels across there, and there are more of them.)
	for (int32 Fleck = 0; Fleck < 230; ++Fleck)
	{
		const float AtX = Dice.FRandRange(0.0f, static_cast<float>(Size)), AtY = Dice.FRandRange(0.0f, static_cast<float>(Size)), Reach = Dice.FRandRange(9.0f, 17.0f);
		const int32 Box = FMath::CeilToInt32(Reach);
		for (int32 DJ = -Box; DJ <= Box; ++DJ)
		{
			for (int32 DI = -Box; DI <= Box; ++DI)
			{
				const int32 I = FMath::FloorToInt32(AtX) + DI, J = FMath::FloorToInt32(AtY) + DJ;
				const float Off = FMath::Sqrt(FMath::Square(I + 0.5f - AtX) + FMath::Square(J + 0.5f - AtY)) / Reach;
				if (Off < 1.0f)
				{
					float& Here = Flecks[((J % Size + Size) % Size) * Size + (I % Size + Size) % Size];
					Here = FMath::Max(Here, FMath::Square(1.0f - Off * Off));
				}
			}
		}
	}
	OutPixels.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const int32 Index = J * Size + I;
			const float Slow = Layered((I + 0.5f) / Size, (J + 0.5f) / Size, 3, 3, 71u) * 0.5f + 0.5f;
			OutPixels[Index] = FColor(ToByte(Swell[Index] * 0.5f + 0.5f), ToByte(Chop[Index] * 0.5f + 0.5f), ToByte(Flecks[Index]), ToByte(Slow));
		}
	}
}

void CampaignTextures::MakeHaze(TArray<FColor>& OutPixels)
{
	// The picture is laid 2,920 of the table's units to a turn: a point of it is about four pixels from where he
	// plays. A band is about 400 pixels long there (a hundred points) and 90 to 160 deep at its head.
	TArray<float> Field;
	Field.Init(0.0f, Size * Size);
	FRandomStream Dice(1954);
	for (int32 Band = 0; Band < 90; ++Band)
	{
		const float HeadX = Dice.FRandRange(0.0f, static_cast<float>(Size)), HeadY = Dice.FRandRange(0.0f, static_cast<float>(Size));
		const float Long = Dice.FRandRange(82.0f, 124.0f), Deep = Dice.FRandRange(22.0f, 40.0f), Strength = Dice.FRandRange(0.6f, 1.0f);
		const float Cap = 0.08f; // (the blunt head: a rounded end a twelfth of the band's length)
		for (int32 J = FMath::FloorToInt32(HeadY - 0.5f * Deep) - 1; J <= FMath::CeilToInt32(HeadY + 0.5f * Deep) + 1; ++J)
		{
			for (int32 I = FMath::FloorToInt32(HeadX - Long) - 1; I <= FMath::CeilToInt32(HeadX + Cap * Long) + 1; ++I)
			{
				// (how far back from the head, 0 to 1 at the tail's end; less than nothing in the rounded end ahead of it)
				const float Back = (HeadX - (I + 0.5f)) / Long;
				if (Back < -Cap || Back >= 1.0f)
				{
					continue;
				}
				const float Half = 0.5f * Deep * (Back < 0.0f ? FMath::Sqrt(FMath::Max(1.0f - FMath::Square(Back / Cap), 0.0f)) : FMath::Pow(1.0f - Back, 0.65f));
				if (Half < 0.05f)
				{
					continue;
				}
				const float Across = FMath::Abs(J + 0.5f - HeadY) / Half;
				if (Across >= 1.0f)
				{
					continue;
				}
				Field[((J % Size + Size) % Size) * Size + (I % Size + Size) % Size] += Strength * (1.0f - Across * Across) * (Back < 0.0f ? 1.0f : FMath::Pow(1.0f - Back, 0.4f));
			}
		}
	}
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
			float& Here = Field[J * Size + I];
			// (lumps about every 150 pixels along a band, which ride with it; and under everything a slow unevenness, so
			// that the pockets between the bands are not one flat tone)
			Here *= 0.7f + 0.6f * (Layered(U, V, 13, 2, 151u) * 0.5f + 0.5f);
			Here += 0.14f * (Layered(U, V, 4, 3, 157u) * 0.5f + 0.5f);
		}
	}
	// Spread as the old wisps were (measured on the screen over France, 17 November 2026: the darkest twentieth at the
	// floor, the lower quarter at 0.34, the middle at 0.585, the upper quarter at 0.81, the palest twentieth whole).
	TArray<int32> Order;
	Order.SetNumUninitialized(Size * Size);
	for (int32 Index = 0; Index < Order.Num(); ++Index)
	{
		Order[Index] = Index;
	}
	Order.Sort([&Field](int32 A, int32 B) { return Field[A] < Field[B]; });
	static const float Shares[7] = { 0.0f, 0.05f, 0.25f, 0.5f, 0.75f, 0.95f, 1.0f }, Values[7] = { 0.0f, 0.0f, 0.34f, 0.585f, 0.81f, 1.0f, 1.0f };
	TArray<float> Spread;
	Spread.SetNumUninitialized(Size * Size);
	for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
	{
		const float Share = (Rank + 0.5f) / Order.Num();
		int32 Knot = 1;
		while (Knot < 6 && Shares[Knot] < Share)
		{
			++Knot;
		}
		Spread[Order[Rank]] = FMath::Lerp(Values[Knot - 1], Values[Knot], (Share - Shares[Knot - 1]) / (Shares[Knot] - Shares[Knot - 1]));
	}
	OutPixels.SetNumUninitialized(Size * Size);
	for (int32 Index = 0; Index < Size * Size; ++Index)
	{
		const uint8 Grey = ToByte(Spread[Index]);
		OutPixels[Index] = FColor(Grey, Grey, Grey, 255);
	}
}

void CampaignTextures::MakeWeather(TArray<FColor>& OutPixels, bool bSnow)
{
	TArray<float> Cloud, Mist;
	Cloud.SetNumUninitialized(Size * Size);
	Mist.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const float U = (I + 0.5f) / Size, V = (J + 0.5f) / Size;
			Cloud[J * Size + I] = Layered(U, V, 12, 3, 131u);
			Mist[J * Size + I] = Layered(U, V, 8, 3, 137u);
		}
	}
	// (the height under which a given share of a field lies: so the cloud covers what it is meant to, whatever the noise)
	auto Level = [](const TArray<float>& Field, float Share)
	{
		TArray<float> Sorted = Field;
		Sorted.Sort();
		return Sorted[FMath::Clamp(FMath::FloorToInt32(Share * Sorted.Num()), 0, Sorted.Num() - 1)];
	};
	const float CloudAt = Level(Cloud, 0.6f), MistAt = Level(Mist, 0.6f);
	// The rain: short soft streaks, each leaning its own way. The snow: small soft flakes.
	// (The art director's ruling of 25 November 2026 04:54Z, art-director/reviews/map-winter-sky-ruling.md: on a 1.5
	// still the falling weather read as RULED DASHES, 12 to 15 screen points long and all leaning the same way, because
	// every streak was 11 to 18 points of this picture long and every one leaned a third of its length to the same
	// side. Her targets: no straight edge longer than about twelve screen points, no two neighbours sharing a lean,
	// and the brightness spread inside a patch no more than a quarter of its mean. The picture is tiled about three
	// times across the screen (RainTiles), so a point of it is about two screen points: a streak is now 3 to 6 points
	// long, leans by a sixth to a half of its length ON THE SIDE OF THE FALL, and is drawn at a lower weight (SkyRain).
	// Flakes lose their square corners. She judges it on the next pictures. Her third target, as she amended it at
	// 05:36Z: the FALLING layer may ride the screen, because rain hangs in the air between the eye and the table; what
	// must be fixed to the ground is the LID'S BODY, the zone's edge and the cloud and mist shares, and that is the sky
	// picture, laid over the map through SkyFrame in map units. So nothing here asks the material to change.)
	FRandomStream Dice(1944);
	TArray<float> Falling;
	Falling.Init(0.0f, Size * Size);
	auto Dab = [&Falling](float X, float Y, float Much)
	{
		const int32 I0 = FMath::FloorToInt32(X), J0 = FMath::FloorToInt32(Y);
		const float PX = X - I0, PY = Y - J0;
		for (int32 DJ = 0; DJ < 2; ++DJ)
		{
			for (int32 DI = 0; DI < 2; ++DI)
			{
				float& Here = Falling[(((J0 + DJ) % Size + Size) % Size) * Size + ((I0 + DI) % Size + Size) % Size];
				Here = FMath::Max(Here, Much * (DI == 0 ? 1.0f - PX : PX) * (DJ == 0 ? 1.0f - PY : PY));
			}
		}
	};
	if (bSnow)
	{
		// (a flake is a middle with four half-lit neighbours a little way off: round at its edge, where a square of
		// four dabs gave it corners and a flat side)
		for (int32 Flake = 0; Flake < 1100; ++Flake)
		{
			const float AtX = Dice.FRandRange(0.0f, static_cast<float>(Size)), AtY = Dice.FRandRange(0.0f, static_cast<float>(Size)), Bright = Dice.FRandRange(0.45f, 0.85f);
			Dab(AtX, AtY, Bright);
			for (const FVector2f& Off : { FVector2f(0.8f, 0.0f), FVector2f(-0.8f, 0.0f), FVector2f(0.0f, 0.8f), FVector2f(0.0f, -0.8f) })
			{
				Dab(AtX + Off.X, AtY + Off.Y, Bright * 0.5f);
			}
		}
	}
	else
	{
		for (int32 Streak = 0; Streak < 900; ++Streak)
		{
			const float AtX = Dice.FRandRange(0.0f, static_cast<float>(Size)), AtY = Dice.FRandRange(0.0f, static_cast<float>(Size));
			const float Long = Dice.FRandRange(3.0f, 6.0f), Bright = Dice.FRandRange(0.4f, 0.75f);
			// (its own lean, all on the side of the fall: from about a sixth of its length to about half of it, so that
			// no two neighbours lean alike and every streak still leans the way its drop is falling.
			// The art director mended her own wording at 05:36Z: "no two neighbours sharing a lean" was badly put,
			// since a streak is the blur of a falling drop. The picture falls down the screen and a third as far
			// across it (RainFall), so a streak that leans against its own travel reads as confetti in motion. The
			// first build of this mend flipped half of them; her numbers, 0.15 to 0.55 and the old sign, do not.
			// Named Slant, not Lean: this file already has a Lean() of its own above, for the hillsides' shading.)
			const float Slant = Dice.FRandRange(0.15f, 0.55f);
			const int32 Steps = FMath::CeilToInt32(Long * 2.0f);
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				const float Along = static_cast<float>(Step) / Steps;
				Dab(AtX + Along * Long * Slant, AtY + Along * Long, Bright * FMath::Sin(Along * UE_PI));
			}
		}
	}
	OutPixels.SetNumUninitialized(Size * Size);
	for (int32 J = 0; J < Size; ++J)
	{
		for (int32 I = 0; I < Size; ++I)
		{
			const int32 Index = J * Size + I;
			// (soft at its edge and fuller toward its middle: a cloud, not a cut-out)
			const float Shape = FMath::SmoothStep(CloudAt - 0.07f, CloudAt + 0.2f, Cloud[Index]);
			const float Billow = Layered((I + 0.5f) / Size, (J + 0.5f) / Size, 36, 2, 139u) * 0.5f + 0.5f;
			const float Band = FMath::SmoothStep(MistAt - 0.1f, MistAt + 0.32f, Mist[Index]);
			OutPixels[Index] = FColor(ToByte(Shape), ToByte(Billow), ToByte(Band), ToByte(Falling[Index]));
		}
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
