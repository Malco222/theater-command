// Target scenes: short mixes of the finished sounds, laid out the way the game should lay them. They are to sound
// what the art director's target pictures are to the look: "when the game plays a night raid, it should sound about
// like this". Each scene is a list of cues (when, which sound, how loud, where), so a builder can read the list as a
// recipe.
//
//   node sound/make_scenes.mjs <folder of made sounds> <out-folder> [scene ...]

import fs from 'node:fs';
import path from 'node:path';
import { SR, sec, db, clamp, lerp, stereo, isStereo, toStereo, lengthOf, loudness, level, limit, gain, peak, toDb, readWav, writeWav, speed } from './kit/dsp.mjs';

/** The lift given to every scene alike (dB). */
const MASTER = 0;

const [src, out, ...want] = process.argv.slice(2);
if (!src || !out) { console.error('usage: node sound/make_scenes.mjs <folder of made sounds> <out-folder> [scene ...]'); process.exit(1); }
fs.mkdirSync(out, { recursive: true });
const cache = new Map();
const sound = (name) => { if (!cache.has(name)) cache.set(name, readWav(path.join(src, name + '.wav'))); return cache.get(name); };

/**
 * One cue. at: seconds. g: loudness in dB against the file as made. pan: -1 left to +1 right, or [from, to] for
 * something that moves. For a loop, len says how long it goes round; in and out are its fades. env is a list of
 * [seconds after the cue starts, dB] for a sound that comes and goes. rate plays it faster and higher (1.06) or slower.
 */
function mix(seconds, cues) {
  const bus = stereo(seconds), n = sec(seconds);
  for (const [at, name, o = {}] of cues) {
    let x = sound(name);
    if (o.rate && o.rate !== 1) x = isStereo(x) ? { L: speed(x.L, o.rate), R: speed(x.R, o.rate) } : speed(x, o.rate);
    const one = lengthOf(x), len = o.len ? sec(o.len) : one, st = isStereo(x);
    const fin = sec(o.in ?? (o.len ? 0.6 : 0)), fout = sec(o.out ?? (o.len ? 1.2 : 0));
    const pan0 = Array.isArray(o.pan) ? o.pan[0] : (o.pan ?? 0), pan1 = Array.isArray(o.pan) ? o.pan[1] : pan0;
    const env = o.env ? o.env.map(([t, d]) => [sec(t), d]) : null;
    const base = db(o.g ?? 0), start = sec(at), offset = o.from ? sec(o.from) % one : 0;
    for (let i = 0; i < len; i++) {
      const j = start + i; if (j < 0 || j >= n) continue;
      let g = base;
      if (fin && i < fin) g *= i / fin;
      if (fout && i > len - fout) g *= (len - i) / fout;
      if (env) {
        let k = 0; while (k < env.length - 1 && env[k + 1][0] <= i) k++;
        const [t0, d0] = env[k], [t1, d1] = env[Math.min(k + 1, env.length - 1)];
        g *= db(t1 > t0 ? lerp(d0, d1, clamp((i - t0) / (t1 - t0), 0, 1)) : d0);
      }
      const pan = lerp(pan0, pan1, i / len), k = (offset + i) % one;
      if (st) { bus.L[j] += x.L[k] * g * Math.min(1, 1 - pan); bus.R[j] += x.R[k] * g * Math.min(1, 1 + pan); }
      else { bus.L[j] += x[k] * g * Math.cos((pan + 1) * Math.PI / 4) * 1.414; bus.R[j] += x[k] * g * Math.sin((pan + 1) * Math.PI / 4) * 1.414; }
    }
  }
  return bus;
}

// A helper for the flak: one gun firing and its shell bursting up among the bombers a moment later.
const flak = (t, pan, g = 0, k = 0) => [
  [t, `flak_heavy_fire_0${1 + (k % 3)}`, { g: g - 9, pan, rate: 0.96 + (k * 7 % 9) / 100 }],
  [t + 0.9 + (k % 4) * 0.12, `flak_burst_0${1 + (k % 4)}`, { g: g - 5, pan: clamp(pan * -0.6 + ((k * 5) % 7 - 3) / 10, -1, 1), rate: 0.95 + (k * 3 % 11) / 100 }],
];
const many = (from, to, every, fn) => { const cues = []; let k = 0; for (let t = from; t < to; t += every[k % every.length], k++) cues.push(...fn(t, k)); return cues; };

