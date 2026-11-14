#include "Campaign/CampaignTerrain.h"

#include "Campaign/CampaignMapData.h"
#include "TheaterCommand.h"
#include "Async/ParallelFor.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	// The coastline is drawn into a fine bitmap first, four times finer than the
	// height grid each way, so that every grid point can ask "how much of my
	// patch is land" and the shoreline comes out smooth instead of stepped.
	constexpr double PixelsPerCell = 4.0;

	constexpr float ShelfHeight = 0.55f; // how far the land stands above the water at the shore
	constexpr float SeaDepth = 2.0f; // how deep the bed drops away from the shore

	// The world's heights, read once and shared by every sheet of terrain: one for every WorldStep map units, the
	// first at the seam (30.45 W) and 71.4 N, wrapping round the world from east to west.
	TArray<int16> WorldHeights;
	int32 WorldWide = 0, WorldHigh = 0;
	float WorldStep = 2.0f;

	void LoadWorldHeights()
	{
		static bool bLooked = false;
		if (bLooked)
		{
			return;
		}
		bLooked = true;
		TArray<uint8> Bytes;
		const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign/elevation-world.bin"));
		if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || Bytes.Num() < 16 || FMemory::Memcmp(Bytes.GetData(), "TCE2", 4) != 0)
		{
			UE_LOG(LogTheater, Display, TEXT("No world elevation data at %s: there is no land beyond the map."), *Path);
			return;
		}
		uint32 FileWidth = 0, FileHeight = 0;
		FMemory::Memcpy(&FileWidth, Bytes.GetData() + 4, 4);
		FMemory::Memcpy(&FileHeight, Bytes.GetData() + 8, 4);
		FMemory::Memcpy(&WorldStep, Bytes.GetData() + 12, 4);
		if (FileWidth == 0 || FileHeight == 0 || WorldStep <= 0.0f || static_cast<int64>(Bytes.Num()) < 16 + static_cast<int64>(FileWidth) * FileHeight * 2)
		{
			return;
		}
		WorldWide = static_cast<int32>(FileWidth);
		WorldHigh = static_cast<int32>(FileHeight);
		WorldHeights.SetNumUninitialized(WorldWide * WorldHigh);
		FMemory::Memcpy(WorldHeights.GetData(), Bytes.GetData() + 16, WorldHeights.Num() * sizeof(int16));
	}

	/** The world's height at a point of the world's own frame (x east from the seam, round and round; y south from 71.4 N), in metres. */
	float WorldMetresAt(double X, double Y)
	{
		if (WorldWide <= 1)
		{
			return 0.0f;
		}
		const double Gx = X / WorldStep, Gy = FMath::Clamp(Y / WorldStep, 0.0, static_cast<double>(WorldHigh - 1));
		const int32 X0 = FMath::FloorToInt32(Gx), Y0 = FMath::Min(FMath::FloorToInt32(Gy), WorldHigh - 2);
		const float Fx = static_cast<float>(Gx - X0), Fy = static_cast<float>(Gy - Y0);
		const int32 XA = ((X0 % WorldWide) + WorldWide) % WorldWide, XB = (XA + 1) % WorldWide;
		const int16* Row0 = WorldHeights.GetData() + Y0 * WorldWide;
		const int16* Row1 = Row0 + WorldWide;
		return FMath::Lerp(FMath::Lerp(static_cast<float>(Row0[XA]), static_cast<float>(Row0[XB]), Fx),
			FMath::Lerp(static_cast<float>(Row1[XA]), static_cast<float>(Row1[XB]), Fx), Fy);
	}

	/** Fills a ring into the fine bitmap. The ring is in map units; the bitmap's corner stands at (OffX, OffY). */
	void FillRing(const TArray<FVector2D>& InRing, double Px, int32 RW, int32 RH, int16 Value, TArray<int16>& Raster, double OffX = 0.0, double OffY = 0.0)
	{
		TArray<FVector2D> Moved;
		if (OffX != 0.0 || OffY != 0.0)
		{
			Moved.Reserve(InRing.Num());
			for (const FVector2D& P : InRing)
			{
				Moved.Add(FVector2D(P.X - OffX, P.Y - OffY));
			}
		}
		const TArray<FVector2D>& Ring = Moved.Num() > 0 ? Moved : InRing;
		double MinY = TNumericLimits<double>::Max(), MaxY = -TNumericLimits<double>::Max();
		for (const FVector2D& P : Ring)
		{
			MinY = FMath::Min(MinY, P.Y);
			MaxY = FMath::Max(MaxY, P.Y);
		}
		if (MaxY < 0.0 || MinY > RH * Px)
		{
			return; // wholly off the bitmap
		}
		const int32 Y0 = FMath::Clamp(FMath::FloorToInt32(MinY / Px - 0.5), 0, RH - 1);
		const int32 Y1 = FMath::Clamp(FMath::CeilToInt32(MaxY / Px - 0.5), 0, RH - 1);
		TArray<double, TInlineAllocator<32>> Crossings;
		for (int32 Y = Y0; Y <= Y1; ++Y)
		{
			const double Sy = (Y + 0.5) * Px;
			Crossings.Reset();
			for (int32 i = 0, n = Ring.Num(); i < n; ++i)
			{
				const FVector2D& A = Ring[i];
				const FVector2D& B = Ring[(i + 1) % n];
				if ((A.Y > Sy) != (B.Y > Sy))
				{
					Crossings.Add(A.X + (Sy - A.Y) * (B.X - A.X) / (B.Y - A.Y));
				}
			}
			Crossings.Sort();
			for (int32 k = 0; k + 1 < Crossings.Num(); k += 2)
			{
				const int32 X0 = FMath::Max(FMath::CeilToInt32(Crossings[k] / Px - 0.5), 0);
				const int32 X1 = FMath::Min(FMath::FloorToInt32(Crossings[k + 1] / Px - 0.5), RW - 1);
				int16* Row = Raster.GetData() + Y * RW;
				for (int32 X = X0; X <= X1; ++X)
				{
					Row[X] = Value;
				}
			}
		}
	}

	float Fbm(FVector2D P, int32 Octaves)
	{
		float Sum = 0.0f, Amp = 0.5f, Total = 0.0f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			Sum += Amp * FMath::PerlinNoise2D(P);
			Total += Amp;
			Amp *= 0.5f;
			P = P * 2.03 + FVector2D(17.3, -9.1);
		}
		return Sum / Total; // about -1..1
	}

	float RidgedFbm(FVector2D P, int32 Octaves)
	{
		float Sum = 0.0f, Amp = 0.5f, Total = 0.0f, Carry = 1.0f;
		for (int32 O = 0; O < Octaves; ++O)
		{
			float Ridge = 1.0f - FMath::Abs(FMath::PerlinNoise2D(P));
			Ridge *= Ridge;
			Sum += Amp * Ridge * Carry;
			Total += Amp;
			Carry = FMath::Clamp(Ridge * 1.6f, 0.0f, 1.0f); // fine ridges only where the big ridges are
			Amp *= 0.5f;
			P = P * 2.07 + FVector2D(-31.7, 12.9);
		}
		return Sum / Total; // 0..1
	}

	/** How strongly each kind of country is modelled in relief, in map units. */
	float ReliefFor(const FString& Terrain)
	{
		if (Terrain == TEXT("mountain")) return 7.0f;
		if (Terrain == TEXT("hills")) return 2.1f;
		if (Terrain == TEXT("forest")) return 0.55f;
		if (Terrain == TEXT("desert")) return 0.5f;
		if (Terrain == TEXT("urban")) return 0.25f;
		if (Terrain == TEXT("marsh")) return 0.1f;
		return 0.3f; // plains
	}
}

