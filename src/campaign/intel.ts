// Spies and the fog of war (the grand campaign).
// The player no longer sees every army on the map. An enemy or neutral force is seen whole only where the
// player's own side can see it: on the ground it holds, beside its troops, and a province beyond. A province
// further off it is a flag and a count; deeper still, nothing. Agents are how the player sees further. Each is
// a piece: recruited for money, sent to a province, and at work there a month later. An agent watches, reads a
// government's books in its capital, or tries an act of sabotage, and every month abroad it may be caught.
// The computer's staffs see the whole map, as they always did, and keep no agents on it. Their services work
// against the player as a monthly chance of sabotage, which the player's agents on guard at home can stop.
// So a war with nobody at the table is fought exactly as before. Every number lives in data/campaign/intel.json.

import data from '../../data/campaign/intel.json';
import { Rng, hashString } from '../core/rng';
import type { Agent, Game, IntelNote, Mission, Region } from './types';
import { allied, atWar, bloc, fieldArmy, headline } from './game';
import { cutRail, onNet, railCut, ruinMost, ruinedAt, workingFactories, wreckFactory } from './rails';
import { levelIndex } from './difficulty';
import { fieldOut, fireField, stores, type StoresView } from './resources';
import { BUDGET, ledger } from './economy';
import { TECH } from './tech';

interface MissionRow { caught: number; success?: number; gain?: number; months?: number; cut?: number }

export const INTEL = data as unknown as {
  sight: { full: number; glimpse: number };
  agents: { cost: number; most: Record<string, number>; train: number; travel: number; skillAt: number[]; watch: number; ranks: string[] };
  missions: Record<Mission, MissionRow>;
  skill: { caught: number; success: number; guard: number };
  risk: { capital: number; war: number; heat: number; heatMonths: number; lieLow: number };
  caught: { opinion: number };
  battle: { attack: number };
  enemy: { chance: number; quiet: number; level: number[]; services: string[]; kinds: { factory: number; rail: number }; near: number; far: number; most: number };
  estimate: { round: number };
  names: string[];
};

/** True when spies and the fog of war are in the game. That is the grand campaign: a historical scenario shows the whole map. */
export const intelRules = (g: Game): boolean => g.flags.open === 1;

const KEEP_NOTES = 60;
const clamp = (x: number, lo: number, hi: number): number => Math.max(lo, Math.min(hi, x));
const draw = (g: Game, ...key: (string | number)[]): number => new Rng(hashString(`intel:${g.seed}:${key.join(':')}`)).next();

// ---- What can be seen ---------------------------------------------------------------------------------

/** How much the viewer knows of what stands in a province: nothing, a flag and a count, or everything. */
export const SIGHT = { none: 0, glimpse: 1, full: 2 } as const;

/** True while the fog of war lies on the map for this viewer. The player can lift it for good in the secret service's room. */
export function fogOn(g: Game, viewer: string | null = g.human): boolean {
  return intelRules(g) && viewer !== null && !!g.nations[viewer]?.alive && g.flags.nofog !== 1;
}

/** Lifts the fog of war or lays it again. Agents and sabotage go on either way. */
export function setFog(g: Game, on: boolean): boolean {
  if (!intelRules(g)) return false;
  if (on) delete g.flags.nofog; else g.flags.nofog = 1;
  return true;
}

/** An agent is at work once its training or its journey is over. */
export const atWork = (g: Game, a: Agent): boolean => a.ready <= g.turn;
/** An agent is abroad when the province it stands in is not held by its own side. */
export const abroad = (g: Game, a: Agent): boolean => !allied(g, g.regions[a.reg].ctrl, a.nat);
export const agentsOf = (g: Game, nat: string | null): Agent[] => (nat === null || !intelRules(g) ? [] : g.intel.agents.filter((a) => a.nat === nat));

/**
 * What the viewer can see of every province: 2 where an army is seen whole, 1 where only its flag and the number
 * of its corps are known, 0 where nothing is known. Who holds the ground is always known. Outside the grand
 * campaign, with nobody at the table, or with the fog lifted, everything is 2.
 */
