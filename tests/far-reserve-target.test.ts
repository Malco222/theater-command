// A front keeps its members from the reserve rule only when it has a target this month (the design director's 22:12Z
// of 21 November 2026, on the count of the places the reserve rule left unanswered; staff.ts `attackMembers`, `aimed`).
// Behind the far switch.
//
// As measured (the six wars of the reserve form, seeds 3, 7 and 11 at the book's stop and the next): a front whose
// offensive was on kept every corps it counted whether or not it could attack anything. By the book the one-province
// front before Yencheng was on from 1939 while the record's line shut Yencheng until July 1941, and Hankow went up to
// fifteen of its first twenty-two months with a threat beside it and every reserve it had kept. A front with no target
// this month attacks nothing and keeps nobody. Here the ground is the war's first day, set by hand at the book's stop:
// an offensive is written on the ground of the main front or of the small front before Yencheng, in a month when the
// line shuts all that front faces and in one when it opens ground there, and one month of Japan's own is played with
// the reserve rule watched.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { reserveWatch, runAI } from '../src/campaign/ai';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { lineShut } from '../src/campaign/line';
import { attackMembers, fronts, membershipOf, ownFront, startOpen, type Front } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

type Told = { corps: Corps; from: Region; to: Region; kept: boolean };

/** The war's first ground at the book's stop (or at the stop where the line binds nothing), in month `turn`; an offensive written on the ground of one of Japan's two far-side fronts. */
const war = (on: 'main' | 'small', turn: number, stop = 1): { g: Game; main: Front; small: Front; front: Front; japan: Corps[] } => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  const [main, small] = fronts(g, 'JAP').filter((f) => ownFront(g, f));
  const front = on === 'main' ? main : small;
  for (let id = 0; id < g.regions.length; id++) if (front.depth[id] >= 1) g.flags[`st:fo:JAP:${id}`] = g.turn;
  return { g, main, small, front, japan: g.corps.filter((c) => c.nat === 'JAP' && !c.dead) };
};
/** The provinces on a front's edge that the record's line leaves open to Japan this month. */
const open = (g: Game, f: Front): string[] => f.edge.map((id) => g.regions[id]).filter((r) => !lineShut(g, 'JAP', r)).map((r) => r.name);

/** One month of Japan's own, and what the reserve rule was told of its corps. */
const month = (g: Game): Told[] => {
  const told: Told[] = [];
  reserveWatch.tell = (e) => { if (e.nat === 'JAP') told.push(e); };
  runAI(g, 'JAP');
  reserveWatch.tell = null;
  return told;
};

afterEach(() => { reserveWatch.tell = null; });

describe('a front keeps its members only when it has a target this month', () => {
  it('the ground: by the book the line shuts Yencheng until month 23, and all the main front faces in month 1 but not in month 25', () => {
    const a = war('small', 1), b = war('small', 25), c = war('main', 1), d = war('main', 25);
    expect(a.small.edge.map((id) => a.g.regions[id].name)).toEqual(['Yencheng']);
    expect(open(a.g, a.small)).toEqual([]);
    expect(open(b.g, b.small)).toEqual(['Yencheng']);
    expect(open(c.g, c.main)).toEqual([]);
    expect(open(d.g, d.main)).toContain('Changsha');
    // (Where the line binds nothing, every province a front faces is open.)
    const e = war('main', 1, 3);
    expect(open(e.g, e.main).length).toBe(e.main.edge.length);
  });

  it('an offensive on, and the line shuts all the front faces: nobody is counted, and the reserve rule sends as with none on', () => {
    for (const on of ['small', 'main'] as const) {
      const { g } = war(on, 1);
      expect(attackMembers(g, 'JAP'), on).toBeNull();
      const told = month(g);
      expect(told.length, on).toBeGreaterThan(0);
      expect(told.every((e) => !e.kept), on).toBe(true);
    }
  });

  it('the same offensive in a month the line opens ground on that front: its members are counted, and kept', () => {
    for (const on of ['small', 'main'] as const) {
      const { g, front, japan } = war(on, 25);
      const counted = attackMembers(g, 'JAP')!;
      expect(counted, on).not.toBeNull();
      const M = membershipOf(front)!;
      for (const c of japan) expect(counted(c), `${on}: ${c.name}`).toBe(M.counts(c, front));
      const told = month(g);
      const kept = told.filter((e) => e.kept);
      if (on === 'main') expect(kept.length).toBeGreaterThan(0);
      // (The month's own decisions may put another front's offensive on: whoever is kept is counted before it or after.)
      const now = attackMembers(g, 'JAP');
      for (const e of kept) expect(counted(e.corps) || !!now?.(e.corps), `${on}: ${e.corps.name}`).toBe(true);
    }
  });

  it('where the line binds nothing, a front with its offensive on counts its members as before', () => {
    const { g, front, japan } = war('small', 1, 3);
    const counted = attackMembers(g, 'JAP')!;
    expect(counted).not.toBeNull();
    const M = membershipOf(front)!;
    for (const c of japan) expect(counted(c), c.name).toBe(M.counts(c, front));
  });
});