// Two passes is close to a tent, three to a bell.
void FCampaignTerrain::Smooth(TArray<float>& Field, int32 Columns, int32 Rows, int32 Radius, int32 Passes)
{
	if (Radius <= 0)
	{
		return;
	}
	TArray<float> Temp;
	Temp.SetNumUninitialized(Field.Num());
	const float Norm = 1.0f / (2 * Radius + 1);
	for (int32 Pass = 0; Pass < Passes; ++Pass)
	{
		for (int32 J = 0; J < Rows; ++J)
		{
			const float* Src = Field.GetData() + J * Columns;
			float* Dst = Temp.GetData() + J * Columns;
			float Sum = 0.0f;
			for (int32 K = -Radius; K <= Radius; ++K)
			{
				Sum += Src[FMath::Clamp(K, 0, Columns - 1)];
			}
			for (int32 I = 0; I < Columns; ++I)
			{
				Dst[I] = Sum * Norm;
				Sum += Src[FMath::Min(I + Radius + 1, Columns - 1)] - Src[FMath::Max(I - Radius, 0)];
			}
		}
		for (int32 I = 0; I < Columns; ++I)
		{
			float Sum = 0.0f;
			for (int32 K = -Radius; K <= Radius; ++K)
			{
				Sum += Temp[FMath::Clamp(K, 0, Rows - 1) * Columns + I];
			}
			for (int32 J = 0; J < Rows; ++J)
			{
				Field[J * Columns + I] = Sum * Norm;
				Sum += Temp[FMath::Min(J + Radius + 1, Rows - 1) * Columns + I] - Temp[FMath::Max(J - Radius, 0) * Columns + I];
			}
		}
	}
}

