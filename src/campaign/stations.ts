// The book's stations (ruling 234, rider b568 of 18 November 2026; the historian's table, defenders-book-b568.md).
//
// By the book the defenders play the record too, as Japan's landings already do. In Luzon the record's army stood
// thin along the beaches and fell back to Bataan; in Malaya it stood forward half-dug, gave up the peninsula and was
// on the island by the end of January; in Burma it fell back up the Irrawaddy and out to India. So at a stop that
// keeps the book, while the book is open in the theatre, the computer's corps the table names stand where the record
// had them month by month, with the record's works and no more:
//   a step     [month, place, works, {joins}]: from that month the corps stands in that place. `works` is the months
//              of fieldworks the record gives it there: none (0, fought in the open), the least step the game has
//              (1, begun or half-dug: the director's reading of "partial") or the whole (prepared positions: Bataan,
//              Kampar, Toungoo). It digs no more than that while the book has it (game.ts, digIn).
//   a move     is a fall-back, made as a retreat is: at the start of its country's month, with no march, to ground
//              its side still holds with no enemy in the field, and with a retreat's room. Where the place is lost
//              already it falls back to the next place on its road that is not. A corps shut in by the enemy does
//              not go. Moves wait for the war; the posture of the eve does not.
//   joins      brigades that fell back into one command are one corps from then on (the 12th Indian Brigade into the
//              11th Indian Division; the Philippine Army's divisions into the two corps of Bataan): the men of both,
//              in the corps named, where it stands in the same place.
//   held       a corps on a station is the book's and not the staff's: it keeps its hold until the book lets go.
//   free       a row marked so is only put down in its place on its month, and is the staff's from there: the road
//              of Japan's Guards through Siam to northern Malaya, and the heavy guns' move from Hong Kong to Luzon.
//              A step may name several places: the first its side holds.
//   slides     the calendar moves with the great choice named (Japan's strike south): before the blow by the month
//              the book gives it in this war, after it by the month it fell.
//   lets go    in the month after the last (`until`), when the book is closed in the theatre, or at a stop that
//              keeps none: every corps is the staff's again and digs as it likes.
//   worth      the record's strength along the road (rider b573): `[month, worth]`, the fighting worth (divisions
//              times strength) with which the corps met that month's fight. The record's fall-backs cost dearly, and
//              Bataan's army wasted on half rations: from that month the corps is cut to the figure where it stands
//              above it, before it joins another and after. A ceiling and never a gift: what the game's own battles
//              cost still counts, and nobody is raised to a figure. A worth of nothing is a command that is gone.
// The record's big attacks (rider b573; the historian's table, strength-and-attacks-b573.md) are the invader's side of
// the same book, 'attacks': each goes in on its month whatever odds its staff would ask, and is fought by the rules.
// Nothing about its result is written.
//   at         the place: the first on the list that is not yet its side's. With the last of them taken it is over.
//   force      the corps that made it, by name. All of them that live go in together, or it waits.
//   from       the ground they went in from. A corps not yet next to the place is put down there as a station is:
//              ground whose field its side holds, with no enemy in it and room. Where none is open the attack is
//              owed, and goes in the first month its force stands at the door, while the table runs.
//   sea        a landing: the force sails from the first port on its list from which the shore can be reached, by
//              the landing rules (the lift, the lane, the sky), ahead of the plan's own landings (rider b575).
//              Within a month the landings go in the record's date order (rider b599 (i): Japan's second ring), and a
//              force that made one earlier in the month sails again on its next, as the record's did every one to two
//              weeks from the place just taken; a row whose port has not fallen waits a month.
//   met        the record's fight (rider b580): `[attacker, defender]`, the fighting worth of each side that met in
//              it. The record's attacks never met a province's whole defence: they struck one part of a thin line
//              with the part of the force that had come up. So the fight is fought between those parts and no more:
//              every corps of a side goes in by the same share, so that the side comes to the record's figure, and
//              what the fight costs falls on the part that fought. Never a gift: a side with less than the figure
//              fights with what it has. The fight is the rules' own and nothing about its result is written.
//              Won, the rows of the line it broke go by the book's withdrawals the same month: to `back`, the place
//              (and works) where the record's line stood next inside that month, or by their own next step in the
//              table taken early. Lost, it stays owed and is asked once a month, to `until` where the record's
//              attack was called off. A landing with met figures came ashore on an empty beach (Java): the fight
//              it meets inland is fought at those figures with no cut for the shore.
//   ground     the ground the record's fight was fought on where that is not the province's own (rider b581: the
//              crossing to Singapore island and the Jurong line were mangrove, forest and creek, not streets).
//   nobody     where nobody stands in the field at the place, the attack goes in unfought, as a landing on an empty
//              shore does: its corps march in from the door, and the town, if it is held, is under siege.
//   made       an attack is made only when its place is taken (the design director's ruling of 21 November 2026 on
//              Rangoon's trace, to ruling 234), and a row of several places only when the last of them is taken
//              (the director's of 06:53Z: each place is the record's, and each stays owed until taken; the row goes on
//              to the next at its next ask). While a place holds, the row stays owed and is asked again. Its first
//              try at each place is the book's: the whole force comes, whatever odds, as the record's did. After a
//              try, the staff's own reckoning: where its corps stand round the held town they hold the siege, and
//              the town is stormed by the staff's own ask, as any town is (ai.ts, sieges); where none of them does,
//              the row goes in again only at the staff's own odds (`bookReckoning`: its least odds for a field, its
//              landing odds for a shore where anything fights). Nothing is dated and nothing is forced. A row with
//              `until` keeps the record's own months of trying (the director's of 11:17Z): lost, it is asked again
//              once a month to `until`, whatever the odds (Bataan's January attack into February).
//              While a row is owed, the corps it landed stay held to it; once it is not, however its place fell, they
//              are the staff's again, but for one a later row still goes with, held to that row in turn. A row after
//              it waits until it is made where it would start from that place: where a corps of it stands round the
//              held town, or where every place it goes in from (`from`, its own place aside) is held so. Nor is a
//              held place a door for it.
//   beside     a row that goes with another (rider b581): Japan's tank regiments, `[month, corps]`, each with the
//              division the record gives it from that month. It is the book's: it stands where its division stands
//              in the field, goes in with every dated attack its division makes, and takes neither shipping of its
//              own nor room on the roads.
//              The first attachment the table gives is a fact of the day, at every stop of the dial and for every
//              country (rider b604): a row held for the corps it goes with lands in that corps' lift the month it
//              lands, and is its staff's own after. Later re-attachments stay the book's.
//   late       a row whose record runs past the table's last month names its own (`until`): from the month after the
//              table's last to its own, the book still stands that one corps on its stations, with its worth, at a
//              stop that keeps the book and while the book is open in the theatre. Nothing else of the table binds
//              then: no dated attack, no other row. Its months are the calendar's own and do not slide with the
//              great choice, as the dated row that brings its corps does not: the slide is the opening's. (The
//              design director's ruling of 07:33Z, 22 November 2026: the 14th Area Army's rows on Luzon and Leyte
//              in 1944 and 1945, which their army's advance walked off Manila to Leyte in the month they landed.)
// 'fleets' are the record's covering forces (rider b575): in its months the fleet named patrols the sea named, by
// the book's order ahead of its admiral's own. What it meets there is fought by the rules.
// Never the player: his own corps are never stationed, and in a war where he is the invader nobody is (the line's
// rule, line.ts: computer against computer). Nor is a dated attack made on ground he holds. Stops 3 to 5 as before.
// The data is history.json, 'stations' (the far side's rows: far-pacific-war.json); with none, nothing here happens.

import { divisionsOf } from './armies';
import { DEFENCE, allied, atWar, friendlyUnits, hasRoom, hostileUnits, limitAttackers, men, moveCorps, moveCost, relocate, removeCorps, resolveBattle, withWorksAside, type AttackOpts } from './game';
import { HISTORY, bookDate, onBook, stopDef, theatreOf } from './history';
import { lineShut } from './line';
import { appoint, generalOf } from './commanders';
import { NAVY, canPass, fleetReach, landCorps, onOwnShipping, rebase, whyNotLand, zoneOf } from './navy';
import { blocked } from './rivers';
import { arrive, besieged, fieldCtrl, takeField } from './siege';
import { islandOf } from './islands';
import { STACK, type BattleReport, type Corps, type Game, type Region, type Stance, type Terrain } from './types';

