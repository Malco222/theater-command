# Click everything: one step list, played on both games

Playtester, 2026-11-17, on Brian's ask: "i want the playtester to really click everything to make sure it works in the 3d game too".

## The idea

One list of steps is played on the browser game and on the 3D game. Each game writes the same kind of log. The two
logs are laid side by side. A step that works in the browser and fails in 3D is a 3D fault. A step that fails in
both is a gap in the rules. A step that does nothing and says nothing is a dead click, in either game.

The rule every step is held to: **a click does something, or the game says why not.**

## Who runs what

- Browser game: the Playtester, in the cloud, any time (a Playwright runner reads the step file).
- 3D game: the 3D builder's hook plays the same file in a test window on Brian's own Mac, in slots when Brian is
  not playing. The Playtester writes the step files and reads the logs and pictures. Nobody touches his company Mac.

## A step file

Plain text, one step a line, three columns split by tabs: VERB, TARGET, EXPECT (may be empty). `#` starts a note.

| Verb | Target | What it does |
| --- | --- | --- |
| `start` | `USA 1939-09 seed 25 colonel` | a new grand campaign |
| `load` | a save's name | a saved game (to be agreed: can the 3D game open a browser save?) |
| `snapshot` / `restore` | a name | keep the game as it stands, and put it back (so a sweep can try every button from the same place) |
| `select-province` | province name | left click: takes what stands there in hand and opens its card |
| `select-unit` | army, corps, fleet or air group name | picks that one, on the map or in the tray |
| `order-province` | province name | right click with what is in hand |
| `select-sea` | sea name | left click on open water of that sea (the browser sends a fleet in hand to a ringed sea this way) |
| `order-sea` | sea name | right click a sea with what is in hand |
| `check-province` | province name | no click: reads the digest, for an EXPECT such as `holder USA` |
| `point-at` | a map mark (`gate NAME`, `airfield PROVINCE`) | rests the pointer on it |
| `click-mark` | the same | clicks the mark |
| `open-page` | page name | Build, Research, Air, Navy, Powers, Spies, Staff, Cabinet, Ledger, News, Battles, Generals, Honours |
| `press` | a button's words | presses it on the open card, page or tray |
| `press-each` | `card`, `page` or `tray` | presses every button there, one at a time, with `restore` between |
| `key` | a key's name | presses that key (R opens the month's report again) |
| `close` | | closes the open card |
| `end-month` | | the game's own End month button, and any question it asks |

EXPECT is one of: `does` (the game changes, or a card opens), `says-why` (nothing changes and words say why),
`says WORDS` (the words shown include WORDS), `card TITLE`, `holder NAT`, `moves`, or empty (record only).

## What each game writes back, for every step

1. the step's number and words;
2. what was in hand;
3. the card or page that is open after it (its title);
4. the words the player is shown (the line under the map, a toast, a card's first lines);
5. a short digest of the game before and after: the month, the holder of the target province, where the unit in
   hand stands and its moves left, the treasury, the list of wars, counts of armies, fleets and air groups, and
   what is on order;
6. a verdict: DID, SAID WHY, DEAD (nothing changed and nothing was said) or ERROR;
7. a picture.

Item 5 has to mean the same thing in both games, or the logs cannot be compared.

## Run order

1. America, September 1939, at peace (`america-run-1.steps.txt`): Brian's five finds first, then every unit, every
   page and every button, then three months ended.
2. America at war from the frontier (`america-run-2.steps.txt`): a village with no guard, a town held only by its
   guard, a city held only by its guard, a town with a corps inside. Then, from a 1942 start or a save: boarding
   ships and landing on a hostile shore; raids; the canal with an enemy at sea.
3. The newest seats next: Mexico, Brazil, Argentina, then Japan when the Pacific lands.
4. Then Britain, Germany, the Soviet Union, Italy, France.
5. After that, every seat again after each version, as the wars already are.

## What it will not catch

A click test proves the controls work. It does not say whether the game is fun or fair: the full wars and the
hands-on reports keep doing that.
