// The air version's open pieces (the design director's b482, item 3), each behind its own switch in
// data/campaign/airversion.json and kept with each campaign in g.flags. The grand campaign only.
//
//   night guns    ruling 107(b): by night the guns round a target bring down half as many bombers as by day (the
//                 standing flak is already halved by night); the bombing they stop is the same
//   start guns    ruling 124: every great power opens with guns round its capital and its two home provinces with
//                 the most factories, level 1 in a start of 1939 or 1940 and level 2 later, player and computer alike
//   the field     ruling 151(b), WRECK THE AIRFIELD: an agent's act of sabotage grounds every air group based on the
//                 field for the month; the other side's services can do the same to the player
//
// With the switches off nothing here runs: every hook elsewhere asks first. The hooks are one line each: air.ts
// (the night guns in assessAir; a wrecked field in whyNotMission, assessAir, fightersOver, airOverBattle), staff.ts
// (the switches and the start guns, as a campaign opens), intel.ts (the agent's mission and the enemy's plot).

import data from '../../data/campaign/airversion.json';
import { turnToDate } from '../core/ticket';
import type { AirFleet, Game } from './types';
import { damageField, fieldRules, fieldShut } from './fieldstate';
import { fieldWithered } from './wither';

export const AIRV = data as unknown as {
  nightGuns: { on: boolean; share: number };
  startGuns: { on: boolean; early: number; late: number; earlyTo: number; provinces: number };
  field: { on: boolean; enemyShare: number };
};

/** The three switches, as a campaign keeps them. */
export type AirPiece = 'night' | 'start' | 'field';
const KEY: Record<AirPiece, string> = { night: 'av:night', start: 'av:start', field: 'av:field' };

/** True in a grand campaign begun with this piece switched on. */
export const airPiece = (g: Game, p: AirPiece): boolean => g.flags.open === 1 && g.flags[KEY[p]] === 1;

/** For tests and probes: a piece on or off in this campaign (the start guns are laid only as a campaign opens). */
export function setAirPiece(g: Game, p: AirPiece, on: boolean): void {
  if (on) g.flags[KEY[p]] = 1;
  else delete g.flags[KEY[p]];
}

// ---- Night guns (ruling 107(b)) -----------------------------------------------------------------------------

/** What the guns round a target are worth against the bombers by night: 'share' with the switch on, else as by day. */
export const nightGunShare = (g: Game, night: boolean): number => (night && airPiece(g, 'night') ? AIRV.nightGuns.share : 1);

// ---- Start guns (ruling 124) --------------------------------------------------------------------------------

/** The provinces a great power's start guns ring: its capital, then its home provinces with the most factories. */
export function startGunSites(g: Game, nat: string): number[] {
  const n = g.nations[nat];
  if (!n?.alive) return [];
  const out: number[] = [];
  const cap = g.regions[n.capital];
  if (cap && cap.owner === nat && cap.ctrl === nat) out.push(cap.id);
  const homes = g.regions.filter((r) => r.owner === nat && r.ctrl === nat && r.fac > 0 && r.id !== n.capital).sort((a, b) => b.fac - a.fac || a.id - b.id);
  for (const r of homes.slice(0, AIRV.startGuns.provinces)) out.push(r.id);
  return out;
}

/** The level of the start guns for a campaign begun in this month. */
export const startGunLevel = (turn: number): number => (turnToDate(turn).year <= AIRV.startGuns.earlyTo ? AIRV.startGuns.early : AIRV.startGuns.late);

/**
 * Sets the switches for a grand campaign as it opens, and lays the start guns: every great power (the list is the
 * caller's: diplomacy's majors on the map in play), player and computer alike.
 */
export function openAirVersion(g: Game, majors: string[]): void {
  if (AIRV.nightGuns.on) g.flags[KEY.night] = 1;
  if (AIRV.field.on) g.flags[KEY.field] = 1;
  if (!AIRV.startGuns.on) return;
  g.flags[KEY.start] = 1;
  layStartGuns(g, majors);
}

