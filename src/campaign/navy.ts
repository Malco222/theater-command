// The war at sea: fleets, who holds which sea, naval battles, the convoy war, and armies
// carried across the water. Like the land war it is resolved from what the campaign map
// knows (ships by class, aircraft on the coasts, admirals, readiness), and every number
// lives in data/campaign/navy.json and data/campaign/seas.json.

import mapData from '../generated/campaign-map.json';
import data from '../../data/campaign/navy.json';
import fleetData from '../../data/campaign/fleets.json';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { AIR, CARRIERS, carrierRules, skyDenies, skyOver } from './air';
import { capOver, deckWorth, syncWings } from './carriers';
import { COMMAND } from './commanders';
import { storyConvoy, storyEdge, storySea } from './stories';
import { techMul, techNotes } from './research';
import { whyBarred } from './ground';
import { allied, assess, atWar, capture, friendlyUnits, headline, hostileUnits, men, relocate, resolveBattle, type Assessment } from './game';
import { traitFleet, traitSeaLoss } from './traits';
import { RES, fuelFleet, laneStopped } from './resources';
import {
  STACK, type BattleReport, type Corps, type Fleet, type Game, type Region, type SeaReport, type SeaSide, type SeaZone, type ShipClass, type Ships, type Stance,
} from './types';

interface ClassSpec { label: string; one: string; many: string; power: number; tough: number; escort: number; raid: number }
interface FleetRow { nat: string; name: string; from?: number; base: string | null; port: string; home: string; zone: string; duty: Stance; tied?: boolean; admiral?: string; ships: Partial<Ships> }
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

/** The sea under a point on the map, or -1 where it is land or off the chart. */
export function seaAt(x: number, y: number): number {
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

export function setupNavy(g: Game): void {
  const md = (mapData as any).seas;
  g.seas = md.zones.map((z: any, id: number) => ({ id, key: z.id, name: z.name, x: z.x, y: z.y, adj: z.adj, coast: z.coast, air: z.air ?? 1, narrow: !!z.narrow }));
  g.gates = md.gates.map((gt: any) => ({ a: gt.a, b: gt.b, name: gt.name, region: gt.region ? g.byName[gt.region]?.id ?? -1 : -1, nation: gt.nation ?? null, closed: !!gt.closed }));
  g.fleets = [];
  g.nextFleet = 1;
  g.seaReports = [];
  g.shoreReports = [];
  g.convoys = {};
  for (const row of NAVY.fleets) if ((row.from ?? 1) <= 1) addFleet(g, row);
}

function addFleet(g: Game, row: FleetRow): Fleet {
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

/** Whether a nation's ships may pass between two neighbouring seas: most are open water, a few are straits someone holds. */
export function canPass(g: Game, a: number, b: number, nat: string): boolean {
  const gt = g.gates.find((x) => (x.a === a && x.b === b) || (x.a === b && x.b === a));
  if (!gt) return true;
  const holder = gt.region >= 0 ? g.regions[gt.region].ctrl : gt.nation!;
  if (gt.closed) return allied(g, holder, nat) || !g.nations[nat].atWar.length;
  return !atWar(g, holder, nat);
}

/** The seas a nation's supply ships and troopships can use: those it can reach from a home port without crossing a sea closed to it. */
export function openSeas(g: Game, nat: string): Uint8Array {
  const open = new Uint8Array(g.seas.length);
  const shut = g.seas.map((z) => closedTo(g, z.id, nat));
  const q: number[] = [];
  const start = (z: number): void => { if (!shut[z] && !open[z]) { open[z] = 1; q.push(z); } };
  for (const r of g.regions) if (r.port && r.owner === nat && r.ctrl === nat) r.seas.forEach(start);
  for (const f of g.fleets) if (f.nat === nat && f.base < 0) start(f.home); // a base off the map is a home port too
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
  if (!allied(g, c.nat, to.ctrl) || hostileUnits(g, to, c.nat).length) return `${to.name} is not in friendly hands.`;
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
  if (lost >= 500) headline(g, 'battle', `${c.name} loses ${lost.toLocaleString('en-US')} men to enemy ships and aircraft on the crossing to ${to.name}.`, c.nat, to.id);
  return true;
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
  if (!seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: surpriseOn(g, nat, to) })) return 'No sea lane to that coast is open: it is too far, or the enemy holds the sea between.';
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
  const sortie = sortieAgainst(g, nat, path);
  const sky = skyOver(g, to.id);
  const held = sky !== null && allied(g, sky, nat);
  let safe = 1;
  path.forEach((z, i) => {
    const e = seaSide(g, z, nat, true);
    const last = i === path.length - 1;
    let hunters = e.ships + e.air * 0.5 + (last ? sortie : 0);
    if (last && held) hunters *= AIR.sky.seaFoe;
    if (hunters > 0) safe *= 1 - hunters / (hunters + 2 * seaSide(g, z, nat, false).total + NAVY.intercept.sea);
  });
  return 1 - safe;
}

