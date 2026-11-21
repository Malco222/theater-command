// Burma's rows at the record's weights, and Japan's 15th Army counted from the strike (ruling 234, the design
// director's notes of 04:47Z, 04:51Z, 04:58Z, 05:00Z, 05:30Z and 05:35Z of 21 November 2026; the historian's
// strength-and-attacks-b573.md, china-scale-restatement.md, far-chart-airfields.md sections 5 and 6 and
// coast-group-armies-and-burma-elites.md section 2). Data only: data/campaign/far-shores.json and the Burma front of
// the plan in far-pacific-war.json.
//
// As measured (seed 22 at stop 3, strike in month 29): the army came by the war a month after the strike at Bangkok
// at 2.0, stood on the Kra from the month after, and read Mergui 0.90, 0.50, 0.69, 0.69, 0.64, 0.64, 0.64 against
// rows that stood at 1.0 each where the record had 0.5, 0.6 and 0.35. Burma never fell. The states here are set by
// hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { createGame, declareWar, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { raisedQuality } from '../src/campaign/history';
import { landMarches } from '../src/campaign/planfronts';
import { ops, startOpen } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';
import setup from '../data/campaign/setup-1939.json';
import world from '../data/campaign/world.json';
import shores from '../data/campaign/far-shores.json';

const A15 = '15th Army (33rd and 55th Divisions)';
const UNO = 'Uno Detachment (143rd Infantry Regiment, of the 55th Division)';
const D33 = '11th Army (33rd Division)';
const BURMA = 'Burma Army (the Burma Rifles; the 1st Burma Division from July 1941)';
const C5 = 'Chinese 5th Army (200th, 22nd and 96th Divisions)';
const C6 = 'Chinese 6th Army (49th, 55th and 93rd Divisions)';
const C66 = 'Chinese 66th Army (38th, 28th and 29th Divisions)';

const dated = (name: string): any[][] => ((setup as any).expeditions as any[][]).filter((r) => r[2] === name);
const first = (name: string): any[] => ((world as any).corps as any[]).find((r) => r[1] === name)!;
const war = (stop = 3): Game => { const g = startOpen(createGame(5, null)); if (stop !== 3) setStop(g, stop, true); return g; };
const of = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead);
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
/** Indochina in Japan's hands, as after the demand; Siam beside Japan and in its war, as the day the strike goes at stops 1 to 3. */
const japans = (g: Game, ...names: string[]): void => { for (const nm of names) g.byName[nm].ctrl = 'JAP'; };

describe("Burma's defenders stand at the record's weights", () => {
  it('Britain\'s rows: the Burma Army never past 0.5, the 17th Indian 0.6 filling to 0.75, the 7th Armoured Brigade 0.35', () => {
    expect(first(BURMA)[5]).toMatchObject({ str: 0.4, est: 0.5 });
    expect(dated('17th Indian Division')[0][6]).toMatchObject({ str: 0.6, est: 0.75 });
    expect(dated('7th Armoured Brigade')[0].slice(3, 5)).toEqual(['arm', 1]);
    expect(dated('7th Armoured Brigade')[0][6]).toMatchObject({ str: 0.35, est: 0.35 });
  });

  it("China's armies stand at China's scale, and what their best divisions were worth beyond their numbers is their rows' quality: counted once", () => {
    expect(dated(C5)[0][6]).toMatchObject({ str: 0.55, est: 0.55 });
    expect(dated(C6)[0][6]).toMatchObject({ str: 0.45, est: 0.45 });
    expect(dated(C66)[0][6]).toMatchObject({ str: 0.45, est: 0.45 });
    // (A fight multiplies a row's weight by its quality, game.ts `corpsBase`: the 200th, the 22nd and the 38th are in
    // the quality of their armies' rows, weighted by their men, and not in the weight as well.)
    const g = war();
    expect(raisedQuality(g, { name: C5 })).toBe(0.65);
    expect(raisedQuality(g, { name: C66 })).toBe(0.55);
    expect(raisedQuality(g, { name: C6 })).toBe(g.nations['CHN'].quality);
  });

  it('the Burma Army fills to its cap and no further', () => {
    const g = war();
    const c = g.corps.find((x) => x.name === BURMA)!;
    expect([c.str, c.full]).toEqual([0.4, 0.5]);
    for (let t = 2; t <= 20; t++) { g.turn = t; startPhase(g, 'UK'); }
    expect(c.str).toBeLessThanOrEqual(0.5);
    expect(c.str).toBeGreaterThan(0.4);
  });
});

