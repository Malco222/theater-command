// The fortress rows (release 2; the design director's 05:38Z, 06:12Z, 06:15Z, 06:17Z and 06:21Z of 22 November 2026,
// on the historian's historian/fortress-garrisons-by-line.md): the book's fortress crews by date at every stop, taking
// no place among the five, their works holding all round for their share of the defence, and beaten or cut off they
// stay in their works on their own stores. Switched by stage.json 'fortRows' (TC_FORTS): off, the game is release 1's.
import { describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import farWorld from '../data/campaign/far-world.json';
import stage from '../data/campaign/stage.json';
import { orderCorps, specOf } from '../src/campaign/economy';
import { FORT_ROWS, FORT_XP, crewFed, crewMonth, crewStores, fortShape, fortsMonth, storesEaten, storesLeft } from '../src/campaign/fortrows';
import { DESERT, GUNS, addCorps, assess, computeSupply, corpsBase, createGame, fortRow, fortRows, fortShare, hasRoom, onPortStores, placed, portKeep, removeCorps, resolveBattle, roomless, setWar, startPhase } from '../src/campaign/game';
import { menLeft } from '../src/campaign/manpower';
import { loadGame, saveGame } from '../src/campaign/save';
import { SIEGE, besieged, breach, crewClock, garrisonOf, housed, invest, joinTown, siegeSupply, siegeView, sortiePower, storm, stormBar, stormForce, townBonus } from '../src/campaign/siege';
import { atkPower, sieges } from '../src/campaign/ai';
import { corpsMood } from '../src/campaign/morale';
import { drafted, training } from '../src/campaign/stories';
import { ENGINEERING } from '../src/campaign/rivers';
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
/** Off, the crews at the experience they have on (11:56Z), so that an on-off comparison reads the rule under test alone. */
const sameCrews = (g: Game): Game => { for (const c of g.corps) if (c.kind === 'fort') c.xp = Math.max(c.xp, FORT_XP); return g; };
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
  const g = sameCrews(open(on));
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
      // (07:03Z: each crew goes in with the organisation it had as the battle began; its men lost stay lost.)
      const crews = target.units.filter((u) => u.kind === 'fort');
      for (const c of crews) c.org = 0.9;
      const strAt = new Map(crews.map((c) => [c.id, c.str]));
      const rep = resolveBattle(g, behind, target);
      expect(rep.taken).toBe(true);
      const forts = rep.defenders.filter((u) => u.kind === 'fort');
      expect(forts.length).toBe(on ? 3 : 1);
      if (on) {
        for (const u of forts) expect(u.fate, u.name).toBe('sheltered');
        expect(garrisonOf(target).filter((c) => c.kind === 'fort')).toHaveLength(3);
        for (const c of crews) { expect(c.org, c.name).toBe(0.9); expect(c.str, c.name).toBeLessThan(strAt.get(c.id)!); }
        expect(housed(g, target)).toBe(0);
        expect(target.ctrl).toBe('FRA');
        expect(besieged(g, target)).toBe(true);
        // It eats none of the town's stores: unfed from the month it is shut in.
        siegeSupply(g);
        for (const c of garrisonOf(target)) expect(c.supplied).toBe(false);
      } else {
        expect(forts[0].fate).toBe('surrendered');
        expect(crews.every((c) => c.dead || c.org < 0.9)).toBe(true);
        expect(garrisonOf(target)).toHaveLength(0);
      }
    }
  });

  it('cut off, a crew lives on its own stores: a month of them eaten a month, its organisation and strength held; no clock, no starving', () => {
    const g = open();
    const months = (nat: string, name: string): number | null => crewStores(row(g, nat, name)!);
    expect(months('FRA', 'Metz fortified region')).toBe(3);
    expect(months('UK', 'Gibraltar garrison')).toBe(16);
    expect(months('POL', 'Hel Fortified Area')).toBe(1);
    expect(months('GRE', 'Metaxas Line forts')).toBeNull();
    // (A row the book gives no figure takes the default, 3, marked judgment.)
    expect(months('FRA', 'Upper Rhine fortified sectors')).toBe(3);
    const count = (c: Corps): number => { let k = 0; while (k < 40 && crewMonth(g, c) === 'held') k++; return k + 1; };
    const metz = row(g, 'FRA', 'Metz fortified region')!, str = metz.str;
    metz.org = 0.8;
    expect(count(metz)).toBe(3);
    expect(metz.str).toBe(str);
    expect(metz.org).toBe(0.8);
    expect(count(row(g, 'UK', 'Gibraltar garrison')!)).toBe(16);
    expect(count(row(g, 'POL', 'Hel Fortified Area')!)).toBe(1);
    const metaxas = row(g, 'GRE', 'Metaxas Line forts')!;
    for (let k = 0; k < 24; k++) expect(crewMonth(g, metaxas)).toBe('held');
    expect(metaxas.org).toBe(1);
    expect(storesLeft(g, metaxas)).toBeNull();
  });
  it('one stores, not two: a fortified port overseas that holds a row feeds from that row\'s stores, and the crew\'s clock runs from the month the sea closed (07:12Z)', () => {
    // (Gibraltar: the historian's 16 months are the place's whole stores. Off, the port keeps its own two.)
    for (const on of [false, true]) {
      const g = open(on);
      const gib = g.byName.Gibraltar, crew = row(g, 'UK', 'Gibraltar garrison')!;
      expect(gib.overseas && !!gib.port && !!gib.fort).toBe(true);
      expect(portKeep(g, gib)).toBe(on ? 16 : DESERT.stores);
      computeSupply(g);
      const opened = g.flags[`stores:${gib.id}`];
      expect(opened).toBe(g.turn);
      // The sea behind it shuts: no one's ships run any longer.
      for (const n of Object.values(g.nations)) { n.seaSupply = false; n.sealift = 0; }
      const keep = on ? 16 : DESERT.stores;
      let gone = 0;
      for (let k = 1; k <= keep + 1 && !gone; k++) {
        g.turn = opened + k;
        startPhase(g, 'UK');
        if (crew.dead) { gone = k; break; }
        // Off: fed its two months, then cut off like any corps, as release 1 has it.
        if (!on && k > DESERT.stores) { expect(crew.supplied).toBe(false); gone = -1; break; }
        expect(crew.supplied, `month ${k}`).toBe(true);
        expect(onPortStores(g, crew)).toBe(on);
        if (on) { expect(crew.org).toBe(1); expect(crew.str).toBe(1); expect(storesLeft(g, crew)).toBe(16 - k); }
      }
      if (on) {
        // Gone in the month its stores run out, and the port with no row left keeps its own two, long spent.
        expect(gone).toBe(16);
        expect(portKeep(g, gib)).toBe(DESERT.stores);
        computeSupply(g);
        expect(g.flags[`stores:${gib.id}`]).toBe(opened);
      } else expect(gone).toBe(-1);
    }
  });
});

