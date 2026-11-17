// The shared battle core: the contract between the campaign and every played battle, land or sea.
//
// The campaign decides who fights, where and with what (src/campaign). When the player chooses to fight a battle
// out instead of letting the dice decide, the campaign writes a FieldTicket, a sim for that domain (src/battle/land,
// src/battle/sea) plays it in real time, and the FieldResult that comes back is boiled down to one number, R: the
// ratio the dice would otherwise have rolled. The rules then apply their own losses, retreats and prisoners from R,
// exactly as they do for an auto-resolved battle. So a played battle can never break the campaign's books: it only
// decides where on the campaign's own scale the result falls.
//
// Everything here is plain data that crosses to the 3D game as JSON, the way the campaign rules do today.
//
// The field's frame, shared by every domain and by the 3D game's camera:
//   metres, origin at the south-west corner, x runs east and y runs north;
//   the attacker comes on from the south edge (y = 0), the defender holds the north;
//   a facing is an angle in radians, 0 = north, PI/2 = east (clockwise, like a compass).
//
// Abstract by design (see project memory air-battle-paused): units are blocks with a strength meter and a morale
// meter that wear each other down at a rate while in contact. There are no weapons, rounds or ballistics.

import type { TicketLine, VictoryLevel } from '../../core/ticket';

export type Side = 'att' | 'def';
export type Domain = 'land' | 'sea' | 'air';

/** One fixed tick of the battle clock, in seconds of battle time. Every sim steps by exactly this much. */
export const DT = 0.1;

/** How the battle is won (the framework's table of battle types; the first slice plays 'field' and 'assault'). */
export type BattleKind = 'field' | 'assault' | 'crossing' | 'sally' | 'relief' | 'rearguard' | 'breakout' | 'landing';

/** The campaign's own reckoning of the fight: the field is balanced to it, so that the odds mean the same thing on both. */
export interface Reckoning {
  /** The attack's and the defence's power, as the rules' assessment counts them (assess() in game.ts). */
  A: number;
  D: number;
  /** A / D. */
  odds: number;
  /** The run of the day's luck the dice would roll, and the odds at which the dice give an even chance. */
  dice: [number, number];
  even: number;
}

/** One corps (or ship group) as the campaign hands it to the field. */
export interface ForceTicket {
  /** The corps' id in the campaign: results are reported back against it. */
  corps: number;
  name: string;
  nat: string;
  /** The campaign's kind: 'inf', 'mot', 'arm', 'cav', 'mtn', 'fort', 'art', 'eng' on land. */
  kind: string;
  divs: number;
  men: number;
  /** 0..1 strength, cohesion and experience, and morale where the campaign keeps it. */
  str: number;
  org: number;
  xp: number;
  mor?: number;
  /** Dug in where it stands (land): starts the battle in fieldworks. */
  dug?: boolean;
  /** Its share of its side's power in the reckoning, 0..1. The shares of a side add up to 1. */
  share: number;
}

export interface SideTicket {
  nat: string;
  /** "German", "Polish": the adjective the cards use. */
  adj: string;
  /** The nation's colour on the campaign map, for blocks and bars. */
  color: string;
  /** True where the player commands this side. */
  human: boolean;
  forces: ForceTicket[];
  /** The general in command of the side's strongest corps, if any: his name and his level. */
  general: { name: string; level: number } | null;
}

/** Everything a battle needs from the campaign, and nothing it can change. P is the domain's own picture of the place. */
export interface FieldTicket<P = unknown> {
  domain: Domain;
  kind: BattleKind;
  /** The battle's own random stream: the same ticket and the same orders always replay the same battle. */
  seed: number;
  turn: number;
  date: string;
  /** "The Battle of Lodz". */
  title: string;
  place: P;
  att: SideTicket;
  def: SideTicket;
  reckoning: Reckoning;
  /** The campaign's Battle Ticket lines, shown before the fight as they are on the attack card. */
  lines: TicketLine[];
  /** Battle time in seconds before the defender is counted as having held. */
  limit: number;
  /** The campaign's weather over the place, in its own words ('clear', 'rain', 'snow', 'mud'), where it keeps any. */
  weather?: string;
}

/** What a unit is doing. */
export type UnitState = 'steady' | 'wavering' | 'routing' | 'gone' | 'withdrawn';

/** The fields every domain's units share, and the 3D game draws from. A domain's own unit type extends this. */
export interface UnitBase {
  id: number;
  side: Side;
  /** The campaign corps it was drawn from. */
  corps: number;
  nat: string;
  kind: string;
  /** "1st Bn, IV Corps": the card's title. */
  name: string;
  x: number;
  y: number;
  face: number;
  /** 0..1: what is left of it. */
  str: number;
  /** 0..1: its nerve. Below the domain's wavering mark it fights worse; below the routing mark it runs. */
  mor: number;
  state: UnitState;
  /** Men or crews at the start, for the card ("612 / 800"). Display only: the campaign is paid in R. */
  men0: number;
  /** Who can see it: a unit not spotted by a side is left out of that side's view. Absent means both see it. */
  spotted?: Record<Side, boolean>;
  /** Its timed abilities (smoke, a barrage, a torpedo run): what each is and when it is ready. */
  abilities?: AbilityState[];
  /** The domain's own fields the screens may draw (a ship's way, a plane's height). Kept as plain data. */
  extra?: Record<string, unknown>;
}

