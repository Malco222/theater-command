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

/**
 * One row of a fleet's ships of a kind (ships by class and navy: naval-battles/ships-by-class-plan.md 5.1, c1): the hulls
 * of one class of the record, its words ('King George V class', or '' for hulls with no row), the worth the rules' fleet
 * power gives each of them (navy.ts fleetPower through shiprows.ts hullWorth), and how many; in the fleet's list order.
 * Only these four keys (plan 6.1, test 6).
 */
export interface ShipRow { id: string | null; label: string; worth: number; n: number }

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
  /**
   * Ruling 240 (damage control): each class's share of loss as the rules read it for this fleet (navy.ts toughOf:
   * navy.json's tough divided by its nation's damage control), where that is not navy.json's own. Absent with the
   * switch off, without the research and in every history campaign: the field then reads navy.json's (sunkCaps).
   */
  tough?: SeaShips;
  /** Ruling 240: what the fleet's ships are fitted with (tech.ts shipKit), for the 3D scenes to read and nothing else. Absent where nothing is. */
  kit?: { stage: 'early' | 'mid' | 'late' | null; vt: boolean; dc: boolean };
  /** Its ships by row, for the kinds the campaign plays by class (ships by class, with the campaign's switch on): hulls
   *  past the rows' count have no row and the kind's weight. Absent, every ship of a kind weighs alike, as ever. */
  rows?: Partial<Record<SeaClass, ShipRow[]>>;
}

/** The sea the battle is fought in, and what the campaign knows of it. */
export interface SeaPlace {
  zone: number;
  /** "Norwegian Sea". */
  zoneName: string;
  fleets: FleetTicket[];
  /** The weight of the aircraft that reach from each side's coasts (navy.ts airOver times navy.json air.weight). */
  air: Record<Side, number>;
  /** Men in the water (release 2's sailor rules): only with the campaign's switch on. Absent, nothing of them is ruled. */
  sailors?: SeaSailors;
  /** Captains mind their own side's torpedoes (release 2's own water: sim.ts ownWater): only with the campaign's switch
   *  on (ticket.ts ownWaterOn). Absent, the field is as before to the byte. */
  ownWater?: true;
}

/** The band of the water men go into (data/battle/sailors.json water): cold, temperate or warm. */
export type SeaWater = 'arctic' | 'cold' | 'temperate' | 'warm';

/**
 * Men in the water (release 2's sailor rules: Brian, 22 November 12:49Z; the design director's form ruling 12:58Z and
 * figures 17:42Z; naval-battles/sailors-r2/form.md). On a ticket only with the campaign's switch on (ticket.ts
 * sailorsOn): absent, the field is release 2's to the byte. It moves who wins nothing: the calls are the staff's unless
 * the player makes them, and the staff's are how the field books every loss today.
 */
export interface SeaSailors {
  /** The sea's band this month (the historian's table by sea and month: data/battle/sailors.json water). */
  water: SeaWater;
  /** Each side's call after the fight, where it holds the water (form B2): stop for survivors, or leave them. The staff's
   *  call is the campaign's to make (form B3); the player's is his own. A side with none stops. */
  stop?: Partial<Record<Side, 'stop' | 'leave'>>;
  /** The fleets, by campaign id, whose standing call is "Abandon early" (form C1); every other fleet's is "Fight for her",
   *  the staff's and today's. */
  early?: number[];
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
  | { kind: 'leave' }
  /** Pick up survivors (form B1, with men in the water only): lie stopped beside the wreck {unit, ship} and take her men
   *  aboard, guns still firing at anything in reach, until told otherwise or she is empty. */
  | { kind: 'rescue'; unit: number; ship: number };

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
  /** A destroyer piece of its fleet's screen (data/battle/sea.json torp), and the enemy piece it is running in on
   *  with torpedoes while it makes a run (ruling 239): for the screens and the 3D game. */
  screen?: boolean;
  run?: number;
  [k: string]: unknown;
}

/** A piece on the field. */
export interface SeaUnit extends UnitBase {
  extra: SeaExtra;
}

/** The sea's orders on top of the core's: the speed to make; with men in the water (SeaSailors), a ship's abandon-ship
 *  call, changed as she is hit (form C1), and the order to pick up survivors at a wreck (form B1). */
export type SeaCommand = Command | { do: 'sea:speed'; units: number[]; speed: SeaSpeed } | { do: 'sea:abandon'; units: number[]; early: boolean }
  | { do: 'sea:rescue'; units: number[]; wreck: { unit: number; ship: number } };

/** The fleet actions of the first slice are all of one kind (ruling 217): two fleets meeting at the month's end. */
export const SEA_KIND: BattleKind = 'field';
