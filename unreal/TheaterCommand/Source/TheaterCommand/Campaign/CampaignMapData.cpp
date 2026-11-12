#include "Campaign/CampaignMapData.h"

#include "TheaterCommand.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	/**
	 * Campaign data lives in one place: the repository's own data/ and
	 * src/generated/ folders, which the browser prototype and its tests also
	 * read. A packaged game cannot reach outside its own folder, so a copy in
	 * Content/Data/Campaign wins when it exists.
	 */
	FString ResolveDataFile(const FString& FileName, const FString& RepoRelativeDir)
	{
		const FString Packaged = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign"), FileName);
		if (IFileManager::Get().FileExists(*Packaged))
		{
			return Packaged;
		}
		const FString Repo = FPaths::Combine(FPaths::ProjectDir(), TEXT("../.."), RepoRelativeDir, FileName);
		return FPaths::ConvertRelativePathToFull(Repo);
	}

	bool ReadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& OutObject, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("Could not read %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, OutObject) || !OutObject.IsValid())
		{
			OutError = FString::Printf(TEXT("%s is not valid JSON"), *Path);
			return false;
		}
		return true;
	}

	bool ReadRing(const TArray<TSharedPtr<FJsonValue>>& Points, TArray<FVector2D>& OutRing)
	{
		OutRing.Reset(Points.Num());
		for (const TSharedPtr<FJsonValue>& Point : Points)
		{
			const TArray<TSharedPtr<FJsonValue>>* Pair = nullptr;
			if (!Point->TryGetArray(Pair) || Pair->Num() < 2)
			{
				return false;
			}
			OutRing.Emplace((*Pair)[0]->AsNumber(), (*Pair)[1]->AsNumber());
		}
		return OutRing.Num() >= 3;
	}

	double RingArea(const TArray<FVector2D>& Ring)
	{
		double Sum = 0.0;
		for (int32 i = 0, n = Ring.Num(); i < n; ++i)
		{
			const FVector2D& A = Ring[i];
			const FVector2D& B = Ring[(i + 1) % n];
			Sum += A.X * B.Y - B.X * A.Y;
		}
		return Sum * 0.5;
	}

	FVector2D RingCentroid(const TArray<FVector2D>& Ring)
	{
		double Area = 0.0, Cx = 0.0, Cy = 0.0;
		for (int32 i = 0, n = Ring.Num(); i < n; ++i)
		{
			const FVector2D& A = Ring[i];
			const FVector2D& B = Ring[(i + 1) % n];
			const double Cross = A.X * B.Y - B.X * A.Y;
			Area += Cross;
			Cx += (A.X + B.X) * Cross;
			Cy += (A.Y + B.Y) * Cross;
		}
		if (FMath::Abs(Area) < 1e-9)
		{
			return Ring[0];
		}
		return FVector2D(Cx / (3.0 * Area), Cy / (3.0 * Area));
	}
}

bool FCampaignMapData::Load(FString& OutError)
{
	bLoaded = false;
	Land.Reset();
	Regions.Reset();
	Nations.Reset();
	Corps.Reset();
	Seas.Reset();
	RegionByName.Reset();

	const FString MapPath = ResolveDataFile(TEXT("campaign-map.json"), TEXT("src/generated"));
	const FString NationsPath = ResolveDataFile(TEXT("nations.json"), TEXT("data/campaign"));
	const FString SetupPath = ResolveDataFile(TEXT("setup-1939.json"), TEXT("data/campaign"));
	SourceDescription = FPaths::GetPath(MapPath);

	if (!LoadMap(MapPath, OutError) || !LoadNations(NationsPath, OutError) || !LoadSetup(SetupPath, OutError))
	{
		return false;
	}

	bLoaded = true;
	UE_LOG(LogTheater, Log, TEXT("Campaign data: %d regions, %d nations, %d corps, %d seas, %d landmasses (from %s)"),
		Regions.Num(), Nations.Num(), Corps.Num(), Seas.Num(), Land.Num(), *SourceDescription);
	return true;
}

