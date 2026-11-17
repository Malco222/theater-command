# The feel pass: lists and measures

For Brian's ask of 2026-11-17: "how do we make the game run more smooth, its not very easy and enjoyable to play.
i want the game tester to really go through every details".

## The two lists (both games play the same steps)

- `first-hour-america.steps.txt`: America at Supreme command (the seat Brian plays, with the fleet and the air
  groups in his own hands). The way in, the map by hand, the three to-dos, a march, a rail move, the arsenal, the
  fleet, an air group moved, the other powers, two months at peace, war with Mexico by marching in, one raid, one
  battle, four months at war.
- `first-hour-germany.steps.txt`: Germany on the door's own defaults (Theatre command). Three battles in the first
  month, the to-dos, the staff and the powers pages, four months at war.

Format: the click-through's step file (`/mnt/project-files/playtest/click-through/README.md`), tab-separated
`VERB<tab>TARGET<tab>EXPECT`, plus one verb: `pan-zoom <place> near|middle|far` (the map dragged and wheeled for a
few seconds at that height). Expectations are blank: these runs measure, they do not pass or fail.
Play them with nothing switched off, as a new player gets the game.

The words are the browser game's on Version 84.2. In the 3D game press the button that does the same job and note
the word where it differs.

## What each step should give back

Presses used; the time from the press to the first picture and to settled; the lowest and middle frame rate; how
many words the card or panel opened on; whether the thing to press next was below the screen's edge; a picture.
For `end-month`: the seconds until the month has turned, and every screen that came up before orders could be
given again, each with its seconds.

## The browser's runner

`/mnt/project-files/playtest/harness/click.cjs` with `TC_NEW=1` (nothing switched off) writes, beside the usual
log, `PREFIX.feel.tsv` (one row a step: presses, click to picture in ms, work in ms, card words and height, side
page words and height, buttons below the edge) and `PREFIX.buttons.txt` (the buttons on the screen after each step).
