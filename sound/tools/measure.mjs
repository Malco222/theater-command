// Says how loud any WAV is, the same way the sound set is measured, so that music and effects can be set against
// each other by number.
//
//   node sound/tools/measure.mjs file.wav [file.wav ...]
//
// For each file: its loudness over its whole length (LUFS), its loudest moment, its peak, and the volume number that
// would bring it to -27 LUFS (where music sits well under this set's effects).

import fs from 'node:fs';
import { loudness, peak, toDb } from '../kit/dsp.mjs';

/** Reads 16-bit WAVs of any layout (the kit's own reader only knows the kit's own files). */
function read(file) {
  const b = fs.readFileSync(file);
  let at = 12, channels = 2, bits = 16, data = null;
  while (at + 8 <= b.length) {
    const id = b.toString('ascii', at, at + 4), size = b.readUInt32LE(at + 4);
    if (id === 'fmt ') { channels = b.readUInt16LE(at + 10); bits = b.readUInt16LE(at + 22); }
    if (id === 'data') { data = b.subarray(at + 8, Math.min(b.length, at + 8 + size)); break; }
    at += 8 + size + (size % 2);
  }
  if (!data || bits !== 16) throw new Error(`${file}: only 16-bit WAVs can be measured`);
  const n = Math.floor(data.length / (channels * 2)), ch = Array.from({ length: Math.min(channels, 2) }, () => new Float32Array(n));
  for (let i = 0; i < n; i++) for (let k = 0; k < ch.length; k++) ch[k][i] = data.readInt16LE((i * channels + k) * 2) / 32768;
  return ch.length === 2 ? { L: ch[0], R: ch[1] } : ch[0];
}

for (const file of process.argv.slice(2)) {
  const x = read(file), l = loudness(x);
  const volume = Math.pow(10, (-27 - l.integrated) / 20);
  console.log(`${file}\n  whole ${l.integrated.toFixed(1)} LUFS   loudest moment ${l.moment.toFixed(1)} LUFS   peak ${toDb(peak(x)).toFixed(1)} dB   volume for -27 LUFS: ${volume.toFixed(2)}`);
}
