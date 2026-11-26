#include "Campaign/CampaignClimate.h"

#include "TheaterCommand.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* const LookNames[WorldLook::Count] = {
		TEXT("temperate"), TEXT("drysouth"), TEXT("prairie"), TEXT("steppe"), TEXT("desert"), TEXT("savanna"), TEXT("jungle"),
		TEXT("monsoon"), TEXT("dryplain"), TEXT("taiga"), TEXT("tundra"), TEXT("ice"), TEXT("alpine"),
	};

	/** A box blur that runs right round the world from east to west and stops at the top and the bottom. */
	void BlurRound(TArray<float>& Field, int32 Columns, int32 Rows, int32 Radius, int32 Passes)
	{
		TArray<float> Temp;
		Temp.SetNumUninitialized(Field.Num());
		const float Norm = 1.0f / (2 * Radius + 1);
		for (int32 Pass = 0; Pass < Passes; ++Pass)
		{
			for (int32 J = 0; J < Rows; ++J)
			{
				const float* Src = Field.GetData() + J * Columns;
				float* Dst = Temp.GetData() + J * Columns;
				for (int32 I = 0; I < Columns; ++I)
				{
					float Sum = 0.0f;
					for (int32 K = -Radius; K <= Radius; ++K)
					{
						Sum += Src[((I + K) % Columns + Columns) % Columns];
					}
					Dst[I] = Sum * Norm;
				}
			}
			for (int32 J = 0; J < Rows; ++J)
			{
				for (int32 I = 0; I < Columns; ++I)
				{
					float Sum = 0.0f;
					for (int32 K = -Radius; K <= Radius; ++K)
					{
						Sum += Temp[FMath::Clamp(J + K, 0, Rows - 1) * Columns + I];
					}
					Field[J * Columns + I] = Sum * Norm;
				}
			}
		}
	}
}

const FCampaignClimate& FCampaignClimate::Get()
{
	static FCampaignClimate Table;
	static bool bLooked = false;
	if (!bLooked)
	{
		bLooked = true;
		Table.Load();
	}
	return Table;
}

