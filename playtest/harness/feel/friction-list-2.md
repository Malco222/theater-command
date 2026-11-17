# America's war by hand, from a 1942 start: where it still drags

The Playtester's feel pass, second lot, 2026-11-17. For Brian's ask: "how do we make the game run more smooth, its
not very easy and enjoyable to play. i want the game tester to really go through every details".

**What this lot rests on.** The browser game on Version 84.2, as a new player (nothing switched off), at 1,600 by
1,000 on a cloud machine. America from the door's 1942 start at Supreme command, at war with Germany and Italy:
an army shipped from New York to Wales, the bombers moved across, a minister, a general, a spy, a raid planned
from Wales, a landing tried on Cotentin, three months ended (45 steps), and every page opened once. The first lot
played 1939, when America is at peace; this is the war Brian's seat actually fights. **The 3D game is still to
count**: the hook's first run waits for the game on the Mac to be closed.

Pictures: `/mnt/project-files/playtest/feel/pictures/america-1942-*.png`. Tables and saves:
`/mnt/project-files/playtest/harness/feel/`.

## The top three

### 1. Turning marks made the map stutter even when nothing moved (mended in Version 84.2.1)

**Corrected 14:45Z.** This item first read "the map in a big war runs at a quarter of its speed", 15 pictures a
second dragged against 60. That figure was my yardstick's fault: the "middle picture" rate flips between 60 and
15 when the share of slow pictures crosses one half. Counted again by steadier measures:

| with turning marks in view, nothing moving | Version 84.2 | Version 84.2.1 |
|---|---|---|
| America, March 1942 (8 marks in view) | 30 a second | 60 |
| the pointer crossing that view | 20 | 60 |
| Germany, December 1939 (the directive ring in view) | 8 | 30 |
| the pointer crossing that view | 7 | 20 |

| dragging over Paris, pictures made over the time of the drag | near | middle |
|---|---|---|
| Version 84.2, America March 1942 | 14 | 17 |
| Version 84.2.1, the same | 18 | 22 |
| Version 84.2.1, every mark stopped | 19 | 24 |
| September 1939, at peace (both versions) | 22 to 23 | 28 to 30 |

Sixteen marks never stop on the 1942 map: nine rings on capitals that can be freed, two siege rings, five fires.
In view, each made the browser paint the whole map again for every picture, like redrawing the whole chalkboard
because one magnet is spinning. Version 84.2.1 moves them to a sheet of their own: the stutter standing still is
gone for America and much less for Germany, and the drag gains a quarter. War is still 18% (near) to 27% (middle)
slower than peace in the drag.

**What this machine cannot tell.** Moving only the map's view box, with no script at all, makes 9 to 15 pictures a
second here at peace and at war, and sliding the drawn map by a transform 9 to 19: a cloud machine has no graphics
card. Whether the drag is smooth on a real computer has to be counted on one. Tables:
`/mnt/project-files/playtest/harness/feel/results/` (`drag-*`, `mapspeed-*`).

### 2. The month's end: three seconds of the map moving with nothing to read

| a month's end, America 1942 | to the month card | of which the page is busy |
|---|---|---|
| a new player (the watch on, as shipped) | 2.7 to 2.9 s | 2.3 s in 12 to 15 jolts |
| the same with the watch switched off | 0.8 to 1.0 s | 0.7 to 0.8 s |
| 1939, for comparison | 0.3 s | |

Two presses, but 4.5 to 4.9 s before orders can be given again. About two seconds of every month is the camera
following other nations' moves that cannot be read at that speed. Ruling 204 already rules "nothing before the
month card" for Version 84.3: this is its before count for a world at war.

### 3. The road to France says no and does not say why

Pictures: `america-1942-landing-refused.png`, `america-1942-channel-closed.png`.

- New York to Wales works: the army, SHIP BY SEA, the port, 3 presses, and it is there the same month. But SHIP BY
  SEA stands 1,098 px below the edge, and a right-click on Wales says only "out of reach" (ruling 206 covers both).
- From Wales, INVADE BY SEA on Cotentin, 352 km away: "No sea lane to that coast is open: it is too far, or the
  enemy holds the sea between." I marched the army to the West Country, on the Channel itself, and tried again a
  month later: the same sentence. It does not say which of the two, nor which sea, nor who holds it.
- The game knows. Click the English Channel and its own panel says "Closed to your troopships and supply ships:
  the enemy is much the stronger here", yours 14, the enemy's 38 (aircraft from the coast count with the ships),
  with Dover Command sitting in harbour beside it. Nothing in the refusal points there, and the map draws nothing
  on a closed sea: the Channel in the picture is empty water.
- The Navy page's list of crossings offers France only "from Gibraltar to Brittany" and Germany "from West Country
  to Hamburg by way of the Irish Sea, then the North Atlantic, then the North Sea". The short crossing every
  player will try first is not on it, and no line says why.

- The other road does open, and speaks plainly: shipped on to Gibraltar (3 presses), the same army's landing card
  for Brittany heads itself "THIS LANDING WILL FAIL · odds 0.07 to 1". Sent anyway, it lost 5,190 men, 38% of
  those sent. That head is the plain speech the refusal lacks. The card is 537 words and 1,739 px, with LAND THE
  TROOPS 740 px below its edge (ruling 206 trims it).

It is like a ferry desk that says "no sailing today" when the board behind it already reads "Channel closed, storm".
**Fix (words only):** the refusal carries what the sea's panel already knows: "The English Channel is closed to
your troopships: the enemy's strength there is 38 to your 14. Put a fleet to sea there or win its sky." Where the
coast is truly too far, say that instead.

## The rest, in order

**4. Pages that grew with the war** (the pane is 826 px tall):

| page on opening, 1942 | words | panes |
|---|---|---|
| News | 3,303 in January, 5,041 by April | 12 and growing |
| Air | 1,895 to 2,225 | 9 to 10.7 |
| Battles (every battle in the world) | 1,221 | 8.4 |
| Home | 3,572 to 3,594 | 7.8 |
| Navy | 1,294 to 1,341 | 5.3 |
| Powers | 916 | 4.8 |
| Research, Staff, Honours, Generals, Build | 239 to 516 | 1.2 to 2.4 |
| Spies | 108 | 1 (it fits) |

Version 84.4's folds are ruled for the province and home panels; the Air and News pages are now the longest.

**5. Presses answer more slowly in a big war.** The middle of 35 presses was 296 ms (200 to 248 in 1939); picking
up an army 368 to 504 ms; opening a page 304 to 400 ms; BEGIN THE CAMPAIGN 1.8 to 1.9 s (0.7 in 1939). The design
director has this counted again on Version 84.4.

**6. A raid's targets come nearest first, not best first.** From Wales, BOMB FACTORIES AND RAILWAYS lists 56
targets by distance (Cotentin 352 km, Normandy 418, Brittany 423), each "a railway". Ruling 206's raid card opens
with the staff's best target chosen, which answers this.

**7. A card that arrives by itself.** At a 1942 start the opening dispatch (118 words) comes up about three
seconds after BEGIN, over whatever the player had started to press.

**8. APPOINT A GENERAL stands 1,098 px below the edge** on the army's page, beside the sea orders. The Generals
page opens on "No corps has a general yet" and then lists the war's other commanders, German ones first.

## What works and should be the model

Ordering a corps from the treasury to-do is 2 presses and says "Ordered". Recruiting an agent is 1 press and says
when he is ready. Shipping the bombers across names the field and the limit. All 45 steps answered; none was dead
or silent.
