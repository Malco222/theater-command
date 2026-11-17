# Where the hand and the eye still pay too much, after the six ruled fixes

The Playtester's feel pass, first lot, 2026-11-17. For Brian's ask: "how do we make the game run more smooth, its
not very easy and enjoyable to play. i want the game tester to really go through every details".

**What this lot rests on.** The browser game on Version 84.2, played by hand as a new player (nothing switched
off, 1,440 by 900, a cloud machine): America's first hour at Supreme command (63 steps), Germany's at the door's
defaults (51 steps), and Britain for the cabinet, the powers, a fleet, a raid and a landing (34 steps). The 3D game
read from its twenty screen pictures of 16 November and from its builder's own timings. **My own seconds in the 3D
game wait for the hook's first run**, which waits for the game on the Mac to be closed.

It starts where the UI director's count stops: the battle, the month's end, the province and home panels and the
silent presses are ruled (ruling 204) and are not repeated here.

A press is a click or a key. "Below the edge" is how far the thing to press stood under the window when the screen
opened. Pictures: `/mnt/project-files/playtest/feel/pictures/`.

## The top three

### 1. The 3D map's speed (paid every second of play)

| | today | source |
|---|---|---|
| The whole world, pictures a second | 27 lowest, 33 middle | the 3D builder's own |
| Europe only | 39 lowest, 43 middle | the 3D builder's own |
| Every order | a hitch of about 70 ms | the 3D builder's own |
| An army in hand, pointer crossing ground it can attack | hitches of 20 to 60 ms | the 3D builder's own |

"Smooth" was Brian's first word, and none of the six fixes touches this. A film runs at 24 pictures a second and a
map you drag wants twice that: at 27 the table judders under the hand. Mine to count through the hook: `pan-zoom`
far at New York, the Eastern Seaboard and the Canal Zone, and Warsaw near in September and again in December.
**Marks I would count:** dragging the whole world never under 40 a second on his Mac; no order hitches over 50 ms;
the odds under the pointer looked up once a month, not on every move.

### 2. The air war by hand: one raid is 7 presses, a full-screen scroll and two cards

Counted as Britain, the browser, Version 84.2 (`raid-1` to `raid-4`):

| step | press | what it costs |
|---|---|---|
| Air page | 1 | 801 words, four screens tall |
| the bomber group in the list | 2 | 996 px below the edge: a scroll of more than a screen |
| PLAN A RAID WITH IT | 3 | |
| the kind of raid (8 kinds) | 4 | the side page is now 1,120 words, six screens |
| the target | 5 | the raid card: 384 words, 1,316 px tall |
| FLY THE RAID BY DAY | 6 | the report card: 191 words |
| CLOSE | 7 | |

America has ten air groups from the first month (Version 84.2) and each can fly one raid a month: seventy presses
a month to fly them all by hand. In the 3D game a group cannot be picked up on the map at all (the builder's own
list). **Fix, in the pattern already ruled for battles:** group, target, Enter, on one card that opens with the
staff's best target chosen; the report as a slip on the target; an airfield's badge on the map picks up its groups.

### 3. Crossing the sea: the order is hidden

Counted as Britain, London to Hamburg, the browser, Version 84.2 (`landing-1` to `landing-3`):

- With the army in hand, a right-click on Hamburg answers "Hamburg is out of reach for the army you have picked up
  this month." Not a word that the sea is the way.
- The button, INVADE BY SEA, is 1,305 px down the province page: nearly two screens below the edge. SHIP BY SEA
  sits beside it.
- The Navy page says the force "can embark the month after". It could be ordered the same month.
- The card is 604 words and 1,778 px tall, two screens, with LAND THE TROOPS below the edge. (The 3D card is about
  270 words and fits; its tray has BY SEA in view. The 3D game is the model here.)

For America every road to the war crosses water, so this is the order Brian's seat needs most.
**Fix:** a right-click on an enemy coast across the water opens the landing card, as marching in opens the attack
card; a right-click on a friendly port across the water ships the army. Where neither can be done the one line
says why and names the sea.

## The rest, in order

**4. To-dos that cannot be done.** America, month one: the arsenal lists seven corps and every one is off (the
cheapest costs 3,400; the treasury holds 1,012 and gains 1,060 a month), so the first order is three months away
(`america-arsenal-month-one-all-off`). Germany at Theatre command is told "3,833 in the treasury to spend" and the
infantry corps is 967 short. Britain: seating one minister costs 6 political capital of 9, and every COURT button
then goes off for the month, with the reason on hover only. **Fix:** a to-do shows only when it can be done this
month, and says what it buys.

**5. The cabinet: one seat is 4 presses over 483 words** (the to-do, MINISTERS, the man, YES APPOINT HIM), on a
card 1,166 px tall with four buttons below the edge (`cabinet-after-one-seat`). The reasons are all there and well
said; it is the reading that costs. **Fix:** the to-do opens straight on the ministers; the man's own row carries
APPOINT.

**6. The powers page (browser): 576 words, five screens, 43 COURT buttons that read alike** (`powers-page`). The 3D
foreign office is already the model: one card, one row a country, COURT and WAR on the row.

**7. The Guide (3D): one long typed page in a scroll box.** Ruling 204 moves the rule book into it (HOW A MONTH WORKS: 3,260 words in 36 points). It
wants a contents list of one-line topics first, or the manual only moves from the seat pocket to the glove box.

**8. The browser's own speed.** (a) A press takes about a quarter of a second to answer: the middle of 66 map and
page presses was 244 to 272 ms, and picking up an army 344 to 400 ms, because every press redraws the whole side
page. Under 100 ms reads as instant. (A cloud machine; his Mac will be quicker, by my guess two or three times.)
(b) Moving the map near Warsaw ran at 60 pictures a second in September 1939 and 9 in December, four months into
the war. **Traced (13:05Z):** the province flash. Provinces that changed hands at the month's end stay in `ui.flash`
for the month, and every redraw of the map writes their flash again and restarts its 2.2 s fade (main.ts line 1329,
style.css line 83). Loaded fresh, the same game moves at 60; with two provinces set to flash, 10; cleared, 60 again
(`/mnt/project-files/playtest/harness/feel/flashtest.cjs`). Ruled a critical fix (ruling 206).

**9. A fleet's orders: 3 presses, no card, one line each.** The model for the rest. Nothing to mend.

## What is still to count

The 3D game's month's end, screen by screen in seconds; its frame rate on the whole world; a raid, a landing and a
sea move in 3D; shipping an American army to Britain. All through the hook, on the two lists in this folder.
