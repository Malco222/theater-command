// The burst marks are drawing (the design director, 27 November 2026, 05:00Z, on the gun crews' tick-by-tick proof:
// teamwork-tick-proof-1127/PROOF.md). With the men drawn, sim.ts shell() lays each shell's burst on the drawn man it
// costs (Drill.manAt): its 'shell' event's x, y and ground, and its crater. So the drawing reaches the burst marks, and a
// switch that moves drawn men (Drill.teamwork's crews at their posts) moves them. That is the one place the rules take
// anything from the drawing, and it must stay one-way: no rule may ever read a crater or a burst's spot. This test pins
// it, as the director asked: "a test that moves every crater and shell spot and shows the rules record unchanged".
// Each battle is fought live, as the 3D game steps it (LiveBattle.tick, a step at a time), with the gun crews drawn at
// their posts (teamwork) and the drill book riding (life): four set pieces, and a river field with fighter-bombers on
// call to both sides, so bursts land in the water and bombs fall. Each is fought twice. In the second:
//  - every burst is laid somewhere else, by turns (the n-th by the n-th way): off its drawn man by a move of its own
//    (8 to 28 m, every way round, so bursts move against each other and against the drawn men, not as one); in the
//    middle of a block in the field, either side's (a burst among the men); on a spot of each kind of ground the field
//    has, the water too, in turn (so a burst's ground, and whether it leaves a crater, changes); out in the open, as far
//    from any block as can be found; or with no drawn man at all, so it falls on the rules' own spot for it (a wreck, or
//    a place on the tray). Bombs are laid the same ways off their own spots;
//  - after every step the craters are made afresh: each moved, resized, aged, renumbered and given another calibre,
//    each its own way, as a new object with only a crater's own fields (anything else a rule hung on one is gone), the
//    list turned round, and now and then a crater that no shell made added.
// The rules record must not move at any step: the whole LandBattle (every unit's place, strength, nerve, state and
// target, every list, map and counter, its wrecks; anything it keeps, all of it) with only the drill and the craters
// left out, and each step's battle channels (t, events, views, contact, cuts, over, end) with only the burst marks left
// out: a 'shell' event's x, y and ground (a bomb's: the block it bursts over and whether that is armour, too, which are
// read off its spot) and the views' crater zones. The drawn channels (men, tails, rounds, phases) are the drawing and
// are not read. A rule that came to read a crater or a burst's spot, or kept one, would fail it. A control takes 1e-9
// off one unit's nerve after contact and shows the record catches it at that very step.
// Two shells can burst in the same step: one step settles every block's blows together (sim.ts, the step's hurt, each
// block's in turn), and a blow that costs a block two models lays two shells (suffer, one shell a model). A rule that
// read one burst against another of the same step is caught only in a battle that has such a step (the design director,
// 27 November 20:41Z, on the replay's B1: a pin with a living mutant is a loosened pin). Prokhorovka, seed 2, the
// defender seated, has one: a field gun's shell and a howitzer's on two blocks 193 m apart, at 117.6 s (found by a sweep
// of 40 battles; gun-crews/teamwork-tick-proof-1127/PIN-MUTANTS.md). The first test holds that the fights still have such
// a step, so a rules change that took it away fails here and another battle is found, never a pin quietly loosened.
import { describe, expect, it } from 'vitest';
import type { AirOnCall, Calibre, Zone } from '../src/battle/core';
import { groundAt, sampleTicket, withTeams, type LandBattle } from '../src/battle/land';
import { livePiece, liveTicket, type LiveBattle, type LiveDelta } from '../src/battle/land/live';

