// Writes the campaign's music: an original piece, composed here as notes and played by a small
// synthesiser, so nothing is borrowed from any recording, film or game.
//
// "Leave Pass": an up-tempo swing number for a dance band of the 1940s, in B flat, about two and a
// half minutes, made to loop without a seam. A four-bar vamp from the rhythm section, then three
// choruses of thirty-two bars: the saxophones state the tune with the brass punching between their
// phrases; a clarinet takes a chorus over a quiet riff; and the whole band shouts the tune out.
//
// The tune and its arrangement are written for this game. The chords under it are the common
// property of the style (a turnaround in the home key, and a bridge that walks round the cycle of
// dominants), as a twelve-bar blues is.
//
//   node Tools/make_music.mjs <out.wav>
//
// It is a synthesiser, not a band: it sets the mood, and can be replaced by recorded music later
// without anything else changing. The slower piece it replaced is kept in make_music_map_room.mjs.

import fs from 'node:fs';

const SR = 44100;
const BPM = 166;
const BEAT = 60 / BPM;
const BAR = 4 * BEAT;
const SWING = 0.64; // where the second quaver of a beat falls: 0.5 is straight, 0.667 a full triplet
const INTRO = 4;
const CHORUS = 32;
const BARS = INTRO + 3 * CHORUS;
const TAIL = 2.5; // seconds of echo after the last bar, folded back onto the beginning so the loop has no seam
const N = Math.round(BARS * BAR * SR);
const NT = N + Math.round(TAIL * SR);

const dryL = new Float32Array(NT);
const dryR = new Float32Array(NT);

// ---- Notes --------------------------------------------------------------------------------

