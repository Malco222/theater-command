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
// Abstract by design (see project memory air-battle-paused, and the design director's ruling 217): units are blocks
// with a strength meter and a morale meter that wear each other down at a rate while in contact. There are no
// weapons, rounds or ballistics.
//
// Drawn up close later (Brian, 2026-11-17 17:50Z: "use the 3d models we made and make them animated ... with the
// sound effects"): the sims decide where a block is and what it is doing; the 3D game turns that into animated
// models and sound. So every unit says how many models it shows ('figures'), how it is drawn up ('w', 'd'), what it
// is doing for the animation ('act', 'speed', 'engaged', 'cover'), and events name the sound that answers them
// ('cue', from the sound director's list). Presentation reads these; nothing in a sim reads them back.

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
  /**
   * Where the attack card did not know the defence (b453): the odds as the card put them to the player, the middle of
   * the staff's band, where its bar stands. Absent, the card printed 'odds' itself.
   */
  seen?: number;
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
  /** The number its first division (or ship, or group) carries, for the blocks' names. */
  first?: number;
  /** Its divisions as the campaign's army tray shows them (land): the name and short name, strength, experience, men. */
  divisions?: { name: string; short: string; str: number; xp: number; men: number }[];
  /** Its share of its side's power in the reckoning, 0..1. The shares of a side add up to 1. */
  share: number;
  /** The battles it has come through before this one, staff-settled or fought (its record): from the third, a tank unit's crews name their tanks (b477). */
  battles?: number;
  /**
   * Land, an attack over a river: false where the card has this corps come on from ground with no river between it and
   * the place ("1 of the 2 attacking corps cross the Vistula"). The card prices it attacking over dry ground at full
   * strength, so the field draws it that ground: the river bends away from its lanes and lies only before the corps that
   * cross (the design director, b464(5)). Absent, it crosses, as every corps of a practice card does.
   */
  cross?: boolean;
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
  /**
   * Which attack on this ground this month it is, from 0: a second attack on the same place keeps its seed (and so its
   * ground) but draws a fresh day's luck, as the campaign's own dice would.
   */
  attempt?: number;
}

/** What a unit is doing. */
export type UnitState = 'steady' | 'wavering' | 'routing' | 'gone' | 'withdrawn';

/** What a unit's models are doing, for the 3D game's animation and the sound's loops. */
export type Act = 'idle' | 'march' | 'run' | 'turn' | 'fight' | 'rout' | 'down';

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
  /** For the models: how many it showed at the start and how many it shows now (one fewer as its strength falls). */
  figures0: number;
  figures: number;
  /** Frontage and depth in metres: the models stand in ranks inside this box, the long side to the front. */
  w: number;
  d: number;
  /** What it is doing, for the animation: standing, marching, at the double, turning, in a fight, running, gone. */
  act: Act;
  /** Speed over the ground now, metres a second, for walk and drive cycles. */
  speed: number;
  /** The enemy unit it is fighting, where it is in a fight: the models turn to face it. */
  engaged?: number;
  /** The ground under it (a domain's own word: 'woods', 'buildings', 'open'): the models take cover in it. */
  cover?: string;
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
  /** Battle time it began, where that matters to how it is drawn (a wreck burns, then smokes: b477). */
  at?: number;
  /** The unit it came from, where it is one's (a wreck: the block whose tank it was, so it is drawn as her model). */
  unit?: number;
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
  /** Hand the side to the computer (true) or take it back (false). The fairness test puts both sides on it. */
  | { do: 'auto'; side: Side; on: boolean }
  /** Hand it to the staff (ruling 217(4); the button is named in b348): the rest goes back to the dice, rolled at the ratio then standing. */
  | { do: 'dice' };

/** A domain's own orders: 'sea:screen', 'land:dig'. */
export interface DomainCommand { do: `${Domain}:${string}`; units?: number[]; [k: string]: unknown }

export type Command = CoreCommand | DomainCommand;

/** Something that happened, for the dispatches strip, the sound and the camera. */
export interface FieldEvent {
  t: number;
  /** The core's kinds; a domain may add its own as '<domain>:<kind>' ('sea:sunk', 'air:lost'). */
  what: 'contact' | 'flanked' | 'wavering' | 'routing' | 'rallied' | 'gone' | 'withdrawn' | 'broken' | 'nearBreak' | 'objective' | 'ability' | 'end' | `${Domain}:${string}`;
  unit?: number;
  side?: Side;
  text: string;
  /** The domain's own details, for the 3D side (how a ship went down, where a barrage fell). */
  extra?: Record<string, unknown>;
  /**
   * On 'broken' (land): why the army broke, as it stood at the break (b496(2)); the same as the result's brokeBy. On the
   * player's own 'nearBreak', what drives his bar as it stood at the warning (never on the enemy's, which under fog would
   * tell more than its bar shows).
   */
  brokeBy?: BreakCause;
  /** The sound that answers it: a name from the sound director's list (/mnt/project-files/sound/sound-list.json). */
  cue?: string;
  /** Where on the field it happened, for the camera and for placing the sound. */
  x?: number;
  y?: number;
}

