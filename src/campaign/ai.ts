// The computer's generals. In Historical mode each nation follows the intentions in
// data/campaign/plan-historical.json: who to attack, when, and toward which objectives.
// The plan only sets intentions. Every region still has to be won under the same rules
// the player uses, so history can come out differently.

import plan from '../../data/campaign/plan-historical.json';
import theatreData from '../../data/campaign/theatres.json';
import { airAI } from './air';
import { aiProduction } from './economy';
import { landCorps, landingOpts, navalAI } from './navy';
import { afterDeclaring, decide, isOpen, staffPhase, theatreStaff } from './staff';
import { KIND, STACK, TERRAIN_DEF, type Corps, type Game, type PlanPhase, type Region } from './types';
import {
  allied, assess, atWar, collapseScore, corpsBase, declareWar, ensureSupply, evacuate, friendlyUnits, frontage, havensOf,
  hostileUnits, limitAttackers, moveCorps, moveCost, railMove, resolveBattle, shipMove, weekOf,
} from './game';

const PLAN = plan as unknown as Record<string, PlanPhase[] | string>;

function phases(nat: string): PlanPhase[] {
  const p = PLAN[nat];
  return Array.isArray(p) ? p : [];
}

/** A war fought overseas to its own plan. See data/campaign/theatres.json. */
interface Theatre { name: string; regions: string[]; plans: Record<string, PlanPhase[]>; bases?: Record<string, string>; open?: boolean; opens?: number }
const THEATRES = Object.entries(theatreData as unknown as Record<string, Theatre | string>).filter((e): e is [string, Theatre] => typeof e[1] !== 'string').map((e) => e[1]);
const theatreIds = new WeakMap<Game, Set<number>[]>();

/** The regions of each theatre, by number. */
function theatreRegions(g: Game): Set<number>[] {
  let ids = theatreIds.get(g);
  if (!ids) theatreIds.set(g, (ids = THEATRES.map((t) => new Set(t.regions.map((nm) => g.byName[nm]?.id ?? -1)))));
  return ids;
}

/**
 * True where a nation's troops in a theatre are an army apart, run by that theatre's plan and not the main one.
 * In Historical mode that is wherever history gave the nation a plan there. In free play only a theatre across the
 * sea keeps an army staff of its own, for the nations with a base there: wars on the Continent are the main staff's.
 */
const apart = (g: Game, k: number, nat: string): boolean => (isOpen(g) ? !!THEATRES[k].open && !!(THEATRES[k].plans[nat] || THEATRES[k].bases?.[nat]) : !!THEATRES[k].plans[nat] && begun(g, k));
/** A theatre with an opening month ('opens') is part of the main fronts until that month comes. */
const begun = (g: Game, k: number): boolean => g.turn >= (THEATRES[k].opens ?? 0);

/** The theatre a corps is fighting in, if its nation has a plan of its own there. */
function theatreOf(g: Game, c: Corps): number {
  const ids = theatreRegions(g);
  for (let k = 0; k < THEATRES.length; k++) if (apart(g, k, c.nat) && ids[k].has(c.reg)) return k;
  return -1;
}

/** While a theatre's army is being run, the regions that are its business. Null while the main plan runs. */
let scope: Set<number> | null = null;
/** True while a plan has told the army being run to make no local counter-attacks. */
let quiet = false;
/** True while the plan in force allows each place one assault a month: what fails is not tried again until the next. */
let single = false;
/** Set while the army being run has orders to leave the theatre once the front there has gone. */
let leaving: PlanPhase['withdraw'] | null = null;
/** Sectors the plan in force has handed over to an ally whose troops have arrived: the nation's own corps leave them, all but one. */
let entrusted = new Set<number>();
/** Enemy garrisons the theatre plan leaves to others: nobody attacks them, closes up to them or stays to watch them. */
let masked = new Set<number>();
/** Corps the plan in force keeps massed at a named place: the line is put in order without them. */
let massed = new Set<number>();
/** Ground the plan in force is giving up: the corps there march out and nobody is sent back in. */
let given = new Set<number>();

/** True for ground in a side theatre where this nation has a plan of its own: the army there looks after it, not the main plan. */
const aside = (g: Game, r: Region, nat: string): boolean => r.theatre >= 0 && !!THEATRES[r.theatre] && apart(g, r.theatre, nat);
/** An army run by a theatre's plan stays on that theatre's ground: it neither marches out of it nor takes a short cut round it. */
const within = (r: Region): boolean => !scope || scope.has(r.id);

/** Months after losing a region in which the local commanders will still counter-attack it on their own. */
const COUNTER_WINDOW = 2;

/** The phase of a theatre's plan in force this month: the first in the list whose months and conditions fit. */
function theatrePhase(g: Game, nat: string, list: PlanPhase[]): PlanPhase | null {
  const ours = (nm: string): boolean => { const r = g.byName[nm]; return !!r && allied(g, nat, r.ctrl); };
  const ph = list.find((p) => g.turn >= p.from && g.turn <= p.to && (p.ifLost ?? []).every((nm) => !ours(nm))) ?? null;
  // A phase that waits on someone else's war is no plan at all until that war has come.
  return ph?.ifAtWar && !atWar(g, ph.ifAtWar[0], ph.ifAtWar[1]) ? null : ph;
}

export function phaseFor(g: Game, nat: string): PlanPhase | null {
  // In free play there is no plan of history's: the nation's own staff has decided this month's orders.
  if (isOpen(g)) {
    // History sets the start: the campaign in Poland was already ordered when the war began, and is fought as it was planned.
    const first = phases(nat)[0];
    if (first?.from === 1 && first.posture === 'attack' && g.turn <= first.to && nat !== g.human && (first.enemies ?? []).some((e) => g.nations[e]?.alive && atWar(g, nat, e))) return first;
    return staffPhase(g, nat, (r) => aside(g, r, nat));
  }
  return phases(nat).find((p) => g.turn >= p.from && g.turn <= p.to) ?? null;
}

/** What history's plan asks of a nation this month in each theatre overseas where it has troops: the phase, or null for "stand on the defensive". */
export function theatrePhases(g: Game, nat: string): { theatre: string; phase: PlanPhase | null; base: string | null }[] {
  const ids = theatreRegions(g);
  const out: { theatre: string; phase: PlanPhase | null; base: string | null }[] = [];
  if (isOpen(g)) return out;
  THEATRES.forEach((T, k) => {
    const list = T.plans[nat];
    if (!list || !begun(g, k) || !g.corps.some((c) => c.nat === nat && ids[k].has(c.reg))) return;
    const phase = theatrePhase(g, nat, list);
    // A garrison standing where a campaign will one day be fought has no orders from it yet.
    if (!phase && list.every((p) => p.from > g.turn)) return;
    out.push({ theatre: T.name, phase, base: phase?.base ?? T.bases?.[nat] ?? null });
  });
  return out;
}

