// An air group sent to any airfield of its side's or its allies' with one order (release 2; Brian 24 November 2026
// 22:08Z: "i want you to be able to move any air unit to any airfield you want that is yours or an allies but it tells
// you how many turns it will take and if there is risk of being intercepted"; the design director's rulings of 21:49Z
// and 22:10Z). The grand campaign's air war only, behind the switch g.flags.ferry (stage.json 'ferry', TC_FERRY=1).
// Off, nothing here is read or written and no die is drawn: every caller asks ferryOn first, and `way`, the one field
// this writes on a group, is never set.
//
// The search is a chain of friendly airfields, each hop no longer than the group's ferry range, measured on the globe
// between the fields themselves (data/campaign/ferry.json: the chart stretches the far north). No route is written
// anywhere: a chain is whatever fields stand where. Where no chain reaches across an ocean the group goes crated by sea,
// by the rule that was already there (air.ts shipAir). Each month a group on its way flies a few hops, held by a closed
// sky (weather, ruling 127); it is out of the fight while it flies (airversion.ts wrecked), and on a hop any enemy
// fighter whose reach covers that leg comes up against it and the air battle is fought as any other (air.ts fly).

import ferryData from '../../data/campaign/ferry.json';
import type { AirFleet, Game, Region } from './types';
import {
  AIR, airRange, airWar, airfieldsHold, assessAir, carrierOf, cover, fieldRoom, fly, groupAt, isOffensive, legKm, legKmAt, overOcean, shipAir,
  whyNotMission, whyNotShipAir, whyNotStop, SHIP_AIR_ONE, SHIP_AIR_ROOM,
} from './air';
import { allied, atWar, headline } from './game';
import { SIGHT, sightOf } from './intel';
import { fleetKnown } from './carriers';
import { fighterTypesOn, typeMix } from './fighters';
import { grounded, skyAt, weatherOn } from './weather';
import { wrecked } from './airversion';
import { fieldRules, fieldShut } from './fieldstate';
import { NAVY, crossingRisk, seaPath } from './navy';
import { Rng, hashString } from '../core/rng';

export const FERRY = ferryData as unknown as {
  range: { ftr: number; bmr: number; types: Record<string, number> };
  perMonth: number;
  safer: number;
  losses: { ftr: number; bmr: number };
  fields: Record<string, [number, number] | string>;
};

/** True where air groups can be sent to any friendly airfield by stages: the grand campaign's air war, with the switch on. */
export const ferryOn = (g: Game): boolean => g.flags.ferry === 1 && airWar(g);

/** Turns the switch on or off (a new grand campaign, or a test). */
export function setFerry(g: Game, on: boolean): void {
  if (on) g.flags.ferry = 1;
  else delete g.flags.ferry;
}

// ---- Where the fields are, on the globe --------------------------------------------------------------------------

// The chart's own projection (tools/build-geo-campaign.mjs, build-geo-world.mjs and build-geo-far.mjs all use it):
// x = (lon + 11) x cos 50 x 40, y = (71.4 - lat) x 40. Every province stands at its seed point, so its latitude and
// longitude come back exactly from where it is drawn.
const SCALE = 40, KX = Math.cos((50 * Math.PI) / 180) * SCALE, NORTH = 71.4, WEST = -11, EARTH = 6371;
const place = new WeakMap<Game, { n: number; lat: Float64Array; lon: Float64Array; cos: Float64Array }>();

function globe(g: Game): { lat: Float64Array; lon: Float64Array; cos: Float64Array } {
  const had = place.get(g);
  if (had && had.n === g.regions.length) return had;
  const n = g.regions.length, lat = new Float64Array(n), lon = new Float64Array(n), cos = new Float64Array(n);
  for (const r of g.regions) {
    const at = FERRY.fields[r.name];
    const [la, lo] = Array.isArray(at) ? at : [NORTH - r.y / SCALE, r.x / KX + WEST];
    lat[r.id] = (la * Math.PI) / 180; lon[r.id] = (lo * Math.PI) / 180; cos[r.id] = Math.cos(lat[r.id]);
  }
  const out = { n, lat, lon, cos };
  place.set(g, out);
  return out;
}

