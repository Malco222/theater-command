// The air battle's own pieces, on the shared battle core (src/battle/core).
//
// An air group is a block: a count of aircraft, a morale meter, fuel and a height. The 3D side draws its own
// aircraft models in formation for it and acts out what the counts decide: a fight is a swirl of its models round
// the fight's centre, a loss is one model falling (see 'air:lost' events). Nothing here flies a single aircraft.

import type { AirMission, BomberRow, BomberSeats, BomberShot, ShipClass } from '../../campaign/types';
import type { BombLaw } from '../../campaign/airfought';
import type { Side, UnitBase } from '../core/types';
import type { Band } from './book';
import type { AirGround } from './ground';
import type { Form, Pass, Role, Style, When } from './doctrine';
import type { Loadout } from './guns';
import type { ShipPlace, Strike } from './ships';
import type { MarkPlace } from './troops';
import type { AirCloud } from './sky';
import type { Defence, DefRole } from './styles';

export type AirKind = 'ftr' | 'bmr';

/** What an air group is doing. Orders set it; the computer sets it for its own side. */
export type AirJob =
  /** Bombers: fly to the target, hold the run, drop, turn for home. */
  | { do: 'route' }
  /**
   * Fighters: stay with that bomber group: close (glued to it), top cover (a band above and behind it, diving on what
   * comes at it) or loose (sweeping ahead of it). The computer deals the roles by its nation's doctrine (doctrine.ts
   * escortRoles; ruling 237.2); the player's ESCORT order gives close or loose.
   */
  | { do: 'escort'; on: number; close: boolean; top?: boolean }
  /** Fighters: go for that enemy group. */
  | { do: 'intercept'; on: number }
  /** Fighters: go for the nearest enemy fighters ('free hunt'). */
  | { do: 'hunt' }
  /** Fighters: hold the sky over a point, taking on anything that comes near. */
  | { do: 'patrol'; x: number; y: number }
  /**
   * Fighters, in a raid on troops (release 2): down to the low band over the target, passes on the troops, then home.
   * Each pass a group completes counts its part of the group on target, and pays its part of the ground fire (troops.ts).
   */
  | { do: 'strafe' }
  /** On the ground, waiting to be scrambled (a defender with no radar warning). */
  | { do: 'wait' }
  /** Back to its own edge or airfield, and off the board. */
  | { do: 'home' };

export interface AirUnit extends UnitBase {
  /** Fighters or bombers: an air group in the air war flies one kind. */
  air: AirKind;
  /** Aircraft now, and at the start. str = n / n0. */
  n: number;
  n0: number;
  /** Height band now, and the band it is climbing or diving to; alt in metres for the 3D side. */
  band: Band;
  to: Band;
  alt: number;
  /** Air seconds of progress toward the next band. */
  climbT: number;
  /** Air seconds of fuel left over the board, and at the start. */
  fuel: number;
  fuel0: number;
  job: AirJob;
  /** Fighting quality per aircraft: the campaign's worth of that side's groups (pilots, leader, readiness, equipment). */
  q: number;
  /** Air seconds left of a bounce: it dived on its enemy from above and strikes first. */
  bounce: number;
  /** Bombers: air seconds held on the run, whether it has dropped, how many bombers dropped, and whether it jettisoned. */
  run: number;
  dropped: number;
  jettisoned: boolean;
  /** Bombers: how much of the guns' run round the target it has flown through, 0 to 1 (more at low height). */
  shot: number;
  /** The aircraft this group was owed this tick (a share of one falls by chance; AirSim.fall). */
  owed: number;
  /** Fighter pilots of its own lost aircraft who came down and will fly again (the rules' pilots back). */
  back: number;
  /** Ids of the enemy groups it is in contact with this tick. */
  contacts: number[];
  /**
   * What fight is left in it, 1 to 0: a group in contact spends it (fuel, nerve, what it carries), and its blows fall
   * off as it does. A fight therefore burns itself out, and what it costs in all is the campaign's own sum however
   * long the groups stay locked together. A fighter group that is spent breaks off for home.
   */
  vig: number;
  /** What it can still do to the enemy, in aircraft: to fighters and to bombers (see AirSim.arm), and at the start. */
  kit: { ftr: number; bmr: number };
  kit0: number;
  /** The fight's luck, drawn when it first makes contact, as the dice draw it for a raid. */
  luck: number;
  /** Bombers: its place in the stream, metres east of the stream's line to the target. */
  lane: number;
  /** Models shown for each one: SKY.look. */
  per: number;
  /** Where its airfield is on the board (defenders), or where it came on (attackers). */
  homeX: number;
  homeY: number;
  /**
   * What the 3D game acts out (presentation only: none of it moves a count, ruling 237): the aircraft it draws for this
   * group (doctrine.ts fighterOf, bomberOf) and its side's way of flying.
   */
  model: string;
  form: Form | 'box';
  style: Style | null;
  /**
   * Fighters: each contact's fire this tick, in aircraft owed per air second, and the pass it flies at a bomber group.
   * A defending group's fire on a bomber group says where it is in the beat of its passes (SKY.flak.pass, shown only):
   * 'go' while it is going in, 'wait' while it stands off to form up again, each the air seconds of it left.
   */
  fire: { on: number; rate: number; pass?: Pass; go?: number; wait?: number }[];
  /** Bombers: aircraft of the box that have fallen out of formation (the design's 1.6), oldest first; those that limped home. */
  strag: Straggler[];
  away: number;
  /** A carrier group's bombers: how this box attacks ships (ships.ts, ruling 238(2)). */
  strike?: Strike;
  /** A computer defence's fighter group: sent at the raid's escort or at its bombers (styles.ts; the tactics panel's 5c). */
  def?: DefRole;
  /** On its staff's climb above a raid radar warned it of (the tactics panel's 5d): its height is drawn, never priced (b418). */
  rise?: boolean;
}

