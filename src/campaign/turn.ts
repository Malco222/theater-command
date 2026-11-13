// The monthly turn: each nation in order gets its month, the computer plays the nations
// the player does not, and the player's orders go through the same rules.

import { KIND, STACK, type Agent, type AirFleet, type AirMission, type BattleReport, type Build, type Cash, type Corps, type Field, type Fleet, type Game, type General, type Kind, type Mission, type Posture, type Region, type Skill, type Stance } from './types';
import { airBattles, airBases, airTargets, previewAir, rebaseAir, setMission, type AirAssess } from './air';
import { NAVY, canPass, landCorps, landingTargets, planLanding, rebase, sail, seaBattles, whyNotShip, type Landing, type LandingPlan } from './navy';
import { DIFFICULTY, levelOf, setLevel, whyNotLevel, type Level } from './difficulty';
import { assignGeneral, promote, relieve, sack, spendPoint } from './commanders';
import {
  allied, assess, atWar, declareWar, ensureSupply, evacuate, friendlyUnits, hasRoom, hostileUnits, limitAttackers,
  endPhase, moveCorps, moveCost, openTheatres, railMove, resolveBattle, shipMove, sovietEvent, startPhase, type Assessment,
} from './game';
import { cancelBuild, orderCorps, orderFort } from './economy';
import { offers, orderOffer, type Offer } from './arsenal';
import { blocked, blowBridges, crossingOf, whyNotBlow } from './rivers';
import { cancelJob, jobsFor, orderJob, type JobKind, type JobOffer } from './engineers';
import { tearUp, whyNotCutRail } from './rails';
import { foundBureau, setTeams } from './research';
import { startResearch, stopResearch, techView, whyNotResearch, type TechView } from './tech';
import { seaLanes, stores, type Lane, type StoresView } from './resources';
import {
  armiesSeen, booksOf, cityReport, recruit, retire, sendAgent, setFog, setMission as setAgentMission, sightOf, spyRoom,
  whyNotMission, whyNotRecruit, whyNotSend, type ArmySeen, type Books, type CityReport, type SpyRoom,
} from './intel';
import { phaseFor, runAI } from './ai';
import { court, monthlyDiplomacy, warForecast, type WarForecast } from './diplomacy';
import { founders, invite, invites, pathOptions, takePath, whyNotInvite, type Invite, type PathId, type PathOption } from './league';
import { awardOf, cashHonour } from './stories';
import { contenders, dominating, keepPeace, startDomination, warDecided, whyNotDomination, worldBoard, worldWinner, type Decided, type WorldBoard } from './domination';
import EVE from '../../data/campaign/eve.json';

export const current = (g: Game): string => g.order[g.phase];

function closePhase(g: Game, nat: string): void {
  // The month's air campaigns and fleet actions are fought once the nation's orders are all given.
  airBattles(g, nat);
  seaBattles(g, nat);
  endPhase(g, nat);
  if (nat === 'GER') sovietEvent(g);
  g.phase++;
  if (g.phase >= g.order.length) {
    g.phase = 0; g.turn++; g.week = 1;
    openTheatres(g);
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
  entrench(g, current(g));
  closePhase(g, current(g));
}

/**
 * Troops under orders to dig in who spent the whole month where they stood are entrenched from now on. Anyone
 * who marched, went by rail or attacked is not, and has to sit a month out before the trenches are ready again.
 */
function entrench(g: Game, nat: string): void {
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead) continue;
    if (c.digs && c.kind !== 'fort' && c.ap >= c.apMax && !c.fought) c.dug = true;
    else if (c.dug) delete c.dug;
  }
}

// ---- The eve of war ------------------------------------------------------------------------

/** The nation history's plan sends against the player before the player's first month comes round, if any. */
export function eveAttacker(g: Game): string | null {
  const me = g.human;
  if (!me || g.turn !== 1) return null;
  return g.order.slice(0, g.order.indexOf(me)).find((n) => {
    const ph = phaseFor(g, n);
    return ph?.posture === 'attack' && (!!ph.declare?.includes(me) || (atWar(g, n, me) && !!ph.enemies?.includes(me)));
  }) ?? null;
}

