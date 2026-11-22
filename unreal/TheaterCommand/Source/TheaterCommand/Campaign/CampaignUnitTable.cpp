#include "Campaign/CampaignUnitTable.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TheaterCommand.h"

namespace
{
	struct FLook
	{
		FString Head, Cloth, Paint, Paint2, Hull;
		bool bWinterWash = false;
		TArray<FString> Ring;
		TArray<UnitTable::FInsigniaLayer> Insignia;
		TArray<UnitTable::FInsigniaLayer> InsigniaClose; // (the layers of a country whose marking is "as built": for close views)
		// (its dated marks: the month each stands from, counted in months from the year 0, and its layers)
		TArray<int32> DatedFrom;
		TArray<TArray<UnitTable::FInsigniaLayer>> Dated;
		// (the rudder's stripes: its own, and those of each dated mark; "v" stands them side by side)
		struct FFin
		{
			bool bUpright = true;
			TArray<FString> Bands;
		};
		FFin Fin;
		TArray<FFin> DatedFin;
		// Whose layout its battleship and its cruiser take: "*" its own, a tag another's by the record, empty the plain one.
		FString BattleshipOf, CruiserOf;
	};

	struct FTable
	{
		TMap<FString, FLook> Countries;
		FLook Default;
		bool bLoaded = false;
	};

	/** A mark's layers as the table gives them, bottom first. A layer that is none of the four shapes is left out. */
	TArray<UnitTable::FInsigniaLayer> ReadLayers(const TArray<TSharedPtr<FJsonValue>>& List)
	{
		TArray<UnitTable::FInsigniaLayer> Out;
		for (const TSharedPtr<FJsonValue>& Value : List)
		{
			const TSharedPtr<FJsonObject>* Entry = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Numbers = nullptr;
			FString Hex;
			if (!Value.IsValid() || !Value->TryGetObject(Entry) || !(*Entry)->TryGetStringField(TEXT("c"), Hex) || Hex.Len() != 6)
			{
				continue;
			}
			auto Number = [&Numbers](int32 Index) { return (*Numbers)[Index].IsValid() ? (*Numbers)[Index]->AsNumber() : 0.0; };
			UnitTable::FInsigniaLayer One;
			One.Colour = FLinearColor(FColor::FromHex(Hex));
			if ((*Entry)->TryGetArrayField(TEXT("ring"), Numbers) && Numbers->Num() >= 2)
			{
				One.Shape = UnitTable::FInsigniaLayer::EShape::Ring;
				One.Inner = Number(0);
				One.Outer = Number(1);
			}
			else if ((*Entry)->TryGetArrayField(TEXT("tri"), Numbers) && Numbers->Num() >= 2)
			{
				One.Shape = UnitTable::FInsigniaLayer::EShape::Triangle;
				One.Outer = Number(0);
				One.Turn = Number(1);
			}
			else if ((*Entry)->TryGetArrayField(TEXT("star"), Numbers) && Numbers->Num() >= 3)
			{
				One.Shape = UnitTable::FInsigniaLayer::EShape::Star;
				One.Outer = Number(0);
				One.Inner = Number(1);
				One.Points = FMath::Clamp(static_cast<int32>(Number(2)), 3, 24);
			}
			else if ((*Entry)->TryGetArrayField(TEXT("rect"), Numbers) && Numbers->Num() >= 4)
			{
				One.Shape = UnitTable::FInsigniaLayer::EShape::Bar;
				One.Across0 = Number(0);
				One.Across1 = Number(1);
				One.Up0 = Number(2);
				One.Up1 = Number(3);
			}
			else
			{
				continue;
			}
			Out.Add(One);
		}
		return Out;
	}

