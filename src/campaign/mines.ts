// Mines at sea (release 2; Brian, 24 November 2026 20:38Z, "yes to all", of the idea "Lay a minefield across a narrow
// sea or a harbour mouth. Enemy ships that sail through risk losing a ship each month until their minesweepers clear
// it, like a toll gate that charges in ships"). Behind its own switch (stage.json 'seaMines', the flag 'seaMines'): off,
// no field is laid or kept, nothing here is read, and every war is the war without it.
//
// A fleet with destroyers, cruisers or submarines (the minelayers) lays a field in a sea in its reach that is narrow
// (seas.json 'narrow') or lies off a harbour an enemy fleet sails from: once a month, and it sails there to lay it. At
// the start of each country's month each of its fleets at sea in an enemy's field, or sailing from a harbour on that
// sea, loses a ship MINES.hit times the field's strength of the time. At every month's end a field wears by MINES.decay,
// and each enemy fleet with destroyers at sea in it sweeps MINES.sweep more; under MINES.gone it is gone. A side knows
// its own and its allies' fields, and an enemy's once it has struck one or swept it.
//
// The same for every country: the computer's admirals lay fields of their own (aiMines), at most MINES.ai.most at a time,
// off the harbour of the strongest enemy fleet they reach, or else across a narrow sea an enemy sails. Every draw is the
// campaign's seed's own, never the game's dice.

import data from '../../data/campaign/mines.json';
import { Rng, hashString } from '../core/rng';
import { allied, atWar, headline } from './game';
import { NAVY, atSea, fleetName, fleetPower, fleetReach, inPassage, passageWhy, sail, shipCount } from './navy';
import { seaWhere } from './sea-words';
import { loseStars, mendStars } from './stars';
import type { Fleet, Game, ShipClass } from './types';
import { noteFight } from './voyage';

export const MINES = data as unknown as { lay: number; hit: number; decay: number; sweep: number; gone: number; ai: { most: number; chance: number; ready: number } };

/** One field: the sea (`z`), whose it is, its strength (`s`, MINES.lay new), the month laid (`t`), and who else knows of it. */
export type Minefield = NonNullable<Game['mines']>[number];

/** True where fleets may lay mines at sea: the grand campaign, with the switch on. */
export const seaMinesOn = (g: Game): boolean => g.flags.open === 1 && g.flags.seaMines === 1;

/** Turns mines at sea on or off in a campaign under way. Off, every field is gone. */
export function setSeaMines(g: Game, on: boolean): void {
  if (on) g.flags.seaMines = 1;
  else {
    delete g.flags.seaMines;
    delete g.mines;
  }
}

const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
const hisMonth = (g: Game): boolean => g.human !== null && g.order[g.phase] === g.human;
const seaWords = (g: Game, z: number): string => seaWhere(g.seas[z]?.name ?? 'sea');
const ONE: Record<ShipClass, string> = { bb: 'battleship', cv: 'carrier', ca: 'cruiser', dd: 'destroyer', ss: 'submarine' };
const one = (c: ShipClass): string => (NAVY.classes as unknown as Record<ShipClass, { one?: string }>)[c]?.one ?? ONE[c];

/** True for a fleet that can lay mines: one with destroyers, cruisers or submarines. */
export const minelayer = (f: Fleet): boolean => f.ships.dd > 0 || f.ships.ca > 0 || f.ships.ss > 0;

/** The seas an enemy of `nat`'s fleets sail from: the seas off their harbours. */
function enemyHarbours(g: Game, nat: string): Set<number> {
  const out = new Set<number>();
  for (const e of g.fleets) if (shipCount(e.ships) > 0 && atWar(g, nat, e.nat)) out.add(e.home);
  return out;
}

/** Whether `nat` may lay a field in sea `z`: a narrow sea, or one off a harbour an enemy fleet sails from. */
const mineable = (g: Game, nat: string, z: number): boolean => !!g.seas[z]?.narrow || enemyHarbours(g, nat).has(z);

/** The order's own rule, the same for the player and the computer's admirals: why `f` cannot lay a field in `z`, or null. */
function ruleOut(g: Game, f: Fleet, z: number): string | null {
  if (!shipCount(f.ships)) return 'There is no such fleet.';
  // (A fleet in passage to a new base does nothing on its way: release 2, the far chart.)
  if (f.stance === 'passage') return passageWhy(g, f);
  if (!minelayer(f)) return 'Mines are laid by destroyers, cruisers or submarines: a fleet of battleships and carriers alone carries none.';
  if (!g.nations[f.nat]?.atWar.length) return 'Mines are laid only in a war.';
  if (f.mined === g.turn) return 'It has laid its mines this month already.';
  if (!g.seas[z]) return 'There is no such sea.';
  if (!fleetReach(g, f).includes(z)) return `${cap(seaWords(g, z))} is out of its reach from ${f.port}.`;
  if (!mineable(g, f.nat, z)) return 'Mines are laid across a narrow sea or off a harbour an enemy fleet sails from: this is open water.';
  return null;
}

