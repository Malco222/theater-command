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
import type { LandUnit } from '../src/battle/land/units';
import type { LandPlace } from '../src/battle/land/field';

type Which = 'a' | 'b' | 'c' | 'd';
/** The options the game ships with (the hunt's probe): looks on but bearers, smoke, scars, mud and platoons. */
const SHIPPED = { platoons: false, bearers: false, smoke: false, scars: false, mud: false, blast: true, escort: true, mech: true, spg: true, td: true, cars: true, aa: true, light: true };
const MOVING = new Set(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
const CARRIED = new Set(['ride', 'mounted', 'horses']);
const FIRING = new Set(['fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade', 'fire']);
const STILL = new Set(['cower', 'wait', 'hide', ...FIRING]);
const NOT_FOOT = new Set(['tank', 'gun', 'lorry', 'hmg', 'mortar']);
const CREWS = new Set(['hmg', 'mortar']);

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

// The review of 26 November (the gun crews thread, its notes on leaving men beside its crews): five faults of this
// switch's own, mended. Where a fault needs a moment that is hard to bring about in a whole battle (a shell in the very
// step a block leaves, a fire order outliving its target, a team taken out of the fight while its weapons stand set up,
// a rally with men beyond the edge), the test sets the rules' state by hand between steps and drives the drill directly,
// as the review's scratch harness did; the drill's own drawing is what is tested, never the rules.
type Drilled = {
  squads: Map<number, { u: LandUnit; men: DrawnMan[]; veh: DrawnMan[] }>;
  step(): void; view(): MenBlock[]; rounds(): Round[];
  shelled(x: number, y: number, by?: number, on?: number, k?: number): void; manAt(id: number, k: number): [number, number] | null;
};
type DrawnMan = { k: number; j?: number; n: number; sec?: number; sub?: number; pr?: true; role: string; x: number; y: number; pose: string; v: number; spot: [number, number]; fell?: number; lv?: number; lvGo?: number; lvOut?: true };
/** The plains battle the review's harness used: seed 5, support teams on both sides. */
const plains = (): LandBattle => new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed: 5, style: { att: 'methodical', def: 'bold' } })));
/** One drill step on its own, the rules' clock moved on a step (the rules are not run). */
const drillStep = (b: LandBattle, d: Drilled): void => { (b as { t: number }).t = Math.round((b.t + 0.1) * 10) / 10; d.step(); };
const grpOf = (m: DrawnMan): number => (m.sec ?? m.k) * 8 + (m.sub ?? 0);
/** The weapon a loader or a gun's crewman keeps by: his section's key man, else the mate of his group who bears it. */
const weaponOf = (men: DrawnMan[], m: DrawnMan): DrawnMan | undefined => {
  const want = m.role === 'loader' ? ['mg', 'at', 'hmg', 'mortar'] : m.role === 'crew' ? ['gun'] : null;
  if (!m.pr || !want) return undefined;
  return men.find((x) => x !== m && x.j === undefined && x.k === (m.sec ?? m.k) && !m.sub && want.includes(x.role)) ?? men.find((x) => x !== m && !x.pr && grpOf(x) === grpOf(m) && want.includes(x.role));
};