	/** A rudder's stripes as the table gives them: {"dir": "v" or "h", "bands": [colours]}. */
	FLook::FFin ReadFin(const TSharedPtr<FJsonObject>& Told)
	{
		FLook::FFin Fin;
		const TSharedPtr<FJsonObject>* Part = nullptr;
		if (Told->TryGetObjectField(TEXT("fin"), Part))
		{
			FString Way;
			(*Part)->TryGetStringField(TEXT("dir"), Way);
			Fin.bUpright = Way != TEXT("h");
			(*Part)->TryGetStringArrayField(TEXT("bands"), Fin.Bands);
		}
		return Fin;
	}

	FLook Read(const TSharedPtr<FJsonObject>& Told)
	{
		FLook Look;
		Look.Fin = ReadFin(Told);
		const TSharedPtr<FJsonObject>* Part = nullptr;
		if (Told->TryGetObjectField(TEXT("soldier"), Part))
		{
			(*Part)->TryGetStringField(TEXT("model"), Look.Head);
		}
		Told->TryGetStringField(TEXT("cloth"), Look.Cloth);
		Told->TryGetStringField(TEXT("paint"), Look.Paint);
		Told->TryGetStringField(TEXT("hull"), Look.Hull);
		// (a marking that is a list is a ring of the flag's colours; one that is words is drawn already, or is the default's note)
		Told->TryGetStringArrayField(TEXT("marking"), Look.Ring);
		Told->TryGetStringField(TEXT("paint2"), Look.Paint2);
		Told->TryGetBoolField(TEXT("winter_wash"), Look.bWinterWash);
		// (The art director's table of 20:59Z on 18 November 2026: a country's own mark in layers, bottom first. A layer
		// is {"ring": [inner, outer]}, {"tri": [radius, degrees]}, {"star": [radius, inner share, points]} or
		// {"rect": [across from, across to, up from, up to]}, with "c" its colour. A layer that is none of these is left out.)
		// (A country whose marking is "as built" keeps on the map the mark the game draws by hand; its layers, where it
		// has them, are for the battle: Sweden's three crowns, which at the map's size would be specks.)
		const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
		FString MarkingWords;
		const bool bAsBuilt = Told->TryGetStringField(TEXT("marking"), MarkingWords) && MarkingWords == TEXT("as built");
		if (Told->TryGetArrayField(TEXT("insignia"), Layers))
		{
			(bAsBuilt ? Look.InsigniaClose : Look.Insignia) = ReadLayers(*Layers);
		}
		// (Her "marking_months": {"from": "1942-06", "insignia": [...]}, each standing from its month until the next.
		// These are plain discs and bars and stand on the map too, whatever the country's first mark is.)
		const TArray<TSharedPtr<FJsonValue>>* Months = nullptr;
		if (Told->TryGetArrayField(TEXT("marking_months"), Months))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Months)
			{
				const TSharedPtr<FJsonObject>* Entry = nullptr;
				FString From, YearText, MonthText;
				if (Value.IsValid() && Value->TryGetObject(Entry) && (*Entry)->TryGetStringField(TEXT("from"), From) && From.Split(TEXT("-"), &YearText, &MonthText)
					&& (*Entry)->TryGetArrayField(TEXT("insignia"), Layers))
				{
					TArray<UnitTable::FInsigniaLayer> Mark = ReadLayers(*Layers);
					if (Mark.Num() > 0)
					{
						Look.DatedFrom.Add(FCString::Atoi(*YearText) * 12 + FMath::Clamp(FCString::Atoi(*MonthText), 1, 12) - 1);
						Look.Dated.Add(MoveTemp(Mark));
						Look.DatedFin.Add(ReadFin(*Entry));
					}
				}
			}
		}
		// (The art director's table of 18 November 2026, after Brian's rule for small nations: a ship's "src" is "own",
		// "shared" (another country's layout by the record, named in "of"), "generic", or "captured". A captured
		// battleship is one a small nation holds by a great power's plans; it is to be that power's model in the
		// small nation's own grey. The rules do not yet say whose plans a country holds, so until they do such a
		// ship is drawn as the plain one, which is no navy's own.)
		auto LayoutOf = [](const TSharedPtr<FJsonObject>& Ship)
		{
			FString Source, Of;
			Ship->TryGetStringField(TEXT("src"), Source);
			if (Source == TEXT("own"))
			{
				return FString(TEXT("*"));
			}
			if (Source == TEXT("shared") && Ship->TryGetStringField(TEXT("of"), Of))
			{
				return Of;
			}
			return FString();
		};
		if (Told->TryGetObjectField(TEXT("battleship"), Part))
		{
			Look.BattleshipOf = LayoutOf(*Part);
		}
		if (Told->TryGetObjectField(TEXT("cruiser"), Part))
		{
			Look.CruiserOf = LayoutOf(*Part);
		}
		return Look;
	}

	const FTable& Table()
	{
		static FTable Kept;
		static bool bTried = false;
		if (!bTried)
		{
			bTried = true;
			FString Text;
			TSharedPtr<FJsonObject> File;
			const FString Path = FPaths::ProjectContentDir() / TEXT("Data") / TEXT("Campaign") / TEXT("unit-table.json");
			if (FFileHelper::LoadFileToString(Text, *Path) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), File) && File.IsValid())
			{
				const TSharedPtr<FJsonObject>* Part = nullptr;
				if (File->TryGetObjectField(TEXT("_default"), Part))
				{
					Kept.Default = Read(*Part);
				}
				if (File->TryGetObjectField(TEXT("countries"), Part))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Each : (*Part)->Values)
					{
						const TSharedPtr<FJsonObject>* Country = nullptr;
						if (Each.Value.IsValid() && Each.Value->TryGetObject(Country))
						{
							Kept.Countries.Add(Each.Key, Read(*Country));
						}
					}
				}
				Kept.bLoaded = Kept.Countries.Num() > 0;
			}
			int32 Marked = 0, TwoCoats = 0, Dated = 0;
		for (const TPair<FString, FLook>& Each : Kept.Countries)
		{
			Marked += Each.Value.Insignia.Num() > 0 ? 1 : 0;
			TwoCoats += Each.Value.Paint2.Len() == 6 ? 1 : 0;
			Dated += Each.Value.Dated.Num() > 0 ? 1 : 0;
		}
		UE_LOG(LogTheater, Display, TEXT("Unit table: %d countries read%s; %d with a mark of their own from the table, %d with a second camouflage colour, %d with marks that change by date"), Kept.Countries.Num(),
			Kept.bLoaded ? TEXT("") : TEXT(" (the game draws as it did before)"), Marked, TwoCoats, Dated);
		}
		return Kept;
	}

	/** The country's line of the table, or the default's; null where there is no table. */
	const FLook* LookOf(const FString& Tag)
	{
		const FTable& Whole = Table();
		if (!Whole.bLoaded)
		{
			return nullptr;
		}
		// (the game has known Japan and China by two tags each)
		const FLook* Found = Whole.Countries.Find(Tag == TEXT("JPN") ? FString(TEXT("JAP")) : Tag == TEXT("CHI") ? FString(TEXT("CHN")) : Tag);
		return Found ? Found : &Whole.Default;
	}

	bool Colour(const FString& Code, FLinearColor& Out)
	{
		if (Code.Len() != 6)
		{
			return false;
		}
		Out = FLinearColor(FColor::FromHex(Code));
		return true;
	}
}

