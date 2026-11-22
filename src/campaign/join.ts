// Neighbours join the fight (release 2; the design director's ruling of 22 November 2026, 03:15Z, on Brian's
// "Neighbours join the fight"). Behind its own switch, stage.json's 'join' (JOIN in stage.ts), off as it ships: off,
// every battle is fought as release 1 fought it, and nothing here is read.
//
// The field moves where and when harm lands, never how much (b415/b418): so a corps that joins a battle is priced
// on the campaign card, in every battle, fought by hand or settled by the staffs, and the 3D field only draws it
// coming up. Nothing joins on the field that the card did not price.
//
// 1. Who: a corps of either side (an ally's too, where its country is at war with every nation on the other side)
//    standing in the field of a province next door to the battle, which its side holds, fed, that has had no battle
//    yet this month and joined none, and that could march into the battle province this month by the move rules (the
//    weeks it has in hand this month, the ground, the bridges, the line a computer staff keeps to). The month is the
//    calendar's, whoever's phase it is: a corps whose nation plays later in the month has its whole month still in
//    hand, whatever it did last month (weeksInHand, battleThisMonth). Never the town's guard, which is no corps, nor a
//    town's garrison or fortress troops. At most STACK a side, the earliest to come up first. The player may hold his
//    own corps back (orderHoldBack, turn.ts: on the tray for an army, on the attack card corps by corps). Who stays
//    (the design director's ruling 3, 11:09Z): a computer staff's corps under a hold stays, as the Berlin garrison and
//    the Ploiesti guard do (a hold binds a staff, never the player's own corps); and so does one that the staff's own
//    front rule keeps where it is. That rule is screen (ai.ts advance, game.ts screens): two corps stay to screen any
//    place that faces the enemy. A corps that has joined a battle this month, or is taken for this one, is gone from
//    its province's count, so of a province facing the enemy all but two may come up in a month. The player's corps
//    are under no front rule. Otherwise a staff, an ally's included, sends every corps it can, save the main body it
//    keeps for a second wave (ai.ts, twoWaves: AttackOpts 'spare').
// 2. When: by the game's own move rules, one map for the joiner, the staffs and the player (the design director's
//    ruling 1, 11:09Z). The week of its month it comes up in is weekOf of its move into the battle province (moveCost),
//    out of the weeks the corps has in hand this month (marchWeeks): never a fixed minute or a draw of the dice. No
//    corps comes up by train: the railway takes the whole month. It counts for the part of the battle it is there for:
//    up in the first week, three quarters of it (early); in the second, half (midway); in the third, a quarter (late).
//    A corps that comes up in the last week is no part of the battle.
// 3. Both sides, every nation: assess() reckons the joiners, so the attack card, the battle and every staff's odds are
//    one reckoning. (A staff's second wave is reckoned without those its first will have spent.)
// 4. They are in the odds and their losses are booked to them (game.ts), they stay where they stood, and their month
//    is spent: no weeks left, and counted as having fought; one in the player's army as well (joint.ts, jointMonth).
//    A corps on foot that comes up into open desert against tanks and lorries is driven round as the defenders there are.
// 5. The card names them, early, midway or late; to the player an enemy next door his staff has not identified is
//    "armies next door may join", with no names and no numbers (turn.ts, outlookOf). A storm of a town and a
//    garrison's sortie take nobody from next door, on either side, and their cards say so in one line (noJoinLine).
// The historical scenarios reckon them alike: the rule is keyed on its switch alone.

import joinData from '../../data/campaign/join.json';
import { clamp } from '../core/math';
import type { TicketLine } from '../core/ticket';
import { NO_MASK, allied, atWar, moveCost, screens, weekOf } from './game';
import { armyOf } from './joint';
import { lineShut } from './line';
import { blocked } from './rivers';
import { fieldCtrl } from './siege';
import { JOIN } from './stage';
import { STACK, type Corps, type Game, type JoinWord, type Kind, type Region } from './types';

export { JOIN };

