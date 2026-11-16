// Draws pictures of sounds (time left to right, pitch low to high, brightness for loudness), so that a sound can be
// checked by eye: node sound/tools/spectro.mjs out.png a.wav b.wav ...
import fs from 'node:fs';
import zlib from 'node:zlib';
import path from 'node:path';
import { fft } from '../kit/dsp.mjs';

function readWav(file) {
  const b = fs.readFileSync(file);
  const ch = b.readUInt16LE(22), sr = b.readUInt32LE(24);
  let p = 12; while (b.toString('ascii', p, p + 4) !== 'data') p += 8 + b.readUInt32LE(p + 4);
  const n = b.readUInt32LE(p + 4) / (2 * ch), x = new Float32Array(n);
  for (let i = 0; i < n; i++) { let s = 0; for (let c = 0; c < ch; c++) s += b.readInt16LE(p + 8 + (i * ch + c) * 2); x[i] = s / ch / 32768; }
  return { x, sr };
}
const W = 1100, H = 190, LO = 30, HI = 16000, GAP = 16;
const files = process.argv.slice(3);
const img = Buffer.alloc(W * (H + GAP) * files.length * 3, 12);
const ramp = (v) => { const t = Math.min(1, Math.max(0, v)); return [Math.round(255 * Math.min(1, t * 2.2)), Math.round(255 * Math.max(0, Math.min(1, t * 2.2 - 0.75))), Math.round(255 * (t < 0.35 ? t * 1.6 : Math.max(0, t * 3 - 2)))]; };
const FONT = { };
files.forEach((file, row) => {
  const { x, sr } = readWav(file);
  const size = 4096, hop = Math.max(64, Math.floor((x.length - size) / W));
  const y0 = row * (H + GAP) + GAP;
  for (let col = 0; col < W; col++) {
    const start = col * hop;
    if (start + size > x.length + size / 2) break;
    const re = new Float64Array(size), im = new Float64Array(size);
    for (let i = 0; i < size; i++) re[i] = (x[start + i] ?? 0) * (0.5 - 0.5 * Math.cos(2 * Math.PI * i / size));
    fft(re, im, false);
    for (let py = 0; py < H; py++) {
      const f0 = LO * Math.pow(HI / LO, py / H), f1 = LO * Math.pow(HI / LO, (py + 1) / H);
      const b0 = Math.max(1, Math.floor(f0 * size / sr)), b1 = Math.max(b0 + 1, Math.ceil(f1 * size / sr));
      let e = 0; for (let k = b0; k < b1 && k < size / 2; k++) e = Math.max(e, re[k] * re[k] + im[k] * im[k]);
      const dbv = 10 * Math.log10(e / (size * size / 16) + 1e-12);
      const [r, g, bl] = ramp((dbv + 80) / 80);
      const o = ((y0 + H - 1 - py) * W + col) * 3;
      img[o] = r; img[o + 1] = g; img[o + 2] = bl;
    }
  }
  // Lines at 100 Hz, 1 kHz and 10 kHz; a white tick every second along the top.
  for (const f of [100, 1000, 10000]) { const py = Math.round(H * Math.log(f / LO) / Math.log(HI / LO)); for (let c = 0; c < W; c += 6) { const o = ((y0 + H - 1 - py) * W + c) * 3; img[o] = img[o + 1] = img[o + 2] = 150; } }
  for (let s = 0; s * sr / hop < W; s++) { const c = Math.round(s * sr / hop); for (let yy = 0; yy < 7; yy++) { const o = ((y0 + yy) * W + c) * 3; img[o] = img[o + 1] = img[o + 2] = 255; } }
  process.stdout.write(`row ${row + 1}: ${path.basename(file)} ${(x.length / sr).toFixed(2)} s\n`);
});
const Ht = (H + GAP) * files.length;
const raw = Buffer.alloc((W * 3 + 1) * Ht);
for (let y = 0; y < Ht; y++) { raw[y * (W * 3 + 1)] = 0; img.copy(raw, y * (W * 3 + 1) + 1, y * W * 3, (y + 1) * W * 3); }
const crcT = new Uint32Array(256).map((_, n) => { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; return c >>> 0; });
const crc = (buf) => { let c = 0xFFFFFFFF; for (const v of buf) c = crcT[(c ^ v) & 255] ^ (c >>> 8); return (c ^ 0xFFFFFFFF) >>> 0; };
const chunk = (type, data) => { const len = Buffer.alloc(4); len.writeUInt32BE(data.length); const td = Buffer.concat([Buffer.from(type), data]); const c = Buffer.alloc(4); c.writeUInt32BE(crc(td)); return Buffer.concat([len, td, c]); };
const head = Buffer.alloc(13); head.writeUInt32BE(W, 0); head.writeUInt32BE(Ht, 4); head[8] = 8; head[9] = 2;
fs.writeFileSync(process.argv[2], Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk('IHDR', head), chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]));
