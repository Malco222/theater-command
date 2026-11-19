# Mix proposal: the new cues and layers (not applied)

Written 19 November 2026 in the thread "Add music and richer battle sound". **Nothing here is in `../mix/mix.json`
yet.** Each item goes in only if Brian stamps the sound it uses Good on the
[listening page](https://claude.ai/artifact/EjLienLHJN29HKCPve1W4U). Gains are starting points, to be set by ear in
the game. Item 6 needs no stamp, only Brian hearing v3. The air battles thread's v3 air patch is reviewed at the end;
it goes in with this as `mix.json` v3 once Brian has heard it.

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
{"name": "air_battle_far_loop", "in": 0.02, "full": 0.15, "hold": 1.0, "out": 1.01, "gainDb": -12, "air": true,
 "note": "the air battle as the strategy camera hears it, 2 to 6.5 km off; rises with the air meter. -12 under the air master (+8 at 1,500 m), so -4 net on the map (air battles thread's v3 patch, section 4)"}
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
  "air_far": {"full": 100, "silent": 14000, "lpf": [[300, 20000], [3000, 3500], [6000, 1500], [12000, 800]],
              "reverb": [[0, 0.02], [6000, 0.3]], "doppler": true,
              "note": "a formation's engine loops at every distance: the aircraft family's curve, heard out to 14 km instead of fading out by 6. Under 3.6 km it is the aircraft family exactly (aircraft starts its fade there)"}
},
"places": {
  "sea_far": {"decaySeconds": 4.0, "preDelayMs": 140, "damping": 0.55, "note": "a gun far off over open water"},
  "horizon": {"decaySeconds": 3.2, "preDelayMs": 120, "damping": 0.6, "note": "guns beyond the hills"}
}
```

## 4. Air (settled with the air battles thread, 19 Nov 07:10Z; see "Review of the v3 air patch" below)

- A formation's engine loops use `air_far` at every distance, so a bomber stream is heard from kilometres off, as in
  Bombers overhead. There is no switch between families: the first draft switched from `aircraft` to `air_far` at
  300 m, and since `aircraft` is already 7.2 dB down there (full to 100 m, rolloff 0.75) while the first `air_far`
  was full to 300 m, a loop would have jumped 7.2 dB. `air_far` is now the aircraft curve heard farther, so under
  3.6 km nothing the air battles thread measured moves.
- `plane_explode` is **not** a big moment (the first draft said it was; `big.names` has `plane_down` and
  `aircraft_break_up` only) and stays out: on the air battles thread's test raid the nine `plane_down` already hold
  the duck for 13% of the 40 s, before release, and the explode losses would roughly double that. The air patch's
  `explosionsYield` keeps planes lost instead.
- The rest (fighter passes within 300 m, flak, planes lost) is the air battles thread's v3 patch.

**Built with the first draft.** `air_battle_far_loop` and `scene_bombers` were placed with the first `air_far` (full
to 300 m, a brighter low-pass). In the bed every source is 2 to 6.5 km off, so the level change is the same for all
of them and the loudness match takes it out; the low-pass is the difference, and a rebuild on the new curve would
be darker. Bombers overhead is a mock-up: in the game the formation sits 7.2 dB lower past 300 m than it does there.
The tool (`soundscape.py`) keeps the first draft so the files rebuild as they are.

## 5. Grit (from the art director, 19 Nov 03:15Z; needs Brian's approval like the rest)

- **Fix first: the thick-of-it bed.** The pack's `battle_loop`, which plays in the thick of a land fight, measures
  26% of its energy under 150 Hz and 6% over 2 kHz, with a 16.9 dB loud-to-quiet gap. Brian's reference clips are
  46 to 75% low, 2 to 4% high, and never drop to silence. Item 2 (front rumble that never leaves) is part of the
  answer; a heavier real take for `battle_loop`, or a low layer under it, is the rest. Recorded library only.
- **Debris timing for big guns.** `impacts` lays the falling earth from 0.6 to 0.9 s after the flash. The effects
  thread's new debris shower lands 1.2 to 3.3 s after a 105 mm's flash. Start the patter at about 1.2 s for big
  guns, or key it to the shower's landings, so you hear what you see.

## 6. The battle limiter and true peak (mix-only; needs nothing stamped)

- `buses.master.limiterCeilingDb` -1.0 to **-1.5**. The limiter works on samples, as UE's submix limiter does, and
  between samples the air battles thread's close view reaches -0.8 dBTP (-0.9 looking out). -1.5 keeps the true peak
  under -1. On land it lowers only the hottest peaks, by 0.5 dB. If the engine's limiter can look between samples
  (oversampled, true peak), set it to -1.0 dBTP instead.

## Review of the v3 air patch (air battles thread, `air-battles/feel/mix-v3/`; reviewed 19 Nov 07:10Z)

**Accepted, with the changes below.** It goes into `mix.json` as v3 once Brian has heard it (the strategy camera
before and after, and the close view, go on the listening page). The rows that wait for Brian's stamp or the 3D
close view keep their `status`.

Taken as written: heavy takes cut to 0 to 0.85 s; flak bursts only from the picture and the clock only for the guns
on the ground; `flashToBang.air` (the ISA speeds check: 340, 333, 323, 311 m/s); `cutFade` 0.3; `bomberDronesFirst`
4; `passesPerGroup` 3 (planes within `sameTakeGap` still share a take, so nothing combs, and 3 catches 5 of 7 passes
40 m behind against 4 at 2); `air.intensity`; the air master (+8 dB at 1,500 m); the far bed at -12 (item 2). The
close view as WAITING: own guns dry, exempt and at 0; `grenade_blast` within 100 m as a stand-in until a close flak
recording; the compressor; `masterDb` 3.3; width 1.0. The far bed keeps the echo the tool gave it: at 2 to 6.5 km
its send is 0.08 to 0.2.

Its five open calls:

1. **`explosionsYield`: kept.** A plane lost is the moment; a flak burst is one of 146. It costs 2 to 4 heard flak
   bursts in each view. Asked of the air battles thread: the same with `air.limits.explosions` 7 (air battle only),
   on map and close. If that wins the flak back at no other cost, 7 goes in. **Result (07:07Z): the limit stays
   at 5.** At 7 flak heard rises (47 to 52 on the map, 18 to 22 close) but a plane lost heard is lost on the map (a
   far explosion at 2,050 m) and in the close view (a `plane_down` at 283 m); at 6 the map still loses it.
2. **`limits.big`: deleted.** Big moments are never capped: `big._how` says they always play and `battle-mix.md`
   section 4 says they are never the ones dropped, so the 2 contradicted both. The duck is one state: more big
   moments at once hold it longer, never deeper (the renderer already takes the deepest, not the sum).
3. **Far planes lost do not cut through a fight beside the camera** (no distance floor), as on land. The opposite
   is a fault: in the close view and 40 m behind, 6 of the 9 `plane_down` are 1.1 to 1.9 km off and unheard
   (margins -12 to -26 dB), yet each ducks everything around the camera for about 2.3 s. Asked of the air battles
   thread: render a rule where, in the air battle, a big moment farther than 1,000 m from the listener still plays
   but does not duck. It must keep 9 of 9 planes lost heard on the map; if it does not, it applies only with the
   camera within 300 m of what it looks at (no air master). **Result (07:07Z): taken, in every view.** The map and
   the fighters keep 9 of 9; the duck falls from 13.9 s to 4.7 s of the 40 (35% to 12%); loudness range drops 0.4 to
   1.0 LU. Its cost: a `plane_down` 311 m off that rode a far one's duck misses the line by 0.4 dB looking aft, and
   close flak heard goes 18 to 16. The air battles thread folds it into its draft 5.
4. **True peak:** item 6 above.
5. **The 7.2 dB jump at 300 m:** item 4 above. Asked: one render with the camera at 3 km, where drones past 3.6 km
   now rise instead of fading out. **Result (07:07Z):** at 3 km the render is sample-identical (the engines limit
   gives all 8 slots to loops 1.6 to 1.9 km off); with the camera at 8 km the drones 4.6 to 4.9 km off rise 2.0 to
   2.8 dB, and nothing else moves.
