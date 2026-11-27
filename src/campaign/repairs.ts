// Repairing damaged cities (Brian, 20 November 2026: "repairing is important but costs money to do so"). A town that
// bombs, shells or the fighting have damaged (citydamage.ts) pays that much less until its holder pays to repair it.
// The price is a few months of what the damage costs (damage.json 'repair'), paid at once and done at once; the
// player may take a repair back, with his money, in the month he ordered it. Computer nations repair out of a purse
// they fill from their income, the towns whose damage costs them most first, away from the front line.
//
// One price and one list for both interfaces: what can be repaired, what it costs, what it brings back each month.

import { DAMAGE, battlePoints, damageCity, damageKey, damageOf, damageRules, damageWords, lastHit, raidPoints, setDamage, shellCityPoints, shellPoints, siegeGunPoints, townRaidPoints, type FightKind } from './citydamage';
import { income, provinceYield } from './economy';
import { townMoney } from './buildings';
import { besieged } from './siege';
import { byHand } from './depth';
import { townName, townSize, type TownSize } from './towns';
import type { Game, Region } from './types';

const POOR = 'The treasury cannot cover it.';
const tenth = (x: number): number => Math.round(x * 10) / 10;

/**
 * What a town's damage costs its holder each month, in production points, at the holder's own rates and before the
 * nation-wide gains and losses: the share in ruins of what its factories, land, harbour, seat of government and town
 * buildings would pay standing whole (a siege left out of account: what it pays once the roads are open again).
 */
export function lostPay(g: Game, r: Region, nat: string = r.ctrl): number {
  const d = damageOf(g, r);
  if (d <= 0) return 0;
  const y = provinceYield(g, r, nat, true);
  // provinceYield has already taken the damage off; what stands whole is the rest over what is left.
  const parts = y.fac + y.land + y.port + y.capital;
  return (parts / (1 - d)) * d + townMoney(g, r, nat) * d;
}

/** What a town pays its holder each month standing whole, in production points, as lostPay counts it. */
export function wholePay(g: Game, r: Region, nat: string = r.ctrl): number {
  const d = damageOf(g, r);
  const y = provinceYield(g, r, nat, true);
  return (y.fac + y.land + y.port + y.capital) / (1 - d) + townMoney(g, r, nat);
}

// ---- Hits -----------------------------------------------------------------------------------------

/** The kinds of fire that damage a town: bombs on its industry, bombs on a besieged town, ships' guns on its defences or on its works, a besieger's artillery. */
export type HitKind = 'raid' | 'town' | 'shell' | 'shellCity' | 'guns';

/** The points of damage a hit of this kind and weight does (see citydamage.ts for the weights). */
export function hitPoints(kind: HitKind, weight: number, stopped = 0, night = false): number {
  return kind === 'raid' ? raidPoints(weight, stopped, night) : kind === 'town' ? townRaidPoints(weight)
    : kind === 'shell' ? shellPoints(weight) : kind === 'shellCity' ? shellCityPoints(weight) : siegeGunPoints(weight);
}

/**
 * What lasting damage to a town is worth to whoever did it, as a raid's achievement is counted (in aircraft).
 * `ruined` (fog on raids, airfog.ts): the share of the town in ruins as the side weighing the raid last saw it, where
 * it cannot see the town; left out, the town as it stands.
 */
export function cityWorthOf(g: Game, r: Region, pts: number, ruined?: number): number {
  if (!damageRules(g) || pts <= 0 || !DAMAGE.worth) return 0;
  const room = Math.max(0, Math.min(pts, DAMAGE.most - (ruined ?? damageOf(g, r)) * 100));
  return (room / 100) * wholePay(g, r) * DAMAGE.worth;
}

/** The points of damage a hit would do to this town, as a forecast shows them: no more than it has left to lose (`ruined` as for cityWorthOf). */
export function cityForecast(g: Game, r: Region, kind: HitKind, weight: number, stopped = 0, night = false, ruined?: number): number {
  if (!damageRules(g)) return 0;
  return Math.max(0, Math.min(hitPoints(kind, weight, stopped, night), DAMAGE.most - (ruined ?? damageOf(g, r)) * 100));
}

/** What one hit did to a town: the points added, where it now stands (percent), what it was worth to the attacker, and a sentence. */
export interface CityHit { region: number; added: number; now: number; worth: number; words: string }