export function sightOf(g: Game, viewer: string | null = g.human): Uint8Array {
  const out = new Uint8Array(g.regions.length);
  if (!fogOn(g, viewer)) return out.fill(SIGHT.full);
  const me = viewer as string;
  const S = INTEL.sight, far = Math.max(S.full, S.glimpse);
  const dist = new Int16Array(g.regions.length).fill(-1);
  let ring: number[] = [];
  for (const r of g.regions) {
    if (allied(g, r.ctrl, me) || r.units.some((u) => !u.dead && allied(g, u.nat, me))) { dist[r.id] = 0; ring.push(r.id); }
  }
  for (const a of agentsOf(g, me)) if (atWork(g, a) && dist[a.reg] !== 0) { dist[a.reg] = 0; ring.push(a.reg); }
  for (let d = 1; d <= far && ring.length; d++) {
    const next: number[] = [];
    for (const id of ring) for (const j of g.regions[id].nb) if (dist[j] < 0) { dist[j] = d; next.push(j); }
    ring = next;
  }
  for (let i = 0; i < out.length; i++) out[i] = dist[i] < 0 ? SIGHT.none : dist[i] <= S.full ? SIGHT.full : SIGHT.glimpse;
  return out;
}

/** One nation's force in a province as the viewer knows it: divisions is null where only the flag and the count are known. */
export interface ArmySeen { reg: number; nat: string; sight: 1 | 2; corps: number; divs: number | null; mine: boolean }

/** Every army the viewer knows of, for a map to draw. Armies in ground the viewer cannot see are not on the list. */
export function armiesSeen(g: Game, viewer: string | null = g.human): ArmySeen[] {
  const sight = sightOf(g, viewer);
  const out: ArmySeen[] = [];
  for (const r of g.regions) {
    if (!r.units.length) continue;
    const by = new Map<string, { corps: number; divs: number }>();
    for (const c of r.units) {
      if (c.dead) continue;
      const row = by.get(c.nat) ?? by.set(c.nat, { corps: 0, divs: 0 }).get(c.nat)!;
      row.corps++; row.divs += c.divs * c.str;
    }
    for (const [nat, row] of by) {
      const mine = viewer !== null && allied(g, nat, viewer);
      const s = mine ? SIGHT.full : sight[r.id];
      if (s === SIGHT.none) continue;
      out.push({ reg: r.id, nat, sight: s as 1 | 2, corps: row.corps, divs: s === SIGHT.full ? row.divs : null, mine });
    }
  }
  return out;
}

// ---- Agents ---------------------------------------------------------------------------------------------

/** An agent's rank, 1 to 3: earned from work that came off. */
export const skillOf = (a: Agent): number => 1 + INTEL.agents.skillAt.filter((x) => a.xp >= x).length;
export const rankOf = (a: Agent): string => INTEL.agents.ranks[skillOf(a) - 1] ?? INTEL.agents.ranks[0];
/** How many agents a nation's service can run at once. */
export const mostAgents = (nat: string): number => INTEL.agents.most[nat] ?? INTEL.agents.most._default;
/** What an agent costs, as the interface shows money. */
export const agentCost = (): number => Math.round(INTEL.agents.cost * BUDGET.scale);

const find = (g: Game, nat: string, id: number): Agent | undefined => (intelRules(g) ? g.intel.agents.find((a) => a.id === id && a.nat === nat) : undefined);

/** Where a new agent is trained and a recalled one goes: the capital while it is held, else any home ground, else an ally's capital. */
export function homeBase(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (!n) return -1;
  if (n.capital >= 0 && g.regions[n.capital].ctrl === nat) return n.capital;
  const own = g.regions.find((r) => r.ctrl === nat && r.owner === nat) ?? g.regions.find((r) => r.ctrl === nat);
  if (own) return own.id;
  const friend = g.regions.find((r) => r.cap && r.ctrl !== nat && allied(g, r.ctrl, nat));
  return friend ? friend.id : -1;
}

function note(g: Game, a: { name: string; nat: string }, reg: number, what: string, text: string, good: boolean): void {
  const N = g.intel.notes;
  N.push({ turn: g.turn, agent: a.name, nat: a.nat, reg, what, text, good });
  if (N.length > KEEP_NOTES) N.splice(0, N.length - KEEP_NOTES);
}

function codename(g: Game, id: number): string {
  const used = new Set([...g.intel.agents.map((a) => a.name), ...g.intel.notes.map((n) => n.agent)]);
  const N = INTEL.names, at = hashString(`intelname:${g.seed}:${id}`) % N.length;
  for (let k = 0; k < N.length; k++) if (!used.has(N[(at + k) % N.length])) return N[(at + k) % N.length];
  return `${N[at]} ${id}`;
}

