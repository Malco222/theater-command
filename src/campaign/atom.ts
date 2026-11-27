// The atomic project (release 2; Brian 25 November 2026 01:14Z, "lets do number 1": a late, very expensive research
// project that any big industrial power can attempt). The design director's ruling of 25 November 01:27Z
// (design-director/atom-1125.md) on the builder's plan (atom-r2/PLAN.md), under ruling 230 (4e): plain, sober words, no
// spectacle, no count of the dead; its effect is a weight on the enemy's will to fight through the surrender rules the
// game already has, never a city-burning loop.
//
// - Three rows of the Industry branch (tech.json, marked 'atom'): Atomic physics, Chain reaction, Atomic weapon design.
//   They follow the tree's one rule like every other row.
// - The atomic plants: not research but a share of the nation's earnings drawn every month (a line in its ledger,
//   economy.ts upkeep) toward one fixed bill. The same bill takes a large industry two years and a small one forever:
//   the gate is the size of the industry, never the nation. They stand at one home site, chosen by the player (the
//   staff's choice by default: the safest home province with industry), and work less as that town is ruined.
// - The bombs: ready when the plants and the weapon design are both done; one, then one more every few months of
//   making, never more than a couple in hand.
// - A strike is armed against a city and flown as an ordinary bombing raid on it (air.ts fly): the air rules' own losses
//   decide whether the bomber that carries it reaches the release. Never a capital (a government must be left to give
//   in), never a mere town. What it does: the town in ruins (citydamage.ts, its most), and a weight on the struck
//   country's will (resources.ts willLost). For some months after, that country may ask for terms though its capital
//   stands and it holds most of its home (game.ts checkCollapse), the will test itself unchanged.
// - Counterplay: sabotage on the plants' province sets them back (intel.ts); bombing the town slows them; losing the
//   province loses them, and the captor takes the papers. A staff knows another's plants only at its own sight.
// - The computer: at the book's stops only the record's nation, on the record's clock; elsewhere a staff takes it up
//   only in a long war it can afford, and stops while in debt or near collapse.
//
// The grand campaign only, behind the switch (stage.json 'atomBomb', the flag 'atom'), switchable in any month. Off:
// the rows leave the tree, the plants draw nothing and nothing here is read. All numbers: data/campaign/atom.json.

import data from '../../data/campaign/atom.json';
import { Rng, hashString } from '../core/rng';
import type { AtomPace, AtomState, Game, Nation, Region } from './types';
import { allied, atWar, collapseScore, headline, homeOf, theName } from './game';
import { income } from './economy';
import { grantTech, hasTech, techById, techOf, techRules } from './tech';
import { DAMAGE, damageOf } from './citydamage';
import { ruinHit, type CityHit } from './repairs';
import { ruinedAt } from './rails';
import { acOn, heldDown } from './aircampaigns';
import { stopDef } from './history';
import { townName, townSize } from './towns';
import { PLANS } from './plans';
import { airFleetsOf, airTargets } from './air';
import type { AirFleet } from './types';

export const ATOM = data as unknown as {
  rows: { physics: string; chain: string; weapon: string };
  plants: { bill: number; pace: Record<AtomPace, number>; make: number; every: number; most: number; site: { fac: number } };
  strike: { need: string; ruin: number };
  will: { first: number; second: number; later: number; most: number; press: number };
  sabotage: { months: number; weight: number };
  raid: { worth: number };
  ai: { war: number; months: number; calm: number; first: number };
  book: { nat: string; physics: number; chain: number; plants: number; ready: number; use: number };
};

export const PACES: AtomPace[] = ['steady', 'full', 'crash'];

/** True in a grand campaign with the atomic project switched on. */
export const atomOn = (g: Game): boolean => g.flags.open === 1 && g.flags.atom === 1;

/** Switches the atomic project on or off, in any month of a grand campaign. Off, the plants stop drawing and the rows leave the tree. */
export function setAtom(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1) return false;
  if (on) g.flags.atom = 1; else delete g.flags.atom;
  // (Off: the rows leave the tree at once, and a team at work on one is free for other work this same month.)
  if (!on) for (const n of Object.values(g.nations)) if (n.tech?.work.some((w) => isAtomRow(techById(w.id)))) n.tech.work = n.tech.work.filter((w) => !isAtomRow(techById(w.id)));
  return true;
}

/** One of the three rows of the tree that belong to the project. */
export const isAtomRow = (t: { atom?: number } | undefined): boolean => !!t?.atom;

const stateOf = (n: Nation): AtomState => (n.atom ??= {});
const paceOf = (s: AtomState | undefined): AtomPace => s?.pace ?? 'full';
const shareOf = (s: AtomState | undefined): number => ATOM.plants.pace[paceOf(s)];

