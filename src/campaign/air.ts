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
import qualityData from '../../data/campaign/air-quality.json';
import { Rng, hashString } from '../core/rng';
import { clamp, tenthPct } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { allied, atWar, ensureSupply, headline, newsWeek, routineNews, theWords, thornGround, worksOf } from './game';
import { techMul, techNotes } from './research';
import { fighterMonth, fighterTypesOn, lawK, typeEdge } from './fighters';
import { armsOf, intoPool, unitsDecide, wayNow } from './units';
import { SKY } from '../battle/air/book';
import { dcOf, flakMul, hasTech, shipKit, shipRule, type GunStage, type ShipResearch } from './tech';
import { NAVY, afloatWatch, aswKit, atSea, fleetsNamed, passageWhy, keptAfloat, keptAfloatPast, shipCount, shipList, sinkShips, surfaceShips, toughOf, wholeShares, zoneOf } from './navy';
import { patrolFigures, patrolMet, patrolOn, patrolReach, patrolStaff } from './seapatrol';
import { fleetKnown, seaStaff, syncWings } from './carriers';
import { muster, theUnit, trainedEdge } from './bombers';
import { grade, storyAir, storyEdge } from './stories';
import { nameLosses, nameSide } from './hulls';
import { loseStars, mendStars } from './stars';
import { groundRules } from './ground';
import { RAILWAYS, bombFactories, bombRail, onNet, railBombChance, railCut, railsAt, ruinChance } from './rails';
import { bombingFor, raidsMul, seat } from './difficulty';
import { fuelAir } from './resources';
import { SIEGE, besieged, besiegerOf, breachCeiling, breachOf, fieldCtrl, hitTown, planTownHit, siegeRules, townBonus, townHeld, townHitText, townShare, townWorksPhrase, type TownHit } from './siege';
import { airfieldOpenTo, prizeOut } from './prizes';
import { townName } from './towns';
import { beyondEurope, farOn } from './world';
import { hopAims, hopsBind, skiesOn } from './pacific';
import { stopDef } from './history';
import { settleFought, type BombLaw, type FoughtAir, type TroopLaw } from './airfought';
import { strafeOn, strafeWatch } from './strafers';
import { cityForecast, cityHit, cityWorthOf, type CityHit } from './repairs';
import { cloudAim, cloudGuns, cloudMet, grounded, skyAt, skyAtSea, weatherOn, whyNotSky, zoneOfRegion, zoneOfSea, type Sky } from './weather';
import { airPiece, nightGunShare, whyNotField, wrecked } from './airversion';
import { cityDefences, cityNight, lightsAt, lightsFloor, markNightRaid } from './citydefences';
import { FIELD, burnedRoom, damageField, fieldKnown, fieldRules, fieldShut, fieldTier, planFieldHit, type FieldBlow, type FieldHit } from './fieldstate';
import { sightOf } from './intel';
import { raidFogOn, rememberSkies, staffScouts, staffWeigh } from './airfog';
import { RESEARCH } from './research';
import { AMBUSH, ambushAt, flyingOrder, orderLapsed, ordersDue } from './secretwar';
import { armedAt, atomRaidWorth, strikeFlown } from './atom';
import { acOn, aimOf, bombLands, campaignOrders, campaignResults, chainAt, gunnerRatio, inCampaign, nightFighters, recordField, skyFought, stockLoss } from './aircampaigns';
import type { AirFleet, AirMission, AirPool, AirReport, AirSide, Corps, Fleet, Game, PlanPhase, Region, SeaSide, ShipClass, Ships } from './types';
import { chartDist, globeDist, globeLeg, mapDist, nearX } from './wrap';
import { seaWhat, seaWhere } from './sea-words';
import { baseField } from './bargains';
import { flagName } from './flagnames';
import { ferryOn, staffFerry } from './ferry';
import { AIRNAMES, airBlocked, airNamesOn, nextNumber, type Label, type RungR2 } from './airnames';
import { nicknamesOn } from './nicknames';
import { airRaidFlown, airSupportFlown } from './airnicks';

type Sched = [number, number, number][];
interface NatSpec { quality: number; ftrRange: number; bmrRange: number }
interface Chain { nat: string; from: number; name: string; by: string; ground: number; plural?: boolean }
interface FleetRow {
  nat: string; name: string; base: string; from?: number; leader?: string; start?: [number, number]; est: Sched; ftrRange?: number; scripted?: boolean; home?: boolean;
  /**
   * The far side's rows (rulings 184 and 187; data/campaign/far-air.json, whose note says where each comes from).
   * 'bmrRange' and 'quality': this command's own, in place of its nation's. 'ally': it may stand on ground an ally
   * holds (America's in Australia). 'arrives': its country builds no aircraft on these rows, so what the record adds
   * to the command arrives with the date. 'moves': the record's forward bases, [month, province], taken by a computer
   * staff while the Pacific's book stands, once, and only while its side holds the ground. 'posted': while that book
   * stands a computer staff leaves the command on the record's field (posted, below). 'byBook' (ruling 234's rider
   * b518, 6): a fall in its figure by date, to nothing too, is the record's own move or a battle's work: it is taken at
   * the stops that keep the book, and at the others the command is kept at the most the record has given it so far
   * (kept, below). 'watch': it stands apart from the rest of its country's air force, watching this neighbour (the
   * Soviet Far East's on Japan): while the two are at peace a computer staff leaves it on its fields at every stop,
   * and at every stop what the record adds to it arrives and what the record takes from it leaves with the date,
   * never through the pool (the aircraft that went west in December 1941 are on Europe's rows already).
   * 'to' (rider b525): the record ends this command by a change of name, and these are the rows that took its place.
   * It is not stood down by the date: it ends in the first month from the record's that every one of them stands,
   * and what it holds passes to them (handOver, below). 'leaves' (b525): [month, fighters, bombers, who, foe], a part
   * of the command that leaves by the record for a reason outside the game (the Soviet volunteers in China, April
   * 1941): it leaves with the date at every stop, and never through the pool; only where no book is kept and `who`
   * is at war with `foe` that month, it stays.
   */
  bmrRange?: number; quality?: number; ally?: boolean; arrives?: boolean; moves?: [number, string][]; posted?: boolean; byBook?: boolean; watch?: string;
  to?: string[]; leaves?: [number, number, number, string, string][];
}
export interface AirOrder { fleet: string; base?: string[]; mission: AirMission; region?: string; sea?: string }
export interface AirPlan { nat: string; from: number; to: number; name: string; against: string; orders: AirOrder[] }
interface Reserve { ftr: number; bmr: number; pilots: number }

export const AIR = data as unknown as {
  kmPerUnit: number;
  ferry: number;
  nations: Record<string, NatSpec>;
  range: { fullAt: number; atLimit: number };
  radar: { without: number; chains: Chain[] };
  /** Units decide, fights of fighters alone: the field's height edge a band (measured), and the warning of observers and lookouts. */
  height: { h: number; observers: number; lookouts: number; escort: { close: number[]; top: number[] }; bombers: number };
  production: Record<string, Sched>;
  pilots: { home: number; away: number; perMonth: Record<string, [number, number][]> };
  reserve: Record<string, Reserve>;
  /** Ruling 187(b): a nation's quality by date, [month, quality], read only at the stops of the history dial that keep the book. None in the older files. */
  decline?: Record<string, [number, number][]>;
  /**
   * The far side (ruling 184's "to start", rider b518): a nation whose commands' own quality stands only until its
   * air force has come of age: at a stop that keeps the book until month `from`, at the others until the country
   * holds `tech`. From then its commands read the nation's.
   */
  rise?: Record<string, { from: number; tech: string }>;
  /**
   * The far side (ruling 187(a), rider b518): how far a nation's land fighters reach from a field beyond Europe's
   * chart, by what they are doing. `cover`: the most over a beach, on patrol and over a sea. `raid`: [month, km],
   * flying with the bombers against industry, railways or a town, from that month (before it, as cover).
   */
  jobs?: Record<string, { cover: number; raid: [number, number][] }>;
  /**
   * The far side (the ranges' ruling with the chart's kilometre, 22 November 2026): a nation's fighters by date from
   * a field beyond Europe's chart, [month, km], the record's routine radius on the globe: read from the row's first
   * month in the place of airwar.json's dated row, which is Europe's.
   */
  ftrReach?: Record<string, [number, number][]>;
  /**
   * The far side (rider b518, 7): the smaller powers whose own works (arsenal.json smallAir.works) keep up the
   * commands the record gives them: each month what the works build goes to the country's pool (arsenal.ts).
   */
  works?: string[];
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
  ground: { intensity: number; fighters: number; scale: number; perDiv: number; max: number; str: number; floor: number; dug: number; cover: Record<string, number>; fire: number; value: number; reach?: number };
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
  /** Fog and reconnaissance on air raids (airfog.ts; off as shipped): see the note in the data. */
  fog: { on: boolean; months: number; forget: number; stale: number; scouts: number; round: number; reads: Record<string, number>; gunsUnknown: number; words: string[] };
  success: { k: number; factory: number };
  ai: { escortReady: number; basing: { better: number; least: number; presence?: number; fields?: number; idle?: boolean }; raids: { share: number; least: number; loss: number; rail: number; ready: number; success: number; look: number; worth: number; sweeps: number; sky: number; skyWorth: number; sour?: { trade: number; dear: number; times: number; rest: number }; beach?: { groups: number; home: number; monthly: number; sky: number } }; back?: FightBack };
};

/** True once the air war is its own game: airfields are places and air groups of one kind fly from them. The grand campaign only. */
export const airWar = (g: Game): boolean => g.flags.airwar === 1;

/** Carriers and the air war at sea (the grand campaign): see carriers.json, and carriers.ts for the wings themselves. */
export const CARRIERS = carrierData as unknown as {
  wing: Record<string, { ftr: number; bmr: number }>;
  /** A navy's wing a deck by date where the switch 'Carrier wings by date' is on: rows of [from month, fighters, strike aircraft], and the news line for its player. */
  wingByDate?: Record<string, { words: string; rows: [number, number, number][] }>;
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
  airfield: { label: 'Bomb an airfield', what: 'Bombers go for the runways, hangars and fuel of one enemy airfield by day: enough bombs shut it until its owner repairs it.' },
  seapatrol: { label: 'Patrol a sea against submarines', what: 'Bombers watch one sea for submarines all month, and fly nothing else.' },
};
const OFFENSIVE: AirMission[] = ['superiority', 'bomb', 'night', 'strike', 'ground', 'fleet', 'harbour', 'town', 'line', 'airfield'];
/** How hard a month of one kind of mission is fought, beside a fight for the sky. */
const intensity = (m: AirMission): number => (m === 'ground' ? AIRWAR.ground.intensity : m === 'line' ? AIRWAR.line.intensity : m === 'fleet' ? CARRIERS.strike.intensity : m === 'harbour' ? CARRIERS.harbour.intensity : m === 'town' ? SIEGE.bombard.intensity : m === 'airfield' ? AIR.combat.intensity.bomb : m === 'seapatrol' ? patrolFigures()?.intensity ?? AIR.combat.intensity.strike : AIR.combat.intensity[m]);
export const isOffensive = (m: AirMission): boolean => OFFENSIVE.includes(m);

const SURFACE = ['bb', 'cv', 'ca', 'dd'] as const;
/**
 * The stage switch 'airQuality' (the design director, 22 Nov 2026, 06:26Z): an air force's quality from the record where
 * air.json's default sells it short, one row a nation with its facts and source (data/campaign/air-quality.json).
 * Off as shipped, and then air.json's quality stands exactly as in release 1; a harness turns it on (TC_AIRQ=1) by
 * setting `on`. Not behind the switch 'Fighter types'.
 */
const AIRQ = { on: false, ...(qualityData as unknown as { rows: Record<string, { quality: number; mark: string; facts: string; source: string }> }) };
export const AIR_QUALITY = (): typeof AIRQ => AIRQ;
const spec = (nat: string): NatSpec => {
  const s = AIR.nations[nat] ?? AIR.nations._default;
  const q = AIRQ.on ? AIRQ.rows[nat] : undefined;
  return q ? { ...s, quality: q.quality } : s;
};
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
let ROWS: Map<string, FleetRow> | null = null, ROWS_N = -1;
const ROW_KEPT = new WeakMap<object, { rows: Map<string, FleetRow>; nat: string; name: string; row: FleetRow | undefined }>();
/**
 * A group's row in air.json: its command's, under the country that raised it (a fallen country's group in another's
 * keeping still reads its own row: ruling 199). The first row of that name, as the list is read.
 */
const rowOf = (f: { nat: string; name: string; parent?: string; raised?: string }): FleetRow | undefined => {
  if (!ROWS || ROWS_N !== AIR.fleets.length) {
    ROWS = new Map();
    ROWS_N = AIR.fleets.length;
    for (const r of AIR.fleets) { const k = `${r.nat}|${r.name}`; if (!ROWS.has(k)) ROWS.set(k, r); }
  }
  // (Speed, the far map: the playtest's finding 41.) Each group keeps the row it was last found, with the names it was
  // found by: looked up again only when those names, or the list, have changed. The over-sea air asks it of every
  // group for every sea.
  const nat = f.raised ?? f.nat, name = f.parent ?? f.name;
  const kept = ROW_KEPT.get(f);
  if (kept && kept.rows === ROWS && kept.nat === nat && kept.name === name) return kept.row;
  const row = ROWS.get(`${nat}|${name}`);
  ROW_KEPT.set(f, { rows: ROWS, nat, name, row });
  return row;
};
/** True for a group of a command that guards its own country (air.json 'home'): it does not follow the army across an ocean. */
export const homeCommand = (f: { nat: string; name: string; parent?: string; raised?: string }): boolean => !!rowOf(f)?.home;
/**
 * True for a group of a command the record posts (air.json 'posted', the far side's rows) while its staff keeps to
 * the record: a computer staff, at a stop of the history dial that keeps the book, while the Pacific's book stands.
 * The staff's own basing passes it by (by the threat, for the bombers' main effort, off a crowded field): it stands
 * on the record's field and goes forward by the record's moves, as the fleets keep the record's bases. A beach the
 * staff means to land on still calls a fighter group up, and a group whose field is lost still falls back.
 */
const posted = (g: Game, f: AirFleet): boolean => f.nat !== g.human && !!rowOf(f)?.posted && hopsBind(g);
/**
 * True for a group of a command that stands apart, watching a neighbour (air.json 'watch', the far side's rows),
 * while its country is at peace with that neighbour: at every stop a computer staff's basing by the threat and for
 * the bombers' main effort passes it by, as the armies beside it hold. (Off a crowded field it still spreads.)
 */
const watching = (g: Game, f: AirFleet): boolean => {
  if (f.nat === g.human || f.raised) return false;
  const w = rowOf(f)?.watch;
  return !!w && !atWar(g, f.nat, w);
};
/**
 * What the record keeps a command at this month: its row by date. Where a row's falls are the record's own moves
 * ('byBook') and the stop of the history dial keeps no book, the most the record has given it so far, kind by kind:
 * a withdrawal the record made is the staff's own to make there, or a battle's to bring about.
 */
function kept(g: Game, r: FleetRow, turn = g.turn): [number, number, number] | null {
  let e = row(r.est, turn);
  if (!e) return e;
  if (r.watch) {
    // (Rider b534: a withdrawal dated inside a war with the country the command watches was dropped for good, not
    // paid late: it is counted back in from its month on. What the record adds or takes after it comes and goes as dated.)
    let ftr = 0, bmr = 0;
    for (let k = 1; k < r.est.length && r.est[k][0] <= turn; k++) {
      if (!g.flags[`air:stay:${r.name}:${r.est[k][0]}`]) continue;
      ftr += Math.max(0, r.est[k - 1][1] - r.est[k][1]);
      bmr += Math.max(0, r.est[k - 1][2] - r.est[k][2]);
    }
    if (ftr || bmr) e = [e[0], e[1] + ftr, e[2] + bmr];
  }
  if (r.to) {
    // (A command the record renamed: gone once it has handed over, and until then not stood down by the date.)
    if (g.flags[`air:over:${r.name}`]) return [e[0], 0, 0];
    if (e[1] + e[2] <= 0) for (const x of r.est) { if (x[0] > turn) break; if (x[1] + x[2] > 0) e = [e[0], x[1], x[2]]; }
  }
  if (!r.byBook || stopDef(g).book) return e;
  // (What left for a reason outside the game is no part of "the most": it is counted back in step by step, and what did leave taken off.)
  const left = (at: number, did: boolean): [number, number] => {
    let ftr = 0, bmr = 0;
    for (const L of r.leaves ?? []) if (L[0] <= at && !(did && g.flags[`air:stay:${r.name}:${L[0]}`])) { ftr += L[1]; bmr += L[2]; }
    return [ftr, bmr];
  };
  let ftr = 0, bmr = 0;
  for (const x of r.est) { if (x[0] > turn) break; const back = left(x[0], false); ftr = Math.max(ftr, x[1] + back[0]); bmr = Math.max(bmr, x[2] + back[1]); }
  const gone = left(turn, true);
  return [e[0], Math.max(0, ftr - gone[0]), Math.max(0, bmr - gone[1])];
}
/** The month the record ends a command it renamed: its last row, where that reads nothing. */
const endOf = (r: FleetRow): number | undefined => { const x = r.est[r.est.length - 1]; return x[1] + x[2] <= 0 ? x[0] : undefined; };
/** The commands the record renamed into this one that are due to hand over and have not: their month has come. */
const dueInto = (g: Game, s: FleetRow): FleetRow[] =>
  AIR.fleets.filter((r) => r.to?.includes(s.name) && !g.flags[`air:over:${r.name}`] && (endOf(r) ?? Infinity) <= g.turn);
/**
 * How well a nation's air force fights, plane for plane: its row in air.json, or by date where the record has it
 * fall (ruling 187(b), air.json 'decline': Japan's, at the stops of the history dial that keep the book and no others).
 */
const natQuality = (nat: string, g?: Game): number => {
  const rows = g ? AIR.decline?.[nat] : undefined;
  return rows && stopDef(g!).book ? (row(rows, g!.turn) ?? rows[0])[1] : spec(nat).quality;
};
/** A nation's value on one of ruling 227's dated rows this month (airwar.json 'day'): its own rows, else _default's. */
const dayRow = (table: Record<string, [number, number][]>, nat: string, turn: number): number => (row(table[nat] ?? table._default, turn) ?? row(table._default, turn) ?? table._default[0])[1];

/**
 * How far one fleet reaches: its nation's ranges, unless the fleet has long-range fighters of its own ('ftrRange' on its
 * row in air.json). In the air war a nation's fighters reach as far as the month allows (ruling 227: drop tanks, the
 * Zero, the Mustang), and a fleet's own longer reach stands.
 */
const reach = (f: { nat: string; name: string; parent?: string; ship?: number; ashore?: number; raised?: string; base?: number }, g?: Game, job?: 'raid'): NatSpec => {
  // (A group in another country's keeping flies its own aircraft with its own crews.)
  const nat = f.raised ?? f.nat;
  // Carrier aircraft are built for the deck, not for range: ashore as aboard.
  if (deckBred(f)) return { quality: natQuality(nat, g) * (CARRIERS.quality[nat] ?? CARRIERS.quality._default), ftrRange: CARRIERS.range.ftr, bmrRange: CARRIERS.range.bmr };
  const own = rowOf(f);
  // The far side: from a field beyond Europe's chart the month's reach is the far side's own row where it has one
  // (the record's routine radius on the globe), and Europe's dated row before that row's first month.
  const out = !!g && f.base !== undefined && beyondEurope(f.base) && farOn(g);
  const laid = out ? row(AIR.ftrReach?.[nat], g!.turn) : null;
  const dated = g && airWar(g) ? (laid ? laid[1] : dayRow(AIRWAR.day.range, nat, g.turn)) : 0;
  let ftrRange = Math.max(own?.ftrRange ?? 0, dated, spec(nat).ftrRange);
  // The far side (ruling 187(a)): from a field beyond Europe's chart its fighters reach by the job. The long reach is
  // the escorts' on a raid and nobody else's (from Saipan it would cover Iwo Jima's beach).
  const J = out ? AIR.jobs?.[nat] : undefined;
  if (J) { const far = job === 'raid' ? row(J.raid, g!.turn) : null; ftrRange = far ? far[1] : Math.min(ftrRange, J.cover); }
  // A nation with a weight row (ruling 227, b424) fights as that share of the air force it is measured against.
  const w = g ? airWeight(g, nat) : null;
  return { ...spec(nat), quality: w !== null ? w * spec(AIRWAR.weight.against).quality : rowQuality(own, g) ?? natQuality(nat, g), ftrRange, bmrRange: own?.bmrRange ?? spec(nat).bmrRange };
};
/** True for a raid the escorts fly their longest for (ruling 187(a)): by day against industry and railways, or a town. */
const raidJob = (m: string): 'raid' | undefined => (m === 'bomb' || m === 'town' ? 'raid' : undefined);
/**
 * A command's own quality, where its row has one and still stands by it. A nation with a 'rise' row (the far side:
 * America's Pacific commands start below the nation's figure) reads the nation's once its air force has come of age:
 * by the month at a stop that keeps the book, by the research at the others.
 */
function rowQuality(own: FleetRow | undefined, g?: Game): number | undefined {
  if (own?.quality === undefined) return undefined;
  const up = g ? AIR.rise?.[own.nat] : undefined;
  if (!up) return own.quality;
  return (stopDef(g!).book ? g!.turn >= up.from : hasTech(g!, own.nat, up.tech)) ? undefined : own.quality;
}

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
/** What one of a group's aircraft is worth beside another's, plane for plane, before its commander, its fitness and its fuel: its command's own figure, else its nation's this month. */
export const airQuality = (f: { nat: string; name: string; parent?: string; ship?: number; ashore?: number; raised?: string }, g?: Game): number => reach(f, g).quality;
/** How far a group reaches. `mission`: the raid it would fly, where the question is an escort's (a fighter's reach is by the job on the far side: ruling 187(a)). */
export const airRange = (f: { nat: string; name: string; parent?: string; ship?: number; raised?: string; base?: number }, g?: Game, mission?: string): { ftr: number; bmr: number } => {
  const R = reach(f, g, mission ? raidJob(mission) : undefined);
  return { ftr: R.ftrRange, bmr: R.bmrRange };
};

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

// ---- Pilots under Units decide (the quality store, #2 form Part 2; the design director, 24 Nov 2026 15:53Z) -----------
// The nation holds what its schools know (air.school, the deck school apart, and the reserve's mean hours and
// experience); a group holds what its own pilots have lived through (hours and fxp on the group). A new pilot leaves
// school with its hours and no experience; one who comes down and flies again carries his group's; a refit blends
// them by count. Off, none of it is read or written.

/** A nation's air arm in the quality store, under the switch only. */
const airArm = (g: Game, nat: string) => armsOf(g, nat)?.air;
/** The hours a new pilot of a group's kind leaves school with: the deck school for a carrier wing, else the nation's school; null where not laid. */
const schoolOf = (g: Game, f: { nat: string; ship?: number; ashore?: number }): number | null => {
  const A = airArm(g, f.nat);
  return !A ? null : deckBred(f) ? A.deck : A.school;
};
/** A group's experience at the start where no war before it is read: from its leader's level today, so the start is today's. */
const seedFxp = (f: AirFleet): number => (f.leader ? clamp((f.leader.level - 1) / (AIR.leader.maxLevel - 1), 0, 1) : 0);
/** A group's pilots' mean hours under the switch: null where its school is not laid (then nothing prices them). */
export const hoursOf = (g: Game, f: AirFleet): number | null => (unitsDecide(g) ? f.hours ?? schoolOf(g, f) : null);
/** A group's experience under the switch, 0 to 1 (its one counter); 0 off. */
export const fxpOf = (g: Game, f: AirFleet): number => (unitsDecide(g) ? f.fxp ?? seedFxp(f) : 0);
/** A level from experience, as a leader's is counted (1 to AIR.leader.maxLevel): what the card and the field read under the switch. */
export const levelOf = (fxp: number): number => 1 + (AIR.leader.maxLevel - 1) * fxp;

/** Blends two counts' means; null where either side has none and the other is not alone. */
const blend = (a: number | null, na: number, b: number | null, nb: number): number | null =>
  na <= 0 ? b : nb <= 0 ? a : a === null || b === null ? null : (a * na + b * nb) / (na + nb);

/**
 * Puts `n` fighter pilots into a nation's reserve: under the switch the reserve's mean hours and experience take theirs
 * in by count (#2 form Part 2 section 6). `from` is the group they flew with (a pilot back carries its figures);
 * without one they are new, at the school's hours and no experience. Off, only the count moves, as before.
 */
export function poolIn(g: Game, nat: string, n: number, from?: AirFleet): void {
  const p = airPool(g, nat);
  const A = airArm(g, nat);
  // The land reserve's hours are the land school's: a carrier pilot ashore brings his experience but not deck hours to it.
  if (A) intoPool(A, p.pilots, n, from && !deckBred(from) ? hoursOf(g, from) : A.school, from ? fxpOf(g, from) : 0);
  p.pilots += n;
}

/**
 * Under the switch, `n` aircraft join group `f` with their pilots: its hours and experience become the means by count.
 * `how`: 'pool', pilots drawn from the reserve (a carrier wing's at the deck school's hours, its experience the
 * reserve's); 'school', new pilots straight from school (bought wings at the buyer's school, BO:2067; a new carrier
 * wing's at the deck school); a bomber group's crews come new, with no experience. Call before the count changes.
 */
