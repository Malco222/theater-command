// City defences, the month card's air rows (release 2). The UI director's ruling of 26 Nov 2026 21:26Z, 4, "Other
// owners": the month card's "the harbour of the Cotentin" (in place of "Cotentin harbour", one form for a harbour
// everywhere) and "The raid on the Ruhr fails". Under the city defences switch; off, as shipped. Handed over by the air
// rules' part 8e (MONTHCARD-HANDOVER.md). A place with no article ("Bremen") as now; Pearl Harbor as it is.
// The rows the ruling does not name (a fleet, shipping) are pinned as shipped both ways. The sky, a town and a rail
// yard take the article since part 7j (the UI director, 27 Nov 2026 01:41Z, 7 and 8): their pins here moved with it,
// and the plural verb ("The Midlands are bombed") is pinned in city-defences-month-card-7j-r2.test.ts.
// A raid on an airfield is drawn as a factories row and is named as one.
import { describe, expect, it } from 'vitest';
import places from '../data/campaign/places.json';
import { harbourOf } from '../src/campaign/air';
import { START_LAID, cityDefences, setCityDefences } from '../src/campaign/citydefences';
import { atWar, createGame, declareWar, isPluralPlace, placeName } from '../src/campaign/game';
import { monthCard, monthMark, type CardRow, type MonthCard } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { townName } from '../src/campaign/towns';
import type { AirReport, Game, Region } from '../src/campaign/types';

const ME = 'GER';
/** September 1939 on the wider world with nobody playing, Germany at war with Poland. The card is Germany's. */
const START = ((): string => {
  const g = startOpen(createGame(25, null));
  if (!atWar(g, ME, 'POL')) declareWar(g, ME, 'POL');
  return saveGame(g);
})();
/** A war with the switch on or off; on, the record's start is marked as laid, as the city defences tests do. */
const game = (on: boolean): Game => { const g = loadGame(START)!; if (on) g.flags[START_LAID] = 1; setCityDefences(g, on); return g; };
const THE = (places as { the: string[] }).the;
/** Every province's name on the map, and every place the news gives an article. */
const NAMES = (): string[] => [...new Set([...loadGame(START)!.regions.map((r) => r.name), ...THE])];

let nextId = 9000;
const wing = (nat: string, ftr: number, bmr: number, lostFtr = 0, lostBmr = 0): AirReport['att'][number] => ({ nat, fleet: 'Air Fleet', leader: null, ftr, bmr, lostFtr, lostBmr });
/** One raid in the month's reports, at a place by its name (a province of the map where there is one of that name). */
function raid(g: Game, place: string, o: Partial<AirReport>): AirReport {
  const at: Region | undefined = g.regions.find((r) => r.name === place);
  const rep = {
    id: nextId++, turn: g.turn, mission: 'bomb', region: at ? at.id : -1, zone: -1, place, attNat: ME, defNat: 'FRA', att: [wing(ME, 40, 80, 0, 6)], def: [wing('FRA', 20, 0, 5)],
    lines: [], odds: 1, level: 'win', won: true, sky: false, hold: 0, result: 'Its factories lose a fifth of the month.', ...o,
  } as unknown as AirReport;
  g.airReports.push(rep);
  return rep;
}
const rowsOf = (c: MonthCard): CardRow[] => [...(c.lead ? [c.lead] : []), ...c.rows];
const mine = (c: MonthCard, rep: AirReport): CardRow[] => rowsOf(c).filter((r) => r.report?.type === 'air' && r.report.id === rep.id);
/** One war for each way of the switch, kept: a raid is laid in its month, read on the card and taken out again. */
const KEPT: Record<string, { g: Game; mark: ReturnType<typeof monthMark> }> = {};
/** The card's row for one raid, alone in its month, with the switch on or off. */
function row(on: boolean, place: string, o: Partial<AirReport> = {}): CardRow {
  const k = KEPT[String(on)] ??= ((): (typeof KEPT)[string] => { const g = game(on); return { g, mark: monthMark(g, ME) }; })();
  expect(cityDefences(k.g)).toBe(on);
  const rep = raid(k.g, place, o);
  const found = mine(monthCard(k.g, k.mark, ME), rep);
  expect(k.g.airReports.pop()).toBe(rep);
  expect(found).toHaveLength(1);
  return found[0];
}
/** The raider's own lost raid, his own raid that hit, and the defender's two rows, in that order. */
const FOUR: Partial<AirReport>[] = [
  { won: false },
  { won: true },
  { attNat: 'UK', defNat: ME, att: [wing('UK', 0, 60, 0, 9)], def: [wing(ME, 30, 0, 1)], won: false },
  { attNat: 'UK', defNat: ME, att: [wing('UK', 0, 60, 0, 4)], def: [wing(ME, 30, 0, 2)], won: true },
];
const heads = (on: boolean, place: string, o: Partial<AirReport> = {}): string[] => FOUR.map((f) => row(on, place, { ...o, ...f }).head);
/** A row with its place and headline left out: what must not move with the switch. */
const rest = (r: CardRow): string => JSON.stringify({ ...r, place: '', head: '', report: null });

