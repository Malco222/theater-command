// City defences 7j, the month card names a place one way (release 2). The UI director's ruling of 27 Nov 2026 01:41Z
// on the month card's air rows, items 5, 7, 8 and "Length": under the city defences switch a plural place takes a
// plural verb by the game's one plural list ("The Midlands are bombed"), a town and a rail yard take the article ("The
// raid on the Azores fails", "The Canary Islands rail yard"), the sky, land and shore rows take it too ("The sky over
// the Riviera is yours", "The Valdai is taken", "The Riviera is shelled from the sea"), and the browser never cuts a
// label. Off, every card as shipped. Item 6 (the sentence under the lead) is the air rules' and is not here.
import { describe, expect, it } from 'vitest';
import places from '../data/campaign/places.json';
import { START_LAID, cityDefences, setCityDefences } from '../src/campaign/citydefences';
import { atWar, createGame, declareWar, isPluralPlace, theWords } from '../src/campaign/game';
import { monthCard, monthMark, type CardRow, type MonthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import type { AirReport, BattleReport, Game, Headline, ShoreReport } from '../src/campaign/types';
import { monthCardHtml } from '../src/campaign/ui/monthview';

const ME = 'GER';
/** September 1939 on the wider world with nobody playing, Germany at war with Poland. The card is Germany's. */
const START = ((): string => {
  const g = startOpen(createGame(25, null));
  if (!atWar(g, ME, 'POL')) declareWar(g, ME, 'POL');
  return saveGame(g);
})();
const game = (on: boolean): Game => { const g = loadGame(START)!; if (on) g.flags[START_LAID] = 1; setCityDefences(g, on); return g; };
const THE = (places as { the: string[] }).the;
const KEPT: Record<string, Game> = {};
const kept = (on: boolean): Game => { const g = (KEPT[String(on)] ??= game(on)); expect(cityDefences(g)).toBe(on); return g; };
const regionOf = (g: Game, name: string): number => { const i = g.regions.findIndex((r) => r.name === name); expect(i).toBeGreaterThanOrEqual(0); return i; };
const rowsOf = (c: MonthCard): CardRow[] => [...(c.lead ? [c.lead] : []), ...c.rows];
let nextId = 70000;

/** The card with one thing laid in its month, and that thing taken out again; `find` picks its row. */
function cardWith(on: boolean, lay: (g: Game) => () => void): MonthCard {
  const g = kept(on), mark = monthMark(g, ME);
  const undo = lay(g);
  try { return monthCard(g, mark, ME); } finally { undo(); }
}
const pushed = <T>(xs: T[], x: T): (() => void) => { xs.push(x); return () => { expect(xs.pop()).toBe(x); }; };

const wing = (nat: string, ftr: number, bmr: number, lostFtr = 0, lostBmr = 0): AirReport['att'][number] => ({ nat, fleet: 'Air Fleet', leader: null, ftr, bmr, lostFtr, lostBmr });
function air(on: boolean, place: string, o: Partial<AirReport> = {}): CardRow {
  const id = nextId++;
  const c = cardWith(on, (g) => {
    const at = g.regions.findIndex((r) => r.name === place);
    return pushed(g.airReports, {
      id, turn: g.turn, mission: 'bomb', region: at, zone: -1, place, attNat: ME, defNat: 'FRA', att: [wing(ME, 40, 80, 0, 6)], def: [wing('FRA', 20, 0, 5)],
      lines: [], odds: 1, level: 'win', won: true, sky: false, hold: 0, result: 'Its factories lose a fifth of the month.', ...o,
    } as unknown as AirReport);
  });
  const r = rowsOf(c).filter((x) => x.report?.type === 'air' && x.report.id === id);
  expect(r).toHaveLength(1);
  return r[0];
}
/** The raider's lost raid, his raid that hit, and the defender's two rows. */
const FOUR: Partial<AirReport>[] = [
  { won: false },
  { won: true },
  { attNat: 'UK', defNat: ME, att: [wing('UK', 0, 60, 0, 9)], def: [wing(ME, 30, 0, 1)], won: false },
  { attNat: 'UK', defNat: ME, att: [wing('UK', 0, 60, 0, 4)], def: [wing(ME, 30, 0, 2)], won: true },
];
const heads = (on: boolean, place: string, o: Partial<AirReport> = {}): string[] => FOUR.map((f) => air(on, place, { ...o, ...f }).head);

const unit = (nat: string, kind = 'inf', guard = false): BattleReport['attackers'][number] => ({ nat, kind, men: 30000, guard, name: `${nat} corps` } as unknown as BattleReport['attackers'][number]);
/** A battle of his in a province, his attack or his defence. */
function land(on: boolean, place: string, o: Partial<BattleReport> & { def?: boolean; own?: boolean } = {}): CardRow {
  const id = nextId++;
  const { def, own, ...rest } = o;
  const c = cardWith(on, (g) => {
    const at = regionOf(g, place);
    const [a, d] = def ? ['POL', ME] : [ME, 'POL'];
    // (A town of his that is besieged and holds: for the card the province is his while the town holds.)
    const was = g.regions[at].ctrl;
    if (own) g.regions[at].ctrl = ME;
    const back = pushed(g.reports, {
      id, turn: g.turn, week: 2, region: at, regionName: place, attNat: a, defNat: d, attackers: [unit(a)], defenders: [unit(d)],
      lines: [], aPower: 1, dPower: 1, odds: 1, level: 'win', taken: true, attLost: 3000, defLost: 4000, prisoners: 0, ...rest,
    } as unknown as BattleReport);
    return () => { back(); g.regions[at].ctrl = was; };
  });
  const r = rowsOf(c).filter((x) => x.report?.type === 'land' && x.report.id === id);
  expect(r).toHaveLength(1);
  return r[0];
}
/** His warships shell a coast. */
function shore(on: boolean, place: string): CardRow {
  const id = nextId++;
  const c = cardWith(on, (g) => pushed(g.shoreReports, {
    id, turn: g.turn, nat: ME, fleet: 'Battle Fleet', admiral: null, ships: { bb: 2, cv: 0, ca: 1, dd: 4, ss: 0 }, sunk: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 },
    region: regionOf(g, place), place, zone: -1, defNat: 'FRA', what: 'troops', guns: 3, cut: 0.1, divs: 2, output: 0, nation: 0, lines: [], result: 'Two divisions were hit.',
  } as unknown as ShoreReport));
  const r = rowsOf(c).filter((x) => x.report?.type === 'shore' && x.report.id === id);
  expect(r).toHaveLength(1);
  return r[0];
}
/** A row with its place and headline left out: what must not move with the switch. */
const rest = (r: CardRow): string => JSON.stringify({ ...r, place: '', head: '', report: null });

