// Units decide's quality store (release 2; the design director's ruling of 24 Nov 2026 15:53Z on the rules builder's #2
// form, rules-builder/release2/soviet/form.md, Part 2): one store for soldiers, pilots and crews, seeded as a grand
// campaign opens with the switch, absent without it. Its figures are data/campaign/units.json; the research rows it
// adds (Field radios and the three training rows) are in the tree only with the switch, and Germany's radios and the
// Scandinavians' winter kit come from the switch's own start list, never tech.json's. The armies' earned command seeds
// are the design director's of 24 Nov 16:06Z (the form's 'Seeds ruled 16:07Z'). Then training and command (Part 1 2a,
// 2b and section 1, as ruled): training as the ladder, command c from radios and what is earned, the attack, the most
// corps, the slow habits and the blitz gate read from them for every nation, earned moved by the month's own battles,
// and history's reforms of an army not applied, their texts told by causes. Then experience, schools, titles and kit
// (Part 1 2d and section 1; Part 2 sections 2 and 4): experience by result, drafts that dilute every nation's corps at
// its schools' level (land.school), the Guards title earned as a name only, the heavy corps by its research row, the
// infantry's anti-tank guns by the defender's own rows, and winter kit in place of the hardy list.
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
import { KIND, type Award, type BattleReport, type Corps, type Hero, type Region } from '../src/campaign/types';
import { EASTERN, addCorps, capture, frozen, resolveBattle } from '../src/campaign/game';
import { fronts } from '../src/campaign/staff';
import { deliver } from '../src/campaign/economy';
import { campXp } from '../src/campaign/buildings';
import { STORIES, cashHonour, designation, drafted, fameOf, guardsWords, raisedXp, titledName, training, whyNotCash } from '../src/campaign/stories';
import { turnToDate } from '../src/core/ticket';
import { VALOUR, sendHero, sendWorth, whyNotSend } from '../src/campaign/valour';
import { eliteCount, eliteOf, eliteRule, traitDefence, whyNoElite } from '../src/campaign/traits';
import { SUPPORT, antiTank, armourOfTheDay, gunsOfRows, gunsYear, type Facing } from '../src/campaign/support';
import { winterAhead, winterReady } from '../src/campaign/tech';
import { earnGuards, schoolsOf, xpByResult } from '../src/campaign/units';
import traitsSource from '../src/campaign/traits.ts?raw';
import gameSource from '../src/campaign/game.ts?raw';
import foldSource from '../src/campaign/fold.ts?raw';
import economySource from '../src/campaign/economy.ts?raw';

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
  const line = "Not this army's own: any army that holds Field radios with a command of 0.6 or more strikes this way.";

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

// ---- Experience, schools, titles and kit (DD 24 Nov 15:53Z, #2 form Part 1 2d and section 1; Part 2 sections 2 and 4) ----

/**
 * September 1939: a Polish stack at Poznan attacked from Neumark by a German one, each of two infantry corps, with the
 * experience given; nobody else there. A test reads each corps' experience after the fight.
 */
