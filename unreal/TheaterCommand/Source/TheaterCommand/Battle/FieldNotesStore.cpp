#include "Battle/FieldNotesStore.h"

#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TheaterCommand.h"

#include <string>

namespace FieldNotesStore
{
	namespace
	{
		bool bWordsRead = false, bWordsOk = false, bBookRead = false;
		FieldNotes::Words TheWords;
		FieldNotes::Book TheBook;
		/** The attack card's tip: shown on a card yet, and the last frame it was asked for (a card opened again leaves a
		 * gap of frames: the card it was shown on has closed). */
		bool bTipShown = false;
		uint64 TipFrame = 0;

		std::string Narrow(const FString& Text)
		{
			const FTCHARToUTF8 Utf8(*Text);
			return std::string(Utf8.Get(), static_cast<size_t>(Utf8.Length()));
		}

		bool InRun()
		{
			static const TCHAR* const Flags[] = { TEXT("TheaterShots="), TEXT("TheaterSteps="), TEXT("TheaterLandBattleShots="), TEXT("TheaterLandScript=") };
			FString Value;
			for (const TCHAR* Flag : Flags)
			{
				if (FParse::Value(FCommandLine::Get(), Flag, Value, false))
				{
					return true;
				}
			}
			return false;
		}

		FString BookFile()
		{
			return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Campaign"), InRun() ? TEXT("field-notes-runs.json") : TEXT("field-notes.json"));
		}
	}

	bool FeatureOn()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("TheaterFieldNotes"));
	}

	const FieldNotes::Words* Words()
	{
		if (!bWordsRead)
		{
			bWordsRead = true;
			FString Text;
			std::string Why = "the file could not be opened";
			const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Battle/field-notes.json");
			bWordsOk = FFileHelper::LoadFileToString(Text, *Path) && FieldNotes::ReadWords(Narrow(Text), TheWords, Why);
			if (!bWordsOk && FeatureOn())
			{
				UE_LOG(LogTheater, Warning, TEXT("Field notes: Content/Data/Battle/field-notes.json not read (%s): no field notes"), UTF8_TO_TCHAR(Why.c_str()));
			}
		}
		return bWordsOk ? &TheWords : nullptr;
	}

	FieldNotes::Book& Book()
	{
		if (!bBookRead)
		{
			bBookRead = true;
			FString Text;
			if (InRun() && !FParse::Param(FCommandLine::Get(), TEXT("TheaterKeepFieldNotes")))
			{
				Text = FParse::Param(FCommandLine::Get(), TEXT("TheaterFieldNotesRun")) ? TEXT("{}") : TEXT("{\"on\":false}");
			}
			else if (!FFileHelper::LoadFileToString(Text, *BookFile()))
			{
				Text = TEXT("{}");
			}
			TheBook = FieldNotes::ReadBook(Narrow(Text));
		}
		return TheBook;
	}

	bool Shown()
	{
		return FeatureOn() && Words() != nullptr && Book().bOn;
	}

	void Save()
	{
		const std::string Text = FieldNotes::WriteBook(Book());
		const FString Path = BookFile();
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		if (!FFileHelper::SaveStringToFile(FString(UTF8_TO_TCHAR(Text.c_str())), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogTheater, Warning, TEXT("Field notes: %s could not be written"), *Path);
		}
	}

	void ArmAgain()
	{
		if (!FeatureOn() || !Words())
		{
			return;
		}
		Book().ArmAgain(*Words());
		Save();
	}

	FString FightTip()
	{
		if (!Shown() || Words()->FightTip.empty() || Book().HasSeen("fight"))
		{
			return FString();
		}
		if (bTipShown && GFrameCounter > TipFrame + 2)
		{
			// (shown on one card, the first that offered FIGHT IT YOURSELF, for as long as it stayed open: the next
			// card finds it seen)
			Book().MarkSeen("fight");
			Save();
			return FString();
		}
		bTipShown = true;
		TipFrame = GFrameCounter;
		return FString(UTF8_TO_TCHAR(Words()->FightTip.c_str()));
	}

	void FightTipSeen()
	{
		if (FeatureOn() && !Book().HasSeen("fight"))
		{
			Book().MarkSeen("fight");
			Save();
		}
	}
}