/** True while the player is placing an army on the eve of war. */
export const onEve = (g: Game): boolean => g.flags.eve === 1;

/**
 * The eve of war. A player whose country is attacked in the opening month before its own turn comes round would
 * otherwise watch it fall without giving an order, so that player places the army first: marches and rail moves
 * on friendly ground, on the corps' opening action points. No shot is fired, no month passes, nothing is
 * produced and nobody else moves.
 */
export function beginEve(g: Game): boolean {
  if (g.phase !== 0 || g.flags.eve !== undefined || !eveAttacker(g)) return false;
  g.flags.eve = 1;
  g.phase = g.order.indexOf(g.human!);
  g.week = 1;
  return true;
}

export interface EvePlan { title: string; advice: string[]; place: [string, number][] }

/** What the staff advises on the eve of war, if the player's nation has such advice and the eve is not over. */
export function evePlan(g: Game): EvePlan | null {
  if (!onEve(g) || !g.human) return null;
  return (EVE as unknown as Record<string, EvePlan | undefined>)[g.human] ?? null;
}

/**
 * Places the army as the staff advises, by rail, strongest corps first. Only corps that have not moved yet can go,
 * so it does the most when it is the first thing done. Returns how many corps it moved.
 */
export function orderEvePlan(g: Game): number {
  const plan = evePlan(g);
  if (!plan) return 0;
  const army = g.corps.filter((c) => canOrder(g, c)).sort((a, b) => b.divs * b.str - a.divs * a.str || a.id - b.id);
  const want = new Map<number, number>();
  let k = 0;
  for (const [name, n] of plan.place) {
    const r = g.byName[name];
    for (let i = 0; i < n && k < army.length; i++, k++) if (r) want.set(army[k].id, r.id);
  }
  let moved = 0;
  // Several passes: a corps can only go where there is room, and some have to leave first.
  for (let pass = 0; pass < 6; pass++) {
    for (const c of army) {
      const to = want.get(c.id);
      if (to !== undefined && c.reg !== to && orderRail(g, c, to)) moved++;
    }
  }
  return moved;
}

/** Ends the eve of war: the month begins with whoever moves first. */
export function endEve(g: Game): void {
  if (!onEve(g)) return;
  // An army left in the positions it held all summer has had time to dig them.
  entrench(g, g.human!);
  g.flags.eve = 0;
  g.phase = 0;
  g.week = 1;
}

/** The levels a grand campaign can be played at, easiest first, and the one this game is on. */
export const levels = (): Level[] => DIFFICULTY.levels;
export const levelNow = (g: Game): Level => levelOf(g);

/** Why the player cannot put the campaign on this level, or null if he can. */
export function whyNotSetLevel(g: Game, id: string): string | null {
  if (g.human === null) return 'Nobody is playing this war.';
  return whyNotLevel(g, id);
}

/** Puts the campaign on another level. It can be done in any month and takes hold at once. */
export function orderLevel(g: Game, id: string): boolean {
  return whyNotSetLevel(g, id) === null && setLevel(g, id);
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
  // A corps sent home on a bond tour or to the schools takes no orders until it is back.
  return !c.dead && c.nat === g.human && current(g) === c.nat && c.kind !== 'fort' && c.leave === undefined;
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
      if (!friendly && (onEve(g) || !atWar(g, c.nat, r.ctrl))) continue; // on the eve of war nobody crosses a frontier
      if (blocked(g, c, r, a)) continue; // tanks and the steepest ground, wheels and a river with its bridges down
      const step = moveCost(g, c, r, a);
      const left = c.ap - ca;
      if (left <= 0 || left < Math.min(step, 4)) continue;
      const cost = ca + step;
      if (hostileUnits(g, r, c.nat).length) {
        if (a === c.reg && !out.has(j)) out.set(j, { cost: 1, prev: a, attack: true });
        continue;
      }
      if (!hasRoom(g, r, c)) continue;
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
  if (onEve(g)) return null;
  const ok = limitAttackers(g, corps.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id)), target);
  if (!ok.length || !hostileUnits(g, target, ok[0].nat).length) return null;
  return resolveBattle(g, ok, target);
}