/**
 * A bomber that has lost an engine, slowed and fallen out of its box (Brian, 19:38Z: "it goes slower and falls out of
 * formation which makes it an easier target"). It is one of its box's count, not a unit: the box's next loss is the
 * straggler's, so the raid loses no more than the card says (ruling 237.1).
 */
export interface Straggler {
  id: number;
  /**
   * The dead engines (engine.1 to engine.N on the model), and whether the airscrew is feathered or windmills. A
   * single-engined bomber's is engine 1, hit and running rough, its airscrew still turning ('windmill').
   */
  eng: number[];
  prop: 'feather' | 'windmill';
  /** What hit it: fighters or the guns, and which group ('flak' for the guns). */
  why: 'ftr' | 'flak';
  by: number | 'flak';
  /** Air seconds since it fell out; metres it is behind and below its box; how fast it drops back and sinks. */
  age: number;
  back: number;
  down: number;
  rate: number;
  sink: number;
  /** The escort group one of whose pairs weaves over it, if any. */
  cover: number | null;
  /** Shown only: air seconds its engine fire still burns (Infinity: it never goes out); whether it will regain its box after the drop, and is closing on it. */
  fire?: number;
  join?: boolean;
}

/** What the campaign hands the air battle as the place: the raid, its target, and what the raid card forecast. */
export interface AirPlace {
  /** The campaign's raid kind and mission ('bomb', 'night', 'superiority' in the first slice). */
  kind: string;
  mission: AirMission;
  /** The target region and its name. */
  target: number;
  name: string;
  night: boolean;
  /** Who flies the raid: the raid card's nation. */
  nat: string;
  /** The defender has radar warning: his fighters are up and waiting when the raid comes on, not caught on the ground. */
  radar: boolean;
  /**
   * The air war's intensity for this mission, the clash between fighters, and the bomber-loss law as the raid card
   * reads it (L.bombers, L.power; the largest share of its bombers a raid can lose; how many times over the escort
   * counts in the fighting weight, and the bombers' own guns, their defence of the month: ruling 227). escort and guns
   * may be absent on a ticket made before ruling 227 reached the battle: 1 and bomberDefence then. (Not the defence's
   * style, which is 'defence' on the place.)
   */
  I: number;
  clash: number;
  law: { bombers: number; power: number; most: number; escort?: number; guns?: number };
  /** C.fighters, C.escortShare, C.bomberDefence: the rules' fighter numbers. */
  fighters: number;
  escortShare: number;
  bomberDefence: number;
  /** The share of bombers the guns bring down over the whole raid (flak, built guns), as the raid card shows it. */
  gunLoss: number;
  /**
   * The guns round the target as the raid card counts them (campaign/air.ts flakGuns): light and heavy. For the scene,
   * which can stand that many gun sites round the target and thicken its bursts by them; nothing in the battle reads it.
   */
  flakGuns: { light: number; heavy: number };
  /** Average fighting worth of an aircraft on each side, as assessAir weighs it. */
  worth: Record<Side, number>;
  /**
   * What the raid card forecast: aircraft each side should lose, and bombers that should get through. foeGuns: of the
   * defenders' losses ('foe'), those the bombers' own gunners bring down (ruling 227, forecastAir's foeGuns); the rest
   * fall to the escort. Absent on a ticket made before ruling 227 reached the battle: none. fire: in a raid on troops,
   * of the raid's own fighters lost ('ftr'), those the card expects the ground fire to bring down as they strafe; the
   * rest fall to the defending fighters. Absent for any other raid: none.
   */
  forecast: { foe: number; foeGuns?: number; ftr: number; bmr: number; through: number; bomberLoss: number; fire?: number };
  /** The raid card's chance of success (forecastAir's 'success'), for the after card's reckoning line. */
  chance: number;
  /**
   * How forecastAir weighs that chance, so the staff can weigh it again from where a battle stands (AirSim.staffChance):
   * the steepness k, what the bombers should achieve beyond aircraft (value), and the chances of a cut railway and a
   * wrecked factory (rail, ruin), each worth 300 aircraft.
   */
  reckon: { k: number; value: number; rail: number; ruin: number };
  /**
   * How the campaign weighs the bombs of a raid on a province, from the bombers that drop (campaign/air.ts bombLaw): the
   * output stopped, the railway cut, a factory wrecked. The field decides the railway and the factory as the bombs fall,
   * by this law (b397). Absent for a fight for the sky.
   */
  bombs?: BombLaw | null;
  /** When it is fought, and the aircraft each nation's fighters and bombers are (doctrine.ts): absent, September 1939 and the plain models. */
  when?: When;
  models?: Record<string, { ftr: string; bmr: string }>;
  /** Each nation's guns that month, on the aircraft drawn for it (guns.ts): shown, never counted. */
  guns?: Record<string, { ftr: Loadout; bmr: Loadout }>;
  /**
   * Where each campaign group comes on: board position, height, fuel, and whether it waits on the ground. over: a
   * fleet's own fighters standing over it as a strike comes on, warned by its lookouts (the tactics panel's S1).
   */
  starts: Record<number, { x: number; y: number; band: Band; fuel: number; wait: boolean; over?: boolean }>;
  /** Where the raiders fly from: their own airfields, carriers at sea (the Home Fleet's aircraft), or both. */
  from?: 'land' | 'sea' | 'both';
  /** The country round the target, in board metres (ground.ts): absent, the page draws a plain country. */
  ground?: AirGround;
  /** The sky that day (sky.ts): a cloud deck under the bombers or scattered cumulus, and one cloud base and top drawn from the table's deck ranges. Shown, never counted. */
  cloud?: AirCloud;
  /** Fighter pilots who come down over this side's ground fly again at this rate (AIR.pilots.home, away). */
  saved: Record<Side, number>;
  /** A raid on a harbour or a strike on a fleet: the campaign's law for ships, the ships as pieces, the stage, the carrier groups by kind (ships.ts). */
  ships?: ShipPlace | null;
  /** A raid on troops (release 2): the army under the raid as pieces on the board, and the campaign's law for it (troops.ts). */
  marks?: MarkPlace | null;
  /**
   * How the computer flies the defence (styles.ts; the tactics panel's item 5, ruling 239): the share of its weight
   * sent at the escort, and how far its staff climbs above a raid radar warned it of. Absent for a human defender,
   * whose staff's opening plan is the plain one.
   */
  defence?: Defence;
  /** At stops 4 and 5, a computer raid's escort roles by nation, in place of its doctrine's order (styles.ts rolesOf). */
  roles?: Record<string, Role[]>;
  /**
   * The raid's bombers one by one, by campaign group (campaign/bombers.ts seatsOf): what each is called, its marking,
   * the missions and kills painted on it, the old hands first. The battle says which of them each loss is ('air:lost'
   * extra.bomber) and hands back the ones that fell, so the campaign strikes off the bombers the player saw go down.
   * Shown, never counted: with it or without it the same aircraft fall at the same moments. Absent for a raid with no
   * bombers kept by name (a carrier's, or a battle not made from a campaign).
   */
  bombers?: Record<number, BomberSeats>;
}

