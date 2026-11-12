// Campaign difficulty, in the grand campaign only. Five levels named for ranks, Cadet to Field Marshal. The middle
// one, Colonel, is the game with nothing added: every number below is 1 and nothing is switched on. A level never
// touches the battle arithmetic. It changes what each side can afford (income, replacements) and how the computer's
// staffs think (their nerve, and on the two hardest levels bombing, landings and offensives timed together).
// The numbers live in data/campaign/difficulty.json. The level is kept in g.flags.level, so it is saved with the game.

import data from '../../data/campaign/difficulty.json';
import { allied, bloc } from './game';
import type { Game } from './types';

export interface Side { income: number; drafts: number }
export interface Rival extends Side { nerve: number; odds: number }
export interface Bombing { share: number; least: number; loss: number; rail: number }
export interface Landings { force: number; odds: number; risk: number; near: number; every: number; chance: number }
export interface Level {
  id: string; name: string; blurb: string; lines: string[];
  you: Side; rival: Rival;
  bombing?: Bombing; landings?: Landings; concert?: number;
}

export const DIFFICULTY = data as unknown as { start: string; levels: Level[] };
/** The level with nothing added: where a campaign starts, and what every game without a level is played at. */
export const FAIR = Math.max(0, DIFFICULTY.levels.findIndex((l) => l.id === DIFFICULTY.start));

/** A war run with nobody at the table can still be umpired as if one nation were the player's: for trials of the levels. */
export const asIf = new WeakMap<Game, string>();
/** The nation the level is measured against. */
export const seat = (g: Game): string | null => g.human ?? asIf.get(g) ?? null;

/** The level a game is played at, counted from 0 for the easiest. Outside the grand campaign it is always the fair one. */
export const levelIndex = (g: Game): number => (g.flags.open === 1 ? g.flags.level ?? FAIR : FAIR);
export const levelOf = (g: Game): Level => DIFFICULTY.levels[levelIndex(g)] ?? DIFFICULTY.levels[FAIR];

/** Why the level cannot be set, or null if it can. */
export function whyNotLevel(g: Game, id: string | number): string | null {
  if (g.flags.open !== 1) return 'A historical scenario is always played as history fought it. Difficulty belongs to the grand campaign.';
  const k = typeof id === 'number' ? id : DIFFICULTY.levels.findIndex((l) => l.id === id);
  return DIFFICULTY.levels[k] ? null : 'There is no such level.';
}

/** Sets the level, by its id or its place in the list. It takes hold at once and is saved with the game. */
export function setLevel(g: Game, id: string | number): boolean {
  if (whyNotLevel(g, id)) return false;
  const k = typeof id === 'number' ? id : DIFFICULTY.levels.findIndex((l) => l.id === id);
  if (k === FAIR) delete g.flags.level; else g.flags.level = k;
  return true;
}

/** The camp a nation stands in or leans toward, if it has one. */
const camp = (g: Game, id: string): 'allies' | 'axis' | null => {
  const n = g.nations[id];
  if (!n) return null;
  return bloc(n) ?? (n.align === 'leaning_axis' ? 'axis' : n.align === 'leaning_allies' ? 'allies' : null);
};
/** True for the player's own nation, its allies, and neutrals that lean to its side: none of them is a rival. */
const friend = (g: Game, nat: string, me: string): boolean => nat === me || allied(g, nat, me) || (camp(g, nat) !== null && camp(g, nat) === camp(g, me));

/** True for a computer-run nation the level bears on: every one that is not the player's and not on the player's side. */
export function rival(g: Game, nat: string): boolean {
  if (g.flags.open !== 1 || g.flags.level === undefined) return false;
  const me = seat(g);
  return me !== null && !friend(g, nat, me);
}

const side = (g: Game, nat: string): Side | null => {
  if (g.flags.open !== 1 || g.flags.level === undefined) return null;
  const me = seat(g);
  if (me === null) return null;
  return nat === me ? levelOf(g).you : friend(g, nat, me) ? null : levelOf(g).rival;
};

/** What the level does to a nation's earnings each month: 1 for no change. */
export const incomeMul = (g: Game, nat: string): number => side(g, nat)?.income ?? 1;
/** What the level does to how fast a nation's losses are made good: 1 for no change. */
export const draftsMul = (g: Game, nat: string): number => side(g, nat)?.drafts ?? 1;
/** What the level does to a rival staff's nerve: the ratios at which it opens and keeps up an offensive, and the odds it wants. */
export const nerveOf = (g: Game, nat: string): { nerve: number; odds: number } => (rival(g, nat) ? levelOf(g).rival : { nerve: 1, odds: 1 });
/** The rules a rival's air staff bombs the player by, if this level has it bomb at all. */
export const bombingFor = (g: Game, nat: string): Bombing | null => (rival(g, nat) ? levelOf(g).bombing ?? null : null);
/** The rules a rival lands on the player's coasts by, if this level has it land at all. */
export const landingsFor = (g: Game, nat: string): Landings | null => (rival(g, nat) ? levelOf(g).landings ?? null : null);
/** The share of its usual ratio at which a rival joins an ally's offensive against the player: 1 when it does not. */
export const concertFor = (g: Game, nat: string): number => (rival(g, nat) ? levelOf(g).concert ?? 1 : 1);
