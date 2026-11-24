// The numbers of a fleet action on the field, from data/battle/sea.json, and the rules' own ship classes from
// data/campaign/navy.json, so the field weighs a ship exactly as the campaign does.

import seaData from '../../../data/battle/sea.json';
import sailorsData from '../../../data/battle/sailors.json';
import navyData from '../../../data/campaign/navy.json';
import carrierData from '../../../data/campaign/carriers.json';
import type { SeaClass, SeaStyle, SeaWater } from './types';

interface ClassOnField { speed: number; turn: number; best: number; reach: number; per: number; crew: number; len: number; beam: number; standoff?: number }

export const SEA = seaData as unknown as {
  field: { size: number; edge: number; limit: number; row: number };
  classes: Record<SeaClass, ClassOnField>;
  speeds: { slow: number; cruise: number; flank: number; accel: number };
  fire: { rate: number; hull: number; far: number; endOn: number; damaged: number; retarget: number };
  nerve: { hit: number; near: number; nearR: number; quiet: number; mend: number; waver: number; wavering: number; break: number };
  side: { breakAt: number; spread: number; admiral: number; pursuit: number; hurt: number };
  strike: { windup: number; every: number; share: number; radius: number };
  air: { every: number; share: number };
  pieces: { most: number; front: number };
  ai: { keep: number; beyond: number; station: number; broadside: number };
  evade: { see: number; speed: number; screen: number };
  jobs: { fire: Record<Exclude<SeaClass, 'cv'>, SeaClass[]>; strike: SeaClass[] };
  torp: { start: number; travel: number; bank: number; away: number; again: number; radius: number; station: number; intercept: number };
  styles: Partial<Record<SeaStyle, StyleNumbers>>;
  carrierBattle: 'field' | 'staff';
  look: { limpSpread: number; limpSpeed: number };
  line: { bend: number; across: number; edge: number; margin: number; crossed: number; settle: number };
  carriers: { path: Record<string, CarrierWay[]>; grow: string[] };
};

/** A navy's way of keeping its carriers together where they keep no station, at one step of its path (sea.json
 *  carriers; the design director, 20:50Z): each for herself never inside 'spacing' metres of a sister, or on a guide,
 *  'spacing' metres abreast. */
export interface CarrierWay { form: 'herself' | 'guide'; spacing: number }

/** A style's numbers (ruling 239; data/battle/sea.json _about.styles). */
export interface StyleNumbers { spread: number; torpWhen: number; press: number; screen: number; cover: number }

/** The rules' classes: a ship's weight in a fleet action (power) and how readily it is sunk (tough: lower is tougher). */
export const RULES_CLASSES = (navyData as unknown as { classes: Record<SeaClass, { label: string; one: string; many: string; power: number; tough: number }> }).classes;

/** The rules' fleet action: the odds an admiral wants before offering battle, and the swing of the dice (navy.ts seaBattles). */
export const RULES_BATTLE = (navyData as unknown as { battle: { seek: number; evadeAt: number; evade: number } }).battle;
export const DICE: [number, number] = [0.8, 1.2];

/** The campaign's own aim of a strike from the air at a fleet (carriers.json strike.aim, read only): how much likelier a
 *  ship of each class is to be the one found and hit. The coasts' raids on the field aim by it (ruling 239, item 4). */
export const STRIKE_AIM = (carrierData as unknown as { strike: { aim: Partial<Record<SeaClass, number>> } }).strike.aim;

/** The campaign's own law for fighters over a fleet and its guns (carriers.json, read only; air.ts's raid on ships):
 *  cap.spoil, what fighters spoil of a strike at full cover; flak.ships, each class's guns against aircraft, times
 *  flak.per a ship, to at most flak.most of a strike. */
export const CAP = (carrierData as unknown as { cap: { spoil: number } }).cap;
export const FLAK = (carrierData as unknown as { flak: { ships: Partial<Record<SeaClass, number>>; per: number; most: number } }).flak;

/** Men in the water (release 2's sailor rules; data/battle/sailors.json): the record's figures as the design director
 *  ruled them, read only where a ticket carries SeaSailors. 'oil.r', 'oil.burn' and 'abandon.afloat' are looks-only knobs
 *  (see their _about). */
export const SAILORS = sailorsData as unknown as {
  down: { sudden: number; fought: number; early: number };
  dead: { share: number };
  alive: Record<SeaWater, [number, number][]>;
  found: { stop: number; leave: number };
  pick: { wreck: number; early: number };
  refuse: { navies: string[] };
  oil: { r: Record<SeaClass, number>; burn: number };
  abandon: { afloat: number };
  water: Record<string, string>;
};

/** The share of the men who went into water of this band still alive after 'h' hours in it: straight lines between the
 *  record's points, none past the band's last point (sailors.json alive). */
export function aliveAfter(band: SeaWater, h: number): number {
  const p = SAILORS.alive[band];
  if (h <= p[0][0]) return p[0][1];
  // A point itself is the record's figure, never a sum that misses it by a rounding (arctic's 0 at three hours).
  for (let i = 1; i < p.length; i++) if (h <= p[i][0]) return h === p[i][0] ? p[i][1] : p[i - 1][1] + ((p[i][1] - p[i - 1][1]) * (h - p[i - 1][0])) / (p[i][0] - p[i - 1][0]);
  return 0;
}

/** The band of a sea's water in a month (0 January), from the historian's table; null for a sea the table lacks. 'a',
 *  arctic, only in the sea-months the historian lays as under 4 C (the design director, 18:50Z). */
export function waterOf(sea: string, month: number): SeaWater | null {
  const b = SAILORS.water[sea]?.[month];
  return b === 'a' ? 'arctic' : b === 'c' ? 'cold' : b === 't' ? 'temperate' : b === 'w' ? 'warm' : null;
}
