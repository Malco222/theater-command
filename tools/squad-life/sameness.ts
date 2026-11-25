// The sameness meter (Brian, 25 November 2026, 13:40Z: "i dont want all the soldiers of a squad doing the exact same
// thing at the same time. I want them to work together and look real as individuals apart of a squad").
// Plays a battle as the 3D game starts it and reads, twice a battle second, what the men of every group on foot (a
// section, or a platoon's squad) are doing: how many share one pose, how often a whole group changes pose in the same
// half second, how alike they face. Looks only: reads the drill, changes nothing.
//
//   node dist/sameness.mjs [piece] [seed] [upto] [mode]      mode: shipped | book | platoons | life
import { livePiece, type LiveOpts } from '../../src/battle/land/live';

interface M { k: number; j?: number; n: number; sec?: number; sub?: number; role: string; pose: string; x: number; y: number; face: number; fell?: number; job?: unknown; v: number; aim?: [number, number]; act?: string }
interface Sq { u: { id: number; side: string; kind: string; state: string }; men: M[]; phase: string }

const [piece = 'normandy-1944', seedS = '11', uptoS = '600', mode = 'shipped'] = process.argv.slice(2);
const opts: LiveOpts & { book?: boolean; life?: boolean } = { me: null, platoons: mode === 'platoons' };
if (mode === 'book' || mode === 'life') opts.book = true;
if (mode === 'life') opts.life = true;
const L = livePiece(piece, Number(seedS), opts);
const squads = (L.drill as unknown as { squads: Map<number, Sq> }).squads;
const VEH = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
/** The gaits: a whole squad up and running together is one thing; a whole squad frozen in one pose is the fault. */
const GAITS = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'ride', 'mounted']);
const grp = (m: M): number => (m.sec ?? m.k) * 8 + (m.sub ?? 0);
/**
 * What the eye reads a man as doing in the 3D game: the clip his pose is drawn with (LandMen.cpp StanceOf: a man lying
 * still at his rifle is drawn as one firing prone, one kneeling and watching as one firing kneeling), and whether he is
 * firing. Two men count as doing the same thing only where the game would draw them the same.
 */
const CLIP: Record<string, string> = { hide: 'prone', fire_prone: 'prone', wait: 'kneel', fire_kneel: 'kneel', throw_grenade: 'stand', fire_stand: 'stand', lmg_burst: 'prone-gun' };
const FIRES = new Set(['fire_prone', 'fire_kneel', 'fire_stand', 'lmg_burst', 'throw_grenade']);
const doing = (m: M): string => `${m.role === 'mg' || m.pose !== 'lmg_burst' ? CLIP[m.pose] ?? m.pose : 'prone'}${FIRES.has(m.pose) && m.aim ? '*' : ''}`;