/** A month of the record, the place (or the places, the first its side holds), the months of works there, and what else. */
type Step = [month: number, place: string | string[], works?: number, more?: { joins?: string }];
interface Row { nat: string; corps: string; free?: boolean; coast?: boolean; beside?: [month: number, corps: string][]; steps: Step[]; worth?: [month: number, worth: number][]; until?: number }
/** One of the record's big attacks: its month, what the record calls it, the place, the corps, and the ground or the ports they went from. */
export interface Attack {
  month: number; name: string; at: string[]; force: string[]; from?: string[]; sea?: boolean;
  /** The last month it is asked for, where the record's attack was called off. */
  until?: number;
  /** The record's fight (rider b580): the fighting worth of each side that met in it, the attacker's and the defender's. */
  met?: [att: number, def: number];
  /** The ground the record's fight was fought on, where that is not the province's own (rider b581). */
  ground?: Terrain;
  /** Where the record's line stood next inside the month, and with what works: won, the rows it broke go there (rider b580). */
  back?: [place: string, works?: number];
  /**
   * Whether the record's fight met prepared works (riders b626 and b628 to ruling 234; the historian's mark on each
   * dated landing). Works count only for the defenders the record has fighting from them: `true` where the landing
   * met the place's own works (they cut the attack in full, and every defender's fieldworks count); a list names
   * the defending rows whose fieldworks it met, the place's works and every other row's fieldworks being set aside
   * (an empty list: it landed where the works were not). With none a landing is read as rider b580 built it: the
   * place's works set aside, the defenders' fieldworks counted.
   */
  works?: true | string[];
  /**
   * The dated choice whose book this row is in, where that is not the table's own (`"JAP:ichigo"`; the design
   * director's rulings of 07:44Z and 07:46Z, 22 November 2026): see `ownBook`. The row is that choice's country's
   * fight (`rowNat`; his 14:36Z of the same day): Britain's takings in East Africa are rows of this one table.
   */
  book?: string;
}
/** A covering fleet by the record: from `month` to `until` (that month alone with none) it patrols the sea named. */
interface Cover { nat?: string; month: number; until?: number; fleet: string; sea?: string; base?: string[] }
export interface Stations {
  /** The book's theatre: closed there, the stations are gone with it. */
  theatre: string;
  /** The great choice ('JAP:south') whose month the calendar moves with. */
  slides: string;
  /** The invader: moves wait for a war with him, and a player who is him is met by the staffs alone. */
  taker: string;
  /** The last month of the record's table. */
  until: number;
  rows: Row[];
  attacks?: Attack[];
  fleets?: Cover[];
}

/** The stations the book gives (history.json, 'stations'). */
export const stations = (): Stations | null => (HISTORY as unknown as { stations?: Stations }).stations ?? null;

/**
 * The country a dated attack is: its own choice's where it names one (`book`), else the table's invader (the design
 * director's 14:36Z of 22 November 2026: a dated attack that names its own choice is that choosing country's fight,
 * one map). As built every row was the invader's, whoever its book named, and the table has one invader: Britain's
 * takings in East Africa by the book (far-africa.json, `UK:eastafrica`) could be nobody's.
 */
const rowNat = (B: Stations, row: Attack): string => (row.book !== undefined ? row.book.split(':')[0] : B.taker);

/** How far the calendar has moved in this war, and whether the blow has fallen. Null where the book gives the choice no month. */
function calendar(g: Game, B: Stations): { slide: number; date: number; went: boolean } | null {
  const [nat, op] = B.slides.split(':');
  const date = HISTORY.dates[nat]?.[op];
  if (date === undefined) return null;
  const went = g.flags[`st:done:${nat}:${op}`] ?? 0;
  const now = went || bookDate(g, nat, op);
  return now === null ? null : { slide: now - date, date, went: went > 0 };
}

/** True in a war where the book dates this country's rows at all, whatever the month (the stations themselves bind only while the table runs). */
export function bookKept(g: Game, nat: string): boolean {
  const B = stations();
  return !!B && nat !== g.human && g.human !== B.taker && !!stopDef(g).book && onBook(g, B.theatre);
}

/** True while the stations bind this country's corps this month. */
export function stationsBind(g: Game, nat: string): boolean {
  const B = stations();
  if (!B || nat === g.human || g.human === B.taker || !stopDef(g).book || !onBook(g, B.theatre)) return false;
  const cal = calendar(g, B);
  return !!cal && g.turn <= B.until + cal.slide;
}

/**
 * True while a dated attack that names its own choice (`book`) is asked (the design director's rulings of 07:44Z and
 * 07:46Z, 22 November 2026, on Nanning's trace). The table is the Pacific's: its theatre, its great choice's
 * calendar, and its last month, the summer of 1942. China's offensive of 1944 has a book of its own, and its rows
 * laid in the table were never asked. Such a row is asked at a stop that keeps the book, computer against computer
 * as the table's are, once its own choice has gone and while that choice's theatre keeps the book. The table's last
 * month and the table's theatre do not bind it. It is asked in its own month as written, with no slide: the month
 * list its theatre keeps (line.ts) does not slide either, so the row and the list read the same months (the choice
 * of 1944 is dated 56 and goes in 57 in every war read, and the list's plain months are what met the record). And
 * the list stays the leave it is: the row is not asked at a place the list keeps shut that month (`bookAttacks`).
 * It lapses with its own `until`, or with nothing done when its book is closed. It is asked of its own choice's
 * country and of no other (`rowNat`), and never of a player.
 */
function ownBook(g: Game, B: Stations, nat: string, row: Attack): boolean {
  if (row.book === undefined || nat !== rowNat(B, row) || nat === g.human || !stopDef(g).book) return false;
  const [n, op] = row.book.split(':');
  return (g.flags[`st:done:${row.book}`] ?? 0) > 0 && onBook(g, theatreOf(n, op));
}

/**
 * How far a dated attack's months have moved in this war, or null while the row is not asked at all: the table's
 * slide for a row of the table, while the stations bind (and, where `fallen` asks it, once the great blow has
 * fallen); none for a row with a book of its own, while that book asks it (`ownBook`).
 */
function slideOf(g: Game, B: Stations, nat: string, row: Attack, fallen: boolean): number | null {
  if (row.book !== undefined) return ownBook(g, B, nat, row) ? 0 : null;
  if (!stationsBind(g, nat)) return null;
  const cal = calendar(g, B)!;
  return fallen && !cal.went ? null : cal.slide;
}

/**
 * True of a row of its own later months (`until`) while the book still stations its corps: past the table's last
 * month, to the row's own, at a stop that keeps the book with the book open in the theatre. The table itself no
 * longer binds its country then (`stationsBind`): the row is the only thing of the book's that does.
 */
function lateBinds(g: Game, B: Stations, row: Row, slide: number): boolean {
  return row.until !== undefined && g.turn > B.until + slide && g.turn <= row.until && bookKept(g, row.nat);
}

/** True of a corps a row of its own later months still stations this month (`lateBinds`). */
export function lateStationed(g: Game, c: Corps): boolean {
  const B = stations();
  if (!B) return false;
  const cal = calendar(g, B);
  return !!cal && B.rows.some((row) => row.nat === c.nat && row.corps === c.name && lateBinds(g, B, row, cal.slide));
}

/**
 * True while the plan's own landings are the book's (rider b626 to ruling 234): the invader's computer staff, in the
 * month its great blow fell by the book and the month after, while that blow's landings are still going ashore
 * (staff.ts). They sail on the record's own shipping, as the table's dated landings do (navy.ts onOwnShipping).
 */
export function bookSails(g: Game, nat: string): boolean {
  const B = stations();
  if (!B || nat !== B.taker || !stationsBind(g, nat)) return false;
  const went = g.flags[`st:done:${B.slides}`] ?? 0;
  return went > 0 && g.turn - went <= 1;
}

