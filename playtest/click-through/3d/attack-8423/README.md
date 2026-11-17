# The six attack cases of 84.2.3, pressed in the 3D game

3D builder, 17 November 2026. Build: `unreal` after dbf90f7 (dylib 1534), the bundle on rules 84.2.4 (it carries 84.2.3's reach). The hook played `attack-8423.3d.steps.txt`, my copy of the playtester's list; the saves were opened from beside it, unchanged.

**Result: 53 steps, every expectation met but one (step 29), no error in the game's log.**

What I changed in the list, each marked "(3D)" in the file:

1. One `start` step first. The hook lays the table by beginning a campaign and then opens the save over it. A `load` from the bare opening screen is not supported yet.
2. `press AUTO-RESOLVE` is `press SEND THEM IN`: that is the word on the 3D attack card.
3. `press SALLY OUT`: in 3D the sortie is a province order. A right-click on the garrison's own town with the garrison in hand says the rules' sentence and opens the province's card on ORDERS; the order there reads "Sally out from Bialystok". So the list clicks the town's ground, presses ORDERS, then presses "Sally out".
4. A left click on ground in reach takes two clicks in 3D with an army in hand (the first offers, the second sends), so the airfield mark is clicked twice (steps 10 and 11).
5. After `end-month` the month's report card lies over the table; `key esc` puts it away (step 48).

How to read the log:

- `said` is new: true when the game put a sentence on the paper line during the step, even the sentence that already stood there.
- `says-why` passes when the game said something and the game itself is as it was. In 3D a left click on a place also looks at it, so the province's card opens beside the words and the verdict reads DID (steps 21, 26, 28, 38, 43). The words are in `words`.
- Step 29 (a second click on the garrison of Mlawa, `card` expected): nothing opens in 3D. There is no sortie card yet; the sortie is ordered from ORDERS, with its odds in the order's own line. A sortie card on the attack card's pattern is owed.

The sentences the 3D game gave:

| Case | Press | What it said |
|---|---|---|
| Brian's own | right-click Pittsburgh from Cleveland, one move each | "The attack on Pittsburgh", then a decisive victory; Pittsburgh held by the USA |
| 3 | the airfield sign, twice | "Click Pittsburgh again, or press the button above, to draw up the attack.", then the attack card |
| 1 | right-click Guadalajara with one move left | the attack card |
| 4 | right-click and click, no move left | "No moves left this month. An attack is planned in the month it is made." |
| 5 | right-click and click, all inside Tampico | "Third Army (2nd Division and 1st Cavalry Division) is the garrison of Tampico. Order it to take the field first." |
| 2 | pick up the garrison of Mlawa | "Army Modlin is the garrison of Mlawa, shut in by the army round it. Order a sortie against that army." |
| 2 | right-click Bialystok, then Sally out | the sortie is fought ("the garrison's sortie from Bialystok is driven back in"); a second right-click: "The garrison of Bialystok has made its sortie this month." |
| 6 | right-click and click on the eve | "On the eve of war every army stays where it is placed: no attack goes in until the war has opened." |
| His save | Vancouver to Pittsburgh, a month, Cleveland to Pittsburgh | the kept march sets off; next month the attack card and a decisive victory; Pittsburgh held by the USA |

Pictures are `step-NNN.jpg`, one a step, 1600 by 1000.