/** Damages a town by a hit of this kind and weight. Null outside the rules or where it did nothing. */
export function cityHit(g: Game, r: Region, kind: HitKind, weight: number, stopped = 0, night = false): CityHit | null {
  if (!damageRules(g)) return null;
  return hurt(g, r, hitPoints(kind, weight, stopped, night));
}

/**
 * Damages the town of a province a battle was fought in: by the kind of fight, the divisions that fought on both
 * sides together, and the attackers' guns, in divisions. Null outside the rules or where it did nothing.
 */
export function battleHit(g: Game, r: Region, kind: FightKind, divs: number, guns: number): CityHit | null {
  if (!damageRules(g)) return null;
  return hurt(g, r, battlePoints(kind, divs, guns));
}

/** Damages a town by so many points of ruin outright (the atomic project's strike, atom.ts). Null outside the rules or where it did nothing. */
export function ruinHit(g: Game, r: Region, pts: number): CityHit | null {
  if (!damageRules(g)) return null;
  return hurt(g, r, pts);
}

function hurt(g: Game, r: Region, pts: number): CityHit | null {
  const pay = DAMAGE.worth ? wholePay(g, r) : 0;
  const added = damageCity(g, r, pts);
  if (added <= 0) return null;
  const now = g.flags[damageKey(r.id)] ?? 0;
  return { region: r.id, added, now, worth: (added / 100) * pay * DAMAGE.worth, words: damageWords(townName(r), added, now) };
}

/** The price of repairing a town outright, in production points: so many months of what its damage costs. */
export function repairCost(g: Game, r: Region, nat: string = r.ctrl): number {
  if (damageOf(g, r) <= 0) return 0;
  return Math.max(DAMAGE.repair.least, tenth(lostPay(g, r, nat) * DAMAGE.repair.months));
}

/**
 * What repairing these towns would add to the nation's income each month, after every nation-wide gain and loss
 * (footing, sinkings, bombing, methods, the level, its abilities, supplies, the cabinet): its income as it would be,
 * less its income now. (The towns' damage is set aside for the reckoning and put back, in place.)
 */
export function repairGain(g: Game, nat: string, regions: number[]): number {
  const ids = regions.filter((id) => g.flags[damageKey(id)] !== undefined);
  if (!ids.length) return 0;
  const now = income(g, nat);
  const kept = ids.map((id) => g.flags[damageKey(id)]);
  for (const id of ids) g.flags[damageKey(id)] = 0;
  try { return Math.max(0, income(g, nat) - now); } finally { ids.forEach((id, i) => { g.flags[damageKey(id)] = kept[i]; }); }
}

/** Why this nation cannot repair this town now, apart from the money; null when it can. */
function whyNotWork(g: Game, nat: string, r: Region | undefined): string | null {
  if (!damageRules(g)) return 'Towns are damaged and repaired only in the grand campaign, with damaged cities switched on.';
  if (!r) return 'There is no such place.';
  const name = townName(r);
  if (damageOf(g, r) <= 0) return `${name} stands undamaged.`;
  const n = g.nations[nat];
  if (!n?.alive) return 'A fallen country repairs nothing.';
  if (r.ctrl !== nat) return `${name} is not yours to repair.`;
  if (besieged(g, r)) return `${name} is under siege: no work can be done there until the ring is broken.`;
  return null;
}

/** Why this nation cannot repair this town now, the money included; null when it can. */
export function whyNotRepair(g: Game, nat: string, region: number): string | null {
  const r = g.regions[region];
  const why = whyNotWork(g, nat, r);
  if (why) return why;
  return g.nations[nat].pp < repairCost(g, r, nat) ? POOR : null;
}

/** A damaged town as an interface shows it: how badly, what it costs, what a repair costs and brings back. */
export interface RepairView {
  region: number;
  /** The town and the province it is the chief place of. */
  town: string;
  province: string;
  size: TownSize;
  /** The share in ruins, 0 to 0.8, and the same in whole percent. */
  damage: number;
  pct: number;
  /** The nation that holds it. */
  holder: string;
  /** For the holder: what the damage costs each month now, after every nation-wide gain and loss (a repair brings that back), in production points. */
  lost: number;
  /** The price of repairing it outright, in production points. */
  cost: number;
  /** Months for a repair to pay for itself, to a tenth. */
  months: number;
  /** Hit this month. */
  hit: boolean;
  /** Why the viewer cannot repair it now (null when he can); `poor` when money is all that stands in the way. */
  why: string | null;
  poor: boolean;
  /** The viewer repaired it this month and may still take the repair back. */
  undo: boolean;
}

