// The men inside each block: the squad brain (Brian, 20 November 2026, 20:43Z: "lets make this super realistic and
// meaningful and everyone has high AI iq"). A block is a division or a regiment, and the rules fight it as one; this
// file says what each of its men, tanks and guns is doing, second by second, so the 3D game can draw soldiers who
// fight as soldiers (the art director, 17 November 17:56Z: "no tray ... the men move, kneel and take cover as men").
//
// Looks only (ruling 217: the card decides, the field acts it out). Nothing in the fight reads anything here, and it
// draws nothing from the battle's own dice: every choice is a hash of the battle's seed, the block, the man and the
// moment, so a battle replays man for man, and a battle fought with the drill on ends exactly as one with it off
// (tests/battle-land-drill.test.ts). It is made only when something asks for it (LandBattle.drillOn: the 3D bridge,
// the browser page); the fits and the campaign's own runs never pay for it.
//
// Every pose is a true tell of the numbers: men go to ground when their block is losing men fast or is shelled, they
// work forward onto a block whose nerve is going, they run when it routs. What the player sees is what the rules hold.
//
// The principles (the same for every army; a style and experience only change the numbers, data/battle/drill.json):
// - Fire and movement: one team keeps the enemy's head down while the other rushes from cover to cover; the machine
//   guns are the base of fire and never rush with the assault.
// - Cover: every man looks round his place in the line for the best shelter within reach of it: fieldworks and
//   bunkers, the edge of a wood or of the houses facing the enemy, a fold of ground, a burnt-out tank, a shell hole.
// - Suppression: a block under heavy fire goes to ground; men caught in the open crawl to cover.
// - The assault: riflemen work forward onto the nearer flank of the block they are fighting and throw grenades from
//   close range; when it wavers they close on it.
// - Tanks: pairs leapfrog, one halting to cover the other; they fight from hull-down behind a crest where the ground
//   has one, back off to cover after firing from the open, and in woods and streets stay behind their infantry.
// - Anti-tank ambush: a holding block's anti-tank team waits hidden on the flank the enemy tanks come at, and fires
//   only when they are close.
// - Guns: a battery stands behind a crest or a wood's edge, fires by its style, and sends a spotter forward to the
//   friendly foot nearest its target, on the highest ground or the tallest house there.
// - Breaking: routing men run for their own edge; men falling back in order go by bounds, a rearguard covering them.

import data from '../../../data/battle/drill.json';
import { DT, type Side } from '../core/types';
import { GROUNDS, type Ground, type LandField } from './field';
import { LAND, MOTION, type Kind } from './rules';
import type { LandBattle } from './sim';
import type { LandUnit } from './units';

/** What a man, tank, gun or lorry is in his block. */
export type Role = 'officer' | 'rifle' | 'mg' | 'at' | 'sapper' | 'holder' | 'tank' | 'gun' | 'lorry' | 'fo';
/**
 * What he is doing, as the 3D game plays it. The men's are the soldier's own clips (battle-models/land/soldier/README.md)
 * and four held poses: 'wait' (kneeling, rifle up, watching: fire_kneel's aim held without a shot), 'hide' (lying
 * still in cover: fire_prone's aim held, no shot), 'observe' (a spotter lying with his glasses on the target),
 * 'mounted' and 'ride' (in a lorry, on a horse), 'horses' (a horse holder), and 'down' (fallen). The vehicles' and
 * guns': 'drive', 'halt', 'fire', 'reverse', 'towed', 'deployed', 'parked'.
 */
export type Pose =
  | 'idle' | 'walk' | 'run' | 'advance' | 'creep' | 'crawl' | 'flee' | 'fire_stand' | 'fire_kneel' | 'fire_prone' | 'lmg_burst'
  | 'throw_grenade' | 'cower' | 'wait' | 'hide' | 'observe' | 'mounted' | 'ride' | 'horses' | 'down'
  | 'drive' | 'halt' | 'fire' | 'reverse' | 'towed' | 'deployed' | 'parked';
/** What the block as a whole is doing, as its men see it. */
export type Phase = 'column' | 'deploy' | 'bound' | 'startline' | 'fight' | 'hold' | 'rest' | 'pinned' | 'fallback' | 'flee' | 'rally' | 'gone' | 'battery' | 'limbered';
/** What shelters a man where he stands. */
export type CoverKind = 'none' | 'works' | 'building' | 'woods' | 'rock' | 'rough' | 'field' | 'wreck' | 'crater' | 'crest';

/** One man, tank, gun or lorry. */
export interface Man {
  /** His place on his block's tray (model k; the rules lift model k off when the block falls to k models). Lorries 100+. */
  k: number;
  role: Role;
  /** 0 the block's headquarters, 1 and 2 its two teams. */
  team: 0 | 1 | 2;
  x: number;
  y: number;
  /** Radians, 0 north, clockwise (as a block's face). */
  face: number;
  pose: Pose;
  /** Metres a battle second he is moving now. */
  v: number;
  /** Where he is going, or the place he holds. */
  spot: [number, number];
  /** What shelters him at his spot. */
  cover: CoverKind;
  /** Where he fires, while he fires. */
  aim?: [number, number];
  /** Battle time his last shot (a tank's or a gun's round, a grenade, an anti-tank round): the 3D plays one per stamp. */
  shot?: number;
  /** Battle time he fell, and how. */
  fell?: number;
  /** An anti-tank team lying hidden, a tank closed up (fighting alone among enemy foot). */
  hidden?: boolean;
  buttoned?: boolean;
  /** Battle time his current bit (a rush, a halt, a burst) ends. */
  until: number;
  /** Where his slot in the line was when he chose his spot: he keeps the spot until his slot moves away from it. */
  slotAt: [number, number];
  /** His own fixed lean in the line, metres across and along, so no two blocks stand alike. */
  jx: number;
  jy: number;
  /** Who he is helping (a fallen comrade's k), while he is. */
  aid?: number;
  /** The fire-and-movement cycle of his last rush: one rush a cycle, however early he gets there. */
  rushed?: number;
}

/** A block's men as the 3D game reads them (LandBattle.menView). Numbers rounded; absent fields are off. */
export interface MenBlock {
  id: number;
  ph: Phase;
  men: MenOut[];
  /** The battery's spotter and his signaller, while its guns fire far (an 'art' block only). */
  fo?: MenOut[];
  /** The foot block the spotter is with, and the block his guns fire on. */
  foWith?: number;
  foOn?: number;
}
export interface MenOut {
  k: number; r: Role; x: number; y: number; f: number; p: Pose; v: number; c: CoverKind;
  a?: [number, number]; s?: number; d?: number; h?: 1; b?: 1;
}

interface Nums { rush: number; halt: number; wave: boolean; assault: number; spacing: number; tankRush: number; tankHalt: number; scoot: number; salvo: 'battery' | 'rapid' | 'ripple'; gunEvery: number }
const DR = data as unknown as {
  style: Record<'plain' | 'bold' | 'methodical' | 'mass', Nums>;
  xp: { raw: number; veteran: number; bunch: number; spread: number; seek: [number, number, number]; pinned: [number, number, number] };
  cover: { ground: Record<Ground, number>; edge: number; crest: number; wreck: number; crater: number };
  ranges: { column: number; bound: number; debus: number; fo: number; ambush: number; grenade: number; hullDown: [number, number] };
  pin: { harm: number; shelled: number };
};
export const DRILL = DR;

/** A fixed hash to [0, 1): the drill's own dice, never the battle's. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const roll = (...ns: number[]): number => { let h = 0x9e3779b9; for (const n of ns) h = Math.imul(h ^ (Math.floor(n) >>> 0), 0x01000193) + 0x7f4a7c15; return mix(h); };
const clamp = (v: number, a: number, b: number): number => (v < a ? a : v > b ? b : v);
const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
const fighting = (u: LandUnit): boolean => live(u) && u.state !== 'routing';
const FOOT_KINDS: Kind[] = ['inf', 'mtn', 'eng', 'fort', 'cav'];
const isFoot = (u: LandUnit): boolean => MOTION[u.kind] === 'foot' || u.kind === 'mot';
const CLOSE: Ground[] = ['woods', 'buildings', 'industry', 'suburb'];
const BUILT: Ground[] = ['buildings', 'industry', 'suburb'];
/** Poses that carry a man over the ground: the 3D plays them only while he moves (walk: one he reaches his place in ends). */
const MOVING: ReadonlySet<Pose> = new Set<Pose>(['walk', 'run', 'advance', 'creep', 'crawl', 'flee', 'drive', 'reverse', 'towed']);
/**
 * The fastest a man goes on foot, closing up on his block: a hard run. The blocks themselves march at up to 6 m a battle
 * second (the battle runs faster than life), so a man must outpace that to catch up; the 3D game caps its run clip's rate.
 */
const CATCH_UP = 7.5;
const COVER_OF: Partial<Record<Ground, CoverKind>> = { works: 'works', bunker: 'works', buildings: 'building', industry: 'building', suburb: 'building', woods: 'woods', rock: 'rock', rough: 'rough', field: 'field' };

/** The block's own frame: forward along its face, across to its right. */
const fwd = (f: number): [number, number] => [Math.sin(f), Math.cos(f)];
const right = (f: number): [number, number] => [Math.cos(f), -Math.sin(f)];
const toWorld = (u: { x: number; y: number }, f: number, a: number, l: number): [number, number] => {
  const [fx, fy] = fwd(f), [rx, ry] = right(f);
  return [u.x + a * rx + l * fx, u.y + a * ry + l * fy];
};
const bearing = (x0: number, y0: number, x1: number, y1: number): number => Math.atan2(x1 - x0, y1 - y0);

/** The ground under a point. */
const groundOf = (f: LandField, x: number, y: number): Ground => {
  const c = clamp(Math.floor(x / f.cell), 0, f.cols - 1), r = clamp(Math.floor(y / f.cell), 0, f.rows - 1);
  return GROUNDS[f.ground[r * f.cols + c]];
};
/** The height at a point, smoothed between the cells' centres (the cells are 25 m: a crest needs finer). */
const heightOf = (f: LandField, x: number, y: number): number => {
  const fx = clamp(x / f.cell - 0.5, 0, f.cols - 1), fy = clamp(y / f.cell - 0.5, 0, f.rows - 1);
  const c0 = Math.floor(fx), r0 = Math.floor(fy), c1 = Math.min(f.cols - 1, c0 + 1), r1 = Math.min(f.rows - 1, r0 + 1), ax = fx - c0, ay = fy - r0;
  const H = f.height, C = f.cols;
  return (H[r0 * C + c0] * (1 - ax) + H[r0 * C + c1] * ax) * (1 - ay) + (H[r1 * C + c0] * (1 - ax) + H[r1 * C + c1] * ax) * ay;
};
/** Storeys of the house at a point (its upper floors give a spotter his view; about 3 m each). */
const storeysOf = (f: LandField, x: number, y: number): number => {
  if (!f.storeys) return 0;
  const c = clamp(Math.floor(x / f.cell), 0, f.cols - 1), r = clamp(Math.floor(y / f.cell), 0, f.rows - 1);
  return f.storeys[r * f.cols + c] ?? 0;
};

