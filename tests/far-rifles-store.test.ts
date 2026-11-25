// China's rifles, stage B1: the store fills (src/campaign/rifles.ts, data/campaign/rifles.json, far-arms.json; the
// design director's rulings of 05:37Z and 05:44Z of 25 November 2026 on pacific-japan/dd-form-arms-1125.md, section
// 15). Nothing draws from the store yet: this file reads only what fills it.
//
//  1. Far off: no nation and no road is laid, and the month writes nothing.
//  2. Far on, the first day: China's shelf, its three works and its row.
//  3. The arsenals: a quiet month, Chungking lost and retaken, the share of home industry held scaling no works.
//  4. The Burma Road: open, cut by an enemy at each place in turn, not by a neutral, and the news of it once each way.
//  5. Haiphong and Hong Kong carry nothing; Hong Kong is cut on the first day; Japan's demand on Indochina cuts Haiphong.
//  6. The Sinkiang road: each road's chain on the chart; the seller's interim test both ways and its war; a seller's
//     shelf laid in the test, its spare, its draw and a registered need.
//  7. N months' arithmetic, once a month, save and load. 8. The views. 9. The lanes untouched.
// Every state is set by hand on the first day's chart; the lanes' last test plays twelve months nobody plays.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { ARMS, has, rowsFrom, stockOf } from '../src/campaign/arms';
import { FAR_LAID } from '../src/campaign/far-rows';
import { addCorps, capitulate, createGame, industryHeld, setWar } from '../src/campaign/game';
import { demand } from '../src/campaign/pacific';
import { RES, laneKey, lanesTo, seaLanes } from '../src/campaign/resources';
import { RIFLES, needsFrom, rifleRoads, riflesMonth, riflesRow, riflesSeen, riflesView, roadsNow } from '../src/campaign/rifles';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const PER = 21300;
const undo: (() => void)[] = [];
afterEach(() => { while (undo.length) undo.pop()!(); });
/** Takes every road off for one test (the arsenals alone), put back after. */
const noRoads = (): void => { const kept = RIFLES.roads.splice(0); undo.push(() => { RIFLES.roads.splice(0, RIFLES.roads.length, ...kept); }); };
const grand = (human: string | null = null): Game => startOpen(createGame(3, human));
/** China's rifles in store, whole rifles to a thousandth. */
const rifles = (g: Game): number => Math.round(stockOf(g, 'CHN', 'rifles') * PER * 1000) / 1000;
/** One month: the turn moves on and China's month runs; gives the rifles it added. */
const month = (g: Game): number => { const was = rifles(g); g.turn++; riflesMonth(g, 'CHN'); return Math.round((rifles(g) - was) * 1000) / 1000; };
const hold = (g: Game, place: string, nat: string): void => { g.byName[place].ctrl = nat; };
const road = (g: Game, id: string) => roadsNow(g, 'CHN').find((r) => r.id === id)!;
const ours = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^(sa|sac|sag|saw|sam|rflm|rflc):/.test(k)).sort();

describe('far off: the rule binds nobody', () => {
  it('no nation and no road is laid, and the month writes nothing', async () => {
    (globalThis as any).TC_FAR = 0;
    vi.resetModules();
    try {
      const A = await import('../src/campaign/arms');
      const R = await import('../src/campaign/rifles');
      const G = await import('../src/campaign/game');
      const S = await import('../src/campaign/staff');
      const F = await import('../src/campaign/far-rows');
      expect(F.FAR_LAID).toEqual([]);
      expect([A.ARMS.nations, R.RIFLES.nations, R.RIFLES.roads]).toEqual([{}, {}, []]);
      const g = S.startOpen(G.createGame(3, null));
      const before = JSON.stringify(g.flags), news = g.headlines.length;
      for (const nat of Object.keys(g.nations)) R.riflesMonth(g, nat);
      expect(JSON.stringify(g.flags)).toBe(before);
      expect(g.headlines.length).toBe(news);
      expect(R.riflesView(g, 'CHN')).toBeNull();
    } finally {
      (globalThis as any).TC_FAR = 1;
      vi.resetModules();
    }
  });
});

