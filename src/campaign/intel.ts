// Spies and the fog of war (the grand campaign).
// The player no longer sees every army on the map. An enemy or neutral force is in view only where the
// player's own side can see it: on the ground it holds, beside its troops, and a province beyond. A province
// further off it is a flag and a count; deeper still, nothing. And being in view is not being known: what an army
// is made of is known only once its corps are identified, in battle, by an agent or from the air, and a report
// goes stale with the months (see 'reports'). Agents are how the player sees further. Each is
// a piece: recruited for money, sent to a province, and at work there a month later. An agent watches, reads a
// government's books in its capital, or tries an act of sabotage, and every month abroad it may be caught.
// The computer's staffs see the whole map, as they always did, and keep no agents on it. Their services work
// against the player as a monthly chance of sabotage, which the player's agents on guard at home can stop.
// So a war with nobody at the table is fought exactly as before. Every number lives in data/campaign/intel.json.

import data from '../../data/campaign/intel.json';
import { Rng, hashString } from '../core/rng';
import type { Agent, Corps, FoeAgent, Game, IntelNote, Mission, Region, StolenOrder } from './types';
import { allied, atWar, bloc, declareWar, fieldArmy, headline, theWords } from './game';
import { ARSENAL, yardLevel } from './arsenal';
import { PLANS, PLAN_PLACES, PLAN_WORDS, canNowWords, doesWith, givesPlans, grantPlans, isCapitalHull, planRules, plansFrom, whyNoTechPlans, type PlanItem } from './plans';
import { CAUSE_WORDS, cutCause, cutOn, cutRail, cutThroughGuns, hastyLeft, onNet, railCut, ruinMost, ruinedAt, workingFactories, wreckFactory, type RailCause } from './rails';
import { turnToDate } from '../core/ticket';
import { levelIndex } from './difficulty';
import { fieldOut, fireField, stores, type StoresView } from './resources';
import { BUDGET, ledger } from './economy';
import { TECH, spyOf } from './tech';
import { AIRV, airPiece, fieldWrecked, groupsOnField, wreckField } from './airversion';
import { huntersOn, ordersKnown, ordersOn, pruneOrders } from './secretwar';
import { fieldOrder } from './air';

interface MissionRow { caught: number; success?: number; gain?: number; months?: number; cut?: number }

export const INTEL = data as unknown as {
  sight: { full: number; glimpse: number; fleets?: boolean };
  agents: { cost: number; most: Record<string, number>; train: number; travel: number; skillAt: number[]; watch: number; ranks: string[] };
  missions: Record<Mission, MissionRow>;
  skill: { caught: number; success: number; guard: number };
  risk: { capital: number; war: number; heat: number; heatMonths: number; lieLow: number };
  caught: { opinion: number; war?: number };
  battle: { attack: number };
  enemy: { chance: number; quiet: number; level: number[]; services: string[]; kinds: { factory: number; rail: number }; near: number; far: number; most: number };
  estimate: { round: number };
  reports: { fresh: number; keep: number; drift: number; widest: number; contact: number; unknown: number };
  names: string[];
  secret: { hunters: { on: boolean; lead: number; here: number; near: number; skill: number }; orders: { on: boolean; ambush: number } };
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
  // Who is on the viewer's side, asked once a nation.
  const side = new Map<string, boolean>();
  const ours = (n: string): boolean => { let v = side.get(n); if (v === undefined) side.set(n, (v = allied(g, n, me))); return v; };
  for (const r of g.regions) {
    if (ours(r.ctrl) || r.units.some((u) => !u.dead && ours(u.nat))) { dist[r.id] = 0; ring.push(r.id); }
  }
  for (const a of agentsOf(g, me)) if (atWork(g, a) && dist[a.reg] !== 0) { dist[a.reg] = 0; ring.push(a.reg); }
  // Ground the player's aircraft have photographed this month is seen as an agent on watch would see it.
  if (me === g.human) for (const [id, turn] of Object.entries(g.intel.scout ?? {})) if (turn === g.turn && dist[+id] !== 0) { dist[+id] = 0; ring.push(+id); }
  for (let d = 1; d <= far && ring.length; d++) {
    const next: number[] = [];
    for (const id of ring) for (const j of g.regions[id].nb) if (dist[j] < 0) { dist[j] = d; next.push(j); }
    ring = next;
  }
  for (let i = 0; i < out.length; i++) out[i] = dist[i] < 0 ? SIGHT.none : dist[i] <= S.full ? SIGHT.full : SIGHT.glimpse;
  // A fleet at sea sees the coasts of the sea it is in: the flags on them, and how many corps stand there.
  if (S.fleets) {
    for (const f of g.fleets) {
      if (f.stance === 'port' || !ours(f.nat) || !g.seas[f.zone] || !Object.values(f.ships).some((n) => n > 0)) continue;
      for (const id of g.seas[f.zone].coast) if (out[id] === SIGHT.none) out[id] = SIGHT.glimpse;
    }
  }
  return out;
}

// ---- What is known of a cut railway ---------------------------------------------------------------------
// The fog lies on a cut line as on everything else (the design director's rulings 113 and 114). On his own ground and
// his allies', and wherever he has sight of a province, the player sees the line as it is. Elsewhere the map shows
// what he last knew, with its month: a cut he saw before he lost sight of the place, or one his own raid, agent or
// troops made and reported. A line mended out of his sight goes on showing as cut until he sees it again. One function
// decides it, so the browser's map and the 3D table never differ. The computer's staffs see the whole map.

/** What a viewer knows of one cut railway. */
export interface CutKnown {
  /** 'now': he sees it as it is. 'seen': last seen cut, in the month `asOf`. 'report': cut by his own doing in `asOf`, and not seen since. */
  how: 'now' | 'seen' | 'report';
  /** The month his knowledge is of: this month where he sees the line. */
  asOf: number;
  /** The month the line was cut, where he sees it now. */
  since?: number;
  /** What cut it, where that was kept and he knows it. */
  cause: RailCause | null;
  /** Months until the railwaymen make a hasty cut good, where he sees it now; 0 otherwise. */
  hasty: number;
}

/**
 * Every cut railway the viewer knows of, by province. Nobody at the table, the fog lifted, or a computer's staff asking: every cut there is.
 * 'seen' is the viewer's sightOf where the caller has it already (the 3D table works it out once a picture); the same answer either way.
 */
export function cutsKnown(g: Game, viewer: string | null = g.human, seen?: Uint8Array): Map<number, CutKnown> {
  const out = new Map<number, CutKnown>();
  if (!intelRules(g)) return out;
  const fog = viewer !== null && viewer === g.human && fogOn(g, viewer);
  const sight = fog ? seen ?? sightOf(g, viewer) : null;
  const kept = fog ? g.intel.cuts ?? {} : {};
  for (const r of g.regions) {
    if (!sight || sight[r.id] !== SIGHT.none) {
      if (railCut(g, r.id)) out.set(r.id, { how: 'now', asOf: g.turn, since: cutOn(g, r.id), cause: cutCause(g, r.id), hasty: hastyLeft(g, r.id) });
    } else {
      const k = kept[r.id];
      if (k) out.set(r.id, { how: k.how, asOf: k.t, cause: k.why ?? null, hasty: 0 });
    }
  }
  return out;
}

