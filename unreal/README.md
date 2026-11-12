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
| Leave a corps behind | Click its card in the tray at the bottom |
| Put the army down | Esc |
| End the month | Enter, or the button at the bottom right |
| Quit | Cmd+Q |

## What is here today

The campaign table for September 1939: Europe, North Africa and the Middle East as a relief model set into a table, 373 territories painted in their nations' colours, frontiers inked in, towns, seas and nations lettered, and every army of the 1939 setup standing on it as a piece (a counter for each corps, under the nation's standard). Click a territory to see whose it is, what it is worth and who is standing on it.

You can pick up an army, see where it could go, send it there and watch the piece carried along the road, and end the month.

**The rules behind it are a stand-in for now.** The game's real rules are the browser prototype's own code, run inside Unreal (see below). Until that code is bundled in, a small stand-in answers in its place: armies march on their own ground, nothing fights, and ending the month only turns the calendar. The screen says so while the stand-in is in use.

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
| `TheaterCommand/Content/Rules/` | The rules file the game loads: `campaign-rules.js` (the real ones, when bundled) or `stand-in-rules.js` |
| `.../CampaignGameMode` | Lays the table and starts the campaign when the game starts |
| `.../CampaignPhotographer` | A development tool that takes a fixed set of pictures and quits (`-TheaterShots=<folder>`) |
| `TheaterCommand/Tools/build_content.py` | Creates the few binary assets (three materials, one empty level) |
| `TheaterCommand/Content/` | Those assets |

Three rules shape the code:

- **One source of truth.** The Unreal build reads the same `data/` and `src/generated/` files the browser prototype and its tests use. When the map or the armies change there, they change here.
- **One set of rules.** The rules are not rewritten in C++. The prototype's TypeScript is bundled into a single JavaScript file and run inside the game by a JavaScript engine (the one that comes with macOS today; a portable one for Windows later). Everything crosses that boundary as JSON: the game asks by name (`start`, `view`, `reach`, `move`, `endMonth`) and draws what comes back. So the 3D game cannot drift from the tested rules.
- **The table is built, not placed.** The level is empty. Everything on it is made from the data when the game starts, so nothing has to be arranged by hand in the editor and nothing large sits in the repository.

## Stand-ins

Some of what is on screen is a placeholder for the real thing:

- **Relief** is invented from each territory's terrain type. Real elevation data will replace it, so the Alps and the Carpathians are where they belong.
- **Army pieces** are plain blocks. Sculpted miniatures will replace them.
- **Borders** are the prototype's approximate ones, grown from each territory's chief town.

## For whoever works on the code

From `TheaterCommand/`:

```
./Tools/build.sh             # compile the C++
./Tools/build_content.sh     # remake the few binary assets, after changing build_content.py
./Tools/take_pictures.sh     # run the game, take the photographer's set of pictures, print frame rates
```

`build.sh` wraps Unreal's own build script: while an Unreal editor is open the build tool numbers each build and sometimes fails to record which is current, and the wrapper checks for that.

Engine: Unreal 5.8, with Xcode and its Metal toolchain (`xcodebuild -downloadComponent MetalToolchain`).
