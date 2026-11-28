// Bridging columns (release 2, batch 3).
//
// Brian, 27 November 2026 17:10Z, "Yes to all" to the idea; the design director's rulings of 17:15Z and 17:48Z on
// r2-three-ideas/FORM-rails-rivers-1127.md, part B. Behind the grand campaign's flag 'bridging' (the form named it
// 'columns', which is supply columns' flag already: columns.ts), OFF until its gate passes.
//
// 1. A nation's bridging columns are its engineer corps, one column each: the arsenal's words for that corps are
//    "Bridging columns, sappers and labour battalions". An engineer corps counts while it is alive, on land, in the
//    field (not shut in a town, not home on leave, not at the schools).
// 2. Where a corps of the nation on wheels or tracks crosses a river whose bridges are down, by a march or an attack,
//    and an engineer corps of the nation whose column is free can reach the near bank this month by its own march
//    (turn.ts reach, called: the move rule's own reach), that column is laid there. The order to cross lays it. A
//    column cannot be in two places. The engineer corps stays where it is.
// 3. A laid column makes the crossing count as standing for its nation only: wheels cross, the march costs what it
//    costs over a bridge, and an attack across is priced at river.attack, so the boats' figure, with or without
//    engineers, is not also read. It carries no trains: the railways read downBetween, which is unchanged. The other
//    side gains nothing from it.
// 4. It comes back at the opening of its nation's month by fact, never by a timer: its bridge stands again; its nation
//    no longer holds the bank it was laid from; or no corps of its nation crossed by it in the month gone.
// The player's corps and every staff's read the same functions (rivers.ts noCrossing and blocked, game.ts moveCost
// and assess), the same for every army. With the switch off nothing here is written, and every answer is the
// answer without it.

import { reach } from './turn';
import { bridgeDown, crossingOf, riverName, type Crossing } from './rivers';
import { allied, headline } from './game';
import type { Corps, Game } from './types';

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const bridgingOn = (g: Game): boolean => g.flags.open === 1 && g.flags.bridging === 1;

const PREFIXES = ['bc:', 'bce:', 'bcu:'];
/** Turns the switch on or off at any time. Off takes every column up. */
export function setBridging(g: Game, on: boolean): boolean {
  if (on) {
    if (g.flags.open !== 1) return false;
    g.flags.bridging = 1;
    return true;
  }
  delete g.flags.bridging;
  for (const k of Object.keys(g.flags)) if (PREFIXES.some((p) => k.startsWith(p))) delete g.flags[k];
  return true;
}

// A laid column, under three flags of the crossing and the nation: the bank it was laid from, the engineer corps that
// carries it, and the last month a corps of the nation crossed by it.
const at = (nat: string, x: Crossing): string => `${nat}:${x.a}:${x.b}`;
const fromKey = (nat: string, x: Crossing): string => `bc:${at(nat, x)}`;
const byKey = (nat: string, x: Crossing): string => `bce:${at(nat, x)}`;
const usedKey = (nat: string, x: Crossing): string => `bcu:${at(nat, x)}`;

/** Whether a column of this nation stands over the crossing. */
export const columnAt = (g: Game, nat: string, x: Crossing): boolean => bridgingOn(g) && g.flags[fromKey(nat, x)] !== undefined;

/** The engineer corps whose columns are laid somewhere, by id. */
function carrying(g: Game, nat: string): Set<number> {
  const out = new Set<number>();
  const head = `bce:${nat}:`;
  for (const k in g.flags) if (k.startsWith(head)) out.add(g.flags[k]);
  return out;
}

/** An engineer corps that can carry a column: alive, of the nation, on land in the field (not shut in a town, not home on leave, not at the schools, not at sea). */
const inField = (c: Corps, nat: string): boolean => !c.dead && c.kind === 'eng' && c.nat === nat && c.town === undefined && c.leave === undefined && !c.school && c.sail === undefined;

/**
 * The engineer corps of the nation whose column is free and that can reach the near bank this month by its own march,
 * the nearest first (one standing on the bank, then the cheapest march, then the lower id). Null where none can.
 */
export function freeColumn(g: Game, nat: string, from: number): Corps | null {
  if (!bridgingOn(g)) return null;
  const busy = carrying(g, nat);
  let best: Corps | null = null, cost = Infinity;
  for (const e of g.corps) {
    if (!inField(e, nat) || busy.has(e.id)) continue;
    let k: number;
    if (e.reg === from) k = 0;
    else {
      const m = reach(g, e).get(from);
      if (!m || m.attack) continue;
      k = m.cost;
    }
    if (k < cost || (k === cost && best && e.id < best.id)) { best = e; cost = k; }
  }
  return best;
}