/** What the viewer knows of the railway of one province being cut, or null where he knows of no cut. Decided as cutsKnown decides it, for the one province. */
export function cutKnown(g: Game, region: number, viewer: string | null = g.human, seen?: Uint8Array): CutKnown | null {
  const r = g.regions[region];
  if (!intelRules(g) || !r || r.id !== region) return cutsKnown(g, viewer, seen).get(region) ?? null;
  const fog = viewer !== null && viewer === g.human && fogOn(g, viewer);
  const k = fog ? g.intel.cuts?.[region] : undefined;
  // A line not cut, and with no cut kept in his record, is known to nobody as cut, seen or not.
  if (!railCut(g, region) && !k) return null;
  if (!fog || (seen ?? sightOf(g, viewer))[region] !== SIGHT.none) return railCut(g, region) ? { how: 'now', asOf: g.turn, since: cutOn(g, region), cause: cutCause(g, region), hasty: hastyLeft(g, region) } : null;
  return k ? { how: k.how, asOf: k.t, cause: k.why ?? null, hasty: 0 } : null;
}

/**
 * The player's record of cut lines is brought up to date with what he can see, as his month opens and as it closes:
 * a cut in his sight is noted as seen this month, and a line he sees whole is struck off. What he cannot see is left
 * as he last knew it. It is a memory for the map, and no rule reads it.
 */
export function seeCuts(g: Game): void {
  const me = g.human;
  if (!intelRules(g) || me === null || !g.nations[me]?.alive) return;
  // Kept with the fog lifted too, so that laying it again does not leave him knowing nothing.
  const sight = g.flags.nofog === 1 ? null : sightOf(g, me);
  const kept = (g.intel.cuts ??= {});
  for (const r of g.regions) {
    if (sight && sight[r.id] === SIGHT.none) continue;
    const why = cutCause(g, r.id);
    if (railCut(g, r.id)) kept[r.id] = why ? { t: g.turn, how: 'seen', why } : { t: g.turn, how: 'seen' };
    else delete kept[r.id];
  }
  if (!Object.keys(kept).length) delete g.intel.cuts;
}

const monthOf = (t: number): string => turnToDate(t).label;
const capital = (s: string): string => s[0].toUpperCase() + s.slice(1);

/** A cut railway in words, for the province panel, the pointer and the 3D table alike. */
export interface CutSaid {
  /** "Railway cut", "Railway cut when last seen" or "Railway reported cut". */
  head: string;
  /** Whole sentences: what cut it and when, and what it stops. */
  line: string;
  /** A few words for the line under the pointer: "railway cut (bombed, June 1941)". */
  short: string;
}

/** What the viewer is told of the railway of a province being cut (ruling 112(c)), or null where he knows of no cut. 'seen' as for cutsKnown. */
export function cutSays(g: Game, region: number, viewer: string | null = g.human, seen?: Uint8Array): CutSaid | null {
  const k = cutKnown(g, region, viewer, seen);
  if (!k) return null;
  const name = g.regions[region].name, cause = k.cause ? CAUSE_WORDS[k.cause] : '';
  if (k.how === 'now') {
    const when = monthOf(k.since ?? g.turn);
    const stops = `No train runs into, out of or through ${name} until ${k.hasty > 0 ? `the railwaymen have it running, in ${monthOf(g.turn + k.hasty)}, or engineers relay the track sooner` : 'engineers relay the track'}. Only engineers can be carried up to the break.`;
    // A line cut by bombing says whether guns stood there that month (ruling 125): other causes say nothing of guns.
    const guns = cutThroughGuns(g, region);
    const through = guns === null ? '' : guns > 0 ? `, through level ${guns} guns` : '. No guns guarded the railway here';
    return { head: 'Railway cut', line: `${cause ? `${capital(cause)} in ${when}` : `Cut since ${when}`}${through}. ${stops}`, short: `railway cut (${cause ? `${cause}, ` : ''}${when})` };
  }
  const when = monthOf(k.asOf);
  if (k.how === 'seen') return { head: 'Railway cut when last seen', line: `Last seen cut in ${when}${cause ? ` (${cause})` : ''}. What has been done to the line since is not known.`, short: `railway cut when last seen (${when})` };
  const who = k.cause === 'bombed' ? 'Your bombers cut it' : k.cause === 'agent' ? 'Your agent blew it' : k.cause ? 'Your troops tore it up' : 'Reported cut';
  return { head: 'Railway reported cut', line: `${who} in ${when}. The line has not been seen since.`, short: `railway reported cut (${when})` };
}

// ---- What is known of a foreign army's make-up ----------------------------------------------------------
// Seeing a flag is not knowing what stands under it. The player's side identifies a foreign corps by fighting it,
// by an agent at work who has it in view, or by photographing its province from the air. A report is exact in the
// month it is made, then goes stale, and after a few months is worth nothing. Only the player keeps reports: the
// computer's staffs see the whole map, as they always did.

/** How well a corps is known: everything, a dated report gone stale, or nothing but that it is there. */
export type Known = 'exact' | 'report' | 'none';

/** How a corps came to be identified: fought in battle, watched by an agent, or photographed from the air. */
export type Source = 'battle' | 'agent' | 'air';

/** Marks corps as identified this month by a nation: nothing happens unless that nation is the player or the player's ally. */
export function identify(g: Game, corps: Corps[], by: string | null, how: Source = 'battle'): void {
  const me = g.human;
  if (!me || !by || !intelRules(g) || !(by === me || allied(g, by, me))) return;
  for (const c of corps) {
    if (c.dead || allied(g, c.nat, me)) continue;
    (g.intel.known ??= {})[c.id] = g.turn;
    (g.intel.how ??= {})[c.id] = how;
  }
}

/** The provinces round a set of provinces that an agent's eyes or a camera cover: each one's own and those around it. */
function cover(g: Game, from: number[]): Set<number> {
  const out = new Set<number>(from);
  let ring = from;
  for (let d = 1; d <= INTEL.sight.full && ring.length; d++) {
    const next: number[] = [];
    for (const id of ring) for (const j of g.regions[id].nb) if (!out.has(j)) { out.add(j); next.push(j); }
    ring = next;
  }
  return out;
}

/** How the viewer last identified a corps, or null where it has no report of it (its own and its allies' need none). */
export function reportSource(g: Game, c: Corps, viewer: string | null = g.human): Source | null {
  if (viewer !== g.human || !fogOn(g, viewer) || allied(g, c.nat, viewer as string) || corpsKnown(g, c, viewer).level === 'none') return null;
  return g.intel.how?.[c.id] ?? 'battle';
}

