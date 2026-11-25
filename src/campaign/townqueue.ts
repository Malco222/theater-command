// A queue for a town (release 2; Brian 25 November 2026 13:41Z: "lets make the campaign map and playing of that feel
// less choppy and hard to manage ... easy to manage the cities, recruitment, ect"; the design director's ruling,
// seamless-r2-1125.md part B). The player lines up what a town builds next, the way a site foreman keeps a list of jobs
// pinned by the door: when one job is done, the next on the list starts, and nobody has to walk over and say so.
//
// A queued building is a press of BUILD made for the player at the opening of his month, straight after his staff's
// month, and nothing more. It makes every check BUILD makes (buildings.ts whyNotCivic) and pays BUILD's price on that
// day; no price is fixed when it is queued. The presses go oldest first across the country. One that the treasury
// cannot pay for waits at the head of its town's queue, and the presses go on to the next town; next month it is
// pressed again in its place. One refused for any other reason comes off the queue with a line saying why, and the
// town's next is tried. A town that is lost takes its queue with it; nothing was paid. The staff never touches a
// player's queue, and builds only in towns where nothing is queued (civicAI). It gives no building the player could
// not have made by hand that month, at the same price and the same speed: it saves the visit, nothing else.
//
// Its own switch, the flag 'townQueue' (OPTIONS, any month): off, nothing of it is read or written, and the war is the
// war without it. Its one saved field, Game.townQueue, is written only while something is queued.

import { BUDGET } from './economy';
import { civicAt, civicGoing, civicLevel, civicLine, civicRules, orderCivic, whyNotCivic, type CivicAhead } from './buildings';
import { headline } from './game';
import { townName } from './towns';
import type { Game } from './types';

/** One queued building: the province, the line of building, and the level it is to be raised to. */
export type Queued = NonNullable<Game['townQueue']>[string][number];

/** True where the player can queue buildings for his towns: the grand campaign, with the switch on. */
export const townQueueOn = (g: Game): boolean => civicRules(g) && g.flags.townQueue === 1;

/** Turns the queue on or off in a campaign under way. Off, every queue is gone; nothing on one was paid for. */
export function setTownQueue(g: Game, on: boolean): void {
  if (on) g.flags.townQueue = 1;
  else {
    delete g.flags.townQueue;
    delete g.townQueue;
  }
}

/** This nation's whole queue, oldest first (empty where there is none). */
export const queueOf = (g: Game, nat: string): Queued[] => (townQueueOn(g) ? g.townQueue?.[nat] ?? [] : []);

/** What is queued in one town, in its order. */
export const queuedAt = (g: Game, nat: string, region: number): Queued[] => queueOf(g, nat).filter((q) => q.region === region);

/** "the merchant quarter at Lyon": a queued building as the news names it. */
export function queuedName(g: Game, q: Queued): string {
  const l = civicLine(q.line), r = g.regions[q.region];
  return l && r ? `the ${l.levels[Math.max(0, q.level - 1)].toLowerCase()} at ${townName(r)}` : 'a building';
}

/** What is spoken for in a town beyond what stands: the building going up there and everything queued. */
function aheadAt(g: Game, nat: string, region: number): CivicAhead {
  const level: Record<string, number> = {};
  const lines = new Set(civicAt(g, region).map((x) => x.line.id));
  const going = civicGoing(g, nat, region);
  if (going?.item) {
    level[going.item] = Math.max(level[going.item] ?? 0, civicLevel(g, going.item, region) + 1);
    lines.add(going.item);
  }
  for (const q of queuedAt(g, nat, region)) {
    level[q.line] = Math.max(level[q.line] ?? 0, q.level);
    lines.add(q.line);
  }
  return { level, plots: lines.size };
}

/**
 * Why this building cannot be queued in this town, or null. Every check BUILD makes, counted as the town will stand
 * once everything ahead of it is built, so a queue is never longer than the town has room for. Money is no reason:
 * an item the treasury cannot pay for waits. A building that BUILD could put up now is not queued: BUILD does it.
 */
export function whyNotQueue(g: Game, nat: string, region: number, line: string): string | null {
  if (!townQueueOn(g)) return 'Building queues are switched off. They can be switched on in OPTIONS.';
  const r = g.regions[region], n = g.nations[nat], l = civicLine(line);
  if (!r || !n || !l) return 'Nothing can be queued there.';
  const why = whyNotCivic(g, nat, r, line, aheadAt(g, nat, region));
  if (why) return why;
  const level = nextQueued(g, nat, region, line);
  if (!civicGoing(g, nat, region) && !queuedAt(g, nat, region).length && level === civicLevel(g, line, region) + 1 && n.pp >= l.cost[level - 1]) {
    return 'The builders are free and the treasury can pay: BUILD puts it up now.';
  }
  return null;
}

/** The level a building queued now in this town would raise its line to. */
export function nextQueued(g: Game, nat: string, region: number, line: string): number {
  const a = aheadAt(g, nat, region);
  return Math.max(civicLevel(g, line, region), a.level[line] ?? 0) + 1;
}

/** Queues the next level of a line of building in one of the player's towns, behind everything queued there. Nothing is paid. */
export function addQueue(g: Game, nat: string, region: number, line: string): Queued | null {
  if (whyNotQueue(g, nat, region, line) !== null) return null;
  const q: Queued = { region, line, level: nextQueued(g, nat, region, line) };
  g.townQueue ??= {};
  (g.townQueue[nat] ??= []).push(q);
  return q;
}