describe('7j, item 5: the verb follows the game\'s one plural list', () => {
  it('is the same list that makes "the Midlands\' output", and no other', () => {
    const plural = [...THE.filter((n) => /s$/.test(n)), 'Netherlands', 'United States'];
    for (const n of plural) {
      expect(isPluralPlace(n)).toBe(true);
      expect(isPluralPlace(`The ${n}`)).toBe(true);
      expect(isPluralPlace(`the ${n}`)).toBe(true);
    }
    for (const n of THE.filter((x) => !plural.includes(x))) expect(isPluralPlace(n)).toBe(false);
    for (const n of ['Paris', 'Wales', 'Flanders', 'Brussels', 'The Pas', 'Bremen', 'Kansas']) expect(isPluralPlace(n)).toBe(false);
    // The news's possessive is made from the same list: the apostrophe alone exactly where the verb is plural.
    expect(theWords("The bombing stopped 1% of Midlands's output.")).toBe("The bombing stopped 1% of the Midlands' output.");
    expect(theWords("The bombing stopped 1% of Ruhr's output.")).toBe("The bombing stopped 1% of the Ruhr's output.");
    for (const n of THE) expect(theWords(`${n}'s output`).endsWith(isPluralPlace(n) ? `${n}' output` : `${n}'s output`)).toBe(true);
  });

  it('bombs the Midlands, and the Midlands beat off the raid, with the switch on; as shipped with it off', () => {
    for (const mission of ['bomb', 'night', 'ground', 'airfield'] as const) {
      expect(air(true, 'Midlands', { mission }).place).toBe('The Midlands');
      expect(heads(true, 'Midlands', { mission })).toEqual(['The raid on the Midlands fails', 'The Midlands are bombed', 'The Midlands beat off the raid', 'The Midlands are bombed']);
      expect(air(false, 'Midlands', { mission }).place).toBe('Midlands');
      expect(heads(false, 'Midlands', { mission })).toEqual(['The raid on Midlands fails', 'Midlands is bombed', 'Midlands beats off the raid', 'Midlands is bombed']);
    }
  });

  it('keeps a singular place singular: the Ruhr is bombed, the Ruhr beats off the raid', () => {
    expect(heads(true, 'Ruhr')).toEqual(['The raid on the Ruhr fails', 'The Ruhr is bombed', 'The Ruhr beats off the raid', 'The Ruhr is bombed']);
    expect(heads(false, 'Ruhr')).toEqual(['The raid on Ruhr fails', 'Ruhr is bombed', 'Ruhr beats off the raid', 'Ruhr is bombed']);
  });

  it('keeps the singular verb for the harbour of a plural place, for the harbour is what is bombed', () => {
    expect(air(true, 'Midlands', { mission: 'harbour' }).place).toBe('The harbour of the Midlands');
    expect(heads(true, 'Midlands', { mission: 'harbour' })).toEqual([
      'The raid on the harbour of the Midlands fails', 'The harbour of the Midlands is bombed', 'The harbour of the Midlands beats off the raid', 'The harbour of the Midlands is bombed',
    ]);
    expect(heads(false, 'Midlands', { mission: 'harbour' })).toEqual(['The raid on Midlands harbour fails', 'Midlands harbour is bombed', 'Midlands harbour beats off the raid', 'Midlands harbour is bombed']);
  });
});

