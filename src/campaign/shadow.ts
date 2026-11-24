// Shadowing (release 2; Brian, 24 November 2026 19:47Z, "yes to all", of the idea "a cruiser can shadow an enemy fleet:
// it follows wherever that fleet goes and reports where it is"). Behind its own switch (stage.json 'shadow', the flag
// 'shadow'): off, nothing here is read, no fleet carries a shadow, and every war is the war without it.
//
// A fleet of cruisers (destroyers may sail with them; no battleship or carrier) is set on an enemy fleet in its reach.
// At the start of every country's month it follows that fleet into the sea it is in, or off the harbour it lies in, and
// keeps it identified for its side all month (Fleet.seen, the contact every sighting rule reads). At the start of its
// own side's month it reports where the fleet is, in a line of news. When the fleet goes out of its reach, contact is
// lost and the shadow ends where it stands. A shadow seeks no battle (navy.ts seaBattles leaves it out of its side's
// patrols); found alone by an enemy who would fight it, it is faster than what it follows and keeps its distance
// SHADOW.slip of the time. Like a detective tailing a car: close enough to read the plates, never close enough to be
// pulled over, and it phones in each turn the car takes.
//
// The same rule for every country: the computer's admirals set shadows of their own (aiShadows), at most SHADOW.ai.most
// at a time, on the strongest enemy fleet at sea they have found.

import data from '../../data/campaign/shadow.json';
import { fleetKnown } from './carriers';
import { atWar, headline } from './game';
import { atSea, fleetName, fleetPower, fleetReach, sail, shipCount, surfaceShips } from './navy';
import { seaWhere } from './sea-words';
import type { Fleet, Game } from './types';

export const SHADOW = data as unknown as { slip: number; ai: { most: number; ready: number; home: number } };

/** True where fleets may be set to shadow an enemy fleet: the grand campaign, with the switch on. */
export const shadowOn = (g: Game): boolean => g.flags.open === 1 && g.flags.shadow === 1;

/** Turns shadowing on or off in a campaign under way. Off, every shadow ends where it stands. */
export function setShadow(g: Game, on: boolean): void {
  if (on) g.flags.shadow = 1;
  else {
    delete g.flags.shadow;
    for (const f of g.fleets) delete f.shadow;
  }
}

const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
const hisMonth = (g: Game): boolean => g.human !== null && g.order[g.phase] === g.human;

/** True for a fleet that can shadow: cruisers, with destroyers or without, and no battleship or carrier. */
export const shadowShips = (f: Fleet): boolean => f.ships.ca > 0 && f.ships.bb === 0 && f.ships.cv === 0;

/**
 * The sea fleet `f` keeps watch from on fleet `t`: the sea `t` is in, or, with `t` in harbour, its home sea or else the
 * first sea of its port's coast that `f` reaches. -1 where none is in its reach.
 */
function watchSea(g: Game, f: Fleet, t: Fleet): number {
  const reach = fleetReach(g, f);
  if (atSea(t)) return reach.includes(t.zone) ? t.zone : -1;
  if (reach.includes(t.home)) return t.home;
  const r = t.base >= 0 ? g.regions[t.base] : undefined;
  return r?.seas.find((z) => reach.includes(z)) ?? -1;
}

/** Where a fleet is, in words: "in the North Sea", "in harbour at Wilhelmshaven". */
const whereIs = (g: Game, t: Fleet): string => (atSea(t) ? seaWhere(g.seas[t.zone]?.name ?? 'sea') : `in harbour at ${t.port}`);

/** The order's own rule, the same for the player and the computer's admirals: why `f` cannot shadow `t` now, or null. */
function ruleOut(g: Game, f: Fleet, t: Fleet | undefined): string | null {
  if (!shipCount(f.ships)) return 'There is no such fleet.';
  if (!shadowShips(f)) return "Shadowing is cruisers' work: a fleet of cruisers, with destroyers or without, and no battleship or carrier. Split the cruisers off first.";
  if (!t || t.nat === f.nat || !shipCount(t.ships) || fleetKnown(g, t, f.nat) === 'none') return 'No such enemy fleet is known.';
  if (!atWar(g, f.nat, t.nat)) return `${g.nations[t.nat]?.name ?? t.nat} is not at war with ${g.nations[f.nat].name}.`;
  if (surfaceShips(t) <= 0) return 'Submarines run submerged: there is nothing on the surface to follow.';
  if (f.shadow === t.id) return `It shadows ${fleetName(t)} already.`;
  if (watchSea(g, f, t) < 0) return `${cap(fleetName(t))} is ${whereIs(g, t)}, out of its reach from ${f.port}.`;
  return null;
}

/** Why the player's fleet `id` cannot be set to shadow enemy fleet `target` now, or null if it can. Read-only. */
export function whyNotShadow(g: Game, id: number, target: number): string | null {
  if (!shadowOn(g)) return 'Shadowing is ordered only where that is switched on.';
  const f = g.fleets.find((x) => x.id === id);
  if (!f || f.nat !== g.human || !hisMonth(g)) return 'Only your own fleet can be given the order, in your own month.';
  return ruleOut(g, f, g.fleets.find((x) => x.id === target));
}

