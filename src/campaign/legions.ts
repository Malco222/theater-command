// Volunteer legions (release 2; Brian's yes of 22 November 2026, the design director's frame of 03:42 UTC and
// rulings of 03:47 and 03:55 UTC; the record is the historian's table, historian/volunteer-legions-1939-45.md).
//
// A country at peace outside both alliances may send one legion of volunteers at a time, a division at most, to fight
// for a country at war: its patron. The legion is a corps of the patron's army, so it is in the patron's wars and
// under the patron's staff, but it keeps its own name and its home's flag. Its men are the home's: the reserve
// (manpower.ts) gives the men to raise it, and its drafts after. The patron pays its upkeep and equips its drafts, as
// Germany did the Blue Division's. The home stays at peace (the frame's point 7), but the great power the legion
// fights loses patience with it (bargains.ts patience, with the bargains switched on).
//
// Who sends one: the player, from the trade office, to a patron his public leans toward or to a friend that has been
// attacked, for political capital as courting costs. A computer-run country at stops 1 and 2 sends the record's
// legions by their dates and no others (the Spanish Blue Division and the Blue Legion after it, the Swedish volunteers
// in Finland); at stops 3 to 5 it sends one when it leans far toward a patron and against its enemy, or when a friend
// has been attacked. A legion comes home on recall, at its tour's end, when its patron makes peace with the enemy it
// was sent to fight, or when its home goes to war itself. Its men go back to the reserve and its veterans teach the
// army at home. Destroyed in the field, its men are lost.
//
// The whole of it is switched by stage.json 'legions': with it off no campaign carries the flag 'legions', nothing
// here runs, and every war is release 1's. The numbers and rows live in data/campaign/legions.json.

import data from '../../data/campaign/legions.json';
import { addCorps, atWar, bloc, headline, removeCorps, theName, warning } from './game';
import { friendsOf, majorsOf } from './diplomacy';
import { drawMen, menLeft, menSpare, poolAtStart, returnMen } from './manpower';
import { PRODUCTION } from './economy';
import { drafted } from './stories';
import { sells } from './bargains';
import { stopOf } from './history';
import { LEGIONS_STAGE } from './stage';
import { MEN_PER_DIV, type Corps, type Game, type LegionAbroad, type LegionBook, type Nation, type Region } from './types';

export interface LegionRow {
  id: string;
  home: string;
  patron: string;
  against: string;
  name: string;
  est: number;
  xp?: number;
  from: number;
  to: number;
  then?: string;
  no?: number;
}

export const LEGIONS = data as unknown as {
  cost: number; leanAt: number; send: number; backAt: number; tour: number; train: number; legionFall: number;
  generic: { est: number; share: number; least: number; xp: number; spare: number; name: string };
  rows: LegionRow[];
};

/** True in a grand campaign opened with the legions switched on. */
export const legionRules = (g: Game): boolean => g.flags.open === 1 && g.flags.legions === 1 && !!g.legions;

const book = (g: Game): LegionBook => g.legions!;
const side = (n: Nation | undefined): number => { const b = n ? bloc(n) : null; return b === 'axis' ? 1 : b === 'allies' ? -1 : 0; };
const The = (nat: string, g: Game): string => theName(g.nations[nat], true);
const the = (nat: string, g: Game): string => theName(g.nations[nat]);
const rowById = (id: string | undefined): LegionRow | undefined => (id ? LEGIONS.rows.find((r) => r.id === id) : undefined);
/** The rows that carry on another's men: not sent on their own. */
const SUCCESSORS = new Set(LEGIONS.rows.map((r) => r.then).filter((x): x is string => !!x));

// ---- Who may send, to whom ---------------------------------------------------------------------------------------

/** How far a country's public leans toward another's side, -100 to 100: 0 toward a country of neither side. */
export const leanTo = (g: Game, home: string, to: string): number => (g.nations[home]?.opinion ?? 0) * side(g.nations[to]);

/**
 * True when a country was attacked by another: struck while it stood outside both sides (game.ts's 'shock' of a
 * declaration), the other struck it first (the 'opening' of their war), and it still stands a month later. The Winter
 * War's Finland, not a France that went to war in 1939 and was struck in 1940.
 */
