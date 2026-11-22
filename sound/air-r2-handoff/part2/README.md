# Air battle sound, release 2, part two (sound director, 21 Nov 2026)

Eighteen commits for the 3D game, **release 2 only**. They go on top of `claude/project-thread-2bdb2m-unreal` at
**f591d2dc** (the air scene's pushed stack: patch one, the limiter header, the ships firing back, the -Wshadow renames).
Checked: `git am` of the eighteen onto f591d2dc gives exactly the sound director's tree.

| Patch | What |
|---|---|
| 0001 | Part two: rounds striking planes, close flak, big moments dip the rest, dead engines thin the drones, the battle's own music (Zero Hour, contact, levels by the fight's meter, the stingers). Adds `Sounds/Real/grenade_blast.wav` and its credits row. |
| 0002 | mix.json v5 with the ground sound thread's `land` block. |
| 0003 | The independent review's findings, mended (below), and the notes in mix.json. |
| 0004 | mix.json only: the ground sound thread's land block of 19:25Z (machine-gun bursts end with a 0.04 s fade). |
| 0005 | The pre-check's two -Wshadow stops: Open's voices line reuses its own `Here`; `Lay`'s pitch parameter is `PitchBy`. |
| 0006 | mix.json only: the ground sound thread's land block of 19:36Z (the Garand's reload heard from its clip recordings; the squad leader's Thompson cue, MP 38 takes standing in). |
| 0007 | mix.json only: the ground sound thread's land block of 19:39Z (`land.smgCue.leaderRoles`: officer and leader; notes say the reload and the Thompson switch on at the first clip cue). |
| 0008 | The master limiter's detector is digital (`bAnalogMode = false`) with 5 ms of look-ahead: with the default analog detector and 3 ms, the ground scene's second film had one transient 1.5 dB past the ceiling for 1.3 ms. The same as the ground fix. |
| 0009 | mix.json only: `limits.worldTotal` 64 (was 40; the engine plays 128 voices now, and at 40 the ground film dropped 892 of 3,725 shots, all off screen), and the ground sound thread's land block of 19:46Z (`land.bodyCues.field_gun_breech` null). |
| 0010 | mix.json only: the ground sound thread's land block of 20:12Z (`land.bodyCues.medic_tend` null; `land.armourStrikes` with an empty list, so nothing changes yet). |
| 0011 | mix.json only: `air.closeView.masterDb` 7.5 (was 3.1). The Bremen chase-view film (patch one only) measured -16.4 LUFS behind the plane, 4.4 dB under the -12 the close view was set for; see `../bremen-chase-film-measured.md`. Needs part two's one-take-per-wing guns and the limiter acting. |
| 0012 | The master limiter actually set: `Made->Settings = Settings` before adding it and `SetSettings` again after (SetSettings alone fills only the copy a preset's first effect refills from `Settings`, so the limiter most likely ran the default dynamics settings: the Bremen chase film passed the ceiling 28 times, for up to 96 ms, and the Mac builder confirms the recording takes the master after its effects). Channels linked by peak, as the ground's. Log line `Air battle sound: master limiter on, ceiling -1.5 dBFS ...` |
| 0013 | The limiter's log line reads back what the effect runs on (`GetSettings`, the copy the effects read), not the numbers asked for: `Air battle sound: master limiter on, read back from the effect's settings: ceiling -1.5 dBFS, knee 0.0 dB, look-ahead 5.0 ms, digital detector.` If the master still ran the engine's default dynamics it says `COMPRESSOR`, -6.0 and a 10 dB knee (the pre-check's point). |
| 0014 | mix.json only: the ground sound thread's land block of 20:54Z (five silent `land.bodyCues` for the bazooka team and the loader's carbine: `bazooka_fire`, `rocket_loaded`, `wire_contact`, `helmet_tap`, `rifle_shot`, each null with a note). No sound changes. |
| 0015 | The master limiter's attack near instant: `buses.master.limiterAttackMs` 0.02, `limiterLookAheadMs` 1, `limiterAnalogDetector` false, `limiterReleaseMs` 500 (was 120), read from mix.json (new helper `AirParts::MixFlag`). The Mac's first runs with the limiter really on still reached 0.00 dBFS: a 1 ms attack misses a gun crack's 0.1 ms first peak. Modelled in `../../limiter-model/README.md`: today's settings let +4.4 dBFS through on Bremen, these hold -1.42. If the engine clamps the attack at 1 ms it is still better than today (model: +2.8). Log line reads back attack and release too. |
| 0016 | mix.json only: the ground sound thread's land block of 21:51Z, round eight's Good takes (Brian, 21:41Z): `crack_woods_01` in `land.cracks.single`, `ricochet_whine_01..03` in `land.groundStrikes.whines`, `steel_hit_01..03` in `land.armourStrikes.smallArms`, new `land.landings` (mortar middle and far) and `land.engines` (Panzer IV and Tiger driving). The WAVs and the code that reads the new rows are on the ground thread's branch (7c474df5). |
| 0017 | Round seven stamped (Brian, 21:40Z): riding a B-17 plays `b17_interior_loop` (a real recording from inside one; craigsmith, CC0) open at +2 dB in place of its drone through the 200 Hz filter, the same loudness measured (`air.closeView.bed.interior`, `interiorGainDb`). Adds `Sounds/Real/b17_interior_loop.wav` and its credits row; `build_sounds.sh` brings it in. Until it is built in, the filtered drone plays. `plane_hit` and `hmg_burst_long` Cut: `air.roundsHit` stays silent. |
| 0018 | mix.json only: the ground sound thread's land block of 23:40Z (five silent `land.bodyCues` for the GI thread's new clips: `radio_squelch`, `radio_talk`, `thompson_mag_out`, `thompson_mag_in`, `thompson_cock`, each null with a note). No sound changes. |
| 0019 | Round nine stamped (Brian, 22 Nov 03:40-03:41Z): real B-17s replace the studio library's. Riding a B-17 plays one of two recordings from inside one, picked by the bomber's key: `b17_interior_whiteman_loop` (Wings Over Whiteman, 2017) or `b17_interior_sioux_loop` (the CAF's B-17 at Sioux City, 2022). `air.closeView.bed.interior` takes a take or a list (new loop in AirBattle.cpp: `InsideTakes`, `InsideCount`, `InsideTry`, `InsideTake`, through `AirParts::MixTake`); a take not yet built in gives way to the next, none keeps the filtered drone. A B-17 formation's engines: `b17_formation_drone_loop` (three B-17s and five P-51s flying over, 2018; `air.loops` B-17). All U.S. Air Force video on DVIDS, public domain, credited; mono 48 kHz 16-bit at -20.0 LUFS like the takes they replace. Adds three WAVs; the old `b17_interior_loop` and `b17_drone_loop` stay in the folder, unplayed. Run `Tools/build_sounds.sh`. Branch air-r2-part2 d38826f0, tree c318c52e. |
| 0020 | mix.json only: the ground sound thread's land block of 22 Nov 05:40Z (new `land.tankMg`: a tank's machine guns by army, the Sherman's roof .50 on `gunner_burst`; `land.mgClose.parts` gains `gunner_burst` and `maxim_burst`). The land block is the same as the ground branch's ba198643; the air block keeps 0019. No sound changes in the air battle. Branch air-r2-part2 0f4cf7ac, tree f00f9bd2. |
| 0021 | From the Bremen r2d film (22 Nov; `sound/air-r2/bremen-r2d-film-measured.md`). mix.json: `air.closeView.masterDb` 7.5 -> 9.5 and `bed.gainDb` 6 -> 1, so the ridden engine is 3 dB under the film and the guns stand out of it (re-mixed from the film through the game's limiter: chase -14.1 LUFS, bursts 7.2 LU over the bed, was 5.1). AirBattle.cpp: a gunner's twin mount (guns of one aircraft, kind and mount within a metre this frame) is heard as one take at the square root of its guns, under a mount key out of the guns' range, stopped with its last gun through WingOf/WingLive, as a fighter's wing already is (a B-17's twin turrets laid two takes in step, +6 dB). Notes corrected; `air.closeView._notBuilt` added. Reviewed on the cloud air_show test: no key collisions, no split mounts on the three battles; a split that only the 150-gun cut could make is closed. Resulting tree b850829b. |
| 0022 | mix.json only: the ground sound thread's land block of 22 Nov 12:46Z. `land.mgClose.moreParts` (maxim_burst's first two bursts, 0.03-0.66 s and 0.9-1.6 s) and its `_moreParts` note; nothing else changes, and the air block is untouched. On air-r2-part2 as 8a179f88; applies cleanly on r2g's mix.json (= 0021). |
| 0023 | mix.json only: the ground sound thread's land block of 22 Nov 19:09Z. `land.mgClose.moreParts` gains mg42_burst_01 and mg42_burst_02 (the Kursk fault), and new `land.voices` (pooled players: pool false, warm 256, most 384; off until the 3D pre-check and a Mac sound-on comparison pass). The land block equals sound/mix/land-block.json; the air block is untouched. On air-r2-part2 as 1f39c19a; applies cleanly on r2h 24b8d1b9. |

`code-only.diff` is the same without the wav and mix.json, for reading.

## What the review found, and what 0003 does

An independent review read both patches; each finding below was upheld by two readers or checked by hand.
Two compile errors it found (the shadowed `Table` and `Heard`) were already mended by the air thread in f591d2dc.

**Voices**
- `AudioMaxChannels=128` only lowers the count: the engine plays the smaller of it and the audio quality level's
  MaxChannels, 32 by default. DefaultEngine.ini now also sets `[/Script/Engine.AudioSettings]` with one quality
  level at 128. The scene logs `Air battle sound: the engine plays up to N voices at once.` when it opens.
- Voice priorities were set after each sound had begun, which does nothing (Play hands the priority to the engine).
  Every take now goes through `AAirBattle::Lay`: made with `UGameplayStatics::CreateSound2D`, given its priority,
  placed, then `Play`. A placed take past its family's silent distance is not made, as the engine's own spawn did.

**Guns**
- A fighter's fixed guns of one kind are one take, at the square root of how many fire. Before, each barrel laid
  the same take on the same frame: four barrels added +12 dB in step (eight, +18) and hit the limiter. The wing's
  take stops with its last barrel. Turret and hand-held guns are unchanged.
- Rounds passing crack by the firing aircraft, not per barrel; the same take never twice inside `air.sameTakeGap`;
  and the line runs past the guns' convergence to `air.roundsPassing.reach` (1000 m, new in mix.json).
- The tally line now separates `out of earshot` from `takes that could not play` (a missing recording).

**Levels**
- The battle's level (`MasterGain`) eases by the clock, about 1 s, not 10% a frame.
- Formation engine loops use the mix's `air.loopsFamily` (air_far, 14 km) instead of the aircraft family (6 km).
  A formation that began out of earshot used to stay silent all battle; it is now begun when it comes in range.
- Dead engines are counted out of the flight core's own engine count for the shape (`AirShow::ShapeOf`).
- A flak burst within `closeFlak.within` is always heard (a farther burst no longer takes its `soundEvery` slot).
- The master limiter's knee is hard (`KneeBandwidthDb = 0`). (0008 makes its detector digital with 5 ms of look-ahead.)

**The battle's music** (CampaignGameMode)
- Zero Hour resolves into The Advance at contact, whatever turn the battle pieces stood at.
- The theme no longer comes back at the opening screen's level (+16.5 dB) under the scene after a stinger; it comes
  up when the scene closes (`bBattleSceneUp`).
- Level changes no longer cut a piece's fade-in short, so contact's 3 s crossfade takes 3 s (`FadeLeft`).
- Without the score's battle pieces the music goes on as it was (it used to restart the map's piece).
- A battle begun during the last one's stinger gets its score.
- A stinger begun again by the music switch is heard out.

Not changed, for the record: the attenuation's low-pass has two points in Unreal, so the mix's middle lpf points are
not used (far guns stay brighter than the mix's 600 m point); and `rifle.wingOfFourOrMore` is not built.

## For the pre-check

Cleared again 20:28Z 21 Nov on 0001 to 0009 (0008's `bAnalogMode` is a member in 5.8; look-ahead 5 ms is inside the header's 0 to 50 ms; mix.json: 25 values added, none gone, one number changed, `limits.worldTotal`). First cleared 19:37Z on 0001 to 0005: all apply clean, 45 files compile under the Mac's flags with 0 errors and 0 new warnings, and the link step is clean. 0006, 0007, 0009, 0010 and 0011 are data only. 0008 adds one line of code: `Settings.bAnalogMode = false` (and `LookAheadMsec` 3 to 5). Left for the Mac: the header tool on the new members, the AudioMixer dependency, and the ini's 128 voices.

Cleared again 20:58Z 21 Nov on 0001 to 0013 (tree 3281665f, the sound director's exactly): 45 files under the Mac's flags on 5.8's headers, 0 errors, 0 new warnings, link clean; `LinkMode` Peak, the preset's public `Settings`, the local `MadeLimiter` and its non-const `GetSettings()` compile as written. The read-back line prints once, in Open(), right after the add; the next film's peaks are the proof. 0014 is mix.json only (10 values added under land). Still for the Mac: the header tool, the AudioMixer line in Build.cs, both `Air battle sound:` log lines, and how it sounds.

Cleared again 21:51Z 21 Nov on 0001 to 0015 (tree d786f7f7): 45 files, 0 errors, 0 new warnings, link clean; `MixFlag` compiles against 5.8's FJsonObject. The engine takes the 0.02 ms attack as given (the attack's ClampMin is editor meta only) and its detector reads before the look-ahead delay, so `../../limiter-model/` is the engine's code. To listen for on the Mac: with a one-sample attack the gain drops in one step 1 ms before each crack leaves the delay (the engine does not smooth the gain across the look-ahead). The Mac must show the read-back line (look-ahead 1.00 ms, attack 0.020 ms, release 500 ms, digital) and a capture peaking under the ceiling.

Cleared again 22:01Z 21 Nov on 0001 to 0017 (tree 72214142): 45 files, 0 errors, 0 new warnings; 0017's new locals shadow nothing; mix.json 1,073 to 1,098 values, none gone, no number changed. A ridden B-24 keeps its filtered drone (no interior take). The Mac must run Tools/build_sounds.sh to bring `b17_interior_loop` in; without it the log says it is missing once and the drone plays.

New Unreal calls to check against 5.8's headers:
- `UGameplayStatics::CreateSound2D` (8 arguments), then `bIsUISound`, `bAllowSpatialization`, `AttenuationSettings`,
  `SetWorldLocation`, `bOverridePriority`, `Priority`, `Play(From)`.
- `#include "AudioDevice.h"`, `UWorld::GetAudioDevice()` as `FAudioDeviceHandle`, `IsValid()`, `->GetMaxChannels()`.
- `FSubmixEffectDynamicsProcessorSettings::KneeBandwidthDb` and (0008) `::bAnalogMode`; (0012) `::LinkMode` = `ESubmixEffectDynamicsChannelLinkMode::Peak`, and `USubmixEffectDynamicsProcessorPreset::Settings` as a public member; (0013) `USubmixEffectDynamicsProcessorPreset::GetSettings()` (EFFECT_PRESET_METHODS, returns the settings copy); (0015) `FJsonObject::TryGetBoolField`, `::AttackTimeMsec` below 1 ms (any clamp?).
- `AirShow::ShapeOf` from AirBattle.cpp (with a static `std::map` cache).
- CampaignGameMode.h: new plain members (no UPROPERTY) and `FadeLeft`.

## For the Mac

1. Copy `zero_hour.wav`, `battle_won.wav` and `battle_lost.wav` from `/mnt/project-files/music/game-wav/` into the
   game's `Music/Score`, then run `Tools/build_music.sh`.
2. Run `Tools/build_sounds.sh` (it imports `grenade_blast`).
3. Rebuild. In the Bremen re-record, keep part of it behind a plane, and send every log line that starts with
   `Air battle sound:` and `Music:`.

The sound director then measures: the voices line (128 wanted), the Fw 190's shot transients, close-view loudness,
peaks under the limiter, and Zero Hour into The Advance at contact. Then it goes to Brian's ear.

## Merge note

The ground branch adds its own `grenade_blast` credits row. When the two meet, keep one row (the same file and
source).
