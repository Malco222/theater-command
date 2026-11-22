// The fortress rows (release 2; the design director's 05:38Z, 06:12Z, 06:15Z, 06:17Z and 06:21Z of 22 November 2026,
// on the historian's historian/fortress-garrisons-by-line.md): the book's fortress crews by date at every stop, taking
// no place among the five, their works holding all round for their share of the defence, and beaten or cut off they
// stay in their works on their own stores. Switched by stage.json 'fortRows' (TC_FORTS): off, the game is release 1's.
import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import farWorld from '../data/campaign/far-world.json';
import stage from '../data/campaign/stage.json';
import { orderCorps, specOf } from '../src/campaign/economy';
import { FORT_ROWS, fortShape, fortsMonth } from '../src/campaign/fortrows';
import { addCorps, assess, corpsBase, createGame, fortRow, fortRows, fortShare, hasRoom, placed, removeCorps, resolveBattle, roomless, setWar } from '../src/campaign/game';
import { menLeft } from '../src/campaign/manpower';
import { loadGame, saveGame } from '../src/campaign/save';
import { besieged, garrisonOf, housed, siegeSupply } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { FORTS_STAGE } from '../src/campaign/stage';
import { runTurnAI } from '../src/campaign/turn';
import { FORT_MUL, KIND, type Corps, type Game, type Kind, type Region } from '../src/campaign/types';

/** A 1939 grand campaign with the fortress rows on (or off), the computer playing every seat. */
function open(on = true, seed = 1): Game {
  const g = createGame(seed, null);
  if (on) g.flags.fortRows = 1;
  return startOpen(g);
}
const twin = (g: Game): Game => loadGame(saveGame(g))!;
const fortsAt = (g: Game, r: Region): Corps[] => r.units.filter((u) => u.kind === 'fort' && !u.dead);
const row = (g: Game, nat: string, name: string): Corps | undefined => g.corps.find((c) => !c.dead && c.nat === nat && c.name === name);
const worth = (c: Corps): number => c.divs * c.str;
/** Takes every corps but those `keep` names out of a province. */
const clear = (g: Game, r: Region, keep: (c: Corps) => boolean = () => false): void => { for (const c of [...r.units]) if (!keep(c)) removeCorps(g, c); };
/** Release 1's fortress rows, on the first day of every 1939 campaign. */
const R1 = ['Metz fortified region', 'Lauter fortified region', 'Montmedy sector', 'Gibraltar garrison'];

describe('the switch', () => {
  it('is off in release 1: a campaign opens with release 1\'s four fortress rows and no others', () => {
    expect((stage as { fortRows?: boolean }).fortRows).toBe(false);
    if (FORTS_STAGE) return; // (a run with TC_FORTS set)
    const g = startOpen(createGame(1, null));
    expect(g.flags.fortRows).toBeUndefined();
    expect(fortRows(g)).toBe(false);
    expect(g.corps.filter((c) => c.kind === 'fort').map((c) => c.name).sort()).toEqual([...R1].sort());
    const metz = row(g, 'FRA', 'Metz fortified region')!;
    expect(roomless(g, metz)).toBe(false);
    expect(fortRow(g, metz)).toBe(false);
  });

  it('never reaches a history campaign, and a save keeps it', () => {
    const h = createGame(1, null);
    h.flags.fortRows = 1;
    expect(fortRows(h)).toBe(false);
    const g = open();
    expect(fortRows(g)).toBe(true);
    expect(fortRows(twin(g))).toBe(true);
    expect(twin(g).corps.filter((c) => c.kind === 'fort').length).toBe(g.corps.filter((c) => c.kind === 'fort').length);
  });
});

