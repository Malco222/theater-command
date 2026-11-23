// The technology tree of the grand campaign. Five branches, four steps deep: a nation's research teams each work
// on one technology at a time, a technology can be started once the ones before it are done, and what it gives
// lasts for the rest of the war: better tanks, a new kind of formation, cheaper ships, another research team.
// Everything lives in data/campaign/tech.json, so every version of the game reads the same tree. A historical
// scenario never sees it: there, research runs on history's clock (research.ts).

import { staffRuns } from './depth';
import data from '../../data/campaign/tech.json';
import supportData from '../../data/campaign/support.json';
import { turnToDate } from '../core/ticket';
import { EASTERN, allied, atWar, headline } from './game';
import { RESEARCH } from './research';
import { traitRule, type Trait, type When } from './traits';
import { RES, keepsStores, oilMonths } from './resources';
import { researchSpeed } from './buildings';
import { govMul } from './government';
import { PLANS, planRules, whyNoTechPlans } from './plans';
import type { Field, Game, Nation, TechState } from './types';
import { controlFirst, radarWords } from './aircampaigns';
import { techLift } from './lift';

export type Branch = 'land' | 'air' | 'naval' | 'industry' | 'doctrine' | 'intel';

export interface Tech {
  id: string;
  branch: Branch;
  /** The step of the tree it stands on, 1 to 4. */
  tier: number;
  /** The year it belongs to: begun earlier, it takes longer. */
  year: number;
  /** Months of one team's work, when not the usual for its step. */
  months?: number;
  needs: string[];
  name: string;
  text: string;
  /** Raises this branch of research.json by a level. */
  field?: Field;
  /** Orders that cannot be placed without it: 'corps:arm', 'ship:cv', 'para'. */
  unlock?: string[];
  /** More research teams. */
  slots?: number;
  /** Ends the eastern winter's penalty for the nation's troops. */
  winterKit?: boolean;
  /** One of the infantry's support weapons (rulings 146 and 152): what it does is worked out in support.ts. */
  weapons?: boolean;
  /** Oil made each month by every working factory at home, out of any raider's reach. */
  oil?: number;
  sealift?: number;
  attack?: number;
  defence?: number;
  when?: When;
  drafts?: number;
  cost?: { what: string[]; mul: number };
  fleet?: number;
  seaLoss?: number;
  /** The Intelligence branch (rider b542 on ruling 235): what it does for the nation's agents (intel.ts). */
  spy?: Partial<SpyTech>;
}

/** What the Intelligence branch has done for a nation's agents, all told. */
export interface SpyTech { caught: number; rest: number; theft: number; agents: number; guard: number }

/** A formation the tree unlocks beyond the ordinary kinds. */
export interface TechUnit { label: string; word: string; divs: number; cost: number; months: number; most: number; blurb: string }

export const TECH = data as unknown as {
  months: number[];
  early: number;
  slots: Record<string, number>;
  spyTeams?: number;
  branches: { id: Branch; label: string; text: string }[];
  eras: string[];
  techs: Tech[];
  start: Record<string, string[]>;
  units: { para: TechUnit };
  ai: { tier: number; branch: number; early: number; teams: number; later: number; fuel?: number; winter?: number; guns?: number; skip: Record<string, string[]>; order: Record<string, Branch[]> };
};

const BY_ID: Record<string, Tech> = {};
const GATE: Record<string, Tech> = {};
for (const t of TECH.techs) {
  BY_ID[t.id] = t;
  for (const k of t.unlock ?? []) GATE[k] = t;
}

/** The technology tree is part of the grand campaign only. */
export const techRules = (g: Game): boolean => g.flags.open === 1;

/**
 * The year of the armour a nation's tanks are built to (looks only: the 3D game's model and the shells a tank is drawn
 * firing). Under the grand campaign's technology, read from the tank designs it has done; otherwise from history's clock.
 */
export function armourYear(g: Game, nat: string): number | null {
  const n = g.nations[nat];
  if (!n?.alive) return null;
  if (techRules(g)) {
    const done = n.tech?.done ?? TECH.start[nat] ?? [];
    return !done.includes('mediums') ? 1939 : !done.includes('heavies') ? 1942 : Math.max(1939 + Math.floor((Math.max(1, g.turn) + 7) / 12), 1943);
  }
  if (!n.lab) return null;
  return 1939 + Math.floor((Math.max(1, g.turn + Math.round(n.lab.clock.armour - n.lab.months)) + 7) / 12);
}

