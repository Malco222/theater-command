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
//
//   INDOCHINA (ruling 199(e); the planner's paper 07). Japan may demand Indochina of Vichy France while Vichy holds
//     Saigon and the two are at peace: all the ground Vichy holds that joins Saigon by land passes to Japan with no
//     war, in the form of an ultimatum (diplomacy.json 'bessarabia'), and the French garrison stands down. The
//     computer's Japan makes it in July 1941 while the Pacific's book stands; where no book dates it, in the first of
//     two months, the month after Germany and the Soviet Union go to war or the thirteenth after France fell, and
//     never once Germany is beaten. At stops 4 and 5 (the sandbox playtest of 19 November) no clock brings it: it is
//     the staff's own choice, drawn in each month it may be made by the staff's character ('demand'.'choose'), and
//     likelier while Germany and the Soviet Union are at war. A declaration of war on Vichy France while it holds
//     Saigon is answered by the handing-over (game.ts declareWar): a player's Japan makes its demand so.
//
//   A FALLEN COUNTRY'S FAR GROUND (ruling 199(b), (c)). The keepers of far ground are Britain, then Australia, then
//     America (world.ts protectorOf). The fallen country's corps on ground that passes into keeping, and its
//     squadrons and aircraft based there, stay at their posts and answer to the keeper (game.ts capitulate): the
//     Indies' garrison and the Dutch squadron fight on under British colours with their own names. Ground a fallen
//     keeper only kept passes on by the same list.

//
//   THE PACIFIC'S SHORES (ruling 200, finding 2 and clause (i); ruling 202(e); the planner's paper 14, section 10,
//     places 1 and 2). The book's row for every other shore is the second front's, and it reads no ground in Asia
//     and the Pacific: there the Pacific's own rows decide, under the Pacific's theatre (far-pacific-war.json,
//     'history'.'hops'). While the Pacific's book stands at a stop that keeps one, the staff's second front is not
//     opened on the Pacific's ground at all (history.ts shoreOpen), and a computer staff lands of its own accord
//     (ai.ts hop) only on its side's rows whose month has come: Japan's side on the places of the record's opening,
//     in the record's order; America's and Britain's on their roads back, each place from its earliest month, and
//     on nothing else that history's Japan held. Ground Japan's side takes that the record never gave it (the line
//     in the south names every province it did: line.ts) is open to them from the month it is lost. The great
//     choice's named landings are not bound by the rows. With the book closed, or at a stop that keeps none, every
//     enemy shore of the Pacific's ground is open to the same routine, and the staff ranks them itself.

import data from '../../data/campaign/pacific.json';
import { FAR_STAGE } from './stage';
import type { Game, Nation, Region } from './types';
import { addCorps, allied, atWar, bloc, declareWar, headline, joinedSide, newsName, removeCorps, sameSide, setWar, theName } from './game';
import { drawMen } from './manpower';
import { numbersUnderFlag } from './armies';
import { FAR_FIRST, WORLD, WORLD_FIRST, WORLD_MAP, farOn, shoreOf, standWithAllies } from './world';
import { station, stationBars, stationed, stationShut, unstation, whyStationBars } from './far';
import { HISTORY, bookDate, onBook, stopDef } from './history';
import { characterOf, monthly } from './staff';
import { neverHis } from './line';
import { ownPath } from './league';
import openData from '../../data/campaign/open.json';

/** A country's shipping in the Pacific (ruling 190; the Pacific planner's paper 05; pacific.json, 'runs'). */
export interface RunRules {
  /** Whose shipping it is, and the share of its sea lift that works the Pacific each month, all runs together. */
  of: string; share: number;
  /** The equator on the chart (a province's y): a harbour below it is the southern run's, one above it the central run's. */
  equator: number;
  /** The run that has the whole share off the book while no beach is named on either road. */
  idle: string;
  /** The special formation (traits.json, 'unit') that sails on a Pacific run before any other corps. */
  first?: string;
  /** The runs' own quays at home, the first in the country's hands with a way open. */
  from: string[];
  /** The lane: countries on whose home ground a run may put an army down, and harbours named for it besides. */
  lane: string[]; havens: string[];
  /** Each run: the side of the equator it ends on; its share from each month on while the book stands; its share off the book while both roads have a beach named. */
  roads: Record<string, { south: boolean; book: [number, number][]; both: number }>;
}

/** The readiness test's figures (pacific.json, 'ready'; read by readiness.ts). */
export interface ReadyRules {
  /** The great choice it names the month of, and the flag that choice waits on. */
  choice: string; flag: string;
  /** The first and the last month after the freeze in which the staff may strike, and the chance in each month it is ready. */
  window: [number, number]; chance: number;
  /** The first month by the staff's character; an opportunist's while one of `of` has fought `with` on land of late. */
  first: Record<string, number>;
  busy: { first: number; of: string[]; with: string };
  /** So many months after the freeze the striking fleet's base is moved, as the book's dated row moves it. */
  gather: { after: number; fleet: string; base: string; port: string; home: string; words: string };
  /** The places the opening is for, each with the beaches that lead to it, the nearest the record's first. */
  leads: Record<string, string[]>;
  /** At stops 4 and 5: by the staff's character, the chance it strikes a country that has only stopped its trade (inTheWay). */
  flank?: Record<string, number>;
}

interface Rules {
  power: string; bloc: 'axis' | 'allies'; china: string;
  pair: [string, string];
  with: Record<string, string[]>;
  pact: { with: string; book: number; fallen: string; months: number };
  freeze: { book: number; places: string[]; wars: string[]; seller: string; south: string[] };
  north: { nat: string; fallen: string; months: number; book: number; while: string[] };
  /** The Soviet Far East's divisions that go west: `westMonth`. */
  west?: WestRules;
  demand: {
    of: string; key: string; theatre: string; book: number; war: [string, string]; fallen: string; months: number; never: string;
    /** At stops 4 and 5: the staff's chance each month by its character, times 'busy' while `war` is on (demandMonth). */
    choose?: Record<string, number> & { busy: number };
    /**
     * Japan's own troops under the agreement, who come with the demand: [place, divisions' worth, the row's name], at
     * experience `xp` (demand). A fourth word, `from`, names the corps on the board a post is made from.
     */
    posts?: [place: string, worth: number, name: string, from?: string[]][]; xp?: number;
    /** The month of the record's coup, the kept French army's end at the book's stops (coupMonth). */
    coup?: number;
  };
  /** America's shipping in the Pacific (ruling 190): `pacificRuns`. */
  runs?: RunRules;
  /** The readiness test (ruling 200(m)): the month the computer's Japan strikes where no book dates it. readiness.ts. */
  ready?: ReadyRules;
  keepers: string[];
  africa?: [number, number];
  shores?: { slice: number; seas: string[] };
  skies?: { per: number; cover?: Record<string, number>; seas: string[] };
  /** The Allies' landing ships in the Pacific by the record (rider b614 to ruling 234): `shoreLift`. */
  lift?: LiftRules;
  /** The month the power draws its ring by the book (paper 14, section 3; rider b599 (iii) to ruling 234): `ringDrawn`. */
  ring?: { from: number };
  says: Record<string, string>;
}