export function attackedBy(g: Game, victim: string, by: string): boolean {
  if (g.flags[`shock:${victim}`] === undefined) return false;
  const a = g.flags[`opening:${by}:${victim}`], b = g.flags[`opening:${victim}:${by}`];
  return a !== undefined && (b === undefined || a <= b) && g.turn - a >= 1;
}

/**
 * True when the patron is a friend of the home's (relations.json) and the enemy attacked it, and the home's public does
 * not lean toward the attacker's side (a country leaning toward the attacker sends its friend sympathy, not men).
 */
export const friendAttacked = (g: Game, home: string, patron: string, against: string): boolean =>
  attackedBy(g, patron, against) && friendsOf(home).some((f) => f.nat === patron) && leanTo(g, home, against) < LEGIONS.leanAt;

/** True for a country that can send a legion: it sells (at peace, outside both sides) and is not a great power. */
export function canSend(g: Game, home: string): boolean {
  return sells(g, home) && !majorsOf(g).includes(home);
}

/** The patron's ground on the front against the enemy: held by the patron, next to the enemy's, with no enemy in it. */
function frontOf(g: Game, patron: string, against: string): Region[] {
  const out: Region[] = [];
  for (const r of g.regions) {
    if (r.ctrl !== patron || r.overseas) continue;
    if (!r.nb.some((j) => g.regions[j].ctrl === against)) continue;
    if (r.units.some((u) => !u.dead && atWar(g, u.nat, patron))) continue;
    out.push(r);
  }
  return out;
}

/**
 * Where a legion comes into the line: the patron's ground on the front against its enemy nearest its home, reached
 * overland through ground held by nobody at war with the patron; or null where volunteers could not get there (no
 * front, or the sea or an enemy between). The record's legions at stops 1 and 2 went as they went: the front nearest
 * home however the way lies ('record').
 */
export function landing(g: Game, home: string, patron: string, against: string, record = false): Region | null {
  const front = frontOf(g, patron, against);
  const cap = g.nations[home]?.capital ?? -1;
  if (!front.length) return null;
  if (cap < 0) return record ? front[0] : null;
  const seen = new Uint8Array(g.regions.length);
  const q = [cap];
  seen[cap] = 1;
  const want = new Set(front.map((r) => r.id));
  for (let i = 0; i < q.length; i++) {
    const at = q[i];
    if (want.has(at)) return g.regions[at];
    for (const j of g.regions[at].nb) {
      if (seen[j]) continue;
      seen[j] = 1;
      const R = g.regions[j];
      if (!record && R.ctrl !== home && R.ctrl !== patron && atWar(g, R.ctrl, patron)) continue;
      q.push(j);
    }
  }
  return record ? front[0] : null;
}

/** The country whose volunteers already serve with this patron, if any: a patron takes one legion at a time. */
const patronHas = (g: Game, patron: string, but?: string): string | undefined =>
  Object.keys(book(g).abroad).sort().find((h) => h !== but && book(g).abroad[h].patron === patron);

/**
 * Why a country could not send a legion to this patron against this enemy now, or null. The player's leaning need only
 * reach 'leanAt'; a computer-run country's must reach 'send' (it is asked only at stops 3 to 5).
 */
export function whyNoLegion(g: Game, home: string, patron: string, against: string, player = home === g.human): string | null {
  if (!legionRules(g)) return 'Volunteer legions are not in this campaign.';
  const H = g.nations[home], P = g.nations[patron], E = g.nations[against];
  if (!H || !P || !E) return 'No such country.';
  if (!canSend(g, home)) return majorsOf(g).includes(home) ? 'A great power sends no volunteer legion.' : 'Only a country at peace outside both sides sends volunteers.';
  if (book(g).abroad[home]) return 'Your volunteers already serve abroad: one legion at a time.';
  if (!P.alive || P.dormant || !E.alive || !atWar(g, patron, against)) return `${The(patron, g)} is not at war with ${the(against, g)}.`;
  const other = patronHas(g, patron, home);
  if (other) return `${The(patron, g)} already has ${g.nations[other].adj} volunteers: one legion at a time.`;
  const friend = friendAttacked(g, home, patron, against);
  const lean = leanTo(g, home, patron);
  if (!friend && (lean < (player ? LEGIONS.leanAt : LEGIONS.send) || leanTo(g, home, against) >= 0)) {
    return player
      ? `Your public does not lean toward ${the(patron, g)}'s side against ${the(against, g)}: ${LEGIONS.leanAt} points are needed, or a friend attacked.`
      : 'It does not lean that way.';
  }
  if (H.pc < LEGIONS.cost) return `A legion costs ${LEGIONS.cost} political capital; you have ${Math.floor(H.pc)}.`;
  const est = rowFor(home, patron, against)?.est ?? genericEst(g, home);
  if ((player ? menLeft(g, home) : menSpare(g, home) - LEGIONS.generic.spare) < est) return 'The reserve of men cannot spare a division.';
  if (!landing(g, home, patron, against)) return `${The(patron, g)} has no front against ${the(against, g)} for volunteers to reach.`;
  return null;
}

