// The player's staff at work (the grand campaign): what it does with each desk he has handed it, month by month,
// and the three depths that hand over several desks at once. The switches themselves are in depth.ts.
//
// For research, building, the air war and the navy the staff is the routine a computer nation uses, called for the
// player's nation and for that one desk: one set of rules, one balance. Trade is signed as a computer nation signs
// it, at the price the player pays. Courting and the secret service have no computer routine, so the rules for
// those two are written here, and kept cautious. Everything the staff buys comes out of the share of the treasury
// the player allows it for the month.
//
// A desk's month ends in one line of news, tagged with the desk so that an interface can offer to take it back.

import { DEPTH, DESKS, capOf, capPct, deskDef, deskRan, depthRules, markRan, setCap, setDesk, staffRuns, type DeskId, type DepthDef } from './depth';
import { atWar, bloc } from './game';
import { PRODUCTION, aiProduction, gunsWanted, income } from './economy';
import { aiArsenal } from './arsenal';
import { civicAI } from './buildings';
import { keepsStores } from './resources';
import { monthsFor, staffTeams, techById, techOf, teamsOf } from './tech';
import { TRADE, bestPartner, dealsOf, endTrade, housesOf, signTrade, tradeRules, wealthOf } from './trade';
import { DIPLOMACY, canCourt, court, receptive, step } from './diplomacy';
import { INTEL, abroad, agentsOf, atWork, intelRules, mostAgents, recruit, sendAgent, setFog, setMission, threat, whyNotMission, whyNotRecruit, whyNotSend } from './intel';
import { cabinetOff, officeSecure, setCabinet, setCanFall } from './government';
import { airAI, airFleetsOf } from './air';
import { atSea, fleetsOf, navalAI } from './navy';
import { shoreAI } from './fleets';
import { seaStaff } from './carriers';
import type { AirReport, Build, Game, Nation } from './types';

const listAnd = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
/** A list cut to its first few, with the rest counted. */
const some = (xs: string[], most = 3): string => (xs.length <= most ? listAnd(xs) : `${xs.slice(0, most).join(', ')} and ${xs.length - most} more`);
const the = (n: Nation): string => (/^(United|Soviet|Netherlands)/.test(n.name) ? `the ${n.name}` : n.name);
const months = (k: number): string => `${k} month${k === 1 ? '' : 's'}`;
const plural = (k: number, one: string, many = `${one}s`): string => `${k === 1 ? (/^[aeiou]/i.test(one) ? 'an' : 'a') : k} ${k === 1 ? one : many}`;

// ---- What the staff may spend -------------------------------------------------------------------------

/**
 * What is left of the month's allowance: the share of the treasury, as it stood when the staff first reached for it
 * this month, that the player lets his staff spend. A treasury in debt allows nothing.
 */
export function allowance(g: Game, nat: string): number {
  if (g.flags['dk:alwt'] !== g.turn) {
    g.flags['dk:alwt'] = g.turn;
    g.flags['dk:alw'] = Math.max(0, g.nations[nat].pp) * capOf(g);
  }
  return g.flags['dk:alw'] ?? 0;
}

/**
 * Lets a staff routine spend, but only out of the month's allowance: while it runs the treasury it sees is the
 * allowance, and what it did not touch is put back afterwards. Returns what it spent.
 */
function fromPurse(g: Game, nat: string, run: () => void): number {
  const n = g.nations[nat];
  const purse = Math.min(allowance(g, nat), Math.max(0, n.pp));
  const held = n.pp - purse;
  n.pp = purse;
  try { run(); } finally {
    const spent = purse - n.pp;
    n.pp += held;
    g.flags['dk:alw'] = Math.max(0, (g.flags['dk:alw'] ?? 0) - spent);
  }
  return purse - (n.pp - held);
}

// ---- Research -----------------------------------------------------------------------------------------

