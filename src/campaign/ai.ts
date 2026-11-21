// The computer's generals. In Historical mode each nation follows the intentions in
// data/campaign/plan-historical.json: who to attack, when, and toward which objectives.
// The plan only sets intentions. Every region still has to be won under the same rules
// the player uses, so history can come out differently.

import { mapDist } from './wrap';
import plan from '../../data/campaign/plan-historical.json';
import theatreData from '../../data/campaign/theatres.json';
import { afloat, airAI, airFleetsOf, fieldRoom, homeCommand, km, rebaseAir, whyNotBase } from './air';
import { aiProduction, armyWorn, gunsWanted, income } from './economy';
import { civicAI } from './buildings';
import { keepsStores } from './resources';
import { aiArsenal, aiFortify, aiProgramme } from './arsenal';
import { blocked, blowBridges } from './rivers';
import { groundRules, isFoot } from './ground';
import { SUPPORT } from './support';
import { aiBridges, aiEngineers } from './engineers';
import { NAVY, canSailTo, closedTo, crossingRisk, fleetsOf, landCorps, landingOpts, landingTargets, liftLeft, navalAI, planLanding, rebase, seaPath, shipCount, useLift } from './navy';
import { shoreAI } from './fleets';
import { shoreOpen, stopOf, stormHeldBy } from './history';
import { RIVALS, afterDeclaring, coastRules, coastWatch, decide, doorQuay, frontAshore, heedsCard, holdsAshore, isOpen, keepsDoor, joinsOn, lodgment, mainFirst, neighbourAcrossTheWater, oceanRuns, builtUp, onTheAttack, onTheDay, opensNow, opensOn, opensSoon, probed, probing, quayGuests, rivalRules, seaBase, seaFoe, staffPhase, strengthAsked, theatreStaff } from './staff';
import { STRENGTH, followUp } from './ashore';
import { beyondEurope, enemyAtTheDoor, overseasOf, type OverseasRules } from './world';
import { countField, homeThreat, homeland, leavesHome, mayLeave, reinforceRules, theatreFront } from './middleeast';
import { foldCorps, foldRules, foldView, whyNotFold } from './fold';
import { lineAny, lineGives, lineLeaves, lineShut } from './line';
import { hashString } from '../core/rng';
import { GREAT, SIEGE, arrive, besieged, driveOut, monthsShut, storesOf, feeders, fedByBook, fieldCtrl, garrison, garrisonOf, greatCity, greatPlace, guardOf, investedOn, previewSally, ringClosed, roomIn, sally, siegeRules, storm, stormForce, takeField, townHeld, whyNotSally } from './siege';
import { townSize } from './towns';
import { KIND, STACK, TERRAIN_DEF, type Corps, type Game, type PlanPhase, type Region } from './types';
import {
  DEFENCE, EASTERN, allied, assess, atWar, collapseScore, corpsBase, declareWar, ensureSupply, evacuate, fights, friendlyUnits, frontage, gunsMayFollow, gunsRule, havensOf, headline,
  hasRoom, hostileUnits, worksAgainst, limitAttackers, moveCorps as moveTo, moveCost, nearHarbour, probeLine, probeRules, probeWeight, putToSea, railMove, railReach, resolveBattle, shipMove, takeOut, voyagesOf, weekOf, whyNoProbe, worksOf,
} from './game';

const PLAN = plan as unknown as Record<string, PlanPhase[] | string>;

/** A computer staff's march: by the book none goes into ground the record's line shuts to it this month (line.ts). */
const moveCorps = (g: Game, c: Corps, to: Region): boolean => !lineShut(g, c.nat, to) && moveTo(g, c, to);

function phases(nat: string): PlanPhase[] {
  const p = PLAN[nat];
  return Array.isArray(p) ? p : [];
}

/** A war fought overseas to its own plan. See data/campaign/theatres.json. */
interface Theatre { name: string; regions: string[]; plans: Record<string, PlanPhase[]>; bases?: Record<string, string>; open?: boolean; opens?: number }
const THEATRE_ENTRIES = Object.entries(theatreData as unknown as Record<string, Theatre | string>).filter((e): e is [string, Theatre] => typeof e[1] !== 'string');
const THEATRES = THEATRE_ENTRIES.map((e) => e[1]);
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
/** How many waves the army being run may send at one place in a month: two, or more for a staff pressing a won war. */
let waves = 2;
/** Set while the army being run fights a war for a strip of frontier: only corps this many marches from the front close up to it. */
let near: number | null = null;
/** True while an army overseas is fighting its staff's main effort: the force massed opposite the target is there to attack it. */
let striking = false;
/** Set while the army being run has orders to leave the theatre once the front there has gone. */
let leaving: PlanPhase['withdraw'] | null = null;
/** Sectors the plan in force has handed over to an ally whose troops have arrived: the nation's own corps leave them, all but one. */
let entrusted = new Set<number>();
/** Enemy garrisons the theatre plan leaves to others: nobody attacks them, closes up to them or stays to watch them. */
let masked = new Set<number>();
/** Set while a plan keeps its army to the letter of its orders: the only places it may step into or win back. */
let bound: Set<string> | null = null;
/** Corps the plan in force keeps massed at a named place: the line is put in order without them. */
let massed = new Set<number>();
/** Ground the plan in force is giving up: the corps there march out and nobody is sent back in. */
let given = new Set<number>();
/** Places the staff has stepped out of this month because the ring was closing round them: no garrison is left behind there. */
let sacks = new Set<number>();
// The corps a staff is resting this month (Version 73): each is kept where it is until the month's moves are made.
let rested: Corps[] = [];
/** True for the staffs that rest their worn corps (rivals.json, `relief`): the main plan of a computer nation in the grand campaign. */
const relieves = (g: Game, nat: string): boolean => !!RIVALS.relief && !scope && rivalRules(g) && nat !== g.human && RIVALS.relief.staffs.includes(nat);
/** A corps such a staff counts as worn: under `worn` of its strength, or sent to rest and neither back to `fit` nor through its months of it. */
const wornOut = (g: Game, c: Corps): boolean => {
  const R = RIVALS.relief!;
  const since = g.flags[`rl:${c.id}`];
  if (since === undefined || g.turn - since >= 2 * R.months) return c.str < R.worn;
  return c.str < R.fit && g.turn - since < R.months;
};
/** A worn corps its staff will rest is stormed into no town and thrown at nobody, in the hours before the relief as after it. */
const spared = (g: Game, c: Corps): boolean => relieves(g, c.nat) && wornOut(g, c);

/** True for ground in a side theatre where this nation has a plan of its own: the army there looks after it, not the main plan. */
const aside = (g: Game, r: Region, nat: string): boolean => r.theatre >= 0 && !!THEATRES[r.theatre] && apart(g, r.theatre, nat);
/** An army run by a theatre's plan stays on that theatre's ground: it neither marches out of it nor takes a short cut round it. */
const within = (r: Region): boolean => !scope || scope.has(r.id);
/**
 * True for ground an army may step into or win back unbidden. A plan that keeps its army to the letter of its
 * orders was written against history's enemy: where a player holds the ground instead, the army answers him freely.
 */
const allowed = (g: Game, r: Region): boolean => !bound || bound.has(r.name) || r.ctrl === g.human;

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
  // The engineers go to their work before the staff moves anyone.
  aiEngineers(g, nat);
  campaign(g, nat);
  // With the month's moves made, a defence on a river blows its bridges.
  aiBridges(g, nat, (x, from) => blowBridges(g, nat, x, from));
  aiProduction(g, nat);
  aiArsenal(g, nat);
  aiFortify(g, nat, Object.keys(coastWatch(g, nat)), coastRules());
  // Its ships and aircraft come last of all (rulings 68(b) and 70(a)): after the corps, the guns and the works.
  aiProgramme(g, nat);
  civicAI(g, nat, income(g, nat), keepsStores(g, nat), gunsWanted(g, nat) || armyWorn(g, nat));
}

function campaign(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (!n.alive) return;
  // The grand campaign: two worn corps standing together are made into one before anyone marches.
  foldWorn(g, nat);
  // The grand campaign: garrisons with no enemy near come out of their towns before the month's marching begins.
  garrisons(g, nat, false);
  sacks = new Set();
  rested = [];
  // (b544: a ringed capital or great city of the staff's side is relieved before the staff moves anyone, and the
  // corps beside it stay there for the relief once half its stores are eaten.)
  const kept = reliefFirst(g, nat);
  try { campaignMoves(g, nat); } finally { if (n.alive) garrisons(g, nat, true); sacks = new Set(); for (const c of rested) c.hold = false; rested = []; for (const c of kept) c.hold = false; }
}

/**
 * The design director's b544: the relief of a besieged capital or great city comes first in a computer staff's
 * month, before the line, the reserves or the plan move anyone. (1) With the corps of its own next door that are
 * fit to attack, it goes in if the odds reach the relief bar. (2) Short of that, once the town has eaten half its
 * stores, those corps are held where they stand for the month, so the relief force gathers month on month instead
 * of being sent to other fronts. A staff's habit at every stop, for its own side's towns; a player's corps are
 * never held. Returns the corps held, for the month's end to free.
 */
export function reliefFirst(g: Game, nat: string): Corps[] {
  const kept: Corps[] = [];
  if (!siegeRules(g) || nat === g.human || !g.nations[nat].alive) return kept;
  for (const r of g.regions) {
    if (!besieged(g, r) || !allied(g, nat, r.ctrl) || !(townSize(g, r) === 'capital' || greatCity(g, r))) continue;
    if (!atWar(g, nat, fieldCtrl(g, r)) || !hostileUnits(g, r, nat).length || lineShut(g, nat, r)) continue;
    const near = g.corps.filter((c) => c.nat === nat && !c.dead && !c.hold && c.kind !== 'fort' && c.ap > 0 && !c.fought && c.org >= 0.6 && g.regions[c.reg].nb.includes(r.id));
    if (!near.length) continue;
    const cut = limitAttackers(g, near, r);
    const pool = cut.length ? footBeside(g, cut, near, r) ?? [] : [];
    if (pool.length && assess(g, pool, r).odds >= reliefBar(g, r)) { resolveBattle(g, pool, r, {}); continue; }
    if (monthsShut(g, r) * 2 < storesOf(g, r)) continue;
    for (const c of near) if (!c.dead && !c.hold) { c.hold = true; kept.push(c); }
  }
  return kept;
}

/**
 * The grand campaign: who stands in the towns. With the month's moves made, a city or a capital the enemy has
 * reached is given a garrison from the infantry standing there, as long as a corps is left in the field; a
 * fortress town gets one too. A great city of the nation's own (ruling 139: its capital, or a city with great works)
 * takes the corps the field can spare against the enemy next door, infantry before the rest, and where the field
 * cannot be held it is filled to its room, a lone corps and all (b512(e)); a staff attacking the countries next door
 * in wars it began garrisons it as any other city until one of them strikes the province (b519(m)). At the start of the month a garrison with no enemy within two marches comes out.
 */
export function garrisons(g: Game, nat: string, post: boolean): void {
  if (!siegeRules(g)) return;
  if (!g.nations[nat].atWar.length) {
    // Ruling 139 as mended (b512(f)): a garrison put in for a war that has ended comes out with the peace.
    if (GREAT.on && !post) for (const r of g.regions) if (r.ctrl === nat && !besieged(g, r)) for (const c of garrisonOf(r).filter((c) => c.nat === nat && !c.fortress)) takeField(g, c);
    return;
  }
  const near = (r: Region, far: number): boolean => {
    let ring = [r.id];
    const seen = new Set(ring);
    for (let d = 0; d < far; d++) {
      const next: number[] = [];
      for (const id of ring) for (const j of g.regions[id].nb) {
        if (seen.has(j)) continue;
        seen.add(j); next.push(j);
        if (hostileUnits(g, g.regions[j], nat).length) return true;
      }
      ring = next;
    }
    return false;
  };
  const attacking = (r: Region): boolean => {
    const foes = [...new Set(r.nb.map((j) => ctl(g, g.regions[j])).filter((x) => atWar(g, nat, x)))];
    if (!foes.length || !foes.every((e) => onTheAttack(g, nat, e))) return false;
    return !foes.some((e) => {
      const since = Math.max(g.flags[`war:${nat}:${e}`] ?? 0, g.flags[`war:${e}:${nat}`] ?? 0);
      return g.reports.some((p) => p.region === r.id && p.attNat === e && p.turn >= since);
    });
  };
  for (const r of g.regions) {
    if (r.ctrl !== nat || besieged(g, r)) continue;
    let inside = garrisonOf(r).filter((c) => c.nat === nat);
    // Ruling 232, riders b587 and b588 (the late garrison): with an enemy field army one march away, a computer staff
    // at the dial's looser stops orders into its capital, or a great city of its own, up to the town's room, its
    // worn corps: those standing in the province (the ones that fell back into it among them), and those in a
    // province next door that can march in this month, the most worn first. Fresh corps stay in the field. It is an
    // order like any garrison (232(d)), its month spent, given when the staff's month opens and again when its moves
    // are made; a beaten army's retreat still never goes inside (232(a)).
    // (Worn is read against the corps' own size, ruling 234's rider b594: a row the record keeps small is not worn at its full.)
    const LATE = SIEGE.ai.lateGarrison;
    if (LATE && driveOut(g) && LATE.stops.includes(stopOf(g)) && r.owner === nat && greatPlace(g, r) && !sacks.has(r.id) && near(r, 1)) {
      const room = roomIn(g, r);
      const worn = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && c.joint === undefined && c.leave === undefined && (c.str / (c.full ?? 1) < LATE.str || c.org < LATE.org);
      const wear = (x: Corps, y: Corps): number => x.str * x.org - y.str * y.org;
      for (const c of r.units.filter(worn).sort(wear)) {
        if (garrisonOf(r).length >= room) break;
        garrison(g, c);
      }
      // (b588: one march wider, worn corps only.)
      const beside = r.nb.map((j) => g.regions[j]).filter((q) => allied(g, nat, ctl(g, q)) && !hostileUnits(g, q, nat).length).flatMap((q) => q.units.filter(worn)).sort(wear);
      for (const c of beside) {
        if (garrisonOf(r).length >= room) break;
        if (moveCorps(g, c, r)) garrison(g, c);
      }
      inside = garrisonOf(r).filter((c) => c.nat === nat);
    }
    if (!post) {
      // (A record garrison with the fortress mark stays in: rider b606.)
      if (inside.length && !near(r, 2)) for (const c of [...inside]) if (!c.fortress) takeField(g, c);
      continue;
    }
    // An attacker does not shut its attackers in (b519(m)): a staff attacking every country next door, in wars it
    // began itself, while the offensive is on, garrisons its great city as it would any other city, until one of
    // them has attacked the province.
    const great = r.owner === nat && greatCity(g, r) && !attacking(r);
    const want = great ? roomIn(g, r) : SIEGE.ai.hold[townSize(g, r)] ?? (r.fort ? 1 : 0);
    // (A place the staff stepped out of this month, because the ring was closing round it, is left no garrison.)
    if (inside.length >= want || !near(r, 1) || sacks.has(r.id)) continue;
    if (great) {
      const all = r.units.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && c.joint === undefined);
      const first = (c: Corps): number => (c.kind === 'inf' ? 0 : 1);
      // The field first (b512(e)): while the field can be held against the enemy next door, only the corps it can
      // spare go inside. Where it cannot be held, the city is filled to its room, a lone corps and all.
      let threat = 0;
      for (const j of r.nb) {
        const e = g.regions[j];
        if (!atWar(g, nat, ctl(g, e))) continue;
        for (const p of hostileUnits(g, e, nat).filter((u) => u.kind !== 'fort').map((u) => atkPower(g, u)).sort((a, b) => b - a).slice(0, frontage(r))) threat += p;
      }
      const need = (SIEGE.ai.great?.field ?? 0) * threat;
      let held = friendlyUnits(g, r, nat).reduce((s, u) => s + defPower(g, u, r), 0);
      const holds = held >= need;
      let have = inside.length;
      for (const c of all.sort((a, b) => first(a) - first(b) || corpsBase(g, a) - corpsBase(g, b))) {
        if (have >= want) break;
        const worth = defPower(g, c, r);
        if (holds && held - worth < need) continue;
        if (garrison(g, c)) { have++; held -= worth; }
      }
      continue;
    }
    const foot = r.units.filter((c) => c.nat === nat && !c.dead && c.kind === 'inf' && c.joint === undefined).sort((a, b) => corpsBase(g, a) - corpsBase(g, b));
    let field = r.units.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng').length, have = inside.length;
    for (const c of foot) {
      if (have >= want || field <= 1) break;
      if (garrison(g, c)) { have++; field--; }
    }
  }
}

