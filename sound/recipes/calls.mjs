// The calls: the short pieces of band music that mark the great moments. A campaign begun, a battle won or lost, war
// declared, a capital fallen, the month's report. They are ours: every tune here is built on one small figure of our
// own (the "command figure": F, B flat, C, A, D) and uses notes a bugle cannot play, so none of them can be any army's
// real call. No anthem, march or film tune is quoted.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono, lengthOf,
} from '../kit/dsp.mjs';
import { bell, knock, tick, brassNote, timpani, snare, bassDrum, stringsNote, glide } from '../kit/parts.mjs';

// The notes, by name.
const N = { Bb1: 34, F2: 41, G2: 43, Bb2: 46, C3: 48, Db3: 49, D3: 50, Eb3: 51, F3: 53, G3: 55, Ab3: 56, A3: 57, Bb3: 58, C4: 60, Db4: 61, D4: 62, Eb4: 63, F4: 65, G4: 67, Ab4: 68, A4: 69, Bb4: 70, C5: 72, Db5: 73, D5: 74, Eb5: 75, F5: 77, G5: 79 };

const hall = (x, wet = -11) => inPlace(x, 'hall', wet);

/** A band's picture: each line of notes sits somewhere between the left and the right. */
function band(seconds) {
  const out = stereo(seconds);
  return {
    out,
    /** Brass: notes are [beat, note, beats, loudness]. */
    brass(r, bpm, at, notes, { pan = 0, players = 3, bright = 1, gate = 0.9, level: g = 1 } = {}) {
      const beat = 60 / bpm;
      for (const [b, note, d, loud = 0.8] of notes) place(out, brassNote(r, note, d * beat * gate, { loud, players, bright }), at + b * beat, pan, g);
      return this;
    },
    strings(r, bpm, at, notes, { pan = 0, level: g = 1 } = {}) {
      const beat = 60 / bpm;
      for (const [b, note, d, loud = 0.6] of notes) place(out, stringsNote(r, note, d * beat, { loud }), at + b * beat, pan, g);
      return this;
    },
    put(x, at, pan = 0, g = 1) { place(out, x, at, pan, g); return this; },
  };
}
/** A side-drum roll that swells or dies. */
function roll(r, dur, { from = 0.2, to = 1, rate = 25, tau = 0.045 } = {}) {
  const out = mono(dur + 0.4);
  for (let t = 0; t < dur; t += (1 / rate) * r.range(0.9, 1.1)) add(out, snare(r, { loud: lerp(from, to, t / dur) * r.range(0.75, 1), tau }), t, 1);
  return out;
}
/** A kettledrum roll. */
function timpRoll(r, note, dur, { from = 0.25, to = 1, rate = 12 } = {}) {
  const out = mono(dur + 1.6);
  for (let t = 0; t < dur; t += (1 / rate) * r.range(0.92, 1.08)) add(out, timpani(r, note, { loud: lerp(from, to, t / dur) * r.range(0.8, 1) * 0.5, dur: 1.5 }), t, 1);
  return out;
}
/** A drum with a cloth over it: for bad news. */
const muffled = (r, loud = 1) => { const o = lp(bassDrum(r, { loud }), 260); add(o, lp(snare(r, { loud: loud * 0.5, tau: 0.06 }), 700), 0.002, 1); return o; };
/** Bars of a march on the side drum: [beat, loudness] strokes, and a flam for the heavy ones. */
function sideDrum(r, bpm, strokes) {
  const beat = 60 / bpm, end = Math.max(...strokes.map(([b]) => b)) * beat + 0.6, out = mono(end);
  for (const [b, loud = 0.7] of strokes) {
    if (loud > 0.85) add(out, snare(r, { loud: loud * 0.35, tau: 0.04 }), b * beat - 0.022, 1);
    add(out, snare(r, { loud, tau: 0.07 }), b * beat, 1);
  }
  return out;
}
/** A big bell, in a tower: for cities and treaties. */
const tower = (r, f0, loud = 1, dur = 4.5) => gain(bell(r, { f0, dur, tau: 1.9, bright: 0.78, strike: 0.5 }), loud);

