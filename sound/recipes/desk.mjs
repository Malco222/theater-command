// The interface: the sounds of a staff officer's desk. Paper, pencil, a rubber stamp, wood, brass, a bakelite
// switch, a typewriter, a telephone. Nothing electronic: in 1939 a button was a thing you could knock on.

import {
  SR, TAU, sec, db, clamp, lerp, smooth, rng, mono, stereo, add, place, gain, mul, line, decay, fade, wander,
  white, pink, brown, sine, buzz, svf, lp, hp, bp, lp4, eq, comb, drive, inPlace, far, loopOf, toMono, widen,
} from '../kit/dsp.mjs';
import { clang, bell, knock, tick, swish, glide, hiss } from '../kit/parts.mjs';

const desk = (x, wet = -16) => inPlace(x, 'desk', wet);
/** A typewriter key going home: the type bar on the platen and the key's own clack. */
const typeKey = (r) => {
  const out = mono(0.12);
  add(out, tick(r, { tone: r.range(2600, 3600), body: r.range(700, 1000), tau: 0.0035 }), 0, 1);
  add(out, knock(r, { f: [r.range(260, 340), 620, 1150], tau: 0.012, dur: 0.1, click: 0.3 }), 0.004, 0.7);
  return out;
};
/** A soft thud on a blotter or a baize table. */
const thud = (r, f = 95, tau = 0.05, g = 1) => { const n = sec(tau * 7), o = mul(sine(n, glide(n, f * 1.7, f, 0.012)), decay(n, tau, 0.0015)); add(o, mul(lp(white(n, r), 500), decay(n, tau * 0.5, 0.001)), 0, 0.6); return gain(o, g); };

