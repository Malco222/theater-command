#include "Campaign/CampaignUnitTable.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TheaterCommand.h"

namespace
{
	struct FLook
	{
		FString Head, Cloth, Paint, Hull;
		TArray<FString> Ring;
		// Whose layout its battleship and its cruiser take: "*" its own, a tag another's by the record, empty the plain one.
		FString BattleshipOf, CruiserOf;
	};

	struct FTable
	{
		TMap<FString, FLook> Countries;
		FLook Default;
		bool bLoaded = false;
	};

	FLook Read(const TSharedPtr<FJsonObject>& Told)
	{
		FLook Look;
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
			UE_LOG(LogTheater, Display, TEXT("Unit table: %d countries read%s"), Kept.Countries.Num(), Kept.bLoaded ? TEXT("") : TEXT(" (the game draws as it did before)"));
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