describe("the book's rows", () => {
  it('stand on the first day at their worth, in their provinces, beside release 1\'s', () => {
    const g = open();
    for (const r of FORT_ROWS.filter((x) => x[0] === 1)) {
      const c = row(g, r[1], r[2])!;
      expect(c, r[2]).toBeDefined();
      expect(c.kind).toBe('fort');
      expect(g.regions[c.reg].name).toBe(r[3]);
      expect(worth(c)).toBeCloseTo(r[4], 6);
      expect(c.hold).toBe(true);
      // Never filled past its own size.
      expect(c.full ?? 1).toBeCloseTo(fortShape(r[4]).str, 6);
    }
    for (const name of R1) expect(g.corps.some((c) => c.name === name && c.kind === 'fort')).toBe(true);
    // Italy's Guardia alla Frontiera on the first day: 1.4 (as ruled, each of the 3.3 split times 0.42).
    const gaf = g.corps.filter((c) => c.nat === 'ITA' && c.kind === 'fort').reduce((s, c) => s + worth(c), 0);
    expect(gaf).toBeCloseTo(1.407, 3);
    // Rows of later months are not there yet.
    expect(g.corps.some((c) => c.nat === 'SOV' && c.kind === 'fort')).toBe(false);
  });

  it('Italy grows to 3.35 in June 1940, row by row, and a row whose fellow is gone forms on its own', () => {
    const g = open();
    g.turn = 10;
    removeCorps(g, row(g, 'ITA', 'Guardia alla Frontiera, Lombardy')!);
    const lombardy = g.byName.Lombardy;
    fortsMonth(g, 'ITA');
    expect(worth(row(g, 'ITA', 'Guardia alla Frontiera, Piedmont')!)).toBeCloseTo(1.0, 6);
    const friuli = row(g, 'ITA', 'Guardia alla Frontiera, Friuli')!;
    expect(friuli.divs).toBe(2);
    expect(worth(friuli)).toBeCloseTo(1.35, 6);
    expect(row(g, 'ITA', 'Vallo Alpino battalions, Lombardy')!.reg).toBe(lombardy.id);
    expect(row(g, 'ITA', 'Vallo Alpino battalions, Piedmont')).toBeUndefined();
  });

  it('never forms on ground its country has lost (the Stalin Line, June 1941)', () => {
    const g = open();
    g.turn = 22;
    g.byName.Minsk.ctrl = 'GER';
    fortsMonth(g, 'SOV');
    expect(row(g, 'SOV', 'Minsk and Slutsk Fortified Regions')).toBeUndefined();
    expect(worth(row(g, 'SOV', 'Mozyr Fortified Region')!)).toBeCloseTo(0.2, 6);
    expect(g.regions[row(g, 'SOV', 'Mozyr Fortified Region')!.reg].name).toBe('Gomel');
    expect(g.corps.filter((c) => c.nat === 'SOV' && c.kind === 'fort')).toHaveLength(7);
  });

  it("at stops 1-2 the rows ORDERED OUT leave on their date, their men to the reserve; at stops 3-5 they stay", () => {
    for (const stop of [1, 3]) {
      const g = open();
      g.flags.hist = stop;
      g.turn = 10;
      const before = menLeft(g, 'FRA');
      fortsMonth(g, 'FRA');
      const upper = row(g, 'FRA', 'Upper Rhine fortified sectors'), lower = row(g, 'FRA', 'Lower Rhine fortified sector');
      if (stop === 1) {
        expect(upper).toBeUndefined();
        expect(lower).toBeUndefined();
        expect(menLeft(g, 'FRA') - before).toBeCloseTo(1.25 + 1.07, 6);
        // (Montherme was broken, not ordered out: it stays.)
        expect(row(g, 'FRA', '102nd Fortress Division')).toBeDefined();
      } else {
        expect(upper).toBeDefined();
        expect(lower).toBeDefined();
        expect(menLeft(g, 'FRA')).toBe(before);
      }
    }
  });
});

describe('a fortress row comes only from the book', () => {
  it('nothing a player or a staff can order is of the fortress kind', () => {
    const g = open();
    expect(specOf(g, 'fort')).toBeUndefined();
    g.nations.FRA.pp = 1e6;
    expect(orderCorps(g, 'FRA', 'fort', g.byName.Paris.id)).toBeNull();
    for (const k of Object.keys(KIND) as Kind[]) if (k !== 'fort') expect(specOf(g, k)?.label ?? '').not.toMatch(/fortress/i);
  });

  it('a year of the computer\'s war raises no fortress row outside the book', () => {
    const book = new Set<string>([
      ...FORT_ROWS.map((r) => `${r[1]}:${r[2]}`),
      ...[...(setup as any).corps, ...(setup as any).reinforcements, ...((setup as any).formations ?? [])].filter((r: unknown[]) => r.includes('fort')).map((r: any[]) => (typeof r[0] === 'number' ? `${r[1]}:${r[2]}` : `${r[0]}:${r[1]}`)),
    ]);
    for (const s of JSON.stringify(farWorld).matchAll(/"([A-Z]{3})",\s*"([^"]+)",\s*"fort"/g)) book.add(`${s[1]}:${s[2]}`);
    const g = open(true, 2);
    for (let k = 0; k < 12; k++) runTurnAI(g);
    const forts = g.corps.filter((c) => c.kind === 'fort');
    expect(forts.length).toBeGreaterThan(R1.length);
    for (const c of forts) expect(book.has(`${c.nat}:${c.name}`), c.name).toBe(true);
  }, 120_000);
});

describe('room: fortress troops take no place among the five', () => {
  it('every nation\'s, as engineers do not, with the switch on only', () => {
    for (const on of [false, true]) {
      const g = open(on);
      const alsace = g.byName.Alsace;
      clear(g, alsace, (u) => u.kind === 'fort');
      for (let k = 0; k < 4; k++) addCorps(g, 'FRA', `Test Corps ${k}`, 'inf', 2, 'Alsace', {});
      const fifth = addCorps(g, 'FRA', 'Fifth Test Corps', 'inf', 2, 'Paris', {});
      expect(placed(g, alsace, 'FRA').length).toBe(on ? 4 : 4 + fortsAt(g, alsace).length);
      expect(hasRoom(g, alsace, fifth)).toBe(on);
    }
  });
});

