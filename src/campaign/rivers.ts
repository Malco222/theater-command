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
import americasRivers from '../generated/world-americas-rivers.json';
import wholeRivers from '../generated/world-whole-rivers.json';
import { FAR_STAGE } from './stage';
import data from '../../data/campaign/engineering.json';
import { Rng, hashString } from '../core/rng';
import { KIND, type Corps, type Game, type Kind, type Region } from './types';
import { barred, groundRules, whyBarred } from './ground';
import { straitOpen } from './history';
import { allied, atWar, headline } from './game';
import { REARGUARD, chargesLaid } from './rearguard';
import { tieBars, whyTieBars } from './far';
import { halted } from './history';

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
  named: Record<'1' | '2' | '3' | 'mines', string>;
  jobs: Record<string, JobText>;
  start: { nations: string[]; corps: number };
  ai: { most: number; reserve: number };
};

/**
 * What a province's works are at a level, in words (ruling 145(b)): 1 "barbed wire, gun pits and an anti-tank ditch",
 * 2 "pillboxes and dragon's teeth", 3 "a line of concrete forts". Empty for none. Words only: no rule reads them.
 */
export const worksName = (lv: number): string => (lv >= 1 ? ENGINEERING.named[String(Math.min(3, Math.floor(lv))) as '1' | '2' | '3'] : '');

/** The rivers and crossings of the map of Europe. */
export const RIVERS = (riverData as unknown as { rivers: River[] }).rivers;
export const CROSSINGS: Crossing[] = (riverData as unknown as { crossings: number[][] }).crossings.map(([a, b, river, x, y]) => ({ a, b, river, x, y }));
/**
 * Those of the wider world (tools/build-rivers.mjs world). Its provinces are numbered on from Europe's and its
 * rivers follow Europe's in the list, so a crossing is found by its two provinces on either map.
 */
const WIDE = (FAR_STAGE ? wholeRivers : americasRivers) as unknown as { rivers: River[]; crossings: number[][] };
const ALL_RIVERS: River[] = [...RIVERS, ...WIDE.rivers];
export const WORLD_CROSSINGS: Crossing[] = WIDE.crossings.map(([a, b, river, x, y]) => ({ a, b, river: river + RIVERS.length, x, y }));
const ALL_CROSSINGS: Crossing[] = [...CROSSINGS, ...WORLD_CROSSINGS];
/** The rivers drawn on a game's map, and the crossings over them: Europe's, and the wider world's where the campaign is played on it. */
export const riversOf = (g: Game): River[] => (g.flags.world === 1 ? ALL_RIVERS : RIVERS);
export const crossingsOf = (g: Game): Crossing[] => (g.flags.world === 1 ? ALL_CROSSINGS : CROSSINGS);

const pair = (a: number, b: number): number => (a < b ? a * 4096 + b : b * 4096 + a);
const BY = new Map(ALL_CROSSINGS.map((c) => [pair(c.a, c.b), c]));
const AT = new Map<number, Crossing[]>();
for (const c of ALL_CROSSINGS) for (const r of [c.a, c.b]) (AT.get(r) ?? AT.set(r, []).get(r)!).push(c);
const WHEELS = new Set<Kind>(ENGINEERING.blown.barred);

/** The river crossing between two neighbouring regions, if there is one. Geography: the same in every game. */
export const crossingOf = (a: number, b: number): Crossing | null => BY.get(pair(a, b)) ?? null;
/** Every river crossing on a region's borders. */
export const crossingsAt = (r: number): Crossing[] => AT.get(r) ?? [];
export const riverName = (c: Crossing): string => ALL_RIVERS[c.river].name;
/** The region on the other bank. */
export const farBank = (c: Crossing, from: number): number => (c.a === from ? c.b : c.a);

const FLAGS = new Map(ALL_CROSSINGS.map((c) => [c, `br:${c.a}:${c.b}`]));
const flag = (c: Crossing): string => FLAGS.get(c) ?? `br:${c.a}:${c.b}`;
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

/**
 * Whether a corps cannot go from where it stands (or from another region) into a neighbour: ground too steep, or
 * bridges down. On the far side, also a strike from a client's ground into a war the client is not in (far.ts). And
 * ground a computer staff does not go into by the book (b512(h), history.json 'restraint'). And, for a country whose
 * war is a declaration and not a front, any ground but its own (diplomacy.json 'stay', Turkey in 1945). And a strait
 * that is shut to it (straitOpen).
 */
