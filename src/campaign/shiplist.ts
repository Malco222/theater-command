// The ships of a fleet one by one (the sandbox: Brian, 19 November 2026, "a sandbox to play and make your own
// history"). A fleet keeps its count of ships of each kind, exactly as it always has, and everything that already
// counts ships goes on counting them. Beside the count it may keep a list: the ships laid down in the war to a class
// of their own (a what-if class, or a design of the nation's own), each named, each with the numbers she fights with
// and her record. Whatever of a kind's count is not on the list is that kind's standard ship.
//
// So a fleet of two carriers, one of them a battle carrier, counts cv: 2 and lists one ship. Its weight in a fleet
// action is one standard carrier's plus the battle carrier's own. A fleet with no list is worth exactly what it was
// worth before this file existed: every sum below falls back to the old product when there is no list, so a game
// that never builds a ship of its own class plays exactly as it did.
//
// This file knows only fleets and numbers; the classes, the names and the orders are in sandbox.ts.

import navyData from '../../data/campaign/navy.json';
import { Rng, hashString } from '../core/rng';
import type { Fleet, Game, ShipClass, ShipHull } from './types';

type Stat = 'power' | 'tough' | 'escort' | 'raid';
const STD = (navyData as unknown as { classes: Record<ShipClass, Record<Stat, number>> }).classes;
const KINDS: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];

/** The ships of a kind a fleet has on its list (a flotilla line counts as its ships). */
export function listed(f: Fleet, c: ShipClass): number {
  let n = 0;
  if (f.hulls) for (const h of f.hulls) if (h.kind === c) n += h.n;
  return n;
}

/** A fleet's standard ships of a kind: its count less the ships on its list. */
export const standard = (f: Fleet, c: ShipClass): number => Math.max(0, f.ships[c] - listed(f, c));

/**
 * What a fleet's ships of one kind add up to in one of the four numbers of navy.json: power in a fleet action,
 * 'tough' (how readily they are lost), escort for convoys, raid on trade. With no list it is the count times the
 * kind's number, the sum the game has always made.
 */
export function worth(f: Fleet, c: ShipClass, k: Stat): number {
  if (!f.hulls?.length) return f.ships[c] * STD[c][k];
  let p = standard(f, c) * STD[c][k];
  for (const h of f.hulls) if (h.kind === c) p += h.n * h[k];
  return p;
}

/**
 * The ships of a kind a fleet can expect to lose to a blow of this weight ('frac' of the fleet): the count times the
 * weight times the kind's 'tough', as it has always been reckoned, or each ship on the list at her own.
 */
export function lossWeight(f: Fleet, c: ShipClass, frac: number): number {
  if (!f.hulls?.length) return f.ships[c] * frac * STD[c].tough;
  return worth(f, c, 'tough') * frac;
}

/** How many carriers' worth of decks a fleet has: its carriers, each ship on its list counted at her share of a wing. */
export function decks(f: Fleet): number {
  if (!f.hulls?.length) return f.ships.cv;
  let d = standard(f, 'cv');
  for (const h of f.hulls) if (h.kind === 'cv') d += h.n * (h.deck ?? 1);
  // A ship of another kind that carries a share of a wing (none of the six what-if classes does; a design may).
  for (const h of f.hulls) if (h.kind !== 'cv' && h.deck) d += h.n * h.deck;
  return d;
}

/** The share by which a fleet's anti-aircraft ships cut its losses to an air strike, never more than 'most'. */
export function aaCut(f: Fleet, most: number): number {
  if (!f.hulls?.length) return 0;
  let a = 0;
  for (const h of f.hulls) a += h.n * (h.aa ?? 0);
  return Math.min(most, a);
}

/** How many seas further than its own a fleet's search reaches: the best of its ships that carry aircraft to scout. */
export function eyes(f: Fleet): number {
  let e = 0;
  if (f.hulls) for (const h of f.hulls) if (h.n > 0) e = Math.max(e, h.eyes ?? 0);
  return e;
}

/** How many of a fleet's ships fly scouts of their own beyond its sea (a scout submarine, a cruiser with a seaplane hangar). */
export function scouts(f: Fleet): number {
  let n = 0;
  if (f.hulls) for (const h of f.hulls) if ((h.eyes ?? 0) > 0) n += h.n;
  return n;
}

/** A fleet's ships of a kind for their oil at sea: each ship on its list at her own share of her kind's need. */
export function oilShips(f: Fleet, c: ShipClass): number {
  if (!f.hulls?.length) return f.ships[c];
  let n = standard(f, c);
  for (const h of f.hulls) if (h.kind === c) n += h.n * (h.oil ?? 1);
  return n;
}

// ---- Losses ----------------------------------------------------------------------------------------------

/**
 * Strikes k ships of a kind off a fleet as lost: the count and the fleet's tally of losses, as every loss always has.
 * With a list, which ships went down is drawn by how readily each is lost (her 'tough'), so a fleet's standard ships
 * and its own classes go down in fair shares. The draw has its own stream of numbers, so it takes nothing from the
 * roll that decided how many were lost: the war goes on exactly as it would have.
 */
