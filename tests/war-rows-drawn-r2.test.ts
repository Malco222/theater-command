// The stood war rows are drawn from the board's own corps (release 2, the grand campaign; src/campaign/drawn.ts,
// data/campaign/counting.json).
//
// The design director's rulings of 16:42Z to 17:19Z, 25 November 2026, on the historian's read
// (historian/war-rows-counting-formations-1125.csv, war-rows-parent-counts-1125.md) and the joiner's count in the
// game's own state (joins/s-rows-count-1125.md): a dated war row of Italy's or Romania's whose divisions stood on
// the first day was laid new and whole beside the board's own army that stands for the same men, 57 to 63 Italian
// field divisions against her 43. The table names the board corps each such row draws on. Read here on boards set by
// hand, a row's month at a time.
//
// (A month's start lays the dated rows before the drafts go out, and the drafts fill a corps under strength: so what
// a row did to the men is read at the moment it is laid, by the rule's own watch.)
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import counting from '../data/campaign/counting.json';
import { divisionsOf } from '../src/campaign/armies';
import { DRAWN, countingFor, drawnWatch, recordCount } from '../src/campaign/drawn';
import { signPeace } from '../src/campaign/diplomacy';
import { createGame, relocate, removeCorps, setWar, startPhase } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const ROWS = (setup as any).expeditions as any[];
const TABLE = counting as unknown as {
  parents: Record<string, Record<string, { of: number; mark: string }>>;
  rows: Record<string, Record<string, { war?: [string, string]; from?: { corps: string; divs: number; record?: number; mark: string }[]; takes?: { name: string; share: number; leaves?: string; leavesHere?: string } }>>;
};
const rowAt = (nat: string, name: string): number => ROWS.findIndex((r) => r[1] === nat && r[2] === name);
const monthOf = (nat: string, name: string): number => ROWS[rowAt(nat, name)][0];

type Told = Parameters<NonNullable<typeof drawnWatch.tell>>[0];
type Laid = Parameters<NonNullable<typeof drawnWatch.laid>>[0];
interface Shot { divs: number; str: number; out: number | undefined; name: string; reg: string }

/**
 * A grand campaign with the wars a test names begun (set without their declarations: no surprise, no news), and the
 * rule's watch: `told` and `laid` as the rule gives them, `before` every corps of the two countries the moment a row
 * was read and `after` the moment it had been laid, by the row's name and the corps' id.
 */
function war(wars: [string, string][], human: string | null = null, open = true): { g: Game; told: Told[]; laid: Laid[]; before: Map<string, Map<number, Shot>>; after: Map<string, Map<number, Shot>> } {
  const g = open ? startOpen(createGame(1, human)) : createGame(1, human);
  for (const [a, b] of wars) setWar(g, a, b);
  const told: Told[] = [], laid: Laid[] = [];
  const before = new Map<string, Map<number, Shot>>(), after = new Map<string, Map<number, Shot>>();
  const shot = (nat: string): Map<number, Shot> => new Map(g.corps.filter((c) => c.nat === nat && !c.dead).map((c) => [c.id, { divs: c.divs, str: c.str, out: c.out, name: c.name, reg: g.regions[c.reg].name }]));
  drawnWatch.tell = (e) => { told.push(e); before.set(e.row, shot(e.nat)); };
  drawnWatch.laid = (e) => { laid.push(e); after.set(e.row, shot(e.nat)); };
  return { g, told, laid, before, after };
}
let added = 0;
afterEach(() => { drawnWatch.tell = null; drawnWatch.laid = null; while (added > 0) { ROWS.pop(); added--; } });

const month = (g: Game, nat: string, turn: number): void => { g.turn = turn; startPhase(g, nat); };
const corps = (g: Game, nat: string, name: string): Corps | undefined => g.corps.find((c) => c.nat === nat && !c.dead && c.name === name);
const names = (g: Game, c: Corps): string[] => divisionsOf(g, c).map((d) => d.name);
const place = (g: Game, c: Corps): string => g.regions[c.reg].name;
const fieldDivs = (g: Game, nat: string): number => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort').reduce((s, c) => s + c.divs, 0);
const sent = (g: Game, nat: string, name: string): number | undefined => g.flags[`sent:${rowAt(nat, name)}`];
/** A corps cut down to its first cards, as a battle leaves it. */
const cut = (c: Corps, divs: number): void => { c.divs = divs; if (c.names) c.names = c.names.slice(0, divs); if (c.nos) c.nos = c.nos.slice(0, divs); };
/** A line as the words file writes it, its places filled by anything. */
const said = (words: string): RegExp => new RegExp(`^${words.replace(/[.*+?^${}()|[\]\\]/g, '\\$&').replace(/\\\{\w+\\\}/g, '.+')}$`);

const ITALY_UK: [string, string][] = [['ITA', 'UK']];
const ITALY_GREECE: [string, string][] = [['ITA', 'UK'], ['ITA', 'GRE']];
const CIAMURIA = 'Ciamuria Corps and Julia Alpine Division', XXVI = 'XXVI Corps', NINTH = '9th Army', ELEVENTH = '11th Army reserves';
const ARIETE = 'Ariete Armoured Division', TRENTO = 'Trento Motorized Division', BRESCIA = 'Brescia and Pavia Divisions', BOLOGNA = 'Bologna and Savona Divisions';
const PISTOIA = 'Pistoia and La Spezia Divisions', SIXTH = '6th Army';

