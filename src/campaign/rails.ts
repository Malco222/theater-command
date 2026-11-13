// Railways, and the damage bombing leaves behind.
//
// The net is laid by tools/build-rails.mjs from data/campaign/rails.mjs: a line to every province outside the
// desert and main lines between the cities. In the grand campaign troops go by rail only along it, and each
// province's line is either working or cut. No train runs into, out of or through a province whose line is cut.
// Lines are cut by troops driven out of a province, by order, and by bombing; only engineers relay them.
//
// An army lives off its railhead: the nearest province joined by working track to one of its depots or supplied
// ports. A couple of provinces of road beyond it cost nothing. Further than that the army fights weaker, until
// the line is brought forward.
//
// Heavy bombing also wrecks factories outright, and a wrecked factory earns nothing until engineers rebuild it.
// The historical scenarios have none of this: there the trains run wherever friendly ground is joined.
//
// Both interfaces read the same functions: where the lines run, which are cut, and how far each province is
// from the player's railhead.

import railData from '../generated/campaign-rails.json';
import data from '../../data/campaign/railways.json';
import { Rng, hashString } from '../core/rng';
import { KIND, type Corps, type Game, type Region } from './types';
import { groundRules } from './ground';
import { downBetween } from './rivers';
import { EASTERN, allied, atWar, headline, inDesert } from './game';
import { besieged } from './siege';

/** A line between two neighbouring provinces (a is the lower id), and the point where it crosses from one into the other. */
export interface RailLink { a: number; b: number; x: number; y: number }

export const RAILWAYS = data as unknown as {
  wreck: { chance: number; unguarded: number; fastCaptor: number };
  /** [furthest by road from the railhead, attack, defence] */
  railhead: [number, number, number][];
  bomb: { scale: number; most: number };
  ruin: { over: number; chance: number; most: number };
};

export const RAIL_LINKS: RailLink[] = (railData as unknown as { links: number[][] }).links.map(([a, b, x, y]) => ({ a, b, x, y }));
const NET = new Map<number, number[]>();
for (const l of RAIL_LINKS) for (const [p, q] of [[l.a, l.b], [l.b, l.a]]) (NET.get(p) ?? NET.set(p, []).get(p)!).push(q);
const NONE: number[] = [];

/** The provinces a province's railway runs to. Geography: the same in every game. */
export const railsAt = (r: number): number[] => NET.get(r) ?? NONE;
/** Whether any railway runs through a province. None crosses the desert, and a few islands have none. */
export const onNet = (r: number): boolean => NET.has(r);

const KEYS: string[] = [];
const key = (r: number): string => (KEYS[r] ??= `rail:${r}`);
/** Whether a province's railway is cut. Never outside the grand campaign. */
export const railCut = (g: Game, r: number): boolean => groundRules(g) && g.flags[key(r)] !== undefined;
/** The month a province's railway was cut, or undefined while it works. */
export const cutOn = (g: Game, r: number): number | undefined => (groundRules(g) ? g.flags[key(r)] : undefined);

/**
 * Cuts the railway of a province, whoever does it and however: retreating troops, an order, bombers, and later
 * saboteurs and partisans. Returns false where there is no line or it is already cut.
 */
export function cutRail(g: Game, r: number): boolean {
  if (!groundRules(g) || !onNet(r) || railCut(g, r)) return false;
  g.flags[key(r)] = g.turn;
  g.supplyDirty = true;
  return true;
}

/** Relays the railway of a province. */
export function mendRail(g: Game, r: number): boolean {
  if (!railCut(g, r)) return false;
  delete g.flags[key(r)];
  g.supplyDirty = true;
  return true;
}

/** Whether a train of this nation can run from one province to the next: a line between them, both ends friendly and whole, the bridges standing. */
export function railOpen(g: Game, nat: string, a: number, b: number): boolean {
  if (!railsAt(a).includes(b) || railCut(g, a) || railCut(g, b) || downBetween(g, a, b)) return false;
  // The station is in the town: no train runs through a province whose town is under siege, whoever is winning there.
  return allied(g, nat, g.regions[a].ctrl) && allied(g, nat, g.regions[b].ctrl) && !besieged(g, g.regions[a]) && !besieged(g, g.regions[b]);
}

/** Whether the news of a railway matters to the player: it was theirs, or they took it. */
const concerns = (g: Game, ...nats: string[]): boolean => g.human !== null && g.nations[g.human].alive && nats.includes(g.human);

// ---- Cutting the line -----------------------------------------------------------------------

/** Why this nation cannot tear up the railway of a province, or null if it can. */
export function whyNotCutRail(g: Game, nat: string, region: number): string | null {
  if (!groundRules(g)) return 'There are no railways to cut in this campaign.';
  const r = g.regions[region];
  if (!r) return 'Pick a province.';
  if (!onNet(region)) return `No railway runs through ${r.name}.`;
  if (railCut(g, region)) return `The railway at ${r.name} is already cut.`;
  if (!allied(g, nat, r.ctrl)) return `${r.name} is not in friendly hands.`;
  if (!r.units.some((u) => u.nat === nat && !u.dead)) return `There are no troops of yours at ${r.name} to tear up the track.`;
  if (!g.nations[nat].atWar.length) return 'Nobody tears up their own railways in peacetime.';
  return null;
}