describe('7j, item 7: the article on town and rail-yard rows', () => {
  it('reads "The raid on the Azores fails" with the switch on, "The raid on Azores fails" off', () => {
    expect(townName(kept(true).regions[regionOf(kept(true), 'Azores')])).toBe('Azores');
    expect(air(true, 'Azores', { mission: 'town', won: false }).head).toBe('The raid on the Azores fails');
    expect(air(true, 'Azores', { mission: 'town' }).place).toBe('The Azores');
    expect(heads(true, 'Azores', { mission: 'town' })).toEqual(['The raid on the Azores fails', 'The Azores are bombed', 'The Azores beat off the raid', 'The Azores are bombed']);
    expect(air(false, 'Azores', { mission: 'town', won: false }).head).toBe('The raid on Azores fails');
    expect(heads(false, 'Azores', { mission: 'town' })).toEqual(['The raid on Azores fails', 'Azores is bombed', 'Azores beats off the raid', 'Azores is bombed']);
  });

  it('names the Canary Islands rail yard, which is one thing and takes the singular verb', () => {
    expect(townName(kept(true).regions[regionOf(kept(true), 'Canary Islands')])).toBe('Canary Islands');
    expect(air(true, 'Canary Islands', { mission: 'line' }).place).toBe('The Canary Islands rail yard');
    expect(heads(true, 'Canary Islands', { mission: 'line' })).toEqual([
      'The raid on the Canary Islands rail yard fails', 'The Canary Islands rail yard is bombed', 'The Canary Islands rail yard beats off the raid', 'The Canary Islands rail yard is bombed',
    ]);
    expect(air(false, 'Canary Islands', { mission: 'line' }).place).toBe('Canary Islands rail yard');
    expect(heads(false, 'Canary Islands', { mission: 'line' })).toEqual([
      'The raid on Canary Islands rail yard fails', 'Canary Islands rail yard is bombed', 'Canary Islands rail yard beats off the raid', 'Canary Islands rail yard is bombed',
    ]);
  });

  it('leaves a name with its own "The" as shipped, and a place with no article bare: The Pas, Bremen', () => {
    for (const on of [false, true]) {
      for (const mission of ['bomb', 'town'] as const) {
        expect(heads(on, 'The Pas', { mission })).toEqual(['The raid on The Pas fails', 'The Pas is bombed', 'The Pas beats off the raid', 'The Pas is bombed']);
      }
      expect(heads(on, 'The Pas', { mission: 'line' })).toEqual(['The raid on The Pas rail yard fails', 'The Pas rail yard is bombed', 'The Pas rail yard beats off the raid', 'The Pas rail yard is bombed']);
      for (const mission of ['bomb', 'town'] as const) {
        expect(air(on, 'Bremen', { mission }).place).toBe('Bremen');
        expect(heads(on, 'Bremen', { mission })).toEqual(['The raid on Bremen fails', 'Bremen is bombed', 'Bremen beats off the raid', 'Bremen is bombed']);
      }
      expect(air(on, 'Bremen', { mission: 'line' }).place).toBe('Bremen rail yard');
    }
  });

  it('moves only the place and the headline', () => {
    for (const [place, mission] of [['Azores', 'town'], ['Canary Islands', 'line'], ['Midlands', 'bomb'], ['Riviera', 'superiority']] as const) {
      FOUR.forEach((f) => expect(rest(air(true, place, { mission, ...f }))).toBe(rest(air(false, place, { mission, ...f }))));
    }
  });
});

