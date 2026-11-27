// Oil and supplies (the grand campaign): the two stocks a great power keeps beside its treasury.
// Money builds an army, oil moves it, supplies keep it fighting. A nation makes some of each at home, buys some
// from its neighbours, and brings the rest in by sea along shipping lanes that an enemy can raid or shut.
// When a stock runs dry the forces get only what came in that month, and fight, fly, sail and recover worse.
// Every number lives in data/campaign/resources.json. The historical scenarios do not use any of this.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/resources.json';
import { civicSum } from './buildings';
import { KIND, type Corps, type Game, type Kind, type Region, type ShipClass, type Stores, type Terrain } from './types';
import { allied, atWar, bloc, headline } from './game';
import { CLASSES, atSea, boatsOut, closedTo, laneCounts, laneLoss, oreRoute, seaRoutes, zoneOf } from './navy';
import { bargainLanes, carvedShare, replacedDeal } from './bargains';
import { workingFactories } from './rails';
import { prizeTaker } from './prizes';
import { fleetAtPeace, freezeAt } from './pacific';
import { TECH } from './tech';
import { turnToDate } from '../core/ticket';
import { FAR_FIRST, farOn, oilAtHome } from './world';
import { footWorn } from './footing';
import { bombWorn, oilHeld } from './aircampaigns';
import { atomWorn } from './atom';

export type Good = 'oil' | 'sup';

interface LaneRow {
  id: string; name: string; from: string; to: string; zones: string[]; oil: number; sup: number; text: string;
  /** A nation the cargoes are bought from: a war with it ends the traffic. */
  source?: string;
  /** An oilfield the cargoes come from: the lane runs while its holder is no enemy. */
  field?: string;
  /** Narrows the lane must pass: an enemy who holds one shuts it. */
  past?: string[];
  /** Sea powers whose blockade shuts the lane for as long as the recipient is at war with one of them. */
  shutBy?: string[];
  /** The lane only runs while the recipient fights beside one of these. */
  allies?: string[];
  /** Places the lane's road crosses: it opens once every one is in the recipient's or an ally's hands, and an enemy who holds one shuts it. */
  held?: string[];
  /** What is said of the lane while it waits on 'held'. */
  idle?: string;
  /** Harbours the cargoes sail from: the lane runs while the recipient or an ally holds one of them, and an enemy who holds them all shuts it. */
  ports?: string[];
  /**
   * The row's harbours, in 'held' or 'ports', are where its ships land, not where they load (the UI director's words
   * of 12:57Z, 25 November 2026, part 3, for the Pacific's three lanes): a lane cut at them says it has no port left
   * to land at, where a lane cut at its loading harbours says nothing sails from them. Only the words change: what
   * stops the lane is the same rule.
   */
  landing?: boolean;
  /** While the lane named here is open, this one carries these shares in place of its own (the aid to the Soviet Union split between two roads). */
  split?: { lane: string; oil: number; sup: number };
  /**
   * Oilfields the lane carries (ruling 194, point 2): in place of a share of need it brings what these yield to the
   * recipient while it holds them (in full to their owner, less to a conqueror, less a field alight), less what
   * raiders sink in its seas.
   */
  fields?: string[];
  /** A lane that does not run through a sea closed to its recipient (ruling 194, point 3), as the ore stops when the Baltic is closed. */
  closes?: boolean;
  /**
   * Stopped by the freeze (pacific.ts freezeAt: the month America's sales to Japan and the south's stop; ruling 194,
   * point 5). 'until' names a place: once the recipient holds it the cargoes sail again.
   */
  freeze?: { until?: string };
  /**
   * What the lane's row names as its key (ruling 195, point (c); the UI director's sheet 25, box 1): the place held or
   * the seller, as its last sentence says it ("It runs while Pusan is yours."). A lane with none says nothing more
   * than it did: Europe's rows keep their words. `laneKey`.
   */
  key?: string;
  /** A lane of the south the freeze does not touch (the historian's rider to 195(c)): its key says so once the freeze has come. */
  spared?: boolean;
  /** For drawing it on the map: where it comes onto the chart, the seas it is drawn through, and a place it passes on the way. */
  enter?: [number, number];
  draw?: string[];
  via?: string;
}
interface DealRow { id: string; name: string; to: string; oil: number; sup: number; text: string; field?: string; seller?: string; ore?: boolean }

export const RES = data as unknown as {
  nations: string[];
  oil: {
    fields: Record<string, number>; occupied: number; home: Record<string, number>;
    need: {
      corps: Record<string, number>; air: number; ships: Record<ShipClass, number>; port: number; peace: number;
      /**
       * A navy on a peacetime allowance of oil (a row by nation; the far side lays Japan's, the only row): while its
       * fleet is not yet mobilised (pacific.ts `fleetAtPeace`) its ships burn 'draw' in 'war' of what they would burn
       * all at sea, wherever they lie and whatever they do. The two figures are the record's, in millions of barrels
       * a month; the rules read only the one against the other. `navyAllowance`.
       */
      navy?: Record<string, { draw: number; war: number }>;
    };
    start: Record<string, number>; cap: number;
    /** The tanks of a nation that holds more or less than 'cap' months of its need (ruling 194, point 5: a row by nation; the far side lays Japan's). */
    caps?: Record<string, number>;
    /**
     * The most a nation's fuel plants make a month with every factory of its homeland working (a row by nation; the
     * far side lays Japan's, the only row): the record's yield at full build, whatever the technology tree would
     * give. `plantRate`.
     */
    plants?: Record<string, number>;
    short: { attack: number; defence: number; air: number; fleet: number; slowAt: number };
  };
  sup: {
    land: Record<Terrain, number>; factory: number; occupied: number; overseas: number; offmap: number;
    ratio: Record<string, number>; perDiv: number; peace: number; start: Record<string, number>; cap: number;
    short: { fight: number; drafts: number; income: number };
  };
  will: {
    sup: number; oil: number; heal: number; per: number; most: number;
    /**
     * The nations that ask for terms (ruling 196: a row by nation; the far side lays Japan's, the only row): a
     * computer nation here gives in when an enemy holds 'home' of its home ground by worth and privation is at 'most'.
     */
    terms?: Record<string, { home: number }>;
  };
  lanes: LaneRow[];
  deals: DealRow[];
  ai: { armourAt: number; synthAt: number };
};

/** True when oil and supplies are counted. That is the grand campaign: the historical scenarios keep history's clock. */
export const resRules = (g: Game): boolean => g.flags.open === 1;

/** True for a power that keeps stocks of its own. Every other country buys what little it needs abroad and is never short. */
export const keepsStores = (g: Game, nat: string): boolean => resRules(g) && RES.nations.includes(nat) && !!g.nations[nat];

const GOODS: Good[] = ['oil', 'sup'];
export const GOOD_NAME: Record<Good, string> = { oil: 'Oil', sup: 'Supplies' };
const pick = (row: Record<string, number>, nat: string): number => row[nat] ?? row._default ?? 0;

// ---- What a nation makes, and what it needs -----------------------------------------------------------

