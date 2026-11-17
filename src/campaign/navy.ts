// The war at sea: fleets, who holds which sea, naval battles, the convoy war, and armies
// carried across the water. Like the land war it is resolved from what the campaign map
// knows (ships by class, aircraft on the coasts, admirals, readiness), and every number
// lives in data/campaign/navy.json and data/campaign/seas.json.

import { byHand } from './depth';
import mapData from '../generated/campaign-map.json';
import worldMap from '../generated/world-map.json';
import data from '../../data/campaign/navy.json';
import fleetData from '../../data/campaign/fleets.json';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { AIR, CARRIERS, carrierRules, skyDenies, skyOver, skyReach } from './air';
import { capOver, deckWorth, syncWings } from './carriers';
import { COMMAND } from './commanders';
import { storyConvoy, storyEdge, storySea } from './stories';
import { techMul, techNotes } from './research';
import { whyBarred } from './ground';
import { allied, assess, atWar, capture, friendlyUnits, headline, hostileUnits, men, monthsUnfed, relocate, resolveBattle, routineNews, warning, type Assessment } from './game';
import { traitFleet, traitSeaLoss } from './traits';
import { SIEGE, arrive, besieged, besiegerOf, feeders, garrisonOf, roomIn, siegeRules } from './siege';
import { harbourOpen, prizeOut } from './prizes';
import { RES, fuelFleet, laneStopped } from './resources';
import {
  STACK, type BattleReport, type Corps, type Fleet, type Game, type Gate, type Region, type SeaReport, type SeaSide, type SeaZone, type ShipClass, type Ships, type Stance,
} from './types';

interface ClassSpec { label: string; one: string; many: string; power: number; tough: number; escort: number; raid: number }
export interface FleetRow { nat: string; name: string; from?: number; base: string | null; port: string; home: string; zone: string; duty: Stance; tied?: boolean; admiral?: string; ships: Partial<Ships> }
/** A convoy route. 'base' is a fortress on the route whose own submarines and aircraft hunt the convoys while an enemy holds it. */
interface Route { id: string; name: string; to: string; zones: string[]; share: number; tons: number; base?: { region: string; raid: number; starved: number } }

export const NAVY = data as unknown as {
  classes: Record<ShipClass, ClassSpec>;
  quality: Record<string, number>;
  air: { weight: number; safe: number; avoid: number; lossPer: number; maxLoss: number };
  closed: { min: number; ratio: number };
  battle: { seek: number; catchRaider: number; evadeAt: number; evade: number };
  ready: { port: number; sea: number; mend: number; floor: number; lost: number; won: number; sortieAt: number };
  intercept: { loss: number; sea: number };
  watch?: { corps: number; every: number };
  landing: { assault: number; surprise: number; range: number; sortie: number; airborne: number; craft?: [number, string[], number][] };
  range: number;
  admiral: { perLevel: number; maxLevel: number; lostWithFlagship: number };
  convoy: { escortWorth: number; ocean: number; air: number; patrolRaid: number; subBase: number; subHunt: number; escortLoss: number };
  convoys: Route[];
  ore: { to: string; source: string; share: number; iceMonths: number[]; winterPort: string; summerZones: string[] };
  fleets: FleetRow[];
  building: { nat: string; fleet: string; cls: ShipClass; from: number; to: number; perMonth: number }[];
  arrivals: [number, string, string, Partial<Ships>, string][];
};

/** The grand campaign's navy: escorts bound to lanes, destroyers that hunt, guns that shell a coast. See fleets.json. */
export const FLEETS = fleetData as unknown as {
  escort: { loose: number };
  hunt: { base: number; rate: number; sea: number; perSub: number; hunter: number; escort: number; loose: number; carrier: number; air: number; most: number; cost: number };
  screen: { per: number; risk: number; most: number };
  bombard: {
    guns: Ships; least: number; ready: number; minReady: number;
    troops: { scale: number; perDiv: number; max: number; str: number; dug: number; fort: number; floor: number };
    city: { scale: number; max: number };
    reply: number;
  };
  ai: { bombard: boolean; ready: number; sortie: number };
  roles: Record<ShipClass, string>;
  brief: Record<ShipClass, string>;
  orders: Record<Stance, { label: string; blurb: string }>;
};

/** True where fleets are handled as pieces: bound to lanes, sent to hunt, stood in to shell a coast. Only the grand campaign. */
export const seaWar = (g: Game): boolean => g.flags.open === 1;

export const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
/** The lines drawn on the chart between one sea and the next. */
export const SEA_BORDERS = (mapData as any).seas.borders as [number, number][][];
/** The same for the game in hand: on the wider world (world.ts) the oceans beyond the old chart have their lines too. */
export const seaBorders = (g: Game): [number, number][][] => (g.flags.world === 1 ? (worldMap as any).seas.borders : SEA_BORDERS);
const SURFACE: ShipClass[] = ['bb', 'cv', 'ca', 'dd'];
const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
export const quality = (g: Game, nat: string): number => (NAVY.quality[nat] ?? NAVY.quality._default) * techMul(g, nat, 'navy');
/** The sea has its own dice, so that a naval battle never changes how a land battle falls out. */
export const seaRng = (g: Game, key: string): Rng => new Rng(hashString(`sea:${g.seed}:${g.turn}:${key}`));
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);

export const zoneOf = (g: Game, key: string): number => g.seas.findIndex((z) => z.key === key);
export const shipCount = (s: Ships, list: ShipClass[] = CLASSES): number => list.reduce((n, c) => n + s[c], 0);
export const surfaceShips = (f: Fleet): number => shipCount(f.ships, SURFACE);
export const atSea = (f: Fleet): boolean => f.stance !== 'port';
export const fleetsOf = (g: Game, nat: string): Fleet[] => g.fleets.filter((f) => f.nat === nat);

/** A way cargoes travel by sea to one nation: a shipping lane of resources.json or a convoy route of navy.json. */
export interface SeaRoute { id: string; name: string; to: string; zones: number[] }
const routeCache = new WeakMap<SeaZone[], SeaRoute[]>();

/** Every route cargoes travel by sea, once each: what an escort can be bound to. */
export function seaRoutes(g: Game): SeaRoute[] {
  let rows = routeCache.get(g.seas);
  if (!rows) {
    rows = [];
    const add = (id: string, name: string, to: string, keys: string[]): void => {
      const zones = keys.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
      if (zones.length && !rows!.some((r) => r.id === id)) rows!.push({ id, name, to, zones });
    };
    for (const l of RES.lanes) add(l.id, l.name, l.to, l.zones);
    for (const c of NAVY.convoys) add(c.id, c.name, c.to, c.zones);
    routeCache.set(g.seas, rows);
  }
  return rows;
}

/** "2 cruisers and 7 destroyers". */
export function shipList(s: Ships): string {
  const parts = CLASSES.filter((c) => s[c] > 0).map((c) => `${s[c]} ${s[c] === 1 ? NAVY.classes[c].one : NAVY.classes[c].many}`);
  if (!parts.length) return 'no ships';
  return parts.length === 1 ? parts[0] : `${parts.slice(0, -1).join(', ')} and ${parts[parts.length - 1]}`;
}

let lookup: Int8Array | null = null;
let lookCols = 0;
let wide: Int8Array | null = null;

/** The same on the wider world: its chart is laid out in the same squares, from its own west edge, each row as runs of [sea, squares]. */
function seaAtWide(x: number, y: number): number {
  const W = worldMap as any as { frame: { x0: number; y0: number }; seas: { cell: number; cols: number; grid: number[][] } };
  const md = W.seas;
  if (!wide) {
    wide = new Int8Array(md.cols * md.grid.length).fill(-1);
    md.grid.forEach((row, r) => {
      let c = 0;
      for (let i = 0; i < row.length; i += 2) for (let k = row[i + 1]; k > 0 && c < md.cols; k--) wide![r * md.cols + c++] = row[i];
    });
  }
  const c = Math.floor((x - W.frame.x0) / md.cell), r = Math.floor((y - W.frame.y0) / md.cell);
  if (c < 0 || r < 0 || c >= md.cols || r >= md.grid.length) return -1;
  return wide[r * md.cols + c];
}

/** The sea under a point on the map, or -1 where it is land or off the chart. Given a game on the wider world, the point may lie anywhere on it. */
export function seaAt(x: number, y: number, g?: Game): number {
  if (g && g.flags.world === 1) return seaAtWide(x, y);
  const md = (mapData as any).seas as { cell: number; grid: string[] };
  if (!lookup) {
    lookCols = Math.ceil((mapData as any).width / md.cell);
    lookup = new Int8Array(lookCols * md.grid.length).fill(-1);
    md.grid.forEach((row, r) => {
      let c = 0;
      for (const m of row.matchAll(/([a-z.])(\d+)/g)) {
        const v = m[1] === '.' ? -1 : m[1].charCodeAt(0) - 97;
        for (let k = Number(m[2]); k > 0 && c < lookCols; k--) lookup![r * lookCols + c++] = v;
      }
    });
  }
  const c = Math.floor(x / md.cell), r = Math.floor(y / md.cell);
  if (c < 0 || r < 0 || c >= lookCols || r >= md.grid.length) return -1;
  return lookup[r * lookCols + c];
}

// ---- Setting up ---------------------------------------------------------------------------

/** Lays a chart's seas and straits into a game: the map of Europe's when it is made, the wider world's when that is attached (world.ts). */
export function chartSeas(g: Game, md: any): void {
  g.seas = md.zones.map((z: any, id: number) => ({ id, key: z.id, name: z.name, x: z.x, y: z.y, adj: z.adj, coast: z.coast, air: z.air ?? 1, narrow: !!z.narrow }));
  g.gates = md.gates.map((gt: any) => ({ a: gt.a, b: gt.b, name: gt.name, region: gt.region ? g.byName[gt.region]?.id ?? -1 : -1, nation: gt.nation ?? null, closed: !!gt.closed }));
}

export function setupNavy(g: Game): void {
  chartSeas(g, (mapData as any).seas);
  g.fleets = [];
  g.nextFleet = 1;
  g.seaReports = [];
  g.shoreReports = [];
  g.convoys = {};
  for (const row of NAVY.fleets) if ((row.from ?? 1) <= 1) addFleet(g, row);
}

export function addFleet(g: Game, row: FleetRow): Fleet {
  const f: Fleet = {
    id: g.nextFleet++, nat: row.nat, name: row.name, ships: { ...none(), ...row.ships }, lost: none(),
    base: row.base ? g.byName[row.base].id : -1, port: row.port, home: zoneOf(g, row.home), zone: zoneOf(g, row.home), stance: 'port',
    station: zoneOf(g, row.zone), duty: row.duty, tied: !!row.tied, ready: 1,
    admiral: row.admiral ? { name: row.admiral, level: 1, xp: 0 } : null, fought: 0,
  };
  g.fleets.push(f);
  return f;
}

/** A new squadron with no ships yet, in a home port: the grand campaign's arsenal fills it. */
export function foundFleet(g: Game, nat: string, r: Region, name: string): Fleet {
  const taken = (x: string): boolean => g.fleets.some((f) => f.name === x);
  let unique = name;
  for (let k = 2; taken(unique); k++) unique = `${name} ${k}`;
  const sea = r.seas[0];
  const f: Fleet = {
    id: g.nextFleet++, nat, name: unique, ships: none(), lost: none(), base: r.id, port: r.name, home: sea, zone: sea, stance: 'port',
    station: sea, duty: 'patrol', tied: false, ready: 1, admiral: null, fought: 0,
  };
  g.fleets.push(f);
  return f;
}

/** On the first day of the war the navies already at war are at their stations. */
export function openingStations(g: Game): void {
  for (const f of g.fleets) if (g.nations[f.nat].atWar.length && f.duty !== 'port') { f.zone = f.station; f.stance = f.duty; }
}

// ---- Who holds a sea ------------------------------------------------------------------------

/** How fit a fleet is to fight: its readiness, and in the grand campaign whether it has the fuel to steam. */
export const fit = (g: Game, f: Fleet): number => (0.5 + 0.5 * f.ready) * fuelFleet(g, f.nat);
const admiralMul = (f: Fleet): number => 1 + (f.admiral ? NAVY.admiral.perLevel * (f.admiral.level - 1) : 0);

/** A fleet's weight in a fleet action. */
export function fleetPower(g: Game, f: Fleet): number {
  let p = 0;
  // A carrier is worth what it can fly: one whose aircraft are spent counts for much less (the grand campaign).
  for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].power * (c === 'cv' ? deckWorth(g, f) : 1);
  return p * quality(g, f.nat) * fit(g, f) * admiralMul(f) * storyEdge(g, f) * traitFleet(g, f.nat);
}

/** Air strength over a sea from the coasts one side holds: this nation's friends, or (hostile) its enemies. */
export function airOver(g: Game, z: number, nat: string, hostile = false): number {
  const zone = g.seas[z];
  if (!zone.coast.length) return 0;
  let s = 0;
  for (const id of zone.coast) {
    const r = g.regions[id];
    if (r.since >= g.turn) continue; // airfields taken this month are not yet in use
    const n = g.nations[r.ctrl];
    if (!n || !n.alive || !n.atWar.length) continue;
    // An enemy who has won the sky over this coast flies from it in strength, and its owner barely at all.
    const sky = skyOver(g, id);
    if (hostile ? atWar(g, r.ctrl, nat) : allied(g, r.ctrl, nat)) s += n.air * (sky ? AIR.sky.seaFoe : 1);
    else if (sky && (hostile ? atWar(g, sky, nat) : allied(g, sky, nat))) s += g.nations[sky].air * AIR.sky.seaOwn;
  }
  return (s / zone.coast.length) * zone.air;
}

export interface SeaBalance { ships: number; air: number; total: number }

