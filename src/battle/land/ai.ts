// The computer general on the field. It fights on general principles, the same plan for every army, with its style's
// numbers on it (styles.ts; the tactics panel, item 1; the design director, ruling 239): never a script, never a
// nation's own battle.
//
// Attacking: its guns open first and fire on a main effort, a short stretch of the enemy's line where the ground is
// softest and the enemy weakest; the main effort's foot marches to a start line out of the enemy's reach, its tanks
// and lorries beside it, waits there under its guns, then goes in block by block, the tanks keeping with the foot;
// the rest of the army pins the line in front of it at once; once the main target is fixed and shaken, the tanks hook
// round its flank (a bold army's behind it, or onto its guns). It holds nothing back: the card prices the whole army
// (the design director, ruling 239's rider).
// A mass has no main effort: every block takes the enemy in its own lane at once, its tanks with the line.
//
// Defending: hold the ground, help a neighbour who is in a fight, and go after a block that is wavering or running
// close by. A reserve held behind the line goes, once the line is struck, to where it is struck hardest; a block on a
// quiet stretch of the line, with no enemy facing it, closes on the fight (the design director, b407). Every style
// holds the same quarter back (units.ts); how far it steps out is its style's, never past its held ground. A
// methodical defence is the one it always was.
//
// A side the player commands has no style: handed to the computer, it fights as the plain general always has (every
// block its own lane, tanks for the ends of the enemy's line), and so does a battle with no styles.

import { dist } from '../core/geometry';
import type { Side } from '../core/types';
import { coverAt, groundAt, type Ground } from './field';
import type { LandBattle } from './sim';
import { LAND, MOTION, reachOf } from './rules';
import { STYLE, type Style } from './styles';
import type { LandUnit } from './units';

const ok = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn' && u.state !== 'routing';
const isFoot = (u: LandUnit): boolean => MOTION[u.kind] === 'foot';
const isMobile = (u: LandUnit): boolean => u.kind === 'arm' || u.kind === 'mot';
/** Close ground where tanks see little: they keep with their foot there and never hook (item 7). */
const CLOSE: Ground[] = ['woods', 'buildings', 'industry', 'suburb'];

/**
 * An attack's plan, set at its general's first thought. Read by the fight (who goes where and when) and sent to the 3D
 * game (phase, main, startY): shells on the main stretch, foot down at the start line, fire lifting as it goes in,
 * tanks wheeling round a flank, a mass going in all along the line.
 */
export interface AttackPlan {
  /** What the attack is doing now: its guns firing for the main effort, the assault, the tanks' hook. */
  phase: 'guns' | 'assault' | 'hook';
  /** The enemy blocks of the main effort, the main target first (none for a mass). */
  main: number[];
  /** Metres north of the start line where the main effort waits (null for a mass). */
  startY: number | null;
  /** Each block's part: the main effort, or pinning the line. */
  role: Record<number, 'main' | 'pin'>;
  /** Each tank or lorry block's group (LandOrder.together), and each foot block's. */
  group: Record<number, number>;
  /** Where each block of the main effort waits on the start line. */
  spot: Record<number, [number, number]>;
  /** When the main effort was all on its start line, and when it went in (null until then). */
  arrived: number | null;
  went: number | null;
  /** The hook, by tank or lorry block: going round to its point, then in, on the block it strikes. */
  hook: Record<number, { stage: 'out' | 'in'; at: [number, number]; target: number }>;
}

/** The main effort's group id: above every block's id, so a pinning block's group (its own foot block's id) never matches it. */
const MAIN = 100000;

export function thinkFor(b: LandBattle, side: Side): void {
  const mine = b.units.filter((u) => u.side === side && ok(u) && u.kind !== 'fort');
  const foes = b.units.filter((u) => u.side !== side && ok(u));
  if (!foes.length) return;
  const style = b.style[side];
  if (side === 'att') {
    if (style) planned(b, side, style, mine, foes);
    else plain(b, side, mine, foes);
    return;
  }
  defend(b, side, style, mine, foes);
}

