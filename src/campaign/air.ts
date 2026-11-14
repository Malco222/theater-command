// The war in the air, on the campaign map. An air fleet is a count of fighters and bombers at
// a base. Left alone it supports the armies and defends its own sky. Given orders it fights a
// month-long air campaign against a region or a sea, settled like a land or sea battle from
// what the map knows: numbers, range, radar, readiness and commanders. Every number lives in
// data/campaign/air.json.
//
// In the grand campaign the air war is its own game (data/campaign/airwar.json, and airwar.ts for what both
// interfaces read). Airfields are places: aircraft can be based only where there is one. Each fleet history
// lists is split into air groups of one kind, fighters or bombers, and a group flies one raid a month, planned
// from its target and flown the moment the order is given. A historical scenario keeps the fleets as they were.

import data from '../../data/campaign/air.json';
import warData from '../../data/campaign/airwar.json';
import carrierData from '../../data/campaign/carriers.json';
import arsenalData from '../../data/campaign/arsenal.json';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';
import type { TicketLine, VictoryLevel } from '../core/ticket';
import { allied, atWar, headline, routineNews, worksOf } from './game';
import { techMul, techNotes } from './research';
import { NAVY, atSea, shipCount, shipList, sinkShips, surfaceShips, zoneOf } from './navy';
import { fleetKnown, seaStaff, syncWings } from './carriers';
import { storyAir, storyEdge } from './stories';
import { groundRules } from './ground';
import { bombFactories, bombRail, onNet, railBombChance, railCut, railsAt, ruinChance } from './rails';
import { bombingFor, raidsMul, seat } from './difficulty';
import { fuelAir } from './resources';
import { SIEGE, besieged, besiegerOf, breachOf, fieldCtrl, hitTown, planTownHit, siegeRules, townBonus, townHeld, townHitText, townShare, type TownHit } from './siege';
import { airfieldOpenTo, prizeOut } from './prizes';
import { townName } from './towns';
import type { AirFleet, AirMission, AirPool, AirReport, AirSide, Corps, Fleet, Game, Region, SeaSide, ShipClass, Ships } from './types';

type Sched = [number, number, number][];
interface NatSpec { quality: number; ftrRange: number; bmrRange: number }
interface Chain { nat: string; from: number; name: string; by: string; ground: number }
interface FleetRow { nat: string; name: string; base: string; from?: number; leader?: string; start?: [number, number]; est: Sched; ftrRange?: number; scripted?: boolean }
export interface AirOrder { fleet: string; base?: string[]; mission: AirMission; region?: string; sea?: string }
export interface AirPlan { nat: string; from: number; to: number; name: string; against: string; orders: AirOrder[] }
interface Reserve { ftr: number; bmr: number; pilots: number }

export const AIR = data as unknown as {
  kmPerUnit: number;
  ferry: number;
  nations: Record<string, NatSpec>;
  range: { fullAt: number; atLimit: number };
  radar: { without: number; chains: Chain[] };
  production: Record<string, Sched>;
  pilots: { home: number; away: number; perMonth: Record<string, [number, number][]> };
  reserve: Record<string, Reserve>;
  leader: { perLevel: number; maxLevel: number };
  combat: {
    fighters: number; bombers: number; escortShare: number; bomberDefence: number; maxBomberLoss: number; flak: number; seaCover: number;
    night: { fighters: number; accuracy: number; flak: number }; luck: number; intensity: Record<string, number>;
  };
  damage: {
    airfields: { scale: number; max: number; ground: number; maxGround: number };
    industry: { scale: number; max: number; maxShare: number };
    shipping: { scale: number; sink: number; maxSink: number; coastal: number; tradeScale: number };
  };
  sky: { win: number; least: number; lasts: number; land: number; seaOwn: number; seaFoe: number };
  airfields: { holds: number };
  ready: { support: number; rest: number; flying: number; sortie: number; nightSortie: number; defend: number; move: number; floor: number };
  fleets: FleetRow[];
  plan: AirPlan[];
};

export interface RaidKindRow { key: string; label: string; short: string; blurb: string; needs: 'bmr' | 'any' }
export const AIRWAR = warData as unknown as {
  fields: { sizes: number[]; least: number; engineers: number; seed: { base: number; city: { class: number; fac: number; vp: number }; town: { class: number; fac: number; vp: number } }; words: [number, string][] };
  groups: { most: number; names: Record<string, { ftr: string; bmr: string }> };
  raids: { kinds: RaidKindRow[] };
  ground: { intensity: number; fighters: number; scale: number; perDiv: number; max: number; str: number; floor: number; dug: number; cover: Record<string, number>; fire: number; value: number };
  /** Bombing the line: see the note in the data. */
  line: { intensity: number; scale: number; most: number; value: number };
  recon: { aircraft: number; loss: number; ready: number };
  losses: { bombers: number; power: number; most: number; night: { bombers: number; power: number }; clash: number };
  success: { k: number };
  ai: { escortReady: number; raids: { share: number; least: number; loss: number; rail: number; ready: number; success: number; look: number; worth: number; sweeps: number; sky: number; skyWorth: number; sour?: { trade: number; dear: number; times: number; rest: number } } };
};

/** True once the air war is its own game: airfields are places and air groups of one kind fly from them. The grand campaign only. */
export const airWar = (g: Game): boolean => g.flags.airwar === 1;

/** Carriers and the air war at sea (the grand campaign): see carriers.json, and carriers.ts for the wings themselves. */
export const CARRIERS = carrierData as unknown as {
  wing: Record<string, { ftr: number; bmr: number }>;
  names: { ftr: string; bmr: string };
  range: { ftr: number; bmr: number; next: number };
  quality: Record<string, number>;
  naval: { carrier: number; land: Record<string, number> };
  strike: { hit: number; most: number; again: number; report: number; aim: Ships; takes: Ships; ready: number; value: Ships; intensity: number };
  harbour: { hit: number; most: number; surprise: number; unready: number; intensity: number };
  flak: { ships: Ships; per: number; most: number };
  cap: { spoil: number; scale: number; most: number };
  sunk: { saved: number };
  empty: number;
  search: { ships: Ships; next: number; air: number; flight: number; find: number; report: number; open: number; subs: number; aircraft: number; ready: number };
  ai: { success: number; loss: number; worth: number; landShare: number; sortie: number; ready: number; shy: number };
};

/** True once carriers have wings of their own and aircraft can be aimed at one fleet or at a harbour. The grand campaign only. */
export const carrierRules = (g: Game): boolean => g.flags.carriers === 1;

/** True for a carrier wing: an air group that flies from a fleet's carriers, goes where they go, and has no airfield. */
export const afloat = (f: { ship?: number }): boolean => f.ship !== undefined;

/** The fleet a carrier wing sails with, or null for a group on land (or a wing whose fleet is gone). */
export const carrierOf = (g: Game, f: { ship?: number }): Fleet | null => (f.ship === undefined ? null : g.fleets.find((x) => x.id === f.ship) ?? null);

/** One key for the place a group flies from, so that groups which share it can be told: its airfield's region, or its fleet. */
export const homeOf = (f: AirFleet): number => (f.ship === undefined ? f.base : -1 - f.ship);

export const MISSION: Record<AirMission, { label: string; what: string }> = {
  support: { label: 'Support the armies', what: 'Flies for the armies and defends its own sky.' },
  superiority: { label: 'Win the sky', what: 'Fighters and bombers go for the enemy air force and its airfields over one region.' },
  bomb: { label: 'Bomb industry', what: 'Bombers attack the factories of one region by day, with fighters if it is in their range.' },
  night: { label: 'Bomb by night', what: 'Bombers attack by night: few are lost, and few bombs find the factories.' },
  strike: { label: 'Strike shipping', what: 'Bombers hunt warships and merchant ships in one sea.' },
  rest: { label: 'Rest and refit', what: 'Stands down to refit. It gets replacements first and does not fly.' },
  ground: { label: 'Raid troops', what: 'Fighters and bombers go for an enemy army where it stands, and cut its organisation for the battle that follows.' },
  fleet: { label: 'Strike a fleet', what: 'Bombers go for one enemy fleet at sea, with fighters if it is in their range.' },
  harbour: { label: 'Raid a harbour', what: 'Bombers go for the warships lying in one enemy port.' },
  town: { label: 'Bomb a town', what: 'Bombers go for the defences of a town an army means to take, and shake the troops inside it.' },
  line: { label: 'Bomb the line', what: 'Bombers go for the fieldworks of an enemy line, and take months of digging off them before the army attacks.' },
};
const OFFENSIVE: AirMission[] = ['superiority', 'bomb', 'night', 'strike', 'ground', 'fleet', 'harbour', 'town', 'line'];
/** How hard a month of one kind of mission is fought, beside a fight for the sky. */
const intensity = (m: AirMission): number => (m === 'ground' ? AIRWAR.ground.intensity : m === 'line' ? AIRWAR.line.intensity : m === 'fleet' ? CARRIERS.strike.intensity : m === 'harbour' ? CARRIERS.harbour.intensity : m === 'town' ? SIEGE.bombard.intensity : AIR.combat.intensity[m]);
export const isOffensive = (m: AirMission): boolean => OFFENSIVE.includes(m);

const SURFACE = ['bb', 'cv', 'ca', 'dd'] as const;
const spec = (nat: string): NatSpec => AIR.nations[nat] ?? AIR.nations._default;
/** The air war has its own dice, so that it never changes how a land or sea battle falls out. */
const airRng = (g: Game, key: string): Rng => new Rng(hashString(`air:${g.seed}:${g.turn}:${key}`));
const row = <T extends number[]>(rows: T[] | undefined, turn: number): T | null => {
  let out: T | null = null;
  for (const r of rows ?? []) if (r[0] <= turn) out = r;
  return out;
};
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
const planes = (n: number, one: string): string => `${n.toLocaleString('en-US')} ${one}${n === 1 ? '' : 's'}`;

export const airFleetsOf = (g: Game, nat: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat);
/** How far one fleet reaches: its nation's ranges, unless the fleet has long-range fighters of its own ('ftrRange' on its row in air.json). */
const reach = (f: { nat: string; name: string; parent?: string; ship?: number }): NatSpec => {
  // Carrier aircraft are built for the deck, not for range.
  if (f.ship !== undefined) return { quality: spec(f.nat).quality * (CARRIERS.quality[f.nat] ?? CARRIERS.quality._default), ftrRange: CARRIERS.range.ftr, bmrRange: CARRIERS.range.bmr };
  const own = AIR.fleets.find((x) => x.nat === f.nat && x.name === (f.parent ?? f.name))?.ftrRange;
  return own ? { ...spec(f.nat), ftrRange: own } : spec(f.nat);
};
export const airRange = (f: { nat: string; name: string; parent?: string; ship?: number }): { ftr: number; bmr: number } => ({ ftr: reach(f).ftrRange, bmr: reach(f).bmrRange });

export function airPool(g: Game, nat: string): AirPool {
  return (g.airPool[nat] ??= { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 });
}

/** Kilometres between the centres of two regions. */
export function km(g: Game, a: number, b: number): number {
  const A = g.regions[a], B = g.regions[b];
  return Math.hypot(A.x - B.x, A.y - B.y) * AIR.kmPerUnit;
}

/** How far a base is from a sea: nothing if it stands on its shore, otherwise the distance to the nearest shore. */
export function seaKm(g: Game, base: number, z: number): number {
  let d = Infinity;
  for (const c of g.seas[z].coast) d = Math.min(d, c === base ? 0 : km(g, base, c));
  return d;
}

/**
 * How far a group has to fly to a region. From an airfield it is the distance between the two. A carrier wing at sea
 * is anywhere in its sea: every coast of that sea is at no distance, and ground inland is as far as it lies from that
 * coast. A wing whose fleet is in harbour flies from the port.
 */
export function kmTo(g: Game, f: AirFleet, region: number): number {
  if (f.ship === undefined) return km(g, f.base, region);
  const c = carrierOf(g, f);
  if (!c) return Infinity;
  if (atSea(c)) return seaKm(g, region, c.zone);
  return c.base >= 0 ? km(g, c.base, region) : Infinity;
}

/** How far a group has to fly to a sea. A carrier wing at sea is in its own sea at no distance, and a neighbouring sea counts as a fixed long flight. */
export function kmToSea(g: Game, f: AirFleet, z: number): number {
  if (f.ship === undefined) return seaKm(g, f.base, z);
  const c = carrierOf(g, f);
  if (!c) return Infinity;
  if (atSea(c)) return c.zone === z ? 0 : g.seas[c.zone].adj.includes(z) ? CARRIERS.range.next : Infinity;
  return c.base >= 0 ? seaKm(g, c.base, z) : Infinity;
}