const crews = (me: 'att' | 'def') => ({ teamwork: true, life: true, me });
const group = (id: number, nat: string): AirOnCall['groups'] => [{ id, name: `${id} Group`, nat, planes: 40, lose: 0.5 }];
const FIGHTS: Array<{ name: string; open: () => LiveBattle }> = [
  { name: 'Prokhorovka, seed 3', open: () => livePiece('prokhorovka-1943', 3, crews('att')) },
  { name: 'Prokhorovka, seed 4', open: () => livePiece('prokhorovka-1943', 4, crews('def')) },
  { name: 'Normandy, seed 2', open: () => livePiece('normandy-1944', 2, crews('def')) },
  { name: 'Prokhorovka, seed 2 (two shells in one step)', open: () => livePiece('prokhorovka-1943', 2, crews('def')) },
  {
    name: 'plains with a river, fighter-bombers on call',
    open: () => liveTicket(withTeams(sampleTicket('plains', 1.3, {
      seed: 7, river: true, date: 'June 1944', nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' },
      air: { att: { share: 0.15, strikes: 3, groups: group(1, 'USA') }, def: { share: 0.1, strikes: 2, groups: group(2, 'GER') } },
    })), crews('att')),
  },
];
const CALS: Calibre[] = ['light', 'mortar', 'field', 'howitzer', 'heavy', 'bomb'];
const inRiver = (g: unknown): boolean => g === 'water' || g === 'blown';
const frac = (v: number): number => v - Math.floor(v);
/** The n-th mark's own move: 8 to 28 m, a golden-angle turn round from the last, so no two marks move alike. */
const shift = (n: number): [number, number] => { const a = n * 2.39996, m = 8 + 20 * frac(n * 0.61803 + 0.17); return [m * Math.cos(a), m * Math.sin(a)]; };

/** The field's spots every 25 m, all of them and by their ground. */
interface Spots { all: Array<[number, number]>; by: Map<string, Array<[number, number]>>; kinds: string[] }
function spotsOf(sim: LandBattle): Spots {
  const all: Array<[number, number]> = [], by = new Map<string, Array<[number, number]>>();
  for (let x = 12; x < sim.field.w - 12; x += 25) for (let y = 12; y < sim.field.h - 12; y += 25) {
    const g = String(groundAt(sim.field, x, y)), q: [number, number] = [x, y];
    all.push(q); if (!by.has(g)) by.set(g, []); by.get(g)!.push(q);
  }
  return { all, by, kinds: [...by.keys()].sort() };
}

/** The ways a mark is laid somewhere else, the n-th by the n-th way. */
const WAYS = ['none', 'near', 'among', 'near', 'ground', 'open'] as const;
type Way = (typeof WAYS)[number];
function relay(n: number, p: [number, number], sim: LandBattle, sp: Spots, bomb: boolean): { way: Way; at: [number, number] | null } {
  const way: Way = WAYS[n % WAYS.length] === 'none' && bomb ? 'near' : WAYS[n % WAYS.length];
  if (way === 'none') return { way, at: null };
  if (way === 'near') { const o = shift(n); return { way, at: [p[0] + o[0], p[1] + o[1]] }; }
  const men = sim.units.filter((u) => u.state !== 'gone');
  if (way === 'among') { const u = men[(n * 7) % men.length]; return { way, at: [u.x, u.y] }; }
  if (way === 'ground') { const g = sp.by.get(sp.kinds[Math.floor(n / WAYS.length) % sp.kinds.length])!; const q = g[(n * 31) % g.length]; return { way, at: [q[0], q[1]] }; }
  let best = sp.all[0], far = -1;
  for (let j = 0; j < 12; j++) {
    const q = sp.all[(n * 97 + j * 1031) % sp.all.length];
    const d = Math.min(...men.map((u) => Math.hypot(u.x - q[0], u.y - q[1])));
    if (d > far) { far = d; best = q; }
  }
  return { way, at: [best[0], best[1]] };
}