describe('7j, item 8: the article on sky, land and shore rows', () => {
  it('reads "The sky over the Riviera is yours"; the sky is one thing, over the Midlands too; Bremen bare', () => {
    const sky = (on: boolean, place: string, won: boolean): string[] => { const r = air(on, place, { mission: 'superiority', won }); return [r.place, r.head]; };
    expect(sky(true, 'Riviera', true)).toEqual(['The sky over the Riviera', 'The sky over the Riviera is yours']);
    expect(sky(true, 'Riviera', false)).toEqual(['The sky over the Riviera', 'The sky over the Riviera is lost']);
    expect(sky(false, 'Riviera', true)).toEqual(['The sky over Riviera', 'The sky over Riviera is yours']);
    expect(sky(true, 'Midlands', true)).toEqual(['The sky over the Midlands', 'The sky over the Midlands is yours']);
    expect(sky(false, 'Midlands', true)).toEqual(['The sky over Midlands', 'The sky over Midlands is yours']);
    for (const on of [false, true]) expect(sky(on, 'Bremen', true)).toEqual(['The sky over Bremen', 'The sky over Bremen is yours']);
  });

  it('reads "The Valdai is taken" and "The Midlands are taken"; Bremen bare; the line under it as shipped', () => {
    const cases: [string, Partial<BattleReport> & { def?: boolean }, string, string, string][] = [
      // place, battle, switch on, the plural on, off
      ['Valdai', { taken: true }, 'The Valdai is taken', 'The Midlands are taken', 'Valdai is taken'],
      ['Valdai', { taken: false }, 'The attack on the Valdai is thrown back', 'The attack on the Midlands is thrown back', 'The attack on Valdai is thrown back'],
      ['Valdai', { def: true, taken: true }, 'The Valdai is lost', 'The Midlands are lost', 'Valdai is lost'],
      ['Valdai', { def: true, taken: false }, 'The Valdai holds', 'The Midlands hold', 'Valdai holds'],
      ['Valdai', { yielded: true }, 'The Valdai is taken without a fight', 'The Midlands are taken without a fight', 'Valdai is taken without a fight'],
      ['Valdai', { def: true, yielded: true }, 'The Valdai is given up', 'The Midlands are given up', 'Valdai is given up'],
    ];
    for (const [place, o, on, many, off] of cases) {
      const a = land(true, place, o), b = land(false, place, o);
      expect(a.place).toBe('The Valdai');
      expect(b.place).toBe('Valdai');
      expect(a.head).toBe(on);
      expect(b.head).toBe(off);
      expect(rest(a)).toBe(rest(b));
      expect(land(true, 'Midlands', o).head).toBe(many);
      expect(land(false, 'Midlands', o).head).toBe(off.replace('Valdai', 'Midlands'));
      for (const s of [true, false]) {
        const r = land(s, 'Bremen', o);
        expect(r.place).toBe('Bremen');
        expect(r.head).toBe(off.replace('Valdai', 'Bremen'));
      }
    }
  });

  it('names a town fought for the same way, the Azores besieged, and the siege of the Azores', () => {
    const laid = land(true, 'Azores', { siege: true, town: true, def: true, own: true, taken: false });
    expect([laid.place, laid.head, laid.tag]).toEqual(['The Azores', 'The Azores are besieged', 'BESIEGED']);
    expect(land(false, 'Azores', { siege: true, town: true, def: true, own: true, taken: false }).head).toBe('Azores is besieged');
    expect(rest(laid)).toBe(rest(land(false, 'Azores', { siege: true, town: true, def: true, own: true, taken: false })));
    expect(land(true, 'Azores', { siege: true, town: true, taken: false }).head).toBe('The Azores are under siege');
    expect(land(true, 'Azores', { relief: true, town: true, taken: true }).head).toBe('The siege of the Azores is raised');
    expect(land(false, 'Azores', { relief: true, town: true, taken: true }).head).toBe('The siege of Azores is raised');
    expect(land(true, 'Valdai', { relief: true, town: true, taken: false }).head).toBe('The siege of the Valdai holds');
  });

  it('reads "The Riviera is shelled from the sea"; the Midlands are; Bremen bare', () => {
    expect([shore(true, 'Riviera').place, shore(true, 'Riviera').head]).toEqual(['The Riviera', 'The Riviera is shelled from the sea']);
    expect([shore(false, 'Riviera').place, shore(false, 'Riviera').head]).toEqual(['Riviera', 'Riviera is shelled from the sea']);
    expect(shore(true, 'Midlands').head).toBe('The Midlands are shelled from the sea');
    expect(shore(false, 'Midlands').head).toBe('Midlands is shelled from the sea');
    for (const on of [false, true]) expect([shore(on, 'Bremen').place, shore(on, 'Bremen').head]).toEqual(['Bremen', 'Bremen is shelled from the sea']);
    expect(rest(shore(true, 'Riviera'))).toBe(rest(shore(false, 'Riviera')));
  });

  it('built beyond her examples, same rule: a province lost without a battle, and a ring closed in a province', () => {
    const lost = (on: boolean): CardRow => {
      const c = cardWith(on, (g) => { const i = regionOf(g, 'Saar'), was = g.regions[i].ctrl; g.regions[i].ctrl = 'POL'; return () => { g.regions[i].ctrl = was; }; });
      const r = rowsOf(c).filter((x) => x.kind === 'GROUND');
      expect(r).toHaveLength(1);
      return r[0];
    };
    expect([lost(true).place, lost(true).head]).toEqual(['The Saar', '1 province lost to Poland']);
    expect([lost(false).place, lost(false).head]).toEqual(['Saar', '1 province lost to Poland']);
    expect(rest(lost(true))).toBe(rest(lost(false)));
    const ring = (on: boolean, at = 'Valdai'): CardRow => {
      const c = cardWith(on, (g) => pushed(g.headlines, { turn: g.turn, week: 2, kind: 'event', arm: 'land', nat: ME, won: ME, lost: 'POL', region: regionOf(g, at),
        text: `The ring closes at ${at}: 2 Polish corps, 60,000 men, are cut off.` } as Headline));
      const r = rowsOf(c).filter((x) => x.kind === 'ON LAND');
      expect(r).toHaveLength(1);
      return r[0];
    };
    // (7k, the UI director, 27 Nov 2026 05:53Z: "in" with the article for a province, "at" for a place named for its
    // town, by the game's own inOrAt. A province with no article takes "in" bare, as inOrAt says it everywhere else.)
    expect([ring(true).place, ring(true).head]).toEqual(['The Valdai', 'The ring closes in the Valdai']);
    expect([ring(false).place, ring(false).head]).toEqual(['Valdai', 'The ring closes at Valdai']);
    expect(rest(ring(true))).toBe(rest(ring(false)));
    expect([ring(true, 'Ardennes').place, ring(true, 'Ardennes').head]).toEqual(['The Ardennes', 'The ring closes in the Ardennes']);
    expect([ring(false, 'Ardennes').place, ring(false, 'Ardennes').head]).toEqual(['Ardennes', 'The ring closes at Ardennes']);
    expect([ring(true, 'Stalingrad').place, ring(true, 'Stalingrad').head]).toEqual(['Stalingrad', 'The ring closes at Stalingrad']);
    expect(ring(true, 'Stalingrad')).toEqual(ring(false, 'Stalingrad'));
    expect([ring(true, 'Bavaria').place, ring(true, 'Bavaria').head]).toEqual(['Bavaria', 'The ring closes in Bavaria']);
    expect([ring(false, 'Bavaria').place, ring(false, 'Bavaria').head]).toEqual(['Bavaria', 'The ring closes at Bavaria']);
    for (const at of ['Ardennes', 'Bavaria']) expect(rest(ring(true, at))).toBe(rest(ring(false, at)));
  });

  it('built beyond her examples, same rule: a town that fell after the siege was laid, "The Valdai is taken", "The Azores are lost"', () => {
    // (His siege laid, and the town his as the card is made: WON. Their siege laid on his town, and the town not his: LOST.)
    const fell = (on: boolean, place: string, def: boolean): CardRow => land(on, place, { siege: true, town: true, def, own: !def, taken: !def });
    for (const [place, def, on, off, tag] of [
      ['Valdai', false, 'The Valdai is taken', 'Valdai is taken', 'WON'],
      ['Azores', false, 'The Azores are taken', 'Azores is taken', 'WON'],
      ['Valdai', true, 'The Valdai is lost', 'Valdai is lost', 'LOST'],
      ['Azores', true, 'The Azores are lost', 'Azores is lost', 'LOST'],
    ] as const) {
      const a = fell(true, place, def), b = fell(false, place, def);
      expect([a.place, a.head, a.tag]).toEqual([`The ${place}`, on, tag]);
      expect([b.place, b.head, b.tag]).toEqual([place, off, tag]);
      expect(rest(a)).toBe(rest(b));
    }
    // (Bremen is his own, so his siege of it fell to him: bare, as shipped.)
    for (const s of [false, true]) expect([fell(s, 'Bremen', false).place, fell(s, 'Bremen', false).head]).toEqual(['Bremen', 'Bremen is taken']);
  });

  describe('built beyond her examples, same rule: the news rows', () => {
    /** A line of news on land in the card's month; `find` is its row. */
    const news = (on: boolean, h: Partial<Headline> & { place?: string; ctrl?: string }): CardRow => {
      const { place, ctrl, ...rest0 } = h;
      const c = cardWith(on, (g) => {
        const at = place ? regionOf(g, place) : -1, was = at >= 0 ? g.regions[at].ctrl : '';
        if (at >= 0 && ctrl) g.regions[at].ctrl = ctrl;
        const back = pushed(g.headlines, { turn: g.turn, week: 2, kind: 'event', arm: 'land', ...(at >= 0 ? { region: at } : {}), ...rest0 } as Headline);
        return () => { back(); if (at >= 0) g.regions[at].ctrl = was; };
      });
      const r = rowsOf(c).filter((x) => x.kind === 'ON LAND');
      expect(r).toHaveLength(1);
      return r[0];
    };
    it('names a place the news gives "in the Ardennes" with no province "The Ardennes"; its headline as shipped', () => {
      const h = { nat: ME, won: ME, lost: 'FRA', text: 'German armour breaks through in the Ardennes.' };
      const a = news(true, h), b = news(false, h);
      expect([a.place, a.head]).toEqual(['The Ardennes', 'German armour breaks through']);
      expect([b.place, b.head]).toEqual(['Ardennes', 'German armour breaks through']);
      expect(rest(a)).toBe(rest(b));
    });
    it('reads "The siege of the Valdai is raised" for a siege raised with no battle', () => {
      const h = { place: 'Valdai', ctrl: ME, nat: ME, won: ME, lost: 'POL', text: 'The siege of the Valdai is raised: the Polish troops fall back.' };
      const a = news(true, h), b = news(false, h);
      expect([a.place, a.head, a.tag]).toEqual(['The Valdai', 'The siege of the Valdai is raised', 'HELD']);
      expect([b.place, b.head, b.tag]).toEqual(['Valdai', 'The siege of Valdai is raised', 'HELD']);
      expect(rest(a)).toBe(rest(b));
    });
    it('reads "The Ruhr is besieged" and "The Azores are besieged" for a siege laid with no battle; Bremen bare', () => {
      for (const [place, on, off] of [
        ['Ruhr', 'The Ruhr is besieged', 'Ruhr is besieged'],
        ['Azores', 'The Azores are besieged', 'Azores is besieged'],
        ['Bremen', 'Bremen is besieged', 'Bremen is besieged'],
      ] as const) {
        const h = { place, ctrl: ME, nat: 'POL', won: 'POL', lost: ME, text: `Polish troops close round ${place}: ${place} is under siege.` };
        const a = news(true, h), b = news(false, h);
        expect([a.place, a.head, a.tag]).toEqual([place === 'Bremen' ? 'Bremen' : `The ${place}`, on, 'BESIEGED']);
        expect([b.place, b.head, b.tag]).toEqual([place, off, 'BESIEGED']);
        expect(rest(a)).toBe(rest(b));
      }
    });
  });

  it('keeps the staff\'s line on a town that held as shipped: "the battle at the Valdai", switch on or off', () => {
    for (const on of [false, true]) {
      const c = cardWith(on, (g) => {
        const a = pushed(g.headlines, { turn: g.turn, week: 3, kind: 'info', nat: ME, text: 'Your staff on the loss of the Valdai: the line must be held.' } as Headline);
        const b = ((): (() => void) => {
          const r = { id: nextId++, turn: g.turn, week: 2, region: regionOf(g, 'Valdai'), regionName: 'Valdai', attNat: 'POL', defNat: ME, attackers: [unit('POL')], defenders: [unit(ME)],
            lines: [], aPower: 1, dPower: 1, odds: 1, level: 'win', taken: false, attLost: 3000, defLost: 2000, prisoners: 0 } as unknown as BattleReport;
          return pushed(g.reports, r);
        })();
        return () => { b(); a(); };
      });
      expect(c.alsoFull.join(' ')).toContain('Your staff on the battle at the Valdai');
    }
  });
});