/**
 * The Soviet Far East's divisions that go west (the design director's rulings of 19:45Z and 20:03Z, 22 November
 * 2026; the historian's far-east-west-and-us-divisions-dec41.md, section (c)). `westMonth`.
 */
interface WestRules {
  nat: string; foe: string; while: string[];
  /** Rows of the far chart that leave it in a month because the old chart's own book raises the same men elsewhere: [month, the row's name, the line a player of that country reads]. */
  leaves: [number, string, string][];
  /**
   * The waves. 'cause': the month counted from ("foe": the country's war with its foe begins; "freeze": the freeze;
   * "war": the power's war with one of 'while' begins). 'after': so many months after the cause; 'book': the month
   * at the stops that keep the book. 'needs' "foe": only while the country is at war with its foe. 'rows': [the
   * held row's name, how many of its divisions the wave frees, the record's name of the formation it gives up].
   */
  waves: { cause: 'foe' | 'freeze' | 'war'; after: number; book: number; needs?: 'foe'; rows: [string, number, string][] }[];
}

interface LiftRules {
  /** The last month the count is read; after it the ordinary sea lift binds alone. */
  until: number;
  /** By country: from the month of a line to the country's next line, so many divisions' worth may be put ashore. */
  by: Record<string, [number, number][]>;
  /** A country's own sea lift by the record (rider b630): from the month of a line its shipping puts that many forces to sea in a month. The first line is its figure as the game opens. `liftMonth`. */
  own?: Record<string, [number, number][]>;
}

/** The Pacific's rules. None while the far side's switch is off. */
export const PACIFIC: Rules | null = FAR_STAGE ? (data as unknown as Rules) : null;

const FREEZE = 'pac:freeze', PACT = 'pac:pact';

/** True where the ties read: the grand campaign on the whole world, before its last act. */
const reads = (g: Game): boolean => PACIFIC !== null && g.flags.open === 1 && !((g.flags.dom ?? 0) > 0) && farOn(g);
const live = (g: Game, id: string): Nation | null => { const n = g.nations[id]; return n?.alive && !n.dormant ? n : null; };

/**
 * The month before which a war one great power owes another does not open, or 0 where no book names one (the design
 * director's 07:18Z of 22 November 2026). Two rules name the Soviet Union's war with Japan: the tie below (three
 * months after Germany is beaten, and at the stops that keep the book not before the book's month) and the finish
 * rule (finish.ts, rulings 111 and 115: notice the month the chief enemy falls, war so many months after, at every
 * stop). The finish rule asked no stop, so the tie's book month was never heard once Germany had fallen: read by
 * the book on twelve wars, the Soviet war with Japan opened in months 58 to 68 in eleven of them, three months
 * after a Germany that fell early, where the record has August 1945, and Mongolia's cavalry rode north China a
 * year before its time. At the stops that keep the book the war now comes at the later of the two: the record held both
 * (Yalta tied the entry to Germany's defeat, and it came on 9 August 1945). The notice is given the month Germany
 * falls, as it was. At the other stops the finish rule's months alone, as they were. Behind the far switch, and
 * no new number: the month is the tie's own (`north.book`).
 */
export const owedNotBefore = (g: Game, who: string, on: string): number =>
  PACIFIC !== null && reads(g) && !!stopDef(g).book && who === PACIFIC.north.nat && on === PACIFIC.power ? PACIFIC.north.book : 0;
const fill = (line: string, words: Record<string, string>): string => line.replace(/\{(\w+)\}/g, (m, k: string) => words[k] ?? m);
const listed = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);

/** The month the freeze came, or 0 while America still sells. */
export const freezeAt = (g: Game): number => (reads(g) ? g.flags[FREEZE] ?? 0 : 0);
/** The readiness test's figures where the Pacific's ties read, or null. */
export const readyRules = (g: Game): ReadyRules | null => (reads(g) ? PACIFIC!.ready ?? null : null);
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

/**
 * True while Japan's fleet is not yet mobilised, and burns its peacetime allowance of oil (resources.ts
 * `navyAllowance`; the historian's paper sub-patrols-japan-oil.md, 6c, laid as found by the design director's 13:43Z
 * of 22 November 2026: the allowance to August 1941, a war's need from September). The clock is the freeze's, Japan's
 * own: through the month America's sales stop, and from the month after the fleet burns what a war asks. By the book
 * that is August 1941; where no book dates the freeze its own cause brings it. And never once Japan is at war with
 * the freeze's seller or with the south's sellers, whatever the freeze. False for every other nation, and where the
 * ties do not read.
 */
export function fleetAtPeace(g: Game, nat: string): boolean {
  const P = PACIFIC;
  if (!P || !reads(g) || nat !== P.power) return false;
  const at = g.flags[FREEZE] ?? 0;
  if (at > 0 && g.turn > at) return false;
  return ![P.freeze.seller, ...P.freeze.wars, ...P.freeze.south].some((e) => atWar(g, nat, e));
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
 * The Soviet Far East's divisions that go west (the design director's rulings of 19:45Z and 20:03Z, 22 November
 * 2026). The far chart's rows there are all held, and nothing read what Japan did: the record sent about thirteen
 * divisions west in 1941 after Germany's attack and three more in July 1942, once Japan had turned south.
 *
 * A wave comes due so many months after its cause (at the stops that keep the book, in the book's month) and is
 * taken in the first month from then that finds the country at peace with the power, and at war with its foe where
 * the wave asks that. The power at war with the country frees nothing, and no wave to come is taken while that war
 * lasts. A wave lifts the hold on so many divisions of a row and no more: a row that gives up all it has loses its
 * hold; one that gives up part gives up a formation of that many, in the same place and of the row's own quality,
 * free of the hold, and the rest stays held. Nothing is moved: the staff's own rules do the rest. A player of that
 * country is told what the Far East can spare, and his formations are left as they stand (ruling 129(d)).
 *
 * 'leaves' is the other half of the same count: men the old chart's own book raises in the west (the 16th Army at
 * Kiev in May 1941) leave the far chart's row they came from in that month, so nobody is counted twice.
 */