describe('the table (counting.json)', () => {
  const board = (nat: string): Map<string, number> => new Map<string, number>([
    ...((setup as any).corps as any[]).filter((r) => r[0] === nat).map((r) => [r[1], r[3]] as [string, number]),
    ...[...((setup as any).reinforcements as any[]), ...(((setup as any).formations as any[]) ?? [])].filter((r) => r[1] === nat).map((r) => [r[2], r[4]] as [string, number]),
  ]);

  it('names only rows of the data, and only corps the board lays for that country', () => {
    for (const [nat, rows] of Object.entries(TABLE.rows)) {
      const corpsOf = board(nat);
      for (const [name, t] of Object.entries(rows)) {
        const i = rowAt(nat, name);
        expect(i, `${nat} ${name}`).toBeGreaterThanOrEqual(0);
        const row = ROWS[i];
        // (Only a war row is drawn: it is lent for its war. A fort row is not read by the rule at all.)
        expect(row[6]?.war ?? t.war, name).toBeDefined();
        expect(row[3], name).not.toBe('fort');
        expect(row[6]?.until, `${name}: a row with a month to end in is a loan, and not this table's`).toBeUndefined();
        for (const f of t.from ?? []) expect(corpsOf.has(f.corps), `${nat} ${name}: ${f.corps}`).toBe(true);
        // It never draws more divisions than it has.
        expect((t.from ?? []).reduce((s, f) => s + f.divs, 0), name).toBeLessThanOrEqual(row[4]);
        if (t.takes) { expect(corpsOf.has(t.takes.name), name).toBe(true); expect(t.takes.share).toBeGreaterThan(0); expect(t.takes.share).toBeLessThan(1); }
        // (A share is drawn only from a corps whose record count is given.)
        for (const f of t.from ?? []) if (f.record !== undefined) expect(recordCount(nat, f.corps), `${name}: ${f.corps}`).toBeGreaterThan(0);
      }
      for (const name of Object.keys(TABLE.parents[nat] ?? {})) expect(corpsOf.has(name), `${nat} ${name}`).toBe(true);
    }
  });

  it('never draws more of the record\'s divisions out of a corps than the record counts in it, nor more whole divisions than the board gives it', () => {
    for (const [nat, rows] of Object.entries(TABLE.rows)) {
      const corpsOf = board(nat);
      const record = new Map<string, number>(), whole = new Map<string, number>();
      for (const t of Object.values(rows)) for (const f of t.from ?? []) {
        if (f.record !== undefined) record.set(f.corps, (record.get(f.corps) ?? 0) + f.record);
        else whole.set(f.corps, (whole.get(f.corps) ?? 0) + f.divs);
      }
      // (A corps drawn dry by share would stand at nothing: the table leaves every one something.)
      for (const [name, n] of record) expect(n, `${nat} ${name}`).toBeLessThan(recordCount(nat, name)!);
      for (const [name, n] of whole) expect(n, `${nat} ${name}`).toBeLessThanOrEqual(corpsOf.get(name)!);
      // No corps is drawn both ways.
      for (const name of record.keys()) expect(whole.has(name), `${nat} ${name}`).toBe(false);
    }
  });

  it('every stand-in placement is flagged: a figure the historian has from memory is never laid as read', () => {
    for (const rows of Object.values(TABLE.rows)) for (const t of Object.values(rows)) for (const f of t.from ?? []) expect(['L', 'D', 'G']).toContain(f.mark);
  });

  it('is read in the grand campaign alone, and for a country\'s own rows', () => {
    const open = startOpen(createGame(1, null)), book = createGame(1, null);
    const row = ROWS[rowAt('ITA', CIAMURIA)];
    expect(countingFor(open, 'ITA', row)?.war).toEqual(['ITA', 'GRE']);
    expect(countingFor(book, 'ITA', row)).toBeUndefined();
    expect(countingFor(open, 'GER', row)).toBeUndefined();
  });
});

describe('a historical scenario keeps the order of battle it was measured with', () => {
  it('the Ciamuria Corps comes whole in its month, with no Greek war, beside an Albania Command of four', () => {
    const { g, told, laid } = war(ITALY_UK, null, false);
    month(g, 'ITA', monthOf('ITA', CIAMURIA));
    const c = corps(g, 'ITA', CIAMURIA)!;
    expect(c.divs).toBe(3);
    expect(c.str).toBe(1);
    expect(corps(g, 'ITA', 'Albania Command')!.divs).toBe(4);
    expect(corps(g, 'ITA', '8th Army')!.divs).toBe(3);
    expect(corps(g, 'ITA', XXVI)!.divs).toBe(2);
    expect(told).toEqual([]);
    expect(laid).toEqual([]);
  });
});

