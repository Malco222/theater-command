# The save guard

Brian's own saved campaigns, loaded on every new version of the rules and played on with no orders, so that a
version which would break his game is caught before he loads it. It changes nothing in the tree.

    bash tools/save-guard/run.sh TREE OUT [months]

`TREE` is a checkout with `node_modules` (and `dist/play.html` built, for the page half); `OUT` a folder for the
logs and pictures; `months` how many months to play (default 5; the page plays 2 of them). About half a minute in
all. It prints two lines a save, each ending `errors: [...]`; an empty list is a pass, and every line to watch
follows them.

## What it checks

**The rules** (`saveguard.ts`, bundled against `TREE` by `run.sh`):

- the start screen offers the save, and it loads;
- every corps, fleet, air group, province, war and country is there as saved, and every corps stands on one list;
- ships are fleets: no "Squadron", "Escort Group" or "Submarine Flotilla" in any name or line of news;
- save, load, save again gives the same save, as loaded and after every month;
- the same month loaded twice plays the same way; a game loaded from a save made as a month opens plays that
  month as the game kept in hand does (a difference there is a line to watch, not a failure);
- each month, played as the page plays END THE MONTH (kept marches, goals, every country's month, the month's
  card): no error, no hole in the news or the card ("undefined", "NaN"), no country's books gone to NaN;
- ruling 211(a), where the tree has it (`gunsRule`): no attack, no ground taken and no siege kept up by guns or
  engineers alone;
- ruling 211(b), where the tree has it (`standsBy`): a war on a dominion that Britain has not answered yet gets
  its one roll as the first month ends, one line of news, the answer the campaign's seed gives, what Brian was
  told (`*.expect.json`), and no second roll after;
- ruling 211(c), where the tree has it (`homeAlarms`, Version 84.2.6): every red row reads whole and its press
  picks up a live army of his, each month; in the save as loaded the first one names the army `alarm.army` says
  (December 1940: the army round Vancouver);
- lines to watch: towns in `hold` changing hands, "United States" without its "the" (ruling 211(f)), a fleet of
  his gone.

**The page** (`savepage.cjs`): the start screen's "Continue your campaign" opens it; his fleets are fleets; two
presses of END THE MONTH each bring up the month's card, whole; the card's band carries Britain's answer; the
browser keeps the campaign as played on; no error from the page. Where the tree has the red row, it is first in the
tray as the save opens, and a real press on it picks up the army round Vancouver and says the cure on the paper.

## The saves

`saves/` holds every campaign it plays, each `*.json` the rules' own `saveGame()` text (what the 3D game writes to
`Saved/Campaign/campaign.json` and the browser keeps under `theater-command-save-v1`). A `*.expect.json` beside
one says what that campaign was told it would see. To guard a new save, drop it in `saves/`.

| Save | Game date | Taken |
|---|---|---|
| `brian-save-usa-1939-11-turn3.json` | November 1939 | 17 November 2026, 11:34 UTC, while he was playing (3D, Version 83.1) |
| `brian-save-usa-1940-12-turn16.json` | December 1940 | 17 November 2026, 14:33 UTC, as he closed the game (3D, Version 83.1) |

Both are the United States from September 1939, the whole world, the grand campaign, seed 89882.