void FCampaignClimate::Load()
{
	const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign/world-climate-boxes.json"));
	FString Text;
	TSharedPtr<FJsonObject> Root;
	if (!FFileHelper::LoadFileToString(Text, *Path) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
	{
		UE_LOG(LogTheater, Display, TEXT("No climate table at %s: the ground beyond the map is all one look."), *Path);
		return;
	}
	const TArray<TSharedPtr<FJsonValue>>* Boxes = nullptr;
	if (!Root->TryGetArrayField(TEXT("boxes"), Boxes) || Boxes->Num() == 0)
	{
		return;
	}

	// Which look each half degree takes: the last box that holds it. (And the blur and the warp of that box, in
	// degrees: two and three unless the box carries its own. The art director's round 32: the desert coast of Peru
	// and Chile is a strip a degree or two wide between the sea and the Andes, and must not spill east of the crest
	// into the forest of the Amazon, so its boxes ask for one and one.)
	TArray<int8> Winner;
	Winner.Init(static_cast<int8>(WorldLook::Temperate), Wide * High);
	TArray<float> BlurDegrees, WarpDegrees;
	BlurDegrees.Init(2.0f, Wide * High);
	WarpDegrees.Init(3.0f, Wide * High);
	int32 Read = 0;
	for (const TSharedPtr<FJsonValue>& Value : *Boxes)
	{
		const TSharedPtr<FJsonObject> Box = Value->AsObject();
		FString Name;
		double West = 0.0, East = 0.0, Low = 0.0, Top = 0.0;
		if (!Box.IsValid() || !Box->TryGetStringField(TEXT("look"), Name) || !Box->TryGetNumberField(TEXT("west"), West) || !Box->TryGetNumberField(TEXT("east"), East)
			|| !Box->TryGetNumberField(TEXT("south"), Low) || !Box->TryGetNumberField(TEXT("north"), Top))
		{
			continue;
		}
		int32 Look = INDEX_NONE;
		for (int32 K = 0; K < WorldLook::Count; ++K)
		{
			if (Name == LookNames[K])
			{
				Look = K;
			}
		}
		if (Look == INDEX_NONE)
		{
			UE_LOG(LogTheater, Warning, TEXT("The climate table names a look the game does not know: %s"), *Name);
			continue;
		}
		++Read;
		double BoxBlur = 2.0, BoxWarp = 3.0;
		Box->TryGetNumberField(TEXT("blur"), BoxBlur);
		Box->TryGetNumberField(TEXT("warp"), BoxWarp);
		for (int32 J = 0; J < High; ++J)
		{
			const double Latitude = South + (J + 0.5) / PerDegree;
			if (Latitude < Low || Latitude > Top)
			{
				continue;
			}
			for (int32 I = 0; I < Wide; ++I)
			{
				// (a box may run past 180 east: the place is tried as it is and one turn of the world on)
				const double Longitude = -180.0 + (I + 0.5) / PerDegree;
				if ((Longitude >= West && Longitude <= East) || (Longitude + 360.0 >= West && Longitude + 360.0 <= East))
				{
					Winner[J * Wide + I] = static_cast<int8>(Look);
					BlurDegrees[J * Wide + I] = static_cast<float>(BoxBlur);
					WarpDegrees[J * Wide + I] = static_cast<float>(BoxWarp);
				}
			}
		}
	}

	// Each look's share of every half degree, with its edges blurred by about two degrees: or by its own box's
	// blur, where the box carries one. Each blur is done by itself and the results are added, so that a narrow
	// strip with a small blur keeps to its strip.
	TArray<int32> Radii;
	TArray<int32> RadiusOf;
	RadiusOf.SetNumUninitialized(Wide * High);
	for (int32 Idx = 0; Idx < Wide * High; ++Idx)
	{
		RadiusOf[Idx] = FMath::Clamp(FMath::RoundToInt32(BlurDegrees[Idx] * PerDegree), 1, 12);
		Radii.AddUnique(RadiusOf[Idx]);
	}
	TArray<float> Part;
	for (int32 K = 0; K < WorldLook::Count; ++K)
	{
		Share[K].Init(0.0f, Wide * High);
		for (const int32 Radius : Radii)
		{
			bool bAny = false;
			Part.Init(0.0f, Wide * High);
			for (int32 Idx = 0; Idx < Wide * High; ++Idx)
			{
				if (Winner[Idx] == K && RadiusOf[Idx] == Radius)
				{
					Part[Idx] = 1.0f;
					bAny = true;
				}
			}
			if (!bAny)
			{
				continue;
			}
			BlurRound(Part, Wide, High, Radius, 2);
			for (int32 Idx = 0; Idx < Wide * High; ++Idx)
			{
				Share[K][Idx] += Part[Idx];
			}
		}
	}
	// How far a place's reading may be warped, in degrees: the least any box within three degrees asks for, so that
	// a place beside a narrow strip is not carried into it, and then softened so that it changes by degrees.
	Warp.SetNumUninitialized(Wide * High);
	{
		const int32 Reach = 3 * PerDegree;
		TArray<float> Rows;
		Rows.SetNumUninitialized(Wide * High);
		for (int32 J = 0; J < High; ++J)
		{
			for (int32 I = 0; I < Wide; ++I)
			{
				float Least = 1.0e9f;
				for (int32 D = -Reach; D <= Reach; ++D)
				{
					Least = FMath::Min(Least, WarpDegrees[J * Wide + ((I + D) % Wide + Wide) % Wide]);
				}
				Rows[J * Wide + I] = Least;
			}
		}
		for (int32 J = 0; J < High; ++J)
		{
			for (int32 I = 0; I < Wide; ++I)
			{
				float Least = 1.0e9f;
				for (int32 D = -Reach; D <= Reach; ++D)
				{
					Least = FMath::Min(Least, Rows[FMath::Clamp(J + D, 0, High - 1) * Wide + I]);
				}
				Warp[J * Wide + I] = Least;
			}
		}
		BlurRound(Warp, Wide, High, PerDegree, 1);
	}
	bLoaded = true;
	UE_LOG(LogTheater, Log, TEXT("Climate: %d boxes of %d read from the world's table"), Read, Boxes->Num());
}

void FCampaignClimate::SharesAt(double Latitude, double Longitude, float OutShares[WorldLook::Count]) const
{
	if (!bLoaded)
	{
		for (int32 K = 0; K < WorldLook::Count; ++K)
		{
			OutShares[K] = K == WorldLook::Temperate ? 1.0f : 0.0f;
		}
		return;
	}
	// The edges are warped by about three degrees, so that no look ends on a ruled line. The warp is read round a
	// cylinder, so that it runs on unbroken right round the world.
	const double Turn = FMath::DegreesToRadians(Longitude);
	const FVector Round(FMath::Cos(Turn) * 6.4, FMath::Sin(Turn) * 6.4, Latitude / 9.0);
	// (by about three degrees, or by as little as a box close by asks for)
	const int32 WarpX = ((FMath::FloorToInt32((Longitude + 180.0) * PerDegree) % Wide) + Wide) % Wide;
	const int32 WarpY = FMath::Clamp(FMath::FloorToInt32((Latitude - South) * PerDegree), 0, High - 1);
	const double Reach = Warp.Num() == Wide * High ? Warp[WarpY * Wide + WarpX] : 3.0;
	const double WarpedLatitude = Latitude + Reach * FMath::PerlinNoise3D(Round);
	const double WarpedLongitude = Longitude + Reach * FMath::PerlinNoise3D(Round + FVector(31.7, 11.3, 5.9));

	const double Gx = (WarpedLongitude + 180.0) * PerDegree - 0.5;
	const double Gy = FMath::Clamp((WarpedLatitude - South) * PerDegree - 0.5, 0.0, static_cast<double>(High - 1));
	const int32 X0 = FMath::FloorToInt32(Gx), Y0 = FMath::Min(FMath::FloorToInt32(Gy), High - 2);
	const float Fx = static_cast<float>(Gx - X0), Fy = static_cast<float>(Gy - Y0);
	const int32 XA = ((X0 % Wide) + Wide) % Wide, XB = (XA + 1) % Wide;
	float Sum = 0.0f;
	for (int32 K = 0; K < WorldLook::Count; ++K)
	{
		const float* Row0 = Share[K].GetData() + Y0 * Wide;
		const float* Row1 = Row0 + Wide;
		OutShares[K] = FMath::Lerp(FMath::Lerp(Row0[XA], Row0[XB], Fx), FMath::Lerp(Row1[XA], Row1[XB], Fx), Fy);
		Sum += OutShares[K];
	}
	if (Sum > 1.0e-4f)
	{
		for (int32 K = 0; K < WorldLook::Count; ++K)
		{
			OutShares[K] /= Sum;
		}
	}
	else
	{
		OutShares[WorldLook::Temperate] = 1.0f;
	}
}
