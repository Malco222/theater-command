// The fairness test for fleet actions (ruling 217): with the computer on both sides, the field must give the
// attacker the same chance the rules' dice give at the same odds, within a tenth, whatever the fleets are made of.
// The seeds are fixed, so the test is exact from run to run, and their luck of the day is spread evenly over the dice's
// run (tests/sea-luck.ts): plain seeds rolled lucky for the attacker and measured the seeds, not the field. The
// tolerance allows for the luck of 40 battles.
import { describe, expect, it } from 'vitest';
import { runOut } from '../src/battle/core/clock';
import { seaChance } from '../src/battle/sea/result';
import { SeaSim } from '../src/battle/sea/sim';
import { plainTicket, type PlainFleet } from '../src/battle/sea/ticket';
import type { Side } from '../src/battle/core/types';
import { evenLuck } from './sea-luck';

const RUNS = 40;
const SEEDS = evenLuck(RUNS);
const ODDS = [0.9, 1, 1.15];

function wins(att: PlainFleet[], def: PlainFleet[], odds: number, air?: Partial<Record<Side, number>>): number {
  let won = 0;
  for (const seed of SEEDS) {
    const sim = new SeaSim(plainTicket({ att, def, seed, odds, air, human: null }));
    runOut(sim);
    if (sim.over!.winner === 'att') won++;
  }
  return won / RUNS;
}

function fair(name: string, att: PlainFleet[], def: PlainFleet[], air?: Partial<Record<Side, number>>): void {
  it(name, () => {
    const gaps = ODDS.map((o) => ({ o, played: wins(att, def, o, air), dice: seaChance(o) }));
    const report = gaps.map((g) => `${g.o}: ${g.played.toFixed(2)} vs ${g.dice.toFixed(2)}`).join('; ');
    for (const g of gaps) expect(Math.abs(g.played - g.dice), report).toBeLessThanOrEqual(0.15);
    expect(gaps.reduce((s, g) => s + Math.abs(g.played - g.dice), 0) / gaps.length, report).toBeLessThanOrEqual(0.1);
  }, 120_000);
}

const GUNS_A: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 4, dd: 8 } };
const GUNS_B: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 4, dd: 12 } };
const CARRIERS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
const BATTLE_LINE: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } };
// A battle line against a fleet with no battleships: drawn up 700 m a row apart, each ship at its own speed, the line
// reached the fight a minute after its screen and won 5 times in 100 at even weight (b407).
const HEAVY_LINE: PlainFleet = { name: 'Atlantic Fleet', nat: 'USA', ships: { bb: 4, ca: 4, dd: 8 } };
const LIGHT_FLEET: PlainFleet = { name: '2nd Fleet', nat: 'ITA', ships: { ca: 7, dd: 25 } };

describe('fleet actions are as fair as the dice, on every kind of fleet', () => {
  fair('the heavier battle line attacking the lighter', [GUNS_A], [GUNS_B]);
  fair('the lighter battle line attacking the heavier', [GUNS_B], [GUNS_A]);
  fair('carriers attacking a battle line', [CARRIERS], [BATTLE_LINE]);
  fair('a battle line attacking carriers', [BATTLE_LINE], [CARRIERS]);
  fair('a battle line attacking a fleet with no battleships', [HEAVY_LINE], [LIGHT_FLEET]);
  fair('a fleet with no battleships attacking a battle line', [LIGHT_FLEET], [HEAVY_LINE]);
  fair("with the coast's aircraft on the attacker's side", [GUNS_A], [GUNS_B], { att: 10 });
  fair("with the coast's aircraft on the defender's side", [GUNS_A], [GUNS_B], { def: 15 });
});
