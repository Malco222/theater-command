// The Pacific war's ties (ruling 197, the offer of war; the Pacific planner's paper 12).
//
// In the game a war is a thread between two countries. December 1941 was a knot: one blow tied five threads in a
// week. This file says which threads the knot ties, and when. Its numbers and names are data/campaign/pacific.json;
// the entry of Germany and Italy into America's war is two events in far-pacific-war.json. Nothing here happens
// while the far side's switch is off (stage.ts), nor in a campaign whose chart does not reach the far side, nor in
// the last act of a campaign of domination, where every power fights for itself.
//
//   WHERE JAPAN STANDS. Outside both blocs, leaning to the Axis (its nation row). So the surprise of its first blows
//     is read by the rule as built (diplomacy.ts wouldSurprise): a blow on a country at peace with it and with every
//     ally it has. The day it strikes a country that is at war with the Axis it becomes the Axis's co-belligerent
//     (game.ts declareWar, as built), so a staff that means to surprise America declares on America first.
//
//   THE TIES, read as each month opens from the state as it stands (monthlyDiplomacy), the first three before
//     history's events and the last three after them:
//     1. the freeze (below);
//     2. Britain and America with each other: Japan at war with one, and the two standing together in an alliance,
//        the other declares war on Japan the month after. A country in the player's hands is never taken to war by
//        it: the card is his;
//     3. the Soviet Union and Japan: the two computers keep the peace while Germany stands (no great choice of either
//        names the other). Three months after Germany is beaten the Soviet staff goes to war with Japan, if Japan is
//        still at war with America or Britain; at the stops that keep the book not before August 1945;
//     4. Japan beside the Axis: from the month it is at war with an Allied country that is at war with the Axis,
//        however that came about (America, struck while neutral, has since entered Germany's war);
//     5. China, at war with Japan from the first day and in neither bloc, stands with the Allies from that month;
//     6. a player's America at war with Japan stands with the Allies from that month too (ruling 32: the computer's
//        does by world.ts worldMonth).
//
//   THE SAME DAY (game.ts declareWar): the dominions, and the Dutch while they stand with the Allies, are at war with
//     Japan the day Britain is, whether Britain's war came by a blow or by a tie. (After the Netherlands has fallen
//     the Indies are in British keeping, ruling 199: a blow on them is a blow on Britain.)
//
//   GERMANY AND ITALY WITH JAPAN. The entry of December 1941 stays on its date, read as that month closes
//     (diplomacy.ts closeDiplomacy): Japan moves last in a month, and Germany declared four days after Pearl Harbor.
//     It comes sooner, the month after Japan and America go to war, only once a pact binds: from September 1940 at
//     the stops that keep the book, from the third month after France is beaten at the others, and only for a war
//     begun once it binds (the month declareWar keeps against the pact's month).
//
//   THE FREEZE. America's sales to Japan, and the south's, stop: at the stops that keep the book in August 1941; at
//     the others the month after Japan's troops hold Saigon or Cam Ranh; at every stop the month after Japan is at
//     war with Britain or the Dutch. It does not lift. It is Japan's clock: the oil book reads it when it is laid
//     (rulings 194 and 195), and the staff's month at the looser stops is counted from it (ruling 200).

import data from '../../data/campaign/pacific.json';
import { FAR_STAGE } from './stage';
import type { Game, Nation } from './types';
import { atWar, bloc, declareWar, headline, joinedSide, newsName, sameSide, setWar, theName } from './game';
import { WORLD, farOn, standWithAllies } from './world';
import { stopDef } from './history';
import { ownPath } from './league';

interface Rules {
  power: string; bloc: 'axis' | 'allies'; china: string;
  pair: [string, string];
  with: Record<string, string[]>;
  pact: { with: string; book: number; fallen: string; months: number };
  freeze: { book: number; places: string[]; wars: string[]; seller: string; south: string[] };
  north: { nat: string; fallen: string; months: number; book: number; while: string[] };
  says: Record<string, string>;
}

/** The Pacific's rules. None while the far side's switch is off. */
export const PACIFIC: Rules | null = FAR_STAGE ? (data as unknown as Rules) : null;

const FREEZE = 'pac:freeze', PACT = 'pac:pact';