bool UnitTable::IsLoaded()
{
	return Table().bLoaded;
}

FString UnitTable::Head(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	return Look ? Look->Head : FString();
}

bool UnitTable::Cloth(const FString& Tag, FLinearColor& Out)
{
	const FLook* Look = LookOf(Tag);
	return Look && Colour(Look->Cloth, Out);
}

bool UnitTable::Paint(const FString& Tag, FLinearColor& Out)
{
	const FLook* Look = LookOf(Tag);
	return Look && Colour(Look->Paint, Out);
}

bool UnitTable::Hull(const FString& Tag, FLinearColor& Out)
{
	const FLook* Look = LookOf(Tag);
	return Look && Colour(Look->Hull, Out);
}

namespace
{
	// (the campaign's month, in months from the year 0: September 1939 until the map says otherwise)
	int32 MonthNow = 1939 * 12 + 8;
	bool bCloseMarksNow = false;
}

void UnitTable::SetTurn(int32 Turn)
{
	MonthNow = 1939 * 12 + 7 + Turn; // (turn 16 is December 1940, as the map's season has it)
}

int32 UnitTable::Month()
{
	return MonthNow;
}

void UnitTable::SetMonth(int32 Year, int32 MonthOfYear)
{
	// ("-TheaterSeason=48" holds the table at one month of the war, here as on the map: turn 48 is August 1943)
	int32 Held = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("TheaterSeason="), Held))
	{
		SetTurn(Held);
		return;
	}
	MonthNow = Year * 12 + FMath::Clamp(MonthOfYear, 1, 12) - 1;
}