/** Runs one nation's month. */
export function runAI(g: Game, nat: string): void {
  campaign(g, nat);
  aiProduction(g, nat);
}

function campaign(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.alive) return;
  // An army overseas fights its own war to its own plan. It stands aside while the main plan is
  // carried out, and the main army stands aside in its turn.
  const away = g.corps.filter((c) => c.nat === nat && !c.hold && theatreOf(g, c) >= 0);
  for (const c of away) c.hold = true;
  try { mainCampaign(g, nat); } finally { for (const c of away) c.hold = false; }
  if (n.alive) overseas(g, nat);
}

/** Runs a nation's corps in each overseas theatre by that theatre's plan. */
function overseas(g: Game, nat: string): void {
  const n = g.nations[nat];
  const ids = theatreRegions(g);
  THEATRES.forEach((T, k) => {
    const open = isOpen(g);
    const phases = open ? [] : T.plans[nat];
    if (!(open ? apart(g, k, nat) : phases && begun(g, k)) || !n.alive || !g.corps.some((c) => c.nat === nat && ids[k].has(c.reg))) return;
    // Some phases wait on someone else's war, as in the main plan, or on how the theatre's own war has gone.
    // In free play the army's own staff weighs up the theatre instead.
    const ph = open ? theatreStaff(g, nat, ids[k], k) : theatrePhase(g, nat, phases);
    if (ph?.declare) for (const e of ph.declare) declareWar(g, nat, e);
    if (!n.alive || !n.atWar.length) return;
    if (ph?.landings) landings(g, nat, ph, ids[k]);
    if (ph?.release) for (const c of g.corps) if (c.nat === nat && c.hold && c.kind !== 'fort' && ph.release.includes(g.regions[c.reg].name)) c.hold = false;
    const rest = g.corps.filter((c) => c.nat === nat && !c.hold && !ids[k].has(c.reg));
    for (const c of rest) c.hold = true;
    scope = ids[k];
    quiet = !!ph?.quiet;
    single = ph?.renew === false;
    leaving = ph?.withdraw ?? null;
    given = new Set((ph?.abandon ?? []).map((nm) => g.byName[nm]?.id).filter((id): id is number => id !== undefined && ids[k].has(id)));
    entrusted = handedOver(g, nat, ph);
    masked = new Set((ph?.mask ?? []).map((nm) => g.byName[nm]?.id).filter((id): id is number => id !== undefined));
    try {
      // The plan's standing orders come first: the force it keeps at a place stays, and ground it gives up is left,
      // the army falling back along the road to the base that feeds it.
      hold(g, nat, ph);
      const base = g.byName[ph?.base ?? T.bases?.[nat] ?? ''];
      if (base) fallBack(g, nat, base.id);
      // Whoever the plan has named so far, and anyone at war with this nation who is on the theatre's ground.
      const enemies = new Set<string>();
      for (const p of phases) if (p.from <= g.turn) for (const e of p.enemies ?? []) if (n.atWar.includes(e)) enemies.add(e);
      for (const id of ids[k]) { const r = g.regions[id]; if (n.atWar.includes(r.ctrl)) enemies.add(r.ctrl); }
      if (ph?.posture === 'attack' && enemies.size) {
        const only = ph.targets ? new Set(ph.targets) : null;
        // Nothing outside the theatre, and nothing the plan has not named, draws this army on.
        const halt = new Set([...(ph.siege ?? []), ...(ph.mask ?? [])]), edge = new Set(halt);
        for (const r of g.regions) {
          if (!ids[k].has(r.id)) { halt.add(r.name); edge.add(r.name); }
          else if (only && !only.has(r.name)) halt.add(r.name);
        }
        offensive(g, nat, ph.objectives ?? [], enemies, ph.minOdds ?? 1.3, [], true, true, halt, only, ph.oddsFor);
        // With the month's fighting done the rest of the army closes up to wherever the enemy now stands.
        if (n.alive) { advance(g, nat, enemies, 0, null, edge); mass(g, nat, ph); defend(g, nat, false); }
      } else { gather(g, nat, ph); mass(g, nat, ph); defend(g, nat); }
    } finally {
      scope = null;
      quiet = false;
      single = false;
      leaving = null;
      given = new Set();
      entrusted = new Set();
      masked = new Set();
      massed = new Set();
      for (const c of rest) c.hold = false;
    }
  });
}

function mainCampaign(g: Game, nat: string): void {
  const n = g.nations[nat];
  const open = isOpen(g);
  if (open) decide(g, nat, (r) => aside(g, r, nat));
  const planned = phaseFor(g, nat);
  // Some phases wait on someone else's war: Germany's allies only march east if Germany does.
  const ph = planned?.ifAtWar && !atWar(g, planned.ifAtWar[0], planned.ifAtWar[1]) ? null : planned;
  if (ph?.declare) {
    let go = true;
    if (ph.declareWhenCollapsing) {
      const t = g.nations[ph.declareWhenCollapsing];
      go = t.alive ? collapseScore(g, t.id) >= (ph.collapseAt ?? 0.3 * t.resilience) : !!t.fellOn;
    }
    if (go) for (const e of ph.declare) declareWar(g, nat, e);
    if (open) afterDeclaring(g, nat);
  }
  navalAI(g, nat, ph?.fleets);
  airAI(g, nat);
  // A nation at peace gives its army no orders, unless its staff is bringing it up for a war of its own choosing.
  if (!n.atWar.length && !(open && ph?.posture === 'redeploy')) return;
  if (ph?.landings) landings(g, nat, ph);
  if (ph?.release) for (const c of g.corps) if (c.nat === nat && c.hold && c.kind !== 'fort' && ph.release.includes(g.regions[c.reg].name)) c.hold = false;
  const posture = ph?.posture ?? 'defend';
  entrusted = handedOver(g, nat, ph);
  given = new Set((ph?.abandon ?? []).map((nm) => g.byName[nm]?.id).filter((id): id is number => id !== undefined));
  quiet = !!ph?.quiet;
  single = ph?.renew === false;
  try { fight(g, nat, ph, posture); } finally { entrusted = new Set(); massed = new Set(); given = new Set(); quiet = false; single = false; }
}

/**
 * The sectors of the front the plan has entrusted to an ally, where at least two of that ally's corps now stand.
 * In a theatre the allies came ashore side by side and the boundary between them was drawn before they sailed:
 * there the ally's sector is his from the day it is in friendly hands.
 */
function handedOver(g: Game, nat: string, ph: PlanPhase | null): Set<number> {
  const out = new Set<number>();
  for (const [ally, places] of Object.entries(ph?.entrust ?? {})) {
    if (!allied(g, nat, ally)) continue;
    for (const name of places) {
      const r = g.byName[name];
      if (r && allied(g, nat, r.ctrl) && (scope || r.units.filter((u) => u.nat === ally).length >= 2)) out.add(r.id);
    }
  }
  return out;
}