/** True where the ties read: the grand campaign on the whole world, before its last act. */
const reads = (g: Game): boolean => PACIFIC !== null && g.flags.open === 1 && !((g.flags.dom ?? 0) > 0) && farOn(g);
const live = (g: Game, id: string): Nation | null => { const n = g.nations[id]; return n?.alive && !n.dormant ? n : null; };
const fill = (line: string, words: Record<string, string>): string => line.replace(/\{(\w+)\}/g, (m, k: string) => words[k] ?? m);
const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);

/** The month the freeze came, or 0 while America still sells. */
export const freezeAt = (g: Game): number => (reads(g) ? g.flags[FREEZE] ?? 0 : 0);
/** The month from which a pact binds Germany and Italy to Japan's war with America, or 0 while none does. */
export const pactAt = (g: Game): number => (reads(g) ? g.flags[PACT] ?? 0 : 0);

/**
 * 'pact:JAP,USA', a condition of history's events (diplomacy.ts holds): a pact binds Germany and Italy to the war
 * between the two, which began once it bound. A war begun before it stays Japan's alone until the entry's own date.
 */
export function pactBinds(g: Game, a: string, b: string): boolean {
  const P = PACIFIC;
  if (!P || !reads(g) || !((a === P.power && b === P.pact.with) || (b === P.power && a === P.pact.with))) return false;
  const at = g.flags[PACT];
  if (at === undefined || !atWar(g, a, b)) return false;
  return Math.max(g.flags[`war:${a}:${b}`] ?? 0, g.flags[`war:${b}:${a}`] ?? 0) >= at;
}

/** The countries that are at war with Japan the day `lead` is: its dominions, and those the rules name with it, while they stand in its alliance. */
function followers(g: Game, lead: string): void {
  const P = PACIFIC!, J = live(g, P.power), L = live(g, lead);
  if (!J || !L || !atWar(g, lead, J.id)) return;
  const ids = [...Object.entries(WORLD.follow).filter(([, of]) => of === lead).map(([id]) => id), ...(P.with[lead] ?? [])];
  const came: Nation[] = [];
  for (const id of ids) {
    const n = live(g, id);
    // (One in the player's hands goes to war only when its player says so, as with every war of its mother country's.)
    if (!n || id === g.human || n.atWar.includes(J.id) || !sameSide(g, id, lead) || sameSide(g, id, J.id)) continue;
    setWar(g, id, J.id);
    g.flags[`war:${id}:${J.id}`] = g.turn;
    came.push(n);
  }
  if (!came.length) return;
  g.supplyDirty = true;
  headline(g, 'war', fill(P.says.follow, { Names: cap(listed(came.map((n) => theName(n)))), are: came.length === 1 ? 'is' : 'are', power: theName(J), lead: newsName(L) }), came[0].id);
}

/** A declaration of war as the Pacific's ties read it (game.ts declareWar, once the war is set): who is at war with Japan the same day. */
export function pacificWar(g: Game, a: string, b: string): void {
  const P = PACIFIC;
  if (!P || !reads(g)) return;
  const other = a === P.power ? b : b === P.power ? a : null;
  if (other !== null) followers(g, other);
}

/** The freeze: the month America's sales to Japan stop, and the south's. Kept in the campaign's flags; it does not lift. */
function freezeMonth(g: Game): void {
  const P = PACIFIC!, F = P.freeze, J = live(g, P.power);
  if (!J || g.flags[FREEZE] !== undefined) return;
  const war = F.wars.some((e) => live(g, e) && atWar(g, J.id, e));
  const due = stopDef(g).book ? g.turn >= F.book : F.places.some((nm) => g.byName[nm]?.ctrl === J.id);
  if (!war && !due) return;
  g.flags[FREEZE] = g.turn;
  // The news is of the trade that stops: a seller already at war with Japan stopped the day the war came.
  const sells = (id: string): boolean => !!live(g, id) && !atWar(g, J.id, id);
  if (!sells(F.seller)) return;
  const [uk, ned] = F.south.map(sells);
  headline(g, 'event', P.says.freeze + (uk && ned ? P.says.freezeBoth : uk ? P.says.freezeBritain : ned ? P.says.freezeDutch : ''), J.id);
}

