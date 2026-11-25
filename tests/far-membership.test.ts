// One membership, the reading (the design director's ruling of 14:08Z of 21 November 2026, far side;
// src/campaign/staff.ts membership).
//
// As measured by hand on the plan's word's states: with the strike south gone, Hong Kong's front had no corps of
// its own, a defence of 4.5, and read 5.6 to one off the army in China, of which the follow-through would bring it
// none: each corps closes up to the front nearest it. One map was weighed and another fought.
//
// The ruling: a corps is a member of one front, the front nearest it by the follow-through's marches through its
// side's ground (ties to the larger defence, then the list's order). This file holds the reading to that: the
// function takes the fronts as found and says whose each province's corps are. What a front's sum makes of its
// members is fronts()' to say and is tested with it. The states are set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { fieldCtrl } from '../src/campaign/siege';
import { fronts, membership, startOpen, type Front } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

const BURMA = ['Rangoon', 'Prome', 'Toungoo', 'Yenangyaung', 'Meiktila', 'Mandalay', 'Lashio', 'Taunggyi', 'Loikaw', 'Bhamo', 'Moulmein', 'Mergui'];

/** A province changes hands for the test: whoever stood on it is sent off, and it is the taker's. */
const lose = (g: Game, name: string, to: string, away: string): Region => {
  const r = g.byName[name];
  for (const u of [...r.units]) relocate(g, u, g.byName[away]);
  r.ctrl = to;
  return r;
};
const corps = (g: Game, nat: string, name: string): Corps => g.corps.find((c) => c.nat === nat && c.name === name && !c.dead)!;
const on = (g: Game, f: Front, place: string): boolean => f.depth[g.byName[place].id] >= 1;

/** June 1942 with Burma lost to Mandalay, Lashio and Bhamo, a Japanese division in Mandalay; Britain's month is open. */
const india = (): Game => {
  const g = startOpen(createGame(3, null));
  g.turn = 34;
  declareWar(g, 'JAP', 'UK');
  for (const nm of BURMA) lose(g, nm, 'JAP', 'Calcutta');
  relocate(g, g.corps.find((c) => c.nat === 'JAP' && !c.dead && !c.hold && c.kind === 'inf')!, g.byName.Mandalay);
  startPhase(g, 'UK');
  return g;
};

/** Month 30, Japan at war with America, Britain and the Netherlands, ashore in northern Malaya, the 5th Division at Ipoh. */
const south = (): Game => {
  const g = startOpen(createGame(3, null));
  // (Month 2 is read in its month: the 21st Army takes the 5th Division from the 12th Army, and the 25th Army's row of
  // month 27 takes it from the 21st, as the record has it. Read first in month 27, after the 25th Army's, the month-2
  // row would draw the one 5th Division a second time: a division is laid once, drawn.ts.)
  g.turn = 2; startPhase(g, 'JAP');
  g.turn = 27; startPhase(g, 'JAP');
  g.turn = 28;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  for (const nm of ['Kota Bharu', 'Kuantan', 'Ipoh', 'Penang']) lose(g, nm, 'JAP', 'Singapore');
  relocate(g, corps(g, 'JAP', '25th Army (5th Division)'), g.byName.Ipoh);
  g.turn = 30; startPhase(g, 'JAP');
  relocate(g, corps(g, 'JAP', '2nd Tank Regiment'), g.byName['Kota Bharu']);
  return g;
};

/** The marches from one front's own provinces through this side's ground, counted afresh for the test. */
const marchesFrom = (g: Game, nat: string, f: Front): Int16Array => {
  const d = new Int16Array(g.regions.length).fill(-1);
  const q = [...f.ours];
  for (const id of q) d[id] = 0;
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, nat, fieldCtrl(g, g.regions[j]))) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
};

/** By province, for this nation's own corps: the nearest front, and of those as near the one with the larger defence. */
const holdsEverywhere = (g: Game, nat: string): number => {
  const list = fronts(g, nat);
  const mem = membership(g, nat, list);
  const d = list.map((f) => marchesFrom(g, nat, f));
  let reached = 0;
  for (const r of g.regions) {
    let best = -1;
    list.forEach((f, k) => {
      if (d[k][r.id] < 0) return;
      if (best < 0 || d[k][r.id] < d[best][r.id] || (d[k][r.id] === d[best][r.id] && f.def > list[best].def)) best = k;
    });
    expect([r.name, mem.at(r.id) ? list.indexOf(mem.at(r.id)!) : -1, mem.marches[r.id]]).toEqual([r.name, best, best < 0 ? -1 : d[best][r.id]]);
    if (best >= 0) reached++;
  }
  return reached;
};

describe("Britain's corps in India, June 1942: all of them are Burma's front's", () => {
  it('the provinces behind the frontier belong to the front against Japan in Burma, by their marches', () => {
    const g = india();
    const list = fronts(g, 'UK');
    const burma = list.find((f) => on(g, f, 'Mandalay'))!;
    expect(burma.enemies).toEqual(['JAP']);
    const mem = membership(g, 'UK', list);
    for (const nm of ['Kalewa', 'Akyab', 'Tamu']) { expect(mem.at(g.byName[nm].id)).toBe(burma); expect(mem.marches[g.byName[nm].id]).toBe(0); }
    for (const nm of ['Imphal', 'Chittagong']) { expect(mem.at(g.byName[nm].id)).toBe(burma); expect(mem.marches[g.byName[nm].id]).toBe(1); }
    expect(mem.at(g.byName.Calcutta.id)).toBe(burma);
    expect(mem.marches[g.byName.Calcutta.id]).toBeGreaterThanOrEqual(2);
    for (const nm of ['IV Corps (23rd Indian Division)', 'XV Corps (14th Indian Division)', '70th British Division', '2nd British Division']) expect(mem.of(corps(g, 'UK', nm))).toBe(burma);
  });

  it('a corps with no front to be reached by land is no front\'s: the army at home', () => {
    const g = india();
    const mem = membership(g, 'UK', fronts(g, 'UK'));
    expect(mem.at(g.byName.London.id)).toBeNull();
    expect(mem.marches[g.byName.London.id]).toBe(-1);
  });

  it('every province: the nearest front by the marches, the larger defence where two are as near', () => {
    expect(holdsEverywhere(g0(), 'UK')).toBeGreaterThan(20);
  });
  const g0 = india;
});

