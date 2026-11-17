# The siege of Pittsburgh in Brian's own game: the trace

Playtester, 2026-11-17, 16:05Z. Asked by the design director at 15:50Z.

**What was played.** Brian's own save `/mnt/project-files/brian-save-usa-1940-12-turn16.json` (United States from September 1939, whole world, seed 89882, Theatre command, December 1940, played by him in the 3D game on rules 83.1), loaded in the browser build of Version 84.2.2 and played forward four ways, in the cloud. Nothing here is a rebuilt game: every line is from his save.

## The short answer

One Canadian corps of guns, with no infantry, walks through the empty north-east. If nothing is done it takes Pittsburgh in January 1941 and **Washington in March 1941**. The game shouts a warning on the month card, but never says where help is. Help is one order away: the army standing round Vancouver can come back by rail.

Think of a house with every guard sent to the far fence. One burglar with a ladder walks in the front door. The alarm rings, but it does not say "your guards are at the far fence, call them back".

## The five questions

**1. How Canada came to be at war.** Brian declared it. The news of October 1939 reads "The United States declares war on Mexico" (week 1) and "The United States declares war on Canada" (week 4). By December 1940 America holds eight Canadian provinces and stands round Vancouver. Canada still holds twelve provinces and has taken Buffalo. Britain, Canada's ally, is at war with Germany and Italy only: after fourteen months of America conquering its dominion it has not gone to war with America (opinion of Britain: minus 40).

**2. Did Canada's staff send guns alone?** Yes.
- Canada's field army in December 1940: 1st Canadian Division shut in Ottawa at no strength, Atlantic Command in Nova Scotia, and XI Artillery Corps (2 artillery divisions, about 28,000 men), raised at Toronto in October 1940. The staff's rule asks for guns when its line meets dug-in troops; as this game shows, nothing then stops the guns attacking with no infantry beside them.
- The guns went alone: closed round Buffalo in October, stormed it in November (5,940 prisoners), closed round Pittsburgh in December, stormed it in January (6,062 more).
- The game's own words say this should not work: the arsenal calls an artillery corps "Slow, weak if caught alone", and the rules refuse it a probe ("Only a fighting corps probes"). Yet it may attack, storm a walled city and hold a province with no other troops.
- It was a smart move on the map (the north-east was empty) and an unreal one in kind (guns do not storm cities without infantry). Under ruling 56 I count it a worst call.

**3. Can the siege take the town?** Yes, and at once. Pittsburgh has walls +62%, no garrison (0 of 2) and a town guard worth 0.4 of a division. The guns storm it at the end of the first month of siege. The siege panel says "Its stores will feed the defenders one more month", which reads as if there is time. There is not.

**4. What the game tells him.**
- December 1940, the month of his save: the to-do tray has three rows (money to spend, research teams idle, the army round Vancouver). **Nothing says Buffalo has fallen or that an enemy corps stands two marches from the capital.** This was the month the cure was cheap.
- The month card that opens January: a WARNING at the top, "The enemy is within reach of Washington. Hold Washington, and win back home ground. Canadian troops stand in Pittsburgh, beside the capital ... The United States gives in when the capital, the home provinces and the army lost together pass what the country will bear, and your campaign ends with it: it is 2% of the way there." Good and loud. It does not say what to do.
- The same card, under YOUR FRONTS: "Pittsburgh LOST against Canadian troops close round Pittsburgh." The town is besieged, not lost, and the sentence is broken. Also "Guadalajara LOST against Lost without a battle."
- January's tray: "Pittsburgh is under siege", third of five rows, in the same dress as "2 research teams have no work".
- Pittsburgh's page: "To raise the siege, attack the army round Pittsburgh from a neighbouring province." No word on who besieges (the hover says 1 corps, 2 divisions), how long the town can hold against a storm, or where the nearest army is.

