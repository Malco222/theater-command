// A fleet action fought out on the field: the sea's BattleSim (src/battle/core/types.ts).
//
// Pieces steam, turn in arcs and carry their way; each wears down the enemy piece it is firing on, while it is within
// its reach, at a rate set by its weight and by the angle its guns bear at (a ship broadside-on brings every turret to
// bear, a ship end-on only some: crossing the T). Wear takes a piece's hull and
// shakes its nerve; a ship goes down when its share of the hull is gone, and a side breaks off when it has lost
// about a fifth of its weight. Carriers fight by strikes and the coasts' aircraft by raids, both counted, not
// flown (ruling 217). Everything is abstract by design (project memory air-battle-paused): nothing is aimed and
// nothing flies.
//
// The first slice's orders (ruling 217): course (move, line), attack, speed, withdraw (one piece, or the whole
// fleet), and the carrier strike. Pieces with no order fight at will, as their captains would: the player steps in
// where he wants a different target, a different course, or the fight broken off. Formations, the torpedo run and
// smoke wait for the polish pass (TODO(polish)).
//
// Same ticket, same orders, same battle: every roll comes from the battle's own seeded stream.

import { Rng } from '../../core/rng';
import { bearing, clamp, diceNow, dist, lineUp, turn, DT } from '../core';
import type { BattleSim, BattleView, Command, FieldEvent, FieldResult, Side, Zone } from '../core/types';
import { DICE, RULES_CLASSES, SEA } from './numbers';
import { drawUp, type PieceInfo } from './pieces';
import { SEA_CUES } from './cues';
import { none, seaLevel, seaRatio, shipsIn, shipWords, sunkCaps } from './result';
import type { SeaClass, SeaCommand, SeaOrder, SeaShips, SeaSpeed, SeaTicket, SeaUnit } from './types';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
/** Pieces that still answer orders and fight as told: not running for home. */
const steady = (u: SeaUnit): boolean => u.state === 'steady' || u.state === 'wavering';

/**
 * The enemy piece nearest a point, for one side. Two pieces equally near (within a metre) go to the one on the side's
 * own left as it drew up, never to whichever the arithmetic happens to round toward: so either side, turned half round,
 * makes the same choice. Fleets draw up in rows centred on the field, so the enemy nearest the middle is often one of
 * two at the same distance; broken by rounding, one side's ships turned to port and the other's to starboard, and the
 * same two fleets won 12 points more often attacking than defending (b407).
 */
export function nearestEnemy(units: readonly SeaUnit[], side: Side, x: number, y: number): SeaUnit | null {
  const f0 = side === 'att' ? 0 : Math.PI, c = Math.cos(f0), s = Math.sin(f0);
  let best: SeaUnit | null = null, bd = Infinity, bl = Infinity;
  for (const e of units) {
    if (e.side === side || !live(e)) continue;
    const d = Math.hypot(e.x - x, e.y - y), lat = (e.x - x) * c - (e.y - y) * s;
    if (d < bd - 1 || (d <= bd + 1 && lat < bl)) { best = e; bd = Math.min(bd, d); bl = lat; }
  }
  return best;
}

/** The enemy piece nearest the middle of a side's own pieces still on the field: the newcomer's first target, and the
 *  staff's (ai.common). */
export function enemyNearestMiddle(units: readonly SeaUnit[], side: Side): SeaUnit | null {
  const mine = units.filter((u) => u.side === side && live(u));
  if (!mine.length) return null;
  const cx = mine.reduce((n, u) => n + u.x, 0) / mine.length, cy = mine.reduce((n, u) => n + u.y, 0) / mine.length;
  return nearestEnemy(units, side, cx, cy);
}

interface Strike { at: number; side: Side; target: number; weight: number; from: string; count: number }

/** Each fleet's ships lost on the field, by class: sunk, and gone out of the fight burning (past the dice's limit). */
export interface SeaLosses { sunk: Record<number, SeaShips>; burnt: Record<number, SeaShips>; toDice?: boolean }

export class SeaSim implements BattleSim<SeaUnit> {
  readonly domain = 'sea' as const;
  readonly ticket: SeaTicket;
  t = 0;
  units: SeaUnit[];
  over: FieldResult | null = null;
  ai: Record<Side, boolean>;

  private readonly info: Map<number, PieceInfo>;
  private readonly byId = new Map<number, SeaUnit>();
  private readonly orders = new Map<number, SeaOrder>();
  private readonly rng: Rng;
  private events: FieldEvent[] = [];
  private zones: Zone[] = [];
  private readonly caps: Record<Side, SeaShips>;
  private readonly lostShips: Record<Side, SeaShips> = { att: none(), def: none() };
  private readonly weight0: Record<Side, number> = { att: 0, def: 0 };
  private readonly breakAt: Record<Side, number>;
  /** The side that handed the action to the staff, if one did. */
  private handedBy: Side | null = null;
  /** The luck of the day, the rules' own run of the dice (0.8 to 1.2): the attacker fights at it and the defender at its inverse. */
  readonly luck: number;
  private broken: Side | null = null;
  private brokeAt = 0;
  /** Battle time the guns first reached an enemy, null before it (ruling 226(1): the screens run fast until then). */
  private contactAt: number | null = null;
  /** Each fleet has been said to be near breaking off (ruling 226(3)): once a side. */
  private warned: Record<Side, boolean> = { att: false, def: false };
  /** Battle time of the coasts' last raid. */
  private lastAir = -Infinity;
  private readonly lastHit = new Map<number, number>();
  private readonly look = new Map<number, number>();
  /** Which way round each piece turns to bring its broadside to bear: kept, so that it does not dither. */
  private readonly hand = new Map<number, number>();
  private strikes: Strike[] = [];
  private readonly nextAir: Record<Side, number>;
  /** Wear each piece has dealt by gunfire, and seconds it has spent with a target in reach, by unit id: see tally. */
  private readonly dealt: number[] = [];
  private readonly inReach: number[] = [];
  /** How hard each piece is turning, as a share of its helm at full way: for 'turn' in the models' act. */
  private readonly swing: number[] = [];
  /** Wear dealt by strikes ('att:cv') and by the coasts' aircraft ('att:air'). */
  private readonly fromAir: Record<string, number> = {};
  /** Wear dealt by each side's classes ('att:bb') over the battle, and seconds each spent in reach ('att:bb:s'): for tuning, read by tools and tests only. */
  get tally(): Record<string, number> {
    const out: Record<string, number> = { ...this.fromAir };
    for (const u of this.units) {
      const k = `${u.side}:${u.extra.cls}`;
      out[k] = (out[k] ?? 0) + this.dealt[u.id];
      out[`${k}:s`] = (out[`${k}:s`] ?? 0) + this.inReach[u.id];
    }
    return out;
  }
  private readonly fleetLoss: { sunk: Record<number, SeaShips>; burnt: Record<number, SeaShips> } = { sunk: {}, burnt: {} };