/** Where a group flies from, for a map to draw: its airfield, or the sea (or harbour) its carriers are in. */
export function groupAt(g: Game, f: AirFleet): { x: number; y: number; sea: number; region: number } {
  const c = carrierOf(g, f);
  if (c && atSea(c)) return { x: g.seas[c.zone].x, y: g.seas[c.zone].y, sea: c.zone, region: -1 };
  const r = g.regions[c && c.base >= 0 ? c.base : f.base];
  return { x: r?.x ?? 0, y: r?.y ?? 0, sea: -1, region: r?.id ?? -1 };
}

/** The enemy warships lying in harbour in one port: what a raid on the harbour goes for. */
export const shipsInPort = (g: Game, region: number, nat: string): Fleet[] => g.fleets.filter((x) => x.base === region && !atSea(x) && surfaceShips(x) > 0 && atWar(g, nat, x.nat));

/** How much of a fighter force is still effective at this distance: all of it close in, less toward the limit of its range, none beyond. */
export function cover(d: number, range: number): number {
  if (d > range) return 0;
  const full = AIR.range.fullAt * range;
  if (d <= full) return 1;
  return 1 - (1 - AIR.range.atLimit) * ((d - full) / (range - full));
}

/** How much of a fleet can fly from where it is based: all of it, unless more aircraft share the region's airfields than they can serve. */
export function crowding(g: Game, f: AirFleet): number {
  // A carrier wing is never larger than its decks hold, and takes no room on an airfield.
  if (f.ship !== undefined) return 1;
  const there = g.airFleets.reduce((s, x) => s + (x.base === f.base && x.ship === undefined ? x.ftr + x.bmr : 0), 0);
  // Airfields built since the game began (the grand campaign's arsenal) each serve more aircraft.
  const holds = airWar(g) ? Math.max(AIRWAR.fields.least, airfieldsHold(g, f.base)) : airfieldsHold(g, f.base);
  return there > holds ? holds / there : 1;
}

/** The class of the airfield a region began the grand campaign with: 0 none, 1 a landing ground, 2 an airfield, 3 an air base. */
export const fieldClass = (g: Game, region: number): number => g.flags[`af:${region}`] ?? 0;

/**
 * How many aircraft the airfields of a region can serve. In a historical scenario every region has airfields.
 * In the grand campaign's air war an airfield is a place: where there is none, and none has been built, the answer is 0.
 */
export function airfieldsHold(g: Game, region: number): number {
  const built = (g.flags[`wk:field:${region}`] ?? 0) * WORKS.field.holds;
  return airWar(g) ? (AIRWAR.fields.sizes[fieldClass(g, region)] ?? 0) + built : AIR.airfields.holds + built;
}

/** True if aircraft can be based in this region at all. */
export const hasAirfield = (g: Game, region: number): boolean => !airWar(g) || airfieldsHold(g, region) > 0;

/** The level of anti-aircraft guns round a region's industry: 0 unless some have been built there. */
export const gunsAt = (g: Game, region: number): number => g.flags[`wk:aa:${region}`] ?? 0;

const WORKS = (arsenalData as unknown as { works: { aa: { cut: number; flak: number }; field: { holds: number } } }).works;

const leaderMul = (f: AirFleet): number => 1 + (f.leader ? AIR.leader.perLevel * (f.leader.level - 1) : 0);
/** What one of a fleet's aircraft is worth in a fight. */
const worth = (g: Game, f: AirFleet): number => reach(f).quality * leaderMul(f) * (0.5 + 0.5 * f.ready) * techMul(g, f.nat, 'fighters') * storyEdge(g, f) * fuelAir(g, f.nat);

/** Who holds the sky over a region, if anyone has won it lately. */
export function skyOver(g: Game, region: number): string | null {
  const s = g.sky[region];
  if (!s || g.turn - s.turn > AIR.sky.lasts || !g.nations[s.nat]?.alive) return null;
  return atWar(g, s.nat, g.regions[region].ctrl) ? s.nat : null;
}

/** The fighters that would come up over a region against this nation: every enemy fleet left to defend, as far as its range and airfields allow. */
export function fightersOver(g: Game, region: number, against: string): number {
  const holder = g.regions[region].ctrl;
  let n = 0;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || !atWar(g, f.nat, against) || !allied(g, f.nat, holder)) continue;
    n += f.ftr * cover(kmTo(g, f, region), reach(f).ftrRange) * crowding(g, f);
  }
  return n;
}

/** True when enemy fighters still defend the sky over a coast and the invader has not won it: no invasion fleet can cross under them. */
export function skyDenies(g: Game, nat: string, region: number): boolean {
  const sky = skyOver(g, region);
  if (sky !== null && allied(g, sky, nat)) return false;
  return fightersOver(g, region, nat) >= AIR.sky.least;
}

/** The share of a nation's production stopped this month by bombing and by strikes on its coastal trade. */
export function airIncomeLoss(g: Game, nat: string): number {
  const r = g.raids[nat];
  return r && r.turn === g.turn ? r.share : 0;
}

/** Stops this share of a nation's production for a month: bombs on its factories, or shells from the sea. */
export function raid(g: Game, by: string, victim: string, share: number): void {
  if (share <= 0) return;
  // The loss falls on the victim's next month of production: this month if it has yet to move, otherwise the next.
  const when = g.order.indexOf(by) < g.order.indexOf(victim) ? g.turn : g.turn + 1;
  const old = g.raids[victim];
  g.raids[victim] = { turn: when, share: Math.min(AIR.damage.industry.maxShare, (old && old.turn === when ? old.share : 0) + share) };
}

/** The radar chain that warns of raids on this ground, if its owner has one and still holds it. */
function radarAt(g: Game, r: Region): Chain | null {
  if (r.owner !== r.ctrl) return null;
  return AIR.radar.chains.find((c) => c.nat === r.ctrl && c.from <= g.turn) ?? null;
}

// ---- Setting up ---------------------------------------------------------------------------

export function setupAir(g: Game): void {
  g.airFleets = [];
  g.nextAir = 1;
  g.airReports = [];
  g.airPool = {};
  g.sky = {};
  g.raids = {};
  for (const r of AIR.fleets) if ((r.from ?? 1) <= 1) addAir(g, r);
}

function addAir(g: Game, r: FleetRow): AirFleet | null {
  const base = g.byName[r.base];
  if (!base) return null;
  const e = row(r.est, Math.max(g.turn, r.from ?? 1)) ?? r.est[0];
  const f: AirFleet = {
    id: g.nextAir++, nat: r.nat, name: r.name, base: base.id,
    ftr: r.start ? r.start[0] : e[1], bmr: r.start ? r.start[1] : e[2], est: { ftr: e[1], bmr: e[2] },
    mission: 'support', target: -1, ready: 1, leader: r.leader ? { name: r.leader, level: 1, xp: 0 } : null, moved: 0, lost: { ftr: 0, bmr: 0 },
  };
  g.airFleets.push(f);
  return f;
}

/** A new air fleet with no aircraft yet, on airfields the nation holds: the grand campaign's arsenal fills it. In the air war it is an air group of one kind. */
export function foundAirFleet(g: Game, nat: string, r: Region, name: string, kind?: 'ftr' | 'bmr'): AirFleet {
  const taken = (x: string): boolean => g.airFleets.some((f) => f.name === x);
  let unique = name;
  for (let k = 2; taken(unique); k++) unique = `${name} ${k}`;
  const f: AirFleet = {
    id: g.nextAir++, nat, name: kind ? groupName(g, nat, kind) : unique, base: r.id, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
    mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 },
  };
  if (kind) { f.kind = kind; f.parent = unique; f.share = 1; }
  g.airFleets.push(f);
  return f;
}

// ---- The air war as its own game (the grand campaign) -------------------------------------

const ordinal = (k: number): string => `${k}${k % 100 >= 11 && k % 100 <= 13 ? 'th' : k % 10 === 1 ? 'st' : k % 10 === 2 ? 'nd' : k % 10 === 3 ? 'rd' : 'th'}`;

/** The next name on a nation's list of air groups of one kind: "3rd Bomber Group". */
function groupName(g: Game, nat: string, kind: 'ftr' | 'bmr'): string {
  const key = `agn:${nat}:${kind}`;
  let name: string;
  do {
    g.flags[key] = (g.flags[key] ?? 0) + 1;
    name = (AIRWAR.groups.names[nat] ?? AIRWAR.groups.names._default)[kind].replace('{nth}', ordinal(g.flags[key])).replace('{n}', String(g.flags[key]));
  } while (g.airFleets.some((f) => f.nat === nat && f.name === name));
  return name;
}

/**
 * Splits a fleet as history lists it into air groups of one kind, none larger than the data allows. The fleet
 * itself becomes the first of them and keeps its commander and its record; a fleet small enough to be one group
 * keeps its name. Each group remembers the command it came from, which still sets what it is kept at.
 */
export function splitFleet(g: Game, f: AirFleet): AirFleet[] {
  if (f.kind) return [f];
  const most = AIRWAR.groups.most;
  const plan: { kind: 'ftr' | 'bmr'; of: number; k: number }[] = [];
  for (const kind of ['ftr', 'bmr'] as const) {
    const n = Math.ceil(Math.max(f.est[kind], f[kind]) / most);
    for (let k = 0; k < n; k++) plan.push({ kind, of: n, k });
  }
  f.parent = f.name;
  f.share = 1;
  if (!plan.length) { f.kind = (f.extra?.bmr ?? 0) > (f.extra?.ftr ?? 0) ? 'bmr' : 'ftr'; return [f]; }
  const whole = { ftr: f.ftr, bmr: f.bmr, est: { ...f.est }, extra: f.extra ? { ...f.extra } : null };
  const at = g.airFleets.indexOf(f);
  const out: AirFleet[] = [];
  plan.forEach((p, i) => {
    const part = (n: number): number => Math.floor(n / p.of) + (p.k < n % p.of ? 1 : 0);
    const grp: AirFleet = i === 0 ? f : {
      id: g.nextAir++, nat: f.nat, name: '', base: f.base, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
      mission: f.mission, target: f.target, ready: f.ready, leader: null, moved: f.moved, lost: { ftr: 0, bmr: 0 },
    };
    grp.kind = p.kind;
    grp.parent = f.parent;
    grp.share = 1 / p.of;
    grp.ftr = p.kind === 'ftr' ? part(whole.ftr) : 0;
    grp.bmr = p.kind === 'bmr' ? part(whole.bmr) : 0;
    grp.est = { ftr: p.kind === 'ftr' ? part(whole.est.ftr) : 0, bmr: p.kind === 'bmr' ? part(whole.est.bmr) : 0 };
    if (whole.extra) grp.extra = { ftr: p.kind === 'ftr' ? part(whole.extra.ftr) : 0, bmr: p.kind === 'bmr' ? part(whole.extra.bmr) : 0 };
    if (i > 0) g.airFleets.splice(at + i, 0, grp);
    out.push(grp);
  });
  // The names are given once every group is on the list, so that no two of a nation's are alike.
  if (out.length > 1) for (const grp of out) grp.name = groupName(g, f.nat, grp.kind!);
  return out;
}

/**
 * The grand campaign opens its air war. Airfields are placed where the war begins with them: an air base wherever
 * history bases an air fleet and at every capital, an airfield in every city, a landing ground in every town.
 * Every air fleet becomes air groups of one kind. Done once; a grand campaign saved before the air war is brought in on loading.
 */
export function openAirWar(g: Game): void {
  if (g.flags.airwar === 1) return;
  g.flags.airwar = 1;
  const S = AIRWAR.fields.seed;
  const seed = (id: number, cls: number): void => { if (fieldClass(g, id) < cls) g.flags[`af:${id}`] = cls; };
  for (const r of g.regions) {
    if (r.cap) seed(r.id, S.base);
    else if (r.fac >= S.city.fac || r.vp >= S.city.vp) seed(r.id, S.city.class);
    else if (r.fac >= S.town.fac || r.vp >= S.town.vp) seed(r.id, S.town.class);
  }
  for (const row of AIR.fleets) { const b = g.byName[row.base]; if (b && !row.scripted) seed(b.id, S.base); }
  for (const f of [...g.airFleets]) { seed(f.base, S.base); splitFleet(g, f); }
}

// ---- Bases and orders ---------------------------------------------------------------------