/** What a landing would meet: the seas to cross, the danger on the way, and the odds on the beach. Nothing is changed. */
export function planLanding(g: Game, cs: Corps[], to: Region): LandingPlan {
  const why = whyNotLand(g, cs, to);
  if (why) return { why, surprise: false, path: [], risk: 0, defended: false, assess: null, sky: false, sortie: 0 };
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
    capture(g, to, nat);
  }
  for (const c of cs) if (!c.dead) c.ap = 0;
  const adj = g.nations[nat].adj;
  if (air) headline(g, 'battle', taken ? `${adj} airborne troops take ${to.name} from the air.` : `A ${adj} airborne assault on ${to.name} is beaten off with heavy loss.`, nat, to.id);
  else headline(g, 'battle', taken ? `${adj} troops land at ${to.name} from the sea.` : `A ${adj} landing at ${to.name} is thrown back into the sea.`, nat, to.id);
  return { ok: true, taken, lostAtSea, report, surprise, air };
}

/** Enemy coasts a force embarked at this port could land on this month. */
export function landingTargets(g: Game, cs: Corps[]): Region[] {
  if (!cs.length) return [];
  if (!airborne(cs) && (g.nations[cs[0].nat].sealift <= 0 || !g.regions[cs[0].reg].port)) return [];
  return g.regions.filter((r) => r.seas.length > 0 && whyNotLand(g, cs, r) === null);
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
  if (!to.atWar.length || !g.nations[f.nat].atWar.length) return `${cap(rt.name)} is not under attack: nobody is at war over it.`;
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
  w += airOver(g, z, nat) * H.air;
  return { weight: w * techMul(g, nat, 'radar'), fleets };
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
    if (told) headline(g, 'battle', `${who} sink ${sunk} ${g.nations[quarry[0].nat].adj} submarine${sunk === 1 ? '' : 's'} in the ${zone.name}${cost ? `, for the loss of ${cost} destroyer${cost === 1 ? '' : 's'}` : ''}.`, nat);
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
    headline(g, 'battle', `A submarine torpedoes and sinks a ${NAVY.classes[cls].one} of the ${f.name} in the ${g.seas[f.zone].name}: the fleet has too few destroyers to screen its heavy ships.`, nat);
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
  if (shipCount(f.ships) >= 5) headline(g, 'event', `With no port left to it, the ${f.name} is scuttled.`, f.nat, old.id);
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
  if (shipCount(sunk)) headline(g, 'battle', `Bombers sink ${shipList(sunk)} of the ${f.name} in the ${g.seas[f.zone].name}.`, f.nat);
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
      headline(g, 'info', `${cap(route.name)}: ${tons.toLocaleString('en-US')} tons of shipping sunk this month${subsLost ? `, ${subsLost} ${subsLost === 1 ? 'submarine' : 'submarines'} destroyed` : ''}.`, nat);
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
    headline(g, 'event', `Admiral ${a.name} is lost with his flagship.`, f.nat);
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
      headline(g, 'event', `${cap(names(met))} ${met.length === 1 ? 'declines' : 'decline'} battle in the ${zone.name} and ${met.length === 1 ? 'makes' : 'make'} for port.`, met[0].nat);
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
    headline(g, 'battle', `${cap(names(win))} ${win.length === 1 ? 'defeats' : 'defeat'} ${names(lose)} in the ${zone.name}: ${shipList(ls)} sunk${shipCount(ws) ? ` for the loss of ${shipList(ws)}` : ''}.`, win[0].nat);
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

/** Orders for one computer-run nation's fleets. Named orders from the plan come first. */
export function navalAI(g: Game, nat: string, orders?: { fleet: string; zone: string; stance: Stance }[]): void {
  if (nat === g.human) return;
  const n = g.nations[nat];
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
      if (invaded) v += 60;
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