/**
 * The size of a legion the record does not name: a share of the reserve its country opened the war with, rounded to a
 * twentieth of a division, never under 'least' and never over a division. A small country's volunteers are few.
 */
export function genericEst(g: Game, home: string): number {
  const G = LEGIONS.generic;
  return Math.max(G.least, Math.min(G.est, Math.round((G.share * poolAtStart(g, home)) * 20) / 20));
}

/** The record's row a legion to this patron against this enemy would be, at any stop: its name and size. */
const rowFor = (home: string, patron: string, against: string): LegionRow | undefined =>
  LEGIONS.rows.find((r) => r.home === home && r.patron === patron && r.against === against && !SUCCESSORS.has(r.id));

// ---- Sending, arriving, coming home -------------------------------------------------------------------------------

function send(g: Game, home: string, patron: string, against: string, row: LegionRow | undefined, now = false): void {
  const H = g.nations[home];
  const est = row?.est ?? genericEst(g, home);
  const name = row?.name ?? LEGIONS.generic.name.replace('{adj}', H.adj);
  drawMen(g, home, est);
  const since = now ? g.turn : g.turn + 1;
  const until = row && stopOf(g) <= 2 && row.to >= since ? row.to : since + LEGIONS.tour - 1;
  const L: LegionAbroad = { corps: -1, patron, against, name, est, since, until };
  if (row) L.row = row.id;
  book(g).abroad[home] = L;
  if (now) arrive(g, home, L, true);
  else {
    const line = `${H.adj} volunteers form the ${name} to fight for ${the(patron, g)} against ${the(against, g)}. They reach the line next month.`;
    headline(g, home === g.human || patron === g.human ? 'info' : 'event', line, home === g.human ? home : patron === g.human ? patron : home, H.capital >= 0 ? H.capital : undefined);
  }
}

function arrive(g: Game, home: string, L: LegionAbroad, quiet = false): void {
  const H = g.nations[home];
  const byRecord = !!L.row && stopOf(g) <= 2;
  const at = atWar(g, L.patron, L.against) && g.nations[L.patron]?.alive ? landing(g, home, L.patron, L.against, byRecord) : null;
  if (!at) {
    returnMen(g, home, L.est);
    delete book(g).abroad[home];
    if (!quiet) headline(g, home === g.human ? 'info' : 'event', `The ${L.name} goes home before it reaches the line: ${the(L.patron, g)} no longer fights ${the(L.against, g)} on any front it could reach.`, home);
    return;
  }
  const row = rowById(L.row);
  const key = `dn:${L.patron}:inf`, was = g.flags[key];
  const c = addCorps(g, L.patron, L.name, 'inf', 1, at.id, { xp: row?.xp ?? LEGIONS.generic.xp, est: L.est });
  // Its division keeps its own number, where the record gives one, and takes none of the patron's.
  if (row?.no) {
    if (was === undefined) delete g.flags[key]; else g.flags[key] = was;
    c.dn = row.no;
    delete c.nos;
  }
  c.str = Math.min(1, L.est);
  c.vol = home;
  L.corps = c.id;
  if (quiet) return;
  const line = `The ${L.name}, ${H.adj} volunteers, comes into ${the(L.patron, g)}'s line at ${at.name}, against ${the(L.against, g)}.`;
  headline(g, home === g.human || L.patron === g.human ? 'info' : 'event', line, home === g.human ? home : L.patron === g.human ? L.patron : home, at.id);
}