/** One side's strength in a sea: its fleets at sea there, and its aircraft within reach. */
export function seaSide(g: Game, z: number, nat: string, hostile: boolean): SeaBalance {
  let ships = 0;
  for (const f of g.fleets) if (f.zone === z && atSea(f) && (hostile ? atWar(g, f.nat, nat) : allied(g, f.nat, nat))) ships += fleetPower(g, f);
  const air = airOver(g, z, nat, hostile) * NAVY.air.weight;
  return { ships, air, total: ships + air };
}

/** A sea is closed to a nation's troopships and supply ships when the enemy is much the stronger there. */
export function closedTo(g: Game, z: number, nat: string): boolean {
  const e = seaSide(g, z, nat, true);
  if (e.total < NAVY.closed.min) return false;
  return e.total >= NAVY.closed.ratio * seaSide(g, z, nat, false).total;
}

/** The smallest strength of a nation's own side in a sea, in whole points, that opens it to its troopships; 0 while it is open. */
export function opensAt(g: Game, z: number, nat: string): number {
  if (!closedTo(g, z, nat)) return 0;
  // From the enemy's strength as the sentence prints it, so that the two figures agree; never less than the rule needs.
  const e = seaSide(g, z, nat, true).total;
  return Math.max(Math.floor(e / NAVY.closed.ratio), Math.floor(Math.round(e) / NAVY.closed.ratio)) + 1;
}

/**
 * A closed sea in a sentence (ruling 207): its name, each side's strength there as the rule counts it, the strength
 * that would open it, and the two things the rule counts that he can order: a fleet at sea in it, and the sky over
 * a coast of it the enemy holds (airOver: his aircraft fly from that coast at seaOwn, and the enemy's at seaFoe).
 */
export function closedLine(g: Game, z: number, nat: string): string {
  const e = seaSide(g, z, nat, true), own = seaSide(g, z, nat, false);
  const foeCoast = g.seas[z].coast.some((id) => { const n = g.nations[g.regions[id].ctrl]; return !!n && n.alive && atWar(g, g.regions[id].ctrl, nat) && skyOver(g, id) === null; });
  return `The ${g.seas[z].name} is closed to your troopships: the enemy has ${round(e.total)} there to your ${round(own.total)}, and ${opensAt(g, z, nat)} would open it. Put a fleet to sea there, or ${foeCoast && e.air >= 0.5 ? "win the sky over the enemy's coast on it" : "beat the enemy's fleets in it"}.`;
}

/** Whether a nation's ships may pass between two neighbouring seas: most are open water, a few are straits someone holds. */
export function canPass(g: Game, a: number, b: number, nat: string): boolean {
  const i = g.gates.findIndex((x) => (x.a === a && x.b === b) || (x.a === b && x.b === a));
  if (i < 0) return true;
  const gt = g.gates[i];
  const holder = gateHolder(g, gt);
  // Shut by its holder's order (ruling 176(e)): only his own ships and his allies' pass.
  if (g.flags[shutKey(i, holder)] !== undefined && !allied(g, holder, nat)) return false;
  if (gt.closed) return allied(g, holder, nat) || !g.nations[nat].atWar.length;
  return !atWar(g, holder, nat);
}

// ---- A gate its holder has shut (Version 84.1; ruling 176(e), on Brian's words of 2026-11-17: "you should be able
// to open or close the panama canal") ----
// The order is the holder's own, so it is kept under his name: the moment the gate changes hands it no longer
// holds, and gatesTick forgets it so that it does not come back with the gate. No staff gives it in release 1.
const shutKey = (i: number, holder: string): string => `gateshut:${i}:${holder}`;
/** The country that holds a gate. */
export const gateHolder = (g: Game, gt: Gate): string => (gt.region >= 0 ? g.regions[gt.region].ctrl : gt.nation!);
/** The month a gate's holder ordered it shut, or -1 while it stands as its own rule leaves it. */
export function gateShutSince(g: Game, i: number): number {
  const gt = g.gates[i];
  return gt ? g.flags[shutKey(i, gateHolder(g, gt))] ?? -1 : -1;
}
/** Shuts a gate in its holder's name, or opens it again. Ships, supply and trade find it so from that moment. */
export function setGate(g: Game, i: number, shut: boolean): void {
  const gt = g.gates[i];
  if (!gt) return;
  const key = shutKey(i, gateHolder(g, gt));
  if (!shut) delete g.flags[key];
  else if (g.flags[key] === undefined) g.flags[key] = g.turn;
  g.supplyDirty = true;
}
/** As a month opens: an order to shut a gate ends when the gate has changed hands, and does not come back with it. */
export function gatesTick(g: Game): void {
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('gateshut:')) continue;
    const [, i, nat] = k.split(':');
    const gt = g.gates[Number(i)];
    if (!gt || gateHolder(g, gt) !== nat) delete g.flags[k];
  }
}

/** The seas a nation's supply ships and troopships can use: those it can reach from a home port without crossing a sea closed to it. */
export function openSeas(g: Game, nat: string): Uint8Array {
  const open = new Uint8Array(g.seas.length);
  const shut = g.seas.map((z) => closedTo(g, z.id, nat));
  const q: number[] = [];
  const start = (z: number): void => { if (!shut[z] && !open[z]) { open[z] = 1; q.push(z); } };
  for (const r of g.regions) if (r.port && r.owner === nat && r.ctrl === nat) r.seas.forEach(start);
  for (const f of g.fleets) if (f.nat === nat && f.base < 0) start(f.home); // a base off the map is a home port too
  // The grand campaign: a harbour the nation has taken serves its supply ships as a home port does, once its own
  // roads reach the quay: the stores come down to it overland and go on by sea. So an army put across a narrow sea
  // from a conquered coast is fed from that coast, and not round by the harbours of the homeland.
  if (seaWar(g) && (g.nations[nat]?.capital ?? -1) >= 0 && g.regions[g.nations[nat].capital].ctrl === nat) {
    const linked = new Uint8Array(g.regions.length);
    const road = [g.nations[nat].capital];
    linked[road[0]] = 1;
    for (let i = 0; i < road.length; i++) for (const j of g.regions[road[i]].nb) if (!linked[j] && allied(g, nat, g.regions[j].ctrl)) { linked[j] = 1; road.push(j); }
    for (const r of g.regions) if (linked[r.id] && r.port && r.ctrl === nat && r.owner !== nat && prizeOut(g, r, 'harbour') <= 0) r.seas.forEach(start);
  }
  // So is the sea a country off the map sends its convoys in by.
  const off = g.nations[nat]?.offmap;
  if (off && g.nations[nat].alive && zoneOf(g, off.sea) >= 0) start(zoneOf(g, off.sea));
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (!open[j] && !shut[j] && canPass(g, q[i], j, nat)) { open[j] = 1; q.push(j); }
  return open;
}

/** True when ships can still reach this coast: an evacuation or a supply run needs at least one sea that is not closed. */
export const seaOpenAt = (g: Game, nat: string, r: Region): boolean => r.seas.some((z) => !closedTo(g, z, nat));

// ---- Armies at sea ----------------------------------------------------------------------------

/** How dangerous a sea is for a nation's troopships: 0 when no enemy ship or aircraft is at work there. */
export function seaRisk(g: Game, z: number, nat: string): number {
  const e = seaSide(g, z, nat, true);
  const hunters = e.ships + e.air * 0.5;
  if (hunters <= 0) return 0;
  return hunters / (hunters + 2 * seaSide(g, z, nat, false).total + NAVY.intercept.sea);
}

export function crossingRisk(g: Game, nat: string, path: number[]): number {
  let safe = 1;
  for (const z of path) safe *= 1 - seaRisk(g, z, nat);
  return 1 - safe;
}

/** The safest way by sea between two coasts, as the seas crossed in order, or null if there is none. */
export function seaPath(g: Game, nat: string, from: Region, to: Region, o: { max?: number; surprise?: boolean } = {}): number[] | null {
  if (!from.seas.length || !to.seas.length) return null;
  const N = g.seas.length;
  const cost = new Float64Array(N).fill(1e9), prev = new Int16Array(N).fill(-1), len = new Int16Array(N), done = new Uint8Array(N);
  const shut = (z: number): boolean => !o.surprise && closedTo(g, z, nat);
  const weight = (z: number): number => 1 + (o.surprise ? 0 : 20 * seaRisk(g, z, nat));
  for (const z of from.seas) if (!shut(z)) { cost[z] = weight(z); len[z] = 1; }
  for (;;) {
    let a = -1;
    for (let i = 0; i < N; i++) if (!done[i] && cost[i] < 1e9 && (a < 0 || cost[i] < cost[a])) a = i;
    if (a < 0) break;
    done[a] = 1;
    if (o.max && len[a] >= o.max) continue;
    for (const j of g.seas[a].adj) {
      if (done[j] || shut(j) || !canPass(g, a, j, nat)) continue;
      const c = cost[a] + weight(j);
      if (c < cost[j]) { cost[j] = c; prev[j] = a; len[j] = len[a] + 1; }
    }
  }
  let end = -1;
  for (const z of to.seas) if (cost[z] < 1e9 && (end < 0 || cost[z] < cost[end])) end = z;
  if (end < 0) return null;
  const path: number[] = [];
  for (let z = end; z >= 0; z = prev[z]) path.unshift(z);
  return path;
}

const liftKey = (g: Game, nat: string): string => `lift:${nat}:${g.turn}`;
/** How many more corps a nation's shipping can carry this month. */
export const liftLeft = (g: Game, nat: string): number => Math.max(0, g.nations[nat].sealift - (g.flags[liftKey(g, nat)] ?? 0));

/** Takes corps across the given seas and returns the men lost to enemy ships and aircraft on the way. */
function crossing(g: Game, cs: Corps[], path: number[], surprise: boolean, landingAt?: Region): number {
  if (surprise) return 0;
  const p = landingAt ? landingRisk(g, cs[0].nat, path, landingAt) : crossingRisk(g, cs[0].nat, path);
  if (p <= 0) return 0;
  let lost = 0;
  for (const c of cs) {
    const before = men(c);
    c.str *= 1 - NAVY.intercept.loss * p;
    c.org = clamp(c.org - 0.3 * p, 0, 1);
    lost += before - men(c);
  }
  return lost;
}

/** Why a corps cannot be shipped to a friendly port, or null if it can. */
export function whyNotShip(g: Game, c: Corps, to: Region): string | null {
  const n = g.nations[c.nat], from = g.regions[c.reg];
  if (n.sealift <= 0) return `${n.name} has no shipping to move an army by sea.`;
  if (c.kind === 'fort' || c.ap < Math.min(4, c.apMax)) return 'Only a corps that has not moved this month can embark.';
  if (!from.port || !to.port || to.id === from.id) return 'Troops are shipped from one port to another.';
  if (!allied(g, c.nat, to.ctrl)) return `${to.name} is not in friendly hands.`;
  // A harbour blocked and its quays blown up takes no ship until it is mended (prizes.ts).
  for (const p of [from, to]) { const out = prizeOut(g, p, 'harbour'); if (out > 0) return `The harbour of ${p.name} is wrecked: no ship will use it for ${out === 1 ? 'a month' : `${out} months`} more.`; }
  if (hostileUnits(g, to, c.nat).length) {
    if (siegeRules(g) && !harbourOpen(g, to)) return `The enemy holds the quays of ${to.name}: nothing can be run into the town by sea.`;
    // A town under siege with its harbour still open: ships can run more troops into the garrison, if it has room.
    if (!siegeRules(g)) return `${to.name} is not in friendly hands.`;
    if (c.kind === 'eng') return `The enemy holds the country round ${to.name}: only fighting troops are run into the town.`;
    if (garrisonOf(to).length >= roomIn(g, to)) return `${to.name} is under siege, and the town has no room for another corps.`;
  }
  const steep = whyBarred(g, c, to);
  if (steep) return steep;
  if (friendlyUnits(g, to, c.nat).length >= STACK) return `${to.name} has no room for another corps.`;
  if (liftLeft(g, c.nat) < 1) return 'All the shipping is in use this month.';
  if (!seaPath(g, c.nat, from, to)) return 'The enemy holds the sea between.';
  return null;
}

/** Ships a corps from one friendly port to another. It takes the whole month, and the enemy may find the convoy. */
export function shipCorps(g: Game, c: Corps, to: Region): boolean {
  if (whyNotShip(g, c, to)) return false;
  const path = seaPath(g, c.nat, g.regions[c.reg], to)!;
  const lost = crossing(g, [c], path, false);
  g.flags[liftKey(g, c.nat)] = (g.flags[liftKey(g, c.nat)] ?? 0) + 1;
  relocate(g, c, to);
  c.ap = 0;
  g.week = 2;
  if (lost >= 500) headline(g, 'battle', `${c.name} loses ${lost.toLocaleString('en-US')} men to enemy ships and aircraft on the crossing to ${to.name}.`, c.nat, to.id, { arm: 'sea', lost: c.nat });
  return true;
}

/** Why no sea lane serves a landing (ruling 207): a sea closed to the troopships, a coast too far, a strait shut, or waters that do not join. */
export type LaneBar = { kind: 'sea'; sea: number } | { kind: 'far'; seas: number; /** The dangerous sea the troopships go round, where that is why the way is too long. */ round?: number } | { kind: 'gate'; gate: number } | { kind: 'none' };

/**
 * The true one of the reasons a landing force has no lane from this port to that coast. It reads the rule and
 * changes nothing: the lane itself is seaPath's, as before. In order: a sea on the shortest crossing that is
 * closed to the troopships; a coast further off than a landing reaches; a strait shut to the nation's ships on
 * the way the waters themselves would give.
 */
