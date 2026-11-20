// The sandbox (Brian, 19 November 2026: "i want this to be a sandbox to play and make your own history, not just
// play out the real one"). What-if starts set how the war begins; turning points let it go another way at the
// moments it could have. A campaign begun without it is the game exactly as it was.
import { describe, expect, it } from 'vitest';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, bloc, createGame, setWar } from '../src/campaign/game';
import { leash, setStop } from '../src/campaign/history';
import { lanesTo } from '../src/campaign/resources';
import { startOpen } from '../src/campaign/staff';
import {
  orderAnswer, orderSandbox, runTurnAI, sandboxCountryList, sandboxOffered, sandboxStartList, sandboxView, turningBook, turningPoints, whyNotAnswer, whyNotOrderSandbox,
} from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { WHATIF, carryOut, eveLines, whatIfMonth, whatIfOpening } from '../src/campaign/whatif';

const grand = (seed = 3, human: string | null = null, stop = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const world = (start: string, seed = 3, human: string | null = null, stop = 3, turns = true): Game => {
  const g = grand(seed, human, stop);
  expect(orderSandbox(g, start, {}, turns)).toBe(true);
  return g;
};
/** The state a war leaves behind, to compare two wars by. */
const state = (g: Game): string => JSON.stringify({
  turn: g.turn,
  regions: g.regions.map((r) => `${r.owner}/${r.ctrl}/${r.units.map((u) => `${u.nat}${u.divs}:${u.str.toFixed(3)}`).join(',')}`),
  nations: Object.values(g.nations).map((n) => `${n.id}:${n.alive}:${n.align}:${[...n.atWar].sort().join('.')}`),
  news: g.headlines.map((h) => h.text),
  flags: Object.keys(g.flags).sort().map((k) => `${k}=${g.flags[k]}`),
});
const months = (g: Game, n: number): Game => { for (let i = 0; i < n; i++) runTurnAI(g); return g; };
const at = (g: Game, turn: number): Game => { g.turn = turn; return g; };

describe('the sandbox switch', () => {
  it('is off on the new-campaign card for release 1', () => {
    expect(sandboxOffered()).toBe(false);
  });

  it('history\'s start without turning points sets only the Sandbox switch, and its own rules replay the war exactly', () => {
    for (const seed of [4, 11]) {
      const plain = grand(seed);
      plain.flags.sandbox = 1; // the one switch, as the start card sets it
      months(plain, 6);
      const g = grand(seed);
      expect(orderSandbox(g, 'history', {}, false)).toBe(true);
      expect(g.flags.sandbox).toBe(1);
      expect(Object.keys(g.flags).filter((k) => k.startsWith('wi') || k.startsWith('tp:'))).toEqual([]);
      expect(sandboxView(g)).toBeNull();
      expect(state(months(g, 6))).toBe(state(plain));
    }
  });

  it('every sandbox world turns on the one Sandbox switch', () => {
    for (const start of ['nopact', 'grand', 'dealt']) expect(world(start).flags.sandbox).toBe(1);
    expect(grand().flags.sandbox).toBeUndefined();
  });

  it('belongs to the first day of a grand campaign', () => {
    expect(whyNotOrderSandbox(createGame(3, 'GER'), 'nopact')).toMatch(/grand campaign/);
    const g = grand();
    expect(whyNotOrderSandbox(g, 'nowhere')).toMatch(/no such start/);
    expect(whyNotOrderSandbox(g, 'custom', { SOV: 'enemies' })).toMatch(/no side/);
    expect(whyNotOrderSandbox(g, 'custom', { JAP: 'axis' })).toMatch(/no choice of side/);
    months(g, 1);
    expect(whyNotOrderSandbox(g, 'nopact')).toMatch(/first day/);
    const once = world('nopact');
    expect(whyNotOrderSandbox(once, 'grand')).toMatch(/has its world/);
  });

  it('offers ten starts and five swing powers, every side told in words', () => {
    const starts = sandboxStartList();
    expect(starts[0].id).toBe('history');
    expect(starts.length).toBe(10);
    expect(starts.some((s) => s.dealt) && starts.some((s) => s.custom)).toBe(true);
    const countries = sandboxCountryList();
    expect(countries.map((c) => c.nat)).toEqual(['SOV', 'ITA', 'USA', 'ESP', 'TUR']);
    for (const c of countries) {
      expect(c.sides[0].id).toBe('history');
      for (const s of c.sides) expect(s.says.length).toBeGreaterThan(40);
    }
    // Every side a start names is one its country has.
    for (const s of starts) for (const [nat, id] of Object.entries(s.sides)) expect(countries.find((c) => c.nat === nat)!.sides.some((x) => x.id === id)).toBe(true);
  });
});

describe('how the war begins', () => {
  it('No Pact: the Red Army stays home, Poland is Germany\'s alone, and Moscow sends no oil', () => {
    const g = world('nopact', 5);
    expect(lanesTo(g, 'GER').find((l) => l.id === 'pact')?.state).toBe('idle');
    months(g, 4);
    expect(g.flags.sovietMove).toBeUndefined();
    expect(g.regions.filter((r) => r.owner === 'POL' && r.ctrl === 'SOV')).toEqual([]);
    expect(g.nations.POL.alive).toBe(false);
    // The Baltic and Bessarabia were the Pact's secret terms: they are not in this war.
    at(g, 11);
    monthlyDiplomacy(g);
    expect(g.flags['event:baltic']).toBeUndefined();
    expect(g.nations.EST.alive).toBe(true);
    // In history's world the same war hands the Red Army its zone.
    const h = months(grand(5), 4);
    expect(h.flags.sovietMove).toBe(1);
  });

  it('The Grand Alliance: Moscow is with the Allies and at war with Germany on the first day', () => {
    const g = world('grand');
    expect(bloc(g.nations.SOV)).toBe('allies');
    expect(atWar(g, 'SOV', 'GER')).toBe(true);
    expect(g.headlines.at(-1)!.head).toBe('The Grand Alliance');
    expect(leash(g, 'SOV', 'west').kind).toBe('never');
  });

  it('Partners in Conquest: the Soviet Union stands in the Axis, and Germany\'s staff will not march east', () => {
    const g = world('partners');
    expect(bloc(g.nations.SOV)).toBe('axis');
    expect(leash(g, 'GER', 'east').kind).toBe('never');
    expect(leash(g, 'GER', 'west').kind).toBe('free');
    // The Pact's share of Poland still goes to Moscow.
    months(g, 3);
    expect(g.flags.sovietMove).toBe(1);
  });

  it('The Arsenal Sails Early: America is with the Allies and at war with Germany on the first day', () => {
    const g = world('arsenal');
    expect(bloc(g.nations.USA)).toBe('allies');
    expect(atWar(g, 'USA', 'GER')).toBe(true);
  });

  it('America First: Pearl Harbor does not bring America into Europe\'s war', () => {
    const g = at(world('firstly'), 28);
    const h = at(grand(), 28);
    monthlyDiplomacy(h);
    expect(atWar(h, 'USA', 'GER')).toBe(true);
    monthlyDiplomacy(g);
    expect(g.flags['event:pearl-harbor-world']).toBeUndefined();
    expect(atWar(g, 'USA', 'GER')).toBe(false);
  });

  it('Italy Sits It Out: Rome is out of the war and its staff takes none of the Axis\'s great choices', () => {
    const g = world('sitout', 3, null, 1);
    expect(bloc(g.nations.ITA)).toBeNull();
    for (const op of ['jackal', 'late', 'greece']) expect(leash(g, 'ITA', op).kind).toBe('never');
    // By the book Italy, once out, stays out: Rome's moment is history's answer at every turn of it.
    for (const t of [9, 15, 21]) { at(g, t); whatIfMonth(g); }
    expect(g.nations.ITA.atWar).toEqual([]);
    expect(g.flags['tp:rome:waits']).toBe(3);
  });

  it('The Mediterranean Ablaze: Italy at war on the first day, and Spain marches in the spring', () => {
    const g = world('ablaze');
    expect(atWar(g, 'ITA', 'UK') && atWar(g, 'ITA', 'FRA')).toBe(true);
    expect(bloc(g.nations.ESP)).toBe('axis');
    expect(g.nations.ESP.atWar).toEqual([]);
    at(g, 7); whatIfMonth(g);
    expect(g.nations.ESP.atWar).toEqual([]);
    at(g, 8); whatIfMonth(g);
    expect(atWar(g, 'ESP', 'UK') && atWar(g, 'ESP', 'FRA')).toBe(true);
    expect(g.headlines.some((h) => /Spain declares war on Britain/.test(h.text))).toBe(true);
  });

  it('A World Dealt Anew: the same seed deals the same world, and history\'s side is the likeliest', () => {
    const deal = (seed: number): string => (sandboxView(world('dealt', seed))?.sides.map((s) => s.side.id).join(' ') ?? '');
    expect(deal(7)).toBe(deal(7));
    const worlds = new Set<string>(), hist: Record<string, number> = {};
    for (let s = 1; s <= 40; s++) {
      const v = sandboxView(world('dealt', s))!;
      worlds.add(v.sides.map((x) => x.side.id).join(' '));
      for (const x of v.sides) if (x.history) hist[x.nat] = (hist[x.nat] ?? 0) + 1;
    }
    expect(worlds.size).toBeGreaterThan(20);
    for (const nat of ['SOV', 'ITA', 'USA', 'ESP', 'TUR']) expect(hist[nat]).toBeGreaterThan(8);
  });

  it('Your Own World: each country where the player puts it', () => {
    const g = grand(3, 'GER');
    expect(orderSandbox(g, 'custom', { SOV: 'axis', ITA: 'allies', TUR: 'axis', USA: 'history' })).toBe(true);
    expect([bloc(g.nations.SOV), bloc(g.nations.ITA), bloc(g.nations.TUR)]).toEqual(['axis', 'allies', 'axis']);
    expect(atWar(g, 'ITA', 'GER')).toBe(true);
    expect(sandboxView(g)!.sides.filter((s) => !s.history).map((s) => s.nat)).toEqual(['SOV', 'ITA', 'TUR']);
    // Turkey in the Axis goes to war with the Soviet Union beside Germany, the month Germany does.
    at(g, 5); setWar(g, 'GER', 'SOV'); g.nations.SOV.align = 'allies'; whatIfMonth(g);
    expect(atWar(g, 'TUR', 'SOV')).toBe(true);
  });

  it('tells Poland\'s player the truth about the Red Army on the eve of war', () => {
    const lines = ['a', 'b', 'Keep out of the eastern provinces: the Red Army crosses under its pact.'];
    expect(eveLines(grand(3, 'POL'), 'POL', lines)).toEqual(lines);
    expect(eveLines(world('nopact', 3, 'POL'), 'POL', lines)[2]).toMatch(/no pact/);
    expect(eveLines(world('grand', 3, 'POL'), 'POL', lines)[2]).toMatch(/at war with Germany beside you/);
  });
});

describe('turning points', () => {
  /** Molotov in Berlin, put to the computer in November 1940 with the conditions as they stood. */
  const molotov = (seed: number, stop: number, human: string | null = null): Game => {
    const g = at(world('history', seed, human, stop), 14);
    whatIfMonth(g);
    return g;
  };

  it('by the book the computer always takes history\'s choice', () => {
    for (let s = 1; s <= 30; s++) {
      const g = molotov(s, 1);
      expect(turningBook(g).find((r) => r.id === 'molotov')).toMatchObject({ history: true, by: 'computer', turn: 14 });
      expect(bloc(g.nations.SOV)).toBeNull();
    }
  });

  it('at the loosest stop the computer often goes another way, and the dice are the war\'s own', () => {
    const signed = Array.from({ length: 40 }, (_, k) => molotov(k + 1, 5)).filter((g) => bloc(g.nations.SOV) === 'axis');
    expect(signed.length).toBeGreaterThan(8);
    expect(signed.length).toBeLessThan(28);
    for (const g of signed) {
      expect(leash(g, 'GER', 'east').kind).toBe('never');
      expect(g.headlines.some((h) => h.head === 'A place in the Pact' && /signs the Tripartite Pact/.test(h.text))).toBe(true);
    }
    expect(bloc(molotov(9, 5).nations.SOV)).toBe(bloc(molotov(9, 5).nations.SOV));
  });

  it('puts the choice to the player who leads one of its countries, in his own words', () => {
    const g = molotov(3, 3, 'GER');
    expect(g.flags['tp:molotov']).toBeUndefined();
    const [q] = turningPoints(g);
    expect(q).toMatchObject({ id: 'molotov', nat: 'GER', name: 'A place in the Pact' });
    expect(q.choices.map((c) => c.label)).toEqual(['Keep Moscow at arm\'s length', 'Offer Moscow a place in the Pact']);
    expect(g.headlines.at(-1)!.warn).toBe('A place in the Pact');
    expect(whyNotAnswer(g, 'molotov', 'nonsense')).toMatch(/not open/);
    expect(whyNotAnswer(g, 'london', 'fight')).toMatch(/not open/);
    // The Soviet player sees the Soviet words.
    expect(turningPoints(molotov(3, 3, 'SOV'))[0].choices.map((c) => c.label)).toEqual(['Turn Berlin\'s offer down', 'Sign the Tripartite Pact']);
  });

  it('an offer the player makes needs the other side\'s yes, given on the computer\'s weights', () => {
    const answers = Array.from({ length: 30 }, (_, k) => { const g = molotov(k + 1, 3, 'GER'); orderAnswer(g, 'molotov', 'sign'); return g; });
    const yes = answers.filter((g) => bloc(g.nations.SOV) === 'axis'), no = answers.filter((g) => bloc(g.nations.SOV) === null);
    expect(yes.length).toBeGreaterThan(5);
    expect(no.length).toBeGreaterThan(5);
    for (const g of no) {
      expect(turningBook(g).find((r) => r.id === 'molotov')).toMatchObject({ by: 'refused', history: true });
      expect(g.headlines.some((h) => /the talks end/.test(h.text))).toBe(true);
    }
    for (const g of yes) expect(turningBook(g).find((r) => r.id === 'molotov')).toMatchObject({ by: 'player', history: false });
  });

  it('a choice the player makes alone is carried out at once', () => {
    const g = molotov(3, 3, 'SOV');
    expect(orderAnswer(g, 'molotov', 'sign')).toBe(true);
    expect(bloc(g.nations.SOV)).toBe('axis');
    expect(turningPoints(g)).toEqual([]);
    expect(orderAnswer(g, 'molotov', 'fail')).toBe(false);
  });

  it('an unanswered question takes history\'s answer when the month ends', () => {
    const g = molotov(3, 5, 'GER');
    at(g, 15); whatIfMonth(g);
    expect(turningPoints(g)).toEqual([]);
    expect(turningBook(g).find((r) => r.id === 'molotov')).toMatchObject({ by: 'unanswered', history: true, turn: 15 });
    expect(bloc(g.nations.SOV)).toBeNull();
  });

  it('branches: a partnership signed may break later, and the way east opens again', () => {
    const g = molotov(3, 3, 'SOV');
    orderAnswer(g, 'molotov', 'sign');
    // Not before eight months have passed.
    at(g, 21); whatIfMonth(g);
    expect(turningPoints(g)).toEqual([]);
    at(g, 22); whatIfMonth(g);
    const [q] = turningPoints(g);
    expect(q.id).toBe('breaks');
    expect(q.choices.map((c) => c.id)).toEqual(['keep', 'turn']);
    orderAnswer(g, 'breaks', 'keep');
    expect(g.flags['tp:breaks:next']).toBe(26);
    at(g, 26); whatIfMonth(g);
    orderAnswer(g, 'breaks', 'turn');
    expect(bloc(g.nations.SOV)).toBeNull();
    expect(leash(g, 'GER', 'east').kind).toBe('free');
  });

  it('a truce leaves each where its armies stand, and takes the partners out with the principal', () => {
    const g = grand(3, null);
    g.nations.ROM.align = g.nations.FIN.align = 'axis';
    for (const x of ['GER', 'ROM', 'FIN']) setWar(g, x, 'SOV');
    setWar(g, 'UK', 'GER');
    g.byName.Kiev.ctrl = 'GER';
    carryOut(g, { truce: [['GER', 'SOV']], partners: true });
    expect(['GER', 'ROM', 'FIN'].some((x) => atWar(g, x, 'SOV'))).toBe(false);
    expect(g.byName.Kiev.ctrl).toBe('GER');
    expect(atWar(g, 'UK', 'GER')).toBe(true);
    // Never the player's own country, unless he made the choice.
    const p = grand(3, 'ROM');
    p.nations.ROM.align = 'axis';
    for (const x of ['GER', 'ROM']) setWar(p, x, 'SOV');
    carryOut(p, { truce: [['GER', 'SOV']], partners: true });
    expect([atWar(p, 'GER', 'SOV'), atWar(p, 'ROM', 'SOV')]).toEqual([false, true]);
  });

  it('an armistice in the west leaves the war in the east', () => {
    const g = grand(3, null);
    for (const x of ['SOV', 'USA']) setWar(g, x, 'GER');
    carryOut(g, { armistice: { with: 'GER', except: ['SOV'] }, partners: true });
    expect(['UK', 'FRA', 'USA', 'CAN'].some((x) => atWar(g, x, 'GER'))).toBe(false);
    expect(atWar(g, 'SOV', 'GER')).toBe(true);
  });

  it('a turning point whose first country has fallen is decided by the next one still standing', () => {
    const g = world('history', 3, null, 5);
    at(g, 56);
    g.flags['wi:f:plot'] = 55; g.flags['tp:plot:at'] = 55;
    if (!atWar(g, 'GER', 'USA')) setWar(g, 'GER', 'USA');
    g.nations.UK.alive = false;
    whatIfMonth(g);
    expect(g.flags['tp:terms']).toBeGreaterThan(0);
    expect(g.flags['tp:terms:by']).toBe(2); // the computer, for America
  });

  it('a peace for Britain is a peace for its dominions', () => {
    const g = grand(3, null);
    expect(atWar(g, 'CAN', 'GER')).toBe(true);
    carryOut(g, { peace: [['UK', 'GER']], dominions: true });
    expect([atWar(g, 'UK', 'GER'), atWar(g, 'CAN', 'GER')]).toEqual([false, false]);
  });

  it('a war declared on joining goes only to the joiner\'s side\'s enemies', () => {
    const g = grand(3, null);
    carryOut(g, { join: { TUR: 'allies' }, war: [['TUR', 'GER'], ['TUR', 'BUL']] });
    expect(atWar(g, 'TUR', 'GER')).toBe(true);
    expect(atWar(g, 'TUR', 'BUL')).toBe(false);
  });

  it('every turning point is well formed: history\'s choice, the dial\'s weights, words for each decider', () => {
    for (const t of WHATIF.turns) {
      expect(t.options.filter((o) => o.hist).length, t.id).toBe(1);
      for (const o of t.options) {
        expect(o.w.length, `${t.id}:${o.id}`).toBe(5);
        // By the book the computer takes history's choice.
        if (!o.hist) expect([o.w[0], o.w[1]], `${t.id}:${o.id}`).toEqual([0, 0]);
      }
      for (const nat of t.by) expect(t.options.some((o) => typeof o.label === 'string' || nat in o.label), `${t.id}:${nat}`).toBe(true);
      expect(t.ask.length).toBeGreaterThan(20);
    }
  });

  it('turns no page of the war nobody began it with', () => {
    const g = at(grand(3, 'GER'), 14);
    whatIfMonth(g);
    expect(turningPoints(g)).toEqual([]);
    expect(turningBook(g)).toEqual([]);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('tp:'))).toEqual([]);
  });
});

