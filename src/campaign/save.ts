// Saved games. A save is the starting seed plus everything that has changed since:
// who holds what, every corps, every nation's books, the news and the battle file,
// and the exact state of the random number stream, so a loaded game replays the
// same war the unsaved one would have.

import { Rng } from '../core/rng';
import { computeSupply, createGame } from './game';
import { seat } from './commanders';
import type { Corps, Game, General } from './types';

export const SAVE_VERSION = 1;
const KEEP_REPORTS = 200;
const KEEP_SEA = 80;

export function saveGame(g: Game): string {
  return JSON.stringify({
    v: SAVE_VERSION, seed: g.seed, human: g.human, turn: g.turn, phase: g.phase, week: g.week, order: g.order,
    nextCorps: g.nextCorps, nextBuild: g.nextBuild, flags: g.flags, rng: g.rng.state,
    dirty: g.supplyDirty, supply: Object.fromEntries(Object.entries(g.supply).map(([k, v]) => [k, Array.from(v).join('')])),
    nations: g.nations,
    // The order corps stand in within a region is kept too: battles and retreats take them in that order.
    regions: g.regions.map((r) => [r.owner, r.ctrl, r.cap ? 1 : 0, r.fort, r.since, r.prev, r.units.map((c) => c.id)]),
    corps: g.corps,
    generals: g.generals,
    fleets: g.fleets, nextFleet: g.nextFleet, seaReports: g.seaReports.slice(-KEEP_SEA), convoys: g.convoys,
    headlines: g.headlines,
    reports: g.reports.slice(-KEEP_REPORTS),
  });
}

/** A one-line description of a save, for the start screen, without loading the whole game. */
export function peekSave(text: string): { human: string | null; turn: number } | null {
  try {
    const d = JSON.parse(text);
    return d && d.v === SAVE_VERSION ? { human: d.human ?? null, turn: d.turn } : null;
  } catch { return null; }
}

export function loadGame(text: string): Game | null {
  let d: any;
  try { d = JSON.parse(text); } catch { return null; }
  if (!d || d.v !== SAVE_VERSION || !Array.isArray(d.regions)) return null;
  const g = createGame(d.seed, d.human ?? null);
  if (d.regions.length !== g.regions.length) return null; // saved on a different map
  g.turn = d.turn; g.phase = d.phase; g.week = d.week; g.order = d.order;
  g.nextCorps = d.nextCorps; g.nextBuild = d.nextBuild; g.flags = d.flags;
  g.rng = new Rng(d.rng);
  // A save from an older build may lack fields added since; the fresh game fills them in.
  for (const [id, n] of Object.entries(d.nations as Record<string, any>)) g.nations[id] = { ...g.nations[id], ...n };
  g.headlines = d.headlines;
  g.reports = d.reports;
  g.regions.forEach((r, i) => {
    const [owner, ctrl, cap, fort, since, prev] = d.regions[i];
    r.owner = owner; r.ctrl = ctrl; r.cap = !!cap; r.fort = fort; r.since = since; r.prev = prev ?? null; r.units = [];
  });
  g.corps = d.corps as Corps[];
  const byId = new Map(g.corps.map((c) => [c.id, c]));
  g.regions.forEach((r, i) => {
    const ids = d.regions[i][6] as number[] | undefined;
    if (ids) for (const id of ids) { const c = byId.get(id); if (c && c.reg === r.id) { r.units.push(c); byId.delete(id); } }
  });
  // Anything a save from an older build did not list goes in as it comes.
  for (const c of g.corps) if (byId.has(c.id)) g.regions[c.reg].units.push(c);
  if (Array.isArray(d.generals)) g.generals = d.generals as General[];
  else {
    // A save from before generals were people: each corps carried only a name and a label.
    g.generals = [];
    for (const c of g.corps as any[]) {
      const name = c.general, label = c.trait;
      c.general = null;
      delete c.trait;
      if (typeof name === 'string') seat(g, c, name, label);
    }
  }
  if (Array.isArray(d.fleets)) { g.fleets = d.fleets; g.nextFleet = d.nextFleet; g.seaReports = d.seaReports ?? []; g.convoys = d.convoys ?? {}; }
  // The supply picture is restored exactly as it stood, so the loaded game plays on identically.
  const fed = g.corps.map((c) => c.supplied);
  computeSupply(g);
  g.corps.forEach((c, i) => { c.supplied = fed[i]; });
  for (const [k, v] of Object.entries((d.supply ?? {}) as Record<string, string>)) g.supply[k] = Uint8Array.from(v, (ch) => (ch === '1' ? 1 : 0));
  g.supplyDirty = !!d.dirty;
  return g;
}
