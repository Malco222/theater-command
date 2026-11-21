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
//              Rangoon's trace, to ruling 234). While the place holds, the row stays owed and is asked again once a
//              month: where its corps already stand round the held town, its attack is the assault on it, by every
//              corps of its side fit to go in, as any storming is. The fight is the rules' own; nothing is dated and
//              nothing is forced. A row after it that starts from that place (`from`) waits until it is made: none
//              of its corps goes from there, and the place is no door for it.
//   beside     a row that goes with another (rider b581): Japan's tank regiments, `[month, corps]`, each with the
//              division the record gives it from that month. It is the book's: it stands where its division stands
//              in the field, goes in with every dated attack its division makes, and takes neither shipping of its
//              own nor room on the roads.
//              The first attachment the table gives is a fact of the day, at every stop of the dial and for every
//              country (rider b604): a row held for the corps it goes with lands in that corps' lift the month it
//              lands, and is its staff's own after. Later re-attachments stay the book's.
// 'fleets' are the record's covering forces (rider b575): in its months the fleet named patrols the sea named, by
// the book's order ahead of its admiral's own. What it meets there is fought by the rules.
// Never the player: his own corps are never stationed, and in a war where he is the invader nobody is (the line's
// rule, line.ts: computer against computer). Nor is a dated attack made on ground he holds. Stops 3 to 5 as before.
// The data is history.json, 'stations' (the far side's rows: far-pacific-war.json); with none, nothing here happens.

import { DEFENCE, allied, atWar, friendlyUnits, hostileUnits, limitAttackers, men, relocate, removeCorps, resolveBattle, withWorksAside, type AttackOpts } from './game';
import { HISTORY, bookDate, onBook, stopDef } from './history';
import { appoint, generalOf } from './commanders';
import { NAVY, canPass, fleetReach, landCorps, onOwnShipping, rebase, whyNotLand, zoneOf } from './navy';
import { arrive, besieged, fieldCtrl, storm, stormers, takeField } from './siege';
import { STACK, type BattleReport, type Corps, type Game, type Region, type Stance, type Terrain } from './types';

/** A month of the record, the place (or the places, the first its side holds), the months of works there, and what else. */
type Step = [month: number, place: string | string[], works?: number, more?: { joins?: string }];
interface Row { nat: string; corps: string; free?: boolean; coast?: boolean; beside?: [month: number, corps: string][]; steps: Step[]; worth?: [month: number, worth: number][] }
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

/** True where a corps can fall back to: ground its side holds, no enemy in the field, and a retreat's room. */
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
  into.str = men / divs;
  into.divs = divs;
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
    for (const c of g.corps) if (c.nat === nat) { if (c.bookWorks !== undefined) letGo(g, c); if (c.bookCoast) delete c.bookCoast; }
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
export const bookWatch: { tell: ((e: { row: Attack; what: 'due' | 'made' | 'owed' | 'over'; why?: string; to?: Region; force?: Corps[]; odds?: number; taken?: boolean; report?: BattleReport }) => void) | null } = { tell: null };

/** True where an attack's corps can be put down to go in from: ground whose field its side holds, no enemy in it, and room. */
const door = (g: Game, c: Corps, r: Region): boolean =>
  allied(g, c.nat, fieldCtrl(g, r)) && !hostileUnits(g, r, c.nat).length && (r.id === c.reg || friendlyUnits(g, r, c.nat).filter((u) => u.kind !== 'eng').length < STACK + 2);

