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
/**
 * A team firing from far back (the design director, 26 November 2026 00:41Z: one rule for every team kind): never the
 * neighbour that came round a flank, the one a line block steps out for (defend). Each kind under its own row: a sniper
 * pair exists only with sniper teams on; the machine-gun and mortar teams are read so under `teams` (sim.teamsRule and
 * ownArm, the ground thread's mend T of 25 November 17:52Z), which joins here as a second term of this one rule.
 */
// (b: the battle whose rows say which kinds count; at the join the teams' term reads b.teamsRule)
const firesFromBack = (_b: LandBattle, m: LandUnit): boolean => m.team === 'snp';
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
  // Tank blocks going round to a tank block's flank (measured armour, release 2) are on that errand till they are there.
  const round = b.measured ? flanksNow(b, side) : null;
  // A block the player has taken in hand is his, not his staff's (direct command, release 2: LandUnit.taken).
  const mine = b.units.filter((u) => u.side === side && ok(u) && u.kind !== 'fort' && !u.taken && !round?.has(u.id));
  // Only what its side has found (release 2, hidden in the woods: "the AI will not know they are there").
  const foes = b.units.filter((u) => u.side !== side && ok(u) && !u.hid);
  if (!foes.length) {
    // An attack that sees nobody still has the place to take: it feels its way forward until it finds them.
    if (side === 'att' && b.units.some((u) => u.side !== side && ok(u))) probe(b, side, mine);
    if (side === 'att' && b.hide && b.ambushed.has(side)) searchCover(b, side, mine);
    return;
  }
  const style = b.style[side];
  // Hold until relieved (release 2): its blocks waiting for help, and those sent to them, are left out of its other orders.
  const stand = b.relief && b.hasJoiners(side) ? relieve(b, side, mine, foes) : null;
  if (side === 'att') {
    // Corps come up from next door (release 2): sent, as a b407 reserve, where the attack is struck hardest, and till
    // then held where they came on, no part of the attack's own plan. (No attacking block is a reserve otherwise.)
    sendOn(b, side, mine, foes, stand);
    const line = mine.some((u) => u.reserve === 'sent' || stand?.has(u.id)) ? mine.filter((u) => u.reserve !== 'sent' && !stand?.has(u.id)) : mine;
    if (style) planned(b, side, style, line, foes);
    else plain(b, side, line, foes);
    if (b.measured) seekFlanks(b, side, mine);
    if (b.hide && b.ambushed.has(side)) searchCover(b, side, mine);
    if (b.smoke) smokeIn(b, side, mine, foes);
    if (b.hide) counterSnipers(b, mine, foes);
    return;
  }
  defend(b, side, style, mine, foes, stand);
  if (b.hide) counterSnipers(b, mine, foes);
  // A reserve sent in at a tank block goes round her too; the line's own tanks hold where they stand.
  if (b.measured) seekFlanks(b, side, mine.filter((u) => u.reserve === 'sent'));
  if (b.smoke) smokeIn(b, side, mine, foes);
}

/**
 * Smoke on call (release 2, ruling 242 S5-A: both staffs alike). A block of foot (or lorried infantry) going in at a
 * block of theirs, with smoke to lay and no screen of its own standing, lays one once its target is within its fire's
 * reach (LAND.reach): the attack's in its assault (an attack with a plan, once its main effort has gone in; the pinning
 * line too from then), the defence's in a counter-attack, a reserve sent forward; never a block standing in its works
 * (within LAND.hold.within of its place, as the drill's 'home'). So on the defence only a reserve sent forward lays
 * smoke: a line block gone out after a wavering enemy, or round a neighbour's flank, is never further from its place
 * than its leash (every style's defLeash, 300 m, LAND.hold.within's own), so it fights from its works. The screen
 * lies square to the way to its target, its front long, its middle min(its army's reach, half the gap) beyond the
 * middle of its front: across the ground it must cross. Ordered as every staff order is (no radio).
 */