describe('city defences, the month card: a harbour raid', () => {
  it('names the harbour of the Cotentin in one form with the switch on, and as shipped with it off', () => {
    expect(row(false, 'Cotentin', { mission: 'harbour', won: false }).place).toBe('Cotentin harbour');
    expect(row(true, 'Cotentin', { mission: 'harbour', won: false }).place).toBe('The harbour of the Cotentin');
    expect(heads(false, 'Cotentin', { mission: 'harbour' })).toEqual([
      'The raid on Cotentin harbour fails', 'Cotentin harbour is bombed', 'Cotentin harbour beats off the raid', 'Cotentin harbour is bombed',
    ]);
    expect(heads(true, 'Cotentin', { mission: 'harbour' })).toEqual([
      'The raid on the harbour of the Cotentin fails', 'The harbour of the Cotentin is bombed', 'The harbour of the Cotentin beats off the raid', 'The harbour of the Cotentin is bombed',
    ]);
  });

  it('gives a place with no article the same one form, "The harbour of Bremen"', () => {
    expect(row(false, 'Bremen', { mission: 'harbour' }).place).toBe('Bremen harbour');
    expect(row(true, 'Bremen', { mission: 'harbour' }).place).toBe('The harbour of Bremen');
    expect(heads(false, 'Bremen', { mission: 'harbour' })).toEqual(['The raid on Bremen harbour fails', 'Bremen harbour is bombed', 'Bremen harbour beats off the raid', 'Bremen harbour is bombed']);
    expect(heads(true, 'Bremen', { mission: 'harbour' })).toEqual(['The raid on the harbour of Bremen fails', 'The harbour of Bremen is bombed', 'The harbour of Bremen beats off the raid', 'The harbour of Bremen is bombed']);
  });

  it('leaves a harbour that is named as one as it is: Pearl Harbor, Grand Harbour', () => {
    for (const name of ['Pearl Harbor', 'Grand Harbour']) {
      for (const on of [false, true]) {
        expect(row(on, name, { mission: 'harbour' }).place).toBe(name);
        expect(heads(on, name, { mission: 'harbour' })).toEqual([`The raid on ${name} fails`, `${name} is bombed`, `${name} beats off the raid`, `${name} is bombed`]);
      }
    }
  });

  it('is the news\'s own form of the place, for every place of the map', () => {
    const names = NAMES();
    expect(names.length).toBeGreaterThan(THE.length);
    for (const name of names) {
      const on = FOUR.map((f) => row(true, name, { mission: 'harbour', ...f })), off = FOUR.map((f) => row(false, name, { mission: 'harbour', ...f }));
      const said = harbourOf(placeName(name)), label = said.charAt(0).toUpperCase() + said.slice(1);
      const was = /\bHarbou?r$/.test(name) ? name : `${name} harbour`;
      expect(said).toBe(/\bHarbou?r$/.test(name) ? name : THE.includes(name) ? `the harbour of the ${name}` : `the harbour of ${name}`);
      expect(on.map((r) => r.place)).toEqual([label, label, label, label]);
      expect(on.map((r) => r.head)).toEqual([`The raid on ${said} fails`, `${label} is bombed`, `${label} beats off the raid`, `${label} is bombed`]);
      expect(off.map((r) => r.place)).toEqual([was, was, was, was]);
      expect(off.map((r) => r.head)).toEqual([`The raid on ${was} fails`, `${was} is bombed`, `${was} beats off the raid`, `${was} is bombed`]);
      expect(on.map((r) => `${r.place} ${r.head}`).join(' ')).not.toMatch(/\bthe the\b/i);
    }
  }, 240000);
});