bool FCampaignMapData::LoadMap(const FString& Path, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ReadJsonFile(Path, Root, OutError))
	{
		return false;
	}

	Width = Root->GetNumberField(TEXT("width"));
	Height = Root->GetNumberField(TEXT("height"));

	for (const TSharedPtr<FJsonValue>& PolyValue : Root->GetArrayField(TEXT("land")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Rings = PolyValue->AsArray();
		if (Rings.Num() == 0)
		{
			continue;
		}
		TArray<FVector2D> Ring;
		if (ReadRing(Rings[0]->AsArray(), Ring))
		{
			Land.Add(MoveTemp(Ring));
		}
	}

	for (const TSharedPtr<FJsonValue>& RegionValue : Root->GetArrayField(TEXT("regions")))
	{
		const TSharedPtr<FJsonObject> Obj = RegionValue->AsObject();
		FCampaignRegion Region;
		Region.Id = Obj->GetIntegerField(TEXT("id"));
		Region.Name = Obj->GetStringField(TEXT("name"));
		Region.Owner = Obj->GetStringField(TEXT("owner"));
		Region.Terrain = Obj->GetStringField(TEXT("terrain"));
		Region.VictoryPoints = Obj->GetIntegerField(TEXT("vp"));
		Region.Factories = Obj->GetIntegerField(TEXT("fac"));
		Region.Center = FVector2D(Obj->GetNumberField(TEXT("x")), Obj->GetNumberField(TEXT("y")));
		Region.Centroid = Region.Center;

		const TSharedPtr<FJsonObject>* Flags = nullptr;
		if (Obj->TryGetObjectField(TEXT("flags"), Flags))
		{
			Region.bCapital = (*Flags)->HasField(TEXT("cap"));
			Region.bPort = (*Flags)->HasField(TEXT("port"));
			Region.bOil = (*Flags)->HasField(TEXT("oil"));
			Region.bOre = (*Flags)->HasField(TEXT("ore"));
			Region.bDepot = (*Flags)->HasField(TEXT("depot"));
			Region.bColony = (*Flags)->HasField(TEXT("col"));
			const TArray<TSharedPtr<FJsonValue>>* Fort = nullptr;
			if ((*Flags)->TryGetArrayField(TEXT("fort"), Fort) && Fort->Num() > 0)
			{
				Region.FortLevel = FMath::RoundToInt32((*Fort)[0]->AsNumber());
			}
		}

		double BestArea = 0.0;
		for (const TSharedPtr<FJsonValue>& PolyValue : Obj->GetArrayField(TEXT("polys")))
		{
			const TArray<TSharedPtr<FJsonValue>>& Rings = PolyValue->AsArray();
			if (Rings.Num() == 0)
			{
				continue;
			}
			TArray<FVector2D> Ring;
			if (!ReadRing(Rings[0]->AsArray(), Ring))
			{
				continue;
			}
			const double Area = FMath::Abs(RingArea(Ring));
			Region.Area += Area;
			if (Area > BestArea)
			{
				BestArea = Area;
				Region.Centroid = RingCentroid(Ring);
			}
			for (const FVector2D& P : Ring)
			{
				Region.Bounds += P;
			}
			Region.Polygons.Add(MoveTemp(Ring));
		}

		for (const TSharedPtr<FJsonValue>& N : Obj->GetArrayField(TEXT("nb")))
		{
			Region.Neighbors.Add(FMath::RoundToInt32(N->AsNumber()));
		}

		RegionByName.Add(Region.Name, Regions.Num());
		Regions.Add(MoveTemp(Region));
	}

	const TSharedPtr<FJsonObject>* SeasObj = nullptr;
	if (Root->TryGetObjectField(TEXT("seas"), SeasObj))
	{
		for (const TSharedPtr<FJsonValue>& ZoneValue : (*SeasObj)->GetArrayField(TEXT("zones")))
		{
			const TSharedPtr<FJsonObject> Zone = ZoneValue->AsObject();
			FCampaignSeaZone Sea;
			Sea.Name = Zone->GetStringField(TEXT("name"));
			Sea.Center = FVector2D(Zone->GetNumberField(TEXT("x")), Zone->GetNumberField(TEXT("y")));
			Seas.Add(MoveTemp(Sea));
		}
	}

	if (Regions.Num() == 0 || Width <= 0.0 || Height <= 0.0)
	{
		OutError = FString::Printf(TEXT("%s has no regions"), *Path);
		return false;
	}
	return true;
}