/** True for a nation whose staff is held to the book's clock: a computer-run nation at the stops that follow the book. */
const onBook = (g: Game, nat: string): boolean => !!stopDef(g).book && nat !== g.human;

// ---- Where the plants stand ---------------------------------------------------------------------------

/** True where a nation could put its plants: its own home ground, held, not overseas, with industry. */
export function siteFits(g: Game, nat: string, r: Region | undefined): boolean {
  return !!r && r.owner === nat && r.ctrl === nat && homeOf(g, r) === nat && r.fac >= ATOM.plants.site.fac;
}

/** The provinces a nation could put its plants in, safest first: furthest from any ground not its side's, then most factories. */
export function siteChoices(g: Game, nat: string): { region: number; hops: number }[] {
  const far = new Map<number, number>();
  const q: number[] = [];
  for (const r of g.regions) if (!allied(g, nat, r.ctrl)) { far.set(r.id, 0); q.push(r.id); }
  for (let i = 0; i < q.length; i++) {
    const d = far.get(q[i])!;
    for (const j of g.regions[q[i]].nb) if (!far.has(j)) { far.set(j, d + 1); q.push(j); }
  }
  return g.regions.filter((r) => siteFits(g, nat, r))
    .map((r) => ({ region: r.id, hops: far.get(r.id) ?? 99, fac: r.fac }))
    .sort((a, b) => b.hops - a.hops || b.fac - a.fac || a.region - b.region)
    .map(({ region, hops }) => ({ region, hops }));
}

/** The staff's choice of site: the safest home province with industry, or null if there is none. */
export const safestSite = (g: Game, nat: string): number | null => siteChoices(g, nat)[0]?.region ?? null;

/** Where the plants will stand: the site chosen, while it still fits, else the staff's choice (the safest). */
export function chosenSite(g: Game, nat: string): number | null {
  const s = g.nations[nat]?.atom?.site;
  return s !== undefined && siteFits(g, nat, g.regions[s]) ? s : safestSite(g, nat);
}

/** The nation whose plants stand in this province and are not yet finished, or null. */
export function plantsAt(g: Game, r: Region): string | null {
  if (!atomOn(g)) return null;
  for (const n of Object.values(g.nations)) {
    const s = n.atom;
    if (n.alive && s?.plants !== undefined && s.done === undefined && s.site === r.id) return n.id;
  }
  return null;
}

/** The share of the plants' work lost to the town's ruin, its wrecked factories, and (with the air campaigns) its works held down: the largest of them. */
export function siteHarm(g: Game, r: Region): number {
  const ruined = r.fac > 0 ? ruinedAt(g, r) / r.fac : 0;
  const held = acOn(g) ? heldDown(g, r, 'works') : 0;
  return Math.max(0, Math.min(1, Math.max(damageOf(g, r), ruined, held)));
}

// ---- What it costs and how long it takes ------------------------------------------------------------------

/** What the plants draw from the nation's earnings in a month at this pace. */
export const drawAt = (g: Game, nat: string, pace: AtomPace): number => ATOM.plants.pace[pace] * Math.max(0, income(g, nat));

/** Months the plants would need from now at this pace and today's earnings, or null where they would never be done. */
export function monthsLeft(g: Game, nat: string, pace: AtomPace = paceOf(g.nations[nat]?.atom)): number | null {
  const s = g.nations[nat]?.atom;
  const left = ATOM.plants.bill - (s?.plants ?? 0);
  const a = drawAt(g, nat, pace);
  return left <= 0 ? 0 : a > 1e-9 ? Math.ceil(left / a - 1e-9) : null;
}

/** What the project draws from the nation's earnings this month (economy.ts upkeep): the plants, or the making of more bombs. */
export function atomBill(g: Game, nat: string): number {
  if (!atomOn(g)) return 0;
  const p = g.nations[nat]?.atom?.paid;
  return p && p[0] === g.turn ? p[1] : 0;
}

// ---- The player's (and the staffs') orders -------------------------------------------------------------

/** Why this nation cannot begin its plants now, or null. */
export function whyNotPlants(g: Game, nat: string, site?: number): string | null {
  if (!atomOn(g) || !techRules(g)) return 'The atomic project is not in this campaign.';
  const n = g.nations[nat];
  if (!n?.alive) return 'The nation is out of the war.';
  if (n.atom?.plants !== undefined) return 'The plants are already started.';
  if (!hasTech(g, nat, ATOM.rows.chain)) return 'Needs Chain reaction first.';
  const at = site ?? chosenSite(g, nat);
  if (at === null || at === undefined || !siteFits(g, nat, g.regions[at])) return 'There is no province of your own at home with industry to build them in.';
  return null;
}

