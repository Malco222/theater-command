// How a computer's fighter defence meets a raid: its style (the tactics panel's air brief, item 5; the design director's
// ruling 239). A style is a temperament, not a weapon: the raid card has priced the fight, and a style moves only where
// the defence's weight falls, never how much (217, b418). Two numbers on one plan, never a script: the share of the
// defence sent at the raid's escort, and how many bands above the raid the staff climbs when radar warns it in time.
// The numbers are data/battle/air-styles.json's. Nothing here reads who the enemy is, the theatre or the date (Brian,
// 19 November: a sandbox to make your own history); a nation is named only in the lean table, at stops 1 to 3.
//
// A pure module: the ticket reads a side's style from the game without changing it (ticket.ts), the battle reads it off
// the ticket (sim.ts).

import data from '../../../data/battle/air-styles.json';
import { stopOf } from '../../campaign/history';
import { TECH, techRules } from '../../campaign/tech';
import type { Game } from '../../campaign/types';
import type { ForceTicket } from '../core/types';
import type { Role } from './doctrine';

export type DefenceStyle = 'bold' | 'methodical' | 'mass';
/** A computer defence's group: sent at the raid's escort, or at its bombers. */
export type DefRole = 'escort' | 'bombers';

/** A computer defence's style on the ticket (AirPlace.defence): its name and its two numbers. */
export interface Defence {
  style: DefenceStyle;
  /** The share of the defence's weight dealt to the raid's escort, in whole groups. */
  escort: number;
  /** Bands above the raid's highest box the staff climbs to when radar warns it in time (drawn, never priced). */
  climb: number;
}

interface StylesData {
  styles: Record<DefenceStyle, { escort: number; climb: number }>;
  cap: { ahead: number };
  pick: { raw: number; escort: number; free: number };
  lean: Record<string, DefenceStyle>;
  path: Record<string, DefenceStyle[]>;
  grow: { first: { xp: number; tech: string }; second: { xp: number; tech: string } };
}

export const STYLES = data as unknown as StylesData;

export const defenceOf = (style: DefenceStyle): Defence => ({ style, ...STYLES.styles[style] });

/**
 * Whether a nation holds a research, read as doctrine.ts designYear reads it: the campaign's own list where the grand
 * campaign's research rules run, never through techOf (it opens the nation's research and so writes to the game).
 */
export function holds(g: Game, nat: string, id: string): boolean {
  const n = g.nations[nat];
  if (!n || !techRules(g)) return false;
  return (n.tech?.done ?? TECH.start[nat] ?? []).includes(id);
}

/**
 * Leaders' experience: the aircraft-weighted mean of the groups' xp (ticket.ts force: leader level / 3, so 0.33 at
 * level 1 to 1.67 at level 5, and 0 for a group with no named leader).
 */
export function experience(forces: ForceTicket[]): number {
  const n = forces.reduce((t, f) => t + f.men, 0);
  return n > 0 ? forces.reduce((t, f) => t + f.men * f.xp, 0) / n : 0;
}

/** What a defence's style is read from: its leaders' experience, its research, and the escort's weight against its own. */
export interface DefenceFacts {
  xp: number;
  /** Holding early-warning radar, and fighter control (tech.json earlywarning, gci). */
  warning: boolean;
  gci: boolean;
  /** The raid's escort and the defence, by the raid card's weights (assessAir's Epow and Dpow). */
  escort: number;
  defence: number;
}

/** Raw: no named leaders to speak of, and no early warning. */
const raw = (f: DefenceFacts): boolean => f.xp < STYLES.pick.raw && !f.warning;

/**
 * Stops 1 to 3: the nation's lean (the air column of ruling 239's table), grown along its path by its leaders and its
 * research, never by date. A nation not in the table flies methodical, or mass while raw.
 */
export function leanOf(nat: string, f: DefenceFacts): DefenceStyle {
  const path = STYLES.path[nat], G = STYLES.grow;
  if (path) {
    const first = f.xp >= G.first.xp || f.warning, second = f.xp >= G.second.xp && f.gci;
    return path[Math.min(path.length - 1, second ? 2 : first ? 1 : 0)];
  }
  return STYLES.lean[nat] ?? (raw(f) ? 'mass' : STYLES.lean._default);
}

/** Stops 4 and 5: from the ticket's facts only, never the nation. */
export function pickOf(f: DefenceFacts): DefenceStyle {
  if (raw(f)) return 'mass';
  return f.escort >= STYLES.pick.escort * f.defence ? 'bold' : 'methodical';
}

/** A computer defence's style at the history dial's stop. */
export const styleOf = (stop: number, nat: string, f: DefenceFacts): DefenceStyle => (stop >= 4 ? pickOf(f) : leanOf(nat, f));

/**
 * The escortRoles hook (the brief's one light hook; doctrine.ts stays as it is): at stops 4 and 5 a computer raid's
 * escort orders its roles by its weight against the defence, never by date. An escort weighing at least pick.free x the
 * defence sweeps first; one outweighed keeps close.
 */
export const rolesOf = (escort: number, defence: number): Role[] => (escort >= STYLES.pick.free * defence ? ['sweep', 'top', 'close'] : ['close', 'top', 'sweep']);

/**
 * A fought raid's two style fields (AirPlace.defence and AirPlace.roles), from the game, which it never changes. The
 * defence's style only where the computer flies the defence (a human defender's opening plan stays today's); the
 * escort's roles only at stops 4 and 5 and only for a computer raid.
 */
export function airStyles(g: Game, raid: { nat: string; forces: ForceTicket[] }, def: { nat: string; forces: ForceTicket[] }, w: { escort: number; defence: number }): { defence?: Defence; roles?: Record<string, Role[]> } {
  const stop = stopOf(g), out: { defence?: Defence; roles?: Record<string, Role[]> } = {};
  if (g.human !== def.nat && def.forces.length) {
    const f: DefenceFacts = { xp: experience(def.forces), warning: holds(g, def.nat, STYLES.grow.first.tech), gci: holds(g, def.nat, STYLES.grow.second.tech), ...w };
    out.defence = defenceOf(styleOf(stop, def.nat, f));
  }
  const escorted = raid.forces.some((x) => x.kind === 'ftr') && raid.forces.some((x) => x.kind === 'bmr');
  if (stop >= 4 && g.human !== raid.nat && escorted) {
    const order = rolesOf(w.escort, w.defence);
    out.roles = Object.fromEntries([...new Set([raid.nat, ...raid.forces.map((x) => x.nat)])].map((n) => [n, order]));
  }
  return out;
}