export default function (def) {
  // ---- Buttons ---------------------------------------------------------------------------------------------
  def('click', { ch: 2, lufs: -30, about: 'Any button pressed: a small firm click, like a bakelite switch. The commonest sound in the game, so it is short and soft.' }, (r) => {
    const out = mono(0.12);
    add(out, tick(r, { tone: 2900, body: 780, tau: 0.004 }), 0.001, 1);
    add(out, tick(r, { tone: 3600, body: 1050, tau: 0.003 }), 0.017, 0.5);
    add(out, thud(r, 190, 0.012, 0.5), 0.001, 1);
    return desk(out, -20);
  });
  def('ui_primary', { ch: 2, lufs: -27, about: 'The one brass button of a screen pressed (END THE MONTH, Attack, Send): a heavier click with brass in it, and its release.' }, (r) => {
    const out = mono(0.3);
    add(out, tick(r, { tone: 2100, body: 560, tau: 0.006 }), 0.001, 1);
    add(out, thud(r, 135, 0.022, 1), 0.001, 1);
    add(out, clang(r, { f0: 1240, dur: 0.25, tau: 0.045, partials: 5, bright: 0.6, click: 0.2 }), 0.003, 0.22); // the brass face
    add(out, tick(r, { tone: 2900, body: 820, tau: 0.004 }), 0.075, 0.4);
    return desk(out, -17);
  });
  def('ui_dial', { ch: 2, lufs: -32, about: 'A dial moved one stop (the history dial, the difficulty, a volume): the small sharp click of a detent.' }, (r) => {
    const out = mono(0.12);
    add(out, tick(r, { tone: 3300, body: 1250, tau: 0.003 }), 0.001, 1);
    add(out, clang(r, { f0: 3150, dur: 0.1, tau: 0.014, partials: 3, bright: 0.5, click: 0.3 }), 0.002, 0.2);
    return desk(out, -20);
  });
  def('tick', { ch: 2, lufs: -38, about: 'The smallest sound on the desk, hardly more than a breath: a row chosen in a list, a card turned in the newsreel. Never played for the pointer merely passing over something.' }, (r) => desk(tick(r, { tone: 3800, body: 1500, tau: 0.0022, dur: 0.035 }), -22));
  def('ui_back', { ch: 2, lufs: -32, about: 'Close, cancel, step back: the same click, lower and softer, so that going back sounds like going back.' }, (r) => {
    const out = mono(0.14);
    add(out, tick(r, { tone: 1900, body: 520, tau: 0.005 }), 0.001, 1);
    add(out, thud(r, 140, 0.016, 0.7), 0.002, 1);
    return desk(out, -20);
  });
  def('ui_tab', { ch: 2, lufs: -32, about: 'A tab chosen: a card flicked forward in an index.' }, (r) => {
    const out = mono(0.2);
    add(out, swish(r, 0.07, { lo: 2600, hi: 7000, rough: 0.4, grain: 400 }), 0, 0.7);
    add(out, tick(r, { tone: 3100, body: 900, tau: 0.003 }), 0.06, 0.9);
    return desk(out, -20);
  });
  def('switch_on', { ch: 2, lufs: -29, about: 'A setting turned on, the lamp lit, night to day: a heavy toggle thrown over.' }, (r) => {
    const out = mono(0.25);
    add(out, tick(r, { tone: 2400, body: 640, tau: 0.006 }), 0.001, 1);
    add(out, clang(r, { f0: 1750, dur: 0.2, tau: 0.03, partials: 4, bright: 0.6, click: 0.3 }), 0.004, 0.2);
    add(out, thud(r, 170, 0.02, 0.8), 0.002, 1);
    return desk(out, -18);
  });
  def('switch_off', { ch: 2, lufs: -30, about: 'A setting turned off, the lamp put out, day to night: the toggle thrown back, a shade lower.' }, (r) => {
    const out = mono(0.25);
    add(out, tick(r, { tone: 1900, body: 520, tau: 0.006 }), 0.001, 1);
    add(out, clang(r, { f0: 1380, dur: 0.2, tau: 0.025, partials: 4, bright: 0.6, click: 0.3 }), 0.004, 0.15);
    add(out, thud(r, 130, 0.022, 0.8), 0.002, 1);
    return desk(out, -18);
  });
  def('ui_error', { ch: 2, lufs: -28, about: 'That cannot be done: two dull knocks on the desk. Never a buzzer.' }, (r) => {
    const out = mono(0.5);
    for (const t of [0.002, 0.15]) add(out, knock(r, { f: [150, 265, 430], tau: 0.035, click: 0.25, weight: 1.3 }), t, t ? 0.8 : 1);
    return desk(out, -18);
  });
  def('ui_notify', { ch: 2, lufs: -27, about: 'Something wants attention: one touch on a desk bell.' }, (r) => inPlace(bell(r, { f0: 1760, dur: 1.6, tau: 0.5, bright: 0.6, strike: 0.5 }), 'room', -18));

  // ---- Cards and pages ---------------------------------------------------------------------------------------
  def('card_open', { ch: 2, lufs: -29, about: 'A card laid on the table: it slides in and lands. For every card that opens (an army\'s, a town\'s, a report).' }, (r) => {
    const out = mono(0.4);
    add(out, swish(r, 0.17, { lo: 1500, hi: 5200, rough: 0.45, grain: 220 }), 0, 0.8);
    add(out, thud(r, 120, 0.02, 0.9), 0.16, 1);
    add(out, tick(r, { tone: 2200, body: 700, tau: 0.004 }), 0.162, 0.4);
    return desk(out, -18);
  });
  def('card_close', { ch: 2, lufs: -31, about: 'A card taken up again: the same slide, backwards and lighter.' }, (r) => {
    const out = mono(0.3);
    add(out, tick(r, { tone: 2000, body: 600, tau: 0.003 }), 0, 0.4);
    add(out, swish(r, 0.15, { lo: 1500, hi: 4600, rough: 0.45, grain: 220, move: 0 }), 0.01, 0.7);
    return desk(out, -18);
  });
  def('page_turn', { ch: 2, lufs: -29, about: 'A page of a heavy book turned: for the ledger, the roll of honour, the guide, and moving from one report to the next.' }, (r) => {
    const out = mono(0.6);
    add(out, swish(r, 0.34, { lo: 900, hi: 5600, rough: 0.7, grain: 90, shape: 0.5 }), 0, 0.9);
    for (let k = 0; k < 5; k++) add(out, tick(r, { tone: r.range(2500, 5000), body: 1200, tau: 0.002 }), r.range(0.05, 0.3), r.range(0.1, 0.25)); // the paper buckling
    add(out, thud(r, 110, 0.03, 0.5), 0.36, 1);
    return desk(out, -17);
  });
  def('screen_open', { ch: 2, lufs: -27, about: 'A whole room of the game opened (the arsenal, research, diplomacy, the ledger): a deep drawer pulled out and a book set down.' }, (r) => {
    const out = mono(0.8);
    add(out, mul(svf(brown(sec(0.18), r), 'bp', 420, 1.2), line(sec(0.18), [[0, 0], [0.05, 1], [0.18, 0.2]])), 0, 2.0); // wood running on wood
    add(out, knock(r, { f: [105, 190, 330], tau: 0.05, weight: 1.4, click: 0.3 }), 0.16, 1); // it lands as the page finishes rising (160 ms)
    add(out, swish(r, 0.2, { lo: 1200, hi: 4200, rough: 0.5, grain: 160 }), 0.23, 0.35);
    return desk(out, -16);
  });
  def('screen_close', { ch: 2, lufs: -28, about: 'That room closed: the book shut and the drawer pushed home.' }, (r) => {
    const out = mono(0.7);
    add(out, thud(r, 100, 0.05, 1), 0.002, 1);
    add(out, swish(r, 0.08, { lo: 1500, hi: 3800, rough: 0.3, grain: 300 }), 0, 0.3);
    add(out, mul(svf(brown(sec(0.22), r), 'bp', 380, 1.2), line(sec(0.22), [[0, 0.3], [0.15, 1], [0.22, 0]])), 0.12, 1.6);
    add(out, knock(r, { f: [95, 180, 300], tau: 0.04, weight: 1.4, click: 0.2 }), 0.33, 0.8);
    return desk(out, -16);
  });
  def('chart_unroll', { ch: 2, lufs: -28, about: 'A chart unrolled across the table: for the raid planner, the sea chart, the map views.' }, (r) => {
    const out = mono(0.9);
    add(out, swish(r, 0.6, { lo: 700, hi: 4800, rough: 0.8, grain: 60, shape: 0.35 }), 0, 1);
    for (let k = 0; k < 9; k++) add(out, tick(r, { tone: r.range(1800, 4200), body: 900, tau: 0.003 }), r.range(0.05, 0.55), r.range(0.1, 0.3));
    add(out, thud(r, 130, 0.025, 0.6), 0.6, 1); add(out, thud(r, 150, 0.02, 0.5), 0.68, 1); // the weights put on its corners
    return desk(out, -17);
  });

  // ---- Orders --------------------------------------------------------------------------------------------------
  // One stamp, three ways of bringing it down. low is how far its pitch is dropped; weight is how hard it lands.
  const stampOf = (r, { low = 1, weight = 1, jump = 1 } = {}) => {
    const out = mono(0.5);
    add(out, thud(r, 62 * low, 0.075 / Math.sqrt(low), 1.3 * weight), 0.004, 1);
    add(out, knock(r, { f: [185 * low, 330 * low, 540 * low], tau: 0.03, weight: 1, click: 0.6 }), 0.004, 0.8);
    add(out, mul(bp(white(sec(0.06), r), 1900 * low, 0.8), decay(sec(0.06), 0.012, 0.0008)), 0.004, 0.9);
    if (jump > 0) add(out, clang(r, { f0: 2300, dur: 0.25, tau: 0.03, partials: 4, bright: 0.5, click: 0 }), 0.02, 0.03 * jump); // the inkwell and pens jumping
    add(out, swish(r, 0.05, { lo: 2200, hi: 5000, rough: 0.3 }), 0.16, 0.12); // and the stamp lifted off
    return desk(out, -15);
  };
  def('stamp', { ch: 2, lufs: -24, about: 'An order confirmed, and the WON stamp: a rubber stamp brought down firmly on paper. The sound of "it is done". Fire it as the stamp lands, not as it starts to fall.' }, (r) => stampOf(r));
  def('stamp_low', { ch: 2, lufs: -24, about: 'The LOST stamp: the same stamp as "stamp", lower and heavier, so that good and bad are told apart before the word is read.' }, (r) => stampOf(r, { low: 0.74, weight: 1.25, jump: 0 }));
  def('stamp_light', { ch: 2, lufs: -29, about: 'Something bought or ordered: the same stamp, lightly, as a clerk marks a chit. For the everyday order; "stamp" is for the ones that matter.' }, (r) => stampOf(r, { low: 1.3, weight: 0.5, jump: 0 }));
  def('pickup', { ch: 2, lufs: -30, about: 'A piece lifted off the table: an army or a fleet taken in the hand. (The 3D game already calls this name; it has been silent until now.)' }, (r) => {
    const out = mono(0.25);
    add(out, swish(r, 0.09, { lo: 900, hi: 2800, rough: 0.5, grain: 200 }), 0, 0.5);
    add(out, knock(r, { f: [420, 760, 1280], tau: 0.012, dur: 0.1, click: 0.4 }), 0.07, 0.6);
    return desk(out, -18);
  });
  def('putdown', { ch: 2, lufs: -28, about: 'A piece set down on the table: wood on wood, and it settles.' }, (r) => {
    const out = mono(0.3);
    add(out, knock(r, { f: [230, 410, 700, 1150], tau: 0.03, click: 0.6 }), 0.002, 1);
    add(out, knock(r, { f: [250, 440, 760], tau: 0.015, click: 0.3 }), 0.055, 0.3);
    add(out, thud(r, 110, 0.03, 0.6), 0.002, 1);
    return desk(out, -17);
  });
  def('order_march', { ch: 2, lufs: -28, about: 'An order to march drawn on the map: one stroke of a wax pencil.' }, (r) => {
    const out = mono(0.45);
    add(out, tick(r, { tone: 2600, body: 800, tau: 0.003 }), 0, 0.3);
    add(out, swish(r, 0.3, { lo: 1300, hi: 3600, q: 1.4, rough: 0.9, grain: 520, shape: 0.3 }), 0.01, 1);
    return desk(out, -18);
  });
  def('order_attack', { ch: 2, lufs: -26, about: 'An attack ordered: two hard strokes of the red pencil, an arrowhead, and the pencil rapped on the map.' }, (r) => {
    const out = mono(0.7);
    add(out, swish(r, 0.2, { lo: 1500, hi: 4200, q: 1.4, rough: 0.9, grain: 560, shape: 0.3 }), 0, 1);
    add(out, swish(r, 0.11, { lo: 1800, hi: 4600, q: 1.4, rough: 0.9, grain: 600, shape: 0.3 }), 0.24, 0.9);
    add(out, swish(r, 0.1, { lo: 1800, hi: 4600, q: 1.4, rough: 0.9, grain: 600, shape: 0.3, move: 0 }), 0.36, 0.9);
    add(out, knock(r, { f: [310, 560, 980], tau: 0.018, click: 0.8 }), 0.5, 0.9);
    return desk(out, -18);
  });
  def('order_cancel', { ch: 2, lufs: -30, about: 'An order taken back: the mark rubbed out.' }, (r) => {
    const out = mono(0.5);
    for (let k = 0; k < 3; k++) add(out, swish(r, 0.12, { lo: 500, hi: 1900, q: 0.8, rough: 0.6, grain: 120, move: k % 2 }), k * 0.13, 0.9 - k * 0.15);
    return desk(out, -18);
  });
  def('select_region', { ch: 2, lufs: -34, about: 'A province chosen on the map: a fingertip on paper.' }, (r) => {
    const out = mono(0.12);
    add(out, thud(r, 170, 0.014, 1), 0, 1);
    add(out, mul(bp(white(sec(0.04), r), 2400, 0.9), decay(sec(0.04), 0.004, 0.0005)), 0, 0.35);
    return desk(out, -19);
  });
  def('coins', { ch: 2, lufs: -27, about: 'Money spent: a few coins counted down onto the desk.' }, (r) => {
    const out = stereo(1.3);
    let t = 0.01;
    for (let k = 0; k < 5; k++) {
      const f = r.range(3400, 5200), pn = r.range(-0.4, 0.4);
      // each coin lands, rings, and chatters as it settles
      let gap = r.range(0.05, 0.09), g = 1;
      for (let b = 0; b < 5; b++) { place(out, clang(r, { f0: f, dur: 0.35, tau: 0.07, partials: 5, bright: 0.75, spread: [1, 1.52, 2.31, 3.4, 4.1], click: 0.5 }), t, pn, g * 0.5); t += gap; gap *= 0.55; g *= 0.55; }
      t += r.range(0.02, 0.07);
    }
    return desk(out, -17);
  });
  def('coin_run', { ch: 2, lufs: -28, about: 'The treasury figure counts to its new value: one short run of coins, done inside the 400 ms the figure takes.' }, (r) => {
    const out = stereo(0.6);
    for (let k = 0; k < 4; k++) place(out, clang(r, { f0: r.range(3500, 5200), dur: 0.28, tau: 0.05, partials: 5, bright: 0.75, spread: [1, 1.52, 2.31, 3.4, 4.1], click: 0.5 }), 0.005 + k * 0.085 + r.range(-0.01, 0.01), r.range(-0.3, 0.3), 0.5 - k * 0.07);
    return desk(out, -18);
  });
  def('alert_slip', { ch: 2, lufs: -28, about: 'An alert slip arrives: a telegraph sounder rattles out three strokes. Dry and short, so that several alerts in a month do not nag.' }, (r) => {
    const out = mono(0.45);
    for (const [t, g] of [[0.002, 1], [0.062, 0.7], [0.13, 0.95], [0.19, 0.7], [0.27, 1], [0.37, 0.75]]) { // down and up, three times: dit, dit, dah
      add(out, clang(r, { f0: r.range(1450, 1600), dur: 0.08, tau: 0.01, partials: 4, bright: 0.6, click: 0.6 }), t, g * 0.5);
      add(out, knock(r, { f: [420, 780], tau: 0.008, dur: 0.06, click: 0.2 }), t, g * 0.5);
    }
    return desk(out, -17);
  });
  def('pen_sign', { ch: 2, lufs: -29, about: 'A paper signed: a steel nib across it and a full stop. For treaties, pacts, promotions, a save made.' }, (r) => {
    const out = mono(0.9);
    for (let k = 0; k < 4; k++) add(out, swish(r, r.range(0.09, 0.16), { lo: 2600, hi: 6500, q: 1.8, rough: 0.9, grain: 800, move: k % 2 }), k * 0.15, r.range(0.6, 1));
    add(out, tick(r, { tone: 3200, body: 1400, tau: 0.002 }), 0.68, 0.5);
    return desk(out, -18);
  });

  // ---- Machines on the desk -------------------------------------------------------------------------------------
  def('typewriter', { ch: 2, lufs: -27, about: 'A line typed and the carriage thrown back: for a report opened, a dispatch written.' }, (r) => {
    const out = mono(2.0);
    let t = 0.01;
    for (let k = 0; k < 13; k++) { add(out, typeKey(r), t, r.range(0.7, 1)); t += r.range(0.055, 0.1) + (r.chance(0.2) ? 0.09 : 0); }
    add(out, bell(r, { f0: 2350, dur: 0.8, tau: 0.25, bright: 0.55, strike: 0.5 }), t + 0.02, 0.25);
    for (let k = 0; k < 22; k++) add(out, tick(r, { tone: 2600, body: 1100, tau: 0.002 }), t + 0.14 + k * 0.011, 0.35); // the ratchet
    add(out, knock(r, { f: [210, 380, 690], tau: 0.025, click: 0.5 }), t + 0.4, 0.8);
    return inPlace(out, 'room', -16);
  });
  def('type_key', { ch: 2, lufs: -33, about: 'One typewriter key: for each line of the month card as it is set out, and for figures that count up.' }, (r) => desk(typeKey(r), -19));
  def('teleprinter', { ch: 2, lufs: -28, about: 'The teleprinter wakes and rattles out a message: news from outside coming in (dispatches, another country\'s move, an intercept).' }, (r) => {
    const out = mono(1.9);
    add(out, mul(lp(buzz(sec(1.8), 50, 'saw'), 700), line(sec(1.8), [[0, 0], [0.08, 1], [1.6, 1], [1.8, 0]])), 0, 0.08);
    for (let t = 0.1; t < 1.6; t += 1 / 11.5) if (!r.chance(0.12)) { add(out, tick(r, { tone: r.range(2000, 2800), body: 900, tau: 0.004 }), t, r.range(0.6, 1)); add(out, knock(r, { f: [330, 720, 1300], tau: 0.01, dur: 0.08, click: 0.3 }), t + 0.003, 0.5); }
    return inPlace(out, 'room', -15);
  });
  def('telephone', { ch: 2, lufs: -26, about: 'The telephone on the desk rings twice: another government wants a word. For an offer, an ultimatum, a cabinet matter.' }, (r) => {
    const out = mono(2.2);
    for (const t0 of [0.02, 0.62]) for (let t = 0; t < 0.4; t += 1 / 24) {
      const two = Math.round(t * 24) % 2;
      add(out, bell(r, { f0: two ? 1480 : 1250, dur: 0.6, tau: 0.16, bright: 0.7, strike: 0.5 }), t0 + t, 0.5);
    }
    return inPlace(out, 'room', -15);
  });
  def('radio', { ch: 2, lufs: -28, about: 'A wireless message through the static: dots and dashes (they spell the game\'s initials). For intelligence, spies, a ship\'s report.' }, (r) => {
    const dur = 2.6, n = sec(dur), out = mono(dur);
    const stat = bp(white(n, r), 1900, 0.5), swell = wander(n, 1.3, r, 0.5);
    for (let i = 0; i < n; i++) out[i] = stat[i] * 0.2 * swell[i] + (r.chance(0.0006) ? r.bip() * 0.8 : 0);
    let t = 0.25; const dot = 0.062;
    for (const sym of '- / -.-.  ... -') {
      if (sym === '/') { t += dot * 4; continue; } if (sym === ' ') { t += dot * 2; continue; }
      const len = sym === '-' ? dot * 3 : dot, m = sec(len);
      add(out, mul(sine(m, 705), line(m, [[0, 0], [0.005, 1], [len - 0.005, 1], [len, 0]])), t, 0.55);
      t += len + dot;
    }
    return inPlace(lp(hp(out, 350), 3200), 'room', -20);
  });
  def('projector', { ch: 2, lufs: -30, about: 'A film projector running: the newsreel\'s own noise, under its pictures.' }, (r) => {
    const dur = 6, n = sec(dur), out = mono(dur);
    for (let t = 0.02; t < dur - 0.03; t += 1 / 16 + r.range(-0.002, 0.002)) { add(out, tick(r, { tone: r.range(2300, 2900), body: 420, tau: 0.004 }), t, r.range(0.6, 0.9)); }
    add(out, lp(buzz(n, 96, 'saw'), 500), 0, 0.12);
    add(out, lp(white(n, r), 1500), 0, 0.05);
    return inPlace(fade(out, 0.4, 0.8), 'room', -16);
  });

  // ---- The month -----------------------------------------------------------------------------------------------
  def('month_ask', { ch: 2, lufs: -25, about: 'The question before the month ends ("armies at the front have no orders"): a clock\'s single soft chime, and two ticks. A pause, not an alarm.' }, (r) => {
    const out = mono(2.6);
    add(out, bell(r, { f0: 660, dur: 2.4, tau: 0.9, bright: 0.6, strike: 0.25 }), 0.02, 0.9);
    add(out, bell(r, { f0: 990, dur: 1.6, tau: 0.5, bright: 0.5, strike: 0.1 }), 0.02, 0.25);
    for (const [t, f] of [[0.75, 2100], [1.25, 1700]]) add(out, tick(r, { tone: f, body: f * 0.3, tau: 0.006 }), t, 0.35);
    return inPlace(out, 'room', -14);
  });
  def('month_end', { ch: 2, lufs: -23, about: 'The month is ended: the calendar page torn off, the day book shut, one stroke of the clock. Played once when END THE MONTH is pressed.' }, (r) => {
    const out = mono(2.8);
    add(out, swish(r, 0.26, { lo: 1300, hi: 6500, rough: 0.85, grain: 240, shape: 0.3 }), 0, 0.9); // the page torn off
    add(out, knock(r, { f: [92, 170, 290], tau: 0.06, weight: 1.6, click: 0.3, dur: 0.4 }), 0.34, 1.2); // the book shut
    add(out, mul(lp(white(sec(0.1), r), 800), decay(sec(0.1), 0.02, 0.001)), 0.34, 1);
    add(out, bell(r, { f0: 440, dur: 2.2, tau: 0.9, bright: 0.62, strike: 0.3 }), 0.62, 0.6);
    return inPlace(out, 'room', -14);
  });
  def('month_passing_loop', { loop: true, ch: 2, lufs: -30, about: 'The other countries moving, very fast: a clock running quick and a teleprinter chattering somewhere. Goes round under the playback of everyone else\'s month.' }, (r) => {
    const dur = 4.8, out = mono(dur + 0.2);
    for (let k = 0; k < 24; k++) add(out, tick(r, { tone: k % 2 ? 1750 : 2150, body: k % 2 ? 520 : 640, tau: 0.006 }), k * 0.2, k % 2 ? 0.55 : 0.75); // the clock, four times too fast
    for (let t = 0.03; t < dur; t += 1 / 11.5) if (!r.chance(0.25)) add(out, lp(tick(r, { tone: r.range(2000, 2700), body: 900, tau: 0.004 }), 1800), t, r.range(0.15, 0.3));
    const w = inPlace(out.slice(0, sec(dur)), 'room', -14);
    return loopOf({ L: w.L.slice(0, sec(dur)), R: w.R.slice(0, sec(dur)) }, 0.4);
  });
  def('month_card_open', { ch: 2, lufs: -25, about: 'The month\'s report laid before you: a dispatch case unlatched and the card drawn out onto the table.' }, (r) => {
    const out = mono(1.1);
    for (const t of [0.002, 0.13]) { add(out, clang(r, { f0: 2100, dur: 0.2, tau: 0.03, partials: 5, bright: 0.6, click: 0.8 }), t, 0.35); add(out, thud(r, 180, 0.015, 0.6), t, 1); } // two latches
    add(out, swish(r, 0.3, { lo: 1100, hi: 4800, rough: 0.5, grain: 150 }), 0.32, 0.9);
    add(out, thud(r, 105, 0.035, 1.1), 0.61, 1);
    add(out, tick(r, { tone: 2100, body: 700, tau: 0.004 }), 0.612, 0.4);
    return desk(out, -15);
  });

  // ---- First Command: the staff notes of the first hour ------------------------------------------------------------
  def('note_arrives', { ch: 2, lufs: -27, about: 'A staff note arrives: a slip of paper slid onto the desk and a knuckle\'s tap to say "read me". For each lesson of the first hour.' }, (r) => {
    const out = mono(0.8);
    add(out, swish(r, 0.22, { lo: 1900, hi: 6000, rough: 0.4, grain: 300 }), 0, 0.8);
    add(out, knock(r, { f: [520, 930, 1600], tau: 0.014, click: 0.5 }), 0.3, 0.7);
    add(out, knock(r, { f: [540, 960, 1650], tau: 0.014, click: 0.5 }), 0.42, 0.55);
    return desk(out, -16);
  });
  def('note_done', { ch: 2, lufs: -24, about: 'A staff note stamped DONE: the stamp, and a small bright touch of the desk bell after it. The stamp alone is "stamp".' }, (r) => {
    const out = mono(1.6);
    add(out, thud(r, 62, 0.075, 1.3), 0.004, 1);
    add(out, knock(r, { f: [185, 330, 540], tau: 0.03, weight: 1, click: 0.6 }), 0.004, 0.8);
    add(out, mul(bp(white(sec(0.06), r), 1900, 0.8), decay(sec(0.06), 0.012, 0.0008)), 0.004, 0.9);
    add(out, bell(r, { f0: 2093, dur: 1.2, tau: 0.4, bright: 0.55, strike: 0.4 }), 0.2, 0.22);
    return inPlace(out, 'room', -17);
  });
}