/**
 * The player's standing order for a corps that is attacked: stand and fight, dig in where it is, or give ground
 * when the blow would break it. Digging takes a month spent on the spot, so the order pays from the month after.
 */
export function orderStance(g: Game, c: Corps, posture: Posture): boolean {
  if (!canOrder(g, c)) return false;
  if (posture === 'back') c.yields = true; else delete c.yields;
  if (posture === 'dig') c.digs = true; else { delete c.digs; delete c.dug; }
  return true;
}

/** The standing order a corps is under. */
export const postureOf = (c: Corps): Posture => (c.yields ? 'back' : c.digs ? 'dig' : 'stand');

/** What the player's engineer corps could do where it stands this month (the grand campaign). */
export function engineerJobs(g: Game, c: Corps): JobOffer[] {
  return c.nat === g.human ? jobsFor(g, c) : [];
}

/** Sets one of the player's engineer corps to work for the month. */
export function orderEngineers(g: Game, c: Corps, job: JobKind): boolean {
  return canOrder(g, c) && orderJob(g, c, job);
}

/** Takes one of the player's engineer corps off the work it was given this month. */
export function orderStopWork(g: Game, c: Corps): boolean {
  return c.nat === g.human && humanUp(g) && cancelJob(g, c);
}

/** Why the player cannot blow the bridges between two regions from the first of them, or null if they can. */
export function whyNotBlowBridges(g: Game, from: number, to: number): string | null {
  const x = crossingOf(from, to);
  if (!x) return 'There is no river between those two.';
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotBlow(g, g.human, x, from);
}

/** The player's order to blow the bridges between a region their troops stand in and its neighbour across the river. */
export function orderBlowBridges(g: Game, from: number, to: number): boolean {
  const x = crossingOf(from, to);
  return !!x && g.human !== null && humanUp(g) && blowBridges(g, g.human, x, from);
}

/** Why the player cannot tear up the railway of a province, or null if they can (the grand campaign). */
export function whyNotCutRailway(g: Game, region: number): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotCutRail(g, g.human, region);
}

/** The player's order to tear up the railway of a province their troops stand in. Only engineers can relay it. */
export function orderCutRailway(g: Game, region: number): boolean {
  return g.human !== null && humanUp(g) && tearUp(g, g.human, region);
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
  return cs.length && !onEve(g) && cs.every((c) => canOrder(g, c)) ? landingTargets(g, cs) : [];
}

/** The odds and the dangers of a landing, before the order is given. */
export function previewLanding(g: Game, cs: Corps[], to: number): LandingPlan {
  return planLanding(g, cs, g.regions[to]);
}

