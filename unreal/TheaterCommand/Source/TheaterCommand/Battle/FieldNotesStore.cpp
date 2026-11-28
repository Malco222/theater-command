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
		bool bWordsRead = false, bWordsOk = false, bBookRead = false, bWhispersRead = false, bWhispersOk = false;
		FieldNotes::Words TheWords;
		StaffWhisper::Words TheWhispers;
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

	void SetOn(bool bOn)
	{
		if (!FeatureOn() || !Words() || (Book().bOn == bOn && !(bOn && Book().bMapOffOwed) && !(!bOn && Book().bMapOnOwed)))
		{
			return;
		}
		Book().bOn = bOn;
		Book().bMapOffOwed = Book().bMapOffOwed && !bOn;
		Book().bMapOnOwed = Book().bMapOnOwed && bOn;
		Save();
	}

	void OffInBattle()
	{
		if (!FeatureOn() || !Words())
		{
			return;
		}
		Book().bMapOffOwed = true;
		Book().bMapOnOwed = false;
		Save();
	}

	bool PracticeOn()
	{
		return FeatureOn() && FParse::Param(FCommandLine::Get(), TEXT("TheaterPracticeNotes"));
	}

	void StartOver()
	{
		if (!PracticeOn() || !Words())
		{
			return;
		}
		Book().StartOver();
		Save();
		UE_LOG(LogTheater, Display, TEXT("Field notes: PRACTISE WITH STAFF NOTES: from the first note again, the campaign map's owed on"));
	}

	bool TakeOnAsked()
	{
		if (!FeatureOn() || !Words() || !Book().bMapOnOwed)
		{
			return false;
		}
		Book().bMapOnOwed = false;
		Save();
		return true;
	}

	bool TakeOffAsked()
	{
		if (!FeatureOn() || !Words() || !Book().bMapOffOwed)
		{
			return false;
		}
		Book().bMapOffOwed = false;
		Save();
		return true;
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

	bool WhisperOn()
	{
		return FParse::Param(FCommandLine::Get(), TEXT("TheaterStaffWhisper"));
	}

	const StaffWhisper::Words* Whispers()
	{
		if (!bWhispersRead)
		{
			bWhispersRead = true;
			FString Text;
			std::string Why = "the file could not be opened";
			const FString Path = FPaths::ProjectContentDir() / TEXT("Data/Battle/staff-whispers.json");
			bWhispersOk = FFileHelper::LoadFileToString(Text, *Path) && StaffWhisper::ReadWords(Narrow(Text), TheWhispers, Why);
			if (!bWhispersOk && WhisperOn())
			{
				UE_LOG(LogTheater, Warning, TEXT("Staff whisper: Content/Data/Battle/staff-whispers.json not read (%s): no whispers"), UTF8_TO_TCHAR(Why.c_str()));
			}
		}
		return bWhispersOk ? &TheWhispers : nullptr;
	}

	bool WhispersShown()
	{
		return WhisperOn() && Whispers() != nullptr && Book().bOn;
	}
}
