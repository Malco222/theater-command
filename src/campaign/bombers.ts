// Bombers by name.
//
// Brian, 20 November 2026: "i would like the game to keep track of real bombers names and how many missions they have
// completed and fighters shot down"; "after a bomber does 25 missions they complete their tour of duty"; "after their
// tour of duty it raises airforce morale and you can send them to either help train new bomber crews which gives the
// new bombers more experience points before even flying 1 mission or they can go back to help sell more war bonds
// which gives you more cash bonus for 5-10 turns".
//
// And, the same day: "i want the same mechanics I talked about bombing to be applied for all nations not just the US
// which I was using as an example", "nose art only for US bombers, other countries didnt do that", "but i still want
// yellow painted bombs for each bombing mission they did and then the symbols showing how many aircraft they shot down".
//
// A bomber group already keeps a record as a whole (stories.ts: its missions, its kills, the 'Twenty-five missions'
// honour). Here its bombers are kept one by one, for every nation. Each has what it is known by (an American bomber a
// name from the nose-art list, data/battle/nose-art.json; any other the marking its air force painted, bomber-marks.ts),
// the missions it has flown, the aircraft its gunners have shot down and whose they were, and a crew with its own
// count of missions. The
// group's numbers are the campaign's and nothing here moves them: when the campaign says a group lost six bombers, this
// says which six, and when it says the group flew a month of missions, this says who flew how many.
//
// The tour is the crew's (the historian): after its nation's tour ('tours': 25 missions for America, 30 operations for
// Britain; only the Western Allies sent crews home in the war itself, and in this game every nation does) a crew goes home. Sent to the schools it leaves its
// bomber to a new crew, and the bomber flies on under the same name with its tally; sent on a war bond tour it flies
// its bomber home with it, and a new aircraft takes the place.
//
// The stories' one rule holds (stories.ts): every nation keeps these records, but only the player's own gain anything
// by them. For the player a finished tour lifts the group's readiness, a crew at the schools
// gives every later replacement experience before its first mission (and fighters bring down fewer of a group so
// trained), and a crew on a bond tour pays the treasury each month. Nothing here draws on the dice of the war: every
// draw is from a stream of the group's own.

import data from '../../data/campaign/stories.json';
import { Rng, hashString } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { NOSE_ART, markOf, type BomberMark, type MarkStyle } from './bomber-marks';
import type { AirFleet, BomberRoster, BomberRow, CrewOrder, CrewsHome, Game } from './types';

export { NOSE_ART };

export const BOMBERS = (data as unknown as {
  bombers: {
    names: Record<string, string>;
    tours: Record<string, number>;
    flown: Record<string, string>;
    fit: number; bare: number;
    risk: [number, number][];
    gunners: number; told: number; roll: number;
    lift: { each: number; most: number };
    bonds: { pp: number; months: [number, number]; most: number };
    school: { crews: number; most: number; edge: number };
  };
}).bombers;
const GRADES = (data as unknown as { grades: [number, string][] }).grades;
const PER_MONTH = (data as unknown as { tour: { perMonth: number } }).tour.perMonth;

/** How a nation's bombers are told apart: 'nose' is a name and nose art, and America's alone. */
export const styleOf = (nat: string): MarkStyle => (BOMBERS.names[nat] ?? BOMBERS.names._default ?? 'number') as MarkStyle;

/** True for a nation whose bombers carry a name and nose art. */
export const carriesNames = (nat: string): boolean => styleOf(nat) === 'nose';

/** How many missions a nation's crews fly before they go home. */
export const tourOf = (nat: string): number => BOMBERS.tours[nat] ?? BOMBERS.tours._default;

/** What a nation calls one of them: a mission, or in Britain an operation. */
export const flownWord = (nat: string): string => BOMBERS.flown[nat] ?? BOMBERS.flown._default;

/** True for a group whose bombers are kept one by one: any nation's, flying from airfields. A carrier's aircraft are not. */
export const keptByName = (f: AirFleet): boolean => !!(BOMBERS.names[f.nat] ?? BOMBERS.names._default) && f.ship === undefined && f.ashore === undefined;

/** What a bomber carries, as it was in the month 'turn': its name and its marking (bomber-marks.ts). Empty for one not yet named. */
export const bomberMark = (g: Game, f: AirFleet, b: BomberRow, turn = g.turn): BomberMark =>
  markOf(styleOf(f.nat), `${g.seed}:${f.id}`, b[0], b[1], turnToDate(turn).year);

