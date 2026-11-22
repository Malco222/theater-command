// The three ground ideas of release 2, each measured on against off (Brian, 22 November 17:33Z: "lets do all three but
// make it where we can toggle it off if someone doesnt want it"; the design director, 18:36Z: "Each switch is measured
// paired within the tenth on 5 grounds, with off-identity"). Skipped unless LONG=1. THREE names the switch to measure
// (THREE=bearers; several by commas; every switch in SWITCHES below when unset): on the five grounds of the relief
// measure (tests/battle-land-relief.test.ts), at odds 1.0 and 1.4, with the switch's army attacking and then defending,
// THREE_SEEDS seeds (80), each battle fought twice, the switch off and on (paired: the same seed, ground and odds), both
// staffs, no player orders. One line a case: the share of the battles the attack wins with the switch off and on, the
// shift (on less off), and the mean battle end (seconds) off and on, and whatever the switch counts. THREE_GROUND runs
// one ground (a measure split over machines); THREE_NAT sets the army. The bar, a shift within a tenth, is held at 40
// seeds or more: fewer are a run of the measure itself (at 4 seeds one battle won the other way moves a share by 0.25).
// The off-identity of each switch is its own test file's (tests/battle-land-bearers.test.ts for the bearers).
import { describe, expect, it } from 'vitest';
import type { FieldTicket, Side } from '../src/battle/core';
import { LandBattle, sampleTicket, type LandPlace } from '../src/battle/land';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;

/**
 * A switch measured, by its name in THREE: 'nat', the army that carries it in the measure (where it differs by army, the
 * one it differs most in; the other side is the field's default army); 'on', turning it on; 'both', anything set alike on
 * both battles of a pair, off and on; 'count', what to tally of each battle fought with it on, its words and its number.
 */
interface Switch { nat: string; on: (b: LandBattle) => void; both?: (b: LandBattle) => void; count?: [string, (b: LandBattle) => number] }
const SWITCHES: Record<string, Switch> = {
  // A. Field telephone lines (LandBattle.phones) goes here by its name, e.g.
  //    phones: { nat: 'GER', on: (b) => { b.phones = true; } },
  // B. Stretcher bearers (LandBattle.bearers): the Soviets, with the largest share of bearers (3.5%), against the default
  // army's 2.75%; tallied, the light cases back in the line a battle, in men.
  bearers: {
    nat: 'SOV',
    on: (b) => { b.bearers = true; },
    count: ['men back in the line', (b) => b.returns.reduce((a, r) => { const u = b.unit(r.unit)!; return a + (r.n * u.men0) / u.str0; }, 0)],
  },
  // C. Night patrol (LandBattle.patrol) goes here by its name, with hiding on in both battles of each pair, e.g.
  //    patrol: { nat: 'GER', on: (b) => { b.patrol = true; }, both: (b) => { b.hide = true; } },
};

const GROUNDS: [string, (seed: number, odds: number, nat: { att: string; def: string }) => T][] = [
  ['plains', (seed, odds, nat) => sampleTicket('plains', odds, { seed, nat })],
  ['forest', (seed, odds, nat) => sampleTicket('forest', odds, { seed, nat })],
  ['hills', (seed, odds, nat) => sampleTicket('hills', odds, { seed, nat })],
  ['city', (seed, odds, nat) => sampleTicket('urban', odds, { seed, town: 'city', nat })],
  ['river', (seed, odds, nat) => sampleTicket('plains', odds, { seed, river: true, nat })],
];

describe('the three ground ideas, each switch on against off (a measure: LONG=1)', () => {
  const LONG = !!env?.env.LONG;
  const names = (env?.env.THREE ?? Object.keys(SWITCHES).join(',')).split(',').map((x) => x.trim()).filter(Boolean);
  const seeds = Number(env?.env.THREE_SEEDS ?? 80), only = env?.env.THREE_GROUND;
  for (const name of names) {
    it.runIf(LONG)(`${name}: every ground, each side, each odds: the attack's win share on against off, and the battle's end`, () => {
      const sw = SWITCHES[name];
      expect(sw, `no switch '${name}' to measure here (${Object.keys(SWITCHES).join(', ')})`).toBeDefined();
      const army = env?.env.THREE_NAT ?? sw.nat;
      const rows: string[] = [], bad: string[] = [];
      env?.stdout?.write(`\n${name}: ${seeds} seeds a case, each fought with the switch off and on; ${army} attacking against the default army, then defending\n`);
      for (const [ground, make] of GROUNDS) {
        if (only && only !== ground) continue;
        for (const side of ['att', 'def'] as Side[]) for (const odds of [1.0, 1.4]) {
          const nat = side === 'att' ? { att: army, def: 'BLU' } : { att: 'RED', def: army };
          let w0 = 0, w1 = 0, e0 = 0, e1 = 0, tally = 0, n = 0;
          for (let seed = 1; seed <= seeds; seed++) {
            for (const on of [false, true]) {
              const b = new LandBattle(make(seed, odds, nat));
              b.ai.att = b.ai.def = true;
              sw.both?.(b);
              if (on) sw.on(b);
              for (let k = 0; !b.over && k < 30000; k++) b.step();
              const win = b.over!.R >= b.ticket.reckoning.even ? 1 : 0;
              if (on) { w1 += win; e1 += b.over!.seconds; tally += sw.count?.[1](b) ?? 0; } else { w0 += win; e0 += b.over!.seconds; }
            }
            n++;
          }
          const shift = (w1 - w0) / n;
          const row = `${name} ${ground.padEnd(6)} ${army} ${side === 'att' ? 'attacking' : 'defending'} odds ${odds.toFixed(1)}: `
            + `the attack wins off ${w0}/${n} (${(w0 / n).toFixed(3)}), on ${w1}/${n} (${(w1 / n).toFixed(3)}), shift ${shift >= 0 ? '+' : ''}${shift.toFixed(3)}; `
            + `mean end off ${(e0 / n).toFixed(1)} s, on ${(e1 / n).toFixed(1)} s`
            + (sw.count ? `; ${(tally / n).toFixed(1)} ${sw.count[0]} a battle` : '');
          rows.push(row);
          env?.stdout?.write(`${row}\n`);
          if (Math.abs(shift) > 0.1) bad.push(row);
        }
      }
      expect(rows.length).toBeGreaterThan(0);
      if (seeds >= 40) expect(bad, bad.join('\n')).toEqual([]);
    }, 7_200_000);
  }
});