bool FCampaignMapData::LoadNations(const FString& Path, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ReadJsonFile(Path, Root, OutError))
	{
		return false;
	}
	for (const auto& Pair : Root->Values)
	{
		const FString Key(Pair.Key.ToView());
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Key.StartsWith(TEXT("_")) || !Pair.Value.IsValid() || !Pair.Value->TryGetObject(Obj))
		{
			continue;
		}
		FCampaignNation Nation;
		Nation.Tag = Key;
		Nation.Name = (*Obj)->GetStringField(TEXT("name"));
		(*Obj)->TryGetStringField(TEXT("adj"), Nation.Adjective);
		(*Obj)->TryGetStringField(TEXT("align"), Nation.Alignment);
		FString Hex;
		if ((*Obj)->TryGetStringField(TEXT("color"), Hex))
		{
			Nation.Color = FColor::FromHex(Hex);
		}
		Nations.Add(Nation.Tag, MoveTemp(Nation));
	}
	if (Nations.Num() == 0)
	{
		OutError = FString::Printf(TEXT("%s has no nations"), *Path);
		return false;
	}
	return true;
}

bool FCampaignMapData::LoadSetup(const FString& Path, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ReadJsonFile(Path, Root, OutError))
	{
		return false;
	}
	// Each line is [nation, name, kind, divisions, region, options].
	for (const TSharedPtr<FJsonValue>& Line : Root->GetArrayField(TEXT("corps")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Fields = Line->AsArray();
		if (Fields.Num() < 5)
		{
			continue;
		}
		FCampaignCorps Unit;
		Unit.Nation = Fields[0]->AsString();
		Unit.Name = Fields[1]->AsString();
		Unit.Kind = Fields[2]->AsString();
		Unit.Divisions = FMath::RoundToInt32(Fields[3]->AsNumber());
		const int32* RegionId = RegionByName.Find(Fields[4]->AsString());
		if (!RegionId)
		{
			UE_LOG(LogTheater, Warning, TEXT("Corps %s is in an unknown region %s"), *Unit.Name, *Fields[4]->AsString());
			continue;
		}
		Unit.RegionId = *RegionId;
		const TSharedPtr<FJsonObject>* Options = nullptr;
		if (Fields.Num() > 5 && Fields[5]->TryGetObject(Options))
		{
			(*Options)->TryGetStringField(TEXT("gen"), Unit.General);
		}
		Corps.Add(MoveTemp(Unit));
	}
	return true;
}

bool FCampaignMapData::PointInRing(const TArray<FVector2D>& Ring, double X, double Y)
{
	bool bInside = false;
	for (int32 i = 0, j = Ring.Num() - 1; i < Ring.Num(); j = i++)
	{
		const FVector2D& A = Ring[i];
		const FVector2D& B = Ring[j];
		if ((A.Y > Y) != (B.Y > Y) && X < (B.X - A.X) * (Y - A.Y) / (B.Y - A.Y) + A.X)
		{
			bInside = !bInside;
		}
	}
	return bInside;
}

int32 FCampaignMapData::FindRegionAt(double X, double Y) const
{
	const FVector2D P(X, Y);
	for (const FCampaignRegion& Region : Regions)
	{
		if (!Region.Bounds.bIsValid || !Region.Bounds.IsInside(P))
		{
			continue;
		}
		for (const TArray<FVector2D>& Ring : Region.Polygons)
		{
			if (PointInRing(Ring, X, Y))
			{
				return Region.Id;
			}
		}
	}
	return INDEX_NONE;
}

FString FCampaignMapData::TerrainLabel(const FString& Terrain)
{
	if (Terrain == TEXT("plains")) return TEXT("Open country");
	if (Terrain == TEXT("hills")) return TEXT("Hills");
	if (Terrain == TEXT("forest")) return TEXT("Forest");
	if (Terrain == TEXT("mountain")) return TEXT("Mountains");
	if (Terrain == TEXT("urban")) return TEXT("City");
	if (Terrain == TEXT("desert")) return TEXT("Desert");
	if (Terrain == TEXT("marsh")) return TEXT("Marsh");
	return Terrain;
}

FString FCampaignMapData::CorpsKindLabel(const FString& Kind)
{
	if (Kind == TEXT("inf")) return TEXT("infantry");
	if (Kind == TEXT("mot")) return TEXT("motorized");
	if (Kind == TEXT("arm")) return TEXT("armoured");
	if (Kind == TEXT("cav")) return TEXT("cavalry");
	if (Kind == TEXT("mtn")) return TEXT("mountain");
	if (Kind == TEXT("fort")) return TEXT("fortress");
	return Kind;
}
