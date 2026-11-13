// The doorway between the campaign rules and the Unreal game.
//
// The rules are the browser prototype's own (src/campaign). Nothing here decides anything about
// the war: it asks the rules, gives them the player's orders, and hands back plain pictures of
// the result as JSON. Tools/build_rules.sh bundles this file and everything it imports into
// Content/Rules/campaign-rules.js, which the game runs.
//
// The Unreal side calls these by name through a global object, TC:
//
//   TC.starts()                         -> { years: [{ year, title, from, note }], groups: [{ title, from, view, sides: [{ nat, name, blurb, aims }] }] }
//   TC.grand(from, seed)                -> { from, nations: [{ nat, name, major, blurb, standing, corps, regions }] }: who can be led in the grand campaign from that month
//   TC.start({ human, from, view, seed }) -> view. view 'grand' is the grand campaign: history up to `from`, and free play after it
//   TC.save()                           -> { text }: the whole campaign as text
//   TC.peek(text)                       -> { human, name, date, open } for a saved campaign, or null
//   TC.load(text)                       -> view, or null if the save cannot be read
//   TC.view()                           -> view
//   TC.reach([corpsId, ...])            -> { regions: [{ id, cost, attack, prev }] }
//   TC.move([corpsId, ...], regionId)   -> { moved: [{ id, path }], view }
//   TC.previewAttack([corpsId, ...], regionId) -> the odds and the Battle Ticket, or null
//   TC.attack([corpsId, ...], regionId) -> { report, note, view }
//   TC.endMonth()                       -> { steps, headlines, events, fallen, view }
//   TC.stance([corpsId, ...], posture)  -> view: stand, dig in ('dig') or fall back ('back') when attacked
//   TC.railReach([corpsId, ...])        -> { regions: [id, ...] } the picked corps could go to by rail
//   TC.rail([corpsId, ...], regionId)   -> { moved, view }
//   TC.province(regionId)               -> what a territory holds, and what the player can raise or build there
//   TC.raise(kind, regionId)            -> { ok, view }: orders a new corps
//   TC.fortify(regionId)                -> { ok, view }: orders the next level of fortification
//   TC.cancel(buildId)                  -> { ok, view }: takes back an order placed this month
//   TC.arsenal(regionId)                -> everything the player can order with that province in mind: corps, aircraft, ships and works
//   TC.order(key, regionId)             -> { ok, name, view }: places one of those orders
//   TC.ledger()                         -> the player's books: money in by source, upkeep by arm, what is left, every province's yield, what is on order
//   TC.declare(nation)                  -> { ok, view }: the player declares war. view.declare lists who it can be declared on
//   TC.diplomacy()                     -> the foreign office: the powers at war, every country at peace on the track, political capital
//   TC.court(nation)                    -> { ok, note, view }: courts a neutral
//   TC.warPlan(nation, regionId?)       -> { ok, lead, friends, lines }: what declaring war would mean, for the question asked first
//   TC.record(corpsId)                  -> one corps' service record: what it is, its honours, its diary
//   TC.roll()                           -> the roll of honour and the memorial wall of the player's nation

import { allied, atWar, bloc, collapseScore, createGame, fieldArmy, men, railReach as railReachOf } from '../../../src/campaign/game';
import { FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, inService, lead, monthsAhead, nextStep, stepsOf, teamsFree, teamsTotal } from '../../../src/campaign/research';
import type { Field, Skill } from '../../../src/campaign/types';
import { factoriesBuilt, workLevel, yardLevel } from '../../../src/campaign/arsenal';
import { hasTech, idleTeams, techOf } from '../../../src/campaign/tech';
import { eliteOf, traitRule, traitRules, traitsOf, eliteRule } from '../../../src/campaign/traits';
import { LEAGUE, leagueFounded, leagueMembers, type PathId } from '../../../src/campaign/league';
import { DIPLOMACY, TRACK, TRACK_LABEL, TRACK_PHRASE, canCourt, courtable, friendsOf, momentum, pcIncome, receptive, step, tradePartners } from '../../../src/campaign/diplomacy';
import {
  arsenalOffers, beginEve, beginHuman, canOrder, candidates, current, declarable, endEve, endHuman, humanUp, onEve, orderAttack, orderBuildCorps, orderArsenal, orderDeclare,
  orderBuildFort, orderCancelBuild, orderMove, orderRail, orderStance, postureOf, previewAttack, reach, runTurnAI, stepAI,
  orderAir, orderAirBase, orderFleet, levelNow, orderLevel, orderLand, orderShip, previewLanding,
  depots, shippingLanes, previewAirOrder,
  sight, secretService, orderRecruitAgent, orderSendAgent, orderAgentMission, orderRetireAgent, orderFog, whyNotSendAgent,
  lastAct, standings, orderDomination, orderKeepPeace, whyNotOrderDomination,
  orderBureau, orderTeams, techTree, orderResearch, orderStopResearch, whyNotOrderResearch, orderAssign, orderPromote, orderRelieve, orderSkill,
  invasions, previewDeclare, orderCourt, leagueInvites, leaguePartners, orderLeagueInvite, orderPath, paths, whyNotLeagueInvite,
  engineerJobs, orderBlowBridges, orderCutRailway, orderEngineers, orderStopWork, whyNotBlowBridges, whyNotCutRailway,
} from '../../../src/campaign/turn';
import { jobLine, type JobKind } from '../../../src/campaign/engineers';
import { BUDGET, PRODUCTION, budgeted, canRaise, corpsUpkeep, fortCost, income, isHome, ledger as ledgerOf, provinceYield } from '../../../src/campaign/economy';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, rankOf, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../../../src/campaign/commanders';
import { STORIES, awardTitle, citation, designation, diaryText, fameOf, grade, honourDef, honoursOf, isGuards, stat, takeOver } from '../../../src/campaign/stories';
import { KIND } from '../../../src/campaign/types';
import { GOALS, GOAL_KEYS, judgeGoals, openGoals } from '../../../src/campaign/goals';
import { isOpen, startOpen } from '../../../src/campaign/staff';
import { loadGame, peekSave, saveGame } from '../../../src/campaign/save';
import { GROUND, groundRules, groundSays } from '../../../src/campaign/ground';
import { CROSSINGS, RIVERS, bridgeDown, riverNotes } from '../../../src/campaign/rivers';
import { RAIL_LINKS, railCut, railNote } from '../../../src/campaign/rails';
import { fleetReach, landingTargets, whyNotShip } from '../../../src/campaign/navy';
import { AIR, MISSION, airBases, airRange, airTargets, forecastAir, km as airKm, whyNotMission } from '../../../src/campaign/air';
import type { AirMission, Stance } from '../../../src/campaign/types';
import { DIFFICULTY } from '../../../src/campaign/difficulty';
import { airAct, airPage } from '../../../src/campaign/airpages';
import { airfieldsSeen, flownRaids } from '../../../src/campaign/turn';
import { STARTS } from './starts';
import grandData from '../../../data/campaign/grand.json';
import { turnToDate } from '../../../src/core/ticket';
import type { BattleReport, Build, Corps, Game, Kind, Posture } from '../../../src/campaign/types';

let g: Game | null = null;
/** The player's nation, once it has been beaten. The war goes on with nobody at the controls. */
let fallenNation: string | null = null;

function game(): Game {
  if (!g) throw new Error('No campaign has been started');
  return g;
}

/** Where a nation stands with the player: own, ally, enemy or neutral. */
function standingOf(game: Game, nat: string): string {
  const me = game.human;
  if (!me) return 'neutral';
  if (nat === me) return 'own';
  if (atWar(game, me, nat)) return 'enemy';
  return allied(game, me, nat) ? 'ally' : 'neutral';
}

/** The picture of the war the game draws. */
function view() {
  const s = game();
  const up = humanUp(s);
  const seen = sight(s);
  // Where air groups are based: each such place is an airfield on the table.
  const based = new Set(s.airFleets.filter((f) => f.ftr + f.bmr > 0).map((f) => f.base));
  // The airfields as the player knows them (the grand campaign): the size of each, and which the fog hides.
  const fields = airfieldsSeen(s);
  const fieldSize = new Map(fields.map((a) => [a.region, a.size] as const));
  const fieldHidden = new Set(fields.filter((a) => a.groups < 0).map((a) => a.region));
  return {
    rules: 'campaign',
    turn: s.turn,
    week: s.week,
    date: turnToDate(s.turn).label,
    human: s.human,
    current: current(s),
    humanUp: up,
    eve: onEve(s),
    open: isOpen(s),
    level: isOpen(s) && s.human ? levelNow(s).name : '',
    fallen: fallenNation,
    // The player's treasury: production points in hand, what the factories earn a month, and what is on order.
    treasury: s.human ? treasuryOf(s, s.human) : null,
    // The countries the player could declare war on this month: those at peace with him that he borders or can reach by sea.
    declare: up ? declarable(s) : [],
    // Bridges that are down (by their place in the list of crossings) and provinces whose railway is cut.
    down: groundRules(s) ? CROSSINGS.map((c, i) => (bridgeDown(s, c) ? i : -1)).filter((i) => i >= 0) : [],
    cut: groundRules(s) ? s.regions.filter((r) => railCut(s, r.id)).map((r) => r.id) : [],
    // Each region: who holds it, and the works built there since the war began that the table shows as models:
    // [factories built, shipyard level, anti-aircraft level, airfield level]. Left out where there are none.
    regions: s.regions.map((r) => {
      const w = [factoriesBuilt(s, r.id), yardLevel(s, r), workLevel(s, 'aa', r.id), fields.length ? fieldSize.get(r.id) ?? 0 : Math.max(workLevel(s, 'field', r.id), based.has(r.id) ? 1 : 0)];
      return w.some((x) => x > 0) ? { id: r.id, ctrl: r.ctrl, stand: standingOf(s, r.ctrl), w } : { id: r.id, ctrl: r.ctrl, stand: standingOf(s, r.ctrl) };
    }),
    // Under the fog of war the player's map shows only what he is allowed to know: an army seen whole, an army
    // only glimpsed (a flag and how many corps, 'dim'), or nothing at all.
    corps: s.corps.filter((c) => !c.dead && seen[c.reg] > 0).map((c) => seen[c.reg] === 1
      ? { id: c.id, nat: c.nat, name: 'An army seen from afar', kind: 'inf', elite: '', divs: 0, str: 0, org: 0, reg: c.reg, ap: 0, apMax: 0, gen: null, posture: postureOf(c), canOrder: false, supplied: true, men: 0, xp: 0, grade: '', nick: null, honours: [], dim: true }
      : ({
      id: c.id, nat: c.nat, name: c.name, kind: c.kind, elite: c.elite ?? '', divs: c.divs, str: c.str, org: c.org, reg: c.reg,
      ap: c.ap, apMax: c.apMax, gen: generalOf(s, c)?.name ?? null, posture: postureOf(c),
      canOrder: up && canOrder(s, c), supplied: c.supplied, men: men(c),
      // Its story so far: how seasoned it is, the name the army gives it, and the honours on its colours.
      xp: Math.round(c.xp * 100), grade: grade(c.xp), nick: c.nick ?? null,
      honours: honoursOf(s, c).filter((a) => !a.old).map((a) => ({ title: awardTitle(s, a), ribbon: honourDef(a.hon)?.ribbon ?? ['#b08d57', '#f2ead6'] })),
      dim: false,
    })),
    // The fleets, and the sea each is in (a fleet in harbour lies at its base), and the air fleets on their airfields.
    fleets: s.fleets.filter((f) => f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd + f.ships.ss > 0).map((f) => ({
      id: f.id, nat: f.nat, name: f.name, zone: f.zone, home: f.home, base: f.base, port: f.port, stance: f.stance,
      bb: f.ships.bb, cv: f.ships.cv, ca: f.ships.ca, dd: f.ships.dd, ss: f.ships.ss,
      // For the player's own fleets: the seas each can reach this month, so that a fleet in hand shows where it can go.
      reach: f.nat === s.human && up ? fleetReach(s, f) : [],
    })),
    // The airfields as the player knows them (the grand campaign): where each is, whose, how big (1 to 4), and how
    // many groups and aircraft stand on it, or -1 where the fog hides that.
    airfields: fields.map((a) => ({ region: a.region, nat: a.holder, side: a.side, size: a.size, word: a.word, holds: a.holds, groups: a.groups, aircraft: a.aircraft, crowded: a.crowded })),
    air: s.airFleets.filter((f) => f.ftr + f.bmr > 0 && !fieldHidden.has(f.base)).map((f) => ({
      id: f.id, nat: f.nat, name: f.name, base: f.base, ftr: Math.round(f.ftr), bmr: Math.round(f.bmr), mission: f.mission,
    })),
    // The year each nation's own designers have reached for tanks, fighters and bombers: a nation that pushes a
    // branch fields its later models early, and one that neglects it keeps the old ones longer.
    designs: Object.fromEntries(Object.values(s.nations).filter((n) => n.alive && (n.lab || techOf(s, n.id))).map((n) => {
      const year = 1939 + Math.floor((Math.max(1, s.turn) + 7) / 12);
      if (techOf(s, n.id)) {
        // The grand campaign: the technology tree decides. No medium tanks, no later tank; heavy tanks bring the
        // heavy models as soon as they are researched, and so with long-range fighters and four-engined bombers.
        const armour = !hasTech(s, n.id, 'mediums') ? 1939 : !hasTech(s, n.id, 'heavies') ? Math.min(year, 1942) : Math.max(year, 1943);
        const fighters = hasTech(s, n.id, 'escorts') ? Math.max(year, 1943) : Math.min(year, 1941);
        const bombers = hasTech(s, n.id, 'heavybombers') ? Math.max(year, 1942) : Math.min(year, 1941);
        return [n.id, [armour, fighters, bombers]];
      }
      return [n.id, (['armour', 'fighters', 'bombers'] as const).map((f) => 1939 + Math.floor((Math.max(1, s.turn + Math.round(n.lab!.clock[f] - n.lab!.months)) + 7) / 12))];
    })),
    // Things the player should be told as his month opens, and the rooms beyond the main screens that are open to him.
    alerts: alertsOf(s),
    pages: pagesOf(s),
    // Oil and supplies in the player's depots (the grand campaign; null for a small country or a scenario), each with
    // the months it will last and its books in words, and every shipping lane to draw over the sea.
    stores: storesOf(s),
    lanes: s.flags.open === 1 ? shippingLanes(s).filter((l) => l.kind === 'sea' && l.path.length > 1 && l.state !== 'idle').map((l) => ({ path: l.path.flat(), state: l.state, flow: l.flow })) : [],
    // Research teams of the player's with nothing to do: they never choose their own work, so the game says so.
    idle: s.human ? idleTeams(s, s.human) : 0,
    // The members of the League, the third alliance: their ground is shown in its own colour.
    league: leagueMembers(s).map((n) => n.id),
    nations: Object.fromEntries(Object.values(s.nations).map((n) => [n.id, {
      name: n.name, adj: n.adj, alive: n.alive, standing: standingOf(s, n.id), atWar: n.atWar,
    }])),
  };
}

