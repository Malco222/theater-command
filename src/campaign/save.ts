// Saved games. A save is the starting seed plus everything that has changed since:
// who holds what, every corps, every nation's books, the news and the battle file,
// and the exact state of the random number stream, so a loaded game replays the
// same war the unsaved one would have.

import { Rng } from '../core/rng';
import { atWar, capitalDistances, computeSupply, createGame, openTheatres } from './game';
import { seat } from './commanders';
import { FIELDS } from './research';
import { newStories } from './stories';
import { openSmallAir, restoreWorks } from './arsenal';
import { resetRailheads } from './rails';
import { openAirWar, openWorldAir } from './air';
import { openCarriers } from './carriers';
import { openFleetRows } from './navy';
import { openSieges } from './siege';
import { FAIR } from './difficulty';
import { rollFromMemory } from './liberation';
import { openMorale } from './morale';
import { openTrade } from './trade';
import { numberDivisions } from './armies';
import mapData from '../generated/campaign-map.json';
import { WORLD_REGIONS, WORLD_STAGES, attachWorld, settleNewGround } from './world';
import { openWinter } from './winter';
import type { Corps, Game, General } from './types';

/** A save is laid over the map it was played on, region by region. */
const MAP_REGIONS = (mapData as any).regions.length as number;
/**
 * Places added on the end of the map by cutting up ground that was already there, and the region each was cut
 * from. A campaign saved before they existed can still be continued: each begins in the hands its parent is in.
 */
const CARVED: Record<string, string> = { Cassino: 'Abruzzo', Anzio: 'Rome' };
/**
 * True if a save with this many regions can be laid over today's map: every place it lacks was carved from one it
 * has. A campaign saved on the wider world (world.ts) has that world's provinces after Europe's: all of them, or
 * as many as the chart had at an earlier stage of its drawing.
 */
const fits = (saved: number, world = false): boolean => (world ? saved === MAP_REGIONS + WORLD_REGIONS || WORLD_STAGES.includes(saved - MAP_REGIONS) : saved === MAP_REGIONS
  || (saved < MAP_REGIONS && ((mapData as any).regions as { name: string }[]).slice(saved).every((r) => r.name in CARVED)));
/** Whether a save was made on the wider world. */
const wide = (d: any): boolean => d?.flags?.world === 1;

export const SAVE_VERSION = 1;
const KEEP_REPORTS = 200;
const KEEP_SEA = 80;
/**
 * The four American armies of the first day are called by their names alone (the UI director's ruling of 17 November
 * 2026, "one name to a thing": the bracket listed divisions that disagreed with the chips under it). A campaign saved
 * before carries the old names on the corps, in the news and in the battle reports; each is read under the new one.
 */
const ARMIES_RENAMED: [string, string][] = [
  ['First Army (1st Division)', 'First Army'], ['Second Army (5th and 6th Divisions)', 'Second Army'],
  ['Third Army (2nd Division and 1st Cavalry Division)', 'Third Army'], ['Fourth Army (3rd Division)', 'Fourth Army'],
];
export const armyWords = (text: string): string => ARMIES_RENAMED.reduce((t, [was, now]) => t.split(was).join(now), text);

