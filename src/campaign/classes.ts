// The sandbox (Brian, 19 November 2026: "i want this to be a sandbox to play and make your own history, not just play
// out the real one"). Ships, aircraft and corps that history never built, opened by research on the tree like any other
// technology, and classes of the player's own design, all ordered from the arsenal on the same lines as history's own:
// money, months, a yard or a factory, and a steel bill drawn from the nation's supplies. The shared shape for the three
// arms is written down in the project's sandbox/SHAPE.md; the rows live in data/campaign/classes.json.
//
// Everything here is behind one switch, g.flags.sandbox, in the grand campaign only. Off, the game is exactly what it
// was: no what-if technology on the tree, no class on an order line, no steel bill, nothing for a computer staff to
// choose. A class is a variant of a kind the arsenal already orders ('of': 'cv', 'ftr', 'arm', 'para' ...) and counts as
// that kind everywhere a kind is counted; a unit of a class carries its id ('cls' on the corps, the air group, the
// order), and what the class changes is read through the helpers below.

import data from '../../data/campaign/classes.json';
import arsenalData from '../../data/campaign/arsenal.json';
import navyData from '../../data/campaign/navy.json';
import { computerDesigns, dialDraw, sandbox, whatifShare } from './history';
import { locked, type Tech } from './tech';
import { storesOf } from './resources';
import { headline } from './game';
import type { When } from './traits';
import type { Build, Game } from './types';

/** What a class of corps is worth in battle, under a 'when' read as for a national ability (traits.ts). */
export interface Edge { attack?: number; defence?: number; when?: When; why?: string }

/** One class: a row of classes.json, or one of a nation's own designs ('own'). Fields not stated are its kind's own. */
export interface ClassRow {
  id: string;
  /** The kind it is a variant of, as the arsenal's order lines name it: 'bb' 'cv' 'ca' 'dd' 'ss', 'ftr' 'bmr', 'arm' 'mot' ..., 'para'. */
  of: string;
  label: string;
  /** What goes into a corps' name: XV Flame Tank Corps. */
  word?: string;
  /** A class of the research tree: the technology that opens it lists 'class:<id>'. */
  whatif?: boolean;
  /** A class of the nation's own design (the drawing board). */
  own?: boolean;
  tech?: string;
  blurb?: string;
  /** Its battle model in battle-models, and the glider that carries an airborne class. */
  model?: string;
  glider?: string;
  cost?: number;
  months?: number;
  count?: number;
  divs?: number;
  yard?: number;
  factories?: number;
  /** Its own share of the steel bill, in place of its column's. */
  steel?: number;
  /** Land: how many the nation can keep at once, in the field and on order. */
  most?: number;
  /** Times its kind's fuel. */
  oil?: number;
  /** Land: what it is worth in battle. */
  edges?: Edge[];
  /** Aircraft: see classes.json's note. */
  fight?: number;
  reach?: number;
  night?: number;
  close?: number;
  escort?: number;
  bomb?: number;
  sea?: number;
  [k: string]: unknown;
}

export const CLASSES = data as unknown as { classes: ClassRow[] };
/** Every row of classes.json: ships, aircraft and corps. */
export const CLASS_ROWS: ClassRow[] = CLASSES.classes;

const BY_ID = new Map<string, ClassRow>();
for (const r of CLASSES.classes) BY_ID.set(r.id, r);

export { sandbox, whatifShare, computerDesigns };

/** A class by its id, as this nation knows it: the rows of classes.json, and its own designs. */
export function classById(g: Game, nat: string, id: string): ClassRow | undefined {
  return BY_ID.get(id) ?? g.nations[nat]?.designs?.find((d) => d.id === id);
}

/** Every class of this kind a nation is shown on the arsenal's line, researched or not, its own designs last. None with the sandbox off. */
export function classesFor(g: Game, nat: string, of: string): ClassRow[] {
  if (!sandbox(g)) return [];
  return [...CLASSES.classes.filter((r) => r.of === of), ...(g.nations[nat]?.designs ?? []).filter((d) => d.of === of)];
}

