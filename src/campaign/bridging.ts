// Bridging columns (release 2, batch 3).
//
// Brian, 27 November 2026 17:10Z, "Yes to all" to the idea; the design director's rulings of 17:15Z and 17:48Z on
// r2-three-ideas/FORM-rails-rivers-1127.md, part B, and of 28 November 01:12Z. Behind the grand campaign's flag
// 'bridging' (the form named it 'columns', which is supply columns' flag already: columns.ts), OFF until its gate.
//
// 1. A nation's bridging columns are its engineer corps, one column each: the arsenal's words for that corps are
//    "Bridging columns, sappers and labour battalions". An engineer corps counts while it is alive, on land, in the
//    field (not shut in a town, not home on leave, not at the schools, not at sea).
// 2. A column serves its side: one nation, or the nations of its bloc at war (the ally feed ruling's word (e): an ally
//    is a nation of the bloc at war). Where a corps of the side on wheels or tracks crosses a river whose bridges are
//    down, by a march or an attack, and an engineer corps of the side whose column is free can reach the near bank
//    this month by its own march (turn.ts reach, called: the move rule's own reach), that corps' column is laid there.
//    The order to cross lays it. A column cannot be in two places. The engineer corps stays where it is.
// 3. A laid column makes the crossing count as standing for its side only: wheels cross, the march costs what it costs
//    over a bridge, and an attack across is priced at river.attack, so the boats' figure, with or without engineers,
//    is not also read. It carries no trains: the railways read downBetween, which is unchanged. The other side gains
//    nothing from it.
// 4. A column is promised once: the attack's card and every staff ask overColumns, which lays the attack's columns
//    on the board as it stands by the very function the attack lays them with, and takes them up again. A column an
//    order has laid is not free.
// 5. It comes back to its own engineer corps by fact, never by a timer, at the opening of that corps' nation's month:
//    its bridge stands again; its side no longer holds the bank it was laid from; or no corps of its side crossed by it
//    in the month gone. And at once, the moment its engineer corps is gone: a column with no corps stands nowhere.
// The player's corps and every staff's read the same functions (rivers.ts noCrossing and blocked, game.ts moveCost
// and assess), the same for every army. With the switch off nothing here is written, and every answer is the
// answer without it.

import { reach } from './turn';
import { bridgeDown, crossingOf, onWheels, riverName, type Crossing } from './rivers';
import { allied, headline } from './game';
import type { Corps, Game, Region } from './types';

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

/**
 * Of one side: one nation, or two nations of one bloc that are both at war (the ally feed ruling's word (e), its
 * allyAtWar read both ways: an at-peace member of the bloc lays nothing for those at war, and crosses by nothing).
 */
export const sameSideAtWar = (g: Game, a: string, b: string): boolean =>
  a === b || (allied(g, a, b) && (g.nations[a]?.atWar.length ?? 0) > 0 && (g.nations[b]?.atWar.length ?? 0) > 0);

// A laid column, under three flags of the crossing and its engineer corps' nation: the bank it was laid from, the
// engineer corps that carries it, and the last month a corps of its side crossed by it.
const at = (nat: string, x: Crossing): string => `${nat}:${x.a}:${x.b}`;
const fromKey = (nat: string, x: Crossing): string => `bc:${at(nat, x)}`;
const byKey = (nat: string, x: Crossing): string => `bce:${at(nat, x)}`;
const usedKey = (nat: string, x: Crossing): string => `bcu:${at(nat, x)}`;

/** The engineer corps by its id, while it exists: alive and on the books. */
const corpsOf = (g: Game, id: number | undefined): Corps | undefined => (id === undefined ? undefined : g.corps.find((c) => c.id === id && !c.dead));
/** A column whose engineer corps is gone stands nowhere. */
const stands = (g: Game, nat: string, x: Crossing): boolean => g.flags[fromKey(nat, x)] !== undefined && corpsOf(g, g.flags[byKey(nat, x)]) !== undefined;

/** The nation whose engineers' column of this side stands over the crossing, or null (always null with the switch off). */
export function columnOf(g: Game, nat: string, x: Crossing): string | null {
  if (!bridgingOn(g)) return null;
  if (stands(g, nat, x)) return nat;
  for (const n of Object.keys(g.nations)) if (n !== nat && stands(g, n, x) && sameSideAtWar(g, nat, n)) return n;
  return null;
}
/** Whether a column of this nation's side stands over the crossing. */
export const columnAt = (g: Game, nat: string, x: Crossing): boolean => columnOf(g, nat, x) !== null;