/** How far apart two provinces' airfields are on the globe, in km (great circle). */
export function realKm(g: Game, a: number, b: number): number {
  const P = globe(g);
  const h = Math.sin((P.lat[b] - P.lat[a]) / 2) ** 2 + P.cos[a] * P.cos[b] * Math.sin((P.lon[b] - P.lon[a]) / 2) ** 2;
  return 2 * EARTH * Math.asin(Math.min(1, Math.sqrt(h)));
}

/**
 * The longest hop a group can fly in one go: its bombers' ferry range, or its fighters' (a group of both flies at the
 * shorter). With Fighter types on, the shortest-legged type in its mix sets its fighters' range (ferry.json 'types').
 */
export function ferryRange(g: Game, f: AirFleet): number {
  const R = FERRY.range;
  let ftr = R.ftr;
  if (fighterTypesOn(g) && f.ftr > 0) {
    const mix = typeMix(g, f);
    if (mix) { const ks = Object.entries(mix).filter(([, s]) => s > 0).map(([k]) => R.types[k] ?? R.ftr); if (ks.length) ftr = Math.min(...ks); }
  }
  const kind = f.kind ?? (f.ftr > 0 && f.bmr > 0 ? 'both' : f.bmr > 0 ? 'bmr' : 'ftr');
  return kind === 'bmr' ? R.bmr : kind === 'ftr' ? ftr : Math.min(ftr, R.bmr);
}

// ---- The search ---------------------------------------------------------------------------------------------------

export interface FerryHop { from: number; to: number; km: number; risk: boolean }
export interface FerryRoute {
  to: number;
  name: string;
  /** 'air' a chain of fields, 'sea' crated by sea (no chain across the ocean), 'none' neither (why says why). */
  how: 'air' | 'sea' | 'none';
  hops: FerryHop[];
  /** Months to get there, this one counted: 1 is this month. 0 where it cannot go. */
  turns: number;
  /** The hops (0 first) within reach of enemy fighters the mover's side knows of. */
  risky: number[];
  /** Where the quickest chain had a known risk and a chain with none was found: its turns, the quickest's, and whether the order takes it. */
  safer?: { turns: number; quick: number; taken: boolean };
  /** The way by sea: the danger to the ships on the way (as for troopships), and why it waits, where it does. */
  sea?: { risk: number | null; wait: string | null };
  why: string | null;
  /** The line for the page, in plain words. */
  words: string;
}

