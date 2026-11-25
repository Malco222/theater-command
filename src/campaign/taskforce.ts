// Task forces (release 2; Brian, 24 November 2026 18:24Z, "i like these", of the idea "a squadron you split off keeps a
// standing job, like guarding a convoy lane, until you change it"). Behind its own switch (stage.json 'taskForce', the
// flag 'taskForce'): off, nothing here is read and every fleet is ordered as before.
//
// One of the player's fleets at sea on a job (a patrol, a raid or a submarine hunt in a sea, or the escort of a shipping
// lane) is made a task force: the job is written down with it. From then on, at the start of each of his months, it is
// worked the way a computer's admiral works his own fleets (navy.ts navalAI), but only ever on that one job: worn out,
// it comes into harbour to refit; fit again, it goes back to its job; driven off it by a lost battle, it goes back the
// same way once it is fit. A new order of his to the fleet rewrites the job, and an order into harbour ends it. Like a
// standing order pinned to the fleet's door: it is read every month until he takes it down.
//
// The fleet is his own (Fleet.ordered): his admirals, where they run the navy for him, leave it alone. No computer
// nation has one; its admirals do all of this already.

import data from '../../data/campaign/taskforce.json';
import { headline } from './game';
import { atSea, escort, fleetReach, sail, seaRoutes, seaWar, shipCount, whyNotEscort } from './navy';
import type { Fleet, Game, Stance } from './types';

export const TASKFORCE = data as unknown as { refit: number; back: number };

/** A task force's job: the sea and what it does there, or the lane it escorts. */
export interface TaskJob { zone: number; stance: Stance; lane?: string }

/** True where the player may make task forces: the grand campaign, with the switch on. */
export const taskForceOn = (g: Game): boolean => g.flags.open === 1 && g.flags.taskForce === 1;

/** Turns task forces on or off in a campaign under way. Off, every task force is released (its fleet stays his). */
export function setTaskForce(g: Game, on: boolean): void {
  if (on) g.flags.taskForce = 1;
  else {
    delete g.flags.taskForce;
    for (const f of g.fleets) delete f.task;
  }
}

const hisMonth = (g: Game): boolean => g.human !== null && g.order[g.phase] === g.human;

/** The job a fleet is doing now, if it is at sea on one. */
export function jobNow(f: Fleet): TaskJob | null {
  if (!atSea(f)) return null;
  if (f.stance === 'escort') return f.lane !== undefined ? { zone: f.zone, stance: 'escort', lane: f.lane } : null;
  return f.stance === 'patrol' || f.stance === 'raid' || f.stance === 'hunt' ? { zone: f.zone, stance: f.stance } : null;
}

/** True while the fleet is on its task force's job. (An escort may be in any sea of its lane.) */
export function onJob(f: Fleet): boolean {
  const t = f.task, now = jobNow(f);
  if (!t || !now) return false;
  return t.stance === 'escort' ? now.stance === 'escort' && now.lane === t.lane : now.stance === t.stance && now.zone === t.zone;
}

/** Why this fleet cannot be made a task force now, or null when it can. */
export function whyNotTask(g: Game, f: Fleet | undefined): string | null {
  if (!taskForceOn(g)) return 'Task forces are made only where that is switched on.';
  if (!f || !shipCount(f.ships)) return 'There is no such fleet.';
  if (f.nat !== g.human || !hisMonth(g)) return 'Only your own fleets, in your own month.';
  if (f.task) return 'It is a task force already.';
  if (!g.nations[f.nat].atWar.length) return 'In peace a fleet keeps to harbour: a task force is made in a war.';
  if (!jobNow(f)) return 'Send it out on a job first: a patrol, a raid, a submarine hunt or a convoy escort. That job becomes its standing order.';
  return null;
}

/** Makes one of the player's fleets a task force on the job it is doing now. False where it cannot be. */
export function makeTask(g: Game, f: Fleet): boolean {
  if (whyNotTask(g, f) !== null) return false;
  f.task = jobNow(f)!;
  f.ordered = true;
  return true;
}

