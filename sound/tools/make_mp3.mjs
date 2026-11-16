// Packs the made sounds as MP3s, for the listening board and for the browser game. (The 3D game takes the WAVs.)
//
//   node sound/tools/make_mp3.mjs <folder of made sounds> <out-folder> [--quality 5] [--only name,name,...]
//
// A loop cannot simply be packed: an MP3 always has a few hundredths of a second of silence at each end, and the loop
// would click every time it came round. So a loop is packed with half a second of itself added at each end (its own
// tail before it, its own head after it). The player then goes round a stretch exactly one loop long inside that,
// starting a quarter of a second in; wherever the packer's silence falls, it is outside the stretch. The list this
// writes (mp3-list.json) gives each loop's "from" and "length" for that.

import fs from 'node:fs';
import path from 'node:path';
import { execFileSync } from 'node:child_process';
import { SR, sec, isStereo, lengthOf, readWav, writeWav } from '../kit/dsp.mjs';

const args = process.argv.slice(2);
const flag = (name, fallback) => { const i = args.indexOf(name); if (i < 0) return fallback; const v = args[i + 1]; args.splice(i, 2); return v; };
const quality = flag('--quality', '5'), only = flag('--only', null)?.split(',');
const [src, out] = args;
if (!src || !out) { console.error('usage: node sound/tools/make_mp3.mjs <folder of made sounds> <out-folder> [--quality 5] [--only a,b]'); process.exit(1); }
fs.mkdirSync(out, { recursive: true });
const PAD = 0.5, FROM = 0.25;
const listFile = ['sound-list.json', 'scene-list.json'].map((f) => path.join(src, f)).find((f) => fs.existsSync(f));
const list = JSON.parse(fs.readFileSync(listFile, 'utf8'));
const rows = [];
let bytes = 0;
for (const s of list) {
  if (only && !only.includes(s.name)) continue;
  const wav = path.join(src, s.name + '.wav'), mp3 = path.join(out, s.name + '.mp3');
  let from = wav;
  if (s.loop) {
    const x = readWav(wav), n = lengthOf(x), p = sec(PAD);
    const wrap = (c) => { const y = new Float32Array(n + p * 2); y.set(c.subarray(n - p), 0); y.set(c, p); y.set(c.subarray(0, p), p + n); return y; };
    from = path.join(out, s.name + '.padded.wav');
    writeWav(from, isStereo(x) ? { L: wrap(x.L), R: wrap(x.R) } : wrap(x));
  }
  execFileSync('ffmpeg', ['-hide_banner', '-loglevel', 'error', '-y', '-i', from, '-codec:a', 'libmp3lame', '-q:a', quality, mp3]);
  if (s.loop) fs.unlinkSync(from);
  const size = fs.statSync(mp3).size; bytes += size;
  rows.push({ name: s.name, file: s.name + '.mp3', bytes: size, seconds: s.seconds, ...(s.loop ? { loop: { from: FROM, length: s.seconds } } : {}) });
}
fs.writeFileSync(path.join(out, 'mp3-list.json'), JSON.stringify(rows, null, 1));
console.log(`${rows.length} MP3s, ${(bytes / 1048576).toFixed(1)} MB`);
