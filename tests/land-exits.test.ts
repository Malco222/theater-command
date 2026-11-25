// Men who leave the field are seen leaving (release 2; Brian, 25 November 2026, 17:11Z: "some soldiers freeze and then
// disapear"; the hunt's freeze-vanish-1125, ranks 3 and 4). Drill.exits, looks only (ruling 217): the men of a routing
// block run on past the field's edge instead of halting and cowering on it, and the men, vehicles and spotters of a
// block that has left the fight (fled off the field, or withdrawn at its out line) stay in view(), marked lv, each
// going for his own edge at his own moment and pace, and are taken off the list one by one once past it.
//
// What must hold: the battle is the same with the switch on, off, and with no drill at all; off, view() is the same as
// without the switch, step for step; on, every living man who leaves the feed before the end leaves it on the move and
// beyond his own edge (or at the cap), fires at nothing and is fired at by nothing; the men of one block do not leave in
// lockstep (Brian, 13:40Z: "i dont want all the soldiers of a squad doing the exact same thing at the same time"); and
// the feed's contract with the 3D game (a man's out is 0.1 s after his last kept point) still stands.
import { describe, expect, it } from 'vitest';
import { LiveBattle, LiveKeeper, livePiece, type LiveDelta, type LiveOpts } from '../src/battle/land/live';
import { LandBattle } from '../src/battle/land/sim';
import { sampleTicket, withTeams } from '../src/battle/land/sample';
import { DRILL, type MenBlock, type MenOut, type Round } from '../src/battle/land/drill';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';

type Which = 'a' | 'b' | 'c' | 'd';
/** The options the game ships with (the hunt's probe): looks on but bearers, smoke, scars, mud and platoons. */
const SHIPPED = { platoons: false, bearers: false, smoke: false, scars: false, mud: false, blast: true, escort: true, mech: true, spg: true, td: true, cars: true, aa: true, light: true };
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
const CARRIED = new Set(['ride', 'mounted', 'horses']);
const FIRING = new Set(['fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire']);
const STILL = new Set(['cower', 'wait', 'hide', ...FIRING]);
const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);

/** The campaign tickets of (a) and (b), as the hunt's probe made them (June 1944: the armies' carriers, guns and cars of the month). */
function sample(which: 'a' | 'b'): FieldTicket<LandPlace> {
  const seed = which === 'a' ? 2 : 3, terrain = which === 'a' ? 'urban' : 'desert';
  const nat = which === 'b' ? { att: 'UK', def: 'GER' } : { att: 'SOV', def: 'GER' };
  const t0 = sampleTicket(terrain, which === 'a' ? 1.1 : 1.4, { seed: seed * 7919 + 13, nat, humanAtt: false, date: 'June 1944' });
  if (which === 'b') {
    // A small battle: two corps a side, their shares kept in proportion.
    const pick = <F extends { share: number }>(fs: F[]): F[] => { const k = [fs[0], fs[2]], tot = k[0].share + k[1].share; return k.map((f) => ({ ...f, share: f.share / tot })); };
    t0.att = { ...t0.att, forces: pick(t0.att.forces) };
    t0.def = { ...t0.def, forces: pick(t0.def.forces) };
  }
  const t = withTeams(t0);
  (t as { turn: number }).turn = 58;
  return t;
}

/** The four battles: (a) urban 1.1 seed 2 and (b) small desert 1.4 seed 3, computer against computer; (c) Normandy, me att, All fall back at 60 s; (d) Prokhorovka, me def, three blocks withdrawn at 80 s. */
function open(which: Which, exits: boolean | undefined): LiveBattle {
  const opts = { ...SHIPPED, ...(exits === undefined ? {} : { exits }) } as LiveOpts;
  if (which === 'c') return livePiece('normandy-1944', 3, { ...opts, me: 'att' });
  if (which === 'd') return livePiece('prokhorovka-1943', 5, { ...opts, me: 'def' });
  return new LiveBattle(sample(which), null, opts);
}

/** The player's order, given once its time has come. */
function order(which: Which, L: LiveBattle): void {
  const s = L.sim;
  if (which === 'c') L.order({ do: 'land:fallBack' } as never);
  if (which === 'd') {
    const ids = s.units.filter((u) => u.side === 'def' && (u.kind === 'inf' || u.kind === 'arm' || u.kind === 'mot') && u.state !== 'gone' && u.state !== 'withdrawn' && u.state !== 'routing').slice(0, 3).map((u) => u.id);
    L.order({ do: 'withdraw', units: ids } as never);
  }
}
const ORDER_AT: Record<Which, number> = { a: Infinity, b: Infinity, c: 60, d: 80 };

