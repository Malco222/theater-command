// The 3D air thread's shown-only asks after the owner's Epirus battle (26 November 2026: "no count to move"): each
// group's view extra carries 'fight', 'rise', 'hold' and 'bounce' (types.ts AirShown; sim.ts shownMore). Each is
// checked tick by tick against what the rules hold, on hand-built battles that make each one happen (two groups in
// contact, a three-group fight, a late scramble, a patrol with and without a quarry, a dive from above) and on the
// pilot tests' raids. Shown only: the pilot raids lose the same aircraft at the same moments as release 2's head
// (tests/battle-air-pilot-raids.ts BASELINE), and the same battles flown with every one of these keys, and what is kept
// for them, overwritten with nonsense each tick lose the same aircraft at the same moments and tell the same events,
// so a later change that lets the battle read a shown field fails here.
import { describe, expect, it } from 'vitest';
import { AirSim } from '../src/battle/air/sim';
import { SKY } from '../src/battle/air/book';
import type { AirShown, AirUnit } from '../src/battle/air/types';
import { DT, type Side } from '../src/battle/core/types';
import { BASELINE, digest, fly, runs, type Run, type Ticket } from './battle-air-pilot-raids';

const ALL = runs();
const off = (u: AirUnit): boolean => u.state === 'gone' || u.state === 'withdrawn';
const flying = (u: AirUnit): boolean => !off(u) && u.job.do !== 'wait';
const away = (u: AirUnit): boolean => u.job.do === 'home' || u.state === 'routing';
const shownOf = (u: AirUnit): AirShown => (u.extra ?? {}) as AirShown;
const r1 = (v: number): number => Math.round(v * 10) / 10;

interface G { side: Side; kind: 'ftr' | 'bmr'; x: number; y: number; band: 0 | 1 | 2; wait?: boolean }

/**
 * A hand-built battle: the Alsace raid's ticket with only the groups given, each where it is put (the raid's groups
 * keep their y; the battle lays them side by side on one line), no staff's defence (no roles, no climb), no named
 * bombers.
 */
function made(gs: G[], o: { mission?: 'bomb' | 'superiority'; radar?: boolean } = {}): Ticket {
  const base = ALL.find((r) => r.key === 'alsace:0')!.ticket;
  const tpl = (side: Side, kind: string) => base[side].forces.find((f) => (f.kind === 'bmr') === (kind === 'bmr')) ?? base.att.forces.find((f) => (f.kind === 'bmr') === (kind === 'bmr'))!;
  const att: Ticket['att']['forces'] = [], def: Ticket['def']['forces'] = [];
  const starts: Ticket['place']['starts'] = {};
  gs.forEach((g, i) => {
    const corps = 500 + i;
    (g.side === 'att' ? att : def).push({ ...tpl(g.side, g.kind), corps, name: `Group ${i + 1}`, men: 60 });
    starts[corps] = { x: g.x, y: g.y, band: g.band, fuel: g.side === 'att' && g.kind === 'ftr' ? SKY.fuel.full : SKY.fuel.home, wait: !!g.wait };
  });
  return {
    ...base, seed: 4242, att: { ...base.att, forces: att }, def: { ...base.def, forces: def },
    place: { ...base.place, mission: o.mission ?? 'bomb', radar: o.radar ?? false, defence: undefined, roles: undefined, bombers: undefined, starts },
  };
}

/** What the four keys showed over a battle, for the battles' own checks below. */
interface Seen {
  sizes: Set<number>; fights: number; rises: number; ended: { at: number; over: number; t: number; band: number }[];
  holds: number; quarried: number; both: number; bounces: { from: number; to: number; at: number; edge: number }[];
  /** Fights by what they touch: none of a box ('bare'), two boxes or more ('twoBoxes'), a knot of one on a box ('ones'). */
  bare: number; twoBoxes: number; ones: number; boxSeen: number;
  /** Ticks a box is named by two fights or more, each such pair's two group lists. */
  shared: { box: number; a: number[]; b: number[] }[];
}

/**
 * Watches a battle tick by tick and checks each group's four keys against the rules' own state that tick: present
 * exactly when they should be, and their numbers true.
 */