/** Money as the player sees it: a production point is shown as BUDGET.scale (a hundred), in whole numbers. */
const shown = (pp: number): number => Math.round(pp * BUDGET.scale);
const inHand = (pp: number): number => (pp >= 0 ? Math.floor(pp * BUDGET.scale + 1e-6) : shown(pp));

/**
 * The player's treasury in a few numbers, for the plate that is always on screen: what is in hand, what comes
 * in each month, what the forces cost, and what is left. `budget` is true in the grand campaign, where upkeep
 * is charged and the treasury can run into debt; in the scenarios only income is counted.
 */
function treasuryOf(s: Game, nat: string) {
  const L = ledgerOf(s, nat);
  return {
    pp: Math.floor(s.nations[nat].pp),
    income: Math.round(income(s, nat)),
    pc: Math.floor(s.nations[nat].pc),
    orders: s.nations[nat].queue.map((b) => orderLine(s, b)),
    scale: BUDGET.scale,
    budget: L.budget,
    money: inHand(L.treasury),
    gross: shown(L.gross),
    upkeep: shown(L.upkeepTotal),
    net: shown(L.net),
    debt: L.debt,
  };
}

/** The player's books in full. Every amount is money as the player sees it. */
function ledger() {
  const s = game();
  const me = s.human;
  if (!me) return null;
  const L = ledgerOf(s, me);
  const line = (l: { label: string; amount: number; count?: number }) => ({ label: l.label, amount: shown(l.amount), count: l.count ?? 0 });
  const orders = s.nations[me].queue.map((b) => ({ ...orderLine(s, b), cost: shown(b.cost) }));
  return {
    nation: s.nations[me].name,
    date: turnToDate(s.turn).label,
    budget: L.budget,
    money: inHand(L.treasury),
    gross: shown(L.gross),
    upkeepTotal: shown(L.upkeepTotal),
    net: shown(L.net),
    debt: L.debt,
    // How deep the debt may go before the lenders stop, and what it does to the army meanwhile.
    debtFloor: shown(BUDGET.debt.floor * Math.max(1, L.gross)),
    income: L.income.map(line),
    changes: L.changes.map(line),
    upkeep: L.upkeep.map(line),
    provinces: L.provinces.map((p) => ({
      id: p.id, name: p.name, land: shown(p.land), port: shown(p.port), oil: shown(p.oil), fac: shown(p.fac), capital: shown(p.capital), total: shown(p.total),
      home: s.regions[p.id].owner === me,
    })),
    orders,
    onOrder: orders.reduce((sum, o) => sum + o.cost, 0),
  };
}

/** Something on order, as a line: what, where, when it is due, and whether the order can still be taken back. */
function orderLine(s: Game, b: Build) {
  const months = Math.max(0, b.eta - s.turn);
  return {
    id: b.id, what: b.what, name: b.name, region: b.region, where: s.regions[b.region]?.name ?? '',
    due: turnToDate(b.eta).label, months, cost: b.cost, canCancel: b.ordered === s.turn && humanUp(s),
  };
}

const corpsById = (s: Game, ids: number[]): Corps[] =>
  ids.map((id) => s.corps.find((c) => c.id === id)).filter((c): c is Corps => !!c && !c.dead);


/** One formation seen to fly a raid: where from, where to, whose, what flies, and what it meets. */
interface Flight { from: number; x: number; y: number; nat: string; bombers: boolean; escort: boolean; region: number; aa: number; meet: string; meetFrom: number; loss: number }

const lastAirReport = (s: Game): number => s.airReports.reduce((m, r) => Math.max(m, r.id), -1);

/** The raids flown since a report (by its id), as the player may know them: each group that flew, from its own airfield. */
function raidsSince(s: Game, since: number, turns: number[], skipNat: string | null = null): { flights: Flight[]; notes: string[]; focus: number[]; mine: boolean } {
  const flights: Flight[] = [], notes: string[] = [], focus: number[] = [];
  let mine = false;
  const seenIds = new Set<number>();
  for (const turn of [...new Set(turns)]) {
    for (const r of flownRaids(s, turn)) {
      if (r.id <= since || seenIds.has(r.id) || r.attNat === skipNat) continue;
      seenIds.add(r.id);
      const riser = r.defenders.find((d) => d.ftr >= 1 && d.base >= 0);
      let first = true;
      for (const a of r.attackers.filter((x) => x.base >= 0 && x.ftr + x.bmr >= 1).slice(0, 4)) {
        flights.push({
          from: a.base, x: r.x, y: r.y, nat: a.nat, bombers: a.bmr >= 1, escort: a.bmr >= 1 && a.ftr >= 1, region: r.region,
          aa: first && r.region >= 0 ? workLevel(s, 'aa', r.region) : 0, meet: first && riser ? riser.nat : '', meetFrom: first && riser ? riser.base : -1,
          loss: a.bmr >= 1 ? a.lostBmr / a.bmr : a.ftr >= 1 ? a.lostFtr / a.ftr : 0,
        });
        first = false;
      }
      if (first) continue;
      notes.push(r.result);
      if (r.region >= 0) focus.push(r.region);
      if (r.defNat === s.human || (r.region >= 0 && s.regions[r.region].ctrl === s.human)) mine = true;
    }
  }
  return { flights, notes, focus, mine };
}

/** One computer nation's month. If it did something worth seeing, that is noted. */
function stepOne(s: Game, steps: unknown[] | null): void {
  {
    const ctrl = s.regions.map((r) => r.ctrl);
    const r0 = s.reports.length;
    const month = s.turn;
    const stood = new Map(s.corps.filter((c) => !c.dead).map((c) => [c.id, c.reg] as const));
    const air0 = lastAirReport(s);
    const nat = stepAI(s);
    if (s.human && s.turn !== month) judgeGoals(s, s.human);
    if (!steps) return;
    // The raids it flew, or that were flown as its month closed: shown over the map with the rest of what it did.
    const air = raidsSince(s, air0, [month, s.turn]);
    const raidNote = air.notes.slice(0, 2).join(' ');
    const taken = s.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = s.reports.slice(r0);
    if (!taken.length && !reps.length) {
      // Nothing was fought for or taken, but armies the player can see may have marched: that is shown too.
      const seen = sight(s);
      const to = [...new Set(s.corps.filter((c) => !c.dead && c.nat === nat && stood.has(c.id) && stood.get(c.id) !== c.reg && (seen[c.reg] > 0 || seen[stood.get(c.id)!] > 0)).map((c) => c.reg))];
      if (air.flights.length) steps.push({ nat, note: raidNote, taken: [], battles: [], focus: air.focus.length ? air.focus : air.flights.map((f) => f.from), mine: air.mine, raids: air.flights, view: view() });
      else if (to.length) steps.push({ nat, note: `${s.nations[nat].adj} forces are on the move.`, taken: [], battles: [], focus: to.slice(0, 6), mine: false, moved: true, view: view() });
      return;
    }
    const some = (names: string[]): string => (names.length <= 3 ? names.join(', ') : `${names.slice(0, 3).join(', ')} and ${names.length - 3} more`);
    const won = reps.filter((r) => r.taken).map((r) => r.regionName);
    const held = reps.filter((r) => !r.taken).map((r) => r.regionName);
    const walked = taken.filter((id) => !reps.some((r) => r.taken && r.region === id)).map((id) => s.regions[id].name);
    const said = [
      won.length ? `took ${some(won)}` : '',
      walked.length ? `moved into ${some(walked)}` : '',
      held.length ? `${won.length || walked.length ? 'and were' : 'were'} thrown back at ${some(held)}` : '',
    ].filter(Boolean);
    steps.push({
      nat,
      note: `${s.nations[nat].adj} forces ${said.join(', ')}.${raidNote ? ` ${raidNote}` : ''}`,
      taken,
      raids: air.flights,
      battles: reps.map((r) => r.region),
      focus: [...new Set([...taken, ...reps.map((r) => r.region)])],
      // The player's own ground or troops were involved.
      mine: reps.some((r) => r.defNat === s.human || r.defenders.some((u) => u.nat === s.human)) || taken.some((id) => ctrl[id] === s.human),
      view: view(),
    });
  }
}

/** Every computer nation's month until the player is up again, or has fallen. */
function runOthers(s: Game, steps: unknown[] | null): void {
  for (let guard = 0; guard < 80 && !humanUp(s); guard++) {
    if (s.human && !s.nations[s.human].alive) break;
    stepOne(s, steps);
  }
}

/** Opens the player's month once the others have had theirs. Returns the nation that fell, if the player's did. */
function arrive(s: Game): string | null {
  if (s.human) judgeGoals(s, s.human);
  const lost = s.human && !s.nations[s.human].alive ? s.human : null;
  if (lost) {
    // The month the country fell in is played out to its end. The war goes on without the player.
    fallenNation = lost;
    s.human = null;
    while (s.phase > 0) stepAI(s);
  }
  if (humanUp(s)) beginHuman(s);
  return lost;
}

/** The places a campaign can begin, with each playable nation's aims for the campaigns that start in September 1939. */
function starts() {
  return {
    years: GRAND.years.map((y) => ({ year: String(y.year), title: y.date.replace(/^1 /, ''), from: y.turn, note: y.text })),
    // How hard a grand campaign can be made, easiest first, and the level it is on unless another is chosen.
    levels: DIFFICULTY.levels.map((l) => ({ id: l.id, name: l.name, blurb: l.blurb, lines: l.lines })),
    level: DIFFICULTY.start,
    groups: STARTS.map((group) => ({
      ...group,
      sides: group.sides.map((side) => ({
        ...side,
        aims: side.nat && group.from === 1
          ? GOAL_KEYS.map((key) => {
            const goal = GOALS[side.nat!]?.[key];
            return goal ? `${key === 'short' ? 'Short campaign' : 'Long campaign'}: ${goal.text}` : '';
          }).filter(Boolean)
          : [],
      })),
    })),
  };
}

/** The grand campaign as the shared data sets it out: the days on offer, and a word to each great power for each. */
const GRAND = grandData as unknown as {
  years: { year: number; turn: number; date: string; text: string }[];
  nations: { id: string; small?: boolean; years: Record<string, string> }[];
};

/** History played by the computer up to a month, kept as saved text so that it is only played once. */
const histories = new Map<number, string>();
let historySeed = -1;

function historyTo(from: number, seed: number): Game {
  if (seed !== historySeed) {
    histories.clear(); // new dice, a new history
    historySeed = seed;
  }
  let text = histories.get(from);
  if (!text) {
    // Carry on from the latest month already played, if there is one before this.
    let latest = 0;
    for (const month of histories.keys()) if (month < from && month > latest) latest = month;
    const h = (latest ? loadGame(histories.get(latest)!) : null) ?? createGame(seed, null);
    while (h.turn < from) runTurnAI(h);
    text = saveGame(h);
    histories.set(from, text);
  }
  const loaded = loadGame(text);
  if (!loaded) throw new Error('History could not be read back');
  return loaded;
}

/** The war as the grand campaign finds it on the first day of a month: history up to then, nobody at the controls. */
function grandGame(from: number, seed: number, human: string | null): Game {
  // September 1939 is free play from the first day. A later year is history up to the day the player takes
  // over, and from that day no nation follows history's plan: each one's staff decides for itself.
  if (from <= 1) return startOpen(createGame(seed, human));
  return startOpen(historyTo(from, seed));
}

/**
 * Who can be led in the grand campaign from a month: every country still standing. The great powers named in
 * the shared data come first, each with the data's word about what the player is taking on that year, and
 * are offered only in the years the data gives them. Every other country is offered whenever it is still there.
 */
function grand(from = 1, seed = 1939) {
  const h = grandGame(from, seed, null);
  const year = String(GRAND.years.find((y) => y.turn === from)?.year ?? '');
  const order = GRAND.nations.map((n) => n.id);
  // The small countries whose path is their own to choose: led like any other, with a word about each in 1939.
  const smallOnes = new Set(GRAND.nations.filter((n) => n.small).map((n) => n.id));
  const nations = Object.entries(h.nations)
    .filter(([nat, n]) => n.alive && (!order.includes(nat) || smallOnes.has(nat) || GRAND.nations.find((x) => x.id === nat)!.years[year] !== undefined))
    .map(([nat, n]) => {
      const foes = Object.keys(h.nations).filter((other) => other !== nat && h.nations[other].alive && atWar(h, nat, other));
      return {
        nat,
        name: n.name,
        major: order.includes(nat) && !smallOnes.has(nat),
        small: smallOnes.has(nat),
        // What makes the country fight, build and earn in its own way, and its special formation, in words.
        abilities: abilityLines(nat),
        blurb: GRAND.nations.find((x) => x.id === nat)?.years[year] ?? '',
        standing: foes.length ? `At war with ${listNames(foes.map((f) => h.nations[f].name))}` : 'At peace',
        corps: h.corps.filter((c) => c.nat === nat).length,
        regions: h.regions.filter((r) => r.ctrl === nat).length,
      };
    })
    .filter((n) => n.corps > 0 || n.regions > 0)
    .sort((a, b) => (a.major === b.major ? (a.major ? order.indexOf(a.nat) - order.indexOf(b.nat) : a.small === b.small ? a.name.localeCompare(b.name) : a.small ? -1 : 1) : a.major ? -1 : 1));
  return { from, nations };
}