export const SCENES = {
  night_raid: {
    title: 'A raid by night', seconds: 44,
    about: 'A town with heavy guns is bombed at night. Crickets and quiet; the siren; the lamps strike; the bombers come over with their uneven beat; the guns open and the shells burst; a lamp catches one; the bombs come down; the town burns; the bombers go home; the all clear; the fire is still burning when the crickets come back.',
    cues: [
      [0, 'land_night_loop', { len: 44, g: 4, in: 0.3, out: 2, env: [[0, 0], [6, 0], [9, -10], [33, -10], [38, -2], [44, 0]] }],
      [1.2, 'owl_01', { g: -2, pan: -0.6 }],
      [3.2, 'air_raid_siren', { g: -9, pan: 0.25 }],
      [7.0, 'searchlight_on', { g: -6, pan: -0.5 }], [7.7, 'searchlight_on', { g: -8, pan: 0.5, rate: 1.05 }], [8.3, 'searchlight_on', { g: -9, pan: 0.1, rate: 0.95 }],
      [7.2, 'searchlight_loop', { len: 25, g: -12, pan: -0.3, out: 1.5 }],
      [8, 'bomber_medium_loop', { len: 27, g: -3, pan: [-0.9, 0.9], in: 7, out: 9, env: [[0, -14], [9, -2], [15, 0], [20, -3], [27, -16]] }],
      ...many(12.5, 26.5, [0.45, 0.5, 0.42, 0.55, 0.47], (t, k) => flak(t, ((k * 37) % 17) / 8.5 - 1, -1, k)),
      [13.5, 'flak_light_01', { g: -10, pan: 0.6 }], [18.2, 'flak_light_02', { g: -10, pan: -0.6 }], [23.4, 'flak_light_01', { g: -11, pan: 0.2, rate: 1.04 }],
      [16.4, 'searchlight_caught', { g: -5, pan: 0.1 }],
      [18.0, 'bombs_falling_01', { g: -6, pan: -0.2 }],
      [20.2, 'bomb_stick_01', { g: -1, pan: -0.25 }],
      [21.3, 'bombs_falling_02', { g: -8, pan: 0.3 }],
      [23.3, 'bomb_stick_02', { g: -2, pan: 0.3 }],
      [21.4, 'fire_start', { g: -8, pan: -0.25 }],
      [22, 'fire_town_loop', { len: 22, g: -3, in: 6, out: 3, env: [[0, -12], [7, 0], [22, -2]] }],
      [25.5, 'plane_down', { g: -6, pan: 0.6 }],
      [29.6, 'fire_collapse_01', { g: -9, pan: -0.3 }],
      [32.2, 'searchlight_off', { g: -8, pan: -0.5 }], [32.8, 'searchlight_off', { g: -10, pan: 0.5, rate: 1.05 }],
      [34, 'siren_all_clear', { g: -11, pan: 0.25 }],
    ],
  },
  day_raid: {
    title: 'A raid by day', seconds: 38,
    about: 'The same town bombed by day. Birds and wind; the deep steady drone of the four-engined bombers, far steadier than the night bombers; their escort weaving above; the defending fighters meet them and there is a dogfight; flak; the bombs; one bomber goes down; they go home. No siren wail in the dark, no lamps, no crickets: a different picture altogether.',
    cues: [
      [0, 'land_day_loop', { len: 38, g: 4, in: 0.3, out: 2, env: [[0, 0], [5, 0], [9, -9], [28, -9], [34, 0]] }],
      [1.0, 'bird_01', { g: 0, pan: 0.5 }], [2.6, 'bird_03', { g: -3, pan: -0.6 }], [34.5, 'bird_04', { g: -2, pan: -0.4 }],
      [2.5, 'air_raid_siren', { g: -13, pan: 0.25 }],
      [3, 'bomber_heavy_loop', { len: 30, g: -2, pan: [-0.9, 0.9], in: 8, out: 10, env: [[0, -14], [10, -2], [16, 0], [22, -3], [30, -16]] }],
      [5, 'fighter_loop', { len: 24, g: -9, pan: [-0.7, 0.8], in: 5, out: 6 }],
      [9.5, 'fighter_pass_01', { g: -4, pan: [0.9, -0.9] }],
      [11.5, 'dogfight', { g: -4, pan: 0.2 }],
      [14.5, 'fighter_pass_02', { g: -5, pan: [-0.9, 0.9] }],
      ...many(14, 24, [0.8, 0.9, 0.75, 1.0], (t, k) => flak(t, ((k * 37) % 17) / 8.5 - 1, -3, k)),
      [17.2, 'bombs_falling_01', { g: -6, pan: -0.2 }],
      [19.4, 'bomb_stick_03', { g: -1, pan: -0.25 }],
      [21.0, 'bomb_stick_01', { g: -3, pan: 0.3 }],
      [22.5, 'plane_down', { g: -5, pan: 0.5 }],
      [24.5, 'fighter_pass', { g: -7, pan: [0.6, -0.9] }],
    ],
  },
  march_and_train: {
    title: 'A march, then a train', seconds: 46,
    about: 'Two ways to move an army, told apart with the eyes shut. First a column steps off and marches: boots, in time. Then a train: the whistle, the engine pulling away, the beat of the exhaust and the wheels over the joints; a flak wagon on it fires at something overhead; at the end of the line it brakes, the buffers close up, and it sets off back the way it came.',
    cues: [
      [0, 'land_day_loop', { len: 46, g: 2, in: 0.3, out: 2 }],
      [0.6, 'order_march', { g: 6, pan: 0 }],
      [1.3, 'marching', { g: 0, pan: -0.3 }],
      [3.2, 'marching_loop', { len: 9.5, g: -1, pan: [-0.3, 0.5], in: 1.2, out: 3 }],
      [6.0, 'bird_02', { g: -3, pan: -0.7 }],
      [13.2, 'putdown', { g: 6 }],
      [15.0, 'train_whistle_01', { g: -4, pan: -0.6 }],
      [16.2, 'train_depart', { g: -2, pan: -0.6 }],
      [20.5, 'train_loop', { len: 12, g: -2, pan: [-0.6, 0.5], in: 2.5, out: 1.5 }],
      [24.3, 'flak_wagon_fire_01', { g: -6, pan: -0.05 }], [25.5, 'flak_wagon_fire_02', { g: -6, pan: 0.1 }],
      [28.5, 'train_whistle_02', { g: -6, pan: 0.35 }],
      [32, 'train_turn_round', { g: -1, pan: 0.5 }],
      [41, 'train_loop', { len: 5, g: -4, pan: [0.5, 0.1], in: 1.5, out: 3 }],
    ],
  },
  month_end: {
    title: 'The month ends', seconds: 30,
    about: 'The war room at the end of a month. A few orders given at the desk; END THE MONTH; the question ("armies at the front have no orders"); the answer; the month ends; the other countries move, quickly; the month is yours again; the month card is laid in front of you with good news; you turn its page and close it.',
    cues: [
      [0, 'room_day_loop', { len: 30, g: 6, in: 0.3, out: 1.5, env: [[0, 0], [12.2, 0], [12.8, -6], [17.8, -6], [18.6, 0]] }],
      [0.8, 'select_region', { g: 4 }], [1.7, 'pickup', { g: 4 }], [2.6, 'order_march', { g: 4 }], [3.5, 'putdown', { g: 4 }],
      [4.5, 'click', { g: 4 }], [5.2, 'stamp', { g: 3 }],
      [6.8, 'click', { g: 4 }],
      [7.1, 'month_ask', { g: 3 }],
      [9.9, 'click', { g: 4 }],
      [10.3, 'month_end', { g: 3 }],
      [12.6, 'month_passing_loop', { len: 5.4, g: 4, in: 0.3, out: 0.5 }],
      [14.0, 'radio', { g: -6, pan: 0.5 }],
      [18.2, 'month_yours', { g: 2 }],
      [19.8, 'month_card_open', { g: 3 }],
      [20.6, 'month_card_good', { g: 1 }],
      [24.6, 'page_turn', { g: 4 }],
      [26.6, 'card_close', { g: 4 }], [27.4, 'click', { g: 4 }],
    ],
  },
  attack: {
    title: 'An attack goes in', seconds: 32,
    about: 'A gun line and an assault. The drums as the two sides are drawn up; the guns open; shells go over and land; tanks start and roll; rifles and machine guns; the noise of the fight; and it is won.',
    cues: [
      [0, 'land_day_loop', { len: 32, g: 0, in: 0.3, out: 2 }],
      [0, 'front_loop', { len: 32, g: 2, in: 1, out: 3 }],
      [0.6, 'order_attack', { g: 6 }],
      [1.5, 'drums', { g: -3 }],
      [5.2, 'artillery_salvo_01', { g: -3, pan: -0.5 }],
      [7.0, 'shell_incoming_01', { g: -6, pan: [-0.4, 0.5] }], [8.6, 'shell_impact_01', { g: -4, pan: 0.5 }],
      [8.3, 'artillery_fire_02', { g: -5, pan: -0.6 }], [9.6, 'shell_impact_02', { g: -6, pan: 0.6 }],
      [10.4, 'artillery_salvo_02', { g: -4, pan: -0.5 }], [12.6, 'shell_impact_03', { g: -5, pan: 0.4 }],
      [11.5, 'tank_engine', { g: -4, pan: -0.2 }],
      [14, 'tank_loop', { len: 10, g: -6, pan: [-0.3, 0.4], in: 1.5, out: 3 }],
      [15.5, 'tank_gun', { g: -4, pan: 0 }], [18.4, 'tank_gun_02', { g: -5, pan: 0.2 }],
      [14.5, 'battle', { g: -4 }],
      [16.5, 'rifles', { g: -6, pan: 0.5 }], [17.8, 'machine_gun', { g: -6, pan: 0.6 }], [20.2, 'rifles_02', { g: -7, pan: 0.3 }], [21.0, 'machine_gun_02', { g: -7, pan: 0.5 }],
      [22.0, 'explosion', { g: -5, pan: 0.4 }],
      [25.5, 'victory', { g: -1 }],
    ],
  },
  fleet: {
    title: 'A fleet at sea', seconds: 34,
    about: 'The sea war. Gulls and the swell; a fleet puts out with a blast on the siren; the engines; destroyers hunt a submarine (the ping, then the charges); the big guns fire a salvo and the shells land; a torpedo strikes.',
    cues: [
      [0, 'sea_day_loop', { len: 34, g: 4, in: 0.3, out: 2 }],
      [0.8, 'gull_01', { g: 0, pan: 0.6 }], [29.5, 'gull_02', { g: -2, pan: -0.5 }],
      [2.2, 'ship_horn', { g: -3, pan: -0.3 }],
      [4.5, 'anchor_chain', { g: -6, pan: -0.3 }],
      [6, 'ship_loop', { len: 26, g: 0, pan: [-0.3, 0.2], in: 3, out: 4 }],
      [9.5, 'sonar_ping', { g: -4, pan: 0.3 }], [12.0, 'sonar_ping', { g: -5, pan: 0.3 }],
      [14.2, 'depth_charge', { g: -3, pan: 0.4 }],
      [18.5, 'ship_guns', { g: -2, pan: -0.2 }],
      [22.5, 'ship_guns_02', { g: -3, pan: 0.1 }],
      [26.0, 'torpedo_hit', { g: -3, pan: 0.5 }],
    ],
  },
  // ---- Stings: short mixes that are themselves sounds, for a card that reports something it cannot show ----------
  raid_day: {
    sting: true, lufs: -19, title: 'Sting: a raid by day', seconds: 7.5,
    about: 'For the raid card (and the browser game) when the raid was flown by day: the steady drone of heavy bombers, a fighter going through, flak, the bombs. No siren, no lamps.',
    cues: [
      [0, 'bomber_heavy_loop', { len: 7.5, g: -2, pan: [-0.6, 0.6], in: 1.2, out: 3 }],
      [0.6, 'fighter_pass_01', { g: -6, pan: [0.8, -0.8] }],
      [1.9, 'flak_burst_02', { g: -6, pan: 0.4 }], [2.6, 'flak_burst_01', { g: -7, pan: -0.3 }], [3.3, 'flak_burst_03', { g: -8, pan: 0.1 }],
      [2.2, 'bombs_falling_01', { g: -8, len: 1.9, out: 0.3 }],
      [3.9, 'bomb_stick_03', { g: -1, pan: -0.1 }],
    ],
  },
  raid_night: {
    sting: true, lufs: -19, title: 'Sting: a raid by night', seconds: 9,
    about: 'For the raid card (and the browser game) when the raid was flown by night: the siren, a lamp struck, the uneven beat of the night bombers, flak, the bombs, and fire after.',
    cues: [
      [0, 'air_raid_siren', { g: -7, len: 4.2, in: 0.05, out: 2.2, pan: 0.2 }],
      [0.9, 'searchlight_on', { g: -5, pan: -0.4 }],
      [0.5, 'bomber_medium_loop', { len: 8, g: -3, pan: [-0.6, 0.6], in: 2, out: 3.5 }],
      [2.6, 'flak_heavy_fire_01', { g: -9, pan: -0.5 }], [3.4, 'flak_burst_01', { g: -6, pan: 0.3 }], [4.0, 'flak_burst_04', { g: -7, pan: -0.2 }],
      [3.1, 'bombs_falling_02', { g: -8, len: 1.9, out: 0.3 }],
      [4.9, 'bomb_stick_01', { g: -1, pan: 0.1 }],
      [5.6, 'fire_town_loop', { len: 3.4, g: -4, in: 1.2, out: 1.6 }],
    ],
  },
  rail_move: {
    sting: true, lufs: -20, title: 'Sting: an army goes by rail', seconds: 5.5,
    about: 'For the browser game when an army is sent by rail: the whistle and the engine pulling away. Set against "marching", it is how a rail move and a march are told apart with the eyes shut.',
    cues: [
      [0, 'train_whistle_01', { g: -3, pan: -0.2 }],
      [0.9, 'train_depart', { g: 0, len: 4.6, out: 1.6, pan: [-0.2, 0.3] }],
    ],
  },
  first_command: {
    title: 'First Command: a staff note', seconds: 16,
    about: 'The first hour\'s lessons. A staff note arrives on the desk; you do what it asks (a piece picked up, an order pencilled, the piece put down); the note is stamped DONE; and, at the very end of the hour, the closing note: the command is yours.',
    cues: [
      [0, 'room_day_loop', { len: 16, g: 6, in: 0.3, out: 1.5 }],
      [0.8, 'note_arrives', { g: 4 }],
      [3.0, 'pickup', { g: 4 }], [3.9, 'order_march', { g: 4 }], [4.8, 'putdown', { g: 4 }],
      [6.0, 'note_done', { g: 3 }],
      [8.6, 'note_closing', { g: 1 }],
    ],
  },
};