export default function (def) {
  // ---- The names the 3D game already calls --------------------------------------------------------------------
  def('bell', { ch: 2, lufs: -23, about: 'One stroke on a deep bell: a decision taken that cannot be taken back (a road of history chosen, a treaty put to the table).' }, (r) => {
    const out = mono(4.2);
    add(out, tower(r, 262, 1, 4), 0.01, 1);
    add(out, mul(sine(sec(3), 131), decay(sec(3), 1.1, 0.01)), 0.01, 0.35); // the hum under it
    return hall(out, -12);
  });
  def('bugle', { ch: 2, lufs: -20, about: 'The campaign begins: the command figure, once, on a single cornet. Our own five notes; a bugle could not play them.' }, (r) => {
    const b = band(5.2);
    b.brass(r, 100, 0.05, [[0, N.F4, 0.5, 0.7], [0.5, N.Bb4, 1.5, 0.85], [2, N.C5, 0.5, 0.8], [2.5, N.A4, 1, 0.8], [3.5, N.D5, 2.6, 0.95]], { players: 1, bright: 1.1 });
    return hall(b.out, -9);
  });
  def('victory', { ch: 2, lufs: -20, about: 'A battle won, a town taken: the last three notes of the command figure, rising, with the band under them. Short, because it is heard often.' }, (r) => {
    const b = band(3.6), bpm = 126, at = 0.3;
    b.put(roll(r, 0.28, { from: 0.3, to: 0.9 }), 0.02, 0.1, 0.5);
    b.brass(r, bpm, at, [[0, N.C5, 0.5, 0.85], [0.5, N.A4, 0.5, 0.8], [1, N.D5, 2.6, 1]], { pan: -0.25, bright: 1.1 });
    b.brass(r, bpm, at, [[0, N.F3, 1, 0.7], [0, N.A3, 1, 0.7], [0, N.C4, 1, 0.7], [1, N.Bb3, 2.6, 0.85], [1, N.D4, 2.6, 0.85], [1, N.F4, 2.6, 0.85]], { pan: 0.3, players: 2, bright: 0.8 });
    b.put(timpani(r, N.F2, { loud: 0.8, dur: 0.9 }), at, 0, 0.8);
    b.put(timpani(r, N.Bb2, { loud: 1 }), at + 60 / bpm, 0, 0.9);
    b.put(snare(r, { loud: 0.9 }), at + 60 / bpm, 0.1, 0.5);
    return hall(b.out);
  });
  def('defeat', { ch: 2, lufs: -21, about: 'A battle lost, a town fallen: the same three notes turned down instead of up, low brass and strings, a muffled drum.' }, (r) => {
    const b = band(4.6), bpm = 72, at = 0.15;
    b.put(muffled(r, 1), at, 0, 0.9);
    b.brass(r, bpm, at, [[0, N.C4, 1, 0.55], [1, N.Ab3, 1, 0.5], [2, N.F3, 2.4, 0.5]], { pan: -0.2, players: 3, bright: 0.55, gate: 1 });
    b.strings(r, bpm, at, [[0, N.F2, 2, 0.6], [0, N.C3, 2, 0.5], [2, N.Bb1, 2.6, 0.65], [2, N.F2, 2.6, 0.55], [2, N.Db3, 2.6, 0.4]], { pan: 0.2 });
    b.put(muffled(r, 0.8), at + 2 * 60 / bpm, 0, 0.9);
    return hall(b.out, -9);
  });
  def('drums', { ch: 2, lufs: -21, about: 'The two sides are drawn up: a roll on the side drum and four strokes, the bass drum under them. Before a battle is joined.' }, (r) => {
    const b = band(3.6), bpm = 112, beat = 60 / bpm;
    b.put(roll(r, 0.9, { from: 0.15, to: 0.85 }), 0.02, -0.1, 0.8);
    b.put(sideDrum(r, bpm, [[0, 1], [0.75, 0.6], [1, 0.9], [1.5, 0.55], [1.75, 0.55], [2, 1]]), 0.95, -0.1, 1);
    for (const k of [0, 1, 2]) b.put(bassDrum(r, { loud: k === 2 ? 1 : 0.75 }), 0.95 + k * beat, 0.15, 0.8);
    return hall(b.out, -13);
  });
  def('newsreel', { ch: 2, lufs: -20, about: 'The band at the head of the newsreel: two bars of our own march, brass and side drum, ending open so that the news can follow.' }, (r) => {
    const b = band(6.2), bpm = 120, at = 0.3, beat = 60 / bpm;
    b.put(roll(r, 0.25, { from: 0.3, to: 0.8 }), 0.03, 0.1, 0.5);
    // The tune: a pick-up, then two bars.
    b.brass(r, bpm, at, [[-0.5, N.F4, 0.5, 0.75], [0, N.Bb4, 1.5, 0.9], [1.5, N.C5, 0.5, 0.8], [2, N.A4, 1, 0.8], [3, N.D5, 1, 0.9],
      [4, N.Eb5, 0.5, 0.85], [4.5, N.D5, 0.5, 0.8], [5, N.C5, 1, 0.85], [6, N.F5, 3, 1]], { pan: -0.3, bright: 1.1 });
    // The second line, a third or a sixth under it.
    b.brass(r, bpm, at, [[0, N.D4, 1.5, 0.7], [1.5, N.Eb4, 0.5, 0.65], [2, N.F4, 1, 0.65], [3, N.F4, 1, 0.7], [4, N.G4, 1, 0.7], [5, N.A4, 1, 0.7], [6, N.A4, 3, 0.8], [6, N.C5, 3, 0.8]], { pan: 0.25, players: 2, bright: 0.85 });
    // The basses walk: the first and the fifth of each chord.
    b.brass(r, bpm, at, [[0, N.Bb2, 0.8, 0.7], [1, N.F2, 0.8, 0.6], [2, N.F2, 0.8, 0.7], [3, N.Bb2, 0.8, 0.6], [4, N.Eb3, 0.8, 0.7], [5, N.F2, 0.8, 0.7], [6, N.F2, 3, 0.8]], { pan: 0.1, players: 2, bright: 0.5 });
    b.put(sideDrum(r, bpm, [[0, 0.9], [1, 0.6], [1.5, 0.5], [2, 0.8], [3, 0.6], [3.5, 0.5], [4, 0.9], [5, 0.6], [5.5, 0.5], [5.75, 0.5], [6, 1]]), at, 0.05, 0.55);
    for (const k of [0, 2, 4, 6]) b.put(bassDrum(r, { loud: 0.8 }), at + k * beat, 0, 0.6);
    b.put(roll(r, 1.2, { from: 0.7, to: 0.2 }), at + 6 * beat, 0.05, 0.4);
    return hall(b.out);
  });

  // ---- The campaign's great moments ---------------------------------------------------------------------------
  def('campaign_start', { ch: 2, lufs: -19, about: 'You take command: a kettledrum roll and the whole command figure from the full band. For the moment a country and a year are chosen.' }, (r) => {
    const b = band(7), bpm = 96, at = 1.0, beat = 60 / bpm;
    b.put(timpRoll(r, N.F2, 0.95, { from: 0.2, to: 1 }), 0.03, 0, 0.9);
    b.brass(r, bpm, at, [[0, N.F4, 0.5, 0.8], [0.5, N.Bb4, 1.5, 0.9], [2, N.C5, 0.5, 0.85], [2.5, N.A4, 1, 0.85], [3.5, N.D5, 3.2, 1]], { pan: -0.3, bright: 1.1 });
    b.brass(r, bpm, at, [[0.5, N.D4, 1.5, 0.75], [0.5, N.F4, 1.5, 0.75], [2, N.Eb4, 0.5, 0.7], [2.5, N.C4, 1, 0.75], [2.5, N.F4, 1, 0.75], [3.5, N.F4, 3.2, 0.9], [3.5, N.Bb4, 3.2, 0.9]], { pan: 0.3, players: 2, bright: 0.85 });
    b.brass(r, bpm, at, [[0.5, N.Bb2, 1.5, 0.8], [2, N.Eb3, 0.5, 0.7], [2.5, N.F2, 1, 0.8], [3.5, N.Bb2, 3.2, 0.9], [3.5, N.Bb1, 3.2, 0.8]], { pan: 0.1, players: 2, bright: 0.5 });
    b.put(timpani(r, N.Bb2, { loud: 1 }), at + 0.5 * beat, 0, 0.9);
    b.put(timpani(r, N.F2, { loud: 0.9 }), at + 2.5 * beat, 0, 0.9);
    b.put(timpani(r, N.Bb2, { loud: 1.1, dur: 2.8 }), at + 3.5 * beat, 0, 1);
    b.put(snare(r, { loud: 1 }), at + 3.5 * beat, 0.1, 0.5);
    return hall(b.out);
  });
  def('war_declared', { ch: 2, lufs: -19, about: 'War is declared: a kettledrum roll that swells, a dark chord from the low brass, and one heavy stroke. For you, on you, or between two others.' }, (r) => {
    const b = band(6.4), at = 1.6;
    b.put(timpRoll(r, N.F2, 1.55, { from: 0.12, to: 1.1 }), 0.03, 0, 1);
    b.put(roll(r, 1.5, { from: 0.1, to: 0.8 }), 0.08, 0.1, 0.45);
    b.brass(r, 60, at, [[0, N.Bb1, 3.6, 0.9], [0, N.F2, 3.6, 0.9], [0, N.Bb2, 3.6, 0.85]], { pan: 0.15, players: 2, bright: 0.6, gate: 1 });
    b.brass(r, 60, at, [[0, N.Db4, 3.6, 0.85], [0, N.F4, 3.6, 0.85], [0, N.Bb4, 1.2, 0.9], [1.2, N.C5, 0.6, 0.85], [1.8, N.Ab4, 1.8, 0.9]], { pan: -0.25, bright: 0.9, gate: 1 });
    b.put(timpani(r, N.Bb2, { loud: 1.3, dur: 3 }), at, 0, 1);
    b.put(bassDrum(r, { loud: 1.2 }), at, 0, 1);
    return hall(b.out, -10);
  });
  def('treaty_signed', { ch: 2, lufs: -22, about: 'A treaty signed, an alliance made, a peace: two warm chords from the brass and a bell. Calm after "war_declared".' }, (r) => {
    const b = band(5.6), bpm = 66, at = 0.1;
    b.brass(r, bpm, at, [[0, N.Eb3, 2, 0.55], [0, N.G3, 2, 0.55], [0, N.Bb3, 2, 0.55], [0, N.Eb4, 2, 0.6], [0, N.G4, 2, 0.6],
      [2, N.Bb2, 3, 0.6], [2, N.F3, 3, 0.55], [2, N.Bb3, 3, 0.55], [2, N.D4, 3, 0.6], [2, N.F4, 3, 0.65]], { players: 2, bright: 0.6, gate: 1.02 });
    b.put(tower(r, 466, 0.5, 3.5), at + 2 * 60 / bpm, 0.2, 0.5);
    return hall(b.out, -9);
  });
  def('city_taken', { ch: 2, lufs: -20, about: 'A great city is yours: three tower bells and the rising figure over them. For a capital or a prize city, not for every town.' }, (r) => {
    const b = band(6.2), bpm = 116, at = 0.45, beat = 60 / bpm;
    [[0, 349, -0.5], [0.42, 466, 0.4], [0.84, 392, -0.1], [1.5, 349, 0.5], [1.92, 466, -0.4], [2.34, 294, 0.1]].forEach(([t, f, pan]) => b.put(tower(r, f, 0.7, 3.6), 0.02 + t, pan, 0.55));
    b.brass(r, bpm, at, [[0, N.F4, 0.5, 0.8], [0.5, N.A4, 0.5, 0.8], [1, N.C5, 0.5, 0.85], [1.5, N.A4, 0.5, 0.8], [2, N.D5, 3, 1]], { pan: -0.25, bright: 1.1 });
    b.brass(r, bpm, at, [[0, N.F3, 2, 0.7], [0, N.C4, 2, 0.7], [2, N.Bb3, 3, 0.85], [2, N.F4, 3, 0.85], [2, N.Bb4, 3, 0.85]], { pan: 0.3, players: 2, bright: 0.8 });
    b.put(timpani(r, N.Bb2, { loud: 1 }), at + 2 * beat, 0, 0.9);
    return hall(b.out);
  });
  def('city_lost', { ch: 2, lufs: -22, about: 'A great city is lost: one bell tolls twice, low strings under it.' }, (r) => {
    const b = band(6);
    b.put(tower(r, 233, 1, 4), 0.02, -0.1, 0.8);
    b.put(tower(r, 233, 0.8, 4), 1.7, -0.1, 0.8);
    b.strings(r, 60, 0.1, [[0, N.Bb1, 4, 0.55], [0, N.F2, 4, 0.5], [1.7, N.Db3, 2.6, 0.45]], { pan: 0.2 });
    return hall(b.out, -9);
  });
  def('nation_falls', { ch: 2, lufs: -20, about: 'A country is beaten and leaves the war: the command figure broken off and falling, muffled drums, a last low note.' }, (r) => {
    const b = band(7.8), bpm = 63, at = 0.15, beat = 60 / bpm;
    b.brass(r, bpm, at, [[0, N.F4, 0.5, 0.6], [0.5, N.Bb4, 1.5, 0.65], [2, N.C5, 0.5, 0.6], [2.5, N.Ab4, 1.5, 0.6], [4, N.F4, 3, 0.5]], { pan: -0.2, players: 2, bright: 0.6, gate: 1 });
    b.strings(r, bpm, at, [[0.5, N.Bb2, 2, 0.6], [0.5, N.Db3, 2, 0.5], [2.5, N.F2, 1.5, 0.6], [2.5, N.C3, 1.5, 0.5], [4, N.Bb1, 3.2, 0.7], [4, N.F2, 3.2, 0.55]], { pan: 0.2 });
    for (const k of [0.5, 2.5, 4]) b.put(muffled(r, k === 4 ? 1 : 0.75), at + k * beat, 0, 0.9);
    return hall(b.out, -9);
  });
  def('nation_freed', { ch: 2, lufs: -20, about: 'A country is set free and comes back into the war: bells from every tower and the figure whole again.' }, (r) => {
    const b = band(7.4), bpm = 108, at = 1.1, beat = 60 / bpm;
    for (let k = 0; k < 11; k++) b.put(tower(r, [349, 466, 392, 523, 294][k % 5] * (k > 4 ? 1 : 1), r.range(0.45, 0.7), 3), 0.02 + k * 0.37 + r.range(-0.03, 0.03), r.range(-0.8, 0.8), 0.5);
    b.brass(r, bpm, at, [[0, N.F4, 0.5, 0.8], [0.5, N.Bb4, 1.5, 0.9], [2, N.C5, 0.5, 0.85], [2.5, N.A4, 1, 0.85], [3.5, N.D5, 1.5, 0.95], [5, N.F5, 3, 1]], { pan: -0.3, bright: 1.1 });
    b.brass(r, bpm, at, [[0.5, N.D4, 1.5, 0.75], [0.5, N.F4, 1.5, 0.75], [2.5, N.C4, 1, 0.7], [2.5, N.F4, 1, 0.7], [3.5, N.Bb3, 1.5, 0.8], [3.5, N.F4, 1.5, 0.8], [5, N.Bb3, 3, 0.85], [5, N.D4, 3, 0.85], [5, N.Bb4, 3, 0.85]], { pan: 0.3, players: 2, bright: 0.85 });
    b.put(timpani(r, N.Bb2, { loud: 1 }), at + 0.5 * beat, 0, 0.9);
    b.put(timpani(r, N.Bb2, { loud: 1.1, dur: 2.6 }), at + 5 * beat, 0, 1);
    return hall(b.out);
  });
  def('world_won', { ch: 2, lufs: -19, about: 'The campaign is won: the whole of our march, four bars, drums and all. Heard once, at the very end.' }, (r) => {
    const b = band(11.6), bpm = 116, at = 1.15, beat = 60 / bpm;
    b.put(timpRoll(r, N.F2, 1.1, { from: 0.2, to: 1 }), 0.03, 0, 0.9);
    b.put(roll(r, 1.05, { from: 0.15, to: 0.9 }), 0.06, 0.1, 0.5);
    b.brass(r, bpm, at, [[-0.5, N.F4, 0.5, 0.8], [0, N.Bb4, 1.5, 0.9], [1.5, N.C5, 0.5, 0.85], [2, N.A4, 1, 0.85], [3, N.D5, 1, 0.9],
      [4, N.Eb5, 0.5, 0.85], [4.5, N.D5, 0.5, 0.85], [5, N.C5, 1, 0.85], [6, N.F5, 2, 0.95],
      [8, N.G5, 1.5, 1], [9.5, N.F5, 0.5, 0.9], [10, N.D5, 1, 0.9], [11, N.Bb4, 1, 0.85],
      [12, N.C5, 1, 0.9], [13, N.A4, 0.5, 0.85], [13.5, N.C5, 0.5, 0.9], [14, N.Bb4, 4.2, 1]], { pan: -0.3, bright: 1.15 });
    b.brass(r, bpm, at, [[0, N.D4, 1.5, 0.7], [1.5, N.Eb4, 0.5, 0.65], [2, N.F4, 1, 0.7], [3, N.F4, 1, 0.7], [4, N.G4, 1, 0.7], [5, N.A4, 1, 0.7], [6, N.A4, 2, 0.8],
      [8, N.Bb4, 1.5, 0.8], [9.5, N.A4, 0.5, 0.75], [10, N.F4, 1, 0.75], [11, N.D4, 1, 0.75], [12, N.Eb4, 1, 0.75], [13, N.Eb4, 1, 0.75], [14, N.D4, 4.2, 0.9], [14, N.F4, 4.2, 0.9]], { pan: 0.3, players: 2, bright: 0.85 });
    b.brass(r, bpm, at, [[0, N.Bb2, 0.8, 0.7], [1, N.F2, 0.8, 0.6], [2, N.F2, 0.8, 0.7], [3, N.Bb2, 0.8, 0.6], [4, N.Eb3, 0.8, 0.7], [5, N.F2, 0.8, 0.7], [6, N.F2, 0.8, 0.75], [7, N.C3, 0.8, 0.65],
      [8, N.Eb3, 0.8, 0.75], [9, N.Bb2, 0.8, 0.65], [10, N.Bb2, 0.8, 0.7], [11, N.G2, 0.8, 0.65], [12, N.Eb3, 0.8, 0.7], [13, N.F2, 0.8, 0.75], [14, N.Bb2, 4.2, 0.9], [14, N.Bb1, 4.2, 0.85]], { pan: 0.1, players: 2, bright: 0.5 });
    const strokes = [];
    for (let bar = 0; bar < 3.5; bar++) strokes.push([bar * 4, 0.9], [bar * 4 + 1, 0.6], [bar * 4 + 1.5, 0.5], [bar * 4 + 2, 0.8], [bar * 4 + 3, 0.6], [bar * 4 + 3.5, 0.5], [bar * 4 + 3.75, 0.5]);
    b.put(sideDrum(r, bpm, strokes.filter(([x]) => x < 14).concat([[14, 1]])), at, 0.05, 0.5);
    for (let k = 0; k <= 14; k += 2) b.put(bassDrum(r, { loud: k === 14 ? 1.1 : 0.8 }), at + k * beat, 0, 0.6);
    b.put(timpani(r, N.Bb2, { loud: 1.2, dur: 3 }), at + 14 * beat, 0, 1);
    b.put(roll(r, 1.6, { from: 0.8, to: 0.2 }), at + 14 * beat, 0.05, 0.4);
    return hall(b.out);
  });
  def('campaign_lost', { ch: 2, lufs: -20, about: 'The campaign is lost: the figure once more, slow and in the dark, the drum with a cloth on it, and silence after.' }, (r) => {
    const b = band(11), bpm = 54, at = 0.2, beat = 60 / bpm;
    b.brass(r, bpm, at, [[0, N.F3, 0.5, 0.55], [0.5, N.Bb3, 1.5, 0.6], [2, N.C4, 0.5, 0.55], [2.5, N.Ab3, 1.5, 0.55], [4, N.Db4, 1.5, 0.6], [5.5, N.C4, 0.5, 0.5], [6, N.Bb3, 3, 0.5]], { pan: -0.2, players: 2, bright: 0.5, gate: 1 });
    b.strings(r, bpm, at, [[0.5, N.Bb1, 2, 0.6], [0.5, N.F2, 2, 0.5], [2.5, N.F2, 1.5, 0.55], [2.5, N.C3, 1.5, 0.45], [4, N.G2, 2, 0.55], [4, N.Eb3, 2, 0.45], [6, N.Bb1, 3.4, 0.7], [6, N.F2, 3.4, 0.55], [6, N.Db3, 3.4, 0.4]], { pan: 0.2 });
    for (const k of [0.5, 2.5, 4, 6]) b.put(muffled(r, k === 6 ? 1 : 0.7), at + k * beat, 0, 0.9);
    return hall(b.out, -9);
  });

  // ---- The month's report --------------------------------------------------------------------------------------
  def('month_card_good', { ch: 2, lufs: -23, about: 'The month went your way: two soft rising chords, a light touch of the bell. Under the month card when the news is good.' }, (r) => {
    const b = band(3.4), bpm = 92, at = 0.05;
    b.brass(r, bpm, at, [[0, N.F3, 1, 0.5], [0, N.A3, 1, 0.5], [0, N.C4, 1, 0.5], [0, N.A4, 1, 0.55], [1, N.Bb3, 2.4, 0.55], [1, N.D4, 2.4, 0.55], [1, N.F4, 2.4, 0.55], [1, N.D5, 2.4, 0.6]], { players: 2, bright: 0.7, gate: 1 });
    b.put(bell(r, { f0: 1175, dur: 2, tau: 0.7, bright: 0.55, strike: 0.3 }), at + 60 / bpm, 0.3, 0.16);
    return hall(b.out, -10);
  });
  def('month_card_bad', { ch: 2, lufs: -23, about: 'The month went against you: two chords that sink, low strings, a muffled stroke. Under the month card when the news is bad.' }, (r) => {
    const b = band(3.8), bpm = 76, at = 0.05;
    b.brass(r, bpm, at, [[0, N.C4, 1, 0.45], [0, N.F3, 1, 0.45], [1, N.Ab3, 2.2, 0.45], [1, N.Db4, 2.2, 0.4]], { pan: -0.15, players: 2, bright: 0.5, gate: 1 });
    b.strings(r, bpm, at, [[0, N.F2, 1, 0.55], [1, N.Bb1, 2.4, 0.6], [1, N.F2, 2.4, 0.5]], { pan: 0.2 });
    b.put(muffled(r, 0.8), at + 60 / bpm, 0, 0.8);
    return hall(b.out, -9);
  });
  def('month_card_quiet', { ch: 2, lufs: -25, about: 'A month in which nothing was settled: one held chord, neither up nor down. Under the month card when the news is mixed or thin.' }, (r) => {
    const b = band(3.2);
    b.brass(r, 60, 0.05, [[0, N.Bb2, 2.2, 0.45], [0, N.F3, 2.2, 0.45], [0, N.C4, 2.2, 0.45], [0, N.F4, 2.2, 0.45]], { players: 2, bright: 0.5, gate: 1 });
    return hall(b.out, -9);
  });
  def('month_yours', { ch: 2, lufs: -24, about: 'The others have moved and the month is yours: two notes, like a hand on the shoulder. Never loud.' }, (r) => {
    const b = band(2.4);
    b.brass(r, 108, 0.03, [[0, N.A4, 0.5, 0.6], [0.5, N.D5, 1.6, 0.7]], { players: 1, bright: 0.9 });
    b.brass(r, 108, 0.03, [[0.5, N.Bb3, 1.6, 0.5], [0.5, N.F4, 1.6, 0.5]], { players: 2, bright: 0.55, pan: 0.2 });
    return hall(b.out, -10);
  });

  // ---- Smaller moments -----------------------------------------------------------------------------------------
  def('alert', { ch: 2, lufs: -21, about: 'Something needs you now (a landing on your shore, an attack on you, a government in danger): two quick notes and a drum tap.' }, (r) => {
    const b = band(2);
    b.brass(r, 138, 0.03, [[0, N.D5, 0.4, 0.9], [0.5, N.D5, 0.4, 0.85], [1, N.A4, 1.2, 0.9]], { players: 2, bright: 1.1, gate: 0.8 });
    b.put(sideDrum(r, 138, [[0, 0.8], [0.5, 0.7], [1, 1]]), 0.03, 0.1, 0.5);
    return hall(b.out, -13);
  });
  def('research_done', { ch: 2, lufs: -24, about: 'The workshops have something new: three small bells climbing, like a good idea arriving.' }, (r) => {
    const out = stereo(2.6);
    [[0, 1175, -0.3], [0.13, 1397, 0], [0.26, 1865, 0.3]].forEach(([t, f, pan]) => place(out, bell(r, { f0: f, dur: 1.8, tau: 0.55, bright: 0.55, strike: 0.35 }), 0.01 + t, pan, 0.6));
    return inPlace(out, 'room', -14);
  });
  def('medal', { ch: 2, lufs: -22, about: 'A medal pinned on, a general promoted: a ruffle on the drum and two bright notes.' }, (r) => {
    const b = band(3);
    b.put(roll(r, 0.45, { from: 0.3, to: 0.9 }), 0.02, 0.1, 0.55);
    b.put(snare(r, { loud: 1 }), 0.5, 0.1, 0.55);
    b.brass(r, 120, 0.5, [[0, N.Bb4, 0.5, 0.8], [0.5, N.D5, 1.8, 0.9]], { pan: -0.2, players: 2, bright: 1.05 });
    b.brass(r, 120, 0.5, [[0, N.F4, 0.5, 0.65], [0.5, N.F4, 1.8, 0.7], [0.5, N.Bb3, 1.8, 0.7]], { pan: 0.25, players: 2, bright: 0.8 });
    return hall(b.out, -11);
  });
  def('drum_low', { ch: 2, lufs: -23, about: 'A red band crosses the screen (war, a country that may fall): one low drum stroke in the hall. When the moment has its own call ("war_declared", "nation_falls"), play the call and not this.' }, (r) => {
    const b = band(3);
    b.put(bassDrum(r, { loud: 1.1 }), 0.01, 0, 1);
    b.put(timpani(r, N.F2, { loud: 0.6, dur: 2.2 }), 0.01, 0, 0.7);
    return hall(b.out, -9);
  });
  def('unit_lost', { ch: 2, lufs: -23, about: 'A division is destroyed, a ship goes down with her name: one muffled stroke and a low note falling.' }, (r) => {
    const b = band(3.4);
    b.put(muffled(r, 1), 0.02, 0, 0.9);
    b.brass(r, 60, 0.05, [[0, N.Db3, 0.9, 0.5], [0.9, N.Bb2, 1.8, 0.45]], { players: 2, bright: 0.45, gate: 1.05 });
    return hall(b.out, -9);
  });

  // ---- First Command -------------------------------------------------------------------------------------------
  def('note_closing', { ch: 2, lufs: -22, about: 'The last staff note of the first hour ("the command is yours"): the command figure, gently, with a bell on its last note. Heard once in a player\'s life.' }, (r) => {
    const b = band(6.2), bpm = 88, at = 0.05, beat = 60 / bpm;
    b.brass(r, bpm, at, [[0, N.F4, 0.5, 0.55], [0.5, N.Bb4, 1.5, 0.65], [2, N.C5, 0.5, 0.6], [2.5, N.A4, 1, 0.6], [3.5, N.D5, 3.2, 0.7]], { pan: -0.2, players: 1, bright: 0.9 });
    b.brass(r, bpm, at, [[0.5, N.Bb2, 1.5, 0.45], [0.5, N.F3, 1.5, 0.45], [0.5, N.D4, 1.5, 0.45], [2.5, N.F2, 1, 0.45], [2.5, N.C4, 1, 0.45], [3.5, N.Bb2, 3.2, 0.5], [3.5, N.F3, 3.2, 0.5], [3.5, N.Bb3, 3.2, 0.5], [3.5, N.F4, 3.2, 0.5]], { pan: 0.2, players: 2, bright: 0.55, gate: 1 });
    b.put(bell(r, { f0: 1175, dur: 2.6, tau: 0.9, bright: 0.55, strike: 0.3 }), at + 3.5 * beat, 0.3, 0.14);
    return hall(b.out, -10);
  });
}
