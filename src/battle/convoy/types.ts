// The wolf pack and convoy battle's own shapes on top of the shared battle core (release 2; ruling 241 D2, with the
// design director's rider of 24 November 12:34Z under Units decide).
//
// One night on one convoy. The pack is the attack (att): one boat a piece, up to 20. The convoy is the defence (def):
// its escorts, up to 16 pieces (several ships to a piece where the guard holds more), and its merchants, one hull a
// ship, in columns. A piece's 'corps' is the campaign fleet it came from (0 for the convoy's merchants).
//
// Abstract by design (ruling 217(1)): contacts, attacks and what they do are rolls on the battle's own stream; the
// courses, depths and looks act them out for the screens and the 3D sea scene, in the names that scene reads
// (naval-battles/wolfpack-r2/field-side-read.md §3; form.md §5).

import type { BattleView, FieldTicket, Side, UnitBase } from '../core/types';
import type { ConvoyCountRecord } from '../../campaign/navy';

/** The kinds on a convoy's night: a boat, an escort, a merchant. A separate table from the fleet action's (form §3). */
export type ConvoyClass = 'ss' | 'dd' | 'mv';

/** A navy's anti-submarine kit (navy.ts AswKit, by its own research): sonar, sea-search radar, ahead-throwers, group training. */
export interface ConvoyKit { asdic: boolean; radar: boolean; hedgehog: boolean; groups: boolean }

/** A navy's staff habits (convoy.json doctrine): boats homed from ashore; a salvo at an escort before a merchant. */
export interface ConvoyHabits { home: boolean; warships: boolean }

/** One campaign fleet on the night: its boats (raiders) or its escorts, and what the rules expect it to lose this month. */
export interface ConvoyFleet {
  id: number;
  name: string;
  nat: string;
  side: Side;
  /** Boats (a raider) or escort ships (an escort fleet) it brings to the night. */
  n: number;
  /** All it has on the route this month (what the card counts): a raider's boats, an escort fleet's destroyers. */
  all: number;
  /** What the rules expect it to lose this month, on the card (ConvoyMeeting expect): booked in its run. */
  expect: number;
}

/** The light of the night (form §5): the sun's height, the moon's lit share, height and bearing, and how far a surfaced boat is seen. */
export interface ConvoyLight {
  sun: number;
  moon: { lit: number; up: number; from: number };
  /** Metres at which an escort's lookouts see a surfaced boat: moves the odds (drawn for the night, ruling 241 Q6). */
  vis: number;
  /** Hours from sunset to sunrise. */
  hours: number;
  /** The day of the month, 1 to 31. */
  day: number;
}

/** The convoy's night as the field reads it. */
export interface ConvoyPlace {
  route: string;
  /** The battle sea: its key and name, and where it lies. */
  sea: string;
  seaName: string;
  lat: number;
  lon: number;
  /** The night's month, 0 for January (as the campaign's turnToDate gives it). */
  when: { year: number; month: number };
  light: ConvoyLight;
  /** Merchants in the convoy. */
  ships: number;
  fleets: ConvoyFleet[];
  /** Kit and habits by navy. */
  kit: Record<string, ConvoyKit>;
  habits: Record<string, ConvoyHabits>;
  /**
   * The card's figures (ConvoyMeeting): raid, guard, G, L and the month's tons, for the card and the after card. With the
   * sea patrol live, 'sight' is what the player's side last saw, in his panel's words (campaign/seasight.ts): the boats
   * last seen on the route's seas, and where he holds boats the aircraft last met over the night's sea.
   */
  card: { raid: number; guard: number; G: number; L: number; tons: number; count?: ConvoyCountRecord; sight?: string[] };
  /** Striking and capture (subSurrender): off until the design director and the historian rule its share. */
  surrender: boolean;
  /** The player's own nation where his side holds allies' pieces too: he commands only his own, the staff the rest (ruling 241 Q4). */
  mine?: string;
}

export type ConvoyTicket = FieldTicket<ConvoyPlace>;

/** A boat's depth as the scene draws it. */
export type Dive = 'surfaced' | 'periscope' | 'deep';