/**
 * The grand campaign: the towns this nation's armies stand round. A town is stormed when the odds are good
 * enough, and at shorter odds once the siege has dragged on. Before the month's marching (`after` false), an army
 * too weak to storm a garrisoned town or a city calls in the corps standing next door; and where the town is not
 * stormed, the troops round it stay to keep it shut in. Called again when the month's battles are over (`after`
 * true), it sends the troops that have just won the country round a town straight on into it, if the odds allow:
 * a garrison that has just been beaten in the field is at its weakest.
 */
function sieges(g: Game, nat: string, after = false): void {
  if (!siegeRules(g)) return;
  const n = g.nations[nat];
  // A city or a garrisoned town whose defenders have all gone inside: the army beside it closes round it in one
  // move, and in strength enough to keep the ring against a sortie. One corps sent alone would be thrown out again.
  if (!after) {
    for (const r of g.regions) {
      if (!n.alive) return;
      if (r.units.length || !within(r) || !atWar(g, nat, r.ctrl) || !townHeld(g, r) || lineShut(g, nat, r)) continue;
      const size = townSize(g, r);
      if (!garrisonOf(r).length && size !== 'capital' && size !== 'city') continue;
      const inside = garrisonOf(r).reduce((s, c) => s + c.divs * c.str, 0) + guardOf(g, r);
      const near = r.nb.map((j) => g.regions[j])
        .filter((x) => within(x) && allied(g, nat, fieldCtrl(g, x)) && !besieged(g, x))
        .flatMap((x) => x.units.filter((c) => c.nat === nat && !c.dead && !c.hold && !spared(g, c) && c.kind !== 'fort' && c.kind !== 'eng' && c.joint === undefined && c.leave === undefined && c.org >= 0.35 && c.ap >= Math.min(moveCost(g, c, r), 4) && c.ap > 0 && !blocked(g, c, r)))
        .sort((x, y) => atkPower(g, y) - atkPower(g, x) || x.id - y.id);
      const want = SIEGE.ai.invest * inside;
      if (near.reduce((s, c) => s + c.divs * c.str, 0) < want) continue;
      let have = 0;
      for (const c of near) {
        if (have >= want || !hasRoom(g, r, c)) break;
        if (moveCorps(g, c, r)) have += c.divs * c.str;
        if (!atWar(g, nat, r.ctrl)) break; // it opened its gates
      }
    }
  }
  for (const r of g.regions) {
    if (!n.alive) return;
    if (!r.units.length || !within(r) || !besieged(g, r)) continue;
    const mine = (): Corps[] => r.units.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && atWar(g, nat, r.ctrl));
    if (!mine().length) continue;
    const sat = g.turn - investedOn(g, r);
    // Off the march it storms only what it is sure to carry; after a month round the place, what it probably will;
    // and when the siege has dragged on, it settles for less and at last for a gamble.
    const need = sat >= SIEGE.ai.last.after ? SIEGE.ai.last.odds : sat >= 2 ? SIEGE.ai.late : sat >= 1 ? SIEGE.ai.assault : SIEGE.ai.hasty;
    // By the book (b509(c)): before Leningrad a computer Germany, and any computer ally round the town with it, keeps
    // the ring and does not storm; and the book feeds the town across the lake, so hunger does not take it either.
    const heldBy = stormHeldBy(g, r.name);
    const noStorm = heldBy !== null && (heldBy === nat || allied(g, nat, heldBy));
    // Allies standing round the same town go in with it: besiegers who each fall short alone storm together. The
    // player's own corps are his to order, and a corps lent to another army keeps to that army.
    const friends = (): Corps[] => r.units.filter((c) => c.nat !== nat && c.nat !== g.human && !c.dead && c.kind !== 'fort' && c.joint === undefined && c.leave === undefined && allied(g, c.nat, nat) && atWar(g, c.nat, r.ctrl));
    const pool = (): Corps[] => {
      const own = mine().filter((c) => c.org >= 0.35);
      return own.some((c) => c.kind !== 'eng') ? stormForce(g, [...own, ...friends().filter((c) => c.org >= 0.35)], r, nat) : [];
    };
    const odds = (): number => { const p = pool(); return p.length ? assess(g, p, r, { town: true }).odds : 0; };
    const inside = garrisonOf(r).reduce((s, c) => s + c.divs * c.str, 0) + guardOf(g, r);
    const size = townSize(g, r);
    const prize = garrisonOf(r).length > 0 || size === 'capital' || size === 'city';
    let now = odds();
    // Too weak to storm it, or too few to shut it in: more corps are called up, the nearest first and the strongest
    // of those, until there are enough or there is no more room. The corps next door come at once. Those up to
    // three marches off come too, unless they stand on a front of their own, and are counted while on their way.
    const outside = (): number => mine().reduce((s, c) => s + c.divs * c.str, 0);
    const shut = (): boolean => ringClosed(g, r);
    if (!after && prize && ((!noStorm && now < need) || !shut())) {
      const far = hops(g, r.id, (x) => within(x) && allied(g, nat, fieldCtrl(g, x)));
      const front = (x: Region): boolean => x.nb.some((j) => atWar(g, nat, fieldCtrl(g, g.regions[j])));
      const call = g.corps
        .filter((c) => c.nat === nat && !c.dead && !c.town && c.reg !== r.id && far[c.reg] >= 1 && far[c.reg] <= 3 && !c.hold && !spared(g, c) && c.kind !== 'fort' && c.kind !== 'eng' && c.joint === undefined && c.leave === undefined && c.ap > 0
          && !besieged(g, g.regions[c.reg]) && (far[c.reg] === 1 ? c.ap >= Math.min(moveCost(g, c, r), 4) : !front(g.regions[c.reg])))
        .sort((x, y) => far[x.reg] - far[y.reg] || atkPower(g, y) - atkPower(g, x) || x.id - y.id);
      // What is on its way counts for what it will be worth when it arrives.
      const force = stormForce(g, mine(), r);
      const a0 = force.length ? assess(g, force, r, { town: true }) : null;
      let coming = 0, power = 0, men = 0;
      for (const c of call) {
        const enough = (noStorm || (a0 && a0.D > 0 ? Math.max(now, (a0.A + power) / a0.D) >= need : now >= need)) && (shut() || outside() + men >= SIEGE.ring * inside);
        if (enough || mine().filter((u) => u.kind !== 'eng').length + coming >= STACK) break;
        if (far[c.reg] === 1) {
          if (!hasRoom(g, r, c)) continue;
          if (moveCorps(g, c, r)) now = odds();
        } else {
          walkToward(g, c, r);
          if (c.reg === r.id) now = odds(); else { coming++; power += atkPower(g, c); men += c.divs * c.str; }
        }
        if (!besieged(g, r)) break; // it opened its gates to the bigger army
      }
    }
    if (!besieged(g, r)) continue;
    if (!noStorm && now >= need && !lineShut(g, nat, r) && storm(g, pool(), r)?.taken) continue;
    if (!besieged(g, r) || after) continue;
    // Not stormed: the siege goes on. Round a prize every corps stays; round a small place held by its guard, enough to shut it in.
    // A port that ships can feed is not starved by waiting: after a season round one only the ring stays, and the
    // rest are free to overrun the country behind it (overrun, below) or to go where the staff wants them.
    const fed = prize && sat >= SIEGE.ai.sea.after && (feeders(g, r).length > 0 || fedByBook(g, r));
    const fast = (c: Corps): number => (KIND[c.kind].mobile ? 1 : 0);
    let have = 0;
    for (const c of mine().filter((u) => u.kind !== 'eng').sort((x, y) => fast(x) - fast(y) || atkPower(g, x) - atkPower(g, y))) {
      if ((!prize || fed) && have > 0 && have >= SIEGE.ring * inside) break;
      c.ap = 0;
      have += c.divs * c.str;
    }
  }
}

/**
 * The grand campaign: the country behind a port this nation has stood round for a season and cannot starve, because
 * ships feed it. Corps with nothing in front of them walk into its undefended ground, the richest first, a province
 * a month, until enough of the country is lost for its government to give in. Troops on a front of their own stay there.
 */
function overrun(g: Game, nat: string): void {
  if (!siegeRules(g)) return;
  const lands = new Set<string>();
  for (const r of g.regions) {
    if (!within(r) || !besieged(g, r) || !atWar(g, nat, r.ctrl) || g.turn - investedOn(g, r) < SIEGE.ai.sea.after || !feeders(g, r).length) continue;
    if (r.units.some((c) => c.nat === nat && !c.dead)) lands.add(r.ctrl);
  }
  if (!lands.size) return;
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.town || c.hold || spared(g, c) || c.ap <= 0 || c.kind === 'fort' || c.kind === 'eng' || c.joint !== undefined || c.leave !== undefined) continue;
    const here = g.regions[c.reg];
    if (!within(here)) continue;
    // An enemy army next door is a front: nobody leaves it to gather provinces.
    if (!besieged(g, here) && here.nb.some((j) => hostileUnits(g, g.regions[j], nat).length > 0)) continue;
    let best: Region | null = null, bv = -1;
    for (const j of here.nb) {
      const x = g.regions[j];
      if (!within(x) || !lands.has(x.ctrl) || x.owner !== x.ctrl || !atWar(g, nat, x.ctrl) || x.units.length || besieged(g, x)) continue;
      if (blocked(g, c, x) || c.ap < Math.min(moveCost(g, c, x), 4) || !hasRoom(g, x, c)) continue;
      const v = x.vp + (x.cap ? 5 : 0) + x.fac * 0.2;
      if (v > bv) { bv = v; best = x; }
    }
    if (best) moveCorps(g, c, best);
    if (!g.nations[nat].alive) return;
  }
}

/** The grand campaign: a garrison shut in by an army it can beat comes out and drives it off. */
function sorties(g: Game, nat: string): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (!g.nations[nat].alive) return;
    if (!r.garr || !within(r) || !besieged(g, r) || whyNotSally(g, r, nat)) continue;
    const a = previewSally(g, r, nat);
    if (a && a.odds >= SIEGE.ai.sally) sally(g, r, nat);
  }
}

function campaignMoves(g: Game, nat: string): void {
  const n = g.nations[nat];
  // An army overseas fights its own war to its own plan. It stands aside while the main plan is
  // carried out, and the main army stands aside in its turn.
  const away = g.corps.filter((c) => c.nat === nat && !c.hold && theatreOf(g, c) >= 0);
  // (The floor at home is measured against the whole field army, those overseas included: ruling 229, 1b.)
  countField(g, nat);
  for (const c of away) c.hold = true;
  try { mainCampaign(g, nat); } finally { for (const c of away) c.hold = false; }
  if (n.alive) overseas(g, nat);
}

/**
 * A phase's 'release': the held corps standing on the ground it names lose their hold and go in with the rest. A
 * garrison row marked to stay keeps it (ruling 221, H): an offensive does not empty the bases behind it.
 */
export function release(g: Game, nat: string, names: string[]): void {
  for (const c of g.corps) if (c.nat === nat && c.hold && !c.stay && c.kind !== 'fort' && names.includes(g.regions[c.reg].name)) c.hold = false;
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
    if (ph?.release) release(g, nat, ph.release);
    const rest = g.corps.filter((c) => c.nat === nat && !c.hold && !ids[k].has(c.reg));
    for (const c of rest) c.hold = true;
    scope = ids[k];
    quiet = !!ph?.quiet;
    single = ph?.renew === false;
    waves = ph?.waves ?? 2;
    striking = ph?.effort !== undefined;
    leaving = ph?.withdraw ?? null;
    given = new Set((ph?.abandon ?? []).map((nm) => g.byName[nm]?.id).filter((id): id is number => id !== undefined && ids[k].has(id)));
    entrusted = handedOver(g, nat, ph);
    masked = new Set((ph?.mask ?? []).map((nm) => g.byName[nm]?.id).filter((id): id is number => id !== undefined));
    try {
      sorties(g, nat);
      sieges(g, nat);
      overrun(g, nat);
      // The plan's standing orders come first: the force it keeps at a place stays, and ground it gives up is left,
      // the army falling back along the road to the base that feeds it.
      hold(g, nat, ph);
      const base = g.byName[ph?.base ?? T.bases?.[nat] ?? ''];
      if (base) fallBack(g, nat, base.id);
      // Whoever the plan has named so far, and anyone at war with this nation who is on the theatre's ground.
      const enemies = new Set<string>();
      for (const p of phases) if (p.from <= g.turn) for (const e of p.enemies ?? []) if (n.atWar.includes(e)) enemies.add(e);
      for (const id of ids[k]) { const r = g.regions[id]; if (n.atWar.includes(r.ctrl)) enemies.add(r.ctrl); }
      // An army overseas brings its guns up as the main army does: the guns go where the works are (the grand campaign).
      if (enemies.size) gunsUp(g, nat, enemies, ph?.targets ? new Set(ph.targets) : null);
      if (ph?.posture === 'attack' && enemies.size) {
        const only = ph.targets ? new Set(ph.targets) : null;
        // Nothing outside the theatre, and nothing the plan has not named, draws this army on.
        const halt = new Set([...(ph.siege ?? []), ...(ph.mask ?? [])]), edge = new Set(halt);
        for (const r of g.regions) {
          if (!ids[k].has(r.id)) { halt.add(r.name); edge.add(r.name); }
          else if (only && !only.has(r.name)) halt.add(r.name);
        }
        // (The month a main effort is named the army masses and attacks nowhere: the word of it runs ahead of the blow.)
        if (!ph.massing) {
          softenLine(g, nat, enemies, halt, only, ph.minOdds ?? 1.3, ph.oddsFor);
          offensive(g, nat, ph.objectives ?? [], enemies, ph.minOdds ?? 1.3, [], true, true, halt, only, ph.oddsFor);
        }
        // With the month's fighting done the rest of the army closes up to wherever the enemy now stands.
        if (n.alive) { advance(g, nat, enemies, 0, null, edge); mass(g, nat, ph); defend(g, nat, false); }
      } else { gather(g, nat, ph); mass(g, nat, ph); defend(g, nat); }
    } finally {
      // The troops that have just won the country round a town go straight on into it, if the odds allow.
      if (n.alive) sieges(g, nat, true);
      scope = null;
      quiet = false;
      single = false;
      waves = 2;
      striking = false;
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
  shoreAI(g, nat);
  airAI(g, nat, ph);
  if (open) homeAgain(g, nat);
  // A nation at peace gives its army no orders, unless its staff is bringing it up for a war of its own choosing.
  if (!n.atWar.length && !(open && ph?.posture === 'redeploy')) return;
  if (ph?.landings) landings(g, nat, ph);
  if (open) { descent(g, nat); ferry(g, nat); reinforce(g, nat); }
  if (ph?.release) release(g, nat, ph.release);
  const posture = ph?.posture ?? 'defend';
  entrusted = handedOver(g, nat, ph);
  given = new Set((ph?.abandon ?? []).map((nm) => g.byName[nm]?.id).filter((id): id is number => id !== undefined));
  quiet = !!ph?.quiet;
  single = ph?.renew === false;
  waves = ph?.waves ?? 2;
  bound = ph?.strict ? new Set(ph.targets ?? []) : null;
  near = ph?.near ?? null;
  // (By the book, a raid whose months are over comes back inside the record's line, before the month's fighting and after it.)
  lineHome(g, nat, g.turn);
  try { fight(g, nat, ph, posture); lineHome(g, nat, g.turn + 1); } finally { entrusted = new Set(); massed = new Set(); given = new Set(); quiet = false; single = false; waves = 2; bound = null; near = null; }
}

/**
 * By the book, when a raid's months are over its corps go back inside the record's line (ruling 215(c)): every corps
 * of the invader's side standing on ground the line leaves out in month `t` marches for the nearest ground of its own
 * side's inside the line. At the start of the month the ground they have left goes back (ruling 219, lineGiveBack).
 */
export function lineHome(g: Game, nat: string, t: number): void {
  if (!lineAny(g, nat)) return;
  if (t === g.turn) lineGiveBack(g, nat);
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort' || !lineLeaves(g, nat, g.regions[c.reg], t)) continue;
    if (c.town !== undefined && !takeField(g, c)) continue;
    c.hold = false;
    const ours = (r: Region): boolean => allied(g, nat, ctl(g, r));
    const d = hops(g, c.reg, ours);
    let goal: Region | null = null;
    for (const r of g.regions) if (d[r.id] > 0 && allied(g, nat, r.ctrl) && !lineLeaves(g, nat, r, t) && (!goal || d[r.id] < d[goal.id])) goal = r;
    // With no road home inside the line, it is taken out by sea (the director, 18:59Z): under F to the nearest
    // harbour its country holds there inside the line, or else home. Never stranded beyond the line.
    if (!goal) {
      const here = g.regions[c.reg];
      const to = (EASTERN.nearRefuge ? nearHarbour(g, nat, here, (r) => !lineLeaves(g, nat, r, t)) : null) ?? (g.nations[nat].capital >= 0 ? g.regions[g.nations[nat].capital] : null);
      if (to && to.id !== here.id) takeOut(g, c, to);
      continue;
    }
    const back = hops(g, goal.id, ours);
    for (let step = 0; step < 6 && c.ap > 0 && c.reg !== goal.id; step++) {
      let next: Region | null = null;
      for (const j of g.regions[c.reg].nb) {
        if (back[j] < 0 || back[j] >= back[c.reg] || friendlyUnits(g, g.regions[j], nat).length >= STACK) continue;
        if (!next || back[j] < back[next.id]) next = g.regions[j];
      }
      if (!next || !moveTo(g, c, next)) break;
    }
  }
}