int32 UnitTable::MarkEra(const FString& Tag)
{
	int32 Era = 0;
	if (const FLook* Look = LookOf(Tag))
	{
		for (int32 K = 0; K < Look->DatedFrom.Num(); ++K)
		{
			if (Look->DatedFrom[K] <= MonthNow)
			{
				Era = K + 1;
			}
		}
	}
	return Era;
}

bool UnitTable::Fin(const FString& Tag, bool& bOutUpright, TArray<FLinearColor>& OutBands)
{
	OutBands.Reset();
	const FLook* Look = LookOf(Tag);
	if (!Look)
	{
		return false;
	}
	const int32 Era = MarkEra(Tag);
	const FLook::FFin& Fin = Era > 0 && Look->DatedFin.IsValidIndex(Era - 1) && Look->DatedFin[Era - 1].Bands.Num() > 0 ? Look->DatedFin[Era - 1] : Look->Fin;
	for (const FString& Code : Fin.Bands)
	{
		FLinearColor One;
		if (Colour(Code, One))
		{
			OutBands.Add(One);
		}
	}
	bOutUpright = Fin.bUpright;
	return OutBands.Num() > 0;
}

TArray<UnitTable::FInsigniaLayer> UnitTable::Insignia(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	if (!Look)
	{
		return TArray<FInsigniaLayer>();
	}
	const int32 Era = MarkEra(Tag);
	return Era > 0 ? Look->Dated[Era - 1] : Look->Insignia.Num() > 0 || !bCloseMarksNow ? Look->Insignia : Look->InsigniaClose;
}

void UnitTable::SetCloseMarks(bool bOn)
{
	bCloseMarksNow = bOn;
}

bool UnitTable::CloseMarks()
{
	return bCloseMarksNow;
}

bool UnitTable::Paint2(const FString& Tag, FLinearColor& Out)
{
	const FLook* Look = LookOf(Tag);
	return Look && Colour(Look->Paint2, Out);
}

namespace
{
	bool bWashNow = false;
}

bool UnitTable::WinterWash(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	return Look && Look->bWinterWash;
}

void UnitTable::SetWash(bool bOn)
{
	bWashNow = bOn;
}

bool UnitTable::Wash()
{
	return bWashNow;
}

TArray<FLinearColor> UnitTable::Ring(const FString& Tag)
{
	TArray<FLinearColor> Out;
	if (const FLook* Look = LookOf(Tag))
	{
		for (const FString& Code : Look->Ring)
		{
			FLinearColor One;
			if (Colour(Code, One))
			{
				Out.Add(One);
			}
		}
	}
	return Out;
}

FString UnitTable::BattleshipOf(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	return !Look ? Tag : Look->BattleshipOf == TEXT("*") ? Tag : Look->BattleshipOf;
}

FString UnitTable::CruiserOf(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	return !Look ? Tag : Look->CruiserOf == TEXT("*") ? Tag : Look->CruiserOf;
}