/** What a boat is about (her captain's, or the player's order carried out). */
export type BoatDoing = 'out' | 'close' | 'away' | 'evade' | 'deep' | 'up' | 'gun' | 'left' | 'lost';
/** What an escort is about. */
export type EscortDoing = 'screen' | 'run' | 'search' | 'attack' | 'open' | 'back' | 'rescue' | 'move' | 'lost';

/** An escort's contact, as the scene reads it: the boat, how it is held, its range and bearing, and the error drawn on it (a look). */
export interface Echo { on: number; kind: 'asdic' | 'radar' | 'sight'; range: number; bearing: number; err: number }

/** What the scene reads off a piece (the recording's extra). Left out where it does not apply. */
export interface ConvoyExtra {
  cls: ConvoyClass;
  ships0: number;
  ships: number;
  sunk: number;
  fleet: string;
  // A boat's.
  dive?: Dive;
  keel?: number;
  scope?: boolean;
  tubes?: number;
  heard?: number[];
  gun?: number;
  gunMm?: number;
  // An escort's.
  echo?: Echo;
  huntPhase?: 'search' | 'run' | 'blind' | 'open';
  huntUntil?: number;
  // A merchant's.
  column?: number;
  [k: string]: unknown;
}

export interface ConvoyUnit extends UnitBase {
  kind: ConvoyClass;
  extra: ConvoyExtra;
}

/** The night's own orders (the form's §3 list; every navy has every one). */
export type ConvoyCommand =
  | { do: 'convoy:hunt'; units: number[]; target: number }
  | { do: 'convoy:screen'; units: number[] }
  | { do: 'convoy:rescue'; units: number[]; target: number }
  | { do: 'convoy:starshell'; units: number[]; x: number; y: number }
  /** under: true attacks submerged, false on the surface; left out, the boat keeps the depth her commander last chose (or the one she is at). */
  | { do: 'convoy:attack'; units: number[]; target?: number; under?: boolean }
  | { do: 'convoy:dive'; units: number[]; deep?: boolean }
  | { do: 'convoy:surface'; units: number[] }
  | { do: 'convoy:evade'; units: number[] }
  | { do: 'convoy:gun'; units: number[]; target: number }
  | { do: 'convoy:leave'; units: number[] };

/** The night's counts: boats lost by fleet, escort ships lost by fleet, merchants sunk. The campaign books these. */
export interface ConvoyCounts { ss: Record<number, number>; dd: Record<number, number>; mv: number }

/** Each count's rank against the computer's own nights for the card, 0 to 1 (calibrate.ts). */
export interface ConvoyRanks { ss: Record<number, number>; dd: Record<number, number>; mv: number }

/** What the night did, measured only (the after card's lines; nothing books them). */
export interface ConvoyTally {
  attacks: number; hunts: number; salvoes: number; hits: number;
  /** Boats forced to the surface, and those of them that dived again and got away. */
  forcedUp: number; escaped: number;
  /** How the boats lost went: these add up to the boats lost. */
  scuttled: number; rammed: number; shelled: number; crushed: number; captured: number;
}

/** A piece of the other side that a side has lost touch with: where and when it last knew of her (the faded 'last seen' mark). */
export interface ConvoySpot { id: number; name: string; cls: ConvoyClass; x: number; y: number; t: number; dive?: Dive; kind?: Echo['kind'] }

/** The night as one side sees it (ConvoySim.view): the core's picture, the marks of pieces lately lost from sight, and the pack's size. */
export interface ConvoyView extends BattleView<ConvoyUnit> {
  /** The other side's pieces lost from sight in the last three minutes, at their last known spots (none in the whole picture). */
  lastSeen: ConvoySpot[];
  /** Every boat the night's card brings, those still to come and those gone included (the pack's score counts them all). */
  pack: number;
}

/** What the night's end carries for the after card (FieldResult.extra). */
export interface ConvoyOverExtra {
  counts: ConvoyCounts;
  /** The whole night's measures: after a hand-over, the staff's share of the rest is in them too, split the same way. */
  tally: ConvoyTally;
  handed: boolean;
  /** Where the player handed the night over: the battle second, and the staff's share of the rest as its dice fell. */
  handedAt?: number;
  staff?: { counts: ConvoyCounts; tally: ConvoyTally };
  /** Battle seconds each side was commanded by its player (0: the staff had it all night). */
  steered: Record<Side, number>;
}