/**
 * Brings the player's reports up to date: every foreign corps an agent at work has in view, or that aircraft have
 * photographed this month, is identified now. Reports too old to be worth anything, and those of corps that are no
 * more, are thrown away. Called when the player's month begins and after a reconnaissance flight.
 */
export function reconnoitre(g: Game): void {
  const me = g.human;
  if (!me || !intelRules(g) || !g.nations[me]?.alive) return;
  const photographed = Object.entries(g.intel.scout ?? {}).filter(([, turn]) => turn === g.turn).map(([id]) => +id);
  for (const id of cover(g, photographed)) identify(g, g.regions[id].units, me, 'air');
  for (const id of cover(g, agentsOf(g, me).filter((a) => atWork(g, a)).map((a) => a.reg))) identify(g, g.regions[id].units, me, 'agent');
  const known = g.intel.known;
  if (!known) return;
  const alive = new Set<number>();
  for (const c of g.corps) if (!c.dead) alive.add(c.id);
  for (const k of Object.keys(known)) if (!alive.has(+k) || g.turn - known[+k] > INTEL.reports.keep) { delete known[+k]; if (g.intel.how) delete g.intel.how[+k]; }
}

/**
 * How well the viewer knows a corps, and how old the report is in months. The viewer's own corps and its allies' are
 * always known, and so is everything with the fog lifted or outside the grand campaign.
 */
export function corpsKnown(g: Game, c: Corps, viewer: string | null = g.human): { level: Known; age: number } {
  if (!fogOn(g, viewer) || allied(g, c.nat, viewer as string) || viewer !== g.human) return { level: 'exact', age: 0 };
  const seen = g.intel.known?.[c.id];
  if (seen === undefined) return { level: 'none', age: -1 };
  const age = Math.max(0, g.turn - seen);
  return age <= INTEL.reports.fresh ? { level: 'exact', age } : age <= INTEL.reports.keep ? { level: 'report', age } : { level: 'none', age: -1 };
}

/** A stale report's reading of a corps' strength: a band that holds the truth and is wider for every month of age. */
export function strengthBand(g: Game, c: Corps, age: number): { lo: number; hi: number } {
  const w = Math.min(INTEL.reports.widest, INTEL.reports.drift * Math.max(0, age));
  const v = draw(g, 'str', c.id, g.turn);
  return { lo: clamp(c.str * (1 - w * v), 0, 1), hi: clamp(c.str * (1 + w * (1 - v)), 0, 1) };
}

/**
 * The staff's estimate of corps it has counted but not identified, in divisions at full strength: a band that holds
 * the truth. It is the same all month, and a new guess the month after.
 */
export function divisionsBand(g: Game, region: number, nat: string, corps: Corps[]): { lo: number; hi: number } {
  const total = corps.reduce((s, c) => s + c.divs * c.str, 0);
  if (!corps.length) return { lo: 0, hi: 0 };
  const w = INTEL.reports.contact, v = draw(g, 'divs', region, nat, g.turn);
  return { lo: total * (1 - w * v), hi: total * (1 + w * (1 - v)) };
}

/** What a viewer makes of a set of corps under one flag: which are known exactly, which from a stale report, which not at all. */
export function sortKnown(g: Game, corps: Corps[], viewer: string | null = g.human): { exact: Corps[]; report: Corps[]; none: Corps[] } {
  const out = { exact: [] as Corps[], report: [] as Corps[], none: [] as Corps[] };
  for (const c of corps) if (!c.dead) out[corpsKnown(g, c, viewer).level].push(c);
  return out;
}

/**
 * How unsure the viewer is of the strength of a force it means to attack, as a share of that strength: nothing where
 * every corps is identified this month, more for each one known from a stale report, most for those not identified.
 * And the band of odds that follows: it holds the true odds, and is not centred on them.
 */
export function oddsBand(g: Game, defenders: Corps[], odds: number, target: number, viewer: string | null = g.human): { exact: boolean; doubt: number; lo: number; hi: number; mid: number } {
  let doubt = 0, all = 0;
  const R = INTEL.reports;
  for (const c of defenders) {
    const k = corpsKnown(g, c, viewer);
    const w = c.divs * Math.max(0.05, c.str);
    doubt += w * (k.level === 'exact' ? 0 : k.level === 'report' ? Math.min(R.widest, R.drift * k.age) : R.unknown);
    all += w;
  }
  doubt = all > 0 ? doubt / all : 0;
  if (doubt <= 0) return { exact: true, doubt: 0, lo: odds, hi: odds, mid: odds };
  // The defence may be anything from this much weaker to this much stronger than it truly is: the truth lies inside.
  const v = draw(g, 'odds', target, g.turn);
  const strong = 1 + doubt * (1 - v), weak = Math.max(0.2, 1 - doubt * v);
  return { exact: false, doubt, lo: odds / strong, hi: odds / weak, mid: odds / ((strong + weak) / 2) };
}

/**
 * One nation's force in a province as the viewer knows it. 'known' is 'full' where every corps is identified, 'partial'
 * where the army is in view but some of it is not identified or is known only from a stale report, and 'count' where
 * only the flag and the number of corps are known. 'divs' is the strength in divisions where all is known, else null;
 * 'est' is the staff's estimate of it as a band where the army stands in view of the viewer's lines, else null.
 */
export interface ArmySeen { reg: number; nat: string; sight: 1 | 2; corps: number; divs: number | null; mine: boolean; known: 'full' | 'partial' | 'count'; identified: number; est: { lo: number; hi: number } | null }

/** The viewer's estimate of an army's strength in divisions, as a band: exact corps as they are, stale reports by their bands, the rest by the staff's count. Null where the unidentified corps are too far off to be estimated. */
export function armyBand(g: Game, region: number, nat: string, corps: Corps[], viewer: string | null, inContact: boolean): { lo: number; hi: number } | null {
  const k = sortKnown(g, corps, viewer);
  if (k.none.length && !inContact) return null;
  let lo = 0, hi = 0;
  for (const c of k.exact) { lo += c.divs * c.str; hi += c.divs * c.str; }
  for (const c of k.report) { const b = strengthBand(g, c, corpsKnown(g, c, viewer).age); lo += c.divs * b.lo; hi += c.divs * b.hi; }
  if (k.none.length) { const b = divisionsBand(g, region, nat, k.none); lo += b.lo; hi += b.hi; }
  return { lo, hi };
}