interface Squad {
  u: LandUnit;
  men: Man[];
  /** Lorries (lorried infantry, a battery's tractor). */
  veh: Man[];
  fo: Man[] | null;
  foWith?: number;
  foOn?: number;
  phase: Phase;
  since: number;
  nextThink: number;
  /** Strength last step, and the share of strength it is losing a second (smoothed). */
  lastStr: number;
  harm: number;
  shelledAt: number;
  /** Where the block stood last step, and its heading over the ground. */
  lastX: number;
  lastY: number;
  heading: number;
  speed: number;
  /** Lorried infantry got down; cavalry got off their horses. */
  down: boolean;
  /** It was running last think (a rally follows). */
  ran: boolean;
  /** Battle time the battery's next salvo, and the gun that fires next in a ripple. */
  salvoAt: number;
  ripple: number;
  /** Who the block's assault is working onto, and from which flank (+1 its right, -1 its left). */
  flankOf?: number;
  flank: number;
  /** Battle time its officer next moves along the line. */
  officerAt: number;
  /** Advancing by leapfrog (slow going) rather than rolling (fast): switched with a margin, so it does not flicker. */
  leap: boolean;
}

export class Drill {
  private readonly b: LandBattle;
  private readonly squads = new Map<number, Squad>();
  private readonly seed: number;
  private settled = false;

  constructor(b: LandBattle) {
    this.b = b;
    this.seed = b.ticket.seed;
    for (const u of b.units) this.squads.set(u.id, this.make(u));
  }

  // ---- Who is in a block ------------------------------------------------------------------------------------------

  private make(u: LandUnit): Squad {
    const men: Man[] = [], veh: Man[] = [];
    const n = Math.max(0, u.figures0);
    const roles = rolesFor(u.kind, u.tray, n);
    for (let k = 0; k < n; k++) {
      const [role, team] = roles[k];
      const jx = (roll(this.seed, u.id, k, 1) - 0.5) * 6, jy = (roll(this.seed, u.id, k, 2) - 0.5) * 5;
      men.push({ k, role, team, x: u.x, y: u.y, face: u.face, pose: 'idle', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx, jy });
    }
    const lorries = u.kind === 'mot' ? (n >= 4 ? 2 : 1) : u.kind === 'art' ? 1 : 0;
    for (let i = 0; i < lorries; i++) veh.push({ k: 100 + i, role: 'lorry', team: 0, x: u.x, y: u.y, face: u.face, pose: 'parked', v: 0, spot: [u.x, u.y], cover: 'none', until: 0, slotAt: [u.x, u.y], jx: 0, jy: 0 });
    const sq: Squad = {
      u, men, veh, fo: null, phase: 'rest', since: 0, nextThink: roll(this.seed, u.id, 3) * 0.5, lastStr: u.str, harm: 0, shelledAt: -99,
      lastX: u.x, lastY: u.y, heading: u.face, speed: 0, down: u.kind !== 'mot' && u.kind !== 'cav', ran: false, salvoAt: 0, ripple: 0, flank: 0, officerAt: 0, leap: true,
    };
    // Drawn up where the battle opens: each man at his place in the line, in the best cover near it.
    this.layStart(sq);
    return sq;
  }