export function loseShips(f: Fleet, c: ShipClass, k: number): void {
  f.ships[c] -= k;
  f.lost[c] += k;
  if (k > 0 && f.hulls?.some((h) => h.kind === c)) pickLost(f, c, k);
}

function pickLost(f: Fleet, c: ShipClass, k: number): void {
  const rng = new Rng(hashString(`hulls:${f.id}:${c}:${f.lost[c]}`));
  const mine = f.hulls!.filter((h) => h.kind === c);
  let std = Math.max(0, f.ships[c] + k - listed(f, c));
  const w0 = STD[c].tough;
  for (let i = 0; i < k; i++) {
    let total = std * w0;
    for (const h of mine) total += h.n * h.tough;
    if (total <= 0) break;
    let x = rng.next() * total;
    if (x < std * w0) { std--; continue; }
    x -= std * w0;
    for (const h of mine) {
      if (h.n <= 0) continue;
      if (x < h.n * h.tough) { fall(f, h); break; }
      x -= h.n * h.tough;
    }
  }
  trim(f);
}

/** One ship of a line goes down: she goes on the fleet's list of the fallen, and a line with no ships left is struck off. */
function fall(f: Fleet, h: ShipHull): void {
  h.n--;
  (f.fallen ??= []).push({ ...h, n: 1 });
  if (h.n <= 0) f.hulls!.splice(f.hulls!.indexOf(h), 1);
  if (!f.hulls!.length) delete f.hulls;
}

/** Keeps the list inside the count: any kind with more on the list than the fleet has loses the last of them. */
export function trim(f: Fleet): void {
  if (!f.hulls) return;
  for (const c of KINDS) {
    let over = listed(f, c) - f.ships[c];
    for (let i = f.hulls.length - 1; over > 0 && i >= 0; i--) {
      const h = f.hulls[i];
      if (h.kind !== c) continue;
      while (over > 0 && h.n > 0) { fall(f, h); over--; if (!f.hulls) return; }
    }
  }
}

// ---- Ships that change fleets -----------------------------------------------------------------------------

/**
 * After ships have moved from one fleet to another by count (a fleet split, a fleet joined, ships sent on), moves the
 * lines of the list with them. A fleet sends its standard ships first, so its own classes stay where they were unless
 * they had to go, or unless 'pick' names them: the ships the player chose by name go first.
 */
export function moveHulls(from: Fleet, to: Fleet, pick: string[] = []): void {
  if (!from.hulls) return;
  const go = (h: ShipHull, n: number): void => {
    const line = to.hulls?.find((x) => x.name === h.name && x.cls === h.cls);
    if (line) line.n += n; else (to.hulls ??= []).push({ ...h, n });
    h.n -= n;
  };
  // The chosen ships first, as far as the counts that moved allow.
  for (const name of pick) {
    const h = from.hulls.find((x) => x.name === name);
    if (h && h.n > 0 && listed(to, h.kind) + h.n <= to.ships[h.kind]) go(h, h.n);
  }
  // Then whatever the fleet that sent them no longer has room for, from the end of its list.
  for (const c of KINDS) {
    let over = listed(from, c) - from.ships[c];
    for (let i = from.hulls.length - 1; over > 0 && i >= 0; i--) {
      const h = from.hulls[i];
      if (h.kind !== c || h.n <= 0) continue;
      const n = Math.min(h.n, over);
      go(h, n);
      over -= n;
    }
  }
  from.hulls = from.hulls.filter((h) => h.n > 0);
  if (!from.hulls.length) delete from.hulls;
}

/** Moves n ships of a kind from one fleet to another, their lines with them. */
export function moveShips(from: Fleet, to: Fleet, c: ShipClass, n: number): void {
  if (n <= 0) return;
  from.ships[c] -= n;
  to.ships[c] += n;
  moveHulls(from, to);
}

// ---- The record -------------------------------------------------------------------------------------------

/** Every ship on a fleet's list comes through a battle afloat: a battle star each. */
export function starHulls(f: Fleet): void {
  if (f.hulls) for (const h of f.hulls) h.stars = (h.stars ?? 0) + 1;
}

/**
 * Writes a fleet's fallen on its nation's memorial: the month, the sea, her stars. Returns them, for the news. A
 * fleet lost whole (it had no port left to go to) loses its list with it.
 */
export function writeFallen(g: Game, f: Fleet, how?: string, whole = false): ShipHull[] {
  if (whole && f.hulls) { for (const h of f.hulls) (f.fallen ??= []).push({ ...h }); delete f.hulls; }
  const out = f.fallen ?? [];
  if (!out.length) return out;
  const n = g.nations[f.nat];
  if (n) for (const h of out) {
    for (let i = 0; i < h.n; i++) (n.memorial ??= []).push({ name: h.name, label: h.label, kind: h.kind, turn: g.turn, sea: f.stance === 'port' ? -1 : f.zone, stars: h.stars ?? 0, ...(how ? { how } : {}) });
  }
  delete f.fallen;
  return out;
}