/** Begins the plants at a site (the staff's choice if none is given), at a pace. */
export function beginPlants(g: Game, nat: string, site?: number, pace?: AtomPace): boolean {
  if (whyNotPlants(g, nat, site) !== null) return false;
  const s = stateOf(g.nations[nat]);
  s.site = site ?? chosenSite(g, nat)!;
  s.plants = 0;
  if (pace) s.pace = pace;
  if (nat === g.human) headline(g, 'info', `Work has started on the atomic plants ${inAt(g.regions[s.site])}. They take ${pct(shareOf(s))} of the nation's earnings every month and, at today's earnings, ${finishWords(monthsLeft(g, nat))}.`, nat, s.site);
  return true;
}

/** Chooses where the plants will stand, before they are started. */
export function chooseSite(g: Game, nat: string, region: number): boolean {
  const n = g.nations[nat];
  if (!atomOn(g) || !n?.alive || n.atom?.plants !== undefined || !siteFits(g, nat, g.regions[region])) return false;
  stateOf(n).site = region;
  return true;
}

/** Sets the pace the plants draw at: steady, full or crash. */
export function setPace(g: Game, nat: string, pace: AtomPace): boolean {
  const n = g.nations[nat];
  if (!atomOn(g) || !n?.alive || !PACES.includes(pace)) return false;
  stateOf(n).pace = pace;
  return true;
}

/** Stops the plants drawing, or lets them draw again. Work done stays done. */
export function pausePlants(g: Game, nat: string, paused: boolean): boolean {
  const n = g.nations[nat];
  if (!atomOn(g) || !n?.alive || n.atom?.plants === undefined) return false;
  if (paused) n.atom.paused = 1; else delete n.atom.paused;
  return true;
}

/** Stops the finished plants making more bombs, or lets them make more. */
export function setMaking(g: Game, nat: string, on: boolean): boolean {
  const n = g.nations[nat];
  if (!atomOn(g) || !n?.alive || n.atom?.done === undefined) return false;
  if (on) delete n.atom.stopMaking; else n.atom.stopMaking = 1;
  return true;
}

// ---- The month ----------------------------------------------------------------------------------------

/**
 * A nation's month for its project, as its month opens (game.ts startPhase, after the laboratories): the book's clock
 * for a computer-run nation at stops 1-2; a lost site; the plants' work and what it draws; the bombs made.
 */
export function atomMonth(g: Game, nat: string): void {
  if (!atomOn(g)) return;
  const n = g.nations[nat];
  if (!n?.alive || !techRules(g)) return;
  if (onBook(g, nat)) { bookMonth(g, n); if (!n.atom) return; }
  const s = n.atom;
  if (!s) return;
  // A site lost: the plants stand on the ground, so they go with it, finished or not, and the captor takes the papers.
  if (s.plants !== undefined && s.site !== undefined && !allied(g, nat, g.regions[s.site].ctrl)) {
    plantsLost(g, n, g.regions[s.site], g.regions[s.site].ctrl);
    return;
  }
  // (On the book's clock the plants are the record's; once done, the bombs come as anyone's.)
  if (onBook(g, nat)) { if (s.done !== undefined) bombMonth(g, n); return; }
  // The plants: a share of the month's earnings, worked as well as the town lets them.
  if (s.plants !== undefined && s.done === undefined) {
    if (s.paused) return;
    if (n.pp < 0) { if (nat === g.human && g.flags[`atom:idle:${nat}`] !== g.turn - 1) headline(g, 'info', 'The atomic plants stand idle: the treasury is in debt. They start again once it is out of debt.', nat); g.flags[`atom:idle:${nat}`] = g.turn; return; }
    const paid = drawAt(g, nat, paceOf(s));
    s.paid = [g.turn, paid];
    s.plants += paid * (1 - siteHarm(g, g.regions[s.site!]));
    if (s.plants >= ATOM.plants.bill - 1e-9) {
      s.plants = ATOM.plants.bill;
      s.done = g.turn;
      // (With the design done the first bomb is ready this same month, and its line says it: the UI director.)
      if (nat === g.human && !hasTech(g, nat, ATOM.rows.weapon)) headline(g, 'info', 'The atomic plants are finished. The bomb waits on Atomic weapon design.', nat, s.site);
    }
  }
  bombMonth(g, n);
}

/**
 * The plants lost with their ground, finished or not: the papers to the one who holds it now, told at the player's
 * sight (133(a): his own side's plants, plants he knew of, or ground his side has taken; the captor's own line, where
 * the papers give him rows he lacked, in place of the other). Bombs already made stay in hand.
 */