/** The legion's corps, wherever it is: in the field, or at sea on a long passage. */
function corpsOf(g: Game, L: LegionAbroad): { c: Corps; atSea: boolean } | null {
  const c = g.corps.find((k) => k.id === L.corps && !k.dead);
  if (c) return { c, atSea: false };
  const v = (g.atSea ?? []).find((x) => x.corps.id === L.corps);
  return v ? { c: v.corps, atSea: true } : null;
}

/** Its veterans teach the army at home: every corps gains a share of their experience, never more than their own. */
function teach(g: Game, home: string, xp: number): void {
  for (const k of g.corps) if (k.nat === home && !k.dead && k.kind !== 'fort') k.xp = Math.max(k.xp, Math.min(xp, k.xp + LEGIONS.train * xp));
}

function comeHome(g: Game, home: string, L: LegionAbroad, c: Corps, why: string): void {
  const bk = book(g);
  const men = Math.max(0, c.divs * c.str);
  returnMen(g, home, men);
  teach(g, home, c.xp);
  removeCorps(g, c);
  if (L.row) bk.done[L.row] = g.turn;
  delete bk.abroad[home];
  const H = g.nations[home];
  const line = `The ${L.name} comes home from ${the(L.patron, g)}'s war against ${the(L.against, g)}: ${why}. ${Math.round(men * MEN_PER_DIV).toLocaleString('en-US')} men return, and their experience goes to the army.`;
  headline(g, home === g.human || L.patron === g.human ? 'info' : 'event', line, home === g.human ? home : L.patron === g.human ? L.patron : home, H.capital >= 0 ? H.capital : undefined);
}

/** At stops 1 and 2: the record's next legion carries on with the men who stay (the Blue Legion after the Blue Division). */
function carryOn(g: Game, home: string, L: LegionAbroad, c: Corps): boolean {
  const old = rowById(L.row), next = rowById(old?.then);
  if (!old || !next || stopOf(g) > 2 || g.turn < next.from || g.turn > next.to) return false;
  if (!atWar(g, next.patron, next.against) || next.patron !== L.patron) return false;
  const bk = book(g);
  const keep = Math.min(c.str * c.divs, next.est);
  const gone = Math.max(0, c.str * c.divs - keep);
  returnMen(g, home, gone);
  teach(g, home, c.xp);
  c.str = keep / c.divs;
  c.full = next.est;
  c.name = next.name;
  bk.done[old.id] = g.turn;
  L.row = next.id; L.name = next.name; L.est = next.est; L.until = next.to;
  headline(g, home === g.human || L.patron === g.human ? 'info' : 'event', `The ${old.name} goes home from ${the(L.patron, g)}'s war against ${the(L.against, g)}. ${Math.round(keep * MEN_PER_DIV).toLocaleString('en-US')} of its volunteers stay on as the ${next.name}.`, home === g.human ? home : L.patron === g.human ? L.patron : home);
  return true;
}

/** Why a legion in the field comes home this month, or null. */
function whyHome(g: Game, home: string, L: LegionAbroad): string | null {
  if (L.recall) return 'its government has called it back';
  if (!canSend(g, home)) return `${the(home, g)} is no longer at peace outside both sides`;
  if (!atWar(g, L.patron, L.against)) return `${the(L.patron, g)} is at peace with ${the(L.against, g)}`;
  if (g.turn > L.until) return 'its tour is over';
  if (home !== g.human && stopOf(g) >= 3 && leanTo(g, home, L.patron) < LEGIONS.backAt && !friendAttacked(g, home, L.patron, L.against)) return `its country no longer leans toward ${the(L.patron, g)}`;
  return null;
}

/**
 * The month's legions, at the month's start: those on their way reach the line, those whose time is up come home, those
 * lost are mourned, and computer-run countries send new ones (the record's at stops 1 and 2, their judgment at 3 to 5).
 */
