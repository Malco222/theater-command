// Packs the browser game's smaller set: small MP3s and one list that says what each is and how to play it.
//
//   node sound/tools/make_browser_set.mjs <folder of made sounds> <folder of scenes> <out-folder>
//
// Mono unless the set says stereo. Loops are packed with half a second of themselves at each end (see make_mp3.mjs
// for why); the list gives the stretch to go round.

import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { sec, isStereo, lengthOf, readWav, writeWav } from '../kit/dsp.mjs';
import { BROWSER_SET } from '../browser/set.mjs';

const [src, scenesDir, out] = process.argv.slice(2);
if (!src || !scenesDir || !out) { console.error('usage: node sound/tools/make_browser_set.mjs <sounds> <scenes> <out-folder>'); process.exit(1); }
fs.mkdirSync(out, { recursive: true });
const list = Object.fromEntries(JSON.parse(fs.readFileSync(path.join(src, 'sound-list.json'), 'utf8')).map((s) => [s.name, s]));
const scenes = Object.fromEntries(JSON.parse(fs.readFileSync(path.join(scenesDir, 'scene-list.json'), 'utf8')).map((s) => [s.name, s]));
// How loud each kind plays against its file as made (dB): the browser game's whole mix in four numbers.
const GAIN = { interface: 0, calls: -2, world: -4, ambience: 0 };
const PAD = 0.5, FROM = 0.25;
const rows = [];
let bytes = 0;
for (const s of BROWSER_SET) {
  const made = s.from === 'scenes' ? scenes[s.name] : list[s.name];
  if (!made) { console.error(`${s.name} is in the browser set but was not made`); process.exitCode = 1; continue; }
  const wav = path.join(s.from === 'scenes' ? scenesDir : src, s.name + '.wav'), mp3 = path.join(out, s.name + '.mp3');
  let from = wav;
  if (s.loop) {
    const x = readWav(wav), n = lengthOf(x), p = sec(PAD);
    const wrap = (c) => { const y = new Float32Array(n + p * 2); y.set(c.subarray(n - p), 0); y.set(c, p); y.set(c.subarray(0, p), p + n); return y; };
    from = path.join(out, s.name + '.padded.wav');
    writeWav(from, isStereo(x) ? { L: wrap(x.L), R: wrap(x.R) } : wrap(x));
  }
  execFileSync('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', '-i', from, '-ac', s.stereo ? '2' : '1', '-codec:a', 'libmp3lame', '-q:a', s.bed ? '8' : '7', mp3]);
  if (s.loop) fs.unlinkSync(from);
  const size = fs.statSync(mp3).size; bytes += size;
  const bus = s.bed ? 'ambience' : (made.bus ?? 'world');
  rows.push({
    name: s.name, file: s.name + '.mp3', bytes: size, seconds: made.seconds, bus, gain: GAIN[bus] ?? 0,
    ...(s.loop ? { loop: { from: FROM, length: made.seconds } } : {}), ...(s.hook ? { hook: s.hook } : {}), on: s.on, about: made.about ?? '',
  });
}
fs.writeFileSync(path.join(out, 'browser-sounds.json'), JSON.stringify(rows, null, 1));
console.log(`${rows.length} sounds for the browser game, ${(bytes / 1024).toFixed(0)} KB (${(bytes * 4 / 3 / 1048576).toFixed(2)} MB once written into the page)`);