function plantsLost(g: Game, n: Nation, r: Region, by: string): void {
  const s = n.atom!;
  const told = !!g.human && (seesPlants(g, g.human, n.id) || allied(g, g.human, by));
  const gave = papersTo(g, by, n.id);
  if (by === g.human && gave.length) headline(g, 'event', `You have taken the ${n.adj} atomic plants ${inAt(r)}. Their papers give you ${gave.map((id) => techById(id)?.name ?? id).join(' and ')}.`, by, r.id);
  else if (told) headline(g, 'event', `${upFirst(theName(n))} has lost its atomic plants ${inAt(r)} to ${theName(g.nations[by])}. All their work is lost with the province.`, n.id, r.id);
  delete s.plants; delete s.site; delete s.paused; delete s.done; delete s.next;
}

/**
 * A country that falls loses its project with it (game.ts capitulate; the plan: a fallen country's project ends): the
 * one who holds the plants' ground takes the papers, and the plants, the bombs in hand and a strike set aside are gone.
 * A country freed later starts again from its research.
 */
export function atomFallen(g: Game, nat: string, by: string): void {
  if (!atomOn(g)) return;
  const n = g.nations[nat], s = n?.atom;
  if (!s) return;
  if (s.plants !== undefined && s.site !== undefined) {
    const r = g.regions[s.site];
    plantsLost(g, n, r, r.ctrl !== nat && !allied(g, nat, r.ctrl) ? r.ctrl : by);
  }
  delete s.plants; delete s.site; delete s.paused; delete s.done; delete s.next; delete s.bombs; delete s.armed; delete s.stopMaking;
}

/** The bombs: the first once the plants and the weapon are done, then one more every few months of making, never more than a couple in hand. */
function bombMonth(g: Game, n: Nation): void {
  const s = n.atom!;
  if (s.done === undefined || !hasTech(g, n.id, ATOM.rows.weapon)) return;
  if (s.ready === undefined) {
    s.ready = g.turn;
    s.bombs = 1;
    s.next = 0;
    s.fresh = g.turn;
    if (n.id === g.human) headline(g, 'info', hasTech(g, n.id, ATOM.strike.need) ? 'An atomic bomb is ready. It can be carried by a raid of four-engined bombers on an enemy city in their reach that is not a capital. Open THE ATOMIC PROJECT.' : 'An atomic bomb is ready, but it needs Four-engined bombers to carry it.', n.id);
    return;
  }
  if (s.stopMaking || (s.bombs ?? 0) >= ATOM.plants.most || n.pp < 0) return;
  const paid = ATOM.plants.make * Math.max(0, income(g, n.id));
  s.paid = [g.turn, paid];
  s.next = (s.next ?? 0) + 1 - siteHarm(g, g.regions[s.site ?? n.capital] ?? g.regions[n.capital]);
  if (s.next >= ATOM.plants.every - 1e-9) {
    s.next = 0;
    s.bombs = Math.min(ATOM.plants.most, (s.bombs ?? 0) + 1);
    s.fresh = g.turn;
    if (n.id === g.human) headline(g, 'info', 'Another atomic bomb is ready.', n.id);
  }
}

/** The book's clock (stops 1-2): only the record's nation, its rows and plants on the record's months. */
function bookMonth(g: Game, n: Nation): void {
  const B = ATOM.book;
  if (n.id !== B.nat) return;
  if (g.turn >= B.physics) grantTech(g, n.id, ATOM.rows.physics);
  if (g.turn >= B.chain) grantTech(g, n.id, ATOM.rows.chain);
  if (g.turn < B.plants) return;
  const s = stateOf(n);
  if (s.plants === undefined) { s.site = safestSite(g, n.id) ?? n.capital; s.plants = 0; s.pace = 'full'; }
  if (s.done !== undefined) return;
  if (g.turn >= B.ready) {
    grantTech(g, n.id, ATOM.rows.weapon);
    s.plants = ATOM.plants.bill;
    s.done = g.turn;
    return;
  }
  // The plants draw their share in the record's months like anyone's; the record's clock says when they are done.
  s.paid = [g.turn, drawAt(g, n.id, 'full')];
  s.plants = ATOM.plants.bill * (g.turn - B.plants + 1) / (B.ready - B.plants + 1);
}

/** The papers of a country's project, taken with its plants' ground: the two research rows, where the captor has a tree. Returns the rows it gave. */
function papersTo(g: Game, by: string, from: string): string[] {
  const n = g.nations[by];
  if (!n?.alive || !techOf(g, by)) return [];
  const gave = [ATOM.rows.physics, ATOM.rows.chain].filter((id) => grantTech(g, by, id));
  const s = stateOf(n);
  if (!(s.papers ??= []).includes(from)) s.papers.push(from);
  return gave;
}