function smokeIn(b: LandBattle, side: Side, mine: LandUnit[], foes: LandUnit[]): void {
  const P = b.plan[side];
  if (side === 'att' && P && P.went === null) return;
  for (const u of mine) {
    if (!hasTarget(u, foes) || (u.waits && !u.waits.free)) continue;
    if (side === 'def' && u.reserve !== 'sent' && dist(u.x, u.y, u.home[0], u.home[1]) < LAND.hold.within) continue;
    if (b.smokeWhy(u) !== null) continue;
    const t = foes.find((f) => f.id === u.order!.target)!, gap = b.gap(u, t);
    if (gap > reachOf(u)) continue;
    const m = b.smokeOf(u)!, k = Math.hypot(t.x - u.x, t.y - u.y);
    if (k < 1e-6) continue;
    const dx = (t.x - u.x) / k, dy = (t.y - u.y) / k, [fx, fy] = [Math.sin(u.face), Math.cos(u.face)];
    const on = Math.min(m.reach, gap / 2), mx = u.x + (fx * u.d) / 2 + dx * on, my = u.y + (fy * u.d) / 2 + dy * on, h = u.w / 2;
    b.command({ do: 'land:smoke', units: [u.id], x0: mx - dy * h, y0: my + dx * h, x1: mx + dy * h, y1: my - dx * h }, side);
  }
}

/**
 * Reconnaissance by fire (the historian's check, release 2; LAND.hide.search): once its attack has been ambushed, each
 * of its blocks with nothing in its sights sweeps the cover ahead in its lane (woods, or a hedge line), as far as
 * 'ahead' metres or its reach, with fire as it goes on ("shooting constantly at the hedgerows"): its search is the
 * sweep's middle and half its depth round it. Where there is no cover there, none. Never its guns far back.
 */
function searchCover(b: LandBattle, side: Side, mine: LandUnit[]): void {
  const F = LAND.hide.search, dir = side === 'att' ? 1 : -1, step = b.hedges.length ? 10 : b.field.cell;
  for (const u of mine) {
    // A block waiting for help (hold until relieved) is never its staff's to order, its fire no more than its feet.
    if (u.waits && !u.waits.free) continue;
    if ((u.kind === 'art' && !u.team) || u.team === 'snp' || u.engaged !== undefined || u.order?.kind === 'withdraw') { if (u.engaged === undefined) { delete u.search; delete u.searchR; } continue; }
    const ahead = Math.min(F.ahead, reachOf(u)), r = ahead / 2;
    const x = u.x, y = u.y + dir * (u.d / 2 + r);
    let cover = false;
    // (Out to the far edge of a block standing there: what it can strike.)
    for (let dy = -r; dy <= r + F.reach && !cover; dy += step) for (let dx = -F.lane; dx <= F.lane && !cover; dx += step) {
      const px = x + dx, py = y + dy;
      if (px >= 0 && py >= 0 && px < b.field.w && py < b.field.h && (coverAt(b.field, px, py) === 'woods' || b.onHedge(px, py))) cover = true;
    }
    if (cover) { u.search = [x, y]; u.searchR = Math.max(r, F.lane); } else { delete u.search; delete u.searchR; }
  }
}

/**
 * Measured armour (release 2; LAND.armour): a tank block sent at a tank block whose front its gun all but cannot get
 * through, where her side or rear it can, goes round to that side first, then in: the attack's, and the defence's
 * reserves sent in to strike back. Never onto close ground, where tanks keep with their foot (CLOSE).
 */
function seekFlanks(b: LandBattle, side: Side, mine: LandUnit[]): void {
  const A = LAND.armour, dir = side === 'att' ? 1 : -1;
  for (const u of mine) {
    if (u.kind !== 'arm' || u.order?.kind !== 'attack' || u.order.target === undefined || b.flanking.has(u.id)) continue;
    const t = b.unit(u.order.target);
    if (!t || t.kind !== 'arm' || !ok(t) || t.hid) continue;
    const front = b.armourAt(u, t, 'front', A.flankAt), sideW = b.armourAt(u, t, 'side', A.flankAt), rear = b.armourAt(u, t, 'rear', A.flankAt);
    if (front === null || sideW === null || rear === null || front >= A.flankBelow || Math.max(sideW, rear) < A.flankBetter * front) continue;
    // Round her nearer flank; past it, toward her rear, where the rear is the better face.
    const flank = Math.sign(u.x - t.x) || 1, deep = rear > sideW ? dir * (t.d / 2 + u.d / 2 + 60) : 0;
    const x = Math.min(b.field.w - u.w / 2 - 10, Math.max(u.w / 2 + 10, t.x + flank * (t.w / 2 + u.w / 2 + 80)));
    const y = Math.min(b.field.h - u.d / 2 - 10, Math.max(u.d / 2 + 10, t.y + deep));
    if (CLOSE.includes(groundAt(b.field, x, y))) continue;
    b.flanking.set(u.id, { target: t.id, at: [x, y], since: b.t });
    b.command({ do: 'move', units: [u.id], x, y }, side);
  }
}

