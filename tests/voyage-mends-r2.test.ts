// The review's mends to the far chart's voyages and fleets struck at anchor (release 2; the design director's ruling of
// 04:33Z, 25 November 2026, 1 and 2). A fleet in passage: finds its way again where a strait shuts on it, or turns back;
// makes only for a harbour an open sea leads to when its base falls; shadows nothing; comes out against no landing and
// answers none; fires no torpedo, meets no minefield off a harbour it has yet to reach, and its carriers search nothing
// on the way. A fleet struck at anchor: the fighters' mark whoever keeps it; its book goes with it into another fleet;
// the player is offered the withdrawal the news names. Played on the whole-world chart.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { searchOf, whyNotSearch } from '../src/campaign/carriers';
import { merge } from '../src/campaign/fleets';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { minesStrike, setSeaMines } from '../src/campaign/mines';
import {
  NAVY, anchorLoss, canPass, fleetReach, fleetsAnswer, inPassage, navalUpkeep, rebase, safeHarbour, setGate, shipCount, sortieAgainst, torpedoRisk,
} from '../src/campaign/navy';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const said = (g: Game, from: number, re: RegExp): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
const gateOf = (g: Game, name: string): number => g.gates.findIndex((x) => (x as { name?: string }).name === name);
/** The next month at sea for a nation: its fleets in passage steam on, and those due come in. */
const nextMonth = (g: Game, nat = 'JAP'): void => { g.turn++; navalUpkeep(g, nat); };

/** The computer's war at its opening, Konigsberg made Japan's, and the Combined Fleet sent there: by the Panama Canal and the Danish straits. */
function voyage(): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, null));
  const f = fleet(g, 'JAP', 'Combined Fleet'), kon = g.byName['Konigsberg'];
  kon.ctrl = 'JAP';
  expect(rebase(g, f, kon)).toBe(true);
  expect(inPassage(f)).toBe(true);
  const names = f.passage!.way.map((z) => g.seas[z].name);
  expect(names.indexOf('Caribbean Sea')).toBe(names.indexOf('Gulf of Panama') + 1);
  expect(names.slice(-2)).toEqual(['Skagerrak', 'Baltic Sea']);
  return { g, f };
}

describe('a strait shut on a fleet in passage', () => {
  it('beyond the reach it steams this month changes nothing yet', () => {
    const { g, f } = voyage();
    const way = [...f.passage!.way];
    setGate(g, gateOf(g, 'the Panama Canal'), true);
    nextMonth(g);
    expect(f.passage!.way).toEqual(way);
  });

  it('on this month\'s reach: it steams another way from the sea it is on, in with news by the same reckoning', () => {
    const { g, f } = voyage();
    nextMonth(g);
    const panama = gateOf(g, 'the Panama Canal');
    setGate(g, panama, true);
    const news = g.headlines.length, set = f.passage!.set, from = f.zone;
    nextMonth(g);
    expect(inPassage(f)).toBe(true);
    const p = f.passage!;
    expect(p.way[0]).toBe(from);
    for (let i = 1; i < p.way.length; i++) expect(canPass(g, p.way[i - 1], p.way[i], 'JAP')).toBe(true);
    expect(p.way.some((z) => g.seas[z].name === 'Caribbean Sea' && p.way[p.way.indexOf(z) - 1] !== undefined && g.seas[p.way[p.way.indexOf(z) - 1]].name === 'Gulf of Panama')).toBe(false);
    expect([p.start, p.set, p.due]).toEqual([set, g.turn, g.turn + Math.ceil(p.way.length / NAVY.range) - 1]);
    expect(f.zone).toBe(p.way[NAVY.range - 1]);
    expect(said(g, news, /^The Combined Fleet finds its way shut, and steams another way to Konigsberg, \d+ seas off, due in /)).toBe(true);
    // In at last, its months in passage counted from the month it first set out.
    for (let k = 0; k < 24 && inPassage(f); k++) nextMonth(g);
    expect(f.stance).toBe('port');
    expect(g.headlines.some((h) => h.text === `The Combined Fleet comes in to its new base at Konigsberg after ${g.turn - set} months in passage.`)).toBe(true);
  });

  it('with no other way open it turns back', () => {
    const { g, f } = voyage();
    const kure = f.passage!.from;
    for (let k = 0; k < 12 && g.turn < f.passage!.due - 1; k++) nextMonth(g);
    setGate(g, gateOf(g, 'the Danish straits'), true);
    const news = g.headlines.length;
    nextMonth(g);
    expect(f.base).toBe(kure);
    expect(said(g, news, /^The Combined Fleet turns back for Kure, \d+ seas off, due in /)).toBe(true);
  });
});

