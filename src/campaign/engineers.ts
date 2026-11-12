// Engineers: the corps that builds, repairs, mines and breaches.
//
// An engineer corps is raised like any other in the grand campaign. Given a month where it stands, it rebuilds
// the bridges that are down on that province's rivers, relays a railway that has been cut, puts a bombed factory
// back to work, raises field works, lays minefields and booby traps, or demolishes fortifications. Sent in with
// an assault, it breaches the enemy's works, clears lanes through his mines and gets infantry over a river whose
// bridges are down. Nothing that is broken mends itself: without engineers on the spot a blown bridge stays
// blown, a cut line stays cut and a wrecked factory stays a ruin.
//
// Both interfaces offer the same jobs from jobsFor() and give them with orderJob().

import arsenalData from '../../data/campaign/arsenal.json';
import { STACK, type Corps, type Game, type Region } from './types';
import { groundRules } from './ground';
import { ENGINEERING, bridgeDown, crossingsAt, downBetween, farBank, rebuildAt, riverName, type Crossing } from './rivers';
import { addCorps, allied, atWar, ensureSupply, friendlyUnits, hasRoom, headline, hostileUnits, moveCorps, railMove, railReach } from './game';
import { orderCorps, raiseSite } from './economy';
import { mendRail, onNet, railCut, railheads, railsAt, rebuildFactory, ruinedAt } from './rails';

export type JobKind = 'bridge' | 'rail' | 'plant' | 'works' | 'mines' | 'demolish';
export const JOBS: JobKind[] = ['bridge', 'rail', 'plant', 'works', 'mines', 'demolish'];
/** Repairs: one corps to a job in a province, since two cannot relay the same track. */
const REPAIRS: Partial<Record<JobKind, string>> = { bridge: 'rebuilding the bridges', rail: 'relaying the railway', plant: 'rebuilding a factory' };

/** One thing an engineer corps could do where it stands, and the reason if it cannot start it now. */
export interface JobOffer { key: JobKind; label: string; blurb: string; why: string | null }

export const isEngineer = (c: Corps): boolean => c.kind === 'eng';

const mineKey = (r: Region | number): string => `mine:${typeof r === 'number' ? r : r.id}`;
/** How thick the minefields round a region are: 0 to 2. Always 0 outside the grand campaign. */
export const minesAt = (g: Game, r: Region): number => (groundRules(g) ? g.flags[mineKey(r)] ?? 0 : 0);
/** Mines are lifted when the ground they guard is lost. */
export function liftMines(g: Game, r: Region): void {
  delete g.flags[mineKey(r)];
}

/** The weight of engineers in a force: its engineer divisions at the strength they stand at. */
export const sappers = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.kind === 'eng' && !c.dead ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);

function whyNot(g: Game, c: Corps, key: JobKind): string | null {
  const r = g.regions[c.reg];
  const E = ENGINEERING;
  if (c.job) return `${c.name} already has work in hand.`;
  if (!allied(g, c.nat, r.ctrl)) return `${r.name} is not in friendly hands.`;
  if (key === 'bridge' && !crossingsAt(r.id).some((x) => bridgeDown(g, x))) return crossingsAt(r.id).length ? 'Every bridge here is standing.' : 'No great river runs past this province.';
  if (key === 'rail' && !railCut(g, r.id)) return onNet(r.id) ? 'The railway here is working.' : 'No railway runs through this province.';
  if (key === 'plant' && !ruinedAt(g, r)) return r.fac > 0 ? 'Every factory here is at work.' : 'There are no factories here.';
  const other = REPAIRS[key] && (key !== 'plant' || ruinedAt(g, r) <= 1) ? r.units.find((u) => u !== c && !u.dead && u.job?.what === key && u.job.reg === r.id) : undefined;
  if (other) return `${other.name} is already ${REPAIRS[key]} here.`;
  if (key === 'works' && (r.fort?.lv ?? 0) >= E.works.most) return `The works here are at level ${r.fort?.lv ?? 0}: as strong as field engineers can make them. Heavier fortifications are ordered from the treasury.`;
  if (key === 'mines' && minesAt(g, r) >= E.mines.most) return 'The minefields here are as thick as they can be laid.';
  if (key === 'demolish' && !r.fort) return 'There are no fortifications here to demolish.';
  return null;
}

/** What this engineer corps could do where it stands. Empty for any other corps, and outside the grand campaign. */
export function jobsFor(g: Game, c: Corps): JobOffer[] {
  if (!groundRules(g) || !isEngineer(c) || c.dead) return [];
  return JOBS.map((key) => ({ key, label: ENGINEERING.jobs[key].label, blurb: ENGINEERING.jobs[key].blurb, why: whyNot(g, c, key) }));
}

