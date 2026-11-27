// What a front on the attack counts is nobody's reserve (the design director's 21:18Z of 21 November 2026, on the
// trace of the book's stop in China in 1944; staff.ts `attackMembers`, ai.ts `defend`). Behind the far switch.
//
// As measured (seeds 3, 7 and 11 at the book's stop, month 59): the main front in China was told "pushed" and read
// two to one on its members; the defence's reserve rule, which knows a threat next door and a need elsewhere and
// nothing of the front, found the push's centre spare at Changsha once a garrison row was the third corps there and
// marched it two provinces back the month before Hengyang opened. One staff weighing by one map and fighting by
// another is the fault. Here the month is September 1939 and the ground is set by hand: an offensive is written on
// the ground of the main front, of the small front before Yencheng, or of neither, and one month of Japan's own is
// played with the reserve rule watched.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { reserveWatch, runAI } from '../src/campaign/ai';
import { createGame } from '../src/campaign/game';
import { attackMembers, fronts, membershipOf, ownFront, startOpen, type Front } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';

type Told = { corps: Corps; from: Region; to: Region; kept: boolean };

/** Japan's two far-side fronts as the war opens, the larger first; an offensive written on the ground of one, or of neither. */
const war = (on: 'none' | 'main' | 'small'): { g: Game; main: Front; small: Front; japan: Corps[] } => {
  const g = startOpen(createGame(3, null));
  const [main, small] = fronts(g, 'JAP').filter((f) => ownFront(g, f));
  const f = on === 'main' ? main : on === 'small' ? small : null;
  if (f) for (let id = 0; id < g.regions.length; id++) if (f.depth[id] >= 1) g.flags[`st:fo:JAP:${id}`] = g.turn;
  return { g, main, small, japan: g.corps.filter((c) => c.nat === 'JAP' && !c.dead) };
};

/** One month of Japan's own, and what the reserve rule was told of its corps. */
const month = (g: Game): Told[] => {
  const told: Told[] = [];
  reserveWatch.tell = (e) => { if (e.nat === 'JAP') told.push(e); };
  runAI(g, 'JAP');
  reserveWatch.tell = null;
  return told;
};

afterEach(() => { reserveWatch.tell = null; });

describe('what a front on the attack counts is not a reserve', () => {
  it('the ground: two fronts against China, each with members of its own', () => {
    const { g, main, small, japan } = war('none');
    expect(main.enemies).toEqual(['CHN']);
    expect(small.enemies).toEqual(['CHN']);
    expect(main.edge.length).toBeGreaterThan(small.edge.length);
    const M = membershipOf(main)!;
    expect(japan.filter((c) => M.counts(c, main)).length).toBeGreaterThan(40);
    expect(japan.filter((c) => M.counts(c, small)).length).toBeGreaterThan(0);
    // (A corps on both lines counts on both; the corps at Nanchang is the main front's alone.)
    const nanchang = japan.filter((c) => g.regions[c.reg].name === 'Nanchang');
    expect(nanchang.length).toBeGreaterThan(0);
    for (const c of nanchang) { expect(M.counts(c, main)).toBe(true); expect(M.counts(c, small)).toBe(false); }
    // (Somebody at home is on neither.)
    expect(japan.some((c) => !M.counts(c, main) && !M.counts(c, small) && g.regions[c.reg].owner === 'JAP')).toBe(true);
  });

  it('the reading: nobody with no offensive on, and the members of the front whose offensive is on', () => {
    expect(attackMembers(war('none').g, 'JAP')).toBeNull();
    for (const on of ['main', 'small'] as const) {
      const { g, main, small, japan } = war(on);
      const f = on === 'main' ? main : small;
      const counted = attackMembers(g, 'JAP')!;
      expect(counted, on).not.toBeNull();
      const M = membershipOf(f)!;
      for (const c of japan) expect(counted(c), `${on}: ${c.name}`).toBe(M.counts(c, f));
    }
  });

  it('with no offensive on, the reserve rule sends its reserves as it did', () => {
    const { g } = war('none');
    const told = month(g);
    expect(told.length).toBeGreaterThan(0);
    expect(told.every((e) => !e.kept)).toBe(true);
    const far = told.find((e) => e.from.name === 'Nanchang')!;
    expect(far).toBeDefined();
    expect(g.regions[far.corps.reg].name).not.toBe('Nanchang');
  });

  it('the main front on the attack: a corps it counts, spare by the count and with a place to go, stays where it stands', () => {
    const { g, main } = war('main');
    const counted = attackMembers(g, 'JAP')!;
    const told = month(g);
    const kept = told.filter((e) => e.kept);
    expect(kept.length).toBeGreaterThan(0);
    for (const e of kept) {
      expect(counted(e.corps), e.corps.name).toBe(true);
      expect(g.regions[e.corps.reg].name, e.corps.name).toBe(e.from.name);
    }
    // Nobody the front counts is sent anywhere by the rule.
    const now = attackMembers(g, 'JAP');
    for (const e of told) if (!e.kept) expect(counted(e.corps) || !!now?.(e.corps), e.corps.name).toBe(false);
    expect(main.enemies).toEqual(['CHN']);
  });

  it('another front on the attack: the corps of a quiet front is a reserve as before', () => {
    const { g, main } = war('small');
    const counted = attackMembers(g, 'JAP')!;
    const M = membershipOf(main)!;
    const told = month(g);
    const sent = told.filter((e) => !e.kept);
    expect(sent.length).toBeGreaterThan(0);
    // The main front's corps at Nanchang goes as it went with no offensive on at all.
    const far = sent.find((e) => e.from.name === 'Nanchang')!;
    expect(far).toBeDefined();
    expect(counted(far.corps)).toBe(false);
    expect(M.counts(far.corps, main)).toBe(true);
    expect(g.regions[far.corps.reg].name).not.toBe('Nanchang');
    for (const e of told) if (e.kept) expect(counted(e.corps), e.corps.name).toBe(true);
  });
});