/** What a bomber is called: 'Prize Pumpkin', 'G for George', '5J+DM'. '' for one with no name yet. */
export const bomberName = (g: Game, f: AirFleet, b: BomberRow, turn = g.turn): string => bomberMark(g, f, b, turn).name;

/** A bomber's row, copied so that what becomes of the bomber afterwards does not change the copy. */
export const copyRow = (b: BomberRow): BomberRow => (b[6] ? [b[0], b[1], b[2], b[3], b[4], b[5], { ...b[6] }] : [b[0], b[1], b[2], b[3], b[4], b[5]]);

/** A crew's experience: the missions it has flown and what it arrived with from the schools. */
export const crewExperience = (b: BomberRow): number => b[5] + b[4];

/** What that experience is called, on the scale every unit's is (stories.json 'grades'), a full tour being the top of it. */
export function crewGrade(b: BomberRow, nat: string): string {
  const x = Math.min(1, crewExperience(b) / tourOf(nat));
  let out = GRADES[0][1];
  for (const [from, name] of GRADES) if (x >= from) out = name;
  return out;
}

/** A stream of the group's own for its next draw: the same war always draws the same. */
const draw = (g: Game, f: AirFleet, R: BomberRoster): Rng => new Rng(hashString(`bombers:${g.seed}:${f.id}:${R.n++}`));

/** The order in which a group takes names from the list: its own shuffle, so no two groups paint the same run. */
const orders = new Map<string, number[]>();
function nameOrder(g: Game, f: AirFleet): number[] {
  const key = `${g.seed}:${f.id}`;
  let o = orders.get(key);
  if (!o) {
    const rng = new Rng(hashString(`noseart:${key}`));
    o = Array.from({ length: NOSE_ART }, (_, i) => i + 1);
    for (let i = o.length - 1; i > 0; i--) { const j = rng.int(i + 1); [o[i], o[j]] = [o[j], o[i]]; }
    if (orders.size > 400) orders.clear();
    orders.set(key, o);
  }
  return o;
}

/**
 * A crew names its bomber: the next name on the group's list that none of its bombers now carries, one never used in
 * the group before a second of a name ('II'). A command with more bombers than there are names has two of a name
 * flying at once, the later one 'II', as its groups would. A bomber of a nation that paints no names takes the first
 * marking in the group that no bomber now carries, so a replacement has the letter of the one it replaces.
 */
function christen(g: Game, f: AirFleet, R: BomberRoster, b: BomberRow): void {
  const flying = new Map<number, number>();
  for (const x of R.on) if (x[0]) flying.set(x[0], (flying.get(x[0]) ?? 0) + 1);
  let best = 0;
  if (carriesNames(f.nat)) {
    let least = Infinity;
    for (const a of nameOrder(g, f)) {
      const u = (flying.get(a) ?? 0) * 1000 + (R.used[a] ?? 0);
      if (u < least) { least = u; best = a; if (!u) break; }
    }
  } else {
    for (best = 1; flying.has(best); best++);
  }
  b[0] = best;
  b[1] = R.used[best] = (R.used[best] ?? 0) + 1;
}

/**
 * Makes a roster up to its group's strength with new bombers, whose crews arrive with what the schools gave them. A
 * named bomber is named by its crew once it has flown; any other is marked the day it arrives.
 */
function fill(g: Game, f: AirFleet, R: BomberRoster): void {
  const head = headStart(g, f.nat), named = carriesNames(f.nat);
  while (R.on.length < f.bmr) R.on.push([0, 0, 0, 0, head, 0]);
  for (const b of R.on) if (!b[0] && (b[2] > 0 || !named)) christen(g, f, R, b);
}

/** Where a nation's finished crews go and what those at home are doing. Made on first use. */
export function crewsHome(g: Game, nat: string): CrewsHome {
  const all = (g.stories.crews ??= {});
  return (all[nat] ??= { order: 'split', school: 0, bonds: [], n: 0, sold: 0 });
}

/** The experience a new bomber's crew arrives with: the player's instructors' doing, and nobody else's. */
export function headStart(g: Game, nat: string): number {
  if (nat !== g.human) return 0;
  const c = g.stories.crews?.[nat];
  return c ? Math.min(BOMBERS.school.most, Math.floor(c.school / BOMBERS.school.crews)) : 0;
}

const riskOf = (b: BomberRow): number => {
  const x = crewExperience(b);
  let w = 1;
  for (const [from, weight] of BOMBERS.risk) if (x >= from) w = weight;
  return w;
};