describe('a fleet whose base falls', () => {
  it('makes only for a harbour an open sea leads to from the sea it is on', () => {
    const g = startOpen(createGame(3, null));
    const f = fleet(g, 'JAP', 'Combined Fleet'), kure = g.regions[f.base], kon = g.byName['Konigsberg'];
    const baltic = g.seas.findIndex((s) => s.name === 'Baltic Sea');
    kon.ctrl = 'JAP';
    f.stance = 'patrol';
    f.zone = baltic;
    setGate(g, gateOf(g, 'the Danish straits'), true);
    kure.ctrl = 'USA';
    navalUpkeep(g, 'JAP');
    expect(g.fleets.includes(f)).toBe(true);
    expect(f.base).toBe(kon.id);
    expect(f.stance).toBe('port');
  });
});

describe('a fleet in passage', () => {
  it('shadows nothing on its way', () => {
    const { g } = voyage();
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    mf.shadow = fleet(g, 'USA', 'Pacific Fleet').id;
    mf.shadowAt = g.turn;
    expect(rebase(g, mf, g.byName['Konigsberg'])).toBe(true);
    expect(inPassage(mf)).toBe(true);
    expect([mf.shadow, mf.shadowAt]).toEqual([undefined, undefined]);
  });

  it('comes out against no landing near the sea it crosses, and answers none off its new base', () => {
    const { g, f } = voyage();
    declareWar(g, 'JAP', 'USA');
    const near = g.seas[f.zone].adj.find((z) => z !== f.zone)!;
    const passing = sortieAgainst(g, 'USA', [near]);
    const last = f.home;
    expect(f.zone).not.toBe(last);
    const answer = fleetsAnswer(g, 'USA', [last]);
    expect([...answer.come, ...answer.kept, ...answer.shy].includes(f)).toBe(false);
    f.stance = 'patrol';
    expect(sortieAgainst(g, 'USA', [near])).toBeGreaterThan(passing);
    expect(fleetReach(g, f).includes(last)).toBe(true);
    const again = fleetsAnswer(g, 'USA', [last]);
    expect([...again.come, ...again.kept, ...again.shy].includes(f)).toBe(true);
    f.stance = 'passage';
  });

  it('fires no torpedo from its boats on the way', () => {
    const { g, f } = voyage();
    declareWar(g, 'JAP', 'USA');
    f.ships.ss = Math.max(f.ships.ss, 10);
    const us = fleet(g, 'USA', 'Pacific Fleet');
    us.ships.dd = 0;
    us.stance = 'patrol';
    us.zone = f.zone;
    expect(torpedoRisk(g, us, f.zone)).toBe(0);
    f.stance = 'raid';
    expect(torpedoRisk(g, us, f.zone)).toBeGreaterThan(0);
    f.stance = 'passage';
  });

  it('meets no minefield off a harbour it has yet to reach', () => {
    const { g, f } = voyage();
    declareWar(g, 'JAP', 'USA');
    setSeaMines(g, true);
    expect(f.zone).not.toBe(f.home);
    g.mines = [{ z: f.home, nat: 'USA', s: 1e9, t: g.turn }];
    const ships = shipCount(f.ships);
    minesStrike(g, 'JAP');
    expect(shipCount(f.ships)).toBe(ships);
    g.mines = [{ z: f.zone, nat: 'USA', s: 1e9, t: g.turn }];
    minesStrike(g, 'JAP');
    expect(shipCount(f.ships)).toBe(ships - 1);
  });

  it("its carriers' aircraft search nothing on the way; its ships keep their own lookout", () => {
    const g = startOpen(createGame(3, null));
    declareWar(g, 'JAP', 'USA');
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    g.byName['Konigsberg'].ctrl = 'JAP';
    expect(rebase(g, mf, g.byName['Konigsberg'])).toBe(true);
    const wing = g.airFleets.find((w) => w.ship === mf.id)!;
    expect(whyNotSearch(g, wing, mf.zone)).toMatch(/^The Mobile Fleet is in passage to Konigsberg/);
    const next = g.seas[mf.zone].adj.find((z) => !g.fleets.some((x) => x.nat === 'JAP' && x !== mf && x.zone === z))!;
    const here = searchOf(g, 'JAP', mf.zone), there = searchOf(g, 'JAP', next);
    expect(here).toBeGreaterThan(0);
    mf.stance = 'patrol';
    expect(searchOf(g, 'JAP', mf.zone)).toBeGreaterThan(here);
    expect(searchOf(g, 'JAP', next)).toBeGreaterThan(there);
    mf.stance = 'passage';
  });
});