/** One damaged town as the viewer sees it, or null where the town stands whole. */
export function repairView(g: Game, region: number, viewer: string | null = g.human): RepairView | null {
  const r = g.regions[region];
  if (!r) return null;
  const d = damageOf(g, r);
  const undo = viewer !== null && undoable(g, viewer, r);
  if (d <= 0 && !undo) return null;
  const nat = viewer ?? r.ctrl;
  const mine = r.ctrl === nat;
  const cost = d > 0 ? repairCost(g, r, r.ctrl) : 0;
  const lost = mine && d > 0 ? repairGain(g, nat, [r.id]) : 0;
  const why = viewer === null ? 'Only the country that holds a town repairs it.' : d > 0 ? whyNotRepair(g, viewer, region) : `${townName(r)} was repaired this month.`;
  return {
    region, town: townName(r), province: r.name, size: townSize(g, r), damage: d, pct: Math.round(d * 100), holder: r.ctrl,
    lost, cost, months: lost > 0 ? tenth(cost / lost) : 0, hit: lastHit(g, r) === g.turn,
    why, poor: why === POOR, undo,
  };
}

/** Every damaged town a nation holds, the ones whose damage costs it most first. */
export function damagedTowns(g: Game, nat: string): RepairView[] {
  if (!damageRules(g) || !g.flags['dmg:any']) return [];
  const out: RepairView[] = [];
  for (const r of g.regions) if (r.ctrl === nat && damageOf(g, r) > 0) out.push(repairView(g, r.id, nat)!);
  // (A repair is priced by what the damage costs, so every one pays back in about the same months: the dearest first.)
  return out.sort((a, b) => b.lost - a.lost || b.pct - a.pct || a.region - b.region);
}

/** Every damaged town on the map, for the map's own marks: where, and how badly. */
export function townsDamaged(g: Game): { region: number; town: string; damage: number; holder: string; hit: boolean }[] {
  if (!damageRules(g) || !g.flags['dmg:any']) return [];
  const out: { region: number; town: string; damage: number; holder: string; hit: boolean }[] = [];
  for (const r of g.regions) {
    const d = damageOf(g, r);
    if (d > 0) out.push({ region: r.id, town: townName(r), damage: d, holder: r.ctrl, hit: lastHit(g, r) === g.turn });
  }
  return out;
}

/** What 'Repair all' would do: the towns it can pay for now, the dearest first, and what is left over for want of money. */
export interface RepairAll {
  /** Every damaged town the nation holds. */
  towns: RepairView[];
  /** The ones the treasury can pay for now, in the order they would be repaired, and their price and monthly gain together. */
  pick: RepairView[];
  cost: number;
  gain: number;
  /** The price of repairing every one of them that can be worked on (not under siege). */
  all: number;
  /** The ones that can be worked on but must wait for money. */
  short: number;
}

export function repairAllPlan(g: Game, nat: string): RepairAll {
  const towns = damagedTowns(g, nat);
  const pick: RepairView[] = [];
  let left = g.nations[nat]?.pp ?? 0, cost = 0, all = 0, short = 0;
  for (const t of towns) {
    if (t.why && !t.poor) continue;
    all += t.cost;
    if (t.cost <= left) { pick.push(t); left -= t.cost; cost += t.cost; } else short++;
  }
  return { towns, pick, cost: tenth(cost), gain: repairGain(g, nat, pick.map((t) => t.region)), all: tenth(all), short };
}

// ---- Repairing -------------------------------------------------------------------------------------

// The player's own repairs can be taken back in the month they were ordered: 'rpt:<id>' the month, 'rpc:<id>' the
// price paid, 'rpo:<id>' the damage it cleared (percent) and 'rph:<id>' the month the town had last been hit.
const undoable = (g: Game, nat: string, r: Region): boolean => nat === g.human && g.flags[`rpt:${r.id}`] === g.turn && r.ctrl === nat;

function forget(g: Game, id: number): void {
  for (const k of ['rpt', 'rpc', 'rpo', 'rph']) delete g.flags[`${k}:${id}`];
}

/** Repairs a town outright and pays for it. True if it was done. */
export function repairCity(g: Game, nat: string, region: number): boolean {
  if (whyNotRepair(g, nat, region)) return false;
  const r = g.regions[region];
  const cost = repairCost(g, r, nat);
  const was = g.flags[damageKey(r.id)] ?? 0, hit = lastHit(g, r);
  g.nations[nat].pp -= cost;
  setDamage(g, r, 0);
  if (nat === g.human) {
    // Only this month's repairs can be taken back: last month's notes go.
    for (const k of Object.keys(g.flags)) if (k.startsWith('rpt:') && g.flags[k] !== g.turn) forget(g, Number(k.slice(4)));
    forget(g, r.id);
    g.flags[`rpt:${r.id}`] = g.turn;
    g.flags[`rpc:${r.id}`] = cost;
    g.flags[`rpo:${r.id}`] = was;
    if (hit !== undefined) g.flags[`rph:${r.id}`] = hit;
  }
  return true;
}

