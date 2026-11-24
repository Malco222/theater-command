# Round twelve: real recordings for four old studio-library takes (24 Nov 2026)

On the listening page (https://claude.ai/artifact/EjLienLHJN29HKCPve1W4U, version 13, section "Real recordings for four
more old studio-library sounds", stamps in db collection "verdicts" under the card ids below). Nothing is wired. The old
takes stay in the game until Brian stamps a new one Good.

Source: the Mac's 22 Nov fetch of the round-two leads (zips /mnt/project-files/sound-leads-1..4*.zip; licences read from the
saved pages in /mnt/project-files/sound-leads-licences.md). Each take was cut and measured, then checked by independent
skeptics; `kept-with-checks.txt` holds every kept take's processing, measurements, card corrections and check notes.

## The twelve takes (wav/, 48 kHz mono 16-bit)

| card id | replaces | take file | source | licence / credit |
|---|---|---|---|---|
| r12_stuka | stuka_drone_loop (12 s loop) | stuka_drone_loop_r12_fs242179_1.wav | Freesound 242179, 139.5-151.95 s, left channel, 0.4 s loop crossfade | CC BY-NC 3.0: "Messerschmitt Bf 109 Startup and Taxiing" by Fight2FlyPhoto (freesound.org/s/242179) |
| r12_torp_shot | torpedo_launch (2.5 s) | torpedo_launch_r12_awmS04697_1.wav | AWM S04697 (HMAS Oxley), 4491.92-4494.46 s | AWM: personal, non-commercial, research and study use only. Credit "Australian War Memorial, S04697" |
| r12_torp_vent | torpedo_launch | torpedo_launch_r12_awmS04697_2.wav | AWM S04697, 4497.76-4500.37 s | as above |
| r12_torp_deck | torpedo_launch | torpedo_launch_r12_dvids665898_1.wav | DVIDS 665898, 20.96-22.28 s, left channel | public domain; video by MC2 Jeremy Graham, VIRIN 190312-N-HI376-1001 |
| r12_torp_far | torpedo_launch | torpedo_launch_r12_dvids665900_1.wav | DVIDS 665900, 0.73-3.49 s, +8.5 dB | public domain; video by MC1 Alexandra Seeley, VIRIN 190315-N-JN784-0001 |
| r12_dc_mortar_a | depth_charge_thrower (0.92 s) | depth_charge_thrower_r12_dvids729278_1.wav | DVIDS 729278, 1.875-2.944 s | public domain; video by Lance Cpl. Drake Nickels, VIRIN 191213-M-BH464-1001 |
| r12_dc_mortar_b | depth_charge_thrower | depth_charge_thrower_r12_dvids729278_2.wav | DVIDS 729278, 26.317-27.008 s | as above |
| r12_dc_close | depth_charge_thrower | depth_charge_thrower_r12_awmS04685_3.wav | AWM S04685 (mortar), 353.17-355.04 s | AWM: personal, non-commercial only. Credit "Australian War Memorial, S04685" |
| r12_run_tank | torpedo_run_loop (9 s loop) | torpedo_run_loop_r12_wolfsetT16_1.wav | Wolfset (figshare doi 10.6084/m9.figshare.25791978), 39.80-49.17 s, 5 Hz high-pass (DC only), 0.3 s crossfade | LICENCE NOT YET READ: confirm on the figshare page before wiring |
| r12_run_whine | torpedo_run_loop | torpedo_run_loop_r12_nps06_1.wav | NPS Glacier Bay clip (lead 06), 5.9-18.5 s, +10.6 dB, 0.5 s crossfade | LICENCE NOT YET READ (NPS: usually public domain; confirm) |
| r12_run_prop | torpedo_run_loop | torpedo_run_loop_r12_oxley10_1.wav | AWM S04697 segment 10, 1392.45-1402.96 s, 1.5 s crossfade | AWM: personal, non-commercial only |
| r12_run_outboard | torpedo_run_loop | torpedo_run_loop_r12_nps07_1.wav | NPS Glacier Bay clip (lead 07), 0.1-9.63 s, 20 Hz high-pass, 0.5 s crossfade | LICENCE NOT YET READ (confirm) |

Levels: one-shots matched to the old take's loudest 50 ms RMS (torpedo_launch -14.72 dBFS, depth_charge_thrower
-17.85 dBFS); loops matched to the old take's integrated loudness (stuka -20.0 LUFS, torpedo run -19.0 LUFS). Allowed
processing only: cut, fades, gain, mono (one channel), resample, loop crossfade, high-pass at or under 20 Hz. Nothing
synthesized, no EQ, pitch or time changes, no layering. `wav-sha256.txt` has the checksums (the shared folder may add a
C2PA chunk to WAVs; audio unchanged).

## What found nothing

- bomb_release: lead 24 (DVIDS 135969 B-52 bomb bay) has clanks and rattles only, no fall or landing.
- ship_sinking: leads 12, 13, 14 show no ship struck, burning, breaking or sinking in their sound (12 aircraft noise, 13
  cabin noise, 14 overloads after 17 s).
- aircraft_break_up: leads 25, 26 (NASA CID .ogv) have no audio track.
- stuka from lead 01 (Freesound 142898, CC BY 3.0): a second engine runs through the run-up; its steady stretch is 4.6 s only.
- torpedo_launch: cv_deck2 buried in ship noise; cw_deck camera wind.

## To wire, once stamped Good (release 2)

- The take WAV goes to Content/Sounds/Real under its own name; the old take's file stays (overwriting or deleting old
  Sounds/Real files needs Brian's OK).
- stuka: mix.json air.loops "Ju 87" -> the new name. Riding a Ju 87 plays it at the eye through air.closeView.bed.lpfHz
  (200 Hz) per AirBattle.cpp ~5737.
- torpedo_launch, torpedo_run_loop, depth_charge_thrower: listed in CampaignSounds.cpp:62 but not placed in mix.json in
  the code checked, so wiring means swapping the file the name points to, plus wherever the sea battle places them.
- credits.json + make_credits.py: Fight2FlyPhoto CC BY-NC line; AWM lines; DVIDS lines; NPS / Wolfset once read.
- Leads 06, 07, 09, 10 licences must be read on their pages first (round three's fetch list asks for them).

## Files

- page.py: builds the page section from index.html (run `python3 -I page.py <clips dir> <page in> <page out> <r12.json>`).
- r12.json: the card texts as published.
- clips/: the 12 comparison clips (old take, 1 s silence, new take) and 10 source excerpts, as on the page.
- kept-with-checks.txt: per-take processing, measurements, corrections and skeptic notes.