/**
 * Raids go back (ruling 219): ground beyond the record's line this month with nobody of the invader's side left on it
 * returns to whoever held it before the raid, as the record's withdrawals did: as if his army had walked back in, so
 * the province changes hands by the rules a march into an empty province follows (siege.ts, arrive). Computer against
 * computer only. Returns the provinces that went back.
 */
export function lineGiveBack(g: Game, nat: string): Region[] {
  const back: Region[] = [];
  for (const { r, to } of lineGives(g, nat, g.turn)) if (arrive(g, r, to)) back.push(r);
  return back;
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
  // Towns under siege are dealt with first, while the troops round them have the whole month in hand.
  sorties(g, nat);
  sieges(g, nat);
  overrun(g, nat);
  if (!n.alive) return;
  try { fightOn(g, nat, ph, posture); } finally { if (n.alive) sieges(g, nat, true); }
}

function fightOn(g: Game, nat: string, ph: PlanPhase | null, posture: string): void {
  const n = g.nations[nat];
  hold(g, nat, ph);
  // An ally's capital on the ward has its guard before anything else is settled (ruling 233, riders b612 and b627).
  keepWards(g, nat, ph);
  // The coast watch is set before anybody marches for the front: a corps that has gone forward is not called back.
  mass(g, nat, ph, true);
  breakOut(g, nat);
  if (!n.alive) return;
  stepBack(g, nat);
  fallBack(g, nat);
  relieve(g, nat);
  if (posture === 'defend') {
    gather(g, nat, ph); mass(g, nat, ph);
    if (!scope && isOpen(g)) localAttacks(g, nat);
    if (n.alive) defend(g, nat);
    return;
  }
  const enemies = enemySet(g, nat);
  const follow = posture === 'attack' && !ph!.force;
  const halt = new Set([...(ph && g.turn === ph.from ? ph.haltOrder ?? [] : []), ...(ph?.siege ?? [])]);
  const only = ph?.targets ? new Set(ph.targets) : ph?.limited ? new Set(objectivesFor(g, nat)) : null;
  gunsUp(g, nat, enemies, only);
  // (The month a main effort is named the army masses and attacks nowhere: the word of it runs ahead of the blow.)
  if (!ph?.massing) {
    softenLine(g, nat, enemies, halt, only, ph?.minOdds ?? 1.3, ph?.oddsFor);
    offensive(g, nat, objectivesFor(g, nat), enemies, ph?.minOdds ?? 1.3, ph?.axis ?? [], g.turn > (ph?.from ?? 0), follow, halt, only, ph?.oddsFor);
  }
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
    standing(g, nat, r).filter((u) => watchman(ph, name, u) && !commandAtHome(g, nat, r, u)).slice(0, count).forEach((u, k) => { if (!KIND[u.kind].mobile || k < 2) massed.add(u.id); });
  }
}

/**
 * The ward's guard (ruling 233, riders b612 and b627): the field corps a staff keeps for the capital of an ally whose
 * leaving is in play. A guard is named once (flags, 'ward:') and kept until the ward ends. Each month the trains take
 * it to that ally's home ground, as near the capital as they run; with no line open onto that ground it marches when
 * the ground is within two marches, and otherwise rides as near the capital as the trains run and marches on from the
 * end of the line. Where the capital is full of the ally's own corps it stands on the nearest ground that has room.
 * Named first is whoever of the field army already stands on that ally's home ground; then the infantry that can
 * stand on it soonest: one the trains set down on it this month or one within two marches of it, and after those
 * whoever the trains bring nearest the capital. Nobody is taken who stands alone, and of two as near the one standing
 * three or more to a place goes, so that no stretch of a front is left thin for it.
 * `held` is the month's list of corps the plan already keeps: nobody on it is taken, and the guard goes onto it.
 * (As a watched harbour's the guard was sent afresh each month, and only where a train could carry it the whole way
 * into a town with room. With Rome's five places taken by Italy's own guard, or no line open into Italy that month,
 * no German corps stood on Italian ground at the signing in 34 wars of the 86 where the ward was set.)
 */
export function keepWards(g: Game, nat: string, ph: PlanPhase | null, held: Set<number> = massed): void {
  for (const [name, count] of Object.entries(ph?.ward ?? {})) {
    const cap = g.byName[name];
    if (!cap || !allied(g, nat, cap.ctrl)) continue;
    const d = hops(g, cap.id, (r) => allied(g, nat, ctl(g, r)));
    const own = (r: Region): number => r.units.filter((u) => u.nat === nat && u.kind !== 'fort').length;
    const home = (r: Region): boolean => r.owner === cap.owner && !r.overseas;
    /** The ally's home ground is within two marches of here, over ground this side holds. */
    const near = (r: Region): boolean => r.nb.some((j) => d[j] >= 0 && (home(g.regions[j]) || g.regions[j].nb.some((k) => d[k] >= 0 && home(g.regions[k]))));
    /** Where the trains would set a corps down this month: on the ally's home ground before anywhere else, and the nearest the capital. */
    const ride = (c: Corps): Region | null => {
      let best: Region | null = null;
      if (c.ap >= 4) for (const id of railReach(g, c)) {
        const r = g.regions[id];
        if (d[id] < 0 || !hasRoom(g, r, c)) continue;
        if (!best || (home(r) !== home(best) ? home(r) : d[id] < d[best.id])) best = r;
      }
      return best;
    };
    const named = new Set<number>();
    const free = (c: Corps): boolean =>
      c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && c.divs >= 2 && !c.town && !c.hold && !c.dyle && c.joint === undefined && c.leave === undefined &&
      d[c.reg] >= 0 && !held.has(c.id) && !named.has(c.id);
    for (let k = 0; k < count; k++) {
      const key = `ward:${nat}:${name}:${k}`;
      let c = g.corps.find((u) => u.id === g.flags[key] && u.nat === nat && !u.dead && d[u.reg] >= 0 && !named.has(u.id));
      if (!c) {
        c = g.corps.filter((u) => free(u) && home(g.regions[u.reg])).sort((a, b) => d[a.reg] - d[b.reg] || a.id - b.id)[0];
        if (!c) {
          // How soon each could stand on the ally's ground: this month (0) or later (1), and how near the capital its month ends.
          const rank = g.corps.filter((u) => free(u) && u.kind === 'inf' && own(g.regions[u.reg]) >= 2).map((u) => {
            const here = g.regions[u.reg], to = ride(u);
            const soon = (to && home(to)) || near(here);
            return { u, late: soon ? 0 : 1, end: to && (home(to) || !soon) ? Math.min(d[to.id], d[u.reg]) : d[u.reg], thin: own(here) >= 3 ? 0 : 1 };
          });
          c = rank.sort((a, b) => a.late - b.late || a.end - b.end || a.thin - b.thin || d[a.u.reg] - d[b.u.reg] || a.u.id - b.u.id)[0]?.u;
        }
        if (!c) continue;
        g.flags[key] = c.id;
      }
      named.add(c.id);
      held.add(c.id);
      if (c.reg === cap.id) continue;
      const here = g.regions[c.reg], to = ride(c);
      const train = !!to && (home(here) ? home(to) && d[to.id] < d[here.id] : home(to) || (!near(here) && d[to.id] < d[here.id]));
      if (train && railMove(g, c, to!)) continue;
      if (c.ap > 0) walkToward(g, c, cap);
    }
  }
}

/** A home command that holds in place, on a quay a floor binds (ruling 229, 1b): it never sails, so it fills no place in a force that would. */
const commandAtHome = (g: Game, nat: string, r: Region, u: Corps): boolean => u.hold && leavesHome(g, nat, r);

/** Whether a corps counts toward the force kept at a staging place: on the coast watch, only a corps of the field army of two divisions or more, and not the town's standing garrison. */
const watchman = (ph: PlanPhase, name: string, u: Corps): boolean => !ph.watch?.includes(name) || (!u.hold && u.divs >= 2 && u.kind !== 'eng');

/**
 * Brings each staging place up to the force the plan wants there: while there are too few, the nearest corps of
 * infantry that can be spared marches to join them. Nobody is taken from a place he holds alone. With `watchOnly`,
 * only the harbours on the coast watch.
 */
function mass(g: Game, nat: string, ph: PlanPhase | null, watchOnly = false): void {
  if (!ph?.staging || ph.posture === 'redeploy') return;
  const own = (r: Region): Corps[] => r.units.filter((u) => u.nat === nat && u.kind !== 'fort');
  // (The quay for an enemy on the staff's own side of the ocean: its force comes by rail, as the coast watch does. Version 81.)
  const door = !watchOnly && isOpen(g) ? doorQuay(g, nat) : null;
  // With no enemy ashore and none across a land frontier, the army at home has no front to thin: for that quay a
  // corps may leave its place empty behind it, unless the place is on the coast watch or is itself a staging place.
  const loose = !!door && !enemyAtTheDoor(g, nat);
  const spare = (c: Corps, least: number): boolean => {
    const at = g.regions[c.reg];
    return own(at).length >= least || (loose && !ph.watch?.includes(at.name) && !(at.name in ph.staging!));
  };
  for (const [name, all] of Object.entries(ph.staging)) {
    if (watchOnly && !ph.watch?.includes(name)) continue;
    // The watch itself first, one harbour after another: a landing force that gathers at one of them waits its turn.
    const count = watchOnly ? Math.min(all, coastRules()?.each ?? 1) : all;
    const r = g.byName[name];
    if (!r || !allied(g, nat, r.ctrl)) continue;
    let have = own(r).filter((u) => massed.has(u.id)).length;
    if (have >= count) continue;
    for (const u of standing(g, nat, r)) if (have < count && !massed.has(u.id) && watchman(ph, name, u) && !commandAtHome(g, nat, r, u) && (!KIND[u.kind].mobile || have < 2)) { massed.add(u.id); have++; }
    const d = hops(g, r.id, (x) => allied(g, nat, ctl(g, x)));
    const first = door?.name === name;
    const pool = g.corps
      .filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && !KIND[c.kind].mobile && c.ap > 0 && !massed.has(c.id) && c.reg !== r.id && d[c.reg] > 0 && d[c.reg] <= 3 && (own(g.regions[c.reg]).length >= 2 || (first && spare(c, 2))))
      .sort((a, b) => d[a.reg] - d[b.reg] || atkPower(g, b) - atkPower(g, a));
    for (const c of pool) {
      if (have >= count || friendlyUnits(g, r, nat).length >= STACK) break;
      if (own(g.regions[c.reg]).length < 2 && !(first && spare(c, 2))) continue;
      walkToward(g, c, r);
      if (c.reg === r.id) { massed.add(c.id); have++; }
    }
    // A harbour on the coast watch with nobody near enough to march there is sent a corps by rail: the nearest
    // that stands three or more to a place (so that no stretch of a front is left thin for it) and that the trains
    // can carry the whole way. Where none can, the nearest goes as far as the line runs and marches on next month.
    if (first) {
      // The quay for the first business (Version 81) is filled by rail in the one month where the trains can do it:
      // the nearest infantry that can be spared, as many as the force still wants. Whoever the trains cannot carry
      // the whole way goes as far as the line runs and marches on.
      const far = g.corps
        .filter((c) => c.nat === nat && !c.dead && !c.hold && c.kind === 'inf' && c.ap >= 4 && !massed.has(c.id) && d[c.reg] > 3 && spare(c, 2))
        .sort((a, b) => d[a.reg] - d[b.reg] || a.id - b.id);
      for (const c of far) {
        if (have >= count || friendlyUnits(g, r, nat).length >= STACK) break;
        if (!spare(c, 2)) continue;
        if (railMove(g, c, r)) { massed.add(c.id); have++; }
        else if (railToward(g, c, r)) have++;
      }
    } else if (have < count && !pool.length && ph.watch?.includes(name) && friendlyUnits(g, r, nat).length < STACK) {
      const far = g.corps
        .filter((c) => c.nat === nat && !c.dead && !c.hold && c.kind === 'inf' && c.ap >= 4 && !massed.has(c.id) && d[c.reg] > 3 && own(g.regions[c.reg]).length >= 3)
        .sort((a, b) => d[a.reg] - d[b.reg] || a.id - b.id);
      const got = far.slice(0, 12).find((c) => railMove(g, c, r));
      if (got) { massed.add(got.id); have++; }
      else if (far[0]) railToward(g, far[0], r);
    }
  }
}

/**
 * Gives up the ground the plan has written off: every corps standing on it marches toward home by the shortest
 * road over friendly country, as far as the month allows. Those nearest home go first and leave room behind them.
 */
function fallBack(g: Game, nat: string, cap = g.nations[nat].capital): void {
  if (!given.size || cap < 0 || !allied(g, nat, ctl(g, g.regions[cap]))) return;
  const home = hops(g, cap, (r) => allied(g, nat, ctl(g, r)) && within(r));
  const leaving = g.corps
    .filter((c) => c.nat === nat && c.kind !== 'fort' && !c.hold && given.has(c.reg) && home[c.reg] > 0)
    .sort((a, b) => home[a.reg] - home[b.reg]);
  // Where the one road home is full of the nation's own troops, one of them goes a stage further to make room.
  const makeRoom = (r: Region, depth = 0): boolean => {
    if (friendlyUnits(g, r, nat).length < STACK) return true;
    if (depth > 3) return false;
    const movers = r.units.filter((u) => u.nat === nat && !u.hold && u.kind !== 'fort' && u.ap > 0);
    const nexts = r.nb.map((j) => g.regions[j]).filter((x) => allied(g, nat, ctl(g, x)) && home[x.id] >= 0 && home[x.id] < home[r.id]).sort((a, b) => friendlyUnits(g, a, nat).length - friendlyUnits(g, b, nat).length);
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
        if (!allied(g, nat, ctl(g, r)) || home[j] < 0 || home[j] >= home[c.reg] || friendlyUnits(g, r, nat).length >= STACK || blocked(g, c, r)) continue;
        // Out of the ground being given up if there is a way, and by the emptier road.
        const worse = (x: Region): number => (given.has(x.id) ? 10 : 0) + friendlyUnits(g, x, nat).length;
        if (!to || worse(r) < worse(to)) to = r;
      }
      if (!to) to = g.regions[c.reg].nb.map((j) => g.regions[j]).find((r) => allied(g, nat, ctl(g, r)) && home[r.id] >= 0 && home[r.id] < home[c.reg] && makeRoom(r)) ?? null;
      if (!to || c.ap < Math.min(moveCost(g, c, to), 4) || !moveCorps(g, c, to)) break;
    }
  });
}

/**
 * A force that has been ringed tries to get back to its own lines (the grand campaign, the staffs named in
 * rivals.json under `ring`). A place with the staff's field corps in it and no road home over friendly ground is
 * cut off. Where ground that still joins the country lies within a few marches the corps make for it by the
 * shortest way. Through a province the enemy holds but has left empty one corps marches, taking it, and the road
 * is open again; where he stands on the way, the ringed corps next to him attack together with any of the army
 * outside that stand next to him, if the odds are good enough, and go through if they win. Corps deeper in the ring close up behind. Whoever the plan keeps at a place
 * stays, and so does a force with no way it can take.
 */