/** Every army the viewer knows of, for a map to draw. Armies in ground the viewer cannot see are not on the list. */
export function armiesSeen(g: Game, viewer: string | null = g.human): ArmySeen[] {
  const sight = sightOf(g, viewer);
  const out: ArmySeen[] = [];
  for (const r of g.regions) {
    if (!r.units.length) continue;
    const by = new Map<string, { corps: number; divs: number; list: Corps[] }>();
    for (const c of r.units) {
      if (c.dead) continue;
      // A corps that marches with an ally's army stands under that army's flag.
      const flag = c.joint ?? c.nat;
      const row = by.get(flag) ?? by.set(flag, { corps: 0, divs: 0, list: [] }).get(flag)!;
      row.corps++; row.divs += c.divs * c.str; row.list.push(c);
    }
    for (const [nat, row] of by) {
      const mine = viewer !== null && allied(g, nat, viewer);
      const s = mine ? SIGHT.full : sight[r.id];
      if (s === SIGHT.none) continue;
      const k = mine ? null : sortKnown(g, row.list, viewer);
      const all = !k || (!k.report.length && !k.none.length);
      const identified = k ? k.exact.length + k.report.length : row.corps;
      const known: ArmySeen['known'] = all ? 'full' : s === SIGHT.full || identified > 0 ? 'partial' : 'count';
      out.push({ reg: r.id, nat, sight: s as 1 | 2, corps: row.corps, divs: all ? row.divs : null, mine, known, identified, est: all ? { lo: row.divs, hi: row.divs } : armyBand(g, r.id, nat, row.list, viewer, s === SIGHT.full) });
    }
  }
  return out;
}

// ---- Agents ---------------------------------------------------------------------------------------------

/** An agent's rank, 1 to 3: earned from work that came off. */
export const skillOf = (a: Agent): number => 1 + INTEL.agents.skillAt.filter((x) => a.xp >= x).length;
export const rankOf = (a: Agent): string => INTEL.agents.ranks[skillOf(a) - 1] ?? INTEL.agents.ranks[0];
/** How many agents a nation's service can run at once: more with Courier networks (tech.json, the Intelligence branch). */
export const mostAgents = (nat: string, g?: Game): number => (INTEL.agents.most[nat] ?? INTEL.agents.most._default) + (g ? spyOf(g, nat).agents : 0);
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
  const most = mostAgents(nat, g);
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

export const MISSIONS: Mission[] = ['watch', 'books', 'factory', 'rail', 'oil', 'airfield', 'orders', 'bbPlans', 'cvPlans', 'bomberPlans', 'jetPlans', 'rocketPlans', 'guidedPlans', 'guard', 'hunt'];
const SABOTAGE: Mission[] = ['factory', 'rail', 'oil', 'airfield'];
/** The work an agent can have at home; everything else is work abroad. */
const AT_HOME: Mission[] = ['guard', 'hunt'];
/**
 * The missions offered in this campaign, each piece of the air version and the secret war only with its switch on:
 * wrecking an airfield (ruling 151(b)), stealing the orders (135), hunting enemy agents (151(c)).
 */
const missionsIn = (g: Game): Mission[] => MISSIONS.filter((m) => (m !== 'airfield' || airPiece(g, 'field')) && (m !== 'orders' || ordersOn(g)) && (m !== 'hunt' || huntersOn(g)));
export const isSabotage = (m: Mission): boolean => SABOTAGE.includes(m);
/**
 * A theft of a great power's plans of one of the weapons only the great powers make (Brian, 18 November; ruling 235
 * and its rider b542; plans.ts): tried once, like an act of sabotage.
 */
export const THEFT: Partial<Record<Mission, PlanItem>> = { bbPlans: 'bb', cvPlans: 'cv', bomberPlans: 'heavybombers', jetPlans: 'jets', rocketPlans: 'rockets', guidedPlans: 'guided' };
export const isTheft = (m: Mission): boolean => THEFT[m] !== undefined;
/** True for work that is tried once as the month turns, and then the agent lies low. */
export const triedOnce = (m: Mission): boolean => isSabotage(m) || isTheft(m);

/** A theft mission's words, from the weapon's (plans.ts). */
function theftText(x: PlanItem): { label: string; doing: string; text: string } {
  const W = PLAN_WORDS[x], war = Math.round(((data as any).caught.war ?? 0) * 10);
  return {
    label: `Steal the ${W.one} plans`, doing: `preparing to steal the ${W.one} plans`,
    text: `A great power's ${W.one} plans are copied and got out of ${PLAN_PLACES[W.place].got}, and ${W.who} can ${doesWith(x)} on its designs. Tried once, next month, and then the agent must lie low. If the agent is caught, a great power at peace with you, and not your ally, declares war ${war} times in 10.`,
  };
}

export const MISSION_TEXT: Record<Mission, { label: string; doing: string; text: string }> = {
  watch: { label: 'Watch and report', doing: 'watching', text: 'Reports every army in the province and the provinces around it: each corps, its kind and its strength. In the province itself the agent also reports what is on order there and which air fleets are based there, and your armies attack it harder for knowing its defences.' },
  books: { label: 'Read the books', doing: 'reading the government\'s books', text: 'In a capital: the government\'s treasury and income, the true size of its army, what it has on order, what its laboratories are working on and what is in its depots.' },
  factory: { label: 'Wreck a factory', doing: 'preparing to wreck a factory', text: 'One working factory is put out of action until engineers rebuild it. Tried once, next month, and then the agent must lie low.' },
  rail: { label: 'Cut the railway', doing: 'preparing to cut the railway', text: 'The track is blown. No train runs through the province until engineers relay it. Tried once, next month, and then the agent must lie low.' },
  oil: { label: 'Fire the oilfield', doing: 'preparing to fire the oilfield', text: `The wells are set alight: the field gives ${Math.round((1 - (data as any).missions.oil.cut) * 100)}% of its oil for ${(data as any).missions.oil.months} months. Tried once, next month, and then the agent must lie low.` },
  airfield: { label: 'Wreck the airfield', doing: 'preparing to wreck the airfield', text: 'Runways cratered and the fuel dump set alight: every enemy air group based on the field is grounded for the month. Tried once, next month, and then the agent must lie low.' },
  orders: { label: 'Steal the orders', doing: 'stealing the orders', text: "Next month's orders of the bomber groups on this field: which groups, how many bombers, the target, by day or by night. Tried each month the agent stays on it. A stolen order is never false; the gamble is in getting it." },
  hunt: { label: 'Hunt enemy agents', doing: 'hunting enemy agents', text: "Searches this province and the provinces around it for the other side's agents. One uncovered can be arrested before it strikes." },
  // (Read when asked for, not as the module loads: plans.ts and this file import each other through game.ts.)
  get bbPlans() { return theftText('bb'); },
  get cvPlans() { return theftText('cv'); },
  get bomberPlans() { return theftText('heavybombers'); },
  get jetPlans() { return theftText('jets'); },
  get rocketPlans() { return theftText('rockets'); },
  get guidedPlans() { return theftText('guided'); },
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
  p *= spyOf(g, a.nat).caught;
  return clamp(p, 0, 0.95);
}