/** Sets an engineer corps to work. It spends the month on it, and the work is done when the nation's next month begins. */
export function orderJob(g: Game, c: Corps, key: JobKind): boolean {
  if (!groundRules(g) || !isEngineer(c) || c.dead || whyNot(g, c, key)) return false;
  c.job = { what: key, reg: c.reg, eta: g.turn + 1, ap: c.ap };
  c.ap = 0;
  return true;
}

/** Takes an engineer corps off its work in the month the order was given: it has its moves back. */
export function cancelJob(g: Game, c: Corps): boolean {
  if (!c.job || c.job.eta !== g.turn + 1 || c.job.reg !== c.reg) return false;
  c.ap = c.job.ap ?? c.apMax;
  delete c.job;
  return true;
}

/** What the work in hand is, in a few words. */
export function jobLine(g: Game, c: Corps): string {
  if (!c.job) return '';
  const r = g.regions[c.job.reg];
  const what = c.job.what === 'bridge' ? `rebuilding the bridges at ${r.name}` : c.job.what === 'rail' ? `relaying the railway at ${r.name}` : c.job.what === 'plant' ? `rebuilding a factory at ${r.name}`
    : c.job.what === 'works' ? `building field works at ${r.name}` : c.job.what === 'mines' ? `laying minefields round ${r.name}` : `demolishing the works at ${r.name}`;
  return `${what[0].toUpperCase()}${what.slice(1)}: done when next month begins`;
}

/** The nations whose ground borders a region, and its own: field works face all of them. */
function facing(g: Game, r: Region): string[] {
  const vs = new Set(r.fort?.vs ?? []);
  vs.add(r.owner);
  for (const j of r.nb) { vs.add(g.regions[j].owner); vs.add(g.regions[j].ctrl); }
  return [...vs];
}

/** The start of a nation's month: work its engineers finished is handed over. A corps driven off its work has done nothing. */
export function engineersMonth(g: Game, nat: string): void {
  if (!groundRules(g)) return;
  for (const c of g.corps) {
    if (c.nat !== nat || !c.job) continue;
    const job = c.job;
    if (c.dead || c.reg !== job.reg || !allied(g, nat, g.regions[job.reg].ctrl)) { delete c.job; continue; }
    if (g.turn < job.eta) continue;
    delete c.job;
    const r = g.regions[job.reg];
    const mine = nat === g.human;
    if (job.what === 'bridge') {
      const done = rebuildAt(g, r.id);
      if (done.length && mine) headline(g, 'info', `${c.name} has rebuilt the ${done.length > 1 ? 'bridges' : `bridges over the ${riverName(done[0])}`} at ${r.name}. Tanks, lorries and guns can cross again.`, nat, r.id);
    } else if (job.what === 'rail') {
      if (mendRail(g, r.id) && mine) headline(g, 'info', `${c.name} has relaid the railway at ${r.name}. The trains run again.`, nat, r.id);
    } else if (job.what === 'plant') {
      if (rebuildFactory(g, r) && mine) { const left = ruinedAt(g, r); headline(g, 'info', `${c.name} has put a bombed factory at ${r.name} back to work${left ? `: ${left} still ${left > 1 ? 'lie' : 'lies'} in ruins` : ''}.`, nat, r.id); }
    } else if (job.what === 'works') {
      if ((r.fort?.lv ?? 0) < ENGINEERING.works.most) {
        r.fort = { lv: (r.fort?.lv ?? 0) + 1, vs: facing(g, r) };
        if (mine) headline(g, 'info', `${c.name} has finished field works at ${r.name}: fortifications, level ${r.fort.lv}.`, nat, r.id);
      }
    } else if (job.what === 'mines') {
      const lv = Math.min(ENGINEERING.mines.most, minesAt(g, r) + 1);
      g.flags[mineKey(r)] = lv;
      if (mine) headline(g, 'info', `${c.name} has laid minefields and booby traps round ${r.name}${lv > 1 ? ', two belts deep' : ''}.`, nat, r.id);
    } else if (job.what === 'demolish' && r.fort) {
      r.fort = r.fort.lv <= 1 ? null : { lv: r.fort.lv - 1, vs: r.fort.vs };
      if (mine) headline(g, 'info', `${c.name} has demolished ${r.fort ? 'part of the fortifications' : 'the fortifications'} at ${r.name}.`, nat, r.id);
    }
  }
}

