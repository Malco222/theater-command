# Theater Command 1939-1945 (working title)

A grand strategy game of the Second World War in Europe: a turn-based campaign map on top, real-time battles underneath, joined by one contract called the Battle Ticket. The design framework (v9) is the source of truth for what the game is.

This repository holds **Phase 1** of the framework's roadmap: the campaign rules and auto-resolve on a simple browser map, built so the same rules and data can be carried into the Unreal Engine 5 build in Phase 2.

## What works today

**The campaign prototype** (open `dist/play.html` after a build)

- Europe from Ireland to the Volga and from Norway to the Caucasus in 315 regions and 20 seas, generated from real coastlines (Natural Earth, public domain). Moscow, Stalingrad, the Crimea, Turkey and the northern edge of the Middle East are on the map.
- 29 nations with their September 1939 armies as corps on the map, reinforcements by month, and replacement pools fed by industry.
- Monthly turns starting September 1939. Each corps gets four moves a month; fast troops in a fresh offensive get more.
- Supply traced from each nation's own capital, industry and depots. Armies that are cut off fight at half strength and waste away.
- Battles settled at once from a Battle Ticket (forces, terrain, forts, supply, air, commanders, tempo, direction of attack) with the framework's victory levels. Fortifications only count against the direction they face, so the Maginot Line can be outflanked.
- Nations ask for terms when they have lost too much of their capital, territory and army. France's fall creates Vichy; eastern Poland goes to the Soviet Union under the 1939 pact; sea-supplied armies can be lifted off from a port.
- Computer opponents that follow history's plan as data (`data/campaign/plan-historical.json`). The plan sets intentions only; every region still has to be won.
- A war economy. Factories earn production points every month (half speed at peace, half value on occupied ground). Losses are made good first; what is left buys new infantry, mountain, motorized and armoured corps and fortifications, which take months to arrive. Computer-run nations start ordering once their scripted mobilisation is over. All costs are data (`data/campaign/production.json`).
- The war at sea. Eighteen fleets start the war in their 1939 bases, each a count of battleships, carriers, cruisers, destroyers and submarines under a named admiral. A fleet patrols a sea to hold it, raids shipping, or lies in port to refit. Fleets that meet fight a fleet action settled from its own Battle Ticket, and aircraft flying from the coast count toward holding a sea, so the narrow waters belong to whoever owns the airfields. The naval rules are data (`data/campaign/navy.json`, `data/campaign/seas.json`).
- The war in the air. Twenty-two air fleets, each a count of fighters and bombers at the airfields of one region under a named commander: the Luftflotten, Fighter Command's groups, Bomber Command, the French air zones and the Soviet district air forces. Left alone a fleet supports the armies and defends the sky within reach of its fighters. Given a mission it fights a month-long air campaign, settled from its own Battle Ticket: win the sky over a region, bomb its industry by day or by night, or strike at shipping in a sea. The air rules are data (`data/campaign/air.json`).
- Range, radar and airfields. Fighters are at full effect to 60% of their range and useless beyond it, so bombers sent further go alone and suffer. Radar gets every defending fighter up in time; a defender without it meets raids with 70% of his fighters. The airfields of one region serve about 1,500 aircraft, and fleets packed in beyond that cannot all fly. Moving base costs readiness, and so does every month of operations.
- Aircraft and pilots. Factories and flying schools deliver on history's schedule into a reserve that tops the fleets up each month, resting fleets first. A new fighter needs a pilot. A pilot shot down over his own ground usually flies again; one shot down over the enemy's does not.
- The sky. An attacker wins the sky over a region when the fighters he has left at the end of the month outweigh, by 1.65 to 1, what the defender can still put up once his losses are made good. It lasts a month. An army attacking there has the air on its side, and no invasion fleet can cross to a coast while enemy fighters still hold the sky over it.
- Bombing cuts the victim's next month of production. Strikes on shipping sink warships at sea and coastal trade.
- The Battle of Britain. On history's plan the Luftwaffe fights over the Channel in July 1940, goes for the airfields of Kent and Hampshire in August, turns on London in September and goes over to the night Blitz in the autumn.
- Sea transport that can be intercepted. Troopships need sea lift and a route through seas the enemy has not closed. A crossing through contested water costs men, and an army overseas is supplied only while a port and an open sea link it to home.
- Landings from the sea. Corps in a port can be put ashore on an enemy coast up to three seas away. Troops storming a defended beach fight at 60% strength, unless they land in the very month war is declared, when surprise makes up the difference. Enemy fleets in harbour nearby put to sea against an invasion, and a landing cannot be attempted at all under a sky the enemy's fighters still hold. That is what makes Operation Sea Lion wait on the air battle.
- Norway, April 1940. Germany declares war on Denmark and Norway and lands at Oslo, Bergen, Trondheim and Narvik under cover of its Battle Fleet; the Home Fleet comes out to meet it and an Allied expedition goes ashore, then leaves when France is in danger.
- The convoy war. German submarines and raiders sink shipping on the Atlantic routes every month, and every ton lost cuts British production. Escorts, patrolling fleets and air cover cut the losses and sink U-boats. The U-boat arm grows on history's schedule.
- Swedish iron ore reaches Germany across the Baltic in summer and through Narvik when the Gulf of Bothnia freezes. In the winter months it arrives only while Narvik is in hands that are not at war with Germany, and losing it costs German industry.
- Saved games. The whole campaign, including the exact state of the random number stream, is written to the browser as you play, and the start screen offers to continue it. A loaded game plays out exactly as the unsaved one would have.
- Diplomacy on the framework's seven-step alignment track. Every neutral has a public opinion that drifts back to its own leaning and bends toward whoever is winning the war. A country that leans toward a bloc trades with it; one that is fully won over joins it. Players spend political capital to court neutrals, and moves that match public opinion go further.
- History as events (`data/campaign/diplomacy.json`): the Soviet occupation of the Baltic states and Bessarabia, the Vienna Award, the Tripartite Pact. Each fires once, in its window, and only if its conditions still hold. If France does not fall, the Danube states do not join the Axis.
- The eastern front of 1941. Germany's plan runs from the build-up in Poland through Barbarossa, Smolensk, Kiev and the drive on Moscow to the winter line; Romania marches with it, and Finland, Hungary and Slovakia declare war. The Soviet plan holds, then strikes back in December.
- Supply that thins out with distance. Inside the Soviet Union every army's supply line is measured from its railheads: the longer it is, the weaker its attacks, the thinner its defence, the slower its replacements and the more each step forward costs. Captured railways are converted over a few months. All of it is data (`data/campaign/east.json`).
- Mud and winter. Mud in November, March and April slows every army in the east. From December to February armies without winter equipment fight and march badly on Soviet ground and lose men to the cold. The Red Army and the Finns do not.
- The Red Army's mobilisation. From the month it is attacked the Soviet Union raises new rifle corps every month, and defence lines are dug around Leningrad and then Moscow.
- Limited manpower. However rich the treasury, an army gets back only about three men in a hundred of its peak strength each month, shared along the front.
- Staffs that learn. The Red Army of 1941 can bring only eight corps into one battle and does not renew a failed assault in the same month. Defenders who hold their ground lose fewer men than those who are driven off it.
- Computer generals who bring reserves up behind a spearhead, garrison what they have taken, leave a besieged city alone when the plan says to starve it out, counter-attack ground they have just lost, and close any gap in a line they are standing on.
- Generals. The real commanders of 1939 start the war at the head of their corps, and others wait for a command. A general gains experience from every battle, and each level brings a skill point for one of four skills: offense, defense, logistics and leadership. Traits are earned from how he actually fights: three victories that end in surrender make an encirclement master, three positions held against a stronger attacker make an iron defender, three costly failed assaults make a butcher. Generals can be wounded, captured or killed, and unknown officers make their names in battle and rise from the ranks. The player assigns, relieves, promotes and dismisses them and spends their skill points; the computer does the same for its own. Everything a general adds is shown on the Battle Ticket. All of it is data (`data/campaign/commanders.json`).
- Play Germany, France, the United Kingdom or Poland from September 1939, Germany or the United Kingdom from July 1940 with the Battle of Britain about to begin, or Germany or the Soviet Union from June 1941. You can also watch history unfold from any of the three dates.