// ---- Who knows of whose plants ---------------------------------------------------------------------------

/**
 * True when the viewer's side knows the owner's project at its own sight (the design director, 133(a)): the owner has
 * used a bomb (the news line), or the viewer's side holds papers taken from it, or one of the viewer's agents is at
 * work in the plants' province or in the owner's capital, or (for the computer's services against the player) one of
 * their agents has stood in the plants' province.
 */
export function seesPlants(g: Game, viewer: string, owner: string): boolean {
  const o = g.nations[owner]?.atom;
  if (!o || (o.plants === undefined && !o.used)) return false;
  if (allied(g, viewer, owner)) return true;
  if (o.used) return true;
  for (const n of Object.values(g.nations)) if (allied(g, viewer, n.id) && n.atom?.papers?.includes(owner)) return true;
  const site = o.site, cap = g.nations[owner].capital;
  if (g.intel?.agents?.some((a) => allied(g, viewer, a.nat) && a.ready <= g.turn && (a.reg === site || (a.reg === cap && a.mission === 'books')))) return true;
  if (site !== undefined && g.intel?.foes?.some((f) => allied(g, viewer, f.nat) && f.reg === site)) return true;
  return false;
}

/**
 * What a raid's cut of a province is worth to a staff beside its output, where an enemy's plants stand there unfinished
 * and the raiders' side has seen them (air.ts forecast). 0 otherwise, and always with the switch off.
 */
export function atomRaidWorth(g: Game, by: string, r: Region, cut: number): number {
  const owner = plantsAt(g, r);
  if (!owner || !atWar(g, by, owner) || !seesPlants(g, by, owner)) return 0;
  return ATOM.raid.worth * cut;
}

/**
 * Sabotage on a factory of a province where a country's plants stand (intel.ts): they lose some months of their work at
 * the present pace. Returns the line to add to the report, or null where nothing stands there.
 */
export function atomSabotage(g: Game, r: Region): string | null {
  const owner = plantsAt(g, r);
  if (!owner) return null;
  const s = g.nations[owner].atom!;
  const back = ATOM.sabotage.months * drawAt(g, owner, paceOf(s));
  if (back <= 0 || (s.plants ?? 0) <= 0) return null;
  s.plants = Math.max(0, (s.plants ?? 0) - back);
  return owner === g.human ? `The atomic plants there lose ${ATOM.sabotage.months} months of their work.` : `The ${g.nations[owner].adj} atomic plants there lose ${ATOM.sabotage.months} months of their work.`;
}

/** How many more times the other side's saboteurs pick the player's plants province (intel.ts): only for plants they have seen. */
export function sabotageWeight(g: Game, foe: string, r: Region): number {
  const owner = plantsAt(g, r);
  return owner && atWar(g, foe, owner) && seesPlants(g, foe, owner) ? ATOM.sabotage.weight : 0;
}

// ---- The strike -----------------------------------------------------------------------------------------

/** The cities on the home ground of a country this nation is at war with, not a mere town: capitals among them, which whyNotStrike rules out. */
export function strikeTargets(g: Game, nat: string): number[] {
  const out: number[] = [];
  for (const r of g.regions) {
    const v = homeOf(g, r);
    if (!v || v !== r.ctrl || !atWar(g, nat, v) || !g.nations[v]?.alive) continue;
    const size = townSize(g, r);
    if (size !== 'city' && size !== 'capital') continue;
    out.push(r.id);
  }
  return out;
}

/** The nation's bomber groups that could fly a raid on this city now (the air rules' own reach): land-based, with bombers. */
export function carriersFor(g: Game, nat: string, region: number): AirFleet[] {
  return airFleetsOf(g, nat).filter((f) => f.ship === undefined && f.ashore === undefined && f.bmr >= 1 && f.flew !== g.turn && airTargets(g, f, 'bomb').includes(region));
}

/** Why this nation cannot arm a strike on this city now, or null. */
export function whyNotStrike(g: Game, nat: string, region: number): string | null {
  if (!atomOn(g)) return 'The atomic project is not in this campaign.';
  const n = g.nations[nat], r = g.regions[region];
  if (!n?.alive || !r) return 'There is no such place.';
  const s = n.atom;
  if (!(s?.bombs ?? 0)) return 'There is no atomic bomb in hand.';
  if (s?.struck === g.turn) return 'One strike a month at most.';
  if (!hasTech(g, nat, ATOM.strike.need)) return 'It needs Four-engined bombers to carry it.';
  const v = homeOf(g, r);
  if (townSize(g, r) === 'capital') return 'Never a capital: a government must be left to give in.';
  if (!v || v !== r.ctrl || !atWar(g, nat, v)) return 'Only a city on the home ground of a country you are at war with.';
  if (townSize(g, r) !== 'city') return 'Only a city.';
  if (!carriersFor(g, nat, region).length) return 'None of your bomber groups can reach it this month.';
  return null;
}