const by = (b: LandBattle, side: Side, u: LandUnit, t: LandUnit, together?: number): void => {
  b.command({ do: 'attack', units: [u.id], target: t.id, ...(together !== undefined ? { together } : {}) }, side);
};
const hasTarget = (u: LandUnit, foes: LandUnit[]): boolean => u.order?.kind === 'attack' && foes.some((f) => f.id === u.order!.target);
/** The enemy in this block's lane: nearest across the field, a little for depth. */
const inLane = (u: LandUnit, foes: LandUnit[]): LandUnit => foes.reduce((best, f) => (Math.abs(f.x - u.x) + 0.3 * Math.abs(f.y - u.y) < Math.abs(best.x - u.x) + 0.3 * Math.abs(best.y - u.y) ? f : best), foes[0]);

/** Guns come up until the enemy is in reach, and stand. */
function gunsUp(b: LandBattle, side: Side, u: LandUnit, foes: LandUnit[]): void {
  const near = foes.reduce((m, f) => Math.min(m, b.gap(u, f)), Infinity);
  const reach = reachOf(u);
  if (near > reach * 0.9 && !u.order) {
    const cy = foes.reduce((s, f) => s + f.y, 0) / foes.length;
    b.command({ do: 'move', units: [u.id], x: u.x, y: Math.min(cy - reach * 0.75, u.y + 400) }, side);
  }
}

/**
 * The plain general, as it always was: every block takes an enemy block in its own lane and goes for it; tanks and
 * lorries go for the ends of the enemy line; guns come up until the enemy is in reach and stand. A side the player
 * commands, handed to the computer, fights so.
 */
function plain(b: LandBattle, side: Side, mine: LandUnit[], foes: LandUnit[]): void {
  const sorted = [...foes].sort((a, c) => a.x - c.x);
  for (const u of mine) {
    if (u.order?.kind === 'withdraw') continue;
    if (u.kind === 'art') { gunsUp(b, side, u, foes); continue; }
    if (hasTarget(u, foes)) continue;
    let t: LandUnit;
    if (isMobile(u)) {
      // The nearer end of their line.
      const left = sorted[0], right = sorted[sorted.length - 1];
      t = dist(u.x, u.y, left.x, left.y) < dist(u.x, u.y, right.x, right.y) ? left : right;
    } else t = inLane(u, foes);
    by(b, side, u, t);
  }
}

/** A block's weight now: what it strikes with, as its blows count it. */
const weight = (u: LandUnit): number => u.power0 * (0.35 + 0.65 * (u.str / u.str0));