/**
 * Alsace held by its fortress rows (release 1's Lauter alone, unless `rows`) and one French corps, with German corps
 * ready in Baden (head-on) and in Lorraine (from behind: French ground the works do not face).
 */
function alsace(on: boolean, rows = false): { g: Game; target: Region; front: Corps[]; behind: Corps[] } {
  const g = open(on);
  setWar(g, 'GER', 'FRA');
  const target = g.byName.Alsace;
  clear(g, target, (u) => u.kind === 'fort' && (rows || u.name === 'Lauter fortified region'));
  addCorps(g, 'FRA', 'Interval Corps', 'inf', 2, 'Alsace', {});
  clear(g, g.byName.Lorraine);
  g.byName.Lorraine.ctrl = 'GER';
  const front = [addCorps(g, 'GER', 'Front Corps', 'inf', 3, 'Baden', {})];
  const behind = [addCorps(g, 'GER', 'Rear Corps', 'inf', 3, 'Lorraine', {})];
  return { g, target, front, behind };
}

describe('(a) the fortress share keeps its works against every attacker', () => {
  it('from behind the attack is cut by 1 + (W - 1) x s; head-on, and off, it is as release 1 has it', () => {
    const off = alsace(false), on = alsace(true);
    const s = fortShare(on.g, on.target.units);
    expect(s).toBeGreaterThan(0.3);
    expect(s).toBeLessThan(1);
    expect(fortShare(off.g, off.target.units)).toBe(0);
    // Head-on: the same either way.
    const hOff = assess(off.g, off.front, off.target), hOn = assess(on.g, on.front, on.target);
    expect(hOn.A).toBeCloseTo(hOff.A, 6);
    expect(hOn.D).toBeCloseTo(hOff.D, 6);
    // From behind: off, the works do nothing; on, the fortress share's works stand.
    const bOff = assess(off.g, off.behind, off.target), bOn = assess(on.g, on.behind, on.target);
    const W = FORT_MUL[on.target.fort!.lv];
    expect(W).toBeGreaterThan(1);
    expect(bOn.A).toBeCloseTo(bOff.A / (1 + (W - 1) * s), 6);
    expect(bOn.D).toBeCloseTo(bOff.D, 6);
    expect(bOn.lines.find((l) => l.label === 'Fortifications')!.effect).toBe("Taken from behind: the fortress troops' share keeps its works all round");
    expect(bOff.lines.find((l) => l.label === 'Fortifications')!.effect).toBe('Outflanked: the works face the other way');
  });
});

describe('(b) beaten or cut off, a fortress row stays in its works on its own stores', () => {
  it('cut off, it is not halved; a field corps is', () => {
    const { g, target } = alsace(true);
    const lauter = row(g, 'FRA', 'Lauter fortified region')!, interval = target.units.find((u) => u.name === 'Interval Corps')!;
    const [f1, i1] = [corpsBase(g, lauter), corpsBase(g, interval)];
    lauter.supplied = false; interval.supplied = false;
    expect(corpsBase(g, lauter)).toBeCloseTo(f1, 9);
    expect(corpsBase(g, interval)).toBeLessThan(i1 * 0.75);
    const off = alsace(false), l0 = row(off.g, 'FRA', 'Lauter fortified region')!;
    const b0 = corpsBase(off.g, l0);
    l0.supplied = false;
    expect(corpsBase(off.g, l0)).toBeLessThan(b0 * 0.75);
  });

  it('beaten in the field, it goes into its works as the garrison, taking no room; the town holds and is besieged', () => {
    for (const on of [false, true]) {
      const { g, target, behind } = alsace(on, true);
      for (let k = 0; k < 4; k++) behind.push(addCorps(g, 'GER', `Rear Corps ${k}`, 'arm', 12, 'Lorraine', {}));
      const rep = resolveBattle(g, behind, target);
      expect(rep.taken).toBe(true);
      const forts = rep.defenders.filter((u) => u.kind === 'fort');
      expect(forts.length).toBe(on ? 3 : 1);
      if (on) {
        for (const u of forts) expect(u.fate, u.name).toBe('sheltered');
        expect(garrisonOf(target).filter((c) => c.kind === 'fort')).toHaveLength(3);
        expect(housed(g, target)).toBe(0);
        expect(target.ctrl).toBe('FRA');
        expect(besieged(g, target)).toBe(true);
        // It eats none of the town's stores: unfed from the month it is shut in.
        siegeSupply(g);
        for (const c of garrisonOf(target)) expect(c.supplied).toBe(false);
      } else {
        expect(forts[0].fate).toBe('surrendered');
        expect(garrisonOf(target)).toHaveLength(0);
      }
    }
  });
});
