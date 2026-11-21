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
// fleet), and the carrier strike. Pieces with no order fight at will, as their captains would, each ship class to its
// own job around the fleet's main effort (ruling 239): the player steps in where they want a different target, a
// different course, or the fight broken off. Destroyers screen their big ships, or make torpedo runs at the enemy's
// with the weight their guns hold back (torp). Formations wait for the polish pass (TODO(polish)).
//
// Same ticket, same orders, same battle: every roll comes from the battle's own seeded stream.

import { Rng } from '../../core/rng';
import { bearing, clamp, diceNow, dist, lineUp, turn, DT } from '../core';
import type { BattleSim, BattleView, Command, FieldEvent, FieldResult, Side, Zone } from '../core/types';
import { CAP, DICE, FLAK, RULES_CLASSES, SEA, STRIKE_AIM } from './numbers';
import { drawUp, type PieceInfo } from './pieces';
import { SEA_CUES } from './cues';
import { none, seaLevel, seaRatio, shipsIn, shipWords, sunkCaps } from './result';
import { headCount, hurtAfloat, ofFleet, theFleet } from './words';
import type { SeaClass, SeaCommand, SeaOrder, SeaShips, SeaSpeed, SeaStyle, SeaTicket, SeaUnit } from './types';
import type { StyleNumbers } from './numbers';
import { seaWhat, seaWhere } from '../../campaign/sea-words';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
/** A number from 0 to under 1 fixed by a whole number: where a lost ship's smoke lies, never drawn from the battle's dice. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
/** Pieces that still answer orders and fight as told: not running for home. */
const steady = (u: SeaUnit): boolean => u.state === 'steady' || u.state === 'wavering';
/** The big guns: the classes whose fire a style shares out over the enemy's big ships (jobs). */
const BIG_GUNS = new Set<string>(['bb', 'ca']);
/** Whether 'u' lies ahead of 'e's beam: it bears less than 90 degrees off her heading, so her own way carries her toward it. */
const aheadOfBeam = (e: SeaUnit, u: SeaUnit): boolean => Math.abs(turn(e.face, bearing(e.x, e.y, u.x, u.y))) < Math.PI / 2;

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
 *  staff's (ai). */
export function enemyNearestMiddle(units: readonly SeaUnit[], side: Side): SeaUnit | null {
  const mine = units.filter((u) => u.side === side && live(u));
  if (!mine.length) return null;
  const cx = mine.reduce((n, u) => n + u.x, 0) / mine.length, cy = mine.reduce((n, u) => n + u.y, 0) / mine.length;
  return nearestEnemy(units, side, cx, cy);
}

interface Strike { at: number; side: Side; target: number; weight: number; from: string; count: number; cv: number }

/**
 * A destroyer piece's torpedo run (ruling 239): in at its target to its style's press range, launch, then away. 'div'
 * is its division (R4): the pieces whose runs opened in the same second at one target, closing in company; 0 alone.
 * 'guard': the division went to turn away a ship threatening its own carriers or outnumbered battleships (R4's Samar
 * cue): it launches as soon as she is in torpedo reach (pressOf) and runs on through its own side's break (the design
 * director, 08:10Z and 11:25Z).
 */
interface Run { target: number; phase: 'in' | 'away'; until: number; away: number; face: number; turned: number; launched: number; near: boolean; shot: boolean; div: number; guard: boolean }
/**
 * A torpedo salvo in the water: it falls torp.travel seconds after its launch. 'drawn' is the part of it the guns had
 * not yet earned (loaded), owed by its side when it lands.
 */
interface Torp { at: number; side: Side; target: number; x: number; y: number; weight: number; from: number; drawn: number }
/**
 * A side's battle line (the ship plan's R1, ruling 239's order at 23:20Z): its battleships and cruisers steaming one
 * course together, its rows as they drew up turned into columns in line ahead. 'hand' is the side of its bearing to
 * the enemy it steers (+1 to the right, -1 to the left); 'order' its pieces foremost first along its course, the van
 * leading; 'turned' the second it last turned together off the field's edge or away from a crossed T.
 */
interface Line { order: number[]; hand: number; course: number; speed: number; turned: number }

/** Each fleet's ships lost on the field, by class: sunk, and gone out of the fight burning (past the dice's limit); and
 *  the ships it took home badly hurt, those burning and those afloat at half their hull or less (b458, 3). */
export interface SeaLosses { sunk: Record<number, SeaShips>; burnt: Record<number, SeaShips>; hurt: Record<number, SeaShips>; toDice?: boolean }

/** The orders that let a beaten enemy go, given by the side chasing it (b458, 1): hold, break off (a piece, or the whole
 *  fleet with WITHDRAW), or steer for a point on its own sea (STEER, and ownSea). */
