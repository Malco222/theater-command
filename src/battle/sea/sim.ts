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
import { aliveAfter, CAP, DICE, FLAK, RULES_CLASSES, SAILORS, SEA, STRIKE_AIM } from './numbers';
import { drawUp, type PieceInfo } from './pieces';
import { SEA_CUES } from './cues';
import { none, seaLevel, seaRatio, shipsIn, shipWords, sunkCaps } from './result';
import { headCount, hurtAfloat, ofFleet, theFleet } from './words';
import type { SeaClass, SeaCommand, SeaOrder, SeaSailors, SeaShips, SeaSpeed, SeaStyle, SeaTicket, SeaUnit } from './types';
import type { CarrierWay, StyleNumbers } from './numbers';
import { seaWhat, seaWhere } from '../../campaign/sea-words';

const other = (s: Side): Side => (s === 'att' ? 'def' : 'att');
const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
/** A number from 0 to under 1 fixed by a whole number: where a lost ship's smoke lies, never drawn from the battle's dice. */
const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
/** Pieces that still answer orders and fight as told: not running for home. */
const steady = (u: SeaUnit): boolean => u.state === 'steady' || u.state === 'wavering';
/** The big guns: the classes whose fire a style shares out over the enemy's big ships (jobs). */
const BIG_GUNS = new Set<string>(['bb', 'ca']);
/** How near a ship sent to a point comes before she stops there (metres): an order to steer for a point, and one to pick
 *  up survivors at a wreck, which she lies stopped beside. */
const ARRIVE = 200;
/** How far inside the field's edge a carrier keeps when it turns away from a battle line or takes its station (metres). */
const EDGE_MARGIN = 1200;
/** Where on each of a piece's ships the own-water rule looks whether a salvo's heads are past her, as shares of her length
 *  from her middle: her bow, her middle and her stern (notPast). */
const HULL = [-0.5, 0, 0.5];
/** Where a salvo fired now at a ship at (x, y) on heading 'face' with way 'way' is aimed: where she will be when it lands. */
const aimAt = (x: number, y: number, face: number, way: number): [number, number] => {
  const F = SEA.field.size, ahead = way * SEA.torp.travel;
  return [Math.round(clamp(x + Math.sin(face) * ahead, 2, F - 2)), Math.round(clamp(y + Math.cos(face) * ahead, 2, F - 2))];
};
/** A piece's ships ([x, y, heading] each: hullsOf) and its way, as they were when the tick's steering began (own water). */
type Pose = { hulls: [number, number, number][]; way: number };
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
 * not yet earned (loaded), owed by its side when it lands. 'dir' is the tracks' bearing, from the launcher's place at
 * the launch to the aim point (taken then, before the launcher turns away), and 't0' the tick it was launched (the battle
 * second of that tick): what the ships it runs at comb by (combing; the ship plan's R6). Neither is a number to tune, nor a look field.
 * 'x0', 'y0': the launcher's place at the launch, where the tracks start (release 2's own water: ownWater).
 */
interface Torp { at: number; side: Side; target: number; x: number; y: number; weight: number; from: number; drawn: number; dir: number; t0: number; x0: number; y0: number }
/**
 * A side's battle line (the ship plan's R1, ruling 239's order at 23:20Z): its battleships and cruisers steaming one
 * course together, its rows as they drew up turned into columns in line ahead. 'hand' is the side of its bearing to
 * the enemy it steers (+1 to the right, -1 to the left); 'order' its pieces foremost first along its course, the van
 * leading; 'turned' the second it last turned together off the field's edge or away from a crossed T.
 */
interface Line { order: number[]; hand: number; course: number; speed: number; turned: number }

/** Each fleet's ships lost on the field, by class: sunk, and gone out of the fight burning (past the dice's limit); and
 *  the ships it took home badly hurt, those burning and those afloat at half their hull or less (b458, 3). 'kept'
 *  (ruling 240, damage control): of those gone out burning, the ships the field would have sunk without damage control
 *  (its limit at navy.json's toughness); absent where there are none, as with the switch off. With men in
 *  the water (SeaSailors): 'abandoned', the ships its own call to abandon early lost, past the field's limit (form C3: the
 *  campaign counts them against the fleet, the field's limit does not); 'sailors' and 'rescue' (SeaMen, SeaRescue). */
export interface SeaLosses {
  sunk: Record<number, SeaShips>; burnt: Record<number, SeaShips>; hurt: Record<number, SeaShips>; kept?: Record<number, SeaShips>;
  toDice?: boolean;
  abandoned?: Record<number, SeaShips>; sailors?: Record<Side, SeaMen>; rescue?: SeaRescue[];
  /** Ships by class (plan 5.1, c3): for each fleet and kind the ticket gave by row, the rows of the ships it lost to the
   *  campaign (sunk, and abandoned early), in the order they went: as many as it lost of the kind. */
  rows?: Record<number, Partial<Record<SeaClass, (string | null)[]>>>;
}

/**
 * A side's men in the water over the action, for the after-action card (form 129(d)) and the campaign (form B4, B5): the
 * crews of its ships lost, those who went down with them, were killed going in, or went into the water alive; of those,
 * the men saved (picked up by their own side, in the fight or after it, or found later), taken prisoner by the enemy,
 * and lost (crew less saved and prisoners). 'call': the abandon-ship call its fleets stood at ('fight', 'early', or
 * 'both' where its fleets differed), the ships it abandoned early by {unit, ship}, and its call after the fight where it
 * held the water ('stop' or 'leave'; null where it did not).
 */
export interface SeaMen {
  crew: number; down: number; dead: number; water: number; saved: number; prisoners: number; lost: number;
  call: { abandon: 'fight' | 'early' | 'both'; early: { unit: number; ship: number }[]; stop: 'stop' | 'leave' | null };
}

/** What a fleet holding the water picked up from one wreck after the fight (form B2), for the closing shot: the men, and
 *  of them the enemy's (its prisoners). */
export interface SeaRescue { fleet: number; wreck: { unit: number; ship: number }; men: number; prisoners: number }

/**
 * A ship lost on the field with her men in the water (SeaSailors): her piece and her place in its losses ('ship', from
 * 1: {unit, ship} keys a wreck), whose she was, where and when she went, how many she carried and how they went, and
 * the men still alive in the water as of 'at' (battle seconds), which the sea wears down (aliveAfter) and pick-ups take
 * from. 'blow': what sank her (the oil burns on it). 'picked': men taken out of the water in the fight, by the side that
 * took them. 'guest': not her crew but men she had picked up in the fight, in the water again with her people (the
 * design director, 18:42Z: they share her fate); their crew, down and water are counted at their own first wreck, so
 * here they are 0, and side, fleet and navy are theirs. A guest group shares her {unit, ship} and her place.
 */
interface Wreck {
  unit: number; ship: number; side: Side; fleet: number; nat: string; cls: SeaClass; x: number; y: number; t: number;
  crew: number; down: number; dead: number; water: number; early: boolean; alive: number; at: number;
  picked: Record<Side, number>; guest?: boolean;
}
/** The blow now landing on the field, for who goes down with a ship it sinks and whether her oil burns (SeaSailors):
 *  gunfire, a torpedo salvo, or aircraft (a carrier's strike or the coasts' raid). */