function listNames(names: string[]): string {
  if (names.length <= 2) return names.join(' and ');
  if (names.length > 4) return `${names.slice(0, 3).join(', ')} and ${names.length - 3} more`;
  return `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
}

/** Starts a campaign. With a later starting month, the computer plays history up to it and then hands over. */
function start(options?: { human?: string | null; from?: number; view?: string; seed?: number; level?: string }) {
  const human = options?.human === undefined ? 'GER' : options.human;
  const from = options?.from ?? 1;
  const seed = options?.seed ?? 1939;
  fallenNation = null;
  if (options?.view === 'grand') {
    g = grandGame(from, seed, human);
  } else {
    g = createGame(seed, from > 1 ? null : human);
    if (options?.view === 'open') startOpen(g);
    while (g.turn < from) runTurnAI(g);
  }
  g.human = human;
  // How hard the war is to be (the grand campaign only): set once the player has his seat.
  if (human && options?.level) orderLevel(g, options.level);
  // Honours earned before the player took over are his to carry; their citations are old news.
  if (from > 1) takeOver(g);
  if (human) {
    openGoals(g, human, from);
    // A country that is attacked before its first month comes round places its army on the eve of war.
    if (!(from === 1 && beginEve(g))) {
      runOthers(g, null);
      arrive(g);
    }
  }
  return view();
}

/** The whole campaign as text, to be kept and picked up again later. */
function save() {
  return { text: saveGame(game()) };
}

/** A line about a saved campaign for the opening screen, without loading it. */
function peek(text: string) {
  const p = peekSave(text);
  if (!p) return null;
  const sample = g ?? createGame(1, null);
  return { human: p.human, name: p.human ? sample.nations[p.human]?.name ?? p.human : null, date: turnToDate(p.turn).label, open: p.open };
}

function load(text: string) {
  const loaded = loadGame(text);
  if (!loaded) return null;
  g = loaded;
  fallenNation = null;
  if (g.human && !humanUp(g)) {
    runOthers(g, null);
    arrive(g);
  }
  return view();
}

/** Where a picked-up army could go: anywhere any one of its corps could, and what it could attack. */
function reachOf(ids: number[]) {
  const s = game();
  const merged = new Map<number, { id: number; cost: number; attack: boolean; prev: number; war?: string }>();
  if (humanUp(s)) {
    const cs = corpsById(s, ids);
    for (const c of cs) {
      for (const [id, r] of reach(s, c)) {
        if (id === c.reg) continue;
        const old = merged.get(id);
        if (!old || r.cost < old.cost) merged.set(id, { id, cost: r.cost, attack: r.attack || !!old?.attack, prev: r.prev });
        else if (r.attack) old.attack = true;
      }
    }
    // Ground across a neutral frontier: the army can go there only by declaring war, and the game asks first.
    for (const [id, on] of invasions(s, cs)) {
      if (merged.has(id)) continue;
      const from = cs.find((c) => s.regions[c.reg].nb.includes(id));
      merged.set(id, { id, cost: 0, attack: false, prev: from ? from.reg : -1, war: on });
    }
  }
  return { regions: [...merged.values()] };
}

function move(ids: number[], to: number) {
  const s = game();
  const moved: { id: number; path: number[] }[] = [];
  for (const c of corpsById(s, ids)) {
    const m = reach(s, c);
    const end = m.get(to);
    if (!end || end.attack || to === c.reg) continue;
    const path: number[] = [];
    for (let at = to; at !== c.reg && at >= 0; at = m.get(at)!.prev) path.unshift(at);
    const n = orderMove(s, c, to);
    if (n > 0) moved.push({ id: c.id, path: path.slice(0, n) });
  }
  return { moved, view: view() };
}

/** The player's corps that could join an attack on a region: the ones picked that stand next to it. */
function attackers(s: Game, ids: number[], to: number): Corps[] {
  const able = new Set(candidates(s, s.regions[to]).map((c) => c.id));
  return corpsById(s, ids).filter((c) => able.has(c.id));
}

function preview(ids: number[], to: number) {
  const s = game();
  const corps = attackers(s, ids, to);
  const a = previewAttack(s, corps, s.regions[to]);
  if (!a) return null;
  return {
    region: to,
    attack: a.A,
    defence: a.D,
    odds: a.odds,
    lines: a.lines,
    attackers: corps.map((c) => ({ id: c.id, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: a.defenders.map((c) => ({ id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
  };
}

/** What a battle came to, in a sentence. */
function battleNote(s: Game, r: BattleReport): string {
  if (r.yielded) return `The ${s.nations[r.defNat].adj} troops fell back from ${r.regionName} without a fight.`;
  const cost = `${r.attLost.toLocaleString('en')} lost against ${r.defLost.toLocaleString('en')}`;
  const prisoners = r.prisoners > 0 ? `, ${r.prisoners.toLocaleString('en')} prisoners` : '';
  return `${r.level} at ${r.regionName}: ${r.taken ? 'the ground is taken' : 'the attack is held'}, ${cost}${prisoners}.`;
}

function attack(ids: number[], to: number) {
  const s = game();
  const before = new Map(s.corps.map((c) => [c.id, c.reg]));
  const report = orderAttack(s, attackers(s, ids, to), s.regions[to]);
  // Corps that followed up into the ground they won, for the game to march across.
  const moved = report
    ? s.corps.filter((c) => !c.dead && c.nat === s.human && c.reg === to && before.get(c.id) !== to).map((c) => ({ id: c.id, path: [to] }))
    : [];
  return { report, note: report ? battleNote(s, report) : '', moved, view: view() };
}

function endMonth() {
  const s = game();
  const h0 = s.headlines.length;
  // Who stands as the month ends, to tell afterwards who has been struck from the map and who has come back.
  const stoodUp = new Set(Object.values(s.nations).filter((n) => n.alive && !n.dormant).map((n) => n.id));
  const wasDown = new Set(Object.values(s.nations).filter((n) => !n.alive && !n.dormant).map((n) => n.id));
  const steps: unknown[] = [];
  let fallen: string | null = null;
  if (!s.human) {
    // Nobody at the controls (the player's country has fallen): one whole month of the war, every nation on the computer.
    const month = s.turn;
    for (let guard = 0; guard < 80 && s.turn === month; guard++) stepOne(s, steps);
  } else {
    const air0 = lastAirReport(s), month = s.turn, me = s.human;
    if (onEve(s)) endEve(s);
    else {
      judgeGoals(s, s.human);
      endHuman(s);
    }
    const air = raidsSince(s, air0, [month, s.turn]);
    if (air.flights.length) steps.push({ nat: me, note: air.notes.slice(0, 2).join(' '), taken: [], battles: [], focus: air.focus.length ? air.focus : air.flights.map((f) => f.from), mine: true, raids: air.flights, view: view() });
    runOthers(s, steps);
    fallen = arrive(s);
  }
  const headlines = s.headlines.slice(h0).map((h) => ({
    kind: h.kind,
    // A letter from the front is signed by whoever wrote it.
    text: h.kind === 'letter' ? `\u201C${h.text}\u201D${h.by ? `  ${h.by}` : ''}` : h.text,
    nat: h.nat ?? null, turn: h.turn, week: h.week,
  }));
  // The few lines worth telling first: declarations of war, countries falling, events, honours won and letters home.
  const weight: Record<string, number> = { fall: 0, war: 1, goal: 2, honour: 3, event: 4, letter: 5 };
  const first = headlines.filter((h) => h.kind in weight).sort((a, b) => weight[a.kind] - weight[b.kind]);
  const events = (first.length ? first : headlines).slice(0, 12).map((h) => ({ kind: h.kind, text: h.text }));
  // The great moments of the month, each told on a screen of its own: a nation eliminated, a nation liberated.
  const said = (nat: string): string => [...headlines].reverse().find((h) => h.nat === nat && (h.kind === 'fall' || h.kind === 'event' || h.kind === 'war'))?.text ?? '';
  const announce: { nat: string; title: string; text: string; good: boolean }[] = [];
  for (const n of Object.values(s.nations)) {
    if (n.id === s.human || n.id === fallen) continue;
    const mine = s.human ? allied(s, s.human, n.id) : false;
    const foe = s.human ? atWar(s, s.human, n.id) : false;
    if (stoodUp.has(n.id) && !n.alive) announce.push({ nat: n.id, title: `${n.name.toUpperCase()} HAS FALLEN`, text: said(n.id) || `${upFirst(theName(s, n.id))} is eliminated from the war.`, good: foe });
    else if (wasDown.has(n.id) && n.alive) announce.push({ nat: n.id, title: `${n.name.toUpperCase()} IS LIBERATED`, text: said(n.id) || `${upFirst(theName(s, n.id))} is free again, and back in the war.`, good: mine || !foe });
  }
  return { steps, headlines, events, fallen, announce, view: view() };
}

/** The player's standing order for the picked corps on the day the enemy comes. */
function stance(ids: number[], posture: Posture) {
  const s = game();
  for (const c of corpsById(s, ids)) orderStance(s, c, posture);
  return view();
}

/** Where the picked corps could go by rail: anywhere any one of them could. */
function railReach(ids: number[]) {
  const s = game();
  const out = new Set<number>();
  if (humanUp(s)) for (const c of corpsById(s, ids)) if (canOrder(s, c)) for (const id of railReachOf(s, c)) out.add(id);
  return { regions: [...out] };
}

function rail(ids: number[], to: number) {
  const s = game();
  const moved: { id: number; path: number[] }[] = [];
  for (const c of corpsById(s, ids)) if (c.reg !== to && orderRail(s, c, to)) moved.push({ id: c.id, path: [to] });
  return { moved, view: view() };
}

/** Where the chosen corps could go by sea this month: friendly ports to be shipped to, and enemy coasts to land on. */
function seaReach(ids: number[]) {
  const s = game();
  const ports = new Set<number>();
  let beaches: number[] = [];
  if (humanUp(s) && !onEve(s)) {
    const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
    for (const c of cs) for (const r of s.regions) if (r.port && r.id !== c.reg && whyNotShip(s, c, r) === null) ports.add(r.id);
    beaches = landingTargets(s, cs).map((r) => r.id);
  }
  return { ports: [...ports], beaches };
}

/** Ships the chosen corps to a friendly port. */
function ship(ids: number[], to: number) {
  const s = game();
  const moved: { id: number; path: number[] }[] = [];
  for (const c of corpsById(s, ids)) if (c.reg !== to && orderShip(s, c, to)) moved.push({ id: c.id, path: [to] });
  return { moved, view: view() };
}

/** What a landing on an enemy coast would meet, before the order is given. */
function landingPlan(ids: number[], to: number) {
  const s = game();
  const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
  const p = previewLanding(s, cs, to);
  const lines: string[] = [];
  if (!p.why) {
    lines.push(p.air ? 'The force goes in by air.' : p.surprise ? 'War was declared this month: the defenders are caught unready, and no fleet can stop the crossing.'
      : p.risk > 0.005 ? `The crossing is dangerous: about ${Math.round(p.risk * 100)} in 100 of the men could be lost to enemy ships and aircraft on the way.` : 'The crossing is safe.');
    if (p.sortie > 0 && !p.surprise && !p.air) lines.push('Enemy warships within reach will put to sea against it.');
    lines.push(p.sky ? 'You hold the sky over the beach.' : 'You do not hold the sky over the beach.');
    lines.push(p.defended ? `The coast is defended: the troops must fight their way ashore${p.assess ? `, at odds of ${p.assess.odds.toFixed(1)} to 1` : ''}.` : 'The coast is undefended: they land unopposed.');
  }
  return { ok: !p.why, why: p.why ?? '', name: s.regions[to].name, lines };
}

/** Lands the chosen corps on an enemy coast. */
function landOn(ids: number[], to: number) {
  const s = game();
  const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
  const before = new Map(cs.map((c) => [c.id, c.reg] as const));
  const r = orderLand(s, cs, to);
  const moved = cs.filter((c) => !c.dead && c.reg !== before.get(c.id)).map((c) => ({ id: c.id, path: [c.reg] }));
  const bits: string[] = [];
  if (r.ok) {
    bits.push(r.taken ? `The landing at ${s.regions[to].name} has succeeded: the troops are ashore.` : `The landing at ${s.regions[to].name} was thrown back into the sea.`);
    if (r.lostAtSea && r.lostAtSea >= 100) bits.push(`${r.lostAtSea.toLocaleString('en-US')} men were lost on the crossing.`);
    if (r.report) bits.push(battleNote(s, r.report));
  }
  return { ok: r.ok, taken: !!r.taken, note: r.ok ? bits.join(' ') : r.why ?? 'The landing could not be made.', moved, view: view() };
}

/** What a territory holds, and what the player can raise or build there this month. */
function province(id: number) {
  const s = game();
  const r = s.regions[id];
  const me = s.human;
  if (!r) return null;
  const up = !!me && humanUp(s) && !onEve(s);
  const n = me ? s.nations[me] : null;
  const home = !!me && isHome(s, me, r);
  const raiseHere = !!me && canRaise(s, me, r);
  const cost = me ? fortCost(s, me, r) : null;
  const level = r.fort?.lv ?? 0;
  return {
    id: r.id,
    name: r.name,
    home,
    industry: r.fac,
    works: level,
    worksFacing: r.fort?.vs.map((v) => s.nations[v]?.name ?? v) ?? [],
    harbour: r.port,
    capital: r.cap,
    // What the province gives whoever holds it each month, as money: by its land, harbour, oil, factories and seat of government.
    yield: (() => {
      const y = provinceYield(s, r);
      return { land: shown(y.land), port: shown(y.port), oil: shown(y.oil), fac: shown(y.fac), capital: shown(y.capital), total: shown(y.total) };
    })(),
    budget: budgeted(s),
    // New corps: what each kind is, what it costs, how long it takes, and whether it can be ordered now.
    raise: raiseHere ? (Object.keys(PRODUCTION.builds) as Kind[]).map((kind) => {
      const spec = PRODUCTION.builds[kind];
      return {
        kind, label: spec.label, divs: spec.divs, cost: shown(spec.cost), months: spec.months, blurb: spec.blurb,
        // What the new corps will cost to keep each month once it has arrived (nothing in the scenarios).
        keep: budgeted(s) ? shown(corpsUpkeep(kind, spec.divs, 1)) : 0,
        ok: up && !!n && n.pp >= spec.cost,
        why: !up ? 'It is not your month.' : n && n.pp < spec.cost ? `Needs ${shown(spec.cost).toLocaleString('en-US')}; the treasury has ${inHand(n.pp).toLocaleString('en-US')}.` : '',
      };
    }) : [],
    raiseNote: raiseHere ? '' : home ? 'New corps are raised in the home country and sent out by sea.' : '',
    // Fortifications: the next level, if there is one.
    fort: home && cost !== null ? {
      level: level + 1, cost: shown(cost), months: PRODUCTION.fort.months,
      ok: up && !!n && n.pp >= cost,
      why: !up ? 'It is not your month.' : n && n.pp < cost ? `Needs ${shown(cost).toLocaleString('en-US')}; the treasury has ${inHand(n.pp).toLocaleString('en-US')}.` : '',
    } : null,
    forming: me ? s.nations[me].queue.filter((b) => b.region === r.id).map((b) => orderLine(s, b)) : [],
    // What kind of country it is for an army, the rivers on its borders and their bridges, and its railway
    // (all only in the grand campaign, where those rules apply).
    // What can be done to the country itself this month: bridges blown, the railway torn up, engineers set to work.
    jobs: (() => {
      const out: { key: string; label: string; detail: string; ok: boolean; why: string }[] = [];
      if (!me || !groundRules(s)) return out;
      for (const n of riverNotes(s, r.id)) {
        if (n.down) continue;
        const why = whyNotBlowBridges(s, r.id, n.far);
        out.push({ key: `blow:${n.far}`, label: `Blow the bridges over the ${n.river}`, detail: `Toward ${s.regions[n.far].name}. Wheels and tracks cannot cross until engineers rebuild them.`, ok: why === null, why: why ?? '' });
      }
      const rail = railNote(s, r.id);
      if (rail && rail.line && !rail.cut) {
        const why = whyNotCutRailway(s, r.id);
        out.push({ key: 'cutrail', label: 'Tear up the railway', detail: 'No trains run through here, for either side, until engineers relay it.', ok: why === null, why: why ?? '' });
      }
      for (const c of s.corps) {
        if (c.dead || c.nat !== me || c.reg !== r.id || c.kind !== 'eng') continue;
        if (c.job) {
          out.push({ key: `stop:${c.id}`, label: `${c.name}: stop work`, detail: jobLine(s, c), ok: up, why: up ? '' : 'It is not your month.' });
          continue;
        }
        for (const job of engineerJobs(s, c)) {
          out.push({ key: `job:${c.id}:${job.key}`, label: `${c.name}: ${job.label}`, detail: job.blurb, ok: up && !job.why, why: job.why ?? (up ? '' : 'It is not your month.') });
        }
      }
      return out;
    })(),
    country: (() => {
      const lines: string[] = [];
      const says = groundSays(s, r);
      if (says) lines.push(says);
      for (const n of riverNotes(s, r.id)) {
        lines.push(`The ${n.river} lies toward ${s.regions[n.far].name}: ${n.down ? 'the bridges are down' : 'the bridges stand'}.`);
      }
      const rail = railNote(s, r.id);
      if (rail) {
        lines.push(rail.line ? (rail.cut ? 'The railway here is cut.' : 'A railway runs through here.')
          : rail.road > 0 ? `No railway: ${rail.road} province${rail.road === 1 ? '' : 's'} of road from the railhead.` : 'No railway.');
        if (rail.ruined > 0) lines.push(`${rail.ruined} of its ${rail.factories} factories lie in ruins from bombing.`);
      }
      return lines;
    })(),
  };
}

/** Carries out one of the works a province offers (see province().jobs). */
function work(key: string, region: number) {
  const s = game();
  const [what, a, b] = key.split(':');
  let ok = false;
  let note = 'That cannot be done now.';
  if (what === 'blow') {
    ok = orderBlowBridges(s, region, Number(a));
    if (ok) note = 'The bridges are blown.';
  } else if (what === 'cutrail') {
    ok = orderCutRailway(s, region);
    if (ok) note = 'The railway is torn up.';
  } else if (what === 'job') {
    const c = s.corps.find((x) => x.id === Number(a));
    ok = !!c && orderEngineers(s, c, b as JobKind);
    if (ok && c) note = jobLine(s, c) + '.';
  } else if (what === 'stop') {
    const c = s.corps.find((x) => x.id === Number(a));
    ok = !!c && orderStopWork(s, c);
    if (ok) note = 'The engineers are taken off their work.';
  }
  return { ok, note, view: view() };
}

interface ForceOption { key: string; label: string; detail: string; ok: boolean; why: string }

/**
 * A fleet or an air fleet, for the panel that opens when it is clicked: what it is, where it is and what it
 * is doing, and (when it is the player's and it is his month) the orders it can be given.
 */
function force(kind: string, id: number) {
  const s = game();
  const up = humanUp(s) && !onEve(s);
  const notNow = up ? '' : 'It is not your month.';
  const options: ForceOption[] = [];
  if (kind === 'fleet') {
    const f = s.fleets.find((x) => x.id === id);
    if (!f) return null;
    const parts: string[] = [];
    const count = (n: number, one: string, many: string) => { if (n > 0) parts.push(`${n} ${n === 1 ? one : many}`); };
    count(f.ships.bb, 'battleship', 'battleships');
    count(f.ships.cv, 'carrier', 'carriers');
    count(f.ships.ca, 'cruiser', 'cruisers');
    count(f.ships.dd, 'destroyer', 'destroyers');
    count(f.ships.ss, 'submarine', 'submarines');
    const sea = s.seas[f.zone]?.name ?? 'the sea';
    const doing = f.stance === 'port' ? `In harbour at ${f.port}.` : f.stance === 'raid' ? `Raiding shipping in the ${sea}.` : `On patrol in the ${sea}.`;
    const mine = f.nat === s.human;
    if (mine) {
      const reach = fleetReach(s, f).slice().sort((a, b) => (a === f.zone ? -1 : b === f.zone ? 1 : 0));
      for (const z of reach) {
        const here = f.zone === z && f.stance === 'patrol';
        options.push({ key: `fleet:${id}:${z}:patrol`, label: `Patrol the ${s.seas[z].name}`, detail: 'Seeks out enemy warships there and guards the convoys.', ok: up && !here, why: here ? 'It is doing so now.' : notNow });
      }
      for (const z of reach) {
        const here = f.zone === z && f.stance === 'raid';
        options.push({ key: `fleet:${id}:${z}:raid`, label: `Raid shipping in the ${s.seas[z].name}`, detail: 'Goes for merchant ships and avoids a fight with the enemy fleet.', ok: up && !here, why: here ? 'It is doing so now.' : notNow });
      }
      options.push({ key: `fleet:${id}:${f.home}:port`, label: `Return to harbour at ${f.port}`, detail: 'Safe from the enemy fleet, and takes no part at sea.', ok: up && f.stance !== 'port', why: f.stance === 'port' ? 'It is in harbour now.' : notNow });
    }
    return { name: f.name, nat: f.nat, mine, lines: [parts.join(', ') || 'No ships left', doing], options };
  }
  const f = s.airFleets.find((x) => x.id === id);
  if (!f) return null;
  const base = s.regions[f.base];
  const targetName = (m: AirMission, t: number) => (m === 'strike' ? `the ${s.seas[t]?.name ?? 'sea'}` : s.regions[t]?.name ?? '');
  const doing = `${MISSION[f.mission].label}${f.target >= 0 ? `: ${targetName(f.mission, f.target)}` : ''}.`;
  const mine = f.nat === s.human;
  if (mine) {
    for (const m of ['support', 'rest'] as AirMission[]) {
      options.push({ key: `air:${id}:${m}:-1`, label: MISSION[m].label, detail: MISSION[m].what, ok: up && f.mission !== m, why: f.mission === m ? 'It is doing so now.' : notNow });
    }
    const far = (m: AirMission, t: number) => {
      const p = m === 'strike' ? s.seas[t] : s.regions[t];
      return Math.hypot(p.x - base.x, p.y - base.y);
    };
    for (const m of ['superiority', 'bomb', 'night', 'strike'] as AirMission[]) {
      const targets = airTargets(s, f, m).sort((a, b) => far(m, a) - far(m, b)).slice(0, m === 'strike' ? 3 : 5);
      for (const t of targets) {
        const now = f.mission === m && f.target === t;
        options.push({ key: `air:${id}:${m}:${t}`, label: `${MISSION[m].label}: ${targetName(m, t)}`, detail: MISSION[m].what, ok: up && !now, why: now ? 'It is doing so now.' : notNow });
      }
    }
    const bases = airBases(s, f).sort((a, b) => Math.hypot(a.x - base.x, a.y - base.y) - Math.hypot(b.x - base.x, b.y - base.y)).slice(0, 5);
    for (const r of bases) {
      options.push({ key: `airbase:${id}:${r.id}`, label: `Move to airfields in ${r.name}`, detail: 'Settling in costs it some readiness for a month.', ok: up, why: notNow });
    }
  }
  return {
    name: f.name, nat: f.nat, mine,
    lines: [`${Math.round(f.ftr)} fighters, ${Math.round(f.bmr)} bombers`, `Based in ${base?.name ?? 'the field'}.`, doing], options,
  };
}

/** Gives a fleet or an air fleet one of the orders its panel offered. */
function forceOrder(key: string) {
  const s = game();
  const [what, id, a, b] = key.split(':');
  let ok = false;
  if (what === 'fleet') ok = orderFleet(s, Number(id), Number(a), b as Stance);
  else if (what === 'air') ok = orderAir(s, Number(id), a as AirMission, Number(b));
  else if (what === 'airbase') ok = orderAirBase(s, Number(id), Number(a));
  return { ok, note: ok ? 'The order is given.' : 'That order cannot be given now.', view: view() };
}

/** The rivers, their crossings, the railways and the steep country: geography that does not change. */
function land() {
  const s = game();
  const steepNames = new Set<string>(GROUND.steep.regions);
  return {
    rivers: RIVERS.map((r) => ({ name: r.name, pts: r.pts })),
    crossings: CROSSINGS.map((c) => [c.a, c.b, c.river, c.x, c.y]),
    rails: RAIL_LINKS.map((l) => [l.a, l.b, l.x, l.y]),
    steep: s.regions.filter((r) => steepNames.has(r.name)).map((r) => r.id),
  };
}

function raise(kind: Kind, region: number) {
  const s = game();
  const b = orderBuildCorps(s, kind, region);
  return { ok: !!b, name: b?.name ?? '', due: b ? turnToDate(b.eta).label : '', view: view() };
}

function fortify(region: number) {
  const s = game();
  const b = orderBuildFort(s, region);
  return { ok: !!b, name: b?.name ?? '', due: b ? turnToDate(b.eta).label : '', view: view() };
}

function cancel(id: number) {
  const s = game();
  return { ok: orderCancelBuild(s, id), view: view() };
}

/**
 * The arsenal: everything the player's nation can order, with one province in mind (-1 for none). The list
 * is the rules' own (arsenalOffers), so both versions of the game offer the same things at the same prices.
 */
function arsenal(region = -1) {
  const s = game();
  const me = s.human;
  if (!me) return null;
  const up = humanUp(s) && !onEve(s);
  return {
    budget: budgeted(s),
    money: inHand(s.nations[me].pp),
    region: s.regions[region]?.name ?? '',
    offers: arsenalOffers(s, region).map((o) => ({
      key: o.key, group: o.group, label: o.label, blurb: o.blurb, size: o.size, months: o.months,
      cost: shown(o.cost), upkeep: shown(o.upkeep), at: o.at, where: s.regions[o.at]?.name ?? '', note: o.note,
      why: up ? o.why : 'It is not your month.', poor: o.poor,
    })),
    orders: s.nations[me].queue.map((b) => ({ ...orderLine(s, b), cost: shown(b.cost) })),
  };
}

/** Places an order from the arsenal. */
function order(key: string, region = -1) {
  const s = game();
  const built = orderArsenal(s, key, region);
  return { ok: !!built, name: built?.name ?? '', view: view() };
}

/** The player declares war on a country he is at peace with. */
function declare(on: string) {
  const s = game();
  return { ok: humanUp(s) && orderDeclare(s, on), view: view() };
}

/** A nation's name as it reads inside a sentence: "the United Kingdom", "Germany". */
const theName = (s: Game, id: string): string => { const nm = s.nations[id]?.name ?? id; return /^(United|Soviet|Netherlands)/.test(nm) ? `the ${nm}` : nm; };
const upFirst = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
const listOf = (names: string[]): string => (names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`);