/** Tears up the railway of a province by order of the nation whose troops stand there. */
export function tearUp(g: Game, nat: string, region: number): boolean {
  if (whyNotCutRail(g, nat, region) || !cutRail(g, region)) return false;
  const r = g.regions[region];
  if (concerns(g, nat)) headline(g, 'info', `${g.nations[nat].adj} troops tear up the railway at ${r.name}. No train runs through until engineers relay it.`, nat, region);
  return true;
}

/** The chance that the track is torn up when a province is lost: whether the loser still has troops next door, and who took it. */
export function wreckChance(g: Game, taken: Region, by: string): number {
  const W = RAILWAYS.wreck;
  const near = taken.nb.some((j) => g.regions[j].units.some((u) => !u.dead && atWar(g, u.nat, by)));
  let p = near ? W.chance : W.unguarded;
  // Fast troops are on the junctions before the demolition parties have finished.
  if (taken.units.some((u) => !u.dead && allied(g, u.nat, by) && KIND[u.kind].mobile)) p += W.fastCaptor;
  return Math.max(0.05, Math.min(0.95, p));
}

/**
 * When a province changes hands in war, the side driven out of it may tear up the track as it goes. The dice are
 * the province's own for the month, so nothing else in the war shifts.
 */
export function railOnCapture(g: Game, taken: Region, by: string, from: string): void {
  if (!groundRules(g) || !onNet(taken.id) || railCut(g, taken.id)) return;
  const roll = new Rng(hashString(`rail:${g.seed}:${g.turn}:${taken.id}`)).next();
  if (roll >= wreckChance(g, taken, by) || !cutRail(g, taken.id)) return;
  if (concerns(g, by, from)) headline(g, 'battle', `${g.nations[from].adj} troops tear up the railway at ${taken.name} as they fall back.`, from, taken.id);
}

/** The chance that a raid cuts the line of the province it bombs: the bombers that got through, less what the guns stop. */
export function railBombChance(g: Game, target: number, through: number, gunsStop = 0): number {
  if (!groundRules(g) || !onNet(target) || railCut(g, target) || through <= 0) return 0;
  return Math.min(RAILWAYS.bomb.most, through / RAILWAYS.bomb.scale) * (1 - gunsStop);
}

/** A raid on a province's rail yards and junctions. Returns true if the line is cut. */
export function bombRail(g: Game, by: string, target: number, through: number, gunsStop = 0): boolean {
  const p = railBombChance(g, target, through, gunsStop);
  if (p <= 0) return false;
  return new Rng(hashString(`railbomb:${g.seed}:${g.turn}:${target}:${by}`)).next() < p && cutRail(g, target);
}

// ---- The railhead ---------------------------------------------------------------------------

/**
 * How many provinces of road lie between each province a nation can draw supply in and its nearest working
 * railway: 0 on the working net, -1 where it draws no supply at all. The working net is everything joined by
 * whole, friendly track to a depot or a supplied port. Worked out the first time it is asked for after the
 * supply picture is drawn, and kept until the picture is drawn again. Null outside the grand campaign and for
 * a nation with no army.
 */
export function railheads(g: Game, nat: string): Int16Array | null {
  if (!groundRules(g)) return null;
  const have = g.railhead[nat];
  if (have) return have;
  const ok = g.supply[nat];
  if (!ok) return null;
  const d = new Int16Array(g.regions.length).fill(-1);
  const net: number[] = [];
  for (let i = 0; i < ok.length; i++) if (ok[i] === 2) { d[i] = 0; net.push(i); }
  for (let i = 0; i < net.length; i++) {
    const a = net[i];
    if (g.flags[key(a)] !== undefined) continue; // the stores are there, but no train leaves
    for (const j of railsAt(a)) {
      if (d[j] === 0 || !ok[j] || g.flags[key(j)] !== undefined || downBetween(g, a, j)) continue;
      d[j] = 0; net.push(j);
    }
  }
  // Beyond the railhead everything comes by road: a step for every province.
  const q = net.slice();
  for (let i = 0; i < q.length; i++) {
    for (const j of g.regions[q[i]].nb) {
      if (d[j] >= 0 || !ok[j]) continue;
      d[j] = d[q[i]] + 1; q.push(j);
    }
  }
  return (g.railhead[nat] = d);
}

/** Forgets every nation's railheads: called whenever the supply picture is drawn, so that they are counted from it afresh. */
export function resetRailheads(g: Game): void {
  g.railhead = {};
}

/** Counts every nation's railheads now, from the supply picture as it stands. */
export function computeRailheads(g: Game): void {
  resetRailheads(g);
  for (const id of Object.keys(g.supply)) railheads(g, id);
}