describe('7j, length: under the switch a label is never cut', () => {
  const help = { esc: (s: string) => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/"/g, '&quot;'), flag: () => '', name: (n: string) => n, all: true, orders: false, tour: false };
  it('carries the wrap on the card only with the switch on, and the browser\'s card takes its class', () => {
    // (A battle on land leads the card, so the harbour row is drawn as a row.)
    const on = cardWith(true, (g) => {
      const a = pushed(g.airReports, { id: nextId++, turn: g.turn, mission: 'harbour', region: -1, zone: -1, place: 'Santa Fe de la Vera Cruz', attNat: ME, defNat: 'FRA',
        att: [wing(ME, 40, 80, 0, 6)], def: [wing('FRA', 20, 0, 5)], lines: [], odds: 1, level: 'win', won: true, sky: false, hold: 0, result: '' } as unknown as AirReport);
      const b = pushed(g.reports, { id: nextId++, turn: g.turn, week: 2, region: regionOf(g, 'Valdai'), regionName: 'Valdai', attNat: ME, defNat: 'POL', attackers: [unit(ME)], defenders: [unit('POL')],
        lines: [], aPower: 1, dPower: 1, odds: 1, level: 'win', taken: true, attLost: 30000, defLost: 40000, prisoners: 0 } as unknown as BattleReport);
      return () => { b(); a(); };
    });
    expect(on.lead?.place).toBe('The Valdai');
    const off = cardWith(false, () => () => undefined);
    expect(on.wrap).toBe(true);
    expect('wrap' in off).toBe(false);
    const html = monthCardHtml(on, help), plain = monthCardHtml(off, help);
    expect(html).toMatch(/^<div class="mc[^"]* wrap">/);
    expect(plain).not.toContain(' wrap');
    expect(plain).toMatch(/^<div class="mc( two)?">/);
    // The longest harbour label, 39 letters: still marked long (smaller type), and wrapped rather than cut.
    expect(html).toContain('<b class="pl long">The harbour of Santa Fe de la Vera Cruz</b>');
  });
});