export const techById = (id: string): Tech | undefined => BY_ID[id];

/** The technology an order waits on, whoever is asking: 'corps:arm' waits on the tank arm. */
export const gateOf = (key: string): Tech | undefined => GATE[key];

const yearOf = (turn: number): number => turnToDate(Math.max(1, turn)).year;

// ---- What a nation has ------------------------------------------------------------------------------

const fresh = (nat: string): TechState => ({ done: (TECH.start[nat] ?? []).filter((id) => id in BY_ID), work: [] });

/** A nation's research: what is done and what its teams are working on. Null outside the grand campaign. */
export function techOf(g: Game, nat: string): TechState | null {
  if (!techRules(g)) return null;
  const n = g.nations[nat];
  if (!n) return null;
  if (!g.flags['tech:open']) openTech(g);
  return (n.tech ??= fresh(nat));
}

interface Derived { src: string[] | null; n: number; level: Record<string, number>; traits: Trait[]; open: Set<string>; winter: boolean; slots: number }
const NOTHING: Derived = { src: null, n: 0, level: {}, traits: [], open: new Set(), winter: false, slots: 0 };
const cache = new WeakMap<Nation, Derived>();

/** Everything a nation's finished technologies add up to, worked out once and kept until it finishes another. */
function sum(g: Game, nat: string): Derived {
  const t = techOf(g, nat);
  if (!t) return NOTHING;
  const n = g.nations[nat];
  const had = cache.get(n);
  if (had && had.src === t.done && had.n === t.done.length) return had;
  const d: Derived = { src: t.done, n: t.done.length, level: {}, traits: [], open: new Set(), winter: false, slots: 0 };
  for (const id of t.done) {
    const x = BY_ID[id];
    if (!x) continue;
    if (x.field) d.level[x.field] = (d.level[x.field] ?? 0) + 1;
    for (const k of x.unlock ?? []) d.open.add(k);
    if (x.winterKit) d.winter = true;
    d.slots += x.slots ?? 0;
    if (x.attack !== undefined || x.defence !== undefined || x.drafts !== undefined || x.cost || x.fleet !== undefined || x.seaLoss !== undefined) {
      d.traits.push({ id: `tech:${x.id}`, name: x.name, text: x.text, attack: x.attack, defence: x.defence, when: x.when, drafts: x.drafts, cost: x.cost, fleet: x.fleet, seaLoss: x.seaLoss });
    }
  }
  cache.set(n, d);
  return d;
}

/** How many levels the tree has added to one of the eight branches of research: tanks, fighters, war production and the rest. */
export const treeLevel = (g: Game, nat: string, f: Field): number => (g.flags.open === 1 ? sum(g, nat).level[f] ?? 0 : 0);

/** What a nation's technologies give it in battle, in prices, at sea and in its drafts, in the shape of national abilities. */
export const techTraits = (g: Game, nat: string): Trait[] => sum(g, nat).traits;

/** The technology this nation still lacks before it can place this order, or null when nothing stands in the way. */
export function locked(g: Game, nat: string, key: string): Tech | null {
  const t = GATE[key];
  return t && techRules(g) && !sum(g, nat).open.has(key) ? t : null;
}

/** True for a nation whose troops are equipped for the eastern winter. */
export const winterReady = (g: Game, nat: string): boolean => g.flags.open === 1 && sum(g, nat).winter;

/** How many research teams a nation has. Teams founded under the old rules of an earlier save still count. */
export function teamsOf(g: Game, nat: string): number {
  if (!techRules(g)) return 0;
  return (TECH.slots[nat] ?? TECH.slots._default) + sum(g, nat).slots + (g.nations[nat].lab?.founded ?? 0);
}

/**
 * Rider b542 on ruling 235 (Brian: a smaller nation's "tech tree is more around spying efforts"): every nation but
 * the great powers has a team more, its secret service's laboratory, that works the Intelligence branch alone.
 */
export const spyTeamsOf = (g: Game, nat: string): number => (techRules(g) && planRules(g) && !PLANS.great.includes(nat) ? TECH.spyTeams ?? 0 : 0);

/** The work of a nation's ordinary teams, and of its spy team. */
const teamWork = (s: TechState): TechState['work'] => s.work.filter((w) => !w.spy);
const spyWork = (s: TechState): TechState['work'] => s.work.filter((w) => w.spy);

