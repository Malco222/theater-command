// THE HOME FRONT (release 2; Brian, 25 November 2026: "I want the UI and user experience to feel seamless and easy to
// manage the cities, recruitment, ect"). One room for every town, raising forces and everything on order. It gives no
// order of its own: BUILD is the town plan's act, RAISE the arsenal's order, TAKE IT BACK its cancel, and the building
// named for a town is the staff's own pick (the design director, seamless-r2-1125.md). ORDER AGAIN repeats the
// player's own orders of the last month he ordered in, and is never kept in the war.
import { afterEach, describe, expect, it } from 'vitest';
import { civicAdvice, civicAI, civicOffers, civicReady } from '../src/campaign/buildings';
import { loadGame, saveGame } from '../src/campaign/save';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

interface Build { line: string; name: string; label: string; cost: number; months: number; effect: string; ok: boolean; why: string; act: string }
interface Town { region: number; town: string; province: string; size: string; own: boolean; pays: number; state: string; plots: { s: string; line: string; level: number; months: number }[]; best: Build | null; more: Build[]; going: { id: number; name: string; due: string; cancel: boolean; act: string } | null; note: string }
interface Board {
  nation: string; date: string; up: boolean; money: number;
  tiles: { k: string; label: string; figure: string; sub: string }[]; counts: { ready: number; going: number; all: number };
  towns: Town[]; raise: { key: string; group: string; label: string; cost: number; ok: boolean; why: string; act: string }[];
  again: { ok: boolean; when: string; text: string; cost: number; left: string; why: string; act: string } | null;
  orders: { id: number; what: string; name: string; due: string; cost: number; cancel: boolean; act: string }[];
  staff: { on: boolean; line: string; label: string; act: string; cap: number; caps: { n: number; now: boolean; act: string }[] } | null;
}
interface PageButton { label: string; act: string; ok: boolean; tip: string }
interface Page { ok: boolean; id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: { title: string; lines: string[]; buttons: PageButton[] }[] }[]; home: Board }
interface View { treasury: { money: number }; todo: { k: string; t: string; tip: string }[]; pages: { id: string; label: string }[] }
interface Acted { ok: boolean; note: string; view: View }

const start = (human = 'GER', seed = 3): View => TC().start({ human, seed, view: 'grand' }) as View;
const page = (): Page => TC().page('home') as Page;
const board = (): Board => TC().homeFront() as Board;
const act = (a: string): Acted => TC().act(a) as Acted;
const text = (): string => (TC().save() as { text: string }).text;
const money = (): number => (TC().view() as View).treasury.money;

afterEach(() => { delete (globalThis as { TC_HOME_FRONT?: unknown }).TC_HOME_FRONT; });