const STEP = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
function midi(name) {
  const m = /^([A-G])([#b]?)(-?\d)$/.exec(name);
  return 12 * (Number(m[3]) + 1) + STEP[m[1]] + (m[2] === '#' ? 1 : m[2] === 'b' ? -1 : 0);
}
const hz = (m) => 440 * 2 ** ((m - 69) / 12);

// A sawtooth with its corners rounded off (polyBLEP), so high notes do not hiss.
function saw(phase, dt) {
  let v = 2 * phase - 1;
  if (phase < dt) { const t = phase / dt; v -= t + t - t * t - 1; }
  else if (phase > 1 - dt) { const t = (phase - 1) / dt; v -= t * t + t + t + 1; }
  return v;
}

let seed = 1942;
function rnd() { // the same numbers every time, so the piece is the same every time it is made
  seed = (seed * 1664525 + 1013904223) >>> 0;
  return seed / 4294967296;
}
const noise = () => rnd() * 2 - 1;

function add(i, v, pan) {
  if (i < 0 || i >= NT) return;
  dryL[i] += v * Math.cos((pan + 1) * Math.PI / 4);
  dryR[i] += v * Math.sin((pan + 1) * Math.PI / 4);
}

const smooth = (t) => (t <= 0 ? 0 : t >= 1 ? 1 : t * t * (3 - 2 * t));

/** When a beat of a bar sounds. Quavers off the beat are played late: that is the swing. */
function at(bar, beat = 0) {
  const whole = Math.floor(beat + 1e-9), part = beat - whole;
  const swung = Math.abs(part - 0.5) < 1e-6 ? SWING : part;
  return (bar * 4 + whole + swung) * BEAT;
}
/** How long a note lasts that starts on a beat and is so many beats long, allowing for the swing at both ends. */
const span = (bar, beat, beats) => at(bar, beat + beats) - at(bar, beat);

// ---- Instruments --------------------------------------------------------------------------

/**
 * A reed or a brass voice: a saw through a filter that opens as the note speaks. `o.bright` sets how far it
 * opens, `o.reed` mixes in the hollow sound of a reed, `o.scoop` slides up into the note, `o.fall` lets it drop
 * away at the end, as a brass section does off a short note.
 */
function wind(start, dur, m, vol, pan, o = {}) {
  const f = hz(m);
  const attack = o.attack ?? 0.022, release = o.release ?? 0.07, bright = o.bright ?? 1, reed = o.reed ?? 0;
  const scoop = o.scoop ?? 0.012, fall = o.fall ?? 0;
  const i0 = Math.round(start * SR), len = Math.round((dur + release) * SR);
  let phase = (m * 0.137) % 1, a = 0, b = 0;
  for (let i = 0; i < len; i++) {
    const t = i / SR;
    const held = t < dur;
    const env = smooth(t / attack) * (held ? 1 - 0.12 * smooth(t / Math.max(dur, 0.05)) : 0.88 * (1 - smooth((t - dur) / release)));
    const vib = 1 + 0.0045 * smooth((t - 0.16) / 0.25) * Math.sin(2 * Math.PI * 5.6 * t + m);
    const slide = 1 - scoop * (1 - smooth(t / 0.05)) - (held ? 0 : fall * smooth((t - dur) / release));
    const dt = f * vib * slide / SR;
    phase += dt; if (phase >= 1) phase -= 1;
    let half = phase + 0.5; if (half >= 1) half -= 1;
    const s = saw(phase, dt) - reed * saw(half, dt) + 0.35 * Math.sin(2 * Math.PI * phase);
    const cut = Math.min(0.7, (500 + (1500 + 3800 * bright) * env * env + f * 1.2) / SR * 2 * Math.PI);
    a += cut * (s - a); b += cut * (a - b);
    add(i0 + i, b * env * vol, pan);
  }
}

/** A string bass, plucked: a round low note with a little thump at the front. */
function bass(start, m, vol, len = 0.36) {
  const f = hz(m);
  const i0 = Math.round(start * SR), n = Math.round((len + 0.08) * SR);
  let phase = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const env = smooth(t / 0.006) * Math.exp(-t / (len * 0.62)) * (t < len ? 1 : 1 - smooth((t - len) / 0.08));
    phase += f * (1 + 0.03 * Math.exp(-t / 0.012)) / SR;
    const tone = 0.8 * Math.sin(2 * Math.PI * phase) + 0.62 * Math.sin(4 * Math.PI * phase) * Math.exp(-t / 0.24) + 0.3 * Math.sin(6 * Math.PI * phase) * Math.exp(-t / 0.12);
    add(i0 + i, tone * env * vol, 0.05);
  }
}

/** A piano note: struck, bright for a moment, and dying away. */
function piano(start, m, vol, pan, len = 0.3) {
  const f = hz(m);
  const i0 = Math.round(start * SR), n = Math.round((len + 0.12) * SR);
  let p1 = 0, p2 = 0.3, a = 0, b = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const env = smooth(t / 0.004) * Math.exp(-t / 0.28) * (t < len ? 1 : 1 - smooth((t - len) / 0.12));
    const d1 = f / SR, d2 = f * 1.0025 / SR;
    p1 += d1; if (p1 >= 1) p1 -= 1;
    p2 += d2; if (p2 >= 1) p2 -= 1;
    const s = 0.5 * (saw(p1, d1) + saw(p2, d2)) + 0.5 * Math.sin(2 * Math.PI * p1);
    const cut = Math.min(0.7, (700 + 5200 * Math.exp(-t / 0.07) + f) / SR * 2 * Math.PI);
    a += cut * (s - a); b += cut * (a - b);
    add(i0 + i, b * env * vol, pan);
  }
}

/** The rhythm guitar: a chord struck short and dry on every beat. */
function chunk(start, notes, vol) {
  notes.forEach((m, k) => {
    const f = hz(m);
    const i0 = Math.round((start + k * 0.004) * SR), n = Math.round(0.11 * SR);
    let p = 0, a = 0;
    for (let i = 0; i < n; i++) {
      const t = i / SR;
      const env = smooth(t / 0.003) * Math.exp(-t / 0.032);
      const d = f / SR; p += d; if (p >= 1) p -= 1;
      a += 0.35 * (saw(p, d) - a);
      add(i0 + i, a * env * vol, -0.35);
    }
  });
}

/** The ride cymbal: a bright ping that rings on. */
function ride(start, vol) {
  const i0 = Math.round(start * SR), n = Math.round(0.42 * SR);
  let lo = 0, last = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const w = noise();
    lo += 0.45 * (w - lo);
    const hiss = (w - lo) * 0.6 + (w - last) * 0.25; last = w; // only the top of the noise
    const ping = Math.sin(2 * Math.PI * 5230 * t) + 0.7 * Math.sin(2 * Math.PI * 7410 * t + 1) + 0.5 * Math.sin(2 * Math.PI * 3940 * t + 2);
    add(i0 + i, (hiss * Math.exp(-t / 0.16) + 0.22 * ping * Math.exp(-t / 0.05)) * smooth(t / 0.002) * vol, 0.32);
  }
}