describe('Albania: the rows of the war on Greece', () => {
  it('no card of the Albania Command stands for two rows: the XXVI Corps counts the one before the Ciamuria Corps\'s two', () => {
    const { g, told } = war(ITALY_GREECE);
    const albania = corps(g, 'ITA', 'Albania Command')!;
    const cards = names(g, albania);
    month(g, 'ITA', monthOf('ITA', CIAMURIA));
    const of = (row: string): string[] => told.filter((e) => e.row === row).at(-1)!.claims.filter((q) => q.from === 'Albania Command').map((q) => q.name);
    expect(of(CIAMURIA)).toEqual(cards.slice(2));
    expect(of(XXVI)).toEqual([cards[1]]);
    expect(albania.divs).toBe(4);
    expect(albania.kept).toBe(3);
  });

  it('where the Albania Command is short, what the two rows find it no longer holds is raised new, and only that', () => {
    const { g, laid } = war(ITALY_GREECE);
    const albania = corps(g, 'ITA', 'Albania Command')!;
    cut(albania, 2);
    month(g, 'ITA', monthOf('ITA', CIAMURIA));
    // (The Ciamuria Corps counts the two that are left; the XXVI Corps finds none, and raises its Albanian one new.)
    expect(laid.find((e) => e.row === CIAMURIA)).toMatchObject({ fresh: 0 });
    const l = laid.find((e) => e.row === XXVI)!;
    expect(l.fresh).toBe(1);
    expect(l.after - l.before).toBeCloseTo(l.str, 9);
    expect(albania.divs).toBe(2);
    expect(albania.kept).toBe(2);
  });

  it('a counting formation drawn whole that holds fewer cards than the table counts: it comes itself, and the rest is raised new', () => {
    const { g, laid } = war(ITALY_GREECE);
    const eighth = corps(g, 'ITA', '8th Army')!;
    cut(eighth, 1);
    const card = names(g, eighth)[0];
    g.flags[`sent:${rowAt('ITA', CIAMURIA)}`] = 2;
    g.flags[`sent:${rowAt('ITA', NINTH)}`] = 2;
    month(g, 'ITA', monthOf('ITA', ELEVENTH));
    const c = corps(g, 'ITA', ELEVENTH)!;
    const l = laid.find((e) => e.row === ELEVENTH)!;
    expect(l.fresh).toBe(1);
    // (Two by share of the 4th Army, the 8th Army's one card, one new: the country is the stronger by the new one alone.)
    expect(l.after - l.before).toBeCloseTo(l.str, 9);
    expect(c).toBe(eighth);
    expect(c.rowed).toMatchObject({ name: '8th Army' });
    expect(c.divs).toBe(4);
    expect(names(g, c)).toContain(card);
  });

  it('the Ciamuria Corps and the XXVI Corps wait for the war on Greece, not for Britain\'s', () => {
    const { g } = war(ITALY_UK);
    const before = fieldDivs(g, 'ITA');
    month(g, 'ITA', monthOf('ITA', CIAMURIA));
    expect(sent(g, 'ITA', CIAMURIA)).toBeUndefined();
    expect(sent(g, 'ITA', XXVI)).toBeUndefined();
    expect(corps(g, 'ITA', CIAMURIA)).toBeUndefined();
    expect(fieldDivs(g, 'ITA')).toBe(before);
  });

  it('the Ciamuria Corps brings one division from the 8th Army; the Albania Command, in the row\'s own theatre, keeps its four', () => {
    const { g, told, laid, after } = war(ITALY_GREECE);
    const before = fieldDivs(g, 'ITA');
    const eighth = corps(g, 'ITA', '8th Army')!, albania = corps(g, 'ITA', 'Albania Command')!, third = corps(g, 'ITA', '3rd Army')!;
    const last = names(g, eighth)[2];
    month(g, 'ITA', monthOf('ITA', CIAMURIA));
    const t = told.find((e) => e.row === CIAMURIA)!;
    expect(t.claims.map((q) => [q.from, q.stays])).toEqual([['Albania Command', true], ['Albania Command', true], ['8th Army', false]]);
    const c = corps(g, 'ITA', CIAMURIA)!;
    const l = laid.find((e) => e.row === CIAMURIA)!;
    expect(l).toMatchObject({ corps: c.id, took: false, fresh: 0 });
    // Nothing is made: the country's strength the moment the row was laid is what it was the moment before.
    expect(l.after - l.before).toBeCloseTo(0, 9);
    expect(after.get(CIAMURIA)!.get(c.id)).toMatchObject({ divs: 1, str: 1 });
    expect(names(g, c)).toEqual([last]);
    expect(place(g, c)).toBe('Albania');
    expect(albania.divs).toBe(4);
    expect(eighth.divs).toBe(2);
    expect(names(g, eighth)).not.toContain(last);
    // It is lent for its war: where the division came from is remembered.
    expect(c.drew).toEqual([{ name: [last, expect.any(String)], no: expect.any(Number), from: eighth.id }]);
    expect(g.flags[`lent:${rowAt('ITA', CIAMURIA)}`]).toBe(c.id);
    // The XXVI Corps, the same month: one from the 3rd Army, the Albania Command's stays.
    const x = corps(g, 'ITA', XXVI)!;
    expect(x.divs).toBe(1);
    expect(third.divs).toBe(1);
    expect(laid.find((e) => e.row === XXVI)!.fresh).toBe(0);
    // Five divisions were laid here before the rule. Now none is: two have moved.
    expect(fieldDivs(g, 'ITA')).toBe(before);
  });

  it('the 9th Army comes with eight thirteenths of the 1st Army, which keeps its four divisions, the weaker', () => {
    const { g, laid, before, after } = war(ITALY_GREECE);
    const first = corps(g, 'ITA', '1st Army')!;
    month(g, 'ITA', monthOf('ITA', NINTH));
    const c = corps(g, 'ITA', NINTH)!;
    const l = laid.find((e) => e.row === NINTH)!;
    expect(l).toMatchObject({ corps: c.id, took: false, fresh: 0 });
    expect(l.after - l.before).toBeCloseTo(0, 9);
    const was = before.get(NINTH)!.get(first.id)!, now = after.get(NINTH)!.get(first.id)!, mine = after.get(NINTH)!.get(c.id)!;
    expect(was).toMatchObject({ divs: 4, out: undefined });
    // 8 of the record's 13: 8/13 of four divisions is about 2.5 of the game's (the historian's own example).
    expect(mine.divs).toBe(4);
    expect(mine.divs * mine.str).toBeCloseTo((8 / 13) * 4 * was.str, 9);
    expect(now.divs).toBe(4);
    expect(now.str).toBeCloseTo(was.str * (5 / 13), 9);
    expect(now.out).toBe(8);
    // Each of its divisions stands for two of the record's, and reads by a number of its own.
    expect(c.drew!.map((r) => [r.from, r.rec])).toEqual([[first.id, 2], [first.id, 2], [first.id, 2], [first.id, 2]]);
    expect(new Set(names(g, c)).size).toBe(4);
    for (const nm of names(g, c)) expect(names(g, first)).not.toContain(nm);
  });

  it('the 11th Army reserves: two by share from the 4th Army, and the 8th Army\'s last two divisions', () => {
    const { g, laid, before, after } = war(ITALY_GREECE);
    const fourth = corps(g, 'ITA', '4th Army')!, eighth = corps(g, 'ITA', '8th Army')!;
    const lastTwo = names(g, eighth).slice(1);
    // (Read by itself: the Ciamuria Corps, which takes the 8th Army's last division first, is set aside.)
    g.flags[`sent:${rowAt('ITA', CIAMURIA)}`] = 2;
    month(g, 'ITA', monthOf('ITA', ELEVENTH));
    const c = corps(g, 'ITA', ELEVENTH)!;
    const l = laid.find((e) => e.row === ELEVENTH)!;
    expect(l.fresh).toBe(0);
    expect(l.after - l.before).toBeCloseTo(0, 9);
    const was = before.get(ELEVENTH)!.get(fourth.id)!, now = after.get(ELEVENTH)!.get(fourth.id)!;
    // 4 of the 4th Army's 9: four ninths of three divisions between the two.
    expect(now.str).toBeCloseTo(was.str * (5 / 9), 9);
    expect(now).toMatchObject({ divs: 3, out: 4 });
    const mine = after.get(ELEVENTH)!.get(c.id)!;
    expect(mine.divs).toBe(4);
    expect(mine.divs * mine.str).toBeCloseTo((4 / 9) * 3 * was.str + 2, 9);
    expect(names(g, c).slice(2)).toEqual(lastTwo);
    expect(eighth.divs).toBe(1);
    expect(names(g, eighth)).not.toContain(lastTwo[0]);
  });

  it('the month\'s four rows together lay no division that was not on the board', () => {
    const { g, laid } = war(ITALY_GREECE);
    const before = fieldDivs(g, 'ITA');
    for (let t = 13; t <= 17; t++) month(g, 'ITA', t);
    for (const name of [CIAMURIA, XXVI, NINTH, ELEVENTH]) {
      expect(sent(g, 'ITA', name), name).toBe(1);
      const l = laid.find((e) => e.row === name)!;
      expect(l.fresh, name).toBe(0);
      expect(l.after - l.before, name).toBeCloseTo(0, 9);
    }
    // Thirteen divisions were laid here before the rule. The cards are six more (the divisions drawn by share stand
    // beside the corps they came out of: four of the 9th Army, two of the 11th), and not one division's worth of
    // men is new. (The seventh card is Ariete's, in Libya, in the last of these months: by share as well.)
    expect(sent(g, 'ITA', ARIETE)).toBe(1);
    expect(fieldDivs(g, 'ITA') - before).toBe(7);
    // (The 8th Army, its last two divisions the row's, came itself under the row's name.)
    const c = corps(g, 'ITA', ELEVENTH)!;
    expect(c.rowed).toMatchObject({ name: '8th Army' });
  });
});