/** True when a new project on this technology would go to the nation's spy team. */
const forSpyTeam = (g: Game, nat: string, s: TechState, t: Tech): boolean => t.branch === 'intel' && spyWork(s).length < spyTeamsOf(g, nat);

export const hasTech = (g: Game, nat: string, id: string): boolean => !!techOf(g, nat)?.done.includes(id);

/** What the Intelligence branch has done for a nation's agents: 'caught' multiplies, the rest add. */
export function spyOf(g: Game, nat: string): SpyTech {
  const out: SpyTech = { caught: 1, rest: 0, theft: 0, agents: 0, guard: 0 };
  for (const id of techOf(g, nat)?.done ?? []) {
    const x = BY_ID[id]?.spy;
    if (!x) continue;
    out.caught *= x.caught ?? 1; out.rest += x.rest ?? 0; out.theft += x.theft ?? 0; out.agents += x.agents ?? 0; out.guard += x.guard ?? 0;
  }
  return out;
}

/** A technology gained with a great power's plans (plans.ts): in service at once, as if researched. */
export function grantTech(g: Game, nat: string, id: string): boolean {
  const s = techOf(g, nat);
  if (!s || !BY_ID[id] || s.done.includes(id)) return false;
  s.done.push(id);
  s.work = s.work.filter((w) => w.id !== id);
  return true;
}

/** The kin of a great power (arsenal.json plans.kin) have its plans of the locked technologies wherever it has researched them. */
function kinTechs(g: Game, n: Nation): void {
  const of = PLANS.kin[n.id];
  if (!of || !planRules(g) || !n.tech) return;
  const m = g.nations[of]?.tech;
  for (const id of PLANS.techs) if (m?.done.includes(id) && !n.tech.done.includes(id)) grantTech(g, n.id, id);
}

// ---- Starting and stopping work ---------------------------------------------------------------------

/** Months one team needs for a technology begun in this month: longer for every year it is ahead of its time. */
export function monthsFor(t: Tech, turn: number): number {
  const base = t.months ?? TECH.months[t.tier];
  return Math.ceil(base * (1 + TECH.early * Math.max(0, t.year - yearOf(turn))) - 1e-9);
}

const and = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/** Why this nation cannot begin this technology now, or null when it can. */
export function whyNotResearch(g: Game, nat: string, id: string): string | null {
  const s = techOf(g, nat), t = BY_ID[id];
  if (!s) return 'The technology tree belongs to the grand campaign.';
  if (!t) return 'There is no such technology.';
  if (!g.nations[nat].alive) return 'The nation is out of the war.';
  if (s.done.includes(id)) return 'Already researched.';
  if (s.work.some((w) => w.id === id)) return 'A team is already at work on it.';
  // (Rider b542: the four weapons only the great powers develop come to the others with a great power's plans.)
  const held = whyNoTechPlans(g, nat, id);
  if (held) return held;
  const lacks = t.needs.filter((x) => !s.done.includes(x));
  if (lacks.length) return `Needs ${and(lacks.map((x) => BY_ID[x]?.name ?? x))} first.`;
  if (!forSpyTeam(g, nat, s, t) && teamWork(s).length >= teamsOf(g, nat)) return 'Every research team is busy. Wait for one to finish, or stop a project.';
  return null;
}

/** Puts a free research team to work on a technology. */
export function startResearch(g: Game, nat: string, id: string): boolean {
  if (whyNotResearch(g, nat, id) !== null) return false;
  const of = monthsFor(BY_ID[id], g.turn), s = techOf(g, nat)!;
  s.work.push({ id, left: of, of, ...(forSpyTeam(g, nat, s, BY_ID[id]) ? { spy: true } : {}) });
  return true;
}

/** Takes a team off a technology. The work done on it is lost. */
export function stopResearch(g: Game, nat: string, id: string): boolean {
  const s = techOf(g, nat);
  const i = s ? s.work.findIndex((w) => w.id === id) : -1;
  if (!s || i < 0) return false;
  s.work.splice(i, 1);
  return true;
}