class Watch {
  private was = new Map<number, { x: number; y: number; alt: number; wait: boolean }>();
  private scrambled = new Map<number, number>();
  private rising = new Map<number, { at: number; over: number }>();
  private dive = new Map<number, { at: number; from: number; to: number; edge: number }>();
  private read = 0;
  seen: Seen = { sizes: new Set(), fights: 0, rises: 0, ended: [], holds: 0, quarried: 0, both: 0, bounces: [], bare: 0, twoBoxes: 0, ones: 0, boxSeen: 0, shared: [] };

  constructor(private key: string, s: AirSim) {
    this.keep(s);
    this.read = s.history().length;
    // The opening view: nothing has met, dived or been scrambled yet.
    for (const u of s.units) {
      const x = shownOf(u);
      expect(x.fight, key).toBeUndefined();
      expect(x.rise, key).toBeUndefined();
      expect(x.bounce, key).toBeUndefined();
    }
  }

  private keep(s: AirSim): void {
    for (const u of s.units) if (!off(u)) this.was.set(u.id, { x: u.x, y: u.y, alt: u.alt, wait: u.job.do === 'wait' });
  }

  look(s: AirSim): void {
    const h = s.history(), now = h.slice(this.read);
    this.read = h.length;
    // The fights as the contacts make them, knot-sized: the fighter groups joined by fighter-to-fighter contacts both
    // hold, from the lowest id up, never walked through a box; each with the boxes any of its groups holds a contact
    // with (both holding). A fighter group whose only contacts are boxes is a knot of one.
    const fightOf = new Map<number, { ids: number[]; boxes: number[] }>();
    const live = s.units.filter((u) => !off(u));
    const both = (a: AirUnit, id: number): AirUnit | undefined => {
      const b = s.unit(id);
      return b && !off(b) && b.contacts.includes(a.id) ? b : undefined;
    };
    for (const u of [...live].sort((a, b) => a.id - b.id)) {
      if (u.air !== 'ftr' || fightOf.has(u.id)) continue;
      const set = new Set([u.id]), boxes = new Set<number>(), todo = [u];
      let held = false;
      while (todo.length) {
        const a = todo.pop()!;
        for (const id of a.contacts) {
          const b = both(a, id);
          if (!b) continue;
          held = true;
          if (b.air === 'bmr') boxes.add(id);
          else if (!set.has(id)) { set.add(id); todo.push(b); }
        }
      }
      if (!held) continue;
      const k = { ids: [...set].sort((a, b) => a - b), boxes: [...boxes].sort((a, b) => a - b) };
      for (const id of k.ids) fightOf.set(id, k);
    }
    // Each box, the fights naming it this tick (by their id): two knots on one box stay two fights.
    const onBox = new Map<number, Map<number, number[]>>();
    for (const u of s.units) {
      const x = shownOf(u), at = `${this.key} group ${u.id} at ${s.t}`;
      // A box carries no fight, ever; a fighter group in a contact both hold always does.
      if (u.air === 'bmr') { expect('fight' in x, at).toBe(false); continue; }
      if (!off(u) && u.contacts.some((id) => both(u, id))) expect(x.fight, at).toBeDefined();
      const f = x.fight;
      if (!f) continue;
      // Fighters only, and no fighter outside it in a contact both hold with one inside it.
      for (const i of f.groups) {
        const g = s.unit(i)!;
        expect(g.air, at).toBe('ftr');
        for (const j of g.contacts) if (both(g, j)?.air === 'ftr') expect(f.groups, at).toContain(j);
      }
      // 'box' and 'boxes' absent unless it touches a box; 'boxes' every box any of its groups holds a contact with.
      const touched = [...new Set(f.groups.flatMap((i) => s.unit(i)!.contacts.filter((j) => both(s.unit(i)!, j)?.air === 'bmr')))].sort((a, b) => a - b);
      if (touched.length) {
        expect(f.boxes, at).toEqual(touched);
        expect(f.box, at).toBe(touched[0]);
        if (u.id === f.id) {
          this.seen.boxSeen++;
          if (touched.length > 1) this.seen.twoBoxes++;
          if (f.groups.length === 1) this.seen.ones++;
          for (const b of touched) { const m = onBox.get(b) ?? new Map<number, number[]>(); m.set(f.id, f.groups); onBox.set(b, m); }
        }
      } else {
        expect('box' in f, at).toBe(false);
        expect('boxes' in f, at).toBe(false);
        if (u.id === f.id) this.seen.bare++;
      }
    }
    for (const [box, m] of onBox) {
      const fs = [...m.values()];
      for (let i = 1; i < fs.length; i++) this.seen.shared.push({ box, a: fs[0], b: fs[i] });
    }
    for (const u of s.units) {
      const x = shownOf(u), at = `${this.key} group ${u.id} at ${s.t}`;
      // fight: on every fighter group of a knot, the same fight on each of its groups.
      const k = fightOf.get(u.id), ids = k?.ids;
      if (k && ids) {
        const f = x.fight!;
        expect(f, at).toBeDefined();
        expect(f.groups, at).toEqual(ids);
        expect(f.id, at).toBe(ids[0]);
        expect(f.boxes ?? [], at).toEqual(k.boxes);
        expect(f.box, at).toBe(k.boxes[0]);
        const us = ids.map((i) => s.unit(i)!);
        const cx = us.reduce((t, g) => t + g.x, 0) / us.length, cy = us.reduce((t, g) => t + g.y, 0) / us.length;
        expect(Math.abs(f.x - cx), at).toBeLessThanOrEqual(0.0501);
        expect(Math.abs(f.y - cy), at).toBeLessThanOrEqual(0.0501);
        // vx, vy times the tick: the centre's move, these same groups from where they were the tick before.
        const px = ids.reduce((t, i) => t + this.was.get(i)!.x, 0) / ids.length, py = ids.reduce((t, i) => t + this.was.get(i)!.y, 0) / ids.length;
        expect(Math.abs(f.vx * DT - (cx - px)), at).toBeLessThan(0.001);
        expect(Math.abs(f.vy * DT - (cy - py)), at).toBeLessThan(0.001);
        this.seen.fights++;
        this.seen.sizes.add(ids.length);
      } else expect(x.fight, at).toBeUndefined();

      // rise: from the tick it leaves the ground until its climb ends; 'at' that tick, 'to' its band's height.
      const w = this.was.get(u.id);
      if (w?.wait && u.job.do !== 'wait') {
        this.scrambled.set(u.id, s.t);
        expect(now.some((e) => e.what === 'air:scramble' && e.unit === u.id), at).toBe(true);
      }
      const t0 = this.scrambled.get(u.id);
      if (t0 !== undefined && !off(u) && u.to > u.band) {
        const st = s.place.starts[u.corps];
        expect(x.rise, at).toEqual({ x: r1(st.x), y: r1(st.y), at: t0, from: 0, to: SKY.bands.alt[u.to], over: expect.any(Number) });
        // Below its height all the while it climbs.
        expect(u.alt, at).toBeLessThan(SKY.bands.alt[u.to]);
        this.rising.set(u.id, { at: t0, over: x.rise!.over });
        this.seen.rises++;
      } else {
        expect(x.rise, at).toBeUndefined();
        const r = this.rising.get(u.id);
        if (r && !off(u)) {
          // The first view without it is the first at its height, at 'at' + 'over'.
          this.seen.ended.push({ ...r, t: s.t, band: u.band });
          expect(s.t, at).toBeCloseTo(r.at + r.over, 6);
          expect(u.alt, at).toBe(SKY.bands.alt[u.band]);
          this.rising.delete(u.id);
          this.scrambled.delete(u.id);
        }
      }

      // hold: a fighter group on patrol, flying, making for no enemy ('quarry' absent): its patrol's point.
      if (u.air === 'ftr') {
        const q = (u.extra as { quarry?: number } | undefined)?.quarry;
        if (flying(u) && !away(u) && u.job.do === 'patrol' && q === undefined) {
          expect(x.hold, at).toEqual({ x: r1(u.job.x), y: r1(u.job.y) });
          this.seen.holds++;
          if (x.rise) this.seen.both++;
        } else {
          expect(x.hold, at).toBeUndefined();
          if (flying(u) && u.job.do === 'patrol' && q !== undefined) this.seen.quarried++;
        }
      } else expect(typeof x.hold === 'object', at).toBe(false);

      // bounce: from the tick of its dive from above while its edge lasts; the heights are the bands'.
      if (now.some((e) => e.what === 'air:bounce' && e.unit === u.id)) {
        const from = Math.round(w!.alt), to = Math.round(u.alt);
        expect(to, at).toBe(SKY.bands.alt[u.band]);
        expect(from, at).toBeGreaterThan(to);
        this.dive.set(u.id, { at: s.t, from, to, edge: u.bounce > 0 ? r1(SKY.edge.bounceFor / SKY.scale) : 0 });
        this.seen.bounces.push(this.dive.get(u.id)!);
      }
      const d = this.dive.get(u.id);
      // 'over' 0: the rules drop it at once (touch()); 'edge' the dive's edge, 'left' what is left of it.
      if (d && !off(u) && (u.bounce > 0 || d.at === s.t)) expect(x.bounce, at).toEqual({ from: d.from, to: d.to, at: d.at, over: 0, edge: d.edge, left: d.edge > 0 ? r1(u.bounce / SKY.scale) : 0 });
      else { expect(x.bounce, at).toBeUndefined(); this.dive.delete(u.id); }
    }
    this.keep(s);
  }
}

