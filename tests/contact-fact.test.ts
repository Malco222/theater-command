// The contact rule's second form: the ONE contact test (drawn.ts `corpsInContact`, `contactRead`). The design
// director's ruling of 06:10Z, 26 November 2026:
//   "A corps is held while it took part in the battle last resolved on its ground (the board's record, no window).
//    Otherwise it is held only when it stands beside the enemy with nobody of its side to keep its province.
//    Relief is whatever the game's own holding test counts as holding that province for that side. N3: three regiments
//    or an ally's troops count exactly as that test counts them, one rule for all sides. N4: a battle given up without
//    fighting is no fight. This is the ONE contact test for every piece, rows and the draw alike, with no copy."
// And her answers of 12:48Z: (1) the fight half reads only while an enemy stands on or beside the corps; (3) the draw of
// a theatre skips corps the one test holds; (4) its ground is the battle's ground; (5) any corps under an order to give
// its province up is not relief; (6) as the game's own holding test counts (game.ts `hostileUnits`).
// The design: /home/user/runs/contact2/SPEC.md sections 4 and 6.1, with its section 13 (A1, A2, A8, A9, A11).
// With the switch 'fromWait' off the test is today's "beside", byte for byte (an oracle copied here, in the test only).
// Seed 1, the first day: Germany's III Corps alone in the Neumark, Poland's Army Poznan beside it in Poznan.
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { divisionsOf } from '../src/campaign/armies';
import { contactRead, corpsInContact, drawnWatch, formationName } from '../src/campaign/drawn';
import { addCorps, atFrontNow, atWar, createGame, declareWar, givesWay, relocate, rowHolds, startPhase, theatreName } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

// (Every .ts file of the rules and the bridge, as text, for the no-copy check: Vite's own glob, read at build time.)
declare global { interface ImportMeta { glob(pattern: string | string[], o: { query: string; import: string; eager: true }): Record<string, string> } }
const SOURCES = import.meta.glob(['../src/**/*.ts', '../unreal/TheaterCommand/Rules/**/*.ts'], { query: '?raw', import: 'default', eager: true });
const ROWS = (setup as any).expeditions as any[];
let added = 0;
const addRow = (row: any[]): number => { ROWS.push(row); added++; return ROWS.length - 1; };
/** The rows a board added, taken off again: each board's war reads only its own. */
const clear = (): void => { while (added > 0) { ROWS.pop(); added--; } };
afterEach(() => { clear(); drawnWatch.tell = null; });
type Told = Parameters<NonNullable<typeof drawnWatch.tell>>[0];

/** Today's body, as at 2a7564b7b: the oracle for the switch off. In this test only. */
const beside = (g: Game, nat: string, c: Corps): boolean => {
  const enemy = (r: Region): boolean => (!!r.ctrl && atWar(g, nat, r.ctrl)) || r.units.some((u) => !u.dead && atWar(g, nat, u.nat));
  const r = g.regions[c.reg];
  return enemy(r) || r.nb.some((j) => enemy(g.regions[j]));
};
const month = (g: Game, t: number): void => { g.turn = t; startPhase(g, 'GER'); };
const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);

/** III Corps alone in the Neumark, Poland beside it; the switch on. */
function board(on = true): { g: Game; c: Corps; nm: Region } {
  clear();
  const g = startOpen(createGame(1, null));
  if (on) expect(setFromWait(g, true)).toBe(true);
  const nm = g.byName.Neumark;
  const c = named(g, 'III Corps')!;
  expect(c.reg).toBe(nm.id);
  for (const u of [...nm.units]) if (u !== c) relocate(g, u, g.byName.Bavaria);
  expect(nm.units).toEqual([c]);
  expect(nm.nb.some((j) => g.regions[j].name === 'Poznan' && g.regions[j].ctrl === 'POL')).toBe(true);
  expect(atWar(g, 'GER', 'POL')).toBe(true);
  return { g, c, nm };
}
const add = (g: Game, nat: string, name: string, kind: Corps['kind'], at: Region, divs = 1): Corps => addCorps(g, nat, name, kind, divs, at.id, {});

