// Research and doctrine. Every major power's weapons improve on history's clock: a branch
// with one design team gets each new design in the month history got it. More teams on a
// branch bring its designs forward, none lets it fall behind, and what counts in battle is
// how far a branch is ahead of or behind its own history. A nation that leaves one team on
// every branch fights exactly as it did. The designs and dates live in
// data/campaign/research.json so they can be changed without code.

import data from '../../data/campaign/research.json';
import type { Field, Game, Lab, Nation } from './types';
import { headline } from './game';

export interface Step { at: number; name: string }
interface FieldSpec { label: string; noun: string; per: number; who: string; news: string }

export const FIELDS: Field[] = ['armour', 'infantry', 'doctrine', 'fighters', 'bombers', 'navy', 'radar', 'industry'];

export const RESEARCH = data as unknown as {
  speed: number[];
  maxLead: number;
  bureau: { cost: number[] };
  fields: Record<Field, FieldSpec>;
  nations: Record<string, { tree: string } & Record<Field, [string, string][]>>;
};

/** The month a design entered service in history, as a turn number: September 1939 is 1, anything earlier is 0 or less. */
const turnOf = (ym: string): number => {
  const [y, m] = ym.split('-').map(Number);
  return (y - 1939) * 12 + m - 8;
};

const STEPS: Record<string, Record<Field, Step[]>> = {};
for (const [nat, d] of Object.entries(RESEARCH.nations)) {
  STEPS[nat] = {} as Record<Field, Step[]>;
  for (const f of FIELDS) STEPS[nat][f] = d[f].map(([ym, name]) => ({ at: turnOf(ym), name }));
}

/** The most teams one branch can use, and the nations that do their own research. */
export const MAX_TEAMS = RESEARCH.speed.length - 1;
export const researches = (nat: string): boolean => nat in STEPS;
export const stepsOf = (nat: string, f: Field): Step[] => STEPS[nat]?.[f] ?? [];
/** The name of a nation's army doctrine. */
export const doctrineOf = (nat: string): string => RESEARCH.nations[nat]?.tree ?? '';

const each = <T>(v: () => T): Record<Field, T> => Object.fromEntries(FIELDS.map((f) => [f, v()])) as Record<Field, T>;

/** A new set of design teams: one on every branch, with the pre-war designs in service. */
export function newLab(): Lab {
  return { teams: each(() => 1), clock: each(() => 0), months: 0, founded: 0 };
}

export function setupResearch(g: Game): void {
  for (const n of Object.values(g.nations)) n.lab = researches(n.id) ? newLab() : null;
}

const count = (steps: Step[], clock: number): number => {
  let k = 0;
  for (const s of steps) if (s.at <= clock) k++;
  return k;
};

/**
 * How many designs a branch is ahead of its own history (or behind, when negative): the designs in
 * service against the ones history had by now.
 */
export function lead(g: Game, nat: string, f: Field): number {
  const lab = g.nations[nat]?.lab;
  if (!lab) return 0;
  const steps = stepsOf(nat, f);
  return Math.max(-RESEARCH.maxLead, Math.min(RESEARCH.maxLead, count(steps, lab.clock[f]) - count(steps, lab.months)));
}

/** What a branch's lead or lag is worth: 1 for a nation on history's schedule. */
export function techMul(g: Game, nat: string, f: Field): number {
  const k = lead(g, nat, f);
  return k === 0 ? 1 : 1 + RESEARCH.fields[f].per * k;
}

/** The design a branch has in service now, and the next one coming. */
export function inService(nat: string, lab: Lab, f: Field): Step | null {
  let cur: Step | null = null;
  for (const s of stepsOf(nat, f)) if (s.at <= lab.clock[f]) cur = s;
  return cur;
}

export function nextStep(nat: string, lab: Lab, f: Field): Step | null {
  return stepsOf(nat, f).find((s) => s.at > lab.clock[f]) ?? null;
}

export const speedOf = (lab: Lab, f: Field): number => RESEARCH.speed[Math.min(MAX_TEAMS, lab.teams[f])] ?? 0;