/** The month's operations on the main fronts, by the posture the plan calls for. */
function fight(g: Game, nat: string, ph: PlanPhase | null, posture: string): void {
  const n = g.nations[nat];
  hold(g, nat, ph);
  fallBack(g, nat);
  if (posture === 'defend') { gather(g, nat, ph); mass(g, nat, ph); defend(g, nat); return; }
  const enemies = enemySet(g, nat);
  const follow = posture === 'attack' && !ph!.force;
  const halt = new Set([...(ph && g.turn === ph.from ? ph.haltOrder ?? [] : []), ...(ph?.siege ?? [])]);
  const only = ph?.targets ? new Set(ph.targets) : ph?.limited ? new Set(objectivesFor(g, nat)) : null;
  offensive(g, nat, objectivesFor(g, nat), enemies, ph?.minOdds ?? 1.3, ph?.axis ?? [], g.turn > (ph?.from ?? 0), follow, halt, only, ph?.oddsFor);
  if (!n.alive) return;
  if (posture === 'redeploy') redeploy(g, nat, ph!, enemies);
  else if (follow) {
    advance(g, nat, enemies, 0, null, halt);
    mass(g, nat, ph);
    // With the month's attacks made, whoever still has marching time shores up the thin places.
    defend(g, nat, false);
    // Where the plan calls for it, troops who closed up to a great prize late in the month go straight in
    // once the line around it has been filled out, and the line is then put in order again.
    if (ph?.lastPush?.length) {
      const fights = g.reports.length;
      offensive(g, nat, ph.lastPush, enemies, ph.minOdds ?? 1.3, [], true, false, halt, new Set(ph.lastPush));
      if (n.alive && g.reports.length > fights) defend(g, nat, false);
    }
  }
}

/** The corps of this nation at a place that a plan could keep there: the foot first, then the fast troops. */
function standing(g: Game, nat: string, r: Region): Corps[] {
  const fast = (c: Corps): number => (KIND[c.kind].mobile ? 1 : 0);
  return r.units.filter((u) => u.nat === nat && u.kind !== 'fort').sort((a, b) => fast(a) - fast(b));
}

/**
 * Marks the force the plan keeps at each of its staging places, before the month's operations begin: those corps
 * stand where they are. Tanks are only kept while fewer than two corps of infantry are there to hold the place.
 */
function hold(g: Game, nat: string, ph: PlanPhase | null): void {
  if (!ph?.staging || ph.posture === 'redeploy') return;
  for (const [name, count] of Object.entries(ph.staging)) {
    const r = g.byName[name];
    if (!r || !allied(g, nat, r.ctrl)) continue;
    standing(g, nat, r).slice(0, count).forEach((u, k) => { if (!KIND[u.kind].mobile || k < 2) massed.add(u.id); });
  }
}

/**
 * Brings each staging place up to the force the plan wants there: while there are too few, the nearest corps of
 * infantry that can be spared marches to join them. Nobody is taken from a place he holds alone.
 */
function mass(g: Game, nat: string, ph: PlanPhase | null): void {
  if (!ph?.staging || ph.posture === 'redeploy') return;
  const own = (r: Region): Corps[] => r.units.filter((u) => u.nat === nat && u.kind !== 'fort');
  for (const [name, count] of Object.entries(ph.staging)) {
    const r = g.byName[name];
    if (!r || !allied(g, nat, r.ctrl)) continue;
    let have = own(r).filter((u) => massed.has(u.id)).length;
    if (have >= count) continue;
    for (const u of standing(g, nat, r)) if (have < count && !massed.has(u.id) && (!KIND[u.kind].mobile || have < 2)) { massed.add(u.id); have++; }
    const d = hops(g, r.id, (x) => allied(g, nat, x.ctrl));
    const pool = g.corps
      .filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && !KIND[c.kind].mobile && c.ap > 0 && !massed.has(c.id) && c.reg !== r.id && d[c.reg] > 0 && d[c.reg] <= 3 && own(g.regions[c.reg]).length >= 2)
      .sort((a, b) => d[a.reg] - d[b.reg] || atkPower(g, b) - atkPower(g, a));
    for (const c of pool) {
      if (have >= count || friendlyUnits(g, r, nat).length >= STACK) break;
      if (own(g.regions[c.reg]).length < 2) continue;
      walkToward(g, c, r);
      if (c.reg === r.id) { massed.add(c.id); have++; }
    }
  }
}

/**
 * Gives up the ground the plan has written off: every corps standing on it marches toward home by the shortest
 * road over friendly country, as far as the month allows. Those nearest home go first and leave room behind them.
 */
function fallBack(g: Game, nat: string, cap = g.nations[nat].capital): void {
  if (!given.size || cap < 0 || !allied(g, nat, g.regions[cap].ctrl)) return;
  const home = hops(g, cap, (r) => allied(g, nat, r.ctrl) && within(r));
  const leaving = g.corps
    .filter((c) => c.nat === nat && c.kind !== 'fort' && !c.hold && given.has(c.reg) && home[c.reg] > 0)
    .sort((a, b) => home[a.reg] - home[b.reg]);
  // Where the one road home is full of the nation's own troops, one of them goes a stage further to make room.
  const makeRoom = (r: Region, depth = 0): boolean => {
    if (friendlyUnits(g, r, nat).length < STACK) return true;
    if (depth > 3) return false;
    const movers = r.units.filter((u) => u.nat === nat && !u.hold && u.kind !== 'fort' && u.ap > 0);
    const nexts = r.nb.map((j) => g.regions[j]).filter((x) => allied(g, nat, x.ctrl) && home[x.id] >= 0 && home[x.id] < home[r.id]).sort((a, b) => friendlyUnits(g, a, nat).length - friendlyUnits(g, b, nat).length);
    for (const x of nexts) {
      if (!movers.length || !makeRoom(x, depth + 1)) continue;
      for (const u of movers) if (u.ap >= Math.min(moveCost(g, u, x), 4) && moveCorps(g, u, x)) return true;
    }
    return false;
  };
  leaving.forEach((c, i) => {
    // An army overseas has one road home and little room on it: whoever gets off the ground being given up first
    // goes a stage further if those still coming behind would not all fit where he stands.
    const behind = scope ? leaving.length - 1 - i : 0;
    for (let step = 0; step < 6 && c.ap > 0; step++) {
      if (!given.has(c.reg) && (!scope || friendlyUnits(g, g.regions[c.reg], nat).length + behind <= STACK)) break;
      let to: Region | null = null;
      for (const j of g.regions[c.reg].nb) {
        const r = g.regions[j];
        if (!allied(g, nat, r.ctrl) || home[j] < 0 || home[j] >= home[c.reg] || friendlyUnits(g, r, nat).length >= STACK) continue;
        // Out of the ground being given up if there is a way, and by the emptier road.
        const worse = (x: Region): number => (given.has(x.id) ? 10 : 0) + friendlyUnits(g, x, nat).length;
        if (!to || worse(r) < worse(to)) to = r;
      }
      if (!to) to = g.regions[c.reg].nb.map((j) => g.regions[j]).find((r) => allied(g, nat, r.ctrl) && home[r.id] >= 0 && home[r.id] < home[c.reg] && makeRoom(r)) ?? null;
      if (!to || c.ap < Math.min(moveCost(g, c, to), 4) || !moveCorps(g, c, to)) break;
    }
  });
}