export function joinPilots(g: Game, f: AirFleet, n: number, kind: 'ftr' | 'bmr', how: 'pool' | 'school'): void {
  const A = airArm(g, f.nat);
  if (!A || n <= 0) return;
  const had = f.ftr + f.bmr;
  const inHours = kind === 'bmr' ? hoursOf(g, f) : how === 'pool' && !deckBred(f) ? A.pool : schoolOf(g, f);
  const inXp = kind === 'ftr' && how === 'pool' ? A.poolXp : 0;
  const h = kind === 'ftr' ? blend(hoursOf(g, f), f.ftr, inHours, n) : hoursOf(g, f);
  if (h === null) delete f.hours; else f.hours = h;
  const was = fxpOf(g, f);
  f.fxp = blend(was, had, inXp, n) ?? 0;
  // The card's refit note (the UI director 24 Nov 18:10Z): who joined this month, and the experience before the first of them.
  f.joined = f.joined?.on === g.turn ? { ...f.joined, n: f.joined.n + n } : { on: g.turn, n, was };
}

const FIGHTER_WAY: Record<'turn' | 'dive-zoom', string> = { turn: 'turning fight', 'dive-zoom': 'dive and climb' };

/**
 * Under Units decide, a group's pilots in words for its card (the UI director 24 Nov 18:10Z): the grade first, the
 * part that changes with play, then the hours where its school is read; and, only in the month new pilots took the
 * grade down a step, how many joined. Off, both are ''. No raw figure and no "not read": an unread clause is left out.
 */
export function pilotWords(g: Game, f: AirFleet): { pilots: string; joined: string } {
  if (!unitsDecide(g)) return { pilots: '', joined: '' };
  const xp = fxpOf(g, f), h = hoursOf(g, f), j = f.joined;
  const pilots = `${grade(xp)} pilots${h !== null ? `, ${Math.round(h)} hours' training` : ''}.`;
  const joined = j && j.on === g.turn && j.n >= 1 && xp < j.was && grade(xp) !== grade(j.was) ? `${Math.round(j.n)} new pilot${Math.round(j.n) === 1 ? '' : 's'} joined this month.` : '';
  return { pilots, joined };
}

/**
 * Under Units decide, the nation's schools and fighter doctrine, said first on each of its group pages (the UI
 * director 24 Nov 18:10Z: no nation air force page): the carrier school on a carrier wing's, the flying schools on
 * the rest. An unread school or doctrine leaves its sentence out; a change training in says when it is ready.
 */
export function schoolWords(g: Game, f: AirFleet): string {
  const A = unitsDecide(g) ? armsOf(g, f.nat)?.air : undefined;
  if (!A) return '';
  const deck = deckBred(f), h = deck ? A.deck : A.school;
  const school = h === null ? '' : deck ? `Your carrier school gives ${Math.round(h)} hours before combat.` : `Your flying schools give ${Math.round(h)} hours before combat.`;
  const w = A.fighter;
  const training = w && w.next !== undefined && w.ready !== undefined && g.turn < w.ready;
  const doctrine = !w ? '' : training ? `Fighter doctrine: ${FIGHTER_WAY[w.id]}; ${FIGHTER_WAY[w.next!]} ready in ${turnToDate(w.ready!).label}.` : `Fighter doctrine: ${FIGHTER_WAY[wayNow(g, w)]}.`;
  return [school, doctrine].filter(Boolean).join(' ');
}

/** Seeds every group's pilots as a grand campaign opens with Units decide (staff.ts startOpen, after the store). */
export function openPilots(g: Game): void {
  if (!unitsDecide(g)) return;
  for (const f of g.airFleets) {
    const h = schoolOf(g, f);
    if (h !== null) f.hours = h;
    f.fxp = seedFxp(f);
  }
}

/** True in the month a war opens between two nations, whichever declared it. */
export const warOpens = (g: Game, a: string, b: string): boolean => g.flags[`war:${a}:${b}`] === g.turn || g.flags[`war:${b}:${a}`] === g.turn;

/** Kilometres between the centres of two regions. */
export function km(g: Game, a: number, b: number): number {
  const A = g.regions[a], B = g.regions[b];
  return chartDist(g, A, B) * AIR.kmPerUnit;
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

/** A group on its way to another field by stages (ferry.ts): the field it took off from on this leg, and the one it is going to in the end. */
export interface Transit { from: number; to: number }

/**
 * How far a group's fighters have to fly to meet aircraft on the leg between two fields (ferry.ts): to the nearest
 * point of that straight leg on the chart, either field included, the way the reach of every fighter is measured (km):
 * on the globe where an end of the leg or the group's own place is of the far side (wrap.ts globeLeg, as chartDist
 * reads it; the join's read of r2-head-1125k, fault 1), else the map's straight line as it always was.
 */
export function legKm(g: Game, f: AirFleet, from: number, to: number): number {
  return legKmAt(g, groupAt(g, f), from, to);
}

/**
 * legKm from a place on the chart: a province, or a sea where region is -1 (ferry.ts reads its known fighters so).
 * One field, one distance (the design director, 14:59Z; the reviews of the air follow-on, finding 1 and notes 2 and 3):
 * - The rules' own distance from the place to a field of the leg is the one kmTo reads: km() from a province, whichever
 *   chart either is on; seaKm from a sea, a carrier wing at sea being anywhere in its sea (a field on its shore at no
 *   distance, not the sea's centre: North Atlantic against the Highlands read 444.0 km, kmTo 0).
 * - From a province, where the nearest point of the leg is one of its ends, the distance is km() to that end (Moscow
 *   against the leg Kirov to Yaroslavl read 248.763 km on the globe where km() reads 259.688 on the map), or to the
 *   leg's other field where km() reads that one nearer: on the far chart the end nearest on the chart need not be the
 *   nearer by km() (Singapore against Kuantan to Kuala Lumpur read 309.355 km, km() to Kuala Lumpur, where km() to
 *   Kuantan is 289.260; the check of the third round). On the old chart the end nearest on the chart is always the
 *   nearer by km(), so nothing moves there.
 * - A point inside the leg is measured by the leg's own ruler (the globe's where an end of the leg or the place is of
 *   the far side, else the map's straight line; from a sea, from the sea's centre), capped at the rules' distance to
 *   each end: a leg never reads farther than either of its own fields (Manila against Formosa to Hong Kong read 904.052
 *   km, km() to Formosa 902.923: the nearest point is found on the chart, not on the globe). On the old chart the cap
 *   never binds for a province (the nearest point on the map's straight line is nearer than either end).
 * - From a sea, where the sea's centre lies beyond an end the leg reads seaKm to the nearer of its two fields: the end
 *   the sea's centre is nearest to on the chart need not be the nearer by seaKm, and a leg never reads farther than
 *   either of its fields.
 * Just inside an old-chart end of a leg that crosses to the far side the reading can still step down from the map's to
 * the globe's: the far chart's two rulers (wrap.ts), not mended here.
 */
export function legKmAt(g: Game, p: { x: number; y: number; region: number; sea: number }, from: number, to: number): number {
  const A = g.regions[from], B = g.regions[to];
  const bx = nearX(g, B.x, A.x), px = nearX(g, p.x, A.x);
  const dx = bx - A.x, dy = B.y - A.y, L = dx * dx + dy * dy;
  const t = L > 0 ? clamp(((px - A.x) * dx + (p.y - A.y) * dy) / L, 0, 1) : 0;
  const atEnd = t <= 0 || t >= 1;
  // The rules' own distance to each field of the leg (kmTo's; seaKm takes the field, then the sea), the nearer of the two.
  const ends = p.region >= 0 ? Math.min(km(g, p.region, from), km(g, p.region, to)) : p.sea >= 0 ? Math.min(seaKm(g, from, p.sea), seaKm(g, to, p.sea)) : Infinity;
  if (atEnd && (p.region >= 0 || p.sea >= 0)) return ends;
  const qx = A.x + t * dx, qy = A.y + t * dy;
  const d = globeLeg(g, A, B, p.region, p.sea) ? globeDist(g, px, p.y, qx, qy) * AIR.kmPerUnit : Math.hypot(px - qx, p.y - qy) * AIR.kmPerUnit;
  // From a sea, a point inside the leg keeps the sea centre's ruler, capped at seaKm to each end: the smallest change
  // that keeps one field one distance. (The nearest shore of the sea to the leg, as kmTo reads a sea for a single
  // field, would be truer: put to the design director, not built.)
  return Math.min(d, ends);
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
export const fieldClass = (g: Game, region: number): number => Math.max(g.flags[flagName('af:', region)] ?? 0, recordField(g, region), baseField(g, region));

/**
 * How many aircraft the airfields of a region can serve. In a historical scenario every region has airfields.
 * In the grand campaign's air war an airfield is a place: where there is none, and none has been built, the answer is 0.
 */
export function airfieldsHold(g: Game, region: number): number {
  const built = (g.flags[flagName('wk:field:', region)] ?? 0) * WORKS.field.holds;
  if (!airWar(g)) return AIR.airfields.holds + built;
  const holds = (AIRWAR.fields.sizes[fieldClass(g, region)] ?? 0) + built;
  // Airfields as targets (fieldstate.ts): each burned level serves 500 aircraft fewer until it is rebuilt.
  return g.flags.fields === 1 && fieldRules(g) ? holds - burnedRoom(g, region) : holds;
}

/** True if aircraft can be based in this region at all. */
export const hasAirfield = (g: Game, region: number): boolean => !airWar(g) || airfieldsHold(g, region) > 0;

/** The level of anti-aircraft guns round a region's industry: 0 unless some have been built there. */
export const gunsAt = (g: Game, region: number): number => g.flags[`wk:aa:${region}`] ?? 0;

/**
 * The guns BUILT round a target as a count (Brian, 20 November 2026: "calculate how many flak guns they have"): the
 * heavy and light guns of each level of anti-aircraft guns built there (two light guns to every heavy one, as the
 * record had it: the historian); none where none are built. The light guns every target has stand apart from them
 * (standingGuns): the count reads what the loss reads, the built guns and the standing ones, never the larger of the
 * two (the design director, 25 Nov 2026 18:59Z). A count for the raid card and the battle's picture; the losses come
 * from the shares the forecast has always used.
 */
export function flakGuns(level: number): { light: number; heavy: number; all: number } {
  const G = AIRWAR.count.guns, n = Math.max(0, level), heavy = n * G.heavy, light = n * G.light;
  return { light, heavy, all: light + heavy };
}

/**
 * The light guns every target on land that bombers go for has, built guns or none: the count of the plain flak share
 * the loss reads there by day and by night (assessAir: AIR.combat.flak, AIR.combat.night.flak). Named apart from the
 * built guns (flakGuns), never folded into them.
 */
export const standingGuns = (): number => AIRWAR.count.guns.base;

// (The UI director, 25 Nov 2026 20:08Z: on the gun forms the months are words to ten, "seen two months ago", "seen 11
// months ago"; a month and this month as the fog's lines said them. 21:22Z, case b: every month the fog prints is this
// helper's, the knowledge lines (airfog.ts knownLines) and the raid row's fighters (airfog.ts fogBits) too.)
const TEN = ['', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];
/** "seen two months ago", "seen 11 months ago", "seen a month ago", "seen this month": the guns' sighting, in words. */
export const seenAgo = (n: number): string => (n <= 0 ? 'seen this month' : n === 1 ? 'seen a month ago' : `seen ${TEN[n] ?? n.toLocaleString('en-US')} months ago`);
/** Her item 2 and item 8: a place last seen with no guns built, "No guns built when seen two months ago: only the 60 light guns every target has." */
export const noneSeen = (ago: number): string => `No guns built when ${seenAgo(ago)}: only the ${standingGuns().toLocaleString('en-US')} light guns every target has.`;

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

/**
 * Experience: the leader's level, or under Units decide the group's own (fxp, read as a level by the same step), so it
 * is priced once (#2 form Part 2 section 1: leaderMul reads fxp).
 */
const leaderMul = (g: Game, f: AirFleet): number => 1 + (unitsDecide(g) ? AIR.leader.perLevel * (levelOf(fxpOf(g, f)) - 1) : f.leader ? AIR.leader.perLevel * (f.leader.level - 1) : 0);
/**
 * What one of a fleet's aircraft is worth in a fight. Under Units decide the player's own stories add nothing on top
 * of the group's experience (storyEdge is a player-only edge: #2 form Part 2 section 6), so every side is priced alike.
 */
const worth = (g: Game, f: AirFleet): number => reach(f, g).quality * leaderMul(g, f) * (0.5 + 0.5 * f.ready) * fighterTech(g, f) * (unitsDecide(g) ? 1 : storyEdge(g, f)) * fuelAir(g, f.nat);
/** The held level a group fights at where hours are not priced (its nation's quality, a carrier wing's at the deck's share of it). */
export const heldOf = (g: Game, f: AirFleet): number => reach(f, g).quality;

/**
 * A group's fighter technology (research.ts techMul). Airfields as targets: a group of a nation that has jets, based
 * on a field strip, flies without the jets' edge, one level of 'fighters' fewer (the Me 262 needed concrete runways).
 */
export function fighterTech(g: Game, f: AirFleet): number {
  const t = techMul(g, f.nat, 'fighters');
  if (g.flags.fields !== 1 || f.ship !== undefined || !fieldRules(g) || !hasTech(g, f.nat, 'jets') || fieldTier(g, f.base) !== 'strip') return t;
  return Math.max(1, t - RESEARCH.fields.fighters.per);
}

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
  // (Release 2, a sea patrol against submarines: met as a strike is, but only once the air fighting's own row for it
  // is laid. The design director's ruling of 24 November 2026 17:49Z, 5 (a): that row gates only the fighters.)
  if (m === 'seapatrol') return patrolMet() && guardsSea(g, f, zone);
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
    if ((wx && grounded(g, f)) || wrecked(g, f)) continue;
    n += f.ftr * cover(kmTo(g, f, region), reach(f, g).ftrRange) * crowding(g, f);
  }
  return n;
}

/**
 * Fog on raids (airfog.ts): how far a nation's fighters reach, by its own table (a group of long-range fighters may reach
 * farther), as reach() reads it for a group based at `base` where one is given: on the far side the far rows
 * (far-air.json ftrReach) and the cover cap (jobs.cover) of a group based there (the join's read of r2-head-1125k,
 * fault 2). With no base, or off the far side, Europe's dated row as before.
 */
export const fighterReach = (g: Game, nat: string, base?: number): number => reach(base === undefined ? { nat, name: '' } : { nat, name: '', base }, g).ftrRange;

/**
 * Fog on raids (airfog.ts, read only with its switch on): every enemy fighter group that guards a province's ground and
 * has it within its reach, whatever it is doing this month and whatever the sky: what a side sees there when it looks,
 * and what the high end of a forecast made out of sight assumes may rise. The fighters whole, not weighed for distance.
 */
export function fightersInReach(g: Game, region: number, against: string): AirFleet[] {
  const holder = g.regions[region].ctrl;
  return g.airFleets.filter((f) => f.ftr > 0 && atWar(g, f.nat, against) && guardsLand(g, f, region, holder) && cover(kmTo(g, f, region), reach(f, g).ftrRange) > 0);
}

/**
 * The fighters each enemy airfield puts over a region against this nation, by the province the field stands in:
 * fightersOver told apart by where its land-based groups come from (a carrier's wing in harbour is no field's).
 */
export function fightersOverFrom(g: Game, region: number, against: string): Map<number, number> {
  const out = new Map<number, number>();
  const holder = g.regions[region].ctrl;
  const wx = weatherOn(g);
  if (wx && skyAt(g, region) === 'closed') return out;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || f.close || f.ship !== undefined || !atWar(g, f.nat, against) || !guardsLand(g, f, region, holder)) continue;
    if ((wx && grounded(g, f)) || wrecked(g, f)) continue;
    const n = f.ftr * cover(kmTo(g, f, region), reach(f, g).ftrRange) * crowding(g, f);
    if (n > 0) out.set(f.base, (out.get(f.base) ?? 0) + n);
  }
  return out;
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
    if ((wx && grounded(g, f)) || wrecked(g, f)) continue;
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

/** The computer's air forces fighting back (Brian, 24 November 2026: airwar.json ai.back). */
export interface FightBack {
  on: boolean; close: number; sweeps: number; sky: number; more: number; risk: number; success: number; worth: number; raided: number; least: number; invaded: number; home: number; full: number;
  minor: { least: number; per: number; ftr: number; split: number; refit: number };
}
/**
 * True where a nation's air staff fights back (Brian, 24 November 2026: "im attacking other nations that have airforces
 * but they arent using them"): the grand campaign with airwar.json ai.back on. One rule for every staff (the design
 * director, 18:38Z): the player's own staff, while he leaves it the air war, does as every computer staff does, and he
 * has each of its tools by hand (ground support, sweeps, his own basing).
 */
export const fightBack = (g: Game, nat: string): boolean => backOn(g) && !!g.nations[nat];
/** The fight back's switch: the grand campaign's air war, with airwar.json ai.back on. */
export const backOn = (g: Game): boolean => AIRWAR.ai.back?.on === true && g.flags.open === 1 && airWar(g);

/**
 * A country at war with an air force but no air group raises one (the fight back, airwar.json ai.back.minor): the
 * great powers' air forces come from the record (air.json fleets), every other country's air force was only its air
 * rating (nations.json 'air'), which tilts its land battles. Once, in the first month of its own at war with its
 * capital held, it raises 'per' aircraft for each point of its rating at its capital: 'ftr' of them in a fighter
 * group and the rest in a bomber group (all fighters below 'split' aircraft), named by its own ladder where the
 * historian gave one. Its capital gets an air base and each of its provinces with a town of any note a landing
 * ground, where it has none. A country that loses them raises no more.
 */
export function raiseAirForce(g: Game, nat: string): AirFleet[] {
  const M = AIRWAR.ai.back?.minor;
  const n = g.nations[nat];
  if (!M || !backOn(g) || !n?.alive || !n.atWar.length || g.flags[`mair:${nat}`] || !((n.air ?? 0) >= M.least) || AIR.production[nat]) return [];
  if (g.airFleets.some((f) => f.nat === nat)) return [];
  const base = g.regions[n.capital];
  if (!base || base.ctrl !== nat) return [];
  g.flags[`mair:${nat}`] = g.turn;
  const all = Math.round(n.air * M.per);
  const ftr = all < M.split ? all : Math.round(all * M.ftr), bmr = all - ftr;
  // (Its capital's air base, and a landing ground in each of its provinces that has a town of any note.)
  seedField(g, base.id, AIRWAR.fields.seed.base);
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && (r.fac > 0 || r.vp > 0)) seedField(g, r.id, 1);
  const out: AirFleet[] = [];
  for (const [kind, k] of [['ftr', ftr], ['bmr', bmr]] as const) {
    if (k < 1) continue;
    const f = foundAirFleet(g, nat, base, `${n.adj} Air Force`, kind, k);
    f.ftr = kind === 'ftr' ? k : 0; f.bmr = kind === 'bmr' ? k : 0;
    f.est = { ftr: f.ftr, bmr: f.bmr };
    out.push(f);
  }
  if (out.length) headline(g, 'info', `The ${n.adj} air force goes to war: ${out.map((f) => f.name).join(" and ")} at ${base.name}.`, nat, base.id);
  return out;
}

/**
 * A country's air rating as its land battles count it (game.ts airOf): nations.json 'air', or nothing once the fight
 * back has raised its air force as air groups. Its air is then its groups, never both (the design director, 18:38Z).
 */
export const ratedAir = (g: Game, nat: string): number => (backOn(g) && g.flags[`mair:${nat}`] ? 0 : g.nations[nat]?.air ?? 0);

/** A country whose air force the fight back raised: what it builds and trains a month, 'refit' of its rating's aircraft (at least one of each kind it flies). */
function minorBuilt(g: Game, nat: string): [number, number, number] | null {
  const M = AIRWAR.ai.back?.minor;
  if (!M || !backOn(g) || !g.flags[`mair:${nat}`]) return null;
  const all = Math.round((g.nations[nat]?.air ?? 0) * M.per);
  const ftr = all < M.split ? all : Math.round(all * M.ftr), bmr = all - ftr;
  return [g.turn, ftr ? Math.max(1, Math.round(ftr * M.refit)) : 0, bmr ? Math.max(1, Math.round(bmr * M.refit)) : 0];
}

/**
 * Where a nation is being attacked on the ground (the fight back): the provinces where its side defended a land battle
 * since last month, and the provinces beside them where its own troops face enemy troops. Its ground support goes up
 * over these, as it does over its own offensive's battles.
 */
export function defendBattles(g: Game, nat: string): number[] {
  const struck = new Set<number>();
  for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn >= g.turn - 1; i--) {
    const b = g.reports[i];
    if (b.yielded || !g.regions[b.region] || !(b.defNat === nat || allied(g, b.defNat, nat)) || !atWar(g, nat, b.attNat)) continue;
    struck.add(b.region);
  }
  if (!struck.size) return [];
  const facing = (id: number): boolean => g.regions[id].nb.some((j) => g.regions[j].units.some((u) => !u.dead && atWar(g, nat, u.nat)));
  const mine = (id: number): boolean => g.regions[id].units.some((u) => !u.dead && u.nat === nat && u.kind !== 'fort');
  const out = new Set<number>();
  for (const r of struck) {
    if (mine(r) || g.regions[r].ctrl === nat || allied(g, nat, g.regions[r].ctrl)) out.add(r);
    for (const j of g.regions[r].nb) if (mine(j) && facing(j)) out.add(j);
  }
  return [...out].sort((a, b) => a - b);
}

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
    if ((wx && grounded(g, f)) || wrecked(g, f)) continue;
    if (!g.nations[f.nat]?.alive) continue;
    const ours = nats.has(f.nat) || ([...nats].some((x) => allied(g, x, f.nat)) && [...enemies].some((e) => atWar(g, f.nat, e)));
    if (!ours || ![...enemies].some((e) => atWar(g, f.nat, e))) continue;
    const reachOf = cover(kmTo(g, f, region), reach(f, g).ftrRange) * crowding(g, f);
    if (reachOf <= 0) continue;
    const planes = f.ftr * reachOf;
    out.W += planes * (0.5 + 0.5 * f.ready) * fighterTech(g, f) * fuelAir(g, f.nat);
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
export function bookCloseAir(g: Game, air: CloseAir | undefined, sorties?: Record<number, number>, region = -1): void {
  if (!air) return;
  const stamp = g.turn * 10 + g.week;
  const flown: AirFleet[] = [];
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
        p.lostFtr += lost; poolIn(g, f.nat, back, f); p.lostPilots += lost - back;
      }
      f.ready = Math.max(AIR.ready.floor, f.ready - CLOSE.ready);
      f.closeAt = stamp;
      flown.push(f);
    }
  }
  // (Release 2, earned names for air groups under the switch 'nicknames': support flown over a battle is a mission completed.)
  if (nicknamesOn(g)) airSupportFlown(g, flown, region);
}

/** True when enemy fighters still defend the sky over a coast and the invader has not won it: no invasion fleet can cross under them. */
export function skyDenies(g: Game, nat: string, region: number): boolean {
  const sky = skyOver(g, region);
  if (sky !== null && allied(g, sky, nat)) return false;
  return fightersOver(g, region, nat) >= AIR.sky.least;
}

// ---- The air over a Pacific sea (rulings 184 and 187; the planner's paper 10, section 4) ------------------------------

/**
 * (Speed, the far map: the playtest's finding 41.) The longest reach any group can have by the tables, its fighters'
 * or its bombers' (all that `reach` reads them from: air.json's nations, rows, far-side reach and jobs, airwar.json's
 * dated ranges, carriers.json's). A field farther than this from a sea puts nothing over it. Over the far side's seas
 * all but a few in a hundred of the checks of each group of the war were of fields beyond it, and each read the
 * group's whole reach first. The tables are read once; nothing changes them in play.
 */
let LONGEST: number | null = null;
function longestReach(): number {
  if (LONGEST !== null) return LONGEST;
  let m = Math.max(CARRIERS.range.ftr, CARRIERS.range.bmr);
  const up = (x: number | undefined): void => { if (x !== undefined) m = Math.max(m, x); };
  for (const s of Object.values(AIR.nations)) { up(s.ftrRange); up(s.bmrRange); }
  for (const r of AIR.fleets) { up(r.ftrRange); up(r.bmrRange); }
  for (const rows of Object.values(AIR.ftrReach ?? {})) for (const r of rows) up(r[1]);
  for (const rows of Object.values(AIRWAR.day.range)) for (const r of rows) up(r[1]);
  for (const J of Object.values(AIR.jobs ?? {})) { up(J.cover); for (const r of J.raid) up(r[1]); }
  return (LONGEST = m);
}

const seaDist = new WeakMap<object, (Float64Array | undefined)[]>();
/** seaKm, kept: how far a field lies from a sea never changes on one chart. (A row of seas for each field.) */
function fieldToSea(g: Game, base: number, z: number): number {
  let m = seaDist.get(g.seas);
  if (!m) { m = []; seaDist.set(g.seas, m); }
  const row = (m[base] ??= new Float64Array(g.seas.length).fill(NaN));
  let d = row[z];
  if (Number.isNaN(d)) { d = seaKm(g, base, z); row[z] = d; }
  return d;
}

const nearSeas = new WeakMap<object, (Int8Array | undefined)[]>();
/**
 * (Speed, the far map: the playtest's finding 41.) For one sea, which fields lie within the longest reach in the
 * tables (longestReach): a row over the provinces, -1 until a field is first measured, then 1 within it and 0 beyond.
 * A group on a field beyond it puts nothing over the sea (groupOverSea), so the sums over every group (groupsOver,
 * navy.ts besideIn and SeaPicture) pass it by at once instead of asking its side and its field's distance for each sea
 * the month looks at. Kept like fieldToSea: a field's distance from a sea never changes on one chart.
 */
export function nearRow(g: Game, z: number): Int8Array {
  let m = nearSeas.get(g.seas);
  if (!m) { m = []; nearSeas.set(g.seas, m); }
  return (m[z] ??= new Int8Array(g.regions.length).fill(-1));
}
/** True when a group's field may put aircraft over the sea of this row (nearRow): false for a field beyond the longest reach, or no field. */
export function fieldNear(g: Game, row: Int8Array, base: number, z: number): boolean {
  const k = row[base];
  // (Past the row's end: no such province, where groupOverSea puts nothing over the sea; or, should the chart ever
  // gain provinces after the row was made, one measured afresh each time.)
  if (k === undefined) return !!g.regions[base] && !(fieldToSea(g, base, z) > longestReach());
  if (k >= 0) return k === 1;
  const near = !(fieldToSea(g, base, z) > longestReach());
  row[base] = near ? 1 : 0;
  return near;
}