/** Flies a hand-built battle, the computer on both sides unless told, watching it. */
function watched(key: string, tk: Ticket, before?: (s: AirSim) => void): { s: AirSim; seen: Seen } {
  const s = new AirSim(tk, { show: true });
  s.ai.att = true; s.ai.def = true;
  const w = new Watch(key, s);
  while (!s.over) { before?.(s); s.step(); w.look(s); }
  return { s, seen: w.seen };
}

describe('the fight, the climb from the ground, the patrol point and the dive, in the view (shown only)', () => {
  it('two groups in contact: one fight of the two, on both, from the first contact', () => {
    const { s, seen } = watched('two', made([{ side: 'att', kind: 'ftr', x: 50_000, y: 1_000, band: 1 }, { side: 'def', kind: 'ftr', x: 50_000, y: 40_000, band: 1 }], { mission: 'superiority', radar: true }));
    expect(s.history().some((e) => e.what === 'contact')).toBe(true);
    expect([...seen.sizes]).toEqual([2]);
    expect(seen.fights).toBeGreaterThan(10);
  });

  it('three groups: one fight of three, the same list on each', () => {
    const { seen } = watched('three', made([
      { side: 'att', kind: 'ftr', x: 50_000, y: 1_000, band: 1 },
      { side: 'def', kind: 'ftr', x: 49_000, y: 40_000, band: 1 }, { side: 'def', kind: 'ftr', x: 51_000, y: 40_000, band: 1 },
    ], { mission: 'superiority', radar: true }));
    expect(seen.sizes.has(3)).toBe(true);
  });

  it('a late scramble: rise from the scramble to the first view at its height, while it holds the target as its patrol', () => {
    const { s, seen } = watched('scramble', made([{ side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 }, { side: 'def', kind: 'ftr', x: 50_000, y: 85_000, band: 0, wait: true }]));
    const sc = s.history().find((e) => e.what === 'air:scramble')!;
    expect(sc).toBeDefined();
    expect(seen.rises).toBeGreaterThan(100);
    // One climb, from the scramble's view, ended on the rules' own clock at the band's height (the low band given at
    // once, one band climbed: SKY.bands.climb air seconds less the scramble's own tick).
    expect(seen.ended).toHaveLength(1);
    expect(seen.ended[0].at).toBe(sc.t);
    expect(seen.ended[0].band).toBe(1);
    expect(seen.ended[0].over).toBeCloseTo(SKY.bands.climb / SKY.scale - DT, 6);
    // It patrols the target while it climbs and after, with no enemy near: it holds, and rises and holds at once.
    expect(seen.holds).toBeGreaterThan(seen.rises);
    expect(seen.both).toBeGreaterThan(0);
  });

  it('a patrol: hold while it makes for no enemy, gone the tick it makes for one', () => {
    // The defender's player holds his group on its own point ('hold': a patrol there) before the clock starts, and again
    // once the raid's coming near has set the staff's one plan for both sides (scramble() thinks for the defence then,
    // and sends it at the raid); he gives no other order, and the raid flies on to the target past it.
    const tk = made([{ side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 }, { side: 'def', kind: 'ftr', x: 50_000, y: 55_000, band: 1 }], { radar: true });
    let orders = 0;
    const { s, seen } = watched('patrol', tk, (t) => {
      t.ai.def = false;
      const f = t.units.find((u) => u.side === 'def')!;
      if (f.job.do === 'patrol' || orders >= 2) return;
      expect(t.command({ do: 'hold', units: [f.id] }, 'def')).toBe(true);
      expect(f.job).toEqual({ do: 'patrol', x: f.x, y: f.y });
      orders++;
    });
    expect(orders).toBe(2);
    expect(seen.holds).toBeGreaterThan(100);
    // The raid comes within its reach of the point: it makes for it ('quarry'), and holds no more.
    expect(seen.quarried).toBeGreaterThan(0);
    expect(s.history().some((e) => e.what === 'contact')).toBe(true);
  });

  it('a dive from above: bounce from the band above to the band below, over the dive\'s edge', () => {
    const { seen } = watched('dive', made([{ side: 'att', kind: 'ftr', x: 50_000, y: 1_000, band: 2 }, { side: 'def', kind: 'ftr', x: 50_000, y: 40_000, band: 1 }], { mission: 'superiority', radar: true }));
    expect(seen.bounces.length).toBeGreaterThan(0);
    expect(seen.bounces[0]).toMatchObject({ from: SKY.bands.alt[2], to: SKY.bands.alt[1], edge: SKY.edge.bounceFor / SKY.scale });
  });

  it("the pilot raids: every key true to the rules' state each tick", () => {
    const sum = { fights: 0, rises: 0, holds: 0, bounces: 0 };
    for (const r of ALL.filter((x) => /^(kent|alsace|twins|order|sky):0$/.test(x.key))) {
      // As fly() flies it (the defender's order mid-battle for 'order'), watched from its opening view.
      const t = new AirSim(r.ticket, { show: true });
      t.ai.att = true; t.ai.def = true;
      const w2 = new Watch(r.key, t);
      let given = false;
      while (!t.over) {
        if (r.play === 'order' && !given && t.fired) {
          const f = t.units.find((u) => u.side === 'def' && u.air === 'ftr' && u.state !== 'gone' && u.job.do !== 'home');
          const to = t.units.filter((u) => u.side === 'att' && u.air === 'bmr' && u.state !== 'gone').find((b) => !(f && f.job.do === 'intercept' && f.job.on === b.id));
          if (f && to) { t.command({ do: 'attack', units: [f.id], target: to.id }, 'def'); given = true; }
        }
        t.step();
        w2.look(t);
      }
      sum.fights += w2.seen.fights; sum.rises += w2.seen.rises; sum.holds += w2.seen.holds; sum.bounces += w2.seen.bounces.length;
    }
    expect(sum.fights).toBeGreaterThan(1000);
    expect(sum.rises).toBeGreaterThan(0);
    expect(sum.bounces).toBeGreaterThan(0);
  });
});

