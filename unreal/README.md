# Theater Command in Unreal Engine 5

This folder is the Unreal Engine build of the game. The browser prototype in the rest of the repository worked out the rules; this is where they get their table, their miniatures and, later, their battlefields.

## Play it

Double-click **Play Theater Command.command** in this folder. A Terminal window opens for a few seconds, then the game opens in its own window. The first start takes a minute or two while Unreal prepares its materials; after that it is quick.

| To do this | Do this |
| --- | --- |
| Move across the map | Drag with the mouse or trackpad, or use W A S D or the arrow keys |
| Zoom | Scroll (two fingers on a trackpad), or the + and - keys |
| Turn the view | Hold the right button and drag, or use Q and E |
| Pick up an army | Click its flag (or the ground it stands on). The piece lifts off the table and the ground it can reach this month lightens |
| Send it somewhere | Click lightened ground once to choose it, then click it again (or press the March To button in the tray) to send the army. Or hold the mouse button on the flag, carry it to where it should go, and let go. Right-click sends it at once |
| Put it down again | Press Esc, click its flag or its own ground again, press Put Down in the tray, or click ground it cannot reach. A single click never moves anybody |
| Attack | Click red ground twice, or drop the flag on it. The two armies are drawn up on a face-off screen with the odds; send them in or call it off |
| Go to war with a country at peace | Drag an army onto its ground, or click one of its territories and press Declare War. The game asks before any war begins; say yes and the army goes on if it can reach |
| Send it by rail | Click By Rail in the tray, then click ground tinted blue twice or drop the flag on it. It takes the corps' whole month |
| Tell it what to do if attacked | Stand, Dig In or Fall Back in the tray |
| Leave a corps behind | Click its card in the tray at the bottom |
| Raise a new corps or fortify | Click one of your own territories; the panel lists what can be ordered there |
| Put the army down | Esc |
| End the month | Enter, or the button at the bottom right. The other nations' moves are played out quickly; Space skips |
| Music on or off | M |
| A corps' story | The Service Record button under its card |
| The roll of honour | The Honours button, top right |
| Change campaign | The Menu button. The campaign saves itself every month and when you leave |
| Quit | Cmd+Q |

The game opens on **the grand campaign**: pick any country still standing and the year you take it over (September 1939, or January 1940, 1941 or 1942: the same days as the browser version, from the shared `data/campaign/grand.json`). History is played up to that day; after it no nation follows history's plan, and the war is yours to shape. Choosing a later year takes a few seconds while that history is played. 1943 and 1944 are played as scenarios instead. The second page, **Historical scenarios**, holds single campaigns from the war as it was fought (the Battle of Britain, Barbarossa, Overlord and the rest), where the other nations keep to history's plan. The screen also offers to continue the campaign you left.

## What is here today

The campaign table for September 1939: Europe, North Africa and the Middle East as a relief model set into a table, 373 territories painted in their nations' colours, frontiers inked in, towns, seas and nations lettered, and every army standing on it as a piece (a counter for each corps, under the flag its nation flew in 1939-45, with the army's strength in thousands of men on a plate beneath). The flags are painted in code (`Campaign/CampaignFlags.cpp`), about fifty of them so far; Germany flies the black, white and red with the iron cross. Click a territory to see whose it is, whether it is friend or enemy, what it is worth and who is standing on it.

**It runs on the real rules.** The campaign rules worked out and tested in the browser prototype are the ones deciding everything here: where an army can march, what an attack comes to, what the other nations do with their month. So a war played here goes the way it goes there.

Each army's standard carries its strength in thousands of men, and is bigger the bigger the army.