/** A timed ability: pressed, it winds up, acts, then needs time before it can be used again. */
export interface AbilityState {
  key: string;
  label: string;
  /** Seconds until it can be used again; 0 when ready. */
  ready: number;
  /** Seconds between the order and the effect. */
  windup: number;
  /** True from the order to the effect. */
  winding: boolean;
  /** Uses left this battle; absent means no limit. */
  left?: number;
}

/** A patch of the field with an effect on it for a while (a smoke bank, a barrage's beaten zone), for the screens. */
export interface Zone {
  id: number;
  kind: string;
  x: number;
  y: number;
  /** Radius in metres. */
  r: number;
  /** Battle time it lasts until. */
  until: number;
  side?: Side;
}

/** Orders every domain understands. Unit ids are the field's own. */
export type CoreCommand =
  /** March there. 'face' turns them on arrival; omitted, they face the way they marched. */
  | { do: 'move'; units: number[]; x: number; y: number; face?: number; run?: boolean }
  /** Draw a line from (x0, y0) to (x1, y1): the units spread along it, facing away from their own edge. */
  | { do: 'line'; units: number[]; x0: number; y0: number; x1: number; y1: number }
  /** Close with that enemy unit and wear it down. */
  | { do: 'attack'; units: number[]; target: number }
  /** Stop and stand where they are. */
  | { do: 'hold'; units: number[] }
  /** Leave the field by their own edge, to fight another day: what leaves is not lost, but the ground is. */
  | { do: 'withdraw'; units: number[] }
  /** The whole side withdraws: the battle ends as a defeat for that side, at the price paid so far. */
  | { do: 'withdrawAll'; side: Side }
  /** A timed ability, at a point or at a unit where it needs one. */
  | { do: 'ability'; units: number[]; key: string; x?: number; y?: number; target?: number }
  /** Hand the side to the computer (true) or take it back (false). Both sides on the computer is "finish it for me". */
  | { do: 'auto'; side: Side; on: boolean };

/** A domain's own orders: 'sea:screen', 'land:dig'. */
export interface DomainCommand { do: `${Domain}:${string}`; units?: number[]; [k: string]: unknown }

export type Command = CoreCommand | DomainCommand;

/** Something that happened, for the dispatches strip, the sound and the camera. */
export interface FieldEvent {
  t: number;
  /** The core's kinds; a domain may add its own as '<domain>:<kind>' ('sea:sunk', 'air:lost'). */
  what: 'contact' | 'flanked' | 'wavering' | 'routing' | 'rallied' | 'gone' | 'withdrawn' | 'broken' | 'objective' | 'ability' | 'end' | `${Domain}:${string}`;
  unit?: number;
  side?: Side;
  text: string;
  /** The domain's own details, for the 3D side (how a ship went down, where a barrage fell). */
  extra?: Record<string, unknown>;
}

/** How a battle ended. */
export interface FieldResult {
  winner: Side;
  /** 'rout': the loser's army broke. 'withdrew': the loser left the field. 'time': the clock ran out and the defender held. 'objective': the assault took or failed to take its ground. */
  how: 'rout' | 'destroyed' | 'withdrew' | 'time' | 'objective';
  seconds: number;
  /** The share of each side's starting power that was lost (strength worn away; routed units count by what they lost). */
  lost: Record<Side, number>;
  /** Men lost on the field, for the after-action card only. */
  men: Record<Side, number>;
  /** The ratio handed back to the campaign: where the dice would have landed. See result.ts. */
  R: number;
  /** The level as the attacker reads it, by the campaign's own scale. */
  level: VictoryLevel;
  /** What happened, in a few plain lines. */
  lines: string[];
  /** Each campaign force's share of its strength lost on the field, for a domain that pays some of the bill exactly. */
  forces: { corps: number; side: Side; lost: number }[];
  /** The domain's own exact results the campaign must apply as they happened (sea: ships sunk by fleet and class). */
  extra?: Record<string, unknown>;
}

/** The plain picture of the battle the screens draw from: the browser page and the 3D game alike. */
export interface BattleView<U extends UnitBase = UnitBase> {
  t: number;
  limit: number;
  units: U[];
  /** Each side's standing power as a share of both sides' at the start: the balance bar. */
  balance: Record<Side, number>;
  /** New since the last view. */
  events: FieldEvent[];
  zones: Zone[];
  /** Which sides the computer is commanding. */
  ai: Record<Side, boolean>;
  over: FieldResult | null;
}

/** What a domain's sim offers the clock, the screens and the hand-back. */
export interface BattleSim<U extends UnitBase = UnitBase> {
  readonly domain: Domain;
  readonly ticket: FieldTicket;
  /** Battle time in seconds. */
  t: number;
  units: U[];
  over: FieldResult | null;
  /** Which sides the computer commands. The player's side is false until he hands it over. */
  ai: Record<Side, boolean>;
  /** One fixed tick of DT seconds. Does nothing once the battle is over. */
  step(): void;
  /** False if the order was refused (a unit that is routing, gone or not the commander's). Orders are taken while paused. */
  command(c: Command, by: Side): boolean;
  /** The picture, with the events since the last call: as one side sees it (unspotted enemies left out), or everything. */
  view(as?: Side): BattleView<U>;
}
