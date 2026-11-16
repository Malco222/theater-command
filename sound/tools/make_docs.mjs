// Writes the two lists that are tables of facts rather than prose: the catalogue of every sound, and the licence log.
// Both come straight from sound-list.json and scene-list.json, so they cannot drift from what was made.
//
//   node sound/tools/make_docs.mjs <folder of made sounds> <folder of scenes> <out-folder>

import fs from 'node:fs';
import path from 'node:path';

const [src, scenesDir, out] = process.argv.slice(2);
const list = JSON.parse(fs.readFileSync(path.join(src, 'sound-list.json'), 'utf8'));
const scenes = JSON.parse(fs.readFileSync(path.join(scenesDir, 'scene-list.json'), 'utf8'));
fs.mkdirSync(out, { recursive: true });

const FAMILIES = [
  ['air', 'Aircraft', 'Engines by kind, fly-pasts, a dive, a dogfight, a plane going down.'],
  ['flak', 'Flak and searchlights', 'The guns that defend a town, their shells bursting, the lamps.'],
  ['guns', 'Guns, bombs and battle', 'Artillery, shells, small arms, tank guns, bombs, explosions, the noise of a fight.'],
  ['ground', 'Armies on the move and at work', 'Tanks, lorries, half-tracks, boots, horses, digging, building, a factory.'],
  ['rail', 'Railways', 'A steam train from every side: going, stopping, turning round at the line\'s end, the rails cut and mended.'],
  ['sea', 'Ships and the sea war', 'Sirens, engines, big guns, sonar, depth charges, torpedoes, a ship going down, landing craft, carriers.'],
  ['fire', 'Fire', 'One building alight, a town burning at night, a roof falling in.'],
  ['desk', 'The desk (interface)', 'Every button, card, order and page: the sounds of a staff officer\'s desk.'],
  ['calls', 'Calls', 'The band\'s short pieces for the great moments, and the sirens.'],
  ['beds', 'Ambience', 'Beds (long loops of pure texture) and sprinkles (single events dropped in at random).'],
];
const kind = (s) => `${s.channels === 1 ? 'mono' : 'stereo'}${s.loop ? ', loop' : ''}`;
const esc = (t) => String(t).replace(/\|/g, '\\|');

// ---- The catalogue -------------------------------------------------------------------------------------------
let cat = `# Catalogue of sounds\n\nEvery sound in the set, by family: ${list.length} sounds and ${scenes.filter((s) => s.sting).length} stings, with ${scenes.filter((s) => !s.sting).length} target scenes. This file is written by \`sound/tools/make_docs.mjs\` from the list the generator makes, so it always matches the files.\n\n`;
cat += `How to read a row. **Name** is the file name without \`.wav\` and the name the game calls. **Kind** says mono or stereo and whether it is a loop: mono sounds are placed on the map by the game, stereo ones are played as they are. **Seconds** is its length. **LUFS** is its loudness as made (for a loop the whole of it, for anything else its loudest moment). **Small speaker** is how many dB it loses through a laptop speaker; under 6 is safe.\n\n`;
for (const [key, title, what] of FAMILIES) {
  const rows = list.filter((s) => s.family === key);
  if (!rows.length) continue;
  cat += `## ${title} (${rows.length})\n\n${what}\n\n| Name | Kind | Seconds | LUFS | Small speaker | What it is and when it plays |\n|---|---|---:|---:|---:|---|\n`;
  for (const s of rows) cat += `| \`${s.name}\` | ${kind(s)} | ${s.seconds} | ${s.lufs} | ${s.smallSpeakerLoss} | ${esc(s.about)} |\n`;
  cat += '\n';
}
cat += `## Stings (${scenes.filter((s) => s.sting).length})\n\nShort mixes of the sounds above that are themselves sounds, for a card that reports something it cannot show.\n\n| Name | Seconds | Made from | What it is and when it plays |\n|---|---:|---|---|\n`;
for (const s of scenes.filter((x) => x.sting)) cat += `| \`${s.name}\` | ${s.seconds} | ${s.sounds.map((n) => `\`${n}\``).join(', ')} | ${esc(s.about)} |\n`;
cat += `\n## Target scenes (${scenes.filter((s) => !s.sting).length})\n\nNot for the game: these are what the game should sound like when the sounds are laid together. Each is a list of cues in \`scene-list.json\` (when, which sound, how loud in dB against the file, where from left to right).\n\n| Scene | Seconds | What happens in it |\n|---|---:|---|\n`;
for (const s of scenes.filter((x) => !x.sting)) cat += `| \`${s.name}\` | ${s.seconds} | ${esc(s.about)} |\n`;
fs.writeFileSync(path.join(out, 'catalogue.md'), cat);

