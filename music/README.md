# Music: the score

Made 17 November 2026 in the thread "Compose an original music score", on Brian's word ("do all of them"
to the coordinator's list, item 4: a main theme plus calm, tense and battle tracks). Revised that evening
as Brian sent his own songs:

- "use this original score as the theme for theme song for the game" (*The Long Walk Home*)
- "add this song to the play as another key song in the game" (*The Silent Front*)
- "i like this one more" (*A Vigil at the Ridge*, to play in the background)
- "add this to the sound tracks" (*Trench Line At Twilight*)
- "i think this should be the main theme song" (*The Final Salute*, 18 Nov; The Long Walk Home moved to homecoming)

(*Sovereign Ground* was sent and then withdrawn: "forget this one i dont like it". It is not used anywhere.)

**To listen:** the listening page (link in the thread) plays all eight, shows where each section starts,
lets you hear each loop's join, and takes a Good / Fix / Cut stamp per piece.

## The eight pieces: each mood in play has two or three that take turns

| File | Title | Plays | Key, tempo | Length | Made by |
|---|---|---|---|---|---|
| `theme` | The Final Salute | Opening screen: the main theme | F minor; a march | 3:02 | Brian |
| `long_walk` | The Long Walk Home | When one of the player's wars ends; then the peacetime map, taking turns with the next two | B flat minor, ending in B flat major; about 75 | 2:58 | Brian |
| `map_room` | The Map Room | Campaign map at peace, taking turns with A Vigil at the Ridge and The Long Walk Home | F major, 66 | 2:27 | written here |
| `vigil` | A Vigil at the Ridge | Campaign map at peace, taking turns with The Map Room and The Long Walk Home | E flat major; slow | 2:58 | Brian |
| `silent_front` | The Silent Front | Campaign map at war, taking turns with Standing To | B flat major and G minor; about 59 | 2:57 | Brian |
| `standing_to` | Standing To | Campaign map at war, taking turns with The Silent Front | G minor, 92 | 2:05 | written here |
| `advance` | The Advance | Battle, taking turns with Trench Line At Twilight | D minor, 132 | 1:35 | written here |
| `trench_line` | Trench Line At Twilight | Battle, taking turns with The Advance | F minor; driving | 2:57 | Brian |

**One tune ties the pieces for play together.** The Long Walk Home (the main theme until 18 Nov) opens on a long F, climbs B flat, C, D flat, and
reaches up to E flat over the chord a half step above home (C flat) before falling back; its answer lifts
from minor into major. The three pieces written here are built from those eight bars:

- **The Map Room** turns the tune to the major (C F G A, then B flat over G flat). The clarinet plays it as
  Brian wrote it, in D minor. Oboe, clarinet, a single horn, then the violins. No drums.
- **Standing To** keeps it in G minor and takes its hope out: where the main theme lifts into a major chord,
  this one stays minor. Horns, then muted trumpets, then the climax.
- **The Advance** hammers it into a march in D minor (its long first note becomes a dotted drumbeat). In
  the middle the trumpets play the main theme's first phrase broad and slow, in its own key, B flat minor.
- Brian's other songs keep their own tunes, in neighbouring keys.
- **Open question for Brian (decision card, 18 Nov):** rebuild the three on The Final Salute's tune now
  that it is the main theme? Recommended: keep them on The Long Walk Home until the main theme settles.
  The Final Salute's opening figure, low in octaves, is C F E D flat C (F minor); it would suit the battle
  and tense pieces.

**Why each of Brian's songs went where it did** (measured, then judged; his call to move any of them):

- **The Final Salute, main theme:** Brian's choice. It opens on drums, then a low figure in octaves, and
  has the steadiest beat of his songs (pulse strength 0.64 to 0.76).
- **The Long Walk Home, homecoming:** it moves from B flat minor to B flat major at its loudest, dark to
  light, and its title is the end of a war. It plays when a war ends, then joins the peacetime pieces so it
  is still heard often.

- **A Vigil at the Ridge, peace:** the warmest of his songs. E flat major throughout, the least drum, a
  quiet hymn-like opening that grows slowly.
- **The Silent Front, the war map:** its title is that moment, a front between battles. Slow, it opens on a
  falling line, builds twice and reaches its biggest moment just before a fade. Standing To is the tense
  side of the same stretch.
- **Trench Line At Twilight, battle:** after The Final Salute, the most drum of his songs and the strongest beat (pulse strength
  0.58, against 0.46 for The Silent Front, 0.44 for The Long Walk Home and 0.09 for A Vigil at the Ridge;
  The Advance is 0.67), and after an 8-second quiet start it
  stays near full strength almost to the end.
- About eight minutes at peace, five at war and four and a half in battle before anything repeats.

## Brian's five songs