/** The record's works on a corps where it stands: none, or so many months of them. */
function works(c: Corps, months: number): void {
  if (months > 0 && c.town === undefined) { c.dug = true; c.dugFor = Math.min(months, DEFENCE.entrench.months); }
  else { if (c.dug) delete c.dug; if (c.dugFor !== undefined) delete c.dugFor; }
  if (c.digging !== undefined) delete c.digging;
}

/** True of a corps whose strength the book sets this month (rider b573), or that goes with another by the book (rider b581): while the book is open its staff folds it into no other (rider b579). */
export function ceilinged(g: Game, c: Corps): boolean {
  const B = stations();
  return !!B && stationsBind(g, c.nat) && B.rows.some((row) => row.nat === c.nat && row.corps === c.name && (!!row.worth || !!row.beside));
}

/** The book lets a corps go: it is the staff's again. */
function letGo(g: Game, c: Corps): void {
  delete c.bookWorks;
  delete g.flags[`bk:b:${c.id}`];
  const mark = g.flags[`bk:h:${c.id}`] ?? 0;
  if (mark & 1) c.hold = false;
  if (mark & 2) delete c.stay;
  delete g.flags[`bk:h:${c.id}`];
}

/**
 * A landing row is no longer owed (made, its place fallen some other way, or called off): the corps its landing put
 * ashore and kept held to it are the staff's again (the director's of 06:53Z, 4), but for one another row of the
 * book still goes with, which stays held, now to that row (the first of them in the book's order with a place still
 * to take), and is let go when that row is no longer owed. Where the book no longer binds, all of them are
 * (`bookStations`).
 */
function landedFree(g: Game, B: Stations, i: number): void {
  const A = B.attacks!, who = rowNat(B, A[i]);
  // (Each row by its own calendar: the table's, or its own book's. `slideOf`. A row of another country's is no part of it.)
  const goes = (r: Attack, k: number, c: Corps): boolean => {
    if (k === i || g.flags[`ba:${k}`] || rowNat(B, r) !== who || !r.force.includes(c.name)) return false;
    const slide = slideOf(g, B, who, r, false);
    return slide !== null && !(r.until !== undefined && g.turn > r.until + slide) && r.at.some((nm) => !!g.byName[nm] && !allied(g, who, g.byName[nm].ctrl));
  };
  for (const c of g.corps) {
    if (g.flags[`bkl:${c.id}`] !== i + 1) continue;
    let j = -1;
    if (!c.dead) A.forEach((r, k) => { if (goes(r, k, c) && (j < 0 || r.month < A[j].month)) j = k; });
    if (j >= 0) { g.flags[`bkl:${c.id}`] = j + 1; c.hold = true; continue; }
    delete g.flags[`bkl:${c.id}`];
    if (!c.dead) c.hold = false;
  }
}

/**
 * True of a corps the book names in a dated attack still to come (the design director's ruling of 03:59Z, 22 November
 * 2026, on the 2nd Division's road to Java): a row of the table not yet made nor called off, with a place still to
 * take, while the stations bind its country. The staff's ships leave such a corps for the attack the book has for it
 * (ai.ts, hop); they took the 2nd Division from Sendai for a shore of their own, three months before its landing in
 * Java. At the stops that keep no book the corps is the staff's to judge, as before.
 */
export function bookedAhead(g: Game, c: Corps): boolean {
  const B = stations();
  if (!B?.attacks) return false;
  // (A row with a book of its own is read by that book: the table's last month does not let its corps go. `ownBook`.
  // Each country by its own rows: `rowNat`.)
  return B.attacks.some((r, k) => {
    if (rowNat(B, r) !== c.nat || g.flags[`ba:${k}`] || !r.force.includes(c.name)) return false;
    const slide = slideOf(g, B, c.nat, r, false);
    return slide !== null && !(r.until !== undefined && g.turn > r.until + slide) && r.at.some((nm) => !!g.byName[nm] && !allied(g, c.nat, g.byName[nm].ctrl));
  });
}

/**
 * True where a corps can fall back to: ground its side holds, no enemy in the field, and a retreat's room.
 * (The book's own station is counted against five and a retreat's two on an island of the Pacific too: the record
 * stood more commands on Singapore Island than the two the islands' table has room for, and the book is not kept
 * from its place by a table written after it. The historian mends the one or the other: form B3 of 25 November
 * 2026, (c).)
 */
const open = (g: Game, c: Corps, to: Region, joining: boolean): boolean =>
  allied(g, c.nat, to.ctrl) && !hostileUnits(g, to, c.nat).length && (to.id === c.reg || joining || friendlyUnits(g, to, c.nat).filter((u) => u.kind !== 'eng').length < STACK + 2);

/** Two commands made one: the men of `c` go into `into`, which stands in the same place. */
function join(g: Game, c: Corps, into: Corps): void {
  const a = into.str * into.divs, b = c.str * c.divs, divs = into.divs + c.divs, men = a + b;
  if (men > 0) {
    into.xp = (into.xp * a + c.xp * b) / men;
    into.org = (into.org * a + c.org * b) / men;
    if (into.mor !== undefined && c.mor !== undefined) into.mor = (into.mor * a + c.mor * b) / men;
  }
  // (What each may fill to goes with it: two brigades of an establishment make a corps of the two establishments.)
  const full = ((into.full ?? 1) * into.divs + (c.full ?? 1) * c.divs) / divs;
  // (Where either carries its divisions' own names, the one command carries them all: a division is laid once, drawn.ts.)
  const names = into.names || c.names ? [...divisionsOf(g, into), ...divisionsOf(g, c)].map((d) => [d.name, d.short] as [string, string]) : null;
  into.str = men / divs;
  into.divs = divs;
  if (names) into.names = names;
  if (full < 1) into.full = full; else delete into.full;
  const gen = generalOf(g, c);
  if (gen) appoint(g, gen, into.general === null ? into : null);
  if (c.bookWorks !== undefined) letGo(g, c);
  removeCorps(g, c);
}

/** The record's strength for a row this month: the fighting worth it may not stand above, or null where the table gives none yet. */
function worthNow(g: Game, row: Row, slide: number, date: number, inWar: boolean): number | null {
  let w: number | null = null;
  for (const [m, v] of row.worth ?? []) if (g.turn >= m + slide && (m <= date || inWar)) w = v;
  return w;
}

/** A ceiling, never a gift: the corps is cut to the record's worth where it stands above it. A worth of nothing is a command that is gone. */
function ceiling(g: Game, c: Corps, w: number | null): void {
  if (w === null || c.dead) return;
  if (w <= 0) { if (c.bookWorks !== undefined) letGo(g, c); removeCorps(g, c); return; }
  if (c.divs * c.str > w) c.str = w / c.divs;
}

/**
 * The start of a computer country's month: its corps the book has stationed take the record's place and posture for
 * the month (ai.ts, before the staff moves anyone). Where the book no longer binds, whatever it held is let go.
 */