describe('city defences, the month card: bombing and the raid on the troops', () => {
  it('reads "The raid on the Ruhr fails" with the switch on, and as shipped with it off', () => {
    for (const mission of ['bomb', 'night'] as const) {
      expect(row(false, 'Ruhr', { mission }).place).toBe('Ruhr');
      expect(row(true, 'Ruhr', { mission }).place).toBe('The Ruhr');
      expect(heads(false, 'Ruhr', { mission })).toEqual(['The raid on Ruhr fails', 'Ruhr is bombed', 'Ruhr beats off the raid', 'Ruhr is bombed']);
      expect(heads(true, 'Ruhr', { mission })).toEqual(['The raid on the Ruhr fails', 'The Ruhr is bombed', 'The Ruhr beats off the raid', 'The Ruhr is bombed']);
    }
  });

  it('names the troops\' ground the same way: "The raid on the Saar fails"', () => {
    expect(row(false, 'Saar', { mission: 'ground' }).place).toBe('Saar');
    expect(row(true, 'Saar', { mission: 'ground' }).place).toBe('The Saar');
    expect(heads(false, 'Saar', { mission: 'ground' })).toEqual(['The raid on Saar fails', 'Saar is bombed', 'Saar beats off the raid', 'Saar is bombed']);
    expect(heads(true, 'Saar', { mission: 'ground' })).toEqual(['The raid on the Saar fails', 'The Saar is bombed', 'The Saar beats off the raid', 'The Saar is bombed']);
  });

  it('leaves a place with no article as it is, switch on or off: Bremen', () => {
    for (const mission of ['bomb', 'night', 'ground'] as const) {
      for (const on of [false, true]) {
        expect(row(on, 'Bremen', { mission }).place).toBe('Bremen');
        expect(heads(on, 'Bremen', { mission })).toEqual(['The raid on Bremen fails', 'Bremen is bombed', 'Bremen beats off the raid', 'Bremen is bombed']);
      }
    }
  });

  it('takes the article for every place the news gives one, and for no other place of the map', () => {
    expect(THE.length).toBeGreaterThan(50);
    for (const name of NAMES()) {
      const the = THE.includes(name);
      for (const mission of ['bomb', 'ground'] as const) {
        const on = FOUR.map((f) => row(true, name, { mission, ...f })), off = FOUR.map((f) => row(false, name, { mission, ...f }));
        expect(off.map((r) => r.place)).toEqual([name, name, name, name]);
        expect(off.map((r) => r.head)).toEqual([`The raid on ${name} fails`, `${name} is bombed`, `${name} beats off the raid`, `${name} is bombed`]);
        if (the) {
          // (7j: a plural place takes a plural verb, "The Midlands are bombed", by the game's one plural list.)
          const [is, beats] = isPluralPlace(name) ? ['are', 'beat'] : ['is', 'beats'];
          expect(on.map((r) => r.place)).toEqual([`The ${name}`, `The ${name}`, `The ${name}`, `The ${name}`]);
          expect(on.map((r) => r.head)).toEqual([`The raid on the ${name} fails`, `The ${name} ${is} bombed`, `The ${name} ${beats} off the raid`, `The ${name} ${is} bombed`]);
        } else {
          expect(on.map((r) => r.place)).toEqual(off.map((r) => r.place));
          expect(on.map((r) => r.head)).toEqual(off.map((r) => r.head));
        }
      }
    }
  }, 240000);

  it('names the place of a raid on an airfield as a bombing row does, for it is drawn as one: the Ruhr', () => {
    // (The month card has no row of its own for a raid on an airfield, air.ts OFFENSIVE: it falls to the factories row.)
    expect(row(false, 'Ruhr', { mission: 'airfield' }).picture).toBe('factories');
    expect(row(true, 'Ruhr', { mission: 'airfield' }).place).toBe('The Ruhr');
    expect(heads(true, 'Ruhr', { mission: 'airfield' })).toEqual(['The raid on the Ruhr fails', 'The Ruhr is bombed', 'The Ruhr beats off the raid', 'The Ruhr is bombed']);
    expect(row(false, 'Ruhr', { mission: 'airfield' }).place).toBe('Ruhr');
    expect(heads(false, 'Ruhr', { mission: 'airfield' })).toEqual(['The raid on Ruhr fails', 'Ruhr is bombed', 'Ruhr beats off the raid', 'Ruhr is bombed']);
    FOUR.forEach((f) => expect(rest(row(true, 'Ruhr', { mission: 'airfield', ...f }))).toBe(rest(row(false, 'Ruhr', { mission: 'airfield', ...f }))));
  });

  it('keeps a name\'s own "The" inside the headline, as shipped: The Pas', () => {
    expect(NAMES()).toContain('The Pas');
    for (const on of [false, true]) {
      expect(row(on, 'The Pas', { won: false }).place).toBe('The Pas');
      expect(heads(on, 'The Pas')).toEqual(['The raid on The Pas fails', 'The Pas is bombed', 'The Pas beats off the raid', 'The Pas is bombed']);
    }
    expect(row(false, 'The Pas', { mission: 'harbour', won: false }).head).toBe('The raid on The Pas harbour fails');
    expect(row(true, 'The Pas', { mission: 'harbour', won: false }).head).toBe('The raid on the harbour of The Pas fails');
  });
});

