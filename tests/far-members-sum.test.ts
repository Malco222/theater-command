// One membership, the sum (the design director's rulings of 14:08Z and 14:31Z of 21 November 2026, far side;
// src/campaign/staff.ts fronts and readMembers; the reading itself is far-membership.test.ts).
//
// A far-side front read what the pool spared: everything its country had on that land, less the cover the fronts
// beside it kept. One map was weighed and another fought, and a pocket of nothing read in the hundreds. The ruling:
// a front whose whole edge stands on the far side reads its members over its defence. Its members are the corps
// that stand on its own provinces and the corps behind the lines that are nearest it by the marches; no pool, no
// cover kept for another front. Two conditions. Spoken for: a corps on two lines is weighed by both and fights for
// one, so the front that has an offensive on takes the corps on its own provinces, and a front read after it in
// the fronts' own order no longer counts them. Held rows hold: a row that holds and stays is its front's defence
// and never its attack. A great choice weighing what it could gather keeps the pool; so does every front with a
// province of the old chart on its edge. Behind the far switch; no number. The states are set by hand on the first
// day's chart; no war is played.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { atWar, computeSupply, createGame, declareWar } from '../src/campaign/game';
import { decide, fronts, garrisonMonth, membershipOf, membersWatch, offensiveWatch, readMembers, startOpen, type Front } from '../src/campaign/staff';
import { shoreOf } from '../src/campaign/world';
import type { Corps, Game } from '../src/campaign/types';

/** A war nobody plays on the first day, far side on. */
const war = (): Game => { const g = startOpen(createGame(3, null)); computeSupply(g); return g; };
const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.nat === 'JAP' && c.name === name && !c.dead)!;
/** The front a named province stands on. */
const at = (g: Game, fs: Front[], place: string): Front => fs.find((f) => f.depth[g.byName[place].id] >= 1)!;
/** A front as the figures the pool and the members share. */
const ground = (f: Front): unknown => ({ key: f.key, enemies: [...f.enemies].sort(), edge: [...f.edge].sort((a, b) => a - b), atk: f.atk, def: f.def, threat: f.threat, fixed: f.fixed });
/** Whoever was not counted, read by read: the corps, the front that spoke for it and the front that went without. */
const spoken = (g: Game, read: () => void): string[] => {
  const told: string[] = [];
  membersWatch.tell = (e) => told.push(`${e.corps.name}: ${g.regions[e.by.edge[0]].name} over ${g.regions[e.from.edge[0]].name}`);
  try { read(); } finally { membersWatch.tell = null; }
  return told;
};

afterEach(() => { membersWatch.tell = null; offensiveWatch.tell = null; });