/**
 * One bomber as an event names it: its seat in its group's list (AirPlace.bombers[corps].seats), and what is painted
 * on it. 'air:lost' extra.bomber is the bomber that falls; 'air:straggle' and 'air:named' extra.bomber the one that
 * falls out or is named; 'air:lost' extra.gunner, on a fighter's loss, the bomber whose gunners got it, with its box.
 */
export interface BomberTold { seat: number; name: string; mark: string; art: number; missions: number; kills: number }

/**
 * What an 'air:hit' event's extra carries beside the look of the hit (fates.ts hitOf): shown, never counted (the
 * design director, pilot-ai 25 November: R2). 'by' is the enemy group whose fire the rules held owed for that hit at
 * that moment, by the id the view gives that group, or 'flak' for the guns (and for the ground fire on a raid on
 * troops); where several owed on the group that tick, one of them by its share of what was owed, never from the
 * rules' main stream. 'gun' is the gun's name as guns.ts gives it ('Browning .303'), only where every gun that group
 * carries is of that one name; left out otherwise, and always for the guns on the ground.
 */
export interface AirHitWho { by: number | 'flak'; gun?: string }

/**
 * The leader's choice in a fighter group's view extra (R3; the design director, pilot-ai 25 November 2026: "R2 ('by'
 * and 'gun' on air:hit) and R3 (the leader's choice in the view): yes, shown only"), beside 'def', 'pass' and
 * 'quarry'. Each field is read off what the rules already hold that tick (the group's job and quarry, its contacts,
 * the enemy each group is making for, the escort's chase): no choice is made for it, no throw drawn, and nothing the
 * rules read is kept for it. Shown, never counted.
 *
 * - 'aim': what its leader has set it on. 'bombers': its quarry is an enemy bomber box; 'escort': its quarry is an
 *   enemy fighter group (the escort, or for a fighter sweep the enemy's fighters). Given to a group sent at an enemy
 *   group (an interceptor), and to a hunter or a patrol while it is making for one. 'wait' and 'turn' are R7's (the
 *   interceptor leader's decisions, not yet built) and never occur yet. Absent for an escort (its choice is 'answer'),
 *   a group strafing troops, on the ground, homeward or broken, and one making for nobody.
 * - 'threat': the enemy fighter group coming down on it (the nearest of those making for it this tick), by the id the
 *   view gives that group, and whether it is a bounce: it is above, so its first contact is a dive from above (or it
 *   has just dived on it and is in contact). Absent while none is making for it.
 * - 'answer', an escort only: while it has turned on an enemy fighter group, that group ('on') and the box of its raid
 *   that group is making for or is in contact with ('box'), or its own box ('with') when that group is on no box.
 *   Absent while it keeps its place by its bombers.
 */
