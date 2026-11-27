// The eastern front of 1944: an army kept to the letter of its orders, and what it may still do; an armistice
// that ends one war inside a bigger one; a country that changes sides with part of it still occupied; works and
// a harbour dug by the calendar on the main front; and armies that form behind a front too crowded to form on.
import { describe, expect, it } from 'vitest';
import type { Corps, Game, PlanPhase, Region } from '../src/campaign/types';
import { addCorps, atWar, capture, createGame, ensureSupply, removeCorps, startPhase } from '../src/campaign/game';
import { phaseFor } from '../src/campaign/ai';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';
import plans from '../data/campaign/plan-historical.json';

const JAN_44 = 53, FEB_44 = 54, MAY_44 = 57, JUNE_44 = 58, JULY_44 = 59, AUG_44 = 60, SEPT_44 = 61, OCT_44 = 62, NOV_44 = 63, DEC_44 = 64;
const MONTHS = [JAN_44, FEB_44, MAY_44, JUNE_44, JULY_44, AUG_44, SEPT_44, OCT_44, NOV_44, DEC_44];
const SOVIET = (plans as unknown as Record<string, PlanPhase[]>).SOV;

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, string> = {};
/** Every battle the Red Army opened in 1944, with the month it was fought. */
const soviet: { turn: number; place: string }[] = [];
{
  const g = createGame(3, null);
  let seen = 0;
  for (let t = 1; t < DEC_44; t++) {
    runTurnAI(g);
    if (t >= JAN_44) for (const r of g.reports.slice(seen)) if (r.attNat === 'SOV') soviet.push({ turn: t, place: r.regionName });
    seen = g.reports.length;
    if (MONTHS.includes(t + 1)) opening[t + 1] = saveGame(g);
  }
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(opening[t])!;
/** Lets the computer play until it is this nation's move. */
const until = (g: Game, nat: string): void => { let guard = 0; while (current(g) !== nat && guard++ < 60) stepAI(g); };
/** Plays this nation's month and stops there. */
const play = (g: Game, nat: string): void => { until(g, nat); stepAI(g); };
/** Opens this nation's month (arrivals, works) without giving any orders. */
const open = (g: Game, nat: string): void => { until(g, nat); g.week = 1; startPhase(g, nat); };

const R = (g: Game, name: string): Region => g.byName[name];
const named = (g: Game, re: RegExp): Corps[] => g.corps.filter((c) => !c.dead && re.test(c.name));
const told = (g: Game, re: RegExp): number[] => g.headlines.filter((h) => re.test(h.text)).map((h) => h.turn);
/** Empties a place and leaves it to a nation. */
const give = (g: Game, name: string, nat: string): void => { for (const c of [...R(g, name).units]) removeCorps(g, c); capture(g, R(g, name), nat); };
/** Mogilev in May 1944: German-held, with three Soviet armies next to it, and not in the month's orders. */
const offPlan = (g: Game): Region => {
  const r = R(g, 'Mogilev');
  expect(r.ctrl).toBe('GER');
  expect(phaseFor(g, 'SOV')!.targets).not.toContain('Mogilev');
  expect(r.nb.filter((j) => g.regions[j].units.some((u) => u.nat === 'SOV' && !u.hold && u.kind !== 'fort')).length).toBeGreaterThanOrEqual(2);
  return r;
};
const empty = (g: Game, r: Region): void => { for (const c of [...r.units]) removeCorps(g, c); };

describe('an army kept to the letter of its orders', () => {
  it('the Red Army of 1944 is on such a plan every month of the year, and so is the German army in front of it', () => {
    for (const t of MONTHS) {
      const g = at(t);
      expect(phaseFor(g, 'SOV')).toMatchObject({ posture: 'attack', strict: true });
      expect(phaseFor(g, 'GER')).toMatchObject({ posture: 'defend', strict: true, quiet: true });
    }
  });

  it('it attacks only the places the month\'s orders name', () => {
    expect(soviet.length).toBeGreaterThanOrEqual(20);
    for (const f of soviet) {
      const ph = SOVIET.find((p) => p.from <= f.turn && p.to >= f.turn && p.posture === 'attack')!;
      expect(ph.targets, `${f.place} in month ${f.turn}`).toContain(f.place);
    }
  });

  it('it steps into no ground the enemy has left empty beyond them', () => {
    const g = at(MAY_44);
    until(g, 'SOV');
    const r = offPlan(g);
    empty(g, r);
    stepAI(g);
    expect(r.ctrl).toBe('GER');
    expect(r.units.length).toBe(0);
  });

  it('the same army without that order walks straight in', () => {
    const g = at(MAY_44);
    until(g, 'SOV');
    const r = offPlan(g);
    empty(g, r);
    const kept = SOVIET.filter((p) => p.strict);
    for (const p of kept) delete p.strict;
    try {
      stepAI(g);
      expect(r.ctrl).toBe('SOV');
    } finally {
      for (const p of kept) p.strict = true;
    }
  });

  it('but an island the enemy has left altogether behind the front is taken back', () => {
    const g = at(MAY_44);
    until(g, 'SOV');
    // A place deep in the Soviet rear, handed to Germany with nobody in it.
    const island = g.regions.find((r) => r.ctrl === 'SOV' && r.owner === 'SOV' && !(phaseFor(g, 'SOV')!.targets ?? []).includes(r.name) && r.nb.every((j) => g.regions[j].ctrl === 'SOV') && r.nb.some((j) => g.regions[j].units.some((u) => u.nat === 'SOV' && !u.hold && u.kind !== 'fort')))!;
    give(g, island.name, 'GER');
    stepAI(g);
    expect(island.ctrl).toBe('SOV');
  });

  it('it makes no counter-attack, and the German army makes no attack at all', () => {
    const g = at(JUNE_44);
    const seen = g.reports.length;
    play(g, 'GER');
    expect(g.reports.slice(seen).filter((r) => r.attNat === 'GER' && r.defNat === 'SOV')).toEqual([]);
  });

  it('against a player it is not bound: ground a human Germany leaves empty is walked into', () => {
    const g = at(MAY_44);
    until(g, 'SOV');
    const r = offPlan(g);
    empty(g, r);
    g.human = 'GER';
    stepAI(g);
    expect(r.ctrl).toBe('SOV');
  });
});

describe('an armistice inside a bigger war', () => {
  it('Finland is at war until September 1944 and at peace from then, and the news says so once', () => {
    expect(atWar(at(AUG_44), 'FIN', 'SOV')).toBe(true);
    const g = at(SEPT_44);
    expect(atWar(g, 'FIN', 'SOV')).toBe(false);
    expect(atWar(g, 'SOV', 'FIN')).toBe(false);
    expect(told(g, /Finland is out of the war/)).toEqual([SEPT_44]);
    // Finland is still a country, and Germany's war goes on.
    expect(g.nations.FIN.alive).toBe(true);
    expect(atWar(g, 'GER', 'SOV')).toBe(true);
  });

  it('the frontier of 1940 is signed over, ground the Red Army already held with the rest', () => {
    const before = at(AUG_44);
    expect(R(before, 'Karelia')).toMatchObject({ owner: 'FIN', ctrl: 'SOV' });
    expect(R(before, 'Petsamo')).toMatchObject({ owner: 'FIN', ctrl: 'FIN' });
    const g = at(SEPT_44);
    expect(R(g, 'Karelia')).toMatchObject({ owner: 'SOV', ctrl: 'SOV' });
    expect(R(g, 'Petsamo')).toMatchObject({ owner: 'SOV', ctrl: 'SOV' });
  });

  it('each side goes home: nobody stands on the other\'s soil, and nothing of Finland\'s is still occupied', () => {
    const g = at(SEPT_44);
    for (const r of g.regions) {
      if (r.owner === 'FIN') expect(r.ctrl, r.name).toBe('FIN');
      if (r.ctrl === 'FIN') expect(r.units.some((u) => u.nat === 'SOV'), r.name).toBe(false);
      if (r.ctrl === 'SOV') expect(r.units.some((u) => u.nat === 'FIN'), r.name).toBe(false);
    }
  });

  it('the Red Army does not attack Finland again', () => {
    const g = at(SEPT_44);
    const seen = g.reports.length;
    play(g, 'SOV');
    expect(g.reports.slice(seen).filter((r) => r.defNat === 'FIN')).toEqual([]);
  });

  it('a saved game keeps the peace', () => {
    const g = loadGame(saveGame(at(SEPT_44)))!;
    expect(atWar(g, 'FIN', 'SOV')).toBe(false);
    expect(g.flags['event:finland-armistice']).toBe(SEPT_44);
  });

  it('there is no armistice while Finland still holds the isthmus', () => {
    const g = at(JUNE_44);
    expect(R(g, 'Karelia').ctrl).toBe('FIN');
    g.turn = SEPT_44;
    monthlyDiplomacy(g);
    expect(atWar(g, 'FIN', 'SOV')).toBe(true);
    expect(g.flags['event:finland-armistice']).toBeUndefined();
  });
});

describe('a country that changes sides', () => {
  it('Romania is gone from the Axis as September 1944 opens, and its army with it', () => {
    const before = at(AUG_44);
    expect(before.nations.ROM.alive).toBe(true);
    expect(before.corps.filter((c) => c.nat === 'ROM' && !c.dead).length).toBeGreaterThanOrEqual(8);
    const g = at(SEPT_44);
    expect(g.nations.ROM.alive).toBe(false);
    expect(g.corps.filter((c) => c.nat === 'ROM' && !c.dead)).toEqual([]);
    expect(told(g, /King Michael/)).toEqual([AUG_44]);
  });

  it('the country goes to the Red Army', () => {
    const g = at(SEPT_44);
    for (const nm of ['Bucharest', 'Ploiesti', 'Dobruja', 'Oltenia', 'Banat', 'Moldavia']) expect(R(g, nm).ctrl, nm).toBe('SOV');
  });

  it('except the ground German troops are still standing on, which stays theirs to be fought for', () => {
    const g = at(AUG_44);
    addCorps(g, 'GER', 'Ploiesti flak division', 'inf', 1, 'Ploiesti', { hold: 1 });
    runTurnAI(g);
    expect(g.nations.ROM.alive).toBe(false);
    expect(R(g, 'Ploiesti').ctrl).toBe('GER');
    expect(R(g, 'Bucharest').ctrl).toBe('SOV');
  });

  it('two Romanian armies and a Bulgarian one march with the Red Army from then on', () => {
    const g = at(SEPT_44);
    expect(named(g, /^Romanian (1st|4th) Army/).map((c) => c.nat)).toEqual(['SOV', 'SOV']);
    expect(named(g, /^Bulgarian 1st Army/).map((c) => [c.nat, g.regions[c.reg].name])).toEqual([['SOV', 'Sofia']]);
    expect(g.nations.BUL.alive).toBe(false);
    // They form as garrisons and the month's orders set them marching.
    expect(named(g, /^Romanian (1st|4th) Army/).every((c) => c.hold)).toBe(true);
    play(g, 'SOV');
    expect(named(g, /^Romanian (1st|4th) Army/).some((c) => c.hold)).toBe(false);
  });

  it('none of it happens while the Red Army is still held on the Pruth', () => {
    const g = at(JULY_44);
    expect(R(g, 'Moldavia').ctrl).toBe('ROM');
    g.turn = SEPT_44;
    monthlyDiplomacy(g);
    expect(g.nations.ROM.alive).toBe(true);
    expect(g.nations.BUL.alive).toBe(true);
  });
});

describe('works dug by the calendar on the main front', () => {
  it('the Panther line goes up in February 1944 at Pskov and on the Narva, and the news says so', () => {
    const g = at(FEB_44);
    expect(R(g, 'Pskov').fort?.lv ?? 0).toBeLessThan(2);
    open(g, 'GER');
    expect(R(g, 'Pskov').fort?.lv).toBe(2);
    expect(R(g, 'Estonia').fort).toMatchObject({ lv: 2, vs: ['SOV'] });
    expect(told(g, /Panther line/).filter((t) => t === FEB_44).length).toBe(2);
  });

  it('nothing is dug at a place its builder has lost', () => {
    const g = at(FEB_44);
    give(g, 'Pskov', 'SOV');
    open(g, 'GER');
    expect(R(g, 'Pskov').fort?.lv ?? 0).toBeLessThan(2);
  });

  it('Courland gets its works and a harbour in October, so the army shut in there is fed by sea', () => {
    const g = at(OCT_44);
    expect(g.flags[`harbour:${R(g, 'Courland').id}`]).toBeUndefined();
    open(g, 'GER');
    expect(R(g, 'Courland').fort?.lv).toBe(1);
    expect(g.flags[`harbour:${R(g, 'Courland').id}`]).toBe(OCT_44);
    expect(R(g, 'Courland').port).toBe(true);
    const later = at(DEC_44);
    ensureSupply(later);
    const pocket = R(later, 'Courland').units.filter((u) => u.nat === 'GER');
    expect(pocket.length).toBeGreaterThanOrEqual(2);
    expect(R(later, 'Courland').nb.every((j) => later.regions[j].ctrl !== 'GER')).toBe(true);
    expect(pocket.every((u) => u.supplied)).toBe(true);
  });

  it('a saved game keeps the harbour', () => {
    const g = loadGame(saveGame(at(NOV_44)))!;
    expect(R(g, 'Courland').port).toBe(true);
  });

  it('Budapest is fortified in November', () => {
    const g = at(NOV_44);
    open(g, 'HUN');
    open(g, 'GER');
    expect(R(g, 'Budapest').fort?.lv).toBe(2);
    expect(told(g, /Attila line/)).toEqual([NOV_44]);
  });

  it('in free play none of these works is dug', () => {
    const g = startOpen(createGame(5, null));
    g.turn = FEB_44;
    open(g, 'GER');
    expect(told(g, /Panther line/)).toEqual([]);
  });
});

describe('armies that form behind a crowded front', () => {
  it('the Polish 1st Army forms when Lublin is free, in the city or at the nearest place behind it with room', () => {
    const g = at(SEPT_44);
    const poles = named(g, /^Polish 1st Army/);
    expect(poles).toHaveLength(1);
    expect(poles[0]).toMatchObject({ nat: 'SOV', divs: 4, hold: true });
    const lublin = R(g, 'Lublin');
    const near = new Set([lublin.id, ...lublin.nb, ...lublin.nb.flatMap((j) => g.regions[j].nb), ...lublin.nb.flatMap((j) => g.regions[j].nb.flatMap((k) => g.regions[k].nb))]);
    expect(near.has(poles[0].reg)).toBe(true);
    expect(g.regions[poles[0].reg].ctrl).toBe('SOV');
    expect(told(g, /Polish committee/).length).toBe(1);
  });

  it('five more of the Red Army\'s front commanders take the field in 1944', () => {
    const names = (g: Game): string[] => g.generals.filter((x) => x.nat === 'SOV').map((x) => x.name);
    expect(names(at(JAN_44))).not.toContain('Ivan Bagramyan');
    for (const nm of ['Leonid Govorov', 'Rodion Malinovsky', 'Fyodor Tolbukhin']) expect(names(at(FEB_44))).toContain(nm);
    for (const nm of ['Ivan Bagramyan', 'Ivan Chernyakhovsky']) expect(names(at(JULY_44))).toContain(nm);
  });
});