/** The battle's outcome: its end time, how it ended, who won, and every block's figures, strength and state. */
const sig = (b: LandBattle): string => JSON.stringify({ t: b.t, how: b.over?.how, winner: b.over?.winner, units: b.units.map((u) => [u.id, u.figures, u.str, u.state]) });

const keyOf = (b: number, m: MenOut, fo: boolean): string => `${b}:${fo ? 'fo' : ''}${m.k}${m.j !== undefined ? `.${m.j}` : ''}`;

/** What one run saw of its leaving men, view by view. */
interface Leaver { b: number; first: number; seen: number; last: MenOut; pace: number; runs: [number, number][]; foot: boolean; mounted: boolean }
/**
 * One run: its feed, outcome and leaving men; bad, a leaving man aiming, hidden or firing; and, step by step, a leaving man
 * who stops dead though he was on the move the step before (halted), one who jumps more than 2.5 m in a step (jumps), and
 * one sitting aboard while no vehicle of his block is in view (aboard).
 */
interface Run { which: Which; L: LiveBattle; deltas: LiveDelta[]; sig: string; end: number; h: number; leavers: Map<string, Leaver>; last: Map<string, MenOut>; bad: string[]; views: string[]; prev: Map<string, [number, MenOut]>; halted: string[]; jumps: string[]; aboard: string[] }

/** Watches every view() the live feed takes: each man's last row, and each leaving man's first and last time, step-off and pace. */
function watch(r: Run, json: boolean, men: boolean): void {
  const drill = r.L.drill as unknown as { view(as?: string): MenBlock[] };
  const orig = drill.view.bind(drill);
  drill.view = (as?: string): MenBlock[] => {
    const out = orig(as), t = Math.round(r.L.sim.t * 100) / 100;
    if (json) r.views.push(JSON.stringify(out));
    const over = !!r.L.sim.over;
    if (men) for (const blk of out) {
      const rides = blk.men.some((m) => m.lv && (m.r === 'lorry' || m.r === 'tank'));
      for (const [list, fo] of [[blk.men, false], [blk.fo ?? [], true]] as [MenOut[], boolean][]) for (const m of list) {
        const key = keyOf(blk.id, m, fo), p = r.prev.get(key);
        r.prev.set(key, [t, m]);
        if (!m.lv) { r.last.delete(key); continue; }
        r.last.set(key, m);
        if (m.a || m.h || FIRING.has(m.p)) r.bad.push(`${t} ${key} ${m.p}${m.a ? ' aim' : ''}${m.h ? ' hidden' : ''}`);
        if (!over && p && t - p[0] < 0.11) {
          if (MOVING.has(p[1].p) && p[1].v > 0 && m.v === 0) r.halted.push(`${t} ${key} ${p[1].p} to ${m.p}`);
          const step = Math.hypot(m.x - p[1].x, m.y - p[1].y);
          if (step > 2.5) r.jumps.push(`${t} ${key} ${m.r} ${step.toFixed(1)} m`);
        }
        if (!over && m.p === 'mounted' && !rides) r.aboard.push(`${t} ${key}`);
        let l = r.leavers.get(key);
        if (!l) r.leavers.set(key, (l = { b: blk.id, first: t, seen: t, last: m, pace: 0, runs: [], foot: !NOT_FOOT.has(m.r) && !CARRIED.has(m.p), mounted: CARRIED.has(m.p) }));
        l.seen = t; l.last = m;
        // (his pace once he is running: the drill's own speed for him, as sent, at the last he ran)
        if (m.p === 'run' && m.v > 0) { l.pace = m.v; l.runs.push([t, m.v]); }
      }
    }
    return out;
  };
}

/** Fights battles in step, a battle second a tick as the game asks, the player's order given in each at the same moment; calls each() after every tick. */
function fight(runs: Run[], each?: () => void): void {
  let ordered = false;
  for (let guard = 0; guard < 4000 && runs.some((r) => !r.L.sim.over); guard++) {
    const t = runs[0].L.sim.t;
    if (!ordered && t >= ORDER_AT[runs[0].which]) { ordered = true; for (const r of runs) order(r.which, r.L); }
    for (const r of runs) if (!r.L.sim.over) r.deltas.push(r.L.tick(1, 50));
    each?.();
  }
  for (const r of runs) { r.sig = sig(r.L.sim); r.end = Math.round(r.L.sim.t * 100) / 100; }
}

function make(which: Which, exits: boolean | undefined, json = false): Run {
  const L = open(which, exits);
  const r: Run = { which, L, deltas: [], sig: '', end: 0, h: L.sim.field.h, leavers: new Map(), last: new Map(), bad: [], views: [], prev: new Map(), halted: [], jumps: [], aboard: [] };
  watch(r, json, exits === true);
  return r;
}