export interface ResLine {
  key: string; label: string; amount: number; count?: number; note?: string; lane?: string;
  /** A row with no figure of its own in the sum (the oilfields overseas, whose oil is counted once, on the lane under them): the column says "by sea". */
  bySea?: boolean;
}

/** What an oilfield yields a month to whoever holds it: less to a conqueror than to its owner. */
export function fieldYield(g: Game, r: Region, nat: string = r.ctrl): number {
  if (!r.oil) return 0;
  const y = (RES.oil.fields[r.name] ?? RES.oil.fields._default) * (r.owner === nat ? 1 : RES.oil.occupied) * (1 - fieldOut(g, r));
  // Air campaigns (ruling 236.2): a refinery the bombers keep coming back to gives that much less.
  const bombed = oilHeld(g, r);
  return bombed > 0 ? y * (1 - bombed) : y;
}

/**
 * True for a field that gives its holder's depots nothing at home: a field on colony ground is oil overseas (ruling
 * 194, point 1, brought in by ruling 231). Nobody draws it at home, owner or conqueror. It feeds the lane that names
 * it, and where no lane does it waits for one. (No field of the old chart or the Americas stands on colony ground.)
 */
export const fieldOverseas = (r: Region): boolean => r.oil && !!r.overseas;

const oilGround = new WeakMap<Region[], Region[]>();

/**
 * A fleet based beside the oil (ruling 194, point (g)): true when a working oilfield overseas that this nation's side
 * holds stands on one of these seas. In 1944 Japan's battle line lay near Singapore and at Tawi-Tawi, and sailed for
 * Leyte from Brunei, because the oil was there and the tankers to bring it home were not. (An oilfield at home is in
 * the nation's own stock already: a harbour beside Akita's wells is as dry as the rest of Japan.)
 */
export const oilBeside = (g: Game, nat: string, seas: number[]): boolean => !!fieldBeside(g, nat, seas);
/** The working oilfield overseas of this nation's side that stands on one of these seas (`oilBeside`; the fleet's card names it), or null. */
export function fieldBeside(g: Game, nat: string, seas: number[]): Region | null {
  let fields = oilGround.get(g.regions);
  if (!fields) { fields = g.regions.filter((r) => fieldOverseas(r)); oilGround.set(g.regions, fields); }
  for (const r of fields) {
    if (!r.seas.some((z) => seas.includes(z))) continue;
    const holder = prizeTaker(g, r, 'oil');
    if (holder && (holder === nat || allied(g, holder, nat)) && fieldYield(g, r, holder) > 0) return r;
  }
  return null;
}

/** The share of an oilfield's yield lost to sabotage this month: wells set alight burn for months. 0 for a field at work. */
export const fieldOut = (g: Game, r: Region): number => (resRules(g) && (g.flags[`oilout:${r.id}`] ?? 0) > g.turn ? (g.flags[`oilcut:${r.id}`] ?? 0) / 100 : 0);

/** Sets an oilfield alight: it gives this much less for this many months. False where there is no field, or it is burning already. */
export function fireField(g: Game, r: Region, months: number, cut: number): boolean {
  if (!resRules(g) || !r.oil || fieldOut(g, r) > 0) return false;
  g.flags[`oilout:${r.id}`] = g.turn + months;
  g.flags[`oilcut:${r.id}`] = Math.round(cut * 100);
  return true;
}

/** What the technology tree's fuel plants make each month: so much for every working factory at home, and none from a bombed one. */
function treeOil(g: Game, nat: string): { amount: number; factories: number } {
  let rate = 0;
  for (const id of g.nations[nat].tech?.done ?? []) rate += TECH.techs.find((t) => t.id === id)?.oil ?? 0;
  if (!rate) return { amount: 0, factories: 0 };
  let fac = g.nations[nat].offmap?.industry ?? 0;
  // (Air campaigns, ruling 236.2: the fuel plants of a province bombed for its oil make that much less.)
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && !r.overseas) { const b = oilHeld(g, r); fac += b > 0 ? workingFactories(g, r) * (1 - b) : workingFactories(g, r); }
  return { amount: plantRate(g, nat, rate) * fac, factories: fac };
}

/** The most a nation's fuel plants make a month by the record (the far side's row), or null where the tree's rate stands. */
export const plantsMost = (g: Game, nat: string): number | null => (farOn(g) ? RES.oil.plants?.[nat] ?? null : null);

/**
 * What one fuel plant makes a month for a nation whose technology gives `rate` a factory (the far side; the design
 * director's ruling of 20:41Z, 22 November 2026, on the historian's japan-oil-1941-45-partial.md). The tree's rate;
 * for a nation with a row in 'plants', no more than an equal share of the record's yield at full build, counted over
 * every factory of its homeland: its plants never made what the tree gives. A plant bombed or lost makes none of its
 * share, as before.
 */
export function plantRate(g: Game, nat: string, rate: number): number {
  const most = plantsMost(g, nat);
  if (most === null) return rate;
  let all = g.nations[nat].offmap?.industry ?? 0;
  for (const r of g.regions) if (r.owner === nat && !r.overseas) all += r.fac;
  return all > 0 ? Math.min(rate, most / all) : rate;
}

/**
 * True for a nation whose fuel plants already make all the record allows them: more of the tree's fuel chemistry adds
 * no oil. With `but`, read as if that technology were not yet had (what the technology itself adds).
 */
export function plantsSpent(g: Game, nat: string, but?: string): boolean {
  if (plantsMost(g, nat) === null) return false;
  let rate = 0;
  for (const id of g.nations[nat].tech?.done ?? []) if (id !== but) rate += TECH.techs.find((t) => t.id === id)?.oil ?? 0;
  return rate > 0 && plantRate(g, nat, rate) < rate;
}

/**
 * The share of what its ships would burn all at sea that a navy on a peacetime allowance burns this month, or 0 for a
 * navy with no such row and for one that is mobilised (the historian's paper sub-patrols-japan-oil.md, 6c, laid as
 * found by the design director's 13:43Z of 22 November 2026). Japan's fleet did not burn at a war's rate from the
 * China war's second month: from 1937 to 1941 the navy built its stocks up, on about a million and a quarter barrels
 * a month, and the war rate (2,900 barrels an hour, about 2.1 million a month) shows only with the mobilisation of
 * 1941. The game's oil has no barrel, so the allowance is laid as the one figure against the other.
 */
export function navyAllowance(g: Game, nat: string): number {
  const A = RES.oil.need.navy?.[nat];
  return A && A.war > 0 && fleetAtPeace(g, nat) ? A.draw / A.war : 0;
}

/**
 * What the forces burn in a month of war, by arm. 'books' is true for the month's own books, where a navy on a
 * peacetime allowance burns that and no more (`navyAllowance`), and false for the count of what a war would ask,
 * which fills the depots on the first day and sizes the tanks: there every ship burns as it lies, as it always did.
 */