describe('the Army of the Po: ten of the record\'s divisions in the game\'s three', () => {
  it('Ariete takes a tenth of it, and Trento a ninth of what the record still counts there', () => {
    const { g, laid, before, after } = war(ITALY_UK);
    const po = corps(g, 'ITA', 'Army of the Po')!;
    month(g, 'ITA', monthOf('ITA', ARIETE));
    const a = corps(g, 'ITA', ARIETE)!;
    expect(laid.find((e) => e.row === ARIETE)).toMatchObject({ corps: a.id, took: false, fresh: 0 });
    const was = before.get(ARIETE)!.get(po.id)!, now = after.get(ARIETE)!.get(po.id)!;
    expect(after.get(ARIETE)!.get(a.id)).toMatchObject({ divs: 1 });
    expect(after.get(ARIETE)!.get(a.id)!.str).toBeCloseTo(0.3 * was.str, 9);
    expect(now.divs).toBe(3);
    expect(now.str).toBeCloseTo(0.9 * was.str, 9);
    expect(now.out).toBe(1);
    expect(names(g, a)).toEqual([ARIETE]);
    expect(a.kind).toBe('arm');
    expect(place(g, a)).toBe('Tripoli');
    expect(a.hold).toBe(true);
    expect(a.drew).toEqual([{ name: [ARIETE, expect.any(String)], no: expect.any(Number), from: po.id, rec: 1 }]);
    month(g, 'ITA', monthOf('ITA', TRENTO));
    const t = corps(g, 'ITA', TRENTO)!;
    const was2 = before.get(TRENTO)!.get(po.id)!, now2 = after.get(TRENTO)!.get(po.id)!;
    // One of the nine the record still counts in it, of the corps as it stands that day.
    expect(after.get(TRENTO)!.get(t.id)!.str).toBeCloseTo((3 * was2.str) / 9, 9);
    expect(now2.str).toBeCloseTo(was2.str * (8 / 9), 9);
    expect(now2.out).toBe(2);
    const l = laid.find((e) => e.row === TRENTO)!;
    expect(l.after - l.before).toBeCloseTo(0, 9);
  });

  it('with the Army of the Po destroyed, Ariete is raised new, as a division destroyed always was', () => {
    const { g, told } = war(ITALY_UK);
    removeCorps(g, corps(g, 'ITA', 'Army of the Po')!);
    month(g, 'ITA', monthOf('ITA', ARIETE));
    const a = corps(g, 'ITA', ARIETE)!;
    expect(a.divs).toBe(1);
    expect(a.str).toBe(1);
    expect(a.drew).toBeUndefined();
    expect(told.filter((e) => e.row === ARIETE)).toEqual([]);
  });

  it('a saved game keeps what was drawn and from where', () => {
    const { g } = war(ITALY_UK);
    month(g, 'ITA', monthOf('ITA', ARIETE));
    const back = loadGame(saveGame(g))!;
    expect(back).not.toBeNull();
    const po = corps(back, 'ITA', 'Army of the Po')!, a = corps(back, 'ITA', ARIETE)!;
    expect(po.out).toBe(1);
    expect(a.drew).toEqual([{ name: [ARIETE, expect.any(String)], no: expect.any(Number), from: po.id, rec: 1 }]);
    expect(back.flags[`lent:${rowAt('ITA', ARIETE)}`]).toBe(a.id);
  });
});