/** The reason a city cannot be struck, as the page sets it out beside the city (the UI director): a few words. */
export function strikeBar(g: Game, nat: string, region: number): string | null {
  const why = whyNotStrike(g, nat, region);
  if (why === null) return null;
  const r = g.regions[region], s = g.nations[nat]?.atom;
  if (r && townSize(g, r) === 'capital') return 'A capital';
  if (s?.struck === g.turn) return 'One strike this month already';
  if (!hasTech(g, nat, ATOM.strike.need)) return 'Needs Four-engined bombers';
  if (why === 'None of your bomber groups can reach it this month.') return "Beyond your bombers' reach";
  return why;
}

/** Arms a strike: the next bombing raid this nation flies on that city this month carries the bomb. */
export function armStrike(g: Game, nat: string, region: number): boolean {
  if (whyNotStrike(g, nat, region) !== null) return false;
  g.nations[nat].atom!.armed = [g.turn, region];
  return true;
}

/** The raid card's line for a raid that carries the bomb (airwar.ts): what it does if the bomber reaches the release, and if not. */
export function strikeWords(g: Game, nat: string, region: number): string {
  const r = g.regions[region], v = g.nations[r.ctrl];
  return `This raid carries the atomic bomb. If its bomber reaches ${townName(r)}, most of the city is destroyed and ${v ? theName(v) : 'the enemy'} loses ${pct(v ? nextWorn(g, v.id) : ATOM.will.first)} of its will to fight. If the bomber is shot down, the bomb goes with it.`;
}

/** Takes an armed strike back before the raid flies. */
export function disarmStrike(g: Game, nat: string): boolean {
  const s = g.nations[nat]?.atom;
  if (!s?.armed) return false;
  delete s.armed;
  return true;
}

/** The city a nation's next raid this month carries the bomb to, or null. */
export function armedAt(g: Game, nat: string): number | null {
  const a = atomOn(g) ? g.nations[nat]?.atom?.armed : undefined;
  return a && a[0] === g.turn && (g.nations[nat].atom?.bombs ?? 0) > 0 ? a[1] : null;
}

/**
 * What an armed strike's raid came to: the one line of news, whether the bomb reached the city, the ruin it left, and
 * whether the line is told (the UI director: a bomb that reached its city is told to every side; one lost on the way,
 * to the striker's side only, for the defender saw an ordinary raid).
 */
export interface StrikeResult { text: string; hit: boolean; city: CityHit | null; tell: boolean }

/**
 * The raid of an armed strike has flown (air.ts fly, a raid on the armed city): `reached` of its `flown` bombers reached
 * the release, and the bomber that carries the bomb is one of them as often as not. The bomb is spent either way. The
 * town's damage goes through the city damage rules alone, and the will through atomWorn alone: the raid itself does no
 * ordinary damage, and is not counted as a raid anywhere else (priced once).
 */
export function strikeFlown(g: Game, nat: string, region: number, flown: number, reached: number): StrikeResult {
  const n = g.nations[nat], s = n.atom!, r = g.regions[region], v = r.ctrl;
  delete s.armed;
  s.bombs = Math.max(0, (s.bombs ?? 1) - 1);
  s.struck = g.turn;
  const p = flown > 0 ? Math.max(0, Math.min(1, reached / flown)) : 0;
  const roll = new Rng(hashString(`atom:${g.seed}:${g.turn}:${nat}:${region}`)).next();
  if (roll >= p) return { text: `The bomber carrying the atomic bomb was lost on the way to ${townName(r)}, and the bomb with it.`, hit: false, city: null, tell: !!g.human && allied(g, g.human, nat) };
  const city = ruinHit(g, r, Math.min(ATOM.strike.ruin, DAMAGE.most));
  const first = !s.used;
  s.used = (s.used ?? 0) + 1;
  const V = g.nations[v];
  ((V.atom ??= {}).hits ??= []).push([g.turn, nat, region]);
  g.flags[`atom:hit:${region}`] = g.turn;
  const text = first ? `A single atomic bomb has destroyed most of ${townName(r)}. ${upFirst(theName(n))} has the atomic bomb.` : `Another ${n.adj} atomic bomb has destroyed most of ${townName(r)}.`;
  return { text, hit: true, city, tell: true };
}

// ---- The will to fight, and terms ----------------------------------------------------------------------

