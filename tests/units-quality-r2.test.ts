// Units decide's quality store (release 2; the design director's ruling of 24 Nov 2026 15:53Z on the rules builder's #2
// form, rules-builder/release2/soviet/form.md, Part 2): one store for soldiers, pilots and crews, seeded as a grand
// campaign opens with the switch, absent without it. Its figures are data/campaign/units.json; the research rows it
// adds (Field radios and the three training rows) are in the tree only with the switch, and Germany's radios and the
// Scandinavians' winter kit come from the switch's own start list, never tech.json's. The armies' earned command seeds
// are the design director's of 24 Nov 16:06Z (the form's 'Seeds ruled 16:07Z').
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame, saveGame } from '../src/campaign/save';
import { TECH, grantTech, hasTech, outOfTree, techById, techRule, techView, treeLevel, whyNotResearch } from '../src/campaign/tech';
import { UNITS, armsOf, openQuality, qualityStore, setUnitsDecide, unitsDecide } from '../src/campaign/units';
import { FIELDS } from '../src/campaign/research';
import type { Game } from '../src/campaign/types';
import units from '../data/campaign/units.json';
import lean from '../data/battle/lean.json';
import unitsSource from '../src/campaign/units.ts?raw';

const ROWS = ['radios', 'nco', 'battledrill', 'officers'];

/** A grand campaign at its first day, opened with Units decide or without it (a test may set the flag on the game it opens). */
function open(on: boolean, seed = 1): Game {
  const g = createGame(seed, null);
  if (on) g.flags.unitsDecide = 1;
  return startOpen(g);
}
/** Command c, worked out as the form has it: the radios row's share if held, plus earned, at most 1. */
const command = (g: Game, nat: string): number => Math.min(1, (hasTech(g, nat, 'radios') ? UNITS.land.radios : 0) + armsOf(g, nat)!.land.earned);
const hasKey = (text: string, key: string): boolean => new RegExp(`"${key}":`).test(text);

describe('off: nothing of the store', () => {
  const g = open(false);

  it('writes no store and keeps the game as it was', () => {
    expect(unitsDecide(g)).toBe(false);
    expect('qs' in g).toBe(false);
    expect(qualityStore(g)).toBeUndefined();
    expect(armsOf(g, 'GER')).toBeUndefined();
    openQuality(g);
    expect('qs' in g).toBe(false);
    // The unit figures are never written while it is off.
    for (const f of g.airFleets) for (const k of ['hours', 'fxp']) expect(k in f, `${f.name} ${k}`).toBe(false);
    for (const f of g.fleets) for (const k of ['crew', 'sxp', 'workup']) expect(k in f, `${f.name} ${k}`).toBe(false);
    expect(hasKey(saveGame(g), 'qs')).toBe(false);
  });

  it('has none of its research rows, and no nation is given its start rows', () => {
    for (const id of ROWS) {
      expect(outOfTree(g, techById(id)), id).toBe(true);
      expect(whyNotResearch(g, 'GER', id), id).toBe('There is no such technology.');
      expect(grantTech(g, 'UK', id), id).toBe(false);
    }
    const shown = techView(g, 'GER')!.nodes.map((n) => n.id);
    for (const id of ROWS) expect(shown, id).not.toContain(id);
    for (const n of Object.values(g.nations)) for (const id of ROWS) expect(n.tech?.done ?? [], `${n.id} ${id}`).not.toContain(id);
    expect(g.nations.SWE.tech!.done).not.toContain('winterkit');
    expect(g.nations.NOR.tech!.done).not.toContain('winterkit');
  });
});

