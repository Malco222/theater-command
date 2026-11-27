// Radio intercepts (release 2, words only).
//
// Brian, 27 November 2026 17:10Z, yes to the idea: "near a month's end you sometimes overhear an enemy order, which may
// be true or a trick". The design director's ruling of 17:41Z (r2-three-ideas/FORM-worn-hospitals-intercepts-1127.md),
// every part a fact of the war and never a draw:
//
// 1. What is heard: an enemy staff naming a main effort against the player or an ally (staff.ts effortNamed, the
//    month it is named). The order is real: its target is the staff's own (`first`), and so is its last month (`to`).
//    It falls in the enemy's month, just before the player's next month opens.
// 2. Never twice: where the player already reads that staff's plans (staff.ts plansKnown: an ally, or an agent on its
//    books), he is told by that, and there is no intercept.
// 3. When it is heard: the player's front touches the sector (he holds the target or a province next to it, or a corps
//    of his stands there) AND the Intelligence branch rows he has built outnumber that enemy's counter-intelligence
//    rows (tech.ts: the rows of branch 'intel'; against them the rows whose 'spy' adds 'guard'). What a nation built,
//    never a national trait.
// 4. A trick only where the enemy has the means and the cause to deceive: it runs one of the player's agents, turned
//    before this month (intel.ts, the double agents), and the player has no double agent of his own in its service
//    (as the books' lie, intel.ts booksLie). Then every intercept from that enemy names a false place: of the provinces
//    the player and his allies hold on the line facing that enemy, the one furthest from the true target. And it
//    carries its telltale, which a true one never does: the signal came through unusually clear.
// 5. It writes one news line and nothing else: it binds no staff, throws no dice, and no rule reads it.
//
// With the switch off nothing here runs.

import { allied, atWar, headline } from './game';
import { agentsOf, doubleIn } from './intel';
import { doublesOn } from './secretwar';
import { techById, techOf } from './tech';
import { effortMonths, type Effort } from './staff';
import type { Game } from './types';

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const interceptsOn = (g: Game): boolean => g.flags.intercepts === 1;

/** Turns the switch on or off at any time. It keeps nothing but the news it has told. */
export function setIntercepts(g: Game, on: boolean): boolean {
  if (on) {
    if (g.flags.open !== 1) return false;
    g.flags.intercepts = 1;
    return true;
  }
  delete g.flags.intercepts;
  return true;
}

/** The Intelligence branch rows a nation has built. */
export const intelRows = (g: Game, nat: string): number => (techOf(g, nat)?.done ?? []).filter((id) => techById(id)?.branch === 'intel').length;

/** The counter-intelligence rows a nation has built: those that strengthen its guard against agents. */
export const counterRows = (g: Game, nat: string): number => (techOf(g, nat)?.done ?? []).filter((id) => (techById(id)?.spy?.guard ?? 0) > 0).length;

/** True where the viewer's front touches a province: he holds it or one next to it, or a corps of his stands there. */
function touches(g: Game, viewer: string, region: number): boolean {
  const r = g.regions[region];
  const mine = (j: number): boolean => g.regions[j].ctrl === viewer || g.corps.some((c) => c.reg === j && c.nat === viewer && !c.dead);
  return mine(r.id) || r.nb.some(mine);
}

/** True where this enemy deceives the viewer by radio: it runs an agent of his it turned before this month, and he has no double in its service. */
export function deceives(g: Game, of: string, viewer: string): boolean {
  if (!doublesOn(g)) return false;
  return agentsOf(g, viewer).some((a) => a.turned !== undefined && a.turnedFor === of && a.turned < g.turn) && doubleIn(g, of, viewer) === null;
}

/** The false place a deceiving enemy's traffic names: on the line the viewer's side holds against it, the province furthest from the true target. */
function falsePlace(g: Game, of: string, viewer: string, target: number): number | null {
  const line = g.regions.filter((r) => (r.ctrl === viewer || allied(g, viewer, r.ctrl)) && r.nb.some((j) => g.regions[j].ctrl === of)).map((r) => r.id);
  if (!line.length) return null;
  // Steps from the true target over the map, the target's own province 0.
  const far = new Int16Array(g.regions.length).fill(-1);
  far[target] = 0;
  const q = [target];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (far[j] < 0) { far[j] = far[q[i]] + 1; q.push(j); }
  let best = -1, at: number | null = null;
  for (const j of line) if (j !== target && far[j] > best) { best = far[j]; at = j; }
  return at;
}

/** What the player hears of a main effort named against him or an ally, where he does not read its staff's plans. Called by staff.ts effortNamed. */
export function interceptEffort(g: Game, e: Effort): void {
  const me = g.human;
  if (!interceptsOn(g) || me === null || !g.nations[me]?.alive || e.nat === me || !atWar(g, me, e.nat)) return;
  if (!(e.enemy === me || allied(g, me, e.enemy))) return;
  if (!touches(g, me, e.first)) return;
  if (intelRows(g, me) <= counterRows(g, e.nat)) return;
  const trick = deceives(g, e.nat, me);
  const place = trick ? falsePlace(g, e.nat, me, e.first) : e.first;
  if (place === null) return;
  // (The UI director, 18:11Z: the line tells what the signal says, never what the staff's orders are, so it stays true
  // of a trick; the months are the effort's own, read as the agent's line reads them, never "this month".)
  const m = effortMonths(e);
  const who = g.nations[e.nat].adj;
  headline(g, 'event', `Intercepted: a signal from the ${who} staff orders its army to mass opposite ${g.regions[place].name} in ${m.mass} and attack there from ${m.strike} through ${m.last}.${trick ? ' The signal came through unusually clear.' : ''}`, e.nat, place);
}
