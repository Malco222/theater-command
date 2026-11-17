// AHEAD OF THE RULES: ruling 213(d), the aircraft works of the smaller powers (the design director, 2026-11-17 16:41Z,
// filled in at 16:57Z and mended at 17:03Z and 17:04Z; the historian's list,
// /mnt/project-files/historian/aircraft-works-213.json). With the cities version. An aircraft industry is a fact of the
// record: for a smaller power a list in data stands in place of the two- and three-factory test (arsenal.json
// 'factories' 2 and 3). A works opens by month for each kind (September 1939 is month 1) and then stays open while the
// country stands. The great powers are unchanged. The grand campaign only. Everyone else gets the record's aircraft
// as dated rows of air points, which arrive in their month only while a giver named stands and is not at war with
// the receiver. Words: /mnt/project-files/ui-director/words-ruling-213.md (passed 16:54Z, 17:02Z and 17:04Z).
//
// THE LIST, twelve: Poland, the Netherlands, Belgium, Yugoslavia, Romania fighters 1 and bombers 1; Canada 6 and 3;
// Switzerland 1 and 37; Finland 1 and 21; Hungary 13 and 49; Sweden 50 and 1; Australia 35 and 21; Spain 1 and never.
// Turkey and Argentina are struck (dated rows instead). South Africa, New Zealand and Manchukuo are off.
//
// The rules builder has not named the reading yet (17:16Z: "the works list as data/campaign/aircraft-works.json read by
// industry() in arsenal.ts"). These tests wait, skipped, until arsenal.ts exports the list as AIRCRAFT_WORKS (the name
// proposed to the rules builder; change WORKS_EXPORT below if the export is called otherwise). What they check is read
// through the Arsenal's own offers(), so nothing else here depends on a name.
import { describe, expect, it } from 'vitest';
import { airFleetsOf } from '../../src/campaign/air';
import * as arsenal from '../../src/campaign/arsenal';
import { offers } from '../../src/campaign/arsenal';
import { turnToDate } from '../../src/core/ticket';
import { majorsOf } from '../../src/campaign/diplomacy';
import { createGame, setWar } from '../../src/campaign/game';
import { startOpen } from '../../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const WORKS_EXPORT = 'AIRCRAFT_WORKS';
const waiting = (arsenal as unknown as Record<string, unknown>)[WORKS_EXPORT] === undefined;

/** The ruled list: the month each kind's works opens, null for never. */
const LIST: Record<string, { ftr: number | null; bmr: number | null }> = {
  POL: { ftr: 1, bmr: 1 }, NED: { ftr: 1, bmr: 1 }, BEL: { ftr: 1, bmr: 1 }, YUG: { ftr: 1, bmr: 1 }, ROM: { ftr: 1, bmr: 1 },
  CAN: { ftr: 6, bmr: 3 }, SUI: { ftr: 1, bmr: 37 }, FIN: { ftr: 1, bmr: 21 }, HUN: { ftr: 13, bmr: 49 }, SWE: { ftr: 50, bmr: 1 },
  AUS: { ftr: 35, bmr: 21 }, ESP: { ftr: 1, bmr: null },
};
/** Smaller powers with no works at all, among them the two struck from the list. */
const NONE = ['BRA', 'MEX', 'ARG', 'TUR', 'IRN', 'NOR', 'GRE', 'BUL', 'POR', 'CHL', 'PER', 'VEN', 'IRQ', 'DEN', 'IRL', 'SAF', 'NZL', 'MAN', 'THA', 'CHN'];
const KIND = { ftr: 'fighters', bmr: 'bombers' } as const;