/**
 * The month's opening, before history's events are read: the month a pact begins to bind, the freeze, Britain and
 * America with each other, and the Soviet staff's month.
 */
export function pacificOpening(g: Game): void {
  const P = PACIFIC;
  if (!P || !reads(g)) return;
  const J = live(g, P.power);
  if (!J) return;
  if (g.flags[PACT] === undefined) {
    const F = g.nations[P.pact.fallen];
    const at = stopDef(g).book ? P.pact.book : F && !F.alive && F.fellOn ? F.fellOn.turn + P.pact.months : null;
    if (at !== null && g.turn >= at) g.flags[PACT] = at;
  }
  freezeMonth(g);
  // Britain and America with each other, read from the state as the month opens: an entry into the war this month
  // (history's events come after) brings the other in next month.
  const [x, y] = P.pair;
  for (const [a, b] of [[x, y], [y, x]]) {
    const A = live(g, a), B = live(g, b);
    if (!A || !B || b === g.human || !atWar(g, J.id, a) || atWar(g, J.id, b) || sameSide(g, b, J.id)) continue;
    const side = bloc(A);
    if (side === null || side === 'league' || bloc(B) !== side) continue;
    declareWar(g, b, J.id);
  }
  // The Soviet staff's month.
  const N = P.north, S = live(g, N.nat), G = g.nations[N.fallen];
  if (S && N.nat !== g.human && !atWar(g, N.nat, J.id) && !sameSide(g, N.nat, J.id) && G && !G.alive && G.fellOn
    && g.turn >= G.fellOn.turn + N.months && (!stopDef(g).book || g.turn >= N.book)
    && N.while.some((e) => live(g, e) && atWar(g, J.id, e)) && !N.while.some((e) => atWar(g, N.nat, e))) {
    headline(g, 'event', fill(P.says.north, { fallen: theName(G), nat: theName(S) }), N.nat);
    declareWar(g, N.nat, J.id);
  }
}

/**
 * Who stands where, read once history's events have been (as a month opens, and again as it closes): Japan beside
 * the Axis from the month it is at war with an Allied country that is at war with the Axis, China with the Allies
 * from that month, and a player's America at war with Japan with them too.
 */
export function pacificSides(g: Game): void {
  const P = PACIFIC;
  if (!P || !reads(g)) return;
  const J = live(g, P.power);
  if (!J) return;
  const other = P.bloc === 'axis' ? 'allies' : 'axis';
  if (bloc(J) === null) {
    let foe: Nation | null = null, his: Nation | null = null;
    for (const e of J.atWar) {
      const X = live(g, e);
      if (!X || bloc(X) !== other) continue;
      const y = X.atWar.map((id) => live(g, id)).find((Y) => !!Y && Y.id !== J.id && bloc(Y) === P.bloc) ?? null;
      if (y) { foe = X; his = y; break; }
    }
    if (foe && his) {
      J.align = P.bloc === 'axis' ? 'axis_cobelligerent' : 'allies_cobelligerent';
      J.opinion = J.home = P.bloc === 'axis' ? 100 : -100;
      g.flags[`step:${J.id}`] = g.turn;
      g.supplyDirty = true;
      joinedSide(g, J.id);
      headline(g, 'event', fill(P.says.beside, { Power: theName(J, true), foe: newsName(foe), axis: theName(his) }), J.id);
    }
  }
  if (bloc(J) !== P.bloc) return;
  const C = live(g, P.china);
  if (C && bloc(C) === null && !ownPath(g, C.id) && atWar(g, C.id, J.id)
    && Object.values(g.nations).some((x) => x.id !== C.id && x.alive && !x.dormant && bloc(x) === other)) {
    C.align = other;
    C.opinion = C.home = other === 'allies' ? -100 : 100;
    g.flags[`step:${C.id}`] = g.turn;
    g.supplyDirty = true;
    joinedSide(g, C.id);
    headline(g, 'event', fill(P.says.china, { China: theName(C, true), power: theName(J) }), C.id);
  }
  if (g.human && g.human !== J.id && atWar(g, g.human, J.id)) standWithAllies(g, g.human);
}