function oilUse(g: Game, nat: string, books = false): { fast: number; fastDivs: number; foot: number; footDivs: number; air: number; planes: number; sea: number; atSea: number; port: number; inPort: number } {
  const N = RES.oil.need;
  const allowance = books ? navyAllowance(g, nat) : 0;
  const u = { fast: 0, fastDivs: 0, foot: 0, footDivs: 0, air: 0, planes: 0, sea: 0, atSea: 0, port: 0, inPort: 0 };
  // (The far side: a corps burns what it holds, as it fights with what it holds. A row kept under a division's full
  // strength, a tank regiment of a quarter of a division or a brigade, burns that share of a division's fuel and is
  // counted as that much of a division: Japan's thirteen tank regiments of December 1941 burned as thirteen tank
  // divisions. The design director's 14:05Z of 22 November 2026, every nation and every kind.)
  const sized = farOn(g);
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead) continue;
    const rate = N.corps[c.kind] ?? N.corps._default, divs = sized ? c.divs * (c.full ?? 1) : c.divs;
    if (c.kind === 'arm' || c.kind === 'mot') { u.fast += rate * divs; u.fastDivs += divs; } else { u.foot += rate * divs; u.footDivs += divs; }
  }
  for (const f of g.airFleets) if (f.nat === nat) { u.planes += f.ftr + f.bmr; u.air += (f.ftr + f.bmr) * N.air; }
  for (const f of g.fleets) {
    if (f.nat !== nat) continue;
    let burn = 0, hulls = 0;
    for (const c of CLASSES) { burn += f.ships[c] * N.ships[c]; hulls += f.ships[c]; }
    // (The far side's one measure, navy.ts `boatsOut`: of a fleet's submarines at sea only the boats out on patrol burn
    // as ships at sea. The rest lie at the base and burn at the harbour's rate. The peacetime allowance is a share of
    // what the ships would burn all at sea in a war, so its base reads the same measure: the record's ratio is the
    // peace draw against the war burn, and a war burn with boats in port is the burn of the boats out. The design
    // director's 18:04Z and 18:23Z of 22 November 2026.)
    const idle = f.ships.ss - boatsOut(g, f), saved = idle * N.ships.ss * (1 - N.port);
    if (allowance > 0) burn = (burn - saved) * allowance;
    if (atSea(f)) {
      if (allowance <= 0 && idle > 0) { burn -= idle * N.ships.ss; hulls -= Math.round(idle); u.port += idle * N.ships.ss * N.port; u.inPort += Math.round(idle); }
      u.sea += burn; u.atSea += hulls;
    } else { u.port += allowance > 0 ? burn : burn * N.port; u.inPort += hulls; }
  }
  return u;
}

/** What a province gives in supplies: what its ground grows and what its working factories turn out. */
function landYield(g: Game, r: Region, built = false): number {
  return RES.sup.land[r.terrain] + (built ? r.fac : workingFactories(g, r)) * RES.sup.factory;
}

/**
 * True for ground that gives its holder no supplies at home with no ship (ruling 195, point 1): a colony or occupied
 * ground on the far side of the world. Europe's and the Americas' count as they always have, a colony at half and
 * occupied ground at a quarter; a nation's own home ground on the far side counts at home with the people it feeds.
 */
export const overTheWater = (r: Region, nat: string): boolean => r.id >= FAR_FIRST && (r.owner !== nat || !!r.overseas);

const divisions = (g: Game, nat: string): number => g.corps.reduce((s, c) => s + (c.nat === nat && !c.dead ? c.divs : 0), 0);

/**
 * A month's need of oil or supplies. 'war' asks what it would be at war (or at peace), whatever the nation is doing
 * now; left out, the need is the month's own, as its books count it.
 */
export function needOf(g: Game, nat: string, good: Good, war?: boolean): number {
  return outLines(g, nat, good, war).reduce((s, l) => s + l.amount, 0);
}

function outLines(g: Game, nat: string, good: Good, asked?: boolean): ResLine[] {
  const n = g.nations[nat];
  const war = asked ?? n.atWar.length > 0;
  if (good === 'oil') {
    const u = oilUse(g, nat, asked === undefined), k = war ? 1 : RES.oil.need.peace;
    return [
      { key: 'fast', label: 'Tanks and lorried troops', amount: u.fast * k, count: u.fastDivs },
      { key: 'foot', label: 'The rest of the army', amount: u.foot * k, count: u.footDivs },
      { key: 'air', label: 'Aircraft', amount: u.air * k, count: u.planes },
      { key: 'sea', label: 'Ships at sea', amount: u.sea * k, count: u.atSea },
      { key: 'port', label: 'Ships in harbour', amount: u.port * k, count: u.inPort },
    ].filter((l) => l.amount > 1e-9);
  }
  // The people at home eat whether there is a war on or not; the army in the field uses far more in war.
  let home = 0, held = 0;
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && !r.overseas) { home += landYield(g, r, true); held++; }
  if (n.offmap) home += n.offmap.industry * RES.sup.offmap;
  const divs = divisions(g, nat);
  return [
    { key: 'people', label: 'The people at home', amount: home / pick(RES.sup.ratio, nat), count: held },
    { key: 'army', label: 'The army in the field', amount: divs * RES.sup.perDiv * (war ? 1 : RES.sup.peace), count: divs },
  ].filter((l) => l.amount > 1e-9);
}

// ---- Lanes and deals ------------------------------------------------------------------------------------

export type LaneState = 'open' | 'raided' | 'cut' | 'idle';

export interface Lane {
  id: string;
  /** Points on the map to draw it through, from where it comes onto the chart to the port it serves. Empty for a lane that is not drawn. */
  path: [number, number][];
  /** A lane across the sea that raiders can attack, or a deal that comes overland. */
  kind: 'sea' | 'land';
  name: string;
  from: string;
  to: string;
  text: string;
  /** The seas it crosses, in order from the open ocean to the home ports. */
  zones: number[];
  /** What it would bring this month if every cargo arrived. */
  full: { oil: number; sup: number };
  /** The share that gets through, 0 to 1. */
  flow: number;
  /** What it brings this month at that flow. */
  oil: number;
  sup: number;
  /** Raiders against escorts on the lane, as the convoy war counts them. */
  raid: number;
  guard: number;
  state: LaneState;
  /** Why it is cut or idle, or what is happening on it, in a sentence. */
  why: string;
}

const friendly = (g: Game, holder: string, nat: string): boolean => {
  if (holder === nat) return true;
  const H = g.nations[holder], N = g.nations[nat];
  if (!H || !N || atWar(g, holder, nat)) return false;
  // A neutral sells to anyone; a country in the other camp does not.
  const hb = bloc(H), nb = bloc(N);
  return hb === null || nb === null || hb === nb;
};

const natOf = (g: Game, id: string): string => g.nations[id]?.name ?? id;
const the = (name: string): string => (/^(United|Soviet|Netherlands|Dominican Republic)/.test(name) ? `the ${name}` : name);

