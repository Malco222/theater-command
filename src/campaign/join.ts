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
//    town's garrison or fortress troops. At most STACK a side, the earliest to come up first. Who stays (the design
//    director's ruling 3, 11:09Z, and Q3 folded, 18:10Z and 18:12Z: "A player never lacks a tool the computer has"):
//    a corps under a hold stays, whoever's it is. The hold is one mark every corps carries, the one the computer
//    staffs read, as the Berlin garrison and the Ploiesti guard do; the player sets and lifts it on each corps of his
//    army with one toggle, worded "Garrison" (orderHold, turn.ts), and holds from the setup start set; the toggle
//    exists only with the switch on. And a staff's corps stays where its own front rule keeps it. That rule is screen
//    (ai.ts advance, game.ts screens): two corps stay to screen any place that faces the enemy, read with the staff's
//    own masks (a theatre plan's 'mask': the garrisons it leaves to others; staffMask, none in free play). A corps
//    that has joined a battle this month, or is taken for this one, is gone from its province's count, so of a
//    province facing the enemy all but two may come up in a month. The player's corps are under no front rule.
//    Otherwise a staff, an ally's included, sends every corps it can, save the main body it keeps for a second wave
//    (ai.ts, twoWaves: AttackOpts 'spare').
// 2. When: by the game's own move rules, one map for the joiner, the staffs and the player (the design director's
//    ruling 1, 11:09Z). The week of its month it comes up in is weekOf of its move into the battle province (moveCost),
//    out of the weeks the corps has in hand this month, of its month's weeks (marchWeeks): never a fixed minute or a
//    draw of the dice. A corps whose month is still to come has the weeks that month will give it, reckoned as its
//    nation's month will reckon them (monthWeeks), not what last month's fuel or offensive gave it. No corps comes up
//    by train: the railway takes the whole month. It counts for the part of the battle it is there for: up in the
//    first week, three quarters of it (early); in the second, half (midway); in the third, a quarter (late). A corps
//    that comes up in the last week is no part of the battle.
// 3. Both sides, every nation: assess() reckons the joiners, so the attack card, the battle and every staff's odds are
//    one reckoning. (A staff's second wave is reckoned without those its first will have spent.)
// 4. They are in the odds and their losses are booked to them (game.ts), they stay where they stood, and their month
//    is spent: no weeks left, and counted as having fought; one in the player's army as well (joint.ts, jointMonth).
//    A corps on foot that comes up into open desert against tanks and lorries is driven round as the defenders there are.
// 5. The card names them, early, midway or late; to the player an enemy next door his staff has not identified is
//    "armies next door may join", with no names and no numbers (turn.ts, outlookOf). A storm of a town and a
//    garrison's sortie take nobody from next door, on either side, and their cards say so in one line (noJoinLine).
// 6. Once a month, in the order the battles are fought: the sequence rule (the design director's rulings of 17:43Z,
//    Q1(b) and Q2, on ruling 4). A corps joins one battle a month, and until it comes up it is free for every card it
//    can reach. "A reserve counts on the first picked card it can reach, and the cards after that read without it. The
//    mover's battles are then fought in that same order." The staff's own pick order makes that true by sequence: a
//    computer staff weighs every card it could fight this round, fights the best of them at once, and weighs all its
//    cards again on the changed map (ai.ts offensive and local attacks); its other jobs (storms, relief, break-outs,
//    counter-attacks, landings) read one card and fight it. The player books nothing either: he fights each battle
//    when he presses (orderAttack). So the battle a card is picked for is the next battle fought, and a reserve that
//    comes up there has its month spent (spendJoiner): every card after it reads without it. Until then nothing keeps
//    it for one place: no province's number, no keeping (keptFor and keptAshore are gone). So:
//    - Within one round of a staff's weighing a reserve stands on every card it could reach, as a staff thinks ("a
//      staff that weighs an enemy reserve on every card it could reach thinks as a staff does"); once the round's
//      battle is fought, the cards are weighed again without those that came up.
//    - Only a reserve that came up is spent. One cut by the five a side, or kept where it is by the front rule, did
//      not come up: it stays free, and the next card reads it again.
//    - The player's card shows every reserve not yet used, his own and the enemy's: his reserves come up at the first
//      of his battles they can reach, and the enemy's reserves against his attacks go the same way.
//    - A landing reads the reserves next to its shore as any battle does.
//    The joiner's week is still its own month's march by the move rules (2, above). One that gave ground in a battle
//    (giveWay) was in that battle and joins none for the rest of the calendar month (foughtNow). A battle's second
//    reckoning, after defenders have given ground, reads the joiners its first named and no others (AttackOpts
//    'joined').
// The historical scenarios reckon them alike: the rule is keyed on its switch alone.