  private layStart(sq: Squad): void {
    const u = sq.u;
    sq.phase = this.phaseOf(sq);
    const tpl = u.kind === 'arm' || u.kind === 'art' ? 'line' : sq.phase === 'rest' ? 'rest' : 'line';
    for (const m of sq.men) {
      const [a, l] = this.slot(sq, m, tpl);
      const s = toWorld(u, u.face, a, l);
      const best = this.seek(sq, m, s, this.threatDir(sq), sq.men);
      m.x = best.x; m.y = best.y; m.spot = [best.x, best.y]; m.cover = best.kind; m.slotAt = s;
      m.pose = u.kind === 'arm' ? 'halt' : u.kind === 'art' ? 'deployed' : u.kind === 'mot' || u.kind === 'cav' ? (u.kind === 'mot' ? 'mounted' : 'ride') : 'wait';
    }
    sq.veh.forEach((v, i) => {
      const s = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 14, -u.d / 2 - 30);
      v.x = s[0]; v.y = s[1]; v.spot = s; v.slotAt = s; v.pose = 'parked';
    });
  }

  // ---- Each step ----------------------------------------------------------------------------------------------------

  step(): void {
    const b = this.b, t = b.t;
    if (b.over) { if (!this.settled) this.settle(); return; }
    for (const sq of this.squads.values()) {
      const u = sq.u;
      // The share of its strength it lost this step, smoothed over about two seconds.
      const lost = Math.max(0, sq.lastStr - u.str) / Math.max(1e-6, u.str0);
      sq.harm = sq.harm * 0.95 + (lost / DT) * 0.05;
      sq.lastStr = u.str;
      const moved = Math.hypot(u.x - sq.lastX, u.y - sq.lastY);
      sq.speed = sq.speed * 0.8 + (moved / DT) * 0.2;
      if (moved > 0.05) sq.heading = bearing(sq.lastX, sq.lastY, u.x, u.y);
      sq.lastX = u.x; sq.lastY = u.y;
      this.falls(sq);
      if (t >= sq.nextThink) { sq.nextThink = t + 0.5; this.think(sq); }
      this.walk(sq);
    }
  }

  /**
   * The battle is over: every man stops where he is. The winners stand easy, the beaten keep their heads down, and those
   * who ran crouch where they got to; tanks halt, guns stand, lorries park.
   */
  private settle(): void {
    this.settled = true;
    const won = this.b.over?.winner;
    for (const sq of this.squads.values()) {
      const u = sq.u;
      for (const m of [...sq.men, ...sq.veh, ...(sq.fo ?? [])]) {
        if (m.fell !== undefined) continue;
        m.v = 0; m.spot = [m.x, m.y]; m.aim = undefined; m.hidden = undefined;
        m.pose = m.role === 'tank' ? 'halt' : m.role === 'gun' ? 'deployed' : m.role === 'lorry' ? 'parked' : m.role === 'holder' ? 'horses'
          : sq.phase === 'flee' ? 'cower' : u.side === won ? (m.k % 3 ? 'idle' : 'wait') : 'wait';
      }
    }
  }

  /** A shell burst at a point (sim.ts shell): men of every block near it duck. */
  shelled(x: number, y: number): void {
    for (const sq of this.squads.values()) if (Math.hypot(sq.u.x - x, sq.u.y - y) < Math.max(sq.u.w, sq.u.d) / 2 + 40) sq.shelledAt = this.b.t;
  }

  /** Where model k of block id stands now (alive, or the moment he falls): the shell that cost him lands there. */
  manAt(id: number, k: number): [number, number] | null {
    const sq = this.squads.get(id);
    const m = sq?.men.find((x) => x.k === k);
    return m ? [m.x, m.y] : null;
  }

  /** Men the rules have lifted off the tray fall where they stand; a tank where her wreck lies. */
  private falls(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    for (const m of sq.men) {
      if (m.fell !== undefined || m.k < u.figures) continue;
      if (u.state === 'withdrawn' || (u.state === 'gone' && u.figures > 0)) continue;
      m.fell = t; m.v = 0; m.aim = undefined; m.hidden = undefined;
      if (m.role === 'tank') {
        // The rules lay a block's wrecks in the order its tanks fall (the lower place first within a tick), as here.
        const nth = sq.men.filter((x) => x.role === 'tank' && x.fell !== undefined && x !== m).length;
        const w = this.b.wrecks.filter((z) => z.unit === u.id)[nth];
        if (w) { m.x = w.x; m.y = w.y; }
      }
      m.pose = 'down';
      m.spot = [m.x, m.y];
      // A comrade goes to him, where one is near and the block is not running.
      if (fighting(u) && m.role !== 'tank' && m.role !== 'gun') {
        const helper = sq.men.filter((h) => h.fell === undefined && h.k < u.figures && h.role === 'rifle' && h.aid === undefined && Math.hypot(h.x - m.x, h.y - m.y) < 30)
          .sort((a, c) => Math.hypot(a.x - m.x, a.y - m.y) - Math.hypot(c.x - m.x, c.y - m.y))[0];
        if (helper && roll(this.seed, u.id, m.k, t * 10) < 0.6) {
          helper.aid = m.k; helper.spot = [m.x + 1.2, m.y - 0.8]; helper.until = t + 6; helper.aim = undefined;
          helper.pose = sq.harm > DR.pin.harm ? 'crawl' : 'creep'; helper.v = helper.pose === 'crawl' ? 1.2 : 2.5;
        }
      }
    }
  }

  // ---- What the block is doing ------------------------------------------------------------------------------------

  private phaseOf(sq: Squad): Phase {
    const b = this.b, u = sq.u;
    if (!live(u)) return 'gone';
    if (u.state === 'routing') return 'flee';
    if (u.order?.kind === 'withdraw' || u.gives || u.fellBack) return 'fallback';
    if (sq.ran && u.state === 'wavering') return 'rally';
    const moving = sq.speed > 0.6 && !!u.order;
    const near = this.nearestGap(u);
    if (u.kind === 'art') return moving ? 'limbered' : 'battery';
    const hot = sq.harm > DR.pin.harm * this.pinScale(u) || b.t - sq.shelledAt < DR.pin.shelled;
    if (u.state === 'wavering' && (u.engaged !== undefined || hot)) return 'pinned';
    if (hot && !moving && u.kind !== 'arm') return 'pinned';
    if (u.engaged !== undefined) return moving ? 'bound' : 'fight';
    if (moving) {
      if (near < DR.ranges.bound || sq.harm > 0.0005) return 'bound';
      return near < DR.ranges.column ? 'deploy' : 'column';
    }
    // The main effort lying at its start line under its guns (ai.ts: the plan's 'guns' phase).
    const P = b.plan[u.side];
    if (P && P.phase === 'guns' && P.role[u.id] === 'main' && near < DR.ranges.column) return 'startline';
    if (u.reserve === 'held' && near > 250) return 'rest';
    return near < DR.ranges.column || u.side === 'def' ? 'hold' : 'rest';
  }

  /** How readily its men go to ground: raw troops sooner, veterans later. */
  private pinScale(u: LandUnit): number { return 1 / DR.xp.pinned[this.band(u)]; }
  private band(u: LandUnit): 0 | 1 | 2 { return u.xp < DR.xp.raw ? 0 : u.xp >= DR.xp.veteran ? 2 : 1; }
  private nums(u: LandUnit): Nums {
    const s = this.b.style[u.side];
    return DR.style[s && this.b.ai[u.side] ? s.id : 'plain'];
  }

  /** Metres from this block's edge to the nearest enemy that is still fighting. */
  private nearestGap(u: LandUnit): number {
    let g = Infinity;
    for (const e of this.b.units) if (e.side !== u.side && fighting(e)) g = Math.min(g, this.b.gap(u, e));
    return g;
  }

  /** What the block faces: who it fights, else its order's target, else the nearest enemy, else nothing. */
  private threat(u: LandUnit): LandUnit | null {
    const b = this.b;
    if (u.engaged !== undefined) { const e = b.unit(u.engaged); if (e && live(e)) return e; }
    if (u.order?.target !== undefined) { const e = b.unit(u.order.target); if (e && live(e)) return e; }
    let best: LandUnit | null = null, d = 1500;
    for (const e of b.units) if (e.side !== u.side && fighting(e)) { const g = b.gap(u, e); if (g < d) { d = g; best = e; } }
    return best;
  }
  private threatDir(sq: Squad): [number, number] {
    const u = sq.u, e = this.threat(u);
    if (!e) return fwd(u.face);
    const d = Math.hypot(e.x - u.x, e.y - u.y) || 1;
    return [(e.x - u.x) / d, (e.y - u.y) / d];
  }

  // ---- Thinking: each block twice a second --------------------------------------------------------------------------

  private think(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    const was = sq.phase;
    let ph = this.phaseOf(sq);
    // Lorried infantry and cavalry get down once the enemy is near or they are hit; they stay down.
    if (!sq.down && (ph === 'fight' || ph === 'pinned' || ph === 'hold' || ph === 'startline' || this.nearestGap(u) < DR.ranges.debus || sq.harm > 0)) {
      if (u.kind === 'mot' || (u.kind === 'cav' && this.nearestGap(u) < DR.ranges.debus * 0.85)) sq.down = true;
      if (u.kind === 'mot' && !sq.down) sq.down = true;
    }
    if (ph !== was) { sq.phase = ph; sq.since = t; }
    ph = sq.phase;
    sq.ran = u.state === 'routing' || (sq.ran && u.state === 'wavering' && t - sq.since < 8);
    if (ph === 'gone') return;
    if (u.kind === 'arm') { this.tanks(sq); return; }
    if (u.kind === 'art') { this.guns(sq); return; }
    this.lorries(sq);
    if ((u.kind === 'mot' || u.kind === 'cav') && !sq.down) { this.mounted(sq); return; }
    switch (ph) {
      case 'column': this.column(sq); break;
      case 'deploy': this.advance(sq, false); break;
      case 'bound': this.advance(sq, true); break;
      case 'startline': this.lie(sq, 'wait'); break;
      case 'fight': this.fight(sq); break;
      case 'hold': this.hold(sq); break;
      case 'rest': this.rest(sq); break;
      case 'pinned': this.pinned(sq); break;
      case 'fallback': this.fallback(sq); break;
      case 'flee': this.flee(sq); break;
      case 'rally': this.rally(sq); break;
      default: break;
    }
    this.ambush(sq);
    this.officer(sq);
    for (const m of sq.men) if (m.fell === undefined && MOVING.has(m.pose) && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 0.3) this.arrive(sq, m);
  }

  /**
   * He has reached his place: off his feet into the pose for it (no man runs on the spot). A rifleman with a target
   * fires from it, without one he kneels watching; a routing man crouches where he got to; a tank halts, a gun towed
   * into the line unhooks, a lorry parks.
   */
  private arrive(sq: Squad, m: Man): void {
    m.v = 0;
    switch (m.role) {
      case 'tank': m.pose = 'halt'; return;
      case 'lorry': m.pose = 'parked'; return;
      case 'gun': if (m.pose === 'towed' && sq.phase === 'battery') m.pose = 'deployed'; return;
      default: {
        m.until = Math.max(m.until, this.b.t + 1.5);
        if (m.pose === 'flee') { m.pose = 'cower'; return; }
        if (m.aid !== undefined) { m.pose = 'wait'; m.aim = undefined; return; } // kneeling over his fallen mate
        if (!m.aim && m.role !== 'officer' && (sq.phase === 'fight' || sq.phase === 'pinned')) m.aim = this.aimAt(m, this.threat(sq.u));
        if (m.aim && m.role !== 'officer') m.pose = this.firePose(m);
        else m.pose = m.role === 'mg' && sq.phase !== 'fight' ? 'hide' : 'wait';
      }
    }
  }

  /** The men still standing, not helping a fallen comrade. */
  private up(sq: Squad): Man[] { return sq.men.filter((m) => m.fell === undefined && m.k < sq.u.figures && !this.aiding(m)); }
  private aiding(m: Man): boolean {
    if (m.aid === undefined) return false;
    if (this.b.t >= m.until) { m.aid = undefined; return false; }
    return true;
  }

  /**
   * His place in the block's line, metres across (right of the middle) and along (forward of the middle). 'line': the
   * two teams side by side across the front, each about its half's middle at the style's spacing, the machine gun at
   * its team's outer end, the officer behind the middle, the anti-tank team beside him. 'column': two files up the
   * middle of the block. 'rest': three knots, the teams and the headquarters.
   */
  private slot(sq: Squad, m: Man, tpl: 'line' | 'column' | 'rest'): [number, number] {
    const u = sq.u, N = this.nums(u), band = this.band(u);
    const sp = N.spacing * (band === 0 ? DR.xp.bunch : band === 2 ? DR.xp.spread : 1);
    const all = sq.men.filter((x) => x.fell === undefined && x.k < u.figures);
    if (tpl === 'column') {
      const i = all.indexOf(m);
      return [(i % 2 ? 1 : -1) * 2.5 + m.jx * 0.2, u.d / 2 - 4 - Math.floor(i / 2) * 5];
    }
    if (u.kind === 'arm' || u.kind === 'art') {
      const i = all.indexOf(m), n = Math.max(1, all.length), gap = Math.min(u.w / n, u.kind === 'art' ? 32 : 40);
      return [(i - (n - 1) / 2) * gap + m.jx, (u.kind === 'arm' ? (i % 2 ? -6 : 6) : 0) + m.jy];
    }
    if (tpl === 'rest') {
      const c: [number, number] = m.team === 0 ? [0, -u.d / 4] : [(m.team === 1 ? -1 : 1) * u.w / 5, 0];
      const mates = all.filter((x) => x.team === m.team), i = mates.indexOf(m), ang = (i / Math.max(1, mates.length)) * Math.PI * 2 + m.jx;
      return [c[0] + Math.cos(ang) * 4.5, c[1] + Math.sin(ang) * 3.5];
    }
    if (m.team === 0) {
      if (m.role === 'officer') return [m.jx, -u.d / 2 + 6];
      return [u.w / 8 + m.jx, -u.d / 2 + 10];
    }
    const side = m.team === 1 ? -1 : 1;
    const mates = all.filter((x) => x.team === m.team).sort((a, c) => (a.role === 'mg' ? -1 : c.role === 'mg' ? 1 : a.k - c.k));
    const i = mates.indexOf(m), n = mates.length;
    const half = u.w / 2 - 6, span = Math.min(half - 4, sp * Math.max(0, n - 1));
    // The machine gun at the outer end; the riflemen inward from it.
    const outer = side * Math.min(half, u.w / 4 + span / 2);
    const a = outer - side * (n > 1 ? (span * i) / (n - 1) : 0);
    const l = u.d / 2 - 4 + (i % 2 ? -3 : 0) + m.jy * 0.5;
    return [a + m.jx * 0.4, l];
  }

  /**
   * The best place near p for a man to stand: the shelter of the ground and what lies on it, a little for being near his
   * place in the line, never on top of a mate. 'dir' points at the enemy. Within his experience's reach of p.
   */
  private seek(sq: Squad, m: Man, p: [number, number], dir: [number, number], mates: Man[], reach?: number): { x: number; y: number; kind: CoverKind; v: number } {
    const f = this.b.field, u = sq.u;
    const R = reach ?? DR.xp.seek[this.band(u)] * (u.kind === 'arm' || u.kind === 'art' ? 2 : 1);
    const going = m.role === 'tank' ? 'tracks' : m.role === 'gun' ? 'guns' : m.role === 'lorry' ? 'wheels' : 'foot';
    const pace = LAND.pace[going];
    const sep = m.role === 'tank' ? 16 : m.role === 'gun' ? 18 : m.role === 'lorry' ? 10 : 3.5;
    const cand: [number, number][] = [p];
    const a0 = roll(this.seed, u.id, m.k, 7) * Math.PI * 2;
    // Settling into a line (not a rush): he looks further, to the wood's edge or the houses behind the line or beside it,
    // as a section leader places his men in the best cover within shouting distance of their places.
    const settle = reach === undefined && going === 'foot' && (sq.phase === 'fight' || sq.phase === 'hold' || sq.phase === 'startline' || sq.phase === 'pinned' || sq.phase === 'rest');
    const rings = settle ? [R * 0.5, R, R * 2, R * 3] : [R * 0.5, R];
    rings.forEach((r, j) => { for (let i = 0; i < 8; i++) { const a = a0 + (i * Math.PI) / 4 + (j % 2 ? Math.PI / 8 : 0); cand.push([p[0] + Math.sin(a) * r, p[1] + Math.cos(a) * r]); } });
    // A burnt-out tank (sheltering behind it, from the enemy) and a shell hole near him.
    if (going === 'foot') {
      for (const z of this.b.wrecks) if (Math.abs(z.x - p[0]) < R + 8 && Math.abs(z.y - p[1]) < R + 8) cand.push([z.x - dir[0] * 4, z.y - dir[1] * 4]);
      for (const z of this.b.craters) if (Math.abs(z.x - p[0]) < R && Math.abs(z.y - p[1]) < R) cand.push([z.x, z.y]);
    }
    let best = { x: p[0], y: p[1], kind: 'none' as CoverKind, v: -Infinity };
    for (const [cx0, cy0] of cand) {
      const cx = clamp(cx0, 3, f.w - 3), cy = clamp(cy0, 3, f.h - 3);
      const g = groundOf(f, cx, cy);
      if (pace[g] <= (going === 'foot' ? 0 : 0.12)) continue;
      const c = this.shelter(cx, cy, g, dir, going === 'foot', m.role === 'tank');
      let s = c.v - (settle ? 0.008 : 0.015) * Math.hypot(cx - p[0], cy - p[1]);
      for (const o of mates) if (o !== m && o.fell === undefined && Math.hypot(o.spot[0] - cx, o.spot[1] - cy) < sep) s -= 0.3;
      if (s > best.v) best = { x: cx, y: cy, kind: c.kind, v: s };
    }
    if (best.v === -Infinity) best = { x: clamp(p[0], 3, f.w - 3), y: clamp(p[1], 3, f.h - 3), kind: 'none', v: 0 };
    return best;
  }

  /** How much shelter a point gives a man (or a tank: a crest is her hull-down) facing 'dir'. */
  private shelter(x: number, y: number, g: Ground, dir: [number, number], foot: boolean, tank: boolean): { v: number; kind: CoverKind } {
    const f = this.b.field, C = DR.cover;
    let v = tank ? (g === 'woods' || BUILT.includes(g) ? 0.4 : 0) : C.ground[g];
    let kind: CoverKind = COVER_OF[g] ?? 'none';
    if (v < -1) return { v, kind: 'none' };
    // The edge facing the enemy: a man lines the wood's or the houses' edge, where he can see and fire.
    if (v >= 0.3) { const ahead = groundOf(f, x + dir[0] * 15, y + dir[1] * 15); if (C.ground[ahead] < v - 0.2) v += C.edge; }
    // A fold of ground in front: hull-down for a tank, a bank to lie behind for a man.
    const rise = heightOf(f, x + dir[0] * 15, y + dir[1] * 15) - heightOf(f, x, y);
    const [lo, hi] = DR.ranges.hullDown;
    if (rise >= (tank ? lo : 0.8) && rise <= (tank ? hi : 4)) { const c = tank ? 0.8 : C.crest; if (c > v - 0.05) { v = Math.max(v, c) + 0.05; kind = 'crest'; } }
    if (foot) {
      for (const z of this.b.wrecks) {
        const dx = z.x - x, dy = z.y - y, d = Math.hypot(dx, dy);
        if (d > 2 && d < 8 && (dx * dir[0] + dy * dir[1]) / d > 0.5 && C.wreck > v) { v = C.wreck; kind = 'wreck'; }
      }
      if (v < C.crater) for (const z of this.b.craters) if (Math.hypot(z.x - x, z.y - y) <= Math.max(1.5, z.r)) { v = C.crater; kind = 'crater'; break; }
    }
    return { v, kind };
  }

  /**
   * A point pulled inside the block's leash: its box, as the rules hold it, and a margin round it (more for men running).
   * The block is the truth: no man strays from it further than this.
   */
  private leash(sq: Squad, p: [number, number], extra = 0): [number, number] {
    const u = sq.u, m = (sq.phase === 'flee' ? 70 : 45) + extra, [rx, ry] = right(u.face), [fx, fy] = fwd(u.face);
    const dx = p[0] - u.x, dy = p[1] - u.y, a0 = dx * rx + dy * ry, l0 = dx * fx + dy * fy;
    const a = clamp(a0, -u.w / 2 - m, u.w / 2 + m), l = clamp(l0, -u.d / 2 - m, u.d / 2 + m);
    return a === a0 && l === l0 ? p : [u.x + a * rx + l * fx, u.y + a * ry + l * fy];
  }

  /**
   * Keeps a man (a tank, a lorry) at a place that moves with his block: he heads a little ahead of it, so he keeps its
   * pace and never stops short; one left behind closes up faster. When the block halts he reaches it and settles.
   */
  private follow(sq: Squad, m: Man, s: [number, number], pose: Pose, mul = 1): void {
    const v = sq.speed, on = v > 0.4, [hx, hy] = fwd(sq.heading);
    const off = Math.hypot(s[0] - m.x, s[1] - m.y), behind = (s[0] - m.x) * hx + (s[1] - m.y) * hy;
    // He heads for a point a second and a half ahead of his place (or of himself, where he is ahead of it), so he never
    // runs onto it while the block moves; behind his place he closes up, ahead of it he eases off.
    const lead = on ? v * 1.5 + clamp(-behind, 0, 10) : 0;
    m.spot = [s[0] + hx * lead, s[1] + hy * lead]; m.slotAt = s; m.cover = 'none';
    m.v = on ? v * (behind > 6 ? 1.4 * mul : behind > 2 ? 1.1 : behind < -2 ? 0.85 : 1) : off > 1 ? 2.5 : 0;
    // The gait for the pace: no man creeps or strolls at a run.
    m.pose = pose === 'walk' && m.v > 2.5 ? 'run' : pose === 'creep' && m.v > 2.8 ? 'advance' : pose === 'crawl' && m.v > 1.6 ? 'creep' : pose;
    // A man runs no faster than a man can (a lorry or a tank as fast as hers): the block's box carries him beyond that.
    m.v = Math.min(m.v, m.role === 'tank' || m.role === 'lorry' || m.role === 'gun' ? 16 : CATCH_UP);
    if (m.v === 0) { m.spot = [m.x, m.y]; this.arrive(sq, m); }
  }

  /** Sends a man to a place near p (the best shelter there unless 'open'), at a pace, in a pose. */
  private go(sq: Squad, m: Man, p: [number, number], pose: Pose, pace: number, until: number, dir?: [number, number], reach?: number): void {
    p = this.leash(sq, p, -4);
    const best = this.seek(sq, m, p, dir ?? this.threatDir(sq), sq.men, reach);
    m.spot = [best.x, best.y]; m.cover = best.kind; m.slotAt = p;
    m.pose = pose; m.v = pace; m.until = until;
  }

  /** The firing pose for where he lies: standing in fieldworks or at a window, kneeling in woods or behind rocks or a crest, lying in the open. */
  private firePose(m: Man): Pose {
    if (m.role === 'mg') return 'lmg_burst';
    switch (m.cover) {
      case 'works': case 'building': return 'fire_stand';
      case 'woods': case 'rock': case 'wreck': case 'rough': return 'fire_kneel';
      default: return 'fire_prone';
    }
  }

  /** The nearest standing man of the block it fights, else a point in that block: where he fires. */
  private aimAt(m: Man, e: LandUnit | null): [number, number] | undefined {
    if (!e) return undefined;
    const sq = this.squads.get(e.id);
    let best: [number, number] | undefined, d = Infinity;
    if (sq) for (const o of sq.men) { if (o.fell !== undefined || o.k >= e.figures) continue; const dd = Math.hypot(o.x - m.x, o.y - m.y); if (dd < d) { d = dd; best = [o.x, o.y]; } }
    return best ?? [e.x, e.y];
  }

  // ---- Foot ---------------------------------------------------------------------------------------------------------

  private column(sq: Squad): void {
    const u = sq.u, t = this.b.t, h = sq.heading;
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'column');
      const s = toWorld(u, h, a, l);
      m.aim = undefined; m.hidden = undefined; m.until = t + 1;
      this.follow(sq, m, s, u.kind === 'cav' && !sq.down ? 'ride' : 'walk');
    }
  }

  /**
   * On the march toward the enemy, in open order across the block's front. Within striking range or under fire it goes
   * by fire and movement: where the block itself goes slowly, the two teams leapfrog, one rushing to the next cover while
   * the other lies firing; where it goes fast, all advance at the crouch and a rolling few drop to fire a moment, then
   * sprint to catch up. A mass goes in waves, both teams together.
   */
  private advance(sq: Squad, under: boolean): void {
    const u = sq.u, t = this.b.t, N = this.nums(u), h = sq.heading, v = Math.max(0.5, sq.speed);
    const e = this.threat(u), dir = this.threatDir(sq);
    const [hx, hy] = fwd(h);
    const close = e ? this.b.gap(u, e) < DR.ranges.bound : false; // within rifle and machine-gun range
    // Leapfrog only where a man can run the block's whole cycle of ground in his rush; faster, the rolling advance.
    const vLeap = 4.7 * N.rush / (N.rush + N.halt);
    sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (!under) {
        m.aim = undefined; m.until = t + 1;
        this.follow(sq, m, s, close ? 'advance' : 'walk');
        continue;
      }
      if (m.role === 'officer' && sq.leap) {
        // The officer goes with whichever team is moving, crouched, a few paces behind it.
        this.follow(sq, m, s, 'creep'); m.aim = undefined;
        continue;
      }
      if (sq.leap) {
        // Leapfrog: team 1 and team 2 on alternate rushes (a mass: together).
        const cyc = N.rush + N.halt, off = N.wave ? 0 : m.team === 2 ? cyc / 2 : 0;
        const at = t + off + m.jx * 0.08 + 100 * cyc, n = Math.floor(at / cyc);
        const rushing = at - n * cyc < N.rush;
        const behind = ((s[0] - m.x) * hx + (s[1] - m.y) * hy) > 30;
        if (rushing && m.rushed !== n && m.role !== 'officer' || behind && m.pose !== 'advance') {
          // Up and forward to the next cover, once a cycle (and whenever he has fallen well behind his place).
          // To where his place will be halfway through the halt that follows: on average he keeps his place.
          const lead = v * (N.rush + N.halt / 2);
          this.go(sq, m, [s[0] + hx * lead, s[1] + hy * lead], 'advance', 3, t + N.rush, dir, 6);
          // Fast enough to be down again before his rush is over: a crouched run, a sprint where he has far to go.
          m.v = clamp(Math.hypot(m.spot[0] - m.x, m.spot[1] - m.y) / (N.rush * 0.85), 2, behind ? CATCH_UP : 5.5);
          m.rushed = n; m.aim = undefined;
        } else if (m.pose === 'advance' || m.pose === 'creep') {
          if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5 && t < m.until + 1) continue;
          // Down where he got to, firing if anything is in reach of his rifle (the officer watching, directing).
          m.v = 0; m.spot = [m.x, m.y]; m.cover = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false).kind;
          m.aim = close && m.role !== 'officer' ? this.aimAt(m, e) : undefined;
          m.pose = m.aim ? this.firePose(m) : 'wait'; m.until = t + 1.5;
        } else if (t >= m.until && close && m.role !== 'officer') { m.aim = this.aimAt(m, e); m.pose = this.firePose(m); m.until = t + 1.5; }
      } else {
        // Rolling: a few at a time drop to fire, then sprint on.
        const share = 0.3, cyc = 6;
        const ph = (t + roll(this.seed, u.id, m.k, 11) * cyc) % cyc;
        const behind = ((s[0] - m.x) * hx + (s[1] - m.y) * hy) > 12;
        if (ph < cyc * share && m.role !== 'officer' && !behind) {
          if (m.pose !== 'fire_prone' && m.pose !== 'lmg_burst' && m.pose !== 'fire_kneel') { m.spot = [m.x, m.y]; m.cover = 'none'; }
          m.v = 0; m.pose = close ? (m.role === 'mg' ? 'lmg_burst' : 'fire_prone') : 'wait'; m.aim = close ? this.aimAt(m, e) : undefined;
        } else {
          m.aim = undefined;
          this.follow(sq, m, s, 'advance', 1.1);
        }
        m.until = t + 0.5;
      }
    }
  }

  /** Lying where they are, in the best cover near their places, in one pose (the start line: waiting for the guns to lift). */
  private lie(sq: Squad, pose: Pose): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || t >= m.until) this.go(sq, m, s, pose, 3, t + 30, dir);
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.pose = m.role === 'mg' ? 'hide' : pose; m.v = 0; }
      else { m.pose = 'creep'; m.v = Math.max(m.v, 2.5); }
    }
  }

  /**
   * In a fight. The base of fire (the machine guns and the riflemen not in the assault) lines the block's front in the
   * best cover there, firing, and shifts its place now and then; the assault works forward from cover to cover onto
   * the nearer flank of the block it fights, throws grenades from close range and closes on it as it wavers. A defence
   * on its own ground keeps everyone in the line and fires from its works.
   */
  private fight(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq);
    const home = u.side === 'def' && u.reserve !== 'sent' && Math.hypot(u.x - u.home[0], u.y - u.home[1]) < LAND.hold.within;
    const men = this.up(sq);
    const rifles = men.filter((m) => m.role === 'rifle' || m.role === 'sapper').sort((a, c) => (a.team === c.team ? a.k - c.k : c.team - a.team));
    const nA = home ? 0 : Math.round(rifles.length * N.assault * (e && e.state === 'wavering' ? 1.2 : 1));
    const assault = new Set(rifles.slice(0, Math.min(rifles.length, nA)));
    // The flank the assault works onto: the side of its target no friend of ours is already on.
    if (e && (sq.flankOf !== e.id || !sq.flank)) {
      sq.flankOf = e.id;
      const [rx, ry] = right(u.face);
      const friends = b.units.filter((f) => f.side === u.side && f !== u && fighting(f) && f.engaged === e.id);
      const lean = friends.reduce((s, f) => s + Math.sign((f.x - u.x) * rx + (f.y - u.y) * ry), 0);
      sq.flank = lean > 0 ? -1 : lean < 0 ? 1 : roll(this.seed, u.id, e.id) < 0.5 ? -1 : 1;
    }
    const gap = e ? b.gap(u, e) : 999, fresh = sq.since === t;
    for (const m of men) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (assault.has(m) && e) {
        // Rush, then fire, then rush again: each bound 6 to 14 m nearer, onto the flank, never closer than a grenade's throw.
        if (t >= m.until) {
          const rushNow = m.pose !== 'advance' && m.pose !== 'crawl' && m.pose !== 'creep';
          if (rushNow) {
            const [rx, ry] = right(u.face), [fx, fy] = fwd(u.face);
            const wavering = e.state !== 'steady';
            const toE = Math.max(0, gap - (wavering ? 5 : DR.ranges.grenade * 0.7));
            const reach = Math.min(u.d / 2 + Math.min(toE, 38), Math.max(0, (m.slotAt[0] - u.x) * fx + (m.slotAt[1] - u.y) * fy) + 6 + 8 * roll(this.seed, u.id, m.k, t));
            const across = clamp(a + sq.flank * (6 + 6 * roll(this.seed, u.id, m.k, t, 3)), -u.w / 2 - 20, u.w / 2 + 20);
            const p: [number, number] = [u.x + across * rx + reach * fx, u.y + across * ry + reach * fy];
            const pose: Pose = sq.harm > DR.pin.harm * 0.6 ? 'crawl' : wavering ? 'advance' : roll(this.seed, u.id, m.k, t, 5) < 0.3 ? 'creep' : 'advance';
            const pace = pose === 'crawl' ? 1.2 : pose === 'creep' ? 2.5 : 5;
            this.go(sq, m, p, pose, pace, t + N.rush + 1, dir, 8);
            m.aim = undefined;
          } else {
            // Down in cover: fire, or a grenade if their men are close.
            const aim = this.aimAt(m, e), near = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
            if (near < DR.ranges.grenade && roll(this.seed, u.id, m.k, t, 9) < 0.45) { m.pose = 'throw_grenade'; m.shot = t; m.aim = aim; m.until = t + 1.6; }
            else { m.pose = this.firePose(m); m.aim = aim; m.until = t + N.halt + 1.5 * roll(this.seed, u.id, m.k, t, 4); }
            m.v = 0; m.spot = [m.x, m.y];
          }
        } else if ((m.pose === 'advance' || m.pose === 'creep' || m.pose === 'crawl') && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) {
          m.pose = this.firePose(m); m.v = 0; m.aim = this.aimAt(m, e); m.until = Math.max(m.until, t + N.halt);
        }
        continue;
      }
      // The base of fire: its place at the front, in cover (sought afresh as the block halts to fight); a fresh place now and
      // then (a gun that has fired long moves).
      const moved = fresh || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 6;
      if (moved || t >= m.until + (m.role === 'mg' ? 20 : 30) * (0.6 + 0.8 * roll(this.seed, u.id, m.k, Math.floor(t / 20)))) {
        this.go(sq, m, s, sq.harm > DR.pin.harm * 0.6 ? 'crawl' : 'creep', sq.harm > DR.pin.harm * 0.6 ? 1.2 : 2.5, t + 1, dir);
        m.aim = undefined;
        continue;
      }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0;
      if (t < m.until && m.pose !== 'creep' && m.pose !== 'crawl') continue;
      const aim = this.aimAt(m, e), near = aim ? Math.hypot(aim[0] - m.x, aim[1] - m.y) : 999;
      // Heads down a moment when the fire on them is heavy; a grenade when theirs are close; else fire, in bursts.
      const r = roll(this.seed, u.id, m.k, t, 6);
      if (sq.harm > DR.pin.harm * 0.5 && m.role !== 'mg' && r < 0.25) { m.pose = 'cower'; m.aim = undefined; m.until = t + 1.5 + r * 4; }
      else if (near < DR.ranges.grenade && m.role === 'rifle' && r < 0.4) { m.pose = 'throw_grenade'; m.shot = t; m.aim = aim; m.until = t + 1.6; }
      else if (r > 0.88 && m.role !== 'mg') { m.pose = 'wait'; m.aim = undefined; m.until = t + 1.2; }
      else { m.pose = this.firePose(m); m.aim = aim; m.until = t + 3 + 3 * r; }
      // Home in its works it holds the line: never out of the works to chase.
      if (home && m.cover === 'none' && u.dug) m.until = Math.min(m.until, t + 1);
    }
  }

  /** Holding its ground, watching: in the best cover along its front, a sentry up at each end, the rest ready. */
  private hold(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq), near = this.nearestGap(u);
    for (const m of this.up(sq)) {
      if (m.role === 'officer') continue; // he walks his line (officer)
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || (m.v > 0 && t >= m.until)) { this.go(sq, m, s, 'creep', 2.5, t + 1, dir); continue; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.aim = undefined;
      if (m.role === 'at' && m.hidden) continue;
      m.pose = m.role === 'mg' ? 'hide' : near < 600 ? 'wait' : roll(this.seed, u.id, m.k, Math.floor(t / 8)) < 0.3 ? 'idle' : 'wait';
    }
  }

  /** A reserve resting out of the fight: in knots, in cover, ready; the headquarters at the middle. */
  private rest(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'rest');
      const s = toWorld(u, u.face, a, l);
      if (sq.since === t || Math.hypot(s[0] - m.slotAt[0], s[1] - m.slotAt[1]) > 5 || (m.v > 0 && t >= m.until)) { this.go(sq, m, s, 'walk', 2, t + 1, dir); continue; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1) continue;
      m.v = 0; m.aim = undefined;
      m.pose = (m.k % 5 === 1) ? 'wait' : roll(this.seed, u.id, m.k, Math.floor(t / 10)) < 0.5 ? 'idle' : 'wait';
    }
  }

  /**
   * Pinned: losing men fast, shelled, or wavering under fire. Down in cover; those caught in the open crawl to the nearest
   * shelter; about a third keep their heads down, the rest fire back, the machine guns hardest.
   */
  private pinned(sq: Squad): void {
    const u = sq.u, t = this.b.t, e = this.threat(u), dir = this.threatDir(sq);
    for (const m of this.up(sq)) {
      if (m.v > 0 && Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1 && m.pose === 'crawl') continue;
      const here = this.shelter(m.x, m.y, groundOf(this.b.field, m.x, m.y), dir, true, false);
      if (here.v < 0.2 && m.role !== 'officer') {
        const best = this.seek(sq, m, [m.x, m.y], dir, sq.men, 14);
        if (best.v > here.v + 0.2) { m.spot = [best.x, best.y]; m.cover = best.kind; m.pose = 'crawl'; m.v = 1.2; m.until = t + 6; m.aim = undefined; continue; }
      }
      m.spot = [m.x, m.y]; m.cover = here.kind; m.v = 0;
      if (t < m.until && m.pose !== 'crawl') continue;
      const r = roll(this.seed, u.id, m.k, t, 8);
      if (m.role === 'mg') { m.pose = 'lmg_burst'; m.aim = this.aimAt(m, e); m.until = t + 2 + r * 2; }
      else if (m.role === 'officer') { m.pose = 'wait'; m.aim = undefined; m.until = t + 2; }
      else if (r < 0.35) { m.pose = 'cower'; m.aim = undefined; m.until = t + 2 + r * 4; }
      else { m.pose = m.cover === 'works' || m.cover === 'building' ? 'fire_kneel' : 'fire_prone'; m.aim = this.aimAt(m, e); m.until = t + 2 + r * 3; }
    }
  }

  /**
   * Falling back in order (a withdrawal, a block worn out at the brink, an army carried off by its break): by bounds,
   * one team facing the enemy and firing while the other runs back past it; at the double, a rolling rearguard.
   */
  private fallback(sq: Squad): void {
    const u = sq.u, t = this.b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), v = Math.max(1, sq.speed), h = sq.heading;
    const vLeap = 4.7 * N.rush / (N.rush + N.halt);
    sq.leap = sq.leap ? v < vLeap * 1.1 : v < vLeap * 0.9;
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, 'line');
      const s = toWorld(u, u.face, a, l);
      const cyc = N.rush + N.halt, ph = (t + (m.team === 2 ? cyc / 2 : 0) + m.jx * 0.08 + 100 * cyc) % cyc;
      const cover = sq.leap ? ph >= N.rush : roll(this.seed, u.id, m.k, Math.floor(t / 2)) < 0.2;
      if (cover && m.role !== 'officer' && e && this.b.gap(u, e) < 500) { m.v = 0; m.spot = [m.x, m.y]; m.pose = m.role === 'mg' ? 'lmg_burst' : 'fire_kneel'; m.aim = this.aimAt(m, e); }
      else { m.aim = undefined; this.follow(sq, m, s, 'run', 1.15); }
      m.until = t + 0.5;
      void dir;
    }
  }

  /** Routing: every man for himself, running for his own edge, spreading as he goes. */
  private flee(sq: Squad): void {
    const u = sq.u, t = this.b.t, back = u.side === 'att' ? Math.PI : 0, v = Math.max(2, sq.speed);
    for (const m of this.up(sq)) {
      const spread = Math.min(1, (t - sq.since) / 15);
      const a = back + (roll(this.seed, u.id, m.k, 13) - 0.5) * 0.9 * spread;
      const lag = (roll(this.seed, u.id, m.k, 14) - 0.5) * 30 * spread;
      const across = (m.jx * 2 + (roll(this.seed, u.id, m.k, 15) - 0.5) * u.w) * (0.5 + spread * 0.7);
      const [rx, ry] = right(u.face);
      const s: [number, number] = [u.x + across * rx + Math.sin(a) * (10 + lag), u.y + across * ry + Math.cos(a) * (10 + lag)];
      m.aim = undefined; m.hidden = undefined; m.until = t + 0.5;
      this.follow(sq, m, s, 'flee', 1.1);
    }
  }

  /** Rallied: the men gather on their officer, facing the enemy again. */
  private rally(sq: Squad): void {
    const u = sq.u, t = this.b.t, dir = this.threatDir(sq);
    const men = this.up(sq), lead = men.find((m) => m.role === 'officer') ?? men[0];
    if (!lead) return;
    lead.v = 0; lead.spot = [lead.x, lead.y]; lead.pose = 'idle'; lead.aim = undefined;
    men.forEach((m, i) => {
      if (m === lead) return;
      const ang = (i / men.length) * Math.PI * 2;
      const p: [number, number] = [lead.x + Math.sin(ang) * 7 + dir[0] * 4, lead.y + Math.cos(ang) * 7 + dir[1] * 4];
      if (Math.hypot(m.spot[0] - p[0], m.spot[1] - p[1]) > 4) this.go(sq, m, p, 'run', 4, t + 2, dir, 4);
      else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.v = 0; m.pose = 'wait'; }
    });
    void u;
  }

  /** Lorried infantry still aboard, cavalry still mounted: they ride where their block goes. */
  private mounted(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (u.kind === 'mot') {
      sq.veh.forEach((v, i) => {
        const s = toWorld(u, sq.heading, 0, u.d / 2 - 8 - i * 18);
        this.follow(sq, v, s, 'drive'); v.until = t + 1;
      });
      for (const m of this.up(sq)) {
        const lorry = sq.veh[m.k % sq.veh.length];
        m.spot = [lorry.spot[0], lorry.spot[1]]; m.slotAt = m.spot; m.pose = 'mounted'; m.v = lorry.v; m.aim = undefined; m.cover = 'none';
      }
      return;
    }
    for (const m of this.up(sq)) {
      const [a, l] = this.slot(sq, m, sq.phase === 'column' ? 'column' : 'line');
      const s = toWorld(u, sq.phase === 'column' ? sq.heading : u.face, a, l);
      m.aim = undefined; m.until = t + 1;
      this.follow(sq, m, s, 'ride');
      if (m.v === 0) m.pose = 'ride'; // mounted, standing
    }
  }

  /**
   * Lorries: aboard, they carry the block; once it is down they pull back behind it and park in what cover there is; a
   * battery's tractor waits behind its guns, and leads them when they move. A horse holder takes his mates' horses back.
   */
  private lorries(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (u.kind === 'cav' && sq.down) {
      for (const m of this.up(sq)) if (m.role === 'holder') { const s = toWorld(u, u.face, m.jx * 3, -u.d / 2 - 70); m.spot = s; m.slotAt = s; m.pose = 'horses'; m.v = 3; m.aim = undefined; }
    }
    if (u.kind !== 'mot' || !sq.down) return;
    sq.veh.forEach((v, i) => {
      if (sq.phase === 'flee' || sq.phase === 'fallback') {
        const s = toWorld(u, u.face, (i - 0.5) * 20, -u.d / 2 - 40);
        this.follow(sq, v, s, 'drive', 1.2); return;
      }
      const s = toWorld(u, u.face, (i - (sq.veh.length - 1) / 2) * 24, -u.d / 2 - 110);
      if (Math.hypot(s[0] - v.slotAt[0], s[1] - v.slotAt[1]) > 25 || t < 0.2) {
        const best = this.seek(sq, v, s, this.threatDir(sq), sq.veh, 30);
        v.spot = [best.x, best.y]; v.slotAt = s; v.cover = best.kind; v.pose = 'drive'; v.v = 6;
      } else if (Math.hypot(v.x - v.spot[0], v.y - v.spot[1]) < 1.5) { v.pose = 'parked'; v.v = 0; }
    });
  }

  /**
   * The anti-tank team's ambush: once enemy tanks come within range of a block holding or fighting where it stands, its
   * anti-tank man goes to the end of its line nearer their flank, lies hidden in the best cover there, and fires only
   * when they are close or are fighting his block; after each shot he moves. Foot in contact with tanks fighting alone
   * (rules: item 7, aloneAt) close in on them with grenades: the close-assault teams.
   */
  private ambush(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t;
    if (!fighting(u) || sq.phase === 'fallback' || sq.phase === 'flee' || sq.phase === 'column') return;
    const at = this.up(sq).find((m) => m.role === 'at');
    const tanks = b.units.filter((e) => e.side !== u.side && e.kind === 'arm' && fighting(e) && b.gap(u, e) < DR.ranges.ambush);
    if (at) {
      if (!tanks.length || sq.speed > 0.6) at.hidden = undefined;
      else {
        const tk = tanks.reduce((m, e) => (b.gap(u, e) < b.gap(u, m) ? e : m), tanks[0]);
        const [rx, ry] = right(u.face), side = Math.sign((tk.x - u.x) * rx + (tk.y - u.y) * ry) || 1;
        const end = toWorld(u, u.face, side * (u.w / 2 - 4), u.d / 2 - 6);
        const dir: [number, number] = [(tk.x - end[0]) / (Math.hypot(tk.x - end[0], tk.y - end[1]) || 1), (tk.y - end[1]) / (Math.hypot(tk.x - end[0], tk.y - end[1]) || 1)];
        const close = b.gap(u, tk) < 150 || tk.engaged === u.id;
        if (!at.hidden && !close) { this.go(sq, at, end, 'creep', 2.5, t + 3, dir, 14); at.hidden = true; at.aim = undefined; }
        else if (at.hidden && Math.hypot(at.x - at.spot[0], at.y - at.spot[1]) < 1 && !close) { at.pose = 'hide'; at.v = 0; }
        if (close && t >= at.until) {
          const ts = this.squads.get(tk.id), tank = ts?.men.filter((x) => x.fell === undefined && x.k < tk.figures).sort((p, q) => Math.hypot(p.x - at.x, p.y - at.y) - Math.hypot(q.x - at.x, q.y - at.y))[0];
          at.hidden = undefined; at.pose = 'fire_kneel'; at.v = 0; at.shot = t; at.aim = tank ? [tank.x, tank.y] : [tk.x, tk.y]; at.until = t + 6 + 3 * roll(this.seed, u.id, t);
          // Then off to a fresh place along the line.
          const next = toWorld(u, u.face, side * (u.w / 2 - 10 - 15 * roll(this.seed, u.id, t, 2)), u.d / 2 - 6);
          at.slotAt = next;
        } else if (!at.hidden && close && Math.hypot(at.x - at.slotAt[0], at.y - at.slotAt[1]) > 3 && t >= at.until - 3) this.go(sq, at, at.slotAt, 'creep', 2.5, t + 3, dir, 6);
      }
    }
    // Close assault on tanks fighting alone among our foot.
    const lone = tanks.find((e) => e.aloneAt !== undefined && t - e.aloneAt < 2 && (e.engaged === u.id || u.engaged === e.id));
    if (lone && isFoot(u)) {
      const ts = this.squads.get(lone.id);
      const tank = ts?.men.filter((x) => x.fell === undefined && x.k < lone.figures)[0];
      if (tank) {
        const team = this.up(sq).filter((m) => m.role === 'rifle' || m.role === 'at').sort((p, q) => Math.hypot(p.x - tank.x, p.y - tank.y) - Math.hypot(q.x - tank.x, q.y - tank.y)).slice(0, 2);
        for (const m of team) {
          const d = Math.hypot(m.x - tank.x, m.y - tank.y);
          if (d > 14) { if (t >= m.until || m.pose !== 'crawl') { const side = m.k % 2 ? 1 : -1, [rx, ry] = right(tank.face); m.spot = [tank.x + rx * side * 9, tank.y + ry * side * 9]; m.pose = 'crawl'; m.v = 2; m.until = t + 8; m.hidden = undefined; } }
          else if (t >= m.until || m.pose === 'crawl') { m.pose = 'throw_grenade'; m.v = 0; m.shot = t; m.aim = [tank.x, tank.y]; m.until = t + 2.5; }
        }
      }
    }
  }

  /** The officer walks his line every so often, crouched, stopping by each team. */
  private officer(sq: Squad): void {
    const u = sq.u, t = this.b.t;
    if (sq.phase !== 'fight' && sq.phase !== 'hold' && sq.phase !== 'startline') return;
    const o = this.up(sq).find((m) => m.role === 'officer');
    if (!o || t < sq.officerAt) return;
    sq.officerAt = t + 8 + 8 * roll(this.seed, u.id, t);
    const team = (Math.floor(t / 8) % 3) as 0 | 1 | 2;
    const mates = this.up(sq).filter((m) => m.team === team && m !== o);
    const by = mates.length ? mates[Math.floor(roll(this.seed, u.id, t, 1) * mates.length)] : null;
    const p: [number, number] = by ? [by.spot[0] - Math.sin(u.face) * 4, by.spot[1] - Math.cos(u.face) * 4] : toWorld(u, u.face, 0, -u.d / 2 + 6);
    this.go(sq, o, p, 'creep', 2.5, t + 6, undefined, 5);
  }

  // ---- Tanks --------------------------------------------------------------------------------------------------------

  /**
   * Tanks. On the road, nose to tail. Coming on, in pairs: where the block goes slowly one pair halts to cover while the
   * other drives to its next firing place; fast, all drive and a tank halts to fire now and then. In a fight each takes
   * the best firing place near its own: hull-down behind a crest where there is one, else a wood's or a house's corner;
   * after firing from the open she backs off a few metres and comes up again (by style). In woods and streets with
   * their own foot near, they keep behind the foot's line. Alone among enemy foot they close up. Shaken, they back off to
   * cover; routing, they drive for their own edge. Each tank's place is where her wreck would lie (sim.ts wreckOffset),
   * so a tank knocked out burns where she stood.
   */
  private tanks(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), e = this.threat(u), dir = this.threatDir(sq), ph = sq.phase, h = sq.heading;
    const tanks = this.up(sq);
    const lone = u.aloneAt !== undefined && t - u.aloneAt < 2;
    const v = Math.max(1, sq.speed);
    // With its own foot, on close ground: the foot leads, the tanks keep behind its line.
    const close = CLOSE.includes(groundOf(b.field, u.x, u.y));
    const foot = b.units.filter((f) => f.side === u.side && fighting(f) && FOOT_KINDS.includes(f.kind) && b.gap(u, f) < 150);
    const [fx, fy] = fwd(u.face);
    let behind = Infinity;
    if (close && foot.length) for (const f of foot) behind = Math.min(behind, (f.x - u.x) * fx + (f.y - u.y) * fy + f.d / 2 - 15);
    tanks.forEach((m, i) => {
      m.buttoned = lone || (e !== null && b.gap(u, e) < 120) || undefined;
      const n = sq.u.figures0 - 1 - m.k;
      const [wa, wl] = wreckOffset(u.id, n, u.w, u.d);
      if (ph === 'flee') { m.aim = undefined; this.follow(sq, m, toWorld(u, u.face, wa, wl), 'drive', 1.1); return; }
      if (ph === 'column') { m.aim = undefined; this.follow(sq, m, toWorld(u, h, 0, u.d / 2 - 10 - i * 22), 'drive'); return; }
      let l = wl;
      if (behind < Infinity) l = Math.min(l, behind);
      const s = toWorld(u, u.face, wa, l);
      if (ph === 'fallback') {
        // Backing off, guns still on the enemy.
        this.follow(sq, m, s, 'reverse'); m.aim = e ? [e.x, e.y] : undefined; return;
      }
      if (ph === 'deploy' || (ph === 'bound' && v >= 3.5)) {
        // All on, a tank halting a moment to fire where one is in reach.
        const halt = ph === 'bound' && e && b.gap(u, e) < 600 && roll(this.seed, u.id, m.k, Math.floor(t / 4)) < 0.25;
        if (halt) { m.spot = [m.x, m.y]; m.v = 0; m.pose = this.tankShot(sq, m, e, N) ? 'fire' : 'halt'; m.aim = this.aimAt(m, e); }
        else { this.follow(sq, m, s, 'drive'); m.aim = e && b.gap(u, e) < 900 ? [e.x, e.y] : undefined; }
        return;
      }
      if (ph === 'bound') {
        // Slow going: the pairs leapfrog.
        const pair = m.k < 2 ? 0 : 1, cyc = N.tankRush + N.tankHalt, off = pair ? cyc / 2 : 0;
        const moving = (t + off + 100 * cyc) % cyc < N.tankRush;
        if (moving) {
          if (m.pose !== 'drive' || t >= m.until) { const [hx, hy] = fwd(h); this.go(sq, m, [s[0] + hx * v * cyc * 0.45, s[1] + hy * v * cyc * 0.45], 'drive', v * cyc / N.tankRush, t + N.tankRush, dir); }
          m.aim = e ? [e.x, e.y] : undefined;
        } else if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 2 || m.pose !== 'drive') {
          m.v = 0; m.spot = [m.x, m.y]; m.pose = e && this.tankShot(sq, m, e, N) ? 'fire' : 'halt'; m.aim = this.aimAt(m, e);
        }
        return;
      }
      // Halted: in a fight, holding, shaken. Her firing place near her own: hull-down where the ground gives one.
      const shaken = ph === 'pinned' || u.state === 'wavering';
      const want: [number, number] = shaken ? [s[0] - dir[0] * 25, s[1] - dir[1] * 25] : s;
      if (Math.hypot(want[0] - m.slotAt[0], want[1] - m.slotAt[1]) > 10 || (m.pose === 'drive' && t >= m.until)) {
        this.go(sq, m, want, shaken ? 'reverse' : 'drive', shaken ? 2 : 4, t + 4, dir);
        m.aim = e ? [e.x, e.y] : undefined;
        return;
      }
      if (m.pose === 'reverse' && t < m.until) return;
      if (m.pose === 'reverse') { m.spot = [m.slotAt[0], m.slotAt[1]]; const best = this.seek(sq, m, m.slotAt, dir, sq.men); m.spot = [best.x, best.y]; m.cover = best.kind; m.pose = 'drive'; m.v = 3; m.until = t + 4; return; }
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) > 1.5) return;
      m.v = 0;
      if (!e || (u.engaged === undefined && b.gap(u, e) > 600)) { m.pose = 'halt'; m.aim = e ? [e.x, e.y] : undefined; return; }
      m.aim = this.aimAt(m, e);
      if (u.engaged !== undefined && this.tankShot(sq, m, e, N)) {
        m.pose = 'fire';
        // Shoot and scoot: from the open, back off and come up again elsewhere.
        if (m.cover === 'none' && roll(this.seed, u.id, m.k, t, 21) < N.scoot) { const p: [number, number] = [m.x - dir[0] * 14, m.y - dir[1] * 14]; m.spot = p; m.pose = 'reverse'; m.v = 2.5; m.until = t + 5; }
      } else m.pose = 'halt';
    });
    // A tank that has just fired shows it a moment (the 3D plays the round by its stamp).
    for (const m of tanks) if (m.pose === 'halt' && m.shot !== undefined && t - m.shot < 1.5) m.pose = 'fire';
  }

  /** Whether this tank fires now (each about every 6 to 10 s while her block fights). */
  private tankShot(sq: Squad, m: Man, e: LandUnit | null, N: Nums): boolean {
    if (!e || sq.u.engaged === undefined) return false;
    const t = this.b.t, every = 6 + 4 * roll(this.seed, sq.u.id, m.k, 31);
    if (m.shot !== undefined && t - m.shot < every) return false;
    m.shot = t;
    void N;
    return true;
  }

  // ---- Guns ---------------------------------------------------------------------------------------------------------

  /**
   * A battery. Moving, its guns are towed behind its tractor in column. Standing, they unlimber in a line behind the
   * block's middle, each in the best place near its own: behind a crest (firing over it at what it cannot see) or a
   * wood's edge. They fire by the style's drill: together, as fast as each can, or one after another down the line;
   * over open sights when the enemy is close or its tanks are near. Shelled, the crews go down a moment. A spotter goes
   * up with the friendly foot nearest the target while the guns fire far.
   */
  private guns(sq: Squad): void {
    const u = sq.u, b = this.b, t = b.t, N = this.nums(u), dir = this.threatDir(sq), h = sq.heading;
    const e = u.aim !== undefined ? b.unit(u.aim) ?? null : u.engaged !== undefined ? b.unit(u.engaged) ?? null : null;
    const guns = this.up(sq), tractor = sq.veh[0];
    if (sq.phase === 'limbered' || sq.phase === 'flee' || sq.phase === 'fallback') {
      guns.forEach((g, i) => { g.aim = undefined; this.follow(sq, g, toWorld(u, h, 0, -10 - i * 18), 'towed'); if (g.v === 0) g.pose = 'towed'; });
      if (tractor) { this.follow(sq, tractor, toWorld(u, h, 0, 8), 'drive'); }
      sq.fo = null;
      return;
    }
    if (tractor) { const s = toWorld(u, u.face, 0, -u.d / 2 - 50); if (Math.hypot(s[0] - tractor.slotAt[0], s[1] - tractor.slotAt[1]) > 20) { tractor.spot = s; tractor.slotAt = s; tractor.pose = 'drive'; tractor.v = 4; } else if (Math.hypot(tractor.x - tractor.spot[0], tractor.y - tractor.spot[1]) < 2) { tractor.pose = 'parked'; tractor.v = 0; } }
    const firing = u.engaged !== undefined && !!e;
    const direct = !!e && (b.gap(u, e) < LAND.guns.closeAt || (e.kind === 'arm' && b.gap(u, e) < 800));
    const hit = t - sq.shelledAt < DR.pin.shelled || sq.harm > DR.pin.harm;
    // The salvo clock.
    let fireNow: (g: Man, i: number) => boolean = () => false;
    if (firing && t >= sq.salvoAt) {
      if (N.salvo === 'battery') { sq.salvoAt = t + N.gunEvery; fireNow = () => true; }
      else if (N.salvo === 'ripple') { const i0 = sq.ripple % Math.max(1, guns.length); sq.ripple++; sq.salvoAt = t + N.gunEvery / Math.max(1, guns.length); fireNow = (_g, i) => i === i0; }
      else { sq.salvoAt = t + 0.5; fireNow = (g) => g.shot === undefined || t - g.shot >= N.gunEvery * (0.7 + 0.3 * roll(this.seed, u.id, g.k, Math.floor(t / 10))); }
    }
    guns.forEach((g, i) => {
      const [a, l] = this.slot(sq, g, 'line');
      const s = toWorld(u, u.face, a, l);
      if (Math.hypot(s[0] - g.slotAt[0], s[1] - g.slotAt[1]) > 12) { this.go(sq, g, s, 'towed', 3, t + 4, dir); g.aim = undefined; return; }
      if (Math.hypot(g.x - g.spot[0], g.y - g.spot[1]) > 1.5) return;
      g.v = 0;
      g.hidden = hit && roll(this.seed, u.id, g.k, Math.floor(t)) < 0.5 ? true : undefined; // the crew down a moment
      if (firing && fireNow(g, i) && !g.hidden) { g.shot = t; g.pose = 'fire'; }
      else if (g.pose !== 'fire' || (g.shot !== undefined && t - g.shot > 2.5)) g.pose = 'deployed';
      g.aim = e ? [e.x + (roll(this.seed, u.id, g.k, t) - 0.5) * e.w * 0.6, e.y + (roll(this.seed, u.id, g.k, t, 1) - 0.5) * e.d * 0.6] : undefined;
      g.buttoned = direct || undefined; // over open sights
    });
    this.spotter(sq, e, firing && !!e && b.gap(u, e) > DR.ranges.fo);
  }

  /**
   * The battery's spotter and his signaller: with the friendly foot block nearest the guns' target, on the highest
   * ground or the tallest house within its stretch, lying with his glasses on the target. Where no friendly foot is near
   * the target, at the battery's own front, on its highest ground.
   */
  private spotter(sq: Squad, e: LandUnit | null, on: boolean): void {
    const u = sq.u, b = this.b, t = b.t, f = b.field;
    if (!on || !e) { sq.fo = null; sq.foWith = undefined; sq.foOn = undefined; return; }
    const friends = b.units.filter((x) => x.side === u.side && fighting(x) && FOOT_KINDS.includes(x.kind) && b.gap(x, e) < 900);
    const host = friends.reduce<LandUnit | null>((m, x) => (!m || b.gap(x, e) < b.gap(m, e) ? x : m), null);
    const base = host ?? u;
    if (!sq.fo || sq.foWith !== base.id || sq.foOn !== e.id) {
      // His place: the best view toward the target within the host's stretch.
      let best: [number, number] = [base.x, base.y], score = -Infinity;
      for (let i = 0; i < 25; i++) {
        const a = (i % 5 - 2) * base.w / 5, l = (Math.floor(i / 5) - 2) * base.d / 3;
        const p = toWorld(base, base.face, a, l);
        const g = groundOf(f, p[0], p[1]);
        if (LAND.pace.foot[g] <= 0 || g === 'water' || g === 'blown') continue;
        const sc = heightOf(f, p[0], p[1]) + storeysOf(f, p[0], p[1]) * 3 + (g === 'woods' ? 1 : 0) - Math.hypot(p[0] - e.x, p[1] - e.y) * 0.002;
        if (sc > score) { score = sc; best = p; }
      }
      const face = bearing(best[0], best[1], e.x, e.y);
      const [rx, ry] = right(face);
      const mk = (k: number, role: Role, pose: Pose, dx: number): Man => ({ k, role, team: 0, x: best[0] + rx * dx - Math.sin(face) * 30, y: best[1] + ry * dx - Math.cos(face) * 30, face, pose, v: 3, spot: [best[0] + rx * dx, best[1] + ry * dx], cover: 'none', until: t + 1, slotAt: best, jx: 0, jy: 0, aim: [e.x, e.y] });
      sq.fo = [mk(200, 'fo', 'observe', 0), mk(201, 'fo', 'wait', 2.5)];
      sq.foWith = base.id; sq.foOn = e.id;
      for (const m of sq.fo) { const c = this.shelter(m.spot[0], m.spot[1], groundOf(f, m.spot[0], m.spot[1]), [Math.sin(face), Math.cos(face)], true, false); m.cover = c.kind; m.pose = 'creep'; }
    }
    for (const m of sq.fo) {
      m.aim = [e.x, e.y]; m.face = bearing(m.x, m.y, e.x, e.y);
      if (Math.hypot(m.x - m.spot[0], m.y - m.spot[1]) < 1) { m.v = 0; m.pose = m.k === 200 ? 'observe' : 'wait'; }
    }
  }

  // ---- Moving each man a step ---------------------------------------------------------------------------------------

  private walk(sq: Squad): void {
    const u = sq.u, f = this.b.field;
    const step = (m: Man): void => {
      if (m.fell !== undefined) return;
      const dx = m.spot[0] - m.x, dy = m.spot[1] - m.y, d = Math.hypot(dx, dy);
      if (d > 0.05 && m.v > 0) {
        const s = Math.min(d, m.v * DT);
        m.x = clamp(m.x + (dx / d) * s, 1, f.w - 1); m.y = clamp(m.y + (dy / d) * s, 1, f.h - 1);
        const head = Math.atan2(dx, dy);
        m.face = m.pose === 'reverse' ? head + Math.PI : head;
        if (s >= d - 1e-9 && MOVING.has(m.pose) && m.role !== 'lorry' && sq.phase !== 'column' && sq.phase !== 'limbered') this.arrive(sq, m);
      } else if (m.aim) m.face = bearing(m.x, m.y, m.aim[0], m.aim[1]);
      else if (m.v === 0 && fighting(u)) { const e = this.threat(u); if (e) m.face = bearing(m.x, m.y, e.x, e.y); }
      // Never far from his block: the block is the truth (the rules' box); a man left behind is carried with it, and his
      // place with him.
      const here: [number, number] = [m.x, m.y], [lx, ly] = this.leash(sq, here, m.role === 'lorry' ? 120 : 0);
      if (lx !== here[0] || ly !== here[1]) {
        m.x = lx; m.y = ly; m.spot = this.leash(sq, m.spot, m.role === 'lorry' ? 116 : -4);
        // Dragged by his block: he is running to catch up, never sliding over the ground lying down.
        if (!MOVING.has(m.pose) && m.role !== 'holder') {
          m.spot = this.leash(sq, m.slotAt, -4); m.v = Math.min(CATCH_UP, Math.max(2.5, sq.speed * 1.25)); m.aim = undefined; m.hidden = undefined;
          m.pose = m.role === 'tank' ? 'drive' : m.role === 'lorry' ? 'drive' : m.role === 'gun' ? 'towed' : sq.phase === 'flee' ? 'flee' : sq.phase === 'bound' || sq.phase === 'fight' ? 'advance' : 'run';
          m.until = this.b.t + 1;
        }
      }
    };
    for (const m of sq.men) step(m);
    for (const m of sq.veh) step(m);
    if (sq.fo) for (const m of sq.fo) {
      const dx = m.spot[0] - m.x, dy = m.spot[1] - m.y, d = Math.hypot(dx, dy);
      if (d > 0.05 && m.v > 0) { const s = Math.min(d, m.v * DT); m.x += (dx / d) * s; m.y += (dy / d) * s; }
    }
  }

  // ---- What the 3D game reads ---------------------------------------------------------------------------------------

  /**
   * Every block's men, as one side sees them: an enemy block not yet identified (fog, b464(2)) sends none, so the 3D
   * game draws its plain stand. Fallen men stay to the end of the fight where they fell.
   */
  view(as?: Side): MenBlock[] {
    const b = this.b, out: MenBlock[] = [];
    const r1 = (v: number): number => Math.round(v * 10) / 10, r2 = (v: number): number => Math.round(v * 100) / 100;
    const one = (m: Man): MenOut => {
      const o: MenOut = { k: m.k, r: m.role, x: r1(m.x), y: r1(m.y), f: r2(m.face), p: m.pose, v: r1(m.v), c: m.cover };
      if (m.aim) o.a = [r1(m.aim[0]), r1(m.aim[1])];
      if (m.shot !== undefined) o.s = r1(m.shot);
      if (m.fell !== undefined) o.d = r1(m.fell);
      if (m.hidden) o.h = 1;
      if (m.buttoned) o.b = 1;
      return o;
    };
    for (const sq of this.squads.values()) {
      const u = sq.u;
      if (as !== undefined && u.side !== as && !b.known(u)) continue;
      if (u.state === 'withdrawn' && !sq.men.some((m) => m.fell !== undefined)) continue;
      const men = sq.men.filter((m) => m.fell !== undefined || (m.k < u.figures && live(u))).map(one);
      for (const v of sq.veh) if (live(u)) men.push(one(v));
      const blk: MenBlock = { id: u.id, ph: sq.phase, men };
      if (sq.fo && live(u)) { blk.fo = sq.fo.map(one); blk.foWith = sq.foWith; blk.foOn = sq.foOn; }
      out.push(blk);
    }
    return out;
  }

  /** The block's men, for tests and the browser page. */
  squad(id: number): { phase: Phase; men: Man[]; veh: Man[]; fo: Man[] | null } | undefined {
    const sq = this.squads.get(id);
    return sq ? { phase: sq.phase, men: sq.men, veh: sq.veh, fo: sq.fo } : undefined;
  }
}

