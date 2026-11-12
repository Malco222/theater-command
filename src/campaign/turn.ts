// The monthly turn: each nation in order gets its month, the computer plays the nations
// the player does not, and the player's orders go through the same rules.

import { KIND, STACK, type BattleReport, type Build, type Corps, type Fleet, type Game, type General, type Kind, type Region, type Skill, type Stance } from './types';
import { NAVY, canPass, landCorps, landingTargets, planLanding, rebase, sail, seaBattles, whyNotShip, type Landing, type LandingPlan } from './navy';
import { assignGeneral, promote, relieve, sack, spendPoint } from './commanders';
import {
  allied, assess, atWar, declareWar, ensureSupply, evacuate, friendlyUnits, hostileUnits, limitAttackers,
  moveCorps, moveCost, railMove, resolveBattle, shipMove, sovietEvent, startPhase, type Assessment,
} from './game';
import { cancelBuild, orderCorps, orderFort } from './economy';
import { runAI } from './ai';
import { court, monthlyDiplomacy } from './diplomacy';

export const current = (g: Game): string => g.order[g.phase];

function closePhase(g: Game, nat: string): void {
  // The month's fleet actions are fought once the nation's orders are all given.
  seaBattles(g, nat);
  if (nat === 'GER') sovietEvent(g);
  g.phase++;
  if (g.phase >= g.order.length) {
    g.phase = 0; g.turn++; g.week = 1;
    monthlyDiplomacy(g);
  }
}

/** Plays one computer-run nation's month. */
export function stepAI(g: Game): string {
  const nat = current(g);
  g.week = 1;
  startPhase(g, nat);
  if (g.nations[nat].alive) runAI(g, nat);
  closePhase(g, nat);
  return nat;
}

/** Opens the player's month: reinforcements, replacements, fresh action points. */
export function beginHuman(g: Game): void {
  g.week = 1;
  startPhase(g, current(g));
}

export function endHuman(g: Game): void {
  closePhase(g, current(g));
}

/** True when the nation up next is the player's and still in the war. */
export function humanUp(g: Game): boolean {
  return g.human !== null && current(g) === g.human && g.nations[g.human].alive;
}

/** Runs a whole month with every nation on the computer. */
export function runTurnAI(g: Game): void {
  const t = g.turn;
  while (g.turn === t) stepAI(g);
}

// ---- The player's orders -----------------------------------------------------------------

export function canOrder(g: Game, c: Corps): boolean {
  return !c.dead && c.nat === g.human && current(g) === c.nat && c.kind !== 'fort';
}

export interface Reach { cost: number; prev: number; attack: boolean }

/** Every region this corps could get to this month, and the enemy-held ones it could attack. */
export function reach(g: Game, c: Corps): Map<number, Reach> {
  const out = new Map<number, Reach>();
  if (c.kind === 'fort' || c.ap <= 0) return out;
  out.set(c.reg, { cost: 0, prev: -1, attack: false });
  const open = [c.reg];
  while (open.length) {
    open.sort((a, b) => out.get(a)!.cost - out.get(b)!.cost);
    const a = open.shift()!;
    const ca = out.get(a)!.cost;
    for (const j of g.regions[a].nb) {
      const r = g.regions[j];
      const friendly = allied(g, c.nat, r.ctrl);
      if (!friendly && !atWar(g, c.nat, r.ctrl)) continue;
      const step = moveCost(g, c, r);
      const left = c.ap - ca;
      if (left <= 0 || left < Math.min(step, 4)) continue;
      const cost = ca + step;
      if (hostileUnits(g, r, c.nat).length) {
        if (a === c.reg && !out.has(j)) out.set(j, { cost: 1, prev: a, attack: true });
        continue;
      }
      if (friendlyUnits(g, r, c.nat).length >= STACK) continue;
      const old = out.get(j);
      if (old && old.cost <= cost) continue;
      out.set(j, { cost, prev: a, attack: false });
      open.push(j);
    }
  }
  return out;
}

/** Marches a corps to a region it can reach, one region at a time. Returns how far it got. */
export function orderMove(g: Game, c: Corps, to: number): number {
  if (!canOrder(g, c)) return 0;
  const m = reach(g, c);
  const end = m.get(to);
  if (!end || end.attack) return 0;
  const path: number[] = [];
  for (let at = to; at !== c.reg && at >= 0; at = m.get(at)!.prev) path.unshift(at);
  let n = 0;
  for (const j of path) { if (!moveCorps(g, c, g.regions[j])) break; n++; }
  return n;
}

/** The player's corps that could join an attack on a region. */
export function candidates(g: Game, target: Region): Corps[] {
  if (g.human === null) return [];
  return g.corps.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id));
}

export function previewAttack(g: Game, corps: Corps[], target: Region): Assessment | null {
  if (!corps.length || !hostileUnits(g, target, corps[0].nat).length) return null;
  return assess(g, corps, target);
}

export function orderAttack(g: Game, corps: Corps[], target: Region): BattleReport | null {
  const ok = limitAttackers(g, corps.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id)), target);
  if (!ok.length || !hostileUnits(g, target, ok[0].nat).length) return null;
  return resolveBattle(g, ok, target);
}

export function orderRail(g: Game, c: Corps, to: number): boolean {
  return canOrder(g, c) && railMove(g, c, g.regions[to]);
}

/** Friendly ports this corps could be shipped to this month. */
export function shipTargets(g: Game, c: Corps): Region[] {
  const from = g.regions[c.reg];
  if (g.nations[c.nat].sealift <= 0 || !from.port || c.kind === 'fort' || c.ap < Math.min(4, c.apMax)) return [];
  return g.regions.filter((r) => r.port && r.id !== from.id && whyNotShip(g, c, r) === null);
}

