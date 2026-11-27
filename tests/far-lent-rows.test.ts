// A row only lent to a campaign leaves when the place it was lent for is decided (the design director's rulings of
// 22:55Z, 21 November, and 03:49Z, 22 November 2026; src/campaign/game.ts, lentOver).
//
// The far side's lent rows left by date alone: Japan's heavy guns in June 1942, her three tank regiments in October,
// China's three armies in Burma in June, wherever the fight they were lent for stood. At the stops that keep the book
// that is the record and it stands. At the stops that keep none the date is not the war's: Singapore may fall in a
// month or hold a year.
//
// The form, far side behind the switch, stops 3 to 5, no number. A row lent to take a place (`takes`) leaves when
// that place is its side's, or when the front of its staff's plan that lists it has ended with the front's own place
// untaken; while the front stands, and after it has taken its place with the row's own still to take, the row stays
// past the book's month. Where no plan's front lists the row the place taken is all that decides it. A row lent to
// hold places (`holds`) leaves when the last of them is lost. The places are the director's list. The states are set by hand on
// a made-up December 1941 (far-attached.test.ts); only the opening of a country's month is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { bookMonth } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, capitulate, createGame, declareWar, hostileUnits, removeCorps, startPhase } from '../src/campaign/game';
import setup from '../data/campaign/setup-1939.json';
import { setStop } from '../src/campaign/history';
import { navalUpkeep } from '../src/campaign/navy';
import { frontOfRow } from '../src/campaign/planfronts';
import { ops, startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const corps = (g: Game, name: string, nat = 'JAP'): Corps | undefined => g.corps.find((c) => c.nat === nat && c.name === name && !c.dead);
const reg = (n: string): string => `${n} Tank Regiment`;
const GUNS23 = '23rd Army artillery (1st Artillery Group)', GUNS14 = '14th Army artillery (1st and 8th Field Heavy Artillery)';
const CHINESE = ['Chinese 6th Army (49th, 55th and 93rd Divisions)', 'Chinese 5th Army (200th, 22nd and 96th Divisions)', 'Chinese 66th Army (38th, 28th and 29th Divisions)'];
const LENT = [reg('1st'), reg('6th'), reg('7th'), GUNS23, GUNS14];
const here = (g: Game, names: string[], nat = 'JAP'): boolean[] => names.map((nm) => !!corps(g, nm, nat));

/** A made-up December 1941 (far-attached.test.ts): Japan's dated rows taken in, her lent rows on their quays. */
const december = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(17, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 9;
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 23 || t === 24 || t === 27) startPhase(g, 'JAP'); }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  navalUpkeep(g, 'USA');
  return g;
};
/** The same with the strike south gone in December 1941: the plan's fronts are read from here on. */
const struck = (stop: number, human: string | null = null): Game => {
  const g = december(stop, human);
  for (const e of ['USA', 'UK', 'NED', 'AUS', 'NZL']) if (!atWar(g, 'JAP', e)) declareWar(g, 'JAP', e);
  g.flags['st:done:JAP:south'] = 28;
  return g;
};
const month = (g: Game, turn: number, nat = 'JAP'): void => { g.turn = turn; startPhase(g, nat); };
/** A place in a country's hands, by hand: whoever of its enemies stood there is gone, as after a fight lost. */
const take = (g: Game, place: string, nat = 'JAP'): void => {
  const r = g.byName[place];
  for (const c of hostileUnits(g, r, nat)) removeCorps(g, c);
  r.ctrl = nat;
};
/** A place lost with its defenders fallen back to another, by hand. */
const fallBack = (g: Game, place: string, to: string, by = 'JAP'): void => {
  const r = g.byName[place];
  for (const c of hostileUnits(g, r, by)) c.reg = g.byName[to].id;
  r.ctrl = by;
};
const left = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn && h.text.includes('lent to the campaign')).map((h) => h.text);