export const blocked = (g: Game, c: Corps, to: Region, from = c.reg): boolean => barred(g, c, to) || noCrossing(g, c.kind, from, to.id) || tieBars(g, c, to, from) || halted(g, c.nat, to.name) || keepsHome(g, c, to) || !straitOpen(g, c, g.regions[from], to);

/** True where a computer-run country whose war is only declared (diplomacy.json 'stay') would send a corps off its own ground. */
export const keepsHome = (g: Game, c: Corps, to: Region): boolean => !!g.flags[`stay:${c.nat}`] && c.nat !== g.human && to.owner !== c.nat;

/** The plain reason a corps cannot make that move, for either interface to show, or null if it can. */
export function whyBlocked(g: Game, c: Corps, to: Region, from = c.reg): string | null {
  const high = whyBarred(g, c, to);
  if (high) return high;
  if (!noCrossing(g, c.kind, from, to.id)) return whyTieBars(g, c, to, from);
  const x = crossingOf(from, to.id)!;
  return `The bridges over the ${riverName(x)} between ${g.regions[from].name} and ${to.name} are down. ${c.name} cannot cross until engineers rebuild them.`;
}

// The railways cross by the same bridges, so the supply picture is drawn afresh whenever one goes down or up.
function rebuild(g: Game, c: Crossing): void {
  delete g.flags[flag(c)];
  g.supplyDirty = true;
}
function blow(g: Game, c: Crossing): void {
  g.flags[flag(c)] = g.turn;
  g.supplyDirty = true;
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
  if (concerns(g, nat, far.ctrl)) headline(g, 'battle', `${g.nations[nat].adj} troops blow the bridges over the ${riverName(c)} between ${g.regions[from].name} and ${far.name}.`, nat, from, { arm: 'land' });
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
 * The lines of bridge news already told this month, so that a river crossed at one place by several bridges, or a
 * place taken twice, is told once (the third playtester, 84.3.1: "German troops seize the bridges over the Dnieper at
 * Kiev before they can be blown" three times in August 1942). News only: every bridge still rolls its own dice.
 */
const bridgeTold = new WeakMap<Game, { turn: number; said: Set<string> }>();
function firstTelling(g: Game, key: string): boolean {
  let t = bridgeTold.get(g);
  if (!t || t.turn !== g.turn) bridgeTold.set(g, (t = { turn: g.turn, said: new Set() }));
  if (t.said.has(key)) return false;
  t.said.add(key);
  return true;
}

/**
 * When a region changes hands, the side that still holds the far bank of each of its rivers blows the bridges,
 * unless the captors are on them first. The dice are the bridge's own, so nothing else in the war shifts.
 */
export function bridgesOnCapture(g: Game, taken: Region, by: string, again?: (far: Region) => boolean): void {
  if (!groundRules(g)) return;
  for (const c of crossingsAt(taken.id)) {
    if (bridgeDown(g, c)) continue;
    const far = g.regions[farBank(c, taken.id)];
    if (!atWar(g, by, far.ctrl)) continue;
    // A crossing's dice are thrown by a capture of either bank: once thrown for this month by an earlier taking of
    // either (`again`, from capture()), the captor's own (the design director's ruling, 25 November 04:55Z). The first
    // keeps its key.
    const key = `bridge:${g.seed}:${g.turn}:${c.a}:${c.b}`;
    const roll = new Rng(hashString(again?.(far) ? `${key}:${by}` : key)).next();
    const name = riverName(c), holder = g.nations[far.ctrl].adj, captor = g.nations[by].adj;
    // (Release 2, rearguard.ts) A rearguard of the far bank's side held this province: its charges were laid already.
    const laid = chargesLaid(g, taken.id, (nat) => nat === far.ctrl || allied(g, nat, far.ctrl));
    if (roll < (laid ? REARGUARD.bridges : blowChance(g, c, taken, by))) {
      blow(g, c);
      if (concerns(g, by, far.ctrl) && firstTelling(g, `blow:${name}:${taken.id}:${far.ctrl}`)) {
        headline(g, 'battle', laid
          ? `The ${holder} rearguard at ${taken.name} blows the bridges over the ${name} behind it as ${captor} forces break through.`
          : `${holder} troops blow the bridges over the ${name} at ${taken.name} as ${captor} forces reach the river.`, far.ctrl, taken.id, { arm: 'land' });
      }
    } else if (concerns(g, by, far.ctrl) && firstTelling(g, `seize:${name}:${taken.id}:${by}`)) {
      headline(g, 'battle', `${captor} troops seize the bridges over the ${name} at ${taken.name} before they can be blown.`, by, taken.id, { arm: 'land', won: by, lost: far.ctrl });
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
