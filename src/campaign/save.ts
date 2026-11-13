// Saved games. A save is the starting seed plus everything that has changed since:
// who holds what, every corps, every nation's books, the news and the battle file,
// and the exact state of the random number stream, so a loaded game replays the
// same war the unsaved one would have.

import { Rng } from '../core/rng';
import { atWar, computeSupply, createGame, openTheatres } from './game';
import { seat } from './commanders';
import { FIELDS } from './research';
import { newStories } from './stories';
import { restoreWorks } from './arsenal';
import { resetRailheads } from './rails';
import { openAirWar } from './air';
import { FAIR } from './difficulty';
import { rollFromMemory } from './liberation';
import mapData from '../generated/campaign-map.json';
import type { Corps, Game, General } from './types';

/** A save is laid over the map it was played on, region by region. */
const MAP_REGIONS = (mapData as any).regions.length as number;
/**
 * Places added on the end of the map by cutting up ground that was already there, and the region each was cut
 * from. A campaign saved before they existed can still be continued: each begins in the hands its parent is in.
 */
const CARVED: Record<string, string> = { Cassino: 'Abruzzo', Anzio: 'Rome' };
/** True if a save with this many regions can be laid over today's map: every place it lacks was carved from one it has. */
const fits = (saved: number): boolean => saved === MAP_REGIONS
  || (saved < MAP_REGIONS && ((mapData as any).regions as { name: string }[]).slice(saved).every((r) => r.name in CARVED));

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
    airFleets: g.airFleets, nextAir: g.nextAir, airReports: g.airReports.slice(-KEEP_SEA), airPool: g.airPool, sky: g.sky, raids: g.raids,
    stories: g.stories,
    // The secret service exists only in the grand campaign: a historical save is written exactly as it always was.
    intel: g.flags.open === 1 ? g.intel : undefined,
    // The roll of the fallen and the freed: left out while it is empty, so a save with nothing on it is written as it always was.
    passings: g.passings.length ? g.passings : undefined,
    headlines: g.headlines,
    reports: g.reports.slice(-KEEP_REPORTS),
  });
}

/** A one-line description of a save, for the start screen, without loading the whole game. */
export function peekSave(text: string): { human: string | null; turn: number; open: boolean; level: number | null } | null {
  try {
    const d = JSON.parse(text);
    return d && d.v === SAVE_VERSION && Array.isArray(d.regions) && fits(d.regions.length) ? { human: d.human ?? null, turn: d.turn, open: d.flags?.open === 1, level: d.flags?.open === 1 ? d.flags?.level ?? FAIR : null } : null;
  } catch { return null; }
}

/** True for a campaign saved on an earlier, smaller map: it reads as a save but cannot be continued on this one. */
export function staleSave(text: string): boolean {
  try {
    const d = JSON.parse(text);
    return !!d && d.v === SAVE_VERSION && Array.isArray(d.regions) && !fits(d.regions.length);
  } catch { return false; }
}