export function bookStations(g: Game, nat: string): void {
  const B = stations();
  if (!B) return;
  if (!stationsBind(g, nat)) {
    // (A row of its own later months still stands its corps on its stations: `lateBinds`. Everything else is let go.)
    const cal = calendar(g, B);
    const late = cal ? B.rows.map((row, i) => ({ row, i })).filter(({ row }) => row.nat === nat && lateBinds(g, B, row, cal.slide)) : [];
    for (const c of g.corps) if (c.nat === nat) {
      if (late.some(({ row }) => row.corps === c.name)) continue;
      if (c.bookWorks !== undefined) letGo(g, c);
      if (c.bookCoast) delete c.bookCoast;
      // (A corps held to a dated landing with a book of its own is that book's while it asks the row: the table's
      // end does not let it go. `ownBook`; the design director's 18:08Z of 22 November 2026.)
      const k = g.flags[`bkl:${c.id}`];
      const held = k !== undefined ? B.attacks?.[k - 1] : undefined;
      if (k !== undefined && !(held && !g.flags[`ba:${k - 1}`] && ownBook(g, B, nat, held))) { delete g.flags[`bkl:${c.id}`]; if (!c.dead) c.hold = false; }
    }
    if (!late.length) return;
    const inWar = cal!.went && (nat === B.taker || allied(g, nat, B.taker) || atWar(g, nat, B.taker));
    // (By the calendar's own months: nothing of a late row slides.)
    const own = { ...cal!, slide: 0 };
    const cut = (): void => {
      for (const { row } of late) {
        if (!row.worth) continue;
        const c = g.corps.find((x) => x.nat === nat && !x.dead && x.name === row.corps);
        if (c) ceiling(g, c, worthNow(g, row, 0, own.date, inWar));
      }
    };
    cut();
    let moved = false;
    for (const { i } of late) if (take(g, B, i, own, inWar)) moved = true;
    cut();
    if (moved) g.supplyDirty = true;
    return;
  }
  // The fortress troops the record's figure for their place already holds (rider b579).
  for (const row of B.rows) if (row.nat === nat && row.coast) { const c = g.corps.find((x) => x.nat === nat && !x.dead && x.name === row.corps); if (c) c.bookCoast = true; }
  const cal = calendar(g, B)!;
  // What waits for the war: the eve's rows go by the calendar alone, a later one once the blow has fallen and the
  // country is in that war (the invader's own side, or at war with him).
  const inWar = cal.went && (nat === B.taker || allied(g, nat, B.taker) || atWar(g, nat, B.taker));
  // The record's strength (rider b573): before the month's moves, so that a brigade joins its division with the men
  // the road left it, and again when the joins are made.
  const cut = (): void => {
    for (const row of B.rows) {
      if (row.nat !== nat || !row.worth) continue;
      const c = g.corps.find((x) => x.nat === nat && !x.dead && x.name === row.corps);
      if (c) ceiling(g, c, worthNow(g, row, cal.slide, cal.date, inWar));
    }
  };
  cut();
  let moved = false;
  B.rows.forEach((row, i) => { if (row.nat === nat && take(g, B, i, cal, inWar)) moved = true; });
  cut();
  // The rows that go with another (rider b581): each to where its division stands.
  for (const row of B.rows) if (row.nat === nat && row.beside && follow(g, row, cal.slide)) moved = true;
  if (moved) g.supplyDirty = true;
}

/** Puts a corps on a station of the book: there, held for the book, with the record's works and no more. True if it moved. */
function put(g: Game, c: Corps, to: Region, months: number): boolean {
  const moved = to.id !== c.reg;
  if (moved) relocate(g, c, to);
  if (c.bookWorks === undefined) g.flags[`bk:h:${c.id}`] = (c.hold ? 0 : 1) | (c.stay ? 0 : 2);
  c.hold = true; c.stay = true;
  c.bookWorks = months;
  works(c, months);
  return moved;
}

/**
 * A row takes the step of its road that is due: the latest by the calendar or, `ahead`, the one after it, taken early
 * by a corps whose line was broken in a dated fight (rider b580). True if the corps moved.
 */
function take(g: Game, B: Stations, i: number, cal: { slide: number; date: number }, inWar: boolean, ahead = false): boolean {
  const row = B.rows[i], nat = row.nat;
  const c = g.corps.find((x) => x.nat === nat && !x.dead && x.name === row.corps);
  if (!c) return false;
  const taken = g.flags[`bk:${i}`] ?? 0;
  let due = taken;
  while (due < row.steps.length && g.turn >= row.steps[due][0] + cal.slide && (row.steps[due][0] <= cal.date || inWar)) due++;
  if (ahead && due < row.steps.length) due++;
  // (Between steps a corps the enemy drove off its station stands in the open where it stopped, unless the book stood it there.)
  if (due === taken) { if (!ahead && !row.free && c.bookWorks !== undefined && taken > 0 && g.flags[`bk:b:${c.id}`] !== c.reg + 1 && !placesOf(row.steps[taken - 1]).includes(g.regions[c.reg].name)) { c.bookWorks = 0; works(c, 0); } return false; }
  const here = g.regions[c.reg];
  // Shut in a town by the enemy, it does not go: it has the walls it stands behind.
  if (c.town !== undefined && hostileUnits(g, here, nat).length) return false;
  // The latest step due, or, where that ground is lost, the next place on its road that is not.
  for (let k = due - 1; k < row.steps.length; k++) {
    const [, , months = 0, more] = row.steps[k];
    const into = more?.joins ? g.corps.find((x) => x.nat === nat && !x.dead && x.name === more.joins && x.kind === c.kind && x.town === undefined) : undefined;
    const to = placesOf(row.steps[k]).map((nm) => g.byName[nm]).find((r) => !!r && open(g, c, r, !!into && into.reg === r.id));
    if (!to) continue;
    const moved = to.id !== c.reg;
    if (moved) relocate(g, c, to);
    g.flags[`bk:${i}`] = k + 1;
    delete g.flags[`bk:b:${c.id}`];
    if (row.free) return moved;
    if (into && into.reg === c.reg && c.town === undefined) { join(g, c, into); return moved; }
    put(g, c, to, months);
    return moved;
  }
  return false;
}

/**
 * A row that goes with another (rider b581, a tank regiment with its division): it is the book's, and stands where
 * the corps the record gives it this month stands, on ground whose field its side holds. True if it moved.
 */
function follow(g: Game, row: Row, slide: number): boolean {
  const c = g.corps.find((x) => x.nat === row.nat && !x.dead && x.name === row.corps);
  if (!c) return false;
  let name: string | null = null;
  for (const [m, nm] of row.beside ?? []) if (g.turn >= m + slide) name = nm;
  const lead = name === null ? undefined : g.corps.find((x) => x.nat === row.nat && !x.dead && x.name === name);
  const to = lead ? g.regions[lead.reg] : g.regions[c.reg];
  if (to.id !== c.reg && !(door(g, c, to) && (c.town === undefined || takeField(g, c)))) { put(g, c, g.regions[c.reg], 0); return false; }
  return put(g, c, to, 0);
}

/**
 * The name of the corps a row goes with this month (riders b581 and b604), or null for a row that goes with nobody.
 * While the book binds its country it is the record's for the month, re-attachments and all. Anywhere else it is the
 * attachment of the day, the first the table gives, and only while the row is still held for it: once it has landed
 * with its corps, or its corps has been let go, it is its staff's own.
 */
function attachment(g: Game, c: Corps): string | null {
  const B = stations();
  const row = B?.rows.find((r) => r.nat === c.nat && r.corps === c.name && !!r.beside?.length);
  if (!B || !row?.beside) return null;
  if (!stationsBind(g, c.nat)) return c.hold ? row.beside[0][1] : null;
  const cal = calendar(g, B)!;
  let name: string | null = null;
  for (const [m, nm] of row.beside) if (g.turn >= m + cal.slide) name = nm;
  return name;
}

/** The corps a row goes with this month, where it lives: null for a row that goes with nobody. */
export function goesWith(g: Game, c: Corps): Corps | null {
  const name = attachment(g, c);
  return name === null ? null : g.corps.find((x) => x.nat === c.nat && !x.dead && x.name === name) ?? null;
}

/** The rows that go this month with the corps of this name, living or not. */
export function ridersOf(g: Game, nat: string, name: string): Corps[] {
  return stations() ? g.corps.filter((x) => x.nat === nat && !x.dead && attachment(g, x) === name) : [];
}

/** The rows of a force that sail in another's lift (rider b604): each is in the force with the corps it goes with. */
export function riding(g: Game, cs: Corps[]): Corps[] {
  if (!stations()) return [];
  return cs.filter((c) => { const lead = goesWith(g, c); return !!lead && lead !== c && cs.includes(lead); });
}

/** The rows that go with a corps of this force, stand in the field on its quay with their month before them, and are not in it. */
export function alongside(g: Game, cs: Corps[]): Corps[] {
  if (!stations() || !cs.length) return [];
  return g.regions[cs[0].reg].units.filter((u) => {
    if (u.nat !== cs[0].nat || u.dead || cs.includes(u) || u.town !== undefined || u.kind === 'fort' || u.ap < Math.min(4, u.apMax)) return false;
    const lead = goesWith(g, u);
    return !!lead && cs.includes(lead);
  });
}