/** Where a sea lane runs on the map: in from the edge of the chart, through its seas, to the recipient's best port on the last of them. */
function lanePath(g: Game, row: LaneRow): [number, number][] {
  const seas = (row.draw ?? row.zones).map((k) => g.seas[zoneOf(g, k)]).filter(Boolean);
  if (!seas.length) return [];
  const pts: [number, number][] = [];
  if (row.enter) pts.push(row.enter);
  const via = row.via ? g.byName[row.via] : null;
  if (via) pts.push([via.x, via.y + 16]);
  // A little below each sea's name, so the line does not run through the lettering.
  for (const z of seas) pts.push([z.x, z.y + 22]);
  const last = seas[seas.length - 1].id;
  let port: Region | null = null;
  for (const r of g.regions) if (r.port && r.owner === row.to && !r.overseas && r.seas.includes(last) && (!port || r.fac > port.fac)) port = r;
  const cap = g.nations[row.to]?.capital ?? -1;
  const end = port ?? (cap >= 0 ? g.regions[cap] : null);
  if (end) pts.push([end.x, end.y]);
  return pts;
}

/** Why nothing sails on a sea lane this month, if nothing does: it is cut by an enemy, or idle because the trade has not begun. */
function laneStop(g: Game, row: LaneRow): { state: 'cut' | 'idle'; why: string } | null {
  const s = laneStopBy(g, row);
  return s ? { state: s.state, why: s.why } : null;
}

/** What a stop turns on, for the row's key sentence: a place, the seller, the freeze, a sea, or anything else. */
type StopBy = 'place' | 'seller' | 'freeze' | 'sea' | 'other';

/** `laneStop`, with what the stop turns on. */
function laneStopBy(g: Game, row: LaneRow): { state: 'cut' | 'idle'; why: string; by: StopBy } | null {
  // (Every reason is a sentence of its own: "the United Kingdom holds Gibraltar" starts with a capital.)
  let by: StopBy = 'other';
  const cut = (why: string, state: 'cut' | 'idle' = 'cut'): { state: 'cut' | 'idle'; why: string; by: StopBy } => ({ state, why: why[0].toUpperCase() + why.slice(1), by });
  const to = row.to;
  if (row.allies) {
    const friend = row.allies.find((a) => g.nations[a]?.alive && allied(g, a, to) && g.nations[a].atWar.length > 0);
    if (!friend || !g.nations[to].atWar.length) return cut(`It runs only while ${the(natOf(g, to))} fights beside the western Allies.`, 'idle');
  }
  by = 'place';
  for (const name of row.held ?? []) {
    const r = g.byName[name];
    if (!r) continue;
    if (atWar(g, r.ctrl, to)) return cut(row.landing ? `${the(natOf(g, r.ctrl))} holds ${r.name} and the lane has no port left to land at.` : `${the(natOf(g, r.ctrl))} holds ${r.name} and the road through it is shut.`);
    if (r.ctrl !== to && !allied(g, r.ctrl, to)) return cut(row.idle ?? `It opens once ${r.name} is in friendly hands.`, 'idle');
  }
  if (row.ports) {
    const ports = row.ports.map((name) => g.byName[name]).filter((r): r is Region => !!r);
    if (ports.length && !ports.some((r) => r.ctrl === to || allied(g, r.ctrl, to))) {
      const all = ports.length === 1 ? ports[0].name : `${ports.slice(0, -1).map((r) => r.name).join(', ')} and ${ports[ports.length - 1].name}`;
      const foe = ports.every((r) => atWar(g, r.ctrl, to)) ? ports[0].ctrl : null;
      // (Harbours the ships land at: one holder names them, more than one is "enemy hands". The UI director, 12:57Z of 25 November 2026, part 3.)
      if (foe && row.landing) return cut(ports.some((r) => r.ctrl !== foe) ? 'Every port the lane lands at is in enemy hands.' : `${the(natOf(g, foe))} holds ${all}${ports.length === 1 ? '' : ','} and the lane has no port left to land at.`);
      if (foe) return cut(`${the(natOf(g, foe))} holds ${all}, and nothing sails from ${ports.length === 1 ? 'it' : 'them'}.`);
      return cut(row.idle ?? `It opens once ${ports.length === 1 ? all : `one of ${all}`} is in friendly hands.`, 'idle');
    }
  }
  by = 'seller';
  if (row.source && g.nations[row.source] && atWar(g, row.source, to)) return cut(`${the(natOf(g, to))} is at war with ${the(natOf(g, row.source))}, which sold the cargoes.`);
  by = 'other';
  if (row.field) {
    const f = g.byName[row.field];
    // (A field held by its own country is that country's to give or keep: "Iraq is at war with the United Kingdom", the UI director's words for ruling 228.)
    if (f && !friendly(g, f.ctrl, to)) {
      if (f.ctrl !== f.owner) return cut(`${f.name} is in the hands of ${the(natOf(g, f.ctrl))}.`);
      return cut(atWar(g, f.ctrl, to) ? `${the(natOf(g, f.ctrl))} is at war with ${the(natOf(g, to))}.` : `${the(natOf(g, f.ctrl))} has gone over to the other side.`);
    }
  }
  by = 'place';
  for (const name of row.past ?? []) {
    const r = g.byName[name];
    if (r && atWar(g, r.ctrl, to)) return cut(`${the(natOf(g, r.ctrl))} holds ${r.name} and lets nothing past.`);
  }
  by = 'other';
  const jailer = (row.shutBy ?? []).find((x) => g.nations[x]?.alive && atWar(g, x, to));
  if (jailer) return cut(`Shut by the blockade of ${the(natOf(g, jailer))}.`);
  by = 'freeze';
  if (row.freeze && freezeAt(g) > 0) {
    // (The freeze stops what was bought; what the recipient has taken for its own sails again.)
    const prize = row.freeze.until ? g.byName[row.freeze.until] : null;
    if (!prize || prize.ctrl !== to) return cut(`Stopped by the freeze: nothing is sold to ${the(natOf(g, to))}.`);
  }
  by = 'other';
  if (row.fields && !carried(g, row).length) return cut(`${the(natOf(g, to))} holds none of the oilfields it carries.`, 'idle');
  by = 'sea';
  if (row.closes) {
    for (const k of row.zones) {
      const z = zoneOf(g, k);
      if (z >= 0 && closedTo(g, z, to)) return cut(`The ${g.seas[z].name} is closed to the ships of ${the(natOf(g, to))}.`);
    }
  }
  return null;
}

/** The fields a lane carries that its recipient holds this month (ruling 194, point 2). */
function carried(g: Game, row: LaneRow): Region[] {
  const out: Region[] = [];
  for (const name of row.fields ?? []) {
    const r = g.byName[name];
    // (Held as the oil book counts a field held: prizes.ts.)
    if (r?.oil && prizeTaker(g, r, 'oil') === row.to) out.push(r);
  }
  return out;
}

/** Why nothing sails on the shipping lane with this id this month, or null while cargoes are moving (or it is no lane of these books). */
export function laneStopped(g: Game, id: string): { state: 'cut' | 'idle'; why: string } | null {
  const row = RES.lanes.find((l) => l.id === id);
  return row && g.nations[row.to] ? laneStop(g, row) : null;
}