function breakOut(g: Game, nat: string): void {
  const R = RIVALS.ring;
  const n = g.nations[nat];
  if (!R || scope || !rivalRules(g) || nat === g.human || !R.staffs.includes(nat) || n.capital < 0) return;
  const friendly = (x: Region): boolean => allied(g, nat, ctl(g, x));
  const field = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && !c.hold && !c.dyle && c.joint === undefined && !massed.has(c.id) && c.ap > 0;
  const tried = new Set<number>();
  for (let round = 0; round < 2 * R.reach && n.alive; round++) {
    if (!friendly(g.regions[n.capital])) return;
    ensureSupply(g);
    const home = hops(g, n.capital, friendly);
    const cut = g.regions.filter((r) => home[r.id] < 0 && friendly(r) && !aside(g, r, nat) && r.units.some(field));
    if (!cut.length) return;
    // How many marches each place lies from the country, over ground that is friendly or an enemy's.
    const far = new Int16Array(g.regions.length).fill(-1);
    const q: number[] = [];
    for (const r of g.regions) if (home[r.id] >= 0) { far[r.id] = 0; q.push(r.id); }
    for (let i = 0; i < q.length; i++) {
      if (far[q[i]] >= R.reach) continue;
      for (const j of g.regions[q[i]].nb) {
        const x = g.regions[j];
        if (far[j] >= 0 || !(friendly(x) || atWar(g, nat, ctl(g, x)))) continue;
        far[j] = far[q[i]] + 1; q.push(j);
      }
    }
    let did = false;
    for (const r of cut.sort((a, b) => far[a.id] - far[b.id] || a.id - b.id)) {
      if (far[r.id] < 0) continue;
      const nexts = r.nb.map((j) => g.regions[j]).filter((x) => far[x.id] >= 0 && far[x.id] < far[r.id]);
      const corps = r.units.filter(field).sort((a, b) => b.ap - a.ap || a.id - b.id);
      // A way with nobody on it: friendly ground nearer the country, or a province the enemy has left empty.
      const open = nexts.filter((x) => !hostileUnits(g, x, nat).length).sort((a, b) => (friendly(a) ? 0 : 1) - (friendly(b) ? 0 : 1) || a.id - b.id);
      let moved = false;
      for (const x of open) {
        // One corps through an empty province opens the road, and the rest wait to see it held. Over ground still
        // friendly the whole force closes up toward the country.
        const ours = friendly(x);
        for (const c of corps) {
          if (blocked(g, c, x) || !hasRoom(g, x, c) || c.ap < Math.min(moveCost(g, c, x), 4)) continue;
          if (moveCorps(g, c, x)) { moved = true; if (!ours) break; }
        }
        if (moved) break;
      }
      if (moved) { did = true; break; }
      // The enemy stands on every way: the weakest place on the road home is attacked by everyone next to it,
      // the ringed corps from inside and whoever of the army stands next to it outside.
      let best: { x: Region; pool: Corps[]; odds: number } | null = null;
      for (const x of nexts) {
        if (tried.has(x.id) || !hostileUnits(g, x, nat).length || lineShut(g, nat, x)) continue;
        const near = x.nb.map((j) => g.regions[j]).filter((y) => friendly(y) && !aside(g, y, nat)).flatMap((y) => y.units.filter(field)).filter((c) => !blocked(g, c, x) && c.org >= 0.35 && (home[c.reg] < 0 || !spared(g, c))); // (a worn corps outside the ring, one its staff rests, is not thrown in)
        const pool = limitAttackers(g, near, x);
        if (!pool.length) continue;
        const odds = assess(g, pool, x).odds;
        if (odds >= R.odds && (!best || odds > best.odds)) best = { x, pool, odds };
      }
      if (best) { tried.add(best.x.id); resolveBattle(g, best.pool, best.x); did = true; break; }
    }
    if (!did) return;
  }
}

/**
 * A staff that will not be ringed (the grand campaign, the staffs named in rivals.json under `ring`). Before the
 * month's operations it looks at each place of its own that faces the enemy. The ways out of a place are the
 * friendly places next door that lie no further from the capital. Where few are left and none of them is safe (no
 * enemy next to it, or held well enough against what stands there), the ring is closing: the place is given up, and
 * so is any place whose only ways out run through ground being given up. A garrison comes out of the town and the
 * field corps march out by the way with room nearest home, a stage further where they come to more ground being
 * given up. The capital is never given up, a force the plan keeps massed stays, and where no way out is left the
 * corps stand and fight where they are.
 */
function stepBack(g: Game, nat: string): void {
  const R = RIVALS.ring;
  const n = g.nations[nat];
  if (!R || scope || !rivalRules(g) || nat === g.human || !R.staffs.includes(nat) || n.capital < 0 || !allied(g, nat, ctl(g, g.regions[n.capital]))) return;
  ensureSupply(g);
  const friendly = (x: Region): boolean => allied(g, nat, ctl(g, x));
  const home = hops(g, n.capital, friendly);
  const press = (r: Region): number => {
    let t = 0;
    for (const j of r.nb) {
      const e = g.regions[j];
      if (!atWar(g, nat, ctl(g, e))) continue;
      const foes = hostileUnits(g, e, nat).filter((u) => u.kind !== 'fort').map((u) => atkPower(g, u)).sort((a, b) => b - a);
      for (const p of foes.slice(0, frontage(r))) t += p;
    }
    return t;
  };
  const stand = (r: Region): number => friendlyUnits(g, r, nat).reduce((sum, u) => sum + defPower(g, u, r), 0);
  const ways = (r: Region): Region[] => r.nb.map((j) => g.regions[j]).filter((x) => friendly(x) && !hostileUnits(g, x, nat).length && home[x.id] >= 0 && home[x.id] <= home[r.id]);
  const safe = (x: Region): boolean => { if (given.has(x.id)) return false; const t = press(x); return t === 0 || stand(x) >= R.safe * t; };
  // The staff's own ground, and an ally's where its corps stand in the field.
  const own = g.regions.filter((r) => friendly(r) && !aside(g, r, nat) && (ctl(g, r) === nat || r.owner === nat || r.units.some((u) => u.nat === nat && u.kind !== 'fort')) && r.id !== n.capital && home[r.id] > 0 && !entrusted.has(r.id));
  const found = new Set<number>();
  for (let pass = 0; pass < 4; pass++) {
    let more = false;
    for (const r of own) {
      if (given.has(r.id)) continue;
      const w = ways(r);
      if (!w.length || w.length > R.exits || w.some(safe)) continue;
      // A place with no enemy next to it goes only when every way out of it runs through ground being given up.
      if (press(r) === 0 && !w.every((x) => given.has(x.id))) continue;
      given.add(r.id); found.add(r.id); more = true;
    }
    if (!more) break;
  }
  if (!found.size) return;
  for (const id of found) {
    sacks.add(id);
    for (const c of [...garrisonOf(g.regions[id])]) if (c.nat === nat) takeField(g, c);
  }
  // Those nearest home go first and leave room behind them.
  const leaving = g.corps
    .filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && !c.hold && !c.dyle && c.joint === undefined && !massed.has(c.id) && found.has(c.reg))
    .sort((a, b) => home[a.reg] - home[b.reg] || a.id - b.id);
  for (const c of leaving) {
    for (let step = 0; step < R.steps && c.ap > 0 && given.has(c.reg); step++) {
      let to: Region | null = null;
      const worse = (x: Region): number => (given.has(x.id) ? 1000 : 0) + 10 * home[x.id] + friendlyUnits(g, x, nat).length;
      for (const x of ways(g.regions[c.reg])) {
        if (friendlyUnits(g, x, nat).length >= STACK || blocked(g, c, x) || c.ap < Math.min(moveCost(g, c, x), 4)) continue;
        if (!to || worse(x) < worse(to)) to = x;
      }
      if (!to || !moveCorps(g, c, to)) break;
    }
  }
}

/**
 * A staff that rests its worn corps (the grand campaign, the staffs named in rivals.json under `relief`; a judgement
 * of the staff's, not a rule of the game). A corps under `worn` of its strength that stands in the line comes out
 * of it before the month's operations, where another corps stays to hold the place and a quiet place lies behind:
 * a friendly place with no enemy beside it, one march away or two, the one nearest home with room. A worn corps is
 * thrown into no attack that month whether it got out or not. Out of the line it rests, and the reserves leave it
 * alone, until the drafts have brought it back to `fit` or it has had `months` months of it; after that it is at
 * the staff's disposal again and is not rested a second time for as many months more.
 */
function relieve(g: Game, nat: string): void {
  const R = RIVALS.relief;
  const n = g.nations[nat];
  if (!R || !relieves(g, nat) || n.capital < 0 || !allied(g, nat, ctl(g, g.regions[n.capital]))) return;
  const friendly = (x: Region): boolean => allied(g, nat, ctl(g, x));
  const foeAt = (x: Region): boolean => hostileUnits(g, x, nat).length > 0;
  const inLine = (x: Region): boolean => x.nb.some((j) => foeAt(g.regions[j]));
  const home = hops(g, n.capital, friendly);
  const room = (x: Region, c: Corps, from: number): boolean => friendly(x) && home[x.id] >= 0 && !given.has(x.id) && !foeAt(x) && friendlyUnits(g, x, nat).length < STACK && !blocked(g, c, x, from);
  const quiet = (x: Region): boolean => !inLine(x);
  const field = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && c.town === undefined;
  const mine = (c: Corps): boolean => field(c) && !c.hold && !c.dyle && c.joint === undefined && c.leave === undefined && !massed.has(c.id) && !aside(g, g.regions[c.reg], nat);
  const key = (c: Corps): string => `rl:${c.id}`;
  const keep = (c: Corps): void => { c.hold = true; rested.push(c); };
  // The weakest first: where two worn corps stand together the stronger stays to hold the place.
  for (const c of g.corps.filter(mine).sort((a, b) => a.str - b.str || a.id - b.id)) {
    const since = g.flags[key(c)];
    if (since !== undefined && (c.str >= R.fit || g.turn - since >= 2 * R.months)) delete g.flags[key(c)];
    if (!wornOut(g, c)) continue; // fit to fight, or it has had its rest
    const here = g.regions[c.reg];
    if (inLine(here) && c.ap > 0 && here.units.some((u) => u !== c && field(u) && !rested.includes(u))) {
      // The quiet place nearest home, one march away or two.
      let way: Region[] | null = null;
      const better = (x: Region): boolean => !way || home[x.id] < home[way[way.length - 1].id] || (home[x.id] === home[way[way.length - 1].id] && friendlyUnits(g, x, nat).length < friendlyUnits(g, way[way.length - 1], nat).length);
      for (const j of here.nb) {
        const x = g.regions[j];
        const k1 = moveCost(g, c, x);
        if (!room(x, c, here.id) || c.ap < Math.min(k1, 4)) continue;
        if (quiet(x)) { if (!way || way.length > 1 || better(x)) way = [x]; continue; }
        if (way?.length === 1) continue;
        for (const jj of x.nb) {
          const y = g.regions[jj];
          if (y.id === here.id || !room(y, c, x.id) || !quiet(y) || c.ap - k1 < Math.min(moveCost(g, c, y, x.id), 4)) continue;
          if (better(y)) way = [x, y];
        }
      }
      if (way) for (const x of way) if (!moveCorps(g, c, x)) break;
    }
    // Out of the line its rest begins, or goes on. Wherever it stands it attacks nobody this month.
    if (!inLine(g.regions[c.reg]) && g.flags[key(c)] === undefined) g.flags[key(c)] = g.turn;
    keep(c);
  }
}

/** The corps a staff would rest this month, on their own as the judgement stands at the start of its month: their ids, and where each stands afterwards. */
export function reliefJudgement(g: Game, nat: string): { id: number; reg: number }[] {
  try { rested = []; relieve(g, nat); return rested.map((c) => ({ id: c.id, reg: c.reg })); } finally { for (const c of rested) c.hold = false; rested = []; }
}

/** The staff's two judgements against a ring, made on their own as they are at the start of its month: the places it gives up. */
export function ringJudgement(g: Game, nat: string): number[] {
  try { breakOut(g, nat); if (g.nations[nat].alive) stepBack(g, nat); return [...sacks]; } finally { given = new Set(); sacks = new Set(); }
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
        const next = r.nb.map((j) => g.regions[j]).filter((x) => ctl(g, x) === nat && within(x) && !at.has(x.id) && crowd(x) < STACK).sort((a, b) => crowd(a) - crowd(b))[0];
        if (!foot || !next || !moveCorps(g, foot, next)) break;
      }
      const d = hops(g, r.id, (x) => allied(g, nat, ctl(g, x)));
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
      if (scope) { walkToward(g, c, r); if (c.reg !== r.id) break; } else if (!railToward(g, c, r)) break;
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
    // Others the plan sends in the same lift, if they are standing on the same quay. (No more than the floor at home
    // lets go: ruling 229, 1b.)
    const force = [c, ...(L.with ?? []).map((nm) => g.corps.find((x) => x.nat === nat && x.name === nm && !x.dead && x.reg === c.reg)).filter((x): x is Corps => !!x)];
    if (leavesHome(g, nat, from)) force.splice(Math.min(force.length, mayLeave(g, nat)));
    if (!force.length) continue;
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

/**
 * For the measuring harness: told of every landing a staff makes, with the forecast of the sea behind the beach as
 * the staff read it that moment (`warned`: it could be closed), and whether it was a further wave to men already
 * ashore. Nothing in the game listens.
 */
export const landingWatch: { tell: ((e: { nat: string; to: Region; force: Corps[]; warned: boolean; sea: number; wave: boolean; taken: boolean }) => void) | null } = { tell: null };

/**
 * A staff with shipping sends the force it keeps by a quay across the sea: the army at home, and an army standing
 * idle on a far shore, each in its turn.
 */
/**
 * The force that dealt with an enemy on its own side of the ocean comes home when he is beaten (Version 81). A corps
 * of a computer-led country beyond the old chart that went by sea to foreign ground on its own side of the water,
 * and now has no enemy next to it and none left standing on that side, takes ship for the nearest harbour of its own
 * country with room, and is again part of the army at home (and so of the army that sails). One that stands where
 * there is no harbour marches to the nearest first. It runs in peace as in war. One that marched in from its own
 * country's ground next door, and stands on the ground of a country now fallen, marches back to it (the Canal Zone's
 * garrison from Panama City, once a besieger kept its siege beyond Europe: b387).
 */
function homeAgain(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (nat === g.human || !keepsDoor(g, nat)) return;
  const away = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && !c.town && c.joint === undefined && beyondEurope(c.reg) && g.regions[c.reg].owner !== nat;
  const abroad = g.corps.filter((c) => away(c) && !!g.flags[`sf:${c.id}`]).sort((a, b) => a.id - b.id);
  const walked = g.corps
    .filter((c) => away(c) && !g.flags[`sf:${c.id}`] && !g.nations[g.regions[c.reg].owner]?.alive && g.regions[c.reg].nb.some((j) => g.regions[j].owner === nat))
    .sort((a, b) => a.id - b.id);
  if (!abroad.length && !walked.length) return;
  if (enemyAtTheDoor(g, nat) || n.atWar.some((e) => g.nations[e]?.alive && neighbourAcrossTheWater(g, nat, e))) return;
  for (const c of walked) {
    const here = g.regions[c.reg];
    if (here.nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) continue;
    const home = here.nb.map((j) => g.regions[j]).filter((r) => r.owner === nat && r.ctrl === nat && friendlyUnits(g, r, nat).length < STACK).sort((a, b) => a.id - b.id)[0];
    if (home && moveCorps(g, c, home)) c.hold = false;
  }
  const havens = g.regions.filter((r) => r.owner === nat && !r.overseas && r.port && r.ctrl === nat);
  for (const c of abroad) {
    const here = g.regions[c.reg];
    if (here.nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) continue;
    if (!here.port) {
      const d = hops(g, here.id, (r) => allied(g, nat, ctl(g, r)));
      const quay = g.regions.filter((r) => r.port && d[r.id] > 0 && allied(g, nat, r.ctrl)).sort((a, b) => d[a.id] - d[b.id] || a.id - b.id)[0];
      if (quay) { c.hold = false; walkToward(g, c, quay); }
      continue;
    }
    if (liftLeft(g, nat) < 1) break;
    const way = havens
      .filter((p) => friendlyUnits(g, p, nat).length < STACK)
      .map((p) => ({ p, far: seaPath(g, nat, here, p)?.length ?? Infinity }))
      .filter((x) => x.far < Infinity)
      .sort((a, b) => a.far - b.far || a.p.id - b.p.id);
    if (way.some((x) => shipMove(g, c, x.p))) { c.hold = false; delete g.flags[`sf:${c.id}`]; }
  }
}

function descent(g: Game, nat: string): void {
  for (const slot of [0, 1]) crossSea(g, nat, slot);
}