const placesOf = (s: Step): string[] => (Array.isArray(s[1]) ? s[1] : [s[1]]);

/** For the measuring harness: told of every dated attack the book weighs, made, owed or over. Nothing in the game listens. */
// (`taken` is the try's: a fight's field won, a landing's force ashore, and on a walk-in unfought the place fallen.
// `held` is the place's: it still holds after the try. A row of several places is owed while any of them holds.)
export const bookWatch: { tell: ((e: { row: Attack; what: 'due' | 'made' | 'owed' | 'over'; why?: string; to?: Region; force?: Corps[]; odds?: number; taken?: boolean; held?: boolean; report?: BattleReport }) => void) | null } = { tell: null };

/**
 * For the measuring harness: told of each corps a dated attack puts down at its door from where it stood (the design
 * director's bound of 07:44Z, 22 November 2026: a jump of more than a month's march is data to check against the
 * record), and of each month of march a corps makes toward its door (`leg`; the director's 11:41Z). `months` is what
 * the whole road to the door takes it by the move rules (1: within a month), null where there is no road over its
 * side's ground. Nothing in the game listens.
 */
export const broughtWatch: { tell: ((e: { row: Attack; corps: Corps; from: Region; to: Region; months?: number | null; leg?: boolean }) => void) | null } = { tell: null };

/**
 * The staff's own reckoning of a book row's later tries at a place (`made`; the design director's ruling of 06:53Z,
 * 21 November 2026): null where its staff would send the force in, or why not. The staff sets it (ai.ts). Unset, a
 * later try goes in as the first did.
 */
export const bookReckoning: { why: ((g: Game, nat: string, force: Corps[], to: Region, sea: boolean) => string | null) | null } = { why: null };

/**
 * True where an attack's corps can be put down to go in from: ground whose field its side holds, no enemy in it, and room.
 * (The book's own put-down, counted as `open` counts a station.)
 */
const door = (g: Game, c: Corps, r: Region): boolean =>
  allied(g, c.nat, fieldCtrl(g, r)) && !hostileUnits(g, r, c.nat).length && (r.id === c.reg || friendlyUnits(g, r, c.nat).filter((u) => u.kind !== 'eng').length < STACK + 2);

/** Puts a corps of a dated attack down where it goes in from. False where it is shut in a town. */
function bring(g: Game, c: Corps, to: Region): boolean {
  if (c.town !== undefined && !takeField(g, c)) return false;
  if (c.reg !== to.id) { relocate(g, c, to); g.supplyDirty = true; }
  return true;
}

/**
 * The months of march a road takes a corps by the move rules, a full month's moves in each (game.ts, `moveCorps`: a
 * step is made with the moves it costs in hand, and any step with four).
 */
function monthsBy(g: Game, c: Corps, road: number[]): number {
  const full = Math.max(1, c.apMax);
  let months = 1, ap = full, at = c.reg;
  for (const id of road) {
    const cost = moveCost(g, c, g.regions[id], at);
    if (ap <= 0 || ap < Math.min(cost, 4)) { months++; ap = full; }
    ap = Math.max(0, ap - cost);
    at = id;
  }
  return months;
}

/**
 * A corps' road to a door over its side's ground, the door last, and the months of march it needs; null with no such
 * road. The road is the one a kept march takes (marches.ts, `farGround`: the cheapest by what each step costs a
 * move, over ground its side holds, by no way closed to it).
 */
function wayTo(g: Game, c: Corps, at: Region): { road: number[]; months: number } | null {
  if (at.id === c.reg || !allied(g, c.nat, at.ctrl)) return null;
  const cost = new Map<number, number>([[c.reg, 0]]), prev = new Map<number, number>(), done = new Set<number>();
  for (;;) {
    let a = -1, best = Infinity;
    for (const [k, v] of cost) if (!done.has(k) && v < best) { best = v; a = k; }
    if (a < 0) return null;
    if (a === at.id) break;
    done.add(a);
    for (const j of g.regions[a].nb) {
      const r = g.regions[j];
      if (done.has(j) || !allied(g, c.nat, r.ctrl) || blocked(g, c, r, a)) continue;
      const step = best + Math.max(1, moveCost(g, c, r, a));
      if (step < (cost.get(j) ?? Infinity)) { cost.set(j, step); prev.set(j, a); }
    }
  }
  const road: number[] = [];
  for (let k = at.id; k !== c.reg; k = prev.get(k)!) road.unshift(k);
  return { road, months: monthsBy(g, c, road) };
}

/**
 * One month of a corps' road to the door of its dated attack, by the move rules and no further: the month is the
 * march's, and it does nothing else in it. Once in a month. True if it moved.
 */
function stride(g: Game, row: Attack, c: Corps, way: { road: number[]; months: number }): boolean {
  const mark = `bkm:${c.id}`;
  if (g.flags[mark] === g.turn) return false;
  g.flags[mark] = g.turn;
  if (c.town !== undefined && !takeField(g, c)) return false;
  const from = g.regions[c.reg];
  for (const id of way.road) if (!moveCorps(g, c, g.regions[id])) break;
  c.ap = 0;
  if (c.reg === from.id) return false;
  g.supplyDirty = true;
  broughtWatch.tell?.({ row, corps: c, from, to: g.regions[c.reg], months: way.months, leg: true });
  return true;
}

/**
 * A dated attack by land that is still to come sets out for its door (the design director's 11:41Z of 22 November
 * 2026 to ruling 234, on the sixteen long moves reported under the 07:44Z bound). The arrival is the record's date
 * and the march is the game's own: a corps that needs more than a month's march to the row's door leaves early
 * enough to arrive by the move rules, the last month of its road being the date's own, when it is put down at the
 * door as it always was. So no corps is moved more than a month's march in a month. It marches over its side's
 * ground to the first door of the row's that is open to it; with no door open yet, or no road, it does not set out.
 * A corps an earlier row of the book still names (`claimed`), one that goes with another, and one that has fought
 * this month, stay; so does one the great blow's own landings still have to put ashore (`bookAttacks`, `owed`).
 *
 * A dated landing still to come sends its corps to their quay a month ahead (the design director's 18:08Z of 22
 * November 2026 to ruling 234, on Assab's trace: one rule, every dated row arrives on its date, by sea or by land).
 * As built a landing's corps were put down on their quay in the date's own month. Britain's Aden force, marched
 * inland by its staff and standing in Addis Ababa's town in June 1941, came out of the town and to Aden that month
 * with its month's march spent, could not embark, and landed at Assab in July against the record's June. Now a corps
 * that is not on one of the row's ports is there, in the field, by the end of the month before: it marches by the
 * move rules over its side's ground, setting out early enough that the last month of its road is the one before the
 * date, or, with no road by land (its quay is over the sea), is put down on the quay in that last month, as the
 * date's own month always put it. One standing in the town of its port comes out into the field that month. From
 * then it is held to its row, and is the staff's again when the row is no longer owed (`landedFree`).
 */