export function legionsMonth(g: Game): void {
  if (!legionRules(g)) return;
  const bk = book(g);
  for (const home of Object.keys(bk.abroad).sort()) {
    const L = bk.abroad[home];
    if (L.corps < 0) { if (g.turn >= L.since) arrive(g, home, L); continue; }
    const at = corpsOf(g, L);
    if (!at) {
      const P = g.nations[L.patron];
      if (L.row) bk.done[L.row] = g.turn;
      delete bk.abroad[home];
      const fell = !P?.alive;
      const line = fell ? `The ${L.name} is lost with the fall of ${the(L.patron, g)}.` : `The ${L.name}, ${g.nations[home].adj} volunteers in ${the(L.patron, g)}'s service, has been destroyed in the field.`;
      if (home === g.human) warning(g, 'Volunteers lost', line, home);
      else headline(g, L.patron === g.human ? 'info' : 'event', line, L.patron === g.human ? L.patron : home);
      continue;
    }
    if (at.atSea) continue;
    const why = whyHome(g, home, L);
    if (!why) continue;
    if (why === 'its tour is over' && carryOn(g, home, L, at.c)) continue;
    comeHome(g, home, L, at.c, why);
  }
  const stop = stopOf(g);
  // At stops 1 and 2 the record's legions, the month before they are in the line (or late, while their dates last).
  if (stop <= 2) {
    for (const row of LEGIONS.rows) {
      if (SUCCESSORS.has(row.id) || bk.done[row.id] !== undefined || bk.abroad[row.home] || row.home === g.human) continue;
      if (g.turn + 1 < row.from || g.turn + 1 > row.to) continue;
      if (!canSend(g, row.home) || !g.nations[row.patron]?.alive || !atWar(g, row.patron, row.against)) continue;
      if (!landing(g, row.home, row.patron, row.against, true)) continue;
      send(g, row.home, row.patron, row.against, row);
    }
    return;
  }
  // At stops 3 to 5 each computer-run country's judgment: the patron it leans to most, or the friend attacked. A
  // patron takes one legion at a time: the record's legion first, then the friend's, then the strongest leaning.
  const at = Object.values(g.nations).filter((n) => n.alive && !n.dormant && n.atWar.length).map((n) => n.id).sort();
  const wish: { home: string; p: string; e: string; score: number }[] = [];
  for (const home of Object.keys(g.nations).sort()) {
    if (home === g.human || bk.abroad[home] || !canSend(g, home) || g.nations[home].pc < LEGIONS.cost) continue;
    let best: { home: string; p: string; e: string; score: number } | null = null;
    for (const p of at) {
      if (p === home) continue;
      const lean = leanTo(g, home, p);
      for (const e of [...g.nations[p].atWar].sort()) {
        const friend = friendAttacked(g, home, p, e);
        if (!friend && (lean < LEGIONS.send || leanTo(g, home, e) >= 0)) continue;
        const score = (rowFor(home, p, e) ? 1000 : 0) + (friend ? 100 : 0) + lean;
        if ((!best || score > best.score) && whyNoLegion(g, home, p, e, false) === null) best = { home, p, e, score };
      }
    }
    if (best) wish.push(best);
  }
  wish.sort((a, b) => b.score - a.score || (a.home < b.home ? -1 : 1));
  for (const w of wish) {
    if (patronHas(g, w.p)) continue;
    g.nations[w.home].pc -= LEGIONS.cost;
    send(g, w.home, w.p, w.e, rowFor(w.home, w.p, w.e));
  }
}

/** A legion's drafts (game.ts's month): from its home's reserve, equipped at the patron's cost. */
export function legionDrafts(g: Game, c: Corps, want: number): void {
  if (!c.vol || want <= 0) return;
  const P = g.nations[c.nat], perDiv = PRODUCTION.replacementCost;
  const give = Math.min(want, menLeft(g, c.vol), Math.max(0, P.pp) / perDiv);
  if (!(give > 0)) return;
  drafted(g, c, c.str, give / c.divs);
  c.str += give / c.divs;
  P.pp -= give * perDiv;
  drawMen(g, c.vol, give);
}

/** The great powers each legion in the field is fighting, for their patience with its home (bargains.ts). */
export function legionFeeds(g: Game): { power: string; home: string; name: string }[] {
  if (!legionRules(g)) return [];
  const out: { power: string; home: string; name: string }[] = [];
  const majors = majorsOf(g);
  for (const home of Object.keys(book(g).abroad).sort()) {
    const L = book(g).abroad[home];
    if (L.corps < 0) continue;
    const E = g.nations[L.against];
    if (!E?.alive || E.dormant || !majors.includes(L.against) || atWar(g, L.against, home)) continue;
    out.push({ power: L.against, home, name: `the ${L.name}` });
  }
  return out;
}