describe('on: the store as the campaign opens', () => {
  const g = open(true);
  const qs = g.qs!;

  it('is seeded once in startOpen, for every nation in the game, with no news yet', () => {
    expect(unitsDecide(g)).toBe(true);
    expect(qs).toBeDefined();
    expect(qualityStore(g)).toBe(qs);
    openQuality(g);
    expect(g.qs).toBe(qs);
    expect(qs.from).toBe(1);
    expect(qs.news).toEqual({});
    expect(Object.keys(qs.nat).sort()).toEqual(Object.keys(g.nations).sort());
  });

  it('seeds training from each nation\'s quality as the campaign opened, by one rule', () => {
    for (const n of Object.values(g.nations)) expect(qs.nat[n.id].land.train, n.id).toBe(n.quality);
    expect(qs.nat.GER.land.train).toBe(1.25);
    expect(qs.nat.SOV.land.train).toBe(0.7);
    expect(qs.nat.FRA.land.train).toBe(0.95);
    expect(qs.nat.UK.land.train).toBe(1.05);
  });

  // DD 24 Nov 16:06Z (the form's 'Seeds ruled 16:07Z'): the start's c by army from the historian's reading of 1939-41,
  // every army not yet read at the range middle, 0.5, never today's 1.
  it('seeds command earned by army as ruled at 16:06Z, less the radios Germany holds: every start c is the seed', () => {
    expect(qs.nat.GER.land.earned).toBeCloseTo(0.85, 12);
    expect(qs.nat.SOV.land.earned).toBe(0);
    expect(qs.nat.FRA.land.earned).toBe(0);
    expect(qs.nat.POL.land.earned).toBe(0.1);
    expect(qs.nat.ITA.land.earned).toBe(0.25);
    expect(qs.nat.UK.land.earned).toBe(0.5);
    expect(qs.nat.USA.land.earned).toBe(0.5);
    expect(qs.nat.HUN.land.earned).toBe(0.5);
    expect(command(g, 'GER')).toBeCloseTo(1, 12);
    expect(command(g, 'SOV')).toBe(0);
    expect(command(g, 'FRA')).toBe(0);
    expect(command(g, 'POL')).toBe(0.1);
    expect(command(g, 'ITA')).toBe(0.25);
    expect(command(g, 'UK')).toBe(0.5);
    expect(command(g, 'USA')).toBe(0.5);
    expect(command(g, 'HUN')).toBe(0.5);
    for (const n of Object.values(g.nations)) {
      expect(qs.nat[n.id].land.school, n.id).toBe(0);
      expect(command(g, n.id), n.id).toBeCloseTo(UNITS.land.earned.seed[n.id] ?? UNITS.land.earned.seed._default, 12);
    }
  });

  it('seeds the staff\'s way from lean.json\'s land column, the rest of Europe as \'_\'', () => {
    const way = (nat: string) => [qs.nat[nat].land.way.att.id, qs.nat[nat].land.way.def.id];
    expect(way('GER')).toEqual(['bold', 'bold']);
    expect(way('SOV')).toEqual(['mass', 'mass']);
    expect(way('FRA')).toEqual(['methodical', 'methodical']);
    expect(way('UK')).toEqual(['methodical', 'methodical']);
    expect(way('HUN')).toEqual([lean.lean._.land, lean.lean._.land]);
    expect(qs.nat.GER.land.way.att.by).toBe('start');
    expect(qs.nat.GER.land.way.att.next).toBeUndefined();
  });

  it('seeds the air and sea slots where the form gives a seed, and null where it does not', () => {
    expect(qs.nat.GER.air).toEqual({ school: 250, deck: null, held: 1.1, pool: 250, poolXp: 0, strike: 0.12, fighter: null, escort: null });
    expect(qs.nat.UK.air).toEqual({ school: null, deck: null, held: 1.05, pool: null, poolXp: 0, strike: 0.08, fighter: null, escort: null });
    expect(qs.nat.SOV.air).toEqual({ school: null, deck: null, held: 0.6, pool: null, poolXp: 0, strike: 0.08, fighter: null, escort: null });
    expect(qs.nat.HUN.air.held).toBe(0.7);
    expect(qs.nat.GER.sea).toEqual({ crew: 1.1, night: null, lookout: null, way: { id: 'methodical', by: 'start' }, pack: { id: 'alone', by: 'start' }, target: { id: 'merchants', by: 'start' } });
    expect(qs.nat.UK.sea.way.id).toBe('bold');
    expect(qs.nat.SOV.sea.way.id).toBe('mass');
    expect(qs.nat.HUN.sea.crew).toBe(0.7);
  });

  it('seeds a nation that joins later by the same rule: a night staff, a carrier school, warships first', () => {
    const h = open(true, 2);
    h.nations.JAP = { ...h.nations.ITA, id: 'JAP', quality: 1, tech: { done: [], work: [] } };
    const j = armsOf(h, 'JAP')!;
    expect(h.qs!.nat.JAP).toBe(j);
    expect([j.land.way.att.id, j.land.way.def.id]).toEqual(['night', 'last-stand']);
    expect(j.land).toMatchObject({ train: 1, earned: 0.5, school: 0 });
    expect(j.air).toMatchObject({ school: null, deck: 500, strike: 0.4 });
    // Japan's night at sea is fought as its stand-in: the sea has no night way.
    expect(j.sea.way.id).toBe('bold');
    expect(j.sea.target.id).toBe('warships');
  });

  it('takes a later start\'s convoy doctrine from its own month', () => {
    const at = (from: number) => {
      const h = open(false, 3);
      h.flags.openFrom = from;
      h.flags.unitsDecide = 1;
      return qualityStore(h)!;
    };
    expect(at(5).nat.GER.sea.pack.id).toBe('alone');
    expect(at(17).nat.GER.sea.pack.id).toBe('homed');
    expect(at(17).nat.ITA.sea.pack.id).toBe('homed');
    expect(at(29).nat.GER.sea.pack.id).toBe('homed');
    expect(at(29).nat.ITA.sea.pack.id).toBe('alone');
    expect(at(29).from).toBe(29);
    expect(at(29).nat.GER.air.school).toBe(250);
  });
});

