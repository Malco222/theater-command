// The Winter War and the peace of Moscow (ruling 228, 1b; the grand campaign only). The Soviet staff's war on
// Finland is one of its great choices (data/campaign/open.json, ops.SOV 'winter'): dated by the book at stops 1 and
// 2 (history.json), its staff's own choice at stops 3 to 5, a Soviet player's own choice always. What the op carries
// besides is read here:
//   works: the Mannerheim Line, laid when the grand campaign opens while Finland still holds the isthmus.
//   peace: how the war ends. By the book (stops 1 and 2, nobody of the two in the player's hands) Finland signs in
//     March 1940 and gives up Karelia whoever holds it. Off the book the peace comes from 'from', or sooner once
//     the Red Army holds Karelia (its town, Viipuri, has fallen), and by 'by' at the latest: Moscow feared the West
//     would step in. It cedes Karelia only if held; any other Finnish ground the Red Army holds goes back, as Petsamo
//     did in 1940. A war begun later than the op's months has its peace 'after' months on.
//   A Soviet or a Finnish player is offered the peace as a choice from the same month: take the terms or fight on.
//   The offer stands while the war lasts, and its terms are what the Red Army holds that month.
// A war that the great war swallows (the Soviet Union at war with a great power, or Finland in an alliance) is
// no longer the Winter War: its peace is the great war's.

import openData from '../../data/campaign/open.json';
import { majorsOf, signPeace, transfer, withdraw } from './diplomacy';
import { atWar, bloc, headline, theName } from './game';
import { dialDraw, onBook, stopDef, theatreOf } from './history';
import type { Game, Region } from './types';

interface Peace { cede: string[]; from: number; by: number; after: number; chance: number }
interface WinterOp { id: string; declare?: string[]; to?: number; works?: Record<string, [number, ...string[]]>; peace?: Peace }

const RED = 'SOV', FIN = 'FIN';
const OP = ((openData as unknown as { ops: Record<string, WinterOp[]> }).ops[RED] ?? []).find((o) => o.id === 'winter' && !!o.peace) ?? null;

const W = {
  book: 'Finland has signed the peace of Moscow. {after}it gives up Karelia, with Viipuri, the Mannerheim Line and its shore of Lake Ladoga. Some 420,000 of its people, a ninth of the nation, have left their homes rather than stay. The Red Army has its new frontier, at five times Finland\'s losses.',
  long: 'After a hundred and five days of war ',
  held: 'Finland has signed the peace of Moscow. It gives up Karelia, which the Red Army holds, and most of the province\'s people have left their homes rather than stay.',
  none: 'Finland and the Soviet Union have made peace in Moscow. The frontier stays where it was.',
  back: 'The Red Army hands back {list}.',
};

const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);

/** The Mannerheim Line: the works the op names, laid on ground their country still owns and holds (startOpen). */
export function layWinterWorks(g: Game): void {
  for (const [name, [lv, ...vs]] of Object.entries(OP?.works ?? {})) {
    const r = g.byName[name];
    if (r && !r.fort && vs.length && r.owner === r.ctrl && !vs.includes(r.owner)) r.fort = { lv, vs: [...vs], by: r.owner };
  }
}

/**
 * The Winter War's place in a grand campaign, as it opens (startOpen) or as a save from before the war had one is
 * loaded: the Mannerheim Line is laid, and a campaign opened after the op's months is marked late, so that history's
 * mark for Finland (history.json, 'finland') does not ask for a war the months before it never had.
 */
export function openWinter(g: Game, loaded = false): void {
  if (!OP) return;
  if (loaded && (Object.keys(g.flags).some((k) => k.startsWith('wnt:')) || g.byName.Karelia?.fort || atWar(g, RED, FIN))) return;
  layWinterWorks(g);
  if (g.turn > (OP.to ?? 0)) g.flags['wnt:late'] = g.turn;
}

/** True once the Winter War is over as a story of its own: made peace, or swallowed by the great war. */
export const winterOver = (g: Game): boolean => !!g.flags['wnt:done'];

/** True while the war between the two has become part of the great war: Moscow at war with a great power, or Finland in an alliance. */
const swallowed = (g: Game): boolean => {
  const big = majorsOf(g);
  return g.nations[RED].atWar.some((e) => e !== FIN && big.includes(e)) || bloc(g.nations[FIN]) !== null;
};

