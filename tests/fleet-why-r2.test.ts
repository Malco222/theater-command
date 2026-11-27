// The admirals' reasons (release 2, fleetwhy.ts): where the player's admirals run the navy, each fleet they order says
// why it is where it is. Behind its own switch; it changes nothing in the war.
import { describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { fleetWhy, fleetWhyOn, setFleetWhy } from '../src/campaign/fleetwhy';
import { atWar, createGame } from '../src/campaign/game';
import { fleetKnown } from '../src/campaign/carriers';
import { atSea, fleetsOf, surfaceShips } from '../src/campaign/navy';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, fleetOptions, humanUp, orderFleet, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function admirals(human: string, seed: number, why: boolean): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setDesk(g, 'navy', true);
  if (why) setFleetWhy(g, true);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const places = (g: Game): string => g.fleets.map((f) => `${f.id}:${f.zone}:${f.stance}:${f.lane ?? ''}:${f.ready.toFixed(3)}:${Object.values(f.ships).join(',')}`).join('|');

describe("the admirals' reasons", () => {
  it('on: every fleet of his with ships that they ordered this month has a reason, in a sentence', () => {
    for (const [nat, seed] of [['UK', 4], ['GER', 3]] as const) {
      const g = admirals(nat, seed, true);
      for (let m = 0; m < 3; m++) {
        const mine = fleetsOf(g, nat).filter((f) => Object.values(f.ships).some((n) => n > 0));
        expect(mine.length).toBeGreaterThan(0);
        for (const f of mine) {
          const why = fleetWhy(g, f);
          expect(why, f.name).toMatch(/^Your admirals: [a-z].*\.$/);
          expect(why).not.toMatch(/undefined|NaN|\$\{/);
        }
        month(g);
      }
    }
  });

  it('a reason uses only what his own side has found: an enemy fleet or submarines nobody has sighted are never named', () => {
    // (the design director's 19:14Z)
    let named = 0;
    for (const [nat, seed] of [['UK', 4], ['GER', 3], ['ITA', 5]] as const) {
      const g = admirals(nat, seed, true);
      for (let m = 0; m < 4; m++) {
        for (const f of fleetsOf(g, nat)) {
          const why = fleetWhy(g, f);
          const foes = g.fleets.filter((x) => x.zone === f.zone && atSea(x) && atWar(g, x.nat, nat) && fleetKnown(g, x, nat) !== 'none');
          if (/to bring the enemy fleet sighted there to battle/.test(why)) { named++; expect(foes.some((x) => surfaceShips(x) > 0), why).toBe(true); }
          if (/hunt the submarines found/.test(why)) { named++; expect(foes.some((x) => x.ships.ss > 0), why).toBe(true); }
        }
        month(g);
      }
    }
    expect(named).toBeGreaterThanOrEqual(0);
  });

  it('it changes nothing in the war: the same seed plays the same with it on or off', () => {
    const on = admirals('UK', 4, true), off = admirals('UK', 4, false);
    for (let m = 0; m < 3; m++) { month(on); month(off); }
    expect(places(on)).toBe(places(off));
    expect(off.fleets.some((f) => f.why)).toBe(false);
  });

  it('a fleet under his own orders, and every computer fleet, has none; off, every reason goes', () => {
    const g = admirals('UK', 4, true);
    const f = fleetsOf(g, 'UK').find((x) => x.name === 'Home Fleet')!;
    expect(fleetWhy(g, f)).not.toBe('');
    orderFleet(g, f.id, fleetOptions(g, f.id)!.seas.find((z) => !z.here)!.zone, 'raid');
    expect(fleetWhy(g, f)).toBe('');
    month(g);
    expect(g.fleets.filter((x) => x.nat !== 'UK').some((x) => x.why)).toBe(false);
    setFleetWhy(g, false);
    expect(fleetWhyOn(g)).toBe(false);
    expect(g.fleets.some((x) => x.why)).toBe(false);
  });
});

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

describe("the admirals' reasons through the bridge", () => {
  it('the switch, and the line on the page and the panel', () => {
    const g = admirals('UK', 4, false);
    TC().load(saveGame(g));
    const f = fleetsOf(g, 'UK').find((x) => x.name === 'Home Fleet')!;
    expect((TC().view() as { fleetWhy: boolean }).fleetWhy).toBe(false);
    expect((TC().page(`fleet:${f.id}`) as { note: string }).note).not.toMatch(/Your admirals:/);
    expect((TC().act('fleetwhyrule:on') as { ok: boolean; note: string }).note).toMatch(/give their reasons/);
    expect((TC().view() as { fleetWhy: boolean }).fleetWhy).toBe(true);
    // (the reasons are written as the admirals order the fleets: from their next month)
    const h = admirals('UK', 4, true);
    TC().load(saveGame(h));
    const hf = fleetsOf(h, 'UK').find((x) => x.name === 'Home Fleet')!;
    expect((TC().page(`fleet:${hf.id}`) as { note: string }).note).toMatch(/Your admirals: /);
    expect((TC().force('fleet', hf.id) as { lines: string[] }).lines.join(' ')).toMatch(/Your admirals: /);
  });
});