/**
 * Before a great offensive the fast troops are taken out of the quiet parts of the line and brought together by
 * rail where it will open. Nobody leaves a place that would be held by fewer than two corps without him, and
 * a corps of infantry marches out to make room where the assembly area is full.
 */
function gather(g: Game, nat: string, ph: PlanPhase | null): void {
  if (!ph?.concentrate) return;
  const places = Object.keys(ph.concentrate).map((nm) => g.byName[nm]).filter((r) => r && allied(g, nat, r.ctrl));
  const at = new Set(places.map((r) => r.id));
  const fast = (r: Region): Corps[] => r.units.filter((u) => u.nat === nat && KIND[u.kind].mobile);
  const crowd = (r: Region): number => friendlyUnits(g, r, nat).length;
  for (const r of places) {
    const want = ph.concentrate[r.name];
    for (let guard = 0; guard < want && fast(r).length < want; guard++) {
      if (crowd(r) >= STACK) {
        const foot = r.units.find((u) => u.nat === nat && !u.hold && u.kind !== 'fort' && !KIND[u.kind].mobile && u.ap > 0);
        const next = r.nb.map((j) => g.regions[j]).filter((x) => x.ctrl === nat && within(x) && !at.has(x.id) && crowd(x) < STACK).sort((a, b) => crowd(a) - crowd(b))[0];
        if (!foot || !next || !moveCorps(g, foot, next)) break;
      }
      const d = hops(g, r.id, (x) => allied(g, nat, x.ctrl));
      const spare = (c: Corps): boolean => {
        const here = g.regions[c.reg];
        return here.units.filter((u) => u.nat === nat && u.kind !== 'fort').length >= 3 || !here.nb.some((j) => hostileUnits(g, g.regions[j], nat).length > 0);
      };
      // The main army comes by rail from anywhere on its own fronts. An army overseas has no railway: its fast
      // troops march, from as far as a month's marching reaches, and only over its own theatre's ground.
      const c = g.corps
        .filter((x) => x.nat === nat && !x.hold && KIND[x.kind].mobile && !at.has(x.reg) && d[x.reg] > 0 && spare(x)
          && (scope ? scope.has(x.reg) && x.ap >= d[x.reg] && d[x.reg] <= 4 : x.ap >= 4 && !aside(g, g.regions[x.reg], nat)))
        .sort((a, b) => (scope ? atkPower(g, b) - atkPower(g, a) : 0) || crowd(g.regions[b.reg]) - crowd(g.regions[a.reg]) || d[a.reg] - d[b.reg])[0];
      if (!c) break;
      if (scope) { walkToward(g, c, r); if (c.reg !== r.id) break; } else if (!railMove(g, c, r)) break;
    }
    fast(r).forEach((u) => massed.add(u.id));
  }
}

/** Enemies this nation is actively campaigning against: everyone named in its plan so far, plus any invader. */
function enemySet(g: Game, nat: string): Set<string> {
  const n = g.nations[nat];
  const s = new Set<string>();
  const named = isOpen(g) ? [phaseFor(g, nat)].filter((p): p is PlanPhase => !!p) : phases(nat).filter((p) => p.from <= g.turn);
  for (const p of named) for (const e of p.enemies ?? []) if (n.atWar.includes(e)) s.add(e);
  for (const r of g.regions) if (r.owner === nat && r.ctrl !== nat && n.atWar.includes(r.ctrl)) s.add(r.ctrl);
  return s;
}

function objectivesFor(g: Game, nat: string): string[] {
  if (isOpen(g)) return phaseFor(g, nat)?.objectives ?? [];
  const list = phases(nat).filter((p) => p.from <= g.turn).sort((a, b) => b.from - a.from);
  return list.flatMap((p) => p.objectives ?? []);
}

// ---- Across the sea --------------------------------------------------------------------

/**
 * Sends the corps the plan names across the sea, each to the first place on its list it can get ashore.
 * The main plan sends them from a home port; a theatre's plan (within) from any port its side holds there.
 */
function landings(g: Game, nat: string, ph: PlanPhase, within?: Set<number>): void {
  for (const L of ph.landings ?? []) {
    const c = g.corps.find((x) => x.nat === nat && x.name === L.corps && !x.dead);
    if (!c) continue;
    const from = g.regions[c.reg];
    // Only a corps still waiting in port: one already overseas has made its landing.
    const waiting = from.port && (within ? within.has(from.id) && allied(g, nat, from.ctrl) && !L.to.includes(from.name) : from.owner === nat);
    if (!waiting) continue;
    // Others the plan sends in the same lift, if they are standing on the same quay.
    const force = [c, ...(L.with ?? []).map((nm) => g.corps.find((x) => x.nat === nat && x.name === nm && !x.dead && x.reg === c.reg)).filter((x): x is Corps => !!x)];
    for (const name of L.to) {
      const to = g.byName[name];
      if (!to) continue;
      let ok = false;
      if (allied(g, nat, to.ctrl)) ok = to.owner !== nat && force.map((x) => shipMove(g, x, to)).some(Boolean);
      else if (atWar(g, nat, to.ctrl)) {
        // A defended shore is only stormed at fair odds.
        if (hostileUnits(g, to, nat).length && assess(g, force, to, landingOpts(g, force, to)).odds < (L.minOdds ?? 1.2)) continue;
        const res = landCorps(g, force, to);
        ok = res.ok && !!res.taken;
        if (res.ok && !res.taken) break; // beaten off: the corps is back in port and has had its month
      }
      // Once ashore the corps stays overseas for as long as the plan's operation runs.
      if (ok) { for (const x of force) { x.hold = false; g.flags[`exp:${x.id}`] = ph.to; } break; }
    }
  }
}

// ---- Attack ---------------------------------------------------------------------------

function hops(g: Game, from: number, pass: (r: Region) => boolean): Int16Array {
  const d = new Int16Array(g.regions.length).fill(-1);
  d[from] = 0;
  const q = [from];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && pass(g.regions[j])) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

/** How strongly each region pulls the offensive: objectives pull hardest, and the ground on the way to them pulls too. */
function objectivePull(g: Game, nat: string, objs: string[]): Float32Array {
  const pull = new Float32Array(g.regions.length);
  objs.forEach((name, k) => {
    const o = g.byName[name];
    if (!o || allied(g, nat, o.ctrl)) return;
    const w = Math.max(3, 14 - 0.5 * k);
    const d = hops(g, o.id, () => true);
    for (let i = 0; i < pull.length; i++) if (d[i] >= 0 && d[i] <= 4) pull[i] = Math.max(pull[i], w * Math.pow(0.6, d[i]));
  });
  return pull;
}