/**
 * One force's month. On the hardest level of the grand campaign a rival goes for the player's coast, and a harbour
 * it already holds there is fed first. A second front (any level) feeds the men already ashore every month, with
 * every ship it has, for as long as they are too few for the front they hold: into a harbour in friendly hands,
 * or over the beach while the harbour town still holds out. Otherwise the staff looks for a
 * harbour of the enemy's it can take: an unguarded one, or a guarded one at good odds, with a crossing that is not
 * too dangerous and no army standing next door that would throw it straight back into the sea.
 */
function crossSea(g: Game, nat: string, slot: number): void {
  const sea = seaFoe(g, nat, slot);
  if (!sea) return;
  const L = sea.L, you = sea.foe;
  const quay = seaBase(g, nat, slot);
  if (!quay) return;
  const sfx = slot ? ':o' : '';
  // (History's list picks the day, ruling 233(u): a staff that has planned for a shore of the list since the first
  // month of its lead lands on it in the shore's month without the monthly dice, where every test passes.)
  const day = sea.second && onTheDay(g, nat, you, sfx);
  const ready = quay.units
    .filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne && u.supplied && u.str >= 0.7 && u.ap >= u.apMax)
    // (The fighting corps take the boats first: guns land on no enemy shore by themselves, ruling 211(a).)
    .sort((a, b) => (gunsRule(g) ? Number(fights(b)) - Number(fights(a)) : 0) || atkPower(g, b) - atkPower(g, a) || a.id - b.id);
  // (From a quay at home, no more go than the floor at home lets go: ruling 229, 1b. The home commands that hold in
  // place are not counted in the floor, and never sail: three of them stand on harbours.)
  if (leavesHome(g, nat, quay)) { const go = ready.filter((u) => !u.hold).slice(0, mayLeave(g, nat)); ready.splice(0, ready.length, ...go); }
  if (sea.second) {
    const ashore = lodgment(g, nat, slot);
    if (ashore?.short && ashore.ports.length) {
      // (The test of strength, Version 83: nobody more is fed into a harbour where, with him and all the ships can
      // still bring, the men ashore would not be expected to hold.)
      for (const c of ready) {
        if (liftLeft(g, nat) < 1) break;
        if (ashore.ports.some((p) => friendlyUnits(g, p, nat).length < STACK && holdsAshore(g, nat, p, [c], followUp(g, nat, quay, [c], p)) && shipMove(g, c, p))) { c.hold = false; g.flags[`sf:${c.id}`] = 1; }
      }
      return;
    }
    if (ashore?.short) {
      // The harbour town still holds out and no ship can dock: the next wave goes in over the beach, beside the men ashore.
      const wave = ready.slice(0, Math.min(liftLeft(g, nat), STACK - friendlyUnits(g, ashore.at, nat).length));
      if (wave.length) {
        const plan = planLanding(g, wave, ashore.at);
        // (Not where its own forecast says the sea behind the beach could be closed: more men would only share the trap.)
        const warned = !!plan.hold && !plan.hold.open;
        if (!plan.why && plan.risk <= L.risk && !(heedsCard(ashore.at, true) && warned) && holdsAshore(g, nat, ashore.at, wave, followUp(g, nat, quay, wave, ashore.at)) && landCorps(g, wave, ashore.at).ok) {
          landingWatch.tell?.({ nat, to: ashore.at, force: wave, warned, sea: plan.hold?.sea ?? -1, wave: true, taken: true });
          for (const c of wave) { c.hold = false; if (!c.dead) g.flags[`sf:${c.id}`] = 1; }
        }
      }
      return;
    }
  }
  // (With its men ashore on its own side of the ocean and not short, a staff lands nobody else there: the force its
  // forecast asks for and no more. Version 81, ruling 80.)
  if (sea.second && !slot && neighbourAcrossTheWater(g, nat, you) && lodgment(g, nat)) return;
  // (The joint crossing, ruling 233(w): where its own corps at the quay are fewer than the force, corps of its
  // computer-run allies standing ready there, with no crossing of their own staff's from that harbour, fill the
  // force up to its size and sail in it, under every test on the whole force. Torch, Sicily and Salerno were
  // Anglo-American landings under one command. Before, two British, two American and one Canadian corps filled
  // Cornwall for a season of 1943 and nobody had the three corps a landing asks.)
  if (ready.length && ready.length < L.force) ready.push(...quayGuests(g, nat, quay, you).slice(0, L.force - ready.length));
  // The test of strength (Version 83; staff.ts, `strengthAsked`): where the usual force would not be expected to
  // hold a beach, a bigger first wave is tried, as many as the ships lift and stand ready at the quay.
  const ask = strengthAsked(g, nat);
  const most = ask === null || !STRENGTH ? L.force : Math.max(L.force, Math.min(ready.length, liftLeft(g, nat), STRENGTH.wave, STACK));
  // (The smallest wave that would hold by itself; failing that the biggest, where it holds with what the ships
  // can bring in after it from the quay.)
  // (Tanks do not go ashore alone against a gunned shore either: infantry from the quay takes the last tank corps'
  // place in the boats, and with none there the wave is not made.)
  const mixed = (r: Region, w: Corps[]): Corps[] | null => {
    if (gunsRule(g) && !w.some(fights)) return null;
    if (!hostileUnits(g, r, nat).length) return w;
    const sent = notTanksAlone(g, w, ready, r);
    return !sent || sent === w ? sent : [...w.slice(0, -1), sent[sent.length - 1]];
  };
  const waveFor = (r: Region): Corps[] | null => {
    for (let k = L.force; k <= most; k++) {
      const w = mixed(r, ready.slice(0, k));
      if (w && holdsAshore(g, nat, r, w)) return w;
    }
    const w = mixed(r, ready.slice(0, most));
    return w && holdsAshore(g, nat, r, w, followUp(g, nat, quay, w, r)) ? w : null;
  };
  if (sea.second) takeAim(g, nat, sfx, quay, ready.slice(0, L.force), L, you, ask === null ? null : waveFor);
  const force = ready.slice(0, Math.min(L.force, liftLeft(g, nat)));
  if (force.length < L.force) return;
  const went = (k: number): void => { for (const c of force) c.hold = false; g.flags[`st:seaNext:${nat}${sfx}`] = g.turn + k; };
  if (g.turn < (g.flags[`st:seaNext:${nat}${sfx}`] ?? 0)) return;
  // The men already ashore come first: while their harbour is still a front, the next lift goes to them.
  const lodged = g.regions[(g.flags[`st:lodged:${nat}${sfx}`] ?? 0) - 1];
  if (lodged && lodged.ctrl === nat && lodged.port && lodged.nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) {
    let sent = 0;
    for (const c of force) if (c.nat === nat && friendlyUnits(g, lodged, nat).length < STACK && holdsAshore(g, nat, lodged, [c], followUp(g, nat, quay, [c], lodged)) && shipMove(g, c, lodged)) sent++;
    if (sent) { went(2); return; }
  }
  // (Against an enemy on the staff's own side of the ocean nobody waits for a day to be picked: he is attacked the
  // month the force stands ready. Version 81.)
  const atOnce = sea.second && neighbourAcrossTheWater(g, nat, you);
  const diced = !atOnce && hashString(`descent:${g.seed}:${g.turn}:${nat}${sfx}`) / 4294967296 >= L.chance;
  if (diced && !day) return;
  const men = force.reduce((s, c) => s + c.divs * c.str, 0);
  let best: Region | null = null, bv = 0, bw = false, bz = -1, bf = force, weak = false, weakMain = false;
  // (An island taken whole, or a colony overseas: the lesser prize beside a beach on a mainland.)
  const lesser = (r: Region): boolean => r.nb.length === 0 || !!r.overseas;
  for (const r of landingTargets(g, force)) {
    if (r.ctrl !== you || !r.port) continue;
    // (The dice said no and the list picked the day: the list's shore of this month and no other.)
    if (diced && !opensNow(g, nat, r)) continue;
    // (A staff with history's list of shores opens its second front on those and nowhere else. A shore of the list
    // whose month comes within the lead is reckoned through like any other and not landed on: where only its
    // strength stands in the way the staff builds up for it beforehand. Ruling 233(q).)
    const soon = sea.second && opensSoon(g, nat, r);
    if (sea.second && ((!soon && !opensOn(g, nat, r)) || !builtUp(g, nat, r))) continue;
    // (An ally's corps in the force goes only where its own staff's list lets it follow an ally ashore.)
    if (force.some((c) => c.nat !== nat && !joinsOn(g, c.nat, r, nat))) continue;
    // (The history dial: by the book no second front is opened on a shore before history's first landing there.)
    if (!soon && !shoreOpen(g, r.owner, !!r.overseas, r.name)) continue;
    const plan = planLanding(g, force, r);
    if (plan.why || plan.risk > L.risk) continue;
    // A staff heeds its own forecast, the one a player reads on the landing card: it puts nobody on a beach where
    // the sea behind could be closed and the force cut off from its supplies. (Except to take a whole island.)
    const warned = !!plan.hold && !plan.hold.open;
    if (heedsCard(r) && warned) continue;
    if (plan.defended && (!plan.assess || plan.assess.odds < L.odds)) continue;
    // The army within a march of the beach.
    let near = 0;
    for (const j of [r.id, ...r.nb]) for (const u of hostileUnits(g, g.regions[j], nat)) if (u.kind !== 'fort') near += u.divs * u.str;
    if (near > L.near * men) continue;
    // And it sails only with a force it expects to be still ashore three months on: the usual one, or a bigger wave.
    const wave = waveFor(r);
    if (!wave) { weak = true; if (!lesser(r)) weakMain = true; continue; }
    if (soon) continue;
    const v = (2 + r.vp + r.fac + (r.cap ? 6 : 0) + (r.owner !== you ? 2 : 0)) * (plan.defended ? 1 : 1.5) - 10 * plan.risk - 0.2 * near;
    if (!best || v > bv) { best = r; bv = v; bw = warned; bz = plan.hold?.sea ?? -1; bf = wave; }
  }
  // (Where only its strength stood in the way it builds up and waits: the quay is filled, and the shipping desk
  // builds the ships to lift what stands there. staff.ts, the staging of the quay.)
  // And a beach on a mainland comes before an island or a colony that passes: for as long as it would rest after
  // a landing ('every' months) the staff builds up for the mainland sooner than spend its force on the lesser prize.
  // (Where a test refused the list's shore in its month, the dice are back the month after, as before.)
  if (diced && !best) return;
  const since = `st:moreFrom:${nat}${sfx}`;
  if ((g.flags[`st:more:${nat}${sfx}`] ?? 0) < g.turn - 2) delete g.flags[since];
  const waits = !!best && weakMain && lesser(best) && g.turn - (g.flags[since] ?? g.turn) < L.every;
  if (weak && (!best || waits)) { g.flags[`st:more:${nat}${sfx}`] = g.turn; g.flags[since] ??= g.turn; }
  if (!best || waits) return;
  const res = landCorps(g, bf, best);
  if (!res.ok) return;
  for (const c of bf) c.hold = false;
  landingWatch.tell?.({ nat, to: best, force: bf, warned: bw, sea: bz, wave: false, taken: !!res.taken });
  went(L.every);
  delete g.flags[`st:aim:${nat}${sfx}`];
  delete g.flags[`st:more:${nat}${sfx}`];
  delete g.flags[since];
  if (res.taken) {
    g.flags[`st:lodged:${nat}${sfx}`] = best.id + 1;
    if (sea.second) for (const c of bf) if (!c.dead) g.flags[`sf:${c.id}`] = 1;
  }
}

/**
 * The army sent across the ocean (the wider world; world.json 'overseas', staff.ts `oceanRuns`). A computer-run
 * country an ocean away from every enemy ships the corps it raises to a harbour of its own side's on the far shore:
 * each ocean's run has a share of the month's shipping, banked while there is nothing to carry or no ship free, and
 * a guard of so many field corps never leaves home. First, though, the corps already across that stand idle on the
 * far shore go on by sea to a second front their side is fighting on, into its harbours: no beach is stormed for
 * that. Then the month's sailings from home, the best corps first, to that front's harbours or, when they are full or
 * the way to them too dangerous, to the harbour where the army is built up; and the next to sail are brought to the quay by
 * rail. It runs after the month's landings (`descent`), which have first call on the ships.
 */
function ferry(g: Game, nat: string): void {
  const O = overseasOf(g, nat);
  if (!O) return;
  const n = g.nations[nat];
  const fit = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !c.airborne && !c.town && c.joint === undefined && c.supplied && c.str >= 0.7 && c.ap >= c.apMax;
  const byWorth = (a: Corps, b: Corps): number => atkPower(g, b) - atkPower(g, a) || a.id - b.id;
  const went = (c: Corps): void => { c.hold = false; g.flags[`sf:${c.id}`] = 1; };
  // No corps is shipped to a full harbour, or over a crossing too dangerous for its staff ('risk'). A hop over a
  // narrow sea (no further than a landing can go) into a harbour its side already holds is a reinforcement and not
  // a venture: it has a ceiling of its own ('hop').
  const sails = (c: Corps, p: Region): boolean => {
    if (friendlyUnits(g, p, nat).length >= STACK) return false;
    const path = seaPath(g, nat, g.regions[c.reg], p);
    if (!path) return false;
    const limit = path.length <= NAVY.landing.range ? Math.max(O.risk, O.hop ?? 0) : O.risk;
    return crossingRisk(g, nat, path) <= limit && shipMove(g, c, p);
  };
  // The test of strength (Version 83, the design director's rulings 92 and 94): no corps is shipped into a harbour
  // where, against what the enemy can bring there within three months, the men of its side there would not be
  // expected to hold with it and with those the ships can bring in after it. One refused at a harbour of the front
  // is put into a harbour farther back on the same ground, out of his reach, and marches up.
  let refused = false;
  const goes = (c: Corps, p: Region, from: Region, most?: number): boolean => {
    if (!holdsAshore(g, nat, p, [c], followUp(g, nat, from, [c], p, most))) { refused = true; return false; }
    return sails(c, p);
  };
  // The far shore: idle corps go on to a front their side holds beyond a narrow sea. Into the harbours of a
  // landing first; after those, of the harbours its side holds near the enemy, the one the fewest seas away.
  const front = frontAshore(g, nat);
  const order = new Map<number, Region[]>();
  const portsFor = (c: Corps): Region[] => {
    if (!front) return [];
    let ports = order.get(c.reg);
    if (!ports) {
      const far = new Map(front.ports.slice(front.lodged).map((p) => [p.id, seaPath(g, nat, g.regions[c.reg], p)?.length ?? Infinity]));
      // (For America and Canada the main front first: a harbour in France or the Low Countries before any other, ruling 98(e).)
      ports = mainFirst(nat, [...front.ports.slice(0, front.lodged), ...front.ports.slice(front.lodged).sort((a, b) => far.get(a.id)! - far.get(b.id)!)]);
      order.set(c.reg, ports);
    }
    return ports;
  };
  if (front) {
    for (const c of g.corps.filter(fit).sort(byWorth)) {
      if (liftLeft(g, nat) < 1) break;
      const here = g.regions[c.reg];
      if (here.owner === nat || !here.port || front.ground[here.id] || !allied(g, nat, here.ctrl)) continue;
      // One with the enemy next to it has a front of its own already.
      if (here.nb.some((j) => atWar(g, nat, g.regions[j].ctrl))) continue;
      // It goes on over a narrow sea, never back across the ocean: a harbour beside an enemy on the other shore (an
      // ally's colony next to an Axis Brazil) is no place to feed the army in a corps at a time (Version 78).
      refused = false;
      if (portsFor(c).some((p) => beyondEurope(p) === beyondEurope(here) && goes(c, p, here))) went(c);
      else if (refused && front.rear.some((p) => beyondEurope(p) === beyondEurope(here) && goes(c, p, here))) went(c);
    }
  }
  const home = (): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && g.regions[c.reg].owner === nat && !g.regions[c.reg].overseas);
  // The force for an enemy on its own side of the ocean is no part of the army that sails: the corps at that quay, up
  // to the landing force, stay there (Version 81).
  const door = doorQuay(g, nat);
  if (door) {
    const want = seaFoe(g, nat)?.L.force ?? 0;
    const kept = door.units.filter((x) => x.nat === nat && !x.dead && x.kind !== 'fort' && x.kind !== 'eng').sort(byWorth).slice(0, want);
    for (const u of kept) massed.add(u.id);
    // And it has first call on the army at home: before any corps is brought down to the ocean quay, the infantry
    // it still wants comes to its own quay by rail (never a corps that stands alone in a harbour on the coast watch).
    const watch = coastWatch(g, nat);
    const lone = (c: Corps): boolean => g.regions[c.reg].name in watch && g.regions[c.reg].units.filter((u) => u.nat === nat && u.kind !== 'fort').length < 2;
    const pool = home().filter((c) => c.kind === 'inf' && !c.hold && c.reg !== door.id && !c.airborne && !c.town && c.joint === undefined && c.str >= 0.6 && c.ap >= 4 && !massed.has(c.id)).sort(byWorth);
    let have = kept.length;
    for (const c of pool) {
      if (have >= want || friendlyUnits(g, door, nat).length >= STACK) break;
      if (!lone(c) && railToward(g, c, door)) { massed.add(c.id); have++; }
    }
  }
  // The ocean runs.
  for (const run of oceanRuns(g, nat)) {
    const key = `fy:${nat}:${run.ocean}`, at = `fyAt:${nat}:${run.ocean}`;
    if (g.flags[at] !== g.turn) { g.flags[at] = g.turn; g.flags[key] = Math.min(O.bank, (g.flags[key] ?? 0) + run.share * n.sealift); }
    let spare = home().length - O.guard;
    // (An expeditionary force of history's size: no more than 'most' of its field corps are kept across the ocean.)
    let more = O.most === undefined ? Infinity : O.most - g.corps.reduce((s, c) => s + (c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !beyondEurope(c.reg) ? 1 : 0), 0);
    for (const c of run.from.units.filter((u) => fit(u) && !u.hold && !(door && massed.has(u.id))).sort(byWorth)) {
      if (spare <= 0 || more <= 0 || (g.flags[key] ?? 0) < 1 || liftLeft(g, nat) < 1) break;
      // (The harbours of a front first; where the test of strength refused one of them, a harbour farther back on its
      // ground; then the harbour where the army is built up.)
      const carried = Math.ceil(((STRENGTH?.months ?? 3) - 1) * run.share * n.sealift);
      const put = (ps: Region[]): Region | undefined => ps.find((p) => goes(c, p, run.from, carried));
      refused = false;
      const to = put(mainFirst(nat, run.to.slice(0, run.ahead))) ?? (refused ? put(run.rear) : undefined) ?? put(run.to.slice(run.ahead));
      if (!to) break;
      g.flags[key] -= 1;
      spare--;
      more--;
      went(c);
      // The first to cross is news for everyone; after that, for the player whose war they have come to.
      const first = !g.flags[`fy:first:${nat}`];
      g.flags[`fy:first:${nat}`] = 1;
      if (first) headline(g, 'event', `The first ${n.adj} troops to cross the ocean come ashore at ${to.name}: ${c.name}. More will follow as fast as the ships can carry them.`, nat, to.id);
      else if (g.human !== null && (allied(g, g.human, nat) || atWar(g, g.human, nat)) && allied(g, g.human, to.ctrl)) headline(g, 'info', `${c.name} of the ${n.adj} army has landed at ${to.name}.`, nat, to.id);
    }
    // The next to sail come down to the quay: as many as the run will carry in the next two months, and never the guard.
    const want = Math.min(spare, Math.ceil(2 * run.share * n.sealift));
    let have = run.from.units.filter((u) => u.nat === nat && !u.dead && !u.hold && u.kind !== 'fort' && u.kind !== 'eng').length;
    for (const u of run.from.units) if (u.nat === nat && !u.hold) massed.add(u.id);
    if (have >= want) continue;
    const pool = home().filter((c) => !c.hold && c.reg !== run.from.id && !c.airborne && !c.town && c.joint === undefined && c.str >= 0.6 && c.ap >= 4 && !massed.has(c.id)).sort(byWorth);
    for (const c of pool) {
      if (have >= want || friendlyUnits(g, run.from, nat).length >= STACK) break;
      if (railToward(g, c, run.from) && c.reg === run.from.id) { massed.add(c.id); have++; }
    }
  }
  if (g.flags[`fy:first:${nat}`]) follow(g, nat, O);
}