describe("Japan with the strike south gone: each front's corps are its own", () => {
  it("Malaya's corps are the front's before Kuala Lumpur, and none of the army in China is", () => {
    const g = south();
    const list = fronts(g, 'JAP');
    const malaya = list.find((f) => on(g, f, 'Kuala Lumpur'))!, china = list.find((f) => on(g, f, 'Chungking'))!;
    expect(malaya).not.toBe(china);
    const mem = membership(g, 'JAP', list);
    expect(mem.of(corps(g, 'JAP', '25th Army (5th Division)'))).toBe(malaya);
    expect(mem.of(corps(g, 'JAP', '2nd Tank Regiment'))).toBe(malaya);
    const inChina = g.corps.filter((c) => c.nat === 'JAP' && !c.dead && g.regions[c.reg].owner === 'CHN');
    expect(inChina.length).toBeGreaterThan(20);
    for (const c of inChina) expect(mem.of(c)).not.toBe(malaya);
  });

  it('every corps is one front\'s or none\'s, and the fronts\' order does not change whose', () => {
    const g = south();
    const list = fronts(g, 'JAP');
    const mem = membership(g, 'JAP', list), back = membership(g, 'JAP', [...list].reverse());
    let members = 0;
    for (const c of g.corps) {
      if (c.dead || c.nat !== 'JAP') continue;
      const f = mem.of(c);
      // (Equal defences apart, which the list's order settles: none here.)
      expect(back.of(c)).toBe(f);
      if (f) { members++; expect(list).toContain(f); }
    }
    expect(members).toBeGreaterThan(30);
    expect(new Set(list.map((f) => f.def)).size).toBe(list.length);
  });

  it('every province: the nearest front by the marches, the larger defence where two are as near', () => {
    expect(holdsEverywhere(south(), 'JAP')).toBeGreaterThan(40);
  });

  it('the map is not moved by what the caller does to his list afterwards', () => {
    const g = south();
    const list = fronts(g, 'JAP');
    const mem = membership(g, 'JAP', list);
    const was = g.regions.map((r) => mem.at(r.id));
    list.reverse();
    expect(g.regions.map((r) => mem.at(r.id))).toEqual(was);
    list.length = 0;
    expect(g.regions.map((r) => mem.at(r.id))).toEqual(was);
  });

  it('a province that stands on two fronts is the one with the larger defence\'s', () => {
    const g = south();
    const list = fronts(g, 'JAP');
    const mem = membership(g, 'JAP', list);
    let shared = 0;
    for (const r of g.regions) {
      const here = list.filter((f) => f.ours.includes(r.id));
      if (here.length < 2) continue;
      shared++;
      expect(mem.at(r.id)).toBe(here.reduce((a, b) => (b.def > a.def ? b : a)));
      expect(mem.marches[r.id]).toBe(0);
    }
    expect(shared).toBeGreaterThan(0);
  });

  it('a corps that stands on two fronts counts on both, and a corps behind the lines on its own front alone', () => {
    const g = south();
    const list = fronts(g, 'JAP');
    const hongKong = list.find((f) => on(g, f, 'Hong Kong'))!, china = list.find((f) => on(g, f, 'Chungking'))!;
    // (Tungkun is Hong Kong's front's one province, and it touches Waichow too: the tie gives it to China's front.)
    expect(hongKong.ours.map((id) => g.regions[id].name)).toEqual(['Tungkun']);
    expect(china.ours).toContain(g.byName.Tungkun.id);
    const mem = membership(g, 'JAP', list);
    expect(mem.at(g.byName.Tungkun.id)).toBe(china);
    const c = g.corps.find((x) => x.nat === 'JAP' && !x.dead && x.kind === 'inf' && g.regions[x.reg].name === 'Canton')!;
    // In Canton, a march behind: China's front's, and no other's.
    expect(mem.of(c)).toBe(china);
    expect(list.filter((f) => mem.counts(c, f))).toEqual([china]);
    // On Tungkun: still China's front's by the tie, and it counts before Hong Kong as well.
    relocate(g, c, g.byName.Tungkun);
    expect(mem.of(c)).toBe(china);
    expect(list.filter((f) => mem.counts(c, f)).sort((x, y) => y.def - x.def)).toEqual([china, hongKong]);
    // And every corps counts on its own front, wherever it stands.
    for (const x of g.corps) if (!x.dead && x.nat === 'JAP' && mem.of(x)) expect(mem.counts(x, mem.of(x)!)).toBe(true);
  });

  it("an ally's corps belongs to the nearest front its own country fights on, or to none", () => {
    const g = south();
    const list = fronts(g, 'JAP');
    const mem = membership(g, 'JAP', list);
    let seen = 0;
    for (const c of g.corps) {
      if (c.dead || c.nat === 'JAP' || !allied(g, 'JAP', c.nat)) continue;
      const f = mem.of(c);
      if (!f) continue;
      seen++;
      expect(f.enemies.some((e) => atWar(g, c.nat, e))).toBe(true);
    }
    expect(seen).toBeGreaterThan(0);
  });
});