describe('men who leave the field: the review of 26 November (Drill.exits, looks only)', () => {
  it('lays down a key man a shell took in the step his block left the fight, as his round says: he never gets up and walks off', { timeout: 300_000 }, () => {
    // (the rules' order in one tick: suffer() strikes the model and calls drill.shelled, move() withdraws the block,
    // then the drill steps; replayed by hand on the attackers' mortar team at 100 s)
    const b = plains(), d = b.drillOn({ sections: true, platoons: true, exits: true }) as unknown as Drilled;
    while (b.t < 100 && !b.over) b.step();
    const u = b.units.find((x) => x.side === 'att' && x.team === 'mor')!, foe = b.units.find((x) => x.side === 'def')!;
    const k = u.figures - 1, at = d.manAt(u.id, k)!;
    u.figures = k;
    d.shelled(at[0], at[1], foe.id, u.id, k);
    u.state = 'withdrawn';
    drillStep(b, d);
    expect(d.rounds().filter((r) => r.on === u.id && r.ok === k && r.r === 'kill').length).toBe(1);
    const seen: string[] = [];
    for (let i = 0; i < 30; i++) {
      const me = d.view().find((x) => x.id === u.id)?.men.find((m) => m.k === k && m.j === undefined);
      seen.push(me ? `${me.p} ${me.v} ${me.d === undefined ? 'up' : 'down'}${me.lv ? ' lv' : ''}` : 'none');
      drillStep(b, d);
    }
    expect([...new Set(seen)]).toEqual(['down 0 down']);
    // (the rest of his team still leave, on their feet)
    expect(d.view().find((x) => x.id === u.id)!.men.filter((m) => m.lv).length).toBeGreaterThan(10);
  });

  it('has no support team aim or fire at a block that has left the fight, though a fire order still names it, but at the block the rules fight', { timeout: 300_000 }, () => {
    // (a player's land:fire order outlives its target: the rules keep the team's aim on it and strike another block, the
    // one they then fight (engaged); held by hand on the block each team fights at 2 s, and that block withdrawn by hand)
    let fought = 0;
    for (const kind of ['mor', 'mg'] as const) {
      const b = plains(), d = b.drillOn({ exits: true }) as unknown as Drilled;
      const team = b.units.find((x) => x.side === 'att' && x.team === kind)!;
      let X: LandUnit | undefined, left = -1, aimed = 0, firing = 0, atLeaving = 0, onLive = 0;
      while (!b.over && (left < 0 || b.t < left + 60)) {
        if (!X && b.t >= 2 && team.engaged !== undefined) X = b.units.find((x) => x.id === team.engaged);
        if (X) team.aim = X.id;
        if (X && left < 0) { X.state = 'withdrawn'; left = b.t; }
        b.step();
        // (from the team's first think after its target left: a team thinks each half second, and a tube's last round,
        // fired while the target still fought, is drawn a moment after)
        if (left < 0 || b.t < left + 0.6) continue;
        const v = d.view(), lv = (v.find((x) => x.id === X!.id)?.men ?? []).filter((m) => m.lv);
        // (the enemy's men still in the fight, and whether the rules now fight a block still in it)
        const foes = v.filter((x) => x.id !== X!.id && b.units.find((y) => y.id === x.id)?.side !== team.side).flatMap((x) => x.men.filter((m) => !m.lv && m.d === undefined));
        const eng = team.engaged !== undefined ? b.units.find((y) => y.id === team.engaged) : undefined;
        const live = !!eng && eng !== X && eng.state !== 'gone' && eng.state !== 'withdrawn';
        let n = 0;
        for (const m of v.find((x) => x.id === team.id)?.men ?? []) {
          // (firing: a machine gun's sweep, or a tube's shot fired after its target left; a tube is drawn in its firing
          // pose a moment after a round it fired while the target still fought)
          if (m.p === 'fire' && (m.s === undefined || m.s > left)) { firing++; n++; }
          if (!m.a) continue;
          aimed++;
          const near = (q: MenOut): number => Math.hypot(q.x - m.a![0], q.y - m.a![1]);
          if (Math.min(Infinity, ...lv.map(near)) < Math.min(Infinity, ...foes.map(near))) atLeaving++;
        }
        if (live) { onLive++; if (n) fought++; }
      }
      expect(left, kind).toBeGreaterThan(0);
      // (no aim is nearer a leaving man than one still in the fight)
      expect(atLeaving, kind).toBe(0);
      // (where the rules fight a block still in the fight, the team is drawn aiming, and firing, at it)
      if (onLive > 0) expect([aimed > 0, firing > 0], kind).toEqual([true, true]);
    }
    expect(fought).toBeGreaterThan(0);
  });

  it('keeps each weapon\'s crew with it as they leave: one moment, drift and pace, each crew its own', { timeout: 300_000 }, () => {
    // (Normandy, all fall back at 60 s: the attackers' support teams withdraw at their out line)
    const L = open('c', true), d = L.drill as unknown as Drilled;
    let ordered = false, grow = 0, crews = 0;
    const gap0 = new Map<DrawnMan, number>(), paces = new Map<number, Set<string>>();
    for (let guard = 0; guard < 4000 && !L.sim.over; guard++) {
      if (!ordered && L.sim.t >= 60) { ordered = true; order('c', L); }
      L.tick(1, 50);
      for (const sq of d.squads.values()) for (const m of sq.men) {
        const w = weaponOf(sq.men, m);
        if (!w || m.lv === undefined || m.lvOut || m.fell !== undefined || w.lv === undefined || w.lvOut || w.fell !== undefined || w.role === 'gun' || m.pose === 'mounted') continue;
        const g = Math.hypot(w.x - m.x, w.y - m.y);
        if (!gap0.has(m)) { gap0.set(m, g); crews++; }
        grow = Math.max(grow, g - gap0.get(m)!);
        // (once both go, a crew man goes at his weapon's pace and gait)
        if (L.sim.t >= Math.max(m.lvGo!, w.lvGo!) + 0.2 && w.v > 0) { expect(m.v, `${sq.u.id}:${m.k}.${m.j}`).toBeCloseTo(w.v, 6); expect(m.pose).toBe(w.pose); }
        if (CREWS.has(w.role) && w.v > 0) { const s = paces.get(sq.u.id) ?? new Set<string>(); s.add(w.v.toFixed(2)); paces.set(sq.u.id, s); }
      }
    }
    expect(crews).toBeGreaterThan(20);
    // (no man of a crew ends up more than a pace or two further from his weapon than he stood as they left)
    expect(grow).toBeLessThan(3);
    // (a team's crews do not go in lockstep: each weapon at its own pace)
    expect([...paces.values()].some((s) => s.size >= 3)).toBe(true);
  });

  it('has a support team take down a weapon that stood set up and carry it off at a run (fleeing, in a rout), its crew with it', { timeout: 300_000 }, () => {
    // (the attackers' mortar and machine-gun teams taken out of the fight by hand at 100 s while set up: withdrawn, and
    // gone in a rout)
    for (const state of ['withdrawn', 'gone'] as const) for (const kind of ['mor', 'mg'] as const) {
      const b = plains(), d = b.drillOn({ sections: true, platoons: true, exits: true }) as unknown as Drilled;
      while (b.t < 100 && !b.over) b.step();
      const u = b.units.find((x) => x.side === 'att' && x.team === kind)!, sq = d.squads.get(u.id)!;
      const ws = sq.men.filter((m) => (m.role === 'hmg' || m.role === 'mortar') && m.k < u.figures);
      expect(ws.every((w) => w.v === 0), `${state} ${kind} set up`).toBe(true);
      u.state = state;
      const t0 = b.t + 0.1, went = new Map<DrawnMan, number>(), bad: string[] = [];
      let gap = 0;
      for (let i = 0; i < 300; i++) {
        drillStep(b, d);
        for (const w of ws) {
          if (w.v > 0 && !went.has(w)) went.set(w, b.t);
          if (!went.has(w) && w.pose !== 'wait') bad.push(`${b.t} ${w.k} ${w.pose} before it went`);
          if (w.v > 0 && w.pose !== (state === 'gone' ? 'flee' : 'run')) bad.push(`${b.t} ${w.k} ${w.pose} ${w.v}`);
          for (const m of sq.men) if (weaponOf(sq.men, m) === w && m.lv !== undefined && !m.lvOut && !w.lvOut) {
            gap = Math.max(gap, Math.hypot(w.x - m.x, w.y - m.y));
            if (b.t >= m.lvGo! + 0.2 && w.v > 0 && (m.v !== w.v || m.pose !== w.pose)) bad.push(`${b.t} ${w.k} crew ${m.pose} ${m.v} weapon ${w.pose} ${w.v}`);
          }
        }
      }
      expect(bad.slice(0, 5), `${state} ${kind}`).toEqual([]);
      // (taken down first where it was not a rout; snatched up within DR.exits.rise in one)
      for (const w of ws) {
        const after = went.get(w)! - t0;
        if (state === 'withdrawn') expect(after, `${kind} ${w.k}`).toBeGreaterThanOrEqual(DRILL.exits.down[0] - 1e-6);
        else expect(after, `${kind} ${w.k}`).toBeLessThanOrEqual(DRILL.exits.rise + 0.1 + 1e-6);
      }
      expect(gap, `${state} ${kind}`).toBeLessThan(12);
    }
  });

  it('has a crew man who was on the move when his block left stop by his weapon while it is taken down, and go with it', { timeout: 300_000 }, () => {
    // (the second review: desert seed 8, squad life on, the defenders' machine-gun team taken out of the fight by hand
    // at 90 s while its guns stood set up and four of its bearers crept about them; withdrawn, and gone in a rout; the
    // drill driven on its own, as in the test above)
    for (const state of ['withdrawn', 'gone'] as const) {
      const b = new LandBattle(withTeams(sampleTicket('desert', 1.2, { seed: 8, style: { att: 'bold', def: 'methodical' } })));
      const d = b.drillOn({ sections: true, platoons: true, life: true, exits: true }) as unknown as Drilled;
      while (b.t < 90 && !b.over) b.step();
      const u = b.units.find((x) => x.side === 'def' && x.team === 'mg' && x.state !== 'gone')!, sq = d.squads.get(u.id)!;
      const ws = sq.men.filter((m) => m.role === 'hmg' && m.k < u.figures && m.fell === undefined);
      expect(ws.every((w) => w.v === 0), state).toBe(true);
      const crew = sq.men.filter((m) => ws.includes(weaponOf(sq.men, m)!) && m.fell === undefined);
      // (the case: some of the crew on the move as their block leaves)
      expect(crew.filter((m) => MOVING.has(m.pose) && m.v > 0).length, state).toBeGreaterThan(0);
      u.state = state;
      if (state === 'gone') u.figures = 0;
      const gap0 = new Map<DrawnMan, number>(), bad: string[] = [];
      let grow = 0;
      for (let i = 0; i < 300; i++) {
        drillStep(b, d);
        for (const m of crew) {
          const w = weaponOf(sq.men, m)!;
          if (m.lv === undefined || m.lvOut || w.lvOut) continue;
          const g = Math.hypot(w.x - m.x, w.y - m.y);
          if (!gap0.has(m)) gap0.set(m, g);
          grow = Math.max(grow, g - gap0.get(m)!);
          // (while his weapon still stands, he stands by it)
          if (w.v === 0 && m.v > 0) bad.push(`${b.t} ${m.k}.${m.j} ${m.pose} ${m.v} by ${w.k} ${w.pose}`);
        }
      }
      expect(gap0.size, state).toBe(crew.length);
      expect(bad.slice(0, 5), state).toEqual([]);
      expect(grow, state).toBeLessThan(1);
    }
  });

  it('brings men a rout carried past the edge back onto the field on their own feet when their block rallies, a tank too', { timeout: 300_000 }, () => {
    // (sample battle (a): an attacking foot block and an armoured one made to rout by hand, then rallied as nerve()
    // rallies a block, the foot block with its middle 14 m from its edge; a tank of the armoured block, which keeps
    // behind its block's middle, set beyond the edge by hand as a rout would leave her)
    for (const kind of ['inf', 'arm'] as const) {
      const L = open('a', true), b = L.sim, d = L.drill as unknown as Drilled;
      while (b.t < 20) b.step();
      const u = b.units.find((x) => x.side === 'att' && x.kind === kind && x.state !== 'gone')!, sq = d.squads.get(u.id)!;
      const rout = (b as unknown as { rout(u: LandUnit): void }).rout.bind(b);
      while (u.y > (kind === 'inf' ? 14 : 3) && !b.over) { u.mor = 0; u.clear = 0; if (u.state !== 'routing') rout(u); b.step(); }
      Object.assign(u, { mor: u.mor0, state: 'wavering', order: null, path: [], act: 'idle', gives: false });
      if (kind === 'arm') { const tk = sq.men.find((m) => m.role === 'tank' && m.fell === undefined)!; Object.assign(tk, { y: -6, spot: [tk.x, -6], pose: 'drive', v: 4 }); }
      const all = [...sq.men, ...sq.veh].filter((m) => m.fell === undefined && m.pose !== 'mounted');
      expect(all.filter((m) => m.y < 0).length, kind).toBeGreaterThan(0);
      const prev = new Map(all.map((m) => [m, [m.x, m.y]]));
      const jumps: string[] = [];
      for (let i = 0; i < 200 && !b.over; i++) {
        b.step();
        for (const m of all) { const p = prev.get(m)!, s = Math.hypot(m.x - p[0], m.y - p[1]); if (m.fell === undefined && s > 2.5) jumps.push(`${b.t.toFixed(1)} ${m.k}.${m.j} ${m.role} ${s.toFixed(1)} m`); prev.set(m, [m.x, m.y]); }
      }
      expect(jumps.slice(0, 5), kind).toEqual([]);
      // (and they are back on the field, on their own feet)
      expect(all.filter((m) => m.fell === undefined && m.y < 0).length, kind).toBe(0);
    }
  });
});
