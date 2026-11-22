// Neighbours join the fight (release 2; the design director's ruling of 22 November 2026, 03:15Z, on Brian's
// "Neighbours join the fight"). Behind its own switch, stage.json's 'join' (JOIN in stage.ts), off as it ships: off,
// every battle is fought as release 1 fought it, and nothing here is read.
//
// The field moves where and when harm lands, never how much (b415/b418): so a corps that joins a battle is priced
// on the campaign card, in every battle, fought by hand or settled by the staffs, and the 3D field only draws it
// coming up. Nothing joins on the field that the card did not price.
//
// 1. Who: a corps of either side (an ally's too, where its country is at war with every nation on the other side)
//    standing in the field of a province next door to the battle, which its side holds, fed, that has fought no
//    battle since its own month began and joined none this month, and that could march into the battle province
//    this month by the move rules (the weeks it has in hand, the ground, the bridges, the line a computer staff
//    keeps to). Never the town's guard, which is no corps, nor a town's garrison or fortress troops. At most STACK
//    a side, the earliest to come up first. The player may hold his own corps back (orderHoldBack, turn.ts); a
//    computer staff, an ally's included, sends every corps it can.
// 2. When: in the march engine's own time (marchWeeks), never a fixed minute or a draw of the dice. It counts for the
//    share of the battle it is there for: early, most of its strength; late, little.
// 3. Both sides, every nation: assess() reckons the joiners, so the attack card, the battle and every staff's odds are
//    one reckoning.
// 4. They are in the odds and their losses are booked to them (game.ts), they stay where they stood, and their month
//    is spent: no weeks left, and counted as having fought.
// 5. The card names them, early, midway or late; to the player an enemy next door his staff has not identified is
//    "armies next door may join", with no names and no numbers (turn.ts, outlookOf).
// The historical scenarios reckon them alike: the rule is keyed on its switch alone.

import joinData from '../../data/campaign/join.json';
import { clamp } from '../core/math';
import type { TicketLine } from '../core/ticket';
import { km } from './air';
import { allied, atWar, moveCost } from './game';
import { armyOf } from './joint';
import { lineShut } from './line';
import { railCut, railsAt } from './rails';
import { blocked } from './rivers';
import { fieldCtrl } from './siege';
import { JOIN } from './stage';
import { STACK, type Corps, type Game, type JoinWord, type Kind, type Region } from './types';

export { JOIN };

/** The numbers of the rule (join.json): the month's window in weeks, the step of a full march, the least of it, the rail's cut, the least share. */
export const JOINING = joinData as unknown as { window: number; stepKm: number; nearest: number; rail: number; least: number };

/** True where corps next door join battles: the switch alone, in the grand campaign and the historical scenarios alike. */
export const joinRules = (): boolean => JOIN.on;

/** One corps that joins a battle from next door, as the card prices it and the field draws it. */
export interface Joiner {
  /** The corps itself (never saved: the report keeps its id). */
  corps: Corps;
  id: number; nat: string; name: string; kind: Kind;
  /** The province it comes from, and its name. */
  from: number; fromName: string;
  /** The weeks its march into the battle province takes, by the march engine. */
  weeks: number;
  /** The card's word for when it comes up. */
  word: JoinWord;
  /** The share of the battle it is there for, 0 to 1: what of its strength counts, and of the losses a corps in the whole fight would take. */
  share: number;
}
/** The joiners of one battle, each side's earliest first. */
export interface Join { att: Joiner[]; def: Joiner[] }
/** A joiner as the card and the 3D field read it: everything but the corps itself. */
export type JoinView = Omit<Joiner, 'corps'>;
export const joinView = (j: Joiner): JoinView => ({ id: j.id, nat: j.nat, name: j.name, kind: j.kind, from: j.from, fromName: j.fromName, weeks: j.weeks, word: j.word, share: j.share });

/** The card's word for a share of the battle: the first third of the month is early, the last late. */
export const joinWord = (share: number): JoinWord => (share > 2 / 3 ? 'early' : share > 1 / 3 ? 'midway' : 'late');

/** True where a working railway joins two provinces the corps' side has held since before this month. */
function byRail(g: Game, c: Corps, to: Region): boolean {
  const from = g.regions[c.reg];
  return railsAt(from.id).includes(to.id) && !railCut(g, from.id) && !railCut(g, to.id)
    && allied(g, c.nat, from.ctrl) && allied(g, c.nat, to.ctrl) && to.since < g.turn;
}

/**
 * The weeks a corps takes to march from where it stands into the battle province: the move's cost in the march
 * engine (moveCost: the road over ground held since before this month, the ground, the rivers and mines, mud and
 * frost, the railhead), shortened where the two centres are nearer than a full step and where a railway joins them.
 * The same game always gives the same weeks.
 */
export function marchWeeks(g: Game, c: Corps, target: Region): number {
  const part = clamp(km(g, c.reg, target.id) / JOINING.stepKm, JOINING.nearest, 1) * (byRail(g, c, target) ? JOINING.rail : 1);
  return moveCost(g, c, target, c.reg) * part;
}

/** The share of the battle a corps that marches so many weeks is there for. */
export const joinShare = (weeks: number): number => clamp(1 - weeks / JOINING.window, 0, 1);