describe('a lent row leaves when the place it was lent for is decided (the director\'s rulings of 22:55Z, 21 November, and 03:49Z, 22 November 2026)', () => {
  it('the rows carry their places: the director\'s list', () => {
    const g = struck(3);
    expect(here(g, LENT)).toEqual([true, true, true, true, true]);
    // Each is on its own front's list: Singapore's, Luzon's (whose own place is Manila) and Hong Kong's.
    for (const nm of LENT) expect(frontOfRow(g, 'JAP', nm), nm).toEqual({ live: true, taken: false });
    expect(frontOfRow(g, 'JAP', '14th Army (48th Division)')).toEqual({ live: true, taken: false });
    expect(frontOfRow(g, 'JAP', 'Kwantung Army')).toBeNull();
  });

  it('at the stops that keep the book every row leaves in the book\'s month, whatever has fallen', () => {
    for (const stop of [1, 2]) {
      const g = struck(stop);
      // Singapore and Hong Kong fall early: nobody leaves for it.
      take(g, 'Singapore'); take(g, 'Hong Kong');
      month(g, 31);
      expect(here(g, LENT), `stop ${stop}`).toEqual([true, true, true, true, true]);
      // The record's months (the director's ruling of 04:01Z, 22 November 2026): the 6th Tank Regiment home in
      // April 1942, the 7th in May with Bataan still America's, the guns in June, the 1st Tank Regiment in July.
      month(g, 32);
      expect(here(g, LENT), `stop ${stop}`).toEqual([true, false, true, true, true]);
      month(g, 33);
      expect(here(g, LENT), `stop ${stop}`).toEqual([true, false, false, true, true]);
      expect(g.byName['Bataan'].ctrl).toBe('USA');
      month(g, 34);
      expect(here(g, LENT), `stop ${stop}`).toEqual([true, false, false, false, false]);
      month(g, 35);
      expect(here(g, LENT), `stop ${stop}`).toEqual([false, false, false, false, false]);
    }
  });

  it('where no book is kept a row lent to take a place stays while its front stands, past the book\'s month', () => {
    const g = struck(3);
    month(g, 34);
    expect(here(g, LENT)).toEqual([true, true, true, true, true]);
    month(g, 38);
    expect(here(g, LENT)).toEqual([true, true, true, true, true]);
    month(g, 45);
    expect(here(g, LENT)).toEqual([true, true, true, true, true]);
    expect(left(g, 34).concat(left(g, 38), left(g, 45))).toEqual([]);
  });

  it('and leaves the month its place is its side\'s: Hong Kong\'s guns in January 1942, Singapore\'s regiments in March', () => {
    const g = struck(3);
    take(g, 'Hong Kong');
    month(g, 29);
    expect(here(g, LENT)).toEqual([true, true, true, false, true]);
    expect(left(g, 29).length).toBe(1);
    expect(left(g, 29)[0]).toContain(GUNS23);
    take(g, 'Singapore');
    month(g, 31);
    expect(here(g, LENT)).toEqual([false, false, true, false, true]);
    expect(left(g, 31)[0]).toContain(`${reg('1st')}; ${reg('6th')}`);
  });

  it('the rows lent for Bataan stay when Manila falls and the Luzon front is over, and leave when Bataan is taken', () => {
    const g = struck(3);
    take(g, 'Manila');
    month(g, 30);
    // The front has taken its own place; the rows' own is still to take.
    expect(frontOfRow(g, 'JAP', reg('7th'))).toEqual({ live: false, taken: true });
    expect(here(g, [reg('7th'), GUNS14])).toEqual([true, true]);
    month(g, 38);
    expect(here(g, [reg('7th'), GUNS14])).toEqual([true, true]);
    take(g, 'Bataan');
    month(g, 39);
    expect(here(g, [reg('7th'), GUNS14])).toEqual([false, false]);
  });

  it('a front that has ended with its place untaken lets its lent rows go', () => {
    const g = struck(3);
    month(g, 30);
    // The Luzon front over, by hand, with Manila America's still: the campaign it was lent to is given up.
    const k = ops('JAP').find((op) => op.id === 'south')!.fronts!.findIndex((F) => F.id === 'luzon');
    g.flags[`st:frO:JAP:${k}`] = 30;
    expect(g.byName['Manila'].ctrl).toBe('USA');
    expect(frontOfRow(g, 'JAP', reg('7th'))).toEqual({ live: false, taken: false });
    month(g, 31);
    expect(here(g, [reg('7th'), GUNS14, reg('1st')])).toEqual([false, false, true]);
  });

  it('where no front of a plan lists the row the place taken is all that decides it: the strike south never ordered, and the player\'s own Japan', () => {
    // No plan taken up: nothing ends by date alone (the director's ruling of 04:26Z, 22 November 2026). The rows stay
    // past the book's months while their places stand, and each leaves when its place is Japan's.
    const g = december(3);
    expect(frontOfRow(g, 'JAP', reg('1st'))).toBeNull();
    for (const t of [33, 34, 35, 38, 45]) { month(g, t); expect(here(g, LENT), `month ${t}`).toEqual([true, true, true, true, true]); }
    take(g, 'Hong Kong');
    month(g, 46);
    expect(here(g, LENT)).toEqual([true, true, true, false, true]);
    // The player's Japan has the computer's rule: no staff's plan is read for him, and a place taken sends its rows home.
    const p = struck(3, 'JAP');
    expect(frontOfRow(p, 'JAP', reg('1st'))).toBeNull();
    take(p, 'Singapore');
    month(p, 31);
    expect(here(p, LENT)).toEqual([false, false, true, true, true]);
    for (const t of [34, 38]) { month(p, t); expect(here(p, LENT), `month ${t}`).toEqual([false, false, true, true, true]); }
    take(p, 'Bataan');
    month(p, 39);
    expect(here(p, LENT)).toEqual([false, false, false, true, false]);
    // By the book his rows keep the book's months, as the computer's do.
    const b = struck(1, 'JAP');
    month(b, 35);
    expect(here(b, LENT)).toEqual([false, false, false, false, false]);
  });

  it('China\'s armies in Burma stay while Mandalay or Lashio is held, and leave when both are lost', () => {
    const g = struck(3);
    for (const t of [30, 31, 32]) month(g, t, 'UK');
    expect(here(g, CHINESE, 'UK')).toEqual([true, true, true]);
    // June 1942, the book's month, and after: Burma holds, and they stay.
    month(g, 34, 'UK');
    month(g, 40, 'UK');
    expect(here(g, CHINESE, 'UK')).toEqual([true, true, true]);
    // Lashio lost, its armies fallen back on Mandalay: Mandalay is held still.
    fallBack(g, 'Lashio', 'Mandalay');
    month(g, 41, 'UK');
    expect(here(g, CHINESE, 'UK')).toEqual([true, true, true]);
    // Mandalay lost, the armies out to Myitkyina: both are gone, and so are they, in Britain's next month.
    fallBack(g, 'Mandalay', 'Myitkyina');
    expect(here(g, CHINESE, 'UK')).toEqual([true, true, true]);
    month(g, 42, 'UK');
    expect(here(g, CHINESE, 'UK')).toEqual([false, false, false]);
    expect(left(g, 42).length).toBe(1);
    for (const nm of CHINESE) expect(left(g, 42)[0]).toContain(nm);
  });

  it('and by the book they leave in June 1942 with Burma held', () => {
    const g = struck(1);
    for (const t of [30, 31, 32]) month(g, t, 'UK');
    expect(here(g, CHINESE, 'UK')).toEqual([true, true, true]);
    expect([g.byName['Mandalay'].ctrl, g.byName['Lashio'].ctrl]).toEqual(['UK', 'UK']);
    month(g, 34, 'UK');
    expect(here(g, CHINESE, 'UK')).toEqual([false, false, false]);
  });
});