const ORD = ['first', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth'];
const ordinal = (i: number): string => ORD[i] ?? `${i + 1}th`;
const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const turnsWord = (n: number): string => `${n} turn${n === 1 ? '' : 's'}`;

/** The enemy fighters the viewer's side knows of: groups on a field it sees whole, and carrier wings of a fleet it has found. Where each is, and how far its fighters reach. */
function knownFighters(g: Game, f: AirFleet, viewer: string): { x: number; y: number; region: number; sea: number; reach: number }[] {
  const sight = sightOf(g, viewer);
  const out: { x: number; y: number; region: number; sea: number; reach: number }[] = [];
  for (const e of g.airFleets) {
    if (e.ftr <= 0 || !atWar(g, e.nat, f.nat) || e.way !== undefined) continue;
    if (e.ship !== undefined) { const c = carrierOf(g, e); if (!c || fleetKnown(g, c, viewer) === 'none') continue; }
    else if (e.base < 0 || sight[e.base] !== SIGHT.full) continue;
    const p = groupAt(g, e);
    out.push({ x: p.x, y: p.y, region: p.region, sea: p.sea, reach: airRange(e, g).ftr });
  }
  return out;
}

/**
 * Distance in km from a place to the leg between two fields: air.ts legKmAt, the one ruler the battle that follows reads
 * (legKm), on the globe wherever the far side is in it (the join's read of r2-head-1125k, fault 1).
 */
export function pointLeg(g: Game, p: { x: number; y: number; region: number; sea: number }, from: number, to: number): number {
  return legKmAt(g, p, from, to);
}

/** Hops this group can still fly this month: none if it has flown a raid or made another move, all of them if it has not started. */
function hopsLeft(g: Game, f: AirFleet): number {
  if (f.flew === g.turn) return 0;
  const at = f.way?.at;
  if (at && at[0] === g.turn) return Math.max(0, FERRY.perMonth - at[1]);
  return f.moved === g.turn ? 0 : FERRY.perMonth;
}

const turnsFor = (g: Game, f: AirFleet, hops: number): number => {
  const now = hopsLeft(g, f);
  return hops <= now ? 1 : 1 + Math.ceil((hops - now) / FERRY.perMonth);
};

/** The first hop for a leg (0-based) by its ordinal, in a sentence: "the second hop", "the second and fourth hops". */
const hopsWord = (ix: number[]): string => `the ${list(ix.map(ordinal))} hop${ix.length === 1 ? '' : 's'}`;

/**
 * Every field a land group could be sent to from where it is, each with its way there (the search, ruling of 22:10Z):
 * the chain with the fewest hops, and among those the fewest within reach of enemy fighters the viewer's side knows of,
 * then the shortest; a chain with no such hop is taken instead where it costs at most 'safer' more turns. Where no chain
 * reaches a field across an ocean, the way by sea (air.ts shipAir); where nothing reaches it, why not. Deterministic:
 * no die, and fields in the order of their ids.
 */
export function ferryRoutes(g: Game, f: AirFleet, viewer: string = f.nat): Map<number, FerryRoute> {
  const out = new Map<number, FerryRoute>();
  if (!ferryOn(g) || f.ship !== undefined || f.base < 0 || !g.regions[f.base]) return out;
  const R = ferryRange(g, f);
  // The fields it could put down on: every airfield the friendly rule opens to it (whyNotStop), and its own.
  const nodes: number[] = [];
  for (const r of g.regions) if (r.id === f.base || (airfieldsHold(g, r.id) > 0 && whyNotStop(g, f, r) === null)) nodes.push(r.id);
  const N = nodes.length, start = nodes.indexOf(f.base);
  const foes = knownFighters(g, f, viewer);
  const nodeRisk = new Uint8Array(N);
  for (let i = 0; i < N; i++) for (const e of foes) if (pointLeg(g, e, nodes[i], nodes[i]) <= e.reach) { nodeRisk[i] = 1; break; }
  const legRisk = (i: number, j: number): boolean => nodeRisk[j] === 1 || foes.some((e) => pointLeg(g, e, nodes[i], nodes[j]) <= e.reach);
  // The hops it can fly, worked out once: each pair within its ferry range.
  const near: number[][] = nodes.map(() => []);
  const legKmOf = new Map<number, number>();
  for (let i = 0; i < N; i++) for (let j = i + 1; j < N; j++) {
    const d = realKm(g, nodes[i], nodes[j]);
    if (d > R) continue;
    near[i].push(j); near[j].push(i);
    legKmOf.set(i * N + j, d); legKmOf.set(j * N + i, d);
  }
  const risky = new Map<number, boolean>();
  const riskOf = (i: number, j: number): boolean => { const k = i * N + j; let v = risky.get(k); if (v === undefined) { v = legRisk(i, j); risky.set(k, v); } return v; };
  // One search over (hops, hops at risk, km), the other over hops with no known risk alone.
  const search = (safeOnly: boolean): { hops: Int32Array; prev: Int32Array } => {
    const hops = new Int32Array(N).fill(-1), bad = new Int32Array(N), dist = new Float64Array(N).fill(Infinity), prev = new Int32Array(N).fill(-1), done = new Uint8Array(N);
    hops[start] = 0; dist[start] = 0;
    const better = (h: number, b: number, d: number, j: number): boolean => hops[j] < 0 || h < hops[j] || (h === hops[j] && (b < bad[j] || (b === bad[j] && d < dist[j] - 1e-9)));
    for (;;) {
      let a = -1;
      for (let i = 0; i < N; i++) if (!done[i] && hops[i] >= 0 && (a < 0 || hops[i] < hops[a] || (hops[i] === hops[a] && (bad[i] < bad[a] || (bad[i] === bad[a] && dist[i] < dist[a]))))) a = i;
      if (a < 0) break;
      done[a] = 1;
      for (const j of near[a]) {
        if (done[j]) continue;
        const r = riskOf(a, j);
        if (safeOnly && r) continue;
        const h = hops[a] + 1, b = bad[a] + (r ? 1 : 0), d = dist[a] + legKmOf.get(a * N + j)!;
        if (better(h, b, d, j)) { hops[j] = h; bad[j] = b; dist[j] = d; prev[j] = a; }
      }
    }
    return { hops, prev };
  };
  const quick = search(false);
  const safe = foes.length ? search(true) : quick;
  const chain = (s: { prev: Int32Array }, j: number): FerryHop[] => {
    const path: number[] = [];
    for (let k = j; k >= 0; k = s.prev[k]) path.unshift(k);
    return path.slice(1).map((k, i) => ({ from: nodes[path[i]], to: nodes[k], km: legKmOf.get(path[i] * N + k)!, risk: riskOf(path[i], k) }));
  };
  const reached = (s: { hops: Int32Array }): number[] => nodes.filter((_, i) => s.hops[i] > 0 || i === start);
  const reach = reached(quick);
  for (const r of g.regions) {
    if (r.id === f.base || airfieldsHold(g, r.id) <= 0) continue;
    // Only fields of its own side or an ally's are offered (the friendly rule, as for any move).
    if (!allied(g, f.nat, r.ctrl)) continue;
    const j = nodes.indexOf(r.id);
    out.set(r.id, routeTo(g, f, r, j < 0 ? null : quick.hops[j] > 0 ? chain(quick, j) : null, j < 0 || safe === quick || safe.hops[j] <= 0 ? null : chain(safe, j), reach, R));
  }
  return out;
}

/** The way to one field: see ferryRoutes. */
export function ferryRoute(g: Game, f: AirFleet, to: number, viewer: string = f.nat): FerryRoute | null {
  return ferryRoutes(g, f, viewer).get(to) ?? null;
}

function routeTo(g: Game, f: AirFleet, r: Region, quick: FerryHop[] | null, safe: FerryHop[] | null, reach: number[], R: number): FerryRoute {
  const base: FerryRoute = { to: r.id, name: r.name, how: 'none', hops: [], turns: 0, risky: [], why: null, words: '' };
  const stopWhy = whyNotStop(g, f, r);
  if (quick) {
    const riskyOf = (hs: FerryHop[]): number[] => hs.map((h, i) => (h.risk ? i : -1)).filter((i) => i >= 0);
    let hops = quick, risky = riskyOf(quick), turns = turnsFor(g, f, quick.length);
    let safer: FerryRoute['safer'];
    if (risky.length && safe) {
      const st = turnsFor(g, f, safe.length);
      safer = { turns: st, quick: turns, taken: st <= turns + FERRY.safer };
      if (safer.taken) { hops = safe; risky = []; turns = st; }
    }
    const by = hops.slice(0, -1).map((h) => g.regions[h.to].name);
    const way = `${turnsWord(turns)} ${by.length ? `by ${list(by)}` : 'in one hop'}`;
    const risk = risky.length ? `Risk: enemy fighters seen within reach of ${hopsWord(risky)}.` : 'No enemy fighters known on the way.';
    const note = !safer ? '' : safer.taken
      ? ` It goes the safer way${safer.turns > safer.quick ? `, ${turnsWord(safer.turns - safer.quick).replace(/^1 turn/, 'one turn')} longer than the quickest` : ''}: the quickest passes within reach of enemy fighters seen.`
      : ` A way with no enemy fighters known on it takes ${turnsWord(safer.turns)}.`;
    return { ...base, how: 'air', hops, turns, risky, ...(safer ? { safer } : {}), words: `${way}. ${risk}${note}` };
  }
  // No chain: across an ocean it goes crated by sea, by the rule there was before (one group of a nation a month).
  const gap = shortWhy(g, f, r, reach, R);
  if (overOcean(g, f, r) && !stopWhy) {
    const free = g.flags[`fy:air:${f.nat}`] !== g.turn && f.moved !== g.turn && f.flew !== g.turn;
    const wait = !fieldRoom(g, r.id, f) ? SHIP_AIR_ROOM : null;
    const path = seaPath(g, f.nat, g.regions[f.base], r);
    const risk = path ? crossingRisk(g, f.nat, path) * NAVY.intercept.loss : null;
    const turns = free ? 1 : 2;
    const danger = risk === null ? 'The danger at sea is not known: its field has no harbour.' : Math.round(risk * 100) >= 1 ? `Danger to the ships on the way: about ${Math.round(risk * 100)}%.` : 'No danger at sea known on the way.';
    return { ...base, how: 'sea', turns, sea: { risk, wait }, words: `${turnsWord(turns)}: no chain of fields in range, so it goes crated by sea. ${danger}${wait ? ` ${wait} It waits until there is room.` : ''} ${gap}` };
  }
  const why = stopWhy ?? `No chain of your side's airfields reaches ${r.name}. ${gap}`;
  return { ...base, why, words: why };
}

/** Why a field is beyond its chains, in plain words: its range, and the gap from the nearest field it can reach. */
function shortWhy(g: Game, f: AirFleet, r: Region, reach: number[], R: number): string {
  let best = -1, bd = Infinity;
  for (const id of reach) { const d = realKm(g, id, r.id); if (d < bd) { bd = d; best = id; } }
  const what = f.kind === 'bmr' ? 'Its bombers fly' : f.kind === 'ftr' ? 'Its fighters fly' : 'It flies';
  return best < 0 ? `${what} at most ${num(R)} km in one hop.`
    : `${what} at most ${num(R)} km in one hop, and from ${g.regions[best].name}, the nearest field it can reach, it is ${num(bd)} km.`;
}

// ---- The order, and the month's hops ------------------------------------------------------------------------------

/**
 * Sends a land group to a friendly field by the way the search finds (the order warns and never refuses: where enemy
 * fighters are known on the way it still goes). False only where there is no way at all. It sets off at once, as far as
 * this month's hops take it.
 */
export function orderFerry(g: Game, f: AirFleet, to: number): FerryRoute | null {
  if (!ferryOn(g) || f.ship !== undefined || to === f.base) return null;
  const route = ferryRoute(g, f, to);
  if (!route || route.how === 'none') return null;
  const was = f.way?.at;
  f.way = { to, stops: route.how === 'sea' ? [to] : route.hops.map((h) => h.to), ...(route.how === 'sea' ? { sea: true } : {}), ...(was ? { at: was } : {}) };
  goOn(g, f);
  return route;
}

/** Calls a group's journey off: it stays at the field it has reached. */
export function cancelFerry(g: Game, f: AirFleet): boolean {
  if (f.way === undefined) return false;
  delete f.way;
  return true;
}

/** Each of a nation's groups on its way flies its hops for the month (as its month opens). Nothing with the switch off. */
export function ferryMonth(g: Game, nat: string): void {
  if (!ferryOn(g)) return;
  for (const f of g.airFleets.filter((x) => x.nat === nat && x.way !== undefined)) goOn(g, f);
}

/** The words of a group on its way, for its card: "On its way to Iceland, 2 hops to go." */
export function wayWords(g: Game, f: AirFleet): string | null {
  if (f.way === undefined) return null;
  const to = g.regions[f.way.to]?.name ?? 'another airfield';
  if (f.way.sea) return `On its way to ${to}, crated by sea`;
  const n = f.way.stops.length;
  return `On its way to ${to}, ${n} hop${n === 1 ? '' : 's'} to go`;
}

function arrive(g: Game, f: AirFleet): void {
  const to = g.regions[f.base];
  delete f.way;
  // Settling in costs it what a move does, once at the end (no other cost on the way: the ruling of 22:10Z).
  f.ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  if (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target)) { f.mission = 'support'; f.target = -1; }
  headline(g, 'event', `${f.name} reaches ${to.name}.`, f.nat, to.id);
}