describe('(b) the stores counted apart from organisation (the design director, 09:06Z)', () => {
  /** Alsace's three rows beaten into their works, the town besieged, the crews shut in on their own stores. */
  function shutIn(): { g: Game; target: Region; crews: Corps[] } {
    const { g, target, behind } = alsace(true, true);
    for (let k = 0; k < 4; k++) behind.push(addCorps(g, 'GER', `Rear Corps ${k}`, 'arm', 12, 'Lorraine', {}));
    expect(resolveBattle(g, behind, target).taken).toBe(true);
    siegeSupply(g);
    const crews = garrisonOf(target).filter((c) => c.kind === 'fort');
    expect(crews.length).toBe(3);
    for (const c of crews) expect(c.supplied).toBe(false);
    return { g, target, crews };
  }

  it('a fed crew fights at the strength it went in with while its stores last: the month\'s count moves nothing the battle reads', () => {
    const { g, crews } = shutIn();
    const c = crews.find((x) => (crewStores(x) ?? 0) >= 3)!;
    const org = c.org, base = corpsBase(g, c);
    for (let k = 1; k < crewStores(c)!; k++) {
      expect(crewMonth(g, c)).toBe('held');
      expect(storesEaten(g, c)).toBe(k);
      expect(c.org).toBe(org);
      expect(corpsBase(g, c)).toBe(base);
    }
  });

  it('the stores\' end is the surrender: it goes in the month they run out, by the starved branch, with no hungry months after', () => {
    const { g, crews } = shutIn();
    const c = crews.find((x) => crewStores(x) !== null)!, months = crewStores(c)!;
    for (let k = 1; k < months; k++) expect(crewMonth(g, c)).toBe('held');
    expect(storesLeft(g, c)).toBe(1);
    expect(crewMonth(g, c)).toBe('starved');
    expect(storesEaten(g, c)).toBe(0);
  });

  it('a month fed in supply restocks it: the count starts again', () => {
    const { g, crews } = shutIn();
    const c = crews.find((x) => crewStores(x) !== null)!, months = crewStores(c)!;
    for (let k = 1; k < months; k++) crewMonth(g, c);
    expect(storesLeft(g, c)).toBe(1);
    crewFed(g, c);
    expect(storesLeft(g, c)).toBe(months);
    expect(g.flags[`eaten:${c.id}`]).toBeUndefined();
  });

  it('a crew most of the way through its stores still sallies to meet a relief, as its organisation holds', () => {
    const { g, target, crews } = shutIn();
    const was = sortiePower(g, target, 'FRA');
    expect(was.corps).toBeGreaterThanOrEqual(crews.length);
    for (const c of crews) { const m = crewStores(c); if (m !== null) for (let k = 1; k < m; k++) crewMonth(g, c); }
    const now = sortiePower(g, target, 'FRA');
    expect(now.corps).toBe(was.corps);
    expect(now.power).toBeCloseTo(was.power, 9);
  });

  it('the guns take its organisation, their own harm, and none of its stores', () => {
    const { g, crews } = shutIn();
    const c = crews.find((x) => crewStores(x) !== null)!;
    crewMonth(g, c);
    const left = storesLeft(g, c);
    c.org -= 0.3;
    expect(storesLeft(g, c)).toBe(left);
  });

  it('the mood panel shows the months of stores left, and the rest that holds', () => {
    const { g, crews } = shutIn();
    const c = crews.find((x) => (crewStores(x) ?? 0) >= 3)!, months = crewStores(c)!;
    crewMonth(g, c);
    const m = corpsMood(g, c);
    expect(m.stores).toEqual({ left: months - 1, months });
    expect(m.restLine).toMatch(/own stores/);
    expect(m.restLine).toMatch(new RegExp(`${months - 1} months`));
    // A corps that is no fortress crew shows none.
    const field = g.corps.find((x) => !x.dead && x.kind !== 'fort')!;
    expect(corpsMood(g, field).stores).toBeUndefined();
  });

  it('off, nothing is counted and nothing written: the mood panel is release 1\'s', () => {
    const g = open(false);
    for (let t = 0; t < 6; t++) runTurnAI(g);
    expect(Object.keys(g.flags).some((k) => k.startsWith('eaten:'))).toBe(false);
    for (const c of g.corps.filter((x) => !x.dead)) expect('stores' in corpsMood(g, c)).toBe(false);
  });
});

