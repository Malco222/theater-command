// The record's line (ruling 215(c), the land twin of the shores rule; the riders of 17:08Z). By the book a computer
// staff's bound in a theatre where history.json gives a line is the record's line for the month: the ground the
// invader held, month by month, as the record has it. It is a bound and never a gift: inside the line the invader
// still takes ground only by battles won, and inside it the country invaded must win its ground back.
//   the ceiling  the invader's computer staff (and an ally's) neither attacks nor enters a province of the invaded
//                country that the line leaves out this month; when a raid's months are over, its corps go back
//                inside the line (ai.ts, lineHome), and ground beyond the line with none of them left on it goes
//                back to whoever held it before the raid, as the record's withdrawals did (ruling 219, lineGives).
//   the floor    the invaded country's computer staff (and an ally's) neither attacks nor enters a province the
//                invader holds that a span of the floor's kinds covers this month, before the floor's month. Raid
//                ground it may always retake.
//   guards       only at a stop that keeps the book, only while the book is open in the line's theatre, and only to
//                the line's last month (the end of the record's spans; the floor may lift before). Never the player,
//                on either side: in a war the player fights as either
//                country the line binds nobody (a player's China faces a Japan held by the book's dated choice and
//                the staff's wait alone), ground a player holds is open to everyone, and nothing goes back to a player
//                or from one. At a stop with slack a
//                span's edges move by up to that many months, by the war's seed; the first and last months stay.
// The data is history.json, 'lines' (one per theatre); with none, nothing here does anything.
//
// The line in the south (ruling 234(a); the far side's rows, far-south-line.json, from the Pacific planner's
// south-line.json). The same two bounds, drawn on the ground of five countries at once, with four things China's
// line does not need:
//   lands    the countries invaded, in place of the one `land`. A player's own ground is never bound, nor is any
//            ground while the player is the invader; the rest of the line stands (computer against computer).
//   ground   Britain's and America's ground reaches round the world, and Germany and Italy fight beside Japan: the
//            line is drawn only on the Pacific's ground (pacific.ts) and on the provinces it names, in a span or
//            under `shut` (India's frontier lies beyond the Pacific's ground). A province it names is on the line
//            whoever owns it (New Caledonia is France's; the rest of France's ground, Indochina, is not on it).
//   back     the floor's month province by province: the road back's own month for each place (rulings 200(i) and
//            202(e)). A province with none keeps the floor's last month: it was passed, or held to the end.
//   stays    when a span ends the invader's corps keep what they hold: nobody marches home and nothing is handed
//            back without a fight. The end of a span means only that the invader does not come again.
//   field    the floor also covers a province where the invader's army holds the field and the town still holds
//            out: on this branch a landing that wins the field does not yet take the province (ruling 232 does that).
//   slides   at a stop with slack the whole calendar moves with the month of the great choice named (Japan's
//            strike south), by that choice's own draw: the war's first landings are never shut out by their own line.
//
// The line in East Africa (the design director's 13:43Z of 22 November 2026, form (ii) to C; far-africa.json, from
// the historian's iea-sahara-1940-41.md). Italy is the one it gives ground to, and it is drawn on named provinces
// only (`ground: 'named'`): Italy's own, each to its last Italian month (a span of kind 'own', and the same month
// under `back`, so that Britain's staff comes in that month and not before), its gains on Britain's ground
// (Kassala, Gallabat and Kurmuk from July 1940, British Somaliland from August, kind 'gain'), and under `shut` the
// ring of ground round them that Italy never held. As first laid without it, Italy's staff read seventeen to two
// in its war's first month and took Port Sudan and Khartoum, and Britain's was in Addis Ababa in month 15.
//
// The stations (the design director's seam c of 14:18Z and his 18:28Z of 22 November 2026: a book-placed row holds its
// stations, one rule for every line). While a line binds, a held corps of the invader's own that stands on ground the
// line gives him this month keeps its hold: its staff's release does not let it go (ai.ts, release). A dated attack
// of the book still moves its force (stations.ts), and a held corps sent back inside the line when a raid's months
// are over is held again where it comes to rest (ai.ts, lineHome). `lineKeeps`.

