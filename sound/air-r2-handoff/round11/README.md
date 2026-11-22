# Round eleven: rounds striking the plane (sound director, 22 Nov 2026)

For Brian's "i also want better sound of bullets riddling a metal plane" (04:07Z) and his Fix on round seven's
`plane_hit` ("doesn't sound like bullets riddling against metal"). Nothing here is in the game until he stamps it.

No real recording of rounds striking an aircraft's skin has been found. These are the nearest real sounds from
Mac fetch 1 (`sound/incoming/planehit-2026-11-22/`): thin metal struck hard by other things. Every card says so.

## Game takes (`wav/`, 48 kHz mono 16-bit)

| Take | From | Licence |
|---|---|---|
| plane_hit_hail_01..09 | corkyburger, Freesound 352281, hail on the metal flashing round a loft window (two Rode lavaliers; each take one channel) | CC0 |
| plane_hit_sheet_01 | Stereo Surgeon, Freesound 262516, a steel sheet hit with a steel implement | CC0 |
| plane_hit_sheet_02, _03 | EpicWizard, Freesound 264889, impacts on a metal shed door (Zoom H4) | CC0 |

Cut from the ranked strikes in the plane-hit triage (`r11.json` maps each take to its source cut), 0.25 ms fade in,
4 ms fade out, levelled to a 5 ms RMS peak of -11 dBFS with the peak held at or under -1.5 dBFS (the hail reaches
the peak cap first). Freesound HQ previews (MP3) are the source: originals need a Freesound sign-in.

## Listening-page cards

`r11_hail` (9 hail strikes) and `r11_sheet` (3). Each clip (`clips/`): the strikes one at a time, then three bursts as
the release 2 flight core lands them (a1acb552 `SendStrike`: a strike every U(0.05, 0.1) s until the damage begins;
here 0.3 s, 0.3 s and 0.8 s, the last bringing a bomber down), any within 0.08 s of the last heard dropped as
`air.roundsHit.every` does: 14 heard in 2.6 s, picked at random with no strike twice in a row (every strike once
before any repeats). Then those bursts over the B-17 interior (Whiteman, +2 dB) and over the fighter engine (+1 dB),
through the close view's chain after patch 0021 (master +9.5, common stage -6.1, mono 0.5 a channel) and the limiter
model, which barely acts (strikes alone peak at -4.1 dBFS a channel; strike over engine touches the ceiling once).
`build.py`, `page.py`; measurements `measured2.json` (energy centroid from each strike's onset, 20 dB fall on a 1 ms
envelope, stand-out over each engine above 2 kHz and across the band). An independent check (two skeptics) corrected
the first draft's wording and pace.

## To wire (only on Brian's Good)

- Copy the Good set's wavs to `Sounds/Real`, list them in `air.roundsHit.takes`, credit them.
- AirBattle.cpp picks with `FMath::RandRange(0, 11) % N`: with 9 takes, takes 1-3 come twice as often. Pick
  uniformly, and never the take just played (with 3 takes a repeat 0.08 s apart overlaps a copy of itself).
- The flight core emits Hit for rounds on aircraft (Text "air"), flak splinters ("flak"), ship AA ("ship", "kill")
  and ground and water spurts ("ground", "water"; Key 0). roundsHit plays on any Hit near the eye, so a strafing
  run's spurts would play metal strikes and take the 0.08 s window from a real one. Skip only the spurts:
  `E.Text != "ground" && E.Text != "water"` (keeps today's empty Text too). bOwn can become `E.Key == S.Subject.Plane`.

## Fetch 2 (22 Nov, `sound/incoming/planehit2-2026-11-22/`): measured, set aside

Each clip had a measurer and a skeptic (scripts and images in the session scratchpad, not kept). None shows a round striking metal.

| Clip | Verdict |
|---|---|
| NoiseCollector 237445 (CC BY 3.0) | The file is the BB striking a watermelon rind (`wmrind5`), not the chandelier cover. One overloaded event: a large sub-100 Hz thump, then a hard object ringing at 5.24 kHz. Not metal skin. |
| Sadiquecat 842997 (CC0, airsoft MP5 at a can) | Six bursts, one per shot, about 10 ms after the gun's spike, loudest on one mic. The skeptic could not separate a pellet striking the can from the muzzle on the barrel-exit mic (the page puts both mics on the gun). Doubtful. Clean edits if ever wanted: the 23.257 s and 23.131 s bursts (L). |
| designerschoice 835914 (CC BY 4.0) | Real strikes on one metal object (29 hits, phone recording), but nothing in the audio shows a gun, and it is an effects company's pack ("The Designer's Choice UCS Pack #11 - BULLETS"), which the standing rule excludes. Each strike also holds near its peak for 40-115 ms and falls 20 dB in 110-150 ms: slow for thin skin. |