function finish(g: Game, n: Nation, id: string, quiet: boolean): void {
  const s = n.tech!, t = BY_ID[id];
  if (!t || s.done.includes(id)) return;
  s.done.push(id);
  // (The shipping may wait for the record's month for this nation: ruling 234, rider b630, lift.json.)
  if (t.sealift) techLift(g, n.id, id, t.sealift);
  // (With the laboratories in his advisers' hands the free team is theirs to set to work: desks.ts.)
  if (!quiet && n.id === g.human) headline(g, 'info', `Research complete: ${t.name}. ${techRule(t, g, n.id)}${staffRuns(g, n.id, 'research') ? '' : ' A research team is free for new work.'}`, n.id);
}

/**
 * The player's advisers look a winter ahead (Version 67). True when his army is not made for the eastern winter and
 * may have to fight in it: he is at war with a country of the deep east, or his ground touches the ground of one
 * that is no ally of his. A computer-run nation keeps history's order, and its mistake.
 */
export function winterAhead(g: Game, nat: string): boolean {
  if (g.flags.open !== 1 || nat !== g.human || EASTERN.winter.hardy.includes(nat) || EASTERN.deepOwners.includes(nat)) return false;
  for (const e of EASTERN.deepOwners) {
    if (!g.nations[e]?.alive || allied(g, nat, e)) continue;
    if (atWar(g, nat, e)) return true;
    for (const r of g.regions) if (r.ctrl === nat && r.nb.some((j) => g.regions[j].ctrl === e)) return true;
  }
  return false;
}

const SUPPORT_TECH = (supportData as unknown as { tech: { antiTank: string } }).tech;
/** True when a country is at war with one that has a tank corps in the field. */
export function facesTanks(g: Game, nat: string): boolean {
  const foes = g.nations[nat]?.atWar ?? [];
  return foes.length > 0 && g.corps.some((c) => !c.dead && c.kind === 'arm' && foes.includes(c.nat));
}

/** What a computer-run nation's staff would research next, or null when nothing is open to it. */
export function staffPick(g: Game, nat: string, turn = g.turn, spy = false): string | null {
  const s = techOf(g, nat);
  if (!s) return null;
  // With the air campaigns on, a staff bombed by night takes Fighter control first (b569(2)).
  const first = controlFirst(g, nat);
  if (first && !s.done.includes(first) && !s.work.some((w) => w.id === first) && BY_ID[first].needs.every((x) => s.done.includes(x))) return first;
  const A = TECH.ai, order = A.order[nat] ?? A.order._default, year = yearOf(turn);
  const skip = (id: string): boolean => A.skip._all.includes(id) || !!A.skip[nat]?.includes(id);
  let best: Tech | null = null, low = Infinity;
  const thirsty = oilMonths(g, nat) < RES.ai.synthAt;
  for (const t of TECH.techs) {
    if (s.done.includes(t.id) || s.work.some((w) => w.id === t.id) || t.needs.some((x) => !s.done.includes(x))) continue;
    // (Rider b542: the spy team works the Intelligence branch alone, in its order; the other teams never do, and
    // nobody's staff begins what only a great power's plans can give it.)
    if ((t.branch === 'intel') !== spy || whyNoTechPlans(g, nat, t.id)) continue;
    const place = order.indexOf(t.branch);
    // A staff watching its oil run down puts the fuel chemists first.
    const fuel = t.oil && thirsty ? A.fuel ?? 0 : 0;
    // A small country buys its oil abroad and has no use for fuel chemistry.
    const idle = t.oil && !keepsStores(g, nat) ? A.later : 0;
    // The player's advisers take up winter equipment as soon as a winter in the east is in sight.
    const cold = t.winterKit && winterAhead(g, nat) ? A.winter ?? 0 : 0;
    // A staff at war with an army that has tanks wants the guns that stop them (ruling 152(g)).
    const guns = t.id === SUPPORT_TECH.antiTank && facesTanks(g, nat) ? A.guns ?? 0 : 0;
    const score = t.tier * A.tier + (place < 0 ? order.length : place) * A.branch + Math.max(0, t.year - year) * A.early - (t.slots ? A.teams : 0) + (skip(t.id) ? A.later : 0) - fuel + idle - cold - guns;
    if (score < low) { low = score; best = t; }
  }
  return best ? best.id : null;
}

/** One month in the laboratories: every team's work moves on, what is finished is in service, and a staff with a free team picks its next project. */
function month(g: Game, n: Nation, turn: number, staffed: boolean, quiet: boolean): void {
  const s = n.tech!;
  kinTechs(g, n);
  const due: string[] = [];
  // Schools and institutes at home quicken every team.
  const pace = researchSpeed(g, n.id) * govMul(g, n.id, 'research');
  for (const w of s.work) { w.left -= pace; if (w.left <= 1e-9) due.push(w.id); }
  if (due.length) {
    s.work = s.work.filter((w) => w.left > 1e-9);
    for (const id of due) finish(g, n, id, quiet);
  }
  if (!staffed) return;
  fill(g, n, turn);
}