**5. What a new player can do.**
- **In December 1940 (his save as it stands): one order cures it.** Pick up the army round Vancouver (5 divisions, about 46,000 men) and right-click Pittsburgh. The game answers "3 corps are on the march to Pittsburgh: about a month more". They go by rail, stand at Cleveland in January, and the attack card on the guns reads "VICTORY IS ALL BUT CERTAIN", odds 2.49 to 1. Fought, it is "Relief of Pittsburgh, DECISIVE VICTORY". Cost: the siege of Vancouver is raised.
- **In January 1941 (a month on, as the 3D builder found it): Pittsburgh cannot be saved, Washington can.** No corps is in reach, a new corps takes 5 months (52,126 lies unspent in the treasury), and the town falls at the month's end. But the same order to Washington ("about 2 months more") brings the army to the Ohio Valley by February and the guns fall back to Toronto, so Washington is never besieged. Pittsburgh stays in Canadian hands that month: the march has used the army's moves ("Pittsburgh is out of reach for the army you have picked up this month"), so the attack waits for March.
- **With nothing done:** Pittsburgh falls in January, Washington is besieged in February and **falls in March 1941**, Norfolk falls in April, Philadelphia is besieged in May. All to the one corps of guns.

## Other things seen on the way

1. **Corrected 16:50Z (I misread this; the retreat is sound).** I wrote that the beaten guns retreat deeper into America. Played again and stopped straight after the battle (report 73, January 1941 week 4), the guns stand at **Buffalo**, which Canada holds, with no move left. At the month's end they march forward again, alone, from Buffalo to Cleveland, which the relieving army had just left, and lay siege to it (the news line is dated January week 2, though it comes after the relief of week 4). That is fix 1's fault (guns going forward alone), not the retreat's.
2. The battle card for guns standing alone says "Massed guns stand in the defence: the other defenders fight 9% stronger under their own guns." There are no other defenders.
3. "American troops tear up the railway at Buffalo as they fall back" and the same at Washington: only the town guard was there, and it is America's own railway at its own capital.
4. "the capital of United States is under siege": it wants "the United States".
5. America's air staff bombs "the factories at Pittsburgh" once Canada holds it: its own city.
6. In the browser on 84.2.2 the attack on the army outside the city opens and fights with one move left. Brian's report of 14:58Z (the 3D game refused it) is for Version 84.2.3's six cases, which I press when it ships.

## Critical fixes (no new ideas)

1. **Guns do not attack alone.** An artillery corps with no other fighting corps beside it may not open an attack or storm a town, for the computer and the player alike. The rules already say it cannot probe.
2. **The warning comes a month sooner and names the cure.** When an enemy army stands on home ground with no army of yours within reach, the tray says so that month, in red, with who it is, what it can take next and the nearest army that can come ("the army round Vancouver, by rail, a month"). This is the red row the design director has ruled, with one more month of warning.
3. **The siege panel counts the storm.** "Stores for one more month" is untrue when the besieger can storm the guard this month.
4. ~~A beaten army falls back toward its own ground.~~ **Withdrawn 16:50Z:** it already does (see the corrected line above). Four fixes stand.
5. **The month card's fronts row** says "besieged" for a town besieged, and never "LOST against" with nothing after it.

## Files

- Pictures: `/mnt/project-files/playtest/feel/pictures/brian-december-1940-buffalo.png`, `brian-january-1941-pittsburgh-besieged.png`, `brian-january-1941-relief-card.png`, `brian-march-1941-washington-besieged.png`.
- Step lists: `/mnt/project-files/playtest/harness/feel/brian/` (nothing done; the order in December; the order in January; the relief fought).
- The retreat checked: `brian-retreat.steps.txt` with the saves `brian-relief-straight-after-the-battle.save.txt` (guns at Buffalo) and `brian-relief-after-the-months-end.save.txt` (guns round Cleveland), in the same folder.
- The month card's words: `/mnt/project-files/playtest/harness/feel/brian/brian-monthcard-dec.txt`.
