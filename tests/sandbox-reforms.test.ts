// Earned reforms (the design director's one rule, 20 November 2026 05:33Z; sandbox stops 4 and 5 only): a reform
// fires only from its own cause in that nation's war, for any nation, never by date. The record's four dated reform
// events are not read at those stops. Stops 1 to 3 are as they were.
import { describe, expect, it } from 'vitest';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { createGame, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { airWarMonths, foughtMonths, heavyRaids, escortedRaids, bombingMonths, landMonths, noteBattle, noteRaid, reformsMonth } from '../src/campaign/reforms';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import type { Game } from '../src/campaign/types';

function grand(stop: number, seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, stop, true);
  return g;
}
const took = (g: Game, k: string, nat: string): number | undefined => g.flags[`event:reform:${k}:${nat}`];
/** Lays 'n' corps of 'nat' on the memorial wall, lost to 'by' in month 'turn'. */
function fall(g: Game, nat: string, by: string, n: number, turn: number): void {
  for (let i = 0; i < n; i++) g.stories.fallen.push({ id: -1, nat, name: `x${i}`, kind: 'inf', divs: 1, turn, reg: 0, how: 'destroyed', by, born: 1, home: 0, xp: 0 } as never);
}

describe('the record\'s four dated reforms', () => {
  it('stop 3: the Soviet reform fires on its month, as release 1 has it, and the counts are never kept', () => {
    const g = grand(3);
    setWar(g, 'GER', 'SOV');
    g.turn = 46;
    monthlyDiplomacy(g);
    expect(g.flags['event:red-army-1943']).toBe(46);
    expect(g.nations.SOV.quality).toBe(0.8);
    noteBattle(g, ['GER'], ['SOV']);
    expect(g.flags['cm:SOV']).toBeUndefined();
    expect(Object.keys(g.flags).some((k) => k.startsWith('event:reform:'))).toBe(false);
  });

  it('stop 5: none fires by date', () => {
    const g = grand(5);
    setWar(g, 'GER', 'SOV');
    setWar(g, 'GER', 'USA');
    for (const t of [46, 55, 57, 70]) { g.turn = t; monthlyDiplomacy(g); }
    for (const id of ['red-army-1943', 'luftwaffe-1944', 'allied-armies-1944', 'german-army-1944']) expect(g.flags[`event:${id}`]).toBeUndefined();
    expect(g.nations.SOV.quality).toBe(0.7);
    expect(g.nations.GER.quality).toBe(1.25);
  });
});

describe('an army learns (slow command, or a top attack below 14)', () => {
  it('at 24 months of land battles: the record\'s Soviet ratings', () => {
    const g = grand(5);
    setWar(g, 'GER', 'SOV');
    g.turn = 40;
    g.flags['cm:SOV'] = 23;
    reformsMonth(g);
    expect(took(g, 'learns', 'SOV')).toBeUndefined();
    g.flags['cm:SOV'] = 24;
    reformsMonth(g);
    expect(took(g, 'learns', 'SOV')).toBe(40);
    const n = g.nations.SOV;
    expect([n.quality, n.slowCommand, n.maxAttack, n.air]).toEqual([0.8, false, 14, 100]);
  });

  it('sooner, at 21, if it destroyed more corps than it lost over 6 months and holds half its factories', () => {
    const g = grand(5);
    setWar(g, 'GER', 'SOV');
    g.turn = 40;
    g.flags['cm:SOV'] = 21;
    fall(g, 'GER', 'SOV', 4, 37);
    fall(g, 'SOV', 'GER', 3, 38);
    reformsMonth(g);
    expect(took(g, 'learns', 'SOV')).toBe(40);
  });

  it('counts only months of land battle against a great power: a war with Finland teaches nothing new', () => {
    const g = grand(5);
    setWar(g, 'SOV', 'FIN');
    setWar(g, 'GER', 'SOV');
    g.turn = 4;
    noteBattle(g, ['SOV'], ['FIN']);
    expect(landMonths(g, 'SOV')).toBe(0);
    expect(landMonths(g, 'FIN')).toBe(1);
    expect(foughtMonths(g, 'SOV', 'FIN')).toBe(1);
    g.turn = 22;
    noteBattle(g, ['GER', 'FIN'], ['SOV']);
    noteBattle(g, ['GER'], ['SOV']);
    expect([landMonths(g, 'SOV'), landMonths(g, 'GER'), landMonths(g, 'FIN')]).toEqual([1, 1, 2]);
  });

  it('never at peace, and never by a war that has no battles', () => {
    const g = grand(5);
    g.flags['cm:SOV'] = 30;
    g.turn = 40;
    reformsMonth(g);
    expect(took(g, 'learns', 'SOV')).toBeUndefined();
  });
});

