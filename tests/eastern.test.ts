// The rules that make the eastern front different: distance, weather, the Red Army's
// mobilisation, limited manpower and the size of battle a staff can run.
import { describe, expect, it } from 'vitest';
import { EASTERN, addCorps, armySize, capture, createGame, frozen, limitAttackers, lineStrength, startPhase, weather } from '../src/campaign/game';
import { PRODUCTION } from '../src/campaign/economy';
import { loadGame, saveGame } from '../src/campaign/save';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const JUNE_41 = 22, AUGUST_41 = 24, OCTOBER_41 = 26, NOVEMBER_41 = 27, DECEMBER_41 = 28, MARCH_42 = 31;

/** One war on the historical plan, remembered at the months the tests look at. */
const war: Record<number, Game> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t <= OCTOBER_41; t++) {
    runTurnAI(g);
    if (t === JUNE_41 - 1 || t === JUNE_41 || t === AUGUST_41 || t === OCTOBER_41) war[t] = loadGame(saveGame(g))!;
  }
}

describe('weather in the east', () => {
  it('November is mud, December to February is winter, June is clear', () => {
    const g = createGame(1, null);
    g.turn = NOVEMBER_41;
    expect(weather(g)).toMatchObject({ mud: true, winter: false });
    g.turn = DECEMBER_41;
    expect(weather(g)).toMatchObject({ mud: false, winter: true });
    g.turn = JUNE_41;
    expect(weather(g)).toMatchObject({ mud: false, winter: false });
  });

  it('the thaw comes in April: in March the frozen ground still carries an army', () => {
    // Each season is counted from the month after it sets in, as the autumn mud is: a month's orders are given at its start.
    const g = createGame(1, null);
    g.turn = MARCH_42;
    expect(weather(g)).toMatchObject({ mud: false, winter: false });
    g.turn = MARCH_42 + 1;
    expect(weather(g)).toMatchObject({ mud: true, winter: false });
    expect(EASTERN.mud).toEqual([10, 3]);
  });

  it('winter bites armies without winter kit, and only on Soviet ground', () => {
    const g = createGame(1, null);
    g.turn = DECEMBER_41;
    expect(frozen(g, 'GER', g.byName['Smolensk'])).toBe(true);
    expect(frozen(g, 'ROM', g.byName['Smolensk'])).toBe(true);
    expect(frozen(g, 'SOV', g.byName['Smolensk'])).toBe(false);
    expect(frozen(g, 'FIN', g.byName['Leningrad'])).toBe(false);
    expect(frozen(g, 'GER', g.byName['Warsaw'])).toBe(false);
    g.turn = JUNE_41;
    expect(frozen(g, 'GER', g.byName['Smolensk'])).toBe(false);
  });
});

describe('the supply line', () => {
  it('is at full strength at home and thins out deep inside the Soviet Union', () => {
    const g = war[OCTOBER_41];
    const ger = g.corps.filter((c) => c.nat === 'GER');
    // Corps overseas (the Afrika Korps in Libya) hang on a sea line of their own; see tests/desert.test.ts.
    const home = ger.filter((c) => g.regions[c.reg].owner !== 'SOV' && !g.regions[c.reg].overseas);
    const deep = ger.filter((c) => g.regions[c.reg].owner === 'SOV');
    expect(deep.length).toBeGreaterThan(10);
    for (const c of home) expect(lineStrength(g, c).mul).toBe(1);
    const thin = deep.filter((c) => lineStrength(g, c).mul < 1);
    expect(thin.length).toBeGreaterThan(5);
    // The further from the frontier, the weaker: no corps with a longer line is better off.
    // (A general with a head for logistics softens it, so compare corps that have none.)
    const byLength = deep.filter((c) => c.general === null).sort((a, b) => lineStrength(g, a).length - lineStrength(g, b).length);
    expect(byLength.length).toBeGreaterThan(5);
    for (let i = 1; i < byLength.length; i++) {
      expect(lineStrength(g, byLength[i]).mul).toBeLessThanOrEqual(lineStrength(g, byLength[i - 1]).mul);
    }
  });

  it('never weakens the Red Army on its own soil', () => {
    const g = war[OCTOBER_41];
    for (const c of g.corps) if (c.nat === 'SOV') expect(lineStrength(g, c).mul).toBe(1);
  });
});

