// A division is laid once (release 2, the grand campaign; src/campaign/drawn.ts).
//
// The design director's rulings of 13:43Z to 14:28Z, 25 November 2026, on the joiner's six-year check
// (joins/named-twice-check-1125.md) and the historian's read of every pair (historian/named-twice-reads-1125.md): a
// dated row that names a division a living corps of its country already holds does not lay it a second time. The
// Balkans' rows of March and April 1941 are read here on a board set by hand, since they name divisions of four
// corps Germany has on her first day: XXII Corps is the 2nd and 9th Panzer Divisions, III Corps holds the 50th
// Infantry, XV Corps the 8th Panzer, and IV Corps the 4th Infantry the 14th Panzer was formed out of. Whole six-year
// wars are read in price-once-wars-r2.test.ts, and the names the rule reads in price-once-names-r2.test.ts.
//
// (A month's start lays the dated rows before the drafts go out, and the drafts fill a corps under strength: so what
// a row did to the men is read at the moment it is laid, by the rule's own watch, and after the month by experience,
// which no draft touches in a corps at full strength.)
import { afterEach, describe, expect, it } from 'vitest';
import setup from '../data/campaign/setup-1939.json';
import { divisionsOf, growCorps, keepNumbers, numberCorps, numberSpokenFor } from '../src/campaign/armies';
import { DRAWN, corpsInContact, drawnWatch, formationName, rowOver } from '../src/campaign/drawn';
import { setStop } from '../src/campaign/history';
import { breakUp, rejoin, setBreakUp } from '../src/campaign/breakup';
import { foldCorps } from '../src/campaign/fold';
import { createGame, declareWar, putToSea, relocate, removeCorps, startPhase } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const MARCH_41 = 19, APRIL_41 = 20, JUNE_41 = 22;
const ROWS = (setup as any).expeditions as any[];
const rowAt = (name: string): number => ROWS.findIndex((r) => r[1] === 'GER' && r[2] === name);
const PANZERS = '2nd and 9th Panzer Divisions', INFANTRY = '50th, 72nd and 164th Infantry Divisions', EIGHTH = '8th and 14th Panzer Divisions';

type Told = Parameters<NonNullable<typeof drawnWatch.tell>>[0];
type Laid = Parameters<NonNullable<typeof drawnWatch.laid>>[0];

/**
 * A grand campaign with Italy's war on Greece begun and Germany in it; with `allies`, Bulgaria, Hungary and Romania
 * are in that war on her side, so that her rows are put down where the record put them; without, they are friends at
 * peace, and her rows form at home. `human` seats a player.
 */
function balkans(human: string | null = null, allies = true, open = true): { g: Game; told: Told[]; laid: Laid[]; then: Map<string, Map<number, Shot>> } {
  const g = open ? startOpen(createGame(1, human)) : createGame(1, human);
  // (The board is the first day's, read in 1941: Germany's first wars stand still on it, so that no German corps stands
  // beside an enemy, as none did behind the lines that spring. A corps in a fight keeps its divisions: the design
  // director's ruling of 18:47Z, 25 November 2026, read in its own test below.)
  for (const x of [...g.nations.GER.atWar]) { g.nations.GER.atWar = g.nations.GER.atWar.filter((y) => y !== x); g.nations[x].atWar = g.nations[x].atWar.filter((y) => y !== 'GER'); }
  declareWar(g, 'ITA', 'GRE');
  declareWar(g, 'GER', 'GRE');
  for (const n of ['BUL', 'HUN', 'ROM']) { g.nations[n].align = 'axis'; if (allies) declareWar(g, n, 'GRE'); }
  const told: Told[] = [], laid: Laid[] = [];
  // Every German corps as it stood the moment a row had been laid, by the row's name and the corps' id.
  const then = new Map<string, Map<number, Shot>>();
  drawnWatch.tell = (e) => told.push(e);
  drawnWatch.laid = (e) => {
    laid.push(e);
    then.set(e.row, new Map(g.corps.filter((c) => c.nat === 'GER' && !c.dead).map((c) => [c.id, { divs: c.divs, str: c.str, xp: c.xp, org: c.org }])));
  };
  return { g, told, laid, then };
}
interface Shot { divs: number; str: number; xp: number; org: number }
/** Rows a test adds at the foot of the data's list, taken off again after it (a row's flags are kept by its place in the list). */
let added = 0;
function addRow(row: any[]): number { ROWS.push(row); added++; return ROWS.length - 1; }
afterEach(() => { drawnWatch.tell = null; drawnWatch.laid = null; while (added > 0) { ROWS.pop(); added--; } });
/** A line as the words file writes it, its places filled by anything. */
const said = (words: string): RegExp => new RegExp(words.replace(/[.*+?^${}()|[\]\\]/g, '\\$&').replace(/\\\{\w+\\\}/g, '.+'));

const month = (g: Game, turn: number): void => { g.turn = turn; startPhase(g, 'GER'); };
const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'GER' && !c.dead && c.name === name);
const names = (g: Game, c: Corps): string[] => divisionsOf(g, c).map((d) => d.name);
const place = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** The living corps of Germany that hold a division of this name. */
const holders = (g: Game, name: string): Corps[] => g.corps.filter((c) => c.nat === 'GER' && !c.dead && names(g, c).includes(name));
/** Every division name two living corps of one country hold. */
function heldTwice(g: Game): string[] {
  const out: string[] = [];
  for (const nat of Object.keys(g.nations)) {
    const seen = new Map<string, Corps>();
    for (const c of g.corps) if (c.nat === nat && !c.dead) for (const d of divisionsOf(g, c)) {
      const was = seen.get(d.name);
      if (was && was !== c) out.push(`${nat} ${d.name}: ${was.name} and ${c.name}`);
      seen.set(d.name, c);
    }
  }
  return out;
}