describe('the one test, switch on: held beside the enemy unless somebody of its side keeps its province', () => {
  it('alone beside the enemy: held; the reading is beside, no battle, not kept', () => {
    const { g, c } = board();
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: false });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('relieved by a corps of its own side on its province: not held', () => {
    const { g, c, nm } = board();
    add(g, 'GER', 'Relief Test Corps', 'inf', nm, 2);
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: true });
    expect(corpsInContact(g, 'GER', c)).toBe(false);
  });

  it("N3: an ally's troops at war with the enemy relieve it; troops not at war with that enemy do not (relief is read per enemy)", () => {
    const { g, c, nm } = board();
    expect(atWar(g, 'SVK', 'POL')).toBe(true);
    expect(atWar(g, 'ITA', 'POL')).toBe(false);
    const ita = add(g, 'ITA', 'Test Italian Corps', 'inf', nm);
    expect(corpsInContact(g, 'GER', c)).toBe(true);
    relocate(g, ita, g.byName.Bavaria);
    add(g, 'SVK', 'Test Slovak Corps', 'inf', nm);
    expect(corpsInContact(g, 'GER', c)).toBe(false);
  });

  it('relief per enemy: with a second enemy beside, the ally at war with only one of them keeps nothing against the other', () => {
    const { g, c, nm } = board();
    add(g, 'SVK', 'Test Slovak Corps', 'inf', nm);
    expect(corpsInContact(g, 'GER', c)).toBe(false);
    // France's corps beside it too: Slovakia is not at war with France.
    expect(atWar(g, 'SVK', 'FRA')).toBe(false);
    expect(nm.nb.includes(g.byName.Stettin.id)).toBe(true);
    add(g, 'FRA', 'Test French Corps', 'inf', g.byName.Stettin);
    expect(atWar(g, 'GER', 'FRA')).toBe(true);
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: false });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('N3: a regiment of its side (a corps under a division in size) alone with it relieves it, each of three alike', () => {
    for (let k = 0; k < 3; k++) {
      const { g, c, nm } = board();
      const r = add(g, 'GER', `Test Regiment ${k + 1}`, 'inf', nm);
      r.full = 0.25; r.str = 0.25;
      expect(corpsInContact(g, 'GER', c)).toBe(false);
    }
  });

  it('a fort, engineers or guns alone with it keep the province as the game counts them (Q-B2): not held', () => {
    for (const kind of ['fort', 'eng', 'art'] as Corps['kind'][]) {
      const { g, c, nm } = board();
      add(g, 'GER', `Test ${kind}`, kind, nm);
      expect(corpsInContact(g, 'GER', c)).toBe(false);
    }
  });

  it('a town garrison of its side only does not keep the field (Q-B3): held', () => {
    const { g, c, nm } = board();
    const t = add(g, 'GER', 'Test Garrison Corps', 'inf', nm);
    // (Inside the town, as siege.ts `garrison` puts it: off the field's list, on the town's.)
    nm.units.splice(nm.units.indexOf(t), 1);
    (nm.garr ??= []).push(t);
    t.town = g.turn;
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('answer 5: a corps under the order to give its province up keeps nothing; one that already gave way this month keeps it', () => {
    const { g, c, nm } = board();
    const y = add(g, 'GER', 'Yielding Test Corps', 'inf', nm, 2);
    y.yields = true;
    expect(givesWay(g, y)).toBe(true);
    expect(corpsInContact(g, 'GER', c)).toBe(true);
    // (Given way once this month, it is not asked again, and fights where it stands: game.ts `giveWay`.)
    y.gaveWay = true;
    expect(givesWay(g, y)).toBe(false);
    expect(corpsInContact(g, 'GER', c)).toBe(false);
  });

  it('leaving: two corps on the province that the same act takes away keep nothing for each other', () => {
    const { g, c, nm } = board();
    const d = add(g, 'GER', 'Second Test Corps', 'inf', nm, 2);
    expect([corpsInContact(g, 'GER', c), corpsInContact(g, 'GER', d)]).toEqual([false, false]);
    const both = new Set([c, d]);
    expect([corpsInContact(g, 'GER', c, both), corpsInContact(g, 'GER', d, both)]).toEqual([true, true]);
    expect([corpsInContact(g, 'GER', c, new Set([d])), corpsInContact(g, 'GER', d, new Set([c]))]).toEqual([true, true]);
  });
});