  constructor(ticket: SeaTicket) {
    this.ticket = ticket;
    this.rng = new Rng(ticket.seed);
    const { units, info } = drawUp(ticket.place.fleets);
    this.units = units;
    this.info = info;
    for (const u of units) {
      this.byId.set(u.id, u);
      this.dealt[u.id] = 0;
      this.inReach[u.id] = 0;
      this.swing[u.id] = 0;
      this.weight0[u.side] += this.weightOf(u);
    }
    for (const f of ticket.place.fleets) { this.fleetLoss.sunk[f.id] = none(); this.fleetLoss.burnt[f.id] = none(); }
    this.caps = sunkCaps(ticket.place.fleets, ticket.reckoning.odds);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    // Each side's captains know their common target from the start: the enemy nearest the middle of their fleet as it is drawn up.
    for (const side of ['att', 'def'] as Side[]) this.focus[side] = this.nearestToMiddle(side);
    const S = SEA.side;
    this.luck = this.rng.range(DICE[0], DICE[1]);
    const adm = (side: Side): number => Math.max(0, ...ticket.place.fleets.filter((f) => f.side === side).map((f) => f.admiral?.level ?? 0));
    this.breakAt = {
      att: S.breakAt + S.admiral * adm('att') + S.spread * (this.rng.next() * 2 - 1),
      def: S.breakAt + S.admiral * adm('def') + S.spread * (this.rng.next() * 2 - 1),
    };
    // The coasts' first raids come in at a random moment of the first interval, so the two sides' do not land together.
    this.nextAir = { att: this.rng.range(0.2, 0.8) * SEA.air.every, def: this.rng.range(0.2, 0.8) * SEA.air.every };
  }

  // ---- What a piece is worth ---------------------------------------------------------------------------------

  /** How a side's ships fight today: the luck of the day for the attacker, its inverse for the defender. */
  private day(side: Side): number { return side === 'att' ? this.luck : 1 / this.luck; }
  private ships(u: SeaUnit): number { return u.extra.ships; }
  /** Its weight still in the fight: whole ships at full weight down to 'damaged' for a wreck. */
  private output(u: SeaUnit): number {
    const p = this.info.get(u.id)!, n0 = u.extra.ships0;
    const afloat = this.ships(u);
    if (afloat <= 0) return 0;
    return p.weight * (afloat * SEA.fire.damaged + (1 - SEA.fire.damaged) * u.str * n0);
  }
  /** Its weight as it stands: what is left of its hull. */
  private weightOf(u: SeaUnit): number { return this.info.get(u.id)!.weight * u.extra.ships0 * (live(u) || u.state === 'withdrawn' ? u.str : 0); }
  private hullOf(u: SeaUnit): number { const p = this.info.get(u.id)!; return p.hull * u.extra.ships0; }

  /** The order a piece is under, for the screens (undefined: it fights at will). */
  orderOf(id: number): SeaOrder | undefined { return this.orders.get(id); }

  /** The share of a side's starting weight it has lost: hull worn away, on pieces still there or gone. */
  lostShare(side: Side): number {
    let left = 0;
    for (const u of this.units) if (u.side === side) left += this.info.get(u.id)!.weight * u.extra.ships0 * u.str;
    return this.weight0[side] > 0 ? 1 - left / this.weight0[side] : 0;
  }

  /** How near a side is to breaking off, from 0 to 1: what it has lost against what it breaks at (ruling 226, 3: no
   *  hidden thresholds, so the screens show this as it is). */
  nearBreak(side: Side): number {
    if (this.broken === side) return 1;
    return Math.max(0, Math.min(1, this.lostShare(side) / this.breakAt[side]));
  }

  /** Whether any shot has told yet on either side. */
  fired(): boolean { return this.lostShare('att') + this.lostShare('def') > 0; }

  /** The staff's chance for the attacker were the rest handed to them now: the dice's run at the ratio then standing,
   *  as the staff reckon it (the day's own luck, which settles it, is not theirs to know). */
  handChance(): number {
    const k = this.ticket.reckoning, [d0, d1] = k.dice;
    if (k.odds * d1 < 1) return 0;
    if (k.odds * d0 >= 1) return 1;
    const now = (k.odds * Math.max(1e-6, 1 - this.lostShare('att'))) / Math.max(1e-6, 1 - this.lostShare('def'));
    return clamp((d1 - 1 / now) / (d1 - d0), 0, 1);
  }

  // ---- Orders ------------------------------------------------------------------------------------------------

  command(c: Command | SeaCommand, by: Side): boolean {
    if (this.over) return false;
    const chasing = this.broken !== null && this.broken !== by;
    const ok = this.order(c, by);
    // Once the enemy has broken off, a side's ships run it down at full speed, the player's as the staff's, until the
    // player gives an order of his own: then they do as told (b402: the newcomer's chase is the staff's).
    if (ok && chasing && c.do !== 'dice' && c.do !== 'auto') this.chaseTaken[by] = true;
    return ok;
  }

  /** The player's sides take the chase in hand with their first order after the enemy breaks off. */
  private readonly chaseTaken: Record<Side, boolean> = { att: false, def: false };