function research(g: Game, nat: string): string | null {
  const begun = staffTeams(g, nat);
  if (!begun.length) return null;
  const names = begun.map((id) => { const t = techById(id)!; return `${t.name} (${months(Math.ceil(monthsFor(t, g.turn)))})`; });
  return `put ${begun.length === 1 ? 'a research team' : `${begun.length} research teams`} to work on ${listAnd(names)}.`;
}

// ---- Building and trade -------------------------------------------------------------------------------

const orderWords = (g: Game, b: Build): string =>
  b.what === 'civic' ? `the ${b.name[0].toLowerCase()}${b.name.slice(1)}`
    : b.what === 'work' ? `${b.item === 'aa' ? 'anti-aircraft guns' : b.item ?? 'works'} at ${g.regions[b.region].name}`
      : `${b.name} at ${g.regions[b.region].name}`;

function build(g: Game, nat: string): string | null {
  const n = g.nations[nat];
  const did: string[] = [];
  // Trade first: an agreement pays every month from now on. One that nothing gets through is given up for a better.
  if (tradeRules(g) && n.capital >= 0) {
    const ctx = new Map();
    for (const d of dealsOf(g, nat)) {
      if (d.state !== 'cut') continue;
      const o = g.nations[d.a === nat ? d.b : d.a];
      if (endTrade(g, d.a, d.b)) did.push(`gave up the trade agreement with ${the(o)}, which nothing was getting through`);
    }
    const signed: string[] = [];
    for (let k = 0; k < 2 && dealsOf(g, nat).length < housesOf(g, nat) && n.pc >= TRADE.sign.pc; k++) {
      const b = bestPartner(g, nat, ctx);
      if (!b || !signTrade(g, nat, b, ctx)) break;
      signed.push(the(g.nations[b]));
    }
    if (signed.length) did.push(`signed ${signed.length === 1 ? 'a trade agreement' : 'trade agreements'} with ${listAnd(signed)}`);
  }
  // Then the month's orders, as a computer nation's staff places them: new corps, guns round bombed industry, building in the towns.
  const had = new Set(n.queue.map((b) => b.id));
  const spent = fromPurse(g, nat, () => {
    aiProduction(g, nat);
    aiArsenal(g, nat);
    civicAI(g, nat, income(g, nat), keepsStores(g, nat), gunsWanted(g, nat));
  });
  const fresh = n.queue.filter((b) => !had.has(b.id));
  if (fresh.length) did.unshift(`ordered ${some(fresh.map((b) => orderWords(g, b)), 4)}`);
  g.flags['dk:spent'] = spent;
  if (!did.length) return null;
  return `${listAnd(did)}.`;
}

// ---- Diplomacy ----------------------------------------------------------------------------------------

function diplomacy(g: Game, nat: string): string | null {
  const n = g.nations[nat], b = bloc(n);
  if (!b || b === 'league' || n.pc < DIPLOMACY.court.cost + DEPTH.diplomacy.keep) return null;
  const side = b === 'axis' ? 1 : -1;
  let best: Nation | null = null, top = 0;
  for (const T of Object.values(g.nations)) {
    if (!canCourt(g, nat, T.id).ok) continue;
    // The neutral most ready to listen, the nearer to joining the better, and a rich one before a poor one.
    const v = receptive(T, b) * (2 + step(T) * side) * Math.sqrt(1 + wealthOf(g, T.id));
    if (v > top || (v === top && best !== null && T.id < best.id)) { top = v; best = T; }
  }
  if (!best || !court(g, nat, best.id)) return null;
  return `courted ${the(best)}.`;
}

// ---- The secret service -------------------------------------------------------------------------------

/** Enemy-held ground beside the player's field army, with the divisions of his that stand against each province. */
function frontOf(g: Game, nat: string): Map<number, number> {
  const front = new Map<number, number>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort') continue;
    for (const j of g.regions[c.reg].nb) if (atWar(g, nat, g.regions[j].ctrl)) front.set(j, (front.get(j) ?? 0) + c.divs);
  }
  return front;
}