/** Two FNV-1a lanes over text and bytes (no node types in this tree's tsconfig). */
class Fnv {
  private a = 0x811c9dc5; private b = 0x01000193 ^ 0x9e3779b9;
  update(x: string | Uint8Array): void {
    let a = this.a, b = this.b;
    if (typeof x === 'string') for (let i = 0; i < x.length; i++) { const c = x.charCodeAt(i); a = Math.imul(a ^ c, 0x01000193); b = Math.imul(b ^ c, 0x85ebca6b) ^ (b >>> 13); }
    else for (let i = 0; i < x.length; i++) { a = Math.imul(a ^ x[i], 0x01000193); b = Math.imul(b ^ x[i], 0x85ebca6b) ^ (b >>> 13); }
    this.a = a; this.b = b;
  }
  digest(): string { return (this.a >>> 0).toString(16).padStart(8, '0') + (this.b >>> 0).toString(16).padStart(8, '0'); }
}

/** The marks a burst event carries: its spot and ground; a bomb's also the block it bursts over and whether armour. */
const marksOf = (e: { what?: unknown; cal?: unknown }): Set<string> | null =>
  e.what !== 'shell' ? null : e.cal === 'bomb' ? new Set(['x', 'y', 'ground', 'on', 'armour']) : new Set(['x', 'y', 'ground']);

/** A canonical digest: Maps and Sets in their own order, typed arrays by bytes, a revisited object by the order it was
 *  first met, functions and anything in `skip` left out; with `marks`, a burst event's marks left out. */
function digest(root: unknown, skip: Set<unknown>, marks: boolean): string {
  const h = new Fnv(), seen = new Map<object, number>();
  const walk = (v: unknown): void => {
    if (v === null || v === undefined) { h.update(v === null ? 'N' : 'U'); return; }
    if (typeof v === 'number') { h.update(Object.is(v, -0) ? 'n-0;' : `n${v};`); return; }
    if (typeof v === 'string') { h.update(`s${v.length}:${v}`); return; }
    if (typeof v === 'boolean') { h.update(v ? 'T' : 'F'); return; }
    if (typeof v === 'bigint') { h.update(`b${v};`); return; }
    if (typeof v !== 'object') return;
    if (skip.has(v)) { h.update('X'); return; }
    const was = seen.get(v);
    if (was !== undefined) { h.update(`r${was};`); return; }
    seen.set(v, seen.size);
    if (ArrayBuffer.isView(v)) { h.update(`t${v.byteLength}:`); h.update(new Uint8Array(v.buffer, v.byteOffset, v.byteLength)); return; }
    if (Array.isArray(v)) { h.update(`a${v.length}[`); for (const x of v) walk(x); h.update(']'); return; }
    if (v instanceof Map) { h.update(`m${v.size}{`); for (const [k, x] of v) { walk(k); walk(x); } h.update('}'); return; }
    if (v instanceof Set) { h.update(`e${v.size}{`); for (const x of v) walk(x); h.update('}'); return; }
    const leave = marks ? marksOf(v as { what?: unknown }) : null;
    h.update(`o${(v as object).constructor?.name ?? ''}{`);
    for (const k of Object.keys(v)) {
      const x = (v as Record<string, unknown>)[k];
      if (leave?.has(k) || typeof x === 'function') continue; // (functions, the test's own hooks among them, are not state)
      h.update(`k${k}:`); walk(x);
    }
    h.update('}');
  };
  walk(root);
  return h.digest();
}

/** A step's battle channels: the drawn ones (men, tails, rounds, phases) left out, and the views' crater zones. */
function channels(d: LiveDelta): unknown {
  const views = d.views.map((v) => ({ ...v, zones: ((v.zones ?? []) as Array<{ kind?: string }>).filter((z) => z.kind !== 'crater') }));
  return { t: d.t, reset: d.reset ?? null, events: d.events, views, contact: d.contact, cuts: d.cuts ?? null, over: d.over ?? null, end: d.end ?? null };
}

type Mark = [number, number, string, string];
/** pairs: each step in which two shells or more (from guns, not bombs) burst, and the least distance between them. */
interface Fought { steps: string[]; marks: Mark[]; ways: Record<string, number>; bombs: number; craters: number; added: number; pairs: Array<[number, number]> }

/** One battle fought live with the gun crews drawn. `move`: the marks laid elsewhere as the head of this file says.
 *  `nudge`: 1e-9 off one unit's nerve at that step, and the fight stopped a few steps on. */