/** Sends the chosen corps across the sea to land on an enemy coast. */
export function orderLand(g: Game, cs: Corps[], to: number): Landing {
  if (!cs.length || onEve(g) || !cs.every((c) => canOrder(g, c))) return { ok: false, why: 'Those corps cannot be given orders now.' };
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

const myAir = (g: Game, id: number): AirFleet | null => {
  const f = g.airFleets.find((x) => x.id === id);
  return f && f.nat === g.human && current(g) === f.nat ? f : null;
};

/** Gives one of the player's air fleets its mission for the month: a region to fight over or bomb, or a sea to strike. */
export function orderAir(g: Game, id: number, mission: AirMission, target = -1): boolean {
  const f = myAir(g, id);
  return !!f && setMission(g, f, mission, target);
}

/** Moves one of the player's air fleets to airfields in another friendly region. */
export function orderAirBase(g: Game, id: number, region: number): boolean {
  const f = myAir(g, id);
  return !!f && rebaseAir(g, f, g.regions[region]);
}

/** Where one of the player's air fleets could be sent on a mission, and where it could be based. */
export function airOptions(g: Game, id: number, mission: AirMission): { targets: number[]; bases: number[] } {
  const f = myAir(g, id);
  return f ? { targets: airTargets(g, f, mission), bases: airBases(g, f).map((r) => r.id) } : { targets: [], bases: [] };
}

/** The odds and the dangers of an air mission, before the order is given. */
export function previewAirOrder(g: Game, id: number, mission: AirMission, target: number): AirAssess | null {
  const f = g.airFleets.find((x) => x.id === id);
  return f ? previewAir(g, f, mission, target) : null;
}

export function orderBuildCorps(g: Game, kind: Kind, region: number, revive?: number): Build | null {
  return humanUp(g) ? orderCorps(g, g.human!, kind, region, revive) : null;
}

/** Cashes in one of the player's honours: its heroes go home on a bond tour, or to the schools as instructors. */
export function orderCash(g: Game, award: number, how: Cash): boolean {
  return humanUp(g) && cashHonour(g, award, how);
}

/** The player has read a citation. */
export function orderSeen(g: Game, award: number): void {
  const a = awardOf(g, award);
  if (a && a.nat === g.human) a.seen = true;
}

/** The player's standing order for a corps: take drafts as they come, or keep the veterans together and take none. */
export function orderDrafts(g: Game, c: Corps, take: boolean): boolean {
  if (c.dead || c.nat !== g.human || c.kind === 'fort') return false;
  if (take) delete c.lean; else c.lean = true;
  return true;
}

/**
 * Everything the player's nation could order from its arsenal, with one province in mind (the one picked on the
 * map, or -1): corps, aircraft, ships and works, each with its price, its upkeep, where it would go and, when it
 * cannot be ordered, why not. Both interfaces draw their build screens from this one list.
 */
export function arsenalOffers(g: Game, region = -1): Offer[] {
  return g.human ? offers(g, g.human, region) : [];
}

/** Places one of those orders: the key of the offer ('corps:arm', 'air:ftr', 'ship:dd', 'work:aa') and the province in mind. */
export function orderArsenal(g: Game, key: string, region = -1): Build | null {
  return humanUp(g) ? orderOffer(g, g.human!, key, region) : null;
}

export function orderBuildFort(g: Game, region: number): Build | null {
  return humanUp(g) ? orderFort(g, g.human!, region) : null;
}

export function orderCancelBuild(g: Game, id: number): boolean {
  return humanUp(g) && cancelBuild(g, g.human!, id);
}

/**
 * The technology tree as the player's nation sees it (the grand campaign): every technology with its branch and
 * step, what it does in words, what it needs, whether it is researched, in work, open or locked, and why it
 * cannot be started now. Null in a historical scenario, or with nobody at the controls.
 */
export function techTree(g: Game): TechView | null {
  return g.human ? techView(g, g.human) : null;
}

/**
 * Oil and supplies (the grand campaign): the books of the player's nation, or of any power that keeps stocks. For
 * each of the two: what is in the depots and the most they hold, every source and every use for the month, months
 * left at this rate, the share of the need that was met, and what a shortage is costing in words; and every lane and
 * overland deal that feeds the nation, with the share getting through. Null in a historical scenario, and for a
 * small country, which buys what it needs abroad and is never short.
 */
export function depots(g: Game, nat: string | null = g.human): StoresView | null {
  return nat ? stores(g, nat) : null;
}

/**
 * Every shipping lane on the map this month, whoever it feeds: the points to draw it through, the seas where it can
 * be raided or guarded, the share of its cargoes getting through, and whether it is open, raided or cut.
 */
export function shippingLanes(g: Game): Lane[] {
  return seaLanes(g);
}

// ---- Spies and the fog of war (the grand campaign) -----------------------------------------------------

/**
 * What the player can see of every province, by region id: 2 where an army is seen whole, 1 where only its flag
 * and the number of its corps are known, 0 where nothing is known. Who holds the ground is always known. Everything
 * is 2 in a historical scenario, with nobody at the controls, or once the player has lifted the fog.
 */
export function sight(g: Game): Uint8Array {
  return sightOf(g, g.human);
}

/**
 * Every army the player knows of, one row for each nation's force in a province: the number of corps, and the
 * divisions it has left where it is seen whole (null where only glimpsed). Armies in unseen ground are not listed:
 * a map drawn from this list shows exactly what the player is allowed to know.
 */
export function visibleArmies(g: Game): ArmySeen[] {
  return armiesSeen(g, g.human);
}

/**
 * The secret service's room: every agent with its rank, where it is, what it is doing, the chance of being caught
 * this month, the work it can be given there (with the reason for any it cannot) and the armies it has in sight;
 * how many agents the service can run and what one costs; the service's file of what has happened; the nations
 * whose books are being read; and the chance of the other side's saboteurs striking at home. Null outside the grand campaign.
 */
export function secretService(g: Game): SpyRoom | null {
  return spyRoom(g, g.human);
}

/** Why the player cannot recruit an agent now, or null if he can. */
export function whyNotRecruitAgent(g: Game): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotRecruit(g, g.human);
}