describe('city defences, the month card: what the switch does not move', () => {
  it('is display only with the switch on as with it off, and a kept war reads as a fresh one', () => {
    for (const on of [false, true]) {
      for (const [place, mission] of [['Cotentin', 'harbour'], ['Ruhr', 'bomb'], ['Saar', 'ground']] as const) {
        const g = game(on), mark = monthMark(g, ME), rep = raid(g, place, { mission, won: false });
        const was = saveGame(g);
        const fresh = mine(monthCard(g, mark, ME), rep);
        expect(saveGame(g)).toBe(was);
        expect(fresh).toHaveLength(1);
        expect(JSON.stringify({ ...fresh[0], report: null })).toBe(JSON.stringify({ ...row(on, place, { mission, won: false }), report: null }));
      }
    }
  });

  it('with the switch off the save carries no mark of it, and the card is the same as in a war that never had it', () => {
    const never = loadGame(START)!, off = game(false), back = game(true);
    setCityDefences(back, false);
    expect(saveGame(off)).toBe(saveGame(never));
    for (const g of [never, off, back]) expect('cityDefences' in g.flags).toBe(false);
    const card = (g: Game): string => { const mark = monthMark(g, ME); const rep = raid(g, 'Cotentin', { mission: 'harbour', won: false, id: 77 }); return JSON.stringify(mine(monthCard(g, mark, ME), rep)); };
    expect(card(off)).toBe(card(never));
    expect(card(back)).toBe(card(never));
    expect(card(never)).toContain('Cotentin harbour');
  });

  it('moves only the place and the headline of a harbour, a bombing or a troops row', () => {
    for (const [place, mission] of [['Cotentin', 'harbour'], ['Ruhr', 'bomb'], ['Ruhr', 'night'], ['Saar', 'ground'], ['Bremen', 'harbour']] as const) {
      FOUR.forEach((f) => {
        const on = row(true, place, { mission, ...f }), off = row(false, place, { mission, ...f });
        expect(rest(on)).toBe(rest(off));
        expect(on.place).not.toBe(off.place);
      });
    }
  });

  it('leaves the rows the ruling does not name as shipped: a fleet, shipping; the sky, a town, a rail yard move only their place and headline', () => {
    const ruhr = loadGame(START)!.regions.find((r) => r.name === 'Ruhr')!;
    // (7j: the sky, a town and a rail yard take the article; nothing else of theirs moves.)
    for (const [place, o] of [['Saar', { mission: 'superiority' }], ['Ruhr', { mission: 'town' }], ['Ruhr', { mission: 'line' }], ['Valdai', { mission: 'town' }], ['Azores', { mission: 'line' }]] as [string, Partial<AirReport>][]) {
      FOUR.forEach((f) => expect(rest(row(true, place, { ...o, ...f }))).toBe(rest(row(false, place, { ...o, ...f }))));
    }
    const others: [string, Partial<AirReport>][] = [
      ['the North Sea', { mission: 'strike', region: -1, zone: 0 }],
      ['the North Sea', { mission: 'fleet', region: -1, zone: 0 }],
    ];
    for (const [place, o] of others) {
      FOUR.forEach((f) => {
        const on = row(true, place, { ...o, ...f }), off = row(false, place, { ...o, ...f });
        expect(JSON.stringify({ ...on, report: null })).toBe(JSON.stringify({ ...off, report: null }));
      });
    }
    // To the letter. (Pins moved by 7j: with the switch on the sky over the Saar takes the article; the Ruhr's town,
    // Essen, takes none, so its town and rail-yard rows read as shipped.)
    expect(row(false, 'Saar', { mission: 'superiority' }).place).toBe('The sky over Saar');
    expect(row(false, 'Saar', { mission: 'superiority' }).head).toBe('The sky over Saar is yours');
    expect(row(true, 'Saar', { mission: 'superiority' }).place).toBe('The sky over the Saar');
    expect(row(true, 'Saar', { mission: 'superiority' }).head).toBe('The sky over the Saar is yours');
    expect(townName(ruhr)).toBe('Essen');
    for (const on of [false, true]) {
      expect(row(on, 'Ruhr', { mission: 'town', won: false }).head).toBe('The raid on Essen fails');
      expect(row(on, 'Ruhr', { mission: 'line', won: false }).head).toBe('The raid on Essen rail yard fails');
    }
  });
});

