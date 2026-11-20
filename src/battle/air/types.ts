// The air battle's own pieces, on the shared battle core (src/battle/core).
//
// An air group is a block: a count of aircraft, a morale meter, fuel and a height. The 3D side draws its own
// aircraft models in formation for it and acts out what the counts decide: a fight is a swirl of its models round
// the fight's centre, a loss is one model falling (see 'air:lost' events). Nothing here flies a single aircraft.

import type { AirMission } from '../../campaign/types';
import type { BombLaw } from '../../campaign/airfought';
import type { Side, UnitBase } from '../core/types';
import type { Band } from './book';
import type { AirGround } from './ground';
import type { Form, Pass, Role, Style, When } from './doctrine';
import type { Loadout } from './guns';
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
  /** Fighters: each contact's fire this tick, in aircraft owed per air second, and the pass it flies at a bomber group. */
  fire: { on: number; rate: number; pass?: Pass }[];
  /** Bombers: aircraft of the box that have fallen out of formation (the design's 1.6), oldest first; those that limped home. */
  strag: Straggler[];
  away: number;
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
  /** The air war's intensity for this mission, the clash between fighters, and the bomber-loss law (L.bombers, L.power). */
  I: number;
  clash: number;
  law: { bombers: number; power: number; most: number };
  /** C.fighters, C.escortShare, C.bomberDefence: the rules' fighter numbers. */
  fighters: number;
  escortShare: number;
  bomberDefence: number;
  /** The share of bombers the guns bring down over the whole raid (flak, built guns), as the raid card shows it. */
  gunLoss: number;
  /** Average fighting worth of an aircraft on each side, as assessAir weighs it. */
  worth: Record<Side, number>;
  /** What the raid card forecast: aircraft each side should lose, and bombers that should get through. */
  forecast: { foe: number; ftr: number; bmr: number; through: number; bomberLoss: number };
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
  /** Where each campaign group comes on: board position, height, fuel, and whether it waits on the ground. */
  starts: Record<number, { x: number; y: number; band: Band; fuel: number; wait: boolean }>;
  /** Where the raiders fly from: their own airfields, carriers at sea (the Home Fleet's aircraft), or both. */
  from?: 'land' | 'sea' | 'both';
  /** The country round the target, in board metres (ground.ts): absent, the page draws a plain country. */
  ground?: AirGround;
  /** The sky that day (sky.ts): a cloud deck under the bombers or scattered cumulus, and one cloud base and top drawn from the table's deck ranges. Shown, never counted. */
  cloud?: AirCloud;
  /** Fighter pilots who come down over this side's ground fly again at this rate (AIR.pilots.home, away). */
  saved: Record<Side, number>;
  /**
   * How the computer flies the defence (styles.ts; the tactics panel's item 5, ruling 239): the share of its weight
   * sent at the escort, and how far its staff climbs above a raid radar warned it of. Absent for a human defender,
   * whose staff's opening plan is the plain one.
   */
  defence?: Defence;
  /** At stops 4 and 5, a computer raid's escort roles by nation, in place of its doctrine's order (styles.ts rolesOf). */
  roles?: Record<string, Role[]>;
}

/** The exact result the rules apply (FieldResult.extra.air): each campaign group's losses, and the bombs dropped. */
export interface AirOutcome {
  /** Each campaign group: what it lost, its downed fighter pilots who fly again, and the aircraft it sent up. */
  groups: { id: number; side: Side; lostFtr: number; lostBmr: number; back: number; flewFtr: number; flewBmr: number }[];
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
}

/** One loss, as the 3D side plays it. */
export interface Fall {
  kind: 'explode' | 'burn' | 'spin';
  /** Parachutes seen. */
  chutes: number;
  /** Over whose ground it came down. */
  over: 'own' | 'enemy' | 'sea';
  /** A fighter pilot picked up who flies again. */
  back: boolean;
  /** A pilot or crew who bailed out over enemy ground: taken prisoner (shown only; the rules count no bomber crews). */
  captured: number;
}
