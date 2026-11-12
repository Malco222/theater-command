// Rivers and the bridges over them.
//
// The great rivers are drawn from data/campaign/rivers.mjs, and tools/build-rivers.mjs finds the crossings: pairs of
// neighbouring regions with a river between them. Each crossing has its bridges, standing or blown.
//
// In the grand campaign an attack across a river is harder, and while the bridges are down nothing on wheels or
// tracks can cross at all: tanks, lorries and guns wait on the bank until engineers rebuild them, and infantry
// goes over in boats at a heavy cost. Troops that lose the near bank blow the bridges behind them unless the
// enemy is quick enough to seize them standing. The historical scenarios have none of this.
//
// Both interfaces read the same functions: where the rivers run, which bridges are down, and why a corps
// cannot cross.

import riverData from '../generated/campaign-rivers.json';
import data from '../../data/campaign/engineering.json';
import { Rng, hashString } from '../core/rng';
import { KIND, type Corps, type Game, type Kind, type Region } from './types';
import { barred, groundRules, whyBarred } from './ground';
import { allied, atWar, headline } from './game';

export interface River { name: string; /** The course in map units. */ pts: [number, number][]; /** The same course as [latitude, longitude]. */ ll: [number, number][] }
/** A river between two neighbouring regions (a is the lower id), and where on the map its bridge stands. */
export interface Crossing { a: number; b: number; river: number; x: number; y: number }

export interface JobText { label: string; blurb: string }
export const ENGINEERING = data as unknown as {
  river: { attack: number };
  blown: { barred: Kind[]; attack: number; withEngineers: number; march: number };
  blow: { chance: number; unguarded: number; engineers: number; fastCaptor: number; aiOdds: number };
  mines: { most: number; attack: number; march: number };
  breach: { perDiv: number; most: number };
  works: { most: number };
  jobs: Record<string, JobText>;
  start: { nations: string[]; corps: number };
  ai: { most: number; reserve: number };
};

export const RIVERS = (riverData as unknown as { rivers: River[] }).rivers;
export const CROSSINGS: Crossing[] = (riverData as unknown as { crossings: number[][] }).crossings.map(([a, b, river, x, y]) => ({ a, b, river, x, y }));

const pair = (a: number, b: number): string => (a < b ? `${a}:${b}` : `${b}:${a}`);
const BY = new Map(CROSSINGS.map((c) => [pair(c.a, c.b), c]));
const AT = new Map<number, Crossing[]>();
for (const c of CROSSINGS) for (const r of [c.a, c.b]) (AT.get(r) ?? AT.set(r, []).get(r)!).push(c);
const WHEELS = new Set<Kind>(ENGINEERING.blown.barred);

/** The river crossing between two neighbouring regions, if there is one. Geography: the same in every game. */
export const crossingOf = (a: number, b: number): Crossing | null => BY.get(pair(a, b)) ?? null;
/** Every river crossing on a region's borders. */
export const crossingsAt = (r: number): Crossing[] => AT.get(r) ?? [];
export const riverName = (c: Crossing): string => RIVERS[c.river].name;
/** The region on the other bank. */
export const farBank = (c: Crossing, from: number): number => (c.a === from ? c.b : c.a);

const flag = (c: Crossing): string => `br:${c.a}:${c.b}`;
/** Whether the bridges of a crossing are down. Always standing outside the grand campaign. */
export const bridgeDown = (g: Game, c: Crossing): boolean => g.flags[flag(c)] !== undefined;
/** The month the bridges of a crossing were blown, or undefined while they stand. */
export const blownOn = (g: Game, c: Crossing): number | undefined => g.flags[flag(c)];
/** The river an army has to cross to go from one region to its neighbour: only where the river rules are in force. */
export const riverBetween = (g: Game, a: number, b: number): Crossing | null => (groundRules(g) ? crossingOf(a, b) : null);
/** Whether the way from one region to its neighbour is over bridges that are down. */
export function downBetween(g: Game, a: number, b: number): boolean {
  const c = riverBetween(g, a, b);
  return c !== null && bridgeDown(g, c);
}
/** Whether a corps of this kind cannot cross: anything on wheels or tracks, while the bridges are down. */
export const noCrossing = (g: Game, kind: Kind, from: number, to: number): boolean => WHEELS.has(kind) && downBetween(g, from, to);

/** Whether a corps cannot go from where it stands (or from another region) into a neighbour: ground too steep, or bridges down. */
export const blocked = (g: Game, c: Corps, to: Region, from = c.reg): boolean => barred(g, c, to) || noCrossing(g, c.kind, from, to.id);

/** The plain reason a corps cannot make that move, for either interface to show, or null if it can. */
export function whyBlocked(g: Game, c: Corps, to: Region, from = c.reg): string | null {
  const high = whyBarred(g, c, to);
  if (high) return high;
  if (!noCrossing(g, c.kind, from, to.id)) return null;
  const x = crossingOf(from, to.id)!;
  return `The bridges over the ${riverName(x)} between ${g.regions[from].name} and ${to.name} are down. ${c.name} cannot cross until engineers rebuild them.`;
}

