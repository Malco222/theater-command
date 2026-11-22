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

import { byHand, staffRuns } from './depth';
import data from '../../data/campaign/air.json';
import warData from '../../data/campaign/airwar.json';
import namesData from '../../data/campaign/air-names.json';
import carrierData from '../../data/campaign/carriers.json';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import rivalsData from '../../data/campaign/rivals.json';
import arsenalData from '../../data/campaign/arsenal.json';
import { Rng, hashString } from '../core/rng';
import { clamp, tenthPct } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { allied, atWar, headline, newsWeek, routineNews, theWords, thornGround, worksOf } from './game';
import { techMul, techNotes } from './research';
import { dcOf, flakMul, hasTech, shipKit, shipRule, type GunStage, type ShipResearch } from './tech';
import { NAVY, afloatWatch, atSea, fleetsNamed, keptAfloat, keptAfloatPast, shipCount, shipList, sinkShips, surfaceShips, toughOf, wholeShares, zoneOf } from './navy';
import { fleetKnown, seaStaff, syncWings } from './carriers';
import { muster, theUnit, trainedEdge } from './bombers';
import { storyAir, storyEdge } from './stories';
import { nameLosses, nameSide } from './hulls';
import { loseStars, mendStars } from './stars';
import { groundRules } from './ground';
import { RAILWAYS, bombFactories, bombRail, onNet, railBombChance, railCut, railsAt, ruinChance } from './rails';
import { bombingFor, raidsMul, seat } from './difficulty';
import { fuelAir } from './resources';
import { SIEGE, besieged, besiegerOf, breachCeiling, breachOf, fieldCtrl, hitTown, planTownHit, siegeRules, townBonus, townHeld, townHitText, townShare, type TownHit } from './siege';
import { airfieldOpenTo, prizeOut } from './prizes';
import { bomberOf, designYear } from '../battle/air/doctrine';
import { townName } from './towns';
import { settleFought, type BombLaw, type FoughtAir } from './airfought';
import { cityForecast, cityHit, cityWorthOf, type CityHit } from './repairs';
import { beyondEurope } from './world';
import { photosOn, raidPhoto } from './photos';
import { HIT_KINDS, hitRatesOn, hitsLine, strikeRates, type HitKind, type StrikeRates } from './hitrates';
import { acesFought } from './aces';
import { fogBound, whyNotFog } from './fog';
import { cloudAim, cloudGuns, cloudMet, grounded, skyAt, skyAtSea, weatherOn, whyNotSky, zoneOfRegion, zoneOfSea, type Sky } from './weather';
import { airPiece, nightGunShare, whyNotField, wrecked } from './airversion';
import { AMBUSH, ambushAt, flyingOrder, orderLapsed, ordersDue } from './secretwar';
import { acOn, aimOf, bombLands, campaignOrders, campaignResults, chainAt, gunnerRatio, inCampaign, nightFighters, recordField, skyFought, stockLoss } from './aircampaigns';
import type { AirFleet, AirMission, AirPool, AirReport, AirSide, Corps, Fleet, Game, PlanPhase, RaidPhoto, Region, SeaSide, ShipClass, Ships } from './types';
import { mapDist } from './wrap';
import { seaWhat, seaWhere } from './sea-words';

type Sched = [number, number, number][];
interface NatSpec { quality: number; ftrRange: number; bmrRange: number }
interface Chain { nat: string; from: number; name: string; by: string; ground: number; plural?: boolean }
interface FleetRow { nat: string; name: string; base: string; from?: number; leader?: string; start?: [number, number]; est: Sched; ftrRange?: number; scripted?: boolean; home?: boolean }
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
  /** Close air support (Brian's ask of 2026-11-18): see the note in the data. */
  close: { on: boolean; most: number; full: number; strike: number; strikes: [number, number]; counts: number; least: number; ready: number; loss: number; enemySky: number; ai: { on: boolean; share: number } };
  fleets: FleetRow[];
  plan: AirPlan[];
};

export interface RaidKindRow { key: string; label: string; short: string; blurb: string; needs: 'bmr' | 'any' }
export const AIRWAR = warData as unknown as {
  fields: { sizes: number[]; least: number; engineers: number; seed: { base: number; city: { class: number; fac: number; vp: number }; town: { class: number; fac: number; vp: number } }; words: [number, string][] };
  /** The defences of a target as the raid card counts them: see the note in the data. */
  count: { squadron: Record<string, number>; later: Record<string, [number, number]>; guns: { base: number; light: number; heavy: number } };
  groups: { most: number; names: Record<string, { ftr: string; bmr: string }> };
  raids: { kinds: RaidKindRow[] };
  ground: { intensity: number; fighters: number; scale: number; perDiv: number; max: number; str: number; floor: number; dug: number; cover: Record<string, number>; fire: number; value: number };
  /** Bombing the line: see the note in the data. */
  line: { intensity: number; scale: number; most: number; value: number };
  recon: { aircraft: number; loss: number; ready: number };
  losses: { bombers: number; power: number; escort: number; day: number; most: number; night: { bombers: number; power: number }; clash: number };
  /** Ruling 227: the historian's dated rows for day raids, [month, value] by nation (see the note in the data). */
  day: { kill: number; defence: Record<string, [number, number][]>; accuracy: Record<string, [number, number][]>; range: Record<string, [number, number][]> };
  /** Ruling 227 (b424): a nation's real weight in the air, beside the air force it is measured against; see the note in the data. */
  weight: { against: string; rows: Record<string, [number, number][]> };
  /** Ruling 227 (b433): what a won sky lends the land battle in the air war; see the note in the data. */
  sky: { land: number; attack: number; defence: number; own: number; radar: string };
  success: { k: number; factory: number };
  ai: { escortReady: number; basing: { better: number; least: number; presence?: number; fields?: number; idle?: boolean }; raids: { share: number; least: number; loss: number; rail: number; ready: number; success: number; look: number; worth: number; sweeps: number; sky: number; skyWorth: number; sour?: { trade: number; dear: number; times: number; rest: number }; beach?: { groups: number; home: number; monthly: number; sky: number } } };
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
  /** (`dc`: ruling 240's Damage control, read through dcOf in tech.ts and nowhere else.) */
  strike: { hit: number; most: number; again: number; report: number; aim: Ships; takes: Ships; ready: number; value: Ships; intensity: number; dc: number };
  harbour: { hit: number; most: number; surprise: number; unready: number; intensity: number };
  /** (`stage` and `vt`: ruling 240's anti-aircraft stages and proximity shells, read through flakMul in tech.ts and nowhere else.) */
  flak: { ships: Ships; per: number; most: number; stage: Record<GunStage, number>; vt: number };
  /** Ruling 240's three switches, all off as shipped: read them through tech.ts (SHIP_RESEARCH, shipRule, shipKit), never here. */
  research: ShipResearch;
  cap: { spoil: number; scale: number; most: number };
  sunk: { saved: number };
  empty: number;
  search: { ships: Ships; next: number; air: number; flight: number; find: number; report: number; open: number; subs: number; aircraft: number; ready: number };
  ai: { success: number; loss: number; worth: number; landShare: number; sortie: number; ready: number; shy: number; trade?: boolean; searches?: number };
};

/** True once carriers have wings of their own and aircraft can be aimed at one fleet or at a harbour. The grand campaign only. */
export const carrierRules = (g: Game): boolean => g.flags.carriers === 1;

/** True for a carrier wing: an air group that flies from a fleet's carriers, goes where they go, and has no airfield. */
export const afloat = (f: { ship?: number }): boolean => f.ship !== undefined;

/** True for a carrier wing wherever it is, aboard or ordered ashore: its aircraft are built for the deck and its crews trained against ships. */
export const deckBred = (f: { ship?: number; ashore?: number }): boolean => f.ship !== undefined || f.ashore !== undefined;

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
/** True for a group of a command that guards its own country (air.json 'home'): it does not follow the army across an ocean. */
export const homeCommand = (f: { nat: string; name: string; parent?: string }): boolean => !!AIR.fleets.find((x) => x.nat === f.nat && x.name === (f.parent ?? f.name))?.home;
/** A nation's value on one of ruling 227's dated rows this month (airwar.json 'day'): its own rows, else _default's. */
const dayRow = (table: Record<string, [number, number][]>, nat: string, turn: number): number => (row(table[nat] ?? table._default, turn) ?? row(table._default, turn) ?? table._default[0])[1];

/**
 * How far one fleet reaches: its nation's ranges, unless the fleet has long-range fighters of its own ('ftrRange' on its
 * row in air.json). In the air war a nation's fighters reach as far as the month allows (ruling 227: drop tanks, the
 * Zero, the Mustang), and a fleet's own longer reach stands.
 */
const reach = (f: { nat: string; name: string; parent?: string; ship?: number; ashore?: number }, g?: Game): NatSpec => {
  // Carrier aircraft are built for the deck, not for range: ashore as aboard.
  if (deckBred(f)) return { quality: spec(f.nat).quality * (CARRIERS.quality[f.nat] ?? CARRIERS.quality._default), ftrRange: CARRIERS.range.ftr, bmrRange: CARRIERS.range.bmr };
  const own = AIR.fleets.find((x) => x.nat === f.nat && x.name === (f.parent ?? f.name))?.ftrRange;
  const dated = g && airWar(g) ? dayRow(AIRWAR.day.range, f.nat, g.turn) : 0;
  const ftrRange = Math.max(own ?? 0, dated, spec(f.nat).ftrRange);
  // A nation with a weight row (ruling 227, b424) fights as that share of the air force it is measured against.
  const w = g ? airWeight(g, f.nat) : null;
  return { ...spec(f.nat), ...(w === null ? {} : { quality: w * spec(AIRWAR.weight.against).quality }), ftrRange };
};

/**
 * A nation's real weight in the air this month beside the air force its row is measured against (ruling 227, b424:
 * the historian's dated row for the Soviet air force, 1.0 the Luftwaffe in the east), or null for a nation with no row
 * and outside the air war.
 */
export const airWeight = (g: Game, nat: string): number | null => {
  const rows = AIRWAR.weight.rows[nat];
  if (!rows || !airWar(g)) return null;
  return (row(rows, g.turn) ?? rows[0])[1];
};
export const airRange = (f: { nat: string; name: string; parent?: string; ship?: number }, g?: Game): { ftr: number; bmr: number } => ({ ftr: reach(f, g).ftrRange, bmr: reach(f, g).bmrRange });

/** What a nation's day bombers can do for themselves this month, formation, guns and turrets (ruling 227); by night, and outside the air war, the flat old figure. */
export const bomberDefence = (g: Game, nat: string, night = false): number => (airWar(g) && !night ? dayRow(AIRWAR.day.defence, nat, g.turn) : AIR.combat.bomberDefence);

/**
 * Day bombing's weight on factories and railways this month (ruling 227): low in 1939 to 1941, full from 1943. The
 * bombardment of a town under siege keeps full weight (the day bombers that did little harm to industry did a great
 * deal to Warsaw and Rotterdam), as does every other mission, and everything outside the air war.
 */
export const dayAim = (g: Game, nat: string, m: AirMission): number => (airWar(g) && m === 'bomb' ? dayRow(AIRWAR.day.accuracy, nat, g.turn) : 1);

/** What a month's stopped output of one province is worth, on the scale its losses are counted on (one aircraft is 1): in the air war by its own factories (ruling 227). */
const outputWorth = (g: Game, r: Region, cut: number): number => (airWar(g) ? cut * Math.max(0, r.fac) * AIRWAR.success.factory : cut * 1000);

/**
 * The defending fighters the bombers' own guns bring down in a day raid of the air war (ruling 227): for each bomber
 * the fighters bring down, 'kill' x the bombers' defence (about 1 for 5 to 6 bombers in 1939 to 1941, 1 for 2 in the
 * combat boxes of 1943). Never none where fighters met the raid and bombers fell to them; none by night, and none
 * outside the air war.
 */
function gunsDown(g: Game, a: AirAssess, night: boolean, bombersLost: number, m: AirMission, target: number): number {
  // With the air campaigns on, the historian's ratio by bomber kind, by night as by day, takes the place of 227's (b557, b559).
  if (acOn(g)) {
    if (!airWar(g) || a.D <= 0 || a.bomberLoss <= 0) return 0;
    return bombersLost * Math.max(0, 1 - a.gunLoss / a.bomberLoss) * gunnerRatio(g, a.att[0]?.f.nat ?? '', m, target, night, a.E > 0);
  }
  if (!airWar(g) || night || a.D <= 0 || a.bomberLoss <= 0) return 0;
  const toFighters = Math.max(0, 1 - a.gunLoss / a.bomberLoss);
  return bombersLost * toFighters * AIRWAR.day.kill * a.defence;
}

export function airPool(g: Game, nat: string): AirPool {
  return (g.airPool[nat] ??= { ftr: 0, bmr: 0, pilots: 0, lostFtr: 0, lostBmr: 0, lostPilots: 0 });
}

/** Kilometres between the centres of two regions. */
export function km(g: Game, a: number, b: number): number {
  const A = g.regions[a], B = g.regions[b];
  return mapDist(g, A.x, A.y, B.x, B.y) * AIR.kmPerUnit;
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

/**
 * The class of the airfield a region began the grand campaign with: 0 none, 1 a landing ground, 2 an airfield, 3 an air
 * base. With the air campaigns on, the record's fields the chart does not seed count too (b574).
 */
export const fieldClass = (g: Game, region: number): number => Math.max(g.flags[`af:${region}`] ?? 0, recordField(g, region));

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

/**
 * The guns round a target as a count (Brian, 20 November 2026: "calculate how many flak guns they have"): the light
 * flak any target puts up, and the heavy and light guns of each level of anti-aircraft guns built there (two light
 * guns to every heavy one, as the record had it: the historian). A count for the raid card and the battle's picture;
 * the losses come from the shares the forecast has always used.
 */
export function flakGuns(level: number): { light: number; heavy: number; all: number } {
  const G = AIRWAR.count.guns, n = Math.max(0, level), heavy = n * G.heavy, light = Math.max(G.base, n * G.light);
  return { light, heavy, all: light + heavy };
}

/** How many fighters make a squadron in a nation's air force in a year: some went to larger squadrons late in the war. */
export function squadronSize(nat: string, year: number): number {
  const later = AIRWAR.count.later[nat];
  return later && year >= later[0] ? later[1] : AIRWAR.count.squadron[nat] ?? AIRWAR.count.squadron._default;
}

/** How many squadrons a number of a nation's fighters make in a year: at least one for any at all. */
export function squadronsOf(nat: string, fighters: number, year: number): number {
  return fighters > 0 ? Math.max(1, Math.round(fighters / squadronSize(nat, year))) : 0;
}

const WORKS = (arsenalData as unknown as { works: { aa: { cut: number; flak: number }; field: { holds: number } } }).works;
/** The share of a bombing raid that anti-aircraft guns of this level stop. */
export const gunsStop = (level: number): number => Math.max(0, Math.min(1, level * WORKS.aa.cut));

const leaderMul = (f: AirFleet): number => 1 + (f.leader ? AIR.leader.perLevel * (f.leader.level - 1) : 0);
/** What one of a fleet's aircraft is worth in a fight. */
const worth = (g: Game, f: AirFleet): number => reach(f, g).quality * leaderMul(f) * (0.5 + 0.5 * f.ready) * techMul(g, f.nat, 'fighters') * storyEdge(g, f) * fuelAir(g, f.nat);

/** Who holds the sky over a region, if anyone has won it lately. */
export function skyOver(g: Game, region: number): string | null {
  const s = g.sky[region];
  if (!s || g.turn - s.turn > AIR.sky.lasts || !g.nations[s.nat]?.alive) return null;
  return atWar(g, s.nat, g.regions[region].ctrl) ? s.nat : null;
}

/** True where two provinces border by land, with no water between: neighbours on the map that no short crossing joins. */
const landBorder = (g: Game, a: number, b: number): boolean => g.regions[a]?.nb.includes(b) === true && !g.links.some(([x, y]) => (x === a && y === b) || (x === b && y === a));

/** True for a carrier wing whose fleet lies in harbour in this province. */
const inHarbour = (g: Game, f: AirFleet, region: number): boolean => { const c = carrierOf(g, f); return !!c && !atSea(c) && c.base === region; };

/**
 * Whether a fighter group comes up over a province held by `holder` against a raid on the land itself: its towns,
 * works, rails and airfields, and the sky over them (ruling 225). Home defence is national: the holder's own groups
 * come up, and an ally's only where it is based on ground the holder controls or on ground that borders the province
 * by land, with no water between (no strait counts). France's groups at Lille do not cover Kent across the Channel;
 * they do cover Brussels, and a British group based in France covers France. Carrier fighters stay with their fleet:
 * over land they come up only over the harbour it lies in.
 */
export function guardsLand(g: Game, f: AirFleet, region: number, holder: string): boolean {
  if (!allied(g, f.nat, holder)) return false;
  if (f.ship !== undefined) return inHarbour(g, f, region);
  return f.nat === holder || g.regions[f.base]?.ctrl === holder || landBorder(g, f.base, region);
}

/** Whether a fighter group comes up over a sea (ruling 225): carrier fighters only over the sea their fleet is in; land-based groups as their range allows. */
export function guardsSea(g: Game, f: AirFleet, zone: number): boolean {
  if (f.ship === undefined) return true;
  const c = carrierOf(g, f);
  return !!c && atSea(c) && c.zone === zone;
}

/**
 * Whether an enemy fighter group comes up against a raid of this kind (ruling 225, with the historian's note: Fighter
 * Command over Arras, Calais and Dunkirk). A raid on troops is met by any enemy fighter in reach, whoever holds the
 * ground, carrier fighters off the coast too; a raid on ships, at sea or in a harbour, by any land-based fighter in
 * reach and by the carrier fighters of the fleets in that sea or that harbour; a raid on the land itself by the
 * groups that guard that ground (guardsLand).
 */
function comesUp(g: Game, f: AirFleet, m: AirMission, target: number, zone: number): boolean {
  if (m === 'ground' || m === 'line') return true;
  if (m === 'strike' || m === 'fleet') return guardsSea(g, f, zone);
  if (m === 'harbour') return f.ship === undefined || inHarbour(g, f, target);
  return guardsLand(g, f, target, g.regions[target].ctrl);
}

/** The fighters that would come up over a region against this nation: every enemy group left to defend that guards it (guardsLand), as far as its range and airfields allow. */
export function fightersOver(g: Game, region: number, against: string): number {
  const holder = g.regions[region].ctrl;
  // Weather (ruling 127): under a closed sky nothing comes up, and a group whose own sky is closed stays down.
  const wx = weatherOn(g);
  if (wx && skyAt(g, region) === 'closed') return 0;
  let n = 0;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || f.close || !atWar(g, f.nat, against) || !guardsLand(g, f, region, holder)) continue;
    if ((wx && grounded(g, f)) || wrecked(g, f) || fogBound(g, f)) continue;
    n += f.ftr * cover(kmTo(g, f, region), reach(f, g).ftrRange) * crowding(g, f);
  }
  return n;
}