/** Which bomber a loss falls on: any of them, the green crews likeliest. */
function unlucky(rng: Rng, on: BomberRow[]): number {
  let all = 0;
  for (const b of on) all += riskOf(b);
  let x = rng.next() * all;
  for (let i = 0; i < on.length; i++) { x -= riskOf(on[i]); if (x < 0) return i; }
  return on.length - 1;
}

const same = (a: BomberRow, b: BomberRow): boolean => a[0] === b[0] && a[1] === b[1] && a[2] === b[2] && a[3] === b[3] && a[4] === b[4] && a[5] === b[5];

function enrol(R: BomberRoster, t: number, how: 'home' | 'lost', b: BomberRow): void {
  R.roll.push({ t, how, b: copyRow(b) });
  if (R.roll.length > BOMBERS.roll) R.roll.splice(0, R.roll.length - BOMBERS.roll);
}

/**
 * Makes a group's roster up to date with the group: struck off are the bombers lost since it was last made up (those a
 * fought battle saw fall, 'fallen', and for the rest a draw); replacements join without a name; and a crew that has
 * flown names its bomber. The first time, the group is mustered as it stands. Returns the bombers struck off.
 */
export function muster(g: Game, f: AirFleet, fallen: BomberRow[] = []): BomberRow[] {
  if (!keptByName(f)) return [];
  const B = BOMBERS;
  let R = f.bombers;
  if (!R) {
    if (f.bmr <= 0) return [];
    R = f.bombers = { on: [], seen: f.lost.bmr, used: {}, home: 0, gone: 0, roll: [], n: 0 };
    const rng = draw(g, f, R);
    // The group as it stands: each bomber with a share of what the group has flown so far, and most with a name.
    const so = Math.min(tourOf(f.nat) - 1, f.rec?.missions ?? 0), named = carriesNames(f.nat);
    for (let i = 0; i < f.bmr; i++) { const m = so > 0 ? rng.int(so + 1) : 0; R.on.push([0, 0, m, 0, 0, m]); }
    for (const b of R.on) if (b[2] > 0 || !named || !rng.chance(B.bare)) christen(g, f, R, b);
    return [];
  }
  const struck: BomberRow[] = [];
  let dead = Math.max(0, f.lost.bmr - R.seen);
  R.seen = f.lost.bmr;
  for (const want of fallen) {
    if (dead <= 0) break;
    const i = R.on.findIndex((b) => same(b, want));
    if (i < 0) continue;
    struck.push(...R.on.splice(i, 1));
    dead--;
  }
  if (dead > 0 && R.on.length) {
    const rng = draw(g, f, R);
    for (; dead > 0 && R.on.length; dead--) struck.push(...R.on.splice(unlucky(rng, R.on), 1));
  }
  for (const b of struck) if (b[0]) { R.gone++; if (b[2] >= B.told) enrol(R, g.turn, 'lost', b); }
  // Bombers sent to the reserve or to another group: those with no name first, then the last to join. Their names are free again.
  while (R.on.length > Math.max(0, f.bmr)) {
    let i = R.on.length - 1;
    for (let k = R.on.length - 1; k >= 0; k--) if (!R.on[k][0]) { i = k; break; }
    const [b] = R.on.splice(i, 1);
    if (b[0] && R.used[b[0]] === b[1]) { if (b[1] > 1) R.used[b[0]] = b[1] - 1; else delete R.used[b[0]]; }
  }
  fill(g, f, R);
  return struck;
}

/** A bomber's gunners shoot down an aircraft: one more on its tally, and on the count of that nation's it has shot down. */
export function credit(b: BomberRow, foe?: string): void {
  b[3]++;
  if (foe) { const by = (b[6] ??= {}); by[foe] = (by[foe] ?? 0) + 1; }
}

/** What became of a month's finished crews: the bombers as they stood, and for the player where the crews went. */
export interface ToursDone {
  done: BomberRow[];
  /** What each of them is called, in the same order ('' for one with no name). */
  names: string[];
  /** The nation's tour, and what it calls one mission of it. */
  tour: number;
  flown: string;
  /** True where the bombers are named ones, which their crews call 'her'. */
  named: boolean;
  school: number;
  bonds: number;
  /** What the group's readiness rose by. */
  lift: number;
}

/**
 * A group has flown a month of bombing: each bomber flies most of its missions, the group's gunners' kills go to
 * bombers of the group, and a crew that reaches the end of its tour goes home. The player's crews go where his
 * standing order sends them (crewsHome().order), and their going lifts the group; a computer nation's go to its
 * schools, the bomber flying on, and nothing else changes.
 */
