// A front between the places of one operation keeps what stands on it (the design director's 11:41Z and 11:58Z of 22
// November 2026 to ruling 234; src/campaign/staff.ts, `waits`, and line.ts, `lineOwes`).
//
// A front keeps its members from the reserve rule only in a month it has a target (22:12Z of 21 November 2026,
// tests/far-reserve-target.test.ts). By the book Changsha falls in month 58 and the record's line opens Hengyang in
// month 60: for two months the front had no target and kept nobody, and the reserve rule took the 11th Army's rows
// north to Shasi and Linfen. In one war of six they were not back until Hengyang fell in 67. The record's 11th Army
// paused after Changsha before Hengyang. Now, at the book's stops, a front whose offensive is on, shut this month,
// keeps what stands on its own ground while the line still owes it a place of an operation that has gone: a span
// whose kind is a dated choice of the book's (Ichigo). A span of no operation is waited for by nobody: the small front
// before Yencheng (a lasting gain, owed in 1941) keeps nobody, as before. The board is set by hand; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { reserveWatch } from '../src/campaign/ai';
import { addCorps, createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { lineOwes, lineShut } from '../src/campaign/line';
import { attackMembers, fronts, membershipOf, ops, ownFront, startOpen, type Front } from '../src/campaign/staff';
import type { Corps, Game } from '../src/campaign/types';

/** Japan's far-side front that faces a named province. */
const facing = (g: Game, place: string): Front => fronts(g, 'JAP').filter((f) => ownFront(g, f)).find((f) => f.edge.includes(g.byName[place].id))!;
/**
 * August 1944 on the first day's chart, by the book: Japan holds every province the record's line has opened to it
 * by now, so that nothing its fronts face is open this month, and the offensive is written on the ground of the front
 * that faces Hengyang (the list's month for it is 60).
 */
const war = (turn: number, stop = 1): { g: Game; front: Front } => {
  const g = startOpen(createGame(3, null));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = turn;
  for (let k = 0; k < 60; k++) {
    const open = fronts(g, 'JAP').filter((f) => ownFront(g, f)).flatMap((f) => f.edge).map((id) => g.regions[id]).filter((r) => !lineShut(g, 'JAP', r));
    if (!open.length) break;
    for (const r of open) { for (const c of r.units) c.dead = true; r.units = []; r.ctrl = 'JAP'; }
  }
  const front = facing(g, 'Hengyang');
  for (let id = 0; id < g.regions.length; id++) if (front.depth[id] >= 1) g.flags[`st:fo:JAP:${id}`] = g.turn - 1;
  return { g, front: facing(g, 'Hengyang') };
};
const gone = (g: Game, op: string, month: number): void => { g.flags[`st:done:JAP:${op}`] = month; };

afterEach(() => { reserveWatch.tell = null; });

describe('a front between the places of one operation keeps what stands on it (the design director, 11:41Z and 11:58Z of 22 November 2026)', () => {
  it('the ground: in month 59 the line shuts all the front faces, and owes it Hengyang by Ichigo', () => {
    const { g, front } = war(59);
    expect(g.byName['Changsha'].ctrl).toBe('JAP');
    expect(g.byName['Hengyang'].ctrl).toBe('CHN');
    expect(front.ours).toContain(g.byName['Changsha'].id);
    expect(front.edge.map((id) => g.regions[id]).filter((r) => !lineShut(g, 'JAP', r))).toEqual([]);
    expect(lineOwes(g, 'JAP', g.byName['Hengyang'])).toBe(true);
    expect(lineOwes(g, 'JAP', g.byName['Hengyang'], ['ichigo'])).toBe(true);
    expect(lineOwes(g, 'JAP', g.byName['Hengyang'], ['raid', 'lasting'])).toBe(false);
    // (Open from its month, it is owed no longer; and ground the line never gives is never owed.)
    g.turn = 60;
    expect(lineOwes(g, 'JAP', g.byName['Hengyang'])).toBe(false);
    expect(lineShut(g, 'JAP', g.byName['Hengyang'])).toBe(false);
    g.turn = 59;
    expect(lineOwes(g, 'JAP', g.byName['Chungking'])).toBe(false);
    // (China is owed nothing by a line that gives ground to Japan.)
    expect(lineOwes(g, 'CHN', g.byName['Hengyang'])).toBe(false);
  });

  it('Ichigo gone: the front counts the corps it counts that stand on its own ground, and no other', () => {
    const { g, front } = war(59);
    // Before the operation has gone the front has no target and waits for nothing: nobody is counted.
    expect(attackMembers(g, 'JAP')).toBeNull();
    gone(g, 'ichigo', 57);
    const at = addCorps(g, 'JAP', '11th Army (40th and 116th Divisions)', 'inf', 2, 'Changsha', { xp: 0.3 });
    const { front: now } = { front: facing(g, 'Hengyang') };
    const counted = attackMembers(g, 'JAP')!;
    expect(counted).not.toBeNull();
    const M = membershipOf(now)!;
    expect(M.counts(at, now)).toBe(true);
    expect(counted(at)).toBe(true);
    const ours = new Set(now.ours);
    const japan = g.corps.filter((c) => c.nat === 'JAP' && !c.dead);
    for (const c of japan) expect(counted(c), c.name).toBe(ours.has(c.reg) && M.counts(c, now));
    // (Some corps the front counts stand elsewhere: those are the reserve's to send.)
    expect(japan.some((c) => M.counts(c, now) && !ours.has(c.reg))).toBe(true);
    expect(front.edge.length).toBeGreaterThan(0);
  });

  it('waits for no span that is not of an operation gone: every other choice of the book gone, nobody is counted', () => {
    const { g } = war(59);
    for (const op of ops('JAP')) if (op.id !== 'ichigo') gone(g, op.id, 1);
    expect(attackMembers(g, 'JAP')).toBeNull();
    // (And the small front before Yencheng in 1939, shut and owed Yencheng in 1941 by a lasting span: nobody.)
    const y = startOpen(createGame(3, null));
    setStop(y, 1, true);
    const small = facing(y, 'Yencheng');
    expect(small.edge.map((id) => y.regions[id].name)).toEqual(['Yencheng']);
    for (let id = 0; id < y.regions.length; id++) if (small.depth[id] >= 1) y.flags[`st:fo:JAP:${id}`] = y.turn;
    for (const op of ops('JAP')) gone(y, op.id, 1);
    expect(lineOwes(y, 'JAP', y.byName['Yencheng'])).toBe(true);
    expect(attackMembers(y, 'JAP')).toBeNull();
  });

  it('stop 2 keeps the book by its own months; where the book is not kept the front waits for nothing', () => {
    // (Stop 2 moves the list's months by its own draw: the case reads a month Changsha is open in and Hengyang owed.)
    const probe = startOpen(createGame(3, null));
    setStop(probe, 2, true);
    const between = [56, 57, 58, 59, 60, 61].filter((m) => {
      probe.turn = m;
      return !lineShut(probe, 'JAP', probe.byName['Changsha']) && lineOwes(probe, 'JAP', probe.byName['Hengyang'], ['ichigo']);
    });
    expect(between.length).toBeGreaterThan(0);
    const two = war(between[0], 2);
    expect(attackMembers(two.g, 'JAP')).toBeNull();
    gone(two.g, 'ichigo', 56);
    expect(attackMembers(two.g, 'JAP')).not.toBeNull();
    // (Stop 3 does not keep the book: its line binds nothing and owes nothing, and nobody waits.)
    const idle = startOpen(createGame(3, null));
    idle.turn = 59;
    const before = attackMembers(idle, 'JAP');
    gone(idle, 'ichigo', 57);
    expect(lineOwes(idle, 'JAP', idle.byName['Hengyang'])).toBe(false);
    expect(lineShut(idle, 'JAP', idle.byName['Hengyang'])).toBe(false);
    const after = attackMembers(idle, 'JAP');
    expect(after === null).toBe(before === null);
    if (before && after) for (const c of idle.corps.filter((c) => c.nat === 'JAP' && !c.dead)) expect(after(c), c.name).toBe(before(c));
  });
});
