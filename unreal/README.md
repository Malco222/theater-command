# Theater Command in Unreal Engine 5

This folder is the Unreal Engine build of the game. The browser prototype in the rest of the repository worked out the rules; this is where they get their table, their miniatures and, later, their battlefields.

## Play it

Double-click **Play Theater Command.command** in this folder. A Terminal window opens for a few seconds, then the game opens in its own window. The first start takes a minute or two while Unreal prepares its materials; after that it is quick.

| To do this | Do this |
| --- | --- |
| Move across the map | Drag with the mouse or trackpad, or use W A S D or the arrow keys |
| Zoom | Scroll (two fingers on a trackpad), or the + and - keys |
| Turn the view | Hold the right button and drag, or use Q and E |
| Pick a territory | Click it |
| Put it down | Esc |
| Quit | Cmd+Q |

## What is here today

The campaign table for 1 September 1939: Europe, North Africa and the Middle East as a relief model set into a table, 373 territories painted in their nations' colours, frontiers inked in, towns and seas lettered, and every army of the 1939 setup standing on it as a piece (a counter for each corps, under the nation's standard). Click a territory to see whose it is, what it is worth and who is standing on it.

It is a map you can look at and move around, not yet a game you can play: nothing moves and no turn passes. The rules that make it a game are already written and tested in the browser prototype, and bringing them across is the next stage.

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
| `.../CampaignGameMode` | Lays the table when the game starts |
| `.../CampaignPhotographer` | A development tool that takes a fixed set of pictures and quits (`-TheaterShots=<folder>`) |
| `TheaterCommand/Tools/build_content.py` | Creates the few binary assets (three materials, one empty level) |
| `TheaterCommand/Content/` | Those assets |

Two rules shape the code:

- **One source of truth.** The Unreal build reads the same `data/` and `src/generated/` files the browser prototype and its tests use. When the map or the armies change there, they change here.
- **The table is built, not placed.** The level is empty. Everything on it is made from the data when the game starts, so nothing has to be arranged by hand in the editor and nothing large sits in the repository.

## Stand-ins

Some of what is on screen is a placeholder for the real thing:

- **Relief** is invented from each territory's terrain type. Real elevation data will replace it, so the Alps and the Carpathians are where they belong.
- **Army pieces** are plain blocks. Sculpted miniatures will replace them.
- **Borders** are the prototype's approximate ones, grown from each territory's chief town.

## For whoever works on the code

Build from `TheaterCommand/`:

```
"/Users/Shared/Epic Games/UE_5.8/Engine/Build/BatchFiles/Mac/Build.sh" TheaterCommandEditor Mac Development -project="$PWD/TheaterCommand.uproject" -waitmutex
```

Engine: Unreal 5.8, with Xcode and its Metal toolchain (`xcodebuild -downloadComponent MetalToolchain`).
