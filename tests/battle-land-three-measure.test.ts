// The three ground ideas of release 2, each measured on against off (Brian, 22 November 17:33Z: "lets do all three but
// make it where we can toggle it off if someone doesnt want it"; the design director, 18:36Z: "Each switch is measured
// paired within the tenth on 5 grounds, with off-identity"). Skipped unless LONG=1. THREE names the switches to measure,
// one or more, comma-separated (THREE=phones, THREE=bearers, THREE=patrol, THREE=phones,bearers), every one in SWITCHES
// where left out; a name not in SWITCHES fails, never silently passes. THREE_SEEDS the seeds a case (80 where left out).
// THREE_GROUND runs one ground (a measure split over machines); THREE_NAT sets the army of a switch measured by army.
//
// The cases: the relief measure's five grounds (tests/battle-land-relief.test.ts, its LONG block: plains, forest, hills,
// a city, a river), odds 1.0 and 1.4, and the attack, then the defence, each seed fought twice on the same ticket: the
// switch off, then on (paired: the same seed, ground and odds), both staffs fighting. 'The attack, then the defence' is
// the player's side (his flag on it, his staff fighting every block of it, as the 2D page and the 3D game fight a side he
// hands to it), or, for a switch that differs by army ('nat'), the side that army fights on, against the field's default
// army, with no player. A line a case: the share of battles the attack wins off and on (a win: the campaign's verdict,
// R at 1.2 or more, as the relief and join measures count it), the shift (on less off), the mean battle end in battle
// seconds off and on, and what the switch's own note counts, a battle. The bar, every case's shift within the tenth, is
// checked from 40 seeds a case: under that the table is only printed (THREE_SEEDS=4 to see that it runs; at 4 seeds one
// battle won the other way moves a share by 0.25). The off-identity of each switch is its own test file's.
//
// A switch is added by name to SWITCHES: what it needs on the field (ticket), what both battles of a pair get (both),
// what turns it on (on), what the player does with it on his side, the same in both (orders, before each step as k
// counts them; his order is refused while it is off), and an optional count for its line (note).
import { describe, expect, it } from 'vitest';
import type { FieldTicket, Side } from '../src/battle/core';
import { LandBattle, sampleTicket, withTeams, type LandPlace } from '../src/battle/land';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
const LONG = !!env?.env.LONG;
const SEEDS = Math.max(1, Math.floor(Number(env?.env.THREE_SEEDS ?? 80)) || 80);
const ONLY = env?.env.THREE_GROUND;

/** A switch as the measure fights it. */
interface Switch {
  /**
   * Where the switch differs by army: the army it is measured with (THREE_NAT sets another), on the attack and then the
   * defence against the field's default army, no player on either side. Left out: the field's own armies, and the side is
   * the player's.
   */
  nat?: string;
  /** The ticket both battles of a pair are fought on (what the switch needs on the field to do anything at all). */
  ticket?: (t: T) => T;
  /** What both battles of a pair get, before the clock starts (e.g. hiding, which the patrol needs). */
  both?: (b: LandBattle, side: Side) => void;
  /** The switch itself: set on the 'on' battle alone, before the clock starts. */
  on: (b: LandBattle) => void;
  /** The player's own use of it on his side, the same in both battles of the pair: before each step, k counting them. */
  orders?: (b: LandBattle, side: Side, k: number) => void;
  /** What it did in an 'on' battle, counted, summed over a case and printed beside it, a battle. */
  note?: (b: LandBattle) => Record<string, number>;
}

const SWITCHES: Record<string, Switch> = {
  // A. Field telephone lines (LandBattle.phones). The practice armies carry no guns, so both battles of a pair carry the
  // support teams: the attack's mortar team is what cuts a wire with a blow (its tanks cut by crossing one). The wires
  // are the defence's.
  phones: {
    ticket: (t) => withTeams(t),
    on: (b) => { b.phones = true; },
    note: (b) => ({ wires: b.wires.length, cuts: b.wireCuts.length, mends: b.wireMends.length, held: b.wireHeld.length }),
  },
  // B. Stretcher bearers (LandBattle.bearers): the Soviets, with the largest share of bearers (3.5%), against the default
  // army's 2.75%; tallied, the light cases back in the line, in men.
  bearers: {
    nat: 'SOV',
    on: (b) => { b.bearers = true; },
    note: (b) => ({ 'men back in the line': b.returns.reduce((a, r) => { const u = b.unit(r.unit)!; return a + (r.n * u.men0) / u.str0; }, 0) }),
  },
};