/** The pilot raids with bomber boxes and escorts, watched from the opening view (no orders given). */
function raidsWatched(re: RegExp): Seen[] {
  return ALL.filter((x) => re.test(x.key)).map((r) => {
    const t = new AirSim(r.ticket, { show: true });
    t.ai.att = true; t.ai.def = true;
    const w = new Watch(r.key, t);
    while (!t.over) { t.step(); w.look(t); }
    return w.seen;
  });
}

describe('the fight is the knot: fighters joined by contact, a box links none and names the knots on it (shown only)', () => {
  // Two defending groups at one box, no escort: defenders never touch each other, so each is a knot of its own.
  const twoOnOne = (): Ticket => made([
    { side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 },
    { side: 'def', kind: 'ftr', x: 49_000, y: 40_000, band: 1 }, { side: 'def', kind: 'ftr', x: 51_000, y: 40_000, band: 1 },
  ], { radar: true });

  it('one box, two knots on it: two fights, each naming the box, neither holding the other\'s groups', () => {
    const { s, seen } = watched('two-on-one', twoOnOne());
    const box = s.units.find((u) => u.air === 'bmr')!.id;
    expect(seen.shared.length).toBeGreaterThan(10);
    for (const p of seen.shared) {
      expect(p.box).toBe(box);
      expect(p.a.filter((i) => p.b.includes(i))).toEqual([]);
    }
  });

  it('a lone fighter group on a box with no enemy fighter: a fight of one, naming the box', () => {
    const tk = made([{ side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 }, { side: 'def', kind: 'ftr', x: 50_000, y: 40_000, band: 1 }], { radar: true });
    const s = new AirSim(tk, { show: true });
    s.ai.att = true; s.ai.def = true;
    const w = new Watch('lone', s);
    const box = s.units.find((u) => u.air === 'bmr')!, ftr = s.units.find((u) => u.air === 'ftr')!;
    let met = 0;
    while (!s.over) {
      s.step(); w.look(s);
      if (ftr.contacts.includes(box.id) && box.contacts.includes(ftr.id) && !off(ftr) && !off(box)) {
        expect(shownOf(ftr).fight, `lone at ${s.t}`).toMatchObject({ id: ftr.id, groups: [ftr.id], box: box.id, boxes: [box.id] });
        met++;
      }
    }
    expect(met).toBeGreaterThan(10);
    expect(w.seen.sizes).toEqual(new Set([1]));
  });

  it('a knot touching two boxes: box the lowest of them, boxes both', () => {
    const tk = made([
      { side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 }, { side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 },
      { side: 'def', kind: 'ftr', x: 50_000, y: 40_000, band: 1 },
    ], { radar: true });
    const s = new AirSim(tk, { show: true });
    s.ai.att = true; s.ai.def = true;
    const w = new Watch('two-boxes', s);
    const boxes = s.units.filter((u) => u.air === 'bmr').map((u) => u.id).sort((a, b) => a - b), ftr = s.units.find((u) => u.air === 'ftr')!;
    let both = 0;
    while (!s.over) {
      s.step(); w.look(s);
      const f = shownOf(ftr).fight;
      if (f?.boxes?.length === 2) { expect(f.boxes).toEqual(boxes); expect(f.box).toBe(boxes[0]); both++; }
    }
    expect(both).toBeGreaterThan(10);
    expect(w.seen.twoBoxes).toBe(both);
  });

  it('a knot with no box: no box and no boxes key', () => {
    const { seen } = watched('bare', made([{ side: 'att', kind: 'ftr', x: 50_000, y: 1_000, band: 1 }, { side: 'def', kind: 'ftr', x: 50_000, y: 40_000, band: 1 }], { mission: 'superiority', radar: true }));
    expect(seen.bare).toBeGreaterThan(10);
    expect(seen.boxSeen).toBe(0);
  });

  it('a box carries no fight, and its own shown hold is as the view gives it without these keys', () => {
    // Two battles in step, one with shownMore() left out: every box's extra is the same in both, every tick.
    const P = AirSim.prototype as unknown as { shownMore: (this: AirSim) => void };
    const was = P.shownMore;
    let held = 0;
    for (const tk of [twoOnOne(), ALL.find((r) => r.key === 'kent:0')!.ticket, ALL.find((r) => r.key === 'alsace:0')!.ticket]) {
      const a = new AirSim(tk, { show: true });
      P.shownMore = function (): void {};
      let b: AirSim;
      try { b = new AirSim(tk, { show: true }); } finally { P.shownMore = was; }
      a.ai.att = a.ai.def = b.ai.att = b.ai.def = true;
      let met = 0;
      while (!a.over) {
        a.step();
        P.shownMore = function (): void {};
        try { b.step(); } finally { P.shownMore = was; }
        for (const u of a.units) {
          if (u.air !== 'bmr') continue;
          const v = b.unit(u.id)!, at = `box ${u.id} at ${a.t}`;
          expect('fight' in (u.extra ?? {}), at).toBe(false);
          expect(JSON.stringify(u.extra), at).toBe(JSON.stringify(v.extra));
          if (typeof (u.extra as { hold?: unknown } | undefined)?.hold === 'number') held++;
          if (u.contacts.length) met++;
        }
      }
      expect(met).toBeGreaterThan(10);
    }
    // The box's shipped 'hold' (the fighter group its guns hold fire for) was there to be kept.
    expect(held).toBeGreaterThan(0);
  });

  it('on the pilot raids: groups hold fighters only, boxes named, knots on one box apart, every fight true each tick', () => {
    const seen = raidsWatched(/^(kent|alsace|twins|order):0$/);
    const sum = (k: 'boxSeen' | 'bare' | 'ones'): number => seen.reduce((t, x) => t + x[k], 0);
    // Watch checks each tick that every group in a fight's groups is a fighter and every box it touches is named.
    expect(sum('boxSeen')).toBeGreaterThan(100);
    expect(sum('ones') + sum('bare')).toBeGreaterThan(0);
    expect(seen.some((x) => [...x.sizes].some((n) => n > 1))).toBe(true);
  });
});

