// The paired measure for release 2's three ground ideas (Brian, 22 November 17:33Z: "lets do all three but make it where
// we can toggle it off if someone doesnt want it"; the design director, 18:36Z: "Each switch is measured paired within
// the tenth on 5 grounds, with off-identity"). Skipped unless LONG=1. THREE names the switches to measure, one or more,
// comma-separated (THREE=patrol; THREE=phones,bearers once they are in), every one listed in SWITCHES where left out;
// THREE_SEEDS the seeds a case (80 where left out).
//
// The cases: the relief measure's five grounds (tests/battle-land-relief.test.ts, its LONG block: plains, forest, hills,
// a city, a river), odds 1.0 and 1.4, and the player's side the attack, then the defence (his staff fighting every block
// of it, as the 2D page and the 3D game fight a side he hands to it; the other side the computer's), each seed fought
// twice on the same ticket: the switch off, then on. A line a case: the share of battles the attack wins off and on (a
// win: R at 1.2 or more, as the relief and join measures count it), the shift (on less off), and the mean battle end in
// battle seconds off and on, with what the switch's own note counts. The bar, every case's shift within the tenth, is
// checked from 40 seeds a case: under that, the table is only printed (THREE_SEEDS=4 to see that it runs).
//
// A switch is added by name to SWITCHES: what both battles of a pair get (both), what turns it on (on), and what the
// player does with it on his side, given the same in both (his order is refused while it is off) (orders, called
// before each step as k counts them), and an optional count for its line (note).
import { describe, expect, it } from 'vitest';
import type { FieldTicket, Side } from '../src/battle/core';
import { LandBattle, sampleTicket, type LandPlace, type LandUnit } from '../src/battle/land';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
const LONG = !!env?.env.LONG;

interface Switch {
  /** What both battles of a pair get, before the clock starts (e.g. hiding, which the patrol needs). */
  both?: (b: LandBattle, side: Side) => void;
  /** Turns the switch on, before the clock starts. */
  on: (b: LandBattle) => void;
  /** The player's own use of it on his side, the same in both battles of the pair: before each step, k counting them. */
  orders?: (b: LandBattle, side: Side, k: number) => void;
  /** What the switch did in a battle it was on in, counted for its line (e.g. how many patrols found a post). */
  note?: (b: LandBattle) => Record<string, number>;
}

const footFront = (b: LandBattle, side: Side): LandUnit | undefined =>
  b.units.filter((u) => u.side === side && !u.team && (u.kind === 'inf' || u.kind === 'mtn' || u.kind === 'cav' || u.kind === 'eng') && u.state !== 'gone' && u.state !== 'withdrawn')
    .sort((a, c) => c.y - a.y || a.id - c.id)[0];

const SWITCHES: Record<string, Switch> = {
  // Night patrol (spec C): both battles of a pair hide (the patrol needs it). The computer's attack always sends one;
  // his attack sends one by his order while he places his army, from his most forward block on foot (as the computer's
  // would go): the setup step is opened in both battles and closed at once, and off his order is refused.
  patrol: {
    both: (b, side) => { b.hideOn(); if (side === 'att') b.setupOn('att', true); },
    on: (b) => { b.patrol = true; },
    orders: (b, side, k) => {
      if (k !== 0 || side !== 'att' || b.setup !== 'att') return;
      const u = footFront(b, 'att');
      if (u) b.command({ do: 'land:patrol', unit: u.id }, 'att');
      b.command({ do: 'land:begin' }, 'att');
    },
    note: (b) => ({ sent: b.nightPatrol ? 1 : 0, found: b.nightPatrol?.found ? 1 : 0, caught: b.nightPatrol?.alerted ? 1 : 0, post: b.nightPatrol && b.nightPatrol.post !== null ? 1 : 0 }),
  },
};

/** The relief measure's grounds. */
const GROUNDS: [string, (seed: number, odds: number) => T][] = [
  ['plains', (seed, odds) => sampleTicket('plains', odds, { seed })],
  ['forest', (seed, odds) => sampleTicket('forest', odds, { seed })],
  ['hills', (seed, odds) => sampleTicket('hills', odds, { seed })],
  ['city', (seed, odds) => sampleTicket('urban', odds, { seed, town: 'city' })],
  ['river', (seed, odds) => sampleTicket('plains', odds, { seed, river: true })],
];
const WIN = 1.2;

/** One battle to its end: the player's side his staff's, the switch on or off. */
function fight(sw: Switch, make: () => T, side: Side, on: boolean): LandBattle {
  const t = make();
  t.att.human = side === 'att'; t.def.human = side === 'def';
  const b = new LandBattle(t);
  b.ai.att = b.ai.def = true;
  sw.both?.(b, side);
  if (on) sw.on(b);
  for (let k = 0; !b.over && k < 30000; k++) { sw.orders?.(b, side, k); b.step(); }
  return b;
}

describe('the three ground ideas, each paired on against off (a measure: LONG=1, THREE=name)', () => {
  it.runIf(LONG)('every ground, both odds, the player on either side: each switch moves the attack\'s wins within a tenth', () => {
    const names = (env?.env.THREE ?? Object.keys(SWITCHES).join(',')).split(',').map((s) => s.trim()).filter(Boolean);
    const unknown = names.filter((n) => !SWITCHES[n]);
    expect(unknown, `no such switch here: ${unknown.join(', ')} (known: ${Object.keys(SWITCHES).join(', ')})`).toEqual([]);
    const N = Math.max(1, Math.floor(Number(env?.env.THREE_SEEDS ?? 80)) || 80);
    const bad: string[] = [];
    for (const name of names) {
      const sw = SWITCHES[name];
      env?.stdout?.write(`\n${name}: ${N} seeds a case, paired off and on${N < 40 ? ' (under 40 seeds: printed only, the bar not checked)' : ''}\n`);
      for (const [ground, make] of GROUNDS) for (const odds of [1.0, 1.4]) for (const side of ['att', 'def'] as Side[]) {
        let w0 = 0, w1 = 0, end0 = 0, end1 = 0;
        const notes: Record<string, number> = {};
        for (let seed = 1; seed <= N; seed++) {
          const off = fight(sw, () => make(seed, odds), side, false), on = fight(sw, () => make(seed, odds), side, true);
          w0 += off.over!.R >= WIN ? 1 : 0; w1 += on.over!.R >= WIN ? 1 : 0;
          end0 += off.over!.seconds; end1 += on.over!.seconds;
          for (const [k, v] of Object.entries(sw.note?.(on) ?? {})) notes[k] = (notes[k] ?? 0) + v;
        }
        const shift = (w1 - w0) / N;
        const row = `${name} ${ground.padEnd(6)} odds ${odds.toFixed(1)} his ${side === 'att' ? 'attack ' : 'defence'}: the attack wins off ${(w0 / N).toFixed(3)}, on ${(w1 / N).toFixed(3)}, shift ${shift >= 0 ? '+' : ''}${shift.toFixed(3)}` +
          ` | mean end off ${(end0 / N).toFixed(1)} s, on ${(end1 / N).toFixed(1)} s` + (Object.keys(notes).length ? ` | ${Object.entries(notes).map(([k, v]) => `${k} ${v}`).join(', ')}` : '');
        env?.stdout?.write(`${row}\n`);
        if (N >= 40 && Math.abs(shift) > 0.1) bad.push(row);
      }
    }
    expect(bad, bad.join('\n')).toEqual([]);
  }, 7_200_000);
});