describe('the home front board', () => {
  it('lists every town the player holds once, with its plots, and its counts add up', () => {
    start();
    const h = board();
    expect(h.nation).toBe('Germany');
    expect(h.up).toBe(true);
    expect(h.towns.length).toBeGreaterThan(20);
    expect(new Set(h.towns.map((t) => t.region)).size).toBe(h.towns.length);
    expect(h.counts.all).toBe(h.towns.length);
    expect(h.counts.ready).toBe(h.towns.filter((t) => t.state === 'ready' || t.state === 'short').length);
    expect(h.counts.going).toBe(h.towns.filter((t) => t.state === 'going').length);
    for (const t of h.towns) {
      expect(t.plots.length).toBeGreaterThan(0);
      expect(['ready', 'short', 'going', 'done']).toContain(t.state);
      if (t.state === 'ready') { expect(t.best).not.toBeNull(); expect(t.best!.ok).toBe(true); expect(t.best!.act).toBe(`civic:${t.region}:${t.best!.line}`); }
      if (t.state === 'short') { expect(t.best!.ok).toBe(false); expect(t.best!.why).toMatch(/short: the treasury holds/); }
      if (t.state === 'done') { expect(t.best).toBeNull(); expect(t.note).not.toBe(''); }
    }
    // The tiles read the treasury and the counts.
    expect(h.tiles.find((t) => t.k === 'money')!.figure).toBe(h.money.toLocaleString('en-US'));
    expect(h.tiles.find((t) => t.k === 'towns')!.figure).toBe(String(h.towns.length));
  });

  it('looking at it writes nothing in the war', () => {
    start();
    const before = text();
    page();
    board();
    expect(text()).toBe(before);
  });

  it('the building named for a town is the staff\'s own pick: the line and the town civicAI would order next', () => {
    start();
    const g = loadGame(text())!;
    // A computer nation with money to spare: what its builders order, against the advice read without ordering.
    for (const nat of ['FRA', 'ITA', 'SOV']) {
      const n = g.nations[nat];
      if (!n?.alive) continue;
      n.pp = 1e6;
      const advice = civicAdvice(g, nat);
      let want: { region: number; line: string } | null = null;
      for (const line of advice.lines) {
        for (const region of advice.home.slice(0, 30)) {
          const o = civicOffers(g, nat, region).find((x) => x.line === line);
          if (o && (o.why === null || o.poor)) { want = { region, line }; break; }
        }
        if (want) break;
      }
      const had = new Set(n.queue.map((b) => b.id));
      g.flags[`cvb:${nat}`] = 1e6;
      civicAI(g, nat, 0);
      const got = n.queue.find((b) => !had.has(b.id) && b.what === 'civic');
      expect(want, nat).not.toBeNull();
      expect(got, nat).toBeDefined();
      expect({ region: got!.region, line: got!.item }, nat).toEqual(want);
    }
  });

  it('BUILD is one press, the town plan\'s own act at its own price, and TAKE IT BACK returns the money', () => {
    start();
    const t = board().towns.find((x) => x.state === 'ready')!;
    const had = money();
    const done = act(t.best!.act);
    expect(done.ok).toBe(true);
    expect(done.note).toMatch(/ordered at/);
    expect(money()).toBe(had - t.best!.cost);
    const after = board().towns.find((x) => x.region === t.region)!;
    expect(after.state).toBe('going');
    expect(after.going!.cancel).toBe(true);
    expect(after.plots.some((p) => p.s === 'going')).toBe(true);
    expect(act(after.going!.act).ok).toBe(true);
    expect(money()).toBe(had);
    expect(board().towns.find((x) => x.region === t.region)!.state).toBe('ready');
  });

  it('the treasury can be spent on the towns one press a town, with nothing else opened', () => {
    start();
    let presses = 0;
    for (let guard = 0; guard < 400; guard++) {
      const t = board().towns.find((x) => x.state === 'ready');
      if (!t) break;
      expect(act(t.best!.act).ok).toBe(true);
      presses++;
    }
    expect(presses).toBeGreaterThan(0);
    const h = board();
    expect(h.towns.some((t) => t.state === 'ready')).toBe(false);
    expect(h.counts.going).toBe(presses);
  });

  it('RAISE is the arsenal\'s own order: the same war as the arsenal\'s press, to the byte', () => {
    start();
    const o = board().raise.find((x) => x.key === 'corps:eng')!;
    expect(o.ok).toBe(true);
    const had = money();
    const done = act(o.act);
    expect(done.ok).toBe(true);
    expect(money()).toBe(had - o.cost);
    const mine = text();
    const on = board().orders;
    expect(on.length).toBeGreaterThan(0);
    expect(on.some((x) => x.cancel && x.cost === o.cost)).toBe(true);
    start();
    expect((TC().order('corps:eng', -1) as { ok: boolean }).ok).toBe(true);
    expect(text()).toBe(mine);
  });

  it('ORDER AGAIN repeats his own orders of last month at today\'s price, names them first, and is not in the war', () => {
    start();
    expect(board().again).toBeNull();
    const eng = board().raise.find((x) => x.key === 'corps:eng')!, ftr = board().raise.find((x) => x.key === 'air:ftr')!;
    expect(act(eng.act).ok).toBe(true);
    expect(act(ftr.act).ok).toBe(true);
    expect(board().again).toBeNull(); // (this month's own orders are not "again" yet)
    TC().endMonth();
    const a = board().again!;
    expect(a).not.toBeNull();
    expect(a.when).toBe('September 1939');
    expect(a.text).toBe('As in September 1939: 1 engineer corps and 1 fighter wing.');
    const now = board().raise;
    expect(a.cost).toBe(now.find((x) => x.key === 'corps:eng')!.cost + now.find((x) => x.key === 'air:ftr')!.cost);
    expect(a.ok).toBe(true);
    expect(text()).not.toMatch(/ordersNow|ordersBefore|homeOrders/);
    const had = money(), queued = board().orders.length;
    const done = act(a.act);
    expect(done.ok).toBe(true);
    expect(done.note).toBe('Ordered again: 1 engineer corps and 1 fighter wing.');
    expect(money()).toBe(had - a.cost);
    expect(board().orders.length).toBe(queued + 2);
  });

  it('ORDER AGAIN leaves out and names what the treasury cannot meet', () => {
    start();
    const eng = board().raise.find((x) => x.key === 'corps:eng')!, ftr = board().raise.find((x) => x.key === 'air:ftr')!;
    act(ftr.act);
    act(eng.act);
    TC().endMonth();
    // Spend down until the engineer corps cannot be paid for and the fighter wing still can.
    for (let guard = 0; guard < 400 && money() >= ftr.cost + eng.cost; guard++) {
      const t = board().towns.find((x) => x.state === 'ready' && money() - x.best!.cost >= ftr.cost);
      if (!t) break;
      act(t.best!.act);
    }
    expect(money()).toBeGreaterThanOrEqual(ftr.cost);
    expect(money()).toBeLessThan(ftr.cost + eng.cost);
    const a = board().again!;
    expect(a.text).toBe('As in September 1939: 1 fighter wing.');
    expect(a.cost).toBe(board().raise.find((x) => x.key === 'air:ftr')!.cost);
    expect(a.left).toMatch(/^Left out: engineer corps \(\d[\d,]* short\)\.$/);
    const done = act(a.act);
    expect(done.ok).toBe(true);
    expect(done.note).toMatch(/^Ordered again: 1 fighter wing\. Left out: engineer corps \(\d[\d,]* short\)\.$/);
  });

  it('after a load nothing is remembered, and the queue\'s own record of last month stands in while he runs building himself', () => {
    start();
    const eng = board().raise.find((x) => x.key === 'corps:eng')!;
    act(eng.act);
    TC().endMonth();
    const saved = text();
    TC().load(saved);
    const a = board().again!;
    expect(a).not.toBeNull();
    expect(a.text).toBe('As in September 1939: 1 engineer corps.');
    // A cancelled order is not repeated.
    start();
    act(eng.act);
    const id = board().orders.find((x) => x.cancel)!.id;
    expect(act(`home:cancel:${id}`).ok).toBe(true);
    TC().endMonth();
    expect(board().again).toBeNull();
  });

  it('the room for a game with no board of its own: rows with the same acts', () => {
    start();
    const p = page();
    expect(p.ok).toBe(true);
    expect(p.title).toBe('THE HOME FRONT  ·  GERMANY  ·  SEPTEMBER 1939');
    expect(p.headline).toMatch(/^[\d,]+ to spend, and \d+ towns can take a building$/);
    expect(p.sections.map((s) => s.heading)).toEqual(['TOWNS THAT CAN TAKE A BUILDING', 'RAISE FORCES', 'ON ORDER', 'WHO GIVES THESE ORDERS']);
    const first = p.sections[0].rows[0], t = p.home.towns.find((x) => x.state === 'ready' || x.state === 'short')!;
    expect(first.buttons.map((b) => b.label)).toEqual(['BUILD', 'TOWN PLAN', 'SHOW ON THE MAP']);
    expect(first.buttons[0].act).toBe(t.best!.act);
    // The room offers to hand over either half of the building desk (release 2, the desk split): recruitment, and towns and trade.
    const who = p.sections.find((s) => s.heading === 'WHO GIVES THESE ORDERS')!;
    expect(who.rows.map((r) => r.title)).toEqual(['RECRUITMENT  ·  YOU', 'TOWNS AND TRADE  ·  YOU']);
    expect(who.rows.flatMap((r) => r.buttons.map((b) => b.act))).toEqual(['desk:build:staff', 'desk:towns:staff']);
    expect(p.top.some((b) => b.act.startsWith('desk:'))).toBe(false);
    expect(p.home.staff!.on).toBe(false);
    expect(p.home.staff!.caps.map((c) => c.n)).toEqual([25, 50, 75, 100]);
  });

  it('its door and its chip show only where the game shows the board, and not while the staff runs the towns', () => {
    const off = start();
    expect(off.pages.some((p) => p.id === 'home')).toBe(false);
    expect(off.todo.some((t) => t.k === 'page:home')).toBe(false);
    (globalThis as { TC_HOME_FRONT?: unknown }).TC_HOME_FRONT = 1;
    const on = TC().view() as View;
    expect(on.pages.some((p) => p.id === 'home' && p.label === 'THE HOME FRONT')).toBe(true);
    const chip = on.todo.find((t) => t.k === 'page:home')!;
    expect(chip).toBeDefined();
    const g = loadGame(text())!;
    const can = civicReady(g, 'GER');
    expect(chip.t).toBe(`${can} TOWNS CAN BUILD`);
    expect(can).toBe(board().towns.filter((t) => t.state === 'ready').length);
    const handed = act('desk:towns:staff');
    expect(handed.ok).toBe(true);
    expect(handed.view.todo.some((t) => t.k === 'page:home')).toBe(false);
  });
});

describe('the town plan beside the board', () => {
  it('leads back to the home front only where the game shows the board', () => {
    start();
    const first = board().towns[0].region;
    const plain = (TC().page(`town:${first}`) as Page).top.map((b) => b.act);
    expect(plain).not.toContain('open:home');
    (globalThis as { TC_HOME_FRONT?: unknown }).TC_HOME_FRONT = 1;
    const shown = (TC().page(`town:${first}`) as Page).top.map((b) => b.act);
    expect(shown).toContain('open:home');
    expect(shown.filter((a) => a !== 'open:home')).toEqual(plain);
  });
});