/** The technology this nation still lacks before it can order this class, or null. A design of its own waits on none. */
export const classLocked = (g: Game, nat: string, row: ClassRow): Tech | null => (row.own || !row.whatif ? null : locked(g, nat, `class:${row.id}`));

/** The classes of this kind a nation can order now, so far as research goes. */
export const usableClasses = (g: Game, nat: string, of: string): ClassRow[] => classesFor(g, nat, of).filter((r) => !classLocked(g, nat, r));

/** The class a corps, an air group or an order is, or null for its kind's own. */
export const classOf = (g: Game, x: { nat: string; cls?: string }): ClassRow | null => (x.cls ? classById(g, x.nat, x.cls) ?? null : null);

type NumField = 'oil' | 'fight' | 'reach' | 'night' | 'close' | 'escort' | 'bomb' | 'sea';
/** One of a class's numbers for this unit: `dflt` (1, no change) for a unit of its kind's own. */
export function classNum(g: Game, x: { nat: string; cls?: string }, field: NumField, dflt = 1): number {
  if (!x.cls) return dflt;
  const v = classOf(g, x)?.[field];
  return typeof v === 'number' ? v : dflt;
}

/** How many of this class a nation has in the field, at sea on passage and on order. */
export function classCount(g: Game, nat: string, id: string): number {
  let k = g.nations[nat].queue.filter((b) => b.cls === id).length;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.cls === id) k++;
  for (const v of g.atSea ?? []) if (v.nat === nat && v.corps.cls === id) k++;
  return k;
}

/** Why this nation cannot order this class now, before money, men and a place to build it; null when it can. */
export function whyNoClass(g: Game, nat: string, row: ClassRow): string | null {
  if (!sandbox(g)) return 'What-if classes belong to the sandbox.';
  const t = classLocked(g, nat, row);
  if (t) return `Needs the technology ${t.name}. Open Research to begin it.`;
  if (row.most !== undefined && classCount(g, nat, row.id) >= row.most) return `The nation can keep ${row.most} of these at once, in the field and on order.`;
  return null;
}

// ---- The steel bill ------------------------------------------------------------------------------------

export type Column = 'ships' | 'land' | 'air';
const STEEL = (arsenalData as unknown as { steel?: Record<Column, number> }).steel ?? { ships: 0, land: 0, air: 0 };

/** The share of an order's money cost that is also drawn from the supplies stock: the class's own, or its column's. */
export const steelShare = (column: Column, row?: ClassRow | null): number => row?.steel ?? STEEL[column] ?? 0;

/**
 * Fixes an order's steel bill when it is placed, with the sandbox on: its share of the money cost, spread evenly over
 * the months it takes. Nothing with the sandbox off, so an order then carries no bill.
 */
export function stampSteel(g: Game, b: Build, column: Column, row?: ClassRow | null): void {
  if (!sandbox(g) || b.cost <= 0) return;
  const share = steelShare(column, row);
  if (share <= 0) return;
  b.steel = Math.round((share * b.cost / Math.max(1, b.eta - b.ordered)) * 100) / 100;
}

/** True for an order that draws its steel this month: one placed in an earlier month that is not yet handed over. */
const drawing = (g: Game, b: Build): boolean => (b.steel ?? 0) > 0 && b.ordered < g.turn && b.eta >= g.turn;

/** What the orders on a nation's books draw in steel this month. */
export const steelOnBooks = (g: Game, nat: string): number => g.nations[nat].queue.reduce((s, b) => s + (drawing(g, b) ? b.steel! : 0), 0);

/**
 * The month's steel, before the month's orders are handed over (game.ts): every order on the books draws its bill from
 * the supplies stock. A month the stock cannot cover, nothing is drawn and every one of those orders slips a month;
 * they are returned, for the month card. A country that keeps no stock buys its steel abroad: the bill is paid in money.
 */