/**
 * What a won sky lends a land battle: 'land' is added to the attacker's air edge where its side holds the sky, and the
 * attack and the defence are then multiplied by 1 + attack x edge and 1 - defence x edge. In the grand campaign's air
 * war (ruling 227, b433) the sky counts for whichever side holds it and the effect is fitted to the record
 * (airwar.json 'sky'); the historical campaigns keep their own figures.
 */
export const skyOnLand = (g: Game): { land: number; attack: number; defence: number; both: boolean } =>
  airWar(g) ? { land: AIRWAR.sky.land, attack: AIRWAR.sky.attack, defence: AIRWAR.sky.defence, both: true } : { land: AIR.sky.land, attack: 0.3, defence: 0.2, both: false };

/**
 * The aircraft a side can bring over a battle in the air war (ruling 227, b444 and b446): the fighters of every group
 * within its fighters' reach of the place and the bombers within its bombers' reach, of the nations on that side and of
 * their allies at war with the other, each counted at its nation's weight row where it has one (airwar.json 'weight',
 * the Soviet row) and at one everywhere else: raw numbers mislead.
 */
export function airOverBattle(g: Game, side: Set<string>, foes: Set<string>, region: number): number {
  const on = new Map<string, boolean>();
  const ours = (nat: string): boolean => {
    let v = on.get(nat);
    if (v === undefined) {
      v = !!g.nations[nat]?.alive && (side.has(nat) || ([...side].some((x) => allied(g, x, nat)) && [...foes].some((e) => atWar(g, nat, e))));
      on.set(nat, v);
    }
    return v;
  };
  const weight = new Map<string, number>();
  // Weather (ruling 127): nothing flies over a battle under a closed sky, nor from an airfield under one.
  const wx = weatherOn(g);
  if (wx && skyAt(g, region) === 'closed') return 0;
  let n = 0;
  for (const f of g.airFleets) {
    if (f.ftr + f.bmr <= 0 || !ours(f.nat)) continue;
    if ((wx && grounded(g, f)) || wrecked(g, f) || fogBound(g, f)) continue;
    const d = kmTo(g, f, region), R = reach(f, g);
    let w = weight.get(f.nat);
    if (w === undefined) { w = airWeight(g, f.nat) ?? 1; weight.set(f.nat, w); }
    if (d <= R.ftrRange) n += f.ftr * w;
    if (d <= R.bmrRange) n += f.bmr * w;
  }
  return n;
}

/**
 * Overwhelming air in a theatre owns the sky (ruling 227, b444 and b446): where nobody has won the sky over a battle, a
 * side that can bring 'own' times the other's aircraft over it, and at least the least a won sky needs, holds it, unless
 * the other contests it: a nation fighting over its own home ground once it has early-warning radar always contests
 * (Britain held in 1940 at three and a half to one), and so does anyone where the sky there has been fought for since
 * last month and the side with the numbers did not win it. 'att' or 'def' for the side that holds it so, or null. The
 * air war only.
 */
export function skyByNumbers(g: Game, aN: Set<string>, dN: Set<string>, region: number): 'att' | 'def' | null {
  const T = AIRWAR.sky.own;
  if (!airWar(g) || !(T > 0)) return null;
  const a = airOverBattle(g, aN, dN, region), d = airOverBattle(g, dN, aN, region);
  const side = a >= AIR.sky.least && a >= T * d ? 'att' : d >= AIR.sky.least && d >= T * a ? 'def' : null;
  if (!side) return null;
  const mine = side === 'att' ? aN : dN, theirs = side === 'att' ? dN : aN;
  const home = g.regions[region].owner;
  if (!g.regions[region].overseas && [...theirs].some((x) => allied(g, x, home)) && g.nations[home]?.alive && hasTech(g, home, AIRWAR.sky.radar)) return null;
  const by = (nat: string): boolean => [...mine].some((x) => allied(g, x, nat));
  for (let i = g.airReports.length - 1; i >= 0; i--) {
    const r = g.airReports[i];
    if (r.turn < g.turn - AIR.sky.lasts) break;
    if (r.mission === 'superiority' && r.region === region && !(by(r.attNat) && r.sky)) return null;
  }
  return side;
}

// ---- Close air support (Brian's ask of 2026-11-18, 18:58Z; air.json 'close') ----------------------------------------

export const CLOSE = AIR.close;
/** True where fighter groups may be put on ground support: the grand campaign, with the switch on. */
export const closeRules = (g: Game): boolean => CLOSE.on === true && g.flags.open === 1;
/**
 * True where a staff may put fighter groups on ground support of its own accord: the player's choice being open, and
 * the staffs' own switch on (air.json close.ai.on). Off, a war fought by staffs alone is the war it was before the
 * player was given the choice (Brian's ask of 2026-11-21: the player picks which fighter groups meet raids and which
 * are held for the armies).
 */
export const closeStaffRules = (g: Game): boolean => closeRules(g) && CLOSE.ai.on === true;

/** One group on call over a battle: what it brings, and the aircraft it loses on each air strike it flies there. */
export interface CloseGroup { id: number; name: string; nat: string; planes: number; lose: number }
/** One side's on-call air over a battle: its weight W, the share of its side's strength it makes, the air strikes, and the groups. */
export interface CloseSide { W: number; share: number; strikes: number; groups: CloseGroup[] }
/** What close air support does to one land battle: the air edge after it, and each side's part. */
export interface CloseAir { edge: number; att: CloseSide; def: CloseSide }

/** What W on call adds to its side of the air edge: up to 'most', at 'full' aircraft. It alone may carry the edge past 1 (b536). */
export const closeEdge = (W: number): number => CLOSE.most * Math.min(1, Math.max(0, W) / CLOSE.full);

/**
 * The ground-support groups that can fly over a land battle in `region` for the side of `nats` against `enemies`: land
 * fighter groups on that order, of those nations or of an ally at war with the other side, with fighters, fit to fly,
 * that have not flown a raid this month or another battle this week, within their fighters' reach. Each is worth its
 * fighters x reach x airfields x readiness x technology x fuel. `underFoe`: the other side holds the sky over it.
 */
export function closeOf(g: Game, nats: Set<string>, enemies: Set<string>, region: number, underFoe = false): CloseSide {
  const out: CloseSide = { W: 0, share: 0, strikes: 0, groups: [] };
  if (!closeRules(g)) return out;
  // Weather (ruling 127): under a closed sky over the battle nobody flies on call, and a group whose own sky is closed,
  // or whose field saboteurs wrecked (151(b)), stays down.
  const wx = weatherOn(g);
  if (wx && skyAt(g, region) === 'closed') return out;
  const stamp = g.turn * 10 + g.week;
  for (const f of g.airFleets) {
    if (!f.close || f.ftr <= 0 || f.ship !== undefined || f.mission !== 'support' || f.flew === g.turn || f.ready < CLOSE.least || f.closeAt === stamp) continue;
    if ((wx && grounded(g, f)) || wrecked(g, f) || fogBound(g, f)) continue;
    if (!g.nations[f.nat]?.alive) continue;
    const ours = nats.has(f.nat) || ([...nats].some((x) => allied(g, x, f.nat)) && [...enemies].some((e) => atWar(g, f.nat, e)));
    if (!ours || ![...enemies].some((e) => atWar(g, f.nat, e))) continue;
    const reachOf = cover(kmTo(g, f, region), reach(f, g).ftrRange) * crowding(g, f);
    if (reachOf <= 0) continue;
    const planes = f.ftr * reachOf;
    out.W += planes * (0.5 + 0.5 * f.ready) * techMul(g, f.nat, 'fighters') * fuelAir(g, f.nat);
    // (No dice: the air war never touches the land war's luck. Under the other side's sky more of them are lost.)
    const lose = Math.min(f.ftr, Math.round(planes * CLOSE.loss * (underFoe ? CLOSE.enemySky : 1)));
    out.groups.push({ id: f.id, name: f.name, nat: f.nat, planes: Math.round(planes), lose });
  }
  return out;
}

/** The air strikes a side's on-call air makes over a battle: W / strike, between strikes[0] and strikes[1], where it counts for anything. */
export const closeStrikes = (side: CloseSide): number =>
  side.share >= CLOSE.counts && side.groups.length ? clamp(Math.round(side.W / CLOSE.strike), CLOSE.strikes[0], CLOSE.strikes[1]) : 0;

/**
 * Books a land battle's close air support to the groups that flew it, once: each loses its aircraft for every air strike
 * its side flew (`sorties`, by group id: the field's own count where the battle was fought on the field, else the
 * strikes the card gave), pays 'ready' readiness, and is done for the week. A side that made no strike flies none.
 */
export function bookCloseAir(g: Game, air: CloseAir | undefined, sorties?: Record<number, number>): void {
  if (!air) return;
  const stamp = g.turn * 10 + g.week;
  for (const [side, home] of [[air.att, false], [air.def, true]] as const) {
    if (!side.strikes && !sorties) continue;
    for (const k of side.groups) {
      const f = g.airFleets.find((x) => x.id === k.id);
      const n = sorties ? sorties[k.id] ?? 0 : side.strikes;
      if (!f || n <= 0) continue;
      const lost = Math.min(f.ftr, k.lose * n);
      if (lost > 0) {
        const p = airPool(g, f.nat);
        // Pilots who come down over their own ground mostly fly again; over the enemy's, few do.
        const back = Math.round(lost * (home ? AIR.pilots.home : AIR.pilots.away));
        f.ftr -= lost; f.lost.ftr += lost;
        p.lostFtr += lost; p.pilots += back; p.lostPilots += lost - back;
      }
      f.ready = Math.max(AIR.ready.floor, f.ready - CLOSE.ready);
      f.closeAt = stamp;
    }
  }
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
  const now = r && r.turn === g.turn ? r.share : 0;
  // Air campaigns (ruling 236.2): what earlier raids still hold down comes on top, as far as the same ceiling.
  return acOn(g) ? Math.min(AIR.damage.industry.maxShare, now + stockLoss(g, nat)) : now;
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
  // With the air campaigns on, every nation's warning comes by the tree, or by the record's month at the book (b553(a)).
  if (acOn(g)) return chainAt(g, r, AIR.radar.chains);
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
export function foundAirFleet(g: Game, nat: string, r: Region, name: string, kind?: 'ftr' | 'bmr', strength = 0): AirFleet {
  const taken = (x: string): boolean => g.airFleets.some((f) => f.name === x);
  let unique = name;
  for (let k = 2; taken(unique); k++) unique = `${name} ${k}`;
  const f: AirFleet = {
    id: g.nextAir++, nat, name: kind ? groupName(g, nat, kind, strength, r) : unique, base: r.id, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
    mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 },
  };
  if (kind) { f.kind = kind; f.parent = unique; f.share = 1; }
  g.airFleets.push(f);
  return f;
}

// ---- The air war as its own game (the grand campaign) -------------------------------------

const ordinal = (k: number): string => `${k}${k % 100 >= 11 && k % 100 <= 13 ? 'th' : k % 10 === 1 ? 'st' : k % 10 === 2 ? 'nd' : k % 10 === 3 ? 'rd' : 'th'}`;

/** A rung of a country's ladder of air formation names (ruling 220, the historian's data/campaign/air-names.json). */
export interface Rung { rung: string; ftr: number; bmr: number; form: { ftr: string; bmr: string }; from?: number; until?: number }
const NAMES = namesData as unknown as { near: [number, number]; navyBases: string[]; ladders: Record<string, Rung[]> };

/** A country's homeland is lost while it does not hold its own capital. */
const homeLost = (g: Game, nat: string): boolean => { const n = g.nations[nat]; return !n || !n.alive || n.capital < 0 || g.regions[n.capital]?.ctrl !== nat; };

/**
 * The rungs a country's new air group may be named for this month: its ladder's rows that hold now (from, until),
 * and while its homeland is lost only the rows of its forces in exile, where it has them. Japan's navy names the
 * groups based on the Pacific islands, its army the rest (and all of them while the map cannot tell).
 */
export function rungsOf(g: Game, nat: string, base: Region | null): Rung[] {
  const ladder = (nat === 'JAP' && base && NAMES.navyBases.includes(base.name) ? NAMES.ladders.JAP_navy : null) ?? NAMES.ladders[nat] ?? [];
  const now = ladder.filter((r) => (r.from ?? 1) <= g.turn && g.turn <= (r.until ?? Infinity));
  const exile = now.filter((r) => /in exile/.test(r.rung));
  return exile.length && homeLost(g, nat) ? exile : now.filter((r) => !/in exile/.test(r.rung));
}

/** The rung nearest a group's full strength by ratio, among those near it (half to twice the rung's), or null. */
export function nearestRung(rungs: Rung[], kind: 'ftr' | 'bmr', strength: number): Rung | null {
  const [lo, hi] = NAMES.near;
  let best: Rung | null = null, bd = Infinity;
  for (const r of rungs) {
    const v = r[kind];
    if (!(v > 0) || strength < lo * v || strength > hi * v) continue;
    const d = Math.abs(Math.log(strength / v));
    if (d < bd) { bd = d; best = r; }
  }
  return best;
}

/** A form of name with its number: {n}, {nth}, {n+K}, {nth_fr} (1re, 1er before a régiment, else 2e), {nth_nl} (1e). */
export function formName(form: string, n: number): string {
  return form
    .replace(/\{n\+(\d+)\}/g, (_, k: string) => String(n + Number(k)))
    .replace(/\{nth_fr\}/g, n === 1 ? (/R[ée]giment/i.test(form) ? '1er' : '1re') : `${n}e`)
    .replace(/\{nth_nl\}/g, `${n}e`)
    .replace(/\{nth\}/g, ordinal(n))
    .replace(/\{n\}/g, String(n));
}

/**
 * A new air group's name, taken once as it is formed and kept as it is worn down (ruling 220): the rung of its
 * country's ladder nearest its full strength, with the next number of that rung; where no rung is near, the plain
 * form ("3rd Fighter Group"), never another country's word. No two of a country's groups share a name.
 */