export function laneBar(g: Game, nat: string, from: Region, to: Region, surprise = false): LaneBar {
  const range = NAVY.landing.range;
  const through = seaPath(g, nat, from, to, { max: range, surprise: true });
  if (through && !surprise) {
    const sea = through.find((z) => closedTo(g, z, nat));
    if (sea !== undefined) return { kind: 'sea', sea };
    // A way within reach with no sea shut, that the troopships still will not take: the safe way round is the longer
    // one. The sea they go round is the most dangerous on the short way.
    const worst = through.reduce((a, z) => (seaRisk(g, z, nat) > seaRisk(g, a, nat) ? z : a), through[0]);
    return { kind: 'far', seas: seaPath(g, nat, from, to)?.length ?? range + 1, ...(seaRisk(g, worst, nat) > 0 ? { round: worst } : {}) };
  }
  const long = seaPath(g, nat, from, to, { surprise: true });
  if (long) return { kind: 'far', seas: long.length };
  // The waters alone, every strait taken as open: the first strait on that way the nation's ships may not pass.
  const prev = new Int16Array(g.seas.length).fill(-2);
  const q: number[] = [];
  for (const z of from.seas) { prev[z] = -1; q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (prev[j] === -2) { prev[j] = q[i]; q.push(j); }
  const end = q.find((z) => to.seas.includes(z));
  if (end === undefined) return { kind: 'none' };
  const way: number[] = [];
  for (let z = end; z >= 0; z = prev[z]) way.unshift(z);
  for (let i = 1; i < way.length; i++) {
    if (canPass(g, way[i - 1], way[i], nat)) continue;
    const gate = g.gates.findIndex((x) => (x.a === way[i - 1] && x.b === way[i]) || (x.a === way[i] && x.b === way[i - 1]));
    if (gate >= 0) return { kind: 'gate', gate };
  }
  return { kind: 'none' };
}

/** The lane's refusal in words. (The page gives a shut strait its own sentence, with its holder: gates.ts.) */
function laneWhy(g: Game, nat: string, from: Region, to: Region, bar: LaneBar): string {
  if (bar.kind === 'sea') return closedLine(g, bar.sea, nat);
  if (bar.kind === 'far') return `${to.name} is too far for a landing from ${from.name}: troopships cross ${NAVY.landing.range} seas at most, and ${bar.round !== undefined ? `the safe way round the ${g.seas[bar.round].name} is longer` : bar.seas > NAVY.landing.range ? `this is ${bar.seas}` : 'the safe way round is longer'}.`;
  if (bar.kind === 'gate') return `The ${g.gates[bar.gate].name} ${/s$/.test(g.gates[bar.gate].name) ? 'are' : 'is'} shut to your ships, and no other way leads to ${to.name}.`;
  return `No sea joins ${from.name} to ${to.name}.`;
}

/** A landing made in the month war is declared catches the defenders unready and sails before any fleet can stop it. */
export const surpriseOn = (g: Game, nat: string, to: Region): boolean => g.flags[`war:${nat}:${to.ctrl}`] === g.turn;

export function whyNotLand(g: Game, cs: Corps[], to: Region): string | null {
  if (!cs.length) return 'No corps chosen.';
  const nat = cs[0].nat, n = g.nations[nat], from = g.regions[cs[0].reg];
  for (const c of cs) { const steep = whyBarred(g, c, to); if (steep) return steep; }
  if (airborne(cs)) {
    if (cs.some((c) => c.nat !== nat || c.reg !== from.id)) return 'An airborne force takes off from one place.';
    if (!allied(g, nat, from.ctrl)) return 'Airborne troops take off from friendly airfields.';
    if (cs.some((c) => c.ap < Math.min(4, c.apMax))) return 'Only corps that have not moved this month can emplane.';
    if (allied(g, nat, to.ctrl) || !atWar(g, nat, to.ctrl)) return `${to.name} is not enemy ground.`;
    if (!from.seas.some((z) => to.seas.includes(z))) return `${to.name} is out of reach: the transport aircraft can cross one sea, from a coast on it to another.`;
    if (g.flags[dropKey(g, nat)]) return 'The transport aircraft can carry one airborne assault a month.';
    if (skyDenies(g, nat, to.id)) return `${g.nations[to.ctrl].adj} fighters still hold the sky over ${to.name}, and no transport aircraft can fly under them. Win the sky there first.`;
    return null;
  }
  if (n.sealift <= 0) return `${n.name} has no shipping for a landing.`;
  if (cs.some((c) => c.nat !== nat || c.reg !== from.id)) return 'A landing force sails from one port.';
  if (!from.port) return 'A landing force has to embark at a port.';
  if (cs.some((c) => c.kind === 'fort' || c.ap < Math.min(4, c.apMax))) return 'Only corps that have not moved this month can embark.';
  if (!to.seas.length) return `${to.name} has no coast.`;
  if (allied(g, nat, to.ctrl) || !atWar(g, nat, to.ctrl)) return `${to.name} is not enemy ground.`;
  if (cs.length > STACK) return `No more than ${STACK} corps can go ashore in one place.`;
  if (liftLeft(g, nat) < cs.length) return liftLeft(g, nat) ? `There is shipping for only ${liftLeft(g, nat)} more corps this month.` : 'All the shipping is in use this month.';
  if (!seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: surpriseOn(g, nat, to) })) return laneWhy(g, nat, from, to, laneBar(g, nat, from, to, surpriseOn(g, nat, to)));
  // Troopships cannot cross under an enemy sky. Only surprise, or winning the sky over the beach, gets them there.
  if (!surpriseOn(g, nat, to) && skyDenies(g, nat, to.id)) return `${g.nations[to.ctrl].adj} fighters still hold the sky over ${to.name}, and no invasion fleet can cross under them. Win the sky there first.`;
  return null;
}

/** True when a force is airborne troops and nothing else: it goes in by air, with no ships and no sea lane. */
export const airborne = (cs: Corps[]): boolean => cs.length > 0 && cs.every((c) => !!c.airborne);
const dropKey = (g: Game, nat: string): string => `drop:${nat}:${g.turn}`;

/**
 * The share of its strength a nation's troops keep when they fight their way off a beach: more for one that has
 * landing ships built for the work and the naval gunfire to cover them, from the month it has them.
 */
export function assaultStrength(g: Game, nat: string): number {
  let s = NAVY.landing.assault;
  for (const [from, nats, to] of NAVY.landing.craft ?? []) if (g.turn >= from && nats.includes(nat)) s = Math.max(s, to);
  return s;
}

/** How a force would fight its way ashore: from landing craft, or from the air. */
export function landingOpts(g: Game, cs: Corps[], to: Region): { sea: number; surprise: boolean; air?: boolean } {
  if (airborne(cs)) return { sea: NAVY.landing.airborne, surprise: false, air: true };
  const surprise = surpriseOn(g, cs[0].nat, to);
  return { sea: surprise ? NAVY.landing.surprise : assaultStrength(g, cs[0].nat), surprise };
}

export interface LandingPlan {
  why: string | null; surprise: boolean; path: number[]; risk: number; defended: boolean; assess: Assessment | null;
  /** True when the force goes in by air. */
  air?: boolean;
  /** True when the invader has won the sky over the beach. */
  sky: boolean;
  /** The strength of the enemy fleets that would put to sea against the landing. */
  sortie: number;
  /** For a landing from the sea that is no surprise: why the crossing is as dangerous as it is, and what would make it safer. */
  account?: CrossingAccount;
  /** For any landing from the sea: whether the sea behind the beach could be kept open once the force is ashore. */
  hold?: SeaHold;
  /** Where the refusal is that no sea lane serves: which of the reasons it is (ruling 207). */
  bar?: LaneBar;
}

/** The enemy warships that put to sea against an invasion: those in harbour on the seas it crosses or the seas next to them, and those at sea next door. */
export function sortieAgainst(g: Game, nat: string, path: number[]): number {
  const near = new Set<number>(path);
  for (const z of path) for (const j of g.seas[z].adj) near.add(j);
  let s = 0;
  for (const f of g.fleets) {
    if (!atWar(g, f.nat, nat) || !surfaceShips(f)) continue;
    if (atSea(f)) { if (!path.includes(f.zone) && near.has(f.zone)) s += fleetPower(g, f); continue; }
    if ((f.base >= 0 ? g.regions[f.base].seas : [f.home]).some((z) => near.has(z))) s += fleetPower(g, f);
  }
  return s * NAVY.landing.sortie;
}

/**
 * The share of a landing force likely to be caught at sea. It runs every danger ordinary shipping does, and off the beach
 * it also meets the enemy fleets that come out against an invasion. An invader who has won the sky over the beach keeps most of them off.
 */
export function landingRisk(g: Game, nat: string, path: number[], to: Region): number {
  return caughtIn(seaDangers(g, nat, path, to));
}

/** One sea of a crossing as the troopships would meet it: who hunts them there, who covers them, and the share caught. */
interface SeaDanger { z: number; last: boolean; ships: number; air: number; sortie: number; own: SeaBalance; hunters: number; caught: number }

/**
 * The seas of a crossing in order. `more` is strength added to the invader's side in the sea `at` (a fleet sent to
 * patrol it), and `beaten` leaves the enemy's warships out: what the crossing would meet if those things were done.
 */
function seaDangers(g: Game, nat: string, path: number[], to: Region, o: { more?: number; at?: number; beaten?: boolean } = {}): SeaDanger[] {
  const sortie = o.beaten ? 0 : sortieAgainst(g, nat, path);
  const sky = skyOver(g, to.id);
  const held = sky !== null && allied(g, sky, nat);
  return path.map((z, i) => {
    const e = seaSide(g, z, nat, true), own = seaSide(g, z, nat, false);
    const last = i === path.length - 1;
    const ships = o.beaten ? 0 : e.ships;
    let hunters = ships + e.air * 0.5 + (last ? sortie : 0);
    if (last && held) hunters *= AIR.sky.seaFoe;
    const mine = o.more && z === o.at ? own.total + o.more : own.total;
    return { z, last, ships, air: e.air, sortie: last ? sortie : 0, own, hunters, caught: hunters > 0 ? hunters / (hunters + 2 * mine + NAVY.intercept.sea) : 0 };
  });
}

/** The share of a force caught on the whole crossing. */
function caughtIn(ds: SeaDanger[]): number {
  let safe = 1;
  for (const d of ds) if (d.hunters > 0) safe *= 1 - d.caught;
  return 1 - safe;
}

/** Works something out as it would be with this nation holding the sky over a region, and puts the sky back as it was. */
function withSky<T>(g: Game, region: number, nat: string, f: () => T): T {
  const was = g.sky[region];
  g.sky[region] = { nat, turn: g.turn };
  try { return f(); } finally { if (was) g.sky[region] = was; else delete g.sky[region]; }
}

/** What would make a crossing safer, and how dangerous it would be then. */
export interface CrossingHelp { id: 'sky' | 'sea' | 'fleets' | 'all'; text: string; risk: number }

/**
 * Why a landing's crossing is as dangerous as it is: the reckoning line by line, as a Battle Ticket sets out a
 * battle, and what the invader could do about it, each with the danger as it would then be.
 */
export interface CrossingAccount {
  /** The share of the force likely to be caught at sea, and the share of its men likely to be lost. */
  risk: number;
  loss: number;
  /** The sky over the beach: the invader's, the enemy's fighters' (no invasion fleet can cross under them), or nobody's. */
  sky: 'ours' | 'denied' | 'open';
  /** The sea where most of the danger lies. */
  worst: number;
  /** The danger as it would be with the sky over the beach won: null where the invader holds it already, or his fighters cannot reach it from any airfield he has. */
  skyWon: number | null;
  lines: TicketLine[];
  helps: CrossingHelp[];
  /** Once ashore: whether the sea behind the beach could be kept open. */
  hold: SeaHold;
}

/**
 * The holding of a beach (Version 73): whether the sea behind it could be kept open to the supply ships once the
 * force is ashore. A sea is closed to a nation when the enemy has `NAVY.closed.min` there and `NAVY.closed.ratio`
 * times its own strength. What the enemy would bring is counted whole: his ships in that sea now, every fleet
 * within a month's sail that his admirals would send into it (fleetsAnswer), and his aircraft over it.
 */
export interface SeaHold {
  /** The sea off the beach: the last the supply ships cross. */
  sea: number;
  /** What the enemy has in it now, the fleets that would come into it at their full strength, and his aircraft over it. */
  there: number;
  come: number;
  air: number;
  /** Fleets that could sail there and would not: kept away by the invader's aircraft over that sea, or too weak to face what he has in it. */
  kept: number;
  shy: number;
  /** All of that together. */
  foe: number;
  /** What the invader has there now: his ships at sea in it and his aircraft over it. */
  own: SeaBalance;
  /** The strength the invader must have there to keep it open against all of that: 0 where the enemy has too little to close any sea. */
  need: number;
  /** True when the sea would stay open with every one of those fleets in it. */
  open: boolean;
  /**
   * True where it stays open and is still contested: the invader has at least half of what the enemy could bring
   * there within the month, and less than all of it. His today, and the enemy can fight him for it (ruling 37).
   */
  contested: boolean;
  /** True for a landing that takes a whole island (a province with no land frontier): cut off there, a force lives on what it carries (ruling 36). */
  island: boolean;
  /** How many months a fresh force cut off from its supplies holds out (game.ts, `monthsUnfed`). */
  months: number;
  /** False where the beach has no harbour: a force is fed only through a port. */
  port: boolean;
  lines: TicketLine[];
}

/** What the enemy's fleets within a month's sail of a beach would do once a force is ashore there. */
export interface FleetsAnswer {
  /** The fleets that would come into the sea off the beach. */
  come: Fleet[];
  /** Those that could sail there and are kept away by the invader's aircraft over that sea. */
  kept: Fleet[];
  /** Those that could sail there and would not face what the invader has in it. */
  shy: Fleet[];
}