function spies(g: Game, nat: string): string | null {
  if (!intelRules(g)) return null;
  const n = g.nations[nat], S = DEPTH.spies;
  const did: string[] = [];
  const war = n.atWar.filter((e) => g.nations[e]?.alive);
  // One recruit a month while the service has room: all it can run in a war, a guard or two in peace.
  const want = war.length ? mostAgents(nat) : Math.min(mostAgents(nat), Math.max(1, S.guards));
  if (agentsOf(g, nat).length < want && whyNotRecruit(g, nat) === null && allowance(g, nat) >= INTEL.agents.cost && n.pp - INTEL.agents.cost >= PRODUCTION.aiReserve) {
    const a = recruit(g, nat);
    if (a) { g.flags['dk:alw'] = Math.max(0, allowance(g, nat) - INTEL.agents.cost); did.push(`recruited agent ${a.name}`); }
  }
  if (!war.length && !S.peace) return did.length ? `${listAnd(did)}.` : null;
  const mine = agentsOf(g, nat).filter((a) => atWork(g, a)).sort((x, y) => x.id - y.id);
  // Where an agent is worth having: the capital of the strongest enemy, for its books, and then the ground
  // in front of the army, most divisions first, no two posts side by side (a watcher sees the provinces round it).
  const strength = (e: string): number => g.corps.reduce((s, c) => s + (c.nat === e && !c.dead ? c.divs * c.str : 0), 0);
  const capitals = war.map((e) => g.nations[e]).filter((E) => E.capital >= 0 && g.regions[E.capital].ctrl === E.id && g.regions[E.capital].owner === E.id)
    .sort((x, y) => strength(y.id) - strength(x.id) || (x.id < y.id ? -1 : 1)).slice(0, 1).map((E) => E.capital);
  const front = [...frontOf(g, nat).entries()].sort((x, y) => y[1] - x[1] || x[0] - y[0]).map(([id]) => id);
  const posts: number[] = [];
  const near = (id: number): boolean => posts.some((p) => p === id || g.regions[p].nb.includes(id));
  for (const id of [...capitals, ...front]) if (!near(id)) posts.push(id);
  // Whoever stands on a post already stays there, and whoever is on the road to one is counted as there.
  const taken = new Set<number>();
  const free: typeof mine = [];
  for (const a of agentsOf(g, nat)) if (posts.includes(a.reg) && abroad(g, a)) taken.add(a.reg);
  // The guard at home is the last to go abroad: kept back once the service has more than one agent and somebody is plotting.
  const guards = agentsOf(g, nat).length > 1 && threat(g, nat).foes.length ? S.guards : 0;
  const home = mine.filter((a) => !abroad(g, a));
  for (const a of mine) {
    if (abroad(g, a)) { if (!posts.includes(a.reg)) free.push(a); continue; }
    if (home.indexOf(a) >= guards) free.push(a);
  }
  for (const a of free) {
    const to = posts.find((p) => !taken.has(p) && whyNotSend(g, nat, a.id, p) === null);
    if (to === undefined) continue;
    if (sendAgent(g, nat, a.id, to)) { taken.add(to); did.push(`sent ${a.name} to ${g.regions[to].name}${capitals.includes(to) ? ' to read the books' : ''}`); }
  }
  // An agent in an enemy capital reads the books; anywhere else abroad it watches. It tries no sabotage.
  for (const a of mine) {
    if (!atWork(g, a) || !abroad(g, a)) continue;
    const m = whyNotMission(g, nat, a.id, 'books') === null ? 'books' : 'watch';
    if (a.mission !== m) setMission(g, nat, a.id, m);
  }
  return did.length ? `${listAnd(did)}.` : null;
}

// ---- The air war --------------------------------------------------------------------------------------

