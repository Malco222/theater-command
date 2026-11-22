// Measured armour and guns (release 2; Brian, 22 November 04:03Z to 04:16Z: the named tanks have real advantages from
// their measured performance against their peers, and German tanks are far weaker at the back, so hits there pay and
// raise the chance they explode). The land vehicle modeller's sheet, cut down for the rules (data/battle/armour.json,
// tools/build-armour.mjs): for each gun and each tank, the chance a hit on each face comes through at each range, the
// share of those that put her out of the fight, and how often she burns or blows up.
//
// The fight stays the rules' blocks: a tank block's blow on a tank block (and a gun's over open sights) is weighed by
// the chance its model's gun gets through the face it strikes at that range, against the field's par for that face
// (the mean of the drawn tanks' duels with one another), on top of the field's own weight for a flank or rear blow
// (LAND.aspect). So two even tanks fight as before; a Tiger's front shrugs off a Sherman's 75, and a Panther's rear does
// not. Off unless the battle is opened with it (LandBattle.measured).

import { turnToDate } from '../../core/ticket';
import data from '../../../data/battle/armour.json';
import { modelOf } from './shells';
import type { Kind } from './rules';

type Faces = { front: number[]; side: number[]; rear: number[] };
interface ArmourData {
  ranges: number[];
  models: Record<string, { kills: number; fire?: { front: number; side: number; rear: number }; explodes?: number; heat?: string; radio?: number; sure: string }>;
  duels: Record<string, Record<string, Faces & { before?: Faces }>>;
}
export const ARMOUR = data as unknown as ArmourData;

export type ArmourFace = 'front' | 'side' | 'rear';
/** The field's word for where a blow lands, as the sheet's face. */
export const faceOfAspect = (a: 'front' | 'flank' | 'rear'): ArmourFace => (a === 'flank' ? 'side' : a);

/**
 * The tank and gun models the rules weigh by: the drawn ones of the field's nations (shells.ts DRAWN), and their par,
 * the mean chance a drawn tank's hit on another drawn tank comes through and puts her out, by face, at 500 m.
 */
const DRAWN_TANKS = ['PanzerIV:short', 'PanzerIV', 'Panther', 'Matilda', 'Sherman', 'LightTank', 'T34', 'T34-85', 'CharB1', 'M13', 'Type97', '7TP', 'MediumTank'];
export const PAR: Record<ArmourFace, number> = (() => {
  const out = { front: 0, side: 0, rear: 0 };
  for (const f of ['front', 'side', 'rear'] as ArmourFace[]) {
    let sum = 0, n = 0;
    for (const s of DRAWN_TANKS) for (const t of DRAWN_TANKS) { const c = ARMOUR.duels[s]?.[t]; if (c) { sum += c[f][1] * ARMOUR.models[s].kills; n++; } }
    out[f] = n ? sum / n : 1;
  }
  return out;
})();

/**
 * The model a block fights with in the battle's month, where it is a tank or gun block the sheet knows: the drawn one
 * (shells.ts modelOf), the German Panzer IV with her short 75 until the long gun came in (March 1942), and the Japanese
 * Type 97 with her short 57 until the new turret's 47 mm came in (April 1942, first in action on Bataan).
 */
export function vehicleOf(kind: Kind, nat: string, turn: number): string | null {
  const d = turnToDate(turn);
  let m = modelOf(kind, nat, d.year);
  // (turnToDate's months count from 0.)
  if (m === 'PanzerIV' && d.year * 12 + d.month + 1 < 1942 * 12 + 3) m = 'PanzerIV:short';
  if (m === 'Type97' && d.year * 12 + d.month + 1 >= 1942 * 12 + 4 && ARMOUR.models['Type97:47']) m = 'Type97:47';
  return m && ARMOUR.models[m] && (kind !== 'arm' || ARMOUR.duels[m]) ? m : null;
}

/** The chance a hit from 'shooter' on that face of 'target' comes through at that range (metres), in the battle's month. */
export function through(shooter: string, target: string, face: ArmourFace, range: number, turn: number): number | null {
  const row = ARMOUR.duels[shooter]?.[target];
  if (!row) return null;
  const heat = ARMOUR.models[shooter]?.heat;
  const d = turnToDate(turn);
  const before = heat && row.before && d.year * 100 + d.month + 1 < Number(heat.slice(0, 4)) * 100 + Number(heat.slice(5, 7));
  const c = (before ? row.before! : row)[face], R = ARMOUR.ranges;
  if (range <= R[0]) return c[0];
  for (let i = 1; i < R.length; i++) if (range <= R[i]) return c[i - 1] + ((c[i] - c[i - 1]) * (range - R[i - 1])) / (R[i] - R[i - 1]);
  return c[R.length - 1];
}

/**
 * What a blow on a tank weighs for the armour it strikes (1 at par): the chance the shooter's gun gets through that
 * face at that range and puts her out, over the field's par for the face, held within [lo, hi]. Null where either model
 * is unknown: the field's own weight stands.
 */
export function armourWeight(shooter: string, target: string, face: ArmourFace, range: number, turn: number, lo: number, hi: number): number | null {
  const c = through(shooter, target, face, range, turn);
  if (c === null) return null;
  return Math.min(hi, Math.max(lo, (c * ARMOUR.models[shooter].kills) / PAR[face]));
}

/**
 * How a tank knocked out by a hit on that face ends (looks only, the drawn battle's own dice): she burns (her
 * fire_when_pierced by face), and of those that burn a share blow up (explodes_of_fires); else she is out of the fight.
 */
export function fateOf(target: string | null, face: ArmourFace, r1: number, r2: number): 'out' | 'burn' | 'blow' {
  const m = target ? ARMOUR.models[target] : undefined;
  const fire = m?.fire?.[face] ?? 0.5;
  if (r1 >= fire) return 'out';
  return r2 < (m?.explodes ?? 0.2) ? 'blow' : 'burn';
}
