// Carriers and the air war at sea (the grand campaign).
//
// A carrier is a floating airfield. Every fleet with carriers has two wings, fighters and strike aircraft, which
// are air groups like any other: they are planned from the same raid planner and counted in the same Battle
// Ticket, but they have no airfield. They go where the fleet goes, they are made up with new aircraft only while
// it lies in harbour, and when a carrier goes down most of what was aboard goes with it.
//
// Aircraft can be aimed at one fleet at sea, or at the ships lying in a harbour (air.ts flies both). Carrier
// fighters left on patrol meet any raid on the ships in their sea, and ships' own guns bring bombers down.
//
// And a fleet has to be found before it can be struck. A nation's search of a sea is the ships its side has
// there, its carriers in the seas next to it, its aircraft on the coasts, and any search its aircraft have flown.
// The player sees on the map only what his side has found; the computer's staffs strike only what theirs have.
//
// Every number is in data/campaign/carriers.json. None of this touches the history campaigns.

import { byHand } from './depth';
import { Rng, hashString } from '../core/rng';
import {
  AIR, AIRWAR, CARRIERS, afloat, airFleetsOf, airPool, airRange, airWar, assessAir, carrierOf, carrierRules, fieldRoom, fly, forecastAir, homeOf, isOffensive,
  kmToSea, shipsInPort, whyNotBase, whyNotMission,
} from './air';
import { allied, atWar, headline } from './game';
import { INTEL, agentsOf, atWork, fogOn } from './intel';
import { CLASSES, FLEETS, NAVY, airOver, atSea, fleetPower, fleetReach, fleetsOf, keptByHand, sail, seaSide, shipCount, shipList, surfaceShips, zoneOf } from './navy';
import { RES, needOf } from './resources';
import type { AirFleet, AirMission, Fleet, Game, Region, Ships } from './types';
import { weatherOn, whyNotSky, zoneOfSea } from './weather';
import { inCampaign } from './aircampaigns';

type WingKind = 'ftr' | 'bmr';
const KINDS: WingKind[] = ['ftr', 'bmr'];
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const plural = (n: number, one: string, many = `${one}s`): string => `${num(n)} ${Math.round(n) === 1 ? one : many}`;

// ---- The wings ------------------------------------------------------------------------------------------

/** The aircraft one carrier of this navy brings. */
export const wingSize = (nat: string): { ftr: number; bmr: number } => CARRIERS.wing[nat] ?? CARRIERS.wing._default;

/** The wings that fly from one fleet's carriers: fighters first. */
export const wingsOf = (g: Game, fleet: number): AirFleet[] => g.airFleets.filter((f) => f.ship === fleet).sort((a, b) => (a.kind === 'ftr' ? 0 : 1) - (b.kind === 'ftr' ? 0 : 1));
const wingOf = (g: Game, fleet: number, kind: WingKind): AirFleet | undefined => g.airFleets.find((f) => f.ship === fleet && f.kind === kind);
const wingName = (c: Fleet, kind: WingKind): string => CARRIERS.names[kind].replace('{fleet}', c.name);
/** A wing has no airfield, but everything that names a group's place needs a region: its fleet's port, or its capital for a base off the map. */
const portOf = (g: Game, c: Fleet): number => (c.base >= 0 ? c.base : Math.max(0, g.nations[c.nat]?.capital ?? 0));

function newWing(g: Game, c: Fleet, kind: WingKind): AirFleet {
  const f: AirFleet = {
    id: g.nextAir++, nat: c.nat, name: wingName(c, kind), base: portOf(g, c), ftr: 0, bmr: 0, est: { ftr: 0, bmr: 0 },
    mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 }, kind, ship: c.id,
  };
  g.airFleets.push(f);
  return f;
}

/** Strikes aircraft off a wing as lost with their ship. */
function loseAboard(g: Game, w: AirFleet, kind: WingKind, n: number): void {
  if (n <= 0) return;
  const p = airPool(g, w.nat);
  w[kind] -= n;
  w.lost[kind] += n;
  if (kind === 'ftr') { p.lostFtr += n; p.lostPilots += n; } else p.lostBmr += n;
}

/**
 * The grand campaign gives its carriers their wings: every fleet with carriers gets fighters and strike aircraft
 * at full strength. Done once, after the air war itself has opened; a grand campaign saved before there were
 * wings is given them when it is loaded.
 */
export function openCarriers(g: Game): void {
  if (g.flags.carriers === 1 || !airWar(g)) return;
  g.flags.carriers = 1;
  // The depots were filled for the forces as they stood: what the new aircraft need is laid in beside it.
  const navies = [...new Set(g.fleets.filter((c) => c.ships.cv > 0).map((c) => c.nat))];
  const need = (nat: string): [number, number] => [needOf(g, nat, 'oil', true), needOf(g, nat, 'sup', true)];
  const before = new Map(navies.map((nat) => [nat, need(nat)]));
  for (const c of g.fleets) {
    if (c.ships.cv <= 0) continue;
    const size = wingSize(c.nat);
    for (const kind of KINDS) {
      if (size[kind] <= 0) continue;
      const w = newWing(g, c, kind);
      w.est[kind] = w[kind] = c.ships.cv * size[kind];
    }
  }
  for (const nat of navies) {
    const st = g.nations[nat]?.res;
    if (!st) continue;
    const was = before.get(nat)!, now = need(nat);
    st.oil += (RES.oil.start[nat] ?? RES.oil.start._default) * (now[0] - was[0]);
    st.sup += (RES.sup.start[nat] ?? RES.sup.start._default) * (now[1] - was[1]);
  }
}

/**
 * Brings every wing into line with the carriers it flies from. A new carrier joins with its aircraft aboard.
 * A carrier sunk takes the aircraft aboard with it, but for those that land on another deck or ashore. A fleet
 * with no carrier left, or a fleet that is gone, has no wing. Called wherever carriers may have been built, sunk
 * or moved (moveWings() has already carried the aircraft across where carriers changed fleets).
 */
