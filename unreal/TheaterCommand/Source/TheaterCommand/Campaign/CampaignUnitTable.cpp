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
		bool bPlainBattleship = false, bPlainCruiser = false;
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
		FString Source;
		if (Told->TryGetObjectField(TEXT("battleship"), Part) && (*Part)->TryGetStringField(TEXT("src"), Source))
		{
			Look.bPlainBattleship = Source == TEXT("generic");
		}
		if (Told->TryGetObjectField(TEXT("cruiser"), Part) && (*Part)->TryGetStringField(TEXT("src"), Source))
		{
			Look.bPlainCruiser = Source == TEXT("generic");
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

bool UnitTable::PlainBattleship(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	return Look && Look->bPlainBattleship;
}

bool UnitTable::PlainCruiser(const FString& Tag)
{
	const FLook* Look = LookOf(Tag);
	return Look && Look->bPlainCruiser;
}
