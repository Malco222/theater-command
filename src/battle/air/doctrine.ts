// How each side really flew, by nation and year (Brian, 2026-11-18 19:35 to 19:38Z: realistic dogfights, realistic
// tactics when fighters attack bombers and bombers defend, escorts doing their job; the design
// /mnt/project-files/air-battles/realism/air-combat-realism.md; the design director's ruling 237).
//
// The battle still decides only how many aircraft fall (ruling 217). This file says how they flew, so the 3D game can
// act each fight out the way it really went: the formation and style of each nation's fighters, the pass a fighter
// makes on a bomber formation, and which aircraft the 3D game draws. The ids in the comments (D, B, E, G and the
// fact-checks FD, FB, FE, FG) are the claims in the research beside the design; rows marked "for the historian" wait
// on the historian's review.

import { TECH, techRules } from '../../campaign/tech';
import type { Game } from '../../campaign/types';
import type { Rng } from '../../core/rng';

/** When the battle is fought, on the campaign's calendar (month 0 is January). */
export interface When { year: number; month: number }

/** The date as a year with its fraction: September 1939 is 1939.67. */
const at = (w: When): number => w.year + w.month / 12;

const COMMONWEALTH = new Set(['UK', 'CAN', 'AUS', 'NZL', 'SAF', 'IND']);

/**
 * The year a nation's own designers have reached for its fighters or bombers: the campaign bridge's 'designs'
 * (unreal/TheaterCommand/Rules/campaign-bridge.ts), which the 3D game reads to pick its models. Read here without
 * touching the nation's research (techOf opens it), so building a ticket never changes the game.
 */
export function designYear(g: Game, nat: string, branch: 'fighters' | 'bombers'): number {
  const year = 1939 + Math.floor((Math.max(1, g.turn) + 7) / 12);
  const n = g.nations[nat];
  if (n && techRules(g)) {
    const done = n.tech?.done ?? TECH.start[nat] ?? [];
    if (branch === 'fighters') return done.includes('escorts') ? Math.max(year, 1943) : Math.min(year, 1941);
    return done.includes('heavybombers') ? Math.max(year, 1942) : Math.min(year, 1941);
  }
  if (n?.lab) return 1939 + Math.floor((Math.max(1, g.turn + Math.round(n.lab.clock[branch] - n.lab.months)) + 7) / 12);
  return year;
}

/** The fighter the 3D game draws for a nation (CampaignAircraft.cpp MainFighterOf), from its designers' year. */
export function fighterOf(nat: string, design: number): string {
  if (nat === 'GER') return design >= 1942 ? 'Fw190' : 'Bf109';
  if (COMMONWEALTH.has(nat)) return design >= 1941 ? 'Spitfire' : 'Hurricane';
  if (nat === 'USA') return design >= 1943 ? 'Mustang' : 'Warhawk';
  if (nat === 'SOV') return 'Yak';
  if (nat === 'JAP' || nat === 'JPN') return 'Zero';
  return `Fighter:${nat}`;
}

/** The bomber the 3D game draws for a nation (CampaignAircraft.cpp MainBomberOf; ruling 59: always a level bomber). */
export function bomberOf(nat: string, design: number): string {
  if (nat === 'USA') return 'B17';
  if (nat === 'GER') return 'He111';
  if (COMMONWEALTH.has(nat)) return design >= 1942 ? 'Lancaster' : 'Bomber:UK';
  return `Bomber:${nat}`;
}

/** Engines on a bomber model: the B-17 and the Lancaster have four; the He 111 and the plain bomber two. */
export const enginesOf = (model: string): number => (model === 'B17' || model === 'Lancaster' ? 4 : 2);

// ---- Fighters against fighters ---------------------------------------------------------------------------------

/**
 * How a nation's fighters fly together: a tight 'vic' of three, 'threes' (looser sections of three), fours in 'line'
 * astern, or 'fours' (two pairs, each pair a leader and his wingman, spread 180 to 250 m apart, D2).
 */
export type Form = 'vic' | 'threes' | 'line' | 'fours';
/** How they fight: 'dive' (a pass from height, fire, zoom away, come again) or 'turn' (into a turning fight). */
export type Style = 'dive' | 'turn';

/** A nation's fighter tactics in a given month (the design's table 1.2). */
export function fighterWay(nat: string, w: When): { form: Form; style: Style } {
  const d = at(w);
  // Pairs and fours diving from height, one pass and zoom away; never turning with a Hurricane, Spitfire or Hawk (D1, D2, D3, D22, D32).
  if (nat === 'GER') return { form: 'fours', style: 'dive' };
  // Tight vics until autumn 1940, then fours in line astern, finger fours from 1942 (D4, D6, D7, D32).
  if (COMMONWEALTH.has(nat)) return { form: d < 1940 + 8 / 12 ? 'vic' : d < 1942 ? 'line' : 'fours', style: 'turn' };
  // Pairs and fours; never turn with a Zero; boom and zoom (D15, D16, D17, D35, D36).
  if (nat === 'USA') return { form: 'fours', style: 'dive' };
  // Threes, low and turning, then pairs and fours from late 1942 (D12, D13, D14, D34).
  if (nat === 'SOV') return { form: d < 1942 + 10 / 12 ? 'threes' : 'fours', style: 'turn' };
  // Threes in a turning fight, fours from 1943 (D19, D20, FD12).
  if (nat === 'JAP' || nat === 'JPN') return { form: d < 1943 ? 'threes' : 'fours', style: 'turn' };
  // France 1939-40 (tight patrols of three, out-turns the Bf 109, D8, D9); Poland and Italy (FDm1, FDm2); everyone
  // else until the historian says otherwise.
  return { form: 'threes', style: 'turn' };
}