/** The living men whose track got an out before the battle ended, with their track. */
function outs(r: Run): { key: string; at: number[][]; ch: (string | number | null)[][]; out: number }[] {
  return LiveKeeper.assemble(r.deltas).filter((tr) => tr.out !== undefined && tr.d === undefined && tr.out < r.end - 1e-6)
    .map((tr) => ({ key: `${tr.b}:${tr.fo ? 'fo' : ''}${tr.k}${tr.j !== undefined ? `.${tr.j}` : ''}`, at: tr.at, ch: tr.ch, out: tr.out! }));
}

/** The time each block's men began to leave (its first leaving man's). */
const leftAt = (r: Run): Map<number, number> => {
  const m = new Map<number, number>();
  for (const l of r.leavers.values()) m.set(l.b, Math.min(m.get(l.b) ?? Infinity, l.first));
  return m;
};
/** A man taken off at the cap (DR.exits.cap after his block left). */
const capped = (r: Run, key: string, out: number): boolean => {
  const l = r.leavers.get(key);
  return !!l && out - (leftAt(r).get(l.b) ?? out) >= DRILL.exits.cap - 0.15;
};
/** Living outs whose last kept point is inside the field (the cap aside). */
const inside = (r: Run): number => outs(r).filter((o) => { const y = o.at[o.at.length - 1][2]; return y > 0 && y < r.h && !capped(r, o.key, o.out); }).length;

// The runs, fought once and shared by the tests: each battle with the switch on and off; (a) and (c) off fought in
// step with the same battle without the switch, every view compared as it is taken.
const on = new Map<Which, Run>(), off = new Map<Which, Run>();
let mismatch: string[] = [];
const get = (which: Which, exits: boolean): Run => {
  const map = exits ? on : off;
  if (!map.has(which)) {
    if (exits) { const r = make(which, true); fight([r]); map.set(which, r); }
    else if (which === 'a' || which === 'c') {
      // Off identical: the switch false and the switch absent, view by view.
      const f = make(which, false, true), n = make(which, undefined, true);
      fight([f, n], () => {
        if (f.views.length !== n.views.length || f.views.some((v, i) => v !== n.views[i])) mismatch.push(`${which} t ${f.L.sim.t}`);
        f.views.length = 0; n.views.length = 0;
      });
      if (f.sig !== n.sig) mismatch.push(`${which} outcome`);
      map.set(which, f);
    } else { const r = make(which, false); fight([r]); map.set(which, r); }
  }
  return map.get(which)!;
};
const ALL: Which[] = ['a', 'b', 'c', 'd'];