/** The chance that an act of sabotage or a theft by this agent comes off: a theft surer with Microfilm (tech.json). */
export const successChance = (a: Agent, m: Mission, g?: Game): number =>
  clamp((INTEL.missions[m].success ?? 0) + INTEL.skill.success * (skillOf(a) - 1) + (g && isTheft(m) ? spyOf(g, a.nat).theft : 0), 0, 0.95);

/** Why an agent cannot be given this work where it stands, or null if it can. */
export function whyNotMission(g: Game, nat: string, id: number, m: Mission): string | null {
  const a = find(g, nat, id);
  if (!a) return 'There is no such agent.';
  const r = g.regions[a.reg], host = g.nations[r.ctrl];
  if (!atWork(g, a)) return `${a.name} is not yet at work: ${waitWords(g, a)}.`;
  if (m === 'guard') return abroad(g, a) ? 'An agent can stand guard only on friendly ground.' : null;
  if (m === 'hunt') return !huntersOn(g) ? 'Not in this campaign.' : abroad(g, a) ? 'An agent hunts enemy agents only on friendly ground.' : null;
  if (!abroad(g, a)) return `${a.name} is on friendly ground. Send the agent abroad first.`;
  if (m === 'watch') return null;
  if (m === 'books') {
    if (!host?.alive || host.capital !== r.id || r.owner !== r.ctrl) return 'A government keeps its books in its capital. Send the agent to one.';
    return null;
  }
  if (m === 'orders') {
    if (!ordersOn(g)) return 'Not in this campaign.';
    if (!atWar(g, nat, r.ctrl)) return `You are at peace with ${host?.name ?? 'this country'}: its air staff has no orders against you.`;
    if (!groupsOnField(g, r.id, (n) => atWar(g, nat, n)).some((f) => f.bmr > 0)) return `No enemy bomber group flies from ${r.name}.`;
    return null;
  }
  if (isTheft(m)) return whyNoTheft(g, nat, a, r, THEFT[m]!);
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
  if (m === 'airfield') {
    if (!airPiece(g, 'field')) return 'Not in this campaign.';
    if (fieldWrecked(g, r.id)) return `The field at ${r.name} is already wrecked this month.`;
    if (!groupsOnField(g, r.id, (n) => atWar(g, nat, n)).length) return `No enemy air group flies from ${r.name}.`;
    return null;
  }
  if (!r.oil) return `There is no oilfield at ${r.name}.`;
  if (fieldOut(g, r) > 0) return `The wells at ${r.name} are already burning.`;
  return null;
}

/** True where a great power keeps its plans of this weapon: its own ground, in its capital or where it makes them (plans.ts). */
export function keepsPlans(g: Game, host: string, r: Region, x: PlanItem): boolean {
  const H = g.nations[host], W = PLAN_WORDS[x];
  if (!H || r.ctrl !== host || r.owner !== host) return false;
  if (H.capital === r.id) return true;
  if (W.place === 'yard') return isCapitalHull(x) && yardLevel(g, r) >= ARSENAL.ships[x].yard;
  if (W.place === 'works') return r.fac >= ARSENAL.air[W.wing ?? 'bmr'].factories;
  return false;
}

/** Why this agent cannot try for a great power's plans of this weapon where it stands, or null. At peace as at war. */
function whyNoTheft(g: Game, nat: string, a: Agent, r: Region, x: PlanItem): string | null {
  const why = theftBar(g, nat, a, r, x);
  return why === null ? null : theWords(why);
}

function theftBar(g: Game, nat: string, a: Agent, r: Region, x: PlanItem): string | null {
  const W = PLAN_WORDS[x], P = PLAN_PLACES[W.place], host = g.nations[r.ctrl], N = g.nations[nat], hull = isCapitalHull(x);
  if (!planRules(g)) return `Every country here ${hull ? 'draws' : 'develops'} its own ${W.many}.`;
  if (PLANS.great.includes(nat)) return hull ? `${N.name} lays down ${W.many} on its own designs.` : `${N.name} develops its own ${W.many}.`;
  const from = plansFrom(g, nat, x);
  if (from !== null) return `${cap(W.who)} already ${W.verbs} ${W.many} on ${g.nations[from]?.adj ?? from} designs.`;
  if (PLANS.kin[nat] !== undefined) return whyNoTechPlans(g, nat, x);
  if (!host?.alive || !PLANS.great.includes(r.ctrl)) return `Plans of ${W.many} are kept by the great powers, in ${P.kept}. Send the agent to one.`;
  if (r.owner !== r.ctrl) return `${host.name} keeps its plans at home, in ${P.home}: ${r.name} is ground it has taken.`;
  const capital = g.regions[host.capital]?.name ?? 'its capital';
  if (!keepsPlans(g, r.ctrl, r, x)) return W.place === 'capital' ? `${host.name} keeps its ${W.one} plans at ${capital}, its capital.` : `${P.lack} ${r.name}: ${host.name} keeps its plans at ${capital} and at ${P.home.replace('its capital and ', 'its ')}.`;
  if (!givesPlans(g, r.ctrl, x)) return hull ? `${host.name} builds no ${W.many} yet: there are no plans to steal.` : `${host.name} has no ${W.many} yet: there are no plans to steal.`;
  if ((a.rest ?? 0) > g.turn) { const k = (a.rest as number) - g.turn; return `${a.name} is lying low for ${k === 1 ? 'another month' : `${k} more months`} after the last attempt.`; }
  return null;
}