export function syncWings(g: Game): void {
  if (!carrierRules(g)) return;
  const saved = CARRIERS.sunk.saved;
  for (const w of g.airFleets.filter(afloat)) {
    const c = carrierOf(g, w);
    if (c && c.ships.cv > 0 && g.nations[c.nat]?.alive) continue;
    // Nothing left to fly from: what can reach the shore goes back to the reserve.
    const kind: WingKind = w.kind === 'bmr' ? 'bmr' : 'ftr';
    const home = Math.round(w[kind] * saved);
    loseAboard(g, w, kind, w[kind] - home);
    const p = airPool(g, w.nat);
    if (kind === 'ftr') { p.ftr += home; p.pilots += home; } else p.bmr += home;
    g.airFleets.splice(g.airFleets.indexOf(w), 1);
  }
  for (const c of g.fleets) {
    if (c.ships.cv <= 0 || !g.nations[c.nat]?.alive) { if (c.bare) delete c.bare; continue; }
    const size = wingSize(c.nat);
    for (const kind of KINDS) {
      // Deck room whose wing was ordered ashore stands empty: no new wing is raised for it (ruling 175(b)).
      const full = c.ships.cv * size[kind];
      if (c.bare && c.bare[kind] > full) c.bare[kind] = full;
      const est = full - bareOf(c, kind);
      let w = wingOf(g, c.id, kind);
      if (!w) { if (est > 0) { w = newWing(g, c, kind); w.est[kind] = w[kind] = est; } continue; }
      const was = w.est[kind];
      if (est > was) w[kind] += est - was;
      if (est < was && was > 0) {
        const aboard = w[kind] * (1 - est / was);
        loseAboard(g, w, kind, Math.min(w[kind], Math.round(aboard * (1 - saved))));
        // What is over for the decks that are left flies ashore to the reserve.
        const over = Math.max(0, w[kind] - est);
        if (over > 0) { const p = airPool(g, w.nat); w[kind] -= over; if (kind === 'ftr') { p.ftr += over; p.pilots += over; } else p.bmr += over; }
      }
      w.est[kind] = est;
      w.base = portOf(g, c);
      w.name = wingName(c, kind);
    }
    trimBare(c);
  }
}

/**
 * Carriers have changed fleets (a squadron split off, or one fleet joined to another): their aircraft go with
 * them. `had` is the carriers the first fleet had before, and `carriers` how many of them went. Call it once the
 * ships themselves have moved.
 */
export function moveWings(g: Game, from: Fleet, to: Fleet, carriers: number, had: number): void {
  if (!carrierRules(g) || carriers <= 0 || had <= 0) return;
  const size = wingSize(from.nat);
  for (const kind of KINDS) {
    // Decks whose wing is ashore stay with the first fleet as far as it keeps carriers to hold them: the carriers
    // sent are those with aircraft aboard first. With no wing ashore the share is the carriers that went, as ever.
    const bare = bareOf(from, kind);
    const decks = size[kind] > 0 ? bare / size[kind] : 0;
    const goBare = Math.max(0, decks - Math.max(0, had - carriers));
    if (goBare > 0) {
      const n = Math.min(bare, Math.round(goBare * size[kind]));
      from.bare![kind] -= n;
      (to.bare ??= { ftr: 0, bmr: 0 })[kind] += n;
    }
    const loaded = had - decks;
    const share = carriers >= had || loaded <= 0 ? 1 : Math.min(1, Math.max(0, carriers - goBare) / loaded);
    const a = wingOf(g, from.id, kind);
    if (!a) continue;
    const n = share >= 1 ? a[kind] : Math.round(a[kind] * share);
    const b = wingOf(g, to.id, kind) ?? newWing(g, to, kind);
    const both = b[kind] + n;
    b.ready = both > 0 ? (b.ready * b[kind] + a.ready * n) / both : a.ready;
    b[kind] += n;
    a[kind] -= n;
    if (a.flew !== undefined && (b.flew ?? -1) < a.flew) b.flew = a.flew;
    if (a.scouted !== undefined && (b.scouted ?? -1) < a.scouted) b.scouted = a.scouted;
    b.est[kind] = Math.max(0, to.ships.cv * size[kind] - bareOf(to, kind));
    a.est[kind] = Math.max(0, from.ships.cv * size[kind] - bareOf(from, kind));
    if (share >= 1) g.airFleets.splice(g.airFleets.indexOf(a), 1);
  }
  trimBare(from);
  trimBare(to);
  // Every carrier went (one fleet joined another): its wings ashore are now the second fleet's.
  if (carriers >= had) for (const f of g.airFleets) if (f.ashore === from.id) f.ashore = to.id;
  syncWings(g);
}

/**
 * How full a fleet's decks are: 1 with every aircraft aboard, 0 with none. A fleet without wings counts as full,
 * but decks whose wing was ordered ashore count as the empty decks they are.
 */
export function wingShare(g: Game, c: Fleet): number {
  if (!carrierRules(g) || c.ships.cv <= 0) return 1;
  let have = 0, est = bareOf(c, 'ftr') + bareOf(c, 'bmr');
  for (const w of g.airFleets) if (w.ship === c.id) { have += w.ftr + w.bmr; est += w.est.ftr + w.est.bmr; }
  return est > 0 ? Math.min(1, have / est) : 1;
}

// ---- Wings ashore (ruling 175(b)) -------------------------------------------------------------------------
//
// A carrier's wing can be ordered ashore to a friendly airfield with room when its fleet lies in that field's
// harbour, or in the sea off it. Ashore it is an air group like any other, with its own aircraft, pilots and
// fitness, and a carrier wing's reach. The same order sends it back aboard a fleet of its nation that has deck
// room standing empty there. While a wing is ashore its carriers fight as empty carriers. The move costs what a
// move between airfields costs. It is the player's order only: no staff gives it.

