# Theater Command in Unreal Engine 5

This folder is the Unreal Engine build of the game. The browser prototype in the rest of the repository worked out the rules; this is where they get their table, their miniatures and, later, their battlefields.

## Play it

Double-click **Play Theater Command.command** in this folder. A Terminal window opens for a few seconds, then the game opens in its own window. The first start takes a minute or two while Unreal prepares its materials; after that it is quick.

| To do this | Do this |
| --- | --- |
| Move across the map | Drag with the mouse or trackpad, or use W A S D or the arrow keys |
| Zoom | Scroll (two fingers on a trackpad), or the + and - keys |
| Turn the view | Hold the right button and drag, or use Q and E |
| Pick up an army | Click one of yours (you lead Germany) |
| Send it somewhere | Right-click a territory. Lightened ground is where it can march; red ground is an attack |
| Attack | Right-click red ground. The two armies are drawn up on a face-off screen with the odds; send them in or call it off |
| Leave a corps behind | Click its card in the tray at the bottom |
| Put the army down | Esc |
| End the month | Enter, or the button at the bottom right. The other nations' moves are played out quickly; Space skips |
| Music on or off | M |
| Change campaign | The Menu button. The campaign saves itself every month and when you leave |
| Quit | Cmd+Q |

The game opens on a screen that offers every place a campaign can begin, from September 1939 to January 1944, with free play and watching, and offers to continue the campaign you left.

## What is here today

The campaign table for September 1939: Europe, North Africa and the Middle East as a relief model set into a table, 373 territories painted in their nations' colours, frontiers inked in, towns, seas and nations lettered, and every army standing on it as a piece (a counter for each corps, under the nation's standard). Click a territory to see whose it is, whether it is friend or enemy, what it is worth and who is standing on it.

**It runs on the real rules.** The campaign rules worked out and tested in the browser prototype are the ones deciding everything here: where an army can march, what an attack comes to, what the other nations do with their month. So a war played here goes the way it goes there.

Each army's standard carries its strength in thousands of men, and is bigger the bigger the army.

What you can do today: choose a campaign and a side, pick up an army, see where it could go, march it, attack with it (face-off screen, odds, the Battle Ticket, then the result with each corps's losses), watch ground change hands and the frontier move, end the month, watch the other nations' months, and read the month's dispatches. If your country falls the war goes on and you can keep watching it.

What the rules can do that the 3D game does not show yet: moving by rail and by sea, landings, fleets and air fleets, building and recruiting, research, diplomacy and generals' skills. The browser prototype has screens for all of these; they come across one at a time.

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

## Music

The music is one original piece, "The Map Room", written as notes in `Tools/make_music.mjs` and played by a small synthesiser in the same file, so nothing in it is borrowed from any recording, film or game. `Tools/build_music.sh` makes it and brings it into Unreal; the launcher does this the first time. The result is about 17 MB and can always be remade, so it is not kept in the repository. It is a synthesiser, not an orchestra: recorded music can replace it later without anything else changing.

## Stand-ins

Some of what is on screen is a placeholder for the real thing:

- **Relief** is invented from each territory's terrain type. Real elevation data will replace it, so the Alps and the Carpathians are where they belong.
- **Army pieces** are plain blocks. Sculpted miniatures will replace them.
- **Borders** are the prototype's approximate ones, grown from each territory's chief town.

## For whoever works on the code

From `TheaterCommand/`:

```
./Tools/build.sh             # compile the C++
./Tools/build_rules.sh       # bundle the rules again, after the prototype's rules or data change (needs Node.js)
./Tools/build_music.sh       # make the music and bring it into Unreal (needs Node.js)
./Tools/build_content.sh     # remake the few binary assets, after changing build_content.py
./Tools/take_pictures.sh     # run the game, take the photographer's set of pictures, print frame rates
```

`build.sh` wraps Unreal's own build script: while an Unreal editor is open the build tool numbers each build and sometimes fails to record which is current, and the wrapper checks for that.

Engine: Unreal 5.8, with Xcode and its Metal toolchain (`xcodebuild -downloadComponent MetalToolchain`).