/** Enemy coasts the player's chosen corps could land on from the port they stand in. */
export function landTargets(g: Game, cs: Corps[]): Region[] {
  return cs.length && cs.every((c) => canOrder(g, c)) ? landingTargets(g, cs) : [];
}

/** The odds and the dangers of a landing, before the order is given. */
export function previewLanding(g: Game, cs: Corps[], to: number): LandingPlan {
  return planLanding(g, cs, g.regions[to]);
}

/** Sends the chosen corps across the sea to land on an enemy coast. */
export function orderLand(g: Game, cs: Corps[], to: number): Landing {
  if (!cs.length || !cs.every((c) => canOrder(g, c))) return { ok: false, why: 'Those corps cannot be given orders now.' };
  return landCorps(g, cs, g.regions[to]);
}

const myFleet = (g: Game, id: number): Fleet | null => {
  const f = g.fleets.find((x) => x.id === id);
  return f && f.nat === g.human && current(g) === f.nat ? f : null;
};

/** Sends one of the player's fleets to a sea to patrol or raid, or back to port. */
export function orderFleet(g: Game, id: number, zone: number, stance: Stance): boolean {
  const f = myFleet(g, id);
  return !!f && sail(g, f, zone, stance);
}

/** Moves one of the player's fleets to a new home port. */
export function orderRebase(g: Game, id: number, region: number): boolean {
  const f = myFleet(g, id);
  return !!f && rebase(g, f, g.regions[region]);
}

export function orderShip(g: Game, c: Corps, to: number): boolean {
  return canOrder(g, c) && shipMove(g, c, g.regions[to]);
}

export function orderBuildCorps(g: Game, kind: Kind, region: number): Build | null {
  return humanUp(g) ? orderCorps(g, g.human!, kind, region) : null;
}

export function orderBuildFort(g: Game, region: number): Build | null {
  return humanUp(g) ? orderFort(g, g.human!, region) : null;
}

export function orderCancelBuild(g: Game, id: number): boolean {
  return humanUp(g) && cancelBuild(g, g.human!, id);
}

/** Ends the player's month and lets the computer play until the player is up again. Returns false if the player's nation has fallen. */
export function passMonth(g: Game): boolean {
  if (humanUp(g)) endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 80) {
    if (g.human === null || !g.nations[g.human].alive) return false;
    stepAI(g);
  }
  if (!humanUp(g)) return false;
  beginHuman(g);
  return true;
}

export function orderEvacuate(g: Game, c: Corps): boolean {
  return canOrder(g, c) && evacuate(g, c);
}

/** Nations the player could declare war on: neighbours by land that are not already friends or enemies. */
// ---- The player's generals ----------------------------------------------------------------

const myGeneral = (g: Game, id: number): General | null => {
  const gen = g.generals[id];
  return gen && gen.nat === g.human && current(g) === gen.nat ? gen : null;
};

/** Gives one of the player's generals a corps. */
export function orderAssign(g: Game, general: number, corps: number): boolean {
  const gen = myGeneral(g, general);
  const c = g.corps.find((x) => x.id === corps);
  return !!gen && !!c && c.nat === g.human && assignGeneral(g, gen, c);
}

export function orderRelieve(g: Game, general: number): boolean {
  const gen = myGeneral(g, general);
  return !!gen && relieve(g, gen);
}

export function orderSack(g: Game, general: number): boolean {
  const gen = myGeneral(g, general);
  return !!gen && sack(g, gen);
}

/** Promotes an unknown officer to command one of the player's corps. */
export function orderPromote(g: Game, corps: number): General | null {
  const c = g.corps.find((x) => x.id === corps);
  return c && canOrder(g, c) ? promote(g, c) : null;
}

export function orderSkill(g: Game, general: number, skill: Skill): boolean {
  const gen = myGeneral(g, general);
  return !!gen && spendPoint(gen, skill);
}

export function declarable(g: Game): string[] {
  const me = g.human;
  if (!me || !g.nations[me].alive) return [];
  const out = new Set<string>();
  const add = (o: string): void => {
    const N = g.nations[o];
    if (N && N.alive && !allied(g, me, o) && !atWar(g, me, o)) out.add(o);
  };
  for (const r of g.regions) {
    if (!allied(g, me, r.ctrl)) continue;
    for (const j of r.nb) add(g.regions[j].ctrl);
  }
  // A nation with troopships can also reach any coast within landing range of its own ports.
  if (g.nations[me].sealift > 0) {
    const depth = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (const r of g.regions) if (r.port && r.ctrl === me) for (const z of r.seas) if (depth[z] < 0) { depth[z] = 1; q.push(z); }
    for (let i = 0; i < q.length; i++) {
      for (const id of g.seas[q[i]].coast) add(g.regions[id].ctrl);
      if (depth[q[i]] >= NAVY.landing.range) continue;
      for (const j of g.seas[q[i]].adj) if (depth[j] < 0 && canPass(g, q[i], j, me)) { depth[j] = depth[q[i]] + 1; q.push(j); }
    }
  }
  return [...out];
}

export function orderDeclare(g: Game, on: string): boolean {
  if (!g.human || current(g) !== g.human || !declarable(g).includes(on)) return false;
  declareWar(g, g.human, on);
  ensureSupply(g);
  return true;
}

/** Spends the player's political capital to pull a neutral toward the player's bloc. */
export function orderCourt(g: Game, target: string): boolean {
  return humanUp(g) && court(g, g.human!, target);
}

export { KIND };