function setsOut(g: Game, nat: string, B: Stations, i: number, date: number, claimed: Set<string>, riders: Row[]): void {
  const row = B.attacks![i];
  if (row.sea) {
    const quays = (row.from ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && r.port && allied(g, nat, r.ctrl));
    for (const nm of row.force) {
      if (claimed.has(nm) || riders.some((r) => r.corps === nm)) continue;
      const c = g.corps.find((x) => x.nat === nat && !x.dead && x.name === nm);
      if (!c || c.fought) continue;
      const ahead = date - g.turn === 1;
      if (quays.some((q) => q.id === c.reg)) {
        // (On its port already: out of the town in the last month, and the row's from then.)
        if (!ahead || (c.town !== undefined && !takeField(g, c))) continue;
      } else {
        const q = quays.find((r) => door(g, c, r));
        if (!q) continue;
        const way = wayTo(g, c, q);
        if (way) {
          // (So many months of march, the last of them the month before the date.)
          if (way.months < date - g.turn || !stride(g, row, c, way) || c.reg !== q.id) continue;
        } else {
          if (!ahead || g.flags[`bkm:${c.id}`] === g.turn) continue;
          const from = g.regions[c.reg];
          if (!bring(g, c, q)) continue;
          g.flags[`bkm:${c.id}`] = g.turn;
          c.ap = 0;
          broughtWatch.tell?.({ row, corps: c, from, to: q, months: null });
        }
      }
      c.hold = true;
      g.flags[`bkl:${c.id}`] = i + 1;
    }
    return;
  }
  const to = row.at.map((nm) => g.byName[nm]).find((r) => !!r && !allied(g, nat, r.ctrl));
  if (!to || !atWar(g, nat, to.ctrl)) return;
  if (g.human !== null && (to.ctrl === g.human || hostileUnits(g, to, nat).some((u) => u.nat === g.human))) return;
  const doors = (row.from ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && to.nb.includes(r.id));
  for (const nm of row.force) {
    if (claimed.has(nm) || riders.some((r) => r.corps === nm)) continue;
    const c = g.corps.find((x) => x.nat === nat && !x.dead && x.name === nm);
    if (!c || c.fought || c.reg === to.id || to.nb.includes(c.reg)) continue;
    const at = doors.find((r) => door(g, c, r));
    const way = at ? wayTo(g, c, at) : null;
    // (So many months of march, the last of them the date's own.)
    if (way && way.months - 1 >= date - g.turn) stride(g, row, c, way);
  }
}

/**
 * The record's fight (rider b580). Each side goes in with the part of it that met in the record's fight and no more:
 * every corps of a side by the same share, so that the side comes to the record's figure (never above what it has),
 * on the ground the record gives where that is not the province's own (rider b581). The fight is the game's own.
 * What it costs falls on the part that fought: the rest of each corps comes out as it went in. A corps that lays
 * down its arms with nowhere to go does so whole.
 */
function fightMet(g: Game, pool: Corps[], to: Region, row: Attack, opts: AttackOpts = {}): BattleReport {
  const nat = pool[0].nat;
  const worth = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
  const defs = hostileUnits(g, to, nat);
  // (The fortress troops that add nothing to a field fight round their works are no part of the line it met: rider b579.)
  const faced = opts.sea !== undefined || (!!to.fort && pool.some((c) => to.fort!.vs.includes(g.regions[c.reg].owner)));
  const line = defs.filter((c) => !c.bookCoast || faced);
  const was: { c: Corps; f: number; divs: number; str: number; org: number; mor?: number; men: number }[] = [];
  if (row.met) {
    const fa = Math.min(1, row.met[0] / Math.max(1e-9, worth(pool))), fd = Math.min(1, row.met[1] / Math.max(1e-9, worth(line)));
    for (const c of pool) was.push({ c, f: fa, divs: c.divs, str: c.str, org: c.org, mor: c.mor, men: men(c) });
    for (const c of defs) was.push({ c, f: line.includes(c) ? fd : 1e-6, divs: c.divs, str: c.str, org: c.org, mor: c.mor, men: men(c) });
  }
  for (const w of was) { w.c.divs *= w.f; w.c.bookPart = [w.f, w.str]; }
  const ground = to.terrain;
  if (row.ground) to.terrain = row.ground;
  // (A landing the record made on an empty beach fought inland: the works on the shore are no part of that fight.
  // By the historian's mark the works count only for the defenders the record has fighting from them: riders b626
  // and b628. Set aside, they are out of the fight alone, never of the place's supply or stores: game.ts.)
  const aside = row.works === true ? false : row.works !== undefined || opts.sea !== undefined;
  const named = Array.isArray(row.works) ? row.works : null;
  const unmet = named ? defs.filter((c) => c.dug && !named.includes(c.name)).map((c) => ({ c, dugFor: c.dugFor })) : [];
  for (const w of unmet) { delete w.c.dug; delete w.c.dugFor; }
  const fight = (): BattleReport => resolveBattle(g, pool, to, opts);
  let report: BattleReport;
  try { report = aside ? withWorksAside(to, fight) : fight(); }
  finally {
    to.terrain = ground;
    // (Fieldworks that were no part of the fight stand as they stood, where their corps still holds them.)
    for (const w of unmet) if (!w.c.dead && w.c.reg === to.id && w.c.town === undefined) { w.c.dug = true; if (w.dugFor !== undefined) w.c.dugFor = w.dugFor; }
    for (const w of was) {
      const c = w.c;
      c.divs = w.divs;
      delete c.bookPart;
      if (c.dead) continue;
      c.str = w.str - w.f * (w.str - c.str);
      c.org = w.org - w.f * (w.org - c.org);
      if (w.mor !== undefined && c.mor !== undefined) c.mor = w.mor - w.f * (w.mor - c.mor);
    }
  }
  // (Those who laid down their arms did so whole: the men of each such corps who were not in the fight are counted with its prisoners.)
  for (const w of was) {
    if (!w.c.dead || w.c.nat === nat) continue;
    const rest = w.men - Math.round(w.men * w.f);
    const u = report.defenders.find((x) => x.name === w.c.name && x.nat === w.c.nat);
    if (rest > 0 && u) { u.lost += rest; report.prisoners += rest; }
  }
  return report;
}

/**
 * A dated fight at the record's figures is won (rider b580): the rows of the line it broke go by the book's
 * withdrawals the same month, to the place where the record's line stood next inside the month (`back`) or, with
 * none, by their own next step in the table, taken early. One shut in a town, or with no ground open, stays where
 * the rout left it.
 */
function fallBack(g: Game, B: Stations, cal: { slide: number; date: number }, beaten: Corps[], back: Attack['back']): void {
  let moved = false;
  for (const c of beaten) {
    if (c.dead) continue;
    const i = B.rows.findIndex((r) => r.nat === c.nat && r.corps === c.name && !r.free && !r.coast && !r.beside);
    if (i < 0 || !stationsBind(g, c.nat)) continue;
    const to = back ? g.byName[back[0]] : undefined;
    if (!to) { if (take(g, B, i, cal, true, true)) moved = true; continue; }
    if (c.town !== undefined && hostileUnits(g, g.regions[c.reg], c.nat).length) continue;
    if (!open(g, c, to, false)) continue;
    if (put(g, c, to, back![1] ?? 0)) moved = true;
    g.flags[`bk:b:${c.id}`] = to.id + 1;
  }
  if (moved) g.supplyDirty = true;
}

/**
 * The record's big attacks (rider b573): the invader's computer staff makes each on its month, by the book and
 * whatever odds it would ask, before its own landings and attacks, and is asked once more when its month's marching
 * is done, for one whose corps have only then come up to the door (ai.ts). An attack that cannot go in is owed while
 * the table runs. The battle is the game's own: at the record's met figures where the table gives them (rider b580).
 * Each country makes its own rows and no other's: the invader the table's, and a country whose choice a row names
 * that row (`rowNat`; the design director's 14:36Z of 22 November 2026).
 */