function raidWords(g: Game, r: AirReport): string {
  const at = r.region >= 0 ? g.regions[r.region] : null;
  if (!at) return `ships in the ${g.seas[r.zone]?.name ?? r.place}`;
  switch (r.mission) {
    case 'bomb': return at.fac > 0 ? `the factories at ${at.name}` : `the railway at ${at.name}`;
    case 'night': return `${at.name} by night`;
    case 'ground': return `the troops at ${at.name}`;
    case 'line': return `the fieldworks at ${at.name}`;
    case 'town': return `the town of ${at.name}`;
    case 'superiority': return `the sky over ${at.name}`;
    case 'harbour': return `the ships in harbour at ${at.name}`;
    default: return at.name;
  }
}

const lossesOf = (reps: AirReport[]): { ours: number; theirs: number } => ({
  ours: reps.reduce((s, r) => s + r.att.reduce((t, x) => t + x.lostFtr + x.lostBmr, 0), 0),
  theirs: reps.reduce((s, r) => s + r.def.reduce((t, x) => t + x.lostFtr + x.lostBmr, 0), 0),
});

function air(g: Game, nat: string): string | null {
  const from = g.airReports.length;
  airAI(g, nat);
  const flown = g.airReports.slice(from).filter((r) => r.attNat === nat);
  if (!flown.length) return null;
  const L = lossesOf(flown);
  return `flew ${plural(flown.length, 'raid')}: ${some(flown.map((r) => raidWords(g, r)), 4)}. It cost ${plural(Math.round(L.ours), 'aircraft', 'aircraft')}${L.theirs >= 0.5 ? ` and shot down ${Math.round(L.theirs)}` : ''}.`;
}

// ---- The navy -----------------------------------------------------------------------------------------

function navy(g: Game, nat: string): string | null {
  const was = new Map(fleetsOf(g, nat).map((f) => [f.id, { zone: f.zone, stance: f.stance }]));
  const shore = g.shoreReports.length, strikes = g.airReports.length;
  navalAI(g, nat);
  shoreAI(g, nat);
  // The carriers go with the navy: their strikes are the admirals' to fly.
  seaStaff(g, nat, 'decks');
  const did: string[] = [];
  const out: string[] = [], back: string[] = [];
  for (const f of fleetsOf(g, nat)) {
    const w = was.get(f.id);
    if (!w || (w.zone === f.zone && w.stance === f.stance)) continue;
    if (atSea(f)) out.push(`the ${f.name} into the ${g.seas[f.zone].name}${f.stance === 'escort' ? ' with the convoys' : f.stance === 'raid' ? ' to raid' : ''}`);
    else if (w.stance !== 'port') back.push(`the ${f.name}`);
  }
  if (out.length) did.push(`sent ${some(out)}`);
  if (back.length) did.push(`brought ${some(back)} into harbour`);
  const shelled = g.shoreReports.slice(shore).filter((r) => r.nat === nat).map((r) => g.regions[r.region].name);
  if (shelled.length) did.push(`shelled ${some([...new Set(shelled)])}`);
  const flown = g.airReports.slice(strikes).filter((r) => r.attNat === nat);
  if (flown.length) did.push(`flew ${plural(flown.length, 'carrier strike')} at ${some(flown.map((r) => raidWords(g, r)))}`);
  return did.length ? `${listAnd(did)}.` : null;
}

// ---- The staff's month --------------------------------------------------------------------------------

const WORK: Record<DeskId, (g: Game, nat: string) => string | null> = { research, build, diplomacy, spies, air, navy };
/** The order the desks are worked in: money before the service that spends it, the fleets before the aircraft that fly from them. */
const ORDER: DeskId[] = ['research', 'build', 'diplomacy', 'spies', 'navy', 'air'];

/**
 * One desk's month, done by the staff. What the routine itself printed for the player is folded into the desk's
 * one line of news; the reports of its raids and bombardments stay where battles are read.
 */
function runDesk(g: Game, nat: string, desk: DeskId): void {
  const from = g.headlines.length;
  const line = WORK[desk](g, nat);
  const made = g.headlines.splice(from);
  for (const h of made) if (!(h.nat === nat && (h.kind === 'info' || h.kind === 'battle'))) g.headlines.push(h);
  markRan(g, desk);
  if (line) g.headlines.push({ turn: g.turn, week: g.week, kind: 'info', text: `${deskDef(desk)!.lead} ${line}`, nat, desk });
}

