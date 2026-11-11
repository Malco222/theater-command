# Theater Command 1939-1945 (working title)

A grand strategy game of the Second World War in Europe: a turn-based campaign map on top, real-time battles underneath, joined by one contract called the Battle Ticket. The design framework (v9) is the source of truth for what the game is.

This repository holds **Phase 1** of the framework's roadmap: the campaign rules and auto-resolve on a simple browser map, built so the same rules and data can be carried into the Unreal Engine 5 build in Phase 2.

## What works today

**The campaign prototype** (open `dist/play.html` after a build)

- Europe from Ireland to the Urals in 224 regions, generated from real coastlines (Natural Earth, public domain).
- 27 nations with their September 1939 armies as corps on the map, reinforcements by month, and replacement pools fed by industry.
- Monthly turns starting September 1939. Each corps gets four moves a month; fast troops in a fresh offensive get more.
- Supply traced from each nation's own capital, industry and depots. Armies that are cut off fight at half strength and waste away.
- Battles settled at once from a Battle Ticket (forces, terrain, forts, supply, air, commanders, tempo, direction of attack) with the framework's victory levels. Fortifications only count against the direction they face, so the Maginot Line can be outflanked.
- Nations ask for terms when they have lost too much of their capital, territory and army. France's fall creates Vichy; eastern Poland goes to the Soviet Union under the 1939 pact; sea-supplied armies can be lifted off from a port.
- Computer opponents that follow history's plan as data (`data/campaign/plan-historical.json`). The plan sets intentions only; every region still has to be won.
- Play Germany, France, the United Kingdom or Poland, or watch history unfold.

**The exit test** from the framework ("France falls in about six weeks") runs as an automated test. With every nation on the historical plan, across 20 different seeds, Poland falls in three to four weeks and France five to six weeks after the May 1940 attack, with most of the British Expeditionary Force lifted off first.

**The flight model** for the air battle (`src/air/flight.ts`) reproduces the documented speed, climb and turn of the Spitfire, Hurricane, Bf 109, Bf 110, Do 17 and He 111 from their physical data, and is covered by tests.

## What is not here yet

- The gray-box air battle screen (the other half of Phase 1). The flight model, scenario data and pilot logic exist; the battle loop and the command screen do not.
- War at sea, strategic air war, production, research and diplomacy beyond declaring war. Norway is not invaded because there is no naval layer yet.
- Graphics. This is deliberately a staff map. The Total War and Company of Heroes level of presentation arrives with the Unreal Engine 5 build.
- Borders are approximate: regions are grown from seed points, not traced from 1939 maps.

## Run it

```
npm install
npm test            # flight model + the 1939-40 exit test
npm run build       # writes dist/play.html, a single file you can open in a browser
npm run history     # prints a full computer-vs-computer war; add "sweep 20" to compare seeds
npm run geo         # regenerates the maps from data/campaign/regions.mjs
```

## How it is laid out

| Path | What it is |
| --- | --- |
| `data/` | Moddable data: nations, regions, starting armies, the historical plan, aircraft |
| `src/core/` | Shared deterministic core: seeded random numbers, maths, the Battle Ticket contract |
| `src/campaign/game.ts` | Campaign rules: war, movement, supply, battles, capitulation |
| `src/campaign/ai.ts` | The computer's generals |
| `src/campaign/turn.ts` | The monthly turn and the player's orders |
| `src/campaign/ui/` | The campaign screen |
| `src/air/` | Flight model and air battle groundwork |
| `tests/exit/` | The framework's exit tests |
| `tools/` | Map generation and the history runner |

Everything in the simulation is deterministic: the same seed and the same orders always replay the same war. That is what will make saved games, replays and multiplayer possible, and it is why the rules can be ported to Unreal without changing their results.

## Roadmap (from the framework)

1. **Phase 1, now.** Campaign rules and auto-resolve in a browser map (done), and a gray-box air battle with 300 aircraft (in progress).
2. **Phase 2.** The Battle of Britain as a vertical slice in Unreal Engine 5: one campaign turn, one playable air battle, the full Battle Ticket loop, on licensed or commissioned art.
3. **Later phases.** Land battles, the war at sea, the full campaign systems, the other theatres.