const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);

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
    air: g.airFleets.filter((f) => f.base === reg && f.ship === undefined && !allied(g, f.nat, a.nat)).map((f) => ({ name: f.name, nat: f.nat, ftr: Math.round(f.ftr), bmr: Math.round(f.bmr) })),
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
export function armyKnown(g: Game, of: string, viewer: string | null = g.human): { divs: number; exact: boolean; few?: boolean } {
  const all = fieldArmy(g, of);
  if (!fogOn(g, viewer) || knowsBooks(g, of, viewer)) return { divs: all, exact: true };
  const k = INTEL.estimate.round;
  // An army too small to round to the nearest ten is known only to be small.
  return all < k / 2 ? { divs: k, exact: false, few: true } : { divs: Math.max(k, Math.round(all / k) * k), exact: false };
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
  const ok = draw(g, g.turn, a.id, 'try') < successChance(a, m, g);
  let text: string;
  let done = false;
  if (ok && m === 'factory' && wreckFactory(g, r)) { done = true; text = `Agent ${a.name} wrecks a factory at ${r.name}. ${host.adj} industry is one works short until engineers rebuild it.`; }
  else if (ok && m === 'rail' && cutRail(g, r.id, 'agent', a.nat)) { done = true; text = `Agent ${a.name} blows the railway at ${r.name}. No ${host.adj} train runs through until engineers relay the track.`; }
  else if (ok && m === 'airfield' && wreckField(g, r.id, (n) => atWar(g, a.nat, n))) {
    done = true;
    const k = groupsOnField(g, r.id, (n) => atWar(g, a.nat, n)).length;
    text = `Agent ${a.name}'s saboteurs wreck the field at ${r.name}. ${k === 1 ? `The ${host.adj} air group based there is` : `All ${k} ${host.adj} air groups based there are`} grounded this month.`;
  }
  else if (ok && m === 'oil' && fireField(g, r, INTEL.missions.oil.months ?? 3, INTEL.missions.oil.cut ?? 0.5)) { done = true; text = `Agent ${a.name} sets the oilfield at ${r.name} alight. The wells will burn for ${INTEL.missions.oil.months} months.`; }
  else if (ok && isTheft(m) && grantPlans(g, a.nat, THEFT[m]!, r.ctrl, 'stolen')) { done = true; text = `Agent ${a.name} copies the ${host.adj} ${PLAN_WORDS[THEFT[m]!].one} plans at ${r.name} and gets them out. ${canNowWords([THEFT[m]!], host.adj)}`; }
  else text = isTheft(m) ? `Agent ${a.name} cannot get into the ${host.adj} drawing offices at ${r.name}: the plans stay where they are.`
    : m === 'factory' ? `Agent ${a.name}'s attempt on the factories at ${r.name} fails: the guards were too many.`
    : m === 'rail' ? `Agent ${a.name}'s charges on the railway at ${r.name} are found before they go off.`
      : m === 'airfield' ? `Agent ${a.name} cannot get past the sentries on the field at ${r.name}.`
        : `Agent ${a.name} cannot get near the wells at ${r.name}.`;
  note(g, a, r.id, done ? 'sabotage' : 'failed', text, done);
  headline(g, done ? 'event' : 'info', text, a.nat, r.id);
  a.mission = 'watch';
  // (Cover identities, tech.json: an agent becomes someone else after an attempt and lies low a month less, never under one.)
  a.rest = g.turn + Math.max(1, INTEL.risk.lieLow - spyOf(g, a.nat).rest);
  g.intel.heat[r.id] = g.turn + INTEL.risk.heatMonths;
  if (done) learn(g, a, INTEL.missions[m].gain ?? 1);
}

/** True when the great power that caught this spy at its plans goes to war over it: at peace with the thief, not its ally, at the chance in intel.json. */
function theftWar(g: Game, a: Agent, host: string): boolean {
  const H = g.nations[host];
  if (!H?.alive || host === a.nat || atWar(g, host, a.nat) || allied(g, host, a.nat)) return false;
  return draw(g, g.turn, a.id, 'war') < (INTEL.caught.war ?? 0);
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
  // (Counter-intelligence, tech.json: the guards stop a plot this much more often.)
  const p = (near ? E.near + INTEL.skill.guard * (skillOf(near) - 1) + E.far * (guards.length - 1) : E.far * guards.length) + spyOf(g, nat).guard;
  return { chance: clamp(p, 0, E.most), by: near ?? [...guards].sort((x, y) => skillOf(y) - skillOf(x) || x.id - y.id)[0] };
}

// ---- The secret war: spy hunters (ruling 151(c)) and stolen orders (ruling 135) -------------------------------

const SECRET = INTEL.secret;

/**
 * The month of the other side's agents on the player's ground, with spy hunters on (ruling 151(c)): his hunters
 * search, the agents whose month has come strike (a guard can still stop them), and the services put in new agents
 * at the rate their plots struck at before, so that in sum the blows come as often as they did.
 */
function foesMonth(g: Game, nat: string): void {
  const foes = (g.intel.foes ??= []);
  // His hunters search their province and the provinces around it.
  for (const a of agentsOf(g, nat).filter((x) => x.mission === 'hunt' && atWork(g, x) && !abroad(g, x)).sort((x, y) => x.id - y.id)) {
    const r = g.regions[a.reg];
    for (const f of foes) {
      if (f.found !== undefined) continue;
      const base = f.reg === a.reg ? SECRET.hunters.here : r.nb.includes(f.reg) ? SECRET.hunters.near : 0;
      if (base <= 0 || draw(g, g.turn, 'hunt', a.id, f.id) >= clamp(base + SECRET.hunters.skill * (skillOf(a) - 1), 0, 0.95)) continue;
      f.found = g.turn;
      const F = g.nations[f.nat], at = g.regions[f.reg];
      const text = `Agent ${a.name} uncovers ${/^[AEIOU]/.test(F?.adj ?? '') ? 'an' : 'a'} ${F?.adj ?? 'enemy'} agent at ${at.name}. Arrest it before it strikes.`;
      note(g, a, f.reg, 'uncovered', text, true);
      headline(g, 'event', text, nat, f.reg);
      learn(g, a, 1);
    }
  }
  // The agents whose month has come strike, unless the province is no longer his or a guard stops them.
  for (const f of foes.filter((x) => x.strike <= g.turn).sort((x, y) => x.id - y.id)) {
    foes.splice(foes.indexOf(f), 1);
    const r = g.regions[f.reg], F = g.nations[f.nat];
    if (!F?.alive || r.ctrl !== nat || r.owner !== nat || !atWar(g, nat, f.nat)) continue;
    const G = guardChance(g, nat, r.id);
    const who = { name: G.by?.name ?? '', nat };
    if (G.by && draw(g, g.turn, 'fguard', f.id) < G.chance) {
      const text = `Agent ${G.by.name}'s watchers catch ${/^[AEIOU]/.test(F.adj) ? 'an' : 'a'} ${F.adj} sabotage ring${f.kind === 'airfield' ? ' on the field' : ''} at ${r.name} before it can strike.`;
      note(g, who, r.id, 'foiled', text, true);
      headline(g, 'info', text, nat, r.id);
      learn(g, G.by, 1);
      continue;
    }
    const mine = (n: string): boolean => n === nat;
    const struck = f.kind === 'rail' ? cutRail(g, r.id, 'saboteurs') : f.kind === 'airfield' ? wreckField(g, r.id, mine) : wreckFactory(g, r);
    if (!struck) continue;
    const text = f.kind === 'rail'
      ? `Saboteurs blow the railway at ${r.name}. Your police blame ${F.adj} agents. No train runs through until engineers relay the track.`
      : f.kind === 'airfield' ? `Grounded this month: saboteurs wrecked the field at ${r.name}. Your police blame ${F.adj} agents.`
        : `Saboteurs wreck a factory at ${r.name}. Your police blame ${F.adj} agents. It stands idle until engineers rebuild it.`;
    note(g, who, r.id, 'struck', text, false);
    headline(g, 'event', text, nat, r.id);
  }
  // New agents come in where the plots of old would have struck at once.
  const T = threat(g, nat);
  for (const foe of T.foes) {
    if (draw(g, g.turn, 'plot', foe) >= T.each) continue;
    const aim = plotAim(g, nat, foe);
    if (!aim) continue;
    const id = g.intel.foeNext ?? 1;
    g.intel.foeNext = id + 1;
    foes.push({ id, nat: foe, reg: aim.reg, kind: aim.kind, strike: g.turn + SECRET.hunters.lead });
  }
}