  private order(c: Command | SeaCommand, by: Side): boolean {
    const mine = (ids: number[] | undefined): SeaUnit[] => (ids ?? []).map((id) => this.byId.get(id)).filter((u): u is SeaUnit => !!u && u.side === by && steady(u));
    switch (c.do) {
      case 'move': {
        const us = mine(c.units);
        if (!us.length) return false;
        if (us.length === 1) this.orders.set(us[0].id, { kind: 'move', x: c.x, y: c.y, face: c.face });
        else for (const s of lineUp(us, by, c.x, c.y, c.x, c.y, 600)) this.orders.set(s.id, { kind: 'move', x: s.x, y: s.y, face: c.face ?? s.face });
        return true;
      }
      case 'line': {
        const us = mine(c.units);
        if (!us.length) return false;
        for (const s of lineUp(us, by, c.x0, c.y0, c.x1, c.y1, 600)) this.orders.set(s.id, { kind: 'move', x: s.x, y: s.y, face: s.face });
        return true;
      }
      case 'attack': {
        const us = mine(c.units), foe = this.byId.get(c.target);
        if (!us.length || !foe || foe.side === by || !live(foe)) return false;
        for (const u of us) {
          // A carrier attacks with its strike: the order is the strike, flown at the next look round (within a
          // second) whenever its aircraft are ready, the same second the staff's carrier would fly it (b402).
          this.orders.set(u.id, { kind: 'attack', target: foe.id });
          if (u.extra.cls !== 'cv') u.extra.target = foe.id;
        }
        // The target the player names is the fleet's common target too, for every ship that can reach it.
        this.focus[by] = foe.id;
        return true;
      }
      case 'hold': {
        const us = mine(c.units);
        for (const u of us) this.orders.set(u.id, { kind: 'hold' });
        return us.length > 0;
      }
      case 'withdraw': {
        const us = mine(c.units);
        for (const u of us) { this.orders.set(u.id, { kind: 'leave' }); u.extra.speed = 'flank'; }
        return us.length > 0;
      }
      case 'withdrawAll': {
        if (c.side !== by || this.broken) return false;
        this.breakSide(by, true);
        return true;
      }
      case 'ability': {
        if (c.key !== 'strike') return false;
        const foe = c.target !== undefined ? this.byId.get(c.target) : undefined;
        if (!foe || foe.side === by || !live(foe)) return false;
        let any = false;
        for (const u of mine(c.units)) if (u.extra.cls === 'cv') { this.orders.set(u.id, { kind: 'attack', target: foe.id }); any = this.strike(u, foe) || any; }
        return any;
      }
      case 'auto': {
        this.ai[c.side] = c.on;
        return true;
      }
      case 'sea:speed': {
        const sc = c as { units: number[]; speed: SeaSpeed };
        const us = mine(sc.units);
        if (!us.length || !(sc.speed in SEA.speeds)) return false;
        for (const u of us) u.extra.speed = sc.speed;
        return true;
      }
      case 'dice': {
        // One press hands the rest back to the dice, at the ratio then standing (ruling 217): on screen, HAND IT TO THE STAFF (log b348).
        // The roll is the day's own luck, the rules' dice already thrown: pressed before a shot is fired, it books exactly
        // what the campaign would have rolled, so handing it over is never worse than not fighting at all.
        if (this.over) return false;
        this.handedBy = by;
        const k = this.ticket.reckoning;
        const R = diceNow(k.odds, k.dice, { att: 1 - this.lostShare('att'), def: 1 - this.lostShare('def') }, (this.luck - k.dice[0]) / (k.dice[1] - k.dice[0]));
        this.finish(R >= 1 ? 'att' : 'def', 'time', true, R);
        return true;
      }
      default:
        return false;
    }
  }

  /** Orders a carrier's strike on an enemy piece. False if its aircraft are not ready. Counted, not flown. */
  private strike(cv: SeaUnit, foe: SeaUnit): boolean {
    const ab = cv.abilities?.find((a) => a.key === 'strike');
    if (!ab || ab.ready > 0 || ab.winding || !steady(cv)) return false;
    const p = this.info.get(cv.id)!;
    const weight = SEA.fire.rate * this.output(cv) * SEA.strike.share * SEA.strike.every * this.day(cv.side);
    const perCarrier = p.fleet.ships.cv > 0 ? Math.round(p.fleet.bombers / p.fleet.ships.cv) : 0;
    const count = Math.round(perCarrier * (this.output(cv) / (p.weight * cv.extra.ships0)));
    ab.winding = true;
    ab.ready = SEA.strike.every;
    this.strikes.push({ at: this.t + SEA.strike.windup, side: cv.side, target: foe.id, weight, from: cv.extra.fleet, count });
    this.say({ what: 'ability', unit: cv.id, side: cv.side, text: `${this.told(cv, true)} flies off ${count ? `${count} bombers` : 'its aircraft'} against ${this.told(foe)}.`, extra: { key: 'strike', target: foe.id, count } });
    return true;
  }

  // ---- The tick -------------------------------------------------------------------------------------------------

  step(): void {
    if (this.over) return;
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    const tick = Math.round(this.t / DT);
    for (const u of this.units) if (live(u)) this.steer(u);
    if (tick % 10 === 0) this.decide();
    for (const u of this.units) { u.engaged = undefined; if (live(u)) this.fire(u); }
    for (const u of this.units) this.pose(u);
    this.landStrikes();
    this.coastAir();
    for (const u of this.units) if (live(u)) this.nerve(u);
    for (const u of this.units) for (const a of u.abilities ?? []) if (a.ready > 0) a.ready = Math.max(0, Math.round((a.ready - DT) * 1000) / 1000);
    this.zones = this.zones.filter((z) => z.until > this.t);
    this.judge();
  }

  /** Once a second: targets, carriers' strikes, and the side's own reckoning. */
  private decide(): void {
    // A common target that has gone or broken off gives way to the enemy now nearest the middle of the fleet.
    for (const side of ['att', 'def'] as Side[]) {
      const f = this.byId.get(this.focus[side]);
      if (!f || !live(f) || f.state === 'routing') this.focus[side] = this.nearestToMiddle(side);
    }
    for (const u of this.units) {
      if (!live(u)) continue;
      const o = this.orders.get(u.id);
      if (o?.kind === 'attack') {
        const foe = this.byId.get(o.target);
        // An order to attack lapses when its target is gone or has broken off: the ship goes back to fighting at will
        // rather than chasing a beaten enemy across the sea.
        if (!foe || !live(foe) || foe.state === 'routing') this.orders.delete(u.id);
      }
      // A carrier with no strike ordered sends one whenever its aircraft are ready, at its fleet's common target, so a
      // newcomer's one order and the staff's play strike the same ship (b402).
      if (u.extra.cls === 'cv' && steady(u)) {
        // The order as it stands now: one that has just lapsed falls through to the common target, as at-will play does.
        const still = this.orders.get(u.id);
        const ordered = still?.kind === 'attack' ? this.byId.get(still.target) : undefined;
        const common = this.byId.get(this.focus[u.side]);
        const foe = ordered && live(ordered) ? ordered : common && live(common) ? common : this.heaviest(other(u.side));
        if (foe) this.strike(u, foe);
      }
      if ((this.look.get(u.id) ?? 0) <= this.t) { this.look.set(u.id, this.t + SEA.fire.retarget); this.aim(u); }
    }
  }