export function bookAttacks(g: Game, nat: string, owed?: ReadonlySet<string>): void {
  const B = stations();
  if (!B?.attacks) return;
  const mine = (r: Attack): boolean => rowNat(B, r) === nat;
  if (nat !== B.taker && !B.attacks.some(mine)) return;
  // The table's own calendar, while the stations bind and the great blow has fallen: its invader's alone. A row that
  // names its own choice is asked by that choice's book instead, in its own month as written (`ownBook`).
  const T = nat === B.taker && stationsBind(g, nat) ? calendar(g, B)! : null;
  const table = T && T.went ? T : null;
  const slideFor = (r: Attack): number | null => (r.book === undefined ? (table ? table.slide : null) : ownBook(g, B, nat, r) ? 0 : null);
  if (!table && !B.attacks.some((r) => r.book !== undefined && ownBook(g, B, nat, r))) return;
  const riders = B.rows.filter((r) => r.nat === nat && r.beside);
  // (By their months, and within a month in the table's order, which is the record's date order: a row owed from an
  // earlier month goes in before the month's own, as the record's did: rider b599 (i).)
  const order = B.attacks.map((_, i) => i).filter((i) => mine(B.attacks![i])).sort((a, b) => B.attacks![a].month - B.attacks![b].month || a - b);
  // The corps the rows before it in the book's order still name: none of them sets out for a later row (`setsOut`).
  // (Nor does one the great blow's own landings still have to put ashore, `owed`: they are the book's too (rider b626)
  // and no rows of this table, so the staff names them. ai.ts, blowOwes.)
  const claimed = new Set<string>(owed);
  order.forEach((i) => {
    const row = B.attacks![i];
    const key = `ba:${i}`;
    const slide = slideFor(row);
    if (slide === null || g.flags[key] || !g.nations[nat].alive) return;
    // (A row still to come: its corps set out in time to arrive by the move rules. The design director's 11:41Z.)
    if (g.turn < row.month + slide) { setsOut(g, nat, B, i, row.month + slide, claimed, riders); for (const nm of row.force) claimed.add(nm); return; }
    // A fight at the record's figures that was lost this month, or a row whose place held this month, is not tried again in it.
    if (g.flags[`bat:${i}`] === g.turn) { for (const nm of row.force) claimed.add(nm); return; }
    if (row.until !== undefined && g.turn > row.until + slide) { g.flags[key] = g.turn; landedFree(g, B, i); bookWatch.tell?.({ row, what: 'over', why: 'the record\'s attack was called off' }); return; }
    const to = row.at.map((nm) => g.byName[nm]).find((r) => !!r && !allied(g, nat, r.ctrl));
    if (!to) { g.flags[key] = g.turn; landedFree(g, B, i); bookWatch.tell?.({ row, what: 'over', why: 'the place is its side\'s' }); return; }
    const force = row.force.map((nm) => g.corps.find((x) => x.nat === nat && !x.dead && x.name === nm)).filter((x): x is Corps => !!x);
    for (const nm of row.force) claimed.add(nm);
    // The rows that go with another (the tank regiments, rider b581) go in with their divisions and wait for nobody.
    const tanks = force.filter((c) => riders.some((r) => r.corps === c.name)), main = force.filter((c) => !tanks.includes(c));
    // (While it is owed, the corps it landed stay held to it, whatever the plan has let go since: the director's of
    // 06:53Z, 4. The plan's release of its landing forces, staff.ts `freed`, comes before the month's first ask.)
    for (const c of main) if (g.flags[`bkl:${c.id}`] === i + 1) c.hold = true;
    const owed = (why: string): void => { bookWatch.tell?.({ row, what: 'owed', why, to, force }); };
    if (!main.length) return owed('none of its corps is in the field');
    if (!atWar(g, nat, to.ctrl)) return owed('no war with its holder');
    // Computer against computer: ground a player holds, or his corps, are met by the staff alone.
    if (g.human !== null && (to.ctrl === g.human || hostileUnits(g, to, nat).some((u) => u.nat === g.human))) return owed('the player holds it');
    // (The month list stays the leave it is: a row with a book of its own is not asked at a place the list keeps shut
    // this month. It stays owed, and goes in when the list opens the place. The table's rows as they were.)
    if (row.book !== undefined && lineShut(g, nat, to)) return owed('the month list keeps the place shut');
    for (const c of tanks) follow(g, riders.find((r) => r.corps === c.name)!, table ? table.slide : calendar(g, B)?.slide ?? 0);
    const line = hostileUnits(g, to, nat);
    // Made only when its place is taken, and a row of several places only with the last of them; while its place
    // holds it stays owed, and is asked again next month. With this place taken and another still to take, it goes
    // on to that one at its next ask.
    const made = (): boolean => {
      if (row.at.every((nm) => !g.byName[nm] || allied(g, nat, g.byName[nm].ctrl))) { g.flags[key] = g.turn; landedFree(g, B, i); return true; }
      if (!allied(g, nat, to.ctrl)) g.flags[`bat:${i}`] = g.turn;
      return false;
    };
    // (A fight at the record's figures that is won still sends the line it broke back by the book, the town held or not.)
    const done = (won: boolean): boolean => {
      const ok = made();
      // (The broken line goes back by the table's own steps, on the table's calendar: none where the table no longer binds.)
      if (won && row.met && table) fallBack(g, B, table, line, row.back);
      return ok;
    };
    // Its first try at each place is the book's; after it, the staff's own reckoning (`made`). (One mark for each of
    // its places, so that a place it has tried and that is lost again is not tried as a first time.)
    const k = row.at.findIndex((nm) => g.byName[nm] === to);
    const tried = (((g.flags[`bt:${i}`] ?? 0) >> k) & 1) === 1;
    const tries = (): void => { g.flags[`bt:${i}`] = (g.flags[`bt:${i}`] ?? 0) | (1 << k); };
    // (A row with `until` keeps the record's own months of trying, whatever the odds: the director's of 11:17Z.)
    const reckons = (cs: Corps[], sea: boolean): string | null => (tried && row.until === undefined ? bookReckoning.why?.(g, nat, cs, to, sea) ?? null : null);
    const oddsOf = (rep: BattleReport | null | undefined): number | undefined => (rep ? rep.aPower / Math.max(rep.dPower, 1e-9) : undefined);
    // The places of the rows before it that are owed with their place held: it does not start from one of them.
    const shut = new Set<number>();
    for (const j of order) {
      if (j === i) break;
      const r = B.attacks![j];
      const s = slideFor(r);
      if (g.flags[`ba:${j}`] || s === null || g.turn < r.month + s || (r.until !== undefined && g.turn > r.until + s)) continue;
      const at = r.at.map((nm) => g.byName[nm]).find((x) => !!x && !allied(g, nat, x.ctrl));
      if (at) shut.add(at.id);
    }
    const stuck = main.find((c) => shut.has(c.reg) && c.town === undefined);
    if (stuck) return owed(`a corps of it stands round ${g.regions[stuck.reg].name}, which holds: the row before it is owed`);
    const starts = (row.from ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && r !== to);
    if (starts.length && starts.every((r) => shut.has(r.id))) return owed(`it goes in from ${starts.map((r) => r.name).join(' or ')}, which holds: the row before it is owed`);
    // After its first try, its corps stand round the town it is owed, and the town holds: they hold the siege, and the
    // staff storms by its own ask. (On its first try the whole force comes, those round the town staying where they are.)
    if (tried && !line.length && besieged(g, to) && main.some((c) => c.reg === to.id && c.town === undefined)) {
      g.flags[`bat:${i}`] = g.turn;
      return owed('its corps stand round the town, which holds');
    }
    if (row.sea) {
      // Each landing sails from its own port: the first of the row's its side holds from which the shore can be reached.
      const quays = (row.from ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && r.port && allied(g, nat, r.ctrl) && main.every((c) => door(g, c, r)));
      // (The record's pace, rider b599 (i): a landing every one to two weeks, each from the place just taken. A corps
      // that made one of the book's landings earlier in its month sails again on its next one, from the port it took
      // or the one it was thrown back to: Menado and then Kendari, Tarakan and then Balikpapan, Wake's try and then
      // its taking. Only for the landing it sails on: refused, it has had its month.)
      // Never one that landed elsewhere: it is not taken from there to another port in the same month.
      const again = main.filter((c) => g.flags[`bsl:${c.id}`] === g.turn);
      const spent = again.map((c) => c.ap);
      const ready = (on: boolean): void => again.forEach((c, k) => { c.ap = on ? Math.max(spent[k], Math.min(4, c.apMax)) : spent[k]; });
      let why = 'none of its ports is its side\'s';
      for (const q of quays) {
        if (again.some((c) => c.reg !== q.id)) { why = 'a corps of it has made its landing elsewhere this month'; continue; }
        if (!main.every((c) => bring(g, c, q))) { why = 'a corps of it is shut in'; break; }
        ready(true);
        // (On the record's own shipping: a dated landing draws nothing from the month's lift, and waits on nothing but
        // its own force and its date. Rider b626.)
        const no = onOwnShipping(nat, () => whyNotLand(g, main, to)) ?? reckons(main, true);
        if (no) { ready(false); why = no; continue; }
        // (The tanks sail with their divisions, and take no shipping of their own.)
        const aboard = tanks.filter((c) => door(g, c, q) && bring(g, c, q));
        bookWatch.tell?.({ row, what: 'due', to, force: [...main, ...aboard] });
        // (A landing with the record's figures came ashore on an empty beach: its fight is inland, with no cut for the shore.)
        const res = onOwnShipping(nat, () => landCorps(g, main, to, row.met ? (cs, at) => fightMet(g, [...cs, ...aboard], at, row, { sea: 1 }) : undefined, true));
        if (!res.ok) { ready(false); why = res.why ?? 'the landing could not sail'; continue; }
        for (const c of main) g.flags[`bsl:${c.id}`] = g.turn;
        // (The corps it landed are this row's now, whatever row held them before.)
        for (const c of main) if (g.flags[`bkl:${c.id}`] !== undefined) g.flags[`bkl:${c.id}`] = i + 1;
        tries();
        const won = !!res.taken;
        if (won) for (const c of aboard) if (!c.dead && c.reg !== to.id && door(g, c, to)) relocate(g, c, to);
        // (While the row is owed the corps it landed stay held to it: ashore with the town holding, or thrown back.
        // When it is no longer owed, however its place came to fall, they are the staff's again: `landedFree`.)
        // (Made by its own landing, its corps are let go as they always were, but for one held to another row.)
        if (done(won)) { for (const c of main) if (!c.dead && g.flags[`bkl:${c.id}`] === undefined) c.hold = false; }
        else for (const c of main) if (!c.dead) { c.hold = true; g.flags[`bkl:${c.id}`] = i + 1; }
        bookWatch.tell?.({ row, what: 'made', to, force: [...main, ...aboard], odds: oddsOf(res.report), taken: won, held: !allied(g, nat, to.ctrl), report: res.report ?? undefined });
        return;
      }
      if (quays.length && !again.length) main.every((c) => bring(g, c, quays[0]));
      return owed(why);
    }
    // (Where nobody stands in the field, a corps of it already round the town is where it goes in.)
    const next = (c: Corps): boolean => c.town === undefined && (to.nb.includes(c.reg) || (!line.length && c.reg === to.id));
    const doors = (row.from ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && to.nb.includes(r.id) && !shut.has(r.id));
    for (const c of force) {
      if (next(c)) continue;
      // (One standing in the town of a province at the door comes out of it; any other is put down at the first door open.)
      const at = to.nb.includes(c.reg) ? g.regions[c.reg] : doors.find((r) => door(g, c, r));
      if (!at) continue;
      if (at.id !== c.reg) {
        // No corps is moved more than a month's march in a month (the design director's 11:41Z of 22 November
        // 2026): one further off by road marches its month toward the door, and the row is owed until it is up.
        // (One with no road over its side's ground is put down as before: its route is asked of the record.)
        if (g.flags[`bkm:${c.id}`] === g.turn) continue;
        const way = wayTo(g, c, at);
        if (way && way.months > 1) { if (!tanks.includes(c)) stride(g, row, c, way); continue; }
        broughtWatch.tell?.({ row, corps: c, from: g.regions[c.reg], to: at, months: way ? 1 : null });
      }
      bring(g, c, at);
    }
    if (!main.every(next)) return owed('not all of its corps stand next to the place');
    const round = main.filter((c) => c.reg === to.id);
    const led = [...round, ...limitAttackers(g, main.filter((c) => c.reg !== to.id), to)];
    if (!led.length) return owed('no road carries it in');
    // (The tanks take no room on the roads beside their divisions.)
    const pool = [...led, ...tanks.filter(next)];
    const no = line.length ? reckons(pool, false) : null;
    if (no) return owed(no);
    bookWatch.tell?.({ row, what: 'due', to, force: pool });
    if (!line.length) {
      // Nobody stands in the field there: it goes in unfought, as a landing on an empty shore does.
      // (Onto an island of the Pacific, as many of it as the island has room for, as the winners of a fight for it
      // would: the rest stay at the door. Rulings 173(b) and 179(s); islands.ts.)
      const isle = islandOf(g, to) !== null;
      for (const c of pool) if (c.reg !== to.id && (!isle || hasRoom(g, to, c))) { relocate(g, c, to); c.ap = 0; }
      tries();
      arrive(g, to, nat);
      g.supplyDirty = true;
      made();
      const fell = allied(g, nat, to.ctrl);
      bookWatch.tell?.({ row, what: 'made', to, force: pool, taken: fell, held: !fell });
      return;
    }
    tries();
    const report = row.met || row.ground ? fightMet(g, pool, to, row) : resolveBattle(g, pool, to);
    done(report.taken);
    bookWatch.tell?.({ row, what: 'made', to, force: pool, odds: oddsOf(report), taken: report.taken, held: !allied(g, nat, to.ctrl), report });
  });
}