/**
 * What drives an army's bar on the land field, and so why it broke (b496(2)): the two measures the bar takes the
 * nearer of, its blocks as their cards read them, and the fresh hold. An army breaks when its bar reaches 1: when 'worn'
 * reaches 'spent' and the fresh hold no longer keeps it at the brink, or when 'running' reaches 'breaks'. Shares are of
 * the army's power at the start, each block weighted by its power, so one big block worn counts for more than a small
 * one; all to three places. Numbers only: the words are the UI director's.
 */
export interface BreakCause {
  side: Side;
  /** Its bar, how near breaking it stands, 0 to 1, as breaking() reckons it: 1 at a break, 0.99 while the hold holds it. */
  bar: number;
  /**
   * Which measure filled the bar: 'nearer' once it has reached its mark, 'none' where neither has (an army driven from
   * the field may reach neither).
   */
  by: 'worn' | 'running' | 'none';
  /**
   * Which measure stands higher against its mark, reached or not: 'worn' (worn / spent) or 'running' (running / breaks);
   * at a tie, 'running', and 'worn' while both are 0. The warning's words go by it (the UI director: 'losses' is 'worn').
   */
  nearer: 'worn' | 'running';
  /** The share of its power worn away across the whole line: strength lost by every block, runners and fled too. */
  worn: number;
  /** What an army of its side will stand worn (LAND.spent: 0.13 for an attack, 0.18 for a defence). */
  spent: number;
  /** The share of its power in blocks running or fled from the field, each counted whole as it started. */
  running: number;
  /** What it will stand running (LAND.breaks, a half). */
  breaks: number;
  /**
   * The share of its power that holds it at the brink: blocks still in the fight that have not met the enemy and stand
   * near whole, and in a small army also the fresh (see 'small').
   */
  fresh: number;
  /** Worn to its mark, it holds at the brink (bar 0.99) while 'fresh' is more than this (LAND.fought, a half). */
  fought: number;
  /**
   * Whether its steady blocks at nine tenths of their strength or more count as fresh: an army of five blocks or fewer,
   * not a defence behind works or in a fort (b484, b488).
   */
  small: boolean;
  /** Worn to its mark, the fresh hold keeps its bar at the brink (0.99) now. Never at a break. */
  holding: boolean;
  /** The fresh hold had kept it at the brink at some time this battle. */
  held: boolean;
  /** For how many battle seconds in all the fresh hold has kept it at the brink, to a tenth. */
  heldFor: number;
  /** Its blocks: every one it brought, fortress works too. */
  blocks: number;
  /** How many of its blocks stand below 'wornAt' of their starting strength, whatever their state (runners and fled too). */
  blocksWorn: number;
  /** Below this share of its starting strength a block counts as worn: LAND.fresh.str, nine tenths, the hold's line. */
  wornAt: number;
  /** How many of its blocks are running. */
  routing: number;
  /** How many of its blocks have fled the field ('gone'; not those the player withdrew). */
  fled: number;
  /**
   * The share of its power, counted as it started, in blocks whose card reads Steady, of all it brought but those the
   * player withdrew; and that in the UI director's three words: 'all' every part of it steady, 'most' more than half,
   * 'less' the rest. For the break line's "though every part of it was still steady" (the UI director, 14:44Z).
   */
  steady: number;
  stillSteady: 'all' | 'most' | 'less';
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
  /** The side whose army broke on the field (routed, or driven from it); absent where it withdrew, held or was handed over. */
  broke?: Side;
  /**
   * Why it broke (land, b496(2)): its bar and what drove it, as it stood at the break, where 'broke' is set. Driven from
   * the field ('destroyed'), none of its blocks was left in the fight, and its bar may stand short of 1 ('none').
   */
  brokeBy?: BreakCause;
  /**
   * The side that withdrew its army after the armies met, and how near breaking it stood then (its bar, 0 to 1): it pays
   * the rules' bill for that defeat times the bar squared, or its field loss where that is larger (b474).
   */
  pulled?: { side: Side; bar: number };
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
  /**
   * How near each army is to breaking, 0 to 1, for the army bars (ruling 226(3)); a domain without one leaves it out.
   * The player's own army is warned at three quarters ('nearBreak').
   */
  breaking?: Record<Side, number>;
  /** What drives each army's bar as it stands now (b496(2)): the same fields the result's brokeBy gives at the break. */
  breakingBy?: Record<Side, BreakCause>;
  /** Battle time of the first contact between the lines, null before it: until then the screens may run the clock faster (ruling 226(1)). */
  contact?: number | null;
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