/** Sets the attack's plan at its general's first thought (styles.ts: the numbers). */
function makePlan(b: LandBattle, side: Side, s: Style, mine: LandUnit[], foes: LandUnit[]): AttackPlan {
  const P: AttackPlan = { phase: s.plan ? 'guns' : 'assault', main: [], startY: null, role: {}, group: {}, spot: {}, arrived: null, went: null, hook: {} };
  const rest = mine.filter((u) => u.kind !== 'art');
  if (!s.plan || !rest.length) {
    for (const u of rest) P.role[u.id] = 'pin';
    P.went = 0;
    return P;
  }
  // 1. The main effort: the enemy line's softest, weakest stretch. Each block of their line (not their reserve, works
  // or guns) scores the cover of its ground (softer higher) over its weight, more at an end of the line, less dug in.
  let line = foes.filter((f) => f.kind !== 'fort' && f.kind !== 'art' && f.reserve !== 'held');
  if (!line.length) line = foes.filter((f) => f.kind !== 'art');
  if (!line.length) line = foes;
  line = [...line].sort((a, c) => a.x - c.x || a.id - c.id);
  const score = line.map((f, i) => (LAND.cover[coverAt(b.field, f.x, f.y)] / Math.max(1e-6, weight(f))) * (i === 0 || i === line.length - 1 ? 1.25 : 1) * (f.dug ? 0.8 : 1));
  let k = 0;
  for (let i = 1; i < line.length; i++) if (score[i] > score[k]) k = i;
  P.main = [line[k].id];
  if (s.stretch >= 2 && line.length > 1) {
    const l = k > 0 ? k - 1 : -1, r = k < line.length - 1 ? k + 1 : -1;
    const n = l < 0 ? r : r < 0 ? l : score[l] >= score[r] ? l : r;
    P.main.push(line[n].id);
  }
  const stretch = P.main.map((id) => b.unit(id)!);
  const cx = stretch.reduce((a, f) => a + f.x, 0) / stretch.length;
  // 2. Who goes where: nearest the main stretch first, the main effort until it has its share of the army's weight; the
  // rest pin the line in front of them.
  const order = [...rest].sort((a, c) => Math.abs(a.x - cx) - Math.abs(c.x - cx) || a.id - c.id);
  const total = rest.reduce((a, u) => a + u.power0, 0);
  let got = 0, i = 0;
  for (; i < order.length && (got < s.main * total || i === 0); i++) { P.role[order[i].id] = 'main'; got += order[i].power0; }
  for (const u of order.slice(i)) P.role[u.id] = 'pin';
  // The main effort carries foot where the army has any: the nearest foot block swaps in for its furthest block.
  const mainUs = (): LandUnit[] => order.filter((u) => P.role[u.id] === 'main');
  if (!mainUs().some(isFoot)) {
    const foot = order.find((u) => isFoot(u) && P.role[u.id] !== 'main');
    if (foot) { const out = mainUs()[mainUs().length - 1]; P.role[out.id] = P.role[foot.id]; P.role[foot.id] = 'main'; }
  }
  // Tanks and lorries never pin alone: with no pinning foot to go beside, they go with the main effort.
  if (!order.some((u) => P.role[u.id] === 'pin' && !isMobile(u))) for (const u of order) if (P.role[u.id] === 'pin') P.role[u.id] = 'main';
  // 3. The start line: the plan's distance short of the enemy's front, on the attack's own bank where a river lies
  // between them, never behind where a block already stands.
  const dir = side === 'att' ? 1 : -1;
  const front = dir > 0 ? Math.min(...line.map((f) => f.y - f.d / 2)) : Math.max(...line.map((f) => f.y + f.d / 2));
  let y = front - dir * STYLE.startLine;
  if (b.ticket.place.river) {
    const near = nearBank(b, cx, dir);
    if (near !== null) y = dir > 0 ? Math.min(y, near - 60) : Math.max(y, near + 60);
  }
  P.startY = y;
  // Its foot straight up to the line from where they stand, before the stretch they were chosen nearest to; its tanks
  // and lorries behind its foot, beside them rather than out on the wings where they drew up.
  const mainFoot = mainUs().filter((u) => !isMobile(u)), mainMobile = mainUs().filter(isMobile);
  const clampX = (u: LandUnit, x: number): number => Math.min(b.field.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, x));
  for (const u of mainFoot) P.spot[u.id] = [u.x, dir > 0 ? Math.max(u.y, y) : Math.min(u.y, y)];
  const behind: Record<number, number> = {};
  for (const u of [...mainMobile].sort((a, c) => a.x - c.x || a.id - c.id)) {
    const f = mainFoot.reduce<LandUnit | null>((m, x) => (!m || Math.abs(x.x - u.x) < Math.abs(m.x - u.x) ? x : m), null);
    const n = f ? (behind[f.id] = (behind[f.id] ?? -1) + 1) : 0;
    const x = f ? f.x + (n % 2 ? 1 : -1) * Math.ceil(n / 2) * (u.w + 20) : u.x;
    P.spot[u.id] = [clampX(u, x), dir > 0 ? Math.max(u.y, y - 90) : Math.min(u.y, y + 90)];
  }
  for (const u of mainUs()) P.group[u.id] = MAIN;
  return P;
}

/** Metres north of the near edge of the card's river at this x, as the attack comes on; null where none lies there. */
function nearBank(b: LandBattle, x: number, dir: number): number | null {
  const f = b.field, wet = (g: Ground): boolean => g === 'water' || g === 'blown' || g === 'bridge';
  let best: number | null = null;
  for (const dx of [-150, 0, 150]) {
    const cxx = Math.min(f.w - 1, Math.max(0, x + dx));
    for (let y = dir > 0 ? 0 : f.h - 1; dir > 0 ? y < f.h : y >= 0; y += dir * f.cell) {
      if (wet(groundAt(f, cxx, y))) { best = best === null ? y : dir > 0 ? Math.min(best, y) : Math.max(best, y); break; }
    }
  }
  return best;
}

