// The sea's own shapes on top of the shared battle core (src/battle/core).
//
// A fleet action on the field is played by pieces: each battleship, carrier and cruiser is a piece of its own, and
// destroyers sail in fours (data/battle/sea.json). A piece is a UnitBase whose 'corps' is the campaign fleet it came
// from, and whose 'str' is what is left of its hull. Ships in a piece of several go down one at a time as its hull
// wears away.
//
// Abstract by design (project memory air-battle-paused, ruling 217): the field counts weight, range, angle, hull and
// nerve. Nothing is aimed and nothing flies; a carrier's strike is counted, as the campaign's card counts it.

import type { BattleKind, Command, FieldTicket, Side, UnitBase } from '../core/types';

/** The classes that fight a fleet action. Submarines do not (navy.ts: "Submarines fight no fleet action"). */
export type SeaClass = 'bb' | 'cv' | 'ca' | 'dd';

/** How a side's captains fight (ruling 239): a temperament, not a weapon. data/battle/sea.json styles. */
export type SeaStyle = 'bold' | 'methodical' | 'mass';
export const SEA_CLASSES: SeaClass[] = ['bb', 'cv', 'ca', 'dd'];

/** Ships by class, as the rules count them (types.ts Ships), without submarines. */
export type SeaShips = Record<SeaClass, number>;

/** One campaign fleet as the field receives it. */
export interface FleetTicket {
  /** The fleet's id in the campaign: what it lost is reported back against it. */
  id: number;
  name: string;
  nat: string;
  side: Side;
  ships: SeaShips;
  /**
   * Everything the rules multiply a ship's weight by for this fleet, apart from the carrier's deck (navy.ts
   * fleetPower): the navy's quality, its fitness for sea, its admiral, its story and its nation's traits.
   */
  mul: number;
  /** A carrier's worth as the rules count it, 0..1 (carriers.ts deckWorth): an empty deck counts for less. */
  deck: number;
  /** Bombers in its carriers' wings, for the strike's count on the card ("24 bombers"). */
  bombers: number;
  admiral: { name: string; level: number } | null;
}

/** The sea the battle is fought in, and what the campaign knows of it. */
export interface SeaPlace {
  zone: number;
  /** "Norwegian Sea". */
  zoneName: string;
  fleets: FleetTicket[];
  /** The weight of the aircraft that reach from each side's coasts (navy.ts airOver times navy.json air.weight). */
  air: Record<Side, number>;
}

export type SeaTicket = FieldTicket<SeaPlace>;

export type SeaSpeed = 'slow' | 'cruise' | 'flank';

/** What a piece has been told to do. */
export type SeaOrder =
  | { kind: 'free' }
  | { kind: 'move'; x: number; y: number; face?: number }
  /** 'fleet': given to two or more pieces at once, the order sets the fleet's main effort and each piece does its job
   *  around it; given to one piece, it is that piece's own target (ruling 239). */
  | { kind: 'attack'; target: number; fleet?: boolean }
  | { kind: 'hold' }
  | { kind: 'leave' };

/** The sea's own fields on a piece, for the screens (UnitBase.extra) and for the sim. */
export interface SeaExtra {
  cls: SeaClass;
  /** Ships in the piece at the start, and still afloat. */
  ships0: number;
  ships: number;
  /** Ships of the piece sunk, and ships that went out of the fight burning (past the dice's limit: ruling 217). */
  sunk: number;
  /** Way on, in metres a second, and the speed it was told to make. */
  way: number;
  speed: SeaSpeed;
  /** The enemy piece it is firing on, or -1. */
  target: number;
  /** The campaign fleet it came from, by name, for the card ("Home Fleet"). */
  fleet: string;
  /** A carrier's aircraft still in the fight, from 1 at the start: they are worn as the fleet is (b415). */
  planes?: number;
  [k: string]: unknown;
}

/** A piece on the field. */
export interface SeaUnit extends UnitBase {
  extra: SeaExtra;
}

/** The sea's orders on top of the core's: the speed to make. */
export type SeaCommand = Command | { do: 'sea:speed'; units: number[]; speed: SeaSpeed };

/** The fleet actions of the first slice are all of one kind (ruling 217): two fleets meeting at the month's end. */
export const SEA_KIND: BattleKind = 'field';