export interface AirAim {
  aim?: 'bombers' | 'escort' | 'wait' | 'turn';
  threat?: { by: number; bounce: boolean };
  answer?: { box: number; on: number };
}

/**
 * What the 3D game reads to lay out and ease a group's flight (the 3D air thread's shown-only asks after the owner's
 * Epirus battle, 26 November 2026: "no count to move"), in its view extra beside AirAim. Each key is absent unless it
 * is true that tick. Built in sim.ts shownMore() at the end of each tick, after every group has moved and settled, from
 * what the rules hold then; the few things it needs from the tick before (each group's point and height, the second a
 * group left the ground, a dive's start) are kept in AirSim.shownKept, which no rule reads. Shown, never counted: no
 * throw is drawn for it and the battle never reads it back.
 *
 * Units: metres in the battle's own frame (the frame of the view's x and y: x east, y north, the raid coming on from
 * y = 0), heights in metres as the bridge's 'alt' gives them, and seconds on the view's clock (the view's t: battle
 * seconds, ten views a second; one battle second is SKY.scale air seconds).
 *
 * - 'fight', on a fighter group in a fight: its knot, the fighter groups of both sides joined to it by contact
 *   (a contact both groups hold), one fighter-to-fighter contact leading to the next. A bomber box is never walked
 *   through, so a box in contact with two knots links neither to the other: they are two fights. A fighter group in
 *   contact with an enemy box and with no enemy fighter is a knot of its own (a fight of one); one in contact with
 *   nothing (no contact both hold) is in no fight. A bomber box carries no 'fight' (its own 'hold', the fighter group
 *   its guns hold fire for, is shown()'s and stays as it is). 'id' is the lowest group id in the knot, so it stays
 *   while that group is in it: when two knots join, the joined fight takes the lower of the two ids; when one splits,
 *   the part with the lowest group keeps the id and the other part takes its own lowest. 'x', 'y' the centre, the
 *   mean of its groups' points (fighters only). 'vx', 'vy' the centre's speed in metres a battle second: how far the
 *   centre of these same groups moved since the tick before, over the tick (0.1 s). On a fight's first tick, and on
 *   any tick a group joins or leaves it, that is still the move of the groups in it now, from where they were a tick
 *   before, so a group joining makes no jump in vx, vy (the centre x, y itself does jump then). 'groups' the ids of
 *   the knot's fighter groups, both sides, in rising order: the same list on every group of the fight. 'boxes' the
 *   ids, in rising order, of every enemy bomber box in contact (a contact both hold) with any group of the knot, and
 *   'box' the lowest of them: the box the knot is working on. Both absent when the knot touches no box.
 * - 'rise', on a group scrambled late, from the tick it is scrambled until the tick its climb ends: 'x', 'y' its
 *   field (where it waited on the ground), 'at' the view second it was scrambled, 'from' the ground (0 m: the bridge's
 *   'alt' for a group on the ground), 'to' the height of the band it is climbing to, and 'over' the seconds from 'at'
 *   to the first view at that height, by the rules' own climb (SKY.bands.climb air seconds a band). The rules put it
 *   on the low band at once (1,800 m, in the scramble's own view) and climb it one band from there; 'over' is that
 *   climb's length on the view's clock, so a take-off at 'at' and a climb ending at at + over meets the rules' group
 *   at its height when they have it there.
 * - 'hold', on a fighter group whose job is a patrol, flying, that is making for no enemy this tick: the patrol's
 *   point, where it is or is bound for (its job's x, y). Absent the tick it makes for an enemy or is given a quarry.
 *   A scrambled group patrols the target while it climbs, so it may carry 'rise' and 'hold' at once.
 * - 'bounce', on a group that has dived from above on an enemy (top cover coming down, or the dive that ends a
 *   staff's climb): 'from' its height the view before the dive, 'to' the band it dived to (one band lower: groups
 *   more than a band apart never meet), 'at' the view second of the dive. The rules change its height at once, in
 *   that view (sim.ts touch()): they hold no time for the drop itself, so 'over' is 0, always; how to spread the
 *   drop is the 3D's. 'edge' and 'left' are the rules' own clock for the dive (u.bounce): SKY.edge.bounceFor air
 *   seconds on the view's clock, and what is left of it. For top cover that clock is the time it strikes harder,
 *   "diving from a higher band". For the dive that ends a staff's climb the clock runs the same, but the rules
 *   price that dive level for as long as the group is still on its staff's climb (drawn, never priced: place_()),
 *   so there 'edge' and 'left' say how long the dive is told, not an edge in the fight. Present from the dive
 *   while the clock runs; a dive that starts no clock (on fighters down on their passes on the troops) shows only
 *   in the view of the dive, with 'edge' and 'left' 0.
 */