const atkPower = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;

/** Steps through friendly ground from every region to the nearest one that borders the goal. */
function doorstep(g: Game, nat: string, goal: Region | null): Int16Array | null {
  if (!goal) return null;
  const d = new Int16Array(g.regions.length).fill(-1);
  const q: number[] = [];
  for (const j of goal.nb) if (allied(g, nat, g.regions[j].ctrl)) { d[j] = 0; q.push(j); }
  if (!q.length) return null;
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, nat, g.regions[j].ctrl)) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

function offensive(g: Game, nat: string, objs: string[], enemies: Set<string>, minOdds: number, axisList: string[], axisOpen: boolean, follow: boolean, halt: Set<string>, only: Set<string> | null, oddsFor: Record<string, number> = {}): void {
  const failed = new Set<number>();
  const axis = new Set(axisList);
  // In the first month the armoured thrust takes its objectives in order; after that the whole line is open.
  const notYet = (name: string): boolean => {
    if (axisOpen || !axis.has(name)) return false;
    for (const a of axisList) {
      if (a === name) return false;
      const r = g.byName[a];
      if (r && !allied(g, nat, r.ctrl)) return true;
    }
    return false;
  };
  const axisGoal = (): Region | null => {
    for (const a of axisList) {
      const r = g.byName[a];
      if (r && !allied(g, nat, r.ctrl)) return enemies.has(r.ctrl) && atWar(g, nat, r.ctrl) && !failed.has(r.id) ? r : null;
    }
    return null;
  };
  // Week by week, so that the first day's attacks all happen before anyone's second bound.
  for (let wave = 1; wave <= 4; wave++) {
   for (let pass = 0; pass < 3; pass++) {
    for (let iter = 0; iter < 200; iter++) {
      if (!g.nations[nat].alive) return;
      ensureSupply(g);
      const pull = objectivePull(g, nat, objs);
      const door = doorstep(g, nat, axisGoal());
      const cand = new Map<number, Corps[]>();
      for (const c of g.corps) {
        if (c.nat !== nat || c.hold || c.kind === 'fort' || c.ap <= 0 || weekOf(c, 1) > wave) continue;
        // An army overseas has no reserves to spare: the force its plan keeps at a place holds it, and joins no attack elsewhere.
        if (scope && massed.has(c.id)) continue;
        // Early in the month the fast troops are kept for the main thrust if they can get to it.
        const kept = door !== null && wave <= 2 && KIND[c.kind].mobile && !massed.has(c.id) && door[c.reg] >= 0 && door[c.reg] <= c.ap;
        for (const j of g.regions[c.reg].nb) {
          const r = g.regions[j];
          if (failed.has(j) || halt.has(r.name) || (only && !only.has(r.name)) || !enemies.has(r.ctrl) || !atWar(g, nat, r.ctrl) || notYet(r.name)) continue;
          if (kept && !axis.has(r.name)) continue;
          let list = cand.get(j);
          if (!list) cand.set(j, (list = []));
          list.push(c);
        }
      }
      let best: { score: number; target: Region; corps: Corps[]; battle: boolean } | null = null;
      for (const [j, avail] of cand) {
        const target = g.regions[j];
        const value = target.vp * 2 + (target.cap ? 8 : 0) + pull[j];
        if (!hostileUnits(g, target, nat).length) {
          const movers = avail
            .filter((c) => c.ap >= Math.min(moveCost(g, c, target), 4))
            .sort((a, b) => KIND[a.kind].move - KIND[b.kind].move || b.ap - a.ap);
          if (!movers.length || value < 2.5) continue;
          if (!axis.has(target.name)) movers.sort((a, b) => (KIND[a.kind].mobile ? 1 : 0) - (KIND[b.kind].mobile ? 1 : 0) || b.ap - a.ap);
          const score = value + 2;
          if (!best || score > best.score) best = { score, target, corps: [movers[0]], battle: false };
        } else {
          const pool = limitAttackers(g, avail.filter((c) => c.org >= 0.35), target);
          if (!pool.length) continue;
          // The tanks go in together along the main axis; elsewhere the infantry does the work.
          const main = axis.has(target.name);
          const fast = (c: Corps): number => (KIND[c.kind].mobile ? 1 : 0);
          pool.sort((a, b) => (main ? fast(b) - fast(a) : fast(a) - fast(b)) || atkPower(g, b) - atkPower(g, a));
          const least = main ? Math.max(1, pool.filter((c) => fast(c)).length) : 1;
          let pick: Corps[] | null = null, odds = 0;
          for (let k = least; k <= pool.length; k++) {
            odds = assess(g, pool.slice(0, k), target).odds;
            if (odds >= 2.6) { pick = pool.slice(0, k); break; }
          }
          if (!pick) { if (odds < (oddsFor[target.name] ?? minOdds)) continue; pick = pool; }
          const score = value + Math.min(odds, 4);
          if (!best || score > best.score) best = { score, target, corps: pick, battle: true };
        }
      }
      if (!best) break;
      let taken: boolean;
      if (best.battle) {
        taken = resolveBattle(g, best.corps, best.target).taken;
        // A slow command does not get a second assault organised in the same month, and some plans allow only one.
        if (!taken && (g.nations[nat].slowCommand || single)) failed.add(best.target.id);
      }
      else { taken = moveCorps(g, best.corps[0], best.target); if (!taken) failed.add(best.target.id); }
      if (taken && g.nations[nat].alive) support(g, nat, best.target);
    }
    // Nothing left to hit this week: close up to the front, then look again.
    if (!follow || !g.nations[nat].alive || !advance(g, nat, enemies, wave, doorstep(g, nat, axisGoal()), halt)) break;
   }
  }
}

/** A spearhead alone is a spearhead lost: bring a second corps up behind it when the enemy is close. */
function support(g: Game, nat: string, at: Region): void {
  if (!allied(g, nat, at.ctrl)) return;
  const danger = at.nb.some((j) => hostileUnits(g, g.regions[j], nat).length > 0);
  const want = danger ? 2 : 1;
  let have = friendlyUnits(g, at, nat).length;
  if (have >= want) return;
  const pool = g.corps
    .filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && g.regions[c.reg].nb.includes(at.id) && c.ap >= Math.min(moveCost(g, c, at), 4))
    .sort((a, b) => (a.kind === 'arm' ? 1 : 0) - (b.kind === 'arm' ? 1 : 0) || b.ap - a.ap);
  for (const c of pool) {
    if (have >= want) break;
    if (moveCorps(g, c, at)) have++;
  }
}

