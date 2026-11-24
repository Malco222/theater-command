// Units decide's quality store (release 2; the design director's ruling of 24 Nov 2026 15:53Z on the rules builder's #2
// form, rules-builder/release2/soviet/form.md, Part 2): one store for soldiers, pilots and crews, seeded as a grand
// campaign opens with the switch, absent without it. Its figures are data/campaign/units.json; the research rows it
// adds (Field radios and the three training rows) are in the tree only with the switch, and Germany's radios and the
// Scandinavians' winter kit come from the switch's own start list, never tech.json's. The armies' earned command seeds
// are the design director's of 24 Nov 16:06Z (the form's 'Seeds ruled 16:07Z'). Then training and command (Part 1 2a,
// 2b and section 1, as ruled): training as the ladder, command c from radios and what is earned, the attack, the most
// corps, the slow habits and the blitz gate read from them for every nation, earned moved by the month's own battles,
// and history's reforms of an army not applied, their texts told by causes.
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
import commandSource from '../src/campaign/command.ts?raw';
import { assess, corpsBase, freshOffensive, limitAttackers, removeCorps, setWar } from '../src/campaign/game';
import { traitAttack, traitCardRule, traitNames, traitRule, traitsOf } from '../src/campaign/traits';
import page from '../src/campaign/ui/main.ts?raw';
import { DIPLOMACY, fireEvent } from '../src/campaign/diplomacy';
import { storyFallen } from '../src/campaign/stories';
import { guardPower, stormForce } from '../src/campaign/siege';
import { techMul } from '../src/campaign/research';
import { runTurnAI } from '../src/campaign/turn';
import { commandMonth, corpsTakenWhole, provincesTaken } from '../src/campaign/command';
import { armyTraining, attackCommand, blitzGate, commandOf, mostCorps, slowHabits } from '../src/campaign/units';
import { KIND, type BattleReport, type Corps, type Region } from '../src/campaign/types';

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

  it('names no nation in the store\'s code or the month\'s command: every per-nation seed is data', () => {
    for (const text of [unitsSource, commandSource]) {
      expect(text.length).toBeGreaterThan(1000);
      const src = text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
      expect(src).not.toMatch(/['"`](GER|SOV|FRA|UK|USA|ITA|JAP|SWE|NOR|FIN|HUN|POL)['"`]/);
    }
  });
});

// ---- Training and command (DD 24 Nov 15:53Z, #2 form Part 1 2a, 2b and section 1) --------------------------------

/** Gives a nation research rows (as the tree would on finishing them). */
const give = (g: Game, nat: string, ...ids: string[]): void => { for (const id of ids) if (!g.nations[nat].tech!.done.includes(id)) g.nations[nat].tech!.done.push(id); };
/** Takes research rows away from a nation. */
const take = (g: Game, nat: string, ...ids: string[]): void => { g.nations[nat].tech!.done = g.nations[nat].tech!.done.filter((x) => !ids.includes(x)); };
/** Sets an army's earned command in the store. */
const earn = (g: Game, nat: string, x: number): void => { armsOf(g, nat)!.land.earned = x; };
/** A copy of a corps, standing where it stands, under another flag or another id. */
const twin = (c: Corps, over: Partial<Corps>): Corps => ({ ...structuredClone(c), ...over });
/** A German infantry corps at the start beside a Polish province held by Polish troops, and that province. */
function polishFront(g: Game): { inf: Corps; to: Region } {
  for (const c of g.corps) {
    if (c.nat !== 'GER' || c.kind !== 'inf' || c.dead || c.town !== undefined) continue;
    for (const j of g.regions[c.reg].nb) {
      const r = g.regions[j];
      if (r.ctrl === 'POL' && r.units.some((u) => u.nat === 'POL' && !u.dead)) return { inf: c, to: r };
    }
  }
  throw new Error('no front');
}
const plains = (g: Game): Region => g.regions.find((r) => r.terrain === 'plains' && !r.fort && !r.units.length)!;
/** A battle report of this month: `nat` attacked `region` and took it, with whatever else is given. */
const report = (g: Game, nat: string, region: number, over: Partial<BattleReport> = {}): BattleReport => ({
  id: 9000 + g.reports.length, turn: g.turn, week: 1, region, regionName: g.regions[region].name, attNat: nat, defNat: 'GER',
  attackers: [], defenders: [], lines: [], aPower: 1, dPower: 1, odds: 1, level: 'Close victory', taken: true, attLost: 0, defLost: 0, prisoners: 0, ...over,
});