/** Every new key on every group, and what is kept for them, overwritten with nonsense after each tick builds them. */
function poisoned<T>(go: () => T): T {
  const P = AirSim.prototype as unknown as { shownMore: (this: AirSim) => void };
  const was = P.shownMore;
  let k = 1;
  const junk = (): number => (k = (k * 48271) % 2147483647) / 1000;
  P.shownMore = function (this: AirSim): void {
    was.call(this);
    for (const u of this.units) {
      u.extra = {
        ...(u.extra ?? {}),
        fight: { id: -junk(), x: junk(), y: junk(), vx: junk(), vy: junk(), groups: [junk(), junk()] },
        rise: { x: junk(), y: junk(), at: junk(), from: junk(), to: junk(), over: junk() },
        hold: { x: junk(), y: junk() },
        bounce: { from: junk(), to: junk(), at: junk(), over: 0, edge: junk(), left: junk() },
      };
    }
    const K = (this as unknown as { shownKept: { at: Map<number, { x: number; y: number; alt: number }>; waited: Set<number>; rise: Map<number, number>; dive: Map<number, unknown> } }).shownKept;
    for (const u of this.units) { K.at.set(u.id, { x: junk(), y: junk(), alt: junk() }); K.rise.set(u.id, junk()); K.waited.add(u.id); }
    K.dive.clear();
  };
  try { return go(); } finally { P.shownMore = was; }
}