/**
 * The enemy's surface fleets that could be in the last sea of a crossing within the month and are not in it now,
 * and what each would do once a force is ashore at `to`. Every fleet that can sail there counts, wherever it lies
 * (fleetReach: the seas within a month of its base, through the straits open to it). A player's own fleets may
 * all come. A computer's admirals follow their own rule (navalAI), and the forecast follows it with them: a raider
 * keeps to its raiding and a fleet tied to its convoys keeps its station; none goes into a sea the invader's
 * aircraft command, unless its own homeland is being invaded across it; and none goes in alone against more than
 * it can face (the invader's whole strength there, or half of it where the homeland is invaded).
 */
export function fleetsAnswer(g: Game, nat: string, path: number[], to?: Region): FleetsAnswer {
  const last = path[path.length - 1];
  const mine = seaSide(g, last, nat, false).total;
  let there = seaSide(g, last, nat, true).total;
  const come: Fleet[] = [], kept: Fleet[] = [];
  const wait: { f: Fleet; home: boolean }[] = [];
  const commanded: Record<string, boolean> = {};
  for (const f of g.fleets) {
    if (!atWar(g, f.nat, nat) || !surfaceShips(f) || (atSea(f) && f.zone === last)) continue;
    const hand = byHand(g, f.nat, 'navy');
    if (!hand && (f.duty === 'raid' || f.tied) && f.station !== last) continue;
    if (!fleetReach(g, f).includes(last)) continue;
    if (hand) { come.push(f); there += fleetPower(g, f); continue; }
    const home = (!!to && to.owner === f.nat && !to.overseas) || invadedHere(g, last, f.nat);
    commanded[f.nat] ??= airOver(g, last, f.nat, true) - airOver(g, last, f.nat, false) > NAVY.air.avoid;
    if (commanded[f.nat] && !home) kept.push(f); else wait.push({ f, home });
  }
  // They come one at a time, and each that comes makes it safer for the next.
  for (let moved = true; moved;) {
    moved = false;
    for (let i = 0; i < wait.length; i++) {
      const p = fleetPower(g, wait[i].f);
      if (there + p < (wait[i].home ? 0.5 : 1) * mine) continue;
      come.push(wait[i].f);
      there += p;
      wait.splice(i--, 1);
      moved = true;
    }
  }
  return { come, kept, shy: wait.map((w) => w.f) };
}

/** The enemy's surface fleets that would come into the last sea of a crossing within the month of a landing at `to`, and are not in it now. */
export const fleetsInReach = (g: Game, nat: string, path: number[], to?: Region): Fleet[] => fleetsAnswer(g, nat, path, to).come;

/** Whether the sea behind a beach could be kept open, as things stand. Nothing is changed. */
export function seaHold(g: Game, nat: string, path: number[], to: Region): SeaHold {
  const sea = path[path.length - 1];
  const e = seaSide(g, sea, nat, true), own = seaSide(g, sea, nat, false);
  const ans = fleetsAnswer(g, nat, path, to);
  const power = (fs: Fleet[]): number => fs.reduce((s, f) => s + fleetPower(g, f), 0);
  const come = power(ans.come), kept = power(ans.kept), shy = power(ans.shy);
  const foe = e.ships + come + e.air;
  const need = foe >= NAVY.closed.min ? foe / NAVY.closed.ratio : 0;
  const open = !(foe >= NAVY.closed.min && foe >= NAVY.closed.ratio * own.total);
  const contested = open && need > 0 && own.total < foe;
  const island = to.nb.length === 0;
  const months = monthsUnfed();
  const name = g.seas[sea].name;
  const lines: TicketLine[] = [];
  if (e.ships >= 0.5) lines.push({ label: `Enemy ships in the ${name} now`, value: round(e.ships), effect: 'at sea there already' });
  if (come >= 0.5) lines.push({ label: 'Enemy fleets that could come', value: round(come), effect: 'at their full strength: every fleet within a month of that sea that his admirals would send' });
  if (kept >= 0.5) lines.push({ label: 'Enemy fleets your aircraft keep away', value: round(kept), effect: 'no admiral takes his fleet under your bombers, except to save his own homeland' });
  if (shy >= 0.5) lines.push({ label: 'Enemy fleets that will not risk it', value: round(shy), effect: 'none of them will face what you have in that sea, while it stays there' });
  if (e.air >= 0.5) lines.push({ label: `Enemy aircraft over the ${name}`, value: round(e.air), effect: 'count against the supply ships too' });
  lines.push({ label: `Your ships and aircraft in the ${name}`, value: `${round(own.ships)} and ${round(own.air)}`, effect: need <= 0 ? 'the enemy has too little in reach to close it' : contested ? `more than the ${round(need)} that keep it open today, and fewer than the ${round(foe)} he can bring` : open ? `more than the ${round(need)} that keep it open` : `${round(need)} would keep it open: one for every ${NAVY.closed.ratio} of his` });
  lines.push({
    label: 'The sea behind the beach', value: contested ? 'contested' : open ? 'stays open' : 'can be closed',
    effect: contested ? 'yours today, but he can bring enough to fight you for it' : open ? 'the supply ships reach the force' : island ? `cut off, the force can hold out for about ${numberWord(months)} months on what it carries` : 'the force ashore would be cut off from its supplies',
  });
  if (!to.port) lines.push({ label: 'A harbour', value: `none at ${to.name}`, effect: 'the force is fed only once it holds a port on an open sea' });
  return { sea, there: e.ships, come, air: e.air, kept, shy, foe, own, need, open, contested, island, months, port: !!to.port, lines };
}

const round = (x: number): string => Math.round(x).toLocaleString('en-US');
/** A small number in words, as the cards say it. */
export const numberWord = (n: number): string => ['no', 'one', 'two', 'three', 'four', 'five', 'six'][n] ?? String(n);

/**
 * The account of a crossing by these seas to that coast. Nothing is changed. (`reach` is the answer to where the
 * nation's fighters can fight for the sky, when the caller has it already.)
 */
export function crossingAccount(g: Game, nat: string, path: number[], to: Region, reach: ReturnType<typeof skyReach> = skyReach(g, nat)): CrossingAccount {
  const ds = seaDangers(g, nat, path, to);
  const risk = caughtIn(ds);
  const holder = skyOver(g, to.id);
  const sky = holder !== null && allied(g, holder, nat) ? 'ours' : skyDenies(g, nat, to.id) ? 'denied' : 'open';
  const worst = ds.reduce((a, d) => (d.caught > a.caught ? d : a), ds[ds.length - 1]);
  const name = (z: number): string => g.seas[z].name;
  // Where the fight for the sky over the beach could be flown from, if anywhere.
  const fly = sky === 'ours' ? null : reach(to.id);
  const skyWon = fly ? withSky(g, to.id, nat, () => landingRisk(g, nat, path, to)) : null;
  const lines: TicketLine[] = [{ label: 'The way', value: path.map((z, i) => (i ? `then the ${name(z)}` : `the ${name(z)}`)).join(', '), effect: path.length > 1 ? 'every sea crossed is a chance to be found' : undefined }];
  for (const d of ds) {
    if (d.ships >= 0.5) lines.push({ label: `Enemy ships at sea in the ${name(d.z)}`, value: round(d.ships), effect: 'hunt the troopships' });
    if (d.air >= 0.5) lines.push({ label: `Enemy aircraft over the ${name(d.z)}`, value: round(d.air), effect: 'count for half against ships under way' });
    if (d.last && d.sortie >= 0.5) lines.push({ label: 'Enemy fleets that will come out', value: round(d.sortie), effect: 'from harbours on these seas and the seas next to them' });
    if (d.last) lines.push({ label: `The sky over ${to.name}`, value: sky === 'ours' ? 'yours' : sky === 'denied' ? 'held by enemy fighters' : 'nobody holds it', effect: sky === 'ours' ? `keeps ${Math.round((1 - AIR.sky.seaFoe) * 100)}% of them off the troopships` : sky === 'denied' ? 'no invasion fleet can cross under them' : fly ? 'no help to either side' : 'out of reach of your fighters from any airfield you hold' });
    if (d.hunters > 0 || d.last) lines.push({ label: `Your ships and aircraft in the ${name(d.z)}`, value: `${round(d.own.ships)} and ${round(d.own.air)}`, effect: d.own.total >= 0.5 ? 'each counts double against the hunters' : 'nothing covers the troopships' });
  }
  lines.push({ label: 'Caught at sea', value: `${Math.round(risk * 100)} in 100`, effect: risk > 0 ? `about ${Math.round(risk * NAVY.intercept.loss * 100)}% of the men lost` : 'nothing is in a position to stop the crossing' });
  const helps: CrossingHelp[] = [];
  if (risk > 0) {
    // The invader's own fleets that could stand in the worst sea and are not there now.
    const cover = fleetsOf(g, nat).filter((f) => surfaceShips(f) > 0 && !(f.zone === worst.z && atSea(f)) && fleetReach(g, f).includes(worst.z));
    const more = cover.reduce((s, f) => s + fleetPower(g, f), 0);
    const foes = ds.some((d) => d.ships >= 0.5 || d.sortie >= 0.5);
    const tell = (id: CrossingHelp['id'], text: string, r: number): void => { if (r < risk - 0.005) helps.push({ id, text, risk: r }); };
    if (fly && skyWon !== null) tell('sky', fly.how === 'now' ? `Win the sky over ${to.name} with a fight for the sky flown by your fighters` : `Bring fighters up to the airfields of ${g.regions[fly.base].name} and win the sky over ${to.name}`, skyWon);
    if (more >= 1) tell('sea', `Send ${cover.length === 1 ? cover[0].name : `your ${cover.length} fleets in reach`} to patrol the ${name(worst.z)}`, caughtIn(seaDangers(g, nat, path, to, { more, at: worst.z })));
    if (foes) tell('fleets', 'Beat the enemy fleets within reach, or strike them in harbour, so that none comes out', caughtIn(seaDangers(g, nat, path, to, { beaten: true })));
    if (helps.length > 1) {
      const rest = (): number => caughtIn(seaDangers(g, nat, path, to, { more: more >= 1 ? more : 0, at: worst.z, beaten: foes }));
      tell('all', 'All of these together', helps.some((h) => h.id === 'sky') ? withSky(g, to.id, nat, rest) : rest());
    }
  }
  return { risk, loss: risk * NAVY.intercept.loss, sky, worst: worst.z, skyWon, lines, helps, hold: seaHold(g, nat, path, to) };
}

/** What a landing would meet: the seas to cross, the danger on the way, and the odds on the beach. Nothing is changed. */
export function planLanding(g: Game, cs: Corps[], to: Region): LandingPlan {
  const why = whyNotLand(g, cs, to);
  if (why) {
    // The lane's own refusal is the one that names a sea, a distance or a strait: say which, for the page.
    const from = g.regions[cs[0].reg], sur = surpriseOn(g, cs[0].nat, to);
    const lane = !airborne(cs) && from.port && to.seas.length > 0 && !seaPath(g, cs[0].nat, from, to, { max: NAVY.landing.range, surprise: sur }) && why === laneWhy(g, cs[0].nat, from, to, laneBar(g, cs[0].nat, from, to, sur));
    return { why, surprise: false, path: [], risk: 0, defended: false, assess: null, sky: false, sortie: 0, ...(lane ? { bar: laneBar(g, cs[0].nat, from, to, sur) } : {}) };
  }
  const nat = cs[0].nat;
  const defended = hostileUnits(g, to, nat).length > 0;
  const sky = skyOver(g, to.id);
  if (airborne(cs)) {
    return {
      why: null, surprise: false, path: [], risk: 0, defended, air: true, sky: sky !== null && allied(g, sky, nat), sortie: 0,
      assess: defended ? assess(g, cs, to, landingOpts(g, cs, to)) : null,
    };
  }
  const surprise = surpriseOn(g, nat, to);
  const path = seaPath(g, nat, g.regions[cs[0].reg], to, { max: NAVY.landing.range, surprise })!;
  return {
    why: null, surprise, path, risk: surprise ? 0 : landingRisk(g, nat, path, to), defended,
    ...(surprise ? {} : { account: crossingAccount(g, nat, path, to) }),
    hold: seaHold(g, nat, path, to),
    sky: sky !== null && allied(g, sky, nat), sortie: surprise ? 0 : sortieAgainst(g, nat, path),
    assess: defended ? assess(g, cs, to, { sea: surprise ? NAVY.landing.surprise : assaultStrength(g, nat), surprise }) : null,
  };
}

export interface Landing { ok: boolean; why?: string; taken?: boolean; lostAtSea?: number; report?: BattleReport | null; surprise?: boolean; air?: boolean }

/** Lands corps on an enemy coast. If it is defended they fight their way ashore, at a disadvantage unless the enemy is surprised. */
export function landCorps(g: Game, cs: Corps[], to: Region): Landing {
  const why = whyNotLand(g, cs, to);
  if (why) return { ok: false, why };
  const nat = cs[0].nat, from = g.regions[cs[0].reg];
  const held = to.ctrl;
  const air = airborne(cs);
  const surprise = !air && surpriseOn(g, nat, to);
  let lostAtSea = 0;
  if (air) g.flags[dropKey(g, nat)] = 1;
  else {
    const path = seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise })!;
    lostAtSea = crossing(g, cs, path, surprise, to);
    g.flags[liftKey(g, nat)] = (g.flags[liftKey(g, nat)] ?? 0) + cs.length;
    for (const z of to.seas) g.flags[`landed:${z}:${nat}`] = g.turn;
  }
  let taken = true;
  let report: BattleReport | null = null;
  if (hostileUnits(g, to, nat).length) {
    report = resolveBattle(g, cs, to, landingOpts(g, cs, to));
    taken = report.taken;
  } else {
    g.week = 1;
    for (const c of cs) relocate(g, c, to);
    // Ashore with no army to meet them: the province is theirs unless its town is held, and then the town is under siege.
    arrive(g, to, nat);
  }
  for (const c of cs) if (!c.dead) c.ap = 0;
  const adj = g.nations[nat].adj;
  if (air) headline(g, 'battle', taken ? `${adj} airborne troops take ${to.name} from the air.` : `A ${adj} airborne assault on ${to.name} is beaten off with heavy loss.`, nat, to.id, { arm: 'land', won: taken ? nat : held, lost: taken ? held : nat });
  else headline(g, 'battle', taken ? `${adj} troops land at ${to.name} from the sea.` : `A ${adj} landing at ${to.name} is thrown back into the sea.`, nat, to.id, { arm: 'land', won: taken ? nat : held, lost: taken ? held : nat });
  return { ok: true, taken, lostAtSea, report, surprise, air };
}