/**
 * The army sent to the Middle East (ruling 229, part 1; middle-east.json, middleeast.ts). At the looser stops of the
 * history dial a computer-run Britain sails its spare field corps from home to the first harbour of Egypt its side
 * holds, the best first, by the sailing every staff's corps take, while the desert's threat outweighs home's: the
 * desert is short of men against the enemy at its edge, and shorter than home is against what can really cross the
 * Channel. The floor at home binds every corps that goes, the sealift caps how many go in a month, and no corps sails
 * over a crossing too dangerous for its staff ('risk') or by a sea the rule does not name ('by'). Those not yet in a
 * harbour from which the way is open are brought to the best one by rail. Where no harbour at home has a way open
 * to the theatre (the Mediterranean too dangerous for the troopships), the staff sends its spare corps round the
 * Cape instead: put to sea from home, with no loss on the way, they land 'cape' months later by the arrival rows'
 * own code (game.ts, putToSea and landVoyages; the design director's b460). The floor, the weighing and the sealift
 * bind them as they bind a sailing; a corps already at sea for the theatre counts in the weighing as on its way.
 * It runs after the month's landings (`descent`), which have first call on the ships.
 */
function reinforce(g: Game, nat: string): void {
  const R = reinforceRules(g, nat);
  const n = g.nations[nat];
  if (!R || !n.alive || !n.atWar.length || liftLeft(g, nat) < 1) return;
  const k = THEATRE_ENTRIES.findIndex((e) => e[0] === R.theatre);
  if (k < 0) return;
  const ground = theatreRegions(g)[k];
  const ports = R.to.map((nm) => g.byName[nm]).filter((r): r is Region => !!r);
  const by = new Set(R.by.map((nm) => g.seas.findIndex((z) => z.name === nm || z.key === nm)).filter((z) => z >= 0));
  // (Corps already at sea for that front, on their way.)
  const coming = (): number => voyagesOf(g, nat).reduce((s, v) => s + (ports.some((p) => p.id === v.to) ? v.corps.divs * v.corps.str : 0), 0);
  // Still short there, and shorter than at home: the harbours of that front its side holds, or none.
  const short = (): Region[] => {
    const f = theatreFront(g, nat, ground, ports);
    if (!f) return [];
    const want = R.cover * f.threat - f.held - coming();
    if (want <= 0) return [];
    const h = homeThreat(g, nat, R.waves);
    return want > R.cover * h.threat - h.held ? ports.filter((p) => f.on.has(p.id)) : [];
  };
  let to = short();
  if (!to.length) return;
  // The way by sea from a harbour at home: by the seas the rule names, and not too dangerous.
  const way = (from: Region, p: Region): boolean => {
    const path = seaPath(g, nat, from, p);
    return !!path && (!by.size || path.every((z) => by.has(z))) && crossingRisk(g, nat, path) <= R.risk;
  };
  const room = (p: Region): boolean => friendlyUnits(g, p, nat).length < STACK;
  // Never the corps the staff keeps at its quay for a landing, nor the one that stands alone in a harbour on the coast watch.
  const quay = g.turn - (g.flags[`st:quayAt:${nat}`] ?? -99) < 6 ? (g.flags[`st:quay:${nat}`] ?? 0) - 1 : -1;
  const watch = coastWatch(g, nat);
  const lone = (c: Corps): boolean => g.regions[c.reg].name in watch && g.regions[c.reg].units.filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort').length < 2;
  const pool = g.corps
    .filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !c.hold && !c.airborne && !c.town && c.joint === undefined && c.supplied && c.str >= 0.7 && c.ap >= c.apMax)
    .filter((c) => homeland(g.regions[c.reg], nat) && c.reg !== quay && !lone(c))
    .sort((a, b) => atkPower(g, b) - atkPower(g, a) || a.id - b.id);
  // No harbour at home with a way open to the theatre: round the Cape (b460). Only while a harbour at home still
  // opens on a sea its ships can use: a homeland whose own waters are shut, or whose harbours are all lost, sends
  // nobody. (The two months at sea take in the journey to the quay.)
  const harbours = g.regions.filter((r) => homeland(r, nat) && r.port && r.ctrl === nat);
  const sails = harbours.some((r) => r.seas.some((z) => !closedTo(g, z, nat)));
  if (R.cape > 0 && sails && !to.some((p) => harbours.some((r) => way(r, p)))) {
    for (const c of pool) {
      if (!to.length || mayLeave(g, nat) < 1 || liftLeft(g, nat) < 1) break;
      // (Looked at again for each: the one the corps before it leaves alone on the coast watch stays.)
      if (lone(c)) continue;
      putToSea(g, c, to[0], R.cape);
      useLift(g, nat);
      to = short();
    }
    return;
  }
  const waiting: Corps[] = [];
  for (const c of pool) {
    if (!to.length || mayLeave(g, nat) < 1 || liftLeft(g, nat) < 1) break;
    if (lone(c)) continue;
    const here = g.regions[c.reg];
    const dest = here.port ? to.find((p) => room(p) && way(here, p) && shipMove(g, c, p)) : undefined;
    if (!dest) { waiting.push(c); continue; }
    if (g.human !== null && (allied(g, g.human, nat) || atWar(g, g.human, nat)) && allied(g, g.human, dest.ctrl)) headline(g, 'info', `${c.name} of the ${n.adj} army has landed at ${dest.name}.`, nat, dest.id);
    to = short();
  }
  // The next to sail are brought by rail to the best harbour at home for the way: as many as the ships could take next month.
  const dest = to.find(room);
  if (!dest || !waiting.length) return;
  const home = g.regions.filter((r) => homeland(r, nat) && r.port && r.ctrl === nat && way(r, dest));
  if (!home.length) return;
  const risk = (r: Region): number => crossingRisk(g, nat, seaPath(g, nat, r, dest)!);
  const best = home.sort((a, b) => risk(a) - risk(b) || a.id - b.id)[0];
  let have = best.units.filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.hold).length;
  const want = Math.min(mayLeave(g, nat), n.sealift);
  for (const c of waiting) {
    if (have >= want || friendlyUnits(g, best, nat).length >= STACK) break;
    if (c.reg !== best.id && railToward(g, c, best) && c.reg === best.id) have++;
  }
}

/**
 * Where a country's army has gone across an ocean, its fleet and its air groups go too (world.json 'overseas'). Once
 * the first corps is across: each fleet named under 'fleet' moves its base to the harbour where the army is built
 * up (or the port of its side's nearest to it that the ships can reach) and takes the sea off it for its beat, so
 * that it can cover the crossing its staff has its eye on (navalAI). And the air groups still at home, beyond the
 * 'air' kept there, are shipped one a month to the airfield of its side's nearest that harbour with room for them:
 * from there its air staff flies them as it flies any group based abroad (airAI: raids, and the sky over the beach).
 * A command marked 'home' in air.json guards its own country and never follows the army (Version 84.2: America's
 * First Air Force and the rest); the commands raised for the war overseas form at home and sail in their turn.
 */
function follow(g: Game, nat: string, O: OverseasRules): void {
  const id = Object.keys(O.oceans).map((o) => g.flags[`st:base:${nat}:${o}`] ?? 0).find((x) => x > 0);
  const base = id ? g.regions[id - 1] : null;
  if (!base || !allied(g, nat, base.ctrl)) return;
  const n = g.nations[nat];
  const far = (r: Region): number => mapDist(g, r.x, r.y, base.x, base.y);
  for (const f of fleetsOf(g, nat)) {
    if (!O.fleet?.includes(f.name) || f.base < 0 || !beyondEurope(f.base) || !shipCount(f.ships)) continue;
    let to: Region | null = null;
    for (const r of g.regions) {
      if (beyondEurope(r) || !r.port || !r.seas.length || !allied(g, nat, r.ctrl) || hostileUnits(g, r, nat).length || !canSailTo(g, f, r)) continue;
      if (!to || far(r) < far(to)) to = r;
    }
    if (!to || !rebase(g, f, to)) continue;
    f.station = to.seas[0];
    headline(g, 'event', `The ${f.name} crosses the ocean to ${to.name}: the ${n.adj} fleet goes where its army has gone, to cover the crossings still to come.`, nat, to.id);
  }
  if (O.air === undefined || g.flags[`fy:air:${nat}`] === g.turn) return;
  const home = airFleetsOf(g, nat).filter((f) => !afloat(f) && beyondEurope(f.base) && !homeCommand(f)).sort((a, b) => b.ftr + b.bmr - (a.ftr + a.bmr) || a.id - b.id);
  if (home.length <= O.air) return;
  for (const f of home) {
    let to: Region | null = null, bd = Infinity;
    for (const r of g.regions) {
      if (beyondEurope(r) || whyNotBase(g, f, r, true) !== null || !fieldRoom(g, r.id, f)) continue;
      const d = km(g, base.id, r.id);
      if (d < bd) { bd = d; to = r; }
    }
    if (!to || !rebaseAir(g, f, to, true)) continue;
    g.flags[`fy:air:${nat}`] = g.turn;
    if (g.human !== null && (allied(g, g.human, nat) || atWar(g, g.human, nat))) headline(g, 'info', `${f.name} of the ${n.adj} air force has crossed the ocean with the army, to the airfields of ${to.name}.`, nat, to.id);
    break;
  }
}

/**
 * The beach a staff opening a second front has its eye on: the harbour of the enemy's within a landing's range of
 * the quay that its force could take and hold (unguarded or guarded at fair odds, no army next door too strong
 * for it), whatever still bars the way by sea or in the air. Its fleet then covers the crossing (navalAI) and its
 * fighters fight for the sky over the beach (airAI), and the landing goes in the month the way is open. A beach
 * the fleet and the fighters cannot open in half a year is given up for a year, and the staff looks at the next.
 */
function takeAim(g: Game, nat: string, sfx: string, quay: Region, force: Corps[], L: { odds: number; near: number }, you: string, waveFor: ((r: Region) => Corps[] | null) | null = null): void {
  const key = `st:aim:${nat}${sfx}`, at = `st:aimAt:${nat}${sfx}`;
  if (!force.length) return;
  const men = force.reduce((s, c) => s + c.divs * c.str, 0);
  let best: Region | null = null, bv = 0;
  for (const r of g.regions) {
    if (r.ctrl !== you || !r.port || !r.seas.length || g.turn < (g.flags[`st:aimNo:${nat}:${r.id}`] ?? 0)) continue;
    // (A beach of the list whose month comes within the lead is named beforehand, so that the fleets stand off it: ruling 233(q).)
    const soon = opensSoon(g, nat, r);
    if ((!soon && !opensOn(g, nat, r)) || !builtUp(g, nat, r) || (!soon && !shoreOpen(g, r.owner, !!r.overseas, r.name))) continue;
    if (force.some((c) => c.nat !== nat && !joinsOn(g, c.nat, r, nat))) continue;
    // The way there over the water, whoever holds it now.
    if (!seaPath(g, nat, quay, r, { max: NAVY.landing.range, surprise: true })) continue;
    const defended = hostileUnits(g, r, nat).length > 0;
    if (defended && assess(g, force, r, landingOpts(g, force, r)).odds < L.odds) continue;
    let near = 0;
    for (const j of [r.id, ...r.nb]) for (const u of hostileUnits(g, g.regions[j], nat)) if (u.kind !== 'fort') near += u.divs * u.str;
    if (near > L.near * men) continue;
    // (No beach is named that the staff's own test of strength would keep its force from: Version 83.)
    if (waveFor && !waveFor(r)) continue;
    const v = (2 + r.vp + r.fac + (r.cap ? 6 : 0) + (r.owner !== you ? 2 : 0)) * (defended ? 1 : 1.5) - 0.2 * near;
    if (!best || v > bv) { best = r; bv = v; }
  }
  if (!best) { delete g.flags[key]; delete g.flags[at]; return; }
  // (The half year a beach stays named runs from the month it may be landed on, not from the lead before it.)
  if (g.flags[key] !== best.id + 1 || opensSoon(g, nat, best)) { g.flags[key] = best.id + 1; g.flags[at] = g.turn; return; }
  if (g.turn - (g.flags[at] ?? g.turn) >= 6) { g.flags[`st:aimNo:${nat}:${best.id}`] = g.turn + 12; delete g.flags[key]; delete g.flags[at]; }
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

export const atkPower = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk;

/** A corps that may go onto this enemy-held ground: guns and engineers follow a fighting corps there, they never lead (ruling 211(a), the grand campaign). */
const mayEnter = (g: Game, c: Corps, to: Region): boolean => !gunsRule(g) || fights(c) || gunsMayFollow(g, c, to);

/**
 * Tanks are not sent alone at a gunned line (ruling 152(g), the grand campaign). Where the pick is tanks with no men
 * on foot and the defence's anti-tank strength is 'gunned' or more, the strongest infantry to hand goes in with them;
 * at 'never' or more with no infantry to send, the attack is not made. Gives the force to send, or null.
 */
export function notTanksAlone(g: Game, pick: Corps[], pool: Corps[], target: Region): Corps[] | null {
  // (And guns never go forward alone, ruling 211(a): a pick with no fighting corps in it takes the strongest one to
  // hand, and with none to hand the attack is not made.)
  if (gunsRule(g) && pick.length && !pick.some(fights)) {
    const lead = pool.filter((c) => fights(c) && !pick.includes(c)).sort((a, b) => atkPower(g, b) - atkPower(g, a))[0];
    return lead ? notTanksAlone(g, [...pick, lead], pool, target) : null;
  }
  if (g.flags.open !== 1 || !pick.some((c) => c.kind === 'arm') || pick.some((c) => isFoot(c))) return pick;
  const at = assess(g, pick, target).faced?.antiTank;
  if (!at) return pick;
  // (The marks are met at their printed value: 0.58 less 0.08 is a half, whatever the machine's arithmetic makes of it.)
  const strength = Math.round(at.strength * 1e6) / 1e6;
  if (strength < SUPPORT.staff.gunned) return pick;
  const foot = pool.filter((c) => isFoot(c) && !pick.includes(c)).sort((a, b) => atkPower(g, b) - atkPower(g, a))[0];
  if (foot) return [...pick, foot];
  return strength >= SUPPORT.staff.never ? null : pick;
}

/**
 * The same rule for a force already cut to the roads (a counter-stroke, a relief, a first wave): the infantry next
 * door goes in beside the tanks, in the place of the weakest tank corps where the roads are full. Null where the
 * rule says the attack is not made.
 */
export function footBeside(g: Game, force: Corps[], near: Corps[], target: Region): Corps[] | null {
  const sent = notTanksAlone(g, force, near, target);
  if (!sent || sent === force) return sent;
  const fits = (cs: Corps[]): boolean => limitAttackers(g, cs, target).length === cs.length;
  if (fits(sent)) return sent;
  const foot = sent[sent.length - 1];
  for (const t of [...force].sort((a, b) => atkPower(g, a) - atkPower(g, b))) {
    const swap = [...force.filter((c) => c !== t), foot];
    if (fits(swap)) return swap;
  }
  return notTanksAlone(g, force, [], target);
}

/** Whose army holds the country of a province: in the grand campaign an army round a town it has not taken holds the roads there. Armies march and are counted by this. */
const ctl = (g: Game, r: Region): string => fieldCtrl(g, r);

/** Steps through friendly ground from every region to the nearest one that borders the goal. */
function doorstep(g: Game, nat: string, goal: Region | null): Int16Array | null {
  if (!goal) return null;
  const d = new Int16Array(g.regions.length).fill(-1);
  const q: number[] = [];
  for (const j of goal.nb) if (allied(g, nat, ctl(g, g.regions[j]))) { d[j] = 0; q.push(j); }
  if (!q.length) return null;
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, nat, ctl(g, g.regions[j]))) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

