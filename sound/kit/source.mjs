// Reads a real recording into the kit. Any file ffmpeg can open (WAV, FLAC, OGG, MP3, or the sound track of a film
// clip) comes back as plain samples at the kit's own rate, so a recipe can cut it, layer it and place it exactly as
// it once did with made-up tones. Nothing here invents sound: it only opens what was recorded.

import { spawnSync } from 'node:child_process';
import { SR } from './dsp.mjs';

/** The ffmpeg and ffprobe to use: the system's, or ones named in the environment (FFMPEG, FFPROBE). */
export const FFMPEG = process.env.FFMPEG || 'ffmpeg', FFPROBE = process.env.FFPROBE || 'ffprobe';

/** What a file is: its format, rate, channels, bit depth, bit rate and length, as ffprobe reports them. */
export function probe(file) {
  const r = spawnSync(FFPROBE, ['-v', 'error', '-print_format', 'json', '-show_format', '-show_streams', file], { encoding: 'utf8', maxBuffer: 1 << 26 });
  if (r.status !== 0) throw new Error(`${file}: ffprobe could not read it (${(r.stderr || '').trim().split('\n').pop()})`);
  const j = JSON.parse(r.stdout);
  const a = (j.streams || []).find((s) => s.codec_type === 'audio');
  if (!a) throw new Error(`${file}: no sound track in it`);
  return {
    container: j.format?.format_name ?? '',
    codec: a.codec_name,
    rate: +a.sample_rate,
    channels: +a.channels,
    bits: +(a.bits_per_raw_sample || a.bits_per_sample || 0) || null,
    kbps: Math.round((+a.bit_rate || +j.format?.bit_rate || 0) / 1000) || null,
    seconds: +(a.duration || j.format?.duration || 0),
    hasVideo: (j.streams || []).some((s) => s.codec_type === 'video' && s.disposition?.attached_pic !== 1),
    tags: { ...(j.format?.tags || {}), ...(a.tags || {}) },
  };
}

/**
 * The samples of a recording, at 48 kHz. "from" and "to" are seconds into the file; "channels" is 1 (both sides
 * summed) or 2 (a one-sided recording is given the same on both). Returns a Float32Array, or { L, R }.
 */
export function decode(file, { from = 0, to = null, channels = 1 } = {}) {
  const args = ['-v', 'error', '-nostdin'];
  if (from > 0) args.push('-ss', String(from));
  if (to != null) args.push('-t', String(Math.max(0, to - from)));
  args.push('-i', file, '-vn', '-map', '0:a:0', '-ac', String(channels), '-ar', String(SR), '-af', 'aresample=resampler=soxr:precision=28', '-f', 'f32le', '-');
  let r = spawnSync(FFMPEG, args, { maxBuffer: 1 << 30 });
  if (r.status !== 0) {
    // An ffmpeg built without the soxr resampler: its own is good enough.
    const plain = args.filter((a, i) => a !== '-af' && args[i - 1] !== '-af');
    r = spawnSync(FFMPEG, plain, { maxBuffer: 1 << 30 });
  }
  if (r.status !== 0) throw new Error(`${file}: ffmpeg could not decode it (${String(r.stderr || '').trim().split('\n').pop()})`);
  const b = r.stdout, n = Math.floor(b.length / (4 * channels));
  if (channels === 1) {
    const out = new Float32Array(n);
    for (let i = 0; i < n; i++) out[i] = b.readFloatLE(i * 4);
    return out;
  }
  const L = new Float32Array(n), R = new Float32Array(n);
  for (let i = 0; i < n; i++) { L[i] = b.readFloatLE(i * 8); R[i] = b.readFloatLE(i * 8 + 4); }
  return { L, R };
}
