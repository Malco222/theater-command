// Makes every sound in the game. Each is synthesised here from tones and noise by our own recipes, so nothing is
// borrowed from any recording, film or game.
//
//   node sound/make_sounds.mjs <out-folder> [name or family ...]     e.g.  node sound/make_sounds.mjs out air rail
//
// One WAV a sound, named for what it is (bomb_stick_01.wav). Sounds placed on the map are mono, so the game can put
// them where they happen; the interface and the beds of ambience are stereo. A list of what was made, with each
// sound's loudness, goes to <out-folder>/sound-list.json.

import fs from 'node:fs';
import path from 'node:path';
import { SR, rng, seedOf, isStereo, toMono, toStereo, lengthOf, peak, toDb, loudness, writeWav, hp, hp4, eq, around, each, gain, copy } from './kit/dsp.mjs';
import { oneShot, loopShot } from './kit/parts.mjs';

import air from './recipes/air.mjs';
import guns from './recipes/guns.mjs';
import ground from './recipes/ground.mjs';
import rail from './recipes/rail.mjs';
import sea from './recipes/sea.mjs';
import fire from './recipes/fire.mjs';
import desk from './recipes/desk.mjs';
import calls from './recipes/calls.mjs';
import beds from './recipes/beds.mjs';

/** How loud each family is made (LUFS). The game then sets families against each other; see the brief's mixing rules. */
export const FAMILY = {
  air: { lufs: -20, bus: 'world' },
  flak: { lufs: -18, bus: 'world' },
  guns: { lufs: -16, bus: 'world' },
  ground: { lufs: -20, bus: 'world' },
  rail: { lufs: -21, bus: 'world' },
  sea: { lufs: -19, bus: 'world' },
  fire: { lufs: -24, bus: 'world' },
  desk: { lufs: -27, bus: 'interface' },
  calls: { lufs: -20, bus: 'calls' },
  beds: { lufs: -30, bus: 'ambience' },
};

const all = [];
const def = (family) => (name, meta, make) => all.push({ name, family, ...meta, make });
air(def('air')); guns(def('guns')); ground(def('ground')); rail(def('rail')); sea(def('sea')); fire(def('fire'));
desk(def('desk')); calls(def('calls')); beds(def('beds'));

const out = process.argv[2];
if (!out) { console.error('usage: node sound/make_sounds.mjs <out-folder> [name or family ...]'); process.exit(1); }
const want = process.argv.slice(3);
fs.mkdirSync(out, { recursive: true });
const list = [];
const started = Date.now();
for (const s of all) {
  if (want.length && !want.some((w) => s.name === w || s.family === w || s.fam === w || s.name.startsWith(w))) continue;
  const fam = FAMILY[s.fam ?? s.family];
  let x = s.make(rng(seedOf(s.name)));
  const target = s.lufs ?? fam.lufs;
  const ch = s.ch ?? (isStereo(x) ? 2 : 1);
  x = ch === 1 ? toMono(x) : toStereo(x);
  // Nothing below what a speaker can use: it only eats loudness and shakes the limiter. A loop is filtered as a loop.
  if (s.loop) x = around(x, (c) => hp4(c, s.lowCut ?? 36));
  else if (isStereo(x)) x = { L: hp(x.L, s.lowCut ?? 20), R: hp(x.R, s.lowCut ?? 20) }; else x = hp(x, s.lowCut ?? 20);
  // Small speakers. Most players hear this through a laptop, which gives almost nothing below about 120 Hz. A sound
  // that lives down there (far guns, a depth charge) would simply vanish. So we measure what each sound loses
  // through a small speaker and, where it loses much, lean its weight upward: the deep part stays for those who
  // have the speakers for it, but the part everyone can hear carries the sound.
  const by = s.loop ? 'integrated' : 'moment';
  const small = (v) => { const y = s.loop ? around(v, (c) => hp4(c, 120)) : (isStereo(v) ? { L: hp4(v.L, 120), R: hp4(v.R, 120) } : hp4(v, 120)); return loudness(v)[by] - loudness(y)[by]; };
  const lost = small(x);
  if (lost > 2.5) {
    const cut = -Math.min(10, (lost - 2.5) * 1.5);
    x = s.loop ? around(x, (c) => eq(c, 'low', 130, cut, 0.7)) : (isStereo(x) ? { L: eq(x.L, 'low', 130, cut, 0.7), R: eq(x.R, 'low', 130, cut, 0.7) } : eq(x, 'low', 130, cut, 0.7));
  }
  const lostNow = small(x);
  // A stereo sound may sit to one side, but only a little: beyond 2 dB it would pull the whole mix over.
  if (isStereo(x)) {
    const rmsOf = (c) => { let q = 0; for (let i = 0; i < c.length; i++) q += c[i] * c[i]; return Math.sqrt(q / c.length) || 1e-9; };
    const lean = 20 * Math.log10(rmsOf(x.L) / rmsOf(x.R));
    if (Math.abs(lean) > 2) { const fix = Math.pow(10, (Math.abs(lean) - 2) / 40); if (lean > 0) { gain(x.L, 1 / fix); gain(x.R, fix); } else { gain(x.L, fix); gain(x.R, 1 / fix); } }
  }
  x = s.loop ? loopShot(x, target) : oneShot(x, target, { fadeIn: s.fadeIn ?? 0.001, fadeOut: s.fadeOut ?? 0.03, floor: s.floor ?? -62 });
  const n = lengthOf(x), l = loudness(x);
  let bad = 0;
  for (const c of isStereo(x) ? [x.L, x.R] : [x]) for (let i = 0; i < n; i++) if (!Number.isFinite(c[i])) bad++;
  if (bad) { console.error(`${s.name}: ${bad} samples are not numbers`); process.exitCode = 1; continue; }
  writeWav(path.join(out, s.name + '.wav'), x);
  const row = {
    name: s.name, family: s.fam ?? s.family, bus: s.bus ?? fam.bus, loop: !!s.loop, channels: ch,
    seconds: +(n / SR).toFixed(2), peakDb: +toDb(peak(x)).toFixed(1), lufs: +(s.loop ? l.integrated : l.moment).toFixed(1),
    smallSpeakerLoss: +lostNow.toFixed(1), recipe: `recipes/${s.family}.mjs`, about: s.about ?? '', same: s.same ?? null,
  };
  list.push(row);
  console.log(`${row.name.padEnd(26)} ${String(row.seconds).padStart(6)} s  ${ch === 1 ? 'mono  ' : 'stereo'}  peak ${String(row.peakDb).padStart(6)} dB  ${String(row.lufs).padStart(6)} LUFS${s.loop ? '  loop' : ''}`);
}
if (!want.length) fs.writeFileSync(path.join(out, 'sound-list.json'), JSON.stringify(list, null, 1));
else fs.writeFileSync(path.join(out, `sound-list.${want.join('-')}.json`), JSON.stringify(list, null, 1));
console.log(`${list.length} sounds in ${((Date.now() - started) / 1000).toFixed(1)} s`);