function groupName(g: Game, nat: string, kind: 'ftr' | 'bmr', strength: number, base: Region | null): string {
  const taken = (x: string): boolean => g.airFleets.some((f) => f.nat === nat && f.name === x);
  const rungs = rungsOf(g, nat, base);
  for (let left = rungs; left.length;) {
    const r = nearestRung(left, kind, strength);
    if (!r) break;
    const form = r.form[kind];
    // A rung with no number in its name (Poland's Brygada Pościgowa) is one formation: once it is taken, the next rung.
    if (!/\{n/.test(form)) { if (!taken(form)) return form; left = left.filter((x) => x !== r); continue; }
    const key = `agn:${nat}:${kind}:${r.rung}`;
    let name: string;
    do { g.flags[key] = (g.flags[key] ?? 0) + 1; name = formName(form, g.flags[key]); } while (taken(name));
    return name;
  }
  const key = `agn:${nat}:${kind}`;
  let name: string;
  do { g.flags[key] = (g.flags[key] ?? 0) + 1; name = formName(AIRWAR.groups.names._default[kind], g.flags[key]); } while (taken(name));
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
  if (out.length > 1) for (const grp of out) grp.name = groupName(g, f.nat, grp.kind!, Math.max(grp.est[grp.kind!], grp[grp.kind!]), g.regions[grp.base] ?? null);
  return out;
}

/**
 * The air commands history bases beyond the old chart (America's: ruling 133(b)) stand on their fields from the
 * first day of a grand campaign: the wider world is attached after the air forces of Europe are set up, so they
 * are raised here. A campaign saved before they were listed has them raised as its month finds them, at the
 * strength history gives them by then. Done once. The commands history forms later form in their month (airUpkeep).
 */
export function openWorldAir(g: Game): void {
  if (g.flags.worldair === 1) return;
  g.flags.worldair = 1;
  for (const r of AIR.fleets) {
    const base = g.byName[r.base];
    if (!base || !beyondEurope(base) || (r.from ?? 1) > 1 || r.scripted || base.ctrl !== r.nat || !g.nations[r.nat]?.alive) continue;
    if (g.airFleets.some((f) => f.nat === r.nat && (f.parent ?? f.name) === r.name)) continue;
    const f = addAir(g, r);
    // An air war already open (a saved campaign): the command's ground has its air base, and it stands as air groups.
    if (f && airWar(g)) { if (fieldClass(g, base.id) < AIRWAR.fields.seed.base) g.flags[`af:${base.id}`] = AIRWAR.fields.seed.base; splitFleet(g, f); }
  }
}

/**
 * The grand campaign opens its air war. Airfields are placed where the war begins with them: an air base wherever
 * history bases an air fleet and at every capital, an airfield in every city, a landing ground in every town.
 * Every air fleet becomes air groups of one kind. Done once; a grand campaign saved before the air war is brought in on loading.
 */
export function openAirWar(g: Game): void {
  if (g.flags.airwar === 1) return;
  g.flags.airwar = 1;
  seedFields(g);
  for (const f of [...g.airFleets]) { seedField(g, f.base, AIRWAR.fields.seed.base); splitFleet(g, f); }
}

const seedField = (g: Game, id: number, cls: number): void => { if (fieldClass(g, id) < cls) g.flags[`af:${id}`] = cls; };

/**
 * The first day's airfields, from region `from` on: a capital's, a city's and a town's by its factories and victory
 * points, and the base of every air force on the record. A campaign saved on a smaller chart lays them on the ground
 * drawn since when it is loaded (world.ts, settleNewGround); a field already there is never made smaller.
 */
export function seedFields(g: Game, from = 0): void {
  const S = AIRWAR.fields.seed;
  for (let i = from; i < g.regions.length; i++) seedField(g, i, groundClass(g.regions[i]));
  for (const row of AIR.fleets) { const b = g.byName[row.base]; if (b && b.id >= from && !row.scripted) seedField(g, b.id, S.base); }
}

/** The first day's airfield for a province by its ground alone: a capital's air base, a city's airfield, a town's landing ground. */
const groundClass = (r: Region): number => {
  const S = AIRWAR.fields.seed;
  return r.cap ? S.base : r.fac >= S.city.fac || r.vp >= S.city.vp ? S.city.class : r.fac >= S.town.fac || r.vp >= S.town.vp ? S.town.class : 0;
};

/** The airfield the first day gives a province on today's chart, the base of an air force on the record included (seedFields). */
export const seedClass = (g: Game, r: Region): number =>
  Math.max(groundClass(r), AIR.fleets.some((row) => !row.scripted && g.byName[row.base]?.id === r.id) ? AIRWAR.fields.seed.base : 0);

// ---- Bases and orders ---------------------------------------------------------------------

export function whyNotBase(g: Game, f: AirFleet, r: Region, bySea = false): string | null {
  if (f.ship !== undefined) return 'It flies from the carriers of its fleet, and goes where they go.';
  if (r.id === f.base) return 'It is already based there.';
  if (thornGround(g, r, f.nat)) return `${g.nations[r.owner].name} keeps its own air force: no group of its allies is based there.`;
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
  // (A group shipped across an ocean with its country's army goes in crates, however far: world.json 'overseas'.)
  if (!bySea && km(g, f.base, r.id) > AIR.ferry) return 'Too far to move in one month.';
  return null;
}

/**
 * True when a field in the wider world lies beyond a month's flying from a land group's base there and one of the two
 * is an island or a colony: the group goes by sea, as it would across an ocean (the Canal Zone's groups to Louisiana,
 * and back; Canada's to Iceland). A staff does not send its groups this way (airBases).
 */
const farIsle = (g: Game, f: AirFleet, r: Region): boolean =>
  f.ship === undefined && f.base >= 0 && beyondEurope(f.base) && beyondEurope(r) && (g.regions[f.base].overseas || r.overseas) && km(g, f.base, r.id) > AIR.ferry;

/** True when a field lies across an ocean from a land group's base, or is a far island of the wider world: the group can get there only by sea (world.json 'overseas'). */
export const overOcean = (g: Game, f: AirFleet, r: Region): boolean => (f.ship === undefined && beyondEurope(f.base) !== beyondEurope(r)) || farIsle(g, f, r);

export const SHIP_AIR_TIP = 'A group crosses an ocean by sea: one group a month, to a friendly airfield with room for all of it. Settling in costs it some readiness.';
export const SHIP_AIR_ONE = 'One air group a month crosses an ocean by sea, and one has gone this month.';
export const SHIP_AIR_ROOM = 'The airfields there have no room for the whole group, and a group that comes by sea is not put down on a crowded field.';

/**
 * Why a land air group cannot be shipped across an ocean to a region's airfields, or null if it can (ruling 133(b)).
 * What a computer country's staff does for the groups that follow its army (ai.ts `follow`) is the player's order
 * too: one group of a nation crosses in a month, however far, to a friendly airfield with room for all of it.
 */
export function whyNotShipAir(g: Game, f: AirFleet, r: Region): string | null {
  const no = whyNotBase(g, f, r, true);
  if (no) return no;
  if (g.flags[`fy:air:${f.nat}`] === g.turn) return SHIP_AIR_ONE;
  return fieldRoom(g, r.id, f) ? null : SHIP_AIR_ROOM;
}

/** Why a group cannot move to a region's airfields, by air or (across an ocean) by sea; null if it can. */
export const whyNotMoveAir = (g: Game, f: AirFleet, r: Region): string | null => (overOcean(g, f, r) ? whyNotShipAir(g, f, r) : whyNotBase(g, f, r));

/** Ships a land air group across an ocean to a friendly airfield with room for it: the nation's one such sailing of the month. */
export function shipAir(g: Game, f: AirFleet, r: Region): boolean {
  if (!overOcean(g, f, r) || whyNotShipAir(g, f, r) || !rebaseAir(g, f, r, true)) return false;
  g.flags[`fy:air:${f.nat}`] = g.turn;
  return true;
}

/** The fields a staff moves its groups between (the far islands of the wider world left out: farIsle). */
export const airBases = (g: Game, f: AirFleet): Region[] => g.regions.filter((r) => !farIsle(g, f, r) && whyNotMoveAir(g, f, r) === null);

/** Every field a player can send a group to now, by air or by sea. */
export const moveTargets = (g: Game, f: AirFleet): Region[] => g.regions.filter((r) => whyNotMoveAir(g, f, r) === null);

/**
 * Whether a nation's fighters could fight for the sky over a region: from the airfields they are on now ('now'), or
 * from one they could be brought up to ('forward', with that airfield), or not at all (null). Fighters reach only
 * so far, and a beach beyond them is one whose sky cannot be won. Carrier wings are left out: they go where the fleet goes.
 */
export function skyReach(g: Game, nat: string): (region: number) => { how: 'now' | 'forward'; base: number } | null {
  const fighters = airFleetsOf(g, nat).filter((f) => f.kind === 'ftr' && f.ftr > 0 && !afloat(f));
  let fields: { base: number; range: number }[] | null = null;
  const known = new Map<number, { how: 'now' | 'forward'; base: number } | null>();
  return (region: number) => {
    if (known.has(region)) return known.get(region)!;
    let out: { how: 'now' | 'forward'; base: number } | null = null;
    for (const f of fighters) if (km(g, f.base, region) <= reach(f, g).ftrRange) { out = { how: 'now', base: f.base }; break; }
    if (!out) {
      // Every airfield a fighter group could move to, with the longest reach of any group that could use it.
      if (!fields) {
        const far = new Map<number, number>();
        const seen = new Set<string>();
        for (const f of fighters) {
          const range = reach(f, g).ftrRange, k = `${range}`;
          if (seen.has(k)) continue;
          seen.add(k);
          for (const r of airBases(g, f)) far.set(r.id, Math.max(far.get(r.id) ?? 0, range));
        }
        fields = [...far.entries()].map(([base, range]) => ({ base, range }));
      }
      let d = Infinity;
      for (const x of fields) { const k = km(g, x.base, region); if (k <= x.range && k < d) { d = k; out = { how: 'forward', base: x.base }; } }
    }
    known.set(region, out);
    return out;
  };
}

/** Moves a fleet to airfields in another friendly region (`bySea`: shipped there, however far). Settling in costs it some readiness. */
export function rebaseAir(g: Game, f: AirFleet, r: Region, bySea = false): boolean {
  if (whyNotBase(g, f, r, bySea)) return false;
  f.base = r.id;
  f.moved = g.turn;
  f.ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  if (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target)) { f.mission = 'support'; f.target = -1; }
  return true;
}

/** True where the airfields of a region have room for this group beside the aircraft already there. */
export function fieldRoom(g: Game, base: number, f: AirFleet): boolean {
  const there = g.airFleets.reduce((s, x) => s + (x !== f && x.base === base && x.ship === undefined ? x.ftr + x.bmr : 0), 0);
  return there + f.ftr + f.bmr <= (airWar(g) ? Math.max(AIRWAR.fields.least, airfieldsHold(g, base)) : airfieldsHold(g, base));
}

// ---- Room on the fields: a staff's month's end (the design director's b437) --------------------------------------

/** How many aircraft the airfields of a region serve in the air war, as crowding() counts them. */
export const fieldServes = (g: Game, region: number): number => Math.max(AIRWAR.fields.least, airfieldsHold(g, region));
/** The aircraft a land group keeps room for on its field: its establishment, or what it has if that is more. */
const keepsRoom = (f: AirFleet): number => Math.max(f.ftr + f.bmr, f.est.ftr + f.est.bmr);
/** The room a field's groups keep, every nation's, as crowding() counts the aircraft on it. */
const roomTaken = (g: Game, region: number): number => g.airFleets.reduce((s, x) => s + (x.ship === undefined && x.base === region ? keepsRoom(x) : 0), 0);
/** The smallest part of a group a staff splits off to put it on another field: half a wing. */
const LEAST_PART = 50;

/** The enemy-held ground with enemy troops on it beside a nation's own field corps: the front its air groups cover. */
function frontOf(g: Game, nat: string): number[] {
  const out = new Set<number>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort') continue;
    for (const j of g.regions[c.reg].nb) {
      const r = g.regions[j];
      if (atWar(g, nat, r.ctrl) && r.units.some((u) => !u.dead && atWar(g, nat, u.nat))) out.add(j);
    }
  }
  return [...out].sort((a, b) => a - b);
}

/**
 * Part of an air group, as a fraction of it, made a group of its own on the same field: it takes its part of the
 * aircraft and of the establishment (its share of the command, and of the wings bought for it), so the command is
 * kept at what it was; the group keeps its name, its commander and its record, and the new one is named as its
 * country names its groups.
 */
function splitOff(g: Game, f: AirFleet, q: number): AirFleet {
  const take = (n: number): number => Math.round(n * q);
  const grp: AirFleet = {
    id: g.nextAir++, nat: f.nat, name: '', base: f.base, ftr: take(f.ftr), bmr: take(f.bmr), est: { ftr: take(f.est.ftr), bmr: take(f.est.bmr) },
    mission: f.mission, target: f.target, ready: f.ready, leader: null, moved: f.moved, lost: { ftr: 0, bmr: 0 },
  };
  if (f.kind) grp.kind = f.kind;
  if (f.parent !== undefined) grp.parent = f.parent;
  if (f.flew !== undefined) grp.flew = f.flew;
  if (f.scouted !== undefined) grp.scouted = f.scouted;
  const share = f.share ?? 1;
  grp.share = share * q;
  f.share = share - grp.share;
  if (f.extra) { grp.extra = { ftr: take(f.extra.ftr), bmr: take(f.extra.bmr) }; f.extra = { ftr: f.extra.ftr - grp.extra.ftr, bmr: f.extra.bmr - grp.extra.bmr }; }
  f.ftr -= grp.ftr; f.bmr -= grp.bmr;
  f.est = { ftr: f.est.ftr - grp.est.ftr, bmr: f.est.bmr - grp.est.bmr };
  const kind = grp.kind ?? (grp.bmr > grp.ftr ? 'bmr' : 'ftr');
  grp.name = groupName(g, f.nat, kind, Math.max(grp.est[kind], grp[kind]), g.regions[grp.base] ?? null);
  g.airFleets.splice(g.airFleets.indexOf(f) + 1, 0, grp);
  return grp;
}

/**
 * A computer staff's month's end in the air war (the design director's b437): no field of its own is left serving
 * fewer aircraft than its groups are kept at. From a crowded field it sends groups to the nearest fields with room
 * that still reach the stretch of front they cover (failing those, the nearest with room), the group whose going is
 * just enough first, so that the field it leaves stays full; a group too big for any field with room is split, and
 * the part that has room goes. The player's groups are never moved, unless he has handed the air war to his staff:
 * he is told (crowdedRow, crowdedNews). Returns the groups moved.
 */
export function spreadAir(g: Game, nat: string): AirFleet[] {
  const moved: AirFleet[] = [];
  if (!airWar(g) || byHand(g, nat, 'air') || !g.nations[nat]?.alive) return moved;
  const land = (f: AirFleet): boolean => f.nat === nat && f.ship === undefined && f.ashore === undefined && f.base >= 0 && !!g.regions[f.base];
  const mine = g.airFleets.filter(land);
  if (!mine.length) return moved;
  const front = frontOf(g, nat);
  const fields = g.regions.filter((r) => hasAirfield(g, r.id));
  const free = (id: number): number => fieldServes(g, id) - roomTaken(g, id);
  /** The fields a group could go to this month, nearest first: those that reach the front it covers, then the rest. */
  const placesFor = (f: AirFleet): Region[] => {
    const R = f.kind === 'bmr' ? reach(f).bmrRange : reach(f).ftrRange;
    const covers = front.filter((t) => km(g, f.base, t) <= R);
    const near = fields
      .map((r) => ({ r, d: km(g, f.base, r.id) }))
      .filter((x) => x.r.id !== f.base && x.d <= AIR.ferry && !overOcean(g, f, x.r) && whyNotBase(g, f, x.r) === null)
      .sort((a, b) => a.d - b.d || a.r.id - b.r.id);
    const reaches = (r: Region): boolean => !covers.length || covers.some((t) => km(g, r.id, t) <= R);
    return [...near.filter((x) => reaches(x.r)), ...near.filter((x) => !reaches(x.r))].map((x) => x.r);
  };
  for (const F of [...new Set(mine.map((f) => f.base))].sort((a, b) => a - b)) {
    for (let guard = 0; guard < 64; guard++) {
      const over = roomTaken(g, F) - fieldServes(g, F);
      if (over <= 0) break;
      const here = g.airFleets.filter((f) => land(f) && f.base === F && f.moved !== g.turn && keepsRoom(f) > 0);
      if (!here.length) break;
      const dest = (f: AirFleet): Region | undefined => placesFor(f).find((r) => free(r.id) >= keepsRoom(f));
      const bySize = [...here].sort((a, b) => keepsRoom(a) - keepsRoom(b) || a.id - b.id);
      // The group whose going is just enough, so that the field stays full; else the largest that has somewhere to go.
      let pick: { f: AirFleet; to: Region } | null = null;
      for (const f of bySize) { if (keepsRoom(f) < over) continue; const to = dest(f); if (to) { pick = { f, to }; break; } }
      if (!pick) for (const f of [...bySize].reverse()) { const to = dest(f); if (to) { pick = { f, to }; break; } }
      if (!pick) {
        // No group has a field with room for all of it: the largest is split, and the part that has room goes.
        const f = bySize[bySize.length - 1];
        const to = placesFor(f).filter((r) => free(r.id) >= LEAST_PART).sort((a, b) => free(b.id) - free(a.id))[0];
        if (!to) break;
        const part = Math.min(over, free(to.id));
        if (part < LEAST_PART || part >= keepsRoom(f)) break;
        pick = { f: splitOff(g, f, part / keepsRoom(f)), to };
      }
      if (!rebaseAir(g, pick.f, pick.to)) break;
      moved.push(pick.f);
    }
  }
  return moved;
}

/** How much of a crowded field's aircraft can fly, in words: what it serves over what is based there (the UI director's sheet for b448). */
export function shareWords(share: number): string {
  const steps: [number, string][] = [[0.85, 'nine in ten'], [0.7, 'three quarters'], [0.6, 'two thirds'], [0.45, 'half'], [0.3, 'a third'], [0.22, 'a quarter'], [0.15, 'a fifth'], [0.07, 'a tenth']];
  return steps.find(([at]) => share >= at)?.[1] ?? 'hardly any';
}

/** A crowded airfield: what it serves, what is based there, and the share of that which can fly. */
export interface CrowdedField { region: number; serves: number; based: number; share: number }

/** A nation's crowded airfields, worst first: the fields its groups are on where more aircraft are based than the field serves. */
export function crowdedFields(g: Game, nat: string | null = g.human): CrowdedField[] {
  if (!nat || !airWar(g)) return [];
  const bases = [...new Set(g.airFleets.filter((f) => f.nat === nat && f.ship === undefined && f.base >= 0 && !!g.regions[f.base]).map((f) => f.base))];
  return bases
    .map((id) => {
      const based = g.airFleets.reduce((s, x) => s + (x.base === id && x.ship === undefined ? x.ftr + x.bmr : 0), 0);
      const serves = fieldServes(g, id);
      return { region: id, serves, based, share: based > 0 ? serves / based : 1 };
    })
    .filter((c) => c.based > c.serves)
    .sort((a, b) => a.share - b.share || b.based - a.based || a.region - b.region);
}

const counted = (n: number): string => Math.round(n).toLocaleString('en-US');
/** "so a third of them fly", "so only a third", never "only hardly any". */
const soFly = (c: CrowdedField, can: string): string => {
  const w = shareWords(c.share);
  return `so ${c.share < 0.6 && w !== 'hardly any' ? 'only ' : ''}${w} of them ${can}`;
};

/** The tray's one row while any field of his is crowded: the worst named, and how many there are (b448; a press opens the worst). */
export function crowdedRow(g: Game, cs: CrowdedField[]): string {
  const w = cs[0], name = g.regions[w.region].name;
  const tail = `it serves ${counted(w.serves)} aircraft and has ${counted(w.based)}, so ${shareWords(w.share)} of them can fly`;
  return theWords(cs.length === 1 ? `${name}'s airfield is crowded: ${tail}` : `${cs.length} of your airfields are crowded, ${name}'s worst: ${tail}`);
}

