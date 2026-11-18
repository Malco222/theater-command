#include "Campaign/CampaignSounds.h"

#include "TheaterCommand.h"
#include "Components/AudioComponent.h"
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

namespace
{
	/**
	 * The real recordings (the design director's rulings 142 and 144, b478; Brian, 18 November 2026: "i approve them").
	 * The game's own names stand. Where the sound thread's battle pack has a recording for the same moment, the game
	 * plays that in place of its synthesised stand-in: under the same name where the pack has one, or under the
	 * pack's own name for that moment (its table "Which sound plays when"). A recording made to run round as a loop,
	 * or longer than the moment on the map, is played for the seconds given and then fades. The recordings are
	 * brought in by Tools/build_sounds.sh from Sounds/Real, to /Game/Campaign/Sounds/Real; a game without them
	 * plays the stand-ins as before. ("-TheaterOldSounds" plays the stand-ins.)
	 * Round five (Brian's stamps on the listening board, 18 November 2026, 14:37 to 14:49 UTC): the desk's sounds
	 * (click, stamp, tick, coins, pickup, bell), the newsreel's (projector, newsreel, radio), drums, victory and
	 * defeat have their recordings too, under the same names; and the train its own (train_loop, of the first
	 * approved batch). Nothing keeps a synthesised stand-in any more.
	 * CUT BY BRIAN at the same check, and silent until a new take passes his ear: the horses, the bugle and the
	 * ship's horn. They have no stand-in either: see Silent below. (The hammer was cut with them; at his third
	 * check, 15:51 UTC the same day, a steel hammer on an anvil passed, and the rifles became a line of real
	 * Lee-Enfields under the same name. The horses and the ship's horn were cut again.)
	 * The pack's cues for a submarine, a carrier, a landing craft and an aircraft (dive_alarm, surfacing,
	 * torpedo_launch, torpedo_run_loop, ship_sinking, depth_charge_roll, depth_charge_thrower, lift_motor,
	 * aircraft_break_up, bomb_release, parachute_open, ramp_winch, ramp_drop, craft_grounds) and its three beds for
	 * under a battle (wind_loop, woods_loop, rain_loop) are brought in with the rest and wait for the sound
	 * director's mix to say where they play.
	 */
	struct FRealSound
	{
		const TCHAR* Name; // the game's name for the moment
		const TCHAR* Recording; // the pack's file
		float Seconds; // how long it plays before it fades; 0: to its end
	};
	const FRealSound RealSounds[] = {
		{ TEXT("bomber_drone"), TEXT("bomber_drone"), 6.0f },
		{ TEXT("fighter_pass"), TEXT("fighter_pass"), 0.0f },
		{ TEXT("machine_gun"), TEXT("machine_gun"), 0.0f },
		{ TEXT("rifles"), TEXT("rifles"), 0.0f },
		{ TEXT("ship_guns"), TEXT("ship_guns"), 0.0f },
		{ TEXT("battle"), TEXT("battle_loop"), 5.0f }, // any unit in a fight
		{ TEXT("explosion"), TEXT("explosion_far_01"), 0.0f }, // a shell or a bomb lands, seen from far off
		{ TEXT("field_gun"), TEXT("field_gun_fire"), 0.0f },
		{ TEXT("halftrack"), TEXT("halftrack_drive_loop"), 3.6f },
		{ TEXT("marching"), TEXT("marching_loop"), 3.6f },
		{ TEXT("siren"), TEXT("air_raid_siren_short"), 0.0f }, // the map's warning: cut for it, twelve seconds
		{ TEXT("tank_engine"), TEXT("tank_loop"), 3.8f },
		{ TEXT("tank_gun"), TEXT("tank_gun_01"), 0.0f },
		{ TEXT("truck_engine"), TEXT("lorry_drive_loop"), 3.4f },
		// (round five: the desk, the newsreel and the stingers, each to its end)
		{ TEXT("click"), TEXT("click"), 0.0f },
		{ TEXT("stamp"), TEXT("stamp"), 0.0f },
		{ TEXT("tick"), TEXT("tick"), 0.0f },
		{ TEXT("coins"), TEXT("coins"), 0.0f },
		{ TEXT("pickup"), TEXT("pickup"), 0.0f },
		{ TEXT("hammer"), TEXT("hammer"), 0.0f }, // something is built or set to work (round six)
		{ TEXT("bell"), TEXT("bell"), 0.0f },
		{ TEXT("projector"), TEXT("projector"), 0.0f },
		{ TEXT("newsreel"), TEXT("newsreel"), 0.0f },
		{ TEXT("radio"), TEXT("radio"), 0.0f },
		{ TEXT("drums"), TEXT("drums"), 0.0f },
		{ TEXT("victory"), TEXT("victory"), 0.0f },
		{ TEXT("defeat"), TEXT("defeat"), 0.0f },
		{ TEXT("train"), TEXT("train_loop"), 3.6f }, // corps sent by rail
	};

