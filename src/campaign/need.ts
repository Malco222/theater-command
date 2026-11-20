// Research by need (the sandbox's lock sweep, finding 7: what ruling 236's rider b554 did for a navy's boats, done for
// the laboratories; after release 1). At the stops tech.json's 'ai.need' names, a computer staff's research answers
// the war it is in rather than history's calendar:
//   the calendar's pull on its choice (tech.json ai 'early') is cut to the share its stop names;
//   a threat it has met pulls the technologies that answer it. A threat is met once it has come against the nation
//     in so many battles, or stood before it so many months running (the historian's thresholds, 19 November 2026:
//     historian/sandbox-thresholds.md, part 6), and stays met: the lesson is not unlearned;
//   a navy with a battle fleet no longer leaves carriers for last, and orders one once it has them (arsenal.ts);
//   which branch a staff favours is dealt from the war's seed as its character is (history.json 'lean').
// Nothing here names a nation. What is met is kept in the game's flags ('nd:'), written only at those stops and
// only for computer-run nations: below them, and for a player's own advisers, nothing is read and nothing written.
// (Airborne troops stay with what a computer staff leaves for last: none of them raises or drops an airborne corps.)

import techData from '../../data/campaign/tech.json';
import { atWar, frozen } from './game';
import { dialDraw, stopDef, stopOf } from './history';
import { NAVY } from './navy';
import type { AirMission, AirReport, BattleReport, Field, Game } from './types';

export interface NeedRules {
  stops: number[];
  /** The share of the calendar's pull ('early') that is left, by stop. */
  early: Record<string, number>;
  /** What each met threat takes off the score of a technology that answers it, and the most any technology gets. */
  pull: number; most: number;
  /** A threat is met after this many battles, or this many months running. */
  met: { battles: number; months: number };
  /** The heavy ships (battleships, carriers, cruisers) that make a battle fleet. */
  fleet: number;
  /** The share of the bombers sent that a raid must lose to count as one that cost too much. */
  bombers: number;
  /** The share of a convoy route's cargoes lost that counts as a month of the submarines' making. */
  bled: number;
  /** What a staff with a battle fleet no longer leaves for last. */
  fleetTechs: string[];
  /** The technologies that answer each threat. */
  threats: Record<string, string[]>;
}

export const NEED = (techData as unknown as { ai: { need?: NeedRules } }).ai.need;

/** The threats a staff can meet: tech.json ai.need 'threats' gives each its answers. */
export type Threat = 'armour' | 'fighters' | 'bombed' | 'bombers' | 'boats' | 'decks' | 'winter';
const THREATS: Threat[] = ['armour', 'fighters', 'bombed', 'bombers', 'boats', 'decks', 'winter'];

/** True where a nation's staff researches by need: a computer-run nation of the grand campaign, at a stop the rules name. */
export const needStop = (g: Game, nat: string): boolean => !!NEED && g.flags.open === 1 && nat !== g.human && NEED.stops.includes(stopOf(g));

/** The share of the calendar's pull a staff still feels: 1 wherever it does not research by need. */
export const needEarly = (g: Game, nat: string): number => (needStop(g, nat) ? NEED!.early[String(stopOf(g))] ?? 1 : 1);

const key = (k: string, nat: string, th: string): string => `nd:${k}:${nat}:${th}`;

/** True once a nation has met this threat. */
export const threatMet = (g: Game, nat: string, th: Threat): boolean => !!g.flags[key('on', nat, th)];

/** The threats a nation has met, with the month each was. */
export function threatsMet(g: Game, nat: string): { threat: Threat; since: number }[] {
  return THREATS.filter((th) => threatMet(g, nat, th)).map((th) => ({ threat: th, since: g.flags[key('on', nat, th)] }));
}

/** What a staff's met threats take off a technology's score: 'pull' for each that it answers, 'most' at the most. */
export function needPull(g: Game, nat: string, tech: string): number {
  if (!needStop(g, nat)) return 0;
  let k = 0;
  for (const th of THREATS) if (NEED!.threats[th]?.includes(tech) && threatMet(g, nat, th)) k++;
  return Math.min(NEED!.most, k * NEED!.pull);
}

/** The heavy ships a navy has: battleships, carriers and cruisers. */
const heavyShips = (g: Game, nat: string): number => g.fleets.reduce((s, f) => s + (f.nat === nat ? f.ships.bb + f.ships.cv + f.ships.ca : 0), 0);
/** True for a navy with a battle fleet: enough heavy ships that naval aircraft would have something to fight beside. */
export const battleFleet = (g: Game, nat: string): boolean => !!NEED && heavyShips(g, nat) >= NEED.fleet;
/** True for a technology a staff at these stops takes in its turn though the list leaves it for last: carriers, for a navy with a battle fleet. */
export const needKeeps = (g: Game, nat: string, tech: string): boolean => needStop(g, nat) && NEED!.fleetTechs.includes(tech) && battleFleet(g, nat);

/**
 * The order in which a staff favours the branches. Where it researches by need the order may be dealt afresh from the
 * war's seed, with the chance its stop gives a staff's character (history.json 'lean': half the time at stop 4, always
 * at stop 5), and a nation with no ships puts the naval branch last however the deal fell.
 */
export function needOrder(g: Game, nat: string, usual: string[]): string[] {
  if (!needStop(g, nat)) return usual;
  const lean = stopDef(g).lean;
  if (typeof lean !== 'number' || dialDraw(g, 'tech:deal', nat) >= lean) return usual;
  const dealt = usual.map((b) => [dialDraw(g, 'tech:order', nat, b), b] as [number, string]).sort((x, y) => x[0] - y[0]).map((x) => x[1]);
  const afloat = g.fleets.some((f) => f.nat === nat && f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd + f.ships.ss > 0);
  return afloat ? dealt : [...dealt.filter((b) => b !== 'naval'), 'naval'];
}