/** The hi-hat closing on the off beats. */
function hat(start, vol) {
  const i0 = Math.round(start * SR), n = Math.round(0.06 * SR);
  let lo = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const w = noise();
    lo += 0.5 * (w - lo);
    add(i0 + i, (w - lo) * Math.exp(-t / 0.014) * vol, -0.3);
  }
}

/** The bass drum, played softly on every beat, and harder under the band's accents. */
function kick(start, vol) {
  const i0 = Math.round(start * SR), n = Math.round(0.22 * SR);
  let phase = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    phase += (52 + 60 * Math.exp(-t / 0.02)) / SR;
    add(i0 + i, Math.sin(2 * Math.PI * phase) * smooth(t / 0.003) * Math.exp(-t / 0.07) * vol, 0);
  }
}

/** The snare drum. */
function snare(start, vol) {
  const i0 = Math.round(start * SR), n = Math.round(0.2 * SR);
  let lo = 0, hi = 0;
  for (let i = 0; i < n; i++) {
    const t = i / SR;
    const w = noise();
    lo += 0.6 * (w - lo); hi += 0.12 * (w - hi);
    const body = Math.sin(2 * Math.PI * 196 * t) * Math.exp(-t / 0.03);
    add(i0 + i, ((lo - hi) * Math.exp(-t / 0.055) + 0.4 * body) * vol, 0.12);
  }
}

// ---- The chords ---------------------------------------------------------------------------

// Each chord: its root (for the bass), and the notes in it, as steps above C.
const CHORD = {
  Bb6: { root: 'Bb1', tones: [10, 2, 5, 7], scale: [10, 0, 2, 5, 7] },
  Bb7: { root: 'Bb1', tones: [10, 2, 5, 8], scale: [10, 0, 2, 3, 5, 7, 8] },
  Eb6: { root: 'Eb2', tones: [3, 7, 10, 0], scale: [3, 5, 7, 10, 0] },
  Ebm6: { root: 'Eb2', tones: [3, 6, 10, 0], scale: [3, 5, 6, 10, 0] },
  G7: { root: 'G1', tones: [7, 11, 2, 5], scale: [7, 9, 11, 0, 2, 4, 5] },
  Cm7: { root: 'C2', tones: [0, 3, 7, 10], scale: [0, 2, 3, 5, 7, 9, 10] },
  F7: { root: 'F1', tones: [5, 9, 0, 3], scale: [5, 7, 9, 10, 0, 2, 3] },
  D7: { root: 'D2', tones: [2, 6, 9, 0], scale: [2, 4, 6, 7, 9, 11, 0] },
  C7: { root: 'C2', tones: [0, 4, 7, 10], scale: [0, 2, 4, 5, 7, 9, 10] },
};

// Eight bars of the tune's first strain, with its two endings, and the eight bars of the bridge.
const HEAD = [['Bb6'], ['G7'], ['Cm7'], ['F7'], ['Bb6', 'Bb7'], ['Eb6', 'Ebm6']];
const A_TURN = [...HEAD, ['Bb6', 'G7'], ['Cm7', 'F7']];
const A_HOME = [...HEAD, ['Bb6', 'F7'], ['Bb6']];
const A_LAST = [...HEAD, ['Bb6', 'F7'], ['Bb6', 'F7']];
const BRIDGE = [['D7'], ['D7'], ['G7'], ['G7'], ['C7'], ['C7'], ['F7'], ['F7']];
const ONE_CHORUS = [...A_TURN, ...A_HOME, ...BRIDGE, ...A_LAST];
const CHART = [['Bb6'], ['G7'], ['Cm7'], ['F7'], ...ONE_CHORUS, ...ONE_CHORUS, ...ONE_CHORUS];

/** The chord sounding at a beat of a bar. */
function chordAt(bar, beat) {
  const row = CHART[((bar % BARS) + BARS) % BARS];
  return CHORD[row[row.length === 2 && beat >= 2 ? 1 : 0]];
}