function poznan(on: boolean, attXp: number, defXp: number): { g: Game; target: Region; att: Corps[]; def: Corps[] } {
  const g = startOpen(createGame(3, null));
  if (on) g.flags.unitsDecide = 1;
  const target = g.byName['Poznan'], from = g.byName['Neumark'];
  for (const u of [...target.units, ...from.units]) removeCorps(g, u);
  const def = [0, 1].map((i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 2, target.id, {}));
  const att = [0, 1].map((i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', 3, from.id, {}));
  for (const c of def) c.xp = defXp;
  for (const c of att) c.xp = attXp;
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  return { g, target, att, def };
}

describe('experience by result (#2 form Part 1 2d)', () => {
  it('gains 0.05, 0.02, 0.04 and 0.02 of what is left to learn: won, repulsed, held, beaten', () => {
    expect(xpByResult(0.3, 'att', true)).toBeCloseTo(0.3 + 0.05 * 0.7, 12);
    expect(xpByResult(0.3, 'att', false)).toBeCloseTo(0.3 + 0.02 * 0.7, 12);
    expect(xpByResult(0.2, 'def', true)).toBeCloseTo(0.2 + 0.04 * 0.8, 12);
    expect(xpByResult(0.2, 'def', false)).toBeCloseTo(0.2 + 0.02 * 0.8, 12);
    // Never past 1, and nothing to learn at 1.
    expect(xpByResult(1, 'att', true)).toBe(1);
    expect(xpByResult(0.99, 'att', true)).toBeLessThan(1);
  });

  it('is what a battle gives under the switch, for the attacker that carries the day and the defender beaten', () => {
    const f = poznan(true, 0.3, 0.2);
    resolveBattle(f.g, f.att, f.target, { fought: 3 });
    for (const c of f.att) expect(c.xp, c.name).toBeCloseTo(0.3 + 0.05 * 0.7, 9);
    for (const c of f.def) expect(c.xp, c.name).toBeCloseTo(0.2 + 0.02 * 0.8, 9);
  });

  it('and for the attacker repulsed and the defender that holds', () => {
    const f = poznan(true, 0.3, 0.2);
    resolveBattle(f.g, f.att, f.target, { fought: 0.6 });
    for (const c of f.att) expect(c.xp, c.name).toBeCloseTo(0.3 + 0.02 * 0.7, 9);
    for (const c of f.def) expect(c.xp, c.name).toBeCloseTo(0.2 + 0.04 * 0.8, 9);
  });

  it('is today\'s flat +0.05 and +0.04 off, win or lose', () => {
    for (const fought of [3, 0.6]) {
      const f = poznan(false, 0.3, 0.2);
      resolveBattle(f.g, f.att, f.target, { fought });
      for (const c of f.att) expect(c.xp, `${fought} ${c.name}`).toBeCloseTo(0.35, 9);
      for (const c of f.def) expect(c.xp, `${fought} ${c.name}`).toBeCloseTo(0.24, 9);
    }
  });
});

describe('drafts and the schools: land.school, for every nation (#2 form Part 1 2d; Part 2 section 2)', () => {
  it('dilutes every nation\'s corps, the computer\'s too, at the level its schools give; off only the player\'s', () => {
    for (const on of [true, false]) {
      const g = open(on);
      expect(g.human).toBeNull();
      const c = g.corps.find((x) => x.nat === 'SOV' && x.kind === 'inf' && !x.dead)!;
      c.xp = 0.5;
      drafted(g, c, 0.6, 0.4);
      const w = 0.4 * STORIES.veterans.dilute;
      expect(c.xp, String(on)).toBeCloseTo(on ? (0.5 * 0.6 + STORIES.veterans.green * w) / (0.6 + w) : 0.5, 12);
    }
  });

  it('reads the store\'s land.school, never g.stories.schools, under the switch; off g.stories.schools, as today', () => {
    const g = open(true);
    const P = STORIES.veterans.perSchool;
    expect(armsOf(g, 'UK')!.land.school).toBe(0);
    expect(schoolsOf(g, 'UK')).toBe(0);
    g.stories.schools.UK = 6;
    expect(training(g, 'UK')).toBeCloseTo(STORIES.veterans.green, 12);
    expect(raisedXp(g, 'UK')).toBeCloseTo(STORIES.veterans.raised, 12);
    armsOf(g, 'UK')!.land.school = 4;
    expect(schoolsOf(g, 'UK')).toBe(4);
    expect(training(g, 'UK')).toBeCloseTo(STORIES.veterans.green + 4 * P, 12);
    expect(raisedXp(g, 'UK')).toBeCloseTo(STORIES.veterans.raised + 4 * P, 12);
    // A computer corps' drafts come at that level.
    const c = g.corps.find((x) => x.nat === 'UK' && x.kind === 'inf' && !x.dead)!;
    c.xp = 0.5;
    drafted(g, c, 0.5, 0.5);
    const w = 0.5 * STORIES.veterans.dilute;
    expect(c.xp).toBeCloseTo((0.5 * 0.5 + (STORIES.veterans.green + 4 * P) * w) / (0.5 + w), 12);
    // Off, g.stories.schools, as today.
    const off = open(false);
    off.stories.schools.UK = 6;
    expect(schoolsOf(off, 'UK')).toBe(6);
    expect(training(off, 'UK')).toBeCloseTo(STORIES.veterans.green + 6 * P, 12);
    expect('qs' in off).toBe(false);
  });

  it('raises every nation\'s new corps from its schools, the computer\'s too; off a computer\'s at 0.05', () => {
    for (const on of [true, false]) {
      const g = open(on);
      if (on) armsOf(g, 'UK')!.land.school = 4;
      else g.stories.schools.UK = 4;
      const n = g.nations.UK;
      n.queue.push({ id: g.nextBuild++, what: 'corps', kind: 'inf', divs: 3, name: 'CXX Corps', region: n.capital, ordered: g.turn, eta: g.turn, cost: 0 });
      deliver(g, 'UK');
      const c = g.corps.find((x) => x.nat === 'UK' && x.name === 'CXX Corps')!;
      const base = on ? STORIES.veterans.raised + 4 * STORIES.veterans.perSchool : 0.05;
      expect(c.xp, String(on)).toBeCloseTo(base + campXp(g, c.reg), 12);
    }
  });

  it('takes the player\'s schools order into land.school under the switch, and an airman\'s into none', () => {
    const g = createGame(1, 'GER');
    g.flags.unitsDecide = 1;
    startOpen(g);
    const c = g.corps.find((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead && x.supplied)!;
    const a: Award = { id: g.stories.awards.length + 1, turn: g.turn, hon: 'rock', kind: 'corps', unit: c.id, name: c.name, nat: 'GER', at: c.reg };
    g.stories.awards.push(a);
    (c.hon ??= []).push(a.id);
    const fame = fameOf(a);
    expect(whyNotCash(g, a, 'school')).toBe('');
    expect(cashHonour(g, a.id, 'school')).toBe(true);
    expect(armsOf(g, 'GER')!.land.school).toBe(fame);
    expect(g.stories.schools.GER).toBeUndefined();
    expect(training(g, 'GER')).toBeCloseTo(STORIES.veterans.green + STORIES.veterans.perSchool * fame, 12);
    // The holder of the highest award from the army: into land.school too.
    const soldier: Hero = { id: 1, turn: g.turn, nat: 'GER', kind: 'corps', unit: c.id, name: c.name, who: 'A sergeant', at: -1, award: a.id };
    (g.stories.heroes ??= []).push(soldier);
    c.supplied = true;
    expect(whyNotSend(g, soldier, 'school')).toBe('');
    expect(sendHero(g, soldier.id, 'school')).toBe(true);
    expect(armsOf(g, 'GER')!.land.school).toBe(fame + VALOUR.school.steps);
    // An airman's school is the air arm's, never the army's drafts': his pilots come, and land.school stays.
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    g.airPool.GER = { ftr: 0, bmr: 0, pilots: 100, lostFtr: 0, lostBmr: 0, lostPilots: 0 };
    const airman: Hero = { id: 2, turn: g.turn, nat: 'GER', kind: 'air', unit: f.id, name: f.name, who: 'A pilot', at: -1, award: a.id };
    g.stories.heroes.push(airman);
    expect(sendWorth(airman, 'school', g)).toBe(`${STORIES.cash.pilots} more trained pilots at once`);
    expect(sendHero(g, airman.id, 'school')).toBe(true);
    expect(armsOf(g, 'GER')!.land.school).toBe(fame + VALOUR.school.steps);
    expect(g.airPool.GER.pilots).toBe(100 + STORIES.cash.pilots);
    expect(g.headlines.at(-1)!.text).toMatch(/more trained pilots join the reserve\.$/);
    // The schools take so many, counted in land.school.
    armsOf(g, 'GER')!.land.school = STORIES.veterans.maxSchools;
    expect(whyNotSend(g, { ...soldier, id: 3, sent: undefined }, 'school')).toBe('The schools have all the instructors they can use.');
  });

  it('is the count the Honours panel shows: the schools the drafts read, never g.stories.schools', () => {
    expect(page).toContain('const schools = schoolsOf(g, me);');
    expect(page).not.toMatch(/S\.schools\[/);
    const g = open(true);
    armsOf(g, 'GER')!.land.school = 4;
    expect(schoolsOf(g, 'GER')).toBe(4);
    expect(g.stories.schools.GER ?? 0).toBe(0);
    const off = open(false);
    off.stories.schools.GER = 2;
    expect(schoolsOf(off, 'GER')).toBe(2);
  });

  it('is today\'s off: a schools order into g.stories.schools, an airman\'s too', () => {
    const g = createGame(1, 'GER');
    startOpen(g);
    const f = g.airFleets.find((x) => x.nat === 'GER')!;
    const airman: Hero = { id: 1, turn: g.turn, nat: 'GER', kind: 'air', unit: f.id, name: f.name, who: 'A pilot', at: -1, award: 1 };
    (g.stories.heroes ??= []).push(airman);
    expect(sendWorth(airman, 'school', g)).toBe(sendWorth(airman, 'school'));
    expect(sendHero(g, airman.id, 'school')).toBe(true);
    expect(g.stories.schools.GER).toBe(VALOUR.school.steps);
    expect('qs' in g).toBe(false);
  });
});

describe('the Guards title, earned, as a name only (#2 form Part 1 section 1; Part 2 section 4; choice 13)', () => {
  it('goes to a corps of any nation at experience 0.5 with three provinces taken, the month it is earned; off, to none', () => {
    const g = open(true);
    const cs = ['UK', 'SOV', 'ITA'].map((n) => g.corps.find((x) => x.nat === n && x.kind === 'inf' && !x.dead)!);
    for (const c of cs) { c.xp = 0.5; c.rec = { taken: 3 }; }
    const [, short, green] = cs;
    short.rec = { taken: 2, won: 6 };
    green.xp = 0.49;
    earnGuards(g, cs);
    expect(cs[0].guards).toBe(g.turn);
    expect(short.guards).toBeUndefined();
    expect(green.guards).toBeUndefined();
    // Earned once: a later month does not move it.
    g.turn += 3;
    earnGuards(g, cs);
    expect(cs[0].guards).toBe(g.turn - 3);
    const off = open(false);
    const c = off.corps.find((x) => x.nat === 'UK' && x.kind === 'inf' && !x.dead)!;
    c.xp = 0.9; c.rec = { taken: 9 };
    earnGuards(off, [c]);
    expect('guards' in c).toBe(false);
  });

  it('is earned in the battle that brings the third province taken and the experience', () => {
    const f = poznan(true, 0.49, 0.2);
    for (const c of f.att) c.rec = { taken: 2 };
    resolveBattle(f.g, f.att, f.target, { fought: 3 });
    for (const c of f.att) {
      expect(c.rec!.taken).toBe(3);
      expect(c.xp).toBeGreaterThanOrEqual(0.5);
      expect(c.guards, c.name).toBe(f.g.turn);
      expect(c.elite).toBeUndefined();
    }
    for (const c of f.def) expect(c.guards).toBeUndefined();
    // Off, the same fight writes no title.
    const o = poznan(false, 0.49, 0.2);
    for (const c of o.att) c.rec = { taken: 2 };
    resolveBattle(o.g, o.att, o.target, { fought: 3 });
    for (const c of o.att) expect('guards' in c).toBe(false);
  });

  it('adds nothing to a corps\' worth and never touches c.elite or the count of special formations', () => {
    const g = open(true);
    const ground = plains(g);
    const c = g.corps.find((x) => x.nat === 'SOV' && x.kind === 'inf' && !x.dead)!;
    c.xp = 0.6; c.rec = { taken: 4 };
    const before = { base: corpsBase(g, c), att: traitAttack(g, c, ground), names: traitNames(g, [c], ground, 'attack'), count: eliteCount(g, 'SOV') };
    earnGuards(g, [c]);
    expect(c.guards).toBe(g.turn);
    expect(c.elite).toBeUndefined();
    expect({ base: corpsBase(g, c), att: traitAttack(g, c, ground), names: traitNames(g, [c], ground, 'attack'), count: eliteCount(g, 'SOV') }).toEqual(before);
  });

  it('is read only by the corps\' name and card: its nation\'s word after the number', () => {
    const g = open(true);
    const sov = twin(g.corps.find((x) => x.nat === 'SOV' && !x.dead)!, { name: 'III Corps', guards: 1 });
    const ger = twin(g.corps.find((x) => x.nat === 'GER' && !x.dead)!, { name: 'XIX Panzer Corps', guards: 1 });
    expect(titledName(sov)).toBe(`III ${designation('SOV')} Corps`);
    expect(titledName(sov)).toBe('III Guards Corps');
    expect(titledName(ger)).toBe(`XIX ${designation('GER')} Panzer Corps`);
    expect(titledName({ ...sov, guards: undefined })).toBe('III Corps');
    expect(titledName({ ...sov, name: 'I Guards Corps' })).toBe('I Guards Corps');
    expect(guardsWords(sov)).toBe('III Guards Corps: its men earned the title in September 1939, with experience of 50 or more and 3 provinces taken. A name only: what they have learned already counts in its strength.');
    expect(guardsWords({ ...sov, guards: undefined })).toBe('');
    // The card: the corps row and the army's card give the titled name and say what the title is.
    expect(page).toContain('<span class="nm">${esc(titledName(c))}');
    expect(page).toContain('<span class="utop"><b>${esc(titledName(c))}</b>');
    // The corps row, the army's card, and the honour's tag where the corps is also named in the order of the day.
    expect(page.match(/guardsWords\(c\)/g)?.length).toBe(3);
    // No rule reads it: not the battle's worth, the special formations, the folds or the cadres.
    for (const text of [traitsSource, gameSource, foldSource, economySource]) expect(text).not.toMatch(/(?<!land)\.guards\b/);
  });

  it('counts ground walked into after the defenders gave way, as its record\'s \'taken\' does, and says provinces taken', () => {
    // A real walk-in: the Poles give the ground up, and the province the Germans march into is their third.
    const f = poznan(true, 0.5, 0.2);
    for (const c of f.def) c.yields = true;
    for (const c of f.att) c.rec = { taken: 2 };
    const rep = resolveBattle(f.g, f.att, f.target);
    expect(rep.yielded).toBe(true);
    for (const c of f.att) {
      expect(c.rec!.taken, c.name).toBe(3);
      expect(c.guards, c.name).toBe(f.g.turn);
      expect(c.elite).toBeUndefined();
    }
    const c = f.att[0];
    expect(guardsWords(c)).toContain(`with experience of ${Math.round(UNITS.land.guards.xp * 100)} or more and ${UNITS.land.guards.taken} provinces taken.`);
    // A walk-in is not a battle, so the card does not say the title was earned in one.
    expect(guardsWords(c)).toContain(`its men earned the title in ${turnToDate(f.g.turn).label},`);
    for (const w of [guardsWords(c), whyNoElite(f.g, 'SOV')!]) { expect(w).not.toMatch(/attacks won|0\.5/); }
    expect(units.land._guards).toContain('provinces taken');
    expect(units.land._guards).not.toContain('attack wins only');
    expect(units.land.guards).not.toHaveProperty('won');
    // Off, the same walk-in counts the province and writes no title.
    const o = poznan(false, 0.5, 0.2);
    for (const c of o.def) c.yields = true;
    for (const c of o.att) c.rec = { taken: 2 };
    expect(resolveBattle(o.g, o.att, o.target).yielded).toBe(true);
    for (const c of o.att) { expect(c.rec!.taken).toBe(3); expect('guards' in c).toBe(false); }
  });

  it('wears the word once on the corps row where the corps is also named in the order of the day', () => {
    // The earned title's own tag stands aside for the honour's, which then tells both.
    expect(page).toContain('if (c.guards !== undefined && !isGuards(g, c)) tags.push(');
    expect(page).toContain('title="Named in the order of the day: the highest honour a formation can earn.${c.guards !== undefined ? ` ${esc(guardsWords(c))}` : \'\'}"');
  });

  it('takes the Soviet Guards corps out of the arsenal and out of battle under the switch: neither bought nor applied', () => {
    const g = open(true);
    const ground = plains(g);
    const u = eliteOf('SOV')!;
    expect(UNITS.land.guards.elite).toContain(u.id);
    g.turn = 41; // January 1943: past the row's year
    expect(whyNoElite(g, 'SOV')).toBe('Not raised: the title is earned in battle. A corps of any army with experience of 50 or more that has taken 3 provinces earns it, as a name only, since its experience already counts in its strength.');
    expect(eliteRule(u, true)).toBe(whyNoElite(g, 'SOV'));
    const c = twin(g.corps.find((x) => x.nat === 'SOV' && x.kind === 'inf' && !x.dead)!, { id: 93100, elite: u.id });
    const plain = twin(c, { id: 93101, elite: undefined });
    expect(traitAttack(g, c, ground)).toBe(traitAttack(g, plain, ground));
    expect(traitDefence(g, c, ground)).toBe(traitDefence(g, plain, ground));
    expect(traitNames(g, [c], ground, 'attack')).not.toContain(u.label);
    // Off, as today: bought from 1942, and worth its x1.15.
    const off = open(false);
    expect(whyNoElite(off, 'SOV')).toBe('Not before 1942.');
    off.turn = 41;
    expect(whyNoElite(off, 'SOV')).toBeNull();
    expect(traitAttack(off, c, off.regions[ground.id]) / traitAttack(off, plain, off.regions[ground.id])).toBeCloseTo(u.attack!, 12);
    expect(eliteRule(u)).toContain('Raised from 1942.');
  });
});

describe('the heavy tank corps needs Heavy tanks, not a year (#2 form Part 1 section 1)', () => {
  it('waits on the research row under the switch, still Germany\'s unit; off, on 1942', () => {
    const g = open(true);
    const u = eliteOf('GER')!;
    expect(UNITS.land.raisedBy[u.id]).toBe('heavies');
    expect(hasTech(g, 'GER', 'heavies')).toBe(false);
    expect(whyNoElite(g, 'GER')).toBe('Needs the technology Heavy tanks. Open Research to begin it.');
    g.turn = 41;
    expect(whyNoElite(g, 'GER')).toBe('Needs the technology Heavy tanks. Open Research to begin it.');
    g.turn = 1;
    give(g, 'GER', 'heavies');
    expect(whyNoElite(g, 'GER')).toBeNull();
    expect(eliteRule(u, true)).toContain('Raised once Heavy tanks is researched.');
    expect(eliteRule(u, true)).not.toContain('1942');
    // Off, the year, held or not.
    const off = open(false);
    give(off, 'GER', 'heavies');
    expect(whyNoElite(off, 'GER')).toBe('Not before 1942.');
    expect(eliteRule(u)).toContain('Raised from 1942.');
    // The cards pass the switch.
    expect(page).toContain('eliteRule(special, unitsDecide(g))');
    expect(page).toContain('eliteRule(u, causes)');
  });
});

describe('anti-tank guns by the defender\'s own rows, not the year (#2 form Part 1 section 1)', () => {
  const B = (y: string) => SUPPORT.antiTank.byYear[y];
  const facing: Facing = { works: () => 0, guns: 0, mines: 0, sappers: 0, fort: 0, foot: 1, tanks: 1 };

  it('gives 1939\'s guns with no row, 1941\'s with Anti-tank guns, 1943\'s with Tank destroyers as well', () => {
    const g = open(true);
    take(g, 'UK', 'antitank', 'tankdestroyers');
    expect(gunsOfRows(g, 'UK')).toEqual(B('1939'));
    give(g, 'UK', 'tankdestroyers');
    expect(gunsOfRows(g, 'UK')).toEqual(B('1939'));
    give(g, 'UK', 'antitank');
    expect(gunsOfRows(g, 'UK')).toEqual(B('1943'));
    take(g, 'UK', 'tankdestroyers');
    expect(gunsOfRows(g, 'UK')).toEqual(B('1941'));
    expect(UNITS.land.antiTank).toEqual({ rows: ['antitank', 'tankdestroyers'], years: [1939, 1941, 1943] });
  });

  it('is what a defence carries under the switch, whatever the year; off, the year\'s guns, at half without the row', () => {
    for (const turn of [1, 41]) {
      const g = open(true), off = open(false);
      for (const x of [g, off]) x.turn = turn;
      const att = (x: Game) => [x.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!];
      const pol = (x: Game) => x.corps.filter((c) => c.nat === 'POL' && c.kind === 'inf' && !c.dead).slice(0, 2);
      const ger = (x: Game) => x.corps.filter((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead).slice(0, 2);
      const at = (x: Game, d: Corps[], works: number) => antiTank(x, att(x), d, plains(x), { ...facing, works: () => works }).guns;
      expect(hasTech(g, 'POL', 'antitank')).toBe(true);
      expect(hasTech(g, 'GER', 'antitank')).toBe(false);
      // Poland holds Anti-tank guns: 1941's guns, in the open and behind works, in 1939 as in 1943.
      expect(at(g, pol(g), 0)).toBeCloseTo(B('1941')[0], 12);
      expect(at(g, pol(g), 1)).toBeCloseTo(B('1941')[1], 12);
      // Germany holds neither row: 1939's guns, in full, since the row prices the want of them.
      expect(at(g, ger(g), 1)).toBeCloseTo(B('1939')[1], 12);
      // Off, the year's guns, and half of them without the row.
      const day = turn === 1 ? B('1939') : B('1943');
      expect(at(off, pol(off), 1), String(turn)).toBeCloseTo(day[1], 12);
      expect(at(off, ger(off), 1), String(turn)).toBeCloseTo(day[1] * SUPPORT.antiTank.without, 12);
    }
  });

  it('measures the attacker\'s armour against the tanks the defence\'s own guns were built to stop, whatever the year; off, the year\'s', () => {
    const step = SUPPORT.antiTank.armourStep;
    const fight = (on: boolean, turn: number, rows: string[], twinNat?: string) => {
      const x = open(on);
      x.turn = turn;
      take(x, 'GER', 'mediums', 'heavies', 'latetanks');
      give(x, 'GER', 'mediums', 'heavies');
      take(x, 'POL', 'antitank', 'tankdestroyers');
      give(x, 'POL', ...rows);
      const att = [x.corps.find((c) => c.nat === 'GER' && c.kind === 'arm' && !c.dead)!];
      const pol = x.corps.find((c) => c.nat === 'POL' && c.kind === 'inf' && !c.dead)!;
      const def = twinNat ? [pol, twin(pol, { id: 93200, nat: twinNat })] : [pol];
      if (twinNat) take(x, twinNat, 'antitank', 'tankdestroyers');
      return antiTank(x, att, def, plains(x), facing).armour;
    };
    expect(armourOfTheDay(1939)).toBe(0);
    expect(armourOfTheDay(1941)).toBe(1);
    expect(armourOfTheDay(1943)).toBe(2);
    for (const [rows, due] of [[[], 0], [['antitank'], 1], [['antitank', 'tankdestroyers'], 2]] as [string[], number][]) {
      // The same rows on both sides: the same armour step in June 1940 as in October 1943.
      const early = fight(true, 10, rows), late = fight(true, 50, rows);
      expect(early, rows.join()).toBeCloseTo(step * (2 - due), 12);
      expect(late, rows.join()).toBe(early);
    }
    // A defence of mixed rows is weighed as its guns are: here half with Anti-tank guns, half with neither.
    expect(fight(true, 10, ['antitank'], 'UK')).toBeCloseTo(step * (2 - 0.5), 12);
    // Off, the calendar's tanks, as today: two steps ahead in 1940, level in 1943.
    expect(fight(false, 10, ['antitank'])).toBeCloseTo(step * 2, 12);
    expect(fight(false, 50, ['antitank'])).toBeCloseTo(0, 12);
    // The year of the guns is the defender's rows, as gunsOfRows reads them.
    const g = open(true);
    take(g, 'UK', 'antitank', 'tankdestroyers');
    expect(gunsYear(g, 'UK')).toBe(1939);
    give(g, 'UK', 'antitank');
    expect(gunsYear(g, 'UK')).toBe(1941);
    expect(gunsOfRows(g, 'UK')).toEqual(B('1941'));
  });

  it('says on the research card what each step\'s guns take off, never a year or the day', () => {
    const g = open(true), off = open(false);
    const at = techRule(techById('antitank')!, g, 'GER'), td = techRule(techById('tankdestroyers')!, g, 'GER');
    expect(at).toContain(`Its infantry's own anti-tank guns take ${Math.round(B('1941')[0] * 100)}% of the shock out of a tank attack in the open and ${Math.round(B('1941')[1] * 100)}% behind finished fieldworks, where without them they take ${Math.round(B('1939')[0] * 100)}%`);
    expect(td).toContain(`they take ${Math.round(B('1943')[0] * 100)}% of the shock out of a tank attack in the open and ${Math.round(B('1943')[1] * 100)}% behind finished fieldworks, where Anti-tank guns alone take ${Math.round(B('1941')[0] * 100)}%`);
    for (const w of [at, td]) { expect(w).not.toMatch(/of the day|19\d\d/); }
    expect(techRule(techById('antitank')!, off, 'GER')).toContain('the anti-tank guns of the day');
  });
});

describe('the hardy list\'s three reads go to winter kit, for every nation (#2 form Part 1 section 1)', () => {
  const DEC = 4; // December 1939
  const sovGround = (g: Game): Region => g.regions.find((r) => r.owner === 'SOV' && r.ctrl === 'SOV' && r.nb.every((j) => g.regions[j].owner === 'SOV' && g.regions[j].ctrl === 'SOV') && !r.cap && r.terrain === 'plains')!;

  it('frozen: an army without winter kit, whoever it is; off the hardy list spares its own', () => {
    for (const on of [true, false]) {
      const g = open(on);
      g.turn = DEC;
      const r = sovGround(g);
      expect(winterReady(g, 'SOV') && winterReady(g, 'FIN')).toBe(true);
      expect(frozen(g, 'SOV', r)).toBe(false);
      expect(frozen(g, 'GER', r)).toBe(true);
      // Sweden and Norway hold the kit from the switch's start list; off the hardy list spares them.
      expect(frozen(g, 'SWE', r)).toBe(false);
      expect(winterReady(g, 'SWE'), String(on)).toBe(on);
      // A hardy army that has lost its kit freezes under the switch; off it is spared.
      take(g, 'SOV', 'winterkit');
      expect(frozen(g, 'SOV', r), String(on)).toBe(on);
      give(g, 'GER', 'winterkit');
      expect(frozen(g, 'GER', r)).toBe(false);
    }
  });

  it('the player\'s winter advice: an army without the kit, never the hardy list', () => {
    for (const on of [true, false]) {
      const g = createGame(1, 'FIN');
      if (on) g.flags.unitsDecide = 1;
      startOpen(g);
      expect(winterAhead(g, 'FIN')).toBe(false);
      take(g, 'FIN', 'winterkit');
      expect(winterAhead(g, 'FIN'), String(on)).toBe(on);
    }
  });

  it('a staff\'s season reckoning: the winter by the kit, and the mud alike for every staff', () => {
    // A Soviet province deep in the Soviet Union held by Italy, at war with it and with nobody else: each staff's front
    // there is on Soviet ground, so it is reckoned by the eastern seasons.
    const season = (on: boolean, turn: number, fix?: (g: Game) => void): { ITA: number; SOV: number } => {
      const g = open(on);
      const x = sovGround(g);
      for (const u of [...x.units]) removeCorps(g, u);
      capture(g, x, 'ITA');
      addCorps(g, 'ITA', 'Probe Corps', 'inf', 3, x.id, {}).supplied = true;
      setWar(g, 'ITA', 'SOV');
      fix?.(g);
      g.turn = turn;
      const atk = (nat: string): number => fronts(g, nat).filter((f) => (nat === 'ITA' ? f.ours : f.edge).includes(x.id)).reduce((s, f) => s + f.atk, 0);
      return { ITA: atk('ITA'), SOV: atk('SOV') };
    };
    const SEP = 1, NOV = 3, JAN = 5;
    for (const on of [true, false]) {
      const clear = season(on, SEP), mud = season(on, NOV), winter = season(on, JAN);
      expect(clear.ITA).toBeGreaterThan(0);
      expect(clear.SOV).toBeGreaterThan(0);
      expect(mud.ITA / clear.ITA, `${on} ITA mud`).toBeCloseTo(0.8, 9);
      expect(winter.ITA / clear.ITA, `${on} ITA winter`).toBeCloseTo(EASTERN.winter.attack, 9);
      // The Soviet staff: off the hardy list spares it the mud; under the switch the mud is the mud for everyone.
      expect(mud.SOV / clear.SOV, `${on} SOV mud`).toBeCloseTo(on ? 0.8 : 1, 9);
      expect(winter.SOV / clear.SOV, `${on} SOV winter`).toBeCloseTo(1, 9);
    }
    // The kit, never the list: an Italian staff with winter kit reckons the winter whole under the switch; off it does not.
    const kit = (g: Game) => give(g, 'ITA', 'winterkit');
    expect(season(true, JAN, kit).ITA / season(true, SEP, kit).ITA).toBeCloseTo(1, 9);
    expect(season(false, JAN, kit).ITA / season(false, SEP, kit).ITA).toBeCloseTo(EASTERN.winter.attack, 9);
    // And a Soviet staff without it reckons the winter's cost.
    const bare = (g: Game) => take(g, 'SOV', 'winterkit');
    expect(season(true, JAN, bare).SOV / season(true, SEP, bare).SOV).toBeCloseTo(EASTERN.winter.attack, 9);
  });
});

describe('the slice\'s new code names no nation', () => {
  it('reads no nation id: every per-nation figure is data', () => {
    const src = unitsSource.replace(/\/\*[\s\S]*?\*\//g, '').replace(/\/\/.*$/gm, '');
    expect(src).not.toMatch(/['"`](GER|SOV|FRA|UK|USA|ITA|JAP|SWE|NOR|FIN|HUN|POL)['"`]/);
    const L = UNITS.land;
    expect(L.guards).toEqual({ xp: 0.5, taken: 3, elite: ['guards'] });
    expect(L.raisedBy).toEqual({ heavy: 'heavies' });
  });
});