function fight(f: (typeof FIGHTS)[number], move: boolean, nudge?: number): Fought {
  const L = f.open(), sim = L.sim, sp = spotsOf(sim);
  const d = (sim as unknown as { drill: { manAt(id: number, k: number): [number, number] | null } }).drill;
  const air = sim as unknown as { bomb(s: string, x: number, y: number, fl: unknown): void };
  expect(d).toBeTruthy();
  const ways: Record<string, number> = {};
  let added = 0, n = 0, m = 0;
  if (move) {
    const at = d.manAt.bind(d), drop = air.bomb.bind(sim);
    d.manAt = (id: number, k: number) => {
      const p = at(id, k);
      if (!p) return p;
      const r = relay(++n, p, sim, sp, false);
      ways[r.way] = (ways[r.way] ?? 0) + 1;
      return r.at;
    };
    air.bomb = (s: string, x: number, y: number, fl: unknown) => {
      const r = relay(++m, [x, y], sim, sp, true);
      ways[`bomb ${r.way}`] = (ways[`bomb ${r.way}`] ?? 0) + 1;
      drop(s, r.at![0], r.at![1], fl);
    };
  }
  const mine = new WeakSet<object>(), bases: Array<{ x: number; y: number; r: number; at: number; until: number; id: number; c: number }> = [];
  const steps: string[] = [], marks: Mark[] = [], pairs: Array<[number, number]> = [];
  for (let k = 0; !sim.over && k < 40000 && (nudge === undefined || k < nudge + 20); k++) {
    if (nudge !== undefined && k === nudge) {
      // (a unit in the field whose nerve is off its ceiling, mor0, which recovery would put it straight back to)
      const u = sim.units.find((x) => x.state !== 'gone' && x.mor > 0.05 && x.mor < x.mor0 - 0.01);
      expect(u).toBeTruthy();
      u!.mor -= 1e-9;
    }
    const x = L.tick(0.1, 1);
    for (const e of x.events) if (e.what === 'shell') marks.push([e.x ?? NaN, e.y ?? NaN, String(e.ground), String(e.cal)]);
    const guns = x.events.filter((e) => e.what === 'shell' && e.cal !== 'bomb');
    if (guns.length >= 2) {
      let least = Infinity;
      for (let i = 0; i < guns.length; i++) for (let j = i + 1; j < guns.length; j++) least = Math.min(least, Math.hypot((guns[i].x ?? 0) - (guns[j].x ?? 0), (guns[i].y ?? 0) - (guns[j].y ?? 0)));
      pairs.push([k, least]);
    }
    if (move) {
      const cs = sim.craters;
      for (const c of cs) if (!mine.has(c)) bases.push({ x: c.x, y: c.y, r: c.r, at: c.at ?? 0, until: c.until, id: c.id, c: Math.max(0, CALS.indexOf(c.cal ?? 'field')) });
      if (k % 40 === 39) {
        const o = shift(9000 + added);
        bases.push({ x: sim.field.w * frac(0.31 + added * 0.437) + o[0] / 4, y: sim.field.h * frac(0.53 + added * 0.291) + o[1] / 4, r: 1.5, at: sim.t, until: sim.t + 9999, id: -(90000 + added), c: 2 });
        added++;
      }
      cs.length = 0;
      for (let i = bases.length - 1; i >= 0; i--) {
        const o = bases[i], p = o.id * 0.7;
        const z: Zone = {
          id: o.id - 1000 * (1 + (k % 3)), kind: 'crater', x: o.x + 6 * Math.sin(k * 0.37 + p), y: o.y + 6 * Math.cos(k * 0.29 - p * 1.3),
          r: o.r * (0.4 + 1.2 * frac(p * 0.618 + k * 0.013)), until: o.until + (k % 5), at: o.at - ((k + Math.abs(o.id)) % 9),
          cal: CALS[(o.c + k + Math.abs(o.id)) % CALS.length],
        };
        mine.add(z); cs.push(z);
      }
    }
    steps.push(digest(sim, new Set<unknown>([d, sim.craters]), false) + digest(channels(x), new Set(), true));
  }
  return { steps, marks, ways, bombs: marks.filter((q) => q[3] === 'bomb').length, craters: sim.craters.length, added, pairs };
}