const LET_GO = new Set<string>(['hold', 'withdraw', 'withdrawAll', 'move', 'line']);
/** The orders that steer for a point: they let the beaten enemy go only where the point is the chaser's own sea. */
const STEER = new Set<string>(['move', 'line']);

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
  /** The coasts' aircraft still in the fight, each side's, in weight: the card counts them in the side's strength. */
  private readonly airLeft: Record<Side, number>;
  /** What is left of each carrier piece's aircraft, 1 at the start, by unit id (1 for every other piece). */
  private readonly deck: number[] = [];
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
  private readonly lastHit = new Map<number, number>();
  private readonly look = new Map<number, number>();
  /** Which way round each piece turns to bring its broadside to bear: kept, so that it does not dither. */
  private readonly hand = new Map<number, number>();
  private strikes: Strike[] = [];
  /** The enemy each carrier last struck, by the carrier's id (strikeTarget's spread, and the C1 mark). */
  private readonly struck = new Map<number, number>();
  /** The destroyer pieces whose torpedo line has been told: once a piece, on its first salvo. */
  private readonly toldTorp = new Set<number>();
  /** Fighters over the fleet (cap): what each carrier holds back of its strikes and has not yet spent, by its id (its
   *  side's pool is their sum); and what each gun ship owes out of its next gunfire for what its guns denied. */
  private readonly cover: number[] = [];
  private readonly owe: number[] = [];
  /** What the fighters did to the last strike or raid that met them: the share they turned back, and whose they were. */
  private capLast: { share: number; fleet: string } | null = null;
  /** What each side's guns have denied, by whose aircraft: a carrier's strikes ('cv') or the coasts' raids ('air'). */
  private readonly flakOn: Record<Side, { cv: number; air: number }> = { att: { cv: 0, air: 0 }, def: { cv: 0, air: 0 } };
  /** A side's carrier has come within the guns' reach of an enemy battleship (the C5 mark). */
  private readonly cvNear: Record<Side, boolean> = { att: false, def: false };
  /** Destroyer pieces on a torpedo run, by unit id; the battle time each may run again; the weight each has banked for
   *  its torpedoes (torp); the screen's pieces, chosen as the fleets draw up, with the big ship each guards and how far
   *  to its side it keeps; and salvos in the water. */
  private readonly runs = new Map<number, Run>();
  private readonly nextRun = new Map<number, number>();
  private readonly bank: number[] = [];
  /**
   * A destroyer piece's loaded salvo not yet earned by its guns (the design director, 05:01Z and 06:14Z: the ship plan's
   * R5): it begins the action with its tubes full, a salvo drawn in advance from its guns' later fire. Weight its guns
   * could not fire earns it down (store); what is still unearned rides with the salvo (Torp.drawn) and is owed by its
   * side when the salvo lands (advance).
   */
  private readonly loaded: number[] = [];
  /**
   * What a side owes for salvoes that landed before its guns had earned them, paid out of its gun ships' next fire,
   * weight for weight, as R2's debt is paid out of its blows (the design director, 06:14Z). Until paid it is weight the
   * tubes dealt beyond the guns; a salvo still running when the action ends owes nothing, having dealt nothing.
   */
  private readonly advance: Record<Side, number> = { att: 0, def: 0 };
  /**
   * What the unearned part of the enemy's landed salvoes took out of a side, in lostShare's own measure, as the take
   * itself measured it (a carrier's deck, the coasts' aircraft, a piece worn through: what it took, not what it weighed).
   * Added back to the side's weight left until the enemy's guns pay the advance, and cut as they pay, part for part: so
   * the advance counts on neither side until it is paid, as R2's debt (the design director, 09:48Z).
   */
  private readonly unpaid: Record<Side, number> = { att: 0, def: 0 };
  private readonly screen = new Set<number>();
  private readonly guard = new Map<number, { ship: number; lat: number }>();
  private torps: Torp[] = [];
  private zoneId = 0;
  /** Runs launched by each side's destroyers outside the screen (the B4 mark). */
  private readonly launched: Record<Side, number> = { att: 0, def: 0 };
  private readonly nextAir: Record<Side, number>;
  /** Wear each piece has dealt by gunfire, and seconds it has spent with a target in reach, by unit id: see tally. */
  private readonly dealt: number[] = [];
  private readonly inReach: number[] = [];
  /** How hard each piece is turning, as a share of its helm at full way: for 'turn' in the models' act. */
  private readonly swing: number[] = [];
  /** What the 3D game draws and nothing in the rules reads back (the ship plan's B1, ruling 239 at 23:20Z): see showLooks. */
  private readonly steered: number[] = [];
  private readonly helmed: number[] = [];
  private readonly away: boolean[] = [];
  private readonly coverHeld: number[] = [];
  private recovers: { at: number; cv: number; side: Side }[] = [];
  /** Each side's battle line once the guns have met (see line), and this tick's plan for it. */
  private readonly lines: Record<Side, Line | null> = { att: null, def: null };
  private readonly linePlans: Record<Side, { t: number; members: SeaUnit[] }> = { att: { t: -1, members: [] }, def: { t: -1, members: [] } };
  /** Wear dealt by strikes ('att:cv') and by the coasts' aircraft ('att:air'). */
  private readonly fromAir: Record<string, number> = {};
  /** Wear dealt by each side's classes ('att:bb') over the battle, and seconds each spent in reach ('att:bb:s'): for tuning, read by tools and tests only. */
  get tally(): Record<string, number> {
    const out: Record<string, number> = { ...this.fromAir, ...this.marks };
    for (const u of this.units) {
      const k = `${u.side}:${u.extra.cls}`;
      out[k] = (out[k] ?? 0) + this.dealt[u.id];
      out[`${k}:s`] = (out[`${k}:s`] ?? 0) + this.inReach[u.id];
    }
    return out;
  }
  /**
   * What the tactics panel's marks read (tactics-panel/briefs/sea.md), in the tally beside the wear by class: the wear
   * each class dealt on each enemy class ('att:bb>ca'); and, once a second after the guns meet, a gun class's seconds of
   * fire while an enemy big ship was in its reach ('att:bb@big:s'), and of those at one ('att:bb@big>big:s'); and, while
   * a side's battleships (or cruisers) in reach of the enemy's are at least as many, the enemy's in their reach counted
   * each second ('att:bb:exposed:s'), with those that no ship of the class was firing on ('att:bb:untroubled:s') and
   * those no ship at all was ('att:bb:unfired:s'). Counted only: nothing reads them back, and they draw no dice.
   */
  private readonly marks: Record<string, number> = {};
  private mark(k: string, n: number): void { this.marks[k] = (this.marks[k] ?? 0) + n; }
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
      this.steered[u.id] = -1;
      this.helmed[u.id] = 0;
      this.away[u.id] = false;
      this.coverHeld[u.id] = 0;
      this.deck[u.id] = 1;
      this.bank[u.id] = 0;
      this.cover[u.id] = 0;
      this.owe[u.id] = 0;
      if (u.extra.cls === 'cv') u.extra.planes = 1;
      this.weight0[u.side] += this.weightOf(u);
    }
    // The card counts the coasts' aircraft in each side's strength (navy.ts seaBattles), so the field does: they are
    // worn down with the fleet and a side breaks off on the whole of what it brought (b415).
    this.airLeft = { att: ticket.place.air.att, def: ticket.place.air.def };
    this.weight0.att += this.airLeft.att;
    this.weight0.def += this.airLeft.def;
    for (const f of ticket.place.fleets) { this.fleetLoss.sunk[f.id] = none(); this.fleetLoss.burnt[f.id] = none(); }
    this.caps = sunkCaps(ticket.place.fleets, ticket.reckoning.odds);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    // Each side's captains know their main effort from the start: the enemy nearest the middle of their fleet as it is drawn up.
    for (const side of ['att', 'def'] as Side[]) this.focus[side] = this.nearestToMiddle(side);
    for (const side of ['att', 'def'] as Side[]) this.drawScreen(side);
    const S = SEA.side;
    this.luck = this.rng.range(DICE[0], DICE[1]);
    // The destroyers go in with their tubes full: a full salvo loaded, drawn in advance from their guns (loaded).
    for (const u of units) {
      this.loaded[u.id] = 0;
      if (u.extra.cls !== 'dd') continue;
      this.bank[u.id] = this.loaded[u.id] = SEA.fire.rate * this.output(u) * this.day(u.side) * SEA.torp.bank;
    }
    const adm = (side: Side): number => Math.max(0, ...ticket.place.fleets.filter((f) => f.side === side).map((f) => f.admiral?.level ?? 0));
    this.breakAt = {
      att: S.breakAt + S.admiral * adm('att') + S.spread * (this.rng.next() * 2 - 1),
      def: S.breakAt + S.admiral * adm('def') + S.spread * (this.rng.next() * 2 - 1),
    };
    // The coasts' first raids come in at a random moment of the first interval, so the two sides' do not land together.
    this.nextAir = { att: this.rng.range(0.2, 0.8) * SEA.air.every, def: this.rng.range(0.2, 0.8) * SEA.air.every };
    this.showLooks();
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
    return p.weight * (afloat * SEA.fire.damaged + (1 - SEA.fire.damaged) * u.str * n0) * this.deck[u.id];
  }
  /** Its weight as it stands: what is left of its hull, and of a carrier's aircraft. */
  private weightOf(u: SeaUnit): number { return this.info.get(u.id)!.weight * u.extra.ships0 * (live(u) || u.state === 'withdrawn' ? u.str : 0) * this.deck[u.id]; }
  private hullOf(u: SeaUnit): number { const p = this.info.get(u.id)!; return p.hull * u.extra.ships0; }

  /** The order a piece is under, for the screens (undefined: it fights at will). */
  orderOf(id: number): SeaOrder | undefined { return this.orders.get(id); }

  /** The share of a side's starting weight it has lost: hull worn away, on pieces still there or gone, and aircraft lost,
   *  the carriers' and the coasts'. Hull that went out of the line with a badly hurt ship counts only as the enemy's
   *  blows pay for it (debt): the break and the count read weight dealt, nothing more (the design director, 03:49Z).
   *  So does the part of a landed salvo its side's guns had not yet earned (advance): owed as R2's debt is, it counts
   *  on neither side until it is paid (the design director, 09:48Z: what V3 meant). */
  lostShare(side: Side): number {
    return this.weight0[side] > 0 ? 1 - (this.left(side) + this.unpaid[side]) / this.weight0[side] : 0;
  }
  /** A side's weight left as lostShare counts it, before what the enemy's unpaid salvoes took is added back (unpaid). */
  private left(side: Side): number {
    let left = this.airLeft[side] + this.debt[other(side)] / SEA.fire.hull;
    for (const u of this.units) if (u.side === side) left += this.info.get(u.id)!.weight * u.extra.ships0 * u.str * this.deck[u.id];
    return left;
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
   *  as the staff reckon it (the day's own luck, which settles it, is not theirs to know); in a chase, sure for the
   *  chaser. */
  handChance(): number {
    // Once a fleet has broken off the staff can only give the sea to the chaser (the 'dice' order), and they say so.
    if (this.broken) return this.broken === 'def' ? 1 : 0;
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
    // player gives an order of their own: then they do as told (b402: the newcomer's chase is the staff's).
    if (ok && chasing && c.do !== 'dice' && c.do !== 'auto') this.chaseTaken[by] = true;
    if (ok && chasing) {
      // An order to attack a beaten enemy is a pursuit: a carrier sent after it may fly new strikes (b458, 1).
      if (c.do === 'attack' || c.do === 'ability') {
        const target = c.target;
        for (const id of c.units) { const u = this.byId.get(id); if (u && u.side === by && u.extra.cls === 'cv' && target !== undefined) this.pursuing.set(id, target); }
      }
      // An order to hold, break off or steer for a point on the chaser's own sea lets the beaten enemy go: the action ends
      // there (b458, 1). A course among or past the beaten fleet runs it down, as an order to attack does: a right-click
      // that misses a fleeing hull by a few pixels never ends the action the player meant to press.
      if (LET_GO.has(c.do) && (!STEER.has(c.do) || this.steersHome(c, by))) this.letGo(by);
    }
    return ok;
  }

  /**
   * Whether a point lies on a side's own sea while it chases a beaten enemy (b458, 1): nearer the middle of its own fleet
   * still in the fight than the beaten fleet's middle and than every enemy piece still afloat. A course for such a point
   * lets the enemy go; one among the beaten fleet, or past it, runs it down. Each enemy, and the enemy's middle, cuts the
   * sea in two along the line halfway between it and the side's middle, so the side's own sea is the part on its side of
   * every such line: a line ordered with both ends there lies there whole.
   */
  ownSea(side: Side, x: number, y: number): boolean {
    const all = this.units.filter((u) => u.side === side && live(u)), fighting = all.filter((u) => u.state !== 'routing');
    const mine = fighting.length ? fighting : all, theirs = this.units.filter((u) => u.side !== side && live(u));
    if (!mine.length || !theirs.length) return true;
    const mid = (us: SeaUnit[]): [number, number] => [us.reduce((n, u) => n + u.x, 0) / us.length, us.reduce((n, u) => n + u.y, 0) / us.length];
    const [mx, my] = mid(mine), [tx, ty] = mid(theirs), home = Math.hypot(x - mx, y - my);
    return Math.hypot(x - tx, y - ty) > home && theirs.every((e) => Math.hypot(e.x - x, e.y - y) > home);
  }

  /** A steer order whose every point is the side's own sea (ownSea). */
  private steersHome(c: Command | SeaCommand, by: Side): boolean {
    if (c.do === 'move') return this.ownSea(by, c.x, c.y);
    if (c.do === 'line') return this.ownSea(by, c.x0, c.y0) && this.ownSea(by, c.x1, c.y1);
    return false;
  }

  /**
   * The side chasing a beaten enemy lets it go (b458, 1): the action ends at once, the side that broke off beaten and
   * every loss as it stands, booked as any other ending is (the after card, and the campaign through handoff.ts). An
   * order to hold, break off or steer for a point on its own sea does this (command); false unless this side's enemy
   * has broken off and the action is still on.
   */
  letGo(side: Side): boolean {
    if (this.over || this.broken === null || this.broken === side) return false;
    this.finish(side, 'withdrew');
    return true;
  }

  /** The player's sides take the chase in hand with their first order after the enemy breaks off. */
  private readonly chaseTaken: Record<Side, boolean> = { att: false, def: false };
  /** Carriers the player has sent after a beaten enemy since it broke off, and at whom: only these strike in the chase. */
  private readonly pursuing = new Map<number, number>();

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
        // Given to two or more pieces, an order to attack is a fleet order (ruling 239): the enemy named is the fleet's
        // main effort and each piece does its job around it, as the staff's captains do, so the newcomer's plain order
        // fights the staff's battle (b402). Given to one piece, it is that piece's own target.
        const fleet = us.length > 1;
        for (const u of us) {
          // A carrier attacks with its strike: the order is the strike, flown at the next look round (within a
          // second) whenever its aircraft are ready, the same second the staff's carrier would fly it (b402).
          this.orders.set(u.id, fleet ? { kind: 'attack', target: foe.id, fleet } : { kind: 'attack', target: foe.id });
          if (!fleet && u.extra.cls !== 'cv') u.extra.target = foe.id;
        }
        if (fleet) this.focus[by] = foe.id;
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
        // Once the enemy has broken off, the chasing fleet breaking off lets it go (command: b458, 1); a fleet that has
        // broken off itself has nothing left to withdraw.
        if (c.side !== by || this.broken === by) return false;
        if (!this.broken) this.breakSide(by, true);
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
        // Once a fleet has broken off, the sea is the chaser's whoever hands it over: the staff settle only by how much,
        // the dice's ratio held to the chaser's side of the line as a fought ending's is (seaRatio). Never the fleet that
        // broke off, a moment after it was said to have had enough.
        if (this.broken) { const w = other(this.broken); this.finish(w, 'time', true, w === 'att' ? Math.max(1, R) : Math.min(0.999, R)); return true; }
        this.finish(R >= 1 ? 'att' : 'def', 'time', true, R);
        return true;
      }
      default:
        return false;
    }
  }

  /**
   * The enemy a carrier left to itself, or under a fleet order, strikes (jobs.strike; ruling 239, item 4): carriers
   * first, then battleships, cruisers and destroyers, ships still fighting before ships breaking off; among them the one
   * nearest its fleet's main effort, or, where its style shares fire out (spread), the one fewest of its side's other
   * carriers last struck, so a second carrier takes a second enemy carrier before doubling up. No dice.
   */
  private strikeTarget(cv: SeaUnit): SeaUnit | null {
    const side = cv.side, main = this.byId.get(this.focus[side]);
    const [mx, my] = main ? [main.x, main.y] : this.centre(other(side));
    const f0 = side === 'att' ? 0 : Math.PI, cs = Math.cos(f0), sn = Math.sin(f0);
    const spread = this.style(side).spread > 0;
    const on = (e: SeaUnit): number => {
      let n = 0;
      for (const [id, t] of this.struck) { const c = this.byId.get(id); if (id !== cv.id && t === e.id && c && live(c)) n++; }
      return n;
    };
    for (const breaking of [false, true]) {
      for (const tc of SEA.jobs.strike) {
        const pool = this.units.filter((e) => e.side !== side && e.extra.cls === tc && live(e) && (breaking || e.state !== 'routing'));
        if (!pool.length) continue;
        return pool.map((e) => ({ e, n: spread ? on(e) : 0, d: Math.round(dist(e.x, e.y, mx, my)), l: (e.x - mx) * cs - (e.y - my) * sn }))
          .sort((a, b) => a.n - b.n || a.d - b.d || a.l - b.l || a.e.id - b.e.id)[0].e;
      }
    }
    return null;
  }

  /** What a carrier last struck, and its first strike's target for the C1 mark: at a carrier where the enemy has one,
   *  else at a battleship where it has one. */
  private struckFirst(cv: SeaUnit, foe: SeaUnit): void {
    const first = !this.struck.has(cv.id);
    this.struck.set(cv.id, foe.id);
    if (!first) return;
    const has = (c: string): boolean => this.units.some((e) => e.side !== cv.side && e.extra.cls === c && live(e));
    this.mark(`${cv.side}:cvFirst`, 1);
    if (has('cv')) { this.mark(`${cv.side}:cvFirstVsCv`, 1); if (foe.extra.cls === 'cv') this.mark(`${cv.side}:cvFirstAtCv`, 1); }
    else if (has('bb')) { this.mark(`${cv.side}:cvFirstVsBb`, 1); if (foe.extra.cls === 'bb') this.mark(`${cv.side}:cvFirstAtBb`, 1); }
  }

  /** Orders a carrier's strike on an enemy piece. False if its aircraft are not ready. Counted, not flown. */
  private strike(cv: SeaUnit, foe: SeaUnit): boolean {
    const ab = cv.abilities?.find((a) => a.key === 'strike');
    if (!ab || ab.ready > 0 || ab.winding || !steady(cv)) return false;
    const p = this.info.get(cv.id)!;
    const whole = SEA.fire.rate * this.output(cv) * SEA.strike.share * SEA.strike.every * this.day(cv.side);
    // With enemy aircraft about, it holds its style's cover back over the fleet (cap), a strike the player orders as one
    // the staff does (ruling 239, 12); what it held last time and has not spent flies now, so no weight is lost.
    const keep = this.bringsAir(other(cv.side)) ? whole * this.style(cv.side).cover : 0;
    const weight = whole - keep + this.cover[cv.id];
    this.cover[cv.id] = keep;
    this.coverHeld[cv.id] = keep;
    const perCarrier = p.fleet.ships.cv > 0 ? Math.round(p.fleet.bombers / p.fleet.ships.cv) : 0;
    const count = Math.round(perCarrier * (this.output(cv) / (p.weight * cv.extra.ships0)));
    ab.winding = true;
    ab.ready = SEA.strike.every;
    this.strikes.push({ at: this.t + SEA.strike.windup, side: cv.side, target: foe.id, weight, from: cv.extra.fleet, count, cv: cv.id });
    this.say({ what: 'ability', unit: cv.id, side: cv.side, text: `${this.told(cv, true)} flies off ${count ? `${count} bombers` : 'its aircraft'} against ${this.told(foe)}.`, extra: { key: 'strike', target: foe.id, count } });
    return true;
  }

  // ---- The tick -------------------------------------------------------------------------------------------------

  step(): void {
    if (this.over) return;
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    const tick = Math.round(this.t / DT);
    this.planLines();
    for (const u of this.units) if (live(u)) this.steer(u);
    if (tick % 10 === 0) { this.sample(); this.watchCarriers(); this.decide(); }
    this.handOwes();
    for (const u of this.units) { u.engaged = undefined; if (live(u)) this.fire(u); }
    this.landStrikes();
    this.recover();
    this.landTorps();
    this.coastAir();
    for (const u of this.units) if (live(u)) this.nerve(u);
    // What the models show, once every blow of the tick has told: a ship a strike or a raid sank is not drawn afloat.
    for (const u of this.units) this.pose(u);
    this.showLooks();
    for (const u of this.units) for (const a of u.abilities ?? []) if (a.ready > 0) a.ready = Math.max(0, Math.round((a.ready - DT) * 1000) / 1000);
    this.zones = this.zones.filter((z) => z.until > this.t);
    this.judge();
  }

  /** Once a second after the guns meet, before the targets change: what the marks read (see marks). */
  private sample(): void {
    if (this.contactAt === null) return;
    // A run that passes within torp.intercept metres of an enemy screen piece (the B6 mark).
    for (const [id, r] of this.runs) {
      const u = this.byId.get(id);
      if (u && r.phase === 'in' && !r.near) r.near = this.units.some((e) => e.side !== u.side && this.screen.has(e.id) && live(e) && dist(u.x, u.y, e.x, e.y) <= SEA.torp.intercept);
    }
    const big = (c: string): boolean => c === 'bb' || c === 'ca' || c === 'cv';
    const near = (a: SeaUnit, b: SeaUnit, r: number): boolean => dist(a.x, a.y, b.x, b.y) <= r;
    for (const side of ['att', 'def'] as Side[]) {
      const mine = this.units.filter((u) => u.side === side && live(u) && u.state !== 'routing');
      const foes = this.units.filter((u) => u.side !== side && live(u) && u.state !== 'routing');
      for (const u of mine) {
        const c = u.extra.cls as SeaClass;
        if (c === 'cv' || u.engaged === undefined) continue;
        const R = SEA.classes[c].reach;
        if (!foes.some((e) => big(e.extra.cls) && near(u, e, R))) continue;
        this.mark(`${side}:${c}@big:s`, 1);
        const t = this.byId.get(u.engaged);
        if (t && big(t.extra.cls)) this.mark(`${side}:${c}@big>big:s`, 1);
      }
      for (const c of ['bb', 'ca'] as SeaClass[]) {
        const R = SEA.classes[c].reach, own = mine.filter((u) => u.extra.cls === c), theirs = foes.filter((e) => e.extra.cls === c);
        const exposed = theirs.filter((e) => own.some((u) => near(u, e, R)));
        const able = own.filter((u) => theirs.some((e) => near(u, e, R)));
        if (!exposed.length || able.length < exposed.length) continue;
        this.mark(`${side}:${c}:exposed:s`, exposed.length);
        this.mark(`${side}:${c}:untroubled:s`, exposed.filter((e) => !own.some((u) => u.engaged === e.id)).length);
        this.mark(`${side}:${c}:unfired:s`, exposed.filter((e) => !mine.some((u) => u.engaged === e.id)).length);
      }
    }
  }

  /** Once a second: targets, carriers' strikes, and the side's own reckoning. */
  private decide(): void {
    // A main effort that has gone or broken off gives way to the enemy now nearest the middle of the fleet.
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
      // A carrier with no strike ordered sends one whenever its aircraft are ready, at its fleet's main effort, so a
      // newcomer's one order and the staff's play strike the same ship (b402). Once a side has broken off, no carrier
      // flies a new strike unless the player has sent it after the beaten enemy since (b458, 1): the staff's carriers,
      // and the player's left to themselves, let it go. A strike already in the air still lands.
      if (u.extra.cls === 'cv' && steady(u) && (this.broken === null || this.pursuing.has(u.id))) {
        // The order as it stands now: one that has just lapsed falls through to the main effort, as at-will play does.
        // In the chase, the enemy it was sent after while that one is still afloat (an order on a ship breaking off lapses).
        const still = this.orders.get(u.id), sent = this.pursuing.get(u.id);
        const ordered = still?.kind === 'attack' && !still.fleet ? this.byId.get(still.target) : sent !== undefined ? this.byId.get(sent) : undefined;
        const foe = ordered && live(ordered) ? ordered : this.strikeTarget(u) ?? this.heaviest(other(u.side));
        if (foe && this.strike(u, foe)) this.struckFirst(u, foe);
      }
      if ((this.look.get(u.id) ?? 0) <= this.t) { this.look.set(u.id, this.t + SEA.fire.retarget); this.aim(u); }
    }
    this.startRuns();
  }

  /** Each side's main effort: the enemy piece nearest the middle of its own fleet, kept until it goes or breaks off,
   *  or the one the player names in a fleet order. Each ship does its job around it (jobs): the staff's play is
   *  exactly the newcomer's (select all, right-click the enemy nearest the middle), so the two give the same battle
   *  (b402, ruling 239). */
  private readonly focus: Record<Side, number> = { att: -1, def: -1 };
  private nearestToMiddle(side: Side): number {
    // An enemy breaking off is no longer the fleet's target while any other is still fighting (only in the chase is
    // every enemy breaking off). All of the side's own pieces stay in the list, so its middle is where it was.
    const fighting = this.units.filter((u) => u.side === side || u.state !== 'routing');
    return (enemyNearestMiddle(fighting, side) ?? enemyNearestMiddle(this.units, side))?.id ?? -1;
  }

  /**
   * The enemy piece a piece fires on (ruling 239, the tactics panel's item 2: each ship class to its own job). A piece
   * told on its own to attack one enemy fires on it whenever it is in reach. Otherwise it does its class's job
   * (jobs.fire): the first class of enemy it has in reach, ships still fighting before ships breaking off; among them
   * the one nearest its fleet's main effort, or, where its style shares the big guns' fire out (spread), the one fewest
   * of its side's battleships and cruisers are firing on, kept until another has two fewer. No dice: the same ships in
   * the same places make the same choice.
   */
  private aim(u: SeaUnit): void {
    const o = this.orders.get(u.id);
    const C = SEA.classes[u.extra.cls as SeaClass];
    if (o?.kind === 'attack' && !o.fleet) {
      const foe = this.byId.get(o.target);
      // A destroyer told to attack a bigger ship also makes a torpedo run at it (startRuns).
      if (foe && live(foe) && dist(u.x, u.y, foe.x, foe.y) <= C.reach) { u.extra.target = foe.id; return; }
    }
    u.extra.target = this.job(u);
  }

  /** The enemy a piece's job puts its guns on, or -1 with none of its classes in reach (see aim). */
  private job(u: SeaUnit): number {
    const c = u.extra.cls as SeaClass;
    if (c === 'cv') return -1;
    const side = u.side, R = SEA.classes[c].reach, main = this.byId.get(this.focus[side]);
    const [mx, my] = main ? [main.x, main.y] : this.centre(other(side));
    // Two equally near the main effort go to the one on the side's own left as it drew up (nearestEnemy).
    const f0 = side === 'att' ? 0 : Math.PI, cs = Math.cos(f0), sn = Math.sin(f0);
    const spread = BIG_GUNS.has(c) && this.style(side).spread > 0;
    const shooters = (e: SeaUnit): number => {
      let n = 0;
      for (const f of this.units) if (f !== u && f.side === side && live(f) && BIG_GUNS.has(f.extra.cls) && f.extra.target === e.id) n++;
      return n;
    };
    const classes = SEA.jobs.fire[c];
    if (this.screen.has(u.id) && !this.broken) {
      // The screen fires first on an enemy destroyer piece making a run, the nearest; then on the destroyers nearest its
      // own big ships; then as its job has it.
      const dds = this.units.filter((e) => e.side !== side && e.extra.cls === 'dd' && live(e) && e.state !== 'routing' && dist(u.x, u.y, e.x, e.y) <= R);
      const running = dds.filter((e) => this.runs.get(e.id)?.phase === 'in');
      const bigs = this.units.filter((f) => f.side === side && f.extra.cls !== 'dd' && live(f));
      const by = (us: SeaUnit[], d: (e: SeaUnit) => number): number => us.map((e) => ({ id: e.id, d: Math.round(d(e)) })).sort((a, b) => a.d - b.d || a.id - b.id)[0].id;
      if (running.length) return by(running, (e) => dist(u.x, u.y, e.x, e.y));
      // (With no big ship of its own left, the destroyer nearest it.)
      if (dds.length) return by(dds, (e) => (bigs.length ? Math.min(...bigs.map((f) => dist(f.x, f.y, e.x, e.y))) : dist(u.x, u.y, e.x, e.y)));
    }
    for (const breaking of [false, true]) {
      for (const tc of classes) {
        const pool = this.units.filter((e) => e.side !== side && e.extra.cls === tc && live(e) && (breaking || e.state !== 'routing') && dist(u.x, u.y, e.x, e.y) <= R);
        if (!pool.length) continue;
        const ranked = pool.map((e) => ({ id: e.id, n: spread ? shooters(e) : 0, d: Math.round(dist(e.x, e.y, mx, my)), l: (e.x - mx) * cs - (e.y - my) * sn }))
          .sort((a, b) => a.n - b.n || a.d - b.d || a.l - b.l || a.id - b.id);
        // Its own target counts it among its shooters: it moves only to one with two fewer on it than that.
        if (spread) { const cur = ranked.find((r) => r.id === u.extra.target); if (cur && cur.n + 1 - ranked[0].n < 2) return cur.id; }
        return ranked[0].id;
      }
    }
    return -1;
  }

  /** A side's style (ruling 239): methodical for every side until the ticket carries each side's. */
  private styleName(side: Side): SeaStyle { void side; return 'methodical'; }
  private style(side: Side): StyleNumbers { return SEA.styles[this.styleName(side)]!; }

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

  /** Whether a gun ship keeps with its fleet until the guns meet: at cruising speed, fighting at will or under an
   *  order to attack (a ship the player has sent at a speed of its own, or on a course, goes as told, and a destroyer
   *  piece whose torpedo run has opened goes in: R4). */
  private keepsStation(e: SeaUnit): boolean {
    if (!live(e) || e.state === 'routing' || e.extra.cls === 'cv' || e.extra.speed !== 'cruise' || this.runs.has(e.id)) return false;
    const o = this.orders.get(e.id);
    return !o || o.kind === 'attack';
  }

  /** Whether a side has no gun ship left in the fight: only carriers, or ships breaking off. */
  private gunless(side: Side): boolean {
    return !this.units.some((e) => e.side === side && live(e) && e.state !== 'routing' && e.extra.cls !== 'cv');
  }

  /** How far toward the enemy a ship stands: y for the attacker, -y for the defender. */
  private ahead(e: SeaUnit): number { return e.side === 'att' ? e.y : -e.y; }

  /** A side's gun ships keeping with its fleet: the cruising speed of the slowest of them, and how far toward the enemy
   *  the rearmost of them stands (see ahead). Null when none keeps station: before the guns meet a fleet keeps together
   *  only to meet a gun line, so not once either side has broken off (the chase runs at full speed) and not against
   *  an enemy with no gun ship left in the fight (the screen runs carriers down at its own speed). */
  private keeping(side: Side): { slowest: number; rear: number } | null {
    if (this.broken !== null || this.contactAt !== null || this.gunless(other(side))) return null;
    let slowest = Infinity, rear = Infinity;
    for (const e of this.units) {
      if (e.side !== side || !this.keepsStation(e)) continue;
      slowest = Math.min(slowest, SEA.classes[e.extra.cls as SeaClass].speed);
      rear = Math.min(rear, this.ahead(e));
    }
    return Number.isFinite(slowest) ? { slowest, rear } : null;
  }

  /** The speed a ship makes until the guns meet if it is keeping with its fleet, else null: for the screens' estimate
   *  of when the action begins. */
  fleetSpeed(u: SeaUnit): number | null {
    const line = this.keepsStation(u) ? this.keeping(u.side) : null;
    return line ? line.slowest * SEA.speeds.cruise : null;
  }

  // ---- Ship handling -------------------------------------------------------------------------------------------

  private steer(u: SeaUnit): void {
    const C = SEA.classes[u.extra.cls as SeaClass], F = SEA.field;
    const o = this.orders.get(u.id) ?? FREE;
    let want = u.face, speed = C.speed * SEA.speeds[u.extra.speed], foeId = -1;
    this.away[u.id] = false;
    // A piece on a run that turns for home, or that the player sends elsewhere, holds or withdraws, is off its run: the
    // branches below never reach runCourse, so the run ends here as runCourse would end it.
    const run0 = this.runs.get(u.id);
    if (run0 && (u.state === 'routing' || this.toldOff(o, run0))) this.endRun(u, run0, run0.phase !== 'in');
    const home = u.side === 'att' ? Math.PI : 0;
    if (u.state === 'routing' || o.kind === 'leave') {
      want = this.breakOffCourse(u, home);
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
      // A ship fighting at will, or under a fleet order, steers the same way for its fleet's main effort: ships far from
      // the fight close on it, as the land thread found idle reserves must (b407).
      // A destroyer piece on a torpedo run goes in at flank and turns away (runCourse); a screen piece keeps its station
      // on the big ship it guards, or goes for an enemy run near it (station).
      const run = u.extra.cls === 'dd' ? this.runCourse(u, o) : null;
      const post = run ?? (this.screen.has(u.id) ? this.station(u, o) : null) ?? this.lineHelm(u);
      if (post) { want = post.want; speed = post.speed; } else {
        const tgt = this.byId.get(o.kind === 'attack' && !o.fleet ? o.target : this.focus[u.side]) ?? null;
        const near = this.steerFoe(u);
        // A carrier keeps its standoff from the nearest enemy whatever it strikes: its aircraft reach any target.
        const foe = u.extra.cls !== 'cv' && tgt && (!near || dist(u.x, u.y, tgt.x, tgt.y) > dist(u.x, u.y, near.x, near.y) + SEA.ai.beyond) ? tgt : near;
        if (foe) { want = this.engage(u, foe); foeId = foe.id; }
      }
      // A carrier group that sees a battle line coming turns away and opens the range while it launches (b389), an
      // attack order or not. Its screen turns with it only within evade.screen metres of a carrier, and that is 0: the
      // screen stays and fights (b402: a screen that ran with its carriers left the plain order short of the card).
      // Only an order to steer, hold or withdraw takes a ship away.
      const away = this.groupEvasion(u, false);
      if (away) { want = away.course; speed = Math.min(C.speed * SEA.speeds.flank, away.speed); foeId = -1; this.away[u.id] = true; }
      // A beaten enemy is run down at full speed while there is time.
      // The player's ships too, until the player gives an order of their own (b402: the newcomer's chase is the staff's).
      if (this.broken && this.broken !== u.side && !this.chaseTaken[u.side]) speed = C.speed * SEA.speeds.flank;
      // Carriers with no gun ship left to stand between them and the enemy's guns are run down at full speed too, by
      // every gun ship not sent at a speed of its own: at cruising speed a battle line never came up with carriers
      // opening the range at theirs, so equal weight of unscreened carriers won every time either way round (b415).
      else if (u.extra.cls !== 'cv' && u.extra.speed === 'cruise' && this.gunless(other(u.side))) speed = C.speed * SEA.speeds.flank;
      // Until the guns first meet, the fleet keeps together (the historian: fleet speed was the slowest heavy ship's, and
      // the screen kept station on the battle line). Each at its own speed, a battle line reached the fight a minute
      // after its screen and lost to a light fleet of the same weight 95 times in 100 (b407, ruling 217); with the
      // screen 600 m ahead of it, still 13 in 100 short. So the screen closes up on the line: a ship more than
      // 'station' ahead of the rearmost slows to half the fleet's speed until the line comes up (never a dead stop).
      // A ship the player has sent at a speed of its own goes at it. The line holds its fleet's course too, from the
      // middle of its own fleet to the middle of the enemy's, so it comes on as it drew up: each steering for the enemy
      // nearest it, the wings turned in toward the middle and the line came on bunched (b438).
      const line = this.keepsStation(u) ? this.keeping(u.side) : null;
      if (line) {
        const [ax, ay] = this.centre(u.side), [bx, by] = this.centre(other(u.side));
        want = bearing(ax, ay, bx, by);
        foeId = -1;
        speed = Math.min(speed, line.slowest * SEA.speeds.cruise);
        if (this.ahead(u) - line.rear > SEA.ai.station) speed = Math.min(speed, line.slowest * SEA.speeds.slow);
      }
    }
    // Stay on the field unless leaving it.
    if (u.state !== 'routing' && o.kind !== 'leave') {
      const m = 800;
      if (u.x < m || u.x > F.size - m || u.y < m || u.y > F.size - m) { want = bearing(u.x, u.y, F.size / 2, F.size / 2); foeId = -1; }
    }
    // Way is gained and lost slowly; the helm answers better with way on.
    const acc = SEA.speeds.accel * DT;
    u.extra.way = u.extra.way < speed ? Math.min(speed, u.extra.way + acc) : Math.max(speed, u.extra.way - acc);
    const helm = C.turn * (0.25 + 0.75 * Math.min(1, u.extra.way / C.speed)) * DT;
    const swing = clamp(turn(u.face, want), -helm, helm);
    this.swing[u.id] = helm > 0 ? Math.abs(swing) / (C.turn * DT) : 0;
    this.helmed[u.id] = helm > 0 ? swing / (C.turn * DT) : 0;
    this.steered[u.id] = foeId;
    u.face = norm(u.face + swing);
    u.x += Math.sin(u.face) * u.extra.way * DT;
    u.y += Math.cos(u.face) * u.extra.way * DT;
    // Out by its own edge: gone home, to fight another day.
    if ((u.state === 'routing' || o.kind === 'leave') && (u.side === 'att' ? u.y < 300 : u.y > F.size - 300)) {
      u.state = 'withdrawn';
      this.say({ what: 'withdrawn', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'have broken off and make' : 'has broken off and makes'} for port.` });
    }
  }

  // ---- The battle line ---------------------------------------------------------------------------------------------

  /** Whether a piece sails in its side's battle line: a battleship or cruiser fighting at will or under a fleet order, at
   *  cruising speed, not breaking off. A piece the player sends at a target, a point or a speed of its own leaves it. */
  private inLine(u: SeaUnit): boolean {
    if (!live(u) || u.state === 'routing' || (u.extra.cls !== 'bb' && u.extra.cls !== 'ca') || u.extra.speed !== 'cruise') return false;
    const o = this.orders.get(u.id);
    return !o || o.kind === 'free' || (o.kind === 'attack' && !!o.fleet);
  }

  /** The middle of a set of pieces, by their weight. */
  private middle(us: SeaUnit[]): [number, number] {
    let x = 0, y = 0, w = 0;
    for (const u of us) { const k = this.weightOf(u) + 1e-6; x += u.x * k; y += u.y * k; w += k; }
    return [x / w, y / w];
  }

  /** The line's course: square to the bearing of the enemy's gun ships on its hand, bent toward them while they lie
   *  beyond its best range and away while they lie within it (line.bend at the guns' reach). */
  private lineCourse(members: SeaUnit[], foes: SeaUnit[], hand: number): number {
    const [mx, my] = this.middle(members), [ex, ey] = this.middle(foes), C = SEA.classes.bb;
    const b = bearing(mx, my, ex, ey), d = dist(mx, my, ex, ey);
    const bend = clamp((d - C.best) / (C.reach - C.best), -1, 1) * SEA.line.bend;
    return norm(b + hand * (Math.PI / 2 - bend));
  }

  /** The pieces in order along a course, the foremost first. */
  private alongCourse(us: SeaUnit[], course: number): number[] {
    const cs = Math.sin(course), cc = Math.cos(course);
    return us.map((u) => ({ id: u.id, a: u.x * cs + u.y * cc })).sort((a, b) => b.a - a.a || a.id - b.id).map((r) => r.id);
  }

  /**
   * Each side's battle line this tick (the ship plan's R1; ruling 239's order at 23:20Z; the historian: a fleet deployed
   * from its cruising order into line ahead as the enemy came in sight, every broadside clear of the next ship, and
   * steered to cross the enemy's T: Jutland, Surigao Strait), planned for both sides at the start of the tick from where
   * the ships lay at the end of the last, so neither side sees the other's move first. Once the guns have met, a side's
   * battleships and cruisers fighting at will or under a fleet order turn together onto one course and keep it at the
   * speed of the slowest of them: the rows they drew up in become columns in line ahead, as long as the rows were wide
   * (b438), the foremost along the course the van. The course is square to the bearing of the enemy's line (its
   * battleships and cruisers), bent toward it while it lies beyond best range and away while within it (lineCourse: so
   * the rear row comes into reach), on the hand the enemy's line is moving across that bearing, so the line keeps level
   * with its van and bends across its head as it draws ahead (crossing the T). With the enemy coming straight on it
   * turns to starboard, as the enemy's captains do: the field is the same turned half round for either side (b402), and
   * the lines pass on opposite courses and wheel to keep each other abeam. It turns together onto the other hand when its
   * van's course would take it off the field (line.edge), and when its van steers at an enemy line whose broadsides bear
   * on it (its T crossed: line.crossed), as a line being crossed turned away together. There is no line against
   * destroyers alone (the gun ships fight each for itself, as before), and none in a carrier group without battleships
   * (its cruisers stay with its carriers: R3 places them). It keeps until a side breaks off or the enemy has no gun ship
   * left, when the chase runs as before. No dice.
   */
  private planLines(): void {
    const sides: Side[] = ['att', 'def'];
    for (const side of sides) this.planLine(side);
    // Its T crossed: judged for both sides on the plans just made, then turned.
    const crossed = sides.map((side) => this.crossed(side));
    sides.forEach((side, i) => { if (crossed[i] !== 0 && crossed[i] !== this.lines[side]!.hand) this.turnTogether(side, crossed[i], 'crossed'); });
  }

  /** A side's line this tick: formed, its pieces still in it, its course and speed, turned off the field's edge. */
  private planLine(side: Side): void {
    const P = this.linePlans[side];
    P.t = this.t;
    P.members = [];
    if (this.contactAt === null || this.broken !== null || this.gunless(other(side))) return;
    // A carrier group without battleships keeps its cruisers with its carriers: they form no line (the historian: carrier
    // forces' cruisers screened them and shunned a surface action, Samar, October 1944). The ship plan's R3 places them.
    const mine = this.units.filter((u) => u.side === side && live(u));
    if (mine.some((u) => u.extra.cls === 'cv') && !mine.some((u) => u.extra.cls === 'bb' && u.state !== 'routing')) return;
    const foes = this.lineFoes(side);
    const members = this.units.filter((u) => u.side === side && this.inLine(u));
    if (!members.length || !foes.length) return;
    const N = SEA.line;
    let L = this.lines[side];
    if (!L) {
      // The hand: the way the enemy's line is moving across the bearing to it; coming straight on, to starboard, as the
      // enemy's own captains turn (the field is the same turned half round for either side: b402).
      const [mx, my] = this.middle(members), [ex, ey] = this.middle(foes), b = bearing(mx, my, ex, ey);
      let vx = 0, vy = 0;
      for (const e of foes) { vx += Math.sin(e.face) * e.extra.way; vy += Math.cos(e.face) * e.extra.way; }
      const lat = (vx * Math.cos(b) - vy * Math.sin(b)) / foes.length;
      const hand = Math.abs(lat) > N.across ? Math.sign(lat) : 1;
      L = { order: [], hand, course: this.lineCourse(members, foes, hand), speed: 0, turned: this.t };
      this.lines[side] = L;
      this.mark(`${side}:lineFormed`, 1);
      this.say({ what: 'sea:line', unit: members[0].id, side, text: '', extra: { why: 'form', hand, course: Math.round(L.course * 100) / 100 } });
    }
    L.course = this.lineCourse(members, foes, L.hand);
    L.order = this.alongCourse(members, L.course);
    L.speed = Math.min(...members.map((u) => SEA.classes[u.extra.cls as SeaClass].speed)) * SEA.speeds.cruise;
    P.members = L.order.map((id) => this.byId.get(id)!);
    // Off the field's edge it turns together onto the other hand, the rear then the van.
    const g = P.members[0], F = SEA.field.size, m = N.margin;
    const ax = g.x + Math.sin(L.course) * N.edge, ay = g.y + Math.cos(L.course) * N.edge;
    if (this.t - L.turned >= N.settle && (ax < m || ax > F - m || ay < m || ay > F - m)) this.turnTogether(side, -L.hand, 'edge');
  }

  /** The enemy's battleships and cruisers still fighting: the line a line deploys against. Against destroyers alone
   *  there is none: the gun ships fight each for itself, as before. */
  private lineFoes(side: Side): SeaUnit[] {
    return this.units.filter((e) => e.side !== side && live(e) && e.state !== 'routing' && (e.extra.cls === 'bb' || e.extra.cls === 'ca'));
  }

  /** Whether a side's line has its T crossed: its guide steams within line.crossed of the bearing of an enemy line whose
   *  course lies square to its own bearing to it, so the enemy's broadsides bear and its own guns end-on. The hand to
   *  turn away together onto, the one that runs with the enemy's line rather than into it; 0 when not crossed. */
  private crossed(side: Side): number {
    const L = this.lines[side], them = this.lines[other(side)];
    const ours = this.linePlans[side].members, theirs = this.linePlans[other(side)].members;
    if (!L || !them || this.t - L.turned < SEA.line.settle || !ours.length || !theirs.length) return 0;
    const N = SEA.line, g = ours[0], [mx, my] = this.middle(ours), [ex, ey] = this.middle(theirs);
    const broadside = Math.abs(Math.sin(turn(them.course, bearing(ex, ey, mx, my)))) > Math.cos(N.crossed);
    if (!broadside || Math.abs(turn(g.face, bearing(g.x, g.y, ex, ey))) >= N.crossed) return 0;
    const foes = this.lineFoes(side);
    const off = (h: number): number => Math.abs(turn(this.lineCourse(ours, foes, h), them.course));
    return off(1) <= off(-1) ? 1 : -1;
  }

  /** A side's line turns together onto a hand: every piece steers the new course at once, so the rear leads. */
  private turnTogether(side: Side, hand: number, why: string): void {
    const L = this.lines[side], members = this.linePlans[side].members;
    if (!L || !members.length) return;
    L.hand = hand;
    L.course = this.lineCourse(members, this.lineFoes(side), hand);
    L.order = this.alongCourse(members, L.course);
    this.linePlans[side].members = L.order.map((id) => this.byId.get(id)!);
    L.turned = this.t;
    this.mark(`${side}:lineTurn:${why}`, 1);
    this.say({ what: 'sea:line', unit: members[0].id, side, text: '', extra: { why, hand, course: Math.round(L.course * 100) / 100 } });
  }

  /** The course and speed of a piece in its side's battle line, or null when it is in none: the line's course at the
   *  cruising speed of its slowest piece, so its rows keep together as columns in line ahead. A piece that has strayed
   *  more than ai.keep metres from the middle of the line works back toward it at up to flank. */
  private lineHelm(u: SeaUnit): { want: number; speed: number } | null {
    if (u.extra.cls !== 'bb' && u.extra.cls !== 'ca') return null;
    const P = this.linePlans[u.side], L = this.lines[u.side];
    if (!L || P.t !== this.t || !P.members.includes(u)) return null;
    const C = SEA.classes[u.extra.cls as SeaClass];
    const [cx, cy] = this.middle(P.members), gap = dist(u.x, u.y, cx, cy);
    if (gap <= SEA.ai.keep) return { want: L.course, speed: L.speed };
    const pull = Math.min(0.6, (gap - SEA.ai.keep) / SEA.ai.keep), back = turn(L.course, bearing(u.x, u.y, cx, cy));
    return { want: norm(L.course + pull * back), speed: Math.cos(back) > 0 ? C.speed * SEA.speeds.flank : L.speed };
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

  /** The course that brings a piece into reach with its broadside to the enemy, and opens the range when it is too near. */
  private engage(u: SeaUnit, foe: SeaUnit): number {
    const C = SEA.classes[u.extra.cls as SeaClass];
    const d = dist(u.x, u.y, foe.x, foe.y), b = bearing(u.x, u.y, foe.x, foe.y);
    const want = u.extra.cls === 'cv' ? (C.standoff ?? 2500) : C.best;
    let off: number;
    // A carrier keeps behind its screen, out of the destroyers' reach, and its aircraft do its fighting.
    if (u.extra.cls === 'cv') off = d < want ? 2.6 : d > want * 1.3 ? 0.6 : Math.PI / 2;
    else if (d > C.reach) off = 0.15;
    // Within 'broadside' metres of the enemy it is steering for (the nearest, or its target: see steer) a ship turns
    // its guns' arcs onto it: closing on to its best range left the ships behind their own screen end-on for the
    // whole fight (b407).
    else if (d > SEA.ai.broadside) off = 0.6;
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

  /**
   * The course of a ship breaking off (the ship plan's R5): away from the enemy nearest her that is still fighting,
   * bent for home, so she opens the range as she goes rather than steaming due north or south across the enemy's bows
   * (Jutland, May 1916: the High Seas Fleet's battle turns away). It is the sum of the two directions,
   * home and away from that enemy, so it never runs toward her own side's far edge; by the field's side edges it holds
   * for home. With no enemy fighting, or that enemy dead ahead on her course home, she steers for home.
   */
  private breakOffCourse(u: SeaUnit, home: number): number {
    let foe: SeaUnit | null = null, fd = Infinity;
    for (const e of this.units) {
      if (e.side === u.side || !live(e) || e.state === 'routing') continue;
      const d = dist(u.x, u.y, e.x, e.y);
      if (d < fd) { fd = d; foe = e; }
    }
    if (!foe || fd <= 0) return home;
    const F = SEA.field.size, m = 800;
    let x = Math.sin(home) + (u.x - foe.x) / fd;
    const y = Math.cos(home) + (u.y - foe.y) / fd;
    if ((u.x < m && x < 0) || (u.x > F - m && x > 0)) x = 0;
    return Math.hypot(x, y) < 1e-9 ? home : Math.atan2(x, y);
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
    if (u.extra.cls === 'cv') return;
    // A destroyer piece whose guns have nothing in reach banks their weight for its torpedoes (store). On a run its guns
    // keep firing at whatever their job finds in reach, so a run takes no weight out of the fight (ruling 239).
    if (u.extra.target < 0) { this.store(u); return; }
    const foe = this.byId.get(u.extra.target);
    if (!foe || !live(foe)) { u.extra.target = -1; this.store(u); return; }
    const C = SEA.classes[u.extra.cls as SeaClass];
    const d = dist(u.x, u.y, foe.x, foe.y);
    if (d > C.reach) { this.store(u); return; }
    const range = this.reachFactor(C, d);
    const arc = SEA.fire.endOn + (1 - SEA.fire.endOn) * Math.abs(Math.sin(turn(u.face, bearing(u.x, u.y, foe.x, foe.y))));
    const nerve = u.state === 'wavering' ? SEA.nerve.wavering : u.state === 'routing' || this.orders.get(u.id)?.kind === 'leave' ? 0.5 : 1;
    if (this.contactAt === null) { this.contactAt = this.t; this.say({ what: 'contact', unit: u.id, side: u.side, text: 'The fleets are in range of each other.' }); }
    u.engaged = foe.id;
    // One punch per unit of weight whatever the class (b415): the card already prices a battleship above a cruiser.
    let w = SEA.fire.rate * this.output(u) * range * arc * nerve * this.day(u.side) * DT;
    // On a torpedo run its guns bear end-on going in and stern-on coming out: the weight they cannot fire is the bank's
    // (store), and pays its own salvo's advance first (repay; the design director, 08:10Z).
    if (this.runs.has(u.id) && arc < 1) this.store(u, SEA.fire.rate * this.output(u) * range * (1 - arc) * nerve * this.day(u.side) * DT);
    // What its guns denied of a strike comes out of its next gunfire first (cap): the trade is weight for weight.
    if (this.owe[u.id] > 0) { const p = Math.min(this.owe[u.id], w); this.owe[u.id] -= p; w -= p; this.mark(`${u.side}:paid`, p); }
    // Its side's guns pay for salvoes that landed before the guns had earned them, weight for weight (loaded, advance).
    // As it is paid, what those salvoes took out of the enemy counts, part for part (unpaid).
    if (this.advance[u.side] > 0) {
      const a = this.advance[u.side], p = Math.min(a, w);
      this.unpaid[other(u.side)] = p >= a ? 0 : this.unpaid[other(u.side)] * (1 - p / a);
      this.advance[u.side] -= p; w -= p; this.mark(`${u.side}:salvoPaid`, p);
    }
    // Gunfire, like a strike, takes the same weight out of the fight whatever it falls on (wear in proportion to the
    // ship's hull): a heavy fleet's light screen is no weaker for being light, and a fleet is worth what the campaign
    // counts it, whatever its mix (the second playtester's newcomer test, b402).
    w = this.take(foe, w);
    this.dealt[u.id] += w;
    this.inReach[u.id] += DT;
    this.mark(`${u.side}:${u.extra.cls}>${foe.extra.cls}`, w);
    if (u.extra.cls === 'dd' && (foe.extra.cls === 'bb' || foe.extra.cls === 'cv')) { this.mark(`${u.side}:ddBig:s`, DT); if (!this.runs.has(u.id)) { this.mark(`${u.side}:ddBigOff:s`, DT); if (d > this.style(u.side).press) this.mark(`${u.side}:ddBigFar:s`, DT); } }
    if (u.extra.cls === 'dd') this.mark(`${u.side}:ddGun:s`, DT);
    if (this.screen.has(u.id)) { const r = this.runs.get(foe.id); if (r?.phase === 'in') r.shot = true; }
  }

  /**
   * A blow of weight 'w' on a side wears the whole of what the side brought, by weight, as the card counts it (b415):
   * the ship it fell on takes the gun ships' share, each carrier its own share and the coasts' aircraft theirs. A
   * carrier is a ship to the card, so its share wears as a hull does ('fire.hull'), but the loss falls on its air
   * group and it stays afloat, out of the guns' reach. The coasts' aircraft have no hull: a blow takes its whole weight
   * from them. Kept whole while the guns fought, carriers and the coasts' aircraft were worth more the longer the gun
   * fight ran (a fifth of the card one way or the other with the fleets' mix); worn with the fleet, they are worth
   * what the card prices them. A blow that falls on a carrier itself falls on its hull. A side that owes for hull its
   * blows put out of the line (debt, fallOut) pays that first; what is left of the blow lands, and is what this returns.
   */
  private take(foe: SeaUnit, w: number): number {
    const by = other(foe.side);
    if (this.debt[by] > 0) { const p = Math.min(this.debt[by], w); this.debt[by] -= p; w -= p; this.mark(`${by}:debtPaid`, p); if (w <= 0) return 0; }
    if (foe.extra.cls === 'cv') { this.wear(foe, this.evenWear(foe, w)); return w; }
    const s = foe.side;
    let ships = 0, total = this.airLeft[s];
    for (const u of this.units) if (u.side === s && live(u)) { const x = this.weightOf(u); total += x; if (u.extra.cls !== 'cv') ships += x; }
    if (total <= 0) return w;
    this.wear(foe, this.evenWear(foe, (w * ships) / total));
    for (const u of this.units) {
      if (u.side !== s || !live(u) || u.extra.cls !== 'cv') continue;
      const whole = this.info.get(u.id)!.weight * u.extra.ships0 * u.str;
      if (whole > 0) this.deck[u.id] = Math.max(0, this.deck[u.id] - (w * this.weightOf(u)) / total / SEA.fire.hull / whole);
      u.extra.planes = this.deck[u.id];
    }
    this.airLeft[s] = Math.max(0, this.airLeft[s] * (1 - w / total));
    return w;
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
    this.fallOut(u);
  }

  /**
   * A ship worn to side.hurt of her hull or less leaves the line burning and makes for port (the ship plan's R2; the
   * design director, 01:36Z 21 November). The record: a captain took his ship out of the line when her steering was
   * gone, she was down a quarter to a third of her speed, or about half her main battery or its fire control was out
   * (Hiei, Lion at the Dogger Bank, Exeter, Prince of Wales, South Dakota); hits and fire alone never sent her out
   * (Seydlitz kept her place). The field's guns wear with the hull (fire.damaged), so half her hull is half her main
   * battery, and side.hurt is the rules' own "badly hurt" (b458). Her own hull is what is left of the piece's damaged
   * ship; the rest of the piece fights on. Whether she sinks is the rules' outcome, as for any ship lost to the fight
   * (loseShip: the dice's limit by class). The hull she takes with her is owed (debt) by the side whose blow put her
   * out, paid out of its next blows, and counts toward her side's loss only as it is paid: a blow takes out its own
   * weight whatever it falls on (b418, b438), so concentrated fire gets ships out of the line sooner and takes no more
   * weight out of the fight, and the break and the odds the player is shown read only weight dealt. What is unpaid when
   * the action ends counts on neither side: she is a damaged ship home, not a loss (the design director, 02:15Z and
   * 03:49Z). No dice, no new number.
   */
  private fallOut(u: SeaUnit): void {
    const n0 = u.extra.ships0, hurt = SEA.side.hurt;
    while (live(u) && u.extra.ships > 0) {
      const own = u.str * n0 - (u.extra.ships - 1);
      if (own <= 1e-9 || own > hurt + 1e-9) return;
      u.str = (u.extra.ships - 1) / n0;
      this.debt[other(u.side)] += this.info.get(u.id)!.weight * own * this.deck[u.id] * SEA.fire.hull;
      this.loseShip(u, own);
    }
  }
  /**
   * Weight a side owes for the hull its blows put out of the line with the ships they hurt (fallOut), in the blows' own
   * measure: paid out of its next blows, weight for weight, as owed flak is. Until paid it counts on neither side.
   */
  private readonly debt: Record<Side, number> = { att: 0, def: 0 };

  /**
   * Where a ship lost leaves her smoke, for the 3D game (the art director's grit direction, 19 November: black oil smoke
   * from burning ships, standing to the end of the fight, as the burning wrecks on land do): a zone at her place in her
   * piece's line, from this second to the end. 'sunk' is the oil burning on the water where she went down; 'burning' is
   * where she left the line on fire, making for port. Drawn only: nothing in the rules reads it, and it draws no dice.
   */
  private smoke(u: SeaUnit, kind: 'sunk' | 'burning'): void {
    const n = this.zones.filter((z) => z.unit === u.id).length, F = SEA.field.size;
    const along = (mix(u.id * 613 + n * 389 + 3) - 0.5) * u.d * 0.8;
    const x = clamp(u.x + along * Math.sin(u.face), 2, F - 2), y = clamp(u.y + along * Math.cos(u.face), 2, F - 2);
    this.zones.push({
      id: ++this.zoneId, kind, x: Math.round(x), y: Math.round(y), r: Math.round(SEA.classes[u.extra.cls as SeaClass].len * 0.6),
      until: this.ticket.limit, side: u.side, at: Math.round(this.t * 10) / 10, unit: u.id,
    });
  }

  /**
   * One ship of a piece goes: down, or out of the fight burning once its side has lost all the dice allow of its class.
   * hull is her own hull as she went (0 to 1), told on her event for the 3D game: 0 for a ship worn through, what she
   * had left for one that fell out of the line (fallOut), so the scene does not draw the hull she takes with her as
   * shell hits (the 3D sea scene thread, 05:04Z 21 November). Drawn only: nothing in the rules reads it.
   */
  private loseShip(u: SeaUnit, hull = 0): void {
    const c = u.extra.cls as SeaClass, side = u.side, fleet = this.info.get(u.id)!.fleet.id;
    u.extra.ships--;
    const one = RULES_CLASSES[c].one, left = Math.round(clamp(hull, 0, 1) * 1000) / 1000;
    if (this.lostShips[side][c] < this.caps[side][c]) {
      this.lostShips[side][c]++;
      u.extra.sunk++;
      this.fleetLoss.sunk[fleet][c]++;
      this.say({ what: 'sea:sunk', unit: u.id, side, text: `A ${one} ${ofFleet(u.extra.fleet)} goes down.`, extra: { cls: c, fleet, hull: left } });
      this.smoke(u, 'sunk');
      // A ship going down shakes the pieces near it by the weight it carried, measured against its side's: a destroyer
      // lost shakes less than a battleship, and a fleet of many light ships no more for the same weight lost (b438).
      const shake = (SEA.nerve.near * this.info.get(u.id)!.weight) / Math.max(1e-6, this.weight0[side]);
      for (const f of this.units) if (f.side === side && f.id !== u.id && live(f) && dist(f.x, f.y, u.x, u.y) <= SEA.nerve.nearR) f.mor = Math.max(0, f.mor - shake);
    } else {
      this.fleetLoss.burnt[fleet][c]++;
      u.extra.burnt = ((u.extra.burnt as number | undefined) ?? 0) + 1;
      // Her course home, for the 3D game to draw her limping away from her place in the line (the ship plan's L4): her
      // side's own edge, give or take a hash of the piece and how many it has lost so (no dice), at a fraction of cruising.
      const L = SEA.look, n = u.extra.burnt as number;
      const limp = { course: Math.round(norm((side === 'att' ? Math.PI : 0) + (mix(u.id * 733 + n) - 0.5) * L.limpSpread) * 100) / 100, speed: Math.round(SEA.classes[c].speed * SEA.speeds.cruise * L.limpSpeed * 10) / 10 };
      this.say({ what: 'sea:burning', unit: u.id, side, text: `A ${one} ${ofFleet(u.extra.fleet)} is out of the fight, burning, and makes for port.`, extra: { cls: c, fleet, limp, hull: left } });
      this.smoke(u, 'burning');
    }
    if (u.extra.ships <= 0) {
      // A piece whose last ship went out of the fight burning has left the field; one whose last ship sank is gone.
      u.state = u.extra.sunk >= u.extra.ships0 || !(u.extra.burnt as number | undefined) ? 'gone' : 'withdrawn';
      u.str = u.state === 'gone' ? 0 : u.str;
      // Off any run it was on: only a piece afloat steers, so nothing else ends it (the recording reads extra.run).
      this.runs.delete(u.id); delete u.extra.run;
      // A lone ship's line has just been said; a piece of several gets one line for the last of them.
      const all = `All ${u.extra.ships0} ${RULES_CLASSES[c].many} ${ofFleet(u.extra.fleet)}`;
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
      // Its aircraft fly home and land on as long again after (drawn only: see recover).
      this.recovers.push({ at: this.t + SEA.strike.windup, cv: s.cv, side: s.side });
      const foe = this.byId.get(s.target);
      const at = foe && live(foe) ? foe : this.heaviest(other(s.side));
      if (!at) continue;
      const landed = this.meet(at, s.weight, s.side, true);
      const got = this.splash(at, landed, s.side);
      this.fromAir[`${s.side}:cv`] = (this.fromAir[`${s.side}:cv`] ?? 0) + got;
      const text = this.turnedBack(s.count ? `${s.count} bombers from ${theFleet(s.from)}` : `aircraft from ${theFleet(s.from)}`, at, 'find')
        ?? `${s.count ? `${headCount(s.count)} bombers` : 'Aircraft'} from ${theFleet(s.from)} find ${this.told(at)}.`;
      this.say({ what: 'sea:strike', unit: at.id, side: s.side, text, extra: { count: s.count, flown: s.weight, landed } });
    }
  }

  /**
   * A strike's aircraft back over their carrier and landing on, 'windup' seconds after they found their target: an event
   * for the 3D game's deck cycle (the ship plan's L2), with no words and no sound of its own until the UI and sound
   * directors give them. 'ditch' when the carrier has gone down meanwhile, and the aircraft come down in the sea. Nothing
   * in the rules reads it: the carrier's next strike waits on its own 'every', as before.
   */
  private recover(): void {
    if (!this.recovers.some((r) => r.at <= this.t)) return;
    const due = this.recovers.filter((r) => r.at <= this.t);
    this.recovers = this.recovers.filter((r) => r.at > this.t);
    for (const r of due) this.say({ what: 'sea:recover', unit: r.cv, side: r.side, text: '', extra: { ditch: this.byId.get(r.cv)?.state === 'gone' } });
  }

  // ---- Torpedoes ---------------------------------------------------------------------------------------------------

  /**
   * The screen (torp; styles: screen): the destroyer pieces nearest the side's own big ships as the fleets draw up,
   * ties by id, no dice; at least one where the style keeps a screen and there are two or more. Each keeps station on the
   * big ship it is nearest, at its own side of it as drawn up. A fleet with no big ship keeps no screen.
   */
  private drawScreen(side: Side): void {
    const dds = this.units.filter((u) => u.side === side && u.extra.cls === 'dd');
    const bigs = this.units.filter((u) => u.side === side && u.extra.cls !== 'dd');
    const share = this.style(side).screen;
    if (!bigs.length || share <= 0) return;
    const k = Math.max(dds.length >= 2 ? 1 : 0, Math.floor(share * dds.length));
    const cs = side === 'att' ? 1 : -1;
    const near = (u: SeaUnit): SeaUnit => bigs.reduce((b, x) => (dist(u.x, u.y, x.x, x.y) < dist(u.x, u.y, b.x, b.y) - 1e-6 ? x : b));
    const ranked = dds.map((u) => ({ u, d: Math.round(dist(u.x, u.y, near(u).x, near(u).y)) })).sort((a, b) => a.d - b.d || a.u.id - b.u.id);
    for (const { u } of ranked.slice(0, k)) {
      const g = near(u);
      this.screen.add(u.id);
      u.extra.screen = true;
      this.guard.set(u.id, { ship: g.id, lat: clamp((u.x - g.x) * cs, -SEA.torp.station, SEA.torp.station) });
    }
  }

  /** Once no enemy destroyer is afloat, the screen's job is done: its pieces leave it for good and run, and fight, as any
   *  other destroyer piece does (the design director's (b), 22:57Z). No dice. */
  private freeScreen(): void {
    for (const side of ['att', 'def'] as Side[]) {
      const mine = this.units.filter((u) => u.side === side && this.screen.has(u.id));
      if (!mine.length || this.units.some((e) => e.side !== side && e.extra.cls === 'dd' && live(e))) continue;
      for (const u of mine) { this.screen.delete(u.id); this.guard.delete(u.id); delete u.extra.screen; }
      this.mark(`${side}:screenFreed`, 1);
    }
  }

  /** The big ship a screen piece guards: the one it drew up beside, or once that is gone or breaking off, its side's
   *  big ship now nearest it (ties by id); null with none left, when the piece fights as any other does. */
  private guarded(u: SeaUnit): SeaUnit | null {
    const gd = this.guard.get(u.id);
    if (!gd) return null;
    const g = this.byId.get(gd.ship);
    if (g && live(g) && g.state !== 'routing') return g;
    let best: SeaUnit | null = null, bd = Infinity;
    for (const f of this.units) {
      if (f.side !== u.side || f.extra.cls === 'dd' || !live(f) || f.state === 'routing') continue;
      const d = dist(u.x, u.y, f.x, f.y);
      if (d < bd - 1e-6) { bd = d; best = f; }
    }
    if (best) gd.ship = best.id;
    return best;
  }

  /**
   * A screen piece's course and speed (torp): for an enemy destroyer piece on a run within 'intercept' metres of it, the
   * nearest; else its station, 'station' metres ahead of the big ship it guards toward the enemy's middle, at its own
   * side of it. Null before the guns meet (the fleet keeps together), once a side has broken off, under an order of the
   * player's own, or with no big ship left to guard: it then fights as any other piece does.
   */
  private station(u: SeaUnit, o: SeaOrder): { want: number; speed: number } | null {
    if (this.contactAt === null || this.broken || !(o.kind === 'free' || (o.kind === 'attack' && o.fleet)) || !steady(u)) return null;
    const g = this.guarded(u);
    if (!g) return null;
    const T = SEA.torp, flank = SEA.classes.dd.speed * SEA.speeds.flank;
    let runner: SeaUnit | null = null, rd = T.intercept;
    for (const e of this.units) {
      if (e.side === u.side || !live(e) || this.runs.get(e.id)?.phase !== 'in') continue;
      const d = dist(u.x, u.y, e.x, e.y);
      if (d < rd) { rd = d; runner = e; }
    }
    if (runner) return { want: this.engage(u, runner), speed: flank };
    // Once its guns find a target it fights where it is, as the line's other pieces do: held on station, its weight
    // stood out of the fight (ruling 239, 22:04Z figures).
    if (u.extra.target >= 0) return null;
    const [ex, ey] = this.centre(other(u.side)), b = bearing(g.x, g.y, ex, ey), lat = this.guard.get(u.id)!.lat;
    const sx = g.x + Math.sin(b) * T.station + Math.cos(b) * lat, sy = g.y + Math.cos(b) * T.station - Math.sin(b) * lat;
    const d = dist(u.x, u.y, sx, sy);
    if (d < 200) return { want: g.face, speed: g.extra.way };
    return { want: bearing(u.x, u.y, sx, sy), speed: clamp(d / 20, g.extra.way, flank) };
  }

  /** A destroyer piece whose guns have nothing in reach banks their weight for its torpedoes (torp), from the moment the
   *  guns first meet, up to torp.bank seconds' worth: its salvo is weight its guns could not fire, and no more. */
  private store(u: SeaUnit, unfired?: number): void {
    if (u.extra.cls !== 'dd' || this.contactAt === null || !steady(u) || this.orders.get(u.id)?.kind === 'leave') return;
    const per = SEA.fire.rate * this.output(u) * this.day(u.side);
    const nerve = u.state === 'wavering' ? SEA.nerve.wavering : 1, cap = per * SEA.torp.bank;
    let add = unfired ?? per * nerve * DT;
    // On a run, what its guns cannot fire pays its own salvo's advance first (repay); the rest banks as ever.
    if (this.runs.has(u.id)) add -= this.repay(u, add);
    const into = Math.max(0, Math.min(add, cap - this.bank[u.id]));
    this.bank[u.id] = Math.min(cap, this.bank[u.id] + into);
    // With the tubes full, what the guns could not fire earns the loaded salvo down instead (loaded).
    this.loaded[u.id] = Math.min(this.bank[u.id], Math.max(0, this.loaded[u.id] - (add - into)));
  }

  /**
   * Weight a destroyer piece on a run cannot fire pays its own salvo's advance first (R4; the design director, 08:10Z:
   * the bank is the weight the guns could not fire, so a run's end-on guns are not a second cost on top of the advance):
   * before it launches, the loaded part of its salvo still unearned; once launched, the unearned part of that salvo
   * still in the water while the run lasts (the salvo is still running when the run ends: torp.travel is longer than
   * torp.away). Only the rest is owed by its side when the salvo lands (V3). Returns what it paid; store() banks the
   * rest as ever.
   */
  private repay(u: SeaUnit, w: number): number {
    let paid = 0;
    const own = Math.min(this.loaded[u.id], w);
    if (own > 0) { this.loaded[u.id] -= own; paid += own; }
    for (const s of this.torps) {
      if (paid >= w) break;
      if (s.from !== u.id || s.drawn <= 0) continue;
      const p = Math.min(s.drawn, w - paid);
      s.drawn -= p; paid += p;
    }
    if (paid > 0) this.mark(`${u.side}:salvoEarnedRun`, paid);
    return paid;
  }

  /** Whether a destroyer piece has a full salvo banked: its bank at the cap store() fills it to (R5). */
  private salvoReady(u: SeaUnit): boolean {
    const full = SEA.fire.rate * this.output(u) * this.day(u.side) * SEA.torp.bank;
    return full > 0 && this.bank[u.id] >= full * (1 - 1e-9);
  }

  /** How shaken the enemy's big ships in reach of a side's big guns are: their nerve on average, or Infinity with none. */
  private shaken(side: Side): number {
    let n = 0, m = 0;
    for (const e of this.units) {
      if (e.side === side || e.extra.cls === 'dd' || !live(e) || e.state === 'routing') continue;
      if (!this.units.some((u) => u.side === side && BIG_GUNS.has(u.extra.cls) && live(u) && u.state !== 'routing' && dist(u.x, u.y, e.x, e.y) <= SEA.classes[u.extra.cls as SeaClass].reach)) continue;
      n++; m += e.mor;
    }
    return n ? m / n : Infinity;
  }

  /**
   * Once a second: the destroyers' torpedo runs (torp), by what their crews could see, never by a clock (the ship plan's
   * R4; the design director, 06:14Z and 06:48Z). A steady destroyer piece outside the screen, free to run and with a full
   * salvo (R5), opens its run on the first of three cues:
   * - its own carriers threatened by an enemy heavy ship closing on one faster than she goes, or its battleships
   *   outnumbered by the enemy's heavy ships in gun reach of them (threats): at once, at the ship doing the threatening
   *   within torp.start, ahead of her beam or not, since its job is to turn her away (Samar, October 1944: the screen's
   *   runs early, turning Kurita's heavy ships away from the escort carriers); such a division launches at once, she
   *   being in torpedo reach (pressOf), and runs on through its own side's break (breakSide);
   * - an enemy big ship within torp.start (runTarget's: battleships, then cruisers, then carriers), the piece ahead of
   *   her beam (of the course she flees on, for a ship running for home: aheadOf): at once once the guns have met and
   *   while its own side's heavy ships are engaged with the enemy's
   *   (engaged; in daylight against an alert line the record's runs came after the guns met and while the lines were
   *   engaged; Surigao's runs before them were the night's, and the field fights no night; a destroyer-only side opens
   *   none: the design director, 08:10Z and 09:48Z);
   * - its style's torpWhen, the enemy's heavy ships in the big guns' reach shaken to that nerve, slowed by gunfire (at
   *   first contact for torpWhen 1): the cue for a piece abaft the beam, which waits for the enemy to be slowed rather
   *   than chase (North Cape, December 1943: the destroyers closed once a hit had cut Scharnhorst's speed).
   * The pieces whose runs open in the same second go as one division at one target, the one the piece nearest its own
   * target would take (a ship threatening the side's charge first), those of them in torpedo reach of her (torp.start),
   * and close in company (runCourse); a lone piece runs alone. One told on its own to attack a bigger ship runs at it.
   * A screen piece launches its bank from station at an enemy big ship within press.
   */
  private startRuns(): void {
    // The screen's job is judged once the guns have met, as before R4 (a screen piece opens nothing before then).
    if (this.contactAt !== null) this.freeScreen();
    for (const side of ['att', 'def'] as Side[]) {
      const st = this.style(side);
      const slowed = this.contactAt !== null && (st.torpWhen >= 1 || this.shaken(side) <= st.torpWhen);
      const threats = this.threats(side), engaged = this.engaged(side);
      const opening: { u: SeaUnit; foe: SeaUnit; guard: boolean }[] = [];
      for (const u of this.units) {
        if (u.side !== side || u.extra.cls !== 'dd' || u.state !== 'steady' || this.runs.has(u.id) || (this.nextRun.get(u.id) ?? 0) > this.t) continue;
        const o = this.orders.get(u.id);
        if (o && o.kind !== 'attack') continue;
        if (o && !o.fleet) {
          // The player's own order waits for the guns to meet, as before R4: the cues are the staff's.
          if (this.contactAt === null) continue;
          const foe = this.byId.get(o.target);
          if (foe && live(foe) && foe.extra.cls !== 'dd' && dist(u.x, u.y, foe.x, foe.y) <= SEA.torp.start) this.beginRun(u, foe);
          continue;
        }
        // (Before the guns meet a screen piece opens nothing, and guarded() is not asked: it may pick a new ship to guard.)
        if (this.screen.has(u.id) && !this.broken && (this.contactAt === null || this.guarded(u))) {
          if (this.contactAt === null || this.bank[u.id] <= 0) continue;
          let foe: SeaUnit | null = null, fd = st.press;
          for (const e of this.units) {
            if (e.side === side || e.extra.cls === 'dd' || !live(e) || e.state === 'routing') continue;
            const d = dist(u.x, u.y, e.x, e.y);
            if (d <= fd) { fd = d; foe = e; }
          }
          if (foe) this.launch(u, foe, null);
          continue;
        }
        // A run carries a full salvo: without one the piece chases with its guns, as destroyers that had fired off
        // their torpedoes did (the design director, 01:34Z and 04:58Z: the ship plan's R5).
        if (!this.salvoReady(u)) continue;
        let foe: SeaUnit | null = null, fd = SEA.torp.start;
        for (const e of threats) { const d = dist(u.x, u.y, e.x, e.y); if (d <= fd && (!foe || d < fd - 1e-6 || e.id < foe.id)) { fd = d; foe = e; } }
        const guard = foe !== null;
        if (!foe) {
          const t = this.runTarget(u);
          if (t && ((this.contactAt !== null && engaged && this.aheadOf(t, u)) || slowed)) foe = t;
        }
        if (foe) opening.push({ u, foe, guard });
      }
      // Those opening together go as one division at one target (B4, as written): the one the piece nearest its own
      // target would take, a ship threatening the side's charge first, so a division sent to turn her away is never
      // drawn off her.
      if (opening.length) {
        opening.sort((a, b) => Number(b.guard) - Number(a.guard) || dist(a.u.x, a.u.y, a.foe.x, a.foe.y) - dist(b.u.x, b.u.y, b.foe.x, b.foe.y) || a.u.id - b.u.id);
        // Only those in torpedo reach of her go (torp.start, as for any run; the design director, 09:48Z: "the division
        // in torpedo reach of that ship"): the others open their own runs in the next second, at a ship in their reach.
        const foe = opening[0].foe, guard = opening[0].guard;
        const go = opening.filter((x) => dist(x.u.x, x.u.y, foe.x, foe.y) <= SEA.torp.start);
        const div = go.length > 1 ? ++this.divId : 0;
        for (const x of go) this.beginRun(x.u, foe, div, guard);
        if (div) this.mark(`${side}:divisions`, 1);
      }
    }
  }
  private divId = 0;

  /** Whether a piece lies ahead of an enemy ship's beam: on her heading, or for a ship running for home (broken off, or
   *  told to withdraw), on the course she flees on (breakOffCourse), as the crews saw her turn away. A piece on her bow
   *  as she turns is astern of her within seconds, and a run begun there is a chase that never reaches press. */
  private aheadOf(e: SeaUnit, u: SeaUnit): boolean {
    if (e.state !== 'routing' && this.orders.get(e.id)?.kind !== 'leave') return aheadOfBeam(e, u);
    const flees = this.breakOffCourse(e, e.side === 'att' ? Math.PI : 0);
    return Math.abs(turn(flees, bearing(e.x, e.y, u.x, u.y))) < Math.PI / 2;
  }

  /** Whether a side's heavy ships are engaged with the enemy's: a battleship or cruiser of its own still fighting within
   *  its gun reach of an enemy battleship or cruiser, one still fighting or, once the enemy has broken off, one it is
   *  running down (the chase: the ships a run may go at, runTarget). By day a destroyer division goes in ahead of an
   *  enemy's beam only while the lines are engaged (the design director, 09:48Z: Jutland, the Java Sea, North Cape after
   *  the guns met); unsupported destroyers by day ran at a heavy line only to cover something (the Samar cue). */
  private engaged(side: Side): boolean {
    const mine = (g: SeaUnit): boolean => BIG_GUNS.has(g.extra.cls) && live(g) && g.state !== 'routing';
    const theirs = (e: SeaUnit): boolean => BIG_GUNS.has(e.extra.cls) && live(e) && (e.state !== 'routing' || this.broken === e.side);
    for (const g of this.units) {
      if (g.side !== side || !mine(g)) continue;
      const R = SEA.classes[g.extra.cls as SeaClass].reach;
      for (const e of this.units) if (e.side !== side && theirs(e) && dist(g.x, g.y, e.x, e.y) <= R) return true;
    }
    return false;
  }

  /**
   * The enemy heavy ships threatening a side's charge (R4's Samar cue): each enemy battleship or cruiser still fighting
   * that is closing on one of the side's carriers, heading at her (ahead of its beam) and going faster than she is, so
   * it will overtake her (what the crews saw as Kurita's ships came on: a carrier keeping her distance is the very
   * flight the screen covers, so gun reach is not the test: the design director, 09:48Z); or within its own gun reach
   * of one of the side's battleships
   * where the enemy's heavy ships in reach of her outnumber the side's own heavy ships in reach of them. Counts on the
   * field, no number.
   */
  private threats(side: Side): SeaUnit[] {
    const heavy = (e: SeaUnit): boolean => BIG_GUNS.has(e.extra.cls) && live(e) && e.state !== 'routing';
    const theirs = this.units.filter((e) => e.side !== side && heavy(e));
    if (!theirs.length) return [];
    const inReach = (e: SeaUnit, f: SeaUnit): boolean => dist(e.x, e.y, f.x, f.y) <= SEA.classes[e.extra.cls as SeaClass].reach;
    // Closing: the range from e to f falling, from their courses and speeds now.
    const closing = (e: SeaUnit, f: SeaUnit): boolean => {
      const rx = f.x - e.x, ry = f.y - e.y;
      const vx = Math.sin(f.face) * f.extra.way - Math.sin(e.face) * e.extra.way, vy = Math.cos(f.face) * f.extra.way - Math.cos(e.face) * e.extra.way;
      return rx * vx + ry * vy < 0;
    };
    const out = new Set<SeaUnit>();
    for (const f of this.units) {
      if (f.side !== side || !live(f) || f.state === 'routing' || (f.extra.cls !== 'cv' && f.extra.cls !== 'bb')) continue;
      // (Her own course carrying her at the carrier, ahead of her beam: a ship crossing or edging away that the carrier
      // happens to be steaming toward is not coming at her.)
      if (f.extra.cls === 'cv') { for (const e of theirs) if (e.extra.way > f.extra.way && aheadOfBeam(e, f) && closing(e, f)) out.add(e); continue; }
      const on = theirs.filter((e) => inReach(e, f));
      if (!on.length) continue;
      if (f.extra.cls === 'bb') {
        const ours = this.units.filter((g) => g.side === side && heavy(g) && on.some((e) => inReach(g, e))).length;
        if (on.length <= ours) continue;
      }
      for (const e of on) out.add(e);
    }
    return [...out];
  }

  /** The enemy big ship a destroyer piece runs at: battleships, then cruisers, then carriers, within torp.start of it,
   *  the one nearest the main effort; with spread, the one fewest runs are closing on first (a division goes at one, startRuns). Ships still fighting, except in a
   *  chase. No dice. */
  private runTarget(u: SeaUnit): SeaUnit | null {
    const side = u.side, main = this.byId.get(this.focus[side]);
    const [mx, my] = main ? [main.x, main.y] : this.centre(other(side));
    const cs = side === 'att' ? 1 : -1, spread = this.style(side).spread > 0;
    // (A runner lost on its run-in is closing on nothing; its run ends as it goes, loseShip.)
    const on = (e: SeaUnit): number => { let n = 0; for (const [id, r] of this.runs) if (r.target === e.id && r.phase === 'in' && live(this.byId.get(id)!)) n++; return n; };
    for (const tc of ['bb', 'ca', 'cv'] as SeaClass[]) {
      const pool = this.units.filter((e) => e.side !== side && e.extra.cls === tc && live(e) && (e.state !== 'routing' || this.broken === e.side) && dist(u.x, u.y, e.x, e.y) <= SEA.torp.start);
      if (!pool.length) continue;
      const ranked = pool.map((e) => ({ e, n: spread ? on(e) : 0, d: Math.round(dist(e.x, e.y, mx, my)), l: (e.x - mx) * cs }))
        .sort((a, b) => a.n - b.n || a.d - b.d || a.l - b.l || a.e.id - b.e.id);
      return ranked[0].e;
    }
    return null;
  }

  private beginRun(u: SeaUnit, foe: SeaUnit, div = 0, guard = false): void {
    this.runs.set(u.id, { target: foe.id, phase: 'in', until: 0, away: 0, face: 0, turned: 0, launched: 0, near: false, shot: false, div, guard });
    u.extra.run = foe.id;
  }

  /**
   * The course and speed of a destroyer piece on a torpedo run (torp): in at flank until its press range (pressOf), when
   * it launches; then away from the target at flank for torp.away seconds. Null when it is on none, or the run has just
   * ended: a piece that wavers on the run-in, or whose target goes, breaks it off with its bank kept; an order of the
   * player's own ends it.
   */
  private runCourse(u: SeaUnit, o: SeaOrder): { want: number; speed: number } | null {
    const run = this.runs.get(u.id);
    if (!run) return null;
    const flank = SEA.classes.dd.speed * SEA.speeds.flank;
    const told = this.toldOff(o, run);
    if (run.phase === 'in') {
      const foe = this.byId.get(run.target);
      if (!foe || !live(foe) || (foe.state === 'routing' && this.broken !== foe.side) || u.state !== 'steady' || told) { this.endRun(u, run, false); return null; }
      const d = dist(u.x, u.y, foe.x, foe.y);
      if (d > this.pressOf(u, run)) {
        // A division closes in company (R4), as the fleet keeps together in the approach: a piece more than the fleet's
        // station distance nearer the target than the division's rearmost still running in slows to half until it
        // comes up, so the division's salvoes go in together. A mate no longer steady, or told off by the player, is
        // out of the division: it is not waited for.
        let rear = d;
        if (run.div) {
          for (const [id, r] of this.runs) {
            const f = this.byId.get(id);
            if (!f || f === u || r.div !== run.div || r.phase !== 'in' || !live(f) || f.state !== 'steady' || this.toldOff(this.orders.get(id) ?? FREE, r)) continue;
            rear = Math.max(rear, dist(f.x, f.y, foe.x, foe.y));
          }
        }
        return { want: bearing(u.x, u.y, foe.x, foe.y), speed: rear - d > SEA.ai.station ? SEA.classes.dd.speed * SEA.speeds.slow : flank };
      }
      this.launch(u, foe, run);
    }
    if (u.state === 'routing' || told) { this.endRun(u, run, true); return null; }
    run.turned = Math.max(run.turned, Math.abs(turn(run.face, u.face)));
    if (this.t >= run.until) { this.endRun(u, run, true); return null; }
    return { want: run.away, speed: flank };
  }

  /**
   * The range a run launches at: its style's press, pressed home to put the salvo where it will tell; a cover run
   * (guard) launches as soon as its target is in torpedo reach (torp.start), its job to turn her away, not to sink her.
   * Samar, 25 October 1944: Johnston fired at about 9,000 m and the others soon after, to turn Kurita's heavy ships away
   * from the escort carriers, and were sunk after (the design director, 11:25Z). No new number.
   */
  private pressOf(u: SeaUnit, run: Run): number { return run.guard ? SEA.torp.start : this.style(u.side).press; }

  /** Whether the player's order takes a piece off its run: sent somewhere, held, withdrawn, or sent at another ship. */
  private toldOff(o: SeaOrder, run: Run): boolean {
    return o.kind === 'move' || o.kind === 'hold' || o.kind === 'leave' || (o.kind === 'attack' && !o.fleet && o.target !== run.target);
  }

  /** A run ends: after its launch it may run again torp.again seconds after it; broken off before, as soon as it is fit. */
  private endRun(u: SeaUnit, run: Run, done: boolean): void {
    this.runs.delete(u.id);
    delete u.extra.run;
    // One that ran on through its own side's break turns for home with the rest once its run is over.
    if (this.broken === u.side && live(u) && u.state !== 'routing') u.state = 'routing';
    if (!done) { this.nextRun.set(u.id, this.t); this.mark(`${u.side}:runsBroken`, 1); return; }
    this.nextRun.set(u.id, run.launched + SEA.torp.again);
    if (run.turned < Math.PI / 2) this.mark(`${u.side}:turnLate`, 1);
  }

  /**
   * A destroyer piece launches its bank at an enemy ship (torp): a salvo that falls torp.travel seconds later, aimed
   * where the target will be by then, its track a zone of kind 'torpedo' from the launch to the aim point (for the 3D
   * game; it lasts while the salvo runs). On a run, it then turns away; from station, it may launch again torp.again
   * seconds later.
   */
  private launch(u: SeaUnit, foe: SeaUnit, run: Run | null): void {
    const T = SEA.torp, F = SEA.field.size, w = this.bank[u.id];
    // How full the bank was, for the 3D game to draw as many tracks (the ship plan's L3): none at an empty bank.
    const full = SEA.fire.rate * this.output(u) * this.day(u.side) * T.bank;
    const share = full > 0 ? Math.round(clamp(w / full, 0, 1) * 100) / 100 : 0;
    this.bank[u.id] = 0;
    // The part of the salvo its guns have not yet earned goes with it, owed by its side when it lands (loaded, advance).
    const drawn = Math.min(this.loaded[u.id], w);
    this.loaded[u.id] = 0;
    const ahead = foe.extra.way * T.travel;
    const x = Math.round(clamp(foe.x + Math.sin(foe.face) * ahead, 2, F - 2)), y = Math.round(clamp(foe.y + Math.cos(foe.face) * ahead, 2, F - 2));
    this.torps.push({ at: this.t + T.travel, side: u.side, target: foe.id, x, y, weight: w, from: u.id, drawn });
    this.zones.push({ id: ++this.zoneId, kind: 'torpedo', x, y, r: T.radius, until: this.t + T.travel, side: u.side, at: Math.round(this.t * 10) / 10, unit: u.id });
    // The line once a piece, on its first salvo: most carry little weight, and the scene shows every salvo's wakes, so a
    // miss is the scene's to show (the UI director, 22:55). Every salvo keeps its event and its sound, the later ones blank.
    const first = !this.toldTorp.has(u.id);
    this.toldTorp.add(u.id);
    const text = first ? `${this.told(u, true)} ${this.many(u) ? 'fire' : 'fires'} torpedoes at ${this.told(foe)}.` : '';
    this.say({ what: 'sea:torpedo', unit: u.id, side: u.side, text, extra: { target: foe.id, x, y, running: T.travel, share } });
    if (!run) { this.nextRun.set(u.id, this.t + T.again); this.mark(`${u.side}:screenLaunch`, 1); return; }
    this.launched[u.side]++;
    this.mark(`${u.side}:runs`, 1);
    if (dist(u.x, u.y, foe.x, foe.y) > this.pressOf(u, run) + 200) this.mark(`${u.side}:launchFar`, 1);
    if (run.near) { this.mark(`${u.side}:runsNearScreen`, 1); if (run.shot) this.mark(`${u.side}:runsShotByScreen`, 1); }
    // (How far it turns away is measured from its heading in, for the late-turn mark; a cover run fires from whatever
    // course it is on, so from its bearing to the target.)
    const inbound = run.guard ? bearing(u.x, u.y, foe.x, foe.y) : u.face;
    Object.assign(run, { phase: 'away', until: this.t + T.away, away: bearing(foe.x, foe.y, u.x, u.y), face: inbound, turned: 0, launched: this.t });
    delete u.extra.run;
  }

  /** Salvos due to fall: on the target, as any blow does (take); with it gone, on the enemy nearest the aim point within
   *  torp.radius, else on the heaviest, so the destroyers' weight is never lost. */
  private landTorps(): void {
    if (!this.torps.some((s) => s.at <= this.t)) return;
    const due = this.torps.filter((s) => s.at <= this.t);
    this.torps = this.torps.filter((s) => s.at > this.t);
    for (const s of due) {
      if (s.weight <= 0) continue;
      const foe = this.byId.get(s.target);
      let at: SeaUnit | null = foe && live(foe) ? foe : null;
      if (!at) {
        let bd = SEA.torp.radius;
        for (const e of this.units) { if (e.side === s.side || !live(e)) continue; const d = dist(e.x, e.y, s.x, s.y); if (d <= bd) { bd = d; at = e; } }
      }
      at ??= this.heaviest(other(s.side));
      if (!at) continue;
      // A hit on a piece of more than one ship never claims them all (the UI director, 22:55): "find their mark among
      // the battleships of the High Seas Fleet"; on one ship, "hit a battleship of the High Seas Fleet". Counted as aimed.
      const among = at.extra.ships > 1 ? `find their mark among the ${RULES_CLASSES[at.extra.cls as SeaClass].many} ${ofFleet(at.extra.fleet)}` : `hit ${this.told(at)}`;
      // Told before the blow is taken, so a ship the salvo sinks goes down after the hit that sank her (the scene's log).
      const by = this.byId.get(s.from)!;
      this.say({ what: 'sea:torpedoHit', unit: at.id, side: s.side, text: `Torpedoes from ${this.told(by)} ${among}.`, extra: { from: s.from } });
      const l0 = this.left(at.side);
      const got = this.take(at, s.weight);
      this.dealt[s.from] += got;
      this.mark(`${s.side}:torp>${at.extra.cls}`, got);
      // What the guns had not earned of it is owed now that it has dealt its weight, paid out of the side's gunfire; and
      // its share of what the salvo took counts on neither side until they pay it (unpaid: as the take measured it).
      if (s.drawn > 0) {
        this.advance[s.side] += s.drawn;
        this.unpaid[at.side] += (Math.max(0, l0 - this.left(at.side)) * s.drawn) / s.weight;
        this.mark(`${s.side}:salvoDrawn`, s.drawn);
      }
      // Measured only (for the design director): salvoes that landed, those on the ship they were fired at, and those
      // that landed before either side broke off (R4's read).
      this.mark(`${s.side}:salvoLanded`, 1);
      if (at === foe) this.mark(`${s.side}:salvoOnTarget`, 1);
      if (this.broken === null) this.mark(`${s.side}:salvoBeforeBreak`, 1);
    }
  }

  /** The side brings aircraft into the action: a carrier with its air group, or the coasts' aircraft (cap). */
  private bringsAir(side: Side): boolean {
    return this.airLeft[side] > 1e-9 || this.units.some((u) => u.side === side && u.extra.cls === 'cv' && live(u) && this.deck[u.id] > 1e-9);
  }

  /**
   * A strike or a raid of weight w coming in on 'at' (cap; ruling 239): the fighters over its side's fleet deny what the
   * campaign's law gives (carriers.json cap.spoil), paid weight for weight out of the pool, each carrier by its share;
   * then the gun ships within the strike's radius deny their share of the rest (flak), each owing it out of its next
   * gunfire. Returns what lands. Two neutral trades, no dice.
   */
  private meet(at: SeaUnit, w: number, by: Side, strike: boolean): number {
    const side = at.side;
    const cvs = this.units.filter((u) => u.side === side && u.extra.cls === 'cv');
    const pool = cvs.reduce((n, u) => n + this.cover[u.id], 0);
    let a = 0;
    this.capLast = null;
    if (pool > 1e-12 && w > 0) {
      a = Math.min(pool, w * CAP.spoil * Math.sqrt(pool / (pool + w)));
      const most = cvs.reduce((b, u) => (this.cover[u.id] > this.cover[b.id] ? u : b), cvs[0]);
      this.capLast = { share: a / w, fleet: most.extra.fleet };
      for (const u of cvs) this.cover[u.id] *= 1 - a / pool;
      this.mark(`${side}:paid`, a);
      this.say({ what: 'sea:cap', unit: at.id, side, text: '', extra: { denied: a, share: a / w, by } });
    }
    const guns = this.units.filter((e) => e.side === side && live(e) && e.extra.cls !== 'cv' && dist(e.x, e.y, at.x, at.y) <= SEA.strike.radius);
    const parts = guns.map((e) => e.extra.ships * (FLAK.ships[e.extra.cls as SeaClass] ?? 0) * FLAK.per);
    const sum = parts.reduce((n, x) => n + x, 0);
    const g = sum > 0 ? Math.min(FLAK.most, sum) * (w - a) : 0;
    if (g > 0) {
      guns.forEach((e, i) => { this.owe[e.id] += (g * parts[i]) / sum; });
      this.flakOn[side][strike ? 'cv' : 'air'] += g;
      this.say({ what: 'sea:flak', unit: at.id, side, text: '', extra: { denied: g, by, ships: guns.filter((_, i) => parts[i] > 0).map((e) => e.id) } });
    }
    this.mark(`${side}:denied`, a + g);
    // The C3 mark: a strike on a side with carriers meets its fighters and lands below the weight flown.
    if (strike && cvs.some(live)) { this.mark(`${side}:strikesIn`, 1); if (a > 0 && a + g > 0) this.mark(`${side}:strikesMet`, 1); }
    return w - a - g;
  }

  /** A gun ship out of the fight hands what it owes to the nearest of its side's gun ships still fighting (cap). */
  private handOwes(): void {
    for (const u of this.units) {
      if (live(u) || this.owe[u.id] <= 0) continue;
      let to: SeaUnit | null = null, bd = Infinity;
      for (const f of this.units) {
        if (f.side !== u.side || f.extra.cls === 'cv' || !live(f)) continue;
        const d = dist(u.x, u.y, f.x, f.y);
        if (d < bd - 1e-6) { bd = d; to = f; }
      }
      if (!to) continue;
      this.owe[to.id] += this.owe[u.id];
      this.owe[u.id] = 0;
    }
  }

  /**
   * The action over, or handed to the staff: what is still owed is settled where it is owed before the count, as the land
   * field pays its air (b418, weight moves where it falls, never how much). What a ship's guns still owe for the aircraft
   * they turned away lands on that ship, or on the first of its side's still fighting: those aircraft get through after
   * all. What a carrier still holds over its fleet flies at the enemy its next strike would have gone for, whole. No line
   * of its own and nothing drawn: the count carries it.
   */
  private settle(): void {
    this.handOwes();
    for (const u of this.units) {
      const o = this.owe[u.id];
      if (!(o > 0)) continue;
      const to = live(u) ? u : this.units.find((f) => f.side === u.side && live(f));
      if (!to) continue;
      this.owe[u.id] = 0;
      const got = this.take(to, o);
      const by = other(u.side), k = this.flakOn[u.side], cv = k.cv + k.air > 0 ? k.cv / (k.cv + k.air) : 0;
      this.fromAir[`${by}:cv`] = (this.fromAir[`${by}:cv`] ?? 0) + got * cv;
      this.fromAir[`${by}:air`] = (this.fromAir[`${by}:air`] ?? 0) + got * (1 - cv);
      this.mark(`${u.side}:paid`, o);
      this.mark(`${u.side}:settled`, o);
    }
    for (const cv of this.units) {
      const c = this.cover[cv.id];
      if (!(c > 0)) continue;
      const at = this.strikeTarget(cv) ?? this.heaviest(other(cv.side));
      if (!at) continue;
      this.cover[cv.id] = 0;
      const got = this.splash(at, c, cv.side);
      this.fromAir[`${cv.side}:cv`] = (this.fromAir[`${cv.side}:cv`] ?? 0) + got;
      this.mark(`${cv.side}:coverFlown`, c);
    }
    // Measured, not settled (for the design director): what strikes still in the air carry when the action ends.
    for (const k of this.strikes) this.mark(`${k.side}:aloft`, k.weight);
  }

  /** The C5 mark: a side's carrier within the guns' reach of an enemy battleship, at any moment of the action. */
  private watchCarriers(): void {
    const R = SEA.classes.bb.reach;
    for (const u of this.units) {
      if (u.extra.cls !== 'cv' || !live(u) || this.cvNear[u.side]) continue;
      if (this.units.some((e) => e.side !== u.side && e.extra.cls === 'bb' && live(e) && dist(u.x, u.y, e.x, e.y) < R)) this.cvNear[u.side] = true;
    }
  }

  /** The coasts' aircraft raid the enemy's ships every so often, with the weight the rules give them over this sea. */
  private coastAir(): void {
    for (const side of ['att', 'def'] as Side[]) {
      const w = this.airLeft[side];
      if (w <= 0 || this.t < this.nextAir[side]) continue;
      this.nextAir[side] += SEA.air.every;
      const foes = this.units.filter((e) => e.side !== side && live(e));
      if (!foes.length) continue;
      // Aircraft from the coast find a ship by the campaign's own aim (carriers.json strike.aim: carriers, then battleships,
      // cruisers, destroyers), times the ships afloat in each piece: one draw, as before (ruling 239, item 4).
      const aims = foes.map((e) => (STRIKE_AIM[e.extra.cls as SeaClass] ?? 0) * e.extra.ships);
      const all = aims.reduce((n, a) => n + a, 0);
      let r = this.rng.next() * all, at = foes[foes.length - 1];
      for (let i = 0; i < foes.length; i++) { r -= aims[i]; if (r < 0) { at = foes[i]; break; } }
      if (all > 0) for (let i = 0; i < foes.length; i++) this.mark(`${side}:raidExp>${foes[i].extra.cls}`, aims[i] / all);
      this.mark(`${side}:raid>${at.extra.cls}`, 1);
      const weight = SEA.fire.rate * w * SEA.air.share * SEA.air.every * this.day(side);
      // The same weight out of the fight whatever it falls on, as gunfire and strikes (b402): a raid that happened on a
      // destroyer took more than twice what one on a battleship took, so a fleet of many light ships paid double.
      const landed = this.meet(at, weight, side, false);
      const got = this.splash(at, landed, side);
      this.fromAir[`${side}:air`] = (this.fromAir[`${side}:air`] ?? 0) + got;
      this.say({ what: 'sea:air', unit: at.id, side, text: this.turnedBack('aircraft from the coast', at, 'attack') ?? `Aircraft from the coast attack ${this.told(at)}.` });
    }
  }

  /** A strike's weight shared over the target and the ships within the strike's radius of it, by their weight. */
  private splash(at: SeaUnit, weight: number, by: Side): number {
    const hit = this.units.filter((e) => e.side !== by && live(e) && dist(e.x, e.y, at.x, at.y) <= SEA.strike.radius);
    const total = hit.reduce((n, e) => n + this.output(e) + (e.id === at.id ? this.output(e) : 0), 0);
    if (total <= 0) return 0;
    let got = 0;
    for (const e of hit) {
      const share = weight * (this.output(e) + (e.id === at.id ? this.output(e) : 0)) / total;
      // A strike or a raid takes the same weight out of the fight whatever it falls on, wear in proportion to the
      // ship's hull, so where the player sends it is a choice of which ships to cripple, not a trick (ruling 226, 4).
      got += this.take(e, share);
    }
    return got;
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
    // A division sent to turn away a ship threatening its carriers or its outnumbered battleships runs on through the
    // break, covering the withdrawal, as Johnston, Hoel and the escorts did off Samar; its blow lands after the break
    // and never decides it (the design director, 08:10Z). Every other ship turns for home.
    for (const u of this.units) {
      if (u.side !== side || !live(u)) continue;
      if (this.runs.get(u.id)?.guard && u.state === 'steady') { this.mark(`${side}:guardOn`, 1); continue; }
      u.state = 'routing';
    }
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
    this.settle();
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
      `${this.adj(winner)} ships hold ${seaWhat(this.ticket.place.zoneName)}.`,
      // Handed to the staff, the rules sink the rest as they settle it, so this line says only what went down in sight.
      !toDice ? `${a} losses: ${shipWords(sa)} sunk. ${d} losses: ${shipWords(sd)} sunk.`
        : shipsIn(sa) + shipsIn(sd) === 0 ? 'No ship had gone down on either side when the staff took over.'
          : `When the staff took over, the ${a} fleet had lost ${shipWords(sa)} and the ${d} fleet ${shipWords(sd)}.`,
    ];
    // HAND IT TO THE STAFF (log b348): the paper line after it, in the UI director's three forms (ruling 226, 5): the
    // whole action before a shot told; "what you had won stays won" only where the field had moved the hander's way by
    // 5 points or more, their share of the balance at the hand-over against the card's (the design director, 20:51 UTC).
    if (toDice) {
      const zone = seaWhere(this.ticket.place.zoneName), by = this.handedBy ?? 'att';
      const k = this.ticket.reckoning.odds, now = (k * (1 - this.lostShare('att'))) / Math.max(1e-6, 1 - this.lostShare('def'));
      const share = (o: number): number => (by === 'att' ? o : 1) / (1 + o);
      lines.push(!this.fired() ? `The staff settled the whole action ${zone}: no shot had been fired.`
        : share(now) - share(k) >= 0.05 ? `The staff settled the rest of the action ${zone} from where it stood: what you had won stays won.`
          : `The staff settled the rest of the action ${zone} from where it stood.`);
    }
    // The ships each fleet took home badly hurt (b458, 3): those that went out of the fight burning, and those still
    // afloat at half their hull or less, on the field or gone home.
    const hurt: Record<number, SeaShips> = {};
    for (const f of this.ticket.place.fleets) hurt[f.id] = { ...this.fleetLoss.burnt[f.id] };
    for (const u of this.units) if (u.state !== 'gone') hurt[this.info.get(u.id)!.fleet.id][u.extra.cls as SeaClass] += hurtAfloat(u.str, u.extra.ships0, u.extra.ships);
    const extra: SeaLosses = { sunk: this.fleetLoss.sunk, burnt: this.fleetLoss.burnt, hurt, ...(toDice ? { toDice } : {}) };
    // The B4 mark: a side with two destroyer pieces outside its screen, against an enemy with a big ship, made a run.
    for (const side of ['att', 'def'] as Side[]) {
      const free = this.units.filter((u) => u.side === side && u.extra.cls === 'dd' && !this.screen.has(u.id)).length;
      if (free < 2 || !this.units.some((e) => e.side !== side && e.extra.cls !== 'dd')) continue;
      this.mark(`${side}:runEligible`, 1);
      if (this.launched[side] > 0) this.mark(`${side}:runBattles`, 1);
    }
    // Bank left: what idle guns banked and never launched; a loaded salvo never fired is no weight moved (loaded).
    for (const u of this.units) {
      if (this.bank[u.id] - (this.loaded[u.id] ?? 0) > 0) this.mark(`${u.side}:bankLeft`, this.bank[u.id] - (this.loaded[u.id] ?? 0));
      if ((this.loaded[u.id] ?? 0) > 0) this.mark(`${u.side}:loadedLeft`, this.loaded[u.id]);
    }
    // Measured, not settled (for the design director): what a side still owes for salvoes that landed before its guns had
    // earned them: weight its tubes dealt beyond its guns, with the action over before they paid it.
    for (const sd of ['att', 'def'] as Side[]) if (this.advance[sd] > 0) this.mark(`${sd}:salvoUnpaid`, this.advance[sd]);
    // Salvoes still running when the action ended: launched, never landed.
    for (const t of this.torps) if (t.weight > 0) this.mark(`${t.side}:salvoInWater`, 1);
    // The C4 and C5 marks: what the guns still owed and the fighters still held at the end; a side's carriers against an
    // enemy with a battleship, and whether one of them came within the guns' reach of it.
    for (const u of this.units) { if (this.owe[u.id] > 0) this.mark(`${u.side}:owedLeft`, this.owe[u.id]); if (this.cover[u.id] > 0) this.mark(`${u.side}:coverLeft`, this.cover[u.id]); }
    for (const side of ['att', 'def'] as Side[]) {
      if (!this.units.some((u) => u.side === side && u.extra.cls === 'cv') || !this.units.some((e) => e.side !== side && e.extra.cls === 'bb')) continue;
      this.mark(`${side}:cvVsBb`, 1);
      if (this.cvNear[side]) this.mark(`${side}:cvNearBb`, 1);
    }
    this.over = {
      winner, how, seconds: this.t, lost, men: crews, R, level: seaLevel(R), lines, forces,
      extra: extra as unknown as Record<string, unknown>,
    };
    this.say({ what: 'end', side: winner, text: lines[0] });
  }

  // ---- What the screens draw --------------------------------------------------------------------------------------

  /** A piece in a sentence, by what it is and whose, the fleet taking "the" only where English does (log b370, b458 6):
   *  "a battleship of the Home Fleet", "3 destroyers of Force H". */
  private told(u: SeaUnit, start = false): string {
    const c = RULES_CLASSES[u.extra.cls as SeaClass], n = Math.max(1, u.extra.ships);
    // At a sentence's head one to nine are words (the UI director, b476 3): "Four destroyers of the Home Fleet ...".
    const s = n === 1 ? `a ${c.one} ${ofFleet(u.extra.fleet)}` : `${start ? headCount(n) : n} ${c.many} ${ofFleet(u.extra.fleet)}`;
    return start ? s[0].toUpperCase() + s.slice(1) : s;
  }

  /**
   * The line for a strike or raid whose fleet's fighters turned back a third or more of it, in place of the find line
   * (the UI director, 20 November 21:17 UTC, ui-director/field-battles/sea-226-review.md): "The Pacific Fleet's
   * fighters turn back half of the 36 bombers from the Striking Force, and the rest find a battleship." Null below a
   * third, where the find line stands. Flak gets no words: its work shows in the hits.
   */
  private turnedBack(what: string, at: SeaUnit, verb: string): string | null {
    const c = this.capLast;
    if (!c || c.share < 1 / 3) return null;
    const fl = theFleet(c.fleet), who = `${fl[0].toUpperCase()}${fl.slice(1)}'s fighters turn back`;
    const all = c.share >= 1 - 1e-9;
    const word = all ? 'all' : c.share <= 0.42 ? 'a third' : c.share <= 0.58 ? 'half' : c.share <= 0.75 ? 'two thirds' : 'most';
    // "all 36 bombers", "all the aircraft"; "half of the 36 bombers", "half of the aircraft".
    const of = all ? (/^\d/.test(what) ? `all ${what}` : `all the ${what}`) : `${word} of the ${what}`;
    if (all) return `${who} ${of}, and none reach its ships.`;
    const c2 = RULES_CLASSES[at.extra.cls as SeaClass], n = Math.max(1, at.extra.ships);
    return `${who} ${of}, and the rest ${verb} ${n === 1 ? `a ${c2.one}` : `${n} ${c2.many}`}.`;
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

  /**
   * What the 3D game draws of how each piece is handled (the ship plan's B1, ruling 239 at 23:20Z: look fields in one
   * commit that move no number). Read from what the sim did this tick; nothing in the rules reads them back and they draw
   * no dice. On every piece: 'foe', the enemy piece whose bearing set its course this tick (-1 when an order, its fleet's
   * line, a carrier group's turn away or the field's edge set it), for the columns to wheel on; 'helm', how hard it is
   * turning, -1 to 1, + to starboard, for heel and wake; 'lineAt', its place in its side's battle line (0 the guide),
   * absent when it is in none. On a destroyer piece on a torpedo run: 'runPhase', 'in' or
   * 'away', and 'runUntil', the second it turns back. 'smoke': a destroyer or cruiser piece laying a smoke screen, as one
   * turning away from its run, one breaking off, or a destroyer piece within its screen's station of a carrier of its
   * own turning away from a battle line (drawn only: it hides nothing, and the guns fire through it). On a carrier:
   * 'flight', its air group on deck, 'out' at the enemy, or 'back' and landing on; and 'cover', the share of the
   * fighters it held over the fleet at its last strike that are still up, 0 to 1.
   */
  private showLooks(): void {
    const L0: Record<Side, boolean> = { att: this.linePlans.att.t === this.t && this.lines.att !== null, def: this.linePlans.def.t === this.t && this.lines.def !== null };
    for (const u of this.units) {
      const x = u.extra, on = live(u), r = on ? this.runs.get(u.id) : undefined;
      x.foe = on ? this.steered[u.id] : -1;
      x.helm = on ? Math.round(this.helmed[u.id] * 100) / 100 : 0;
      if (r) { x.runPhase = r.phase; if (r.phase === 'away') x.runUntil = Math.round(r.until * 10) / 10; else delete x.runUntil; } else { delete x.runPhase; delete x.runUntil; }
      const at = on && L0[u.side] ? this.linePlans[u.side].members.indexOf(u) : -1;
      if (at >= 0) x.lineAt = at; else delete x.lineAt;
      const light = x.cls === 'dd' || x.cls === 'ca';
      x.smoke = on && light && (r?.phase === 'away' || u.state === 'routing'
        || (x.cls === 'dd' && this.units.some((c) => c.side === u.side && c.extra.cls === 'cv' && live(c) && this.away[c.id] && dist(u.x, u.y, c.x, c.y) <= SEA.torp.station)));
      if (x.cls !== 'cv') continue;
      x.flight = this.strikes.some((s) => s.cv === u.id) ? 'out' : this.recovers.some((s) => s.cv === u.id) ? 'back' : 'deck';
      x.cover = this.coverHeld[u.id] > 0 ? Math.round(clamp(this.cover[u.id] / this.coverHeld[u.id], 0, 1) * 100) / 100 : 0;
    }
  }

  /** Each side's strength still in the fight, its ships and its coasts' aircraft, as a share of both sides' at the start. */
  balance(): Record<Side, number> {
    const total = this.weight0.att + this.weight0.def;
    const standing = (side: Side): number => this.units.reduce((n, u) => n + (u.side === side && live(u) ? this.weightOf(u) : 0), this.airLeft[side]);
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

  /** Where the wind blows from in this battle, for the 3D game (the ship plan's L6): seaWind. */
  get wind(): number { return seaWind(this.ticket.seed); }
  /** How hard it blows, for the 3D game: seaWindSpeed. */
  get windSpeed(): number { return seaWindSpeed(this.ticket.seed); }

  /** Battle time the guns first reached an enemy, null before it. */
  get contactTime(): number | null { return this.contactAt; }

  /** The side that has broken off the action and is making for port, while the other chases; null before. */
  get brokenSide(): Side | null { return this.broken; }

  /** Nothing is happening yet: no gun has reached an enemy. The screens run time fast while it holds (ruling 226(1)).
   *  Strikes and the coasts' raids are counted, not flown, so one in the air does not end it, before or after it lands
   *  (b458, 2a): it holds until the guns meet, and once they have it never holds again. A side that breaks off before
   *  they meet ends it too: what follows is the chase, not the approach. */
  get quiet(): boolean { return this.contactAt === null && this.broken === null; }
}

const TAU = Math.PI * 2;
const norm = (a: number): number => ((a % TAU) + TAU) % TAU;

/**
 * One wind for a battle, from its seed (the ship plan's L6): the bearing it blows from, in the field's bearings (0 toward
 * +y, as a ship's face; + to starboard). A hash, not a draw: it leans the smoke, sets the carriers' deck wind and the sea
 * state's run in the 3D game, and nothing in the rules reads it.
 */
export function seaWind(seed: number): number { return Math.round(mix((seed ^ 0x9e3779b9) >>> 0) * TAU * 1000) / 1000; }
/** The wind's strength for a battle, in metres a second, 4 to 11 (a light breeze to a strong one): a hash of the seed as
 *  seaWind is, for the 3D game's sea state and smoke. Nothing in the rules reads it. */
export function seaWindSpeed(seed: number): number { return Math.round((4 + 7 * mix((seed ^ 0x51ed270b) >>> 0)) * 10) / 10; }
/** The order of a piece that has none: fight at will. */
const FREE: SeaOrder = { kind: 'free' };
