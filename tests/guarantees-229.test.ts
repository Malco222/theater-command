// Ruling 229, part 3: a printed guarantee is kept. The declare-war card lists a small country's real guarantors only,
// from a one-way table (the historian's facts), each with what it does to the one who would attack; in the grand
// campaign a computer-run guarantor whose guarantee covers the attacker declares war on it the same day, and a player
// who leads a guarantor is told and never forced. A computer staff reads the card too. Greece's reserves count from
// its war's start, and Yugoslavia calls up twelve divisions over its war's second and third months.
import { describe, expect, it } from 'vitest';
import guar from '../data/campaign/guarantees.json';
import setup from '../data/campaign/setup-1939.json';
import { atWar, bloc, createGame, declareWar, setWar, startPhase } from '../src/campaign/game';
import { warForecast } from '../src/campaign/diplomacy';
import { guarantorsOf } from '../src/campaign/guarantees';
import { deterred } from '../src/campaign/league';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

const grand = (human: string | null = null, turn = 1): Game => { const g = startOpen(createGame(3, human)); g.turn = turn; return g; };
const fall = (g: Game, nat: string): void => { g.nations[nat].alive = false; g.nations[nat].fellOn = { turn: g.turn - 1, week: 2, to: 'GER', how: 'beaten' }; };
const newsSince = (g: Game, from: number) => g.headlines.slice(from);

describe('the guarantee table', () => {
  it('runs one way, from the great power to the small country, and Turkey stands by nobody', () => {
    for (const row of guar.rows) {
      expect(['UK', 'FRA']).toContain(row.by);
      expect(['POL', 'GRE', 'ROM', 'TUR', 'BEL', 'POR']).toContain(row.for);
      expect(row.tie).toMatch(/guarantor|ally since 1373/);
    }
    expect(guar.rows.filter((r) => r.by === 'FRA').map((r) => r.for).sort()).toEqual(['BEL', 'GRE', 'POL', 'ROM', 'TUR']);
    expect(guar.rows.filter((r) => r.for === 'ROM').every((r) => r.to === 10)).toBe(true);
    expect(guar.rows.filter((r) => r.consult).map((r) => r.for)).toEqual(['POL', 'POL']);
  });
});

describe('the card lists who stands by a country', () => {
  it('its real guarantors only, each with what it does to the one looking', () => {
    const g = grand('ITA');
    const gre = guarantorsOf(g, 'ITA', 'GRE');
    expect(gre.map((x) => x.nat)).toEqual(['UK', 'FRA']);
    expect(gre[0]).toEqual({ nat: 'UK', stands: 'comes', comes: true, why: "Greece's guarantor since April 1939: it declares war on you the same month." });
    expect(warForecast(g, 'ITA', 'GRE').guarantors).toEqual(gre);
    // The Balkan Entente comes off the card: nobody stands by Yugoslavia against Italy.
    expect(guarantorsOf(g, 'ITA', 'YUG')).toEqual([]);
    // Belgium's guarantee covered Germany alone.
    expect(guarantorsOf(g, 'ITA', 'BEL')[0].why).toBe("Belgium's guarantor against Germany since 1937: it is not bound to declare war on you.");
    // Poland's covered Germany alone, and bound Britain and France only to consult against anyone else.
    const pol = guarantorsOf(g, 'SOV', 'POL');
    expect(pol.map((x) => [x.nat, x.stands])).toEqual([['UK', 'consult'], ['FRA', 'consult']]);
    expect(pol[0].why).toBe("Poland's guarantor against Germany since August 1939: against you it would consult with Poland, not declare war.");
    expect(guarantorsOf(g, 'GER', 'POL')[0].why).toBe("Poland's guarantor against Germany since August 1939: it is at war with you already.");
    expect(guarantorsOf(g, 'ESP', 'POR').map((x) => x.nat)).toEqual(['UK']);
    // Turkey's treaty dates from October 1939, and covers a European power only.
    expect(guarantorsOf(g, 'SOV', 'TUR')).toEqual([]);
    g.turn = 2;
    expect(guarantorsOf(g, 'SOV', 'TUR').map((x) => x.stands)).toEqual(['comes', 'comes']);
    expect(guarantorsOf(g, 'USA', 'TUR').map((x) => x.stands)).toEqual(['free', 'free']);
  });

  it('ends where the record ends it: Romania in July 1940, France at its fall, and a country in the other camp', () => {
    const g = grand(null, 10);
    expect(guarantorsOf(g, 'SOV', 'ROM').map((x) => x.nat)).toEqual(['UK', 'FRA']);
    g.turn = 11;
    expect(guarantorsOf(g, 'SOV', 'ROM')).toEqual([]);
    g.turn = 8;
    fall(g, 'FRA');
    expect(guarantorsOf(g, 'SOV', 'ROM').map((x) => x.nat)).toEqual(['UK']);
    // A France freed later does not take its guarantees up again.
    g.nations.FRA.alive = true;
    expect(guarantorsOf(g, 'SOV', 'ROM').map((x) => x.nat)).toEqual(['UK']);
    g.nations.ROM.align = 'axis_cobelligerent';
    expect(guarantorsOf(g, 'SOV', 'ROM')).toEqual([]);
  });

  it('is empty in a historical scenario, which keeps the war it had, and in the last act', () => {
    expect(guarantorsOf(createGame(1, 'ITA'), 'ITA', 'GRE')).toEqual([]);
    const g = grand();
    g.flags.dom = 1;
    expect(guarantorsOf(g, 'ITA', 'GRE')).toEqual([]);
  });
});