describe('Libya: the 5th Army is the board\'s Tripolitania row', () => {
  it('while it stands in Libya the Brescia and Bologna rows lay nothing: their divisions are there already', () => {
    const { g, told, laid } = war(ITALY_UK);
    const fifth = corps(g, 'ITA', '5th Army')!;
    const before = fieldDivs(g, 'ITA');
    for (let t = monthOf('ITA', BRESCIA); t <= monthOf('ITA', BOLOGNA); t++) month(g, 'ITA', t);
    for (const name of [BRESCIA, BOLOGNA]) {
      expect(sent(g, 'ITA', name), name).toBe(1);
      expect(corps(g, 'ITA', name), name).toBeUndefined();
      expect(told.find((e) => e.row === name)!.claims.map((q) => [q.from, q.stays])).toEqual([['5th Army', true]]);
      expect(laid.find((e) => e.row === name)).toBeUndefined();
    }
    expect(fifth.divs).toBe(3);
    // (Ariete and Trento came in those months, each a division drawn by share: no other card is new.)
    expect(sent(g, 'ITA', ARIETE)).toBe(1);
    expect(sent(g, 'ITA', TRENTO)).toBe(1);
    expect(fieldDivs(g, 'ITA') - before).toBe(2);
  });

  it('with the 5th Army destroyed the Brescia row is raised new at Tripoli', () => {
    const { g } = war(ITALY_UK);
    removeCorps(g, corps(g, 'ITA', '5th Army')!);
    month(g, 'ITA', monthOf('ITA', BRESCIA));
    const c = corps(g, 'ITA', BRESCIA)!;
    expect(c).toMatchObject({ divs: 1, str: 1 });
    expect(place(g, c)).toBe('Tripoli');
  });

  it('with the 5th Army gone home to Italy, the Brescia row draws its last division out to Tripoli under the row\'s name', () => {
    const { g, laid } = war(ITALY_UK);
    const fifth = corps(g, 'ITA', '5th Army')!;
    relocate(g, fifth, g.byName['Lombardy']);
    fifth.hold = false;
    month(g, 'ITA', monthOf('ITA', BRESCIA));
    const c = corps(g, 'ITA', BRESCIA)!;
    expect(c.divs).toBe(1);
    expect(names(g, c)).toEqual(['Brescia Division']);
    expect(fifth.divs).toBe(2);
    const l = laid.find((e) => e.row === BRESCIA)!;
    expect(l.fresh).toBe(0);
    expect(l.after - l.before).toBeCloseTo(0, 9);
    expect(c.drew).toEqual([{ name: ['Brescia Division', expect.any(String)], no: expect.any(Number), from: fifth.id }]);
  });
});