export function steelMonth(g: Game, nat: string): Build[] {
  if (!sandbox(g)) return [];
  const n = g.nations[nat];
  const on = n.queue.filter((b) => drawing(g, b));
  const bill = on.reduce((s, b) => s + b.steel!, 0);
  if (bill <= 1e-9) return [];
  const st = storesOf(g, nat);
  if (!st) { n.pp -= bill; return []; }
  if (st.sup + 1e-9 >= bill) { st.sup -= bill; return []; }
  for (const b of on) b.eta += 1;
  if (nat === g.human) {
    const names = on.map((b) => b.name);
    headline(g, 'info', `Short of steel: the depots hold ${Math.round(st.sup)} supplies and the orders on the books need ${Math.round(bill)} this month, so ${on.length === 1 ? 'the order' : `all ${on.length} orders`} slip a month: ${names.length <= 4 ? names.join('; ') : `${names.slice(0, 4).join('; ')} and ${names.length - 4} more`}. Open Supplies on the top bar to see where they go.`, nat);
  }
  return on;
}

// ---- The computer's choice --------------------------------------------------------------------------

/**
 * The class a computer staff (or the player's own building staff) takes for an order of this kind, or null for the
 * kind's own. By the history dial: never where its share is 0, and at most that share of its orders, by the war's
 * seed; its own designs too where the dial lets computer staffs draw them. `salt` names the order, so that two
 * orders in one month are drawn apart.
 */
export function pickClass(g: Game, nat: string, of: string, salt: string | number = ''): ClassRow | null {
  const share = whatifShare(g);
  if (share <= 0) return null;
  const mine = computerDesigns(g);
  const list = usableClasses(g, nat, of).filter((r) => (r.whatif || (r.own && mine)) && whyNoClass(g, nat, r) === null);
  if (!list.length || dialDraw(g, 'class', nat, g.turn, of, salt) >= share) return null;
  return list[Math.min(list.length - 1, Math.floor(dialDraw(g, 'which', nat, g.turn, of, salt) * list.length))];
}

// ---- In words -----------------------------------------------------------------------------------------

const pc = (x: number): string => `${Math.round(Math.abs(x - 1) * 100)}%`;
const more = (x: number, up: string, down: string): string => `${x >= 1 ? up : down} ${pc(x)}`;

/** One of a land class's edges in words: "Attacks 40% harder against fortified ground." */
function edgeWords(e: Edge): string {
  const parts: string[] = [];
  if (e.attack !== undefined && e.attack !== 1) parts.push(`attacks ${pc(e.attack)} ${e.attack > 1 ? 'harder' : 'weaker'}`);
  if (e.defence !== undefined && e.defence !== 1) parts.push(`defends ${pc(e.defence)} ${e.defence > 1 ? 'harder' : 'weaker'}`);
  if (!parts.length) return '';
  const s = parts.join(' and ');
  return `${s[0].toUpperCase()}${s.slice(1)}${e.why ? ` ${e.why}` : ''}.`;
}

type ShipStat = 'power' | 'tough' | 'escort' | 'raid';
const SHIPS = (navyData as unknown as { classes: Record<string, Record<ShipStat, number> & { one: string }> }).classes;