// ---- Opening, orders and the view ------------------------------------------------------------------------------------

/** As a grand campaign opens with the switch on: the flag, and the record's legions already in the line that day. */
export function openLegions(g: Game): void {
  if (LEGIONS_STAGE) startLegions(g);
}

/** Turns the legions on in a grand campaign whatever stage.json says (a test, the test kitchen), as openLegions would. */
export function startLegions(g: Game): void {
  if (g.flags.open !== 1 || g.legions) return;
  g.flags.legions = 1;
  g.legions = { abroad: {}, done: {} };
  if (stopOf(g) > 2) return;
  for (const row of LEGIONS.rows) {
    if (g.turn < row.from || g.turn > row.to || g.legions.abroad[row.home] || row.home === g.human) continue;
    if (!canSend(g, row.home) || !g.nations[row.patron]?.alive || !atWar(g, row.patron, row.against)) continue;
    send(g, row.home, row.patron, row.against, row, true);
  }
}

/** The player sends a legion: null when it is done, or why not. */
export function sendLegion(g: Game, home: string, patron: string, against: string): string | null {
  const why = whyNoLegion(g, home, patron, against, true);
  if (why) return why;
  g.nations[home].pc -= LEGIONS.cost;
  send(g, home, patron, against, rowFor(home, patron, against));
  return null;
}

/** The player calls his legion home: it leaves at the next month's start. Null when it is done, or why not. */
export function recallLegion(g: Game, home: string): string | null {
  if (!legionRules(g)) return 'Volunteer legions are not in this campaign.';
  const L = book(g).abroad[home];
  if (!L) return 'You have no volunteers abroad.';
  if (L.corps < 0) {
    returnMen(g, home, L.est);
    delete book(g).abroad[home];
    return null;
  }
  L.recall = true;
  return null;
}

export interface LegionView {
  home: string; homeName: string; patron: string; against: string; name: string;
  /** Its corps, or -1 on its way; where it stands, or -1. */
  corps: number; region: number;
  /** Men in its ranks now, and its size. */
  men: number; est: number;
  since: number; until: number; monthsLeft: number;
  recall: boolean;
  /** The record's legion it is, if it is one. */
  record: boolean;
}

/** Every legion abroad: public knowledge, as the record's were. */
export function legionsView(g: Game): LegionView[] {
  if (!legionRules(g)) return [];
  const out: LegionView[] = [];
  for (const home of Object.keys(book(g).abroad).sort()) {
    const L = book(g).abroad[home];
    const at = L.corps >= 0 ? corpsOf(g, L) : null;
    out.push({
      home, homeName: g.nations[home]?.name ?? home, patron: L.patron, against: L.against, name: L.name,
      corps: L.corps, region: at && !at.atSea ? at.c.reg : -1,
      men: Math.round((at ? at.c.str * at.c.divs : 0) * MEN_PER_DIV), est: L.est,
      since: L.since, until: L.until, monthsLeft: Math.max(0, L.until - g.turn + 1), recall: !!L.recall, record: !!L.row,
    });
  }
  return out;
}

/** The patrons a country could send its volunteers to now, with the enemy each fights, and why not where it cannot. */
export function legionOffers(g: Game, home: string): { patron: string; against: string; why: string | null }[] {
  if (!legionRules(g) || !canSend(g, home)) return [];
  const out: { patron: string; against: string; why: string | null }[] = [];
  for (const p of Object.keys(g.nations).sort()) {
    const P = g.nations[p];
    if (p === home || !P.alive || P.dormant || !P.atWar.length) continue;
    for (const e of [...P.atWar].sort()) {
      const lean = leanTo(g, home, p);
      if (!friendAttacked(g, home, p, e) && (lean <= 0 || leanTo(g, home, e) >= 0)) continue;
      out.push({ patron: p, against: e, why: whyNoLegion(g, home, p, e, true) });
    }
  }
  return out;
}