describe('men who leave the field are seen leaving (Drill.exits, looks only)', () => {
  it('ends every battle the same with the switch on, off, and with no drill at all', { timeout: 600000 }, () => {
    for (const w of ALL) expect(get(w, true).sig, w).toBe(get(w, false).sig);
    // The rules alone, with no drill, against the drill with the switch on (its key men only, to keep the test quick):
    // the plain battle's path, (b) (a rout off the edge).
    for (const w of ['b'] as const) {
      const flags = (b: LandBattle): LandBattle => { Object.assign(b, { blast: true, escort: true, mech: true, spg: true, td: true, cars: true, aa: true, light: true }); return b; };
      const bare = flags(new LandBattle(sample(w))), drawn = flags(new LandBattle(sample(w)));
      drawn.drillOn({ exits: true });
      for (let k = 0; k < 40000 && !bare.over; k++) bare.step();
      for (let k = 0; k < 40000 && !drawn.over; k++) drawn.step();
      expect(sig(drawn), w).toBe(sig(bare));
    }
  });

  it('with the switch off (false or absent) sends the very view() of a battle without it, step for step', () => {
    get('a', false); get('c', false);
    expect(mismatch).toEqual([]);
  });

  it('lets every living man who leaves the feed go on the move, beyond his own edge or at the cap, marked lv', () => {
    for (const w of ALL) {
      const r = get(w, true), bad: string[] = [];
      for (const o of outs(r)) {
        const last = r.last.get(o.key), ch = o.ch[o.ch.length - 1], pose = String(ch[1]);
        const [p1, p0] = [o.at[o.at.length - 1], o.at[o.at.length - 2]];
        const v = p0 ? Math.hypot(p1[1] - p0[1], p1[2] - p0[2]) / Math.max(1e-6, p1[0] - p0[0]) : 0;
        const y = p1[2];
        if (!last?.lv) bad.push(`${o.key} not lv`);
        if (!(MOVING.has(pose) || CARRIED.has(pose)) || STILL.has(pose)) bad.push(`${o.key} pose ${pose}`);
        if (!(v > 0)) bad.push(`${o.key} still`);
        if (y > 0 && y < r.h && !capped(r, o.key, o.out)) bad.push(`${o.key} inside y=${y}`);
      }
      expect(bad.slice(0, 10), w).toEqual([]);
    }
    // There are leaving men to see: a rout off the edge in (b), the fall back in (c), the withdraw in (d).
    for (const w of ['b', 'c', 'd'] as const) expect(get(w, true).leavers.size, w).toBeGreaterThan(20);
  });

  it('never has a leaving man fire or be fired at, aim, hide or stand in a firing pose', () => {
    for (const w of ALL) {
      const r = get(w, true), bad = [...r.bad];
      const rounds = r.deltas.flatMap((d) => d.rounds) as Round[];
      for (const x of rounds) {
        const shooter = x.by !== undefined && x.k !== undefined ? r.leavers.get(`${x.by}:${x.k}${x.j !== undefined ? `.${x.j}` : ''}`) : undefined;
        const mark = x.ok !== undefined ? r.leavers.get(`${x.on}:${x.ok}${x.oj !== undefined ? `.${x.oj}` : ''}`) : undefined;
        if (shooter && x.t >= shooter.first) bad.push(`${x.t} shot by ${x.by}:${x.k}.${x.j}`);
        if (mark && x.t >= mark.first) bad.push(`${x.t} shot at ${x.on}:${x.ok}.${x.oj}`);
      }
      expect(bad.slice(0, 10), w).toEqual([]);
    }
  });

  it('has the men of a withdrawn block get up each in his own moment and run each at his own pace', () => {
    let blocks = 0;
    for (const w of ['c', 'd'] as const) {
      const r = get(w, true), by = new Map<number, Leaver[]>();
      for (const l of r.leavers.values()) if (l.foot && r.L.sim.unit(l.b)?.state === 'withdrawn' && l.pace > 0 && l.seen - l.first >= 1.5) { const a = by.get(l.b) ?? []; a.push(l); by.set(l.b, a); }
      for (const [b, men] of by) {
        // (a block that left in the battle's last second or so has had no time to show its step-off)
        if (men.length < 8) continue;
        blocks++;
        // (his step-off: from when he runs at his own pace, whether he got up to it or was on the move already and broke
        // into it; a man on the move goes on as he was till then)
        const go = (l: Leaver): number => { let i = l.runs.length; while (i > 0 && Math.abs(l.runs[i - 1][1] - l.pace) < 0.15) i--; return l.runs[Math.min(i, l.runs.length - 1)][0]; };
        const steps = new Set(men.map((l) => Math.round((go(l) - l.first) * 10)));
        const paced = men.filter((l) => l.pace > 0);
        expect(steps.size, `${w} block ${b} step-off`).toBeGreaterThanOrEqual(4);
        expect(Math.max(...paced.map((l) => l.pace)) - Math.min(...paced.map((l) => l.pace)), `${w} block ${b} pace`).toBeGreaterThanOrEqual(0.5);
      }
    }
    expect(blocks).toBeGreaterThan(0);
  });

  it('never stops a leaving man dead who was on the move, never has one jump, and never leaves men aboard a lorry that has gone', () => {
    // (the reviews of 25 November: a withdrawn block's runners stood still for their step-off, a battery's towed gun leapt
    // to her tractor, and men aboard sat on in the air a step or two after their lorry was taken off the list)
    for (const w of ALL) {
      const r = get(w, true);
      expect(r.halted.slice(0, 10), `${w} halted`).toEqual([]);
      expect(r.jumps.slice(0, 10), `${w} jumps`).toEqual([]);
      expect(r.aboard.slice(0, 10), `${w} aboard`).toEqual([]);
    }
  });

  it('keeps the feed\'s contract with the 3D game: a living man\'s out is 0.1 s after his last kept point', () => {
    for (const w of ALL) for (const r of [get(w, true), get(w, false)]) {
      const bad = outs(r).filter((o) => Math.abs(o.out - o.at[o.at.length - 1][0] - 0.1) > 1e-6).map((o) => `${o.key} ${o.out} ${o.at[o.at.length - 1][0]}`);
      expect(bad.slice(0, 10), w).toEqual([]);
    }
  });

  it('leaves no living man going out of the feed inside the field with the switch on (and the fault is there with it off)', () => {
    for (const w of ALL) expect(inside(get(w, true)), w).toBe(0);
    expect(inside(get('c', false))).toBeGreaterThan(0);
  });
});