/**
 * The aircraft one land air group puts over a sea whose air is counted from the groups in reach (pacific.json
 * 'skies'): its fighters by how far its field lies from the sea's nearest shore (nothing to fly if the field stands on
 * that shore), its bombers in full inside their reach, as much of the group as its field can serve. Nothing from a
 * resting group, from one whose field has gone unfed, from a carrier wing afloat (a wing ashore counts as the land
 * group it is), or from a country at peace; three tenths from a group under a sky its enemy has won, as a coast
 * counts today. 'S.cover' is the most a nation's fighters reach for this job (ruling 187(a)).
 */
export function groupOverSea(g: Game, f: AirFleet, z: number, S: { cover?: Record<string, number> }): number {
  if (f.ship !== undefined || f.mission === 'rest' || f.unfed || f.ftr + f.bmr <= 0 || !g.regions[f.base]) return 0;
  const n = g.nations[f.nat];
  if (!n?.alive || !n.atWar.length) return 0;
  const d = fieldToSea(g, f.base, z);
  // (A field beyond the longest reach in the tables puts nothing over the sea, whatever its group's own reach: as below.)
  if (d > longestReach()) return 0;
  const R = reach(f, g);
  const most = S.cover?.[f.raised ?? f.nat];
  const ftr = f.ftr > 0 ? f.ftr * cover(d, most === undefined ? R.ftrRange : Math.min(R.ftrRange, most)) : 0;
  const bmr = f.bmr > 0 && d <= R.bmrRange ? f.bmr : 0;
  if (ftr + bmr <= 0) return 0;
  const sky = skyOver(g, f.base);
  return (ftr + bmr) * crowding(g, f) * (sky !== null && atWar(g, sky, f.nat) ? AIR.sky.seaFoe : 1);
}

/**
 * Air strength over such a sea for one side, this nation's friends or (hostile) its enemies: the aircraft of its land
 * groups in reach, 'per' of them to a point, times the sea's air share. navy.ts airOver asks it for the seas the rule
 * names; SeaPicture adds the same figures in the same order.
 */