/** What strikes have taken from a country's will to fight (resources.ts willLost): it does not wear off. */
export function atomWorn(g: Game, nat: string): number {
  if (!atomOn(g)) return 0;
  return wornAfter(g.nations[nat]?.atom?.hits?.length ?? 0);
}

/** What the next strike that reaches a city of this country would take from its will to fight (the raid card's figure). */
export const nextWorn = (g: Game, nat: string): number => {
  const k = g.nations[nat]?.atom?.hits?.length ?? 0;
  return wornAfter(k + 1) - wornAfter(k);
};

/** The will to fight that k strikes take, all told. */
function wornAfter(k: number): number {
  if (k <= 0) return 0;
  const W = ATOM.will;
  return Math.min(W.most, W.first + (k > 1 ? W.second : 0) + Math.max(0, k - 2) * W.later);
}

/**
 * The country that struck this one with an atomic bomb in the last months (the newest), while still at war with it, or
 * null: for those months it may ask for terms though its capital stands free and it holds most of its home (game.ts
 * checkCollapse). Never the player's own country: his seat is not moved by this path.
 */
export function atomPress(g: Game, nat: string): string | null {
  if (!atomOn(g) || nat === g.human) return null;
  const hits = g.nations[nat]?.atom?.hits;
  if (!hits?.length) return null;
  for (let i = hits.length - 1; i >= 0; i--) {
    const [turn, by] = hits[i];
    if (g.turn - turn > ATOM.will.press) break;
    if (g.nations[by]?.alive && atWar(g, nat, by)) return by;
  }
  return null;
}

// ---- The computer's staffs ------------------------------------------------------------------------------

/** The months of war a nation has fought against a great power, as its staff counts them (the flag 'war:a:b' is the month it opened; none, a war since the first day, as ai.ts counts it). */
function warMonths(g: Game, nat: string): number {
  let most = 0;
  for (const foe of g.nations[nat]?.atWar ?? []) {
    if (!PLANS.great.includes(foe) || !g.nations[foe]?.alive) continue;
    const from = Math.max(g.flags[`war:${nat}:${foe}`] ?? 0, g.flags[`war:${foe}:${nat}`] ?? 0);
    most = Math.max(most, g.turn - from);
  }
  return most;
}

/**
 * True when a computer-run staff wants the project (stops 3-5): a war of some length against a great power, earnings
 * that could finish the plants in time at full pace, and a country not near collapse. Never the player's own country
 * (his advisers do not begin it for him), never at the book's stops.
 */
export function atomWanted(g: Game, nat: string): boolean {
  if (!atomOn(g) || nat === g.human || stopDef(g).book) return false;
  const n = g.nations[nat];
  if (!n?.alive || !techOf(g, nat)) return false;
  if (warMonths(g, nat) < ATOM.ai.war) return false;
  if (collapseScore(g, nat) >= ATOM.ai.calm * n.resilience) return false;
  const left = ATOM.plants.bill - (n.atom?.plants ?? 0);
  const a = drawAt(g, nat, 'full');
  return a > 1e-9 && left / a <= ATOM.ai.months;
}

/**
 * A computer-run nation's month for its project (turn.ts stepAI): it begins the plants when it wants them, stops them
 * while in debt or near collapse, and flies a strike when it has a bomb: at the enemy nearest to breaking, the most
 * industrial city in reach.
 */
export function aiAtom(g: Game, nat: string): void {
  if (!atomOn(g) || nat === g.human) return;
  const n = g.nations[nat];
  if (!n?.alive || !techOf(g, nat)) return;
  const s = n.atom;
  if (!onBook(g, nat)) {
    if (s?.plants === undefined && hasTech(g, nat, ATOM.rows.chain) && atomWanted(g, nat)) beginPlants(g, nat, undefined, 'full');
    if (s?.plants !== undefined && s.done === undefined) {
      const stop = n.pp < 0 || collapseScore(g, nat) >= ATOM.ai.calm * n.resilience;
      pausePlants(g, nat, stop);
    }
  }
  if (!(n.atom?.bombs ?? 0)) return;
  if (onBook(g, nat) && g.turn < ATOM.book.use) return;
  const pick = aiStrikePick(g, nat);
  if (!pick) return;
  if (!armStrike(g, nat, pick.region)) return;
  aiFly(g, nat, pick.region, pick.groups);
}

