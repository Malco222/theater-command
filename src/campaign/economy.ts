// The war economy: factories earn production points, losses are made good first, and
// what is left buys new formations and fortifications that take months to arrive.
// The numbers live in data/campaign/production.json so they can be changed without code.

import data from '../../data/campaign/production.json';
import setup from '../../data/campaign/setup-1939.json';
import { STACK, type Build, type Game, type Kind, type Region } from './types';
import { addCorps, allied, headline } from './game';
import { DIPLOMACY, tradePartners } from './diplomacy';
import { airIncomeLoss } from './air';
import { seaIncomeLoss } from './navy';
import { techMul } from './research';

export interface BuildSpec { label: string; divs: number; cost: number; months: number; blurb: string }

export const PRODUCTION = data as unknown as {
  baseIncome: number; occupiedIncome: number; peaceIncome: number; replacementCost: number; manpower: number;
  aiBank: [number, number]; aiReserve: number;
  builds: Record<string, BuildSpec>;
  fort: { cost: number[]; months: number };
  firstCorpsNumber: Record<string, number>;
  armourName: Record<string, string>;
};

export const firstCorpsNumber = (nat: string): number => PRODUCTION.firstCorpsNumber[nat] ?? PRODUCTION.firstCorpsNumber._default;

/** Production points a nation earns this month. */
export function income(g: Game, nat: string): number {
  const n = g.nations[nat];
  let fac = 0;
  for (const r of g.regions) if (r.ctrl === nat) fac += r.owner === nat ? r.fac : r.fac * PRODUCTION.occupiedIncome;
  // A country whose homeland is off the map has its factories there, out of any enemy's reach.
  fac += n.offmap?.industry ?? 0;
  // Neutrals that lean toward a bloc trade with its major powers.
  const trade = tradePartners(g, nat).length * DIPLOMACY.tradeIncome;
  // What comes by sea can be sunk on the way: cargoes on the convoy routes, ore from the north.
  // And the factories turn out more, or less, as the nation's production methods run ahead of or behind their time.
  return ((PRODUCTION.baseIncome + fac) * (n.atWar.length ? 1 : PRODUCTION.peaceIncome) + trade) * (1 - seaIncomeLoss(g, nat)) * (1 - airIncomeLoss(g, nat)) * techMul(g, nat, 'industry');
}

/** Last month in which this nation still has pre-war mobilisation arriving. */
function lastScheduled(nat: string): number {
  let t = 0;
  for (const row of (setup as any).reinforcements) if (row[1] === nat) t = Math.max(t, row[0]);
  return t;
}

/** A computer-run nation starts ordering new formations once it is at war and its mobilisation has run its course. */
export function canBuildYet(g: Game, nat: string): boolean {
  return g.nations[nat].atWar.length > 0 && g.turn > lastScheduled(nat);
}

const ROMAN: [number, string][] = [[100, 'C'], [90, 'XC'], [50, 'L'], [40, 'XL'], [10, 'X'], [9, 'IX'], [5, 'V'], [4, 'IV'], [1, 'I']];
function roman(n: number): string {
  let out = '';
  for (const [v, s] of ROMAN) while (n >= v) { out += s; n -= v; }
  return out;
}

function corpsName(nat: string, kind: Kind, no: number): string {
  const arm = PRODUCTION.armourName[nat] ?? PRODUCTION.armourName._default;
  const word = kind === 'arm' ? `${arm} ` : kind === 'mot' ? 'Motorized ' : kind === 'mtn' ? 'Mountain ' : '';
  return `${roman(no)} ${word}Corps`;
}

export const isHome = (g: Game, nat: string, r: Region): boolean => r.owner === nat && r.ctrl === nat;

/** Where new formations can be raised: at home, not in the colonies. Troops for overseas are raised at home and shipped. */
export const canRaise = (g: Game, nat: string, r: Region): boolean => isHome(g, nat, r) && !r.overseas;

