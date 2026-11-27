// Supply dumps that burn (release 2).
//
// Brian, 27 November 2026 13:40Z, yes to the playtester's idea: "a raid on a supply dump shows it burning on the campaign
// map, and the army nearby moves slower the next month". Forward dumps (logistics.ts) already lose part of their stock to
// the enemy aircraft over them each month, by those aircraft's pressure there. Under this switch:
//
// 1. A dump that loses at least 'least' division-months so is set burning (logistics.ts tendDumps books it). No die is
//    thrown: it is the aircraft over it that burn it.
// 2. It smokes on the campaign map that month and the next, as the bombed towns do (campaign-bridge.ts 'bombed').
// 3. In the month that follows the bombing, the corps that draw their month's supply on that dump (columns.ts: the
//    nearest ready dump with stock for them, nearer than the railhead) have 'slow' fewer moves (game.ts startPhase,
//    beside the fuel shortage's own): the fuel and shells they were to draw went up in the fire. A corps that merely
//    stands within its reach, of either side, loses nothing (the design director's gate, 27 Nov 14:12Z, item 2).
// 4. Every nation alike, the player's and the computers'.
//
// With the switch off nothing here is read and nothing is written. It needs the forward dumps: the setter will not turn
// it on where they are off, and turning the dumps off puts it off (logistics.ts setLogistics), so it is never a dead
// switch. The dumps are set only in a war's first month, so it reaches new wars.

import data from '../../data/campaign/dumpfires.json';
import { dumpsOn } from './logistics';
import type { Game } from './types';

export const FIRES = data as { least: number; slow: number; smoke: number };

/**
 * One dump set burning: whose, where, the month, the division-months that went up, 1 where the player's aircraft were
 * among those over it, and the corps that drew their month's supply on it (by id), once the columns have gone out.
 */
export interface DumpFire { nat: string; region: number; turn: number; burnt: number; mine?: 1; fed?: number[] }

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const dumpFiresOn = (g: Game): boolean => g.flags.dumpfires === 1;

/** Turns the switch on or off at any time. Off puts every fire out. It will not go on where the forward dumps are off. */
export function setDumpFires(g: Game, on: boolean): boolean {
  if (on) {
    if (!dumpsOn(g)) return false;
    g.flags.dumpfires = 1;
    return true;
  }
  delete g.flags.dumpfires;
  delete g.dumpFires;
  return true;
}

/** Called by tendDumps as a nation's month opens: its fires older than last month are out. */
export function fadeFires(g: Game, nat: string): void {
  if (!g.dumpFires) return;
  g.dumpFires = g.dumpFires.filter((f) => f.nat !== nat || f.turn >= g.turn - 1);
  if (!g.dumpFires.length) delete g.dumpFires;
}

/** Books a dump set burning this month. */
export function bookFire(g: Game, fire: DumpFire): void {
  (g.dumpFires ??= []).push(fire);
}

/**
 * Called by the columns (columns.ts) as a nation's corps at province 'to' draw their month's supply on its dump at
 * 'region': where that dump was set burning this month, those corps are noted on the fire.
 */
export function noteFed(g: Game, nat: string, region: number, to: number): void {
  if (!dumpFiresOn(g) || !g.dumpFires) return;
  const fire = g.dumpFires.find((f) => f.nat === nat && f.region === region && f.turn === g.turn);
  if (!fire) return;
  const ids = g.corps.filter((c) => c.nat === nat && c.reg === to && !c.dead && c.kind !== 'fort' && c.supplied).map((c) => c.id);
  if (ids.length) fire.fed = [...(fire.fed ?? []), ...ids];
}

/** The corps (by id) of a nation that move slower this month: those that drew on its dumps set burning this month. */
export function slowedByFire(g: Game, nat: string): Set<number> | null {
  if (!dumpFiresOn(g) || !g.dumpFires) return null;
  const out = new Set<number>();
  for (const f of g.dumpFires) if (f.nat === nat && f.turn === g.turn) for (const id of f.fed ?? []) out.add(id);
  return out.size ? out : null;
}

/** Whether a nation's dump at a province is burning (this month or last), for the map. */
export const burningAt = (g: Game, nat: string, region: number): boolean =>
  dumpFiresOn(g) && !!g.dumpFires?.some((f) => f.nat === nat && f.region === region && f.turn >= g.turn - 1);