/** The staff's pick: the enemy nearest to breaking (its collapse score against what it will bear), and its most industrial city a group can reach. */
export function aiStrikePick(g: Game, nat: string): { region: number; groups: number[] } | null {
  let best: { region: number; groups: number[]; near: number; worth: number } | null = null;
  for (const id of strikeTargets(g, nat)) {
    if (whyNotStrike(g, nat, id) !== null) continue;
    const r = g.regions[id], v = g.nations[r.ctrl];
    const near = v.resilience > 0 ? collapseScore(g, v.id) / v.resilience : 0;
    const worth = r.fac * (1 - damageOf(g, r)) + r.vp;
    const groups = carriersFor(g, nat, id).sort((a, b) => b.bmr - a.bmr || a.id - b.id).slice(0, 1).map((f) => f.id);
    if (!best || near > best.near + 1e-9 || (Math.abs(near - best.near) <= 1e-9 && (worth > best.worth || (worth === best.worth && id < best.region)))) best = { region: id, groups, near, worth };
  }
  return best ? { region: best.region, groups: best.groups } : null;
}

/** Hooked by turn.ts: flies a staff's armed raid at once through the air war's own raid (airwar.ts launchRaid). */
export const atomFly: { raid?: (g: Game, nat: string, region: number, groups: number[]) => boolean } = {};
function aiFly(g: Game, nat: string, region: number, groups: number[]): void {
  const ok = atomFly.raid?.(g, nat, region, groups) ?? false;
  if (!ok) disarmStrike(g, nat);
}

// ---- For the interfaces --------------------------------------------------------------------------------

export interface AtomView {
  on: boolean;
  rows: { id: string; done: boolean; work: boolean }[];
  canPlants: string | null;
  plants: number | null;
  bill: number;
  site: number | null;
  sites: { region: number; hops: number }[];
  pace: AtomPace;
  paused: boolean;
  /** Idle this month: the treasury was in debt as the month opened, and the plants drew nothing. */
  idle: boolean;
  /** Drawn this month already (as the month opened); false the month they are started, when the draw is next month's. */
  drawn: boolean;
  draw: number;
  months: number | null;
  done: boolean;
  bombs: number;
  making: boolean;
  next: number;
  armed: number | null;
  targets: { region: number; why: string | null }[];
  /** What strikes have taken from the nation's own will. */
  worn: number;
  /** Other countries' projects the nation knows of. */
  known: { nat: string; plants: number | null; done: boolean; bombs: number; used: number }[];
}

/** The project as one nation sees it. Null outside the rules. */
export function atomView(g: Game, nat: string): AtomView | null {
  if (!atomOn(g) || !techRules(g) || !g.nations[nat]) return null;
  const n = g.nations[nat], s = n.atom, t = techOf(g, nat);
  const rows = [ATOM.rows.physics, ATOM.rows.chain, ATOM.rows.weapon].map((id) => ({ id, done: !!t?.done.includes(id), work: !!t?.work.some((w) => w.id === id) }));
  const known = Object.values(g.nations).filter((x) => x.id !== nat && x.alive && x.atom && (x.atom.plants !== undefined || x.atom.used) && seesPlants(g, nat, x.id))
    .map((x) => ({ nat: x.id, plants: x.atom!.plants ?? null, done: x.atom!.done !== undefined, bombs: x.atom!.bombs ?? 0, used: x.atom!.used ?? 0 }));
  return {
    on: true, rows, canPlants: whyNotPlants(g, nat), plants: s?.plants ?? null, bill: ATOM.plants.bill,
    site: s?.plants !== undefined ? s.site ?? null : chosenSite(g, nat), sites: s?.plants === undefined ? siteChoices(g, nat).slice(0, 6) : [],
    pace: paceOf(s), paused: !!s?.paused, idle: s?.plants !== undefined && s.done === undefined && !s.paused && g.flags[`atom:idle:${nat}`] === g.turn, drawn: atomBill(g, nat) > 0,
    draw: atomBill(g, nat) || (s?.plants !== undefined && s.done === undefined && !s.paused ? drawAt(g, nat, paceOf(s)) : 0),
    months: s?.done === undefined ? monthsLeft(g, nat) : 0, done: s?.done !== undefined, bombs: s?.bombs ?? 0, making: !s?.stopMaking, next: s?.next ?? 0,
    armed: armedAt(g, nat),
    targets: (s?.bombs ?? 0) > 0 ? strikeTargets(g, nat).map((region) => ({ region, why: strikeBar(g, nat, region) })) : [],
    worn: atomWorn(g, nat), known,
  };
}

// ---- Words ----------------------------------------------------------------------------------------------

const pct = (x: number): string => `${Math.round(x * 100)}%`;
const upFirst = (t: string): string => t.charAt(0).toUpperCase() + t.slice(1);
const inAt = (r: Region): string => `at ${townName(r)}`;
/** How long the plants will take at today's earnings, in the news line's words: past five years it says so. */
export const finishWords = (m: number | null): string => (m === null || m > 60 ? 'will take more than five years' : `will be finished in about ${m <= 1 ? 'a month' : `${m} months`}`);