/**
 * The last sentence of a lane's row (ruling 195, point (c) and the historian's rider; the UI director's sheet 25,
 * box 1): what the lane runs on, a place held or a peace kept, and for a stopped lane what stopped it and what would
 * start it again, or that nothing will. "Yours" to the nation the lane feeds, by its name to everyone else, so an
 * enemy reads both ways to stop a lane: sink it, or take its key. Empty for a lane whose row names no key.
 */
export function laneKey(g: Game, id: string, me: string | null): string {
  const row = RES.lanes.find((l) => l.id === id);
  if (!row?.key || !g.nations[row.to]) return '';
  const to = row.to, mine = me === to, whose = mine ? 'yours' : `${natOf(g, to)}'s`;
  const hands = (place: string): string => (mine ? `${place} is yours` : `${place} is in ${natOf(g, to)}'s hands`);
  const at = freezeAt(g), month = at > 0 ? turnToDate(at).label : '';
  const prize = row.freeze?.until ? g.byName[row.freeze.until] : null;
  // What it runs on: a seller's good will, a narrows no enemy holds, or a place of the recipient's own.
  // (Once the freeze has come, a lane it stops with a prize named runs only for the prize in the recipient's own hands.)
  const frozen = at > 0 && !!row.freeze && !!prize;
  const runs = row.source ? `${row.from} sells to ${mine ? 'you' : natOf(g, to)}` : row.past && !frozen ? `the holder of ${row.key} is no enemy of ${whose}` : hands(prize?.name ?? row.key);
  const stop = laneStopBy(g, row);
  if (!stop) return `It runs while ${runs}.${row.spared && at > 0 ? ' The freeze does not touch it.' : ''}`;
  if (stop.by === 'freeze') return `Stopped by the freeze of ${month}. ${prize ? `It runs again when ${hands(prize.name)}.` : 'It does not run again.'}`;
  if (stop.by === 'seller') return `${stop.why} It does not run again.`;
  // (A place of its own that it does not hold, with no war on: the row's "it opens once" would say the key twice.)
  const place = g.byName[row.key];
  if (stop.by === 'place' && stop.state === 'idle' && place) { const who = the(natOf(g, place.ctrl)); return `${who[0].toUpperCase()}${who.slice(1)} holds ${place.name}. It runs again when ${runs}.`; }
  if (stop.by === 'sea') return `${stop.why} It runs again when that sea is open to ${mine ? 'your ships' : 'them'}.`;
  return `${stop.why} It runs again when ${runs}.`;
}

/** The lane that carries an oilfield overseas home to this nation, if one of its rows names the field. */
const fieldLane = (r: Region, nat: string): LaneRow | undefined => RES.lanes.find((l) => l.to === nat && !!l.fields?.includes(r.name));

/** A field's monthly figure at the wells, to one decimal as the sheet prints it: 19.8, 8.4, 14. */
const fmt = (x: number): string => (Math.round(x * 10) / 10).toString();

/** A share in tenths, in words: 0.6 is "six tenths". */
const TENTHS = ['none', 'a tenth', 'two tenths', 'three tenths', 'four tenths', 'half', 'six tenths', 'seven tenths', 'eight tenths', 'nine tenths', 'all'];

/**
 * What the province card says of an oilfield overseas (ruling 194, points 1 and 2; the UI director's sheet 24, box
 * 2): what it yields at the wells, where its oil goes and how much of it is getting through, and for an enemy's field
 * what it would be in the reader's own hands. Null for any other ground: a field at home keeps its words.
 */
export function fieldCard(g: Game, r: Region, me: string | null): { wells: string; goes: string; yours?: string } | null {
  if (!resRules(g) || !fieldOverseas(r)) return null;
  const holder = prizeTaker(g, r, 'oil') ?? r.ctrl;
  const y = fieldYield(g, r, holder);
  const wells = `${fmt(y)} oil a month at the wells${holder !== r.owner ? `, ${TENTHS[Math.round(RES.oil.occupied * 10)] ?? `${Math.round(RES.oil.occupied * 100)}%`} of what its owner drew` : ''}.`;
  const went = (nat: string, home: string): string => {
    const row = keepsStores(g, nat) ? fieldLane(r, nat) : undefined;
    if (!row) return '';
    const l = lanesTo(g, nat).find((x) => x.id === row.id);
    const state = !l || l.state === 'idle' ? ', which is not running' : l.state === 'cut' ? ', which is cut' : `: ${Math.round(l.flow * 100)}% getting through`;
    return `${home} by sea, on ${row.name}${state}.`;
  };
  const own = holder === me;
  const goes = went(holder, own ? 'Its oil goes home' : `Its oil goes home to ${natOf(g, holder)}`) || 'Its oil reaches no one at home.';
  if (own || !me || !atWar(g, me, holder)) return { wells, goes };
  // (An enemy's field says what it would be in the reader's hands, as an enemy's island does: taking it denies the oil, and may fill no tank of his own.)
  const row = keepsStores(g, me) ? fieldLane(r, me) : undefined;
  return { wells, goes, yours: row ? `its oil would go home by sea, on ${row.name}.` : 'its oil would reach no one at home.' };
}

/** One sea lane as it stands this month for the nation it feeds. 'need' is that nation's monthly need of each good. */
function seaLane(g: Game, row: LaneRow, need: Record<Good, number>): Lane {
  const zones = row.zones.map((k) => zoneOf(g, k)).filter((z) => z >= 0);
  // (Ruling 228(3a): once the Persian road is open the aid round the North Cape is half what it was, the rest coming through Iran.)
  const other = row.split ? RES.lanes.find((l) => l.id === row.split!.lane) : undefined;
  const part = row.split && other && !laneStop(g, other) ? row.split : row;
  // (A neutral's bargain may carve a share of a lane's supply out of it: Argentina's meat, release 2.)
  const full = { oil: part.oil * need.oil, sup: part.sup * need.sup * (1 - carvedShare(g, row.id)) };
  // (A lane that carries fields brings their yield, whatever the need: ruling 194, point 2.)
  if (row.fields) full.oil = carried(g, row).reduce((s, r) => s + fieldYield(g, r, row.to), 0);
  const lane: Lane = { id: row.id, path: lanePath(g, row), kind: 'sea', name: row.name, from: row.from, to: row.to, text: row.text, zones, full, flow: 1, oil: 0, sup: 0, raid: 0, guard: 0, state: 'open', why: '' };
  const to = row.to;
  const stop = laneStop(g, row);
  if (stop) return { ...lane, flow: 0, state: stop.state, why: stop.why };
  if (zones.length && g.nations[to].atWar.length) {
    const m = laneLoss(g, to, zones, row.id);
    lane.raid = m.raid; lane.guard = m.guard; lane.flow = 1 - m.loss;
    if (m.loss >= 0.02) { lane.state = 'raided'; lane.why = `Raiders are sinking ${Math.round(m.loss * 100)}% of the cargoes.`; }
  }
  lane.oil = full.oil * lane.flow; lane.sup = full.sup * lane.flow;
  return lane;
}