import joinData from '../../data/campaign/join.json';
import { clamp } from '../core/math';
import type { TicketLine } from '../core/ticket';
import { NO_MASK, allied, atWar, monthWeeks, moveCost, screens, weekOf } from './game';
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
 * (weeksInHand) of its month's weeks (weeksOfMonth), as it does for every move. A whole week, 1 to 4. The same game
 * always gives the same week.
 */
export function marchWeeks(g: Game, c: Corps, target: Region): number {
  return weekOf(c, moveCost(g, c, target, c.reg), weeksInHand(g, c), weeksOfMonth(g, c));
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
  return begun(g, armyOf(c)) || g.flags[`back:${c.id}`] === g.turn ? c.ap : weeksOfMonth(g, c);
}

/**
 * The weeks this month gives a corps: those its nation's month gave it where that month has begun (startPhase), and
 * where it is still to come, the weeks startPhase will give it, reckoned by the same terms as the game stands
 * (monthWeeks): never what last month's fuel or offensive gave it.
 */
export function weeksOfMonth(g: Game, c: Corps): number {
  return begun(g, c.nat) || begun(g, armyOf(c)) || g.flags[`back:${c.id}`] === g.turn ? c.apMax : monthWeeks(g, c);
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
 * True where a corps standing in a province next door is fit to come up in a battle there, its month apart: in the
 * field, under no hold, fed, on ground its side holds, and not barred from the battle province by the move rules (the
 * ground and the bridges, and the line a computer staff keeps to).
 */
function standsReady(g: Game, c: Corps, target: Region): boolean {
  // (The hold is one mark, read alike for every corps, the player's and the staffs': the Berlin garrison, the Ploiesti
  // guard, the Westwall, and every corps the player has made a garrison with his toggle (orderHold, turn.ts).)
  if (c.dead || c.kind === 'fort' || c.town !== undefined || c.leave !== undefined || c.fortress || c.hold) return false;
  if (!c.supplied) return false;
  const here = g.regions[c.reg];
  if (!here.nb.includes(target.id) || !allied(g, c.nat, here.ctrl) || !allied(g, c.nat, fieldCtrl(g, here))) return false;
  // The move rules, as a march into the battle province would read them (moveCorps): the ground and the bridges, the
  // line a computer staff keeps to.
  return !blocked(g, c, target) && !(armyOf(c) !== g.human && lineShut(g, armyOf(c), target));
}

/**
 * True where a computer staff reads a corps as one that stays where it is (staff.ts fronts: it adds no threat, and beyond
 * the first line only part of its defence counts): a corps under a hold, as release 1 reads it. Save, with the switch on,
 * a hold the player has set or lifted with his own toggle: his garrison keeps his corps out of the battles next door and
 * binds nothing else, for he still sends it where he will, so the staffs read it as any corps of his (the review of Q3,
 * findings 3 and 11). His holds from the setup that he has never pressed read as release 1 reads them, and a game with
 * nobody playing reads every hold as ever.
 */
export const staffHeld = (g: Game, c: Corps): boolean =>
  c.hold && !(JOIN.on && g.human !== null && armyOf(c) === g.human && g.flags[`garrison:${c.id}`] !== undefined);

/**
 * True where a corps standing in a province next door could come up in a battle there, side apart: fit to (standsReady),
 * with no battle yet this month and none joined (one that gave ground in a battle was in it: giveWay counts it,
 * foughtNow), and able to march in this month by the move rules, out of the weeks it has in hand.
 */
export function mayJoin(g: Game, c: Corps, target: Region): boolean {
  if (!standsReady(g, c, target) || battleThisMonth(g, c)) return false;
  const cost = moveCost(g, c, target, c.reg), hand = weeksInHand(g, c);
  return hand > 0 && hand >= Math.min(cost, 4);
}

/**
 * A corps' side in a battle of the army of attNat (its nations aN) against defNat's (dN): the attack's where it is
 * allied with the attacker and at war with every nation on the defence, the defence's the other way round; else null.
 */
function sideOf(g: Game, c: Corps, attNat: string, aN: string[], defNat: string, dN: string[]): 'att' | 'def' | null {
  if (allied(g, c.nat, attNat) && dN.every((d) => atWar(g, c.nat, d))) return 'att';
  if (allied(g, c.nat, defNat) && aN.every((a) => atWar(g, c.nat, a))) return 'def';
  return null;
}

/**
 * The places a corps' own staff masks this month: a theatre plan's 'mask', the enemy garrisons it leaves to others
 * (Bordeaux in the American plan for France from month 60). The staff's front rule reads no enemy there, and so does
 * the join's. ai.ts gives the reading (maskOf); none in free play, under a main plan, or for the player.
 */
export const staffMask: { of: (g: Game, c: Corps) => ReadonlySet<number> } = { of: () => NO_MASK };

/** What a battle tells joinersOf: a storm of a town, a sortie, the staff's second wave kept out, a second reckoning. (A landing reads its reserves as any battle does.) */
export interface JoinOpts { town?: boolean; sally?: boolean; spare?: Corps[]; joined?: number[] }

/** One corps next door that passes a battle's tests: its side, week and share; 'spent' where it passes them all but its month, which an earlier battle this month took. */
interface Near { c: Corps; r: Region; side: 'att' | 'def'; weeks: number; share: number; spent?: true }

/**
 * Every corps next door that passes this battle's tests (its side, mayJoin, a share above nothing): under the sequence
 * rule each is free for the card until it comes up (head comment, 6). A second reckoning, 'joined', reads only those its
 * first reckoning named. With 'spent', for the war's records, also those that would pass them but have come up in a
 * battle already this month, their march read on their month's whole weeks. Null where nobody may join this battle.
 */
function nearBy(g: Game, attackers: Corps[], target: Region, defenders: Corps[], opts: JoinOpts, spent = false): Near[] | null {
  if (!JOIN.on || opts.town || opts.sally || !attackers.length || !defenders.length) return null;
  const attNat = armyOf(attackers[0]), defNat = defenders[0].nat;
  const aN = [...new Set(attackers.map((c) => c.nat))], dN = [...new Set(defenders.map((c) => c.nat))];
  // (Those in the battle itself, and those kept out of it: a staff's main body held for its second wave.)
  const inBattle = new Set<number>();
  for (const c of attackers) inBattle.add(c.id);
  for (const c of defenders) inBattle.add(c.id);
  for (const c of opts.spare ?? []) inBattle.add(c.id);
  const out: Near[] = [];
  for (const j of target.nb) {
    const r = g.regions[j];
    for (const c of r.units) {
      if (inBattle.has(c.id) || (opts.joined && !opts.joined.includes(c.id))) continue;
      // Its side: the attack's or the defence's, and at war with every nation on the other side.
      const side = sideOf(g, c, attNat, aN, defNat, dN);
      if (!side) continue;
      if (mayJoin(g, c, target)) {
        // (Up in the last week, it is no part of the battle.)
        const weeks = marchWeeks(g, c, target), share = joinShare(weeks);
        if (share > 0) out.push({ c, r, side, weeks, share });
      } else if (spent && c.joinedIn === g.turn && standsReady(g, c, target)) {
        // (For the records: it would come up here but for the battle it came up in earlier this month.)
        const m = weeksOfMonth(g, c), cost = moveCost(g, c, target, c.reg), weeks = weekOf(c, cost, m, m), share = joinShare(weeks);
        if (m > 0 && m >= Math.min(cost, 4) && share > 0) out.push({ c, r, side, weeks, share, spent: true });
      }
    }
  }
  return out;
}

/**
 * Why a corps next door that passed every test of a battle did not come up in it: kept where it is by the front rule,
 * cut by the five a side, or come up already this month. Where both the front rule and the five would leave it out,
 * the front rule is named: it would have stayed had there been room.
 */
export type LeftOut = 'stack' | 'front' | 'joined';

/**
 * Of the corps next door that pass a battle's tests, those that come up, each side's earliest first: by the week and
 * then the corps' number, down to STACK of them; a computer staff's corps that its front rule (screen, with the staff's
 * own masks) keeps where it is stays: those that have joined this month, and those taken here, are gone from its
 * province's count. 'left' names the rest and why. Neither is spent: only a joiner, once its battle is fought. With
 * 'why' (the war's records, passedOver), one cut by the five is also read against its front rule, and named by it where
 * the rule keeps it; who comes up is the same either way.
 */
function choose(g: Game, near: Near[], why = false): { join: Join; left: { j: Joiner; why: LeftOut }[] } {
  const att: Joiner[] = [], def: Joiner[] = [];
  for (const { c, r, side, weeks, share } of near) {
    (side === 'att' ? att : def).push({ corps: c, id: c.id, nat: c.nat, name: c.name, kind: c.kind, from: r.id, fromName: r.name, weeks, word: joinWord(share), share });
  }
  const taken = new Set<number>(), left: { j: Joiner; why: LeftOut }[] = [];
  const gone = (u: Corps): boolean => u.joinedIn === g.turn || taken.has(u.id);
  const front = (j: Joiner): boolean => armyOf(j.corps) !== g.human && screens(g, j.corps, j.corps.nat, staffMask.of(g, j.corps), gone);
  const first = (xs: Joiner[]): Joiner[] => {
    const out: Joiner[] = [];
    for (const j of xs.sort((x, y) => x.weeks - y.weeks || x.id - y.id)) {
      if (out.length >= STACK) { left.push({ j, why: why && front(j) ? 'front' : 'stack' }); continue; }
      if (front(j)) { left.push({ j, why: 'front' }); continue; }
      out.push(j);
      taken.add(j.id);
    }
    return out;
  };
  return { join: { att: first(att), def: first(def) }, left };
}

/**
 * The corps next door that join this battle, each side's earliest first, at most STACK a side; null where nobody
 * does, or the switch is off. Not for a storm of a town or a garrison's sortie: those are fought by the army round
 * the town and the garrison inside it, nor for the corps 'spare' names. Every corps next door that has not come up
 * this month is read (the sequence rule: nearBy, choose). Reads the game and leaves it as it was.
 */
export function joinersOf(g: Game, attackers: Corps[], target: Region, defenders: Corps[], opts: JoinOpts = {}): Join | null {
  const near = nearBy(g, attackers, target, defenders, opts);
  if (!near) return null;
  const { join } = choose(g, near);
  return join.att.length || join.def.length ? join : null;
}

/**
 * For the war's records: the corps next door that pass every test of this battle but do not come up in it, and why
 * (LeftOut): kept by the front rule, or else cut by the five a side (both stay free for the next card), or come up
 * already in an earlier battle this month. Empty with the switch off. Reads the game and leaves it as it was.
 */
export function passedOver(g: Game, attackers: Corps[], target: Region, defenders: Corps[], opts: JoinOpts = {}): { corps: Corps; side: 'att' | 'def'; why: LeftOut }[] {
  const near = nearBy(g, attackers, target, defenders, opts, true);
  if (!near) return [];
  const out = choose(g, near.filter((x) => !x.spent), true).left.map(({ j, why }) => ({ corps: j.corps, side: near.find((x) => x.c === j.corps)!.side, why }));
  for (const x of near) if (x.spent) out.push({ corps: x.c, side: x.side, why: 'joined' });
  return out;
}

/** The second reckoning of a battle reads only the joiners its first named (AttackOpts 'joined'); off, the options as they were. */
export function keepJoiners<T extends JoinOpts>(opts: T, join: Join | null | undefined): T {
  return JOIN.on ? { ...opts, joined: join ? [...join.att, ...join.def].map((j) => j.id) : [] } : opts;
}

/** A watcher of the battles as they are fought (release 2), for the war's records: told once a battle, at its first reckoning. It moves nothing. */
export const joinWatch: { tell: ((e: { g: Game; attackers: Corps[]; target: Region; defenders: Corps[]; opts: JoinOpts; join: Join | null }) => void) | null } = { tell: null };

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
