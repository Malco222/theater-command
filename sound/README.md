# Sound

Every sound in the game is made by the program in this folder. Nothing is recorded, sampled or borrowed: each sound is synthesised from tones and noise to a recipe written for this game, and the same program always writes the same files.

**To listen:** the [listening board](https://claude.ai/artifact/Y4PGKtarPo2J2mqs6oUuYV) plays every sound and scene and takes a Good, Fix or Cut stamp for each.

## What is here

| Path | What it is |
|---|---|
| `docs/sound-brief.md` | The plan: the families, day and night, march against train, mixing and loudness rules, which event gets which sound |
| `docs/gate-7-sound-list.md` | The sound team's list against gate 7 of the release 1 finish line |
| `docs/implementation-3d.md` | The plan for the 3D game |
| `docs/implementation-browser.md` | The browser game's smaller set and how to wire it |
| `docs/ui-hooks.md` | The interface's thirty-six sound hooks and the sound that answers each |
| `docs/catalogue.md`, `docs/licence-log.md` | Every sound, and where every file came from (written by `tools/make_docs.mjs`) |
| `make_sounds.mjs`, `recipes/`, `kit/` | The generator: 203 sounds in ten families |
| `make_scenes.mjs` | Mixes the seven target scenes and the five stings from the sounds |
| `browser/` | The browser game's set (`set.mjs`) and its player (`sound.ts`) |
| `browser-set/` | The browser game's 80 sounds, packed as small MP3s, with `browser-sounds.json` |
| `board/`, `tools/make_board.mjs` | The listening board's page and its builder |
| `tools/` | The fault check, the MP3 packer, the loudness meter, the spectrogram |

## To make everything again

It needs Node.js (no packages) and, for the MP3s only, `ffmpeg`. `OUT` is any folder outside the repository; the WAVs are not kept in git.

```
node sound/make_sounds.mjs $OUT/wav                      # 203 WAVs and sound-list.json, about two minutes
node sound/tools/check.mjs $OUT/wav                      # the fault check
node sound/make_scenes.mjs $OUT/wav $OUT/scenes          # 7 scenes, 5 stings, scene-list.json
node sound/tools/make_mp3.mjs $OUT/wav $OUT/mp3          # MP3 copies to listen to
node sound/tools/make_mp3.mjs $OUT/scenes $OUT/mp3scenes
node sound/tools/make_browser_set.mjs $OUT/wav $OUT/scenes sound/browser-set
node sound/tools/make_docs.mjs $OUT/wav $OUT/scenes sound/docs
node sound/tools/make_board.mjs $OUT/wav $OUT/scenes $OUT/mp3 $OUT/mp3scenes sound/browser-set $OUT/board
```

`node sound/make_sounds.mjs $OUT/wav desk` makes one family; a sound's name makes one sound.

## The rule that matters

Nobody who works on these sounds can hear them. They are checked by measurement (`tools/check.mjs`) and by picture (`tools/spectro.mjs`), which finds faults but cannot say whether a tank sounds like a tank. A person's ears decide, on the listening board.