  /** Each side's common target: the enemy piece nearest the middle of its own fleet, kept until it goes or breaks
   *  off, or the one the player names. Every ship that can reach it fires on it: the staff's play is exactly the
   *  newcomer's (select all, right-click the enemy nearest the middle), so the two give the same battle (b402). */
  private readonly focus: Record<Side, number> = { att: -1, def: -1 };
  private nearestToMiddle(side: Side): number {
    // An enemy breaking off is no longer the fleet's target while any other is still fighting (only in the chase is
    // every enemy breaking off). All of the side's own pieces stay in the list, so its middle is where it was.
    const fighting = this.units.filter((u) => u.side === side || u.state !== 'routing');
    return (enemyNearestMiddle(fighting, side) ?? enemyNearestMiddle(this.units, side))?.id ?? -1;
  }

  /** The enemy piece a piece fires on: the one it was told to attack if it can reach it, else its fleet's common target
   *  if it can reach that, else one in reach. */
  private aim(u: SeaUnit): void {
    const o = this.orders.get(u.id);
    const C = SEA.classes[u.extra.cls as SeaClass];
    const told = o?.kind === 'attack' ? o.target : -1;
    if (told >= 0) {
      const foe = this.byId.get(told);
      if (foe && live(foe) && dist(u.x, u.y, foe.x, foe.y) <= C.reach) { u.extra.target = foe.id; return; }
    }
    {
      const foe = this.byId.get(this.focus[u.side]);
      if (foe && live(foe) && dist(u.x, u.y, foe.x, foe.y) <= C.reach) { u.extra.target = foe.id; return; }
    }
    let total = 0;
    const near: [SeaUnit, number][] = [];
    for (const e of this.units) {
      if (e.side === u.side || !live(e)) continue;
      const d = dist(u.x, u.y, e.x, e.y);
      if (d > C.reach) continue;
      // Any ship in reach is as good a target as another by its weight (the field counts no class twice, b415): only how
      // well the guns tell at that range sets the odds of picking it.
      const w = this.reachFactor(C, d);
      near.push([e, w]);
      total += w;
    }
    if (!near.length) { u.extra.target = -1; return; }
    // Keep the current target if it is still in reach most of the time: ships hold a target, they do not flit.
    const cur = near.find(([e]) => e.id === u.extra.target);
    if (cur && this.rng.chance(0.7)) return;
    let r = this.rng.next() * total;
    for (const [e, w] of near) { r -= w; if (r <= 0) { u.extra.target = e.id; return; } }
    u.extra.target = near[near.length - 1][0].id;
  }

  /** How much of a piece's fire tells at this range: all of it within its best range, falling to fire.far at its reach
   *  (fire.far is 1: the guns tell as hard anywhere in reach, b415). */
  private reachFactor(C: { reach: number; best: number }, d: number): number {
    return d <= C.best ? 1 : SEA.fire.far + (1 - SEA.fire.far) * (C.reach - d) / (C.reach - C.best);
  }

  private heaviest(side: Side): SeaUnit | null {
    let best: SeaUnit | null = null, bw = 0;
    for (const e of this.units) if (e.side === side && live(e)) { const w = this.output(e); if (w > bw) { bw = w; best = e; } }
    return best;
  }

  /** The enemy a piece with no order steers by: the nearest, looked for again once a second. */
  private readonly foeOf = new Map<number, { t: number; id: number }>();
  private steerFoe(u: SeaUnit): SeaUnit | null {
    const k = this.foeOf.get(u.id);
    const known = k ? this.byId.get(k.id) : undefined;
    if (k && known && live(known) && this.t - k.t < 1) return known;
    const foe = this.nearestFoe(u);
    this.foeOf.set(u.id, { t: this.t, id: foe ? foe.id : -1 });
    return foe;
  }

  private nearestFoe(u: SeaUnit): SeaUnit | null { return nearestEnemy(this.units, u.side, u.x, u.y); }

  // ---- Ship handling -------------------------------------------------------------------------------------------