const kindOfWing = (w: AirFleet): WingKind => (w.kind === 'bmr' ? 'bmr' : 'ftr');
/** What sending a wing ashore costs, said by the order before it is given. */
export const PRICE_ASHORE = 'While it is ashore its carriers fight as empty carriers. The move costs it some readiness for a month.';
/** Deck room of one kind standing empty in a fleet because its wing was ordered ashore, in aircraft. */
export const bareOf = (c: Fleet, kind: WingKind): number => c.bare?.[kind] ?? 0;
function trimBare(c: Fleet): void { if (c.bare && c.bare.ftr <= 0 && c.bare.bmr <= 0) delete c.bare; }
/** True where an airfield's province is where the fleet is: the harbour it lies in, or a coast of the sea it is in. */
const offShore = (g: Game, c: Fleet, region: number): boolean => (atSea(c) ? !!g.seas[c.zone]?.coast.includes(region) : c.base >= 0 && c.base === region);
/** The provinces a fleet's wing could reach the shore in from where the fleet is. */
export const shoresOf = (g: Game, c: Fleet): number[] => (atSea(c) ? g.seas[c.zone]?.coast ?? [] : c.base >= 0 ? [c.base] : []);

/** Why this wing cannot fly ashore to this airfield now, or null if it can. */
export function whyNotAshore(g: Game, w: AirFleet, r: Region): string | null {
  if (w.ashore !== undefined) return 'It is already ashore.';
  const c = carrierRules(g) ? carrierOf(g, w) : null;
  if (!c) return 'Only a carrier wing aboard its fleet can be sent ashore.';
  const kind = kindOfWing(w);
  if (w[kind] <= 0) return 'It has no aircraft to send ashore.';
  if (w.flew === g.turn) return 'It flew its strike this month: it is back aboard next month.';
  if (w.moved === g.turn) return 'It has already changed base this month.';
  if (!offShore(g, c, r.id)) {
    return atSea(c) ? `The ${c.name} is in the ${g.seas[c.zone].name}: its wing can fly ashore only to an airfield on that sea.`
      : c.base >= 0 ? `The ${c.name} lies at ${c.port}: from harbour its wing can fly ashore only to the airfield there.`
      : `The ${c.name} lies at ${c.port}, where there is no airfield for its wing to fly to.`;
  }
  // The airfield itself, as for any group that moves: friendly, built, not wrecked, not taken this month.
  const land = { ...w, ship: undefined, base: -1, moved: 0 } as AirFleet;
  const no = whyNotBase(g, land, r, true);
  if (no) return no;
  if (!fieldRoom(g, r.id, land)) return `The airfield at ${r.name} has no room for it.`;
  return null;
}

/** Why this wing cannot be sent ashore anywhere now, in one sentence, or null if there is somewhere it can go. */
export function whyNoShore(g: Game, w: AirFleet): string | null {
  const c = carrierRules(g) ? carrierOf(g, w) : null;
  if (!c) return 'Only a carrier wing aboard its fleet can be sent ashore.';
  const ids = shoresOf(g, c);
  let first: string | null = null;
  for (const id of ids) {
    const no = whyNotAshore(g, w, g.regions[id]);
    if (!no) return null;
    // A sentence about the wing itself (no aircraft, flown, moved) is the answer wherever it looks.
    if (/^It /.test(no)) return no;
    if (allied(g, w.nat, g.regions[id].ctrl)) first ??= no;
  }
  if (first) return first;
  return atSea(c) ? `No friendly airfield lies on the ${g.seas[c.zone].name}, where the ${c.name} is.`
    : c.base >= 0 ? whyNotAshore(g, w, g.regions[c.base]) : `The ${c.name} lies at ${c.port}, where there is no airfield for its wing to fly to.`;
}

/** Sends a wing ashore to an airfield. Returns false, and changes nothing, where whyNotAshore() says no. */
export function flyAshore(g: Game, w: AirFleet, r: Region): boolean {
  if (whyNotAshore(g, w, r)) return false;
  const c = carrierOf(g, w)!, kind = kindOfWing(w);
  (c.bare ??= { ftr: 0, bmr: 0 })[kind] += w.est[kind];
  delete w.ship;
  w.ashore = c.id;
  w.base = r.id;
  w.moved = g.turn;
  w.ready = Math.max(AIR.ready.floor, w.ready - AIR.ready.move);
  if (isOffensive(w.mission) && whyNotMission(g, w, w.mission, w.target)) { w.mission = 'support'; w.target = -1; }
  return true;
}

/** Why this wing ashore cannot go back aboard this fleet now, or null if it can. */
export function whyNotAboard(g: Game, f: AirFleet, c: Fleet): string | null {
  if (!carrierRules(g) || f.ashore === undefined || f.ship !== undefined) return 'Only a carrier wing ashore can be sent aboard.';
  if (c.nat !== f.nat) return 'It goes aboard only the carriers of its own navy.';
  const kind = kindOfWing(f);
  if (c.ships.cv <= 0) return `The ${c.name} has no carriers.`;
  if (bareOf(c, kind) <= 0) return `The decks of the ${c.name} are full: there is no room aboard for it.`;
  if (f.flew === g.turn) return 'It flew its raid this month: it is back at its airfield next month.';
  if (f.moved === g.turn) return 'It has already changed base this month.';
  if (!offShore(g, c, f.base)) {
    return atSea(c) ? `The ${c.name} is in the ${g.seas[c.zone].name}: the wing must be at an airfield on that sea to fly out to it.`
      : `The ${c.name} lies at ${c.port}: the wing must be at the airfield there to go aboard.`;
  }
  return null;
}

/** A fleet a wing ashore could go back aboard: where it lies, the deck room standing empty for this kind of aircraft, and why not now (or null). */
export interface DeckOption { fleet: number; name: string; where: string; room: number; why: string | null }

/** The fleets of its nation with deck room standing empty for this wing: those it can board now first. */
export function decksFor(g: Game, f: AirFleet): DeckOption[] {
  if (!carrierRules(g) || f.ashore === undefined) return [];
  const kind = kindOfWing(f);
  return g.fleets.filter((c) => c.nat === f.nat && c.ships.cv > 0 && bareOf(c, kind) > 0)
    .map((c) => ({ fleet: c.id, name: c.name, where: atSea(c) ? `in the ${g.seas[c.zone].name}` : `in harbour at ${c.port}`, room: bareOf(c, kind), why: whyNotAboard(g, f, c) }))
    .sort((a, b) => (a.why ? 1 : 0) - (b.why ? 1 : 0) || (a.fleet === f.ashore ? 0 : 1) - (b.fleet === f.ashore ? 0 : 1) || a.fleet - b.fleet);
}

