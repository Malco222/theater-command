# Version 84.2.3: the six attack cases, pressed in the browser

Playtester, 2026-11-17, 16:30Z. Asked by the design director (15:50Z) on Brian's find of 14:58Z: "if an enemy army is out of a city i couldnt attack them". Pressed on the built page of Version 84.2.3 (main 5927c3d) and, beside it, on Version 84.2.2, with the same step lists, in the cloud, at 1,600 by 1,000. Real presses on the map throughout; two cases are staged by setting an army's moves.

**Verdict: all six answer on 84.2.3. On 84.2.2 five of them answered with nothing, or with the wrong thing.** The 3D game is still to press (its run folder has not come).

| # | The case | Where pressed | Version 84.2.2 | Version 84.2.3 |
|---|---|---|---|---|
| 1 | One move left, an enemy army next door on ground a march could not afford | Brian's save a month on: the Tampico army set to 1 move, the Mexican army in the hills of Guadalajara | The hover box has no word of an attack. The right-click does open the card | The hover box ends "Right-click to plan an attack". The right-click opens "Battle for Guadalajara" |
| 2 | His corps inside a town, the enemy round it | Poland, September 1939: Army Modlin in Mlawa, Narew Group in Bialystok | First click looks at the German army. Second click: nothing. Right-click: nothing | First click: "Mlawa is under siege with your corps inside. Right-click the army round it, or click it again, to plan a sortie." Second click and right-click both open "THE SORTIE". SALLY OUT fights it. A second try that month: "The garrison of Bialystok has made its sortie this month." |
| 3 | A mark under the pointer (an airfield's pad) | Brian's save a month on: the army at Cleveland, the Canadian guns round Pittsburgh | A click on Pittsburgh's pad opens the Air page and drops the army. A right-click on it: nothing. A right-click on Detroit's pad: nothing | The click and the right-click on Pittsburgh's pad both open "Battle for Pittsburgh". The right-click on Detroit's pad marches the army to Detroit |
| 4 | No move left, the enemy next door | The Tampico army set to 0 moves | Right-click: "No moves left this month. An attack is planned in the month it is made." A click: looks at the enemy, says nothing | The right-click and the click both give that sentence |
| 5 | All inside a town, the enemy next door | Third Army put inside Tampico, Fourth Army marched off | Right-click on Guadalajara: nothing | Right-click and click: "Your corps in Tampico are inside the town. TAKE THE FIELD, on the page of Tampico, brings a corps out to march and fight like any army." |
| 6 | The eve of war | Poland, 31 August 1939: Army Pomorze, the German army at Elbing | The right-click and the click open "Battle for Elbing" (120,000 men); by the rules builder's account its order then did nothing (I did not press it) | Both say: "On the eve of war every army stays where it is placed: no attack goes in until the war has opened." |

**Not pressed:** the city's badge in case 3. No badge stood beside an army of mine in these games (the only badges on the map were Ottawa's).

**The case Brian met, in his own game.** In his save played a month on, with the army brought back to Cleveland with one move each, the attack on the guns round Pittsburgh opened and was fought in the browser on both versions ("Relief of Pittsburgh, DECISIVE VICTORY"). The browser's right-click never read the rule that 84.2.3 mends; the 3D game did, which is why it refused him. So the proof for his own case is the 3D press.

## Seen on the way (small)

1. The hover box says "Mexican army: 1 corps, 0 divisions" for one division at 21% strength. (The rules builder has this in 84.2.4: "at 54% strength".)
2. With an army in hand, a plain click on a green province marches it there, the same as a right-click. The hint names only the right-click ("On a touch screen, tap it"). I spent a whole army's month by clicking provinces to look at them. A question for the UI director, not a fault: should the hint say so, or should a plain click only look?
3. On the eve of war the month's button reads FINISH DEPLOYMENT; my runner looked for END MONTH. Mine to mend.

## On Version 84.2.4 (main b8889c9, pressed 16:38Z to 16:55Z, in the browser, in the cloud)

The same eight lists and the click list, played again. All six cases still answer. What changed, each seen:

1. **The winner stays in hand.** After "Relief of Pittsburgh" the side page reads "NO MOVES LEFT THIS MONTH. Right-click a friendly place and this army marches there by itself, starting next month", with no second pick-up, and the right-click on Cleveland answers "3 corps will march to Cleveland from next month, about a month." On 84.2.3 the army had to be picked up again first.
2. **Case 5's sentence** is now "Third Army (2nd Division and 1st Cavalry Division) is the garrison of Tampico. Order it to take the field first. TAKE THE FIELD is on the page of Tampico.", from the right-click and from the click alike.
3. **The hover box counts by the roll:** "Canadian army: 1 corps, 2 divisions at 93% strength"; "Mexican army: 1 corps, 1 division at 21% strength" (it read "0 divisions").
4. **A flag takes its click.** By `flagclick.cjs` (a new game, the map centred on each of my flags at two zooms, widths 600 and 1,400, a real click on the middle of the flag's cloth):

| Seat | Flags | Version 84.2.3: clicks that picked the army up | Version 84.2.4 |
|---|---|---|---|
| United States | 6 | 5 of 12. The other 7 opened the Air page: every flag at the far zoom, and Canal Zone at the opening zoom | 12 of 12 |
| France | 21 | 40 of 42 (Lorraine at both zooms opened the Air page) | 42 of 42 |
| Germany | 20 | 40 of 40 | 40 of 40 |
| United Kingdom | 12 | 22 of 24 | 22 of 24 |
| Soviet Union | 21 | 42 of 42 | 42 of 42 |
| Italy | 15 | 30 of 30 | 30 of 30 |
| Japan | 20 | 40 of 40 | 40 of 40 |

Britain's two are Gibraltar's fortress garrison (a fortress, drawn with a flag): the click selects Gibraltar and picks up nothing, on both versions. A fortress does not march, so I count that as right.

**Still as on 84.2.3:** a plain click on a green province marches an army in hand (ONE MEANING TO A PRESS comes by 84.3). The city badge is still not pressed (none stands beside an army of mine).

Logs: `a8424` to `h8424` and `cm8424` in `/mnt/project-files/playtest/harness/feel/v8423/`; the flag count in `/mnt/project-files/playtest/harness/feel/results/v8424-flagclick.txt`.

## Files

Step lists `attack8423-a` to `-h` and `clickmove`, and the logs of both versions: `/mnt/project-files/playtest/harness/feel/v8423/`. The runner has three new verbs for this: `point-province`, `order-mark` (a right-click on a pad or badge) and `set-moves`.