describe('Tunisia and Sicily', () => {
  it('Pistoia comes with one thirteenth of the 1st Army, and La Spezia, raised in 1941, is new', () => {
    const { g, laid, before, after } = war([['ITA', 'UK'], ['ITA', 'USA']]);
    g.byName['Tunis'].ctrl = 'ITA';
    const first = corps(g, 'ITA', '1st Army')!;
    month(g, 'ITA', monthOf('ITA', PISTOIA));
    const c = corps(g, 'ITA', PISTOIA)!;
    const l = laid.find((e) => e.row === PISTOIA)!;
    expect(l).toMatchObject({ corps: c.id, fresh: 1, str: 1 });
    // One division is new, and one only.
    expect(l.after - l.before).toBeCloseTo(1, 9);
    expect(names(g, c)).toEqual(['Pistoia Division', 'La Spezia Division']);
    const was = before.get(PISTOIA)!.get(first.id)!, now = after.get(PISTOIA)!.get(first.id)!;
    expect(now.str).toBeCloseTo(was.str * (12 / 13), 9);
    expect(now.out).toBe(1);
    const mine = after.get(PISTOIA)!.get(c.id)!;
    expect(mine.divs * mine.str).toBeCloseTo((4 / 13) * was.str + 1, 9);
    expect(c.drew).toEqual([{ name: ['Pistoia Division', expect.any(String)], no: expect.any(Number), from: first.id, rec: 1 }]);
  });

  // (Sicily lies in a side theatre whose landings are history's own to plan, so in a free war the row's own line
  // never comes: game.ts, expeditions, "an invasion force ... is nobody's to send". The table's share is read here on
  // a row of the same name set down at Tripoli.)
  function sixth(human: string | null): { g: Game; sicily: Corps; i: number } {
    const { g } = war([['ITA', 'UK'], ['ITA', 'USA']], human);
    ROWS.push([47, 'ITA', SIXTH, 'inf', 3, 'Tripoli', { xp: 0.05, war: ['UK', 'ITA'] }]);
    added++;
    return { g, sicily: corps(g, 'ITA', 'Sicily Command')!, i: ROWS.length - 1 };
  }

  it('the 6th Army takes three quarters of the Sicily Command where it stands, as W Force takes its brigade', () => {
    const { g, sicily, i } = sixth('ITA');
    expect(sent(g, 'ITA', SIXTH)).toBeUndefined();
    // (Every strength the Sicily Command is given, in order: the first is the row's take, before the month's drafts.)
    let str = 0.8;
    const given: number[] = [];
    Object.defineProperty(sicily, 'str', { get: () => str, set: (x: number) => { given.push(x); str = x; }, configurable: true, enumerable: true });
    // (The row waits for the war the table names, with America, whatever its own words say.)
    expect(countingFor(g, 'ITA', ROWS[i])?.war).toBeUndefined();
    g.turn = 47;
    const news = g.headlines.length;
    startPhase(g, 'ITA');
    expect(g.flags[`sent:${i}`]).toBe(1);
    const c = corps(g, 'ITA', SIXTH)!;
    expect(c.divs).toBe(3);
    expect(sicily.divs).toBe(2);
    // Three quarters go: a quarter of 0.8 stays. (The drafts of the month have gone out since.)
    expect(given[0]).toBeCloseTo(0.8 * 0.25, 9);
    expect(sicily.str).toBeLessThan(0.8);
    const line = g.headlines.slice(news).find((h) => /6th Army is formed/.test(h.text))!;
    expect(line.text).toBe('The 6th Army is formed in Sicily out of three quarters of your Sicily Command, in Sicily. The rest stays on as the Sicily Command.');
  });

  it('where the Sicily Command stands at the row\'s own place, the place is said once', () => {
    const { g, sicily } = sixth('ITA');
    sicily.hold = false;
    relocate(g, sicily, g.byName['Tripoli']);
    g.turn = 47;
    const news = g.headlines.length;
    startPhase(g, 'ITA');
    const line = g.headlines.slice(news).find((h) => /6th Army is formed/.test(h.text))!;
    expect(line.text).toBe('The 6th Army is formed in Sicily out of three quarters of your Sicily Command. The rest stays on as the Sicily Command.');
  });
});

