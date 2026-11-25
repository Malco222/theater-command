// New units ready (release 1.5.1; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered"; his
// card 19:00Z: the plate in 1.5.1). The rules tag the news of a unit handed over to the player (a corps, aircraft,
// ships) as 'ready'; the 3D bridge gives each month's handed-over units in a list of their own. Display only.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { deliver, orderCorps } from '../src/campaign/economy';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type ViewCorps = { id: number; nat: string; reg: number };
type Ready = { what: string; text: string; region: number; corps: number; name: string };
type Month = { ready: Ready[]; view: { corps: ViewCorps[] } };
const TC = (globalThis as unknown as { TC: { start: (o: object) => { corps?: ViewCorps[] }; endMonth: () => Month; raise: (kind: string, region: number) => { ok: boolean; name: string; due: string } } }).TC;

const open = (nat: string, seed = 1939): Game => startOpen(createGame(seed, nat));

describe('new units ready', () => {
  it('tags the news of his corps handed over as ready, with the corps; never a computer nation\'s', () => {
    const g = open('FRA');
    const n = g.nations.FRA;
    n.pp += 500;
    const b = orderCorps(g, 'FRA', 'inf', n.capital);
    expect(b).not.toBeNull();
    b!.eta = g.turn;
    const at = g.headlines.length;
    deliver(g, 'FRA');
    const told = g.headlines.slice(at).filter((h) => h.ready);
    expect(told).toHaveLength(1);
    expect(told[0].ready).toBe('corps');
    const made = g.corps.find((c) => c.id === told[0].unit)!;
    expect(made.name).toBe(b!.name);
    expect(made.nat).toBe('FRA');
    expect(told[0].text).toMatch(/ready for orders/);
    // (a computer nation's units are handed over without a word)
    const ger = g.nations.GER;
    ger.pp += 500;
    const theirs = orderCorps(g, 'GER', 'inf', ger.capital);
    expect(theirs).not.toBeNull();
    theirs!.eta = g.turn;
    const at2 = g.headlines.length;
    deliver(g, 'GER');
    expect(g.headlines.slice(at2).filter((h) => h.ready)).toEqual([]);
  });

  it('tags his aircraft and ships handed over as ready too', () => {
    const g = open('FRA');
    const n = g.nations.FRA;
    const port = g.regions.find((r) => r.owner === 'FRA' && r.ctrl === 'FRA' && r.port && r.seas.length)!;
    n.queue.push({ id: g.nextBuild++, what: 'air', item: 'ftr', count: 2, region: n.capital, ordered: g.turn, eta: g.turn, cost: 0 } as never);
    n.queue.push({ id: g.nextBuild++, what: 'ship', item: 'dd', count: 1, region: port.id, ordered: g.turn, eta: g.turn, cost: 0 } as never);
    const at = g.headlines.length;
    deliver(g, 'FRA');
    const told = g.headlines.slice(at);
    expect(told.length).toBeGreaterThanOrEqual(2);
    expect(told.filter((h) => h.ready === 'air')).toHaveLength(1);
    expect(told.filter((h) => h.ready === 'ship')).toHaveLength(1);
    expect(told.every((h) => !!h.ready)).toBe(true);
  });

  it('through the 3D bridge: the month a corps is handed over says so, with where it stands', () => {
    const paris = open('FRA').nations.FRA.capital;
    TC.start({ view: 'grand', human: 'FRA', seed: 1939, from: 5 });
    const order = TC.raise('inf', paris);
    expect(order.ok).toBe(true);
    let month: Month | null = null;
    for (let i = 0; i < 8; i++) {
      const out = TC.endMonth();
      if (out.ready.some((r) => r.what === 'corps' && r.name === order.name)) { month = out; break; }
      // (nothing is said as ready before it is)
      expect(out.ready.filter((r) => r.name === order.name)).toEqual([]);
    }
    expect(month).not.toBeNull();
    const told = month!.ready.find((r) => r.name === order.name)!;
    expect(told.corps).toBeGreaterThanOrEqual(0);
    expect(told.region).toBeGreaterThanOrEqual(0);
    expect(told.text).toMatch(/ready for orders/);
    const fresh = month!.view.corps.find((c) => c.id === told.corps)!;
    expect(fresh.nat).toBe('FRA');
    expect(fresh.reg).toBe(told.region);
    // A month on, it is not told again.
    expect(TC.endMonth().ready.some((r) => r.corps === told.corps)).toBe(false);
  });
});