describe("Japan's 15th Army is about 1.4, and where no book dates its rows it is counted from the strike", () => {
  it('the rows: the book\'s at 0.7 a division for stops 1 and 2; for stops 3 to 5 the 143rd Regiment at Saigon and the body at Raheng by the war', () => {
    const rows = dated(A15);
    expect(rows).toHaveLength(2);
    expect(rows[0].slice(0, 6)).toEqual([28, 'JAP', A15, 'inf', 2, 'Bangkok']);
    expect(rows[0][6]).toMatchObject({ str: 0.7, est: 0.7, war: ['JAP', 'UK'], from: [D33], stops: [1, 2] });
    expect(rows[1]).toEqual([28, 'JAP', A15, 'inf', 2, 'Raheng', { gen: 'Shojiro Iida', xp: 0.35, str: 0.56, est: 0.56, war: ['JAP', 'UK'], after: 1, from: [D33], stops: [3, 4, 5] }]);
    expect(dated(UNO)).toEqual([[27, 'JAP', UNO, 'inf', 1, 'Saigon', { xp: 0.35, str: 0.28, est: 0.28, hold: 1, stops: [3, 4, 5] }]]);
    // (About 1.4 either way: two divisions at 0.7, or 0.28 on the Kra and a body of 1.12.)
    expect(2 * 0.7).toBeCloseTo(1.4, 9);
    expect(0.28 + 2 * 0.56).toBeCloseTo(1.4, 9);
    // The new rows stand after every row the written files lay: a saved game knows a dated row by its place in the list.
    const all = (setup as any).expeditions as any[][];
    const mine = ((shores as any)['setup-1939'].expeditions as any[][]).map((r) => r[2]);
    expect(all.slice(-mine.length).map((r) => r[2])).toEqual(mine);
  });

  it("the plan: the Burma front ends at Myitkyina, where the record's campaign ended, with the 56th and 18th Divisions on its list; the 143rd Regiment moves by land to the Kra", () => {
    const F = ops('JAP').find((op) => op.id === 'south')!.fronts!.find((f) => f.id === 'burma')!;
    expect(F.place).toBe('Myitkyina');
    expect(F.rows).toEqual([A15, UNO, '56th Division', '18th Division']);
    expect(F.land).toEqual([{ corps: UNO, to: 'Kra' }]);
    // A row of a live front goes no farther from its place than it stands: every place of the record lies on a road
    // that never leads away from Myitkyina, from the Kra and from Raheng alike.
    const g = war();
    const d = landMarches(g, g.byName['Myitkyina'].id);
    const far = (nm: string): number => d[g.byName[nm].id];
    expect(['Kra', 'Mergui', 'Moulmein', 'Rangoon', 'Toungoo', 'Mandalay', 'Lashio', 'Myitkyina'].map(far)).toEqual([8, 7, 6, 6, 5, 3, 2, 0]);
    expect(far('Raheng')).toBe(6);
    expect(g.byName['Raheng'].nb).toContain(g.byName['Moulmein'].id);
  });

  it('at stop 3 the regiment waits at Saigon with the strike\'s rows, held; the body comes a month after the war begins, at Raheng, and the 33rd Division leaves China then', () => {
    const g = war();
    japans(g, 'Saigon');
    g.turn = 27; startPhase(g, 'JAP');
    const uno = of(g, UNO)!;
    expect([at(g, uno), uno.str, uno.full, uno.hold]).toEqual(['Saigon', 0.28, 0.28, true]);
    // No war, no army: its month on the calendar is no cause.
    for (const t of [28, 29, 30]) { g.turn = t; startPhase(g, 'JAP'); }
    expect(of(g, A15)).toBeUndefined();
    expect(of(g, D33)).toBeDefined();
    // The war comes in month 31, and Siam stands beside Japan that day (far-pacific-war.json, 'siam-beside-japan').
    g.turn = 31;
    declareWar(g, 'JAP', 'UK');
    expect(g.byName['Raheng'].ctrl).toBe('THA');
    startPhase(g, 'JAP');
    expect(of(g, A15)).toBeUndefined();
    g.turn = 32; startPhase(g, 'JAP');
    const a15 = of(g, A15)!;
    expect([at(g, a15), a15.divs, a15.str, a15.full, a15.hold]).toEqual(['Raheng', 2, 0.56, 0.56, false]);
    expect(a15.general).not.toBeNull();
    expect(of(g, D33)).toBeUndefined();
    // It came once.
    g.turn = 33; startPhase(g, 'JAP');
    expect(g.corps.filter((c) => c.name === A15 && !c.dead)).toHaveLength(1);
  });

  it('a strike before its calendar month still brings the body a month after: the count is the war\'s, not the date\'s', () => {
    const g = war();
    g.turn = 20;
    declareWar(g, 'JAP', 'UK');
    startPhase(g, 'JAP');
    expect(of(g, A15)).toBeUndefined();
    g.turn = 21; startPhase(g, 'JAP');
    expect(at(g, of(g, A15)!)).toBe('Raheng');
  });

  it("at the book's stops the book's row comes, at Bangkok and 0.7 a division, and neither row of the looser stops", () => {
    for (const stop of [1, 2]) {
      const g = war(stop);
      japans(g, 'Saigon');
      g.turn = 27; startPhase(g, 'JAP');
      expect(of(g, UNO), `stop ${stop}`).toBeUndefined();
      g.turn = 28;
      declareWar(g, 'JAP', 'UK');
      g.turn = 29; startPhase(g, 'JAP');
      const a15 = of(g, A15)!;
      expect([at(g, a15), a15.divs, a15.str, a15.full], `stop ${stop}`).toEqual(['Bangkok', 2, 0.7, 0.7]);
      expect(g.corps.filter((c) => c.name === A15 && !c.dead)).toHaveLength(1);
      expect(of(g, UNO)).toBeUndefined();
    }
  });
});