/** The month the Winter War began, while it lasts as a war of its own; 0 otherwise. */
export const winterSince = (g: Game): number => g.flags['wnt:on'] ?? 0;

/** The ground the peace would hand over this month, and the Finnish ground the Red Army holds that it would hand back. */
function terms(g: Game): { cede: Region[]; back: Region[] } {
  const named = new Set(OP?.peace?.cede ?? []);
  const cede = [...named].map((n) => g.byName[n]).filter((r): r is Region => !!r && r.owner === FIN && r.ctrl === RED);
  const back = g.regions.filter((r) => r.owner === FIN && r.ctrl === RED && !named.has(r.name));
  return { cede, back };
}

/** The months the peace may come in, for a war begun in `at`: from the op's months, or `after` months on for a war begun later. */
function window(at: number): { from: number; by: number } {
  const P = OP!.peace!;
  return { from: Math.max(P.from, at + P.after), by: Math.max(P.by, at + P.after + (P.by - P.from)) };
}

/** Ends the Winter War as a story of its own: no more offers, and the flags go. */
function close(g: Game): void {
  if (!g.flags['wnt:on']) return;
  g.flags['wnt:done'] = g.turn;
  delete g.flags['wnt:on'];
  delete g.flags['wnt:offer'];
  delete g.flags['wnt:card'];
  delete g.flags['wnt:cardAt'];
}

/**
 * The peace of Moscow. By the book Karelia goes whoever holds it; otherwise only if the Red Army holds it. Every other
 * piece of Finland it holds is handed back, the troops go home, and the news says what was signed.
 */
export function makeWinterPeace(g: Game, book: boolean): boolean {
  if (!OP?.peace || !winterSince(g) || !atWar(g, RED, FIN)) return false;
  const at = winterSince(g);
  const { cede, back } = terms(g);
  const given = book ? (OP.peace.cede.map((n) => g.byName[n]).filter((r): r is Region => !!r && r.owner === FIN && (r.ctrl === FIN || r.ctrl === RED))) : cede;
  for (const r of given) {
    // (The works face the frontier that is gone, and the Red Army that dug the old ones out has no use for them.)
    r.fort = null;
    for (const k of ['siege', 'sgby', 'sgm', 'guard']) delete g.flags[`${k}:${r.id}`];
    transfer(g, r, RED);
    withdraw(g, FIN, r);
  }
  if (!signPeace(g, RED, FIN)) return false;
  for (const k of ['st:on', 'st:pushed', 'st:rest']) delete g.flags[`${k}:${RED}:${FIN}`];
  g.supplyDirty = true;
  const handed = back.map((r) => r.name);
  const text = book
    ? cap(W.book.replace('{after}', at === 3 && g.turn === 7 ? W.long : '').replace('Moscow. it', 'Moscow. It'))
    : given.length ? W.held : W.none;
  headline(g, 'event', handed.length ? `${text} ${W.back.replace('{list}', listOf(handed))}` : text, FIN);
  close(g);
  return true;
}

/**
 * The month's reading of the Winter War, at the month's opening (after the diplomacy of the month). It notes a war
 * that has begun, ends one the great war has swallowed or that is over by other means, makes the offer to a player
 * on either side, and settles a war the computer fights on both sides.
 */
export function winterMonth(g: Game): void {
  if (g.flags.open !== 1 || !OP?.peace) return;
  const war = atWar(g, RED, FIN) && g.nations[RED].alive && g.nations[FIN].alive;
  if (!winterSince(g)) {
    if (!war || g.flags['wnt:done']) return;
    // (A war between the two that the great war brought is not the Winter War, and there is only ever one.)
    if (swallowed(g)) { g.flags['wnt:done'] = g.turn; return; }
    g.flags['wnt:on'] = g.flags[`war:${RED}:${FIN}`] ?? g.flags[`war:${FIN}:${RED}`] ?? g.turn;
  }
  if (!war || swallowed(g)) { close(g); return; }
  const at = winterSince(g), P = OP.peace, { from, by } = window(at);
  const held = terms(g).cede.length > 0;
  if (g.human === RED || g.human === FIN) {
    if (held || g.turn >= from) g.flags['wnt:offer'] ??= g.turn;
    return;
  }
  // By the book Finland signs in its month, and gives up Karelia whoever holds it.
  if (stopDef(g).book && onBook(g, theatreOf(RED, OP.id))) {
    if (g.turn >= P.from) makeWinterPeace(g, true);
    return;
  }
  if ((held && g.turn > at) || g.turn >= by || (g.turn >= from && dialDraw(g, 'wnt', g.turn) < P.chance)) makeWinterPeace(g, false);
}

