// Damaged cities (Brian, 20 November 2026): artillery, bomb runs and the fighting in a province leave its chief town
// damaged. Its factories, its land, its harbour, the seat of government there and its town buildings pay that much
// less until the holder pays to repair it (repairs.ts). The grand campaign only, behind its switch: a historical
// scenario, and a grand campaign started before the switch, are played exactly as they always were.
//
// This file keeps the state and the amounts and nothing else, so that the money, the air war, the sieges and the
// battles can all read it without importing one another. The numbers live in data/campaign/damage.json.

import data from '../../data/campaign/damage.json';
import type { Game, Region } from './types';

export const DAMAGE = data as unknown as {
  most: number;
  raid: { full: number; points: number; night: number };
  town: { points: number };
  shell: { points: number; cityPoints: number; cityFull: number };
  guns: { perDiv: number; most: number };
  battle: { field: number; streets: number; storm: number; repulsed: number; guns: number; gunsMost: number; full: number };
  repair: { months: number; least: number };
  ai: { share: number; reserve: number; quiet: number };
  worth: number;
};

/** True where towns take damage and are repaired: a grand campaign with the switch on. */
export const damageRules = (g: Game): boolean => g.flags.open === 1 && g.flags.damage === 1;

/** Turns damaged cities on for a grand campaign (the interfaces do it when a new one begins). Nothing outside one. */
export function openDamage(g: Game, on = true): void {
  if (g.flags.open !== 1) return;
  if (on) g.flags.damage = 1; else delete g.flags.damage;
}

// The flags: 'dmg:<id>' the share of the town in ruins, in percent to a tenth (absent while it stands whole);
// 'dmgt:<id>' the month it was last hit; 'dmg:any' once any town has been hit, so that a war with none pays nothing
// to ask. (Hot paths: every province's pay is asked many times a month, so the key strings are kept.)
const KEYS: string[] = [];
const HIT: string[] = [];
export const damageKey = (id: number): string => (KEYS[id] ??= `dmg:${id}`);
const hitKey = (id: number): string => (HIT[id] ??= `dmgt:${id}`);

/** The share of a province's town that lies in ruins, 0 to DAMAGE.most / 100. Always 0 outside the rules. */
export function damageOf(g: Game, r: Region): number {
  if (!g.flags['dmg:any'] || !damageRules(g)) return 0;
  const p = g.flags[damageKey(r.id)];
  return p === undefined ? 0 : p / 100;
}

/** The share of a town's pay still coming in: 1 for a town standing whole. */
export const keepOf = (g: Game, r: Region): number => 1 - damageOf(g, r);

/** The month the town was last hit, or undefined if it never has been (or has been repaired since). */
export const lastHit = (g: Game, r: Region): number | undefined => g.flags[hitKey(r.id)];

/**
 * Leaves more of a town in ruins, by so many percent of the whole, up to DAMAGE.most. Returns the points actually
 * added (0 outside the rules, or for a town already as ruined as it can be).
 */
export function damageCity(g: Game, r: Region, pts: number): number {
  if (!damageRules(g) || !(pts > 0)) return 0;
  const was = g.flags[damageKey(r.id)] ?? 0;
  const now = Math.round(Math.min(DAMAGE.most, was + pts) * 10) / 10;
  if (now <= was) return 0;
  g.flags[damageKey(r.id)] = now;
  g.flags[hitKey(r.id)] = g.turn;
  g.flags['dmg:any'] = 1;
  return Math.round((now - was) * 10) / 10;
}

/** Sets a town's damage outright, in percent: 0 clears it. For repairs and their taking back. */
export function setDamage(g: Game, r: Region, pct: number): void {
  const p = Math.round(Math.min(DAMAGE.most, Math.max(0, pct)) * 10) / 10;
  if (p <= 0) { delete g.flags[damageKey(r.id)]; delete g.flags[hitKey(r.id)]; return; }
  g.flags[damageKey(r.id)] = p;
  g.flags['dmg:any'] = 1;
}

// ---- How much each kind of fire does ------------------------------------------------------------

const clamp01 = (x: number): number => (x <= 0 ? 0 : x >= 1 ? 1 : x);

/**
 * A raid on a province's industry: the bombers through as fly() counts them (after their accuracy), and the share of
 * them the anti-aircraft guns there stop. By night the bombs fall on the town at large.
 */
export const raidPoints = (through: number, stopped: number, night: boolean): number =>
  DAMAGE.raid.points * clamp01((through * (night ? DAMAGE.raid.night : 1) * (1 - clamp01(stopped))) / DAMAGE.raid.full);

/** Bombs on a besieged town: the hit's share of a full bombardment (siege.ts TownHit.share). */
export const townRaidPoints = (share: number): number => DAMAGE.town.points * clamp01(share);

/** Ships' guns on a besieged town's defences, by the hit's share of a full shelling. */
export const shellPoints = (share: number): number => DAMAGE.shell.points * clamp01(share);

/** Ships' guns on a city's works, by the weight of fire. */
export const shellCityPoints = (guns: number): number => DAMAGE.shell.cityPoints * clamp01(guns / DAMAGE.shell.cityFull);

/** A besieging army's artillery in a month, by its weight in divisions of guns. */
export const siegeGunPoints = (weight: number): number => Math.min(DAMAGE.guns.most, DAMAGE.guns.perDiv * Math.max(0, weight));

export type FightKind = 'field' | 'streets' | 'storm' | 'repulsed';

/** A battle in the province: its kind, the divisions that fought on both sides together, and the attackers' guns in divisions. */
export function battlePoints(kind: FightKind, divs: number, guns: number): number {
  const B = DAMAGE.battle;
  const size = Math.max(0.25, clamp01(divs / B.full));
  return (B[kind] + Math.min(B.gunsMost, B.guns * Math.max(0, guns))) * size;
}

/** One sentence for a report: what this hit did to the town and where it now stands. */
export function damageWords(name: string, added: number, now: number): string {
  const pct = (x: number): string => `${x < 1 ? '<1' : Math.round(x)}%`;
  return `${name} took ${pct(added)} more damage and is now ${pct(now)} in ruins: its factories and its pay are down by as much until it is repaired.`;
}