/** One overland deal as it stands this month. */
function deal(g: Game, row: DealRow): Lane {
  const full = { oil: row.oil, sup: row.sup };
  const lane: Lane = { id: row.id, path: [], kind: 'land', name: row.name, from: '', to: row.to, text: row.text, zones: [], full, flow: 1, oil: row.oil, sup: row.sup, raid: 0, guard: 0, state: 'open', why: '' };
  const stop = (why: string, state: LaneState = 'cut'): Lane => ({ ...lane, flow: 0, oil: 0, sup: 0, state, why });
  const to = row.to;
  if (row.ore) {
    lane.from = 'Sweden';
    const way = oreRoute(g);
    // At peace the ore comes as it always did; at war it comes by whichever way is open.
    if (way === 'cut') return stop('The ore cannot get through: the Baltic is frozen or closed and Narvik is in enemy hands.');
    lane.why = way === 'narvik' ? 'Coming through Narvik while the Baltic is frozen.' : '';
    return lane;
  }
  let seller = row.seller ?? '';
  if (row.field) {
    const f = g.byName[row.field];
    if (!f) return stop('', 'idle');
    seller = f.ctrl;
    lane.from = natOf(g, f.owner);
    // A field the buyer holds itself is its own production, not a purchase.
    if (seller === to) return stop(`${f.name} is in ${natOf(g, to)}'s own hands and counts as its own production.`, 'idle');
  } else lane.from = natOf(g, seller);
  const S = g.nations[seller];
  if (!S || (!S.alive && !row.field)) return stop(`${the(natOf(g, seller))} has fallen.`);
  if (!friendly(g, seller, to)) {
    const f = row.field ? g.byName[row.field] : null;
    if (f && f.owner !== seller) return stop(`${f.name} is in the hands of ${the(natOf(g, seller))}.`);
    return stop(atWar(g, seller, to) ? `${the(natOf(g, to))[0].toUpperCase()}${the(natOf(g, to)).slice(1)} is at war with ${the(natOf(g, seller))}.` : `${the(natOf(g, seller))[0].toUpperCase()}${the(natOf(g, seller)).slice(1)} has gone over to the other side.`);
  }
  return lane;
}

/** Every lane and deal that feeds a nation, as it stands this month. */
export function lanesTo(g: Game, nat: string): Lane[] {
  if (!keepsStores(g, nat)) return [];
  const need = { oil: needOf(g, nat, 'oil'), sup: needOf(g, nat, 'sup') };
  // (A deal a neutral's bargain replaces follows the bargain's buyer instead, while its seller sells: release 2.)
  return [...RES.lanes.filter((l) => l.to === nat).map((l) => seaLane(g, l, need)), ...RES.deals.filter((d) => d.to === nat && !replacedDeal(g, d.id)).map((d) => deal(g, d)), ...bargainLanes(g, nat)];
}

/**
 * What the stores page prints for a lane that carries nothing laid, in place of the share or the tons it would print
 * (the UI director's words of 12:57Z, 25 November 2026, part 2(b), for the design director's ruling of 12:39Z: "a 0
 * that stands for 'not read' is not a read zero"). It wears no colour of its own. One place, read by the browser's
 * stores page and the 3D game's alike.
 */
export const NOT_COUNTED = {
  value: 'Not counted',
  line: 'Its shipping is not counted in the game, so it adds nothing to these stores, and its loss takes nothing from them.',
} as const;

/**
 * The words for a sea lane whose shipping the game does not count (NOT_COUNTED), or null for one that carries
 * something. It asks the navy's one predicate, the one every staff reads (navy.ts laneCounts), so the page and the
 * staffs can never disagree. A lane with no seas on this chart is no route of the navy's and keeps its own words.
 */
export function laneNotCounted(g: Game, l: Lane): typeof NOT_COUNTED | null {
  if (l.kind !== 'sea') return null;
  const rt = seaRoutes(g).find((r) => r.id === l.id);
  return rt && !laneCounts(rt) ? NOT_COUNTED : null;
}

/** Every sea lane on the map, whoever it feeds: what an interface draws across the water. */
export function seaLanes(g: Game): Lane[] {
  if (!resRules(g)) return [];
  const out: Lane[] = [];
  for (const nat of RES.nations) {
    if (!g.nations[nat]?.alive) continue;
    for (const l of lanesTo(g, nat)) if (l.kind === 'sea' && l.zones.length) out.push(l);
  }
  return out;
}

// ---- The books ---------------------------------------------------------------------------------------------

export interface Book {
  good: Good;
  /** In the depots now, and the most they can hold. */
  stock: number;
  cap: number;
  /** Where it comes from each month, and where it goes. */
  income: ResLine[];
  spend: ResLine[];
  made: number;
  need: number;
  net: number;
  /** Months until the depots are empty at this rate; Infinity when the stock is steady or growing. */
  months: number;
  /** The share of the need that was met when the month began: 1 unless the depots were empty. */
  met: number;
  /** What the shortage is costing, in words. Empty when the need is met. */
  effects: string[];
}


export interface StoresView {
  nat: string; oil: Book; sup: Book; lanes: Lane[];
  /** The share of the nation's will to fight that privation at home has worn away, 0 to 1: all of it, whatever the cause. */
  willLost: number;
  /** Of that, what shortage took, and what all-out war took (the nation's footing, ruling 172; 0 with it switched off). */
  hungerLost: number;
  footWorn: number;
  /** Of that, what atomic strikes on its cities took (release 2, atom.ts; 0 with the project off): it does not come back. */
  atomWorn: number;
}