/** The nearest note of a chord (or of its scale) at or below a pitch, or strictly below it. */
function below(m, set, strictly) {
  for (let p = strictly ? m - 1 : m; p > m - 13; p--) if (set.includes(((p % 12) + 12) % 12)) return p;
  return m;
}
function above(m, set, strictly) {
  for (let p = strictly ? m + 1 : m; p < m + 13; p++) if (set.includes(((p % 12) + 12) % 12)) return p;
  return m;
}

// ---- The rhythm section, all the way through ---------------------------------------------------

const loud = (bar) => (bar < INTRO ? 0.8 : bar < INTRO + CHORUS ? 0.9 : bar < INTRO + 2 * CHORUS ? 0.78 : 1.12);

// The bass walks: the root on the first beat, notes of the chord after it, and a step into the next chord.
let lastBass = midi('Bb1');
for (let bar = 0; bar < BARS; bar++) {
  for (let beat = 0; beat < 4; beat++) {
    const chord = chordAt(bar, beat);
    const root = midi(chord.root);
    const next = chordAt(beat === 3 ? bar + 1 : bar, beat === 3 ? 0 : beat + 1);
    const changing = next !== chord || beat === 3;
    let note;
    if (beat === 0 || (beat === 2 && chordAt(bar, 0) !== chord)) note = root;
    else if (changing) {
      // Lean into the next chord's root from a semitone above or below, whichever is nearer.
      const target = midi(next.root);
      note = lastBass >= target ? target + 1 : target - 1;
      if (Math.abs(note - lastBass) > 7) note += note < lastBass ? 12 : -12;
    } else {
      // Step to the next note of the chord, up or down, turning round at the ends of the instrument.
      const up = lastBass < midi('E2') ? true : lastBass > midi('C3') ? false : rnd() < 0.6;
      note = up ? above(lastBass, chord.tones, true) : below(lastBass, chord.tones, true);
    }
    while (note < midi('E1')) note += 12;
    while (note > midi('F3')) note -= 12;
    bass(at(bar, beat), note, (beat % 2 === 0 ? 0.33 : 0.29) * loud(bar), BEAT * 0.92);
    lastBass = note;
  }
}

// The drums: the ride cymbal's "ding, ding-a ding", the hi-hat on two and four, a soft bass drum under it all.
for (let bar = 0; bar < BARS; bar++) {
  const level = loud(bar);
  const shout = bar >= INTRO + 2 * CHORUS;
  for (let beat = 0; beat < 4; beat++) {
    ride(at(bar, beat), (beat % 2 ? 0.19 : 0.145) * level);
    if (beat % 2) { ride(at(bar, beat + 0.5), 0.09 * level); hat(at(bar, beat), 0.18 * level); }
    kick(at(bar, beat), (shout ? 0.17 : 0.11) * level);
    if (shout && beat % 2) snare(at(bar, beat), 0.11);
  }
  // A light tap now and then, and a fill to turn each eight bars round.
  if (bar % 2 === 1 && !shout) snare(at(bar, 3.5), 0.035 * level);
  const inChorus = bar - INTRO;
  if (bar === INTRO - 1 || (inChorus >= 0 && inChorus % 8 === 7)) {
    [[2, 0.07], [2.5, 0.05], [3, 0.09], [3.33, 0.06], [3.67, 0.11]].forEach(([beat, hit]) => snare(at(bar) + beat * BEAT, hit * level * (shout ? 1.5 : 1)));
    kick(at(bar + 1, 0), 0.22 * level);
  }
}

// Guitar on every beat and the piano's short chords between the band's phrases.
for (let bar = 0; bar < BARS; bar++) {
  const level = loud(bar);
  for (let beat = 0; beat < 4; beat++) {
    const chord = chordAt(bar, beat);
    const top = below(midi('G4'), chord.tones, false);
    const voicing = [top, below(top, chord.tones, true)];
    voicing.push(below(voicing[1], chord.tones, true), below(voicing[1] - 3, chord.tones, true));
    chunk(at(bar, beat), voicing, (beat % 2 ? 0.1 : 0.075) * level);
  }
  // The pianist's left hand is the bass player's business; the right hand answers on the off beats.
  const pattern = bar % 4 === 0 ? [0, 1.5] : bar % 4 === 1 ? [2.5] : bar % 4 === 2 ? [0.5, 2] : [1.5, 3.5];
  for (const beat of pattern) {
    const chord = chordAt(bar, beat);
    const top = below(midi('D5'), chord.tones, false);
    [top, below(top, chord.tones, true), below(top - 5, chord.tones, true)].forEach((m, k) => piano(at(bar, beat), m, 0.12 * level, 0.25 - k * 0.05, 0.16));
  }
}