describe('Romania: the armies of the Don are the board\'s own corps', () => {
  /** Germany and Romania at war with the Soviet Union, the two places of the rows in German hands, Romania's corps of month 21 landed. */
  function don(): ReturnType<typeof war> {
    const w = war([['GER', 'SOV'], ['ROM', 'SOV']]);
    const g = w.g;
    g.nations['ROM'].align = 'axis';
    // (And the ground between them and Bessarabia, clear of the enemy: an army set down on an island in Soviet
    // ground starves in three months, and nothing of the rule could be read after.)
    for (const to of ['Don Bend', 'Kalmyk Steppe']) {
      const from = g.byName['Bessarabia'].id, goal = g.byName[to].id;
      const prev = new Map<number, number>([[from, -1]]);
      const queue = [from];
      while (queue.length && !prev.has(goal)) { const at = queue.shift()!; for (const nb of g.regions[at].nb) if (!prev.has(nb)) { prev.set(nb, at); queue.push(nb); } }
      for (let at = goal; at !== from && at >= 0; at = prev.get(at)!) {
        g.regions[at].ctrl = 'GER';
        for (const u of g.corps.filter((c) => c.reg === at && !c.dead && c.nat === 'SOV')) removeCorps(g, u);
      }
    }
    g.supplyDirty = true;
    month(g, 'ROM', 21);
    // (By the rows' months the front stands far to the east: no Romanian corps stands beside the enemy. A corps in a
    // fight keeps its divisions, the design director's ruling of 18:47Z, read on its own below.)
    for (const c of g.corps.filter((x) => x.nat === 'ROM' && !x.dead)) for (const j of [c.reg, ...g.regions[c.reg].nb]) {
      if (g.regions[j].ctrl !== 'SOV') continue;
      g.regions[j].ctrl = 'GER';
      for (const u of g.corps.filter((x) => x.reg === j && !x.dead && x.nat === 'SOV')) removeCorps(g, u);
    }
    return w;
  }
  const A = '3rd Army: I and II Corps', B = '3rd Army: IV and V Corps', C = '4th Army: VI Corps', D = '4th Army: VII Corps';

  it('the four rows lay no new division: fourteen come out of the corps that stand for them, three of which come themselves', () => {
    const { g, laid } = don();
    const ii = corps(g, 'ROM', 'II Corps')!, v = corps(g, 'ROM', 'V Corps')!, third = corps(g, 'ROM', '3rd Army')!, fourth = corps(g, 'ROM', '4th Army')!, cav = corps(g, 'ROM', 'Cavalry Corps')!;
    const before = fieldDivs(g, 'ROM');
    const cavNames = names(g, cav);
    month(g, 'ROM', 37);
    month(g, 'ROM', 38);
    for (const name of [A, B, C, D]) {
      expect(sent(g, 'ROM', name), name).toBe(1);
      const l = laid.find((e) => e.row === name)!;
      expect(l.fresh, name).toBe(0);
      expect(l.after - l.before, name).toBeCloseTo(0, 9);
    }
    expect(fieldDivs(g, 'ROM')).toBe(before);
    // The II Corps is the row's own: it comes itself, with one division of the 3rd Army.
    expect(corps(g, 'ROM', A)).toBe(ii);
    expect(ii.divs).toBe(4);
    expect(ii.rowed).toMatchObject({ name: 'II Corps' });
    // The 3rd Army, its last two divisions the next row's, comes itself with two of the V Corps.
    expect(corps(g, 'ROM', B)).toBe(third);
    expect(third.divs).toBe(4);
    expect(third.rowed).toMatchObject({ name: '3rd Army', hold: true });
    expect(corps(g, 'ROM', C)).toBe(fourth);
    expect(fourth.divs).toBe(3);
    // The V Corps' last division with the Cavalry Corps' two, which is no more.
    expect(corps(g, 'ROM', D)).toBe(v);
    expect(v.divs).toBe(3);
    expect(names(g, v).slice(1)).toEqual(cavNames);
    expect(cav.dead || !g.corps.includes(cav)).toBe(true);
    for (const c of [ii, third, fourth, v]) expect(['Don Bend', 'Kalmyk Steppe']).toContain(place(g, c));
  });

  it('the war over, each division goes back to the corps it came out of, and a corps that came itself has its own name and orders again', () => {
    const { g } = don();
    const ii = corps(g, 'ROM', 'II Corps')!, v = corps(g, 'ROM', 'V Corps')!, third = corps(g, 'ROM', '3rd Army')!, fourth = corps(g, 'ROM', '4th Army')!;
    const before = fieldDivs(g, 'ROM');
    month(g, 'ROM', 37);
    month(g, 'ROM', 38);
    // While the war lasts nothing goes back.
    month(g, 'ROM', 39);
    expect(corps(g, 'ROM', A)).toBe(ii);
    signPeace(g, 'GER', 'SOV');
    month(g, 'ROM', 40);
    expect(ii.name).toBe('II Corps');
    expect(third.name).toBe('3rd Army');
    expect(fourth.name).toBe('4th Army');
    expect(v.name).toBe('V Corps');
    // The 3rd Army has its division back from the II Corps, and the V Corps its two from the 3rd Army: as they stood.
    expect(ii.divs).toBe(3);
    expect(third.divs).toBe(3);
    expect(fourth.divs).toBe(3);
    // (The Cavalry Corps was broken up into the row: its two divisions have no corps to go back to, and stand on.)
    expect(v.divs).toBe(5);
    expect(third.hold).toBe(true);
    expect(ii.hold).toBe(false);
    for (const c of [ii, third, fourth, v]) { expect(c.drew).toBeUndefined(); expect(c.rowed).toBeUndefined(); }
    for (const name of [A, B, C, D]) expect(g.flags[`lent:${rowAt('ROM', name)}`], name).toBe(-1);
    expect(fieldDivs(g, 'ROM')).toBe(before);
    // Nothing is told again, and nothing more moves.
    month(g, 'ROM', 41);
    expect(third.divs).toBe(3);
  });
});

describe('the war\'s end, for what a row raised new', () => {
  it('Pistoia and La Spezia: the war with America over, La Spezia, raised new, stays with the corps, and Pistoia stands on in Tunisia', () => {
    const { g } = war([['ITA', 'UK'], ['ITA', 'USA']]);
    g.byName['Tunis'].ctrl = 'ITA';
    month(g, 'ITA', monthOf('ITA', PISTOIA));
    const c = corps(g, 'ITA', PISTOIA)!;
    const i = rowAt('ITA', PISTOIA);
    expect(g.flags[`lent:${i}`]).toBe(c.id);
    signPeace(g, 'ITA', 'USA');
    month(g, 'ITA', monthOf('ITA', PISTOIA) + 1);
    expect(c.dead).toBeFalsy();
    expect(g.corps).toContain(c);
    expect(names(g, c)).toEqual(['Pistoia Division', 'La Spezia Division']);
    expect(c.drew).toBeUndefined();
    expect(c.rowed).toBeUndefined();
    expect(g.flags[`lent:${i}`]).toBe(-1);
  });
});

