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

### 1. The map in a big war runs at a quarter of its speed, all the time

| America, March 1942, dragging the map | pictures a second |
|---|---|
| as it is, a turning ring in view | 15 to 20 (slow ones 9) |
| as it is, no ring in view | 30 |
| the same save with the rings and the flicker switched off | 60 |
| September 1939, at peace | 60 |

Sixteen marks never stop on that map: nine rings on capitals that can be freed, two siege rings, five fires. Each
one makes the browser paint the whole map again for every picture, like redrawing the whole chalkboard to move one
magnet. In a 1942 war there is always one somewhere, so the whole campaign runs like this, not only one bad month.
**Traced and with the rules builder, who has the mend built as Version 84.2.1** (the marks go on a sheet of their
own). I count it again there, a mark in view and not, both window sizes.

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