describe('city defences, the month card: a town the news gives an article', () => {
  it('gives the town and rail-yard rows of such a town the article with the switch on, as shipped with it off: Valdai, the Azores', () => {
    // (Pins moved by 7j, the UI director's 7: "The raid on the Azores fails", "The Canary Islands rail yard". These towns
    // carry their province's name, which the news gives "the". The Azores are plural; a rail yard is one thing.)
    for (const name of ['Valdai', 'Azores']) {
      expect(THE).toContain(name);
      expect(townName(loadGame(START)!.regions.find((r) => r.name === name)!)).toBe(name);
      expect(row(false, name, { mission: 'town' }).place).toBe(name);
      expect(heads(false, name, { mission: 'town' })).toEqual([`The raid on ${name} fails`, `${name} is bombed`, `${name} beats off the raid`, `${name} is bombed`]);
      expect(row(false, name, { mission: 'line' }).place).toBe(`${name} rail yard`);
      expect(heads(false, name, { mission: 'line' })).toEqual([`The raid on ${name} rail yard fails`, `${name} rail yard is bombed`, `${name} rail yard beats off the raid`, `${name} rail yard is bombed`]);
      expect(row(true, name, { mission: 'line' }).place).toBe(`The ${name} rail yard`);
      expect(heads(true, name, { mission: 'line' })).toEqual([`The raid on the ${name} rail yard fails`, `The ${name} rail yard is bombed`, `The ${name} rail yard beats off the raid`, `The ${name} rail yard is bombed`]);
    }
    expect(row(true, 'Valdai', { mission: 'town' }).place).toBe('The Valdai');
    expect(heads(true, 'Valdai', { mission: 'town' })).toEqual(['The raid on the Valdai fails', 'The Valdai is bombed', 'The Valdai beats off the raid', 'The Valdai is bombed']);
    expect(row(true, 'Azores', { mission: 'town' }).place).toBe('The Azores');
    expect(heads(true, 'Azores', { mission: 'town' })).toEqual(['The raid on the Azores fails', 'The Azores are bombed', 'The Azores beat off the raid', 'The Azores are bombed']);
  });
});