/** Puts a corps of a dated attack down where it goes in from. False where it is shut in a town. */
function bring(g: Game, c: Corps, to: Region): boolean {
  if (c.town !== undefined && !takeField(g, c)) return false;
  if (c.reg !== to.id) { relocate(g, c, to); g.supplyDirty = true; }
  return true;
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
 */
export function bookAttacks(g: Game, nat: string): void {
  const B = stations();
  if (!B?.attacks || nat !== B.taker || !stationsBind(g, nat)) return;
  const cal = calendar(g, B)!;
  if (!cal.went) return;
  const riders = B.rows.filter((r) => r.nat === nat && r.beside);
  // (By their months, and within a month in the table's order, which is the record's date order: a row owed from an
  // earlier month goes in before the month's own, as the record's did: rider b599 (i).)
  const order = B.attacks.map((_, i) => i).sort((a, b) => B.attacks![a].month - B.attacks![b].month || a - b);
  order.forEach((i) => {
    const row = B.attacks![i];
    const key = `ba:${i}`;
    if (g.flags[key] || g.turn < row.month + cal.slide || !g.nations[nat].alive) return;
    // A fight at the record's figures that was lost this month, or a row whose place held this month, is not tried again in it.
    if (g.flags[`bat:${i}`] === g.turn) return;
    if (row.until !== undefined && g.turn > row.until + cal.slide) { g.flags[key] = g.turn; bookWatch.tell?.({ row, what: 'over', why: 'the record\'s attack was called off' }); return; }
    const to = row.at.map((nm) => g.byName[nm]).find((r) => !!r && !allied(g, nat, r.ctrl));
    if (!to) { g.flags[key] = g.turn; bookWatch.tell?.({ row, what: 'over', why: 'the place is its side\'s' }); return; }
    const force = row.force.map((nm) => g.corps.find((x) => x.nat === nat && !x.dead && x.name === nm)).filter((x): x is Corps => !!x);
    // The rows that go with another (the tank regiments, rider b581) go in with their divisions and wait for nobody.
    const tanks = force.filter((c) => riders.some((r) => r.corps === c.name)), main = force.filter((c) => !tanks.includes(c));
    const owed = (why: string): void => { bookWatch.tell?.({ row, what: 'owed', why, to, force }); };
    if (!main.length) return owed('none of its corps is in the field');
    if (!atWar(g, nat, to.ctrl)) return owed('no war with its holder');
    // Computer against computer: ground a player holds, or his corps, are met by the staff alone.
    if (g.human !== null && (to.ctrl === g.human || hostileUnits(g, to, nat).some((u) => u.nat === g.human))) return owed('the player holds it');
    for (const c of tanks) follow(g, riders.find((r) => r.corps === c.name)!, cal.slide);
    const line = hostileUnits(g, to, nat);
    // Made only when its place is taken; while the place holds it stays owed, and is asked again next month.
    const made = (): boolean => {
      if (allied(g, nat, to.ctrl)) { g.flags[key] = g.turn; return true; }
      g.flags[`bat:${i}`] = g.turn;
      return false;
    };
    // (A fight at the record's figures that is won still sends the line it broke back by the book, the town held or not.)
    const done = (won: boolean): void => {
      made();
      if (won && row.met) fallBack(g, B, cal, line, row.back);
    };
    const oddsOf = (rep: BattleReport | null | undefined): number | undefined => (rep ? rep.aPower / Math.max(rep.dPower, 1e-9) : undefined);
    // A row before it that is owed, its place held, keeps this one from starting from that place.
    const shut = new Set<number>();
    for (const j of order) {
      if (j === i) break;
      const r = B.attacks![j];
      if (g.flags[`ba:${j}`] || g.turn < r.month + cal.slide || (r.until !== undefined && g.turn > r.until + cal.slide)) continue;
      const at = r.at.map((nm) => g.byName[nm]).find((x) => !!x && !allied(g, nat, x.ctrl));
      if (at && (row.from ?? []).some((nm) => g.byName[nm] === at)) shut.add(at.id);
    }
    const stuck = main.find((c) => shut.has(c.reg));
    if (stuck) return owed(`it starts from ${g.regions[stuck.reg].name}, which holds: the row before it is owed`);
    // Its corps stand round the town, held, that it is owed: its attack this month is the assault on it.
    if (!line.length && besieged(g, to) && main.some((c) => c.reg === to.id && c.town === undefined)) {
      const round = (c: Corps): boolean => c.reg === to.id && c.town === undefined;
      const nextTo = (c: Corps): boolean => c.town === undefined && to.nb.includes(c.reg);
      if (!main.every((c) => round(c) || nextTo(c))) return owed('not all of its corps stand round the town or next to it');
      for (const c of [...main, ...tanks]) if (!round(c) && nextTo(c)) { relocate(g, c, to); c.ap = 0; g.supplyDirty = true; }
      const going = stormers(g, to, nat);
      bookWatch.tell?.({ row, what: 'due', to, force: going });
      const report = storm(g, going, to);
      if (!report) { g.flags[`bat:${i}`] = g.turn; return owed('the town cannot be stormed this month'); }
      made();
      bookWatch.tell?.({ row, what: 'made', to, force: going, odds: oddsOf(report), taken: allied(g, nat, to.ctrl), report });
      return;
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
        const no = onOwnShipping(nat, () => whyNotLand(g, main, to));
        if (no) { ready(false); why = no; continue; }
        // (The tanks sail with their divisions, and take no shipping of their own.)
        const aboard = tanks.filter((c) => door(g, c, q) && bring(g, c, q));
        bookWatch.tell?.({ row, what: 'due', to, force: [...main, ...aboard] });
        // (A landing with the record's figures came ashore on an empty beach: its fight is inland, with no cut for the shore.)
        const res = onOwnShipping(nat, () => landCorps(g, main, to, row.met ? (cs, at) => fightMet(g, [...cs, ...aboard], at, row, { sea: 1 }) : undefined, true));
        if (!res.ok) { ready(false); why = res.why ?? 'the landing could not sail'; continue; }
        for (const c of main) g.flags[`bsl:${c.id}`] = g.turn;
        const won = !!res.taken;
        if (won) for (const c of aboard) if (!c.dead && c.reg !== to.id && door(g, c, to)) relocate(g, c, to);
        if (won || !row.met) for (const c of main) if (!c.dead) c.hold = false;
        done(won);
        bookWatch.tell?.({ row, what: 'made', to, force: [...main, ...aboard], odds: oddsOf(res.report), taken: won, report: res.report ?? undefined });
        return;
      }
      if (quays.length && !again.length) main.every((c) => bring(g, c, quays[0]));
      return owed(why);
    }
    const next = (c: Corps): boolean => c.town === undefined && to.nb.includes(c.reg);
    const doors = (row.from ?? []).map((nm) => g.byName[nm]).filter((r): r is Region => !!r && to.nb.includes(r.id) && !shut.has(r.id));
    for (const c of force) {
      if (next(c)) continue;
      // (One standing in the town of a province at the door comes out of it; any other is put down at the first door open.)
      const at = to.nb.includes(c.reg) ? g.regions[c.reg] : doors.find((r) => door(g, c, r));
      if (at) bring(g, c, at);
    }
    if (!main.every(next)) return owed('not all of its corps stand next to the place');
    const led = limitAttackers(g, main, to);
    if (!led.length) return owed('no road carries it in');
    // (The tanks take no room on the roads beside their divisions.)
    const pool = [...led, ...tanks.filter(next)];
    bookWatch.tell?.({ row, what: 'due', to, force: pool });
    if (!line.length) {
      // Nobody stands in the field there: it goes in unfought, as a landing on an empty shore does.
      for (const c of pool) { relocate(g, c, to); c.ap = 0; }
      arrive(g, to, nat);
      g.supplyDirty = true;
      const taken = made();
      bookWatch.tell?.({ row, what: 'made', to, force: pool, taken });
      return;
    }
    const report = row.met || row.ground ? fightMet(g, pool, to, row) : resolveBattle(g, pool, to);
    done(report.taken);
    bookWatch.tell?.({ row, what: 'made', to, force: pool, odds: oddsOf(report), taken: report.taken, report });
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
      if (!rebase(g, f, r)) return false;
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
        if (r && rebase(g, f, r)) { f.port = port; f.home = home; f.zone = home; f.station = station; }
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