/** Writes a nation's queue back, leaving nothing behind where it is empty. */
function keep(g: Game, nat: string, list: Queued[]): void {
  if (list.length) (g.townQueue ??= {})[nat] = list;
  else if (g.townQueue) {
    delete g.townQueue[nat];
    if (!Object.keys(g.townQueue).length) delete g.townQueue;
  }
}

/**
 * Takes a building off a town's queue (its place in that town's queue, from 0), and everything queued behind it in
 * the same line that needs it. Nothing was paid, so nothing is returned.
 */
export function dropQueue(g: Game, nat: string, region: number, at: number): boolean {
  const all = queueOf(g, nat);
  const mine = all.filter((q) => q.region === region);
  const gone = mine[at];
  if (!gone) return false;
  keep(g, nat, all.filter((q) => q !== gone && !(q.region === region && q.line === gone.line && q.level > gone.level)));
  return true;
}

/** Money as the player sees it, in whole numbers (a production point is BUDGET.scale). */
const money = (pp: number): number => Math.floor(pp * BUDGET.scale + 1e-6);

/** What one town's queue is doing now: going up behind the builders, waiting for money (and how much more), or ready for its next press. */
export interface QueueState { q: Queued; name: string; cost: number; short: number; after: string }

/**
 * Each town's first queued building and how it stands, for the board and the towns chip: `short` is what the
 * treasury lacks for it (0 when it could be paid), `after` the building going up ahead of it ('' when the builders are
 * free). Display only.
 */
export function queueHeads(g: Game, nat: string): QueueState[] {
  const n = g.nations[nat];
  if (!n) return [];
  const out: QueueState[] = [], seen = new Set<number>();
  for (const q of queueOf(g, nat)) {
    if (seen.has(q.region)) continue;
    seen.add(q.region);
    const l = civicLine(q.line), going = civicGoing(g, nat, q.region);
    const cost = l ? l.cost[Math.min(l.cost.length, civicLevel(g, q.line, q.region) + 1) - 1] : 0;
    out.push({ q, name: queuedName(g, q), cost: Math.round(cost * BUDGET.scale), short: going ? 0 : Math.max(0, Math.round(cost * BUDGET.scale) - money(n.pp)), after: going ? going.name.split(' at ')[0].toLowerCase() : '' });
  }
  return out;
}

/**
 * The presses, at the opening of the player's month, straight after his staff's month (turn.ts beginHuman). Oldest
 * first across the country; in each town only the first that can go is pressed, since a town's builders put up one
 * building at a time. Writes one line of news when anything was begun, waits or came off a queue.
 */
export function pressQueue(g: Game, nat: string | null = g.human): void {
  if (!nat || !townQueueOn(g)) return;
  const n = g.nations[nat], list = g.townQueue?.[nat];
  if (!n?.alive || !list?.length) return;
  const done = new Set<number>(), lost = new Set<number>(), out = new Set<Queued>();
  const begun: string[] = [], dropped: string[] = [], gone: string[] = [];
  const waits: { name: string; cost: number }[] = [];
  for (const q of list) {
    if (done.has(q.region) || lost.has(q.region)) continue;
    const r = g.regions[q.region], l = civicLine(q.line);
    // A town that is lost takes its queue with it: nothing was paid for any of it.
    if (!r || r.ctrl !== nat) {
      lost.add(q.region);
      const k = list.filter((x) => x.region === q.region).length;
      gone.push(`${r ? townName(r) : 'A town'} is lost, and ${k === 1 ? 'the building' : `the ${k} buildings`} queued there with it: nothing had been paid.`);
      continue;
    }
    const now = civicLevel(g, q.line, q.region);
    // Already standing (he built it by hand): it comes off, and the town's next is tried.
    if (!l || now >= q.level) { out.add(q); continue; }
    // The builders are at work there: it is pressed at the opening after their building is done.
    if (civicGoing(g, nat, q.region)) { done.add(q.region); continue; }
    const why = whyNotCivic(g, nat, r, q.line);
    if (why) {
      out.add(q);
      dropped.push(`${upFirst(queuedName(g, q))} comes off the queue. ${why}`);
      continue;
    }
    done.add(q.region);
    const cost = l.cost[now];
    if (n.pp < cost) {
      waits.push({ name: queuedName(g, { ...q, level: now + 1 }), cost });
      continue;
    }
    if (!orderCivic(g, nat, q.region, q.line)) continue;
    begun.push(queuedName(g, { ...q, level: now + 1 }));
    if (now + 1 >= q.level) out.add(q);
  }
  keep(g, nat, list.filter((q) => !out.has(q) && !lost.has(q.region)));
  const said = [
    begun.length ? `From your queue, the builders begin ${listAnd(begun)}.` : '',
    // What each waiting building lacks, from the treasury as the presses left it: the figure the board shows.
    ...waits.map((w) => `${upFirst(w.name)} waits for ${Math.max(1, Math.round(w.cost * BUDGET.scale) - money(n.pp)).toLocaleString('en-US')} more.`),
    ...dropped, ...gone,
  ].filter(Boolean);
  if (said.length) headline(g, 'info', said.join(' '), nat);
}

const upFirst = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);
const listAnd = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