export function whyNotBase(g: Game, f: AirFleet, r: Region): string | null {
  if (f.ship !== undefined) return 'It flies from the carriers of its fleet, and goes where they go.';
  if (r.id === f.base) return 'It is already based there.';
  // An airfield lies outside the town, and goes with the country round it (prizes.ts): an army that holds the
  // field of an enemy province has the use of its airfield after a month, and a wrecked one serves nobody.
  if (!airfieldOpenTo(g, f.nat, r)) {
    const ours = besieged(g, r) && allied(g, f.nat, fieldCtrl(g, r));
    if (!allied(g, f.nat, r.ctrl) && !ours) return 'That ground is not in friendly hands.';
    if (!hasAirfield(g, r.id)) return 'There is no airfield there. Engineers can lay one out, and the treasury can build one.';
    const out = prizeOut(g, r, 'airfield');
    if (out > 0) return `The airfield there is wrecked: it will be ${out === 1 ? 'a month' : `${out} months`} before aircraft can use it.`;
    return ours ? 'Airfields taken this month are not yet in use.' : 'The enemy holds the country round the town, and the airfield with it.';
  }
  if (!hasAirfield(g, r.id)) return 'There is no airfield there. Engineers can lay one out, and the treasury can build one.';
  if (r.since >= g.turn) return 'Airfields taken this month are not yet in use.';
  if (f.moved === g.turn) return 'It has already changed base this month.';
  if (km(g, f.base, r.id) > AIR.ferry) return 'Too far to move in one month.';
  return null;
}

export const airBases = (g: Game, f: AirFleet): Region[] => g.regions.filter((r) => whyNotBase(g, f, r) === null);

/** Moves a fleet to airfields in another friendly region. Settling in costs it some readiness. */
export function rebaseAir(g: Game, f: AirFleet, r: Region): boolean {
  if (whyNotBase(g, f, r)) return false;
  f.base = r.id;
  f.moved = g.turn;
  f.ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  if (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target)) { f.mission = 'support'; f.target = -1; }
  return true;
}

/** True when a strike on this sea has something to hit: enemy ships at sea, or an enemy's trade along its coast. */
function shipping(g: Game, nat: string, z: number): boolean {
  if (g.fleets.some((x) => x.zone === z && atSea(x) && atWar(g, nat, x.nat))) return true;
  if (NAVY.convoys.some((c) => atWar(g, nat, c.to) && c.zones.includes(g.seas[z].key))) return true;
  return g.seas[z].coast.some((id) => g.regions[id].port && atWar(g, nat, g.regions[id].ctrl));
}

/** Why a fleet cannot fly this mission against this target, or null if it can. */
export function whyNotMission(g: Game, f: AirFleet, m: AirMission, target: number): string | null {
  if (m === 'support' || m === 'rest') return null;
  if (!g.nations[f.nat].atWar.length) return 'The nation is at peace.';
  const R = reach(f);
  // An air group of fighters goes on a day raid as its escort, as far as its fighters reach.
  const escort = f.kind === 'ftr' && f.ftr > 0 && m !== 'night';
  if (m === 'strike') {
    if (!g.seas[target]) return 'Pick a sea.';
    if (!f.bmr && !escort) return 'It has no bombers.';
    if (kmToSea(g, f, target) > (escort ? R.ftrRange : R.bmrRange)) return escort ? 'That sea is beyond its fighters.' : 'That sea is beyond its bombers.';
    return shipping(g, f.nat, target) ? null : 'There is no enemy shipping there.';
  }
  if (m === 'fleet') {
    if (!carrierRules(g)) return 'Aircraft are aimed at one fleet only in the grand campaign.';
    const t = g.fleets.find((x) => x.id === target);
    if (!t) return 'Pick a fleet.';
    if (!atWar(g, f.nat, t.nat)) return `The ${t.name} is not an enemy's.`;
    if (!surfaceShips(t)) return `The ${t.name} is submarines and nothing else: destroyers and patrols hunt those.`;
    if (!atSea(t)) return `The ${t.name} is in harbour at ${t.port}: raid the harbour.`;
    if (!f.bmr && !escort) return 'It has no bombers.';
    if (kmToSea(g, f, t.zone) > (escort ? R.ftrRange : R.bmrRange)) return escort ? 'That fleet is beyond its fighters.' : 'That fleet is beyond its bombers.';
    return null;
  }
  const r = g.regions[target];
  if (!r) return 'Pick a region.';
  const d = kmTo(g, f, target);
  if (m === 'harbour') {
    if (!carrierRules(g)) return 'Harbours are raided only in the grand campaign.';
    if (!shipsInPort(g, target, f.nat).length) return 'No enemy warships lie in harbour there.';
    if (!f.bmr && !escort) return 'It has no bombers.';
    return d > (escort ? R.ftrRange : R.bmrRange) ? (escort ? 'That harbour is beyond its fighters.' : 'That harbour is beyond its bombers.') : null;
  }
  if (m === 'ground') {
    if (!airWar(g)) return 'Raids on troops are flown only in the grand campaign.';
    if (!r.units.some((u) => !u.dead && atWar(g, f.nat, u.nat))) return 'There are no enemy troops there.';
    return (f.ftr > 0 && d <= R.ftrRange) || (f.bmr > 0 && d <= R.bmrRange) ? null : 'It is out of range.';
  }
  if (m === 'line') {
    if (!airWar(g)) return 'Fieldworks are bombed only in the grand campaign.';
    if (!r.units.some((u) => !u.dead && atWar(g, f.nat, u.nat) && worksOf(u) > 0)) return 'No enemy troops are dug in there.';
    if (!f.bmr && !escort) return 'It has no bombers.';
    return d > (escort ? R.ftrRange : R.bmrRange) ? (escort ? 'That line is beyond its fighters.' : 'That line is beyond its bombers.') : null;
  }
  if (m === 'town') {
    if (!siegeRules(g) || !airWar(g)) return 'Towns are bombed only in the grand campaign.';
    if (!atWar(g, f.nat, r.ctrl)) return `${townName(r)} is not in enemy hands.`;
    if (!townHeld(g, r)) return `Nobody holds ${townName(r)}: an army can walk in.`;
    if (!f.bmr && !escort) return 'It has no bombers.';
    return d > (escort ? R.ftrRange : R.bmrRange) ? (escort ? 'That town is beyond its fighters.' : 'That town is beyond its bombers.') : null;
  }
  if (!atWar(g, f.nat, r.ctrl)) return 'That region is not in enemy hands.';
  if (m === 'superiority') return (f.ftr > 0 && d <= R.ftrRange) || (f.bmr > 0 && d <= R.bmrRange) ? null : 'It is out of range.';
  if (!f.bmr && !escort) return f.kind === 'ftr' ? 'Fighters do not fly with the night bombers.' : 'It has no bombers.';
  if (d > (escort ? R.ftrRange : R.bmrRange)) return escort ? 'It is beyond its fighters.' : 'It is beyond its bombers.';
  // In the grand campaign the rail yards and junctions are a target too.
  if (r.fac > 0 || (onNet(target) && !railCut(g, target) && groundRules(g))) return null;
  return !groundRules(g) ? 'There is no industry there to bomb.' : onNet(target) ? 'There is no industry there, and its railway is already cut.' : 'There is no industry or railway there to bomb.';
}

/** Everything a fleet could be sent against on this mission: region ids, or sea ids for a strike. */
export function airTargets(g: Game, f: AirFleet, m: AirMission): number[] {
  if (!isOffensive(m)) return [];
  const all = m === 'strike' ? g.seas.map((z) => z.id) : m === 'fleet' ? g.fleets.map((x) => x.id) : g.regions.map((r) => r.id);
  return all.filter((t) => whyNotMission(g, f, m, t) === null);
}

export function setMission(g: Game, f: AirFleet, m: AirMission, target = -1): boolean {
  if (whyNotMission(g, f, m, target)) return false;
  f.mission = m;
  f.target = isOffensive(m) ? target : -1;
  return true;
}

// ---- The month in the air -----------------------------------------------------------------

/** The start of a nation's month in the air: new formations, lost bases, new aircraft and pilots, and rest. */
export function airUpkeep(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n?.alive) return;
  for (const r of AIR.fleets) {
    if (r.nat !== nat || (r.from ?? 1) <= 1 || g.turn < r.from! || g.flags[`air:${r.name}`]) continue;
    // An air force history raised for an invasion of its own planning is nobody's to raise in free play.
    if (r.scripted && g.flags.open === 1) continue;
    const base = g.byName[r.base];
    if (!base || base.ctrl !== nat || base.since >= g.turn) continue;
    const f = addAir(g, r);
    if (!f) continue;
    g.flags[`air:${r.name}`] = g.turn;
    headline(g, 'info', `${f.name} forms in ${base.name}.`, nat, base.id);
    // The air war: it forms as air groups, and the ground it forms on has its airfields.
    if (airWar(g)) { if (!hasAirfield(g, base.id)) g.flags[`af:${base.id}`] = AIRWAR.fields.seed.base; splitFleet(g, f); }
  }
  const p = airPool(g, nat);
  const built = row(AIR.production[nat], g.turn), trained = row(AIR.pilots.perMonth[nat], g.turn);
  if (built) { p.ftr += built[1]; p.bmr += built[2]; }
  if (trained) p.pilots += trained[1];
  for (const f of airFleetsOf(g, nat)) {
    // A carrier wing has no airfield to lose: it goes where its fleet goes.
    if (f.ship !== undefined) continue;
    // Airfields lie out in the country: a group whose province is under siege has lost them, though the town holds.
    // (The army round an enemy town has its airfield, and a wrecked field serves nobody: prizes.ts.)
    if (!airfieldOpenTo(g, nat, g.regions[f.base]) && !fallBack(g, f)) continue;
    // The staff moves strength between fleets by changing what each is kept at.
    const r = AIR.fleets.find((x) => x.nat === nat && x.name === (f.parent ?? f.name));
    const e = r ? row(r.est, g.turn) : null;
    // An air group is kept at its share of its command's aircraft of its own kind.
    const ef = e ? (f.kind === 'bmr' ? 0 : Math.round(e[1] * (f.share ?? 1))) : 0, eb = e ? (f.kind === 'ftr' ? 0 : Math.round(e[2] * (f.share ?? 1))) : 0;
    // Wings bought from the arsenal are kept on top of what the staff allots.
    if (e) f.est = f.extra ? { ftr: ef + f.extra.ftr, bmr: eb + f.extra.bmr } : { ftr: ef, bmr: eb };
    if (f.ftr > f.est.ftr) { const s = f.ftr - f.est.ftr; f.ftr -= s; p.ftr += s; p.pilots += s; }
    if (f.bmr > f.est.bmr) { const s = f.bmr - f.est.bmr; f.bmr -= s; p.bmr += s; }
  }
  // Replacements go to resting fleets first.
  const order = airFleetsOf(g, nat).sort((a, b) => (a.mission === 'rest' ? 0 : 1) - (b.mission === 'rest' ? 0 : 1) || a.id - b.id);
  const R = AIR.ready;
  for (const f of order) {
    // A carrier wing is made up only while its fleet lies in harbour: no replacement aircraft reach a deck at sea.
    const ship = f.ship === undefined ? null : carrierOf(g, f);
    const refits = f.ship === undefined || (!!ship && !atSea(ship));
    const nf = refits ? Math.max(0, Math.min(f.est.ftr - f.ftr, p.ftr, p.pilots)) : 0;
    f.ftr += nf; p.ftr -= nf; p.pilots -= nf;
    const nb = refits ? Math.max(0, Math.min(f.est.bmr - f.bmr, p.bmr)) : 0;
    f.bmr += nb; p.bmr -= nb;
    f.ready = clamp(f.ready + (f.mission === 'rest' ? R.rest : f.mission === 'support' ? R.support : R.flying), R.floor, 1);
    // A group that flew a planned raid last month is back at its airfield, on patrol again.
    if (f.flew !== undefined && f.flew < g.turn && isOffensive(f.mission)) { f.mission = 'support'; f.target = -1; }
    // Orders that no longer make sense lapse.
    if (!n.atWar.length || (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target))) { f.mission = 'support'; f.target = -1; }
  }
  const most = AIR.reserve[nat] ?? AIR.reserve._default;
  p.ftr = Math.min(p.ftr, most.ftr);
  p.bmr = Math.min(p.bmr, most.bmr);
  p.pilots = Math.min(p.pilots, most.pilots);
}