describe('the Red Army mobilises', () => {
  it('raises rifle corps from the month it is attacked, and not before', () => {
    const before = war[JUNE_41 - 1], after = war[JUNE_41];
    expect(before.flags['mobilised:SOV']).toBeUndefined();
    expect(after.flags['mobilised:SOV']).toBe(JUNE_41);
    const rifle = (g: Game): number => g.corps.filter((c) => c.nat === 'SOV' && c.name.endsWith(EASTERN.mobilisation.SOV.name)).length;
    expect(rifle(before)).toBe(0);
    expect(rifle(after)).toBeGreaterThan(0);
    expect(rifle(war[AUGUST_41])).toBeGreaterThan(rifle(after));
    expect(after.headlines.some((h) => /general mobilisation/.test(h.text) && h.nat === 'SOV')).toBe(true);
  });

  it('digs defence lines around Leningrad and then Moscow', () => {
    expect(war[JUNE_41].byName['Leningrad'].fort?.lv).toBe(2);
    expect(war[AUGUST_41].byName['Leningrad'].fort?.lv).toBe(3);
    expect(war[AUGUST_41].byName['Moscow'].fort?.lv).toBe(1);
    expect(war[OCTOBER_41].byName['Moscow'].fort?.lv).toBe(2);
  });
});

describe('manpower', () => {
  it('a mauled army gets back only a few men in a hundred each month, however rich the treasury', () => {
    const g = loadGame(saveGame(war[JUNE_41 - 1]))!;
    const ger = g.nations.GER;
    const army = g.corps.filter((c) => c.nat === 'GER');
    for (const c of army) c.str = 0.5;
    ger.pp = 100000;
    const was = army.reduce((s, c) => s + c.str * c.divs, 0);
    g.turn++;
    startPhase(g, 'GER');
    const got = army.filter((c) => !c.dead).reduce((s, c) => s + c.str * c.divs, 0) - was;
    const cap = PRODUCTION.manpower * Math.max(ger.armyPeak, armySize(g, 'GER'));
    expect(got).toBeGreaterThan(0);
    expect(got).toBeLessThanOrEqual(cap + 1e-6);
    expect(got).toBeLessThan(0.1 * was);
  });
});

describe('the size of battle a staff can run', () => {
  it('the Red Army of 1941 brings no more than eight corps into one battle; the Germans as many as the roads carry', () => {
    const g = createGame(1, null);
    const target = g.byName['Smolensk'];
    const around = target.nb.map((i) => g.regions[i]).slice(0, 3);
    expect(around.length).toBe(3);
    const raise = (nat: string) => around.flatMap((r) => [1, 2, 3, 4].map((k) => addCorps(g, nat, `${nat} test ${r.name} ${k}`, 'inf', 3, r.id, {})));
    expect(g.nations.SOV.maxAttack).toBe(8);
    expect(limitAttackers(g, raise('SOV'), target).length).toBe(8);
    expect(limitAttackers(g, raise('GER'), target).length).toBeGreaterThan(8);
  });
});

describe('ground that changes hands', () => {
  it('remembers who held it before, and a saved game keeps that', () => {
    const g = loadGame(saveGame(war[JUNE_41]))!;
    const minsk = g.byName['Minsk'];
    const holder = minsk.ctrl;
    capture(g, minsk, holder === 'SOV' ? 'GER' : 'SOV');
    expect(minsk.prev).toBe(holder);
    const back = loadGame(saveGame(g))!;
    expect(back.byName['Minsk'].prev).toBe(holder);
    expect(back.byName['Minsk'].ctrl).toBe(minsk.ctrl);
  });
});