const ORD = ['1st', '2nd', '3rd', '4th'];

/** The engineers each great power takes into the grand campaign. Done once, when free play begins. */
export function startEngineers(g: Game): void {
  if (!groundRules(g) || g.flags.sappers) return;
  g.flags.sappers = 1;
  for (const nat of ENGINEERING.start.nations) {
    const n = g.nations[nat];
    if (!n || !n.alive || n.capital < 0 || !allied(g, nat, g.regions[n.capital].ctrl)) continue;
    for (let k = 0; k < ENGINEERING.start.corps; k++) {
      const cap = g.regions[n.capital];
      // At the capital if there is room, else the nearest home province with any.
      const at = [cap, ...cap.nb.map((j) => g.regions[j])].find((r) => r.ctrl === nat && r.owner === nat && friendlyUnits(g, r, nat).length < STACK);
      if (!at) break;
      addCorps(g, nat, `${ORD[k] ?? `${k + 1}th`} Engineer Corps`, 'eng', (arsenalData.land as Record<string, { divs: number }>).eng.divs, at.id, {});
    }
  }
}

/** A bank where bridging would help this nation: it holds the bank, and the far side is its own or its enemy's. */
/** True while this nation's staff has an offensive under way against that enemy (the staff's own mark, see staff.ts). */
const marching = (g: Game, nat: string, enemy: string): boolean => (g.flags[`st:on:${nat}:${enemy}`] ?? 0) > 0;

/** Repair work this nation wants done in a province, and how much it matters. For bridges, the crossings in question. */
interface Site { reg: number; score: number; what: JobKind; spans: Crossing[] }

function wanted(g: Game, nat: string): Site[] {
  const out: Site[] = [];
  // The railheads are only counted when some line of this nation's is cut.
  let head: Int16Array | null = null;
  if (g.regions.some((r) => railCut(g, r.id) && allied(g, nat, r.ctrl))) { ensureSupply(g); head = railheads(g, nat); }
  const troops = (r: Region): boolean => r.units.some((u) => u.nat === nat && u.kind !== 'eng');
  for (const r of g.regions) {
    if (!allied(g, nat, r.ctrl)) continue;
    const site: Site = { reg: r.id, score: 0, what: 'bridge', spans: [] };
    const own = troops(r);
    for (const x of crossingsAt(r.id)) {
      if (!bridgeDown(g, x)) continue;
      const far = g.regions[farBank(x, r.id)];
      // A road of its own that is cut, or a bank where an army on the offensive is waiting. A defence leaves the bridges in front of it down.
      const score = allied(g, nat, far.ctrl) ? (own ? 3 : 2) : atWar(g, nat, far.ctrl) && own && marching(g, nat, far.ctrl) ? 4 : 0;
      if (!score) continue;
      site.spans.push(x);
      site.score = Math.max(site.score, score);
    }
    if (site.spans.length) out.push(site);
    // A cut line at the end of the working net: relaying it brings the railhead a province forward. One further
    // out waits its turn. It matters most where the army stands on it or next to it.
    if (head && railCut(g, r.id) && (head[r.id] === 0 || railsAt(r.id).some((j) => head[j] === 0 && !railCut(g, j) && !downBetween(g, r.id, j)))) {
      out.push({ reg: r.id, score: own || r.nb.some((j) => troops(g.regions[j])) ? 3 : 2, what: 'rail', spans: [] });
    }
    // A factory of its own that bombing has left in ruins.
    if (r.ctrl === nat && ruinedAt(g, r)) out.push({ reg: r.id, score: 1, what: 'plant', spans: [] });
  }
  return out.sort((p, q) => q.score - p.score || p.reg - q.reg);
}

/** Marches a corps by road toward the nearest of these regions through friendly country. Returns the one it is making for, or -1. */
function march(g: Game, c: Corps, goals: Set<number>): number {
  const prev = new Map<number, number>([[c.reg, -1]]);
  const q = [c.reg];
  let goal = -1;
  for (let i = 0; i < q.length && goal < 0; i++) {
    for (const j of g.regions[q[i]].nb) {
      const r = g.regions[j];
      if (prev.has(j) || !allied(g, c.nat, r.ctrl) || hostileUnits(g, r, c.nat).length) continue;
      prev.set(j, q[i]);
      if (goals.has(j)) { goal = j; break; }
      q.push(j);
    }
  }
  if (goal < 0) return -1;
  const way: number[] = [];
  for (let at = goal; at !== c.reg; at = prev.get(at)!) way.unshift(at);
  for (const j of way) if (!moveCorps(g, c, g.regions[j])) break;
  return goal;
}