/** Orders a new corps. The points are paid now; the corps arrives in a few months. */
export function orderCorps(g: Game, nat: string, kind: Kind, region: number): Build | null {
  const n = g.nations[nat];
  const spec = PRODUCTION.builds[kind];
  const r = g.regions[region];
  if (!n.alive || !spec || !r || !canRaise(g, nat, r) || n.pp < spec.cost) return null;
  n.pp -= spec.cost;
  const b: Build = { id: g.nextBuild++, what: 'corps', kind, divs: spec.divs, name: corpsName(nat, kind, n.corpsNo++), region, ordered: g.turn, eta: g.turn + spec.months, cost: spec.cost };
  n.queue.push(b);
  return b;
}

/** What the next level of fortification would cost in a region, or null if it cannot be fortified further. */
export function fortCost(g: Game, nat: string, r: Region): number | null {
  if (!isHome(g, nat, r)) return null;
  const queued = g.nations[nat].queue.filter((b) => b.what === 'fort' && b.region === r.id).length;
  const next = (r.fort?.lv ?? 0) + queued + 1;
  return next > 3 ? null : PRODUCTION.fort.cost[next];
}

export function orderFort(g: Game, nat: string, region: number): Build | null {
  const n = g.nations[nat];
  const r = g.regions[region];
  const cost = r ? fortCost(g, nat, r) : null;
  if (!n.alive || cost === null || n.pp < cost) return null;
  n.pp -= cost;
  const b: Build = { id: g.nextBuild++, what: 'fort', name: `Fortifications at ${r.name}`, region, ordered: g.turn, eta: g.turn + PRODUCTION.fort.months, cost };
  n.queue.push(b);
  return b;
}

/** An order can be taken back for a full refund in the month it was placed. */
export function cancelBuild(g: Game, nat: string, id: number): boolean {
  const n = g.nations[nat];
  const i = n.queue.findIndex((b) => b.id === id);
  if (i < 0 || n.queue[i].ordered !== g.turn) return false;
  n.pp += n.queue[i].cost;
  if (n.queue[i].what === 'corps') n.corpsNo--;
  n.queue.splice(i, 1);
  return true;
}

/** Hands over everything on order that is due this month. */
export function deliver(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.queue.length) return;
  const due = n.queue.filter((b) => b.eta <= g.turn);
  if (!due.length) return;
  n.queue = n.queue.filter((b) => b.eta > g.turn);
  for (const b of due) {
    const r = g.regions[b.region];
    if (b.what === 'fort') {
      if (!isHome(g, nat, r)) continue; // the ground was lost before the works were finished
      const vs = new Set(r.fort?.vs ?? []);
      vs.add(r.owner);
      for (const j of r.nb) vs.add(g.regions[j].owner);
      r.fort = { lv: Math.min(3, (r.fort?.lv ?? 0) + 1), vs: [...vs] };
      if (nat === g.human) headline(g, 'info', `The new fortifications at ${r.name} are finished.`, nat, r.id);
      continue;
    }
    const room = (x: Region): boolean => canRaise(g, nat, x) && x.units.filter((u) => allied(g, u.nat, nat)).length < STACK;
    let at: Region | null = room(r) ? r : null;
    if (!at && n.capital >= 0 && room(g.regions[n.capital])) at = g.regions[n.capital];
    if (!at) at = g.regions.find((x) => room(x) && x.fac > 0) ?? g.regions.find(room) ?? null;
    if (!at) continue; // nowhere left to form up
    const c = addCorps(g, nat, b.name, b.kind!, b.divs!, at.id, { xp: 0.05 });
    c.org = 0.7;
    if (nat === g.human) headline(g, 'info', `${b.name} has finished forming at ${at.name} and is ready for orders.`, nat, at.id);
  }
}

/** The computer's factory policy: keep a reserve for replacements, spend the rest on new corps. */
export function aiProduction(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (nat === g.human || !n.alive || n.capital < 0 || !canBuildYet(g, nat)) return;
  const cap = g.regions[n.capital];
  if (!isHome(g, nat, cap)) return;
  let home = 0;
  for (const r of g.regions) if (isHome(g, nat, r)) home += r.fac;
  while (n.queue.length < 2) {
    const kind: Kind = home >= 20 && n.corpsNo % 3 === 2 ? 'arm' : 'inf';
    if (n.pp < PRODUCTION.builds[kind].cost + PRODUCTION.aiReserve) break;
    if (!orderCorps(g, nat, kind, cap.id)) break;
  }
}