/** Recruits an agent for the player: paid for at once, trained at home, ready for orders next month. */
export function orderRecruitAgent(g: Game): Agent | null {
  return whyNotRecruitAgent(g) ? null : recruit(g, g.human!);
}

/** Why the player's agent cannot be sent to a province, or null if it can. */
export function whyNotSendAgent(g: Game, id: number, region: number): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotSend(g, g.human, id, region);
}

/** Sends the player's agent to any province on the map: a month on the road, then watching abroad or on guard at home. */
export function orderSendAgent(g: Game, id: number, region: number): boolean {
  return whyNotSendAgent(g, id, region) === null && sendAgent(g, g.human!, id, region);
}

/** Why the player's agent cannot be given this work where it stands, or null if it can. */
export function whyNotAgentMission(g: Game, id: number, mission: Mission): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotMission(g, g.human, id, mission);
}

/** Gives the player's agent its work: 'watch', 'books', 'factory', 'rail', 'oil' or 'guard'. */
export function orderAgentMission(g: Game, id: number, mission: Mission): boolean {
  return whyNotAgentMission(g, id, mission) === null && setAgentMission(g, g.human!, id, mission);
}

/** Retires one of the player's agents, which frees a place in the service. */
export function orderRetireAgent(g: Game, id: number): boolean {
  return !!g.human && humanUp(g) && retire(g, g.human, id);
}

/** Lifts the fog of war for this campaign, or lays it again. Agents and sabotage go on either way. */
export function orderFog(g: Game, on: boolean): boolean {
  return !!g.human && setFog(g, on);
}

/** A foreign government's books as the player's agent in its capital reads them, or null where none is reading. An ally's are always open. */
export function foreignBooks(g: Game, nat: string): Books | null {
  return g.human ? booksOf(g, nat, g.human) : null;
}

/** What the player's agent in a province reports of it beyond its armies: what is on order there and the air fleets based there. Null without one. */
export function agentReport(g: Game, region: number): CityReport | null {
  return g.human ? cityReport(g, region, g.human) : null;
}

// ---- World Domination: the last act of the grand campaign ------------------------------------------------

/** Where the campaign stands with its last act: whether the war between the alliances is decided, and what the player has chosen. */
export interface LastAct {
  /** How the war between the alliances was decided, or null while it is still being fought. */
  decided: Decided | null;
  /** True while the player may still choose World Domination: the war is decided and the last act has not begun. */
  open: boolean;
  /** Why it cannot be chosen today, in words, or null. */
  why: string | null;
  /** The month the player took the victory and left the world as it is, or 0. The last act stays open after that. */
  peace: number;
  /** True once the last act has begun, and the month it did. */
  on: boolean;
  since: number;
  /** How many nations still stand. */
  left: number;
  /** The nation that has won the last act, or null. */
  winner: string | null;
}

/** The state of the campaign's last act, for an interface. Null outside the grand campaign or with nobody at the controls. */
export function lastAct(g: Game): LastAct | null {
  if (g.flags.open !== 1 || !g.human) return null;
  const why = whyNotDomination(g, g.human);
  return {
    decided: warDecided(g), open: why === null, why,
    peace: g.flags['dom:peace'] ?? 0,
    on: dominating(g), since: g.flags.dom ?? 0,
    left: contenders(g).length,
    winner: worldWinner(g),
  };
}

/** Why the player cannot open the last act now, or null when he can. */
export function whyNotOrderDomination(g: Game): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotDomination(g, g.human);
}

/** The player chooses World Domination: every alliance is dissolved and every nation still standing is at war with every other. It cannot be undone. */
export function orderDomination(g: Game): boolean {
  return whyNotOrderDomination(g) === null && startDomination(g, g.human);
}