export function saveGame(g: Game): string {
  return JSON.stringify({
    // A grand campaign is written in today's words: its air groups may be squadrons (ruling 220), and no old fleet
    // word is read into them on loading. Left out of a historical save, which is written exactly as it always was.
    v: SAVE_VERSION, words: g.flags.open === 1 ? 1 : undefined, seed: g.seed, human: g.human, turn: g.turn, phase: g.phase, week: g.week, order: g.order,
    nextCorps: g.nextCorps, nextBuild: g.nextBuild, flags: g.flags, rng: g.rng.state,
    dirty: g.supplyDirty,
    // The supply picture, in the order of the countries' names so that the same picture is always written the same way.
    supply: Object.fromEntries(Object.entries(g.supply).sort(([a], [b]) => (a < b ? -1 : a > b ? 1 : 0)).map(([k, v]) => [k, Array.from(v).join('')])),
    nations: g.nations,
    // The order corps stand in within a region is kept too: battles and retreats take them in that order. So is the
    // order of the corps inside its town, as an eighth entry where there is a garrison (Version 84.3): it decides who
    // defends a storm, who is turned out of a full town and what becomes of each corps when the town falls. A
    // province with nobody inside its town is written as it always was.
    regions: g.regions.map((r) => [r.owner, r.ctrl, r.cap ? 1 : 0, r.fort, r.since, r.prev, r.units.map((c) => c.id), ...(r.garr?.length ? [r.garr.map((c) => c.id)] : [])]),
    corps: g.corps,
    generals: g.generals,
    fleets: g.fleets, nextFleet: g.nextFleet, seaReports: g.seaReports.slice(-KEEP_SEA), convoys: g.convoys,
    // Coasts shelled from the sea, in the grand campaign: left out while there are none, so a historical save is written as it always was.
    shoreReports: g.shoreReports.length ? g.shoreReports.slice(-KEEP_SEA) : undefined,
    trade: g.trade.length ? g.trade : undefined,
    airFleets: g.airFleets, nextAir: g.nextAir, airReports: g.airReports.slice(-KEEP_SEA), airPool: g.airPool, sky: g.sky, raids: g.raids,
    stories: g.stories,
    // The secret service exists only in the grand campaign: a historical save is written exactly as it always was.
    intel: g.flags.open === 1 ? g.intel : undefined,
    // The roll of the fallen and the freed: left out while it is empty, so a save with nothing on it is written as it always was.
    passings: g.passings.length ? g.passings : undefined,
    // Corps at sea on a long passage: left out while there are none, so a save with none is written as it always was.
    atSea: g.atSea?.length ? g.atSea : undefined,
    // The air campaigns' memory (ruling 236.2): absent while the switch is off, so such a save is written as it always was.
    ac: g.ac,
    // Scarred ground (release 1.5's looks): the grand campaign's own; a historical save is written as it always was,
    // and is given its scars back from its reports when loaded.
    scars: g.flags.open === 1 ? g.scars : undefined,
    captured: g.flags.open === 1 ? g.captured : undefined,
    headlines: g.headlines,
    reports: g.reports.slice(-KEEP_REPORTS),
  });
}

/** A one-line description of a save, for the start screen, without loading the whole game. */
export function peekSave(text: string): { human: string | null; turn: number; open: boolean; level: number | null } | null {
  try {
    const d = JSON.parse(text);
    return d && d.v === SAVE_VERSION && Array.isArray(d.regions) && fits(d.regions.length, wide(d)) ? { human: d.human ?? null, turn: d.turn, open: d.flags?.open === 1, level: d.flags?.open === 1 ? d.flags?.level ?? FAIR : null } : null;
  } catch { return null; }
}

/** True for a campaign saved on an earlier, smaller map: it reads as a save but cannot be continued on this one. */
export function staleSave(text: string): boolean {
  try {
    const d = JSON.parse(text);
    return !!d && d.v === SAVE_VERSION && Array.isArray(d.regions) && !fits(d.regions.length, wide(d));
  } catch { return false; }
}

/**
 * A campaign saved before every group of ships was called a fleet (the player's ask of 17 November 2026: "ships
 * should be called fleets"): "Atlantic Squadron" reads "Atlantic Fleet", an escort group an escort fleet, wherever
 * the save wrote the name: on the fleet, on its carrier wings, in the news and in the battle reports. An air group's
 * own name ("1st Air Squadron") stands as it is. Names only: nothing else in the save is touched. A save written in
 * today's words (marked "words") takes none of these: America's "1st Pursuit Squadron" is an air group (ruling 220).
 * Version 84.3 (the design director's rider of 17 November): the five fleets whose names did not say ships take the
 * word as well, in every save. Germany's fleet in the Baltic is named for its port, since the Soviet Baltic Fleet
 * shares that sea.
 */
