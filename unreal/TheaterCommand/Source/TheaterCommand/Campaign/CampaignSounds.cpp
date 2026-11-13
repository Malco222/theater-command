#include "Campaign/CampaignSounds.h"

#include "TheaterCommand.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

namespace
{
	bool bEffectsOn = true;

	USoundBase* Find(const TCHAR* Name)
	{
		static TMap<FName, TWeakObjectPtr<USoundBase>> Known;
		static TSet<FName> Missing;
		const FName Key(Name);
		if (const TWeakObjectPtr<USoundBase>* Found = Known.Find(Key))
		{
			if (Found->IsValid())
			{
				return Found->Get();
			}
		}
		if (Missing.Contains(Key))
		{
			return nullptr;
		}
		USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/%s.%s"), Name, Name), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sound)
		{
			Missing.Add(Key);
			UE_LOG(LogTheater, Display, TEXT("Sound: %s has not been made (run Tools/build_sounds.sh)"), Name);
			return nullptr;
		}
		Known.Add(Key, Sound);
		return Sound;
	}
}

void CampaignSounds::Play(const UObject* World, const TCHAR* Name, float Volume, float Pitch)
{
	if (!bEffectsOn || !World || !Name || !FApp::CanEverRenderAudio())
	{
		return;
	}
	if (USoundBase* Sound = Find(Name))
	{
		// "-TheaterQuiet" plays everything too softly to hear: for trying the sound out without disturbing anyone.
		static const bool bQuiet = FParse::Param(FCommandLine::Get(), TEXT("TheaterQuiet"));
		UGameplayStatics::PlaySound2D(World, Sound, bQuiet ? Volume * 0.001f : Volume, Pitch);
	}
}

void CampaignSounds::PlayLater(const UObject* World, const TCHAR* Name, float Seconds, float Volume)
{
	UWorld* Where = World ? World->GetWorld() : nullptr;
	if (!Where)
	{
		return;
	}
	TWeakObjectPtr<UWorld> Weak(Where);
	const FString Which(Name);
	FTimerHandle Handle;
	Where->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak, Which, Volume]()
	{
		if (Weak.IsValid())
		{
			CampaignSounds::Play(Weak.Get(), *Which, Volume);
		}
	}), FMath::Max(Seconds, 0.01f), false);
}

bool CampaignSounds::IsOn()
{
	return bEffectsOn;
}

void CampaignSounds::SetOn(bool bOn)
{
	bEffectsOn = bOn;
}

bool CampaignSounds::Toggle()
{
	bEffectsOn = !bEffectsOn;
	return bEffectsOn;
}

const TCHAR* CampaignSounds::OfArmy(const TArray<FString>& Kinds, const FString& Nation)
{
	if (Kinds.Contains(TEXT("arm")))
	{
		return TEXT("tank_engine");
	}
	if (Kinds.Contains(TEXT("mot")))
	{
		return Nation == TEXT("GER") || Nation == TEXT("USA") ? TEXT("halftrack") : TEXT("truck_engine");
	}
	if (Kinds.Contains(TEXT("cav")))
	{
		return TEXT("horses");
	}
	return TEXT("marching");
}

const TCHAR* CampaignSounds::OfOrder(const FString& Key)
{
	if (Key.StartsWith(TEXT("corps:arm"))) return TEXT("tank_engine");
	if (Key.StartsWith(TEXT("corps:mot"))) return TEXT("truck_engine");
	if (Key.StartsWith(TEXT("corps:cav"))) return TEXT("horses");
	if (Key.StartsWith(TEXT("corps:art"))) return TEXT("field_gun");
	if (Key.StartsWith(TEXT("corps:"))) return TEXT("marching");
	if (Key.StartsWith(TEXT("air:bmr"))) return TEXT("bomber_drone");
	if (Key.StartsWith(TEXT("air:"))) return TEXT("fighter_pass");
	if (Key.StartsWith(TEXT("ship:"))) return TEXT("ship_horn");
	return TEXT("hammer");
}