void FCampaignTerrain::LoadElevation()
{
	Elevation.Reset();
	ElevationWidth = ElevationHeight = 0;
	TArray<uint8> Bytes;
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign/elevation.bin"));
	if (!FFileHelper::LoadFileToArray(Bytes, *Path, FILEREAD_Silent) || Bytes.Num() < 12 || FMemory::Memcmp(Bytes.GetData(), "TCE1", 4) != 0)
	{
		UE_LOG(LogTheater, Display, TEXT("No elevation data at %s: the relief will be invented from each region's kind of country."), *Path);
		return;
	}
	uint32 FileWidth = 0, FileHeight = 0;
	FMemory::Memcpy(&FileWidth, Bytes.GetData() + 4, 4);
	FMemory::Memcpy(&FileHeight, Bytes.GetData() + 8, 4);
	if (FileWidth == 0 || FileHeight == 0 || static_cast<int64>(Bytes.Num()) < 12 + static_cast<int64>(FileWidth) * FileHeight * 2)
	{
		return;
	}
	ElevationWidth = static_cast<int32>(FileWidth);
	ElevationHeight = static_cast<int32>(FileHeight);
	Elevation.SetNumUninitialized(ElevationWidth * ElevationHeight);
	FMemory::Memcpy(Elevation.GetData(), Bytes.GetData() + 12, Elevation.Num() * sizeof(int16));
}

float FCampaignTerrain::MetresAt(double X, double Y) const
{
	// The map's own heights where they reach, and beyond them the world's.
	if (WorldShiftX >= 0.0 && (ElevationWidth <= 1 || X < 0.0 || Y < 0.0 || X > ElevationWidth - 1 || Y > ElevationHeight - 1))
	{
		return WorldMetresAt(X + WorldShiftX, Y);
	}
	if (ElevationWidth <= 1)
	{
		return 0.0f;
	}
	const double Gx = FMath::Clamp(X, 0.0, static_cast<double>(ElevationWidth - 1));
	const double Gy = FMath::Clamp(Y, 0.0, static_cast<double>(ElevationHeight - 1));
	const int32 X0 = FMath::Min(FMath::FloorToInt32(Gx), ElevationWidth - 2);
	const int32 Y0 = FMath::Min(FMath::FloorToInt32(Gy), ElevationHeight - 2);
	const float Fx = static_cast<float>(Gx - X0), Fy = static_cast<float>(Gy - Y0);
	const int16* Row0 = Elevation.GetData() + Y0 * ElevationWidth + X0;
	const int16* Row1 = Row0 + ElevationWidth;
	return FMath::Lerp(FMath::Lerp(static_cast<float>(Row0[0]), static_cast<float>(Row0[1]), Fx),
		FMath::Lerp(static_cast<float>(Row1[0]), static_cast<float>(Row1[1]), Fx), Fy);
}