const FLEETS_RENAMED: [RegExp, string][] = [
  [/\bWestern Approaches Command\b/g, 'Western Approaches Fleet'], [/\bDover Command\b/g, 'Dover Fleet'], [/\bU-boat Arm\b/g, 'U-boat Fleet'],
  [/\bBaltic Station\b/g, 'Swinemunde Fleet'], [/\bSubmarine Command\b/g, 'Submarine Fleet'],
];
const OLD_FLEET_WORDS = /Squadron|Escort Group|Submarine Flotilla|Western Approaches Command|Dover Command|U-boat Arm|Baltic Station|Submarine Command/;
export const fleetWords = (text: string, today = false): string => {
  let t = today ? text : text.replace(/( Air)? Squadron\b/g, (m, air) => (air ? m : ' Fleet')).replace(/ Escort Group\b/g, ' Escort Fleet').replace(/ Submarine Flotilla\b/g, ' Submarine Fleet');
  for (const [was, now] of FLEETS_RENAMED) t = t.replace(was, now);
  return t;
};

export function loadGame(text: string): Game | null {
  if (text.includes(' Army (')) text = armyWords(text);
  let d: any;
  const today = text.startsWith(`{"v":${SAVE_VERSION},"words":1,`);
  try { d = JSON.parse(OLD_FLEET_WORDS.test(text) ? fleetWords(text, today) : text); } catch { return null; }
  if (!d || d.v !== SAVE_VERSION || !Array.isArray(d.regions)) return null;
  if (!fits(d.regions.length, wide(d))) return null; // saved on a different map
  const g = createGame(d.seed, d.human ?? null);
  // A campaign saved on the wider world is laid over that world: its provinces, seas and countries, and nothing
  // of its first day, which the save brings itself.
  if (wide(d)) attachWorld(g, false);
  // A place carved out since the save was made starts as the ground it was cut from stands: same owner, same holder, same works.
  const saved: number = d.regions.length;
  const grown = saved < g.regions.length;
  for (let i = saved; i < g.regions.length; i++) {
    const r = g.regions[i];
    // A province of the wider world drawn since the save was made begins as it stood on the first day.
    if (!(r.name in CARVED)) { d.regions.push([r.owner, r.ctrl, r.cap ? 1 : 0, r.fort, r.since, null, []]); continue; }
    const [owner, ctrl, , fort, since, prev] = d.regions[g.byName[CARVED[r.name]].id];
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
  // The distances to each capital are counted from the capitals the save brings (the fresh game counted them from the
  // first day's): Vichy's, set at the armistice, is the one that moves.
  for (const n of Object.values(g.nations)) if (n.capital >= 0) g.capDist[n.id] = capitalDistances(g, n.capital);
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
    delete r.garr;
  });
  g.corps = d.corps as Corps[];
  const byId = new Map(g.corps.map((c) => [c.id, c]));
  g.regions.forEach((r, i) => {
    const ids = d.regions[i][6] as number[] | undefined;
    if (ids) for (const id of ids) { const c = byId.get(id); if (c && c.reg === r.id && !c.town) { r.units.push(c); byId.delete(id); } }
    // The corps inside the town, in the order they stood there (a save from Version 84.3 on).
    const inside = d.regions[i][7] as number[] | undefined;
    if (inside) for (const id of inside) { const c = byId.get(id); if (c && c.reg === r.id && c.town) { (r.garr ??= []).push(c); byId.delete(id); } }
  });
  // Anything a save from an older build did not list goes in as it comes. A corps in garrison that such a save
  // did not list goes back inside its town in the order of the corps' roll.
  for (const c of g.corps) if (byId.has(c.id)) { const r = g.regions[c.reg]; if (c.town) (r.garr ??= []).push(c); else r.units.push(c); }
  // A carved place that finds itself in the front line is not left empty: if the ground it was cut from has two
  // or more corps free to move, the first of them takes post there.
  for (let i = saved; i < g.regions.length; i++) {
    const r = g.regions[i], parent = g.byName[CARVED[r.name]];
    if (!parent) continue;
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
  // And one saved before the air commands beyond the old chart were listed (America's): they are raised on their fields.
  if (g.flags.open === 1) openWorldAir(g);
  // And one saved before the aircraft from abroad (ruling 213(d)): the rows due before the month it was saved in came
  // with the order of battle it already has, so they are marked as come and not delivered all at once.
  if (g.flags.open === 1 && g.flags.gifts === undefined) openSmallAir(g);
  // A grand campaign saved before morale was kept, or before divisions carried numbers, picks both up here.
  openMorale(g);
  numberDivisions(g);
  // A save from before the units kept records: the roll of honour starts empty and fills from here on.
  g.stories = { ...newStories(), ...(d.stories ?? {}) };
  // A save from before spies: the service starts with nobody on its books.
  g.intel = { agents: [], next: 1, notes: [], heat: {}, ...(d.intel ?? {}) };
  // A save from before the roll of the fallen was kept: it is written up from what each nation remembers of its own fall.
  g.passings = Array.isArray(d.passings) ? d.passings : rollFromMemory(g);
  // Corps at sea on a long passage (ruling 229, part 1): none in a save from before the Cape was sailed.
  if (Array.isArray(d.atSea) && d.atSea.length) g.atSea = d.atSea;
  if (d.ac && typeof d.ac === 'object') g.ac = d.ac;
  // Scarred ground: as saved, or for a save without it (release 1's own), from the battles its reports still hold.
  if (d.captured && typeof d.captured === 'object') g.captured = d.captured;
  if (d.scars && typeof d.scars === 'object') g.scars = d.scars;
  else
    for (const r of g.reports ?? []) {
      if (r.yielded) continue;
      const was = (g.scars ??= {})[r.region];
      g.scars[r.region] = { n: (was?.n ?? 0) + 1, last: Math.max(was?.last ?? r.turn, r.turn) };
    }
  if (Array.isArray(d.fleets)) { g.fleets = d.fleets; g.nextFleet = d.nextFleet; g.seaReports = d.seaReports ?? []; g.shoreReports = d.shoreReports ?? []; g.convoys = d.convoys ?? {}; }
  // A grand campaign saved before carriers had aircraft of their own: every carrier fleet is given its wings.
  if (g.flags.open === 1) openCarriers(g);
  // And one saved before the fleet rows had the grand campaign's own fields: Force H takes its beat on the Western
  // Mediterranean, once, while it still stands as the history row had it (ruling 233 rider b564).
  if (g.flags.open === 1) openFleetRows(g);
  // And one saved before towns were garrisoned and besieged goes on under those rules.
  openSieges(g);
  // A campaign saved on an earlier stage of the wider chart: the ground drawn since stands as on the first day,
  // with the garrisons and squadrons that belong on it.
  if (wide(d) && grown) settleNewGround(g, saved);
  // A grand campaign saved before the Winter War had its place (ruling 228, 1b): the Mannerheim Line is laid as a new
  // campaign lays it, and one saved after the war's months is marked late (winter.ts, openWinter).
  if (g.flags.open === 1) openWinter(g, true);
  // Trade agreements; a grand campaign saved before there were any signs history's when it is loaded.
  g.trade = Array.isArray(d.trade) ? d.trade : [];
  openTrade(g);
  // The supply picture is restored exactly as it stood, so the loaded game plays on identically.
  const fed = g.corps.map((c) => c.supplied);
  // Drawing the picture marks the month a fortified port overseas last filled its stores: those marks are the save's own.
  const stores = new Map(Object.entries(g.flags).filter(([k]) => k.startsWith('stores:')));
  computeSupply(g);
  for (const k of Object.keys(g.flags)) if (k.startsWith('stores:')) { if (stores.has(k)) g.flags[k] = stores.get(k)!; else delete g.flags[k]; }
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