describe('a dated row that names a division a living corps holds (the design director, 25 November 2026)', () => {
  it('a corps all of whose divisions the row names comes itself, under the row\'s name and orders, and nothing is raised', () => {
    const { g, told, laid } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    expect(names(g, xxii)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(g.regions[xxii.reg].theatre).toBe(-1);
    xxii.str = 0.8; xxii.xp = 0.6;
    const before = g.corps.filter((c) => c.nat === 'GER').length;
    month(g, MARCH_41);
    const e = told.find((t) => t.row === PANZERS)!;
    expect(e.claims.map((q) => [q.name, q.id, q.stays])).toEqual([['2nd Panzer Division', xxii.id, false], ['9th Panzer Division', xxii.id, false]]);
    const l = laid.find((t) => t.row === PANZERS)!;
    expect(l).toMatchObject({ corps: xxii.id, took: true, fresh: 0 });
    expect(l.after).toBeCloseTo(l.before, 9);
    // The same corps, at Sofia under the row's name and its order to hold; its men and experience are its own.
    expect(corps(g, PANZERS)).toBe(xxii);
    expect(corps(g, 'XXII Corps')).toBeUndefined();
    expect(place(g, xxii)).toBe('Sofia');
    expect(g.byName['Sofia'].units).toContain(xxii);
    expect(xxii.hold).toBe(true);
    expect(xxii.divs).toBe(2);
    expect(xxii.xp).toBeCloseTo(0.6, 9);
    expect(names(g, xxii)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(divisionsOf(g, xxii).map((d) => d.no)).toEqual([2, 9]);
    expect(holders(g, '2nd Panzer Division')).toEqual([xxii]);
    expect(g.flags[`lent:${rowAt(PANZERS)}`]).toBe(xxii.id);
    // The other rows of the month, which name nobody's divisions, are laid as they always were.
    expect(corps(g, '5th and 11th Panzer Divisions')!.divs).toBe(2);
    expect(g.corps.filter((c) => c.nat === 'GER').length).toBeGreaterThan(before);
    expect(heldTwice(g)).toEqual([]);
  });

  it('when the row\'s time is up the corps has its own name back and stands on where it is, free of the row\'s orders', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    month(g, MARCH_41);
    expect(xxii.rowed).toEqual({ name: 'XXII Corps', hold: false });
    // (A save between the two months keeps where its divisions came from.)
    const g2 = loadGame(saveGame(g))!;
    for (const game of [g, g2]) {
      const c = game.corps.find((x) => x.id === xxii.id)!;
      month(game, JUNE_41);
      expect(c.dead).toBe(false);
      expect(c.name).toBe('XXII Corps');
      expect(c.hold).toBe(false);
      expect(place(game, c)).toBe('Sofia');
      expect(names(game, c)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
      expect(c.drew).toBeUndefined();
      expect(c.rowed).toBeUndefined();
      expect(game.flags[`lent:${rowAt(PANZERS)}`]).toBe(-1);
      // The row's own corps beside it, raised new, leaves as a lent row's always did, and the news says so of it alone.
      expect(game.corps.some((x) => x.name === '5th and 11th Panzer Divisions' && !x.dead)).toBe(false);
      const left = game.headlines.filter((h) => h.turn === JUNE_41 && /leave for other fronts/.test(h.text));
      expect(left).toHaveLength(1);
      expect(left[0].text).toContain('5th and 11th Panzer Divisions');
      expect(left[0].text).not.toContain(PANZERS);
      expect(left[0].text).not.toContain('XXII Corps');
      // The corps that stands on has not left: its own line, under the name it stands on by, in the file's words.
      const on = game.headlines.filter((h) => h.turn === JUNE_41 && said(DRAWN.stood).test(h.text));
      expect(on).toHaveLength(1);
      expect(on[0].text).toContain('German formations lent to the campaign in the Balkans');
      expect(on[0].text).toMatch(/: XXII Corps\.$/);
    }
  });

  it('one division of a bigger corps is drawn with an even share of it, the corps keeps the rest, and only the new divisions are new strength', () => {
    const { g, laid, then } = balkans();
    const iii = corps(g, 'III Corps')!;
    expect(names(g, iii)).toEqual(['50th Infantry Division', '218th Infantry Division', '207th Infantry Division']);
    iii.str = 0.6; iii.xp = 0.9; iii.org = 0.5;
    month(g, MARCH_41);
    const c = corps(g, INFANTRY)!;
    const l = laid.find((t) => t.row === INFANTRY)!;
    expect(l).toMatchObject({ corps: c.id, took: false, fresh: 2, str: 1 });
    expect(l.after - l.before).toBeCloseTo(2, 9);
    // As the row was laid: III Corps keeps two divisions as they stood, and the row's corps has the 50th as it stood
    // in III Corps beside two divisions raised at the row's own figures.
    const shot = then.get(INFANTRY)!;
    expect(shot.get(iii.id)).toEqual({ divs: 2, str: 0.6, xp: 0.9, org: 0.5 });
    const mine = shot.get(c.id)!;
    expect(mine.divs).toBe(3);
    expect(mine.str * mine.divs).toBeCloseTo(0.6 + 2, 9);
    expect(mine.xp).toBeCloseTo((0.9 * 0.6 + 0.3 * 2) / 2.6, 9);
    expect(mine.org).toBeCloseTo((0.5 * 0.6 + 1 * 2) / 2.6, 9);
    // III Corps keeps its other divisions' names and numbers, and stays where it was.
    expect(iii.divs).toBe(2);
    expect(names(g, iii)).toEqual(['218th Infantry Division', '207th Infantry Division']);
    expect(divisionsOf(g, iii).map((d) => d.no)).toEqual([218, 207]);
    expect(place(g, iii)).not.toBe('Plovdiv');
    expect(place(g, c)).toBe('Plovdiv');
    expect(c.hold).toBe(true);
    expect(names(g, c)).toEqual(['50th Infantry Division', '72nd Infantry Division', '164th Infantry Division']);
    expect(divisionsOf(g, c)[0].no).toBe(50);
    expect(holders(g, '50th Infantry Division')).toEqual([c]);
    // (The row has no `until`, so it is no loan with a month to end in. But it is keyed to a war, and such a row is
    // lent for that war: the design director's ruling 2 of 16:45Z, 25 November 2026, drawn.ts "the war's end". Where
    // the 50th came from is remembered, and while the war lasts nothing goes back.)
    expect(c.drew).toEqual([{ name: ['50th Infantry Division', '50th Infantry'], no: 50, from: iii.id }]);
    month(g, JUNE_41);
    expect(names(g, c)).toHaveLength(3);
    expect(iii.divs).toBe(2);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a division formed out of another is that other one, drawn from the corps that holds it: never new strength', () => {
    const { g, told, laid } = balkans();
    const xv = corps(g, 'XV Corps')!, iv = corps(g, 'IV Corps')!;
    expect(names(g, xv)).toEqual(['7th Panzer Division', '8th Panzer Division']);
    expect(names(g, iv)).toEqual(['4th Infantry Division', '46th Infantry Division', '27th Infantry Division']);
    month(g, MARCH_41);
    month(g, APRIL_41);
    const e = told.find((t) => t.row === EIGHTH)!;
    expect(e.claims.map((q) => [q.name, q.id, q.conv])).toEqual([['8th Panzer Division', xv.id, null], ['14th Panzer Division', iv.id, '4th Infantry Division']]);
    const l = laid.find((t) => t.row === EIGHTH)!;
    expect(l.fresh).toBe(0);
    expect(l.after).toBeCloseTo(l.before, 9);
    const c = corps(g, EIGHTH)!;
    expect(place(g, c)).toBe('Transdanubia');
    expect(names(g, c)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(divisionsOf(g, c).map((d) => d.no)).toEqual([8, 14]);
    expect(names(g, xv)).toEqual(['7th Panzer Division']);
    expect(names(g, iv)).toEqual(['46th Infantry Division', '27th Infantry Division']);
    expect(holders(g, '4th Infantry Division')).toEqual([]);
    expect(c.drew).toEqual([
      { name: ['8th Panzer Division', '8th Panzer'], no: 8, from: xv.id },
      { name: ['14th Panzer Division', '14th Panzer'], no: 14, from: iv.id, conv: true },
    ]);
    expect(heldTwice(g)).toEqual([]);
  });

  it('with nobody holding the old division it is taken out of the largest free infantry corps on home soil', () => {
    const { g, told, laid, then } = balkans();
    removeCorps(g, corps(g, 'IV Corps')!);
    month(g, MARCH_41);
    const was = new Map(g.corps.filter((c) => c.nat === 'GER' && !c.dead).map((c) => [c.id, c.divs]));
    month(g, APRIL_41);
    const donor = told.find((t) => t.row === EIGHTH)!.claims[1];
    expect(donor).toMatchObject({ name: '14th Panzer Division', conv: '4th Infantry Division', stays: false });
    const d = g.corps.find((c) => c.id === donor.id)!;
    expect(d.kind).toBe('inf');
    expect(d.hold).toBe(false);
    expect(g.regions[d.reg].owner).toBe('GER');
    // The largest such corps as the month opened, and it is one division the fewer as the row is laid.
    expect(was.get(d.id)).toBe(Math.max(...g.corps.filter((c) => was.has(c.id) && c.kind === 'inf' && !c.hold && c.town === undefined && g.regions[c.reg].owner === 'GER').map((c) => was.get(c.id)!)));
    expect(then.get(EIGHTH)!.get(d.id)!.divs).toBe(was.get(d.id)! - 1);
    const l = laid.find((t) => t.row === EIGHTH)!;
    expect(l.fresh).toBe(0);
    expect(l.after).toBeCloseTo(l.before, 9);
    expect(names(g, corps(g, EIGHTH)!)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(heldTwice(g)).toEqual([]);
  });

  it('when a lent row ends a drawn division goes back to its corps where the two stand together, and a formed one never reverts', () => {
    const { g } = balkans();
    const xv = corps(g, 'XV Corps')!, iv = corps(g, 'IV Corps')!;
    month(g, MARCH_41);
    month(g, APRIL_41);
    const c = corps(g, EIGHTH)!;
    // XV Corps has followed into the Balkans, to the row's corps itself: the 8th Panzer rejoins it there. (A return
    // never moves a division for free: the design director's rulings of 19:03Z and 19:17Z, 25 November 2026.)
    relocate(g, xv, g.regions[c.reg]);
    c.xp = 0.9;
    month(g, JUNE_41);
    expect(names(g, xv)).toEqual(['7th Panzer Division', '8th Panzer Division']);
    expect(divisionsOf(g, xv).map((d) => d.no)).toEqual([7, 8]);
    expect(xv.name).toBe('XV Corps');
    // The 14th Panzer stands on where it is, a corps of its own under its own name, free of the row's orders.
    expect(c.dead).toBe(false);
    expect(c.name).toBe('14th Panzer Division');
    expect(names(g, c)).toEqual(['14th Panzer Division']);
    expect(c.hold).toBe(false);
    expect(place(g, c)).toBe('Transdanubia');
    expect(c.drew).toBeUndefined();
    // Each brings its corps' figures as they stand: the 8th what the row's corps had learned, to XV Corps' own.
    expect(xv.divs + c.divs).toBe(3);
    expect(xv.xp).toBeCloseTo((0.4 + 0.9) / 2, 9);
    expect(c.xp).toBeCloseTo(0.9, 9);
    expect(names(g, iv)).toEqual(['46th Infantry Division', '27th Infantry Division']);
    expect(heldTwice(g)).toEqual([]);
  });

  it('in the same theatre but apart from its corps, the division stands on where it is, under its own name and free: no return moves it (19:17Z)', () => {
    const { g } = balkans();
    const xv = corps(g, 'XV Corps')!;
    month(g, MARCH_41);
    month(g, APRIL_41);
    const c = corps(g, EIGHTH)!;
    // XV Corps has followed into the Balkans, to the Banat: the same side theatre, another province.
    relocate(g, xv, g.byName['Banat']);
    expect(g.regions[xv.reg].theatre).toBe(g.regions[c.reg].theatre);
    expect(xv.reg).not.toBe(c.reg);
    const at = place(g, c);
    month(g, JUNE_41);
    expect(names(g, xv)).toEqual(['7th Panzer Division']);
    expect(c.dead).toBe(false);
    expect(names(g, c)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(c.name).toBe('8th and 14th Panzer Divisions');
    expect(c.hold).toBe(false);
    expect(place(g, c)).toBe(at);
    expect(c.drew).toBeUndefined();
    expect(heldTwice(g)).toEqual([]);
  });

  it('and with its corps on another front the division stands on with the row\'s corps, under the names of what it holds', () => {
    const { g } = balkans();
    const xv = corps(g, 'XV Corps')!;
    month(g, MARCH_41);
    month(g, APRIL_41);
    const c = corps(g, EIGHTH)!;
    expect(g.regions[xv.reg].theatre).toBe(-1);
    month(g, JUNE_41);
    expect(c.dead).toBe(false);
    expect(c.name).toBe(EIGHTH);
    expect(names(g, c)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(c.hold).toBe(false);
    expect(c.drew).toBeUndefined();
    expect(names(g, xv)).toEqual(['7th Panzer Division']);
  });

  it('and with its corps gone when the loan ends, the division stands on where it is, under the names of what it holds, and moves nowhere', () => {
    // (The row is lent with a month to end in: no six-year war of the gate has reached that end, so it is pinned here.
    // The design director, 20:29Z, 25 November 2026.)
    expect(ROWS[rowAt(EIGHTH)][6].until).toBe(JUNE_41);
    const { g } = balkans();
    const xv = corps(g, 'XV Corps')!;
    month(g, MARCH_41);
    month(g, APRIL_41);
    const c = corps(g, EIGHTH)!;
    expect(c.drew!.find((r) => r.name[0] === '8th Panzer Division')!.from).toBe(xv.id);
    const at = c.reg;
    removeCorps(g, xv);
    month(g, JUNE_41);
    expect(c.dead).toBe(false);
    expect(c.reg).toBe(at);
    expect(c.name).toBe(EIGHTH);
    expect(names(g, c)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(c.hold).toBe(false);
    expect(c.drew).toBeUndefined();
    expect(holders(g, '8th Panzer Division')).toEqual([c]);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a row never pulls a division out of the theatre it is for: one already there stays with its corps, and the row forms only its new divisions', () => {
    const { g, told, laid } = balkans();
    const xxii = corps(g, 'XXII Corps')!, iii = corps(g, 'III Corps')!;
    relocate(g, xxii, g.byName['Banat']);
    relocate(g, iii, g.byName['Sofia']);
    month(g, MARCH_41);
    expect(told.find((t) => t.row === PANZERS)!.claims.every((q) => q.stays)).toBe(true);
    // Both of the row's divisions stand in the Balkans already: nothing is formed, and the row has landed.
    expect(laid.find((t) => t.row === PANZERS)).toBeUndefined();
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(xxii.name).toBe('XXII Corps');
    expect(place(g, xxii)).toBe('Banat');
    expect(xxii.hold).toBe(false);
    expect(g.flags[`sent:${rowAt(PANZERS)}`]).toBe(1);
    // The 50th stays with III Corps at Sofia; the 72nd and 164th form at Plovdiv. Their corps carries the row's name,
    // by which every other row would find it, and its cards read the two divisions it holds.
    expect(iii.divs).toBe(3);
    expect(place(g, iii)).toBe('Sofia');
    const c = corps(g, INFANTRY)!;
    expect(place(g, c)).toBe('Plovdiv');
    expect(c.divs).toBe(2);
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    expect(holders(g, '50th Infantry Division')).toEqual([iii]);
    const l = laid.find((t) => t.row === INFANTRY)!;
    expect(l.after - l.before).toBeCloseTo(2, 9);
    // (And it is not tried again next month.)
    month(g, APRIL_41);
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(heldTwice(g)).toEqual([]);
  });

  it('a row that falls back to forming at home takes nobody: every held division stays with its corps', () => {
    const { g, told } = balkans(null, false);
    const xxii = corps(g, 'XXII Corps')!, iii = corps(g, 'III Corps')!;
    const at = place(g, xxii);
    month(g, MARCH_41);
    const e = told.find((t) => t.row === PANZERS)!;
    expect(e.home).toBe(true);
    expect(e.claims.every((q) => q.stays)).toBe(true);
    expect(xxii.name).toBe('XXII Corps');
    expect(place(g, xxii)).toBe(at);
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(iii.divs).toBe(3);
    const c = corps(g, INFANTRY)!;
    expect(g.regions[c.reg].owner).toBe('GER');
    expect(c.divs).toBe(2);
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a division whose corps has died is laid whole, as a first arrival is', () => {
    const { g, told } = balkans();
    removeCorps(g, corps(g, 'XXII Corps')!);
    month(g, MARCH_41);
    expect(told.find((t) => t.row === PANZERS)).toBeUndefined();
    const c = corps(g, PANZERS)!;
    expect(c.born).toBe(MARCH_41);
    expect(c.divs).toBe(2);
    expect(c.str).toBe(1);
    expect(names(g, c)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(place(g, c)).toBe('Sofia');
  });

  it('a corps in a fight keeps its divisions: with an enemy beside it the row forms without them (the design director, 18:47Z)', () => {
    const { g, told, laid } = balkans();
    const iii = corps(g, 'III Corps')!;
    const at = place(g, iii);
    // (Germany at war with Poland again, whose ground lies beside III Corps.)
    const beside = g.regions[iii.reg].nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL');
    expect(beside).toBeDefined();
    declareWar(g, 'GER', 'POL');
    month(g, MARCH_41);
    expect(told.find((t) => t.row === INFANTRY)!.claims).toEqual([expect.objectContaining({ name: '50th Infantry Division', from: 'III Corps', stays: true })]);
    // The 50th stays and fights where it is; the row forms its two new divisions, and nothing of III Corps.
    const l = laid.find((t) => t.row === INFANTRY)!;
    expect(l.fresh).toBe(2);
    expect(l.after - l.before).toBeCloseTo(2, 9);
    expect(iii.divs).toBe(3);
    expect(place(g, iii)).toBe(at);
    expect(names(g, corps(g, INFANTRY)!)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a corps that cannot leave where it is stays: one sent home out of the line is not drawn', () => {
    const { g, told } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    xxii.leave = APRIL_41;
    const at = place(g, xxii);
    month(g, MARCH_41);
    expect(told.find((t) => t.row === PANZERS)!.claims.every((q) => q.stays)).toBe(true);
    expect(place(g, xxii)).toBe(at);
    expect(xxii.name).toBe('XXII Corps');
    expect(corps(g, PANZERS)).toBeUndefined();
  });
});

describe('what four independent reviews of the rule found (25 November 2026)', () => {
  it('a corps of another arm is never the row\'s own corps: its last division comes over, and the row\'s corps is of the row\'s arm', () => {
    const { g, told, laid } = balkans();
    const xv = corps(g, 'XV Corps')!, iv = corps(g, 'IV Corps')!;
    // IV Corps is the 4th Infantry Division alone, the division the 14th Panzer is formed out of.
    iv.divs = 1;
    iv.names = [['4th Infantry Division', '4th Infantry']];
    keepNumbers(iv, [4]);
    month(g, MARCH_41);
    month(g, APRIL_41);
    expect(told.find((t) => t.row === EIGHTH)!.claims.map((q) => [q.name, q.id, q.stays])).toEqual([['8th Panzer Division', xv.id, false], ['14th Panzer Division', iv.id, false]]);
    const l = laid.find((t) => t.row === EIGHTH)!;
    expect(l.took).toBe(false);
    expect(l.fresh).toBe(0);
    expect(l.after).toBeCloseTo(l.before, 9);
    const c = corps(g, EIGHTH)!;
    expect(c).not.toBe(iv);
    expect(c.kind).toBe('arm');
    expect(names(g, c)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(iv.dead).toBe(true);
    expect(holders(g, '4th Infantry Division')).toEqual([]);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a row that keeps a lent corps for good ends what the loan remembered: nothing of it goes back, and it is not ended with the loan', () => {
    const { g, laid } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    month(g, MARCH_41);
    expect(xxii.rowed).toBeDefined();
    expect(xxii.drew).toHaveLength(2);
    // (Pulled back from the Greek border to Varna, out of contact: a corps in a fight keeps its divisions, 18:47Z.)
    relocate(g, xxii, g.byName['Varna']);
    // A second row, not lent, names the same two divisions for the capital in April.
    const i = addRow([APRIL_41, 'GER', PANZERS, 'arm', 2, 'Berlin', {}]);
    month(g, APRIL_41);
    expect(laid.filter((t) => t.row === PANZERS).map((t) => [t.turn, t.corps, t.took, t.fresh])).toEqual([[MARCH_41, xxii.id, true, 0], [APRIL_41, xxii.id, true, 0]]);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(g.regions[xxii.reg].owner).toBe('GER');
    expect(g.regions[xxii.reg].theatre).toBe(-1);
    expect(xxii.rowed).toBeUndefined();
    expect(xxii.drew).toBeUndefined();
    expect(g.flags[`lent:${rowAt(PANZERS)}`]).toBe(-1);
    month(g, JUNE_41);
    expect(xxii.dead).toBe(false);
    expect(xxii.name).toBe(PANZERS);
    expect(names(g, xxii)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(heldTwice(g)).toEqual([]);
  });

  it('nothing goes back to a corps that is itself on its way out: the division stands on with the row\'s corps', () => {
    const { g } = balkans();
    const xv = corps(g, 'XV Corps')!;
    month(g, MARCH_41);
    month(g, APRIL_41);
    const c = corps(g, EIGHTH)!;
    relocate(g, xv, g.byName['Banat']);
    expect(rowOver(g, c, (x) => x === xv)).toBe(true);
    expect(names(g, xv)).toEqual(['7th Panzer Division']);
    expect(names(g, c)).toEqual(['8th Panzer Division', '14th Panzer Division']);
    expect(c.drew).toBeUndefined();
  });

  it('a row that adds divisions to a corps already landed is not read by the rule where that corps is gone: it is laid as it always was', () => {
    const { g, told, laid } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    const at = place(g, xxii);
    // (Its name lists the whole corps after the join, the old divisions first: the first of them is XXII Corps' own.)
    const NAME = '2nd, 9th and 16th Panzer Divisions';
    const i = addRow([MARCH_41 - 1, 'GER', NAME, 'arm', 1, 'Berlin', { joins: 'A corps that never landed' }]);
    month(g, MARCH_41 - 1);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(told.find((t) => t.row === NAME)).toBeUndefined();
    expect(laid.find((t) => t.row === NAME)).toBeUndefined();
    expect(corps(g, NAME)!.divs).toBe(1);
    expect(xxii.divs).toBe(2);
    expect(xxii.name).toBe('XXII Corps');
    expect(place(g, xxii)).toBe(at);
  });

  it('a row that adds divisions to a corps the rule laid finds it by the row\'s name, and the new division carries its own name', () => {
    const { g } = balkans();
    const iii = corps(g, 'III Corps')!;
    relocate(g, iii, g.byName['Sofia']);
    month(g, MARCH_41);
    // The 50th stayed with III Corps: the row's corps holds the 72nd and the 164th under the row's name.
    const c = corps(g, INFANTRY)!;
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    const NAME = '50th, 72nd, 164th and 199th Infantry Divisions';
    const i = addRow([APRIL_41, 'GER', NAME, 'inf', 1, 'Plovdiv', { joins: INFANTRY }]);
    month(g, APRIL_41);
    expect(g.flags[`sent:${i}`]).toBe(1);
    expect(c.name).toBe(NAME);
    expect(c.divs).toBe(3);
    // The one it brings is the one of its name that nobody holds: not the 50th, which III Corps has.
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division', '199th Infantry Division']);
    expect(holders(g, '50th Infantry Division')).toEqual([iii]);
    expect(heldTwice(g)).toEqual([]);
  });
});

describe('what a second review, of the mends themselves, found (25 November 2026)', () => {
  const left = (g: Game): string[] => g.headlines.filter((h) => /leave for other fronts/.test(h.text)).map((h) => h.text);

  it('a corps on a long passage lives and holds its divisions: the row raises nothing for one that is at sea', () => {
    const { g, told, laid } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    putToSea(g, xxii, g.byName['Hamburg'], 2);
    expect(g.corps.includes(xxii)).toBe(false);
    g.atSea![0].due = APRIL_41;
    month(g, MARCH_41);
    expect(told.find((t) => t.row === PANZERS)!.claims.map((q) => [q.name, q.id, q.stays])).toEqual([['2nd Panzer Division', xxii.id, true], ['9th Panzer Division', xxii.id, true]]);
    expect(laid.find((t) => t.row === PANZERS)).toBeUndefined();
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(g.flags[`sent:${rowAt(PANZERS)}`]).toBe(1);
    month(g, APRIL_41);
    // Landed: it is the one corps that holds them, as it stood, and nothing was raised under their names.
    expect(g.corps.includes(xxii)).toBe(true);
    expect(xxii.divs).toBe(2);
    expect(holders(g, '2nd Panzer Division')).toEqual([xxii]);
    expect(holders(g, '9th Panzer Division')).toEqual([xxii]);
    expect(laid.find((t) => t.row === PANZERS)).toBeUndefined();
    expect(heldTwice(g)).toEqual([]);
  });

  it('the player breaks a row\'s corps up and joins it again: it keeps the divisions it holds, not the ones its name lists', () => {
    const { g } = balkans('GER');
    month(g, MARCH_41);
    const c = corps(g, INFANTRY)!, iii = corps(g, 'III Corps')!;
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    setBreakUp(g, true);
    g.phase = g.order.indexOf('GER');
    const part = breakUp(g, c, [1])!;
    expect(part).not.toBeNull();
    expect(names(g, part)).toEqual(['164th Infantry Division']);
    expect(rejoin(g, part, c)).toBe(true);
    expect(c.divs).toBe(2);
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    expect(holders(g, '50th Infantry Division')).toEqual([iii]);
    expect(heldTwice(g)).toEqual([]);
    // (A corps whose name gives its divisions needs none written out once it is whole again, as before.)
    const xv = corps(g, 'XV Corps')!;
    const off = breakUp(g, xv, [1])!;
    expect(rejoin(g, off, xv)).toBe(true);
    expect(xv.names).toBeUndefined();
    expect(names(g, xv)).toEqual(['7th Panzer Division', '8th Panzer Division']);
  });

  it('a division that goes back to a corps which is itself on loan stays with that corps when its own loan ends, whichever row is read first', () => {
    for (const first of [true, false]) {
      const { g } = balkans();
      const xxii = corps(g, 'XXII Corps')!;
      // A lent row draws the 2nd Panzer out of XXII Corps for Sofia the month before the Balkans' row takes the corps itself.
      const A = [MARCH_41 - 1, 'GER', '2nd Panzer Division', 'arm', 1, 'Sofia', { hold: 1, until: first ? JUNE_41 - 1 : JUNE_41 }];
      const i = addRow(A);
      month(g, MARCH_41 - 1);
      const a = g.corps.find((c) => c.id === g.flags[`lent:${i}`])!;
      expect(names(g, a)).toEqual(['2nd Panzer Division']);
      expect(names(g, xxii)).toEqual(['9th Panzer Division']);
      const men = a.str + xxii.str;
      month(g, MARCH_41);
      // The 2nd stands at Sofia already and stays with its corps; XXII Corps comes itself with the 9th.
      expect(xxii.name).toBe(PANZERS);
      expect(names(g, xxii)).toEqual(['9th Panzer Division']);
      expect(a.dead).toBe(false);
      for (let t = APRIL_41; t <= JUNE_41; t++) month(g, t);
      expect(a.dead, `first ${first}`).toBe(true);
      expect(xxii.dead).toBe(false);
      expect(xxii.name).toBe('XXII Corps');
      expect(names(g, xxii).sort(), `first ${first}`).toEqual(['2nd Panzer Division', '9th Panzer Division']);
      expect(xxii.drew).toBeUndefined();
      expect(xxii.divs * xxii.str, `first ${first}`).toBeGreaterThan(men - 0.5);
      // Its men are with their corps: nothing of it left for other fronts, and no line says it did.
      expect(left(g).some((t) => /(: |; )2nd Panzer Division(;|\.)/.test(t)), `first ${first}`).toBe(false);
      expect(heldTwice(g)).toEqual([]);
      while (added > 0) { ROWS.pop(); added--; }
    }
  });

  it('a corps ends its loan once: a lent row that added to it has nothing left to end, and the corps that stood on stands', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    month(g, MARCH_41);
    const NAME = '2nd, 9th and 903rd Panzer Divisions';
    const i = addRow([APRIL_41, 'GER', NAME, 'arm', 1, 'Sofia', { joins: PANZERS, until: JUNE_41 + 2 }]);
    month(g, APRIL_41);
    expect(xxii.name).toBe(NAME);
    expect(names(g, xxii)).toEqual(['2nd Panzer Division', '9th Panzer Division', '903rd Panzer Division']);
    expect(g.flags[`lent:${i}`]).toBe(xxii.id);
    month(g, JUNE_41);
    // The first loan's end: its own two divisions stand on under its own name; the one the second row raised leaves.
    expect(xxii.name).toBe('XXII Corps');
    expect(names(g, xxii)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(g.flags[`lent:${i}`]).toBe(-1);
    month(g, JUNE_41 + 1);
    month(g, JUNE_41 + 2);
    expect(xxii.dead).toBe(false);
    expect(xxii.divs).toBe(2);
  });

  it('a second loan that takes a lent corps whole remembers the corps each division first came out of', () => {
    const { g } = balkans();
    const xv = corps(g, 'XV Corps')!;
    const home = xv.reg;
    // (Varna, behind the Greek border: a lent corps in contact would keep its division, the ruling of 18:47Z.)
    const iA = addRow([10, 'GER', '8th Panzer Division', 'arm', 1, 'Varna', { hold: 1, until: 40 }]);
    const iB = addRow([12, 'GER', '8th Panzer Division', 'arm', 1, g.regions[home].name, { until: 14 }]);
    month(g, 10);
    const c = g.corps.find((x) => x.id === g.flags[`lent:${iA}`])!;
    expect(names(g, c)).toEqual(['8th Panzer Division']);
    expect(names(g, xv)).toEqual(['7th Panzer Division']);
    month(g, 11);
    month(g, 12);
    // The second row is for XV Corps' own ground, where the division's corps came from: the lent corps comes itself.
    expect(g.flags[`lent:${iA}`]).toBe(-1);
    expect(g.flags[`lent:${iB}`]).toBe(c.id);
    expect(c.reg).toBe(home);
    expect(c.drew).toEqual([{ name: ['8th Panzer Division', '8th Panzer'], no: c.drew![0].no, from: xv.id }]);
    month(g, 13);
    month(g, 14);
    // Its time up, the 8th is XV Corps' again, and nothing left for other fronts.
    expect(names(g, xv).sort()).toEqual(['7th Panzer Division', '8th Panzer Division']);
    expect(c.dead).toBe(true);
    expect(left(g)).toEqual([]);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a row that adds to a corps takes no name another living corps holds, and a division its name has dropped keeps a name of its own', () => {
    const { g } = balkans();
    const iii = corps(g, 'III Corps')!;
    relocate(g, iii, g.byName['Sofia']);
    month(g, MARCH_41);
    const c = corps(g, INFANTRY)!;
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    // Its new name drops the 72nd and the 164th, and lists the 50th, which III Corps holds.
    const NAME = '50th, 902nd and 903rd Infantry Divisions';
    addRow([APRIL_41, 'GER', NAME, 'inf', 1, 'Plovdiv', { joins: INFANTRY }]);
    month(g, APRIL_41);
    expect(c.name).toBe(NAME);
    expect(names(g, c)).toEqual(['902nd Infantry Division', '164th Infantry Division', '903rd Infantry Division']);
    expect(holders(g, '50th Infantry Division')).toEqual([iii]);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a row that adds to a corps laid the old way writes its names out too: the one it brings is never one another corps holds', () => {
    const { g } = balkans();
    const iii = corps(g, 'III Corps')!;
    // A corps with no names written, read by its own name; the row that adds to it lists the 50th, III Corps' own.
    const one = addRow([MARCH_41 - 2, 'GER', '901st and 902nd Infantry Divisions', 'inf', 2, 'Berlin', {}]);
    month(g, MARCH_41 - 2);
    const c = corps(g, '901st and 902nd Infantry Divisions')!;
    expect(g.flags[`sent:${one}`]).toBe(1);
    expect(c.names).toBeUndefined();
    const NAME = '901st, 902nd and 50th Infantry Divisions';
    addRow([MARCH_41 - 1, 'GER', NAME, 'inf', 1, 'Berlin', { joins: '901st and 902nd Infantry Divisions' }]);
    month(g, MARCH_41 - 1);
    expect(c.name).toBe(NAME);
    expect(c.divs).toBe(3);
    expect(names(g, c).slice(0, 2)).toEqual(['901st Infantry Division', '902nd Infantry Division']);
    expect(names(g, c)[2]).not.toBe('50th Infantry Division');
    expect(holders(g, '50th Infantry Division')).toEqual([iii]);
    expect(heldTwice(g)).toEqual([]);
  });

  it('laid by itself, a row that adds to a corps reads the divisions it brings, not the gone corps\' own that another corps holds', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    const NAME = '2nd, 9th and 16th Panzer Divisions';
    addRow([MARCH_41 - 1, 'GER', NAME, 'arm', 1, 'Berlin', { joins: 'A corps that never landed' }]);
    month(g, MARCH_41 - 1);
    expect(names(g, corps(g, NAME)!)).toEqual(['16th Panzer Division']);
    expect(names(g, xxii)).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(heldTwice(g)).toEqual([]);
  });

  it('two divisions formed out of others in one row never ask the same corps for the same place', () => {
    const { g, told, laid } = balkans();
    const NAME = '901st and 902nd Panzer Divisions';
    addRow([MARCH_41 - 1, 'GER', NAME, 'arm', 2, 'Berlin', { was: { '901st Panzer Division': '801st Infantry Division', '902nd Panzer Division': '802nd Infantry Division' } }]);
    month(g, MARCH_41 - 1);
    const t = told.find((x) => x.row === NAME)!;
    expect(t.claims).toHaveLength(2);
    expect(t.claims[0].id).not.toBeNull();
    expect(t.claims[1].id).not.toBeNull();
    expect(t.claims[0].id).not.toBe(t.claims[1].id);
    const l = laid.find((x) => x.row === NAME)!;
    expect(l.fresh).toBe(0);
    expect(l.after).toBeCloseTo(l.before, 9);
    expect(names(g, corps(g, NAME)!)).toEqual(['901st Panzer Division', '902nd Panzer Division']);
  });
});

describe('what a third review, of the second review\'s mends, found (25 November 2026)', () => {
  const left = (g: Game): string[] => g.headlines.filter((h) => /leave for other fronts/.test(h.text)).map((h) => h.text);
  /** Two places at home on the main front, neither where this corps stands. */
  const elsewhere = (g: Game, c: Corps): string[] => ['Berlin', 'Hamburg', 'Munich', 'Leipzig'].filter((n) => g.byName[n] && g.byName[n].id !== c.reg && g.byName[n].ctrl === 'GER');

  it('a division a lent corps drew goes back to the corps it first came out of, though another loan had it in between', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    const [one, two] = elsewhere(g, xxii);
    const iA = addRow([10, 'GER', '2nd and 901st Panzer Divisions', 'arm', 2, one, { until: 16 }]);
    const iC = addRow([11, 'GER', '2nd Panzer Division', 'arm', 1, two, { until: 13 }]);
    month(g, 10);
    const p = g.corps.find((x) => x.id === g.flags[`lent:${iA}`])!;
    expect(names(g, p)).toEqual(['2nd Panzer Division', '901st Panzer Division']);
    expect(p.drew!.map((r) => [r.name[0], r.from])).toEqual([['2nd Panzer Division', xxii.id]]);
    month(g, 11);
    const c = g.corps.find((x) => x.id === g.flags[`lent:${iC}`])!;
    expect(c).not.toBe(p);
    expect(names(g, c)).toEqual(['2nd Panzer Division']);
    expect(names(g, p)).toEqual(['901st Panzer Division']);
    // (Each loan ends where the corps it owes stands with it: a return never moves a division for free, 19:17Z.)
    relocate(g, c, g.regions[p.reg]);
    month(g, 12);
    month(g, 13);
    // The second loan's time is up: the 2nd is back with the corps it was drawn from, which owes it to XXII Corps still.
    expect(c.dead).toBe(true);
    expect(names(g, p).sort()).toEqual(['2nd Panzer Division', '901st Panzer Division']);
    expect(p.drew!.find((r) => r.name[0] === '2nd Panzer Division')!.from).toBe(xxii.id);
    relocate(g, p, g.regions[xxii.reg]);
    for (let t = 14; t <= 16; t++) month(g, t);
    // And the first loan's: XXII Corps has its division again; only the one the row raised new leaves.
    expect(names(g, xxii).sort()).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    expect(p.dead).toBe(true);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a lent corps folded into a cadre has ended its loan: raised again, it is not sent off when the row\'s time is up', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!, xv = corps(g, 'XV Corps')!;
    const [one] = elsewhere(g, xxii);
    const iA = addRow([10, 'GER', '2nd Panzer Division', 'arm', 1, one, { until: 14 }]);
    month(g, 10);
    const a = g.corps.find((x) => x.id === g.flags[`lent:${iA}`])!;
    expect(a.drew).toBeDefined();
    // Both worn, standing together: the lent corps is folded into XV Corps and goes home as a cadre.
    relocate(g, xv, g.regions[a.reg]);
    a.str = 0.3; xv.str = 0.3; a.supplied = true; xv.supplied = true;
    expect(foldCorps(g, xv, a)).toBe(true);
    expect(a.dead).toBe(true);
    expect(a.drew).toBeUndefined();
    expect(g.flags[`lent:${iA}`]).toBe(-1);
    // Raised again from the arsenal (economy.ts, reform: the same corps, with its number), it is its own corps.
    const cadres = g.nations.GER.cadres!;
    cadres.splice(cadres.indexOf(a), 1);
    a.dead = false; a.str = 1;
    g.corps.push(a);
    g.regions[a.reg].units.push(a);
    for (let t = 11; t <= 15; t++) month(g, t);
    expect(a.dead).toBe(false);
    expect(g.corps.includes(a)).toBe(true);
    expect(left(g)).toEqual([]);
  });

  it('a lent corps that a later row added a division to leaves with a line when its time is up: what it drew goes back, the new one goes', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    const [one] = elsewhere(g, xxii);
    const iA = addRow([10, 'GER', '2nd Panzer Division', 'arm', 1, one, { until: 14 }]);
    month(g, 10);
    const a = g.corps.find((x) => x.id === g.flags[`lent:${iA}`])!;
    // The row that adds to it lists only names living corps hold: its new division reads by its number, and no name is written for it.
    const NAME = '9th and 2nd Panzer Divisions';
    addRow([11, 'GER', NAME, 'arm', 1, one, { joins: '2nd Panzer Division' }]);
    month(g, 11);
    expect(a.name).toBe(NAME);
    expect(a.divs).toBe(2);
    expect(a.names!.length).toBe(1);
    expect(names(g, a)[0]).toBe('2nd Panzer Division');
    expect(names(g, a)[1]).not.toBe('9th Panzer Division');
    // (It ends its loan beside XXII Corps: a return never moves a division for free, 19:17Z.)
    relocate(g, a, g.regions[xxii.reg]);
    for (let t = 12; t <= 14; t++) month(g, t);
    expect(a.dead).toBe(true);
    expect(names(g, xxii).sort()).toEqual(['2nd Panzer Division', '9th Panzer Division']);
    // It held a division that was nobody's to take back: it left for other fronts, and the line says so.
    expect(left(g).some((t) => t.includes(NAME))).toBe(true);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a share another row took of a row is taken once from a corps that keeps two of its divisions', () => {
    const { g } = balkans();
    const xxii = corps(g, 'XXII Corps')!;
    const here = place(g, xxii);
    // Both of the row's divisions stand where the row is for: they stay, and the row lays nothing.
    const iR = addRow([10, 'GER', '9th and 2nd Panzer Divisions', 'arm', 2, here, {}]);
    // (A second row of the month that names one of them: its watch reads the corps as the first row left it, before the month's drafts.)
    addRow([10, 'GER', '2nd Panzer Division', 'arm', 1, here, {}]);
    g.flags[`takecut:${iR}`] = 0.5;
    const seen: [string, number][] = [];
    drawnWatch.tell = (e) => seen.push([e.row, xxii.str]);
    const was = xxii.str;
    month(g, 10);
    expect(seen.map((x) => x[0])).toEqual(['9th and 2nd Panzer Divisions', '2nd Panzer Division']);
    expect(seen[0][1]).toBe(was);
    // Half of each of its two divisions: half of the corps, not three quarters of three quarters of it.
    expect(seen[1][1]).toBeCloseTo(was * 0.5, 9);
    expect(g.flags[`takecut:${iR}`]).toBeUndefined();
    expect(xxii.divs).toBe(2);
  });

  it('a row that names a division and forms another out of it asks for its place once: the named one comes, the other is found elsewhere', () => {
    const { g, told, laid } = balkans();
    const iv = holders(g, '4th Infantry Division')[0];
    const [one] = elsewhere(g, iv);
    const NAME = '901st and 4th Infantry Divisions';
    addRow([10, 'GER', NAME, 'inf', 2, one, { was: { '901st Infantry Division': '4th Infantry Division' } }]);
    month(g, 10);
    const t = told.find((x) => x.row === NAME)!;
    expect(t.claims.map((q) => q.name)).toEqual(['901st Infantry Division', '4th Infantry Division']);
    expect(t.claims[1].id).toBe(iv.id);
    expect(t.claims[1].conv).toBeNull();
    expect(t.claims[0].conv).toBe('4th Infantry Division');
    expect(t.claims[0].id).not.toBe(iv.id);
    expect(t.claims[0].id).not.toBeNull();
    const l = laid.find((x) => x.row === NAME)!;
    expect(l.fresh).toBe(0);
    expect(l.after).toBeCloseTo(l.before, 9);
    expect(names(g, corps(g, NAME)!)).toEqual(['901st Infantry Division', '4th Infantry Division']);
    expect(holders(g, '4th Infantry Division')).toHaveLength(1);
    expect(heldTwice(g)).toEqual([]);
  });

  it('a corps that grows counts on from its nation\'s numbers: the places it has yet to fill hold nobody\'s number', () => {
    const g = startOpen(createGame(1, null));
    const key = 'dn:GER:inf';
    // A corps numbered last carries its arm's highest numbers, running on one from the next.
    let c: Corps | undefined;
    for (const x of g.corps.filter((u) => u.nat === 'GER' && u.kind === 'inf' && !u.dead && u.dn !== undefined && !u.names)) {
      delete x.dn; delete x.nos;
      numberCorps(g, x);
      if (x.nos === undefined && x.dn! + x.divs - 1 === g.flags[key]) { c = x; break; }
    }
    expect(c).toBeDefined();
    const top = g.flags[key];
    const want: number[] = [];
    for (let n = top + 1; want.length < 2; n++) if (!numberSpokenFor(g, 'GER', 'inf', n)) want.push(n);
    const had = divisionsOf(g, c!).map((d) => d.no);
    growCorps(g, c!, 2, 'inf');
    expect(divisionsOf(g, c!).map((d) => d.no)).toEqual([...had, ...want]);
    expect(g.flags[key]).toBe(want[1]);
  });
});

describe('the player\'s seat (her ruling 4 of 14:28Z): one rule with every seat, and his divisions are never moved for him', () => {
  it('his corps stay as they stand, the rows form only their new divisions, and each row\'s line tells him once', () => {
    const { g, laid } = balkans('GER');
    const xxii = corps(g, 'XXII Corps')!, iii = corps(g, 'III Corps')!, xv = corps(g, 'XV Corps')!, iv = corps(g, 'IV Corps')!;
    const was = [xxii, iii, xv, iv].map((c) => ({ name: c.name, reg: c.reg, divs: c.divs, str: c.str, hold: c.hold, names: names(g, c) }));
    month(g, MARCH_41);
    month(g, APRIL_41);
    expect([xxii, iii, xv, iv].map((c) => ({ name: c.name, reg: c.reg, divs: c.divs, str: c.str, hold: c.hold, names: names(g, c) }))).toEqual(was);
    // All of one row's divisions are his already: nothing is formed, the row has landed and waits as an offer.
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(corps(g, EIGHTH)).toBeUndefined();
    expect(holders(g, '14th Panzer Division')).toEqual([]);
    expect(laid.map((l) => l.row)).toEqual([INFANTRY]);
    for (const [row, turn] of [[PANZERS, MARCH_41], [INFANTRY, MARCH_41], [EIGHTH, APRIL_41]] as [string, number][]) {
      expect(g.flags[`offer:${rowAt(row)}`], row).toBe(turn);
      expect(g.flags[`sent:${rowAt(row)}`], row).toBe(1);
    }
    // The new divisions form where the row puts them, in a corps under the row's name that holds the two of them.
    const c = corps(g, INFANTRY)!;
    expect(place(g, c)).toBe('Plovdiv');
    expect(c.divs).toBe(2);
    expect(names(g, c)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    // The lines, in the file's words.
    const lines = g.headlines.filter((h) => h.nat === 'GER' && / (is|are) due in | was due to be formed in /.test(h.text));
    expect(lines).toHaveLength(3);
    expect(lines[0].text).toMatch(said(DRAWN.many));
    // (The UI director's marks of 17:26Z: a place can be a region, so "in"; numbers of one arm are joined as a row writes them.)
    expect(lines[0].text).toContain('The 2nd and 9th Panzer Divisions are due in Sofia this month, but you hold them already, in ');
    expect(lines[1].text).toMatch(said(DRAWN.one));
    expect(lines[1].text).toContain('The 50th Infantry Division is due in Plovdiv this month, but you hold it already, in ');
    expect(lines[2].text).toContain('The 8th Panzer Division is due in Transdanubia');
    expect(lines[2].text).toContain('The 14th Panzer Division was due to be formed in Transdanubia this month out of your 4th Infantry Division, in ');
    // (Every place a line names is filled, where it names one twice: the UI director's words of 16:48Z.)
    for (const l of lines) expect(l.text).not.toMatch(/\{\w+\}/);
    expect(lines[0].text).toContain('they go to Sofia only if you send them');
    expect(lines[2].text).toContain('your 4th Infantry Division stays as it is');
    expect(lines[2].text).toMatch(said(DRAWN.conv));
    // And nothing is told again in a later month.
    month(g, APRIL_41 + 1);
    expect(g.headlines.filter((h) => / (is|are) due in | was due to be formed in /.test(h.text))).toHaveLength(3);
    expect(heldTwice(g)).toEqual([]);
  });

  it('where his divisions stand at the row\'s own place already, the line says the place once', () => {
    const { g } = balkans('GER');
    relocate(g, corps(g, 'XXII Corps')!, g.byName['Sofia']);
    month(g, MARCH_41);
    const line = g.headlines.find((h) => h.nat === 'GER' && / are due in Sofia /.test(h.text))!;
    expect(line.text).toMatch(said(DRAWN.hereMany));
    expect(line.text).toBe('The 2nd and 9th Panzer Divisions are due in Sofia this month, and they are there already with your own forces. No division is raised twice.');
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(heldTwice(g)).toEqual([]);
  });

  it('a division of his whose corps has died before the row\'s month is laid whole, as for every seat', () => {
    const { g } = balkans('GER');
    removeCorps(g, corps(g, 'XXII Corps')!);
    month(g, MARCH_41);
    expect(corps(g, PANZERS)!.divs).toBe(2);
    expect(g.flags[`offer:${rowAt(PANZERS)}`]).toBeUndefined();
  });

  it('nothing is taken out of his home army to form a division for him', () => {
    const { g, told, laid } = balkans('GER');
    removeCorps(g, corps(g, 'IV Corps')!);
    month(g, MARCH_41);
    month(g, APRIL_41);
    expect(told.find((t) => t.row === EIGHTH)!.claims[1]).toMatchObject({ name: '14th Panzer Division', id: null, stays: true });
    expect(laid.find((t) => t.row === EIGHTH)).toBeUndefined();
    expect(holders(g, '14th Panzer Division')).toEqual([]);
    expect(corps(g, EIGHTH)).toBeUndefined();
  });
});

describe('outside the grand campaign nothing is read', () => {
  it('a history scenario keeps the order of battle it was measured with: the row is laid beside the corps', () => {
    const { g, told, laid } = balkans(null, true, false);
    expect(g.flags.open).not.toBe(1);
    const xxii = corps(g, 'XXII Corps')!;
    const at = place(g, xxii);
    month(g, MARCH_41);
    expect(told).toEqual([]);
    expect(laid).toEqual([]);
    expect(xxii.name).toBe('XXII Corps');
    expect(place(g, xxii)).toBe(at);
    const c = corps(g, PANZERS)!;
    expect(c).not.toBe(xxii);
    expect(c.divs).toBe(2);
    expect(corps(g, INFANTRY)!.divs).toBe(3);
    expect(corps(g, 'III Corps')!.divs).toBe(3);
  });
});

describe('the names a corps takes from the divisions it holds', () => {
  it('reads one division by its own name and several as a dated row writes them', () => {
    const d = (...xs: string[]): [string, string][] => xs.map((x) => [x, x.replace(/ Division$/, '')]);
    expect(formationName(d('50th Division'))).toBe('50th Division');
    expect(formationName(d('72nd Infantry Division', '164th Infantry Division'))).toBe('72nd and 164th Infantry Divisions');
    expect(formationName(d('50th Infantry Division', '72nd Infantry Division', '164th Infantry Division'))).toBe('50th, 72nd and 164th Infantry Divisions');
    expect(formationName(d('New Zealand Division', '9th Australian Division'))).toBe('New Zealand and 9th Australian Divisions');
    expect(formationName(d('8th Panzer Division', '14th Panzer Division'))).toBe('8th and 14th Panzer Divisions');
    expect(formationName(d('8th Indian Brigade'))).toBeNull();
    expect(formationName([])).toBeNull();
  });
});

describe("the dial: a date never lifts a corps out of a fight at stops 3 to 5; at the book's stops the book's rows keep their book", () => {
  // The design director's ruling of 21:50Z, 25 November 2026: her 21:25Z scope for "a date never lifts a corps out of
  // a fight" reaches the draw's door too. At stops 1 and 2 a row that draws a held division takes it at its date, even
  // from a fight, and it is still laid once. Stops 3 to 5 are unchanged.
  function fight(stop: number): ReturnType<typeof balkans> & { iii: Corps; at: string } {
    const b = balkans();
    expect(setStop(b.g, stop, true)).toBe(true);
    const iii = corps(b.g, 'III Corps')!;
    const at = place(b.g, iii);
    declareWar(b.g, 'GER', 'POL');
    expect(corpsInContact(b.g, 'GER', iii)).toBe(true);
    month(b.g, MARCH_41);
    return { ...b, iii, at };
  }
  it.each([1, 2])('at stop %i the row draws the 50th out of III Corps in its month, fight or not, and it is laid once', (stop) => {
    const { g, told, iii } = fight(stop);
    expect(told.find((t) => t.row === INFANTRY)!.claims).toEqual([expect.objectContaining({ name: '50th Infantry Division', from: 'III Corps', stays: false })]);
    expect(iii.divs).toBe(2);
    expect(names(g, corps(g, INFANTRY)!)).toContain('50th Infantry Division');
    expect(heldTwice(g)).toEqual([]);
  });
  it.each([4, 5])('at stop %i it acts as at stop 3: the 50th stays and fights where it is, and the row forms without it', (stop) => {
    const { g, told, iii, at } = fight(stop);
    expect(told.find((t) => t.row === INFANTRY)!.claims).toEqual([expect.objectContaining({ name: '50th Infantry Division', from: 'III Corps', stays: true })]);
    expect(iii.divs).toBe(3);
    expect(place(g, iii)).toBe(at);
    expect(names(g, corps(g, INFANTRY)!)).toEqual(['72nd Infantry Division', '164th Infantry Division']);
    expect(heldTwice(g)).toEqual([]);
  });
});

describe('his corps on a passage at sea: the line says where it is bound, never the port it sailed from (the UI director, 21:52Z)', () => {
  it('bound elsewhere: he holds them already, at sea, bound for the harbour, and they go to the row\'s place only if he sends them', () => {
    const { g } = balkans('GER');
    const xxii = corps(g, 'XXII Corps')!;
    const from = place(g, xxii);
    g.turn = MARCH_41 - 1;
    putToSea(g, xxii, g.byName['Hamburg'], 2);
    month(g, MARCH_41);
    const line = g.headlines.find((h) => h.nat === 'GER' && / are due in Sofia /.test(h.text))!;
    expect(line.text).toMatch(said(DRAWN.many));
    expect(line.text).toBe('The 2nd and 9th Panzer Divisions are due in Sofia this month, but you hold them already, at sea, bound for Hamburg. No division is raised twice: they go to Sofia only if you send them.');
    expect(line.text).not.toContain(from);
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(heldTwice(g)).toEqual([]);
  });
  it('bound for the row\'s own place: they are on their way there already, at sea, and nothing is promised about sending', () => {
    const { g } = balkans('GER');
    const xxii = corps(g, 'XXII Corps')!;
    g.turn = MARCH_41 - 1;
    putToSea(g, xxii, g.byName['Sofia'], 2);
    month(g, MARCH_41);
    const line = g.headlines.find((h) => h.nat === 'GER' && / are due in Sofia /.test(h.text))!;
    expect(line.text).toMatch(said(DRAWN.hereSeaMany));
    expect(line.text).toBe('The 2nd and 9th Panzer Divisions are due in Sofia this month, and they are on their way there already with your own forces, at sea. No division is raised twice.');
    expect(corps(g, PANZERS)).toBeUndefined();
    expect(heldTwice(g)).toEqual([]);
  });
  it('ashore the words are as they were: in the place where his corps stands', () => {
    expect(DRAWN.ashore).toBe('in {where}');
    expect(DRAWN.one.replace('{at}', DRAWN.ashore)).toBe('{names} is due in {place} this month, but you hold it already, in {where}. No division is raised twice: it goes to {place} only if you send it.');
  });
});