/** Enemy coasts a force embarked at this port could land on this month. */
export function landingTargets(g: Game, cs: Corps[]): Region[] {
  if (!cs.length) return [];
  if (!airborne(cs) && (g.nations[cs[0].nat].sealift <= 0 || !g.regions[cs[0].reg].port)) return [];
  return g.regions.filter((r) => r.seas.length > 0 && whyNotLand(g, cs, r) === null);
}

// ---- The crossings a nation could make ------------------------------------------------------------------

/** The safest way over the water to one enemy-held shore: from which port, to which coast, and what bars it today. */
export interface CrossingOutlook {
  /** Who holds the shore, and whose ground it is. `home` when it is the enemy's own country, and no colony of his. */
  foe: string; owner: string; home: boolean;
  from: Region; to: Region; path: number[];
  /** Why no landing can be made there now, or null: no shipping, a sea the enemy has closed, a sky his fighters hold. */
  bar: string | null;
  barred: 'lift' | 'sea' | 'sky' | null;
  account: CrossingAccount;
  /** True for the crossing the nation has named as its plan. */
  planned: boolean;
  /** True for a crossing listed because it would be the safest way to its shore once the sky over the beach was won. */
  hope?: boolean;
  /** The sea that bars it, where a closed sea does. */
  shut?: number;
  /** True for the shortest crossing to its shore, listed (for the page alone) because a closed sea bars it and the list shows a longer way round. */
  short?: boolean;
}

const aimKey = (nat: string): string => `st:aim:${nat}`;
const quayKey = (nat: string): string => `st:quay:${nat}`;

/**
 * The crossing a nation has named as its plan (the grand campaign): the quay and the beach. A computer staff names
 * its own (ai.ts); a player names his from the list of crossings. The admirals then stand a fleet off the beach and
 * the air staff fights for the sky over it, for a player where he has handed those desks to his staff.
 */
export function plannedCrossing(g: Game, nat: string): { from: Region; to: Region } | null {
  const to = g.regions[(g.flags[aimKey(nat)] ?? 0) - 1], from = g.regions[(g.flags[quayKey(nat)] ?? 0) - 1];
  if (!to || !from || !atWar(g, nat, to.ctrl) || !allied(g, nat, from.ctrl)) return null;
  return { from, to };
}

/** Names a crossing as the plan, or (with no beach) drops the plan. */
export function planCrossing(g: Game, nat: string, from: Region | null, to: Region | null): boolean {
  if (!from || !to) { delete g.flags[aimKey(nat)]; delete g.flags[quayKey(nat)]; return true; }
  if (!seaWar(g) || !from.port || !to.seas.length || !allied(g, nat, from.ctrl) || !atWar(g, nat, to.ctrl)) return false;
  if (!seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: true })) return false;
  g.flags[aimKey(nat)] = to.id + 1;
  g.flags[quayKey(nat)] = from.id + 1;
  return true;
}

/**
 * The ground a nation's own army can march to: enemy ground joined by land to ground it holds itself or to where a
 * field corps of its own stands. An ally's front is not its own: the Red Army on the Bug does not put Brittany
 * within a march of a British or an American corps in Devon, and those shores stay on the list of crossings.
 * The coast watch counts by it too (until Version 77 it counted an ally's front as the attacker's own, and with
 * the Soviet Union in the war no German coast was watched).
 */
function armyReach(g: Game, nat: string): Uint8Array {
  const byLand = new Uint8Array(g.regions.length);
  const q: number[] = [];
  const own = (u: Corps): boolean => u.nat === nat && !u.dead && u.kind !== 'fort';
  for (const r of g.regions) if (r.ctrl === nat || (allied(g, nat, r.ctrl) && (r.units.some(own) || (r.garr ?? []).some(own)))) { byLand[r.id] = 1; q.push(r.id); }
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!byLand[j] && atWar(g, nat, g.regions[j].ctrl)) { byLand[j] = 1; q.push(j); }
  return byLand;
}

/** One crossing looked at: the lane (through a closed sea if there is no other), what bars it, and its account. */
function lookAcross(g: Game, nat: string, from: Region, to: Region, reach: ReturnType<typeof skyReach>): Omit<CrossingOutlook, 'foe' | 'owner' | 'home' | 'planned'> | null {
  const n = g.nations[nat];
  let path = seaPath(g, nat, from, to, { max: NAVY.landing.range });
  let shut = -1;
  if (!path) {
    path = seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: true });
    if (!path) return null;
    shut = path.find((z) => closedTo(g, z, nat)) ?? -1;
  }
  const account = crossingAccount(g, nat, path, to, reach);
  const barred = n.sealift <= 0 ? 'lift' : shut >= 0 ? 'sea' : account.sky === 'denied' ? 'sky' : null;
  const bar = barred === 'lift' ? `${n.name} has no shipping for a landing. Troopships are laid down in the shipyards.`
    : barred === 'sea' ? `The ${g.seas[shut].name} is closed to your troopships: the enemy is much the stronger there.`
    : barred === 'sky' ? `${g.nations[to.ctrl].adj} fighters hold the sky over ${to.name}, and no invasion fleet can cross under them. Win the sky there first.`
    : null;
  return { from, to, path, bar, barred, account, ...(barred === 'sea' ? { shut } : {}) };
}

/**
 * The crossings open to a nation at war (the grand campaign): for each shore an enemy holds that its army cannot
 * reach by land, the safest way to it from a port on its own side, within a landing's range. Barred ones are listed
 * too, with the reason: a coast under an enemy sky is offered to no landing force, and this says why. The crossing
 * named as the plan comes first, with a safer way to the same shore after it when there is one.
 */
export function crossingsOf(g: Game, nat: string, o: { short?: boolean } = {}): CrossingOutlook[] {
  const n = g.nations[nat];
  if (!seaWar(g) || !n?.alive || !n.atWar.length) return [];
  // One port for each stretch of water: the one with most of this nation's troops on its quays.
  const troops = (r: Region): number => r.units.reduce((s, u) => s + (u.nat === nat && !u.dead && u.kind !== 'fort' ? 1 : 0), 0);
  const ports = new Map<string, Region>();
  for (const r of g.regions) {
    if (!r.port || !r.seas.length || !allied(g, nat, r.ctrl)) continue;
    const k = r.seas.join(','), p = ports.get(k);
    if (!p || troops(r) > troops(p) || (troops(r) === troops(p) && r.ctrl === nat && p.ctrl !== nat)) ports.set(k, r);
  }
  if (!ports.size) return [];
  // Enemy ground the nation's own army can reach by land needs no ships. The other shores, by whose ground they are
  // and who holds them. (Until Version 76 an ally's front counted as its own, and once the Soviet Union was in the
  // war every shore of the continent dropped off a British or an American player's list.)
  const byLand = armyReach(g, nat);
  // Nor is the lost ground of a living ally that the ally's own army can march to: freeing it is that army's work
  // (Odessa is the Red Army's to retake, not a beach for a corps from Devon). A fallen ally's country stays.
  const theirs = new Map<string, Uint8Array>();
  const allysOwn = (r: Region): boolean => {
    if (r.owner === nat || r.owner === r.ctrl || !g.nations[r.owner]?.alive || !allied(g, nat, r.owner)) return false;
    if (!theirs.has(r.owner)) theirs.set(r.owner, armyReach(g, r.owner));
    return !!theirs.get(r.owner)![r.id];
  };
  const shores = new Map<string, Region[]>();
  for (const r of g.regions) {
    if (byLand[r.id] || !r.seas.length || !atWar(g, nat, r.ctrl) || !g.nations[r.ctrl]?.alive || allysOwn(r)) continue;
    const k = `${r.owner}:${r.ctrl}:${r.overseas ? 1 : 0}`;
    shores.set(k, [...(shores.get(k) ?? []), r]);
  }
  const plan = plannedCrossing(g, nat);
  const reach = skyReach(g, nat);
  const out: CrossingOutlook[] = [];
  // What a crossing would run with the sky over its beach won, where that can be done and nothing else bars the way.
  const won = (x: CrossingOutlook): number => (x.barred === 'lift' || x.barred === 'sea' ? Infinity : x.account.skyWon ?? (x.account.sky === 'ours' ? x.account.risk : Infinity));
  const better = (a: CrossingOutlook, b: CrossingOutlook | null): boolean => {
    if (!b) return true;
    if ((a.bar === null) !== (b.bar === null)) return a.bar === null;
    return a.account.risk < b.account.risk - 1e-9;
  };
  for (const [k, coasts] of shores) {
    const [owner, foe, far] = k.split(':');
    const home = owner === foe && far === '0';
    let best: CrossingOutlook | null = null, hope: CrossingOutlook | null = null, short: CrossingOutlook | null = null;
    // The lane depends only on the waters at each end: one look for each pair, then the sky over each coast.
    const lanes = new Map<string, Region[]>();
    for (const c of coasts) lanes.set(c.seas.join(','), [...(lanes.get(c.seas.join(',')) ?? []), c]);
    for (const p of ports.values()) {
      for (const group of lanes.values()) {
        if (!seaPath(g, nat, p, group[0], { max: NAVY.landing.range, surprise: true })) continue;
        for (const c of group) {
          const look = lookAcross(g, nat, p, c, reach);
          if (!look) continue;
          const row: CrossingOutlook = { ...look, foe, owner, home, planned: false };
          if (better(row, best)) best = row;
          if (won(row) < Infinity && (!hope || won(row) < won(hope) - 1e-9)) hope = row;
          if (o.short && row.barred === 'sea' && (!short || row.path.length < short.path.length)) short = row;
        }
      }
    }
    // The crossing named as the plan is always shown, and beside it a safer way to the same shore if there is one now.
    if (plan && `${plan.to.owner}:${plan.to.ctrl}:${plan.to.overseas ? 1 : 0}` === k) {
      const look = lookAcross(g, nat, plan.from, plan.to, reach);
      if (look) {
        const named: CrossingOutlook = { ...look, foe, owner, home, planned: true };
        out.push(named);
        if (best && best.from === named.from && best.to === named.to) best = null;
        else if (best && !better(best, named)) best = null;
      }
    }
    if (best) out.push(best);
    // A beach whose sky could be won, where that would make it a clearly safer way than the safest as things stand.
    const shown: CrossingOutlook[] = out.filter((x) => x.foe === foe && x.owner === owner && x.home === home);
    if (hope && !shown.some((x) => x.from === hope!.from && x.to === hope!.to) && shown.every((x) => x.bar !== null || won(hope!) < x.account.risk - 0.05) && shown.every((x) => won(hope!) < won(x) - 0.05)) out.push({ ...hope, hope: true });
    // For the page (ruling 207): the short crossing a closed sea bars, where every way listed to this shore is longer.
    const listed: CrossingOutlook[] = out.filter((x) => x.foe === foe && x.owner === owner && x.home === home);
    if (short && listed.length && listed.every((x) => x.barred !== 'sea' && x.path.length > short!.path.length)) out.push({ ...short, short: true });
  }
  // The enemy's own country first, then the safest.
  return out.sort((a, b) => Number(b.planned) - Number(a.planned) || Number(b.home) - Number(a.home) || Number(a.bar !== null) - Number(b.bar !== null) || a.account.risk - b.account.risk || a.to.id - b.to.id);
}

// ---- The coast watch's report --------------------------------------------------------------------------

/**
 * The coast watch (the grand campaign). When a nation's month closes with troops standing on a quay, shipping
 * to carry them, and an enemy's home coast within a landing's range over seas its troopships can use, that enemy's
 * coast watch reports it: his staff mans its harbours, and a player is told. Only a corps that has not moved in
 * the month can embark, so the report always comes a month before the landing can. A coast the attacker's own
 * army could march to is not watched for this: the war there is a land war already.
 */
export function coastReport(g: Game, att: string): void {
  const W = NAVY.watch;
  const n = g.nations[att];
  if (!W || !seaWar(g) || !n?.alive || n.sealift <= 0 || !n.atWar.length) return;
  // The field corps on a quay as the month closes: they will have had no march to make when the next one opens. A town's garrison is not counted.
  const idle = (r: Region): number => r.units.reduce((s, u) => s + (u.nat === att && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne && !u.town ? 1 : 0), 0);
  const quays = g.regions.filter((r) => r.port && r.seas.length > 0 && allied(g, att, r.ctrl) && idle(r) >= W.corps).sort((a, b) => idle(b) - idle(a) || a.id - b.id);
  if (!quays.length) return;
  const byLand = armyReach(g, att);
  for (const d of [...n.atWar].sort()) {
    const x = g.nations[d];
    if (!x?.alive || x.capital < 0) continue;
    const coasts = g.regions.filter((r) => r.owner === d && r.ctrl === d && !r.overseas && r.seas.length > 0 && !byLand[r.id]);
    if (!coasts.length) continue;
    let seen: { quay: Region; coast: Region; corps: number } | null = null;
    for (const q of quays) {
      let near: Region | null = null, len = Infinity;
      for (const c of coasts) {
        const path = seaPath(g, att, q, c, { max: NAVY.landing.range });
        if (path && (path.length < len || (path.length === len && near !== null && 2 * c.vp + c.fac + (c.cap ? 8 : 0) > 2 * near.vp + near.fac + (near.cap ? 8 : 0)))) { near = c; len = path.length; }
      }
      if (near) { seen = { quay: q, coast: near, corps: Math.min(idle(q), n.sealift) }; break; }
    }
    if (!seen) continue;
    // His staff mans the harbours of the homeland from now (staff.ts, the coast watch).
    g.flags[`st:watch:${d}`] = g.turn;
    const key = `cw:${att}:${d}`, at = `cwAt:${att}:${d}`;
    if (g.flags[key] === seen.quay.id + 1 && g.turn - (g.flags[at] ?? -99) < W.every) continue;
    g.flags[key] = seen.quay.id + 1;
    g.flags[at] = g.turn;
    const force = seen.corps === 1 ? 'one corps' : `${seen.corps} corps`;
    if (d === g.human) warning(g, 'Coast watch', `The coast watch reports ${n.adj} shipping gathering at ${seen.quay.name}, with troops on the quays: enough to carry ${force}. ${seen.coast.name} is within their reach, and a landing could come as soon as next month.`, d, seen.coast.id, att);
    else if (att === g.human) headline(g, 'info', `${x.adj} coast watchers have seen your shipping and troops gather at ${seen.quay.name}: ${x.name} expects a landing and is manning its harbours.`, att, seen.quay.id);
  }
}