What you can do today: choose a campaign and a side, pick up an army, see where it could go, march it, attack with it (face-off screen, odds, the Battle Ticket, then the result with each corps's losses), watch ground change hands and the frontier move, end the month, watch the other nations' months, and read the month's dispatches. If your country falls the war goes on and you can keep watching it.

**Unit stories.** Every corps keeps a record. Its card in the army tray shows the name the army has given it, how seasoned it is and a ribbon for each honour; the Service Record button under the card opens its honours with their citations and its diary. Honours fly as two-coloured streamers under the army's standard on the map. The Honours button opens the roll of honour and the memorial wall, and honours won and letters from the front arrive with the month's dispatches.

**The arsenal.** The Arsenal button opens everything your nation can order, in four columns: the army (infantry, mountain, artillery, motorized and armoured corps), the air force (fighter and bomber wings), the navy (destroyers, submarines, troopships, cruisers, carriers, battleships) and works for the territory you have selected (factories, shipyards, anti-aircraft guns, airfields, fortifications). Each shows its price, how long it takes, what it will cost to keep each month, and where it will go; what you cannot order yet says why. Click one to order it. Aircraft, ships and works are part of the grand campaign; a scenario offers corps and fortifications. The list is the rules' own, so the browser version offers the same things at the same prices.

**The treasury and the ledger.** The date plate always shows the money in the treasury and, in the grand campaign, what comes in, what the forces cost to keep, and what is left each month. Press it, or the Ledger button, for the books: money in by source, upkeep by arm, what is on order, and every province you hold with what it gives, richest first (click one to go to it). Every territory's panel says what it is worth each month. Click one of your own territories and its panel shows what it holds and what can be ordered there: new infantry, mountain, motorized and armoured corps, and the next level of fortification, each with its cost and how long it takes. An order can be taken back in the month it was placed.

**Standing orders and the railway.** An army in hand can be told to stand, dig in or fall back if it is attacked, and can be sent by rail to any friendly ground the track reaches.

What the rules can do that the 3D game does not show yet: sending an honoured unit home on a bond tour or as instructors, choosing which corps take drafts, raising a lost corps again under its old name, moving by sea, landings, fleets and air fleets, building and recruiting, research, diplomacy and generals' skills. The browser prototype has screens for all of these; they come across one at a time.

## How it is put together

| Path | What it is |
| --- | --- |
| `TheaterCommand/TheaterCommand.uproject` | The Unreal project. Double-click to open it in the Unreal editor |
| `TheaterCommand/Source/TheaterCommand/Campaign/` | The campaign table, in C++ |
| `.../CampaignMapData` | Reads the map, the nations and the 1939 armies from the prototype's own data files |
| `.../CampaignTerrain` | Works out the relief: coastlines, and mountains where the data says mountains |
| `.../CampaignMapActor` | Builds the table: land, sea, frontiers, lettering, town markers, the wooden surround |
| `.../ArmyPiece` | One nation's force in one territory, as a piece |
| `.../CampaignCameraPawn`, `.../CampaignPlayerController` | The view over the table and the controls |
| `.../CampaignHUD` | The panels on the screen |
| `.../CampaignEnvironment` | The lighting |
| `.../CampaignView`, `.../CampaignSession` | The picture of the war the rules hand over, and the campaign in progress: questions asked of the rules and orders given to them |
| `TheaterCommand/Source/TheaterCommand/Rules/RulesHost` | Runs the rules, which are JavaScript, inside the game |
| `TheaterCommand/Rules/campaign-bridge.ts` | The doorway to the rules: a short TypeScript file that asks the prototype's rules and hands back plain pictures of the war |
| `TheaterCommand/Content/Rules/campaign-rules.js` | The rules as the game runs them: the doorway and the prototype's `src/campaign`, bundled into one file by `Tools/build_rules.sh` |
| `TheaterCommand/Content/Rules/stand-in-rules.js` | A simple stand-in the game falls back on if the real rules file is missing |
| `.../CampaignGameMode` | Lays the table and starts the campaign when the game starts |
| `.../CampaignPhotographer` | A development tool that takes a fixed set of pictures and quits (`-TheaterShots=<folder>`) |
| `TheaterCommand/Tools/build_content.py` | Creates the few binary assets (three materials, one empty level) |
| `TheaterCommand/Content/` | Those assets |

Three rules shape the code:

- **One source of truth.** The Unreal build reads the same `data/` and `src/generated/` files the browser prototype and its tests use. When the map or the armies change there, they change here.
- **One set of rules.** The rules are not rewritten in C++. The prototype's TypeScript is bundled into a single JavaScript file and run inside the game by a JavaScript engine (the one that comes with macOS today; a portable one for Windows later). Everything crosses that boundary as JSON: the game asks by name (`start`, `view`, `reach`, `move`, `previewAttack`, `attack`, `endMonth`) and draws what comes back. So the 3D game cannot drift from the tested rules.
- **The table is built, not placed.** The level is empty. Everything on it is made from the data when the game starts, so nothing has to be arranged by hand in the editor and nothing large sits in the repository.

## The land and the sea

The relief is the real one. The land is modelled from NOAA's ETOPO 2022 elevation data (NOAA National Centers for Environmental Information, public domain, doi:10.25921/fd45-gt74), with heights exaggerated the way a relief map's are, and the sea is shaded by its real depth: pale over the shelves of the North Sea and the Baltic, dark over the Atlantic and the deep basins of the Mediterranean. `Tools/make_elevation.mjs` turns NOAA's grid into `Content/Data/Campaign/elevation.bin`, laid out in the campaign map's own projection; the header of that script gives the address the data was fetched from.

## The ground

The land's colour is not painted on its mesh but kept as pictures laid over it, one pixel for every point of the terrain (`Campaign/CampaignGround.cpp`): the bare country, the nations' paint, the lie of the land, and where rock, snow, sand and woods show. Over that the land material (`M_Land`) lays fine detail that repeats: grass, tree-tops, rock and sand, all worked out from noise when the game starts (`Campaign/CampaignTextures.cpp`), with no image files. Woods stand in patches, thick in forest country and a copse here and there in the open, and are laid on thinly from far off so that the nations' colours still read. When ground changes hands only the paint picture is touched. The sea is one sheet coloured from a picture (pale over the shoals, dark over the deeps) with small ripples sliding across it.

## Music

The music is one original piece, "Leave Pass": an up-tempo swing number for a 1940s dance band (walking bass, ride cymbal, rhythm guitar, a saxophone section, brass, and a clarinet chorus), about two and a half minutes on a loop. It is written as notes in `Tools/make_music.mjs` and played by a small synthesiser in the same file, so nothing in it is borrowed from any recording, film or game; the chords under the tune are the common property of the style. `Tools/build_music.sh` makes it and brings it into Unreal; the launcher does this the first time and whenever the piece has been rewritten. The result is about 20 MB and can always be remade, so it is not kept in the repository. It is a synthesiser, not a band: recorded music can replace it later without anything else changing. The slower piece it replaced, "The Map Room", is kept in `Tools/make_music_map_room.mjs`.

## Stand-ins

Some of what is on screen is a placeholder for the real thing:

- **Army pieces** carry simple figures made in code (`Campaign/CampaignMiniatures.cpp`): riflemen, a tank for armour, a field gun for artillery, a lorry for motorized troops, a horseman, a bunker, moulded in the nation's colour like the pieces of a board game. Sculpted miniatures can replace them.
- **Towns and trees** are made in code too: a cluster of houses for every town of note (a domed palace in a capital, a church in a city, a shed with a chimney for each pair of factories), and trees standing wherever the ground is wooded, drawn only near the eye.
- **Borders** are the prototype's approximate ones, grown from each territory's chief town.

## For whoever works on the code

From `TheaterCommand/`:

```
./Tools/build.sh             # compile the C++
./Tools/build_rules.sh       # bundle the rules again, after the prototype's rules or data change (needs Node.js)
./Tools/build_music.sh       # make the music and bring it into Unreal (needs Node.js)
node Tools/make_elevation.mjs <etopo.dods> Content/Data/Campaign/elevation.bin   # remake the height map from NOAA's data
./Tools/build_content.sh     # remake the few binary assets, after changing build_content.py
./Tools/take_pictures.sh     # run the game, take the photographer's set of pictures, print frame rates
```

`build.sh` wraps Unreal's own build script: while an Unreal editor is open the build tool numbers each build and sometimes fails to record which is current, and the wrapper checks for that.

Engine: Unreal 5.8, with Xcode and its Metal toolchain (`xcodebuild -downloadComponent MetalToolchain`).