/**
 * What declaring war on a country would mean, in the words the question is asked in: where it stands, who its
 * friends are, and what follows from saying yes. Nothing is changed. With a region, the army in hand is on its way there.
 */
function warPlan(on: string, target = -1) {
  const s = game();
  const f = previewDeclare(s, on);
  const n = s.nations[on];
  if (!f || !n || !s.human) return { ok: false, name: '', lead: '', friends: '', lines: [] as string[] };
  const mine = bloc(s.nations[s.human]);
  const blocOf = (b: 'allies' | 'axis'): string => (b === 'axis' ? 'the Axis' : 'the Allies');
  const The = upFirst(theName(s, on));
  const trade = shown(DIPLOMACY.tradeIncome).toLocaleString('en-US');
  const divs = fieldArmy(s, on).toLocaleString('en-US');
  const lines: string[] = [];
  lines.push(f.joins ? `${The} joins ${blocOf(f.joins)} the moment it is attacked, and its ${divs} divisions fight for them.` : `${The} and its ${divs} divisions are at war with you from today.`);
  if (f.league.length) lines.push(`The League stands by it. ${The} belongs to ${LEAGUE.name}, and war on one member is war on all: ${listOf(f.league.map((x) => theName(s, x)))} ${f.league.length === 1 ? 'goes' : 'go'} to war with you the same day, with ${f.league.reduce((sum, id) => sum + fieldArmy(s, id), 0).toLocaleString('en-US')} divisions between them.`);
  if (f.trade) lines.push(`Its trade ends. ${The} trades with you today, worth ${trade} a month to your treasury.`);
  if (f.sneak > 1) lines.push(`Surprise. It is at peace and not expecting you: this month your attacks on its ground are ${Math.round((f.sneak - 1) * 100)}% stronger. Next month the bonus is gone.`);
  else if (f.ready === 'war') lines.push('No surprise: it is already at war with your side and its army is ready.');
  else if (f.ready === 'alert') lines.push(`No surprise: ${traitsOf(on).find((t) => t.alert)?.name.toLowerCase() ?? 'its intelligence service'} has read the signs, and its army is ready.`);
  else lines.push('No surprise: its army is watching the frontier and is ready for you.');
  const moved = f.alarm.filter((a) => a.from !== a.to);
  const friends = f.alarm.filter((a) => a.friend);
  const other = mine === 'axis' ? 'the Allies' : 'the Axis', own = mine === 'axis' ? 'the Axis' : 'the Allies';
  if (f.alarm.length) {
    if (friends.length) lines.push(`Its friends turn against you. Public opinion in ${listOf(friends.map((a) => theName(s, a.nat)))} swings hard toward ${other}, and part of that never comes back.`);
    const rest = f.alarm.length - friends.length;
    if (rest > 0) lines.push(`The neutral world grows wary. ${rest} other ${rest === 1 ? 'neutral moves' : 'neutrals move'} a little away from ${own}. Every attack on a neutral adds to it.`);
    for (const a of moved) {
      const was = TRACK.indexOf(a.from) - 3, now = TRACK.indexOf(a.to) - 3;
      const how = now === 0 ? `no longer leans toward ${blocOf(was > 0 ? 'axis' : 'allies')}` : Math.abs(now) === 1 ? `begins to lean toward ${blocOf(now > 0 ? 'axis' : 'allies')}` : `throws in its lot with ${blocOf(now > 0 ? 'axis' : 'allies')}`;
      lines.push(`${upFirst(theName(s, a.nat))} ${how}${a.trade ? `, and stops trading with you (${trade} a month lost)` : ''}.`);
    }
    if (!moved.length) lines.push('Nobody changes sides over this one attack, but the next may tip them.');
  } else if (f.joins === null && bloc(n) === 'league') lines.push('The neutrals on the track between the blocs take no side in a quarrel with the League.');
  else if (f.joins === null && bloc(n) !== null) lines.push(`${The} already belongs to ${blocOf(bloc(n) as 'allies' | 'axis')}: the neutral world expects this war and takes no fright at it.`);
  else if (!mine) lines.push('Your country belongs to neither bloc, so the neutrals have no side to turn away from. They will remember it all the same.');
  const at = TRACK.indexOf(n.align) - 3;
  const stands = n.league ? `is a member of ${LEAGUE.name} and at peace with you` : at === 0 ? 'is neutral and at peace with you' : Math.abs(at) === 1 ? `is neutral, leans toward ${blocOf(at > 0 ? 'axis' : 'allies')}, and is at peace with you` : `is ${TRACK_PHRASE[n.align]}, but at peace with you`;
  const r = target >= 0 ? s.regions[target] : null;
  const lead = r ? `Your army is about to cross the frontier into ${r.name}. ${The} ${stands}. Marching in means war.` : `${The} ${stands}.`;
  // Friends that share a tie are named together, with the tie said once.
  const ties = new Map<string, string[]>();
  for (const x of f.friends) ties.set(x.why, [...(ties.get(x.why) ?? []), theName(s, x.nat)]);
  const friendsLine = f.friends.length
    ? `Its friends: ${[...ties].map(([why, who]) => `${listOf(who)} (${why})`).join('; ')}.`
    : `${The} has no close friends among the nations.`;
  return { ok: true, name: theName(s, on), lead, friends: friendsLine, lines };
}