/** The tank blocks still going round (seekFlanks): there, stopped, or too long about it, they go in at her. Their ids. */
function flanksNow(b: LandBattle, side: Side): Set<number> {
  const busy = new Set<number>();
  for (const [id, f] of b.flanking) {
    const u = b.unit(id), t = b.unit(f.target);
    if (!u || u.side !== side) continue;
    if (!ok(u) || u.taken || u.order?.kind === 'withdraw') { b.flanking.delete(id); continue; }
    if (!t || !ok(t)) { b.flanking.delete(id); continue; }
    if (!u.order || dist(u.x, u.y, f.at[0], f.at[1]) < 40 || b.t - f.since > LAND.armour.flankFor) {
      b.flanking.delete(id);
      by(b, side, u, t);
      continue;
    }
    busy.add(id);
  }
  return busy;
}

/**
 * Every block but its guns goes forward in its own lane, LAND.hide.staff.ahead at a time, while the attack sees none of
 * the enemy (all of them hidden in the woods): advance to contact. Its guns stand.
 */
function probe(b: LandBattle, side: Side, mine: LandUnit[]): void {
  const dir = side === 'att' ? 1 : -1, step = LAND.hide.staff.ahead;
  for (const u of mine) {
    if (u.kind === 'art' || u.order) continue;
    const y = Math.min(b.field.h - u.d / 2 - 10, Math.max(u.d / 2 + 10, u.y + dir * step));
    if (Math.abs(y - u.y) > 20) b.command({ do: 'move', units: [u.id], x: u.x, y }, side);
  }
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
 * A sniper team of the computer's (release 2, LAND.snipers; the design director's ruling of 25 November 16:30Z, Q6(3)),
 * never as a gun: slipping away, it goes to other hiding ground near by (sim slipSpot). It keeps LAND.snipers.keep metres
 * from every found enemy block, falling back toward its own line when one comes nearer. With nothing in its reach it
 * goes one bound to the next hiding ground toward the enemy, never ahead of its own leading foot; with anything in its
 * reach it stays where it is.
 */
function snipersUp(b: LandBattle, side: Side, u: LandUnit, mine: LandUnit[], foes: LandUnit[]): void {
  if (u.order?.kind === 'withdraw' || (u.waits && !u.waits.free)) return;
  const K = LAND.snipers.keep, dir = side === 'att' ? 1 : -1, near = foes.filter((f) => f.kind !== 'fort' && b.gap(u, f) < K);
  if (u.slipTo !== undefined) {
    if (u.order) return;
    const spot = b.slipSpot(u);
    if (spot) b.command({ do: 'move', units: [u.id], x: spot[0], y: spot[1] }, side);
    return;
  }
  if (near.length) {
    // Already going somewhere far enough from them: on its way.
    if (u.order?.kind === 'move' && !near.some((f) => dist(f.x, f.y, u.order!.x, u.order!.y) < K)) return;
    const spot = b.sniperSpot(u, [u.x, u.y - dir * 400], u.y);
    const gap = near.reduce((m, f) => Math.min(m, b.gap(u, f)), Infinity);
    const [x, y] = spot ?? [u.x, u.y - dir * (K - gap + 50)];
    b.command({ do: 'move', units: [u.id], x, y }, side);
    return;
  }
  if (u.order || foes.some((f) => b.gap(u, f) <= reachOf(u))) return;
  const foot = mine.filter((m) => !m.team && m.kind !== 'art' && m.kind !== 'fort');
  if (!foot.length || !foes.length) return;
  const front = dir > 0 ? Math.max(...foot.map((m) => m.y)) : Math.min(...foot.map((m) => m.y));
  const to = foes.reduce((m, f) => (b.gap(u, f) < b.gap(u, m) ? f : m), foes[0]);
  const spot = b.sniperSpot(u, [to.x, to.y], front);
  if (spot && dir * (spot[1] - u.y) > 20) b.command({ do: 'move', units: [u.id], x: spot[0], y: spot[1] }, side);
}