describe('training: the ladder (DD 24 Nov 15:53Z (4))', () => {
  it('reads the start seed with no rows, then 0.85, 1.05 and 1.25 for one, two or three training rows, whichever they are', () => {
    const g = open(true);
    expect(armyTraining(g, 'SOV')).toBe(0.7);
    give(g, 'SOV', 'officers');
    expect(armyTraining(g, 'SOV')).toBe(0.85);
    take(g, 'SOV', 'officers');
    give(g, 'SOV', 'nco');
    expect(armyTraining(g, 'SOV')).toBe(0.85);
    give(g, 'SOV', 'battledrill');
    expect(armyTraining(g, 'SOV')).toBe(1.05);
    give(g, 'SOV', 'officers');
    expect(armyTraining(g, 'SOV')).toBe(1.25);
    // The same ladder for every nation: the Italians climb it rung for rung.
    give(g, 'ITA', 'nco', 'battledrill', 'officers');
    expect(armyTraining(g, 'ITA')).toBe(1.25);
  });

  it('never takes an army below its own start: a seed above the rung is kept', () => {
    const g = open(true);
    expect(armyTraining(g, 'GER')).toBe(1.25);
    give(g, 'GER', 'nco');
    expect(armyTraining(g, 'GER')).toBe(1.25);
    give(g, 'UK', 'nco');
    expect(armyTraining(g, 'UK')).toBe(1.05);
    give(g, 'UK', 'battledrill');
    expect(armyTraining(g, 'UK')).toBe(1.05);
    give(g, 'UK', 'officers');
    expect(armyTraining(g, 'UK')).toBe(1.25);
    give(g, 'FRA', 'nco');
    expect(armyTraining(g, 'FRA')).toBe(0.95);
  });

  it('is what corpsBase and the town guard read under the switch, never the nation\'s quality; off they read the quality', () => {
    const g = open(true), off = open(false);
    const sov = g.corps.find((c) => c.nat === 'SOV' && c.kind === 'inf' && !c.dead)!;
    const before = corpsBase(g, sov);
    g.nations.SOV.quality = 2;
    expect(corpsBase(g, sov)).toBe(before);
    give(g, 'SOV', 'nco');
    expect(corpsBase(g, sov) / before).toBeCloseTo(0.85 / 0.7, 12);
    const r = g.regions.find((x) => x.owner === 'POL' && guardPower(g, x) > 0)!;
    expect(r).toBeDefined();
    const guard = guardPower(g, r);
    g.nations.POL.quality *= 3;
    expect(guardPower(g, r)).toBe(guard);
    give(g, 'POL', 'nco', 'battledrill');
    expect(guardPower(g, r) / guard).toBeCloseTo(1.05 / armsOf(g, 'POL')!.land.train, 12);
    // Off: the nation's quality, as today, and the training rows are not in the tree.
    const osov = off.corps.find((c) => c.id === sov.id)!;
    const was = corpsBase(off, osov);
    expect(armyTraining(off, 'SOV')).toBe(0.7);
    off.nations.SOV.quality = 1.4;
    expect(corpsBase(off, osov) / was).toBeCloseTo(2, 12);
  });

  it('is what the card\'s Troops line reads, and names training alone', () => {
    const g = open(true), off = open(false);
    const { inf, to } = polishFront(g);
    const troops = (h: Game) => assess(h, [h.corps.find((c) => c.id === inf.id)!], h.regions[to.id]).lines.find((l) => l.label === 'Troops');
    const on = troops(g)!;
    expect(on.effect).toMatch(/as the better trained army/);
    expect(on.effect).not.toMatch(/led/);
    expect(troops(off)!.effect).toMatch(/as the better trained and led army/);
    g.nations.GER.quality = 0.5;
    expect(troops(g)).toEqual(on);
  });
});