/** How far a nation's troops in a province are from their railhead, and what that makes them worth. */
export interface Railhead { /** Provinces of road to the nearest working railway: 0 on it. */ road: number; attack: number; defence: number }
const AT_HOME: Railhead = { road: 0, attack: 1, defence: 1 };

/**
 * What a nation's troops in a province are worth for their distance from the railhead. Call ensureSupply first.
 * The desert has its own supply line, counted from the ports, and so has an invader deep inside the Soviet Union,
 * counted from the frontier: neither is counted twice.
 */
export function railheadAt(g: Game, nat: string, region: number): Railhead {
  if (!groundRules(g)) return AT_HOME;
  const r = g.regions[region];
  if (inDesert(r)) return AT_HOME;
  if (EASTERN.deepOwners.includes(r.owner) && !EASTERN.deepOwners.includes(nat)) return AT_HOME;
  const road = railheads(g, nat)?.[region] ?? 0;
  if (road <= 0) return AT_HOME;
  for (const [far, attack, defence] of RAILWAYS.railhead) if (road <= far) return { road, attack, defence };
  return { road, attack: 1, defence: 1 };
}

/** The same for one corps where it stands. A corps that is cut off altogether has worse troubles, counted elsewhere. */
export const railheadOf = (g: Game, c: Corps): Railhead => (c.supplied ? railheadAt(g, c.nat, c.reg) : AT_HOME);

// ---- Factories in ruins ---------------------------------------------------------------------

const ruinKey = (r: number): string => `ruin:${r}`;
/** How many of a province's factories lie wrecked by bombing. Always 0 outside the grand campaign. */
export const ruinedAt = (g: Game, r: Region): number => (groundRules(g) ? Math.min(r.fac, g.flags[ruinKey(r.id)] ?? 0) : 0);
/** The factories of a province that are at work. */
export const workingFactories = (g: Game, r: Region): number => r.fac - ruinedAt(g, r);
/** The most of a province's factories that bombing can wreck outright. */
export const ruinMost = (r: Region): number => Math.ceil(r.fac * RAILWAYS.ruin.most);

/** The chance that a raid which stopped this share of a province's output also wrecks one of its factories outright. */
export function ruinChance(g: Game, r: Region, stopped: number): number {
  const U = RAILWAYS.ruin;
  if (!groundRules(g) || stopped <= U.over || ruinedAt(g, r) >= ruinMost(r)) return 0;
  return Math.min(0.95, stopped * U.chance);
}

/** A heavy raid on a province's industry. Returns true if a factory is wrecked. */
export function bombFactories(g: Game, by: string, r: Region, stopped: number): boolean {
  const p = ruinChance(g, r, stopped);
  if (p <= 0 || new Rng(hashString(`ruin:${g.seed}:${g.turn}:${r.id}:${by}`)).next() >= p) return false;
  g.flags[ruinKey(r.id)] = ruinedAt(g, r) + 1;
  return true;
}

/** Wrecks one working factory of a province outright, by a saboteur's hand. False where no more can be wrecked. */
export function wreckFactory(g: Game, r: Region): boolean {
  if (!groundRules(g) || r.fac <= 0 || ruinedAt(g, r) >= ruinMost(r)) return false;
  g.flags[ruinKey(r.id)] = ruinedAt(g, r) + 1;
  return true;
}

/** Engineers put one wrecked factory back to work. */
export function rebuildFactory(g: Game, r: Region): boolean {
  const n = ruinedAt(g, r);
  if (n <= 0) return false;
  if (n <= 1) delete g.flags[ruinKey(r.id)]; else g.flags[ruinKey(r.id)] = n - 1;
  return true;
}

// ---- For the province panel -----------------------------------------------------------------

/** The railway of one province, as the province panel shows it. */
export interface RailNote {
  /** Whether a railway runs through the province at all. */
  line: boolean;
  /** The provinces its lines run to. */
  to: number[];
  cut: boolean;
  /** The month it was cut. */
  since?: number;
  /** For the nation asked about: provinces of road from here to its nearest working railway, 0 on it, -1 where it draws no supply. */
  road: number;
  /** What that nation's troops here are worth for it. */
  attack: number;
  defence: number;
  /** Factories wrecked by bombing, and the factories the province has. */
  ruined: number;
  factories: number;
}

/** The state of a province's railway and what it means to one nation's troops there. Null outside the grand campaign. */
export function railNote(g: Game, region: number, nat: string | null = g.human): RailNote | null {
  if (!groundRules(g)) return null;
  const r = g.regions[region];
  const head = nat ? railheadAt(g, nat, region) : AT_HOME;
  const raw = nat ? railheads(g, nat)?.[region] ?? -1 : -1;
  return {
    line: onNet(region), to: railsAt(region), cut: railCut(g, region), since: cutOn(g, region),
    road: raw < 0 ? -1 : head.road, attack: head.attack, defence: head.defence,
    ruined: ruinedAt(g, r), factories: r.fac,
  };
}
