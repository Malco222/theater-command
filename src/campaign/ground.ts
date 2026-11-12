// How the ground suits each arm, and what tanks and infantry do for each other.
//
// The classic rules already make a mountain or a city hard to take and blunt a tank attack in close country.
// The grand campaign goes further, from data/campaign/ground.json: every arm has ground it is at home on and
// ground it is not, the steepest mountains are closed to tanks altogether, and infantry and armour that fight
// together are each worth more than they are alone. The historical scenarios do not use any of this, so their
// battles are worked out exactly as they always were.
//
// Both interfaces read the same functions: the browser's province panel and Battle Ticket, and the 3D game.

import groundData from '../../data/campaign/ground.json';
import { KIND, type Corps, type Game, type Kind, type Region, type Terrain } from './types';

type ByKind = Partial<Record<Kind, Partial<Record<Terrain, number>>>>;

export const GROUND = groundData as unknown as {
  attack: ByKind;
  defence: ByKind;
  march: ByKind;
  steep: { regions: string[]; barred: Kind[]; attack: Partial<Record<Kind, number>>; march: Partial<Record<Kind, number>> };
  says: Record<Terrain | 'steep', string>;
  arms: {
    foot: Kind[]; perTank: number; infantry: number; armour: number; alone: Partial<Record<Terrain, number>>;
    defence: { infantry: number; alone: Partial<Record<Terrain, number>> };
  };
};

const STEEP = new Set(GROUND.steep.regions);
const BARRED = new Set<Kind>(GROUND.steep.barred);
const FOOT = new Set<Kind>(GROUND.arms.foot);

/** Whether these rules are in force: the grand campaign only. */
export const groundRules = (g: Game): boolean => g.flags.open === 1;

/** The steepest ground on the map: high Alps, Arctic fjords, the main ridge of the Caucasus. */
export const steep = (g: Game, r: Region): boolean => groundRules(g) && STEEP.has(r.name);

/** Whether a corps of this kind cannot go into this region at all. */
export const barredKind = (g: Game, kind: Kind, r: Region): boolean => BARRED.has(kind) && steep(g, r);

/** Whether this corps cannot go into this region at all: tanks and the steepest ground. */
export const barred = (g: Game, c: Corps, r: Region): boolean => barredKind(g, c.kind, r);

/** The plain reason a corps cannot enter, for either interface to show, or null if it can. */
export function whyBarred(g: Game, c: Corps, r: Region): string | null {
  return barred(g, c, r) ? `${r.name} is too steep for tanks. ${c.name} cannot go there: send infantry or mountain troops.` : null;
}

/** What this ground does to an attack by this kind of corps, beyond the classic rules. 1 is no change. */
export function groundAttack(g: Game, kind: Kind, r: Region): number {
  if (!groundRules(g)) return 1;
  return (GROUND.attack[kind]?.[r.terrain] ?? 1) * (STEEP.has(r.name) ? GROUND.steep.attack[kind] ?? 1 : 1);
}

/** What this ground does to a defence by this kind of corps, beyond the classic rules. 1 is no change. */
export function groundDefence(g: Game, kind: Kind, r: Region): number {
  return groundRules(g) ? GROUND.defence[kind]?.[r.terrain] ?? 1 : 1;
}

/** Extra moves this kind of corps spends to enter this ground on the advance. */
export function groundMarch(g: Game, kind: Kind, r: Region): number {
  if (!groundRules(g)) return 0;
  return (GROUND.march[kind]?.[r.terrain] ?? 0) + (STEEP.has(r.name) ? GROUND.steep.march[kind] ?? 0 : 0);
}

const pct = (m: number): string => `${Math.round(Math.abs(m - 1) * 100)}%`;
const NAME: Record<Kind, string> = { inf: 'infantry', mot: 'motorized troops', arm: 'tanks', cav: 'cavalry', mtn: 'mountain troops', fort: 'fortress troops', art: 'artillery' };
const KINDS = Object.keys(KIND) as Kind[];
const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);

/** What a ground rule does to one arm, as the interfaces show it. */
export interface GroundEffect { kind: Kind; attack: number; defence: number; march: number; barred: boolean }