describe('the fight half: the battle last resolved on its ground', () => {
  it('held while it took part in the battle last resolved where it stands, though relieved', () => {
    const { g, c, nm } = board();
    add(g, 'GER', 'Relief Test Corps', 'inf', nm, 2);
    g.lastBattle = { [nm.id]: [c.id] };
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: true, kept: true });
    expect(corpsInContact(g, 'GER', c)).toBe(true);
  });

  it('answer 4: its ground is the battle\'s ground: moved to another province beside the enemy, the battle no longer holds it', () => {
    const { g, c, nm } = board();
    g.lastBattle = { [nm.id]: [c.id] };
    const gz = g.byName.Grenzmark;
    relocate(g, c, gz);
    expect(gz.units.some((u) => u !== c && u.nat === 'GER')).toBe(true);
    expect(contactRead(g, 'GER', c)).toEqual({ beside: true, battle: false, kept: true });
    expect(corpsInContact(g, 'GER', c)).toBe(false);
  });

  it('answer 1: with no enemy on or beside it the battle holds nobody', () => {
    const { g, c } = board();
    const bav = g.byName.Bavaria;
    relocate(g, c, bav);
    g.lastBattle = { [bav.id]: [c.id] };
    expect(contactRead(g, 'GER', c)).toEqual({ beside: false, battle: true, kept: false });
    expect(corpsInContact(g, 'GER', c)).toBe(false);
  });
});

describe('the switch off: today\'s "beside", on every board, and no record', () => {
  it('every board above reads as the oracle with the switch off', () => {
    const boards: ((g: Game, c: Corps, nm: Region) => void)[] = [
      () => {},
      (g, _c, nm) => { add(g, 'GER', 'Relief Test Corps', 'inf', nm, 2); },
      (g, _c, nm) => { add(g, 'SVK', 'Test Slovak Corps', 'inf', nm); },
      (g, _c, nm) => { const y = add(g, 'GER', 'Yielding Test Corps', 'inf', nm, 2); y.yields = true; },
      (g, c, nm) => { g.lastBattle = { [nm.id]: [c.id] }; },
      (g, c) => { relocate(g, c, g.byName.Bavaria); },
      (g, c) => { relocate(g, c, g.byName.Grenzmark); },
    ];
    for (const set of boards) {
      const { g, c, nm } = board(true);
      set(g, c, nm);
      expect(setFromWait(g, false)).toBe(true);
      expect(g.lastBattle).toBeUndefined();
      for (const x of g.corps.filter((u) => !u.dead)) expect(corpsInContact(g, x.nat, x)).toBe(beside(g, x.nat, x));
    }
  });
});