/** Why the player's fleet `id` cannot lay a field in sea `z` now, or null if it can. Read-only. */
export function whyNotMines(g: Game, id: number, z: number): string | null {
  if (!seaMinesOn(g)) return 'Mines at sea are laid only where that is switched on.';
  const f = g.fleets.find((x) => x.id === id);
  if (!f || f.nat !== g.human || !hisMonth(g)) return 'Only your own fleet can be given the order, in your own month.';
  return ruleOut(g, f, z);
}

/** Lays `f`'s field in `z`: it sails there on patrol, and the field is new, or its own field there is made whole again. */
function lay(g: Game, f: Fleet, z: number): boolean {
  if (!sail(g, f, z, 'patrol')) return false;
  const list = (g.mines ??= []);
  const m = list.find((x) => x.z === z && x.nat === f.nat);
  if (m) { m.s = MINES.lay; m.t = g.turn; } else list.push({ z, nat: f.nat, s: MINES.lay, t: g.turn });
  f.mined = g.turn;
  return true;
}

/** The player's fleet `id` lays a field in sea `z`. His own order: his admirals leave the fleet where it went. */
export function orderMines(g: Game, id: number, z: number): boolean {
  if (whyNotMines(g, id, z) !== null) return false;
  const f = g.fleets.find((x) => x.id === id)!;
  if (!lay(g, f, z)) return false;
  f.ordered = true;
  return true;
}

/** The seas the player's fleet `id` can lay a field in now. Empty while the switch is off. */
export function mineSeas(g: Game, id: number): number[] {
  if (!seaMinesOn(g)) return [];
  const f = g.fleets.find((x) => x.id === id);
  if (!f) return [];
  return fleetReach(g, f).filter((z) => whyNotMines(g, id, z) === null);
}

/** Whether `viewer` knows of field `m`: its own side's, or an enemy's it has struck or swept. */
export const fieldKnown = (g: Game, m: Minefield, viewer: string): boolean => m.nat === viewer || allied(g, m.nat, viewer) || !!m.known?.includes(viewer);

const learn = (m: Minefield, nat: string): void => { if (!(m.known ??= []).includes(nat)) m.known.push(nat); };

/** One ship of `f` goes down, drawn by the ships it has: the class sunk. */
function sinkOne(f: Fleet, roll: number): ShipClass | null {
  const total = shipCount(f.ships);
  if (!total) return null;
  let k = roll * total;
  for (const c of ['bb', 'cv', 'ca', 'dd', 'ss'] as ShipClass[]) {
    if (k < f.ships[c]) {
      f.ships[c] -= 1;
      f.lost[c] += 1;
      loseStars(f, c, 1);
      mendStars(f);
      return c;
    }
    k -= f.ships[c];
  }
  return null;
}

/**
 * Country `nat`'s month opens: each of its fleets at sea in an enemy's field, or sailing from a harbour on its sea, may
 * strike a mine, and loses a ship if it does. The news tells the player of his own losses and of what his fields sink.
 */
export function minesStrike(g: Game, nat: string): void {
  if (!seaMinesOn(g) || !g.mines?.length) return;
  for (const m of g.mines) {
    if (m.s <= 0 || !atWar(g, m.nat, nat)) continue;
    for (const f of g.fleets) {
      // (A fleet in passage meets a field only on the sea it is on, not off a harbour it has yet to reach: release 2.)
      if (f.nat !== nat || !shipCount(f.ships) || !atSea(f) || (f.zone !== m.z && (inPassage(f) || f.home !== m.z))) continue;
      const rng = new Rng(hashString(`mine:${g.seed}:${g.turn}:${m.z}:${m.nat}:${f.id}`));
      if (rng.next() >= MINES.hit * m.s) continue;
      const c = sinkOne(f, rng.next());
      if (!c) continue;
      learn(m, f.nat);
      const layer = g.nations[m.nat]?.adj ?? m.nat, where = seaWords(g, m.z);
      noteFight(g, f, `Lost a ${one(c)} to ${/^[aeiou]/i.test(layer) ? 'an' : 'a'} ${layer} minefield ${where}.`);
      if (f.nat === g.human) headline(g, 'battle', `${cap(fleetName(f))} loses a ${one(c)} to ${/^[aeiou]/i.test(layer) ? 'an' : 'a'} ${layer} minefield ${where}.`, f.nat);
      else if (m.nat === g.human || (g.human && allied(g, m.nat, g.human) && m.nat !== f.nat)) {
        const whose = m.nat === g.human ? 'Your' : `The ${layer}`;
        headline(g, 'battle', `${whose} minefield ${where} sinks a ${g.nations[f.nat]?.adj ?? f.nat} ${one(c)} of ${fleetName(f)}.`, m.nat);
      }
    }
  }
}

/**
 * The month's end: every field wears, and each enemy fleet with destroyers at sea in it sweeps it, and knows of it from
 * then. A field worn under MINES.gone is gone, with a line of news for the player where it was his.
 */