const COUNT_HEAD = ['No', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'];
/** "the harbour of Naples", but a place whose name is a harbour stands alone: "Pearl Harbor" (the UI director, mend 6). */
export const harbourOf = (place: string): string => (/\bHarbou?r$/.test(place) ? place : `the harbour of ${place}`);
/**
 * The month card's line as the player's month opens (b448): news of a field of his that has become crowded since
 * the month before, and not again while it stays crowded. Not while his staff runs the air war: it spreads the groups.
 */
export function crowdedNews(g: Game): void {
  const me = g.human;
  if (!me || !airWar(g) || !g.nations[me]?.alive) return;
  const pre = `crowd:${me}:`;
  const now = staffRuns(g, me, 'air') ? [] : crowdedFields(g, me);
  const at = new Set(now.map((c) => c.region));
  for (const k of Object.keys(g.flags)) if (k.startsWith(pre) && !at.has(Number(k.slice(pre.length)))) delete g.flags[k];
  const fresh = now.filter((c) => g.flags[`${pre}${c.region}`] === undefined);
  for (const c of now) g.flags[`${pre}${c.region}`] ??= g.turn;
  if (!fresh.length) return;
  const w = fresh[0], name = g.regions[w.region].name;
  const line = `serves ${counted(w.serves)} aircraft and ${counted(w.based)} are based there, ${soFly(w, 'fly')}. Move some groups to a field with room and they all fly.`;
  const head = fresh.length === 1 ? `${name}'s airfield is crowded` : `${COUNT_HEAD[fresh.length] ?? fresh.length} airfields are crowded`;
  const text = fresh.length === 1 ? `It ${line}` : `${name}'s is the worst: it ${line}`;
  g.headlines.push({ turn: g.turn, week: newsWeek(g), kind: 'info', nat: me, region: w.region, head: theWords(head), text: theWords(text) });
}

/** True when a strike on this sea has something to hit: enemy ships at sea, or an enemy's trade along its coast. */
function shipping(g: Game, nat: string, z: number): boolean {
  if (g.fleets.some((x) => x.zone === z && atSea(x) && atWar(g, nat, x.nat))) return true;
  if (NAVY.convoys.some((c) => atWar(g, nat, c.to) && c.zones.includes(g.seas[z].key))) return true;
  return g.seas[z].coast.some((id) => g.regions[id].port && atWar(g, nat, g.regions[id].ctrl));
}

/** The weather zone a mission's target lies in: a sea's, a fleet's sea's, or a province's (ruling 127). */
export function skyZone(g: Game, m: AirMission, target: number): number {
  if (m === 'strike') return zoneOfSea(g, target);
  if (m === 'fleet') { const t = g.fleets.find((x) => x.id === target); return t && atSea(t) ? zoneOfSea(g, t.zone) : -1; }
  return zoneOfRegion(g, target);
}

/** Why a fleet cannot fly this mission against this target, or null if it can. */
export function whyNotMission(g: Game, f: AirFleet, m: AirMission, target: number): string | null {
  if (m === 'support' || m === 'rest') return null;
  if (!g.nations[f.nat].atWar.length) return 'The nation is at peace.';
  // Weather (ruling 127): a mission needs an open sky at its base and at its target.
  if (weatherOn(g)) { const why = whyNotSky(g, f, skyZone(g, m, target)); if (why) return why; }
  // A field wrecked by saboteurs grounds every group based on it for the month (ruling 151(b)).
  const field = whyNotField(g, f);
  if (field) return field;
  // Fog over the airfields (fog.ts): fog has closed its field for the month.
  const fog = whyNotFog(g, f);
  if (fog) return fog;
  const R = reach(f, g);
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
  // (Air campaigns, ruling 236.2: an oilfield is a target too.)
  if (r.fac > 0 || (onNet(target) && !railCut(g, target) && groundRules(g)) || (!!r.oil && acOn(g))) return null;
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
  if (airWar(g)) growCommands(g, nat);
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

/**
 * A command history grows (ruling 228, item 4: Bomber Command to 1,400 heavies, the Eighth to 1,600) forms new air
 * groups as it grows, so that none is kept at more than the data allows: when its establishment of a kind passes what
 * its groups of that kind can hold, new groups of that kind form beside the first of them, empty, and fill from the
 * month's deliveries. Every group of the kind is then kept at an even part of the command (the old ones keep their
 * proportions among themselves). A command that has lost every group of a kind raises none again.
 */
function growCommands(g: Game, nat: string): void {
  const most = AIRWAR.groups.most;
  for (const r of AIR.fleets) {
    if (r.nat !== nat) continue;
    const e = row(r.est, g.turn);
    if (!e) continue;
    for (const [kind, want] of [['ftr', e[1]], ['bmr', e[2]]] as const) {
      const have = g.airFleets.filter((f) => f.nat === nat && f.ship === undefined && f.kind === kind && (f.parent ?? f.name) === r.name);
      const n = Math.ceil(want / most);
      if (!have.length || n <= have.length) continue;
      const first = have[0], base = g.regions[first.base];
      if (!base) continue;
      for (const f of have) f.share = (f.share ?? 1) * (have.length / n);
      let at = g.airFleets.indexOf(have[have.length - 1]);
      for (let k = have.length; k < n; k++) {
        const grp: AirFleet = {
          id: g.nextAir++, nat, name: '', base: first.base, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
          mission: 'rest', target: -1, ready: first.ready, leader: null, moved: first.moved, lost: { ftr: 0, bmr: 0 }, kind, parent: r.name, share: 1 / n,
        };
        grp.name = groupName(g, nat, kind, Math.round(want / n), base);
        g.airFleets.splice(++at, 0, grp);
        headline(g, 'info', `${grp.name} forms in ${base.name}, a new group of ${r.name}.`, nat, base.id);
      }
    }
  }
}

/** A fleet whose base has fallen flies out to the nearest ground its nation still holds, leaving some aircraft behind. */
function fallBack(g: Game, f: AirFleet): boolean {
  const old = g.regions[f.base];
  const nearest = (fields: boolean): Region | null => {
    let out: Region | null = null, bd = Infinity;
    for (const r of g.regions) {
      if (r.ctrl !== f.nat || (fields && !hasAirfield(g, r.id)) || besieged(g, r) || prizeOut(g, r, 'airfield') > 0) continue;
      const d = mapDist(g, r.x, r.y, old.x, old.y);
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
    headline(g, 'event', `With nowhere left to fly from, ${f.name} is disbanded.`, f.nat, old.id, { arm: 'air', lost: f.nat });
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
  /** What the bombers can do for themselves (formation, guns and turrets): their side of the fighting weight a bomber, and the fighters their guns bring down (ruling 227). */
  defence: number;
  /** How many fewer of the raid's bombers the fighters bring down for their crews' training at the player's schools (bombers.ts): a share, 0 for most raids. */
  drilled: number;
  /** The level of anti-aircraft guns round the industry being bombed. */
  aa: number;
  /**
   * The defences in numbers (Brian, 20 November 2026): the flak guns round a target on land that bombers go for (0
   * for any other raid), and the enemy's fighter squadrons and groups able to meet the raid. Counts of what the
   * forecast already weighs, for the raid card; nothing in the rules reads them.
   */
  guns: { light: number; heavy: number; all: number };
  squadrons: number;
  groups: number;
  /**
   * Of the bombers expected to be lost, the share the guns bring down: the flak any target puts up, the anti-aircraft
   * guns built round it, and a fleet's own guns. The rest fall to fighters. It is a part of `bomberLoss`, set apart for
   * the raid card to show, and nothing in the rules reads it.
   */
  gunLoss: number;
  lines: TicketLine[];
  /** A strike on a fleet or a raid on a harbour: the fleets under attack, the share of the bombers their guns bring down, and the share of the strike the fighter cover spoils. */
  ships: Fleet[];
  flak: number;
  spoil: number;
  /** What the bombers that go are worth against ships, beside carrier crews trained for it (1). */
  /** Hit rates by kind (hitrates.ts), only with that switch on: the strike's bombers by kind and each kind's rate. */
  rates?: StrikeRates;
  naval: number;
  /** What is left of a strike on a fleet already struck this month: it is scattered and warned. */
  again: number;
  /** A harbour caught unready in the month war was declared. */
  surprise: boolean;
  /** The sky over the target (weather, ruling 127): clear with no weather. */
  sky: Sky;
  /** What cloud leaves of the bombs that find the target: 1 under a clear sky. */
  aim: number;
  /**
   * Ruling 240 (C3), in step with `ships`: how many times the hits each fleet's surface warships take to sink, by its
   * nation's damage control (tech.ts dcOf). Set only where some fleet's is not 1: with the switch off, or nobody
   * holding the research, an assessment has no such key and is what it always was.
   */
  tough?: number[];
}

const SHIPS: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const noShips = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
/** What one of a group's bombers is worth against ships: carrier crews are trained for it, land bombers mostly are not. */
const navalSkill = (f: AirFleet): number => (deckBred(f) ? CARRIERS.naval.carrier : CARRIERS.naval.land[f.nat] ?? CARRIERS.naval.land._default);

/**
 * What a strike on ships should do, before the dice: the hits to expect, the ships of each class they should sink,
 * and what that is worth when the raid is judged. `through` is the bombers that reach the ships, and `found` the
 * share of them that find a fleet known only from a report.
 *
 * Ruling 240 (C3), only where the assessment carries `tough`: the class totals and the cap are worked as ever, on
 * the count without anyone's damage control, so no navy's losses hang on a neighbour's research; only then is each
 * fleet's own share of each surface class divided by its own figure. `sunk`, `count` and `value` are then the eased
 * totals, `by` each fleet's eased share (in step with `a.ships`) and `plain` the class totals before the easing.
 * The hits are not changed, and submarines are never eased.
 */
export function shipHits(a: AirAssess, m: AirMission, through: number, found = 1): ShipHits {
  return shipHitsBy(shipLaw(a, m, found), through);
}

/** What a strike on ships should do (shipHits); `by` and `plain` only where some fleet's damage control is not 1 (ruling 240). */
export interface ShipHits {
  hits: number; sunk: Ships; count: number; value: number; by?: Ships[]; plain?: Ships;
  /** Hit rates by kind only: the hits each kind scored, and the bombers of each kind they came from. */
  kinds?: Record<HitKind, number>; flew?: Record<HitKind, number>;
}

/**
 * The law of a strike on ships, apart from the bombers that get through: what shipHits weighs, kept so an air battle
 * can say as each box drops what the campaign will make of the bombs dropped so far (ruling 238, b397's law for ships).
 * `have` is the ships of each class under attack, and `fleets` each fleet's own, in the order fly() books them.
 */
export interface ShipLaw {
  harbour: boolean;
  naval: number;
  spoil: number;
  again: number;
  found: number;
  surprise: boolean;
  have: Ships;
  fleets: { id: number; nat: string; name: string; ships: Ships }[];
  /** Ruling 240 (C3): each fleet's damage control, in step with `fleets`; only where some fleet's is not 1 (AirAssess.tough). */
  tough?: number[];
  /** Hit rates by kind (hitrates.ts), only with that switch on: each kind's rate, the strike's share of each kind, and its land groups' own split (for a box that flies as a land group's). */
  rates?: Record<HitKind, number>;
  mix?: Record<HitKind, number>;
  land?: Record<HitKind, number>;
}

export function shipLaw(a: AirAssess, m: AirMission, found = 1): ShipLaw {
  const have = noShips();
  for (const x of a.ships) for (const c of SHIPS) have[c] += x.ships[c];
  return {
    harbour: m === 'harbour', naval: a.naval, spoil: a.spoil, again: a.again, found, surprise: a.surprise, have,
    fleets: a.ships.map((x) => ({ id: x.id, nat: x.nat, name: x.name, ships: { ...x.ships } })),
    ...(a.tough ? { tough: [...a.tough] } : {}),
    ...(a.rates ? { rates: { ...a.rates.rates }, mix: { ...a.rates.mix }, land: { ...a.rates.land } } : {}),
  };
}

/**
 * shipHits on a kept law (the same sums, in the same order). With hit rates by kind, each bomber through counts at its
 * kind's rate, and `byKind` (the air battle's boxes dropped so far, by kind) takes the place of the strike's own mix;
 * the spoil, the naval skill and the harbour's factors are then not read (the at-anchor rows are the surprises).
 */
export function shipHitsBy(L: ShipLaw, through: number, byKind?: Partial<Record<HitKind, number>>): ShipHits {
  const S = CARRIERS.strike, H = CARRIERS.harbour;
  const sunk = noShips();
  let hits: number;
  let kinds: Record<HitKind, number> | undefined, flew: Record<HitKind, number> | undefined;
  if (L.rates && L.mix) {
    kinds = { dive: 0, torpedo: 0, level: 0, mast: 0 }; flew = { dive: 0, torpedo: 0, level: 0, mast: 0 };
    for (const k of HIT_KINDS) { flew[k] = byKind ? byKind[k] ?? 0 : through * L.mix[k]; kinds[k] = flew[k] * L.rates[k] * L.again * L.found; }
    hits = HIT_KINDS.reduce((s2, k) => s2 + kinds![k], 0);
  } else {
    hits = through * L.naval * S.hit * (1 - L.spoil) * L.again * L.found;
    if (L.harbour) hits *= H.hit * (L.surprise ? H.surprise : 1);
  }
  const have = L.have;
  const weight = SHIPS.reduce((s, c) => s + have[c] * S.aim[c], 0);
  if (weight <= 0 || hits <= 0) return { hits: 0, sunk, count: 0, value: 0, ...(kinds ? { kinds, flew } : {}) };
  let count = 0;
  for (const c of SHIPS) { sunk[c] = Math.min(have[c], ((hits * have[c] * S.aim[c]) / weight) / S.takes[c]); count += sunk[c]; }
  // No strike sinks more than so much of what it finds.
  const most = (L.harbour ? H.most : S.most) * SHIPS.reduce((s, c) => s + (S.aim[c] > 0 ? have[c] : 0), 0);
  if (count > most) { for (const c of SHIPS) sunk[c] *= most / count; if (kinds) for (const k of HIT_KINDS) kinds[k] *= most / count; hits *= most / count; count = most; }
  const told = kinds ? { kinds, flew: flew! } : {};
  if (L.tough) {
    const plain = { ...sunk }, tough = L.tough;
    const by = L.fleets.map((x, i) => {
      const part = noShips();
      for (const c of SHIPS) if (have[c] > 0) part[c] = (plain[c] * (x.ships[c] / have[c])) / (c === 'ss' ? 1 : tough[i] ?? 1);
      return part;
    });
    for (const c of SHIPS) sunk[c] = by.reduce((s, part) => s + part[c], 0);
    count = SHIPS.reduce((s, c) => s + sunk[c], 0);
    return { hits, sunk, count, value: SHIPS.reduce((s, c) => s + sunk[c] * S.value[c], 0), by, plain, ...told };
  }
  return { hits, sunk, count, value: SHIPS.reduce((s, c) => s + sunk[c] * S.value[c], 0), ...told };
}

/** How hard the fighters fight over this kind of raid: in the air war only a fight for the sky is fought to a finish. */
const clashOf = (g: Game, m: AirMission): number => (airWar(g) && m !== 'superiority' ? AIRWAR.losses.clash : 1);

/**
 * The largest share of its bombers one raid can lose. In the air war a day raid may lose up to the record's worst
 * (ruling 227: the Heligoland Bight, 55 to 65%), a night raid no more than before.
 */
const mostBomberLoss = (g: Game, night = false): number => (airWar(g) ? (night ? AIRWAR.losses.most : AIRWAR.losses.day) : AIR.combat.maxBomberLoss);

/** What the map says about one air campaign before it is flown: who can reach, who will meet them, and what helps each side. */
export function assessAir(g: Game, fleets: AirFleet[], m: AirMission, target: number): AirAssess {
  const C = AIR.combat;
  // A strike on one fleet is fought over the sea that fleet is in.
  const quarry = m === 'fleet' ? g.fleets.find((x) => x.id === target) ?? null : null;
  const sea = m === 'strike' || m === 'fleet', night = m === 'night';
  const zone = m === 'fleet' ? quarry?.zone ?? -1 : target;
  const nat = fleets[0]?.nat ?? '';
  const place = sea ? (g.seas[zone] ? seaWhat(g.seas[zone].name) : 'the sea') : g.regions[target]?.name ?? '';
  // Weather (ruling 127): the sky over the target, and the groups whose own sky is closed stay on the ground.
  const wx = weatherOn(g);
  const sky: Sky = wx ? (sea ? skyAtSea(g, zone) : skyAt(g, target)) : 'clear';
  const out: AirAssess = { why: null, place, att: [], def: [], E: 0, Epow: 0, B: 0, D: 0, Dpow: 0, radar: null, bomberLoss: 0, defence: 0, drilled: 0, aa: 0, guns: { light: 0, heavy: 0, all: 0 }, squadrons: 0, groups: 0, gunLoss: 0, lines: [], ships: [], flak: 0, spoil: 0, naval: 1, again: 1, surprise: false, sky, aim: cloudAim(sky, night) };
  if (!fleets.length) { out.why = 'No air fleet has these orders.'; return out; }
  if (sky === 'closed') { out.why = `Closed sky this month: nothing flies over ${place}.`; return out; }
  if (m === 'fleet' && (!quarry || !atSea(quarry) || !surfaceShips(quarry))) { out.why = 'That fleet is not at sea.'; return out; }
  if (m === 'fleet') out.ships = [quarry!];
  else if (m === 'harbour') out.ships = shipsInPort(g, target, nat);
  if (m === 'harbour' && !out.ships.length) { out.why = 'No enemy warships lie in harbour there.'; return out; }
  // A harbour raided in the month war is declared on its fleet is caught unready.
  out.surprise = m === 'harbour' && out.ships.some((x) => g.flags[`war:${nat}:${x.nat}`] === g.turn);
  const unready = out.surprise ? CARRIERS.harbour.unready : 1;
  for (const f of fleets) {
    if ((wx && grounded(g, f)) || wrecked(g, f) || fogBound(g, f)) continue;
    const d = sea ? kmToSea(g, f, zone) : kmTo(g, f, target);
    const R = reach(f, g);
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
  // Who comes up is comesUp()'s (ruling 225): over troops and ships any enemy fighter in reach, over the land itself the
  // groups that guard that ground, and carrier fighters only where their fleet is.
  // A raid flown on an order the defender's agent read (ruling 135(c)): every fighter in reach meets it in full,
  // with nothing lost for distance or for want of radar.
  const amb = !sea && ambushAt(g, target);
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || f.close || !atWar(g, f.nat, nat)) continue;
    if (!comesUp(g, f, m, target, zone)) continue;
    if ((wx && grounded(g, f)) || wrecked(g, f) || fogBound(g, f)) continue;
    const d = sea ? kmToSea(g, f, zone) : kmTo(g, f, target);
    let w = (amb ? (d <= reach(f, g).ftrRange ? AMBUSH() : 0) : cover(d, reach(f, g).ftrRange)) * crowding(g, f);
    // Carrier fighters are at home over the sea, and over their own fleet its lookouts warn them: they need no radar chain.
    const deck = f.ship !== undefined;
    if (sea && !deck) w *= C.seaCover;
    // (With the air campaigns on, the historian's dated row for the defender's nation, gated by Fighter control: b553(b), b558(2).)
    if (night) w *= acOn(g) ? nightFighters(g, f.nat) : C.night.fighters;
    if (!amb && !out.radar && !(deck && sea && d <= 0)) w *= AIR.radar.without;
    w *= unready;
    // Under cloud fewer of the defenders find the bombers.
    if (wx) w *= cloudMet(sky);
    if (w <= 0) continue;
    out.def.push({ f, d, w });
    out.D += f.ftr * w;
    // Fighters over their own ground are only as good as the radar and controllers that put them onto the raid.
    out.Dpow += f.ftr * w * worth(g, f) * techMul(g, f.nat, 'radar');
  }
  // The bombers' own defence: formation, guns and turrets. In the air war a day raid's is the nation's of the month (ruling 227).
  out.defence = bomberDefence(g, nat, night);
  const guns = out.B * out.defence;
  const I = intensity(m);
  // The defending fighters' share of the fighting weight over the target: the escort and the bombers' own guns are the
  // rest. In the air war an escort counts for more than its numbers: it breaks up the attacks before they are pressed.
  const escort = airWar(g) && !night ? AIRWAR.losses.escort : 1;
  const met = out.Dpow / Math.max(1, out.Dpow + out.Epow * escort + guns);
  const most = mostBomberLoss(g, night);
  // The air war counts its losses nearer history's: an escort saves far more than its numbers.
  const L = airWar(g) ? (night ? AIRWAR.losses.night : AIRWAR.losses) : null;
  // Under cloud the guns bring down fewer of the bombers (weather, ruling 127): 1 under a clear sky.
  const gun = cloudGuns(sky, night);
  // By night the guns round a target bring down half as many (ruling 107(b), behind its switch): 1 by day, or off.
  const aaGun = gun * nightGunShare(g, night);
  // Crews the player's instructors trained keep their formation: the fighters bring down fewer of them (bombers.ts).
  // The guns' share is as it was. 0 until the player has sent a crew to the schools, and always for everyone else.
  out.drilled = out.B > 0 ? out.att.reduce((t, x) => t + x.bmr * trainedEdge(g, x.f), 0) / out.B : 0;
  out.bomberLoss = out.B > 0 ? Math.min(most, (L ? L.bombers * Math.pow(met, L.power) : C.bombers * met) * (1 - out.drilled) * I + (night ? C.night.flak : C.flak) * gun * I) : 0;
  // Guns round the factories bring down more of the bombers that come for them.
  out.aa = m === 'bomb' || night || m === 'harbour' || m === 'town' ? gunsAt(g, target) : 0;
  if (out.aa && out.B > 0) out.bomberLoss = Math.min(most, out.bomberLoss + out.aa * WORKS.aa.flak * I * unready * aaGun);
  // The same defences as counts: the guns round a target on land, and the squadrons able to meet the raid.
  if (!sea && out.B > 0) out.guns = flakGuns(out.aa);
  out.groups = out.def.length;
  out.squadrons = out.def.reduce((n, x) => n + squadronsOf(x.f.nat, x.f.ftr * x.w, turnToDate(g.turn).year), 0);
  // Ships under attack: their own guns, what the fighter cover spoils, and what the crews are worth against ships.
  const shipGuns = out.ships.reduce((s, x) => s + SHIPS.reduce((n, c) => n + x.ships[c] * CARRIERS.flak.ships[c], 0), 0);
  // Ruling 240 (C1, C2): each fleet's guns count for the stage of its own navy's research, and for more again with
  // proximity shells (tech.ts flakMul: exactly 1 for every fleet while the switches are off, and then nothing here is
  // other than it was). `staged` is the weight so counted. Guns on land are not changed.
  const mul = out.ships.map((x) => flakMul(g, x.nat));
  const staged = mul.some((k) => k !== 1) ? out.ships.reduce((s, x, i) => s + SHIPS.reduce((n, c) => n + x.ships[c] * CARRIERS.flak.ships[c], 0) * mul[i], 0) : shipGuns;
  if (out.ships.length) {
    out.flak = Math.min(CARRIERS.flak.most, shipGuns * CARRIERS.flak.per) * unready * gun;
    // The cap goes by weight of guns: what the plain weight would bring down, never more than `most`, in the share the
    // staged weight is of the plain weight. So no navy lifts the cap for another lying beside it in harbour.
    if (staged !== shipGuns) out.flak = Math.min(CARRIERS.flak.most, shipGuns * CARRIERS.flak.per) * (staged / shipGuns) * unready * gun;
    if (out.B > 0) out.bomberLoss = Math.min(most, out.bomberLoss + out.flak * I);
    out.spoil = CARRIERS.cap.spoil * Math.sqrt(met);
    out.naval = out.B > 0 ? out.att.reduce((s, a) => s + a.bmr * navalSkill(a.f), 0) / out.B : 1;
    // Hit rates by kind (hitrates.ts; the design director, 22 November 18:27Z): the row the ships' cover gives, fighters
    // over them or not. Only with its switch on; the naval skill above is then not read.
    if (hitRatesOn(g)) {
      const r = strikeRates(out.att.filter((x) => x.bmr > 0).map((x) => {
        const deck = deckBred(x.f);
        return { nat: x.f.nat, bmr: x.bmr, deck, type: deck ? undefined : bomberOf(x.f.nat, designYear(g, x.f.nat, 'bombers')) };
      }), g.turn, m === 'harbour', out.D > 0);
      if (r) out.rates = r;
    }
    const before = Math.max(0, ...out.ships.map((x) => (x.struck && x.struck[0] === g.turn ? x.struck[1] : 0)));
    out.again = Math.pow(CARRIERS.strike.again, before);
    // Ruling 240 (C3): each fleet's damage control, by its own nation's research (tech.ts dcOf: exactly 1 for every
    // fleet while the switch is off, and then the assessment has no such key).
    const tough = out.ships.map((x) => dcOf(g, x.nat));
    if (tough.some((k) => k !== 1)) out.tough = tough;
  }
  // What of the bombers' loss is the guns' doing, for the raid card (the player's ask of 2026-11-16).
  if (out.B > 0) out.gunLoss = Math.min(out.bomberLoss, ((night ? C.night.flak : C.flak) * gun + out.aa * WORKS.aa.flak * unready * aaGun + out.flak) * I);

  // The Battle Ticket.
  const af = out.att.reduce((s, a) => s + a.f.ftr, 0);
  const ab = out.att.reduce((s, a) => s + (a.bmr > 0 ? a.f.bmr : 0), 0);
  const df = out.def.reduce((s, x) => s + x.f.ftr, 0);
  out.lines.push({
    label: 'Forces',
    value: `${[af ? planes(af, 'fighter') : '', ab ? planes(ab, 'bomber') : ''].filter(Boolean).join(' and ')} against ${df ? planes(df, 'fighter') : 'no fighters'}`,
    effect: out.D < df ? `About ${Math.round(out.D).toLocaleString('en-US')} of the defending fighters can meet the raids` : undefined,
  });
  // (A raid flown on an order the defender's agent read: the report says so, ruling 135(c).)
  const read = amb && out.D > 0 ? (g.intel.orders ?? []).find((o) => o.nat === nat && o.target === target && o.turn === g.turn && !o.done) : undefined;
  if (read) out.lines.push({ label: 'Intelligence', value: `Met by fighters posted on Agent ${read.agent}'s report`, effect: 'They knew the target and the hour: every fighter in reach came up' });
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
  // The defences in numbers: who can come up, and the guns below.
  if (out.groups) {
    out.lines.push({
      label: 'In range to intercept',
      value: `About ${planes(out.squadrons, 'fighter squadron')}`,
      effect: `From ${list(out.def.map((x) => theUnit(x.f.name)))}: about ${Math.round(out.D).toLocaleString('en-US')} fighters can meet the raid.`,
    });
  } else out.lines.push({ label: 'In range to intercept', value: 'No enemy fighters' });
  if (out.guns.all) {
    out.lines.push({
      label: 'Flak',
      value: `About ${out.guns.all.toLocaleString('en-US')} guns round ${place}`,
      effect: `${out.guns.heavy ? `${out.guns.heavy.toLocaleString('en-US')} heavy and ${out.guns.light.toLocaleString('en-US')} light` : 'Light guns only'}. They may bring down ${Math.round(out.gunLoss * 100) >= 1 ? `about ${Math.round(out.gunLoss * 100)}%` : 'almost none'} of the bombers.`,
    });
  }
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
  if (out.aa && m === 'harbour') out.lines.push({ label: 'Anti-aircraft guns', value: `Level ${out.aa} guns ring ${harbourOf(place)}`, effect: 'They bring down more of the bombers' });
  if (m === 'town') {
    const r = g.regions[target], gone = breachOf(g, r);
    out.lines.push({ label: 'The town', value: `Troops inside ${townName(r)} fight ${Math.round((townBonus(g, r) - 1) * 100)}% stronger${gone > 0 ? `, with ${Math.round(gone * 100)}% of its works already broken down` : ''}`, effect: `${SIEGE.bombard.raidFull} bombers getting through break down ${SIEGE.breach.raid}% of its works, fewer their share; bombs never break more than ${breachCeiling('raid')}% in all` });
  }
  if (out.aa && m === 'town') out.lines.push({ label: 'Anti-aircraft guns', value: `Level ${out.aa} guns ring ${place}`, effect: `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing and bring down more of the bombers` });
  else if (out.aa) out.lines.push({ label: 'Anti-aircraft guns', value: `Level ${out.aa} guns ring the factories of ${place}`, effect: `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing and bring down more of the bombers` });
  if (out.ships.length) {
    const pc = (x: number): string => `${Math.round(x * 100)}%`;
    if (out.surprise) out.lines.push({ label: 'Surprise', value: 'War was declared this month', effect: `The harbour is caught unready: its fighters and guns count ${pc(CARRIERS.harbour.unready)}, and the hits are multiplied by ${CARRIERS.harbour.surprise}` });
    if (m === 'harbour') out.lines.push({ label: 'In harbour', value: 'Ships at anchor cannot manoeuvre', effect: `Each bomber that gets through does ${CARRIERS.harbour.hit} times the damage it would at sea` });
    // Ruling 240 (C1, C2): with either rule in force the line gives the staged weight, then the strongest stage among the
    // fleets under attack (C1) and whether any of them fires proximity shells (C2), in the UI director's words. It never
    // names the research. With both rules off: release 1's line.
    const kits = shipGuns > 0 && (shipRule(g, 'stage') || shipRule(g, 'vt')) ? out.ships.map((x) => shipKit(g, x.nat)) : [];
    const stage = (['late', 'mid', 'early'] as const).find((s) => kits.some((k) => k.stage === s));
    const shells = kits.some((k) => k.vt);
    // (A share that rounds to nothing says so, as ruled for the city's guns: the UI director's rider 2 of 22 November.)
    if (kits.length) out.lines.push({ label: "Ships' guns", value: `Weight of anti-aircraft fire ${Math.round(staged)}${stage ? `, ${stage}-war guns${shells ? ' with proximity shells' : ''}` : shells ? ', with proximity shells' : ''}`, effect: Math.round(out.flak * I * 100) === 0 ? 'They may bring down almost none of the bombers' : `They may bring down about ${pc(out.flak * I)} of the bombers: battleships and carriers carry the most guns` });
    else if (shipGuns > 0) out.lines.push({ label: "Ships' guns", value: `Weight of anti-aircraft fire ${Math.round(shipGuns)}`, effect: `About ${pc(out.flak * I)} of the bombers brought down: battleships and carriers carry the most guns` });
    const decks = out.def.filter((x) => x.f.ship !== undefined);
    if (out.D >= 1) out.lines.push({ label: 'Fighter cover', value: decks.length ? `${planes(Math.round(decks.reduce((s, x) => s + x.f.ftr * x.w, 0)), 'carrier fighter')} over the ships${decks.length < out.def.length ? ', and fighters from the coast' : ''}` : 'Fighters from the coast', effect: `They break up about ${pc(out.spoil)} of the strike. An escort of fighters cuts that` });
    else out.lines.push({ label: 'Fighter cover', value: 'None', effect: 'Nothing breaks up the strike' });
    if (out.B > 0) {
      const land = out.att.filter((a) => a.bmr > 0 && !deckBred(a.f)), deckB = out.att.filter((a) => a.bmr > 0 && deckBred(a.f));
      out.lines.push({ label: 'Crews', value: deckB.length && land.length ? 'Carrier crews and land bombers' : deckB.length ? 'Carrier crews, trained to hit ships' : 'Land bombers', effect: land.length ? `A land bomber counts ${pc(navalSkill(land[0].f))} of a carrier aircraft against ships${deckB.length ? `: the strike as a whole counts ${pc(out.naval)}` : ''}` : undefined });
    }
    if (out.again < 1) out.lines.push({ label: 'Already struck', value: 'The fleet has been struck this month', effect: `It is scattered and warned: this strike counts ${pc(out.again)}` });
    if (out.ships.some((x) => atSea(x) && fleetKnown(g, x, nat) !== 'exact')) out.lines.push({ label: 'Search', value: 'The ships are known only from a report', effect: `About ${pc(CARRIERS.strike.report)} of the strike finds them. Identify them first, by a search of that sea or by contact, for a full strike` });
  }
  if (df > 0 || out.radar) {
    out.lines.push(out.radar
      ? { label: 'Radar', value: `${cap(out.radar.name)} ${out.radar.plural ? 'give' : 'gives'} warning`, effect: 'Every defending fighter in reach gets up in time, and fewer aircraft are caught on the ground' }
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

/** What a nation's aircraft weigh against troops this month, beside their numbers: its weight row where it has one (ruling 227, b424), else in full. */
export const troopWeight = (g: Game, nat: string): number => airWeight(g, nat) ?? 1;

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
export function fly(g: Game, nat: string, m: AirMission, target: number, fleets: AirFleet[], salt = '', battle?: FoughtAir): AirReport | null {
  const a = assessAir(g, fleets, m, target);
  if (a.why) return null;
  // A fought battle that knew the bombers by name: each such group's roster is made up now, before any loss is booked,
  // exactly as the battle's ticket made it up on a copy (bombers.ts seatsOf), so the bombers the battle names are found
  // on it. After the assessment, which the ticket made on the roster as it stood. A draw of the group's own stream only.
  if (battle?.fell) for (const x of a.att) if (battle.fell[x.f.id]) muster(g, x.f);
  const C = AIR.combat, I = intensity(m);
  const sea = m === 'strike' || m === 'fleet', night = m === 'night';
  // The sea fought over: the one struck, or the one the fleet struck is in.
  const zone = m === 'fleet' ? a.ships[0].zone : target;
  // Where at sea, as the UI director's sea words say it: "in the North Sea", "off the Azores".
  const seaAt = sea && g.seas[zone] ? seaWhere(g.seas[zone].name) : `in ${a.place}`;
  const rng = airRng(g, `${m}:${target}:${nat}${salt}`);
  // A raid fought out as an air battle (airfought.ts) books the field's own losses; otherwise the dice roll.
  // Handed to the staff partway, the dice roll as for any raid, and the battle's part is added to what they give the rest.
  const staff = battle?.staff;
  const luck = (): number => (battle && !staff ? 1 : rng.range(1 - C.luck, 1 + C.luck));
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
  const bLoss = a.B * Math.min(mostBomberLoss(g, night), a.bomberLoss * luck());
  // The bombers' guns bring down some of the fighters that close with them, the more the better their defence (ruling 227).
  const gunKills = gunsDown(g, a, night, bLoss, m, target);
  // A raid fought out: the field's own losses stand (b397), the bombers' gunners' kills with them.
  const fix = battle && !staff ? settleFought(battle, a.B) : null;
  // Pilots who come down over their own ground mostly fly again; over the enemy's, or the sea, few do.
  const saved = (home: boolean): number => (sea ? (AIR.pilots.home + AIR.pilots.away) / 2 : home ? AIR.pilots.home : AIR.pilots.away);
  // What the battle had already cost a group when it was handed to the staff (none when it was not).
  const had = (f: AirFleet) => (staff ? battle!.lost[f.id] : undefined) ?? { ftr: 0, bmr: 0, back: 0 };
  const down = (f: AirFleet, side: AirSide, n: number, home: boolean, rest = 1): void => {
    // Handed to the staff: what was lost stands, and the dice settle the rest's share of the pool (rest 1 at once).
    const h = had(f), rolled = fix ? 0 : rng.roll(Math.max(0, n * rest));
    const k = Math.min(f.ftr, fix ? fix.ftr(f) : h.ftr + rolled);
    if (!k) return;
    const p = airPool(g, f.nat);
    const back = fix ? fix.back(f, k) : Math.min(k, h.back) + rng.roll(Math.max(0, k - h.ftr) * saved(home));
    f.ftr -= k; f.lost.ftr += k; side.lostFtr += k;
    p.lostFtr += k; p.pilots += back; p.lostPilots += k - back;
  };
  const bomber = (f: AirFleet, side: AirSide, n: number): void => {
    const k = Math.min(f.bmr, fix ? fix.bmr(f) : had(f).bmr + rng.roll(Math.max(0, n * (staff ? staff.rest.bmr : 1))));
    f.bmr -= k; f.lost.bmr += k; side.lostBmr += k;
    airPool(g, f.nat).lostBmr += k;
  };
  a.def.forEach((x, i) => down(x.f, def[i], a.D > 0 ? (dLoss + gunKills) * ((x.f.ftr * x.w) / a.D) : 0, true, staff?.rest.foe));
  a.att.forEach((x, i) => {
    down(x.f, att[i], a.E > 0 ? eLoss * (x.ftr / a.E) : 0, false, staff?.rest.ftr);
    bomber(x.f, att[i], a.B > 0 ? bLoss * (x.bmr / a.B) : 0);
  });
  // The bombers' gunners' kills, for each group's record (ruling 227). Fought, those the field booked to its boxes.
  // Rolled, the gunners' part (gunKills beside the fighters' dLoss) of the fighters the defence lost, shared by bombers;
  // handed to the staff, the field's so far and that part of what the dice added.
  const guns: Record<number, number> = {};
  if (fix) for (const x of a.att) guns[x.f.id] = battle?.guns?.[x.f.id] ?? 0;
  else {
    const lostD = def.reduce((s, x) => s + x.lostFtr, 0), was = a.def.reduce((s, x) => s + had(x.f).ftr, 0);
    const part = dLoss + gunKills > 0 ? gunKills / (dLoss + gunKills) : 0;
    const more = wholeShares(Math.round(Math.max(0, lostD - was) * part), a.att.map((x) => x.bmr));
    a.att.forEach((x, i) => { guns[x.f.id] = (staff ? battle?.guns?.[x.f.id] ?? 0 : 0) + more[i]; });
  }
  const lostB = att.reduce((s, x) => s + x.lostBmr, 0);
  // The bombers that bomb: fought, those that reached the release on the field, including any lost after it (b384(a));
  // handed to the staff, all but those lost on the way in and those that had turned back.
  const bombed = fix ? fix.through()
    : Math.max(0, Math.min(a.B, a.B - lostB + (staff ? Math.min(lostB, battle?.lostAfter ?? 0) - staff.away : 0)));
  const through = bombed * (night ? C.night.accuracy : dayAim(g, nat, m)) * techMul(g, nat, 'bombers') * a.aim;

  // What the bombers did.
  let result = '', value = 0, ground = 0, shipsSunk = 0, notable = false;
  // Damaged cities (citydamage.ts): what the bombs did to the town itself, where they did anything.
  let city: CityHit | null = null;
  let struck: SeaSide[] | undefined;
  // Recon photos (photos.ts): what a raid on works, oil or a railway did, for the photo taken the day after.
  let photo: RaidPhoto | null = null;
  if (m === 'fleet' || m === 'harbour') {
    // A strike on ships: the hits fall on the classes the crews aim for, and each fleet under attack takes its share.
    const exp = shipHits(a, m, through, foundShare(g, a, nat));
    const have = noShips();
    for (const x of a.ships) for (const c of SHIPS) have[c] += x.ships[c];
    const afloatBefore = a.ships.reduce((s, x) => s + surfaceShips(x), 0);
    const total = noShips();
    // Ruling 240 (C3): one draw for each fleet and class, as ever, on the fleet's own figure; and what that same draw
    // would have sunk without damage control (`plain`: the figure of release 1) says which ships it kept afloat.
    const plain = exp.plain ?? exp.sunk;
    const afloat: { x: Fleet; kept: Ships }[] = [];
    const keptAll = noShips();
    struck = a.ships.map((x) => ({ nat: x.nat, fleet: x.name, admiral: x.admiral?.name ?? null, ships: { ...x.ships }, sunk: noShips() }));
    a.ships.forEach((x, i) => {
      const kept = noShips();
      for (const c of SHIPS) {
        if (have[c] <= 0) continue;
        const share = plain[c] * (x.ships[c] / have[c]);
        // Fought out, the field's own draw stands (ruling 238): the dice are still thrown, so nothing after them moves.
        // Handed to the staff, what had sunk stays sunk and the staff's dice decide the rest from all the bombs.
        const [n, n0] = rng.rollBoth(exp.by ? exp.by[i][c] : share, share);
        const field = battle?.sunk?.[x.id]?.[c];
        const k = Math.min(x.ships[c], field === undefined ? n : staff ? Math.max(field, n) : field);
        // The ships kept afloat come from whichever sinkings stand (the design director, 13:38Z on step 9): the dice's;
        // the field's, counted on its own throw (battle.kept, ShipBook kept()); handed to the staff, the field's where
        // what it had sunk stands (as many as the dice or more), else the dice's. With no fleet's damage control other
        // than 1 there is no `kept` from the field and n0 is n: none, and nothing is said.
        kept[c] = field !== undefined && (!staff || field >= n) ? battle?.kept?.[x.id]?.[c] ?? 0 : Math.min(x.ships[c], n0) - k;
        x.ships[c] -= k; x.lost[c] += k; struck![i].sunk[c] = k; total[c] += k; keptAll[c] += kept[c];
        loseStars(x, c, k);
      }
      mendStars(x);
      afloatWatch.tell?.({ route: 'air', nat: x.nat, sunk: struck![i].sunk, saved: kept });
      // (The report keeps them beside the sinkings, for the after card of a fought raid: never with the switch off.)
      if (shipCount(kept)) { afloat.push({ x, kept }); struck![i].kept = { ...kept }; }
      // Ships that are hit and not sunk limp home: the fleet is the less fit for sea.
      x.ready = Math.max(0, x.ready - Math.min(0.3, 0.05 + CARRIERS.strike.ready * (afloatBefore > 0 ? exp.hits / afloatBefore : 0)));
      x.struck = [g.turn, (x.struck && x.struck[0] === g.turn ? x.struck[1] : 0) + 1, atSea(x) ? x.zone : -1];
      // It has been seen by those who struck it, and the strike gives away the decks it came from.
      (x.seen ??= {})[nat] = g.turn;
      for (const y of a.att) { const c = carrierOf(g, y.f); if (c && atSea(c)) (c.seen ??= {})[x.nat] = g.turn; }
    });
    // The battleships and carriers that went down, by name, for the card that marks them (a record only).
    struck.forEach((x, i) => nameSide(g, x, `air:${(g.flags.airBattles ?? 0) + 1}:${i}`));
    shipsSunk = shipCount(total);
    value += SHIPS.reduce((s, c) => s + total[c] * CARRIERS.strike.value[c], 0);
    const whom = a.ships.length === 1 ? `the ${a.ships[0].name}` : `the ${a.ships.length} fleets`;
    result = through <= 0 ? 'No bombers got through.'
      // ("Two battleships, 3 carriers ... of the Pacific Fleet are sunk at anchor": a small opening figure in words, and a verb.)
      : shipsSunk ? `${shipList(total).replace(/^(\d+)\b/, (d) => COUNT_HEAD[Number(d)] ?? d)} of ${whom} ${shipsSunk === 1 ? 'is' : 'are'} sunk${m === 'harbour' ? ' at anchor' : ''}.`
      // (Ruling 240, the UI director's rider 2: a ship saved and none sunk is one sentence in the past, for every fleet struck;
      // rider 3: several fleets by name, three and then "and N more".)
      : afloat.length ? `Ships of ${a.ships.length === 1 ? whom : fleetsNamed(a.ships)} were hit, but none was sunk: ${keptAfloatPast(keptAll)}.`
      : exp.hits >= 0.5 ? `Ships of ${whom} were hit, but none was sunk.` : `The bombers did ${whom} little harm.`;
    // (Ruling 240: beside ships sunk, the ships damage control kept afloat: after the one fleet struck, named once ("One of
    // its cruisers is ..."); with several fleets, a line for each that has any.)
    if (shipsSunk && afloat.length) result += ` ${afloat.map((y) => keptAfloat(y.x, y.kept, a.ships.length === 1 ? 'its' : 'fleet')).join(' ')}`;
    // Hit rates by kind: the hits each kind of bomber scored (the design director: the raid card shows them).
    if (exp.kinds && exp.flew && through > 0) { const line = hitsLine(exp.kinds, exp.flew); if (line) result += ` ${line}`; }
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
    let cut = a.aa ? open * (1 - a.aa * WORKS.aa.cut) : open;
    // Air campaigns (ruling 236.2): the damage carries over, and comes back at the historian's rate for the target.
    if (acOn(g)) cut = bombLands(g, nat, r, cut, night, aimOf(g, fleets));
    const nation = total > 0 ? cut * (r.fac / total) : 0;
    raid(g, nat, r.ctrl, nation);
    // The air war: output is weighed by the province's own factories (ruling 227), so one with no industry has none to
    // stop, and the raid is judged on its railway alone.
    value += outputWorth(g, r, cut);
    // (No line where no bombs fell.)
    result = r.fac > 0 && cut > 0 ? `The bombing stopped ${Math.round(cut * 100)}% of ${r.name}'s output for a month: ${tenthPct(nation)} of ${g.nations[r.ctrl].adj} production.` : '';
    // The grand campaign: a heavy raid wrecks a factory outright, and the bombs that fall on the yards and
    // junctions may cut the province's railway. Both stay as they are until engineers come.
    // Fought out as an air battle, the field's own throws decide both (b397), so the campaign cuts the line the field showed cut.
    const wrecked = bombFactories(g, nat, r, cut, battle?.rolls?.ruin);
    if (wrecked) { value += 300; notable = true; result += ` A factory was wrecked outright: it earns nothing until engineers rebuild it.`; }
    const railCut = bombRail(g, nat, target, through, a.aa ? a.aa * WORKS.aa.cut : 0, a.aa ?? 0, battle?.rolls?.rail);
    if (railCut) { value += 300; notable = true; result += ` The railway through ${r.name} is cut: no train runs until engineers relay it.`; }
    else if (!result) result = through > 0 ? `The bombs fell on the rail yards of ${r.name}, but the line still runs.` : 'No bombers got through.';
    // Damaged cities: the bombs that fall on the town leave it damaged until its holder pays to repair it.
    city = cityHit(g, r, 'raid', through, gunsStop(a.aa ?? 0), night);
    if (city) { value += city.worth; result += ` ${city.words}`; }
    if (nat === g.human && photosOn(g)) photo = raidPhoto(g, { aim: acOn(g) ? aimOf(g, fleets) : 'works', fac: r.fac, bombed, through, cut, wrecked, rail: railCut, city: city?.added ?? 0, night, sky: a.sky });
    result = result.trim();
  } else if (m === 'town') {
    // A raid on a town: part of its works are broken down for the assault, and the troops inside are shaken.
    const r = g.regions[target];
    const hit = hitTown(g, r, 'raid', townShare('raid', through) * (a.aa ? 1 - a.aa * WORKS.aa.cut : 1));
    value += hit.share * hit.divs * SIEGE.bombard.value;
    result = through > 0 ? townHitText(g, r, hit, 'bombs', true) : 'No bombers got through.';
    // Damaged cities: the bombs that break down its works fall on its streets too.
    city = cityHit(g, r, 'town', hit.share);
    if (city) { value += city.worth; result += ` ${city.words}`; }
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
    const cut = troopCut(r, foes, (through + low * G.fighters) * troopWeight(g, nat));
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
    // (Ruling 240, C3: each fleet's damage control eases its own losses, on the same draws; the ships it kept afloat are told.)
    const afloat: string[] = [];
    afloatWatch.strike?.(a, nat, target);
    for (const f of g.fleets) {
      if (f.zone !== target || !atSea(f) || !atWar(g, nat, f.nat) || !surfaceShips(f)) continue;
      const kept = none();
      const s = sinkShips(f, frac, rng, [...SURFACE], dcOf(g, f.nat), kept);
      nameLosses(g, f.nat, f.name, s, `shipping:${(g.flags.airBattles ?? 0) + 1}:${f.id}`); // their names are gone with them (a record only)
      afloatWatch.tell?.({ route: 'shipping', nat: f.nat, sunk: s, saved: kept, fleet: f, frac, dc: dcOf(g, f.nat) });
      f.ready = Math.max(0, f.ready - 0.1);
      for (const c of SURFACE) sunk[c] += s[c];
      if (shipCount(kept)) afloat.push(keptAfloat(f, kept));
    }
    if (sunk.cv && carrierRules(g)) syncWings(g);
    // Trade: an ocean convoy route through this sea, or the coasters along an enemy shore.
    const hurt = tradeHurt(g, nat, target, through);
    for (const [victim, s] of Object.entries(hurt)) raid(g, nat, victim, s);
    const worst = Object.entries(hurt).sort((x, y) => y[1] - x[1])[0];
    shipsSunk = shipCount(sunk);
    value += shipCount(sunk) * 30 + (worst ? worst[1] * 3000 : 0);
    result = [shipCount(sunk) ? `${cap(shipList(sunk))} sunk.` : '', ...afloat, worst ? `Merchant shipping losses cost ${g.nations[worst[0]].name} ${tenthPct(worst[1])} of a month's production.` : ''].filter(Boolean).join(' ') || 'The bombers found little to hit.';
  }

  // Has the sky been won? The attacker's surviving fighters against what the defender can put up once his losses are made good.
  let sky = false, hold = 0, held = false;
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
    // Air campaigns (ruling 236.2): the battle for the sky over a province carries over from month to month.
    if (acOn(g)) held = skyFought(g, nat, target, sky) === 'held';
  }

  // Wear, and what the commanders learned.
  const aLost = att.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
  const dLost = def.reduce((s, x) => s + x.lostFtr + x.lostBmr, 0);
  const ratio = (dLost + value + 10) / (aLost + 10);
  // Fought to the end, the verdict is the field's (b331; the design director, b384(a)): the same measure on the losses
  // the field itself saw, with what the bombs did, and a side that broke off everything lost. The booked losses are the
  // field's (b397), so the two agree; should a group's strength cut a loss short, it books a bare close result the
  // field's way, never the opposite.
  const rolled = sky || ratio >= 1;
  const seen = (rows: { f: AirFleet }[]): number => rows.reduce((t, x) => t + (battle!.lost[x.f.id]?.ftr ?? 0) + (battle!.lost[x.f.id]?.bmr ?? 0), 0);
  const fieldWon = battle && !staff ? (battle.gaveUp ? battle.gaveUp === 'def' : sky || (seen(a.def) + value + 10) / (seen(a.att) + 10) >= 1) : null;
  const won = fieldWon ?? rolled;
  for (const x of a.att) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - (night ? AIR.ready.nightSortie : AIR.ready.sortie)); gainXp(x.f, won); }
  for (const x of a.def) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - AIR.ready.defend * I * (night ? 0.2 : 1)); gainXp(x.f, !won); }
  const level: VictoryLevel = fieldWon !== null && fieldWon !== rolled ? (won ? 'Close victory' : 'Close defeat')
    : sky || ratio >= 2 ? 'Decisive victory' : ratio >= 1 ? 'Close victory' : ratio >= 0.5 ? 'Close defeat' : 'Decisive defeat';

  g.flags.airBattles = (g.flags.airBattles ?? 0) + 1;
  const troops = m === 'ground' || m === 'line' ? g.regions[target].units.find((u) => !u.dead && atWar(g, nat, u.nat))?.nat : undefined;
  const defNat = troops ?? struck?.[0]?.nat ?? def[0]?.nat ?? (sea ? '' : g.regions[target].ctrl);
  const rep: AirReport = {
    id: g.flags.airBattles, turn: g.turn, mission: m, region: sea ? -1 : target, zone: sea ? zone : -1, place: a.place,
    attNat: nat, defNat, att, def, lines: a.lines, odds: a.Dpow > 0 ? a.Epow / a.Dpow : 9, level, won, sky, hold, result,
  };
  if (struck) { rep.ships = struck; if (m === 'fleet') rep.fleet = target; if (a.surprise) rep.surprise = true; }
  // A raid fought out as an air battle keeps how many bombers bombed, for the battle's after card.
  if (battle && a.B > 0) rep.dropped = bombed;
  // The grand campaign keeps what the raid was worth beside what it cost: a staff learns from its own results.
  if (g.flags.open === 1) rep.worth = Math.round(value);
  if (city) rep.city = { region: city.region, added: city.added, now: city.now };
  if (photo) rep.photo = photo;
  g.airReports.push(rep);
  storyAir(g, m === 'fleet' ? 'strike' : m === 'harbour' || m === 'town' ? 'bomb' : m, sea ? zone : target, a.att.map((x) => x.f), a.def.map((x) => x.f), { sky, sunk: shipsSunk, attLost: aLost, defLost: dLost, guns, ...(battle?.fell ? { fallen: battle.fell, shot: battle.shot } : {}) });
  // Named aces (aces.ts): each fighter group's ace may be named, add to his tally, or fall. Off with the switch.
  acesFought(g, att, a.att.map((x) => x.f), def, a.def.map((x) => x.f), sea ? -1 : target, a.place);

  // Groups of one fleet name the fleet once: "The Mobile Fleet's strike wing and carrier fighters" (the UI director, mend 6).
  const flying = fleets.filter((f) => a.att.some((x) => x.f === f));
  const deck = flying.length > 1 ? carrierOf(g, flying[0]) : null;
  const who = deck && flying.every((f) => carrierOf(g, f) === deck && f.name.startsWith(`${deck.name} `))
    ? `The ${deck.name}'s ${list([...flying].sort((x, y) => (x.kind === 'bmr' ? 0 : 1) - (y.kind === 'bmr' ? 0 : 1)).map((f) => f.name.slice(deck.name.length + 1)))}`
    : list(flying.map((f) => f.name));
  const cost = `${planes(aLost, 'aircraft').replace('aircrafts', 'aircraft')} lost`;
  const adj = defNat ? g.nations[defNat].adj : 'enemy';
  // Once the British and American air forces fight the same enemy side by side, the sky they win is the Allies'.
  const together = !!defNat && (nat === 'UK' || nat === 'USA') && ['UK', 'USA'].every((x) => g.nations[x]?.alive && g.nations[x].atWar.includes(defNat));
  let text: string;
  if (m === 'superiority') text = `${who} ${a.att.length === 1 ? 'fights' : 'fight'} for the sky over ${a.place}: ${dLost} ${adj} aircraft destroyed, ${cost}. ${sky ? (together ? 'The Allied air forces now hold the sky there.' : `The ${g.nations[nat].adj} air force now holds the sky there.`) : held ? (nat === g.human ? 'The sky there is still yours.' : `The ${g.nations[nat].adj} air force still holds the sky there.`) : df(def) ? 'The defenders still hold their sky.' : 'No fighters came up to meet them.'}`;
  else if (m === 'fleet') text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} the ${a.ships[0].name} ${seaAt}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'harbour') text = `${who} ${a.att.length === 1 ? 'raids' : 'raid'} ${harbourOf(a.place)}${a.surprise ? ' without warning' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'town') text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} the defences of ${townName(g.regions[target])}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (sea) text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} at shipping ${seaAt}. ${result} ${cap(cost)}.`;
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
    : sea ? `shipping struck ${seaAt}`
    : m === 'line' ? `the ${adj} fieldworks at ${a.place} bombed`
    : m === 'ground' ? `the ${adj} troops at ${a.place} raided`
    : `${a.place} bombed${night ? ' by night' : ''}`;
  // Who had the better of it, for an interface that sets the news out as won and lost (Version 80, display only).
  const fought = { arm: 'air' as const, ...(defNat ? (won ? { won: nat, lost: defNat } : { won: defNat, lost: nat }) : {}) };
  if (full) headline(g, 'battle', text, nat, sea ? undefined : target, fought);
  else routineNews(g, `air:${nat}:${m}:${sea ? `z${zone}` : target}`, 'battle', text, `${brief}${aLost ? ` (${aLost} aircraft lost)` : ''}`, nat, sea ? undefined : target, fought);
  return rep;
}

