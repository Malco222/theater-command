// Version 68: critical fixes from the playtest of Version 66 and the measuring of Version 67 (the grand campaign).
//
// 1. Money without men. A building staff that can raise no corps for want of men puts the treasury into aircraft, as
//    far as the air force can use them, and the desk says so (depth.json 'build.machines'; desks.ts).
// 3. (At the foot.) The last levy: a Red Army that is being worn away comes back (traits.json 'levy'; traits.ts).

import { describe, expect, it } from 'vitest';
import { airFleetsOf } from '../src/campaign/air';
import { ARSENAL } from '../src/campaign/arsenal';
import { DEPTH, setDesk } from '../src/campaign/depth';
import { boughtAir, commandView, saidLastMonth, staffMonth, wingsWanted } from '../src/campaign/desks';
import { createGame, startPhase } from '../src/campaign/game';
import { MANPOWER, menLeft } from '../src/campaign/manpower';
import { startOpen } from '../src/campaign/staff';
import { TRAITS, fieldStrength, traitLevy, traitRule } from '../src/campaign/traits';
import { beginHuman, endHuman, humanUp, orderDesk, orderStaffCap, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A grand campaign brought to the player's first month, which is not yet opened. */
function ready(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  return g;
}
const lines = (g: Game, desk: string): string[] => g.headlines.filter((h) => h.desk === desk && h.turn === g.turn).map((h) => h.text);
const buildDesk = (g: Game): string => commandView(g)!.desks.find((d) => d.id === 'build')!.state;

describe('money without men', () => {
  /** Germany at war in September 1939 with building left to the staff, a full treasury and the reserve of men spent. */
  function spent(): Game {
    const g = ready('GER', 3);
    setDesk(g, 'build', true);
    startPhase(g, 'GER');
    g.nations.GER.pp = 400;
    g.flags['mp:GER'] = 0;
    return g;
  }

  it('with no men for corps a staff at war orders a wing of fighters and a wing of bombers, and says so', () => {
    const g = spent();
    expect(g.nations.GER.atWar.length).toBeGreaterThan(0);
    expect(wingsWanted(g, 'GER').sort()).toEqual(['bmr', 'ftr']);
    staffMonth(g);
    const q = g.nations.GER.queue;
    expect(q.filter((b) => b.what === 'corps').length).toBe(0);
    expect(q.filter((b) => b.what === 'air' && b.item === 'ftr').length).toBe(DEPTH.build!.machines!.each);
    expect(q.filter((b) => b.what === 'air' && b.item === 'bmr').length).toBe(DEPTH.build!.machines!.each);
    expect(lines(g, 'build').join(' ')).toMatch(/ordered .*(fighter|bomber) wing/i);
    expect(buildDesk(g)).toMatch(/no men to spare for new corps/);
    expect(buildDesk(g)).toMatch(/putting the money into aircraft/);
    // The staff does not send the player to the Build tab for what it is doing itself.
    expect(buildDesk(g)).not.toMatch(/ordered by hand/);
  });

  it('keeps so many wings on order at a time and no more', () => {
    const g = spent();
    staffMonth(g);
    const before = g.nations.GER.queue.filter((b) => b.what === 'air').length;
    // The same month again, as far as the staff's own rule goes: nothing more is wanted while the wings are forming.
    g.nations.GER.pp = 400;
    delete g.flags['dk:ran:build'];
    delete g.flags['dk:alwt'];
    staffMonth(g);
    expect(g.nations.GER.queue.filter((b) => b.what === 'air').length).toBe(before);
  });

  it('with men to spare it raises corps and buys no aircraft', () => {
    const g = ready('GER', 3);
    setDesk(g, 'build', true);
    startPhase(g, 'GER');
    g.nations.GER.pp = 400;
    expect(wingsWanted(g, 'GER')).toEqual([]);
    staffMonth(g);
    expect(g.nations.GER.queue.filter((b) => b.what === 'air').length).toBe(0);
    expect(g.nations.GER.queue.filter((b) => b.what === 'corps').length).toBeGreaterThan(0);
  });

  it('a country at peace buys none, however full the treasury', () => {
    const g = ready('SOV', 5);
    setDesk(g, 'build', true);
    startPhase(g, 'SOV');
    g.nations.SOV.pp = 400;
    g.flags['mp:SOV'] = 0;
    expect(g.nations.SOV.atWar.length).toBe(0);
    expect(wingsWanted(g, 'SOV')).toEqual([]);
    staffMonth(g);
    expect(g.nations.SOV.queue.filter((b) => b.what === 'air').length).toBe(0);
  });

  it('stops when what it has bought stands at its share of the establishment, and calls the rest a reserve', () => {
    const g = spent();
    const A = boughtAir(g, 'GER');
    expect(A.base.ftr).toBeGreaterThan(ARSENAL.air.ftr.count);
    expect(A.over).toEqual({ ftr: 0, bmr: 0 });
    // Bought aircraft are carried on a fleet as 'extra', over the establishment history gave it.
    const share = DEPTH.build!.machines!.share;
    const f = airFleetsOf(g, 'GER').find((x) => x.ship === undefined && x.est.ftr > 0)!;
    const b = airFleetsOf(g, 'GER').find((x) => x.ship === undefined && x.est.bmr > 0)!;
    const addF = Math.ceil(share * A.base.ftr), addB = Math.ceil(share * A.base.bmr);
    (f.extra ??= { ftr: 0, bmr: 0 }).ftr += addF; f.est.ftr += addF;
    (b.extra ??= { ftr: 0, bmr: 0 }).bmr += addB; b.est.bmr += addB;
    expect(boughtAir(g, 'GER').base).toEqual(A.base);
    expect(wingsWanted(g, 'GER')).toEqual([]);
    staffMonth(g);
    expect(g.nations.GER.queue.filter((x) => x.what === 'air').length).toBe(0);
    expect(buildDesk(g)).toMatch(/what is left in the treasury is your reserve/);
  });

  it('the enemy bombing the factories puts fighters first', () => {
    const g = spent();
    // Bombers bought already, fighters not: left alone the staff would even them out with fighters anyway, so tip it the other way.
    const f = airFleetsOf(g, 'GER').find((x) => x.ship === undefined && x.est.ftr > 0)!;
    (f.extra ??= { ftr: 0, bmr: 0 }).ftr += 100; f.est.ftr += 100;
    expect(wingsWanted(g, 'GER')[0]).toBe('bmr');
    g.raids.GER = { turn: g.turn, share: 0.1 };
    expect(wingsWanted(g, 'GER')[0]).toBe('ftr');
  });
});

// 2. A staff does not say the same thing every month. The envoys speak when they take up a country and when it moves;
//    any desk whose line is word for word last month's keeps quiet. A share of the treasury changed after the month's
//    allowance was drawn says that it takes hold next month.
describe('a staff that does not repeat itself', () => {
  /** The same with the first month open, as the game's own screen would have it. */
  function playing(human = 'GER', seed = 3): Game {
    const g = ready(human, seed);
    beginHuman(g);
    return g;
  }
  function month(g: Game): void {
    endHuman(g);
    for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
    beginHuman(g);
  }

  it('the envoys say when they begin courting a country, and after that only when it moves', () => {
    const g = playing('GER', 3);
    g.nations.GER.pc = 60;
    orderDesk(g, 'diplomacy', true);
    const all: string[] = [...lines(g, 'diplomacy')];
    expect(all[0]).toMatch(/^Your envoys began courting /);
    expect(commandView(g)!.desks.find((d) => d.id === 'diplomacy')!.state).toMatch(/Your envoys are courting /);
    let courted = 1;
    for (let k = 0; k < 8; k++) {
      month(g);
      g.nations.GER.pc = Math.max(g.nations.GER.pc, 20);
      all.push(...lines(g, 'diplomacy'));
      courted += Object.keys(g.flags).filter((f) => f.startsWith('court:GER:') && g.flags[f] === g.turn).length;
    }
    // They courted in most months, and spoke in fewer: once when they take up a country and once each time it moves
    // (on the chart with Latin America, Argentina moves twice in these months), and never the bare "courted X.".
    expect(courted).toBeGreaterThan(4);
    expect(all.length).toBeLessThan(courted);
    const began = all.filter((t) => /began courting /.test(t));
    expect(began).toEqual([...new Set(began)]);
    expect(began.length).toBeGreaterThan(1);
    for (const t of all) expect(t).toMatch(/began courting |which has moved a step /);
  });

  it('a desk whose line is last month\'s over again keeps quiet, and speaks again after a break', () => {
    const g = playing('GER', 3);
    expect(saidLastMonth(g, 'navy', 'shelled Estonia.')).toBe(false);
    g.turn++;
    expect(saidLastMonth(g, 'navy', 'shelled Estonia.')).toBe(true);
    g.turn++;
    expect(saidLastMonth(g, 'navy', 'shelled Estonia.')).toBe(true);
    g.turn++;
    expect(saidLastMonth(g, 'navy', 'shelled Latvia.')).toBe(false);
    g.turn++;
    expect(saidLastMonth(g, 'navy', 'shelled Estonia.')).toBe(false);
    // A month with nothing said, then the same line: that is news again.
    g.turn += 2;
    expect(saidLastMonth(g, 'navy', 'shelled Estonia.')).toBe(false);
    // Each desk keeps its own memory.
    g.turn++;
    expect(saidLastMonth(g, 'air', 'shelled Estonia.')).toBe(false);
  });

  it('a share changed after the month\'s allowance was drawn takes hold next month, and the view says so', () => {
    const g = playing('GER', 3);
    orderDesk(g, 'build', true);
    expect(commandView(g)!.capNext).toBe(false);
    const other = DEPTH.cap.steps.find((c) => c !== commandView(g)!.cap)!;
    const left = commandView(g)!.left;
    expect(orderStaffCap(g, other)).toBe(true);
    expect(commandView(g)!.capNext).toBe(true);
    expect(commandView(g)!.left).toBe(left);
    month(g);
    expect(commandView(g)!.capNext).toBe(false);
  });
});

// 3. The last levy. A careful German buyer wore the Red Army from 230 divisions to 115 in the year 1942, because the
//    Soviet reserve of men gave out. "Reserves without end" now means it: with the enemy on its soil, its reserve all
//    but spent and its army driven under so many divisions, the Soviet Union calls up every man fit to bear arms.
//    A Red Army that is in no trouble is no larger for it (traits.json 'levy'; traits.ts traitLevy).
describe('the last levy', () => {
  const T = TRAITS.nations.SOV.traits.find((t) => t.levy)!, L = T.levy!;
  /** The Soviet Union at war with Germany, a home province lost, its reserve of men spent. */
  function pressed(human: string | null = null, open = true): Game {
    const g = open ? startOpen(createGame(2, human)) : createGame(2, human);
    for (const [a, b] of [['GER', 'SOV'], ['SOV', 'GER']]) if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
    g.regions.find((r) => r.owner === 'SOV' && r.ctrl === 'SOV' && !r.overseas && !r.cap)!.ctrl = 'GER';
    g.flags['mp:SOV'] = 0;
    return g;
  }
  const levies = (g: Game): number => g.headlines.filter((h) => /calls up every man fit to bear arms/.test(h.text)).length;

  it('with the reserve spent and the army under the mark the men come, and the news says so', () => {
    const g = pressed();
    expect(fieldStrength(g, 'SOV')).toBeLessThan(L.under);
    expect(traitLevy(g, 'SOV')).toBe(L.men);
    expect(menLeft(g, 'SOV')).toBe(L.men);
    expect(levies(g)).toBe(1);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/^The Soviet Union calls up every man fit to bear arms: [\d.,]+ million more men for its armies\.$/);
  });

  it('never twice within the year, and only so many times in a war', () => {
    const g = pressed();
    let got = 0;
    for (let k = 0; k < L.most + 2; k++) {
      g.flags['mp:SOV'] = 0;
      got += traitLevy(g, 'SOV') > 0 ? 1 : 0;
      g.flags['mp:SOV'] = 0;
      g.turn += L.gap - 1;
      expect(traitLevy(g, 'SOV')).toBe(0);
      g.turn += 1;
    }
    expect(got).toBe(L.most);
    expect(levies(g)).toBe(L.most);
  });

  it('none while the reserve holds, the army stands over the mark, the land is whole or the country is at peace', () => {
    const held = pressed();
    held.flags['mp:SOV'] = Math.ceil(L.spent * MANPOWER.pool.SOV);
    expect(traitLevy(held, 'SOV')).toBe(0);

    const strong = pressed();
    strong.corps.find((c) => c.nat === 'SOV' && !c.dead && c.kind !== 'fort')!.divs += L.under;
    expect(fieldStrength(strong, 'SOV')).toBeGreaterThanOrEqual(L.under);
    expect(traitLevy(strong, 'SOV')).toBe(0);

    const whole = pressed();
    for (const r of whole.regions) if (r.owner === 'SOV') r.ctrl = 'SOV';
    expect(traitLevy(whole, 'SOV')).toBe(0);

    const peace = pressed();
    for (const n of Object.values(peace.nations)) n.atWar = n.atWar.filter((x) => x !== 'SOV' && n.id !== 'SOV');
    expect(traitLevy(peace, 'SOV')).toBe(0);
    for (const g of [held, strong, whole, peace]) expect(levies(g)).toBe(0);
  });

  it('it is the country\'s own ability: the player who leads it has it, Germany has not, and history\'s scenarios never see it', () => {
    expect(traitLevy(pressed('SOV'), 'SOV')).toBe(L.men);
    const g = pressed();
    g.flags['mp:GER'] = 0;
    g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.overseas && !r.cap)!.ctrl = 'SOV';
    expect(traitLevy(g, 'GER')).toBe(0);
    expect(traitLevy(pressed(null, false), 'SOV')).toBe(0);
  });

  it('the month\'s rules call it, so the men are there for the call-up and the depots', () => {
    const g = pressed();
    startPhase(g, 'SOV');
    expect(g.flags['lv:SOV']).toBe(1);
    expect(g.flags['lvt:SOV']).toBe(g.turn);
    expect(menLeft(g, 'SOV')).toBeGreaterThan(0);
  });

  it('the abilities card says what it does, from the rule\'s own numbers', () => {
    const words = traitRule(T);
    expect(words).toContain(`its army in the field under ${L.under} divisions`);
    expect(words).toMatch(/calls up every man fit to bear arms: [\d.,]+ million more men, three times in a war at most and never twice within a year\./);
  });
});
