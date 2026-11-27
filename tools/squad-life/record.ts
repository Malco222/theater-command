// Records one battle's foot blocks as the old drill and as squad life draw them, for the before/after picture (the
// battle itself is the same in both, so the two play side by side in step).
//   node dist/record.mjs [piece] [seed] [from] [to] [out.json]
import { writeFileSync } from 'node:fs';
import { livePiece } from '../../src/battle/land/live';

const [piece = 'normandy-1944', seedS = '11', fromS = '0', toS = '120', out = 'record.json'] = process.argv.slice(2);
const NOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
const run = (life: boolean) => {
  const L = livePiece(piece, Number(seedS), { me: null, platoons: false, ...(life ? { life: true } : {}) } as never);
  const squads = (L.drill as any).squads as Map<number, any>;
  const frames: any[] = [];
  while (!L.sim.over && L.sim.t < Number(toS) - 1e-6) {
    L.to(L.sim.t + 0.25, 100000);
    if (L.sim.t < Number(fromS)) continue;
    const blocks: any[] = [];
    for (const sq of squads.values()) {
      const u = sq.u;
      if (u.kind !== 'inf' && u.kind !== 'mot' && u.kind !== 'mtn' && u.kind !== 'eng') continue;
      const men = sq.men.filter((m: any) => m.fell === undefined && !m.job && !NOT.has(m.role)).map((m: any) => [Math.round(m.x * 10) / 10, Math.round(m.y * 10) / 10, Math.round(m.face * 100) / 100, m.pose, m.aim ? 1 : 0, m.sec ?? m.k, m.role === 'mg' ? 1 : m.lead || (m.j === undefined && m.hasSec) ? 2 : 0, m.n]);
      blocks.push({ id: u.id, side: u.side, ph: sq.phase, x: Math.round(u.x), y: Math.round(u.y), w: Math.round(u.w), d: Math.round(u.d), f: Math.round(u.face * 100) / 100, men });
    }
    frames.push({ t: Math.round(L.sim.t * 100) / 100, blocks });
  }
  return frames;
};
const before = run(false), after = run(true);
writeFileSync(out, JSON.stringify({ piece, seed: Number(seedS), before, after }));
// A summary of each block's phases over the window, to pick the one to show.
const phases = new Map<number, string[]>();
for (const f of after) for (const b of f.blocks) { const l = phases.get(b.id) ?? []; if (l[l.length - 1] !== b.ph) l.push(`${b.ph}@${Math.round(f.t)}`); phases.set(b.id, l); }
for (const [id, l] of phases) { const b = after[after.length - 1].blocks.find((x: any) => x.id === id); console.log(id, b?.side, 'men', b?.men.length, l.join(' ')); }