/** A staff sets its free teams to work, the spy team on the Intelligence branch (rider b542). Returns the projects begun. */
function fill(g: Game, n: Nation, turn: number): string[] {
  const s = n.tech!;
  const begun: string[] = [];
  for (const spy of [false, true]) {
    const room = (): boolean => (spy ? spyWork(s).length < spyTeamsOf(g, n.id) : teamWork(s).length < teamsOf(g, n.id));
    while (room()) {
      const id = staffPick(g, n.id, turn, spy);
      if (!id) break;
      const of = monthsFor(BY_ID[id], turn);
      s.work.push({ id, left: of, of, ...(spy ? { spy: true } : {}) });
      begun.push(id);
    }
  }
  return begun;
}

/**
 * When the grand campaign opens: every nation gets the technologies it had when the war began. A campaign that
 * opens in a later year, or a save from before the tree existed, gives each of them the months of research in
 * between, chosen as its own staff would have.
 */
export function openTech(g: Game): void {
  if (!techRules(g) || g.flags['tech:open']) return;
  g.flags['tech:open'] = 1;
  const all = Object.values(g.nations);
  for (const n of all) {
    n.tech ??= fresh(n.id);
    // The old way of steering research is closed in the grand campaign: every branch keeps to history's clock.
    if (n.lab) for (const f of Object.keys(n.lab.teams) as Field[]) n.lab.teams[f] = 1;
  }
  for (let turn = 1; turn < g.turn; turn++) for (const n of all) if (n.alive || n.dormant) month(g, n, turn, true, true);
  // The staffs set their teams to work at once; the player's wait for his word.
  for (const n of all) if (n.alive && n.id !== g.human) month0(g, n);
}

/** Fills a staff's free teams without a month passing. Returns the projects begun. */
function month0(g: Game, n: Nation): string[] {
  return fill(g, n, g.turn);
}

/** The player's advisers set his free research teams to work, as a computer nation's staff would (depth.ts). Returns the projects begun. */
export function staffTeams(g: Game, nat: string): string[] {
  const s = techOf(g, nat), n = g.nations[nat];
  return s && n?.alive ? month0(g, n) : [];
}

/** A nation's month of research, at the start of its turn. */
export function techTick(g: Game, nat: string): void {
  const s = techOf(g, nat);
  const n = g.nations[nat];
  if (!s || !n.alive) return;
  month(g, n, g.turn, nat !== g.human, false);
}

/** Once a month: a nation that has not yet come onto the stage (the United States before its entry) goes on with its research at home. */
export function techMonth(g: Game): void {
  if (!techRules(g)) return;
  for (const n of Object.values(g.nations)) if (!n.alive && n.dormant && techOf(g, n.id)) month(g, n, g.turn, true, true);
}

// ---- Airborne troops --------------------------------------------------------------------------------

/** How many airborne corps a nation has in the field and on order. */
export function paraCount(g: Game, nat: string): number {
  let k = g.nations[nat].queue.filter((b) => b.airborne).length;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.airborne) k++;
  return k;
}

/** Why this nation cannot order an airborne corps now, apart from money and a place to raise it; null when it can. */
export function whyNoPara(g: Game, nat: string): string | null {
  if (!techRules(g)) return 'Airborne corps are raised in the grand campaign.';
  const need = locked(g, nat, 'para');
  if (need) return `Needs the technology ${need.name}.`;
  const most = TECH.units.para.most;
  if (paraCount(g, nat) >= most) return `The transport aircraft can serve ${most} airborne corps at once, in the field and on order.`;
  return null;
}

// ---- In words ---------------------------------------------------------------------------------------