/** The month's work on every desk the player has handed to his staff, as his month opens. */
export function staffMonth(g: Game): void {
  const nat = g.human;
  if (!depthRules(g) || nat === null || !g.nations[nat]?.alive) return;
  for (const desk of ORDER) if (staffRuns(g, nat, desk) && !deskRan(g, desk)) runDesk(g, nat, desk);
}

/**
 * Hands a desk to the staff or takes it back. With 'now' (the player's own month is open) a staff that is given a
 * desk goes to work on it at once, unless it has already done that desk's work this month.
 */
export function handOver(g: Game, desk: DeskId, staff: boolean, now = false): boolean {
  if (!setDesk(g, desk, staff)) return false;
  const nat = g.human;
  if (staff && now && nat !== null && g.nations[nat]?.alive && !deskRan(g, desk)) runDesk(g, nat, desk);
  return true;
}

// ---- The three depths ---------------------------------------------------------------------------------

/** Sets every switch a depth names: the desks, the cabinet, whether it can fall, and the fog of war. */
export function setDepth(g: Game, id: string, now = false): boolean {
  const d = DEPTH.depths.find((x) => x.id === id);
  if (!d || !depthRules(g)) return false;
  setCabinet(g, d.cabinet);
  setCanFall(g, d.canFall);
  setFog(g, d.fog);
  for (const k of ORDER) handOver(g, k, d.staff.includes(k), now);
  return true;
}

/** The depth the game stands at, or null where the player has made a mix of his own. */
export function depthNow(g: Game): DepthDef | null {
  if (!depthRules(g)) return null;
  for (const d of DEPTH.depths) {
    if (DESKS.some((k) => staffRuns(g, g.human, k) !== d.staff.includes(k))) continue;
    if ((g.flags.nofog !== 1) !== d.fog || cabinetOff(g) === d.cabinet) continue;
    // With no cabinet there is nobody to bring him down: the office is not asked about.
    if (d.cabinet && officeSecure(g) === d.canFall) continue;
    return d;
  }
  return null;
}

// ---- What an interface shows --------------------------------------------------------------------------

export interface DeskView {
  id: DeskId; name: string;
  /** The panel that belongs to it, or null. */
  tab: string | null;
  /** True while the staff runs it. */
  staff: boolean;
  /** What that means: the staff's way of running it, or what it asks of the player. */
  text: string;
  /** How it stands now, in a line. */
  state: string;
  /** The staff's line in this month's news, or null if it had nothing to tell. */
  news: string | null;
}

export interface DepthView {
  id: string; name: string; under: string; blurb: string;
  /** True for the depth the game stands at. */
  on: boolean;
  /** The desks on the player's own desk at this depth, and the ones the staff runs, by name. */
  yours: string[]; staff: string[];
  cabinet: boolean; canFall: boolean; fog: boolean;
}

export interface CommandView {
  /** The depth the game stands at, or null for a mix of the player's own. */
  depth: string | null;
  /** Its name, or the words for a mix. */
  name: string; under: string;
  depths: DepthView[];
  desks: DeskView[];
  /** What the staff may spend in a month, in percent of the treasury, and the steps on offer. */
  cap: number; caps: number[];
  /** What is left of this month's allowance, and what building took of it, in production points. */
  left: number; spent: number;
  cabinet: boolean; canFall: boolean; fog: boolean;
}

