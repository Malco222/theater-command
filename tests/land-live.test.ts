// The live ground battle (release 2): the land rules fought step by step as the 3D game shows them
// (src/battle/land/live.ts), and the doorway the game calls (unreal/TheaterCommand/Rules/land-bridge.ts).
//
// What must hold: a battle cut into many short ticks gives the very tracks, rounds and views of one fought in one go
// (so how fast the game asks never changes what is shown); what the screen draws from the deltas is never more than a
// kept point's slack from where the rules have each man; the player's orders reach the rules; and a man standing still
// costs nothing to keep.
import { describe, expect, it } from 'vitest';
import { KEEP_M, LiveKeeper, livePiece, type LiveDelta } from '../src/battle/land/live';
import type { MenOut } from '../src/battle/land/drill';

const UPTO = 30; // battle seconds fought in each test: past the first contact's set-up, and quick
const canon = (v: unknown): string =>
  JSON.stringify(v, (_k, x) => (x && typeof x === 'object' && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([a], [b]) => (a < b ? -1 : 1))) : x));

function fight(tick: number): LiveDelta[] {
  const L = livePiece('normandy-1944', 11, { me: 'att' });
  const out: LiveDelta[] = [];
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) out.push(JSON.parse(JSON.stringify(L.to(Math.min(UPTO, L.sim.t + tick), 100000))));
  return out;
}