const waitWords = (g: Game, a: Agent): string => { const m = a.ready - g.turn; return m <= 1 ? 'until next month' : `for ${m} more months`; };

/** Why this nation cannot recruit an agent, or null if it can. */
export function whyNotRecruit(g: Game, nat: string): string | null {
  if (!intelRules(g)) return 'Secret services belong to the grand campaign.';
  const n = g.nations[nat];
  if (!n?.alive) return 'There is no government left to run a secret service.';
  const most = mostAgents(nat);
  if (agentsOf(g, nat).length >= most) return `Your service can run ${most} agents at once. Retire one to make room.`;
  if (n.pp < INTEL.agents.cost) return `An agent costs ${agentCost()} to find and train. The treasury does not have it.`;
  if (homeBase(g, nat) < 0) return 'There is nowhere left to train one.';
  return null;
}

/** Recruits an agent. It trains at home and is ready for orders next month. */
export function recruit(g: Game, nat: string): Agent | null {
  if (whyNotRecruit(g, nat)) return null;
  const id = g.intel.next++;
  const a: Agent = { id, nat, name: codename(g, id), xp: 0, reg: homeBase(g, nat), ready: g.turn + INTEL.agents.train, mission: 'guard', born: g.turn };
  g.nations[nat].pp -= INTEL.agents.cost;
  g.intel.agents.push(a);
  note(g, a, a.reg, 'recruit', `Agent ${a.name} is recruited and begins training at ${g.regions[a.reg].name}.`, true);
  return a;
}

/** Why an agent cannot be sent to a province, or null if it can. */
export function whyNotSend(g: Game, nat: string, id: number, to: number): string | null {
  const a = find(g, nat, id);
  if (!a) return 'There is no such agent.';
  const r = g.regions[to];
  if (!r) return 'Pick a province.';
  if (!atWork(g, a)) return `${a.name} is ${a.born + INTEL.agents.train === a.ready && a.xp === 0 && !a.months ? 'in training' : 'on the road'} ${waitWords(g, a)}.`;
  if (a.reg === to) return `${a.name} is already at ${r.name}.`;
  if (g.nations[r.ctrl]?.dormant) return `There is nothing at ${r.name} to watch.`;
  return null;
}

/** Sends an agent to a province. It is on the road for a month and at work there from the next: watching abroad, on guard at home. */
export function sendAgent(g: Game, nat: string, id: number, to: number): boolean {
  if (whyNotSend(g, nat, id, to)) return false;
  const a = find(g, nat, id)!;
  a.reg = to;
  a.ready = g.turn + INTEL.agents.travel;
  a.mission = abroad(g, a) ? 'watch' : 'guard';
  return true;
}

/** Retires an agent, which frees a place in the service. */
export function retire(g: Game, nat: string, id: number): boolean {
  const a = find(g, nat, id);
  if (!a) return false;
  g.intel.agents.splice(g.intel.agents.indexOf(a), 1);
  note(g, a, a.reg, 'retire', `Agent ${a.name} is retired from the service.`, true);
  return true;
}

// ---- Missions -------------------------------------------------------------------------------------------

export const MISSIONS: Mission[] = ['watch', 'books', 'factory', 'rail', 'oil', 'guard'];
const SABOTAGE: Mission[] = ['factory', 'rail', 'oil'];
export const isSabotage = (m: Mission): boolean => SABOTAGE.includes(m);

export const MISSION_TEXT: Record<Mission, { label: string; doing: string; text: string }> = {
  watch: { label: 'Watch and report', doing: 'watching', text: 'Reports every army in the province and the provinces around it: each corps, its kind and its strength. In the province itself the agent also reports what is on order there and which air fleets are based there, and your armies attack it harder for knowing its defences.' },
  books: { label: 'Read the books', doing: 'reading the government\'s books', text: 'In a capital: the government\'s treasury and income, the true size of its army, what it has on order, what its laboratories are working on and what is in its depots.' },
  factory: { label: 'Wreck a factory', doing: 'preparing to wreck a factory', text: 'One working factory is put out of action until engineers rebuild it. Tried once, next month, and then the agent must lie low.' },
  rail: { label: 'Cut the railway', doing: 'preparing to cut the railway', text: 'The track is blown. No train runs through the province until engineers relay it. Tried once, next month, and then the agent must lie low.' },
  oil: { label: 'Fire the oilfield', doing: 'preparing to fire the oilfield', text: `The wells are set alight: the field gives ${Math.round((1 - (data as any).missions.oil.cut) * 100)}% of its oil for ${(data as any).missions.oil.months} months. Tried once, next month, and then the agent must lie low.` },
  guard: { label: 'Guard against saboteurs', doing: 'on guard', text: 'Watches for enemy agents here and in the provinces around, and is some help everywhere at home.' },
};

