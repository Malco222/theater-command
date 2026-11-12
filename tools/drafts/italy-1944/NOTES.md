# Italy 1944 draft (paused 2026-11-12)

Work in progress on Anzio, Cassino, Rome and the Gothic Line. Paused to build free play first.

Done on this branch:
- Map: regions Cassino (mountain) and Anzio (plains, port) appended; road Apulia to Abruzzo. History is hash-identical up to September 1943 (T49).
- Rules: expedition rows can carry `joins` (divisions go into a named corps, which takes the new name and kind), `from` as a list of corps names (those corps are struck off as the new one forms) and `news` (a headline in place of the bare arrival notice). The main army no longer rails corps into a side theatre (`advance` in ai.ts).
- Data: first draft only. `it44.py` regenerates the Italy theatre block and the 1944 formation rows from the `*-pre44.json` copies here (edit the SP path at its top). `it44sum.ts` prints a per-seed summary.
- First run (seed 3): Anzio is landed unopposed in January 1944 and the first attack on Cassino fails at 0.36.

Still to do: move LI Mountain Corps' arrival to Cassino; German phases (counter-attack at Anzio in March, retreat to the Arno, the Gothic Line); British and American phases for February to December 1944 (second and third Cassino, Diadem in May with renewals, Rome in June, Tuscany and Marche, Olive); air plan (superiority over Cassino in April and May); Dragoon departures (`until: 60` on VI Corps and the French corps); update the 1943 exit and rule tests for the new map; new exit test and rule tests; README; STORY_ENDS.

The tests on this branch are not expected to pass yet.