**The exit tests** from the framework run as automated tests, with every nation on the historical plan across 20 different seeds.

- "France falls in about six weeks": Poland falls in about four weeks and France six to eight weeks after the May 1940 attack, with most of the British Expeditionary Force lifted off first. Norway is invaded in April and holds out into the summer.
- "A believable Barbarossa" (the Phase 3 test, run early against the campaign rules): Minsk falls in June 1941, Smolensk in July, Kiev in September and Kharkov in October. Moscow and Leningrad hold. In December the Red Army strikes back and wins ground, and the German army is hurt but not broken. Through March 1942 Germany loses roughly 0.5 to 1.0 million men and the Soviet Union 2.6 to 3.5 million.

- "Played historically, the Luftwaffe fails to win air superiority by mid-September" (the Phase 2 test, run early against the campaign rules): on history's plan the sky over Kent, Hampshire and London is never won, and it is never close. From July to November 1940 Fighter Command loses about 1,000 to 1,200 fighters and 520 to 620 pilots and ends as strong as it began; the Luftwaffe loses about 1,800 to 2,100 aircraft. History's figures to the end of October were 1,023 fighters, 544 pilots and 1,887 aircraft. Britain is not invaded.
- The what-if is tested as well. A player who brings all five air fleets within reach of Kent, spreads them over the airfields of Calais, Flanders, Lille, the Somme and Rouen, rests them for a month and then goes for No. 11 Group's airfields month after month wins the sky over Kent in September or October. The two western air fleets alone never do, and neither does everything crammed into Calais. Once the sky is won the troopships can cross, and the Royal Navy still sinks part of the force on the way.