export function flownMonth(g: Game, f: AirFleet, kills: number, foe?: string): ToursDone | null {
  const R = f.bombers;
  if (!R || !R.on.length) return null;
  const B = BOMBERS, rng = draw(g, f, R), tour = tourOf(f.nat);
  for (const b of R.on) {
    let n = 0;
    for (let i = 0; i < PER_MONTH; i++) if (rng.chance(B.fit)) n++;
    n = Math.min(n, tour - b[5]);
    b[2] += n; b[5] += n;
  }
  for (let k = 0; k < kills; k++) credit(R.on[rng.int(R.on.length)], foe);
  const done = R.on.filter((b) => b[5] >= tour);
  if (!done.length) return null;
  const mine = f.nat === g.human;
  const out: ToursDone = { done: done.map(copyRow), names: done.map((b) => bomberName(g, f, b)), tour, flown: flownWord(f.nat), named: carriesNames(f.nat), school: 0, bonds: 0, lift: 0 };
  const C = mine ? crewsHome(g, f.nat) : null;
  const schoolFull = (): boolean => !C || C.school >= B.school.crews * B.school.most;
  const bondsFull = (): boolean => !C || C.bonds.length >= B.bonds.most;
  const head = headStart(g, f.nat);
  const home: BomberRow[] = [];
  for (const b of done) {
    R.home++;
    enrol(R, g.turn, 'home', b);
    // The player's standing order; where the one is full, the other; where both are, the crew goes home and that is all.
    let to: 'school' | 'bonds' | null = null;
    if (C) {
      to = C.order === 'split' ? (C.n % 2 ? 'bonds' : 'school') : C.order;
      C.n++;
      if (to === 'school' && schoolFull()) to = bondsFull() ? null : 'bonds';
      else if (to === 'bonds' && bondsFull()) to = schoolFull() ? null : 'school';
    }
    if (to === 'bonds') {
      // The crew flies its bomber home to be seen: a new aircraft will take its place.
      C!.bonds.push(B.bonds.months[0] + rng.int(B.bonds.months[1] - B.bonds.months[0] + 1));
      out.bonds++;
      home.push(b);
    } else {
      if (to === 'school') { C!.school++; out.school++; }
      // The bomber flies on with a new crew, under the same name and with its tally.
      b[4] = head; b[5] = 0;
    }
  }
  if (home.length) {
    R.on = R.on.filter((b) => !home.includes(b));
    fill(g, f, R);
  }
  if (mine) {
    const had = R.lift && R.lift[0] === g.turn ? R.lift[1] : 0;
    out.lift = Math.max(0, Math.min(B.lift.most - had, B.lift.each * done.length, 1 - f.ready));
    f.ready = Math.min(1, f.ready + out.lift);
    R.lift = [g.turn, had + out.lift];
  }
  return out;
}

/** The player's bond tours pay the treasury for the month, and each is a month nearer its end. Returns what they paid. */
export function bondsMonth(g: Game, nat: string): number {
  const C = nat === g.human ? g.stories.crews?.[nat] : undefined;
  if (!C || !C.bonds.length) return 0;
  const pay = BOMBERS.bonds.pp * C.bonds.length;
  g.nations[nat].pp += pay;
  C.sold += pay;
  C.bonds = C.bonds.map((m) => m - 1).filter((m) => m > 0);
  return pay;
}

/**
 * How many fewer of one of the player's groups the fighters bring down for its crews' training: 'school.edge' for a
 * group whose crews all arrived with the most the schools give, and in proportion for less. 0 for everyone else, and
 * for the player until he has sent a crew to the schools.
 */
export function trainedEdge(g: Game, f: AirFleet): number {
  const R = f.bombers;
  if (f.nat !== g.human || !R || !R.on.length) return 0;
  let h = 0;
  for (const b of R.on) h += b[4];
  return (BOMBERS.school.edge * h) / (R.on.length * BOMBERS.school.most);
}

/** The player's standing order for crews who finish a tour. */
export function setCrewOrder(g: Game, how: CrewOrder): boolean {
  if (!g.human) return false;
  crewsHome(g, g.human).order = how;
  return true;
}

/** A group's bombers for a card: the old hands first. */
export function bomberCard(g: Game, f: AirFleet): { name: string; mark: string; missions: number; kills: number; crew: number; grade: string; b: BomberRow }[] {
  return (f.bombers?.on ?? [])
    .map((b) => { const m = bomberMark(g, f, b); return { name: m.name, mark: m.mark, missions: b[2], kills: b[3], crew: b[5], grade: crewGrade(b, f.nat), b }; })
    .sort((x, y) => y.missions - x.missions || y.kills - x.kills || x.name.localeCompare(y.name));
}