/** The chance each month that the agent is caught at this work where it stands. */
export function caughtChance(g: Game, a: Agent, m: Mission = a.mission): number {
  if (!abroad(g, a)) return 0;
  const r = g.regions[a.reg], K = INTEL.risk;
  let p = INTEL.missions[m].caught;
  if (r.cap && m !== 'books') p *= K.capital;
  if (atWar(g, a.nat, r.ctrl)) p *= K.war;
  if ((g.intel.heat[r.id] ?? 0) > g.turn) p *= K.heat;
  p *= 1 - INTEL.skill.caught * (skillOf(a) - 1);
  return clamp(p, 0, 0.95);
}

/** The chance that an act of sabotage by this agent comes off. */
export const successChance = (a: Agent, m: Mission): number => clamp((INTEL.missions[m].success ?? 0) + INTEL.skill.success * (skillOf(a) - 1), 0, 0.95);

/** Why an agent cannot be given this work where it stands, or null if it can. */
export function whyNotMission(g: Game, nat: string, id: number, m: Mission): string | null {
  const a = find(g, nat, id);
  if (!a) return 'There is no such agent.';
  const r = g.regions[a.reg], host = g.nations[r.ctrl];
  if (!atWork(g, a)) return `${a.name} is not yet at work: ${waitWords(g, a)}.`;
  if (m === 'guard') return abroad(g, a) ? 'An agent can stand guard only on friendly ground.' : null;
  if (!abroad(g, a)) return `${a.name} is on friendly ground. Send the agent abroad first.`;
  if (m === 'watch') return null;
  if (m === 'books') {
    if (!host?.alive || host.capital !== r.id || r.owner !== r.ctrl) return 'A government keeps its books in its capital. Send the agent to one.';
    return null;
  }
  if (!atWar(g, nat, r.ctrl)) return `You are at peace with ${host?.name ?? 'this country'}. Sabotage is an act of war.`;
  if ((a.rest ?? 0) > g.turn) { const k = (a.rest as number) - g.turn; return `${a.name} is lying low for ${k === 1 ? 'another month' : `${k} more months`} after the last attempt.`; }
  if (m === 'factory') {
    if (r.fac <= 0) return `There is no industry at ${r.name}.`;
    if (workingFactories(g, r) <= 0 || ruinedAt(g, r) >= ruinMost(r)) return `No factory at ${r.name} is left to wreck.`;
    return null;
  }
  if (m === 'rail') {
    if (!onNet(r.id)) return `No railway runs through ${r.name}.`;
    if (railCut(g, r.id)) return `The railway at ${r.name} is already cut.`;
    return null;
  }
  if (!r.oil) return `There is no oilfield at ${r.name}.`;
  if (fieldOut(g, r) > 0) return `The wells at ${r.name} are already burning.`;
  return null;
}

/** Gives an agent its work. Watching and guarding begin at once; an act of sabotage is tried when the month turns. */
export function setMission(g: Game, nat: string, id: number, m: Mission): boolean {
  if (whyNotMission(g, nat, id, m)) return false;
  find(g, nat, id)!.mission = m;
  return true;
}

// ---- What an agent brings home ----------------------------------------------------------------------

/** True if the viewer has an agent at work in this very province. */
export const agentAt = (g: Game, reg: number, viewer: string | null = g.human): Agent | null =>
  agentsOf(g, viewer).find((a) => a.reg === reg && atWork(g, a) && abroad(g, a)) ?? null;

/** What an agent in a province reports of it beyond the armies: what is on order there and which air fleets are based there. */
export interface CityReport { reg: number; agent: string; orders: { name: string; eta: number }[]; air: { name: string; nat: string; ftr: number; bmr: number }[]; factories: number; ruined: number }