function incomeLines(g: Game, nat: string, good: Good, lanes: Lane[]): ResLine[] {
  const n = g.nations[nat];
  const out: ResLine[] = [];
  if (good === 'oil') {
    let own = 0, nOwn = 0, taken = 0, nTaken = 0;
    for (const r of g.regions) {
      // The wells lie outside the town: an army that holds the field round it has them, though the town holds (prizes.ts).
      // (A field on colony ground gives nothing at home: fieldOverseas.)
      if (!r.oil || fieldOverseas(r) || prizeTaker(g, r, 'oil') !== nat) continue;
      if (r.owner === nat) { own += fieldYield(g, r, nat); nOwn++; } else { taken += fieldYield(g, r, nat); nTaken++; }
    }
    // The wells and plants at home work for as long as the homeland is held.
    const home = (oilAtHome(g, nat) ?? pick(RES.oil.home, nat)) * (n.offmap || (n.capital >= 0 && g.regions[n.capital].ctrl === nat) ? 1 : 0.5);
    out.push({ key: 'fields', label: 'Oilfields at home', amount: own, count: nOwn });
    out.push({ key: 'taken', label: 'Captured oilfields', amount: taken, count: nTaken });
    out.push({ key: 'home', label: n.offmap ? 'The oilfields of the homeland, beyond the map' : 'Small wells and plants at home', amount: home });
    const plants = treeOil(g, nat);
    out.push({ key: 'tech', label: 'Synthetic fuel plants', amount: plants.amount, count: plants.factories });
  } else {
    let land = 0, held = 0, fac = 0, nFac = 0, occ = 0, nOcc = 0, sea = 0, nSea = 0;
    for (const r of g.regions) {
      if (r.ctrl !== nat) continue;
      // (Ruling 195, point 1: colonies and occupied ground on the far side of the world give nothing at home with no
      // ship, to anybody. Lanes stand for them, as ruling 194 has it for oil.)
      if (overTheWater(r, nat)) continue;
      const works = workingFactories(g, r);
      if (r.owner !== nat) { occ += landYield(g, r) * RES.sup.occupied; nOcc++; }
      else if (r.overseas) { sea += landYield(g, r) * RES.sup.overseas; nSea++; }
      else { land += RES.sup.land[r.terrain]; held++; fac += works * RES.sup.factory; nFac += works; }
    }
    out.push({ key: 'land', label: 'Farms, mines and forests at home', amount: land, count: held });
    out.push({ key: 'fac', label: 'Factories at home', amount: fac, count: nFac });
    if (n.offmap) out.push({ key: 'homeland', label: 'The farms and mills of the homeland, beyond the map', amount: n.offmap.industry * RES.sup.offmap });
    out.push({ key: 'colonies', label: 'The colonies', amount: sea, count: nSea });
    out.push({ key: 'occupied', label: 'Taken from occupied ground', amount: occ, count: nOcc });
    // Farm buildings in the towns (the grand campaign).
    const farms = civicSum(g, nat, true).sup;
    if (farms > 0) out.push({ key: 'farms', label: 'Cooperative farms, granaries and food works', amount: farms });
  }
  for (const l of lanes) {
    // The oilfields overseas stand directly above the lane that carries their oil, with no figure of their own: the
    // oil is counted once, on the lane (ruling 194, point 2; the UI director's sheet 24, box 1).
    const row = good === 'oil' && l.kind === 'sea' ? RES.lanes.find((x) => x.id === l.id) : undefined;
    const held = row?.fields ? carried(g, row) : [];
    if (held.length) out.push({ key: `overseas:${l.id}`, label: 'Oilfields overseas', amount: 0, count: held.length, bySea: true, note: `${held.length} field${held.length === 1 ? '' : 's'}, feeding ${l.name} · ${fmt(l.full.oil)} a month at the wells` });
    if (l.full[good] <= 0) continue;
    const label = l.kind === 'sea' ? `${l.name[0].toUpperCase()}${l.name.slice(1)}` : `${l.name}${l.from ? `, from ${the(l.from)}` : ''}`;
    out.push({ key: `lane:${l.id}`, label, amount: l[good], lane: l.id, note: l.state === 'open' ? (l.why || undefined) : l.state === 'raided' ? `${Math.round(l.flow * 100)}% getting through` : l.why });
  }
  // What the nation has agreed to send abroad is gone before its own forces draw a drop.
  for (const d of RES.deals) {
    if (d.seller !== nat || d[good] <= 0) continue;
    const l = deal(g, d);
    if (l.state === 'open') out.push({ key: `sold:${d.id}`, label: `Deliveries to ${the(natOf(g, d.to))}`, amount: -d[good] });
  }
  return out.filter((l) => Math.abs(l.amount) > 1e-9 || l.lane !== undefined || l.bySea);
}

const SHORT_OIL = (m: number): string[] => {
  const s = RES.oil.short, lack = 1 - m, pc = (x: number): number => Math.round(x * lack * 100);
  return [
    `Tanks and lorried troops attack ${pc(s.attack)}% weaker and defend ${pc(s.defence)}% weaker`,
    ...(m < s.slowAt ? ['They lose one move a month'] : []),
    `Aircraft are worth ${pc(s.air)}% less in every fight`,
    `The fleet is ${pc(s.fleet)}% less fit for sea`,
  ];
};
const SHORT_SUP = (m: number): string[] => {
  const s = RES.sup.short, lack = 1 - m, pc = (x: number): number => Math.round(x * lack * 100);
  return [`Every corps fights ${pc(s.fight)}% weaker`, `${pc(s.drafts)}% fewer replacements reach the armies`, `The treasury's income is down ${pc(s.income)}% as factories stand idle`];
};

/** The state a nation's stocks are kept in, made the first time it is asked for: depots filled for so many months of war. */
export function storesOf(g: Game, nat: string): Stores | null {
  if (!keepsStores(g, nat)) return null;
  const n = g.nations[nat];
  if (!n.res) {
    n.res = { oil: 0, sup: 0, oilMet: 1, supMet: 1 };
    n.res.oil = pick(RES.oil.start, nat) * needOf(g, nat, 'oil', true);
    n.res.sup = pick(RES.sup.start, nat) * needOf(g, nat, 'sup', true);
  }
  return n.res;
}

/** A nation's books for oil and for supplies this month: what comes in, what goes out, what is left, and every lane that feeds it. */
export function stores(g: Game, nat: string): StoresView | null {
  const st = storesOf(g, nat);
  if (!st) return null;
  const lanes = lanesTo(g, nat);
  const book = (good: Good): Book => {
    const income = incomeLines(g, nat, good, lanes), spend = outLines(g, nat, good);
    const made = income.reduce((s, l) => s + l.amount, 0), need = spend.reduce((s, l) => s + l.amount, 0);
    const cap = (good === 'oil' ? RES.oil.caps?.[nat] ?? RES.oil.cap : RES.sup.cap) * Math.max(needOf(g, nat, good, true), 1);
    const net = made - need;
    const met = good === 'oil' ? st.oilMet : st.supMet;
    return { good, stock: st[good], cap, income, spend, made, need, net, months: net < -1e-9 ? st[good] / -net : Infinity, met, effects: met < 0.995 ? (good === 'oil' ? SHORT_OIL(met) : SHORT_SUP(met)) : [] };
  };
  return { nat, oil: book('oil'), sup: book('sup'), lanes, willLost: willLost(g, nat), hungerLost: hungerLost(g, nat), footWorn: footWorn(g, nat), atomWorn: atomWorn(g, nat) };
}

/** Opens the stocks of every power that keeps them, once, when a grand campaign begins. */
export function openStores(g: Game): void {
  if (!resRules(g)) return;
  // Counted afresh as the campaign opens: a depot looked into while the world was still being set up (before
  // America's air commands stood on their fields, Version 84.2) was filled for a smaller force than takes the field.
  for (const nat of RES.nations) if (g.nations[nat]) { delete g.nations[nat].res; storesOf(g, nat); }
}

/**
 * The month's reckoning, at the start of a nation's turn: what was made and brought in goes into the depots, what the
 * forces and the people need comes out. If the depots run dry the need is only part met, and the nation suffers for it
 * until next month's reckoning.
 */