describe('command c and the attack (#2 form Part 1 2b)', () => {
  it('is Field radios\' 0.15 where held, plus what is earned, at most 1, worked out and never stored', () => {
    const g = open(true);
    expect(commandOf(g, 'GER')).toBeCloseTo(1, 12);
    expect(commandOf(g, 'SOV')).toBe(0);
    earn(g, 'SOV', 0.4);
    expect(commandOf(g, 'SOV')).toBeCloseTo(0.4, 12);
    give(g, 'SOV', 'radios');
    expect(commandOf(g, 'SOV')).toBeCloseTo(0.55, 12);
    earn(g, 'SOV', 0.95);
    expect(commandOf(g, 'SOV')).toBe(1);
    // Staff colleges, Battle groups and Deep operations keep their one price in techMul and feed no c.
    give(g, 'UK', 'staff', 'combined', 'deep');
    expect(commandOf(g, 'UK')).toBe(0.5);
    for (const n of Object.values(g.qs!.nat)) expect(Object.keys(n.land).sort()).toEqual(['earned', 'school', 'train', 'way']);
  });

  it('multiplies attack by 0.85 + 0.15c for every nation; off, a slow command\'s 0.85', () => {
    const g = open(true), off = open(false);
    expect(attackCommand(g, 'SOV')).toBeCloseTo(0.85, 12);
    expect(attackCommand(g, 'GER')).toBeCloseTo(1, 12);
    expect(attackCommand(g, 'UK')).toBeCloseTo(0.925, 12);
    expect(attackCommand(g, 'POL')).toBeCloseTo(0.865, 12);
    expect(attackCommand(g, 'ITA')).toBeCloseTo(0.8875, 12);
    // The nation's own slow command is not read under the switch.
    g.nations.SOV.slowCommand = false;
    g.nations.UK.slowCommand = true;
    expect(attackCommand(g, 'SOV')).toBeCloseTo(0.85, 12);
    expect(attackCommand(g, 'UK')).toBeCloseTo(0.925, 12);
    earn(g, 'SOV', 0.6);
    expect(attackCommand(g, 'SOV')).toBeCloseTo(0.94, 12);
    expect(attackCommand(off, 'SOV')).toBe(0.85);
    expect(attackCommand(off, 'FRA')).toBe(0.85);
    expect(attackCommand(off, 'GER')).toBe(1);
    expect(attackCommand(off, 'UK')).toBe(1);
  });

  it('is the attack the card reckons: a German infantry attack at c 0.6 is 0.94 of the same at c 1', () => {
    const g = open(true);
    const { inf, to } = polishFront(g);
    const full = assess(g, [inf], to).A;
    earn(g, 'GER', 0.45);
    expect(commandOf(g, 'GER')).toBeCloseTo(0.6, 12);
    expect(assess(g, [inf], to).A / full).toBeCloseTo(0.94, 9);
    g.nations.GER.slowCommand = true;
    expect(assess(g, [inf], to).A / full).toBeCloseTo(0.94, 9);
  });

  it('sets the most corps in one battle at 8 + 12c, rounded down, for every nation; off, the nation\'s own limit', () => {
    const g = open(true), off = open(false);
    expect(mostCorps(g, 'SOV')).toBe(8);
    expect(mostCorps(g, 'FRA')).toBe(8);
    expect(mostCorps(g, 'POL')).toBe(9);
    expect(mostCorps(g, 'ITA')).toBe(11);
    expect(mostCorps(g, 'UK')).toBe(14);
    expect(mostCorps(g, 'GER')).toBe(20);
    // Fifty steps of 0.01 make 0.5: a cap of 14, not 13.
    let x = 0;
    for (let i = 0; i < 50; i++) x += 0.01;
    earn(g, 'SOV', x);
    expect(mostCorps(g, 'SOV')).toBe(14);
    g.nations.GER.maxAttack = 3;
    expect(mostCorps(g, 'GER')).toBe(20);
    expect(mostCorps(off, 'SOV')).toBe(8);
    expect(mostCorps(off, 'GER')).toBe(0);
  });

  it('caps a storm and a battle\'s force at that many corps', () => {
    const g = open(true);
    const { inf, to } = polishFront(g);
    const many = Array.from({ length: 25 }, (_, i) => twin(inf, { id: 90000 + i, reg: to.id, ap: 4 }));
    g.nations.GER.maxAttack = 3;
    expect(stormForce(g, many, to).length).toBe(20);
    take(g, 'GER', 'radios');
    earn(g, 'GER', 0);
    expect(stormForce(g, many, to).length).toBe(8);
    earn(g, 'GER', 0.5);
    expect(stormForce(g, many, to).length).toBe(14);
    // In the field the roads into the province bind as before: never more than the cap, wherever they come from.
    const from = g.regions.filter((r) => r.nb.includes(to.id) && r.ctrl === 'GER');
    const spread = Array.from({ length: 30 }, (_, i) => twin(inf, { id: 91000 + i, reg: from[i % from.length].id, ap: 4 }));
    expect(limitAttackers(g, spread, to).length).toBeLessThanOrEqual(14);
    earn(g, 'GER', 0);
    expect(limitAttackers(g, spread, to).length).toBeLessThanOrEqual(8);
  });

  it('keeps a slow command\'s habits while c is under 0.3, for every nation; off, the nation\'s own slow command', () => {
    const g = open(true), off = open(false);
    expect(slowHabits(g, 'SOV')).toBe(true);
    expect(slowHabits(g, 'FRA')).toBe(true);
    expect(slowHabits(g, 'POL')).toBe(true);
    expect(slowHabits(g, 'ITA')).toBe(true);
    expect(slowHabits(g, 'UK')).toBe(false);
    expect(slowHabits(g, 'GER')).toBe(false);
    earn(g, 'SOV', 0.3);
    expect(slowHabits(g, 'SOV')).toBe(false);
    g.nations.SOV.slowCommand = true;
    expect(slowHabits(g, 'SOV')).toBe(false);
    expect(slowHabits(off, 'SOV')).toBe(true);
    expect(slowHabits(off, 'FRA')).toBe(true);
    expect(slowHabits(off, 'POL')).toBe(false);
    expect(slowHabits(off, 'UK')).toBe(false);
  });
});