/**
 * The grand campaign: the guns go where the works are. Before the month's attacks an artillery corps that stands
 * beside no dug-in enemy and no fortress is sent to the nearest sector of its army's front that does, the sectors
 * opposite the staff's targets first, as long as other troops stand there and the sector has not its guns already.
 */
export function gunsUp(g: Game, nat: string, enemies: Set<string>, targets: Set<string> | null): void {
  if (g.flags.open !== 1) return;
  const guns = g.corps.filter((c) => c.nat === nat && !c.dead && c.kind === 'art' && !c.hold && !c.town && c.ap > 0 && c.joint === undefined && c.leave === undefined && !massed.has(c.id));
  if (!guns.length) return;
  const works = (x: Region): boolean => enemies.has(ctl(g, x)) && atWar(g, nat, ctl(g, x)) && (worksAgainst(g, x, nat) || hostileUnits(g, x, nat).some((u) => u.dug));
  const facing = (r: Region): number => {
    let best = 0;
    for (const j of r.nb) { const x = g.regions[j]; if (works(x)) best = Math.max(best, !targets || targets.has(x.name) ? 2 : 1); }
    return best;
  };
  const gunsAt = (r: Region): number => r.units.filter((u) => u.nat === nat && u.kind === 'art').length;
  for (const c of guns) {
    const here = g.regions[c.reg];
    const now = facing(here);
    if (now === 2) continue;
    const far = hops(g, c.reg, (x) => within(x) && allied(g, nat, ctl(g, x)));
    let best: Region | null = null, bv = now ? 10 : 0;
    for (const r of g.regions) {
      const d = far[r.id];
      if (d < 1 || d > 4) continue;
      const f = facing(r);
      if (!f || gunsAt(r) >= DEFENCE.frontage.guns || !r.units.some((u) => u.nat === nat && u.kind !== 'art' && u.kind !== 'eng')) continue;
      const v = f * 10 - d;
      if (v > bv) { bv = v; best = r; }
    }
    if (!best) continue;
    // An army overseas has one road, and the sector at the end of it stands full. Its weakest corps on foot, one the
    // road would not carry into the attack in any case, steps back to where the guns stood and they take its place.
    if (scope && here.nb.includes(best.id) && friendlyUnits(g, best, nat).length >= STACK) {
      const out = best.units
        .filter((u) => u.nat === nat && !u.dead && u.kind === 'inf' && !u.hold && !u.town && u.ap > 0 && u.joint === undefined && u.leave === undefined && !massed.has(u.id))
        .sort((a, b) => atkPower(g, a) - atkPower(g, b) || a.id - b.id)[0];
      if (out) moveCorps(g, out, here);
    }
    walkToward(g, c, best);
  }
}

/**
 * The two-wave attack (the grand campaign). Where the whole force the roads will carry cannot take a place at the
 * odds the staff asks, but more corps stand ready behind it than the roads carry at once, the staff sends those in
 * first, to wear the works and tire the men in them, and the main body after them the same month. Returns the first
 * wave and the odds the main body will then have, or null where two waves would not do it either.
 */
export function twoWaves(g: Game, avail: Corps[], pool: Corps[], target: Region, need: number): { corps: Corps[]; then: number } | null {
  if (g.flags.open !== 1) return null;
  const H = DEFENCE.hammer;
  const spare = avail.filter((c) => c.org >= 0.35 && !pool.includes(c));
  const cut = limitAttackers(g, spare, target);
  // (A first wave of tanks alone is not thrown at a gunned line either: infantry goes in with it, or there is no first wave.)
  const first = cut.length ? footBeside(g, cut, spare, target) : null;
  if (!first?.length) return null;
  const a1 = assess(g, first, target);
  if (a1.odds < H.first) return null;
  const a2 = assess(g, pool, target, { after: { blows: a1.weight, wear: a1.wear } });
  return a2.odds >= need * H.second ? { corps: first, then: a2.odds } : null;
}

function offensive(g: Game, nat: string, objs: string[], enemies: Set<string>, minOdds: number, axisList: string[], axisOpen: boolean, follow: boolean, halt: Set<string>, only: Set<string> | null, oddsFor: Record<string, number> = {}): void {
  const failed = new Set<number>();
  // Places a first wave has gone in at this month, and how many: the main body follows, and nobody sends a third
  // (a staff pressing a won war sends one more where it has the corps).
  const waved = new Map<number, number>();
  const axis = new Set(axisList);
  // In the first month the armoured thrust takes its objectives in order; after that the whole line is open.
  const notYet = (name: string): boolean => {
    if (axisOpen || !axis.has(name)) return false;
    for (const a of axisList) {
      if (a === name) return false;
      const r = g.byName[a];
      if (r && !allied(g, nat, ctl(g, r))) return true;
    }
    return false;
  };
  const axisGoal = (): Region | null => {
    for (const a of axisList) {
      const r = g.byName[a];
      if (r && !allied(g, nat, ctl(g, r))) return enemies.has(r.ctrl) && atWar(g, nat, r.ctrl) && !failed.has(r.id) ? r : null;
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
        // (In a main effort the force massed opposite the target is the attack.)
        if (scope && massed.has(c.id) && !striking) continue;
        // Early in the month the fast troops are kept for the main thrust if they can get to it.
        const kept = door !== null && wave <= 2 && KIND[c.kind].mobile && !massed.has(c.id) && door[c.reg] >= 0 && door[c.reg] <= c.ap;
        for (const j of g.regions[c.reg].nb) {
          const r = g.regions[j];
          if (failed.has(j) || halt.has(r.name) || (only && !only.has(r.name)) || !enemies.has(ctl(g, r)) || !atWar(g, nat, ctl(g, r)) || notYet(r.name) || lineShut(g, nat, r)) continue;
          if (kept && !axis.has(r.name)) continue;
          if (blocked(g, c, r)) continue; // no tanks up the steepest ground
          let list = cand.get(j);
          if (!list) cand.set(j, (list = []));
          list.push(c);
        }
      }
      let best: { score: number; target: Region; corps: Corps[]; battle: boolean; wave?: boolean } | null = null;
      for (const [j, avail] of cand) {
        const target = g.regions[j];
        const value = target.vp * 2 + (target.cap ? 8 : 0) + pull[j];
        if (!hostileUnits(g, target, nat).length) {
          const movers = avail
            .filter((c) => c.ap >= Math.min(moveCost(g, c, target), 4) && mayEnter(g, c, target))
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
          // Against works the guns are brought into the attack first (the grand campaign): they are what breaks a dug-in line.
          const works = g.flags.open === 1 && (worksAgainst(g, target, nat) || hostileUnits(g, target, nat).some((u) => u.dug));
          const gun = (c: Corps): number => (works && c.kind === 'art' ? 1 : 0);
          pool.sort((a, b) => gun(b) - gun(a) || (main ? fast(b) - fast(a) : fast(a) - fast(b)) || atkPower(g, b) - atkPower(g, a));
          const least = main ? Math.max(1, pool.filter((c) => fast(c)).length) : 1;
          let pick: Corps[] | null = null, odds = 0, ready = 0;
          for (let k = least; k <= pool.length; k++) {
            const a = assess(g, pool.slice(0, k), target);
            odds = a.odds; ready = a.prepared;
            if (odds >= 2.6) { pick = pool.slice(0, k); break; }
          }
          // Against a line that is dug in and fed the staff asks for longer odds (the grand campaign), and it does
          // not come straight back at a town whose siege has just been raised unless it has much the stronger force.
          let wave = false;
          if (!pick) {
            const sore = g.flags.open === 1 && g.turn - (g.flags[`relieved:${j}`] ?? -99) <= SIEGE.ai.relieved.months ? SIEGE.ai.relieved.odds : 0;
            const need = (oddsFor[target.name] ?? minOdds) + DEFENCE.staff.odds * ready + sore;
            if (odds >= need) pick = pool;
            else {
              // Not at these odds in one blow: in two, if there are the corps for it.
              const two = single || (waved.get(j) ?? 0) >= waves - 1 ? null : twoWaves(g, avail, pool, target, need);
              if (!two) continue;
              pick = two.corps; odds = two.then; wave = true;
            }
          }
          // Tanks do not go alone at a gunned line: infantry goes in with them, or they do not go.
          if (!wave) {
            const sent = notTanksAlone(g, pick, pool, target);
            if (!sent) continue;
            if (sent !== pick) { pick = sent; odds = assess(g, pick, target).odds; }
          }
          // (A place that needs two waves is a dearer prize than one that falls to one.)
          const score = value + Math.min(odds, 4) - (wave ? 0.5 : 0);
          if (!best || score > best.score) best = { score, target, corps: pick, battle: true, wave };
        }
      }
      if (!best) break;
      let taken: boolean;
      if (best.battle) {
        taken = resolveBattle(g, best.corps, best.target).taken;
        // A slow command does not get a second assault organised in the same month, and some plans allow only one.
        // (Two waves laid on beforehand are one operation: the main body goes in behind the first wave.)
        if (best.wave) waved.set(best.target.id, (waved.get(best.target.id) ?? 0) + 1);
        else if (!taken && (g.nations[nat].slowCommand || single)) failed.add(best.target.id);
      }
      else { taken = moveCorps(g, best.corps[0], best.target); if (!taken) failed.add(best.target.id); }
      if (taken && g.nations[nat].alive) support(g, nat, best.target);
    }
    // Nothing left to hit this week: close up to the front, then look again.
    if (!follow || !g.nations[nat].alive || !advance(g, nat, enemies, wave, doorstep(g, nat, axisGoal()), halt)) break;
   }
  }
}

/**
 * A quiet front is not a truce. A staff standing on the defensive still takes a weak point when it sees one: a
 * province next to its line that nobody holds, or one where a single attack has good odds. A few a month and no
 * more, with no pursuit: the line is put in order again afterwards.
 */
function localAttacks(g: Game, nat: string): void {
  const P = probing(g, nat);
  if (!P) return;
  const failed = new Set<number>();
  for (let made = 0; made < P.most && g.nations[nat].alive; made++) {
    ensureSupply(g);
    const cand = new Map<number, Corps[]>();
    for (const c of g.corps) {
      if (c.nat !== nat || c.dead || c.hold || c.kind === 'fort' || c.ap <= 0 || c.org < 0.35) continue;
      for (const j of g.regions[c.reg].nb) {
        const r = g.regions[j];
        const lord = ctl(g, r);
        if (failed.has(j) || !P.enemies.has(lord) || !atWar(g, nat, lord) || aside(g, r, nat) || blocked(g, c, r) || lineShut(g, nat, r)) continue;
        let list = cand.get(j);
        if (!list) cand.set(j, (list = []));
        list.push(c);
      }
    }
    let best: { score: number; target: Region; corps: Corps[]; battle: boolean } | null = null;
    for (const [j, avail] of cand) {
      const target = g.regions[j];
      const value = target.vp + (target.cap ? 4 : 0);
      if (!hostileUnits(g, target, nat).length) {
        // Nobody there: one corps walks in, if it leaves somebody behind where it stood.
        const mover = avail.filter((c) => c.ap >= Math.min(moveCost(g, c, target), 4) && mayEnter(g, c, target) && friendlyUnits(g, g.regions[c.reg], nat).length > 1).sort((a, b) => b.ap - a.ap)[0];
        if (mover && (!best || 5 + value > best.score)) best = { score: 5 + value, target, corps: [mover], battle: false };
        continue;
      }
      const pool = limitAttackers(g, avail, target).sort((a, b) => atkPower(g, b) - atkPower(g, a));
      if (!pool.length) continue;
      let pick: Corps[] = pool, odds = 0;
      for (let k = 1; k <= pool.length; k++) {
        odds = assess(g, pool.slice(0, k), target).odds;
        if (odds >= 2.6) { pick = pool.slice(0, k); break; }
      }
      if (odds < P.odds) continue;
      const sent = notTanksAlone(g, pick, pool, target);
      if (!sent) continue;
      if (sent !== pick) { pick = sent; odds = assess(g, pick, target).odds; }
      const score = Math.min(odds, 4) + value;
      if (!best || score > best.score) best = { score, target, corps: pick, battle: true };
    }
    if (!best) break;
    const foe = ctl(g, best.target);
    const taken = best.battle ? resolveBattle(g, best.corps, best.target).taken : moveCorps(g, best.corps[0], best.target);
    if (!taken) failed.add(best.target.id);
    probed(g, nat, foe, P.step);
    if (taken && g.nations[nat].alive) support(g, nat, best.target);
  }
}

/**
 * A staff softens a prepared line before it attacks it (the grand campaign), as the player can. Where the defenders
 * stand in fieldworks and the attack the staff means to make falls short of the odds it asks, though not by far,
 * one corps that is not going in with the main body probes the line first: the defenders are named, their works
 * are a month the worse, and they meet the attack already pressed. A few probes a month and no more.
 */