/**
 * A computer nation's engineers: to the repairs it wants most (one corps to a crossing, a cut line or a ruined
 * factory), by rail or by road; with none to see to, they dig in and mine where they stand next to the enemy.
 * The staff leaves them alone.
 */
export function aiEngineers(g: Game, nat: string): void {
  if (!groundRules(g) || nat === g.human || !g.nations[nat].alive) return;
  const mine = g.corps.filter((c) => c.nat === nat && c.kind === 'eng' && !c.dead);
  const sites = wanted(g, nat);
  // The crossings somebody is already rebuilding, from either bank, and the other repairs already in hand.
  const covered = new Set<Crossing>();
  const taken = new Set<string>();
  const cover = (reg: number, what: JobKind): void => {
    if (what === 'bridge') { for (const x of crossingsAt(reg)) if (bridgeDown(g, x)) covered.add(x); } else taken.add(`${what}:${reg}`);
  };
  const open = (s: Site): boolean => (s.what === 'bridge' ? s.spans.some((x) => !covered.has(x)) : !taken.has(`${s.what}:${s.reg}`));
  for (const c of mine) if (c.job && REPAIRS[c.job.what as JobKind]) cover(c.job.reg, c.job.what as JobKind);
  for (const c of mine) {
    c.hold = true;
    if (c.job) continue;
    const here = sites.find((s) => s.reg === c.reg && open(s));
    if (here && orderJob(g, c, here.what)) { cover(c.reg, here.what); continue; }
    if (c.ap > 0) {
      const todo = sites.filter((s) => open(s) && hasRoom(g, g.regions[s.reg], c));
      if (todo.length) {
        // By rail to where the work is, and to work on the day they arrive. Where no railway runs yet, by road
        // toward the nearest of it.
        const reach = railReach(g, c);
        const site = todo.find((s) => reach.has(s.reg));
        const goal = site && railMove(g, c, g.regions[site.reg]) ? site.reg : march(g, c, new Set(todo.map((s) => s.reg)));
        if (goal >= 0) {
          const job = (site && site.reg === goal ? site : todo.find((s) => s.reg === goal))!;
          cover(goal, job.what);
          if (c.reg === goal) orderJob(g, c, job.what);
          continue;
        }
      }
    }
    // With no repairs to see to, engineers who stand next to the enemy dig in, then mine the roads.
    const at = g.regions[c.reg];
    if (at.ctrl === nat && at.nb.some((j) => hostileUnits(g, g.regions[j], nat).length > 0)) {
      if (!orderJob(g, c, 'works')) orderJob(g, c, 'mines');
    }
  }
  // More repairs waiting than engineers to see to them: another corps is raised, as far as the treasury allows.
  const n = g.nations[nat];
  const spec = (arsenalData.land as Record<string, { cost: number }>).eng;
  const coming = n.queue.filter((b) => b.what === 'corps' && b.kind === 'eng').length;
  const idle = mine.filter((c) => !c.job).length;
  const waiting = new Set(sites.filter((s) => s.what === 'bridge').flatMap((s) => s.spans).filter((x) => !covered.has(x))).size + sites.filter((s) => s.what !== 'bridge' && open(s)).length;
  if (waiting > idle + coming && mine.length + coming < ENGINEERING.ai.most && n.pp >= spec.cost + ENGINEERING.ai.reserve) {
    const at = raiseSite(g, nat, 'eng', n.capital);
    if (at >= 0) orderCorps(g, nat, 'eng', at);
  }
}
/**
 * A computer nation on the defensive blows the bridges in front of it: where the enemy across a river is much
 * stronger than its own troops on the bank.
 */
export function aiBridges(g: Game, nat: string, blowIt: (c: Crossing, from: number) => boolean): void {
  if (!groundRules(g) || nat === g.human || !g.nations[nat].alive) return;
  const weight = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    const own = r.units.filter((u) => u.nat === nat);
    if (!own.length) continue;
    for (const x of crossingsAt(r.id)) {
      if (bridgeDown(g, x)) continue;
      const far = g.regions[farBank(x, r.id)];
      // An army on the offensive keeps the bridges it means to cross.
      if (!atWar(g, nat, far.ctrl) || marching(g, nat, far.ctrl)) continue;
      const foe = hostileUnits(g, far, nat);
      if (foe.length && weight(foe) >= ENGINEERING.blow.aiOdds * weight(friendlyUnits(g, r, nat))) blowIt(x, r.id);
    }
  }
}
