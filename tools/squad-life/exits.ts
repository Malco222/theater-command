// The exits meter (Brian, 25 November 2026, 17:11Z: "some soldiers freeze and then disapear"; Drill.exits). Fights the
// four battles of tests/land-exits.test.ts as the 3D game is fed them, with the switch off and on, and counts the living
// men whose track the feed ends before the battle does (an out): how many, how many of them go out inside the field,
// how many are standing still as they go (their last kept stretch has no length, or they are in a still pose), and the
// middle of their distances from their own edge at their last kept point (metres inside the field; below 0, beyond it).
// Looks only: reads the feed, changes nothing.
//
//   npx esbuild tools/squad-life/exits.ts --bundle --platform=node --format=esm --outfile=dist/exits.mjs && node dist/exits.mjs [a b c d]
import { LiveBattle, LiveKeeper, livePiece, type LiveDelta, type LiveOpts } from '../../src/battle/land/live';
import { sampleTicket, withTeams } from '../../src/battle/land/sample';

type Which = 'a' | 'b' | 'c' | 'd';
const SHIPPED = { platoons: false, bearers: false, smoke: false, scars: false, mud: false, blast: true, escort: true, mech: true, spg: true, td: true, cars: true, aa: true, light: true };
const STILL = new Set(['cower', 'wait', 'hide', 'idle', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire', 'halt', 'parked', 'deployed', 'observe']);
const NAME: Record<Which, string> = { a: 'urban 1.1 seed 2', b: 'small desert 1.4 seed 3', c: 'normandy-1944 seed 3, all fall back at 60 s', d: 'prokhorovka-1943 seed 5, 3 blocks withdrawn at 80 s' };

function open(w: Which, exits: boolean): LiveBattle {
  const opts = { ...SHIPPED, exits } as LiveOpts;
  if (w === 'c') return livePiece('normandy-1944', 3, { ...opts, me: 'att' });
  if (w === 'd') return livePiece('prokhorovka-1943', 5, { ...opts, me: 'def' });
  const seed = w === 'a' ? 2 : 3, nat = w === 'b' ? { att: 'UK', def: 'GER' } : { att: 'SOV', def: 'GER' };
  const t0 = sampleTicket(w === 'a' ? 'urban' : 'desert', w === 'a' ? 1.1 : 1.4, { seed: seed * 7919 + 13, nat, humanAtt: false, date: 'June 1944' });
  if (w === 'b') {
    const pick = <F extends { share: number }>(fs: F[]): F[] => { const k = [fs[0], fs[2]], tot = k[0].share + k[1].share; return k.map((f) => ({ ...f, share: f.share / tot })); };
    t0.att = { ...t0.att, forces: pick(t0.att.forces) };
    t0.def = { ...t0.def, forces: pick(t0.def.forces) };
  }
  const t = withTeams(t0);
  (t as { turn: number }).turn = 58;
  return new LiveBattle(t, null, opts);
}

function measure(w: Which, exits: boolean): string {
  const L = open(w, exits), deltas: LiveDelta[] = [];
  let ordered = false;
  while (!L.sim.over) {
    if (!ordered && w === 'c' && L.sim.t >= 60) { ordered = true; L.order({ do: 'land:fallBack' } as never); }
    if (!ordered && w === 'd' && L.sim.t >= 80) {
      ordered = true;
      const ids = L.sim.units.filter((u) => u.side === 'def' && (u.kind === 'inf' || u.kind === 'arm' || u.kind === 'mot') && u.state !== 'gone' && u.state !== 'withdrawn' && u.state !== 'routing').slice(0, 3).map((u) => u.id);
      L.order({ do: 'withdraw', units: ids } as never);
    }
    deltas.push(L.tick(1, 50));
  }
  const end = L.sim.t, h = L.sim.field.h;
  const outs = LiveKeeper.assemble(deltas).filter((tr) => tr.out !== undefined && tr.d === undefined && tr.out < end - 1e-6);
  let inside = 0, still = 0;
  const gaps: number[] = [];
  for (const tr of outs) {
    const p1 = tr.at[tr.at.length - 1], p0 = tr.at[tr.at.length - 2], pose = String(tr.ch[tr.ch.length - 1][1]);
    const v = p0 ? Math.hypot(p1[1] - p0[1], p1[2] - p0[2]) / Math.max(1e-6, p1[0] - p0[0]) : 0;
    const side = L.sim.unit(tr.b)?.side;
    if (p1[2] > 0 && p1[2] < h) inside++;
    if (v <= 0.05 || STILL.has(pose)) still++;
    gaps.push(side === 'att' ? p1[2] : h - p1[2]);
  }
  gaps.sort((a, b) => a - b);
  const mid = gaps.length ? gaps[Math.floor(gaps.length / 2)] : NaN;
  return `${w} ${NAME[w].padEnd(52)} exits ${exits ? 'on ' : 'off'}  living outs ${String(outs.length).padStart(4)}  inside ${String(inside).padStart(4)}  still ${String(still).padStart(4)}  median from edge ${Number.isNaN(mid) ? '-' : mid.toFixed(1)} m`;
}

const which = (process.argv.slice(2).length ? process.argv.slice(2) : ['a', 'b', 'c', 'd']) as Which[];
for (const w of which) for (const exits of [false, true]) console.log(measure(w, exits));