export function softenLine(g: Game, nat: string, enemies: Set<string>, halt: Set<string>, only: Set<string> | null, minOdds: number, oddsFor: Record<string, number> = {}): void {
  if (!rivalRules(g) || !probeRules(g) || nat === g.human) return;
  const S = RIVALS.soften;
  const cand = new Map<number, Corps[]>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.hold || c.kind === 'fort' || c.ap <= 0 || c.town !== undefined) continue;
    if (scope && !scope.has(c.reg)) continue;
    for (const j of g.regions[c.reg].nb) {
      const r = g.regions[j];
      if (halt.has(r.name) || (only && !only.has(r.name)) || !enemies.has(ctl(g, r)) || !atWar(g, nat, ctl(g, r)) || blocked(g, c, r) || lineShut(g, nat, r)) continue;
      let list = cand.get(j);
      if (!list) cand.set(j, (list = []));
      list.push(c);
    }
  }
  const wants: { target: Region; prober: Corps; gap: number }[] = [];
  for (const [j, avail] of cand) {
    const target = g.regions[j];
    const foes = hostileUnits(g, target, nat);
    if (!foes.length || Math.max(...foes.map(worksOf)) < S.works) continue;
    const pool = limitAttackers(g, avail.filter((c) => c.org >= 0.35), target);
    if (!pool.length) continue;
    const odds = assess(g, pool, target).odds;
    const need = oddsFor[target.name] ?? minOdds;
    if (odds >= need || odds < S.from * need) continue;
    // Who probes: a corps beside the line that is not going in with the main body, or one of them where everyone is:
    // the one with the weight for a full probe, and of those the weakest in the attack.
    const free = avail.filter((c) => whyNoProbe(g, c, target) === null);
    const spare = free.filter((c) => !pool.includes(c));
    const prober = [...(spare.length ? spare : free.length > 1 ? free : [])].sort((a, b) => probeWeight(b) - probeWeight(a) || atkPower(g, a) - atkPower(g, b) || a.id - b.id)[0];
    if (prober) wants.push({ target, prober, gap: need - odds });
  }
  wants.sort((a, b) => a.gap - b.gap || a.target.id - b.target.id);
  const used = new Set<number>();
  let made = 0;
  for (const w of wants) {
    if (made >= S.most) break;
    if (used.has(w.prober.id) || whyNoProbe(g, w.prober, w.target) !== null) continue;
    if (probeLine(g, w.prober, w.target)) { used.add(w.prober.id); made++; }
  }
}

/**
 * A computer staff folds its worn corps together (the grand campaign), as the player can: where two of its corps
 * of a kind stand in one province, both well under strength, the men of both fill one and the other goes home as
 * a cadre, to be raised again when the treasury allows (economy.ts, aiProduction). Nobody is folded where men
 * would be left over.
 */
export function foldWorn(g: Game, nat: string): void {
  if (!rivalRules(g) || !foldRules(g) || nat === g.human) return;
  const worn = RIVALS.fold.worn;
  const seen = new Set<number>();
  for (const c of [...g.corps]) {
    if (c.nat !== nat || c.dead || c.str >= worn || seen.has(c.reg)) continue;
    seen.add(c.reg);
    const here = g.regions[c.reg].units.filter((u) => u.nat === nat && !u.dead && u.str < worn);
    let best: { keep: Corps; gone: Corps; str: number } | null = null;
    for (const keep of here) for (const gone of here) {
      if (keep === gone || whyNotFold(g, keep, gone) !== null) continue;
      const v = foldView(g, keep, gone);
      if (v.back > 0) continue;
      if (!best || v.str > best.str || (v.str === best.str && (keep.xp > best.keep.xp || (keep.xp === best.keep.xp && keep.id < best.keep.id)))) best = { keep, gone, str: v.str };
    }
    if (best) foldCorps(g, best.keep, best.gone);
  }
}

/** A spearhead alone is a spearhead lost: bring a second corps up behind it when the enemy is close. */
function support(g: Game, nat: string, at: Region): void {
  if (!allied(g, nat, ctl(g, at))) return;
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
    if (scope ? !scope.has(r.id) || !allied(g, nat, ctl(g, r)) : ctl(g, r) !== nat || aside(g, r, nat)) continue;
    if (entrusted.has(r.id) || given.has(r.id)) continue; // an ally holds this stretch now, or the plan has written it off
    let open = false, walled = false;
    for (const j of r.nb) {
      const t = g.regions[j];
      if (!enemies.has(ctl(g, t)) || !atWar(g, nat, ctl(g, t)) || halt.has(t.name)) continue; // a sector under a halt order draws no one
      if (t.fort && t.fort.vs.includes(r.owner) && t.fort.lv >= 2 && worksAgainst(g, t, nat)) walled = true; else open = true;
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
      if (!allied(g, nat, ctl(g, g.regions[j])) || !within(g.regions[j])) continue;
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
    // (A war for a strip of frontier is fought by the troops near it: the rest of the army stays where it stands.)
    if (near !== null && !(dist[c.reg] <= near)) continue;
    if (door && KIND[c.kind].mobile && door[c.reg] > 0) {
      // Fast troops close up on the main thrust's next objective.
      for (let step = 0; step < 6 && c.ap > 0 && door[c.reg] > 0; step++) {
        let to: Region | null = null;
        for (const j of g.regions[c.reg].nb) {
          const r = g.regions[j];
          if (door[j] < 0 || door[j] >= door[c.reg] || friendlyUnits(g, r, nat).length >= STACK || blocked(g, c, r)) continue;
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
        if (u >= STACK || blocked(g, c, r)) continue;
        if (dist[r.id] < bd || u < bu) { to = r; bd = dist[r.id]; bu = u; }
      }
      if (to && railToward(g, c, to)) { made++; continue; }
    }
    for (let step = 0; step < 6 && c.ap > 0; step++) {
      const here = c.reg;
      if (atFront[here] || dist[here] >= 1e9) break;
      let to: Region | null = null, bd = dist[here], bu = 99;
      for (const j of g.regions[here].nb) {
        const r = g.regions[j];
        if (!allied(g, nat, ctl(g, r)) || dist[j] >= bd + (to ? 0.001 : 0)) continue;
        const u = friendlyUnits(g, r, nat).length;
        if (u >= STACK || blocked(g, c, r)) continue;
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
  const engaged = (c: Corps): boolean => g.regions[c.reg].nb.some((j) => enemies.has(ctl(g, g.regions[j])) && atWar(g, nat, ctl(g, g.regions[j])));
  const free = (): Corps[] => g.corps.filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap >= 4 && !engaged(c));
  const room = (r: Region): boolean => allied(g, nat, ctl(g, r)) && friendlyUnits(g, r, nat).length < STACK;
  const nearest = (list: Corps[], to: Region): Corps | null => {
    const d = hops(g, to.id, (r) => allied(g, nat, ctl(g, r)));
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
    if (c.reg !== r.id && (!railToward(g, c, r) || c.reg !== r.id)) continue;
    c.hold = true;
  }
  // Small forces kept at a jumping-off point for a later operation.
  const staged = new Set<number>();
  // (The grand campaign: whoever already stands at one of these places stays there. The watch in one harbour is
  // not put on a train to make up the force wanted at the next.)
  if (isOpen(g)) for (const name of Object.keys(ph.staging ?? {})) {
    const r = g.byName[name];
    if (r && allied(g, nat, r.ctrl)) for (const u of r.units) if (u.nat === nat && !u.hold) staged.add(u.id);
  }
  for (const [name, count] of Object.entries(ph.staging ?? {})) {
    const r = g.byName[name];
    if (!r || !allied(g, nat, r.ctrl)) continue;
    const there = r.units.filter((u) => u.nat === nat && !u.hold);
    there.forEach((u) => staged.add(u.id));
    for (let k = there.length; k < count; k++) {
      const c = nearest(free().filter((c) => c.kind === 'inf' && !staged.has(c.id)), r);
      if (!c || !railToward(g, c, r)) break;
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
    if (to) railToward(g, c, to);
  }
}

// ---- Defend ---------------------------------------------------------------------------

export function defPower(g: Game, c: Corps, r: Region): number {
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
  const mine = (r: Region): boolean => (scope ? scope.has(r.id) && allied(g, nat, ctl(g, r)) : !aside(g, r, nat) && (ctl(g, r) === nat || (r.owner === nat && allied(g, nat, ctl(g, r)))));
  for (const r of g.regions) {
    if (!mine(r)) continue;
    for (const j of r.nb) {
      const e = g.regions[j];
      if (!atWar(g, nat, ctl(g, e)) || masked.has(j)) continue; // a garrison the plan leaves to others threatens nobody
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
      const gap = here.nb.map((j) => g.regions[j]).find((r) => atWar(g, nat, ctl(g, r)) && lost(r) && !late(r) && !hostileUnits(g, r, nat).length && c.ap >= Math.min(moveCost(g, c, r), 4) && (allowed(g, r) || forsaken(g, nat, r)));
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
    const d = hops(g, here.id, (r) => allied(g, nat, ctl(g, r)) && within(r));
    let to: Region | null = null, bs = 0;
    for (const r of g.regions) {
      if (!mine(r) || d[r.id] <= 0 || friendlyUnits(g, r, nat).length >= STACK) continue;
      const s = need(r) / (1 + 0.5 * d[r.id]);
      if (s > bs) { bs = s; to = r; }
    }
    if (!to) continue;
    const before = c.reg;
    // There are no railways in the desert: overseas, everything marches.
    if (!scope && !n.slowCommand && d[to.id] > 2 && c.ap >= 4) railToward(g, c, to);
    else walkToward(g, c, to);
    if (c.reg !== before) { held[here.id] -= spared; held[to.id] += defPower(g, c, to); }
  }

  // Counter-attack where the odds are clearly good. A counter-attack answers a fresh loss: ground
  // the enemy has held for months is a matter for a planned offensive, not for the local commander.
  for (const r of g.regions) {
    if (!n.alive || quiet) return;
    if (scope && !scope.has(r.id)) continue;
    // (A town of the nation's own side under siege is relieved whenever the odds allow, however long the siege has lasted.)
    const shut = besieged(g, r) && allied(g, nat, r.ctrl);
    if (!atWar(g, nat, ctl(g, r)) || !lost(r) || (!shut && (late(r) || g.turn - r.since > COUNTER_WINDOW)) || !hostileUnits(g, r, nat).length || !allowed(g, r) || lineShut(g, nat, r)) continue;
    // Troops that have already fought this month are in no state to be thrown straight into another attack.
    // In free play the force a staff keeps at a place stays there: it is not thrown into a counter-attack next door.
    const kept = isOpen(g) ? massed : null;
    const near = g.corps.filter((c) => c.nat === nat && !c.hold && c.kind !== 'fort' && c.ap > 0 && !c.fought && c.org >= 0.6 && !kept?.has(c.id) && g.regions[c.reg].nb.includes(r.id));
    const cut = limitAttackers(g, near, r);
    // (The local commander's tanks do not go alone at a gunned line any more than the army's: the infantry next door goes in with them, or he waits.)
    const pool = cut.length ? footBeside(g, cut, near, r) ?? [] : [];
    // Overseas a local counter-stroke sends two corps in to hold what it wins: the rest keep the door open behind them.
    // (The grand campaign: an army goes to the relief of a town at shorter odds than it would counter-attack, and of its capital at shorter still.)
    const bar = shut && siegeRules(g) ? reliefBar(g, r) : 1.8;
    if (pool.length && assess(g, pool, r).odds >= bar) resolveBattle(g, pool, r, scope ? { occupy: 2 } : {});
  }
}

/**
 * The grand campaign: the odds at which a staff goes to the relief of a besieged town of its own side, the garrison's
 * sortie counted in them: a capital's, and a great city's like a capital's (ruling 139), are the shorter.
 */
export const reliefBar = (g: Game, r: Region): number => (townSize(g, r) === 'capital' || greatCity(g, r) ? SIEGE.ai.relief.capital : SIEGE.ai.relief.other);

/**
 * True for ground the enemy has abandoned altogether: an island of his behind the front with none of his troops
 * anywhere on it. An army kept strictly to its orders may still walk in and take such a place back.
 */
function forsaken(g: Game, nat: string, r: Region): boolean {
  const seen = new Set([r.id]);
  const q = [r.id];
  for (let i = 0; i < q.length; i++) {
    if (q.length > 12) return false; // too big to be an island: this is the enemy's country, not a pocket he left
    const x = g.regions[q[i]];
    if (hostileUnits(g, x, nat).length) return false;
    for (const j of x.nb) if (!seen.has(j) && atWar(g, nat, g.regions[j].ctrl)) { seen.add(j); q.push(j); }
  }
  return true;
}

/**
 * By rail to a region. In the grand campaign, where the line may be cut on the way, as near to it as the trains
 * still run: the corps marches on from the end of the line next month.
 */
function railToward(g: Game, c: Corps, to: Region): boolean {
  if (railMove(g, c, to)) return true;
  if (!groundRules(g) || c.ap < 4) return false;
  const reach = railReach(g, c);
  if (!reach.size) return false;
  const d = hops(g, to.id, (r) => allied(g, c.nat, ctl(g, r)));
  let best: Region | null = null, bd = d[c.reg] < 0 ? 1e9 : d[c.reg];
  for (const id of reach) {
    const r = g.regions[id];
    if (d[id] < 0 || d[id] >= bd || !hasRoom(g, r, c)) continue;
    best = r; bd = d[id];
  }
  return !!best && railMove(g, c, best);
}

function walkToward(g: Game, c: Corps, to: Region): void {
  // The way there, going round any region too packed with troops to march through.
  const d = hops(g, to.id, (r) => allied(g, c.nat, ctl(g, r)) && within(r) && (r.id === c.reg || friendlyUnits(g, r, c.nat).length < STACK));
  for (let step = 0; step < 6 && c.ap > 0 && c.reg !== to.id; step++) {
    let next: Region | null = null;
    for (const j of g.regions[c.reg].nb) {
      if (d[j] < 0 || d[j] >= d[c.reg]) continue;
      const r = g.regions[j];
      if (friendlyUnits(g, r, c.nat).length >= STACK || blocked(g, c, r)) continue;
      if (!next || d[j] < d[next.id]) next = r;
    }
    if (!next || !moveCorps(g, c, next)) return;
  }
}

/** An expeditionary corps goes home when it is cut off, broken, or its host is collapsing. */
export function wantsOut(g: Game, c: Corps): boolean {
  if (g.regions[c.reg].owner === c.nat || c.dead) return false;
  // The men of a second front went ashore to stay: they come off only when cut off or broken.
  if (g.flags[`sf:${c.id}`]) return !c.supplied || c.org < 0.3;
  if (theatreOf(g, c) >= 0) {
    // An army with a plan of its own in a side theatre is there to stay, unless that plan is to get out once the
    // front has gone: then it leaves when the enemy holds one of the places the plan names, when it is cut off,
    // or when the country it stands in has fallen. Whoever is already in one of the theatre's havens stays there.
    if (!scope || !leaving || !scope.has(c.reg) || havensOf(g.regions[c.reg]).includes(g.regions[c.reg].name)) return false;
    return !c.supplied || !g.nations[g.regions[c.reg].owner].alive || leaving.when.some((nm) => !!g.byName[nm] && atWar(g, c.nat, g.byName[nm].ctrl));
  }
  // Beyond Europe, an army on ground it has taken from a country it is still at war with, or in the field round one of
  // that country's towns it besieges, is nobody's guest: it holds its conquest or keeps its siege, and comes off only
  // when cut off or broken. (Japan in China has no ally on the continent to be its host, and without this its whole
  // army sailed home in September 1939; reading the province's holder alone, a besieger sailed home while the town
  // held: ruling 224, b387. Reading the field's alone, a garrison besieged in a town it took would.) Europe keeps the
  // host rule below.
  const here = g.regions[c.reg];
  if (beyondEurope(here) && (here.ctrl === c.nat || fieldCtrl(g, here) === c.nat) && atWar(g, c.nat, here.owner)) return !c.supplied || c.org < 0.3;

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
  const d = hops(g, c.reg, (r) => allied(g, c.nat, ctl(g, r)));
  return d[H.capital] < 0 || collapseScore(g, host) >= 0.7 * H.resilience;
}

function evacuations(g: Game, nat: string): void {
  ensureSupply(g);
  const out = g.corps.filter((c) => c.nat === nat && (!scope || scope.has(c.reg)) && wantsOut(g, c));
  if (!out.length) return;
  const stuck: Corps[] = [];
  for (const c of out) {
    if (!g.regions[c.reg].port) {
      const d = hops(g, c.reg, (r) => allied(g, c.nat, ctl(g, r)) && within(r));
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
      if (!r.port || !atWar(g, nat, r.ctrl) || lineShut(g, nat, r)) continue;
      const k = trapped.filter((c) => g.regions[c.reg].nb.includes(r.id)).length;
      if (k > most) { most = k; goal = r; }
    }
    if (!goal) return;
    const force = limitAttackers(g, trapped.filter((c) => g.regions[c.reg].nb.includes(goal!.id)), goal);
    // Nobody who can get at the port (tanks below a mountain, a river with its bridges down): there is no way out this month.
    if (!force.length) return;
    if (hostileUnits(g, goal, nat).length) {
      if (assess(g, force, goal).odds < 1) return;
      if (!resolveBattle(g, force, goal).taken) return;
    } else if (!moveCorps(g, force.find((c) => mayEnter(g, c, goal!)) ?? force[0], goal)) return;
    for (const c of trapped) {
      if (c.dead) continue;
      if (c.reg !== goal.id && g.regions[c.reg].nb.includes(goal.id)) moveCorps(g, c, goal);
      if (c.reg === goal.id) evacuate(g, c);
    }
  }
}