/** The first block of the main stretch still fighting. */
const mainNow = (b: LandBattle, P: AttackPlan): LandUnit | null => P.main.map((id) => b.unit(id)).find((f) => !!f && ok(f)) ?? null;

/** A block of the main effort: the main stretch while it stands, then its lane. */
function mainTarget(b: LandBattle, P: AttackPlan, u: LandUnit, foes: LandUnit[]): LandUnit {
  const stretch = P.main.map((id) => b.unit(id)).filter((f): f is LandUnit => !!f && ok(f));
  if (stretch.length) return stretch.reduce((m, f) => (Math.abs(f.x - u.x) < Math.abs(m.x - u.x) ? f : m), stretch[0]);
  return inLane(u, foes);
}

/** The attack on its plan (styles.ts: the numbers). */
function planned(b: LandBattle, side: Side, s: Style, mine: LandUnit[], foes: LandUnit[]): void {
  const P = (b.plan[side] ??= makePlan(b, side, s, mine, foes));
  const limit = b.ticket.limit;
  const guns = mine.filter((u) => u.kind === 'art'), rest = mine.filter((u) => u.kind !== 'art');
  const role = (u: LandUnit): 'main' | 'pin' => P.role[u.id] ?? 'pin';
  // Guns: fire on the main effort, the next block of its stretch when one breaks, then on whatever is nearest; they stay
  // where they stand unless nothing is in reach (a deep city).
  for (const g of guns) {
    if (s.plan) { const t = mainNow(b, P); if (t) g.aim = t.id; else delete g.aim; }
    if (g.order?.kind !== 'withdraw') gunsUp(b, side, g, foes);
  }
  // The main effort: to its start line, wait there under its guns, then in.
  if (P.went === null) {
    const us = rest.filter((u) => role(u) === 'main');
    const stood = us.filter((u) => !isMobile(u)).length ? us.filter((u) => !isMobile(u)) : us;
    for (const u of us) {
      const at = P.spot[u.id];
      if (!at || u.order?.kind === 'withdraw') continue;
      if (!u.order && dist(u.x, u.y, at[0], at[1]) > 20 && u.engaged === undefined) b.command({ do: 'move', units: [u.id], x: at[0], y: at[1], face: side === 'att' ? 0 : Math.PI }, side);
    }
    // On its start line when three quarters of its foot by weight are there: a straggler held up by a friend in its way
    // does not hold the rest.
    const there = (u: LandUnit): boolean => !u.order && (!P.spot[u.id] || dist(u.x, u.y, P.spot[u.id][0], P.spot[u.id][1]) <= 25);
    const all = stood.reduce((a, u) => a + u.power0, 0);
    if (P.arrived === null && stood.filter(there).reduce((a, u) => a + u.power0, 0) >= 0.75 * all) P.arrived = b.t;
    // It waits under its own guns: an army with none on the field goes in as soon as it is on its start line.
    const wait = guns.length ? s.wait : 0;
    if ((P.arrived !== null && b.t >= P.arrived + wait) || b.t >= STYLE.latest * limit || !us.length) { P.went = b.t; P.phase = 'assault'; }
  }
  const going = P.went !== null;
  // The pinning foot first, so their tanks can go with them.
  const pinFoot = rest.filter((u) => role(u) === 'pin' && !isMobile(u));
  for (const u of pinFoot) {
    if (u.order?.kind === 'withdraw' || hasTarget(u, foes)) continue;
    by(b, side, u, inLane(u, foes), u.id);
    P.group[u.id] = u.id;
  }
  for (const u of rest) {
    if (u.order?.kind === 'withdraw') continue;
    const r = role(u);
    if (r === 'pin' && isMobile(u)) {
      if (hasTarget(u, foes)) continue;
      // Tanks and lorries go with the nearest pinning foot block, at its target; alone where the line has none.
      const partner = pinFoot.filter((f) => ok(f) && f.order?.kind === 'attack').reduce<LandUnit | null>((m, f) => (!m || Math.abs(f.x - u.x) < Math.abs(m.x - u.x) ? f : m), null);
      const t = partner ? foes.find((f) => f.id === partner.order!.target) : undefined;
      if (partner && t) { P.group[u.id] = partner.id; by(b, side, u, t, partner.id); }
      else by(b, side, u, inLane(u, foes));
      continue;
    }
    if (r === 'main' && going) {
      if (P.hook[u.id]) continue;
      if (hasTarget(u, foes)) continue;
      if (isMobile(u)) {
        // With the main effort's foot, at the stretch block nearest it.
        const foot = rest.filter((f) => role(f) === 'main' && !isMobile(f) && ok(f));
        const partner = foot.reduce<LandUnit | null>((m, f) => (!m || Math.abs(f.x - u.x) < Math.abs(m.x - u.x) ? f : m), null);
        const t = partner?.order?.kind === 'attack' ? foes.find((f) => f.id === partner.order!.target) : undefined;
        by(b, side, u, t ?? mainTarget(b, P, partner ?? u, foes), foot.length ? MAIN : undefined);
      } else by(b, side, u, mainTarget(b, P, u, foes), MAIN);
    }
  }
  if (!s.plan) return;
  // The hook: once the main target is held by the main effort's foot and shaken, its tanks go round its nearer flank
  // and strike it there (a bold army's go behind it, or onto its guns close behind). Never in woods or streets, where
  // they stay with the foot.
  const t = mainNow(b, P);
  if (going && t && s.hook !== null) {
    const fixed = rest.some((f) => isFoot(f) && f.engaged === t.id) || (t.engaged !== undefined && rest.some((f) => f.id === t.engaged && isFoot(f)));
    if (fixed && t.mor < s.hook * t.mor0 && !CLOSE.includes(coverAt(b.field, t.x, t.y))) {
      const dir = side === 'att' ? 1 : -1;
      for (const u of rest) {
        if (role(u) !== 'main' || !isMobile(u) || P.hook[u.id] || u.order?.kind === 'withdraw') continue;
        const flank = Math.sign(u.x - t.x) || 1;
        let target = t;
        if (s.deep) {
          const gun = foes.filter((g) => g.kind === 'art' && dir * (g.y - t.y) > 0 && dir * (g.y - t.y) <= STYLE.deepBehind && Math.abs(g.x - t.x) <= STYLE.deepBehind)
            .reduce<LandUnit | null>((m, g) => (!m || dist(g.x, g.y, t.x, t.y) < dist(m.x, m.y, t.x, t.y) ? g : m), null);
          if (gun) target = gun;
        }
        const at: [number, number] = [t.x + flank * (t.w / 2 + u.w / 2 + 80), t.y + (s.deep ? dir * (t.d / 2 + 60) : 0)];
        const x = Math.min(b.field.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, at[0]));
        if (CLOSE.includes(groundAt(b.field, x, at[1]))) continue;
        P.hook[u.id] = { stage: 'out', at: [x, at[1]], target: target.id };
        b.command({ do: 'move', units: [u.id], x, y: at[1] }, side);
        P.phase = 'hook';
      }
    }
  }
  for (const [id, h] of Object.entries(P.hook)) {
    const u = b.unit(Number(id));
    if (!u || !ok(u) || u.order?.kind === 'withdraw') continue;
    const tg = b.unit(h.target);
    if (!tg || !ok(tg)) { if (!hasTarget(u, foes)) by(b, side, u, mainTarget(b, P, u, foes)); continue; }
    if (h.stage === 'out' && (!u.order || dist(u.x, u.y, h.at[0], h.at[1]) < 40)) { h.stage = 'in'; by(b, side, u, tg); }
    else if (h.stage === 'in' && !hasTarget(u, foes)) by(b, side, u, mainTarget(b, P, u, foes));
  }
}