/** Releases a task force: it stays where it is, under his orders, with no standing job. */
export function releaseTask(g: Game, f: Fleet): boolean {
  if (!f.task || f.nat !== g.human) return false;
  delete f.task;
  return true;
}

/** After a new order of his to a task force: the job it is now doing becomes its standing order; into harbour, it is released. */
export function retask(g: Game, f: Fleet): void {
  if (!f.task || !taskForceOn(g)) return;
  const now = jobNow(f);
  if (now) f.task = now;
  else if (!atSea(f)) delete f.task;
}

/** What the job is, in words: "guards the Atlantic lane", "patrols the North Sea". */
export function jobWords(g: Game, t: TaskJob): string {
  if (t.stance === 'escort') return `guards ${seaRoutes(g).find((r) => r.id === t.lane)?.name ?? 'its lane'}`;
  const sea = g.seas[t.zone]?.name ?? 'its sea';
  return t.stance === 'raid' ? `raids shipping in the ${sea}` : t.stance === 'hunt' ? `hunts submarines in the ${sea}` : `patrols the ${sea}`;
}

/** Why a task force cannot go back to its job now, or null when it can. */
export function whyNotBack(g: Game, f: Fleet): string | null {
  const t = f.task;
  if (!t) return 'It has no standing job.';
  const reach = fleetReach(g, f);
  if (t.stance === 'escort') {
    const why = whyNotEscort(g, f, t.lane ?? '');
    if (why) return why;
    const rt = seaRoutes(g).find((r) => r.id === t.lane);
    return rt && rt.zones.some((z) => reach.includes(z)) ? null : `No sea of ${rt?.name ?? 'its lane'} is in its reach from ${f.port}.`;
  }
  if (t.stance === 'hunt' && !seaWar(g)) return 'Submarines are hunted only in the grand campaign.';
  return reach.includes(t.zone) ? null : `The ${g.seas[t.zone]?.name ?? 'sea'} is out of its reach from ${f.port}.`;
}

/**
 * The task forces of one nation at the start of its month (only ever the player's): the worn come in to refit, the fit
 * go back to their jobs. One line of news for each that comes or goes.
 */
export function taskForcesMonth(g: Game, nat: string): void {
  if (!taskForceOn(g) || nat !== g.human) return;
  for (const f of g.fleets) {
    if (f.nat !== nat || !f.task) continue;
    if (!shipCount(f.ships)) { delete f.task; continue; }
    // (A task force in passage to a new base keeps on its way, and goes back to its job once it is in: release 2.)
    if (f.stance === 'passage') continue;
    if (!g.nations[nat].atWar.length) continue;
    const job = jobWords(g, f.task);
    if (atSea(f) && f.ready < TASKFORCE.refit) {
      if (sail(g, f, f.home, 'port')) headline(g, 'info', `Task force ${f.name} comes into ${f.port} to refit. It goes back to its job, it ${job}, once it is fit for sea.`, nat);
      continue;
    }
    if (onJob(f) || f.ready < TASKFORCE.back) continue;
    const back = f.task.stance === 'escort' ? escort(g, f, f.task.lane ?? '') : sail(g, f, f.task.zone, f.task.stance);
    if (back) headline(g, 'info', `Task force ${f.name} is back on its job: it ${job}.`, nat);
  }
}

/** The line for a task force's page and panel: its job, and where it stands with it. Empty for any other fleet. */
export function taskLine(g: Game, f: Fleet): string {
  if (!taskForceOn(g) || !f.task) return '';
  const job = jobWords(g, f.task);
  if (onJob(f)) return `Task force: it ${job}. It comes into harbour to refit when worn, and goes back when fit.`;
  if (atSea(f)) return `Task force: it ${job}, and goes back to that next month.`;
  const why = whyNotBack(g, f);
  if (why) return `Task force: it ${job}, but cannot go back to it now. ${why}`;
  return f.ready < TASKFORCE.back
    ? `Task force refitting: it ${job}, and goes back at ${Math.round(TASKFORCE.back * 100)}% readiness (now ${Math.round(f.ready * 100)}%).`
    : `Task force: it ${job}, and goes back to it next month.`;
}