/** A fleet whose base has fallen flies out to the nearest ground its nation still holds, leaving some aircraft behind. */
function fallBack(g: Game, f: AirFleet): boolean {
  const old = g.regions[f.base];
  const nearest = (fields: boolean): Region | null => {
    let out: Region | null = null, bd = Infinity;
    for (const r of g.regions) {
      if (r.ctrl !== f.nat || (fields && !hasAirfield(g, r.id)) || besieged(g, r) || prizeOut(g, r, 'airfield') > 0) continue;
      const d = Math.hypot(r.x - old.x, r.y - old.y);
      if (d < bd) { bd = d; out = r; }
    }
    return out;
  };
  let best = nearest(true);
  // The air war: with no airfield left anywhere, a landing ground is cleared on the nearest ground the nation holds.
  if (!best && airWar(g)) { best = nearest(false); if (best) g.flags[`af:${best.id}`] = 1; }
  const p = airPool(g, f.nat);
  if (!best) {
    g.airFleets.splice(g.airFleets.indexOf(f), 1);
    p.lostFtr += f.ftr; p.lostBmr += f.bmr;
    headline(g, 'event', `With nowhere left to fly from, ${f.name} is disbanded.`, f.nat, old.id);
    return false;
  }
  const lf = Math.round(f.ftr * 0.1), lb = Math.round(f.bmr * 0.1);
  f.ftr -= lf; f.bmr -= lb;
  f.lost.ftr += lf; f.lost.bmr += lb;
  p.lostFtr += lf; p.lostBmr += lb; p.pilots += lf;
  f.base = best.id;
  f.ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  f.mission = 'support';
  f.target = -1;
  headline(g, 'event', `With ${old.name} ${allied(g, f.nat, old.ctrl) && besieged(g, old) ? 'under siege' : 'lost'}, ${f.name} falls back to airfields in ${best.name}.`, f.nat, best.id);
  return true;
}

/** When a nation falls its air force leaves the war. */
export function airOnFall(g: Game, nat: string): void {
  g.airFleets = g.airFleets.filter((f) => f.nat !== nat);
  for (const k of Object.keys(g.sky)) if (g.sky[+k].nat === nat) delete g.sky[+k];
}

// ---- An air campaign, weighed up -----------------------------------------------------------

export interface AirAssess {
  /** Why the mission cannot be flown at all, or null. */
  why: string | null;
  place: string;
  att: { f: AirFleet; d: number; esc: number; crowd: number; ftr: number; bmr: number }[];
  def: { f: AirFleet; d: number; w: number }[];
  /** Escorting fighters that count over the target, and their fighting weight. */
  E: number;
  Epow: number;
  /** Bombers that can reach it. */
  B: number;
  /** Defending fighters that will meet the raids, and their fighting weight. */
  D: number;
  Dpow: number;
  radar: Chain | null;
  /** Share of the bombers expected to be lost. */
  bomberLoss: number;
  /** The level of anti-aircraft guns round the industry being bombed. */
  aa: number;
  lines: TicketLine[];
  /** A strike on a fleet or a raid on a harbour: the fleets under attack, the share of the bombers their guns bring down, and the share of the strike the fighter cover spoils. */
  ships: Fleet[];
  flak: number;
  spoil: number;
  /** What the bombers that go are worth against ships, beside carrier crews trained for it (1). */
  naval: number;
  /** What is left of a strike on a fleet already struck this month: it is scattered and warned. */
  again: number;
  /** A harbour caught unready in the month war was declared. */
  surprise: boolean;
}

const SHIPS: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const noShips = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
/** What one of a group's bombers is worth against ships: carrier crews are trained for it, land bombers mostly are not. */
const navalSkill = (f: AirFleet): number => (f.ship !== undefined ? CARRIERS.naval.carrier : CARRIERS.naval.land[f.nat] ?? CARRIERS.naval.land._default);

/**
 * What a strike on ships should do, before the dice: the hits to expect, the ships of each class they should sink,
 * and what that is worth when the raid is judged. `through` is the bombers that reach the ships, and `found` the
 * share of them that find a fleet known only from a report.
 */
export function shipHits(a: AirAssess, m: AirMission, through: number, found = 1): { hits: number; sunk: Ships; count: number; value: number } {
  const S = CARRIERS.strike, H = CARRIERS.harbour;
  const sunk = noShips();
  let hits = through * a.naval * S.hit * (1 - a.spoil) * a.again * found;
  if (m === 'harbour') hits *= H.hit * (a.surprise ? H.surprise : 1);
  const have = noShips();
  for (const x of a.ships) for (const c of SHIPS) have[c] += x.ships[c];
  const weight = SHIPS.reduce((s, c) => s + have[c] * S.aim[c], 0);
  if (weight <= 0 || hits <= 0) return { hits: 0, sunk, count: 0, value: 0 };
  let count = 0;
  for (const c of SHIPS) { sunk[c] = Math.min(have[c], ((hits * have[c] * S.aim[c]) / weight) / S.takes[c]); count += sunk[c]; }
  // No strike sinks more than so much of what it finds.
  const most = (m === 'harbour' ? H.most : S.most) * SHIPS.reduce((s, c) => s + (S.aim[c] > 0 ? have[c] : 0), 0);
  if (count > most) { for (const c of SHIPS) sunk[c] *= most / count; hits *= most / count; count = most; }
  return { hits, sunk, count, value: SHIPS.reduce((s, c) => s + sunk[c] * S.value[c], 0) };
}

/** How hard the fighters fight over this kind of raid: in the air war only a fight for the sky is fought to a finish. */
const clashOf = (g: Game, m: AirMission): number => (airWar(g) && m !== 'superiority' ? AIRWAR.losses.clash : 1);

/** The largest share of its bombers one raid can lose. */
const mostBomberLoss = (g: Game): number => (airWar(g) ? AIRWAR.losses.most : AIR.combat.maxBomberLoss);