/** Brings the rest of the army up to the front. With a week given, only as far as that week allows. Returns how many moves were made. */
function advance(g: Game, nat: string, enemies: Set<string>, wave: number, door: Int16Array | null, halt: Set<string>): number {
  let made = 0;
  const N = g.regions.length;
  const dist = new Float32Array(N).fill(1e9);
  const atFront = new Uint8Array(N);
  const full = (r: Region): boolean => friendlyUnits(g, r, nat).length >= STACK;
  const q: number[] = [];
  for (const r of g.regions) {
    // An army closes up to its own front, not to an ally's. Overseas the allies share one front.
    // A side theatre where the nation has a plan of its own is no front of the main army's: nobody is railed there.
    if (scope ? !scope.has(r.id) || !allied(g, nat, r.ctrl) : r.ctrl !== nat || aside(g, r, nat)) continue;
    if (entrusted.has(r.id) || given.has(r.id)) continue; // an ally holds this stretch now, or the plan has written it off
    let open = false, walled = false;
    for (const j of r.nb) {
      const t = g.regions[j];
      if (!enemies.has(t.ctrl) || !atWar(g, nat, t.ctrl) || halt.has(t.name)) continue; // a sector under a halt order draws no one
      if (t.fort && t.fort.vs.includes(r.owner) && t.fort.lv >= 2) walled = true; else open = true;
    }
    if (open) atFront[r.id] = 1;
    // A stretch of front already packed with troops needs no more: the columns behind look further along.
    if ((open || walled) && !full(r)) { dist[r.id] = open ? 0 : 4; q.push(r.id); }
  }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    // A region packed with troops cannot be marched through: the columns behind it go round.
    if (full(g.regions[a])) continue;
    for (const j of g.regions[a].nb) {
      if (!allied(g, nat, g.regions[j].ctrl) || !within(g.regions[j])) continue;
      if (dist[a] + 1 < dist[j]) { dist[j] = dist[a] + 1; q.push(j); }
    }
  }
  const movers = g.corps
    .filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap > 0)
    .sort((a, b) => KIND[a.kind].move - KIND[b.kind].move);
  // Two corps stay to screen any place that faces the enemy, even where the plan has called a halt.
  const screen = (c: Corps): boolean => {
    const here = g.regions[c.reg];
    return friendlyUnits(g, here, nat).length <= 2 && here.nb.some((j) => !masked.has(j) && hostileUnits(g, g.regions[j], nat).length > 0);
  };
  for (const c of movers) {
    if (screen(c) || massed.has(c.id)) continue;
    if (door && KIND[c.kind].mobile && door[c.reg] > 0) {
      // Fast troops close up on the main thrust's next objective.
      for (let step = 0; step < 6 && c.ap > 0 && door[c.reg] > 0; step++) {
        let to: Region | null = null;
        for (const j of g.regions[c.reg].nb) {
          const r = g.regions[j];
          if (door[j] < 0 || door[j] >= door[c.reg] || friendlyUnits(g, r, nat).length >= STACK) continue;
          if (!to || door[j] < door[to.id]) to = r;
        }
        if (!to || (wave > 0 && weekOf(c, moveCost(g, c, to)) > wave) || !moveCorps(g, c, to)) break;
        made++;
      }
      continue;
    }
    if (!scope && wave <= 1 && c.ap >= 4 && dist[c.reg] >= 3 && dist[c.reg] < 1e9) {
      // Far behind the front: go by rail to the nearest railhead that has been in friendly hands a while.
      // Only a railhead the trains can actually reach: a front across the sea is no use to a corps on this shore.
      const byRail = hops(g, c.reg, (r) => allied(g, nat, r.ctrl));
      let to: Region | null = null, bd = dist[c.reg] - 1, bu = 99;
      for (const r of g.regions) {
        if (r.since >= g.turn || dist[r.id] > bd || byRail[r.id] < 0 || !allied(g, nat, r.ctrl)) continue;
        const u = friendlyUnits(g, r, nat).length;
        if (u >= STACK) continue;
        if (dist[r.id] < bd || u < bu) { to = r; bd = dist[r.id]; bu = u; }
      }
      if (to && railMove(g, c, to)) { made++; continue; }
    }
    for (let step = 0; step < 6 && c.ap > 0; step++) {
      const here = c.reg;
      if (atFront[here] || dist[here] >= 1e9) break;
      let to: Region | null = null, bd = dist[here], bu = 99;
      for (const j of g.regions[here].nb) {
        const r = g.regions[j];
        if (!allied(g, nat, r.ctrl) || dist[j] >= bd + (to ? 0.001 : 0)) continue;
        const u = friendlyUnits(g, r, nat).length;
        if (u >= STACK) continue;
        if (dist[j] < bd || u < bu) { to = r; bd = dist[j]; bu = u; }
      }
      if (!to || (wave > 0 && weekOf(c, moveCost(g, c, to)) > wave) || !moveCorps(g, c, to)) break;
      made++;
    }
  }
  return made;
}

// ---- Redeploy -------------------------------------------------------------------------

function redeploy(g: Game, nat: string, ph: PlanPhase, enemies: Set<string>): void {
  const engaged = (c: Corps): boolean => g.regions[c.reg].nb.some((j) => enemies.has(g.regions[j].ctrl) && atWar(g, nat, g.regions[j].ctrl));
  const free = (): Corps[] => g.corps.filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap >= 4 && !engaged(c));
  const room = (r: Region): boolean => allied(g, nat, r.ctrl) && friendlyUnits(g, r, nat).length < STACK;
  const nearest = (list: Corps[], to: Region): Corps | null => {
    const d = hops(g, to.id, (r) => allied(g, nat, r.ctrl));
    let best: Corps | null = null, bd = 1e9;
    for (const c of list) if (d[c.reg] >= 0 && d[c.reg] < bd) { bd = d[c.reg]; best = c; }
    return best;
  };

  // Occupation troops for what has been taken.
  for (const name of ph.garrison ?? []) {
    const r = g.byName[name];
    if (!r || r.ctrl !== nat || r.units.some((u) => u.nat === nat && u.hold)) continue;
    const c = nearest(free().filter((c) => c.kind === 'inf'), r);
    if (!c) continue;
    if (c.reg !== r.id && !railMove(g, c, r)) continue;
    c.hold = true;
  }
  // Small forces kept at a jumping-off point for a later operation.
  const staged = new Set<number>();
  for (const [name, count] of Object.entries(ph.staging ?? {})) {
    const r = g.byName[name];
    if (!r || !allied(g, nat, r.ctrl)) continue;
    const there = r.units.filter((u) => u.nat === nat && !u.hold);
    there.forEach((u) => staged.add(u.id));
    for (let k = there.length; k < count; k++) {
      const c = nearest(free().filter((c) => c.kind === 'inf' && !staged.has(c.id)), r);
      if (!c || !railMove(g, c, r)) break;
      staged.add(c.id);
    }
  }
  const armour = new Set(ph.armour ?? []);
  const assembly = ph.assembly ?? [];
  for (const c of free()) {
    if (staged.has(c.id)) continue;
    const here = g.regions[c.reg].name;
    const mobile = KIND[c.kind].mobile;
    if (mobile && armour.has(here)) continue;
    if (!mobile && assembly.includes(here) && !armour.has(here)) continue;
    let to: Region | null = null;
    if (mobile) to = (ph.armour ?? []).map((nm) => g.byName[nm]).find((r) => r && room(r)) ?? null;
    if (!to) {
      // Infantry leaves the armour's assembly areas free while there is room elsewhere.
      const open = assembly.map((nm) => g.byName[nm]).filter((r) => r && room(r));
      const pref = mobile ? open : open.filter((r) => !armour.has(r.name)).concat(open.filter((r) => armour.has(r.name)));
      if (!mobile && assembly.includes(here)) continue;
      let bu = 99;
      for (const r of pref) {
        const u = friendlyUnits(g, r, nat).length + (!mobile && armour.has(r.name) ? 10 : 0);
        if (u < bu) { bu = u; to = r; }
      }
    }
    if (to) railMove(g, c, to);
  }
}