export interface AirShown {
  fight?: { id: number; x: number; y: number; vx: number; vy: number; groups: number[]; box?: number; boxes?: number[] };
  rise?: { x: number; y: number; at: number; from: number; to: number; over: number };
  hold?: { x: number; y: number };
  bounce?: { from: number; to: number; at: number; over: 0; edge: number; left: number };
}

/** The exact result the rules apply (FieldResult.extra.air): each campaign group's losses, and the bombs dropped. */
export interface AirOutcome {
  /** Each campaign group: what it lost, its downed fighter pilots who fly again, and the aircraft it sent up. */
  groups: {
    id: number; side: Side; lostFtr: number; lostBmr: number; back: number; flewFtr: number; flewBmr: number;
    /** A group whose bombers the battle knew by name (AirPlace.bombers): the rows of those that fell, in the order they fell, and what each one's gunners shot down. */
    fell?: BomberRow[];
    shot?: BomberShot[];
    /** A bomber group's gunners' kills: the enemy fighters whose loss the battle booked to its boxes (ruling 227). */
    guns?: number;
  }[];
  /** The raid card's chance of success, for the after card's reckoning line. */
  chance: number;
  /** Bombers that dropped on the target, and bombers that jettisoned and turned back. */
  dropped: number;
  jettisoned: number;
  /**
   * Bombers lost on the field before they reached the release (or without ever reaching it), and those lost after it:
   * a bomber shot down after bombing still bombed (the design director, b384(a)).
   */
  lostBefore: number;
  lostAfter: number;
  /** A side that broke off everything (BREAK OFF ALL): it gave the battle up, whatever the count says. */
  gaveUp?: Side;
  /** Handed to the staff (sim.settleNow): each pool's share of the raid's cost still to come, and the bombers that had turned back. */
  staff?: { rest: { foe: number; ftr: number; bmr: number }; away: number; chance?: number; fired?: boolean };
  /** The field's throws for the railway and a factory, and whether the bombs had cut the one and wrecked the other (b397). */
  rolls?: { rail?: number; ruin?: number };
  railCut?: boolean;
  ruined?: boolean;
  /** A strike on ships: the ships of each class sunk in each fleet, by fleet id (ShipBook.sunk), every class it has. */
  sunk?: Record<number, Partial<Record<ShipClass, number>>>;
  /** And, where damage control is in play (ruling 240), those its repair parties kept afloat (ShipBook.kept). */
  kept?: Record<number, Partial<Record<ShipClass, number>>>;
  /**
   * A raid on troops (release 2): the fighters that strafed, each completed pass counting its part of its group, and
   * the organisation cut the field earned by the campaign's own law (the campaign books it from 'dropped' and 'strafed').
   */
  strafed?: number;
  troops?: { cut: number };
  /**
   * With the strafers' rule on (campaign/strafers.ts): the campaign groups that went down on the troops and paid the
   * ground fire, and the ground fire charged them (aircraft owed, before whole aircraft fall). Absent otherwise.
   */
  strafers?: number[];
  strafeFire?: number;
}