function grand(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
/** The reason the Arsenal gives beside an order of aircraft, or null where it can be ordered. */
const why = (g: Game, nat: string, wing: 'ftr' | 'bmr'): string | null => offers(g, nat).find((o) => o.key === `air:${wing}`)!.why;
/** A reason that is about the works, as against money or airfields. */
const WORKS_WORDS = /aircraft industry|builds no |builds (fighters|bombers) at home, but no|come only from abroad/;
const on = (g: Game, n: string): boolean => !!g.nations[n]?.alive;
const aircraft = (g: Game, nat: string, kinds: ('ftr' | 'bmr')[] = ['ftr', 'bmr']): number => airFleetsOf(g, nat).reduce((s, f) => s + kinds.reduce((t, k) => t + f[k], 0), 0);

describe.skipIf(waiting)('ruling 213(d) aircraft works: the twelve on the list open by the month, and stay open', () => {
  it('before its month a works is shut, and the sentence names the month (case 4, which comes before cases 2 and 3)', () => {
    const g = grand();
    for (const [nat, list] of Object.entries(LIST)) {
      if (!on(g, nat)) continue;
      for (const wing of ['ftr', 'bmr'] as const) {
        const from = list[wing];
        if (from === null || from <= 1) continue;
        g.turn = from - 1;
        expect(why(g, nat, wing), `${nat} ${wing} in month ${from - 1}`).toBe(
          `${g.nations[nat].name} builds no ${KIND[wing]} before ${turnToDate(from).label}: until then more ${KIND[wing]} come only from abroad.`);
        g.turn = from;
        expect(why(g, nat, wing) ?? '', `${nat} ${wing} in month ${from}`).not.toMatch(WORKS_WORDS);
      }
    }
  });

  it('the models of the words, in full', () => {
    const g = grand();
    g.turn = 20;
    expect(why(g, 'SWE', 'ftr')).toBe('Sweden builds no fighters before October 1943: until then more fighters come only from abroad.');
    expect(why(g, 'SWE', 'bmr') ?? '').not.toMatch(WORKS_WORDS);
    expect(why(g, 'ESP', 'bmr')).toBe('Spain builds fighters at home, but no bombers: more bombers come only from abroad.');
    expect(why(g, 'ESP', 'ftr') ?? '').not.toMatch(WORKS_WORDS);
    expect(why(g, 'IRN', 'ftr')).toBe('Iran has no aircraft industry: more aircraft come only from abroad.');
    expect(why(g, 'IRN', 'bmr')).toBe('Iran has no aircraft industry: more aircraft come only from abroad.');
    // Hungary is not case 2: its bombers come in September 1943.
    expect(why(g, 'HUN', 'bmr')).toBe('Hungary builds no bombers before September 1943: until then more bombers come only from abroad.');
    if (on(g, 'AUS')) expect(why(g, 'AUS', 'ftr')).toBe('Australia builds no fighters before July 1942: until then more fighters come only from abroad.');
  });

  it('a works on the list from the first day is open in month 1, and the factory count no longer decides', () => {
    const g = grand();
    for (const nat of ['POL', 'NED', 'BEL', 'YUG', 'ROM']) {
      if (!on(g, nat)) continue;
      // Every factory gone: the works is a fact of the record, not of the count.
      for (const r of g.regions) if (r.owner === nat) r.fac = 0;
      expect(why(g, nat, 'ftr') ?? '', nat).not.toMatch(WORKS_WORDS);
      expect(why(g, nat, 'bmr') ?? '', nat).not.toMatch(WORKS_WORDS);
    }
  });

  it('a works once open stays open while the country stands, all through the war', () => {
    const g = grand();
    for (const month of [1, 13, 37, 49, 50, 60, 72]) {
      g.turn = month;
      for (const [nat, list] of Object.entries(LIST)) {
        if (!on(g, nat)) continue;
        for (const wing of ['ftr', 'bmr'] as const) {
          const from = list[wing];
          if (from !== null && month >= from) expect(why(g, nat, wing) ?? '', `${nat} ${wing} in month ${month}`).not.toMatch(WORKS_WORDS);
        }
      }
    }
  });
});

describe.skipIf(waiting)('ruling 213(d) aircraft works: smaller powers off the list build none, whatever their factories', () => {
  it('no works, both wings, any month: the one sentence, struck Turkey and Argentina included', () => {
    const g = grand();
    for (const month of [1, 24, 60]) {
      g.turn = month;
      for (const nat of NONE) {
        if (!on(g, nat)) continue;
        const line = `${g.nations[nat].name} has no aircraft industry: more aircraft come only from abroad.`;
        expect(why(g, nat, 'ftr'), `${nat} fighters in month ${month}`).toBe(line);
        expect(why(g, nat, 'bmr'), `${nat} bombers in month ${month}`).toBe(line);
      }
    }
  });

  it('a factory gives no works: Brazil with a home province of five factories still builds none', () => {
    const g = grand();
    const r = g.regions.find((x) => x.owner === 'BRA' && x.ctrl === 'BRA')!;
    r.fac = 5;
    expect(why(g, 'BRA', 'ftr')).toBe('Brazil has no aircraft industry: more aircraft come only from abroad.');
    expect(why(g, 'BRA', 'bmr')).toBe('Brazil has no aircraft industry: more aircraft come only from abroad.');
  });
});

describe.skipIf(waiting)('ruling 213(d) aircraft works: the great powers are unchanged', () => {
  it('a great power still needs a home province of 2 factories for fighters and 3 for bombers, in today\'s words', () => {
    const g = grand();
    const majors = majorsOf(g);
    expect(majors).toEqual(expect.arrayContaining(['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA']));
    for (const nat of majors) {
      expect(why(g, nat, 'ftr') ?? '', nat).not.toMatch(WORKS_WORDS);
      expect(why(g, nat, 'bmr') ?? '', nat).not.toMatch(WORKS_WORDS);
    }
    // Italy with no home province of more than one factory: the factory test, as today.
    for (const r of g.regions) if (r.owner === 'ITA') r.fac = Math.min(r.fac, 1);
    expect(why(g, 'ITA', 'ftr')).toBe('Needs an aircraft industry: a home province with 2 factories.');
    expect(why(g, 'ITA', 'bmr')).toBe('Needs an aircraft industry: a home province with 3 factories.');
  });
});

describe.skipIf(waiting)("ruling 213(d) aircraft works: the month a works opens, one line in its own player's news", () => {
  it('Canada as the player: its bombers open in November 1939 with one plain line, and nothing was said in September', () => {
    const g = grand('CAN');
    const line = 'Canada now builds bombers at home: Bomber wings can be ordered on the Arsenal page.';
    expect(g.headlines.some((h) => h.text === line)).toBe(false);
    nextMonth(g);
    nextMonth(g);
    expect(g.turn).toBe(3);
    const said = g.headlines.filter((h) => h.text === line);
    expect(said.length).toBe(1);
    expect(said[0].nat).toBe('CAN');
    expect(said[0].kind).not.toBe('war');
    // The fighters are not open yet, and are not announced.
    expect(g.headlines.some((h) => h.text.startsWith('Canada now builds fighters'))).toBe(false);
  });

  it('nobody else reads it: with Germany at the table no line is written for Canada\'s works', () => {
    const g = grand('GER');
    nextMonth(g);
    nextMonth(g);
    expect(g.turn).toBe(3);
    expect(g.headlines.some((h) => h.text.startsWith('Canada now builds'))).toBe(false);
  });

  it('a works that stands on the first day is not announced', () => {
    const g = grand('SWE');
    nextMonth(g);
    expect(g.headlines.some((h) => /^Sweden now builds/.test(h.text))).toBe(false);
  });
});

describe.skipIf(waiting)('ruling 213(d) aircraft works: dated rows bring the record\'s aircraft from abroad', () => {
  it('Bulgaria, which builds none, receives its row of month 6 from Germany, and nothing before it', () => {
    const g = grand('GER');
    const before: number[] = [];
    while (g.turn < 5) { before.push(aircraft(g, 'BUL')); nextMonth(g); }
    before.push(aircraft(g, 'BUL'));
    // Nothing comes before the row's month: Bulgaria has no works to build any.
    expect(new Set(before).size).toBe(1);
    nextMonth(g);
    nextMonth(g);
    expect(g.turn).toBe(7);
    // Two air points, at about 35 aircraft a point.
    const got = aircraft(g, 'BUL') - before[before.length - 1];
    expect(got).toBeGreaterThanOrEqual(50);
    expect(got).toBeLessThanOrEqual(90);
  }, 120_000);

  it('a row comes only while its giver is not at war with the receiver: Sweden\'s fighters of month 7 are America\'s', () => {
    const at7 = (war: boolean): number => {
      const g = grand('GER');
      if (war) setWar(g, 'USA', 'SWE');
      while (g.turn < 6) nextMonth(g);
      const was = aircraft(g, 'SWE', ['ftr']);
      nextMonth(g);
      nextMonth(g);
      expect(g.turn).toBe(8);
      // Sweden's own fighters come only from October 1943 (month 50): anything new before that came from abroad.
      return aircraft(g, 'SWE', ['ftr']) - was;
    };
    // At peace with America: month 7's row comes (with Italy's of month 8, if it is in by then).
    const peace = at7(false);
    expect(peace).toBeGreaterThan(0);
    // At war with America: month 7's row does not come; only Italy's of month 8 can.
    expect(at7(true)).toBeLessThan(peace);
  }, 120_000);
});