export function resourcesTick(g: Game, nat: string): void {
  const v = stores(g, nat);
  if (!v) return;
  const st = g.nations[nat].res!;
  const hungryBefore = st.hunger ?? 0;
  for (const good of GOODS) {
    const b = v[good];
    let stock = st[good] + b.made - b.need, met = 1;
    if (stock < 0) { met = b.need > 0 ? Math.max(0, Math.min(1, (b.need + stock) / b.need)) : 1; stock = 0; }
    st[good] = Math.min(stock, b.cap);
    const was = good === 'oil' ? st.oilMet : st.supMet;
    if (good === 'oil') st.oilMet = met; else st.supMet = met;
    if (nat !== g.human) continue;
    const name = GOOD_NAME[good].toLowerCase(), key = `res:${good}:${nat}`, low = `res:low:${good}:${nat}`;
    if (met < 0.995 && was >= 0.995) {
      headline(g, 'info', good === 'oil'
        ? `The oil has run out. Only ${Math.round(met * 100)}% of what the forces need came in this month: tanks, aircraft and ships are short of fuel. Open Oil on the top bar to see where it comes from and what is stopping it.`
        : `The depots are empty. Only ${Math.round(met * 100)}% of the supplies the nation needs came in this month: the armies fight weaker, fewer replacements go out and factories stand idle. Open Supplies on the top bar to see why.`, nat);
      g.flags[key] = g.turn;
    } else if (met >= 0.995 && was < 0.995) {
      headline(g, 'info', `There is ${name} enough again: the forces have all they need this month.`, nat);
      delete g.flags[key];
    }
    // A warning while there is still time to do something about it.
    const left = b.net < -1e-9 ? st[good] / -b.net : Infinity;
    if (met >= 0.995 && left <= 3 && g.flags[low] === undefined) {
      g.flags[low] = g.turn;
      headline(g, 'info', `${GOOD_NAME[good]} for ${Math.max(1, Math.round(left))} more month${Math.round(left) <= 1 ? '' : 's'}: more is going out than coming in. Open ${GOOD_NAME[good]} on the top bar.`, nat);
    } else if (left > 6 && g.flags[low] !== undefined) delete g.flags[low];
  }
  // Privation at home: every month the depots are empty the people go shorter, and a people that goes short long
  // enough will not fight on. A month with everything it needs lets it recover.
  const W = RES.will, lack = W.sup * (1 - st.supMet) + W.oil * (1 - st.oilMet);
  const hunger = lack > 1e-9 ? hungryBefore + lack : Math.max(0, hungryBefore - W.heal);
  if (hunger > 0) st.hunger = hunger; else delete st.hunger;
  if (nat === g.human && hungerLost(g, nat) >= 0.1 && hungryBefore * W.per < 0.1) {
    headline(g, 'info', `Rationing bites at home. The longer the depots stay empty the less the nation will bear: its will to fight is falling, and a country that is starved long enough asks for terms. Open Powers to see how much fight is left.`, nat);
  }
}

/**
 * What privation has taken from a nation's will to fight, as a share of the whole: it counts toward surrender beside
 * a lost capital, lost ground and a broken army. This is how a blockade chokes a country that cannot feed itself.
 */
export function willLost(g: Game, nat: string): number {
  // (All-out war wears it too, with the nation's footing switched on: a count of its own above what shortage takes, ruling 172.)
  // (And the bombing of its cities, with the air campaigns on: a small count of its own, ruling 236.2, b549.)
  // (And atomic strikes on its cities, with the atomic project on: atom.ts, the design director's ruling of 25 November.)
  return hungerLost(g, nat) + footWorn(g, nat) + bombWorn(g, nat) + atomWorn(g, nat);
}

/** What privation alone has taken from a nation's will to fight: what shortage takes, up to its own ceiling. */
export function hungerLost(g: Game, nat: string): number {
  if (g.flags.open !== 1) return 0;
  const h = g.nations[nat]?.res?.hunger ?? 0;
  return h > 0 ? Math.min(RES.will.most, h * RES.will.per) : 0;
}

// ---- What a shortage does ---------------------------------------------------------------------------------

const metOf = (g: Game, nat: string, good: Good): number => {
  if (g.flags.open !== 1) return 1;
  const r = g.nations[nat]?.res;
  return r ? (good === 'oil' ? r.oilMet : r.supMet) : 1;
};

/** The share of its fuel need a nation's forces have this month, and of its supplies. 1 when nothing is short. */
export const fuelMet = (g: Game, nat: string): number => metOf(g, nat, 'oil');
export const suppliesMet = (g: Game, nat: string): number => metOf(g, nat, 'sup');

const runsOnFuel = (kind: Kind): boolean => kind === 'arm' || kind === 'mot';

/** What a shortage leaves of a corps' attack: fuel for tanks and lorries, and supplies for everyone. */
export function shortAttack(g: Game, c: Corps): number {
  const sup = 1 - RES.sup.short.fight * (1 - suppliesMet(g, c.nat));
  return runsOnFuel(c.kind) ? sup * (1 - RES.oil.short.attack * (1 - fuelMet(g, c.nat))) : sup;
}

export function shortDefence(g: Game, c: Corps): number {
  const sup = 1 - RES.sup.short.fight * (1 - suppliesMet(g, c.nat));
  return runsOnFuel(c.kind) ? sup * (1 - RES.oil.short.defence * (1 - fuelMet(g, c.nat))) : sup;
}

/** True when tanks and lorries are so short of fuel that they lose a move this month. */
export const fuelSlows = (g: Game, c: Corps): boolean => runsOnFuel(c.kind) && KIND[c.kind].mobile && fuelMet(g, c.nat) < RES.oil.short.slowAt;

/** What a fuel shortage leaves of an air force's worth, and of a fleet's fitness for sea. */
export const fuelAir = (g: Game, nat: string): number => 1 - RES.oil.short.air * (1 - fuelMet(g, nat));
export const fuelFleet = (g: Game, nat: string): number => 1 - RES.oil.short.fleet * (1 - fuelMet(g, nat));

/** What a shortage of supplies leaves of the monthly drafts, and of the treasury's income. */
export const supDrafts = (g: Game, nat: string): number => 1 - RES.sup.short.drafts * (1 - suppliesMet(g, nat));
export const supIncome = (g: Game, nat: string): number => 1 - RES.sup.short.income * (1 - suppliesMet(g, nat));

// ---- For the computer's staffs ------------------------------------------------------------------------------

/** Months of oil a nation has in hand at the rate it is going: Infinity when it is not losing any, or keeps no stock. */
export function oilMonths(g: Game, nat: string): number {
  const v = stores(g, nat);
  if (!v) return Infinity;
  if (v.oil.met < 0.995) return 0;
  return v.oil.months;
}

/** True when a staff should raise no more tanks for want of fuel to run them. */
export const shortOfOil = (g: Game, nat: string): boolean => oilMonths(g, nat) < RES.ai.armourAt;

/** A word for how a nation stands, for those who can only judge it from outside. */
export function standing(b: Book): 'ample' | 'steady' | 'tightening' | 'short' | 'dry' {
  if (b.met < 0.6) return 'dry';
  if (b.met < 0.995) return 'short';
  if (b.months <= 6) return 'tightening';
  return b.stock >= b.cap * 0.6 ? 'ample' : 'steady';
}