describe('an army comes of age (rated below 1.0, not one that learns)', () => {
  it('America at 16 months of land battles; Italy, having lost heart, never; the Soviet Union never', () => {
    const g = grand(5);
    setWar(g, 'GER', 'USA');
    setWar(g, 'ITA', 'UK');
    setWar(g, 'GER', 'SOV');
    g.turn = 50;
    g.flags['event:italy-loses-heart'] = 47;
    for (const n of ['USA', 'ITA']) g.flags[`cm:${n}`] = 16;
    g.flags['cm:SOV'] = 10;
    reformsMonth(g);
    expect(g.nations.USA.quality).toBe(1);
    expect(took(g, 'ofAge', 'ITA')).toBeUndefined();
    expect(took(g, 'ofAge', 'SOV')).toBeUndefined();
  });

  it('sooner, at 12, if it has destroyed as many corps as it lost', () => {
    const g = grand(5);
    setWar(g, 'GER', 'USA');
    g.turn = 50;
    g.flags['cm:USA'] = 12;
    fall(g, 'GER', 'USA', 2, 45);
    fall(g, 'USA', 'GER', 3, 46);
    reformsMonth(g);
    expect(took(g, 'ofAge', 'USA')).toBeUndefined();
    fall(g, 'GER', 'USA', 1, 49);
    reformsMonth(g);
    expect(took(g, 'ofAge', 'USA')).toBe(50);
  });
});

describe('an air force bleeds, and its enemies\' come of age', () => {
  it('six months of escorted day raids on its home ground; night raids never count toward it', () => {
    const g = grand(5);
    setWar(g, 'GER', 'USA');
    setWar(g, 'GER', 'UK');
    grantTech(g, 'USA', 'escorts');
    const home = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && r.fac > 0)!;
    for (let t = 40; t < 46; t++) { g.turn = t; noteRaid(g, 'UK', home, true); noteRaid(g, 'USA', home, false); }
    expect(escortedRaids(g, 'GER')).toBe(6);
    expect(heavyRaids(g, 'GER')).toBe(0);
    expect(bombingMonths(g, 'UK')).toBe(6);
    g.turn = 46;
    reformsMonth(g);
    expect(took(g, 'bleeds', 'GER')).toBe(46);
    expect(g.nations.GER.air).toBe(45);
  });

  it('backstop: 36 months of raids on its home ground, by day or by night, while an enemy holds heavy bombers', () => {
    const g = grand(5);
    setWar(g, 'GER', 'UK');
    const home = g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && r.fac > 0)!;
    g.turn = 18;
    noteRaid(g, 'UK', home, true);
    expect(heavyRaids(g, 'GER')).toBe(0);
    grantTech(g, 'UK', 'heavybombers');
    for (let t = 19; t < 54; t++) { g.turn = t; noteRaid(g, 'UK', home, true); }
    expect(heavyRaids(g, 'GER')).toBe(35);
    expect(escortedRaids(g, 'GER')).toBe(0);
    reformsMonth(g);
    expect(took(g, 'bleeds', 'GER')).toBeUndefined();
    g.turn = 54;
    noteRaid(g, 'UK', home, true);
    reformsMonth(g);
    expect(took(g, 'bleeds', 'GER')).toBe(54);
    expect(airWarMonths(g, 'UK')).toBe(37);
    expect(airWarMonths(g, 'GER')).toBe(37);
  });

  it('the largest output among those fighting the bled enemy doubles, the rest take half again, after 12 months fighting it', () => {
    const g = grand(5);
    setWar(g, 'GER', 'USA');
    setWar(g, 'GER', 'UK');
    g.turn = 55;
    g.flags['event:reform:bleeds:GER'] = 54;
    g.flags['cv:GER:USA'] = 12;
    g.flags['cv:GER:UK'] = 30;
    reformsMonth(g);
    expect(g.nations.USA.air).toBe(140);
    expect(g.nations.UK.air).toBe(90);
    expect(foughtMonths(g, 'UK', 'GER')).toBe(30);
  });

  it('backstop: 48 months bombing an enemy\'s home ground or being bombed on its own', () => {
    const g = grand(5);
    setWar(g, 'GER', 'UK');
    g.turn = 57;
    g.flags['bb:UK'] = 47;
    reformsMonth(g);
    expect(took(g, 'ofAgeAir', 'UK')).toBeUndefined();
    g.flags['bb:UK'] = 48;
    reformsMonth(g);
    expect(took(g, 'ofAgeAir', 'UK')).toBe(57);
  });
});

describe('the cadre is spent (rated 1.0 or more, once)', () => {
  it('when the corps it has lost reach 0.6 of its peak', () => {
    const g = grand(5);
    setWar(g, 'GER', 'SOV');
    g.turn = 50;
    g.flags['peak:GER'] = 100;
    fall(g, 'GER', 'SOV', 59, 45);
    reformsMonth(g);
    expect(took(g, 'cadre', 'GER')).toBeUndefined();
    fall(g, 'GER', 'SOV', 1, 49);
    reformsMonth(g);
    expect(g.nations.GER.quality).toBe(1.15);
    g.turn = 51;
    reformsMonth(g);
    expect(g.nations.GER.quality).toBe(1.15);
  });
});