export function cityReport(g: Game, reg: number, viewer: string | null = g.human): CityReport | null {
  const a = agentAt(g, reg, viewer);
  const r = g.regions[reg];
  if (!a || !r) return null;
  const orders: CityReport['orders'] = [];
  for (const n of Object.values(g.nations)) for (const b of n.queue) if (b.region === reg && n.id === r.ctrl) orders.push({ name: b.name, eta: b.eta });
  return {
    reg, agent: a.name, orders: orders.sort((x, y) => x.eta - y.eta),
    air: g.airFleets.filter((f) => f.base === reg && !allied(g, f.nat, a.nat)).map((f) => ({ name: f.name, nat: f.nat, ftr: Math.round(f.ftr), bmr: Math.round(f.bmr) })),
    factories: r.fac, ruined: ruinedAt(g, r),
  };
}

/** True if the viewer may read this nation's books: its own, an ally's, or one whose capital has an agent of the viewer's reading them. */
export function knowsBooks(g: Game, of: string, viewer: string | null = g.human): boolean {
  if (!intelRules(g) || viewer === null || !g.nations[viewer]?.alive) return true;
  if (allied(g, of, viewer)) return true;
  const n = g.nations[of];
  if (!n?.alive || n.capital < 0) return false;
  return agentsOf(g, viewer).some((a) => a.reg === n.capital && a.mission === 'books' && atWork(g, a) && g.regions[a.reg].ctrl === of);
}

/** The size of a nation's army as the viewer knows it: to the division with its books open, to the nearest ten without. */
export function armyKnown(g: Game, of: string, viewer: string | null = g.human): { divs: number; exact: boolean } {
  const all = fieldArmy(g, of);
  if (!fogOn(g, viewer) || knowsBooks(g, of, viewer)) return { divs: all, exact: true };
  const k = INTEL.estimate.round;
  return { divs: Math.max(k, Math.round(all / k) * k), exact: false };
}

/** A foreign government's books, as an agent in its capital reads them. */
export interface Books {
  nat: string; agent: string | null;
  treasury: number; gross: number; upkeep: number; net: number; scale: number;
  divisions: number; corps: number;
  queue: { name: string; what: string; eta: number; region: number }[];
  research: { name: string; left: number }[];
  researched: number;
  stores: StoresView | null;
}

export function booksOf(g: Game, of: string, viewer: string | null = g.human): Books | null {
  const n = g.nations[of];
  if (!intelRules(g) || !n?.alive || !knowsBooks(g, of, viewer)) return null;
  const L = ledger(g, of);
  const a = agentsOf(g, viewer).find((x) => x.reg === n.capital && x.mission === 'books' && atWork(g, x));
  return {
    nat: of, agent: a?.name ?? null,
    treasury: L.treasury, gross: L.gross, upkeep: L.upkeepTotal, net: L.net, scale: L.scale,
    divisions: fieldArmy(g, of), corps: g.corps.filter((c) => c.nat === of && !c.dead).length,
    queue: [...n.queue].sort((x, y) => x.eta - y.eta).map((b) => ({ name: b.name, what: b.what, eta: b.eta, region: b.region })),
    research: (n.tech?.work ?? []).map((w) => ({ name: TECH.techs.find((t) => t.id === w.id)?.name ?? w.id, left: Math.max(1, Math.ceil(w.left - 1e-9)) })),
    researched: n.tech?.done.length ?? 0,
    stores: stores(g, of),
  };
}

/** What an agent at work in a province is worth to its own side's attack on it: the defences are mapped. */
export function intelAttack(g: Game, nat: string, target: Region): { mul: number; agent: string } | null {
  if (!intelRules(g) || !g.intel.agents.length) return null;
  const a = g.intel.agents.find((x) => x.reg === target.id && atWork(g, x) && allied(g, x.nat, nat) && abroad(g, x));
  return a ? { mul: 1 + INTEL.battle.attack, agent: a.name } : null;
}

// ---- The month's secret war ---------------------------------------------------------------------------

function lose(g: Game, a: Agent, text: string, what: string): void {
  g.intel.agents.splice(g.intel.agents.indexOf(a), 1);
  note(g, a, a.reg, what, text, false);
  headline(g, 'event', text, a.nat, a.reg);
}