describe('on: the research rows and the start list', () => {
  const g = open(true);

  it('adds four rows to the doctrine branch, with no field, so techMul never counts them', () => {
    for (const id of ROWS) {
      const t = techById(id)!;
      expect(t, id).toBeDefined();
      expect(t.units, id).toBe(true);
      expect(t.field, id).toBeUndefined();
      expect(t.year, id).toBe(1939);
      expect(outOfTree(g, t), id).toBe(false);
      expect(techRule(t).length, id).toBeGreaterThan(10);
    }
    expect(techById('radios')).toMatchObject({ tier: 2, needs: ['staff'], name: 'Field radios' });
    expect(techById('nco')).toMatchObject({ tier: 2, needs: [] });
    expect(techById('battledrill')).toMatchObject({ tier: 3, needs: ['nco'] });
    expect(techById('officers')).toMatchObject({ tier: 4, needs: ['battledrill'] });
    const shown = techView(g, 'SOV')!.nodes.map((n) => n.id);
    for (const id of ROWS) expect(shown, id).toContain(id);
    const h = open(true, 4);
    const before = FIELDS.map((f) => treeLevel(h, 'SOV', f));
    for (const id of ROWS) expect(grantTech(h, 'SOV', id), id).toBe(true);
    expect(FIELDS.map((f) => treeLevel(h, 'SOV', f))).toEqual(before);
  });

  it('gives Germany radios and Sweden and Norway winter kit from its own list, never from tech.json\'s start', () => {
    expect(hasTech(g, 'GER', 'radios')).toBe(true);
    expect(hasTech(g, 'SWE', 'winterkit')).toBe(true);
    expect(hasTech(g, 'NOR', 'winterkit')).toBe(true);
    for (const nat of Object.keys(g.nations)) expect(hasTech(g, nat, 'radios'), nat).toBe(nat === 'GER');
    for (const [nat, list] of Object.entries(TECH.start)) for (const id of ROWS) expect(list, `${nat} ${id}`).not.toContain(id);
    expect(TECH.start.GER).not.toContain('radios');
    expect(TECH.start.SWE ?? []).not.toContain('winterkit');
    expect(TECH.start.NOR ?? []).not.toContain('winterkit');
    // Each row once, and no team set to a row already held.
    for (const nat of ['GER', 'SWE', 'NOR']) {
      const s = g.nations[nat].tech!;
      expect(new Set(s.done).size, nat).toBe(s.done.length);
      for (const id of UNITS.startRows[nat]) expect(s.work.some((w) => w.id === id), nat).toBe(false);
    }
  });
});

describe('save and load', () => {
  it('keeps the store through a save', () => {
    const g = open(true);
    const text = saveGame(g);
    expect(hasKey(text, 'qs')).toBe(true);
    const h = loadGame(text)!;
    expect(unitsDecide(h)).toBe(true);
    expect(h.qs).toEqual(g.qs);
    expect(saveGame(h)).toBe(text);
  });

  it('never saves a store left behind by a flag cleared by hand, and a save without one is seeded on first use', () => {
    const g = open(true);
    delete g.flags.unitsDecide;
    expect(g.qs).toBeDefined();
    expect(qualityStore(g)).toBeUndefined();
    const text = saveGame(g);
    expect(hasKey(text, 'qs')).toBe(false);
    const h = loadGame(text)!;
    expect('qs' in h).toBe(false);
    // The flag set by hand on that game: the store is seeded on first use, never fallen back from.
    h.flags.unitsDecide = 1;
    const qs = qualityStore(h)!;
    expect(qs).toBeDefined();
    expect(h.qs).toBe(qs);
    expect(qs.nat.SOV.land.earned).toBe(0);
    expect(qs.nat.GER.land.earned).toBeCloseTo(0.85, 12);
  });

  it('seeds on first use with the flag set by hand after a campaign opened without it: Germany then holds no radios, and keeps c 1 as earned', () => {
    const g = open(false);
    g.flags.unitsDecide = 1;
    expect(hasTech(g, 'GER', 'radios')).toBe(false);
    expect(armsOf(g, 'GER')!.land.earned).toBe(1);
    expect(command(g, 'GER')).toBe(1);
  });
});