// ---- Fleet orders -------------------------------------------------------------------------------

/** Seas a fleet can be sent to: those within reach of its base, through straits open to it. */
export function fleetReach(g: Game, f: Fleet): number[] {
  const d = new Int16Array(g.seas.length).fill(-1);
  const q: number[] = [];
  for (const z of f.base >= 0 ? g.regions[f.base].seas : [f.home]) if (d[z] < 0) { d[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) {
    if (d[q[i]] >= NAVY.range) continue;
    for (const j of g.seas[q[i]].adj) if (d[j] < 0 && canPass(g, q[i], j, f.nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  }
  return q;
}

/**
 * Sends a fleet to a sea to patrol or raid there, or back to harbour. In the grand campaign it can also be sent to hunt
 * submarines, or (with the lane named) to sail as the escort of a shipping lane that crosses that sea.
 */
export function sail(g: Game, f: Fleet, zone: number, stance: Stance, lane?: string): boolean {
  if ((stance === 'escort' || stance === 'hunt') && !seaWar(g)) return false;
  if (stance === 'escort' && !seaRoutes(g).some((r) => r.id === lane && r.zones.includes(zone))) return false;
  if (stance !== 'port' && (!shipCount(f.ships) || !fleetReach(g, f).includes(zone))) return false;
  const to = stance === 'port' ? f.home : zone;
  // Who holds a sea decides which ports can be supplied, so the supply map is redrawn when a fleet moves.
  if (f.zone !== to || f.stance !== stance) g.supplyDirty = true;
  f.zone = to;
  f.stance = stance;
  if (stance === 'escort') f.lane = lane; else delete f.lane;
  return true;
}

/** A ship's worth with a convoy, summed over a fleet: destroyers, carriers and cruisers count, battleships and submarines do not. */
export function escortWorth(s: Ships): number {
  let p = 0;
  for (const c of CLASSES) p += s[c] * NAVY.classes[c].escort;
  return p;
}

/** How hard an enemy's raiders are pressing these seas: what a lane's escorts are up against. */
function raidersIn(g: Game, nat: string, z: number): number {
  let p = 0;
  for (const f of g.fleets) if (f.zone === z && atSea(f) && atWar(g, f.nat, nat)) for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].raid;
  return p;
}

/** Why a fleet cannot be bound to a lane as its escort, or null if it can (the grand campaign). */
export function whyNotEscort(g: Game, f: Fleet, lane: string): string | null {
  if (!seaWar(g)) return 'Escorts are bound to shipping lanes only in the grand campaign.';
  const rt = seaRoutes(g).find((r) => r.id === lane);
  if (!rt) return 'There is no such lane.';
  const to = g.nations[rt.to];
  if (!shipCount(f.ships)) return `The ${f.name} has no ships.`;
  if (!to?.alive || (rt.to !== f.nat && !allied(g, f.nat, rt.to))) return `${cap(rt.name)} feeds ${to?.name ?? rt.to}, which is no ally of ${g.nations[f.nat].name}.`;
  if (!to.atWar.length || !g.nations[f.nat].atWar.length) return `Not under attack: nobody is at war over ${rt.name}.`;
  const stop = laneStopped(g, lane);
  if (stop) return `Nothing sails on ${rt.name} now. ${stop.why}`.trim();
  if (escortWorth(f.ships) <= 0) return `The ${f.name} has nothing that can guard a convoy: escorts are destroyers, carriers and cruisers.`;
  const reach = fleetReach(g, f);
  if (!rt.zones.some((z) => reach.includes(z))) return `${cap(rt.name)} is out of reach of the ${f.name} from ${f.port}.`;
  return null;
}

/** Binds a fleet to a shipping lane as its escort: it sails with the convoys, in the sea of the lane where the raiders are thickest. */
export function escort(g: Game, f: Fleet, lane: string): boolean {
  if (whyNotEscort(g, f, lane)) return false;
  const rt = seaRoutes(g).find((r) => r.id === lane)!;
  const reach = fleetReach(g, f);
  let best = -1, bv = -1;
  for (const z of reach) {
    if (!rt.zones.includes(z)) continue;
    const v = raidersIn(g, rt.to, z);
    if (v > bv) { bv = v; best = z; }
  }
  return best >= 0 && sail(g, f, best, 'escort', lane);
}

/** True when a fleet could steam from its present base to this coast, through straits open to it. */
export function canSailTo(g: Game, f: Fleet, r: Region): boolean {
  const seen = new Uint8Array(g.seas.length);
  const q = [f.home];
  seen[f.home] = 1;
  for (let i = 0; i < q.length; i++) {
    if (r.seas.includes(q[i])) return true;
    for (const j of g.seas[q[i]].adj) if (!seen[j] && canPass(g, q[i], j, f.nat)) { seen[j] = 1; q.push(j); }
  }
  return false;
}

/** Friendly ports a fleet could move its base to. */
export function baseTargets(g: Game, f: Fleet): Region[] {
  return g.regions.filter((r) => r.port && r.seas.length > 0 && r.id !== f.base && allied(g, f.nat, r.ctrl) && canSailTo(g, f, r));
}

/** Moves a fleet's base to another friendly port. The fleet spends the month in harbour there. */
export function rebase(g: Game, f: Fleet, r: Region): boolean {
  if (!r.port || !r.seas.length || r.id === f.base || !allied(g, f.nat, r.ctrl) || !canSailTo(g, f, r)) return false;
  f.base = r.id;
  f.port = r.name;
  f.home = r.seas[0];
  f.zone = f.home;
  f.stance = 'port';
  delete f.lane;
  g.supplyDirty = true;
  syncWings(g);
  return true;
}

/** Sinks about this share of a fleet's ships, class by class, and returns what went down. */
export function sinkShips(f: Fleet, frac: number, rng: Rng, list: ShipClass[] = CLASSES): Ships {
  const out = none();
  for (const c of list) {
    const k = Math.min(f.ships[c], rng.roll(f.ships[c] * frac * NAVY.classes[c].tough));
    f.ships[c] -= k;
    f.lost[c] += k;
    out[c] = k;
  }
  return out;
}

// ---- The month at sea -----------------------------------------------------------------------------

/** The start of a nation's month at sea: new ships, lost bases, refits, bombers, and the convoy war. */
export function navalUpkeep(g: Game, nat: string): void {
  const n = g.nations[nat];
  for (const row of NAVY.fleets) {
    if (row.nat !== nat || (row.from ?? 1) !== g.turn || g.turn <= 1 || g.fleets.some((f) => f.nat === nat && f.name === row.name)) continue;
    const f = addFleet(g, row);
    headline(g, 'info', `${f.name} forms at ${f.port}.`, nat);
  }
  const named = (name: string): Fleet | undefined => g.fleets.find((f) => f.nat === nat && f.name === name);
  for (const b of NAVY.building) if (b.nat === nat && g.turn >= b.from && g.turn <= b.to) { const f = named(b.fleet); if (f) f.ships[b.cls] += b.perMonth; }
  for (const [turn, who, fleet, ships, what] of NAVY.arrivals) {
    const f = who === nat && turn === g.turn ? named(fleet) : undefined;
    if (!f) continue;
    for (const c of CLASSES) f.ships[c] += ships[c] ?? 0;
    headline(g, 'info', `${cap(what)} ${/ and /.test(what) || /s$/.test(what.split(' ')[0]) ? 'join' : 'joins'} the ${f.name}.`, nat);
  }
  for (const f of fleetsOf(g, nat)) {
    if (f.base >= 0 && !allied(g, nat, g.regions[f.base].ctrl) && !newBase(g, f)) continue;
    if (!n.atWar.length) { f.stance = 'port'; f.zone = f.home; }
    if (atSea(f)) {
      airAttack(g, f);
      f.ready = f.ready > NAVY.ready.floor ? Math.max(NAVY.ready.floor, f.ready + NAVY.ready.sea) : Math.min(NAVY.ready.floor, f.ready + NAVY.ready.mend);
    } else f.ready = Math.min(1, f.ready + NAVY.ready.port);
  }
  // The grand campaign: the destroyers hunt before the month's cargoes are counted, and thin screens pay for it.
  const sunk = seaWar(g) && n.atWar.length ? huntSubs(g, nat) : undefined;
  if (sunk) torpedoes(g, nat);
  convoyWar(g, nat, sunk);
}

/** How much of a fleet's hunting weight falls on a sea: all of it for a fleet sent there to hunt, part for an escort whose lane crosses it or a patrol there. */
function huntShare(g: Game, f: Fleet, z: number): number {
  const H = FLEETS.hunt;
  if (f.stance === 'escort') {
    const rt = seaRoutes(g).find((r) => r.id === f.lane);
    return rt && rt.zones.includes(z) ? H.escort : f.zone === z ? H.loose : 0;
  }
  if (f.zone !== z) return 0;
  return f.stance === 'hunt' ? H.hunter : f.stance === 'patrol' ? H.loose : 0;
}

/** What a nation's hunters are worth against the submarines in one sea: its destroyers and carriers there, and the aircraft on the coasts. */
export function huntersIn(g: Game, nat: string, z: number): { weight: number; fleets: Fleet[] } {
  const H = FLEETS.hunt;
  let w = 0;
  const fleets: Fleet[] = [];
  for (const f of g.fleets) {
    if (f.nat !== nat || !atSea(f)) continue;
    const p = (f.ships.dd + f.ships.cv * H.carrier * deckWorth(g, f)) * huntShare(g, f, z) * quality(g, nat) * fit(g, f);
    if (p > 0) { w += p; fleets.push(f); }
  }
  w += ownAirOver(g, z, nat) * H.air;
  return { weight: w * techMul(g, nat, 'radar'), fleets };
}

/**
 * The aircraft a nation itself flies from the coasts of a sea. The hunt counts these and not the whole alliance's
 * air cover, which every ally would otherwise sink submarines with in turn.
 */
function ownAirOver(g: Game, z: number, nat: string): number {
  const zone = g.seas[z];
  const n = g.nations[nat];
  if (!zone.coast.length || !n || !n.alive || !n.atWar.length) return 0;
  let s = 0;
  for (const id of zone.coast) {
    const r = g.regions[id];
    if (r.since >= g.turn) continue;
    const sky = skyOver(g, id);
    if (r.ctrl === nat) s += n.air * (sky ? AIR.sky.seaFoe : 1);
    else if (sky === nat) s += n.air * AIR.sky.seaOwn;
  }
  return (s / zone.coast.length) * zone.air;
}

/** The share of the enemy submarines at sea in one place that hunters of this weight sink in a month. */
export function huntShareSunk(weight: number, subs: number): number {
  const H = FLEETS.hunt;
  return weight > 0 && subs > 0 ? Math.min(H.most, H.base + H.rate * (weight / (weight + subs * H.perSub + H.sea))) : 0;
}

/**
 * The grand campaign's month of submarine hunting for one nation: in every sea where enemy submarines are out, its
 * destroyers, carrier aircraft and the aircraft on its coasts sink a share of them. Returns what was sunk, by sea.
 */
export function huntSubs(g: Game, nat: string): Record<number, number> {
  const out: Record<number, number> = {};
  for (const zone of g.seas) {
    const z = zone.id;
    const quarry = g.fleets.filter((f) => f.zone === z && atSea(f) && f.ships.ss > 0 && atWar(g, f.nat, nat));
    if (!quarry.length) continue;
    const h = huntersIn(g, nat, z);
    const subs = quarry.reduce((s, f) => s + f.ships.ss, 0);
    const share = huntShareSunk(h.weight, subs);
    if (share <= 0) continue;
    const rng = seaRng(g, `hunt:${nat}:${z}`);
    let sunk = 0;
    for (const f of quarry) {
      const k = Math.min(f.ships.ss, rng.roll(f.ships.ss * share));
      f.ships.ss -= k; f.lost.ss += k; sunk += k;
    }
    if (!sunk) continue;
    out[z] = sunk;
    // The submarines hit back at the groups sent out after them.
    const groups = h.fleets.filter((f) => f.stance === 'hunt' && f.ships.dd > 0);
    const dd = groups.reduce((s, f) => s + f.ships.dd, 0);
    let cost = 0;
    for (const f of groups) {
      const k = Math.min(f.ships.dd, rng.roll(sunk * FLEETS.hunt.cost * (f.ships.dd / dd)));
      f.ships.dd -= k; f.lost.dd += k; cost += k;
    }
    const who = groups.length ? `Destroyers of the ${groups[0].name}` : h.fleets.length ? `The escorts of the ${h.fleets[0].name}` : `${g.nations[nat].adj} aircraft`;
    const told = !g.human || g.human === nat || allied(g, g.human, nat) || quarry.some((f) => f.nat === g.human);
    const subsSunk = `${sunk} ${g.nations[quarry[0].nat].adj} submarine${sunk === 1 ? '' : 's'}`;
    if (told) routineNews(g, `hunt:${nat}:${z}`, 'battle', `${who} sink ${subsSunk} in the ${zone.name}${cost ? `, for the loss of ${cost} destroyer${cost === 1 ? '' : 's'}` : ''}.`, `${subsSunk} sunk in the ${zone.name}`, nat, undefined, { arm: 'sea', won: nat, lost: quarry[0].nat });
  }
  return out;
}

/** The share of the destroyer screen a fleet's heavy ships are missing: 0 when there are destroyers enough, 1 when there are none. */
export function screenGap(f: Fleet): number {
  const heavy = f.ships.bb + f.ships.cv + f.ships.ca;
  return heavy > 0 ? clamp(1 - f.ships.dd / (heavy * FLEETS.screen.per), 0, 1) : 0;
}

/** The chance that a fleet at sea in this sea loses a heavy ship to a submarine this month, for want of destroyers round it. */
export function torpedoRisk(g: Game, f: Fleet, z: number): number {
  const gap = screenGap(f);
  if (gap <= 0) return 0;
  let subs = 0;
  for (const x of g.fleets) if (x.zone === z && atSea(x) && x.ships.ss > 0 && atWar(g, x.nat, f.nat)) subs += x.ships.ss * quality(g, x.nat) * fit(g, x);
  return Math.min(FLEETS.screen.most, subs * FLEETS.screen.risk * gap);
}

/** The grand campaign: heavy ships at sea among enemy submarines without destroyers enough to screen them may be torpedoed. */
function torpedoes(g: Game, nat: string): void {
  for (const f of fleetsOf(g, nat)) {
    if (!atSea(f)) continue;
    const p = torpedoRisk(g, f, f.zone);
    if (p <= 0) continue;
    const rng = seaRng(g, `torpedo:${f.id}`);
    if (!rng.chance(p)) continue;
    const heavy: ShipClass[] = ['bb', 'cv', 'ca'];
    let pick = rng.range(0, f.ships.bb + f.ships.cv + f.ships.ca);
    let cls: ShipClass = 'ca';
    for (const c of heavy) { if (pick < f.ships[c]) { cls = c; break; } pick -= f.ships[c]; }
    if (f.ships[cls] <= 0) continue;
    f.ships[cls]--; f.lost[cls]++;
    f.ready = Math.max(0, f.ready - 0.05);
    headline(g, 'battle', `A submarine torpedoes and sinks a ${NAVY.classes[cls].one} of the ${f.name} in the ${g.seas[f.zone].name}: the fleet has too few destroyers to screen its heavy ships.`, nat, undefined, { arm: 'sea', lost: nat });
  }
}

/** A fleet whose base has fallen makes for the nearest port its nation still holds. Returns false if there is none and the fleet is lost. */
function newBase(g: Game, f: Fleet): boolean {
  const old = g.regions[f.base];
  let best: Region | null = null, bd = 1e9;
  for (const r of g.regions) {
    if (!r.port || !r.seas.length || r.ctrl !== f.nat || !canSailTo(g, f, r)) continue;
    const d = Math.hypot(r.x - old.x, r.y - old.y);
    if (d < bd) { bd = d; best = r; }
  }
  if (best) {
    rebase(g, f, best);
    headline(g, 'event', `With ${old.name} lost, the ${f.name} moves its base to ${best.name}.`, f.nat, best.id);
    return true;
  }
  g.fleets.splice(g.fleets.indexOf(f), 1);
  if (shipCount(f.ships) >= 5) headline(g, 'event', `With no port left to it, the ${f.name} is scuttled.`, f.nat, old.id, { arm: 'sea', lost: f.nat });
  return false;
}

/** Bombers flying from an enemy coast take their toll of a surface fleet that stays within their reach. */
function airAttack(g: Game, f: Fleet): void {
  if (!surfaceShips(f)) return;
  const net = airOver(g, f.zone, f.nat, true) - airOver(g, f.zone, f.nat, false);
  if (net <= NAVY.air.safe) return;
  // Carrier fighters on patrol over the fleet keep a share of them off it (the grand campaign).
  const sunk = sinkShips(f, Math.min(NAVY.air.maxLoss, (net - NAVY.air.safe) * NAVY.air.lossPer) * (1 - capOver(g, f)), seaRng(g, `air:${f.id}`), SURFACE);
  f.ready = Math.max(0, f.ready - 0.1);
  if (shipCount(sunk)) headline(g, 'battle', `Bombers sink ${shipList(sunk)} of the ${f.name} in the ${g.seas[f.zone].name}.`, f.nat, undefined, { arm: 'air', lost: f.nat });
}

/** How much of its raiding strength a fleet at sea brings against cargoes: all of it when it is raiding, part when it is on patrol, none on other work. */
const raidShare = (f: Fleet): number => (f.stance === 'raid' ? 1 : f.stance === 'patrol' ? NAVY.convoy.patrolRaid : 0);

/**
 * How much of its escort worth a friendly fleet at sea brings to the cargoes that cross these seas. In the history
 * campaigns a fleet on patrol in one of them guards them. In the grand campaign the fleets bound to the lane as its
 * escort guard it in full, and a fleet that only patrols or hunts in one of its seas gives distant cover.
 */
function guardShare(g: Game, f: Fleet, zones: number[], lane?: string): number {
  if (!seaWar(g)) return f.stance === 'patrol' && zones.includes(f.zone) ? 1 : 0;
  if (f.stance === 'escort' && lane !== undefined && f.lane === lane) return 1;
  return f.stance !== 'raid' && zones.includes(f.zone) ? FLEETS.escort.loose : 0;
}

/** The month's fighting on the convoy routes that feed this nation: raiders against escorts. */
export function convoyWar(g: Game, nat: string, sunkAt?: Record<number, number>): void {
  const n = g.nations[nat];
  for (const route of NAVY.convoys) {
    if (route.to !== nat) continue;
    if (!n.atWar.length) { delete g.convoys[route.id]; continue; }
    const zones = route.zones.map((k) => zoneOf(g, k));
    const rng = seaRng(g, `convoy:${route.id}`);
    let raid = 0, guard = 0;
    const raiders: Fleet[] = [], escorts: Fleet[] = [];
    for (const f of g.fleets) {
      if (!atSea(f)) continue;
      if (atWar(g, f.nat, nat)) {
        if (!zones.includes(f.zone)) continue;
        let p = 0;
        for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].raid;
        p *= quality(g, f.nat) * fit(g, f) * raidShare(f);
        if (p > 0) { raid += p; raiders.push(f); }
      } else if (allied(g, f.nat, nat)) {
        const w = guardShare(g, f, zones, route.id);
        if (w <= 0) continue;
        let p = 0;
        for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].escort * (c === 'cv' ? deckWorth(g, f) : 1);
        p *= quality(g, f.nat) * fit(g, f) * w;
        if (p > 0) { guard += p; escorts.push(f); }
      }
    }
    if (route.base) {
      // An enemy fortress astride the route hunts the convoys for as long as it holds out, and harder while it is itself fed.
      const b = g.byName[route.base.region];
      if (b && atWar(g, b.ctrl, nat)) raid += route.base.raid * quality(g, b.ctrl) * (g.supply[b.ctrl] && !g.supply[b.ctrl][b.id] ? route.base.starved : 1);
    }
    for (const z of zones) guard += airOver(g, z, nat) * NAVY.convoy.air;
    // Radar and direction finding tell the escorts where the raiders are.
    const G = guard * NAVY.convoy.escortWorth * techMul(g, nat, 'radar');
    const L = raid > 0 ? raid / (raid + G + NAVY.convoy.ocean) : 0;
    const tons = Math.round((route.tons * L) / 1000) * 1000;
    let subsLost = 0, escortsLost = 0;
    if (raid > 0) {
      // In the grand campaign the submarines have already been hunted, sea by sea, by the destroyers and aircraft alone.
      if (sunkAt) for (const z of zones) subsLost += sunkAt[z] ?? 0;
      else {
        const hunt = NAVY.convoy.subBase + NAVY.convoy.subHunt * (G / (raid + G + NAVY.convoy.ocean));
        for (const f of raiders) {
          const k = Math.min(f.ships.ss, rng.roll(f.ships.ss * hunt));
          f.ships.ss -= k; f.lost.ss += k; subsLost += k;
        }
      }
      const dd = escorts.reduce((s, f) => s + f.ships.dd, 0);
      for (const f of escorts) {
        const k = Math.min(f.ships.dd, rng.roll(raid * NAVY.convoy.escortLoss * (f.ships.dd / Math.max(1, dd))));
        f.ships.dd -= k; f.lost.dd += k; escortsLost += k;
      }
    }
    g.convoys[route.id] = { turn: g.turn, raid, guard, tons, total: (g.convoys[route.id]?.total ?? 0) + tons, share: route.share * L, loss: L, subsLost, escortsLost };
    storyConvoy(g, raiders, escorts, tons, raid > 0);
    // The monthly figures are news only to the nations fighting over the route.
    if (tons > 0 && g.human && (g.human === nat || atWar(g, g.human, nat))) {
      headline(g, 'info', `${cap(route.name)}: ${tons.toLocaleString('en-US')} tons of shipping sunk this month${subsLost ? `, ${subsLost} ${subsLost === 1 ? 'submarine' : 'submarines'} destroyed` : ''}.`, nat, undefined, { arm: 'sea', lost: nat });
    }
  }
}

