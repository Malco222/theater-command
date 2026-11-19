// The numbers of a fleet action on the field, from data/battle/sea.json, and the rules' own ship classes from
// data/campaign/navy.json, so the field weighs a ship exactly as the campaign does.

import seaData from '../../../data/battle/sea.json';
import navyData from '../../../data/campaign/navy.json';
import type { SeaClass, SeaStyle } from './types';

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
  jobs: { fire: Record<Exclude<SeaClass, 'cv'>, SeaClass[]> };
  styles: Partial<Record<SeaStyle, StyleNumbers>>;
};

/** A style's numbers (ruling 239; data/battle/sea.json _about.styles). */
export interface StyleNumbers { spread: number }

/** The rules' classes: a ship's weight in a fleet action (power) and how readily it is sunk (tough: lower is tougher). */
export const RULES_CLASSES = (navyData as unknown as { classes: Record<SeaClass, { label: string; one: string; many: string; power: number; tough: number }> }).classes;

/** The rules' fleet action: the odds an admiral wants before offering battle, and the swing of the dice (navy.ts seaBattles). */
export const RULES_BATTLE = (navyData as unknown as { battle: { seek: number; evadeAt: number; evade: number } }).battle;
export const DICE: [number, number] = [0.8, 1.2];