/** America at war with Japan in its first month, its Pacific Fleet at anchor at its base. */
function anchored(human: string | null = null): { g: Game; f: Fleet } {
  const g = startOpen(createGame(3, human));
  if (human) for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  if (!atWar(g, 'JAP', 'USA')) declareWar(g, 'JAP', 'USA');
  const f = fleet(g, 'USA', 'Pacific Fleet');
  expect(f.stance).toBe('port');
  return { g, f };
}
function strike(g: Game, f: Fleet, lost: number, km = 600): void {
  const n = Math.round(lost * (shipCount(f.ships) - f.ships.ss));
  (f.anchor ??= []).push({ t: g.turn, at: f.base, n, lost, w: 300, from: [{ sea: f.home, field: -1, km }] });
}

describe('a fleet struck at anchor', () => {
  it('kept by the player with no harbour out of reach: its harbour is marked for the fighters at the opening of its month', () => {
    const { g, f } = anchored('USA');
    f.ordered = true;
    strike(g, f, 0.3, 1e9);
    expect(safeHarbour(g, f)).toBeNull();
    navalUpkeep(g, 'USA');
    expect(g.flags[`anc:${f.id}`]).toBe(g.turn);
  });

  it('joined into another fleet in the harbour, its book goes with it, its losses a share of the whole', () => {
    const { g, f } = anchored();
    const other = g.fleets.find((x) => x.nat === 'USA' && x.id !== f.id && x.base === f.base && x.home === f.home && x.stance === 'port')
      ?? (() => { const x = { ...f, id: g.nextFleet++, name: 'Second Fleet', ships: { ...f.ships }, lost: { ...f.lost }, anchor: undefined }; g.fleets.push(x); return x; })();
    strike(g, f, 0.4);
    const sa = shipCount(f.ships) - f.ships.ss, sb = shipCount(other.ships) - other.ships.ss;
    expect(merge(g, f, other)).toBe(true);
    expect(other.anchor?.length).toBe(1);
    expect(anchorLoss(g, other)).toBeCloseTo((0.4 * sa) / (sa + sb), 9);
  });
});

// ---- Through the 3D game's doorway ------------------------------------------------------------------------------
type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface Button { label: string; act: string; ok: boolean; tip: string }
interface PageOut { sections: { heading: string; rows: { title: string; buttons: Button[] }[] }[] }
const buttons = (p: PageOut): Button[] => p.sections.flatMap((s) => s.rows.flatMap((r) => r.buttons));

describe("the fleet's page", () => {
  it('offers the player the withdrawal the news names, and it is made', () => {
    const { g, f } = anchored('USA');
    f.ordered = true;
    strike(g, f, 0.3);
    const to = safeHarbour(g, f)!;
    expect(to).toBeTruthy();
    TC().load(saveGame(g));
    const page = TC().page(`fleet:${f.id}`) as PageOut;
    expect(page.sections.some((s) => s.heading === "OUT OF THE STRIKERS' REACH")).toBe(true);
    const b = buttons(page).find((x) => x.label === `WITHDRAW TO ${to.name.toUpperCase()}`)!;
    expect(b.ok).toBe(true);
    expect((TC().act(b.act) as { ok: boolean }).ok).toBe(true);
  });
});