/**
 * The share of its cargoes a nation would lose this month on a lane through these seas: raiders against escorts,
 * counted as the convoy war counts them, with no dice thrown and no ship lost. The grand campaign's shipping lanes
 * for oil and supplies are read from this.
 */
export function laneLoss(g: Game, nat: string, zones: number[], lane?: string): { raid: number; guard: number; loss: number } {
  let raid = 0, guard = 0;
  for (const f of g.fleets) {
    if (!atSea(f)) continue;
    if (atWar(g, f.nat, nat)) {
      if (!zones.includes(f.zone)) continue;
      let p = 0;
      for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].raid;
      raid += p * quality(g, f.nat) * fit(g, f) * raidShare(f);
    } else if (allied(g, f.nat, nat)) {
      const w = guardShare(g, f, zones, lane);
      if (w <= 0) continue;
      let p = 0;
      for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].escort * (c === 'cv' ? deckWorth(g, f) : 1);
      guard += p * quality(g, f.nat) * fit(g, f) * w;
    }
  }
  for (const z of zones) guard += airOver(g, z, nat) * NAVY.convoy.air;
  const G = guard * NAVY.convoy.escortWorth * techMul(g, nat, 'radar');
  const loss = raid > 0 ? (raid / (raid + G + NAVY.convoy.ocean)) * traitSeaLoss(g, nat) : 0;
  return { raid, guard, loss: Math.min(0.95, loss) };
}

/** Which way Swedish iron ore is reaching Germany this month, if at all. */
export function oreRoute(g: Game): 'baltic' | 'narvik' | 'cut' | 'none' {
  const o = NAVY.ore, n = g.nations[o.to], src = g.byName[o.source];
  if (!n?.alive || !n.atWar.length || !src) return 'none';
  if (atWar(g, o.to, src.ctrl)) return 'cut';
  if (!o.iceMonths.includes(turnToDate(g.turn).month + 1) && !o.summerZones.some((k) => closedTo(g, zoneOf(g, k), o.to))) return 'baltic';
  const port = g.byName[o.winterPort];
  return port && !atWar(g, o.to, port.ctrl) ? 'narvik' : 'cut';
}

/** The share of a nation's production lost at sea this month: cargoes sunk on its convoy routes, and ore that cannot get through. */
export function seaIncomeLoss(g: Game, nat: string): number {
  let s = 0;
  for (const route of NAVY.convoys) {
    const m = route.to === nat ? g.convoys[route.id] : undefined;
    if (m && g.turn - m.turn <= 1) s += m.share;
  }
  if (nat === NAVY.ore.to && oreRoute(g) === 'cut') s += NAVY.ore.share;
  // A nation that reads the raiders' signals steers its convoys round them (the grand campaign).
  return Math.min(0.9, s) * traitSeaLoss(g, nat);
}

// ---- Fleet actions ----------------------------------------------------------------------------------

const sideOf = (f: Fleet): SeaSide => ({ nat: f.nat, fleet: f.name, admiral: f.admiral?.name ?? null, ships: { ...f.ships }, sunk: none() });

function afterAction(g: Game, f: Fleet, side: SeaSide, won: boolean, rng: Rng): void {
  f.fought = g.turn;
  f.ready = Math.max(0, f.ready - (won ? NAVY.ready.won : NAVY.ready.lost));
  const a = f.admiral;
  if (!a) return;
  if (!won && side.sunk.bb + side.sunk.cv > 0 && rng.chance(NAVY.admiral.lostWithFlagship)) {
    headline(g, 'event', `Admiral ${a.name} is lost with his flagship.`, f.nat, undefined, { arm: 'sea', lost: f.nat });
    const pool = COMMAND.names[f.nat];
    f.admiral = pool ? { name: `${rng.pick(pool.first)} ${rng.pick(pool.last)}`, level: 1, xp: 0 } : null;
    return;
  }
  a.xp += won ? 2 : 1;
  if (a.level < NAVY.admiral.maxLevel && a.xp >= a.level * 3) { a.xp = 0; a.level++; }
}

/**
 * The fleet actions at the end of a nation's month. Wherever its patrolling fleets share a sea
 * with an enemy at sea and are strong enough to offer battle, they do.
 */
