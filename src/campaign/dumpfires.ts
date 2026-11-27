// Supply dumps that burn (release 2).
//
// Brian, 27 November 2026 13:40Z, yes to the playtester's idea: "a raid on a supply dump shows it burning on the campaign
// map, and the army nearby moves slower the next month". Forward dumps (logistics.ts) already lose part of their stock to
// the enemy aircraft over them each month, by those aircraft's pressure there. Under this switch:
//
// 1. A dump that loses at least 'least' division-months so is set burning (logistics.ts tendDumps books it). No die is
//    thrown: it is the aircraft over it that burn it.
// 2. It smokes on the campaign map that month and the next, as the bombed towns do (campaign-bridge.ts 'bombed').
// 3. In the month that follows the bombing, every corps of its side standing within the dump's reach on its own side's
//    ground has 'slow' fewer moves (game.ts startPhase, beside the fuel shortage's own): the fuel and shells it was to
//    draw went up in the fire.
// 4. Every nation alike, the player's and the computers'.
//
// With the switch off nothing here is read and nothing is written. It does nothing where the dumps are off.

import data from '../../data/campaign/dumpfires.json';
import { dumpRoads } from './logistics';
import type { Game } from './types';

export const FIRES = data as { least: number; slow: number; smoke: number };

/** One dump set burning: whose, where, the month, the division-months that went up, and 1 where the player's aircraft were among those over it. */
export interface DumpFire { nat: string; region: number; turn: number; burnt: number; mine?: 1 }

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const dumpFiresOn = (g: Game): boolean => g.flags.dumpfires === 1;

/** Turns the switch on or off at any time. Off puts every fire out. */
export function setDumpFires(g: Game, on: boolean): boolean {
  if (on) { g.flags.dumpfires = 1; return true; }
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

/** The provinces where a nation's corps move slower this month: within reach of its dumps set burning this month. */
export function slowedByFire(g: Game, nat: string): Set<number> | null {
  if (!dumpFiresOn(g) || !g.dumpFires) return null;
  const out = new Set<number>();
  for (const f of g.dumpFires) {
    if (f.nat !== nat || f.turn !== g.turn) continue;
    const road = dumpRoads(g, nat, f.region);
    road.forEach((d, i) => { if (d >= 0) out.add(i); });
  }
  return out.size ? out : null;
}

/** Whether a nation's dump at a province is burning (this month or last), for the map. */
export const burningAt = (g: Game, nat: string, region: number): boolean =>
  dumpFiresOn(g) && !!g.dumpFires?.some((f) => f.nat === nat && f.region === region && f.turn >= g.turn - 1);