// ---- The licence log -----------------------------------------------------------------------------------------
let log = `# Licence log\n\n**Every sound in this set is original.** Each was synthesised from mathematical tones and random noise by our own program (\`sound/make_sounds.mjs\` and the recipes beside it), written for this game in November 2026. No recording, sample, library sound, impulse response, soundfont or music from any other source is in any of them, and nothing was copied from any game, film or record.\n\n`;
log += `| Question | Answer |\n|---|---|\n`;
log += `| Whose they are | The project's. They were made for this game, and no third party's material is in them. |\n`;
log += `| Licence needed to ship them | None. There is no third-party material to license and no credit owed. |\n`;
log += `| Third-party sounds used | None. |\n`;
log += `| Third-party code used | None. The generator uses only what comes with Node.js (\`node:fs\`, \`node:path\`). The MP3 copies are packed with ffmpeg and LAME; that packs our audio and adds none of theirs. |\n`;
log += `| The echoes | Made by the generator from filtered noise (\`makePlace\` in \`kit/dsp.mjs\`). No measured or bought impulse responses. |\n`;
log += `| The tunes | The band calls are our own, built on one five-note figure (F, B flat, C, A, D) written for this game. They use notes a bugle cannot play, so none can be a real army's call; no anthem, march, hymn or film tune is quoted. The Morse in \`radio\` spells the game's own initials. |\n`;
log += `| Real things imitated | Only kinds of thing (a radial engine, a steam whistle, an air raid siren's wail, a 1940s telephone bell), which nobody owns. No maker's recording of any of them was used or analysed. |\n`;
log += `| Can they be made again | Yes, exactly. Every sound's random numbers are seeded from its name, so the same program always writes the same files. |\n\n`;
log += `**Sources looked at and not used.** The brief allowed CC0 libraries. The free libraries were not reachable from the workshop (the network policy refuses them), so none was downloaded, listened to or used. If CC0 recordings are added later, each one must get a row here with its address, its author, the date fetched and the text of its licence, and it must be checked that the licence really is CC0 and not "free for non-commercial use".\n\n`;
log += `**What has not been checked.** The tunes were written to be unlike any call or tune the sound director knows, but nobody has yet listened to them: a person's ear should confirm that none sounds like something famous by accident. That check is Brian's listening pass (gate 7, item 2).\n\n`;
log += `## Every file\n\n| File | Family | Source | Recipe | Third-party material | Licence |\n|---|---|---|---|---|---|\n`;
for (const s of list) log += `| \`${s.name}.wav\` | ${s.family} | Original synthesis | \`${s.recipe ?? ''}\` | none | none needed (original) |\n`;
for (const s of scenes) log += `| \`${s.name}.wav\` | ${s.sting ? 'sting' : 'target scene'} | A mix of the files above | \`make_scenes.mjs\` | none | none needed (original) |\n`;
fs.writeFileSync(path.join(out, 'licence-log.md'), log);
console.log(`catalogue.md and licence-log.md written: ${list.length} sounds, ${scenes.length} mixes`);