/** Why this wing ashore cannot go aboard any fleet now, in one sentence, or null if there is one it can board. */
export function whyNoDeck(g: Game, f: AirFleet): string | null {
  const all = decksFor(g, f);
  if (!all.length) return 'No carrier of its navy has deck room standing empty for it.';
  return all[0].why;
}

/** Sends a wing ashore back aboard a fleet. Returns false, and changes nothing, where whyNotAboard() says no. */
export function flyAboard(g: Game, f: AirFleet, c: Fleet): boolean {
  if (whyNotAboard(g, f, c)) return false;
  const kind = kindOfWing(f);
  const take = Math.min(bareOf(c, kind), f.est[kind]);
  // More aircraft than the deck room left for them (a carrier was lost meanwhile): the rest stay ashore in the reserve.
  const over = Math.max(0, f[kind] - take);
  if (over > 0) { const p = airPool(g, f.nat); f[kind] -= over; if (kind === 'ftr') { p.ftr += over; p.pilots += over; } else p.bmr += over; }
  c.bare![kind] -= take;
  trimBare(c);
  const ready = Math.max(AIR.ready.floor, f.ready - AIR.ready.move);
  const b = wingOf(g, c.id, kind);
  if (b) {
    // The fleet has raised a wing of this kind for a new carrier meanwhile: the two are one wing again.
    const n = f[kind], both = b[kind] + n;
    b.ready = both > 0 ? (b.ready * b[kind] + ready * n) / both : b.ready;
    b[kind] += n;
    b.est[kind] += take;
    b.lost.ftr += f.lost.ftr;
    b.lost.bmr += f.lost.bmr;
    if (f.flew !== undefined && (b.flew ?? -1) < f.flew) b.flew = f.flew;
    if (f.scouted !== undefined && (b.scouted ?? -1) < f.scouted) b.scouted = f.scouted;
    b.moved = g.turn;
    g.airFleets.splice(g.airFleets.indexOf(f), 1);
    return true;
  }
  f.ship = c.id;
  delete f.ashore;
  f.base = portOf(g, c);
  f.name = wingName(c, kind);
  f.est = { ftr: 0, bmr: 0 };
  f.est[kind] = take;
  f.moved = g.turn;
  f.ready = ready;
  if (isOffensive(f.mission) && whyNotMission(g, f, f.mission, f.target)) { f.mission = 'support'; f.target = -1; }
  return true;
}

/** A fleet's wings that are ashore: where each is. Empty where none is. */
export const wingsAshore = (g: Game, fleet: number): AirFleet[] => g.airFleets.filter((f) => f.ashore === fleet);

/** The line a fleet's card carries while deck room stands empty for wings ordered ashore, or '' where none is. */
export function bareLine(g: Game, c: Fleet): string {
  if (!carrierRules(g) || c.ships.cv <= 0 || bareOf(c, 'ftr') + bareOf(c, 'bmr') <= 0) return '';
  const all = wingsOf(g, c.id).every((w) => w.est.ftr + w.est.bmr <= 0);
  return all ? `Its wings are ashore: its carriers fight as empty carriers.` : `Part of its wings are ashore: its carriers fight at ${Math.round(deckWorth(g, c) * 100)}% of their worth until they are back aboard.`;
}

/** The same in few words, for where the card has one short line (the fleet's tray). */
export function bareBrief(g: Game, c: Fleet): string {
  if (!bareLine(g, c)) return '';
  if (wingsOf(g, c.id).every((w) => w.est.ftr + w.est.bmr <= 0)) return 'Its wings are ashore: its carriers fight empty.';
  return `${wingsAshore(g, c.id).length > 1 ? 'Wings are' : 'A wing is'} ashore: its carriers fight at ${Math.round(deckWorth(g, c) * 100)}%.`;
}

/** What a fleet's carriers are worth beside carriers with full wings: in a fleet action, with a convoy, hunting submarines. */
export const deckWorth = (g: Game, c: Fleet): number => (carrierRules(g) && c.ships.cv > 0 ? CARRIERS.empty + (1 - CARRIERS.empty) * wingShare(g, c) : 1);

/** The share of the toll enemy bombers on the coasts take of a fleet that its side's carrier fighters, on patrol in that sea, keep off it. */
export function capOver(g: Game, c: Fleet): number {
  if (!carrierRules(g) || !atSea(c)) return 0;
  let n = 0;
  for (const w of g.airFleets) {
    if (w.ship === undefined || w.ftr <= 0 || w.mission !== 'support' || !allied(g, w.nat, c.nat)) continue;
    const at = carrierOf(g, w);
    if (at && atSea(at) && at.zone === c.zone) n += w.ftr * (0.5 + 0.5 * w.ready);
  }
  return Math.min(CARRIERS.cap.most, n / (n + CARRIERS.cap.scale));
}

/** One of a fleet's wings as a card for its tray. */
export interface WingCard {
  id: number;
  fleet: number;
  kind: WingKind;
  name: string;
  /** "Fighters" or "Strike aircraft", and three words for the face of the card. */
  label: string;
  brief: string;
  aircraft: number;
  est: number;
  ready: number;
  /** What it is doing, in a phrase. */
  doing: string;
  flew: boolean;
  /** How far it reaches, in kilometres, and how far a neighbouring sea counts as. */
  range: number;
  /** Aircraft lost since the war began. */
  lost: number;
  /** How it is made up again, in a sentence: only in harbour. */
  note: string;
  /** Where it is, in a line: "Aboard the Atlantic Fleet". */
  where: string;
  /** Why it cannot be sent ashore anywhere now, or null where it can (ruling 175(b)). */
  whyNotAshore: string | null;
}