describe('the blitz gate: Field radios and c 0.6 or more, for any nation (#2 form Part 1 section 1)', () => {
  it('opens for Germany at the start and for nobody else; any army with radios and the command passes it', () => {
    const g = open(true);
    for (const nat of Object.keys(g.nations)) expect(blitzGate(g, nat), nat).toBe(nat === 'GER');
    // The nation's own blitz is not read.
    g.nations.GER.blitz = false;
    g.nations.SOV.blitz = true;
    expect(blitzGate(g, 'GER')).toBe(true);
    expect(blitzGate(g, 'SOV')).toBe(false);
    // Earned command alone does not pass it: it needs the radios.
    earn(g, 'SOV', 1);
    expect(blitzGate(g, 'SOV')).toBe(false);
    give(g, 'SOV', 'radios');
    earn(g, 'SOV', 0.45);
    expect(blitzGate(g, 'SOV')).toBe(true);
    earn(g, 'SOV', 0.44);
    expect(blitzGate(g, 'SOV')).toBe(false);
    // Britain passes it once it researches radios (0.5 + 0.15): the cause working.
    give(g, 'UK', 'radios');
    expect(blitzGate(g, 'UK')).toBe(true);
    take(g, 'GER', 'radios');
    expect(blitzGate(g, 'GER')).toBe(false);
  });

  it('gives the fresh offensive\'s tempo and move to whoever passes it', () => {
    const g = open(true);
    g.nations.SOV.offStart.FIN = g.turn;
    g.nations.GER.offStart.POL = g.turn;
    expect(freshOffensive(g, 'SOV', 'FIN')).toBe(false);
    expect(freshOffensive(g, 'GER', 'POL')).toBe(true);
    give(g, 'SOV', 'radios');
    earn(g, 'SOV', 0.5);
    expect(freshOffensive(g, 'SOV', 'FIN')).toBe(true);
    take(g, 'GER', 'radios');
    expect(freshOffensive(g, 'GER', 'POL')).toBe(false);
  });

  it('gives the armoured spearheads\' x1.15 to any army through it, and reads no nation\'s own spearheads or tempo row', () => {
    const g = open(true), off = open(false);
    const ground = plains(g);
    const ger = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
    const sov = twin(ger, { id: 92000, nat: 'SOV', elite: undefined });
    const gerAt = traitAttack(g, ger, ground), sovAt = traitAttack(g, sov, ground);
    expect(traitNames(g, [ger], ground, 'attack')).toContain('Armoured spearheads');
    expect(traitNames(g, [sov], ground, 'attack')).not.toContain('Armoured spearheads');
    give(g, 'SOV', 'radios');
    earn(g, 'SOV', 0.45);
    expect(traitAttack(g, sov, ground) / sovAt).toBeCloseTo(1.15, 12);
    expect(traitNames(g, [sov], ground, 'attack')).toContain('Armoured spearheads');
    take(g, 'GER', 'radios');
    expect(traitAttack(g, ger, ground) / gerAt).toBeCloseTo(1 / 1.15, 12);
    expect(traitNames(g, [ger], ground, 'attack')).not.toContain('Armoured spearheads');
    // Off, Germany's own row, as today.
    const oger = off.corps.find((c) => c.id === ger.id)!;
    expect(traitNames(off, [oger], off.regions[ground.id], 'attack')).toContain('Armoured spearheads');
    expect(blitzGate(off, 'GER')).toBe(true);
    expect(blitzGate(off, 'SOV')).toBe(false);
  });
});

