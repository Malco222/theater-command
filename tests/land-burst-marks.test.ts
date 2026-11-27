// The burst marks are drawing (the design director, 27 November 2026, 05:00Z, on the gun crews' tick-by-tick proof:
// teamwork-tick-proof-1127/PROOF.md). With the men drawn, sim.ts shell() lays each shell's burst on the drawn man it
// costs (Drill.manAt): its 'shell' event's x, y and ground, and its crater. So the drawing reaches the burst marks, and a
// switch that moves drawn men (Drill.teamwork's crews at their posts) moves them. That is the one place the rules take
// anything from the drawing, and it must stay one-way: no rule may ever read a crater or a shell's spot. This test pins
// it, as the director asked: "a test that moves every crater and shell spot and shows the rules record unchanged".
// Each battle is a set piece fought live, as the 3D game steps it (LiveBattle.tick, a step at a time), with the gun
// crews drawn at their posts (teamwork) and the drill book riding (life). It is fought twice. In the second, every
// burst is laid 7 m off the drawn man it costs (manAt moved), and after every step every crater is moved and resized
// about the spot it was laid on. The rules record must not move at any step: the whole LandBattle (every unit's place,
// strength, nerve, state and target, every list, map and counter, its wrecks) with the drill and the craters left out,
// and each step's battle channels (t, events, views, contact, cuts, over, end) with a shell's x, y and ground and the
// views' crater zones left out. The drawn channels (men, tails, rounds, phases) are the drawing and are not read. A
// rule that came to read a crater or a shell's spot would fail it. A control takes 1e-9 off one unit's nerve (a unit in
// the field, its nerve below its ceiling) after contact, and shows the record catches it at that very step.
import { describe, expect, it } from 'vitest';
import { livePiece, type LiveBattle, type LiveDelta } from '../src/battle/land/live';

const FIGHTS: Array<{ key: string; seed: number; me: 'att' | 'def' }> = [
  { key: 'prokhorovka-1943', seed: 3, me: 'att' },
  { key: 'prokhorovka-1943', seed: 4, me: 'def' },
  { key: 'normandy-1944', seed: 2, me: 'def' },
];
const MARK = new Set(['x', 'y', 'ground']);

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

/** A canonical digest: Maps and Sets in their own order, typed arrays by bytes, a revisited object by the order it was
 *  first met, functions and anything in `skip` left out, and a 'shell' event's marks (x, y, ground) left out. */
function digest(root: unknown, skip: Set<unknown>): string {
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
    const shell = (v as { what?: unknown }).what === 'shell';
    h.update(`o${(v as object).constructor?.name ?? ''}{`);
    for (const k of Object.keys(v)) { if (shell && MARK.has(k)) continue; h.update(`k${k}:`); walk((v as Record<string, unknown>)[k]); }
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

interface Fought { steps: string[]; marks: string; moved: number; shells: number; craters: number }

/** One set piece fought live with the gun crews drawn. `move`: every burst laid off its drawn man, every crater moved
 *  after each step. `nudge`: one unit's nerve moved by 1e-9 at that step, and the fight stopped a few steps on. */
function fight(f: (typeof FIGHTS)[number], move: boolean, nudge?: number): Fought {
  const L: LiveBattle = livePiece(f.key, f.seed, { teamwork: true, life: true, me: f.me });
  const sim = L.sim, d = (sim as unknown as { drill: { manAt(id: number, k: number): [number, number] | null } }).drill;
  expect(d).toBeTruthy();
  let moved = 0, shells = 0;
  if (move) {
    const at = d.manAt.bind(d);
    d.manAt = (id: number, k: number) => { const p = at(id, k); if (!p) return p; moved++; return [p[0] + 7, p[1] - 5]; };
  }
  const laid = new Map<object, [number, number, number]>();
  const steps: string[] = [], marks: number[][] = [];
  for (let k = 0; !sim.over && k < 40000 && (nudge === undefined || k < nudge + 20); k++) {
    if (nudge !== undefined && k === nudge) {
      // (a unit in the field whose nerve is off its ceiling, mor0, which recovery would put it straight back to)
      const u = sim.units.find((x) => x.state !== 'gone' && x.mor > 0.05 && x.mor < x.mor0 - 0.01);
      expect(u).toBeTruthy();
      u!.mor -= 1e-9;
    }
    const x = L.tick(0.1, 1);
    for (const e of x.events) if (e.what === 'shell') { shells++; marks.push([e.x ?? NaN, e.y ?? NaN]); }
    if (move) for (const c of sim.craters) {
      let o = laid.get(c);
      if (!o) { o = [c.x, c.y, c.r]; laid.set(c, o); }
      c.x = o[0] + 6 * Math.sin(k * 0.37 + c.id); c.y = o[1] + 6 * Math.cos(k * 0.29 - c.id); c.r = o[2] * (1.25 + 0.5 * Math.sin(k * 0.11));
    }
    steps.push(digest(sim, new Set<unknown>([d, sim.craters])) + digest(channels(x), new Set()));
  }
  return { steps, marks: JSON.stringify(marks), moved, shells, craters: sim.craters.length };
}

describe('the burst marks (where a shell bursts, and its crater) are drawing: no rule reads them', () => {
  const seen = FIGHTS.map((f) => ({ f, as: fight(f, false), off: fight(f, true) }));

  it('moves the marks in the second fight: every burst laid off its drawn man, every crater moved', () => {
    let bursts = 0;
    for (const s of seen) {
      expect(s.as.shells).toBeGreaterThanOrEqual(5);
      expect(s.off.moved).toBeGreaterThanOrEqual(5);
      expect(s.as.craters).toBeGreaterThan(0);
      expect(s.off.marks).not.toBe(s.as.marks);
      bursts += s.off.moved;
    }
    expect(bursts).toBeGreaterThanOrEqual(20);
  });

  it('leaves the rules record the same at every step', () => {
    for (const s of seen) {
      expect(s.off.steps.length).toBe(s.as.steps.length);
      expect(s.off.steps.findIndex((x, i) => x !== s.as.steps[i])).toBe(-1);
    }
  });

  it('catches a rules change at once (control: 1e-9 off one unit\'s nerve at step 1500, after contact)', () => {
    const s = seen[0], nudged = fight(s.f, false, 1500);
    expect(nudged.steps.length).toBe(1520);
    expect(nudged.steps.findIndex((x, i) => x !== s.as.steps[i])).toBe(1500);
  });
});