// The sandbox playtest (19 November 2026): history's 1940-41 calendar and America's December 1941 fired on history's
// month in every war at every stop. With the turning points on, they can come later or not at all, and America has
// roads of its own into the war.
describe('history\'s calendar in a war with turning points', () => {
  const fates = (g: Game): Record<string, number> => Object.fromEntries(WHATIF.calendar.rows.map((c) => [c.id, g.flags[`wi:cal:${c.id}`] ?? 0]));
  const through = (g: Game, a: number, b: number): Game => { for (let t = a; t <= b; t++) { at(g, t); whatIfMonth(g); } return g; };

  it('keeps history\'s months by the book, and in a war without turning points', () => {
    for (const seed of [1, 2, 3, 4, 5]) {
      const g = through(world('history', seed, null, 1), 9, 27);
      expect(Object.values(fates(g)).every((f) => f === 1), String(seed)).toBe(true);
      const cal = WHATIF.calendar.rows.map((c) => c.id);
      expect(Object.keys(g.flags).filter((k) => cal.some((id) => k === `wi:from:${id}`) || k.startsWith('wi:bar:ev:'))).toEqual([]);
      // (The Americas are only held until America's war is seen: by the book that is history's month, and theirs after it.)
      expect(g.flags['wi:from:mexico-war']).toBe(29);
    }
    const off = through(world('nopact', 3, null, 5, false), 9, 27);
    expect(Object.values(fates(off)).every((f) => f === 0)).toBe(true);
  });

  it('at the loose end deals each event on time, later or never, the same for the same seed', () => {
    const count = [0, 0, 0, 0];
    for (let seed = 1; seed <= 30; seed++) {
      const g = through(world('history', seed, null, 5), 9, 27), f = fates(g);
      expect(f, String(seed)).toEqual(fates(through(world('history', seed, null, 5), 9, 27)));
      for (const v of Object.values(f)) count[v]++;
      // Only an event a turning point has already taken out of the war (Germany holding back from America) goes undealt.
      for (const [id, v] of Object.entries(f)) if (!v) expect(g.flags[`wi:bar:ev:${id}`], `${seed} ${id}`).toBe(1);
    }
    expect(count[0]).toBeLessThan(15);
    expect(count[1]).toBeGreaterThan(count[2]);
    expect(count[2]).toBeGreaterThan(10);
    expect(count[3]).toBeGreaterThan(10);
  });

  it('an event that never comes is out of the war, and its news is told the month it would have happened', () => {
    let seen = 0;
    for (let seed = 1; seed <= 40 && !seen; seed++) {
      const g = world('history', seed, null, 5);
      through(g, 9, 9);
      if (g.flags['wi:cal:baltic'] !== 3) continue;
      seen = seed;
      expect(g.flags['wi:bar:ev:baltic']).toBe(1);
      g.nations.POL.alive = false;
      const before = g.headlines.length;
      through(g, 10, 10);
      expect(g.headlines.slice(before).some((h) => /Baltic states/.test(h.text))).toBe(true);
      monthlyDiplomacy(g);
      expect(g.flags['event:baltic']).toBeUndefined();
      expect(g.nations.EST.alive).toBe(true);
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('a country the player leads keeps history\'s calendar', () => {
    for (let seed = 1; seed <= 20; seed++) {
      const g = through(world('history', seed, 'SOV', 5), 9, 12);
      expect([g.flags['wi:cal:baltic'], g.flags['wi:cal:bessarabia']], String(seed)).toEqual([1, 1]);
    }
  });

  it('a December 1941 that never comes opens America\'s own road into the war', () => {
    let seen = 0;
    for (let seed = 1; seed <= 60 && !seen; seed++) {
      const g = through(world('history', seed, null, 5), 27, 27);
      if (g.flags['wi:cal:pearl-harbor-world'] !== 3) continue;
      seen = seed;
      expect(g.flags['wi:bar:ev:pearl-harbor-world']).toBe(1);
      expect(g.flags['wi:f:no-declaration']).toBe(27);
    }
    expect(seen).toBeGreaterThan(0);
  });
});

describe('America in a sandbox world', () => {
  it('has a mood for the whole war, drawn from its seed', async () => {
    const { moodOf } = await import('../src/campaign/whatif');
    const moods = Array.from({ length: 30 }, (_, k) => moodOf(world('nopact', k + 1), 'usa'));
    expect(Math.min(...moods)).toBeGreaterThanOrEqual(0.3);
    expect(Math.max(...moods)).toBeLessThanOrEqual(1.7);
    expect(new Set(moods.map((m) => m.toFixed(3))).size).toBeGreaterThan(20);
    expect(moodOf(world('nopact', 7), 'usa')).toBe(moodOf(world('nopact', 7), 'usa'));
  });

  it('may go to war before December 1941 at the loose end, and never in America First', () => {
    let early = 0;
    for (let seed = 1; seed <= 30; seed++) {
      const g = world('history', seed, null, 5);
      g.nations.FRA.alive = false;
      for (let t = 10; t <= 27; t++) { at(g, t); whatIfMonth(g); }
      if (atWar(g, 'USA', 'GER')) early++;
    }
    expect(early).toBeGreaterThan(0);
    for (let seed = 1; seed <= 10; seed++) {
      const g = world('firstly', seed, null, 5);
      g.nations.FRA.alive = false;
      g.flags['wi:f:no-declaration'] = 1;
      for (let t = 10; t <= 60; t++) { at(g, t); whatIfMonth(g); }
      expect(atWar(g, 'USA', 'GER'), String(seed)).toBe(false);
      expect(g.flags['tp:america-early:waits'] ?? 0).toBe(0);
    }
  });

  it('the Americas follow America into the war, as long after it as history had them', () => {
    const g = world('arsenal', 3, null, 1);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    // The month opens: America's war is dated from its declaration, before the month's diplomacy.
    at(g, 2); whatIfOpening(g);
    expect(g.flags['wi:war:USA:GER']).toBe(1);
    // By the book exactly history's lag: Central America at once (the first month there is), Mexico five months on,
    // Brazil eight. The declarations of 1945, made for a seat at the United Nations, keep history's months.
    expect(g.flags['wi:from:isthmus-war']).toBe(2);
    expect(g.flags['wi:from:mexico-war']).toBe(6);
    expect(g.flags['wi:from:brazil-war']).toBe(9);
    expect(g.flags['wi:from:americas-war-1945']).toBeUndefined();
    expect(g.flags['wi:from:argentina-war']).toBeUndefined();
    monthlyDiplomacy(g);
    expect(g.flags['event:isthmus-war']).toBe(2);
    expect(g.flags['event:brazil-war']).toBeUndefined();
    // And the war America started with has no December 1941 of its own to come or to miss.
    for (let t = 2; t <= 30; t++) { at(g, t); whatIfMonth(g); }
    expect(g.flags['wi:cal:pearl-harbor-world']).toBeUndefined();
    expect(g.flags['event:pearl-harbor-world']).toBeUndefined();
  });

  it("deals no December 1941 to an America already at war with Germany, and takes it back if America goes in that month", () => {
    const told = (g: Game): boolean => g.headlines.some((h) => h.text.startsWith('December 1941 passes'));
    // America went to war in 1941 by its own road: its Pearl Harbor keeps history's calendar, and no peace is announced.
    for (let seed = 1; seed <= 30; seed++) {
      const g = world('history', seed, null, 5);
      at(g, 20); setWar(g, 'USA', 'GER');
      for (let t = 26; t <= 30; t++) { at(g, t); whatIfMonth(g); }
      expect(g.flags['wi:cal:pearl-harbor-world'], String(seed)).toBe(1);
      expect(g.flags['wi:f:no-declaration']).toBeUndefined();
      expect(told(g)).toBe(false);
    }
    // Dealt 'never' in November 1941, then America goes to war that same month: no news of a peace, and the flag comes down.
    let seen = 0;
    for (let seed = 1; seed <= 40; seed++) {
      const g = world('history', seed, null, 5);
      at(g, 27); whatIfMonth(g);
      if (g.flags['wi:cal:pearl-harbor-world'] !== 3) continue;
      seen++;
      expect(g.flags['wi:f:no-declaration']).toBe(27);
      setWar(g, 'USA', 'GER');
      for (let t = 28; t <= 31; t++) { at(g, t); whatIfMonth(g); }
      expect(told(g), String(seed)).toBe(false);
      expect(g.flags['wi:f:no-declaration']).toBeUndefined();
    }
    expect(seen).toBeGreaterThan(0);
  });

  it("America's early entry takes Pearl Harbor out of the war, and makes no war on a neutral Italy", () => {
    const early = WHATIF.turns.find((t) => t.id === 'america-early')!.options.find((o) => o.id === 'enter')!;
    const g = world('sitout', 3, null, 5);
    at(g, 14);
    expect(carryOut(g, early.do!)).toBe(true);
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(atWar(g, 'USA', 'ITA')).toBe(false);
    expect(g.flags['wi:bar:ev:pearl-harbor-world']).toBe(1);
    // Asked again after America is already in (a stale question): Italy, still neutral, is left alone.
    expect(carryOut(g, early.do!)).toBe(true);
    expect(atWar(g, 'USA', 'ITA')).toBe(false);
  });

  it('a question the war has moved past cannot be answered', () => {
    const g = world('history', 3, 'USA', 3);
    at(g, 15); g.nations.FRA.alive = false; setWar(g, 'UK', 'GER');
    g.flags['tp:america-early:ask'] = 15;
    expect(whyNotAnswer(g, 'america-early', 'wait')).toBeNull();
    setWar(g, 'USA', 'GER');
    expect(whyNotAnswer(g, 'america-early', 'enter')).toMatch(/no longer stands/);
  });

  it("when America comes in late, the Americas follow it, not history's calendar", () => {
    // Pearl Harbor slips to month 33, Mexico's own month in history: Mexico must not join in the same month.
    const g = world('nopact', 2, null, 3);
    for (let t = 29; t <= 32; t++) { at(g, t); whatIfMonth(g); }
    expect(g.flags['wi:from:mexico-war']).toBe(34);
    expect(g.flags['wi:from:isthmus-war']).toBeUndefined();
    at(g, 33); setWar(g, 'USA', 'GER'); monthlyDiplomacy(g);
    expect(g.flags['event:isthmus-war']).toBe(33);
    expect(g.flags['event:mexico-war']).toBeUndefined();
    whatIfMonth(g);
    expect(g.flags['wi:war:USA:GER']).toBe(33);
    expect(g.flags['wi:from:mexico-war']).toBeGreaterThanOrEqual(37);
    expect(g.flags['wi:from:brazil-war']).toBeGreaterThanOrEqual(40);
  });
});