// ---- The tune -----------------------------------------------------------------------------

// Each note: [bar within the eight, beat, length in beats, pitch]. Quavers off the beat swing.
const TUNE_HEAD = [
  [0, 0.5, 0.5, 'F4'], [0, 1, 0.5, 'G4'], [0, 1.5, 0.5, 'Bb4'], [0, 2, 1.5, 'D5'], [0, 3.5, 0.5, 'Bb4'],
  [1, 0, 1, 'B4'], [1, 1.5, 0.5, 'D5'], [1, 2, 1, 'F5'], [1, 3, 0.5, 'D5'], [1, 3.5, 0.5, 'B4'],
  [2, 0, 1.5, 'C5'], [2, 1.5, 0.5, 'Eb5'], [2, 2, 1, 'G5'], [2, 3, 1, 'Eb5'],
  [3, 0, 0.5, 'F5'], [3, 0.5, 0.5, 'Eb5'], [3, 1, 0.5, 'C5'], [3, 1.5, 0.5, 'A4'], [3, 2, 1.5, 'F4'],
  [4, 0.5, 0.5, 'F4'], [4, 1, 0.5, 'G4'], [4, 1.5, 0.5, 'Bb4'], [4, 2, 1, 'D5'], [4, 3, 1, 'Ab4'],
  [5, 0, 1.5, 'G4'], [5, 1.5, 0.5, 'Bb4'], [5, 2, 1.5, 'Gb4'], [5, 3.5, 0.5, 'Eb4'],
];
const TUNE_TURN = [
  [6, 0, 1, 'F4'], [6, 1, 0.5, 'D4'], [6, 1.5, 0.5, 'F4'], [6, 2, 1, 'G4'], [6, 3, 1, 'B4'],
  [7, 0, 1, 'C5'], [7, 1, 0.5, 'Bb4'], [7, 1.5, 0.5, 'G4'], [7, 2, 1.5, 'A4'],
];
const TUNE_HOME = [
  [6, 0, 1, 'F4'], [6, 1, 0.5, 'D4'], [6, 1.5, 0.5, 'F4'], [6, 2, 1, 'Bb4'], [6, 3, 0.5, 'A4'], [6, 3.5, 2.5, 'Bb4'],
];
const TUNE_BRIDGE = [
  [0, 0, 0.5, 'F#5'], [0, 1.5, 1.5, 'A5'], [0, 3.5, 0.5, 'F#5'],
  [1, 0, 1, 'D5'], [1, 1.5, 0.5, 'C5'], [1, 2, 1.5, 'A4'],
  [2, 0, 0.5, 'B4'], [2, 1.5, 1.5, 'D5'], [2, 3.5, 0.5, 'B4'],
  [3, 0, 1, 'G4'], [3, 1.5, 0.5, 'F5'], [3, 2, 1.5, 'D5'],
  [4, 0, 0.5, 'E5'], [4, 1.5, 1.5, 'G5'], [4, 3.5, 0.5, 'E5'],
  [5, 0, 1, 'C5'], [5, 1.5, 0.5, 'Bb4'], [5, 2, 1.5, 'G4'],
  [6, 0, 0.5, 'A4'], [6, 1, 0.5, 'C5'], [6, 1.5, 0.5, 'Eb5'], [6, 2, 1, 'F5'], [6, 3, 0.5, 'Eb5'], [6, 3.5, 0.5, 'C5'],
  [7, 0, 1, 'A4'], [7, 1, 0.5, 'F4'], [7, 1.5, 0.5, 'Eb4'], [7, 2, 1.5, 'F4'],
];

/** How long to hold a written note: short ones are clipped, as a section tongues them, long ones nearly full. */
const held = (bar, beat, beats) => span(bar, beat, beats) * (beats <= 0.5 ? 0.78 : 0.9);

/**
 * A section plays a line in close harmony: the tune on top, three notes of the chord under it, and the lowest
 * voice doubling the tune an octave down. `shift` moves it by octaves; `voice` says what the players sound like.
 */
