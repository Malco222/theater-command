// The two Khalkhin Gol rows hold (the design director, 19:30Z of 21 November 2026, form B).
//
// Release 1's event of eastern Poland takes every Soviet corps that is not held and stands them in turn on its six
// places. With the whole world on the chart the 1st Army Group and its tanks at Tamsag Bulak were not held, so the
// event took them to Polesie and Wilno in the war's first month. The record kept the group in Mongolia (the 17th Army
// from 1940, to 1945): its two rows hold and stay like the other sixteen rows of the Soviet Far East. The rows are
// data behind the switch (far-shores.json's amendments); the event is as it was.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { describe, expect, it } from 'vitest';
import { capitulate, createGame, sovietEvent } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { shoreOf } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';
import shores from '../data/campaign/far-shores.json';
import farWorld from '../data/campaign/far-world.json';

const GROUP = '1st Army Group (Zhukov; the 57th Special Corps)';
const TANKS = '1st Army Group tanks';
const SIX = ['Wilno', 'Brest', 'Lwow', 'Volhynia', 'Bialystok', 'Polesie'];
const soviet = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'SOV' && !c.dead);
const of = (g: Game, name: string): Corps => soviet(g).find((c) => c.name === name)!;
const at = (g: Game, c: Corps): string => g.regions[c.reg].name;
const far = (c: Corps): boolean => shoreOf(c.reg) === 2;

describe('the two Khalkhin Gol rows hold and stay', () => {
  it('the hand-kept file amends the two rows the written file lays, and nothing else of the Soviet Far East', () => {
    const amended = ((shores as any).rows.amend as any[]).filter((a) => a.nat === 'SOV');
    expect(amended.map((a) => [a.file, a.list, a.name, a.src, a.set])).toEqual([
      ['world', 'corps', GROUP, 'far-world', { hold: 1 }],
      ['world', 'corps', TANKS, 'far-world', { hold: 1 }],
    ]);
    // The written file itself is as its tool left it: sixteen of its eighteen Soviet rows hold.
    const written = ((farWorld as any).world.corps as any[]).filter((r) => r[0] === 'SOV');
    expect([written.length, written.filter((r) => r[5]?.hold).length]).toEqual([18, 16]);
  });

  it('on the first day every Soviet row beyond the old chart holds, the group and its tanks at Tamsag Bulak among them', () => {
    const g = startOpen(createGame(7, null));
    const east = soviet(g).filter(far);
    expect(east.length).toBe(18);
    expect(east.filter((c) => !c.hold).map((c) => c.name)).toEqual([]);
    expect([at(g, of(g, GROUP)), at(g, of(g, TANKS))]).toEqual(['Tamsag Bulak', 'Tamsag Bulak']);
    // Their weight is the written file's own: the amendment lays one word over the row's.
    expect([of(g, GROUP).divs, of(g, GROUP).str, of(g, TANKS).divs, of(g, TANKS).str]).toEqual([3, 0.8, 1, 0.8]);
  });

  it('the Red Army crosses into eastern Poland without them: the march is the old chart\'s corps alone', () => {
    const g = startOpen(createGame(7, null));
    const before = new Map(soviet(g).map((c) => [c.name, c.reg]));
    capitulate(g, 'POL', 'GER');
    sovietEvent(g);
    expect(g.flags.sovietMove).toBe(1);
    expect([at(g, of(g, GROUP)), at(g, of(g, TANKS))]).toEqual(['Tamsag Bulak', 'Tamsag Bulak']);
    const marched = soviet(g).filter((c) => before.get(c.name) !== c.reg);
    expect(marched.length).toBeGreaterThan(0);
    // Every corps that marched began on the old chart and stands on one of the event's six places.
    for (const c of marched) {
      expect(shoreOf(before.get(c.name)!), c.name).toBe(0);
      expect(SIX, c.name).toContain(at(g, c));
    }
    // No Soviet row beyond the old chart moved, and the zone is not handed over empty.
    for (const c of soviet(g).filter((x) => shoreOf(before.get(x.name)!) === 2)) expect(c.reg, c.name).toBe(before.get(c.name));
    for (const n of SIX.filter((x) => g.byName[x].ctrl === 'SOV')) expect(g.byName[n].units.some((u) => u.nat === 'SOV' && !u.dead), n).toBe(true);
  });
});