describe('the live ground battle', () => {
  it('gives the same battle however finely the game asks for it', () => {
    const fine = fight(0.5), coarse = fight(UPTO);
    expect(fine.length).toBeGreaterThan(coarse.length);
    expect(canon(LiveKeeper.assemble(fine))).toBe(canon(LiveKeeper.assemble(coarse)));
    expect(canon(fine.flatMap((d) => d.rounds))).toBe(canon(coarse.flatMap((d) => d.rounds)));
    expect(canon(fine.flatMap((d) => d.views))).toBe(canon(coarse.flatMap((d) => d.views)));
    expect(canon(fine.flatMap((d) => d.events))).toBe(canon(coarse.flatMap((d) => d.events)));
    // Every round once, in order.
    const ns = fine.flatMap((d) => d.rounds.map((r) => r.n));
    expect(new Set(ns).size).toBe(ns.length);
    expect([...ns].sort((a, b) => a - b)).toEqual(ns);
  });

  it('draws every man within a kept point\'s slack of where the rules have him, at every step already fought', () => {
    const L = livePiece('normandy-1944', 11, { me: 'att' });
    // Where the drill really had a sample of the men, every step.
    const truth = new Map<number, Map<number, [number, number]>>();
    const K = (L as unknown as { keeper: { see: (...a: unknown[]) => void; men: Map<string, { n: number }> } }).keeper;
    const see = K.see.bind(K);
    K.see = (t: unknown, b: unknown, m: unknown, fo: unknown) => {
      see(t, b, m, fo);
      const mm = m as MenOut;
      const n = K.men.get(`${b}:${fo ? 'fo' : ''}${mm.k}${mm.j !== undefined ? `.${mm.j}` : ''}`)!.n;
      if (n % 11) return;
      let at = truth.get(n);
      if (!at) truth.set(n, (at = new Map()));
      at.set(Math.round((t as number) * 100), [mm.x, mm.y]);
    };
    // The tracks as the scene builds them from the deltas: kept points, and a provisional tail dropped once a later kept point is in.
    type T = { at: number[][]; tail: number[] | null; out?: number };
    const tracks: T[] = [];
    const sample = (tr: T, t: number): [number, number] => {
      const pts = tr.tail && tr.tail[0] > tr.at[tr.at.length - 1][0] ? [...tr.at, tr.tail] : tr.at;
      if (t <= pts[0][0]) return [pts[0][1], pts[0][2]];
      for (let i = 1; i < pts.length; i++) if (t <= pts[i][0]) {
        const a = pts[i - 1], b = pts[i], u = (t - a[0]) / (b[0] - a[0]);
        return [a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u];
      }
      const z = pts[pts.length - 1];
      return [z[1], z[2]];
    };
    let worst = 0, checked = 0, from = -1;
    while (!L.sim.over && L.sim.t < UPTO) {
      const d: LiveDelta = JSON.parse(JSON.stringify(L.tick(0.5)));
      for (const m of d.men) {
        const tr = (tracks[m.n] ??= { at: [], tail: null });
        if (m.at) tr.at.push(...m.at);
        if (m.out === -1) delete tr.out; else if (m.out !== undefined) tr.out = m.out;
      }
      for (const tr of tracks) if (tr?.tail && tr.tail[0] <= tr.at[tr.at.length - 1][0]) tr.tail = null;
      for (let i = 0; i < d.tails.length; i += 4) tracks[d.tails[i]].tail = [d.t, d.tails[i + 1], d.tails[i + 2], d.tails[i + 3]];
      for (const [n, at] of truth) {
        const tr = tracks[n];
        for (const [tc, [x, y]] of at) {
          const t = tc / 100;
          if (t <= from || t > d.t + 1e-9 || (tr.out !== undefined && t >= tr.out)) continue;
          const [sx, sy] = sample(tr, t);
          worst = Math.max(worst, Math.hypot(sx - x, sy - y));
          checked++;
        }
      }
      from = d.t + 1e-9;
    }
    expect(checked).toBeGreaterThan(10000);
    // KEEP_M, and the tenth of a metre each point is rounded to.
    expect(worst).toBeLessThanOrEqual(KEEP_M + 0.08);
  });

  it('keeps the recorder\'s very points for a man who stands, walks and stands again', () => {
    // The recorder's line test without the short cut for standing men, for comparison.
    const naive = (pts: number[][]): number[][] => {
      const at = [pts[0]], pend: number[][] = [];
      for (const p of pts.slice(1)) {
        const a = at[at.length - 1];
        const off = (q: number[]) => { const u = (q[0] - a[0]) / (p[0] - a[0]); return Math.hypot(q[1] - (a[1] + (p[1] - a[1]) * u), q[2] - (a[2] + (p[2] - a[2]) * u)) > KEEP_M || Math.abs(q[3] - (a[3] + (p[3] - a[3]) * u)) > 0.05; };
        if (pend.some(off)) { at.push(pend[pend.length - 1]); pend.length = 0; }
        pend.push(p);
      }
      if (pend.length) at.push(pend[pend.length - 1]);
      return at;
    };
    const walk: number[][] = [];
    let x = 10, y = 20, f = 0;
    for (let i = 0; i < 600; i++) {
      const t = Math.round(i) / 10;
      const phase = Math.floor(i / 60) % 4;
      if (phase === 1) { x += 0.13; y += 0.05 * Math.sin(i / 7); }
      if (phase === 3) { y -= 0.21; f = 1.2; }
      walk.push([Math.round(t * 100) / 100, Math.round(x * 10) / 10, Math.round(y * 10) / 10, f]);
    }
    const k = new LiveKeeper();
    const man = (p: number[]): MenOut => ({ k: 0, r: 'rifle', x: p[1], y: p[2], f: p[3], p: 'stand', c: '', h: false, b: false } as unknown as MenOut);
    const deltas: LiveDelta[] = [];
    for (const p of walk) {
      k.see(p[0], 1, man(p), false);
      if (Math.round(p[0] * 10) % 5 === 4) deltas.push({ t: p[0], ...k.drain(), rounds: [], events: [], views: [], phases: [], contact: null });
    }
    k.close();
    deltas.push({ t: 60, ...k.drain(), rounds: [], events: [], views: [], phases: [], contact: null });
    expect(LiveKeeper.assemble(deltas)[0].at).toEqual(naive(walk));
    // Standing still: no tails sent while he stands.
    const still = deltas.slice(2, 10).flatMap((d) => d.tails);
    expect(still).toEqual([]);
  });

  it('takes the player\'s orders, and his deployment only where the rules allow it', async () => {
    await import('../unreal/TheaterCommand/Rules/land-bridge');
    const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
    expect(TC.landPieces().map((p: { key: string }) => p.key)).toContain('normandy-1944');
    const h = TC.landStart('normandy-1944', null, { me: 'att', setup: true });
    expect(h.me).toBe('att');
    expect(h.setup.room).toHaveLength(4);
    const d0 = TC.landTick(1);
    expect(d0.t).toBe(0); // the clock does not run while he deploys
    const mine = (h.blocks as { id: number; side: string; kind: string; team?: unknown }[]).filter((b) => b.side === 'att');
    const units = d0.views[0].units as { id: number; x: number; y: number }[];
    const [x0, y0, x1, y1] = h.setup.room as number[];
    // Outside his room: refused, with the reason in plain words.
    const far = TC.landCanPlace(mine[mine.length - 1].id, units[0].x, y1 + 400);
    expect(far.ok).toBe(false);
    expect(far.why.length).toBeGreaterThan(5);
    // Inside it, where the rules allow: placed, and the men start again from there.
    const spots = mine.flatMap((b) => [[40, 0], [-40, 0], [0, -30], [60, -20]].map(([dx, dy]) => {
      const u = units.find((v) => v.id === b.id)!;
      return { id: b.id, x: Math.min(x1 - 20, Math.max(x0 + 20, u.x + dx)), y: Math.min(y1 - 20, Math.max(y0 + 20, u.y + dy)) };
    }));
    const spot = spots.find((s) => TC.landCanPlace(s.id, s.x, s.y).ok);
    expect(spot).toBeDefined();
    expect(TC.landPlace(spot!.id, spot!.x, spot!.y, 0).ok).toBe(true);
    const placed = TC.landTick(0.5);
    expect(placed.reset).toBe(true);
    expect(placed.t).toBe(0);
    expect(TC.landBegin().ok).toBe(true);
    let d = TC.landTick(2);
    expect(d.t).toBeGreaterThan(1.9);
    const enemy = (h.blocks as { id: number; side: string }[]).find((b) => b.side === 'def')!;
    type U = { id: number; taken?: boolean; orderIn?: number; order: { target: number } | null };
    const unit = (dl: { views: { units: U[] }[] }, id: number): U => dl.views[dl.views.length - 1].units.find((u) => u.id === id)!;
    // His order goes by radio: the block is his at once, and the order lands a few seconds later.
    expect(TC.landOrder({ do: 'attack', units: [mine[0].id], target: enemy.id }).ok).toBe(true);
    d = TC.landTick(1);
    expect(unit(d, mine[0].id).taken).toBe(true);
    expect(unit(d, mine[0].id).orderIn).toBeGreaterThan(0);
    d = TC.landTick(15);
    expect(unit(d, mine[0].id).orderIn).toBeUndefined();
    expect(unit(d, mine[0].id).order?.target).toBe(enemy.id);
    // Handed back to his staff.
    expect(TC.landOrder({ do: 'land:staff', units: [mine[0].id] }).ok).toBe(true);
    d = TC.landTick(1);
    expect(unit(d, mine[0].id).taken).toBeUndefined();
    // An order for the other side's block is not his to give.
    expect(TC.landOrder({ do: 'hold', units: [enemy.id] }).ok).toBe(false);
    expect(TC.landEnd()).toHaveProperty('over');
  });
});
