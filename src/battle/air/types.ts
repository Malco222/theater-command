// The air battle's own pieces, on the shared battle core (src/battle/core).
//
// An air group is a block: a count of aircraft, a morale meter, fuel and a height. The 3D side draws its own
// aircraft models in formation for it and acts out what the counts decide: a fight is a swirl of its models round
// the fight's centre, a loss is one model falling (see 'air:lost' events). Nothing here flies a single aircraft.

import type { AirMission } from '../../campaign/types';
import type { Side, UnitBase } from '../core/types';
import type { Band } from './book';

export type AirKind = 'ftr' | 'bmr';

/** What an air group is doing. Orders set it; the computer sets it for its own side. */
export type AirJob =
  /** Bombers: fly to the target, hold the run, drop, turn for home. */
  | { do: 'route' }
  /** Fighters: stay with that bomber group: close (glued to it) or loose (sweeping ahead of it). */
  | { do: 'escort'; on: number; close: boolean }
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
  /** Losses still owed below one whole aircraft; a whole one falls when this reaches 1. */
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
  /** Where each campaign group comes on: board position, height, fuel, and whether it waits on the ground. */
  starts: Record<number, { x: number; y: number; band: Band; fuel: number; wait: boolean }>;
  /** Fighter pilots who come down over this side's ground fly again at this rate (AIR.pilots.home, away). */
  saved: Record<Side, number>;
}

/** The exact result the rules apply (FieldResult.extra.air): each campaign group's losses, and the bombs dropped. */
export interface AirOutcome {
  groups: { id: number; side: Side; lostFtr: number; lostBmr: number; back: number }[];
  /** Bombers that dropped on the target, and bombers that jettisoned and turned back. */
  dropped: number;
  jettisoned: number;
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