describe('earned command: one test on the month\'s own battles (#2 form Part 1 2b)', () => {
  it('rises 0.01 for a month in which a computer nation\'s own attacks took two provinces or more', () => {
    const g = open(true);
    expect(g.human).toBeNull();
    const [a, b, c] = g.regions.filter((r) => r.ctrl === 'GER').map((r) => r.id);
    g.reports.push(report(g, 'UK', a), report(g, 'UK', a));
    expect(provincesTaken(g, 'UK')).toBe(1);
    // A field won round a town that holds out, a relief and a sally take no province.
    g.reports.push(report(g, 'UK', b, { siege: true }), report(g, 'UK', b, { relief: true }), report(g, 'UK', c, { sally: true }), report(g, 'UK', c, { taken: false }));
    expect(provincesTaken(g, 'UK')).toBe(1);
    commandMonth(g);
    expect(armsOf(g, 'UK')!.land.earned).toBe(0.5);
    // A walk-in after the defenders gave ground counts.
    g.reports.push(report(g, 'UK', b, { yielded: true }));
    expect(provincesTaken(g, 'UK')).toBe(2);
    commandMonth(g);
    expect(armsOf(g, 'UK')!.land.earned).toBe(0.51);
    // Last month's battles are not this month's.
    const h = open(true, 5);
    h.reports.push(report(h, 'UK', a, { turn: h.turn - 1 }), report(h, 'UK', b, { turn: h.turn - 1 }));
    expect(provincesTaken(h, 'UK')).toBe(0);
  });

  it('is the same test for the player\'s own nation', () => {
    const g = createGame(1, 'SOV');
    g.flags.unitsDecide = 1;
    startOpen(g);
    expect(g.human).toBe('SOV');
    const [a, b] = g.regions.filter((r) => r.ctrl === 'POL').map((r) => r.id);
    g.reports.push(report(g, 'SOV', a, { defNat: 'POL' }), report(g, 'SOV', b, { defNat: 'POL' }));
    commandMonth(g);
    expect(armsOf(g, 'SOV')!.land.earned).toBe(0.01);
    expect(commandOf(g, 'SOV')).toBe(0.01);
  });

  it('falls 0.01 for each corps taken whole, never under 0 nor over 1; a corps shattered in battle was not taken whole', () => {
    const g = open(true);
    const uk = g.corps.filter((c) => c.nat === 'UK' && !c.dead && c.kind === 'inf');
    storyFallen(g, uk[0], 'surrendered', 'GER', uk[0].reg);
    removeCorps(g, uk[0]);
    storyFallen(g, uk[1], 'starved', 'GER', uk[1].reg);
    removeCorps(g, uk[1]);
    storyFallen(g, uk[2], 'destroyed', 'GER', uk[2].reg);
    removeCorps(g, uk[2]);
    expect(corpsTakenWhole(g, 'UK')).toBe(2);
    commandMonth(g);
    expect(armsOf(g, 'UK')!.land.earned).toBe(0.48);
    // A month that carried an offensive through and lost a corps whole comes out even.
    const [a, b] = g.regions.filter((r) => r.ctrl === 'GER').map((r) => r.id);
    g.stories.fallen.length = 0;
    storyFallen(g, uk[3], 'surrendered', 'GER', uk[3].reg);
    g.reports.push(report(g, 'UK', a), report(g, 'UK', b));
    commandMonth(g);
    expect(armsOf(g, 'UK')!.land.earned).toBe(0.48);
    // The Soviet army, at 0, has nothing to lose.
    g.stories.fallen.length = 0;
    const sov = g.corps.find((c) => c.nat === 'SOV' && !c.dead && c.kind === 'inf')!;
    storyFallen(g, sov, 'surrendered', 'GER', sov.reg);
    commandMonth(g);
    expect(armsOf(g, 'SOV')!.land.earned).toBe(0);
    // Nor does an army at 1 rise past it.
    earn(g, 'GER', 1);
    g.stories.fallen.length = 0;
    g.reports.push(report(g, 'GER', g.regions.find((r) => r.ctrl === 'POL')!.id), report(g, 'GER', g.regions.filter((r) => r.ctrl === 'POL')[1].id));
    commandMonth(g);
    expect(armsOf(g, 'GER')!.land.earned).toBe(1);
  });

  it('moves at each month\'s end by that month\'s battles, for every nation in a computer war', () => {
    const g = open(true, 2);
    const before = Object.fromEntries(Object.keys(g.nations).map((n) => [n, armsOf(g, n)!.land.earned]));
    const turn = g.turn;
    runTurnAI(g);
    expect(g.turn).toBe(turn + 1);
    let moved = 0;
    for (const nat of Object.keys(g.nations)) {
      const regions = new Set(g.reports.filter((r) => r.turn === turn && r.attNat === nat && r.taken && !r.siege && !r.relief && !r.sally).map((r) => r.region)).size;
      const whole = g.stories.fallen.filter((f) => f.turn === turn && f.nat === nat && f.how !== 'destroyed').length;
      const want = Math.min(1, Math.max(0, before[nat] + (regions >= 2 ? 0.01 : 0) - 0.01 * whole));
      expect(armsOf(g, nat)!.land.earned, nat).toBeCloseTo(want, 9);
      if (Math.abs(want - before[nat]) > 1e-9) moved++;
    }
    // Germany carries its offensive in Poland through in September.
    expect(armsOf(g, 'GER')!.land.earned).toBeCloseTo(0.86, 9);
    expect(moved).toBeGreaterThan(0);
  });

  it('is nothing off: no store, no news, no change', () => {
    const g = open(false);
    const [a, b] = g.regions.filter((r) => r.ctrl === 'POL').map((r) => r.id);
    g.reports.push(report(g, 'GER', a, { defNat: 'POL' }), report(g, 'GER', b, { defNat: 'POL' }));
    const news = g.headlines.length;
    commandMonth(g);
    expect('qs' in g).toBe(false);
    expect(g.headlines.length).toBe(news);
  });
});