/** Finds the way on again from where the group is, when the next field can no longer take it. False where there is none. */
function replan(g: Game, f: AirFleet): boolean {
  const to = f.way!.to;
  const route = ferryRoute(g, f, to);
  if (!route || route.how === 'none') {
    headline(g, 'event', `${f.name} cannot go on to ${g.regions[to].name}: ${route?.why ?? 'there is no way there now'} It stays at ${g.regions[f.base].name}.`, f.nat, f.base);
    delete f.way;
    return false;
  }
  f.way!.stops = route.how === 'sea' ? [to] : route.hops.map((h) => h.to);
  if (route.how === 'sea') f.way!.sea = true; else delete f.way!.sea;
  return true;
}

function goOn(g: Game, f: AirFleet): void {
  const w = f.way;
  if (!w) return;
  if (f.ftr + f.bmr <= 0 || f.base < 0 || !g.regions[f.base]) { delete f.way; return; }
  if (f.base === w.to) { arrive(g, f); return; }
  let left = hopsLeft(g, f);
  const done = (): number => (w.at && w.at[0] === g.turn ? w.at[1] : 0);
  while (left > 0 && f.way) {
    if (w.sea) {
      // Crated by sea, by the rule there was before: one group of a nation a month, to a field with room for all of it.
      const dest = g.regions[w.to];
      if (!overOcean(g, f, dest)) { if (!replan(g, f)) return; continue; }
      // (Asked as of a group not on its way, since a group on its way is let move by nothing else.)
      delete f.way;
      const why = whyNotShipAir(g, f, dest);
      f.way = w;
      if (why === SHIP_AIR_ONE || why === SHIP_AIR_ROOM) return;
      if (why) { if (!replan(g, f) || f.way?.sea) return; continue; }
      delete f.way;
      if (shipAir(g, f, dest)) { headline(g, 'event', `${f.name} reaches ${dest.name}, crated by sea.`, f.nat, dest.id); return; }
      f.way = w;
      return;
    }
    const next = w.stops[0];
    const r = g.regions[next];
    if (!r || whyNotStop(g, f, r) || realKm(g, f.base, next) > ferryRange(g, f)) { if (!replan(g, f)) return; continue; }
    // A cratered runway lets nobody out (airfields as targets, the same test as today's move, air.ts whyNotBase): the
    // group waits where it is until its crews have the field open again.
    if (g.flags.fields === 1 && fieldRules(g) && fieldShut(g, f.base)) {
      headline(g, 'event', `${f.name} is held at ${g.regions[f.base].name}: the runway is cratered until it is repaired.`, f.nat, f.base);
      w.at = [g.turn, done()];
      return;
    }
    // A closed sky over either end holds the hop for the month (weather, ruling 127).
    if (weatherOn(g) && (skyAt(g, f.base) === 'closed' || skyAt(g, next) === 'closed')) {
      headline(g, 'event', `${f.name} is held at ${g.regions[f.base].name}: the sky is closed on the way to ${r.name}.`, f.nat, f.base);
      w.at = [g.turn, done()];
      return;
    }
    const from = f.base;
    // Any enemy fighter whose reach covers this leg comes up, as against a raid on troops; the battle is fought as any other.
    const met = assessAir(g, [f], 'superiority', next, undefined, { from, to: w.to });
    const caught = !met.why && met.def.length > 0;
    if (caught) fly(g, f.nat, 'superiority', next, [f], `:ferry:${f.id}:${done()}`, undefined, { from, to: w.to });
    if (f.ftr + f.bmr <= 0) { delete f.way; return; }
    forcedDown(g, f, from, next);
    f.base = next;
    f.moved = g.turn;
    w.stops.shift();
    w.at = [g.turn, done() + 1];
    left--;
    if (f.base === w.to || !w.stops.length) { arrive(g, f); return; }
    // A group that had to fight for it lands and goes on next month.
    if (caught) return;
  }
}