import { allied, atWar } from './game';
import { HISTORY, bookDate, dialDraw, onBook, stopDef, stopOf } from './history';
import { pacificGround } from './pacific';
import type { Corps, Game, Region } from './types';

/** The months the invader held a province, and how (the historian's kinds: 'day one', 'lasting', 'raid', 'ichigo'). */
export interface Span { from: number; to: number; kind: string }
export interface Line {
  id: string;
  /** The book's theatre: when the book is closed there, the line is gone with it. */
  theatre: string;
  /** The country invaded, whose ground the line is drawn on, and the invader. */
  land: string;
  taker: string;
  /** The line's last month: from the month after it, the staffs decide for themselves. */
  until: number;
  /** The floor: before this month, the invaded country does not retake what a span of these kinds covers. */
  floor: { until: number; kinds: string[] };
  /** For each province the invader ever held, the months he held it. A province not named was never his. */
  spans: Record<string, Span[]>;
  /** The countries invaded, where the line is drawn on the ground of several: read in place of `land`. */
  lands?: string[];
  /** Where the line is not drawn on all the ground of the countries invaded: 'pacific' is the Pacific's ground and every province named in `spans` or `shut`; 'named' is the provinces named in `spans` or `shut` and no other. */
  ground?: string;
  /** Provinces beyond the Pacific's ground that the line is drawn on and leaves out. */
  shut?: string[];
  /** The floor's month province by province. One not named keeps the floor's own last month. */
  back?: Record<string, number>;
  /** True where the invader's corps keep what they hold when a span ends: nobody marches home, nothing is handed back. */
  stays?: boolean;
  /** The great choice ('JAP:south') whose month the whole calendar slides with at a stop with slack. */
  slides?: string;
  /** True where the floor reads who holds the field as well as who holds the province. */
  field?: boolean;
}

const NONE: Line[] = [];
/** The lines the book draws (history.json, 'lines'). */
export const lines = (): Line[] => (HISTORY as { lines?: Line[] }).lines ?? NONE;
const landsOf = (L: Line): string[] => L.lands ?? [L.land];
const shutSets = new WeakMap<Line, Set<string>>();
const shutOf = (L: Line): Set<string> => { let set = shutSets.get(L); if (!set) shutSets.set(L, (set = new Set(L.shut ?? []))); return set; };
/** True for ground a line is drawn on: a province of a country invaded, and within the line's own ground where it has one. */
function onGround(g: Game, L: Line, r: Region): boolean {
  // (A province a line with its own ground names is on it whoever owns it: New Caledonia, France's, under 'shut'.)
  if (L.ground && (L.spans[r.name] || shutOf(L).has(r.name))) return true;
  // ('named': the line is drawn on the provinces it names and on no other. East Africa's: Britain's ground and
  // Italy's reach round the world, and the line is the horn of Africa's alone.)
  if (L.ground === 'named') return false;
  if (L.lands ? !L.lands.includes(r.owner) : L.land !== r.owner) return false;
  return !L.ground || pacificGround(g, r);
}
/** The line drawn on a province, if the book has one there. (Never on the player's own ground: computer against computer.) */
const lineOn = (g: Game, r: Region): Line | undefined => (r.owner === g.human ? undefined : lines().find((L) => onGround(g, L, r)));

/** True while a line binds this month: a stop that keeps the book, the book open in its theatre, and the player on neither side. */
export function lineBinds(g: Game, L: Line): boolean {
  return !!stopDef(g).book && onBook(g, L.theatre) && g.turn <= L.until && (!!L.lands || g.human !== L.land) && g.human !== L.taker;
}

/** How many months a line's whole calendar has slid in this war: the draw of the great choice it slides with. */
function slide(g: Game, L: Line): number {
  if (!L.slides) return 0;
  const [nat, op] = L.slides.split(':');
  const date = HISTORY.dates[nat]?.[op], now = bookDate(g, nat, op);
  return date === undefined || now === null ? 0 : now - date;
}