/**
 * Whether the way between two banks counts as standing for a corps of this nation on wheels or tracks (`wheels`) or on
 * foot: a column of the nation stands there, or (on wheels) a free column can be laid there by the order to cross.
 * False with the switch off, and wherever the bridges stand (there is nothing to lay).
 */
export function columnFor(g: Game, nat: string, from: number, to: number, wheels: boolean): boolean {
  if (!bridgingOn(g)) return false;
  const x = crossingOf(from, to);
  if (!x || !bridgeDown(g, x)) return false;
  if (columnAt(g, nat, x)) return true;
  return wheels && freeColumn(g, nat, from) !== null;
}

/**
 * A corps of the nation crosses from one bank to the other (a march made, or an attack going in): over the nation's
 * column where one stands, which is marked used for the month; or, on wheels or tracks, over a column laid there now
 * by the nearest free engineer corps. Returns true where it went over a column. Nothing where the bridges stand, on
 * foot with no column (the infantry goes over in boats), or with the switch off.
 */
export function crossOver(g: Game, c: Corps, from: number, to: number, wheels: boolean): boolean {
  if (!bridgingOn(g)) return false;
  const x = crossingOf(from, to);
  if (!x || !bridgeDown(g, x)) return false;
  const nat = c.nat;
  if (!columnAt(g, nat, x)) {
    if (!wheels) return false;
    const e = freeColumn(g, nat, from);
    if (!e) return false;
    g.flags[fromKey(nat, x)] = from;
    g.flags[byKey(nat, x)] = e.id;
    if (nat === g.human) headline(g, 'info', `${e.name} lays a bridging column over the ${riverName(x)} between ${g.regions[from].name} and ${g.regions[to].name}. Wheels and tracks cross by it while your troops use it and hold the bank.`, nat, from, { arm: 'land' });
  }
  g.flags[usedKey(nat, x)] = g.turn;
  return true;
}

/** The columns of a nation laid now: the crossing, the bank laid from, the engineer corps that carries it. */
export function columnsOf(g: Game, nat: string): { x: Crossing; from: number; by: number; used: number }[] {
  const out: { x: Crossing; from: number; by: number; used: number }[] = [];
  if (!bridgingOn(g)) return out;
  const head = `bc:${nat}:`;
  for (const k in g.flags) {
    if (!k.startsWith(head)) continue;
    const [, , a, b] = k.split(':');
    const x = crossingOf(+a, +b);
    if (!x) continue;
    out.push({ x, from: g.flags[k], by: g.flags[byKey(nat, x)], used: g.flags[usedKey(nat, x)] ?? -1 });
  }
  return out;
}

/**
 * The reckoning that opens a nation's month (game.ts startPhase): each of its columns comes back where its bridge
 * stands again, where the nation no longer holds the bank it was laid from, or where no corps of the nation crossed by
 * it in the month gone. Returns the crossings it came back from, with the fact.
 */
export function bridgingMonth(g: Game, nat: string): { x: Crossing; why: 'bridge' | 'bank' | 'unused' }[] {
  const back: { x: Crossing; why: 'bridge' | 'bank' | 'unused' }[] = [];
  for (const col of columnsOf(g, nat)) {
    const why = !bridgeDown(g, col.x) ? 'bridge' : !allied(g, nat, g.regions[col.from].ctrl) ? 'bank' : col.used < g.turn - 1 ? 'unused' : null;
    if (!why) continue;
    delete g.flags[fromKey(nat, col.x)];
    delete g.flags[byKey(nat, col.x)];
    delete g.flags[usedKey(nat, col.x)];
    back.push({ x: col.x, why });
  }
  return back;
}

/** An engineer corps' card line under the switch: where its column is laid, or that it is free. '' for any other corps, or with the switch off. */
export function columnLine(g: Game, c: Corps): string {
  if (c.kind !== 'eng' || !bridgingOn(g)) return '';
  const col = columnsOf(g, c.nat).find((k) => k.by === c.id);
  if (!col) return 'Its bridging column is free: where the bridges are down and this corps can reach the bank this month, the first of your corps on wheels to cross there lays it.';
  const other = col.x.a === col.from ? col.x.b : col.x.a;
  return `Its bridging column is laid over the ${riverName(col.x)} between ${g.regions[col.from].name} and ${g.regions[other].name}. Wheels and tracks cross by it while your troops use it and hold the bank.`;
}