/** What the map says about one air campaign before it is flown: who can reach, who will meet them, and what helps each side. */
export function assessAir(g: Game, fleets: AirFleet[], m: AirMission, target: number): AirAssess {
  const C = AIR.combat;
  // A strike on one fleet is fought over the sea that fleet is in.
  const quarry = m === 'fleet' ? g.fleets.find((x) => x.id === target) ?? null : null;
  const sea = m === 'strike' || m === 'fleet', night = m === 'night';
  const zone = m === 'fleet' ? quarry?.zone ?? -1 : target;
  const nat = fleets[0]?.nat ?? '';
  const place = sea ? `the ${g.seas[zone]?.name ?? 'sea'}` : g.regions[target]?.name ?? '';
  const out: AirAssess = { why: null, place, att: [], def: [], E: 0, Epow: 0, B: 0, D: 0, Dpow: 0, radar: null, bomberLoss: 0, aa: 0, lines: [], ships: [], flak: 0, spoil: 0, naval: 1, again: 1, surprise: false };
  if (!fleets.length) { out.why = 'No air fleet has these orders.'; return out; }
  if (m === 'fleet' && (!quarry || !atSea(quarry) || !surfaceShips(quarry))) { out.why = 'That fleet is not at sea.'; return out; }
  if (m === 'fleet') out.ships = [quarry!];
  else if (m === 'harbour') out.ships = shipsInPort(g, target, nat);
  if (m === 'harbour' && !out.ships.length) { out.why = 'No enemy warships lie in harbour there.'; return out; }
  // A harbour raided in the month war is declared on its fleet is caught unready.
  out.surprise = m === 'harbour' && out.ships.some((x) => g.flags[`war:${nat}:${x.nat}`] === g.turn);
  const unready = out.surprise ? CARRIERS.harbour.unready : 1;
  for (const f of fleets) {
    const d = sea ? kmToSea(g, f, zone) : kmTo(g, f, target);
    const R = reach(f);
    const esc = night ? 0 : cover(d, R.ftrRange);
    const crowd = crowding(g, f);
    const a = { f, d, esc, crowd, ftr: f.ftr * esc * crowd, bmr: d <= R.bmrRange ? f.bmr * crowd : 0 };
    if (a.ftr <= 0 && a.bmr <= 0) continue;
    out.att.push(a);
    out.E += a.ftr;
    out.Epow += a.ftr * worth(g, f);
    out.B += a.bmr;
  }
  if (!out.att.length) { out.why = 'Nothing can reach the target.'; return out; }
  if (m !== 'superiority' && m !== 'ground' && out.B <= 0) { out.why = 'No bombers can reach the target.'; return out; }

  // Radar, and the fighters that come up to meet the raids.
  if (sea) {
    for (const id of g.seas[zone].coast) {
      const c = radarAt(g, g.regions[id]);
      if (c && atWar(g, nat, c.nat)) { out.radar = c; break; }
    }
  } else out.radar = radarAt(g, g.regions[target]);
  // A raid on troops is met by every enemy fighter in reach, whoever holds the ground the troops stand on.
  const holder = sea || m === 'ground' || m === 'line' ? null : g.regions[target].ctrl;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || !atWar(g, f.nat, nat)) continue;
    if (holder && !allied(g, f.nat, holder)) continue;
    const d = sea ? kmToSea(g, f, zone) : kmTo(g, f, target);
    let w = cover(d, reach(f).ftrRange) * crowding(g, f);
    // Carrier fighters are at home over the sea, and over their own fleet its lookouts warn them: they need no radar chain.
    const deck = f.ship !== undefined;
    if (sea && !deck) w *= C.seaCover;
    if (night) w *= C.night.fighters;
    if (!out.radar && !(deck && sea && d <= 0)) w *= AIR.radar.without;
    w *= unready;
    if (w <= 0) continue;
    out.def.push({ f, d, w });
    out.D += f.ftr * w;
    // Fighters over their own ground are only as good as the radar and controllers that put them onto the raid.
    out.Dpow += f.ftr * w * worth(g, f) * techMul(g, f.nat, 'radar');
  }
  const guns = out.B * C.bomberDefence;
  const I = intensity(m);
  // The defending fighters' share of the fighting weight over the target: the escort and the bombers' own guns are the rest.
  const met = out.Dpow / Math.max(1, out.Dpow + out.Epow + guns);
  const most = mostBomberLoss(g);
  // The air war counts its losses nearer history's: an escort saves far more than its numbers.
  const L = airWar(g) ? (night ? AIRWAR.losses.night : AIRWAR.losses) : null;
  out.bomberLoss = out.B > 0 ? Math.min(most, (L ? L.bombers * Math.pow(met, L.power) : C.bombers * met) * I + (night ? C.night.flak : C.flak) * I) : 0;
  // Guns round the factories bring down more of the bombers that come for them.
  out.aa = m === 'bomb' || night || m === 'harbour' || m === 'town' ? gunsAt(g, target) : 0;
  if (out.aa && out.B > 0) out.bomberLoss = Math.min(most, out.bomberLoss + out.aa * WORKS.aa.flak * I * unready);
  // Ships under attack: their own guns, what the fighter cover spoils, and what the crews are worth against ships.
  const shipGuns = out.ships.reduce((s, x) => s + SHIPS.reduce((n, c) => n + x.ships[c] * CARRIERS.flak.ships[c], 0), 0);
  if (out.ships.length) {
    out.flak = Math.min(CARRIERS.flak.most, shipGuns * CARRIERS.flak.per) * unready;
    if (out.B > 0) out.bomberLoss = Math.min(most, out.bomberLoss + out.flak * I);
    out.spoil = CARRIERS.cap.spoil * Math.sqrt(met);
    out.naval = out.B > 0 ? out.att.reduce((s, a) => s + a.bmr * navalSkill(a.f), 0) / out.B : 1;
    const before = Math.max(0, ...out.ships.map((x) => (x.struck && x.struck[0] === g.turn ? x.struck[1] : 0)));
    out.again = Math.pow(CARRIERS.strike.again, before);
  }

  // The Battle Ticket.
  const af = out.att.reduce((s, a) => s + a.f.ftr, 0);
  const ab = out.att.reduce((s, a) => s + (a.bmr > 0 ? a.f.bmr : 0), 0);
  const df = out.def.reduce((s, x) => s + x.f.ftr, 0);
  out.lines.push({
    label: 'Forces',
    value: `${[af ? planes(af, 'fighter') : '', ab ? planes(ab, 'bomber') : ''].filter(Boolean).join(' and ')} against ${df ? planes(df, 'fighter') : 'no fighters'}`,
    effect: out.D < df ? `About ${Math.round(out.D).toLocaleString('en-US')} of the defending fighters can meet the raids` : undefined,
  });
  if (night) out.lines.push({ label: 'Night', value: 'Flown in darkness', effect: 'Few fighters find the bombers, and only about a third of the bombs find the target' });
  else if (af > 0) {
    const far = out.att.filter((a) => a.f.ftr > 0).sort((a, b) => a.esc - b.esc)[0];
    const pct = Math.round((out.att.reduce((s, a) => s + a.f.ftr * a.esc, 0) / af) * 100);
    out.lines.push({
      label: 'Range',
      value: pct >= 100 ? 'The target is well inside fighter range' : pct > 0 ? `The fighters are at ${pct}% effect over the target` : 'The target is beyond fighter range',
      effect: pct >= 100 ? undefined : pct > 0 ? `${far.f.name} flies ${Math.round(far.d)} km to get there, near the limit of its fighters` : 'The bombers go alone',
    });
  } else if (out.B > 0) out.lines.push({ label: 'Range', value: 'No escort', effect: 'The bombers go alone' });
  const packed = [...out.att.map((a) => a.f), ...out.def.map((x) => x.f)].filter((f) => crowding(g, f) < 1);
  if (packed.length) {
    const bases = [...new Set(packed.map((f) => g.regions[f.base].name))];
    const least = Math.min(...packed.map((f) => crowding(g, f)));
    out.lines.push({
      label: 'Airfields',
      value: `The airfields of ${list(bases)} are crowded`,
      effect: `${list(packed.map((f) => f.name))} can fly only ${Math.round(least * 100)}% of ${packed.length === 1 ? 'its' : 'their'} aircraft from there`,
    });
  }
  if (out.aa && m === 'harbour') out.lines.push({ label: 'Anti-aircraft guns', value: `Level ${out.aa} guns ring the harbour of ${place}`, effect: 'They bring down more of the bombers' });
  if (m === 'town') {
    const r = g.regions[target], gone = breachOf(g, r);
    out.lines.push({ label: 'The town', value: `Troops inside ${townName(r)} fight ${Math.round((townBonus(g, r) - 1) * 100)}% stronger${gone > 0 ? `, with ${Math.round(gone * 100)}% of its works already broken down` : ''}`, effect: `${SIEGE.bombard.raidFull} bombers getting through break down ${SIEGE.breach.raid}% of its works, fewer their share; never more than ${SIEGE.breach.most}% in all` });
  }
  if (out.aa && m === 'town') out.lines.push({ label: 'Anti-aircraft guns', value: `Level ${out.aa} guns ring ${place}`, effect: `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing and bring down more of the bombers` });
  else if (out.aa) out.lines.push({ label: 'Anti-aircraft guns', value: `Level ${out.aa} guns ring the factories of ${place}`, effect: `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing and bring down more of the bombers` });
  if (out.ships.length) {
    const pc = (x: number): string => `${Math.round(x * 100)}%`;
    if (out.surprise) out.lines.push({ label: 'Surprise', value: 'War was declared this month', effect: `The harbour is caught unready: its fighters and guns count ${pc(CARRIERS.harbour.unready)}, and the hits are multiplied by ${CARRIERS.harbour.surprise}` });
    if (m === 'harbour') out.lines.push({ label: 'In harbour', value: 'Ships at anchor cannot manoeuvre', effect: `Each bomber that gets through does ${CARRIERS.harbour.hit} times the damage it would at sea` });
    if (shipGuns > 0) out.lines.push({ label: "Ships' guns", value: `Weight of anti-aircraft fire ${Math.round(shipGuns)}`, effect: `About ${pc(out.flak * I)} of the bombers brought down: battleships and carriers carry the most guns` });
    const decks = out.def.filter((x) => x.f.ship !== undefined);
    if (out.D >= 1) out.lines.push({ label: 'Fighter cover', value: decks.length ? `${planes(Math.round(decks.reduce((s, x) => s + x.f.ftr * x.w, 0)), 'carrier fighter')} over the ships${decks.length < out.def.length ? ', and fighters from the coast' : ''}` : 'Fighters from the coast', effect: `They break up about ${pc(out.spoil)} of the strike. An escort of fighters cuts that` });
    else out.lines.push({ label: 'Fighter cover', value: 'None', effect: 'Nothing breaks up the strike' });
    if (out.B > 0) {
      const land = out.att.filter((a) => a.bmr > 0 && a.f.ship === undefined), deckB = out.att.filter((a) => a.bmr > 0 && a.f.ship !== undefined);
      out.lines.push({ label: 'Crews', value: deckB.length && land.length ? 'Carrier crews and land bombers' : deckB.length ? 'Carrier crews, trained to hit ships' : 'Land bombers', effect: land.length ? `A land bomber counts ${pc(navalSkill(land[0].f))} of a carrier aircraft against ships${deckB.length ? `: the strike as a whole counts ${pc(out.naval)}` : ''}` : undefined });
    }
    if (out.again < 1) out.lines.push({ label: 'Already struck', value: 'The fleet has been struck this month', effect: `It is scattered and warned: this strike counts ${pc(out.again)}` });
    if (out.ships.some((x) => atSea(x) && fleetKnown(g, x, nat) !== 'exact')) out.lines.push({ label: 'Search', value: 'The ships are known only from a report', effect: `About ${pc(CARRIERS.strike.report)} of the strike finds them. Identify them first, by a search of that sea or by contact, for a full strike` });
  }
  if (df > 0 || out.radar) {
    out.lines.push(out.radar
      ? { label: 'Radar', value: `${cap(out.radar.name)} gives warning`, effect: 'Every defending fighter in reach gets up in time, and fewer aircraft are caught on the ground' }
      : { label: 'Radar', value: 'No radar warning', effect: `Only ${Math.round(AIR.radar.without * 100)}% of the defending fighters meet the raids` });
  }
  const kitA = techNotes(g, out.att.map((a) => a.f.nat), out.B > 0 ? ['fighters', 'bombers'] : ['fighters']), kitD = techNotes(g, out.def.map((x) => x.f.nat), ['fighters', 'radar']);
  if (kitA.length || kitD.length) {
    const up = (k: number): string => `${k > 0 ? 'up' : 'down'} ${Math.abs(k)}%`;
    out.lines.push({ label: 'Equipment', value: [...kitA, ...kitD].map((x) => x.text).join('; '), effect: [...kitA.map((x) => `Attacker ${up(x.pct)}`), ...kitD.map((x) => `Defender ${up(x.pct)}`)].join(', ') });
  }
  const names = (fs: AirFleet[]): string => list(fs.filter((f) => f.leader).map((f) => `${f.leader!.name}${f.leader!.level > 1 ? ` (level ${f.leader!.level})` : ''}`)) || 'no commander named';
  out.lines.push({ label: 'Commanders', value: `${names(out.att.map((a) => a.f))} against ${names(out.def.map((x) => x.f))}` });
  const worn = [...out.att.map((a) => a.f), ...out.def.map((x) => x.f)].filter((f) => f.ready < 0.8);
  if (worn.length) out.lines.push({ label: 'Readiness', value: `${list(worn.map((f) => f.name))} not fully fit to fly`, effect: 'A worn fleet fights below its strength' });
  return out;
}

/** The player's look at a mission before ordering it: this fleet together with any others already given the same orders. */
export function previewAir(g: Game, f: AirFleet, m: AirMission, target: number): AirAssess {
  const group = airFleetsOf(g, f.nat).filter((x) => x.id === f.id || (x.mission === m && x.target === target));
  return assessAir(g, group, m, target);
}

// ---- Flying it ------------------------------------------------------------------------------

function gainXp(f: AirFleet, won: boolean): void {
  const l = f.leader;
  if (!l) return;
  l.xp += won ? 2 : 1;
  if (l.level < AIR.leader.maxLevel && l.xp >= l.level * 3) { l.xp = 0; l.level++; }
}

/** How much a raid of this weight cuts the organisation of the troops in a region: less for every division there, and less in cover. */
/** The months of work that many bombers getting through take off the fieldworks of a line. */
export const lineWear = (through: number): number => Math.min(AIRWAR.line.most, Math.round((through / AIRWAR.line.scale) * 10) / 10);
const monthsWord = (m: number): string => { const v = Math.round(m * 10) / 10; return v === 1 ? 'a month' : `${v} months`; };

export function troopCut(r: Region, foes: Corps[], weight: number): number {
  const G = AIRWAR.ground;
  const divs = foes.reduce((s, c) => s + c.divs, 0);
  return divs > 0 ? Math.min(G.max, weight / (G.scale + G.perDiv * divs)) * (G.cover[r.terrain] ?? 1) : 0;
}

/** What a strike on a sea costs each enemy's trade: an ocean convoy route through it, or the coasters along an enemy shore. */
function tradeHurt(g: Game, nat: string, target: number, through: number): Record<string, number> {
  const S = AIR.damage.shipping;
  const zone = g.seas[target];
  const hurt: Record<string, number> = {};
  const weight = (through / (through + S.tradeScale)) * zone.air;
  for (const c of NAVY.convoys) if (atWar(g, nat, c.to) && c.zones.includes(zone.key)) hurt[c.to] = Math.max(hurt[c.to] ?? 0, (c.share / c.zones.length) * weight);
  for (const id of zone.coast) {
    const r = g.regions[id];
    if (r.port && r.owner === r.ctrl && atWar(g, nat, r.ctrl)) hurt[r.ctrl] = Math.max(hurt[r.ctrl] ?? 0, S.coastal * weight);
  }
  return hurt;
}

/**
 * Flies one air campaign and files its report. `salt` gives a raid planned and flown at once (the grand campaign's
 * air war) dice of its own, so that two raids on one target in a month do not fall out alike.
 */