function section(first, line, shift, vol, pans, voice) {
  for (const [bar, beat, beats, name] of line) {
    const lead = midi(name) + 12 * shift;
    const chord = chordAt(first + bar, beat);
    const second = below(lead, chord.tones, true), third = below(second, chord.tones, true), fourth = below(third, chord.tones, true);
    const dur = held(first + bar, beat, beats);
    const when = at(first + bar, beat) + 0.008;
    const accent = Math.abs((beat % 1) - 0.5) < 1e-6 ? 1.12 : 1; // the off-beat notes are leaned on
    [lead, second, third, fourth, lead - 12].forEach((m, k) => wind(when + k * 0.003, dur, m, vol * accent * (k === 0 ? 1.25 : k === 4 ? 0.9 : 0.8), pans[k], voice));
  }
}

/** The brass punch a chord: short, bright, falling away. */
function punch(bar, beat, vol, top = 'F5') {
  const chord = chordAt(bar, beat);
  let m = below(midi(top), chord.tones, false);
  for (let k = 0; k < 5; k++) {
    wind(at(bar, beat) + k * 0.002, 0.11, m, vol * (k === 0 ? 1.15 : 0.85), [0.5, 0.2, 0.62, 0.34, 0.45][k], { bright: 1.5, attack: 0.01, release: 0.12, fall: 0.1 });
    m = below(m, chord.tones, true);
  }
  kick(at(bar, beat), 0.17);
  snare(at(bar, beat), 0.075);
}

const SAX = { reed: 0.55, bright: 0.55, scoop: 0.018 };
const BRASS = { bright: 1.25, attack: 0.014, scoop: 0.008, release: 0.09 };
const SAX_PANS = [-0.45, -0.25, -0.6, -0.35, -0.5];
const BRASS_PANS = [0.5, 0.25, 0.62, 0.38, 0.45];

/** Where the brass answer the saxophones in the first strain: between the phrases of the tune. */
const ANSWERS = [[1, 2.5], [3, 3.5], [5, 3], [7, 3.5]];

// Chorus one: the saxophones have the tune, the brass punch between their phrases and take the bridge.
{
  const c = INTRO;
  section(c, [...TUNE_HEAD, ...TUNE_TURN], 0, 0.125, SAX_PANS, SAX);
  section(c + 8, [...TUNE_HEAD, ...TUNE_HOME], 0, 0.125, SAX_PANS, SAX);
  section(c + 16, TUNE_BRIDGE, 0, 0.1, BRASS_PANS, BRASS);
  section(c + 24, [...TUNE_HEAD, ...TUNE_HOME], 0, 0.13, SAX_PANS, SAX);
  for (const first of [c, c + 8, c + 24]) ANSWERS.slice(0, 3).forEach(([bar, beat]) => punch(first + bar, beat, 0.075));
  // Under the bridge the saxophones hold the two notes that make each chord what it is.
  for (let bar = 0; bar < 8; bar++) {
    const chord = chordAt(c + 16 + bar, 0);
    const third = above(midi('F3'), [chord.tones[1]], false), seventh = above(third, [chord.tones[3]], true);
    [third, seventh].forEach((m, k) => wind(at(c + 16 + bar), BAR * 0.94, m, 0.07, k ? -0.3 : -0.55, { ...SAX, attack: 0.08 }));
  }
}