describe('far on, the first day', () => {
  it('far-arms is laid after the books, and China has a rifles shelf of 0', () => {
    expect(FAR_LAID.indexOf('far-arms')).toBe(FAR_LAID.indexOf('far-books') + 1);
    const g = grand();
    expect(has(g, 'CHN', 'rifles')).toBe(true);
    expect(stockOf(g, 'CHN', 'rifles')).toBe(0);
    expect(Object.keys(ARMS.nations)).toEqual(['CHN']);
    expect(Object.keys(RIFLES.nations)).toEqual(['CHN']);
  });

  it('three works, each place on the chart and China\'s, at 3,000, 1,000 and 500 rifles a month', () => {
    const g = grand();
    const works = ARMS.nations.CHN.rifles!.works!;
    expect(works.map((w) => [w.place, Math.round(w.gain * PER * 1e6) / 1e6])).toEqual([['Chungking', 3000], ['Changteh', 1000], ['Kweiyang', 500]]);
    for (const w of works) expect([w.place, g.byName[w.place]?.owner, g.byName[w.place]?.ctrl]).toEqual([w.place, 'CHN', 'CHN']);
  });

  it('its row reads 21,300 rifles to a division\'s worth, refit 0.05, and binds', () => {
    const g = grand();
    expect(riflesRow(g, 'CHN')).toMatchObject({ perDiv: 21300, refit: 0.05, binds: true });
    expect(riflesRow(g, 'JAP')).toBeNull();
    expect(rifleRoads(g, 'CHN').map((r) => r.id)).toEqual(['burma-road', 'sinkiang-road', 'haiphong-rail', 'hongkong-canton']);
  });
});

describe('the arsenals', () => {
  it('a quiet month puts 4,500 rifles on the shelf; Chungking lost, 1,500; retaken, 4,500 again', () => {
    noRoads();
    const g = grand();
    expect(month(g)).toBe(4500);
    hold(g, 'Chungking', 'JAP');
    expect(month(g)).toBe(1500);
    hold(g, 'Chungking', 'CHN');
    expect(month(g)).toBe(4500);
  });

  it('the share of home industry held scales no works', () => {
    noRoads();
    const g = grand();
    const works = new Set(['Chungking', 'Changteh', 'Kweiyang']);
    for (const r of g.regions) if (r.owner === 'CHN' && r.fac > 0 && !works.has(r.name)) r.ctrl = 'JAP';
    expect(industryHeld(g, 'CHN')).toBeLessThan(0.9);
    expect(month(g)).toBe(4500);
  });
});

describe('the Burma Road', () => {
  it('open on the first day: +2,500 rifles a month', () => {
    const g = grand();
    expect(road(g, 'burma-road')).toMatchObject({ state: 'open', tons: 1250, rifles: 2500 });
    const all = month(g);
    expect(all).toBe(4500 + 2500 + 1300);
  });

  it('an enemy of China holding each place in turn cuts it, that place named; a neutral holder does not', () => {
    const g = grand();
    for (const place of RIFLES.roads.find((r) => r.id === 'burma-road')!.past) {
      const was = g.byName[place].ctrl;
      hold(g, place, 'JAP');
      expect(road(g, 'burma-road'), place).toMatchObject({ state: 'cut', rifles: 0, why: `Japan holds ${place} and lets nothing past.`, at: place });
      // (The United States, at peace with China and with Japan on the first day.)
      hold(g, place, 'USA');
      expect(road(g, 'burma-road'), place).toMatchObject({ state: 'open', rifles: 2500 });
      hold(g, place, was);
    }
  });

  it('a cut is news once, and running again is news once, to the receiver\'s seat only', () => {
    const g = grand('CHN');
    const said = (): string[] => g.headlines.filter((h) => /Burma Road/.test(h.text)).map((h) => `T${h.turn} ${h.text}`);
    month(g);
    hold(g, 'Lashio', 'JAP');
    month(g); month(g);
    hold(g, 'Lashio', 'UK');
    month(g); month(g);
    expect(said()).toEqual([
      'T3 The Burma Road is cut: Japan holds Lashio. 2,500 fewer rifles reach your store each month.',
      'T5 The Burma Road runs again: 2,500 rifles a month can reach your store.',
    ]);
    // A war nobody plays: nothing is told anybody.
    const q = grand();
    const n = q.headlines.length;
    month(q); hold(q, 'Lashio', 'JAP'); month(q);
    expect(q.headlines.length).toBe(n);
    expect(q.flags['rflc:burma-road']).toBe(q.turn);
  });
});