/** The engineer corps of a nation whose columns are laid somewhere, by id. */
function carrying(g: Game, nat: string): Set<number> {
  const out = new Set<number>();
  const head = `bce:${nat}:`;
  for (const k in g.flags) if (k.startsWith(head)) out.add(g.flags[k]);
  return out;
}

/** An engineer corps that can carry a column: alive, on land in the field (not shut in a town, not home on leave, not at the schools, not at sea). */
const inField = (c: Corps): boolean => !c.dead && c.kind === 'eng' && c.town === undefined && c.leave === undefined && !c.school && c.sail === undefined;

/**
 * The engineer corps of the side whose column is free and that can reach the near bank this month by its own march,
 * the nearest first (one standing on the bank, then the cheapest march, then the lower id). Null where none can.
 */
export function freeColumn(g: Game, nat: string, from: number): Corps | null {
  if (!bridgingOn(g)) return null;
  const busy = new Map<string, Set<number>>();
  let best: Corps | null = null, cost = Infinity;
  for (const e of g.corps) {
    if (!inField(e) || !sameSideAtWar(g, nat, e.nat)) continue;
    let mine = busy.get(e.nat);
    if (!mine) busy.set(e.nat, (mine = carrying(g, e.nat)));
    if (mine.has(e.id)) continue;
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
 * foot: a column of its side stands there, or (on wheels) a free column of its side can be laid there by the order to
 * cross. False with the switch off, and wherever the bridges stand (there is nothing to lay).
 */
export function columnFor(g: Game, nat: string, from: number, to: number, wheels: boolean): boolean {
  if (!bridgingOn(g)) return false;
  const x = crossingOf(from, to);
  if (!x || !bridgeDown(g, x)) return false;
  if (columnAt(g, nat, x)) return true;
  return wheels && freeColumn(g, nat, from) !== null;
}

/** Where a crossing writes: the flags themselves, or (overColumns) the flags kept to be put back. */
type Write = (k: string, v: number) => void;
const onBoard = (g: Game): Write => (k, v) => { g.flags[k] = v; };

// The one laying function: a corps crosses from one bank to the other over its side's column where one stands (marked
// used for the month), or, on wheels or tracks, over the column of the nearest free engineer corps of its side, laid
// there now. True where it went over a column. `tell`: the player hears of a column laid.
function goOver(g: Game, c: Corps, from: number, to: number, wheels: boolean, write: Write, tell: boolean): { over: boolean; laid: Corps | null } {
  const x = crossingOf(from, to);
  if (!x || !bridgeDown(g, x)) return { over: false, laid: null };
  let nat = columnOf(g, c.nat, x), laid: Corps | null = null;
  if (nat === null) {
    if (!wheels) return { over: false, laid: null };
    const e = freeColumn(g, c.nat, from);
    if (!e) return { over: false, laid: null };
    nat = e.nat; laid = e;
    write(fromKey(nat, x), from);
    write(byKey(nat, x), e.id);
    if (tell && g.human !== null && (e.nat === g.human || c.nat === g.human)) headline(g, 'info', `${e.name} lays a bridging column over the ${riverName(x)} between ${g.regions[from].name} and ${g.regions[to].name}. Wheels and tracks cross by it while your side uses it and holds the bank.`, g.human, from, { arm: 'land' });
  }
  write(usedKey(nat, x), g.turn);
  return { over: true, laid };
}

/**
 * A corps of the side crosses from one bank to the other (a march made, or an attack going in): over its side's
 * column where one stands, which is marked used for the month; or, on wheels or tracks, over a column laid there now
 * by the nearest free engineer corps of its side. Returns true where it went over a column. Nothing where the bridges
 * stand, on foot with no column (the infantry goes over in boats), or with the switch off.
 */
export function crossOver(g: Game, c: Corps, from: number, to: number, wheels: boolean): boolean {
  if (!bridgingOn(g)) return false;
  return goOver(g, c, from, to, wheels, onBoard(g), true).over;
}

/** The order in which an attack's corps go over a river (game.ts resolveBattle): wheels and tracks first, then the foot. */
export const crossingOrder = (attackers: readonly Corps[]): Corps[] => [...attackers.filter((c) => onWheels(c.kind)), ...attackers.filter((c) => !onWheels(c.kind))];

/**
 * An attack's crossings over bridging columns, as going in would lay them on the board as it stands: the very laying
 * function the attack calls (crossOver, in crossingOrder), its flags put back after. Gives the corps that go over a
 * column, by id, and the engineer corps whose one column going in would lay. A column one corps lays is not free for
 * the next. Nothing is written, and nothing with the switch off.
 */
export function overColumns(g: Game, attackers: readonly Corps[], target: Region): { over: Set<number>; laid: Corps[] } {
  const over = new Set<number>(), laid: Corps[] = [];
  if (!bridgingOn(g)) return { over, laid };
  const kept = new Map<string, number | undefined>();
  const write: Write = (k, v) => { if (!kept.has(k)) kept.set(k, g.flags[k]); g.flags[k] = v; };
  try {
    for (const c of crossingOrder(attackers)) {
      const r = goOver(g, c, c.reg, target.id, onWheels(c.kind), write, false);
      if (r.over) over.add(c.id);
      if (r.laid) laid.push(r.laid);
    }
  } finally {
    for (const [k, v] of kept) if (v === undefined) delete g.flags[k]; else g.flags[k] = v;
  }
  return { over, laid };
}

/** The columns of a nation's engineer corps laid now: the crossing, the bank laid from, the engineer corps that carries it. */
export function columnsOf(g: Game, nat: string): { x: Crossing; from: number; by: number; used: number }[] {
  return laidOf(g, nat).filter((c) => !c.gone).map(({ x, from, by, used }) => ({ x, from, by, used }));
}
function laidOf(g: Game, nat: string): { x: Crossing; from: number; by: number; used: number; gone: boolean }[] {
  const out: { x: Crossing; from: number; by: number; used: number; gone: boolean }[] = [];
  if (!bridgingOn(g)) return out;
  const head = `bc:${nat}:`;
  for (const k in g.flags) {
    if (!k.startsWith(head)) continue;
    const [, , a, b] = k.split(':');
    const x = crossingOf(+a, +b);
    if (!x) continue;
    const by = g.flags[byKey(nat, x)];
    out.push({ x, from: g.flags[k], by, used: g.flags[usedKey(nat, x)] ?? -1, gone: corpsOf(g, by) === undefined });
  }
  return out;
}
const takeUp = (g: Game, nat: string, x: Crossing): void => { delete g.flags[fromKey(nat, x)]; delete g.flags[byKey(nat, x)]; delete g.flags[usedKey(nat, x)]; };

/**
 * The reckoning that opens a nation's month (game.ts startPhase): each column of its engineer corps comes back where
 * its engineer corps is gone, its bridge stands again, its side no longer holds the bank it was laid from, or no corps
 * of its side crossed by it in the month gone. Returns the crossings it came back from, with the fact. The flags of a
 * column of any nation whose engineer corps is gone are cleared too (it stood nowhere from the moment its corps went).
 */
export function bridgingMonth(g: Game, nat: string): { x: Crossing; why: 'gone' | 'bridge' | 'bank' | 'unused' }[] {
  const back: { x: Crossing; why: 'gone' | 'bridge' | 'bank' | 'unused' }[] = [];
  if (!bridgingOn(g)) return back;
  for (const col of laidOf(g, nat)) {
    const why = col.gone ? 'gone' : !bridgeDown(g, col.x) ? 'bridge' : !sameSideAtWar(g, nat, g.regions[col.from].ctrl) ? 'bank' : col.used < g.turn - 1 ? 'unused' : null;
    if (!why) continue;
    takeUp(g, nat, col.x);
    back.push({ x: col.x, why });
  }
  for (const n of Object.keys(g.nations)) if (n !== nat) for (const col of laidOf(g, n)) if (col.gone) takeUp(g, n, col.x);
  return back;
}

/** An engineer corps' card line under the switch: where its column is laid, or that it is free. '' for any other corps, or with the switch off. */
export function columnLine(g: Game, c: Corps): string {
  if (c.kind !== 'eng' || !bridgingOn(g)) return '';
  const col = columnsOf(g, c.nat).find((k) => k.by === c.id);
  if (!col) return 'Its bridging column is free: where the bridges are down and this corps can reach the bank this month, the first corps of your side on wheels to cross there lays it.';
  const other = col.x.a === col.from ? col.x.b : col.x.a;
  return `Its bridging column is laid over the ${riverName(col.x)} between ${g.regions[col.from].name} and ${g.regions[other].name}. Wheels and tracks of your side cross by it while they use it and hold the bank.`;
}