/** Where a plot is aimed, as the services pick it: an airfield he flies from (with its switch), a railway or a factory. */
function plotAim(g: Game, nat: string, foe: string): { reg: number; kind: FoeAgent['kind'] } | null {
  const K = INTEL.enemy.kinds;
  if (airPiece(g, 'field') && draw(g, g.turn, 'field', foe) < AIRV.field.enemyShare) {
    const pool: Region[] = [];
    for (const r of g.regions) if (r.ctrl === nat && r.owner === nat && !r.overseas) for (const _ of groupsOnField(g, r.id, (n) => n === nat)) pool.push(r);
    if (pool.length) return { reg: pool[Math.floor(draw(g, g.turn, 'fieldwhere', foe) * pool.length) % pool.length].id, kind: 'airfield' };
  }
  const rail = draw(g, g.turn, 'kind', foe) < K.rail / (K.rail + K.factory);
  const pool: Region[] = [];
  for (const r of g.regions) {
    if (r.ctrl !== nat || r.owner !== nat || r.overseas) continue;
    if (rail) { if (onNet(r.id) && !railCut(g, r.id)) pool.push(r); }
    else if (workingFactories(g, r) > 0 && ruinedAt(g, r) < ruinMost(r)) for (let k = 0; k < workingFactories(g, r); k++) pool.push(r);
  }
  if (!pool.length) return null;
  return { reg: pool[Math.floor(draw(g, g.turn, 'where', foe) * pool.length) % pool.length].id, kind: rail ? 'rail' : 'factory' };
}

/** The other side's agents his hunters have uncovered and that are still at large: each can be arrested. */
export const foesFound = (g: Game, nat: string | null = g.human): FoeAgent[] =>
  nat && huntersOn(g) ? (g.intel.foes ?? []).filter((f) => f.found !== undefined && g.regions[f.reg].ctrl === nat) : [];

/** Why an uncovered enemy agent cannot be arrested, or null. */
export function whyNotArrest(g: Game, nat: string, id: number): string | null {
  if (!huntersOn(g)) return 'Not in this campaign.';
  const f = (g.intel.foes ?? []).find((x) => x.id === id);
  if (!f) return 'No such agent is at large.';
  if (f.found === undefined) return 'Nobody knows where that agent is.';
  if (g.regions[f.reg].ctrl !== nat) return 'That ground is no longer yours.';
  return null;
}

/** Arrests an uncovered enemy agent: it is ended, and nothing more is shown (ruling 151(c)). */
export function arrest(g: Game, nat: string, id: number): boolean {
  if (whyNotArrest(g, nat, id)) return false;
  const foes = g.intel.foes!, f = foes.find((x) => x.id === id)!;
  foes.splice(foes.indexOf(f), 1);
  const F = g.nations[f.nat], r = g.regions[f.reg];
  const text = `${/^[AEIOU]/.test(F?.adj ?? '') ? 'An' : 'A'} ${F?.adj ?? 'enemy'} agent is arrested at ${r.name}.`;
  note(g, { name: '', nat }, r.id, 'arrested', text, true);
  headline(g, 'info', text, nat, r.id);
  return true;
}

/**
 * An agent on STEAL THE ORDERS tries each month (ruling 135): on a success the strongest bomber group on the field
 * that could fly has next month's order written now, and the player reads it. It does not always work: the agent may
 * find nothing, or no raid is planned from there.
 */
function stealOrders(g: Game, a: Agent): void {
  const r = g.regions[a.reg];
  pruneOrders(g);
  const ok = draw(g, g.turn, a.id, 'orders') < successChance(a, 'orders');
  if (!ok) {
    note(g, a, r.id, 'nothing', `Agent ${a.name} finds nothing at ${r.name} this month: the orders were not to be had.`, false);
    return;
  }
  const host = r.ctrl;
  const o = fieldOrder(g, host, r.id);
  const H = g.nations[host];
  if (!o) {
    const text = `Agent ${a.name}, ${r.name}: no ${H.adj} raid is ordered from the field for next month.`;
    note(g, a, r.id, 'orders', text, true);
    headline(g, 'info', text, a.nat, r.id);
    learn(g, a, INTEL.missions.orders.gain ?? 1);
    return;
  }
  (g.intel.orders ??= []).push({ nat: host, field: r.id, groups: o.groups, target: o.target, mission: o.mission, bombers: o.bombers, turn: g.turn + 1, agent: a.name, by: a.nat });
  const names = o.groups.map((id) => g.airFleets.find((f) => f.id === id)).filter((f) => f && f.bmr > 0).map((f) => f!.name);
  const t = g.regions[o.target];
  const text = `Agent ${a.name}, ${r.name}: ${names.join(' and ')} ${names.length > 1 ? 'fly' : 'flies'} against ${t.name} next month, ${o.mission === 'night' ? 'by night' : 'by day'}, about ${o.bombers.toLocaleString('en-US')} bombers.`;
  note(g, a, r.id, 'orders', text, true);
  headline(g, 'event', text, a.nat, o.target);
  learn(g, a, INTEL.missions.orders.gain ?? 1);
}

/** An agent caught at the field: the enemy changes the orders it read there. The words for the line. */
function cancelOrders(g: Game, a: Agent): string {
  const read = (g.intel.orders ?? []).filter((o) => o.agent === a.name && o.by === a.nat && o.field === a.reg && o.turn > g.turn && !o.done);
  for (const o of read) o.done = 'lapsed';
  return read.length ? ` The enemy changes the orders ${a.name} had read.` : '';
}

/** THE ENEMY'S ORDERS, for the AIR tab (ruling 135(b)): what the player's agents have read and is still to come. */
export function enemyOrders(g: Game, viewer: string | null = g.human): { text: string; target: number; turn: number; done?: string }[] {
  return ordersKnown(g, viewer).map((o) => {
    const t = g.regions[o.target], F = g.nations[o.nat];
    const when = o.turn > g.turn ? 'next month' : 'this month';
    const state = o.done === 'flown' ? ' Flown.' : o.done === 'lapsed' ? ' Cancelled.' : '';
    return { text: `${F.adj} bombers from ${g.regions[o.field].name} against ${t.name} ${when}, ${o.mission === 'night' ? 'by night' : 'by day'}, about ${o.bombers.toLocaleString('en-US')}: read by Agent ${o.agent}.${state}`, target: o.target, turn: o.turn, done: o.done };
  });
}

/**
 * A plot against one of the player's airfields on home ground (ruling 151(b)), picked by how many of his air groups
 * are based there; a guard can stop it as any other. False if he has no group based at home, so the plot goes as it
 * always did.
 */