describe('a tank division keeps a share for each regiment lent elsewhere (the director\'s rulings of 03:49Z and 04:01Z, 22 November 2026)', () => {
  const D1 = '1st Tank Division', D2 = '2nd Tank Division';
  /** A regiment's worth as it stands: its strength by its divisions. */
  const worth = (g: Game, name: string): number => { const c = corps(g, name)!; return c.str * c.divs; };
  const full = (g: Game, name: string): number => corps(g, name)!.full ?? 1;

  it('the rows name all four regiments of each division, and the lent ones carry the record\'s months', () => {
    const rows = (name: string): any => ((setup as any).expeditions as any[]).find((r) => r[1] === 'JAP' && r[2] === name);
    expect([rows(D1)[0], rows(D1)[6].str, rows(D1)[6].est, rows(D1)[6].from, rows(D1)[6].awaits]).toEqual([34, 0.45, 0.8, [reg('3rd'), reg('5th'), reg('9th')], [reg('1st')]]);
    expect([rows(D2)[0], rows(D2)[6].str, rows(D2)[6].est, rows(D2)[6].from, rows(D2)[6].awaits]).toEqual([34, 0.5, 0.8, [reg('10th'), reg('11th')], [reg('6th'), reg('7th')]]);
    expect(['1st', '6th', '7th'].map((n) => [rows(reg(n))[6].until, rows(reg(n))[6].est])).toEqual([[35, 0.25], [32, 0.25], [33, 0.25]]);
  });

  it('by the book: the 2nd forms with all four at 0.5, the 1st at 0.45 a regiment short, and takes its 1st Regiment in July 1942', () => {
    const g = struck(1);
    for (const t of [32, 33]) month(g, t);
    expect(here(g, [reg('1st'), reg('6th'), reg('7th'), D1, D2])).toEqual([true, false, false, false, false]);
    month(g, 34);
    // The 2nd: the 6th and 7th are home by the book, and its figure counts them. Nothing is held.
    expect(full(g, D2)).toBe(0.8);
    expect(corps(g, D2)!.str).toBeGreaterThanOrEqual(0.5);
    // The 1st: its own regiment is lent still, and a quarter of a division's armour is kept for it.
    expect(full(g, D1)).toBeCloseTo(0.55, 9);
    expect(corps(g, D1)!.str).toBeGreaterThanOrEqual(0.45);
    expect(corps(g, D1)!.str).toBeLessThanOrEqual(0.55 + 1e-9);
    // July 1942: the regiment comes home at the worth it stands at, and the share is the division's again.
    const before = corps(g, D1)!.str, w = worth(g, reg('1st'));
    expect(w).toBeCloseTo(0.25, 9);
    month(g, 35);
    expect(corps(g, reg('1st'))).toBeUndefined();
    expect(full(g, D1)).toBe(0.8);
    expect(corps(g, D1)!.str).toBeGreaterThanOrEqual(before + w - 1e-9);
    expect(corps(g, D1)!.str).toBeLessThanOrEqual(0.8 + 1e-9);
  });

  it('where no book is kept the divisions form in their month all the same, short of the regiments still lent, and the drafts do not fill the shares', () => {
    const g = struck(3);
    month(g, 34);
    expect(here(g, [reg('1st'), reg('6th'), reg('7th'), D1, D2])).toEqual([true, true, true, true, true]);
    // The 1st: 0.45 and a share held. The 2nd: its figure counts four, and two are not there: half of 0.5, two shares held.
    expect([full(g, D1), full(g, D2)].map((x) => +x.toFixed(9))).toEqual([0.55, 0.3]);
    expect(corps(g, D1)!.str).toBeGreaterThanOrEqual(0.45);
    expect(corps(g, D2)!.str).toBeGreaterThanOrEqual(0.25);
    // Four months on, Singapore and Bataan still standing: the divisions stay short.
    for (const t of [35, 36, 37, 38]) month(g, t);
    expect([full(g, D1), full(g, D2)].map((x) => +x.toFixed(9))).toEqual([0.55, 0.3]);
    expect(corps(g, D1)!.str).toBeLessThanOrEqual(0.55 + 1e-9);
    expect(corps(g, D2)!.str).toBeLessThanOrEqual(0.3 + 1e-9);
    // Singapore falls: the 1st and the 6th come home, each to its own division, at the worth they stand at.
    const b1 = corps(g, D1)!.str, b2 = corps(g, D2)!.str, w1 = worth(g, reg('1st')), w6 = worth(g, reg('6th'));
    take(g, 'Singapore');
    month(g, 39);
    expect(here(g, [reg('1st'), reg('6th'), reg('7th')])).toEqual([false, false, true]);
    expect([full(g, D1), +full(g, D2).toFixed(9)]).toEqual([0.8, 0.55]);
    expect(corps(g, D1)!.str).toBeGreaterThanOrEqual(b1 + w1 - 1e-9);
    expect(corps(g, D2)!.str).toBeGreaterThanOrEqual(b2 + w6 - 1e-9);
    expect(corps(g, D2)!.str).toBeLessThanOrEqual(0.55 + 1e-9);
    // Bataan falls: the 7th, and the 2nd holds no share any more.
    take(g, 'Bataan');
    month(g, 40);
    expect(corps(g, reg('7th'))).toBeUndefined();
    expect(full(g, D2)).toBe(0.8);
  });

  it('a regiment home before its division forms is remembered: the 1st adds its worth, and the 6th is in the figure already', () => {
    const g = struck(3);
    take(g, 'Singapore');
    month(g, 31);
    expect(here(g, [reg('1st'), reg('6th'), reg('7th')])).toEqual([false, false, true]);
    month(g, 34);
    // The 1st: 0.45 and the regiment's 0.25, nothing held. The 2nd: one counted regiment is not there, and its share is held.
    expect(full(g, D1)).toBe(0.8);
    expect(corps(g, D1)!.str).toBeGreaterThanOrEqual(0.7 - 1e-9);
    expect(+full(g, D2).toFixed(9)).toBe(0.55);
    expect(corps(g, D2)!.str).toBeGreaterThanOrEqual(0.375 - 1e-9);
    expect(corps(g, D2)!.str).toBeLessThanOrEqual(0.55 + 1e-9);
  });

  it('a regiment destroyed while lent holds no share: the division forms without it and the drafts fill its place', () => {
    const g = struck(3);
    removeCorps(g, corps(g, reg('6th'))!);
    month(g, 34);
    // The 2nd: neither counted regiment is there (0.25), and only the 7th, lent still, has a share held.
    expect(+full(g, D2).toFixed(9)).toBe(0.55);
    expect(corps(g, D2)!.str).toBeGreaterThanOrEqual(0.25);
    expect(corps(g, D2)!.str).toBeLessThanOrEqual(0.55 + 1e-9);
  });

  it('with the far side\'s stops off the book the shares are the same at every stop: stops 4 and 5 read as stop 3', () => {
    for (const stop of [4, 5]) {
      const g = struck(stop);
      month(g, 34);
      expect([full(g, D1), full(g, D2)].map((x) => +x.toFixed(9)), `stop ${stop}`).toEqual([0.55, 0.3]);
    }
  });
});