/** The turn the next design arrives at the present pace, or null if nobody is working on it (or nothing is left to design). */
export function arrival(g: Game, nat: string, f: Field): number | null {
  const lab = g.nations[nat]?.lab;
  if (!lab) return null;
  const next = nextStep(nat, lab, f), v = speedOf(lab, f);
  if (!next || v <= 0) return null;
  // The teams have worked 'months' months, one for every turn so far, so the count is also the turn they last worked in.
  return lab.months + Math.max(1, Math.ceil((next.at - lab.clock[f]) / v - 1e-9));
}

/** Months a branch's clock is ahead of history's (negative when behind). */
export const monthsAhead = (lab: Lab, f: Field): number => lab.clock[f] - lab.months;

export const teamsTotal = (lab: Lab): number => FIELDS.length + lab.founded;
export const teamsUsed = (lab: Lab): number => FIELDS.reduce((s, f) => s + lab.teams[f], 0);
export const teamsFree = (lab: Lab): number => teamsTotal(lab) - teamsUsed(lab);

/** Puts this many design teams on a branch. Teams taken off a branch are free to go elsewhere. */
export function setTeams(g: Game, nat: string, f: Field, teams: number): boolean {
  const lab = g.nations[nat]?.lab;
  if (!lab || !FIELDS.includes(f) || !Number.isInteger(teams) || teams < 0 || teams > MAX_TEAMS) return false;
  if (teamsUsed(lab) - lab.teams[f] + teams > teamsTotal(lab)) return false;
  lab.teams[f] = teams;
  return true;
}

/** What founding one more design team would cost, or null if the nation has all it can use. */
export function bureauCost(n: Nation): number | null {
  return n.lab ? RESEARCH.bureau.cost[n.lab.founded] ?? null : null;
}

/** Founds a new design team, paid for from production. It starts unassigned. */
export function foundBureau(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  const cost = n ? bureauCost(n) : null;
  if (!n || !n.alive || !n.lab || cost === null || n.pp < cost) return false;
  n.pp -= cost;
  n.lab.founded++;
  return true;
}

const plural = (k: number, one: string): string => `${k} ${one}${k === 1 ? '' : 's'}`;

/** A month of work in the design offices: every branch moves on as fast as its teams allow. */
export function researchTick(g: Game, nat: string): void {
  const lab = g.nations[nat]?.lab;
  if (!lab) return;
  const was = lab.months;
  lab.months++;
  for (const f of FIELDS) {
    const steps = stepsOf(nat, f);
    const before = lab.clock[f];
    lab.clock[f] = before + speedOf(lab, f);
    if (nat !== g.human) continue;
    const spec = RESEARCH.fields[f];
    for (const s of steps) {
      if (s.at > before && s.at <= lab.clock[f]) {
        const early = s.at - lab.months;
        headline(g, 'info', `${spec.news}: ${s.name}${early > 0 ? `, ${plural(early, 'month')} ahead of history` : early < 0 ? `, ${plural(-early, 'month')} later than in history` : ''}.`, nat);
      } else if (s.at > was && s.at <= lab.months && s.at > lab.clock[f]) {
        headline(g, 'info', `${spec.label} falling behind: by now history had ${s.name}.`, nat);
      }
    }
  }
}

/** One side's equipment, for a battle ticket: which of these branches are ahead of or behind their time, and by how much. */
export function techNotes(g: Game, nats: Iterable<string>, fields: Field[]): { text: string; pct: number }[] {
  const out: { text: string; pct: number }[] = [];
  for (const nat of new Set(nats)) {
    for (const f of fields) {
      const k = lead(g, nat, f);
      if (!k) continue;
      const gen = Math.abs(k) === 1 ? 'a generation' : `${Math.abs(k)} generations`;
      out.push({ text: `${g.nations[nat].adj} ${RESEARCH.fields[f].noun} ${gen} ${k > 0 ? 'ahead of' : 'behind'} history`, pct: Math.round(RESEARCH.fields[f].per * k * 100) });
    }
  }
  return out;
}