describe('(b) amended, the storm (07:36Z): a crew in its works fights from them at the field\'s weight', () => {
  /**
   * Metz in its works: the field of Lorraine lost, the crew shut in Nancy with a French corps beside it, the town's own
   * guard out of action (so the garrison is the whole defence), and a German storm force in the field round it.
   */
  function metz(on: boolean, guns = false): { g: Game; town: Region; crew: Corps; force: Corps[] } {
    const g = sameCrews(open(on));
    setWar(g, 'GER', 'FRA');
    const town = g.byName.Lorraine;
    const crew = row(g, 'FRA', 'Metz fortified region')!;
    clear(g, town, (c) => c === crew);
    town.units.splice(town.units.indexOf(crew), 1);
    joinTown(g, town, crew);
    g.flags[`guard:${town.id}`] = 100;
    const force = [addCorps(g, 'GER', 'Storm Corps', 'inf', 6, 'Lorraine', {}), addCorps(g, 'GER', 'Second Storm Corps', 'inf', 6, 'Lorraine', {})];
    if (guns) force.push(addCorps(g, 'GER', 'Siege Artillery', 'art', 2, 'Lorraine', {}), addCorps(g, 'GER', 'Assault Pioneers', 'eng', 1, 'Lorraine', {}));
    return { g, town, crew, force };
  }
  const lv = (r: Region): number => r.fort!.lv;

  it('head-on: the crew\'s share stands behind FORT_MUL in place of the town\'s walls; off, the walls as release 1 has them', () => {
    const on = metz(true), off = metz(false);
    expect(lv(on.town)).toBe(3);
    const W = FORT_MUL[3], walls = townBonus(off.g, off.town);
    expect(walls).toBeGreaterThan(1);
    expect(walls).toBeLessThan(W);
    const aOn = assess(on.g, on.force, on.town, { town: true }), aOff = assess(off.g, off.force, off.town, { town: true });
    expect(aOn.A).toBeCloseTo(aOff.A, 6);
    expect(aOn.D / aOff.D).toBeCloseTo(W / walls, 6);
    expect(aOn.odds).toBeLessThan(aOff.odds);
    const line = aOn.lines.find((l) => l.label === 'Fortress crew')!;
    expect(line.value).toBe('Metz fortified region: 100% of the garrison');
    expect(line.effect).toBe(`It fights from its own works of level 3, ${Math.round((W - 1) * 100)}% stronger, in place of the town's walls`);
    expect(aOff.lines.some((l) => l.label === 'Fortress crew')).toBe(false);
  });

  it('with guns and engineers the works are eased as in the field; the rest of the garrison keeps the walls; no share has both', () => {
    const on = metz(true, true), off = metz(false, true);
    const guns = on.force.filter((c) => c.kind === 'art').reduce((t, c) => t + c.divs * c.str, 0);
    const sap = on.force.filter((c) => c.kind === 'eng').reduce((t, c) => t + c.divs * c.str, 0);
    const soften = Math.min(GUNS.softenMost, GUNS.soften * guns), opened = Math.min(ENGINEERING.breach.most, ENGINEERING.breach.perDiv * sap);
    const eased = 1 - (1 - soften) * (1 - opened);
    expect(eased).toBeGreaterThan(0);
    const W = 1 + (FORT_MUL[3] - 1) * (1 - eased), walls = townBonus(off.g, off.town, eased);
    const aOn = assess(on.g, on.force, on.town, { town: true }), aOff = assess(off.g, off.force, off.town, { town: true });
    expect(aOn.D / aOff.D).toBeCloseTo(W / walls, 6);
    expect(aOn.lines.find((l) => l.label === 'Fortress crew')!.effect).toContain(`${Math.round((W - 1) * 100)}% stronger`);
    // A French corps in the town beside the crew: it keeps the walls, the crew its works.
    const mixed = metz(true, true), plain = metz(false, true);
    for (const x of [mixed, plain]) { const f = addCorps(x.g, 'FRA', 'Nancy Corps', 'inf', 2, 'Vosges', {}); x.g.byName.Vosges.units.splice(x.g.byName.Vosges.units.indexOf(f), 1); joinTown(x.g, x.town, f); }
    const crewBase = corpsBase(mixed.g, mixed.crew) * KIND.fort.def;
    const nancy = garrisonOf(mixed.town).find((c) => c.name === 'Nancy Corps')!;
    const restBase = corpsBase(mixed.g, nancy) * KIND[nancy.kind].def;
    const mOn = assess(mixed.g, mixed.force, mixed.town, { town: true }), mOff = assess(plain.g, plain.force, plain.town, { town: true });
    expect(mOn.D / mOff.D).toBeCloseTo((crewBase * W + restBase * walls) / ((crewBase + restBase) * walls), 4);
    const line = mOn.lines.find((l) => l.label === 'Fortress crew')!;
    expect(line.value).toBe(`Metz fortified region: ${Math.round((crewBase / (crewBase + restBase)) * 100)}% of the garrison`);
    expect(line.effect).toContain("of the garrison behind the town's walls");
  });

  it('Montmedy still falls to a storm as La Ferte did', () => {
    const g = open(true);
    setWar(g, 'GER', 'FRA');
    const town = g.byName.Verdun;
    const crew = row(g, 'FRA', 'Montmedy sector')!;
    clear(g, town, (c) => c === crew);
    town.units.splice(town.units.indexOf(crew), 1);
    joinTown(g, town, crew);
    const force = [addCorps(g, 'GER', 'Storm Corps', 'inf', 6, 'Verdun', {}), addCorps(g, 'GER', 'Second Storm Corps', 'inf', 6, 'Verdun', {}), addCorps(g, 'GER', 'Siege Artillery', 'art', 2, 'Verdun', {})];
    const a = assess(g, force, town, { town: true });
    expect(a.lines.some((l) => l.label === 'Fortress crew')).toBe(true);
    expect(a.odds).toBeGreaterThan(SIEGE.ai.assault * 2);
    const rep = storm(g, force, town)!;
    expect(rep.taken).toBe(true);
    expect(crew.dead).toBe(true);
  });

  it('a long bombardment breaks the works for the crew as for the walls: a crew behind works broken to the most is takeable', () => {
    const on = metz(true);
    breach(on.g, on.town, 100);
    const b = (on.g.flags[`breach:${on.town.id}`] ?? 0) / 100;
    expect(b).toBe(SIEGE.breach.most / 100);
    const a = assess(on.g, on.force, on.town, { town: true });
    const W = 1 + (FORT_MUL[3] - 1) * (1 - b);
    expect(a.lines.find((l) => l.label === 'Fortress crew')!.effect).toContain(`${Math.round((W - 1) * 100)}% stronger`);
    const fresh = metz(true);
    expect(a.odds).toBeGreaterThan(assess(fresh.g, fresh.force, fresh.town, { town: true }).odds * 1.5);
  });
});


