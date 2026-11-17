// Makes the game's sounds from real recordings. Each sound is cut from one or more field recordings named in
// sound/recordings/sources.json (the licence log) and mixed by its recipe in sound/recordings/recipes.mjs. Nothing
// is synthesised: if a recording is missing, the sound is not made.
//
//   node sound/make_from_recordings.mjs <folder of raw recordings> <out-folder> [name ...]
//
// One WAV a sound, under the same names the game already uses, so nothing a builder holds has to change. A list of
// what was made goes to <out-folder>/sound-list.json in the same form as the old one, with each sound's sources.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { SR, isStereo, toMono, toStereo, lengthOf, peak, toDb, loudness, writeWav } from './kit/dsp.mjs';
import { decode } from './kit/source.mjs';
import { finishShot, finishLoop } from './kit/film.mjs';
import recipes from './recordings/recipes.mjs';

const here = path.dirname(fileURLToPath(import.meta.url));
const [raw, out, ...want] = process.argv.slice(2);
if (!raw || !out) { console.error('usage: node sound/make_from_recordings.mjs <raw-folder> <out-folder> [name ...]'); process.exit(1); }
const sources = JSON.parse(fs.readFileSync(path.join(here, 'recordings/sources.json'), 'utf8'));
const byId = Object.fromEntries(sources.map((s) => [s.id, s]));

/** Family loudness, as in the first set, so the game's mixing rules still hold. */
const FAMILY = { air: -20, flak: -18, guns: -16, ground: -20, rail: -21, sea: -19, fire: -24, desk: -27, calls: -20, beds: -30 };

const all = [];
const def = (name, meta, make) => all.push({ name, ...meta, make });
recipes(def);

fs.mkdirSync(out, { recursive: true });
const list = [];
for (const s of all) {
  if (want.length && !want.includes(s.name)) continue;
  const used = new Set();
  /** take('id', { from, to, channels }): the samples of a logged recording. Using a recording not in the log is an error. */
  const take = (id, o = {}) => {
    const row = byId[id]; if (!row) throw new Error(`${s.name}: "${id}" is not in the licence log`);
    const file = path.join(raw, row.file); if (!fs.existsSync(file)) throw new Error(`${s.name}: ${row.file} has not been fetched`);
    used.add(id);
    return decode(file, { channels: s.ch ?? 1, ...o });
  };
  let x;
  try { x = s.make(take); } catch (e) { console.error(`NOT MADE ${s.name}: ${e.message}`); process.exitCode = 1; continue; }
  const ch = s.ch ?? (isStereo(x) ? 2 : 1);
  x = ch === 1 ? toMono(x) : toStereo(x);
  const target = s.lufs ?? FAMILY[s.family];
  const done = s.loop ? finishLoop(x, target, s.finish) : finishShot(x, target, s.finish);
  x = done.x;
  writeWav(path.join(out, s.name + '.wav'), x);
  const n = lengthOf(x), l = loudness(x);
  const row = {
    name: s.name, family: s.family, bus: s.bus ?? 'world', loop: !!s.loop, channels: ch, seconds: +(n / SR).toFixed(2),
    peakDb: +toDb(peak(x)).toFixed(1), lufs: +(s.loop ? l.integrated : l.moment).toFixed(1), quieterThanAsked: done.short > 0.3 ? done.short : 0,
    title: s.title ?? s.name, about: s.about ?? '', real: true, sources: [...used].map((id) => ({ id, title: byId[id].title, licence: byId[id].licence, credit: byId[id].credit, page: byId[id].page })),
  };
  list.push(row);
  console.log(`${row.name.padEnd(22)} ${String(row.seconds).padStart(6)} s  ${ch === 1 ? 'mono  ' : 'stereo'}  peak ${String(row.peakDb).padStart(6)} dB  ${String(row.lufs).padStart(6)} LUFS${row.quieterThanAsked ? `  (${row.quieterThanAsked} dB under its target, to keep its peak whole)` : ''}  from ${[...used].join(', ')}`);
}
fs.writeFileSync(path.join(out, 'sound-list.json'), JSON.stringify(list, null, 1));
console.log(`${list.length} sounds made from recordings`);