function plotField(g: Game, nat: string, foe: string): boolean {
  const mine = (n: string): boolean => n === nat;
  const pool: Region[] = [];
  for (const r of g.regions) {
    if (r.ctrl !== nat || r.owner !== nat || r.overseas || fieldWrecked(g, r.id)) continue;
    for (const _ of groupsOnField(g, r.id, mine)) pool.push(r);
  }
  if (!pool.length) return false;
  const F = g.nations[foe];
  const r = pool[Math.floor(draw(g, g.turn, 'fieldwhere', foe) * pool.length) % pool.length];
  const G = guardChance(g, nat, r.id);
  const who = { name: G.by?.name ?? '', nat };
  if (G.by && draw(g, g.turn, 'guard', foe) < G.chance) {
    const text = `Agent ${G.by.name}'s watchers catch ${/^[AEIOU]/.test(F.adj) ? 'an' : 'a'} ${F.adj} sabotage ring on the field at ${r.name} before it can strike.`;
    note(g, who, r.id, 'foiled', text, true);
    headline(g, 'info', text, nat, r.id);
    learn(g, G.by, 1);
    return true;
  }
  if (!wreckField(g, r.id, mine)) return false;
  const text = `Grounded this month: saboteurs wrecked the field at ${r.name}. Your police blame ${F.adj} agents.`;
  note(g, who, r.id, 'struck', text, false);
  headline(g, 'event', text, nat, r.id);
  return true;
}

/** The other side's services at work against the player's home ground. */
function plots(g: Game, nat: string): void {
  if (huntersOn(g)) { foesMonth(g, nat); return; }
  const T = threat(g, nat);
  for (const foe of T.foes) {
    if (draw(g, g.turn, 'plot', foe) >= T.each) continue;
    const F = g.nations[foe], K = INTEL.enemy.kinds;
    // With the field's switch on, a share of the plots go for an airfield the player flies from (ruling 151(b)).
    if (airPiece(g, 'field') && draw(g, g.turn, 'field', foe) < AIRV.field.enemyShare && plotField(g, nat, foe)) continue;
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
    const struck = rail ? cutRail(g, r.id, 'saboteurs') : wreckFactory(g, r);
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
    if (!abroad(g, a)) { if (a.mission !== 'hunt' || !huntersOn(g)) a.mission = 'guard'; continue; }
    // Ground that has changed hands under an agent: a guard finds itself behind the lines, a reader's capital has fallen.
    if (AT_HOME.includes(a.mission) || (a.mission !== 'watch' && whyNotMission(g, nat, a.id, a.mission))) a.mission = 'watch';
    const was = a.mission;
    // The risk is the one the player was shown when the order was given: the alarm an attempt raises counts from next month.
    const risk = caughtChance(g, a, was);
    if (triedOnce(was)) attempt(g, a);
    if (was === 'orders') stealOrders(g, a);
    if (draw(g, g.turn, a.id, 'caught') < risk) {
      const host = g.nations[r.ctrl];
      // (Caught at the field: the enemy changes the orders it may have read.)
      const changed = was === 'orders' ? cancelOrders(g, a) : '';
      // (Brian, 18 November: a great power that catches a spy at its plans may declare war on the country that sent it.)
      const war = isTheft(was) && theftWar(g, a, r.ctrl);
      lose(g, a, `Agent ${a.name} is caught at ${r.name} by ${host.adj} counter-intelligence. Nothing more is heard.${changed}${war ? ` ${host.name} declares war on ${g.nations[nat].name} over the attempt on its plans.` : resent(g, a, r.ctrl)}`, 'caught');
      if (war) { g.flags[`provoked:${r.ctrl}:${nat}`] = g.turn; declareWar(g, r.ctrl, nat); }
      continue;
    }
    a.months = (a.months ?? 0) + 1;
    if (a.months % INTEL.agents.watch === 0) learn(g, a, 1);
  }
  for (const k of Object.keys(g.intel.heat)) if (g.intel.heat[+k] <= g.turn) delete g.intel.heat[+k];
  // (A field wrecked in an earlier month is mended: its mark goes.)
  if (airPiece(g, 'field')) for (const k of Object.keys(g.flags)) if (k.startsWith('wreck:') && (g.flags[k] ?? 0) < g.turn) delete g.flags[k];
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
  /** With spy hunters (ruling 151(c)): the enemy agents uncovered and still at large, each with ARREST. */
  found?: { id: number; nat: string; reg: number; where: string; text: string }[];
  /** With stolen orders (ruling 135): THE ENEMY'S ORDERS. */
  orders?: { text: string; target: number; turn: number; done?: string }[];
}

function viewOf(g: Game, a: Agent): AgentView {
  const r = g.regions[a.reg], out = abroad(g, a), work = atWork(g, a);
  const offers: MissionOffer[] = missionsIn(g).filter((m) => AT_HOME.includes(m) !== out).map((m) => {
    const why = whyNotMission(g, a.nat, a.id, m);
    return { id: m, label: MISSION_TEXT[m].label, text: MISSION_TEXT[m].text, ok: why === null, why: why ?? '', now: a.mission === m, caught: caughtChance(g, a, m), success: triedOnce(m) ? successChance(a, m, g) : null };
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
    : `${a.mission === 'guard' ? 'On guard' : a.mission === 'hunt' ? 'Hunting enemy agents' : a.mission === 'watch' ? 'Watching' : a.mission === 'books' ? 'Reading the books' : a.mission === 'orders' ? 'Stealing the orders: tried each month' : MISSION_TEXT[a.mission].label + ': tried next month'} at ${r.name}`;
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
    nat, agents: mine.map((a) => viewOf(g, a)), most: mostAgents(nat, g), cost: agentCost(), whyNotRecruit: whyNotRecruit(g, nat),
    notes: g.intel.notes.filter((n) => n.nat === nat).slice().reverse(),
    fog: fogOn(g, nat),
    books: Object.values(g.nations).filter((n) => n.alive && !allied(g, n.id, nat) && knowsBooks(g, n.id, nat)).map((n) => n.id),
    threat: { chance: T.chance, foes: T.foes, guards: mine.filter((a) => a.mission === 'guard' && atWork(g, a) && !abroad(g, a)).length },
    ...(huntersOn(g) ? { found: foesFound(g, nat).map((f) => ({ id: f.id, nat: f.nat, reg: f.reg, where: g.regions[f.reg].name, text: `${/^[AEIOU]/.test(g.nations[f.nat]?.adj ?? '') ? 'An' : 'A'} ${g.nations[f.nat]?.adj ?? 'enemy'} agent at ${g.regions[f.reg].name}, uncovered ${f.found === g.turn ? 'this month' : 'earlier'}` })) } : {}),
    ...(ordersOn(g) ? { orders: enemyOrders(g, nat) } : {}),
  };
}
