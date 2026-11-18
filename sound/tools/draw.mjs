// Draws a recording so that it can be read by eye, by a mixer who cannot hear it. Time runs left to right, pitch
// bottom to top on a true logarithmic scale (30 Hz to 16 kHz) with lines at the marked pitches, and brightness is
// level. A gunshot is a thin upright line; an engine is a ladder of level lines; a voice is a stack of wavering lines
// between 100 and 4000 Hz; music is level lines that step.
//
//   node sound/tools/draw.mjs <out.png> <file[@from-to]> [<file[@from-to]> ...] [--height 260] [--range 80]
//
// (ffmpeg's own picture was tried first and drew tones at the wrong pitch when zoomed, so this draws its own.)

import fs from 'node:fs';
import path from 'node:path';
import { spawnSync } from 'node:child_process';
import { SR, fft } from '../kit/dsp.mjs';
import { decode, probe, FFMPEG } from '../kit/source.mjs';

const a = process.argv.slice(2);
const opt = (k, d) => (a.includes(k) ? +a[a.indexOf(k) + 1] : d);
const H = opt('--height', 260), RANGE = opt('--range', 80), W = 1500, LEFT = 46, LO = 30, HI = 16000;
const specs = a.filter((x, i) => !x.startsWith('--') && !(a[i - 1] || '').startsWith('--'));
const out = specs.shift();
if (!out || !specs.length) { console.error('usage: node sound/tools/draw.mjs <out.png> <file[@from-to]> ...'); process.exit(1); }

const N = 4096, MARKS = [50, 100, 200, 500, 1000, 2000, 5000, 10000];
const rowOf = (hz) => Math.round((1 - Math.log(hz / LO) / Math.log(HI / LO)) * (H - 1));
// Heat: black, purple, red, orange, yellow, white.
const HEAT = [[0, 0, 0], [40, 0, 70], [130, 0, 90], [220, 30, 30], [250, 140, 0], [255, 230, 60], [255, 255, 255]];
const heat = (t) => { const x = Math.min(0.9999, Math.max(0, t)) * (HEAT.length - 1), i = Math.floor(x), f = x - i; return HEAT[i].map((c, k) => Math.round(c + (HEAT[i + 1][k] - c) * f)); };

const total = H * specs.length, img = Buffer.alloc((W + LEFT) * total * 3), labels = [];
specs.forEach((spec, n) => {
  const [file, span] = spec.split('@'), info = probe(file);
  const [from, to] = span ? span.split('-').map(Number) : [0, info.seconds];
  const x = decode(file, { from, to, channels: 1 }), top = n * H;
  const win = new Float32Array(N); for (let i = 0; i < N; i++) win[i] = 0.5 - 0.5 * Math.cos(2 * Math.PI * i / N);
  const cols = [];
  let peak = 1e-9;
  for (let c = 0; c < W; c++) {
    const mid = Math.round((c + 0.5) / W * x.length), re = new Float32Array(N), im = new Float32Array(N);
    for (let i = 0; i < N; i++) re[i] = (x[mid - N / 2 + i] || 0) * win[i];
    fft(re, im);
    const col = new Float32Array(H);
    for (let r = 0; r < H; r++) {
      // Each row takes the strongest bin between its own edges, so that a thin line is never missed.
      const f0 = LO * Math.pow(HI / LO, 1 - (r + 0.5) / (H - 1)), f1 = LO * Math.pow(HI / LO, 1 - (r - 0.5) / (H - 1));
      const k0 = Math.max(1, Math.floor(f0 * N / SR)), k1 = Math.max(k0, Math.min(N / 2 - 1, Math.ceil(f1 * N / SR)));
      let m = 0; for (let k = k0; k <= k1; k++) { const p = re[k] * re[k] + im[k] * im[k]; if (p > m) m = p; }
      col[r] = m; if (m > peak) peak = m;
    }
    cols.push(col);
  }
  for (let c = 0; c < W; c++) for (let r = 0; r < H; r++) {
    const d = 10 * Math.log10(cols[c][r] / peak + 1e-12), rgb = heat(1 + d / RANGE), at = ((top + r) * (W + LEFT) + LEFT + c) * 3;
    img[at] = rgb[0]; img[at + 1] = rgb[1]; img[at + 2] = rgb[2];
  }
  // Pitch lines, and a tick along the bottom every so many seconds.
  for (const hz of MARKS) { const r = rowOf(hz); for (let c = 0; c < W; c += 6) { const at = ((top + r) * (W + LEFT) + LEFT + c) * 3; img[at] = img[at + 1] = img[at + 2] = 150; } labels.push([2, top + r - 6, hz >= 1000 ? hz / 1000 + 'k' : String(hz)]); }
  const len = to - from, every = [0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 30, 60].find((s) => len / s <= 16) ?? 120;
  for (let t = Math.ceil(from / every) * every; t < to; t += every) {
    const c = Math.round((t - from) / len * W);
    for (let r = H - 10; r < H; r++) { const at = ((top + r) * (W + LEFT) + LEFT + c) * 3; img[at] = img[at + 1] = img[at + 2] = 255; }
    labels.push([LEFT + c + 3, top + H - 15, String(+t.toFixed(1))]);
  }
  labels.push([LEFT + 6, top + 4, `${path.basename(file).replace(/\.[a-z0-9]+$/i, '')}  ${from} to ${+to.toFixed(1)} s`]);
});
// The picture goes to ffmpeg as bare pixels (a PPM header was sometimes misread), which also writes the labels.
const rawFile = out + '.rgb';
fs.writeFileSync(rawFile, img);
const text = labels.map(([x, y, s]) => `drawtext=text='${s.replace(/[':\\,]/g, ' ')}':x=${x}:y=${y}:fontsize=12:fontcolor=white:box=1:boxcolor=black@0.55`).join(',');
const r = spawnSync(FFMPEG, ['-v', 'error', '-y', '-nostdin', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', `${W + LEFT}x${total}`, '-i', rawFile, '-vf', text, '-frames:v', '1', out], { encoding: 'utf8' });
fs.unlinkSync(rawFile);
if (r.status !== 0) { console.error((r.stderr || '').trim()); process.exit(1); }
console.log(`drawn to ${out}`);