// ---- Rooms drawn by one general panel: the bridge writes the words and the buttons, the game draws them ----------

interface PageButton { label: string; act: string; ok: boolean; tip: string; danger?: boolean }
interface PageRow { nat: string; title: string; lines: string[]; buttons: PageButton[] }
interface Page { id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: PageRow[] }[] }

/** A question about World Domination waiting for its second and third yes. */
let domAsked = 0;

function pagesOf(s: Game): { id: string; label: string }[] {
  const out: { id: string; label: string }[] = [];
  if (!s.human) return out;
  if (secretService(s)) out.push({ id: 'spies', label: 'THE SECRET SERVICE' });
  const act = lastAct(s);
  if (act && (act.decided || act.on)) out.push({ id: 'lastact', label: act.on ? 'THE WORLD STANDINGS' : 'THE LAST ACT' });
  return out;
}

function alertsOf(s: Game): string[] {
  const out: string[] = [];
  if (!s.human) return out;
  const act = lastAct(s);
  if (act?.winner) out.push(`${s.nations[act.winner]?.name ?? act.winner} has won the world.`);
  else if (act?.open && !act.peace) out.push('The war between the alliances is decided. Open DIPLOMACY and choose THE LAST ACT.');
  return out;
}

const pct = (x: number): string => `${Math.round(x * 100)}%`;

function spiesPage(s: Game, region: number): Page | null {
  const room = secretService(s);
  if (!room) return null;
  const up = humanUp(s) && !onEve(s);
  const here = s.regions[region];
  const rows: PageRow[] = room.agents.map((a) => {
    const whySend = here ? whyNotSendAgent(s, a.id, region) : 'Close this, click a province on the map, and open the room again.';
    const buttons: PageButton[] = a.missions.map((m) => ({
      label: m.now ? `${m.label.toUpperCase()} (NOW)` : m.label.toUpperCase(), act: `spy:mission:${a.id}:${m.id}`, ok: up && m.ok && !m.now,
      tip: `${m.text}${m.success !== null ? ` Chance of success ${pct(m.success)}.` : ''} Chance of being caught ${pct(m.caught)}.${m.ok ? '' : ` ${m.why}`}`,
    }));
    buttons.push({ label: here ? `SEND TO ${here.name.toUpperCase()}` : 'SEND TO A PROVINCE', act: `spy:send:${a.id}`, ok: up && !!here && whySend === null && here.id !== a.reg, tip: whySend ?? `The agent travels to ${here?.name ?? 'the province selected on the map'} and sets to work there.` });
    buttons.push({ label: 'RETIRE', act: `spy:retire:${a.id}`, ok: up, tip: 'The agent leaves the service, which frees a place in it.', danger: true });
    const lines = [`${a.where}${a.abroad ? `, in ${theName(s, a.host)}` : ', at home'}. ${a.doing}`];
    if (a.wait > 0) lines.push(`At work in ${a.wait} month${a.wait === 1 ? '' : 's'}.`);
    if (a.atWork) lines.push(`Chance of being caught this month: ${pct(a.caught)}.${a.rest > 0 ? ` Lying low for ${a.rest} more month${a.rest === 1 ? '' : 's'} before another act of sabotage.` : ''}`);
    for (const x of a.sees.slice(0, 5)) lines.push(`In sight at ${s.regions[x.reg].name}: ${s.nations[x.nat]?.adj ?? x.nat} army, ${x.corps} corps, ${x.divs} division${x.divs === 1 ? '' : 's'}.`);
    return { nat: a.nat, title: `${a.rank} ${a.name}`, lines, buttons };
  });
  const notes = room.notes.slice(-10).reverse().map((n) => `${turnToDate(n.turn).label}: ${n.text}`);
  return {
    id: 'spies', title: 'THE SECRET SERVICE',
    headline: `${room.agents.length} of ${room.most} agents in the service`,
    note: `Agents abroad show you what the fog of war hides, read a foreign government's books, and can strike at factories, railways and oil. Agents at home guard against the other side's. ${room.threat.foes.length ? `This month the chance of an enemy strike at home is ${pct(room.threat.chance)}, with ${room.threat.guards} of your agents on guard.` : 'No enemy service is working against you at home.'}`,
    top: [
      { label: `RECRUIT AN AGENT  \u00B7  ${Math.round(room.cost).toLocaleString('en-US')}`, act: 'spy:recruit', ok: up && room.whyNotRecruit === null, tip: room.whyNotRecruit ?? 'A new agent joins the service, at home, and can be sent abroad.' },
      { label: room.fog ? 'LIFT THE FOG OF WAR' : 'LAY THE FOG OF WAR AGAIN', act: room.fog ? 'fog:off' : 'fog:on', ok: true, tip: room.fog ? 'Show every army on the map, as before spies were in the game. Agents and sabotage go on either way.' : 'Hide again what your armies and agents cannot see.' },
    ],
    sections: [
      { heading: 'YOUR AGENTS', text: rows.length ? (here ? `The province selected on the map is ${here.name}: an agent can be sent there.` : 'To send an agent somewhere, close this, click the province on the map, and open the room again.') : 'The service has no agents yet. Recruit one above.', rows },
      { heading: "THE SERVICE'S FILE", text: notes.length ? notes.join('\n') : 'Nothing has been reported yet.', rows: [] },
    ],
  };
}

function lastActPage(s: Game): Page | null {
  const act = lastAct(s);
  if (!act || !(act.decided || act.on)) return null;
  const board = standings(s);
  const up = humanUp(s);
  const row = (r: (typeof board.rows)[number]): PageRow => ({
    nat: r.nat, title: r.you ? `${r.name} (you)` : r.name,
    lines: [r.alive
      ? `${r.provinces} province${r.provinces === 1 ? '' : 's'}, ${pct(r.share)} of the world. ${r.few ? `Fewer than ${r.divisions} divisions` : `${r.exact ? '' : 'About '}${r.divisions} division${r.divisions === 1 ? '' : 's'}`}${r.income !== null ? `. Income ${Math.round(r.income * board.scale).toLocaleString('en-US')} a month` : ''}.`
      : `Fell in ${r.fell ? turnToDate(r.fell.turn).label : 'the war'}${r.fell ? ` to ${theName(s, r.fell.to)}` : ''}.`],
    buttons: [],
  });
  const top: PageButton[] = [];
  let note = '';
  if (act.winner) note = `${s.nations[act.winner]?.name ?? act.winner} is the last nation standing. The world is won.`;
  else if (act.on) note = `The last act began in ${turnToDate(act.since).label}. Every alliance is dissolved and every nation still standing is at war with every other. ${act.left} are left. The last one standing wins the world.`;
  else {
    const why = whyNotOrderDomination(s);
    note = `The war between the alliances is decided. You can take the victory and leave the world as it is, or choose World Domination: every alliance is dissolved, your allies of yesterday become your enemies, and the last nation standing wins. It cannot be undone.${act.peace ? ' You have taken the victory; the last act stays open to you.' : ''}`;
    if (domAsked === 0) top.push({ label: 'WORLD DOMINATION', act: 'dom:ask', ok: up && why === null, tip: why ?? 'Turn on every nation still standing. You will be asked twice whether you mean it.', danger: true });
    else top.push({ label: domAsked === 1 ? 'YES, I MEAN IT' : 'YES. BEGIN THE LAST ACT. IT CANNOT BE UNDONE', act: domAsked === 1 ? 'dom:ask2' : 'dom:yes', ok: up && why === null, tip: why ?? '', danger: true }, { label: 'NO', act: 'dom:no', ok: true, tip: 'Leave things as they are.' });
    if (!act.peace) top.push({ label: 'TAKE THE VICTORY AND KEEP THE PEACE', act: 'dom:peace', ok: up, tip: 'The war is won. The world stays as it is, and the last act can still be chosen in a later month.' });
  }
  return {
    id: 'lastact', title: act.on ? 'THE WORLD STANDINGS' : 'THE LAST ACT',
    headline: act.winner ? 'The world is won' : act.on ? `${act.left} nations still stand` : 'The war is decided',
    note, top,
    sections: [
      { heading: 'STILL STANDING', text: '', rows: board.rows.map(row) },
      { heading: 'FALLEN', text: board.fallen.length ? '' : 'Nobody has fallen yet.', rows: board.fallen.map(row) },
    ],
  };
}

// ---- The air war: airfields, the groups on them, and the planning of a raid --------------------------------

const craftOf = (ftr: number, bmr: number): string => [ftr >= 1 ? `${Math.round(ftr).toLocaleString('en-US')} fighters` : '', bmr >= 1 ? `${Math.round(bmr).toLocaleString('en-US')} bombers` : ''].filter(Boolean).join(', ') || 'no aircraft fit to fly';
const airPlace = (s: Game, m: AirMission, t: number): string => (m === 'strike' ? `the ${s.seas[t]?.name ?? 'sea'}` : s.regions[t]?.name ?? '');
const airDoing = (s: Game, f: Game['airFleets'][number]): string => `${MISSION[f.mission].label}${f.target >= 0 ? `: ${airPlace(s, f.mission, f.target)}` : ''}`;
const RAID_TITLE: Record<string, string> = { bomb: 'A BOMBING RAID BY DAY', night: 'RAIDS BY NIGHT', superiority: 'A FIGHT FOR THE SKY', strike: 'A STRIKE ON SHIPPING' };

