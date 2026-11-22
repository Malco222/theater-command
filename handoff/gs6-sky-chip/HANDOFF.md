# GS6 sky on the graphics chip: handoff

Status 22 Nov 18:35Z: RESUMED 17:37Z and BUILT. Patch A (ground) and patch B (air scene) are written, tested in the
cloud, and saved; patch A passed the pre-check's Mac-flags compile. See NOTES.md for the switch, the numbers and the
Mac run asks. Steps 1-6 below are all done. What is left: patch B's pre-check verdict, the Mac picture/timing runs,
and each owning thread choosing the default.

## What exists
- `AirSkyChip.inl` (this folder; also untracked in the lane's ground worktree, branch gs6-sky-chip-uhhxrn on uhhxrn 79bd6bc0).
  One text read by BOTH the C++ cloud test (through a float3/float4 shim) and M_CloudTurned's Custom node (HLSL):
  AirSky::Painter::Paint's per-point arithmetic for a sheet-shape puff (turn to the eye, light, fades, lens, base foot,
  melt, odd-point mend, sheet place), as a struct with methods. It covers the r2h painter's extras too (lids, Long,
  SmallSight) through flags, so the same file serves patch A and patch B.
  Checked: glslang's HLSL front end compiles it (structs at global scope). glslang rejects the struct-inside-function
  form UE's Custom node uses (a glslang bug; DXC takes it), so the Mac's content build is the real check.
- Before numbers (cloud, uhhxrn c3d9c358, alone): 'the sky turned to an eye' 2.70 / 2.95 / 3.22 ms; 'the sky's shapes'
  2.89 / 2.94 / 3.41 ms (Bremen / Midway / Oahu). Mac (audit E5): 1.98 ms a turn cool, 5.40 hot, 22-64 turns per 30 s,
  ~800 puffs (26,400 points) re-sent each near turn, 3,200 each far turn.

## The design (to pick up)
1. AirSky::Chip (new class, AirSky.h/.cpp): lays a float table (PF_A32B32G32R32F, 2048 texels wide). Row 0 header
   (sun way, paints x light / ink, reach, NearAll, FarAll, count, sides); then 8 texels a puff, far to near
   (layout at the top of the .inl). Gather = Paint's cloud loop with a 400 m margin (reach, haze, Lod shares), no
   per-point work. Lift easing as the painter's.
2. Mesh made ONCE: one PMC, Room x 33 points all at local 0 (two unreferenced corner points for bounds), the
   painter's own triangles; UV0 = (K, ring 0/1/2), UV1 = (slot's first texel column, row) (whole numbers < 2048, exact in
   half floats). Nothing re-sent per frame.
3. M_CloudTurned (new builder beside build_cloud, NOT at the shared tail): Custom node = .inl + glue (Table.Load x13,
   eye = TransformPosition(CameraPositionWS, World->Local) swapped X/Y and / UnitsPerMetre), WPO = TransformVector
   (Local->World) of the result; Colour and Sheet through two VertexInterpolators; pixel part as M_Cloud.
   On any build failure delete the asset so C++ falls back to the CPU rooms.
4. Re-lay (table upload only, ~0.5 MB) when the eye has moved 2-3% of the nearest drawn puff's distance (order),
   3 m of carry while a lift eases, 15 m when a cloud sits within 4 km of the target's clearing. Ground: no drift
   re-lays (clearing 35 km off, no Ground). Switch: `-TheaterSkyChip=0|1` (0 = old rooms); the owning thread picks
   the default. Tunable `-TheaterSkyChipSort=`.
5. Tests (new Tools/air_show/air_sky_chip_test.cpp, own block in run.sh): every point vs Painter::Paint at the same
   eye (pos < 0.25 m, colour/alpha +-2 bytes, uv +-2e-3); a stale table at a moved eye equals a fresh one by puff;
   timing rows (Wants us, Lay ms) and a 30 s ground eye path: turns and puffs re-sent, rooms vs chip.
6. Patch B: same on r2h 24b8d1b9 (AirBattle.cpp sky part only; shadows stay CPU, painted by a shadow-only Part).

Receivers unchanged: ground (patch A, before MB10), Pearl/Midway (patch B, after r2h), second pre-check (both).
  60:  ok   the sky turned to an eye within budget                     2.70 ms (under 5 ms (the least of three middles of 15))
  61:  ok   the sky's shapes turned to an eye within budget            2.89 ms (under 5 ms (the least of three middles of 15; the sheet's shapes, as where M_Cloud is built))
  203:  ok   the sky turned to an eye within budget                     2.95 ms (under 5 ms (the least of three middles of 15))
  204:  ok   the sky's shapes turned to an eye within budget            2.94 ms (under 5 ms (the least of three middles of 15; the sheet's shapes, as where M_Cloud is built))
  372:  ok   the sky turned to an eye within budget                     3.22 ms (under 5 ms (the least of three middles of 15))
  373:  ok   the sky's shapes turned to an eye within budget            3.41 ms (under 5 ms (the least of three middles of 15; the sheet's shapes, as where M_Cloud is built))