/** Takes back a repair the player ordered this month: the town as it was, and his money back. */
export function undoRepair(g: Game, nat: string, region: number): boolean {
  const r = g.regions[region];
  if (!r || !undoable(g, nat, r)) return false;
  g.nations[nat].pp += g.flags[`rpc:${r.id}`] ?? 0;
  setDamage(g, r, g.flags[`rpo:${r.id}`] ?? 0);
  const hit = g.flags[`rph:${r.id}`];
  if (hit !== undefined) g.flags[`dmgt:${r.id}`] = hit;
  forget(g, r.id);
  return true;
}

/** Repairs every town the treasury can pay for, the ones whose damage costs most first. Returns the towns repaired. */
export function repairAll(g: Game, nat: string): string[] {
  const done: string[] = [];
  for (const t of repairAllPlan(g, nat).pick) if (repairCity(g, nat, t.region)) done.push(t.town);
  return done;
}

// ---- The computer's repairs ------------------------------------------------------------------------

/**
 * Whether a staff's repair pays for itself within the price's own horizon. A repair is priced at 'repair.months' of what
 * the damage costs the one who pays (repairCost), so a repair priced so is repaid in exactly those months; one priced at
 * the floor ('repair.least'), because the damage costs next to nothing, would take longer, and a staff leaves it be (the
 * design director, 22 November 2026: Germany's staff was buying 0.5 repairs of towns losing 0.002 a month). What the
 * damage costs is read at the payer's own rates (lostPay): an occupier counts its occupied share of the town's pay, not
 * the town's whole worth.
 */
export const repairPays = (cost: number, lost: number): boolean => cost <= tenth(lost * DAMAGE.repair.months);

/**
 * A computer nation's repairs for the month (and the player's staff, when he has handed it towns and trade). It
 * puts a share of its income by in a purse while it has damaged towns, and pays out of it for the towns whose damage
 * costs it most, keeping a reserve in hand. It buys only a repair that pays for itself within the price's own horizon
 * (repairPays). It does not rebuild on the front line: it leaves alone a town under siege, one beside ground its enemies
 * hold, and one hit in the last months ('quiet'). No dice: the same war repairs the same towns. Returns the towns
 * repaired. (The player's own Repair button asks none of this: he chooses his repairs.)
 */
export function aiRepair(g: Game, nat: string): string[] {
  if (!damageRules(g) || !g.flags['dmg:any']) return [];
  const n = g.nations[nat];
  if (!n?.alive) return [];
  if (nat === g.human && byHand(g, nat, 'towns')) return [];
  const P = `rpp:${nat}`;
  const towns: { r: Region; cost: number; lost: number }[] = [];
  for (const r of g.regions) {
    if (r.ctrl !== nat || damageOf(g, r) <= 0) continue;
    const cost = repairCost(g, r, nat), lost = lostPay(g, r, nat);
    if (repairPays(cost, lost)) towns.push({ r, cost, lost });
  }
  if (!towns.length) { delete g.flags[P]; return []; }
  const all = towns.reduce((s, t) => s + t.cost, 0);
  let purse = Math.min(all, (g.flags[P] ?? 0) + Math.max(0, income(g, nat)) * DAMAGE.ai.share);
  towns.sort((a, b) => b.lost - a.lost || a.r.id - b.r.id);
  const foes = new Set(n.atWar);
  const done: string[] = [];
  for (const t of towns) {
    const hit = lastHit(g, t.r);
    if ((hit !== undefined && g.turn - hit < DAMAGE.ai.quiet) || besieged(g, t.r) || t.r.nb.some((j) => foes.has(g.regions[j].ctrl))) continue;
    if (t.cost > purse || n.pp < t.cost + DAMAGE.ai.reserve) continue;
    n.pp -= t.cost;
    purse -= t.cost;
    setDamage(g, t.r, 0);
    done.push(townName(t.r));
  }
  const kept = Math.round(purse * 100) / 100;
  if (kept >= 0.01) g.flags[P] = kept; else delete g.flags[P];
  return done;
}