/** An airfield: the air groups based in a province, what each is doing, and the work each can be given. */
function airfieldPage(s: Game, region: number): Page | null {
  const r = s.regions[region];
  if (!r || !s.human) return null;
  const up = humanUp(s) && !onEve(s);
  const here = s.airFleets.filter((f) => f.base === region && f.ftr + f.bmr > 0);
  const level = workLevel(s, 'field', region);
  const rows: PageRow[] = here.map((f) => {
    const mine = f.nat === s.human;
    const rg = airRange(f);
    const lines = [`${craftOf(f.ftr, f.bmr)}. Fighters reach ${rg.ftr} km, bombers ${rg.bmr} km.`, `Orders: ${airDoing(s, f)}.`];
    const buttons: PageButton[] = [];
    if (mine) {
      if (f.bmr >= 1) {
        buttons.push({ label: 'PLAN A BOMBING RAID', act: `open:raid:bomb:${f.id}`, ok: up, tip: MISSION.bomb.what });
        buttons.push({ label: 'PLAN RAIDS BY NIGHT', act: `open:raid:night:${f.id}`, ok: up, tip: MISSION.night.what });
        buttons.push({ label: 'PLAN A STRIKE ON SHIPPING', act: `open:raid:strike:${f.id}`, ok: up, tip: MISSION.strike.what });
      }
      buttons.push({ label: 'FIGHT FOR THE SKY', act: `open:raid:superiority:${f.id}`, ok: up, tip: MISSION.superiority.what });
      buttons.push({ label: 'SUPPORT THE ARMIES', act: `air:${f.id}:support:-1`, ok: up && f.mission !== 'support', tip: `${MISSION.support.what} Its fighters patrol their own sky and meet any raid that comes.` });
      buttons.push({ label: 'REST AND REFIT', act: `air:${f.id}:rest:-1`, ok: up && f.mission !== 'rest', tip: MISSION.rest.what });
      buttons.push({ label: 'MOVE TO ANOTHER AIRFIELD', act: `open:rebase:${f.id}`, ok: up, tip: 'Settling in costs it some readiness for a month.' });
    }
    return { nat: f.nat, title: f.name, lines, buttons };
  });
  return {
    id: 'airfield', title: `THE AIRFIELDS AT ${r.name.toUpperCase()}`,
    headline: here.length ? `${here.length} air group${here.length === 1 ? '' : 's'} based here` : 'No air group is based here',
    note: `${level > 0 ? `Airfields of level ${level}.` : 'Rough landing grounds only.'} Aircraft take off from here and come back here. Fighters left to support the armies also guard this sky against raids. Bombers are sent on raids you plan: you pick the target, see the odds and the likely losses, and decide which groups fly it together. Better airfields are built from the ARSENAL with this province selected.`,
    top: [], sections: [{ heading: 'THE AIR GROUPS', text: here.length ? '' : 'Order aircraft in the ARSENAL, or move a group here from another airfield.', rows }],
  };
}

/** Planning a raid for one air group: every target in reach with what the raid would meet and do, and who else could fly it. */
function raidPage(s: Game, mission: AirMission, id: number): Page | null {
  const f = s.airFleets.find((x) => x.id === id);
  if (!f || f.nat !== s.human) return null;
  const up = humanUp(s) && !onEve(s);
  const base = s.regions[f.base];
  const far = (t: number): number => { const p = mission === 'strike' ? s.seas[t] : s.regions[t]; return Math.hypot(p.x - base.x, p.y - base.y); };
  const targets = airTargets(s, f, mission).sort((a, b) => far(a) - far(b)).slice(0, 10);
  const others = s.airFleets.filter((x) => x.nat === f.nat && x.id !== f.id && x.ftr + x.bmr > 0);
  const rows: PageRow[] = [];
  for (const t of targets) {
    const a = previewAirOrder(s, f.id, mission, t);
    const why = whyNotMission(s, f, mission, t) ?? a?.why ?? (a ? null : 'Nothing can reach the target.');
    const place = airPlace(s, mission, t);
    const lines: string[] = [];
    const buttons: PageButton[] = [];
    if (why || !a) lines.push(why ?? '');
    else {
      const fc = forecastAir(s, a, mission, t);
      const loss = Math.round(fc.bomberLoss * 100);
      if (mission === 'superiority') {
        lines.push(`${a.D <= 0 ? 'No fighters will come up.' : fc.odds >= AIR.sky.win ? 'The sky can be won.' : fc.odds >= 1.2 ? 'A hard fight that wears them down.' : fc.odds >= 0.8 ? 'An even fight.' : 'The defenders have the edge.'} Fighter odds ${fc.odds.toFixed(1)} to 1.${a.B > 0 ? ` About ${loss}% of the bombers are likely to be lost.` : ''}`);
      } else if (mission === 'strike') {
        lines.push(`Likely bomber losses: about ${loss}%. Warships caught at sea there could lose up to ${Math.round(fc.cut * 100)}% of their number.`);
      } else {
        const fac = s.regions[t].fac > 0;
        lines.push(`Likely bomber losses: about ${loss}% (${loss <= 10 ? 'light' : loss <= 25 ? 'the fighters will take a toll' : 'heavy'}). ${fac ? `The raid should stop about ${Math.round(fc.cut * 100)}% of its output for a month.` : 'There is no industry here: the bombers go for the rail yards.'}${fc.rail ? ` About a ${Math.round(fc.rail * 100)}% chance of cutting its railway.` : ''}${fc.ruin ? ` About a ${Math.round(fc.ruin * 100)}% chance of wrecking a factory outright.` : ''}`);
      }
      lines.push(`Flying it: ${a.att.map((x) => `${x.f.name} from ${s.regions[x.f.base].name} (${craftOf(mission === 'night' ? 0 : x.ftr, x.bmr)})`).join('; ')}.`);
      lines.push(a.def.length ? `Rising to meet them: ${a.def.map((x) => `${x.f.name}, about ${Math.round(x.f.ftr * x.w)} fighters`).join('; ')}.${a.aa > 0 ? ` Anti-aircraft guns of level ${a.aa}.` : ''}` : `No enemy fighters are in reach.${a.aa > 0 ? ` Anti-aircraft guns of level ${a.aa}.` : ''}`);
      const now = f.mission === mission && f.target === t;
      buttons.push({ label: now ? 'ORDERED' : `SEND ${f.name.toUpperCase()}`, act: `air:${f.id}:${mission}:${t}`, ok: up && !now, tip: 'Flown when you end the month. A group on a raid does not defend its own sky that month.' });
      // Other groups that could fly the same raid with it: together they stand a better chance.
      for (const o of others) {
        if (o.mission === mission && o.target === t) continue;
        if (whyNotMission(s, o, mission, t) !== null) continue;
        buttons.push({ label: `ALSO SEND ${o.name.toUpperCase()}`, act: `air:${o.id}:${mission}:${t}`, ok: up, tip: `${o.name} from ${s.regions[o.base].name} (${craftOf(o.ftr, o.bmr)}) joins the raid. Groups given the same target fly it together, and the odds above are worked out again.` });
        if (buttons.length >= 5) break;
      }
    }
    rows.push({ nat: mission === 'strike' ? '' : s.regions[t].ctrl, title: mission === 'strike' ? upFirst(place) : `${place}, ${Math.round(airKm(s, f.base, t)).toLocaleString('en-US')} km`, lines, buttons });
  }
  return {
    id: `raid:${mission}:${id}`, title: `${RAID_TITLE[mission] ?? 'A RAID'}  \u00B7  ${f.name.toUpperCase()}`,
    headline: targets.length ? `${targets.length} target${targets.length === 1 ? '' : 's'} in reach, nearest first` : 'No target is in reach',
    note: `${MISSION[mission].what} ${f.name} flies from ${base.name} with ${craftOf(f.ftr, f.bmr)}. Its orders now: ${airDoing(s, f)}. Send more than one group against the same target and they fly it together, which spreads the defenders' fire.`,
    top: [{ label: 'BACK TO THE AIRFIELD', act: 'open:airfield', ok: true, tip: '' }],
    sections: [{ heading: 'TARGETS', text: targets.length ? '' : 'Move the group to an airfield nearer the enemy, or wait for aircraft with a longer reach.', rows }],
  };
}

/** Moving an air group: the airfields it could go to. */
function rebasePage(s: Game, id: number): Page | null {
  const f = s.airFleets.find((x) => x.id === id);
  if (!f || f.nat !== s.human) return null;
  const up = humanUp(s) && !onEve(s);
  const base = s.regions[f.base];
  const rows: PageRow[] = airBases(s, f).sort((a, b) => Math.hypot(a.x - base.x, a.y - base.y) - Math.hypot(b.x - base.x, b.y - base.y)).slice(0, 14).map((r) => ({
    nat: r.ctrl, title: r.name, lines: [`${workLevel(s, 'field', r.id) > 0 ? `Airfields of level ${workLevel(s, 'field', r.id)}` : 'Rough landing grounds'}.${s.airFleets.some((x) => x.base === r.id && x.id !== f.id && x.ftr + x.bmr > 0) ? ' Other groups are based there already.' : ''}`],
    buttons: [{ label: 'MOVE HERE', act: `airbase:${f.id}:${r.id}`, ok: up, tip: 'Settling in costs it some readiness for a month.' }],
  }));
  return {
    id: `rebase:${id}`, title: `A NEW AIRFIELD  \u00B7  ${f.name.toUpperCase()}`, headline: `${rows.length} airfield${rows.length === 1 ? '' : 's'} it could move to, nearest first`,
    note: `${f.name} is based at ${base.name}. A group reaches farther into enemy country from an airfield nearer the front.`,
    top: [{ label: 'BACK TO THE AIRFIELD', act: 'open:airfield', ok: true, tip: '' }], sections: [{ heading: 'AIRFIELDS', text: '', rows }],
  };
}

/** One of the rooms the general panel shows. region is the province selected on the map, or -1. */
function page(id: string, region = -1) {
  const s = game();
  const part = id.split(':');
  const p = id === 'spies' ? spiesPage(s, region) : id === 'lastact' ? lastActPage(s)
    : airPage(s, id, region) ? airPage(s, id, region)
    : id === 'airfield' ? airfieldPage(s, region)
    : part[0] === 'raid' ? raidPage(s, part[1] as AirMission, Number(part[2]))
    : part[0] === 'rebase' ? rebasePage(s, Number(part[1])) : null;
  return p ? { ok: true, ...p } : { ok: false };
}

/** Something pressed in one of those rooms. Says what came of it; the game then draws the room again. */
function act(action: string, region = -1) {
  const s = game();
  const h0 = s.headlines.length;
  const part = action.split(':');
  let ok = false, note = '';
  // Aircraft seen leaving for their target when a raid is ordered: where from, where to, whose, and what flies.
  let fly: Flight | null = null;
  let flies: Flight[] = [];
  let open = '';
  const air0 = lastAirReport(s);
  const air = airAct(s, action);
  if (air) {
    // The air war as its own game (the grand campaign): a raid is flown the moment it is ordered, by every group picked.
    ok = air.ok; note = air.note; open = air.ok ? air.open ?? '' : '';
    if (air.ok && air.report !== undefined) {
      const flown = raidsSince(s, air0, [s.turn]);
      flies = flown.flights;
      // What came of it, in the report's own words first, kept short enough to read while the raid is flown.
      const told = flown.notes.join(' ') || note;
      note = told.length > 190 ? `${told.slice(0, told.lastIndexOf('.', 190) + 1) || told.slice(0, 190)}` : told;
    }
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'air') {
    const f = s.airFleets.find((x) => x.id === Number(part[1]));
    const m = part[2] as AirMission, t = Number(part[3]);
    // What the raid will meet, as the planner worked it out: guns round the target, fighters rising, likely losses.
    const before = f && t >= 0 ? previewAirOrder(s, f.id, m, t) : null;
    const meets = before && !before.why ? { aa: before.aa, meet: before.def.find((x) => x.f.ftr * x.w >= 1)?.f.nat ?? '', loss: forecastAir(s, before, m, t).bomberLoss } : { aa: 0, meet: '', loss: 0 };
    ok = orderAir(s, Number(part[1]), m, t);
    note = ok ? (t >= 0 ? `${f?.name ?? 'The group'} will fly against ${airPlace(s, m, t)} when the month ends.` : `${f?.name ?? 'The group'}: ${MISSION[m].label.toLowerCase()}.`) : 'That order cannot be given now.';
    if (ok && f && t >= 0) { const p = m === 'strike' ? s.seas[t] : s.regions[t]; fly = { from: f.base, x: p.x, y: p.y, nat: f.nat, bombers: m !== 'superiority' && f.bmr >= 1, escort: m !== 'night' && f.ftr >= 1, region: m === 'strike' ? -1 : t, meetFrom: -1, ...meets }; }
  } else if (part[0] === 'airbase') {
    ok = orderAirBase(s, Number(part[1]), Number(part[2]));
    note = ok ? `The group moves to ${s.regions[Number(part[2])]?.name ?? 'its new airfield'}.` : 'It cannot move there now.';
  } else if (action === 'spy:recruit') { const a = orderRecruitAgent(s); ok = !!a; note = a ? `${a.name} joins the service.` : 'No agent could be recruited.'; }
  else if (part[0] === 'spy' && part[1] === 'send') { const why = whyNotSendAgent(s, Number(part[2]), region); ok = orderSendAgent(s, Number(part[2]), region); note = ok ? `The agent sets out for ${s.regions[region]?.name ?? 'the province'}.` : why ?? 'The agent cannot go there.'; }
  else if (part[0] === 'spy' && part[1] === 'mission') { ok = orderAgentMission(s, Number(part[2]), part[3] as Parameters<typeof orderAgentMission>[2]); note = ok ? 'The agent has new orders.' : 'The agent cannot take that work now.'; }
  else if (part[0] === 'spy' && part[1] === 'retire') { ok = orderRetireAgent(s, Number(part[2])); note = ok ? 'The agent has left the service.' : ''; }
  else if (action === 'fog:off' || action === 'fog:on') { ok = orderFog(s, action === 'fog:on'); note = ok ? (action === 'fog:on' ? 'The fog of war lies on the map again.' : 'The fog of war is lifted: every army is shown.') : ''; }
  else if (action === 'dom:ask') { domAsked = 1; ok = true; }
  else if (action === 'dom:ask2') { domAsked = 2; ok = true; }
  else if (action === 'dom:no') { domAsked = 0; ok = true; }
  else if (action === 'dom:yes') { domAsked = 0; ok = orderDomination(s); note = ok ? 'The last act has begun. Every nation still standing is at war with every other.' : whyNotOrderDomination(s) ?? 'It cannot be chosen now.'; }
  else if (action === 'dom:peace') { domAsked = 0; ok = orderKeepPeace(s); note = ok ? 'You take the victory, and the world stays as it is.' : ''; }
  const news = s.headlines.slice(h0).map((h) => h.text);
  return { ok, note: news.length ? news.join(' ') : note, fly, flies: fly ? [fly] : [], open: '', view: view() };
}