describe('history\'s reforms of an army under the switch (#2 form Part 1 section 1; choice 8)', () => {
  const told = (g: Game, id: string): number => g.headlines.filter((h) => h.text === DIPLOMACY.events.find((e) => e.id === id)!.text).length;

  it('applies no reform\'s training, slow command or most corps; the air parts as before', () => {
    const g = open(true);
    const army = (nat: string) => ({ quality: g.nations[nat].quality, slowCommand: g.nations[nat].slowCommand, maxAttack: g.nations[nat].maxAttack });
    const sov = army('SOV'), ger = army('GER'), usa = army('USA'), ita = army('ITA');
    // The Red Army's: its air as before, and its text is not told on the calendar.
    expect(fireEvent(g, 'red-army-1943')).toBe(true);
    expect(army('SOV')).toEqual(sov);
    expect(g.nations.SOV.air).toBe(100);
    expect(told(g, 'red-army-1943')).toBe(0);
    // The German army's has nothing else: it does not fire at all.
    expect(fireEvent(g, 'german-army-1944')).toBe(false);
    expect(army('GER')).toEqual(ger);
    expect(told(g, 'german-army-1944')).toBe(0);
    // The Allies' keeps its air and its text, without the American training.
    expect(fireEvent(g, 'allied-armies-1944')).toBe(true);
    expect(army('USA')).toEqual(usa);
    expect(g.nations.USA.air).toBe(140);
    expect(g.nations.UK.air).toBe(90);
    expect(told(g, 'allied-armies-1944')).toBe(1);
    // Italy's heart is news only: morale must carry it.
    expect(fireEvent(g, 'italy-loses-heart')).toBe(true);
    expect(army('ITA')).toEqual(ita);
    expect(told(g, 'italy-loses-heart')).toBe(1);
    expect(armyTraining(g, 'ITA')).toBe(0.75);
  });

  it('applies them whole off, as today', () => {
    const g = open(false);
    expect(fireEvent(g, 'red-army-1943')).toBe(true);
    expect(g.nations.SOV).toMatchObject({ quality: 0.8, slowCommand: false, maxAttack: 14, air: 100 });
    expect(told(g, 'red-army-1943')).toBe(1);
    expect(fireEvent(g, 'german-army-1944')).toBe(true);
    expect(g.nations.GER.quality).toBe(1.15);
    expect(fireEvent(g, 'italy-loses-heart')).toBe(true);
    expect(g.nations.ITA.quality).toBe(0.45);
  });

  it('tells the Red Army\'s text once, at the first month\'s end its command stands at 0.5 or more, at war with Germany', () => {
    const g = open(true);
    commandMonth(g);
    expect(told(g, 'red-army-1943')).toBe(0);
    earn(g, 'SOV', 0.5);
    // Not while the event's own conditions fail: the Soviet Union is not at war with Germany.
    commandMonth(g);
    expect(told(g, 'red-army-1943')).toBe(0);
    setWar(g, 'SOV', 'GER');
    commandMonth(g);
    expect(told(g, 'red-army-1943')).toBe(1);
    expect(g.qs!.news['SOV:command:0.5']).toBe(g.turn);
    earn(g, 'SOV', 0.2);
    commandMonth(g);
    earn(g, 'SOV', 0.8);
    commandMonth(g);
    expect(told(g, 'red-army-1943')).toBe(1);
    // The reform itself never touched the army.
    expect(g.nations.SOV.quality).toBe(0.7);
  });

  it('tells the German army\'s text once, when its mean corps experience falls under 0.5 from 0.5 or more', () => {
    const g = open(true);
    setWar(g, 'GER', 'SOV');
    const ger = g.corps.filter((c) => c.nat === 'GER' && !c.dead);
    // It starts under 0.5: it has not fallen there, so nothing is told.
    commandMonth(g);
    expect(told(g, 'german-army-1944')).toBe(0);
    for (const c of ger) c.xp = 0.6;
    commandMonth(g);
    expect(told(g, 'german-army-1944')).toBe(0);
    for (const c of ger) c.xp = 0.45;
    commandMonth(g);
    expect(told(g, 'german-army-1944')).toBe(1);
    for (const c of ger) c.xp = 0.6;
    commandMonth(g);
    for (const c of ger) c.xp = 0.4;
    commandMonth(g);
    expect(told(g, 'german-army-1944')).toBe(1);
    expect(g.nations.GER.quality).toBe(1.25);
    expect(armyTraining(g, 'GER')).toBe(1.25);
  });
});

