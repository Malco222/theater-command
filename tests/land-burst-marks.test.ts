// The burst marks are drawing (the design director, 27 November 2026, 05:00Z, on the gun crews' tick-by-tick proof:
// teamwork-tick-proof-1127/PROOF.md). With the men drawn, sim.ts shell() lays each shell's burst on the drawn man it
// costs (Drill.manAt): its 'shell' event's x, y and ground, and its crater. So the drawing reaches the burst marks, and a
// switch that moves drawn men (Drill.teamwork's crews at their posts) moves them. That is the one place the rules take
// anything from the drawing, and it must stay one-way: no rule may ever read a crater or a burst's spot. This test pins
// it, as the director asked: "a test that moves every crater and shell spot and shows the rules record unchanged".
// Each battle is fought live, as the 3D game steps it (LiveBattle.tick, a step at a time), with the gun crews drawn at
// their posts (teamwork) and the drill book riding (life): three set pieces, and a river field with fighter-bombers on
// call to both sides, so bursts land in the water and bombs fall. Each is fought twice. In the second:
//  - every burst is laid off its drawn man by a move of its own (8 to 28 m, every way round), so bursts move against
//    each other and against the drawn men, not as one; and every sixth is given no drawn man, so it falls on the
//    rules' own spot for it (a wreck, or a place on the tray);
//  - every bomb bursts off its own spot by a move of its own;
//  - after every step every crater is moved, resized, aged, renumbered and given another calibre, each its own way,
//    the list is turned round, and now and then a crater that no shell made is added.
// The rules record must not move at any step: the whole LandBattle (every unit's place, strength, nerve, state and
// target, every list, map and counter, its wrecks; anything it keeps, all of it) with only the drill and the craters
// left out, and each step's battle channels (t, events, views, contact, cuts, over, end) with only the burst marks left
// out: a 'shell' event's x, y and ground (a bomb's: the block it bursts over and whether that is armour, too, which are
// read off its spot) and the views' crater zones. The drawn channels (men, tails, rounds, phases) are the drawing and
// are not read. A rule that came to read a crater or a burst's spot, or kept one, would fail it. A control takes 1e-9
// off one unit's nerve after contact and shows the record catches it at that very step.
import { describe, expect, it } from 'vitest';
import type { AirOnCall, Calibre } from '../src/battle/core';
import { sampleTicket, withTeams } from '../src/battle/land';
import { livePiece, liveTicket, type LiveBattle, type LiveDelta } from '../src/battle/land/live';

