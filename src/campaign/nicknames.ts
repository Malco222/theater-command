// Earned nicknames (release 2; Brian's yes of 23 November 2026, idea 2): a corps that wins several hard fights is
// given a name by its own army, in its nation's own style, and the name is shown on its flag.
//
// A hard fight is one won against the odds or at a price (data/campaign/nicknames.json 'hard'). The count is kept in
// the corps' record ('hard'); the name comes once the count reaches 'need'. Every nation's corps earn names alike, and
// a name changes no number for anyone: it is a name on a flag and in the record. Every name is our own, and none is a
// real formation's nickname ('blocked', kept out by a test).
//
// The grand campaign's flag 'nicknames', set as a campaign opens where stage.json switches it on (or by the player at
// any time: a name is only a name). Off, nothing here is read and every war is the war without it.

import data from '../../data/campaign/nicknames.json';
import { Rng, hashString } from '../core/rng';
import type { Corps, Game } from './types';

interface Style { any: string[]; arm: string[]; place: string[] }

export const NICKNAMES = data as unknown as {
  hard: { attackOdds: number; holdOdds: number; bloody: number; walkover: number };
  need: number;
  most: number;
  nations: Record<string, string>;
  styles: Record<string, Style>;
  blocked: string[];
};

/** True where this campaign gives earned nicknames. */
export const nicknamesOn = (g: Game): boolean => g.flags.open === 1 && g.flags.nicknames === 1;

/** Turns earned nicknames on or off. A name already given stays on its corps either way. */
export function setNicknames(g: Game, on: boolean): void {
  if (on) g.flags.nicknames = 1;
  else delete g.flags.nicknames;
}

/** The style a nation names its formations in: its own row, or the default. */
export const styleKey = (nat: string): string => (NICKNAMES.nations[nat] && NICKNAMES.styles[NICKNAMES.nations[nat]] ? NICKNAMES.nations[nat] : '_default');
const styleOf = (nat: string): Style => NICKNAMES.styles[styleKey(nat)];

const BLOCKED = NICKNAMES.blocked.map((b) => b.toLowerCase());
/** True for a name that is, or holds, a real formation's nickname. */
export const isBlocked = (name: string): boolean => { const n = name.toLowerCase(); return BLOCKED.some((b) => n.includes(b)); };

/**
 * Whether a battle won was a hard fight for one corps on the winning side: an attack carried at poor odds, a line held
 * against equal or better odds, or either won at a heavy price. An attack at walkover odds never is.
 */
export function hardWin(side: 'att' | 'def', preOdds: number, lossFrac: number): boolean {
  const H = NICKNAMES.hard;
  if (side === 'att') return preOdds < H.walkover && (preOdds <= H.attackOdds || lossFrac >= H.bloody);
  return preOdds >= H.holdOdds || lossFrac >= H.bloody;
}

/** The names a nation's units carry now, or carried to the memorial wall: no second unit is given one of them. */
export function takenNames(g: Game, nat: string): Set<string> {
  const taken = new Set<string>();
  for (const c of g.corps) if (c.nat === nat && c.nick) taken.add(c.nick);
  for (const f of g.airFleets) if (f.nat === nat && f.nick) taken.add(f.nick);
  for (const f of g.fleets) if (f.nat === nat && f.nick) taken.add(f.nick);
  for (const f of g.stories.fallen) if (f.nat === nat && f.nick) taken.add(f.nick);
  return taken;
}

/** Whether a nation has room for one more name: fewer than 'most' of its corps in the field carry one (always room for the first). */
export function roomForName(g: Game, nat: string): boolean {
  let field = 0, named = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort') { field++; if (c.nick) named++; }
  return named < Math.max(1, Math.floor(field * NICKNAMES.most));
}

/**
 * The name a corps' own army gives it after its hard fights: one in its nation's style that none of the nation's units
 * carries, for an armoured corps from the armour names first, sometimes after the place of the fight that made it.
 * The same corps in the same war is always given the same name. Undefined when the style has none left.
 */
export function earnedName(g: Game, c: Corps, place: string): string | undefined {
  const s = styleOf(c.nat);
  const taken = takenNames(g, c.nat);
  const ok = (n: string): boolean => !taken.has(n) && !isBlocked(n);
  const placed = place ? s.place.map((t) => t.replace('{place}', place)).filter(ok) : [];
  const first = (c.kind === 'arm' ? s.arm : s.any).filter(ok);
  const rng = new Rng(hashString(`hardnick:${g.seed}:${c.nat}:${c.name}`));
  // About one name in four is after the place.
  if (placed.length && (rng.next() < 0.25 || !first.length)) return rng.pick(placed);
  if (first.length) return rng.pick(first);
  const rest = (c.kind === 'arm' ? s.any : s.arm).filter(ok);
  return rest.length ? rng.pick(rest) : undefined;
}