if (import.meta.url === `file://${process.argv[1]}`) {
  const made = [];
  for (const [key, sc] of Object.entries(SCENES)) {
    if (want.length && !want.includes(key)) continue;
    const bus = mix(sc.seconds, sc.cues);
    // Every scene gets the same lift, so that the desk stays quieter than the raid, as it will be in the game.
    const natural = loudness(bus).integrated;
    if (sc.sting) level(bus, sc.lufs, { by: 'moment', ceiling: -1 });
    else { gain(bus, db(MASTER)); if (peak(bus) > db(-1)) limit(bus, -1); }
    const file = sc.sting ? key : `scene_${key}`;
    writeWav(path.join(out, `${file}.wav`), bus);
    const names = [...new Set(sc.cues.map((c) => c[1]))];
    made.push({ name: file, sting: !!sc.sting, title: sc.title, seconds: sc.seconds, about: sc.about, sounds: names, cues: sc.cues.map(([at, name, o = {}]) => ({ at, name, ...o })) });
    console.log(`${file.padEnd(24)} ${sc.seconds} s  ${names.length} sounds, ${sc.cues.length} cues  peak ${toDb(peak(bus)).toFixed(1)} dB  ${loudness(bus).integrated.toFixed(1)} LUFS (as mixed ${natural.toFixed(1)})`);
  }
  if (!want.length) fs.writeFileSync(path.join(out, 'scene-list.json'), JSON.stringify(made, null, 1));
}