export function seaBattles(g: Game, nat: string): SeaReport[] {
  const out: SeaReport[] = [];
  if (!g.nations[nat]?.alive) return out;
  const sum = (fs: Fleet[]): number => fs.reduce((s, f) => s + fleetPower(g, f), 0);
  for (const zone of g.seas) {
    const z = zone.id;
    const foes = g.fleets.filter((f) => f.zone === z && atSea(f) && atWar(g, f.nat, nat) && shipCount(f.ships) > 0);
    if (!foes.length) continue;
    const mine = g.fleets.filter((f) => f.zone === z && f.stance === 'patrol' && surfaceShips(f) > 0 && (f.nat === nat || (allied(g, f.nat, nat) && foes.some((e) => atWar(g, f.nat, e.nat)))));
    if (!mine.some((f) => f.nat === nat)) continue;
    const rng = seaRng(g, `battle:${z}:${nat}`);
    // A patrolling enemy stands and fights. Raiding surface ships have to be found first; submarines are the escorts' business.
    const met = foes.filter((f) => surfaceShips(f) > 0 && (f.stance === 'patrol' || rng.chance(NAVY.battle.catchRaider)));
    if (!met.length) continue;
    const airA = airOver(g, z, nat) * NAVY.air.weight, airD = airOver(g, z, nat, true) * NAVY.air.weight;
    const A = sum(mine) + airA, D = Math.max(1, sum(met) + airD);
    if (A < NAVY.battle.seek * D) continue;
    const names = (fs: Fleet[]): string => { const l = fs.map((f) => `the ${f.name}`); return l.length === 1 ? l[0] : `${l.slice(0, -1).join(', ')} and ${l[l.length - 1]}`; };
    if (A >= NAVY.battle.evadeAt * D && rng.chance(NAVY.battle.evade)) {
      for (const f of met) sail(g, f, f.home, 'port');
      headline(g, 'event', `${cap(names(met))} ${met.length === 1 ? 'declines' : 'decline'} battle in the ${zone.name} and ${met.length === 1 ? 'makes' : 'make'} for port.`, met[0].nat, undefined, { arm: 'sea', won: nat, lost: met[0].nat });
      continue;
    }
    const att = mine.map(sideOf), def = met.map(sideOf);
    const R = (A / D) * rng.range(0.8, 1.2);
    const won = R >= 1;
    const edge = won ? R : 1 / R;
    const hard = clamp(0.14 + 0.08 * edge, 0.18, 0.5), light = clamp(0.16 / edge, 0.03, 0.14);
    mine.forEach((f, i) => { att[i].sunk = sinkShips(f, won ? light : hard, rng); });
    met.forEach((f, i) => { def[i].sunk = sinkShips(f, won ? hard : light, rng); });
    const total = (sides: SeaSide[]): Ships => { const t = none(); for (const s of sides) for (const c of CLASSES) t[c] += s.sunk[c]; return t; };
    const ships = (fs: SeaSide[]): number => fs.reduce((s, x) => s + shipCount(x.ships), 0);
    const lines: TicketLine[] = [
      { label: 'Forces', value: `${ships(att)} ships against ${ships(def)}`, effect: `Fleet strength ${Math.round(sum(mine))} against ${Math.round(sum(met))} before the battle` },
    ];
    if (airA > 0.5 || airD > 0.5) lines.push({ label: 'Air', value: airA > airD ? 'The attacker has the stronger air cover from the coast' : airD > airA ? 'The defender has the stronger air cover from the coast' : 'Even', effect: `Worth ${Math.round(airA)} to the attacker and ${Math.round(airD)} to the defender` });
    const flag = (fs: Fleet[]): string => fs.filter((f) => f.admiral).map((f) => `${f.admiral!.name}${f.admiral!.level > 1 ? ` (level ${f.admiral!.level})` : ''}`).join(', ') || 'no admiral named';
    lines.push({ label: 'Admirals', value: `${flag(mine)} against ${flag(met)}` });
    const kitA = techNotes(g, mine.map((f) => f.nat), ['navy']), kitD = techNotes(g, met.map((f) => f.nat), ['navy']);
    if (kitA.length || kitD.length) lines.push({ label: 'Equipment', value: [...kitA, ...kitD].map((x) => x.text).join('; '), effect: [...kitA.map((x) => `Attacker ${x.pct > 0 ? 'up' : 'down'} ${Math.abs(x.pct)}%`), ...kitD.map((x) => `Defender ${x.pct > 0 ? 'up' : 'down'} ${Math.abs(x.pct)}%`)].join(', ') });
    const worn = [...mine, ...met].filter((f) => f.ready < 0.8);
    if (worn.length) lines.push({ label: 'Readiness', value: `${worn.map((f) => f.name).join(', ')} not fully fit for sea`, effect: 'A fleet in need of refit fights below its strength' });
    mine.forEach((f, i) => afterAction(g, f, att[i], won, rng));
    met.forEach((f, i) => afterAction(g, f, def[i], !won, rng));
    // Fleets that have fought are in contact: each side has the other identified this month (the grand campaign's search at sea).
    if (carrierRules(g)) {
      for (const f of mine) for (const e of met) (f.seen ??= {})[e.nat] = g.turn;
      for (const f of met) for (const e of mine) (f.seen ??= {})[e.nat] = g.turn;
    }
    // The beaten side breaks off and makes for harbour.
    for (const f of won ? met : mine) sail(g, f, f.home, 'port');
    const level: VictoryLevel = won ? (edge >= 2 ? 'Decisive victory' : 'Close victory') : edge >= 2 ? 'Decisive defeat' : 'Close defeat';
    g.flags.seaBattles = (g.flags.seaBattles ?? 0) + 1;
    const rep: SeaReport = {
      id: g.flags.seaBattles, turn: g.turn, zone: z, zoneName: zone.name, attNat: nat, defNat: met[0].nat,
      att, def, lines, aPower: A, dPower: D, odds: R, level, won,
    };
    g.seaReports.push(rep);
    out.push(rep);
    const win = won ? mine : met, lose = won ? met : mine;
    const ws = total(won ? att : def), ls = total(won ? def : att);
    storySea(g, z, win, lose, ls.bb + ls.cv, shipCount(ls), shipCount(ws));
    headline(g, 'battle', `${cap(names(win))} ${win.length === 1 ? 'defeats' : 'defeat'} ${names(lose)} in the ${zone.name}: ${shipList(ls)} sunk${shipCount(ws) ? ` for the loss of ${shipList(ws)}` : ''}.`, win[0].nat, undefined, { arm: 'sea', won: win[0].nat, lost: lose[0].nat });
  }
  return out;
}

/** When a nation falls its navy leaves the war. */
export function fleetsOnFall(g: Game, nat: string): void {
  const gone = fleetsOf(g, nat);
  if (!gone.length) return;
  const big = gone.reduce((s, f) => s + shipCount(f.ships, SURFACE), 0) >= 20;
  g.fleets = g.fleets.filter((f) => f.nat !== nat);
  if (big) headline(g, 'event', `The ${g.nations[nat].adj} fleet is laid up in its ports under the armistice.`, nat);
}

// ---- The computer's admirals ----------------------------------------------------------------------------

/** True when an enemy landed troops on this sea's coast this month or last. */
function landedLately(g: Game, z: number, nat: string): boolean {
  for (const n of Object.values(g.nations)) if (atWar(g, n.id, nat) && (g.flags[`landed:${z}:${n.id}`] ?? -9) >= g.turn - 1) return true;
  return false;
}

/** True when the enemy stands on this nation's own soil on this sea's coast. A colony overseas is not the homeland: the battle fleet is not risked for it unasked. */
function invadedHere(g: Game, z: number, nat: string): boolean {
  return g.seas[z].coast.some((id) => g.regions[id].owner === nat && !g.regions[id].overseas && atWar(g, nat, g.regions[id].ctrl));
}

/**
 * True where a sea lies off a port this nation's side stands round and ships can still feed (the grand campaign):
 * such a town is never starved while one sea off it is open, so a fleet is wanted there to close it.
 */
function blockadeWanted(g: Game, z: number, nat: string): boolean {
  if (!siegeRules(g)) return false;
  return g.seas[z].coast.some((id) => {
    const r = g.regions[id];
    if (!r.port || !r.seas.includes(z) || !atWar(g, nat, r.ctrl) || !besieged(g, r)) return false;
    const by = besiegerOf(g, r);
    return by !== null && allied(g, nat, by) && feeders(g, r).length > 0;
  });
}

/**
 * The seas a nation's staff means to send its second front across (ai.ts keeps the beach it has its eye on, and
 * staff.ts the quay, in the flags): the sea off the beach, and the others on the way to it.
 */
function crossings(g: Game, nat: string): { off: Set<number>; way: Set<number> } {
  const off = new Set<number>(), way = new Set<number>();
  for (const sfx of ['', ':o']) {
    const aim = g.regions[(g.flags[`st:aim:${nat}${sfx}`] ?? 0) - 1], quay = g.regions[(g.flags[`st:quay:${nat}${sfx}`] ?? 0) - 1];
    if (!aim || !quay || !atWar(g, nat, aim.ctrl)) continue;
    const path = seaPath(g, nat, quay, aim, { max: NAVY.landing.range, surprise: true });
    if (!path) continue;
    path.forEach((z, i) => (i === path.length - 1 ? off : way).add(z));
  }
  return { off, way };
}

/** Orders for one computer-run nation's fleets. Named orders from the plan come first. */
export function navalAI(g: Game, nat: string, orders?: { fleet: string; zone: string; stance: Stance }[]): void {
  // The player's fleets are his own to order, unless he has handed the navy to his admirals (depth.ts).
  if (byHand(g, nat, 'navy')) return;
  const n = g.nations[nat];
  let cross: { off: Set<number>; way: Set<number> } | null = null;
  for (const f of fleetsOf(g, nat)) {
    const home = (): void => { sail(g, f, f.home, 'port'); };
    if (!n.atWar.length || !shipCount(f.ships)) { home(); continue; }
    const o = orders?.find((x) => x.fleet === f.name);
    if (o) { if (!sail(g, f, zoneOf(g, o.zone), o.stance)) home(); continue; }
    // A worn fleet stays in harbour until it is fit for sea again.
    if (f.ready < 0.5 || (!atSea(f) && f.ready < NAVY.ready.sortieAt)) { home(); continue; }
    if (f.duty === 'raid') { if (!sail(g, f, f.station, 'raid')) home(); continue; }
    // The grand campaign: a fleet tied to a station on one of its side's shipping lanes sails with the convoys.
    const lane = seaWar(g) ? laneOnBeat(g, f) : null;
    if (lane && f.tied && escort(g, f, lane.id)) continue;
    const mine = fleetPower(g, f);
    const reach = fleetReach(g, f);
    cross ??= crossings(g, nat);
    // A fleet hunts on its own beat and the seas next to it. It goes further only to meet a landing or an invasion.
    const beat = new Set([f.station, ...g.seas[f.station].adj]);
    let best = -1, bv = 0;
    for (const z of f.tied ? [f.station] : reach) {
      const e = seaSide(g, z, nat, true);
      const own = seaSide(g, z, nat, false).total + (f.zone === z && atSea(f) ? 0 : mine);
      // Keep out of water the enemy's bombers command, unless the homeland is being invaded across it and there is a fighting chance.
      const invaded = invadedHere(g, z, nat) && own >= 0.5 * e.total;
      if (airOver(g, z, nat, true) - airOver(g, z, nat, false) > NAVY.air.avoid && !invaded) continue;
      // A fleet that aircraft have just struck in a sea keeps out of it for a while (the grand campaign).
      if (f.struck && f.struck[2] === z && g.turn - f.struck[0] <= CARRIERS.ai.shy && !invaded && carrierRules(g)) continue;
      // Enemy surface ships that could be brought to battle here. Submarines are no quarry for a battle fleet.
      let quarry = 0;
      for (const x of g.fleets) if (x.zone === z && atSea(x) && surfaceShips(x) > 0 && atWar(g, x.nat, nat)) quarry += fleetPower(g, x);
      let v = 0;
      if (f.duty === 'patrol' && beat.has(z) && quarry > 0 && own >= NAVY.battle.seek * e.total) v += quarry; // an enemy fleet that can be beaten
      if (landedLately(g, z, nat) && own >= e.total) v += 30; // cut the sea lane behind an enemy landing
      // Its own staff's second front: stand off the beach the army means to land on, and on the way to it.
      if (f.duty === 'patrol' && own >= e.total && (cross.off.has(z) || cross.way.has(z))) v += cross.off.has(z) ? 40 : 25;
      if (invaded) v += 60;
      // A port the army stands round and cannot starve while ships reach it: close the sea off it, if the fleet can live there.
      if (f.duty === 'patrol' && own >= NAVY.battle.seek * e.total && blockadeWanted(g, z, nat)) v += SIEGE.ai.sea.blockade;
      if (z === f.station && f.duty === 'patrol' && (quarry <= 0 || own >= 0.7 * e.total)) v += 5; // the usual beat
      if (v > bv) { bv = v; best = z; }
    }
    // With no enemy fleet to fight, a fleet whose beat is on a lane the raiders are bleeding guards the cargoes instead.
    if (lane && f.duty === 'patrol' && bv <= 5 && laneLoss(g, lane.to, lane.zones, lane.id).raid > 0 && escort(g, f, lane.id)) continue;
    if (best < 0 || !sail(g, f, best, 'patrol')) home();
  }
}

/** The shipping lane a computer-run fleet's station lies on, if it feeds the fleet's own nation or an ally at war: its own first. */
function laneOnBeat(g: Game, f: Fleet): SeaRoute | null {
  let found: SeaRoute | null = null;
  for (const rt of seaRoutes(g)) {
    if (!rt.zones.includes(f.station) || whyNotEscort(g, f, rt.id)) continue;
    if (rt.to === f.nat) return rt;
    found ??= rt;
  }
  return found;
}