/**
 * Who is on a block's tray, model by model (the rules lift the last first, so model 0 stands longest): a full tray of
 * foot has its officer, two machine guns (one a team), its anti-tank man and its riflemen, the two teams interleaved
 * so both thin together; engineers carry charges in their last two; a cavalry tray's every fourth man holds the horses
 * once they are off; a regiment's thin tray has a machine gun and riflemen. Tanks and guns are each their own.
 */
export function rolesFor(kind: Kind, tray: number, n: number): [Role, 0 | 1 | 2][] {
  const out: [Role, 0 | 1 | 2][] = [];
  for (let k = 0; k < n; k++) {
    if (kind === 'arm') { out.push(['tank', (k % 2 ? 2 : 1)]); continue; }
    if (kind === 'art') { out.push(['gun', 0]); continue; }
    if (tray >= 8) {
      if (k === 0) out.push(['officer', 0]);
      else if (k === 1) out.push(['mg', 1]);
      else if (k === 2) out.push(['mg', 2]);
      else if (k === 3) out.push([kind === 'fort' ? 'mg' : 'at', kind === 'fort' ? 1 : 0]);
      else {
        const team: 1 | 2 = k % 2 ? 2 : 1;
        if (kind === 'eng' && k >= tray - 2) out.push(['sapper', team]);
        else if (kind === 'cav' && k % 4 === 3) out.push(['holder', team]);
        else out.push(['rifle', team]);
      }
    } else out.push(k === 0 ? ['mg', 1] : ['rifle', k % 2 ? 2 : 1]);
  }
  return out;
}

/**
 * Where a block's n-th wreck lies in its box, metres across (right of its middle) and along (forward): the one formula
 * the rules use to lay a knocked-out tank's wreck (sim.ts wreck) and the drill uses to place each tank, so a tank burns
 * where she stood. Fixed by the block and the count, never by the battle's dice.
 */
export function wreckOffset(id: number, n: number, w: number, d: number): [number, number] {
  return [(mix(id * 977 + n * 131 + 7) - 0.5) * w * 0.9, (mix(id * 613 + n * 389 + 3) - 0.5) * d * 0.8];
}