void FCampaignTerrain::Build(const FCampaignMapData& Data)
{
	BuildOver(Data, 0.0, 0.0, Data.Width, Data.Height, 1.5, false);
}

void FCampaignTerrain::BuildOver(const FCampaignMapData& Data, double MinX, double MinY, double Wide, double High, double InCell, bool bLandFromHeights)
{
	const double StartTime = FPlatformTime::Seconds();
	LoadElevation();
	OriginX = MinX;
	OriginY = MinY;
	ExtentX = Wide;
	ExtentY = High;
	Cell = InCell;
	WorldShiftX = -1.0;
	if (Data.bWorld)
	{
		LoadWorldHeights();
		WorldShiftX = WorldWide > 1 ? Data.ShiftX : -1.0;
	}

	NX = FMath::CeilToInt32(Wide / Cell) + 1;
	NY = FMath::CeilToInt32(High / Cell) + 1;
	const int32 Count = NX * NY;

	// 1. Paint land and regions into the fine bitmap.
	const double Px = Cell / PixelsPerCell;
	const int32 RW = FMath::CeilToInt32(Wide / Px);
	const int32 RH = FMath::CeilToInt32(High / Px);
	TArray<int16> Raster;
	Raster.Init(Sea, RW * RH);
	if (bLandFromHeights && WorldShiftX >= 0.0)
	{
		// Beyond the map's own box no coast is drawn yet: the land there is wherever the world's heights stand above the sea.
		ParallelFor(RH, [&](int32 Y)
		{
			const double My = MinY + (Y + 0.5) * Px;
			int16* Row = Raster.GetData() + Y * RW;
			for (int32 X = 0; X < RW; ++X)
			{
				const double Mx = MinX + (X + 0.5) * Px;
				if ((Mx < 0.0 || My < 0.0 || Mx > Data.Width || My > Data.Height) && WorldMetresAt(Mx + WorldShiftX, My) > 0.0f)
				{
					Row[X] = UnclaimedLand;
				}
			}
		});
	}
	for (const TArray<FVector2D>& Ring : Data.Land)
	{
		FillRing(Ring, Px, RW, RH, UnclaimedLand, Raster, MinX, MinY);
	}
	for (const FCampaignRegion& R : Data.Regions)
	{
		for (const TArray<FVector2D>& Ring : R.Polygons)
		{
			FillRing(Ring, Px, RW, RH, static_cast<int16>(R.Id), Raster, MinX, MinY);
		}
	}

	// 2. For every grid point: how much of its patch is land, and whose.
	Coverage.SetNumUninitialized(Count);
	Region.SetNumUninitialized(Count);
	const int32 Half = FMath::RoundToInt32(PixelsPerCell / 2.0);
	const float PatchNorm = 1.0f / (4 * Half * Half);
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Cx = FMath::RoundToInt32(I * PixelsPerCell);
			const int32 Cy = FMath::RoundToInt32(J * PixelsPerCell);
			int32 LandCount = 0;
			int16 AnyLand = Sea;
			for (int32 Y = Cy - Half; Y < Cy + Half; ++Y)
			{
				const int16* Row = Raster.GetData() + FMath::Clamp(Y, 0, RH - 1) * RW;
				for (int32 X = Cx - Half; X < Cx + Half; ++X)
				{
					const int16 V = Row[FMath::Clamp(X, 0, RW - 1)];
					if (V != Sea)
					{
						++LandCount;
						if (AnyLand < 0 || V >= 0)
						{
							AnyLand = V;
						}
					}
				}
			}
			const int16 Centre = Raster[FMath::Clamp(Cy, 0, RH - 1) * RW + FMath::Clamp(Cx, 0, RW - 1)];
			Coverage[Index(I, J)] = LandCount * PatchNorm;
			Region[Index(I, J)] = (Centre != Sea) ? Centre : AnyLand;
		}
	}
	Raster.Empty();

	// 3. Islands and enclaves too small for the grid to see (Malta, Gibraltar)
	// get a small mound of their own so they are on the table and can be clicked.
	for (const FCampaignRegion& R : Data.Regions)
	{
		if (R.Area <= 0.0 || R.Area > 40.0)
		{
			continue;
		}
		const double Radius = FMath::Max(2.6, FMath::Sqrt(R.Area / UE_DOUBLE_PI) + 1.4);
		const int32 I0 = FMath::Max(FMath::FloorToInt32((R.Centroid.X - OriginX - Radius) / Cell), 0);
		const int32 I1 = FMath::Min(FMath::CeilToInt32((R.Centroid.X - OriginX + Radius) / Cell), NX - 1);
		const int32 J0 = FMath::Max(FMath::FloorToInt32((R.Centroid.Y - OriginY - Radius) / Cell), 0);
		const int32 J1 = FMath::Min(FMath::CeilToInt32((R.Centroid.Y - OriginY + Radius) / Cell), NY - 1);
		for (int32 J = J0; J <= J1; ++J)
		{
			for (int32 I = I0; I <= I1; ++I)
			{
				const double D = FVector2D::Distance(PointAt(I, J), R.Centroid);
				const float Mound = 1.0f - FMath::SmoothStep(static_cast<float>(Radius * 0.55), static_cast<float>(Radius), static_cast<float>(D));
				const int32 Idx = Index(I, J);
				if (Mound > Coverage[Idx])
				{
					Coverage[Idx] = Mound;
					if (Region[Idx] < 0 && Mound > 0.3f)
					{
						Region[Idx] = static_cast<int16>(R.Id);
					}
				}
			}
		}
	}

	// 4. Smooth the shoreline a little, then measure how far every point is
	// from it: outward over the sea and inward over the land.
	Smooth(Coverage, NX, NY, 1, 1);
	{
		TArray<float> FromSea, FromLand;
		FromSea.Init(1.0e6f, Count);
		FromLand.Init(1.0e6f, Count);
		for (int32 Idx = 0; Idx < Count; ++Idx)
		{
			(Coverage[Idx] >= 0.5f ? FromLand : FromSea)[Idx] = 0.0f;
		}
		SpreadDistance(FromSea); // for land points: cells to the nearest water
		SpreadDistance(FromLand); // for sea points: cells to the nearest land
		ShoreDistance.SetNumUninitialized(Count);
		for (int32 Idx = 0; Idx < Count; ++Idx)
		{
			// The shoreline itself runs between a land point and its sea neighbour, about half a cell from each.
			ShoreDistance[Idx] = Coverage[Idx] >= 0.5f
				? static_cast<float>((FromSea[Idx] - 0.5f) * Cell)
				: static_cast<float>(-(FromLand[Idx] - 0.5f) * Cell);
		}
	}

	// 5. How rough the country is, spread across region boundaries so that
	// mountains rise out of the plain instead of stopping at a line.
	Roughness.SetNumUninitialized(Count);
	TArray<float> RegionRelief;
	RegionRelief.SetNum(Data.Regions.Num());
	for (const FCampaignRegion& R : Data.Regions)
	{
		RegionRelief[R.Id] = ReliefFor(R.Terrain);
	}
	for (int32 Idx = 0; Idx < Count; ++Idx)
	{
		const int16 Id = Region[Idx];
		Roughness[Idx] = Id >= 0 ? RegionRelief[Id] : (Id == UnclaimedLand ? 0.6f : 0.0f);
	}
	Smooth(Roughness, NX, NY, FMath::RoundToInt32(10.0 / Cell), 2);

	// 6. Heights.
	Height.SetNumUninitialized(Count);
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Index(I, J);
			const float C = Coverage[Idx];
			float H;
			if (C >= 0.5f && HasElevation())
			{
				// The real ground. Heights are raised the way a relief map raises them: low hills more than
				// in proportion, so that they can be seen at all, and the great ranges less, so that they
				// do not tower over the pieces. 2,500 metres stands about five and a half map units high.
				const FVector2D P = PointAt(I, J);
				float Metres = FMath::Max(MetresAt(P.X, P.Y), 0.0f);
				if (Cell > 2.5)
				{
					// (a coarse sheet's point stands for the ground round it, not for the one spot it falls on)
					const double Third = Cell / 3.0;
					for (const FVector2D& Tap : { FVector2D(-Third, 0.0), FVector2D(Third, 0.0), FVector2D(0.0, -Third), FVector2D(0.0, Third) })
					{
						Metres += FMath::Max(MetresAt(P.X + Tap.X, P.Y + Tap.Y), 0.0f);
					}
					Metres /= 5.0f;
				}
				const float Lift = 5.6f * FMath::Pow(Metres / 2500.0f, 0.78f);
				// A little tooth on the slopes, more of it the higher the country, so they do not look moulded in glass.
				const float Tooth = (0.05f + 0.045f * Lift) * Fbm(P * 0.23, 3);
				H = ShelfHeight * FMath::SmoothStep(0.5f, 0.92f, C)
					+ FMath::Max(Lift + Tooth, 0.0f) * FMath::SmoothStep(0.2f, 2.6f, ShoreDistance[Idx]);
			}
			else if (C >= 0.5f)
			{
				H = ShelfHeight * FMath::SmoothStep(0.5f, 0.92f, C);
				const float Fade = FMath::SmoothStep(1.0f, 8.0f, ShoreDistance[Idx]);
				if (Fade > 0.0f)
				{
					const FVector2D P = PointAt(I, J);
					const float Amp = Roughness[Idx];
					const float Rolling = 0.5f + 0.34f * Fbm(P * 0.033, 3) + 0.16f * Fbm(P * 0.12 + FVector2D(40.0, 7.0), 3);
					// Ridges are sharpened so that a range reads as crests and valleys, not as a dome.
					const float Ridged = 0.12f + 0.88f * FMath::Pow(RidgedFbm(P * 0.058, 5), 1.35f);
					const float Mountain = FMath::Clamp((Amp - 0.9f) / 2.6f, 0.0f, 1.0f);
					H += Amp * FMath::Lerp(Rolling, Ridged, Mountain) * Fade;
				}
			}
			else
			{
				H = -SeaDepth * FMath::SmoothStep(0.0f, 1.0f, (0.5f - C) * 2.0f);
			}
			Height[Idx] = H;
		}
	}

	MeasureFrontiers(Data);

	UE_LOG(LogTheater, Log, TEXT("Terrain: %d x %d grid points built in %.2f s"), NX, NY, FPlatformTime::Seconds() - StartTime);
}