function westMonth(g: Game): void {
  const P = PACIFIC!, W = P.west;
  if (!W) return;
  const S = live(g, W.nat);
  if (!S) return;
  W.leaves.forEach(([month, name, words], i) => {
    const key = `pac:west:left:${i}`;
    if (g.flags[key] !== undefined || g.turn < month) return;
    g.flags[key] = g.turn;
    // (A campaign begun too late for the book's own row to form keeps the far chart's.)
    if (g.turn > month + 6) return;
    const c = g.corps.find((x) => x.nat === W.nat && !x.dead && x.name === name);
    if (!c) return;
    removeCorps(g, c);
    if (W.nat === g.human) headline(g, 'info', words, W.nat);
  });
  const J = live(g, P.power);
  if (!J || atWar(g, W.nat, J.id)) return;
  const since = (a: string, b: string): number => (atWar(g, a, b) ? Math.max(g.flags[`war:${a}:${b}`] ?? 0, g.flags[`war:${b}:${a}`] ?? 0) : 0);
  const foe = since(W.nat, W.foe);
  const wars = W.while.map((e) => since(J.id, e)).filter((t) => t > 0);
  const cause = { foe, freeze: g.flags[FREEZE] ?? 0, war: wars.length ? Math.min(...wars) : 0 };
  W.waves.forEach((w, i) => {
    const key = `pac:west:${i}`;
    if (g.flags[key] !== undefined || cause[w.cause] <= 0 || (w.needs === 'foe' && foe <= 0)) return;
    if (g.turn < (stopDef(g).book ? w.book : cause[w.cause] + w.after)) return;
    g.flags[key] = g.turn;
    const spared: string[] = [];
    let n = 0;
    for (const [name, divs, record] of w.rows) {
      const c = g.corps.find((x) => x.nat === W.nat && !x.dead && x.hold && x.name === name);
      if (!c) continue;
      const k = Math.min(divs, c.divs);
      n += k;
      spared.push(`the ${record} at ${g.regions[c.reg].name}`);
      if (W.nat === g.human) continue;
      if (k >= c.divs) { c.hold = false; continue; }
      c.divs -= k;
      const free = addCorps(g, W.nat, record, c.kind, k, c.reg, { xp: c.xp });
      free.str = c.str; free.org = c.org; free.ap = c.ap; free.apMax = c.apMax; free.supplied = c.supplied;
      if (c.full !== undefined) free.full = c.full;
    }
    if (n > 0 && W.nat === g.human) headline(g, 'event', fill(P.says.west, { n: String(n), divisions: n === 1 ? 'division' : 'divisions', list: spared.length > 1 ? `${spared.slice(0, -1).join(', ')} and ${spared[spared.length - 1]}` : spared[0] }), W.nat);
  });
}

/**
 * A country's sea lift by the record (rider b630 to ruling 234; the historian's historian/sea-lift-record-b630.md):
 * the most times its rows put to sea in a month of the record, from the month the shipping for it was gathered.
 * Japan's is 4 on the China coast and 11 from October 1941, when the southern operation's shipping was held: the
 * ten forces of December 1941 in eleven sailings. Counted as the game counts its lift, one for each time a row puts
 * to sea. A fact of the day, like the landing ships: by the date, at every stop of the dial, for player and computer
 * alike, wherever the far side's switch is on, in the grand campaign's last act too. What the country's yards have
 * added since stays added.
 */
export function liftMonth(g: Game): void {
  if (!PACIFIC || !farOn(g)) return;
  for (const [nat, rows] of Object.entries(PACIFIC.lift?.own ?? {})) {
    const n = g.nations[nat];
    if (!n) continue;
    for (let i = 1; i < rows.length; i++) {
      const key = `lift:own:${nat}:${i}`;
      if (g.turn < rows[i][0] || g.flags[key] !== undefined) continue;
      n.sealift = Math.max(0, n.sealift + rows[i][1] - rows[i - 1][1]);
      g.flags[key] = g.turn;
    }
  }
}
/**
 * True for a country whose sea lift is the record's own count (`liftMonth`): its landing craft are in that count
 * already, so a technology adds no shipping to it (rider b630: Japan's Daihatsu craft and the Shinshu Maru were in
 * service from 1937).
 */
export const liftByRecord = (g: Game, nat: string): boolean => PACIFIC !== null && farOn(g) && !!PACIFIC.lift?.own?.[nat];

/**
 * The month's opening, before history's events are read: the month a pact begins to bind, the freeze, Britain and
 * America with each other, and the Soviet staff's month.
 */