/**
 * How a lost aircraft goes (fates.ts; Brian, 20 November): it blows up; goes down as one sheet of flame; with an engine
 * on fire; with a wing shot or burnt off; with its tail shot away; with its pilot hit, nosing over and going straight
 * in; in a spin; gliding down on dead engines to ditch or crash-land; coming apart; struck by another falling aircraft
 * or its wreckage; or in a collision with the aircraft it was fighting.
 */
export type FallHow = 'explode' | 'flamer' | 'burn' | 'wing' | 'tail' | 'pilot' | 'spin' | 'glide' | 'breakup' | 'struck' | 'collide';

/** One loss, as the 3D side plays it. */
export interface Fall {
  /** The old three looks, kept for anything that reads only these (fates.ts kindOf). */
  kind: 'explode' | 'burn' | 'spin';
  /** Parachutes seen. */
  chutes: number;
  /** Over whose ground it came down. */
  over: 'own' | 'enemy' | 'sea';
  /** A fighter pilot picked up who flies again. */
  back: boolean;
  /** A pilot or crew who bailed out over enemy ground: taken prisoner (shown only; the rules count no bomber crews). */
  captured: number;
  /**
   * The men the look draws from (release 2, 'bailouts' on; absent with it off): a bomber's crew by its kit; 1 for every
   * fighter but the two-seat Fulmar, 2 until the historian confirms (hole H-FULMAR). `chutes + out` never more.
   */
  crew?: number;
  // What follows is drawn in a shown battle only (fates.ts): absent, play 'kind'.
  /** The loss's own number, which a collision or a strike names ('with'). */
  fid?: number;
  /**
   * How it goes, and where it was hit: an engine, a wing or the tail ('side' port or starboard), the cockpit, the tanks,
   * the bomb bay, the fuselage. 'engineAt': which engine, as a place across the aircraft from port 0 to starboard 1
   * (engine k of N is the one whose span holds it, k = floor(engineAt * N) + 1); a straggler's is its dead engine, 'eng'.
   */
  how?: FallHow;
  hit?: 'engine' | 'wing' | 'tail' | 'cockpit' | 'tank' | 'bay' | 'fuselage';
  side?: 'port' | 'starboard';
  engineAt?: number;
  /** Air seconds: until the fire shows; until a burning one blows apart on the way down; until it strikes the sea or the ground. */
  fire?: number;
  boom?: number;
  dur?: number;
  /** Where it ends: blown apart in the air, in the sea, on land; 'ditch' a glide onto the sea under control. */
  end?: 'air' | 'sea' | 'land';
  ditch?: boolean;
  /**
   * The other loss in a collision or a strike, and what struck it: wreckage, a falling aircraft, or the two met ('ram');
   * 'meant': a fighter that rammed on purpose and the aircraft it rammed (book.ts SKY.fate.ram.meant).
   */
  with?: number;
  cause?: 'debris' | 'falling' | 'ram';
  meant?: true;
  /** Crew seen to climb out after a ditching or a crash-landing; air seconds after the hit each parachute opens. */
  out?: number;
  chuteAt?: number[];
  /** Which way it rolls, and which of the 3D side's looks for this way down (never the last one shown for it). */
  roll?: 1 | -1;
  look?: number;
}