function learn(g: Game, a: Agent, gain: number): void {
  const was = skillOf(a);
  a.xp += gain;
  if (skillOf(a) > was) {
    const text = `Agent ${a.name} has earned the rank of ${rankOf(a)}: harder to catch, and surer in what is attempted.`;
    note(g, a, a.reg, 'rank', text, true);
    headline(g, 'info', text, a.nat, a.reg);
  }
}

/** One agent's act of sabotage, tried the month after it is ordered. */
function attempt(g: Game, a: Agent): void {
  const m = a.mission, r = g.regions[a.reg], host = g.nations[r.ctrl];
  const ok = draw(g, g.turn, a.id, 'try') < successChance(a, m);
  let text: string;
  let done = false;
  if (ok && m === 'factory' && wreckFactory(g, r)) { done = true; text = `Agent ${a.name} wrecks a factory at ${r.name}. ${host.adj} industry is one works short until engineers rebuild it.`; }
  else if (ok && m === 'rail' && cutRail(g, r.id)) { done = true; text = `Agent ${a.name} blows the railway at ${r.name}. No ${host.adj} train runs through until engineers relay the track.`; }
  else if (ok && m === 'oil' && fireField(g, r, INTEL.missions.oil.months ?? 3, INTEL.missions.oil.cut ?? 0.5)) { done = true; text = `Agent ${a.name} sets the oilfield at ${r.name} alight. The wells will burn for ${INTEL.missions.oil.months} months.`; }
  else text = m === 'factory' ? `Agent ${a.name}'s attempt on the factories at ${r.name} fails: the guards were too many.`
    : m === 'rail' ? `Agent ${a.name}'s charges on the railway at ${r.name} are found before they go off.`
      : `Agent ${a.name} cannot get near the wells at ${r.name}.`;
  note(g, a, r.id, done ? 'sabotage' : 'failed', text, done);
  headline(g, done ? 'event' : 'info', text, a.nat, r.id);
  a.mission = 'watch';
  a.rest = g.turn + INTEL.risk.lieLow;
  g.intel.heat[r.id] = g.turn + INTEL.risk.heatMonths;
  if (done) learn(g, a, INTEL.missions[m].gain ?? 1);
}

/** A neutral that catches a spy thinks the worse of the side that sent it. */
function resent(g: Game, a: Agent, host: string): string {
  const H = g.nations[host], b = bloc(g.nations[a.nat]);
  if (!H?.alive || H.atWar.length || H.league || bloc(H) !== null || (b !== 'axis' && b !== 'allies')) return '';
  const k = b === 'axis' ? -1 : 1;
  H.opinion = clamp(H.opinion + k * INTEL.caught.opinion, -100, 100);
  return ` ${H.name} protests, and thinks the worse of ${b === 'axis' ? 'the Axis' : 'the Allies'} for it.`;
}

/** The chance each month that the services of the powers at war with this nation strike on its home ground, and who they are. */
export function threat(g: Game, nat: string): { chance: number; each: number; foes: string[] } {
  const E = INTEL.enemy;
  // The other side's services need a few months to get their people into place: nothing happens at the very start.
  const quiet = g.flags.spyfrom !== undefined && g.turn < g.flags.spyfrom + E.quiet;
  const each = intelRules(g) && !quiet ? E.chance * (E.level[levelIndex(g)] ?? 1) : 0;
  const foes = each > 0 ? g.order.filter((f) => E.services.includes(f) && g.nations[f]?.alive && atWar(g, nat, f)) : [];
  return { chance: 1 - Math.pow(1 - each, foes.length), each, foes };
}

/** The chance that this nation's guards stop a plot against a province: best with a guard in it or beside it. */
export function guardChance(g: Game, nat: string, reg: number): { chance: number; by: Agent | null } {
  const E = INTEL.enemy;
  const guards = agentsOf(g, nat).filter((a) => a.mission === 'guard' && atWork(g, a) && !abroad(g, a));
  if (!guards.length) return { chance: 0, by: null };
  const r = g.regions[reg];
  const near = guards.filter((a) => a.reg === reg || r.nb.includes(a.reg)).sort((x, y) => skillOf(y) - skillOf(x))[0] ?? null;
  const p = near ? E.near + INTEL.skill.guard * (skillOf(near) - 1) + E.far * (guards.length - 1) : E.far * guards.length;
  return { chance: clamp(p, 0, E.most), by: near ?? [...guards].sort((x, y) => skillOf(y) - skillOf(x) || x.id - y.id)[0] };
}