/**
 * The counter to a sniper team (Q6(4)): a found enemy sniper team, vague or located, within the reach of one of its
 * machine-gun or mortar teams gets one such team aimed at it, the one already on it or the nearest free. The aim holds
 * until the sniper team is hidden again, out of reach, falling back or broken, and wins over the main effort's for that
 * team (planned() aims first). Guns never leave the main effort for it.
 */
function counterSnipers(b: LandBattle, mine: LandUnit[], foes: LandUnit[]): void {
  const theirs = foes.filter((f) => f.team === 'snp' && f.order?.kind !== 'withdraw');
  for (const g of mine) {
    if (g.hunts === undefined) continue;
    const f = theirs.find((x) => x.id === g.hunts);
    if (f && g.order?.kind !== 'withdraw' && b.gap(g, f) <= reachOf(g)) continue;
    if (g.aim === g.hunts) delete g.aim;
    delete g.hunts;
  }
  for (const f of theirs) {
    let h = mine.find((g) => g.hunts === f.id);
    if (!h) {
      const can = mine.filter((g) => (g.team === 'mg' || g.team === 'mor') && g.hunts === undefined && g.order?.kind !== 'withdraw' && b.gap(g, f) <= reachOf(g));
      h = can.reduce<LandUnit | undefined>((m, g) => (!m || b.gap(g, f) < b.gap(m, f) ? g : m), undefined);
    }
    if (h) { h.hunts = f.id; h.aim = f.id; }
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
    if (u.team === 'snp') { snipersUp(b, side, u, mine, foes); continue; }
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

/**
 * Where a side's line is struck, hardest first (b407): its blocks in a fight, the most worn first. Its guns firing from
 * far back are not the line struck, as they are not contact (sim.ts contactAt).
 */
const struckOf = (mine: LandUnit[], foes: LandUnit[]): LandUnit[] => mine.filter((m) => m.kind !== 'art' && m.reserve !== 'held' && m.engaged !== undefined && foes.some((f) => f.id === m.engaged))
  .sort((a, c) => a.str / a.str0 - c.str / c.str0);

/**
 * The attack's corps come up from next door (neighbours join the fight, release 2): each with nothing in its sights is
 * sent, as the defence sends its reserve (b407), at the enemy its line is fighting where it is struck hardest. Until
 * the line is struck, its general's plan (or the plain general) gives it its lane. No attacking block is a reserve
 * otherwise, so where no corps joins nothing here moves.
 */
function sendOn(b: LandBattle, side: Side, mine: LandUnit[], foes: LandUnit[], skip: Set<number> | null = null): void {
  const sent = mine.filter((u) => u.reserve === 'sent' && u.kind !== 'art' && u.order?.kind !== 'withdraw' && !hasTarget(u, foes) && !skip?.has(u.id));
  if (!sent.length) return;
  const struck = struckOf(mine, foes);
  if (!struck.length) return;
  sent.forEach((u, i) => by(b, side, u, foes.find((f) => f.id === struck[i % struck.length].engaged)!));
}

/**
 * Hold until relieved (release 2; LandBattle.relief): once its line is struck, a staff tells the block struck hardest
 * that may wait (waitWhy) to dig in for a corps coming up from next door, as many as there are such corps not yet
 * waited for (one block each); and sends that corps' nearest block to each of its side's waiting blocks before anywhere
 * else: at the enemy the waiting block is fighting, or else to where it stands. A block of the player's that waits is
 * relieved so too wherever his staff thinks for the corps. Returns the blocks its other orders leave be: those waiting,
 * and those sent to them.
 */
function relieve(b: LandBattle, side: Side, mine: LandUnit[], foes: LandUnit[]): Set<number> {
  const skip = new Set<number>();
  for (const u of struckOf(mine, foes)) if (b.waitWhy(u) === null) b.command({ do: 'land:wait', units: [u.id] }, side);
  const waiting = b.units.filter((u) => u.side === side && ok(u) && u.waits && !u.waits.free).sort((a, c) => a.str / a.str0 - c.str / c.str0 || a.id - c.id);
  for (const w of waiting) {
    skip.add(w.id);
    const near = (r: LandUnit): number => dist(r.x, r.y, w.x, w.y);
    // (Never a block that is itself waiting: its staff may not order it, and it would hold the help back.)
    const rs = mine.filter((r) => r.corps === w.waits!.corps && r.joined !== undefined && r.kind !== 'art' && r.order?.kind !== 'withdraw' && !(r.waits && !r.waits.free) && !skip.has(r.id));
    if (!rs.length) continue;
    const r = rs.reduce((m, x) => (near(x) < near(m) - 1e-9 || (Math.abs(near(x) - near(m)) <= 1e-9 && x.id < m.id) ? x : m));
    skip.add(r.id);
    const foe = w.engaged !== undefined ? foes.find((f) => f.id === w.engaged) : undefined;
    if (foe) { if (!(r.order?.kind === 'attack' && r.order.target === foe.id)) by(b, side, r, foe); }
    else if (!(r.order?.kind === 'move' && dist(r.order.x, r.order.y, w.x, w.y) <= LAND.alone.near)) b.command({ do: 'move', units: [r.id], x: w.x, y: w.y }, side);
  }
  return skip;
}

/** A block's weight now: what it strikes with, as its blows count it. */
const weight = (u: LandUnit): number => u.power0 * (0.35 + 0.65 * (u.str / u.str0));

/** Sets the attack's plan at its general's first thought (styles.ts: the numbers). */
function makePlan(b: LandBattle, side: Side, s: Style, mine: LandUnit[], foes: LandUnit[]): AttackPlan {
  const P: AttackPlan = { phase: s.plan ? 'guns' : 'assault', main: [], startY: null, role: {}, group: {}, spot: {}, arrived: null, went: null, hook: {} };
  // (A corps from next door is no part of the plan: it comes up as a reserve.)
  const rest = mine.filter((u) => u.kind !== 'art' && u.reserve !== 'sent');
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
  // (a sniper team is never a gun: never aimed, never waited under: snipersUp)
  const guns = mine.filter((u) => u.kind === 'art' && u.team !== 'snp'), rest = mine.filter((u) => u.kind !== 'art');
  for (const u of mine) if (u.team === 'snp') snipersUp(b, side, u, mine, foes);
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
function defend(b: LandBattle, side: Side, style: Style | null, mine: LandUnit[], foes: LandUnit[], skip: Set<number> | null = null): void {
  const leash = style?.defLeash ?? 300, finish = style?.defFinish ?? 150;
  const edge = b.field.streetEdge, cell = b.field.cell;
  const outside = (f: LandUnit): boolean => {
    if (!edge) return false;
    const y = edge[Math.min(edge.length - 1, Math.max(0, Math.floor(f.x / cell)))];
    return y < 0 || f.y < y;
  };
  // Where the line is struck, hardest first: its blocks in a fight, the most worn first (b407). Its guns firing from
  // far back are not the line struck, as they are not contact (sim.ts contactAt).
  const struck = struckOf(mine, foes);
  let next = 0;
  const sendTo = (u: LandUnit): void => {
    const at = struck[next++ % struck.length];
    u.reserve = 'sent';
    by(b, side, u, foes.find((f) => f.id === at.engaged)!);
  };
  // Bold, while the line is not yet struck: the held block walks along its own row toward the stretch the attack is
  // coming at (the design director, ruling 239's rider, 03:20Z). It strikes nothing on the way and is sent, as every
  // reserve is, once the line is struck. A methodical or mass defence's stands in the middle until then.
  // Night patrol (release 2): alerted by the patrol it caught, the computer's defence of any style walks it so at once,
  // toward the attack wherever it comes on (the design director, 18:36Z, Q7(b)); the player's moves his own.
  const shift = b.alertedStaff(side) ? Infinity : style?.defShift;
  const near = !struck.length && shift ? coming(b, mine, foes, shift) : null;
  for (const u of mine) {
    if (u.team === 'snp' && !skip?.has(u.id)) { snipersUp(b, side, u, mine, foes); continue; }
    if (u.kind === 'art' || u.order?.kind === 'withdraw' || skip?.has(u.id)) continue;
    // Hidden in the woods (release 2): its line keeps there, holding its fire, until the enemy finds it.
    if (u.hid && u.reserve !== 'held') continue;
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
    // (A team firing from far back is no neighbour in a fight (firesFromBack): what it fires on has not come round
    // anyone's flank, and a block that stepped out for it only opened the line. Sniper pairs, release 2: none exists
    // unless sniper teams are on, so with them off this reads as it did.)
    const round = foes.find((f) => dist(f.x, f.y, u.home[0], u.home[1]) < leash && !outside(f) && mine.some((m) => !firesFromBack(b, m) && m.engaged === f.id && m !== u));
    if (round) by(b, side, u, round);
  }
}