/** The peace as a player sees it: the card at the month's start and the standing offer under Powers. */
export interface WinterPeace {
  /** Who offers (Finland to a Soviet player, the Soviet Union to a Finnish one), and the player. */
  from: string; to: string;
  title: string; body: string; note: string;
  /** The lit answer, pressed by Enter and by Esc: the one that can be undone next month. */
  no: string; noLine: string;
  /** The answer that ends the war, a click. */
  yes: string; yesLine: string;
  /** The line on the other side's row under Powers. */
  row: string;
  cede: string[]; back: string[];
  since: number;
  /** A number that changes when the terms do: the card comes up again when it changes. */
  key: number;
}

export function winterPeace(g: Game): WinterPeace | null {
  // (A war the great war has swallowed this month has no peace of its own to offer, though the month's opening has not yet closed it.)
  if (g.flags.open !== 1 || !OP?.peace || !g.flags['wnt:offer'] || !winterSince(g) || !atWar(g, RED, FIN) || swallowed(g)) return null;
  if (g.human !== RED && g.human !== FIN) return null;
  const { cede, back } = terms(g);
  const held = cede.length > 0, red = g.human === RED;
  const key = (held ? 2 : 1) + 4 * back.length;
  const cedeNames = cede.map((r) => r.name), backNames = back.map((r) => r.name);
  if (red) {
    return {
      from: FIN, to: RED, title: 'Finland asks for peace',
      body: `${held ? 'Helsinki will give up Karelia, with Viipuri and the Mannerheim Line, if the war ends now.' : 'Helsinki asks for peace on the frontier as it stands.'}${back.length ? ' Any other Finnish ground you hold goes back.' : ''}`,
      // (Only the book's terms were the ones Moscow took: the UI director's mend.)
      note: held ? 'In March 1940 Moscow took these terms: Britain and France were readying an army to help Finland.' : '',
      no: 'Fight on', noLine: 'The war goes on. The offer stands while it lasts, on Finland\'s row under Powers.',
      yes: 'Make peace', yesLine: held ? 'Karelia becomes Soviet ground and the war with Finland ends.' : 'The war with Finland ends where the armies stand.',
      row: held ? 'Asks for peace: it gives up Karelia if the war ends now.' : 'Asks for peace on the frontier as it stands.',
      cede: cedeNames, back: backNames, since: g.flags['wnt:offer'], key,
    };
  }
  return {
    from: RED, to: FIN, title: 'Moscow offers peace',
    body: `${held ? 'The war ends if Finland gives up Karelia, with Viipuri and the Mannerheim Line.' : 'The war ends on the frontier as it stands.'}${back.length ? ' The Red Army hands back the rest.' : ''}`,
    note: '',
    no: 'Fight on', noLine: `The war goes on. The offer stands while it lasts, on ${theName(g.nations[RED])}'s row under Powers.`,
    yes: 'Take the terms', yesLine: held ? 'Karelia becomes Soviet ground and the war ends.' : 'The war ends where the armies stand.',
    row: held ? 'Offers peace for Karelia.' : 'Offers peace on the frontier as it stands.',
    cede: cedeNames, back: backNames, since: g.flags['wnt:offer'], key,
  };
}

/**
 * True when the card should come up, at the month's start: the offer is new, or its terms have changed. The first
 * reading of a month is its only one, card or none: terms that change in the middle of a month wait for the next
 * (the row under Powers has them at once). Marks it seen.
 */
export function winterCardDue(g: Game): boolean {
  const wp = winterPeace(g);
  if (!wp || g.flags['wnt:cardAt'] === g.turn) return false;
  g.flags['wnt:cardAt'] = g.turn;
  if (g.flags['wnt:card'] === wp.key) return false;
  g.flags['wnt:card'] = wp.key;
  return true;
}

/** Why the player cannot take the peace now, or null. */
export function whyNotWinterPeace(g: Game, up: boolean): string | null {
  if (!winterPeace(g)) return 'There is no offer of peace.';
  if (!up) return 'Orders are given in your own month.';
  return null;
}

/** The player takes the peace on this month's terms. */
export function takeWinterPeace(g: Game, up: boolean): boolean {
  return whyNotWinterPeace(g, up) === null && makeWinterPeace(g, false);
}