/** The wings of one fleet as cards. Empty for a fleet without carriers, and outside the grand campaign. */
export function wingCards(g: Game, c: Fleet): WingCard[] {
  if (!carrierRules(g)) return [];
  const R = CARRIERS.range;
  return wingsOf(g, c.id).map((w) => {
    const kind: WingKind = w.kind === 'bmr' ? 'bmr' : 'ftr';
    const n = w[kind], est = w.est[kind];
    const away = w.flew === g.turn;
    const doing = away ? 'Flew its strike this month: back aboard next month'
      : w.mission === 'rest' ? 'Stood down to rest and refit'
      : kind === 'ftr' ? (atSea(c) ? `On patrol over the ships in the ${g.seas[c.zone].name}` : `In harbour at ${c.port}: on patrol over its fleet`)
      : 'Ranged on deck, ready for a strike';
    const short = est - n;
    return {
      id: w.id, fleet: c.id, kind, name: w.name, label: kind === 'ftr' ? 'Carrier fighters' : 'Strike aircraft', brief: kind === 'ftr' ? 'Fleet cover · escort' : 'Ships · harbours · coast',
      aircraft: n, est, ready: w.ready, doing, flew: away, range: kind === 'ftr' ? R.ftr : R.bmr, lost: w.lost[kind],
      where: `Aboard the ${c.name}`, whyNotAshore: whyNoShore(g, w),
      note: short <= 0 ? 'At full strength.' : atSea(c) ? `${plural(short, 'aircraft', 'aircraft')} short. No replacements reach a deck at sea: bring the fleet into harbour.` : `${plural(short, 'aircraft', 'aircraft')} short: made up from the reserve while the fleet lies in harbour.`,
    };
  });
}

// ---- Finding ships at sea -------------------------------------------------------------------------------

/** How well a fleet is known to a nation: identified, reported (ships are there, not what), or not found at all. */
export type SeaKnown = 'exact' | 'report' | 'none';

/** How much more search open ocean takes than narrow waters. */
const openness = (g: Game, z: number): number => 1 + CARRIERS.search.open * (1 - Math.min(1, g.seas[z]?.air ?? 1));

/** What a nation's side brings to finding ships in one sea: its ships there, its carriers next door, its aircraft on the coasts, and a search flown this month. */
export function searchOf(g: Game, nat: string, z: number): number {
  const S = CARRIERS.search;
  let w = 0;
  for (const f of g.fleets) {
    if (!atSea(f) || !(f.nat === nat || allied(g, f.nat, nat))) continue;
    if (f.zone === z) { for (const c of CLASSES) w += f.ships[c] * S.ships[c] * (c === 'cv' ? wingShare(g, f) : 1); }
    else if (f.ships.cv > 0 && g.seas[f.zone]?.adj.includes(z)) w += f.ships.cv * S.next * wingShare(g, f);
  }
  w += airOver(g, z, nat) * S.air;
  const me = g.human;
  let flown = !!me && (nat === me || allied(g, nat, me)) && g.intel?.sea?.[z] === g.turn;
  // A computer staff's search counts for its side as the player's does for his (ruling 52), once for a sea.
  const by = g.intel?.seaBy;
  if (!flown && by) for (const k in by) if (by[k][z] === g.turn && (k === nat || allied(g, k, nat))) { flown = true; break; }
  if (flown) w += S.flight;
  return w;
}

/** The last month a nation, or one of its allies, had this fleet in contact: it fought it, was shelled by it or struck by its aircraft. */
function contact(g: Game, f: Fleet, nat: string): number {
  let t = -99;
  for (const [k, turn] of Object.entries(f.seen ?? {})) if (turn > t && (k === nat || allied(g, k, nat))) t = turn;
  return t;
}

/** True when a port is in a nation's full view: its own side's ground or troops are within sight of it, or one of its agents is, or its aircraft have photographed it. */
function portInView(g: Game, region: number, nat: string): boolean {
  const far = INTEL.sight.full;
  const agents = new Set(agentsOf(g, nat).filter((a) => atWork(g, a)).map((a) => a.reg));
  const shot = nat === g.human ? g.intel?.scout ?? {} : {};
  const seen = new Set<number>([region]);
  let ring = [region];
  for (let d = 0; d <= far && ring.length; d++) {
    const next: number[] = [];
    for (const id of ring) {
      const r = g.regions[id];
      if (allied(g, r.ctrl, nat) || agents.has(id) || shot[id] === g.turn || r.units.some((u) => !u.dead && allied(g, u.nat, nat))) return true;
      for (const j of r.nb) if (!seen.has(j)) { seen.add(j); next.push(j); }
    }
    ring = next;
  }
  return false;
}

/**
 * How well a nation's side has a fleet found, by the rules of the search alone. Its own and its allies' it knows.
 * A fleet it was in contact with this month is identified, and reported the month after. Ships in harbour are
 * always reported, and identified where the port is in full view or the two nations are at peace. At sea it turns
 * on the search of that sea.
 */
export function fleetFound(g: Game, f: Fleet, nat: string): SeaKnown {
  if (f.nat === nat || allied(g, f.nat, nat)) return 'exact';
  const met = contact(g, f, nat);
  if (met === g.turn) return 'exact';
  // Nobody hides a fleet in harbour from a nation it is at peace with: the attachés count the ships.
  if (!atSea(f)) return !atWar(g, f.nat, nat) || (f.base >= 0 && portInView(g, f.base, nat)) ? 'exact' : 'report';
  const S = CARRIERS.search;
  const need = openness(g, f.zone) * (surfaceShips(f) > 0 ? 1 : S.subs);
  const w = searchOf(g, nat, f.zone);
  if (w >= S.find * need) return 'exact';
  return w >= S.report * need || met === g.turn - 1 ? 'report' : 'none';
}

/**
 * How well a fleet is known to whoever is looking. With nobody at the table, outside the grand campaign, or with
 * the player's fog of war lifted, everything is known.
 */
export function fleetKnown(g: Game, f: Fleet, viewer: string | null = g.human): SeaKnown {
  if (viewer === null || !carrierRules(g)) return 'exact';
  if (viewer === g.human && !fogOn(g, viewer)) return 'exact';
  return fleetFound(g, f, viewer);
}

/** The word for a force of this many surface ships, for a report that cannot say more. */
const sizeWord = (n: number, subs: boolean): string => (subs ? 'Submarines' : n <= 6 ? 'A few ships' : n <= 20 ? 'A small fleet' : 'A fleet');