/** How this region's ground treats each arm that it treats differently from the classic rules. */
export function groundEffects(g: Game, r: Region): GroundEffect[] {
  if (!groundRules(g)) return [];
  const out: GroundEffect[] = [];
  for (const kind of KINDS) {
    const e = { kind, attack: groundAttack(g, kind, r), defence: groundDefence(g, kind, r), march: groundMarch(g, kind, r), barred: barredKind(g, kind, r) };
    // The classic rule is part of the picture a player needs: tanks attack at 60% in mountain, city and marsh.
    if (kind === 'arm' && (r.terrain === 'mountain' || r.terrain === 'urban' || r.terrain === 'marsh')) e.attack *= 0.6;
    if (e.attack !== 1 || e.defence !== 1 || e.march || e.barred) out.push(e);
  }
  return out;
}

/** The province panel's notes on the ground, one plain sentence each. Empty outside the grand campaign. */
export function groundNotes(g: Game, r: Region): string[] {
  if (!groundRules(g)) return [];
  const out: string[] = [];
  for (const e of groundEffects(g, r)) {
    if (e.barred) { out.push(`${cap(NAME[e.kind])} cannot enter: the ground is too steep.`); continue; }
    const bits: string[] = [];
    if (e.attack !== 1) bits.push(`attack ${e.attack > 1 ? 'up' : 'down'} ${pct(e.attack)}`);
    if (e.defence !== 1) bits.push(`defence ${e.defence > 1 ? 'up' : 'down'} ${pct(e.defence)}`);
    if (e.march) bits.push(`${e.march === 1 ? 'one more move' : `${e.march} more moves`} to advance into it`);
    out.push(`${cap(NAME[e.kind])}: ${bits.join(', ')}.`);
  }
  const lone = GROUND.arms.alone[r.terrain];
  if (lone) out.push(`Tanks that attack here without infantry beside them lose a further ${pct(lone)}.`);
  return out;
}

/** The headline for this ground: what kind of country it is for an army. */
export const groundSays = (g: Game, r: Region): string => (groundRules(g) ? GROUND.says[steep(g, r) ? 'steep' : r.terrain] : '');

/** What tanks and infantry on one side of a battle do for each other. */
export interface Arms {
  /** Divisions of infantry of every kind, and of tanks, at the strength they stand at. */
  foot: number; tanks: number;
  /** How far the tanks there are go round the infantry (1 = every division has its support). */
  share: number;
  /** How far the infantry there is covers the tanks (1 = every armoured division has infantry with it). */
  cover: number;
  /** What each is worth because of the other. */
  infantry: number; armour: number;
  /** The part of the armour figure that comes from tanks fighting without infantry in close country (1 = none). */
  alone: number;
}

/** Whether this corps counts as infantry beside the tanks. */
export const isFoot = (c: Corps, defending = false): boolean => FOOT.has(c.kind) || (defending && c.kind === 'fort');

/** Works out combined arms for one side of a battle. Null where there are no tanks, or outside the grand campaign. */
export function combinedArms(g: Game, cs: Corps[], r: Region, defending: boolean): Arms | null {
  if (!groundRules(g)) return null;
  let foot = 0, tanks = 0;
  for (const c of cs) {
    if (!defending && barred(g, c, r)) continue;
    const w = c.divs * c.str;
    if (c.kind === 'arm') tanks += w; else if (isFoot(c, defending)) foot += w;
  }
  if (tanks <= 0) return null;
  const A = GROUND.arms;
  const share = foot > 0 ? Math.min(1, (tanks * A.perTank) / foot) : 0;
  const cover = Math.min(1, foot / tanks);
  const lone = (defending ? A.defence.alone : A.alone)[r.terrain];
  const alone = lone !== undefined && cover < 1 ? 1 - (1 - lone) * (1 - cover) : 1;
  return {
    foot, tanks, share, cover,
    infantry: 1 + (defending ? A.defence.infantry : A.infantry) * share,
    armour: (defending ? 1 : 1 + A.armour * cover) * alone,
    alone,
  };
}

/** One side's ground effects in a battle, as a sentence for the Battle Ticket. */
export function groundLine(g: Game, cs: Corps[], r: Region, defending: boolean): string {
  const seen = new Set<Kind>();
  const bits: string[] = [];
  for (const c of cs) {
    if (seen.has(c.kind)) continue;
    seen.add(c.kind);
    const m = defending ? groundDefence(g, c.kind, r) : groundAttack(g, c.kind, r);
    if (m !== 1) bits.push(`${NAME[c.kind]} ${defending ? 'defend' : 'attack'} ${pct(m)} ${m > 1 ? 'stronger' : 'weaker'}`);
  }
  return bits.join(', ');
}