/** The one number of the rule (join.json): the month's window in weeks. */
export const JOINING = joinData as unknown as { window: number };

/** True where corps next door join battles: the switch alone, in the grand campaign and the historical scenarios alike. */
export const joinRules = (): boolean => JOIN.on;

/** One corps that joins a battle from next door, as the card prices it and the field draws it. */
export interface Joiner {
  /** The corps itself (never saved: the report keeps its id). */
  corps: Corps;
  id: number; nat: string; name: string; kind: Kind;
  /** The province it comes from, and its name. */
  from: number; fromName: string;
  /** The week of the month it comes up in (weekOf), 1 to 3. */
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

/**
 * The week of its own month a corps comes up in, by the move rules: its move into the battle province costs what any
 * march there costs (moveCost: the road over ground held since before this month, the ground, the rivers and mines,
 * mud and frost, the railheads), and weekOf reads that out of the weeks the corps has in hand this month
 * (weeksInHand), as it does for every move. A whole week, 1 to 4. The same game always gives the same week.
 */
export function marchWeeks(g: Game, c: Corps, target: Region): number {
  return weekOf(c, moveCost(g, c, target, c.reg), weeksInHand(g, c));
}

/** The share of the battle a corps that comes up in that week is there for: 1 less the week over the window. */
export const joinShare = (weeks: number): number => clamp(1 - weeks / JOINING.window, 0, 1);

/**
 * True where a nation's month has begun: its phase has come round this month, so that what its corps have spent and
 * fought since is this month's. (A nation outside the order of play reads as it stands.)
 */
function begun(g: Game, nat: string): boolean {
  const at = g.order.indexOf(nat);
  return at < 0 || at <= g.phase;
}

/**
 * The weeks a corps has in hand this month: what is left of them where the month of the army it marches with has
 * begun (or where it left that army this month, its month had with it), and the whole month where that month is
 * still to come. What it did not spend last month is no part of this one.
 */
export function weeksInHand(g: Game, c: Corps): number {
  return begun(g, armyOf(c)) || g.flags[`back:${c.id}`] === g.turn ? c.ap : c.apMax;
}

/**
 * True where a corps has had a battle this month, in whoever's phase it was fought: joined one, fought one since its
 * own month began, or fought one this month before that (foughtIn). A battle of last month is not this month's.
 */
export function battleThisMonth(g: Game, c: Corps): boolean {
  return c.joinedIn === g.turn || c.foughtIn === g.turn || (c.fought && begun(g, c.nat));
}

/** Notes that a corps has fought a battle this month (release 2, its switch on); off, nothing is written. */
export function foughtNow(g: Game, c: Corps): void {
  if (JOIN.on) c.foughtIn = g.turn;
}

/**
 * True where a corps standing in a province next door could come up in a battle there, side apart: in the field,
 * not held back by the player nor under a staff's hold, fed, with no battle yet this month and none joined, on ground
 * its side holds, and able to march in this month by the move rules.
 */
export function mayJoin(g: Game, c: Corps, target: Region): boolean {
  if (c.dead || c.kind === 'fort' || c.town !== undefined || c.leave !== undefined || c.fortress || c.holdBack) return false;
  // (A hold binds a computer staff, never the player's own corps: the Berlin garrison, the Ploiesti guard, the Westwall.)
  if (armyOf(c) !== g.human && c.hold) return false;
  if (!c.supplied || battleThisMonth(g, c)) return false;
  const here = g.regions[c.reg];
  if (!here.nb.includes(target.id) || !allied(g, c.nat, here.ctrl) || !allied(g, c.nat, fieldCtrl(g, here))) return false;
  // The move rules, as a march into the battle province would read them (moveCorps): the ground and the bridges, the
  // line a computer staff keeps to, and the weeks it has in hand.
  if (blocked(g, c, target) || (armyOf(c) !== g.human && lineShut(g, armyOf(c), target))) return false;
  const cost = moveCost(g, c, target, c.reg), hand = weeksInHand(g, c);
  return hand > 0 && hand >= Math.min(cost, 4);
}

/**
 * The corps next door that join this battle, each side's earliest first, at most STACK a side; null where nobody
 * does, or the switch is off. Not for a storm of a town or a garrison's sortie: those are fought by the army round
 * the town and the garrison inside it, nor for the corps 'spare' names. Reads the game and leaves it as it was.
 */
export function joinersOf(g: Game, attackers: Corps[], target: Region, defenders: Corps[], opts: { town?: boolean; sally?: boolean; spare?: Corps[] } = {}): Join | null {
  if (!JOIN.on || opts.town || opts.sally || !attackers.length || !defenders.length) return null;
  const attNat = armyOf(attackers[0]), defNat = defenders[0].nat;
  const aN = [...new Set(attackers.map((c) => c.nat))], dN = [...new Set(defenders.map((c) => c.nat))];
  // (Those in the battle itself, and those kept out of it: a staff's main body held for its second wave.)
  const inBattle = new Set<number>();
  for (const c of attackers) inBattle.add(c.id);
  for (const c of defenders) inBattle.add(c.id);
  for (const c of opts.spare ?? []) inBattle.add(c.id);
  const att: Joiner[] = [], def: Joiner[] = [];
  for (const j of target.nb) {
    const r = g.regions[j];
    for (const c of r.units) {
      if (inBattle.has(c.id)) continue;
      // Its side: the attack's or the defence's, and at war with every nation on the other side.
      const forA = allied(g, c.nat, attNat) && dN.every((d) => atWar(g, c.nat, d));
      const forD = !forA && allied(g, c.nat, defNat) && aN.every((a) => atWar(g, c.nat, a));
      if ((!forA && !forD) || !mayJoin(g, c, target)) continue;
      // (Up in the last week, it is no part of the battle.)
      const weeks = marchWeeks(g, c, target), share = joinShare(weeks);
      if (share <= 0) continue;
      (forA ? att : def).push({ corps: c, id: c.id, nat: c.nat, name: c.name, kind: c.kind, from: r.id, fromName: r.name, weeks, word: joinWord(share), share });
    }
  }
  // Each side's earliest, by the week and then the corps' number, down to STACK of them; a computer staff's corps that
  // its front rule (screen) keeps where it is stays: those that have joined this month, and those taken here, are gone.
  const taken = new Set<number>();
  const gone = (u: Corps): boolean => u.joinedIn === g.turn || taken.has(u.id);
  const first = (xs: Joiner[]): Joiner[] => {
    const out: Joiner[] = [];
    for (const j of xs.sort((x, y) => x.weeks - y.weeks || x.id - y.id)) {
      if (out.length >= STACK) break;
      if (armyOf(j.corps) !== g.human && screens(g, j.corps, j.corps.nat, NO_MASK, gone)) continue;
      out.push(j);
      taken.add(j.id);
    }
    return out;
  };
  const join = { att: first(att), def: first(def) };
  return join.att.length || join.def.length ? join : null;
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
/** The card's line where nobody may come up from next door: a storm of a town, a garrison's sortie (the design director, 08:16Z). */
export const JOIN_NONE = 'Corps next door';
export const STORM_ALONE = 'Only the troops on this ground fight a storm';
export const SORTIE_ALONE = 'Only the garrison and the besiegers fight a sortie';

/** The card's one line for a storm or a sortie, which nobody next door joins on either side; null for any other battle, or with the switch off. */
export function noJoinLine(opts: { town?: boolean; sally?: boolean }): TicketLine | null {
  if (!JOIN.on || (!opts.town && !opts.sally)) return null;
  return { label: JOIN_NONE, value: opts.town ? STORM_ALONE : SORTIE_ALONE };
}

/** A joiner's month is spent: no weeks left, counted as having fought, and joined this month. It stays where it stood. */
export function spendJoiner(g: Game, c: Corps): void {
  c.fought = true;
  c.ap = 0;
  c.joinedIn = g.turn;
}