export function loadGame(text: string): Game | null {
  let d: any;
  try { d = JSON.parse(text); } catch { return null; }
  if (!d || d.v !== SAVE_VERSION || !Array.isArray(d.regions)) return null;
  if (!fits(d.regions.length)) return null; // saved on a different map
  const g = createGame(d.seed, d.human ?? null);
  // A place carved out since the save was made starts as the ground it was cut from stands: same owner, same holder, same works.
  const saved: number = d.regions.length;
  const grown = saved < g.regions.length;
  for (let i = saved; i < g.regions.length; i++) {
    const [owner, ctrl, , fort, since, prev] = d.regions[g.byName[CARVED[g.regions[i].name]].id];
    d.regions.push([owner, ctrl, 0, fort ? { ...fort, vs: [...(fort.vs ?? [])] } : fort, since, prev, []]);
  }
  // A save from before a country was added to the game: it takes its place in the order of play, and the month goes on from where it was.
  const order: string[] = [...d.order];
  let phase: number = d.phase;
  g.order.forEach((id, k) => {
    if (order.includes(id)) return;
    const at = k > 0 && order.includes(g.order[k - 1]) ? order.indexOf(g.order[k - 1]) + 1 : order.length;
    order.splice(at, 0, id);
    if (at <= phase) phase++;
  });
  g.turn = d.turn; g.phase = phase; g.week = d.week; g.order = order;
  g.nextCorps = d.nextCorps; g.nextBuild = d.nextBuild; g.flags = d.flags;
  g.rng = new Rng(d.rng);
  // What the calendar has changed on the map itself: theatres that have opened, and harbours built off open beaches.
  openTheatres(g);
  for (const k of Object.keys(g.flags)) if (k.startsWith('harbour:') && g.regions[+k.slice(8)]) g.regions[+k.slice(8)].port = true;
  // And what the player's treasury has built on it: the factories added since the game began.
  restoreWorks(g);
  // A save from an older build may lack fields added since; the fresh game fills them in.
  for (const [id, n] of Object.entries(d.nations as Record<string, any>)) g.nations[id] = { ...g.nations[id], ...n };
  // A save from before research: the design teams are taken to have kept to history's schedule so far.
  for (const [id, n] of Object.entries(d.nations as Record<string, any>)) {
    const lab = g.nations[id]?.lab;
    if (!lab || n.lab !== undefined) continue;
    const at = d.order.indexOf(id);
    lab.months = Math.max(0, at >= 0 && at <= d.phase ? d.turn : d.turn - 1);
    for (const f of FIELDS) lab.clock[f] = lab.months;
  }
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
  // A carved place that finds itself in the front line is not left empty: if the ground it was cut from has two
  // or more corps free to move, the first of them takes post there.
  for (let i = saved; i < g.regions.length; i++) {
    const r = g.regions[i], parent = g.byName[CARVED[r.name]];
    const free = parent.units.filter((c) => c.nat === r.ctrl && !c.hold && c.kind !== 'fort');
    if (free.length < 2 || !r.nb.some((j) => atWar(g, r.ctrl, g.regions[j].ctrl))) continue;
    parent.units.splice(parent.units.indexOf(free[0]), 1);
    free[0].reg = r.id;
    r.units.push(free[0]);
  }
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
  if (Array.isArray(d.airFleets)) { g.airFleets = d.airFleets; g.nextAir = d.nextAir; g.airReports = d.airReports ?? []; g.airPool = d.airPool ?? {}; g.sky = d.sky ?? {}; g.raids = d.raids ?? {}; }
  // A grand campaign saved before the air war became its own game: its airfields are placed and its fleets become air groups.
  if (g.flags.open === 1) openAirWar(g);
  // A save from before the units kept records: the roll of honour starts empty and fills from here on.
  g.stories = { ...newStories(), ...(d.stories ?? {}) };
  // A save from before spies: the service starts with nobody on its books.
  g.intel = { agents: [], next: 1, notes: [], heat: {}, ...(d.intel ?? {}) };
  // A save from before the roll of the fallen was kept: it is written up from what each nation remembers of its own fall.
  g.passings = Array.isArray(d.passings) ? d.passings : rollFromMemory(g);
  if (Array.isArray(d.fleets)) { g.fleets = d.fleets; g.nextFleet = d.nextFleet; g.seaReports = d.seaReports ?? []; g.convoys = d.convoys ?? {}; }
  // The supply picture is restored exactly as it stood, so the loaded game plays on identically.
  const fed = g.corps.map((c) => c.supplied);
  computeSupply(g);
  g.corps.forEach((c, i) => { c.supplied = fed[i]; });
  // On a map that has grown since, the saved picture is a region short: it is drawn afresh the first time it is asked for.
  if (!grown) for (const [k, v] of Object.entries((d.supply ?? {}) as Record<string, string>)) g.supply[k] = Uint8Array.from(v, (ch) => (ch === '2' ? 2 : ch === '1' ? 1 : 0));
  // The railheads are counted from the picture as it stood.
  resetRailheads(g);
  g.supplyDirty = grown || !!d.dirty;
  // A grand campaign saved before the railways were laid has no depots marked in its picture: it is drawn afresh.
  if (g.flags.open === 1 && !Object.values(g.supply).some((t) => t.includes(2))) g.supplyDirty = true;
  return g;
}