export function minesMonth(g: Game): void {
  if (!seaMinesOn(g) || !g.mines?.length) return;
  for (const m of g.mines) {
    let cut = MINES.decay;
    for (const f of g.fleets) {
      // (A fleet in passage sweeps nothing on its way: it only crosses the sea. Release 2, the far chart.)
      if (!shipCount(f.ships) || f.ships.dd <= 0 || !atSea(f) || inPassage(f) || f.zone !== m.z || !atWar(g, f.nat, m.nat)) continue;
      cut += MINES.sweep;
      const knew = fieldKnown(g, m, f.nat);
      learn(m, f.nat);
      if (f.nat === g.human && !knew) headline(g, 'info', `The destroyers of ${fleetName(f)} find ${/^[aeiou]/i.test(g.nations[m.nat]?.adj ?? '') ? 'an' : 'a'} ${g.nations[m.nat]?.adj ?? m.nat} minefield ${seaWords(g, m.z)} and begin to sweep it.`, f.nat);
    }
    m.s = Math.max(0, m.s - cut);
  }
  const gone = g.mines.filter((m) => m.s < MINES.gone);
  for (const m of gone) {
    if (m.nat === g.human) headline(g, 'info', `Your minefield ${seaWords(g, m.z)} is spent: the mines have broken loose, sunk or been swept.`, m.nat);
    else if (g.human && m.known?.includes(g.human)) headline(g, 'info', `The ${g.nations[m.nat]?.adj ?? m.nat} minefield ${seaWords(g, m.z)} is cleared.`, g.human);
  }
  g.mines = g.mines.filter((m) => m.s >= MINES.gone);
}

/**
 * The computer's admirals' mines, at country `nat`'s month (after their other orders): while fewer than MINES.ai.most of
 * its fields are laid, in MINES.ai.chance of the months, one fit minelayer lays a field off the harbour of the strongest
 * enemy fleet it reaches, or else across a narrow sea where an enemy fleet is at sea.
 */
export function aiMines(g: Game, nat: string): void {
  if (!seaMinesOn(g) || nat === g.human || !g.nations[nat]?.atWar.length) return;
  if ((g.mines ?? []).filter((m) => m.nat === nat).length >= MINES.ai.most) return;
  if (new Rng(hashString(`mineai:${g.seed}:${g.turn}:${nat}`)).next() >= MINES.ai.chance) return;
  const layers = g.fleets
    .filter((f) => f.nat === nat && minelayer(f) && f.shadow === undefined && !f.tied && f.stance !== 'escort' && f.ready >= MINES.ai.ready && f.mined !== g.turn)
    .sort((a, b) => b.ready - a.ready || a.id - b.id);
  if (!layers.length) return;
  const have = new Set((g.mines ?? []).filter((m) => m.nat === nat).map((m) => m.z));
  const foes = g.fleets.filter((e) => shipCount(e.ships) > 0 && atWar(g, nat, e.nat)).sort((a, b) => fleetPower(g, b) - fleetPower(g, a) || a.id - b.id);
  const want: number[] = [];
  for (const e of foes) if (!want.includes(e.home)) want.push(e.home);
  for (const e of foes) if (atSea(e) && g.seas[e.zone]?.narrow && !want.includes(e.zone)) want.push(e.zone);
  for (const z of want) {
    if (have.has(z)) continue;
    const f = layers.find((x) => ruleOut(g, x, z) === null);
    if (f && lay(g, f, z)) return;
  }
}

/** The fields `viewer` knows of, for the map and the sea's tip: the sea, whose, whether its own side's, and a word for its strength. */
export function minesSeen(g: Game, viewer: string | null = g.human): { zone: number; nat: string; own: boolean; word: string }[] {
  if (!seaMinesOn(g) || !viewer) return [];
  return (g.mines ?? [])
    .filter((m) => fieldKnown(g, m, viewer))
    .map((m) => ({ zone: m.z, nat: m.nat, own: m.nat === viewer || allied(g, m.nat, viewer), word: m.s >= 0.66 ? 'thick' : m.s >= 0.33 ? 'thinning' : 'thin' }));
}

/** The line for a fleet's panel where it lies in, or sails from, a field of an enemy's it knows of; '' otherwise. */
export function mineLine(g: Game, f: Fleet): string {
  if (!seaMinesOn(g) || !atSea(f)) return '';
  // (As minesStrike reads it: a fleet in passage meets a field only on the sea it is on.)
  const m = (g.mines ?? []).find((x) => (x.z === f.zone || (!inPassage(f) && x.z === f.home)) && atWar(g, x.nat, f.nat) && fieldKnown(g, x, f.nat));
  if (!m) return '';
  return `${cap(g.nations[m.nat]?.adj ?? m.nat)} mines ${seaWords(g, m.z)}: it may lose a ship each month it sails there, until destroyers sweep them.`;
}