type Blow = 'gun' | 'torp' | 'air';

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
  /** Ruling 240: the limit by class without damage control (navy.json's toughness), the ships that limit would have let
   *  go down so far, and by fleet the ships that went out burning that it would have sunk (SeaLosses.kept). */
  private readonly plainCaps: Record<Side, SeaShips>;
  private readonly plainLost: Record<Side, SeaShips> = { att: none(), def: none() };
  private readonly kept: Record<number, SeaShips> = {};
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
  /** The enemy salvo each battleship, cruiser or carrier combed this tick, by her id (combing; the ship plan's R6): kept
   *  until it lands, and read by the crossed-T test while her line's guide combs (crossed). */
  private readonly combs = new Map<number, Torp>();
  /**
   * The build's stage of R6 (the design director, 05:17Z, Q2(a) and (c)): the whole rule is written, and this holds parts
   * of it off while each step is read on every bar. 1: only the ship a salvo is fired at combs (the torpedo water held
   * off), and no ship of a carrier group (R1's own test, planLine); 2: the water too, still outside a carrier group; 3:
   * nothing held. A build stage only, never read from the data, the ticket, a flag or a look field: it goes at step 3,
   * with r6Held's hold, the test that pins it (test 14), and the build-stage clause at the end of sea.json's _about.torp
   * ("while the rule is built in steps ...").
   */
  private readonly r6Step: number = 1;
  /**
   * Release 2's own water (Brian, 25 November 12:52Z: "the friendly ships would pass though right in front of
   * torpedoes. have more awareness to avoid friendly fire"; naval-battles/own-water-r2/form.md): captains mind their own
   * side's torpedoes. A destroyer holds its salvo while one of its own ships would be in the tracks' way (clearAim), and
   * every ship with way on keeps out of the water ahead of its own side's running salvoes (keepClear). On only with the
   * ticket's ownWater (the campaign's switch, ticket.ts ownWaterOn); off, nothing below reads or writes anything.
   */
  private readonly ownWater: boolean;
  /** The own-side salvo each ship keeps clear of, by her id (keepClear): held until its heads are past her or it lands. */
  private readonly clearing = new Map<number, Torp>();
  /** The pieces holding their salvo for a ship of their own in the way, by id: that ship's piece and the second it was
   *  last found there (clearAim; the marks, and the look field heldFor). */
  private readonly holding = new Map<number, { by: number; t: number }>();
  /** The ships whose course keeping clear of an own salvo set this tick (keepClear; the look fields). */
  private readonly clearSteered = new Set<number>();
  /** The course each ship took up keeping clear of a salvo while others ran beside it, held while no own salvo's heads
   *  would cross her on it (clearAway). */
  private readonly clearHeading = new Map<number, number>();
  /** Every piece's ships and way as the tick's steering began, while the pieces steer (clearAim), else null. */
  private poses: Map<number, Pose> | null = null;
  /** Ships turning back to their helm's course after keeping clear, until they are on it (crossed reads the line's). */
  private readonly clearBack = new Set<number>();
  /** Each piece's wake (own water: hullsOf): the places her leader has passed, newest first, one every length of hers. */
  private readonly trail = new Map<number, [number, number][]>();
  /**
   * R6 item (2), how a ship that combed regains her place (the design director, 19:03Z; naval-battles/r6/regain-form.md,
   * on the historian's record, historian/regain-station-record-1122.md). A build switch, as r6Step is: a private readonly
   * set by nothing, so no datum, ticket, flag or look field can move it. Off, no place is kept and the line helm is
   * release 2's to the byte.
   */
  private readonly r6Back: boolean = true;
  /** Where each line ship stood from the middle of her line, [east, north] in metres, as her comb began (R6 item 2: her
   *  place, carried on with the line), kept until she is back within ai.station of it or leaves the line; and those of
   *  them whose comb has ended, working back to it. Taken only for a ship inside the line's keep: one beyond it had no
   *  place to keep and her helm was already working her back toward the line's middle. */
  private readonly place = new Map<number, [number, number]>();
  private readonly regaining = new Set<number>();
  /** Whether each side is a carrier group this tick (carrierGroup), taken once at the start of the tick before any ship
   *  steers, for R1's line (planLine) and the build stage's hold (r6Held) alike: a piece that leaves the field in the
   *  middle of the steer then changes it for no ship until the next tick, whatever the order the pieces steer in. */
  private readonly groupAt: Record<Side, boolean> = { att: false, def: false };
  /** The ships whose comb set their course this tick (R6): a comb the field's edge overrode is held (combs) but did not
   *  steer her, and the crossed-T test reads such a guide's own heading (crossed). */
  private readonly combSteered = new Set<number>();
  /** Measured only (the marks; nothing in the rules reads them): the course each combing ship's helm would have steered
   *  without the comb this tick, her comb course, and whether an enemy lay in her reach as her comb began. */
  private readonly combBare: number[] = [];
  private readonly combTo: number[] = [];
  private readonly combIn: boolean[] = [];
  /** Measured only: the salvo each ship combed last, to tell taking the same one up again (combResume) from a new comb;
   *  and the ships whose comb began or ended this tick, for the marks taken once every piece has steered (combReach). */
  private readonly combLast: (Torp | undefined)[] = [];
  private readonly combBegun = new Set<number>();
  private readonly combEnded = new Set<number>();
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
  /** By side, the pieces keeping the AA ring this tick (R3): each one's carrier, its place in that carrier's ring and how
   *  many share it. */
  private readonly rings: Record<Side, Map<number, { cv: number; i: number; k: number }>> = { att: new Map(), def: new Map() };
  /** Whether a side's ring has been let go for good because its carriers sighted a battle line (R3; see planRings). */
  private readonly ringGone: Record<Side, boolean> = { att: false, def: false };
  /** The carriers that took their station this tick (carrierHelm, and not turned away): for R4's overtaking cue. */
  private readonly onStation = new Set<number>();
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
  private readonly fleetLoss: { sunk: Record<number, SeaShips>; burnt: Record<number, SeaShips>; abandoned: Record<number, SeaShips> } = { sunk: {}, burnt: {}, abandoned: {} };
  /** Men in the water (release 2's sailor rules), as the ticket carries them; undefined with the switch off, and then
   *  nothing below is read or written. */
  private readonly sail: SeaSailors | undefined;
  private readonly wrecks: Wreck[] = [];
  /** Each piece's ships lost so far, sunk or abandoned: the 'ship' of its next wreck less one. */
  private readonly wreckOf = new Map<number, number>();
  /** A piece's own abandon-ship call, where the player has changed it from its fleet's (form C1). */
  private readonly abandonCall = new Map<number, boolean>();
  private blow: Blow = 'gun';
  /** Pieces picking up survivors in the fight (form B1), by unit id: the wreck, the men taken aboard and the count last told. */
  private readonly rescuing = new Map<number, { w: Wreck; got: number; told: number }>();
  /** The men each piece has taken aboard in the fight and still carries, by the group of men in the water they came
   *  from: a ship of the piece that is lost takes her share of them with her (the design director, 18:42Z). */
  private readonly aboard = new Map<number, Map<Wreck, number>>();
  /** The sinkings of ships their men abandoned, told 'abandon.afloat' seconds after (a looks-only knob): booked already. */
  private sinkings: { at: number; w: Wreck; lit: boolean; text: string; extra: Record<string, unknown> }[] = [];
  /** Ships by class (plan 4.7, c3): the rows of each piece's hulls still with it, by unit id, where its fleet gives its
   *  kind by row; and the rows each fleet has lost to the campaign, by kind (SeaLosses.rows). */
  private readonly hullsLeft = new Map<number, (string | null)[]>();
  private readonly lostRows: Record<number, Partial<Record<SeaClass, (string | null)[]>>> = {};

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
      const p = info.get(u.id)!;
      if (p.rows) { this.hullsLeft.set(u.id, [...p.rows]); ((this.lostRows[p.fleet.id] ??= {})[u.extra.cls as SeaClass] ??= []); }
    }
    // The card counts the coasts' aircraft in each side's strength (navy.ts seaBattles), so the field does: they are
    // worn down with the fleet and a side breaks off on the whole of what it brought (b415).
    this.airLeft = { att: ticket.place.air.att, def: ticket.place.air.def };
    this.weight0.att += this.airLeft.att;
    this.weight0.def += this.airLeft.def;
    for (const f of ticket.place.fleets) { this.fleetLoss.sunk[f.id] = none(); this.fleetLoss.burnt[f.id] = none(); this.fleetLoss.abandoned[f.id] = none(); }
    this.sail = ticket.place.sailors;
    this.ownWater = ticket.place.ownWater === true;
    this.caps = sunkCaps(ticket.place.fleets, ticket.reckoning.odds);
    this.plainCaps = sunkCaps(ticket.place.fleets, ticket.reckoning.odds, true);
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
    // (A ship's abandon-ship call is no order to her helm: it leaves the chase as it was.)
    if (ok && chasing && c.do !== 'dice' && c.do !== 'auto' && c.do !== 'sea:abandon') this.chaseTaken[by] = true;
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
      case 'sea:rescue': {
        // Pick up survivors (form B1): the player's own order, never the staff's. Any piece of the side still answering
        // orders, at any wreck with men alive in the water who will come aboard: not an enemy's men whose navy refuses an
        // enemy's pick-up (sailors.json refuse).
        const rc = c as { units: number[]; wreck?: { unit: number; ship: number } };
        if (!this.sail || !rc.wreck) return false;
        const w = this.wreckFor({ unit: rc.wreck.unit, ship: rc.wreck.ship });
        if (!w || this.aliveIn(this.takers(w, by)) < 0.5) return false;
        const us = mine(rc.units);
        for (const u of us) this.orders.set(u.id, { kind: 'rescue', unit: w.unit, ship: w.ship });
        return us.length > 0;
      }
      case 'sea:abandon': {
        // A ship's abandon-ship call, changed from her fleet's as she is hit (form C1): only with men in the water, and
        // on any piece of the side's still afloat, breaking off or not (a crew can leave a ship that is running).
        const ac = c as { units: number[]; early: boolean };
        if (!this.sail) return false;
        const us = (ac.units ?? []).map((id) => this.byId.get(id)).filter((u): u is SeaUnit => !!u && u.side === by && live(u));
        for (const u of us) this.abandonCall.set(u.id, !!ac.early);
        return us.length > 0;
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
    this.planRings();
    if (this.ownWater) this.poses = new Map(this.units.filter(live).map((u) => [u.id, { hulls: this.hullsOf(u), way: u.extra.way }]));
    for (const u of this.units) if (live(u)) this.steer(u);
    this.poses = null;
    this.combReach();
    if (tick % 10 === 0) { this.sample(); this.watchCarriers(); this.decide(); }
    this.handOwes();
    for (const u of this.units) { u.engaged = undefined; if (live(u)) this.fire(u); }
    this.landStrikes();
    this.recover();
    this.landTorps();
    this.coastAir();
    if (this.sail) { this.pickUp(); this.sinkDue(false); }
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
   *  order to attack (a ship the player has sent at a speed of its own, or on a course, goes as told, a destroyer
   *  piece whose torpedo run has opened goes in: R4, and a piece of the AA ring keeps its place round its carrier: R3). */
  private keepsStation(e: SeaUnit): boolean {
    if (!live(e) || e.state === 'routing' || e.extra.cls === 'cv' || e.extra.speed !== 'cruise' || this.runs.has(e.id) || this.rings[e.side].has(e.id)) return false;
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
    // The enemy salvo she combs this tick (combing), the course it gives her, and the course her helm would have steered
    // without it (for the marks only). Only the branch of a ship fighting at will or under an order to attack sets it:
    // routing, withdrawn, sent on a course or held, she combs nothing, and any comb she had ends (combed).
    let comb: Torp | null = null, combTo = 0, bare = 0, edge = false;
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
      if (d > ARRIVE) want = bearing(u.x, u.y, o.x, o.y);
      else { want = o.face ?? u.face; speed = 0; }
    } else if (o.kind === 'hold') {
      speed = 0;
    } else if (o.kind === 'rescue') {
      // Pick up survivors (form B1): she steams for the wreck at her own speed and lies stopped beside it, as a ship sent to
      // a point stops there. Stopped, she combs no tracks (combing needs way on), and she keeps no line, ring, screen
      // station or fleet's pace: those read only a ship fighting at will or under an attack order. Her guns fire on.
      const w = this.wreckFor(o);
      if (w && dist(u.x, u.y, w.x, w.y) > ARRIVE) want = bearing(u.x, u.y, w.x, w.y);
      else speed = 0;
    } else {
      // An order to attack sets what the guns fire on whenever it is in reach. The ship keeps with the fight as its
      // captain would, engaging the enemy nearest it, and steers for the target only when that lies well beyond the
      // fight: sending every ship at one point drew the line into a knot the enemy fought around (ruling 226, 4).
      // A ship fighting at will, or under a fleet order, steers the same way for its fleet's main effort: ships far from
      // the fight close on it, as the land thread found idle reserves must (b407).
      // A destroyer piece on a torpedo run goes in at flank and turns away (runCourse); a piece of the AA ring keeps its
      // place round its carrier (ringHelm, R3); a screen piece keeps its station on the big ship it guards, or goes for an
      // enemy run near it (station); a carrier keeps its station astern while the enemy brings aircraft (carrierHelm: all
      // action beside a battle line, until the guns meet in a carrier group); with no station, two or more carriers keep
      // their navy's way: on their guide (guideHelm), or each for herself, apart (below).
      const run = u.extra.cls === 'dd' ? this.runCourse(u, o) : null;
      const post: { want: number; speed: number; hold?: number } | null = run ?? this.ringHelm(u) ?? (this.screen.has(u.id) ? this.station(u, o) : null) ?? this.lineHelm(u) ?? this.carrierHelm(u) ?? this.guideHelm(u);
      // Whether she combs a salvo this tick (below), taken here for her speed: a helm that keeps her on a place (her line,
      // her ring, her station, her guide) gives, while she combs, the speed it keeps on that place ('hold'), not the
      // speed it closes on it at, which the comb itself would raise toward flank as it carries her off her place. The comb
      // never changes speed (the ship plan's R6, the form: "Speed is not changed").
      const away = this.groupEvasion(u, false);
      comb = away ? null : this.combing(u);
      if (post) { want = post.want; speed = comb && post.hold !== undefined ? post.hold : post.speed; } else {
        const tgt = this.byId.get(o.kind === 'attack' && !o.fleet ? o.target : this.focus[u.side]) ?? null;
        const near = this.steerFoe(u);
        // A carrier keeps its standoff from the nearest enemy whatever it strikes: its aircraft reach any target.
        const foe = u.extra.cls !== 'cv' && tgt && (!near || dist(u.x, u.y, tgt.x, tgt.y) > dist(u.x, u.y, near.x, near.y) + SEA.ai.beyond) ? tgt : near;
        if (foe) { want = this.engage(u, foe); foeId = foe.id; }
        // A carrier on her own keeps her navy's way with her sisters: each for herself, or out of her guide's formation
        // under air attack, never inside its spacing of a sister (apart); a guide paces her formation at the fleet's
        // keep-together way (guideWaits).
        if (u.extra.cls === 'cv') {
          want = this.apart(u, want);
          if (this.guideWaits(u)) { speed = Math.min(speed, C.speed * SEA.speeds.slow); this.mark(`${u.side}:cvPace:s`, DT); }
        }
      }
      // A carrier group that sees a battle line coming turns away and opens the range while it launches (b389), an
      // attack order or not. Its screen turns with it only within evade.screen metres of a carrier, and that is 0: the
      // screen stays and fights (b402: a screen that ran with its carriers left the plain order short of the card).
      // The AA ring is let go the moment a carrier sights the battle line (R3, planRings), so the screen meets the surface
      // ships here as before. Only an order to steer, hold or withdraw takes a ship away.
      if (away) { want = away.course; speed = Math.min(C.speed * SEA.speeds.flank, away.speed); foeId = -1; this.away[u.id] = true; this.onStation.delete(u.id); }
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
      // Tracks running at her (the ship plan's R6; the design director, 05:17Z): a battleship, cruiser or carrier turns
      // away from an enemy salvo fired at her, or whose water she is in, her stern to the tracks, until it lands (combing,
      // combCourse). It overrides her line, her ring, her station, her guide and her own fight, and the fleet's keeping
      // together, as the record's ships left their station to turn from tracks (Jutland, 31 May 1916: Hercules, Revenge,
      // Colossus; Samar, 25 October 1944: Yamato; the 1944 War Instructions: station rules "are not construed to prevent
      // maneuvers to avoid torpedoes"). Her speed is not changed (above). A carrier group's turn away from a battle line
      // is already a turn away and stands; the field's edge, below, still wins.
      if (comb) { bare = want; want = combTo = this.combCourse(u, comb); foeId = -1; }
    }
    // Her own side's tracks (release 2's own water): a ship with way on that the heads of one of her side's running
    // salvoes would pass through keeps out of its water until they are past her, whatever she is doing: fighting, on a
    // run, breaking off, sent somewhere by the player (the 1944 War Instructions: station rules "are not construed to
    // prevent maneuvers to avoid torpedoes"). A comb of an enemy salvo (above) stands, and the field's edge (below) wins.
    // Not while she combs an enemy salvo, nor while a carrier group turns away from a battle line (its own turn away, kept
    // apart from her sisters, stands, as for the comb; the salvo she kept clear of is kept for after). A line ship keeping
    // clear keeps her place in her line and works back to it after, as from a comb (holdPlace; R6 item 2).
    const steered0 = this.clearSteered.has(u.id);
    if (this.ownWater) {
      const had = this.clearing.has(u.id);
      const clear = comb || this.away[u.id] ? this.stopClear(u) : this.keepClear(u, u.state === 'routing' || o.kind === 'leave');
      if (clear !== null) { want = clear; foeId = -1; this.clearSteered.add(u.id); } else this.clearSteered.delete(u.id);
      const has = this.clearing.has(u.id);
      if (!had && has) this.holdPlace(u); else if (had && !has && this.place.has(u.id)) this.regaining.add(u.id);
    }
    // Stay on the field unless leaving it.
    if (u.state !== 'routing' && o.kind !== 'leave') {
      const m = 800;
      if (u.x < m || u.x > F.size - m || u.y < m || u.y > F.size - m) { want = bare = bearing(u.x, u.y, F.size / 2, F.size / 2); foeId = -1; edge = true; }
    }
    // A keep-clear the edge overrides did not set her course (as for the comb: combSteered). One just ended leaves her
    // turning back to her helm's course: the crossed-T test reads her line's course until she is on it (clearBack).
    if (edge) this.clearSteered.delete(u.id);
    if (this.ownWater) {
      if (this.clearSteered.has(u.id) || edge) this.clearBack.delete(u.id);
      else if (steered0) this.clearBack.add(u.id);
    }
    // A comb the edge overrides is still held until its salvo lands, but did not set her course: the event tells the
    // course she steers, and the crossed-T test reads her own heading (crossed).
    if (comb && !edge) this.combSteered.add(u.id); else this.combSteered.delete(u.id);
    this.combed(u, comb, comb ? want : 0, bare);
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
    if (this.ownWater) {
      this.lay(u);
      if (this.clearBack.has(u.id) && Math.abs(turn(u.face, want)) < 1e-9) this.clearBack.delete(u.id);
    }
    // Out by its own edge: gone home, to fight another day.
    if ((u.state === 'routing' || o.kind === 'leave') && (u.side === 'att' ? u.y < 300 : u.y > F.size - 300)) {
      u.state = 'withdrawn';
      this.clearing.delete(u.id); this.clearSteered.delete(u.id); this.clearHeading.delete(u.id); this.holding.delete(u.id); this.clearBack.delete(u.id);
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
   * (until the guns meet, where the enemy brings aircraft, its escorts ring its carriers, R3, and otherwise they fight
   * each for itself). It keeps until a side breaks off or the enemy has no gun ship
   * left, when the chase runs as before. No dice.
   */
  private planLines(): void {
    const sides: Side[] = ['att', 'def'];
    for (const side of sides) this.groupAt[side] = this.carrierGroup(side);
    for (const side of sides) this.planLine(side);
    // Its T crossed: judged for both sides on the plans just made, then turned.
    const crossed = sides.map((side) => this.crossed(side));
    sides.forEach((side, i) => { if (crossed[i] !== 0 && crossed[i] !== this.lines[side]!.hand) this.turnTogether(side, crossed[i], 'crossed'); });
  }

  /** Whether a side is a carrier group: carriers afloat and no battleship afloat and not breaking off. Taken once a tick,
   *  before any ship steers (groupAt). */
  private carrierGroup(side: Side): boolean {
    const mine = this.units.filter((u) => u.side === side && live(u));
    return mine.some((u) => u.extra.cls === 'cv') && !mine.some((u) => u.extra.cls === 'bb' && u.state !== 'routing');
  }

  /** A side's line this tick: formed, its pieces still in it, its course and speed, turned off the field's edge. */
  private planLine(side: Side): void {
    const P = this.linePlans[side];
    P.t = this.t;
    P.members = [];
    if (this.contactAt === null || this.broken !== null || this.gunless(other(side))) return;
    // A carrier group without battleships keeps its cruisers with its carriers: they form no line (the historian: carrier
    // forces' cruisers screened them and shunned a surface action, Samar, October 1944). Until the guns meet its escorts
    // keep the AA ring round its carriers where the enemy brings aircraft (planRings, R3); after it they fight each for itself.
    if (this.groupAt[side]) return;
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
    // While the guide combs a salvo (R6) her heading is the tracks', not the line's: the test reads the line's course, so
    // a comb neither trips it nor clears it (the design director, 05:17Z; without this, the prototype's line on p0 turned
    // away together about once a battle where none did before). Only while the comb sets her course: where the field's
    // edge overrides it she steers the edge's course, and that is read as any other (combSteered).
    // So too while she keeps clear of her own side's tracks, and turns back after (release 2's own water: clearSteered,
    // clearBack).
    const gf = this.combSteered.has(g.id) || this.clearSteered.has(g.id) || this.clearBack.has(g.id) ? L.course : g.face;
    if (!broadside || Math.abs(turn(gf, bearing(g.x, g.y, ex, ey))) >= N.crossed) return 0;
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
   *  more than ai.keep metres from the middle of the line works back toward it at up to flank. 'hold': the line's speed,
   *  the speed she keeps while she combs a salvo (R6: the comb never changes speed).
   *
   *  A ship whose comb has ended works back the same way to her own place (R6 item 2; the design director, 19:03Z), the
   *  point she held from the line's middle as the comb began, carried on with the line: at flank while it lies ahead of
   *  her beam and at the line's speed while it lies astern, her course the line's bent toward it by up to 0.6 of the
   *  angle, until she is within ai.station of it; then the line steers her as before. The line does not wait for her,
   *  and her guns fire at their arc as ever (the record: no rule for the way back but no risk of collision and
   *  "approximate positions"; Jutland's line came back to its course and then closed; Salt Lake City fired as she closed).
   *  While she combs again the helm she would have steered is this one, so the bank reads it (combBare). One rule for
   *  every navy and both sides, the player's line too. No new number, no random draw. */
  private lineHelm(u: SeaUnit): { want: number; speed: number; hold?: number } | null {
    if (u.extra.cls !== 'bb' && u.extra.cls !== 'ca') return null;
    const P = this.linePlans[u.side], L = this.lines[u.side];
    if (!L || P.t !== this.t || !P.members.includes(u)) { this.place.delete(u.id); this.regaining.delete(u.id); return null; }
    const C = SEA.classes[u.extra.cls as SeaClass];
    const [cx, cy] = this.middle(P.members), at = this.regaining.has(u.id) ? this.place.get(u.id) : undefined;
    const tx = at ? cx + at[0] : cx, ty = at ? cy + at[1] : cy, near = at ? SEA.ai.station : SEA.ai.keep;
    const gap = dist(u.x, u.y, tx, ty);
    if (gap <= near) {
      if (at) { this.place.delete(u.id); this.regaining.delete(u.id); this.mark(`${u.side}:regained`, 1); }
      return { want: L.course, speed: L.speed };
    }
    if (at) this.mark(`${u.side}:regain:s`, DT);
    const pull = Math.min(0.6, (gap - near) / near), back = turn(L.course, bearing(u.x, u.y, tx, ty));
    return { want: norm(L.course + pull * back), speed: Math.cos(back) > 0 ? C.speed * SEA.speeds.flank : L.speed, hold: L.speed };
  }

  /**
   * The AA ring (the ship plan's R3): while the enemy brings aircraft and until the guns first meet, a side with carriers
   * and no battleship keeps its whole escort round them, cruisers and destroyers alike, as the carrier forces of the
   * record cruised (Coral Sea, Midway, Santa Cruz: the screen on circles round the carriers; the 1944 War Instructions
   * put the heavy ships on the 2,000-yard circle and the AA cruiser at 1,000). A piece keeps it at cruising speed, fighting
   * at will or under a fleet order, and not on a torpedo run: an order to attack one ship, a speed or a course of the
   * player's own takes it out, and the plain order does not, so the newcomer's battle stays the staff's (b402, mark 4).
   * The pieces are shared in turn, by id, over the carriers fighting at will or sent to strike (one the player sends on
   * a course, holds or withdraws keeps no ring). At contact the ring is let go and its ships fight as any other: held
   * through the action it kept its guns' weight out of the fight, and the card prices that weight whole (cvcv missed its
   * mark by .13 to .38 in every such form: the design director, 14:36Z, the ring earning its keep by where its guns
   * stand, not by flak, which at the campaign's law takes 0.2 to 0.3% of a strike whatever the ring). It is let go for
   * good, too, once a carrier of its side sights an enemy battleship within evade.see and turns away (evasion): the
   * screen left the carriers to meet surface ships (Samar, October 1944), and a ring that ran with them kept the guns
   * from ever meeting while the battle line followed at its fleet's speed. None once a side has broken off (the chase
   * runs as before) or against an enemy with no gun ship left (the screen runs its carriers down, as before). A fleet
   * with a battle line keeps its line (R1), its carriers astern of it. Planned for both sides at the start of the tick,
   * before any ship steers (ringHelm reads the places as each one steers). No dice.
   */
  private planRings(): void {
    this.onStation.clear();
    for (const side of ['att', 'def'] as Side[]) {
      const ring = this.rings[side];
      ring.clear();
      if (this.ringGone[side] || this.contactAt !== null || this.broken !== null || !this.bringsAir(other(side)) || this.gunless(other(side))) continue;
      const mine = this.units.filter((u) => u.side === side && live(u) && u.state !== 'routing');
      if (mine.some((u) => u.extra.cls === 'bb')) continue;
      const all = mine.filter((u) => u.extra.cls === 'cv');
      if (all.some((c) => this.evasion(c) !== null)) { this.ringGone[side] = true; this.mark(`${side}:ringGone`, this.t); continue; }
      const cvs = all.filter((c) => { const o = this.orders.get(c.id); return !o || o.kind === 'free' || o.kind === 'attack'; }).sort((a, b) => a.id - b.id);
      if (!cvs.length) continue;
      const keeps = mine.filter((u) => {
        if ((u.extra.cls !== 'ca' && u.extra.cls !== 'dd') || u.extra.speed !== 'cruise' || this.runs.has(u.id)) return false;
        const o = this.orders.get(u.id);
        return !o || o.kind === 'free' || (o.kind === 'attack' && !!o.fleet);
      }).sort((a, b) => a.id - b.id);
      const per = new Map<number, number[]>();
      keeps.forEach((u, n) => { const c = cvs[n % cvs.length].id; per.set(c, [...(per.get(c) ?? []), u.id]); });
      for (const [c, ids] of per) ids.forEach((id, i) => ring.set(id, { cv: c, i, k: ids.length }));
    }
  }

  /** A ring piece's course and speed (R3), or null when it keeps no ring: its station torp.station metres from its
   *  carrier (the screen's own distance from the ship it guards, inside the strike's radius, so its guns count against
   *  aircraft and it shares what a strike lands), the ring's places spaced evenly round from the bearing of the enemy's
   *  middle. On station it keeps the carrier's course and way; off it, it closes as a screen piece does. 'hold': the way
   *  it keeps on station, kept while it combs a salvo (R6: the comb never changes speed). */
  private ringHelm(u: SeaUnit): { want: number; speed: number; hold?: number } | null {
    const r = this.rings[u.side].get(u.id);
    if (!r) return null;
    const cv = this.byId.get(r.cv)!, [ex, ey] = this.centre(other(u.side)), R = SEA.torp.station;
    const a = bearing(cv.x, cv.y, ex, ey) + (2 * Math.PI * r.i) / r.k;
    const sx = cv.x + Math.sin(a) * R, sy = cv.y + Math.cos(a) * R, d = dist(u.x, u.y, sx, sy);
    const flank = SEA.classes[u.extra.cls as SeaClass].speed * SEA.speeds.flank;
    this.mark(`${u.side}:ring:s`, DT);
    if (d < 200) return { want: cv.face, speed: Math.min(flank, cv.extra.way) };
    return { want: bearing(u.x, u.y, sx, sy), speed: clamp(d / 20, cv.extra.way, flank), hold: Math.min(flank, cv.extra.way) };
  }

  /**
   * A carrier's station (R3) while the enemy brings aircraft: ai.keep metres astern of the middle of its side's gun ships
   * keeping with the fleet (fighting at will or under a fleet order, at cruising speed, not on a torpedo run, and not in
   * the AA ring), on the line from the enemy's middle through theirs (the 1944 War Instructions: "commanders of carrier
   * groups maneuver their units away from the enemy"; Formidable hauled out to the disengaged side at Matapan and stayed
   * there while the battleships fought; the record's tens of kilometres are beyond the field, and the standoff stands for
   * them), and further back along that line until no enemy lies within its standoff of it; then kept inside the field's
   * edge by the margin the turn away keeps (evasion), which may bring it nearer. Two or more carriers keeping station
   * stand abreast across that line, their navy's spacing apart (carrierWay: the record's one figure for carriers in one
   * group, whatever they stand behind; the design director, 21:00Z: it was torp.station, the screen's own 1,000 m,
   * closer than any figure in the record), each taking the place on its own hand as they lie, so none steams onto
   * another's (at 1,000 m, closing on them from ahead, they passed about 240 m apart as each turned to the fleet's
   * course: recorded, a look for the turning circles). Nearer an enemy
   * than its standoff it opens the range along the line. On station it keeps the gun ships' course and way, and with the
   * gun ships slower than its own slowest speed it keeps way on round its station (a carrier where the enemy's aircraft reach never lay stopped). Null with no such
   * gun ship (a carrier group's escorts all in its ring), with the enemy bringing no aircraft, or for a carrier the player
   * sends at a speed of its own: it then keeps its standoff as before. Once the guns have met it runs only while the side
   * has a battleship afloat and not breaking off, its carriers astern of the line it fights in (Matapan); a carrier group
   * has none after contact, the ring's own window: it is the screen's geometry, so it runs when the screen does (the
   * design director, 14:36Z). Before contact a carrier group's ring holds every gun ship it could stand astern of. Kept
   * on after contact, the group's escorts freed from the ring and fighting, it held the carriers astern through the gun
   * fight and moved cvcvr .05 past its bar; let go at contact in a fleet with battleships as well, its carriers fell back
   * on their standoffs from abreast and crossed 113 m apart. Wherever carriers keep no station (a carrier group's before
   * contact as after) they keep their navy's way (carrierWay): on a guide (guideHelm) or each for herself but never
   * inside its spacing of a sister (apart); each left to her own standoff with neither, a carrier group's came 132-214 m
   * apart in the gun fight on 7 of 12 runs.
   * It runs only while the enemy brings aircraft (applied in
   * every action it moved the uscv mark, its cause not traced).
   * Its turn into the wind to launch is the 3D game's to show (seaWind): the rules keep its course (the design director,
   * 14:36Z: weather stays off the rules, and one wind for both sides would make a battle uneven by its luck).
   * 'hold': the way she keeps on station, kept while she combs a salvo (R6: the comb never changes speed).
   */
  private carrierHelm(u: SeaUnit): { want: number; speed: number; hold?: number } | null {
    if (!this.keepsCarrierStation(u) || !this.bringsAir(other(u.side))) return null;
    if (this.contactAt !== null && !this.units.some((b) => b.side === u.side && b.extra.cls === 'bb' && live(b) && b.state !== 'routing')) return null;
    const guns = this.units.filter((e) => {
      if (e.side !== u.side || !live(e) || e.state === 'routing' || e.extra.cls === 'cv' || e.extra.speed !== 'cruise' || this.runs.has(e.id) || this.rings[u.side].has(e.id)) return false;
      const o = this.orders.get(e.id);
      return !o || o.kind === 'free' || (o.kind === 'attack' && !!o.fleet);
    });
    if (!guns.length) return null;
    const [gx, gy] = this.middle(guns), [ex, ey] = this.centre(other(u.side)), gd = dist(gx, gy, ex, ey);
    if (gd < 1e-6) return null;
    const ux = (gx - ex) / gd, uy = (gy - ey) / gd, C = SEA.classes.cv, so = C.standoff ?? 2500;
    const near = this.nearestFoe(u);
    if (near && dist(u.x, u.y, near.x, near.y) < so) return { want: Math.atan2(ux, uy), speed: C.speed * SEA.speeds.cruise };
    // Abreast across the line, in the order they lie along it (ties by id): the same turned half round for either side.
    const px = uy, py = -ux, across = (c: SeaUnit): number => c.x * px + c.y * py;
    const mates = this.units.filter((c) => c.side === u.side && this.keepsCarrierStation(c)).sort((a, b) => across(a) - across(b) || a.id - b.id);
    const off = (mates.indexOf(u) - (mates.length - 1) / 2) * this.carrierWay(u.side).spacing;
    const bx = gx + px * off, by = gy + py * off;
    // How far astern: ai.keep, or past the point where the line leaves every enemy's standoff (the larger root of
    // |b + u·t - e| = standoff, for each enemy whose standoff the line crosses).
    let back = SEA.ai.keep;
    for (const e of this.units) {
      if (e.side === u.side || !live(e)) continue;
      const wx = bx - e.x, wy = by - e.y, h = wx * ux + wy * uy, q = h * h - (wx * wx + wy * wy - so * so);
      if (q > 0) back = Math.max(back, -h + Math.sqrt(q));
    }
    const F = SEA.field.size, m = EDGE_MARGIN;
    const sx = clamp(bx + ux * back, m, F - m), sy = clamp(by + uy * back, m, F - m);
    const d = dist(u.x, u.y, sx, sy);
    let vx = 0, vy = 0;
    for (const e of guns) { vx += Math.sin(e.face) * e.extra.way; vy += Math.cos(e.face) * e.extra.way; }
    vx /= guns.length; vy /= guns.length;
    const fv = Math.hypot(vx, vy), lo = C.speed * SEA.speeds.slow, hi = C.speed * SEA.speeds.flank;
    this.mark(`${u.side}:cvStation:s`, DT);
    this.onStation.add(u.id);
    if (d < 200) return { want: fv >= lo ? Math.atan2(vx, vy) : d > 1e-6 ? bearing(u.x, u.y, sx, sy) : u.face, speed: clamp(fv, lo, hi) };
    return { want: bearing(u.x, u.y, sx, sy), speed: clamp(d / 20, lo, hi), hold: clamp(fv, lo, hi) };
  }

  /**
   * A side's carriers' way of keeping company where they keep no station (the design director, 20:50Z and 20:56Z: by
   * navy and era, as a 239 style): its navy's path in sea.json 'carriers' ('_' for a navy not named), stepped by the
   * research the side holds (carriers.grow: Carrier task forces), never by a date, as land's styles grow (stepsOf). The
   * record (carrier-spacing-record.md): in 1942 every navy's carriers, and Japan's throughout, manoeuvred each for
   * herself under air attack; the US from late 1943 and the British Pacific Fleet in 1945 sailed as one body on a guide.
   */
  private carrierWay(side: Side): CarrierWay {
    const P = SEA.carriers, path = P.path[this.ticket[side].nat] ?? P.path._, held = this.ticket[side].doctrine ?? [];
    let n = 0;
    while (n < P.grow.length && held.includes(P.grow[n])) n++;
    return path[Math.min(path.length - 1, n)];
  }

  /** Whether a carrier is under direct air attack: an enemy strike is aloft at her, flown off and not yet landed. */
  private underAir(u: SeaUnit): boolean {
    return this.strikes.some((k) => k.side !== u.side && k.target === u.id);
  }

  /** Whether a carrier keeps formation on a guide: as she would keep station (keepsCarrierStation), and not under direct
   *  air attack (underAir): a carrier under attack swung on her own (the record), out of the guide's formation. */
  private inFormation(c: SeaUnit): boolean {
    return this.keepsCarrierStation(c) && !this.underAir(c);
  }

  /**
   * Carriers steaming on their guide, where their navy's way is the guide's (carrierWay; the design director, 19:12Z
   * form G, 20:50Z by navy and era): wherever a side's carriers keep no station (carrierHelm), before the guns meet as
   * after (a carrier group's ring holds every gun ship she could stand astern of, so it has none before contact either:
   * with the guide only after contact its carriers drew 7 km apart first and a follower never reached her place), the
   * first of them by id in formation (inFormation) is the guide and keeps her standoff as before (engage), pacing her
   * formation (guideWaits); each other takes her place abreast of the guide, her navy's spacing from her across the line
   * from the enemy's middle through her, in the order they lie across it (ties by id), as on station, closing on it at up
   * to flank and on it keeping the guide's course and way (with the guide slower than slow, keeping way on round it at
   * slow, as on station); the place is kept inside the field's edge as on station, which may bring it nearer. The guide
   * sunk, breaking off, sent at a speed of her own, under an order to steer, hold or withdraw, or under direct air
   * attack, the next by id in formation is the guide; a carrier out of formation goes as she is told, or steers her own
   * standoff. The game marks no flagship among a fleet's carriers, so none is preferred. The record: the US cruising
   * dispositions from late 1943 (one ship the guide, every other keeping bearing and distance on it, the formation
   * turning together), the British Pacific Fleet's in 1945. Null for a lone carrier, for the guide, for a carrier out of
   * formation, and where her navy's way is each for herself (apart). 'hold': the way she keeps on her
   * place, kept while she combs a salvo (R6: the comb never changes speed).
   */
  private guideHelm(u: SeaUnit): { want: number; speed: number; hold?: number } | null {
    const p = this.guidePlace(u);
    if (!p) return null;
    const { guide, sx, sy } = p, C = SEA.classes.cv;
    const d = dist(u.x, u.y, sx, sy), fv = guide.extra.way, lo = C.speed * SEA.speeds.slow, hi = C.speed * SEA.speeds.flank;
    this.mark(`${u.side}:cvGuide:s`, DT);
    if (d < 200) return { want: fv >= lo ? guide.face : d > 1e-6 ? bearing(u.x, u.y, sx, sy) : u.face, speed: clamp(fv, lo, hi) };
    return { want: bearing(u.x, u.y, sx, sy), speed: clamp(d / 20, lo, hi), hold: clamp(fv, lo, hi) };
  }

  /** A carrier's guide and her place abreast of the guide (guideHelm), or null where she steams on no guide. */
  private guidePlace(u: SeaUnit): { guide: SeaUnit; sx: number; sy: number } | null {
    if (!this.inFormation(u)) return null;
    const way = this.carrierWay(u.side);
    if (way.form !== 'guide') return null;
    const group = this.units.filter((c) => c.side === u.side && this.inFormation(c));
    if (group.length < 2) return null;
    const guide = group.reduce((a, b) => (b.id < a.id ? b : a));
    if (guide === u) return null;
    const [ex, ey] = this.centre(other(u.side)), gd = dist(guide.x, guide.y, ex, ey);
    if (gd < 1e-6) return null;
    const ux = (guide.x - ex) / gd, uy = (guide.y - ey) / gd, px = uy, py = -ux;
    const across = (c: SeaUnit): number => c.x * px + c.y * py;
    group.sort((a, b) => across(a) - across(b) || a.id - b.id);
    const off = (group.indexOf(u) - group.indexOf(guide)) * way.spacing;
    const F = SEA.field.size, m = EDGE_MARGIN;
    return { guide, sx: clamp(guide.x + px * off, m, F - m), sy: clamp(guide.y + py * off, m, F - m) };
  }

  /** Whether a guide paces her formation (the design director, 20:50Z: she keeps her standoff at a way her followers can
   *  close on, so they reach their places): a carrier steaming on her lies more than her navy's spacing from her place.
   *  She then makes the way a fleet keeping together makes for its rearmost: slow (steer). */
  private guideWaits(u: SeaUnit): boolean {
    if (u.extra.cls !== 'cv') return false;
    const way = this.carrierWay(u.side);
    for (const c of this.units) {
      if (c === u || c.side !== u.side || c.extra.cls !== 'cv') continue;
      const p = this.guidePlace(c);
      if (p && p.guide === u && dist(c.x, c.y, p.sx, p.sy) > way.spacing) return true;
    }
    return false;
  }

  /**
   * Each for herself, where that is her navy's way (carrierWay; the design director, 20:50Z): a carrier steering off her
   * own enemy (engage, its pull to the fleet's middle included) never closes inside her navy's spacing from a sister:
   * with the nearest sister afloat inside it and her course closing on her, she turns to the course square to the line
   * between them on the side nearer her course, neither closing nor opening by will. It binds her under direct air
   * attack too, in either way (the design director, 21:00Z): she swings on her own, out of her guide's formation where
   * she keeps one, but not into a sister (with no line under attack, both carriers struck at once came 9-80 m apart at
   * Japan's 2,000 m in 4 of 12 runs). A guide and her followers keep their places and keep no line. A carrier sent at a
   * speed of her own, or under an order to steer, hold or withdraw, goes as told and keeps no line. The
   * record: in 1942 every navy's carriers, and Japan's throughout, manoeuvred each for herself, drawing apart by miles;
   * let each steer off her own nearest enemy with no line, a carrier group's carriers came 132-214 m apart in the gun
   * fight on 7 of 12 runs (R3). A carrier combing a salvo ('combs': R6) is always held to it, in every navy, whatever
   * order she combs under and at a speed of her own too, as under air attack (the design director, 05:17Z, Q3: her
   * navy's spacing binds a combing carrier as it binds one under air attack, 21:00Z and 23:10Z).
   */
  private apart(u: SeaUnit, want: number, combs = false): number {
    const way = this.carrierWay(u.side);
    if (!combs && (!this.keepsCarrierStation(u) || (way.form === 'guide' && !this.underAir(u)))) return want;
    let near: SeaUnit | null = null, nd = way.spacing;
    for (const c of this.units) {
      if (c === u || c.side !== u.side || c.extra.cls !== 'cv' || !live(c)) continue;
      const d = dist(u.x, u.y, c.x, c.y);
      if (d < nd) { nd = d; near = c; }
    }
    if (!near) return want;
    const b = bearing(u.x, u.y, near.x, near.y), t = turn(b, want);
    if (Math.abs(t) >= Math.PI / 2) return want;
    // R3's mark counts her own helm's course (the call in steer); a combing carrier's call (combCourse) is R6's own mark,
    // so a comb neither counts R3's seconds twice nor adds to them.
    this.mark(combs ? `${u.side}:combApart:s` : `${u.side}:cvApart:s`, DT);
    return norm(b + (t >= 0 ? 1 : -1) * Math.PI / 2);
  }

  /** Whether a carrier takes its station (carrierHelm) when there is one to take: afloat, not breaking off, at cruising
   *  speed, fighting at will or sent to strike (one the player sends at a speed of its own goes as told). */
  private keepsCarrierStation(c: SeaUnit): boolean {
    if (c.extra.cls !== 'cv' || !live(c) || c.state === 'routing' || c.extra.speed !== 'cruise') return false;
    const o = this.orders.get(c.id);
    return !o || o.kind === 'free' || o.kind === 'attack';
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
    const a = bearing(bb.x, bb.y, u.x, u.y), F = SEA.field, m = EDGE_MARGIN, ahead = 2000;
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

  // ---- Her own side's tracks (release 2's own water) -----------------------------------------------------------------

  /** Where a salvo fired at a ship now is aimed: where she will be by the time it lands, on her course and way (launch). */
  private aimOf(foe: SeaUnit): [number, number] {
    return aimAt(foe.x, foe.y, foe.face, foe.extra.way);
  }

  /**
   * When the heads of a salvo would pass through a ship on her present course and way: the seconds from now, or null if
   * they never do (release 2's own water). The salvo runs from the tubes (x0, y0) to its aim point (x, y) in torp.travel
   * seconds from t0, its tracks spreading as they go: its water is the fan from the tubes, where it is as narrow as
   * the tubes, there as wide as the launcher's beam (tubes: half of it), to torp.radius either side of the aim point more
   * (the zone the 3D game draws there). The piece is her ships as the 3D game lays them (hullsOf), each the whole of her
   * from stern to bow, widened by half her beam, going on as she goes now (or on course 'face', for a course she might
   * take: clearAway). The heads pass through her if some point of a hull is in the fan at the moment they come level with
   * it, before they reach the aim point: for a point k of her length from her middle, where it lies along and across the
   * tracks, when the heads reach it and where it then is are each straight-line in k, so the points that are met make one
   * stretch of her, found exactly, and the soonest of them is at one of its ends. 'pose', her ships and way as the tick
   * began (clearAim's, so that what a launcher reads does not hang on the order the pieces steer in). Geometry of the
   * field's own numbers only: no new number, no dice.
   */
  private meets(u: SeaUnit, x0: number, y0: number, x: number, y: number, t0: number, tubes: number, face?: number, pose?: Pose): number | null {
    const L = dist(x0, y0, x, y), T = SEA.torp.travel;
    if (L < 1) return null;
    const ux = (x - x0) / L, uy = (y - y0) / L, V = L / T, R = SEA.torp.radius;
    const h = V * Math.max(0, this.t - t0), left = T - Math.max(0, this.t - t0);
    const len = SEA.classes[u.extra.cls as SeaClass].len, beam = tubes + u.w / 2, way = pose ? pose.way : u.extra.way;
    let best: number | null = null;
    for (const [hx, hy, hf] of pose ? pose.hulls : this.hullsOf(u)) {
      const hv = face ?? hf, vx = Math.sin(hv) * way, vy = Math.cos(hv) * way;
      const va = vx * ux + vy * uy, vb = vx * uy - vy * ux, W = V - va;
      if (W <= 1e-9) continue;
      // Each as [at her middle, per length of her toward her bow].
      const sx = Math.sin(hf) * len, sy = Math.cos(hf) * len;
      const a0 = [(hx - x0) * ux + (hy - y0) * uy, sx * ux + sy * uy], b0 = [(hx - x0) * uy - (hy - y0) * ux, sx * uy - sy * ux];
      const tau = [(a0[0] - h) / W, a0[1] / W];
      const a = [a0[0] + va * tau[0], a0[1] + va * tau[1]], b = [b0[0] + vb * tau[0], b0[1] + vb * tau[1]];
      const room = [(R * a[0]) / L + beam, (R * a[1]) / L];
      // Every bound as f0 + f1 k <= 0: the heads yet to reach her, before the salvo lands, short of the aim point, inside
      // the fan either side.
      let lo = -0.5, hi = 0.5;
      for (const [f0, f1] of [[-tau[0], -tau[1]], [tau[0] - left, tau[1]], [a[0] - L, a[1]], [b[0] - room[0], b[1] - room[1]], [-b[0] - room[0], -b[1] - room[1]]]) {
        if (Math.abs(f1) < 1e-12) { if (f0 > 0) { lo = 1; break; } continue; }
        if (f1 > 0) hi = Math.min(hi, -f0 / f1); else lo = Math.max(lo, -f0 / f1);
      }
      if (lo > hi) continue;
      const soonest = Math.max(0, Math.min(tau[0] + tau[1] * lo, tau[0] + tau[1] * hi));
      if (best === null || soonest < best) best = soonest;
    }
    return best;
  }

  /**
   * Where a destroyer piece's salvo at a ship would be aimed, if she may launch it now, else null (release 2's own water;
   * with it off, always where launch has always aimed: aimOf). Held when another ship of her side afloat, with her way as
   * it is, would be in the tracks' way when the heads come level with her (meets, from the tubes now to the aim point).
   * While the pieces steer, every ship, the target too, is read as the tick began (poses), so what a launcher reads does
   * not hang on the order the pieces steer in, nor on which side steers first. Held, she runs on as before and launches
   * the first second it is clear: a piece on a run keeps closing at flank (runCourse), a screen piece looks again each
   * second (startRuns). The crews' rule in the record: a ship of one's own in the line of fire fouled the range, and the
   * tubes waited.
   */
  private clearAim(u: SeaUnit, foe: SeaUnit): [number, number] | null {
    if (!this.ownWater) return this.aimOf(foe);
    const pf = this.poses?.get(foe.id);
    const [x, y] = pf ? aimAt(pf.hulls[0][0], pf.hulls[0][1], pf.hulls[0][2], pf.way) : this.aimOf(foe);
    // (The ship in the way, for the look field only: the one the heads would reach soonest, then the nearer the tubes.)
    let by: SeaUnit | null = null, bt = Infinity, bd = Infinity;
    for (const f of this.units) {
      if (f === u || f.side !== u.side || !live(f)) continue;
      const pose = this.poses?.get(f.id);
      const tau = this.meets(f, u.x, u.y, x, y, this.t, u.w / 2, undefined, pose);
      if (tau === null) continue;
      const d = pose ? dist(u.x, u.y, pose.hulls[0][0], pose.hulls[0][1]) : dist(u.x, u.y, f.x, f.y);
      if (tau < bt - 1e-9 || (Math.abs(tau - bt) <= 1e-9 && d < bd)) { by = f; bt = tau; bd = d; }
    }
    if (!by) { this.holding.delete(u.id); return [x, y]; }
    // (A new hold, unless she was held at her last look: each tick on a run, each second on the screen.)
    const was = this.holding.get(u.id);
    if (!was || was.t < this.t - 1 - 1e-6) this.mark(`${u.side}:torpHeld`, 1);
    this.holding.set(u.id, { by: by.id, t: this.t });
    this.mark(`${u.side}:torpHeld:s`, this.runs.has(u.id) ? DT : 1);
    return null;
  }

  /**
   * The course a ship steers this tick to keep out of her own side's torpedo water, or null (release 2's own water). WHO:
   * every ship with way on, of every class, whatever she is doing, from the tick after a salvo's launch (seen by every
   * ship at once, as an enemy salvo is: runningAt), for any salvo of her side but her own whose heads would pass through
   * her (meets). WHICH: the one she kept clear of last tick, while its heads are not yet past her; else the one they would
   * reach soonest, then the one needing the smaller turn, then to starboard (as combSooner), never by unit id. HOW: square
   * off its tracks on the side of them she lies (clearCourse), at her own speed, unless that would carry her into another
   * own salvo's water (clearAway), until its heads are past every point of her column or it lands; then her helm as
   * before. Marks only: turns begun and seconds held.
   */
  private keepClear(u: SeaUnit, out: boolean): number | null {
    if (u.extra.way <= 0) { this.clearing.delete(u.id); this.clearHeading.delete(u.id); return null; }
    const held = this.clearing.get(u.id);
    if (held && held.at > this.t && this.notPast(u, held)) { this.mark(`${u.side}:clear:s`, DT); return this.clearAway(u, held, out); }
    let best: Torp | null = null, bt = Infinity;
    for (const s of this.torps) {
      if (s.side !== u.side || s.from === u.id || s.weight <= 0 || s.t0 >= this.t || s.at <= this.t) continue;
      const tau = this.meets(u, s.x0, s.y0, s.x, s.y, s.t0, (this.byId.get(s.from)?.w ?? 0) / 2);
      if (tau === null) continue;
      if (!best || tau < bt - 1e-9 || (Math.abs(tau - bt) <= 1e-9 && this.clearSooner(u, s, best))) { best = s; bt = tau; }
    }
    if (!best) { this.clearing.delete(u.id); this.clearHeading.delete(u.id); return null; }
    if (best !== held) { this.mark(`${u.side}:clear`, 1); this.mark(`${u.side}:clear>${u.extra.cls}`, 1); this.clearHeading.delete(u.id); }
    this.clearing.set(u.id, best);
    this.mark(`${u.side}:clear:s`, DT);
    return this.clearAway(u, best, out);
  }

  /**
   * The course a ship keeping clear of salvo 's' steers (keepClear): square off its tracks on the side of them she lies
   * (clearCourse), unless the heads of some other running salvo of her side would then cross her (two salvoes running
   * side by side, she between them); then, in turn, along its tracks, square off on the other side, back up its tracks:
   * the first of these that no own salvo's heads would cross, as she lies now at her own speed (meets, on that course).
   * Once on one she holds it while none would cross her on it (a captain steadies on a clear course), judged again each
   * tick as she swings: held while it has gone foul, the heads crossed her more often than a helm that sometimes goes
   * from one hand to the other. A ship breaking off takes none that would carry her out over the field's side, as
   * breakOffCourse never does. If every one is crossed,
   * square off on her side, as with one salvo. The order is the field's own geometry, never a unit id (b402).
   */
  private clearAway(u: SeaUnit, s: Torp, out: boolean): number {
    const side = this.clearCourse(u, s), F = SEA.field.size, m = 800;
    const offSide = (c: number): boolean => out && ((u.x < m && Math.sin(c) < -1e-9) || (u.x > F - m && Math.sin(c) > 1e-9));
    const ways = [side, norm(s.dir), norm(side + Math.PI), norm(s.dir + Math.PI)].filter((c) => !offSide(c));
    const first = ways.length ? ways[0] : side;
    const running = this.torps.filter((o) => o.side === u.side && o.from !== u.id && o.weight > 0 && o.t0 < this.t && o.at > this.t && this.notPast(u, o));
    if (running.length < 2) { this.clearHeading.delete(u.id); return first; }
    const clear = (c: number): boolean => running.every((o) => this.meets(u, o.x0, o.y0, o.x, o.y, o.t0, (this.byId.get(o.from)?.w ?? 0) / 2, c) === null);
    const was = this.clearHeading.get(u.id);
    if (was !== undefined && !offSide(was) && clear(was)) return was;
    for (const c of ways) {
      if (clear(c)) { this.clearHeading.set(u.id, c); return c; }
    }
    this.clearHeading.delete(u.id);
    return first;
  }

  /** Keeps clear of nothing this tick (she combs, or her carrier group turns away): the course is let go, the salvo kept,
   *  so that taking it up again after is no new turn. */
  private stopClear(u: SeaUnit): null {
    this.clearHeading.delete(u.id);
    return null;
  }

  /** Whether a salvo's heads have yet to come level with some point of one of a piece's ships (keepClear). */
  private notPast(u: SeaUnit, s: Torp): boolean {
    const L = dist(s.x0, s.y0, s.x, s.y);
    if (L < 1) return false;
    const ux = (s.x - s.x0) / L, uy = (s.y - s.y0) / L, h = (L * Math.max(0, this.t - s.t0)) / SEA.torp.travel;
    const len = SEA.classes[u.extra.cls as SeaClass].len;
    return this.hullsOf(u).some(([hx, hy, hf]) => HULL.some((k) => {
      const a = (hx + Math.sin(hf) * k * len - s.x0) * ux + (hy + Math.cos(hf) * k * len - s.y0) * uy;
      return a >= h && a <= L;
    }));
  }

  /**
   * A piece's ships where the 3D game lays them (own water; SeaShow.cpp's columns): her leader at the piece's place on
   * its heading, the rest of her ships afloat astern of him along the way he came, as far apart as the piece's length
   * gives (pieces.ts: three lengths), each heading along her own stretch of it; where the wake runs out, straight on
   * astern. Each as [x, y, heading].
   */
  private hullsOf(u: SeaUnit): [number, number, number][] {
    const len = SEA.classes[u.extra.cls as SeaClass].len, N = u.extra.ships0;
    const gap = N > 1 && u.d > len ? (u.d - len) / (N - 1) : 3 * len;
    const out: [number, number, number][] = [[u.x, u.y, u.face]];
    const wake = this.trail.get(u.id) ?? [];
    let px = u.x, py = u.y, back = 0, i = 0, dir = u.face;
    for (let k = 1; k < u.extra.ships; k++) {
      const want = k * gap;
      for (;;) {
        if (i < wake.length) {
          const [qx, qy] = wake[i], seg = dist(px, py, qx, qy);
          if (seg > 1e-6) dir = bearing(qx, qy, px, py);
          if (back + seg >= want) { const f = (want - back) / Math.max(seg, 1e-6); out.push([px + (qx - px) * f, py + (qy - py) * f, dir]); break; }
          back += seg; px = qx; py = qy; i++;
        } else {
          out.push([px - Math.sin(dir) * (want - back), py - Math.cos(dir) * (want - back), dir]);
          break;
        }
      }
    }
    return out;
  }

  /** Lays a point of a piece's wake once she has run a length of hers from the last, and lets go what lies further astern
   *  than her rearmost ship (hullsOf). */
  private lay(u: SeaUnit): void {
    const len = SEA.classes[u.extra.cls as SeaClass].len;
    let wake = this.trail.get(u.id);
    if (!wake) { wake = []; this.trail.set(u.id, wake); }
    if (wake.length && dist(u.x, u.y, wake[0][0], wake[0][1]) < len) return;
    wake.unshift([u.x, u.y]);
    let run = 0;
    for (let i = 1; i < wake.length; i++) {
      run += dist(wake[i - 1][0], wake[i - 1][1], wake[i][0], wake[i][1]);
      if (run > u.d + len) { wake.length = i + 1; break; }
    }
  }

  /** The course square off a salvo's tracks, on the side of them her middle lies; on them, the smaller turn, then to
   *  starboard (keepClear). */
  private clearCourse(u: SeaUnit, s: Torp): number {
    const b = (u.x - s.x0) * Math.cos(s.dir) - (u.y - s.y0) * Math.sin(s.dir);
    const right = norm(s.dir + Math.PI / 2), left = norm(s.dir - Math.PI / 2);
    if (b > 1e-6) return right;
    if (b < -1e-6) return left;
    return Math.abs(turn(u.face, left)) < Math.abs(turn(u.face, right)) - 1e-6 ? left : right;
  }

  /** Of two salvoes whose heads would reach her at the same moment, whether she keeps clear of 'a' first: the smaller
   *  turn, then the one further to starboard (keepClear; as combSooner). */
  private clearSooner(u: SeaUnit, a: Torp, b: Torp): boolean {
    const ta = turn(u.face, this.clearCourse(u, a)), tb = turn(u.face, this.clearCourse(u, b));
    if (Math.abs(Math.abs(ta) - Math.abs(tb)) > 1e-6) return Math.abs(ta) < Math.abs(tb);
    return ta > tb;
  }

  // ---- Combing the tracks (the ship plan's R6) -----------------------------------------------------------------------

  /**
   * The enemy salvo a battleship, cruiser or carrier combs this tick, or null (the ship plan's R6; the design director,
   * 05:17Z). WHO: the ship it was fired at, and every battleship, cruiser or carrier of her side within torp.radius of
   * its aim point (its torpedo water, the zone the 3D game draws), each for herself: in the record a ship turned from
   * tracks running at her whether or not they were aimed at her (Yamato off Samar, 25 October 1944, from torpedoes that
   * missed Haruna; at Jutland, 31 May 1916, single ships of the line from tracks fired at the line). No destroyer combs:
   * the design director's "heavy ships" (12:02Z). WHEN: from the tick after the launch (a salvo launched this tick is
   * seen by every ship at once on the next, whatever the order the pieces steer in), while it runs with weight. HOW
   * LONG: the one she combed last tick, until it lands (torp.travel; Q4), whether or not she is still in its water; then
   * the next. Of several, the one launched first; of several launched in the same tick, the one needing the smaller turn,
   * then the one further to starboard (combTurn), never by unit id, so the choice is the same with the field turned half
   * round (b402). One rule for every navy (Q6). The build's stage holds parts of it off (r6Held). No random draw, no new
   * number.
   */
  private combing(u: SeaUnit): Torp | null {
    const c = u.extra.cls;
    if ((c !== 'bb' && c !== 'ca' && c !== 'cv') || !this.torps.length || this.r6Held(u)) return null;
    const held = this.combs.get(u.id);
    if (held && this.runningAt(u, held)) return held;
    let best: Torp | null = null;
    for (const s of this.combable(u)) if (!best || s.t0 < best.t0 || (s.t0 === best.t0 && this.combSooner(u, s, best))) best = s;
    return best;
  }

  /** An enemy salvo still running with weight, launched before this tick. */
  private runningAt(u: SeaUnit, s: Torp): boolean {
    return s.side !== u.side && s.weight > 0 && s.t0 < this.t && s.at > this.t;
  }

  /** The enemy salvoes running that she would take up now: fired at her, or (from the build's step 2) with her in their
   *  water, torp.radius of the aim point. */
  private combable(u: SeaUnit): Torp[] {
    return this.torps.filter((s) => this.runningAt(u, s) && (s.target === u.id || (this.r6Step >= 2 && dist(u.x, u.y, s.x, s.y) <= SEA.torp.radius)));
  }

  /** The turn from her heading onto a salvo's comb course, + to starboard: with the tracks dead at her bow, the half turn
   *  to starboard (combCourse). */
  private combTurn(u: SeaUnit, s: Torp): number {
    const d = turn(u.face, s.dir);
    return Math.abs(d) > Math.PI - 1e-6 ? Math.PI : d;
  }

  /** Of two salvoes launched in the same tick, whether she takes 'a' before 'b': the smaller turn, then the one further
   *  to starboard (as engage's hand and the line's, within the same tolerance). Turns within the tolerance on the same
   *  hand go to starboard too, so the launchers' order never decides between them; only salvoes on the very same
   *  bearing, which give her the same course, are left in the order they were launched. */
  private combSooner(u: SeaUnit, a: Torp, b: Torp): boolean {
    const ta = this.combTurn(u, a), tb = this.combTurn(u, b);
    if (Math.abs(Math.abs(ta) - Math.abs(tb)) > 1e-6) return Math.abs(ta) < Math.abs(tb);
    return ta > tb;
  }

  /**
   * The course a ship combing a salvo steers (R6; the design director, 05:17Z): away, her stern to the tracks, on their
   * own bearing from the launcher's place at the launch to the aim point ('dir'), the shorter way round. With the tracks
   * dead at her bow she turns to starboard, as engage does and as the line's hand does (turn gives +pi for exactly +pi
   * and -pi for exactly -pi: the rounding trap engage guards with the same tolerance). Away is the record's way in every
   * case that gives one: the Grand Fleet at Jutland in 1916, the US cruisers at Empress Augusta Bay in 1943, Yamato at
   * Samar in 1944, the 1944 War Instructions' "turn away". A carrier keeps her navy's spacing from a sister (apart: Q3).
   */
  private combCourse(u: SeaUnit, s: Torp): number {
    const want = Math.abs(turn(u.face, s.dir)) > Math.PI - 1e-6 ? norm(u.face + Math.PI - 1e-6) : s.dir;
    return u.extra.cls === 'cv' ? this.apart(u, want, true) : want;
  }

  /**
   * Whether the build's stage holds her comb off (r6Step; the design director, 05:17Z, Q2): at steps 1 and 2, every ship
   * of a carrier group, a side with carriers and no battleship still fighting (R1's own test, carrierGroup), taken once
   * at the start of each tick as R1 takes it (groupAt), so the order the pieces steer in never changes it. (Step 1's
   * other hold, the torpedo water, is in combable.) A build stage only: it goes at step 3.
   */
  private r6Held(u: SeaUnit): boolean {
    return this.r6Step < 3 && this.groupAt[u.side];
  }

  /** Her place in her line from its middle, as a turn off her line's course begins (a comb, R6 item 2; or keeping clear
   *  of her own side's tracks), unless she holds one already that she has not yet worked back to: the point she held
   *  before the first. Only for a line ship inside the line's keep (place). */
  private holdPlace(u: SeaUnit): void {
    const P = this.linePlans[u.side];
    if (this.r6Back && !this.place.has(u.id) && this.lines[u.side] && P.t === this.t && P.members.includes(u)) {
      const [cx, cy] = this.middle(P.members);
      if (dist(u.x, u.y, cx, cy) <= SEA.ai.keep) this.place.set(u.id, [u.x - cx, u.y - cy]);
    }
  }

  /**
   * What a ship's comb this tick leaves behind (R6): the salvo she combs, kept for the next tick (combing, crossed); a
   * 'sea:comb' event each time she takes one, the first and each switch to the next, with the launcher ('from'), the
   * course she steers this tick (her comb course, or the field's edge's where it wins), the second it lands ('until')
   * and the ship it was fired at ('fired': hers or a line mate's), for the 3D game. The event has no words: the dispatch
   * box shows the latest line with words for 8 s, so a comb's line the tick after a launch would put out "X fires
   * torpedoes at Y." at once; whether and how a comb is told is the UI director's. And the marks, measured only (nothing
   * in the rules reads them): combs by class, those on a salvo fired at another ship (combWater), seconds combing (in her
   * line), switches and those wider than a right angle, combs begun before a break, the same salvo taken up again after
   * an order or a rout let it go (combResume, not a new comb), and salvoes taken by the same-tick tie (combTie). The
   * marks that read where the enemy lies are taken once every piece has steered (combReach).
   */
  private combed(u: SeaUnit, s: Torp | null, course: number, bare: number): void {
    const was = this.combs.get(u.id);
    if (!s && !was) return;
    const side = u.side, cls = u.extra.cls;
    if (!s) {
      this.combs.delete(u.id);
      this.combEnded.add(u.id);
      if (this.place.has(u.id)) this.regaining.add(u.id);
      return;
    }
    if (s !== was) {
      if (!was) {
        // Her place in her line as her comb begins (R6 item 2), unless she holds one already from a comb she has not yet
        // worked back from: the point she held before the first.
        this.holdPlace(u);
        if (this.combLast[u.id] === s) this.mark(`${side}:combResume`, 1);
        else {
          this.mark(`${side}:combs`, 1);
          this.mark(`${side}:combs>${cls}`, 1);
          if (s.target !== u.id) this.mark(`${side}:combWater`, 1);
          if (this.broken === null) this.mark(`${side}:combBeforeBreak`, 1);
          if (this.combable(u).filter((x) => x.t0 === s.t0).length > 1) this.mark(`${side}:combTie`, 1);
        }
        this.combBegun.add(u.id);
      } else {
        this.mark(`${side}:combSwitch`, 1);
        if (Math.abs(turn(this.combTo[u.id], course)) > Math.PI / 2) this.mark(`${side}:combSwitchWide`, 1);
        if (this.combable(u).filter((x) => x.t0 === s.t0).length > 1) this.mark(`${side}:combTie`, 1);
      }
      this.say({ what: 'sea:comb', unit: u.id, side, text: '', extra: { from: s.from, course: Math.round(norm(course) * 100) / 100, until: Math.round(s.at * 10) / 10, fired: s.target } });
    }
    this.combs.set(u.id, s);
    this.combLast[u.id] = s;
    this.combBare[u.id] = bare;
    this.combTo[u.id] = course;
    this.mark(`${side}:comb:s`, DT);
    this.mark(`${side}:comb>${cls}:s`, DT);
    if (this.linePlans[side].members.includes(u)) this.mark(`${side}:combInLine:s`, DT);
  }

  /**
   * The comb marks that read where the enemy lies (R6; measured only, nothing in the rules reads them), taken once every
   * piece has steered this tick, so the order the pieces steer in never moves them: seconds combing with a gun target in
   * reach (combGun:s, never a carrier's) and with no enemy in her class's reach (combSilent:s), whether an enemy lay in
   * reach as a comb began, and combs that began with one and ended with none (combEndOut).
   */
  private combReach(): void {
    const inReach = (u: SeaUnit): boolean => {
      const near = this.nearestFoe(u);
      return !!near && dist(u.x, u.y, near.x, near.y) <= SEA.classes[u.extra.cls as SeaClass].reach;
    };
    for (const id of this.combEnded) {
      const u = this.byId.get(id)!;
      if (this.combIn[id] && !inReach(u)) this.mark(`${u.side}:combEndOut`, 1);
    }
    for (const [id] of this.combs) {
      const u = this.byId.get(id)!;
      if (!live(u)) continue;
      const near = inReach(u), C = SEA.classes[u.extra.cls as SeaClass];
      if (this.combBegun.has(id)) this.combIn[id] = near;
      const tgt = u.extra.target >= 0 ? this.byId.get(u.extra.target) : undefined;
      if (u.extra.cls !== 'cv' && tgt && live(tgt) && dist(u.x, u.y, tgt.x, tgt.y) <= C.reach) this.mark(`${u.side}:combGun:s`, DT);
      if (!near) this.mark(`${u.side}:combSilent:s`, DT);
    }
    this.combBegun.clear();
    this.combEnded.clear();
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
    // While she combs (R6), her fire at her own angle against her fire at the angle of the course her helm would have
    // steered without the comb, every other term the same (combLost, signed, measured). What her guns could not bring to
    // bear is the bank's, as a destroyer's idle guns' weight is (store; the design director, 11:58Z, by 08:12Z (a): the
    // bank is the weight the guns could not fire), every class and every navy.
    if (this.combs.has(u.id)) {
      const arc0 = SEA.fire.endOn + (1 - SEA.fire.endOn) * Math.abs(Math.sin(turn(this.combBare[u.id], bearing(u.x, u.y, foe.x, foe.y))));
      const lost = SEA.fire.rate * this.output(u) * range * (arc0 - arc) * nerve * this.day(u.side) * DT;
      this.mark(`${u.side}:combLost`, lost);
      if (lost > 0) this.store(u, lost);
    }
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
    // What a battleship or cruiser banked while she combed goes with her first blow once the comb is over, on the ship her
    // guns bear on, as a destroyer's bank lands (take): the weight her guns could not fire, fired, and no more.
    if (u.extra.cls !== 'dd' && this.bank[u.id] > 0 && !this.combs.has(u.id)) {
      const b = this.bank[u.id];
      this.bank[u.id] = 0;
      w += b;
      this.mark(`${u.side}:combPaid`, b);
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
  private smoke(u: SeaUnit, kind: 'sunk' | 'burning'): Zone {
    const n = this.zones.filter((z) => z.unit === u.id).length, F = SEA.field.size;
    const along = (mix(u.id * 613 + n * 389 + 3) - 0.5) * u.d * 0.8;
    const x = clamp(u.x + along * Math.sin(u.face), 2, F - 2), y = clamp(u.y + along * Math.cos(u.face), 2, F - 2);
    const z: Zone = {
      id: ++this.zoneId, kind, x: Math.round(x), y: Math.round(y), r: Math.round(SEA.classes[u.extra.cls as SeaClass].len * 0.6),
      until: this.ticket.limit, side: u.side, at: Math.round(this.t * 10) / 10, unit: u.id,
    };
    this.zones.push(z);
    return z;
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
    // Ships by class (plan 4.7): which of the piece's hulls she is, by a fixed rotation over those still with it (the
    // piece's losses so far, modulo the hulls left; no draw): one row only, unless rows were merged past the limit.
    const hs = this.hullsLeft.get(u.id), row = hs?.length ? hs.splice((this.info.get(u.id)!.rows!.length - hs.length) % hs.length, 1)[0] : undefined;
    const one = RULES_CLASSES[c].one, left = Math.round(clamp(hull, 0, 1) * 1000) / 1000;
    // (Ruling 240: whether she would have gone down at navy.json's toughness. The fleet's damage control lowers only the
    // limit, so one that goes out burning where she would have is a ship it kept afloat.)
    const plain = this.plainLost[side][c] < this.plainCaps[side][c];
    if (plain) this.plainLost[side][c]++;
    if (this.lostShips[side][c] < this.caps[side][c]) {
      this.lostShips[side][c]++;
      u.extra.sunk++;
      this.fleetLoss.sunk[fleet][c]++;
      if (row !== undefined) this.lostRows[fleet][c]!.push(row);
      // With men in the water (SeaSailors): under her side's call to abandon early her men leave her first, in good order
      // (form C3), unless she went too fast for any call to matter (sudden); told before her sinking, so her boats are
      // in the water first. Her men otherwise, and her oil, are told after it, in the same second, at her wreck.
      const z = this.smoke(u, 'sunk'), early = !!this.sail && this.abandons(u) && !this.sudden(u);
      if (early) this.say({ what: 'sea:abandon', unit: u.id, side, x: z.x, y: z.y, text: `The crew of a ${one} ${ofFleet(u.extra.fleet)} leave her early, in good order.`, extra: { ship: (this.wreckOf.get(u.id) ?? 0) + 1, cls: c, fleet, early: true } });
      if (early) this.sinkLater(this.inWater(u, z, true), `A ${one} ${ofFleet(u.extra.fleet)} goes down.`, { cls: c, fleet, hull: left });
      else this.say({ what: 'sea:sunk', unit: u.id, side, text: `A ${one} ${ofFleet(u.extra.fleet)} goes down.`, extra: { cls: c, fleet, hull: left } });
      if (this.sail && !early) this.oil(this.inWater(u, z, false));
      // A ship going down shakes the pieces near it by the weight it carried, measured against its side's: a destroyer
      // lost shakes less than a battleship, and a fleet of many light ships no more for the same weight lost (b438).
      const shake = (SEA.nerve.near * this.info.get(u.id)!.weight) / Math.max(1e-6, this.weight0[side]);
      for (const f of this.units) if (f.side === side && f.id !== u.id && live(f) && dist(f.x, f.y, u.x, u.y) <= SEA.nerve.nearR) f.mor = Math.max(0, f.mor - shake);
    } else if (this.sail && hull <= 0 && this.abandons(u)) {
      // Abandon early (form C3): a ship worn through past the field's limit, who would have gone out of the fight burning
      // and made for port, is lost instead, by her side's own call: her men leave her first, in good order, and she goes
      // down. The field's limit does not count her (lostShips); the campaign counts her against her fleet (abandoned). In
      // the fight she is gone exactly as a ship going out burning is, so who wins never moves; and a ship that fell out
      // of the line with hull left (fallOut) still makes for port, since she can steam.
      this.fleetLoss.abandoned[fleet][c]++;
      if (row !== undefined) this.lostRows[fleet][c]!.push(row);
      const z = this.smoke(u, 'sunk'), ship = (this.wreckOf.get(u.id) ?? 0) + 1;
      this.say({ what: 'sea:abandon', unit: u.id, side, x: z.x, y: z.y, text: `A ${one} ${ofFleet(u.extra.fleet)} is abandoned: her men leave her before she can take them with her.`, extra: { ship, cls: c, fleet, early: true } });
      this.sinkLater(this.inWater(u, z, true), `The abandoned ${one} ${ofFleet(u.extra.fleet)} goes down.`, { cls: c, fleet, hull: left, abandoned: true });
    } else {
      this.fleetLoss.burnt[fleet][c]++;
      if (plain) (this.kept[fleet] ??= none())[c]++;
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
      // Off any comb too (R6): only a piece afloat steers, so nothing else ends it.
      this.combs.delete(u.id); this.combSteered.delete(u.id); this.place.delete(u.id); this.regaining.delete(u.id);
      // And off keeping clear of her own side's tracks (own water).
      this.clearing.delete(u.id); this.clearSteered.delete(u.id); this.clearHeading.delete(u.id); this.holding.delete(u.id); this.clearBack.delete(u.id);
      // A lone ship's line has just been said; a piece of several gets one line for the last of them.
      const all = `All ${u.extra.ships0} ${RULES_CLASSES[c].many} ${ofFleet(u.extra.fleet)}`;
      this.say({ what: u.state === 'gone' ? 'gone' : 'withdrawn', unit: u.id, side, text: u.extra.ships0 === 1 ? '' : u.state === 'gone' ? `${all} are lost.` : `${all} are out of the fight.` });
    }
  }

  // ---- Men in the water (release 2's sailor rules; read only where the ticket carries SeaSailors) ------------------

  /** The piece's abandon-ship call (form C1): its own where the player changed it, else its fleet's standing call. */
  private abandons(u: SeaUnit): boolean {
    return this.abandonCall.get(u.id) ?? (this.sail?.early ?? []).includes(this.info.get(u.id)!.fleet.id);
  }

  /**
   * She went too fast for any call to matter (the design director, 17:42Z, S1; widened 19:15Z): a destroyer sunk by a
   * torpedo salvo or from the air, by torpedo or bomb (Hammann and Jarvis to torpedoes, Sims to bombs, broken within
   * minutes: historian-destroyer-sudden-cases-1122.md). The field has no magazine and books a loss at the blow, so it
   * cannot see a big ship go in under five minutes: every other loss is slow, fought for or abandoned early.
   */
  private sudden(u: SeaUnit): boolean { return u.extra.cls === 'dd' && (this.blow === 'torp' || this.blow === 'air'); }

  /**
   * One ship's crew in the water at her wreck (form A1, A2): her class's crew, less those who went down with her by how
   * she went (sailors.json down) and those killed going in (none until sourced). sea:men tells it; nothing in the fight
   * reads it. 'early': her men left her early, in good order.
   *
   * Men her piece had picked up in the fight share her fate (the design director, 18:42Z, as Hammann's men did with
   * Yorktown's): her share of them, the piece's men aboard over its ships afloat before her loss, go down by the same
   * share as her people and the rest go into the water at her wreck, a guest group for each fleet they came from, the
   * sea's clock starting again for them. sea:men's 'guests' counts those (absent where she carried none).
   */
  private inWater(u: SeaUnit, at: Zone, early: boolean): Wreck {
    const c = u.extra.cls as SeaClass, f = this.info.get(u.id)!.fleet, D = SAILORS.down;
    const ship = (this.wreckOf.get(u.id) ?? 0) + 1;
    this.wreckOf.set(u.id, ship);
    const share = this.sudden(u) ? D.sudden : early ? D.early : D.fought;
    const crew = SEA.classes[c].crew, down = Math.round(crew * share);
    const dead = Math.round(crew * SAILORS.dead.share), water = Math.max(0, crew - down - dead);
    const w: Wreck = {
      unit: u.id, ship, side: u.side, fleet: f.id, nat: f.nat, cls: c, x: at.x, y: at.y, t: this.t,
      crew, down, dead, water, early, alive: water, at: this.t, picked: { att: 0, def: 0 },
    };
    this.wrecks.push(w);
    // Her share of the men aboard: loseShip has already taken her from the piece's ships afloat.
    const on = this.aboard.get(u.id);
    let guests = 0;
    if (on) {
      const of = 1 / (u.extra.ships + 1), keep = Math.max(0, 1 - share - SAILORS.dead.share);
      for (const [g, n] of on) {
        const hers = n * of;
        on.set(g, n - hers);
        g.picked[u.side] -= hers;
        const wet = hers * keep;
        if (wet <= 0) continue;
        let h = this.wrecks.find((k) => k.guest && k.unit === u.id && k.ship === ship && k.side === g.side && k.fleet === g.fleet);
        if (!h) {
          h = { unit: u.id, ship, side: g.side, fleet: g.fleet, nat: g.nat, cls: c, x: at.x, y: at.y, t: this.t, crew: 0, down: 0, dead: 0, water: 0, early, alive: 0, at: this.t, picked: { att: 0, def: 0 }, guest: true };
          this.wrecks.push(h);
        }
        h.alive += wet;
        guests += wet;
      }
      if (u.extra.ships <= 0) this.aboard.delete(u.id);
    }
    this.say({ what: 'sea:men', unit: u.id, side: u.side, x: at.x, y: at.y, text: '', extra: { ship, fleet: f.id, men: water, dead, down, early, ...(guests > 0 ? { guests: Math.round(guests) } : {}) } });
    return w;
  }

  /**
   * A wreck's oil (form D, as the design director narrowed it at 17:39Z): every wreck leaves a slick, and it burns only
   * where a carrier went down to a torpedo salvo or to aircraft. Burning oil takes no man and delays no pick-up (the
   * record has no figure for either); its spread and burning time are looks-only knobs (sailors.json oil) for the scene.
   */
  private oil(w: Wreck, lit = w.cls === 'cv' && this.blow !== 'gun'): void {
    const r = SAILORS.oil.r[w.cls];
    const one = RULES_CLASSES[w.cls].one, fleet = this.byId.get(w.unit)!.extra.fleet;
    this.say({
      what: 'sea:oil', unit: w.unit, side: w.side, x: w.x, y: w.y, text: lit ? `Oil burns on the water where a ${one} ${ofFleet(fleet)} went down.` : '',
      extra: { ship: w.ship, r, litR: lit ? r : 0, until: lit ? Math.round((this.t + SAILORS.oil.burn) * 10) / 10 : null },
    });
  }

  /** The men still alive in a wreck's water at battle second 't' (form A3): the sea wears them down by its band, the
   *  share alive after so long in it against the share at the last reckoning (aliveAfter), with no draw. */
  private soak(w: Wreck, t: number): void {
    if (t <= w.at) return;
    const band = this.sail!.water, a0 = aliveAfter(band, (w.at - w.t) / 3600), a1 = aliveAfter(band, (t - w.t) / 3600);
    w.alive = a0 > 0 ? (w.alive * a1) / a0 : 0;
    w.at = t;
  }

  /**
   * A ship her men abandoned (form C3, or her side's early call on a ship the fight sinks) is booked lost as they leave
   * her; her sinking and her slick are told 'abandon.afloat' seconds later, or at the action's end if that comes first,
   * so the scene has her boats clear before she goes (a looks-only knob, at the sea scene's ask). Her oil burns on the
   * blow that wore her through, not on what is landing when it is told.
   */
  private sinkLater(w: Wreck, text: string, extra: Record<string, unknown>): void {
    // Her sea:sunk names her hull as her sea:abandon did ('ship'), so the scene sinks the right one of a piece's
    // abandoned ships (the sea scene's 18:53Z ask).
    this.sinkings.push({ at: this.t + SAILORS.abandon.afloat, w, lit: w.cls === 'cv' && this.blow !== 'gun', text, extra: { ...extra, ship: w.ship } });
  }

  /** Tells the abandoned ships' sinkings now due (sinkLater): all of them at the action's end. */
  private sinkDue(all: boolean): void {
    if (!this.sinkings.length) return;
    const due = this.sinkings.filter((k) => all || k.at <= this.t + 1e-9);
    this.sinkings = this.sinkings.filter((k) => !due.includes(k));
    for (const k of due) {
      this.say({ what: 'sea:sunk', unit: k.w.unit, side: k.w.side, x: k.w.x, y: k.w.y, text: k.text, extra: k.extra });
      this.oil(k.w, k.lit);
    }
  }

  /** The wreck an order to pick up survivors names, if there is one (her own crew's group, never a guest group). */
  private wreckFor(o: { unit: number; ship: number }): Wreck | undefined {
    return this.wrecks.find((w) => w.unit === o.unit && w.ship === o.ship && !w.guest);
  }

  /** The groups of men in the water at a wreck's place who will come aboard a ship of 'side' (all of them, but not an
   *  enemy's men whose navy refuses an enemy's pick-up: sailors.json refuse), each worn down by the sea to now. */
  private takers(w: Wreck, side: Side): Wreck[] {
    const men = this.wrecks.filter((g) => g.unit === w.unit && g.ship === w.ship && (g.side === side || !SAILORS.refuse.navies.includes(g.nat)));
    for (const g of men) this.soak(g, this.t);
    return men;
  }

  private aliveIn(men: Wreck[]): number {
    let n = 0;
    for (const g of men) n += g.alive;
    return n;
  }

  /**
   * Pieces under an order to pick up survivors take men aboard (form B1): once lying stopped beside the wreck, each
   * ship 'pick.wreck' men a minute, or 'pick.early' from a ship whose men left her in good order, of those still alive
   * in the water (soak). Told as she starts (none yet), at each man more, and as she leaves (told otherwise, broken off
   * or lost) or the wreck is empty; then her order lapses and she fights at will. Her own side's men are saved, the
   * enemy's are its prisoners. No draw.
   */
  private pickUp(): void {
    for (const u of this.units) {
      const o = this.orders.get(u.id), r = this.rescuing.get(u.id);
      const w = o?.kind === 'rescue' && live(u) && steady(u) ? this.wreckFor(o) : undefined;
      if (r && r.w !== w) { this.rescueEnds(u, r); this.rescuing.delete(u.id); }
      if (!w) continue;
      const men = this.takers(w, u.side);
      let now = this.rescuing.get(u.id);
      // A wreck the sea or another ship has emptied before she starts: her order lapses, with nothing to tell.
      if (!now && this.aliveIn(men) < 0.5) { this.orders.delete(u.id); continue; }
      if (u.extra.way > 0 || dist(u.x, u.y, w.x, w.y) > ARRIVE) continue;
      if (!now) {
        now = { w, got: 0, told: 0 };
        this.rescuing.set(u.id, now);
        this.say({ what: 'sea:rescue', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'stop' : 'stops'} to pick men out of the water.`, extra: { wreck: { unit: w.unit, ship: w.ship }, men: 0, done: false } });
      }
      // Of every group at the wreck that will come aboard, in proportion to the men each has alive (the last takes what is
      // left of the minute's men, so a wreck of one group is taken exactly as before there were guests).
      const all = this.aliveIn(men), k = Math.min(all, ((w.early ? SAILORS.pick.early : SAILORS.pick.wreck) * u.extra.ships * DT) / 60);
      const on = this.aboard.get(u.id) ?? new Map<Wreck, number>();
      this.aboard.set(u.id, on);
      let given = 0;
      men.forEach((g, i) => {
        const kg = i === men.length - 1 ? k - given : (k * g.alive) / all;
        given += kg;
        g.alive -= kg;
        g.picked[u.side] += kg;
        on.set(g, (on.get(g) ?? 0) + kg);
      });
      now.got += k;
      if (this.aliveIn(men) < 0.5) {
        for (const g of men) g.alive = 0;
        this.rescueEnds(u, now);
        this.rescuing.delete(u.id);
        this.orders.delete(u.id);
      } else if (Math.floor(now.got) > now.told) {
        now.told = Math.floor(now.got);
        this.say({ what: 'sea:rescue', unit: u.id, side: u.side, text: '', extra: { wreck: { unit: w.unit, ship: w.ship }, men: now.told, done: false } });
      }
    }
  }

  /** A piece stops picking up survivors: what she took aboard, told with done. */
  private rescueEnds(u: SeaUnit, r: { w: Wreck; got: number }): void {
    const n = Math.round(r.got);
    this.say({ what: 'sea:rescue', unit: u.id, side: u.side, text: `${this.told(u, true)} ${this.many(u) ? 'have' : 'has'} picked up ${n.toLocaleString('en-US')} ${n === 1 ? 'man' : 'men'}.`, extra: { wreck: { unit: r.w.unit, ship: r.w.ship }, men: n, done: true } });
  }

  /**
   * After the fight (form B2, A4; the design director's figures 17:42Z): who picks up whose men, and each side's count.
   * The side holding the water (the one that did not break off) stops for survivors if its call says so and picks up
   * every man of both sides still alive 'found.stop' hours after the fight, its own saved and the enemy's its
   * prisoners, but none of a navy whose men refuse an enemy's pick-up (sailors.json refuse): those are lost. On time
   * neither side broke off, and each stops for its own. Men nobody stops for are found 'found.leave' hours after the
   * fight, as many as the water has left alive, by whoever holds the water: its own saved, the enemy's its prisoners,
   * under the same refusal (the design director, 18:42Z). Each group's hours in the water run from its own wreck's time,
   * so men who went in early wait longest. No draw: what happened, the sea, the month and the calls decide it all.
   */
  private sailorsAfter(winner: Side, how: FieldResult['how']): { sailors: Record<Side, SeaMen>; rescue: SeaRescue[]; line: string } {
    const F = SAILORS.found, end = this.t, stops = (s: Side): 'stop' | 'leave' => this.sail!.stop?.[s] ?? 'stop';
    const holds = (s: Side): boolean => how === 'time' || s === winner;
    const blank = (s: Side): SeaMen => {
      const fl = this.ticket.place.fleets.filter((f) => f.side === s), early = fl.filter((f) => (this.sail!.early ?? []).includes(f.id)).length;
      return { crew: 0, down: 0, dead: 0, water: 0, saved: 0, prisoners: 0, lost: 0, call: { abandon: early === 0 ? 'fight' : early === fl.length ? 'early' : 'both', early: [], stop: holds(s) ? stops(s) : null } };
    };
    const out: Record<Side, SeaMen> = { att: blank('att'), def: blank('def') }, rescue: SeaRescue[] = [];
    for (const w of this.wrecks) {
      const o = out[w.side], foe = other(w.side);
      o.crew += w.crew; o.down += w.down; o.dead += w.dead; o.water += w.water;
      if (w.early && !w.guest) o.call.early.push({ unit: w.unit, ship: w.ship });
      o.saved += Math.round(w.picked[w.side]);
      o.prisoners += Math.round(w.picked[foe]);
      const by = how === 'time' ? w.side : winner;
      if (by !== w.side && SAILORS.refuse.navies.includes(w.nat)) continue;
      if (stops(by) === 'stop') {
        this.soak(w, end + F.stop * 3600);
        const men = Math.round(w.alive);
        if (by === w.side) o.saved += men; else o.prisoners += men;
        // Picked up by the fleet of the holder's ship nearest the wreck as the action ends (ties by id), for the closing shot.
        const near = this.units.filter((e) => e.side === by && live(e)).reduce<SeaUnit | null>((b, e) => (!b || dist(e.x, e.y, w.x, w.y) < dist(b.x, b.y, w.x, w.y) - 1e-6 ? e : b), null);
        const fleet = near ? this.info.get(near.id)!.fleet.id : this.ticket.place.fleets.find((f) => f.side === by)?.id ?? 0;
        if (men > 0) rescue.push({ fleet, wreck: { unit: w.unit, ship: w.ship }, men, prisoners: by === w.side ? 0 : men });
      } else {
        this.soak(w, end + F.leave * 3600);
        const men = Math.round(w.alive);
        if (by === w.side) o.saved += men; else o.prisoners += men;
      }
      w.alive = 0;
    }
    for (const s of ['att', 'def'] as Side[]) out[s].lost = out[s].crew - out[s].saved - out[s].prisoners;
    const n = (k: number): string => k.toLocaleString('en-US');
    const said = (['att', 'def'] as Side[]).filter((s) => out[s].crew > 0).map((s) => {
      const o = out[s];
      return `${this.adj(s)} sailors: ${n(o.saved)} saved${o.prisoners ? `, ${n(o.prisoners)} taken prisoner` : ''}, ${n(o.lost)} lost.`;
    });
    return { sailors: out, rescue, line: said.join(' ') };
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
      this.blow = 'air';
      const got = this.splash(at, landed, s.side);
      this.blow = 'gun';
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
   *  guns first meet, up to torp.bank seconds' worth: its salvo is weight its guns could not fire, and no more. A ship of
   *  any class banks what her guns could not bring to bear while she combs a salvo, and only that (R6; the design
   *  director, 11:58Z), and fires it with her first blow after the comb (fire). */
  private store(u: SeaUnit, unfired?: number): void {
    if ((u.extra.cls !== 'dd' && (unfired === undefined || !this.combs.has(u.id))) || this.contactAt === null || !steady(u) || this.orders.get(u.id)?.kind === 'leave') return;
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
          const aim = foe ? this.clearAim(u, foe) : null;
          if (foe && aim) this.launch(u, foe, null, aim);
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
      // A carrier keeping her station (R3) holds back to her gun ships' way and could make her cruising speed at once,
      // so a ship comes up on her only going faster than that (or than she is going, if more).
      if (f.extra.cls === 'cv') {
        const pace = this.onStation.has(f.id) ? Math.max(f.extra.way, SEA.classes.cv.speed * SEA.speeds.cruise) : f.extra.way;
        for (const e of theirs) if (e.extra.way > pace && aheadOfBeam(e, f) && closing(e, f)) out.add(e);
        continue;
      }
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
      // A ship of her own side in the tracks' way (release 2's own water): she holds her salvo and runs on in at her
      // target, launching the first second the way is clear (clearAim).
      const aim = this.clearAim(u, foe);
      if (!aim) return { want: bearing(u.x, u.y, foe.x, foe.y), speed: flank };
      this.launch(u, foe, run, aim);
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
    return o.kind === 'move' || o.kind === 'hold' || o.kind === 'leave' || o.kind === 'rescue' || (o.kind === 'attack' && !o.fleet && o.target !== run.target);
  }

  /** A run ends: after its launch it may run again torp.again seconds after it; broken off before, as soon as it is fit. */
  private endRun(u: SeaUnit, run: Run, done: boolean): void {
    this.runs.delete(u.id);
    this.holding.delete(u.id);
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
  private launch(u: SeaUnit, foe: SeaUnit, run: Run | null, aim?: [number, number]): void {
    const T = SEA.torp, w = this.bank[u.id];
    // How full the bank was, for the 3D game to draw as many tracks (the ship plan's L3): none at an empty bank.
    const full = SEA.fire.rate * this.output(u) * this.day(u.side) * T.bank;
    const share = full > 0 ? Math.round(clamp(w / full, 0, 1) * 100) / 100 : 0;
    this.bank[u.id] = 0;
    // The part of the salvo its guns have not yet earned goes with it, owed by its side when it lands (loaded, advance).
    const drawn = Math.min(this.loaded[u.id], w);
    this.loaded[u.id] = 0;
    const [x, y] = aim ?? this.aimOf(foe);
    this.torps.push({ at: this.t + T.travel, side: u.side, target: foe.id, x, y, weight: w, from: u.id, drawn, dir: bearing(u.x, u.y, x, y), t0: this.t, x0: u.x, y0: u.y });
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
      this.blow = 'torp';
      const got = this.take(at, s.weight);
      this.blow = 'gun';
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
      // What the guns denied of the aircraft and never paid for with their fire lands as the aircraft's weight.
      this.blow = 'air';
      const got = this.take(to, o);
      this.blow = 'gun';
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
      this.blow = 'air';
      const got = this.splash(at, c, cv.side);
      this.blow = 'gun';
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
      this.blow = 'air';
      const got = this.splash(at, landed, side);
      this.blow = 'gun';
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
    const extra: SeaLosses = { sunk: this.fleetLoss.sunk, burnt: this.fleetLoss.burnt, hurt, ...(Object.keys(this.kept).length ? { kept: this.kept } : {}), ...(toDice ? { toDice } : {}) };
    // Ships by class (c3): the rows of the ships lost, only where the ticket gave rows.
    if (this.hullsLeft.size) extra.rows = this.lostRows;
    // Men in the water (SeaSailors): the ships each fleet's own call abandoned, and who was picked up after the fight,
    // with a line for each side's men on the after-action card (form 129(d)). Nothing here moves who won or by how much.
    if (this.sail) {
      // What the action's end leaves told: the abandoned ships' sinkings, and the pick-ups under way (done).
      this.sinkDue(true);
      for (const [id, r] of this.rescuing) this.rescueEnds(this.byId.get(id)!, r);
      this.rescuing.clear();
      const after = this.sailorsAfter(winner, how);
      Object.assign(extra, { abandoned: this.fleetLoss.abandoned, sailors: after.sailors, rescue: after.rescue });
      const ab = (side: Side): SeaShips => { const s = none(); for (const f of this.ticket.place.fleets) if (f.side === side) for (const c of ['bb', 'cv', 'ca', 'dd'] as SeaClass[]) s[c] += this.fleetLoss.abandoned[f.id][c]; return s; };
      const left = (['att', 'def'] as Side[]).filter((sd) => shipsIn(ab(sd)) > 0).map((sd) => `${this.adj(sd)} ships abandoned: ${shipWords(ab(sd))}.`);
      lines.push(...left, ...(after.line ? [after.line] : []));
    }
    // The B4 mark: a side with two destroyer pieces outside its screen, against an enemy with a big ship, made a run.
    for (const side of ['att', 'def'] as Side[]) {
      const free = this.units.filter((u) => u.side === side && u.extra.cls === 'dd' && !this.screen.has(u.id)).length;
      if (free < 2 || !this.units.some((e) => e.side !== side && e.extra.cls !== 'dd')) continue;
      this.mark(`${side}:runEligible`, 1);
      if (this.launched[side] > 0) this.mark(`${side}:runBattles`, 1);
    }
    // Bank left: what idle guns banked and never launched; a loaded salvo never fired is no weight moved (loaded). What a
    // big ship banked while combing and had not fired when the action ended is told apart (combLeft).
    for (const u of this.units) {
      if (u.extra.cls !== 'dd') { if (this.bank[u.id] > 0) this.mark(`${u.side}:combLeft`, this.bank[u.id]); continue; }
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
   * 'away', and 'runUntil', the second it turns back. On a ship combing a salvo's tracks (R6): 'combFrom', the piece
   * that launched it, and 'combUntil', the second it lands; absent when she combs none. 'smoke': a destroyer or cruiser piece laying a smoke screen, as one
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
      const cb = on ? this.combs.get(u.id) : undefined;
      if (cb) { x.combFrom = cb.from; x.combUntil = Math.round(cb.at * 10) / 10; } else { delete x.combFrom; delete x.combUntil; }
      // Release 2's own water (only with it on), for the words on the player's own pieces (the UI director, 13:25Z):
      // 'heldFor', the piece of her own side in the way of the salvo she holds (looked for each second at the most: a
      // screen's launch), and 'heldForName', it as the log names a piece ("a cruiser of the Home Fleet": the field names no
      // ship); 'clearFrom' and 'clearUntil', the launcher of the own salvo she keeps clear of and the second it lands, and
      // 'clearFromName'; and 'ordered', with either, when she is under an order of the player's own, which stands.
      if (this.ownWater) {
        const hd = on ? this.holding.get(u.id) : undefined, hb = hd && hd.t >= this.t - 1 - 1e-9 ? this.byId.get(hd.by) : undefined;
        if (hd && hb) { x.heldFor = hd.by; x.heldForName = this.told(hb); } else { delete x.heldFor; delete x.heldForName; }
        const cl = on && this.clearSteered.has(u.id) ? this.clearing.get(u.id) : undefined, cb2 = cl ? this.byId.get(cl.from) : undefined;
        if (cl && cb2) { x.clearFrom = cl.from; x.clearUntil = Math.round(cl.at * 10) / 10; x.clearFromName = this.told(cb2); } else { delete x.clearFrom; delete x.clearUntil; delete x.clearFromName; }
        if ((x.heldFor !== undefined || x.clearFrom !== undefined) && this.orders.has(u.id)) x.ordered = true; else delete x.ordered;
      }
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
 * +y, as a ship's face; + to starboard). A hash, not a draw: it leans the smoke, sets the carriers' deck wind (their turn
 * into it to launch and recover is the 3D game's to show: R3, the design director, 14:36Z) and the sea state's run in the
 * 3D game, and nothing in the rules reads it.
 */
export function seaWind(seed: number): number { return Math.round(mix((seed ^ 0x9e3779b9) >>> 0) * TAU * 1000) / 1000; }
/** The wind's strength for a battle, in metres a second, 4 to 11 (a light breeze to a strong one): a hash of the seed as
 *  seaWind is, for the 3D game's sea state and smoke. Nothing in the rules reads it. */
export function seaWindSpeed(seed: number): number { return Math.round((4 + 7 * mix((seed ^ 0x51ed270b) >>> 0)) * 10) / 10; }
/** The order of a piece that has none: fight at will. */
const FREE: SeaOrder = { kind: 'free' };