/**
 * The stretch an attack is coming at: of the enemy's blocks on the march (not guns) and this line's own (not guns, not
 * held), the pair nearest each other within 'within' metres; null while none is that near.
 */
function coming(b: LandBattle, mine: LandUnit[], foes: LandUnit[], within: number): { line: LandUnit; foe: LandUnit } | null {
  let best: { line: LandUnit; foe: LandUnit } | null = null, gap = within;
  for (const f of foes) {
    if (f.kind === 'art' || f.kind === 'fort' || f.speed <= 0.5) continue;
    for (const l of mine) {
      if (l.kind === 'art' || l.reserve === 'held') continue;
      const g = b.gap(f, l);
      if (g < gap) { gap = g; best = { line: l, foe: f }; }
    }
  }
  return best;
}

/**
 * A held block walking behind its line toward that stretch: along its own row, never further from home than the held
 * ground reaches (LAND.hold.within, less a margin), facing the enemy. The order goes again only when the stretch moves.
 */
function walk(b: LandBattle, side: Side, u: LandUnit, near: { line: LandUnit; foe: LandUnit }): void {
  const most = LAND.hold.within - 10;
  const want = Math.round(u.home[0] + Math.max(-most, Math.min(most, near.line.home[0] - u.home[0])));
  if (b.shifted.get(u.id) === want) return;
  b.shifted.set(u.id, want);
  b.command({ do: 'move', units: [u.id], x: want, y: u.home[1], face: Math.PI }, side);
  b.reserveSeen(side, near.foe);
}

