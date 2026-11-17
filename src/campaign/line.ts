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

import { allied, atWar } from './game';
import { HISTORY, dialDraw, onBook, stopDef, stopOf } from './history';
import type { Game, Region } from './types';

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
}

const NONE: Line[] = [];
/** The lines the book draws (history.json, 'lines'). */
export const lines = (): Line[] => (HISTORY as { lines?: Line[] }).lines ?? NONE;
/** The line drawn on a country's ground, if the book has one. */
const lineOn = (land: string): Line | undefined => lines().find((L) => L.land === land);

/** True while a line binds this month: a stop that keeps the book, the book open in its theatre, and the player on neither side. */
export function lineBinds(g: Game, L: Line): boolean {
  return !!stopDef(g).book && onBook(g, L.theatre) && g.turn <= L.until && g.human !== L.land && g.human !== L.taker;
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
    const slack = stopDef(g).slack ?? 0;
    out = (L.spans[name] ?? []).map((s, k) => {
      const d = slack ? Math.floor(dialDraw(g, 'line', L.id, name, k) * (2 * slack + 1)) - slack : 0;
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

/**
 * True where the line shuts a province to a computer staff this month: it neither attacks the province nor marches
 * into it. Never true for the player's own corps, nor for ground the player holds.
 */
export function lineShut(g: Game, nat: string, r: Region): boolean {
  if (!lines().length || nat === g.human) return false;
  const L = lineOn(r.owner);
  if (!L || r.ctrl === g.human || !lineBinds(g, L)) return false;
  if (withSide(g, nat, L.taker)) return !inLine(g, L, r, g.turn);
  if (withSide(g, nat, L.land) && withSide(g, r.ctrl, L.taker)) return g.turn < L.floor.until && inLine(g, L, r, g.turn, L.floor.kinds);
  return false;
}

/** True for a corps of the invader's side standing on ground the line leaves out in month `t`: it goes back inside. */
export function lineLeaves(g: Game, nat: string, r: Region, t: number): boolean {
  if (!lines().length || nat === g.human) return false;
  const L = lineOn(r.owner);
  return !!L && lineBinds(g, L) && t <= L.until && withSide(g, nat, L.taker) && !inLine(g, L, r, t);
}

/** True if any line could bind this nation this month: a quick test before the march home is walked. */
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
    if (!lineBinds(g, L) || t > L.until || !withSide(g, nat, L.taker)) continue;
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

export const lineAny = (g: Game, nat: string): boolean => lines().some((L) => lineBinds(g, L) && nat !== g.human && withSide(g, nat, L.taker));