export function groupsOver(g: Game, z: number, nat: string, hostile: boolean, S: { per: number; cover?: Record<string, number> }, beside?: ((x: string) => boolean) | null): number {
  let s = 0;
  const near = nearRow(g, z);
  for (const f of g.airFleets) {
    // (`beside`: of its friends, those who count beside it in this sea: navy.ts besideIn. None is left out with the far side off.)
    // (A group whose field is beyond every reach in the tables adds nothing: fieldNear. Asked first, as it is the cheapest.)
    if (f.ship !== undefined || !fieldNear(g, near, f.base, z) || !(hostile ? atWar(g, f.nat, nat) : allied(g, f.nat, nat) && (!beside || beside(f.nat)))) continue;
    s += groupOverSea(g, f, z, S);
  }
  return (s / S.per) * g.seas[z].air;
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

function addAir(g: Game, r: FleetRow, at?: Region): AirFleet | null {
  const base = at ?? g.byName[r.base];
  if (!base) return null;
  const e = kept(g, r, Math.max(g.turn, r.from ?? 1)) ?? r.est[0];
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
    id: g.nextAir++, nat, name: unique, base: r.id, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
    mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 },
  };
  if (kind) f.name = groupName(g, nat, kind, strength, r, f);
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
export function rungsOf(g: Game, nat: string, base: Region | null, islands: string[] = NAMES.navyBases): Rung[] {
  const ladder = (nat === 'JAP' && base && islands.includes(base.name) ? NAMES.ladders.JAP_navy : null) ?? NAMES.ladders[nat] ?? [];
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

/**
 * A form of name with its number: {n}, {nth}, {n+K}, {nth_fr} (1re, 1er before a régiment, else 2e), {nth_nl} (1e).
 * (Release 2, under the switch 'airNames': where an arm's list holds whole labels, "I./JG 26" or "II/5", the label
 * stands for every number token.)
 */
export function formName(form: string, n: number | string): string {
  if (typeof n === 'string') return form.replace(/\{n\+\d+\}|\{nth_fr\}|\{nth_nl\}|\{nth\}|\{n\}/g, n);
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
function groupName(g: Game, nat: string, kind: 'ftr' | 'bmr', strength: number, base: Region | null, grp?: AirFleet): string {
  // (Release 2, the switch 'airNames': the arm's own numbers and words. Off, everything below is ruling 220's to the letter.)
  if (airNamesOn(g)) return groupNameR2(g, nat, kind, strength, base, grp);
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
 * A new air group's name under the switch 'airNames' (release 2; the design director's rulings of 25 November 2026,
 * airnames.ts; the historian's lists, air-unit-names-r2.json): the same rung as ruling 220 picks, with (a) the next
 * number from the arm's own list in forming order, going on past its end; (b) the rung's role form where the arm said
 * the role in its names; (c) for Japan's navy groups, the airfield's name at forming ({base}), a second group at the
 * same field taking the navy's numbered form. Where no rung is near, the arm's own plain form (its 'fallback'; for an
 * arm with none, the nearest rung of its own ladder however far), never another country's word. No name holds a
 * blocked one. The positions are kept in g.airNames, apart from every other counter, and no dice are thrown. `grp`,
 * where given, is told its short form and whether its name says its role (words for the card and the counter only).
 */
function groupNameR2(g: Game, nat: string, kind: 'ftr' | 'bmr', strength: number, base: Region | null, grp?: AirFleet): string {
  const taken = (x: string): boolean => airBlocked(x) || g.airFleets.some((f) => f.nat === nat && f.name === x);
  const tell = (name: string, form: { ftr: string; bmr: string }, short?: string): string => {
    if (grp) {
      grp.roleNamed = form.ftr !== form.bmr;
      if (short) grp.short = short; else delete grp.short;
    }
    return name;
  };
  const navy = nat === 'JAP' && !!base && AIRNAMES.navyIslands.includes(base.name);
  const arm = navy ? 'JAP_navy' : nat;
  const rungs = rungsOf(g, nat, base, AIRNAMES.navyIslands);
  const plain = (form: { ftr: string; bmr: string; numbers?: { ftr: Label[]; bmr: Label[] } }): string => {
    const got = nextNumber(g, `${arm}:${kind}`, form.numbers?.[kind], (n) => formName(form[kind], n), taken);
    return tell(got.name, form);
  };
  const named = (r: Rung & RungR2): string => {
    const form = r.formR2 ?? r.form;
    const f = form[kind];
    const short = (n: Label, at = ''): string | undefined => (r.short?.[kind] ? formName(r.short[kind], n).replace(/\{base\}/g, at) : undefined);
    // Japan's navy: the airfield's name at forming (the record's station where the province holds one); a second group
    // at the same field takes the navy's numbered form (the historian: the record's numbered groups).
    if (/\{base\}/.test(f) && base) {
      const at = AIRNAMES.navyStations[base.name] ?? base.name;
      const x = f.replace(/\{base\}/g, at);
      if (!taken(x)) return tell(x, form, short('', at));
      return plain(AIRNAMES.fallback[arm] ?? AIRWAR.groups.names._default);
    }
    // A rung with no number in its name (Poland's Brygada Pościgowa) is one formation.
    if (!/\{n/.test(f)) return tell(f, form);
    // (Germany's Gruppen: a label is passed over while a Geschwader-size group bears its Geschwader's number.)
    const skip = r.skipIfTaken?.[kind];
    const clash = (n: Label): boolean => !!skip && typeof n === 'string' && /\d+$/.test(n) && taken(formName(skip, Number(/\d+$/.exec(n)![0])));
    const got = nextNumber(g, `${nat}:${kind}:${r.rung}`, r.numbers?.[kind], (n) => formName(f, n), (x) => taken(x), clash);
    return tell(got.name, form, short(got.n));
  };
  const one = (r: Rung): boolean => { const f = ((r as Rung & RungR2).formR2 ?? r.form)[kind]; return !/\{n|\{base\}/.test(f) && taken(f); };
  for (let left = rungs; left.length;) {
    const r = nearestRung(left, kind, strength);
    if (!r) break;
    if (one(r)) { left = left.filter((x) => x !== r); continue; }
    return named(r as Rung & RungR2);
  }
  const own = AIRNAMES.fallback[arm];
  if (own) return plain(own);
  // No plain form of its own: the rung of its own ladder nearest the group's strength, however far.
  let best: Rung | null = null, bd = Infinity;
  for (const r of rungs) {
    const v = r[kind];
    if (!(v > 0) || one(r)) continue;
    const d = Math.abs(Math.log(Math.max(1, strength) / v));
    if (d < bd) { bd = d; best = r; }
  }
  if (best) return named(best as Rung & RungR2);
  return plain(AIRWAR.groups.names._default);
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
    // Under Units decide a group split off flies with the same pilots: their hours and experience go with it.
    if (i > 0 && f.hours !== undefined) grp.hours = f.hours;
    if (i > 0 && f.fxp !== undefined) grp.fxp = f.fxp;
    grp.ftr = p.kind === 'ftr' ? part(whole.ftr) : 0;
    grp.bmr = p.kind === 'bmr' ? part(whole.bmr) : 0;
    grp.est = { ftr: p.kind === 'ftr' ? part(whole.est.ftr) : 0, bmr: p.kind === 'bmr' ? part(whole.est.bmr) : 0 };
    if (whole.extra) grp.extra = { ftr: p.kind === 'ftr' ? part(whole.extra.ftr) : 0, bmr: p.kind === 'bmr' ? part(whole.extra.bmr) : 0 };
    if (i > 0) g.airFleets.splice(at + i, 0, grp);
    out.push(grp);
  });
  // The names are given once every group is on the list, so that no two of a nation's are alike.
  if (out.length > 1) for (const grp of out) grp.name = groupName(g, f.nat, grp.kind!, Math.max(grp.est[grp.kind!], grp[grp.kind!]), g.regions[grp.base] ?? null, grp);
  return out;
}

/**
 * The air commands history bases beyond the old chart (America's: ruling 133(b)) stand on their fields from the
 * first day of a grand campaign: the wider world is attached after the air forces of Europe are set up, so they
 * are raised here. A campaign saved before they were listed has them raised as its month finds them, at the
 * strength history gives them by then. Done once. The commands history forms later form in their month (airUpkeep).
 */
/** Whether a command's row can stand on a province: ground its country holds, or for a row marked 'ally' ground an ally holds that takes its allies' groups. */
const rowGround = (g: Game, r: FleetRow, at: Region): boolean => at.ctrl === r.nat || (!!r.ally && allied(g, r.nat, at.ctrl) && !thornGround(g, at, r.nat));
/** True in a month the record keeps a command at nothing: it was withdrawn, its men and aircraft gone to other commands. */
const stoodDown = (g: Game, r: FleetRow): boolean => { const e = kept(g, r); return !!e && e[1] + e[2] <= 0; };

export function openWorldAir(g: Game): void {
  if (g.flags.worldair === 1) return;
  g.flags.worldair = 1;
  for (const r of AIR.fleets) {
    const base = g.byName[r.base];
    if (!base || !beyondEurope(base) || (r.from ?? 1) > 1 || r.scripted || !rowGround(g, r, base) || !g.nations[r.nat]?.alive) continue;
    if (g.airFleets.some((f) => f.nat === r.nat && (f.parent ?? f.name) === r.name)) continue;
    // (A command the record has stood down by this month does not stand: a campaign begun late.)
    if (stoodDown(g, r)) continue;
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
  // (On its way to another field by stages, ferry.ts: the switch 'ferry' alone writes `way`, so off this is never read.)
  if (f.way !== undefined) return `It is on its way to ${g.regions[f.way.to]?.name ?? 'another airfield'}: send it somewhere else from its page, or call it off.`;
  if (r.id === f.base) return 'It is already based there.';
  const no = landsAt(g, f, r);
  if (no) return no;
  if (f.moved === g.turn) return 'It has already changed base this month.';
  // Airfields as targets (fieldstate.ts): a shut runway takes nobody in and lets nobody out, and bombers need concrete.
  if (g.flags.fields === 1 && fieldRules(g) && fieldShut(g, f.base)) return `Grounded: the runway at ${g.regions[f.base].name} is cratered until it is repaired.`;
  const on = landsOn(g, f, r);
  if (on) return on;
  // (A group shipped across an ocean with its country's army goes in crates, however far: world.json 'overseas'.)
  if (!bySea && km(g, f.base, r.id) > AIR.ferry) return 'Too far to move in one month.';
  return null;
}

/** Why a group could not land on a region's airfields at all, whatever its own state: the ground, the field and when it was taken. */
function landsAt(g: Game, f: AirFleet, r: Region): string | null {
  if (thornGround(g, r, f.nat)) return `${g.nations[r.owner].name} keeps its own air force: no group of its allies is based there.`;
  // An airfield lies outside the town, and goes with the country round it (prizes.ts): an army that holds the
  // field of an enemy province has the use of its airfield after a month, and a wrecked one serves nobody.
  if (!airfieldOpenTo(g, f.nat, r)) {
    const ours = besieged(g, r) && allied(g, f.nat, fieldCtrl(g, r));
    if (!allied(g, f.nat, r.ctrl) && !ours) return 'That ground is not in friendly hands.';
    if (!hasAirfield(g, r.id)) return 'There is no airfield there. Engineers can lay one out, and the treasury can build one.';
    const out = prizeOut(g, r, 'airfield');
    if (out > 0 && g.flags.fields === 1 && fieldRules(g)) return `The runway at ${r.name} is cratered: no aircraft can land until it is repaired${out === 1 ? ' (its crews have it open next month)' : ''}.`;
    if (out > 0) return `The airfield there is wrecked: it will be ${out === 1 ? 'a month' : `${out} months`} before aircraft can use it.`;
    return ours ? 'Airfields taken this month are not yet in use.' : 'The enemy holds the country round the town, and the airfield with it.';
  }
  if (!hasAirfield(g, r.id)) return 'There is no airfield there. Engineers can lay one out, and the treasury can build one.';
  if (r.since >= g.turn) return 'Airfields taken this month are not yet in use.';
  return null;
}

/** Airfields as targets (fieldstate.ts): why a group could not land on this field's runway now, or null. */
function landsOn(g: Game, f: AirFleet, r: Region): string | null {
  if (!(g.flags.fields === 1 && fieldRules(g))) return null;
  if (fieldShut(g, r.id)) return `The runway at ${r.name} is cratered: no aircraft can land until it is repaired.`;
  if (f.kind === 'bmr' && f.ashore === undefined && fieldTier(g, r.id) === 'strip') return `Bombers need a concrete airfield: ${r.name} has a field strip.`;
  return null;
}

/**
 * Why a land group could not put down on a region's airfields on its way somewhere (ferry.ts): the friendly rule and the
 * field's own state, exactly as for a move (whyNotBase), leaving out where the group is, how far it is and whether it has
 * moved this month. Null where it could.
 */
export const whyNotStop = (g: Game, f: AirFleet, r: Region): string | null => landsAt(g, f, r) ?? landsOn(g, f, r);

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
  grp.name = groupName(g, f.nat, kind, Math.max(grp.est[kind], grp[kind]), g.regions[grp.base] ?? null, grp);
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
  // (A group on its way to another field by stages, ferry.ts, is left to go on: only the switch 'ferry' writes `way`.)
  const land = (f: AirFleet): boolean => f.nat === nat && f.ship === undefined && f.ashore === undefined && f.base >= 0 && !!g.regions[f.base] && f.way === undefined;
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
      const here = g.airFleets.filter((f) => land(f) && f.base === F && f.moved !== g.turn && keepsRoom(f) > 0 && !posted(g, f));
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

/** Why a group cannot patrol this sea against submarines (release 2, seapatrol.ts), or null if it can. */
function whyNotPatrol(g: Game, f: AirFleet, z: number): string | null {
  if (!patrolOn(g)) return 'Sea patrols against submarines are flown only where units decide, once the patrol is live.';
  if (!g.seas[z]) return 'Pick a sea.';
  if (deckBred(f)) return 'Carrier aircraft cover their own fleet.';
  if (f.kind === 'ftr' || f.bmr <= 0) return 'Only bombers patrol the sea: fighters do not reach far enough.';
  const field = whyNotField(g, f);
  if (field) return field;
  // (As a raid: a bomber group left on a field strip flies from it only once moved to a concrete airfield.)
  if (g.flags.fields === 1 && f.kind === 'bmr' && f.ashore === undefined && fieldRules(g) && fieldTier(g, f.base) === 'strip') return `Bombers need a concrete airfield to fly from: ${g.regions[f.base].name} has a field strip.`;
  if (patrolReach(g, f, z)) return null;
  return g.seas[z].coast.length || aswKit(g, f.nat).patrol ? 'That sea is beyond its bombers.' : 'Only long-range patrol aircraft reach a sea with no coast.';
}

/** The weather zone a mission's target lies in: a sea's, a fleet's sea's, or a province's (ruling 127). */
export function skyZone(g: Game, m: AirMission, target: number): number {
  if (m === 'strike' || m === 'seapatrol') return zoneOfSea(g, target);
  if (m === 'fleet') { const t = g.fleets.find((x) => x.id === target); return t && atSea(t) ? zoneOfSea(g, t.zone) : -1; }
  return zoneOfRegion(g, target);
}

/** Why a fleet cannot fly this mission against this target, or null if it can. */
export function whyNotMission(g: Game, f: AirFleet, m: AirMission, target: number): string | null {
  if (m === 'support' || m === 'rest') return null;
  if (!g.nations[f.nat].atWar.length) return 'The nation is at peace.';
  // (A carrier's aircraft fly nothing while it is in passage to a new base: release 2, the far chart. Its fighters'
  // standing cover over it, 'support', is not barred.)
  { const c = carrierOf(g, f); if (c?.stance === 'passage') return passageWhy(g, c); }
  // (The sea patrol, release 2: a standing order the sky works on through the month, not a raid it bars.)
  if (m === 'seapatrol') return whyNotPatrol(g, f, target);
  // Weather (ruling 127): a mission needs an open sky at its base and at its target.
  if (weatherOn(g)) { const why = whyNotSky(g, f, skyZone(g, m, target)); if (why) return why; }
  // A field wrecked by saboteurs grounds every group based on it for the month (ruling 151(b)).
  const field = whyNotField(g, f);
  if (field) return field;
  // Airfields as targets: a bomber group left on a field strip flies no raid from it until it is moved.
  if (g.flags.fields === 1 && f.kind === 'bmr' && f.ship === undefined && f.ashore === undefined && fieldRules(g) && fieldTier(g, f.base) === 'strip') return `Bombers need a concrete airfield to fly from: ${g.regions[f.base].name} has a field strip.`;
  const R = reach(f, g, raidJob(m));
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
  if (m === 'airfield') {
    if (!fieldRules(g)) return 'Airfields are bombed only with airfields as targets.';
    if (!atWar(g, f.nat, r.ctrl)) return 'That airfield is not in enemy hands.';
    if (!hasAirfield(g, target)) return 'There is no airfield there.';
    if (!f.bmr && !escort) return f.kind === 'ftr' ? 'Fighters go only as the escort of bombers.' : 'It has no bombers.';
    return d > (escort ? R.ftrRange : R.bmrRange) ? (escort ? 'That airfield is beyond its fighters.' : 'That airfield is beyond its bombers.') : null;
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
  if (m === 'seapatrol') return g.seas.map((z) => z.id).filter((t) => whyNotMission(g, f, m, t) === null);
  if (!isOffensive(m)) return [];
  const all = m === 'strike' ? g.seas.map((z) => z.id) : m === 'fleet' ? g.fleets.map((x) => x.id) : g.regions.map((r) => r.id);
  return all.filter((t) => whyNotMission(g, f, m, t) === null);
}

/**
 * Whether a harbour lies within an air group's raid by distance alone, from where the group is now: the test
 * whyNotMission makes of a harbour raid, without the war, the sky or the ships there (release 2, harbourstrike.ts:
 * the page that tells a carrier fleet's commander which harbours his aircraft reach, at peace too).
 */
export function harbourInReach(g: Game, f: AirFleet, target: number): boolean {
  if (!g.regions[target]) return false;
  const R = reach(f, g, raidJob('harbour'));
  const escort = f.kind === 'ftr' && f.ftr > 0;
  return kmTo(g, f, target) <= (escort ? R.ftrRange : R.bmrRange);
}

export function setMission(g: Game, f: AirFleet, m: AirMission, target = -1): boolean {
  if (whyNotMission(g, f, m, target)) return false;
  f.mission = m;
  f.target = isOffensive(m) || m === 'seapatrol' ? target : -1;
  return true;
}

// ---- The month in the air -----------------------------------------------------------------

/**
 * Where a command forms: its base, or for one the record moved forward before it formed (it was stood down and comes
 * again, the 4th Air Army in the Philippines) the latest of its forward bases its side holds.
 */
function formsAt(g: Game, r: FleetRow): Region | undefined {
  if (r.moves) for (let i = r.moves.length - 1; i >= 0; i--) {
    const at = r.moves[i][0] <= g.turn ? g.byName[r.moves[i][1]] : undefined;
    if (at && rowGround(g, r, at)) return at;
  }
  return g.byName[r.base];
}

/**
 * The record's forward bases (air.json 'moves', the far side's rows): a computer staff's command goes to the latest
 * one whose month has come, once, by sea however far (ruling 184: aircraft follow by ship), if its side holds the
 * ground and its airfield is in use; a field is laid out there as it is where a command forms. Only at the stops of
 * the history dial that keep the book, while the Pacific's book stands: elsewhere the staff bases its groups itself.
 */
function recordMoves(g: Game, nat: string): void {
  if (nat === g.human || !hopsBind(g)) return;
  for (const r of AIR.fleets) {
    if (r.nat !== nat || !r.moves) continue;
    let m: [number, string] | null = null;
    for (const x of r.moves) if (x[0] <= g.turn) m = x;
    const key = `air:mv:${r.name}`;
    if (!m || g.flags[key] === m[0]) continue;
    const to = g.byName[m[1]];
    if (!to || !rowGround(g, r, to) || to.since >= g.turn) continue;
    const groups = g.airFleets.filter((f) => f.nat === nat && f.ship === undefined && f.ashore === undefined && rowOf(f) === r && f.base !== to.id);
    if (airWar(g) && !hasAirfield(g, to.id) && groups.length) g.flags[`af:${to.id}`] = AIRWAR.fields.seed.base;
    let went = false;
    for (const f of groups) went = rebaseAir(g, f, to, true) || went;
    // (Taken, or nothing of the command left to go: either way this move is done with.)
    if (went || !groups.length) g.flags[key] = m[0];
    if (went) headline(g, 'info', `${r.name} moves up to airfields in ${to.name}.`, nat, to.id);
  }
}

/**
 * The far side: a nation's supply picture for its land air groups' fields, as the month's supply rule draws it
 * (game.ts computeSupply: the map a corps' drafts read), or null where every field counts as fed: with the far
 * side's switch off, for a country at peace, and for a country with no army in the field, which has no picture.
 * (Ruling 184's count of a field's unfed months; and the design director's ruling of 21:58Z, 21 November 2026, on
 * Luzon's sky: a land-based group takes from its country's reserve only where its field is fed, and the reckoning of
 * what a defender can put up once his losses are made good reads the same. The picture is drawn fresh if the board
 * has moved since it was last drawn, so that a group reads the map the corps beside it reads.)
 */
function fedFields(g: Game, nat: string): Uint8Array | null {
  if (!skiesOn(g) || !g.nations[nat]?.atWar.length) return null;
  ensureSupply(g);
  return g.supply[nat] ?? null;
}
/** True where a land air group's field is fed (`fedFields`). A carrier wing has no field: it is made up in harbour. */
export function fieldFed(g: Game, f: AirFleet): boolean {
  if (f.ship !== undefined) return true;
  const fed = fedFields(g, f.nat);
  return !fed || !!fed[f.base];
}

/** The start of a nation's month in the air: new formations, lost bases, new aircraft and pilots, and rest. */
export function airUpkeep(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n?.alive) return;
  // (Rider b525: a part of a command that leaves for a reason outside the game stays where no book is kept and the reason no longer holds.
  // Riders b522 and b534: so does what the record takes from a command that watches a neighbour, in a month the two are at war: the
  // record's reason, a quiet border, does not hold, and the withdrawal is dropped for good.)
  if (!stopDef(g).book) for (const r of AIR.fleets) {
    if (r.nat !== nat) continue;
    for (const L of r.leaves ?? []) if (L[0] === g.turn && atWar(g, L[3], L[4])) g.flags[`air:stay:${r.name}:${L[0]}`] = 1;
    if (!r.watch || !atWar(g, nat, r.watch)) continue;
    for (let k = 1; k < r.est.length; k++) if (r.est[k][0] === g.turn && (r.est[k][1] < r.est[k - 1][1] || r.est[k][2] < r.est[k - 1][2])) g.flags[`air:stay:${r.name}:${g.turn}`] = 1;
  }
  for (const r of AIR.fleets) {
    if (r.nat !== nat || (r.from ?? 1) <= 1 || g.turn < r.from! || g.flags[`air:${r.name}`]) continue;
    // An air force history raised for an invasion of its own planning is nobody's to raise in free play.
    if (r.scripted && g.flags.open === 1) continue;
    // (A command the record keeps at nothing this month does not form; one that moved forward forms where the record has it by now.)
    if (stoodDown(g, r)) continue;
    const base = formsAt(g, r);
    if (!base || !rowGround(g, r, base) || base.since >= g.turn) continue;
    const f = addAir(g, r, base);
    if (!f) continue;
    g.flags[`air:${r.name}`] = g.turn;
    // (Rider b525: where the record renamed a command into this one, what that command held by the record is counted
    // against this one's figure: it forms without them, and takes what the old command has left: handOver.)
    for (const old of dueInto(g, r)) {
      const had = kept(g, old);
      if (had) { f.ftr = Math.max(0, f.ftr - had[1]); f.bmr = Math.max(0, f.bmr - had[2]); }
    }
    headline(g, 'info', `${f.name} forms in ${base.name}.`, nat, base.id);
    // The air war: it forms as air groups, and the ground it forms on has its airfields.
    if (airWar(g)) { if (!hasAirfield(g, base.id)) g.flags[`af:${base.id}`] = AIRWAR.fields.seed.base; splitFleet(g, f); }
  }
  firstOfKind(g, nat);
  handOver(g);
  if (airWar(g)) growCommands(g, nat);
  // (The fight back: a country at war whose air force was only a rating raises it, and keeps it up.)
  raiseAirForce(g, nat);
  const p = airPool(g, nat);
  const minor = AIR.production[nat] ? null : minorBuilt(g, nat);
  const built = row(AIR.production[nat], g.turn) ?? minor, trained = row(AIR.pilots.perMonth[nat], g.turn) ?? (minor ? [minor[0], minor[1]] : null);
  if (built) { p.ftr += built[1]; p.bmr += built[2]; }
  if (trained) poolIn(g, nat, trained[1]);
  // The far side (ruling 184): whether each land group's field is fed, as the supply picture stands. (A country with no army in the field has no picture: its groups count as fed.)
  const fed = fedFields(g, nat);
  for (const f of airFleetsOf(g, nat)) {
    // A carrier wing has no airfield to lose: it goes where its fleet goes.
    if (f.ship !== undefined) continue;
    // Airfields lie out in the country: a group whose province is under siege has lost them, though the town holds.
    // (The army round an enemy town has its airfield, and a wrecked field serves nobody: prizes.ts.)
    if (!airfieldOpenTo(g, nat, g.regions[f.base]) && !fallBack(g, f)) continue;
    if (fed && !fed[f.base]) f.unfed = (f.unfed ?? 0) + 1; else if (f.unfed !== undefined) delete f.unfed;
    // The staff moves strength between fleets by changing what each is kept at.
    const r = rowOf(f);
    const e = r ? kept(g, r) : null;
    // A command the record withdrew is stood down: its aircraft and pilots go back to the pool for the commands that
    // took its place. (Wings bought for a group are the player's own: it stays, at what was bought.)
    if (e && e[1] + e[2] <= 0 && !f.extra) {
      p.ftr += f.ftr; poolIn(g, nat, f.ftr, f); p.bmr += f.bmr;
      g.airFleets.splice(g.airFleets.indexOf(f), 1);
      if (g.flags[`air:${r!.name}`]) { delete g.flags[`air:${r!.name}`]; delete g.flags[`air:mv:${r!.name}`]; }
      if (!g.airFleets.some((x) => x !== f && x.nat === nat && rowOf(x) === r)) headline(g, 'info', `${r!.name} is stood down: its men and aircraft go to other commands.`, nat, f.base);
      continue;
    }
    // An air group is kept at its share of its command's aircraft of its own kind.
    const ef = e ? (f.kind === 'bmr' ? 0 : Math.round(e[1] * (f.share ?? 1))) : 0, eb = e ? (f.kind === 'ftr' ? 0 : Math.round(e[2] * (f.share ?? 1))) : 0;
    // Wings bought from the arsenal are kept on top of what the staff allots.
    if (e) {
      const est = f.extra ? { ftr: ef + f.extra.ftr, bmr: eb + f.extra.bmr } : { ftr: ef, bmr: eb };
      // (A country that builds no aircraft on these rows, and a command that stands apart: what the record adds to the command arrives with the date. Not in another's keeping, whose works supply it.)
      const apart = !!r!.watch && !f.raised;
      if ((r!.arrives || r!.watch) && !f.raised) { f.ftr += Math.max(0, est.ftr - f.est.ftr); f.bmr += Math.max(0, est.bmr - f.est.bmr); }
      f.est = est;
      // (And what the record takes from a command that stands apart leaves with the date: it is on other rows already.)
      if (apart) { f.ftr = Math.min(f.ftr, est.ftr); f.bmr = Math.min(f.bmr, est.bmr); }
      // (So does a part of a command that leaves for a reason outside the game: home, not to the pool.)
      for (const L of r!.leaves ?? []) {
        if (L[0] !== g.turn || g.flags[`air:stay:${r!.name}:${L[0]}`] || f.raised) continue;
        if (f.kind !== 'bmr') f.ftr = Math.max(0, f.ftr - Math.round(L[1] * (f.share ?? 1)));
        if (f.kind !== 'ftr') f.bmr = Math.max(0, f.bmr - Math.round(L[2] * (f.share ?? 1)));
      }
    }
    if (f.ftr > f.est.ftr) { const s = f.ftr - f.est.ftr; f.ftr -= s; p.ftr += s; poolIn(g, nat, s, f); }
    if (f.bmr > f.est.bmr) { const s = f.bmr - f.est.bmr; f.bmr -= s; p.bmr += s; }
  }
  recordMoves(g, nat);
  // Replacements go to resting fleets first.
  const order = airFleetsOf(g, nat).sort((a, b) => (a.mission === 'rest' ? 0 : 1) - (b.mission === 'rest' ? 0 : 1) || a.id - b.id);
  const R = AIR.ready;
  for (const f of order) {
    // A carrier wing is made up only while its fleet lies in harbour: no replacement aircraft reach a deck at sea.
    // (The far side: nor do any reach a land group whose field is not fed. A corps draws its drafts only when it is
    // supplied, and the air reads the same map: the design director's ruling of 21:58Z, 21 November 2026.)
    const ship = f.ship === undefined ? null : carrierOf(g, f);
    const refits = f.ship === undefined ? !fed || !!fed[f.base] : !!ship && !atSea(ship);
    const nf = refits ? Math.max(0, Math.min(f.est.ftr - f.ftr, p.ftr, p.pilots)) : 0;
    // (Under Units decide the newcomers dilute what the group knows: its hours and experience become the means.)
    joinPilots(g, f, nf, 'ftr', 'pool');
    f.ftr += nf; p.ftr -= nf; p.pilots -= nf;
    const nb = refits ? Math.max(0, Math.min(f.est.bmr - f.bmr, p.bmr)) : 0;
    joinPilots(g, f, nb, 'bmr', 'pool');
    f.bmr += nb; p.bmr -= nb;
    f.ready = clamp(f.ready + (f.mission === 'rest' ? R.rest : f.mission === 'support' ? R.support : R.flying), R.floor, 1);
    // A group that flew a planned raid last month is back at its airfield, on patrol again.
    if (f.flew !== undefined && f.flew < g.turn && isOffensive(f.mission)) { f.mission = 'support'; f.target = -1; }
    // Orders that no longer make sense lapse.
    if (!n.atWar.length || ((isOffensive(f.mission) || f.mission === 'seapatrol') && whyNotMission(g, f, f.mission, f.target))) { f.mission = 'support'; f.target = -1; }
  }
  const most = AIR.reserve[nat] ?? AIR.reserve._default;
  p.ftr = Math.min(p.ftr, most.ftr);
  p.bmr = Math.min(p.bmr, most.bmr);
  p.pilots = Math.min(p.pilots, most.pilots);
  // Fighter types (the switch 'Fighter types'): the month the nation reached its fighter stage, and new types as news.
  fighterMonth(g, nat);
}

/**
 * The record's changes of name (ruling 234's rider b525): a command the record renamed ends in the first month, from
 * the record's own, that every command which took its place stands, and what it holds passes to them, kind by kind
 * up to what each is kept at, in the order the row names them. What is left over goes to the pool of the country that
 * took it over. So nothing stands twice, and nothing vanishes because a date came: where the new command never forms
 * (its ground is not held), the old one stands. Every stop, every seat. Read in every country's month, since the two
 * commands may be different countries' (the volunteer group in China's service, the task force America's).
 */
function handOver(g: Game): void {
  for (const r of AIR.fleets) {
    const at = r.to ? endOf(r) : undefined;
    if (at === undefined || at > g.turn || g.flags[`air:over:${r.name}`]) continue;
    const next = r.to!.map((n) => AIR.fleets.find((x) => x.name === n)).filter((x): x is FleetRow => !!x);
    const groups = (x: FleetRow): AirFleet[] => g.airFleets.filter((f) => f.ship === undefined && rowOf(f) === x);
    // (A command that has formed stands, though a battle has left it no group.)
    if (!next.length || !next.every((x) => groups(x).length > 0 || g.flags[`air:${x.name}`] || ((x.from ?? 1) <= 1 && g.flags.worldair === 1))) continue;
    const old = groups(r);
    g.flags[`air:over:${r.name}`] = g.turn;
    if (!old.length) continue;
    let ftr = old.reduce((s, f) => s + f.ftr, 0), bmr = old.reduce((s, f) => s + f.bmr, 0);
    const where = old[0].base;
    for (const x of next) {
      for (const f of groups(x)) {
        const a = Math.min(ftr, Math.max(0, f.est.ftr - f.ftr)), b = Math.min(bmr, Math.max(0, f.est.bmr - f.bmr));
        f.ftr += a; ftr -= a; f.bmr += b; bmr -= b;
      }
    }
    const p = airPool(g, next[0].nat);
    p.ftr += ftr; poolIn(g, next[0].nat, ftr, old[0]); p.bmr += bmr;
    for (const f of old) g.airFleets.splice(g.airFleets.indexOf(f), 1);
    if (g.flags[`air:${r.name}`]) { delete g.flags[`air:${r.name}`]; delete g.flags[`air:mv:${r.name}`]; }
    headline(g, 'info', `${r.name} is no more: its men and aircraft are now ${list(next.map((x) => x.name))}.`, old[0].nat, where);
  }
}

/**
 * A kind the record gives a command for the first time: its figure of that kind was nothing until this month (Far
 * East Command's fighters of September 1941, where the record has only old bombers before). In the air war a command
 * stands as groups of one kind, so its first group of the new kind forms beside the others, empty, and is filled as
 * any group is: from the pool, or with the date where the command's additions arrive. A command that has lost every
 * group of a kind it once had raises none again.
 */
function firstOfKind(g: Game, nat: string): void {
  if (!airWar(g)) return;
  for (const r of AIR.fleets) {
    if (r.nat !== nat || r.est.length < 2) continue;
    const e = kept(g, r), was = kept(g, r, g.turn - 1);
    if (!e || !was) continue;
    for (const [kind, want, had] of [['ftr', e[1], was[1]], ['bmr', e[2], was[2]]] as const) {
      if (want <= 0 || had > 0) continue;
      // (Its groups under whoever keeps them: a fallen country's command in another's keeping.)
      const mine = g.airFleets.filter((f) => f.ship === undefined && rowOf(f) === r);
      if (!mine.length || mine.some((f) => f.kind === kind || f.kind === undefined)) continue;
      const first = mine[0], base = g.regions[first.base];
      if (!base) continue;
      const grp: AirFleet = {
        id: g.nextAir++, nat: first.nat, name: '', base: first.base, ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
        mission: 'support', target: -1, ready: first.ready, leader: null, moved: first.moved, lost: { ftr: 0, bmr: 0 }, kind, parent: r.name, share: 1,
      };
      if (first.raised) grp.raised = first.raised;
      // (The group itself goes with the asking: under the air arms' own names it is told its short form and whether
      // its name says its role, as every other group named there is. Off, nothing reads it.)
      grp.name = groupName(g, first.nat, kind, want, base, grp);
      g.airFleets.splice(g.airFleets.indexOf(mine[mine.length - 1]) + 1, 0, grp);
      headline(g, 'info', `${grp.name} forms in ${base.name}, a new group of ${r.name}.`, first.nat, base.id);
    }
  }
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
        grp.name = groupName(g, nat, kind, Math.round(want / n), base, grp);
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
      const d = chartDist(g, r, old);
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
  p.lostFtr += lf; p.lostBmr += lb; poolIn(g, f.nat, lf, f);
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

/**
 * Fog on raids (airfog.ts, behind its switch): the defence of a target as one side knows it, for a forecast made on
 * that side's knowledge in place of the truth. It is given only where a raid is weighed before it flies, by the raid
 * card or by a staff; a raid that is flown is always priced on what is really there.
 */
export interface KnownDefence {
  /** True for an enemy fighter group this side knows of for sure: its field is in the side's sight now. */
  sure: (f: AirFleet) => boolean;
  /** Fighters besides those, last seen in reach of the target and counted as all rising, and whose they were. */
  extra: number;
  extraNat: string;
  /** The level of the guns round the target as this side counts them. */
  aa: number;
  /**
   * For the card's words only (the UI director's 20:08Z marks): the level of guns this side last saw built there (-1
   * where it never saw the place), and how many months ago (airfog.ts SkyKnown). Nothing in the rules reads them.
   */
  seen?: number;
  ago?: number;
  /** The share of the town in ruins as this side last saw it (0 to 1). */
  ruined: number;
}

export interface AirAssess {
  /** Fog on raids: set only where the mission was weighed on one side's knowledge (KnownDefence), never on a raid flown. */
  known?: KnownDefence;
  /**
   * Fog on raids: the fighters last seen in reach of the target that the weighing counts as all rising (KnownDefence's
   * extra): how many, the share of them that meets the raid, one's fighting weight and whose they were. Set only with
   * `known`, and only where some rise. The sky gate of a raid on troops reads it (skyWon, skyGate), so that the card's
   * gate counts the fighters the card's own odds count.
   */
  unseen?: { n: number; w: number; each: number; nat: string };
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
  /**
   * The standing light guns every target has (standingGuns), counted apart from the built guns in `guns` wherever the
   * loss reads the plain flak share and the count is given: a target on land that bombers go for, by day or by night
   * (0 for a fight for the sky by fighters alone and for any raid at sea). A count for the raid card; nothing in the
   * rules reads it.
   */
  standing: number;
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
  /**
   * The fighters' own types (fighters.ts, the switch 'Fighter types'): the escort's edge over the defenders from the
   * aircraft's measured performance at the mission's height, in log units. Absent when the switch is off.
   */
  typeEdge?: number;
  /**
   * Under Units decide, the pilots' training on the raid card (the UI director 24 Nov 18:10Z), only where it moves
   * the fight (k > 0) and every group on both sides has its hours read: "Pilots' training: ours 250 hours, theirs 350."
   */
  pilotsLine?: string;
  /**
   * Units decide, the fighters' own fight by day (sweep-height-form-1124.md, escort-height-form-1124.md): the defenders'
   * warning in time W (0 to 1), who is above in bands (x: 1 the attackers, 0 level, -1 the defenders; a sweep's 1 - 2W,
   * an escort's the mean of its groups' by role) and the field's height edge h a band. Set, the escort's flat share is
   * gone and the defenders' weight in that fight is without the radar's control. `plain`: a raid's bomber loss before
   * warned defenders above its bombers (the field's arming; set only where that moved it). Absent: off, or by night.
   */
  height?: { W: number; x: number; h: number; plain?: number };
  /** What the bombers that go are worth against ships, beside carrier crews trained for it (1). */
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
  /**
   * City defences (the design director's ruling of 25 Nov 2026 02:23Z), a night raid only and only with the switch on:
   * the searchlights at the target. `level` their level, `cloud` true where cloud blinds them, `lit` (0 to 1) the
   * share of the guns' levels firing by searchlight after the too-many-bombers cut (none without Fighter control's
   * radar, none with no guns). Absent otherwise, so nothing is other than it was with the switch off.
   */
  lights?: { level: number; cloud: boolean; lit: number };
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
export interface ShipHits { hits: number; sunk: Ships; count: number; value: number; by?: Ships[]; plain?: Ships }

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
}

export function shipLaw(a: AirAssess, m: AirMission, found = 1): ShipLaw {
  const have = noShips();
  for (const x of a.ships) for (const c of SHIPS) have[c] += x.ships[c];
  return {
    harbour: m === 'harbour', naval: a.naval, spoil: a.spoil, again: a.again, found, surprise: a.surprise, have,
    fleets: a.ships.map((x) => ({ id: x.id, nat: x.nat, name: x.name, ships: { ...x.ships } })),
    ...(a.tough ? { tough: [...a.tough] } : {}),
  };
}

/** shipHits on a kept law (the same sums, in the same order). */
export function shipHitsBy(L: ShipLaw, through: number): ShipHits {
  const S = CARRIERS.strike, H = CARRIERS.harbour;
  const sunk = noShips();
  let hits = through * L.naval * S.hit * (1 - L.spoil) * L.again * L.found;
  if (L.harbour) hits *= H.hit * (L.surprise ? H.surprise : 1);
  const have = L.have;
  const weight = SHIPS.reduce((s, c) => s + have[c] * S.aim[c], 0);
  if (weight <= 0 || hits <= 0) return { hits: 0, sunk, count: 0, value: 0 };
  let count = 0;
  for (const c of SHIPS) { sunk[c] = Math.min(have[c], ((hits * have[c] * S.aim[c]) / weight) / S.takes[c]); count += sunk[c]; }
  // No strike sinks more than so much of what it finds.
  const most = (L.harbour ? H.most : S.most) * SHIPS.reduce((s, c) => s + (S.aim[c] > 0 ? have[c] : 0), 0);
  if (count > most) { for (const c of SHIPS) sunk[c] *= most / count; hits *= most / count; count = most; }
  if (L.tough) {
    const plain = { ...sunk }, tough = L.tough;
    const by = L.fleets.map((x, i) => {
      const part = noShips();
      for (const c of SHIPS) if (have[c] > 0) part[c] = (plain[c] * (x.ships[c] / have[c])) / (c === 'ss' ? 1 : tough[i] ?? 1);
      return part;
    });
    for (const c of SHIPS) sunk[c] = by.reduce((s, part) => s + part[c], 0);
    count = SHIPS.reduce((s, c) => s + sunk[c], 0);
    return { hits, sunk, count, value: SHIPS.reduce((s, c) => s + sunk[c] * S.value[c], 0), by, plain };
  }
  return { hits, sunk, count, value: SHIPS.reduce((s, c) => s + sunk[c] * S.value[c], 0) };
}

/** How hard the fighters fight over this kind of raid: in the air war only a fight for the sky is fought to a finish. */
const clashOf = (g: Game, m: AirMission): number => (airWar(g) && m !== 'superiority' ? AIRWAR.losses.clash : 1);

/**
 * The largest share of its bombers one raid can lose. In the air war a day raid may lose up to the record's worst
 * (ruling 227: the Heligoland Bight, 55 to 65%), a night raid no more than before.
 */
const mostBomberLoss = (g: Game, night = false): number => (airWar(g) ? (night ? AIRWAR.losses.most : AIRWAR.losses.day) : AIR.combat.maxBomberLoss);

/** What the map says about one air campaign before it is flown: who can reach, who will meet them, and what helps each side. */
/**
 * The defenders' warning in time over the fighters' own fight (Units decide; the design director 24 Nov 15:20Z and
 * 16:09Z): 1 with a chain over the place, an order their agent read, or carriers with radar researched; a carrier's
 * lookouts over its own fleet, and ground observers over ground held by anyone on the defenders' side, 0.5 (the range
 * middle, until the historian's lead times; rule C); 0 over ground the other side holds, or the sea, with none.
 */
function warnedIn(g: Game, a: AirAssess, sea: boolean, amb: boolean, target: number): number {
  if (amb || a.radar) return 1;
  if (a.def.length && a.def.every((x) => x.f.ship !== undefined)) return a.def.every((x) => hasTech(g, x.f.nat, AIRWAR.sky.radar)) ? 1 : AIR.height.lookouts;
  if (sea) return 0;
  const r = g.regions[target];
  return r && a.def.some((x) => x.f.nat === r.ctrl || allied(g, x.f.nat, r.ctrl)) ? AIR.height.observers : 0;
}

/**
 * Units decide: the role an escort's k-th fighter group flies (its groups in the field's order, air/ticket.ts). Every
 * nation's the field's present rule until the doctrine store lands (the design director 24 Nov 15:33Z): the first
 * SKY.escort.topFrom groups close escort, then top cover and close escort by turns.
 */
export const UD_ROLES = ['close', 'top'] as const;
export const udRole = (k: number): 'close' | 'top' => (k < SKY.escort.topFrom ? 'close' : UD_ROLES[(k - SKY.escort.topFrom + 1) % UD_ROLES.length]);

/**
 * Units decide: an escort's height over the defenders, in bands (1 above, 0 level, -1 below), by its groups' roles and
 * weighed by their fighters. The form (the design director 15:33Z) put the close escort at the bombers' height, top
 * cover a band above and the defenders 2W - 1 bands from the bombers by their warning; each role's height is as the
 * field fights it, measured at W 0, 0.5 and 1 (AIR.height.escort): a close escort cannot leave its bombers to dive on
 * defenders below, and top cover's dive is about half a band short.
 */
function escortAbove(g: Game, a: AirAssess, W: number): number {
  const at = (row: number[]): number => {
    const i = clamp(W, 0, 1) * (row.length - 1), lo = Math.floor(i), hi = Math.min(row.length - 1, lo + 1);
    return row[lo] + (row[hi] - row[lo]) * (i - lo);
  };
  const close = at(AIR.height.escort.close), top = at(AIR.height.escort.top);
  let k = 0, sum = 0, wt = 0;
  for (const x of a.att) {
    // The groups the field flies, in its order (air/ticket.ts): those that reach the target with a fighter or more.
    if (!(x.esc > 0 && Math.round(x.f.ftr * x.crowd) > 0)) continue;
    const w = x.ftr * worth(g, x.f);
    sum += w * (udRole(k++) === 'top' ? top : close);
    wt += w;
  }
  return wt > 0 ? sum / wt : 0;
}

/** A count in words to ten, for the searchlights' line (the UI director, 25 Nov 2026 20:33Z), and in digits above ten. */
const upToTen = (n: number): string => ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'][n] ?? String(n);

export function assessAir(g: Game, fleets: AirFleet[], m: AirMission, target: number, known?: KnownDefence, transit?: Transit): AirAssess {
  const C = AIR.combat;
  // A strike on one fleet is fought over the sea that fleet is in.
  const quarry = m === 'fleet' ? g.fleets.find((x) => x.id === target) ?? null : null;
  const sea = m === 'strike' || m === 'fleet' || m === 'seapatrol', night = m === 'night';
  const zone = m === 'fleet' ? quarry?.zone ?? -1 : target;
  const nat = fleets[0]?.nat ?? '';
  const place = sea ? (g.seas[zone] ? seaWhat(g.seas[zone].name) : 'the sea') : g.regions[target]?.name ?? '';
  // Weather (ruling 127): the sky over the target, and the groups whose own sky is closed stay on the ground.
  const wx = weatherOn(g);
  const sky: Sky = wx ? (sea ? skyAtSea(g, zone) : skyAt(g, target)) : 'clear';
  const out: AirAssess = { why: null, place, att: [], def: [], E: 0, Epow: 0, B: 0, D: 0, Dpow: 0, radar: null, bomberLoss: 0, defence: 0, drilled: 0, aa: 0, guns: { light: 0, heavy: 0, all: 0 }, standing: 0, squadrons: 0, groups: 0, gunLoss: 0, lines: [], ships: [], flak: 0, spoil: 0, naval: 1, again: 1, surprise: false, sky, aim: cloudAim(sky, night) };
  if (known) out.known = known;
  if (!fleets.length) { out.why = 'No air fleet has these orders.'; return out; }
  if (sky === 'closed') { out.why = `Closed sky this month: nothing flies over ${place}.`; return out; }
  if (m === 'fleet' && (!quarry || !atSea(quarry) || !surfaceShips(quarry))) { out.why = 'That fleet is not at sea.'; return out; }
  if (m === 'fleet') out.ships = [quarry!];
  else if (m === 'harbour') out.ships = shipsInPort(g, target, nat);
  if (m === 'harbour' && !out.ships.length) { out.why = 'No enemy warships lie in harbour there.'; return out; }
  // A harbour raided in the month war is declared on its fleet is caught unready.
  out.surprise = m === 'harbour' && out.ships.some((x) => g.flags[`war:${nat}:${x.nat}`] === g.turn);
  // (Airfields as targets: a field raided in the month war opens between the two is caught unready, as a harbour is.)
  if (m === 'airfield' && g.regions[target]) out.surprise = warOpens(g, nat, g.regions[target].ctrl);
  const unready = out.surprise ? CARRIERS.harbour.unready : 1;
  for (const f of fleets) {
    // (A group on its way is over the leg it flies, and out of the fight only for anything else: ferry.ts.)
    if (!transit && ((wx && grounded(g, f)) || wrecked(g, f))) continue;
    const d = transit ? 0 : sea ? kmToSea(g, f, zone) : kmTo(g, f, target);
    const R = reach(f, g, raidJob(m));
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

  // Units decide: the fighters' own fight is decided by height, which follows the defenders' warning (below). The
  // defenders' weight there leaves out the radar's control, which is that warning, priced once (dPlain).
  let dPlain = 0;
  // Radar, and the fighters that come up to meet the raids.
  if (sea) {
    for (const id of g.seas[zone].coast) {
      const c = radarAt(g, g.regions[id]);
      if (c && atWar(g, nat, c.nat)) { out.radar = c; break; }
    }
  } else out.radar = transit ? null : radarAt(g, g.regions[target]);
  // Who comes up is comesUp()'s (ruling 225): over troops and ships any enemy fighter in reach, over the land itself the
  // groups that guard that ground, and carrier fighters only where their fleet is.
  // A raid flown on an order the defender's agent read (ruling 135(c)): every fighter in reach meets it in full,
  // with nothing lost for distance or for want of radar.
  const amb = !sea && !transit && ambushAt(g, target);
  // City defences (the ruling of 25 Nov 2026 02:23Z, choice 4): searchlights give the night fighters a floor under a
  // clear sky. 0 by day, under cloud, with no lights or with the switch off, and then the line below is as it was.
  // A forecast under fog on raids counts the floor as any forecast does: the design director, 25 Nov 2026 19:44Z, ruled
  // radar and searchlights known truth reads for the joint sight form (audit 28), read as they truly are, never one true
  // and one none. (Not over a group on its way between fields: see the guns by night below.)
  const floor = night && !transit ? lightsFloor(g, target, sky) : 0;
  for (const f of g.airFleets) {
    if (f.ftr <= 0 || f.mission !== 'support' || f.close || !atWar(g, f.nat, nat)) continue;
    // A group on its way to another field is met, as troops are, by any enemy fighter whose reach covers its leg (ferry.ts).
    if (!transit && !comesUp(g, f, m, target, zone)) continue;
    if ((wx && grounded(g, f)) || wrecked(g, f)) continue;
    // (Fog on raids: weighed on one side's knowledge, only the groups it knows of for sure are counted by name.)
    if (known && !known.sure(f)) continue;
    const d = transit ? legKm(g, f, transit.from, target) : sea ? kmToSea(g, f, zone) : kmTo(g, f, target);
    let w = (amb ? (d <= reach(f, g).ftrRange ? AMBUSH() : 0) : cover(d, reach(f, g).ftrRange)) * crowding(g, f);
    // Carrier fighters are at home over the sea, and over their own fleet its lookouts warn them: they need no radar chain.
    const deck = f.ship !== undefined;
    if (sea && !deck) w *= C.seaCover;
    // (With the air campaigns on, the historian's dated row for the defender's nation, gated by Fighter control: b553(b), b558(2).)
    if (night) {
      const k = acOn(g) ? nightFighters(g, f.nat) : C.night.fighters;
      w *= floor > 0 ? Math.max(k, floor) : k;
    }
    if (!amb && !out.radar && !(deck && sea && d <= 0)) w *= AIR.radar.without;
    w *= unready;
    // Under cloud fewer of the defenders find the bombers.
    if (wx) w *= cloudMet(sky);
    if (w <= 0) continue;
    out.def.push({ f, d, w });
    out.D += f.ftr * w;
    // Fighters over their own ground are only as good as the radar and controllers that put them onto the raid.
    out.Dpow += f.ftr * w * worth(g, f) * techMul(g, f.nat, 'radar');
    dPlain += f.ftr * w * worth(g, f);
  }
  // Fog on raids: the fighters last seen in reach, counted as all rising. Nobody knows which groups they are now, so
  // they come up with no loss for distance or a crowded field, at their nation's own quality and fighter research, fit
  // to fly and with no commander named; the night, the want of radar, a surprise and the cloud tell on them as on any.
  let unseenUp = 0;
  if (known && known.extra > 0) {
    let w = 1;
    if (night) w *= acOn(g) ? nightFighters(g, known.extraNat) : C.night.fighters;
    if (!amb && !out.radar) w *= AIR.radar.without;
    w *= unready;
    if (wx) w *= cloudMet(sky);
    const each = reach({ nat: known.extraNat, name: '' }, g).quality * techMul(g, known.extraNat, 'fighters');
    unseenUp = known.extra * w;
    out.D += unseenUp;
    out.Dpow += unseenUp * each * techMul(g, known.extraNat, 'radar');
    dPlain += unseenUp * each;
    if (unseenUp > 0) out.unseen = { n: known.extra, w, each, nat: known.extraNat };
  }
  // The fighters' own types (fighters.ts): a small edge from measured performance, off as shipped.
  // (Fog on raids: with no group known by name there are no types to set against the escort's.)
  if (fighterTypesOn(g) && !night && out.Epow > 0 && out.Dpow > 0 && !(known && !out.def.length)) {
    // Under Units decide each group brings its pilots' hours and its held level, for the pair (fighters.ts pilotsEdge).
    const pilots = (f: AirFleet) => (unitsDecide(g) ? { h: hoursOf(g, f), q: heldOf(g, f) } : {});
    const E = typeEdge(g, out.att.map((x) => ({ f: x.f, w: x.ftr * worth(g, x.f), ...pilots(x.f) })), out.def.map((x) => ({ f: x.f, w: x.f.ftr * x.w * worth(g, x.f), ...pilots(x.f) })), m);
    out.typeEdge = E;
    // The raid card's pilots' line: the escort's mean hours against the defenders', each by the fighters that fight.
    if (lawK(g) > 0) {
      const mean = (xs: { f: AirFleet; w: number }[]): number | null => {
        let s = 0, n = 0;
        for (const x of xs) { if (x.w <= 0) continue; const h = hoursOf(g, x.f); if (h === null) return null; s += x.w * h; n += x.w; }
        return n > 0 ? s / n : null;
      };
      const ours = mean(out.att.map((x) => ({ f: x.f, w: x.ftr }))), theirs = mean(out.def.map((x) => ({ f: x.f, w: x.f.ftr * x.w })));
      if (ours !== null && theirs !== null) out.pilotsLine = `Pilots' training: ours ${Math.round(ours)} hours, theirs ${Math.round(theirs)}.`;
    }
    out.Epow *= Math.exp(E / 2);
    out.Dpow *= Math.exp(-E / 2);
    dPlain *= Math.exp(-E / 2);
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
  // City defences (the design director's ruling of 25 Nov 2026 02:23Z, choices 1, 2 and 5): with the switch on, by
  // night the guns count by what they can see, radar and searchlights, in place of 107(b)'s half (one cause, one price).
  // Under fog on raids the forecast counts the guns the side counts (known.aa), and Fighter control's radar and the
  // searchlights as they truly are: the design director, 25 Nov 2026 19:44Z, ruled both known truth reads for the joint
  // sight form (audit 28), never one true and one none. One level of lights lights one level of the guns so counted.
  // Never over a ferry leg, and rightly so in the design director's terms of 19:44Z: a group on its way between fields
  // meets no city defences "provided a ferry leg never crosses an enemy city's guns. Where it can, those guns reach it."
  // In these rules it cannot: ferry.ts meets it as a fight for the sky by day ('superiority', so `night` is false), and
  // no province's built guns count on that mission (out.aa below), with the switch on or off.
  const cn = night && !transit && cityDefences(g) ? cityNight(g, target, out.B, sky, false, known ? { guns: known.aa, lights: lightsAt(g, target) } : undefined) : null;
  const aaGun = gun * (cn ? cn.share : nightGunShare(g, night));
  if (cn) out.lights = { level: cn.lights, cloud: cn.cloud, lit: cn.litShare };
  // Crews the player's instructors trained keep their formation: the fighters bring down fewer of them (bombers.ts).
  // The guns' share is as it was. 0 until the player has sent a crew to the schools, and always for everyone else.
  out.drilled = out.B > 0 ? out.att.reduce((t, x) => t + x.bmr * trainedEdge(g, x.f), 0) / out.B : 0;
  out.bomberLoss = out.B > 0 ? Math.min(most, (L ? L.bombers * Math.pow(met, L.power) : C.bombers * met) * (1 - out.drilled) * I + (night ? C.night.flak : C.flak) * gun * I) : 0;
  // Guns round the factories bring down more of the bombers that come for them.
  out.aa = m === 'bomb' || night || m === 'harbour' || m === 'town' ? (known ? known.aa : gunsAt(g, target)) : 0;
  if (out.aa && out.B > 0) out.bomberLoss = Math.min(most, out.bomberLoss + out.aa * WORKS.aa.flak * I * unready * aaGun);
  // The same defences as counts: the guns built round a target on land and, apart from them, the light guns every
  // such target has (the plain flak share above), and the squadrons able to meet the raid.
  if (!sea && out.B > 0) { out.guns = flakGuns(out.aa); out.standing = standingGuns(); }
  out.groups = out.def.length;
  out.squadrons = out.def.reduce((n, x) => n + squadronsOf(x.f.nat, x.f.ftr * x.w, turnToDate(g.turn).year), 0);
  if (known && unseenUp > 0) out.squadrons += squadronsOf(known.extraNat, unseenUp, turnToDate(g.turn).year);
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
    const before = Math.max(0, ...out.ships.map((x) => (x.struck && x.struck[0] === g.turn ? x.struck[1] : 0)));
    out.again = Math.pow(CARRIERS.strike.again, before);
    // Ruling 240 (C3): each fleet's damage control, by its own nation's research (tech.ts dcOf: exactly 1 for every
    // fleet while the switch is off, and then the assessment has no such key).
    const tough = out.ships.map((x) => dcOf(g, x.nat));
    if (tough.some((k) => k !== 1)) out.tough = tough;
  }
  // What of the bombers' loss is the guns' doing, for the raid card (the player's ask of 2026-11-16).
  if (out.B > 0) out.gunLoss = Math.min(out.bomberLoss, ((night ? C.night.flak : C.flak) * gun + out.aa * WORKS.aa.flak * unready * aaGun + out.flak) * I);
  // Units decide (the design director, 24 Nov 2026 02:28Z, 15:20Z and 15:33Z; air-battles/dogfight/sweep-height-form-1124.md
  // and escort-height-form-1124.md): by day the attacker's flat edge in the fighters' own fight (escortShare) gives way to
  // height. A sweep picks its height and flies high; defenders warned in time climb above it, half-warned meet it level,
  // unwarned are bounced from above. An escort flies where its role puts it, the close escort at the bombers' height and
  // top cover a band above, and the defenders sit against the bombers by their warning (2W - 1 bands): each group's
  // height over them, one band at most either way, weighed by its fighters, is the escort's. The edge a band is the
  // field's own bounce as measured there (AIR.height.h), so the card and the field agree. The bombers' losses above are
  // not moved by it: what each role saves them is to be measured before it is laid (3.5 for both until then).
  if (unitsDecide(g) && !night && out.Epow > 0 && out.Dpow > 0) {
    const W = warnedIn(g, out, sea, amb, target), h = AIR.height.h;
    const x = out.B <= 0 ? 1 - 2 * W : escortAbove(g, out, W);
    out.height = { W, x, h };
    // Defenders warned in time are above the bombers too, and dive on them: the fighters' share of the bombers' loss
    // moves by what the field measures for a band above them (AIR.height.bombers), not the guns'. The field is armed
    // without it (plain), as it is without the escort's height, so its own dive brings it back and not twice.
    const over = 2 * W - 1;
    if (out.B > 0 && over > 0) {
      const plain = out.bomberLoss, byFighters = Math.max(0, plain - out.gunLoss);
      out.bomberLoss = Math.min(most, out.gunLoss + byFighters * Math.pow(AIR.height.bombers, over));
      out.height.plain = plain;
    }
    // The radar's control is the warning: the defenders' weight in this fight is without it, and their height is with W.
    out.Dpow = dPlain;
    // The exchange goes as the square root of the power ratio: the ratio moves by h^2x, the exchange by h^x.
    out.Epow *= Math.pow(h, x);
    out.Dpow *= Math.pow(h, -x);
  }

  // The Battle Ticket.
  const af = out.att.reduce((s, a) => s + a.f.ftr, 0);
  const ab = out.att.reduce((s, a) => s + (a.bmr > 0 ? a.f.bmr : 0), 0);
  const df = out.def.reduce((s, x) => s + x.f.ftr, 0);
  out.lines.push({
    label: 'Forces',
    // (Fog on raids: weighed on one side's knowledge, only the groups it knows of for sure are named, and the fighters
    // last seen near the place are said as last seen, not as there.)
    value: `${[af ? planes(af, 'fighter') : '', ab ? planes(ab, 'bomber') : ''].filter(Boolean).join(' and ')} against ${known ? `${df ? planes(df, 'fighter') : 'no fighters'} known for sure${unseenUp > 0 ? `; about ${Math.round(known.extra).toLocaleString('en-US')} ${df ? 'more ' : ''}were last seen near it` : ''}` : df ? planes(df, 'fighter') : 'no fighters'}`,
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
      effect: unseenUp > 0
        ? `From ${list(out.def.map((x) => theUnit(x.f.name)))}, and the others last seen near it: about ${Math.round(out.D).toLocaleString('en-US')} fighters may meet the raid.`
        : `From ${list(out.def.map((x) => theUnit(x.f.name)))}: about ${Math.round(out.D).toLocaleString('en-US')} fighters can meet the raid.`,
    });
  } else if (unseenUp > 0) {
    // (Fog on raids: no group known for sure, but the forecast counts the fighters last seen near the place, all rising.)
    out.lines.push({ label: 'In range to intercept', value: `About ${planes(out.squadrons, 'fighter squadron')}, not seen now`, effect: `Reckoned from the fighters last seen near it, all of them rising: about ${Math.round(out.D).toLocaleString('en-US')} fighters may meet the raid.` });
  } else out.lines.push({ label: 'In range to intercept', value: known ? 'No enemy fighters known for sure' : 'No enemy fighters' });
  if (out.height && out.B > 0) {
    // An escorted raid: where the warning puts the defenders against the bombers, and where the escort's roles put it.
    const { W, x } = out.height, hx = Math.pow(out.height.h, Math.abs(x)).toFixed(1);
    const them = W >= 1 ? 'Warned in time, they climb above your bombers' : W > 0 ? 'Warned late, they meet your bombers at their height' : 'Unwarned, they come up at your bombers from below';
    const roles = out.att.filter((a) => a.esc > 0 && Math.round(a.f.ftr * a.crowd) > 0).length > SKY.escort.topFrom ? 'your close escort flies with the bombers and your top cover a band above' : 'your escort flies close, with the bombers';
    const dive = out.height.plain !== undefined ? ` They dive on your bombers as well: about ${AIR.height.bombers.toFixed(1)} times the bombers lost to fighters.` : '';
    // (A quarter of a band or less either way reads as level: about 1.1 to 1, inside the field's own spread.)
    out.lines.push(x <= -0.25
      ? { label: 'Height', value: 'The defenders will be above your escort', effect: `${them}; ${roles}. About ${hx} to 1 their way on height alone.${dive}` }
      : x >= 0.25
        ? { label: 'Height', value: 'Your escort will be above them', effect: `${them}; ${roles}. About ${hx} to 1 your way on height alone.${dive}` }
        : { label: 'Height', value: 'Level', effect: `${them}; ${roles}. Neither side has the height to speak of.${dive}` });
  } else if (out.height) {
    const hx = out.height.h.toFixed(1);
    out.lines.push(out.height.x < 0
      ? { label: 'Height', value: 'The defenders will be above you', effect: `They are warned in time to climb above your fighters and dive on them: about ${hx} to 1 their way on height alone.` }
      : out.height.x > 0
        ? { label: 'Height', value: 'You will be above them', effect: `No warning reaches them in time: your fighters dive on them from above, about ${hx} to 1 your way on height alone.` }
        : { label: 'Height', value: 'Level', effect: 'Ground observers warn them in time to climb to your height, not above it: neither side has the height.' });
  }
  if (out.guns.all || out.standing) {
    // (The UI director, 25 Nov 2026 19:10Z, section B: the built guns first, the standing light guns named apart.)
    const n = (x: number): string => x.toLocaleString('en-US'), S = n(out.standing);
    const split = `${n(out.guns.light)} light and ${n(out.guns.heavy)} heavy, and the ${S} light guns every target has.`;
    // (Her 20:08Z marks, items 1, 2 and 13: under fog the built guns never scouted are reckoned at the worst, and a none
    // is said as seen or as reckoned; on troops the built guns do not count, and the card says why where it knows of some.)
    const only = `only the ${S} light guns every target has.`, Only = `Only the ${S} light guns every target has.`;
    const Seen = (ago: number): string => { const w = seenAgo(ago); return `${w.charAt(0).toUpperCase()}${w.slice(1)}`; };
    const seen = known ? known.seen ?? -1 : -1;
    const counts = m === 'bomb' || night || m === 'harbour' || m === 'town';
    const flakWords = (): string => {
      // (Her 21:22Z marks, case e: the clause, where it knows of guns built there, on the line, an airfield raid and a
      // fight for the sky flown with bombers (they hit its airfields) as on troops.)
      const not = m === 'ground' ? 'troops in the field' : m === 'line' ? 'the line' : m === 'airfield' ? 'the airfield' : m === 'superiority' ? 'the airfields' : '';
      if (!counts) return not && (known ? seen > 0 : gunsAt(g, target) > 0) ? `Only the ${S} light guns every target has: the guns built here guard the town, not ${not}.` : Only;
      if (!out.guns.all) return !known ? Only : seen === 0 ? noneSeen(known.ago ?? 0) : `None reckoned built: ${only}`;
      if (known && seen < 0) return `Not scouted, so reckoned at the worst: about ${n(out.guns.all)} guns built round ${place}, ${split}`;
      // (Her 21:22Z marks, case c: a place seen at a level, the month is the hedge, leading the line: "Seen two months
      // ago: about 720 guns built round Rhineland, ...".)
      return known ? `${Seen(known.ago ?? 0)}: about ${n(out.guns.all)} guns built round ${place}, ${split}` : `About ${n(out.guns.all)} guns built here, ${split}`;
    };
    out.lines.push({
      label: 'Flak',
      // (Fog on raids: weighed on one side's knowledge, the built guns are its reckoning, not a sight of them; the
      // standing light guns every target has are never hidden.)
      value: flakWords(),
      effect: `They may bring down ${Math.round(out.gunLoss * 100) >= 1 ? `about ${Math.round(out.gunLoss * 100)}%` : 'almost none'} of the bombers.`,
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
  // (The Anti-aircraft guns line under fog on raids, the UI director's 21:22Z marks, case c: the month is the hedge, as
  // on the Flak line. Seen at a level: "Level 2 guns ringed the factories of Rhineland when seen two months ago"; never
  // scouted: "Not scouted, so reckoned at the worst: level 3 guns round the factories of Rhineland". In sight, as before.
  // The searchlights' hedge below reads the same `known`.)
  const aaLine = (round: string): string => !known ? `Level ${out.aa} guns ring ${round}`
    : (known.seen ?? -1) < 0 ? `Not scouted, so reckoned at the worst: level ${out.aa} guns round ${round}`
      : `Level ${out.aa} guns ringed ${round} when ${seenAgo(known.ago ?? 0)}`;
  // City defences, a night raid with the switch on: how the guns fire in the dark, and what they bring down against
  // what they would by day (the UI director's words, section 3). Empty otherwise.
  const dark = cn ? `By night they fire ${!cn.radar ? 'blind' : cn.lit > 0 ? 'by radar and searchlight' : 'by radar'}, and bring down about ${Math.round(cn.share * 100)}% as many bombers as they would by day.` : '';
  if (out.aa && m === 'harbour') out.lines.push({ label: 'Anti-aircraft guns', value: aaLine(harbourOf(place)), effect: dark || 'They bring down more of the bombers' });
  if (m === 'town') {
    const r = g.regions[target], gone = breachOf(g, r);
    out.lines.push({ label: 'The town', value: `Troops inside ${townName(r)} fight ${Math.round((townBonus(g, r) - 1) * 100)}% stronger${townWorksPhrase(g, r)}${gone > 0 ? `, with ${Math.round(gone * 100)}% of its works already broken down` : ''}`, effect: `${SIEGE.bombard.raidFull} bombers getting through break down ${SIEGE.breach.raid}% of its works, fewer their share; bombs never break more than ${breachCeiling('raid')}% in all` });
  }
  // (The town's guns name the town, as the card's town line does, in all three forms: the UI director's answer of 25 Nov
  // 2026 22:29Z, question 1, "Level {2} guns ring {Metz}". One card names one place.)
  if (out.aa && m === 'town') out.lines.push({ label: 'Anti-aircraft guns', value: aaLine(townName(g.regions[target])), effect: dark ? `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing. ${dark}` : `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing and bring down more of the bombers` });
  else if (out.aa) out.lines.push({ label: 'Anti-aircraft guns', value: aaLine(`the factories of ${place}`), effect: dark ? `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing. ${dark}` : `They stop ${Math.round(out.aa * WORKS.aa.cut * 100)}% of the bombing and bring down more of the bombers` });
  // City defences: the searchlights' line, a night raid with lights built and the switch on (the UI director's words,
  // section 2). One line, the reading that governs: cloud, then no guns, then no Fighter control, then too many bombers.
  // The clauses that rest on the guns' levels (none, some lit, every level lit) read the side's count under fog on raids,
  // so there they carry the guns' own hedge, "may", under the very condition that makes the guns' line "may ring". The
  // design director, 25 Nov 2026 20:32Z, ruled one footing on one card: the card never states as plain fact what the
  // side has not seen. The words are the UI director's, 20:33Z: the guns are "built here", and counts are in words.
  if (cn && cn.lights > 0) {
    const over = `Level ${cn.lights} searchlights over ${place}`, few = 'they help the night fighters a little.';
    const light = known ? 'may light' : 'light';
    out.lines.push(cn.cloud
      ? { label: 'Searchlights', value: `Cloud over ${place}`, effect: 'The searchlights cannot see the bombers through it.' }
      : !cn.guns ? { label: 'Searchlights', value: over, effect: `${known ? 'There may be' : 'There are'} no guns built here for them to light. They help the night fighters a little.` }
        : !cn.radar ? { label: 'Searchlights', value: over, effect: "Without Fighter control's radar to lay the guns, they help only the night fighters, a little." }
          : cn.sat < 1 ? { label: 'Searchlights', value: 'More bombers than the lights can hold', effect: `About ${Math.round(out.B).toLocaleString('en-US')} bombers over ${place}, and a beam holds one at a time: the lights do about ${Math.round(cn.sat * 100)}% of their work.` }
            : { label: 'Searchlights', value: over, effect: cn.lit >= cn.guns ? `With radar laying the guns, they ${light} every level of the guns built here, and ${few}` : `With radar laying the guns, they ${light} ${upToTen(cn.lit)} of the ${upToTen(cn.guns)} levels of guns built here, and ${few}` });
  }
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

function gainXp(g: Game, f: AirFleet, won: boolean): void {
  // Under Units decide every group seasons, leader or not, in the leader's steps read as a level (#2 form Part 2
  // section 6): a fight survived is worth as many steps as it is to a leader at that level.
  if (unitsDecide(g)) {
    const L = levelOf(fxpOf(g, f));
    f.fxp = Math.min(1, fxpOf(g, f) + (won ? 2 : 1) / (3 * L) / (AIR.leader.maxLevel - 1));
  }
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
/**
 * Airfields as targets: what a raid brings to the field it hits (fieldstate.ts FieldBlow): the bombers through, the
 * escort that comes through, the holder's radar, surprise, and the share of each fighter group based there that went
 * up to meet it (its weight as a defender: the rest were parked).
 */
function fieldBlow(g: Game, a: AirAssess, nat: string, target: number, through: number, strafe: number): FieldBlow {
  const up = new Map<number, number>();
  for (const x of a.def) if (x.f.base === target && x.f.ship === undefined) up.set(x.f.id, Math.min(1, x.w));
  const holder = g.regions[target]?.ctrl ?? '';
  return { through, strafe, radar: a.radar ? a.radar.ground : 1, surprise: warOpens(g, nat, holder), foe: (n) => atWar(g, nat, n), up };
}

/**
 * The sky's figure (air.json sky.win): the attacker's fighting weight over what the defender can put up, with the
 * fight's own type edge (fighters.ts; absent when its switch is off) and, under Units decide, its height; 99 with no
 * defender. The fight for the sky reads it on the defender's losses made good for the month; a raid on troops on the
 * defenders still in the air over the target (skyWon).
 */
function skyHold(a: AirAssess, Epow: number, Dpow: number): number {
  const te = a.typeEdge;
  let hold = Dpow > 0 ? (te ? (Epow * Math.exp(te / 2)) / (Dpow * Math.exp(-te / 2)) : Epow / Dpow) : 99;
  if (a.height && Dpow > 0) hold *= Math.pow(a.height.h, 2 * a.height.x);
  return hold;
}

/**
 * A raid on troops with the strafers' rule on (strafers.ts): whether the raid's side won the sky in the air fight just
 * flown, read on the fighters of both sides as the fight left them (the design director, 16:04Z: "a group goes down only
 * when it is free of the air fight. Settled: its side won the sky (the enemy's fighters beaten or gone)"). The rules'
 * own test of a won sky is the fight for the sky's (fly(), superiority): at least sky.least fighters still over the
 * target and sky.win (1.65) times what the defender can put up, by the same weights, type edge and height. That test
 * reads the defender with his losses made good for the month; over the target, in the raid, nothing is made good, so
 * here it reads the defenders still in the air (their losses not made good). With none left, none having risen or
 * every one that rose lost, the sky is won.
 */
export function skyWon(g: Game, a: AirAssess): boolean {
  let E = 0, Epow = 0, D = 0;
  for (const x of a.att) { const n = x.f.ftr * x.esc * x.crowd; E += n; Epow += n * worth(g, x.f); }
  for (const x of a.def) D += Math.min(x.f.est.ftr, x.f.ftr) * x.w * worth(g, x.f) * (a.height ? 1 : techMul(g, x.f.nat, 'radar'));
  // (Fog on raids: a weighing on one side's knowledge counts the fighters last seen near the target as rising.)
  if (a.unseen) D += a.unseen.n * a.unseen.w * a.unseen.each * (a.height ? 1 : techMul(g, a.unseen.nat, 'radar'));
  return D <= 0 || (E >= AIR.sky.least && skyHold(a, Epow, D) >= AIR.sky.win);
}

export function fly(g: Game, nat: string, m: AirMission, target: number, fleets: AirFleet[], salt = '', battle?: FoughtAir, transit?: Transit): AirReport | null {
  const a = assessAir(g, fleets, m, target, undefined, transit);
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
  const eLoss = C.fighters * I * clash * 2 * (1 - share) * (a.height ? 1 : C.escortShare) * luck();
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
    p.lostFtr += k; poolIn(g, f.nat, back, f); p.lostPilots += k - back;
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
  // (A group caught on its way carries no bombs to drop: ferry.ts.)
  const through = transit ? 0 : bombed * (night ? C.night.accuracy : dayAim(g, nat, m)) * techMul(g, nat, 'bombers') * a.aim;

  // What the bombers did.
  let result = '', value = 0, ground = 0, shipsSunk = 0, notable = false;
  // Damaged cities (citydamage.ts): what the bombs did to the town itself, where they did anything.
  let city: CityHit | null = null;
  // (Release 2, atom.ts) The strike's own line, where this raid carried the atomic bomb.
  let atomLine: { text: string; hit: boolean; tell: boolean } | null = null;
  // Airfields as targets (fieldstate.ts): what the bombs did to a field, where they hit one.
  let fh: FieldHit | null = null;
  // (A raid on troops with the strafers' rule on, strafers.ts: the names of the groups that went down, for the report.)
  let strafed: string[] | undefined;
  const bookGround = (hit: FieldHit): void => {
    for (const x of hit.losses) {
      const f = g.airFleets.find((y) => y.id === x.id);
      let side = def.find((s) => s.id === x.id);
      if (!side && f) { side = before(f); side.ftr += x.ftr; side.bmr += x.bmr; def.push(side); }
      if (side) { side.lostFtr += x.ftr; side.lostBmr += x.bmr; }
    }
  };
  let struck: SeaSide[] | undefined;
  if (transit) result = '';
  else if (m === 'fleet' || m === 'harbour') {
    // (The far side: a raid flown at a harbour is remembered, by who flew it and where, for the news that names it:
    // the entry of December 1941 speaks of Pearl Harbor only where it was raided. Rider b641 (e) to ruling 234.)
    if (m === 'harbour' && farOn(g)) g.flags[`harbour:${nat}:${g.regions[target].name}`] = g.turn;
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
      // (Release 2, the far chart: a strike at anchor is booked on the fleet, with where its strikers flew from as the
      // fleet's side saw them, for its staff's weighing of staying against moving its base: navy.ts anchorLoss.)
      if (m === 'harbour' && farOn(g)) {
        const sunk = shipCount(struck![i].sunk) - struck![i].sunk.ss, had = shipCount(struck![i].ships) - struck![i].ships.ss;
        // (Only the groups that carried bombs struck it: an escort's fighters reach no further than the bombers they guard.)
        const from = a.att.filter((y) => y.bmr > 0).map((y) => {
          const c = carrierOf(g, y.f), R = reach(y.f, g, raidJob(m));
          return c && atSea(c) ? { sea: c.zone, field: -1, km: R.bmrRange } : { sea: -1, field: c ? c.base : y.f.base, km: R.bmrRange };
        });
        const w = a.att.reduce((s, y) => s + y.ftr + y.bmr, 0) * (airWeight(g, nat) ?? 1);
        x.anchor = [...(x.anchor ?? []).filter((e) => e.t >= g.turn - 1), { t: g.turn, at: target, n: sunk, lost: had > 0 ? sunk / had : 0, w, from }];
      }
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
    // A carrier that goes down takes most of its aircraft with it.
    if (total.cv) syncWings(g);
  } else if (m === 'superiority') {
    const A = AIR.damage.airfields;
    const hit = g.airFleets.filter((f) => f.base === target && f.ship === undefined && atWar(g, f.nat, nat));
    const cut = Math.min(A.max, through / A.scale);
    // Airfields as targets (the design director, 05:55Z, answer 5): the sky raid's bombers hit the field through
    // damageField, at its threshold, levels and rate on the ground; the sweep and the readiness cut stay as they were.
    const fieldsOn = g.flags.fields === 1 && fieldRules(g);
    for (const f of hit) {
      f.ready = Math.max(AIR.ready.floor, f.ready - cut);
      if (fieldsOn) { value += cut * 300; continue; }
      const all = f.ftr + f.bmr;
      const n = Math.min(all * A.maxGround, (through * A.ground * (a.radar ? a.radar.ground : 1)) / hit.length);
      const gf = all ? Math.min(f.ftr, rng.roll(n * (f.ftr / all))) : 0, gb = all ? Math.min(f.bmr, rng.roll(n * (f.bmr / all))) : 0;
      const p = airPool(g, f.nat);
      f.ftr -= gf; f.bmr -= gb; f.lost.ftr += gf; f.lost.bmr += gb;
      p.lostFtr += gf; p.lostBmr += gb; poolIn(g, f.nat, gf, f);
      const side = def.find((s) => s.fleet === f.name && s.nat === f.nat);
      if (side) { side.lostFtr += gf; side.lostBmr += gb; }
      ground += gf + gb;
      value += cut * 300;
    }
    if (fieldsOn && through > 0) {
      fh = damageField(g, target, 'sky', fieldBlow(g, a, nat, target, through, 0));
      if (fh) { ground = fh.ground; value += fh.worth; bookGround(fh); notable = fh.closed || fh.levels > 0; }
    }
    result = through <= 0 ? '' : fieldsOn
      ? (hit.length ? `The airfields of ${list(hit.map((f) => f.name))} were bombed: readiness cut by ${Math.round(cut * 100)}%.${fh ? ` ${fh.words}` : ''}` : fh?.words || 'The bombers found no airfields in use.')
      : hit.length
      ? `The airfields of ${list(hit.map((f) => f.name))} were bombed: readiness cut by ${Math.round(cut * 100)}%${ground ? ` and ${planes(ground, 'aircraft').replace('aircrafts', 'aircraft')} destroyed on the ground` : ''}.`
      : 'The bombers found no airfields in use.';
  } else if (m === 'airfield') {
    // Airfields as targets: the bombers go for the runway, the hangars and the fuel; the escort that comes through strafes.
    const strafe = a.att.reduce((s, x, i) => s + Math.max(0, x.ftr - att[i].lostFtr), 0);
    fh = damageField(g, target, 'raid', fieldBlow(g, a, nat, target, through, strafe));
    if (fh) { ground = fh.ground; value += fh.worth; bookGround(fh); notable = fh.closed || fh.levels > 0; result = fh.words; }
    else result = through > 0 ? 'The bombs found no airfield in use.' : 'No bombers got through.';
  } else if ((m === 'bomb' || night) && armedAt(g, nat) === target) {
    // The atomic project (release 2, atom.ts; the design director, 25 November): this raid carries the bomb. The air
    // rules above decided how many bombers reached the release; the bomb is with one of them or lost. Nothing else is
    // done to the city: no ordinary bombing, no raid on its output, no air campaign's count (priced once).
    const hit = strikeFlown(g, nat, target, a.B, bombed);
    atomLine = { text: hit.text, hit: hit.hit, tell: hit.tell };
    // (the raid's after card says it where the line is told; the raid's own line of news reads as any raid, below)
    result = hit.tell ? hit.text : '';
    notable = true;
    city = hit.city;
    if (city) value += city.worth;
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
    // City defences: the night raid's own mark where its bombs fell, for the computer's order of searchlights (switch on only).
    if (night && through > 0) markNightRaid(g, target);
    // The air war: output is weighed by the province's own factories (ruling 227), so one with no industry has none to
    // stop, and the raid is judged on its railway alone.
    value += outputWorth(g, r, cut);
    // (The atomic project, release 2: what the forecast weighed for an enemy's unfinished atomic plants there, where the
    // raiders' side had seen them, is booked as the raid's worth too, so that the card and the result describe one raid.)
    value += atomRaidWorth(g, nat, r, cut);
    // (No line where no bombs fell.)
    result = r.fac > 0 && cut > 0 ? `The bombing stopped ${Math.round(cut * 100)}% of ${r.name}'s output for a month: ${tenthPct(nation)} of ${g.nations[r.ctrl].adj} production.` : '';
    // The grand campaign: a heavy raid wrecks a factory outright, and the bombs that fall on the yards and
    // junctions may cut the province's railway. Both stay as they are until engineers come.
    // Fought out as an air battle, the field's own throws decide both (b397), so the campaign cuts the line the field showed cut.
    if (bombFactories(g, nat, r, cut, battle?.rolls?.ruin)) { value += 300; notable = true; result += ` A factory was wrecked outright: it earns nothing until engineers rebuild it.`; }
    if (bombRail(g, nat, target, through, a.aa ? a.aa * WORKS.aa.cut : 0, a.aa ?? 0, battle?.rolls?.rail)) { value += 300; notable = true; result += ` The railway through ${r.name} is cut: no train runs until engineers relay it.`; }
    else if (!result) result = through > 0 ? `The bombs fell on the rail yards of ${r.name}, but the line still runs.` : 'No bombers got through.';
    // Damaged cities: the bombs that fall on the town leave it damaged until its holder pays to repair it.
    city = cityHit(g, r, 'raid', through, gunsStop(a.aa ?? 0), night);
    if (city) { value += city.worth; result += ` ${city.words}`; }
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
    // Ground fire takes its share of the fighters that come down low. Fought out (release 2), the fighters it brought
    // down are among the field's own losses, booked above; handed to the staff partway, what the field had cost stands
    // (booked above too) and the dice roll only the rest's share of the ground fire.
    const rule = strafeOn();
    // (The strafers' rule, strafers.ts: only the groups with the reach to strafe go down, those of their fighters the air
    // fight left, and only they pay the ground fire; fought out, the field's strafers paid it there.)
    const strafers: number[] = [];
    // (The sky gate, the design director 16:04Z: settled, a group goes down only when its side won the sky in the air
    // fight just flown, skyWon. Fought out, the field decides it group by group. Read before the ground fire, and no die
    // is thrown for it, so every throw of the raid stays where it was.)
    const won = rule && (!battle || staff) ? skyWon(g, a) : true;
    let fired = 0;
    if (rule) a.att.forEach((x, i) => {
      // (The same-key rule, the design director 12:59Z question 4: the raid's own stream is thrown exactly as with the
      // rule off, every group's ground-fire throw drawn and set aside, so no later throw of the raid moves; each group
      // that goes down then throws its own ground fire on a stream keyed by its own id, so a group that no longer throws
      // moves no one else's.)
      if (!battle || staff) {
        const k0 = Math.min(x.f.ftr, rng.roll(Math.max(0, x.ftr * G.fire * luck() * (staff ? staff.rest.ftr : 1))));
        if (k0) rng.roll(k0 * saved(false));
      }
      const left = x.f.ftr * x.esc * x.crowd;
      if (!(won && x.ftr > 0 && canStrafe(g, x)) || left <= 0) return;
      strafers.push(x.f.id);
      if (battle && !staff) return;
      const own = airRng(g, `${m}:${target}:${nat}${salt}:low:${x.f.id}`);
      const k = Math.min(x.f.ftr, own.roll(Math.max(0, left * G.fire * own.range(1 - C.luck, 1 + C.luck) * (staff ? staff.rest.ftr : 1))));
      if (k) {
        const p = airPool(g, x.f.nat), back = own.roll(k * saved(false));
        x.f.ftr -= k; x.f.lost.ftr += k; att[i].lostFtr += k;
        p.lostFtr += k; poolIn(g, x.f.nat, back, x.f); p.lostPilots += k - back;
      }
      fired += k;
    });
    else if (!battle) a.att.forEach((x, i) => down(x.f, att[i], x.ftr * G.fire * luck(), false));
    else if (staff) a.att.forEach((x, i) => {
      const k = Math.min(x.f.ftr, rng.roll(Math.max(0, x.ftr * G.fire * luck() * staff.rest.ftr)));
      if (!k) return;
      // (As down() books a loss, its pilots back through the pool's own door; down() itself would add the field's losses again.)
      const p = airPool(g, x.f.nat), back = rng.roll(k * saved(false));
      x.f.ftr -= k; x.f.lost.ftr += k; att[i].lostFtr += k;
      p.lostFtr += k; poolIn(g, x.f.nat, back, x.f); p.lostPilots += k - back;
    });
    // The fighters that count on the troops: every one that came through; fought out, those that strafed on the field.
    const low = fix?.strafed() ?? a.att.reduce((s, x) => s + (!rule || strafers.includes(x.f.id) ? x.f.ftr * x.esc * x.crowd : 0), 0);
    const cut = troopCut(r, foes, (through + low * G.fighters) * troopWeight(g, nat));
    if (rule) {
      const route = battle && !staff ? 'fought' : 'settled';
      strafeWatch.tell?.({ route, target, nat, strafers: route === 'fought' ? [...(battle?.strafers ?? [])] : strafers, fire: route === 'fought' ? battle?.strafeFire ?? 0 : fired, weight: low, cut });
      // (The words, the UI director 15:04Z: the report says who went down, by the same ids the hook hears, named as
      // the report names its groups and in the raid's order. Nothing is drawn: a record only.)
      const down = route === 'fought' ? battle?.strafers ?? [] : strafers;
      strafed = a.att.flatMap((x, i) => (down.includes(x.f.id) ? [att[i].fleet] : []));
    }
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
  if (m === 'superiority' && !transit) {
    const Eafter = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd, 0);
    const EpowAfter = a.att.reduce((s, x) => s + x.f.ftr * x.esc * x.crowd * worth(g, x.f), 0);
    // (The far side: no losses are made good on a field that is not fed, as none will be when its country's month
    // opens (airUpkeep): such a group counts at what it has, and the reserve's aircraft go to the groups that are fed.)
    const fedNow = a.def.map((x) => fieldFed(g, x.f));
    const lostBy: Record<string, number> = {};
    def.forEach((s, i) => { if (fedNow[i]) lostBy[s.nat] = (lostBy[s.nat] ?? 0) + s.lostFtr; });
    let Dnext = 0;
    a.def.forEach((x, i) => {
      const p = airPool(g, x.f.nat);
      const built = row(AIR.production[x.f.nat], g.turn + 1), trained = row(AIR.pilots.perMonth[x.f.nat], g.turn + 1);
      const can = Math.min(p.ftr + (built ? built[1] : 0), p.pilots + (trained ? trained[1] : 0));
      const lost = lostBy[x.f.nat] ?? 0;
      const back = fedNow[i] && lost > 0 ? Math.min(lost, can) * (def[i].lostFtr / lost) : 0;
      Dnext += Math.min(x.f.est.ftr, x.f.ftr + back) * x.w * worth(g, x.f) * (a.height ? 1 : techMul(g, x.f.nat, 'radar'));
    });
    hold = skyHold(a, EpowAfter, Dnext);
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
  for (const x of a.att) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - (night ? AIR.ready.nightSortie : AIR.ready.sortie)); gainXp(g, x.f, won); }
  for (const x of a.def) { x.f.ready = Math.max(AIR.ready.floor, x.f.ready - AIR.ready.defend * I * (night ? 0.2 : 1)); gainXp(g, x.f, !won); }
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
  if (fh) rep.field = { region: fh.region, shut: fh.shut, levels: fh.levels, ground: fh.ground };
  if (transit) rep.ferry = transit.to;
  if (strafed) rep.strafed = strafed;
  // City defences: the searchlights' reading over a night raid, only with the switch on (absent otherwise).
  if (a.lights) rep.lights = { ...a.lights };
  g.airReports.push(rep);
  storyAir(g, m === 'fleet' ? 'strike' : m === 'harbour' || m === 'town' || m === 'airfield' ? 'bomb' : m, sea ? zone : target, a.att.map((x) => x.f), a.def.map((x) => x.f), { sky, sunk: shipsSunk, attLost: aLost, defLost: dLost, guns, ...(battle?.fell ? { fallen: battle.fell, shot: battle.shot } : {}) });
  // (Release 2, earned names for air groups under the switch 'nicknames', airnicks.ts: the missions completed and the
  // hard fights, read from what the raid has just recorded. Off, nothing is counted or written.)
  if (nicknamesOn(g)) airRaidFlown(g, m, { odds: rep.odds, bombers: a.B, bombed, E: a.E, D: a.D, sky: sky || held }, a.att.map((x, i) => ({ f: x.f, side: att[i] })), a.def.map((x, i) => ({ f: x.f, side: def[i] })), sea ? -1 : target);

  // (Release 2, atom.ts; the UI director under ruling 230 4e) A raid that carried the atomic bomb reads as any raid in
  // its own line; the strike is told in a line of its own, plain, the same whoever struck whom.
  if (atomLine) result = '';
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
  if (transit) text = `${who}, on ${a.att.length === 1 ? 'its' : 'their'} way to ${g.regions[transit.to]?.name ?? 'another airfield'}, ${a.att.length === 1 ? 'is' : 'are'} met by ${adj} fighters near ${a.place}: ${dLost} ${adj} aircraft destroyed, ${cost}.`;
  else if (m === 'superiority') text = `${who} ${a.att.length === 1 ? 'fights' : 'fight'} for the sky over ${a.place}: ${dLost} ${adj} aircraft destroyed, ${cost}. ${sky ? (together ? 'The Allied air forces now hold the sky there.' : `The ${g.nations[nat].adj} air force now holds the sky there.`) : held ? (nat === g.human ? 'The sky there is still yours.' : `The ${g.nations[nat].adj} air force still holds the sky there.`) : df(def) ? 'The defenders still hold their sky.' : 'No fighters came up to meet them.'}`;
  else if (m === 'fleet') text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} the ${a.ships[0].name} ${seaAt}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'harbour') text = `${who} ${a.att.length === 1 ? 'raids' : 'raid'} ${harbourOf(a.place)}${a.surprise ? ' without warning' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'airfield') text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} the airfield at ${a.place}${a.surprise ? ' without warning' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} aircraft destroyed` : ''}.`;
  else if (m === 'town') text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} the defences of ${townName(g.regions[target])}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (sea) text = `${who} ${a.att.length === 1 ? 'strikes' : 'strike'} at shipping ${seaAt}. ${result} ${cap(cost)}.`;
  else if (m === 'line') text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} the ${adj} fieldworks at ${a.place}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else if (m === 'ground') text = `${who} ${a.att.length === 1 ? 'raids' : 'raid'} the ${adj} troops at ${a.place}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  else text = `${who} ${a.att.length === 1 ? 'bombs' : 'bomb'} ${a.place}${night ? ' by night' : ''}. ${result} ${cap(cost)}${dLost ? `, ${dLost} ${adj} fighters shot down` : ''}.`;
  // A fight for the sky in which nobody met anybody is not news (the grand campaign).
  if (g.flags.open === 1 && m === 'superiority' && !sky && aLost + dLost === 0) return rep;
  // What changes something is told in full: the sky won, a ship sunk, a factory wrecked, a railway cut, a fleet or
  // a harbour struck. The same raid on the same place month after month goes into the month's summary line.
  const full = !!transit || sky || notable || shipsSunk > 0 || m === 'fleet' || m === 'harbour';
  const brief = m === 'superiority' ? `a fight for the sky over ${a.place}`
    : m === 'town' ? `the defences of ${townName(g.regions[target])} bombed`
    : m === 'airfield' ? `the airfield at ${a.place} bombed`
    : sea ? `shipping struck ${seaAt}`
    : m === 'line' ? `the ${adj} fieldworks at ${a.place} bombed`
    : m === 'ground' ? `the ${adj} troops at ${a.place} raided`
    : `${a.place} bombed${night ? ' by night' : ''}`;
  // Who had the better of it, for an interface that sets the news out as won and lost (Version 80, display only).
  const fought = { arm: 'air' as const, ...(defNat && !atomLine?.hit ? (won ? { won: nat, lost: defNat } : { won: defNat, lost: nat }) : {}) };
  if (atomLine) text = text.replace('.  ', '. ');
  if (full) headline(g, 'battle', text, nat, sea ? undefined : target, fought);
  else routineNews(g, `air:${nat}:${m}:${sea ? `z${zone}` : target}`, 'battle', text, `${brief}${aLost ? ` (${aLost} aircraft lost)` : ''}`, nat, sea ? undefined : target, fought);
  if (atomLine?.tell) headline(g, 'event', atomLine.text, nat, target);
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
 * 'skies' are the shores its landing plan still owes a landing on, where the plan holds the sky (planSky; none but
 * where ai.ts names them).
 */
export function airAI(g: Game, nat: string, ph?: PlanPhase | null, skies?: number[]): void {
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
  // Fog on raids (airfog.ts, its switch on): the staff notes what it sees of the enemy's air defence as its month opens.
  if (raidFogOn(g)) rememberSkies(g, nat);
  // In the grand campaign history's air plan does not apply: the air groups patrol, and the staff plans raids of its own.
  // Ships come first: the carriers' wings, and a few of the land bombers, go for the fleets the staff has found.
  // (A landing plan holds the sky it lands under before anything the staff decides this month is flown: rider b631 (iv)
  // to ruling 234. An order an agent read last month is flown before it, as written: a stolen order is never false.)
  // (Release 2, the sea patrol: with Units decide and the patrol live and laid, the patrol staff runs after the raid
  // staff and weighs each bomber group's patrol over the seas of its side's convoys against the raid that staff chose
  // for it, at that raid's own score, before the raid flies; then every group left on support against nothing (the
  // design director's ruling of 24 November 2026 17:49Z, 2 (a)). Nothing is read otherwise.)
  if (g.flags.open === 1) {
    flyReadOrders(g, nat, mine);
    const plan = skies ? planSky(g, nat, mine, skies) : null;
    seaStaff(g, nat, nat === g.human ? 'land' : 'all', plan?.named);
    const patrol = patrolOn(g);
    raidStaff(g, nat, mine, ph, plan, patrol ? (f, v) => patrolStaff(g, nat, mine, { f, v }) : undefined);
    if (patrol) patrolStaff(g, nat, mine);
    return;
  }
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
 * So do the groups a landing plan has named for its sky ('held'; planSky).
 */
function baseByThreat(g: Game, nat: string, mine: AirFleet[], beaches: number[], battles: number[] = [], held?: Set<number>): void {
  const K = AIRWAR.ai.basing;
  const own = (r: Region): boolean => r.ctrl === nat && r.owner === nat;
  // The places to guard: the nation's own troops at the front (beside ground an enemy holds), and its own cities and works.
  const troops = new Set<number>();
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort' && !troops.has(c.reg) && g.regions[c.reg].nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) troops.add(c.reg);
  const cities = g.regions.filter((r) => own(r) && (r.fac > 0 || r.vp > 0)).map((r) => r.id);
  // (The fight back: the places the enemy raided since last month ask for fighters besides, by the aircraft he sent,
  // so a staff's fighters go where the raids are coming.)
  const raided = new Map<number, number>();
  const FB = fightBack(g, nat) ? AIRWAR.ai.back! : null;
  if (FB) {
    for (let i = g.airReports.length - 1; i >= 0 && g.airReports[i].turn >= g.turn - 1; i--) {
      const r = g.airReports[i];
      if (r.region < 0 || !g.regions[r.region] || g.regions[r.region].ctrl !== nat || !(r.defNat === nat || allied(g, r.defNat, nat)) || !atWar(g, nat, r.attNat)) continue;
      const sent = r.att.reduce((s, x) => s + x.ftr + x.bmr, 0) * (airWeight(g, r.attNat) ?? 1);
      raided.set(r.region, (raided.get(r.region) ?? 0) + sent * FB.raided);
    }
  }
  // (Release 2, the far chart: a harbour where a fleet struck at anchor stays, none being out of its strikers' reach, has
  // the fighters first: it asks for its strikers' weight at the margin that holds the sky. navy.ts withdraws marks it;
  // the design director's ruling of 04:33Z, 25 November 2026, 2. Nothing is marked off the far chart.)
  const struck = new Map<number, number>();
  for (const f of g.fleets) {
    if (f.nat !== nat || f.base < 0 || atSea(f) || !((g.flags[`anc:${f.id}`] ?? -9) >= g.turn - 1)) continue;
    const w = (f.anchor ?? []).filter((e) => e.t >= g.turn - 2 && e.at === f.base).reduce((s, e) => s + e.w, 0);
    if (w > 0) struck.set(f.base, Math.max(struck.get(f.base) ?? 0, w));
  }
  const places = [...new Set([...troops, ...cities, ...raided.keys(), ...struck.keys()])];
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
  for (const [id, w] of raided) ask[at.get(id)!] += w;
  // The troops fighting the battles of the staff's main effort ask for the sky there (b447): the margin over the
  // enemy aircraft that can reach them that holds it by numbers (airwar.json sky 'own'), not their bare share.
  if (battles.length) for (const id of troops) if (battles.includes(id) || g.regions[id].nb.some((j) => battles.includes(j))) ask[at.get(id)!] *= Math.max(1, AIRWAR.sky.own);
  for (const [id, w] of struck) ask[at.get(id)!] += w * Math.max(1, AIRWAR.sky.own);
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
  const stays = (f: AirFleet): boolean => f.way !== undefined || f.moved === g.turn || f.flew === g.turn || f.mission !== 'support' || posted(g, f) || watching(g, f) || !!held?.has(f.id) || beaches.some((t) => kmTo(g, f, t) <= reach(f, g).ftrRange);
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
  // (A group on its way by stages, ferry.ts, counts where it is going.)
  for (const f of fighters) if (stays(f)) meets(f, f.way?.to ?? f.base, true);
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
    // With the switch 'ferry' (ferry.ts) a field across an ocean is weighed too, as one beyond a month's flight: the
    // group goes there by stages, or crated by sea, by the player's own order. Off, nothing across an ocean is weighed.
    const ferry = ferryOn(g);
    for (const r of open) {
      if (r.id === f.base || (overOcean(g, f, r) && !ferry) || !fieldRoom(g, r.id, f)) continue;
      const near = km(g, f.base, r.id) <= AIR.ferry && !overOcean(g, f, r);
      // (Ruling 52(a): a field beyond a month's flight is weighed too, to be reached by stages. Only for a group out of the fight.)
      if (!near && (P <= 0 || engaged)) continue;
      if (engaged && foeAt(r.id) > R) continue;
      const v = meets(f, r.id, false);
      if (near) { if (!best || v > best.v) best = { r, v }; } else if (!far || v > far.v) far = { r, v };
    }
    const need = here * K.better + K.least * f.ftr;
    if (best && best.v > need && rebaseAir(g, f, best.r)) { meets(f, best.r.id, true); continue; }
    // A spare group sent where it is asked for by stages, with the player's order (ferry.ts): the same search and warning.
    if (far && far.v > need && ferry && staffFerry(g, f, far.r)) { meets(f, far.r.id, true); continue; }
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
function closeStaff(g: Game, mine: AirFleet[], battles: number[], back = false): void {
  if (!((back && closeRules(g)) || closeStaffRules(g)) || !battles.length) return;
  const land = mine.filter((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined && !afloat(f));
  let left = (back ? AIRWAR.ai.back!.close : CLOSE.ai.share) * land.reduce((s, f) => s + f.ftr, 0);
  const near = (f: AirFleet): number => Math.min(...battles.map((b) => kmTo(g, f, b)));
  const can = land
    .filter((f) => f.mission === 'support' && f.flew !== g.turn && f.ready >= CLOSE.least && battles.some((b) => cover(kmTo(g, f, b), reach(f, g).ftrRange) > 0))
    .map((f) => ({ f, d: near(f) }))
    .sort((a, b) => a.d - b.d || a.f.id - b.f.id);
  // (The fight back: a staff defending its own ground also brings groups up from beyond reach of its battles, each to
  // the open field within a month's flight that covers most of them, after the groups already in reach. Over every
  // battle at least as many fighters stay on patrol as go on ground support: the sky is not given up for the ground.)
  if (back) {
    const ftrR = (f: AirFleet): number => reach(f, g).ftrRange;
    const over = (f: AirFleet, base: number): number[] => battles.filter((b) => cover(km(g, base, b), ftrR(f)) > 0);
    const patrol = new Map<number, number>(), onCall = new Map<number, number>();
    for (const f of land) if (f.mission === 'support' && !f.close) for (const b of over(f, f.base)) patrol.set(b, (patrol.get(b) ?? 0) + f.ftr);
    const put = (f: AirFleet, to: Region | null): void => {
      if (f.ftr > left) return;
      const bs = over(f, to ? to.id : f.base);
      const stays = (b: number): number => (patrol.get(b) ?? 0) - (to ? 0 : f.ftr);
      if (!bs.length || bs.some((b) => stays(b) < (onCall.get(b) ?? 0) + f.ftr)) return;
      if (to && !rebaseAir(g, f, to)) return;
      if (!to) for (const b of over(f, f.base)) patrol.set(b, (patrol.get(b) ?? 0) - f.ftr);
      for (const b of bs) onCall.set(b, (onCall.get(b) ?? 0) + f.ftr);
      f.close = true;
      delete f.escorts;
      left -= f.ftr;
    };
    for (const { f } of can) put(f, null);
    // (Only groups that could still go are looked at for a field: the share left shrinks and never grows.)
    const far: { f: AirFleet; to: Region; d: number }[] = [];
    for (const f of land) {
      if (left <= 0) break;
      if (f.ftr > left || f.close || f.mission !== 'support' || f.flew === g.turn || f.moved === g.turn || f.ready < CLOSE.least || can.some((x) => x.f === f)) continue;
      let to: Region | null = null, bv = 0;
      for (const r of airBases(g, f)) { if (overOcean(g, f, r) || !fieldRoom(g, r.id, f)) continue; const v = over(f, r.id).length; if (v > bv) { bv = v; to = r; } }
      if (to) far.push({ f, to, d: near(f) });
    }
    far.sort((a, b) => a.d - b.d || a.f.id - b.f.id);
    for (const { f, to } of far) put(f, to);
    return;
  }
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
  const bombers = mine.filter((f) => f.kind !== 'ftr' && f.bmr > 0 && !afloat(f) && f.moved !== g.turn && f.flew !== g.turn && f.mission === 'support' && !posted(g, f) && !watching(g, f));
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
 * Fighter groups brought forward for a beach a staff means to land on: with no fighter group in reach of it, one
 * based overseas is brought up to the friendly airfield nearest the beach. (A player's air staff, for the crossing
 * he has named, 'BP': it brings up to 'monthly' fighter groups forward each month until 'groups' of them can reach
 * the beach or the airfields in reach of it are full, from the homeland too while more than 'home' groups still
 * guard it.) A group in 'held' is not moved.
 */
function callUp(g: Game, nat: string, mine: AirFleet[], t: number, BP: typeof AIRWAR.ai.raids.beach, held?: Set<number>): void {
  const roomOn = (base: number, f: AirFleet): boolean => {
    const there = g.airFleets.reduce((s, x) => s + (x.base === base && x.ship === undefined ? x.ftr + x.bmr : 0), 0);
    return there + f.ftr + f.bmr <= (airWar(g) ? Math.max(AIRWAR.fields.least, airfieldsHold(g, base)) : airfieldsHold(g, base));
  };
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
      if (f.moved === g.turn || f.flew === g.turn || held?.has(f.id) || (guards(f) && !spare)) continue;
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

/** What a landing plan has of the air this month: the shores whose sky it holds, and the groups it has named for them. */
export interface PlanSky { shores: number[]; named: Set<number> }

/**
 * For the measuring harness and the tests: told of each sky a landing plan holds this month. 'to' is where its fight
 * goes (the enemy field that denies several of its shores, or the shore itself), 'shores' the plan's shores it is for,
 * 'named' the groups the plan holds for them and 'flew' those that went up. In a month the shore is open 'to' is the
 * shore and nothing is flown. Nothing in the game listens.
 */
export const skyWatch: { tell: ((e: { nat: string; to: Region; shores: Region[]; named: AirFleet[]; flew: AirFleet[]; report: AirReport | null }) => void) | null } = { tell: null };

/** The month a shore became one of its country's plan's skies (planSky): kept until no landing is owed on it. */
export const planSkyKey = (nat: string, shore: number): string => `sky:plan:${nat}:${shore}`;

/**
 * The plan holds the sky it lands under (rider b631 (iv) to ruling 234, with the design director's notes of 12:41Z,
 * 13:57Z, 14:11Z and 14:38Z, 19 November 2026). 'skies' are the shores a staff's landing plan still owes a landing on, in
 * the plan's order (ai.ts, planSkies).
 *   its skies  a shore becomes one of the plan's skies in the month enemy fighters deny the sky over it, and stays
 *              one until no landing is owed on it, whether or not the sky is denied in a later month.
 *   named      every land-based group of the country in reach of such a shore, as it stands that month, and in a
 *              month of fighting those in reach of the field the fight goes over. The groups named are the plan's
 *              for the month: the basing rules do not move them, and they fly nothing else. In a month the sky is
 *              open they stand where they are, in reach. Carrier wings stay the navy's.
 *   the fight  in a month the sky is denied, first of all the staff decides, before its strikes and raids: the
 *              named groups that are fit to fly (the raids' readiness, and not flown this month) go up together,
 *              whatever the forecast. A plan does not wait to be sure of the sky it lands under; its landings still
 *              read their own odds before they sail. It is a fight for the sky, so it goes with a fighter group in
 *              it: bombers with none in reach wait for the one brought up.
 *   where      clearing a sky is beating the fighters that deny it. Where one enemy field's fighters are by
 *              themselves enough to deny several of the plan's shores and a bomber group of the plan's reaches
 *              that field, the fight goes over the field, so that the bombers strike its groups on the ground, and
 *              the one fight is for every shore the field denies. With no bomber group that reaches the field
 *              there is no reason to go there: the fight goes over the shore, the first in the plan's order, with
 *              every group that reaches it. So it does where no fighter group can reach the field or be brought
 *              up to: a fight for the sky is not flown by bombers alone.
 *   brought up with no fighter group in reach of where the fight goes, one is brought up, as for a hop (callUp).
 * A group in reach of two of the plan's skies is the first one's, in the plan's order. A sky the month's earlier
 * fights have cleared is not fought for again that month. Null where the plan holds no sky this month.
 */
function planSky(g: Game, nat: string, mine: AirFleet[], skies: number[]): PlanSky | null {
  const count = `sky:plan:${nat}`;
  if (!airWar(g) || (!skies.length && !g.flags[count])) return null;
  const R = AIRWAR.ai.raids, least = AIR.sky.least;
  const owed = skies.filter((t, i) => skies.indexOf(t) === i && !!g.regions[t] && atWar(g, nat, g.regions[t].ctrl));
  // A shore no landing is owed on any more is no longer the plan's.
  if (g.flags[count]) for (const k of Object.keys(g.flags)) if (k.startsWith(`${count}:`) && !owed.includes(Number(k.slice(count.length + 1)))) delete g.flags[k];
  for (const t of owed) if (g.flags[planSkyKey(nat, t)] === undefined && skyDenies(g, nat, t)) g.flags[planSkyKey(nat, t)] = g.turn;
  const shores = owed.filter((t) => g.flags[planSkyKey(nat, t)] !== undefined);
  if (shores.length) g.flags[count] = shores.length; else delete g.flags[count];
  if (!shores.length) return null;
  const named = new Set<number>();
  const reaches = (f: AirFleet, t: number): boolean => whyNotMission(g, f, 'superiority', t) === null;
  const land = (): AirFleet[] => mine.filter((f) => !afloat(f) && !named.has(f.id));
  const from = new Map<number, Map<number, number>>();
  const over = (t: number): Map<number, number> => { let m = from.get(t); if (!m) { m = fightersOverFrom(g, t, nat); from.set(t, m); } return m; };
  const done = new Set<number>();
  let flown = 0;
  for (const s of shores) {
    if (done.has(s)) continue;
    done.add(s);
    if (!skyDenies(g, nat, s)) {
      // An open month: its groups stand in reach, the plan's still.
      const groups = land().filter((f) => reaches(f, s));
      for (const f of groups) named.add(f.id);
      skyWatch.tell?.({ nat, to: g.regions[s], shores: [g.regions[s]], named: groups, flew: [], report: null });
      continue;
    }
    // The enemy field that by itself denies this shore and another of the plan's that is denied this month: the
    // one that denies most of them, then the one with most fighters over this shore.
    from.clear();
    const denied = shores.filter((t) => t === s || (!done.has(t) && skyDenies(g, nat, t)));
    let field = -1, most = 1, v = 0;
    for (const [b, n] of over(s)) {
      if (n < least || !atWar(g, nat, g.regions[b].ctrl)) continue;
      const k = denied.filter((t) => (over(t).get(b) ?? 0) >= least).length;
      if (k > most || (k === most && k > 1 && (n > v || (n === v && b < field)))) { field = b; most = k; v = n; }
    }
    // (The field is in reach when the plan's bombers reach it: the fight goes there for them, 14:38Z.)
    const bombed = field >= 0 && land().some((f) => f.bmr > 0 && reaches(f, field) && kmTo(g, f, field) <= reach(f, g).bmrRange);
    let to = s;
    if (bombed && field !== s) {
      callUp(g, nat, mine, field, undefined, named);
      if (land().some((f) => f.kind === 'ftr' && f.ftr > 0 && reaches(f, field))) to = field;
    }
    if (to === s) callUp(g, nat, mine, s, undefined, named);
    const its = bombed && to === field ? denied.filter((t) => (over(t).get(field) ?? 0) >= least) : [s];
    for (const t of its) done.add(t);
    const groups = land().filter((f) => reaches(f, to) || its.some((t) => reaches(f, t)));
    for (const f of groups) named.add(f.id);
    const fleets = groups.filter((f) => f.flew !== g.turn && f.mission === 'support' && f.ready >= R.ready && reaches(f, to));
    const goes = fleets.some((f) => f.kind === 'ftr' && f.ftr > 0) && !assessAir(g, fleets, 'superiority', to).why;
    const rep = goes ? fly(g, nat, 'superiority', to, fleets, `:sky${flown++}`) : null;
    if (rep) for (const f of fleets) { f.mission = 'superiority'; f.target = to; f.flew = g.turn; }
    skyWatch.tell?.({ nat, to: g.regions[to], shores: its.map((t) => g.regions[t]), named: groups, flew: rep ? fleets : [], report: rep });
  }
  return { shores, named };
}

/**
 * The grand campaign: a computer nation's air staff plans raids of its own and flies them at once, as the player's
 * are flown. Each month a share of its bomber groups goes out, each with the fighter groups on its own airfield as
 * escort by day: against the factories and railway junctions of its enemies, by night where the day would cost too
 * much, or against the enemy armies that face its own. Its fighters may also fight for the sky over a stretch of
 * front where they expect to win it. A raid is flown only where the forecast says it should pay and the bombers'
 * losses stay within what the staff will bear. A rival on the harder levels sends more, bears more, and goes for
 * the player first; on the easier levels it sends fewer. Where the sea patrol is live and laid, 'patrol' is its staff's
 * look at each bomber raid chosen, before it flies (seapatrol.ts patrolStaff): true where it takes the raid's bomber
 * group onto patrol in its stead.
 */
/**
 * A computer staff's raid rules: airwar.json ai.raids, with the level's own (difficulty.json). Where the staff fights back
 * (airwar.json ai.back) it raids 'more' times as often at 'risk' more bomber losses, takes a forecast 'success' and a gain
 * of 'worth' aircraft, and flies up to 'sweeps' fights for the sky at 'sky' times the odds that win one; each level keeps
 * its own margin over the one below.
 */
/** True while a country fights on its own soil: an enemy holds ground it owns, or attacked it on its own ground since last month. */
function invaded(g: Game, nat: string): boolean {
  if (g.regions.some((r) => r.owner === nat && r.ctrl !== nat && atWar(g, nat, r.ctrl))) return true;
  for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn >= g.turn - 1; i--) {
    const b = g.reports[i];
    if (!b.yielded && b.defNat === nat && g.regions[b.region]?.owner === nat && atWar(g, nat, b.attNat)) return true;
  }
  return false;
}

function staffRules(g: Game, nat: string) {
  const FB = fightBack(g, nat) ? AIRWAR.ai.back! : null;
  const R0 = AIRWAR.ai.raids;
  const R = FB ? { ...R0, success: FB.success, worth: FB.worth, sweeps: FB.sweeps, sky: FB.sky } : R0;
  const B = bombingFor(g, nat);
  const base = B ? { ...R, ...B } : { ...R, share: R.share * raidsMul(g, nat) };
  // (A country with the enemy on its own soil takes 'invaded' times the risk: it throws in what it has.)
  const risk = FB ? FB.risk * (invaded(g, nat) ? FB.invaded : 1) : 0;
  // (A rival at a level that gives its air staff rules of its own, General and up, keeps the level's share and least:
  // the level the player chose sets how often and how heavily it comes. The fight back adds only its risk.)
  const more = FB && !B ? { share: Math.min(1, base.share * FB.more), least: Math.min(base.least, FB.least) } : {};
  return { FB, R, B, rules: FB ? { ...base, ...more, loss: base.loss + risk } : base };
}

function raidStaff(g: Game, nat: string, mine: AirFleet[], ph?: PlanPhase | null, plan?: PlanSky | null, patrol?: (f: AirFleet, v: number) => boolean): void {
  if (!airWar(g)) return;
  const { FB, R, B, rules } = staffRules(g, nat);
  const you = B ? seat(g) : null;
  const rng = airRng(g, `staff:${nat}`);
  const used = new Set<number>();
  // (A group on ground support flies over the army's battles, not on raids or sweeps. Nor does a group a landing
  // plan has named for its sky fly anything else this month: planSky.)
  const fit = (f: AirFleet): boolean => !used.has(f.id) && f.flew !== g.turn && f.mission === 'support' && !f.close && !plan?.named.has(f.id);
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
  // flown only for an offensive of the staff's own, to give its army the air where it attacks. (Where the staff fights
  // back, airwar.json ai.back, also over a stretch of front where the armies have fought since last month.)
  // So is the sky over a beach the staff means to land on (the second front, ai.ts): no invasion fleet crosses
  // under the enemy's fighters. With no fighter group in reach of it, one based overseas is brought up to the
  // friendly airfield nearest the beach.
  // (A player who has named his crossing and handed over the air war has the sky over his beach fought for whenever it is not his: it keeps the enemy's ships and aircraft off his troopships.)
  const wanted = (t: number): boolean => { if (skyDenies(g, nat, t)) return true; const sky = skyOver(g, t); return nat === g.human && !(sky !== null && allied(g, sky, nat)); };
  // (The Pacific's hops are beaches too, each named a month ahead: pacific.ts. None while the far side is off.)
  const beaches = [...['', ':o'].map((s) => (g.flags[`st:aim:${nat}${s}`] ?? 0) - 1), ...hopAims(g, nat).map((h) => h.to.id)].filter((t, i, a) => t >= 0 && a.indexOf(t) === i && !!g.regions[t] && atWar(g, nat, g.regions[t].ctrl) && wanted(t));
  // (A player's air staff, for the crossing he has named: it brings up to 'monthly' fighter groups forward each
  // month until 'groups' of them can reach the beach or the airfields in reach of it are full, from the homeland
  // too while more than 'home' groups still guard it.)
  const BP = nat === g.human ? R.beach : undefined;
  for (const t of beaches) callUp(g, nat, mine, t, BP, plan?.named);
  // The rest of the fighters are based by the threat to the nation's troops and cities (ruling 227, b428): the beaches
  // have first call on them, and a group in reach of a beach stays. The battles of the staff's main effort ask for
  // the sky, and its bombers go in reach of them (b447).
  const battles = effortBattles(g, nat, ph);
  // (The fight back: the battles it is defending ask for its fighters' cover, its bombers and its ground support as
  // well as its own offensive's.)
  const held = FB ? defendBattles(g, nat) : [];
  const fights = [...new Set([...battles, ...held])];
  // (The groups a landing plan has named for its sky are not moved, and are not put on ground support: planSky.)
  const free = plan ? mine.filter((f) => !plan.named.has(f.id)) : mine;
  baseByThreat(g, nat, mine, beaches, fights, plan?.named);
  baseBombers(g, nat, free, fights);
  if (ph?.posture === 'attack' || held.length) closeStaff(g, free, [...new Set([...(ph?.posture === 'attack' ? battles : []), ...held])], !!FB);
  let flown = 0;
  for (let k = 0; k < R.sweeps && (front.size || beaches.length); k++) {
    let best: { t: number; fleets: AirFleet[]; v: number } | null = null;
    const bases = [...new Set(mine.filter((f) => fit(f) && f.kind === 'ftr' && f.ftr > 0 && f.ready >= R.ready).map(homeOf))];
    const spots = [...beaches.filter((t) => !hit.has(t)).map((t): [number, number] => [t, 6]), ...[...front.entries()].filter(([t]) => !hit.has(t) && (pushed(t) || (!!FB && fought.has(t))) && skyOver(g, t) === null).sort((a, b) => b[1] - a[1] || a[0] - b[0])].slice(0, R.look);
    for (const base of bases) {
      for (const [t, divs] of spots) {
        const fleets = fightersAt(base).filter((f) => f.ready >= R.ready && whyNotMission(g, f, 'superiority', t) === null);
        if (!fleets.length) continue;
        const { a, fc } = staffWeigh(g, nat, fleets, 'superiority', t);
        if (!fc || a.E < AIR.sky.least) continue;
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
      const { a, fc } = staffWeigh(g, nat, fleets, 'superiority', t);
      if (!fc || a.E < AIR.sky.least) continue;
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
  // (This raid is sent by its gates, with no score of its own, so the sea patrol's staff, which weighs a group against
  // its raid's score, is not asked about it: its groups fly it as the staff sends them.)
  const lineAt = (t: number): void => {
    if (hit.has(t) || !g.regions[t]) return;
    const bombers = leads().filter((f) => whyNotMission(g, f, 'line', t) === null).sort((a, b) => b.bmr - a.bmr || a.id - b.id);
    if (!bombers.length) return;
    const fleets = [...bombers];
    for (const base of new Set(bombers.map(homeOf))) fleets.push(...fightersAt(base).filter((e) => whyNotMission(g, e, 'line', t) === null));
    const { fc } = staffWeigh(g, nat, fleets, 'line', t);
    if (!fc) return;
    if (fc.bomberLoss > rules.loss || fc.success < R.success || fc.cut < rivalsData.soften.raid) return;
    send('line', t, fleets, flown++);
  };
  const aims = Object.keys(g.flags).filter((k) => k === `st:eff:${nat}` || k.startsWith(`st:eff:${nat}:t`)).map((k) => g.flags[k] - 1).sort((a, b) => a - b);
  for (const t of aims) lineAt(t);

  // Airfields as targets: the enemy's fields with aircraft on them, the most first (each lead looks at 'look' of those in its reach).
  const fieldsOn = g.flags.fields === 1 && fieldRules(g);
  const parked = new Map<number, number>();
  if (fieldsOn) for (const x of g.airFleets) if (x.ship === undefined && x.ftr + x.bmr > 0 && atWar(g, nat, x.nat)) parked.set(x.base, (parked.get(x.base) ?? 0) + x.ftr + x.bmr);
  // (The design director, 06:21Z: a staff chooses only among fields it knows, by the one gate every reader uses.)
  const known = fieldsOn ? sightOf(g, nat) : null;
  const fieldsByPlanes = fieldsOn ? [...parked.keys()].filter((id) => fieldKnown(g, nat, id, known!)).sort((a, b) => parked.get(b)! - parked.get(a)! || a - b) : [];
  // The factories and junctions one bomber group weighs: its enemies' in its reach, the richest first.
  const worksFor = (f: AirFleet): Region[] => g.regions
    .filter((r) => !hit.has(r.id) && atWar(g, nat, r.ctrl) && (r.fac > 0 || railsAt(r.id).length >= 3) && whyNotMission(g, f, 'bomb', r.id) === null)
    .sort((a, b) => b.fac + rules.rail * railsAt(b.id).length - (a.fac + rules.rail * railsAt(a.id).length) || a.id - b.id)
    .slice(0, R.look);
  // Fog on raids (airfog.ts, its switch on): before it weighs them, the staff photographs the targets it would weigh
  // that it has never seen, or last saw too long ago, so many a month; then it weighs every target on what it knows.
  if (raidFogOn(g)) {
    const look: number[] = [];
    const fields = new Set<number>();
    for (const f of leads().sort((a, b) => b.bmr - a.bmr || a.id - b.id)) {
      if (fields.has(homeOf(f))) continue;
      fields.add(homeOf(f));
      for (const r of worksFor(f)) if (!look.includes(r.id)) look.push(r.id);
    }
    staffScouts(g, nat, mine, look);
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
        // (The fight back: a staff never bombs the factories of its own country's ground the enemy has taken.)
        if (FB && (m === 'bomb' || m === 'night') && r.owner === nat) return;
        const { fc } = staffWeigh(g, nat, fleets, m, r.id);
        if (!fc) return;
        if (fc.bomberLoss > rules.loss || fc.success < R.success || (m === 'line' && fc.cut < rivalsData.soften.raid)) return;
        // Factories stopped, a factory wrecked, a junction cut or an army shaken, against the aircraft it should cost.
        const railWorth = (m === 'bomb' || m === 'night') && r.fac <= 0 ? rules.rail : 1;
        // (A town its own army stands round comes first with the staff: the assault waits on the bombers.)
        // (The fight back: the invader's troops on its own soil come first, 'home' times, and the gain asked of a raid
        // goes with the bombers it sends, all of 'worth' from 'full' bombers up, so a small air force flies too.)
        const home = FB && (m === 'ground' || m === 'line') && r.owner === nat ? FB.home : 1;
        const v = (fc.worth * railWorth * home * (m === 'town' ? SIEGE.ai.raid : 1) + fc.foeLoss) * (you !== null && r.ctrl === you ? rules.focus ?? 1 : 1) - fc.ftrLoss - fc.bmrLoss;
        const bar = FB ? FB.worth * Math.min(1, fleets.reduce((n, x) => n + x.bmr, 0) / FB.full) : R.worth;
        if (v > bar && (!pick.best || v > pick.best.v)) pick.best = { m, t: r.id, fleets, v };
      };
      const works = worksFor(f);
      for (const r of works) {
        weigh('bomb', r, [f, ...escorts.filter((e) => whyNotMission(g, e, 'bomb', r.id) === null)]);
        weigh('night', r, [f]);
      }
      // Airfields as targets: the enemy's fields in reach with the most aircraft on them, weighed by the same rules.
      if (fieldsOn) {
        let n = 0;
        for (const id of fieldsByPlanes) {
          if (n >= R.look) break;
          if (hit.has(id) || whyNotMission(g, f, 'airfield', id) !== null) continue;
          n++;
          weigh('airfield', g.regions[id], [f, ...escorts.filter((e) => whyNotMission(g, e, 'airfield', id) === null)]);
        }
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
    // (Release 2, the sea patrol, where it is live and laid: before the raid flies the patrol staff weighs its bomber
    // group's patrol against the raid's own score, and a group it takes onto patrol gives up the raid, which is not
    // flown (the design director's ruling of 24 November 2026 17:49Z, 2 (a)). The month's count of raids stands.)
    if (patrol?.(pick.best.fleets[0], pick.best.v)) continue;
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
  const { R, B, rules } = staffRules(g, nat);
  const you = B ? seat(g) : null;
  const here = (f: AirFleet): boolean => f.nat === nat && f.ship === undefined && f.base === field && f.mission === 'support' && f.flew !== g.turn;
  const lead = g.airFleets.filter((f) => here(f) && f.kind !== 'ftr' && f.bmr >= rules.least && f.ready >= R.ready).sort((a, b) => b.bmr - a.bmr || a.id - b.id)[0];
  if (!lead) return null;
  const escorts = g.airFleets.filter((x) => here(x) && x.kind === 'ftr' && x.ftr > 0 && x.ready >= AIRWAR.ai.escortReady);
  const S = R.sour;
  let best: { m: 'bomb' | 'night'; t: number; fleets: AirFleet[]; v: number } | null = null;
  const weigh = (m: 'bomb' | 'night', r: Region, fleets: AirFleet[]): void => {
    if (S && g.turn < (g.flags[`rso:${nat}:${m}:${r.id}`] ?? 0)) return;
    const { fc } = staffWeigh(g, nat, fleets, m, r.id);
    if (!fc) return;
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
  /**
   * A raid on troops with the strafers' rule on (strafers.ts): the fighters the card expects the ground fire to bring
   * down (in ftrLoss already), and the groups it charges with it, those with the reach to strafe. Absent otherwise.
   */
  fire?: number;
  strafers?: number[];
  /** Airfields as targets: what the bombs should do to the field (fieldstate.ts planFieldHit). */
  field?: { shut: boolean; levels: number; ground: number; grounded: number; room: number; built: number };
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
 * A raid on troops with the strafers' rule on (strafers.ts): whether a group of the raid has the reach to go down on the
 * troops, the troops lying within airwar.json ground.reach of its fighter range (G).
 */
export function canStrafe(g: Game, x: { f: AirFleet; d: number }): boolean {
  return x.d <= (AIRWAR.ground.reach ?? AIR.range.fullAt) * reach(x.f, g).ftrRange;
}

/**
 * The card's strafers (the strafers' rule): the exact expectation of what the staff's settled raid (fly(), no battle)
 * charges, given the air fight the card itself expects (air: the escort's losses at even luck). The design director,
 * 12:59Z question 4: "the card is the exact expectation of the settled raid; where a term is not linear in survivors,
 * the card computes the expectation of the outcome, not the outcome at the expected count".
 *
 * The settled raid, group by group: the air fight takes k = min(F, roll(air x luck x share)) of the group's F fighters
 * (share = its x.ftr over the raid's E; luck even between 1 - combat.luck and 1 + combat.luck, one throw for the raid);
 * those left count (F - k) x esc x crowd; of those, a group with the reach to strafe pays ground.fire x its own luck
 * (mean 1), and the rest weigh on the troops. A roll's mean is its argument and never passes the whole group, so
 * E[k | luck] = min(F, air x luck x share), and over the luck that is the mean of a clipped line: not linear, which is
 * where the card's old "even spread of losses, keep = 1 - air / E" went wrong (the clip at F, and losses counted on the
 * whole group F against a share counted on F x esc x crowd). The ground fire and the weight are linear in what is left,
 * so their means follow from E[k]. A group is named when it is more likely than not to have a fighter left; with the
 * sky gate (16:04Z), more likely than not to win the sky and have a fighter left, and every figure is taken over the
 * lucks on which the sky is won (skyGate).
 */
export function strafersExpected(g: Game, a: AirAssess, air: number): { ids: number[]; left: number; fire: number; weight: number; parts: StrafePart[]; gate?: SkyGate } {
  const L = AIR.combat.luck, lo = 1 - L, hi = 1 + L, fire = AIRWAR.ground.fire;
  // (The sky gate, the design director 16:04Z: in the settled raid a group goes down only when the sky is won, skyWon.
  // Sure to be won on every luck, the card is as it was, to the last digit; sure to be lost, nobody goes down.)
  const gate = skyGate(g, a, air);
  const ids: number[] = [];
  const parts: StrafePart[] = [];
  let left = 0, weight = 0;
  if (gate === 'lost') return { ids, left, fire: 0, weight, parts };
  for (const x of a.att) {
    if (!(x.ftr > 0) || !canStrafe(g, x)) continue;
    const F = x.f.ftr, c = x.esc * x.crowd, m = a.E > 0 ? air * (x.ftr / a.E) : 0;
    parts.push({ F, c, m, keep: 1 - c * fire });
    if (gate !== 'won') {
      // Won on part of the luck: the chance the sky is won and the group has a fighter left, and what it leaves then.
      const l = gate.mean((u) => c * (F - Math.min(F, m * u)));
      if (gate.mean((u) => Math.max(0, Math.min(1, F - m * u))) > 0.5) ids.push(x.f.id);
      left += l;
      weight += l * (1 - c * fire);
      continue;
    }
    // E[min(F, m u)] for u even on [lo, hi]; and the chance a fighter is left, P(roll(m u) < F).
    const lost = m <= 0 ? 0 : clipMean(m, F, lo, hi);
    const some = m <= 0 ? 1 : chanceBelow(m, F, lo, hi);
    const l = c * Math.max(0, F - lost);
    if (some > 0.5) ids.push(x.f.id);
    left += l;
    weight += l * (1 - c * fire);
  }
  return gate === 'won' ? { ids, left, fire: left * fire, weight, parts } : { ids, left, fire: left * fire, weight, parts, gate };
}

/**
 * The sky gate as the card weighs it (the design director, 16:04Z; skyWon): over the settled raid's three lucks, each
 * even on [1 - combat.luck, 1 + combat.luck], the escort's (u: its air losses), the defenders' (their losses to the
 * escort) and the bombers' (ub: the bombers lost, and so the defenders their gunners bring down, as gunsDown counts
 * them), whether the sky is won after the air fight. The rolls' whole numbers are taken at their means, as the card
 * takes them everywhere. 'won' or 'lost' where it is so on every luck; otherwise, for a given u and ub, the chance over
 * the defenders' luck, in closed form (sky), and `mean`, the mean over the escort's luck of that chance (taken in closed
 * form over the bombers' luck) times a term of the escort's luck.
 */
export interface SkyGate {
  sky: (u: number, ub: number) => number;
  /** The breaks over the bombers' luck, for a given u, between which sky is linear in ub. */
  breaks: (u: number) => number[];
  /** Over the escort's luck, the mean of f(u) x (the chance over the other two lucks that the sky is won). */
  mean: (f: (u: number) => number) => number;
  /** The same, of sum over ub of an integrand: E over (u, ub) of h(u, ub, sky), the bombers' luck split at its breaks. */
  mean2: (h: (u: number, ub: number, p: number) => number, cuts: (u: number) => number[]) => number;
}

function skyGate(g: Game, a: AirAssess, air: number): 'won' | 'lost' | SkyGate {
  const C = AIR.combat, lo = 1 - C.luck, hi = 1 + C.luck;
  // The defenders as skyWon weighs them: q a fighter's weight over the target, s the group's share of their losses.
  // (Fog on raids: the fighters last seen near the target, which the card's odds count as rising, are one more group
  // here, fit to fly and taking their share of the defenders' losses, so the gate and the odds weigh one defence.)
  const un = a.unseen;
  const def = a.D > 0 ? [
    ...a.def.map((x) => ({ F: x.f.ftr, est: x.f.est.ftr, q: x.w * worth(g, x.f) * (a.height ? 1 : techMul(g, x.f.nat, 'radar')), s: (x.f.ftr * x.w) / a.D })),
    ...(un ? [{ F: un.n, est: un.n, q: un.w * un.each * (a.height ? 1 : techMul(g, un.nat, 'radar')), s: (un.n * un.w) / a.D }] : []),
  ].filter((d) => d.F > 0 && d.q > 0 && d.s > 0) : [];
  if (!def.length) return 'won';
  const Dnow = (X: number): number => def.reduce((t, d) => t + Math.min(d.est, d.F - Math.min(d.F, X * d.s)) * d.q, 0);
  const Xall = Math.max(...def.map((d) => d.F / d.s));
  // The raid's fighters as the escort's luck u leaves them, as skyWon counts them and weighs them.
  const att = a.att.map((x) => ({ F: x.f.ftr, c: x.esc * x.crowd, v: worth(g, x.f), m: a.E > 0 ? air * (x.ftr / a.E) : 0 }));
  const Eleft = (u: number): number => att.reduce((t, x) => t + x.c * (x.F - Math.min(x.F, x.m * u)), 0);
  const Epow = (u: number): number => att.reduce((t, x) => t + x.c * (x.F - Math.min(x.F, x.m * u)) * x.v, 0);
  // hold = K x Epow / D (skyHold, its type edge and height); the sky is won when D <= K x Epow / win, or D is 0.
  const K = skyHold(a, 1, 1);
  const seen = new Map<number, number>();
  const Xstar = (u: number): number => {
    const was = seen.get(u);
    if (was !== undefined) return was;
    const X = Xfor(u);
    seen.set(u, X);
    return X;
  };
  const Xfor = (u: number): number => {
    const thr = Eleft(u) >= AIR.sky.least ? (K * Epow(u)) / AIR.sky.win : 0;
    if (Dnow(0) <= thr) return 0;
    let x0 = 0, x1 = Xall;
    for (let i = 0; i < 64; i++) { const mid = (x0 + x1) / 2; if (Dnow(mid) <= thr) x1 = mid; else x0 = mid; }
    return x1;
  };
  // The defenders' losses: d x their luck, and the bombers' gunners' kills, gk x the bombers lost (gunsDown is linear
  // in them; a raid on troops does not read the target there), the bombers lost B x min(most, bomberLoss x ub).
  const se = Math.sqrt(a.Epow), sd = Math.sqrt(a.Dpow);
  const share = se + sd > 0 ? se / (se + sd) : 0;
  const d = C.fighters * intensity('ground') * Math.min(a.E, a.D) * clashOf(g, 'ground') * 2 * share;
  const gk = gunsDown(g, a, false, 1, 'ground', -1), most = mostBomberLoss(g), slope = gk * a.B * a.bomberLoss;
  const Gb = (ub: number): number => gk * a.B * Math.min(most, a.bomberLoss * ub);
  const at = (X: number, ub: number): number => (d > 0 ? Math.max(0, Math.min(1, (hi - (X - Gb(ub)) / d) / (hi - lo))) : Gb(ub) >= X ? 1 : 0);
  // On every luck: won where the worst (the escort's most losses, the defenders' and gunners' fewest) wins it.
  if (d * lo + Gb(lo) >= Xstar(hi)) return 'won';
  if (d * hi + Gb(hi) < Xstar(lo)) return 'lost';
  const kink = slope > 0 && a.bomberLoss > 0 ? most / a.bomberLoss : Infinity;
  const breaks = (u: number): number[] => {
    const X = Xstar(u), out = [lo, hi];
    if (kink > lo && kink < hi) out.push(kink);
    // Where the chance reaches 0 or 1 (or the gunners alone decide it): Gb(ub) = X - d x hi, X - d x lo, X.
    if (slope > 0) for (const y of [X - d * hi, X - d * lo, X]) { const ub = y / slope; if (ub > lo && ub < hi && ub < kink) out.push(ub); }
    return out;
  };
  // The escort's luck, split where a group's loss reaches the whole group and where the fighters left pass sky.least.
  const knots = [lo, hi];
  for (const x of att) if (x.m > 0 && x.F / x.m > lo && x.F / x.m < hi) knots.push(x.F / x.m);
  if ((Eleft(lo) >= AIR.sky.least) !== (Eleft(hi) >= AIR.sky.least)) {
    let u0 = lo, u1 = hi;
    for (let i = 0; i < 64; i++) { const mid = (u0 + u1) / 2; if (Eleft(mid) >= AIR.sky.least) u0 = mid; else u1 = mid; }
    knots.push(u0);
  }
  knots.sort((x, y) => x - y);
  // Composite Simpson's rule over u, 24 panels between each pair of knots; over ub, Simpson on each piece between the
  // breaks, where the integrand is a quadratic (a line times a line), so that part is exact.
  const N = 24;
  const overU = (f: (u: number) => number): number => {
    let sum = 0;
    for (let i = 0; i + 1 < knots.length; i++) {
      const u0 = knots[i], u1 = knots[i + 1];
      if (!(u1 > u0)) continue;
      const h = (u1 - u0) / N;
      // (Just inside each end, so a step at a knot is read from the piece's own side.)
      const ev = (k: number): number => f(k === 0 ? u0 + 1e-12 * (u1 - u0) : k === N ? u1 - 1e-12 * (u1 - u0) : u0 + k * h);
      let t = ev(0) + ev(N);
      for (let k = 1; k < N; k++) t += (k % 2 ? 4 : 2) * ev(k);
      sum += (t * h) / 3;
    }
    return sum / (hi - lo);
  };
  const overB = (u: number, h: (ub: number) => number, more: number[]): number => {
    const xs = [...breaks(u), ...more.filter((b) => b > lo && b < hi)].sort((x, y) => x - y);
    let sum = 0;
    for (let i = 0; i + 1 < xs.length; i++) {
      const b0 = xs[i], b1 = xs[i + 1];
      if (b1 > b0) sum += ((b1 - b0) / 6) * (h(b0) + 4 * h((b0 + b1) / 2) + h(b1));
    }
    return sum / (hi - lo);
  };
  const sky = (u: number, ub: number): number => at(Xstar(u), ub);
  return {
    sky, breaks,
    mean: (f) => overU((u) => { const X = Xstar(u); return f(u) * overB(u, (ub) => at(X, ub), []); }),
    mean2: (h, cuts) => overU((u) => { const X = Xstar(u); return overB(u, (ub) => h(u, ub, at(X, ub)), cuts(u)); }),
  };
}

/**
 * One group with the reach to strafe, as strafersExpected weighs it: F fighters, c = esc x crowd of them counting, m the
 * air fight's loss to it at even luck, keep the share of a counting fighter's weight left after the ground fire.
 */
export interface StrafePart { F: number; c: number; m: number; keep: number }

/**
 * The card's cut on the troops with the strafers' rule on: the expectation of the settled raid's troopCut over the
 * raid's luck (the design director, 12:59Z question 4: where a term is not linear the card gives the expectation of the
 * outcome; the review of the air follow-on, finding 2). troopCut clips at ground.max, so it is not linear where the clip
 * binds. The settled raid throws two independent lucks that move the weight on the troops, each even on
 * [1 - combat.luck, 1 + combat.luck]: the bombers' (their losses, so those that get through) and the escort's (the air
 * fight's losses, so the strafers left). Given them, the expected weight is linear in what gets through and in the
 * strafers left, as the card's own figures read them: through(ub) = through x (1 - bomberLoss x ub) / (1 - bomberLoss),
 * mean the card's through; strafers left E[F - min(F, m ue)] x c x keep for each group, mean strafersExpected's weight.
 * So the expectation is a double integral of a clipped plane: taken exactly, in closed form over the bombers' luck and by
 * Simpson's rule over the escort's between every kink, where the integrand is a quadratic. No dice are drawn.
 * Where the clip cannot bind at the raid's best luck the cut is troopCut at the expected weight, the card's figure
 * before this, to the last digit. The rolls' whole numbers and each strafer's own ground-fire luck are taken at their
 * means (linear terms, except where they move the clip by a fraction of an aircraft).
 */
function troopCutExpected(g: Game, a: AirAssess, r: Region, foes: Corps[], nat: string, through: number, s: { weight: number; parts: StrafePart[]; gate?: SkyGate }): number {
  const G = AIRWAR.ground, w = troopWeight(g, nat);
  const linear = troopCut(r, foes, (through + s.weight * G.fighters) * w);
  const divs = foes.reduce((t, c) => t + c.divs, 0);
  if (divs <= 0 || w <= 0) return linear;
  const L = AIR.combat.luck, lo = 1 - L, hi = 1 + L, bl = a.bomberLoss;
  // The weight in bombers through plus strafers x ground.fighters: what the clip M stands at in those units.
  const M = (G.max * (G.scale + G.perDiv * divs)) / w;
  const S = (u: number): number => s.parts.reduce((t, p) => t + p.c * (p.F - Math.min(p.F, p.m * u)) * p.keep, 0) * G.fighters;
  // through(ub) = T0 - beta x ub.
  const T0 = bl < 1 ? through / (1 - bl) : through, beta = bl < 1 ? (through * bl) / (1 - bl) : 0;
  if (T0 - beta * lo + S(lo) <= M) return linear;
  // (The sky gate, 16:04Z: won on part of the luck only. The strafers weigh on the troops only where the sky is won;
  // the bombers' luck moves both what gets through and the gunners' share in winning it, so the two lucks are taken
  // together: over the bombers' luck exactly, piece by piece between the breaks of the gate and of the clip.)
  const gate = s.gate;
  if (gate) {
    const clip = (y: number): number => Math.min(M, y);
    const cuts = (u: number): number[] => (beta > 0 ? [(T0 + S(u) - M) / beta, (T0 - M) / beta] : []);
    const sum = gate.mean2((u, ub, p) => p * clip(T0 - beta * ub + S(u)) + (1 - p) * clip(T0 - beta * ub), cuts);
    return sum * (w / (G.scale + G.perDiv * divs)) * (G.cover[r.terrain] ?? 1);
  }
  // E over the bombers' luck of min(M, A - beta ub), in closed form.
  const overB = (A: number): number => {
    if (beta <= 0) return Math.min(M, A);
    const c = Math.max(lo, Math.min(hi, (A - M) / beta));
    return (M * (c - lo) + (A * (hi - c) - (beta * (hi * hi - c * c)) / 2)) / (hi - lo);
  };
  const h = (u: number): number => overB(T0 + S(u));
  // Kinks over the escort's luck: where a group's loss reaches the whole group, and where the clip meets either edge of
  // the bombers' luck (S is linear between the first kind, so the second kind is found on each piece).
  const knots = [lo, hi];
  for (const p of s.parts) if (p.m > 0 && p.F / p.m > lo && p.F / p.m < hi) knots.push(p.F / p.m);
  knots.sort((x, y) => x - y);
  const all = [...knots];
  for (let i = 0; i + 1 < knots.length; i++) {
    const u0 = knots[i], u1 = knots[i + 1];
    if (u1 <= u0) continue;
    const s0 = S(u0), s1 = S(u1);
    if (s1 === s0) continue;
    for (const b of [lo, hi]) {
      const u = u0 + ((M + beta * b - T0 - s0) / (s1 - s0)) * (u1 - u0);
      if (u > u0 && u < u1) all.push(u);
    }
  }
  all.sort((x, y) => x - y);
  let sum = 0;
  for (let i = 0; i + 1 < all.length; i++) {
    const u0 = all[i], u1 = all[i + 1];
    if (u1 <= u0) continue;
    sum += ((u1 - u0) / 6) * (h(u0) + 4 * h((u0 + u1) / 2) + h(u1));
  }
  return (sum / (hi - lo)) * (w / (G.scale + G.perDiv * divs)) * (G.cover[r.terrain] ?? 1);
}

/** The mean of min(F, m u) for u even on [lo, hi] (m > 0). */
function clipMean(m: number, F: number, lo: number, hi: number): number {
  if (m * hi <= F) return m * (lo + hi) / 2;
  if (m * lo >= F) return F;
  const u = F / m;
  return ((m / 2) * (u * u - lo * lo) + F * (hi - u)) / (hi - lo);
}

/** The chance that roll(m u) < F for u even on [lo, hi] (m > 0, F whole): 1 below F - 1, F - y between, 0 from F. */
function chanceBelow(m: number, F: number, lo: number, hi: number): number {
  // P(roll(y) < F) = clamp(F - y, 0, 1) for whole F; integrate over y = m u, u even on [lo, hi].
  const y0 = m * lo, y1 = m * hi;
  if (y1 <= y0) return Math.max(0, Math.min(1, F - y0));
  const prim = (y: number): number => (y <= F - 1 ? y : y >= F ? (F - 1) + 0.5 : (F - 1) + ((y - (F - 1)) - ((y - (F - 1)) ** 2) / 2));
  return (prim(y1) - prim(y0)) / (y1 - y0);
}

/**
 * The law fly() weighs a raid on troops by (airfought.ts TroopLaw), from the bombers that drop and the fighters that
 * strafe. Null for any other raid, or where no enemy stands there. An air battle carries it (release 2: a raid on troops
 * fought out), so the field earns the very cut the campaign books.
 */
export function troopLaw(g: Game, a: AirAssess, m: AirMission, target: number): TroopLaw | null {
  if (m !== 'ground') return null;
  const G = AIRWAR.ground, r = g.regions[target], nat = a.att[0]?.f.nat ?? '';
  const divs = r.units.reduce((s, c) => s + (!c.dead && atWar(g, nat, c.nat) ? c.divs : 0), 0);
  if (divs <= 0) return null;
  const w = troopWeight(g, nat);
  return {
    scale: G.scale, perDiv: G.perDiv, max: G.max, cover: G.cover[r.terrain] ?? 1, divs,
    // As fly() aims the bombers: the day's accuracy, their research and the weather on the day; then the nation's weight row.
    bomber: dayAim(g, nat, m) * techMul(g, nat, 'bombers') * a.aim * w, fighter: G.fighters * w,
    fire: G.fire, dug: G.dug, str: G.str, value: G.value,
    // (The strafers' rule, strafers.ts: the groups with the reach to strafe.)
    ...(strafeOn() ? { reach: a.att.filter((x) => x.ftr > 0 && canStrafe(g, x)).map((x) => x.f.id) } : {}),
  };
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
  let ftrLoss = C.fighters * I * clash * 2 * (1 - share) * (a.height ? 1 : C.escortShare);
  const bmrLoss = a.B * a.bomberLoss;
  // The defenders' losses: to the escort, and to the bombers' own guns (ruling 227).
  const foeGuns = gunsDown(g, a, m === 'night', bmrLoss, m, target);
  const foeLoss = C.fighters * I * clash * 2 * share + foeGuns;
  let cut = 0, nation = 0, value = 0;
  let atomReach: number | null = null;
  let rail: number | undefined, ruin: number | undefined;
  let ships: Ships | undefined, hits: number | undefined;
  let lumps: [number, number][] | undefined, expected = 0;
  let town: TownHit | undefined;
  let city: number | undefined;
  let field: FieldHit | null = null;
  let fire: number | undefined, strafers: number[] | undefined;
  if (m === 'superiority') {
    const based = g.airFleets.filter((f) => f.base === target && f.ship === undefined && atWar(g, f.nat, nat)).length;
    cut = based ? Math.min(AIR.damage.airfields.max, through / AIR.damage.airfields.scale) : 0;
    value = cut * 300 * based;
    // Airfields as targets: the bombers' hit on the field, at the one rate the field raid is weighed at (answer 5).
    if (g.flags.fields === 1 && fieldRules(g) && through > 0) { field = planFieldHit(g, target, 'sky', fieldBlow(g, a, nat, target, through, 0)); if (field) value += field.worth; }
  } else if (m === 'airfield') {
    field = planFieldHit(g, target, 'raid', fieldBlow(g, a, nat, target, through, Math.max(0, a.E - ftrLoss)));
    cut = field?.closed ? 1 : 0;
    value = field?.worth ?? 0;
  } else if ((m === 'bomb' || m === 'night') && armedAt(g, nat) === target) {
    // (The atomic project, release 2, atom.ts) A raid that carries the bomb does nothing else to the city (priced once):
    // its one chance is that the bomber with the bomb reaches the release, as often as the raid's bombers do (fly).
    atomReach = Math.max(0, Math.min(1, 1 - a.bomberLoss));
  } else if (m === 'bomb' || m === 'night') {
    const r = g.regions[target];
    const total = g.regions.reduce((s, x) => s + (x.ctrl === r.ctrl ? x.fac : 0), 0);
    cut = Math.min(AIR.damage.industry.max, through / AIR.damage.industry.scale);
    if (a.aa) cut *= 1 - a.aa * WORKS.aa.cut;
    nation = total > 0 ? cut * (r.fac / total) : 0;
    value = outputWorth(g, r, cut);
    // (The atomic project, release 2: an enemy's unfinished atomic plants there, where the raiders' side has seen them, atom.ts.)
    value += atomRaidWorth(g, nat, r, cut);
    // The grand campaign: the chance of cutting the province's railway, and of wrecking a factory outright.
    if (groundRules(g)) {
      rail = railBombChance(g, target, through, a.aa ? a.aa * WORKS.aa.cut : 0);
      ruin = ruinChance(g, r, cut);
      if (r.fac <= 0) cut = 0;
    }
    // Damaged cities: the lasting damage the bombs should leave on the town, and what that is worth.
    // (Fog on raids: weighed on one side's knowledge, the town is as ruined as that side last saw it.)
    const pts = cityForecast(g, r, 'raid', through, gunsStop(a.aa ?? 0), m === 'night', a.known?.ruined);
    if (pts > 0) { city = pts; value += cityWorthOf(g, r, pts, a.known?.ruined); }
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
    const pts = cityForecast(g, g.regions[target], 'town', town.share, 0, false, a.known?.ruined);
    if (pts > 0) { city = pts; value += cityWorthOf(g, g.regions[target], pts, a.known?.ruined); }
  } else if (m === 'line') {
    cut = lineWear(through);
    value = cut * g.regions[target].units.reduce((s, c) => s + (!c.dead && atWar(g, nat, c.nat) && worksOf(c) > 0 ? c.divs : 0), 0) * AIRWAR.line.value;
  } else if (m === 'ground') {
    const G = AIRWAR.ground;
    const r = g.regions[target];
    const foes = r.units.filter((u) => !u.dead && atWar(g, nat, u.nat));
    if (strafeOn()) {
      // (The strafers' rule, strafers.ts: the ground fire, and the fighters' weight on the troops, only on the fighters
      // expected to be left after the air fight that have the reach to strafe, each group its share of the air's losses.)
      const s = strafersExpected(g, a, ftrLoss);
      ftrLoss += s.fire;
      fire = s.fire; strafers = s.ids;
      // (The cut clips at ground.max, so it is the expectation of the settled raid's cut over the raid's luck, ruling 4;
      // where the clip cannot bind on any luck that is the cut at the expected weight, to the last digit.)
      cut = troopCutExpected(g, a, r, foes, nat, through, s);
      strafeWatch.tell?.({ route: 'card', target, nat, strafers: s.ids, fire: s.fire, weight: s.weight, cut });
    } else {
      const fire = a.E * G.fire;
      ftrLoss += fire;
      cut = troopCut(r, foes, (through + Math.max(0, a.E - ftrLoss) * G.fighters) * troopWeight(g, nat));
    }
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
  if (atomReach !== null) success = atomReach;
  const fc: AirForecast = { odds, bomberLoss: a.bomberLoss, cut, nation, rail, ruin, ftrLoss, bmrLoss, foeLoss, foeGuns, success, worth: value + 300 * (pr + pf), ships, hits, town };
  if (city !== undefined) fc.city = city;
  if (fire !== undefined) { fc.fire = fire; fc.strafers = strafers; }
  if (field) fc.field = { shut: field.closed, levels: field.levels, ground: field.ground, grounded: field.grounded, room: field.room, built: field.built };
  return fc;
}