// Defending: the line stays where it was drawn up. A block steps out only to finish one that is wavering right in front
// of it, or to take one that has come round a neighbour's flank, and never far from its own ground (its style's leash;
// today's 300 m where it has none). In a capital's or a city's streets never out of them: an enemy south of the edge of
// its core is left to come on into the houses (the design director, b529).
function defend(b: LandBattle, side: Side, style: Style | null, mine: LandUnit[], foes: LandUnit[]): void {
  const leash = style?.defLeash ?? 300, finish = style?.defFinish ?? 150;
  const edge = b.field.streetEdge, cell = b.field.cell;
  const outside = (f: LandUnit): boolean => {
    if (!edge) return false;
    const y = edge[Math.min(edge.length - 1, Math.max(0, Math.floor(f.x / cell)))];
    return y < 0 || f.y < y;
  };
  // Where the line is struck, hardest first: its blocks in a fight, the most worn first (b407). Its guns firing from
  // far back are not the line struck, as they are not contact (sim.ts contactAt).
  const struck = mine.filter((m) => m.kind !== 'art' && m.reserve !== 'held' && m.engaged !== undefined && foes.some((f) => f.id === m.engaged))
    .sort((a, c) => a.str / a.str0 - c.str / c.str0);
  let next = 0;
  const sendTo = (u: LandUnit): void => {
    const at = struck[next++ % struck.length];
    u.reserve = 'sent';
    by(b, side, u, foes.find((f) => f.id === at.engaged)!);
  };
  // Bold, while the line is not yet struck: the held block walks along its own row toward the stretch the attack is
  // coming at (the design director, ruling 239's rider, 03:20Z). It strikes nothing on the way and is sent, as every
  // reserve is, once the line is struck. A methodical or mass defence's stands in the middle until then.
  const near = !struck.length && style?.defShift ? coming(b, mine, foes, style.defShift) : null;
  for (const u of mine) {
    if (u.kind === 'art' || u.order?.kind === 'withdraw') continue;
    if (u.reserve === 'held') { if (struck.length) sendTo(u); else if (near) walk(b, side, u, near); continue; }
    if (u.reserve === 'sent') { if (!hasTarget(u, foes) && struck.length) sendTo(u); continue; }
    // A quiet stretch: no enemy in this block's lane anywhere on the field, while the line is struck elsewhere.
    if (struck.length && !hasTarget(u, foes) && u.engaged === undefined && !foes.some((f) => Math.abs(f.x - u.home[0]) < LAND.quiet)) { sendTo(u); continue; }
    const away = dist(u.x, u.y, u.home[0], u.home[1]);
    if (hasTarget(u, foes)) {
      const t = foes.find((f) => f.id === u.order!.target)!;
      if (away > leash || (u.engaged !== t.id && outside(t))) b.command({ do: 'move', units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side);
      continue;
    }
    if (u.engaged !== undefined) continue;
    if (away > 40 && !u.order) { b.command({ do: 'move', units: [u.id], x: u.home[0], y: u.home[1], face: Math.PI }, side); continue; }
    const shaky = foes.find((f) => f.state === 'wavering' && b.gap(u, f) < finish && !outside(f));
    if (shaky) { by(b, side, u, shaky); continue; }
    const round = foes.find((f) => dist(f.x, f.y, u.home[0], u.home[1]) < leash && !outside(f) && mine.some((m) => m.engaged === f.id && m !== u));
    if (round) by(b, side, u, round);
  }
}