describe("the crews are trained, not green (the design director's 11:56Z)", () => {
  const trained = (xp: number): boolean => xp >= 0.2 && xp < 0.45;

  it("every nation's crews read trained with the rows on, release 1's four and the book's alike; off, release 1's stay as they were", () => {
    expect(trained(FORT_XP)).toBe(true);
    const g = open(true);
    const crews = g.corps.filter((c) => !c.dead && c.kind === 'fort');
    expect(new Set(crews.map((c) => c.nat)).size).toBeGreaterThan(3);
    for (const name of R1) expect(crews.some((c) => c.name === name)).toBe(true);
    for (const c of crews) expect(trained(c.xp)).toBe(true);
    if (FORTS_STAGE) return; // (a run with TC_FORTS set opens every campaign with the rows on)
    const off = open(false);
    for (const name of R1) expect(row(off, name === 'Gibraltar garrison' ? 'UK' : 'FRA', name)!.xp).toBe(0.15);
  });

  it('a row formed later in the war is trained too, and a corps of any other kind is not touched', () => {
    const g = open(true);
    const late = addCorps(g, 'SOV', 'Test fortified region', 'fort', 1, 'Minsk', { hold: 1 });
    expect(late.xp).toBe(FORT_XP);
    const field = addCorps(g, 'GER', 'Storm Corps', 'inf', 3, 'Lorraine', {});
    expect(field.xp).toBe(0.15);
    const keeps = addCorps(g, 'FRA', 'Veteran works', 'fort', 1, 'Lorraine', { xp: 0.6 });
    expect(keeps.xp).toBe(0.6);
  });

  it("the player's crew takes its drafts at the crew's figure; his field corps still take the depots' green", () => {
    const g = open(true);
    g.human = 'FRA';
    const crew = row(g, 'FRA', 'Metz fortified region')!;
    const field = row(g, 'FRA', '3rd Army')!;
    field.xp = crew.xp = FORT_XP;
    drafted(g, crew, 0.5, 0.5);
    drafted(g, field, 0.5, 0.5);
    expect(crew.xp).toBeCloseTo(FORT_XP, 9);
    expect(field.xp).toBeLessThan(FORT_XP);
    expect(field.xp).toBeGreaterThan(training(g, 'FRA'));
  });
});