describe('a guarantee kept', () => {
  it('brings Britain and France into Italy\'s war on Greece the same day, in one story', () => {
    const g = grand();
    expect(atWar(g, 'UK', 'ITA')).toBe(false);
    const from = g.headlines.length;
    declareWar(g, 'ITA', 'GRE');
    expect(atWar(g, 'UK', 'ITA')).toBe(true);
    expect(atWar(g, 'FRA', 'ITA')).toBe(true);
    expect(g.flags['war:UK:ITA']).toBe(g.turn);
    expect(g.flags['war:FRA:ITA']).toBe(g.turn);
    expect(bloc(g.nations.GRE)).toBe('allies');
    const story = newsSince(g, from).filter((h) => h.head?.includes('stand'));
    expect(story).toHaveLength(1);
    expect(story[0]).toMatchObject({ kind: 'war', head: 'Britain and France stand by Greece', text: 'Britain and France declare war on Italy: they keep the guarantee of April 1939 to Greece.' });
    // The same blow in a historical scenario brings nobody in.
    const h = createGame(1, null);
    declareWar(h, 'ITA', 'GRE');
    expect(atWar(h, 'UK', 'ITA')).toBe(false);
  });

  it('covers only whom it covered: the Soviet Union on Poland brings no war, on Romania before July 1940 it does', () => {
    const g = grand();
    declareWar(g, 'SOV', 'POL');
    expect(atWar(g, 'UK', 'SOV')).toBe(false);
    expect(atWar(g, 'FRA', 'SOV')).toBe(false);
    const r = grand(null, 8);
    declareWar(r, 'SOV', 'ROM');
    expect(atWar(r, 'UK', 'SOV')).toBe(true);
    expect(atWar(r, 'FRA', 'SOV')).toBe(true);
    // Britain and France declare nothing else by it: the Soviet Union stays outside both camps.
    expect(bloc(r.nations.SOV)).toBeNull();
    expect(r.headlines.some((h) => h.text === 'Britain and France declare war on the Soviet Union: they keep the guarantee of April 1939 to Romania.')).toBe(true);
    const late = grand(null, 11);
    declareWar(late, 'SOV', 'ROM');
    expect(atWar(late, 'UK', 'SOV')).toBe(false);
  });

  it('Portugal brings Britain alone; Belgium brings both against Germany only', () => {
    const g = grand();
    declareWar(g, 'ESP', 'POR');
    expect(atWar(g, 'UK', 'ESP')).toBe(true);
    expect(atWar(g, 'FRA', 'ESP')).toBe(false);
    expect(g.headlines.some((h) => h.head === 'Britain stands by Portugal' && h.text === 'Britain declares war on Spain: it keeps the alliance of 1373 with Portugal.')).toBe(true);
    const b = grand();
    declareWar(b, 'ITA', 'BEL');
    expect(atWar(b, 'UK', 'ITA')).toBe(false);
  });

  it('says nothing where the guarantor is at war already, and France comes alone where Britain is', () => {
    const g = grand();
    setWar(g, 'UK', 'ITA');
    const from = g.headlines.length;
    declareWar(g, 'ITA', 'GRE');
    const story = newsSince(g, from).filter((h) => h.head?.includes('stand'));
    expect(story.map((h) => h.head)).toEqual(['France stands by Greece']);
    expect(story[0].text).toBe('France declares war on Italy: it keeps the guarantee of April 1939 to Greece.');
    const w = grand();
    setWar(w, 'UK', 'ITA'); setWar(w, 'FRA', 'ITA');
    const at = w.headlines.length;
    declareWar(w, 'ITA', 'GRE');
    expect(newsSince(w, at).some((h) => h.head?.includes('stand'))).toBe(false);
  });

  it('never forces a player who leads a guarantor: he is told, and the Declare war card is his', () => {
    const g = grand('UK');
    const from = g.headlines.length;
    declareWar(g, 'ITA', 'GRE');
    expect(atWar(g, 'UK', 'ITA')).toBe(false);
    expect(atWar(g, 'FRA', 'ITA')).toBe(true);
    const news = newsSince(g, from);
    expect(news.find((h) => h.head === 'Greece calls on your guarantee')).toMatchObject({ kind: 'info', nat: 'UK', text: 'Italy has attacked Greece, which you guaranteed in April 1939. Whether to declare war on Italy is yours to decide, on the Declare war card.' });
    expect(news.some((h) => h.head === 'France stands by Greece')).toBe(true);
    // An attack his guarantee does not cover tells him nothing.
    const p = grand('UK');
    const at = p.headlines.length;
    declareWar(p, 'SOV', 'POL');
    expect(newsSince(p, at).some((h) => /calls on your guarantee/.test(h.head ?? ''))).toBe(false);
  });

  it('is read by a computer staff before it opens an operation, as the League is', () => {
    const g = grand(null, 8);
    expect(deterred(g, 'SOV', ['ROM'])).toBe(guar.deter);
    expect(deterred(g, 'ITA', ['YUG'])).toBe(1);
    g.turn = 11;
    expect(deterred(g, 'SOV', ['ROM'])).toBe(1);
    expect(deterred(createGame(1, null), 'ITA', ['GRE'])).toBe(1);
  });
});