describe('the war\'s end, for a division drawn by share', () => {
  it('in another theatre than the corps it came out of, it stands on where it is, and the row\'s corps keeps its name and orders', () => {
    const { g } = war(ITALY_UK);
    const po = corps(g, 'ITA', 'Army of the Po')!;
    month(g, 'ITA', monthOf('ITA', ARIETE));
    const a = corps(g, 'ITA', ARIETE)!;
    signPeace(g, 'ITA', 'UK');
    month(g, 'ITA', monthOf('ITA', ARIETE) + 1);
    expect(a.dead).toBeFalsy();
    expect(a.name).toBe(ARIETE);
    expect(a.hold).toBe(true);
    expect(a.drew).toBeUndefined();
    expect(po.divs).toBe(3);
    // The record's division is out of the Army of the Po for good.
    expect(po.out).toBe(1);
    expect(g.flags[`lent:${rowAt('ITA', ARIETE)}`]).toBe(-1);
  });

  it('in the same theatre it goes back as a division of its own, and the corps counts the record\'s divisions as its own again', () => {
    const { g } = war(ITALY_UK);
    const po = corps(g, 'ITA', 'Army of the Po')!;
    month(g, 'ITA', monthOf('ITA', ARIETE));
    const a = corps(g, 'ITA', ARIETE)!;
    relocate(g, po, g.byName['Tripoli']);
    const men = po.divs * po.str + a.divs * a.str;
    signPeace(g, 'ITA', 'UK');
    g.turn = monthOf('ITA', ARIETE) + 1;
    // (Read at the war's end itself, before the month's drafts: the rule's own function.)
    const laidOff = g.corps.filter((c) => c.nat === 'ITA' && !c.dead).length;
    startPhase(g, 'ITA');
    expect(g.corps.includes(a) && !a.dead).toBe(false);
    expect(po.divs).toBe(4);
    expect(names(g, po)).toContain(ARIETE);
    expect(po.out).toBeUndefined();
    // (No man is lost by it: the drafts of the month can only have added.)
    expect(po.divs * po.str).toBeGreaterThanOrEqual(men - 1e-9);
    expect(g.corps.filter((c) => c.nat === 'ITA' && !c.dead).length).toBeLessThan(laidOff + 1);
  });
});

describe('the player\'s own corps are never drawn for him', () => {
  it('Ariete is not formed, his Army of the Po keeps every division, and the line tells him so', () => {
    const { g, laid } = war(ITALY_UK, 'ITA');
    const po = corps(g, 'ITA', 'Army of the Po')!;
    const before = fieldDivs(g, 'ITA');
    g.turn = monthOf('ITA', ARIETE);
    const news = g.headlines.length;
    startPhase(g, 'ITA');
    expect(sent(g, 'ITA', ARIETE)).toBe(1);
    expect(corps(g, 'ITA', ARIETE)).toBeUndefined();
    expect(laid).toEqual([]);
    expect(po.divs).toBe(3);
    expect(po.out).toBeUndefined();
    expect(fieldDivs(g, 'ITA')).toBe(before);
    const line = g.headlines.slice(news).find((h) => /Ariete/.test(h.text))!;
    expect(line.text).toMatch(said(DRAWN.from));
    expect(line.text).toBe('The Ariete Armoured Division was due in Tripoli this month, drawn from your Army of the Po in Lombardy. No division is raised twice: it is not formed, and your Army of the Po keeps every division it has.');
    // Told once.
    month(g, 'ITA', monthOf('ITA', ARIETE) + 1);
    expect(g.headlines.filter((h) => /Ariete Armoured Division was due/.test(h.text))).toHaveLength(1);
  });

  it('the 9th Army: so many divisions, to make up the row, from his 1st Army', () => {
    const { g } = war(ITALY_GREECE, 'ITA');
    g.turn = monthOf('ITA', NINTH);
    const news = g.headlines.length;
    startPhase(g, 'ITA');
    expect(corps(g, 'ITA', NINTH)).toBeUndefined();
    const line = g.headlines.slice(news).find((h) => /9th Army/.test(h.text))!;
    expect(line.text).toMatch(said(DRAWN.fromMany));
    expect(line.text).toBe('Four divisions were due in Albania this month to make up the 9th Army, drawn from your 1st Army in Liguria. No division is raised twice: they are not formed, and your 1st Army keeps every division it has.');
    expect(corps(g, 'ITA', '1st Army')!.out).toBeUndefined();
  });

  it('no division of his is named for two rows: each row counts the cards before the ones an earlier row named', () => {
    const { g, told } = war(ITALY_GREECE, 'ITA');
    const albania = corps(g, 'ITA', 'Albania Command')!, eighth = corps(g, 'ITA', '8th Army')!, third = corps(g, 'ITA', '3rd Army')!;
    for (const row of [CIAMURIA, NINTH, ELEVENTH]) month(g, 'ITA', monthOf('ITA', row));
    const named: string[] = [];
    for (const row of [CIAMURIA, XXVI, ELEVENTH]) for (const q of told.filter((e) => e.row === row).at(-1)!.claims) if (!q.share) named.push(q.name);
    expect(named).toHaveLength(3 + 2 + 2);
    expect(new Set(named).size).toBe(named.length);
    expect([albania.kept, eighth.kept, third.kept]).toEqual([3, 3, 1]);
    expect([albania.divs, eighth.divs, third.divs]).toEqual([4, 3, 2]);
  });

  it('the Ciamuria Corps: the divisions the table counts in his corps are his already, told by their own names', () => {
    const { g } = war(ITALY_GREECE, 'ITA');
    const albania = corps(g, 'ITA', 'Albania Command')!, eighth = corps(g, 'ITA', '8th Army')!;
    g.turn = monthOf('ITA', CIAMURIA);
    const news = g.headlines.length;
    startPhase(g, 'ITA');
    expect(corps(g, 'ITA', CIAMURIA)).toBeUndefined();
    expect(albania.divs).toBe(4);
    expect(eighth.divs).toBe(3);
    const line = g.headlines.slice(news).find((h) => / are due in Albania /.test(h.text))!;
    expect(line.text).toMatch(said(DRAWN.many));
    expect(line.text).toContain(' Infantry Divisions are due in Albania this month, but you hold them already, in Albania and Veneto.');
    expect(line.text).not.toMatch(/\{\w+\}/);
  });
});