describe('every caller: a row reads the one test with the corps that leave in the same act (section 13 A1)', () => {
  it('the old chart\'s row that takes a named corps whole: relieved, it is taken in its month; its own other corps are no relief', () => {
    // Relieved: the Test Corps beside the enemy in the Neumark with III Corps there, which the row does not take.
    {
      const { g, nm } = board();
      add(g, 'GER', 'Test Corps', 'inf', nm, 2);
      const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps'] }]);
      month(g, 2);
      expect(g.flags[`wait:${i}`]).toBeUndefined();
      expect(g.flags[`sent:${i}`]).toBe(1);
      expect(named(g, 'Test Corps')).toBeUndefined();
    }
    // The row takes both: neither keeps the province for the other, and the row waits with both where they stand.
    {
      const { g, nm } = board();
      add(g, 'GER', 'Test Corps', 'inf', nm, 2);
      const i = addRow([2, 'GER', 'Test Army', 'inf', 2, 'Magdeburg', { from: ['Test Corps', 'III Corps'] }]);
      month(g, 2);
      expect(g.flags[`wait:${i}`]).toBe(2);
      expect(g.flags[`sent:${i}`]).toBeUndefined();
      expect(g.regions[named(g, 'Test Corps')!.reg].name).toBe('Neumark');
      expect(g.regions[named(g, 'III Corps')!.reg].name).toBe('Neumark');
    }
  });

  /** A row of month 2 naming the 50th Infantry Division, which III Corps holds, to form at Magdeburg; what the draw told. */
  const drawFrom = (g: Game, more: Record<string, unknown> = {}): { i: number; told: Told[] } => {
    const told: Told[] = [];
    drawnWatch.tell = (e) => told.push(e);
    expect(divisionsOf(g, named(g, 'III Corps')!).map((d) => d.name)).toContain('50th Infantry Division');
    const i = addRow([2, 'GER', 'Test Army (50th Infantry Division)', 'inf', 1, 'Magdeburg', more]);
    return { i, told };
  };
  const claimOf = (told: Told[]): { from: string | null; stays: boolean } => { const t = told.find((x) => x.row === 'Test Army (50th Infantry Division)')!; return { from: t.claims[0].from, stays: t.claims[0].stays }; };

  it('the draw: III Corps relieved by a corps that stays keeps no division back; relieved only by the row\'s own `from` corps, it keeps the 50th', () => {
    {
      const { g, nm } = board();
      add(g, 'GER', 'Other Test Corps', 'inf', nm, 2);
      const { told } = drawFrom(g);
      month(g, 2);
      expect(claimOf(told)).toEqual({ from: 'III Corps', stays: false });
    }
    {
      const { g, nm } = board();
      add(g, 'GER', 'From Test Corps', 'inf', nm, 2);
      const { told, i } = drawFrom(g, { from: ['From Test Corps'] });
      month(g, 2);
      expect(claimOf(told)).toEqual({ from: 'III Corps', stays: true });
      expect(g.flags[`sent:${i}`]).toBe(1);
      expect(named(g, 'III Corps')!.divs).toBe(3);
    }
  });

  it('the draw: relieved only by a lent corps whose loan ends that month, III Corps keeps the 50th', () => {
    const { g, nm } = board();
    const lent = add(g, 'GER', 'Lent Test Corps', 'inf', nm, 2);
    const j = addRow([1, 'GER', 'Lent Test Corps', 'inf', 2, 'Neumark', { until: 2 }]);
    g.flags[`sent:${j}`] = 1;
    g.flags[`lent:${j}`] = lent.id;
    const { told } = drawFrom(g);
    month(g, 2);
    expect(lent.dead).toBe(true);
    expect(claimOf(told)).toEqual({ from: 'III Corps', stays: true });
  });

  it('A11: a corps the row would empty that stays for another reason (here at the battle schools) still keeps the province', () => {
    const run = (school: boolean): Told[] => {
      const { g, nm } = board();
      const ii = named(g, 'II Corps')!;
      relocate(g, ii, nm);
      if (school) ii.school = true;
      const all = [...divisionsOf(g, named(g, 'III Corps')!).filter((d) => d.name === '50th Infantry Division'), ...divisionsOf(g, ii)];
      const name = formationName(all.map((d) => [d.name, d.short] as [string, string]))!;
      const told: Told[] = [];
      drawnWatch.tell = (e) => told.push(e);
      addRow([2, 'GER', name, 'inf', all.length, 'Magdeburg', {}]);
      month(g, 2);
      return told.filter((t) => t.row === name);
    };
    // II Corps at school: it cannot leave, so it keeps the Neumark, and III Corps gives the 50th.
    const at = run(true)[0].claims;
    expect(at.filter((q) => q.from === 'II Corps').every((q) => q.stays)).toBe(true);
    expect(at.find((q) => q.from === 'III Corps')).toMatchObject({ stays: false });
    // II Corps free: the row would empty it, so it keeps nothing for III Corps, which keeps the 50th; II Corps, kept by
    // III Corps, comes.
    const free = run(false)[0].claims;
    expect(free.find((q) => q.from === 'III Corps')).toMatchObject({ stays: true });
    expect(free.filter((q) => q.from === 'II Corps').every((q) => !q.stays)).toBe(true);
  });

  it("answer 3: the draw of a theatre skips a corps the one test holds, and takes a free one; with the switch off it takes the largest as before", () => {
    const run = (on: boolean): { held: Corps; free: Corps } => {
      clear();
      const g = startOpen(createGame(1, null));
      if (on) setFromWait(g, true);
      declareWar(g, 'GER', 'GRE');
      const balkans = g.regions.filter((r) => theatreName(r) === 'the Balkans');
      const greek = (r: Region): boolean => r.ctrl === 'GRE' || r.units.some((u) => u.nat === 'GRE');
      const line = balkans.find((r) => !greek(r) && !atWar(g, 'GER', r.ctrl) && r.nb.some((j) => greek(g.regions[j])))!;
      const quiet = balkans.find((r) => r !== line && !greek(r) && !atWar(g, 'GER', r.ctrl) && !r.nb.some((j) => { const x = g.regions[j]; return (!!x.ctrl && atWar(g, 'GER', x.ctrl)) || x.units.some((u) => atWar(g, 'GER', u.nat)); }))!;
      expect(line && quiet).toBeTruthy();
      for (const u of [...line.units]) if (u.nat === 'GER') relocate(g, u, g.byName.Bavaria);
      const held = add(g, 'GER', 'Held Test Corps', 'inf', line, 3);
      const free = add(g, 'GER', 'Free Test Corps', 'inf', quiet, 2);
      addRow([2, 'GER', 'Test Theatre Army', 'inf', 1, 'Magdeburg', { from: 'the Balkans' }]);
      month(g, 2);
      return { held, free };
    };
    const on = run(true);
    expect([on.held.divs, on.free.divs]).toEqual([3, 1]);
    const off = run(false);
    expect([off.held.divs, off.free.divs]).toEqual([2, 2]);
  });

  it('the telling reads the same test: two corps of one row on one province are both at the front; a corps of another row relieves', () => {
    const { g, c, nm } = board();
    const d = add(g, 'GER', 'Test Corps', 'inf', nm, 2);
    expect(rowHolds(g, 'GER', ['Test Corps', 'III Corps'], g.byName.Magdeburg.id, -1).front).toEqual([c, d]);
    expect(rowHolds(g, 'GER', ['Test Corps'], g.byName.Magdeburg.id, -1).front).toEqual([]);
    expect(atFrontNow(g, 'GER', d, ['Test Corps', 'III Corps'])).toBe(true);
    expect(atFrontNow(g, 'GER', d, ['Test Corps'])).toBe(false);
  });
});

describe('no second copy of the test', () => {
  it('contactRead( and enemiesBeside( are written in src/campaign/drawn.ts alone; corpsInContact has one body', () => {
    const files = Object.keys(SOURCES);
    expect(files.length).toBeGreaterThan(50);
    expect(files).toContain('../src/campaign/drawn.ts');
    expect(files.some((p) => p.startsWith('../unreal/TheaterCommand/Rules/'))).toBe(true);
    const where = (needle: RegExp): string[] => files.filter((p) => needle.test(SOURCES[p])).map((p) => p.replace(/^\.\.\//, ''));
    expect(where(/\bcontactRead\(/)).toEqual(['src/campaign/drawn.ts']);
    expect(where(/\benemiesBeside\(/)).toEqual(['src/campaign/drawn.ts']);
    expect(where(/function corpsInContact\b/)).toEqual(['src/campaign/drawn.ts']);
  });
});
