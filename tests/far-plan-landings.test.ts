// After the strike's day a plan's landing reads what the staff's own hops read (rider b644 (1) to ruling 234; the
// design director's ruling of 18:53Z and her note of 20:31Z, 19 November 2026; `plansAsk` in src/campaign/ai.ts).
//
// The plan's test (1.2 against the corps in the field, read on the quay, no reading of the sea) is what the day itself
// must give to carry the ground: over the all-in's 24 wars 109 of the plan's 238 follow-ons were thrown back, and none
// of the staff's own 94 hops, which ask 1.5 and a crossing no riskier than 0.25. So from the month after the plan
// went its own landings and its follow-ons on the Pacific's ground ask what the hops ask, and no number is new. The
// strike's own day stays on the plan's test. To reach the ask a front sends the rows it lists for a shore together.
// The states are set by hand on the first day's chart; one month of Japan's own is played. No war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { frontWatch, runAI } from '../src/campaign/ai';
import { addCorps, assess, createGame, declareWar } from '../src/campaign/game';
import { landingOpts, planLanding } from '../src/campaign/navy';
import { followKey, frontsClear, sailedKey } from '../src/campaign/planfronts';
import { descentAsks, ops, startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

const D48 = '14th Army (48th Division)';
// (The 38th Division's row for Ambon: its 228th Regiment, by ruling 234's notes of 22:05Z and 22:10Z of 20 November 2026.)
const R228 = '228th Infantry Regiment (the Ito Detachment)';
const SAKAGUCHI = 'Sakaguchi Detachment (56th Mixed Infantry Group)';
const KURE = 'Naval landing forces (Kure 1st and 2nd)';
const INDIES = 4; // the Indies front in the plan's list
const SOUTH = () => ops('JAP').find((op) => op.id === 'south')!;

/** A war at stop 3: the strike south went in month `went`, and it is now month `turn`. */
const war = (turn: number, went = 28): Game => {
  const g = startOpen(createGame(3, null));
  g.turn = went;
  g.flags['st:done:JAP:south'] = went;
  for (const e of ['USA', 'UK', 'NED']) declareWar(g, 'JAP', e);
  g.turn = turn;
  return g;
};
const put = (g: Game, nat: string, name: string, place: string, str = 1, hold = false): Corps => {
  const c = addCorps(g, nat, name, 'inf', 1, place, { xp: 0.3, hold });
  c.str = str;
  return c;
};
/** A place in Japan's hands, with nobody of the other side in it. */
const japans = (g: Game, name: string): void => {
  const r = g.byName[name];
  r.ctrl = 'JAP';
  r.units = r.units.filter((u) => u.nat === 'JAP');
};
/** A shore held by one corps of the Dutch in the field, and nobody else. */
const held = (g: Game, name: string, str: number): void => {
  const r = g.byName[name];
  r.units = [];
  put(g, 'NED', `${name} garrison`, name, str, true);
};
const reads = (g: Game, cs: Corps[], to: string): number => assess(g, cs, g.byName[to], landingOpts(g, cs, g.byName[to])).odds;
/** Sets a force's strength so that it reads `want` on a defended shore, as a landing reads it; answers the reading. */
const tune = (g: Game, cs: Corps[], to: string, want: number): number => {
  let lo = 0.01, hi = 1;
  for (let i = 0; i < 40; i++) {
    const m = (lo + hi) / 2;
    for (const c of cs) c.str = m;
    if (reads(g, cs, to) < want) lo = m; else hi = m;
  }
  for (const c of cs) c.str = hi;
  return reads(g, cs, to);
};
const where = (g: Game, c: Corps): string => g.regions[c.reg].name;

type Told = { what: string; front: string; force: string[]; to: string; from: string; taken?: boolean; why?: string };
const told: Told[] = [];
const listen = (): void => { told.length = 0; frontWatch.tell = (e) => { told.push({ what: e.what, front: e.front, force: e.force.map((c) => c.name), to: e.to.name, from: e.from.name, taken: e.taken, why: e.why }); }; };
const of = (name: string): Told[] => told.filter((t) => t.force[0] === name);
const oddsTold = (t: Told | undefined): number => Number(/^the odds, ([\d.]+)$/.exec(t?.why ?? '')?.[1] ?? NaN);
afterEach(() => { frontWatch.tell = null; frontsClear(); });

/** The Sakaguchi Detachment at Davao, its own landing made and Jolo taken: Tarakan is its next, held by the Dutch. */
const tarakan = (turn: number, want: number): { g: Game; sak: Corps; read: number } => {
  const g = war(turn);
  japans(g, 'Davao');
  japans(g, 'Jolo');
  held(g, 'Tarakan', 0.3);
  const sak = put(g, 'JAP', SAKAGUCHI, 'Davao');
  g.flags[sailedKey(sak)] = 28;
  return { g, sak, read: tune(g, [sak], 'Tarakan', want) };
};

describe("a plan's landing after the strike's day reads what the staff's own hops read (rider b644 (1) to ruling 234)", () => {
  it('asks the two figures the hops ask, and no number of its own', () => {
    expect(descentAsks()).toEqual({ odds: 1.5, risk: 0.25 });
    expect(SOUTH().fronts![INDIES].id).toBe('indies');
    expect(SOUTH().fronts![INDIES].then!.every((f) => f.minOdds === undefined)).toBe(true);
  });

  it('a follow-on that reads between the plan\'s 1.2 and the staff\'s 1.5 waits, stays owed, and its row is still the plan\'s', () => {
    const { g, sak, read } = tarakan(29, 1.35);
    expect(read).toBeGreaterThan(1.3);
    expect(read).toBeLessThan(1.4);
    expect(planLanding(g, [sak], g.byName.Tarakan).why).toBeNull();
    listen();
    runAI(g, 'JAP');
    const mine = of(SAKAGUCHI);
    expect(mine.map((t) => [t.what, t.to])).toEqual([['wait', 'Tarakan']]);
    expect(oddsTold(mine[0])).toBeGreaterThanOrEqual(1.2);
    expect(oddsTold(mine[0])).toBeLessThan(1.5);
    expect([where(g, sak), sak.hold, g.byName.Tarakan.ctrl]).toEqual(['Davao', true, 'NED']);
    expect(g.flags[followKey('JAP', INDIES, 0)]).toBeUndefined();
  });

  it('the same follow-on sails once it reads 1.5', () => {
    const { g, read } = tarakan(29, 1.6);
    expect(read).toBeGreaterThanOrEqual(1.5);
    listen();
    runAI(g, 'JAP');
    expect(of(SAKAGUCHI).filter((t) => t.what === 'land').map((t) => [t.from, t.to])[0]).toEqual(['Davao', 'Tarakan']);
  });

  it('a shore with nobody in its field and no guard in its town is walked into as before', () => {
    const g = war(29);
    japans(g, 'Davao');
    japans(g, 'Jolo');
    g.byName.Tarakan.units = [];
    const sak = put(g, 'JAP', SAKAGUCHI, 'Davao', 0.1);
    g.flags[sailedKey(sak)] = 28;
    listen();
    runAI(g, 'JAP');
    expect(of(SAKAGUCHI).filter((t) => t.what === 'land').map((t) => [t.to, t.taken])[0]).toEqual(['Tarakan', true]);
  });

  it('a follow-on waits on a crossing riskier than the staff\'s own figure, whatever it reads at the beach', () => {
    const g = war(29);
    for (const nm of ['Davao', 'Jolo', 'Tarakan']) japans(g, nm);
    // (Nobody of Japan's covers the crossing: its fleets are taken off the chart.)
    g.fleets = g.fleets.filter((f) => f.nat !== 'JAP');
    // (The far side's one measure counts of the Dutch East Indies Fleet's fifteen submarines the boats out alone, and
    // the first day's chart then reads this crossing at 0.24, under the staff's figure. Four cruisers more are set in
    // that fleet by hand, so that the crossing is the riskier one this test is about.)
    g.fleets.find((f) => f.nat === 'NED' && f.name === 'Dutch East Indies Fleet')!.ships.ca += 4;
    held(g, 'Balikpapan', 0.1);
    const sak = put(g, 'JAP', SAKAGUCHI, 'Davao');
    g.flags[sailedKey(sak)] = 28;
    g.flags[followKey('JAP', INDIES, 0)] = 28;
    expect(reads(g, [sak], 'Balikpapan')).toBeGreaterThan(1.5);
    const before = planLanding(g, [sak], g.byName.Balikpapan);
    expect(before.why).toBeNull();
    expect(before.risk).toBeGreaterThan(0.25);
    listen();
    runAI(g, 'JAP');
    const mine = of(SAKAGUCHI);
    expect(mine.map((t) => [t.what, t.to])).toEqual([['wait', 'Balikpapan']]);
    expect(mine[0].why).toMatch(/^the crossing's risk, 0\.\d\d$/);
    expect(Number(mine[0].why!.slice(-4))).toBeGreaterThan(0.25);
    expect([where(g, sak), g.byName.Balikpapan.ctrl]).toEqual(['Davao', 'NED']);
  });

  /** Kure's landing forces and the 38th Division's 228th Regiment, both listed for Ambon, the hops before it made; each reads 1.4 alone.
   * (The garrison is a strong one, so that the rows are no remnants for their own staff to fold together.) */
  const ambon = (kureAt: string): { g: Game; kure: Corps; d38: Corps } => {
    const g = war(29);
    for (const nm of ['Davao', 'Jolo', 'Kendari', 'Hong Kong']) japans(g, nm);
    held(g, 'Ambon', 0.8);
    const kure = put(g, 'JAP', KURE, kureAt), d38 = put(g, 'JAP', R228, 'Davao');
    g.flags[sailedKey(kure)] = 28;
    tune(g, [kure], 'Ambon', 1.4);
    tune(g, [d38], 'Ambon', 1.4);
    return { g, kure, d38 };
  };

  it('to reach the ask a front sends the rows it lists for a shore together, where they stand on one quay', () => {
    const { g, kure, d38 } = ambon('Davao');
    expect(reads(g, [kure], 'Ambon')).toBeLessThan(1.5);
    expect(reads(g, [d38], 'Ambon')).toBeLessThan(1.5);
    expect(reads(g, [kure, d38], 'Ambon')).toBeGreaterThan(2.5);
    listen();
    runAI(g, 'JAP');
    const lands = told.filter((t) => t.what === 'land');
    expect(lands.map((t) => [t.force, t.from, t.to])).toEqual([[[KURE, R228], 'Davao', 'Ambon']]);
    // Ashore together, each has made its own follow-on for Ambon; neither waited on the odds.
    expect(lands[0].taken).toBe(true);
    expect([g.flags[followKey('JAP', INDIES, 4)], g.flags[followKey('JAP', INDIES, 5)]]).toEqual([29, 29]);
    expect(told.filter((t) => t.what === 'wait' && t.to === 'Ambon')).toEqual([]);
    expect([where(g, kure), where(g, d38), g.byName.Ambon.ctrl]).toEqual(['Ambon', 'Ambon', 'JAP']);
  });

  it('a row listed for the shore later in the table comes to the first row\'s quay, where neither reads it alone', () => {
    const { g, kure, d38 } = ambon('Jolo');
    listen();
    runAI(g, 'JAP');
    expect(of(KURE).map((t) => [t.what, t.to])).toEqual([['wait', 'Ambon']]);
    expect(oddsTold(of(KURE)[0])).toBeLessThan(1.5);
    expect(of(R228).map((t) => [t.what, t.from, t.to])).toEqual([['stage', 'Davao', 'Jolo']]);
    expect([where(g, kure), where(g, d38), g.byName.Ambon.ctrl]).toEqual(['Jolo', 'Jolo', 'NED']);
    expect([g.flags[followKey('JAP', INDIES, 4)], g.flags[followKey('JAP', INDIES, 5)]]).toEqual([undefined, undefined]);
  });

  it('a later row that reads the ask alone is not called away: it lands by itself', () => {
    const { g, d38 } = ambon('Jolo');
    tune(g, [d38], 'Ambon', 1.7);
    listen();
    runAI(g, 'JAP');
    expect(of(KURE).map((t) => [t.what, t.to])).toEqual([['wait', 'Ambon']]);
    expect(of(R228).filter((t) => t.what !== 'wait').map((t) => [t.what, t.from, t.to])).toEqual([['land', 'Davao', 'Ambon']]);
  });

  /** The 48th Division on its quay on Formosa with Lingayen held, as the plan's own landing reads it. */
  const lingayen = (turn: number, want: number): { g: Game; d48: Corps; read: number } => {
    const g = war(turn);
    const d48 = put(g, 'JAP', D48, 'Formosa', 1, true);
    const r = g.byName.Lingayen;
    r.units = [];
    put(g, 'USA', 'Lingayen garrison', 'Lingayen', 0.3, true);
    return { g, d48, read: tune(g, [d48], 'Lingayen', want) };
  };

  it("the plan's own landing still owed after the strike's day reads the same: under 1.5 it does not storm the shore", () => {
    const { g, d48, read } = lingayen(29, 1.35);
    expect(read).toBeLessThan(1.5);
    listen();
    runAI(g, 'JAP');
    const waits = of(D48).filter((t) => t.what === 'wait' && t.to === 'Lingayen');
    expect(waits.map((t) => t.front)).toEqual(['luzon']);
    expect(oddsTold(waits[0])).toBeGreaterThanOrEqual(1.2);
    expect(oddsTold(waits[0])).toBeLessThan(1.5);
    expect(where(g, d48)).not.toBe('Lingayen');
    expect(g.headlines.some((h) => /Lingayen/.test(String(h.text)) && /land|landing/.test(String(h.text)))).toBe(false);
  });

  it("on the strike's own day the plan's landing stays on the plan's test: at 1.35 it sails for Lingayen", () => {
    const g = war(28);
    const at = SOUTH();
    g.flags['st:go:JAP'] = 28;
    g.flags['st:goOp:JAP'] = ops('JAP').indexOf(at) + 1;
    const d48 = put(g, 'JAP', D48, 'Formosa', 1, true);
    g.byName.Lingayen.units = [];
    put(g, 'USA', 'Lingayen garrison', 'Lingayen', 0.3, true);
    const read = tune(g, [d48], 'Lingayen', 1.35);
    expect(read).toBeLessThan(1.5);
    listen();
    runAI(g, 'JAP');
    expect(of(D48).filter((t) => t.what === 'wait')).toEqual([]);
    expect(g.headlines.some((h) => /troops land at Lingayen from the sea|landing at Lingayen is thrown back/.test(String(h.text)))).toBe(true);
  });
});