function stateOf(g: Game, nat: string, desk: DeskId): string {
  const n = g.nations[nat];
  switch (desk) {
    case 'research': {
      const s = techOf(g, nat), teams = teamsOf(g, nat);
      if (!s) return '';
      const at = s.work.map((w) => `${techById(w.id)?.name ?? w.id} (${months(Math.max(1, Math.ceil(w.left)))})`);
      return at.length ? `${s.work.length} of ${teams} team${teams === 1 ? '' : 's'} at work: ${some(at, 4)}.` : `No research team has work.`;
    }
    case 'build': {
      const corps = n.queue.filter((b) => b.what === 'corps').length, other = n.queue.length - corps;
      const deals = dealsOf(g, nat).length, houses = housesOf(g, nat);
      return `${corps ? `${corps} corps forming` : 'No corps forming'}${other ? `, ${other} other order${other === 1 ? '' : 's'} in hand` : ''}. ${deals} of ${houses} trading house${houses === 1 ? '' : 's'} in use.`;
    }
    case 'diplomacy': {
      const b = bloc(n);
      if (!b || b === 'league') return 'Only a member of the Axis or the Allies courts neutrals.';
      const open = Object.values(g.nations).filter((T) => canCourt(g, nat, T.id).ok || g.flags[`court:${nat}:${T.id}`] === g.turn).length;
      return `${Math.floor(n.pc)} political capital in hand; a round of courting costs ${DIPLOMACY.court.cost}. ${open ? `${open} neutral${open === 1 ? '' : 's'} can be talked to.` : 'No neutral is open to courting.'}`;
    }
    case 'spies': {
      const all = agentsOf(g, nat), out = all.filter((a) => abroad(g, a)).length;
      return all.length ? `${all.length} of ${mostAgents(nat)} agents: ${out} abroad, ${all.length - out} at home.` : `No agents yet. The service can run ${mostAgents(nat)}.`;
    }
    case 'air': {
      const all = airFleetsOf(g, nat), flew = all.filter((f) => f.flew === g.turn).length;
      return all.length ? `${all.length} air group${all.length === 1 ? '' : 's'}: ${flew} flew this month.` : 'No air groups.';
    }
    case 'navy': {
      const all = fleetsOf(g, nat), sea = all.filter((f) => atSea(f)).length;
      return all.length ? `${all.length} fleet${all.length === 1 ? '' : 's'}: ${sea} at sea, ${all.length - sea} in harbour.` : 'No fleets.';
    }
  }
}

/** Everything an interface needs to show the depths, the desks and their switches. Null outside the grand campaign or with nobody playing. */
export function commandView(g: Game): CommandView | null {
  const nat = g.human;
  if (!depthRules(g) || nat === null || !g.nations[nat]) return null;
  const now = depthNow(g);
  const nameOf = (k: DeskId): string => deskDef(k)!.name;
  return {
    depth: now?.id ?? null, name: now?.name ?? 'Your own mix', under: now?.under ?? 'Custom',
    depths: DEPTH.depths.map((d) => ({
      id: d.id, name: d.name, under: d.under, blurb: d.blurb, on: now?.id === d.id,
      yours: ['The armies and their battles', ...DESKS.filter((k) => !d.staff.includes(k)).map(nameOf)], staff: d.staff.map(nameOf),
      cabinet: d.cabinet, canFall: d.canFall, fog: d.fog,
    })),
    desks: DEPTH.desks.map((d) => {
      const staff = staffRuns(g, nat, d.id);
      const news = [...g.headlines].reverse().find((h) => h.turn === g.turn && h.desk === d.id && h.nat === nat)?.text ?? null;
      return { id: d.id, name: d.name, tab: d.tab ?? null, staff, text: staff ? d.runs : d.hand, state: stateOf(g, nat, d.id), news };
    }),
    cap: capPct(g), caps: DEPTH.cap.steps,
    left: g.flags['dk:alwt'] === g.turn ? g.flags['dk:alw'] ?? 0 : Math.max(0, g.nations[nat].pp) * capOf(g),
    spent: g.flags['dk:ran:build'] === g.turn ? g.flags['dk:spent'] ?? 0 : 0,
    cabinet: !cabinetOff(g), canFall: !officeSecure(g), fog: g.flags.nofog !== 1,
  };
}

/** Sets the share of the treasury the staff may spend in a month. It takes hold with the next month's allowance. */
export const setStaffCap = (g: Game, pct: number): boolean => setCap(g, pct);