void FCampaignTerrain::MeasureFrontiers(const FCampaignMapData& Data)
{
	BorderDistance.Init(1.0e6f, NX * NY);
	auto OwnerOf = [&](int32 Idx) -> const FString*
	{
		const int16 Id = Region[Idx];
		return Id >= 0 ? &Data.Regions[Id].Owner : nullptr;
	};
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			const int32 Idx = Index(I, J);
			const FString* Mine = OwnerOf(Idx);
			if (!Mine)
			{
				continue;
			}
			const FString* Right = I + 1 < NX ? OwnerOf(Idx + 1) : nullptr;
			const FString* Down = J + 1 < NY ? OwnerOf(Idx + NX) : nullptr;
			if (Right && *Right != *Mine)
			{
				BorderDistance[Idx] = 0.0f;
				BorderDistance[Idx + 1] = 0.0f;
			}
			if (Down && *Down != *Mine)
			{
				BorderDistance[Idx] = 0.0f;
				BorderDistance[Idx + NX] = 0.0f;
			}
		}
	}
	SpreadDistance(BorderDistance);
}

void FCampaignTerrain::SpreadDistance(TArray<float>& Field) const
{
	const float Straight = 1.0f, Diagonal = 1.4142f;
	for (int32 J = 0; J < NY; ++J)
	{
		for (int32 I = 0; I < NX; ++I)
		{
			float& D = Field[Index(I, J)];
			if (I > 0) D = FMath::Min(D, Field[Index(I - 1, J)] + Straight);
			if (J > 0) D = FMath::Min(D, Field[Index(I, J - 1)] + Straight);
			if (I > 0 && J > 0) D = FMath::Min(D, Field[Index(I - 1, J - 1)] + Diagonal);
			if (I + 1 < NX && J > 0) D = FMath::Min(D, Field[Index(I + 1, J - 1)] + Diagonal);
		}
	}
	for (int32 J = NY - 1; J >= 0; --J)
	{
		for (int32 I = NX - 1; I >= 0; --I)
		{
			float& D = Field[Index(I, J)];
			if (I + 1 < NX) D = FMath::Min(D, Field[Index(I + 1, J)] + Straight);
			if (J + 1 < NY) D = FMath::Min(D, Field[Index(I, J + 1)] + Straight);
			if (I + 1 < NX && J + 1 < NY) D = FMath::Min(D, Field[Index(I + 1, J + 1)] + Diagonal);
			if (I > 0 && J + 1 < NY) D = FMath::Min(D, Field[Index(I - 1, J + 1)] + Diagonal);
		}
	}
}