/** Aircraft forced down on a hop, by cause (ferry.json 'losses': unknown, 0, and then nothing is rolled). */
function forcedDown(g: Game, f: AirFleet, from: number, to: number): void {
  const L = FERRY.losses;
  if (!(L.ftr > 0) && !(L.bmr > 0)) return;
  const rng = new Rng(hashString(`ferry:${g.seed}:${g.turn}:${f.id}:${from}:${to}`));
  const kf = L.ftr > 0 ? Math.min(f.ftr, rng.roll(f.ftr * L.ftr)) : 0, kb = L.bmr > 0 ? Math.min(f.bmr, rng.roll(f.bmr * L.bmr)) : 0;
  f.ftr -= kf; f.bmr -= kb; f.lost.ftr += kf; f.lost.bmr += kb;
}

// ---- The staffs ---------------------------------------------------------------------------------------------------

/**
 * A computer staff's spare group sent to a field that asks for it, beyond a month's flight or across an ocean (air.ts
 * baseByThreat, behind the same switch): the same search and warning as the player's order. False where there is no way.
 */
export function staffFerry(g: Game, f: AirFleet, to: Region): boolean {
  if (!ferryOn(g) || f.way !== undefined) return false;
  return orderFerry(g, f, to.id) !== null;
}

/** Enemy fighters that would come up on a leg now, by the battle's own rule (for tests and the page's after-the-fact words). */
export function legCovered(g: Game, f: AirFleet, from: number, to: number): boolean {
  const wx = weatherOn(g);
  return g.airFleets.some((e) => e.ftr > 0 && e.mission === 'support' && !e.close && atWar(g, e.nat, f.nat) && !((wx && grounded(g, e)) || wrecked(g, e)) && cover(legKm(g, e, from, to), airRange(e, g).ftr) > 0);
}