- Set by `source/prepare_songs.py`: the level only, each to sit with the piece it takes turns with.
  The Final Salute -4.3 dB and The Silent Front -4.47 dB, to -16 LUFS; The Long Walk Home -6.67 dB and
  A Vigil at the Ridge -6.55 dB, to -18; Trench Line At Twilight -3.08 dB, to -15. True peaks -2.6 dB or lower. No cuts, no EQ, no new endings.
- All five end with a fade. As loops, about 2.5 seconds of near-silence pass before they start again.
- The click detector flags spots inside the originals (Long Walk Home 2, Silent Front 1, Vigil 3, Trench
  Line 6, the last mostly at drum strokes; Final Salute none). They are in the recordings as received, not added here.
- The originals are kept untouched in `brian-originals/`.
- All five files carry Content Credentials signed by Google: "Created by Google Generative AI", with a SynthID
  watermark. See `licence-log.md` for what that means for a game that is sold.
- The Long Walk Home's tune was read from the recording by a transcriber (basic-pitch), then played back on
  the orchestra (`source/pieces/walk_check.py`) and compared bar by bar: 0.89 average likeness on the right
  bars, 0.32 on the wrong ones.

## How the three pieces written here were made

- **Real recorded instruments, no synthesiser.** Every note is a recording of a real player from the
  VS Chamber Orchestra 2: Community Edition (CC0): string sections, horns, trumpets, trombones, tuba,
  flute, oboe, clarinet, bassoon, harp, timpani, a marching snare, bass drum, cymbals and tam-tam.
- The notes are written in Python (`source/pieces/*.py`), one file a piece, with the harmony of every bar
  written beside the tune. A checker compares every held note on a strong beat against that harmony and
  every note against its instrument's range; all three pass with no warnings.
- Each desk is levelled from measurements so that a marked mf is equally strong on every instrument, then
  placed on a concert stage in a hall about 2.5 seconds long. In every section the desk with the tune
  measures loudest (the horn chords under the Advance's broad middle were brought down a step for this).
- Set by loudness (LUFS): Standing To at -16, the Map Room at -18, the Advance at -15; peaks under -1 dB.
- **The loop joins.** `source/seamcheck.py` plays each loop's last two seconds into its first two and looks
  for a click. Two fixes went in: the finishing stage now hears a loop's end before its start, and a note
  that starts a hair before the first beat now has its first moments laid at the loop's end instead of
  being cut off. All three joins now measure below the music's own attacks (4.2x, 4.2x, 4.4x, where a click
  scores above 9). The first Standing To had a click at its join (32x) that the earlier check missed.

**What was measured, not heard.** No Claude can hear. The checks in `checks/` are: the balance of every
desk in every section (`*_balance.txt`), a picture of each piece (`*_picture.png`), `summary.json` and
`brian_songs.json`. Brian's ear is the test.

## Folders

- `mp3/`: for listening (the three written here at 192 kbps, Brian's five at 320 kbps); `the_walk.mp3`
  is The Long Walk Home's first phrase alone.
- `game-wav/`: 44.1 kHz 16-bit WAV, for the game.
- `checks/`: balance tables, pictures, summaries.
- `brian-originals/`: Brian's five songs exactly as he sent them.
- `earlier/`: the first main theme (a hymn, `first_theme`), the five-note call, and the first versions of the
  three pieces built on that call (`call-versions/`, MP3 only), with their checks. Kept in case they are
  wanted back.
- `source/`: the player (`engine.py`), the orchestra (`orchestra.py`), the notation and checker
  (`score.py`), the pieces, `prepare_songs.py`, the checks. `source/pieces_call_version/` holds the first
  versions. Rebuild with `python3 make_all.py <out>` and `python3 prepare_songs.py brian-originals <out>`;
  it needs the sample library from github.com/sgossner/VSCO-2-CE (master branch) and
  `pip install numpy scipy soundfile soxr pedalboard pyloudnorm matplotlib`.

## Into the 3D game (waits for Brian's ear, ruling 144)

Nothing here is in either game yet. When Brian passes the pieces:

1. Copy into `Saved/Music/`: `theme.wav` (The Final Salute), `long_walk.wav`, and the files for each mood
   in play: `map_room.wav` and `vigil.wav` (peace, with `long_walk.wav`), `silent_front.wav` and `standing_to.wav` (war, on the map), `advance.wav` and
   `trench_line.wav` (battle). Stop `build_music.sh` from running the synthesiser scripts.
2. In `build_music.py` and `SetMusicMood`, let each mood hold a list and play its pieces one after the
   other: opening screen `theme`; when a war ends, `long_walk` first; at peace Map Room, Vigil, Long Walk Home; at war on the map Silent Front then
   Standing To; fighting The Advance then Trench Line.
3. Volume (ruling 142, music near -27 LUFS under play): these files are about 6 dB quieter than the old
   theme, so `MusicVolume` near 0.28 rather than 0.15; the opening screen louder.