/**
 * True where a corps standing in a province next door could come up in a battle there, side apart: in the field,
 * not held back, fed, with no battle of its own month behind it and none joined this month, on ground its side
 * holds, and able to march in this month by the move rules.
 */
export function mayJoin(g: Game, c: Corps, target: Region): boolean {
  if (c.dead || c.kind === 'fort' || c.town !== undefined || c.leave !== undefined || c.fortress || c.holdBack) return false;
  if (!c.supplied || c.fought || c.joinedIn === g.turn) return false;
  const here = g.regions[c.reg];
  if (!here.nb.includes(target.id) || !allied(g, c.nat, here.ctrl) || !allied(g, c.nat, fieldCtrl(g, here))) return false;
  // The move rules, as a march into the battle province would read them (moveCorps): the ground and the bridges, the
  // line a computer staff keeps to, and the weeks it has in hand.
  if (blocked(g, c, target) || (armyOf(c) !== g.human && lineShut(g, armyOf(c), target))) return false;
  const cost = moveCost(g, c, target, c.reg);
  return c.ap > 0 && c.ap >= Math.min(cost, 4);
}

/**
 * The corps next door that join this battle, each side's earliest first, at most STACK a side; null where nobody
 * does, or the switch is off. Not for a storm of a town or a garrison's sortie: those are fought by the army round
 * the town and the garrison inside it. Reads the game and leaves it as it was.
 */
export function joinersOf(g: Game, attackers: Corps[], target: Region, defenders: Corps[], opts: { town?: boolean; sally?: boolean } = {}): Join | null {
  if (!JOIN.on || opts.town || opts.sally || !attackers.length || !defenders.length) return null;
  const attNat = armyOf(attackers[0]), defNat = defenders[0].nat;
  const aN = [...new Set(attackers.map((c) => c.nat))], dN = [...new Set(defenders.map((c) => c.nat))];
  const inBattle = new Set<number>();
  for (const c of attackers) inBattle.add(c.id);
  for (const c of defenders) inBattle.add(c.id);
  const att: (Joiner & { km: number })[] = [], def: (Joiner & { km: number })[] = [];
  for (const j of target.nb) {
    const r = g.regions[j];
    for (const c of r.units) {
      if (inBattle.has(c.id)) continue;
      // Its side: the attack's or the defence's, and at war with every nation on the other side.
      const forA = allied(g, c.nat, attNat) && dN.every((d) => atWar(g, c.nat, d));
      const forD = !forA && allied(g, c.nat, defNat) && aN.every((a) => atWar(g, c.nat, a));
      if ((!forA && !forD) || !mayJoin(g, c, target)) continue;
      const weeks = marchWeeks(g, c, target), share = joinShare(weeks);
      if (share < JOINING.least) continue;
      (forA ? att : def).push({ corps: c, id: c.id, nat: c.nat, name: c.name, kind: c.kind, from: r.id, fromName: r.name, weeks, word: joinWord(share), share, km: km(g, r.id, target.id) });
    }
  }
  if (!att.length && !def.length) return null;
  const first = (xs: (Joiner & { km: number })[]): Joiner[] => xs.sort((x, y) => x.weeks - y.weeks || x.km - y.km || x.id - y.id).slice(0, STACK).map(({ km: _, ...j }) => j);
  return { att: first(att), def: first(def) };
}

const pct = (x: number): string => `${Math.round(x * 100)}%`;
const listAnd = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** "Early: XV Corps from Upper Silesia; midway: X Corps from Lower Silesia": who comes up, and when. */
function whoWhen(js: Joiner[]): string {
  const out: string[] = [];
  for (const w of ['early', 'midway', 'late'] as JoinWord[]) {
    const these = js.filter((j) => j.word === w);
    if (these.length) out.push(`${out.length ? w : w[0].toUpperCase() + w.slice(1)}: ${these.map((j) => `${j.name} from ${j.fromName}`).join(', ')}`);
  }
  return out.join('; ');
}

/** The battle card's lines for the joiners, one a side that has any: who, when, and what of each counts. */
export function joinLines(join: Join): TicketLine[] {
  const out: TicketLine[] = [];
  const say = (label: string, js: Joiner[]): void => {
    if (!js.length) return;
    out.push({ label, value: whoWhen(js), effect: `${js.length === 1 ? 'It comes' : 'They come'} up from next door and ${js.length === 1 ? 'counts' : 'count'} for the part of the battle ${js.length === 1 ? 'it is' : 'they are'} in: ${listAnd(js.map((j) => `${j.name} ${pct(j.share)}`))}. ${js.length === 1 ? 'It stays' : 'They stay'} where ${js.length === 1 ? 'it stands' : 'they stand'}, and ${js.length === 1 ? 'its' : 'their'} month is spent` });
  };
  say(JOIN_ATTACK, join.att);
  say(JOIN_DEFENCE, join.def);
  return out;
}
/** The card's labels for the two sides' joiners. */
export const JOIN_ATTACK = 'Joining the attack';
export const JOIN_DEFENCE = 'Joining the defence';
/** What the player's card says where the enemy next door is not identified (b453): no names, no numbers. */
export const MAY_JOIN = 'Armies next door may join';

/** A joiner's month is spent: no weeks left, counted as having fought, and joined this month. It stays where it stood. */
export function spendJoiner(g: Game, c: Corps): void {
  c.fought = true;
  c.ap = 0;
  c.joinedIn = g.turn;
}