/** The other side's services at work against the player's home ground. */
function plots(g: Game, nat: string): void {
  const T = threat(g, nat);
  for (const foe of T.foes) {
    if (draw(g, g.turn, 'plot', foe) >= T.each) continue;
    const F = g.nations[foe], K = INTEL.enemy.kinds;
    const rail = draw(g, g.turn, 'kind', foe) < K.rail / (K.rail + K.factory);
    // Factories are picked by how many a province has; a railway anywhere on home ground will do.
    const pool: Region[] = [];
    for (const r of g.regions) {
      if (r.ctrl !== nat || r.owner !== nat || r.overseas) continue;
      if (rail) { if (onNet(r.id) && !railCut(g, r.id)) pool.push(r); }
      else if (workingFactories(g, r) > 0 && ruinedAt(g, r) < ruinMost(r)) for (let k = 0; k < workingFactories(g, r); k++) pool.push(r);
    }
    if (!pool.length) continue;
    const r = pool[Math.floor(draw(g, g.turn, 'where', foe) * pool.length) % pool.length];
    const G = guardChance(g, nat, r.id);
    const who = { name: G.by?.name ?? '', nat };
    if (G.by && draw(g, g.turn, 'guard', foe) < G.chance) {
      const text = `Agent ${G.by.name}'s watchers catch ${/^[AEIOU]/.test(F.adj) ? 'an' : 'a'} ${F.adj} sabotage ring at ${r.name} before it can strike.`;
      note(g, who, r.id, 'foiled', text, true);
      headline(g, 'info', text, nat, r.id);
      learn(g, G.by, 1);
      continue;
    }
    const struck = rail ? cutRail(g, r.id) : wreckFactory(g, r);
    if (!struck) continue;
    const text = rail
      ? `Saboteurs blow the railway at ${r.name}. Your police blame ${F.adj} agents. No train runs through until engineers relay the track.`
      : `Saboteurs wreck a factory at ${r.name}. Your police blame ${F.adj} agents. It stands idle until engineers rebuild it.`;
    note(g, who, r.id, 'struck', text, false);
    headline(g, 'event', text, nat, r.id);
  }
}

/**
 * The secret war for a month, run as the player's month opens: acts of sabotage ordered last month are tried,
 * agents abroad may be caught, quiet work is counted toward rank, and the other side's services strike at home.
 * Nothing happens for a computer-run nation, or outside the grand campaign.
 */
export function intelTick(g: Game, nat: string): void {
  if (!intelRules(g) || nat !== g.human || !g.nations[nat]?.alive) return;
  if (g.flags.spyfrom === undefined) g.flags.spyfrom = g.turn;
  for (const a of [...agentsOf(g, nat)].sort((x, y) => x.id - y.id)) {
    if (!atWork(g, a)) continue;
    const r = g.regions[a.reg];
    if (!abroad(g, a)) { a.mission = 'guard'; continue; }
    // Ground that has changed hands under an agent: a guard finds itself behind the lines, a reader's capital has fallen.
    if (a.mission === 'guard' || (a.mission !== 'watch' && whyNotMission(g, nat, a.id, a.mission))) a.mission = 'watch';
    const was = a.mission;
    // The risk is the one the player was shown when the order was given: the alarm an attempt raises counts from next month.
    const risk = caughtChance(g, a, was);
    if (isSabotage(was)) attempt(g, a);
    if (draw(g, g.turn, a.id, 'caught') < risk) {
      const host = g.nations[r.ctrl];
      lose(g, a, `Agent ${a.name} is caught at ${r.name} by ${host.adj} counter-intelligence. Nothing more is heard.${resent(g, a, r.ctrl)}`, 'caught');
      continue;
    }
    a.months = (a.months ?? 0) + 1;
    if (a.months % INTEL.agents.watch === 0) learn(g, a, 1);
  }
  for (const k of Object.keys(g.intel.heat)) if (g.intel.heat[+k] <= g.turn) delete g.intel.heat[+k];
  plots(g, nat);
}

// ---- The secret service's room, for the interface --------------------------------------------------------

export interface MissionOffer { id: Mission; label: string; text: string; ok: boolean; why: string; now: boolean; caught: number; success: number | null }