/** A ship class in words, against the standard ship of her kind (navy.json): her numbers are hulls.ts's. */
function shipWords(row: ClassRow): string {
  const std = SHIPS[row.of], one = std.one;
  const n = (k: ShipStat): number | undefined => (typeof row[k] === 'number' ? (row[k] as number) : undefined);
  const vs = (k: ShipStat): number | undefined => { const v = n(k); return v === undefined || std[k] <= 0 ? undefined : v / std[k]; };
  const out: string[] = [];
  // Twice or more in whole words: "three times as well", not "200% better".
  const times = (x: number): string => (Math.abs(x - 2) < 0.05 ? 'twice' : Math.abs(x - 3) < 0.05 ? 'three times' : `${Math.round(x * 10) / 10} times`);
  const p = vs('power'), t = vs('tough'), e = vs('escort'), r = vs('raid');
  if (p !== undefined && p >= 1.95) out.push(`Worth ${times(p)} a standard ${one} in a fleet action.`);
  else if (p !== undefined && Math.abs(p - 1) >= 0.05) out.push(`Worth ${pc(p)} ${p > 1 ? 'more' : 'less'} than a standard ${one} in a fleet action.`);
  if (t !== undefined && Math.abs(t - 1) >= 0.05) out.push(`${t < 1 ? 'Harder' : 'Easier'} to sink: lost ${pc(t)} ${t < 1 ? 'less' : 'more'} readily.`);
  if (e !== undefined && e >= 1.95) out.push(`Guards a convoy ${times(e)} as well.`);
  else if (e !== undefined && Math.abs(e - 1) >= 0.05) out.push(`Guards a convoy ${pc(e)} ${e > 1 ? 'better' : 'worse'}.`);
  else if (n('escort') && !std.escort) out.push('Guards convoys, which her kind does not.');
  if (r !== undefined && r >= 1.95) out.push(`Raids trade ${times(r)} as hard.`);
  else if (r !== undefined && Math.abs(r - 1) >= 0.05) out.push(`Raids trade ${pc(r)} ${r > 1 ? 'harder' : 'less'}.`);
  if (typeof row.deck === 'number' && row.of !== 'cv') out.push(`Flies ${Math.round(row.deck * 100)}% of a carrier's air wing.`);
  else if (typeof row.deck === 'number' && row.deck !== 1) out.push(`Flies ${Math.round(row.deck * 100)}% of a fleet carrier's air wing.`);
  if (typeof row.aa === 'number') out.push(`Cuts her fleet's losses to air attack by ${Math.round(row.aa * 100)}%.`);
  if (typeof row.eyes === 'number' && row.eyes > 0) out.push(`Her seaplane scouts ${row.eyes === 1 ? 'one sea' : `${row.eyes} seas`} beyond her fleet's.`);
  if (row.oil !== undefined && row.oil !== 1) out.push(`Burns ${pc(row.oil)} ${row.oil > 1 ? 'more' : 'less'} oil than her kind.`);
  return out.join(' ');
}

/** What a class does, in plain words worked out from its numbers, so that the words and the rule cannot drift apart. */
export function classRule(row: ClassRow): string {
  // (A ship's numbers mean what navy.json's do: 'escort' is convoy guarding, not an aircraft's.)
  if (SHIPS[row.of]) return shipWords(row);
  const out: string[] = [];
  for (const e of row.edges ?? []) { const w = edgeWords(e); if (w) out.push(w); }
  if (row.fight !== undefined && row.fight !== 1) out.push(`Each aircraft is worth ${more(row.fight, 'more', 'less').replace(/^(more|less) (.*)$/, '$2 $1')} in a fight in the air.`);
  if (row.reach !== undefined && row.reach !== 1) out.push(`Reaches ${pc(row.reach)} ${row.reach > 1 ? 'further' : 'less far'}.`);
  if (row.night !== undefined) out.push(`By night ${Math.round(row.night * 100)}% of its fighters find the raid.`);
  if (row.close !== undefined && row.close !== 1) out.push(`Worth ${pc(row.close)} ${row.close > 1 ? 'more' : 'less'} on call over a land battle.`);
  if (row.escort !== undefined) out.push(`Each of its bombers counts as ${Math.round(row.escort * 100)}% of a fighter in the escort.`);
  if (row.bomb !== undefined && row.bomb !== 1) out.push(`Puts ${Math.round(row.bomb * 100)}% of a bomber's weight of bombs on the land.`);
  if (row.sea !== undefined && row.sea !== 1) out.push(`Puts ${Math.round(row.sea * 100)}% of a bomber's weight on ships and shipping.`);
  if (row.most !== undefined) out.push(`The nation can keep ${row.most} at once.`);
  if (row.oil !== undefined && row.oil !== 1) out.push(`Burns ${pc(row.oil)} ${row.oil > 1 ? 'more' : 'less'} fuel than its kind.`);
  return out.join(' ');
}

/** What a technology that opens a class lets the nation order, in words: "The Flame tank corps can be ordered from the Arsenal." */
export function unlockWords(key: string): string | null {
  if (!key.startsWith('class:')) return null;
  const row = BY_ID.get(key.slice(6));
  if (!row) return null;
  const many = /s$/.test(row.label) ? row.label : `${row.label}s`;
  return `${many[0].toUpperCase()}${many.slice(1)} can be ordered from the Arsenal. ${classRule(row)}`.trim();
}