	/** The moments Brian cut on 18 November 2026: they play nothing, neither a recording nor a stand-in, until a new take passes his ear. */
	const TCHAR* const Silent[] = { TEXT("horses"), TEXT("bugle"), TEXT("ship_horn") };

	/** The real recording for one of the game's names, and how long it plays: nothing where there is none, or it has not been brought in. */
	USoundBase* FindReal(const TCHAR* Name, float& OutSeconds)
	{
		static const bool bOld = FParse::Param(FCommandLine::Get(), TEXT("TheaterOldSounds"));
		static TMap<FName, TWeakObjectPtr<USoundBase>> Known;
		static TSet<FName> Missing;
		if (bOld)
		{
			return nullptr;
		}
		for (const FRealSound& Each : RealSounds)
		{
			if (FCString::Strcmp(Each.Name, Name) != 0)
			{
				continue;
			}
			OutSeconds = Each.Seconds;
			const FName Key(Each.Recording);
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
			USoundBase* Sound = LoadObject<USoundBase>(nullptr, *FString::Printf(TEXT("/Game/Campaign/Sounds/Real/%s.%s"), Each.Recording, Each.Recording), nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Sound)
			{
				Missing.Add(Key);
				UE_LOG(LogTheater, Display, TEXT("Sound: the recording %s has not been brought in (run Tools/build_sounds.sh): %s plays its stand-in"), Each.Recording, Name);
				return nullptr;
			}
			Known.Add(Key, Sound);
			return Sound;
		}
		return nullptr;
	}
}

void CampaignSounds::Play(const UObject* World, const TCHAR* Name, float Volume, float Pitch)
{
	if (!bEffectsOn || !World || !Name || !FApp::CanEverRenderAudio())
	{
		return;
	}
	for (const TCHAR* const Cut : Silent)
	{
		if (FCString::Strcmp(Cut, Name) == 0)
		{
			UE_LOG(LogTheater, Verbose, TEXT("Sound: %s is silent (cut by Brian; a new take is awaited)"), Name);
			return;
		}
	}
	// "-TheaterQuiet" plays everything too softly to hear: for trying the sound out without disturbing anyone.
	static const bool bQuiet = FParse::Param(FCommandLine::Get(), TEXT("TheaterQuiet"));
	float Seconds = 0.0f;
	if (USoundBase* Recording = FindReal(Name, Seconds))
	{
		// (The recordings are mastered far quieter than the stand-ins were made: measured, 19 to 27 dB under full scale
		// where a stand-in engine stood 6 under and a stand-in gun 14. Each is played two and a half times as loud as
		// its moment asked of its stand-in, and never over its master's own level, so that with the music at 0.15 the
		// guns and engines stand some 6 dB over it.)
		const float Loud = bQuiet ? Volume * 0.001f : FMath::Min(Volume * 2.5f, 1.0f);
		UE_LOG(LogTheater, Verbose, TEXT("Sound: %s plays its recording (%s), at %.2f%s"), Name, *Recording->GetName(), FMath::Min(Volume * 2.5f, 1.0f),
			Seconds > 0.0f ? *FString::Printf(TEXT(", for %.1f s"), Seconds) : TEXT(", to its end"));
		if (Seconds <= 0.0f)
		{
			UGameplayStatics::PlaySound2D(World, Recording, Loud, Pitch);
			return;
		}
		if (UAudioComponent* Player = UGameplayStatics::SpawnSound2D(World, Recording, Loud, Pitch, 0.0f, nullptr, false, true))
		{
			// (a loop, or a recording longer than its moment on the map: it plays for its seconds and fades over the last of them)
			UWorld* Where = World->GetWorld();
			if (Where)
			{
				TWeakObjectPtr<UAudioComponent> Weak(Player);
				FTimerHandle Handle;
				Where->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([Weak]()
				{
					if (Weak.IsValid())
					{
						Weak->FadeOut(0.7f, 0.0f);
					}
				}), FMath::Max(Seconds - 0.7f, 0.1f), false);
			}
		}
		return;
	}
	if (USoundBase* Sound = Find(Name))
	{
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