describe('the burst marks (where a shell or bomb bursts, and its crater) are drawing: no rule reads them', () => {
  const seen = FIGHTS.map((f) => ({ f, as: fight(f, false), off: fight(f, true) }));

  it('lays every mark somewhere else in the second fight, by every way', () => {
    const ways: Record<string, number> = {};
    let bombs = 0, ground = 0, wet = 0, moved = 0, all = 0;
    for (const s of seen) {
      expect(s.as.marks.length, s.f.name).toBeGreaterThanOrEqual(5);
      expect(s.off.marks.length, s.f.name).toBe(s.as.marks.length);
      for (const [w, c] of Object.entries(s.off.ways)) ways[w] = (ways[w] ?? 0) + c;
      // Every crater made afresh, and more of them than the shells made.
      expect(s.off.added, s.f.name).toBeGreaterThan(0);
      expect(s.off.craters, s.f.name).toBeGreaterThan(s.as.craters);
      bombs += s.as.bombs; all += s.as.marks.length;
      moved += s.as.marks.filter((q, i) => Math.hypot(s.off.marks[i][0] - q[0], s.off.marks[i][1] - q[1]) > 1).length;
      ground += s.as.marks.filter((q, i) => q[2] !== s.off.marks[i][2]).length;
      wet += s.as.marks.filter((q, i) => inRiver(q[2]) !== inRiver(s.off.marks[i][2])).length;
    }
    const seenAs = JSON.stringify({ ways, bombs, ground, wet, moved, all });
    for (const w of ['none', 'near', 'among', 'ground', 'open', 'bomb near', 'bomb among', 'bomb ground', 'bomb open']) expect(ways[w] ?? 0, `${w}: ${seenAs}`).toBeGreaterThanOrEqual(2);
    expect(bombs, seenAs).toBeGreaterThanOrEqual(10);
    expect(moved, seenAs).toBeGreaterThanOrEqual(Math.floor(all * 0.85));
    expect(ground, seenAs).toBeGreaterThanOrEqual(12);
    expect(wet, seenAs).toBeGreaterThanOrEqual(2);
    // Two shells bursting in one step: at the same steps in both fights (the rules lay them), the second fight's laid
    // another distance apart (so a rule that read one against the other would read another figure).
    const pairs = seen.flatMap((s) => s.as.pairs.map(([k, d], i) => ({ at: `${s.f.name}, step ${k}`, as: d, off: s.off.pairs[i]?.[1] ?? NaN })));
    expect(pairs.length, 'no fight has two shells in one step: find a battle that does (PIN-MUTANTS.md)').toBeGreaterThanOrEqual(1);
    for (const s of seen) expect(s.off.pairs.map((q) => q[0]), s.f.name).toEqual(s.as.pairs.map((q) => q[0]));
    expect(pairs.filter((q) => Math.abs(q.off - q.as) > 1).length, JSON.stringify(pairs)).toBeGreaterThanOrEqual(1);
  });

  it('leaves the rules record the same at every step', () => {
    for (const s of seen) {
      expect(s.off.steps.length, s.f.name).toBe(s.as.steps.length);
      expect(s.off.steps.findIndex((x, i) => x !== s.as.steps[i]), s.f.name).toBe(-1);
    }
  });

  it('catches a rules change at once (control: 1e-9 off one unit\'s nerve at step 1500, after contact)', () => {
    const s = seen[0], nudged = fight(s.f, false, 1500);
    expect(nudged.steps.length).toBe(1520);
    expect(nudged.steps.findIndex((x, i) => x !== s.as.steps[i])).toBe(1500);
  }, 600_000); // (1,520 steps digested whole: some 30 s alone, longer on a busy machine)
});