/** True if a fleet of this nation based at this port would have that sea in its reach (navy.ts, fleetReach). */
function reaches(g: Game, nat: string, port: Region, sea: number): boolean {
  const d = new Map<number, number>(port.seas.map((z) => [z, 1]));
  const q = [...port.seas];
  for (let i = 0; i < q.length; i++) {
    if (q[i] === sea) return true;
    if (d.get(q[i])! >= NAVY.range) continue;
    for (const j of g.seas[q[i]].adj) if (!d.has(j) && canPass(g, q[i], j, nat)) { d.set(j, d.get(q[i])! + 1); q.push(j); }
  }
  return false;
}

/**
 * The record's fleets (riders b575 and b577): the orders the book gives a bound navy's admirals this month, ahead of
 * their own. A row with a sea sends its fleet there for its months; where the sea lies beyond the fleet's reach the
 * fleet first moves its base to the first of the row's ports that its side holds and that reaches it, and it is back
 * at its own base the month after its last (never in a month of its cover: a whole-month test, rider b560). A row
 * with no sea is a move of base and nothing more (the China Fleet to Ceylon). A fleet the book cannot get to its sea
 * is left to its admiral.
 */
export function bookFleets(g: Game, nat: string): { fleet: string; zone: string; stance: Stance }[] {
  const B = stations();
  if (!B?.fleets || !stationsBind(g, nat)) return [];
  const cal = calendar(g, B)!;
  if (!cal.went) return [];
  const out: { fleet: string; zone: string; stance: Stance }[] = [];
  for (const row of B.fleets) {
    if ((row.nat ?? B.taker) !== nat) continue;
    const f = g.fleets.find((x) => x.nat === nat && x.name === row.fleet);
    if (!f) continue;
    const from = row.month + cal.slide, to = (row.until ?? row.month) + cal.slide;
    if (g.turn < from) continue;
    const ports = (): Region[] => (row.base ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && r.port && r.seas.length > 0 && allied(g, nat, r.ctrl));
    const move = (r: Region): boolean => {
      const old: [number, string, number, number] = [f.base, f.port, f.home, f.station];
      // (The book's dated moves are the record's, made in their month as its own sailings are: no voyage. Release 2.)
      if (!rebase(g, f, r, true)) return false;
      f.bookBase ??= old;
      f.station = f.home;
      return true;
    };
    if (row.sea === undefined) {
      // A move of base: once, to the first of its ports its side holds, and from there its admiral's.
      const key = `bf:${nat}:${row.fleet}:${row.month}`;
      if (g.flags[key]) continue;
      const r = ports()[0];
      if (r && (r.id === f.base || move(r))) { g.flags[key] = g.turn; delete f.bookBase; }
      continue;
    }
    if (g.turn > to) {
      // Home, the month after its last.
      if (f.bookBase && g.turn === to + 1) {
        const [base, port, home, station] = f.bookBase;
        const r = base >= 0 ? g.regions[base] : null;
        if (r && rebase(g, f, r, true)) { f.port = port; f.home = home; f.zone = home; f.station = station; }
        delete f.bookBase;
      }
      continue;
    }
    const z = zoneOf(g, row.sea);
    if (z < 0) continue;
    if (!fleetReach(g, f).includes(z)) {
      const r = ports().find((x) => reaches(g, nat, x, z));
      if (r) move(r);
    }
    if (fleetReach(g, f).includes(z)) out.push({ fleet: row.fleet, zone: row.sea, stance: 'patrol' });
  }
  return out;
}