/**
 * A grand campaign saved before the start guns were on (1.5.1; Brian, 24 November 2026 22:31Z, on the art director's
 * card: "Yes, when I load a saved war") lays them once as it is loaded, for every great power, the player's and the
 * computer's alike: the guns it would have opened with, at the level of the month the war began (openFrom, or the
 * first month), round the capital and chief factory provinces each still owns and holds as it is loaded.
 * A province that already has guns keeps the higher level. True when it laid them.
 */
export function startGunsOnLoad(g: Game, majors: string[]): boolean {
  if (!AIRV.startGuns.on || g.flags.open !== 1 || g.flags[KEY.start] === 1) return false;
  g.flags[KEY.start] = 1;
  layStartGuns(g, majors, g.flags.openFrom ?? 1);
  return true;
}

/** The start guns themselves (ruling 124), for openAirVersion, startGunsOnLoad and tests: at the level of the month
 * the war began (the month in play, unless told). A province keeps the higher level. */
export function layStartGuns(g: Game, majors: string[], began: number = g.turn): void {
  const lv = startGunLevel(began);
  for (const nat of majors) for (const id of startGunSites(g, nat)) g.flags[`wk:aa:${id}`] = Math.max(g.flags[`wk:aa:${id}`] ?? 0, lv);
}

// ---- The field wrecked (ruling 151(b)) ----------------------------------------------------------------------

/**
 * True for a province whose airfield saboteurs wrecked this month. With airfields as targets (fieldstate.ts) the one
 * field state answers instead: its runway shut, by anything.
 */
export const fieldWrecked = (g: Game, region: number): boolean => (g.flags.fields === 1 && fieldRules(g) ? fieldShut(g, region) : g.flags[`wreck:${region}`] === g.turn);

/**
 * True for an air group grounded because its field cannot be flown from (never a carrier's wing): release 1, saboteurs
 * wrecked it this month; with airfields as targets, its runway is shut until repaired (fieldstate.ts fieldShut). And a
 * group on its way to another airfield by stages (release 2, ferry.ts): it is out of the fight until it gets there, so
 * it meets no raid, flies none, and lends no land battle its aircraft. Only the switch 'ferry' writes `way`.
 * (The whole-world chart, ruling 173(e) with rider (o)(2): and a group whose field stands on a cut-off island of the
 * historian's table and has gone unfed. It flies nothing and meets nothing until the island is fed: wither.ts.)
 */
export const wrecked = (g: Game, f: AirFleet): boolean => f.ship === undefined && (f.way !== undefined || fieldShut(g, f.base) || fieldWithered(g, f));

/** Why a group cannot fly for a wrecked or shut field, or null. */
export function whyNotField(g: Game, f: AirFleet): string | null {
  if (!wrecked(g, f)) return null;
  if (f.way !== undefined) return `On its way to ${g.regions[f.way.to]?.name ?? 'another airfield'}: out of the fight until it gets there.`;
  if (!fieldShut(g, f.base)) return `Grounded. ${g.regions[f.base].name} is cut off, and no fuel or bombs reach its field. It flies again once they reach the island by sea.`;
  return g.flags.fields === 1 && fieldRules(g) ? `Grounded: the runway at ${g.regions[f.base].name} is cratered until it is repaired.` : `Grounded this month: saboteurs wrecked the field at ${g.regions[f.base].name}.`;
}

/** The air groups of a side based on a province's field: what wrecking it would ground. */
export const groupsOnField = (g: Game, region: number, nat: (n: string) => boolean): AirFleet[] =>
  g.airFleets.filter((f) => f.base === region && f.ship === undefined && f.ftr + f.bmr > 0 && nat(f.nat));

/** Wrecks a province's field for this month. False if it is already wrecked or nothing is based there. */
export function wreckField(g: Game, region: number, nat: (n: string) => boolean): boolean {
  if (fieldWrecked(g, region) || !groupsOnField(g, region, nat).length) return false;
  // With airfields as targets the wreck writes the one field state, as the light hit (fieldstate.ts damageField 'agent').
  if (g.flags.fields === 1 && fieldRules(g)) return damageField(g, region, 'agent') !== null;
  g.flags[`wreck:${region}`] = g.turn;
  return true;
}