/** A fleet as a viewer may draw it. Where it is only reported there is a flag and a rough size, and no more. */
export interface FleetSeen {
  id: number;
  nat: string;
  known: SeaKnown;
  /** Its name, or what a report calls it: "Italian ships reported". */
  name: string;
  atSea: boolean;
  zone: number;
  /** The region of its port, or -1. */
  base: number;
  /** Its ships, or null where they are not identified. */
  ships: Ships | null;
  /** How many ships, or -1 where that is not known, and a word for a force of that size. */
  count: number;
  size: string;
  /** Whether it has carriers, where that is known. */
  carriers: number;
  side: 'own' | 'ally' | 'foe' | 'other';
}

/** Every fleet the viewer can put on the map: his own side's, and those of others as far as they have been found. */
export function fleetsSeen(g: Game, viewer: string | null = g.human): FleetSeen[] {
  const out: FleetSeen[] = [];
  for (const f of g.fleets) {
    if (!shipCount(f.ships)) continue;
    const known = fleetKnown(g, f, viewer);
    if (known === 'none') continue;
    const side = viewer === null ? 'other' : f.nat === viewer ? 'own' : allied(g, f.nat, viewer) ? 'ally' : atWar(g, f.nat, viewer) ? 'foe' : 'other';
    const exact = known === 'exact';
    const surface = surfaceShips(f);
    out.push({
      id: f.id, nat: f.nat, known, name: exact ? f.name : `${g.nations[f.nat].adj} ships reported`, atSea: atSea(f), zone: f.zone, base: f.base,
      ships: exact ? { ...f.ships } : null, count: exact ? shipCount(f.ships) : -1, size: sizeWord(surface, surface <= 0), carriers: exact ? f.ships.cv : -1, side,
    });
  }
  return out;
}

/** One sea as a viewer knows it: each side's strength as far as it has been found, and how many forces there are only reports. */
export interface SeaPicture {
  zone: number;
  /** The viewer's side: ships and aircraft from the coasts. */
  ours: number;
  /** The enemy: the ships that are identified, and the aircraft that reach from his coasts. */
  theirs: number;
  theirAir: number;
  /** Enemy forces reported but not identified, whose strength is not counted. */
  reported: number;
  /** The viewer's search of this sea, what it takes to get a report and to identify a surface fleet here, and a word for how well it is searched. */
  search: number;
  needReport: number;
  needFind: number;
  word: string;
}

export function seaPicture(g: Game, z: number, viewer: string): SeaPicture {
  const us = seaSide(g, z, viewer, false), air = seaSide(g, z, viewer, true).air;
  let ships = 0, reported = 0;
  for (const f of g.fleets) {
    if (f.zone !== z || !atSea(f) || !atWar(g, f.nat, viewer) || !shipCount(f.ships)) continue;
    const k = fleetKnown(g, f, viewer);
    if (k === 'exact') ships += fleetPower(g, f); else if (k === 'report') reported++;
  }
  const S = CARRIERS.search, open = openness(g, z), w = searchOf(g, viewer, z);
  const word = !carrierRules(g) || !fogOn(g, viewer) ? 'In full view' : w >= S.find * open ? 'Well searched: a fleet here would be identified' : w >= S.report * open ? 'Thinly searched: a fleet here would be reported, not identified' : 'Not searched: a fleet could be here unseen';
  return { zone: z, ours: us.total, theirs: ships + air, theirAir: air, reported, search: w, needReport: S.report * open, needFind: S.find * open, word };
}

// ---- A search flown from the planner --------------------------------------------------------------------

const mineOnly = (g: Game, id: number, nat: string | null): AirFleet | null => {
  const f = g.airFleets.find((x) => x.id === id);
  return f && nat !== null && f.nat === nat && carrierRules(g) ? f : null;
};

/** Why a group cannot search a sea this month, or null if it can. */
export function whyNotSearch(g: Game, f: AirFleet, z: number): string | null {
  if (!carrierRules(g)) return 'Seas are searched only in the grand campaign.';
  if (!g.seas[z]) return 'Pick a sea.';
  if (f.ftr + f.bmr <= 0) return 'It has no aircraft fit to fly.';
  if (f.scouted === g.turn) return 'It has flown its reconnaissance this month.';
  if (!g.nations[f.nat].atWar.length) return 'The nation is at peace.';
  // Weather (ruling 127): no search flies from or over a closed sky.
  if (weatherOn(g)) { const why = whyNotSky(g, f, zoneOfSea(g, z)); if (why) return why; }
  const R = airRange(f, g);
  return kmToSea(g, f, z) <= (f.bmr > 0 ? R.bmr : R.ftr) ? null : 'That sea is out of its range.';
}

/** A sea a group could search, with what the search stands at there now. */
export interface SearchOption { zone: number; name: string; x: number; y: number; km: number; searched: boolean; word: string; reported: number }

/** The seas one of a nation's groups could search this month, nearest first. */
export function searchOptions(g: Game, id: number, nat: string | null = g.human): SearchOption[] {
  const f = mineOnly(g, id, nat);
  if (!f) return [];
  const out: SearchOption[] = [];
  for (const z of g.seas) {
    if (whyNotSearch(g, f, z.id)) continue;
    const p = seaPicture(g, z.id, f.nat);
    out.push({ zone: z.id, name: z.name, x: z.x, y: z.y, km: kmToSea(g, f, z.id), searched: g.intel.sea?.[z.id] === g.turn, word: p.word, reported: p.reported });
  }
  return out.sort((a, b) => a.km - b.km || a.zone - b.zone);
}

/**
 * A search of one sea: a few aircraft of one group quarter it for the month. It adds to the nation's search of that
 * sea, which may be enough to report a fleet there, or to identify one. One flight a month for each group, beside
 * its raid, as a reconnaissance over land is.
 */
