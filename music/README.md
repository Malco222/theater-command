# Music: the original score

Made in the thread "Compose an original music score", 17 November 2026, on Brian's word ("do all of them"
to the coordinator's list, item 4: a main theme plus calm, tense and battle tracks).

**To listen:** the listening page (link in the thread) plays all four, shows where each section starts,
lets you hear each loop's seam, and takes a Good / Fix / Cut stamp per piece.

## The four pieces

| File | Title | For | Key, tempo | Length |
|---|---|---|---|---|
| `theme` | Theater Command | The main theme: opening screen | B flat major, 72 | 2:15 |
| `map_room` | The Map Room | Calm: the campaign map at peace | F major, 66 | 2:27 |
| `standing_to` | Standing To | Tense: the campaign map at war | G minor, 92 | 2:05 |
| `advance` | The Advance | Battle | D minor, 132 | 1:35 |

Every piece loops without a seam: the hall's echo after the last bar is laid over the first bars.

**One tune ties them together.** All four are built on the game's own five-note call, F  B flat  C  A  D
(the figure the sound team's calls already use). The theme sings it as a hymn; the map room hands it
gently from oboe to clarinet to horn to violins; Standing To turns it minor in the horns; the battle
hammers it into a march, and in the middle the trumpets break into the main theme's version of it.

## How it was made

- **Real recorded instruments, no synthesiser.** Every note is a recording of a real player from the
  VS Chamber Orchestra 2: Community Edition (CC0): string sections, horns, trumpets, trombones, tuba,
  flute, oboe, clarinet, bassoon, harp, timpani, a marching snare, bass drum, cymbals and tam-tam.
  The old music (`make_music*.mjs` on the `unreal` branch) was drawn by a synthesiser; this replaces it.
- The notes are written in Python (`source/pieces/*.py`), one file a piece, with the harmony of every
  bar written beside the tune. A checker compares every held note on a strong beat against that harmony
  and every note against its instrument's range; all four pieces pass with no warnings.
- Each desk is levelled from measurements so that a marked mf is equally strong on every instrument,
  then placed on a concert stage (violins left, cellos right, brass and drums at the back) in a hall
  about 2.5 seconds long.
- The finished files are set by loudness (LUFS): the theme and Standing To at -16, the map room at -18,
  the battle at -15, peaks under -1 dB.

**What was measured, not heard.** No Claude can hear. The checks in `checks/` are: the balance of every
desk in every section (`*_balance.txt`), a picture of each piece (`*_picture.png`), and `summary.json`.
No clicks were found apart from the sharp attacks of staccato trombones in the battle piece; every loop
seam is within 6 dB from end to start. Brian's ear is the test.

## Folders

- `mp3/`: for listening (192 kbps).
- `game-wav/`: 44.1 kHz 16-bit WAV, for the game.
- `checks/`: balance tables, pictures, summary.
- `source/`: the player (`engine.py`), the orchestra (`orchestra.py`), the notation and checker
  (`score.py`), the four pieces, the checks. Rebuild with `python3 make_all.py <out>`; it needs the
  sample library from github.com/sgossner/VSCO-2-CE (master branch) and `pip install numpy scipy
  soundfile soxr pedalboard pyloudnorm matplotlib`.

## Into the 3D game (waits for Brian's ear, ruling 144)

Nothing here is in either game yet. When Brian passes the pieces:

1. Copy into `Saved/Music/`: `theme.wav` as is, `map_room.wav` as is, `advance.wav` as `battle.wav`,
   and `standing_to.wav` as a fourth piece. Stop `build_music.sh` from running the synthesiser scripts.
2. Add `standing_to.wav` (asset `Tense`) to `build_music.py` and a `tense` mood to `SetMusicMood`:
   opening screen `theme`, at peace `quiet`, at war on the map `tense`, fighting `battle`.
3. Volume (ruling 142, music near -27 LUFS under play): these files are 6 dB quieter than the old theme,
   so `MusicVolume` near 0.28 rather than 0.15; the opening screen louder.