function rebuild(g: Game, c: Crossing): void {
  delete g.flags[flag(c)];
}
function blow(g: Game, c: Crossing): void {
  g.flags[flag(c)] = g.turn;
}

/** Rebuilds every bridge that is down on a region's rivers. Returns the crossings rebuilt. */
export function rebuildAt(g: Game, region: number): Crossing[] {
  const done = crossingsAt(region).filter((c) => bridgeDown(g, c));
  done.forEach((c) => rebuild(g, c));
  return done;
}

/** Whether the news of a bridge matters to the player: it is theirs, their ally's or their enemy's. */
const concerns = (g: Game, ...nats: string[]): boolean => g.human !== null && g.nations[g.human].alive && nats.some((n) => n === g.human || allied(g, g.human!, n) || atWar(g, g.human!, n));

/** Why this nation cannot blow the bridges of a crossing from the bank it stands on, or null if it can. */
export function whyNotBlow(g: Game, nat: string, c: Crossing, from: number): string | null {
  if (!groundRules(g)) return 'There are no bridges to blow in this campaign.';
  const r = g.regions[from], name = riverName(c);
  if (c.a !== from && c.b !== from) return `The ${name} does not run past ${r.name}.`;
  if (bridgeDown(g, c)) return `The bridges over the ${name} are already down.`;
  if (!allied(g, nat, r.ctrl)) return `${r.name} is not in friendly hands.`;
  if (!r.units.some((u) => u.nat === nat)) return `There are no troops of yours at ${r.name} to set the charges.`;
  if (!g.nations[nat].atWar.length) return 'Nobody blows their own bridges in peacetime.';
  return null;
}

/** Blows the bridges of a crossing by order, from the bank the nation's troops stand on. */
export function blowBridges(g: Game, nat: string, c: Crossing, from: number): boolean {
  if (whyNotBlow(g, nat, c, from)) return false;
  blow(g, c);
  const far = g.regions[farBank(c, from)];
  if (concerns(g, nat, far.ctrl)) headline(g, 'battle', `${g.nations[nat].adj} troops blow the bridges over the ${riverName(c)} between ${g.regions[from].name} and ${far.name}.`, nat, from);
  return true;
}

/** The chance that the bridges go up when the near bank is lost: who holds the far bank, and who took this one. */
export function blowChance(g: Game, c: Crossing, taken: Region, by: string): number {
  const B = ENGINEERING.blow;
  const far = g.regions[farBank(c, taken.id)];
  const guards = far.units.filter((u) => !u.dead && allied(g, u.nat, far.ctrl));
  let p = guards.some((u) => u.kind === 'eng') ? B.engineers : guards.length ? B.chance : B.unguarded;
  // Fast troops arrive before the charges are fired.
  if (taken.units.some((u) => !u.dead && allied(g, u.nat, by) && KIND[u.kind].mobile)) p += B.fastCaptor;
  return Math.max(0.05, Math.min(0.98, p));
}

/**
 * When a region changes hands, the side that still holds the far bank of each of its rivers blows the bridges,
 * unless the captors are on them first. The dice are the bridge's own, so nothing else in the war shifts.
 */
export function bridgesOnCapture(g: Game, taken: Region, by: string): void {
  if (!groundRules(g)) return;
  for (const c of crossingsAt(taken.id)) {
    if (bridgeDown(g, c)) continue;
    const far = g.regions[farBank(c, taken.id)];
    if (!atWar(g, by, far.ctrl)) continue;
    const roll = new Rng(hashString(`bridge:${g.seed}:${g.turn}:${c.a}:${c.b}`)).next();
    const name = riverName(c), holder = g.nations[far.ctrl].adj, captor = g.nations[by].adj;
    if (roll < blowChance(g, c, taken, by)) {
      blow(g, c);
      if (concerns(g, by, far.ctrl)) headline(g, 'battle', `${holder} troops blow the bridges over the ${name} at ${taken.name} as ${captor} forces reach the river.`, far.ctrl, taken.id);
    } else if (concerns(g, by, far.ctrl)) {
      headline(g, 'battle', `${captor} troops seize the bridges over the ${name} at ${taken.name} before they can be blown.`, by, taken.id);
    }
  }
}

/** One river on a region's border, as the province panel shows it. */
export interface RiverNote { crossing: Crossing; river: string; far: number; down: boolean; since?: number }

/** The rivers on a region's borders and the state of their bridges. Empty outside the grand campaign. */
export function riverNotes(g: Game, region: number): RiverNote[] {
  if (!groundRules(g)) return [];
  return crossingsAt(region).map((c) => ({ crossing: c, river: riverName(c), far: farBank(c, region), down: bridgeDown(g, c), since: blownOn(g, c) }));
}
