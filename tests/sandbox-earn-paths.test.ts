// The earlier paths to the four reforms at the sandbox stops (the historian's thresholds, historian/sandbox-thresholds.md,
// section 1): what the war has done can earn a reform before its backstop of time at war. Stops 1 to 3 are as they were.
import { describe, expect, it } from 'vitest';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { createGame, setWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import { grantTech, hasTech } from '../src/campaign/tech';
import type { Game } from '../src/campaign/types';
import { combatMonths, corpsTrade, factoriesHeld, peakCorps } from '../src/campaign/warclock';

function grand(stop: number, seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, stop, true);
  return g;
}
const at = (g: Game, turn: number): void => {
  g.turn = turn;
  monthlyDiplomacy(g);
};
/** A corps of 'nat' lost to 'by' in month 'turn', on the memorial wall. */
const fall = (g: Game, nat: string, by: string, turn: number): void => {
  g.stories.fallen.push({ id: g.stories.fallen.length + 1, nat, name: `Test ${nat} ${turn}`, kind: 'inf', divs: 3, turn, reg: 0, how: 'destroyed', by, born: 1, home: -1, xp: 0.2 } as any);
};
function eastWar(stop: number, began: number): Game {
  const g = grand(stop);
  setWar(g, 'GER', 'SOV');
  g.flags['war:GER:SOV'] = began;
  return g;
}

describe('the counts the paths read', () => {
  it('factories held, corps traded, combat months and the peak', () => {
    const g = grand(5);
    expect(factoriesHeld(g, 'SOV')).toBe(1);
    const kiev = g.regions.find((r) => r.owner === 'SOV' && r.fac > 0)!;
    kiev.ctrl = 'GER';
    expect(factoriesHeld(g, 'SOV')).toBeLessThan(1);
    fall(g, 'GER', 'SOV', 40);
    fall(g, 'GER', 'SOV', 41);
    fall(g, 'SOV', 'GER', 41);
    expect(corpsTrade(g, 'SOV', 40)).toEqual({ lost: 1, destroyed: 2 });
    expect(corpsTrade(g, 'SOV', 41)).toEqual({ lost: 1, destroyed: 1 });
    expect(combatMonths(g, 'SOV')).toBe(2);
    startPhase(g, 'GER');
    expect(peakCorps(g, 'GER')).toBe(g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort').length);
    const s3 = grand(3);
    startPhase(s3, 'GER');
    expect(peakCorps(s3, 'GER')).toBe(0);
  });
});

describe('the army learns (red-army-1943)', () => {
  it('stop 5: 18 months into the German war, holding its factories and winning the trade of corps', () => {
    const g = eastWar(5, 30);
    for (const t of [45, 46, 47]) fall(g, 'GER', 'SOV', t);
    fall(g, 'SOV', 'GER', 46);
    at(g, 47);
    expect(g.flags['event:red-army-1943']).toBeUndefined();
    at(g, 48);
    expect(g.flags['event:red-army-1943']).toBe(48);
  });

  it('stop 5: losing the trade, it waits for the backstop at 24 months', () => {
    const g = eastWar(5, 30);
    for (const t of [45, 46, 47]) fall(g, 'SOV', 'GER', t);
    for (let t = 48; t < 54; t++) at(g, t);
    expect(g.flags['event:red-army-1943']).toBeUndefined();
    at(g, 54);
    expect(g.flags['event:red-army-1943']).toBe(54);
  });

  it('stop 3: the record\'s month, whatever the trade', () => {
    const g = eastWar(3, 22);
    for (const t of [40, 41, 42]) fall(g, 'GER', 'SOV', t);
    at(g, 43);
    expect(g.flags['event:red-army-1943']).toBeUndefined();
    at(g, 46);
    expect(g.flags['event:red-army-1943']).toBe(46);
  });
});

describe('the cadre is spent (german-army-1944)', () => {
  it('stop 5: corps lost reach half its peak, before 35 months of the eastern war', () => {
    const g = eastWar(5, 22);
    g.flags['peak:GER'] = 40;
    for (let i = 0; i < 19; i++) fall(g, 'GER', 'SOV', 30 + (i % 10));
    at(g, 40);
    expect(g.flags['event:german-army-1944']).toBeUndefined();
    fall(g, 'GER', 'SOV', 40);
    at(g, 41);
    expect(g.flags['event:german-army-1944']).toBe(41);
  });
});

describe('field armies come of age (allied-armies-1944)', () => {
  it('stop 5: 24 months of war and 15 months of fighting, or 29 months of war', () => {
    const g = grand(5);
    setWar(g, 'GER', 'USA');
    g.flags['war:GER:USA'] = 28;
    for (let t = 30; t < 45; t++) fall(g, 'GER', 'USA', t);
    at(g, 51);
    expect(g.flags['event:allied-armies-1944']).toBeUndefined();
    at(g, 52);
    expect(g.flags['event:allied-armies-1944']).toBe(52);
    const h = grand(5);
    setWar(h, 'GER', 'USA');
    h.flags['war:GER:USA'] = 28;
    at(h, 56);
    expect(h.flags['event:allied-armies-1944']).toBeUndefined();
    at(h, 57);
    expect(h.flags['event:allied-armies-1944']).toBe(57);
  });
});

describe('the air arm bleeds out (luftwaffe-1944)', () => {
  it('stop 5: 27 months of war with America, only once an enemy holds four-engined bombers', () => {
    const g = grand(5);
    setWar(g, 'GER', 'USA');
    g.flags['war:GER:USA'] = 28;
    expect(['USA', 'UK', 'FRA', 'SOV'].some((n) => hasTech(g, n, 'heavybombers'))).toBe(false);
    at(g, 55);
    expect(g.flags['event:luftwaffe-1944']).toBeUndefined();
    grantTech(g, 'USA', 'heavybombers');
    at(g, 56);
    expect(g.flags['event:luftwaffe-1944']).toBe(56);
  });
});