const pct = (x: number): string => `${Math.round(x * 100)}%`;
const FIELD_RULE: Record<Field, (p: string) => string> = {
  armour: (p) => `Armoured and motorised corps fight ${p} harder.`,
  infantry: (p) => `Every corps that is not armoured or motorised fights ${p} harder.`,
  doctrine: (p) => `Every corps attacks ${p} harder.`,
  fighters: (p) => `Fighters fight ${p} harder.`,
  bombers: (p) => `Bombers that get through do ${p} more damage.`,
  navy: (p) => `Fleets, raiders and convoy escorts fight ${p} harder.`,
  radar: (p) => `Fighters over their own sky and the escorts on the convoy routes are worth ${p} more.`,
  industry: (p) => `Earnings up ${p}.`,
};
const UNLOCK_RULE: Record<string, string> = {
  'corps:arm': 'Armoured corps can be raised.',
  'corps:mot': 'Motorised corps can be raised.',
  'ship:cv': 'Aircraft carriers can be laid down.',
  para: 'Airborne corps can be raised.',
};

/**
 * What the infantry's four support technologies do (rulings 146 and 152; the rule is in support.ts and the numbers in
 * support.json): each weapon has a job of its own in place of 6% on everything.
 */
function supportWords(id: string): string | null {
  const S = supportData as unknown as {
    antiTank: { without: number; tankDestroyers: number; alone: { cut: number }; handHeld: { open: number; close: number } };
    machineGuns: { defence: number }; mortars: { works: number }; assault: { attack: number };
    tech: { antiTank: string; tankDestroyers: string; handHeld: string; support: string };
  };
  const A = S.antiTank, T = S.tech;
  if (id === T.antiTank) return `Its infantry keeps up with the anti-tank guns of the day: without this they count at ${pct(A.without)} of their worth. They take the shock out of a tank attack, most from behind finished fieldworks, and the tanks pay for what they lose. Tanks that come on with no infantry beside them lose up to ${pct(A.alone.cut)} of their attack to the guns.`;
  if (id === T.support) return `Machine guns: its infantry defends up to ${pct(S.machineGuns.defence)} stronger against men on foot, most from finished fieldworks, and more of the attacker's losses fall on his infantry. Mortars: its infantry in the attack takes up to ${pct(S.mortars.works)} of the worth out of fieldworks, never out of concrete.`;
  if (id === T.tankDestroyers) return `Every line its infantry holds takes ${pct(A.tankDestroyers)} more of the shock out of a tank attack.`;
  if (id === T.handHeld) return `Its men on foot attack ${pct(S.assault.attack)} stronger. Hand-held anti-tank weapons: a line its infantry holds takes ${pct(A.handHeld.open)} more of the shock out of a tank attack, ${pct(A.handHeld.close)} in forest and hills.`;
  return null;
}

/** What a technology does, in plain words worked out from its numbers, so the words and the rule cannot drift apart. */
export function techRule(t: Tech, g?: Game, nat?: string): string {
  const out: string[] = [];
  for (const k of t.unlock ?? []) out.push(UNLOCK_RULE[k] ?? `Unlocks ${k}.`);
  if (t.field) out.push(FIELD_RULE[t.field](pct(RESEARCH.fields[t.field].per)));
  // (The two radar steps, with the air campaigns on: what the warning chain and fighter control do, rider b553.)
  const radar = g && nat ? radarWords(g, nat, t.id) : null;
  if (radar) out.push(radar);
  const kit = supportWords(t.id);
  if (kit) out.push(kit);
  const rest = traitRule({ id: t.id, name: t.name, text: t.text, attack: t.attack, defence: t.defence, when: t.when, drafts: t.drafts, cost: t.cost, sealift: t.sealift, fleet: t.fleet, seaLoss: t.seaLoss });
  if (rest) out.push(rest);
  if (t.slots) out.push(t.slots === 1 ? 'One more research team.' : `${t.slots} more research teams.`);
  if (t.winterKit) out.push('Its troops no longer march and fight worse in the eastern winter.');
  if (t.oil) out.push(`Every working factory at home also makes ${t.oil} oil a month, where no raider can reach it. A country that keeps no stocks of oil has no use for it.`);
  // The Intelligence branch (rider b542): what it does for the nation's agents (intel.ts).
  const x = t.spy;
  if (x?.caught !== undefined) out.push(`Its agents abroad are caught ${pct(1 - x.caught)} less often.`);
  if (x?.rest) out.push(`After an attempt its agents lie low ${x.rest === 1 ? 'a month' : `${x.rest} months`} less, never under a month.`);
  if (x?.theft) out.push(`Its agents' thefts of a great power's plans come off ${Math.round(x.theft * 100)} more times in every 100.`);
  if (x?.agents) out.push(`Its secret service can run ${x.agents === 1 ? 'one more agent' : `${x.agents} more agents`} at once.`);
  if (x?.guard) out.push(`Its agents on guard at home stop ${Math.round(x.guard * 100)} more of every 100 enemy plots.`);
  return out.join(' ');
}