/** The player's oil and supplies, for the two gauges beside the treasury. */
function storesOf(s: Game) {
  const v = s.human ? depots(s) : null;
  if (!v) return null;
  const amount = (x: number): string => (Math.round(x * 10) / 10).toLocaleString('en-US');
  const book = (bk: typeof v.oil, word: string) => {
    const months = Number.isFinite(bk.months) ? Math.max(0, Math.floor(bk.months)) : -1;
    const lines = [
      `${word}: ${amount(bk.stock)} in the depots, which hold ${amount(bk.cap)}.`,
      `Comes in each month: ${amount(bk.made)}.`,
      ...bk.income.filter((l) => Math.abs(l.amount) >= 0.05).map((l) => `   ${l.label}: ${amount(l.amount)}${l.note ? ` (${l.note})` : ''}`),
      `Goes out each month: ${amount(bk.need)}.`,
      ...bk.spend.filter((l) => Math.abs(l.amount) >= 0.05).map((l) => `   ${l.label}: ${amount(l.amount)}`),
      bk.net >= 0 ? `The stock is ${bk.net > 0.05 ? 'growing' : 'steady'}.` : months <= 0 ? 'The depots are empty.' : `At this rate the depots are empty in ${months} month${months === 1 ? '' : 's'}.`,
      ...bk.effects,
    ];
    return { stock: bk.stock, cap: bk.cap, net: bk.net, months, met: bk.met, short: bk.met < 0.999, tip: lines.join('\n') };
  };
  return { oil: book(v.oil, 'Oil'), sup: book(v.sup, 'Supplies') };
}

/** A nation's abilities and its special formation, each as a line. */
function abilityLines(nat: string): string[] {
  const out = traitsOf(nat).map((t) => `${t.name}. ${traitRule(t, BUDGET.scale)}`);
  const u = eliteOf(nat);
  if (u) out.push(`Special formation, from the arsenal: ${u.label}. ${eliteRule(u)}`);
  return out;
}

/** The abilities of the player's nation, for the page that says how to play. They count in the grand campaign only. */
function abilities() {
  const s = game();
  const me = s.human ? s.nations[s.human] : null;
  return { nation: me?.name ?? '', active: traitRules(s), lines: me ? abilityLines(me.id) : [] };
}

/**
 * The foreign office: who is at war and how much fight each has left, and where every country at peace stands
 * between the two blocs, with what the player can do about each: court it, or declare war on it.
 */
function diplomacy() {
  const s = game();
  const me = s.human && s.nations[s.human]?.alive ? s.nations[s.human] : null;
  if (!me) return null;
  const mine = bloc(me);
  const up = humanUp(s) && !onEve(s);
  const can = new Set(up ? declarable(s) : []);
  const partners = new Set(tradePartners(s, me.id).map((n) => n.id));
  const m = momentum(s);
  const wind = Math.abs(m) < 0.15 ? 'Neither side has the upper hand, so neutrals are sitting tight.'
    : `${m > 0 ? 'The Axis is' : 'The Allies are'} winning, and neutrals ${Math.abs(m) > 0.6 ? 'are bending' : 'are starting to bend'} that way.`;
  const all = Object.values(s.nations).filter((n) => !n.dormant);
  const powers = all.filter((n) => n.atWar.length || n.fellOn)
    .sort((a, b) => (b.alive ? 1 : 0) - (a.alive ? 1 : 0) || b.armyPeak - a.armyPeak)
    .map((n) => ({
      nat: n.id, name: n.name, alive: n.alive, divs: n.alive ? fieldArmy(s, n.id) : 0,
      will: n.alive ? Math.max(0, Math.min(1, 1 - collapseScore(s, n.id) / n.resilience)) : 0,
      side: n.id === me.id ? 'own' : atWar(s, me.id, n.id) ? 'enemy' : allied(s, me.id, n.id) ? 'ally' : 'other',
      note: n.alive ? `At war with ${listOf(n.atWar.map((x) => theName(s, x)))}` : `Fell in ${turnToDate(n.fellOn!.turn).label}: asked ${theName(s, n.fellOn!.to)} for terms`,
      war: can.has(n.id),
    }));
  const rows = all.filter((n) => n.alive && !n.atWar.length && !n.league && n.id !== me.id).sort((a, b) => a.opinion - b.opinion).map((n) => {
    const open = (mine === 'axis' || mine === 'allies') && courtable(n);
    const c = open ? canCourt(s, me.id, n.id) : { ok: false, why: '' };
    const k = open ? receptive(n, mine as 'axis' | 'allies') : 0;
    const frs = friendsOf(n.id).filter((f) => s.nations[f.nat]?.alive);
    return {
      nat: n.id, name: n.name, label: TRACK_LABEL[n.align], step: step(n), opinion: Math.round(n.opinion),
      divs: fieldArmy(s, n.id),
      court: open && up, canCourt: up && c.ok, why: open && up && !c.ok ? c.why : '',
      word: !open ? '' : k >= 1.3 ? 'Receptive' : k >= 0.8 ? 'Open to offers' : k >= 0.5 ? 'Wary' : 'Very hard to move',
      friends: frs.length ? `Friends: ${listOf(frs.map((f) => theName(s, f.nat)))}` : '',
      trades: partners.has(n.id),
      war: can.has(n.id),
    };
  });
  const side = mine === 'axis' ? 'the Axis' : mine === 'allies' ? 'the Allies' : mine === 'league' ? LEAGUE.name : '';
  // The League, once it exists: its members, and who stands with whom.
  const born = s.flags['league:born'];
  const members = leagueMembers(s);
  const league = {
    founded: leagueFounded(s), full: upFirst(LEAGUE.full),
    since: born !== undefined ? turnToDate(born).label : '',
    members: members.map((n) => ({
      nat: n.id, name: n.name, you: n.id === me.id, divs: fieldArmy(s, n.id),
      note: `${n.atWar.length ? `At war with ${listOf(n.atWar.map((x) => theName(s, x)))}` : 'At peace'}, member since ${turnToDate(s.flags[`league:${n.id}`] ?? born ?? s.turn).label}`,
      war: can.has(n.id),
    })),
  };
  // A small country in the player's hands chooses its own path.
  const opts = paths(s);
  const others = members.filter((n) => n.id !== me.id);
  const foes = me.atWar.filter((x) => s.nations[x]?.alive);
  const at = step(me);
  const where = mine === 'league' ? `a member of ${LEAGUE.name}${others.length ? `, with ${listOf(others.map((n) => theName(s, n.id)))}` : ''}`
    : mine ? `a member of ${mine === 'axis' ? 'the Axis' : 'the Allies'}` : TRACK_PHRASE[me.align];
  const path = {
    open: opts.length > 0,
    lead: opts.length ? `${upFirst(theName(s, me.id))} is ${where}. ${foes.length ? `You are at war with ${listOf(foes.map((x) => theName(s, x)))}.` : 'You are at peace.'} A small country chooses its own path, and nobody chooses it for you: no treaty signs your land away, and no courting moves you.` : '',
    options: opts.map((o) => {
      const T = PATH_TEXT[o.id];
      let text = T.text;
      if (o.id === 'alone' && !o.now && o.ok) text = `Step back to strict neutrality: you stop leaning toward ${at > 0 ? 'the Axis' : 'the Allies'}, and answer to nobody.`;
      if (o.id === 'join' && others.length) text += ` Its members today: ${listOf(others.map((n) => theName(s, n.id)))}.`;
      return {
        id: o.id, ok: o.ok && up, now: o.now, why: o.ok && !up ? 'Not in your month.' : o.why,
        title: !o.now ? T.title : o.id === 'join' ? upFirst(LEAGUE.full) : o.id === 'axis' ? 'The Axis' : o.id === 'allies' ? 'The Allies' : T.title,
        text: o.ok || o.now ? text : '',
        final: o.id === 'axis' || o.id === 'allies' || o.id === 'join',
      };
    }),
    partners: opts.some((o) => o.id === 'found' && o.ok) ? leaguePartners(s).map((x) => ({ nat: x.nat, name: s.nations[x.nat].name, ok: x.ok && up, why: x.why })) : [],
    invites: me.league ? leagueInvites(s).map((x) => ({ nat: x.nat, name: s.nations[x.nat].name, ok: x.ok, why: x.ok ? (whyNotLeagueInvite(s, x.nat) ?? x.why) : x.why, can: x.ok && whyNotLeagueInvite(s, x.nat) === null })) : [],
    inviteCost: LEAGUE.invite.cost,
  };
  const worth = shown(DIPLOMACY.tradeIncome).toLocaleString('en-US');
  return {
    nation: me.name, date: turnToDate(s.turn).label, bloc: side, up,
    capital: Math.floor(me.pc), income: pcIncome(me.id), cap: DIPLOMACY.pcCap, partners: partners.size, cost: DIPLOMACY.court.cost,
    wind,
    help: mine === 'league' ? 'The League courts nobody: it wins members by invitation, and only those who feel the danger say yes.' : mine
      ? `Courting a neutral costs ${DIPLOMACY.court.cost} political capital and pulls it toward ${side}. A country that leans your way trades with you${DIPLOMACY.majors.includes(me.id) ? `, worth ${worth} a month to your treasury` : ''}; one that is fully won over joins your side.`
      : 'Your country belongs to neither bloc, so it has nobody to win the neutrals over to.',
    powers, rows, league, path,
  };
}

/** The last month the research strips show, as the browser game draws them. */
const WAR_ENDS = 76;
const PACE = ['Nobody at work: this branch stands still', 'One team: keeps to history\'s schedule', 'Two teams: half as fast again', 'Three teams: nearly twice the pace'];

/**
 * The design offices: every branch of the armed forces with what is in service, what comes next and when, how far
 * ahead of history's clock or behind it the designers are, and the teams at work on it. The other powers' designs
 * in service follow. A country with no design offices of its own gets only those.
 */
function research() {
  const s = game();
  const me = s.human && s.nations[s.human]?.alive ? s.nations[s.human] : null;
  if (!me) return null;
  const up = humanUp(s) && !onEve(s);
  const when = (t: number): string => turnToDate(Math.max(1, t)).label;
  const others = Object.values(s.nations).filter((n) => n.lab && n.alive && n.id !== me.id)
    .sort((a, b) => fieldArmy(s, b.id) - fieldArmy(s, a.id))
    .map((n) => ({
      nat: n.id, name: n.name, doctrine: doctrineOf(n.id),
      lines: FIELDS.map((f) => ({ label: RESEARCH.fields[f].label, name: inService(n.id, n.lab!, f)?.name ?? '' })).filter((l) => l.name),
    }));
  const lab = me.lab;
  if (!lab) {
    return {
      nation: me.name, date: turnToDate(s.turn).label, up, own: false, teams: 0, free: 0, doctrine: '', money: inHand(me.pp),
      note: `${me.name} has no design offices of its own and buys its weapons abroad. The weapons of every great power improve as the war goes on, each new design arriving in the month history brought it into service.`,
      bureau: null, fields: [], others,
    };
  }
  const free = teamsFree(lab);
  const cost = bureauCost(me);
  const fields = FIELDS.map((f: Field) => {
    const spec = RESEARCH.fields[f];
    const cur = inService(me.id, lab, f), next = nextStep(me.id, lab, f);
    const ahead = Math.round(monthsAhead(lab, f));
    const eta = arrival(s, me.id, f);
    const k = lead(s, me.id, f);
    const pct = Math.round(spec.per * k * 100);
    const n = lab.teams[f];
    const at = (t: number): number => Math.max(0, Math.min(WAR_ENDS, t)) / WAR_ENDS;
    return {
      id: f, label: f === 'doctrine' ? `${spec.label}: ${doctrineOf(me.id)}` : spec.label,
      ahead, pace: ahead > 0 ? `${ahead} month${ahead === 1 ? '' : 's'} ahead` : ahead < 0 ? `${-ahead} month${ahead === -1 ? '' : 's'} behind` : 'On history\'s schedule',
      now: cur?.name ?? 'nothing yet',
      next: next ? `Next: ${next.name}. History had it in ${when(next.at)}; ${eta === null ? 'at this pace it never comes' : `yours is due ${when(eta)}`}.` : 'The designers have nothing further on the drawing board.',
      effect: k ? `${spec.who}: ${pct > 0 ? '+' : ''}${pct}% while these designs are ${Math.abs(k) === 1 ? 'a generation' : `${Math.abs(k)} generations`} ${k > 0 ? 'ahead of' : 'behind'} history.` : '',
      good: k > 0,
      teams: n, max: MAX_TEAMS, work: PACE[n] ?? '',
      more: up && n < MAX_TEAMS && free > 0, fewer: up && n > 0,
      fill: at(lab.clock[f]), today: at(lab.months),
      marks: stepsOf(me.id, f).filter((st) => st.at > 0).map((st) => ({ at: at(st.at), got: st.at <= lab.clock[f], name: `${st.name}: ${when(st.at)} in history` })),
    };
  });
  return {
    nation: me.name, date: turnToDate(s.turn).label, up, own: true, teams: teamsTotal(lab), free, doctrine: doctrineOf(me.id), money: inHand(me.pp),
    note: 'Every design team keeps one branch of the armed forces on history\'s schedule. Put a second team on a branch and its next designs arrive early; take the team away and the branch falls behind. In battle a design counts for as long as it is ahead of its time.',
    bureau: cost === null ? null : { cost: shown(cost), can: up && me.pp >= cost, why: !up ? 'It is not your month.' : me.pp < cost ? `Needs ${shown(cost).toLocaleString('en-US')}; the treasury has ${inHand(me.pp).toLocaleString('en-US')}.` : '' },
    fields, others,
  };
}

/**
 * The technology tree of the grand campaign as the player's nation sees it: every technology with its branch and
 * step, what it does, what it needs, where it stands, and why it cannot be started now. ok is false in a
 * historical scenario, where research runs on history's clock and research() is the screen to show.
 */