// ---- Defend ---------------------------------------------------------------------------

function defPower(g: Game, c: Corps, r: Region): number {
  return corpsBase(g, c) * KIND[c.kind].def * TERRAIN_DEF[r.terrain];
}

function defend(g: Game, nat: string, line = true): void {
  ensureSupply(g);
  const n = g.nations[nat];
  if (n.seaSupply && (!scope || leaving)) evacuations(g, nat);

  // Where is the danger, and how well is each place held?
  const N = g.regions.length;
  const threat = new Float32Array(N), held = new Float32Array(N);
  // What this nation has to hold: its own country, and whatever it has taken and occupies.
  // Overseas, the allies hold one front between them and each looks to all of it.
  // A side theatre where the nation has a plan of its own is that army's business, and no concern of the main plan.
  const mine = (r: Region): boolean => (scope ? scope.has(r.id) && allied(g, nat, r.ctrl) : !aside(g, r, nat) && (r.ctrl === nat || (r.owner === nat && allied(g, nat, r.ctrl))));
  for (const r of g.regions) {
    if (!mine(r)) continue;
    for (const j of r.nb) {
      const e = g.regions[j];
      if (!atWar(g, nat, e.ctrl) || masked.has(j)) continue; // a garrison the plan leaves to others threatens nobody
      const foes = hostileUnits(g, e, nat).filter((u) => u.kind !== 'fort').map((u) => atkPower(g, u)).sort((a, b) => b - a);
      for (const p of foes.slice(0, frontage(r))) threat[r.id] += p;
    }
    for (const u of friendlyUnits(g, r, nat)) held[r.id] += defPower(g, u, r);
  }
  // In free play a staff does not wait for the enemy to reach the gates: the capital is covered against the
  // strongest force he has within three marches of it, as if that force already stood next door.
  if (!scope && isOpen(g) && n.capital >= 0 && mine(g.regions[n.capital]) && threat[n.capital] === 0) {
    const cap = g.regions[n.capital];
    const seen = new Set([cap.id]);
    let ring = [cap.id], worst = 0;
    for (let d = 0; d < 3; d++) {
      const next: number[] = [];
      for (const id of ring) for (const j of g.regions[id].nb) {
        if (seen.has(j)) continue;
        seen.add(j); next.push(j);
        const foes = hostileUnits(g, g.regions[j], nat).filter((u) => u.kind !== 'fort').map((u) => atkPower(g, u)).sort((a, b) => b - a);
        worst = Math.max(worst, foes.slice(0, frontage(cap)).reduce((sum, p) => sum + p, 0));
      }
      ring = next;
    }
    threat[cap.id] = worst;
  }
  const importance = (r: Region): number => 1 + 0.5 * r.vp + (r.cap ? 4 : 0);
  // A sector handed over to an ally is his to hold: no reserves go there.
  // Nor into ground the plan is giving up.
  const need = (r: Region): number => (entrusted.has(r.id) || given.has(r.id) ? 0 : importance(r) * Math.max(0, 1.3 * threat[r.id] - held[r.id]));

  // Ground worth fighting to get back: the nation's own, its allies', and what it held until the enemy took it.
  const lost = (r: Region): boolean => allied(g, nat, r.owner) || r.prev === nat;
  // A slow command needs a month to organise any answer to ground it has just lost.
  const late = (r: Region): boolean => n.slowCommand && r.since >= g.turn;
  const movers = g.corps.filter((c) => c.nat === nat && !c.hold && !c.dyle && c.kind !== 'fort' && c.ap > 0);
  for (const c of movers) {
    if (c.dead || c.ap <= 0 || massed.has(c.id)) continue;
    const here = g.regions[c.reg];
    if (!mine(here)) continue; // abroad on an ally's ground: stand where the plan put it

    // Step back into ground the enemy has left empty behind his spearheads. An army overseas that its plan
    // has told to stand quiet stays where it is: it is being saved for a set-piece battle, not a scramble.
    if (!(scope && quiet)) {
      const others = friendlyUnits(g, here, nat).length > 1 || threat[here.id] === 0;
      const gap = here.nb.map((j) => g.regions[j]).find((r) => atWar(g, nat, r.ctrl) && lost(r) && !late(r) && !hostileUnits(g, r, nat).length && c.ap >= Math.min(moveCost(g, c, r), 4));
      if (gap && others && moveCorps(g, c, gap)) continue;
    }
    // An army standing on the defensive keeps a continuous line: an empty place in it is an open door, and a
    // neighbour with three corps or more sends one to shut it. A slow command does not see the gap in time,
    // and an army on the attack accepts gaps behind its spearheads.
    if (line && !n.slowCommand && friendlyUnits(g, here, nat).length >= 3) {
      const door = here.nb.map((j) => g.regions[j]).find((r) => mine(r) && !given.has(r.id) && threat[r.id] > 0 && !friendlyUnits(g, r, nat).length && c.ap >= Math.min(moveCost(g, c, r), 4));
      if (door) {
        const was = defPower(g, c, here);
        if (moveCorps(g, c, door)) { held[here.id] -= was; held[door.id] += defPower(g, c, door); continue; }
      }
    }
    let spared = 0;
    if (entrusted.has(here.id)) {
      // An ally has taken this stretch over: one corps stays to stiffen him, the rest are free for the plan's own business.
      // In a theatre nobody stays: the army goes back inside its own boundary.
      if (!scope && here.units.filter((u) => u.nat === nat && u.kind !== 'fort').length <= 1) continue;
      spared = defPower(g, c, here);
    } else if (threat[here.id] > 0) {
      // In the line. It can be spared only if the place stays well held without it.
      spared = defPower(g, c, here);
      if (friendlyUnits(g, here, nat).length <= 2 || held[here.id] - spared < 1.6 * threat[here.id]) continue;
    }

    // A reserve: go where the line is thinnest for what it protects.
    const d = hops(g, here.id, (r) => allied(g, nat, r.ctrl) && within(r));
    let to: Region | null = null, bs = 0;
    for (const r of g.regions) {
      if (!mine(r) || d[r.id] <= 0 || friendlyUnits(g, r, nat).length >= STACK) continue;
      const s = need(r) / (1 + 0.5 * d[r.id]);
      if (s > bs) { bs = s; to = r; }
    }
    if (!to) continue;
    const before = c.reg;
    // There are no railways in the desert: overseas, everything marches.
    if (!scope && !n.slowCommand && d[to.id] > 2 && c.ap >= 4) railMove(g, c, to);
    else walkToward(g, c, to);
    if (c.reg !== before) { held[here.id] -= spared; held[to.id] += defPower(g, c, to); }
  }

  // Counter-attack where the odds are clearly good. A counter-attack answers a fresh loss: ground
  // the enemy has held for months is a matter for a planned offensive, not for the local commander.
  for (const r of g.regions) {
    if (!n.alive || quiet) return;
    if (scope && !scope.has(r.id)) continue;
    if (!atWar(g, nat, r.ctrl) || !lost(r) || late(r) || g.turn - r.since > COUNTER_WINDOW || !hostileUnits(g, r, nat).length) continue;
    // Troops that have already fought this month are in no state to be thrown straight into another attack.
    // In free play the force a staff keeps at a place stays there: it is not thrown into a counter-attack next door.
    const kept = isOpen(g) ? massed : null;
    const pool = limitAttackers(g, g.corps.filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap > 0 && !c.fought && c.org >= 0.6 && !kept?.has(c.id) && g.regions[c.reg].nb.includes(r.id)), r);
    // Overseas a local counter-stroke sends two corps in to hold what it wins: the rest keep the door open behind them.
    if (pool.length && assess(g, pool, r).odds >= 1.8) resolveBattle(g, pool, r, scope ? { occupy: 2 } : {});
  }
}

