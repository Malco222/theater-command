# Mix proposal: the new cues and layers (not applied)

Written 19 November 2026 in the thread "Add music and richer battle sound". **Nothing here is in `../mix/mix.json`
yet.** Each item goes in only if Brian stamps the sound it uses Good on the
[listening page](https://claude.ai/artifact/EjLienLHJN29HKCPve1W4U). Gains are starting points, to be set by ear in
the game. The air battles thread is preparing its own asks for `mix.json` `air`; item 4 should be settled with it.

## 1. Music (needs the score cues stamped: The Field Is Ours, The Line Gives Way, Zero Hour, The Long Front)

- `music.end.stinger`: `{"won": "battle_won", "lost": "battle_lost"}` in place of the pack's `victory` and `defeat`
  (both are American bugle and band calls, so they play for every nation). The ducking stays as it is: everything
  else 10 dB under, music back 1 s after.
- `music.beforeContact`: play `zero_hour` (looping) instead of the battle pieces, at 0 dB; at first contact,
  crossfade into `advance` or `trench_line` over 3 s, then the existing `onContact` -8 dB. Zero Hour waits on A,
  the dominant of The Advance's D minor.
- War-map mood list: `silent_front`, `standing_to`, `long_front`, taking turns.

## 2. Beds (needs `front_rumble_loop`, `naval_guns_far_loop` and `air_battle_far_loop` stamped)

Add to `beds.list`, same rules as the others (silent below `in`, full from `full` to `hold`, gone above `out`):

```json
{"name": "front_rumble_loop", "in": 0.0, "full": 0.0, "hold": 1.0, "out": 1.01, "gainDb": -6, "land": true,
 "note": "the war beyond this battle, 3 to 14 km off; never leaves, so the battle has a low floor that never goes silent"},
{"name": "naval_guns_far_loop", "in": 0.0, "full": 0.0, "hold": 0.4, "out": 0.7, "gainDb": -8, "sea": true,
 "note": "another fight beyond this one, 6 to 18 km off; rides on top of sea_day_loop"},
{"name": "air_battle_far_loop", "in": 0.02, "full": 0.15, "hold": 1.0, "out": 1.01, "gainDb": -4, "air": true,
 "note": "the air battle as the strategy camera hears it, 2 to 6.5 km off; rises with the meter (air battles thread, feel-benchmark gap 2)"}
```

Why it never leaves: the art director's grit direction (19 Nov, from Brian's reference clips) asks for a low
bed that never drops more than about 12 dB under the peaks. `front_rumble_loop` measures 47% of its energy under
150 Hz with a 5.4 dB loud-to-quiet gap; the pack's `battle_loop` measures 26% and 16.9 dB (README, grit table).

`beds.beforeContact` would then read: `rifles_far_loop` at -14 dB and `front_rumble_loop` at its gain, in place of
the single far salvo every 15 to 30 s (`far_guns_01..03`), which the rumble already holds.

## 3. Distance families and places (needed by items 2 and 4)

```json
"distance": {
  "horizon": {"full": 800, "silent": 40000, "lpf": [[800, 12000], [3000, 2600], [8000, 1100], [15000, 650], [30000, 400]],
              "reverb": [[0, 0.15], [15000, 0.75]], "note": "guns and blasts past the battle's edge"},
  "air_far": {"full": 300, "silent": 14000, "lpf": [[300, 20000], [2500, 6000], [6000, 2400], [12000, 1100]],
              "reverb": [[0, 0.03], [12000, 0.35]], "note": "formations at height: a B-17 group heard from 14 km"}
},
"places": {
  "sea_far": {"decaySeconds": 4.0, "preDelayMs": 140, "damping": 0.55, "note": "a gun far off over open water"},
  "horizon": {"decaySeconds": 3.2, "preDelayMs": 120, "damping": 0.6, "note": "guns beyond the hills"}
}
```

## 4. Air (needs Bombers overhead and Air support stamped; settle with the air battles thread)

- A formation's engine loops use `air_far` when it is more than 300 m from the listener, so a bomber stream is
  heard coming from kilometres off, as in Bombers overhead.
- The rest (fighter passes within 300 m, flak firing and bursting 1.5 to 4 s later, `plane_down` and
  `plane_explode` as big moments) is already in `mix.json` `air`; the scenes follow it.

## 5. Grit (from the art director, 19 Nov 03:15Z; needs Brian's approval like the rest)

- **Fix first: the thick-of-it bed.** The pack's `battle_loop`, which plays in the thick of a land fight, measures
  26% of its energy under 150 Hz and 6% over 2 kHz, with a 16.9 dB loud-to-quiet gap. Brian's reference clips are
  46 to 75% low, 2 to 4% high, and never drop to silence. Item 2 (front rumble that never leaves) is part of the
  answer; a heavier real take for `battle_loop`, or a low layer under it, is the rest. Recorded library only.
- **Debris timing for big guns.** `impacts` lays the falling earth from 0.6 to 0.9 s after the flash. The effects
  thread's new debris shower lands 1.2 to 3.3 s after a 105 mm's flash. Start the patter at about 1.2 s for big
  guns, or key it to the shower's landings, so you hear what you see.