interface Acc { n: number; modal: number; clone: number; frozen: number; mass: number; changes: number; faceSame: number; kinds: number; mins: number }
const acc = new Map<string, Acc>();
const A = (ph: string): Acc => { let a = acc.get(ph); if (!a) acc.set(ph, (a = { n: 0, modal: 0, clone: 0, frozen: 0, mass: 0, changes: 0, faceSame: 0, kinds: 0, mins: 0 })); return a; };
const last = new Map<string, string>();
/** What the groups caught all alike were doing, by phase (to see where the sameness is left). */
const alike = new Map<string, number>();
const UPTO = Number(uptoS);
while (!L.sim.over && L.sim.t < UPTO - 1e-6) {
  L.to(L.sim.t + 0.5, 100000);
  for (const sq of squads.values()) {
    if (sq.u.kind === 'arm' || sq.u.kind === 'art' || sq.u.state === 'gone' || sq.u.state === 'withdrawn') continue;
    const groups = new Map<number, M[]>();
    for (const m of sq.men) {
      if (m.fell !== undefined || m.job || VEH.has(m.role)) continue;
      const g = grp(m); let a = groups.get(g); if (!a) groups.set(g, (a = [])); a.push(m);
    }
    for (const [g, men] of groups) {
      if (men.length < 5) continue;
      const a = A(sq.phase), all = A('ALL');
      const count = new Map<string, number>();
      for (const m of men) count.set(doing(m), (count.get(doing(m)) ?? 0) + 1);
      const top = Math.max(...count.values()) / men.length;
      // Pose changes this half second, and the most that changed to one same thing together.
      const to = new Map<string, number>(); let ch = 0;
      for (const m of men) {
        const key = `${sq.u.id}:${m.k}:${m.j ?? -1}`, was = last.get(key), now = doing(m);
        if (was !== undefined && was !== now) { ch++; to.set(now, (to.get(now) ?? 0) + 1); }
        last.set(key, now);
      }
      const together = to.size ? Math.max(...to.values()) / men.length : 0;
      // Facing: the share within 6 degrees of the group's middle facing.
      const sx = men.reduce((s, m) => s + Math.sin(m.face), 0), sy = men.reduce((s, m) => s + Math.cos(m.face), 0), mid = Math.atan2(sx, sy);
      const same = men.filter((m) => Math.abs(Math.atan2(Math.sin(m.face - mid), Math.cos(m.face - mid))) < 0.105).length / men.length;
      const topPose = [...count.entries()].sort((p, q) => q[1] - p[1])[0][0], halted = top >= 0.9 && !GAITS.has(topPose.replace('*', ''));
      if (top >= 0.9) { const what = `${sq.phase}:${[...count.entries()].sort((p, q) => q[1] - p[1])[0][0]}`; alike.set(what, (alike.get(what) ?? 0) + 1); }
      for (const x of [a, all]) { x.n++; x.modal += top; if (top >= 0.9) x.clone++; if (halted) x.frozen++; if (together >= 0.6) x.mass++; x.changes += ch; x.faceSame += same; x.kinds += count.size; x.mins += 0.5 / 60; }
      void g;
    }
  }
}
const rows = [...acc.entries()].sort((a, c) => (a[0] === 'ALL' ? 1 : c[0] === 'ALL' ? -1 : c[1].n - a[1].n));
console.log(`${piece} seed ${seedS} to ${L.sim.t.toFixed(0)} s, mode ${mode}: groups of 5+ men on foot, read every half second`);
console.log('phase       samples  same-pose  alike%  frozen-alike%  kinds  mass-switch/min  face-alike');
for (const [ph, a] of rows) {
  console.log(`${ph.padEnd(11)} ${String(a.n).padStart(7)}  ${(100 * a.modal / a.n).toFixed(1).padStart(8)}%  ${(100 * a.clone / a.n).toFixed(1).padStart(5)}%  ${(100 * a.frozen / a.n).toFixed(1).padStart(12)}%  ${(a.kinds / a.n).toFixed(2).padStart(5)}  ${(a.mass / a.mins).toFixed(2).padStart(15)}  ${(100 * a.faceSame / a.n).toFixed(1).padStart(9)}%`);
}
console.log('all alike, doing what: ' + [...alike.entries()].sort((p, q) => q[1] - p[1]).slice(0, 12).map(([k, v]) => `${k} ${v}`).join(', '));
const all = acc.get('ALL')!;
console.log(JSON.stringify({ piece, seed: Number(seedS), mode, t: Math.round(L.sim.t), samples: all.n, samePose: +(100 * all.modal / all.n).toFixed(1), clones: +(100 * all.clone / all.n).toFixed(1), frozen: +(100 * all.frozen / all.n).toFixed(1), kinds: +(all.kinds / all.n).toFixed(2), massPerMin: +(all.mass / all.mins).toFixed(2), faceAlike: +(100 * all.faceSame / all.n).toFixed(1), units: L.sim.units.map((u) => [u.id, +u.str.toFixed(6), u.figures, u.state, +u.x.toFixed(3), +u.y.toFixed(3)]) }));