Where 1941 still differs from history: Odessa falls a month or two early, Rostov is never reached, Soviet losses are lower than the real ones, and Finland, Hungary and Slovakia declare war but only defend.

**The flight model** for the air battle (`src/air/flight.ts`) reproduces the documented speed, climb and turn of the Spitfire, Hurricane, Bf 109, Bf 110, Do 17 and He 111 from their physical data, and is covered by tests.

## What is not here yet

- The gray-box air battle screen (the other half of Phase 1). The flight model, scenario data and pilot logic exist; the battle loop and the command screen do not.
- The rest of the war in the air. The computer flies air offensives only where history's plan has one, which today means Germany against Britain in 1940 and 1941: it does not fly a British night offensive against Germany, does not react to a player's air offensive by moving its fighter groups, and never attempts Sea Lion itself. Support for the armies still comes from each nation's air rating, not from where its air fleets stand. There are no aircraft types, no carrier aircraft and no air transport.
- Research.
- The rest of the war at sea: building warships to order (today new ships arrive on history's schedule), carrier strikes and port raids, mines, and the Mediterranean and North African campaigns the Italian and British fleets are there to fight over. Sea battles are settled at once from fleet strength; there is no tactical naval battle.
- The rest of diplomacy: changing your own nation's side, treaties, espionage, and computer-run nations that court neutrals themselves. Today the computer follows history's events and the drift of the war.
- The Balkans campaign of April 1941, Italy's part in the east, and everything after the spring of 1942. History's plans stop there; the game goes on, but computer-run nations only hold their lines.
- The rest of the commander system: skills and traits for admirals and air commanders (today they gain levels from their battles, and an admiral can be lost with his flagship), command limits, rivalries and politics between generals, and staff college. Generals lead single corps today, because the prototype has no armies above the corps yet.
- Graphics. This is deliberately a staff map. The Total War and Company of Heroes level of presentation arrives with the Unreal Engine 5 build.
- Borders are approximate: regions are grown from seed points, not traced from 1939 maps.

## Run it

```
npm install
npm test            # flight model, war economy, diplomacy, eastern front rules, generals, the war at sea, the war in the air, saved games and the three exit tests
npm run build       # writes dist/play.html, a single file you can open in a browser
npm run history     # prints a full computer-vs-computer war; add "sweep 20" to compare seeds
npm run geo         # regenerates the maps from data/campaign/regions.mjs
```

## How it is laid out

| Path | What it is |
| --- | --- |
| `data/` | Moddable data: nations, regions, seas, starting armies, fleets and air fleets, the historical plans, the eastern front rules, generals, aircraft |
| `src/core/` | Shared deterministic core: seeded random numbers, maths, the Battle Ticket contract |
| `src/campaign/game.ts` | Campaign rules: war, movement, supply, weather, battles, mobilisation, capitulation |
| `src/campaign/economy.ts` | Production points, replacements, new corps and fortifications |
| `src/campaign/diplomacy.ts` | The alignment track, courting, and history's events |
| `src/campaign/navy.ts` | The war at sea: fleets, sea control, fleet actions, convoys, troopships and landings |
| `src/campaign/air.ts` | The war in the air: air fleets, range, radar, missions, the sky, bombing and strikes |
| `src/campaign/commanders.ts` | Generals: experience, skills, traits, wounds and capture, promotion |
| `src/campaign/save.ts` | Saved games |
| `src/campaign/ai.ts` | The computer's generals |
| `src/campaign/turn.ts` | The monthly turn and the player's orders |
| `src/campaign/ui/` | The campaign screen |
| `src/air/` | Flight model and air battle groundwork |
| `tests/` | Rule tests: flight model, war economy, the war at sea, the war in the air, saved games, diplomacy, the eastern front, generals |
| `tests/exit/` | The framework's exit tests |
| `tools/` | Map generation and the history runner |

Everything in the simulation is deterministic: the same seed and the same orders always replay the same war. That is what will make saved games, replays and multiplayer possible, and it is why the rules can be ported to Unreal without changing their results.

## Roadmap (from the framework)

1. **Phase 1, now.** Campaign rules and auto-resolve in a browser map (done, and now deepening with production, saved games, diplomacy, the eastern front of 1941, generals, the war at sea and the war in the air), and a gray-box air battle with 300 aircraft (paused).
2. **Phase 2.** The Battle of Britain as a vertical slice in Unreal Engine 5: one campaign turn, one playable air battle, the full Battle Ticket loop, on licensed or commissioned art. Its campaign half (radar, airfields, the aircraft and pilot pipeline, Dowding's groups under Park and Leigh-Mallory, the Channel battles and Sea Lion as a what-if) is already playable on the browser map, and its exit test passes there.
3. **Later phases.** Land battles, the war at sea, the full campaign systems, the other theatres.