describe('one membership: a far-side front reads its members over its defence', () => {
  it("reads Japan's fronts in China on their members, the pool's figure standing beside them", () => {
    const g = war();
    const fs = fronts(g, 'JAP'), pooled = fronts(g, 'JAP', { pooled: true });
    expect(fs.length).toBe(2);
    // Nothing but the last sum differs from the pool's reading: the same fronts, edges, defence and attack figure.
    expect(fs.map(ground)).toEqual(pooled.map(ground));
    for (const f of fs) {
      expect(f.edge.every((id) => shoreOf(id) === 2)).toBe(true);
      expect(f.members).toBeDefined();
      expect(f.ratio).toBe(f.members! / f.def);
      expect(f.near).toBe(f.members! / f.defNear);
      expect(f.members!).toBeLessThan(f.atk);
      expect(membershipOf(f)).toBeDefined();
    }
    for (const f of pooled) { expect(f.members).toBeUndefined(); expect(membershipOf(f)).toBeUndefined(); }
    // The main front reads under the pool's figure, which counted the rows that hold and stay and the corps before
    // the Yencheng patch; the patch reads on the two corps on its own line and not on the army of all China.
    const main = at(g, fs, 'Chungking'), patch = at(g, fs, 'Yencheng');
    expect(main.ratio).toBeLessThan(at(g, pooled, 'Chungking').ratio);
    expect(main.ratio).toBeGreaterThan(1);
    expect(patch.members!).toBeLessThan(0.15 * patch.atk);
    expect(patch.ratio).toBeLessThan(0.15 * at(g, pooled, 'Yencheng').ratio);
    const M = membershipOf(patch)!;
    const before = g.corps.filter((c) => c.nat === 'JAP' && !c.dead && M.counts(c, patch) && !(c.hold && c.stay));
    expect(before.map((c) => c.name).sort()).toEqual(['12th Army (Shantung)', '13th Army (the Huai river)']);
    for (const c of before) expect(patch.ours).toContain(c.reg);
  });

  it('leaves a front with a province of the old chart on its edge to the pool, to the last figure', () => {
    const g = war();
    const ger = fronts(g, 'GER'), pooled = fronts(g, 'GER', { pooled: true });
    expect(ger.length).toBeGreaterThan(0);
    expect(ger.map((f) => ({ ...(ground(f) as object), ratio: f.ratio, near: f.near }))).toEqual(pooled.map((f) => ({ ...(ground(f) as object), ratio: f.ratio, near: f.near })));
    for (const f of ger) {
      expect(f.edge.every((id) => shoreOf(id) === 0)).toBe(true);
      expect(f.members).toBeUndefined();
      expect(membershipOf(f)).toBeUndefined();
    }
  });

  it('keeps the pool for a great choice weighing what it could gather', () => {
    const g = war();
    garrisonMonth(g, 'JAP');
    const own = at(g, fronts(g, 'JAP'), 'Chungking'), gathered = at(g, fronts(g, 'JAP', { gather: true }), 'Chungking');
    expect(gathered.members).toBeUndefined();
    expect(membershipOf(gathered)).toBeUndefined();
    // The garrison named, the front's own reading is under one to one; what the country could gather is far over it.
    expect(own.ratio).toBeLessThan(1);
    expect(gathered.ratio).toBeGreaterThan(1.5);
    expect(gathered.ratio).toBeGreaterThan(2 * own.ratio);
  });

  it('held rows hold: a row that holds and stays is no part of its front\'s attack, and a row that only holds still is', () => {
    const g = war();
    const first = at(g, fronts(g, 'JAP'), 'Chungking');
    // A free corps on the main front's own line and on no other: the 11th Army's at Yochow.
    const c = corps(g, '11th Army (Yochow)');
    expect(c.hold).toBe(false);
    expect(first.ours).toContain(c.reg);
    // Held where it stands, it is still the staff's to send in: the reading is the same.
    c.hold = true;
    const held = at(g, fronts(g, 'JAP'), 'Chungking');
    expect(held.members).toBe(first.members);
    expect(held.fixed).toBeGreaterThan(first.fixed);
    // Marked to stay as well, it is the front's defence alone: the members are less by what it brought, and the
    // pool's figure beside them is what it was.
    c.stay = true;
    const stays = at(g, fronts(g, 'JAP'), 'Chungking');
    expect(stays.members!).toBeLessThan(first.members!);
    expect(first.members! - stays.members!).toBeGreaterThan(0.03 * first.members!);
    expect(stays.fixed).toBe(held.fixed);
    expect(stays.atk).toBe(first.atk);
    expect(stays.ratio).toBe(stays.members! / stays.def);
  });

  it('spoken for: the front with an offensive on takes the corps on its own provinces, and a front read after it goes without', () => {
    const g = war();
    if (!atWar(g, 'JAP', 'UK')) declareWar(g, 'JAP', 'UK');
    // Three fronts in the fronts' own order, the larger defence first: China's main front, Hong Kong, the Yencheng patch.
    const names = (fs: Front[]): string[] => fs.map((f) => g.regions[f.edge[0]].name);
    let quiet: Front[] = [];
    // Nobody with an offensive on: a corps on two lines is weighed by both, and nobody is told of.
    expect(spoken(g, () => { quiet = fronts(g, 'JAP'); })).toEqual([]);
    expect(names(quiet).slice(1)).toEqual(['Hong Kong', 'Yencheng']);
    const [main, hk, patch] = quiet;
    expect(hk.enemies).toEqual(['UK']);
    const d18 = corps(g, '21st Army (18th Division)'), huai = corps(g, '13th Army (the Huai river)');
    for (const f of [main, hk]) expect(f.ours).toContain(d18.reg);
    for (const f of [main, patch]) expect(f.ours).toContain(huai.reg);
    expect(hk.members).toBeGreaterThan(0);
    expect(hk.ratio).toBeGreaterThan(1);
    const was = { main: main.members!, hk: hk.members!, patch: patch.members! };
    // The main front has an offensive on: it takes the 18th Division at Tungkun and the corps on the Huai, both on
    // its own provinces. Hong Kong's front, read after it, has nobody; the patch has the one corps that is its alone.
    let on: Front[] = [];
    expect(spoken(g, () => { on = fronts(g, 'JAP', { busy: (f) => f.depth[g.byName.Chungking.id] >= 1 }); }))
      .toEqual(['21st Army (18th Division): Loyang over Hong Kong', '13th Army (the Huai river): Loyang over Yencheng']);
    expect(on[0].members).toBe(was.main);
    expect(on[1].members).toBe(0);
    expect(on[1].ratio).toBe(0);
    expect(on[2].members!).toBeGreaterThan(0);
    expect(on[2].members!).toBeLessThan(was.patch);
    // A front that is on takes nothing from a front read before it: with the patch alone on, all read as they did.
    let last: Front[] = [];
    expect(spoken(g, () => { last = fronts(g, 'JAP', { busy: (f) => f.depth[g.byName.Yencheng.id] >= 1 }); })).toEqual([]);
    expect(last.map((f) => f.members)).toEqual([was.main, was.hk, was.patch]);
    // Read again with the offensive called off, the corps are given back.
    readMembers(on);
    expect(on.map((f) => f.members)).toEqual([was.main, was.hk, was.patch]);
    readMembers(on, (f) => f === on[0]);
    expect(on[1].members).toBe(0);
  });

  it("is read again in the staff's month as each front decides: the patch is told what the main front's opening leaves it", () => {
    const g = war();
    const alone = at(g, fronts(g, 'JAP'), 'Yencheng').members!;
    const told: { front: Front; how: string; members: number | undefined }[] = [];
    offensiveWatch.tell = (e) => { if (e.nat === 'JAP') told.push({ front: e.front, how: e.how, members: e.front.members }); };
    decide(g, 'JAP');
    offensiveWatch.tell = null;
    const main = told.find((e) => e.front.depth[g.byName.Chungking.id] >= 1)!, patch = told.find((e) => e.front.depth[g.byName.Yencheng.id] >= 1)!;
    expect(told.indexOf(main)).toBeLessThan(told.indexOf(patch));
    expect(main.how).toBe('opened');
    // The main front opened this month before the patch was asked: the corps on the Huai is spoken for, and the
    // patch decides on the corps that is its alone.
    expect(patch.members!).toBeGreaterThan(0);
    expect(patch.members!).toBeLessThan(alone);
    expect(patch.front.ratio).toBe(patch.members! / patch.front.def);
  });
});