  private steer(u: SeaUnit): void {
    const C = SEA.classes[u.extra.cls as SeaClass], F = SEA.field;
    const o = this.orders.get(u.id) ?? FREE;
    let want = u.face, speed = C.speed * SEA.speeds[u.extra.speed];
    const home = u.side === 'att' ? Math.PI : 0;
    if (u.state === 'routing' || o.kind === 'leave') {
      want = home;
      speed = C.speed * SEA.speeds.flank;
    } else if (o.kind === 'move') {
      const d = dist(u.x, u.y, o.x, o.y);
      if (d > 200) want = bearing(u.x, u.y, o.x, o.y);
      else { want = o.face ?? u.face; speed = 0; }
    } else if (o.kind === 'hold') {
      speed = 0;
    } else {
      // An order to attack sets what the guns fire on whenever it is in reach. The ship keeps with the fight as its
      // captain would, engaging the enemy nearest it, and steers for the target only when that lies well beyond the
      // fight: sending every ship at one point drew the line into a knot the enemy fought around (ruling 226, 4).
      // A ship fighting at will steers the same way for its fleet's common target: ships far from the fight close on
      // it, as the land thread found idle reserves must (b407).
      const tgt = this.byId.get(o.kind === 'attack' ? o.target : this.focus[u.side]) ?? null;
      const near = this.steerFoe(u);
      // A carrier keeps its standoff from the nearest enemy whatever it strikes: its aircraft reach any target.
      const foe = u.extra.cls !== 'cv' && tgt && (!near || dist(u.x, u.y, tgt.x, tgt.y) > dist(u.x, u.y, near.x, near.y) + SEA.ai.beyond) ? tgt : near;
      if (foe) want = this.engage(u, foe);
      // A carrier group that sees a battle line coming turns away and opens the range while it launches (b389), an
      // attack order or not. Its screen turns with it only within evade.screen metres of a carrier, and that is 0: the
      // screen stays and fights (b402: a screen that ran with its carriers left the plain order short of the card).
      // Only an order to steer, hold or withdraw takes a ship away.
      const away = this.groupEvasion(u, false);
      if (away) { want = away.course; speed = Math.min(C.speed * SEA.speeds.flank, away.speed); }
      // A beaten enemy is run down at full speed while there is time.
      // The player's ships too, until the player gives an order of his own (b402: the newcomer's chase is the staff's).
      if (this.broken && this.broken !== u.side && !this.chaseTaken[u.side]) speed = C.speed * SEA.speeds.flank;
    }
    // Stay on the field unless leaving it.
    if (u.state !== 'routing' && o.kind !== 'leave') {
      const m = 800;
      if (u.x < m || u.x > F.size - m || u.y < m || u.y > F.size - m) want = bearing(u.x, u.y, F.size / 2, F.size / 2);
    }
    // Way is gained and lost slowly; the helm answers better with way on.
    const acc = SEA.speeds.accel * DT;
    u.extra.way = u.extra.way < speed ? Math.min(speed, u.extra.way + acc) : Math.max(speed, u.extra.way - acc);
    const helm = C.turn * (0.25 + 0.75 * Math.min(1, u.extra.way / C.speed)) * DT;
    const swing = clamp(turn(u.face, want), -helm, helm);
    this.swing[u.id] = helm > 0 ? Math.abs(swing) / (C.turn * DT) : 0;
    u.face = norm(u.face + swing);
    u.x += Math.sin(u.face) * u.extra.way * DT;
    u.y += Math.cos(u.face) * u.extra.way * DT;
    // Out by its own edge: gone home, to fight another day.
    if ((u.state === 'routing' || o.kind === 'leave') && (u.side === 'att' ? u.y < 300 : u.y > F.size - 300)) {
      u.state = 'withdrawn';
      this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'have broken off and make' : 'has broken off and makes'} for port.` });
    }
  }

  /** The course and speed of a carrier group turning away from a battle line, for a carrier or a ship of its screen
   *  (within 'screen' metres of a carrier of its own side), or null when no battle line is near. */
  private groupEvasion(u: SeaUnit, ordered: boolean): { course: number; speed: number } | null {
    // A fleet with a battle line of its own stands and fights, its carriers behind its line: only a carrier group
    // without battleships turns away (a mixed fleet split itself running).
    if (this.units.some((f) => f.side === u.side && f.extra.cls === 'bb' && live(f) && f.state !== 'routing')) return null;
    const speed = SEA.classes.cv.speed * SEA.evade.speed;
    if (u.extra.cls === 'cv') { const h = this.evasion(u); return h === null ? null : { course: h, speed }; }
    if (ordered || !SEA.evade.screen) return null;
    let cv: SeaUnit | null = null, cd = SEA.evade.screen;
    for (const c of this.units) {
      if (c.side !== u.side || c.extra.cls !== 'cv' || !live(c) || c.state === 'routing') continue;
      const d = dist(u.x, u.y, c.x, c.y);
      if (d < cd) { cd = d; cv = c; }
    }
    if (!cv) return null;
    const h = this.evasion(cv);
    return h === null ? null : { course: h, speed };
  }

  /** The course a carrier takes away from the nearest enemy battleship within sight, or null when none is near. Along
   *  the edge of the field when straight away would run it off. */
  private evasion(u: SeaUnit): number | null {
    let bb: SeaUnit | null = null, bd = SEA.evade.see;
    for (const e of this.units) {
      if (e.side === u.side || e.extra.cls !== 'bb' || !live(e) || e.state === 'routing') continue;
      const d = dist(u.x, u.y, e.x, e.y);
      if (d < bd) { bd = d; bb = e; }
    }
    if (!bb) return null;
    const a = bearing(bb.x, bb.y, u.x, u.y), F = SEA.field, m = 1200, ahead = 2000;
    for (const k of [0, 0.5, -0.5, 1, -1, 1.5, -1.5]) {
      const h = norm(a + k), x = u.x + Math.sin(h) * ahead, y = u.y + Math.cos(h) * ahead;
      if (x > m && x < F.size - m && y > m && y < F.size - m) return h;
    }
    return norm(a + Math.PI / 2);
  }

  /** The course that brings a piece to its best range with its broadside to the enemy. */
  private engage(u: SeaUnit, foe: SeaUnit): number {
    const C = SEA.classes[u.extra.cls as SeaClass];
    const d = dist(u.x, u.y, foe.x, foe.y), b = bearing(u.x, u.y, foe.x, foe.y);
    const want = u.extra.cls === 'cv' ? (C.standoff ?? 2500) : C.best;
    let off: number;
    // A carrier keeps behind its screen, out of the destroyers' reach, and its aircraft do its fighting.
    if (u.extra.cls === 'cv') off = d < want ? 2.6 : d > want * 1.3 ? 0.6 : Math.PI / 2;
    else if (d > C.reach) off = 0.15;
    else if (d > want * 1.15) off = 0.6;
    else if (d < want * 0.75) off = 2.1;
    else off = Math.PI / 2;
    // Turn whichever way round is shorter, and keep to it unless the other way becomes much shorter.
    let h = this.hand.get(u.id);
    const a = Math.abs(turn(u.face, b + off)), z = Math.abs(turn(u.face, b - off));
    // Dead ahead, the two ways round are the same length: both sides then turn the same way round (+1), whatever the
    // rounding of their bearings, so neither side's line crosses the other's by the arithmetic.
    if (h === undefined || (h > 0 ? z + 0.8 < a : a + 0.8 < z)) h = a <= z + 1e-6 ? 1 : -1;
    this.hand.set(u.id, h);
    let want2 = norm(b + h * off);
    // A ship that has strayed far from its own fleet works back toward it: fleets fight together, not ship by ship.
    const [cx, cy] = this.centre(u.side), gap = dist(u.x, u.y, cx, cy);
    if (gap > SEA.ai.keep) {
      const pull = Math.min(0.6, (gap - SEA.ai.keep) / SEA.ai.keep);
      want2 = norm(want2 + pull * turn(want2, bearing(u.x, u.y, cx, cy)));
    }
    return want2;
  }

  /** The middle of a side's pieces still in the fight, weighted by their weight; kept for one tick. */
  private centres: { t: number; at: Record<Side, [number, number]> } = { t: -1, at: { att: [0, 0], def: [0, 0] } };
  private centre(side: Side): [number, number] {
    if (this.centres.t !== this.t) {
      const at: Record<Side, [number, number]> = { att: [0, 0], def: [0, 0] }, w: Record<Side, number> = { att: 0, def: 0 };
      for (const u of this.units) if (live(u)) { const k = this.weightOf(u) + 1e-6; at[u.side][0] += u.x * k; at[u.side][1] += u.y * k; w[u.side] += k; }
      for (const sd of ['att', 'def'] as Side[]) if (w[sd] > 0) at[sd] = [at[sd][0] / w[sd], at[sd][1] / w[sd]];
      this.centres = { t: this.t, at };
    }
    return this.centres.at[side];
  }

  // ---- Fire and wear -------------------------------------------------------------------------------------------

  private fire(u: SeaUnit): void {
    if (u.extra.cls === 'cv' || u.extra.target < 0) return;
    const foe = this.byId.get(u.extra.target);
    if (!foe || !live(foe)) { u.extra.target = -1; return; }
    const C = SEA.classes[u.extra.cls as SeaClass];
    const d = dist(u.x, u.y, foe.x, foe.y);
    if (d > C.reach) return;
    const range = this.reachFactor(C, d);
    const arc = SEA.fire.endOn + (1 - SEA.fire.endOn) * Math.abs(Math.sin(turn(u.face, bearing(u.x, u.y, foe.x, foe.y))));
    const nerve = u.state === 'wavering' ? SEA.nerve.wavering : u.state === 'routing' || this.orders.get(u.id)?.kind === 'leave' ? 0.5 : 1;
    if (this.contactAt === null) { this.contactAt = this.t; this.say({ what: 'contact', unit: u.id, side: u.side, text: 'The fleets are in range of each other.' }); }
    u.engaged = foe.id;
    // One punch per unit of weight whatever the class (b415): the card already prices a battleship above a cruiser.
    const w = SEA.fire.rate * this.output(u) * range * arc * nerve * this.day(u.side) * DT;
    this.dealt[u.id] += w;
    this.inReach[u.id] += DT;
    // Gunfire, like a strike, takes the same weight out of the fight whatever it falls on (wear in proportion to the
    // ship's hull): a heavy fleet's light screen is no weaker for being light, and a fleet is worth what the campaign
    // counts it, whatever its mix (the second playtester's newcomer test, b402).
    this.wear(foe, this.evenWear(foe, w));
  }

  /** The wear that takes 'w' of weight out of the fight on this piece, whatever it is: in proportion to its hull. */
  private evenWear(e: SeaUnit, w: number): number {
    const p = this.info.get(e.id)!;
    return (w * p.hull) / (p.weight * SEA.fire.hull);
  }

  /** Wear on a piece's hull: ships go down as it wears away, and its nerve is shaken. */
  private wear(u: SeaUnit, amount: number): void {
    if (amount <= 0 || !live(u)) return;
    const H = this.hullOf(u);
    u.str = Math.max(0, u.str - amount / H);
    u.mor = Math.max(0, u.mor - SEA.nerve.hit * amount / H);
    this.lastHit.set(u.id, this.t);
    const afloat = u.str <= 1e-9 ? 0 : Math.ceil(u.str * u.extra.ships0 - 1e-9);
    while (u.extra.ships > afloat) this.loseShip(u);
  }

  /** One ship of a piece goes: down, or out of the fight burning once its side has lost all the dice allow of its class. */
  private loseShip(u: SeaUnit): void {
    const c = u.extra.cls as SeaClass, side = u.side, fleet = this.info.get(u.id)!.fleet.id;
    u.extra.ships--;
    const one = RULES_CLASSES[c].one;
    if (this.lostShips[side][c] < this.caps[side][c]) {
      this.lostShips[side][c]++;
      u.extra.sunk++;
      this.fleetLoss.sunk[fleet][c]++;
      this.say({ what: 'sea:sunk', unit: u.id, side, text: `A ${one} of the ${u.extra.fleet} goes down.`, extra: { cls: c, fleet } });
      for (const f of this.units) if (f.side === side && f.id !== u.id && live(f) && dist(f.x, f.y, u.x, u.y) <= SEA.nerve.nearR) f.mor = Math.max(0, f.mor - SEA.nerve.near);
    } else {
      this.fleetLoss.burnt[fleet][c]++;
      u.extra.burnt = ((u.extra.burnt as number | undefined) ?? 0) + 1;
      this.say({ what: 'sea:burning', unit: u.id, side, text: `A ${one} of the ${u.extra.fleet} is out of the fight, burning, and makes for port.`, extra: { cls: c, fleet } });
    }
    if (u.extra.ships <= 0) {
      // A piece whose last ship went out of the fight burning has left the field; one whose last ship sank is gone.
      u.state = u.extra.sunk >= u.extra.ships0 || !(u.extra.burnt as number | undefined) ? 'gone' : 'withdrawn';
      u.str = u.state === 'gone' ? 0 : u.str;
      // A lone ship's line has just been said; a piece of several gets one line for the last of them.
      const all = `All ${u.extra.ships0} ${RULES_CLASSES[c].many} of the ${u.extra.fleet}`;
      this.say({ what: u.state === 'gone' ? 'gone' : 'withdrawn', unit: u.id, side, text: u.extra.ships0 === 1 ? '' : u.state === 'gone' ? `${all} are lost.` : `${all} are out of the fight.` });
    }
  }

  /** Strikes due to land, counted, not flown: the weight falls on the target and the ships near it. */
  private landStrikes(): void {
    const due = this.strikes.filter((s) => s.at <= this.t);
    if (!due.length) return;
    this.strikes = this.strikes.filter((s) => s.at > this.t);
    for (const s of due) {
      for (const u of this.units) for (const a of u.abilities ?? []) if (a.key === 'strike' && a.winding && u.side === s.side && u.extra.fleet === s.from) a.winding = false;
      const foe = this.byId.get(s.target);
      const at = foe && live(foe) ? foe : this.heaviest(other(s.side));
      if (!at) continue;
      this.splash(at, s.weight, s.side);
      this.fromAir[`${s.side}:cv`] = (this.fromAir[`${s.side}:cv`] ?? 0) + s.weight;
      this.say({ what: 'sea:strike', unit: at.id, side: s.side, text: `${s.count ? `${s.count} bombers` : 'Aircraft'} from the ${s.from} find ${this.told(at)}.`, extra: { count: s.count } });
    }
  }

  /** The coasts' aircraft raid the enemy's ships every so often, with the weight the rules give them over this sea. */
  private coastAir(): void {
    for (const side of ['att', 'def'] as Side[]) {
      const w = this.ticket.place.air[side];
      if (w <= 0 || this.t < this.nextAir[side]) continue;
      this.nextAir[side] += SEA.air.every;
      const foes = this.units.filter((e) => e.side !== side && live(e));
      if (!foes.length) continue;
      // Aircraft from the coast find whatever ship they find: any piece is as likely as any other.
      const at = foes[this.rng.int(foes.length)];
      const weight = SEA.fire.rate * w * SEA.air.share * SEA.air.every * this.day(side);
      // The same weight out of the fight whatever it falls on, as gunfire and strikes (b402): a raid that happened on a
      // destroyer took more than twice what one on a battleship took, so a fleet of many light ships paid double.
      this.splash(at, weight, side);
      this.fromAir[`${side}:air`] = (this.fromAir[`${side}:air`] ?? 0) + weight;
      this.say({ what: 'sea:air', unit: at.id, side, text: `Aircraft from the coast attack ${this.told(at)}.` });
      this.lastAir = this.t;
    }
  }

  /** A strike's weight shared over the target and the ships within the strike's radius of it, by their weight. */
  private splash(at: SeaUnit, weight: number, by: Side): void {
    const hit = this.units.filter((e) => e.side !== by && live(e) && dist(e.x, e.y, at.x, at.y) <= SEA.strike.radius);
    const total = hit.reduce((n, e) => n + this.output(e) + (e.id === at.id ? this.output(e) : 0), 0);
    if (total <= 0) return;
    for (const e of hit) {
      const share = weight * (this.output(e) + (e.id === at.id ? this.output(e) : 0)) / total;
      // A strike or a raid takes the same weight out of the fight whatever it falls on, wear in proportion to the
      // ship's hull, so where the player sends it is a choice of which ships to cripple, not a trick (ruling 226, 4).
      this.wear(e, this.evenWear(e, share));
    }
  }

  // ---- Nerve ------------------------------------------------------------------------------------------------------

  private nerve(u: SeaUnit): void {
    const N = SEA.nerve;
    if (this.t - (this.lastHit.get(u.id) ?? -1e9) > N.quiet) u.mor = Math.min(1, u.mor + N.mend * DT);
    if (u.state === 'routing') {
      if (this.broken !== u.side && u.mor >= N.waver + 0.1) { u.state = 'steady'; this.say({ what: 'rallied', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'have' : 'has'} rallied.` }); }
      return;
    }
    if (u.mor < N.break) {
      u.state = 'routing';
      this.say({ what: 'routing', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'break off and turn' : 'breaks off and turns'} for home.` });
    } else if (u.mor < N.waver) {
      if (u.state !== 'wavering') { u.state = 'wavering'; this.say({ what: 'wavering', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'are' : 'is'} wavering.` }); }
    } else if (u.state === 'wavering') u.state = 'steady';
  }

  // ---- How it ends ----------------------------------------------------------------------------------------------

  private breakSide(side: Side, ordered: boolean): void {
    this.broken = side;
    this.brokeAt = this.t;
    for (const u of this.units) if (u.side === side && live(u)) u.state = 'routing';
    this.say({ what: 'broken', side, text: ordered ? `The ${this.adj(side)} fleet breaks off the action on its admiral's order.` : `The ${this.adj(side)} fleet has had enough and breaks off the action.` });
  }

  private adj(side: Side): string { return this.ticket[side].adj; }

  private judge(): void {
    for (const side of ['att', 'def'] as Side[]) {
      if (!this.units.some((u) => u.side === side && live(u))) {
        const lost = this.units.filter((u) => u.side === side);
        this.finish(other(side), lost.every((u) => u.state === 'gone') ? 'destroyed' : 'withdrew');
        return;
      }
    }
    if (!this.broken) for (const side of ['att', 'def'] as Side[]) if (this.lostShare(side) >= this.breakAt[side]) { this.breakSide(side, false); break; }
    if (!this.broken) for (const side of ['att', 'def'] as Side[]) {
      if (this.warned[side] || this.nearBreak(side) < 0.75) continue;
      this.warned[side] = true;
      this.say({ what: 'nearBreak', side, text: this.ticket[side].human ? 'Your fleet is near breaking off: pull back the worn, or hand it to the staff.' : `The ${this.adj(side)} fleet is near breaking off.` });
    }
    if (this.broken && this.t - this.brokeAt >= SEA.side.pursuit) { this.finish(other(this.broken), 'withdrew'); return; }
    if (this.t >= this.ticket.limit) this.finish(this.leader(), 'time');
  }

  /** Who has the better of it so far, by the exchange. */
  private leader(): Side {
    if (this.broken) return other(this.broken);
    const la = this.lostShare('att'), ld = this.lostShare('def');
    return (ld + 0.05) / (la + 0.05) >= 1 ? 'att' : 'def';
  }

  private finish(winner: Side, how: FieldResult['how'], toDice = false, rolled?: number): void {
    if (this.over) return;
    const lost = { att: this.lostShare('att'), def: this.lostShare('def') };
    const R = rolled ?? seaRatio(winner, lost, this.ticket.reckoning.odds);
    const crews: Record<Side, number> = { att: 0, def: 0 };
    for (const u of this.units) crews[u.side] += u.extra.sunk * SEA.classes[u.extra.cls as SeaClass].crew;
    const forces: FieldResult['forces'] = [];
    for (const f of this.ticket.place.fleets) {
      let w0 = 0, left = 0;
      for (const u of this.units) if (this.info.get(u.id)!.fleet.id === f.id) { const p = this.info.get(u.id)!; w0 += p.weight * u.extra.ships0; left += p.weight * u.extra.ships0 * u.str; }
      forces.push({ corps: f.id, side: f.side, lost: w0 > 0 ? 1 - left / w0 : 0 });
    }
    const sunkOf = (side: Side): SeaShips => { const s = none(); for (const f of this.ticket.place.fleets) if (f.side === side) for (const c of ['bb', 'cv', 'ca', 'dd'] as SeaClass[]) s[c] += this.fleetLoss.sunk[f.id][c]; return s; };
    const sa = sunkOf('att'), sd = sunkOf('def'), a = this.ticket.att.adj, d = this.ticket.def.adj;
    const lines = [
      `${this.adj(winner)} ships hold the ${this.ticket.place.zoneName}.`,
      // Handed to the staff, the rules sink the rest as they settle it, so this line says only what went down in sight.
      !toDice ? `${a} losses: ${shipWords(sa)} sunk. ${d} losses: ${shipWords(sd)} sunk.`
        : shipsIn(sa) + shipsIn(sd) === 0 ? 'No ship had gone down on either side when the staff took over.'
          : `When the staff took over, the ${a} fleet had lost ${shipWords(sa)} and the ${d} fleet ${shipWords(sd)}.`,
    ];
    // HAND IT TO THE STAFF (log b348): the paper line after it, in the UI director's three forms (ruling 226, 5): the
    // whole action before a shot told; "what you had won stays won" only where the field had moved the hander's way by
    // 5 points or more, his share of the balance at the hand-over against the card's (the design director, 20:51 UTC).
    if (toDice) {
      const zone = this.ticket.place.zoneName, by = this.handedBy ?? 'att';
      const k = this.ticket.reckoning.odds, now = (k * (1 - this.lostShare('att'))) / Math.max(1e-6, 1 - this.lostShare('def'));
      const share = (o: number): number => (by === 'att' ? o : 1) / (1 + o);
      lines.push(!this.fired() ? `The staff settled the whole action in the ${zone}: no shot had been fired.`
        : share(now) - share(k) >= 0.05 ? `The staff settled the rest of the action in the ${zone} from where it stood: what you had won stays won.`
          : `The staff settled the rest of the action in the ${zone} from where it stood.`);
    }
    const extra: SeaLosses = { sunk: this.fleetLoss.sunk, burnt: this.fleetLoss.burnt, ...(toDice ? { toDice } : {}) };
    this.over = {
      winner, how, seconds: this.t, lost, men: crews, R, level: seaLevel(R), lines, forces,
      extra: extra as unknown as Record<string, unknown>,
    };
    this.say({ what: 'end', side: winner, text: lines[0] });
  }

  // ---- What the screens draw --------------------------------------------------------------------------------------

  /** A piece in a sentence, by what it is and whose, the fleet taking "the" as English does (log b370): "a battleship of the Home Fleet", "3 destroyers of the Home Fleet". */
  private told(u: SeaUnit, start = false): string {
    const c = RULES_CLASSES[u.extra.cls as SeaClass], n = Math.max(1, u.extra.ships);
    const s = n === 1 ? `a ${c.one} of the ${u.extra.fleet}` : `${n} ${c.many} of the ${u.extra.fleet}`;
    return start ? s[0].toUpperCase() + s.slice(1) : s;
  }

  /** More than one ship still afloat in the piece: the sentence takes a plural verb. */
  private many(u: SeaUnit): boolean { return u.extra.ships > 1; }

  /** An event, placed where its unit is and with the sound that answers it (cues.ts), unless it names its own. */
  private say(e: Omit<FieldEvent, 't'>): void {
    const u = e.unit !== undefined ? this.byId.get(e.unit) : undefined;
    const cue = SEA_CUES[e.what];
    this.events.push({ t: this.t, ...(u ? { x: Math.round(u.x), y: Math.round(u.y) } : {}), ...(cue ? { cue } : {}), ...e });
  }

  /** What the models show: one ship a figure, the way on, and what the piece is doing, for the animation and the sound. */
  private pose(u: SeaUnit): void {
    u.figures = u.extra.ships;
    if (!live(u)) { u.act = u.state === 'gone' ? 'down' : 'idle'; u.speed = 0; u.engaged = undefined; return; }
    const C = SEA.classes[u.extra.cls as SeaClass];
    u.speed = Math.round(u.extra.way * 10) / 10;
    u.act = u.state === 'routing' || this.orders.get(u.id)?.kind === 'leave' ? 'rout'
      : u.engaged !== undefined ? 'fight'
        : u.extra.way < 0.5 ? 'idle'
          : this.swing[u.id] > 0.6 ? 'turn'
            : u.extra.way > C.speed * SEA.speeds.cruise * 1.05 ? 'run' : 'march';
  }

  /** Each side's ships still in the fight, as a share of both sides' weight at the start. */
  balance(): Record<Side, number> {
    const total = this.weight0.att + this.weight0.def;
    const standing = (side: Side): number => this.units.reduce((n, u) => n + (u.side === side && live(u) ? this.weightOf(u) : 0), 0);
    return { att: total > 0 ? standing('att') / total : 0, def: total > 0 ? standing('def') / total : 0 };
  }

  view(as?: Side): BattleView<SeaUnit> {
    const events = this.events;
    this.events = [];
    void as; // Everything at sea is in sight in the first slice: spotting and smoke wait for the polish pass (TODO(polish)).
    return {
      t: this.t,
      limit: this.ticket.limit,
      units: this.units.map((u) => ({ ...u, extra: { ...u.extra }, abilities: u.abilities?.map((a) => ({ ...a })) })),
      balance: this.balance(),
      events,
      zones: this.zones.map((z) => ({ ...z })),
      ai: { ...this.ai },
      over: this.over,
      breaking: { att: this.nearBreak('att'), def: this.nearBreak('def') },
      contact: this.contactAt,
    };
  }

  /** Battle time the guns first reached an enemy, null before it. */
  get contactTime(): number | null { return this.contactAt; }

  /** The side that has broken off the action and is making for port, while the other chases; null before. */
  get brokenSide(): Side | null { return this.broken; }

  /** Nothing is happening yet: no gun has reached an enemy, no strike is in the air and no raid has just come in. The
   *  screens run time fast while it holds, and again after an early strike has come and gone (ruling 226(1); the
   *  design director, 20:18 UTC). Once the guns have met it never holds again. */
  get quiet(): boolean { return this.contactAt === null && this.strikes.length === 0 && this.t - this.lastAir >= 10; }
}

const TAU = Math.PI * 2;
const norm = (a: number): number => ((a % TAU) + TAU) % TAU;
/** The order of a piece that has none: fight at will. */
const FREE: SeaOrder = { kind: 'free' };
