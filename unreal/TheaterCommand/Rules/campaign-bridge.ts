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
import { DIPLOMACY, TRACK, TRACK_LABEL, TRACK_PHRASE, canCourt, courtable, friendsOf, momentum, pcIncome, receptive, step, tradePartners } from '../../../src/campaign/diplomacy';
import {
  arsenalOffers, beginEve, beginHuman, canOrder, candidates, current, declarable, endEve, endHuman, humanUp, onEve, orderAttack, orderBuildCorps, orderArsenal, orderDeclare,
  orderBuildFort, orderCancelBuild, orderMove, orderRail, orderStance, postureOf, previewAttack, reach, runTurnAI, stepAI,
  orderAir, orderAirBase, orderFleet, levelNow, orderLevel, orderLand, orderShip, previewLanding,
  invasions, previewDeclare, orderCourt,
  engineerJobs, orderBlowBridges, orderCutRailway, orderEngineers, orderStopWork, whyNotBlowBridges, whyNotCutRailway,
} from '../../../src/campaign/turn';
import { jobLine, type JobKind } from '../../../src/campaign/engineers';
import { BUDGET, PRODUCTION, budgeted, canRaise, corpsUpkeep, fortCost, income, isHome, ledger as ledgerOf, provinceYield } from '../../../src/campaign/economy';
import { generalOf, rankOf } from '../../../src/campaign/commanders';
import { STORIES, awardTitle, citation, designation, diaryText, fameOf, grade, honourDef, honoursOf, isGuards, stat, takeOver } from '../../../src/campaign/stories';
import { KIND } from '../../../src/campaign/types';
import { GOALS, GOAL_KEYS, judgeGoals, openGoals } from '../../../src/campaign/goals';
import { isOpen, startOpen } from '../../../src/campaign/staff';
import { loadGame, peekSave, saveGame } from '../../../src/campaign/save';
import { GROUND, groundRules, groundSays } from '../../../src/campaign/ground';
import { CROSSINGS, RIVERS, bridgeDown, riverNotes } from '../../../src/campaign/rivers';
import { RAIL_LINKS, railCut, railNote } from '../../../src/campaign/rails';
import { fleetReach, landingTargets, whyNotShip } from '../../../src/campaign/navy';
import { MISSION, airBases, airTargets } from '../../../src/campaign/air';
import type { AirMission, Stance } from '../../../src/campaign/types';
import { DIFFICULTY } from '../../../src/campaign/difficulty';
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
    regions: s.regions.map((r) => ({ id: r.id, ctrl: r.ctrl, stand: standingOf(s, r.ctrl) })),
    corps: s.corps.filter((c) => !c.dead).map((c) => ({
      id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, str: c.str, org: c.org, reg: c.reg,
      ap: c.ap, apMax: c.apMax, gen: generalOf(s, c)?.name ?? null, posture: postureOf(c),
      canOrder: up && canOrder(s, c), supplied: c.supplied, men: men(c),
      // Its story so far: how seasoned it is, the name the army gives it, and the honours on its colours.
      xp: Math.round(c.xp * 100), grade: grade(c.xp), nick: c.nick ?? null,
      honours: honoursOf(s, c).filter((a) => !a.old).map((a) => ({ title: awardTitle(s, a), ribbon: honourDef(a.hon)?.ribbon ?? ['#b08d57', '#f2ead6'] })),
    })),
    // The fleets, and the sea each is in (a fleet in harbour lies at its base), and the air fleets on their airfields.
    fleets: s.fleets.filter((f) => f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd + f.ships.ss > 0).map((f) => ({
      id: f.id, nat: f.nat, name: f.name, zone: f.zone, base: f.base, port: f.port, stance: f.stance,
      bb: f.ships.bb, cv: f.ships.cv, ca: f.ships.ca, dd: f.ships.dd, ss: f.ships.ss,
    })),
    air: s.airFleets.filter((f) => f.ftr + f.bmr > 0).map((f) => ({
      id: f.id, nat: f.nat, name: f.name, base: f.base, ftr: Math.round(f.ftr), bmr: Math.round(f.bmr), mission: f.mission,
    })),
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