const crews = (me: 'att' | 'def') => ({ teamwork: true, life: true, me });
const group = (id: number, nat: string): AirOnCall['groups'] => [{ id, name: `${id} Group`, nat, planes: 40, lose: 0.5 }];
const FIGHTS: Array<{ name: string; open: () => LiveBattle }> = [
  { name: 'Prokhorovka, seed 3', open: () => livePiece('prokhorovka-1943', 3, crews('att')) },
  { name: 'Prokhorovka, seed 4', open: () => livePiece('prokhorovka-1943', 4, crews('def')) },
  { name: 'Normandy, seed 2', open: () => livePiece('normandy-1944', 2, crews('def')) },
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
interface Fought { steps: string[]; marks: Mark[]; moved: number; unmanned: number; bombs: number; craters: number; added: number }

/** One battle fought live with the gun crews drawn. `move`: the marks moved as the head of this file says. `nudge`:
 *  1e-9 off one unit's nerve at that step, and the fight stopped a few steps on. */
function fight(f: (typeof FIGHTS)[number], move: boolean, nudge?: number): Fought {
  const L = f.open(), sim = L.sim;
  const d = (sim as unknown as { drill: { manAt(id: number, k: number): [number, number] | null } }).drill;
  const air = sim as unknown as { bomb(s: string, x: number, y: number, fl: unknown): void };
  expect(d).toBeTruthy();
  let moved = 0, unmanned = 0, bombs = 0, added = 0, n = 0;
  if (move) {
    const at = d.manAt.bind(d), drop = air.bomb.bind(sim);
    d.manAt = (id: number, k: number) => {
      const p = at(id, k);
      if (!p) return p;
      if (++n % 6 === 0) { unmanned++; return null; }
      const o = shift(n); moved++;
      return [p[0] + o[0], p[1] + o[1]];
    };
    air.bomb = (s: string, x: number, y: number, fl: unknown) => { const o = shift(5000 + bombs++); drop(s, x + o[0], y + o[1], fl); };
  }
  const laid = new Map<object, { x: number; y: number; r: number; at: number; until: number; id: number; c: number }>();
  const steps: string[] = [], marks: Mark[] = [];
  for (let k = 0; !sim.over && k < 40000 && (nudge === undefined || k < nudge + 20); k++) {
    if (nudge !== undefined && k === nudge) {
      // (a unit in the field whose nerve is off its ceiling, mor0, which recovery would put it straight back to)
      const u = sim.units.find((x) => x.state !== 'gone' && x.mor > 0.05 && x.mor < x.mor0 - 0.01);
      expect(u).toBeTruthy();
      u!.mor -= 1e-9;
    }
    const x = L.tick(0.1, 1);
    for (const e of x.events) if (e.what === 'shell') marks.push([e.x ?? NaN, e.y ?? NaN, String(e.ground), String(e.cal)]);
    if (move) {
      const cs = sim.craters;
      for (const c of cs) {
        let o = laid.get(c);
        if (!o) { o = { x: c.x, y: c.y, r: c.r, at: c.at ?? 0, until: c.until, id: c.id, c: CALS.indexOf(c.cal ?? 'field') }; laid.set(c, o); }
        const p = o.id * 0.7;
        c.x = o.x + 6 * Math.sin(k * 0.37 + p); c.y = o.y + 6 * Math.cos(k * 0.29 - p * 1.3);
        c.r = o.r * (0.4 + 1.2 * frac(p * 0.618 + k * 0.013));
        c.at = o.at - ((k + Math.abs(o.id)) % 9); c.until = o.until + (k % 5);
        c.id = o.id - 1000 * (1 + (k % 3)); c.cal = CALS[(o.c + k + Math.abs(o.id)) % CALS.length];
      }
      cs.reverse();
      if (k % 40 === 39) {
        const w = sim.field.w, h = sim.field.h, o = shift(9000 + added);
        cs.push({ id: -(90000 + added), kind: 'crater', x: w * frac(0.31 + added * 0.437) + o[0] / 4, y: h * frac(0.53 + added * 0.291) + o[1] / 4, r: 1.5, until: o[0] + 9999, at: sim.t, cal: 'field' });
        added++;
      }
    }
    steps.push(digest(sim, new Set<unknown>([d, sim.craters]), false) + digest(channels(x), new Set(), true));
  }
  return { steps, marks, moved, unmanned, bombs: marks.filter((m) => m[3] === 'bomb').length, craters: sim.craters.length, added };
}

describe('the burst marks (where a shell or bomb bursts, and its crater) are drawing: no rule reads them', () => {
  const seen = FIGHTS.map((f) => ({ f, as: fight(f, false), off: fight(f, true) }));

  it('moves every mark in the second fight, each its own way', () => {
    let moved = 0, unmanned = 0, bombs = 0, ground = 0, wet = 0;
    for (const s of seen) {
      expect(s.as.marks.length).toBeGreaterThanOrEqual(5);
      expect(s.off.marks.length).toBe(s.as.marks.length);
      // Every burst is somewhere else; no two by the same move.
      const dx = s.as.marks.map((m, i) => Math.round((s.off.marks[i][0] - m[0]) * 10) + ',' + Math.round((s.off.marks[i][1] - m[1]) * 10));
      expect(dx.filter((v) => v === '0,0').length).toBeLessThanOrEqual(Math.ceil(s.as.marks.length / 10));
      expect(new Set(dx).size).toBeGreaterThanOrEqual(Math.floor(s.as.marks.length * 0.8));
      // Every crater moved, and more of them than the shells made.
      expect(s.off.added).toBeGreaterThan(0);
      expect(s.off.craters).toBeGreaterThan(s.as.craters);
      moved += s.off.moved; unmanned += s.off.unmanned; bombs += s.as.bombs;
      ground += s.as.marks.filter((m, i) => m[2] !== s.off.marks[i][2]).length;
      wet += s.as.marks.concat(s.off.marks).filter((m) => inRiver(m[2])).length;
    }
    expect(moved).toBeGreaterThanOrEqual(25);
    expect(unmanned).toBeGreaterThanOrEqual(3);
    expect(bombs).toBeGreaterThanOrEqual(10);
    expect(ground).toBeGreaterThanOrEqual(8);
    expect(wet).toBeGreaterThanOrEqual(1);
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
  });
});