describe("the besieger reads the crews' clock (the design director's 13:05Z)", () => {
  /** Metz's crew shut in Nancy, cut off, `sat` months into the siege, with these German corps round it and every other German corps held where it stands. */
  function shutIn(on: boolean, sat: number, round: [Kind, number][], next: [Kind, number][] = []): { g: Game; town: Region; crew: Corps; near: Corps[] } {
    const g = sameCrews(open(on));
    setWar(g, 'GER', 'FRA');
    const town = g.byName.Lorraine;
    const crew = row(g, 'FRA', 'Metz fortified region')!;
    clear(g, town, (c) => c === crew);
    town.units.splice(town.units.indexOf(crew), 1);
    joinTown(g, town, crew);
    for (const c of g.corps) if (c.nat === 'GER' && !c.dead) c.hold = true;
    round.forEach(([k, d], i) => addCorps(g, 'GER', `Round ${i}`, k, d, 'Lorraine', {}));
    const near = next.map(([k, d], i) => addCorps(g, 'GER', `Next door ${i}`, k, d, 'Saar', {}));
    invest(g, town, 'GER');
    g.flags[`siege:${town.id}`] = g.turn - sat;
    crew.supplied = false;
    return { g, town, crew, near };
  }
  const odds = (g: Game, town: Region): number => assess(g, stormForce(g, town.units.filter((c) => c.nat === 'GER'), town), town, { town: true }).odds;
  const stormed = (g: Game, town: Region): number => g.reports.filter((r) => r.region === town.id && r.turn === g.turn).length;

  it('its bar does not fall while hunger would take the crews before the siege comes to its gamble; fed, or off, it falls as before', () => {
    const { g, town, crew } = shutIn(true, 1, [['inf', 6]]);
    expect(crewClock(g, town)).toBe(3);
    for (const sat of [0, 1, 2, 3]) expect(stormBar(g, town, sat)).toBe(SIEGE.ai.hasty);
    // (Three months of stores four months in would end past the gamble's month: the bar falls as it did.)
    expect(stormBar(g, town, 4)).toBe(SIEGE.ai.late);
    g.flags[`eaten:${crew.id}`] = 2;
    expect(crewClock(g, town)).toBe(1);
    expect(stormBar(g, town, 5)).toBe(SIEGE.ai.hasty);
    expect(stormBar(g, town, 6)).toBe(SIEGE.ai.last.odds);
    // Fed, the crew has no clock running: the bar falls with the months.
    crew.supplied = true;
    expect(crewClock(g, town)).toBeNull();
    expect(stormBar(g, town, 1)).toBe(SIEGE.ai.assault);
    const off = shutIn(false, 1, [['inf', 6]]);
    expect(crewClock(off.g, off.town)).toBeNull();
    expect(stormBar(off.g, off.town, 1)).toBe(SIEGE.ai.assault);
  });

  it('a staff round a crew-held town waits for the hunger at odds it would once have stormed at; off, it storms', () => {
    const round: [Kind, number][] = [['inf', 6], ['inf', 6], ['inf', 5]];
    const on = shutIn(true, 1, round), off = shutIn(false, 1, round);
    const o = odds(on.g, on.town);
    expect(o).toBeGreaterThanOrEqual(SIEGE.ai.assault);
    expect(o).toBeLessThan(SIEGE.ai.hasty);
    expect(odds(off.g, off.town)).toBeGreaterThanOrEqual(SIEGE.ai.assault);
    sieges(on.g, 'GER');
    sieges(off.g, 'GER');
    expect(stormed(on.g, on.town)).toBe(0);
    expect(besieged(on.g, on.town)).toBe(true);
    expect(stormed(off.g, off.town)).toBe(1);
  });

  it("the staff calls up guns and engineers by the storm's own odds, not by their worth in the line; off, by worth", () => {
    // Four corps round the town leave one place among the five; next door stand guns, a corps of the line, and engineers (who take no place).
    const round: [Kind, number][] = [['inf', 2], ['inf', 2], ['inf', 2], ['inf', 2]];
    const next: [Kind, number][] = [['art', 2], ['inf', 4], ['eng', 1]];
    const on = shutIn(true, 1, round, next), off = shutIn(false, 1, round, next);
    const [gunsOn, lineOn, engOn] = on.near;
    // The guns add more to the storm than the line corps would, though they are worth less in the line (ai.ts atkPower, the off ranking).
    const base = on.town.units.filter((c) => c.nat === 'GER');
    const w = (c: Corps): number => assess(on.g, stormForce(on.g, [...base, { ...c, reg: on.town.id }], on.town), on.town, { town: true }).odds;
    expect(w(gunsOn)).toBeGreaterThan(w(lineOn));
    expect(atkPower(on.g, gunsOn)).toBeLessThan(atkPower(on.g, lineOn));
    sieges(on.g, 'GER');
    sieges(off.g, 'GER');
    expect(gunsOn.reg).toBe(on.town.id);
    expect(engOn.reg).toBe(on.town.id);
    expect(lineOn.reg).not.toBe(on.town.id);
    const [gunsOff, lineOff, engOff] = off.near;
    expect(lineOff.reg).toBe(off.town.id);
    expect(gunsOff.reg).not.toBe(off.town.id);
    expect(engOff.reg).not.toBe(off.town.id);
  });

  it("the siege card shows the crews' stores to both sides, as the staffs read them; off, nothing", () => {
    const { g, town } = shutIn(true, 1, [['inf', 6]]);
    const v = siegeView(g, town.id)!;
    expect(v.crew).toEqual({ left: 3, months: 3 });
    expect(v.lines).toContain("Its fortress crews live on their works' own stores: 3 months of them left. When they run out the crews give in, whatever the town's own stores.");
    const off = shutIn(false, 1, [['inf', 6]]);
    const vo = siegeView(off.g, off.town.id)!;
    expect(vo.crew).toBeNull();
    expect(vo.lines.some((l) => /fortress crews/.test(l))).toBe(false);
  });
});