// ---- The tree, for an interface to draw -------------------------------------------------------------

export interface TechNode {
  id: string;
  branch: Branch;
  tier: number;
  name: string;
  text: string;
  /** What it does, in words. */
  rule: string;
  year: number;
  needs: { id: string; name: string; done: boolean }[];
  /** Researched, a team at work on it, open to be started, or waiting on the technologies before it. */
  state: 'done' | 'work' | 'ready' | 'locked';
  /** Months a team would need if it began now, and what that would be in the technology's own year. */
  months: number;
  base: number;
  /** For one in work: months still to go, and the months it was begun with. */
  left: number;
  of: number;
  /** Why it cannot be started now, or null when a free team can begin. */
  why: string | null;
}

export interface TechView {
  nat: string;
  teams: number;
  busy: number;
  /** Of the teams, those of the secret service's laboratory, which work the Intelligence branch alone (rider b542). */
  spyTeams: number;
  /** Teams with nothing to do: the ordinary ones, and the secret service's. */
  free: number;
  spyFree: number;
  done: number;
  total: number;
  branches: { id: Branch; label: string; text: string; done: number; total: number }[];
  /** The name of each step of the tree, 1 to 4. */
  eras: string[];
  nodes: TechNode[];
}

/** The whole tree as one nation sees it. Null outside the grand campaign. */
export function techView(g: Game, nat: string): TechView | null {
  const s = techOf(g, nat);
  if (!s) return null;
  const nodes: TechNode[] = TECH.techs.map((t) => {
    const w = s.work.find((x) => x.id === t.id);
    const done = s.done.includes(t.id);
    const open = t.needs.every((x) => s.done.includes(x));
    return {
      id: t.id, branch: t.branch, tier: t.tier, name: t.name, text: t.text, rule: techRule(t, g, nat), year: t.year,
      needs: t.needs.map((x) => ({ id: x, name: BY_ID[x]?.name ?? x, done: s.done.includes(x) })),
      state: done ? 'done' : w ? 'work' : open ? 'ready' : 'locked',
      months: monthsFor(t, g.turn), base: t.months ?? TECH.months[t.tier],
      left: w ? Math.max(1, Math.ceil(w.left - 1e-9)) : 0, of: w ? w.of : 0,
      why: whyNotResearch(g, nat, t.id),
    };
  });
  return {
    nat, teams: teamsOf(g, nat) + spyTeamsOf(g, nat), busy: s.work.length, spyTeams: spyTeamsOf(g, nat),
    free: Math.max(0, teamsOf(g, nat) - teamWork(s).length), spyFree: Math.max(0, spyTeamsOf(g, nat) - spyWork(s).length), done: nodes.filter((x) => x.state === 'done').length, total: nodes.length,
    branches: TECH.branches.map((b) => ({ ...b, done: nodes.filter((x) => x.branch === b.id && x.state === 'done').length, total: nodes.filter((x) => x.branch === b.id).length })),
    eras: TECH.eras, nodes,
  };
}

/** True when the player has a research team with nothing to do and something it could begin. */
export function idleTeams(g: Game, nat: string): number {
  const s = techOf(g, nat);
  if (!s || !g.nations[nat].alive) return 0;
  const free = Math.max(0, teamsOf(g, nat) - teamWork(s).length), spare = Math.max(0, spyTeamsOf(g, nat) - spyWork(s).length);
  return (free > 0 && staffPick(g, nat) ? free : 0) + (spare > 0 && staffPick(g, nat, g.turn, true) ? spare : 0);
}

/** For a battle ticket: what the tree has added to these branches, nation by nation. */
export function treeNotes(g: Game, nats: Iterable<string>, fields: Field[]): { text: string; pct: number }[] {
  const out: { text: string; pct: number }[] = [];
  if (g.flags.open !== 1) return out;
  for (const nat of new Set(nats)) {
    for (const f of fields) {
      const k = treeLevel(g, nat, f);
      if (!k) continue;
      out.push({ text: `${g.nations[nat].adj} ${RESEARCH.fields[f].noun}: ${k === 1 ? 'one technology' : `${k} technologies`} researched`, pct: Math.round(RESEARCH.fields[f].per * k * 100) });
    }
  }
  return out;
}