describe('Haiphong and Hong Kong', () => {
  it('carry nothing; the road from Hong Kong is cut on the first day by Japan in the Canton delta', () => {
    const g = grand();
    expect(road(g, 'haiphong-rail')).toMatchObject({ state: 'open', tons: 0, rifles: 0 });
    expect(road(g, 'hongkong-canton')).toMatchObject({ state: 'cut', tons: 0, rifles: 0, who: 'JAP' });
    expect(['Tungkun', 'Canton'].map((p) => g.byName[p].ctrl)).toEqual(['JAP', 'JAP']);
    expect(road(g, 'hongkong-canton').why).toBe('Japan holds Tungkun and lets nothing past.');
  });

  it("Japan's demand on Indochina cuts the Haiphong railway by cause", () => {
    const g = grand();
    // (France beaten by Germany, as tests/far-indochina-kept.test.ts drives the demand; Hanoi's post is made from the
    // regiment at Canton.)
    const regiment = addCorps(g, 'JAP', '21st Army (170th Infantry Regiment, of the 104th Division)', 'inf', 1, 'Canton', { xp: 0.35, est: 0.3 });
    regiment.str = 0.3;
    g.turn = 10;
    capitulate(g, 'FRA', 'GER');
    g.turn = 23;
    expect(road(g, 'haiphong-rail').state).toBe('open');
    expect(demand(g)).toBe(true);
    expect(road(g, 'haiphong-rail')).toMatchObject({ state: 'cut', rifles: 0, why: 'Japan holds Haiphong and lets nothing past.' });
  });
});

describe('the roads on the chart', () => {
  it('every place of every road is on the chart, each beside the one before it', () => {
    const g = grand();
    for (const r of RIFLES.roads) {
      expect(r.past.length, r.id).toBeGreaterThan(1);
      for (let i = 0; i < r.past.length; i++) {
        const here = g.byName[r.past[i]];
        expect(here, `${r.id}: ${r.past[i]}`).toBeDefined();
        if (i > 0) expect(here.nb.includes(g.byName[r.past[i - 1]].id), `${r.id}: ${r.past[i - 1]} to ${r.past[i]}`).toBe(true);
      }
    }
    expect(RIFLES.roads.find((r) => r.id === 'sinkiang-road')!.past).toEqual(['Ayaguz', 'Tihwa', 'Turfan', 'Hami', 'Anhsi', 'Yumen', 'Changyeh', 'Liangchow', 'Lanchow']);
  });
});

describe('the Sinkiang road', () => {
  it('with no Soviet shelf, the interim test: at peace 1,300 a month; at war with a great power nothing; at war with China cut', () => {
    const g = grand();
    expect(has(g, 'SOV', 'rifles')).toBe(false);
    expect(road(g, 'sinkiang-road')).toMatchObject({ state: 'open', rifles: 1300 });
    setWar(g, 'SOV', 'GER');
    expect(road(g, 'sinkiang-road')).toMatchObject({ state: 'dry', rifles: 0 });
    expect(month(g)).toBe(4500 + 2500);
    setWar(g, 'SOV', 'CHN');
    expect(road(g, 'sinkiang-road')).toMatchObject({ state: 'cut', rifles: 0, why: 'China is at war with the Soviet Union, which sold the cargoes.', by: 'seller' });
  });

  it('with a Soviet shelf: only the spare is sent, the shelf falls by it in its own divisions\' worth, and a need takes the spare to nothing', () => {
    let start = 10;
    undo.push(rowsFrom((_g, nat) => (nat === 'SOV' ? { rifles: { start } } : undefined)));
    const g = grand();
    const sov = (): number => stockOf(g, 'SOV', 'rifles');
    // Plenty: the whole 1,300, at 10,000 rifles to a Soviet division's worth (sellerPerDiv, G).
    expect(month(g)).toBe(4500 + 2500 + 1300);
    expect(sov()).toBeCloseTo(10 - 0.13, 12);
    // Little: 500 rifles' worth on the shelf, and that is all that comes.
    g.flags['sa:SOV'] = 0.05;
    expect(month(g)).toBe(4500 + 2500 + 500);
    expect(sov()).toBe(0);
    expect(road(g, 'sinkiang-road')).toMatchObject({ state: 'dry', rifles: 0 });
    // A need of its own takes the whole stock: nothing is sold.
    g.flags['sa:SOV'] = 3;
    undo.push(needsFrom((_g, nat) => (nat === 'SOV' ? 3 : 0)));
    expect(month(g)).toBe(4500 + 2500);
    expect(sov()).toBe(3);
    start = 0;
  });
});