export interface AgentView {
  id: number; nat: string; name: string; rank: string; skill: number; xp: number;
  /** Experience at which the next rank comes, or null at the top. */
  next: number | null;
  reg: number; where: string; host: string; abroad: boolean;
  atWork: boolean;
  /** Months until the agent is at work: 0 when it is. */
  wait: number;
  mission: Mission;
  /** What the agent is doing, in words. */
  doing: string;
  /** The chance this month of being caught. */
  caught: number;
  /** Months the agent must still lie low before another act of sabotage. */
  rest: number;
  missions: MissionOffer[];
  /** The armies the agent has in sight where it stands: one row for each nation's force. */
  sees: { reg: number; nat: string; corps: number; divs: number }[];
}

export interface SpyRoom {
  nat: string;
  agents: AgentView[];
  most: number;
  cost: number;
  whyNotRecruit: string | null;
  notes: IntelNote[];
  fog: boolean;
  /** Foreign nations whose books an agent is reading. */
  books: string[];
  /** The other side's services: the chance of a strike at home this month, and who is behind it. */
  threat: { chance: number; foes: string[]; guards: number };
}

function viewOf(g: Game, a: Agent): AgentView {
  const r = g.regions[a.reg], out = abroad(g, a), work = atWork(g, a);
  const offers: MissionOffer[] = (out ? MISSIONS.filter((m) => m !== 'guard') : (['guard'] as Mission[])).map((m) => {
    const why = whyNotMission(g, a.nat, a.id, m);
    return { id: m, label: MISSION_TEXT[m].label, text: MISSION_TEXT[m].text, ok: why === null, why: why ?? '', now: a.mission === m, caught: caughtChance(g, a, m), success: isSabotage(m) ? successChance(a, m) : null };
  });
  const sees: AgentView['sees'] = [];
  if (work && out) {
    for (const id of [r.id, ...r.nb]) {
      const by = new Map<string, { corps: number; divs: number }>();
      for (const c of g.regions[id].units) {
        if (c.dead || allied(g, c.nat, a.nat)) continue;
        const row = by.get(c.nat) ?? by.set(c.nat, { corps: 0, divs: 0 }).get(c.nat)!;
        row.corps++; row.divs += c.divs * c.str;
      }
      for (const [nat, row] of by) sees.push({ reg: id, nat, ...row });
    }
  }
  const T = INTEL.agents.skillAt;
  const training = !work && a.born + INTEL.agents.train === a.ready && a.xp === 0 && !a.months;
  const doing = !work ? `${training ? 'In training' : `On the road to ${r.name}`}: at work ${a.ready - g.turn <= 1 ? 'next month' : `in ${a.ready - g.turn} months`}`
    : `${a.mission === 'guard' ? 'On guard' : a.mission === 'watch' ? 'Watching' : a.mission === 'books' ? 'Reading the books' : MISSION_TEXT[a.mission].label + ': tried next month'} at ${r.name}`;
  return {
    id: a.id, nat: a.nat, name: a.name, rank: rankOf(a), skill: skillOf(a), xp: a.xp, next: T[skillOf(a) - 1] ?? null,
    reg: a.reg, where: r.name, host: r.ctrl, abroad: out, atWork: work, wait: Math.max(0, a.ready - g.turn),
    mission: a.mission, doing, caught: work ? caughtChance(g, a) : 0, rest: Math.max(0, (a.rest ?? 0) - g.turn),
    missions: offers, sees,
  };
}

/** Everything the secret service's room shows. Null outside the grand campaign or with nobody at the table. */
export function spyRoom(g: Game, nat: string | null = g.human): SpyRoom | null {
  if (!intelRules(g) || nat === null || !g.nations[nat]) return null;
  const mine = agentsOf(g, nat);
  const T = threat(g, nat);
  return {
    nat, agents: mine.map((a) => viewOf(g, a)), most: mostAgents(nat), cost: agentCost(), whyNotRecruit: whyNotRecruit(g, nat),
    notes: g.intel.notes.filter((n) => n.nat === nat).slice().reverse(),
    fog: fogOn(g, nat),
    books: Object.values(g.nations).filter((n) => n.alive && !allied(g, n.id, nat) && knowsBooks(g, n.id, nat)).map((n) => n.id),
    threat: { chance: T.chance, foes: T.foes, guards: mine.filter((a) => a.mission === 'guard' && atWork(g, a) && !abroad(g, a)).length },
  };
}