export function searchSea(g: Game, id: number, z: number, nat: string | null = g.human): { ok: boolean; text: string; found: number } {
  const f = mineOnly(g, id, nat);
  const why = !f ? 'That group is not yours to send.' : whyNotSearch(g, f, z);
  if (!f || why) return { ok: false, text: why ?? '', found: 0 };
  const foes = g.fleets.filter((x) => x.zone === z && atSea(x) && shipCount(x.ships) > 0 && atWar(g, x.nat, f.nat));
  const before = foes.map((x) => fleetKnown(g, x, f.nat));
  f.scouted = g.turn;
  f.ready = Math.max(AIR.ready.floor, f.ready - CARRIERS.search.ready);
  // Last month's searches are out of date: the ships have moved.
  const kept: Record<number, number> = {};
  for (const [k, turn] of Object.entries(g.intel.sea ?? {})) if (turn === g.turn) kept[+k] = turn;
  kept[z] = g.turn;
  g.intel.sea = kept;
  const after = foes.map((x) => fleetKnown(g, x, f.nat));
  const rank = { none: 0, report: 1, exact: 2 } as const;
  const news = foes.filter((_, i) => rank[after[i]] > rank[before[i]]);
  const told = news.map((x) => (fleetKnown(g, x, f.nat) === 'exact' ? `the ${x.name} (${shipList(x.ships)})` : `${g.nations[x.nat].adj} ships, not identified`));
  const sea = g.seas[z].name;
  const text = told.length ? `Aircraft of the ${f.name} searching the ${sea} found ${told.join(', and ')}.`
    : foes.some((_, i) => after[i] !== 'none') ? `Aircraft of the ${f.name} searched the ${sea} and found nothing new.`
    : `Aircraft of the ${f.name} searched the ${sea} and found no enemy ships.`;
  if (f.nat === g.human) headline(g, 'info', text, f.nat);
  return { ok: true, text, found: news.length };
}

// ---- The computer's admirals and air staff at sea -------------------------------------------------------

/**
 * A computer staff's searches of the sea (ruling 52): flown as the player's are, a few aircraft of one group
 * quartering one sea for the month at the same cost in readiness, and counted the same in its side's search of
 * that sea. The staff looks where its own war points it, and at nothing it could not know: first a sea where enemy
 * ships are reported and not identified; then the seas where its side's ships, cargoes or coasts were attacked
 * since last month; then the seas off its own harbours and its fleets' stations, taken in turn. A sea its side
 * has in full view already is left alone. So many searches a month and no more (carriers.json ai.searches), by
 * the nearest group that can fly one, and never by a group the flight would leave unfit to raid.
 */
function staffSearch(g: Game, nat: string): void {
  const most = CARRIERS.ai.searches ?? 0;
  if (!(most > 0) || !g.intel) return;
  const S = CARRIERS.search;
  const ours = (k: string): boolean => k === nat || allied(g, k, nat);
  const wanted: number[] = [];
  const want = (z: number | undefined): void => { if (z !== undefined && g.seas[z] && !wanted.includes(z)) wanted.push(z); };
  for (const x of g.fleets) if (atSea(x) && shipCount(x.ships) > 0 && atWar(g, nat, x.nat) && fleetFound(g, x, nat) === 'report') want(x.zone);
  for (const f of g.fleets) if (ours(f.nat) && f.struck && f.struck[0] >= g.turn - 1) want(f.struck[2]);
  for (let i = g.seaReports.length - 1; i >= 0 && g.seaReports[i].turn >= g.turn - 1; i--) if (ours(g.seaReports[i].attNat) || ours(g.seaReports[i].defNat)) want(g.seaReports[i].zone);
  for (let i = g.shoreReports.length - 1; i >= 0 && g.shoreReports[i].turn >= g.turn - 1; i--) if (ours(g.shoreReports[i].defNat)) want(g.shoreReports[i].zone);
  for (const rt of NAVY.convoys) {
    const m = g.convoys[rt.id];
    if (ours(rt.to) && m && m.turn >= g.turn - 1 && m.tons > 0) want(zoneOf(g, rt.zones[g.turn % rt.zones.length]));
  }
  const round = new Set<number>();
  for (const f of fleetsOf(g, nat)) if (shipCount(f.ships) > 0) { round.add(f.station); round.add(f.home); }
  for (const r of g.regions) if (r.port && r.ctrl === nat && r.owner === nat) for (const z of r.seas) round.add(z);
  const beat = [...round].filter((z) => !!g.seas[z]).sort((a, b) => a - b);
  for (let i = 0; i < beat.length; i++) want(beat[(i + g.turn) % beat.length]);
  const by = ((g.intel.seaBy ??= {})[nat] ??= {});
  for (const k of Object.keys(by)) if (by[+k] !== g.turn) delete by[+k];
  const ready = AIRWAR.ai.raids.ready;
  const groups = airFleetsOf(g, nat).filter((f) => f.ftr + f.bmr > 0 && f.scouted !== g.turn && !(f.ready >= ready && f.ready - S.ready < ready));
  let flown = Object.keys(by).length;
  for (const z of wanted) {
    if (flown >= most || !groups.length) break;
    if (by[z] === g.turn || searchOf(g, nat, z) >= S.find * openness(g, z)) continue;
    let best: AirFleet | null = null, bd = 0;
    for (const f of groups) {
      if (f.scouted === g.turn || whyNotSearch(g, f, z)) continue;
      const d = kmToSea(g, f, z);
      if (!best || d < bd || (d === bd && f.id < best.id)) { best = f; bd = d; }
    }
    if (!best) continue;
    best.scouted = g.turn;
    best.ready = Math.max(AIR.ready.floor, best.ready - S.ready);
    by[z] = g.turn;
    flown++;
  }
}

/** True where a strike on this sea would cost an enemy of this nation trade: his convoys cross it, or his coasters run under his own shore (air.ts counts it so). */
function tradeIn(g: Game, nat: string, z: number): boolean {
  const sea = g.seas[z];
  if (NAVY.convoys.some((c) => atWar(g, nat, c.to) && c.zones.includes(sea.key))) return true;
  return sea.coast.some((id) => { const r = g.regions[id]; return r.port && r.owner === r.ctrl && atWar(g, nat, r.ctrl); });
}

interface Blow { m: AirMission; t: number; fleets: AirFleet[]; v: number }

/**
 * The best strike on ships one bomber group could fly from where it is, with these fighters to go with it if fighters
 * will come up. `sure` flies only at a fleet that is identified. With `trade`, the enemy's cargoes are weighed beside
 * his warships (ruling 52): a strike at the shipping in a sea his convoys cross or his coasters work.
 */