export function fly(g: Game, nat: string, m: AirMission, target: number, fleets: AirFleet[], salt = ''): AirReport | null {
  const a = assessAir(g, fleets, m, target);
  if (a.why) return null;
  const C = AIR.combat, I = intensity(m);
  const sea = m === 'strike' || m === 'fleet', night = m === 'night';
  // The sea fought over: the one struck, or the one the fleet struck is in.
  const zone = m === 'fleet' ? a.ships[0].zone : target;
  const rng = airRng(g, `${m}:${target}:${nat}${salt}`);
  const luck = (): number => rng.range(1 - C.luck, 1 + C.luck);
  const before = (f: AirFleet): AirSide => {
    const side: AirSide = { nat: f.nat, fleet: f.name, id: f.id, base: f.base, leader: f.leader?.name ?? null, ftr: f.ftr, bmr: f.bmr, lostFtr: 0, lostBmr: 0 };
    if (f.ship !== undefined) { const p = groupAt(g, f); side.at = [p.x, p.y]; }
    return side;
  };
  const att = a.att.map((x) => before(x.f)), def = a.def.map((x) => before(x.f));

  // Fighter against fighter, then the defenders against the bombers.
  // Numbers tell, but less than in proportion: the smaller side picks its moments.
  const se = Math.sqrt(a.Epow), sd = Math.sqrt(a.Dpow);
  const share = se + sd > 0 ? se / (se + sd) : 0;
  const clash = Math.min(a.E, a.D) * clashOf(g, m);
  const dLoss = C.fighters * I * clash * 2 * share * luck();
  const eLoss = C.fighters * I * clash * 2 * (1 - share) * C.escortShare * luck();
  const bLoss = a.B * Math.min(mostBomberLoss(g), a.bomberLoss * luck());
  // Pilots who come down over their own ground mostly fly again; over the enemy's, or the sea, few do.
  const saved = (home: boolean): number => (sea ? (AIR.pilots.home + AIR.pilots.away) / 2 : home ? AIR.pilots.home : AIR.pilots.away);
  const down = (f: AirFleet, side: AirSide, n: number, home: boolean): void => {
    const k = Math.min(f.ftr, rng.roll(Math.max(0, n)));
    if (!k) return;
    const p = airPool(g, f.nat);
    const back = rng.roll(k * saved(home));
    f.ftr -= k; f.lost.ftr += k; side.lostFtr += k;
    p.lostFtr += k; p.pilots += back; p.lostPilots += k - back;
  };
  const bomber = (f: AirFleet, side: AirSide, n: number): void => {
    const k = Math.min(f.bmr, rng.roll(Math.max(0, n)));
    f.bmr -= k; f.lost.bmr += k; side.lostBmr += k;
    airPool(g, f.nat).lostBmr += k;
  };
  a.def.forEach((x, i) => down(x.f, def[i], a.D > 0 ? dLoss * ((x.f.ftr * x.w) / a.D) : 0, true));
  a.att.forEach((x, i) => {
    down(x.f, att[i], a.E > 0 ? eLoss * (x.ftr / a.E) : 0, false);
    bomber(x.f, att[i], a.B > 0 ? bLoss * (x.bmr / a.B) : 0);
  });
  const lostB = att.reduce((s, x) => s + x.lostBmr, 0);
  const through = Math.max(0, a.B - lostB) * (night ? C.night.accuracy : 1) * techMul(g, nat, 'bombers');

  // What the bombers did.
  let result = '', value = 0, ground = 0, shipsSunk = 0, notable = false;
  let struck: SeaSide[] | undefined;
  if (m === 'fleet' || m === 'harbour') {
    // A strike on ships: the hits fall on the classes the crews aim for, and each fleet under attack takes its share.
    const exp = shipHits(a, m, through, foundShare(g, a, nat));
    const have = noShips();
    for (const x of a.ships) for (const c of SHIPS) have[c] += x.ships[c];
    const afloatBefore = a.ships.reduce((s, x) => s + surfaceShips(x), 0);
    const total = noShips();
    struck = a.ships.map((x) => ({ nat: x.nat, fleet: x.name, admiral: x.admiral?.name ?? null, ships: { ...x.ships }, sunk: noShips() }));
    a.ships.forEach((x, i) => {
      for (const c of SHIPS) {
        const k = have[c] > 0 ? Math.min(x.ships[c], rng.roll(exp.sunk[c] * (x.ships[c] / have[c]))) : 0;
        x.ships[c] -= k; x.lost[c] += k; struck![i].sunk[c] = k; total[c] += k;
      }
      // Ships that are hit and not sunk limp home: the fleet is the less fit for sea.
      x.ready = Math.max(0, x.ready - Math.min(0.3, 0.05 + CARRIERS.strike.ready * (afloatBefore > 0 ? exp.hits / afloatBefore : 0)));
      x.struck = [g.turn, (x.struck && x.struck[0] === g.turn ? x.struck[1] : 0) + 1, atSea(x) ? x.zone : -1];
      // It has been seen by those who struck it, and the strike gives away the decks it came from.
      (x.seen ??= {})[nat] = g.turn;
      for (const y of a.att) { const c = carrierOf(g, y.f); if (c && atSea(c)) (c.seen ??= {})[x.nat] = g.turn; }
    });
    shipsSunk = shipCount(total);
    value += SHIPS.reduce((s, c) => s + total[c] * CARRIERS.strike.value[c], 0);
    const whom = a.ships.length === 1 ? `the ${a.ships[0].name}` : `the ${a.ships.length} fleets`;
    result = through <= 0 ? 'No bombers got through.'
      : shipsSunk ? `${cap(shipList(total))} of ${whom} sunk${m === 'harbour' ? ' at anchor' : ''}.`
      : exp.hits >= 0.5 ? `Ships of ${whom} were hit, but none was sunk.` : `The bombers did ${whom} little harm.`;
    // A carrier that goes down takes most of its aircraft with it.
    if (total.cv) syncWings(g);
  } else if (m === 'superiority') {
    const A = AIR.damage.airfields;
    const hit = g.airFleets.filter((f) => f.base === target && f.ship === undefined && atWar(g, f.nat, nat));
    const cut = Math.min(A.max, through / A.scale);
    for (const f of hit) {
      f.ready = Math.max(AIR.ready.floor, f.ready - cut);
      const all = f.ftr + f.bmr;
      const n = Math.min(all * A.maxGround, (through * A.ground * (a.radar ? a.radar.ground : 1)) / hit.length);
      const gf = all ? Math.min(f.ftr, rng.roll(n * (f.ftr / all))) : 0, gb = all ? Math.min(f.bmr, rng.roll(n * (f.bmr / all))) : 0;
      const p = airPool(g, f.nat);
      f.ftr -= gf; f.bmr -= gb; f.lost.ftr += gf; f.lost.bmr += gb;
      p.lostFtr += gf; p.lostBmr += gb; p.pilots += gf;
      const side = def.find((s) => s.fleet === f.name && s.nat === f.nat);
      if (side) { side.lostFtr += gf; side.lostBmr += gb; }
      ground += gf + gb;
      value += cut * 300;
    }
    result = through <= 0 ? '' : hit.length
      ? `The airfields of ${list(hit.map((f) => f.name))} were bombed: readiness cut by ${Math.round(cut * 100)}%${ground ? ` and ${planes(ground, 'aircraft').replace('aircrafts', 'aircraft')} destroyed on the ground` : ''}.`
      : 'The bombers found no airfields in use.';
  } else if (m === 'bomb' || night) {
    const N = AIR.damage.industry;
    const r = g.regions[target];
    const total = g.regions.reduce((s, x) => s + (x.ctrl === r.ctrl ? x.fac : 0), 0);
    const open = Math.min(N.max, through / N.scale);
    const cut = a.aa ? open * (1 - a.aa * WORKS.aa.cut) : open;
    const nation = total > 0 ? cut * (r.fac / total) : 0;
    raid(g, nat, r.ctrl, nation);
    // The air war: a province with no industry has no output to stop, and the raid is judged on its railway alone.
    value += airWar(g) && r.fac <= 0 ? 0 : cut * 1000;
    result = r.fac > 0 ? `The bombing stopped ${Math.round(cut * 100)}% of ${r.name}'s output for a month: ${(nation * 100).toFixed(1)}% of ${g.nations[r.ctrl].adj} production.` : '';
    // The grand campaign: a heavy raid wrecks a factory outright, and the bombs that fall on the yards and
    // junctions may cut the province's railway. Both stay as they are until engineers come.
    if (bombFactories(g, nat, r, cut)) { value += 300; notable = true; result += ` A factory was wrecked outright: it earns nothing until engineers rebuild it.`; }
    if (bombRail(g, nat, target, through, a.aa ? a.aa * WORKS.aa.cut : 0)) { value += 300; notable = true; result += ` The railway through ${r.name} is cut: no train runs until engineers relay it.`; }
    else if (!result) result = through > 0 ? `The bombs fell on the rail yards of ${r.name}, but the line still runs.` : 'No bombers got through.';
    result = result.trim();
  } else if (m === 'town') {
    // A raid on a town: part of its works are broken down for the assault, and the troops inside are shaken.
    const r = g.regions[target];
    const hit = hitTown(g, r, 'raid', townShare('raid', through) * (a.aa ? 1 - a.aa * WORKS.aa.cut : 1));
    value += hit.share * hit.divs * SIEGE.bombard.value;
    result = through > 0 ? townHitText(g, r, hit, 'bombs', true) : 'No bombers got through.';
  } else if (m === 'line') {
    // Bombing the line: the works of the troops dug in there lose months of digging, before the army goes in.
    const r = g.regions[target];
    const months = lineWear(through);
    let divs = 0, held = 0;
    for (const c of r.units) {
      if (c.dead || !atWar(g, nat, c.nat) || worksOf(c) <= 0) continue;
      held = Math.max(held, worksOf(c));
      c.dugFor = Math.max(0, Math.round((worksOf(c) - months) * 10) / 10);
      divs += c.divs;
    }
    value += months * divs * AIRWAR.line.value;
    const took = Math.min(months, held), left = Math.max(0, held - months);
    result = took >= 0.05
      ? `The bombing took ${monthsWord(took)} off the fieldworks at ${r.name}, which leaves ${left < 0.05 ? 'nothing of them' : `${monthsWord(left)} of work in them`}. The troops there dig them back a month at a time: attack while it lasts.`
      : 'Too few bombers got through to break up the works.';
  } else if (m === 'ground') {
    // A raid on troops: the army there is harried for a month, and goes into its next battle the weaker for it.
    const G = AIRWAR.ground;
    const r = g.regions[target];
    const foes = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));
    // Ground fire takes its share of the fighters that come down low.
    a.att.forEach((x, i) => down(x.f, att[i], x.ftr * G.fire * luck(), false));
    const low = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd, 0);
    const cut = troopCut(r, foes, through + low * G.fighters);
    let divs = 0;
    for (const c of foes) {
      const k = cut * (c.dug ? G.dug : 1);
      c.org = Math.max(Math.min(c.org, G.floor), c.org - k);
      c.str = Math.max(0.05, c.str * (1 - k * G.str));
      divs += c.divs;
    }
    value += cut * divs * G.value;
    result = cut > 0
      ? `The raids cut the organisation of ${foes.length === 1 ? foes[0].name : `the ${foes.length} corps`} at ${r.name} by ${Math.round(cut * 100)}%${foes.some((c) => c.dug) ? ', less where they are dug in' : ''}. They fight the weaker for it until they have rested.`
      : 'The aircraft found little to hit.';
  } else {
    const S = AIR.damage.shipping;
    const frac = Math.min(S.maxSink, S.sink * (through / (through + S.scale)));
    const none = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
    const sunk = none();
    for (const f of g.fleets) {
      if (f.zone !== target || !atSea(f) || !atWar(g, nat, f.nat) || !surfaceShips(f)) continue;
      const s = sinkShips(f, frac, rng, [...SURFACE]);
      f.ready = Math.max(0, f.ready - 0.1);
      for (const c of SURFACE) sunk[c] += s[c];
    }
    if (sunk.cv && carrierRules(g)) syncWings(g);
    // Trade: an ocean convoy route through this sea, or the coasters along an enemy shore.
    const hurt = tradeHurt(g, nat, target, through);
    for (const [victim, s] of Object.entries(hurt)) raid(g, nat, victim, s);
    const worst = Object.entries(hurt).sort((x, y) => y[1] - x[1])[0];
    shipsSunk = shipCount(sunk);
    value += shipCount(sunk) * 30 + (worst ? worst[1] * 3000 : 0);
    result = [shipCount(sunk) ? `${cap(shipList(sunk))} sunk.` : '', worst ? `Merchant shipping losses cost ${g.nations[worst[0]].name} ${(worst[1] * 100).toFixed(1)}% of a month's production.` : ''].filter(Boolean).join(' ') || 'The bombers found little to hit.';
  }

  // Has the sky been won? The attacker's surviving fighters against what the defender can put up once his losses are made good.
  let sky = false, hold = 0;
  if (m === 'superiority') {
    const Eafter = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd, 0);
    const EpowAfter = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd * worth(g, x.f), 0);
    const lostBy: Record<string, number> = {};
    def.forEach((s) => { lostBy[s.nat] = (lostBy[s.nat] ?? 0) + s.lostFtr; });
    let Dnext = 0;
    a.def.forEach((x, i) => {
      const p = airPool(g, x.f.nat);
      const built = row(AIR.production[x.f.nat], g.turn + 1), trained = row(AIR.pilots.perMonth[x.f.nat], g.turn + 1);
      const can = Math.min(p.ftr + (built ? built[1] : 0), p.pilots + (trained ? trained[1] : 0));
      const lost = lostBy[x.f.nat] ?? 0;
      const back = lost > 0 ? Math.min(lost, can) * (def[i].lostFtr / lost) : 0;
      Dnext += Math.min(x.f.est.ftr, x.f.ftr + back) * x.w * worth(g, x.f) * techMul(g, x.f.nat, 'radar');
    });
    hold = Dnext > 0 ? EpowAfter / Dnext : 99;
    sky = Eafter >= AIR.sky.least && hold >= AIR.sky.win;
    if (sky) g.sky[target] = { nat, turn: g.turn };
  }

  // Wear, and what the commanders learned.
  const aLost = att.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
  const dLost = def.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
  const ratio = (dLost + value + 10) / (aLost + 10);
  const won = sky || ratio >= 1;
  for (const x of a.att) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - (night ? AIR.ready.nightSortie : AIR.ready.sortie)); gainXp(x.f, won); }
  for (const x of a.def) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - AIR.ready.defend * I * (night ? 0.2 : 1)); gainXp(x.f, !won); }
  const level: VictoryLevel = sky || ratio >= 2 ? 'Decisive victory' : ratio >= 1 ? 'Close victory' : ratio >= 0.5 ? 'Close defeat' : 'Decisive defeat';

  g.flags.airBattles = (g.flags.airBattles ?? 0) + 1;
  const troops = m === 'ground' || m === 'line' ? g.regions[target].units.find((u) => !u.dead && atWar(g, nat, u.nat))?.nat : undefined;
  const defNat = troops ?? struck?.[0]?.nat ?? def[0]?.nat ?? (sea ? '' : g.regions[target].ctrl);
  const rep: AirReport = {
    id: g.flags.airBattles, turn: g.turn, mission: m, region: sea ? -1 : target, zone: sea ? zone : -1, place: a.place,
    attNat: nat, defNat, att, def, lines: a.lines, odds: a.Dpow > 0 ? a.Epow / a.Dpow : 9, level, won, sky, hold, result,
  };
  if (struck) { rep.ships = struck; if (m === 'fleet') rep.fleet = target; if (a.surprise) rep.surprise = true; }
  // The grand campaign keeps what the raid was worth beside what it cost: a staff learns from its own results.
  if (g.flags.open === 1) rep.worth = Math.round(value);
  g.airReports.push(rep);
  storyAir(g, m === 'fleet' ? 'strike' : m === 'harbour' || m === 'town' ? 'bomb' : m, sea ? zone : target, a.att.map((x) => x.f), a.def.map((x) => x.f), { sky, sunk: shipsSunk, attLost: aLost, defLost: dLost });

  const who = list(fleets.filter((f) => a.att.some((x) => x.f === f)).map((f) => f.name));
  const cost = `${planes(aLost, 'aircraft').replace('aircrafts', 'aircraft')} lost`;
  const adj = defNat ? g.nations[defNat].adj : 'enemy';
  // Once the British and American air forces fight the same enemy side by side, the sky they win is the Allies'.
  const together = !!defNat && (nat === 'UK' || nat === 'USA') && ['UK', 'USA'].every((x) => g.nations[x]?.alive && g.nations[x].atWar.includes(defNat));
  let text: string;
  if (m === 'superiority') text = `${who} ${a.att.length === 1 ? 'fights' : 'fight'} for the sky over ${a.place}: ${dLost} ${adj} aircraft destroyed, ${cost}. ${sky ? (together ? 'The Allied air forces now hold the sky there.' : `The ${g.nations[nat].adj} air force now holds the sky there.`) : df(def) ? 'The defenders still hold their sky.' : 'No fighters came up to meet them.'}`;
  else if (m === 'fleet') text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} the ${a.ships[0].name} in ${a.place}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'harbour') text = `${who} ${a.att.length === 1 ? 'raids' : 'raid'} the harbour of ${a.place}${a.surprise ? ' without warning' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'town') text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} the defences of ${townName(g.regions[target])}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (sea) text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} at shipping in ${a.place}. ${result} ${cap(cost)}.`;
  else if (m === 'line') text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} the ${adj} fieldworks at ${a.place}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'ground') text = `${who} ${a.att.length === 1 ? 'raids' : 'raid'} the ${adj} troops at ${a.place}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} ${a.place}${night ? ' by night' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  // A fight for the sky in which nobody met anybody is not news (the grand campaign).
  if (g.flags.open === 1 && m === 'superiority' && !sky && aLost + dLost === 0) return rep;
  // What changes something is told in full: the sky won, a ship sunk, a factory wrecked, a railway cut, a fleet or
  // a harbour struck. The same raid on the same place month after month goes into the month's summary line.
  const full = sky || notable || shipsSunk > 0 || m === 'fleet' || m === 'harbour';
  const brief = m === 'superiority' ? `a fight for the sky over ${a.place}`
    : m === 'town' ? `the defences of ${townName(g.regions[target])} bombed`
    : sea ? `shipping struck in ${a.place}`
    : m === 'line' ? `the ${adj} fieldworks at ${a.place} bombed`
    : m === 'ground' ? `the ${adj} troops at ${a.place} raided`
    : `${a.place} bombed${night ? ' by night' : ''}`;
  if (full) headline(g, 'battle', text, nat, sea ? undefined : target);
  else routineNews(g, `air:${nat}:${m}:${sea ? `z${zone}` : target}`, 'battle', text, `${brief}${aLost ? ` (${aLost} aircraft lost)` : ''}`, nat, sea ? undefined : target);
  return rep;
}

const df = (sides: AirSide[]): number => sides.reduce((s, x) => s + x.ftr, 0);

/** The air campaigns a nation ordered this month, flown once all its orders are given. */
export function airBattles(g: Game, nat: string): AirReport[] {
  const out: AirReport[] = [];
  if (!g.nations[nat]?.alive) return out;
  // The air war: a fighter group ordered to stick with a bomber group goes on the raid the bombers were left to fly.
  const followers: AirFleet[] = [];
  if (airWar(g)) for (const f of airFleetsOf(g, nat)) {
    if (f.escorts === undefined || f.mission !== 'support' || f.flew === g.turn || f.ftr <= 0) continue;
    const b = g.airFleets.find((x) => x.id === f.escorts && x.nat === nat);
    if (!b || b.flew === g.turn || !isOffensive(b.mission) || whyNotMission(g, f, b.mission, b.target)) continue;
    f.mission = b.mission;
    f.target = b.target;
    followers.push(f);
  }
  const groups = new Map<string, AirFleet[]>();
  for (const f of airFleetsOf(g, nat)) {
    // A group that has flown its raid this month (planned and flown at once in the air war) does not fly again.
    if (f.flew === g.turn || !isOffensive(f.mission) || whyNotMission(g, f, f.mission, f.target)) continue;
    const k = `${OFFENSIVE.indexOf(f.mission)}:${f.target}`;
    if (!groups.has(k)) groups.set(k, []);
    groups.get(k)!.push(f);
  }
  // The fight for the sky comes first, so that bombed airfields tell on the raids that follow.
  for (const k of [...groups.keys()].sort()) {
    const fleets = groups.get(k)!;
    const rep = fly(g, nat, fleets[0].mission, fleets[0].target, fleets);
    if (rep) out.push(rep);
  }
  // The escorts are back on patrol next month.
  for (const f of followers) f.flew = g.turn;
  return out;
}

// ---- The computer's air staff ----------------------------------------------------------------

/** Orders for one computer-run nation's air fleets: history's plan where it has one, otherwise support for the armies. */
export function airAI(g: Game, nat: string): void {
  if (nat === g.human) return;
  const n = g.nations[nat];
  const mine = airFleetsOf(g, nat);
  // A group that has flown this month already stays away until the next.
  for (const f of mine) if (f.flew !== g.turn) { f.mission = 'support'; f.target = -1; }
  if (!n.alive || !n.atWar.length) return;
  // In the grand campaign history's air plan does not apply: the air groups patrol, and the staff plans raids of its own.
  // Ships come first: the carriers' wings, and a few of the land bombers, go for the fleets the staff has found.
  if (g.flags.open === 1) { seaStaff(g, nat); raidStaff(g, nat, mine); return; }
  for (const ph of AIR.plan) {
    if (ph.nat !== nat || g.turn < ph.from || g.turn > ph.to || !atWar(g, nat, ph.against)) continue;
    for (const o of ph.orders) {
      const f = mine.find((x) => x.name === o.fleet);
      if (!f) continue;
      const bases = (o.base ?? []).map((b) => g.byName[b]).filter(Boolean);
      if (bases.length && !bases.some((r) => r.id === f.base)) for (const r of bases) if (rebaseAir(g, f, r)) break;
      const target = o.mission === 'strike' ? zoneOf(g, o.sea ?? '') : o.region ? g.byName[o.region]?.id ?? -1 : -1;
      setMission(g, f, o.mission, target);
    }
  }
}

/**
 * The grand campaign: a computer nation's air staff plans raids of its own and flies them at once, as the player's
 * are flown. Each month a share of its bomber groups goes out, each with the fighter groups on its own airfield as
 * escort by day: against the factories and railway junctions of its enemies, by night where the day would cost too
 * much, or against the enemy armies that face its own. Its fighters may also fight for the sky over a stretch of
 * front where they expect to win it. A raid is flown only where the forecast says it should pay and the bombers'
 * losses stay within what the staff will bear. A rival on the harder levels sends more, bears more, and goes for
 * the player first; on the easier levels it sends fewer.
 */
function raidStaff(g: Game, nat: string, mine: AirFleet[]): void {
  if (!airWar(g)) return;
  const R = AIRWAR.ai.raids;
  const B = bombingFor(g, nat);
  const rules = B ? { ...R, ...B } : { ...R, share: R.share * raidsMul(g, nat) };
  const you = B ? seat(g) : null;
  const rng = airRng(g, `staff:${nat}`);
  const used = new Set<number>();
  const fit = (f: AirFleet): boolean => !used.has(f.id) && f.flew !== g.turn && f.mission === 'support';
  const leads = (): AirFleet[] => mine.filter((f) => fit(f) && f.kind !== 'ftr' && f.bmr >= rules.least && f.ready >= R.ready);
  const fightersAt = (base: number): AirFleet[] => mine.filter((x) => fit(x) && x.kind === 'ftr' && x.ftr > 0 && homeOf(x) === base && x.ready >= AIRWAR.ai.escortReady);
  // The front: enemy-held ground with enemy troops on it, beside this nation's own field corps, and how many of its divisions stand against each.
  const front = new Map<number, number>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort') continue;
    for (const j of g.regions[c.reg].nb) {
      const r = g.regions[j];
      if (atWar(g, nat, r.ctrl) && r.units.some((u) => !u.dead && atWar(g, nat, u.nat))) front.set(j, (front.get(j) ?? 0) + c.divs);
    }
  }
  const foes = (r: Region): number => r.units.reduce((s, u) => s + (!u.dead && atWar(g, nat, u.nat) ? u.divs : 0), 0);
  // The air force comes to a front only where the armies are fighting: a quiet front stays quiet in the air too.
  // A stretch of front is live where a battle has been fought on it or beside it since last month, or where this
  // nation's staff has an offensive under way against the troops that hold it.
  const fought = new Set<number>();
  for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn >= g.turn - 1; i--) {
    const b = g.reports[i];
    if (b.yielded || !(allied(g, b.attNat, nat) || allied(g, b.defNat, nat))) continue;
    fought.add(b.region);
    for (const j of g.regions[b.region].nb) fought.add(j);
  }
  const pushing = (e: string): boolean => Object.keys(g.flags).some((k) => k.startsWith(`st:on:${nat}:${e}`));
  const against = new Map<string, boolean>();
  const pushed = (t: number): boolean => g.regions[t].units.some((u) => {
    if (u.dead || !atWar(g, nat, u.nat)) return false;
    if (!against.has(u.nat)) against.set(u.nat, pushing(u.nat));
    return against.get(u.nat)!;
  });
  for (const t of [...front.keys()]) if (!fought.has(t) && !pushed(t)) front.delete(t);
  // One raid on a place in a month: the staff spreads its blows.
  const hit = new Set<number>();
  // A raid on factories that keeps costing more than it gains is given up: after so many poor trades running the
  // staff leaves that target alone for a while. A wrecked factory or a cut railway counts as a good month.
  const S = R.sour;
  const soured = (m: AirMission, t: number): boolean => !!S && g.turn < (g.flags[`rso:${nat}:${m}:${t}`] ?? 0);
  const send = (m: AirMission, t: number, fleets: AirFleet[], k: number): void => {
    const rep = fly(g, nat, m, t, fleets, `:staff${k}`);
    hit.add(t);
    for (const f of fleets) { used.add(f.id); if (rep) { f.mission = m; f.target = t; f.flew = g.turn; } }
    if (!rep || !S || (m !== 'bomb' && m !== 'night')) return;
    const lost = rep.att.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
    const key = `rs:${nat}:${m}:${t}`;
    if (lost < S.dear || (rep.worth ?? 0) >= S.trade * lost) { delete g.flags[key]; return; }
    g.flags[key] = (g.flags[key] ?? 0) + 1;
    if (g.flags[key] >= S.times) { delete g.flags[key]; g.flags[`rso:${nat}:${m}:${t}`] = g.turn + S.rest; }
  };

  // A fight for the sky over the stretch of front where most of the army stands, if the fighters should win it:
  // flown only for an offensive of the staff's own, to give its army the air where it attacks.
  let flown = 0;
  for (let k = 0; k < R.sweeps && front.size; k++) {
    let best: { t: number; fleets: AirFleet[]; v: number } | null = null;
    const bases = [...new Set(mine.filter((f) => fit(f) && f.kind === 'ftr' && f.ftr > 0 && f.ready >= R.ready).map(homeOf))];
    const spots = [...front.entries()].filter(([t]) => !hit.has(t) && pushed(t) && skyOver(g, t) === null).sort((a, b) => b[1] - a[1] || a[0] - b[0]).slice(0, R.look);
    for (const base of bases) {
      for (const [t, divs] of spots) {
        const fleets = fightersAt(base).filter((f) => f.ready >= R.ready && whyNotMission(g, f, 'superiority', t) === null);
        if (!fleets.length) continue;
        const a = assessAir(g, fleets, 'superiority', t);
        if (a.why || a.E < AIR.sky.least) continue;
        const fc = forecastAir(g, a, 'superiority', t);
        if (fc.odds < AIR.sky.win * R.sky) continue;
        const v = R.skyWorth * Math.min(1, divs / 6) + fc.foeLoss - fc.ftrLoss;
        if (v > R.worth && (!best || v > best.v)) best = { t, fleets, v };
      }
    }
    if (!best) break;
    send('superiority', best.t, best.fleets, flown++);
  }

  // The bombers: so many raids a month as the staff's share of the groups fit to fly, taken to the nearest whole raid by the luck of the month.
  const most = Math.floor(leads().length * rules.share + rng.range(0, 1));
  for (let k = 0; k < most; k++) {
    const pick: { best: { m: AirMission; t: number; fleets: AirFleet[]; v: number } | null } = { best: null };
    const seen = new Set<number>();
    for (const f of leads().sort((a, b) => b.bmr - a.bmr || a.id - b.id)) {
      // One look from each airfield: its strongest bomber group, with the fighters that share the field.
      if (seen.has(homeOf(f))) continue;
      seen.add(homeOf(f));
      const escorts = fightersAt(homeOf(f));
      const weigh = (m: AirMission, r: Region, fleets: AirFleet[]): void => {
        if (soured(m, r.id)) return;
        const a = assessAir(g, fleets, m, r.id);
        if (a.why) return;
        const fc = forecastAir(g, a, m, r.id);
        if (fc.bomberLoss > rules.loss || fc.success < R.success) return;
        // Factories stopped, a factory wrecked, a junction cut or an army shaken, against the aircraft it should cost.
        const railWorth = (m === 'bomb' || m === 'night') && r.fac <= 0 ? rules.rail : 1;
        // (A town its own army stands round comes first with the staff: the assault waits on the bombers.)
        const v = (fc.worth * railWorth * (m === 'town' ? SIEGE.ai.raid : 1) + fc.foeLoss) * (you !== null && r.ctrl === you ? rules.focus ?? 1 : 1) - fc.ftrLoss - fc.bmrLoss;
        if (v > R.worth && (!pick.best || v > pick.best.v)) pick.best = { m, t: r.id, fleets, v };
      };
      const works = g.regions
        .filter((r) => !hit.has(r.id) && atWar(g, nat, r.ctrl) && (r.fac > 0 || railsAt(r.id).length >= 3) && whyNotMission(g, f, 'bomb', r.id) === null)
        .sort((a, b) => b.fac + rules.rail * railsAt(b.id).length - (a.fac + rules.rail * railsAt(a.id).length) || a.id - b.id)
        .slice(0, R.look);
      for (const r of works) {
        weigh('bomb', r, [f, ...escorts.filter((e) => whyNotMission(g, e, 'bomb', r.id) === null)]);
        weigh('night', r, [f]);
      }
      const troops = [...front.keys()].map((t) => g.regions[t])
        .filter((r) => !hit.has(r.id) && whyNotMission(g, f, 'ground', r.id) === null)
        .sort((a, b) => (front.get(b.id) ?? 0) * foes(b) - (front.get(a.id) ?? 0) * foes(a) || a.id - b.id)
        .slice(0, R.look);
      for (const r of troops) weigh('ground', r, [f, ...escorts.filter((e) => whyNotMission(g, e, 'ground', r.id) === null)]);
      // A town its side's army stands round: the bombers break down the works for the assault.
      if (siegeRules(g)) {
        for (const r of g.regions) {
          if (hit.has(r.id) || !besieged(g, r) || !allied(g, nat, besiegerOf(g, r) ?? '') || breachOf(g, r) * 100 > SIEGE.breach.most - 10 || whyNotMission(g, f, 'town', r.id) !== null) continue;
          weigh('town', r, [f, ...escorts.filter((e) => whyNotMission(g, e, 'town', r.id) === null)]);
        }
      }
    }
    if (!pick.best) break;
    send(pick.best.m, pick.best.t, pick.best.fleets, flown++);
  }
}

/** What history's plan has this nation's air fleets doing this month, if anything. */
export function airPlanFor(g: Game, nat: string, turn = g.turn): AirPlan | null {
  if (g.flags.open === 1) return null;
  return AIR.plan.find((p) => p.nat === nat && turn >= p.from && turn <= p.to && atWar(g, nat, p.against)) ?? null;
}

/** The name history gives this month's air operations, if the plan has one for this nation. */
export function airPlanName(g: Game, nat: string): string | null {
  return airPlanFor(g, nat)?.name ?? null;
}

/** New aircraft and pilots a nation gets each month at this point in the war. */
export function airDeliveries(g: Game, nat: string): { ftr: number; bmr: number; pilots: number } {
  const built = row(AIR.production[nat], g.turn), trained = row(AIR.pilots.perMonth[nat], g.turn);
  return { ftr: built ? built[1] : 0, bmr: built ? built[2] : 0, pilots: trained ? trained[1] : 0 };
}

/** What a mission is likely to do: the odds, the losses to expect on both sides, what gets done, and the chance it is a success. */
export interface AirForecast {
  /** The fighter odds going in. */
  odds: number;
  /** The share of the bombers likely to be lost. */
  bomberLoss: number;
  /** Readiness lost by the groups based there (a fight for the sky), the region's output stopped (bombing), the share of ships sunk (a strike), or the organisation the troops lose (a raid on troops). */
  cut: number;
  /** The share of the victim's whole production stopped by bombing. */
  nation: number;
  /** The grand campaign: the chance of cutting the province's railway, and of wrecking a factory outright. */
  rail?: number;
  ruin?: number;
  /** Aircraft each side is likely to lose. */
  ftrLoss: number;
  bmrLoss: number;
  foeLoss: number;
  /** The chance the raid costs the enemy more than it costs the raiders. */
  success: number;
  /** What the raid should achieve beyond aircraft shot down, counted in aircraft: output stopped, a railway cut, a factory wrecked, an army shaken. */
  worth: number;
  /** A strike on ships: the ships of each class it should sink, and the hits it should score. */
  ships?: Ships;
  hits?: number;
  /** A raid on a town: what it should do to the works and to the troops inside. */
  town?: TownHit;
}

/** The share of a strike that finds the ships it is sent against: all of it where they are identified, less where they are known only from a report. */
export function foundShare(g: Game, a: AirAssess, nat: string): number {
  // A harbour does not move: only ships at sea have to be found.
  return a.ships.some((x) => atSea(x) && fleetKnown(g, x, nat) !== 'exact') ? CARRIERS.strike.report : 1;
}

/**
 * What a mission is likely to do, without the dice, for the player to see before he gives the order:
 * the fighter odds going in, the share of the bombers likely to be lost, and what those that get through should achieve.
 */
export function forecastAir(g: Game, a: AirAssess, m: AirMission, target: number): AirForecast {
  const C = AIR.combat, I = intensity(m);
  const nat = a.att[0]?.f.nat ?? '';
  const through = a.B * (1 - a.bomberLoss) * (m === 'night' ? C.night.accuracy : 1) * techMul(g, nat, 'bombers');
  const odds = a.Dpow > 0 ? a.Epow / a.Dpow : 9;
  // The fighter battle as the flight settles it, at even luck.
  const se = Math.sqrt(a.Epow), sd = Math.sqrt(a.Dpow);
  const share = se + sd > 0 ? se / (se + sd) : 0;
  const clash = Math.min(a.E, a.D) * clashOf(g, m);
  const foeLoss = C.fighters * I * clash * 2 * share;
  let ftrLoss = C.fighters * I * clash * 2 * (1 - share) * C.escortShare;
  const bmrLoss = a.B * a.bomberLoss;
  let cut = 0, nation = 0, value = 0;
  let rail: number | undefined, ruin: number | undefined;
  let ships: Ships | undefined, hits: number | undefined;
  let lumps: [number, number][] | undefined, expected = 0;
  let town: TownHit | undefined;
  if (m === 'superiority') {
    const based = g.airFleets.filter((f) => f.base === target && f.ship === undefined && atWar(g, f.nat, nat)).length;
    cut = based ? Math.min(AIR.damage.airfields.max, through / AIR.damage.airfields.scale) : 0;
    value = cut * 300 * based;
  } else if (m === 'bomb' || m === 'night') {
    const r = g.regions[target];
    const total = g.regions.reduce((s, x) => s + (x.ctrl === r.ctrl ? x.fac : 0), 0);
    cut = Math.min(AIR.damage.industry.max, through / AIR.damage.industry.scale);
    if (a.aa) cut *= 1 - a.aa * WORKS.aa.cut;
    nation = total > 0 ? cut * (r.fac / total) : 0;
    value = airWar(g) && r.fac <= 0 ? 0 : cut * 1000;
    // The grand campaign: the chance of cutting the province's railway, and of wrecking a factory outright.
    if (groundRules(g)) {
      rail = railBombChance(g, target, through, a.aa ? a.aa * WORKS.aa.cut : 0);
      ruin = ruinChance(g, r, cut);
      if (r.fac <= 0) cut = 0;
    }
  } else if (m === 'strike') {
    cut = Math.min(AIR.damage.shipping.maxSink, AIR.damage.shipping.sink * (through / (through + AIR.damage.shipping.scale)));
    const afloat = g.fleets.reduce((s, f) => s + (f.zone === target && atSea(f) && atWar(g, nat, f.nat) ? surfaceShips(f) : 0), 0);
    const worst = Math.max(0, ...Object.values(tradeHurt(g, nat, target, through)));
    value = cut * afloat * 30 + worst * 3000;
  } else if (m === 'fleet' || m === 'harbour') {
    const exp = shipHits(a, m, through, foundShare(g, a, nat));
    const there = a.ships.reduce((s, x) => s + surfaceShips(x), 0);
    cut = there > 0 ? exp.count / there : 0;
    ships = exp.sunk;
    hits = exp.hits;
    // Ships go down whole. What is sure to sink counts for its worth; the odd fraction of each class is a throw of the dice.
    const V = CARRIERS.strike.value;
    value = SHIPS.reduce((s, c) => s + Math.floor(exp.sunk[c]) * V[c], 0);
    lumps = SHIPS.filter((c) => exp.sunk[c] % 1 > 0.001).map((c): [number, number] => [exp.sunk[c] % 1, V[c]]);
    expected = exp.value;
  } else if (m === 'town') {
    town = planTownHit(g, g.regions[target], 'raid', townShare('raid', through) * (a.aa ? 1 - a.aa * WORKS.aa.cut : 1));
    cut = town.share;
    value = town.share * town.divs * SIEGE.bombard.value;
  } else if (m === 'line') {
    cut = lineWear(through);
    value = cut * g.regions[target].units.reduce((s, c) => s + (!c.dead && atWar(g, nat, c.nat) && worksOf(c) > 0 ? c.divs : 0), 0) * AIRWAR.line.value;
  } else if (m === 'ground') {
    const G = AIRWAR.ground;
    const r = g.regions[target];
    const foes = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));
    const fire = a.E * G.fire;
    ftrLoss += fire;
    cut = troopCut(r, foes, through + Math.max(0, a.E - ftrLoss) * G.fighters);
    value = cut * foes.reduce((s, c) => s + c.divs, 0) * G.value;
  }
  // The chance of success: a raid succeeds when it costs the enemy more than it costs the raiders. A cut railway
  // and a wrecked factory are each worth a great deal and each a throw of the dice, so the raid is weighed with
  // and without them; the luck of the day spreads the rest.
  const k = AIRWAR.success.k;
  const cost = ftrLoss + bmrLoss + 10;
  const beats = (worth: number): number => { const x = Math.pow((foeLoss + worth + 10) / cost, k); return x / (1 + x); };
  const pr = rail ?? 0, pf = ruin ?? 0;
  let success = (1 - pr) * (1 - pf) * beats(value) + pr * (1 - pf) * beats(value + 300) + (1 - pr) * pf * beats(value + 300) + pr * pf * beats(value + 600);
  if (lumps) {
    // A strike on ships is weighed over every way the dice can fall, class by class.
    success = 0;
    for (let mask = 0; mask < 1 << lumps.length; mask++) {
      let p = 1, w = value;
      lumps.forEach(([q, v], i) => { if ((mask >> i) & 1) { p *= q; w += v; } else p *= 1 - q; });
      success += p * beats(w);
    }
    value = expected;
  }
  return { odds, bomberLoss: a.bomberLoss, cut, nation, rail, ruin, ftrLoss, bmrLoss, foeLoss, success, worth: value + 300 * (pr + pf), ships, hits, town };
}
