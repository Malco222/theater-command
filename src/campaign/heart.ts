/**
 * Junior partners, for the sandbox stops (4 and 5) of the history dial in the open grand campaign: the historian's
 * section 7, ruled in by the design director (20 November 2026, 07:40Z) under the one rule, a reform fires only from
 * its own cause in that nation's war. Any nation fighting beside a far stronger ally may lose heart from its own
 * causes; then, if it is a computer's, its government may fall with an enemy on its soil, and it may sign an
 * armistice. These stand in place of the record's dated loss of heart, fall of Mussolini and Italian armistice there
 * (diplomacy.ts skips those). Causes, numbers and words: heart.json. Stops 1 to 3 never read this.
 */
import data from '../../data/campaign/heart.json';
import { allied, atWar, headline, theName } from './game';
import { landMonths, peakCorps, reformsOn, trade } from './reforms';
import { signArmistice } from './diplomacy';
import type { BattleReport, Game, Nation, Region } from './types';

interface Heart {
  senior: number; land: number; losses: number; army: number; quality: number; regime: number; armistice: number;
  text: { heart: string; regime: string };
}
export const HEART = data as unknown as Heart;

/** The flags: 'event:reform:heart:NAT' the month it lost heart; 'event:regime:NAT' the month its government fell. */
export const lostHeart = (g: Game, nat: string): boolean => g.flags[`event:reform:heart:${nat}`] !== undefined;
export const regimeFell = (g: Game, nat: string): boolean => g.flags[`event:regime:${nat}`] !== undefined;

/** A nation's army rating on the first month these stops saw it ('q0:NAT'): the loss of heart is reckoned from it. */
const firstQuality = (g: Game, n: Nation): number => g.flags[`q0:${n.id}`] ?? n.quality;

/**
 * The senior partner a nation fights beside: a co-belligerent (at war with one of its enemies, never with it) whose
 * peak field army is at least 'senior' times its own; the strongest such, where there are several. None for the
 * strongest of any coalition, which is nobody's junior.
 */
export function seniorOf(g: Game, nat: string): string | undefined {
  const n = g.nations[nat];
  if (!n?.alive) return undefined;
  const foes = n.atWar.filter((e) => g.nations[e]?.alive);
  const mine = Math.max(1, peakCorps(g, nat));
  let best: string | undefined, most = 0;
  for (const x of Object.values(g.nations)) {
    if (!x.alive || x.id === nat || atWar(g, nat, x.id) || !x.atWar.some((e) => foes.includes(e))) continue;
    const p = peakCorps(g, x.id);
    if (p >= HEART.senior * mine && p > most) { best = x.id; most = p; }
  }
  return best;
}

/** Its home provinces: its own ground, not overseas. */
const homeOf = (g: Game, nat: string): Region[] => g.regions.filter((r) => r.owner === nat && !r.overseas);
/** Whether an enemy holds one of its home provinces. */
const enemyAtHome = (g: Game, nat: string): boolean => homeOf(g, nat).some((r) => atWar(g, nat, r.ctrl));

/** Every overseas province it held on its first day is out of its own and its allies' hands (false where it had none). */
function overseasLost(g: Game, nat: string): boolean {
  const over = g.regions.filter((r) => (r.first ?? r.owner) === nat && r.overseas);
  return over.length > 0 && over.every((r) => !allied(g, nat, r.ctrl));
}

/**
 * A land battle was fought: a nation that lost 'army' divisions or more in it, destroyed or taken, while its senior
 * partner fought on the same side, has lost a whole army at its senior partner's side ('ha:NAT', the first month).
 */
export function noteArmyLost(g: Game, rep: BattleReport): void {
  if (!reformsOn(g)) return;
  for (const side of [rep.attackers, rep.defenders]) {
    const lost: Record<string, number> = {};
    for (const u of side) if (!u.guard && (u.fate === 'destroyed' || u.fate === 'surrendered')) lost[u.nat] = (lost[u.nat] ?? 0) + u.divs;
    for (const [nat, divs] of Object.entries(lost)) {
      if (divs < HEART.army || g.flags[`ha:${nat}`] !== undefined) continue;
      const senior = seniorOf(g, nat);
      if (senior && side.some((u) => u.nat === senior)) g.flags[`ha:${nat}`] = g.turn;
    }
  }
}

/** Why a junior partner loses heart, or null while it has not. */
export function heartCause(g: Game, nat: string): 'losses' | 'overseas' | 'army' | null {
  if (landMonths(g, nat) < HEART.land) return null;
  const peak = peakCorps(g, nat);
  if (peak > 0 && trade(g, nat, 0).lost >= HEART.losses * peak) return 'losses';
  if (overseasLost(g, nat)) return 'overseas';
  if (g.flags[`ha:${nat}`] !== undefined) return 'army';
  return null;
}

const fill = (s: string, n: Nation): string => s.replace('{adj}', n.adj).replace('{the}', theName(n));

/**
 * Each month at the sandbox stops (after the earned reforms, which keep the peaks): every junior partner at war is
 * weighed for the loss of heart; a computer's that has lost it, for the fall of its government and then the armistice.
 */
export function heartMonth(g: Game): void {
  if (!reformsOn(g)) return;
  for (const n of Object.values(g.nations)) if (n.alive && g.flags[`q0:${n.id}`] === undefined) g.flags[`q0:${n.id}`] = n.quality;
  for (const n of Object.values(g.nations)) {
    if (!n.alive || !n.atWar.some((e) => g.nations[e]?.alive)) continue;
    if (!lostHeart(g, n.id)) {
      if (!seniorOf(g, n.id) || !heartCause(g, n.id)) continue;
      n.quality = Math.min(n.quality, Math.round(HEART.quality * firstQuality(g, n) * 100) / 100);
      g.flags[`event:reform:heart:${n.id}`] = g.turn;
      headline(g, 'event', fill(HEART.text.heart, n), n.id);
    }
    // The government and the armistice are the computer's alone: the player's own country stands or falls by the
    // cabinet's rules (ruling 99).
    if (n.id === g.human) continue;
    const home = enemyAtHome(g, n.id), k = `hh:${n.id}`;
    if (!regimeFell(g, n.id)) {
      if (!home) { delete g.flags[k]; continue; }
      g.flags[k] ??= g.turn;
      if (g.turn < g.flags[k] + HEART.regime) continue;
      g.flags[`event:regime:${n.id}`] = g.turn;
      headline(g, 'event', fill(HEART.text.regime, n), n.id);
      continue;
    }
    if (home && g.turn >= g.flags[`event:regime:${n.id}`] + HEART.armistice && signArmistice(g, n.id)) g.flags[`event:armistice:${n.id}`] = g.turn;
  }
}

/** The leavings in play at these stops (ruling 233, rider b612): a computer's junior partner that has lost heart can leave the war if an enemy takes any of its home provinces. */
export function heartLeavings(g: Game): { nation: string; grounds: string[] }[] {
  if (!reformsOn(g)) return [];
  return Object.values(g.nations)
    .filter((n) => n.alive && n.id !== g.human && lostHeart(g, n.id))
    .map((n) => ({ nation: n.id, grounds: homeOf(g, n.id).map((r) => r.name) }));
}
