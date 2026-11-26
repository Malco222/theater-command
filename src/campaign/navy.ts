// The war at sea: fleets, who holds which sea, naval battles, the convoy war, and armies
// carried across the water. Like the land war it is resolved from what the campaign map
// knows (ships by class, aircraft on the coasts, admirals, readiness), and every number
// lives in data/campaign/navy.json and data/campaign/seas.json.

import { chartDist, seaDist } from './wrap';
import { byHand } from './depth';
import mapData from '../generated/campaign-map.json';
import americasMap from '../generated/world-americas.json';
import wholeMap from '../generated/world-whole.json';
import { FAR_STAGE, SUBSCENE_ENV } from './stage';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/navy.json';
import fleetData from '../../data/campaign/fleets.json';
import convoyBattleData from '../../data/campaign/convoybattle.json';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';

/** The wider world's chart: the Americas with their cities, or the whole world once the far stage is on (stage.ts). */
const worldMap = FAR_STAGE ? wholeMap : americasMap;
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { AIR, CARRIERS, airPool, assessAir, carrierRules, fieldNear, groupOverSea, groupsOver, km, nearRow, seaKm, skyDenies, skyOver, skyReach } from './air';
import { dayShare, oddsOf, skyAtSea, skySupport, weatherOn, zoneOfSea } from './weather';
import { galeConvoyLoss, galeLaneLoss, galeSeas } from './heavyseas';
import { capOver, deckWorth, fleetKnown, syncWings, wingShare, wingStep } from './carriers';
import { COMMAND } from './commanders';
import { nameLosses, nameSide } from './hulls';
import { storyConvoy, storyEdge, storySea } from './stories';
import { bookSeaAction, loseStars, mendStars } from './stars';
import { seaHead, seaIs, seaPlural, seaWhat, seaWhere } from './sea-words';
import { fleetWhyOn } from './fleetwhy';
import { SHADOW, allShadows } from './shadow';
import { noteFight } from './voyage';
import { techMul, techNotes } from './research';
import { dcOf, hasTech, techTraits } from './tech';
import { PLANS } from './plans';
import { unitsDecide } from './units';
import { patrolLoss, patrolOn, watchAgainst, watchFor } from './seapatrol';
import { noteSeaSight, seaMonth, seenAir, type SeaMonth } from './seasight';
import causeData from '../../data/campaign/convoycause.json';
import { whyBarred } from './ground';
import { lineShut } from './line';
import { alongside, riding } from './stations';
import { stationBars, whyStationBars } from './far';
import { FAR_SEAS_FIRST, farOn } from './world';
import { PACIFIC, coversBeaches, hopAims, pacificGround, ringDrawn, shoreLift, skiesOver, useShoreLift, type ShoreLift } from './pacific';
import { HISTORY, bookDate, dialDraw, onBook, stopDef, stopOf, theatreOf, unitsFree } from './history';
import { aloneLine, allied, thornGround, assess, atWar, capture, fights, friendlyUnits, gunsMayFollow, gunsRule, headline, hostileUnits, men, monthsUnfed, placeName, relocate, resolveBattle, roomForAll, routineNews, warning, placed, type Assessment } from './game';
import { fits, islandOf, roomRefusal, roomSays } from './islands';
import { traitFleet, traitSeaLoss } from './traits';
import { SIEGE, arrive, besieged, besiegerOf, feeders, fieldCtrl, garrisonOf, roomIn, siegeRules, housed } from './siege';
import { harbourOpen, prizeOut } from './prizes';
import { RES, fieldBeside, fuelFleet, laneStopped } from './resources';
import { bargainOutputLoss, replacedDeal } from './bargains';
import {
  MEN_PER_DIV, STACK, type BattleReport, type Corps, type Fleet, type Game, type Gate, type Nation, type Region, type SeaReport, type SeaSide, type SeaZone, type ShipClass, type Ships, type Stance, type FleetStance,
  type SubLook, type SubLookFleet,
} from './types';

interface ClassSpec { label: string; one: string; many: string; power: number; tough: number; escort: number; raid: number }
export interface FleetRow {
  nat: string; name: string; from?: number; base: string | null; port: string; home: string; zone: string; duty: Stance; tied?: boolean; admiral?: string; ships: Partial<Ships>;
  /** The share of its navy's wing each of its carriers brings, where less than one (Fleet.deck; ruling 189(b)). */
  deck?: number;
  /** What the row says instead in the grand campaign (Force H's beat, ruling 233 rider b564). The history campaigns read the row as it stands. */
  open?: Partial<Pick<FleetRow, 'home' | 'zone' | 'tied'>>;
  /**
   * The month and the ships it forms with instead at these stops of the grand campaign, for the computer's navy only,
   * as every row of the dial (Force H in June 1940 at stops 1 and 2: ruling 233, the director's note of 10:06Z). Where
   * the dial reaches that stop only after the month, the fleet forms in the first month at one of them, up to its own.
   */
  book?: { stops: number[]; from: number; ships: Partial<Ships> };
  /**
   * The far side (ruling 234(d)): [sea, chance, whose]. At a stop of the history dial that keeps no book, in a month its
   * country is at peace, the fleet is at sea there on this chance by the war's seed and the month: a strike that
   * opens a war finds it in harbour or does not, and nobody can read which from where a row sits. (At the stops
   * that keep the book the record's own month at sea is a dated move with 'sea'.) 'whose' names the country whose
   * blow the month waits for: where it is given, peace is peace with that country (rider b593: `navalUpkeep`).
   */
  cruise?: [string, number, string?];
}
/**
 * A single arrival's sixth entry. 'stops': the history dial's stops it is read at (the grand campaign only; for the
 * player's navy in his start month, and after it only a ship coming onto the chart: offChartFor). 'sunk': the ships
 * that leave are lost. 'from': a move, the ships coming from that fleet of the same navy, as many of them as it has.
 * 'by': for a move, the turn of the waiting row of 'from' that brings those ships there; while that row has not been
 * read they come from off the chart instead, and it is closed. 'fromIf': a move from that fleet where it is on the
 * chart in this game, and otherwise an arrival from off it. 'waits': the row is read only once play has taken ships of
 * its kind from the fleet that no waiting row has made good. 'lapse': from that turn on the row is never read. 'most':
 * the most of each class named the fleet may hold after the row, so a row read late into a fleet formed with those
 * ships already brings no second of them. 'not': for a row with no 'stops', the stops at which the computer's navy does
 * not read it, a dated row standing in its place (the carriers' rows, ruling 233: the director's notes of 08:34Z,
 * 08:38Z, 10:02Z and 10:06Z). 'back': a ship's return to the chart, the turn and fleet of the row that sent her off it.
 * 'instead': on a dated row, the turn of the plain row ('not') it stands in for (the director's note of 15:52Z on
 * ruling 234 rider b639: what the player's navy reads after his start).
 */
export interface ArrivalDial { stops?: number[]; sunk?: boolean; from?: string; fromIf?: string; by?: number; waits?: boolean; lapse?: number; most?: Partial<Ships>; not?: number[]; back?: [number, string]; instead?: number }
/**
 * One group of a navy's submarines on its beat (navy.json 'beats'; ruling 233 rider b564): a fleet row of its own, or
 * only the name of a fleet the navy already has. 'held': a place it sails from only while its own side holds it.
 * 'stations': dated changes of its beat, [from turn, sea]. 'hunts': the nation whose trade it hunts ('to') and the
 * record's seas where it hunted, by half-year ([from turn, seas]); at stops 1 to 3 its beat is read from them (huntBeat;
 * ruling 233 riders b601 and b603), its dated beat only where no sea in its reach carries that trade. 'admirals': dated
 * changes of its commander, [from turn, name] (rider b601: commanders are facts of the day, at every stop).
 * 'bases', the far side's form alone (`placeFarBoats`): dated changes of its base, [from turn, province, the port's
 * name, home sea]. It sails from the latest of them (its row's own base the first) that its side holds.
 */
export interface BeatGroup extends Partial<Omit<FleetRow, 'name'>> {
  name: string; held?: string; stations?: [number, string][]; admirals?: [number, string][];
  /** The nation a far-side group hunts and the record's seas by half-year. 'after' "fleet": its boats were sent against that nation's fleet and its landings, not its cargoes (`fleetBeat`). */
  hunts?: { to: string; after?: string; seas: [number, string[]][] };
  bases?: [number, string, string, string][];
}
/**
 * What some of a navy's groups keep when it places its boats by need (ruling 233 rider b598(1)): while a fleet of an
 * enemy's with surface ships has its home port on one of 'seas', the groups named keep together at least least[0]
 * boats in least[1] of all its groups have.
 */
export interface BeatFloor { groups: string[]; least: [number, number]; seas: string[] }
/**
 * A navy's submarines by beat: Italy's shared out by the historian's rows, Germany's lent from 'from' to the
 * Mediterranean. 'far' marks the far side's form (`placeFarBoats`; none in the older files): the rows are boats on
 * hand by command, lent from the fleets named in 'pools' in their order, and read while the navy is at war with a
 * nation its groups hunt. 'war': the month of the record's war that the rows are dated by.
 */
export interface Beats {
  groups: BeatGroup[]; rows: number[][]; from?: string; cause?: { from: number; by: string[]; ground: string[] }; floor?: BeatFloor;
  /** 'all': the rows' counts are the first month's and no ceiling, so every boat the pools have goes to the groups, in the row's shares. */
  far?: { war: number; all?: boolean }; pools?: string[];
  /** The far side's one measure: of a navy's boats at sea, the share out on patrol, as rows of [from this turn, boats out, of this many on hand]. */
  patrol?: [number, number, number][];
}
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
  /** Release 2, the far chart: how many months a staff weighs a fleet's losses at anchor over (anchorLoss, strikeMoves). */
  anchor?: { horizon: number };
  admiral: { perLevel: number; maxLevel: number; lostWithFlagship: number };
  convoy: { escortWorth: number; ocean: number; air: number; patrolRaid: number; subBase: number; subHunt: number; escortLoss: number };
  convoys: Route[];
  ore: { to: string; source: string; share: number; iceMonths: number[]; winterPort: string; summerZones: string[] };
  fleets: FleetRow[];
  building: { nat: string; fleet: string; cls: ShipClass; from: number; to: number; perMonth: number }[];
  arrivals: [number, string, string, Partial<Ships>, string, ArrivalDial?][];
  /** The far side: a fleet's base by date. None in the older files. */
  moves?: FleetMove[];
  /** The submarines' beats by nation (ruling 233 rider b564). */
  beats?: Record<string, Beats>;
  /**
   * The far side (ruling 194, point 6): a navy whose submarines raid at 'share' of their worth in the far side's seas
   * while their torpedoes fail: until month 'book' where the stop keeps the book, and elsewhere until its 'month'th
   * month at war with 'foe'. None in the older files. `subsWorth`.
   */
  subs?: Record<string, { share: number; book: number; month: number; foe: string }>;
  /**
   * The far side's ship move (the design director's 13:56Z and 18:23Z of 22 November 2026): the seas a corps shipped
   * from port to port crosses in a month, and the record's passages it is the middle of. None in the older files.
   * `sailReach`, `sailWay`, `sailLeg`.
   */
  sail?: { reach: number; passages: { ports: string[]; days: [number, number]; note?: string }[] };
};

/**
 * A fleet's base by date (navy.json, 'moves'; the Pacific planner's rows): in its month a navy the computer or a
 * player's staff runs moves the fleet's base to the province named, if its country holds the place, and only at the
 * stops of the history dial the row names (every stop where it names none). A player who runs his navy himself
 * moves his fleets when he says (depth.ts, `byHand`).
 *
 * A move of base named with one of the great choices ('with', as a month at sea is; the Asiatic Fleet's months, the
 * design director's rulings of 03:55Z and 04:25Z, 22 November 2026) belongs to the war that choice opens: it keeps
 * its distance from the choice where the stop draws the choice's month, and it is read only while its country is at
 * war with the nation whose choice it is. A row marked 'ally' may take a harbour an ally holds, as an air command's
 * row so marked takes an ally's field (air.ts, `rowGround`): an ally at war with that same nation, where the row
 * names one. A row marked 'eve' is the other side of the blow: it is read in the choice's own month, before the
 * blow falls, on the book's word that it falls that month (`bookLays`: the choice's nation is a computer's, the stop
 * keeps the book and the book is open in the choice's theatre), while its country is still at peace with that
 * nation. Where the book lays no blow (a player's own Japan) no eve is read, and the surprise is the player's.
 */
export interface FleetMove {
  turn: number; nat: string; fleet: string; words: string; stops?: number[];
  /** A move of base: the province, what the harbour is called, and the sea it opens on. 'ally': the harbour may be an ally's. 'eve': read before the blow it is named with. */
  base?: string; port?: string; home?: string; ally?: boolean; eve?: boolean;
  /**
   * The far side (ruling 234(d)) in place of a base: the fleet spends that month at sea in this sea, though its
   * country is at peace, and is back at its base the month after (the Pacific Fleet's carriers in December 1941).
   * Player's and computer's alike: at peace nobody's fleet is his to send out. 'with': the great choice whose month
   * it keeps, 'JAP:south': at a stop where that choice's date is drawn, the move's month slides as far.
   */
  sea?: string; with?: string;
}

/** The grand campaign's navy: escorts bound to lanes, destroyers that hunt, guns that shell a coast. See fleets.json. */
export const FLEETS = fleetData as unknown as {
  escort: { loose: number };
  /** A computer's navy that keeps its harbours once an event has come: the country, and the event's flag (ruling 233(k)). */
  harbour?: Record<string, string>;
  hunt: { base: number; rate: number; sea: number; perSub: number; hunter: number; escort: number; loose: number; carrier: number; air: number; most: number; cost: number };
  screen: { per: number; risk: number; most: number };
  bombard: {
    guns: Ships; least: number; ready: number; minReady: number;
    troops: { scale: number; perDiv: number; max: number; str: number; dug: number; fort: number; floor: number };
    city: { scale: number; max: number };
    reply: number;
  };
  ai: { bombard: boolean; ready: number; sortie: number; guard?: boolean; hunt?: boolean; allies?: boolean };
  roles: Record<ShipClass, string>;
  brief: Record<ShipClass, string>;
  orders: Record<FleetStance, { label: string; blurb: string }>;
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
/**
 * A fleet's name in a sentence, as the service says it (the UI director, b458): "the Home Fleet" and "the Polish Navy",
 * but plain "Force H".
 */
export const fleetName = (f: { name: string }): string => (/(Fleet|Navy)$/.test(f.name) ? `the ${f.name}` : f.name);

export const zoneOf = (g: Game, key: string): number => g.seas.findIndex((z) => z.key === key);
export const shipCount = (s: Ships, list: ShipClass[] = CLASSES): number => list.reduce((n, c) => n + s[c], 0);
export const surfaceShips = (f: Fleet): number => shipCount(f.ships, SURFACE);
export const atSea = (f: Fleet): boolean => f.stance !== 'port';
/**
 * True while a fleet is in passage to a new base beyond a month's steaming (release 2, the far chart; the design
 * director's ruling of 04:33Z, 25 November 2026; rebase). It is at sea (atSea), so it is seen, searched for and
 * attacked as any fleet at sea; it strikes, patrols, escorts and holds no sea. No fleet is in passage on the old chart.
 */
export const inPassage = (f: Fleet): boolean => f.stance === 'passage';
/** A fleet at sea that holds the sea it is on: every one but a fleet in passage, which is only crossing it. */
const holdsSea = (f: Fleet): boolean => atSea(f) && f.stance !== 'passage';
export const fleetsOf = (g: Game, nat: string): Fleet[] => g.fleets.filter((f) => f.nat === nat);

/**
 * A way cargoes travel by sea to one nation: a shipping lane of resources.json or a convoy route of navy.json.
 * `carries`: false where it carries nothing laid (cargoLaid), as seaRoutes reads it; every staff's choice by sea asks
 * laneCounts. (A route laid in by hand without it, as a test's, counts.)
 */
export interface SeaRoute { id: string; name: string; to: string; zones: number[]; carries?: boolean }
const routeCache = new WeakMap<SeaZone[], SeaRoute[]>();

/**
 * Whether a sea lane or convoy route carries something laid (the design director's ruling of 12:39Z, 25 November
 * 2026, Pacific batch 3, A(2)): oil or supplies on its lane row (its own shares, or those it carries for another
 * lane), oilfields, or a convoy route of its id. A 0 that stands for "not read" is not a read zero: a lane laid with
 * nothing counted yet is no staff's need. Every staff, both sides, every stop, reads this one predicate wherever it
 * counts lanes by sea (laneCounts: shippingIn, lanesTo, laneUnguarded, laneOnBeat, fleets.ts openEscorts).
 * The lane stays on the pages and open to the player: escorting it and raiding it never read it. When a cargo is
 * read and laid, the lane counts by the same rule with nothing else to change. Every row laid before the ruling
 * carries something, so no existing choice moves, on either chart.
 */
function cargoLaid(id: string): boolean {
  if (NAVY.convoys.some((c) => c.id === id)) return true;
  const l = RES.lanes.find((x) => x.id === id);
  if (!l) return false;
  return l.oil > 0 || l.sup > 0 || (l.fields?.length ?? 0) > 0 || (!!l.split && (l.split.oil > 0 || l.split.sup > 0));
}

/** The one predicate every staff's count of lanes by sea reads: a route counts while it carries something laid (cargoLaid). */
export const laneCounts = (rt: SeaRoute): boolean => rt.carries !== false;

/** Whether the sea lane or convoy route of this id carries something laid, as every staff counts it (cargoLaid). */
export const laneCarries = (g: Game, id: string): boolean => { const rt = seaRoutes(g).find((r) => r.id === id); return !!rt && laneCounts(rt); };

/**
 * A fleet's page on a lane whose shipping is not counted in the game (cargoLaid false), word for word (the UI director's
 * words of 12:57Z, 25 November 2026, for the design director's ruling of 12:39Z, Pacific batch 3, A(2) and A(7)): the
 * first in place of the tons and the sunk line, the second after it, on the fleet page and nowhere else.
 */
export const LANE_UNCOUNTED = 'Its shipping is not counted in the game: an escort saves no cargo here, and raiders sink none.';
export const LANE_NO_STAFF = 'So no staff on either side, yours included, sends ships to guard or raid it.';

/** Every route cargoes travel by sea, once each: what an escort can be bound to. */
export function seaRoutes(g: Game): SeaRoute[] {
  let rows = routeCache.get(g.seas);
  if (!rows) {
    rows = [];
    const add = (id: string, name: string, to: string, keys: string[]): void => {
      const zones = keys.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
      if (zones.length && !rows!.some((r) => r.id === id)) rows!.push({ id, name, to, zones, carries: cargoLaid(id) });
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
// The wider world's lookup holds sea numbers past 127 once the far side is on the chart (145 seas): two bytes a cell.
let wide: Int16Array | null = null;

/** The same on the wider world: its chart is laid out in the same squares, from its own west edge, each row as runs of [sea, squares]. */
function seaAtWide(x: number, y: number): number {
  const W = worldMap as any as { frame: { x0: number; x1: number; y0: number }; world: { width: number }; seas: { cell: number; cols: number; grid: number[][] } };
  const md = W.seas;
  // Where the chart goes all the way round the world (the far side on: its frame is as wide as the world), a point a
  // whole turn east or west of the frame is the same place, and is read there. The 3D table's eye follows the map
  // round, so the pointer's place may lie a turn outside the frame (seam-3 of the 3D map audit, 24 November 2026).
  // A point inside the frame is read exactly as before, and on a narrower chart no point is moved.
  const span = W.frame.x1 - W.frame.x0 >= W.world.width - 0.5 ? W.world.width : 0;
  if (span > 0 && (x < W.frame.x0 || x >= W.frame.x0 + span)) x -= Math.floor((x - W.frame.x0) / span) * span;
  if (!wide) {
    wide = new Int16Array(md.cols * md.grid.length).fill(-1);
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

/**
 * The squares of the chart in play, sea by sea (their centres, in map units), and each square's area in square
 * nautical miles: the map's own measure of a sea's waters (release 2, the sea patrol's reach, ruled for the dormant
 * interim at 17:49Z). Read only by the sea patrol, which nothing reads while it is dormant (its switch off, or its
 * figures unread).
 */
const squaresOf = new Map<string, { at: Map<number, { x: number; y: number }[]>; nm2: number }>();
export function seaSquares(g: Game): { at: Map<number, { x: number; y: number }[]>; nm2: number } {
  const wideChart = g.flags.world === 1;
  const key = wideChart ? 'world' : 'europe';
  let s = squaresOf.get(key);
  if (s) return s;
  const at = new Map<number, { x: number; y: number }[]>();
  const put = (z: number, x: number, y: number): void => { if (z >= 0) { let l = at.get(z); if (!l) at.set(z, (l = [])); l.push({ x, y }); } };
  let cell: number;
  if (wideChart) {
    const W = worldMap as any as { frame: { x0: number; y0: number }; seas: { cell: number; cols: number; grid: number[][] } };
    cell = W.seas.cell;
    for (let r = 0; r < W.seas.grid.length; r++) for (let c = 0; c < W.seas.cols; c++) {
      const x = W.frame.x0 + (c + 0.5) * cell, y = W.frame.y0 + (r + 0.5) * cell;
      put(seaAtWide(x, y), x, y);
    }
  } else {
    const md = (mapData as any).seas as { cell: number; grid: string[] };
    cell = md.cell;
    const cols = Math.ceil((mapData as any).width / cell);
    for (let r = 0; r < md.grid.length; r++) for (let c = 0; c < cols; c++) {
      const x = (c + 0.5) * cell, y = (r + 0.5) * cell;
      put(seaAt(x, y), x, y);
    }
  }
  const km = cell * AIR.kmPerUnit;
  s = { at, nm2: (km / 1.852) ** 2 };
  squaresOf.set(key, s);
  return s;
}

/** The chart's top edge and scale (tools/build-geo-campaign.mjs; the world chart shares both): latitude = north - y / scale. */
const CHART = { north: 71.4, scale: 40 };
const latOf = new Map<string, Map<number, number>>();
/** A sea's mean latitude on the chart, over its squares (degrees north; release 2, for the day's length over it). */
export function seaLatitude(g: Game, z: number): number {
  const key = g.flags.world === 1 ? 'world' : 'europe';
  let m = latOf.get(key);
  if (!m) latOf.set(key, (m = new Map()));
  let v = m.get(z);
  if (v === undefined) {
    const at = seaSquares(g).at.get(z) ?? [];
    v = at.length ? CHART.north - at.reduce((a, p) => a + p.y, 0) / at.length / CHART.scale : 50;
    m.set(z, v);
  }
  return v;
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

export function addFleet(g: Game, given: FleetRow): Fleet {
  const row = rowFor(g, given);
  const f: Fleet = {
    id: g.nextFleet++, nat: row.nat, name: row.name, ships: { ...none(), ...row.ships }, lost: none(),
    base: row.base ? g.byName[row.base].id : -1, port: row.port, home: zoneOf(g, row.home), zone: zoneOf(g, row.home), stance: 'port',
    station: zoneOf(g, row.zone), duty: row.duty, tied: !!row.tied, ready: 1,
    admiral: row.admiral ? { name: row.admiral, level: 1, xp: 0 } : null, fought: 0,
  };
  if (row.cruise) f.cruise = row.cruise[2] === undefined ? [row.cruise[0], row.cruise[1]] : [row.cruise[0], row.cruise[1], row.cruise[2]];
  if (row.deck !== undefined && row.deck < 1) f.deck = row.deck;
  g.fleets.push(f);
  return f;
}

/** A fleet row as the campaign in hand reads it: in the grand campaign its 'open' fields stand in for its own. */
const rowFor = (g: Game, row: FleetRow): FleetRow => (row.open && seaWar(g) ? { ...row, ...row.open } : row);

/**
 * As a grand campaign opens on a game begun as a history campaign (a later starting month), or one saved before the
 * rows had a grand campaign's own fields is loaded, the fleets already formed take what their rows say for the grand
 * campaign: Force H its beat on the Western Mediterranean. Only a fleet that still stands as the history row has it
 * (at the row's base, on its home sea and station, tied or not as it was) is changed, so this is done once: a fleet
 * that has moved, or been sent to another beat since, keeps what it has.
 */
export function openFleetRows(g: Game): void {
  if (!seaWar(g)) return;
  // A save from the build that dated the Ark Royal's leaving row at 11, before Force H had its book (ruling 233, the
  // director's note of 10:06Z): the row's mark goes to its new date, so she does not leave the Home Fleet twice.
  const old = g.flags['arr:11:UK:Home Fleet'];
  if (old !== undefined && g.flags['arr:10:UK:Home Fleet'] === undefined) {
    g.flags['arr:10:UK:Home Fleet'] = old;
    delete g.flags['arr:11:UK:Home Fleet'];
  }
  for (const row of NAVY.fleets) {
    const f = row.open && g.fleets.find((x) => x.nat === row.nat && x.name === row.name);
    if (!f || !row.base || g.byName[row.base]?.id !== f.base) continue;
    if (f.home !== zoneOf(g, row.home) || f.station !== zoneOf(g, row.zone) || f.tied !== !!row.tied) continue;
    const o = row.open!;
    if (o.home && zoneOf(g, o.home) >= 0) {
      f.home = zoneOf(g, o.home);
      if (!atSea(f)) f.zone = f.home;
    }
    if (o.zone && zoneOf(g, o.zone) >= 0) f.station = zoneOf(g, o.zone);
    if (o.tied !== undefined) f.tied = o.tied;
  }
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
export const fit = (g: Game, f: Fleet): number => (0.5 + 0.5 * f.ready) * fleetFuel(g, f);

/**
 * True for a fleet based beside the oil (ruling 194, point (g)): its base is a harbour on a sea of the far side that
 * touches a working oilfield overseas its side holds (resources.ts oilBeside). Read in the far side's seas alone, so
 * no fleet of Europe's moves.
 */
export const besideOil = (g: Game, f: Fleet): boolean => !!wellsOf(g, f);
/** The oilfield a fleet based beside the oil is fuelled from (`besideOil`), or null. */
export function wellsOf(g: Game, f: Fleet): Region | null {
  if (f.base < 0 || !farOn(g)) return null;
  const seas = g.regions[f.base]?.seas.filter((z) => z >= FAR_SEAS_FIRST) ?? [];
  return seas.length > 0 ? fieldBeside(g, f.nat, seas) : null;
}

/**
 * The fleet's fuel line (the UI director's sheet 24, box 4; ruling 194, point (g)). 'wells' with the field's name for
 * a fleet based beside the oil, whenever that holds, short month or not. Else 'short' with the share of its fitness
 * for sea the want of oil costs it this month, which is the Oil page's own figure (`fuelFleet`). Null where neither
 * holds. Display only: no figure moves.
 */
export function fuelLine(g: Game, f: Fleet): { kind: 'wells'; field: string } | { kind: 'short'; less: number } | null {
  const wells = wellsOf(g, f);
  if (wells) return { kind: 'wells', field: wells.name };
  const less = Math.round((1 - fuelFleet(g, f.nat)) * 100);
  return less >= 1 ? { kind: 'short', less } : null;
}

/**
 * What a fuel shortage leaves of a fleet's fitness for sea: its nation's share, or nothing lost for a fleet based
 * beside the oil while the nation's stock is dry. Its need stays in the nation's book.
 */
export function fleetFuel(g: Game, f: Fleet): number {
  const k = fuelFleet(g, f.nat);
  return k < 1 && besideOil(g, f) ? 1 : k;
}
const admiralMul = (f: Fleet): number => 1 + (f.admiral ? NAVY.admiral.perLevel * (f.admiral.level - 1) : 0);

/** A fleet's weight in a fleet action. */
export function fleetPower(g: Game, f: Fleet): number {
  let p = 0;
  // A carrier is worth what it can fly: one whose aircraft are spent counts for much less (the grand campaign).
  // (The far side's one measure: of its submarines only the boats out count; a boat at its base holds no sea. `boatsOut`.)
  for (const c of CLASSES) p += (c === 'ss' ? boatsOut(g, f) : f.ships[c]) * NAVY.classes[c].power * (c === 'cv' ? deckWorth(g, f) : 1);
  return p * quality(g, f.nat) * fit(g, f) * admiralMul(f) * storyEdge(g, f) * traitFleet(g, f.nat);
}

/**
 * Air strength over a sea from the coasts one side holds: this nation's friends, or (hostile) its enemies. Over the
 * far side's seas it is counted from the land air groups in reach instead (rulings 184 and 187: pacific.json 'skies',
 * air.ts groupsOver); none while the far side's switch is off.
 */
export function airOver(g: Game, z: number, nat: string, hostile = false, beside: ((x: string) => boolean) | null | undefined = hostile ? null : besideIn(g, z, nat)): number {
  const zone = g.seas[z];
  const S = skiesOver(g, z);
  if (S) return groupsOver(g, z, nat, hostile, S, beside);
  if (!zone.coast.length) return 0;
  // Weather (ruling 127): no aircraft cover a sea under a closed sky, and cloud halves what they do.
  const wx = seaSkyMul(g, z);
  if (wx === 0) return 0;
  // (Of its friends, those who count beside it in this sea: besideIn. None is left out with the far side off.)
  const side = (x: string): boolean => (hostile ? atWar(g, x, nat) : allied(g, x, nat) && (!beside || beside(x)));
  let s = 0;
  for (const id of zone.coast) {
    const r = g.regions[id];
    if (r.since >= g.turn) continue; // airfields taken this month are not yet in use
    const n = g.nations[r.ctrl];
    if (!n || !n.alive || !n.atWar.length) continue;
    // An enemy who has won the sky over this coast flies from it in strength, and its owner barely at all.
    const sky = skyOver(g, id);
    if (side(r.ctrl)) s += n.air * (sky ? AIR.sky.seaFoe : 1);
    else if (sky && side(sky)) s += g.nations[sky].air * AIR.sky.seaOwn;
  }
  return (s / zone.coast.length) * zone.air * wx;
}

/**
 * The far side (the director's ruling of 03:23Z, 22 November 2026, on seven Soviet bomber divisions read as America's
 * own over the Honshu Coast, with the Soviet Union not at war with Japan): a nation counts in a sea's read only
 * against those it is at war with. This is the test of who counts beside a nation in one sea: itself always, and a
 * friend only if it is at war with one of the nation's enemies that stand in that sea's read (a fleet at sea in it,
 * or aircraft counted over it). Null where every friend counts as before: with none of its enemies in the sea,
 * where the read closes nothing, and with the far side off (the old chart's form is after release). `form` is the
 * far switch unless a reading asks for the other form on the same month. SeaPicture reads the same from its lists.
 */
export function besideIn(g: Game, z: number, nat: string, form: boolean = farOn(g)): ((x: string) => boolean) | null {
  if (!form) return null;
  const foes: string[] = [];
  const foe = (x: string): void => { if (!foes.includes(x)) foes.push(x); };
  // (A fleet in passage holds no sea, as seaSide and SeaPicture read it: holdsSea, release 2, the far chart.)
  for (const f of g.fleets) if (f.zone === z && holdsSea(f) && atWar(g, f.nat, nat) && fleetPower(g, f) > 0) foe(f.nat);
  const zone = g.seas[z];
  const S = skiesOver(g, z);
  if (S) {
    const near = nearRow(g, z);
    for (const f of g.airFleets) if (fieldNear(g, near, f.base, z) && atWar(g, f.nat, nat) && !foes.includes(f.nat) && groupOverSea(g, f, z, S) > 0) foe(f.nat);
  } else if (zone.coast.length && seaSkyMul(g, z) !== 0) {
    for (const id of zone.coast) {
      const r = g.regions[id];
      if (r.since >= g.turn) continue;
      const n = g.nations[r.ctrl];
      if (!n || !n.alive || !n.atWar.length) continue;
      const sky = skyOver(g, id);
      if (atWar(g, r.ctrl, nat)) { if (n.air > 0) foe(r.ctrl); }
      else if (sky && atWar(g, sky, nat) && g.nations[sky].air > 0) foe(sky);
    }
  }
  if (!foes.length) return null;
  return (x) => x === nat || foes.some((e) => atWar(g, x, e));
}

/** What the sky over a sea leaves of the aircraft over it (weather, ruling 127): none when closed, half under cloud, all with no weather. */
const seaSkyMul = (g: Game, z: number): number => (weatherOn(g) ? skySupport(skyAtSea(g, z)) : 1);

export interface SeaBalance { ships: number; air: number; total: number }

/** One side's strength in a sea: its fleets at sea there, and its aircraft within reach. */
export function seaSide(g: Game, z: number, nat: string, hostile: boolean, form: boolean = farOn(g)): SeaBalance {
  // (The far side: of its friends, those at war with an enemy of its own in this sea: besideIn.)
  const beside = hostile ? null : besideIn(g, z, nat, form);
  let ships = 0;
  // (A fleet in passage crosses the sea and holds none of it: release 2, the far chart. SeaPicture.afloat reads the same.)
  for (const f of g.fleets) if (f.zone === z && holdsSea(f) && (hostile ? atWar(g, f.nat, nat) : allied(g, f.nat, nat) && (!beside || beside(f.nat)))) ships += fleetPower(g, f);
  const air = airOver(g, z, nat, hostile, beside) * NAVY.air.weight;
  return { ships, air, total: ships + air };
}

/**
 * (Speed, the far map: the playtest's finding 40, 25 November 2026.) A sea reading: while a question that changes
 * nothing is answered (the 3D game's list of ports a corps can be shipped to and coasts it can land on, and why not),
 * each sea's closing and risk for a nation, and each search by sea from a coast, are worked out once and kept to the
 * end of the question. Asked port by port, the same search from the corps' own port was made again for every port
 * on the map, and every sea's balance again inside every search: with Japan at war with eight nations, 17 to 21
 * seconds for one corps in Tokyo. Nothing in such a question moves a ship, an aircraft or a front, so nothing they
 * read can change while it is asked; the reading is dropped as the question returns, and an order is never asked
 * inside one. A question about another game (a copy) reads that game afresh.
 */
interface SeaReading { g: Game; closed: Map<string, Int8Array>; risk: Map<string, Float64Array>; searches: Map<string, SeaSearch>; steps: Map<string, ConvoySteps> }
let reading: SeaReading | null = null;
export function seaReading<T>(g: Game, f: () => T): T {
  if (reading) return f();
  reading = { g, closed: new Map(), risk: new Map(), searches: new Map(), steps: new Map() };
  try { return f(); } finally { reading = null; }
}
const readingOf = (g: Game): SeaReading | null => (reading !== null && reading.g === g ? reading : null);

/** A sea is closed to a nation's troopships and supply ships when the enemy is much the stronger there. */
export function closedTo(g: Game, z: number, nat: string): boolean {
  const R = readingOf(g);
  if (!R) return closedNow(g, z, nat);
  let a = R.closed.get(nat);
  if (!a) R.closed.set(nat, (a = new Int8Array(g.seas.length).fill(-1)));
  if (a[z] < 0) a[z] = closedNow(g, z, nat) ? 1 : 0;
  return a[z] === 1;
}
/**
 * (Speed, the far map: the playtest's finding 41.) closedTo and seaRisk of one sea at once, from one count of each
 * side: asked one after the other, each counted the enemy's side again, and our own where it needed it. The trade
 * routes (trade.ts) and every search by sea (seaSearch) ask both of each sea they look at. Same figures, same order.
 */
export function seaOdds(g: Game, z: number, nat: string): { shut: boolean; risk: number } {
  const R = readingOf(g);
  if (!R) return oddsNow(g, z, nat);
  let a = R.closed.get(nat), b = R.risk.get(nat);
  if (!a) R.closed.set(nat, (a = new Int8Array(g.seas.length).fill(-1)));
  if (!b) R.risk.set(nat, (b = new Float64Array(g.seas.length).fill(NaN)));
  if (a[z] < 0 || Number.isNaN(b[z])) { const o = oddsNow(g, z, nat); a[z] = o.shut ? 1 : 0; b[z] = o.risk; }
  return { shut: a[z] === 1, risk: b[z] };
}
function oddsNow(g: Game, z: number, nat: string): { shut: boolean; risk: number } {
  const e = seaSide(g, z, nat, true);
  const hunters = e.ships + e.air * 0.5;
  const closes = e.total >= NAVY.closed.min;
  // (Our own side is counted only where closedTo or seaRisk would have counted it.)
  const own = closes || hunters > 0 ? seaSide(g, z, nat, false).total : 0;
  return { shut: closes && e.total >= NAVY.closed.ratio * own, risk: hunters <= 0 ? 0 : hunters / (hunters + 2 * own + NAVY.intercept.sea) };
}
function closedNow(g: Game, z: number, nat: string): boolean {
  const e = seaSide(g, z, nat, true);
  if (e.total < NAVY.closed.min) return false;
  return e.total >= NAVY.closed.ratio * seaSide(g, z, nat, false).total;
}

/** A sea as SeaPicture.state gives it. */
export interface SeaState {
  fleets: { nat: string; power: number }[];
  /** The coasts' air, where the coasts decide the air over the sea (none where the land air groups do). */
  coasts: { ctrl: string; air: number; sky: string | null; skyAir: number }[];
  /** The land air groups over it, where they decide the air over the sea (skiesOver; none where the coasts do). */
  groups: { nat: string; n: number }[];
  /** Whether the land air groups decide (skiesOver), and the sky's share where the coasts do (1 where they do not). */
  skies: boolean;
  wx: number;
}

/** True when two states of a sea (SeaPicture.state) are alike in everything closed() reads. */
export function sameSeaState(a: SeaState | null, b: SeaState | null): boolean {
  if (a === b) return true;
  if (!a || !b || a.skies !== b.skies || a.wx !== b.wx || a.fleets.length !== b.fleets.length || a.coasts.length !== b.coasts.length || a.groups.length !== b.groups.length) return false;
  for (let i = 0; i < a.fleets.length; i++) if (a.fleets[i].nat !== b.fleets[i].nat || a.fleets[i].power !== b.fleets[i].power) return false;
  for (let i = 0; i < a.coasts.length; i++) {
    const x = a.coasts[i], y = b.coasts[i];
    if (x.ctrl !== y.ctrl || x.air !== y.air || x.sky !== y.sky || x.skyAir !== y.skyAir) return false;
  }
  for (let i = 0; i < a.groups.length; i++) if (a.groups[i].nat !== b.groups[i].nat || a.groups[i].n !== b.groups[i].n) return false;
  return true;
}

/**
 * The seas as they stand at one moment, for asking of nation after nation whether each sea is closed to it (the supply
 * picture asks it of every nation with ships, every phase). What a sea holds does not depend on who asks: the fleets
 * at sea in it and their weight, and the coasts that put aircraft over it. So that is worked out once a sea, and only
 * whose side each is on is asked again. The sums are seaSide's and airOver's, in the same order, so a sea is closed
 * here exactly when closedTo finds it so (a test holds the two together). Good only while nothing moves.
 */
export class SeaPicture {
  private fleets: { nat: string; power: number }[][] | null = null;
  private coasts: ({ ctrl: string; air: number; sky: string | null; skyAir: number }[] | undefined)[] = [];
  private groups: ({ nat: string; n: number }[] | undefined)[] = [];
  private most: Float64Array | null = null;
  private harbours: Region[] | null = null;
  private byHolder: Map<string, Region[]> | null = null;
  constructor(private readonly g: Game) {}

  /** Every province with a port, in order. */
  ports(): Region[] {
    return (this.harbours ??= this.g.regions.filter((r) => r.port));
  }

  /** The provinces with a port that this nation holds, in order: ports() with the others left out. */
  held(nat: string): Region[] {
    if (!this.byHolder) {
      const by = new Map<string, Region[]>();
      for (const r of this.ports()) { const l = by.get(r.ctrl); if (l) l.push(r); else by.set(r.ctrl, [r]); }
      this.byHolder = by;
    }
    return this.byHolder.get(nat) ?? [];
  }

  /**
   * What closed() reads of a sea besides who is at war with whom and who is whose friend, for keeping its answers from
   * one picture to the next (the supply picture's kept lanes, game.ts): null where all there is in the sea could not
   * close it to anybody, or else its fleets at sea with their weight, and its air as closed() counts it: over the far
   * side's seas the land air groups in reach with what each puts there (skiesOver, groupsOf), elsewhere the coasts that
   * put aircraft over it and its sky. Where two pictures' states of a sea are alike (sameSeaState), closed() answers
   * alike there for a nation whose friends and enemies are the same (friendsKeyOf: besideIn's and the sides' tests
   * are atWar() and allied()).
   */
  state(z: number): SeaState | null {
    if (this.mostIn(z) < NAVY.closed.min) return null;
    const S = skiesOver(this.g, z);
    if (S) return { fleets: this.afloat(z), coasts: [], groups: this.groupsOf(z, S), skies: true, wx: 1 };
    return { fleets: this.afloat(z), coasts: this.coastsOf(z), groups: [], skies: false, wx: this.g.seas[z].coast.length ? seaSkyMul(this.g, z) : 1 };
  }

  /** closedTo, from the picture. */
  closed(z: number, nat: string): boolean {
    // A sea where even everything in it together comes short of the least that closes a sea is open to everybody.
    if (this.mostIn(z) < NAVY.closed.min) return false;
    const e = this.side(z, nat, true);
    if (e < NAVY.closed.min) return false;
    return e >= NAVY.closed.ratio * this.side(z, nat, false);
  }

  /**
   * The most any side could have in a sea: every fleet at sea in it and the larger share of every coast's air, added
   * as side() adds them. No part is below nothing, so whatever side() adds for one side is never more than this.
   */
  private mostIn(z: number): number {
    const g = this.g, zone = g.seas[z];
    // Only while the sea's own weight for air is no less than nothing does a larger sum stay the larger.
    if (zone.air < 0 || NAVY.air.weight < 0) return Infinity;
    if (!this.most) this.most = new Float64Array(g.seas.length).fill(NaN);
    if (Number.isNaN(this.most[z])) {
      let ships = 0;
      for (const f of this.afloat(z)) ships += Math.max(0, f.power);
      let air = 0;
      const S = skiesOver(g, z);
      if (S) {
        let s = 0;
        for (const x of this.groupsOf(z, S)) s += x.n;
        air = (s / S.per) * zone.air;
      } else if (zone.coast.length) {
        let s = 0;
        for (const c of this.coastsOf(z)) s += Math.max(0, c.air * (c.sky ? AIR.sky.seaFoe : 1), c.sky ? c.skyAir * AIR.sky.seaOwn : 0);
        air = (s / zone.coast.length) * zone.air;
      }
      this.most[z] = ships + air * NAVY.air.weight;
    }
    return this.most[z];
  }

  /** The fleets at sea in a sea, with their weight, in the order of the game's fleets. */
  private afloat(z: number): { nat: string; power: number }[] {
    const g = this.g;
    if (!this.fleets) {
      const by: { nat: string; power: number }[][] = g.seas.map(() => []);
      for (const f of g.fleets) if (holdsSea(f) && by[f.zone]) by[f.zone].push({ nat: f.nat, power: fleetPower(g, f) });
      this.fleets = by;
    }
    return this.fleets[z];
  }

  /** The coasts of a sea that put aircraft over it (airOver's), with who has won the sky over each. */
  private coastsOf(z: number): { ctrl: string; air: number; sky: string | null; skyAir: number }[] {
    const g = this.g;
    let cs = this.coasts[z];
    if (!cs) {
      cs = [];
      for (const id of g.seas[z].coast) {
        const r = g.regions[id];
        if (r.since >= g.turn) continue;
        const n = g.nations[r.ctrl];
        if (!n || !n.alive || !n.atWar.length) continue;
        const sky = skyOver(g, id);
        cs.push({ ctrl: r.ctrl, air: n.air, sky, skyAir: sky ? g.nations[sky].air : 0 });
      }
      this.coasts[z] = cs;
    }
    return cs;
  }

  /** The land air groups that put aircraft over a sea counted from them (air.ts groupsOver), each with what it puts there, in the order of the game's groups. */
  private groupsOf(z: number, S: { cover?: Record<string, number> }): { nat: string; n: number }[] {
    const g = this.g;
    let gs = this.groups[z];
    if (!gs) {
      gs = [];
      const near = nearRow(g, z);
      for (const f of g.airFleets) { if (!fieldNear(g, near, f.base, z)) continue; const n = groupOverSea(g, f, z, S); if (n > 0) gs.push({ nat: f.nat, n }); }
      this.groups[z] = gs;
    }
    return gs;
  }

  /** seaSide's total. */
  private side(z: number, nat: string, hostile: boolean): number {
    const g = this.g;
    const beside = hostile ? null : this.beside(z, nat);
    let ships = 0;
    for (const f of this.afloat(z)) if (hostile ? atWar(g, f.nat, nat) : allied(g, f.nat, nat) && (!beside || beside(f.nat))) ships += f.power;
    return ships + this.air(z, nat, hostile, beside) * NAVY.air.weight;
  }

  /** besideIn, from the picture's lists. */
  private beside(z: number, nat: string): ((x: string) => boolean) | null {
    const g = this.g;
    if (!farOn(g)) return null;
    const foes: string[] = [];
    const foe = (x: string): void => { if (!foes.includes(x)) foes.push(x); };
    for (const f of this.afloat(z)) if (f.power > 0 && atWar(g, f.nat, nat)) foe(f.nat);
    const S = skiesOver(g, z);
    if (S) {
      for (const x of this.groupsOf(z, S)) if (atWar(g, x.nat, nat)) foe(x.nat);
    } else if (g.seas[z].coast.length && seaSkyMul(g, z) !== 0) {
      for (const c of this.coastsOf(z)) {
        if (atWar(g, c.ctrl, nat)) { if (c.air > 0) foe(c.ctrl); }
        else if (c.sky && atWar(g, c.sky, nat) && c.skyAir > 0) foe(c.sky);
      }
    }
    if (!foes.length) return null;
    return (x) => x === nat || foes.some((e) => atWar(g, x, e));
  }

  /** airOver. */
  private air(z: number, nat: string, hostile: boolean, beside: ((x: string) => boolean) | null): number {
    const g = this.g, zone = g.seas[z];
    const side = (x: string): boolean => (hostile ? atWar(g, x, nat) : allied(g, x, nat) && (!beside || beside(x)));
    const S = skiesOver(g, z);
    if (S) {
      let s = 0;
      for (const x of this.groupsOf(z, S)) if (side(x.nat)) s += x.n;
      return (s / S.per) * zone.air;
    }
    if (!zone.coast.length) return 0;
    const wx = seaSkyMul(g, z);
    if (wx === 0) return 0;
    let s = 0;
    for (const c of this.coastsOf(z)) {
      if (side(c.ctrl)) s += c.air * (c.sky ? AIR.sky.seaFoe : 1);
      else if (c.sky && side(c.sky)) s += c.skyAir * AIR.sky.seaOwn;
    }
    return (s / zone.coast.length) * zone.air * wx;
  }
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
  const sea = g.seas[z].name, it = seaPlural(sea) ? 'them' : 'it';
  return `${seaHead(sea)} ${seaIs(sea)} closed to your troopships: the enemy has ${round(e.total)} there to your ${round(own.total)}, and ${opensAt(g, z, nat)} would open ${it}. Put a fleet to sea there, or ${foeCoast && e.air >= 0.5 ? `win the sky over the enemy's coast on ${it}` : `beat the enemy's fleets in ${it}`}.`;
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

/**
/**
 * The seas a nation's supply ships and troopships can use: those it can reach from a home port without crossing a sea closed to it. A picture of the seas, if given, answers which are closed.
 *
 * The far side (the director's ruling of 03:01Z, 22 November 2026, on Luzon reading fed with the sea behind it
 * shut): the supply ships sail from where the country makes its arms. A home port sends them out only where it is
 * joined by land to the capital; a home port across the water (Manila, Pearl Harbor, Truk, Malta) is a quay only
 * while the month's open seas join it to one of those, so it no longer feeds itself on its own sea alone. A country
 * that holds no capital on the chart, or has no home port its roads reach from it (Portugal: the chart gives Lisbon
 * none), has no such place, and its home ports read as they did. `homeward` is the far switch unless a reading asks
 * for the other form on the same month; with the far side off nothing changes. (seaStarts)
 *
 * (For the supply picture's kept lanes, game.ts: `asked`, if given, is filled in with every answer the walk took, -1
 * where it asked nothing, and `starts` are seaStarts' seas, in the same form, when the caller has them already.
 * Neither changes the answer.)
 *
 * `carrying`, if given, is another nation whose stores the ships carry (true reach, game.ts trueReach; the design
 * director's ruling of 13:58Z and 14:26Z, 25 November 2026): a sea closed to that nation is closed to every ship
 * carrying it stores, whatever the flag, so the walk takes the seas closed to either. The same harbours, the same
 * passages for the ships' own flag, the same homeward form. Absent, nothing changes.
 */
export function openSeas(g: Game, nat: string, seas?: SeaPicture, homeward: boolean = farOn(g), asked?: Int8Array, starts?: number[], carrying?: string): Uint8Array {
  const open = new Uint8Array(g.seas.length);
  // Whether a sea is closed to the nation is asked only of the seas its ships could reach, once each (-1: not asked yet).
  const shutAt = asked ?? new Int8Array(g.seas.length).fill(-1);
  const closedFor = (z: number, who: string): boolean => (seas ? seas.closed(z, who) : closedTo(g, z, who));
  const shut = (z: number): boolean => { if (shutAt[z] < 0) shutAt[z] = closedFor(z, nat) || (carrying !== undefined && carrying !== nat && closedFor(z, carrying)) ? 1 : 0; return shutAt[z] === 1; };
  const q: number[] = [];
  for (const z of starts ?? seaStarts(g, nat, seas, undefined, homeward)) if (!open[z] && !shut(z)) { open[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (!open[j] && !shut(j) && canPass(g, q[i], j, nat)) { open[j] = 1; q.push(j); }
  return open;
}

/**
 * The seas a nation's ships set out from (openSeas'), in the order it takes them, each as often as it comes up: the
 * seas off its own ports (on the far side, homeward, only those its roads join to its capital, where it has any),
 * its fleets' home seas off the map, the seas off the harbours it has taken that its roads reach, and the sea a
 * country off the map sends its convoys in by. `linkedOf`, if given, answers which provinces the nation's roads join
 * to its capital exactly as roadsHome walks them (game.ts keeps the walk from call to call).
 */
export function seaStarts(g: Game, nat: string, seas?: SeaPicture, linkedOf?: (nat: string) => Uint8Array, homeward: boolean = farOn(g)): number[] {
  const out: number[] = [];
  // Only a province the nation holds can be either kind of port below: with a picture of the seas, those alone are looked at.
  const coastal = seas ? seas.held(nat) : g.regions;
  const cap = g.nations[nat]?.capital ?? -1;
  const capHeld = cap >= 0 && g.regions[cap].ctrl === nat;
  // The provinces its roads join to its capital (null where it holds none): walked only where an answer turns on them.
  let linked: Uint8Array | null | undefined;
  const roads = (): Uint8Array | null => (linked !== undefined ? linked : (linked = capHeld ? (linkedOf ? linkedOf(nat) : roadsHome(g, nat)) : null));
  const own = (r: Region): boolean => r.port && r.owner === nat && r.ctrl === nat;
  // The far side: only the home ports its own roads reach from the capital send the ships out (where it has one).
  const home = homeward && coastal.some(own) && roads() && coastal.some((r) => own(r) && linked![r.id]) ? linked! : null;
  for (const r of coastal) if (own(r) && (!home || home[r.id])) out.push(...r.seas);
  for (const f of g.fleets) if (f.nat === nat && f.base < 0) out.push(f.home); // a base off the map is a home port too
  // The grand campaign: a harbour the nation has taken serves its supply ships as a home port does, once its own
  // roads reach the quay: the stores come down to it overland and go on by sea. So an army put across a narrow sea
  // from a conquered coast is fed from that coast, and not round by the harbours of the homeland.
  if (seaWar(g) && capHeld && coastal.some((r) => r.port && r.ctrl === nat && r.owner !== nat)) {
    const l = roads()!;
    for (const r of coastal) if (l[r.id] && r.port && r.ctrl === nat && r.owner !== nat && prizeOut(g, r, 'harbour') <= 0) out.push(...r.seas);
  }
  // So is the sea a country off the map sends its convoys in by.
  const off = g.nations[nat]?.offmap;
  if (off && g.nations[nat].alive && zoneOf(g, off.sea) >= 0) out.push(zoneOf(g, off.sea));
  return out;
}

/**
 * The provinces a nation's roads join to its capital: the capital, and every province held by it or a friend that a
 * road from there reaches through such provinces. `friend`, if given, answers for each province whether its holder is
 * the nation's friend (allied), as it would be asked here.
 */
export function roadsHome(g: Game, nat: string, friend?: Uint8Array): Uint8Array {
  const linked = new Uint8Array(g.regions.length);
  const road = [g.nations[nat].capital];
  linked[road[0]] = 1;
  for (let i = 0; i < road.length; i++) for (const j of g.regions[road[i]].nb) if (!linked[j] && (friend ? friend[j] === 1 : allied(g, nat, g.regions[j].ctrl))) { linked[j] = 1; road.push(j); }
  return linked;
}

/** True when ships can still reach this coast: an evacuation or a supply run needs at least one sea that is not closed. */
export const seaOpenAt = (g: Game, nat: string, r: Region): boolean => r.seas.some((z) => !closedTo(g, z, nat));

// ---- Armies at sea ----------------------------------------------------------------------------

/** How dangerous a sea is for a nation's troopships: 0 when no enemy ship or aircraft is at work there. */
export function seaRisk(g: Game, z: number, nat: string): number {
  const R = readingOf(g);
  if (!R) return riskNow(g, z, nat);
  let a = R.risk.get(nat);
  if (!a) R.risk.set(nat, (a = new Float64Array(g.seas.length).fill(NaN)));
  if (Number.isNaN(a[z])) a[z] = riskNow(g, z, nat);
  return a[z];
}
function riskNow(g: Game, z: number, nat: string): number {
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
  return seaPathTo(seaSearch(g, nat, from, o), to);
}

/** A search by sea from one coast (seaSearch): the cheapest way to every sea it can reach, each sea's cost and the sea before it on the way. */
export interface SeaSearch { cost: Float64Array; prev: Int16Array }

/**
 * (Speed, the far map.) seaPath's search from a coast, which reads nothing of where it is going: every sea's cost
 * from there, and the way back. Each sea's closing and risk are asked once a search (they were asked again for
 * every way into it; closedTo and seaRisk only read the war, and nothing changes during a search).
 */
export function seaSearch(g: Game, nat: string, from: Region, o: { max?: number; surprise?: boolean } = {}): SeaSearch {
  const R = readingOf(g);
  if (!R) return o.surprise ? surpriseSearch(g, nat, from, o) : searchNow(g, nat, from, o);
  const k = `${nat}|${from.id}|${o.max ?? 0}|${o.surprise ? 1 : 0}`;
  let S = R.searches.get(k);
  if (!S) R.searches.set(k, (S = searchNow(g, nat, from, o)));
  return S;
}
/**
 * (Speed, the far map: the playtest's finding 41.) A search for a landing by surprise reads nothing of the war but the
 * straits: every sea weighs the same and none is closed to it. So it is kept, by nation, coast and reach, and used
 * again while every strait lets that nation's ships through, or not, as it did (canPass, strait by strait). A
 * computer staff asks it of every harbour for every shore it wants: some twenty thousand times in a far month.
 * (Nothing that is handed a search changes it.)
 */
const SURPRISE = new WeakMap<object, Map<string, SeaSearch>>();
function surpriseSearch(g: Game, nat: string, from: Region, o: { max?: number; surprise?: boolean }): SeaSearch {
  let m = SURPRISE.get(g.seas);
  if (!m) SURPRISE.set(g.seas, (m = new Map()));
  let k = `${nat}|${from.id}|${o.max ?? 0}|`;
  for (const gt of g.gates) k += canPass(g, gt.a, gt.b, nat) ? '1' : '0';
  let S = m.get(k);
  if (!S) {
    if (m.size >= 50000) m.clear();
    m.set(k, (S = searchNow(g, nat, from, o)));
  }
  return S;
}
function searchNow(g: Game, nat: string, from: Region, o: { max?: number; surprise?: boolean }): SeaSearch {
  const N = g.seas.length;
  const cost = new Float64Array(N).fill(1e9), prev = new Int16Array(N).fill(-1), len = new Int16Array(N), done = new Uint8Array(N);
  const shutAt = new Int8Array(N).fill(-1), weightAt = new Float64Array(N);
  // (Each sea's closing and risk from one count of its sides: seaOdds.)
  const look = (z: number): void => { const s = seaOdds(g, z, nat); shutAt[z] = s.shut ? 1 : 0; weightAt[z] = 1 + 20 * s.risk; };
  const shut = (z: number): boolean => {
    if (o.surprise) return false;
    if (shutAt[z] < 0) look(z);
    return shutAt[z] === 1;
  };
  const weight = (z: number): number => {
    if (o.surprise) return 1; // (never 0: at least 1)
    if (shutAt[z] < 0) look(z);
    return weightAt[z];
  };
  // (Speed, the far map: the playtest's finding 41.) The next sea is taken from a heap, cheapest first and of two as
  // cheap the lower number first, as the scan of every sea took it: the same seas in the same order, the same ways.
  const heap = new SeaHeap();
  for (const z of from.seas) if (!shut(z)) { cost[z] = weight(z); len[z] = 1; heap.push(cost[z], z); }
  while (heap.size) {
    const c0 = heap.topCost(), a = heap.pop();
    if (done[a] || c0 !== cost[a] || !(cost[a] < 1e9)) continue;
    done[a] = 1;
    if (o.max && len[a] >= o.max) continue;
    for (const j of g.seas[a].adj) {
      if (done[j] || shut(j) || !canPass(g, a, j, nat)) continue;
      const c = cost[a] + weight(j);
      if (c < cost[j]) { cost[j] = c; prev[j] = a; len[j] = len[a] + 1; heap.push(c, j); }
    }
  }
  return { cost, prev };
}

/** A small heap of seas by cost, the lower number first between two as cheap (seaSearch). */
class SeaHeap {
  private c: number[] = [];
  private i: number[] = [];
  get size(): number { return this.c.length; }
  topCost(): number { return this.c[0]; }
  private less(a: number, b: number): boolean { return this.c[a] < this.c[b] || (this.c[a] === this.c[b] && this.i[a] < this.i[b]); }
  private swap(a: number, b: number): void { const c = this.c[a], i = this.i[a]; this.c[a] = this.c[b]; this.i[a] = this.i[b]; this.c[b] = c; this.i[b] = i; }
  push(cost: number, z: number): void {
    let k = this.c.length;
    this.c.push(cost); this.i.push(z);
    while (k > 0) { const p = (k - 1) >> 1; if (!this.less(k, p)) break; this.swap(k, p); k = p; }
  }
  pop(): number {
    const top = this.i[0], n = this.c.length - 1;
    this.swap(0, n); this.c.pop(); this.i.pop();
    let k = 0;
    for (;;) {
      const l = 2 * k + 1, r = l + 1;
      let m = k;
      if (l < n && this.less(l, m)) m = l;
      if (r < n && this.less(r, m)) m = r;
      if (m === k) break;
      this.swap(k, m); k = m;
    }
    return top;
  }
}

/** The cheapest way by sea from a coast to every sea it can reach (seaSearch). */
const seaWays = (g: Game, nat: string, from: Region, o: { max?: number; surprise?: boolean }): SeaSearch => seaSearch(g, nat, from, o);

/** The seas crossed in order on the way to one of them. */
function wayTo(prev: Int16Array, end: number): number[] {
  const path: number[] = [];
  for (let z = end; z >= 0; z = prev[z]) path.unshift(z);
  return path;
}

/** The way a search (seaSearch) found to a coast, as the seas crossed in order, or null if there is none. */
export function seaPathTo(s: SeaSearch, to: Region): number[] | null {
  let end = -1;
  for (const z of to.seas) if (s.cost[z] < 1e9 && (end < 0 || s.cost[z] < s.cost[end])) end = z;
  return end < 0 ? null : wayTo(s.prev, end);
}

/**
 * The way a landing sails, and the card of the sea off its beach where this had to read it.
 *
 * A crossing's way was chosen by the day's risk in each sea (`seaPath`), and its card then read what could come to
 * the last of them (`seaHold`): a staff weighed by one map and fought by another. Where a beach stands on two seas
 * it took the one with the less risk today, found that sea's card "can be closed" and waited, while the other
 * sea's card read open (Sandakan from Jolo in February 1942, seed 19, stop 1: the Celebes Sea for the Sulu Sea).
 * So on the far side, where a beach can be reached by more than one of its seas and a way to it crosses a sea of
 * the far side: of the ways (the safest to each of the beach's seas) the landing takes one whose card reads open,
 * and among those the least risk today. If none reads open it is the safest way as before, and a staff that heeds
 * the card waits. The old chart's crossings are read as they always were. (The design director's ruling of
 * 08:53Z, 22 November 2026.)
 */
function landingWay(g: Game, nat: string, from: Region, to: Region, surprise: boolean): { path: number[]; hold?: SeaHold } | null {
  if (!from.seas.length || !to.seas.length) return null;
  const W = seaWays(g, nat, from, { max: NAVY.landing.range, surprise });
  let end = -1;
  for (const z of to.seas) if (W.cost[z] < 1e9 && (end < 0 || W.cost[z] < W.cost[end])) end = z;
  if (end < 0) return null;
  const first = wayTo(W.prev, end);
  const ends = farOn(g) ? to.seas.filter((z) => W.cost[z] < 1e9) : [];
  if (ends.length < 2) return { path: first };
  const ways = ends.map((z) => ({ z, path: z === end ? first : wayTo(W.prev, z) }));
  if (!ways.some((w) => w.path.some((z) => z >= FAR_SEAS_FIRST))) return { path: first };
  let best: { path: number[]; hold: SeaHold } | null = null, bestCost = Infinity;
  for (const w of ways) {
    const hold = seaHold(g, nat, w.path, to);
    if (w.z === end && hold.open) return { path: first, hold };
    if (hold.open && W.cost[w.z] < bestCost) { best = { path: w.path, hold }; bestCost = W.cost[w.z]; }
  }
  return best ?? { path: first };
}

/** The seas a landing crosses in order, or null if it cannot sail: `landingWay`. */
export const landingPath = (g: Game, nat: string, from: Region, to: Region, surprise: boolean): number[] | null => landingWay(g, nat, from, to, surprise)?.path ?? null;

const liftKey = (g: Game, nat: string): string => `lift:${nat}:${g.turn}`;
/** How many more corps a nation's shipping can carry this month. */
export const liftLeft = (g: Game, nat: string): number => Math.max(0, g.nations[nat].sealift - (g.flags[liftKey(g, nat)] ?? 0));
/** Counts corps a nation's shipping has carried this month. */
export const useLift = (g: Game, nat: string, n = 1): void => { g.flags[liftKey(g, nat)] = (g.flags[liftKey(g, nat)] ?? 0) + n; };

/**
 * By the book a dated landing sails on the record's own shipping (rider b626 to ruling 234): the record shows the
 * shipping sufficed, since those landings were made on those dates. What this country ships or lands inside the call
 * draws nothing from the month's lift and is not refused for want of it; its staff's own landings use the lift as
 * before. Asked by the book's table (stations.ts) and for the landings of the book's great blow (ai.ts), at the
 * history dial's first two stops; by nothing else.
 */
let ownShips: string | null = null;
export function onOwnShipping<T>(nat: string, f: () => T): T {
  const was = ownShips;
  ownShips = nat;
  try { return f(); } finally { ownShips = was; }
}

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

/**
 * The far side's ship move: how many seas a corps shipped from port to port crosses in a month (navy.json 'sail',
 * the middle of the record's troop-convoy passages). 0 where there is no limit: the old chart, and the book's own
 * dated sailings, which are the record's (`onOwnShipping`).
 */
export const sailReach = (g: Game, nat?: string): number => (farOn(g) && (nat === undefined || ownShips !== nat) ? NAVY.sail?.reach ?? 0 : 0);

/** The fewest seas a nation's ships cross from a coast to every sea (1 for the coast's own seas; closed seas and shut straits kept out), and the sea before each on the way. */
interface ConvoySteps { steps: Int16Array; prev: Int16Array }
function convoySteps(g: Game, nat: string, from: Region): ConvoySteps {
  const R = readingOf(g);
  if (!R) return stepsNow(g, nat, from);
  const k = `${nat}|${from.id}`;
  let S = R.steps.get(k);
  if (!S) R.steps.set(k, (S = stepsNow(g, nat, from)));
  return S;
}
function stepsNow(g: Game, nat: string, from: Region): ConvoySteps {
  const N = g.seas.length, steps = new Int16Array(N), prev = new Int16Array(N).fill(-1);
  const q: number[] = [];
  for (const z of from.seas) if (!closedTo(g, z, nat)) { steps[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    for (const j of g.seas[a].adj) if (!steps[j] && !closedTo(g, j, nat) && canPass(g, a, j, nat)) { steps[j] = steps[a] + 1; prev[j] = a; q.push(j); }
  }
  return { steps, prev };
}
/** The fewest seas a nation's ships cross between two coasts, or 0 where there is no way by sea. */
export function seasBetween(g: Game, nat: string, from: Region, to: Region): number {
  const S = convoySteps(g, nat, from).steps;
  let best = 0;
  for (const z of to.seas) if (S[z] && (!best || S[z] < best)) best = S[z];
  return best;
}

/**
 * The way a corps is shipped from port to port, as the seas crossed in order, or null where it cannot sail there
 * this month. On the old chart it is the safest way, as it always was (`seaPath`). On the far side it is the safest
 * way among those a month's sailing covers; where the safest ways all run longer and a shorter one is open, the
 * convoy takes the shortest; and where the shortest is longer than a month's sailing there is none.
 */
export function sailWay(g: Game, nat: string, from: Region, to: Region): number[] | null {
  const reach = sailReach(g, nat);
  if (!reach) return seaPath(g, nat, from, to);
  const safe = seaPath(g, nat, from, to, { max: reach });
  if (safe) return safe;
  if (!from.seas.length || !to.seas.length) return null;
  const S = convoySteps(g, nat, from);
  let end = -1;
  for (const z of to.seas) if (S.steps[z] && (end < 0 || S.steps[z] < S.steps[end])) end = z;
  return end >= 0 && S.steps[end] <= reach ? wayTo(S.prev, end) : null;
}

/**
 * The far side's ship move: the port a corps bound further than a month's sailing goes to first. Of the ports its
 * side or an ally holds that it can be shipped to this month, the one fewest seas from where it is bound, and nearer
 * to it than the corps stands now. Null where there is none (and on the old chart, which has no such limit).
 */
export function sailLeg(g: Game, c: Corps, to: Region, rides = false): Region | null {
  if (!sailReach(g, c.nat)) return null;
  const from = g.regions[c.reg];
  const S = convoySteps(g, c.nat, to).steps;
  const off = (p: Region): number => { let d = 0; for (const z of p.seas) if (S[z] && (!d || S[z] < d)) d = S[z]; return d; };
  const now = off(from);
  if (!now) return null;
  const ports = g.regions.filter((p) => p.port && p.id !== from.id && p.id !== to.id && allied(g, c.nat, p.ctrl)).map((p) => ({ p, d: off(p) })).filter((x) => x.d > 0 && x.d < now).sort((a, b) => a.d - b.d || a.p.id - b.p.id);
  for (const { p } of ports) if (!whyNotQuay(g, c, p, rides) && sailWay(g, c.nat, from, p)) return p;
  return null;
}

/** Where a corps goes first when a port is refused it only for lying beyond a month's sailing (`sailLeg`); null for every other refusal, and where it can sail there. */
export function sailFirst(g: Game, c: Corps, to: Region, rides = false): Region | null {
  if (!sailReach(g, c.nat) || whyNotQuay(g, c, to, rides) || sailWay(g, c.nat, g.regions[c.reg], to)) return null;
  return sailLeg(g, c, to, rides);
}

/** Why a corps cannot be shipped to a friendly port, or null if it can. */
export function whyNotShip(g: Game, c: Corps, to: Region, rides = false): string | null {
  const why = whyNotQuay(g, c, to, rides);
  if (why) return why;
  const from = g.regions[c.reg];
  if (sailWay(g, c.nat, from, to)) return null;
  // (The far side's ship move: a port further off than a month's sailing is said so, with the port to ship to first.)
  const reach = sailReach(g, c.nat), seas = reach ? seasBetween(g, c.nat, from, to) : 0;
  if (!seas) return 'The enemy holds the sea between.';
  const leg = sailLeg(g, c, to, rides);
  return `${placeName(to.name, true)} is ${seas} seas away, and a troop convoy sails ${reach} seas in a month. ${leg ? `Ship them to ${placeName(leg.name)} first, and on from there next month.` : 'No port in friendly hands lies on the way.'}`;
}

/** Why a corps cannot be shipped to a friendly port, the sea between left aside (`whyNotShip`). */
function whyNotQuay(g: Game, c: Corps, to: Region, rides = false): string | null {
  const n = g.nations[c.nat], from = g.regions[c.reg];
  if (n.sealift <= 0) return `${n.name} has no shipping to move an army by sea.`;
  if (c.kind === 'fort' || c.ap < Math.min(4, c.apMax)) return 'Only a corps that has not moved this month can embark.';
  // (The place at fault is named, so a refused order by sea says what is wrong with it: the UI director, build 1638.)
  if (to.id === from.id) return `They are in ${placeName(to.name)} already.`;
  for (const p of [to, from]) if (!p.port) return `${placeName(p.name, true)} has no port: troops are shipped from one port to another.`;
  if (!allied(g, c.nat, to.ctrl)) return `${to.name} is not in friendly hands.`;
  if (thornGround(g, to, c.nat)) return `${g.nations[to.owner].name} keeps its own army: no corps of its allies is shipped there.`;
  // (By the book a computer staff ships nobody into a harbour beyond the record's line, as it lands nobody there: a
  // place its side holds once its span is over is left, not reinforced. line.ts; ruling 224, b390. Never the player's.)
  if (lineShut(g, c.nat, to)) return `By the book, ${to.name} lies beyond the record's line this month.`;
  // A harbour blocked and its quays blown up takes no ship until it is mended (prizes.ts).
  for (const p of [from, to]) { const out = prizeOut(g, p, 'harbour'); if (out > 0) return `The harbour of ${p.name} is wrecked: no ship will use it for ${out === 1 ? 'a month' : `${out} months`} more.`; }
  if (hostileUnits(g, to, c.nat).length) {
    if (siegeRules(g) && !harbourOpen(g, to)) return `The enemy holds the quays of ${to.name}: nothing can be run into the town by sea.`;
    // A town under siege with its harbour still open: ships can run more troops into the garrison, if it has room.
    if (!siegeRules(g)) return `${to.name} is not in friendly hands.`;
    if (c.kind === 'eng') return `The enemy holds the country round ${to.name}: only fighting troops are run into the town.`;
    if (housed(g, to) >= roomIn(g, to)) return `${to.name} is under siege, and the town has no room for another corps.`;
  }
  const steep = whyBarred(g, c, to);
  if (steep) return steep;
  // (A harbour holds five corps of a side, and an island of the Pacific its own room in men, said in its own words:
  // islands.ts `fits`. On an island the quay counts as a march and a landing count, engineers apart: a ship must not be
  // refused where a march or a landing would be let in. Every other harbour counts all the corps of a side on its quay,
  // as it always did.)
  if (!fits(g, to, c.nat, [c], { rows: placed(g, to, c.nat).length })) return roomRefusal(g, to, [c]) ?? roomSays(g, to) ?? `${to.name} has no room for another corps.`;
  // (A row that goes with a corps already shipped there this month sails in that corps' lift: `rides`, rider b604 to ruling 234.)
  if (!rides && ownShips !== c.nat && liftLeft(g, c.nat) < 1) return 'All the shipping is in use this month.';
  return null;
}

/** Whether a corps can be shipped to a port this month: `whyNotShip` without its words (the list of ports a corps can sail to asks this of every port). */
export const canShip = (g: Game, c: Corps, to: Region, rides = false): boolean => !whyNotQuay(g, c, to, rides) && !!sailWay(g, c.nat, g.regions[c.reg], to);

/** Ships a corps from one friendly port to another. It takes the whole month, and the enemy may find the convoy. */
export function shipCorps(g: Game, c: Corps, to: Region, rides = false): boolean {
  if (!canShip(g, c, to, rides)) return false;
  const path = sailWay(g, c.nat, g.regions[c.reg], to)!;
  const lost = crossing(g, [c], path, false);
  if (!rides && ownShips !== c.nat) g.flags[liftKey(g, c.nat)] = (g.flags[liftKey(g, c.nat)] ?? 0) + 1;
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
  if (bar.kind === 'far') return `${to.name} is too far for a landing from ${from.name}: troopships cross ${NAVY.landing.range} seas at most, and ${bar.round !== undefined ? `the safe way round ${seaWhat(g.seas[bar.round].name)} is longer` : bar.seas > NAVY.landing.range ? `this is ${bar.seas}` : 'the safe way round is longer'}.`;
  if (bar.kind === 'gate') return `The ${g.gates[bar.gate].name} ${/s$/.test(g.gates[bar.gate].name) ? 'are' : 'is'} shut to your ships, and no other way leads to ${to.name}.`;
  return `No sea joins ${from.name} to ${to.name}.`;
}

/** A landing made in the month war is declared catches the defenders unready and sails before any fleet can stop it. */
export const surpriseOn = (g: Game, nat: string, to: Region): boolean => g.flags[`war:${nat}:${to.ctrl}`] === g.turn;

export function whyNotLand(g: Game, cs: Corps[], to: Region): string | null {
  if (!cs.length) return 'No corps chosen.';
  const nat = cs[0].nat, n = g.nations[nat], from = g.regions[cs[0].reg];
  for (const c of cs) { const steep = whyBarred(g, c, to); if (steep) return steep; }
  // Guns and engineers land on no enemy shore by themselves (ruling 211(a), the grand campaign).
  if (gunsRule(g) && !cs.some(fights) && !allied(g, nat, to.ctrl) && !gunsMayFollow(g, cs[0], to)) return aloneLine(cs);
  // (By the book a computer staff lands nobody beyond the record's line: line.ts. Never the player's.)
  if (lineShut(g, nat, to)) return `By the book, ${to.name} lies beyond the record's line this month.`;
  if (airborne(cs)) {
    if (cs.some((c) => c.nat !== nat || c.reg !== from.id)) return 'An airborne force takes off from one place.';
    if (!allied(g, nat, from.ctrl)) return 'Airborne troops take off from friendly airfields.';
    if (cs.some((c) => c.ap < Math.min(4, c.apMax))) return 'Only corps that have not moved this month can emplane.';
    if (allied(g, nat, to.ctrl) || !atWar(g, nat, to.ctrl)) return `${to.name} is not enemy ground.`;
    if (stationBars(g, nat, to)) return whyStationBars(g, nat, to);
    if (!from.seas.some((z) => to.seas.includes(z))) return `${to.name} is out of reach: the transport aircraft can cross one sea, from a coast on it to another.`;
    if (g.flags[dropKey(g, nat)]) return 'The transport aircraft can carry one airborne assault a month.';
    if (skyDenies(g, nat, to.id)) return `${g.nations[to.ctrl].adj} fighters still hold the sky over ${to.name}, and no transport aircraft can fly under them. Win the sky there first.`;
    return islandFull(g, cs, to);
  }
  if (n.sealift <= 0) return `${n.name} has no shipping for a landing.`;
  if (cs.some((c) => (c.nat !== nat && !sailsWith(g, nat, c)) || c.reg !== from.id)) return 'A landing force sails from one port.';
  if (!from.port) return 'A landing force has to embark at a port.';
  if (cs.some((c) => c.kind === 'fort' || c.ap < Math.min(4, c.apMax))) return 'Only corps that have not moved this month can embark.';
  if (!to.seas.length) return `${to.name} has no coast.`;
  if (allied(g, nat, to.ctrl) || cs.some((c) => !atWar(g, c.nat, to.ctrl))) return `${to.name} is not enemy ground.`;
  // (Nor on ground the enemy's troops stand on by agreement with an owner any country of the force is at peace with: far.ts.)
  for (const c of cs) if (stationBars(g, c.nat, to)) return whyStationBars(g, c.nat, to);
  if (cs.length > STACK) return `No more than ${STACK} corps can go ashore in one place.`;
  // (A row that goes with a corps of the force sails in that corps' lift: rider b604 to ruling 234, stations.ts.)
  // (Nor is a dated landing of the book, on the record's own shipping: rider b626.)
  if (ownShips !== nat && liftLeft(g, nat) < cs.length - riding(g, cs).length) return liftLeft(g, nat) ? `There is shipping for only ${liftLeft(g, nat)} more corps this month.` : 'All the shipping is in use this month.';
  // (The landing ships in the Pacific carry no more of a country's troops to an enemy shore there than the record's did:
  // a second count, read for the Pacific's ground alone. Rider b614 to ruling 234; pacific.ts shoreLift. Not read with
  // the far side off.)
  if (!shipsFor(g, cs, to)) return shoreLiftWhy(n, shoreLift(g, nat, to)!, worthOf(cs));
  if (!seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: surpriseOn(g, nat, to) })) return laneWhy(g, nat, from, to, laneBar(g, nat, from, to, surpriseOn(g, nat, to)));
  // Troopships cannot cross under an enemy sky. Only surprise, or winning the sky over the beach, gets them there.
  if (!surpriseOn(g, nat, to) && skyDenies(g, nat, to.id)) return `${g.nations[to.ctrl].adj} fighters still hold the sky over ${to.name}, and no invasion fleet can cross under them. Win the sky there first.`;
  // (The island's room is asked last, of a force that could otherwise sail: every older refusal is said as it was.)
  return islandFull(g, cs, to);
}

/**
 * An island of the Pacific takes no more corps ashore than it has room for, those of the force's side already on it
 * counted and engineers apart, as a march counts them (rulings 173(b) and 179(s): "the limit of an open beach is the
 * island's room of one corps and what the corps carries"). The island's own sentence where the force is too many
 * ("Wake has room for one corps."), or null: always null for ground that is no island of the table, and with the far
 * side off.
 */
const islandFull = (g: Game, cs: Corps[], to: Region): string | null => (islandOf(g, to) && !roomForAll(g, to, cs) ? roomRefusal(g, to, cs) ?? roomSays(g, to) : null);

/** What a force is worth in divisions: size times strength. */
const worthOf = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
/** True when the Pacific's landing ships will carry a force to a shore, or are not asked (pacific.ts shoreLift). */
export function shipsFor(g: Game, cs: Corps[], to: Region): boolean {
  const ships = cs.length ? shoreLift(g, cs[0].nat, to) : null;
  return !ships || worthOf(cs) <= ships.left + 1e-6;
}
/** Why the landing ships will not carry a force to an enemy shore of the Pacific's ground (pacific.ts shoreLift), in
 * plain words: the UI director's lines (ui-director/words-pacific-news.md, the landing ships' refusal). */
function shoreLiftWhy(n: Nation, ships: ShoreLift, worth: number): string {
  const due = turnToDate(ships.due).label;
  const menOf = (w: number): string => (Math.round((w * MEN_PER_DIV) / 100) * 100).toLocaleString('en-US');
  if (ships.of <= 0) return ships.first
    ? `There are no landing ships in these waters for ${n.adj} troops yet. The first are due in ${due}.`
    : `There are no landing ships in these waters for ${n.adj} troops now. More are due in ${due}.`;
  if (ships.left * MEN_PER_DIV < 50) return `The landing ships in these waters have carried all the ${n.adj} troops they can for now. More are due in ${due}.`;
  return `This force has ${menOf(worth)} men, and the landing ships in these waters can carry only ${menOf(ships.left)} more ${n.adj} troops before ${turnToDate(ships.next).label}.`;
}

/**
 * The joint crossing (ruling 233(w); the grand campaign, computer staffs only): a corps of a computer-run ally may
 * sail in a computer staff's landing force from the same quay, as Torch, Sicily and Salerno were Anglo-American
 * landings under one command (ai.ts `crossSea` forms such a force, open.json 'descent.joint'). A player's corps are
 * never drawn into one, a corps lent to another army keeps to that army, and a player's own landing force stays one
 * nation's.
 */
export const sailsWith = (g: Game, nat: string, c: Corps): boolean =>
  g.flags.open === 1 && g.human !== nat && g.human !== c.nat && c.joint === undefined && allied(g, nat, c.nat);

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
    // (A fleet in passage to a new base comes out against nothing on its way: release 2, the far chart.)
    if (!atWar(g, f.nat, nat) || !surfaceShips(f) || inPassage(f)) continue;
    if (atSea(f)) { if (!path.includes(f.zone) && near.has(f.zone)) s += fleetPower(g, f); continue; }
    if (keepsHarbour(g, f.nat)) continue;
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
    // (A fleet in passage to a new base keeps on its way and answers no landing: release 2, the far chart.)
    if (!atWar(g, f.nat, nat) || !surfaceShips(f) || inPassage(f) || (atSea(f) && f.zone === last)) continue;
    const hand = byHand(g, f.nat, 'navy');
    if (!hand && (f.duty === 'raid' || tiedNow(g, f)) && f.station !== last) continue;
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
  if (e.ships >= 0.5) lines.push({ label: `Enemy ships ${seaWhere(name)} now`, value: round(e.ships), effect: 'at sea there already' });
  if (come >= 0.5) lines.push({ label: 'Enemy ships that could come', value: round(come), effect: 'at their full strength: every fleet within a month of that sea that his admirals would send' });
  if (kept >= 0.5) lines.push({ label: 'Enemy ships your aircraft keep away', value: round(kept), effect: 'no admiral takes his fleet under your bombers, except to save his own homeland' });
  if (shy >= 0.5) lines.push({ label: 'Enemy ships that will not risk it', value: round(shy), effect: 'none of them will face what you have in that sea, while it stays there' });
  if (e.air >= 0.5) lines.push({ label: `Enemy aircraft over ${seaWhat(name)}`, value: round(e.air), effect: 'count against the supply ships too' });
  lines.push({ label: `Your ships and aircraft ${seaWhere(name)}`, value: `${round(own.ships)} and ${round(own.air)}`, effect: need <= 0 ? 'the enemy has too little in reach to close it' : contested ? `more than the ${round(need)} that keep it open today, and fewer than the ${round(foe)} he can bring` : open ? `more than the ${round(need)} that keep it open` : `${round(need)} would keep it open: one for every ${NAVY.closed.ratio} of his` });
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
  const lines: TicketLine[] = [{ label: 'The way', value: path.map((z, i) => (i ? `then ${seaWhat(name(z))}` : seaWhat(name(z)))).join(', '), effect: path.length > 1 ? 'every sea crossed is a chance to be found' : undefined }];
  for (const d of ds) {
    if (d.ships >= 0.5) lines.push({ label: `Enemy ships at sea ${seaWhere(name(d.z))}`, value: round(d.ships), effect: 'hunt the troopships' });
    if (d.air >= 0.5) lines.push({ label: `Enemy aircraft over ${seaWhat(name(d.z))}`, value: round(d.air), effect: 'count for half against ships under way' });
    if (d.last && d.sortie >= 0.5) lines.push({ label: 'Enemy ships that will come out', value: round(d.sortie), effect: 'from harbours on these seas and the seas next to them' });
    if (d.last) lines.push({ label: `The sky over ${to.name}`, value: sky === 'ours' ? 'yours' : sky === 'denied' ? 'held by enemy fighters' : 'nobody holds it', effect: sky === 'ours' ? `keeps ${Math.round((1 - AIR.sky.seaFoe) * 100)}% of them off the troopships` : sky === 'denied' ? 'no invasion fleet can cross under them' : fly ? 'no help to either side' : 'out of reach of your fighters from any airfield you hold' });
    if (d.hunters > 0 || d.last) lines.push({ label: `Your ships and aircraft ${seaWhere(name(d.z))}`, value: `${round(d.own.ships)} and ${round(d.own.air)}`, effect: d.own.total >= 0.5 ? 'each counts double against the hunters' : 'nothing covers the troopships' });
  }
  // (A share that rounds to nothing is said in words, as the chances at the ends are: the UI director, build 1638.)
  const lost = Math.round(risk * NAVY.intercept.loss * 100);
  lines.push({ label: 'Caught at sea', value: `${Math.round(risk * 100)} in 100`, effect: risk > 0 ? (lost > 0 ? `about ${lost}% of the men lost` : 'almost none of the men lost') : 'nothing is in a position to stop the crossing' });
  const helps: CrossingHelp[] = [];
  if (risk > 0) {
    // The invader's own fleets that could stand in the worst sea and are not there now.
    const cover = fleetsOf(g, nat).filter((f) => surfaceShips(f) > 0 && !(f.zone === worst.z && atSea(f)) && fleetReach(g, f).includes(worst.z));
    const more = cover.reduce((s, f) => s + fleetPower(g, f), 0);
    const foes = ds.some((d) => d.ships >= 0.5 || d.sortie >= 0.5);
    const tell = (id: CrossingHelp['id'], text: string, r: number): void => { if (r < risk - 0.005) helps.push({ id, text, risk: r }); };
    if (fly && skyWon !== null) tell('sky', fly.how === 'now' ? `Win the sky over ${to.name} with a fight for the sky flown by your fighters` : `Bring fighters up to the airfields of ${g.regions[fly.base].name} and win the sky over ${to.name}`, skyWon);
    if (more >= 1) tell('sea', `Send ${cover.length === 1 ? cover[0].name : `your ${cover.length} fleets in reach`} to patrol ${seaWhat(name(worst.z))}`, caughtIn(seaDangers(g, nat, path, to, { more, at: worst.z })));
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
  const way = landingWay(g, nat, g.regions[cs[0].reg], to, surprise)!;
  const path = way.path;
  return {
    why: null, surprise, path, risk: surprise ? 0 : landingRisk(g, nat, path, to), defended,
    ...(surprise ? {} : { account: crossingAccount(g, nat, path, to) }),
    hold: way.hold ?? seaHold(g, nat, path, to),
    sky: sky !== null && allied(g, sky, nat), sortie: surprise ? 0 : sortieAgainst(g, nat, path),
    assess: defended ? assess(g, cs, to, { sea: surprise ? NAVY.landing.surprise : assaultStrength(g, nat), surprise }) : null,
  };
}

export interface Landing { ok: boolean; why?: string; taken?: boolean; lostAtSea?: number; report?: BattleReport | null; surprise?: boolean; air?: boolean }

/**
 * Reading only: a probe is told each landing that sails, once its crossing is run: who, from which coast, to which
 * shore, by which seas in order, and what the sea took. It changes nothing, and no rule reads it.
 */
export const landWatch: { tell: ((e: { nat: string; from: Region; to: Region; path: number[]; surprise: boolean; force: Corps[]; lostAtSea: number }) => void) | null } = { tell: null };

/**
 * Lands corps on an enemy coast. If it is defended they fight their way ashore, at a disadvantage unless the enemy is surprised.
 * A row that goes with a corps of the force (stations.ts, rider b604 to ruling 234) lands in that corps' lift, and is
 * its staff's own once ashore. A computer's staff takes such rows along where they stand with the corps on the quay;
 * the player's force is his to choose, and a dated attack of the book brings its own (`alone`).
 */
export function landCorps(g: Game, cs: Corps[], to: Region, fight?: (cs: Corps[], to: Region) => BattleReport, alone = false): Landing {
  if (!alone && cs.length && cs[0].nat !== g.human) {
    const more = alongside(g, cs);
    if (more.length && whyNotLand(g, [...cs, ...more], to) === null) cs = [...cs, ...more];
  }
  const why = whyNotLand(g, cs, to);
  if (why) return { ok: false, why };
  const nat = cs[0].nat, from = g.regions[cs[0].reg];
  const held = to.ctrl;
  // (Whose army held the field there before the landing, to tell a shore the landing wins: on the far side, and on the
  // old chart under Units decide at a stop that keeps no book. Below.)
  const field = farOn(g) || unitsFree(g) ? fieldCtrl(g, to) : null;
  const air = airborne(cs);
  const surprise = !air && surpriseOn(g, nat, to);
  const rode = air ? [] : riding(g, cs);
  let lostAtSea = 0;
  if (air) g.flags[dropKey(g, nat)] = 1;
  else {
    const path = landingPath(g, nat, from, to, surprise)!;
    // (What the Pacific's landing ships loaded is counted as it sailed, before the crossing's losses: pacific.ts.)
    useShoreLift(g, nat, to, worthOf(cs));
    lostAtSea = crossing(g, cs, path, surprise, to);
    if (ownShips !== nat) g.flags[liftKey(g, nat)] = (g.flags[liftKey(g, nat)] ?? 0) + cs.length - rode.length;
    for (const z of to.seas) g.flags[`landed:${z}:${nat}`] = g.turn;
    // (And the ground itself, for the coast watch: a landing on this enemy's homeland is not news to come.)
    g.flags[`landedAt:${to.id}:${nat}`] = g.turn;
    landWatch.tell?.({ nat, from, to, path, surprise, force: cs, lostAtSea });
  }
  let taken = true;
  let report: BattleReport | null = null;
  if (hostileUnits(g, to, nat).length) {
    // (By the book a landing the record made on an empty beach meets its fight inland, at the record's figures: stations.ts, rider b580.)
    report = fight ? fight(cs, to) : resolveBattle(g, cs, to, landingOpts(g, cs, to));
    taken = report.taken;
  } else {
    g.week = 1;
    for (const c of cs) relocate(g, c, to);
    // Ashore with no army to meet them: the province is theirs unless its town is held, and then the town is under siege.
    arrive(g, to, nat);
  }
  for (const c of cs) if (!c.dead) c.ap = 0;
  if (taken) for (const c of rode) if (!c.dead) c.hold = false;
  // (The far side: a shore won from the sea is marked with its month, as the ground changes hands or the army is
  // first laid round a town that holds. The staff's answer by land reads it to tell a beach from ground an enemy
  // marched into: ai.ts `beach`, the design director's B11 of 25 November 2026. The mark above, `landedAt`, is
  // written as the landing sails, whatever becomes of it: a landing thrown back in the month the province then
  // fell to a march read as a beach by it, and a second wave put ashore round a town still holding wiped the first
  // wave's month. The skeptic's finding. Ground won by a drop from the air is marked the same way, on the same
  // condition: the design director's ruling of 13:39Z, 25 November 2026, on B11's eighth point, "a drop from the air
  // is answered as a landing: mark it like a beach, so b641's landing answer governs it, not B11's march overland".
  // As built a drop left no mark, and the held rows on its front answered it as if the enemy had marched in.
  // With the far side off it is marked on the old chart too, a landing and a drop alike, under Units decide at a
  // stop that keeps no book (history.ts `unitsFree`), where the held rows there answer an attack by land: the
  // design director's ruling of 15:09Z, 25 November 2026, point 4, "written under Units decide at stops 3 to 5, so a
  // beach reads as a landing (b641) and a drop as one, never as an entry by land". Nothing is marked with the far
  // side off and Units decide off, nor at the book's stops with the far side off.)
  if (field !== null && taken && (to.ctrl !== held || fieldCtrl(g, to) !== field)) g.flags[`shoreWon:${to.id}`] = g.turn;
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
  // (Ground the enemy's troops stand on by agreement with an owner the nation is at peace with is shut to its army, and nothing is reached through it: far.ts `stationBars`.)
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!byLand[j] && atWar(g, nat, g.regions[j].ctrl) && !stationBars(g, nat, g.regions[j])) { byLand[j] = 1; q.push(j); }
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
    : barred === 'sea' ? `${seaHead(g.seas[shut].name)} ${seaIs(g.seas[shut].name)} closed to your troopships: the enemy is much the stronger there.`
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
    // (Nor a shore the enemy's troops stand on by agreement with an owner the nation is at peace with: far.ts `stationBars`.)
    if (byLand[r.id] || !r.seas.length || !atWar(g, nat, r.ctrl) || !g.nations[r.ctrl]?.alive || allysOwn(r) || stationBars(g, nat, r)) continue;
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
/**
 * True when a nation has come ashore from the sea on another's homeland since their war began, and holds or stands
 * on that ground still. A landing on some other coast of the same sea, or ground of his homeland taken by land, is not it.
 */
function landedOn(g: Game, att: string, d: string): boolean {
  const since = g.flags[`war:${att}:${d}`] ?? g.flags[`war:${d}:${att}`] ?? 0;
  return g.regions.some((r) => r.owner === d && !r.overseas && (g.flags[`landedAt:${r.id}:${att}`] ?? -1) >= since
    && (r.ctrl === att || r.units.some((u) => u.nat === att && !u.dead)));
}

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
      // (one search from the quay for all the coasts: seaPath's, which reads nothing of where it goes)
      const from = seaSearch(g, att, q, { max: NAVY.landing.range });
      for (const c of coasts) {
        const path = seaPathTo(from, c);
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
    // A warning never comes after its event (ruling 229(e)): once this attacker has come ashore from the sea on the
    // enemy's homeland in this war and stands on it still, the landing is not news to come, and nobody is told.
    if (landedOn(g, att, d)) continue;
    const force = seen.corps === 1 ? 'one corps' : `${seen.corps} corps`;
    if (d === g.human) warning(g, 'Coast watch', `The coast watch reports ${n.adj} shipping gathering at ${seen.quay.name}, with troops on the quays: enough to carry ${force}. ${seen.coast.name} is within their reach, and a landing could come as soon as next month.`, d, seen.coast.id, att);
    else if (att === g.human) headline(g, 'info', `${x.adj} coast watchers have seen your shipping and troops gather at ${seen.quay.name}: ${x.name} expects a landing and is manning its harbours.`, att, seen.quay.id);
  }
}

// ---- Fleet orders -------------------------------------------------------------------------------

/** Where a fleet sails from, for a far-side submarine group how far its boats raid (`Fleet.reach`), and what sails. */
type Sailing = Pick<Fleet, 'nat' | 'base' | 'home'> & { reach?: number } & Partial<Pick<Fleet, 'ships'>>;

/**
 * Seas a fleet can be sent to: those within reach of its base, through straits open to it. `calm` reads the reach as
 * if no gale blew (release 2, heavyseas.ts). `raid`: the seas it can be sent to raid in, which for a submarine group
 * of the far side's beats run as far as the record's boats patrolled from that base (`Fleet.reach`, set by
 * placeFarBoats), and for the player's own boats as far as his navy's group raids from that base (ownBoatsReach, read
 * as the month stands); for every other fleet the same seas as for any errand. (The join of 24 November 2026: release
 * 2's `calm` keeps the third place its callers know, and the far side's `raid` takes the fourth.)
 */
export function fleetReach(g: Game, f: Sailing, calm = false, raid = false): number[] {
  const far = !raid ? undefined : f.nat === g.human ? ownBoatsReach(g, f) : f.reach;
  const range = far !== undefined ? Math.max(NAVY.range, far) : NAVY.range;
  const d = new Int16Array(g.seas.length).fill(-1);
  const q: number[] = [];
  // (Release 2, heavyseas.ts) A sea in a gale can be sailed into but not on through this month; `calm` reads the reach
  // as if no gale blew (a tied fleet's beat is its beat whatever the weather). Nothing with that switch off.
  const gales = calm ? null : galeSeas(g);
  for (const z of f.base >= 0 ? g.regions[f.base].seas : [f.home]) if (d[z] < 0) { d[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) {
    if (d[q[i]] >= range || gales?.has(q[i])) continue;
    for (const j of g.seas[q[i]].adj) if (d[j] < 0 && canPass(g, q[i], j, f.nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  }
  return q;
}

/**
 * How many seas off its base each sea lies for a fleet, counted as its reach is (its base's own seas 1, through
 * straits open to it) and as far as the chart runs: 0 for a sea it cannot sail to.
 */
function seaSteps(g: Game, f: Sailing): Int16Array {
  const d = new Int16Array(g.seas.length);
  const q: number[] = [];
  for (const z of f.base >= 0 ? g.regions[f.base].seas : [f.home]) if (!d[z]) { d[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (!d[j] && canPass(g, q[i], j, f.nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

/**
 * Whether a shipping lane is one for a fleet's page (release 2, the far chart; the design director's ruling of 04:33Z,
 * 25 November 2026, 3, finding 50 of the 1.5.1 playtest): on the whole world only a lane within two months' steaming of
 * its base (two reaches of seas, through straits open to it), or the one it escorts, is listed. A fleet in passage
 * lists none. Every lane elsewhere.
 */
export function laneNear(g: Game, f: Fleet, lane: { id: string; zones: number[] }): boolean {
  if (!farOn(g) || (f.stance === 'escort' && f.lane === lane.id)) return true;
  if (inPassage(f)) return false;
  const d = seaSteps(g, f);
  return lane.zones.some((z) => d[z] > 0 && d[z] <= 2 * NAVY.range);
}

/**
 * Whether a fleet's tie to its home and its beat has lapsed (ruling 233 rider b598(2), for every tied fleet in every
 * campaign): the month its home port is lost (no longer in its own side's hands, as newBase reads a fallen base), or
 * once its beat is out of reach from the port it is in. From then its admiral places it as any other fleet. The home
 * it is tied to is its row's: a tied fleet away from that home while the enemy holds it has lost it too (so a save
 * from before this rule, with a fleet moved from its fallen home and still tied, is mended once loaded).
 */
export function tieLapses(g: Game, f: Fleet): boolean {
  if (!f.tied) return false;
  if (f.base >= 0 && !allied(g, f.nat, g.regions[f.base].ctrl)) return true;
  const row = NAVY.fleets.find((r) => r.nat === f.nat && r.name === f.name);
  const home = row?.base ? g.byName[row.base] : undefined;
  if (home && home.id !== f.base && !allied(g, f.nat, home.ctrl)) return true;
  return !fleetReach(g, f, true).includes(f.station);
}

/**
 * Whether a fleet is tied as its enemies and its own admirals read it this month (rider b598(2)): a tie that has
 * lapsed since its own month began (its home taken by a nation that moves after it) no longer holds it, though
 * the flag is cleared only at its own next month's start.
 */
export const tiedNow = (g: Game, f: Fleet): boolean => f.tied && !tieLapses(g, f);

/**
 * Sends a fleet to a sea to patrol or raid there, or back to harbour. In the grand campaign it can also be sent to hunt
 * submarines, or (with the lane named) to sail as the escort of a shipping lane that crosses that sea.
 */
export function sail(g: Game, f: Fleet, zone: number, stance: Stance, lane?: string): boolean {
  // (A fleet in passage keeps on its way, beaten, declining battle or worn: nothing sends it to a sea or straight into a
  // harbour. Its owner can turn it back (turnBack). Release 2, the far chart.)
  if (inPassage(f)) return false;
  if ((stance === 'escort' || stance === 'hunt') && !seaWar(g)) return false;
  if (stance === 'escort' && !seaRoutes(g).some((r) => r.id === lane && r.zones.includes(zone))) return false;
  if (stance !== 'port' && (!shipCount(f.ships) || !fleetReach(g, f, false, stance === 'raid').includes(zone))) return false;
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
  if (inPassage(f)) return passageWhy(g, f);
  // (A lane may be named in the plural: "the Arctic convoys" feed, "the Atlantic lane" feeds.)
  const many = /s$/.test(rt.name);
  if (!to?.alive || (rt.to !== f.nat && !allied(g, f.nat, rt.to))) return `${cap(rt.name)} ${many ? 'feed' : 'feeds'} ${to?.name ?? rt.to}, which is no ally of ${g.nations[f.nat].name}.`;
  if (!to.atWar.length || !g.nations[f.nat].atWar.length) return `Not under attack: nobody is at war over ${rt.name}.`;
  const stop = laneStopped(g, lane);
  if (stop) return `Nothing sails on ${rt.name} now. ${stop.why}`.trim();
  if (escortWorth(f.ships) <= 0) return `The ${f.name} has nothing that can guard a convoy: escorts are destroyers, carriers and cruisers.`;
  const reach = fleetReach(g, f);
  if (!rt.zones.some((z) => reach.includes(z))) return `${cap(rt.name)} ${many ? 'are' : 'is'} out of reach of ${fleetName(f)} from ${f.port}.`;
  return null;
}

/**
 * The sea of a lane a fleet sails in as its escort: of the lane's seas in its reach, the one where the raiders are
 * thickest (the first in its reach on a tie), or -1 where none is in reach. It moves nothing: escort reads it to sail
 * the fleet there, and escortGain to price the fleet bound to the lane without moving it.
 */
function escortSea(g: Game, f: Fleet, rt: SeaRoute): number {
  const reach = fleetReach(g, f);
  let best = -1, bv = -1;
  for (const z of reach) {
    if (!rt.zones.includes(z)) continue;
    const v = raidersIn(g, rt.to, z);
    if (v > bv) { bv = v; best = z; }
  }
  return best;
}

/** Binds a fleet to a shipping lane as its escort: it sails with the convoys, in the sea of the lane where the raiders are thickest. */
export function escort(g: Game, f: Fleet, lane: string): boolean {
  if (whyNotEscort(g, f, lane)) return false;
  const best = escortSea(g, f, seaRoutes(g).find((r) => r.id === lane)!);
  return best >= 0 && sail(g, f, best, 'escort', lane);
}

/** True when a fleet could steam from its present base to this coast, through straits open to it. */
export function canSailTo(g: Game, f: Fleet, r: Region): boolean {
  const seen = new Uint8Array(g.seas.length);
  // (Release 2, the far chart: a fleet steams from the sea it is on, at sea, or from its harbour's seas, as its voyage
  // is reckoned (voyageWay). The old chart reads its home sea.)
  const q = farOn(g) ? [...new Set(wayFrom(g, f))] : [f.home];
  for (const z of q) seen[z] = 1;
  for (let i = 0; i < q.length; i++) {
    if (r.seas.includes(q[i])) return true;
    for (const j of g.seas[q[i]].adj) if (!seen[j] && canPass(g, q[i], j, f.nat)) { seen[j] = 1; q.push(j); }
  }
  return false;
}

/**
 * A fleet's base moved by order of its staff's book (navy.json 'moves', and the base a planned strike comes home to,
 * carriers.ts): to a harbour its country holds, the sea it lies on named. It lies in harbour there, and the news
 * says so. False where the place is not its country's, or it is there already. `friend`: for a row of the book
 * marked 'ally', whether the harbour's holder is one whose harbour the fleet may take. False for a fleet the player
 * keeps by hand.
 */
export function orderedTo(g: Game, f: Fleet, m: { base: string; port: string; home: string; words: string }, friend?: (holder: string) => boolean): boolean {
  // (A fleet the player has ordered himself is his until he hands it back: no book row moves it, as no admiral does.
  // 1.5.1, Brian 24 November 2026 18:32Z, "they dont stay where they were moved"; navalAI skips it the same way.)
  if (keptByHand(g, f)) return false;
  const r = g.byName[m.base], sea = zoneOf(g, m.home);
  if (!r || (r.ctrl !== f.nat && !friend?.(r.ctrl)) || r.id === f.base || sea < 0) return false;
  f.base = r.id; f.port = m.port; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  delete f.lane;
  // (The book's dated moves are the record's, and are made in their month as the book's own sailings are: one ends a
  // voyage the fleet was on. Release 2, the far chart.)
  delete f.passage;
  g.supplyDirty = true;
  syncWings(g);
  headline(g, 'info', `${m.words}.`, f.nat);
  return true;
}

/**
 * A base move of the staff's book for a fleet of the player's that is his (the audit's re-check of 25 November 2026:
 * one patrol ordered by hand before May 1940 kept the Pacific Fleet off Pearl Harbor for good, and nothing said so).
 * The move waits on the fleet (Fleet.planBase) and the news tells him: kept by hand under the admirals, it goes there
 * once he hands it back; with the navy his own, the move is his to make, and its page offers it. Nothing is moved here.
 */
export function bookWaits(g: Game, f: Fleet, m: { base: string; port: string; home: string; words: string }): void {
  const r = g.byName[m.base];
  if (!r || r.id === f.base || f.nat !== g.human) { delete f.planBase; return; }
  f.planBase = { ...m, turn: g.turn };
  const name = fleetName(f), at = f.base >= 0 ? g.regions[f.base].name : f.port;
  headline(g, 'info', byHand(g, f.nat, 'navy')
    ? `${m.words}, by the staff's plan. You run the navy yourself: ${name} stays at ${at} until you move its base to ${r.name} (its page offers the move).`
    : `${m.words}, by the staff's plan. You keep ${name} under your own orders, so it stays at ${at}: hand it back to your admirals and it goes to ${r.name}, or move its base there yourself.`, f.nat);
}

/** A base move of the staff's book that waits on one of the player's fleets (bookWaits), or null: none, or it is there. */
export function bookBaseOf(g: Game, f: Fleet): { region: Region; words: string; turn: number } | null {
  const b = f.planBase, r = b ? g.byName[b.base] : undefined;
  return b && r && r.id !== f.base && f.nat === g.human ? { region: r, words: b.words, turn: b.turn } : null;
}

/** Friendly ports a fleet could move its base to. */
export function baseTargets(g: Game, f: Fleet): Region[] {
  return g.regions.filter((r) => r.port && r.seas.length > 0 && r.id !== f.base && allied(g, f.nat, r.ctrl) && canSailTo(g, f, r));
}

/**
 * Moves a fleet's base to another friendly port. Within a month's steaming the fleet spends the month in harbour there.
 * On the far chart a move beyond that is a voyage (release 2; the design director's ruling of 04:33Z, 25 November
 * 2026, finding 39 of the 1.5.1 playtest): the fleet sets out in passage (voyageWay, setOut) and comes in a month for
 * each further reach of seas, steamed from the sea it is on if it is at sea. Its base, port and home are the new
 * harbour's at once. `instant`: a move the book dates (the record's, as the book's own sailings are), made in its month
 * wherever it goes. A fleet already in passage is put on a new way from where it is.
 */
export function rebase(g: Game, f: Fleet, r: Region, instant = false): boolean {
  if (!r.port || !r.seas.length || r.id === f.base || !allied(g, f.nat, r.ctrl) || !canSailTo(g, f, r)) return false;
  const way = !instant && farOn(g) ? voyageWay(g, f, r) : null;
  if (!instant && farOn(g) && !way) return false;
  const was = inPassage(f) && f.passage ? { base: f.passage.from, port: f.passage.fromPort, home: f.passage.fromHome } : { base: f.base, port: f.port, home: f.home };
  f.base = r.id;
  f.port = r.name;
  f.home = r.seas[0];
  delete f.lane;
  if (way && way.length > NAVY.range) setOut(g, f, way, was, false);
  else {
    f.zone = f.home;
    f.stance = 'port';
    delete f.passage;
  }
  g.supplyDirty = true;
  syncWings(g);
  return true;
}

// ---- A base move is a voyage (release 2, the far chart) ----------------------------------------------------------
// The design director's ruling of 04:33Z, 25 November 2026 (finding 39 of the 1.5.1 playtest: Japan's Mobile Fleet
// moved its base to Konigsberg in a month). A fleet steams its reach (NAVY.range seas, the sea it starts on counted as
// the first, as fleetReach counts them) in a month. A move of base within that is made the month it is ordered, as
// always. A longer one takes a month for each reach of seas: the fleet is in passage, at sea on its way, seen at the
// other side's own sight and open to attack as any fleet at sea (met at the month's end as a fleet not on patrol is,
// seaBattles), and it strikes, patrols, escorts and holds nothing. It comes in at the opening of the month its last
// reach is steamed, with news, and lies that month in harbour. It can be turned back, by the same reckoning. Every
// nation and the computer alike: the cause is distance. On the old chart there is no voyage.

/**
 * The seas a fleet would steam, in order, on its way to a harbour: from the sea it is on if at sea (else its base's
 * seas) to the first of the harbour's seas it reaches, through straits open to it. Null where no open sea leads there.
 */
function voyageWay(g: Game, f: Fleet, r: Region): number[] | null {
  const prev = new Int16Array(g.seas.length).fill(-2);
  const q: number[] = [];
  for (const z of wayFrom(g, f)) if (prev[z] === -2) { prev[z] = -1; q.push(z); }
  for (let i = 0; i < q.length; i++) {
    if (r.seas.includes(q[i])) {
      const way: number[] = [];
      for (let z = q[i]; z >= 0; z = prev[z]) way.unshift(z);
      return way;
    }
    for (const j of g.seas[q[i]].adj) if (prev[j] === -2 && canPass(g, q[i], j, f.nat)) { prev[j] = q[i]; q.push(j); }
  }
  return null;
}

/** The seas a fleet's way starts from: the sea it is on if at sea, else its base's seas. */
const wayFrom = (g: Game, f: Fleet): number[] => (atSea(f) ? [f.zone] : f.base >= 0 ? g.regions[f.base].seas : [f.home]);

/** The seas open to a fleet from where it is, through straits open to it: where a voyage of its can lead (voyageWay). */
function seasOpen(g: Game, f: Fleet): Uint8Array {
  const seen = new Uint8Array(g.seas.length), q: number[] = [];
  for (const z of wayFrom(g, f)) if (!seen[z]) { seen[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (!seen[j] && canPass(g, q[i], j, f.nat)) { seen[j] = 1; q.push(j); }
  return seen;
}

/** Months of a voyage of so many seas: one for each reach of seas, the last of them the month it comes in. */
const voyageOf = (seas: number): number => Math.max(1, Math.ceil(seas / NAVY.range));

/**
 * The months a fleet's move of base to a harbour takes beyond the month it is ordered: none within its reach (and none
 * on the old chart, nor where no open sea leads there), one more for each further reach of seas. For every navy's
 * base-move rule, the general's fleets with an army among them.
 */
export function voyageMonths(g: Game, f: Fleet, r: Region): number {
  if (!farOn(g)) return 0;
  const way = voyageWay(g, f, r);
  return way ? voyageOf(way.length) - 1 : 0;
}

/** What a move of base costs a fleet in months off its station: the voyage's months and its month in harbour at the new base. */
export const moveCost = (g: Game, f: Fleet, r: Region): number => voyageMonths(g, f, r) + 1;

/** The sea a fleet in passage is on in a month of its voyage: the last sea of the reach it has steamed by then. */
const passageSea = (p: NonNullable<Fleet['passage']>, turn: number): number => p.way[Math.min((turn - p.set + 1) * NAVY.range, p.way.length) - 1];

/**
 * Sets a fleet out in passage on a way longer than its reach, its base, port and home already the new harbour's: at sea
 * on the last sea of the first reach this month, and in at the opening of the month its last reach is steamed. `from`:
 * the harbour it is turned back to. With news.
 */
function setOut(g: Game, f: Fleet, way: number[], from: { base: number; port: string; home: number }, back: boolean): void {
  const months = voyageOf(way.length) - 1;
  f.passage = { from: from.base, fromPort: from.port, fromHome: from.home, way, set: g.turn, due: g.turn + months };
  if (back) f.passage.back = true;
  f.stance = 'passage';
  f.zone = passageSea(f.passage, g.turn);
  // It follows no enemy fleet on its way (release 2, shadow.ts): its shadow ends as it sets out.
  delete f.shadow;
  delete f.shadowAt;
  g.supplyDirty = true;
  const due = turnToDate(f.passage.due).label;
  headline(g, 'info', back
    ? `${cap(fleetName(f))} turns back for ${f.port}, ${way.length} seas off, due in ${due}.`
    : `${cap(fleetName(f))} sails for its new base at ${f.port}, ${way.length} seas off, due in ${due}.`, f.nat, f.base);
}

/**
 * Turns a fleet in passage back to the harbour it left, by the same reckoning from the sea it is on: within its reach
 * it is back in harbour this month, beyond it in passage again. False where it is in no passage, or the harbour it left
 * is no longer its side's.
 */
export function turnBack(g: Game, f: Fleet): boolean {
  const p = f.passage;
  if (!inPassage(f) || !p || p.from < 0) return false;
  const r = g.regions[p.from];
  if (!r?.port || !r.seas.length || !allied(g, f.nat, r.ctrl)) return false;
  const way = voyageWay(g, f, r);
  if (!way) return false;
  const to = { base: f.base, port: f.port, home: f.home };
  f.base = r.id;
  f.port = p.fromPort;
  f.home = r.seas.includes(p.fromHome) ? p.fromHome : r.seas[0];
  delete f.lane;
  if (way.length > NAVY.range) setOut(g, f, way, to, !p.back);
  else {
    delete f.passage;
    f.zone = f.home;
    f.stance = 'port';
    g.supplyDirty = true;
    headline(g, 'info', `${cap(fleetName(f))} turns back, and is in harbour at ${f.port} again.`, f.nat, f.base);
  }
  syncWings(g);
  return true;
}

// ---- A fleet struck at anchor withdraws out of reach (release 2, the far chart) -------------------------------------
// The design director's ruling of 04:33Z, 25 November 2026, 2 (finding 42 of the 1.5.1 playtest: America's battle fleet
// lay at Pearl Harbor under twelve monthly carrier strikes, its worn ships kept in the harbour being struck). One rule,
// not a trigger of its own: the losses a fleet took at anchor this month and last, as its side counts them, are the cost
// of staying, weighed against the cost of a move of base (moveCost, the voyage's months with it). Where it goes is the
// nearest friendly harbour beyond the strikers' reach, measured from where they were seen to fly from. With none it
// stays, and its side's air staff puts fighters over the harbour first (air.ts baseByThreat). A fleet the player keeps
// by hand waits for him: he is told once, and its card says so while it is struck. Every nation and the computer alike.

/** A fleet's strikes at anchor this month and last, where it lies in that harbour still. None off the far chart. */
const anchorStrikes = (g: Game, f: Fleet): NonNullable<Fleet['anchor']> =>
  farOn(g) && !atSea(f) ? (f.anchor ?? []).filter((e) => e.t >= g.turn - 1 && e.at === f.base) : [];

/** The share of a fleet's surface ships sunk by air strikes at anchor this month and last, as its own side counts them (0 to 1): the cost of staying. */
export function anchorLoss(g: Game, f: Fleet): number {
  return Math.min(1, anchorStrikes(g, f).reduce((s, e) => s + e.lost, 0));
}

/** Whether a harbour lies beyond the reach of every strike on a fleet at anchor this month and last, from where its strikers were seen to fly. */
function outOfStrikes(g: Game, f: Fleet, r: Region): boolean {
  for (const e of anchorStrikes(g, f)) for (const o of e.from) {
    const d = o.sea >= 0 ? seaKm(g, r.id, o.sea) : o.field >= 0 && g.regions[o.field] ? km(g, o.field, r.id) : Infinity;
    if (d <= o.km) return false;
  }
  return true;
}

/**
 * The friendly harbour a fleet struck at anchor would withdraw to: the nearest beyond its strikers' reach (fewest
 * months of voyage, then nearest on the chart), open to it and in no sea closed to its nation. Null where there is none,
 * or it has not been struck at anchor this month or last.
 */
export function safeHarbour(g: Game, f: Fleet): Region | null {
  if (!anchorStrikes(g, f).length) return null;
  const here = g.regions[f.base];
  let best: Region | null = null, bm = Infinity, bd = Infinity;
  for (const r of baseTargets(g, f)) {
    if (closedTo(g, r.seas[0], f.nat) || !outOfStrikes(g, f, r)) continue;
    const m = voyageMonths(g, f, r), d = here ? chartDist(g, r, here) : 0;
    if (m < bm || (m === bm && d < bd)) { best = r; bm = m; bd = d; }
  }
  return best;
}

/**
 * Whether a fleet struck at anchor does better to move its base to harbour `r` than to stay: its losses there this
 * month and last, borne month after month over the staff's horizon (navy.json 'anchor'), against the move's months
 * off station (moveCost).
 */
export function strikeMoves(g: Game, f: Fleet, r: Region): boolean {
  const H = NAVY.anchor?.horizon ?? 6;
  return (anchorLoss(g, f) * H * (H + 1)) / 2 > moveCost(g, f, r);
}

/** The line for a fleet's card while it is struck at anchor (this month or last): its losses, and the harbour out of reach. Empty otherwise. */
export function struckLine(g: Game, f: Fleet): string {
  const hits = anchorStrikes(g, f);
  if (!hits.length) return '';
  const n = hits.reduce((s, e) => s + e.n, 0), r = safeHarbour(g, f);
  const lost = n ? `${n === 1 ? 'one ship' : `${numberWord(n)} ships`} lost there this month and last` : 'struck there this month or last';
  return `Struck at anchor at ${f.port}: ${lost}. ${r ? `Nearest friendly harbour out of the strikers' reach: ${r.name}${voyageMonths(g, f, r) ? `, ${voyageMonths(g, f, r) + 1} months away` : ''}.` : "No friendly harbour is out of the strikers' reach: fighters over the harbour keep them off."}`;
}

/**
 * The staff's withdrawal of a fleet struck at anchor (navalAI, every computer navy and a player's navy his admirals
 * run): to the harbour out of reach where staying costs more than the move; else it stays, and the harbour is marked
 * for its air staff's fighters first (`anc:<fleet>`). True where it moved. A fleet with an army (the general's
 * fleetWithArmy, `withArmy:<fleet>`) keeps to its lodgment.
 */
function withdraws(g: Game, f: Fleet): boolean {
  if (!anchorStrikes(g, f).length || g.flags[`withArmy:${f.id}`]) return false;
  const to = safeHarbour(g, f), from = f.port;
  if (to && strikeMoves(g, f, to) && rebase(g, f, to)) {
    headline(g, 'info', `${cap(fleetName(f))} withdraws from ${from}, struck at anchor there, to ${to.name}, out of the strikers' reach.`, f.nat, to.id);
    return true;
  }
  if (!to && anchorLoss(g, f) > 0) g.flags[`anc:${f.id}`] = g.turn;
  return false;
}

/**
 * A fleet struck at anchor with no friendly harbour out of its strikers' reach: its harbour is marked for its side's air
 * staff's fighters first (`anc:<fleet>`, air.ts baseByThreat), at the opening of its month, whoever keeps it.
 */
function anchorMark(g: Game, f: Fleet): void {
  if (anchorLoss(g, f) > 0 && !safeHarbour(g, f)) g.flags[`anc:${f.id}`] = g.turn;
}

/** The player's fleet struck at anchor that he keeps by hand (or whose navy he runs himself): told once a spell of strikes. */
function anchorTold(g: Game, f: Fleet): void {
  if (!anchorStrikes(g, f).length || f.nat !== g.human || !(keptByHand(g, f) || byHand(g, f.nat, 'navy'))) return;
  const k = `anct:${f.id}`, told = g.flags[k];
  g.flags[k] = g.turn;
  if (told !== undefined && told >= g.turn - 2) return;
  const r = safeHarbour(g, f);
  headline(g, 'info', `${cap(fleetName(f))} is struck from the air at anchor at ${f.port}. You keep it under your own orders, so it stays there: ${r ? `the nearest friendly harbour out of the strikers' reach is ${r.name}, and its page offers the move.` : 'no friendly harbour is out of the strikers\' reach, and fighters over the harbour keep them off.'}`, f.nat, f.base);
}

/**
 * True the month a fleet comes in from its passage: its last reach steamed, it lies that month in harbour at its new
 * base as a fleet whose base its staff moved that month does (navalAI, coverHops), so that a voyage costs a month for
 * each reach of seas (moveCost). A player's own orders are his, as after any move of base.
 */
export const cameIn = (g: Game, f: Fleet): boolean => g.flags[`fin:${f.id}`] === g.turn && !atSea(f);

/** Why a fleet in passage can do nothing else this month: the words for its page and its orders. */
export function passageWhy(g: Game, f: Fleet): string {
  const p = f.passage;
  const due = p ? `, due in ${turnToDate(p.due).label}` : '';
  return `${cap(fleetName(f))} is in passage to ${f.port}, at sea on the way${due}: it strikes, patrols and escorts nothing until it comes in.`;
}

/** Whether a strait on the reach a fleet in passage steams this month has been shut to it since it set out on its way. */
function wayShut(g: Game, f: Fleet, p: NonNullable<Fleet['passage']>): boolean {
  const at = Math.min((g.turn - p.set) * NAVY.range, p.way.length) - 1, to = Math.min((g.turn - p.set + 1) * NAVY.range, p.way.length) - 1;
  for (let i = Math.max(1, at + 1); i <= to; i++) if (!canPass(g, p.way[i - 1], p.way[i], f.nat)) return true;
  return false;
}

/**
 * A nation's fleets in passage at the opening of its month: each steams its next reach, and one whose last reach this
 * is comes in to its new base and lies there this month, with news. One whose new base has fallen on the way makes for
 * the nearest harbour it can, as any fleet whose base is lost (newBase), steamed from where it is. One that finds a
 * strait on this month's reach shut to it steams another way from the sea it is on, by the same reckoning; with no way
 * open it turns back, and with no way back either it makes for the nearest harbour it can reach.
 */
function steamPassages(g: Game, nat: string): void {
  for (const f of fleetsOf(g, nat)) {
    const p = f.passage;
    if (!p) continue;
    if (!inPassage(f)) { delete f.passage; continue; }
    if (p.set >= g.turn) continue;
    if (f.base >= 0 && !allied(g, nat, g.regions[f.base].ctrl)) { newBase(g, f); continue; }
    if (wayShut(g, f, p)) {
      const way = voyageWay(g, f, g.regions[f.base]);
      if (!way) { if (!turnBack(g, f)) newBase(g, f, farOn(g), true); continue; }
      p.start ??= p.set;
      p.way = way;
      p.set = g.turn;
      p.due = g.turn + voyageOf(way.length) - 1;
      if (way.length > NAVY.range) {
        f.zone = passageSea(p, g.turn);
        g.supplyDirty = true;
        headline(g, 'info', `${cap(fleetName(f))} finds its way shut, and steams another way to ${f.port}, ${way.length} seas off, due in ${turnToDate(p.due).label}.`, nat, f.base);
        continue;
      }
    } else if (g.turn < p.due) { f.zone = passageSea(p, g.turn); g.supplyDirty = true; continue; }
    const months = g.turn - (p.start ?? p.set);
    delete f.passage;
    f.zone = f.home;
    f.stance = 'port';
    g.flags[`fin:${f.id}`] = g.turn;
    g.supplyDirty = true;
    syncWings(g);
    headline(g, 'info', p.back
      ? `${cap(fleetName(f))} is back in harbour at ${f.port}, ${months === 1 ? 'a month' : `${months} months`} after it turned back.`
      : `${cap(fleetName(f))} comes in to its new base at ${f.port} after ${months === 1 ? 'a month' : `${months} months`} in passage.`, nat, f.base);
  }
}

/**
 * A class's share of loss where the hits to sink a ship are multiplied by `dc` (ruling 240, Damage control): navy.json's
 * `tough` divided by it, for surface warships only, never submarines. With `dc` 1 it is `tough` itself to the last
 * bit. The fight (sinkShips) and the forecast (lossOf) both read this one division, through `dueAt`.
 */
const toughAt = (c: ShipClass, dc: number): number => NAVY.classes[c].tough / (c === 'ss' ? 1 : dc);

/**
 * A class's share of loss in this fleet: navy.json's `tough`, eased by its nation's damage control (ruling 240; tech.ts
 * dcOf, which is exactly 1 with the switch off, without the research and in every history campaign). Every nation
 * alike. Whatever else turns a battle's wear into ships sunk should read this figure, or `lossOf` below.
 */
export const toughOf = (g: Game, f: Fleet, c: ShipClass): number => toughAt(c, dcOf(g, f.nat));

/**
 * The ships of a class that `n` of them should lose at that share of loss, before the dice (ruling 240). The fight
 * (sinkShips) rolls about this figure and the forecast of a fleet action (lossOf) rounds it: both read this one
 * expression, so that the two cannot part by a single bit.
 */
const dueAt = (n: number, share: number, c: ShipClass, dc: number): number => n * share * toughAt(c, dc);

/**
 * The ships of class `c` this fleet should lose at that share of loss, before the dice: the figure sinkShips rolls
 * about when it is given the fleet's own damage control (dcOf of its nation). The forecast of a fleet action
 * (seaplan.ts lossAt) reads it.
 */
export const lossOf = (g: Game, f: Fleet, c: ShipClass, share: number): number => dueAt(f.ships[c], share, c, dcOf(g, f.nat));

/**
 * Sinks about this share of a fleet's ships, class by class, and returns what went down. `dc` (ruling 240) is what the
 * fleet's damage control multiplies the hits to sink by (dcOf of its nation; 1 as ever where the caller gives none).
 * One draw for each class, as ever: where `saved` is given it is filled with the ships that same draw would have
 * sunk without damage control and did not.
 */
export function sinkShips(f: Fleet, frac: number, rng: Rng, list: ShipClass[] = CLASSES, dc = 1, saved?: Ships): Ships {
  const out = none();
  for (const c of list) {
    const [n, plain] = rng.rollBoth(dueAt(f.ships[c], frac, c, dc), dueAt(f.ships[c], frac, c, 1));
    const k = Math.min(f.ships[c], n);
    if (saved) saved[c] = Math.min(f.ships[c], plain) - k;
    f.ships[c] -= k;
    f.lost[c] += k;
    out[c] = k;
    // The ships that went down take their battle stars with them (stars.ts: a fixed rotation, b487).
    loseStars(f, c, k);
  }
  mendStars(f);
  return out;
}

const COUNT_WORD = ['No', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'];

/** "A cruiser", "an ...": a count of one is "a" wherever it stands (the UI director's rider 2 of 22 November). */
const aShip = (one: string, head: boolean): string => `${/^[aeiou]/i.test(one) ? (head ? 'An' : 'an') : head ? 'A' : 'a'} ${one}`;
const andList = (parts: string[]): string => (parts.length === 1 ? parts[0] : `${parts.slice(0, -1).join(', ')} and ${parts[parts.length - 1]}`);

/**
 * How the line for ships kept afloat names their fleet (ruling 240; the UI director's words of 21 November and riders 2
 * and 3 of 22 November): 'fleet' names it, for news every side reads and where nothing before has named it; 'its' only
 * where the sentence just before names the fleet, so the fleet is named once; "fleet's" where the fleet was named
 * earlier but another sentence stands between ("The shore batteries sink a destroyer. One of the fleet's cruisers is
 * ..."); 'yours' is for a text addressed to the fleet's owner alone. With several classes, 'its' and "fleet's" say "of
 * the fleet" once (rider 3): "A cruiser and 2 destroyers of the fleet are badly hit, ...". 'your fleet' names one of
 * the owner's own fleets where two or more of them were struck, so that their lines do not read alike (rider 6): "A
 * destroyer of your Battle Fleet was badly hit, ..." (no "the" after "your").
 */
export type KeptForm = 'fleet' | 'its' | "fleet's" | 'yours' | 'your fleet';

/**
 * Ruling 240: the one line that tells of ships damage control kept afloat, in the UI director's words of 21 November and
 * its riders. The game counts ships and never names them: "A cruiser of the Home Fleet is badly hit, but the repair
 * parties keep the ship afloat." A count that opens the sentence is a word, a count inside it a figure, and a count of
 * one is "a" ("Two destroyers of ...", "A cruiser and 2 destroyers of ...", "A battleship and a cruiser of ...").
 * After a sentence that names the fleet: "One of its cruisers is badly hit, ..."; with another sentence between: "One
 * of the fleet's cruisers is ..."; several classes in either: "A cruiser and 2 destroyers of the fleet are ...". To the
 * owner alone: "One of your cruisers is ...", and with several classes "One of your cruisers and 2 of your destroyers
 * are ...". No pronoun for a ship. `past` (rider 3: the line takes the tense of the sentence before it) tells it in the
 * past, for a report of what happened: "A cruiser of the Home Fleet was badly hit, but the repair parties kept the ship
 * afloat." Empty when nothing was saved. (`true` is 'yours' and `false` 'fleet', as the third argument was before the
 * forms had names.)
 */
export function keptAfloat(f: { name: string }, saved: Ships, how: KeptForm | boolean = 'fleet', past = false): string {
  const n = shipCount(saved);
  if (!n) return '';
  const form: KeptForm = how === true ? 'yours' : how === false ? 'fleet' : how;
  const head = (k: number): string => COUNT_WORD[k] ?? String(k);
  const classes = CLASSES.filter((c) => saved[c] > 0);
  // ("One of its ..." for one class; with several, the fleet form says "of the fleet" once. The owner's form passes as built.)
  const owned = form === 'yours' || (form !== 'fleet' && form !== 'your fleet' && classes.length === 1);
  const whose = form === 'yours' ? 'your' : form === 'its' ? 'its' : "the fleet's";
  const parts = classes.map((c, i) => {
    const k = saved[c], one = NAVY.classes[c].one, many = NAVY.classes[c].many;
    if (owned) return `${i ? (k === 1 ? 'one' : k) : head(k)} of ${whose} ${many}`;
    return k === 1 ? aShip(one, !i) : `${i ? k : head(k)} ${many}`;
  });
  const of = owned ? '' : form === 'fleet' ? ` of ${fleetName(f)}` : form === 'your fleet' ? ` of your ${f.name}` : ' of the fleet';
  const verb = past ? (n === 1 ? 'was' : 'were') : n === 1 ? 'is' : 'are';
  return `${andList(parts)}${of} ${verb} badly hit, but the repair parties ${past ? 'kept' : 'keep'} the ${n === 1 ? 'ship' : 'ships'} afloat.`;
}

/**
 * Ruling 240, rider 3 (22 November): the fleets a strike hit, by name, for the line of ships kept afloat when none was
 * sunk: "the Home Fleet and the Mediterranean Fleet"; three names, then "and N more". (Release 1's "the 2 fleets"
 * elsewhere is left as it is.)
 */
export function fleetsNamed(fs: { name: string }[]): string {
  const names = fs.map((f) => fleetName(f));
  return names.length <= 3 ? andList(names) : `${names.slice(0, 3).join(', ')} and ${names.length - 3} more`;
}

/**
 * Ruling 240, rider 2 (22 November): the same ships told in the past, for a strike's report that sank none: "the repair
 * parties kept a badly hit cruiser and 2 destroyers afloat" (the caller puts it after "Ships of the Home Fleet were hit,
 * but none was sunk: "). Empty when nothing was saved.
 */
export function keptAfloatPast(saved: Ships): string {
  if (!shipCount(saved)) return '';
  const parts = CLASSES.filter((c) => saved[c] > 0).map((c, i) => {
    const k = saved[c], one = NAVY.classes[c].one, many = NAVY.classes[c].many;
    if (!i) return k === 1 ? `a badly hit ${one}` : `${k} badly hit ${many}`;
    return k === 1 ? aShip(one, false) : `${k} ${many}`;
  });
  return `the repair parties kept ${andList(parts)} afloat`;
}

/** What one fleet lost, and what damage control kept afloat, where a warship may be sunk (ruling 240): for afloatWatch. */
export interface AfloatTold {
  /** air: a strike on a fleet or a harbour; shipping: the older strike at shipping in a sea; action: a fleet action; toll: land bombers' monthly toll; shore: batteries answering a bombardment; torpedo: a submarine's torpedo. */
  route: 'air' | 'shipping' | 'action' | 'toll' | 'shore' | 'torpedo';
  nat: string;
  sunk: Ships;
  saved: Ships;
  /** The older strike at shipping only: the fleet after the strike, the share the fight sank at, and the fleet's damage control. */
  fleet?: Fleet;
  frac?: number;
  dc?: number;
}

/**
 * A war runner's eyes on ruling 240 (as staff.ts's strengthWatch and ai.ts's landingWatch): `tell` hears of each fleet
 * as it takes its losses on any of the six routes, `strike` of each older strike at shipping before its dice are thrown
 * (with the assessment it is flown on). Nothing in the game sets either, and they move nothing.
 */
export const afloatWatch: { tell: ((e: AfloatTold) => void) | null; strike: ((a: unknown, nat: string, target: number) => void) | null } = { tell: null, strike: null };

// ---- The month at sea -----------------------------------------------------------------------------

/** True while the book lays the great choice named ('with': nation and choice) and it has not gone: a computer's nation, at a stop that keeps the book, with the book open in the choice's theatre. */
function bookLays(g: Game, named: string): boolean {
  const [who, op] = named.split(':');
  return who !== g.human && !!stopDef(g).book && onBook(g, theatreOf(who, op)) && !((g.flags[`st:done:${who}:${op}`] ?? 0) > 0);
}

/** How many months a dated move has slid with the great choice it is named with ('with': nation and choice), where the stop draws that choice's date. */
function moveSlide(g: Game, m: FleetMove): number {
  if (!m.with) return 0;
  const [who, op] = m.with.split(':');
  const date = HISTORY.dates[who]?.[op], now = bookDate(g, who, op);
  return date === undefined || now === null ? 0 : now - date;
}

/**
 * Ships of a class that play has taken from a fleet and no waiting row has yet made good (the carriers' rows, ruling
 * 233): its losses, less what the waiting rows read for it have brought. Counts, not names: the game knows how many
 * carriers a fleet has lost, never which.
 */
export const gapOf = (g: Game, f: Fleet, c: ShipClass): number => f.lost[c] - (g.flags[`fill:${f.nat}:${f.name}:${c}`] ?? 0);
const fill = (g: Game, f: Fleet, c: ShipClass, n: number): void => { g.flags[`fill:${f.nat}:${f.name}:${c}`] = (g.flags[`fill:${f.nat}:${f.name}:${c}`] ?? 0) + n; };

/** The start of a nation's month at sea: new ships, lost bases, refits, bombers, and the convoy war. */
/**
 * The first month of a grand campaign begun after September 1939 (rider b639 to ruling 234). Its start is the record's
 * for every navy, the player's included: the dial's rows dated before the start, and in its first month, are the
 * start position, facts of the day and not the dial moving a player's ships. From then on the dial leashes the
 * computer's navies only, and a player moves his own ships; arrivals with no date of the dial come to him as before.
 */
export const startMonth = (g: Game): boolean => g.flags.open === 1 && g.flags.openFrom !== undefined && g.flags.openFrom === g.turn;
/** True where the history dial's rows are read for this navy this month: a computer's always, a player's in the start month alone. */
const dialRuns = (g: Game, nat: string): boolean => nat !== g.human || startMonth(g);
/**
 * After his start month, the one kind of the dial's rows a player's navy reads: a ship coming onto the chart from off it,
 * once (the director's note of 15:52Z on rider b639). A player cannot order a ship that is off the map, so she comes to
 * him on her row's date at her row's fleet. Never a move ('from', or 'fromIf' while that fleet is on the chart), a
 * departure, or a row standing in for a plain row he reads ('instead'). A return ('back') comes only where the row that
 * sent her off the chart was read for him, at his start: a ship whose leaving fell after his start stayed with him, so
 * her return would count her twice. And a row whose ships the fleet's own row already holds ('most') only where that
 * fleet formed by its book (flag bk:<nat>:<fleet>): formed by its plain row, it has them (Force H's Enterprise and Renown).
 */
const offChartFor = (g: Game, nat: string, fleet: string, ships: Partial<Ships>, dial: ArrivalDial): boolean =>
  nat === g.human && !dialRuns(g, nat) && !dial.from && dial.instead === undefined && CLASSES.every((c) => (ships[c] ?? 0) >= 0)
  && !(dial.fromIf && g.fleets.some((f) => f.nat === nat && f.name === dial.fromIf))
  && (!dial.back || !!g.flags[`arr:${dial.back[0]}:${nat}:${dial.back[1]}`])
  && (dial.most === undefined || !!g.flags[`bk:${nat}:${fleet}`]);

export function navalUpkeep(g: Game, nat: string): void {
  const n = g.nations[nat];
  // (Release 2, carrier wings by date: the month a navy's room a deck changes, its player is told. The wings follow in syncWings.)
  wingStep(g, nat);
  // (Release 2, the far chart: its fleets in passage steam on, and those due come in, as the month opens; and a fleet
  // of the player's struck at anchor that he keeps is told once.)
  steamPassages(g, nat);
  for (const f of fleetsOf(g, nat)) { anchorMark(g, f); anchorTold(g, f); }
  for (const given of NAVY.fleets) {
    // A row with a book of its own forms by it at the book's stops, for the computer's navy (Force H in June 1940),
    // and for a player's in the start month of a campaign begun later (rider b639).
    const b = given.book;
    const booked = !!b && g.flags.open === 1 && b.stops.includes(stopOf(g)) && dialRuns(g, nat);
    const row = booked ? { ...given, from: b!.from, ships: b!.ships } : given;
    const due = booked ? b!.from <= g.turn && g.turn <= Math.max(b!.from, given.from ?? 1) : (row.from ?? 1) === g.turn;
    if (row.nat !== nat || !due || g.turn <= 1 || g.fleets.some((f) => f.nat === nat && f.name === row.name)) continue;
    if (row.base && !g.byName[row.base]) continue; // a fleet of ground this campaign's chart does not reach
    const f = addFleet(g, row);
    if (booked) g.flags[`bk:${nat}:${f.name}`] = g.turn;
    headline(g, 'info', `${f.name} forms at ${f.port}.`, nat);
  }
  const named = (name: string): Fleet | undefined => g.fleets.find((f) => f.nat === nat && f.name === name);
  for (const b of NAVY.building) if (b.nat === nat && g.turn >= b.from && g.turn <= b.to) { const f = named(b.fleet); if (f) { mendStars(f, b.cls); f.ships[b.cls] += b.perMonth; } }
  // The month's rows, a nation's own, those that wait for a gap after the others: the gap is judged once the month's
  // comings and goings are in (the carriers' rows, ruling 233).
  const mine = NAVY.arrivals.filter((r) => r[1] === nat);
  for (const [turn, who, fleet, ships, what, dial] of [...mine.filter((r) => !r[5]?.waits), ...mine.filter((r) => r[5]?.waits)]) {
    // A row dated by the history dial (Force H's carriers, ruling 233 rider b564) is read only at its stops of the
    // grand campaign, and like every row of the dial it leashes the computer's navies and never the player's after
    // his start month (a campaign begun later opens with the record's ships for every navy: rider b639). It is
    // read once, in the first month at one of its stops on or after its date, so a grand campaign begun later, or a
    // dial turned to the book later, still has the record's ships; a row from before this month changes them without
    // news. A row with no date of the dial is read in its month, except at the stops where a dated row stands in its
    // place ('not'), for the computer's navy. After his start a player's navy reads only a ship coming onto the chart.
    const from = dial?.from;
    const key = `arr:${turn}:${who}:${fleet}${from ? `<${from}` : ''}`;
    // (A player's ship coming onto the chart after his start: hers alone, so neither a gap nor 'most' holds her back.)
    const onto = !!dial?.stops && who === nat && offChartFor(g, nat, fleet, ships, dial);
    // A row that lapses ('lapse') is closed for good from that turn: the record's window for that posting has closed.
    const due = (dial?.lapse === undefined || g.turn < dial.lapse) && (dial?.stops
      ? turn <= g.turn && g.flags.open === 1 && dial.stops.includes(stopOf(g)) && (dialRuns(g, nat) || onto) && !g.flags[key]
      : turn === g.turn && !(dial?.not && g.flags.open === 1 && dial.not.includes(stopOf(g)) && nat !== g.human));
    const f = who === nat && due ? named(fleet) : undefined;
    // A waiting row is read only once play has taken from the fleet a ship of each kind it brings that no waiting
    // row has made good: the arrival that replaced her waits until the one she replaced has gone. (Not a player's ship
    // coming back to the chart after his start: she is his, and comes on her date.)
    if (!f || (dial?.waits && !onto && !CLASSES.every((c) => (ships[c] ?? 0) <= 0 || gapOf(g, f, c) > 0))) continue;
    // (Battle stars: the fleet's lists mended to its counts before any ship is added or taken.)
    mendStars(f);
    // A move takes its ships from the fleet they sailed with, as many as are there, so no ship is in two fleets. Where
    // they were to come there by a waiting row not yet read, they come from off the chart, and that row is closed.
    // ('fromIf': a move from that fleet only where it is on the chart in this game, as the China Fleet is on the far
    // side's; elsewhere the ships come from off the chart. Its key is the row's own, so stops and saves read it as before.)
    let src: Fleet | undefined;
    if (from) {
      const brought = dial?.by !== undefined ? `arr:${dial.by}:${who}:${from}` : undefined;
      if (brought && !g.flags[brought]) g.flags[brought] = g.turn;
      else if (!(src = named(from))) continue;
    } else if (dial?.fromIf) src = named(dial.fromIf);
    if (dial?.stops) g.flags[key] = g.turn;
    // A count below none is ships leaving for another fleet (the far side's rows): never more than are there, and the words say where they go.
    const leaving = CLASSES.some((c) => (ships[c] ?? 0) < 0);
    let gone = 0, came = 0;
    for (const c of CLASSES) {
      let n = ships[c] ?? 0;
      const was = f.ships[c];
      // ('most': never more of a class than that after the row, so one read late into a fleet formed with them brings none.)
      if (n > 0 && !onto && dial?.most?.[c] !== undefined) n = Math.max(0, Math.min(n, dial.most[c]! - was));
      if (src && n > 0) { n = Math.min(n, src.ships[c]); src.ships[c] -= n; }
      f.ships[c] = Math.max(0, was + n);
      if (was > f.ships[c]) {
        gone += was - f.ships[c];
        // The record's sinking is not play's: it leaves no gap for a waiting row.
        if (dial?.sunk) { f.lost[c] += was - f.ships[c]; fill(g, f, c, was - f.ships[c]); }
      } else came += f.ships[c] - was;
      if (dial?.waits && n > 0) fill(g, f, c, n);
    }
    // A dial's row of ships lost that play has already sunk says nothing: the news has told it once; nor a move that
    // finds none of its ships. Nor does one read late: its month is past. (A waiting row's month is the one it is read in.)
    if ((dial && leaving && !gone) || ((src || dial?.most) && !came) || (turn < g.turn && !dial?.waits)) continue;
    // (Many ships take the plural, however the phrase opens: "More of the American destroyers join", the third playtester, 84.3.1.)
    // (And a phrase that opens with a count of two or more: "Two new fleet carriers join", the playtest's #50.)
    const many = / and /.test(what) || /s$/.test(what.split(' ')[0]) || (/\bof\b/.test(what) && /s$/.test(what))
      || /^(?:\d+|two|three|four|five|six|seven|eight|nine|ten|eleven|twelve|twenty|thirty|forty|fifty)\b/i.test(what);
    const words = leaving ? `${cap(what)}.`
      : src ? `${cap(what)} ${many ? 'leave' : 'leaves'} ${fleetName(src)} for ${fleetName(f)}.`
      : `${cap(what)} ${many ? 'join' : 'joins'} ${fleetName(f)}.`;
    headline(g, 'info', words, nat);
  }
  // The far side (ruling 234(d)): the fleets that spend this month of peace at sea, and where.
  // (Peace, for such a month, is peace with the country whose blow the row waits for, where the row names one: America
  // is at war with Germany from the close of December 1941 whenever Japan strikes, and a strike that comes later must
  // still find the carriers out or in. At war with anybody a player's fleets are his own to send out, and nothing is
  // done with them. Rider b593 to ruling 234. A row that names nobody reads as it did: at war with nobody.)
  const peace = (whose?: string): boolean => !n.atWar.length || (whose !== undefined && !atWar(g, nat, whose) && !byHand(g, nat, 'navy'));
  // (A base move that waited for him is made once the fleet is his admirals' again: handed back, or the navy handed them.)
  for (const f of fleetsOf(g, nat)) {
    if (!f.planBase || keptByHand(g, f) || byHand(g, nat, 'navy')) continue;
    const b = f.planBase;
    delete f.planBase;
    orderedTo(g, f, b);
  }
  const out = new Map<number, number>();
  for (const m of NAVY.moves ?? []) {
    if (m.nat !== nat || (m.stops && !m.stops.includes(stopOf(g)))) continue;
    if (m.sea !== undefined) {
      // (The record's month at sea keeps the month of the choice it is named with, where that month is drawn.)
      const z = m.turn + moveSlide(g, m) === g.turn && peace(m.with?.split(':')[0]) ? zoneOf(g, m.sea) : -1;
      const f = z >= 0 ? named(m.fleet) : undefined;
      if (f) { out.set(f.id, z); headline(g, 'info', `${m.words}.`, nat); }
      continue;
    }
    // (A player who runs his navy himself moves his own fleets; where his staff runs it, the staff keeps the dates.)
    // (A move named with a great choice keeps that choice's month and is read at war with the nation whose it is; one
    // marked 'eve' before that war, on the book's word that the blow falls this month.)
    // (A move of a fleet that is his, his by hand or with the navy his own, waits for him and the news says so: bookWaits.)
    const foe = m.with?.split(':')[0];
    if (m.turn + moveSlide(g, m) !== g.turn) continue;
    if (foe !== undefined && (m.eve ? atWar(g, nat, foe) || !bookLays(g, m.with!) : !atWar(g, nat, foe))) continue;
    const f = named(m.fleet);
    const friend = m.ally ? (holder: string): boolean => allied(g, nat, holder) && (foe === undefined || atWar(g, holder, foe)) : undefined;
    if (!f || m.base === undefined || m.port === undefined || m.home === undefined) continue;
    const row = { base: m.base, port: m.port, home: m.home, words: m.words };
    if (byHand(g, nat, 'navy') || keptByHand(g, f)) bookWaits(g, f, row);
    else orderedTo(g, f, row, friend);
  }
  if (!stopDef(g).book) {
    for (const f of fleetsOf(g, nat)) {
      const z = f.cruise && peace(f.cruise[2]) ? zoneOf(g, f.cruise[0]) : -1;
      if (z >= 0 && dialDraw(g, 'cruise', nat, f.name, g.turn) < f.cruise![1]) out.set(f.id, z);
    }
  }
  // The submarines go out to their beats before the month at sea is counted (the grand campaign).
  placeBoats(g, nat);
  for (const f of fleetsOf(g, nat)) {
    // A tie lapses the month its home falls, or once its beat is out of reach (ruling 233 rider b598(2)). Read before
    // a fallen base is moved: newBase's own line of news tells the base's loss, and the tie goes with it.
    if (tieLapses(g, f)) f.tied = false;
    if (f.base >= 0 && !allied(g, nat, g.regions[f.base].ctrl) && !newBase(g, f)) continue;
    // (A fleet in passage keeps on its way in peace as in war: release 2, the far chart.)
    if (!n.atWar.length) { if (!inPassage(f)) { f.stance = 'port'; f.zone = f.home; } delete f.cruised; }
    // (Back from its month at sea into a war: it is at its base, and from here its admiral's.)
    else if (f.cruised !== undefined) { if (f.cruised < g.turn && f.stance === 'patrol') { f.stance = 'port'; f.zone = f.home; g.supplyDirty = true; } delete f.cruised; }
    if (atSea(f)) {
      airAttack(g, f);
      f.ready = f.ready > NAVY.ready.floor ? Math.max(NAVY.ready.floor, f.ready + NAVY.ready.sea) : Math.min(NAVY.ready.floor, f.ready + NAVY.ready.mend);
    } else f.ready = Math.min(1, f.ready + NAVY.ready.port);
    // A month of peace at sea is a cruise, not a patrol: it wears nothing, and the ships are simply not in harbour.
    // (It takes a fleet lying in harbour, as every fleet of a country at war with nobody does: one its admiral has at
    // sea in another war stays where he sent it; nor does a fleet the player keeps by hand go out on the book's word.)
    const z = out.get(f.id);
    if (z !== undefined && !keptByHand(g, f) && f.base >= 0 && shipCount(f.ships) > 0 && !atSea(f)) { f.zone = z; f.stance = 'patrol'; f.cruised = g.turn; if (n.atWar.length) g.supplyDirty = true; }
  }
  // The grand campaign: the destroyers hunt before the month's cargoes are counted, and thin screens pay for it.
  // (Release 2, ruling 241: with the convoy battle on, a night the player fought on one of this nation's routes is
  // read first, as the fleets stand before the hunt, and booked into the hunt's and the convoy's own rolls.)
  const nights = convoyNights(g, nat);
  const sunk = seaWar(g) && n.atWar.length ? huntSubs(g, nat, nights) : undefined;
  // (Release 2, Units decide, once live: the aircraft on patrol hunt too, after the ships, inside the same cap.)
  if (sunk && patrolOn(g)) patrolSubs(g, nat, sunk);
  if (sunk) torpedoes(g, nat);
  // (Release 2, Units decide, once the sea patrol is live: once the hunt, its patrols' attacks and the battles on its
  // convoy routes are fought, what its month at sea showed this nation of the enemy's boats, and what its own boats met
  // of the enemy's aircraft, go into its record of what it has seen (seasight.ts; the design director's ruling of 24
  // November 2026 17:54Z). Nothing is read or written for it otherwise.)
  const seen = patrolOn(g) ? seaMonth() : undefined;
  convoyWar(g, nat, sunk, nights, seen);
  if (seen) noteSeaSight(g, nat, seen, sunk);
}

// ---- The submarines' beats (ruling 233 rider b564) ----------------------------------------------------------------
// Italy's boats are not all on one sea: the historian's rows put them on four beats, the Atlantic from Bordeaux and the
// Mediterranean's west, centre and east, and Germany sent U-boats into the Mediterranean from the autumn of 1941. In the
// grand campaign a computer-run navy's boats are placed by those rows each month, before the month at sea is counted.
// Only submarines move: a transfer never touches a surface ship, and no boat is made or lost by it.

/**
 * A whole number of boats out of 'total' for each weight, in proportion, by largest remainder: the parts always add up
 * to the total exactly. With no weight anywhere every part is none.
 */
export function wholeShares(total: number, weights: number[]): number[] {
  const w = weights.map((x) => Math.max(0, x));
  const sum = w.reduce((a, b) => a + b, 0);
  if (sum <= 0 || total <= 0) return w.map(() => 0);
  const out = w.map((x) => Math.floor((x * total) / sum));
  const rest = w.map((x, i) => x * total - out[i] * sum);
  let left = total - out.reduce((a, b) => a + b, 0);
  for (const i of w.map((_, i) => i).sort((a, b) => rest[b] - rest[a] || a - b)) {
    if (left <= 0) break;
    out[i]++;
    left--;
  }
  return out;
}

/**
 * The enemy's shipping in a sea, as a staff placing its boats by need counts it: the sea lanes and convoy routes
 * through it that feed a nation at war with this one, while cargoes still sail on them (and a lane carries something
 * laid: cargoLaid).
 */
export function shippingIn(g: Game, nat: string, z: number): number {
  let n = 0;
  for (const rt of seaRoutes(g)) if (laneCounts(rt) && rt.zones.includes(z) && g.nations[rt.to]?.alive && atWar(g, nat, rt.to) && !laneStopped(g, rt.id)) n++;
  return n;
}

/** The record's cause for U-boats in the Mediterranean (stops 3 to 5): an army of the Axis fighting on the African shore. */
export function armyInAfrica(g: Game, cause: NonNullable<Beats['cause']>): boolean {
  if (g.turn < cause.from) return false;
  const ground = cause.ground.map((name) => g.byName[name]).filter((r): r is Region => !!r);
  for (const r of ground) {
    for (const c of r.units) {
      if (c.dead || c.kind === 'fort' || !cause.by.includes(c.nat)) continue;
      if (ground.some((x) => atWar(g, c.nat, x.ctrl))) return true;
    }
  }
  return false;
}

/** The beat a group's boats are sent to this month: its row's station, or the latest dated change of it. */
function datedStation(g: Game, G: BeatGroup, row: FleetRow): number {
  let key = row.zone;
  for (const [turn, sea] of G.stations ?? []) if (g.turn >= turn) key = sea;
  return zoneOf(g, key);
}

/** How many of the sea lanes and convoy routes through a sea feed this nation this month (while it stands and cargoes sail on them, and a lane carries something laid: cargoLaid). */
export function lanesTo(g: Game, to: string, z: number): number {
  let n = 0;
  for (const rt of seaRoutes(g)) if (laneCounts(rt) && rt.to === to && rt.zones.includes(z) && g.nations[rt.to]?.alive && !laneStopped(g, rt.id)) n++;
  return n;
}

/**
 * A group's beat by the record's seas (ruling 233 riders b601 and b603, the Bordeaux flotilla at stops 1 to 3): the
 * sea in the historian's list for the month's half-year that carries the most lane feeding `H.to` (between seas that
 * carry as much, the first in the list). Where every sea in the list carries none, the nearest sea to the list that
 * carries one, counted outwards from the list's seas through straits open to the group (between seas as near, the one
 * with more such lanes, then the first in the chart's order), so that its sinkings fall on that trade where the chart
 * carries it; never the list's first sea for its own sake. Only seas within the group's reach from its port count, so
 * that it can sail there. -1 before the list's first half-year, or where no sea in its reach carries such a lane.
 * (`month`: the month the list is read by, the game's own unless the far side's form reads an early war as the
 * record's first month.)
 */
export function huntBeat(g: Game, f: Sailing, H: NonNullable<BeatGroup['hunts']>, month: number = g.turn): number {
  let list: string[] | undefined;
  for (const [turn, seas] of H.seas) if (month >= turn) list = seas;
  if (!list) return -1;
  const reach = new Set(fleetReach(g, f, false, true));
  const own = list.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
  let best = -1, most = 0;
  for (const z of own) { const n = reach.has(z) ? lanesTo(g, H.to, z) : 0; if (n > most) { best = z; most = n; } }
  if (best >= 0) return best;
  const d = new Int16Array(g.seas.length).fill(-1);
  const q: number[] = [];
  for (const z of own) if (d[z] < 0) { d[z] = 0; q.push(z); }
  for (let i = 0; i < q.length; i++) for (const j of g.seas[q[i]].adj) if (d[j] < 0 && canPass(g, q[i], j, f.nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  for (const z of q) {
    const n = reach.has(z) ? lanesTo(g, H.to, z) : 0;
    if (n > 0 && (best < 0 || d[z] < d[best] || (d[z] === d[best] && (n > most || (n === most && z < best))))) { best = z; most = n; }
  }
  return best;
}

/** A group's commander this month: its row's admiral, or the latest dated change of him (navy.json beats 'admirals'; rider b601). */
function datedAdmiral(g: Game, G: BeatGroup, row: FleetRow): string | undefined {
  let name = row.admiral;
  for (const [turn, who] of G.admirals ?? []) if (g.turn >= turn) name = who;
  return name;
}

/**
 * True while a fleet of an enemy's with surface ships, a quarry for the boats, has its home port on one of these seas
 * (ruling 233 rider b598(1)): a port the fleet's base opens on, or its home sea for a base off the map.
 */
export function foeBasedOn(g: Game, nat: string, seas: string[]): boolean {
  const zs = seas.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
  return g.fleets.some((f) => atWar(g, f.nat, nat) && surfaceShips(f) > 0 && (f.base >= 0 ? g.regions[f.base].seas : [f.home]).some((z) => zs.includes(z)));
}

/**
 * Places a computer-run navy's submarines on their beats for the month (navy.json 'beats'; the grand campaign only,
 * never the player's own navy).
 *
 * Italy: every boat in its groups is shared out afresh by the row of the month, as shares of what the groups have, so
 * the game's own losses stand. A group that cannot sail ('held' ground lost, or its base gone before it ever formed)
 * is left out and the others share its boats. At stops 1 to 3 the shares are the historian's row by date, and the
 * Atlantic flotilla's beat is read from the historian's seas by half-year: the one of them with the most British lane,
 * or where none has any, the nearest sea to them that does; its dated beat only where no sea in its reach carries one
 * (huntBeat; ruling 233 riders b601 and b603). At stops 4 and 5 the staff places the boats by need, as
 * ruling 236 rider b554 has a staff's budget move: each group's beat is the sea within its reach where the enemy's
 * shipping is thickest (its dated beat where none is thicker), and the boats start where the war opened them (the
 * first row) and move towards a share in proportion to that shipping, about a fifth of the way in a year; with no
 * enemy shipping on any beat, towards the opening shares. Need counts lanes and not the enemy's fleets: a lane and a
 * fleet's weight have no common measure in the rules, so while an enemy fleet is based on a Mediterranean sea the
 * three Mediterranean beats keep at least a third of the boats instead (navy.json beats 'floor'; rider b598(1)), each
 * counted while its beat is on a Mediterranean sea.
 *
 * Germany: as many boats as the row asks, up to what it has, go from the U-boat Fleet to the two Mediterranean
 * groups, and any beyond the row come back to it. At stops 1 and 2 by date; at stops 3 to 5 only while an Axis army
 * fights in North Africa, the record's cause, and otherwise all of them come back. These boats are only lent: a group
 * whose base is lost sends them home rather than look for a port of its own nation's in the Mediterranean.
 *
 * Each group's commander is the record's of the day ('admirals'; rider b601), at every stop: a new one takes over
 * with a line of news, and a group formed after his date forms under him.
 *
 * A group formed here that is left with no boats stands down, and forms again, with its line of news, when it has some.
 */
export function placeBoats(g: Game, nat: string): void {
  const B = NAVY.beats?.[nat];
  // (A player's boats are placed by the rows in his start month alone: rider b639. From then on they are his to move.)
  if (!B || !seaWar(g) || !dialRuns(g, nat) || !g.nations[nat]?.alive) return;
  if (B.far) { placeFarBoats(g, nat, B); return; }
  const stop = stopOf(g);
  let row: number[] | undefined;
  for (const r of B.rows) if (r[0] <= g.turn) row = r;
  // By need, a navy at war before the record's first row places its boats as that row would have opened them.
  if (!row && !B.from && stop >= 4 && g.nations[nat].atWar.length) row = B.rows[0];
  if (!row) return;
  const rows = B.groups.map((G) => (G.base !== undefined ? ({ ...G, nat } as FleetRow) : NAVY.fleets.find((r) => r.nat === nat && r.name === G.name)));
  const fleets = B.groups.map((G) => g.fleets.find((f) => f.nat === nat && f.name === G.name));
  const ours = (place: string | null | undefined): boolean => !!place && !!g.byName[place] && allied(g, nat, g.byName[place].ctrl);
  // A group sails while its own ground is held, and forms only from a base its side holds. Once formed, a group of the
  // navy's own boats that loses its base moves to another port of its own (newBase); lent boats need theirs held.
  const can = B.groups.map((G, i) => !!rows[i] && (!G.held || ours(G.held)) && ((!!fleets[i] && !B.from) || ours(fleets[i] ? g.regions[fleets[i]!.base]?.name : rows[i]!.base)));
  // Each group's beat: by date, or at stops 4 and 5 for Italy the sea in its reach with the most enemy shipping.
  const beat = B.groups.map((G, i) => (can[i] ? datedStation(g, G, rows[i]!) : -1));
  // Where a group sails from: its own fleet, or for one not yet formed, its row's port.
  const from = (i: number): Pick<Fleet, 'nat' | 'base' | 'home'> => fleets[i] ?? { nat, base: g.byName[rows[i]!.base!].id, home: zoneOf(g, rows[i]!.home) };
  // At stops 1 to 3 a group that hunts a nation's trade takes the sea of the record's half-year that carries the most of
  // it, or the nearest that does, so that it is never parked on an empty sea; its dated beat only where none in its
  // reach does (ruling 233 riders b601 and b603).
  if (stop <= 3) B.groups.forEach((G, i) => { if (can[i] && G.hunts) { const z = huntBeat(g, from(i), G.hunts); if (z >= 0) beat[i] = z; } });
  const pool = B.from ? g.fleets.find((f) => f.nat === nat && f.name === B.from) : undefined;
  let want = B.groups.map((_, i) => (can[i] ? row![i + 1] : 0));
  let total: number;
  if (B.from) {
    if (!pool) return;
    if (stop >= 3 && B.cause && !armyInAfrica(g, B.cause)) want = want.map(() => 0);
    const have = pool.ships.ss + fleets.reduce((n, f) => n + (f?.ships.ss ?? 0), 0);
    total = Math.min(want.reduce((a, b) => a + b, 0), have);
  } else {
    total = fleets.reduce((n, f) => n + (f?.ships.ss ?? 0), 0);
    if (stop >= 4) {
      // By need (ruling 236 rider b554): the boats go where the enemy's cargoes sail, each group within its own reach.
      B.groups.forEach((_, i) => {
        if (!can[i]) return;
        const reach = fleetReach(g, from(i));
        if (!reach.includes(beat[i])) beat[i] = -1;
        for (const z of reach) if (shippingIn(g, nat, z) > (beat[i] >= 0 ? shippingIn(g, nat, beat[i]) : 0)) beat[i] = z;
      });
      const opening = B.groups.map((_, i) => (can[i] ? B.rows[0][i + 1] : 0));
      const need = beat.map((z) => (z >= 0 ? shippingIn(g, nat, z) : 0));
      const goal = need.some((x) => x > 0) ? need : opening;
      const sum = goal.reduce((a, b) => a + b, 0);
      // Where the boats are now: as the war opened them, the first month they are placed at all.
      const now = B.groups.some((G, i) => G.base !== undefined && fleets[i]) ? fleets.map((f, i) => (can[i] ? f?.ships.ss ?? 0 : 0)) : wholeShares(total, opening);
      // A sixtieth of the way each month is about a fifth of it in a year. (Boats whose group cannot sail go to the others now.)
      want = now.map((x, i) => (sum > 0 ? x + ((total * goal[i]) / sum - x) / 60 : x));
    }
    if (!want.some((x) => x > 0)) return;
  }
  const parts = wholeShares(total, want);
  // By need, the boats are not drawn off every sea an enemy fleet is based on (ruling 233 rider b598(1)): while one is
  // based on a Mediterranean sea, the Mediterranean's beats keep a third of the boats between them. Need itself counts
  // only lanes, since a lane and a fleet's weight have no common measure in the rules; the gradual move stands.
  const F = B.floor;
  if (F && !B.from && stop >= 4 && foeBasedOn(g, nat, F.seas)) {
    const idx = B.groups.map((_, i) => i);
    // A group counts for the floor while its beat is on one of the floor's seas, not by its name alone.
    const seas = F.seas.map((k) => zoneOf(g, k));
    const on = (i: number): boolean => seas.includes(beat[i] >= 0 ? beat[i] : fleets[i]?.station ?? -1);
    const inside = idx.filter((i) => can[i] && F.groups.includes(B.groups[i].name) && on(i)), outside = idx.filter((i) => !inside.includes(i));
    const least = Math.ceil((total * F.least[0]) / F.least[1]);
    if (inside.length && inside.reduce((n, i) => n + parts[i], 0) < least) {
      // The floor's boats go among its beats as the month's move would share them (as the war opened them, where that gives none); the rest as before.
      const w = inside.map((i) => Math.max(0, want[i]));
      wholeShares(least, w.some((x) => x > 0) ? w : inside.map((i) => B.rows[0][i + 1])).forEach((x, k) => { parts[inside[k]] = x; });
      wholeShares(total - least, outside.map((i) => want[i])).forEach((x, k) => { parts[outside[k]] = x; });
    }
  }
  B.groups.forEach((G, i) => {
    let f = fleets[i];
    const admiral = G.admirals ? datedAdmiral(g, G, rows[i]!) : undefined;
    if (!f && parts[i] > 0) {
      f = addFleet(g, admiral ? { ...rows[i]!, admiral } : rows[i]!);
      headline(g, 'info', `${cap(fleetName(f))} forms at ${f.port}.`, nat);
    }
    if (!f) return;
    // The record's commander of the day (rider b601): a new one starts at the first level, as any new admiral does.
    if (admiral && f.admiral?.name !== admiral) {
      f.admiral = { name: admiral, level: 1, xp: 0 };
      headline(g, 'info', `${admiral} takes command of ${fleetName(f)}.`, nat);
    }
    // (Battle stars: both lists mended to their counts before boats move between them.)
    mendStars(f, 'ss'); if (pool) mendStars(pool, 'ss');
    if (pool) pool.ships.ss -= parts[i] - f.ships.ss;
    f.ships.ss = parts[i];
    if (beat[i] >= 0) f.station = beat[i];
    // A group formed here with no boats left stands down: they have gone to the others or home, and it forms again
    // when it has some (the Atlantic flotilla when Bordeaux is retaken).
    if (G.base !== undefined && shipCount(f.ships) === 0) g.fleets.splice(g.fleets.indexOf(f), 1);
  });
}

// ---- The convoy war by cause (release 2, Units decide; audit #5) -------------------------------------------------
// The design director's ruling of 23 November 2026 07:26Z on naval-battles/convoy-r2/form.md, under Brian's core theme
// (07:11Z and 07:13Z): what a convoy loses and what its escorts sink come from the kit, the aircraft and the work of the
// ships on the table, the same for every navy, never from the nation or the month. The record (OEG Report 51) only sets
// how strong each is: data/campaign/convoycause.json. Read only where a grand campaign opened with Units decide; off,
// none of this is read, no die is thrown and every number is as it was.

export const CAUSE = causeData as unknown as {
  cover: number; asv: string; find: number; radar: number; seaLoss: string[];
  incident: { coordinated: number; independent: number; coordinatedKit: number; independentKit: number; atw: number };
  /** The on-station share's days by class (null: not read), and a boat's raid weight on station refit with it at T2 (null: 1). */
  station: Partial<Record<Exclude<ShipClass, 'dd'>, { patrol: number | null; passage: number | null; refit: number | null }>> & { raid: number | null; dd?: EscortCycleRow };
  /** The convoy-side count's figures; a route with a null in 'ships' or 'grt' keeps the per-boat share ('find'). 'sinkOnStations': k on the escorts on the close screen's stations, read only with the escorts' losses form laid (null there: E and k as the tons ruling set them). */
  count: { attacked: number; sighted: number; boatsT2: number; perAttack: number; ways: number; t2: { convoys: number; months: number }; sink: number | null; sinkOnStations?: number | null; ships: Record<string, number | null>; grt: Record<string, number | null> };
  /** The boat ceiling: a boat's torpedoes inside its hull and outside it (used only in a calm sea), the share of those fired that hit, the ships sunk a hit, and its ready tubes, the torpedoes of one approach (read with 'load'). A null in inside, hit or sinkHit: no ceiling. */
  ceiling: { inside: number | null; outside: number | null; hit: number | null; sinkHit: number | null; tubes?: number | null };
  /** The escort's radar before a boat fires: the share of the boats that try which fire at T2's ratio of radar-fitted escorts a convoy to boats trying, and that ratio. A null in either: the pack fires as counted. 'eye': the eye term's two record points (every escort finds, radar-fitted ones again); a null in it: radar alone. */
  firing: { t2: number | null; rT2: number | null; screen?: { stations: number | null; columnsAStation: number | null; shipsAColumn: number | null; maxColumns: number | null }; eye?: EyeRow };
  /** What a boat that fires puts into one convoy (the load form): hours to reload its ready tubes, and the share of boats that regain contact after an attack. A null in reload: the whole load into one convoy, as first built; a null in regain: one approach. */
  load?: { reload: number | null; regain: number | null; next?: NextNightRow };
  /** The sea patrol against submarines (seapatrol.ts): 'live' its switch; the aircraft that watch a sea's waters reached, attacks on a boat a month in watched waters and the share of them that sink it, by kit, and aircraft lost a month; 'intensity' the air fighting's own row for a patrol met by enemy fighters (the air thread's Q6), which gates only the fighters (null: none meets a patrol). With live false, or a null in a0, attacks or kill, none of it is read (the design director's ruling of 24 November 2026 17:49Z, 5 (a)). */
  patrol: { live: boolean; reach: 'map' | 'air'; a0: number | null; intensity: number | null; attacks: { kit: number | null; none: number | null }; kill: { kit: number | null; none: number | null }; lost: number | null };
  /** The escorts' losses form's switch (the design director's ruling of 24 November 2026 17:58Z): true, laid; null (unread), the escort meets the count's incidents and E and k stand as the tons ruling set them. */
  escorts?: boolean | null;
};

/**
 * The next night's contact (convoycause.json load.next): the boats' surface speed in knots, and for each route its
 * convoys by class, each class's speed in knots and its share of the route's sailings. A null speed or share, or a route
 * left null: unread, and its boats have no next night.
 */
interface NextNightRow { boat: number | null; convoys: Record<string, { class: string; speed: number | null; share: number | null }[] | null> }

/** The eye term's record points (convoycause.json firing.eye): for 1941's sample and T2, the share of boats trying that torpedoed a ship, the torpedoes a boat that fired put in (n), and the escorts on stations and the radar-fitted ones for each boat trying. A null: unread. */
interface EyeRow { effective41: number | null; n41: number | null; e41: number | null; r41: number | null; effectiveT2: number | null; nT2: number | null; eT2: number | null; rT2: number | null }

/** The escorts' cycle on a convoy route (convoycause.json station.dd): days with a convoy, turnaround and refit in port, the share of strength out of service, and a convoy's crossing. A null: unread. */
interface EscortCycleRow { convoy: number | null; turnaround: number | null; refit: number | null; unavailable: number | null; crossing: number | null }

/** A navy's anti-submarine kit: sonar and depth charges, sea-search radar, ahead-throwing weapons, escort-group training, long-range patrol aircraft. */
export interface AswKit { asdic: boolean; radar: boolean; hedgehog: boolean; groups: boolean; patrol: boolean }

/** A navy's anti-submarine kit, by its research alone. A kin navy has the better of its own and its senior partner's, as its ships' fittings do (tech.ts shipKit). */
export function aswKit(g: Game, nat: string): AswKit {
  const senior = PLANS.kin[nat];
  const has = (id: string): boolean => hasTech(g, nat, id) || (senior !== undefined && hasTech(g, senior, id));
  return { asdic: has('sonar'), radar: has(CAUSE.asv), hedgehog: has('hedgehog'), groups: has('escortgroups'), patrol: has('patrolaircraft') };
}

/** True where a navy's aircraft carry anti-submarine kit: depth charges and the radar to find a boat (convoycause.json 'asv'). */
export const airKit = (k: AswKit): boolean => k.asdic && k.radar;

/**
 * The chance that one attack on a convoy ends with the boat sunk, by a support group (OEG T10's coordinated column) or by
 * the convoy's close escort (its independent column), with this kit. With the escorts' losses form laid (the design
 * director's ruling of 24 November 2026 17:58Z, Q2 (a); escortChance), the coordinated column is worked by the support
 * groups, shared over the route's convoys at sea at once (his ruling of 22:24Z, 3 (b)), and by a convoy's escorts beyond
 * its close screen's stations, and the independent column by the escorts on the stations.
 */
export function incidentRate(k: AswKit, group: boolean): number {
  const I = CAUSE.incident;
  if (k.hedgehog && k.groups) return group ? I.coordinatedKit : I.independentKit;
  // Ahead-throwing weapons alone: the depth-charge rate times OEG T9's first half-year edge.
  return (group ? I.coordinated : I.independent) * (k.hedgehog ? I.atw : 1);
}

/** What a ship's kit makes of each attack against depth charges alone: 1 without ahead-throwing weapons. */
const kitEdge = (k: AswKit, group: boolean): number => incidentRate(k, group) / incidentRate({ ...k, hedgehog: false }, group);

/** Radar on escorts: what sea-search radar adds (not laid: 1 until the historian's figure is read). */
const radarKit = (k: AswKit): number => 1 + (k.radar ? CAUSE.radar : 0);

/** A navy's kit as the submarine scene's record holds it (types.ts SubLook). */
type KitLook = NonNullable<SubLook['convoys'][number]['seas'][number]['kit']>;

/** Where an escort's look on the scene comes from: its navy's kit as the sea scene reads it, with its escort-group training. HF/DF has no rule yet. */
const kitLook = (k: AswKit): KitLook => ({ asdic: k.asdic, radar: k.radar, hedgehog: k.hedgehog, hfdf: false, groups: k.groups });

/** The air cover over a convoy's sea: where it flies from, and the share of the sea's waters it reaches. */
export interface SeaCover { kind: 'none' | 'shore' | 'carrier' | 'long-range' | 'patrol'; share: number }

/**
 * The air cover a convoy has in one sea from aircraft carrying anti-submarine kit, for the nation it feeds: a carrier
 * bound to the lane as its escort, in her sea; the aircraft on the coasts its side holds, over the share of the sea they
 * reach (the sea's 'air'), or all of it with long-range patrol aircraft, which also reach a sea with no coast next to
 * such a coast. Weather closes or halves it, as it does all aircraft at sea. The best of these counts.
 */
export function seaCover(g: Game, nat: string, z: number, lane?: string): SeaCover {
  const wx = seaSkyMul(g, z);
  let best: SeaCover = { kind: 'none', share: 0 };
  if (wx === 0) return best;
  const take = (kind: SeaCover['kind'], share: number): void => { if (share > best.share) best = { kind, share: Math.min(1, share) }; };
  if (lane !== undefined) {
    // (By the aircraft she has aboard: an empty deck covers nothing.)
    for (const f of g.fleets) if (f.stance === 'escort' && f.lane === lane && f.zone === z && f.ships.cv > 0 && allied(g, f.nat, nat) && airKit(aswKit(g, f.nat))) take('carrier', wx * Math.min(1, wingShare(g, f)));
  }
  // (Release 2, the sea patrol, once live: the aircraft its side keeps on patrol there with kit, over the share of the
  // sea they watch, in place of the coasts held. The sky is already in the aircraft on patrol.)
  if (patrolOn(g)) {
    const w = watchFor(g, z, (x) => allied(g, x, nat));
    if (w && w.kit > 0) take('patrol', w.kit);
    return best;
  }
  // A coast its side holds, flying aircraft with kit, under a sky no enemy has won.
  const flies = (id: number): AswKit | null => {
    const r = g.regions[id];
    const n = g.nations[r.ctrl];
    if (r.since >= g.turn || !n?.alive || !n.atWar.length || !(n.air > 0) || !allied(g, r.ctrl, nat) || skyOver(g, id)) return null;
    const k = aswKit(g, r.ctrl);
    return airKit(k) ? k : null;
  };
  const zone = g.seas[z];
  for (const id of zone.coast) {
    const k = flies(id);
    if (k) take(k.patrol ? 'long-range' : 'shore', (k.patrol ? 1 : zone.air) * wx);
  }
  if (!zone.coast.length) {
    for (const a of zone.adj) for (const id of g.seas[a]?.coast ?? []) if (flies(id)?.patrol) take('long-range', wx);
  }
  return best;
}

/** What is left of the raiders' strength in a sea under this cover: all of it with none, OEG T7's share where all the sea is covered. */
const coverCut = (c: SeaCover): number => 1 - c.share * (1 - CAUSE.cover);

// ---- Time on station and the convoy-side count (release 2, Units decide; the one-pass ruling) --------------------
// The design director's ruling of 24 November 2026 13:23Z on naval-battles/convoy-r2/one-pass-form.md, and the days of
// 13:31Z. A boat is on station only part of its cycle: the passage out from its port and back, by the sea graph, and
// the refit in port take the rest. A navy that bases nearer the sea it raids keeps more boats there, and its staff does
// so (nearerPort). What its boats meet of a route's escort is counted from the convoys' side: the convoys that sail,
// the share of them the boats on station find and attack, and the boats that attack each. Read only with Units decide.

/** Seas crossed from a fleet's port to a sea by the sea graph, through the straits open to it: 0 where its port opens on that sea. A sea it cannot reach that way (it got there before a strait closed) is counted the shortest way round regardless. */
export function passageSeas(g: Game, f: Pick<Fleet, 'nat' | 'base' | 'home'>, z: number): number {
  for (const open of [true, false]) {
    const d = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (const s of f.base >= 0 ? g.regions[f.base].seas : [f.home]) if (d[s] < 0) { d[s] = 0; q.push(s); }
    for (let i = 0; i < q.length; i++) {
      if (q[i] === z) return d[z];
      for (const j of g.seas[q[i]].adj) if (d[j] < 0 && (!open || canPass(g, q[i], j, f.nat))) { d[j] = d[q[i]] + 1; q.push(j); }
    }
  }
  return 0;
}

/** The days in a month the escorts' cycle and a raider's move of beat are read against (D: the game's month as thirty days). */
const MONTH_DAYS = 30;

/** A class's on-station days, or null while any of them is unread. */
function stationDays(c: Exclude<ShipClass, 'dd'>): { patrol: number; passage: number; refit: number } | null {
  const r = CAUSE.station[c];
  return r && r.patrol !== null && r.passage !== null && r.refit !== null ? (r as { patrol: number; passage: number; refit: number }) : null;
}

/**
 * The share of the month a fleet's ships of a class are on station in a sea, raiding from a port (its own unless
 * given): the patrol's days there over its whole cycle, with the passage out and back by the sea graph and the refit
 * in port. 1 for a class whose days are unread, and everywhere without Units decide.
 */
export function onStation(g: Game, f: Pick<Fleet, 'nat' | 'base' | 'home'>, z: number, c: Exclude<ShipClass, 'dd'> = 'ss'): number {
  const D = unitsDecide(g) ? stationDays(c) : null;
  if (!D) return 1;
  return D.patrol / (D.patrol + 2 * D.passage * passageSeas(g, f, z) + D.refit);
}

/**
 * The share of the month a fleet's boats are on station in a sea as the convoys meet them (release 2, the sea patrol;
 * the design director's 02:25Z ruling and Q5 (a)): on station, less the time aircraft without anti-submarine kit keep
 * them down there, 1 - the share of the sea those aircraft watch. onStation itself where none watch, or the patrol is not live.
 * This is the world, what the boats meet. A boats' staff weighs its beats on the aircraft its side's boats last met
 * instead (seenOnWatch, below; the design director's ruling of 24 November 2026 17:54Z).
 */
export function onWatch(g: Game, f: Pick<Fleet, 'nat' | 'base' | 'home'>, z: number): number {
  return onStation(g, f, z) * keptUp(g, f.nat, z);
}
/** What aircraft without kit leave of a boat's time up on station in a sea: 1 - the share of it they watch (1 where none do). The world's: what the boats meet, never what a staff believes. */
function keptUp(g: Game, nat: string, z: number): number {
  const w = patrolOn(g) ? watchAgainst(g, z, nat) : null;
  return w && w.none > 0 ? 1 - w.none : 1;
}

/**
 * What a boats' staff believes aircraft without kit leave of its boats' time up in a sea: 1 - the part of a boat's time
 * its side's boats last met them keeping the boats down there (seasight.ts seenAir), never the watch truly kept there
 * (the design director's 24 November 2026 17:49Z amendment to ruling 1, and his ruling of 17:54Z). 1 where its side's
 * boats never met any there: a sea never seen counts as clear (Q2 (a)).
 */
function seenUp(g: Game, nat: string, z: number): number {
  const a = seenAir(g, nat, z);
  return a && a.none > 0 ? 1 - a.none : 1;
}
/** A fleet's time up on station in a sea as its own staff sees it: on station, less the time its side's boats last met aircraft without kit keeping them down there (seenUp). */
const seenOnWatch = (g: Game, f: Pick<Fleet, 'nat' | 'base' | 'home'>, z: number): number => onStation(g, f, z) * seenUp(g, f.nat, z);

/**
 * A fleet's raiding strength against cargoes, before its nation's quality and fitness: each class's raid. With Units
 * decide and the boats' days read, the boats raid only while on station, each at its weight on station, refit once at
 * OEG T2's window so that window keeps its tons (convoycause.json station.raid).
 */
function raidPower(g: Game, f: Fleet, cause: boolean): number {
  let p = 0;
  for (const c of CLASSES) p += c === 'ss' ? boatRaid(g, f, cause) : f.ships[c] * NAVY.classes[c].raid;
  return p;
}
/**
 * The boats' part of a fleet's raid strength: on station only, where the days are laid (Units decide). The lanes' form
 * weighs a boat on station by station.raid (its refit at T2's window); the convoys' side counts it at 1 ('weighed' false),
 * since there the boats on station that find and attack carry the share.
 */
function boatRaid(g: Game, f: Fleet, cause: boolean, weighed = true): number {
  const st = cause && stationDays('ss') ? onStation(g, f, f.zone) * (weighed ? CAUSE.station.raid ?? 1 : 1) : 1;
  // (The far side's one measure and ruling 194's faulty torpedoes: its submarines out, `boatsOut`, at `subsWorth`. Every
  // boat on hand at full worth in the old chart's seas and with the far side off.)
  return boatsOut(g, f) * subsWorth(g, f) * NAVY.classes.ss.raid * st;
}

/**
 * The held port a computer-run raiding fleet's staff would base it at: the friendly port, held since before this
 * month and within its reach by sea, from which its boats spend most time on station on its beat (or on the beat
 * given, as its staff weighs a new one: quieterBeat), where that is more than from its own port (nearest the beat by
 * the sea graph; on a tie, the nearer on the chart). Null where none is better, for a tied fleet, while its days are
 * unread, and without Units decide. One rule for every navy: the player moves his own fleets' bases himself.
 */
export function nearerPort(g: Game, f: Fleet, z = f.station): Region | null {
  if (!unitsDecide(g) || f.tied || f.duty !== 'raid' || f.ships.ss <= 0 || !stationDays('ss')) return null;
  const here = onStation(g, f, z);
  const at = g.seas[z];
  let best: Region | null = null, bv = here, bd = Infinity;
  for (const r of g.regions) {
    if (!r.port || !r.seas.length || r.id === f.base || r.since >= g.turn || !allied(g, f.nat, r.ctrl) || !canSailTo(g, f, r)) continue;
    const v = onStation(g, { nat: f.nat, base: r.id, home: r.seas[0] }, z);
    const d = seaDist(g, r, at);
    if (v > bv + 1e-9 || (best && Math.abs(v - bv) <= 1e-9 && d < bd)) { best = r; bv = v; bd = d; }
  }
  return best;
}

/**
 * A computer-run raider's better beat under the sea patrol (release 2, once live; Q5 (a)): of the seas of the convoy
 * routes its beat lies on, within its reach by sea, the one where the month's gain in its boats' time up on station
 * (seenOnWatch: the passage there and back by the sea graph, less the time aircraft without kit keep them down there as
 * its side's boats last met them) over their time up on their own beat, less what the move itself loses (moveLoss), is
 * most, where that is more than nothing. Each beat, its own too, is read from the port its staff would base it at
 * (nearerPort). -1 where none is, where its side's boats last met no aircraft without kit on its beat, for a tied fleet,
 * and while the patrol is not live. A boat keeps its beat unless the move's gain covers the time it costs (the design
 * director's ruling of 24 November 2026 17:49Z, 1 (a)): without it the boats' staff and the patrol staff chased each
 * other from sea to sea, and with Atlantic ports held the boats spent every other month in harbour.
 * The staff reads only what its side has seen (the design director's 17:49Z amendment to ruling 1, and his ruling of
 * 17:54Z): the aircraft over each sea as its side's boats last met them there (seasight.ts seenAir), never the patrols
 * truly flown or ordered there. A sea its boats have never met aircraft in counts as clear (Q2 (a)), so a staff learns
 * of new cover only once it has flown its month over its boats there, keeping them down, attacking them or sinking them
 * (the patrols' own months, patrolSubs), and never at the order. The keeping down itself, in the convoys' count, the
 * lanes and the next night, stays the world's (keptUp).
 */
export function quieterBeat(g: Game, f: Fleet): number {
  if (!patrolOn(g) || f.tied || f.duty !== 'raid' || f.ships.ss <= 0) return -1;
  const here = f.station;
  // (Only out of the aircraft's way: a beat where its side's boats last met no aircraft without kit stays as its staff set it.)
  if (seenUp(g, f.nat, here) >= 1) return -1;
  const seas = new Set<number>();
  for (const r of NAVY.convoys) {
    const zs = r.zones.map((k) => zoneOf(g, k));
    if (zs.includes(here) && atWar(g, f.nat, r.to)) for (const z of zs) seas.add(z);
  }
  const reach = new Set(fleetReach(g, f));
  const stay = nearerPort(g, f, here);
  const up = seenOnWatch(g, stay ? portOf(f, stay) : f, here);
  let best = -1, bv = 0;
  for (const z of [...seas].sort((a, b) => a - b)) {
    if (z === here || !reach.has(z)) continue;
    const to = nearerPort(g, f, z);
    const gain = seenOnWatch(g, to ? portOf(f, to) : f, z) - up - moveLoss(g, f, z, to, stay);
    if (gain > bv + 1e-9) { best = z; bv = gain; }
  }
  return best;
}

/** A fleet as based at another port: what seenOnWatch and onStation read of it. */
const portOf = (f: Fleet, r: Region): Pick<Fleet, 'nat' | 'base' | 'home'> => ({ nat: f.nat, base: r.id, home: r.seas[0] });

/**
 * The time up on station a raider's move of beat to a sea itself loses in the month, against keeping its beat, as a
 * share of the month (release 2, the sea patrol; the design director's ruling of 24 November 2026 17:49Z, 1 (a)), read
 * from the way its staff makes the move (navalAI) and weighed, as quieterBeat weighs the move, on the aircraft as its
 * side's boats last met them (seenOnWatch, the ruling of 17:54Z): the beat is changed, the boats sail to it from where
 * they are, and the staff then bases them at the held port it would raid the beat from (nearerPort), which costs the
 * month in harbour (rebase): the month's whole time up on its beat (seenOnWatch there, from the port it has), given up. So: where the move
 * takes the fleet to another base and staying would not ('rebases' without 'would'), that month in harbour; where
 * staying would move its base too, nothing, for the month in harbour comes either way. Where the move keeps its base:
 * for a fleet at sea, its boats on station crossing from the sea they are in to the new beat, station.ss.passage days
 * for each sea crossed by the sea graph, through the straits open to it (passageSeas), at their time up on station
 * there, over the month's days (a fleet in harbour sails to the new beat as to the old, and loses nothing on the way);
 * and where staying would move its base ('would'), less the month in harbour that staying costs and the move does not,
 * so that what the move loses can be less than nothing. Nothing while the boats' days are unread: no tuned figure
 * stands in for them. `to` and `stay`: the base the move would take it to, and the one staying would ('rebases' and
 * 'would'). (Release 2, the far chart: a move of base costs its voyage's months too, moveCost, one rule for every navy;
 * the design director's ruling of 04:33Z, 25 November 2026. Off the far chart a move costs its month, as before.)
 */
function moveLoss(g: Game, f: Fleet, z: number, to: Region | null, stay: Region | null): number {
  const D = stationDays('ss');
  if (!D) return 0;
  if (to) return (stay ? voyageMonths(g, f, to) - voyageMonths(g, f, stay) : moveCost(g, f, to)) * seenOnWatch(g, f, f.station);
  const passage = atSea(f) ? (seenOnWatch(g, f, f.zone) * D.passage * passageSeas(g, { nat: f.nat, base: -1, home: f.zone }, z)) / MONTH_DAYS : 0;
  return stay ? passage - moveCost(g, f, stay) * seenOnWatch(g, f, f.station) : passage;
}

/**
 * A route's convoy count for a month: the boats on station on its seas, the convoys that sail (both ways), those the
 * boats sight and attack, the incidents with the escort at OEG T2's own rate (the boats that fired there), and those
 * boats for each convoy attacked (the pack), all on average. The pack is what the screen contest reads its boats that
 * try from. With the escorts' losses form laid (escortWork), the escort meets the boats that try by cause instead, and
 * 'incidents' stays the pack's source only.
 */
export interface ConvoyCount { boats: number; sailed: number; sighted: number; attacked: number; incidents: number; pack: number }

/** The incidents with the escort a boat on station meets in a month, at OEG T2's own rate: the window's convoys a month x the share attacked x the boats a convoy attacked / the boats on station (D, 0.21). The escorts' losses form replaces it for the escort's incidents where it is laid (escortWork), never before. */
const escortRate = (): number => {
  const K = CAUSE.count;
  return ((K.t2.convoys / K.t2.months) * K.attacked * K.perAttack) / K.boatsT2;
};

/** The convoys' side of a route this month (Units decide), as the fleets stand now: null where a figure it needs is not laid, and the route keeps the per-boat share. */
export function convoyCount(g: Game, route: ConvoyRoute): ConvoyCount | null {
  const K = CAUSE.count;
  const ships = K.ships[route.id], grt = K.grt[route.id];
  if (!unitsDecide(g) || ships === null || ships === undefined || grt === null || grt === undefined) return null;
  const zones = route.zones.map((k) => zoneOf(g, k));
  let boats = 0;
  for (const f of g.fleets) {
    if (!atSea(f) || f.ships.ss <= 0 || !zones.includes(f.zone) || !atWar(g, f.nat, route.to)) continue;
    boats += f.ships.ss * raidShare(f) * onWatch(g, f, f.zone);
  }
  // The route's tons go out loaded; the convoys come back too, and T2's share is over both ('ways').
  const sailed = (K.ways * route.tons) / (ships * grt);
  // One phi for every route, fitted to OEG T2: the share attacked is 'attacked' with 'boatsT2' on station. F, finding from kit, is 1: none is laid.
  const phi = -Math.log(1 - K.attacked) / K.boatsT2;
  const attacked = sailed * (1 - Math.exp(-phi * boats));
  // The boats that meet the escort are counted from the boats' side, at T2's own rate: more boats on station make bigger packs on the convoys found.
  const incidents = boats * escortRate();
  return { boats, sailed, sighted: Math.min(sailed, (attacked * K.sighted) / K.attacked), attacked, incidents, pack: attacked > 0 ? incidents / attacked : 0 };
}

/**
 * The incidents one of a raiding fleet's boats meets with a route's escort in a month, on average: its share raiding and
 * on station, times T2's rate a boat on station where the route's count is laid, or the per-boat share (OEG T2) where
 * it is not. With the escorts' losses form laid (the design director's ruling of 24 November 2026 17:58Z, Q1 (a)), the
 * rate a boat on station is the route's incidents by cause (w, escortWork: the convoys attacked x the boats that try x
 * (1 - f^2)) over its boats on station, so the route's incidents fall to its raiding fleets by their share of its boats
 * on station, as the count's do.
 */
const incidentsPerBoat = (g: Game, f: Fleet, count: ConvoyCount | null, w?: EscortWork | null): number => raidShare(f) * onWatch(g, f, f.zone) * (count ? (w ? w.rate : escortRate()) : CAUSE.find);

/** True where a boat raids a convoy route of this nation's side: one of its seas, the route's nation at war with the boat's. */
function onRouteOf(g: Game, nat: string, f: Fleet): boolean {
  for (const r of NAVY.convoys) {
    const to = g.nations[r.to];
    if (!to?.alive || !to.atWar.length || !allied(g, r.to, nat) || !atWar(g, f.nat, r.to)) continue;
    if (r.zones.some((k) => zoneOf(g, k) === f.zone)) return true;
  }
  return false;
}

/**
 * The chance that an attack on this route's convoys in one sea ends with the boat sunk: the support groups there (its
 * side's destroyers on hunting duty in the sea) and the close escort (destroyers bound to the route, which sail with the
 * convoys through all its seas), each at its own navy's rate, by destroyers and readiness. None without destroyers.
 * Where the escorts' losses form is laid, escortChance gives it instead (Q2 (a)).
 */
function incidentChance(g: Game, nat: string, route: string, z: number): number {
  let w = 0, s = 0;
  for (const f of g.fleets) {
    if (!atSea(f) || f.ships.dd <= 0 || !allied(g, f.nat, nat)) continue;
    const group = f.stance === 'hunt' && f.zone === z;
    if (!group && !(f.stance === 'escort' && f.lane === route)) continue;
    w += f.ships.dd;
    s += f.ships.dd * fit(g, f) * incidentRate(aswKit(g, f.nat), group);
  }
  return w > 0 ? s / w : 0;
}

// A month's boats lost by fleet, so that the hunt and the convoys together keep inside the hunt's cap (a sea loses at
// most 'most' of its boats a month), whichever nations' months they fall in. Kept on the fleet (Fleet.capBook), so a
// save made part-way through the month, and every copy the look-ahead makes, plays on as the live month does. Written
// only here, and these are reached only with Units decide on.
/** Notes boats about to be lost from a fleet this month (called before they are taken off). */
function noteLost(g: Game, f: Fleet, k: number): void {
  if (f.capBook?.turn !== g.turn) f.capBook = { turn: g.turn, had: f.ships.ss, lost: 0 };
  f.capBook.lost += k;
}
/** How many more of its boats a fleet can lose this month, as an expected count, inside the hunt's cap. */
function roomLeft(g: Game, f: Fleet): number {
  const b = f.capBook?.turn === g.turn ? f.capBook : undefined;
  return Math.max(0, FLEETS.hunt.most * (b?.had ?? f.ships.ss) - (b?.lost ?? 0));
}

/** The convoy war's flat cut to what reaches a nation: with Units decide, only the tree's convoy system (convoycause.json 'seaLoss'). */
export function seaLossMul(g: Game, nat: string): number {
  if (!unitsDecide(g)) return traitSeaLoss(g, nat);
  let m = 1;
  for (const t of techTraits(g, nat)) if (t.seaLoss !== undefined && CAUSE.seaLoss.includes(t.id.replace(/^tech:/, ''))) m *= t.seaLoss;
  return m;
}

/**
 * The base a far-side group sails from this month: the latest of its dated bases that its side holds, its row's own
 * the first. At stops 1 and 2 the list keeps the record's dates. At stops 3 to 5 a date gives way when every base
 * before it is lost (the design director's ruling of 21:00Z and 21:04Z, 22 November 2026, on Japan's boats that stood
 * down from January to July 1943 with Kwajalein lost and Saipan dated February 1944): with none of the bases whose
 * date has come in its side's hands, the group sails from the first of the later ones that its side holds, the
 * earliest date first. A base whose date has come, retaken, takes the group back. None on the whole list, and the
 * group stands down. Every navy's groups.
 */
export function baseInForce(g: Game, nat: string, G: BeatGroup): { base: string; port: string; home: string } | null {
  const list: [number, string, string, string][] = [[0, G.base ?? '', G.port ?? '', G.home ?? ''], ...(G.bases ?? [])];
  const held = ([, base, , home]: [number, string, string, string]): boolean => { const r = g.byName[base]; return !!r && allied(g, nat, r.ctrl) && r.seas.includes(zoneOf(g, home)); };
  for (let i = list.length - 1; i >= 0; i--) {
    const [from, base, port, home] = list[i];
    if (from <= g.turn && held(list[i])) return { base, port, home };
  }
  if (stopOf(g) <= 2) return null;
  const later = list.filter((x) => x[0] > g.turn && held(x)).sort((a, b) => a[0] - b[0])[0];
  return later ? { base: later[1], port: later[2], home: later[3] } : null;
}

/**
 * The beat of a group whose boats were sent against the enemy's fleet and not its cargoes (Japan's: the design
 * director's ruling of 20:03Z of 22 November 2026, on the historian's table of where its boats were lost). Of the
 * record's seas for the half-year that it can reach, the one where the hunted side's fleet stands this month, the
 * strongest in surface ships; failing a fleet, one where that side has landed troops this month or last. None
 * where neither is so, and the group then keeps the first of the list it can reach, as any group does.
 */
export function fleetBeat(g: Game, f: Sailing, H: NonNullable<BeatGroup['hunts']>, month: number = g.turn): number {
  let list: string[] = [];
  for (const [turn, seas] of H.seas) if (month >= turn) list = seas;
  const reach = new Set(fleetReach(g, f, false, true));
  let best = -1, most = 0;
  for (const k of list) {
    const z = zoneOf(g, k);
    if (z < 0 || !reach.has(z)) continue;
    let n = 0;
    for (const x of g.fleets) if (x.zone === z && atSea(x) && atWar(g, x.nat, f.nat) && (x.nat === H.to || allied(g, x.nat, H.to))) n += surfaceShips(x);
    if (n <= 0 && landedLately(g, z, f.nat)) n = 1;
    if (n > most) { most = n; best = z; }
  }
  return best;
}

/** The record's seas where a group hunted in the half-year a month falls in (its 'hunts' list in force), or none before the first. */
const huntSeas = (G: BeatGroup, month: number): string[] => {
  let list: string[] = [];
  for (const [from, seas] of G.hunts?.seas ?? []) if (month >= from) list = seas;
  return list;
};

/**
 * How far a far-side group's boats raid from the base they are in (the design director's note of 13:45Z, 22 November
 * 2026): no number of ours, but the farthest of the record's seas where the group hunted in that half-year, counted
 * on the chart from that base as any fleet's reach is counted. Never less than any fleet's reach.
 */
export function boatsReach(g: Game, f: Sailing, G: BeatGroup, month: number = g.turn): number {
  const d = seaSteps(g, f);
  let far = NAVY.range;
  for (const k of huntSeas(G, month)) { const z = zoneOf(g, k); if (z >= 0 && d[z] > far) far = d[z]; }
  return far;
}

/**
 * How far the player's own boats raid from the base they are in (the design director's ruling of 17:42Z, 22 November
 * 2026: a player never lacks a tool the computer has). No staff places his boats after his first month, so no group
 * carries a reach for him. A fleet of submarines alone raids as far from its base as his navy's group raids from that
 * base in that half-year (boatsReach): the group whose own base, or a dated base it has by that month, is the place
 * (or the later base its date gave way to, `baseInForce`), and the farther where two groups share it. A war that comes before the record's is read by the record's first
 * month, as the staff's placing reads it. From any other base, with a surface ship in company, and for a navy whose
 * beats are not the far side's form: any fleet's reach.
 */
export function ownBoatsReach(g: Game, f: Sailing): number {
  const B = NAVY.beats?.[f.nat];
  if (!B?.far || !farOn(g) || f.base < 0 || !f.ships || f.ships.ss <= 0 || shipCount(f.ships, SURFACE) > 0) return NAVY.range;
  const at = g.regions[f.base].name;
  let far = NAVY.range;
  for (const G of B.groups) {
    if (!G.hunts || (G.base !== at && !(G.bases ?? []).some(([from, base]) => from <= g.turn && base === at) && baseInForce(g, f.nat, G)?.base !== at)) continue;
    far = Math.max(far, boatsReach(g, f, G, atWar(g, f.nat, G.hunts.to) ? Math.max(g.turn, B.far.war) : g.turn));
  }
  return far;
}

/**
 * The far side's form of the beats (the design director's notes of 13:45Z, 13:59Z and 14:06Z, 22 November 2026; the
 * historian's paper sub-patrols-japan-oil.md, sections 4 and 9): America's, Britain's and the Netherlands' boats
 * against Japan's trade, and Japan's own.
 *
 * The rows are boats on hand by command, in port or at sea, the measure of Germany's rows. They are lent from the
 * navy's own fleets ('pools', in their order) and come home to the first of them: as many as the row asks, up to what
 * the navy has, so a row above that is a split among the commands and no demand. A navy whose rows are marked
 * 'far.all' (Japan's, whose one row is the record's 63 of December 1941) sends its groups every boat it has, in the
 * row's shares: the count is the first month's and no ceiling. They are read while the navy is at
 * war with a nation its groups hunt, and at no other time: with that war over, or not begun, every boat is with its
 * pool. A war that comes before the record's ('far.war') is read as the record's first month.
 *
 * A group sails from the record's base of the day ('bases'), the latest its side holds, at every stop: it moves there
 * with a line of news, and a group with no such base left sends its boats home. Its boats raid as far from that base
 * as the record's boats patrolled in that half-year (boatsReach), so the forward bases shorten nothing by a number of
 * ours: they move the place the reach is counted from. Within that reach its beat is, at stops 1 and 2, the sea of the record's list
 * for the half-year that carries the most of the hunted nation's lanes, or the nearest to the list that carries one
 * (huntBeat); at stops 3 to 5 the sea where the enemy's shipping is thickest, the staff's own pick. Where no cargo
 * sails in its reach it keeps the first of the record's seas for the half-year that it can reach.
 */
function placeFarBoats(g: Game, nat: string, B: Beats): void {
  const stop = stopOf(g);
  const fleets = B.groups.map((G) => g.fleets.find((f) => f.nat === nat && f.name === G.name));
  const pools = (B.pools ?? []).map((name) => g.fleets.find((f) => f.nat === nat && f.name === name)).filter((f): f is Fleet => !!f);
  const quarry = B.groups.some((G) => !!G.hunts && atWar(g, nat, G.hunts.to));
  if (!quarry && !fleets.some((f) => !!f)) return;
  const month = quarry ? Math.max(g.turn, B.far!.war) : g.turn;
  let row: number[] | undefined;
  for (const r of B.rows) if (r[0] <= month) row = r;
  const bases = B.groups.map((G) => baseInForce(g, nat, G));
  const want = B.groups.map((_, i) => (quarry && row && bases[i] ? row[i + 1] : 0));
  const held = fleets.map((f) => f?.ships.ss ?? 0);
  const out = held.reduce((a, b) => a + b, 0);
  // (With no pool left to go home to, the boats stay with the groups: shared by the row, or where they are.)
  // (A navy whose row is its first month's count and no ceiling, 'far.all': every boat it has goes to its groups, in
  // the row's shares, the month it is built. The design director's ruling of 21:00Z, 22 November 2026, on Japan's 141
  // boats on hand in August 1945 with 71 of them lying in the 6th Fleet's harbour, where no hunter reached them.)
  const asked = want.reduce((a, b) => a + b, 0), onHand = out + pools.reduce((n, f) => n + f.ships.ss, 0);
  const total = pools.length ? (B.far!.all && asked > 0 ? onHand : Math.min(asked, onHand)) : out;
  const parts = wholeShares(total, pools.length || want.some((x) => x > 0) ? want : held);
  // Boats come home before others go out: home to the first pool, out from the pools in their order.
  if (pools.length) {
    let need = 0;
    parts.forEach((x, i) => { if (x < held[i]) pools[0].ships.ss += held[i] - x; else need += x - held[i]; });
    for (const p of pools) { const k = Math.min(need, p.ships.ss); p.ships.ss -= k; need -= k; }
  }
  B.groups.forEach((G, i) => {
    let f = fleets[i];
    const at = bases[i];
    if (!f && parts[i] > 0 && at) {
      f = addFleet(g, { ...G, nat, base: at.base, port: at.port, home: at.home } as FleetRow);
      headline(g, 'info', `${cap(fleetName(f))} forms at ${f.port}.`, nat);
    }
    if (!f) return;
    f.ships.ss = parts[i];
    // A group with no boats left stands down, and forms again, with its line of news, when it has some.
    if (shipCount(f.ships) === 0) { g.fleets.splice(g.fleets.indexOf(f), 1); return; }
    // The record's base of the day, while its side holds it.
    if (at && g.regions[f.base]?.name !== at.base) orderedTo(g, f, { ...at, words: `${cap(fleetName(f))} moves its base to ${at.port}` }, (holder) => allied(g, nat, holder));
    f.reach = boatsReach(g, f, G, month);
    if (!G.hunts) return;
    const reach = fleetReach(g, f, false, true);
    let beat = -1;
    if (stop <= 2) beat = G.hunts.after === 'fleet' ? fleetBeat(g, f, G.hunts, month) : huntBeat(g, f, G.hunts, month);
    else { let most = 0; for (const z of reach) { const n = shippingIn(g, nat, z); if (n > most) { beat = z; most = n; } } }
    if (beat < 0) beat = huntSeas(G, month).map((k) => zoneOf(g, k)).find((z) => z >= 0 && reach.includes(z)) ?? -1;
    if (beat >= 0) f.station = beat;
  });
}

/**
 * The far side (the design director's ruling of 19:52Z of 22 November 2026): a fleet laid for harbour, lying in it,
 * keeps the patrol's share of its destroyers on the sea at its own door, as navies kept local patrols on their
 * approaches. No fleet does with the far side off.
 */
export const harbourPatrol = (g: Game, f: Fleet): boolean => f.duty === 'port' && !atSea(f) && f.ships.dd > 0 && farOn(g);

/**
 * How much of a fleet's hunting weight falls on a sea: all of it for a fleet sent there to hunt, part for an escort
 * whose lane crosses it or a patrol there (and, on the far side, for the harbour patrols of a fleet laid for harbour
 * in the sea its port stands on).
 */
function huntShare(g: Game, f: Fleet, z: number): number {
  const H = FLEETS.hunt;
  if (!atSea(f)) return f.home === z && harbourPatrol(g, f) ? H.loose : 0;
  if (f.stance === 'escort') {
    const rt = seaRoutes(g).find((r) => r.id === f.lane);
    return rt && rt.zones.includes(z) ? H.escort : f.zone === z ? H.loose : 0;
  }
  if (f.zone !== z) return 0;
  return f.stance === 'hunt' ? H.hunter : f.stance === 'patrol' ? H.loose : 0;
}

/**
 * What a nation's hunters are worth against the submarines in one sea: its destroyers and carriers there, and the
 * aircraft on the coasts. With Units decide (the convoy war by cause) radar is the kit its ships carry, not its lead
 * over its own schedule, and the weight comes in parts: 'air', the aircraft's, and 'kit', the ships' weight times
 * what their kit and training make of each attack against depth charges alone (OEG T10). `but`: a fleet left out of
 * the count (the far side's huntingSea asks what the others bring).
 */
export function huntersIn(g: Game, nat: string, z: number, but?: Fleet): { weight: number; fleets: Fleet[]; air?: number; kit?: number } {
  const H = FLEETS.hunt;
  const cause = unitsDecide(g);
  const k = cause ? aswKit(g, nat) : null;
  let w = 0, kit = 0;
  const fleets: Fleet[] = [];
  for (const f of g.fleets) {
    if (f.nat !== nat || f === but) continue;
    // (A fleet in harbour sends its destroyers alone: its carriers' aircraft are not flown from the anchorage.)
    const out = atSea(f);
    if (!out && !harbourPatrol(g, f)) continue;
    let p = (f.ships.dd + (out ? f.ships.cv * H.carrier * deckWorth(g, f) : 0)) * huntShare(g, f, z) * quality(g, nat) * fit(g, f);
    if (k) { p *= radarKit(k); kit += p * kitEdge(k, f.stance === 'hunt'); }
    if (p > 0) { w += p; fleets.push(f); }
  }
  // (Release 2, the sea patrol, once live: the aircraft hunt on patrol, by their own count and kit (patrolSubs), in place of this flat term.)
  const air = cause && patrolOn(g) ? 0 : ownAirOver(g, z, nat) * H.air;
  w += air;
  return k ? { weight: w, fleets, air, kit } : { weight: w * techMul(g, nat, 'radar'), fleets };
}

/**
 * The aircraft a nation itself flies from the coasts of a sea. The hunt counts these and not the whole alliance's
 * air cover, which every ally would otherwise sink submarines with in turn.
 */
function ownAirOver(g: Game, z: number, nat: string): number {
  const zone = g.seas[z];
  const n = g.nations[nat];
  if (!zone.coast.length || !n || !n.alive || !n.atWar.length) return 0;
  const wx = seaSkyMul(g, z);
  if (wx === 0) return 0;
  let s = 0;
  for (const id of zone.coast) {
    const r = g.regions[id];
    if (r.since >= g.turn) continue;
    const sky = skyOver(g, id);
    if (r.ctrl === nat) s += n.air * (sky ? AIR.sky.seaFoe : 1);
    else if (sky === nat) s += n.air * AIR.sky.seaOwn;
  }
  return (s / zone.coast.length) * zone.air * wx;
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
export function huntSubs(g: Game, nat: string, nights?: ConvoyNights): Record<number, number> {
  const out: Record<number, number> = {};
  for (const zone of g.seas) {
    const z = zone.id;
    const quarry = g.fleets.filter((f) => f.zone === z && atSea(f) && f.ships.ss > 0 && atWar(g, f.nat, nat));
    if (!quarry.length) continue;
    const h = huntersIn(g, nat, z);
    // (The far side's one measure: the boats out are the boats that can be found and sunk. `boatsOut`.)
    const subs = quarry.reduce((s, f) => s + boatsOut(g, f), 0);
    const share = huntShareSunk(h.weight, subs);
    if (share <= 0) continue;
    // (Units decide: the ships' part of the hunt goes by their kit; a boat raiding a convoy route of this nation's side
    // meets the convoy's escort instead (convoyWar), and here only the aircraft and the ships' share of its time off it.
    // With the escorts' losses form laid, the support groups in the route's seas work its incidents as coordinated hunts,
    // shared over its convoys at sea at once (escortChance), so the hunt's ships never take a raiding boat's time on the
    // route here as well: nothing is counted twice, Q4 (a).)
    const parts = h.kit !== undefined && h.weight > 0 ? { air: (share * h.air!) / h.weight, ships: (share * h.kit) / h.weight } : null;
    const rng = seaRng(g, `hunt:${nat}:${z}`);
    // (Release 2, ruling 241 D1, record only: the sea's hunt as the submarine scene stages it, while its setting is on.)
    const look = subScene.on ? { zone: z, hunters: h.fleets.map((f) => lookFleet(f, f.ships.dd)), air: ownAirOver(g, z, nat), quarry: quarry.map((f) => lookFleet(f, f.ships.ss)), sunk: 0, cost: 0 } : null;
    let sunk = 0;
    for (const f of quarry) {
      if (parts) {
        const raiding = onRouteOf(g, nat, f) ? raidShare(f) : 0;
        // (Inside the hunt's cap for the whole month, whoever hunts the fleet and whatever convoy it attacks. The far
        // side's one measure: the boats out are the boats hunted, `boatsOut`, every boat on hand with the far side off.)
        const x = Math.min(boatsOut(g, f) * Math.min(FLEETS.hunt.most, parts.air + parts.ships * (1 - raiding)), roomLeft(g, f));
        // (A night the player fought on a convoy route stands for the convoy's incidents there, so it books them, not this roll.)
        const k = Math.min(f.ships.ss, raiding > 0 ? rng.roll(x) : booked(nights, 'ss', f.id, rng.roll(x), x));
        noteLost(g, f, k);
        f.ships.ss -= k; f.lost.ss += k; sunk += k;
        loseStars(f, 'ss', k);
        continue;
      }
      // (A night the player fought books its boats lost here, for its own raiders, inside the roll's run: release 2.
      // The far side's one measure: the boats out are the boats hunted, `boatsOut`.)
      const k = Math.min(f.ships.ss, booked(nights, 'ss', f.id, rng.roll(boatsOut(g, f) * share), boatsOut(g, f) * share));
      f.ships.ss -= k; f.lost.ss += k; sunk += k;
      loseStars(f, 'ss', k);
    }
    if (look) { look.sunk = sunk; lookOf(g, nat).hunts.push(look); }
    if (!sunk) continue;
    out[z] = sunk;
    // The submarines hit back at the groups sent out after them.
    const groups = h.fleets.filter((f) => f.stance === 'hunt' && f.ships.dd > 0);
    const dd = groups.reduce((s, f) => s + f.ships.dd, 0);
    let cost = 0;
    for (const f of groups) {
      const k = Math.min(f.ships.dd, rng.roll(sunk * FLEETS.hunt.cost * (f.ships.dd / dd)));
      f.ships.dd -= k; f.lost.dd += k; cost += k;
      loseStars(f, 'dd', k);
    }
    if (look) look.cost = cost;
    // (A fleet at sea is named before one whose harbour patrols alone were out: the far side's.)
    const first = h.fleets.find(atSea) ?? h.fleets[0];
    const who = groups.length ? `Destroyers of ${fleetName(groups[0])}` : first ? `${atSea(first) ? 'The escorts' : 'The harbour patrols'} of ${fleetName(first)}` : `${g.nations[nat].adj} aircraft`;
    const told = !g.human || g.human === nat || allied(g, g.human, nat) || quarry.some((f) => f.nat === g.human);
    const subsSunk = `${sunk} ${g.nations[quarry[0].nat].adj} submarine${sunk === 1 ? '' : 's'}`;
    // (The harbour patrols' line counts one boat in words, the UI director's of 20:04Z; the older lines stand as they were.)
    const sinks = groups.length || !first || atSea(first) || sunk !== 1 ? subsSunk : `a ${g.nations[quarry[0].nat].adj} submarine`;
    if (told) routineNews(g, `hunt:${nat}:${z}`, 'battle', `${who} sink ${sinks} ${seaWhere(zone.name)}${cost ? `, for the loss of ${cost} destroyer${cost === 1 ? '' : 's'}` : ''}.`, `${subsSunk} sunk ${seaWhere(zone.name)}`, nat, undefined, { arm: 'sea', won: nat, lost: quarry[0].nat });
  }
  return out;
}

/**
 * The sea patrol's month for one nation (release 2, Units decide, once live; seapatrol.ts, Q4 (a)): in every sea its
 * bomber groups patrol, the enemy boats on station there are attacked as often as the patrols' aircraft watch them,
 * at the attacks a boat-month and the kills an attack of the kit they carry; this nation's aircraft sink their share of
 * those, by their share of the aircraft on patrol, so that allies do not sink the same boats twice. Booked inside the
 * hunt's cap for the month, on dice of their own ('patrol:<nation>:<sea>'). The boats sunk join the month's hunt by sea
 * ('sunk', which the convoys' record reads), and the month's record by the boats' nation and sea (g.patrolSunk), which the
 * boats' own side reads as its boats lost to aircraft (seasight.ts).
 * The same record keeps, by sea and the boats' nation, this nation's part of the watch its patrols kept against boats on
 * station there this month ('met': the parts of a boat's time watched by its aircraft with kit and without, its share
 * of the watch by its share of the aircraft on patrol, as the kills are shared). That is the watch as it acted on the
 * boats, which their own side reads as the aircraft its boats met there (seasight.ts): a patrol is seen by doing it,
 * once it has flown its month over the boats, never at its order (the design director's ruling of 24 November 2026
 * 17:54Z). Throws no dice.
 */
function patrolSubs(g: Game, nat: string, sunk: Record<number, number>): void {
  const zones = [...new Set(g.airFleets.filter((f) => f.nat === nat && f.mission === 'seapatrol').map((f) => f.target))].filter((z) => !!g.seas[z]).sort((a, b) => a - b);
  const rec: Record<string, number> = {}, seas: Record<number, number> = {}, at: Record<number, Record<string, number>> = {};
  const met: Record<number, Record<string, { kit: number; none: number }>> = {};
  const kit = airKit(aswKit(g, nat));
  for (const z of zones) {
    const quarry = g.fleets.filter((f) => f.zone === z && atSea(f) && f.ships.ss > 0 && atWar(g, f.nat, nat));
    if (!quarry.length) continue;
    // (The watch this nation's patrols kept over the boats on station here, by the boats' nation.)
    for (const f of quarry) {
      if (met[z]?.[f.nat]) continue;
      const w = watchAgainst(g, z, f.nat);
      const mine = w?.by[nat] ?? 0;
      if (!w || mine <= 0) continue;
      const part = ((w.kit + w.none) * mine) / w.A;
      (met[z] ??= {})[f.nat] = kit ? { kit: part, none: 0 } : { kit: 0, none: part };
    }
    const rng = seaRng(g, `patrol:${nat}:${z}`);
    let n = 0;
    // (The boats sunk in this sea, by their nation, for the month's words.)
    const here: Record<string, number> = {};
    for (const f of quarry) {
      const w = watchAgainst(g, z, f.nat);
      const mine = w?.by[nat] ?? 0;
      if (!w || mine <= 0) continue;
      const x = Math.min(f.ships.ss * onStation(g, f, z) * w.s * w.kills * (mine / w.A), roomLeft(g, f));
      const k = Math.min(f.ships.ss, rng.roll(x));
      if (k <= 0) continue;
      noteLost(g, f, k);
      f.ships.ss -= k; f.lost.ss += k; n += k;
      loseStars(f, 'ss', k);
      rec[f.nat] = (rec[f.nat] ?? 0) + k;
      here[f.nat] = (here[f.nat] ?? 0) + k;
    }
    if (!n) continue;
    sunk[z] = (sunk[z] ?? 0) + n;
    seas[z] = n;
    at[z] = here;
    const told = !g.human || g.human === nat || allied(g, g.human, nat) || quarry.some((f) => f.nat === g.human);
    // (Each nation's boats by its own name: "1 German and 2 Italian submarines". The news is tagged with the nation
    // that lost the most, the first of them on a tie.)
    const boats = Object.keys(here);
    const subs = `${andList(boats.map((x) => `${here[x]} ${g.nations[x].adj}`))} submarine${n === 1 ? '' : 's'}`;
    const most = boats.reduce((a, b) => (here[b] > here[a] ? b : a));
    if (told) routineNews(g, `patrol:${nat}:${z}`, 'battle', `${g.nations[nat].adj} aircraft on patrol sink ${subs} ${seaWhere(g.seas[z].name)}.`, `${subs} sunk from the air ${seaWhere(g.seas[z].name)}`, nat, undefined, { arm: 'sea', won: nat, lost: most });
  }
  (g.patrolSunk ??= {})[nat] = { turn: g.turn, sunk: rec, seas, at, met };
  // (Q6 (a): the aircraft lost on patrol, after the month's hunt, and only these two: an operational share of those on
  // patrol where it is laid, and the enemy's fighters where they reach the sea and meet it, as they meet a strike there
  // (seapatrol.ts patrolLoss). Booked to the group's and the nation's losses, as a raid's are.)
  for (const f of g.airFleets) {
    if (f.nat !== nat || f.mission !== 'seapatrol' || !g.seas[f.target] || f.bmr <= 0) continue;
    const x = patrolLoss(g, f, f.target);
    const k = Math.min(f.bmr, seaRng(g, `patrol:lost:${nat}:${f.id}`).roll(x.op + x.fighters));
    if (k > 0) { f.bmr -= k; f.lost.bmr += k; airPool(g, f.nat).lostBmr += k; }
  }
}

/**
 * The boats a nation's hunt would sink this month among these enemy fleets in one sea, on average, as huntSubs counts
 * them; with Units decide, also those the convoys' escorts there would sink in their attacks (convoyWar). For the fleet
 * orders' look-ahead. Off, it is the boats times the hunt's share, as before. Boats not raiding keep the hunt's curve
 * (its 1 - raiding split); a raiding boat meets the escort, with the escorts' losses form laid by cause (escortWork and
 * escortChance: the incidents from the boats that try, coordinated with the support groups in the route's seas, shared
 * over its convoys at sea at once, and the escorts beyond the screen's stations, independent with those on them), and
 * the hunt's ships never take it on the curve as well.
 */
export function huntExpected(g: Game, nat: string, z: number, quarry: Fleet[]): number {
  const h = huntersIn(g, nat, z);
  const subs = quarry.reduce((s, f) => s + f.ships.ss, 0);
  const share = huntShareSunk(h.weight, subs);
  if (h.kit === undefined) return subs * share;
  let n = 0;
  for (const f of quarry) {
    const raiding = onRouteOf(g, nat, f) ? raidShare(f) : 0;
    let x = h.weight > 0 ? (share * (h.air! + h.kit * (1 - raiding))) / h.weight : 0;
    // (Release 2, the sea patrol, once live: what this nation's aircraft on patrol there would sink, as patrolSubs counts it.)
    const w = patrolOn(g) ? watchAgainst(g, z, f.nat) : null;
    if (w && (w.by[nat] ?? 0) > 0) x += onStation(g, f, z) * w.s * w.kills * (w.by[nat] / w.A);
    if (raiding > 0) {
      for (const r of NAVY.convoys) {
        const to = g.nations[r.to];
        // (The incidents a boat meets: the route's count a boat on station, or the per-boat share where it is not laid;
        // with the escorts' losses form laid, the route's incidents by cause and who works them, as convoyWar counts them.)
        if (to?.alive && to.atWar.length && allied(g, r.to, nat) && atWar(g, f.nat, r.to) && r.zones.some((k) => zoneOf(g, k) === z)) {
          const count = convoyCount(g, r), w = escortWork(g, r.to, r, count);
          x += incidentsPerBoat(g, f, count, w) * (w ? escortChance(g, r.to, w).chance : incidentChance(g, r.to, r.id, z));
        }
      }
    }
    n += Math.min(f.ships.ss * Math.min(FLEETS.hunt.most, x), roomLeft(g, f));
  }
  return n;
}

/** The share of these enemy boats a nation's hunt would sink this month: the hunt's share, exactly as before, unless Units decide is on (then huntExpected's, per boat). */
export function huntShareOf(g: Game, nat: string, z: number, quarry: Fleet[]): number {
  const subs = quarry.reduce((s, f) => s + f.ships.ss, 0);
  if (!unitsDecide(g)) return huntShareSunk(huntersIn(g, nat, z).weight, subs);
  return subs > 0 ? huntExpected(g, nat, z, quarry) / subs : 0;
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
  // (Boats in passage to a new base fire on nothing on their way: holdsSea, release 2, the far chart.)
  for (const x of g.fleets) if (x.zone === z && holdsSea(x) && x.ships.ss > 0 && atWar(g, x.nat, f.nat)) subs += boatsOut(g, x) * quality(g, x.nat) * fit(g, x);
  return Math.min(FLEETS.screen.most, subs * FLEETS.screen.risk * gap);
}

/** The grand campaign: heavy ships at sea among enemy submarines without destroyers enough to screen them may be torpedoed. */
function torpedoes(g: Game, nat: string): void {
  for (const f of fleetsOf(g, nat)) {
    if (!atSea(f)) continue;
    const p = torpedoRisk(g, f, f.zone);
    if (p <= 0) continue;
    const rng = seaRng(g, `torpedo:${f.id}`);
    // One draw says whether a ship is hit, as ever. Ruling 240 (C3): the same draw says whether it sinks, which with
    // damage control is only when it is under p / dc (dcOf: exactly 1 without it, and then every ship hit goes down).
    const u = rng.next();
    if (!(u < p)) continue;
    const heavy: ShipClass[] = ['bb', 'cv', 'ca'];
    let pick = rng.range(0, f.ships.bb + f.ships.cv + f.ships.ca);
    let cls: ShipClass = 'ca';
    for (const c of heavy) { if (pick < f.ships[c]) { cls = c; break; } pick -= f.ships[c]; }
    if (f.ships[cls] <= 0) continue;
    const sinks = u < p / dcOf(g, f.nat);
    if (sinks) {
      f.ships[cls]--; f.lost[cls]++;
      if (cls !== 'ca') nameLosses(g, f.nat, f.name, { ...none(), [cls]: 1 }, `torpedo:${g.turn}:${f.id}`); // her name is gone with her (a record only)
      loseStars(f, cls, 1);
    }
    afloatWatch.tell?.({ route: 'torpedo', nat: f.nat, sunk: { ...none(), [cls]: sinks ? 1 : 0 }, saved: { ...none(), [cls]: sinks ? 0 : 1 } });
    f.ready = Math.max(0, f.ready - 0.05);
    if (subScene.on) lookTorpedo(g, f, cls, sinks);
    if (sinks) headline(g, 'battle', `A submarine torpedoes and sinks a ${NAVY.classes[cls].one} of ${fleetName(f)} ${seaWhere(g.seas[f.zone].name)}: the fleet has too few destroyers to screen its heavy ships.`, nat, undefined, { arm: 'sea', lost: nat });
    // (The ship is hit and the repair parties save it: the fleet is the less fit for sea all the same.)
    // (Rider 2 of 22 November: the fleet is named once, "One of its cruisers is badly hit, ...".)
    else headline(g, 'battle', `A submarine attacks ${fleetName(f)} ${seaWhere(g.seas[f.zone].name)}: the fleet has too few destroyers to screen its heavy ships. ${keptAfloat(f, { ...none(), [cls]: 1 }, 'its')}`, nat, undefined, { arm: 'sea' });
  }
}

/**
 * The far side's haven for a fleet whose base has fallen (the design director's ruling of 03:42Z, 22 November 2026, on
 * the Asiatic Fleet's bases; every nation, no number): a fed port held by the fleet's own nation or by an ally at war
 * with the enemy the base fell to, and never one lying in a sea that is closed to the fleet's nation. Fed is the
 * fleet's own nation's supply picture as it stands: whether its own forces are supplied there (the holder's picture,
 * where the fleet's nation has no army on the board to draw one).
 */
export function haven(g: Game, f: Fleet, r: Region): boolean {
  if (!r.port || !r.seas.length) return false;
  const foe = f.base >= 0 ? g.regions[f.base].ctrl : '';
  if (r.ctrl !== f.nat && !(allied(g, f.nat, r.ctrl) && !!foe && atWar(g, f.nat, foe) && atWar(g, r.ctrl, foe))) return false;
  if (!(g.supply[f.nat] ?? g.supply[r.ctrl])?.[r.id]) return false;
  return !closedTo(g, r.seas[0], f.nat);
}

/**
 * A fleet whose base has fallen makes for the nearest port its nation still holds. Returns false if there is none and
 * the fleet is lost. On the far side it makes for the nearest haven first (`haven`: fed, its own nation's or an ally's
 * at war with the same enemy, and in no closed sea); with none in its reach it reads as it did. `shut`: a fleet in
 * passage whose ways on and back are both shut to it (release 2, the far chart), its base still its side's.
 */
function newBase(g: Game, f: Fleet, form: boolean = farOn(g), shut = false): boolean {
  const old = g.regions[f.base];
  // (Release 2, the far chart: a move of base is a voyage, so only a harbour an open sea leads to from where it is.)
  const open = farOn(g) ? seasOpen(g, f) : null;
  const nearest = (ok: (r: Region) => boolean): Region | null => {
    let best: Region | null = null, bd = 1e9;
    for (const r of g.regions) {
      if (!r.port || !r.seas.length || !ok(r) || !canSailTo(g, f, r) || (open && !r.seas.some((z) => open[z]))) continue;
      const d = chartDist(g, r, old);
      if (d < bd) { bd = d; best = r; }
    }
    return best;
  };
  const best = (form ? nearest((r) => haven(g, f, r)) : null) ?? nearest((r) => r.ctrl === f.nat);
  if (best && rebase(g, f, best)) {
    headline(g, 'event', shut ? `With its way to ${old.name} and back shut, ${fleetName(f)} makes for ${best.name} instead.` : `With ${old.name} lost, ${fleetName(f)} moves its base to ${best.name}.`, f.nat, best.id);
    return true;
  }
  nameLosses(g, f.nat, f.name, f.ships, `scuttle:${g.turn}:${f.id}`); // the names go down with the ships (a record only)
  g.fleets.splice(g.fleets.indexOf(f), 1);
  if (shipCount(f.ships) >= 5) headline(g, 'event', `With no port left to it, ${fleetName(f)} is scuttled.`, f.nat, old.id, { arm: 'sea', lost: f.nat });
  return false;
}

/** Bombers flying from an enemy coast take their toll of a surface fleet that stays within their reach. */
function airAttack(g: Game, f: Fleet): void {
  if (!surfaceShips(f)) return;
  const net = airOver(g, f.zone, f.nat, true) - airOver(g, f.zone, f.nat, false);
  if (net <= NAVY.air.safe) return;
  // Carrier fighters on patrol over the fleet keep a share of them off it (the grand campaign).
  // Ruling 240 (C3): the fleet's damage control eases the toll, on the same draws, and the ships it kept afloat are
  // told. This runs in the history campaigns too: there dcOf is exactly 1 and nothing is other than it was.
  const kept = none();
  const sunk = sinkShips(f, Math.min(NAVY.air.maxLoss, (net - NAVY.air.safe) * NAVY.air.lossPer) * (1 - capOver(g, f)), seaRng(g, `air:${f.id}`), SURFACE, dcOf(g, f.nat), kept);
  nameLosses(g, f.nat, f.name, sunk, `coast:${g.turn}:${f.id}`); // their names are gone with them (a record only)
  f.ready = Math.max(0, f.ready - 0.1);
  afloatWatch.tell?.({ route: 'toll', nat: f.nat, sunk, saved: kept });
  // (Rider 2 of 22 November: the lead-in has named the fleet, so the line names it once: "One of its cruisers is ...".)
  const afloat = keptAfloat(f, kept, 'its');
  if (shipCount(sunk)) headline(g, 'battle', `Bombers sink ${shipList(sunk)} of ${fleetName(f)} ${seaWhere(g.seas[f.zone].name)}.${afloat ? ` ${afloat}` : ''}`, f.nat, undefined, { arm: 'air', lost: f.nat });
  else if (afloat) headline(g, 'battle', `Bombers attack ${fleetName(f)} ${seaWhere(g.seas[f.zone].name)}. ${afloat}`, f.nat, undefined, { arm: 'air' });
}

/** How much of its raiding strength a fleet at sea brings against cargoes: all of it when it is raiding, part when it is on patrol, none on other work. */
export const raidShare = (f: Fleet): number => (f.stance === 'raid' ? 1 : f.stance === 'patrol' ? NAVY.convoy.patrolRaid : 0);

/**
 * The far side (ruling 194, point 6): what a navy's submarines are worth as raiders this month, where its fleet is.
 * America's raid at a quarter in the far side's seas while their torpedoes fail: until September 1943 where the stop
 * keeps the book, and where it does not, until America's twenty-second month at war with Japan. 1 for every other
 * navy, in the old chart's seas, and with the far side off (navy.json has no 'subs' then).
 */
export function subsWorth(g: Game, f: Fleet): number {
  const S = NAVY.subs?.[f.nat];
  if (!S || f.zone < FAR_SEAS_FIRST || !farOn(g)) return 1;
  if (stopDef(g).book) return g.turn < S.book ? S.share : 1;
  const since = Math.max(g.flags[`war:${f.nat}:${S.foe}`] ?? 0, g.flags[`war:${S.foe}:${f.nat}`] ?? 0);
  return since > 0 && g.turn - since + 1 >= S.month ? 1 : S.share;
}

/**
 * The faulty torpedoes as the cards say them (the UI director's sheet 24, box 3; ruling 194(f)): the share of their
 * worth a navy's submarines raid at in the far side's seas, and the month they are mended. The month is the book's at
 * the stops that keep the book; elsewhere it is counted from the start of the navy's war with its foe, and before
 * that war no month is named ('until' null). Null once they are mended, and for every navy whose torpedoes work.
 * `subsWorth` is the rule; this reads it and changes nothing.
 */
export function torpedoFault(g: Game, nat: string): { share: number; until: number | null } | null {
  const S = NAVY.subs?.[nat];
  if (!S || !farOn(g)) return null;
  if (stopDef(g).book) return g.turn < S.book ? { share: S.share, until: S.book } : null;
  const since = Math.max(g.flags[`war:${nat}:${S.foe}`] ?? 0, g.flags[`war:${S.foe}:${nat}`] ?? 0);
  if (since <= 0) return { share: S.share, until: null };
  return g.turn - since + 1 >= S.month ? null : { share: S.share, until: since + S.month - 1 };
}
/**
 * The sentence: "Faulty torpedoes: your submarines raid at a quarter of their worth until September 1943." `boats` is
 * "your submarines", "these boats" or "your boats". Where no month can be named yet it ends "until the fault is
 * found" (the UI director's words of 20:04Z, 22 November 2026).
 */
export function torpedoWords(g: Game, nat: string, boats: string, month = true): string | null {
  const t = torpedoFault(g, nat);
  if (!t) return null;
  const share = t.share === 0.25 ? 'a quarter' : t.share === 0.5 ? 'half' : `${Math.round(t.share * 100)}%`;
  return `Faulty torpedoes: ${boats} raid at ${share} of their worth${!month ? '' : t.until !== null ? ` until ${turnToDate(t.until).label}` : ' until the fault is found'}.`;
}
/** Whether a navy's faulty boats are among the raiders in these seas: a fleet of its own at sea there with boats, raiding or on patrol, in a sea of the far side. */
export const faultyRaiders = (g: Game, nat: string, zones: number[]): boolean =>
  !!torpedoFault(g, nat) && g.fleets.some((f) => f.nat === nat && atSea(f) && f.ships.ss > 0 && zones.includes(f.zone) && f.zone >= FAR_SEAS_FIRST && raidShare(f) > 0);

/**
 * The far side's one measure for the boat (the design director's 18:03Z and 18:04Z of 22 November 2026): of a fleet's
 * submarines, how many are out on patrol this month. The rest are on passage to the beat and back or refitting at the
 * base. The same number raids the cargoes, is hunted, threatens an unscreened fleet, burns as ships at sea in the oil
 * book and weighs in holding a sea: before this every boat on hand raided and every boat on hand was hunted, so the
 * rule sank America's boats at half again the record's count. The share is the record's, by navy and month (navy.json
 * beats 'patrol', rows of [from this turn, boats out, of this many on hand]; the row in force is the latest begun, the
 * first before any has). Every boat for a navy with no row, in the old chart's seas, and with the far side off: there
 * Germany's measure stands, every boat on hand in the theatre at full weight. The rows and the pools stay boats on
 * hand: this is read only where a boat at sea is counted.
 */
export function boatsOut(g: Game, f: Pick<Fleet, 'nat' | 'zone' | 'ships'>): number {
  const row = patrolRow(g, f);
  return row ? (f.ships.ss * row[1]) / row[2] : f.ships.ss;
}
/** The patrol row in force for a fleet's boats where the one measure reads them (a far sea, the far side on); null where every boat on hand counts. */
export function patrolRow(g: Game, f: Pick<Fleet, 'nat' | 'zone'>): [number, number, number] | null {
  const P = NAVY.beats?.[f.nat]?.patrol;
  if (!P?.length || f.zone < FAR_SEAS_FIRST || !farOn(g)) return null;
  let row = P[0];
  for (const r of P) if (r[0] <= g.turn) row = r;
  return row;
}

/**
 * How much of its escort worth a friendly fleet at sea brings to the cargoes that cross these seas. In the history
 * campaigns a fleet on patrol in one of them guards them. In the grand campaign the fleets bound to the lane as its
 * escort guard it in full, and a fleet that only patrols or hunts in one of its seas gives distant cover.
 */
function guardShare(g: Game, f: Fleet, zones: number[], lane?: string): number {
  if (!seaWar(g)) return f.stance === 'patrol' && zones.includes(f.zone) ? 1 : 0;
  if (f.stance === 'escort' && lane !== undefined && f.lane === lane) return 1;
  // (A fleet in passage guards nothing on its way: release 2, the far chart.)
  return f.stance !== 'raid' && f.stance !== 'passage' && zones.includes(f.zone) ? FLEETS.escort.loose : 0;
}

/** The most of its cargoes a lane or a convoy route can lose in a month. */
const LANE_CAP = 0.95;

type ConvoyRoute = (typeof NAVY.convoys)[number];

/**
 * What a convoy the boats attack loses, from the convoys' side (the design director's tons ruling of 24 November 2026
 * 14:04Z, naval-battles/convoy-r2/tons-form.md): the pack's raid strength P against the convoy's escort E, the route's
 * escort worth shared over the convoys sailed. Ships sunk a convoy attacked = its ships x P / (P + k E), k refit once at
 * OEG T2's window, and never more than the pack's torpedoes sink on an average night (the boat ceiling, below): torps,
 * where it is laid. With the escorts' losses form laid (its Q5, 'price once', the design director's ruling of 24 November
 * 2026 17:58Z), E is the escorts on the close screen's stations with the convoy (onStations), a count of ships, and k is
 * set again once on it at the tons ruling's own record point, solved on the record's own E there, OEG T2's 6.5 escorts
 * a convoy (count.sinkOnStations; the design director's ruling of 24 November 2026 22:24Z, 1 (b)). The game's stations
 * at T2 (10.19) are more than the record's E because the game has too few convoys at sea, a fault named and never
 * tuned, so the contest there sinks fewer ships than the record point. With that figure unread, E and k as above.
 * L is the share of the route's ships sunk, both ways, and so of its cargoes.
 */
export interface ConvoySide { P: number; E: number; sunk: number; L: number; torps?: number; fire?: number; fired?: number }

/**
 * The share of the boats that try which reach firing against a convoy (the design director's ruling of 24 November 2026
 * 16:35Z on naval-battles/convoy-r2/radar-firing-form.md): 1 / (1 + c x r), r the radar-fitted escorts a convoy for
 * each boat trying, c set so the share is 't2' at T2's ratio 'rT2' (the record's, never the game's). 1 with no
 * radar-fitted escort. Null where it is not laid.
 */
export function firingShare(radarEscorts: number, tries: number): number | null {
  const F = CAUSE.firing;
  if (!F || F.t2 === null || F.rT2 === null || F.t2 <= 0 || F.rT2 <= 0) return null;
  const c = (1 / F.t2 - 1) / F.rT2;
  const r = tries > 0 ? radarEscorts / tries : 0;
  return 1 / (1 + c * r);
}

/**
 * The share of the boats that try which fired, from the record's share that torpedoed a ship: a boat that put n
 * torpedoes into a convoy at p a hit torpedoed at least one ship 1 - (1 - p)^n of the time (the design director's
 * rulings of 24 November 2026 17:13Z and 17:24Z on naval-battles/convoy-r2/eye-term-form.md Q2: n in hits' units).
 */
const firedOf = (effective: number, n: number, p: number): number => effective / (1 - Math.pow(1 - p, n));

/**
 * The eye term (the design director's ruling of 24 November 2026 17:13Z on naval-battles/convoy-r2/eye-term-form.md,
 * Q1 (a)): the share of the boats that try which fire = 1 / (1 + c_e e + c_r r), e every escort on the close screen's
 * stations for each boat trying (lookouts and asdic find, whatever the kit), r the radar-fitted ones (a radar-fitted
 * escort counts in both, for it keeps its lookouts). c_e and c_r are solved together on the record's two points, 1941's
 * sample and T2 (Q3), record against record, never on the game; t2 is the share at T2's own point, so the count's pack
 * comes back there. Null where a figure or the hit share is unread, or the two points give no pair of positive weights.
 */
export function eyePair(): { ce: number; cr: number; t2: number } | null {
  const E = CAUSE.firing?.eye, p = CAUSE.ceiling?.hit;
  if (!E || p == null || [E.effective41, E.n41, E.e41, E.r41, E.effectiveT2, E.nT2, E.eT2, E.rT2].some((x) => x == null)) return null;
  const f41 = firedOf(E.effective41!, E.n41!, p), fT2 = firedOf(E.effectiveT2!, E.nT2!, p);
  const det = E.e41! * E.rT2! - E.r41! * E.eT2!;
  if (!(f41 > 0 && f41 <= 1 && fT2 > 0 && fT2 <= 1) || Math.abs(det) < 1e-12) return null;
  const a = 1 / f41 - 1, b = 1 / fT2 - 1;
  const ce = (a * E.rT2! - b * E.r41!) / det, cr = (b * E.e41! - a * E.eT2!) / det;
  return ce >= 0 && cr >= 0 ? { ce, cr, t2: fT2 } : null;
}

/**
 * The share of the escorts bound to a route that is at sea with a convoy at any one time (the design director's
 * ruling of 24 November 2026 16:58Z, Q1 (d) of naval-battles/convoy-r2/screen-cycle-form.md): its days with a convoy
 * over its whole cycle, times the share of strength in service. Null while any day of station.dd is unread.
 */
export function escortsAtSea(): number | null {
  const D = CAUSE.station.dd;
  if (!D || D.convoy === null || D.turnaround === null || D.refit === null || D.unavailable === null || D.crossing === null) return null;
  const cycle = D.convoy + D.turnaround + D.refit;
  return cycle > 0 && D.crossing > 0 ? (D.convoy / cycle) * (1 - D.unavailable) : null;
}

/**
 * The close screen's stations round a convoy of the route (Q2 (b), the same ruling): the record's rule, a base and one
 * more station for so many columns of the convoy's front, the columns its ships over the ships a column, up to the
 * widest front (past it the columns go deeper). Null while
 * any figure is unread: the escorts with the convoy are not capped.
 */
export function screenStations(route: ConvoyRoute): number | null {
  const S = CAUSE.firing.screen, ships = CAUSE.count.ships[route.id];
  if (!S || S.stations === null || S.columnsAStation === null || S.shipsAColumn === null || S.maxColumns === null || !ships || S.columnsAStation <= 0 || S.shipsAColumn <= 0) return null;
  return S.stations + Math.min(ships / S.shipsAColumn, S.maxColumns) / S.columnsAStation;
}

/**
 * The escorts on a convoy's stations, all of them and the radar-fitted ones, for the firing share; 'with', all the escorts with the convoy. Escorts a convoy are those at sea with the convoys at
 * once over the convoys at sea at once (the cycle, Q1 (d)); while the cycle is unread, the escorts bound to the route
 * over the convoys sailed a month, as first built. Where the screen's stations are read, the escorts on stations are
 * the lesser of those with the convoy and the stations, times the radar-fitted share of them (Q2 (b)): a thin screen
 * fills fewer stations, and escorts beyond them add nothing to frustrating attacks (Q3 (a)). While the escorts' losses
 * form is unread they still count in the escort's strength E (Q4 (a)); with it laid they are spare ships that work
 * incidents as coordinated hunts (escortChance), and E is the escorts on the stations alone (its Q5).
 */
function onStations(route: ConvoyRoute, count: ConvoyCount, radarShips: number, escortShips: number): { all: number; radar: number; with: number } {
  if (count.sailed <= 0) return { all: 0, radar: 0, with: 0 };
  const atSea = escortsAtSea(), crossing = CAUSE.station.dd?.crossing ?? 0;
  const aConvoy = (ships: number): number => (atSea === null ? ships / count.sailed : (ships * atSea * MONTH_DAYS) / (count.sailed * crossing));
  const radar = aConvoy(radarShips), stations = screenStations(route), all = aConvoy(escortShips);
  if (stations === null) return { all, radar, with: all };
  const on = Math.min(all, stations);
  return { all: on, radar: all > 0 ? on * (radar / all) : 0, with: all };
}
const radarOnStations = (route: ConvoyRoute, count: ConvoyCount, radarShips: number, escortShips: number): number => onStations(route, count, radarShips, escortShips).radar;

/**
 * The convoys a route's escorts are shared over at any one time, as onStations counts them: with the escorts' cycle
 * read, the route's convoys at sea at once, its convoys sailed a month x a convoy's crossing over the month's days (Q1
 * (d); about 3.3 on the Atlantic at T2); while the cycle is unread, the convoys sailed a month, as first built. 0 where
 * none sailed. The escorts' losses form shares its support groups over the same count (escortChance).
 */
function convoysShared(count: ConvoyCount): number {
  if (count.sailed <= 0) return 0;
  return escortsAtSea() === null ? count.sailed : (count.sailed * CAUSE.station.dd!.crossing!) / MONTH_DAYS;
}

/**
 * The screen contest on a convoy attacked, as the convoy's side reads it: the boats that try, and the share of them that
 * fire (null where no firing share is laid). The escort's radar before a boat fires, ruled 16:35Z: the count's pack is
 * the boats that fired at T2, against escorts with radar, so the boats that try are the pack / t2, and those that fire
 * are the tries x the share at this convoy's own ratio of radar-fitted escorts to boats trying. At T2's ratio it gives
 * the count's pack back. With the eye term laid, 17:13Z, every escort on the stations finds as well, and t2 is the eye's
 * own share at T2's point. The eye term reads only its own figures: a null in radar alone's t2 or rT2 leaves a laid eye
 * running, and a null in any eye figure falls back to radar alone, as ruled (review of 24 Nov). The escorts' losses form
 * reads the same contest for its incidents (escortWork), so the screen is priced once.
 */
function screenContest(route: ConvoyRoute, count: ConvoyCount, radarShips: number, escortShips: number): { tries: number; fire: number | null } {
  const F = CAUSE.firing;
  const radarLaid = !!F && F.t2 !== null && F.rT2 !== null && F.t2 > 0;
  const eye = F ? eyePair() : null;
  const tries = eye ? count.pack / eye.t2 : radarLaid ? count.pack / F.t2! : count.pack;
  let fire: number | null = null;
  if (eye) {
    const on = onStations(route, count, radarShips, escortShips);
    fire = tries > 0 ? 1 / (1 + (eye.ce * on.all + eye.cr * on.radar) / tries) : 1;
  } else if (radarLaid) fire = firingShare(radarOnStations(route, count, radarShips, escortShips), tries);
  return { tries, fire };
}

function convoySide(route: ConvoyRoute, count: ConvoyCount, boats: number, G: number, ceilingAt: (fire: number) => RouteCeiling | null, radarShips = 0, escortShips = 0): ConvoySide {
  const K = CAUSE.count, ships = K.ships[route.id] ?? 0;
  // (The screen contest, applied once, here, on the pack the contest and the ceiling both read. With the escorts' losses
  // form unread the escort's kills keep the count's incidents; laid, they read this same contest, escortWork.)
  const { tries, fire } = screenContest(route, count, radarShips, escortShips);
  const pack = fire === null ? count.pack : tries * fire;
  // (The pack, times a boat's raid strength on station: its kit, fit and the air over it, as the lanes count them.)
  const P = count.boats > 0 ? (pack * boats) / count.boats : 0;
  // (The escorts' losses form, its Q5, 'price once' (the design director's ruling of 24 November 2026 17:58Z, from his
  // 16:58Z ruling): the escorts beyond the screen's stations now hunt as spare ships, so E is the escorts on the stations
  // with the convoy, and k is set again once on it at the tons ruling's own record point, solved on the record's own E
  // there, 6.5 escorts a convoy (count.sinkOnStations; the design director's ruling of 24 November 2026 22:24Z, 1 (b)).
  // The game's stations at T2 are more than that, for the game has too few convoys at sea: named, never tuned. With the
  // form or that figure unread, E is the route's escort worth over the convoys sailed and k the tons ruling's.)
  const kOn = escortsLaid() ? K.sinkOnStations ?? null : null;
  const E = kOn !== null ? onStations(route, count, radarShips, escortShips).all : count.sailed > 0 ? G / count.sailed : 0;
  const k = kOn ?? K.sink!;
  const contest = P > 0 ? (ships * P) / (P + k * E) : 0;
  // (The boat ceiling: never more than the pack's torpedoes sink on an average night, each boat's at most 'each'. A convoy
  // attacked has at least one boat on it, so the pack counts as one where the count's falls under one: on a short route
  // of many convoys, as Libya's, its incidents a convoy attacked are fewer than one. Builder's choice, review of 24 Nov.)
  // (The load form, 17:27Z Q3 (a): a boat holds one load for its patrol, so the torpedoes the route's firing boats put
  // into the month's convoys attacked are never more than its boats on station hold this month; past that each convoy's
  // share falls alike.)
  const ceil = ceilingAt(fire ?? 1);
  let each = ceil ? ceil.each : null;
  if (ceil && each !== null && ceil.torpedoes !== undefined && ceil.month !== undefined) {
    const used = count.attacked * Math.max(1, pack) * ceil.torpedoes;
    if (used > ceil.month) each *= ceil.month / used;
  }
  const torps = each === null ? undefined : Math.max(1, pack) * each;
  const sunk = torps === undefined ? contest : Math.min(contest, torps);
  return { P, E, sunk, L: count.sailed > 0 && ships > 0 ? (count.attacked * sunk) / (count.sailed * ships) : 0, ...(torps !== undefined ? { torps } : {}), ...(fire !== null ? { fire, fired: pack } : {}) };
}

// ---- The escort's incidents and kills, by cause (release 2, Units decide) -----------------------------------------
// The design director's ruling of 24 November 2026 17:58Z on naval-battles/convoy-r2/escort-losses-form.md, with his
// notes of 17:59Z and 18:00Z: (a) on Q1 to Q4, and Q5, 'price once', from his 16:58Z ruling; and his rulings of 22:24Z
// on the build's reading: k on the stations solved once on the record's own escorts, 1 (b); the route's seas as built,
// 2 (a); the support groups shared over the route's convoys at sea at once, 3 (b); the overshoot named, never tuned,
// 4 (a). One switch, convoycause.json 'escorts': laid as shipped; null, the escort meets the count's incidents (T2's
// 0.21 a boat on station) at the old split, and E and k stand as the tons ruling set them. One rule for every navy;
// nothing here reads the date or a flag.

/** True where the escorts' losses form is laid (convoycause.json 'escorts'). */
const escortsLaid = (): boolean => CAUSE.escorts === true;

/**
 * A convoy attacked, by cause (Q1 (a)): of the boats that try, the screen frustrates 1 - f and finds those it does not
 * frustrate after they fire at a = 1 - f, the same contest with the same escorts and the same weights (no new figure).
 * So the incidents a convoy attacked are the tries x [(1 - f) + f x a] = the tries x (1 - f^2). a = 1 - f is a one-cause
 * start, flagged: the record's bounds by period are checks, never fitted.
 */
export function escortsMet(tries: number, fire: number): { frustrated: number; after: number; met: number } {
  const a = 1 - fire;
  const frustrated = tries * (1 - fire), after = tries * fire * a;
  return { frustrated, after, met: frustrated + after };
}

/**
 * The escort's work on a route this month, by cause (the escorts' losses form): the screen contest's boats that try and
 * the share that fire, the incidents a convoy attacked (escortsMet), the rate a boat on station meets them in the month,
 * and the escorts a convoy on the close screen's stations and beyond them with each group's mean chance that an
 * incident ends with the boat sunk (OEG Table 10, by each fleet's kit and readiness: the independent column on the
 * stations, the coordinated beyond them), the route's seas, where its support groups hunt, and the route's convoys at
 * sea at once, which its escorts and support groups are shared over (convoysShared; escortChance).
 */
export interface EscortWork { tries: number; fire: number; frustrated: number; after: number; met: number; rate: number; on: number; beyond: number; onRate: number; beyondRate: number; seas: number[]; convoys: number }

/**
 * The escort's work on a route, as the fleets stand now: null where the form, Units decide or the route's count is not
 * laid. The escorts are those the screen counts (routeForces: each allied fleet at sea at its share on the route, every
 * escort vessel a destroyer), and the screen contest is the convoy's side's own (screenContest): with the eye unread,
 * radar alone's, and where no firing share is laid, f = 1, so the screen neither frustrates nor finds after firing and
 * no boat meets the escort. The month's incidents are the convoys attacked x the incidents a convoy attacked; they fall
 * to the raiding fleets by their share of the route's boats on station (incidentsPerBoat), as the count's do.
 */
export function escortWork(g: Game, nat: string, route: ConvoyRoute, count: ConvoyCount | null): EscortWork | null {
  if (!escortsLaid() || !count) return null;
  const zones = route.zones.map((k) => zoneOf(g, k));
  let all = 0, radar = 0, ind = 0, co = 0;
  for (const f of g.fleets) {
    if (!atSea(f) || atWar(g, f.nat, nat) || !allied(g, f.nat, nat)) continue;
    const w = guardShare(g, f, zones, route.id);
    if (w <= 0) continue;
    const dd = f.ships.dd * w, kit = aswKit(g, f.nat), q = fit(g, f);
    all += dd;
    if (kit.radar) radar += dd;
    ind += dd * q * incidentRate(kit, false);
    co += dd * q * incidentRate(kit, true);
  }
  const { tries, fire } = screenContest(route, count, radar, all);
  const f = fire ?? 1, m = escortsMet(tries, f);
  const s = onStations(route, count, radar, all);
  return {
    tries, fire: f, ...m,
    rate: count.boats > 0 ? (count.attacked * m.met) / count.boats : 0,
    on: s.all, beyond: Math.max(0, s.with - s.all),
    onRate: all > 0 ? ind / all : 0, beyondRate: all > 0 ? co / all : 0,
    seas: zones, convoys: convoysShared(count),
  };
}

/**
 * Who works an incident on a route (Q2 (a)): it is coordinated (OEG Table 10's coordinated column) with the chance
 * spare / (spare + on stations), and independent otherwise, all counted for one convoy. Spare is the escorts with the
 * convoy beyond the close screen's stations (they detach to hunt), counted a convoy as the screen counts them, plus the
 * support groups, its side's destroyers on hunting duty in the route's seas, as the ruling words it (the design
 * director, 24 November 2026 17:58Z, and accepted at 22:24Z, 2 (a)): a group hunting in any sea of the route is spare
 * for an incident in any of them, so the chance is the same in all the route's seas. A support group is in one place at
 * a time, so its ships are shared over the route's convoys at sea at once (w.convoys, the count the screen shares the
 * escorts over; the design director's ruling of 24 November 2026 22:24Z, 3 (b)), and never more than whole with one
 * convoy where fewer than one is at sea at once (the one-place-at-a-time rule: a group's share is its ships over the
 * larger of 1 and the convoys at sea, so no ship counts twice; kept by the design director, 24 November 2026 23:57Z, 3;
 * no route the game ships has fewer than about 3.3 at sea at once, so it never binds in play today); with no convoy at
 * sea the groups add nothing. The escorts are counted at
 * the escorts' cycle's share at sea over the convoys at sea at once (onStations), a group's ships in full over the same
 * count, for the game holds a fleet on hunting duty at sea all month and the ruling names no share at sea for a group:
 * so for one convoy a destroyer on hunting duty in the route's seas counts about 2.85 times one bound to the route's
 * escort ((1 + 0.25 x 0.384) against 0.384), and where the stations are not full that ratio alone sets the coordinated
 * share. Built as ruled, named, never tuned. In the grand campaign a group hunting in one of the route's seas also
 * counts at the loose share among the escorts with each convoy (escortWork), as the screen has always counted it: that
 * count is unchanged, but beside the group's own part, now shared, it is about a tenth of it (0.096), where with the
 * groups whole it was about 3% on the Atlantic route. Named and left as built. On stations is the escorts filling the
 * close screen. No new figure: each ship at its own navy's column and kit steps (incidentRate) and its readiness. The
 * chance that an incident ends with the boat sunk is the mean over them.
 */
export function escortChance(g: Game, nat: string, w: EscortWork): { coordinated: number; chance: number } {
  let H = 0, h = 0;
  const share = w.convoys > 0 ? 1 / Math.max(1, w.convoys) : 0;
  for (const f of g.fleets) {
    if (!atSea(f) || f.ships.dd <= 0 || f.stance !== 'hunt' || !w.seas.includes(f.zone) || !allied(g, f.nat, nat)) continue;
    H += f.ships.dd * share;
    h += f.ships.dd * share * fit(g, f) * incidentRate(aswKit(g, f.nat), true);
  }
  const spare = w.beyond + H, all = spare + w.on;
  return all > 0 ? { coordinated: spare / all, chance: (w.on * w.onRate + w.beyond * w.beyondRate + h) / all } : { coordinated: 0, chance: 0 };
}

/** True where every figure of the boat ceiling is laid. */
const ceilingLaid = (): boolean => {
  const C = CAUSE.ceiling;
  return !!C && C.inside !== null && C.hit !== null && C.sinkHit !== null;
};

/**
 * The share of the torpedoes carried outside the hull a boat can use in a sea this month: those need a calm sea (the
 * design director's ruling of 24 November 2026 14:49Z). With weather, all of them under a clear sky and none under cloud
 * or a closed sky; with no weather, the sea's sky as the weather table expects it this month, the chance of a clear sky
 * (the ruling of 16:27Z: the expected value, neither all calm nor all rough), and one of two where the table has no sky
 * for the sea.
 */
export function calmShare(g: Game, z: number): number {
  if (weatherOn(g)) return skyAtSea(g, z) === 'clear' ? 1 : 0;
  const zone = zoneOfSea(g, z);
  const o = zone >= 0 ? oddsOf(zone, g.turn) : undefined;
  const all = o ? o[0] + o[1] + o[2] : 0;
  return o && all > 0 ? o[0] / all : 0.5;
}

/**
 * The boat ceiling (the design director's rulings of 24 November 2026, 14:46Z, 14:49Z, 16:27Z and 16:31Z): the ships one
 * boat that fires can sink in one convoy battle on an average night, in a sea. Its torpedoes inside the hull, and those
 * carried outside it as far as the sea is calm (calmShare), x the share of torpedoes fired that hit, one figure for every
 * boat that fires (the escort's radar acts before a boat fires, not on its hits), x the ships sunk a hit. Nothing in it
 * reads the flag or the date. Null where a figure is not laid.
 */
export function boatCeiling(g: Game, z: number, fire = 1, at?: ConvoyAt): number | null {
  if (!ceilingLaid()) return null;
  const C = CAUSE.ceiling;
  return torpedoesInto(g, z, fire, at) * C.hit! * C.sinkHit!;
}

/** The convoy route a boat fires on and the boats' nation, where its next night is read. */
export interface ConvoyAt { route: string; nat: string }

/** True where the load form is laid on the ceiling: the ready tubes and the hours to reload them. */
const loadLaid = (): boolean => ceilingLaid() && CAUSE.ceiling.tubes != null && CAUSE.ceiling.tubes > 0 && CAUSE.load?.reload != null;

/** A boat's load for a convoy: its torpedoes inside the hull, and those outside it as far as the sea is calm. */
const loadIn = (g: Game, z: number): number => CAUSE.ceiling.inside! + calmShare(g, z) * (CAUSE.ceiling.outside ?? 0);

/**
 * The share of a night a further approach fits in, in a sea this month (the load form's Q2 (a), the design director's
 * ruling of 24 November 2026 17:27Z): the boat's ready tubes reloaded inside the dark hours left after an attack, the
 * attack falling at any hour of the dark alike, so 1 - reload hours / dark hours, none where the night is shorter than a
 * reload. The dark hours are the day's less its daylight (dayShare, the shared astronomy).
 */
export function nightFits(g: Game, z: number): number {
  const dark = 24 * (1 - dayShare(seaLatitude(g, z), turnToDate(g.turn).month));
  return dark > 0 ? Math.max(0, 1 - CAUSE.load!.reload! / dark) : 0;
}

/**
 * The salvos a boat that fires puts into one convoy (the load form, Q1 (a)): the first, and each further approach with
 * the chance q (it fires again past the screen, has kept contact and has reloaded in the dark), up to its load:
 * 1 + q + q^2 + ... over load / tubes salvos, the last a part salvo of what is left.
 */
export function salvosInto(q: number, salvos: number): number {
  if (salvos <= 0) return 0;
  const whole = Math.floor(salvos);
  let s = 0, t = 1;
  for (let k = 0; k < whole; k++) { s += t; t *= q; }
  return s + (salvos - whole) * t;
}

/**
 * The torpedoes one boat that fires puts into one convoy in a sea, at the convoy's firing share. As first built, its
 * whole usable load. With the load form laid (the design director's rulings of 24 November 2026 17:27Z and 17:37Z on
 * naval-battles/convoy-r2/load-per-convoy-form.md): salvos of its ready tubes (all of them each approach, both eras, F),
 * a further approach going ahead with q = the firing share x the share of the night it fits in x the share of boats that
 * regain contact after an attack (contact is lost running in, F). With that share unread, one approach.
 */
export function torpedoesInto(g: Game, z: number, fire = 1, at?: ConvoyAt): number {
  const load = loadIn(g, z);
  if (!loadLaid()) return load;
  const C = CAUSE.ceiling, regain = CAUSE.load!.regain;
  // (A further approach the same night: a reload in the dark left and the share that regain contact, unread. Failing that,
  // the next night: contact held through the day (nextNight). Either way it meets the screen again and fires at the share.)
  const w = regain == null ? 0 : nightFits(g, z) * regain;
  const h = at ? nextNight(g, z, at.route, at.nat) : 0;
  const q = fire * (w + (1 - w) * h);
  return C.tubes! * salvosInto(q, load / C.tubes!);
}

/**
 * The share of the boats that fired on a convoy one night that hold or regain contact for the next night (the load
 * form's Q2 (a), the design director's ruling of 24 November 2026 18:00Z: the next-night part from its causes; the
 * second run within one night stays unread). By day a boat runs on the surface after the convoy, and what its speed has
 * over the convoy's (1 - the convoy's speed / its own, none where the convoy is as fast) is its margin to work round
 * ahead of it for the next night, weighed over the route's convoys by class (load.next). Two things break contact: the
 * aircraft that keep boats down by day, as the sea patrol keeps them down (keptUp, as built: one cause acting once, and
 * nothing while the patrol is dormant), and a rough sea (the complement of calmShare, as built). 0 where a speed is unread.
 */
export function nextNight(g: Game, z: number, route: string, nat: string): number {
  const N = CAUSE.load?.next;
  const boat = N?.boat ?? null, classes = N?.convoys[route] ?? null;
  if (boat === null || boat <= 0 || !classes || !classes.length) return 0;
  let margin = 0, all = 0;
  for (const c of classes) {
    if (c.speed === null || c.share === null) return 0;
    margin += c.share * Math.max(0, 1 - c.speed / boat);
    all += c.share;
  }
  return all > 0 ? (margin / all) * keptUp(g, nat, z) * calmShare(g, z) : 0;
}

/** A route's boat ceiling: the ships a boat that fires sinks in one convoy at most; with the load form, the torpedoes it puts in and those the route's boats on station hold this month (the per-patrol cap). */
interface RouteCeiling { each: number; torpedoes?: number; month?: number }

/** The boat ceiling on a route at a firing share, over the boats on station on its seas as the count counts them (convoyCount): null where it is not laid or no boat is on station. */
function routeCeiling(g: Game, route: ConvoyRoute, fire = 1): RouteCeiling | null {
  if (!ceilingLaid()) return null;
  const zones = route.zones.map((k) => zoneOf(g, k));
  const load = loadLaid();
  let w = 0, c = 0, t = 0, held = 0;
  for (const f of g.fleets) {
    if (!atSea(f) || f.ships.ss <= 0 || !zones.includes(f.zone) || !atWar(g, f.nat, route.to)) continue;
    const b = f.ships.ss * raidShare(f) * onWatch(g, f, f.zone);
    const at = { route: route.id, nat: f.nat };
    w += b; c += b * boatCeiling(g, f.zone, fire, at)!;
    if (load) { t += b * torpedoesInto(g, f.zone, fire, at); held += b * loadIn(g, f.zone); }
  }
  if (w <= 0) return null;
  if (!load) return { each: c / w };
  // (A boat holds one load for its days on station, so in a month of them the boats on station hold 30 / patrol loads.)
  const patrol = CAUSE.station.ss?.patrol;
  return { each: c / w, torpedoes: t / w, ...(patrol ? { month: (held * MONTH_DAYS) / patrol } : {}) };
}

/**
 * A month's share lost and tons sunk on a route where the boats sink from the convoys' side, with the month's swing w
 * (1 where none is drawn): the swing acts on the ships sunk a convoy attacked, never past the convoy's ships, and on the
 * other raiders' share as on the lanes. Where the boat ceiling holds the ships sunk, the swing acts around it, so the
 * month's mean stays the card's (ruling 241's mean-1 swing): a good night's torpedoes hit more often than the average's; the two shares together at most LANE_CAP. The boats' tons are of the ships
 * sailing both ways, the others' of the loaded sailings, as the lanes count them.
 */
function sideLoss(route: ConvoyRoute, side: ConvoySide, Lo: number, w: number): { L: number; tons: number; exact: number; sunk: number } {
  const ships = CAUSE.count.ships[route.id] ?? 0;
  const sunk = Math.min(ships, side.sunk * w);
  const Lb = side.sunk > 0 ? side.L * (sunk / side.sunk) : 0;
  const lo = Lo * w, sum = Lb + lo, L = Math.min(LANE_CAP, sum);
  const cut = sum > 0 ? L / sum : 1;
  // (The card's tons are rounded to the thousand. 'exact' is the same tons before the rounding, for the escort's worth
  // (escortGain), which differences two months and would lose a small escort's tons in the rounding.)
  const exact = (CAUSE.count.ways * route.tons * Lb + route.tons * lo) * cut;
  return { L, tons: Math.round(exact / 1000) * 1000, exact, sunk: sunk * cut };
}

/** A route's tons ruling figures this month as the fleets stand now (Units decide, where laid), for the record and the readings. */
export function convoySideOf(g: Game, nat: string, route: ConvoyRoute): ConvoySide | undefined {
  return routeForces(g, nat, route).side;
}

/**
 * The raiders hunting a convoy route this month and the escorts guarding it, as convoyWar counts them: strength,
 * fleets, the escorts' worth with radar, and the share of the cargoes the raiders would sink. Throws no dice.
 */
function routeForces(g: Game, nat: string, route: ConvoyRoute): { zones: number[]; raid: number; guard: number; raiders: Fleet[]; escorts: Fleet[]; G: number; L: number; cover?: SeaCover[]; side?: ConvoySide; Lo?: number } {
  const zones = route.zones.map((k) => zoneOf(g, k));
  let raid = 0, guard = 0, boats = 0, weighed = 0, radarShips = 0, escortShips = 0;
  const raiders: Fleet[] = [], escorts: Fleet[] = [];
  // (Units decide: the raiders in a sea under air cover with anti-submarine kit sink at OEG T7's share of their rate.)
  const cover = unitsDecide(g) ? zones.map((z) => seaCover(g, nat, z, route.id)) : undefined;
  for (const f of g.fleets) {
    if (!atSea(f)) continue;
    if (atWar(g, f.nat, nat)) {
      if (!zones.includes(f.zone)) continue;
      // (Units decide: a fleet's boats raid only while on station, the one-pass ruling. The far side's one measure and
      // its faulty torpedoes are read in the boats' part, boatRaid: with neither on, its worth times the three together,
      // as it has always been multiplied.)
      const m = quality(g, f.nat) * fit(g, f) * raidShare(f) * (cover ? coverCut(cover[zones.indexOf(f.zone)]) : 1);
      // (Release 2, the sea patrol: boats kept down by aircraft without kit raid the convoys only for their time up.)
      const up = cover ? keptUp(g, f.nat, f.zone) : 1;
      const p = up === 1 ? raidPower(g, f, !!cover) * m : raidPower(g, f, !!cover) * m - boatRaid(g, f, !!cover) * m * (1 - up);
      if (p > 0) { raid += p; raiders.push(f); boats += boatRaid(g, f, !!cover, false) * m * up; weighed += boatRaid(g, f, !!cover) * m * up; }
    } else if (allied(g, f.nat, nat)) {
      const w = guardShare(g, f, zones, route.id);
      if (w <= 0) continue;
      let p = 0;
      for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].escort * (c === 'cv' ? deckWorth(g, f) : 1);
      const kit = aswKit(g, f.nat);
      p *= quality(g, f.nat) * fit(g, f) * w * (cover ? radarKit(kit) : 1);
      if (p > 0) { guard += p; escorts.push(f); }
      // (The escort ships on the route, and those with sea-search radar, for the firing share: every escort vessel is a
      // destroyer in the game's classes, counted at its share on the route.)
      escortShips += f.ships.dd * w;
      if (kit.radar) radarShips += f.ships.dd * w;
    }
  }
  if (route.base) {
    // An enemy fortress astride the route hunts the convoys for as long as it holds out, and harder while it is itself fed.
    const b = g.byName[route.base.region];
    if (b && atWar(g, b.ctrl, nat)) raid += route.base.raid * quality(g, b.ctrl) * (g.supply[b.ctrl] && !g.supply[b.ctrl][b.id] ? route.base.starved : 1);
  }
  // Radar and direction finding tell the escorts where the raiders are. (Units decide: the aircraft are the cover above,
  // and radar is each escort's own kit.)
  if (cover) {
    const G = guard * NAVY.convoy.escortWorth;
    // (The tons ruling: where the route's count and the sinking figure are laid, the boats sink from the convoys' side,
    // one convoy attacked at a time; any other raider, a surface ship or a fortress astride the route, keeps the lanes' form.)
    const count = (CAUSE.count.sink ?? null) === null ? null : convoyCount(g, route);
    if (count) {
      // (P counts a boat on station at 1; the others' part is the raid less the boats as the lanes weigh them.)
      const side = convoySide(route, count, boats, G, (fire) => routeCeiling(g, route, fire), radarShips, escortShips);
      const rest = raid - weighed;
      const Lo = rest > 1e-12 ? rest / (rest + G + NAVY.convoy.ocean) : 0;
      return { zones, raid, guard, raiders, escorts, G, L: sideLoss(route, side, Lo, 1).L, cover, side, Lo };
    }
    return { zones, raid, guard, raiders, escorts, G, L: raid > 0 ? raid / (raid + G + NAVY.convoy.ocean) : 0, cover };
  }
  for (const z of zones) guard += airOver(g, z, nat) * NAVY.convoy.air;
  const G = guard * NAVY.convoy.escortWorth * techMul(g, nat, 'radar');
  const L = raid > 0 ? raid / (raid + G + NAVY.convoy.ocean) : 0;
  return { zones, raid, guard, raiders, escorts, G, L };
}

// ---- The convoy battle's seam (release 2, ruling 241) -----------------------------------------------------------
// With the convoy battle on (a grand campaign that opened with stage.json 'convoyBattle'), every month on a convoy route
// takes a swing, and a night the player fights on one is booked back here. The field (ConvoySim) and the screen that
// offers the night are outside src/campaign; they reach the rules only through convoyField below and convoyhand.ts.
// Off, nothing here is read: no die is thrown and every number is release 1's.

/** The convoy battle's figures (data/campaign/convoybattle.json). */
export const CONVOY_BATTLE = convoyBattleData as { swing: { low: number; high: number }; hand: string[] };

/** True where this campaign plays the convoy battle: a grand campaign that opened with it (set at its start only). */
export const convoyRules = (g: Game): boolean => g.flags.open === 1 && g.flags.convoyBattle === 1;

/** A raider in a convoy's night: its fleet, its boats, and the boats its enemy's hunt would sink this month on average. */
export interface ConvoyRaider { id: number; nat: string; ss: number; expect: number }
/** An escort in a convoy's night: its fleet, its destroyers, and those the convoy's roll would cost it on average. */
export interface ConvoyEscort { id: number; nat: string; dd: number; expect: number }

/**
 * A month on a convoy route as the rules are about to fight it, before the hunt: the card's figures. The look-ahead
 * notes these on a copy; the real month asks the seam with the same meeting and books what it is given.
 */
export interface ConvoyMeeting {
  turn: number;
  route: string;
  /** The nation the route feeds, in whose month the convoy war is counted. */
  owner: string;
  zones: number[];
  raiders: ConvoyRaider[];
  escorts: ConvoyEscort[];
  /** The card: raiders' and escorts' strength, the escorts' worth with radar, the share of the cargoes the raiders would sink (before the swing), and its tons. */
  raid: number;
  guard: number;
  G: number;
  L: number;
  tons: number;
  /** Units decide, where the route's convoy count is laid: the convoys that sail this month, those sighted and attacked, the incidents with the escort at T2's rate, and the boats a convoy attacked, on average; with the escorts' losses form laid, also the incidents a convoy attacked by cause ('met'). */
  count?: ConvoyCountRecord;
}

/** A route's convoy count as the record and the card carry it: boats on station, convoys sailed, sighted and attacked, incidents, the pack, and where the tons are counted from the convoys' side the ships a convoy attacked lost (the card's expected figure; the month's record, after the swing), to two places. Where the escort's radar before a boat fires is laid, 'fired' is the boats that fire a convoy attacked, the pack the contest and the ceiling read (the card's boats each); with the sea patrol live, 'watched' is the share of the route's waters its aircraft watch; with the escorts' losses form laid, 'met' is the incidents with the escort a convoy attacked, by cause (escortWork). */
export type ConvoyCountRecord = ConvoyCount & { sunk?: number; torps?: number; fired?: number; watched?: number; met?: number };
const countRecord = (c: ConvoyCount, sunk?: number, torps?: number, fired?: number, watched?: number, met?: number): ConvoyCountRecord => ({ boats: r2(c.boats), sailed: r2(c.sailed), sighted: r2(c.sighted), attacked: r2(c.attacked), incidents: r2(c.incidents), pack: r2(c.pack), ...(sunk !== undefined ? { sunk: r2(sunk) } : {}), ...(torps !== undefined ? { torps: r2(torps) } : {}), ...(fired !== undefined ? { fired: r2(fired) } : {}), ...(watched !== undefined ? { watched: r2(watched) } : {}), ...(met !== undefined ? { met: r2(met) } : {}) });
/**
 * The share of a route's waters its side's aircraft on patrol watch this month (release 2, the sea patrol, once live;
 * Q8's convoy card): each sea's watched share, the seas weighed by their size as 1 / 'air' stands in for it, the scale
 * of the reading first ruled at 15:14Z. The interim reading the design director ruled at 17:49Z ('map') measures each
 * sea's waters on the chart (seaSquares), but the card's weights do not read that measure yet. Undefined where the
 * patrol is not live.
 */
function routeWatched(g: Game, nat: string, zones: number[]): number | undefined {
  if (!patrolOn(g)) return undefined;
  let w = 0, s = 0;
  for (const z of zones) {
    const size = 1 / Math.max(1e-6, g.seas[z].air);
    w += size; s += size * (watchFor(g, z, (x) => allied(g, x, nat))?.s ?? 0);
  }
  return w > 0 ? s / w : 0;
}
const r2 = (x: number): number => Math.round(x * 100) / 100;

/** A night the player fought, as the rules take it: the swing, and boats and destroyers lost by fleet id. A fleet left out rolls its dice. */
export interface ConvoyFought { swing: number; ss: Record<number, number>; dd: Record<number, number> }

/**
 * The seam: set by the screen around the month's close (convoyhand.ts), unset otherwise. Asked once a route-month,
 * while the convoy battle is on; undefined leaves the month to the staff's dice.
 */
export const convoyField: { fought?: (m: ConvoyMeeting) => ConvoyFought | undefined } = {};

/** The nights booked into one nation's month, by route: what the field gave, which fleets it covers, and whether any of it had to be held into the dice's run. */
export type ConvoyNights = Map<string, { fought: ConvoyFought; ss: Set<number>; dd: Set<number>; clamped: boolean; expect?: Map<number, number> }>;

/** The month's meeting on a convoy route, as the fleets stand now. Throws no dice. */
export function convoyMeeting(g: Game, nat: string, route: ConvoyRoute): ConvoyMeeting {
  const { zones, raid, guard, raiders, escorts, G, L, side, Lo } = routeForces(g, nat, route);
  const hunt = new Map<number, number>();
  // (Units decide: a night fought on the route is the convoy's incidents, so a raider's expected loss is theirs.)
  const cause = unitsDecide(g) && seaWar(g);
  if (cause) {
    // (The one-pass ruling: the convoys' side of the month, where its figures are laid; the card's sighted line. With the
    // escorts' losses form laid, the incidents by cause and who works them, alike in all the route's seas.)
    const count = convoyCount(g, route);
    const w = escortWork(g, nat, route, count);
    const laid = w ? escortChance(g, nat, w).chance : null;
    const p = new Map(zones.map((z) => [z, laid ?? incidentChance(g, nat, route.id, z)]));
    return {
      turn: g.turn, route: route.id, owner: nat, zones, raid, guard, G, L, tons: side ? sideLoss(route, side, Lo ?? 0, 1).tons : Math.round((route.tons * L) / 1000) * 1000,
      raiders: raiders.map((f) => ({ id: f.id, nat: f.nat, ss: f.ships.ss, expect: Math.min(f.ships.ss * incidentsPerBoat(g, f, count, w) * (p.get(f.zone) ?? 0), roomLeft(g, f)) })),
      escorts: escorts.map((f) => ({ id: f.id, nat: f.nat, dd: f.ships.dd, expect: raid > 0 ? raid * NAVY.convoy.escortLoss * (f.ships.dd / Math.max(1, escorts.reduce((s, e) => s + e.ships.dd, 0))) : 0 })),
      ...(count ? { count: countRecord(count, side?.sunk, side?.torps, side?.fired, routeWatched(g, nat, zones), w?.met) } : {}),
    };
  }
  if (seaWar(g) && g.nations[nat].atWar.length) {
    for (const z of zones) {
      const quarry = g.fleets.filter((f) => f.zone === z && atSea(f) && f.ships.ss > 0 && atWar(g, f.nat, nat));
      if (!quarry.length) continue;
      hunt.set(z, huntShareSunk(huntersIn(g, nat, z).weight, quarry.reduce((s, f) => s + f.ships.ss, 0)));
    }
  }
  const dd = escorts.reduce((s, f) => s + f.ships.dd, 0);
  return {
    turn: g.turn, route: route.id, owner: nat, zones, raid, guard, G, L, tons: Math.round((route.tons * L) / 1000) * 1000,
    raiders: raiders.map((f) => ({ id: f.id, nat: f.nat, ss: f.ships.ss, expect: f.ships.ss * (hunt.get(f.zone) ?? 0) })),
    escorts: escorts.map((f) => ({ id: f.id, nat: f.nat, dd: f.ships.dd, expect: raid > 0 ? raid * NAVY.convoy.escortLoss * (f.ships.dd / Math.max(1, dd)) : 0 })),
  };
}

/**
 * What a fleet's escort of a convoy route is worth this month to the admirals who weigh its duties (release 2, for the
 * general's fleetWithArmy; the design director's ruling of 25 November 2026 05:31Z on
 * naval-battles/convoy-r2/escort-gain-form.md, (a) on all five questions and Q6 as written). It throws no dice and
 * changes nothing, and nothing in the game calls it: the general's round 2 calls it under his own switch, fleetWithArmy.
 *
 * It is the route's month as the rules price it (routeForces, and the tons the card gives), run twice with the fleets
 * as they stand now: once with the fleet off the route, as if it lay in harbour, so that no share of it is on the
 * route; and once with it bound to the route as its full escort, at sea on that lane in the sea escort would sail it to
 * (escortSea), so that its guard share is 1. dL is the share of the route's cargoes saved, the month's L without the
 * fleet less its L with it. tons is the shipping saved, the same two months' tons before the card's rounding to the
 * thousand.
 *
 * Priced once (Q1 (a)): everything the escorts' form prices comes in through that one month, and nothing is added
 * beside it. With Units decide off the month is the lanes' old form, raiders / (raiders + G + ocean), so dL is that
 * form's; with it on, it is the convoys' side's, with the escorts' form where it is laid.
 *
 * Its points are dL on the admirals' scale (Q5 (a), marked G), by the battleship: a battleship is 150 on the strike
 * scale and 10 in fleet power, and a nation's whole month of goods is 3000 on the strike scale, as the sea patrol and
 * the carrier staff price trade, so the route's whole month is 200 x its share, and points = 200 x share x dL.
 * pointsByDestroyer is the same by the destroyer, 10 to 1, so 300 x share x dL: the (b) the ruling asked to be
 * reported beside it.
 *
 * Q2: a truth read today (Units decide audit item 28). It reads the boats at sea, as every admirals' term does, and it
 * moves to the boats its side last saw (seenBoats) with them in ruling 3's joint form, never alone. Q3: the boats the
 * escort sinks are left out. They sink nothing in later months, but that is a later month's gain, not this month's
 * tons. Q4: the escort's own losses (escortLoss) are not netted, for no admirals' term nets a fleet's own risk; if the
 * joint form brings that risk into the admirals' terms, it comes to every term at once. Q6: only a fleet that can
 * escort the route is priced.
 *
 * Null where the route is not a convoy route of navy.json (a lane of resources.json alone is priced by laneLoss, not
 * here), where the nation it feeds is not alive and at war, or where the fleet cannot escort it (whyNotEscort).
 *
 * The fleet is placed for each month and put back in a finally, as seapatrol.ts patrolWorth does: its sea, its stance
 * and its lane, and nothing else. Its lane is assigned again where it had one and taken off where it had none, so the
 * save writes its fields in the same order as before. sail is never called, so the supply picture is never marked to
 * be drawn again (the save writes that mark). The caches checked: seaRoutes' routeCache is kept by the game's seas and
 * built from the data's rows, and reads no fleet. The chart's own (seaAt's lookups, seaSquares, seaLatitude and
 * seapatrol.ts's squares within reach) read only the chart. The far map's speedups (the supply picture's SeaPicture,
 * game.ts's supply books and kept sea lanes, the seas a search by sea asks once) are made and read only by the supply
 * picture and the searches by sea, which routeForces never asks. seaCover, onWatch (onStation, passageSeas and
 * keptUp), convoyCount, routeCeiling and the screen's stations (onStations) keep nothing: each reads the fleets afresh
 * on every call, so the second month sees the fleet bound, and nothing of either placement is left once it is put
 * back. What else routeForces and the choice of sea reach that keeps anything reads no fleet: tech.ts's research by
 * nation (aswKit), the skies and gales dealt for the month (weather.ts and heavyseas.ts) and line.ts's spans (canPass).
 * The hunt cap's book on a fleet (capBook) is read by the meeting's expected losses, which this does not price.
 */
export function escortGain(g: Game, f: Fleet, route: string): { dL: number; tons: number; points: number; pointsByDestroyer: number } | null {
  const rt = NAVY.convoys.find((r) => r.id === route);
  if (!rt) return null;
  const to = g.nations[rt.to];
  if (!to?.alive || !to.atWar.length || whyNotEscort(g, f, route) !== null) return null;
  const lane = seaRoutes(g).find((r) => r.id === route);
  const z = lane ? escortSea(g, f, lane) : -1;
  if (z < 0) return null;
  // (The month as the card prices it: routeForces' L, and its tons, from the convoys' side where it is counted.)
  const month = (): { L: number; tons: number } => {
    const m = routeForces(g, rt.to, rt);
    return { L: m.L, tons: m.side ? sideLoss(rt, m.side, m.Lo ?? 0, 1).exact : rt.tons * m.L };
  };
  const was = { zone: f.zone, stance: f.stance, lane: f.lane, had: Object.prototype.hasOwnProperty.call(f, 'lane') };
  let off: { L: number; tons: number }, on: { L: number; tons: number };
  try {
    // (Off the route: in harbour, as sail sends a fleet home. A fleet in harbour reads no lane, so its own is left.)
    f.stance = 'port'; f.zone = f.home;
    off = month();
    // (Bound to the route as its full escort, in the sea escort would choose.)
    f.stance = 'escort'; f.zone = z; f.lane = route;
    on = month();
  } finally {
    f.zone = was.zone; f.stance = was.stance;
    if (was.had) f.lane = was.lane; else delete f.lane;
  }
  const dL = off.L - on.L;
  // (Q5 (a), marked G: 3000, a nation's whole month of goods on the strike scale as the sea patrol and the carrier staff
  // price trade, over a battleship's 150 there against its 10 in fleet power. (b), by the destroyer, beside it.)
  const points = (3000 / (CARRIERS.strike.value.bb / NAVY.classes.bb.power)) * rt.share * dL;
  const pointsByDestroyer = (3000 / (CARRIERS.strike.value.dd / NAVY.classes.dd.power)) * rt.share * dL;
  return { dL, tons: off.tons - on.tons, points, pointsByDestroyer };
}

/** The nights fought on this nation's convoy routes this month, asked of the seam before the hunt. Empty while the convoy battle is off or nobody has set the seam. */
function convoyNights(g: Game, nat: string): ConvoyNights | undefined {
  const ask = convoyField.fought;
  if (!ask || !convoyRules(g) || !g.nations[nat].atWar.length) return undefined;
  let out: ConvoyNights | undefined;
  for (const route of NAVY.convoys) {
    // (Only the routes a hand may fight: the convoys to Libya stay with the staff until their rows are checked.)
    if (route.to !== nat || !CONVOY_BATTLE.hand.includes(route.id)) continue;
    const m = convoyMeeting(g, nat, route);
    const fought = ask(m);
    if (!fought) continue;
    // (Units decide: the night stands for the convoy's incidents, booked against the expectation its card showed.)
    (out ??= new Map()).set(route.id, { fought, ss: new Set(m.raiders.map((r) => r.id)), dd: new Set(m.escorts.map((e) => e.id)), clamped: false, ...(unitsDecide(g) ? { expect: new Map(m.raiders.map((r) => [r.id, r.expect])) } : {}) });
  }
  return out;
}

/**
 * A roll as a fought night books it: the dice are thrown as ever (so no later roll moves); where the night covers this
 * fleet its own count stands in for them, held inside the roll's run (its floor and one over, core/rng.ts roll). A
 * count held in is marked, and the night then reads as a bare close result (b331).
 */
function booked(nights: ConvoyNights | undefined, kind: 'ss' | 'dd', id: number, rolled: number, expected: number, route?: string): number {
  if (!nights) return rolled;
  for (const [rid, n] of nights) {
    if (route !== undefined && rid !== route) continue;
    if (!n[kind].has(id)) continue;
    const want = n.fought[kind][id];
    if (want === undefined) return rolled;
    const lo = Math.floor(expected), hi = lo + 1;
    const k = Math.max(lo, Math.min(hi, Math.round(want)));
    if (k !== want) n.clamped = true;
    return k;
  }
  return rolled;
}

/** The month's swing on a convoy route: drawn on its own key, or the fought night's held inside the same bounds. */
function swingOf(g: Game, route: string, night?: { fought: ConvoyFought; clamped: boolean }): number {
  const { low, high } = CONVOY_BATTLE.swing;
  const drawn = low + (high - low) * seaRng(g, `convoySwing:${route}`).next();
  if (!night) return drawn;
  const w = night.fought.swing;
  const k = Math.max(low, Math.min(high, Number.isFinite(w) ? w : 1));
  if (k !== w) night.clamped = true;
  return k;
}

// ---- The submarine scene's record (release 2, ruling 241 D1) ----------------------------------------------------
// The month's submarine scene is look only: its viewing setting is outside the rules' save and no rule reads what is
// written here. While it is on, each nation's month at sea is written into g.subLook (this month and last) for the
// scene to stage; off, nothing is written and every save is release 1's.

/** The submarine scene's viewing setting: set by the screen (a run outside the browser by TC_SUBSCENE=1). */
export const subScene = { on: SUBSCENE_ENV };

const lookFleet = (f: Fleet, n: number): SubLookFleet => ({ fleet: f.id, nat: f.nat, n });
/** A raider as the scene stages it: its boats as the month began, and its heavy ships (battleships, carriers, cruisers), which raid too. */
const lookRaider = (f: Fleet, n: number): SubLookFleet => ({ ...lookFleet(f, n), heavy: f.ships.bb + f.ships.cv + f.ships.ca });

/** This nation's record for this month, made the first time it is written to; anything older than last month is let go. */
function lookOf(g: Game, nat: string): SubLook {
  const keep = (g.subLook ?? []).filter((l) => l.turn >= g.turn - 1);
  let l = keep.find((x) => x.turn === g.turn && x.nat === nat);
  if (!l) { l = { turn: g.turn, nat, hunts: [], torpedoes: [], convoys: [] }; keep.push(l); }
  g.subLook = keep;
  return l;
}

/** A torpedoing as the scene stages it: the boats' fleets in the sea that could have fired, and one picked on its own key, by their weight as torpedoRisk counts them. */
function lookTorpedo(g: Game, f: Fleet, cls: ShipClass, sunk: boolean): void {
  const could = g.fleets.filter((x) => x.zone === f.zone && holdsSea(x) && x.ships.ss > 0 && atWar(g, x.nat, f.nat));
  const w = could.map((x) => x.ships.ss * quality(g, x.nat) * fit(g, x));
  const all = w.reduce((a, b) => a + b, 0);
  let by: number | null = null;
  if (all > 0) {
    let pick = seaRng(g, `look:torpedo:${f.id}`).next() * all;
    for (let i = 0; i < could.length; i++) { pick -= w[i]; if (pick < 0 || i === could.length - 1) { by = could[i].id; break; } }
  }
  const byNat = by === null ? null : could.find((x) => x.id === by)!.nat;
  lookOf(g, f.nat).torpedoes.push({ fleet: f.id, zone: f.zone, cls, sunk, could: could.map((x) => x.id), by, couldNat: could.map((x) => x.nat), byNat });
}

/**
 * A convoy month as the scene stages it, sea by sea: the month's tons shared by the raiders' strength in each sea
 * (evenly where only a fortress raids). The raiders' boats and the escorts' destroyers are as the month began, before
 * the convoy's own roll (a history campaign's convoy sinks boats as well as destroyers); its strength shares are read
 * as the fleets stand after it, as before.
 */
function lookConvoy(g: Game, nat: string, route: string, zones: number[], raiders: Fleet[], escorts: Fleet[], at: { dd: Map<number, number>; ss: Map<number, number> }, tons: number, cause?: { cover: SeaCover[]; lost: Record<number, number> }): void {
  const power = (f: Fleet): number => raidPower(g, f, !!cause) * quality(g, f.nat) * fit(g, f) * raidShare(f);
  const by = zones.map((z) => raiders.filter((f) => f.zone === z).reduce((a, f) => a + power(f), 0));
  const all = by.reduce((a, b) => a + b, 0);
  // (An escort bound to the route from outside its seas is staged where the raiders are strongest.)
  const most = zones[by.indexOf(Math.max(...by))];
  const seas = zones.map((z, i) => ({
    zone: z,
    raiders: raiders.filter((f) => f.zone === z).map((f) => lookRaider(f, at.ss.get(f.id) ?? f.ships.ss)),
    escorts: escorts.filter((f) => f.zone === z || (z === most && !zones.includes(f.zone))).map((f) => lookFleet(f, at.dd.get(f.id) ?? f.ships.dd)),
    air: airOver(g, z, nat) * NAVY.convoy.air,
    tons: Math.round((all > 0 ? tons * (by[i] / all) : tons / zones.length) / 1000) * 1000,
    // (Units decide only: the kit of the navy with most destroyers answering attacks in the sea, each answering navy's
    // own kit by navy, the cover's kind, and the boats the escorts sank there. Off, none of these is written.)
    ...(cause ? { ...seaKits(g, nat, route, z), cover: cause.cover[i].kind, lost: cause.lost[z] ?? 0 } : {}),
  }));
  lookOf(g, nat).convoys.push({ route, tons, seas });
}

/**
 * The boats that met a route's convoys this month, sea by sea, for the record of what the route's nation has seen
 * (release 2, once the sea patrol is live; seasight.ts, the design director's ruling of 24 November 2026 17:54Z, Q2 (a)):
 * those that fired on its convoys (the count's convoys attacked x the boats that fire a convoy attacked) and those that
 * met its escort (the count's incidents, as the escorts' losses form counts them where it is laid (escortWork), or each
 * boat's per-boat share where the count is not laid), each fleet's part by its share of the route's boats on station as
 * the count shares them, and never more than its boats on station there.
 * Within a route a sea's fleets add up; between routes the most counts, since the same boats may meet both. Throws no dice.
 */
function metConvoys(g: Game, count: ConvoyCount | null, side: ConvoySide | undefined, raiders: Fleet[], seen: SeaMonth, work: EscortWork | null): void {
  const fired = count ? count.attacked * (side?.fired ?? count.pack) : 0;
  const attack: Record<number, number> = {}, incident: Record<number, number> = {};
  for (const f of raiders) {
    if (f.ships.ss <= 0) continue;
    const most = f.ships.ss * onStation(g, f, f.zone);
    const b = f.ships.ss * raidShare(f) * onWatch(g, f, f.zone);
    if (count && count.boats > 0) attack[f.zone] = (attack[f.zone] ?? 0) + Math.min(most, (fired * b) / count.boats);
    incident[f.zone] = (incident[f.zone] ?? 0) + Math.min(most, f.ships.ss * incidentsPerBoat(g, f, count, work));
  }
  for (const z of Object.keys(attack)) seen.attack[+z] = Math.max(seen.attack[+z] ?? 0, attack[+z]);
  for (const z of Object.keys(incident)) seen.incident[+z] = Math.max(seen.incident[+z] ?? 0, incident[+z]);
}

/** The allied navies answering attacks on a route's convoys in one sea (their groups hunting there and the close escort), each with its destroyers there, in the order of the fleets. */
function answering(g: Game, nat: string, route: string, z: number): Map<string, number> {
  const by = new Map<string, number>();
  for (const f of g.fleets) {
    if (!atSea(f) || f.ships.dd <= 0 || !allied(g, f.nat, nat)) continue;
    if ((f.stance === 'hunt' && f.zone === z) || (f.stance === 'escort' && f.lane === route)) by.set(f.nat, (by.get(f.nat) ?? 0) + f.ships.dd);
  }
  return by;
}

/** The navy with most destroyers answering attacks on a route's convoys in one sea, or the route's own. */
function answerer(g: Game, nat: string, route: string, z: number, by = answering(g, nat, route, z)): string {
  let best = nat, most = 0;
  for (const [k, n] of by) if (n > most) { most = n; best = k; }
  return best;
}

/**
 * A convoy sea's kits for the scene's record: each answering navy's own kit, keyed by its nation (the route's own
 * nation's alone where none answers), and as 'kit', as before, the kit of the navy with most destroyers there.
 */
function seaKits(g: Game, nat: string, route: string, z: number): { kit: KitLook; kits: Record<string, KitLook> } {
  const by = answering(g, nat, route, z);
  const kits = Object.fromEntries((by.size ? [...by.keys()] : [nat]).map((k) => [k, kitLook(aswKit(g, k))]));
  return { kit: { ...kits[answerer(g, nat, route, z, by)] }, kits };
}

/**
 * The month's fighting on the convoy routes that feed this nation: raiders against escorts. With 'seen' (release 2, once
 * the sea patrol is live), what each route's convoys met of the enemy's boats, sea by sea, for this nation's record of
 * what it has seen (metConvoys).
 */
export function convoyWar(g: Game, nat: string, sunkAt?: Record<number, number>, nights?: ConvoyNights, seen?: SeaMonth): void {
  const n = g.nations[nat];
  for (const route of NAVY.convoys) {
    if (route.to !== nat) continue;
    if (!n.atWar.length) { delete g.convoys[route.id]; continue; }
    const rng = seaRng(g, `convoy:${route.id}`);
    const { zones, raid, guard, raiders, escorts, G, L: L0, cover, side, Lo } = routeForces(g, nat, route);
    // (Release 2, ruling 241: with the convoy battle on, the month's share sunk takes a swing, drawn on its own key, or
    // the night the player fought gives it, inside the same bounds. Off, nothing is drawn and L is release 1's.)
    const night = nights?.get(route.id);
    const swing = convoyRules(g) ? swingOf(g, route.id, night) : undefined;
    // (The tons ruling: where the boats sink from the convoys' side, the swing acts on the ships sunk a convoy attacked.)
    const month = side ? sideLoss(route, side, Lo ?? 0, swing ?? 1) : undefined;
    const L = month ? month.L : swing === undefined ? L0 : Math.min(LANE_CAP, L0 * swing);
    const tons = month ? month.tons : Math.round((route.tons * L) / 1000) * 1000;
    let subsLost = 0, escortsLost = 0;
    let byEscorts: Record<number, number> | undefined;
    let count: ConvoyCount | null = null;
    let work: EscortWork | null = null;
    // (Release 2, ruling 241 D1, record only: the escorts' destroyers and the raiders' boats as the month began, before the convoy's roll.)
    const at = subScene.on ? { dd: new Map(escorts.map((f) => [f.id, f.ships.dd])), ss: new Map(raiders.map((f) => [f.id, f.ships.ss])) } : null;
    if (raid > 0) {
      // In the grand campaign the submarines have already been hunted, sea by sea, by the destroyers and aircraft alone.
      if (sunkAt) for (const z of zones) subsLost += sunkAt[z] ?? 0;
      else {
        const hunt = NAVY.convoy.subBase + NAVY.convoy.subHunt * (G / (raid + G + NAVY.convoy.ocean));
        for (const f of raiders) {
          const k = Math.min(f.ships.ss, rng.roll(f.ships.ss * hunt));
          f.ships.ss -= k; f.lost.ss += k; subsLost += k;
          loseStars(f, 'ss', k);
        }
      }
      const dd = escorts.reduce((s, f) => s + f.ships.dd, 0);
      for (const f of escorts) {
        const x = raid * NAVY.convoy.escortLoss * (f.ships.dd / Math.max(1, dd));
        const k = Math.min(f.ships.dd, booked(nights, 'dd', f.id, rng.roll(x), x, route.id));
        f.ships.dd -= k; f.lost.dd += k; escortsLost += k;
        loseStars(f, 'dd', k);
      }
      // (Units decide: boats lose where they fight. Each boat that finds a convoy is an incident with its escort, sunk at
      // the escort's rate; thrown on its own key, so no other roll of the month moves. A fought night books them. With
      // the escorts' losses form laid, the incidents are the boats the screen frustrates or finds after they fire, and an
      // incident is coordinated or independent by who is spare and who is on the stations, Q1 (a) and Q2 (a).)
      if (cover && sunkAt) {
        const irng = seaRng(g, `incident:${route.id}`);
        // (The one-pass ruling: what the boats on station meet is counted from the convoys' side, where its figures are laid.)
        count = convoyCount(g, route);
        work = escortWork(g, nat, route, count);
        // (Release 2, once the sea patrol is live: the boats its convoys and escort met on the route, sea by sea, before
        // any is lost, for the record of what this nation has seen.)
        if (seen) metConvoys(g, count, side, raiders, seen, work);
        const laid = work ? escortChance(g, nat, work).chance : null;
        for (const z of zones) {
          const p = laid ?? incidentChance(g, nat, route.id, z);
          let lost = 0;
          for (const f of raiders) {
            if (f.zone !== z || f.ships.ss <= 0) continue;
            const x = Math.min(f.ships.ss * incidentsPerBoat(g, f, count, work) * p, roomLeft(g, f));
            // (A fought night is held inside the run of the expectation its card showed, before the hunt; the dice roll on this month's.)
            const k = Math.min(f.ships.ss, booked(nights, 'ss', f.id, irng.roll(x), night?.expect?.get(f.id) ?? x, route.id));
            noteLost(g, f, k);
            f.ships.ss -= k; f.lost.ss += k; subsLost += k; lost += k;
            loseStars(f, 'ss', k);
          }
          if (lost) {
            (byEscorts ??= {})[z] = lost;
            if (seen) seen.sunk[z] = (seen.sunk[z] ?? 0) + lost;
          }
        }
      }
    }
    g.convoys[route.id] = { turn: g.turn, raid, guard, tons, total: (g.convoys[route.id]?.total ?? 0) + tons, share: route.share * L, loss: L, subsLost, escortsLost };
    // (Record only, and only with the convoy battle on: the month's swing, and whether a fought night set it.)
    if (swing !== undefined) g.convoys[route.id].swing = swing;
    if (night) g.convoys[route.id].fought = night.clamped ? 'clamped' : 'field';
    // (Record only, and only with Units decide: the boats the convoy's escorts sank, of subsLost.)
    if (cover) g.convoys[route.id].byEscorts = Object.values(byEscorts ?? {}).reduce((a, b) => a + b, 0);
    // (Record only, with Units decide and the route's count laid: the convoys' side of the month, as the boats stood after the hunt.)
    if (count) g.convoys[route.id].count = countRecord(count, month?.sunk, side?.torps, side?.fired, routeWatched(g, nat, zones), work?.met);
    // (Record only, with the sea patrol live: the boats this nation's aircraft on patrol sank in the route's seas, of subsLost.)
    if (cover && patrolOn(g)) { const ps = g.patrolSunk?.[nat]; g.convoys[route.id].byAircraft = ps?.turn === g.turn ? zones.reduce((a, z) => a + (ps.seas[z] ?? 0), 0) : 0; }
    if (at) lookConvoy(g, nat, route.id, zones, raiders, escorts, at, tons, cover && { cover, lost: byEscorts ?? {} });
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
  // (Units decide: air cover and kit as on a convoy route, routeForces.)
  const cover = unitsDecide(g) ? zones.map((z) => coverCut(seaCover(g, nat, z, lane))) : null;
  for (const f of g.fleets) {
    if (!atSea(f)) continue;
    if (atWar(g, f.nat, nat)) {
      if (!zones.includes(f.zone)) continue;
      const c = cover ? cover[zones.indexOf(f.zone)] : 1;
      const p = raidPower(g, f, !!cover) * quality(g, f.nat) * fit(g, f) * raidShare(f) * c;
      // (Release 2, the sea patrol, as on a convoy route (routeForces): boats kept down by aircraft without kit raid the
      // lane only for their time up. 1 where none watch, or the patrol is not live, and then the raid is as before.)
      const up = cover ? keptUp(g, f.nat, f.zone) : 1;
      raid += up === 1 ? p : p - boatRaid(g, f, !!cover) * quality(g, f.nat) * fit(g, f) * raidShare(f) * c * (1 - up);
    } else if (allied(g, f.nat, nat)) {
      const w = guardShare(g, f, zones, lane);
      if (w <= 0) continue;
      let p = 0;
      for (const c of CLASSES) p += f.ships[c] * NAVY.classes[c].escort * (c === 'cv' ? deckWorth(g, f) : 1);
      guard += p * quality(g, f.nat) * fit(g, f) * w * (cover ? radarKit(aswKit(g, f.nat)) : 1);
    }
  }
  if (!cover) for (const z of zones) guard += airOver(g, z, nat) * NAVY.convoy.air;
  const G = guard * NAVY.convoy.escortWorth * (cover ? 1 : techMul(g, nat, 'radar'));
  const loss = raid > 0 ? (raid / (raid + G + NAVY.convoy.ocean)) * seaLossMul(g, nat) : 0;
  // (Release 2, heavyseas.ts) A lane through a sea in a gale is late with more of its cargoes. Nothing with that switch off.
  return { raid, guard, loss: Math.min(LANE_CAP, loss + galeLaneLoss(g, zones)) };
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
  // (With a neutral's bargains the ore is Sweden's to sell, and its share follows the bargain: release 2.)
  if (nat === NAVY.ore.to && !replacedDeal(g, 'ore') && oreRoute(g) === 'cut') s += NAVY.ore.share;
  s += bargainOutputLoss(g, nat);
  // (Release 2, heavyseas.ts) Convoys through a gale are late. Nothing with that switch off.
  s += galeConvoyLoss(g, nat);
  // A nation that reads the raiders' signals steers its convoys round them (the grand campaign).
  return Math.min(0.9, s) * seaLossMul(g, nat);
}

// ---- Fleet actions ----------------------------------------------------------------------------------

const sideOf = (f: Fleet): SeaSide => ({ nat: f.nat, fleet: f.name, admiral: f.admiral?.name ?? null, ships: { ...f.ships }, sunk: none() });

/** A fleet action about to be fought, as the rules weigh it, for a screen that lets the player fight it out (src/battle/sea). */
export interface SeaMeeting { turn: number; zone: number; nat: string; mine: number[]; met: number[]; A: number; D: number; airA: number; airD: number }

/**
 * A fleet action fought out on the field (src/battle/sea): the ratio it came to, which stands in for the dice, and the
 * ships of each class each fleet lost there, by fleet id. Submarines take no part on the field: the dice sink them
 * from R as before. 'toDice': the player handed the rest of the action back to the dice, so the rules sink as they
 * would at R, and never fewer than had already gone down on the field. 'kept' (ruling 240): the ships each fleet's
 * damage control kept afloat on the field, by fleet id (the field caps its sinkings by each fleet's own toughness).
 */
export interface SeaFought { R: number; sunk: Record<number, Partial<Ships>>; toDice?: boolean; kept?: Record<number, Partial<Ships>> }

/**
 * The seam to the played fleet action (src/battle/sea/handoff.ts). Unset, as it is everywhere but on a screen that
 * offers the player his fleet actions, every action is settled by the dice exactly as before.
 */
export const seaField: { fought?: (m: SeaMeeting) => SeaFought | undefined } = {};

/**
 * What a fleet lost in an action fought out on the field: the ships that went down there, and its submarines as the dice
 * sink them. Handed back to the dice, the dice sink with the fleet's damage control as an action the dice fight does
 * (`dc`, `kept`: ruling 240); the ships the field sank are the field's, and so are those its damage control kept
 * afloat there (SeaFought.kept).
 */
function sinkFought(f: Fleet, p: SeaFought, frac: number, rng: Rng, dc = 1, kept?: Ships): Ships {
  const out = p.toDice ? sinkShips(f, frac, rng, CLASSES, dc, kept) : sinkShips(f, frac, rng, ['ss'], dc, kept);
  if (kept && !p.toDice) for (const c of SURFACE) kept[c] += p.kept?.[f.id]?.[c] ?? 0;
  const there = p.sunk[f.id] ?? {};
  for (const c of SURFACE) {
    const more = Math.min(f.ships[c], Math.max(0, (there[c] ?? 0) - (p.toDice ? out[c] : 0)));
    f.ships[c] -= more;
    f.lost[c] += more;
    out[c] += more;
    loseStars(f, c, more);
  }
  mendStars(f);
  return out;
}

/**
 * (`spared`, ruling 240: the battleships and carriers of this fleet that damage control kept afloat. Where they are
 * all that stood between its admiral and the dice for a lost flagship, the dice are thrown all the same and nothing
 * comes of them, so that every fleet after this one in the battle has the draws it would have had.)
 */
function afterAction(g: Game, f: Fleet, side: SeaSide, won: boolean, rng: Rng, spared = 0): void {
  f.fought = g.turn;
  f.ready = Math.max(0, f.ready - (won ? NAVY.ready.won : NAVY.ready.lost));
  const a = f.admiral;
  if (!a) return;
  const flagship = side.sunk.bb + side.sunk.cv > 0;
  if (!won && (flagship || spared > 0) && rng.chance(NAVY.admiral.lostWithFlagship)) {
    const pool = COMMAND.names[f.nat];
    if (flagship) {
      headline(g, 'event', `Admiral ${a.name} is lost with his flagship.`, f.nat, undefined, { arm: 'sea', lost: f.nat });
      f.admiral = pool ? { name: `${rng.pick(pool.first)} ${rng.pick(pool.last)}`, level: 1, xp: 0 } : null;
      return;
    }
    if (pool) { rng.pick(pool.first); rng.pick(pool.last); }
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
    // (A shadow seeks no battle: release 2, shadow.ts. No fleet carries one with that switch off.)
    // (Release 2, seekOnce) Nor does a fleet that has sought battle once this month: by order, or in an earlier meeting.
    const mine = g.fleets.filter((f) => f.zone === z && f.stance === 'patrol' && f.shadow === undefined && !hasSought(g, f) && surfaceShips(f) > 0 && (f.nat === nat || (allied(g, f.nat, nat) && foes.some((e) => atWar(g, f.nat, e.nat)))));
    if (!mine.some((f) => f.nat === nat)) continue;
    const rng = seaRng(g, `battle:${z}:${nat}`);
    // A patrolling enemy stands and fights. Raiding surface ships have to be found first; submarines are the escorts' business.
    const met = foes.filter((f) => surfaceShips(f) > 0 && (f.stance === 'patrol' || rng.chance(NAVY.battle.catchRaider)));
    if (!met.length) continue;
    const airA = airOver(g, z, nat) * NAVY.air.weight, airD = airOver(g, z, nat, true) * NAVY.air.weight;
    const A = sum(mine) + airA, D = Math.max(1, sum(met) + airD);
    if (A < NAVY.battle.seek * D) continue;
    const names = (fs: Fleet[]): string => { const l = fs.map(fleetName); return l.length === 1 ? l[0] : `${l.slice(0, -1).join(', ')} and ${l[l.length - 1]}`; };
    // (Release 2, shadow.ts) Shadows found alone are faster than what they follow: they keep their distance, most times.
    if (allShadows(g, met) && rng.chance(SHADOW.slip)) {
      headline(g, 'event', `${cap(names(met))} ${met.length === 1 ? 'keeps its' : 'keep their'} distance from ${names(mine)} ${seaWhere(zone.name)}, too fast to be brought to battle.`, met[0].nat, undefined, { arm: 'sea' });
      continue;
    }
    if (A >= NAVY.battle.evadeAt * D && rng.chance(NAVY.battle.evade)) {
      for (const f of met) sail(g, f, f.home, 'port');
      headline(g, 'event', `${cap(names(met))} ${met.length === 1 ? 'declines' : 'decline'} battle ${seaWhere(zone.name)} and ${met.length === 1 ? 'makes' : 'make'} for port.`, met[0].nat, undefined, { arm: 'sea', won: nat, lost: met[0].nat });
      continue;
    }
    out.push(fleetAction(g, zone, nat, mine, met, rng, airA, airD));
  }
  return out;
}

/**
 * One fleet action, fought: `mine` (the side of `nat`) against `met` in `zone`, settled on the field where the player
 * fights it out (seaField) or else by the dice, as every fleet action is. The month's actions (seaBattles) and an attack
 * the player orders at once (release 2, fleetattack.ts) both fight here, on the same figures and the same draws of `rng`.
 * `at` (the attack on a fleet in its harbour, release 2): the harbour, the weight its batteries add to the defence
 * (`shore`), and the share of their weight the ships at anchor fight with (`anchor`). Left out, nothing changes.
 */
export function fleetAction(g: Game, zone: Game['seas'][number], nat: string, mine: Fleet[], met: Fleet[], rng: Rng, airA: number, airD: number, at?: { port: string; shore: number; anchor: number }): SeaReport {
  const z = zone.id;
  const sum = (fs: Fleet[]): number => fs.reduce((s, f) => s + fleetPower(g, f), 0);
  const names = (fs: Fleet[]): string => { const l = fs.map(fleetName); return l.length === 1 ? l[0] : `${l.slice(0, -1).join(', ')} and ${l[l.length - 1]}`; };
  const where = at ? `at ${at.port}` : seaWhere(zone.name);
  const A = sum(mine) + airA, D = Math.max(1, (at ? sum(met) * at.anchor + at.shore : sum(met)) + airD);
  const att = mine.map(sideOf), def = met.map(sideOf);
  // (Release 2, seekOnce) The seeking side has sought its battle for the month; the defenders are not marked.
  markSought(g, mine);
  // (src/battle/sea) An action the player fought out on the field hands back the ratio it came to and the ships it cost.
  const played = seaField.fought?.({ turn: g.turn, zone: z, nat, mine: mine.map((f) => f.id), met: met.map((f) => f.id), A, D, airA, airD });
  const R = played ? played.R : (A / D) * rng.range(0.8, 1.2);
  const won = R >= 1;
  const edge = won ? R : 1 / R;
  const hard = clamp(0.14 + 0.08 * edge, 0.18, 0.5), light = clamp(0.16 / edge, 0.03, 0.14);
  // Ruling 240 (C3): each fleet's damage control eases its own losses, its surface ships only, on the same draws.
  // Who wins, the odds, the cost in readiness and who sails home are not touched. `keptA` and `keptD` are the
  // ships it kept afloat, fleet by fleet. (An action fought out on the field sinks what the field sank, and keeps afloat
  // what its damage control kept there; handed back to the dice, the dice sink with damage control, as here.)
  const keptA = mine.map(() => none()), keptD = met.map(() => none());
  mine.forEach((f, i) => { att[i].sunk = played ? sinkFought(f, played, won ? light : hard, rng, dcOf(g, f.nat), keptA[i]) : sinkShips(f, won ? light : hard, rng, CLASSES, dcOf(g, f.nat), keptA[i]); });
  met.forEach((f, i) => { def[i].sunk = played ? sinkFought(f, played, won ? hard : light, rng, dcOf(g, f.nat), keptD[i]) : sinkShips(f, won ? hard : light, rng, CLASSES, dcOf(g, f.nat), keptD[i]); });
  if (afloatWatch.tell) { mine.forEach((f, i) => afloatWatch.tell!({ route: 'action', nat: f.nat, sunk: att[i].sunk, saved: keptA[i] })); met.forEach((f, i) => afloatWatch.tell!({ route: 'action', nat: f.nat, sunk: def[i].sunk, saved: keptD[i] })); }
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
  mine.forEach((f, i) => afterAction(g, f, att[i], won, rng, keptA[i].bb + keptA[i].cv));
  met.forEach((f, i) => afterAction(g, f, def[i], !won, rng, keptD[i].bb + keptD[i].cv));
  // Fleets that have fought are in contact: each side has the other identified this month (the grand campaign's search at sea).
  if (carrierRules(g)) {
    for (const f of mine) for (const e of met) (f.seen ??= {})[e.nat] = g.turn;
    for (const f of met) for (const e of mine) (f.seen ??= {})[e.nat] = g.turn;
  }
  // The beaten side breaks off and makes for harbour.
  for (const f of won ? met : mine) sail(g, f, f.home, 'port');
  const level: VictoryLevel = won ? (edge >= 2 ? 'Decisive victory' : 'Close victory') : edge >= 2 ? 'Decisive defeat' : 'Close defeat';
  g.flags.seaBattles = (g.flags.seaBattles ?? 0) + 1;
  // The battleships and carriers that went down, by name, for the card that marks them (a record only).
  att.forEach((s, i) => nameSide(g, s, `sea:${g.flags.seaBattles}:a${i}`));
  def.forEach((s, i) => nameSide(g, s, `sea:${g.flags.seaBattles}:d${i}`));
  const rep: SeaReport = {
    id: g.flags.seaBattles, turn: g.turn, zone: z, zoneName: zone.name, attNat: nat, defNat: met[0].nat,
    att, def, lines, aPower: A, dPower: D, odds: R, level, won,
  };
  if (at) {
    rep.port = at.port;
    const n = airA > 0.5 || airD > 0.5 ? 2 : 1;
    lines.splice(n, 0, { label: 'In harbour', value: `The ${names(met)} ${met.length === 1 ? 'was' : 'were'} caught at anchor at ${at.port}`, effect: `Ships at anchor fight at ${Math.round(at.anchor * 100)}% of their weight` });
    if (at.shore > 0) lines.splice(n + 1, 0, { label: 'Shore batteries', value: `The guns of ${at.port} fire on the attackers`, effect: `Worth ${Math.round(at.shore)} to the defence` });
  }
  g.seaReports.push(rep);
  const win = won ? mine : met, lose = won ? met : mine;
  const ws = total(won ? att : def), ls = total(won ? def : att);
  storySea(g, z, win, lose, ls.bb + ls.cv, shipCount(ls), shipCount(ws));
  // Battle stars (b477): every ship of the winning fleets still afloat earns this action, on the dice or on the field.
  // A record only: it draws no dice and changes nothing that is fought.
  bookSeaAction(g, z, win);
  // (Ruling 240: the ships damage control kept afloat, a line for each fleet that has any.)
  const afloat = [...mine.map((f, i) => keptAfloat(f, keptA[i])), ...met.map((f, i) => keptAfloat(f, keptD[i]))].filter(Boolean);
  headline(g, 'battle', `${cap(names(win))} ${win.length === 1 ? 'defeats' : 'defeat'} ${names(lose)} ${where}: ${shipList(ls)} sunk${shipCount(ws) ? ` for the loss of ${shipList(ws)}` : ''}.${afloat.length ? ` ${afloat.join(' ')}` : ''}`, win[0].nat, undefined, { arm: 'sea', won: win[0].nat, lost: lose[0].nat });
  // (Release 2, voyage.ts) Each fleet's log has the action, from its own side: a record only, nothing with that switch off.
  const sankA = total(def), sankD = total(att);
  mine.forEach((f, i) => noteFight(g, f, `${won ? 'Won' : 'Lost'} a fleet action ${where} against ${names(met)}: sank ${shipList(sankA)}, lost ${shipList(att[i].sunk)}.`));
  met.forEach((f, i) => noteFight(g, f, `${won ? 'Lost' : 'Won'} a fleet action ${where} against ${names(mine)}: sank ${shipList(sankD)}, lost ${shipList(def[i].sunk)}.`));
  return rep;
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
function crossings(g: Game, nat: string, of = nat): { off: Set<number>; way: Set<number> } {
  const off = new Set<number>(), way = new Set<number>();
  for (const sfx of ['', ':o']) {
    const aim = g.regions[(g.flags[`st:aim:${of}${sfx}`] ?? 0) - 1], quay = g.regions[(g.flags[`st:quay:${of}${sfx}`] ?? 0) - 1];
    if (!aim || !quay || !atWar(g, of, aim.ctrl) || !atWar(g, nat, aim.ctrl)) continue;
    const path = seaPath(g, of, quay, aim, { max: NAVY.landing.range, surprise: true });
    if (!path) continue;
    path.forEach((z, i) => (i === path.length - 1 ? off : way).add(z));
  }
  // (And the Pacific's hops, each a crossing of its own: pacific.ts. None while the far side is off.)
  for (const { from, to } of hopAims(g, nat)) {
    const path = seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: true });
    if (path) path.forEach((z, i) => (i === path.length - 1 ? off : way).add(z));
  }
  return { off, way };
}

/**
 * The cover of the Pacific's hops (ruling 200; the shores a staff has named are pacific.ts's, `hopAims`). A sea on
 * the way to a named shore that is closed to the nation's troopships is opened by its patrol fleets together: of
 * those fit for sea, with no order from the plan and the sea in reach, the strongest go there until the enemy is
 * no longer much the stronger (navy.json 'closed'), and none goes where all of them together would not do it. The
 * seas are taken in the order the shores were named, the sea off the beach last on each way. Returns the fleets
 * sent: the admirals' own rule leaves them where they were put. Nothing while the far side is off (no shore is named).
 */
function coverHops(g: Game, nat: string, orders?: { fleet: string }[], cover?: { covers: Set<number>; moved: Set<number> }): Set<number> {
  const sent = new Set<number>();
  const aims = hopAims(g, nat);
  if (!aims.length) return sent;
  const seas: number[] = [];
  for (const { from, to } of aims) for (const z of seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: true }) ?? []) if (!seas.includes(z)) seas.push(z);
  // (A fleet that has gone to the beach it covers counts as a patrol fleet; one that moved its base this month lies in harbour: `coverBases`.)
  const patrols = (f: Fleet): boolean => (f.duty === 'patrol' || !!cover?.covers.has(f.id)) && !cover?.moved.has(f.id);
  // (Nor one on its month of peace at sea: `navalAI`.)
  // (Nor one in passage to a new base, nor one in from it this month: release 2, the far chart.)
  const free = fleetsOf(g, nat).filter((f) => patrols(f) && !inPassage(f) && !cameIn(g, f) && !f.tied && f.cruised !== g.turn && shipCount(f.ships) > 0 && !orders?.some((o) => o.fleet === f.name) && f.ready >= 0.5 && (atSea(f) || f.ready >= NAVY.ready.sortieAt));
  for (const z of seas) {
    if (!closedTo(g, z, nat)) continue;
    const need = seaSide(g, z, nat, true).total / NAVY.closed.ratio;
    let own = seaSide(g, z, nat, false).total;
    const there = (f: Fleet): boolean => f.zone === z && atSea(f);
    const can = free.filter((f) => !sent.has(f.id) && !there(f) && fleetReach(g, f).includes(z)).sort((a, b) => fleetPower(g, b) - fleetPower(g, a) || a.id - b.id);
    if (own + can.reduce((t, f) => t + fleetPower(g, f), 0) <= need) continue;
    for (const f of can) {
      if (own > need) break;
      const w = fleetPower(g, f);
      if (sail(g, f, z, 'patrol')) { own += w; sent.add(f.id); }
    }
    // (The fleets already there stay for the crossing.)
    for (const f of free) if (there(f)) sent.add(f.id);
  }
  return sent;
}

/**
 * The fleet goes to the beach it covers (ruling 200(j); the Pacific planner's paper 14, section 4, "Which fleet goes
 * where"; rider b585 to ruling 234, item 1(b)). When the staff has named a shore of the Pacific's for a hop, the
 * fleets of its own that could cover it and have the sea off the beach in their reach cover the crossing as patrol
 * fleets do, whatever their own duty. Where the sea lies beyond the reach of every one of them, the strongest moves its
 * base to the harbour of its side nearest the beach that can base a fleet within three seas of it (its reach from
 * there takes in the sea off the beach); it spends that month in harbour there, and from the next it covers the
 * crossing. Between landings, with no named beach of its own left in its reach, it goes back to the base it had (the
 * fleet table's, by date: ruling 189). While a named beach is still in its reach it stays, a worn one resting in
 * harbour there; an order of the plan or the book that month is carried out first, and a dated move of the book that
 * takes it elsewhere meanwhile stands. One the enemy drives from its cover base covers from the harbour it is driven
 * to, and still goes home when its beaches are done. A fleet that may cover: warships on the surface, fit for sea,
 * free (no order of the plan or the book, not tied to a station, not a raider) and based on the Pacific's ground; a
 * beach, a shore on the Pacific's ground (the plan's landings elsewhere are covered by the admirals there). Every
 * computer staff's in the Pacific's war, the power's included (rider b599 (iii) to ruling 234), never a player's, and
 * nothing while the far side is off (pacific.ts `coversBeaches`); the power's battle line, and once its ring is drawn
 * its carrier striking force, keep to their own rule and are never among the fleets that may cover (`keptTogether`). Returns the fleets covering
 * a beach, and those that moved their base this month.
 */
function coverBases(g: Game, nat: string, orders?: { fleet: string }[]): { covers: Set<number>; moved: Set<number> } {
  const covers = new Set<number>(), moved = new Set<number>();
  const mine = fleetsOf(g, nat);
  const was = (f: Fleet): number | undefined => g.flags[`fl:cover:${f.id}`];
  const forget = (f: Fleet): void => { delete g.flags[`fl:cover:${f.id}`]; delete g.flags[`fl:coverAt:${f.id}`]; };
  const ordered = (f: Fleet): boolean => !!orders?.some((o) => o.fleet === f.name);
  // A covering fleet the book moved meanwhile stands where the book put it: that is the fleet table's base now. One
  // driven from its cover base (a harbour lost) covers from where it lies, and still goes home when its beaches are done.
  for (const f of mine) {
    const at = g.flags[`fl:coverAt:${f.id}`];
    if (at === undefined || f.base === at - 1) continue;
    // (A row of the book with a sea takes it out from its cover base and brings it back there after: the cover stands.)
    if (f.bookBase) continue;
    if (bookMoved(g, nat, f)) forget(f);
    else g.flags[`fl:coverAt:${f.id}`] = f.base + 1;
  }
  // Back to the base it had (release 2, the far chart: the design director's ruling of 04:33Z, 25 November 2026, 1(a)):
  // a fleet still in passage from it to its cover base turns back from where it is; one in passage from elsewhere comes
  // in first, and is read the month after. True where it moved.
  const goBack = (f: Fleet, r: Region | undefined): boolean => {
    if (!inPassage(f)) { forget(f); return !!r && r.id !== f.base && rebase(g, f, r); }
    if (f.passage?.from !== r?.id) return false;
    forget(f);
    return turnBack(g, f);
  };
  if (!coversBeaches(g, nat)) {
    for (const f of mine) if (was(f) !== undefined && !ordered(f) && goBack(f, g.regions[was(f)! - 1])) moved.add(f.id);
    return { covers, moved };
  }
  // (A fleet on its month of peace at sea is left alone that month: `navalAI`. The power's main fleet keeps to its own rule.)
  const kept = keptTogether(g, nat);
  const fit = (f: Fleet): boolean => surfaceShips(f) > 0 && f.duty !== 'raid' && !inPassage(f) && !f.tied && f.cruised !== g.turn && !ordered(f) && !kept.has(f.id) && f.ready >= 0.5 && f.base >= 0 && pacificGround(g, g.regions[f.base]);
  // The sea off each named beach of the Pacific's, in the order the beaches were named, with the first beach named on it.
  const beaches: { z: number; to: Region }[] = [];
  for (const { from, to } of hopAims(g, nat)) {
    if (!pacificGround(g, to)) continue;
    const path = seaPath(g, nat, from, to, { max: NAVY.landing.range, surprise: true });
    const z = path?.length ? path[path.length - 1] : -1;
    if (z >= 0 && !beaches.some((b) => b.z === z)) beaches.push({ z, to });
  }
  // A fleet on its way to its cover base covers what it will reach from there: no second fleet is sent after it.
  const onWay = (f: Fleet): boolean => inPassage(f) && g.flags[`fl:coverAt:${f.id}`] === f.base + 1 && !ordered(f) && !kept.has(f.id);
  for (const { z, to } of beaches) {
    const can = mine.filter(fit);
    const reaching = mine.filter((f) => (fit(f) || onWay(f)) && fleetReach(g, f).includes(z));
    if (reaching.length) { for (const f of reaching) covers.add(f.id); continue; }
    for (const f of can.filter((x) => !covers.has(x.id)).sort((a, b) => fleetPower(g, b) - fleetPower(g, a) || a.id - b.id)) {
      const r = coverBase(g, f, z);
      if (!r) continue;
      const from = f.base;
      if (!rebase(g, f, r)) continue;
      if (was(f) === undefined) g.flags[`fl:cover:${f.id}`] = from + 1;
      g.flags[`fl:coverAt:${f.id}`] = r.id + 1;
      covers.add(f.id);
      moved.add(f.id);
      headline(g, 'info', `The ${f.name} moves its base to ${r.name} to cover the landing at ${to.name}.`, nat, r.id);
      break;
    }
  }
  // Between landings: a fleet with no named beach left in its reach goes back to the base it had. So does the power's
  // carrier striking force the month its ring is drawn, whatever beach is still in its reach: it is kept together now.
  for (const f of mine) {
    const at = was(f);
    if (at === undefined || covers.has(f.id) || ordered(f)) continue;
    const reach = fleetReach(g, f);
    if (!kept.has(f.id) && beaches.some((b) => reach.includes(b.z))) continue;
    const r = g.regions[at - 1], going = inPassage(f);
    if (!goBack(f, r)) continue;
    moved.add(f.id);
    // (Turned back on its way, the turn is its news.)
    if (!going) headline(g, 'info', `The ${f.name} returns to ${r.name}.`, nat, r.id);
  }
  return { covers, moved };
}

/**
 * The power's main fleet, which never goes to a beach to cover it (rider b599 (iii) to ruling 234; the Pacific
 * planner's paper 14). Its battle line, the fleet with the most battleships, always: in the opening only the carriers
 * and the lighter fleets cover the landings (section 2, and the record: the main body lay in home waters through the
 * opening; the design director, 16:22Z). Once the ring is drawn its carrier striking force too, of its other fleets
 * the one with the most carriers (section 3, "The fleet is kept together and comes out for the line"): they share the
 * line's base and sail together, by the book's dated moves and the admirals' own odds (pacific.ts, `ringDrawn`).
 * Empty for every other nation, and while the far side is off.
 */
function keptTogether(g: Game, nat: string): Set<number> {
  const out = new Set<number>();
  if (!PACIFIC || nat !== PACIFIC.power) return out;
  const mine = fleetsOf(g, nat).filter((f) => shipCount(f.ships) > 0);
  const top = (k: 'bb' | 'cv', not?: Fleet): Fleet | undefined => mine
    .filter((f) => f.ships[k] > 0 && f !== not)
    .sort((a, b) => b.ships[k] - a.ships[k] || fleetPower(g, b) - fleetPower(g, a) || a.id - b.id)[0];
  const line = top('bb');
  if (line) out.add(line.id);
  if (!ringDrawn(g)) return out;
  const carriers = top('cv', line);
  if (carriers) out.add(carriers.id);
  return out;
}

/**
 * Whether the book moved the fleet's base this month for good (stations.ts, `bookFleets`): a row that is a move of
 * base (marked done this month). A row with a sea is not: it takes the fleet out from where it lay and brings it back
 * there after its last month (its `bookBase`), so a fleet covering a beach keeps its cover and its own base to go home
 * to (`coverBases`).
 */
function bookMoved(g: Game, nat: string, f: Fleet): boolean {
  const key = `bf:${nat}:${f.name}:`;
  for (const k in g.flags) if (k.startsWith(key) && g.flags[k] === g.turn) return true;
  return false;
}

/**
 * The harbour of the fleet's side nearest a beach's sea that can base it within three seas of it (the fleet's reach
 * from there takes in the sea), nearest by sea and then on the map; null if there is none. A harbour the enemy stands
 * in or besieges is no base.
 */
function coverBase(g: Game, f: Fleet, z: number): Region | null {
  // Seas counted from the beach's own as one, through the waters the fleet may pass, out to the fleet's reach.
  const d = new Int16Array(g.seas.length).fill(-1);
  const q = [z];
  d[z] = 1;
  for (let i = 0; i < q.length; i++) {
    if (d[q[i]] >= NAVY.range) continue;
    for (const j of g.seas[q[i]].adj) if (d[j] < 0 && canPass(g, j, q[i], f.nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  }
  const beach = g.seas[z];
  let best: Region | null = null, bd = Infinity, bm = Infinity;
  for (const r of g.regions) {
    if (!r.port || !r.seas.length || r.id === f.base || !allied(g, f.nat, r.ctrl) || hostileUnits(g, r, f.nat).length || besieged(g, r)) continue;
    const near = Math.min(...r.seas.map((s) => (d[s] < 0 ? Infinity : d[s])));
    if (near === Infinity || near > bd) continue;
    const m = seaDist(g, r, beach);
    if (near === bd && m >= bm) continue;
    if (!canSailTo(g, f, r)) continue;
    best = r; bd = near; bm = m;
  }
  return best;
}

/**
 * Ruling 233(v), the grand campaign: the same seas for the second fronts of this nation's allies in the same war
 * (fleets.json 'ai.allies'). A computer admiral whose own staff has named no crossing that month covers an ally's
 * named crossing under the same tests: the fleet that covered the landings in Sicily and at Salerno was two navies'
 * under one command. (Before it an American fleet patrolled within reach of a sea shut to a British landing force
 * and had no orders for it.)
 */
function alliesCrossings(g: Game, nat: string): { off: Set<number>; way: Set<number> } {
  const off = new Set<number>(), way = new Set<number>();
  if (!FLEETS.ai.allies || g.flags.open !== 1) return { off, way };
  for (const id of Object.keys(g.nations)) {
    const A = g.nations[id];
    if (id === nat || !A.alive || !A.atWar.length || !allied(g, nat, id)) continue;
    const c = crossings(g, nat, id);
    for (const z of c.off) off.add(z);
    for (const z of c.way) way.add(z);
  }
  for (const z of off) way.delete(z);
  return { off, way };
}

/**
 * Ruling 233(k), the grand campaign: a computer's navy whose government has changed and is treating for peace keeps
 * its harbours (fleets.json 'harbour': the country and the event). The Italian fleet did not come out after 25 July
 * 1943. Its surface fleets lie in port and come out against no landing; its submarines keep the sea. A player's
 * fleets sail as he likes.
 */
export function keepsHarbour(g: Game, nat: string): boolean {
  const ev = FLEETS.harbour?.[nat];
  return !!ev && g.flags.open === 1 && nat !== g.human && !!g.flags[ev];
}

/**
 * True for a fleet of the player's that he has given an order himself: his admirals, when they run the navy for him,
 * leave it where he sent it (1.5.1; Brian, 24 November 2026 18:32Z). They neither move it nor take it out of harbour.
 */
export const keptByHand = (g: Game, f: Fleet): boolean => f.ordered === true && f.nat === g.human;

// ---- One battle sought a month (release 2; the design director's ruling of 25 November 2026, 02:38Z) ----------------
// seaBattles runs at the close of every nation's month and takes onto the seeking side every allied fleet in the sea at
// war with a foe there, so one fleet could seek battle in its allies' meetings and in its own; and a fleet sent at the
// enemy by order could seek again at its month's end where the order missed or struck a harbour. With the switch on, a
// fleet seeks battle once a month: every fleet on the seeking side of an action is marked (Fleet.sought), left out of
// every later meeting that month and refused a second order. Defenders are never marked. Off: nothing is written.

/** True where each fleet seeks battle at most once a month (the switch 'seekOnce'). */
export const seekOnceOn = (g: Game): boolean => g.flags.open === 1 && g.flags.seekOnce === 1;
/** Switches the one battle sought a month on or off in a campaign under way. */
export function setSeekOnce(g: Game, on: boolean): void {
  if (on) g.flags.seekOnce = 1;
  else delete g.flags.seekOnce;
}
/** True where the fleet has sought battle this month and may seek no more (the switch on). */
export const hasSought = (g: Game, f: Fleet): boolean => seekOnceOn(g) && f.sought === g.turn;
/** Marks fleets as having sought battle this month, where the switch is on. */
export function markSought(g: Game, fs: Fleet[]): void {
  if (seekOnceOn(g)) for (const f of fs) f.sought = g.turn;
}

/** Orders for one computer-run nation's fleets. Named orders from the plan come first. */
export function navalAI(g: Game, nat: string, orders?: { fleet: string; zone: string; stance: Stance }[]): void {
  // The player's fleets are his own to order, unless he has handed the navy to his admirals (depth.ts).
  if (byHand(g, nat, 'navy')) return;
  const n = g.nations[nat];
  let cross: { off: Set<number>; way: Set<number> } | null = null;
  // (The fleet goes to the beach it covers, and the cover of the hops: the Pacific's, none while the far side is off.)
  const bases = coverBases(g, nat, orders);
  const covering = n.atWar.length ? coverHops(g, nat, orders, bases) : null;
  const duty = (f: Fleet): Stance => (bases.covers.has(f.id) ? 'patrol' : f.duty);
  let theirs: { off: Set<number>; way: Set<number> } | null = null;
  // (Release 2, fleetwhy.ts) The admirals' reason for each of the player's fleets, in a sentence, where that is switched on.
  const says = nat === g.human && fleetWhyOn(g);
  for (const f of fleetsOf(g, nat)) {
    // A fleet the player has ordered himself stays where he sent it (1.5.1).
    if (keptByHand(g, f)) continue;
    // A computer's shadow keeps to its quarry (release 2, shadow.ts aiShadows: it brings the worn home itself).
    if (f.shadow !== undefined) continue;
    const say = (why: string): void => { if (says) f.why = [g.turn, why]; };
    // A fleet in passage keeps on its way to its new base (release 2, the far chart: steamPassages brings it in).
    if (inPassage(f)) { say(`In passage to its new base at ${f.port}, it keeps on its way.`); continue; }
    const where = (z: number): string => seaWhere(g.seas[z]?.name ?? 'sea');
    const what = (z: number): string => seaWhat(g.seas[z]?.name ?? 'sea');
    // (Release 2, fleetwhy.ts) Whether his own side has found an enemy surface fleet, or submarines, at sea there.
    const found = (z: number, subs: boolean): boolean => g.fleets.some((x) => x.zone === z && atSea(x) && atWar(g, x.nat, nat) && (subs ? x.ships.ss > 0 : surfaceShips(x) > 0) && fleetKnown(g, x, nat) !== 'none');
    const sighted = (z: number): boolean => found(z, false), subsFound = (z: number): boolean => found(z, true);
    const home = (why?: string): void => { sail(g, f, f.home, 'port'); if (why) say(why); };
    // (A fleet on its month of peace at sea is not called home by its admiral: the month is the upkeep's, by the record
    // or by the chance, and Japan's blow must find it out, ruling 234(d). Nor is it given any other order that month,
    // where its country is at war with somebody else: rider b593. No fleet has such a month with the far side off.)
    if (f.cruised === g.turn) continue;
    if (!n.atWar.length || !shipCount(f.ships) || (f.duty !== 'raid' && keepsHarbour(g, nat))) { home(n.atWar.length ? undefined : 'The country is at peace, so it lies in harbour.'); continue; }
    const o = orders?.find((x) => x.fleet === f.name);
    if (o) { if (!sail(g, f, zoneOf(g, o.zone), o.stance)) home(); continue; }
    if (covering?.has(f.id) || bases.moved.has(f.id)) continue;
    // (Release 2, the far chart: a fleet struck at anchor this month or last withdraws out of the strikers' reach,
    // where staying costs more than the move: withdraws. Read before the worn rest, which kept it in the struck harbour.)
    if (withdraws(g, f)) { say(`Struck at anchor, it withdraws to ${f.port}, out of the strikers' reach.`); continue; }
    // (In from its passage this month, it lies in harbour, as a fleet moved this month does: cameIn.)
    if (cameIn(g, f)) { say(`In from its passage this month, it lies in harbour at ${f.port}.`); continue; }
    // A worn fleet stays in harbour until it is fit for sea again.
    if (f.ready < 0.5 || (!atSea(f) && f.ready < NAVY.ready.sortieAt)) {
      home(`Worn down to ${Math.round(f.ready * 100)}% readiness, it refits in harbour and sails again at ${Math.round(NAVY.ready.sortieAt * 100)}%.`);
      continue;
    }
    if (f.duty === 'raid') {
      // (Release 2, the sea patrol, once live (Q5 (a)): the staff moves its boats to the sea of their routes, within
      // reach, where they spend most of the month up on station, out of waters aircraft without kit keep them down in
      // as its side's boats last met them there (the design director's ruling of 24 November 2026 17:54Z), and only
      // where that gain covers the time the move itself loses (his ruling of 17:49Z, 1 (a)): quieterBeat.)
      const quiet = quieterBeat(g, f);
      if (quiet >= 0) {
        headline(g, 'info', `${cap(fleetName(f))} shifts its hunting ground from ${seaWhat(g.seas[f.station].name)} to ${seaWhat(g.seas[quiet].name)}, out of the aircraft's way.`, nat);
        f.station = quiet;
      }
      // (Units decide, the one-pass ruling: the staff bases a raider at the held port nearest its beat, where its boats
      // spend more time on station. The move takes the month in harbour, as any move of base does.)
      const to = nearerPort(g, f);
      if (to) {
        const from = f.port;
        if (rebase(g, f, to)) { headline(g, 'info', `${cap(fleetName(f))} moves its base from ${from} to ${to.name}, nearer its hunting ground ${seaWhere(g.seas[f.station].name)}.`, nat, to.id); say(`It shifts its base to ${to.name}, nearer its hunting ground ${where(f.station)}.`); continue; }
      }
      if (sail(g, f, f.station, 'raid')) say(`A raider, it hunts the enemy's merchant ships ${where(f.station)}.`);
      else home(`Its hunting ground ${where(f.station)} is out of its reach, so it waits in harbour.`);
      continue;
    }
    // The grand campaign: a fleet tied to a station on one of its side's shipping lanes sails with the convoys.
    const lane = seaWar(g) ? laneOnBeat(g, f) : null;
    const tied = tiedNow(g, f);
    if (lane && tied && escort(g, f, lane.id)) { say(`Tied to ${lane.name}, it sails with the convoys.`); continue; }
    const mine = fleetPower(g, f);
    const reach = fleetReach(g, f);
    cross ??= crossings(g, nat);
    // (His own staff's crossing first: an ally's is covered in a month when his own staff has named none.)
    theirs ??= cross.off.size || cross.way.size ? { off: new Set<number>(), way: new Set<number>() } : alliesCrossings(g, nat);
    // A fleet hunts on its own beat and the seas next to it. It goes further only to meet a landing or an invasion.
    const beat = new Set([f.station, ...g.seas[f.station].adj]);
    let best = -1, bv = 0;
    // (Release 2, fleetwhy.ts) What weighed most in the sea chosen, and whether enemy bombers kept it out of any sea.
    let bestWhy = '', shy = 0;
    for (const z of tied ? [f.station] : reach) {
      const e = seaSide(g, z, nat, true);
      const own = seaSide(g, z, nat, false).total + (f.zone === z && atSea(f) ? 0 : mine);
      // Keep out of water the enemy's bombers command, unless the homeland is being invaded across it and there is a fighting chance.
      const invaded = invadedHere(g, z, nat) && own >= 0.5 * e.total;
      if (airOver(g, z, nat, true) - airOver(g, z, nat, false) > NAVY.air.avoid && !invaded) { shy++; continue; }
      // A fleet that aircraft have just struck in a sea keeps out of it for a while (the grand campaign).
      if (f.struck && f.struck[2] === z && g.turn - f.struck[0] <= CARRIERS.ai.shy && !invaded && carrierRules(g)) continue;
      // Enemy surface ships that could be brought to battle here. Submarines are no quarry for a battle fleet.
      let quarry = 0;
      for (const x of g.fleets) if (x.zone === z && atSea(x) && surfaceShips(x) > 0 && atWar(g, x.nat, nat)) quarry += fleetPower(g, x);
      let v = 0;
      // (each reason's weight, and its words for the admirals' note: the heaviest is the one given)
      let top = 0, why = '';
      const sea = says ? what(z) : '', at = says ? where(z) : '';
      const add = (w: number, words: () => string): void => { v += w; if (says && w > top) { top = w; why = words(); } };
      // (The admirals' words use only what their own side has found, the design director's 19:14Z: an enemy fleet nobody
      // has sighted there is never named, and the note says only where it patrols.)
      if (duty(f) === 'patrol' && beat.has(z) && quarry > 0 && own >= NAVY.battle.seek * e.total) add(quarry, () => (sighted(z) ? `It patrols ${sea} to bring the enemy fleet sighted there to battle: it is judged strong enough to win.` : `It patrols ${sea}.`)); // an enemy fleet that can be beaten
      if (landedLately(g, z, nat) && own >= e.total) add(30, () => `It patrols ${sea} to cut the sea lane behind the enemy's landing.`); // cut the sea lane behind an enemy landing
      // Its own staff's second front: stand off the beach the army means to land on, and on the way to it.
      if (duty(f) === 'patrol' && own >= e.total && (cross.off.has(z) || cross.way.has(z))) add(cross.off.has(z) ? 40 : 25, cross.off.has(z) ? () => `It stands ${at}, off the beach your army means to land on.` : () => `It patrols ${sea}, which your army's landing must cross.`);
      // An ally's second front, in a month when its own staff has named none (ruling 233(v)).
      if (duty(f) === 'patrol' && own >= e.total && (theirs.off.has(z) || theirs.way.has(z))) add(theirs.off.has(z) ? 40 : 25, () => `It patrols ${sea} to cover an ally's landing.`);
      if (invaded) add(60, () => `It patrols ${sea}: the enemy is crossing it to invade the homeland.`);
      // A port the army stands round and cannot starve while ships reach it: close the sea off it, if the fleet can live there.
      if (duty(f) === 'patrol' && own >= NAVY.battle.seek * e.total && blockadeWanted(g, z, nat)) add(SIEGE.ai.sea.blockade, () => `It patrols ${sea} to blockade the port your army is besieging.`);
      if (z === f.station && duty(f) === 'patrol' && (quarry <= 0 || own >= 0.7 * e.total)) add(5, () => `It patrols ${sea}, its usual beat: no other sea in its reach needs it more.`); // the usual beat
      if (v > bv) { bv = v; best = z; bestWhy = why; }
    }
    // With no enemy fleet to fight, a fleet whose beat is on a lane the raiders are bleeding guards the cargoes instead.
    if (lane && duty(f) === 'patrol' && bv <= 5 && laneLoss(g, lane.to, lane.zones, lane.id).raid > 0 && escort(g, f, lane.id)) { say(`It guards ${lane.name}: raiders are sinking cargoes on its beat, and there is no enemy fleet to fight.`); continue; }
    // Ruling 52 (the grand campaign): a fleet with nothing better than its usual beat is not left idle on it. It
    // sails with a lane of its own side's that the raiders are bleeding and nobody guards, anywhere in its reach; and
    // failing that its destroyers hunt the submarines at work on its beat.
    if (seaWar(g) && f.duty === 'patrol' && !f.tied && bv <= 5) {
      const clear = (z: number): boolean => !(airOver(g, z, nat, true) - airOver(g, z, nat, false) > NAVY.air.avoid) && !(f.struck && f.struck[2] === z && g.turn - f.struck[0] <= CARRIERS.ai.shy && carrierRules(g));
      const bare = FLEETS.ai.guard ? laneUnguarded(g, f, reach.filter(clear)) : null;
      if (bare && escort(g, f, bare.id)) { say(`It guards ${bare.name}: raiders are sinking its cargoes, and no other ship of yours guards it.`); continue; }
      // (The far side: it hunts wherever the enemy's boats are out within its reach, not on its own beat alone.)
      const z = !FLEETS.ai.hunt ? -1 : farOn(g) ? huntingSea(g, f, reach.filter(clear)) : huntingGround(g, f, [...beat].filter((x) => reach.includes(x) && clear(x)));
      if (z >= 0 && sail(g, f, z, 'hunt')) { const on = beat.has(z) ? ', on its beat' : ''; say(says && subsFound(z) ? `Its destroyers hunt the submarines found ${where(z)}${on}.` : `Its destroyers sweep ${where(z)} for submarines${on}.`); continue; }
    }
    if (best >= 0 && sail(g, f, best, 'patrol')) say(bestWhy || `It patrols ${what(best)}.`);
    else home(shy > 0 ? 'Enemy bombers command the seas in its reach, so it waits in harbour.' : 'Nothing in its reach is worth the risk this month, so it waits in harbour.');
  }
}

/**
 * A lane of a fleet's own side that the raiders are bleeding and no fleet of its side is sailing with, with a sea
 * among those given (the seas in the fleet's reach that it can live in): its own nation's first.
 */
function laneUnguarded(g: Game, f: Fleet, seas: number[]): SeaRoute | null {
  let found: SeaRoute | null = null;
  for (const rt of seaRoutes(g)) {
    // (A lane that carries nothing laid is no staff's need: cargoLaid.)
    if (!laneCounts(rt) || !rt.zones.some((z) => seas.includes(z)) || whyNotEscort(g, f, rt.id)) continue;
    if (g.fleets.some((x) => x !== f && atSea(x) && x.stance === 'escort' && x.lane === rt.id && (x.nat === f.nat || allied(g, x.nat, f.nat)))) continue;
    // (Bleeding: raiders at sea on it now, an enemy in the fortress astride it, or cargo lost on it since last month.)
    const m = g.convoys[rt.id];
    const fort = NAVY.convoys.find((c) => c.id === rt.id)?.base;
    const held = fort ? g.byName[fort.region] : undefined;
    if (!(laneLoss(g, rt.to, rt.zones, rt.id).raid > 0) && !(held && atWar(g, held.ctrl, rt.to)) && !(m && m.turn >= g.turn - 1 && m.tons > 0)) continue;
    if (rt.to === f.nat) return rt;
    found ??= rt;
  }
  return found;
}

/**
 * The sea among those given where a fleet's destroyers should hunt: the one with the most enemy submarines at sea.
 * None for a fleet without destroyers, or while enemy surface ships are out in any of them.
 */
function huntingGround(g: Game, f: Fleet, seas: number[]): number {
  if (f.ships.dd <= 0) return -1;
  let best = -1, most = 0;
  for (const z of seas) {
    let subs = 0;
    for (const x of g.fleets) {
      if (x.zone !== z || !atSea(x) || !atWar(g, x.nat, f.nat)) continue;
      if (surfaceShips(x) > 0) return -1;
      subs += boatsOut(g, x);
    }
    if (subs > most) { most = subs; best = z; }
  }
  return best;
}

/**
 * The far side's hunting ground (the design director's ruling of 19:52Z of 22 November 2026). Each month a staff at
 * war has the seas where the enemy's boats are out, and its fleets with destroyers and nothing better to do hunt
 * there wherever they can reach and live, not on their own beat alone. They are sent one at a time, each to the sea
 * where its own hunters would sink the most boats by the hunt's rule, counting the hunters of its side already
 * there: so many boats draw it, and hunters enough already there send it on to the next sea. A sea where enemy
 * surface ships are out is no hunting ground. Any fleet the hunt counts as a hunter goes, its carriers' aircraft as
 * its destroyers (the director's 20:03Z): a fleet with neither has no hunting ground.
 */
export function huntingSea(g: Game, f: Fleet, seas: number[]): number {
  const H = FLEETS.hunt;
  const mine = (f.ships.dd + f.ships.cv * H.carrier * deckWorth(g, f)) * H.hunter * quality(g, f.nat) * fit(g, f) * techMul(g, f.nat, 'radar');
  if (!(mine > 0)) return -1;
  let best = -1, most = 0;
  for (const z of seas) {
    let subs = 0, surface = false;
    for (const x of g.fleets) {
      if (x.zone !== z || !atSea(x) || !atWar(g, x.nat, f.nat)) continue;
      if (surfaceShips(x) > 0) { surface = true; break; }
      subs += boatsOut(g, x);
    }
    if (surface || subs <= 0) continue;
    let there = 0;
    for (const nat of Object.keys(g.nations)) if (nat === f.nat || allied(g, nat, f.nat)) there += huntersIn(g, nat, z, f).weight;
    const gain = subs * (huntShareSunk(there + mine, subs) - huntShareSunk(there, subs));
    if (gain > most) { most = gain; best = z; }
  }
  return best;
}

/** The shipping lane a computer-run fleet's station lies on, if it feeds the fleet's own nation or an ally at war: its own first. */
function laneOnBeat(g: Game, f: Fleet): SeaRoute | null {
  let found: SeaRoute | null = null;
  for (const rt of seaRoutes(g)) {
    // (A lane that carries nothing laid is no staff's need: cargoLaid.)
    if (!laneCounts(rt) || !rt.zones.includes(f.station) || whyNotEscort(g, f, rt.id)) continue;
    if (rt.to === f.nat) return rt;
    found ??= rt;
  }
  return found;
}