// A span's months in this war, moved by the stop's slack (stop 2: a month either way) as a whole, so that a raid of a
// month stays a month, and drawn once for the war. The first day and the line's last month do not move.
const edges = new WeakMap<Game, { stop: number; at: Map<string, [number, number][]> }>();
function monthsOf(g: Game, L: Line, name: string): [number, number][] {
  const stop = stopOf(g);
  let memo = edges.get(g);
  if (!memo || memo.stop !== stop) edges.set(g, (memo = { stop, at: new Map() }));
  const key = `${L.id}:${name}`;
  let out = memo.at.get(key);
  if (!out) {
    const slack = stopDef(g).slack ?? 0, all = slide(g, L);
    out = (L.spans[name] ?? []).map((s, k) => {
      const d = L.slides ? all : slack ? Math.floor(dialDraw(g, 'line', L.id, name, k) * (2 * slack + 1)) - slack : 0;
      return [s.from <= 1 ? s.from : s.from + d, s.to >= L.until ? s.to : s.to + d];
    });
    memo.at.set(key, out);
  }
  return out;
}

/** True if the line gives this province to the invader in month `t`; with `kinds`, by a span of one of them. */
export function inLine(g: Game, L: Line, r: Region, t: number, kinds?: string[]): boolean {
  const spans = L.spans[r.name];
  if (!spans) return false;
  const m = monthsOf(g, L, r.name);
  return spans.some((s, k) => (!kinds || kinds.includes(s.kind)) && t >= m[k][0] && t <= m[k][1]);
}

const withSide = (g: Game, nat: string, side: string): boolean => nat === side || allied(g, nat, side);
const withLands = (g: Game, nat: string, L: Line): boolean => landsOf(L).some((x) => withSide(g, nat, x));
/** The month the floor lifts on a province: its own road-back month where the line gives one, else the floor's last. */
const floorUntil = (g: Game, L: Line, r: Region): number => { const m = L.back?.[r.name]; return m === undefined ? L.floor.until : m + slide(g, L); };

/**
 * True for ground of a country invaded where the invader's own army of that front stands: a line that sends its
 * corps home when a span ends (China's) is drawn on it, and `nat` is of the invader's side. The staff's ships take
 * nobody from there for a war elsewhere (ai.ts hop): by the book that army keeps the record's line.
 */
export function frontArmys(g: Game, nat: string, r: Region): boolean {
  if (!lines().length || nat === g.human) return false;
  const L = lineOn(g, r);
  return !!L && !L.stays && lineBinds(g, L) && withSide(g, nat, L.taker);
}

/**
 * True for ground a line is drawn on that it never gives the invader to keep: no span of the floor's kinds names it
 * (ruling 200(i): what his side takes there, Midway on its one try or ground beyond the book, is open to the other
 * side from the month it is lost).
 */
export function neverHis(g: Game, r: Region): boolean {
  const L = lineOn(g, r);
  return !!L && !(L.spans[r.name] ?? []).some((s) => L.floor.kinds.includes(s.kind));
}

/**
 * True where the line shuts a province to a computer staff this month: it neither attacks the province nor marches
 * into it. Never true for the player's own corps, nor for ground the player holds.
 */
export function lineShut(g: Game, nat: string, r: Region): boolean {
  if (!lines().length || nat === g.human) return false;
  const L = lineOn(g, r);
  if (!L || r.ctrl === g.human || !lineBinds(g, L)) return false;
  if (withSide(g, nat, L.taker)) return !inLine(g, L, r, g.turn);
  // (Where the line says so, the floor reads who holds the field: a province the invader's army stands in with its town still holding out is his ground for the floor, as it will be once a field won takes the province: ruling 232.)
  const his = withSide(g, r.ctrl, L.taker) || (!!L.field && withLands(g, r.ctrl, L) && r.units.some((u) => !u.dead && withSide(g, u.nat, L.taker)));
  if (withLands(g, nat, L) && his) return g.turn < floorUntil(g, L, r) && inLine(g, L, r, g.turn, L.floor.kinds);
  return false;
}

/**
 * True where the line shuts a province to the invader's side this month and still owes it to him: a span of the
 * province's opens in a later month of this war (as the stop moves it). `kinds` narrows the spans read.
 */