const df = (sides: AirSide[]): number => sides.reduce((s, x) => s + x.ftr, 0);

/** The air campaigns a nation ordered this month, flown once all its orders are given. */
export function airBattles(g: Game, nat: string): AirReport[] {
  const out: AirReport[] = [];
  if (!g.nations[nat]?.alive) return out;
  // Air campaigns (ruling 236.2): each standing campaign gives its groups their orders for the month first.
  if (acOn(g)) campaignOrders(g, nat);
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
  if (acOn(g)) campaignResults(g, nat, out);
  return out;
}

// ---- The computer's air staff ----------------------------------------------------------------

/**
 * Orders for one computer-run nation's air fleets: history's plan where it has one, otherwise support for the armies.
 * 'ph' is the month's orders for its main army, whose named offensive draws the air groups (ruling 227, b447).
 */
export function airAI(g: Game, nat: string, ph?: PlanPhase | null): void {
  // The player's air force is his own to order, unless he has handed the air war to his staff (depth.ts).
  if (byHand(g, nat, 'air')) return;
  const n = g.nations[nat];
  // His air staff leaves the carriers' wings to whoever runs his navy.
  // (Air campaigns, ruling 236.2: a group flying in a campaign is the campaign's, and the staff leaves it alone.)
  const mine = airFleetsOf(g, nat).filter((f) => (!afloat(f) || !byHand(g, nat, 'navy')) && !inCampaign(g, f));
  // A group that has flown this month already stays away until the next.
  for (const f of mine) if (f.flew !== g.turn) { f.mission = 'support'; f.target = -1; }
  // (Its ground support is decided afresh each month, once its fighters are based: closeStaff.)
  if (closeRules(g)) for (const f of mine) delete f.close;
  if (!n.alive || !n.atWar.length) return;
  // In the grand campaign history's air plan does not apply: the air groups patrol, and the staff plans raids of its own.
  // Ships come first: the carriers' wings, and a few of the land bombers, go for the fleets the staff has found.
  if (g.flags.open === 1) { flyReadOrders(g, nat, mine); seaStaff(g, nat, nat === g.human ? 'land' : 'all'); raidStaff(g, nat, mine, ph); return; }
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
 * Where a staff's fighters stand (ruling 227, b428): each month its fighter groups are based by the threat, every
 * front weighed by the enemy aircraft that can reach the nation's troops there (troops beside ground an enemy holds), and the homeland by the enemy bombers
 * that can reach its cities and works. Each enemy group's weight (its aircraft, at their real weight in the air) is
 * shared among the places it can reach, and each place asks for that share of the nation's fighters. A group goes to
 * the airfield where it meets most of what is still asked for, and moves only when that is clearly better than where it
 * is (airwar.json ai.basing). Groups that flew or moved this month, carrier wings, and groups in reach of a beach the
 * staff means to land on stay where they are. Home defence is kept for the raids the staff expects, no more.
 */
function baseByThreat(g: Game, nat: string, mine: AirFleet[], beaches: number[], battles: number[] = []): void {
  const K = AIRWAR.ai.basing;
  const own = (r: Region): boolean => r.ctrl === nat && r.owner === nat;
  // The places to guard: the nation's own troops at the front (beside ground an enemy holds), and its own cities and works.
  const troops = new Set<number>();
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort' && !troops.has(c.reg) && g.regions[c.reg].nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) troops.add(c.reg);
  const cities = g.regions.filter((r) => own(r) && (r.fac > 0 || r.vp > 0)).map((r) => r.id);
  const places = [...new Set([...troops, ...cities])];
  if (!places.length) return;
  const at = new Map(places.map((id, i) => [id, i]));
  const ask = new Float64Array(places.length);
  for (const e of g.airFleets) {
    if (!atWar(g, nat, e.nat) || e.ftr + e.bmr <= 0) continue;
    const R = reach(e, g), w = airWeight(g, e.nat) ?? 1;
    // Its fighters and bombers go for troops; only its bombers for cities and works.
    const hits: [number, number][] = [];
    for (const id of places) {
      const d = kmTo(g, e, id);
      const n = (troops.has(id) ? (d <= R.ftrRange ? e.ftr : 0) : 0) + (d <= R.bmrRange ? e.bmr : 0);
      if (n > 0) hits.push([at.get(id)!, n]);
    }
    const all = hits.reduce((s, x) => s + x[1], 0);
    if (!all) continue;
    // Each enemy group counts once, its weight shared among the places it reaches.
    const weight = Math.max(...hits.map((x) => x[1])) * w;
    for (const [i, n] of hits) ask[i] += (weight * n) / all;
  }
  // The troops fighting the battles of the staff's main effort ask for the sky there (b447): the margin over the
  // enemy aircraft that can reach them that holds it by numbers (airwar.json sky 'own'), not their bare share.
  if (battles.length) for (const id of troops) if (battles.includes(id) || g.regions[id].nb.some((j) => battles.includes(j))) ask[at.get(id)!] *= Math.max(1, AIRWAR.sky.own);
  // Ruling 52(a): the fighters come up behind the army whether or not enemy aircraft are over it. Each stretch of
  // front asks for a standing share besides, by the divisions the nation has there: 'presence' of all that the
  // threat asks for, and all of the fighters where nothing threatens at all.
  const P = g.flags.open === 1 ? K.presence ?? 0 : 0;
  if (P > 0 && troops.size) {
    const divs = new Map<number, number>();
    let all = 0;
    for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort' && troops.has(c.reg)) { divs.set(c.reg, (divs.get(c.reg) ?? 0) + c.divs); all += c.divs; }
    const threat = ask.reduce((s, x) => s + x, 0);
    const share = threat > 0 ? threat * P : 1;
    if (all > 0) for (const [id, d] of divs) ask[at.get(id)!] += (share * d) / all;
  }
  const asked = ask.reduce((s, x) => s + x, 0);
  if (asked <= 0) return;
  const fighters = mine.filter((f) => f.kind === 'ftr' && f.ftr > 0 && !afloat(f));
  const stays = (f: AirFleet): boolean => f.moved === g.turn || f.flew === g.turn || f.mission !== 'support' || beaches.some((t) => kmTo(g, f, t) <= reach(f, g).ftrRange);
  const free = fighters.filter((f) => !stays(f));
  if (!free.length) return;
  const total = fighters.reduce((s, f) => s + f.ftr, 0);
  for (let i = 0; i < ask.length; i++) ask[i] *= total / asked;
  // The places within a fighter's reach of each airfield, worked out once.
  const longest = Math.max(...fighters.map((f) => reach(f, g).ftrRange));
  const near = new Map<number, [number, number][]>();
  const nearOf = (base: number): [number, number][] => {
    let l = near.get(base);
    if (!l) {
      l = [];
      for (let i = 0; i < places.length; i++) { const d = km(g, base, places[i]); if (d <= longest) l.push([i, d]); }
      near.set(base, l);
    }
    return l;
  };
  const covered = new Float64Array(places.length);
  const meets = (f: AirFleet, base: number, gain: boolean): number => {
    const R = reach(f, g).ftrRange;
    let v = 0;
    for (const [i, d] of nearOf(base)) {
      const c = cover(d, R);
      if (c <= 0) continue;
      const n = f.ftr * c;
      if (gain) covered[i] += n;
      else v += Math.min(n, Math.max(0, ask[i] - covered[i]));
    }
    return v;
  };
  /** What a group would serve from a base of all that is asked within its reach there, whoever else covers it already. */
  const serves = (f: AirFleet, base: number): number => {
    const R = reach(f, g).ftrRange;
    let v = 0;
    for (const [i, d] of nearOf(base)) { const c = cover(d, R); if (c > 0) v += Math.min(f.ftr * c, ask[i]); }
    return v;
  };
  for (const f of fighters) if (stays(f)) meets(f, f.base, true);
  // The airfields open to this nation's groups (whyNotBase, leaving out each group's own base and how far it can fly in a month).
  const open = g.regions.filter((r) => whyNotBase(g, free[0], r, true) === null || r.id === free[0].base);
  // Ruling 52(a), its second part: a move behind the army never takes a group out of reach of the enemy. A group
  // that can reach ground an enemy holds from where it stands goes only to a field from which it still can.
  const foes = P > 0 ? g.regions.filter((r) => atWar(g, nat, r.ctrl)).map((r) => r.id) : [];
  const toFoe = new Map<number, number>();
  const foeAt = (base: number): number => {
    let d = toFoe.get(base);
    if (d === undefined) { d = Infinity; for (const id of foes) d = Math.min(d, km(g, base, id)); toFoe.set(base, d); }
    return d;
  };
  for (const f of free.sort((a, b) => b.ftr - a.ftr || a.id - b.id)) {
    const here = meets(f, f.base, false);
    const R = reach(f, g).ftrRange;
    const engaged = P > 0 && foeAt(f.base) <= R;
    let best: { r: Region; v: number } | null = null, far: { r: Region; v: number } | null = null;
    const inFerry = (r: Region): boolean => r.id !== f.base && !overOcean(g, f, r) && km(g, f.base, r.id) <= AIR.ferry && fieldRoom(g, r.id, f);
    for (const r of open) {
      if (r.id === f.base || overOcean(g, f, r) || !fieldRoom(g, r.id, f)) continue;
      const near = km(g, f.base, r.id) <= AIR.ferry;
      // (Ruling 52(a): a field beyond a month's flight is weighed too, to be reached by stages. Only for a group out of the fight.)
      if (!near && (P <= 0 || engaged)) continue;
      if (engaged && foeAt(r.id) > R) continue;
      const v = meets(f, r.id, false);
      if (near) { if (!best || v > best.v) best = { r, v }; } else if (!far || v > far.v) far = { r, v };
    }
    const need = here * K.better + K.least * f.ftr;
    if (best && best.v > need && rebaseAir(g, f, best.r)) { meets(f, best.r.id, true); continue; }
    if (far && far.v > need) {
      // Too far to fly in a month: it goes to the open field within a month's flight that lies nearest to it.
      let step: Region | null = null, sd = km(g, f.base, far.r.id);
      for (const r of open) { if (!inFerry(r)) continue; const d = km(g, r.id, far.r.id); if (d < sd) { sd = d; step = r; } }
      if (step && rebaseAir(g, f, step)) { meets(f, step.id, true); continue; }
    }
    // Ruling 52(a), b489: a group left where nothing within its reach asks for fighters at all (a campaign that is
    // over, a front that has moved on) does not stand idle there, though every ask is met without it. It goes to the
    // open field within a month's flight from which it would serve the most of what is asked.
    if (P > 0 && K.idle !== false && here <= 0 && serves(f, f.base) <= 0) {
      let back: Region | null = null, most = 0;
      for (const r of open) { if (!inFerry(r)) continue; const v = serves(f, r.id); if (v > most) { most = v; back = r; } }
      if (back && rebaseAir(g, f, back)) { meets(f, back.id, true); continue; }
    }
    meets(f, f.base, true);
  }
}

/**
 * The battles of a staff's named main effort this month (ruling 227, b447): the places its main army's offensive is
 * aimed at, on its main front alone, or the sector of a main effort it has named, where an enemy holds the country.
 * While the army is being brought up for a great blow the staff has named, the ground opposite the assembly: the
 * air comes up with the army, and is ready when the blow falls. None while the army stands on the defensive.
 */
export function effortBattles(g: Game, nat: string, ph?: PlanPhase | null): number[] {
  const out = new Set<number>();
  if (ph?.posture === 'redeploy') {
    for (const nm of ph.assembly ?? []) {
      const r = g.byName[nm];
      if (r) for (const j of r.nb) if (!allied(g, nat, fieldCtrl(g, g.regions[j]))) out.add(j);
    }
    return [...out];
  }
  if (ph?.posture !== 'attack') return [];
  for (const nm of ph.main ?? ph.targets ?? ph.objectives ?? []) {
    const r = g.byName[nm];
    if (r && atWar(g, nat, fieldCtrl(g, r))) out.add(r.id);
  }
  return [...out];
}

/**
 * A staff's ground support (close air support, the design director's b536): while its main army is on the attack,
 * the staff puts on ground support the land fighter groups that reach the battles of its main effort, nearest first,
 * up to close.ai.share of its land fighters; the rest patrol, guard its troops and cities, and fly its sweeps and
 * escorts as before. On the defensive, or while the army is being brought up, every group patrols. The grand campaign,
 * with the switch on, only.
 */
function closeStaff(g: Game, mine: AirFleet[], battles: number[]): void {
  if (!closeStaffRules(g) || !battles.length) return;
  const land = mine.filter((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined && !afloat(f));
  let left = CLOSE.ai.share * land.reduce((s, f) => s + f.ftr, 0);
  const near = (f: AirFleet): number => Math.min(...battles.map((b) => kmTo(g, f, b)));
  const can = land
    .filter((f) => f.mission === 'support' && f.flew !== g.turn && f.ready >= CLOSE.least && battles.some((b) => cover(kmTo(g, f, b), reach(f, g).ftrRange) > 0))
    .map((f) => ({ f, d: near(f) }))
    .sort((a, b) => a.d - b.d || a.f.id - b.f.id);
  for (const { f } of can) {
    if (f.ftr > left) continue;
    f.close = true;
    delete f.escorts;
    left -= f.ftr;
  }
}

/**
 * Where a staff's bombers stand (ruling 227, b447): while it has a main effort, each bomber group goes to the airfield
 * from which it reaches most of that effort's battles, and moves only when that is clearly better than where it is
 * (airwar.json ai.basing, as the fighters). With no main effort the bombers stay where they are. Groups that flew or
 * moved this month, and carrier wings, stay too.
 */
function baseBombers(g: Game, nat: string, mine: AirFleet[], battles: number[]): void {
  if (!battles.length) return;
  const K = AIRWAR.ai.basing;
  const bombers = mine.filter((f) => f.kind !== 'ftr' && f.bmr > 0 && !afloat(f) && f.moved !== g.turn && f.flew !== g.turn && f.mission === 'support');
  if (!bombers.length) return;
  const worth = (f: AirFleet, base: number): number => {
    const R = reach(f, g).bmrRange;
    let v = 0;
    for (const t of battles) v += cover(km(g, base, t), R);
    return v * f.bmr;
  };
  const open = g.regions.filter((r) => whyNotBase(g, bombers[0], r, true) === null || r.id === bombers[0].base);
  for (const f of bombers.sort((a, b) => b.bmr - a.bmr || a.id - b.id)) {
    const here = worth(f, f.base);
    const fields: { r: Region; v: number }[] = [];
    for (const r of open) {
      if (r.id === f.base || overOcean(g, f, r) || km(g, f.base, r.id) > AIR.ferry || !fieldRoom(g, r.id, f)) continue;
      fields.push({ r, v: worth(f, r.id) });
    }
    const top = Math.max(0, ...fields.map((x) => x.v));
    if (!(top > here * K.better + K.least * f.bmr)) continue;
    // Bombers stand back: of the fields that reach the battles near enough as well as the best ('better'), the one
    // farthest from them, leaving the forward fields to the fighters.
    const back = (r: Region): number => Math.min(...battles.map((t) => km(g, r.id, t)));
    const to = fields.filter((x) => x.v * K.better >= top).sort((a, b) => back(b.r) - back(a.r) || a.r.id - b.r.id)[0];
    if (to && to.v > here * K.better + K.least * f.bmr) rebaseAir(g, f, to.r);
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
function raidStaff(g: Game, nat: string, mine: AirFleet[], ph?: PlanPhase | null): void {
  if (!airWar(g)) return;
  const R = AIRWAR.ai.raids;
  const B = bombingFor(g, nat);
  const rules = B ? { ...R, ...B } : { ...R, share: R.share * raidsMul(g, nat) };
  const you = B ? seat(g) : null;
  const rng = airRng(g, `staff:${nat}`);
  const used = new Set<number>();
  // (A group on ground support flies over the army's battles, not on raids or sweeps.)
  const fit = (f: AirFleet): boolean => !used.has(f.id) && f.flew !== g.turn && f.mission === 'support' && !f.close;
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
  // So is the sky over a beach the staff means to land on (the second front, ai.ts): no invasion fleet crosses
  // under the enemy's fighters. With no fighter group in reach of it, one based overseas is brought up to the
  // friendly airfield nearest the beach.
  // (A player who has named his crossing and handed over the air war has the sky over his beach fought for whenever it is not his: it keeps the enemy's ships and aircraft off his troopships.)
  const wanted = (t: number): boolean => { if (skyDenies(g, nat, t)) return true; const sky = skyOver(g, t); return nat === g.human && !(sky !== null && allied(g, sky, nat)); };
  const beaches = ['', ':o'].map((s) => (g.flags[`st:aim:${nat}${s}`] ?? 0) - 1).filter((t, i, a) => t >= 0 && a.indexOf(t) === i && !!g.regions[t] && atWar(g, nat, g.regions[t].ctrl) && wanted(t));
  // (A player's air staff, for the crossing he has named: it brings up to 'monthly' fighter groups forward each
  // month until 'groups' of them can reach the beach or the airfields in reach of it are full, from the homeland
  // too while more than 'home' groups still guard it.)
  const BP = nat === g.human ? R.beach : undefined;
  const roomOn = (base: number, f: AirFleet): boolean => {
    const there = g.airFleets.reduce((s, x) => s + (x.base === base && x.ship === undefined ? x.ftr + x.bmr : 0), 0);
    return there + f.ftr + f.bmr <= (airWar(g) ? Math.max(AIRWAR.fields.least, airfieldsHold(g, base)) : airfieldsHold(g, base));
  };
  for (const t of beaches) {
    const fighters = mine.filter((f) => f.kind === 'ftr' && f.ftr > 0 && !afloat(f));
    const cap = g.nations[nat].capital;
    const guards = (f: AirFleet): boolean => g.regions[f.base].owner === nat && !g.regions[f.base].overseas && cap >= 0;
    for (let k = 0; k < (BP ? BP.monthly : 1); k++) {
      const within = fighters.filter((f) => whyNotMission(g, f, 'superiority', t) === null).length;
      if (within >= (BP ? BP.groups : 1)) break;
      const spare = !!BP && fighters.filter(guards).length > BP.home;
      let move: { f: AirFleet; to: Region; d: number } | null = null;
      for (const f of fighters) {
        // The fighters that guard the homeland stay over it.
        if (f.moved === g.turn || f.flew === g.turn || (guards(f) && !spare)) continue;
        if (BP && whyNotMission(g, f, 'superiority', t) === null) continue;
        for (const r of airBases(g, f)) {
          const d = km(g, r.id, t);
          // (Not onto airfields that are full already: aircraft that cannot be served fly nowhere. Nor across an
          // ocean, where a group goes only by sea.)
          if (!roomOn(r.id, f) || overOcean(g, f, r)) continue;
          // (Of two groups that could go as near, a player's staff sends the one that is not over the capital.)
          if (d <= reach(f, g).ftrRange && (!move || d < move.d || (!!BP && d === move.d && move.f.base === cap && f.base !== cap))) move = { f, to: r, d };
        }
      }
      if (!move) break;
      rebaseAir(g, move.f, move.to);
    }
  }
  // The rest of the fighters are based by the threat to the nation's troops and cities (ruling 227, b428): the beaches
  // have first call on them, and a group in reach of a beach stays. The battles of the staff's main effort ask for
  // the sky, and its bombers go in reach of them (b447).
  const battles = effortBattles(g, nat, ph);
  baseByThreat(g, nat, mine, beaches, battles);
  baseBombers(g, nat, mine, battles);
  if (ph?.posture === 'attack') closeStaff(g, mine, battles);
  let flown = 0;
  for (let k = 0; k < R.sweeps && (front.size || beaches.length); k++) {
    let best: { t: number; fleets: AirFleet[]; v: number } | null = null;
    const bases = [...new Set(mine.filter((f) => fit(f) && f.kind === 'ftr' && f.ftr > 0 && f.ready >= R.ready).map(homeOf))];
    const spots = [...beaches.filter((t) => !hit.has(t)).map((t): [number, number] => [t, 6]), ...[...front.entries()].filter(([t]) => !hit.has(t) && pushed(t) && skyOver(g, t) === null).sort((a, b) => b[1] - a[1] || a[0] - b[0])].slice(0, R.look);
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
    // (A player's staff, over the beach of the crossing he has named: every fit fighter group in reach goes up
    // together, from whatever fields they stand on, and it flies when it expects to win the sky, with 'sky' times
    // the odds that win it. The sky is what he asked for; the margin a staff keeps for its own offensives is not.)
    if (BP) for (const t of beaches) {
      if (hit.has(t)) continue;
      const fleets = mine.filter((f) => fit(f) && f.kind === 'ftr' && f.ftr > 0 && f.ready >= R.ready && f.ready >= AIRWAR.ai.escortReady && whyNotMission(g, f, 'superiority', t) === null);
      if (!fleets.length) continue;
      const a = assessAir(g, fleets, 'superiority', t);
      if (a.why || a.E < AIR.sky.least) continue;
      const fc = forecastAir(g, a, 'superiority', t);
      if (fc.odds < AIR.sky.win * BP.sky) continue;
      const v = R.skyWorth + fc.foeLoss - fc.ftrLoss;
      if (v > R.worth && (!best || v > best.v)) best = { t, fleets, v };
    }
    if (!best) break;
    send('superiority', best.t, best.fleets, flown++);
  }

  // The fieldworks in front of a main effort come first with the bombers (rivals.json, 'soften'): every bomber group
  // in reach of the province the staff has named goes at the works there, with the fighters on their fields, before
  // the army attacks. The works on a front where a general offensive is under way are weighed with the other raids below.
  const lineAt = (t: number): void => {
    if (hit.has(t) || !g.regions[t]) return;
    const bombers = leads().filter((f) => whyNotMission(g, f, 'line', t) === null).sort((a, b) => b.bmr - a.bmr || a.id - b.id);
    if (!bombers.length) return;
    const fleets = [...bombers];
    for (const base of new Set(bombers.map(homeOf))) fleets.push(...fightersAt(base).filter((e) => whyNotMission(g, e, 'line', t) === null));
    const a = assessAir(g, fleets, 'line', t);
    if (a.why) return;
    const fc = forecastAir(g, a, 'line', t);
    if (fc.bomberLoss > rules.loss || fc.success < R.success || fc.cut < rivalsData.soften.raid) return;
    send('line', t, fleets, flown++);
  };
  const aims = Object.keys(g.flags).filter((k) => k === `st:eff:${nat}` || k.startsWith(`st:eff:${nat}:t`)).map((k) => g.flags[k] - 1).sort((a, b) => a - b);
  for (const t of aims) lineAt(t);

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
        if (fc.bomberLoss > rules.loss || fc.success < R.success || (m === 'line' && fc.cut < rivalsData.soften.raid)) return;
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
      // The fieldworks where its own offensive is going in: months of digging taken off before the army attacks.
      for (const r of troops) if (pushed(r.id) && whyNotMission(g, f, 'line', r.id) === null) weigh('line', r, [f, ...escorts.filter((e) => whyNotMission(g, e, 'line', r.id) === null)]);
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

/**
 * Orders an agent of the player's read last month (ruling 135, behind its switch): flown before anything else the
 * staff does, as written, if they still can be; a stolen order is never false. One that cannot be flown lapses, and
 * the player is told why.
 */
function flyReadOrders(g: Game, nat: string, mine: AirFleet[]): void {
  ordersDue(g, nat).forEach((o, k) => {
    const t = g.regions[o.target];
    const fleets = o.groups.map((id) => mine.find((f) => f.id === id)).filter((f): f is AirFleet => !!f && f.flew !== g.turn && f.mission === 'support' && f.ship === undefined && f.base === o.field && whyNotMission(g, f, o.mission, o.target) === null);
    const why = !t || !atWar(g, nat, t.ctrl) ? "the target is no longer the enemy's" : !fleets.some((f) => f.bmr > 0) ? 'the bombers have left the field or cannot fly' : null;
    if (why) { orderLapsed(g, o, why); return; }
    flyingOrder(g, o.target);
    const rep = fly(g, nat, o.mission, o.target, fleets, `:order${k}`);
    flyingOrder(g, null);
    if (!rep) { orderLapsed(g, o, 'the bombers could not reach it'); return; }
    for (const f of fleets) { f.mission = o.mission; f.target = o.target; f.flew = g.turn; }
    o.done = 'flown';
  });
}

/**
 * The order a computer staff would write today for the bomber groups on one of its fields (ruling 135, read by an
 * agent): the strongest bomber group there fit to fly, with the fighters on the field by day, against the target the
 * staff's own reckoning picks among its enemies' factories and junctions, by day or by night. Null when the staff
 * would fly nothing from there. The same weighing as the staff's (raidStaff), for one field.
 */
export function fieldOrder(g: Game, nat: string, field: number): { groups: number[]; target: number; mission: 'bomb' | 'night'; bombers: number } | null {
  if (!airWar(g)) return null;
  const R = AIRWAR.ai.raids;
  const B = bombingFor(g, nat);
  const rules = B ? { ...R, ...B } : { ...R, share: R.share * raidsMul(g, nat) };
  const you = B ? seat(g) : null;
  const here = (f: AirFleet): boolean => f.nat === nat && f.ship === undefined && f.base === field && f.mission === 'support' && f.flew !== g.turn;
  const lead = g.airFleets.filter((f) => here(f) && f.kind !== 'ftr' && f.bmr >= rules.least && f.ready >= R.ready).sort((a, b) => b.bmr - a.bmr || a.id - b.id)[0];
  if (!lead) return null;
  const escorts = g.airFleets.filter((x) => here(x) && x.kind === 'ftr' && x.ftr > 0 && x.ready >= AIRWAR.ai.escortReady);
  const S = R.sour;
  let best: { m: 'bomb' | 'night'; t: number; fleets: AirFleet[]; v: number } | null = null;
  const weigh = (m: 'bomb' | 'night', r: Region, fleets: AirFleet[]): void => {
    if (S && g.turn < (g.flags[`rso:${nat}:${m}:${r.id}`] ?? 0)) return;
    const a = assessAir(g, fleets, m, r.id);
    if (a.why) return;
    const fc = forecastAir(g, a, m, r.id);
    if (fc.bomberLoss > rules.loss || fc.success < R.success) return;
    const railWorth = r.fac <= 0 ? rules.rail : 1;
    const v = (fc.worth * railWorth + fc.foeLoss) * (you !== null && r.ctrl === you ? rules.focus ?? 1 : 1) - fc.ftrLoss - fc.bmrLoss;
    if (v > R.worth && (!best || v > best.v)) best = { m, t: r.id, fleets, v };
  };
  const works = g.regions
    .filter((r) => atWar(g, nat, r.ctrl) && (r.fac > 0 || railsAt(r.id).length >= 3) && whyNotMission(g, lead, 'bomb', r.id) === null)
    .sort((a, b) => b.fac + rules.rail * railsAt(b.id).length - (a.fac + rules.rail * railsAt(a.id).length) || a.id - b.id)
    .slice(0, R.look);
  for (const r of works) {
    weigh('bomb', r, [lead, ...escorts.filter((e) => whyNotMission(g, e, 'bomb', r.id) === null)]);
    weigh('night', r, [lead]);
  }
  const b = best as { m: 'bomb' | 'night'; t: number; fleets: AirFleet[]; v: number } | null;
  return b ? { groups: b.fleets.map((f) => f.id), target: b.t, mission: b.m, bombers: Math.round(b.fleets.reduce((s, f) => s + f.bmr, 0)) } : null;
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
  /** Of the defenders' losses, those the bombers' own guns bring down (ruling 227); the rest fall to the escort. */
  foeGuns: number;
  /** The chance the raid costs the enemy more than it costs the raiders. */
  success: number;
  /** What the raid should achieve beyond aircraft shot down, counted in aircraft: output stopped, a railway cut, a factory wrecked, an army shaken. */
  worth: number;
  /** A strike on ships: the ships of each class it should sink, and the hits it should score. */
  ships?: Ships;
  hits?: number;
  /** A raid on a town: what it should do to the works and to the troops inside. */
  town?: TownHit;
  /** Damaged cities (the grand campaign with its switch): the percent of the town the bombs should leave in ruins. */
  city?: number;
}

/** The share of a strike that finds the ships it is sent against: all of it where they are identified, less where they are known only from a report. */
export function foundShare(g: Game, a: AirAssess, nat: string): number {
  // A harbour does not move: only ships at sea have to be found.
  return a.ships.some((x) => atSea(x) && fleetKnown(g, x, nat) !== 'exact') ? CARRIERS.strike.report : 1;
}

/**
 * The law fly() weighs a raid's bombs by, from the bombers that drop (airfought.ts BombLaw): the output stopped, the
 * railway cut and a factory wrecked, for a raid on a province by day or by night. Null for any other raid. An air battle
 * carries it, so the field says as the bombs fall what the campaign will make of them (the design director, b397).
 */
export function bombLaw(g: Game, a: AirAssess, m: AirMission, target: number): BombLaw | null {
  if (m !== 'bomb' && m !== 'night') return null;
  const N = AIR.damage.industry, r = g.regions[target], nat = a.att[0]?.f.nat ?? '';
  const stop = a.aa ? a.aa * WORKS.aa.cut : 0;
  const R = RAILWAYS.bomb, U = RAILWAYS.ruin;
  return {
    // As fly() aims them: the air war's day accuracy (227) and the weather on the day (127).
    hit: (m === 'night' ? AIR.combat.night.accuracy : dayAim(g, nat, m)) * techMul(g, nat, 'bombers') * a.aim,
    // As fly() prices them: each whole share of output stopped worth what outputWorth says of the province.
    out: { scale: N.scale, most: N.max, keep: 1 - stop, worth: outputWorth(g, r, 1) },
    // Where the line can be cut at all, and a factory wrecked at all: the rules' own tests, asked at the most.
    rail: railBombChance(g, target, 1e12) > 0 ? { scale: R.scale, most: R.most, stop } : null,
    ruin: ruinChance(g, r, 1) > 0 ? { over: U.over, chance: U.chance, cap: 0.95 } : null,
  };
}

/**
 * What a mission is likely to do, without the dice, for the player to see before he gives the order:
 * the fighter odds going in, the share of the bombers likely to be lost, and what those that get through should achieve.
 */
/**
 * Aircraft come down whole: the flight rounds each group's share of a loss up or down by its own dice. The chance of
 * each whole number of aircraft lost in all, `total` spread over groups by `shares`.
 */
function whole(total: number, shares: number[]): Map<number, number> {
  let out = new Map<number, number>([[0, 1]]);
  for (const s of shares.length ? shares : [1]) {
    const x = Math.max(0, total * s), n = Math.floor(x), f = x - n, next = new Map<number, number>();
    for (const [k, p] of out) {
      next.set(k + n, (next.get(k + n) ?? 0) + p * (1 - f));
      if (f > 0) next.set(k + n + 1, (next.get(k + n + 1) ?? 0) + p * f);
    }
    out = next;
  }
  return out;
}

/**
 * A night raid's chance as the flight throws it (the night guns' switch). The flight throws three dice of luck: on the
 * bombers brought down, on the defenders' fighters lost and on the escort's. Each is spread evenly over its range, and
 * at every throw the raid is weighed, with and without a cut railway and a wrecked factory, by the reckoning fly() uses
 * to call it won.
 */
function nightChance(g: Game, a: AirAssess, target: number, nat: string, escortLoss: number, foeFighters: number): number {
  const C = AIR.combat, N = AIR.damage.industry, r = g.regions[target];
  const gunsStop = a.aa ? a.aa * WORKS.aa.cut : 0;
  const spread = (n: number): number[] => Array.from({ length: n }, (_, i) => 1 - C.luck + (2 * C.luck * (i + 0.5)) / n);
  const bombers = spread(12), fighters = foeFighters > 0 || escortLoss > 0 ? spread(4) : [1];
  const bmrShares = a.att.filter((x) => x.bmr > 0).map((x) => x.bmr / Math.max(1, a.B));
  const defShares = a.D > 0 ? a.def.map((x) => (x.f.ftr * x.w) / a.D) : [];
  let won = 0, throws = 0;
  for (const lb of bombers) {
    const lost = a.B * Math.min(mostBomberLoss(g, true), a.bomberLoss * lb);
    const through = Math.max(0, a.B - lost) * C.night.accuracy * techMul(g, nat, 'bombers') * a.aim;
    const cut = Math.min(N.max, through / N.scale) * (1 - gunsStop);
    const pr = groundRules(g) ? railBombChance(g, target, through, gunsStop) : 0;
    const pf = groundRules(g) ? ruinChance(g, r, cut) : 0;
    // (Damaged cities: the lasting damage to the town counts with the output stopped.)
    const worth = outputWorth(g, r, cut) + cityWorthOf(g, r, cityForecast(g, r, 'raid', through, gunsStop, true)) + 10, guns = gunsDown(g, a, true, lost, 'night', target);
    for (const ld of fighters) {
      for (const le of fighters) {
        // Aircraft come down whole: each side's losses are rounded up or down by the flight's own dice.
        let p = 0;
        for (const [down, pd] of whole(foeFighters * ld + guns, defShares)) {
          for (const [gone, pg] of whole(lost, bmrShares)) {
            const esc = escortLoss * le;
            const wins = (more: number): number => (down + worth + more >= gone + esc + 10 ? 1 : 0);
            p += pd * pg * ((1 - pr) * (1 - pf) * wins(0) + (pr * (1 - pf) + (1 - pr) * pf) * wins(300) + pr * pf * wins(600));
          }
        }
        won += p;
        throws++;
      }
    }
  }
  return won / throws;
}

export function forecastAir(g: Game, a: AirAssess, m: AirMission, target: number): AirForecast {
  const C = AIR.combat, I = intensity(m);
  const nat = a.att[0]?.f.nat ?? '';
  const through = a.B * (1 - a.bomberLoss) * (m === 'night' ? C.night.accuracy : dayAim(g, nat, m)) * techMul(g, nat, 'bombers') * a.aim;
  const odds = a.Dpow > 0 ? a.Epow / a.Dpow : 9;
  // The fighter battle as the flight settles it, at even luck.
  const se = Math.sqrt(a.Epow), sd = Math.sqrt(a.Dpow);
  const share = se + sd > 0 ? se / (se + sd) : 0;
  const clash = Math.min(a.E, a.D) * clashOf(g, m);
  let ftrLoss = C.fighters * I * clash * 2 * (1 - share) * C.escortShare;
  const bmrLoss = a.B * a.bomberLoss;
  // The defenders' losses: to the escort, and to the bombers' own guns (ruling 227).
  const foeGuns = gunsDown(g, a, m === 'night', bmrLoss, m, target);
  const foeLoss = C.fighters * I * clash * 2 * share + foeGuns;
  let cut = 0, nation = 0, value = 0;
  let rail: number | undefined, ruin: number | undefined;
  let ships: Ships | undefined, hits: number | undefined;
  let lumps: [number, number][] | undefined, expected = 0;
  let town: TownHit | undefined;
  let city: number | undefined;
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
    value = outputWorth(g, r, cut);
    // The grand campaign: the chance of cutting the province's railway, and of wrecking a factory outright.
    if (groundRules(g)) {
      rail = railBombChance(g, target, through, a.aa ? a.aa * WORKS.aa.cut : 0);
      ruin = ruinChance(g, r, cut);
      if (r.fac <= 0) cut = 0;
    }
    // Damaged cities: the lasting damage the bombs should leave on the town, and what that is worth.
    const pts = cityForecast(g, r, 'raid', through, gunsStop(a.aa ?? 0), m === 'night');
    if (pts > 0) { city = pts; value += cityWorthOf(g, r, pts); }
  } else if (m === 'strike') {
    cut = Math.min(AIR.damage.shipping.maxSink, AIR.damage.shipping.sink * (through / (through + AIR.damage.shipping.scale)));
    const afloat = g.fleets.reduce((s, f) => s + (f.zone === target && atSea(f) && atWar(g, nat, f.nat) ? surfaceShips(f) : 0), 0);
    const worst = Math.max(0, ...Object.values(tradeHurt(g, nat, target, through)));
    // Ruling 240 (C3), rider 4 (the design director, 22 November 11:12Z; item (d) of 03:57Z before it): the fight sinks
    // each class of each fleet at the strike's share times navy.ts toughOf, navy.json's `tough` for the class over the
    // fleet's own damage control (sinkShips through dueAt). With the dc switch on the forecast counts each struck fleet's
    // surface ships at that same figure, class by class, so the worth the staffs weigh (carriers.ts bestBlow) and the
    // card's share are the fight's expected loss: the staff weighs what it fights. With the switch off every figure here
    // is release 1's to the bit.
    const fought = shipRule(g, 'dc') ? g.fleets.reduce((s, f) => s + (f.zone === target && atSea(f) && atWar(g, nat, f.nat) ? SURFACE.reduce((n, c) => n + f.ships[c] * toughOf(g, f, c), 0) : 0), 0) : afloat;
    if (fought !== afloat) { value = cut * fought * 30 + worst * 3000; cut *= fought / afloat; }
    else value = cut * afloat * 30 + worst * 3000;
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
    const pts = cityForecast(g, g.regions[target], 'town', town.share);
    if (pts > 0) { city = pts; value += cityWorthOf(g, g.regions[target], pts); }
  } else if (m === 'line') {
    cut = lineWear(through);
    value = cut * g.regions[target].units.reduce((s, c) => s + (!c.dead && atWar(g, nat, c.nat) && worksOf(c) > 0 ? c.divs : 0), 0) * AIRWAR.line.value;
  } else if (m === 'ground') {
    const G = AIRWAR.ground;
    const r = g.regions[target];
    const foes = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));
    const fire = a.E * G.fire;
    ftrLoss += fire;
    cut = troopCut(r, foes, (through + Math.max(0, a.E - ftrLoss) * G.fighters) * troopWeight(g, nat));
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
  // With the night guns' switch (ruling 107(b); 133(c)(4), b491): by night the luck of a raid lies almost all in the
  // bombers brought down, a narrower spread than the day's, so the card weighs that luck as the flight throws it.
  if (m === 'night' && airPiece(g, 'night')) success = nightChance(g, a, target, nat, ftrLoss, foeLoss - foeGuns);
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
  const fc: AirForecast = { odds, bomberLoss: a.bomberLoss, cut, nation, rail, ruin, ftrLoss, bmrLoss, foeLoss, foeGuns, success, worth: value + 300 * (pr + pf), ships, hits, town };
  if (city !== undefined) fc.city = city;
  return fc;
}
