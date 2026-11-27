// Ruling 226, mark 4: the obvious order honours the card. The first thing a newcomer tries (pick up every ship,
// right-click the nearest enemy, start the clock) must win within a tenth of the card's chance, on every kind of fleet,
// whether the player attacks or defends. Better play may do better and poor play worse; the obvious order is never a
// trap. The seeds are fixed, so the test is exact from run to run; 100 seeds at each of three odds, their luck of the
// day spread evenly over the dice's run (tests/sea-luck.ts), so the test measures the field and not the seeds' luck.
// The staff fires on the same target the newcomer picks (the enemy nearest the middle of the fleet, ties going to the
// side's own left: sim.ts enemyNearestMiddle), so the plain order, Space alone and the computer's play give the same
// winner seed for seed; the last test here holds that.
import { describe, expect, it } from 'vitest';
import { runOut } from '../src/battle/core/clock';
import type { Side } from '../src/battle/core/types';
import { seaChance } from '../src/battle/sea/result';
import { SeaSim, enemyNearestMiddle } from '../src/battle/sea/sim';
import { PRACTICE } from '../src/battle/sea/practice';
import { plainTicket, type PlainFleet } from '../src/battle/sea/ticket';
import { evenLuck } from './sea-luck';

const SEEDS = 100;
const ODDS = [0.9, 1, 1.15];
const live = (u: { state: string }): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** Every ship of the player's picked up, and sent at the enemy piece nearest the middle of them. */
function obvious(sim: SeaSim, me: Side): void {
  const mine = sim.units.filter((u) => u.side === me && live(u));
  const target = enemyNearestMiddle(sim.units, me)!.id;
  expect(sim.command({ do: 'attack', units: mine.map((u) => u.id), target }, me)).toBe(true);
}

function honoured(name: string, att: PlainFleet[], def: PlainFleet[], air?: Partial<Record<Side, number>>): void {
  for (const me of ['att', 'def'] as Side[]) {
    it(`${name}, the player ${me === 'att' ? 'attacking' : 'defending'}`, () => {
      const rows = ODDS.map((odds) => {
        let won = 0;
        for (const seed of evenLuck(SEEDS)) {
          const sim = new SeaSim(plainTicket({ att, def, seed, odds, air, human: me }));
          obvious(sim, me);
          runOut(sim);
          if (sim.over!.winner === me) won++;
        }
        const card = me === 'att' ? seaChance(odds) : 1 - seaChance(odds);
        return { odds, played: won / SEEDS, card };
      });
      const report = rows.map((r) => `${r.odds}: ${r.played.toFixed(2)} vs ${r.card.toFixed(2)}`).join('; ');
      const gap = rows.reduce((s, r) => s + (r.played - r.card), 0) / rows.length;
      expect(Math.abs(gap), report).toBeLessThanOrEqual(0.1);
    }, 300_000);
  }
}

const GUNS_A: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 4, dd: 8 } };
const GUNS_B: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 4, dd: 12 } };
const CARRIERS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
const BATTLE_LINE: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } };

describe('the obvious order wins about as often as the card says (ruling 226, mark 4)', () => {
  honoured('the heavier battle line against the lighter', [GUNS_A], [GUNS_B]);
  honoured('the lighter battle line against the heavier', [GUNS_B], [GUNS_A]);
  honoured('carriers against a battle line', [CARRIERS], [BATTLE_LINE]);
  honoured('a battle line against carriers', [BATTLE_LINE], [CARRIERS]);
  honoured("with the coast's aircraft on the attacker's side", [GUNS_A], [GUNS_B], { att: 10 });
  honoured("with the coast's aircraft on the defender's side", [GUNS_A], [GUNS_B], { def: 15 });
});

// The practice actions, played exactly as a newcomer plays them (the second playtester's test, b402): pick up the whole
// fleet, right-click the enemy nearest its middle, Space; and Space alone. Each at the campaign's own reckoning of its
// fleets, 100 seeds, within a tenth of the staff's chance.
describe('the practice actions honour their cards, played as a newcomer plays them (b402)', () => {
  for (const p of PRACTICE) {
    it(p.name, () => {
      const card = seaChance(plainTicket({ att: p.att, def: p.def, air: p.air, seed: 1 }).reckoning.odds);
      const rate = (order: boolean): number => {
        let won = 0;
        for (const seed of evenLuck(100)) {
          const sim = new SeaSim(plainTicket({ att: p.att, def: p.def, air: p.air, seed, human: 'att' }));
          if (order) obvious(sim, 'att');
          runOut(sim);
          if (sim.over!.winner === 'att') won++;
        }
        return won / 100;
      };
      const plain = rate(true), alone = rate(false);
      const report = `card ${card.toFixed(2)}, plain order ${plain.toFixed(2)}, Space alone ${alone.toFixed(2)}`;
      expect(Math.abs(plain - card), report).toBeLessThanOrEqual(0.1);
      expect(Math.abs(alone - card), report).toBeLessThanOrEqual(0.1);
    }, 300_000);
  }
});

// Mark 4 by construction (b402): the newcomer's plain order and Space alone fight the very battle the computer fights,
// to the end: the same winner, the same ships sunk and burning, the same losses and the same result for the campaign,
// the chase after the break included (the player's ships run a beaten enemy down as the staff's do).
describe("the newcomer's plain order and Space alone are the staff's own play (b402)", () => {
  const grounds: [string, PlainFleet[], PlainFleet[], Partial<Record<Side, number>>?][] = [
    ['two battle lines', [GUNS_A], [GUNS_B]], ['carriers against a battle line', [CARRIERS], [BATTLE_LINE]],
    ["the coast's aircraft on the defender's side", [GUNS_A], [GUNS_B], { def: 15 }],
  ];
  for (const [name, att, def, air] of grounds) {
    for (const me of ['att', 'def'] as Side[]) {
      it(`${name}, the player ${me === 'att' ? 'attacking' : 'defending'}`, () => {
        for (const seed of evenLuck(20)) {
          const result = (mode: 'cpu' | 'plain' | 'alone'): unknown => {
            const sim = new SeaSim(plainTicket({ att, def, seed, odds: 1, air, human: mode === 'cpu' ? null : me }));
            if (mode === 'plain') obvious(sim, me);
            runOut(sim);
            const o = sim.over!;
            return { winner: o.winner, how: o.how, seconds: o.seconds, lost: o.lost, men: o.men, R: o.R, extra: o.extra };
          };
          const cpu = result('cpu');
          expect(result('plain'), `seed ${seed}, plain order`).toEqual(cpu);
          expect(result('alone'), `seed ${seed}, Space alone`).toEqual(cpu);
        }
      }, 120_000);
    }
  }
});
