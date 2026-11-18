# The score

The eight pieces of the game's music go here as `.wav` files. They are large (about 212 MB together) and are not kept
in the repository: they are in the project's Library under `music/game-wav/`, with the music thread's README
(`music/README.md`), which says who made each and why it plays where it does.

| File | Title | Plays |
|---|---|---|
| `theme.wav` | The Final Salute (Brian) | the opening screen: the main theme |
| `map_room.wav` | The Map Room | the map at peace, in turn with the next two |
| `vigil.wav` | A Vigil at the Ridge (Brian) | the map at peace |
| `long_walk.wav` | The Long Walk Home (Brian) | the map at peace; and once, when one of the player's wars ends |
| `silent_front.wav` | The Silent Front (Brian) | the map at war, in turn with the next |
| `standing_to.wav` | Standing To | the map at war |
| `advance.wav` | The Advance | a battle, in turn with the next |
| `trench_line.wav` | Trench Line At Twilight (Brian) | a battle |

`Tools/build_music.sh` brings them into the game (to `/Game/Campaign/Music/Score`), and the launcher runs it by itself
when a file here is added or changed. A game without them plays the three synthesised pieces instead.
