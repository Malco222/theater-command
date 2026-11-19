# Music: the score

Made 17 November 2026 in the thread "Compose an original music score", on Brian's word ("do all of them"
to the coordinator's list, item 4: a main theme plus calm, tense and battle tracks). Revised that evening
as Brian sent his own songs:

- "use this original score as the theme for theme song for the game" (*The Long Walk Home*)
- "add this song to the play as another key song in the game" (*The Silent Front*)
- "i like this one more" (*A Vigil at the Ridge*, to play in the background)
- "add this to the sound tracks" (*Trench Line At Twilight*)
- "i think this should be the main theme song" (*The Final Salute*, 18 Nov; The Long Walk Home moved to homecoming)
- On the decision card (18 Nov) Brian chose "Rebuild all three on Final Salute": the three pieces written here now grow
  from The Final Salute; the versions built on The Long Walk Home are kept in `earlier/walk-versions/`.

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

**One tune ties the pieces for play together: The Final Salute's.** It opens with a low call in octaves over a
held F: C, up to F, down to E and D flat, back to C (5 1 7 flat-6 5 in F minor), played twice, the second time
with a voice below leaning from G to F. Above it comes the tune: up to C, down through A flat to F, then up to
a long D flat that sighs back to C, and on through E flat to C again. The D flat falling to C is in both. The
three pieces written here are built from those sixteen bars:

- **The Map Room** turns the tune to the major (C, A, F, then the long D flat over a borrowed D flat chord,
  sighing to C). The clarinet plays it in D minor in Brian's own rhythm. Oboe, clarinet, a single horn, then
  the violins. No drums.
- **Standing To** gives the call to the horns with trombones an octave below, in G minor (D G F sharp E flat
  D), over a held G as Brian's sits over F; the second time a voice leans from A to G. Later the horns carry
  the tune over muted trumpets, into the climax.
- **The Advance** hammers it into a march in D minor: the call on dotted drumbeats (A D C sharp B flat A),
  then the head of the tune. Between, the horns toss the call from chord to chord. In the middle the trumpets
  play the tune itself, broad, in its own key, F minor, in Brian's rhythm, then turn to A major and back.
- Brian's other songs keep their own tunes, in neighbouring keys.
- The tune was read from the recording by a transcriber (basic-pitch), played back on the orchestra
  (`source/pieces/salute_check.py`) and compared bar by bar: 0.84 average likeness on the right bars, 0.36 on
  the wrong ones (16 bars from 9.1 s).

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
- The Long Walk Home's tune was read the same way for the first rebuild (`source/pieces/walk_check.py`): 0.89
  average likeness on the right bars, 0.32 on the wrong ones.

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

## Four more cues (19 November 2026): a battle's end, before contact, the war map

Made in the thread "Add music and richer battle sound", on Brian's "all 4" to the coordinator's list (item 4: a
second pass on battle sound plus original score cues). Same orchestra, same tools, all built on The Final Salute.
To listen and stamp: the [listening page](https://claude.ai/artifact/EjLienLHJN29HKCPve1W4U) (it also has the
new battle sound). Not wired into either game until Brian stamps them.

| File | Title | Plays | Key, tempo | Length | Loudness |
|---|---|---|---|---|---|
| `battle_won` | The Field Is Ours | Once, when a battle is won (battle-mix.md section 6, the end) | F major; 100 slowing to 80 | 0:16 with its ring | -14 LUFS |
| `battle_lost` | The Line Gives Way | Once, when a battle is lost | F minor; 72 slowing to 54 | 0:15 with its ring | -17 LUFS |
| `zero_hour` | Zero Hour | Before contact in a battle, looping, until the first shot; then The Advance or Trench Line | A minor, 84 | 1:31, loops | -16.7 LUFS |
| `long_front` | The Long Front | Campaign map at war, taking turns with The Silent Front and Standing To | C minor, 66 | 2:25, loops | -16.8 LUFS |

How each grows from The Final Salute:

- **The Field Is Ours:** a drum roll on C, then horns with trumpets an octave above play the call turned to the
  major on dotted beats (C, F, E, D). The tune's long D flat, which sighs to C in Brian's song, here climbs: D flat
  to E flat to F, over D flat and E flat chords into F major, everyone at full strength.
- **The Line Gives Way:** one horn plays the call slowly (C, F, E, D flat), the trombones an octave under, and stops
  on C without going home. The violins come in on the long D flat and let it sigh to C, as in Brian's song, over
  a chord that does not rest. One plucked F closes it.
- **Zero Hour:** a slow heartbeat on bass drum and timpani, cellos and basses ticking in threes, threes and twos, a
  thin high tremolo. Muted horns (trombones an octave under) play the call in A minor (E, A, G sharp, F); the
  cellos take the head of the tune, open horns answer with its close, four chords climb to E with the drums
  hammering, then it drops back to the heartbeat and a clarinet plays the call low. It waits on A, the dominant of
  The Advance's D minor, so first contact resolves into it.
- **The Long Front:** a harp over low strings; the cellos play the tune in C minor, low; the horns take it an
  octave up with the violins answering; the whole orchestra plays it broad in E flat major (the long note on a
  bright C flat chord, as The Map Room turns it); then a clarinet and an oboe play the call quietly and the harp
  brings it round. No drums.

What was measured (in `checks/`: `<name>_balance.txt`, `<name>_picture.png`, `<name>.json`):

- Harmony and range checker: no warnings on any of the four.
- The tune leads in every section. The Field Is Ours: trumpets -15.4 and horns -16.0 LUFS against strings -19.5 to
  -21. The Line Gives Way: the single horn -21.5, the violins next at -25.6.
- Limiter: none on the stingers; 1.1 dB on Zero Hour (its four chords only), 0.7 dB on The Long Front (the major
  section). True peaks -1.1 dB or lower.
- Loop joins (`seamcheck.py`, where a click scores above 9): Zero Hour 5.6, The Long Front 3.6.
- The victory is 3 dB louder than the defeat, on purpose.

Rebuild one with `python3 source/pieces/<name>.py <out folder>` (they are not in `make_all.py`); the stingers
render without looping and with a 4.5 s tail. The muted horn in Zero Hour uses the sample library's
`FHornMute.sfz`, set up in the piece itself.

## Folders

- `mp3/`: for listening (the three written here at 192 kbps, Brian's five at 320 kbps); `the_salute.mp3`
  is The Final Salute's call and tune alone (8.9 to 41.6 s); `the_walk.mp3` is The Long Walk Home's first phrase.
- `game-wav/`: 44.1 kHz 16-bit WAV, for the game.
- `checks/`: balance tables, pictures, summaries.
- `brian-originals/`: Brian's five songs exactly as he sent them.
- `earlier/`: the first main theme (a hymn, `first_theme`), the five-note call, the first versions of the
  three pieces built on that call (`call-versions/`, MP3 only), and the versions built on The Long Walk Home
  (`walk-versions/`: WAV, MP3 and checks), with their checks. Kept in case they are wanted back.
- `source/`: the player (`engine.py`), the orchestra (`orchestra.py`), the notation and checker
  (`score.py`), the pieces, `prepare_songs.py`, the checks. `source/pieces_call_version/` holds the first
  versions; `source/pieces_walk_version/` the versions built on The Long Walk Home. Rebuild with `python3 make_all.py <out>` and `python3 prepare_songs.py brian-originals <out>`;
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
4. The four cues of 19 November, once stamped: `battle_won.wav` and `battle_lost.wav` become the battle's end
   stingers in place of the pack's `victory` and `defeat` (`mix.json` `music.end.stinger`); `zero_hour.wav` plays
   before contact and crossfades into `advance` or `trench_line` over 3 s at first contact; `long_front.wav` joins
   the war-map list after `silent_front` and `standing_to`. The proposal is in
   `../sound/soundscapes/mix-proposal.md`; `mix.json` is not changed until Brian approves.
