// Three supply orders on top of the supply columns (release 2; Brian's yes of 23 November 2026 13:06Z, "Do all 3"):
// move the lorries, horse-drawn wagons, forward dumps. Each is behind its own switch (stage.json 'lorryShift',
// 'wagons', 'dumps'), and each does nothing unless the columns run too.
import { beforeAll, describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { columnsMonth, columnsOn, setColumns, type ColumnRun } from '../src/campaign/columns';
import { createGame, groundWeather } from '../src/campaign/game';
import { income } from '../src/campaign/economy';
import { fuelMet } from '../src/campaign/resources';
import { railheads } from '../src/campaign/rails';
import {
  LOGI, cancelDump, dumpPlaces, dumpPrice, dumpsOf, dumpsOn, firstFront, frontsAt, lorryPool, orderDump, roadWear, setLogistics,
  setLorryFirst, shareOutLorries, shiftOn, tendDumps, wagonsOn, whyNoDump,
} from '../src/campaign/logistics';
import { loadGame, saveGame } from '../src/campaign/save';
import { DUMPS_STAGE, LORRYSHIFT_STAGE, WAGONS_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

type Switches = { cols?: boolean; shift?: boolean; wagons?: boolean; dumps?: boolean };

/** A grand campaign from September 1939, nobody at the table, with the switches given. */
function war(on: Switches, seed = 1): Game {
  const g = startOpen(createGame(seed, null));
  if (on.cols) expect(setColumns(g, true)).toBe(true);
  if (on.shift) expect(setLogistics(g, 'lorryShift', true)).toBe(true);
  if (on.wagons) expect(setLogistics(g, 'wagons', true)).toBe(true);
  if (on.dumps) expect(setLogistics(g, 'dumps', true)).toBe(true);
  return g;
}

/** Plays whole months, every nation in turn. */
function months(g: Game, n: number): void {
  const end = g.turn + n;
  let guard = 0;
  while (g.turn < end && guard++ < 4000) stepAI(g);
}

/** A column of the size the columns send: 'lorries' is what it needs. */
const run = (path: number[], divs: number): ColumnRun => ({ path, divs, lorries: divs * 40 * (path.length - 1), lost: 0, share: 1, hits: [] });

describe('the switches', () => {
  it('ship off, and none is read where a campaign does not carry it', () => {
    const st = stage as Record<string, unknown>;
    expect([st.lorryShift, st.wagons, st.dumps]).toEqual([false, false, false]);
    expect([LORRYSHIFT_STAGE, WAGONS_STAGE, DUMPS_STAGE]).toEqual([false, false, false]);
    const g = war({});
    expect([g.flags.lorryShift, g.flags.wagons, g.flags.dumps]).toEqual([undefined, undefined, undefined]);
    expect([shiftOn(g), wagonsOn(g), dumpsOn(g)]).toEqual([false, false, false]);
  });
  it('each does nothing without the supply columns, and is set in the opening month only', () => {
    const g = war({ shift: true, wagons: true, dumps: true });
    expect(columnsOn(g)).toBe(false);
    expect([shiftOn(g), wagonsOn(g), dumpsOn(g)]).toEqual([false, false, false]);
    expect(setColumns(g, true)).toBe(true);
    expect([shiftOn(g), wagonsOn(g), dumpsOn(g)]).toEqual([true, true, true]);
    expect(setLogistics(g, 'dumps', false)).toBe(true);
    expect(dumpsOn(g)).toBe(false);
    months(g, 1);
    expect(setLogistics(g, 'dumps', true)).toBe(false);
    expect(setLogistics(g, 'wagons', false)).toBe(false);
    expect(wagonsOn(g)).toBe(true);
  });
});

describe('moving the lorries', () => {
  it("a nation's lorries come from its industry and stand idle without fuel", () => {
    const g = war({ cols: true, shift: true });
    for (const nat of ['GER', 'UK', 'POL']) {
      expect(lorryPool(g, nat)).toBe(Math.round(LOGI.pool.perIncome * Math.max(0, income(g, nat)) * fuelMet(g, nat)));
    }
    expect(lorryPool(g, 'GER')).toBeGreaterThan(lorryPool(g, 'POL'));
  });
  it('the front named first gets its lorries whole, the biggest first; the rest share what is left, and wagons take the remainder', () => {
    for (const horses of [false, true]) {
      const g = war({ cols: true, shift: true, wagons: horses });
      // Two provinces of Germany's facing Poland and one facing France, each with a road of three provinces.
      const road = railheads(g, 'GER')!;
      const own = g.regions.filter((r) => r.ctrl === 'GER' && road[r.id] >= 0);
      const pol = own.filter((r) => frontsAt(g, 'GER', r.id).includes('POL')).map((r) => r.id);
      const fra = own.filter((r) => frontsAt(g, 'GER', r.id).includes('FRA') && !frontsAt(g, 'GER', r.id).includes('POL')).map((r) => r.id);
      expect(pol.length).toBeGreaterThanOrEqual(2);
      expect(fra.length).toBeGreaterThanOrEqual(1);
      const pool = lorryPool(g, 'GER');
      // Sized so the first front's columns need about two thirds of the lorries and all of them twice what there is.
      const big = Math.round(pool / 120 / 3) * 2, small = Math.max(1, Math.round(big / 3));
      const runs = [run([0, 0, 0, pol[0]], big), run([0, 0, 0, pol[1]], small), run([0, 0, 0, fra[0]], big * 2)];
      const need = runs.map((r) => r.lorries);
      expect(setLorryFirst(g, 'GER', 'POL')).toBe(true);
      g.human = 'GER';
      expect(firstFront(g, 'GER', runs)).toBe('POL');
      shareOutLorries(g, 'GER', runs);
      // The Polish front whole, the bigger concentration first.
      expect(runs[0].lorries).toBe(need[0]);
      expect(runs[1].lorries).toBe(need[1]);
      expect(runs[0].first && runs[1].first).toBe(true);
      // The French front gets what is left, and the rest goes by wagon, or not at all.
      expect(runs[2].first).toBeUndefined();
      expect(runs[2].lorries).toBe(Math.max(0, pool - need[0] - need[1]));
      expect(runs[2].need).toBe(need[2]);
      if (horses) { expect(runs[2].horse).toBe(need[2] - runs[2].lorries); expect(runs[2].short).toBeUndefined(); }
      else { expect(runs[2].short).toBe(need[2] - runs[2].lorries); expect(runs[2].horse).toBeUndefined(); }
    }
  });
  it('a computer nation sends its lorries first to the offensive it has under way with the most divisions beyond the railhead', () => {
    const g = war({ cols: true, shift: true });
    const own = g.regions.filter((r) => r.ctrl === 'GER');
    const pol = own.find((r) => frontsAt(g, 'GER', r.id).includes('POL'))!.id;
    const fra = own.find((r) => frontsAt(g, 'GER', r.id).includes('FRA') && !frontsAt(g, 'GER', r.id).includes('POL'))!.id;
    const runs = [run([0, pol], 4), run([0, fra], 9)];
    const N = g.nations.GER;
    N.lastFight = { POL: g.turn };
    expect(firstFront(g, 'GER', runs)).toBe('POL');
    N.lastFight = { POL: g.turn, FRA: g.turn - 1 };
    expect(firstFront(g, 'GER', runs)).toBe('FRA');
    N.lastFight = { POL: g.turn - 5, FRA: g.turn - 5 };
    expect(firstFront(g, 'GER', runs)).toBeNull();
  });
});

describe('horse-drawn wagons', () => {
  it('the ground tells: mud bogs lorries worse than horses, snow and the desert are worse for horses; off, nothing', () => {
    const g = war({ cols: true, wagons: true });
    const seen = { mud: false, snow: false, desert: false };
    const start = g.turn;
    for (let t = start; t < start + 24; t++) {
      g.turn = t;
      for (const r of g.regions) {
        const ground = groundWeather(g, r);
        const w = roadWear(g, r.id);
        expect(w.horse).toBeGreaterThanOrEqual(LOGI.wagons.perLink);
        if (r.terrain === 'desert') { seen.desert = true; expect(w.horse).toBeGreaterThan(w.lorry); }
        else if (ground === 'mud') { seen.mud = true; expect(w.lorry).toBeGreaterThan(w.horse - LOGI.wagons.perLink); }
        else if (ground === 'snow') { seen.snow = true; expect(w.horse - LOGI.wagons.perLink).toBeGreaterThan(w.lorry); }
      }
    }
    g.turn = start;
    expect(seen).toEqual({ mud: true, snow: true, desert: true });
    const off = war({ cols: true });
    for (const r of off.regions.slice(0, 50)) expect(roadWear(off, r.id)).toEqual({ lorry: 0, horse: 0 });
  });
});

describe('forward dumps', () => {
  let g: Game;
  let at = -1;
  beforeAll(() => {
    // The opening month, with Germany's railways laid as if they ran only to its westernmost province: its corps in the
    // east stand provinces of road beyond the railhead (the railheads are kept until the supply picture is drawn again).
    g = war({ cols: true, dumps: true }, 2);
    const own = g.regions.filter((r) => r.ctrl === 'GER');
    const west = own.reduce((a, b) => (b.x < a.x ? b : a)).id;
    const road = new Int16Array(g.regions.length).fill(-1);
    road[west] = 0;
    const q = [west];
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (road[j] < 0 && g.regions[j].ctrl === 'GER') { road[j] = road[q[i]] + 1; q.push(j); }
    g.railhead.GER = road;
    columnsMonth(g, 'GER');
  });

  it('one is ordered only on own ground far enough beyond the railhead, at most so many, and can be taken back in its month', () => {
    const road = railheads(g, 'GER')!;
    const rail = g.regions.find((r) => r.ctrl === 'GER' && road[r.id] === 0)!.id;
    g.nations.GER.pp = 500;
    expect(whyNoDump(g, 'GER', rail)).toMatch(/too close to the railhead/);
    const foreign = g.regions.find((r) => r.ctrl && r.ctrl !== 'GER')!.id;
    expect(whyNoDump(g, 'GER', foreign)).toMatch(/not held by your own troops/);
    const places = dumpPlaces(g, 'GER');
    const ok = places.filter((p) => whyNoDump(g, 'GER', p.region) === null);
    expect(ok.length).toBeGreaterThan(0);
    at = ok[0].region;
    expect(road[at]).toBeGreaterThanOrEqual(LOGI.dumps.minLinks);
    const pp = g.nations.GER.pp;
    expect(orderDump(g, 'GER', at)).toBe(true);
    expect(g.nations.GER.pp).toBeCloseTo(pp - dumpPrice(), 6);
    expect(whyNoDump(g, 'GER', at)).toMatch(/already/);
    expect(cancelDump(g, 'GER', at)).toBe(true);
    expect(g.nations.GER.pp).toBeCloseTo(pp, 6);
    expect(orderDump(g, 'GER', at)).toBe(true);
    const more = ok.slice(1).map((p) => p.region);
    for (const p of more.slice(0, LOGI.dumps.most - 1)) expect(orderDump(g, 'GER', p)).toBe(true);
    if (more.length >= LOGI.dumps.most) expect(whyNoDump(g, 'GER', more[LOGI.dumps.most - 1])).toMatch(/at most/);
    for (const p of more.slice(0, LOGI.dumps.most - 1)) expect(cancelDump(g, 'GER', p)).toBe(true);
  });

  it('a column brings it up the month after, and from then on the corps nearer to it are fed from it', () => {
    const d = dumpsOf(g, 'GER').find((x) => x.region === at)!;
    // Not this month: it was ordered in it.
    columnsMonth(g, 'GER');
    expect(d.fill).toBe(LOGI.dumps.size);
    expect(g.columns?.GER?.some((r) => r.fill)).toBeFalsy();
    // The month after: a stocking column from the railhead, ambushed like any other.
    g.turn++;
    columnsMonth(g, 'GER');
    const fill = g.columns!.GER.find((r) => r.fill)!;
    expect(fill.path[0]).toBe(g.regions.find((r) => railheads(g, 'GER')![r.id] === 0)!.id);
    expect(fill.path[fill.path.length - 1]).toBe(at);
    expect(d.fill).toBe(0);
    expect(d.stock).toBeCloseTo(LOGI.dumps.size * fill.share, 6);
    // The month after that: corps closer to the dump than to the railhead take their supply from it.
    const before = d.stock;
    g.turn++;
    columnsMonth(g, 'GER');
    const fed = (g.columns?.GER ?? []).filter((r) => r.dump === at);
    const onIt = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.reg === at && c.supplied && c.kind !== 'fort').reduce((t, c) => t + c.divs, 0);
    expect(fed.length + onIt).toBeGreaterThan(0);
    // (Less again by what enemy aircraft over it burned as the month opened.)
    expect(d.stock).toBeLessThanOrEqual(before - fed.reduce((t, r) => t + r.divs, 0) + 1e-9);
    for (const r of fed) {
      expect(r.path[0]).toBe(at);
      expect(r.path.length - 1).toBeLessThan(railheads(g, 'GER')![r.path[r.path.length - 1]]);
    }
    // A corps on the dump itself sends no column at all.
    if (onIt) expect((g.columns?.GER ?? []).some((r) => !r.fill && r.path[r.path.length - 1] === at)).toBe(false);
  });

  it('its ground lost, it is burned', () => {
    expect(dumpsOf(g, 'GER').some((x) => x.region === at)).toBe(true);
    const keep = g.regions[at].ctrl;
    g.regions[at].ctrl = 'FRA';
    tendDumps(g, 'GER');
    expect(dumpsOf(g, 'GER').some((x) => x.region === at)).toBe(false);
    g.regions[at].ctrl = keep;
  });

  it('the save keeps the dumps and the front named first, and writes nothing of them while the switches are off', () => {
    const h = war({ cols: true, shift: true, dumps: true }, 3);
    h.dumps = [{ nat: 'GER', region: 5, stock: 12, fill: 0, turn: 1 }];
    h.lorryFirst = { GER: 'POL' };
    const back = loadGame(saveGame(h))!;
    expect(back.dumps).toEqual(h.dumps);
    expect(back.lorryFirst).toEqual(h.lorryFirst);
    const off = war({ cols: true }, 3);
    const text = saveGame(off);
    expect(text).not.toMatch(/"dumps"|"lorryFirst"/);
  });
});

describe('in a war with all three on', () => {
  it('columns carry by lorry and by wagon, the save keeps them, and every share stays between the floor and whole', () => {
    const g = war({ cols: true, shift: true, wagons: true, dumps: true }, 4);
    months(g, 6);
    const all = Object.values(g.columns ?? {}).flat();
    expect(all.length).toBeGreaterThan(0);
    for (const r of all) {
      expect(r.share).toBeGreaterThanOrEqual(0);
      expect(r.share).toBeLessThanOrEqual(1);
      expect(r.lorries + (r.horse ?? 0) + (r.short ?? 0)).toBeLessThanOrEqual((r.need ?? r.lorries) + 1);
    }
    const back = loadGame(saveGame(g))!;
    expect(back.columns).toEqual(g.columns);
  }, 120_000);
});