export function lineOwes(g: Game, nat: string, r: Region, kinds?: string[]): boolean {
  if (!lines().length || nat === g.human) return false;
  const L = lineOn(g, r);
  if (!L || r.ctrl === g.human || !lineBinds(g, L) || !withSide(g, nat, L.taker) || inLine(g, L, r, g.turn)) return false;
  const spans = L.spans[r.name] ?? [], m = monthsOf(g, L, r.name);
  return spans.some((s, k) => (!kinds || kinds.includes(s.kind)) && m[k][0] > g.turn);
}

/** True for a corps of the invader's side standing on ground the line leaves out in month `t`: it goes back inside. */
export function lineLeaves(g: Game, nat: string, r: Region, t: number): boolean {
  if (!lines().length || nat === g.human) return false;
  const L = lineOn(g, r);
  return !!L && !L.stays && lineBinds(g, L) && t <= L.until && withSide(g, nat, L.taker) && !inLine(g, L, r, t);
}

/**
 * Raids go back (ruling 219): the ground of the invaded country that the invader's side holds beyond the line in month
 * `t` with no corps of that side on it, in the field or in its town, and whom each province goes back to: whoever held
 * it before the invader took it, or else the invaded country, so long as it is a computer still at war with the holder.
 * Nothing a player holds, and nothing to a player.
 */
export function lineGives(g: Game, nat: string, t: number): { r: Region; to: string }[] {
  const out: { r: Region; to: string }[] = [];
  if (nat === g.human) return out;
  for (const L of lines()) {
    if (L.stays || !lineBinds(g, L) || t > L.until || !withSide(g, nat, L.taker)) continue;
    for (const r of g.regions) {
      if (r.owner !== L.land || r.ctrl === g.human || !withSide(g, r.ctrl, L.taker) || inLine(g, L, r, t)) continue;
      if (r.units.some((c) => !c.dead && withSide(g, c.nat, L.taker)) || (r.garr ?? []).some((c) => !c.dead && withSide(g, c.nat, L.taker))) continue;
      const fits = (x: string | null): x is string => !!x && x !== g.human && !!g.nations[x]?.alive && !withSide(g, x, L.taker) && atWar(g, x, r.ctrl);
      const to = fits(r.prev) ? r.prev : fits(L.land) ? L.land : null;
      if (to) out.push({ r, to });
    }
  }
  return out;
}

/** The line drawn on a province, if the book has one there: for a report. */
export const lineOf = (g: Game, r: Region): Line | undefined => lineOn(g, r);

/** For the measuring harness: told of each held corps a line keeps on its station when its staff's release names its ground. Nothing in the game listens. */
export const lineKeepWatch: { tell: ((e: { line: string; corps: Corps; at: Region }) => void) | null } = { tell: null };

/**
 * True of a corps of the invader's own standing on ground his line gives him in month `t`, while the line binds: a
 * held one keeps its station there (the design director's seam c of 14:18Z and his 18:28Z of 22 November 2026, one
 * rule for every line). In East Africa Italy's staff opened an offensive of its own in June 1940, and a live front
 * let go every held row on its ground: fourteen of Italy's twenty-two rows were loose that month, Gondar's marched
 * to Kurmuk in July and stood at Keren in March 1941, where it held the 4th and 5th Indian Divisions a month past
 * the record. By the book the rows keep their places until their month ends or a dated attack takes their ground.
 * Never a player's corps, nor on a player's own ground; where no line binds, the staff decides as before.
 */
export function lineKeeps(g: Game, c: Corps, t = g.turn): boolean {
  if (!lines().length || c.nat === g.human) return false;
  const r = g.regions[c.reg];
  const L = lineOn(g, r);
  return !!L && c.nat === L.taker && lineBinds(g, L) && inLine(g, L, r, t);
}

/** True if any line could bind this nation this month: a quick test before the march home is walked. */
export const lineAny = (g: Game, nat: string): boolean => lines().some((L) => !L.stays && lineBinds(g, L) && nat !== g.human && withSide(g, nat, L.taker));