// Chorus two: a clarinet makes up its own tune over the chords, and the saxophones keep a quiet riff going.
{
  const c = INTRO + CHORUS;
  const CLARINET = { reed: 1, bright: 0.85, scoop: 0.02, attack: 0.018 };
  let pitch = midi('F5'), rising = true;
  for (let bar = 0; bar < CHORUS; bar++) {
    const lastOfEight = bar % 8 === 7;
    for (let half = 0; half < 2; half++) {
      const beat0 = half * 2;
      // A long note to end each eight bars, and a breath after it.
      if (lastOfEight) {
        if (half === 0) {
          const end = below(pitch + 2, chordAt(c + bar, 0).tones, false);
          wind(at(c + bar, 0), span(c + bar, 0, 2.5) * 0.9, end, 0.22, 0.1, CLARINET);
          pitch = end;
        }
        continue;
      }
      // Otherwise two beats at a time: a run of quavers, a longer note and two quavers, three off the beat, one held note, or a rest.
      const pick = rnd();
      const cell = pick < 0.36 ? [[0, 0.5], [0.5, 0.5], [1, 0.5], [1.5, 0.5]]
        : pick < 0.56 ? [[0, 1], [1, 0.5], [1.5, 0.5]]
        : pick < 0.72 ? [[0.5, 0.5], [1, 0.5], [1.5, 0.5]]
        : pick < 0.86 ? [[0, 1.5], [1.5, 0.5]]
        : [];
      for (const [offset, beats] of cell) {
        const beat = beat0 + offset;
        const chord = chordAt(c + bar, beat);
        if (pitch > midi('D6')) rising = false;
        if (pitch < midi('C5')) rising = true;
        if (rnd() < 0.22) rising = !rising;
        // Notes on the beat are notes of the chord; the ones between pass through its scale. Now and then a leap.
        const onBeat = offset % 1 === 0;
        const set = onBeat ? chord.tones : chord.scale;
        let next = rising ? above(pitch, set, true) : below(pitch, set, true);
        if (rnd() < 0.18) next = rising ? above(next, chord.tones, true) : below(next, chord.tones, true);
        pitch = next;
        wind(at(c + bar, beat) + 0.006, held(c + bar, beat, beats), pitch, (onBeat ? 0.19 : 0.22), 0.1, CLARINET);
      }
    }
  }
  // The riff: three soft chords from the saxophones, the same shape every two bars, moved to fit the chord.
  for (let bar = 0; bar < CHORUS; bar++) {
    if (bar >= 16 && bar < 24) {
      // Under the bridge, long notes instead.
      const chord = chordAt(c + bar, 0);
      const third = above(midi('F3'), [chord.tones[1]], false), seventh = above(third, [chord.tones[3]], true);
      [third, seventh].forEach((m, k) => wind(at(c + bar), BAR * 0.94, m, 0.045, k ? -0.3 : -0.55, { ...SAX, attack: 0.08 }));
      continue;
    }
    const hits = bar % 2 === 0 ? [[0, 0.5], [1.5, 0.5], [2.5, 1]] : [[1, 0.5], [2.5, 0.5]];
    for (const [beat, beats] of hits) {
      const chord = chordAt(c + bar, beat);
      let m = below(midi('C5'), chord.tones, false);
      for (let k = 0; k < 4; k++) {
        wind(at(c + bar, beat) + 0.01, held(c + bar, beat, beats), m, 0.04, SAX_PANS[k], SAX);
        m = below(m, chord.tones, true);
      }
    }
  }
}

// Chorus three: the whole band. Brass on the tune an octave up, saxophones under them, and everybody on the accents.
{
  const c = INTRO + 2 * CHORUS;
  const SHOUT = { ...BRASS, bright: 1.6 };
  for (const [first, ending] of [[c, TUNE_TURN], [c + 8, TUNE_HOME], [c + 24, TUNE_HOME]]) {
    section(first, [...TUNE_HEAD, ...ending], 1, 0.1, BRASS_PANS, SHOUT);
    section(first, [...TUNE_HEAD, ...ending], 0, 0.105, SAX_PANS, SAX);
    ANSWERS.forEach(([bar, beat]) => { kick(at(first + bar, beat), 0.2); snare(at(first + bar, beat), 0.11); });
  }
  // The bridge: brass state it, saxophones an octave below them.
  section(c + 16, TUNE_BRIDGE, 0, 0.115, BRASS_PANS, SHOUT);
  section(c + 16, TUNE_BRIDGE, -1, 0.095, SAX_PANS, SAX);
  // The last bar hands the tune back to the rhythm section, which is where the loop begins again.
  punch(c + 31, 2.5, 0.095, 'A5');
}

// The vamp the piece opens on (and returns to each time round): the saxophones hint at the riff.
for (let bar = 0; bar < INTRO; bar++) {
  const hits = bar % 2 === 0 ? [[1.5, 0.5], [2.5, 1]] : [[1, 0.5], [2.5, 0.5]];
  for (const [beat, beats] of hits) {
    const chord = chordAt(bar, beat);
    let m = below(midi('C5'), chord.tones, false);
    for (let k = 0; k < 4; k++) {
      wind(at(bar, beat) + 0.01, held(bar, beat, beats), m, 0.045, SAX_PANS[k], SAX);
      m = below(m, chord.tones, true);
    }
  }
}