describe('the Balkan armies counted from the war\'s start', () => {
  const rows = (setup as any).expeditions as any[][];
  const greek = rows.map((row, i) => ({ row, i })).filter(({ row }) => row[1] === 'GRE');
  const yugo = rows.map((row, i) => ({ row, i })).filter(({ row }) => row[1] === 'YUG');

  it('Greece\'s reserves come over its war\'s first three months, however early the war', () => {
    expect(greek.map(({ row }) => row[6].after)).toEqual([0, 0, 1, 2]);
    expect(greek.reduce((s, { row }) => s + row[4], 0)).toBe(9);
    const g = grand(null, 6);
    declareWar(g, 'ITA', 'GRE');
    const arrived: number[][] = [];
    for (const t of [6, 7, 8]) {
      g.turn = t;
      startPhase(g, 'GRE');
      arrived.push(greek.filter(({ i }) => g.flags[`sent:${i}`] === 1).map(({ i }) => i));
    }
    expect(arrived.map((a) => a.length)).toEqual([2, 3, 4]);
    expect(greek.map(({ i }) => g.flags[`due:${i}`])).toEqual([6, 6, 7, 8]);
    expect(g.corps.some((c) => c.nat === 'GRE' && c.name === 'III Corps' && !c.dead)).toBe(true);
  });

  it('Yugoslavia calls up twelve divisions over its war\'s second and third months, in the grand campaign only', () => {
    expect(yugo.map(({ row }) => [row[2], row[4], row[6].after])).toEqual([['Sixth Army', 3, 1], ['Third Army', 3, 1], ['Seventh Army', 3, 2], ['Coastal Army', 3, 2]]);
    for (const { row } of yugo) expect(row[6]).toMatchObject({ open: 1, xp: 0.05, war: ['YUG', '*'] });
    const g = grand(null, 6);
    declareWar(g, 'ITA', 'YUG');
    const count = (): number => g.corps.filter((c) => c.nat === 'YUG' && !c.dead).length;
    const start = count();
    g.turn = 6; startPhase(g, 'YUG');
    expect(count()).toBe(start);
    g.turn = 7; startPhase(g, 'YUG');
    expect(count()).toBe(start + 2);
    g.turn = 8; startPhase(g, 'YUG');
    expect(count()).toBe(start + 4);
    // A historical scenario keeps the Yugoslav army it was measured with.
    const h = createGame(1, null);
    h.turn = 6;
    setWar(h, 'ITA', 'YUG');
    const before = h.corps.filter((c) => c.nat === 'YUG' && !c.dead).length;
    for (const t of [7, 8]) { h.turn = t; startPhase(h, 'YUG'); }
    expect(h.corps.filter((c) => c.nat === 'YUG' && !c.dead).length).toBe(before);
  });

  it('a country overrun before its call-up was ever counted never has it: freed, it comes back with its liberation corps alone (2b)', () => {
    const g = grand(null, 6);
    declareWar(g, 'ITA', 'YUG');
    fall(g, 'YUG');
    g.nations.YUG.alive = true;
    const count = (): number => g.corps.filter((c) => c.nat === 'YUG' && !c.dead).length;
    const start = count();
    for (const t of [7, 8, 9]) { g.turn = t; startPhase(g, 'YUG'); }
    expect(count()).toBe(start);
    for (const { i } of yugo) expect(g.flags[`sent:${i}`]).toBe(2);
    // One counted before its country fell keeps the months it was given.
    const k = grand(null, 6);
    declareWar(k, 'ITA', 'YUG');
    k.turn = 6; startPhase(k, 'YUG');
    fall(k, 'YUG');
    k.nations.YUG.alive = true;
    k.turn = 7; startPhase(k, 'YUG');
    expect(yugo.filter(({ i }) => k.flags[`sent:${i}`] === 1)).toHaveLength(2);
  });
});