export function pacificOpening(g: Game): void {
  const P = PACIFIC;
  liftMonth(g);
  if (!P || !reads(g)) return;
  const J = live(g, P.power);
  if (!J) return;
  if (g.flags[PACT] === undefined) {
    const F = g.nations[P.pact.fallen];
    const at = stopDef(g).book ? P.pact.book : F && !F.alive && F.fellOn ? F.fellOn.turn + P.pact.months : null;
    if (at !== null && g.turn >= at) g.flags[PACT] = at;
  }
  freezeMonth(g);
  westMonth(g);
  // (The demand on Vichy France is made once the freeze has been read: where no date brings the freeze, it follows the demand a month on.)
  demandMonth(g);
  coupMonth(g);
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
    headline(g, 'event', fill(P.says.north, { fallen: theName(G), nat: theName(S), power: theName(J) }), N.nat);
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

// ---- Indochina: the demand on Vichy France (ruling 199(e)) ------------------------------------------------

/** The ground a demand would bring: all that Vichy France holds, its own, that joins the key place by land. Empty where none can be made. */
function demanded(g: Game): Region[] {
  const P = PACIFIC;
  if (!P || !reads(g)) return [];
  const D = P.demand, J = live(g, P.power), V = live(g, D.of), key = g.byName[D.key];
  const his = (r: Region): boolean => r.owner === D.of && r.ctrl === D.of;
  if (!J || !V || !key || !his(key) || atWar(g, J.id, D.of)) return [];
  const seen = new Set<number>([key.id]), out = [key];
  for (let i = 0; i < out.length; i++) for (const j of out[i].nb) {
    const r = g.regions[j];
    if (!seen.has(j) && his(r)) { seen.add(j); out.push(r); }
  }
  return out;
}

/** True where Japan may demand Indochina of Vichy France: Vichy holds Saigon and the two are at peace. */
export const mayDemand = (g: Game): boolean => demanded(g).length > 0;

/**
 * The demand: Japan's troops are stationed in the colony with no war. Japan is its holder from that day; the ground
 * stays Vichy France's and carries the agreement's mark (far.ts `station`; the design director's ruling of 21:08Z of
 * 21 November 2026, form 2): a country at war with Japan and at peace with Vichy France forms no front over it and
 * orders no ground attack or landing into it. As first built the ground became Japan's own, and Britain and China,
 * at peace with Vichy throughout, took Hanoi and Laos from it in 9 wars of 12.
 *
 * The French army stays at its posts all over the colony, as
 * it did until 9 March 1945 (the historian's japan-taken-towns-1942.md, section 7): every corps of Vichy's on the
 * ground answers to Japan from that day as a fallen country's corps answers to its keeper (ruling 199(b),
 * `keptForces`), marked as raised by Vichy, and holds and stays where it stands: it defends its post against any
 * attacker and never marches or attacks. Japan's own troops under the agreement come with it, to the posts the
 * record gives them in Tonkin ('posts'). Vichy's aircraft and ships based there stand down. True if it was made.
 *
 * (The design director's rulings of 16:44Z and 16:46Z of 21 November 2026, mending his own added line to 199(e),
 * "the French garrison lays down its arms". With the garrison gone and Japan's one division held at Phnom Penh,
 * Tonkin stood empty the day of the demand and China's army walked into Hanoi the month after in 12 wars of 12; in
 * the record no Chinese force crossed into Tonkin before 1945. At the book's stops the troops are dated rows and
 * not paid for; where the demand comes by cause their weight is drawn from Japan's reserve of men.)
 */
export function demand(g: Game): boolean {
  const ground = demanded(g);
  if (!ground.length) return false;
  const P = PACIFIC!, D = P.demand, J = g.nations[P.power], V = g.nations[D.of];
  const ids = new Set(ground.map((r) => r.id));
  // (Under its new flag no division of it reads by a number that is another's name there: armies.ts numbersUnderFlag.)
  for (const c of g.corps) if (c.nat === D.of && !c.dead && ids.has(c.reg)) { c.raised ??= D.of; c.nat = J.id; c.hold = true; c.stay = true; numbersUnderFlag(g, c); }
  g.airFleets = g.airFleets.filter((f) => !(f.nat === D.of && ids.has(f.base)));
  g.fleets = g.fleets.filter((f) => !(f.nat === D.of && ids.has(f.base)));
  for (const r of ground) { r.ctrl = J.id; r.cap = false; r.since = g.turn; station(g, r, J.id); }
  for (const [place, worth, name, from] of D.posts ?? []) {
    const r = g.byName[place];
    if (!r || !ids.has(r.id) || !(worth > 0)) continue;
    // (The far side: a post made from corps on the board, `from`, as a formations row is since Oahu's split
    // (game.ts startPhase): the design director's ruling of 08:48Z of 22 November 2026, on Canton's rows. The post
    // takes its strength out of the corps it names, in their order, wherever they stand, and never more than they
    // hold; a corps emptied is struck off; with none of them left the post is not made. Its men are the corps' own:
    // nothing is drawn for it from the reserve of men, and nobody is counted twice. Every country's alike.)
    const made = farOn(g) && Array.isArray(from) ? from.flatMap((nm) => g.corps.filter((x) => x.nat === J.id && !x.dead && x.name === nm)) : null;
    if (made && !made.length) continue;
    if (!made && !stopDef(g).book) drawMen(g, J.id, worth);
    const c = addCorps(g, J.id, name, 'inf', 1, r.id, { xp: D.xp ?? 0.3, est: worth, hold: 1, stay: 1 });
    c.str = worth;
    if (made) {
      const want = c.divs * c.str;
      let got = 0;
      for (const x of made) {
        const has = x.divs * x.str, take = Math.min(has, want - got);
        if (take <= 0) continue;
        got += take;
        if (has - take < 1e-9) removeCorps(g, x); else x.str = (has - take) / x.divs;
      }
      c.str = got / c.divs;
    }
  }
  g.flags['pac:demand'] = g.turn;
  g.supplyDirty = true;
  headline(g, 'event', fill(P.says.demand, { Power: theName(J, true), power: theName(J), of: theName(V), Of: theName(V, true), key: D.key }), J.id, g.byName[D.key].id);
  return true;
}

/**
 * True for a corps of Vichy's that stayed at its post in Indochina on the demand and answers to Japan. The record's
 * French army kept order in the colony and was never Japan's to march: so for this demand hold and stay binds every
 * keeper, a player too. He gives such a corps no orders (turn.ts canOrder), and nobody folds it into another
 * (fold.ts). The design director's 19:40Z of 21 November 2026, point (f). No corps is such with the far side off.
 */
export const atItsPost = (c: { nat: string; raised?: string }): boolean => PACIFIC !== null && c.raised === PACIFIC.demand.of && c.nat === PACIFIC.power;
/**
 * Why such a corps takes no orders, in the UI director's words (19:48Z of 21 November 2026,
 * ui-director/words-far-guards-indochina.md): `post` for an order refused, `card` for the line under its name on its
 * card, where a corps shows its trouble. Null for every other corps.
 */
export const postWords = (c: { nat: string; raised?: string }, where: 'post' | 'card'): string | null => (atItsPost(c) ? PACIFIC!.says[where] ?? null : null);

/**
 * The words on the card of a province of the colony while it is held by agreement, in the UI director's words (20:58Z
 * of 21 November 2026 and the rider of 21:27Z, ui-director/words-far-guards-indochina.md): `all`, one line under the
 * place's name, and `own`, a second line for a country the agreement shuts out (told to make war on the owner only
 * where it can: far.ts `stationShut`) and for nobody else. Null for any other ground.
 */
export function agreedWords(g: Game, r: Region, reader: string | null): { all: string; own: string | null } | null {
  const P = PACIFIC;
  if (!P || !stationed(g, r) || r.ctrl !== P.power) return null;
  const O = g.nations[r.owner], H = g.nations[r.ctrl];
  const w = { owner: theName(O), Owner: theName(O, true), holder: theName(H), Holder: theName(H, true) };
  const line = (key: string): string | null => (P.says[key] ? fill(P.says[key], w) : null);
  const all = line('agreed');
  if (!all) return null;
  // (The holder's own player is "you" in the one line, and has no second: the rider of 21:27Z.)
  if (reader === r.ctrl) return { all: line('agreedHolder') ?? all, own: null };
  const bound = !!reader && stationBars(g, reader, r);
  return { all, own: bound ? line(stationShut(g, reader!, r) ? 'agreedShut' : 'agreedBound') : null };
}

/**
 * True for ground the book keeps under the agreement whatever becomes of its owner: at stops 1 and 2 the mark holds
 * to the coup. The record's colony stayed under Decoux to 9 March 1945, though France was freed in August 1944
 * (finish.ts `endVichy` leaves such ground Vichy's; the design director's 21:14Z of 21 November 2026, on reading 4).
 */
export const bookKeepsAgreement = (g: Game, r: Region): boolean => PACIFIC !== null && !!stopDef(g).book && stationed(g, r);

/** A declaration of war on Vichy France while it holds Saigon is answered by the handing-over, and there is no war (game.ts declareWar). */
export function demandInstead(g: Game, a: string, b: string): boolean {
  const P = PACIFIC;
  return !!P && a === P.power && b === P.demand.of && demand(g);
}

/** The month the computer's Japan makes its demand. */
function demandMonth(g: Game): void {
  const P = PACIFIC!, D = P.demand;
  if (P.power === g.human || g.flags['pac:demand'] !== undefined || !mayDemand(g)) return;
  let due: boolean;
  if (onBook(g, D.theatre)) due = g.turn >= D.book;
  else if (stopDef(g).ambitions && D.choose) {
    // (At stops 4 and 5 it is the staff's choice: drawn each month it may be made, by its character, and likelier
    // while the Soviet Union is held by Germany and the north is safe. Never once Germany is beaten.)
    const C = D.choose, busy = atWar(g, D.war[0], D.war[1]) ? C.busy : 1;
    due = !!g.nations[D.never]?.alive && monthly(g, P.power, 'demand', g.turn) < (C[characterOf(g, P.power)] ?? 0) * busy;
  } else {
    const never = g.nations[D.never], F = g.nations[D.fallen];
    const east = Math.max(g.flags[`war:${D.war[0]}:${D.war[1]}`] ?? 0, g.flags[`war:${D.war[1]}:${D.war[0]}`] ?? 0);
    due = !!never?.alive && ((east > 0 && g.turn > east) || (!!F && !F.alive && !!F.fellOn && g.turn >= F.fellOn.turn + D.months));
  }
  if (due) demand(g);
}

/**
 * The kept French army's end at the book's stops: the record's coup of 9 March 1945 ('coup'), when Japan struck
 * and broke it in about two days. The corps Vichy raised that answer to Japan are struck off that month. At stops
 * 3 to 5 they hold to the war's end for now: how a kept colony's army ends by cause is after release (the design
 * director's 16:44Z of 21 November 2026). A computer's Japan alone: no coup comes for a player's Japan for now, and
 * the kept corps stay at their posts, his to command no more than the computer's (`atItsPost`).
 *
 * The agreement ends with them: the mark is struck from the colony's ground and what Japan still holds of it is
 * Japan's own from that month, as all of it was from the demand before the ruling of 21:08Z of 21 November 2026.
 */
function coupMonth(g: Game): void {
  const P = PACIFIC!, D = P.demand;
  if (!D.coup || g.flags['pac:demand'] === undefined || g.flags['pac:coup'] !== undefined || !stopDef(g).book || g.turn < D.coup || P.power === g.human) return;
  const J = live(g, P.power);
  if (!J) return;
  g.flags['pac:coup'] = g.turn;
  for (const r of g.regions) {
    // (Ground Japan lost keeps its owner, and so does ground whose owner is at war with Japan: the mark alone goes.)
    const his = r.ctrl === J.id && stationed(g, r);
    if (unstation(g, r, J.id) && his) r.owner = J.id;
  }
  g.supplyDirty = true;
  const kept = g.corps.filter((c) => !c.dead && c.nat === J.id && c.raised === D.of);
  if (!kept.length) return;
  for (const c of kept) removeCorps(g, c);
  if (P.says.coup) headline(g, 'event', fill(P.says.coup, { Power: theName(J, true), power: theName(J) }), J.id, g.byName[D.key]?.id);
}

// ---- Who stands in Japan's way (stops 4 and 5) --------------------------------------------------------------

/** What inTheWay reads of a great choice: whom it declares on, and the ground its landings and strikes go for. */
interface WayOp { id: string; declare?: string[]; landings?: { to: string[] }[]; strikes?: { harbour: string }[] }

/**
 * The countries the strike south declares on at stops 4 and 5 (the sandbox playtest of 19 November): not history's
 * list but those that stand in Japan's way the month it goes. Of the countries the choice names, living, outside
 * Japan's side and not yet at war with it:
 *   - one that fights a power of Japan's bloc stands in its way;
 *   - so does one bound to one of those: its dominions and those the ties name with it while they stand beside it
 *     (at war the same day, followers), and Britain or America while the two stand in one alliance (at war the month
 *     after: pacificOpening);
 *   - one that has only stopped Japan's trade (the freeze) and holds ground the strike goes for is the flank: the
 *     staff strikes it, or hopes to leave it out, by the chance its character gives, drawn once a war.
 * Nobody is in the way unless one of them holds ground the strike goes for. Null where history's list stands: at
 * stops 1 to 3, for every other choice, and where the ties do not read.
 */
export function inTheWay(g: Game, nat: string, op: WayOp): string[] | null {
  const P = PACIFIC, R = P?.ready;
  if (!P || !R || !reads(g) || !stopDef(g).ambitions || nat !== P.power || op.id !== R.choice) return null;
  const road = new Set([...(op.landings ?? []).flatMap((L) => L.to), ...(op.strikes ?? []).map((s) => s.harbour), ...Object.keys(R.leads)]);
  const onRoad = (id: string): boolean => [...road].some((nm) => g.byName[nm]?.ctrl === id);
  const open = (op.declare ?? []).filter((id) => !!live(g, id) && !allied(g, nat, id) && !atWar(g, nat, id));
  const side = (id: string): string | null => { const b = bloc(g.nations[id]); return b && b !== 'league' ? b : null; };
  const fights = (id: string): boolean => g.nations[id].atWar.some((e) => { const E = live(g, e); return !!E && bloc(E) === P.bloc; });
  const cut = (id: string): boolean => freezeAt(g) > 0 && (id === P.freeze.seller || P.freeze.south.includes(id));
  const who = characterOf(g, nat);
  const out = new Set(open.filter((id) => fights(id) || (cut(id) && onRoad(id) && monthly(g, nat, 'flank', id) < (R.flank?.[who] ?? 1))));
  // (As the ties read them: a dominion or a country named with one in the way only while it stands beside it, and not
  // beside Japan, as followers() asks; Britain and America only while the two stand in one alliance.)
  const bound = (id: string): boolean => [...out].some((o) => (((P.with[o] ?? []).includes(id) || WORLD.follow[id] === o) && sameSide(g, id, o) && !sameSide(g, id, nat))
    || (P.pair.includes(id) && P.pair.includes(o) && side(o) !== null && side(o) === side(id)));
  for (let grew = true; grew;) {
    grew = false;
    for (const id of open) if (!out.has(id) && bound(id)) { out.add(id); grew = true; }
  }
  return [...out].some(onRoad) ? open.filter((id) => out.has(id)) : [];
}

// ---- A fallen country's far ground (ruling 199(b), (c)) ---------------------------------------------------

/**
 * The keepers of ground on the far side of the world, in their order; null for ground anywhere else, which keeps the
 * world's own list. Ruling 199's list is Asia's and the Pacific's: Africa's far ground (pacific.json 'africa', the
 * provinces from the first to the last counted from the far side's first) keeps the world's own order, Britain and
 * then America (ruling 234(e)1). What is kept there, and how, is the same.
 */
export function keepersOf(g: Game, r: Region): string[] | null {
  if (PACIFIC === null || !reads(g) || shoreOf(r) !== 2) return null;
  const A = PACIFIC.africa, id = provinceOf(r.id) - FAR_FIRST;
  return A && id >= A[0] && id <= A[1] ? WORLD.protector : PACIFIC.keepers;
}

/** The first keeper of this ground still fighting the victor, as protectorOf reads its list (world.ts). */
function keeperAt(g: Game, by: string, r: Region): string | null {
  for (const id of keepersOf(g, r) ?? []) {
    const n = g.nations[id];
    if (n?.alive && n.seaSupply && n.atWar.includes(by)) return id;
  }
  return null;
}

/**
 * As a country falls (game.ts capitulate, before its forces are struck off): its corps on far ground that is about
 * to pass into keeping, and its squadrons and aircraft based on such ground, stay at their posts and answer to the
 * keeper. A corps keeps its post (a player who is the keeper commands it as his own). Returns the keeper of each
 * province so kept. `leave` names ground that passes to someone else by the fall's own terms (Vichy's, under the
 * armistice): nothing there is kept.
 */
export function keptForces(g: Game, nat: string, by: string, leave?: (r: Region) => boolean): Map<number, string> {
  const kept = new Map<number, string>();
  if (!PACIFIC || !reads(g)) return kept;
  // (Ground an ally's troops stand on is that ally's, as built: nothing passes into keeping there.)
  const held = new Set<number>();
  for (const u of g.corps) if (!u.dead && u.nat !== nat && atWar(g, u.nat, by)) held.add(u.reg);
  for (const r of g.regions) {
    if (r.owner !== nat || r.ctrl !== nat || !r.overseas || shoreOf(r) !== 2 || held.has(r.id) || leave?.(r)) continue;
    const keeper = keeperAt(g, by, r);
    if (keeper) kept.set(r.id, keeper);
  }
  if (!kept.size) return kept;
  // (A kept corps is still its own army's: by the book it fights at that army's figure. Rider b626 to ruling 234.)
  // (Nor does a division of it read by a number that is another's name under the keeper's flag: armies.ts numbersUnderFlag.)
  for (const c of g.corps) if (c.nat === nat && !c.dead && kept.has(c.reg)) { c.raised ??= nat; c.nat = kept.get(c.reg)!; c.hold = true; numbersUnderFlag(g, c); }
  for (const f of g.fleets) if (f.nat === nat && kept.has(f.base)) f.nat = kept.get(f.base)!;
  // (A kept air group still reads its own country's row for what it is kept at and how it flies: air.ts rowOf.)
  for (const f of g.airFleets) if (f.nat === nat && kept.has(f.base)) { f.raised ??= nat; f.nat = kept.get(f.base)!; }
  return kept;
}

// ---- The air over a Pacific sea (rulings 184 and 187) -------------------------------------------------------

const skySeas = new WeakMap<object, Uint8Array>();

/**
 * True in a campaign where the air over the Pacific's seas is counted from the groups in reach: the grand campaign
 * on a chart that reaches the far side. (In the last act of a campaign of domination too: how the air over a sea is
 * counted is no tie between countries.)
 */
export const skiesOn = (g: Game): boolean => !!PACIFIC?.skies && g.flags.open === 1 && farOn(g);

/**
 * The rule for the air over this sea where it is counted from the land air groups in reach (pacific.json 'skies';
 * air.ts groupsOver), or null where the coasts decide as they always have: every sea of the old chart, the western
 * Indian Ocean, and every sea while the far side's switch is off, outside the grand campaign, or on a chart that
 * does not reach the far side.
 */
export function skiesOver(g: Game, z: number): { per: number; cover?: Record<string, number> } | null {
  const S = PACIFIC?.skies;
  if (!S || !skiesOn(g)) return null;
  let on = skySeas.get(g.seas);
  if (!on) {
    on = new Uint8Array(g.seas.length);
    const keys = new Set(S.seas);
    for (const zone of g.seas) if (keys.has(zone.key)) on[zone.id] = 1;
    skySeas.set(g.seas, on);
  }
  return on[z] ? S : null;
}

// ---- The Pacific's shores ---------------------------------------------------------------------------------

/** How many of the Pacific's shores a staff names at once for its fleet and its fighters (ai.ts hop keeps them in the flags `st:hop:` and `st:hopQ:`, the beach and the quay; navy.ts and air.ts read them). */
export const HOP_AIMS = 6;

/** The crossings a staff has named for its hops this month: the quay and the beach of each. */
export function hopAims(g: Game, nat: string): { from: Region; to: Region }[] {
  const out: { from: Region; to: Region }[] = [];
  if (!PACIFIC) return out;
  for (let i = 0; i < HOP_AIMS; i++) {
    const to = g.regions[(g.flags[`st:hop:${nat}:${i}`] ?? 0) - 1], from = g.regions[(g.flags[`st:hopQ:${nat}:${i}`] ?? 0) - 1];
    if (to && from && atWar(g, nat, to.ctrl) && !stationBars(g, nat, to)) out.push({ from, to });
  }
  return out;
}

/** The Pacific's own shores rows (far-pacific-war.json, 'history'.'hops'): a place and the month it opens, for each side. */
interface HopRows {
  theatre: string; slides?: string; axis: [string, number][]; allies: [string, number][];
  /**
   * Whose road a row of the side against the power is (the planner's paper 14, section 4, points 4 and 8; paper 15):
   * a place under `roads` is that country's and no other staff's; every other row is `lead`'s; a country under
   * `behind` goes behind the lead's army and lands of its own accord only on the places named for it; any other
   * country of that side reads none of the rows. With no `lead`, every staff of the side reads them all.
   */
  lead?: string; roads?: Record<string, string[]>; behind?: Record<string, string[]>;
}
const hopRows = (): HopRows | null => (PACIFIC ? (HISTORY as unknown as { hops?: HopRows }).hops ?? null : null);

/** The province a far-side city was cut out of; a province is its own. */
const provinceOf = (id: number): number => (WORLD_MAP.regions[id - WORLD_FIRST] as { of?: number } | undefined)?.of ?? id;

/**
 * True for the Pacific's ground (pacific.json, 'shores'): the far side's first slice, from Pearl Harbor to Dunedin,
 * with its cities, and any province with a coast on one of the seas named (the Bay of Bengal and Ceylon's waters:
 * ruling 202(h)). Never while the far side is off, nor on a chart that does not reach it.
 */
export function pacificGround(g: Game, r: Region): boolean {
  const S = PACIFIC?.shores;
  if (!S || !farOn(g)) return false;
  const id = provinceOf(r.id);
  return (id >= FAR_FIRST && id < FAR_FIRST + S.slice) || r.seas.some((z) => S.seas.includes(g.seas[z].key));
}

const liftFlag = (nat: string, from: number): string => `shorelift:${nat}:${from}`;
/** What `shoreLift` reads: what is `left` of the stretch's worth `of`, the stretch's first month `from` and the month
 * after its last (`next`), the month more comes or the count ends (`due`), and whether the country has had none yet. */
export interface ShoreLift { left: number; of: number; from: number; next: number; due: number; first: boolean }
/**
 * The landing ships in the Pacific for a country's troops bound for an enemy shore there (rider b614 to ruling 234;
 * the historian's count, historian/allied-lift-pacific-1941-43-b593.md). A second count beside the month's sea lift
 * (navy.ts), read for a landing from the sea on an enemy shore of the Pacific's ground and for nothing else: in each
 * stretch of the record, a half-year mostly, a country puts ashore there no more than the record did, in divisions'
 * worth (size times strength). The record's count is a country's own troops landed on an enemy-held shore, in
 * anyone's ships, since there is no boarding here. It is a fact of the day: it binds player and computer alike, by
 * the date, at every stop of the dial and whenever the war began. A country the rows do not name has none; Japan and
 * those who fight beside it are not counted (Japan's own lift is the record's: `liftMonth`). After the rows' last month the ordinary lift
 * binds alone. Null where the count is not read.
 */
export function shoreLift(g: Game, nat: string, to: Region): ShoreLift | null {
  const L = reads(g) ? PACIFIC!.lift : undefined;
  if (!L || g.turn > L.until || nat === PACIFIC!.power || allied(g, nat, PACIFIC!.power)) return null;
  if (!atWar(g, nat, to.ctrl) || !pacificGround(g, to)) return null;
  let from = 0, of = 0, next = 0, due = 0, first = true;
  for (const [m, w] of L.by[nat] ?? []) {
    if (m <= g.turn) { from = m; of = w; first = first && w <= 0; continue; }
    next ||= m;
    if (w > 0) { due = m; break; }
  }
  return { left: Math.max(0, of - (g.flags[liftFlag(nat, from)] ?? 0) / 1000), of, from, next: next || L.until + 1, due: due || L.until + 1, first };
}

/** A landing on such a shore has sailed: what it loaded is taken off the stretch's count, thrown back or not. */
export function useShoreLift(g: Game, nat: string, to: Region, worth: number): void {
  const S = shoreLift(g, nat, to);
  if (S) g.flags[liftFlag(nat, S.from)] = (g.flags[liftFlag(nat, S.from)] ?? 0) + Math.round(worth * 1000);
}

/** True while the Pacific's rows are what a computer staff's landings on the Pacific's ground go by: a stop that keeps the book, and the Pacific's book open. */
export function hopsBind(g: Game): boolean {
  const H = hopRows();
  return !!H && farOn(g) && !!stopDef(g).book && onBook(g, H.theatre);
}

/** How many months the Pacific's calendar has slid in this war: the draw of the great choice it slides with (a stop with slack). */
function hopSlide(g: Game, H: HopRows): number {
  if (!H.slides) return 0;
  const [nat, op] = H.slides.split(':');
  const date = HISTORY.dates[nat]?.[op], now = bookDate(g, nat, op);
  return date === undefined || now === null ? 0 : now - date;
}

/**
 * Whether the power has drawn its ring (the Pacific planner's paper 14, section 3, "The ring: Japan after the
 * opening"; rider b599 (iii) to ruling 234). While the Pacific's book binds (stops 1 and 2), from the outer line's
 * month by the book ('ring', May 1942), slid with the strike. At the other stops, from the first month its offensive
 * has gone the staffs' own 'stall' months without gaining ground since its war with America began (section 3: "When
 * its offensive has gained nothing for three months, Japan's staff draws its line"); once drawn it stays drawn. From
 * then its carrier striking force and its battle line are kept together (navy.ts, `keptTogether`); before it the
 * opening's own rule stands (section 2: the carriers and the lighter fleets cover the landings, the battle line stays
 * in home waters). Never while the far side is off.
 */
export function ringDrawn(g: Game): boolean {
  const P = PACIFIC, H = hopRows();
  if (!P?.ring || !reads(g)) return false;
  if (H && hopsBind(g)) return g.turn >= P.ring.from + hopSlide(g, H);
  const key = `pac:ring:${P.power}`;
  if (g.flags[key] !== undefined) return true;
  const a = g.flags[`war:${P.power}:${P.pact.with}`], b = g.flags[`war:${P.pact.with}:${P.power}`];
  if (!atWar(g, P.power, P.pact.with) || (a === undefined && b === undefined)) return false;
  const since = Math.max(Math.min(a ?? Infinity, b ?? Infinity), g.flags[`st:gain:${P.power}`] ?? 0);
  if (g.turn - since < (openData as { stall: number }).stall) return false;
  g.flags[key] = g.turn;
  return true;
}

/**
 * The shores of the Pacific's ground a computer staff may land on of its own accord. `open`: this month, in the
 * order it takes them while the book stands (the rows' own); `soon`: those that open next month, named a month
 * ahead so that the fleet is off the beach and the fighters over it in time. `book` says whether the rows bind.
 * Null for the player, for a country in neither camp of the Pacific's war, and wherever the far side is off.
 */
export function hopShores(g: Game, nat: string): { book: boolean; open: Region[]; soon: Region[] } | null {
  const P = PACIFIC, H = hopRows();
  if (!P?.shores || !H || !farOn(g) || nat === g.human || !g.nations[P.power]?.alive) return null;
  const mine = nat === P.power || allied(g, nat, P.power);
  if (!mine && !atWar(g, nat, P.power)) return null;
  // (A shore the enemy's troops stand on by agreement with an owner the staff's country is at peace with is no shore of its: far.ts `stationBars`.)
  const foe = (r: Region): boolean => r.seas.length > 0 && atWar(g, nat, r.ctrl) && !stationBars(g, nat, r);
  if (!hopsBind(g)) return { book: false, open: g.regions.filter((r) => foe(r) && pacificGround(g, r)), soon: [] };
  const side: 'axis' | 'allies' = mine ? P.bloc : P.bloc === 'axis' ? 'allies' : 'axis';
  const d = hopSlide(g, H);
  const open: Region[] = [], soon: Region[] = [];
  const listed = new Set<number>();
  // (Whose road each row is: the side against the power only, and only where the rows name a lead.)
  const roads = !mine && H.lead ? H.roads ?? {} : null;
  const own = roads ? roads[nat] ?? H.behind?.[nat] ?? null : null;
  const others = roads ? new Set(Object.values(roads).flat()) : null;
  const reads = (name: string): boolean => !roads || (own ? own.includes(name) : nat === H.lead && !others!.has(name));
  for (const [name, month] of H[side]) {
    const r = g.byName[name];
    if (!r) continue;
    listed.add(r.id);
    if (!foe(r) || !reads(name)) continue;
    if (g.turn >= month + d) open.push(r);
    else if (g.turn === month + d - 1) soon.push(r);
  }
  // (Ruling 200(i): what the power's side takes that the record never gave it is open to the other side at once.)
  if (!mine) for (const r of g.regions) if (!listed.has(r.id) && foe(r) && (r.ctrl === P.power || allied(g, r.ctrl, P.power)) && neverHis(g, r)) open.push(r);
  return { book: true, open, soon };
}

// ---- America's two oceans (ruling 190) -------------------------------------------------------------------------
//
// The computer's America ships its army by shares of its sea lift (world.json 'overseas'; staff.ts `oceanRuns`).
// The Atlantic's share is in that table. The Pacific's is here: one share for the ocean, on top of the Atlantic's,
// worked by two runs, one for each road. The southern run ends on a harbour of its side's south of the equator
// (Australia, New Zealand, New Caledonia, Fiji, Samoa, and what is taken beyond them); the central run on one north of
// it, from Hawaii westward. Before America is at war with Japan there is no run and no share (190(d)).

/** The run a harbour of the Pacific's ground is on, by the side of the equator it lies on; null off that ground and where the far side is off. */
export function runOf(g: Game, r: Region): string | null {
  const R = PACIFIC?.runs;
  if (!R || !pacificGround(g, r)) return null;
  const south = r.y > R.equator;
  return Object.keys(R.roads).find((k) => R.roads[k].south === south) ?? null;
}

/**
 * True for a harbour on the lane of the Pacific's runs: one the country itself holds, one on the home ground of a
 * country the lane names, still in that country's hands, or one named for it (America's own line). Australia, New
 * Zealand, New Caledonia, Fiji, Samoa, and what is taken beyond them: no army is put down in a harbour of a third
 * country's colonies because the enemy is near it.
 */
const onLane = (R: RunRules, r: Region): boolean => r.ctrl === R.of || (R.lane.includes(r.owner) && r.ctrl === r.owner && !r.overseas) || R.havens.includes(r.name);

/** Whether a province is where the named run's ships can put an army down: on its side of the equator, and on the lane. Null for an ocean that is no run of the Pacific's. */
export const onRun = (g: Game, ocean: string, r: Region): boolean | null => (PACIFIC?.runs?.roads[ocean] ? runOf(g, r) === ocean && onLane(PACIFIC.runs, r) : null);

/**
 * Whether a corps standing in one harbour may be sent on by ship to another, as far as the Pacific's lane goes: a
 * corps across the Pacific goes on only along the lane, and no corps is sent on to the Pacific's ground from
 * anywhere else. True of every pair with the far side off, and for a country whose shipping the runs are not.
 */
export function laneOn(g: Game, nat: string, here: Region, to: Region): boolean {
  const R = runsOf(g, nat);
  if (!R) return true;
  const a = runOf(g, here) !== null, b = runOf(g, to) !== null;
  return a === b && (!b || onLane(R, to));
}

/** A run of the Pacific's own quays at home (pacific.json, 'runs.from'), in their order; null for any other ocean. */
export const runQuays = (g: Game, ocean: string): Region[] | null =>
  PACIFIC?.runs?.roads[ocean] ? PACIFIC.runs.from.map((nm) => g.byName[nm]).filter((r): r is Region => !!r) : null;

/** The country whose shipping the Pacific's runs are, while they are its staff's to work: never a player's. */
const runsOf = (g: Game, nat: string): RunRules | null => {
  const R = PACIFIC?.runs;
  return R && reads(g) && nat === R.of && nat !== g.human ? R : null;
};

/**
 * The Pacific's runs this month, each with the share of the country's sea lift that works it: none before it is at
 * war with the power, or once the power is beaten. While the book stands, the rows' shares by date (the southern
 * run all of it until June 1943, then three parts in five), slid with the Pacific's calendar. Off the book the
 * share follows the roads with a beach named for a hop: three to two when both have one, and all of it to the
 * southern run, the lane to Australia, while neither has.
 */
export function pacificRuns(g: Game, nat: string): [string, number][] {
  const R = runsOf(g, nat);
  if (!R || !live(g, PACIFIC!.power) || !atWar(g, nat, PACIFIC!.power)) return [];
  const roads = Object.entries(R.roads);
  if (hopsBind(g)) {
    const d = hopSlide(g, hopRows()!);
    return roads.map(([k, row]) => [k, row.book.reduce((s, [t, x]) => (g.turn >= t + d ? x : s), 0)] as [string, number]).filter((x) => x[1] > 0);
  }
  const named = new Set<string>();
  for (const a of hopAims(g, nat)) { const k = runOf(g, a.to); if (k) named.add(k); }
  if (named.size > 1) return roads.filter(([k]) => named.has(k)).map(([k, row]) => [k, row.both]);
  return [[[...named][0] ?? R.idle, R.share]];
}

/**
 * Whether a staff's fleets go to the beach they cover (ruling 200(j); the Pacific planner's paper 14, section 4,
 * "Which fleet goes where"; rider b585 to ruling 234, item 1(b)): every computer staff in the Pacific's war while the far
 * side is on, since nothing belongs to one country (rider b599 (iii)): the power and its friends while they are at war
 * with anybody, the side against the power while it is at war with the power. Where the power's own fleet rule differs
 * it governs (paper 14, section 3: its carrier striking force and its battle line are kept together at the line's
 * base; navy.ts `keptTogether`). Never a player's fleets.
 */
export function coversBeaches(g: Game, nat: string): boolean {
  const P = PACIFIC;
  if (!P || !reads(g) || nat === g.human || !live(g, P.power)) return false;
  return nat === P.power || allied(g, nat, P.power) ? !!live(g, nat)?.atWar.length : atWar(g, nat, P.power);
}

/** The Pacific's share of a country's shipping once its war there is won (the power beaten that it fought): it joins the Atlantic's. 0 otherwise. */
export function pacificFreed(g: Game, nat: string): number {
  const R = runsOf(g, nat);
  if (!R) return 0;
  const p = g.nations[PACIFIC!.power];
  return p && !p.alive && (g.flags[`war:${nat}:${p.id}`] !== undefined || g.flags[`war:${p.id}:${nat}`] !== undefined) ? R.share : 0;
}

/** The special formation that sails on a Pacific run before any other corps (pacific.json, 'runs.first'), for the named run; null for any other ocean. */
export const firstOnRun = (ocean: string): string | null => (PACIFIC?.runs?.roads[ocean] ? PACIFIC.runs.first ?? null : null);