function walkToward(g: Game, c: Corps, to: Region): void {
  // The way there, going round any region too packed with troops to march through.
  const d = hops(g, to.id, (r) => allied(g, c.nat, r.ctrl) && within(r) && (r.id === c.reg || friendlyUnits(g, r, c.nat).length < STACK));
  for (let step = 0; step < 6 && c.ap > 0 && c.reg !== to.id; step++) {
    let next: Region | null = null;
    for (const j of g.regions[c.reg].nb) {
      if (d[j] < 0 || d[j] >= d[c.reg]) continue;
      const r = g.regions[j];
      if (friendlyUnits(g, r, c.nat).length >= STACK) continue;
      if (!next || d[j] < d[next.id]) next = r;
    }
    if (!next || !moveCorps(g, c, next)) return;
  }
}

/** An expeditionary corps goes home when it is cut off, broken, or its host is collapsing. */
function wantsOut(g: Game, c: Corps): boolean {
  if (g.regions[c.reg].owner === c.nat || c.dead) return false;
  if (theatreOf(g, c) >= 0) {
    // An army with a plan of its own in a side theatre is there to stay, unless that plan is to get out once the
    // front has gone: then it leaves when the enemy holds one of the places the plan names, when it is cut off,
    // or when the country it stands in has fallen. Whoever is already in one of the theatre's havens stays there.
    if (!scope || !leaving || !scope.has(c.reg) || havensOf(g.regions[c.reg]).includes(g.regions[c.reg].name)) return false;
    return !c.supplied || !g.nations[g.regions[c.reg].owner].alive || leaving.when.some((nm) => !!g.byName[nm] && atWar(g, c.nat, g.byName[nm].ctrl));
  }

  // The host is the biggest continental ally. If it has fallen, there is nothing left to stay for.
  let host: string | null = null, size = 0;
  for (const n of Object.values(g.nations)) {
    if (n.id === c.nat || n.dormant || !allied(g, c.nat, n.id) || n.capital < 0 || n.seaSupply) continue;
    if (n.armyPeak > size) { size = n.armyPeak; host = n.id; }
  }
  // A corps the plan sent overseas stays for the length of the operation, unless it is cut off,
  // broken, or the country it came to help, or the main ally on the Continent, has fallen.
  const until = g.flags[`exp:${c.id}`];
  if (until !== undefined && g.turn <= until) return !c.supplied || c.org < 0.3 || !host || !g.nations[host].alive || !g.nations[g.regions[c.reg].owner].alive;
  if (!c.supplied || c.org < 0.45 || !host || !g.nations[host].alive) return true;
  const H = g.nations[host];
  const d = hops(g, c.reg, (r) => allied(g, c.nat, r.ctrl));
  return d[H.capital] < 0 || collapseScore(g, host) >= 0.7 * H.resilience;
}

function evacuations(g: Game, nat: string): void {
  ensureSupply(g);
  const out = g.corps.filter((c) => c.nat === nat && (!scope || scope.has(c.reg)) && wantsOut(g, c));
  if (!out.length) return;
  const stuck: Corps[] = [];
  for (const c of out) {
    if (!g.regions[c.reg].port) {
      const d = hops(g, c.reg, (r) => allied(g, c.nat, r.ctrl) && within(r));
      let port: Region | null = null;
      for (const r of g.regions) {
        if (!r.port || r.owner === c.nat || d[r.id] < 0 || friendlyUnits(g, r, c.nat).length >= STACK) continue;
        if (!port || d[r.id] < d[port.id]) port = r;
      }
      if (port) walkToward(g, c, port);
    }
    if (g.regions[c.reg].port) evacuate(g, c); else stuck.push(c);
  }
  // No way to the sea: fight a way through to the nearest port.
  for (let tries = 0; tries < 3; tries++) {
    const trapped = stuck.filter((c) => !c.dead && c.ap > 0 && g.regions[c.reg].owner !== nat);
    if (!trapped.length) return;
    let goal: Region | null = null, most = 0;
    for (const r of g.regions) {
      if (!r.port || !atWar(g, nat, r.ctrl)) continue;
      const k = trapped.filter((c) => g.regions[c.reg].nb.includes(r.id)).length;
      if (k > most) { most = k; goal = r; }
    }
    if (!goal) return;
    const force = limitAttackers(g, trapped.filter((c) => g.regions[c.reg].nb.includes(goal!.id)), goal);
    if (hostileUnits(g, goal, nat).length) {
      if (assess(g, force, goal).odds < 1) return;
      if (!resolveBattle(g, force, goal).taken) return;
    } else if (!moveCorps(g, force[0], goal)) return;
    for (const c of trapped) {
      if (c.dead) continue;
      if (c.reg !== goal.id && g.regions[c.reg].nb.includes(goal.id)) moveCorps(g, c, goal);
      if (c.reg === goal.id) evacuate(g, c);
    }
  }
}