// ---- Fighters against bombers ----------------------------------------------------------------------------------

/**
 * The pass a fighter section makes on a bomber formation: 'headon' (from ahead and above, B21, B23), 'astern' (the
 * slashing pass from high astern, B15), 'quarter' (from the rear quarter, above or below), 'beam' (from the side, B2),
 * 'below' (creeping up from low astern under the belly, B4), 'close' (closing to about 90 m from astern in a line of
 * armoured fighters, 1944 on, B26).
 */
export type Pass = 'headon' | 'astern' | 'quarter' | 'beam' | 'below' | 'close';

/** Mostly, some, rarely (the design's weights 0.6, 0.3, 0.1, for the historian). */
const M = 0.6, S = 0.3, R = 0.1;

/** The passes an attacker makes on a bomber, with their weights (the design's second table in 1.2). */
export function passesOn(attacker: string, w: When, bomber: string, bomberNat: string): [Pass, number][] {
  const d = at(w);
  if (attacker === 'GER' && bomber === 'B17') {
    if (d < 1942 + 10.75 / 12) return [['astern', M], ['quarter', S], ['below', R]]; // to 23 November 1942 (B21, B22, FB9)
    if (d < 1943 + 3 / 12) return [['headon', M], ['quarter', S], ['astern', R]]; // the head-on months (B21, B23, FB8)
    if (d < 1944) return [['quarter', M], ['headon', S], ['astern', R]]; // mixed quarters, some head-on (B23)
    return [['quarter', 0.4], ['headon', S], ['close', S]]; // and the armoured Fw 190s from astern (B26)
  }
  if (attacker === 'GER' && bomber === 'Lancaster') return [['quarter', M], ['astern', S], ['below', R]]; // on the trailing vic (B5)
  // Fighter Command in 1940 was ordered to go in head-on; few managed it (B27).
  if (COMMONWEALTH.has(attacker) && bomberNat === 'GER' && d < 1942) return [['headon', S], ['beam', S], ['quarter', S], ['below', R]];
  if (attacker === 'JAP' || attacker === 'JPN') return [['headon', M], ['astern', R], ['beam', R], ['below', R]]; // mostly frontal (B28)
  if (attacker === 'SOV') return [['quarter', M], ['astern', S], ['below', R]]; // for the historian (B29, FBm7)
  if (bomber === 'B17' || bomber === 'Lancaster') return [['quarter', M], ['astern', S], ['headon', R]];
  // The 1939-41 twins, with a nose and a tail gun and little on the beam or below: from the beam and the quarter,
  // above and below; rarely head-on (B2, B4, B27; France's own passes are for the historian, FBm2).
  return [['beam', S], ['quarter', S], ['below', S], ['headon', R]];
}

/** One pass from the weights, by a throw from the battle's own stream for passes. */
export function pickPass(rng: Rng, rows: [Pass, number][]): Pass {
  const all = rows.reduce((s, [, k]) => s + k, 0);
  let x = rng.next() * all;
  for (const [p, k] of rows) { x -= k; if (x < 0) return p; }
  return rows[rows.length - 1][0];
}

// ---- Escorts ---------------------------------------------------------------------------------------------------

/**
 * The jobs an escort is given, in the order a nation's doctrine gave them out (the design's table 1.8; the order is
 * the historian's to confirm, ruling 237.2): 'close' weaves over its bombers, 'top' flies a height band above them,
 * 'sweep' clears the way ahead.
 */
export type Role = 'close' | 'top' | 'sweep';

export function escortRoles(nat: string, w: When): Role[] {
  const d = at(w);
  if (nat === 'GER') return d < 1940 + 8 / 12 ? ['close', 'sweep', 'top'] : ['close', 'top', 'sweep']; // E1, E2, E6, B33
  if (COMMONWEALTH.has(nat) && d >= 1941) return ['close', 'top', 'sweep']; // E3
  if (nat === 'USA') return d < 1944 ? ['close', 'top'] : ['sweep', 'top', 'close']; // E4, E13; E16, FE6
  if (nat === 'SOV' && d >= 1942 + 8 / 12) return ['close']; // E21
  if (nat === 'JAP' || nat === 'JPN') return ['close']; // E20
  return ['close', 'top', 'sweep'];
}