// ---- The room: echo, the fold for the loop, and the level -----------------------------------

/** A dance hall: eight echoes in parallel and four diffusers in series (the long-published Freeverb layout), kept short. */
function hall(input, spread) {
  const out = new Float32Array(input.length);
  const combs = [1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617].map((d) => ({ buf: new Float32Array(d + spread), i: 0, store: 0 }));
  const passes = [556, 441, 341, 225].map((d) => ({ buf: new Float32Array(d + spread), i: 0 }));
  const feedback = 0.8, damp = 0.4;
  for (let n = 0; n < input.length; n++) {
    const x = input[n] * 0.03;
    let y = 0;
    for (const c of combs) {
      const v = c.buf[c.i];
      c.store = v * (1 - damp) + c.store * damp;
      c.buf[c.i] = x + c.store * feedback;
      if (++c.i >= c.buf.length) c.i = 0;
      y += v;
    }
    for (const p of passes) {
      const v = p.buf[p.i];
      p.buf[p.i] = y + v * 0.5;
      if (++p.i >= p.buf.length) p.i = 0;
      y = v - y;
    }
    out[n] = y;
  }
  return out;
}

const wetL = hall(dryL, 0);
const wetR = hall(dryR, 23);
const outL = new Float32Array(N);
const outR = new Float32Array(N);
for (let i = 0; i < NT; i++) {
  const l = dryL[i] * 0.9 + wetL[i] * 0.42;
  const r = dryR[i] * 0.9 + wetR[i] * 0.42;
  // What rings on after the last bar is laid over the first bars, so the end runs into the beginning.
  outL[i % N] += l;
  outR[i % N] += r;
}

// The loudest moments are eased down rather than cut off, so the band can be loud without clipping.
let peak = 0;
for (let i = 0; i < N; i++) peak = Math.max(peak, Math.abs(outL[i]), Math.abs(outR[i]));
const drive = 1.9 / peak;
const ease = (v) => Math.tanh(v * drive) / Math.tanh(drive);

const header = Buffer.alloc(44);
header.write('RIFF', 0); header.writeUInt32LE(36 + N * 4, 4); header.write('WAVE', 8);
header.write('fmt ', 12); header.writeUInt32LE(16, 16); header.writeUInt16LE(1, 20); header.writeUInt16LE(2, 22);
header.writeUInt32LE(SR, 24); header.writeUInt32LE(SR * 4, 28); header.writeUInt16LE(4, 32); header.writeUInt16LE(16, 34);
header.write('data', 36); header.writeUInt32LE(N * 4, 40);
const pcm = Buffer.alloc(N * 4);
for (let i = 0; i < N; i++) {
  outL[i] = ease(outL[i]) * 0.9;
  outR[i] = ease(outR[i]) * 0.9;
  pcm.writeInt16LE(Math.round(outL[i] * 32767), i * 4);
  pcm.writeInt16LE(Math.round(outR[i] * 32767), i * 4 + 2);
}

const out = process.argv[2] ?? 'theme.wav';
fs.mkdirSync(new URL('.', new URL(out, `file://${process.cwd()}/`)), { recursive: true });
fs.writeFileSync(out, Buffer.concat([header, pcm]));

// How loud each part came out, as a check that the shape is the one intended.
const parts = [['the vamp', 0, INTRO], ['saxophones', INTRO, INTRO + CHORUS], ['clarinet', INTRO + CHORUS, INTRO + 2 * CHORUS], ['the whole band', INTRO + 2 * CHORUS, BARS]];
const report = parts.map(([name, from, to]) => {
  let sum = 0;
  const a = Math.round(from * BAR * SR), b = Math.min(N, Math.round(to * BAR * SR));
  for (let i = a; i < b; i++) sum += outL[i] ** 2 + outR[i] ** 2;
  return `${name} ${(10 * Math.log10(sum / (2 * (b - a)))).toFixed(1)} dB`;
});
console.log(`${out}: ${(N / SR).toFixed(1)} s at ${BPM} beats a minute, peak before levelling ${peak.toFixed(2)}; ${report.join(', ')}`);