function tech() {
  const s = game();
  const t = techTree(s);
  const me = s.human ? s.nations[s.human] : null;
  if (!t || !me) return { ok: false };
  const up = humanUp(s) && !onEve(s);
  return {
    ok: true, nation: me.name, date: turnToDate(s.turn).label, up,
    teams: t.teams, busy: t.busy, idle: Math.max(0, t.teams - t.busy), done: t.done, total: t.total,
    eras: t.eras,
    branches: t.branches,
    nodes: t.nodes.map((n) => ({
      id: n.id, branch: n.branch, tier: n.tier, name: n.name, text: n.text, rule: n.rule, state: n.state,
      months: n.months, left: n.left, of: n.of,
      needs: n.needs.filter((x) => !x.done).map((x) => x.name),
      can: up && n.state === 'ready' && n.why === null,
      why: n.state === 'ready' ? (!up ? 'It is not your month.' : n.why ?? '') : '',
      stop: up && n.state === 'work',
    })),
  };
}

/** Puts one of the player's free research teams to work on a technology. */
function techStart(id: string) {
  const s = game();
  const why = whyNotOrderResearch(s, id);
  return { ok: orderResearch(s, id), why: why ?? '', view: view() };
}

/** Takes the player's team off a technology. The work done on it is lost. */
function techStop(id: string) {
  const s = game();
  return { ok: orderStopResearch(s, id), view: view() };
}

/** Puts this many design teams on a branch of the player's research. */
function teams(field: string, n: number) {
  const s = game();
  return { ok: orderTeams(s, field as Field, n), view: view() };
}

/** Founds one more design team for the player, paid for from the treasury. */
function bureau() {
  const s = game();
  return { ok: orderBureau(s), view: view() };
}

/**
 * The generals of the player's nation: those in command and those waiting for one, each with his rank, skills,
 * traits and record, and the corps that have no general, with the best man waiting for each.
 */
function generals() {
  const s = game();
  const me = s.human && s.nations[s.human]?.alive ? s.nations[s.human] : null;
  if (!me) return null;
  const up = humanUp(s) && !onEve(s);
  const corpsLine = (c: Corps): string => `${c.name} (${KIND[c.kind].label.toLowerCase()}, ${c.divs} division${c.divs === 1 ? '' : 's'}) at ${s.regions[c.reg].name}`;
  const mine = s.generals.filter((x) => x.nat === me.id && (x.status === 'active' || x.status === 'wounded'));
  const list = mine.sort((a, b) => (a.corps >= 0 ? 0 : 1) - (b.corps >= 0 ? 0 : 1) || b.level - a.level || a.id - b.id).map((x) => {
    const c = corpsOf(s, x);
    const next = xpToNext(x);
    return {
      id: x.id, name: x.name, rank: rankOf(x), level: x.level, points: up ? x.points : 0,
      progress: Number.isFinite(next) && next > 0 ? Math.max(0, Math.min(1, x.xp / next)) : 1,
      where: x.status === 'wounded' ? `Wounded: fit again in ${turnToDate(x.backOn).label}` : c ? `Commands ${corpsLine(c)}` : 'Waiting for a command',
      commands: !!c, waiting: x.status === 'active' && !c,
      record: x.rec.battles ? `${x.rec.battles} battle${x.rec.battles === 1 ? '' : 's'}, ${x.rec.won} won${x.rec.wounds ? `, wounded ${x.rec.wounds === 1 ? 'once' : `${x.rec.wounds} times`}` : ''}` : 'No battles yet',
      skills: SKILLS.map((k) => ({ id: k, label: COMMAND.skills[k].label, value: x.skills[k], max: COMMAND.skillMax, blurb: COMMAND.skills[k].blurb, more: up && x.points > 0 && x.skills[k] < COMMAND.skillMax })),
      traits: x.traits.map((t) => COMMAND.traits[t]).filter(Boolean).map((t) => ({ label: t.label, good: t.good, effect: t.effect })),
      relieve: up && x.status === 'active' && !!c,
    };
  });
  const free = waiting(s, me.id);
  const empty = vacant(s, me.id).filter((c) => !c.dead)
    .sort((a, b) => b.divs - a.divs || a.id - b.id)
    .map((c) => {
      // The man on the list who suits this corps best, if any may take it this month.
      const able = free.filter((x) => !whyNotAssign(s, x, c)).sort((a, b) => fit(s, b, c) - fit(s, a, c) || b.level - a.level);
      const best = able[0];
      const why = whyNotPromote(s, c);
      return {
        corps: c.id, name: c.name, line: `${KIND[c.kind].label}, ${c.divs} division${c.divs === 1 ? '' : 's'}, at ${s.regions[c.reg].name}`,
        best: best && up ? { id: best.id, name: best.name, rank: rankOf(best), level: best.level } : null,
        promote: up && !why && canOrder(s, c), whyPromote: why || (!up ? 'It is not your month.' : canOrder(s, c) ? '' : 'This corps cannot be given orders this month.'),
      };
    });
  return {
    nation: me.name, date: turnToDate(s.turn).label, up, capital: Math.floor(me.pc), promoteCost: COMMAND.promote.pc,
    commanding: list.filter((x) => x.commands).length, waiting: free.length,
    note: `A corps with no general fights by the book. Give it one who is waiting, or promote an unknown officer from its ranks for ${COMMAND.promote.pc} political capital: he starts at level 1 and learns faster than the old guard. Generals gain experience in battle, and each new level gives a skill point to spend.`,
    generals: list, vacant: empty,
  };
}

/** Gives one of the player's waiting generals a corps. */
function assign(general: number, corps: number) {
  const s = game();
  return { ok: humanUp(s) && orderAssign(s, general, corps), view: view() };
}

/** Promotes an unknown officer to command one of the player's corps. */
function promoteIn(corps: number) {
  const s = game();
  const gen = humanUp(s) ? orderPromote(s, corps) : null;
  return { ok: !!gen, name: gen ? `${rankOf(gen)} ${gen.name}` : '', view: view() };
}

/** Takes one of the player's generals off his corps and puts him back on the list. */
function relieveOf(general: number) {
  const s = game();
  return { ok: humanUp(s) && orderRelieve(s, general), view: view() };
}

/** Spends one of a general's skill points. */
function skillUp(general: number, skill: string) {
  const s = game();
  return { ok: humanUp(s) && orderSkill(s, general, skill as Skill), view: view() };
}

const PATH_TEXT: Record<PathId, { title: string; text: string }> = {
  axis: { title: 'Join the Axis', text: 'Germany\'s side. Axis armies may cross your land and yours theirs, you share their supply lines, and you can go to war beside them. You are at war with nobody until you declare it or the Allies do.' },
  allies: { title: 'Join the Allies', text: 'Britain\'s side. Allied armies may cross your land and yours theirs, you share their supply lines, and you can go to war beside them. You are at war with nobody until you declare it or the Axis does.' },
  alone: { title: 'Stand alone', text: 'You are at nobody\'s side and nobody\'s quarrel is yours. If you are attacked you fight alone, unless you choose a side then.' },
  found: { title: 'Found the League', text: 'A third alliance, of smaller nations, apart from the Axis and the Allies. Its members are allies of one another, and war on one is war on them all. Pick the country that founds it with you; as a member you can invite more.' },
  join: { title: 'Join the League', text: 'The third alliance, of smaller nations, apart from the Axis and the Allies. Its members are allies of one another, and war on one is war on them all.' },
};

/** The player's small country takes a path: joins a bloc, stands alone, founds the League with a partner, or joins it. */
function takePathOf(id: string, partner = '') {
  const s = game();
  const h0 = s.headlines.length;
  const ok = orderPath(s, id as PathId, partner || undefined);
  const news = s.headlines.slice(h0).map((h) => h.text);
  return { ok, note: !ok ? 'That path cannot be taken today.' : news.length ? news.join(' ') : 'It is done.', view: view() };
}

/** The player, a member of the League, invites a willing country into it. */
function inviteNation(nat: string) {
  const s = game();
  const h0 = s.headlines.length;
  const why = whyNotLeagueInvite(s, nat);
  const ok = orderLeagueInvite(s, nat);
  const news = s.headlines.slice(h0).map((h) => h.text);
  return { ok, note: !ok ? why ?? 'They would not join today.' : news.length ? news.join(' ') : `${upFirst(theName(s, nat))} joins ${LEAGUE.name}.`, view: view() };
}

/** Spends the player's political capital to pull a neutral toward his bloc, and says what came of it. */
function courtNation(nat: string) {
  const s = game();
  const h0 = s.headlines.length;
  const before = s.nations[nat]?.opinion ?? 0;
  const ok = orderCourt(s, nat);
  const news = s.headlines.slice(h0).map((h) => h.text);
  const moved = Math.round(Math.abs((s.nations[nat]?.opinion ?? 0) - before));
  const note = !ok ? 'They could not be courted this month.' : news.length ? news.join(' ') : `Your envoys are heard in ${theName(s, nat)}: opinion there moves ${moved} point${moved === 1 ? '' : 's'} your way.`;
  return { ok, note, view: view() };
}

const dateOf = (turn: number): string => turnToDate(turn).label;

/** One corps' service record: what it is, where it came from, the honours it carries and its diary, newest first. */
function record(id: number) {
  const s = game();
  const c = s.corps.find((x) => x.id === id);
  if (!c) return null;
  const n = s.nations[c.nat];
  const gen = generalOf(s, c);
  const mine = c.nat === s.human;
  const notes: string[] = [];
  if (c.home !== undefined && mine && c.kind !== 'fort') {
    const home = s.regions[c.home];
    notes.push(c.rec?.exile
      ? `Home: ${home.name}. It is in enemy hands. The corps attacks ${Math.round(STORIES.home.attack * 100)}% stronger to win it back.`
      : `Home: ${home.name}. The corps defends it ${Math.round(STORIES.home.defence * 100)}% stronger than any other ground.`);
  }
  for (const r of s.stories.rivals.filter((x) => x.mine === c.id).slice(-2)) {
    const adj = s.nations[r.theirsNat]?.adj ?? '';
    notes.push(r.over
      ? `Old enemy: ${adj} ${r.theirsName}. ${r.over === 'won' ? `Destroyed by this corps in ${dateOf(r.ended ?? r.since)}, after ${r.met} meetings.` : r.over === 'lost' ? `It had the better of this corps in the end, ${dateOf(r.ended ?? r.since)}.` : `Lost elsewhere in ${dateOf(r.ended ?? r.since)}.`}`
      : `Old enemy: ${adj} ${r.theirsName}. They first met at ${s.regions[r.first]?.name ?? 'the front'} and have met ${r.met} times.`);
  }
  return {
    id: c.id,
    name: c.name,
    nick: c.nick ?? null,
    what: `${n.adj} ${KIND[c.kind].label.toLowerCase()}${isGuards(s, c) ? ` \u00B7 ${designation(c.nat)}` : ''} \u00B7 ${c.divs} division${c.divs > 1 ? 's' : ''} \u00B7 ${men(c).toLocaleString('en')} men`,
    where: `At ${s.regions[c.reg].name}${gen ? ` \u00B7 ${rankOf(gen)} ${gen.name}` : ''}`,
    raised: `Raised ${c.heir !== undefined ? 'again ' : ''}${dateOf(c.born ?? 1)}${c.home !== undefined ? ` at ${s.regions[c.home].name}` : ''} \u00B7 ${stat(s, c, 'service')} months in service`,
    xp: Math.round(c.xp * 100),
    grade: grade(c.xp),
    notes,
    honours: honoursOf(s, c).map((a) => ({
      title: awardTitle(s, a), name: honourDef(a.hon)?.name ?? '', citation: citation(s, a), date: dateOf(a.turn),
      ribbon: honourDef(a.hon)?.ribbon ?? ['#b08d57', '#f2ead6'], old: !!a.old,
    })),
    diary: (c.log ?? []).slice(-14).reverse().map((line) => ({ date: dateOf(line[0]), text: diaryText(s, c, line) })),
  };
}

/** The roll of honour and the memorial wall: the player's nation's, or everybody's when nobody is being led. */
function roll() {
  const s = game();
  const me = s.human;
  const ours = <T extends { nat: string }>(list: T[]): T[] => (me ? list.filter((x) => x.nat === me) : list);
  return {
    awards: ours(s.stories.awards).filter((a) => !a.old).slice(-40).reverse().map((a) => ({
      id: a.id, title: awardTitle(s, a), name: honourDef(a.hon)?.name ?? '', unit: a.name, nat: a.nat, kind: a.kind,
      citation: citation(s, a), date: dateOf(a.turn), ribbon: honourDef(a.hon)?.ribbon ?? ['#b08d57', '#f2ead6'], fame: fameOf(a),
    })),
    fallen: ours(s.stories.fallen).slice(-40).reverse().map((f) => ({
      name: f.name, nat: f.nat, date: dateOf(f.turn), where: s.regions[f.reg]?.name ?? '', how: f.how,
      by: s.nations[f.by]?.adj ?? f.by, general: f.general ?? null, honours: honoursOf(s, f).map((a) => awardTitle(s, a)),
    })),
  };
}

/** The raids flown in a month as the player may know them, and how many air battles the war has seen (for checking). */
function raidsFlown(turn?: number) {
  const s = game();
  return { battles: s.flags.airBattles ?? 0, reports: s.airReports.length, turns: [...new Set(s.airReports.map((r) => r.turn))], raids: flownRaids(s, turn ?? s.turn) };
}

(globalThis as Record<string, unknown>).TC = {
  raidsFlown,
  land, work, force, forceOrder, seaReach, ship, landingPlan, landOn, record, roll, stance, railReach, rail, province, raise, fortify, cancel, declare, warPlan, diplomacy, court: courtNation, path: takePathOf, invite: inviteNation, page, act, abilities, research, tech, techStart, techStop, teams, bureau, generals, assign, promote: promoteIn, relieve: relieveOf, skill: skillUp, ledger, arsenal, order, starts, grand, start, save, peek, load, view, reach: reachOf, move, previewAttack: preview, attack, endMonth };