/** One computer nation's month. If it did something worth seeing, that is noted. */
function stepOne(s: Game, steps: unknown[] | null): void {
  {
    const ctrl = s.regions.map((r) => r.ctrl);
    const r0 = s.reports.length;
    const month = s.turn;
    const nat = stepAI(s);
    if (s.human && s.turn !== month) judgeGoals(s, s.human);
    if (!steps) return;
    const taken = s.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = s.reports.slice(r0);
    if (!taken.length && !reps.length) return;
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
      note: `${s.nations[nat].adj} forces ${said.join(', ')}.`,
      taken,
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
  nations: { id: string; years: Record<string, string> }[];
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
  const nations = Object.entries(h.nations)
    .filter(([nat, n]) => n.alive && (!order.includes(nat) || GRAND.nations.find((x) => x.id === nat)!.years[year] !== undefined))
    .map(([nat, n]) => {
      const foes = Object.keys(h.nations).filter((other) => other !== nat && h.nations[other].alive && atWar(h, nat, other));
      return {
        nat,
        name: n.name,
        major: order.includes(nat),
        blurb: GRAND.nations.find((x) => x.id === nat)?.years[year] ?? '',
        standing: foes.length ? `At war with ${listNames(foes.map((f) => h.nations[f].name))}` : 'At peace',
        corps: h.corps.filter((c) => c.nat === nat).length,
        regions: h.regions.filter((r) => r.ctrl === nat).length,
      };
    })
    .filter((n) => n.corps > 0 || n.regions > 0)
    .sort((a, b) => (a.major === b.major ? (a.major ? order.indexOf(a.nat) - order.indexOf(b.nat) : a.name.localeCompare(b.name)) : a.major ? -1 : 1));
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
  const steps: unknown[] = [];
  let fallen: string | null = null;
  if (!s.human) {
    // Nobody at the controls (the player's country has fallen): one whole month of the war, every nation on the computer.
    const month = s.turn;
    for (let guard = 0; guard < 80 && s.turn === month; guard++) stepOne(s, steps);
  } else {
    if (onEve(s)) endEve(s);
    else {
      judgeGoals(s, s.human);
      endHuman(s);
    }
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
  return { steps, headlines, events, fallen, view: view() };
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
  if (f.trade) lines.push(`Its trade ends. ${The} trades with you today, worth ${trade} a month to your treasury.`);
  if (f.sneak > 1) lines.push(`Surprise. It is at peace and not expecting you: this month your attacks on its ground are ${Math.round((f.sneak - 1) * 100)}% stronger. Next month the bonus is gone.`);
  else if (n.atWar.some((x) => x === s.human || allied(s, x, s.human!))) lines.push('No surprise: it is already at war with your side and its army is ready.');
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
  } else if (f.joins === null && bloc(n) !== null) lines.push(`${The} already belongs to ${blocOf(bloc(n)!)}: the neutral world expects this war and takes no fright at it.`);
  else if (!mine) lines.push('Your country belongs to neither bloc, so the neutrals have no side to turn away from. They will remember it all the same.');
  const at = TRACK.indexOf(n.align) - 3;
  const stands = at === 0 ? 'is neutral and at peace with you' : Math.abs(at) === 1 ? `is neutral, leans toward ${blocOf(at > 0 ? 'axis' : 'allies')}, and is at peace with you` : `is ${TRACK_PHRASE[n.align]}, but at peace with you`;
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
  const rows = all.filter((n) => n.alive && !n.atWar.length && n.id !== me.id).sort((a, b) => a.opinion - b.opinion).map((n) => {
    const open = !!mine && courtable(n);
    const c = open ? canCourt(s, me.id, n.id) : { ok: false, why: '' };
    const k = open ? receptive(n, mine!) : 0;
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
  const side = mine === 'axis' ? 'the Axis' : mine === 'allies' ? 'the Allies' : '';
  const worth = shown(DIPLOMACY.tradeIncome).toLocaleString('en-US');
  return {
    nation: me.name, date: turnToDate(s.turn).label, bloc: side, up,
    capital: Math.floor(me.pc), income: pcIncome(me.id), cap: DIPLOMACY.pcCap, partners: partners.size, cost: DIPLOMACY.court.cost,
    wind,
    help: mine
      ? `Courting a neutral costs ${DIPLOMACY.court.cost} political capital and pulls it toward ${side}. A country that leans your way trades with you${DIPLOMACY.majors.includes(me.id) ? `, worth ${worth} a month to your treasury` : ''}; one that is fully won over joins your side.`
      : 'Your country belongs to neither bloc, so it has nobody to win the neutrals over to.',
    powers, rows,
  };
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

(globalThis as Record<string, unknown>).TC = {
  land, work, force, forceOrder, seaReach, ship, landingPlan, landOn, record, roll, stance, railReach, rail, province, raise, fortify, cancel, declare, warPlan, diplomacy, court: courtNation, ledger, arsenal, order, starts, grand, start, save, peek, load, view, reach: reachOf, move, previewAttack: preview, attack, endMonth };