describe('one rule for every nation: a swap of every cause swaps the attack', () => {
  it('swaps the Soviet and German attack worth when their training, command and rows are swapped', () => {
    const g = open(true);
    // The kit each nation's designs give is its own (research.ts lead, by nation): laid aside here, so the tree alone counts.
    Reflect.deleteProperty(g.nations.GER, 'lab');
    Reflect.deleteProperty(g.nations.SOV, 'lab');
    const ground = plains(g);
    const base = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!;
    const ger = twin(base, { id: 93000, org: 1, str: 1, xp: 0.3, supplied: true, elite: undefined });
    const sov = twin(ger, { id: 93001, nat: 'SOV' });
    const worth = (c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk * attackCommand(g, c.nat) * traitAttack(g, c, ground) * techMul(g, c.nat, 'doctrine');
    const wGer = worth(ger), wSov = worth(sov);
    expect(wGer / wSov).toBeGreaterThan(1.5);
    const G = g.nations.GER, S = g.nations.SOV;
    [G.tech, S.tech] = [S.tech, G.tech];
    [g.qs!.nat.GER.land, g.qs!.nat.SOV.land] = [g.qs!.nat.SOV.land, g.qs!.nat.GER.land];
    // Their own rows (quality, slow command, most corps, blitz) are left as they were: none is read.
    expect(worth(ger)).toBeCloseTo(wSov, 12);
    expect(worth(sov)).toBeCloseTo(wGer, 12);
    expect(blitzGate(g, 'SOV')).toBe(true);
    expect(blitzGate(g, 'GER')).toBe(false);
    expect(mostCorps(g, 'SOV')).toBe(20);
    expect(mostCorps(g, 'GER')).toBe(8);
    expect(slowHabits(g, 'GER')).toBe(true);
    expect(slowHabits(g, 'SOV')).toBe(false);
  });
});

describe('the nation card under the switch: the blitz rows are no nation\'s own (#2 form Part 1 section 1)', () => {
  const t = (nat: string, id: string) => traitsOf(nat).find((x) => x.id === id)!;
  const line = "Not this army's own with Units decide: any army that holds Field radios with a command of 0.6 or more strikes this way.";

  it('gives Germany\'s Lightning war and Armoured spearheads to any army through the gate, with what they do', () => {
    for (const id of UNITS.land.blitzTraits) {
      const words = traitCardRule(t('GER', id), 100, true);
      expect(words.startsWith(line), id).toBe(true);
      expect(words.endsWith(traitRule(t('GER', id), 100)), id).toBe(true);
    }
    expect(traitCardRule(t('GER', 'panzer'), 100, true)).toBe(`${line} Attack up 15% for tanks and motorised troops in plains and desert.`);
  });

  it('keeps traitRule\'s words off, and for every other row on', () => {
    for (const id of UNITS.land.blitzTraits) expect(traitCardRule(t('GER', id), 100, false)).toBe(traitRule(t('GER', id), 100));
    expect(traitCardRule(t('GER', 'uboats'), 100, true)).toBe(traitRule(t('GER', 'uboats'), 100));
    expect(traitCardRule(t('UK', 'radar'), 100, true)).toBe(traitRule(t('UK', 'radar'), 100));
    // The convoy war's rows as the convoy slice left them: not in force with the switch, as they were without it.
    expect(traitCardRule(t('UK', 'signals'), 100, true)).toBe('Not in force: with Units decide, convoys are kept safe by escorts, aircraft and their equipment.');
    expect(traitCardRule(t('UK', 'signals'), 100, false)).toBe(traitRule(t('UK', 'signals'), 100));
  });

  it('is what the card shows', () => {
    const body = page.slice(page.indexOf('function traitList('), page.indexOf('function traitsCard('));
    expect(body).toContain('const causes = isOpen(g) ? unitsDecide(g) : UNITS_STAGE;');
    expect(body).toContain('traitCardRule(t, BUDGET.scale, causes)');
    expect(body).not.toMatch(/traitRule\(/);
  });
});
