#include "Campaign/CampaignMapData.h"

#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TheaterCommand.h"
#include "Campaign/TheaterSpeed.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
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

FString FCampaignMapData::ResolveDataFile(const FString& FileName, const FString& RepoRelativeDir)
{
	const FString Packaged = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Data/Campaign"), FileName);
	if (IFileManager::Get().FileExists(*Packaged))
	{
		return Packaged;
	}
	const FString Repo = FPaths::Combine(FPaths::ProjectDir(), TEXT("../.."), RepoRelativeDir, FileName);
	return FPaths::ConvertRelativePathToFull(Repo);
}

bool FCampaignMapData::Load(FString& OutError)
{
	bLoaded = false;
	Land.Reset();
	Regions.Reset();
	GridNX = GridNY = 0; // (the squares list provinces by their place in Regions: none until it is read again)
	GridStart.Reset();
	GridRegions.Reset();
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
	if (bWorld && Regions.Num() > FirstBeyond)
	{
		// (the countries that exist only on the wider chart: Canada, Mexico, Cuba)
		FString Ignored;
		LoadNations(ResolveDataFile(TEXT("world.json"), TEXT("data/campaign")), Ignored);
	}

	RuleRegionGrid();
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
	// The world the map lies in: itself, or (with -TheaterWorld) the frame of the whole-world map with Europe set in it.
	bWorld = FParse::Param(FCommandLine::Get(), TEXT("TheaterWorld"));
	WorldMinX = bWorld ? -500.0 : 0.0;
	WorldMinY = 0.0;
	WorldWidth = bWorld ? 9256.14 : Width;
	WorldHeight = bWorld ? 5096.0 : Height;
	ShiftX = bWorld ? 500.0 : 0.0;

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

	auto ReadRegions = [this](const TArray<TSharedPtr<FJsonValue>>& RegionValues)
	{
	for (const TSharedPtr<FJsonValue>& RegionValue : RegionValues)
	{
		const TSharedPtr<FJsonObject> Obj = RegionValue->AsObject();
		FCampaignRegion Region;
		Region.Id = Obj->GetIntegerField(TEXT("id"));
		Region.Name = Obj->GetStringField(TEXT("name"));
		Region.Owner = Obj->GetStringField(TEXT("owner"));
		Region.Home = Region.Owner;
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
	};
	ReadRegions(Root->GetArrayField(TEXT("regions")));

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

	Charted = FBox2D(FVector2D(0.0, 0.0), FVector2D(Width, Height));
	FirstBeyond = Regions.Num();
	LandHoles.Reset();
	Lakes.Reset();
	if (bWorld)
	{
		// (Rules version 74) The wider chart.
		// (Rules Version 85, 18 November 2026: the rules read the Americas with their 339 cities, 540 provinces in all,
		// from world-americas.json, a file of the same form. The game went on reading world-map.json and its 201, so an
		// army that marched into a city stood on ground the table did not have and was nowhere to be seen. The table
		// reads the file the rules read; the old one only where the new is not there.)
		FString WidePath = ResolveDataFile(TEXT("world-americas.json"), TEXT("src/generated"));
		if (!FPaths::FileExists(WidePath))
		{
			WidePath = ResolveDataFile(TEXT("world-map.json"), TEXT("src/generated"));
		}
		TSharedPtr<FJsonObject> Wide;
		FString WideError;
		if (ReadJsonFile(WidePath, Wide, WideError))
		{
			const TSharedPtr<FJsonObject>* Frame = nullptr;
			if (Wide->TryGetObjectField(TEXT("frame"), Frame))
			{
				Charted = FBox2D(FVector2D((*Frame)->GetNumberField(TEXT("x0")), (*Frame)->GetNumberField(TEXT("y0"))), FVector2D((*Frame)->GetNumberField(TEXT("x1")), (*Frame)->GetNumberField(TEXT("y1"))));
			}
			for (const TSharedPtr<FJsonValue>& PolyValue : Wide->GetArrayField(TEXT("land")))
			{
				// (the first ring is the coast; any after it are waters cut out of the land)
				const TArray<TSharedPtr<FJsonValue>>& Rings = PolyValue->AsArray();
				for (int32 K = 0; K < Rings.Num(); ++K)
				{
					TArray<FVector2D> Ring;
					if (ReadRing(Rings[K]->AsArray(), Ring))
					{
						(K == 0 ? Land : LandHoles).Add(MoveTemp(Ring));
					}
				}
			}
			ReadRegions(Wide->GetArrayField(TEXT("regions")));
			const TSharedPtr<FJsonObject>* WideSeas = nullptr;
			if (Wide->TryGetObjectField(TEXT("seas"), WideSeas))
			{
				// (Europe's seas keep their numbers, and the oceans follow)
				Seas.Reset();
				for (const TSharedPtr<FJsonValue>& ZoneValue : (*WideSeas)->GetArrayField(TEXT("zones")))
				{
					const TSharedPtr<FJsonObject> Zone = ZoneValue->AsObject();
					FCampaignSeaZone Sea;
					Sea.Name = Zone->GetStringField(TEXT("name"));
					Sea.Center = FVector2D(Zone->GetNumberField(TEXT("x")), Zone->GetNumberField(TEXT("y")));
					Seas.Add(MoveTemp(Sea));
				}
			}
			const TArray<TSharedPtr<FJsonValue>>* LakeValues = nullptr;
			if (Wide->TryGetArrayField(TEXT("lakes"), LakeValues))
			{
				for (const TSharedPtr<FJsonValue>& LakeValue : *LakeValues)
				{
					const TSharedPtr<FJsonObject> Obj = LakeValue->AsObject();
					FCampaignLake Lake;
					Lake.Name = Obj->GetStringField(TEXT("name"));
					Lake.Level = Obj->GetNumberField(TEXT("level"));
					Lake.Label = FVector2D(Obj->GetNumberField(TEXT("x")), Obj->GetNumberField(TEXT("y")));
					ReadRing(Obj->GetArrayField(TEXT("outline")), Lake.Outline);
					Lakes.Add(MoveTemp(Lake));
				}
			}
			RuledRivers.Reset();
			RuledBroad.Reset();
			int32 Streams = 0;
			{
				TSharedPtr<FJsonObject> Waters;
				FString WatersError;
				const TArray<TSharedPtr<FJsonValue>>* RiverValues = nullptr;
				// (and the rivers the rules cross: world-americas-rivers.json with the cities, 165 where there were 15. Each has
				// a class: a great or a middle river is water with banks; a small one is a stream, which the art director
				// has ruled is paint on the ground and no obstacle. Streams are left out here until that paint is built.)
				FString WatersPath = ResolveDataFile(TEXT("world-americas-rivers.json"), TEXT("src/generated"));
				if (!FPaths::FileExists(WatersPath))
				{
					WatersPath = ResolveDataFile(TEXT("world-rivers.json"), TEXT("src/generated"));
				}
				if (ReadJsonFile(WatersPath, Waters, WatersError) && Waters->TryGetArrayField(TEXT("rivers"), RiverValues))
				{
					for (const TSharedPtr<FJsonValue>& RiverValue : *RiverValues)
					{
						FString Class;
						if (RiverValue->AsObject()->TryGetStringField(TEXT("class"), Class) && Class == TEXT("small"))
						{
							++Streams;
							continue;
						}
						TArray<FVector2D> Course;
						if (ReadRing(RiverValue->AsObject()->GetArrayField(TEXT("pts")), Course) || Course.Num() >= 2)
						{
							RuledRivers.Add(MoveTemp(Course));
							RuledBroad.Add(RiverBroad(RiverValue->AsObject()->GetStringField(TEXT("name"))));
						}
					}
				}
			}
			UE_LOG(LogTheater, Display, TEXT("The wider chart: %d provinces beyond Europe (from %d), %d lands, %d waters cut out of them, %d lakes named, %d seas, %d rivers the rules cross (%d streams left out), from %s"),
				Regions.Num() - FirstBeyond, FirstBeyond, Land.Num(), LandHoles.Num(), Lakes.Num(), Seas.Num(), RuledRivers.Num(), Streams, *FPaths::GetCleanFilename(WidePath));
		}
		else
		{
			UE_LOG(LogTheater, Warning, TEXT("No wider chart at %s (%s): the world beyond Europe has no provinces."), *WidePath, *WideError);
		}
	}
	if (Regions.Num() == 0 || Width <= 0.0 || Height <= 0.0)
	{
		OutError = FString::Printf(TEXT("%s has no regions"), *Path);
		return false;
	}
	return true;
}

double FCampaignMapData::RiverBroad(const FString& Name)
{
	static const TSet<FString> Great = { TEXT("Rhine"), TEXT("Danube"), TEXT("Vistula"), TEXT("Dnieper"), TEXT("Volga"), TEXT("Don"), TEXT("Elbe"), TEXT("Oder"), TEXT("Loire"), TEXT("Seine"),
		TEXT("Rhone"), TEXT("Po"), TEXT("Nile"), TEXT("Dvina"), TEXT("Niemen"), TEXT("Bug"), TEXT("Dniester"),
		TEXT("Mississippi"), TEXT("Missouri"), TEXT("Ohio"), TEXT("St Lawrence"), TEXT("Columbia"),
		TEXT("Madeira"), TEXT("Paraguay"), TEXT("Uruguay"), TEXT("Magdalena") };
	// (The art director's round 31: the Amazon's trunk is about three times the common great river at Manaus, and
	// wider toward its mouth; the Madeira one and a half to two times. A river as broad as the rest read as a road.)
	// (her round 33: the trunk's mean is twice what it was)
	return Name == TEXT("Amazon") ? 4.4 : Name == TEXT("Madeira") ? 1.1 : Name == TEXT("Parana") || Name == TEXT("Orinoco") ? 0.85 : Great.Contains(Name) ? 0.625 : 0.425;
}

bool FCampaignMapData::IsCharted(double X, double Y) const
{
	if (Y < Charted.Min.Y || Y > Charted.Max.Y)
	{
		return false;
	}
	for (int32 Turn = -1; Turn <= 1; ++Turn)
	{
		const double Turned = X + Turn * WorldWidth;
		if (Turned >= Charted.Min.X && Turned <= Charted.Max.X)
		{
			return true;
		}
		if (!bWorld)
		{
			break;
		}
	}
	return false;
}

bool FCampaignMapData::LoadNations(const FString& Path, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	if (!ReadJsonFile(Path, Root, OutError))
	{
		return false;
	}
	// (the nations' own file has them at its top; the wider chart's file keeps its countries under "nations")
	const TSharedPtr<FJsonObject>* Inner = nullptr;
	const TSharedPtr<FJsonObject> From = Root->TryGetObjectField(TEXT("nations"), Inner) ? *Inner : Root;
	for (const auto& Pair : From->Values)
	{
		const FString Key(Pair.Key.ToView());
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Key.StartsWith(TEXT("_")) || !Pair.Value.IsValid() || !Pair.Value->TryGetObject(Obj) || Nations.Contains(Key))
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
	// The table's own paints. The colours in the data are chosen for a flat map on a screen; under the room's warm
	// lamp a grey reads as bare paper, so ground held by a grey country looked as if nobody held it. These are
	// the same countries' colours mixed stronger, so that ground always shows whose it is.
	static const TCHAR* TablePaints[][2] =
	{
		// (deepened by the art director, 2026-11-13, so that each holds up as a stain over real ground)
		{ TEXT("GER"), TEXT("545C68") }, { TEXT("POL"), TEXT("B85464") }, { TEXT("FRA"), TEXT("466CB0") }, { TEXT("SOV"), TEXT("9C3A2E") },
		{ TEXT("UK"), TEXT("B04A3E") }, { TEXT("ITA"), TEXT("548C5C") }, { TEXT("ROM"), TEXT("C4B048") }, { TEXT("BEL"), TEXT("CEA834") },
		{ TEXT("NED"), TEXT("D68230") }, { TEXT("HUN"), TEXT("B08C5C") }, { TEXT("SWE"), TEXT("CCB85C") },
		{ TEXT("DEN"), TEXT("B26084") }, { TEXT("YUG"), TEXT("607CA8") },
		// (The twelve small republics of Latin America, which the rules give one parchment between them: the art
		// director's picks of 15 November 2026, provisional until Brian approves them. Panama's plum also sets the
		// Canal Zone apart from the American olive.)
		{ TEXT("GUA"), TEXT("9383C4") }, { TEXT("ELS"), TEXT("C98494") }, { TEXT("HON"), TEXT("9DBD8C") }, { TEXT("NIC"), TEXT("BF7F73") },
		{ TEXT("COS"), TEXT("7FAE8C") }, { TEXT("PAN"), TEXT("A8607F") }, { TEXT("HAI"), TEXT("8FBA7A") }, { TEXT("DOM"), TEXT("B08FB5") },
		{ TEXT("ECU"), TEXT("CF8F66") }, { TEXT("BOL"), TEXT("6FA58C") }, { TEXT("PAR"), TEXT("C4838F") },
		// (The art director's round 33: Uruguay's lilac came out a grey on the ground. Deeper, and laid thicker, as
		// Argentina's was: its squares are to be seen at about (126, 108, 178). Provisional until Brian approves.)
		{ TEXT("URU"), TEXT("5D3CCB") },
		// (The art director's round 31: Venezuela's turquoise read as shallow sea from the top, 6.9 from the water
		// beside it. Emerald, provisional until Brian approves it.)
		{ TEXT("VEN"), TEXT("1F9D6B") },
		// (The same round: Argentina's sky blue, deeper, laid 1.35 times thicker and beside the pampas' gold. The
		// colour itself, sky blue for Argentina, is Brian's to approve.)
		{ TEXT("ARG"), TEXT("2A7CDC") },
	};
	for (const auto& Paint : TablePaints)
	{
		if (FCampaignNation* Nation = Nations.Find(Paint[0]))
		{
			Nation->Color = FColor::FromHex(Paint[1]);
		}
	}
	// ("-TheaterPaint=ARG:3F86D0,URU:8C7AC0" on the command line tries another table paint for a country, for a picture)
	{
		FString Tried;
		if (FParse::Value(FCommandLine::Get(), TEXT("TheaterPaint="), Tried, false))
		{
			TArray<FString> Each;
			Tried.ParseIntoArray(Each, TEXT(","));
			for (const FString& One : Each)
			{
				FString TriedTag, TriedPaint;
				if (One.Split(TEXT(":"), &TriedTag, &TriedPaint))
				{
					if (FCampaignNation* Nation = Nations.Find(TriedTag))
					{
						Nation->Color = FColor::FromHex(TriedPaint);
					}
				}
			}
		}
	}
	// No stain is pale or washed out, or its country reads as snow or as bare table (the art director's rule,
	// 2026-11-13): none brighter than about 0.55, none with less colour in it than about 0.3. Germany's field
	// grey is the one exception.
	for (TPair<FString, FCampaignNation>& Pair : Nations)
	{
		if (Pair.Key == TEXT("GER"))
		{
			continue;
		}
		FLinearColor Seen(Pair.Value.Color.R / 255.0f, Pair.Value.Color.G / 255.0f, Pair.Value.Color.B / 255.0f);
		FLinearColor Hsv = Seen.LinearRGBToHSV(); // (hue, saturation and value of the colour as it is seen)
		if (Hsv.G < 0.3f)
		{
			Hsv.G = 0.3f;
			Seen = Hsv.HSVToLinearRGB();
		}
		const float Bright = 0.30f * Seen.R + 0.59f * Seen.G + 0.11f * Seen.B;
		if (Bright > 0.55f)
		{
			Seen *= 0.55f / Bright;
		}
		Pair.Value.Color = FColor(static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Seen.R * 255.0f), 0, 255)), static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Seen.G * 255.0f), 0, 255)), static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Seen.B * 255.0f), 0, 255)));
	}
	// "-TheaterColour=GER:3D4A54,SVK:7C8794" tries other paints without a rebuild.
	FString Tried;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterColour="), Tried, false))
	{
		TArray<FString> Pairs;
		Tried.ParseIntoArray(Pairs, TEXT(","));
		for (const FString& Pair : Pairs)
		{
			FString Tag, Hex;
			if (Pair.Split(TEXT(":"), &Tag, &Hex))
			{
				if (FCampaignNation* Nation = Nations.Find(Tag))
				{
					Nation->Color = FColor::FromHex(Hex);
				}
			}
		}
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

void FCampaignMapData::RuleRegionGrid()
{
	GridNX = GridNY = 0;
	GridStart.Reset();
	GridRegions.Reset();
	FBox2D All(ForceInit);
	for (const FCampaignRegion& Region : Regions)
	{
		if (Region.Bounds.bIsValid)
		{
			All += Region.Bounds;
		}
	}
	if (!All.bIsValid)
	{
		return;
	}
	GridMin = All.Min;
	GridMax = All.Max;
	const FVector2D Size = All.GetSize();
	// (about 256 squares along the longer side: a province's bounds reach into a handful)
	GridCell = FMath::Max(FMath::Max(Size.X, Size.Y) / 256.0, 1.0e-3);
	GridNX = FMath::Clamp(FMath::FloorToInt32(Size.X / GridCell) + 1, 1, 4096);
	GridNY = FMath::Clamp(FMath::FloorToInt32(Size.Y / GridCell) + 1, 1, 4096);
	auto Square = [this](double V, double Min, int32 N)
	{
		return FMath::Clamp(FMath::FloorToInt32((V - Min) / GridCell), 0, N - 1);
	};
	TArray<int32> Count;
	Count.Init(0, GridNX * GridNY);
	for (int32 Pass = 0; Pass < 2; ++Pass)
	{
		if (Pass == 1)
		{
			// (counted first, then laid out one list after another)
			GridStart.SetNumUninitialized(GridNX * GridNY + 1);
			GridStart[0] = 0;
			for (int32 Index = 0; Index < Count.Num(); ++Index)
			{
				GridStart[Index + 1] = GridStart[Index] + Count[Index];
				Count[Index] = 0;
			}
			GridRegions.SetNumUninitialized(GridStart.Last());
		}
		for (int32 Index = 0; Index < Regions.Num(); ++Index)
		{
			const FBox2D& Bounds = Regions[Index].Bounds;
			if (!Bounds.bIsValid)
			{
				continue;
			}
			const int32 I0 = Square(Bounds.Min.X, GridMin.X, GridNX), I1 = Square(Bounds.Max.X, GridMin.X, GridNX);
			const int32 J0 = Square(Bounds.Min.Y, GridMin.Y, GridNY), J1 = Square(Bounds.Max.Y, GridMin.Y, GridNY);
			for (int32 J = J0; J <= J1; ++J)
			{
				for (int32 I = I0; I <= I1; ++I)
				{
					const int32 Cell = J * GridNX + I;
					if (Pass == 1)
					{
						GridRegions[GridStart[Cell] + Count[Cell]] = Index; // (in the provinces' own order, as the whole list is read)
					}
					++Count[Cell];
				}
			}
		}
	}
}

int32 FCampaignMapData::FindRegionAt(double InX, double Y) const
{
	// (Speed, 18 November 2026: tc.Speed.RegionGrid. Every province of the world was looked at for each place asked
	// about; the rivers alone ask it for each point of their course whenever the haze moves. Now only the provinces
	// whose bounds reach into the place's square are, in the same order, so the same province is found: one whose
	// bounds hold the place reaches into its square.)
	const bool bGrid = GridNX > 0 && GridStart.Num() == GridNX * GridNY + 1 && TheaterSpeed::RegionGrid();
	// (on the whole-world map a place may be counted a turn of the world east or west of where its province is kept)
	for (int32 Turn = 0; Turn <= (bWorld ? 2 : 0); ++Turn)
	{
		const double X = InX + (Turn == 1 ? -WorldWidth : Turn == 2 ? WorldWidth : 0.0);
		const FVector2D P(X, Y);
		if (bGrid)
		{
			if (X < GridMin.X || X > GridMax.X || Y < GridMin.Y || Y > GridMax.Y)
			{
				continue; // (outside every province's bounds)
			}
			const int32 I = FMath::Clamp(FMath::FloorToInt32((X - GridMin.X) / GridCell), 0, GridNX - 1);
			const int32 J = FMath::Clamp(FMath::FloorToInt32((Y - GridMin.Y) / GridCell), 0, GridNY - 1);
			const int32 Cell = J * GridNX + I;
			for (int32 At = GridStart[Cell]; At < GridStart[Cell + 1]; ++At)
			{
				const FCampaignRegion& Region = Regions[GridRegions[At]];
				if (!Region.Bounds.IsInside(P))
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
			continue;
		}
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
	if (Kind == TEXT("art")) return TEXT("artillery");
	if (Kind == TEXT("cav")) return TEXT("cavalry");
	if (Kind == TEXT("mtn")) return TEXT("mountain");
	if (Kind == TEXT("fort")) return TEXT("fortress");
	return Kind;
}