FVector2f FCampaignTerrain::GradientAt(int32 I, int32 J) const
{
	const int32 IL = FMath::Max(I - 1, 0), IR = FMath::Min(I + 1, NX - 1);
	const int32 JU = FMath::Max(J - 1, 0), JD = FMath::Min(J + 1, NY - 1);
	const float Dx = (Height[Index(IR, J)] - Height[Index(IL, J)]) / static_cast<float>((IR - IL) * Cell);
	const float Dy = (Height[Index(I, JD)] - Height[Index(I, JU)]) / static_cast<float>((JD - JU) * Cell);
	return FVector2f(Dx, Dy);
}

float FCampaignTerrain::Sample(const TArray<float>& Field, double X, double Y) const
{
	if (Field.Num() == 0)
	{
		return 0.0f;
	}
	const double Gx = FMath::Clamp((X - OriginX) / Cell, 0.0, static_cast<double>(NX - 1));
	const double Gy = FMath::Clamp((Y - OriginY) / Cell, 0.0, static_cast<double>(NY - 1));
	const int32 I0 = FMath::Min(FMath::FloorToInt32(Gx), NX - 2);
	const int32 J0 = FMath::Min(FMath::FloorToInt32(Gy), NY - 2);
	const float Fx = static_cast<float>(Gx - I0);
	const float Fy = static_cast<float>(Gy - J0);
	const float A = Field[Index(I0, J0)], B = Field[Index(I0 + 1, J0)];
	const float C = Field[Index(I0, J0 + 1)], D = Field[Index(I0 + 1, J0 + 1)];
	return FMath::Lerp(FMath::Lerp(A, B, Fx), FMath::Lerp(C, D, Fx), Fy);
}