/** The whole event stream of a battle and its end: what may not move. */
const story = (s: AirSim): string => JSON.stringify({ h: s.history(), over: s.over, n: s.units.map((u) => [u.id, u.n, u.state, u.x, u.y, u.band, u.job]) });

describe('shown only: nothing the battle counts moves', () => {
  it("every pilot raid loses the same aircraft on each side, each at the same moment, as release 2's head; the main stream ends where it did", () => {
    for (const r of ALL) expect(digest(fly(r)), r.key).toBe(BASELINE[r.key]);
  });

  it('with the four keys and what is kept for them overwritten each tick, every battle tells the same events, loses the same aircraft and ends the same', () => {
    const hand: Run[] = [
      { key: 'hand:scramble', ticket: made([{ side: 'att', kind: 'bmr', x: 50_000, y: 1_000, band: 1 }, { side: 'def', kind: 'ftr', x: 50_000, y: 85_000, band: 0, wait: true }]), play: 'ai' },
      { key: 'hand:dive', ticket: made([{ side: 'att', kind: 'ftr', x: 50_000, y: 1_000, band: 2 }, { side: 'def', kind: 'ftr', x: 49_000, y: 40_000, band: 1 }, { side: 'def', kind: 'ftr', x: 51_000, y: 40_000, band: 1 }], { mission: 'superiority', radar: true }), play: 'ai' },
    ];
    const some = [...ALL.filter((x) => x.key.endsWith(':0') || x.key.endsWith(':1')), ...hand];
    for (const r of some) {
      const clean = fly(r);
      const bad = poisoned(() => fly(r));
      expect(digest(bad), r.key).toBe(digest(clean));
      expect(story(bad), r.key).toBe(story(clean));
      if (BASELINE[r.key]) expect(digest(bad), r.key).toBe(BASELINE[r.key]);
    }
  });
});