function bestBlow(g: Game, nat: string, lead: AirFleet, escorts: AirFleet[], hit: Set<string>, sure = false, trade = false): Blow | null {
  const A = CARRIERS.ai;
  let best: Blow | null = null;
  const weigh = (m: AirMission, t: number): void => {
    if (hit.has(`${m}:${t}`) || whyNotMission(g, lead, m, t)) return;
    let fleets = [lead];
    let a = assessAir(g, fleets, m, t);
    if (a.why) return;
    // The fighters go only where fighters will meet the bombers: otherwise they stay over their own ships and airfields.
    const with_ = escorts.filter((e) => whyNotMission(g, e, m, t) === null);
    if (a.D >= 1 && with_.length) { fleets = [lead, ...with_]; a = assessAir(g, fleets, m, t); if (a.why) return; }
    const fc = forecastAir(g, a, m, t);
    if (fc.success < A.success || fc.bomberLoss > A.loss) return;
    const v = fc.worth + fc.foeLoss - fc.ftrLoss - fc.bmrLoss;
    if (v > A.worth && (!best || v > best.v)) best = { m, t, fleets, v };
  };
  const ports = new Set<number>();
  for (const x of g.fleets) {
    if (!atWar(g, nat, x.nat) || !surfaceShips(x)) continue;
    if (!atSea(x)) { if (x.base >= 0) ports.add(x.base); continue; }
    // Only what the staff has found can be struck.
    const k = fleetFound(g, x, nat);
    if (k === 'exact' || (k === 'report' && !sure)) weigh('fleet', x.id);
  }
  for (const r of ports) if (shipsInPort(g, r, nat).length) weigh('harbour', r);
  if (trade) for (const z of g.seas) if (tradeIn(g, nat, z.id)) weigh('strike', z.id);
  return best;
}

/**
 * The computer's strikes on ships for one nation, flown at once as the player's are. Each carrier fleet's strike
 * wing goes for the best fleet its side has found in reach, or for ships in a harbour; a carrier fleet lying in
 * harbour with nothing in reach puts to sea for the month to strike, where its side would be much the stronger.
 * A few of the land bombers may go for ships too. A strike is flown only where the forecast says it should pay.
 */
export function seaStaff(g: Game, nat: string, what: 'all' | 'decks' | 'land' = 'all'): void {
  // The player's carriers go with his navy and his land bombers with his air force: each flies for him only where
  // he has handed that desk to his staff (depth.ts), and then it is asked for by itself.
  const his = what === 'decks' ? byHand(g, nat, 'navy') : what === 'land' ? byHand(g, nat, 'air') : nat === g.human;
  if (!carrierRules(g) || his || !g.nations[nat]?.alive || !g.nations[nat].atWar.length) return;
  const A = CARRIERS.ai, R = AIRWAR.ai.raids;
  // The searches first (ruling 52): what they find can be struck this month. The player's own reconnaissance stays his.
  if (nat !== g.human) staffSearch(g, nat);
  const trade = !!A.trade;
  const hit = new Set<string>();
  let flown = what === 'land' ? 100 : 0;
  const fit = (f: AirFleet | undefined): f is AirFleet => !!f && f.flew !== g.turn && f.mission === 'support' && f.ready >= A.ready && !inCampaign(g, f);
  const send = (b: Blow): void => {
    const rep = fly(g, nat, b.m, b.t, b.fleets, `:sea${flown++}`);
    hit.add(`${b.m}:${b.t}`);
    if (rep) for (const f of b.fleets) { f.mission = b.m; f.target = b.t; f.flew = g.turn; }
  };
  // The carriers.
  for (const c of what === 'land' ? [] : fleetsOf(g, nat)) {
    if (c.ships.cv <= 0) continue;
    const lead = wingOf(g, c.id, 'bmr'), cap = wingOf(g, c.id, 'ftr');
    if (!fit(lead) || lead.bmr < 6) continue;
    const escorts = fit(cap) && cap.ftr > 0 ? [cap] : [];
    const here = bestBlow(g, nat, lead, escorts, hit, false, trade);
    if (here) { send(here); continue; }
    // In harbour with nothing in reach: a sortie, into a sea where its side would be much the stronger with it there.
    if (atSea(c) || !(A.sortie > 0) || c.duty === 'raid' || c.ready < FLEETS.ai.ready || keptByHand(g, c)) continue;
    const power = fleetPower(g, c);
    const keep = { zone: c.zone, stance: c.stance };
    let at = -1, plan: Blow | null = null;
    for (const z of fleetReach(g, c)) {
      if (seaSide(g, z, nat, false).total + power < A.sortie * seaSide(g, z, nat, true).total) continue;
      // Weighed as it would be with the fleet there, and then everything is put back.
      c.zone = z; c.stance = 'patrol';
      const b = bestBlow(g, nat, lead, escorts, hit);
      c.zone = keep.zone; c.stance = keep.stance;
      if (b && (!plan || b.v > plan.v)) { plan = b; at = z; }
    }
    if (plan && sail(g, c, at, 'patrol')) {
      const b = bestBlow(g, nat, lead, escorts, hit);
      if (b) send(b);
    }
  }
  // A few of the land bombers.
  if (what === 'decks') return;
  const rng = new Rng(hashString(`air:${g.seed}:${g.turn}:seastaff:${nat}`));
  const leads = airFleetsOf(g, nat).filter((f) => !afloat(f) && f.kind !== 'ftr' && f.bmr >= R.least && fit(f)).sort((a, b) => b.bmr - a.bmr || a.id - b.id);
  let most = Math.floor(leads.length * A.landShare + rng.range(0, 1));
  for (const lead of leads) {
    if (most <= 0) break;
    const escorts = airFleetsOf(g, nat).filter((x) => x.kind === 'ftr' && x.ftr > 0 && fit(x) && homeOf(x) === homeOf(lead));
    const b = bestBlow(g, nat, lead, escorts, hit, true, trade);
    if (!b) continue;
    send(b);
    most--;
  }
}
