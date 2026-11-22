# GS6: battle clouds turned on the graphics chip (release 2)

Lane "Move battle clouds onto the graphics chip". Brief: production/polish-priorities-1122/lanes/gs6-sky-gpu.md.
Same clouds, same count, same sizes: only WHERE each puff is turned to the eye moves (game thread -> vertex shader).

## What is here
| Folder / file | What | Base |
|---|---|---|
| `patch-uhhxrn/` (3 patches) | PATCH A, ground battle (LandBattle.cpp LaySky/DrawSky) | claude/project-thread-uhhxrn 3b17b852 |
| `patch-r2h/` (2 patches) | PATCH B, Pearl/Midway air scene (AirBattle.cpp sky) | claude/project-thread-2bdb2m-unreal 24b8d1b9 |
| `gs6-uhhxrn.bundle` | patch A as a git bundle | 3b17b852.. |
| `AirSkyChip.inl`, `HANDOFF.md` | the shader text alone; the design note | |
Also on branch claude/project-thread-75mdq4 under handoff/gs6-sky-chip/. No other thread's branch was pushed.

## The switch
- `-TheaterSkyChip=0` brings back the old way (the two rooms turned on the CPU). Default in the code: **1 (chip)**
  when `M_CloudTurned` exists; without it, or with soft discs, the old way runs and the log says so. **The owning
  thread picks the default** (flip the `ChipAsked = 1` line to 0 to ship it off).
- `-TheaterSkyChipSort=0.025`: how stale the far-to-near order may get (share of the nearest puff's distance).
- Content: new material `M_CloudTurned` (Tools/build_content.py, `build_cloud_turned`, listed after M_Highlight,
  away from the shared tail). Mac: `TC_REBUILD=M_CloudTurned`. A failed build deletes the half asset and says why.

## Numbers ("cloud" = this container, not the Mac)
| | Before (rooms) | After (chip) |
|---|---|---|
| Game-thread cost of one sky update | 2.70 / 2.95 / 3.22 ms a turn (Bremen / Midway / Oahu, cloud); Mac E5: 1.98 cool, 5.40 hot | one table lay: 0.29 ms ground, 0.41-0.61 ms air recordings (cloud, worst eye) |
| Updates in 30 s of ground shots (dolly, crane, orbit, aerial) | 131 turns (cumulus), 129 (deck) | 25 lays each |
| Puffs / points re-sent per update | near turn ~800 puffs = 26,400-33,600 points; far turn 3,200 puffs; ~158 MB of points per 30 s | **0 points**; a table of at most 2,780 puffs (~356 KB); 7-8 MB per 30 s |
| Shadows on the sea (air scene) | painted with the far room | painted alone, identical point for point, 0.24 ms, as loosely as the far room |
| Painter's own timing rows ('the sky turned to an eye', 'the sky's shapes') | unchanged: the painter is untouched (re-run: 2.78 / 3.01 / 3.36 ground head; r2h 1.36-3.27) | n/a on the chip path |
The GPU's new share (a vertex shader on 6,000 x 33 = 198,000 points a frame, most collapsing at once) can only be
measured on the Mac.

## Checks done
- Tools/air_show/air_sky_chip_test.cpp (in run.sh): compiles the SAME AirSkyChip.inl as C++ and checks every point
  the chip draws against Painter::Paint. Ground: 1,263,372 points, 0.010 m at most apart, colour within 1 of 255.
  r2h (lids, small clouds, drawn-out rags): 1,000,428 points, same limits. A table read anywhere within its slack =
  a fresh lay (0 of 10 million points differ); every puff drawable at the 400 m margin is in the table; room 6,000
  slots, 2,780 used at most. ALL CHECKS PASSED on both, g++ and clang 18, -Wall -Wextra -Wshadow -Werror.
- Patch A: pre-check's Mac-flags compile + link + module check PASS (18:14Z). Patch B: asked.
- Ground land tests (land_show, land_field, land_play) pass on 3b17b852 + patch A (they do not compile the changed files).
- The Custom node's HLSL front end: glslang compiles .inl + glue (structs at global scope). The struct-in-function
  form the Custom node uses needs DXC: **only the Mac's content build can check it.**

## Mac run asks (through the production lead and coordinator; laptop lid open)
1. Content build with `TC_REBUILD=M_CloudTurned`. Log: "M_CloudTurned: N vertex shader instructions", no shader errors.
2. Ground cut 6 twice, same cameras: default and `-TheaterSkyChip=0`. AD compares frames against her figures (small
   clouds 1.5-2:1; sea shadows 0.81-0.84 vs 0.75 is the air scene's). Log: "Land battle sky, 30 s, on the graphics
   chip: N table lays, X ms a lay" against the old "near turns ... ms a turn"; frame and GPU times.
3. Air scene `-TheaterStage=midway` (and Oahu, Bremen) A/B the same way. Log: "Air battle sky, 5 s, on the graphics
   chip: ... the slowest X ms" against the rooms' line; frame and GPU times.
If the clouds vanish or the log shows a shader compile error for M_CloudTurned: run with `-TheaterSkyChip=0` and send
me the error lines.