/** Sets `f` on `t`: it sails to the sea it watches from, on patrol, and has `t` in contact from now. */
function setOn(g: Game, f: Fleet, t: Fleet): boolean {
  const z = watchSea(g, f, t);
  if (z < 0 || !sail(g, f, z, 'patrol')) return false;
  f.shadow = t.id;
  (t.seen ??= {})[f.nat] = g.turn;
  return true;
}

/** Sets the player's fleet `id` to shadow enemy fleet `target`. His own order: his admirals leave it to its work. */
export function orderShadow(g: Game, id: number, target: number): boolean {
  if (whyNotShadow(g, id, target) !== null) return false;
  const f = g.fleets.find((x) => x.id === id)!, t = g.fleets.find((x) => x.id === target)!;
  if (!setOn(g, f, t)) return false;
  f.ordered = true;
  return true;
}

/** Ends a fleet's shadow: it stays where it is. False where it had none. */
export function stopShadow(f: Fleet): boolean {
  if (f.shadow === undefined) return false;
  delete f.shadow;
  return true;
}

/** The enemy fleets the player's fleet `id` can be set to shadow now. Empty while the switch is off. */
export function shadowTargets(g: Game, id: number): number[] {
  if (!shadowOn(g)) return [];
  return g.fleets.filter((t) => whyNotShadow(g, id, t.id) === null).map((t) => t.id);
}

/**
 * The shadows at the start of country `nat`'s month, every country's alike: each follows its quarry to the sea it
 * watches from and keeps it in contact; out of its reach, or gone, the shadow ends. A shadow of `nat`'s own reports
 * where its quarry is, in a line of news (the player's only: the computer's admirals read their own reports).
 */
export function shadowsFollow(g: Game, nat: string): void {
  if (!shadowOn(g)) return;
  for (const f of g.fleets) {
    if (f.shadow === undefined) continue;
    const own = f.nat === nat;
    const tell = (text: string): void => { if (own && f.nat === g.human) headline(g, 'info', text, f.nat); };
    const t = g.fleets.find((x) => x.id === f.shadow);
    if (!shipCount(f.ships) || !g.nations[f.nat]?.alive || !shadowShips(f)) { delete f.shadow; continue; }
    if (!t || !shipCount(t.ships) || surfaceShips(t) <= 0) {
      delete f.shadow;
      tell(`${cap(fleetName(f))} has nothing left to shadow: the fleet it followed is gone.`);
      continue;
    }
    if (!atWar(g, f.nat, t.nat)) { delete f.shadow; tell(`${cap(fleetName(f))} stops shadowing ${fleetName(t)}: the two countries are no longer at war.`); continue; }
    const z = watchSea(g, f, t);
    if (z < 0) {
      delete f.shadow;
      tell(`${cap(fleetName(f))} has lost contact with ${fleetName(t)}: last seen ${whereIs(g, t)}, out of its reach from ${f.port}.`);
      continue;
    }
    if (f.zone !== z || f.stance !== 'patrol') sail(g, f, z, 'patrol');
    (t.seen ??= {})[f.nat] = g.turn;
    tell(`${cap(fleetName(f))}, shadowing ${fleetName(t)}, reports it ${whereIs(g, t)}.`);
  }
}

/** True where the enemy fleets `met` are all shadows, and they keep their distance this time (seaBattles' draw). */
export const allShadows = (g: Game, met: Fleet[]): boolean => shadowOn(g) && met.length > 0 && met.every((f) => f.shadow !== undefined);

/**
 * The computer's admirals' shadows, at country `nat`'s month (after their other orders): a worn shadow comes home, and
 * while fewer than SHADOW.ai.most are out, one fit cruiser fleet is set on the strongest enemy fleet at sea they know of.
 */
export function aiShadows(g: Game, nat: string): void {
  if (!shadowOn(g) || nat === g.human || !g.nations[nat]?.atWar.length) return;
  const mine = g.fleets.filter((f) => f.nat === nat && shipCount(f.ships) > 0);
  for (const f of mine) if (f.shadow !== undefined && f.ready < SHADOW.ai.home) { delete f.shadow; sail(g, f, f.home, 'port'); }
  if (mine.filter((f) => f.shadow !== undefined).length >= SHADOW.ai.most) return;
  const free = mine.filter((f) => f.shadow === undefined && shadowShips(f) && f.ready >= SHADOW.ai.ready && f.stance !== 'escort' && f.duty !== 'raid');
  if (!free.length) return;
  const prey = g.fleets
    .filter((t) => atSea(t) && atWar(g, nat, t.nat) && surfaceShips(t) > 0 && fleetKnown(g, t, nat) !== 'none')
    .sort((a, b) => fleetPower(g, b) - fleetPower(g, a) || a.id - b.id);
  for (const t of prey) {
    const f = free.find((x) => ruleOut(g, x, t) === null);
    if (f && setOn(g, f, t)) return;
  }
}

/** The line for a shadow's page and panel: what it follows and where that is. Empty for any other fleet. */
export function shadowLine(g: Game, f: Fleet): string {
  if (!shadowOn(g) || f.shadow === undefined) return '';
  const t = g.fleets.find((x) => x.id === f.shadow);
  if (!t) return '';
  return `Shadowing ${fleetName(t)}: it reports it ${whereIs(g, t)}, and follows it wherever it goes in its reach.`;
}
