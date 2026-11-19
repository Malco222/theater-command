# Soundscapes: two far battle layers and four listening scenes

Made 19 November 2026 in the thread "Add music and richer battle sound", on Brian's "all 4" to the coordinator's
list (item 4: a second pass on battle sound plus original score cues; the brief asked for distant artillery
rumble, aircraft flybys and ship guns). **Not wired into either game, and `mix.json` is unchanged**, until Brian
stamps them on the [listening page](https://claude.ai/artifact/EjLienLHJN29HKCPve1W4U). What would change in
`mix.json` if he passes them is in `mix-proposal.md`.

## What is here

| File | What it is | Length | Loudness |
|---|---|---|---|
| `front_rumble_loop` | A new bed: batteries firing and shells landing 3 to 14 km off, rolling like thunder | 32 s, loops | -27 LUFS |
| `naval_guns_far_loop` | A new bed: battleship and cruiser salvos 6 to 18 km off across water, with the sea's long echo | 32 s, loops | -26 LUFS |
| `air_battle_far_loop` | A new bed (added 04:50Z, asked by the air battles thread): two bomber streams droning, fighters passing, gun bursts and heavy flak 2 to 6.5 km off, as the strategy camera hears an air battle | 32 s, loops | -26 LUFS |
| `scene_horizon` | The guns on the horizon: behind the line before a battle; a battery 400 m left, its shells landing 4 km ahead, a column and a lorry, over the new rumble | 60 s | -21.1 LUFS |
| `scene_bombers` | Bombers overhead: a flak site under a daylight raid; B-17s 3 km up passing over with Doppler, P-51 escorts, flak, one bomber hit | 72 s | -19.0 LUFS |
| `scene_sea` | Gun duel at sea: a cruiser's deck; her salvos, enemy shells short, over, straddling, a hit aft, and the new far sea fight beyond | 70 s | -19.3 LUFS |
| `scene_strike` | Air support (Brian's ground support ask): two P-47s strafe and bomb the enemy line 250 m ahead, heard from behind ours | 48 s | -19.0 LUFS |

Folders: `wav/` (48 kHz, 24-bit), `mp3/` (for listening), `checks/` (one JSON per file with every sound placed:
its time, distance, gain, low-pass and echo; a picture with the marked moments; `credits.json`).

The scenes are mock-ups for the ear: what a moment in the game could sound like with the new layers and the
approved pack together. In the game the sim places each sound itself, by the rules in `../mix/`.

## How they were made

- **Only real recordings from the approved pack** (`../battle-pack/`): 59 different ones across the seven files,
  listed per file in `checks/credits.json`. All are already credited in the pack's credits; none carries a
  non-commercial-only licence. Nothing is synthesized, and nothing comes from films, series or games.
- **The mix rules of `../mix/mix.json`**, applied by the tool `sound/tools/soundscape.py` (repository branch
  `claude/project-thread-trbi5p`): level by distance (rolloff 0.75), the low-pass closing with distance, more echo
  with distance, sound arriving distance / 343 s after the flash, and Doppler on anything moving (set to 60% of the
  true shift).
- **Two new distance families** for things farther than the current ones reach: `horizon` (full to 800 m, gone at
  40 km, the low-pass down to 400 Hz at 30 km) and `air_far` (formations at height: full to 300 m, gone at 14 km).
  **Two new places**: `sea_far` (a 4 s roll over open water) and `horizon` (a 3.2 s roll, guns beyond hills). The
  echo is a decaying noise tail built by the tool, as in `mix/preview/`; no recorded room.
- **Loops** are built round in a circle (an engine held for the whole loop has its overrun crossfaded into its start), so the end runs into the start, and are not limited, so the join cannot
  click. Scenes go through a limiter at -1 dB.
- Rebuild: `python3 -I sound/tools/soundscape.py <out folder> [names]` from the repository root, with the pack at
  `/mnt/project-files/sound/battle-pack/`. It needs `numpy scipy soundfile pyloudnorm` and ffmpeg.

## What was measured (not heard: Brian's ear is the test)

| File | LUFS | Range (LU) | Energy under 200 Hz | Lost on laptop speakers | Loop join (a click scores over 9) |
|---|---|---|---|---|---|
| `front_rumble_loop` | -27.0 | 2.9 | 78% | 4.9 dB | 2.8 |
| `naval_guns_far_loop` | -26.0 | 7.8 | 60% | 3.1 dB | 3.4 |
| `air_battle_far_loop` | -26.0 | 2.0 | 52% | 2.5 dB | 7.7 |
| `scene_horizon` | -21.1 | 9.5 | 69% | 3.4 dB | |
| `scene_bombers` | -19.0 | 8.3 | 51% | 2.3 dB | |
| `scene_sea` | -19.3 | 9.4 | 46% | 1.6 dB | |
| `scene_strike` | -19.0 | 8.1 | 50% | 2.0 dB | |

How far the marked moments stand over each scene's middle level (per second): horizon, the shells landing +8 dB
and the battery +5; sea, her salvo +8.8, shells short +9.3, straddled +10.6, hit aft +11.6; strike, the first P-47
+8.3, strafing and bombs about +5.7; bombers only +1 to +4, as the busiest scene. The formation's drone falls about
20% in pitch as it passes (1.12 to 0.90).

**Against the grit targets** (art director's `grit-direction.md`, 19 Nov, from Brian's reference clips: 46 to 75%
of the energy under 150 Hz, 2 to 4% over 2 kHz, the quiet floor 7 to 12 dB under the peaks). Same measure here:
share of energy under 150 Hz and over 2 kHz, and the gap between the loud (95th percentile) and quiet (10th) 0.4 s
levels. On Brian's bomber-waist clip this measure gives 45% / 4.7% / 7.0 dB (the art director's 46% / 4% / 11 dB):
the shares agree, the gap reads about 4 dB tighter than theirs.

| File | Under 150 Hz | Over 2 kHz | Loud to quiet gap | Against the targets |
|---|---|---|---|---|
| `front_rumble_loop` | 47% | 0.1% | 5.4 dB | A low wall that never stops: on target |
| `naval_guns_far_loop` | 45% | 0.0% | 24.1 dB | Low enough, but silent between salvos: needs the sea beds under it |
| `air_battle_far_loop` | 39% | 2.3% | 3.5 dB | A steady wall; a little under the clips' low share |
| `scene_horizon` | 51% | 2.9% | 12.6 dB | Close |
| `scene_bombers` | 40% | 1.6% | 10.6 dB | Close; a little light in the low end |
| `scene_sea` | 35% | 10.6% | 11.1 dB | Too bright: three times the highs of the clips |
| `scene_strike` | 33% | 4.3% | 11.3 dB | Light in the low end |
| pack `battle_loop` (in the game now) | 26% | 6.0% | 16.9 dB | The biggest gap: thin, crisp, with drops |
| pack `battle_far_loop` | 29% | 0.6% | 13.7 dB | Light in the low end |
| pack `sea_battle_loop` | 37% | 0.7% | 17.8 dB | Drops between guns |

## Known faults

- `front_rumble_loop` lives low: laptop speakers lose 4.9 dB of it, against 2.2 dB for the pack's
  `battle_far_loop`. On a laptop it will sound quieter than its number.
- `naval_guns_far_loop` drops almost silent between salvos (to about -60 dB). It is meant to ride on top of
  `sea_day_loop` and `sea_battle_loop`; alone it has gaps.
- Still no real recording of a shell coming in (the gap noted in `../mix/battle-mix.md` section 3).