describe('the month', () => {
  it('N quiet months give exactly N months\' arithmetic, and a second call in a month adds nothing', () => {
    const g = grand();
    for (let i = 0; i < 7; i++) month(g);
    expect(rifles(g)).toBe(7 * 8300);
    riflesMonth(g, 'CHN');
    expect(rifles(g)).toBe(7 * 8300);
  });

  it('save and load keep the shelf and the roads\' memory', () => {
    const g = grand();
    month(g); hold(g, 'Lashio', 'JAP'); month(g);
    const h = loadGame(saveGame(g))!;
    expect(ours(h)).toEqual(ours(g));
    for (const k of ours(g)) expect(h.flags[k], k).toBe(g.flags[k]);
    expect(rifles(h)).toBe(rifles(g));
    expect(month(h)).toBe(4500 + 1300);
  });

  it('nobody else is written: only China\'s and its roads\' flags', () => {
    const g = grand();
    const before = new Set(Object.keys(g.flags));
    for (const nat of Object.keys(g.nations)) { g.turn = 2; riflesMonth(g, nat); }
    expect(Object.keys(g.flags).filter((k) => !before.has(k)).sort()).toEqual(['rflc:burma-road', 'rflc:haiphong-rail', 'rflc:hongkong-canton', 'rflc:sinkiang-road', 'rflm:CHN', 'sa:CHN', 'sam:CHN']);
  });
});

describe('the views', () => {
  it("the own seat reads the stock, the inflow by source and the picture line", () => {
    const g = grand();
    month(g); month(g); month(g);
    const v = riflesView(g, 'CHN')!;
    expect(v.stock).toBe(3 * 8300);
    expect(v.inflow.works.map((w) => [w.place, Math.round(w.rifles), w.held])).toEqual([['Chungking', 3000, true], ['Changteh', 1000, true], ['Kweiyang', 500, true]]);
    expect(v.inflow.roads.map((r) => [r.id, r.tons, Math.round(r.rifles), r.state])).toEqual([['burma-road', 1250, 2500, 'open'], ['sinkiang-road', 650, 1300, 'open'], ['haiphong-rail', 0, 0, 'open'], ['hongkong-canton', 0, 0, 'cut']]);
    expect(Math.round(v.inflow.total)).toBe(8300);
    expect(v.picture.line).toBe("Rifles in store ■■■■□□□□□□ 24,900 of the 64,000 an infantry corps takes · +8,300 a month: Chungking's arsenals 3,000 · Changteh's arsenals 1,000 · Kweiyang's arsenals 500 · the Burma Road 2,500 · the Sinkiang road 1,300");
  });

  it('another seat sees the roads and the monthly inflow, and no stock figure anywhere', () => {
    const g = grand();
    for (let i = 0; i < 4; i++) month(g);
    const s = riflesSeen(g, 'CHN', 'JAP')!;
    const text = JSON.stringify(s);
    expect(text).not.toMatch(/stock/i);
    expect(text).not.toContain(String(4 * 8300));
    expect(text).not.toContain((4 * 8300).toLocaleString('en-US'));
    expect(Math.round(s.inflow.total)).toBe(8300);
    expect(s.card).toBe('China arms its new corps from its arsenals, the Burma Road and the Sinkiang road: about 8,300 rifles a month, a new infantry corps about every 8 months.');
    expect(riflesSeen(g, 'JAP', 'CHN')).toBeNull();
  });
});

describe('the lanes are untouched', () => {
  /** Every lane's state and key sentence, in the dump bundled against the base tree and this one (seed 17). */
  const dump = async (g: Game): Promise<string> => sha1(JSON.stringify({
    sea: seaLanes(g).map((l) => [l.id, l.to, l.state, l.why, Math.round(l.flow * 1e6), laneKey(g, l.id, null), laneKey(g, l.id, l.to)]),
    to: RES.nations.map((n: string) => [n, lanesTo(g, n).map((l) => [l.id, l.state, l.why, Math.round(l.oil * 1e6), Math.round(l.sup * 1e6), laneKey(g, l.id, n)])]),
  }));
  const sha1 = async (text: string): Promise<string> => [...new Uint8Array(await crypto.subtle.digest('SHA-1', new TextEncoder().encode(text)))].map((b) => b.toString(16).padStart(2, '0')).join('').slice(0, 12);

  it('no road is a lane, and the lanes read the same on the first day and after twelve months nobody plays as on the base tree', async () => {
    const ids = new Set(RES.lanes.map((l) => l.id));
    for (const r of RIFLES.roads) expect(ids.has(r.id), r.id).toBe(false);
    const g = startOpen(createGame(17, null));
    expect(await dump(g)).toBe(BASE_T1);
    for (let t = 0; t < 12; t++) runTurnAI(g);
    expect(await dump(g)).toBe(BASE_T13);
  }, 600_000);
});

// The dump's hashes on the base tree (a99350fb, /home/user/wt/rifles-base), seed 17, far on: first day and month 13.
const BASE_T1 = '1508241fcb58';
const BASE_T13 = 'b8cf99b5df82';