/** The relief measure's five grounds, each with the armies' nations where a switch sets them. */
const GROUNDS: [string, (seed: number, odds: number, nat?: { att: string; def: string }) => T][] = [
  ['plains', (seed, odds, nat) => sampleTicket('plains', odds, { seed, ...(nat ? { nat } : {}) })],
  ['forest', (seed, odds, nat) => sampleTicket('forest', odds, { seed, ...(nat ? { nat } : {}) })],
  ['hills', (seed, odds, nat) => sampleTicket('hills', odds, { seed, ...(nat ? { nat } : {}) })],
  ['city', (seed, odds, nat) => sampleTicket('urban', odds, { seed, town: 'city', ...(nat ? { nat } : {}) })],
  ['river', (seed, odds, nat) => sampleTicket('plains', odds, { seed, river: true, ...(nat ? { nat } : {}) })],
];

/** One battle to its end, the switch on or off: both staffs fight, the player's side (where it has one) his staff's. */
function fight(sw: Switch, made: T, side: Side, on: boolean): LandBattle {
  let t = made;
  if (!sw.nat) t = { ...t, att: { ...t.att, human: side === 'att' }, def: { ...t.def, human: side === 'def' } };
  if (sw.ticket) t = sw.ticket(t);
  const b = new LandBattle(t);
  b.ai.att = b.ai.def = true;
  sw.both?.(b, side);
  if (on) sw.on(b);
  for (let k = 0; !b.over && k < 30000; k++) { sw.orders?.(b, side, k); b.step(); }
  return b;
}

describe('the three ground ideas, each switch paired on against off: the attack\'s win share moves within a tenth (a measure: LONG=1)', () => {
  const names = (env?.env.THREE ?? Object.keys(SWITCHES).join(',')).split(',').map((s) => s.trim()).filter(Boolean);
  for (const name of names) {
    it.runIf(LONG)(`${name}: every ground, each odds, the attack and the defence, ${SEEDS} seeds a case`, () => {
      const sw = SWITCHES[name];
      expect(sw, `no switch '${name}' to measure in this build (SWITCHES has ${Object.keys(SWITCHES).join(', ')})`).toBeDefined();
      const army = sw.nat ? env?.env.THREE_NAT ?? sw.nat : undefined;
      const rows: string[] = [], bad: string[] = [];
      env?.stdout?.write(`\n${name}: ${SEEDS} seeds a case, each fought with the switch off and on; ` +
        `${army ? `${army} attacking against the default army, then defending` : 'the player attacking, then defending'}` +
        `${SEEDS < 40 ? ' (under 40 seeds: printed only, the bar not checked)' : ''}\n`);
      for (const [ground, make] of GROUNDS) {
        if (ONLY && ONLY !== ground) continue;
        for (const odds of [1.0, 1.4]) for (const side of ['att', 'def'] as Side[]) {
          const nat = army ? (side === 'att' ? { att: army, def: 'BLU' } : { att: 'RED', def: army }) : undefined;
          let w0 = 0, w1 = 0, e0 = 0, e1 = 0;
          const notes: Record<string, number> = {};
          for (let seed = 1; seed <= SEEDS; seed++) {
            const a = fight(sw, make(seed, odds, nat), side, false), b = fight(sw, make(seed, odds, nat), side, true);
            w0 += a.over?.winner === 'att' ? 1 : 0; w1 += b.over?.winner === 'att' ? 1 : 0;
            e0 += a.over?.seconds ?? a.t; e1 += b.over?.seconds ?? b.t;
            for (const [k, v] of Object.entries(sw.note?.(b) ?? {})) notes[k] = (notes[k] ?? 0) + v;
          }
          const shift = (w1 - w0) / SEEDS;
          const who = army ? `${army} ${side === 'att' ? 'attacking' : 'defending'}` : `his ${side === 'att' ? 'attack ' : 'defence'}`;
          const row = `${name} ${ground.padEnd(6)} odds ${odds.toFixed(1)} ${who}: the attack wins off ${w0}/${SEEDS} (${(w0 / SEEDS).toFixed(3)}), ` +
            `on ${w1}/${SEEDS} (${(w1 / SEEDS).toFixed(3)}), shift ${shift >= 0 ? '+' : ''}${shift.toFixed(3)}` +
            ` | mean end off ${(e0 / SEEDS).toFixed(1)} s, on ${(e1 / SEEDS).toFixed(1)} s` +
            (Object.keys(notes).length ? ` | on, a battle: ${Object.entries(notes).map(([k, v]) => `${k} ${(v / SEEDS).toFixed(1)}`).join(', ')}` : '');
          rows.push(row);
          env?.stdout?.write(`${row}\n`);
          if (Math.abs(shift) > 0.1) bad.push(row);
        }
      }
      expect(rows.length).toBe((ONLY ? 1 : GROUNDS.length) * 2 * 2);
      if (SEEDS >= 40) expect(bad, bad.join('\n')).toEqual([]);
    }, 7_200_000);
  }
});
