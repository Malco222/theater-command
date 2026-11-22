# Calibration, 22 Nov 14:44-15:00Z (cloud)

Head ca889b49 (ground, claude/project-thread-uhhxrn), which the first pre-check cleared at 14:23Z. Each row run alone, 3 times.

| Row | Pre-check said (14:23Z) | Runner got (3 runs) | Match |
|---|---|---|---|
| land_play (g++), Normandy | PASS; walk pan 18 of 23 hold, first at 30 s, block 8, 56/55/56 m off | PASS x3; 18 of 23, first at 30 s, block 8, 56/55/56 m off | yes |
| land_play (g++), Prokhorovka | PASS; walk pan 22 of 23, first at 40 s, block 14, 70 m off | PASS x3; 22 of 23, first at 40 s, block 14, 70/70/70 m off | yes |
| land_play's two 8 ms clock gates | tripped at 9.0 and 8.2 ms beside a compile; 2.7 and 2.5 ms alone at high priority | general's view slowest 2.23 [1.68-3.42] and 2.62 [1.88-4.45] ms; ground cut 1.64 [1.57-2.17] and 0.65 [0.61-0.77] ms; VM steal 0.1% | yes (alone, normal priority, no flake) |
| land_field (g++) | all checks pass both battles; haycock 2,172 near / 110 far; fern 540 | 127 ok lines, 0 FAIL, each run; haycock 2172/110; fern 540; clutter 6,812 and 14,635 (as the ground thread's own count) | yes |
| land_field (clang++ 18) | all checks pass | cut by a container restart at 14:56Z; rerun started 17:28Z, its file lands here as ca889b49-land_field-clang-calib.txt | pending |

Every time above is a cloud time (x86 VM, 4 vCPU), not a Mac number.