describe('setUnitsDecide at setup, after a campaign opened without the switch', () => {
  it('lays the switch\'s start list and seeds the store from it, as a campaign opened with the switch', () => {
    const g = open(false);
    expect(g.turn).toBe(1);
    expect(setUnitsDecide(g, true)).toBe(true);
    expect(unitsDecide(g)).toBe(true);
    expect(hasTech(g, 'GER', 'radios')).toBe(true);
    expect(hasTech(g, 'SWE', 'winterkit')).toBe(true);
    expect(hasTech(g, 'NOR', 'winterkit')).toBe(true);
    for (const nat of Object.keys(g.nations)) expect(hasTech(g, nat, 'radios'), nat).toBe(nat === 'GER');
    for (const [nat, ids] of Object.entries(UNITS.startRows)) {
      const s = g.nations[nat].tech!;
      expect(new Set(s.done).size, nat).toBe(s.done.length);
      for (const id of ids) expect(s.work.some((w) => w.id === id), `${nat} ${id}`).toBe(false);
    }
    expect(g.qs).toBeDefined();
    expect(armsOf(g, 'GER')!.land.earned).toBeCloseTo(0.85, 12);
    expect(command(g, 'GER')).toBeCloseTo(1, 12);
    // The same seeds, nation by nation, as the campaign opened with the switch.
    const on = open(true);
    for (const nat of Object.keys(on.nations)) expect(armsOf(g, nat), nat).toEqual(armsOf(on, nat));
    // Set twice, nothing is laid twice.
    const done = g.nations.GER.tech!.done.slice();
    expect(setUnitsDecide(g, true)).toBe(true);
    expect(g.nations.GER.tech!.done).toEqual(done);
  });

  it('set off again, takes the store with it and saves none', () => {
    const g = open(false);
    setUnitsDecide(g, true);
    expect(g.qs).toBeDefined();
    expect(setUnitsDecide(g, false)).toBe(true);
    expect(unitsDecide(g)).toBe(false);
    expect('qs' in g).toBe(false);
    expect(qualityStore(g)).toBeUndefined();
    expect(hasKey(saveGame(g), 'qs')).toBe(false);
  });

  it('is refused once the first month has begun, and lays nothing', () => {
    const g = open(false);
    g.turn += 1;
    expect(setUnitsDecide(g, true)).toBe(false);
    expect(unitsDecide(g)).toBe(false);
    expect(hasTech(g, 'GER', 'radios')).toBe(false);
    expect('qs' in g).toBe(false);
  });
});

describe('the data', () => {
  it('holds every figure the later steps read, each with its note', () => {
    const L = UNITS.land;
    expect(L.ladder).toEqual([0.85, 1.05, 1.25]);
    expect(L.trainRows).toEqual(['nco', 'battledrill', 'officers']);
    expect(L.radios).toBe(0.15);
    expect(L.earned).toEqual({ seed: { _default: 0.5, GER: 1, SOV: 0, FRA: 0, POL: 0.1, ITA: 0.25, JAP: 0.5, UK: 0.5, USA: 0.5 }, month: 0.01, provinces: 2, corps: -0.01 });
    // The switch's start list names rows the tree has.
    for (const [nat, ids] of Object.entries(UNITS.startRows)) for (const id of ids) expect(techById(id), `${nat} ${id}`).toBeDefined();
    expect(L.byOrders).toEqual({ need: 0.9, stop: 1.3, main: 2.6 });
    expect(L.mostCorps).toEqual({ base: 8, perC: 12 });
    expect(L.attack).toEqual({ base: 0.85, perC: 0.15 });
    expect(L.gates).toEqual({ renewal: 0.3, blitz: 0.6 });
    expect(L.sight).toEqual({ from: 0.6, span: 0.4 });
    expect(L.xp).toEqual({ won: 0.05, repulsed: 0.02, held: 0.04, beaten: 0.02 });
    expect(L.label).toEqual({ share: 0.148, cap: 0.9 });
    expect(L.school).toBe(0);
    // Every entry has its note, and every figure its mark (F, L, D, M, G, U) beside where it comes from in the form. The
    // id that names a row, the lists of ids and the escort doctrines are told in the note of the entry beside them.
    const told = ['radiosRow', 'packs', 'targets', 'escort', 'land', 'air', 'sea'];
    const noted = (o: Record<string, unknown>, at: string): void => {
      for (const k of Object.keys(o)) {
        if (k.startsWith('_') || told.includes(k)) continue;
        const note = o[`_${k}`];
        expect(typeof note, `${at}${k}`).toBe('string');
        if (/\d/.test(JSON.stringify(o[k]))) expect(note as string, `${at}${k}`).toMatch(/\b[FLDMGU][.:;)]/);
      }
    };
    noted(units as unknown as Record<string, unknown>, '');
    for (const arm of ['land', 'air', 'sea'] as const) noted((units as unknown as Record<string, Record<string, unknown>>)[arm], `${arm}.`);
  });

  it('names no nation in the store\'s code: every per-nation seed is data', () => {
    expect(unitsSource.length).toBeGreaterThan(1000);
    const src = unitsSource.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
    expect(src).not.toMatch(/['"`](GER|SOV|FRA|UK|USA|ITA|JAP|SWE|NOR|FIN|HUN|POL)['"`]/);
  });
});