/** The player takes the victory and leaves the world as it is. World Domination can still be chosen in a later month. */
export function orderKeepPeace(g: Game): boolean {
  return !!g.human && humanUp(g) && keepPeace(g);
}

/** The standings of the world: who still stands, how much of the map each holds, and who has fallen to whom. Foreign armies are as well known as the fog allows. */
export function standings(g: Game): WorldBoard {
  return worldBoard(g, g.human);
}

/** Why the player cannot begin this technology now, or null when a free research team can start on it. */
export function whyNotOrderResearch(g: Game, id: string): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  return whyNotResearch(g, g.human, id);
}

/** Puts one of the player's free research teams to work on a technology. */
export function orderResearch(g: Game, id: string): boolean {
  return humanUp(g) && startResearch(g, g.human!, id);
}

/** Takes the player's team off a technology. The work done on it is lost. */
export function orderStopResearch(g: Game, id: string): boolean {
  return humanUp(g) && stopResearch(g, g.human!, id);
}

/** Puts this many of the player's design teams on a branch of research. */
export function orderTeams(g: Game, field: Field, teams: number): boolean {
  return humanUp(g) && setTeams(g, g.human!, field, teams);
}

/** Founds a new design team for the player, paid for from production. */
export function orderBureau(g: Game): boolean {
  return humanUp(g) && foundBureau(g, g.human!);
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
  if (!me || !g.nations[me].alive || onEve(g)) return [];
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

/**
 * Ground the player's corps can march into only by declaring war: the land of a country he is at peace with, next
 * to where one of the corps stands. Each region is given with the country that holds it. Either interface shows
 * the declaration prompt (previewDeclare) when a corps is sent there, and nothing moves unless the player says yes.
 */
export function invasions(g: Game, cs: Corps[]): Map<number, string> {
  const out = new Map<number, string>();
  if (!humanUp(g) || onEve(g)) return out;
  const can = new Set(declarable(g));
  if (!can.size) return out;
  for (const c of cs) {
    if (!canOrder(g, c) || c.ap <= 0) continue;
    for (const j of g.regions[c.reg].nb) {
      const r = g.regions[j];
      if (can.has(r.ctrl) && !blocked(g, c, r, c.reg)) out.set(j, r.ctrl);
    }
  }
  return out;
}

/** What declaring war on a country would mean: who it joins, whether the blow is a surprise, its friends, and how the neutrals take it. */
export function previewDeclare(g: Game, on: string): WarForecast | null {
  return g.human && declarable(g).includes(on) ? warForecast(g, g.human, on) : null;
}

// ---- A small country's path, and the League ------------------------------------------------------------------

/**
 * The paths open to a small country in the player's hands: join the Axis, join the Allies, stand alone, found the
 * League or join it, each with whether it can be taken today and why not. Empty for a great power, for a watcher,
 * and outside the grand campaign. Either interface shows them on one card.
 */
export function paths(g: Game): PathOption[] {
  return g.human ? pathOptions(g, g.human) : [];
}

/** The countries the player could found the League with, each with its answer and the reason. */
export function leaguePartners(g: Game): Invite[] {
  return g.human && paths(g).length ? founders(g, g.human) : [];
}

/** Takes a path. Joining an alliance cannot be undone. Founding the League needs the partner who founds it with the player. */
export function orderPath(g: Game, id: PathId, partner?: string): boolean {
  return humanUp(g) && !onEve(g) && takePath(g, g.human!, id, partner);
}

/** The countries the player, as a member of the League, could invite, each with its answer and the reason. */
export function leagueInvites(g: Game): Invite[] {
  return g.human ? invites(g, g.human) : [];
}

/** Why the player cannot invite this country into the League today, or null if he can. */
export function whyNotLeagueInvite(g: Game, target: string): string | null {
  if (!g.human || !humanUp(g) || onEve(g)) return 'Not in your month.';
  return whyNotInvite(g, g.human, target);
}

/** Invites a willing country into the League, for political capital. */
export function orderLeagueInvite(g: Game, target: string): boolean {
  return whyNotLeagueInvite(g, target) === null && invite(g, g.human!, target);
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