const BOMBING: AirMission[] = ['bomb', 'night', 'town', 'line', 'harbour'];
const AT_SHIPS: AirMission[] = ['fleet', 'harbour', 'strike'];

/** How many technologies of a branch of arms a nation has in service from the tree. */
const levelOf = (g: Game, nat: string, f: Field): number => {
  let k = 0;
  for (const id of g.nations[nat]?.tech?.done ?? []) if (FIELD[id] === f) k++;
  return k;
};
const FIELD: Record<string, Field | undefined> = Object.fromEntries((techData as unknown as { techs: { id: string; field?: Field }[] }).techs.map((t) => [t.id, t.field]));

/**
 * The month's watch for one nation, before its staff sets its free teams to work: what has come against it since
 * the last one. Enemy tanks as good as its own, in battle or on its front; enemy fighters as good as its own that
 * brought its aircraft down; bombers over its ground; raids of its own that lost too many bombers; submarines on
 * its convoy routes; carrier aircraft at its ships; and a winter in the deep east without the equipment for it.
 */
export function needWatch(g: Game, nat: string): void {
  if (!needStop(g, nat)) return;
  const n = g.nations[nat];
  if (!n?.alive || n.dormant) return;
  const hits: Partial<Record<Threat, number>> = {}, here: Partial<Record<Threat, boolean>> = {};
  const hit = (th: Threat): void => { hits[th] = (hits[th] ?? 0) + 1; };
  const foe = (e: string): boolean => atWar(g, nat, e);
  const armour = levelOf(g, nat, 'armour'), fighters = levelOf(g, nat, 'fighters');
  const tanksOf = (e: string): boolean => foe(e) && levelOf(g, e, 'armour') >= armour;

  // The land battles since the last watch: enemy tanks on the other side of one this nation fought in.
  const rb = g.flags[key('r', nat, 'land')] ?? 0;
  for (let i = rb; i < g.reports.length; i++) {
    const r: BattleReport = g.reports[i];
    const mine = r.attackers.some((u) => u.nat === nat) ? r.defenders : r.defenders.some((u) => u.nat === nat) ? r.attackers : null;
    if (mine?.some((u) => u.kind === 'arm' && !u.guard && tanksOf(u.nat))) hit('armour');
  }
  if (g.reports.length !== rb) g.flags[key('r', nat, 'land')] = g.reports.length;

  // The air battles since the last watch.
  const ra = g.flags[key('r', nat, 'air')] ?? 0;
  for (let i = ra; i < g.airReports.length; i++) {
    const r: AirReport = g.airReports[i];
    const att = r.att.some((s) => s.nat === nat), def = !att && (r.defNat === nat || r.def.some((s) => s.nat === nat));
    if (!att && !def) continue;
    const us = (att ? r.att : r.def).filter((s) => s.nat === nat), them = att ? r.def : r.att;
    const lost = us.reduce((s, x) => s + (x.lostFtr ?? 0) + (x.lostBmr ?? 0), 0);
    if (lost > 0 && them.some((s) => s.ftr > 0 && foe(s.nat) && levelOf(g, s.nat, 'fighters') >= fighters)) hit('fighters');
    if (def && r.defNat === nat && BOMBING.includes(r.mission) && r.att.some((s) => s.bmr > 0)) hit('bombed');
    if (def && r.defNat === nat && AT_SHIPS.includes(r.mission) && r.att.some((s) => s.at !== undefined)) hit('decks');
    if (att && r.attNat === nat && BOMBING.includes(r.mission)) {
      const sent = us.reduce((s, x) => s + x.bmr, 0), down = us.reduce((s, x) => s + (x.lostBmr ?? 0), 0);
      if (sent > 0 && down / sent >= NEED!.bombers) hit('bombers');
    }
  }
  if (g.airReports.length !== ra) g.flags[key('r', nat, 'air')] = g.airReports.length;

  // What stands before it this month.
  if (n.atWar.length) {
    const edge = new Set<number>();
    for (const r of g.regions) if (r.ctrl === nat) for (const j of r.nb) if (foe(g.regions[j].ctrl)) edge.add(j);
    here.armour = g.corps.some((c) => !c.dead && c.kind === 'arm' && edge.has(c.reg) && tanksOf(c.nat));
    here.winter = g.corps.some((c) => !c.dead && c.nat === nat && c.kind !== 'fort' && frozen(g, nat, g.regions[c.reg]));
    here.bombed = (g.raids[nat]?.turn ?? -9) >= g.turn - 1;
    here.boats = NAVY.convoys.some((c) => { const m = c.to === nat ? g.convoys[c.id] : undefined; return !!m && m.turn >= g.turn - 1 && (m.loss ?? 0) >= NEED!.bled; });
  }

  for (const th of THREATS) {
    if (threatMet(g, nat, th)) continue;
    const b = (g.flags[key('b', nat, th)] ?? 0) + (hits[th] ?? 0);
    const m = here[th] ? (g.flags[key('m', nat, th)] ?? 0) + 1 : 0;
    if (b >= NEED!.met.battles || m >= NEED!.met.months) {
      g.flags[key('on', nat, th)] = g.turn;
      delete g.flags[key('b', nat, th)];
      delete g.flags[key('m', nat, th)];
      continue;
    }
    if (b > 0) g.flags[key('b', nat, th)] = b; else delete g.flags[key('b', nat, th)];
    if (m > 0) g.flags[key('m', nat, th)] = m; else delete g.flags[key('m', nat, th)];
  }
}
