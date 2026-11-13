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

import { DEFENCE, allied, atWar, bloc, collapseScore, createGame, fieldArmy, frontage, limitAttackers, men, railReach as railReachOf, weather as weatherNow } from '../../../src/campaign/game';
import { FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, inService, lead, monthsAhead, nextStep, stepsOf, teamsFree, teamsTotal } from '../../../src/campaign/research';
import type { Field, Skill } from '../../../src/campaign/types';
import { factoriesBuilt, workLevel, yardLevel } from '../../../src/campaign/arsenal';
import { hasTech, idleTeams, techOf } from '../../../src/campaign/tech';
import { eliteOf, traitRule, traitRules, traitsOf, eliteRule } from '../../../src/campaign/traits';
import { LEAGUE, leagueFounded, leagueMembers, type PathId } from '../../../src/campaign/league';
import { DIPLOMACY, TRACK, TRACK_LABEL, TRACK_PHRASE, canCourt, courtable, friendsOf, momentum, pcIncome, receptive, step, tradePartners } from '../../../src/campaign/diplomacy';
import {
  arsenalOffers, beginEve, beginHuman, canOrder, candidates, current, declarable, endEve, endHuman, humanUp, onEve, orderRebase, orderAttack, orderBuildCorps, orderArsenal, orderDeclare,
  orderBuildFort, orderCancelBuild, orderMove, orderRail, orderStance, postureOf, previewAttack, reach, runTurnAI, stepAI,
  orderAir, orderAirBase, orderFleet, levelNow, orderLevel, orderLand, orderShip, previewLanding,
  depots, shippingLanes, previewAirOrder,
  townBuildings, orderBuilding, buildingTips, tradeLedger, orderTrade, orderEndTrade,
  fleetsInSight, wingsOfFleet,
  prizesOf, prizesOnMap, whyNotWreckPrize, orderWreck, whyNotBurnOrder, orderBurn, orderScorchPolicy,
  governmentOf, whyNotAppointMinister, orderAppoint, whyNotOrderDecree, orderDecree, whyNotSetAsideDirective, orderSetAside,
  townSiege, siegesInSight, whyNotGarrisonTown, orderGarrison, whyNotLeaveTown, orderTakeField, whyNotStormTown, stormOutlook, orderStorm, whyNotSallyOut, sallyOutlook, orderSally,
  sight, secretService, orderRecruitAgent, orderSendAgent, orderAgentMission, orderRetireAgent, orderFog, whyNotSendAgent,
  lastAct, standings, orderDomination, orderKeepPeace, whyNotOrderDomination,
  orderBureau, orderTeams, techTree, orderResearch, orderStopResearch, whyNotOrderResearch, orderAssign, orderPromote, orderRelieve, orderSkill,
  invasions, previewDeclare, orderCourt, leagueInvites, leaguePartners, orderLeagueInvite, orderPath, paths, whyNotLeagueInvite,
  engineerJobs, orderBlowBridges, orderCutRailway, orderEngineers, orderStopWork, whyNotBlowBridges, whyNotCutRailway,
} from '../../../src/campaign/turn';
import { jobLine, type JobKind } from '../../../src/campaign/engineers';
import { BUDGET, PRODUCTION, budgeted, canRaise, corpsUpkeep, fortCost, income, isHome, ledger as ledgerOf, provinceYield } from '../../../src/campaign/economy';
import { PRIZES, type PrizeKind, type ScorchPolicy } from '../../../src/campaign/prizes';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, rankOf, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../../../src/campaign/commanders';
import { STORIES, awardTitle, citation, designation, diaryText, fameOf, grade, honourDef, honoursOf, isGuards, stat, takeOver } from '../../../src/campaign/stories';
import { KIND } from '../../../src/campaign/types';
import { GOALS, GOAL_KEYS, goalChecks, goalEnds, goalState, goalWhen, judgeGoals, openGoals } from '../../../src/campaign/goals';
import { isOpen, startOpen } from '../../../src/campaign/staff';
import { loadGame, peekSave, saveGame } from '../../../src/campaign/save';
import { GROUND, groundRules, groundSays } from '../../../src/campaign/ground';
import { CROSSINGS, RIVERS, bridgeDown, riverNotes } from '../../../src/campaign/rivers';
import { RAIL_LINKS, railCut, railNote } from '../../../src/campaign/rails';
import { baseTargets, fleetReach, landingTargets, seaAt as seaUnder, whyNotShip } from '../../../src/campaign/navy';
import { AIR, MISSION, airBases, airRange, airTargets, forecastAir, km as airKm, whyNotMission } from '../../../src/campaign/air';
import type { AirMission, Stance } from '../../../src/campaign/types';
import { DIFFICULTY } from '../../../src/campaign/difficulty';
import { airAct, airPage } from '../../../src/campaign/airpages';
import { fleetOptions, fleetView, orderBombard, orderEscort, orderMerge, orderSplit, previewBombard, whyNotSplit } from '../../../src/campaign/turn';
import { orderScout as sendScout, previewRaid as weighRaid, joinOffers, orderJoinAll, orderSendBack, orderEvacuate, evePlan, orderEvePlan, orderCash, orderSeen, orderDrafts, orderSack } from '../../../src/campaign/turn';
import { whyNotCash } from '../../../src/campaign/stories';
import { popular } from '../../../src/campaign/commanders';
import { seaOpenAt } from '../../../src/campaign/navy';
import { airfieldsSeen, announcements, attackOutlook, whyNotAttack, corpsDetail, corpsKnownAs, fallenCapitalAt, flownRaids, knowledgeOf, nationsFallen, playerFell, prize as prizeOfPlace, townIn, towns as townsOfMap } from '../../../src/campaign/turn';
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

/** A corps of the player's as its card reads: its divisions, how it feels and how rested it is. Empty if the rules keep none. */
function cardOf(s: Game, c: Corps): Record<string, unknown> {
  try {
    const d = corpsDetail(s, c);
    const m = d.mood;
    const pts = (x: number): string => `${x >= 0 ? '+' : '\u2212'}${Math.max(1, Math.round(Math.abs(x) * 100))}`;
    return {
      mor: m.morale ?? -1, morWord: m.moraleWord, trend: m.trend, rest: m.rest, restWord: m.restWord, moodLine: m.line, restLine: m.restLine, resting: m.resting,
      why: m.why.slice(0, 6).map((w) => `${pts(w.delta)}  ${w.text}${w.earlier ? ' (last month)' : ''}`),
      service: d.service,
      dv: d.divisions.map((x) => ({ name: x.short, full: x.name, kind: x.kind, str: x.str, men: x.men, stripes: x.stripes, grade: x.grade, hard: x.hardened })),
    };
  } catch {
    return {};
  }
}

/** What taking a place is worth, in a sentence; nothing where it is worth nothing (bare ground, or a scenario that keeps no money). */
function prizeLine(s: Game, region: number): string {
  const z = prizeOfPlace(s, region);
  return z && (z.income > 0 || z.incomeNow > 0 || z.vp > 0 || z.factories > 0 || z.oil > 0 || z.capitalOf || z.frees) ? z.line : '';
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
    // Where an ally's corps stand beside the player's own and could be asked to march with them: the province, how many, and whose.
    joins: s.human && up ? [...new Set(s.corps.filter((c) => !c.dead && c.nat === s.human && c.kind !== 'fort').map((c) => c.reg))].map((reg) => {
      const can = joinOffers(s, reg).filter((o) => !o.why);
      return { region: reg, n: can.length, nats: [...new Set(can.map((o) => s.nations[o.nat]?.adj ?? o.nat))] };
    }).filter((x) => x.n > 0) : [],
    // Ground the player has no eyes on (where the fog of war is kept): the map dims it.
    unseen: s.human ? s.regions.filter((r) => seen[r.id] === 0).map((r) => r.id) : [],
    // The weather the rules keep: mud in the east in spring and autumn, and the eastern winter. With it, the ground it lies on.
    weather: (() => {
      const w = weatherNow(s);
      // (the countries the rules count as the east; the winter bites on Soviet ground)
      const east = new Set(w.mud ? ['POL', 'SOV', 'LIT', 'LAT', 'EST', 'ROM', 'FIN'] : w.winter ? ['SOV'] : []);
      return { mud: w.mud, winter: w.winter, text: w.mud || w.winter ? w.text : '', ground: s.regions.filter((r) => east.has(r.owner)).map((r) => r.id) };
    })(),
    // On the eve of war: the staff has a plan for placing the army, and will carry it out if asked.
    evePlan: onEve(s) && !!evePlan(s),
    // Whether the player's country is at war with anybody: the music follows it.
    atWar: !!s.human && (s.nations[s.human]?.atWar.length ?? 0) > 0,
    eve: onEve(s),
    // (Version 58) In the grand campaign whoever stands a month digs in unasked: the tray then offers two orders,
    // stand (and dig in) or fall back; and the works are finished after this many months.
    autoDig: s.flags.open === 1 && !!DEFENCE.entrench.auto, dugFull: DEFENCE.entrench.months,
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
    corps: s.corps.filter((c) => !c.dead && seen[c.reg] > 0).map((c) => {
      // A foreign corps is known exactly, from a report gone stale, or not identified at all (the grand campaign).
      const kn = s.human && c.nat !== s.human && !allied(s, c.nat, s.human) ? knowledgeOf(s, c) : { level: 'exact' as const, age: 0 };
      if (seen[c.reg] === 1 || kn.level === 'none') {
        return { id: c.id, nat: c.nat, name: seen[c.reg] === 1 ? 'An army seen from afar' : 'A corps not identified', kind: 'inf', elite: '', divs: 0, str: 0, org: 0, reg: c.reg, ap: 0, apMax: 0, gen: null, posture: postureOf(c), canOrder: false, supplied: true, men: 0, xp: 0, grade: '', nick: null, honours: [], dim: true, near: seen[c.reg] !== 1 };
      }
      if (kn.level === 'report') {
        // Known from a report some months old: its name and its arm, and its strength only as a band.
        const card = corpsKnownAs(s, c);
        const lo = card?.strLo ?? c.str, hi = card?.strHi ?? c.str, mid = (lo + hi) / 2;
        return { id: c.id, nat: c.nat, name: c.name, kind: c.kind, elite: '', divs: c.divs, str: mid, org: 0.5, reg: c.reg, ap: 0, apMax: 0, gen: null, posture: postureOf(c), canOrder: false, supplied: true, men: Math.round(c.divs * 15000 * mid), xp: 0, grade: '', nick: null, honours: [], dim: false, stale: kn.age, strLo: lo, strHi: hi };
      }
      return ({
      id: c.id, nat: s.human && c.joint === s.human ? s.human : c.nat, ally: s.human && c.joint === s.human ? c.nat : '', name: c.name, kind: c.kind, elite: c.elite ?? '', divs: c.divs, str: c.str, org: c.org, reg: c.reg,
      ap: c.ap, apMax: c.apMax, gen: generalOf(s, c)?.name ?? null, posture: postureOf(c),
      canOrder: up && canOrder(s, c), supplied: c.supplied, dug: !!c.dug, dugFor: (c as { dugFor?: number }).dugFor ?? 0, town: !!(c as { town?: unknown }).town, men: men(c),
      // Its story so far: how seasoned it is, the name the army gives it, and the honours on its colours.
      xp: Math.round(c.xp * 100), grade: grade(c.xp), nick: c.nick ?? null,
      honours: honoursOf(s, c).filter((a) => !a.old).map((a) => ({ title: awardTitle(s, a), ribbon: honourDef(a.hon)?.ribbon ?? ['#b08d57', '#f2ead6'] })),
      dim: false,
      // The player's own corps as its card reads: its divisions by name, its morale and its rest, each with why.
      lean: !!c.lean,
      ...(c.nat === s.human || (s.human && c.joint === s.human) ? cardOf(s, c) : {}),
    }); }),
    // The fleets, and the sea each is in (a fleet in harbour lies at its base), and the air fleets on their airfields.
    // (Version 53: only the fleets the player's side has found. One that is only reported is a flag and a rough
    // size: its ships are not counted, and what it is about is not known. One not found is not on the table.)
    fleets: fleetsInSight(s).map((k) => ({ k, f: s.fleets.find((x) => x.id === k.id)! })).filter((x) => !!x.f).map(({ k, f }) => {
      const exact = k.known === 'exact';
      const wings = exact ? wingsOfFleet(s, f.id) : [];
      return {
      id: f.id, nat: f.nat, name: k.name, zone: f.zone, home: f.home, base: f.base, port: exact ? f.port : '', stance: exact ? f.stance : (k.atSea ? 'patrol' : 'port'),
      bb: exact ? f.ships.bb : 0, cv: exact ? f.ships.cv : 0, ca: exact ? f.ships.ca : 0, dd: exact ? f.ships.dd : 0, ss: exact ? f.ships.ss : 0,
      report: !exact, size: k.size,
      // The aircraft its carriers bring: fighters, and strike aircraft.
      wingFtr: Math.round(wings.filter((w) => w.kind === 'ftr').reduce((n, w) => n + w.aircraft, 0)),
      wingStrike: Math.round(wings.filter((w) => w.kind !== 'ftr').reduce((n, w) => n + w.aircraft, 0)),
      // For the player's own fleets: the seas each can reach this month, so that a fleet in hand shows where it can go.
      reach: f.nat === s.human && up ? fleetReach(s, f) : [],
      // And the friendly ports it could make its new home.
      bases: f.nat === s.human && up ? baseTargets(s, f).map((r) => r.id) : [],
      };
    }),
    // The airfields as the player knows them (the grand campaign): where each is, whose, how big (1 to 4), and how
    // many groups and aircraft stand on it, or -1 where the fog hides that.
    airfields: fields.map((a) => ({ region: a.region, nat: a.holder, side: a.side, size: a.size, word: a.word, holds: a.holds, groups: a.groups, aircraft: a.aircraft, crowded: a.crowded })),
    air: s.airFleets.filter((f) => f.ftr + f.bmr > 0 && (f as { ship?: number }).ship === undefined && !fieldHidden.has(f.base)).map((f) => ({
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
    // Capitals of fallen countries the player would set free by taking them (or holds already): ringed in gold.
    free: nationsFallen(s).filter((f) => f.canReturn && f.yours && f.capital >= 0).map((f) => ({ region: f.capital, nat: f.nat })),
    alerts: alertsOf(s),
    todo: todoOf(s),
    // For the night: the nations at war, whose towns are blacked out; and the fronts, where armies at war stand in
    // provinces side by side, as far as the player sees them (pairs of regions), for the guns that flicker there.
    dark: Object.values(s.nations).filter((n) => n.alive && n.atWar.length > 0).map((n) => n.id),
    fronts: (() => {
      const there = new Map<number, Set<string>>();
      for (const c of s.corps) {
        if (c.dead || !(seen[c.reg] > 0) || c.kind === 'fort') continue;
        if (!there.has(c.reg)) there.set(c.reg, new Set());
        there.get(c.reg)!.add(c.nat);
      }
      const out: number[] = [];
      for (const [reg, nats] of there) {
        for (const nb of s.regions[reg].nb) {
          const other = nb > reg ? there.get(nb) : undefined;
          if (other && [...nats].some((a) => [...other].some((b) => atWar(s, a, b)))) out.push(reg, nb);
        }
      }
      return out.slice(0, 160);
    })(),
    // Prizes to mark on the map (Version 56): for each province the region, the tag of the army that holds prizes
    // outside a town it has not taken ('' if none; an airfield alone does not count), and 1 if something there burns.
    prizes: (() => {
      try {
        return prizesOnMap(s).map((m) => ({ region: m.region, by: m.by && m.seized.some((k) => k !== 'airfield') ? m.by : '', burning: m.burning.length > 0 ? 1 : 0 }))
          .filter((m) => m.by || m.burning);
      } catch { return []; }
    })(),
    // Towns under siege that the player knows of (Version 54): region, 1 if the ring is closed, months of stores left.
    sieges: (() => { try { return siegesInSight(s).filter((v) => v.by).flatMap((v) => [v.region, v.closed ? 1 : 0, v.left]); } catch { return []; } })(),
    // Where battles were lately fought, for the scars they leave on the table: three numbers for each place, the
    // region, the months since, and how big the fighting was (1 to 3). Only what the player saw or took part in.
    scars: (() => {
      const last = new Map<number, [number, number]>();
      for (const r of s.reports) {
        const age = s.turn - r.turn;
        if (age < 0 || age > 3 || r.yielded) continue;
        const mine = !s.human || r.attNat === s.human || r.defNat === s.human || allied(s, r.attNat, s.human) || allied(s, r.defNat, s.human);
        if (!mine && !(seen[r.region] > 0)) continue;
        const lost = r.attLost + r.defLost;
        const size = lost >= 30000 ? 3 : lost >= 8000 ? 2 : 1;
        const cur = last.get(r.region);
        if (!cur) last.set(r.region, [age, size]);
        else last.set(r.region, [Math.min(cur[0], age), Math.max(cur[1], size)]);
      }
      return [...last.entries()].flatMap(([reg, [age, size]]) => [reg, age, size]);
    })(),
    // Every building of the towns' own that stands or is going up, for the table: region, line (0 market, 1 farm,
    // 2 school, 3 camp, 4 hospital, 5 hall), the level that stands, and 1 while the next is being built.
    civic: (() => {
      const LINES = ['market', 'farm', 'school', 'camp', 'hospital', 'hall'];
      const seen = new Map<string, [number, number, number, number]>();
      for (const k of Object.keys(s.flags)) {
        if (!k.startsWith('cv:')) continue;
        const [, line, reg] = k.split(':');
        const i = LINES.indexOf(line), lv = s.flags[k];
        if (i >= 0 && lv > 0) seen.set(`${reg}:${i}`, [Number(reg), i, lv, 0]);
      }
      for (const n of Object.values(s.nations)) {
        for (const b of n.queue) {
          const q = b as unknown as { what?: string; item?: string; region?: number };
          const i = q.what === 'civic' ? LINES.indexOf(q.item ?? '') : -1;
          if (i < 0 || q.region === undefined) continue;
          const cur = seen.get(`${q.region}:${i}`);
          if (cur) cur[3] = 1; else seen.set(`${q.region}:${i}`, [q.region, i, 0, 1]);
        }
      }
      return [...seen.values()].flat();
    })(),
    pages: pagesOf(s),
    // Oil and supplies in the player's depots (the grand campaign; null for a small country or a scenario), each with
    // the months it will last and its books in words, and every shipping lane to draw over the sea.
    stores: storesOf(s),
    lanes: s.flags.open === 1 ? [
      ...shippingLanes(s).filter((l) => l.kind === 'sea' && l.path.length > 1 && l.state !== 'idle').map((l) => ({ path: l.path.flat(), state: l.state, flow: l.flow })),
      // The player's trade routes, over land and sea, in gold: red where one is cut.
      ...(tradeLedger(s)?.deals ?? []).filter((d) => d.path.length > 1).map((d) => ({ path: d.path.flat(), state: d.state === 'open' ? 'trade' : `trade-${d.state}`, flow: d.full > 0 ? d.pays / d.full : 1 })),
    ] : [],
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
interface Flight { from: number; x: number; y: number; nat: string; bombers: boolean; escort: boolean; region: number; aa: number; meet: string; meetFrom: number; loss: number; fromX?: number; fromY?: number; fleet?: number }

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
      // (a carrier wing has no airfield: it takes off from where its fleet was, which the report gives as a point)
      const deck = (x: { base: number; x: number; y: number }): boolean => x.base < 0 && (x.x > 0 || x.y > 0);
      for (const a of r.attackers.filter((x) => (x.base >= 0 || deck(x)) && x.ftr + x.bmr >= 1).slice(0, 4)) {
        flights.push({
          from: a.base, ...(deck(a) ? { fromX: a.x, fromY: a.y, fleet: (s.airFleets.find((g) => g.id === a.id) as { ship?: number } | undefined)?.ship ?? -1 } : {}), x: r.x, y: r.y, nat: a.nat, bombers: a.bmr >= 1, escort: a.bmr >= 1 && a.ftr >= 1, region: r.region,
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

const lastSeaReport = (s: Game): number => s.seaReports.reduce((m, r) => Math.max(m, r.id), -1);
const lastShoreReport = (s: Game): number => (s.shoreReports ?? []).reduce((m, r) => Math.max(m, r.id), -1);

/** The coasts shelled from the sea since a report (by its id): where, what came of each, and whether it was the player's coast. */
function shellingSince(s: Game, since: number): { regions: number[]; notes: string[]; mine: boolean } {
  const fresh = (s.shoreReports ?? []).filter((r) => r.id > since && r.nat !== s.human);
  return {
    regions: [...new Set(fresh.map((r) => r.region))],
    notes: fresh.map((r) => `${s.nations[r.nat]?.adj ?? r.nat} warships shell ${r.place}. ${r.result}`),
    mine: fresh.some((r) => r.defNat === s.human || s.regions[r.region]?.ctrl === s.human),
  };
}

/** The fights at sea since a report (by its id): the seas they were in, a line on each, and whether the player's ships were in one. */
function seaFightsSince(s: Game, since: number): { zones: number[]; notes: string[]; mine: boolean } {
  const fresh = s.seaReports.filter((r) => r.id > since);
  const adj = (nat: string): string => s.nations[nat]?.adj ?? nat;
  return {
    zones: [...new Set(fresh.map((r) => r.zone))],
    notes: fresh.map((r) => `${adj(r.attNat)} and ${adj(r.defNat)} ships fight in the ${r.zoneName}: ${r.won ? `the ${adj(r.attNat)} fleet has the better of it` : `the ${adj(r.defNat)} fleet holds the sea`}.`),
    mine: fresh.some((r) => r.attNat === s.human || r.defNat === s.human),
  };
}

/** One computer nation's month. If it did something worth seeing, that is noted. */
function stepOne(s: Game, steps: unknown[] | null): void {
  {
    const ctrl = s.regions.map((r) => r.ctrl);
    const r0 = s.reports.length;
    const month = s.turn;
    const stood = new Map(s.corps.filter((c) => !c.dead).map((c) => [c.id, c.reg] as const));
    const air0 = lastAirReport(s), sea0 = lastSeaReport(s), shore0 = lastShoreReport(s);
    const nat = stepAI(s);
    if (s.human && s.turn !== month) judgeGoals(s, s.human);
    if (!steps) return;
    // The raids it flew, or that were flown as its month closed: shown over the map with the rest of what it did.
    const air = raidsSince(s, air0, [month, s.turn]);
    const raidNote = air.notes.slice(0, 2).join(' ');
    const sea = seaFightsSince(s, sea0);
    const shore = shellingSince(s, shore0);
    const seaNote = [...sea.notes.slice(0, 2), ...shore.notes.slice(0, 1)].join(' ');
    const taken = s.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = s.reports.slice(r0);
    if (!taken.length && !reps.length) {
      // Nothing was fought for or taken, but armies the player can see may have marched: that is shown too.
      const seen = sight(s);
      const to = [...new Set(s.corps.filter((c) => !c.dead && c.nat === nat && stood.has(c.id) && stood.get(c.id) !== c.reg && (seen[c.reg] > 0 || seen[stood.get(c.id)!] > 0)).map((c) => c.reg))];
      if (air.flights.length) steps.push({ nat, note: [raidNote, seaNote].filter(Boolean).join(' '), taken: [], battles: [], focus: air.focus.length ? air.focus : air.flights.map((f) => f.from), mine: air.mine || sea.mine || shore.mine, raids: air.flights, seas: sea.zones, shelled: shore.regions, view: view() });
      else if (sea.zones.length || shore.regions.length) steps.push({ nat, note: seaNote, taken: [], battles: [], focus: shore.regions, mine: sea.mine || shore.mine, seas: sea.zones, shelled: shore.regions, view: view() });
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
      note: `${s.nations[nat].adj} forces ${said.join(', ')}.${shore.notes.length ? ` ${shore.notes[0]}` : ''}${raidNote ? ` ${raidNote}` : ''}`,
      taken,
      raids: air.flights,
      seas: sea.zones,
      shelled: shore.regions,
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
    // The rules remember the seat: if the country is liberated, the player is given it back.
    if (!playerFell(s)) s.human = null;
    while (s.phase > 0 && !s.human) stepAI(s);
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
  lastMonth = null;
  lastReel = -9;
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
  lastMonth = null;
  lastReel = -9;
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
/** The corps of those picked that could attack a region at all. */
function wouldAttack(s: Game, ids: number[], to: number): Corps[] {
  const able = new Set(candidates(s, s.regions[to]).map((c) => c.id));
  return corpsById(s, ids).filter((c) => able.has(c.id));
}

/**
 * And those of them that the front has room for: only so many corps can attack a province from each side, the
 * strongest first, as the battle itself will take them. (Without this the card would promise better odds than
 * the battle delivers.)
 */
function attackers(s: Game, ids: number[], to: number): Corps[] {
  return limitAttackers(s, wouldAttack(s, ids, to), s.regions[to]);
}

/** A line for the Battle Ticket where the front is too narrow for everybody: how many go in, and who waits. */
function frontLine(s: Game, ids: number[], to: number): { label: string; value: string; effect: string } | null {
  const all = wouldAttack(s, ids, to), going = new Set(attackers(s, ids, to).map((c) => c.id));
  const left = all.filter((c) => !going.has(c.id));
  if (!left.length) return null;
  const cap = frontage(s.regions[to]);
  return {
    label: 'The front', value: `${going.size} of ${all.length} corps go in`,
    effect: `Only ${cap} corps can attack ${s.regions[to].terrain === 'plains' ? 'open country' : s.regions[to].terrain === 'mountain' ? 'mountains' : 'this ground'} from one side, the strongest first. ${listOf(left.slice(0, 3).map((c) => c.name))}${left.length > 3 ? ` and ${left.length - 3} more` : ''} ${left.length === 1 ? 'waits' : 'wait'} behind.`,
  };
}

function preview(ids: number[], to: number) {
  const s = game();
  const corps = attackers(s, ids, to);
  // (Version 55) Enemy troops standing on the ground of a country the player is not at war with cannot be attacked
  // until war is declared on that country: the rules say why, and the game tells the player instead of the odds.
  const foeThere = !!s.human && s.regions[to].units.some((u) => !u.dead && atWar(s, s.human!, u.nat));
  const refused = foeThere ? whyNotAttack(s, s.regions[to]) : '';
  if (refused) return { region: to, refused };
  const a = previewAttack(s, corps, s.regions[to]);
  if (!a) return null;
  // Where the defenders are not all identified, the odds are a band and the ticket leaves out what is not known.
  const look = attackOutlook(s, corps, s.regions[to]);
  // (Version 55) The odds at which it is an even chance: 1.2 to 1 in the open, more for an assault on a town.
  const even = look?.even ?? 1.2;
  // (Version 58: an attack on a line that is dug in and fed needs better odds too, and the rules' own "Prepared
  // defence" line says so: it is not said twice.)
  const saidAlready = [...(look?.lines ?? []), ...a.lines].some((l) => l.label === 'Prepared defence');
  const evenLine = even > 1.21 && !saidAlready ? [{ label: 'An even chance', value: `${even.toFixed(1)} to 1`, effect: 'Luck runs wider in the streets of a town than in the open, and mostly against the attacker: it takes better odds here than in the field to make it an even chance.' }] : [];
  const allies = [...new Set(corps.filter((c) => c.nat !== s.human).map((c) => s.nations[c.nat]?.name ?? c.nat))];
  const named = (c: Corps) => {
    const kn = c.nat !== s.human && s.human && !allied(s, c.nat, s.human) ? knowledgeOf(s, c) : { level: 'exact' as const, age: 0 };
    return kn.level === 'none' ? { id: c.id, nat: c.nat, name: 'A corps not identified', kind: '', divs: 0, men: 0, gen: null }
      : kn.level === 'report' ? { id: c.id, nat: c.nat, name: `${c.name} (a report ${kn.age} month${kn.age === 1 ? '' : 's'} old)`, kind: c.kind, divs: c.divs, men: 0, gen: null }
        : { id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null };
  };
  if (look && !look.exact) {
    return {
      region: to, allies, prize: prizeLine(s, to), attack: look.odds, defence: 1, odds: look.odds,
      exact: false, lo: look.lo, hi: look.hi, chance: look.chance, chanceLo: look.chanceLo, chanceHi: look.chanceHi, note: look.note, even,
      lines: [...look.lines, ...(frontLine(s, ids, to) ? [frontLine(s, ids, to)!] : []), ...evenLine],
      attackers: corps.map((c) => ({ id: c.id, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
      defenders: a.defenders.map(named),
    };
  }
  return {
    allies, exact: true, lo: a.odds, hi: a.odds, chance: look?.chance ?? 0, chanceLo: look?.chance ?? 0, chanceHi: look?.chance ?? 0, note: '', even,
    region: to,
    // What taking the place is worth, in a sentence, for the screen that asks whether to attack.
    prize: prizeLine(s, to),
    attack: a.A,
    defence: a.D,
    odds: a.odds,
    lines: [...a.lines, ...(frontLine(s, ids, to) ? [frontLine(s, ids, to)!] : []), ...evenLine],
    attackers: corps.map((c) => ({ id: c.id, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: a.defenders.map((c) => ({ id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
  };
}

/** What a battle came to, in a sentence. */
function battleNote(s: Game, r: BattleReport): string {
  if (r.yielded) return `The ${s.nations[r.defNat].adj} troops fell back from ${r.regionName} without a fight.`;
  const cost = `${r.attLost.toLocaleString('en')} lost against ${r.defLost.toLocaleString('en')}`;
  const prisoners = r.prisoners > 0 ? `, ${r.prisoners.toLocaleString('en')} prisoners` : '';
  const x = r as BattleReport & { siege?: boolean; town?: boolean; relief?: boolean; sally?: boolean; gates?: boolean; prepared?: number };
  const town = townSiege(s, r.region)?.town ?? r.regionName;
  // (Version 54: the town is the key to its province. Winning the country round it does not take it.)
  // (Version 55: a town held only by its guard may open its gates to the army that has just won the field outside.)
  const came = x.gates ? `the army outside is beaten, and ${town} opens its gates without a fight`
    : x.town ? (r.taken ? `${town} is stormed and taken` : `the assault on ${town} is thrown back`)
    : x.sally ? (r.taken ? `the garrison of ${town} breaks the ring: the siege is raised` : `the garrison's sortie from ${town} is driven back in`)
      : x.relief ? (r.taken ? `the besiegers are driven off and ${town} is relieved` : `the relief of ${town} is held off`)
        : x.siege ? `the country is won, but ${town} holds: it is now besieged`
          // (Version 58: a line that was dug in and fed either held or was broken, and the note says which)
          : (x.prepared ?? 0) >= 0.5 ? (r.taken ? 'the prepared line is broken and the ground is taken' : 'the attack breaks on a prepared line')
            : r.taken ? 'the ground is taken' : 'the attack is held';
  return `${r.level} at ${r.regionName}: ${came}, ${cost}${prisoners}.`;
}

function attack(ids: number[], to: number) {
  const s = game();
  const before = new Map(s.corps.map((c) => [c.id, c.reg]));
  const report = orderAttack(s, attackers(s, ids, to), s.regions[to]);
  // Corps that followed up into the ground they won, for the game to march across.
  const moved = report
    ? s.corps.filter((c) => !c.dead && c.nat === s.human && c.reg === to && before.get(c.id) !== to).map((c) => ({ id: c.id, path: [to] }))
    : [];
  // (where the town still holds, the province has not changed hands: the screen shows no capture)
  const siege = !!(report as (BattleReport & { siege?: boolean }) | null)?.siege;
  return { report: report && siege ? { ...report, taken: false } : report, siege, note: report ? battleNote(s, report) : '', moved, view: view() };
}

function endMonth() {
  const s = game();
  const h0 = s.headlines.length;
  // Who stands as the month ends, to tell afterwards who has been struck from the map and who has come back.
  const stoodUp = new Set(Object.values(s.nations).filter((n) => n.alive && !n.dormant).map((n) => n.id));
  const wasDown = new Set(Object.values(s.nations).filter((n) => !n.alive && !n.dormant).map((n) => n.id));
  const steps: unknown[] = [];
  let fallen: string | null = null;
  const roll0 = s.passings.length, month0 = s.turn;
  const seaStart = lastSeaReport(s);
  // How the player's own campaigns stood, to tell afterwards which were decided this month.
  const me0 = s.human;
  const goals0 = GOAL_KEYS.map((key) => goalState(s, key));
  if (!s.human) {
    // Nobody at the controls (the player's country has fallen): one whole month of the war, every nation on the computer.
    const month = s.turn;
    for (let guard = 0; guard < 80 && s.turn === month && !s.human; guard++) stepOne(s, steps);
    if (s.human) {
      // The country has been liberated: the player leads it again. The others finish their month, and his opens.
      fallenNation = null;
  lastMonth = null;
  lastReel = -9;
      runOthers(s, steps);
      if (humanUp(s)) beginHuman(s);
    }
  } else {
    const air0 = lastAirReport(s), sea00 = lastSeaReport(s), month = s.turn, me = s.human;
    if (onEve(s)) endEve(s);
    else {
      judgeGoals(s, s.human);
      endHuman(s);
    }
    const air = raidsSince(s, air0, [month, s.turn]);
    if (air.flights.length) steps.push({ nat: me, note: air.notes.slice(0, 2).join(' '), taken: [], battles: [], focus: air.focus.length ? air.focus : air.flights.map((f) => f.from), mine: true, raids: air.flights, view: view() });
    const sea = seaFightsSince(s, sea00);
    if (sea.zones.length) steps.push({ nat: me, note: sea.notes.slice(0, 2).join(' '), taken: [], battles: [], focus: [], mine: true, seas: sea.zones, view: view() });
    runOthers(s, steps);
    fallen = arrive(s);
  }
  // Fights at sea that no nation's own move accounts for (they are settled as a month opens): shown last.
  {
    const shown = new Set((steps as { seas?: number[] }[]).flatMap((x) => x.seas ?? []));
    const late = s.seaReports.filter((r) => r.id > seaStart && !shown.has(r.zone));
    if (late.length) {
      const all = seaFightsSince(s, seaStart);
      const zones = [...new Set(late.map((r) => r.zone))];
      const adj = (nat: string): string => s.nations[nat]?.adj ?? nat;
      steps.push({
        nat: late[0].attNat, taken: [], battles: [], focus: [], seas: zones, mine: late.some((r) => r.attNat === s.human || r.defNat === s.human),
        note: late.slice(0, 2).map((r) => `${adj(r.attNat)} and ${adj(r.defNat)} ships fight in the ${r.zoneName}: ${r.won ? `the ${adj(r.attNat)} fleet has the better of it` : `the ${adj(r.defNat)} fleet holds the sea`}.`).join(' ') || all.notes[0] || '',
        view: view(),
      });
    }
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
  const announce: { nat: string; title: string; text: string; good: boolean; kicker?: string; more?: string; facts?: { label: string; value: string }[]; region?: number }[] = [];
  const capitalOfNat = (nat: string): number => s.regions.find((r) => r.cap && r.owner === nat)?.id ?? -1;
  // The rules keep a roll of every nation that leaves the war or comes back into it, in words (the grand campaign).
  const cards = [...new Set([month0, s.turn])].flatMap((t) => announcements(s, t)).filter((c) => c.id > roll0 && c.nat !== fallen);
  for (const c of cards) {
    const foe = s.human ? atWar(s, s.human, c.nat) : false;
    const friend = s.human ? allied(s, s.human, c.nat) || c.nat === s.human : false;
    announce.push({
      nat: c.nat, title: c.title.toUpperCase(), text: c.text, more: c.more, kicker: c.kicker, facts: c.facts, region: c.region,
      good: c.kind === 'freed' ? friend || !foe : foe,
    });
  }
  if (!cards.length) for (const n of Object.values(s.nations)) {
    if (n.id === s.human || n.id === fallen) continue;
    const mine = s.human ? allied(s, s.human, n.id) : false;
    const foe = s.human ? atWar(s, s.human, n.id) : false;
    if (stoodUp.has(n.id) && !n.alive) announce.push({ nat: n.id, title: `${n.name.toUpperCase()} HAS FALLEN`, text: said(n.id) || `${upFirst(theName(s, n.id))} is eliminated from the war.`, good: foe });
    else if (wasDown.has(n.id) && n.alive) announce.push({ nat: n.id, title: `${n.name.toUpperCase()} IS LIBERATED`, text: said(n.id) || `${upFirst(theName(s, n.id))} is free again, and back in the war.`, good: mine || !foe });
  }
  // A campaign of the player's own decided this month: won, or lost.
  // (not when the country itself has fallen: that is told in its own way, and there is no playing on)
  if (me0 && s.nations[me0] && !fallen) {
    GOAL_KEYS.forEach((key, i) => {
      const now = goalState(s, key), goal = GOALS[me0]?.[key];
      if (!goal || goals0[i] !== 'open' || (now !== 'won' && now !== 'lost')) return;
      announce.unshift({
        nat: me0, title: now === 'won' ? `VICTORY: ${goal.title.toUpperCase()}` : `${goal.title.toUpperCase()}: THE CHANCE IS GONE`,
        text: now === 'won' ? `${goal.text} It is done. The ${key === 'short' ? 'short' : 'long'} campaign is won.` : `${goal.text} It was not to be: the ${key === 'short' ? 'short' : 'long'} campaign is lost.`,
        more: now === 'won' ? (key === 'short' ? 'The war goes on, and the long campaign is still to win. You can play on as long as you like.' : 'You can play on as long as you like.') : 'The war goes on, and you can play on as long as you like.',
        kicker: `${key === 'short' ? 'The short campaign' : 'The long campaign'} \u00B7 ${turnToDate(month0).label}`,
        facts: goalChecks(s, me0, goal).slice(0, 3).map((c) => ({ label: c.label, value: c.done ? 'done' : 'not done' })),
        region: capitalOfNat(me0), good: now === 'won',
      });
    });
  }
  // The newsreel: the month's few great stories, each with the place to show while it is told. A quiet month has none.
  const short = (t: string, most = 230): string => (t.length <= most ? t : `${t.slice(0, t.lastIndexOf('.', most) + 1) || `${t.slice(0, most)}...`}`);
  const capitalOf = (nat: string | null): number => (nat ? s.regions.find((r) => r.cap && r.owner === nat)?.id ?? -1 : -1);
  const reel: { nat: string; title: string; text: string; region: number }[] = [];
  for (const a of announce) reel.push({ nat: a.nat, title: a.title, text: short(a.text), region: a.region ?? capitalOf(a.nat) });
  // A declaration of war that touches the player is always news; one between others is told when a reel is due anyway.
  const myName = s.human ? s.nations[s.human]?.name ?? '' : '';
  const touches = (h: { nat: string | null; text: string }): boolean => !!s.human && (h.nat === s.human || (!!myName && h.text.includes(myName)));
  const wars = headlines.filter((x) => x.kind === 'war');
  const always = reel.length > 0 || wars.some(touches);
  for (const h of [...wars.filter(touches), ...wars.filter((x) => !touches(x))].slice(0, 2)) reel.push({ nat: h.nat ?? '', title: 'WAR IS DECLARED', text: short(h.text), region: capitalOf(h.nat) });
  // (the fighting on the ground only: raids are flown every month and are shown as they happen)
  const fights = (steps as { nat: string; note: string; focus?: number[]; mine?: boolean; moved?: boolean; battles?: number[]; taken?: number[]; raids?: unknown[] }[])
    .filter((x) => x.note && !x.moved && ((x.battles?.length ?? 0) > 0 || (x.taken?.length ?? 0) > 0))
    .sort((a, b) => (b.mine ? 100 : 0) + (b.battles?.length ?? 0) + (b.taken?.length ?? 0) - ((a.mine ? 100 : 0) + (a.battles?.length ?? 0) + (a.taken?.length ?? 0)));
  // A newsreel is an occasion: always for a nation's fall or return, a campaign decided or a declaration of war;
  // for hard fighting no more than once in three months.
  const heavy = fights.some((x) => x.mine && (x.taken?.length ?? 0) > 0) || fights.reduce((n, x) => n + (x.taken?.length ?? 0), 0) >= 4;
  const big = always || ((heavy || wars.length > 0) && s.turn - lastReel >= 3);
  if (big) lastReel = s.turn;
  for (const f of fights.slice(0, 2)) {
    const n = s.nations[f.nat];
    reel.push({ nat: f.nat, title: `THE ${(n?.adj ?? f.nat).toUpperCase()} ARMY ON THE MARCH`, text: short(f.note), region: f.focus?.[0] ?? capitalOf(f.nat) });
  }
  const newsreel = big ? { date: turnToDate(month0).label, cards: reel.slice(0, 4) } : null;
  lastMonth = {
    date: turnToDate(month0).label,
    rows: (steps as { nat: string; note: string; focus?: number[]; mine?: boolean; moved?: boolean }[]).filter((x) => x.note && !x.moved).map((x) => ({ nat: x.nat, note: x.note, region: x.focus?.[0] ?? -1, mine: !!x.mine })),
    news: events.map((e) => e.text),
  };
  // A title card as each year opens: the year, and how the player stands as it begins.
  const year = (() => {
    const d = turnToDate(s.turn);
    const me = s.human && s.nations[s.human]?.alive ? s.nations[s.human] : null;
    if (d.month !== 0 || !me) return null;
    const foes = me.atWar.filter((id) => s.nations[id]?.alive);
    const held = s.regions.filter((r) => r.ctrl === me.id).length, own = s.regions.filter((r) => r.owner === me.id).length;
    const divs = s.corps.filter((c) => !c.dead && c.nat === me.id).reduce((n, c) => n + c.divs, 0);
    const lines = [
      foes.length ? `At war with ${listOf(foes.slice(0, 5).map((id) => theName(s, id)))}${foes.length > 5 ? ` and ${foes.length - 5} more` : ''}.` : 'At peace.',
      `${held} province${held === 1 ? '' : 's'} under your flag${held > own ? `, ${held - own} more than your own country` : held < own ? `, ${own - held} of your own in other hands` : ''}. ${divs} division${divs === 1 ? '' : 's'} in the field.`,
    ];
    for (const key of GOAL_KEYS) {
      const goal = GOALS[me.id]?.[key];
      if (!goal || goalState(s, key) !== 'open') continue;
      const checks = goalChecks(s, me.id, goal);
      lines.push(`${key === 'short' ? 'The short campaign' : 'The long campaign'}: ${checks.filter((c) => c.done).length} of ${checks.length} done, ${goalWhen(goal, (t: number) => turnToDate(t).label)}.`);
      break;
    }
    const months = s.turn - 1;
    return { year: String(d.year), title: foes.length ? `${me.name} at war` : `${me.name} at peace`, kicker: months > 0 ? `${months} month${months === 1 ? '' : 's'} since September 1939` : '', lines };
  })();
  return { steps, headlines, events, fallen, announce, newsreel, year, view: view() };
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
    // The chief town of the province, and what the player gets for taking the place (or has from holding it).
    town: (() => { const t = townIn(s, r.id); return t ? { name: t.name, size: t.sizeWord, worth: t.worth, rank: t.rank } : null; })(),
    prize: (() => { const z = prizeOfPlace(s, r.id); return z && prizeLine(s, r.id) ? { kind: z.kind, line: z.line, items: z.items.map((i) => `${i.label}: ${i.text}`) } : null; })(),
    // The town's building plots (the grand campaign): what stands, what is going up, how many are free.
    plots: (() => {
      const t = townBuildings(s, r.id);
      if (!t) return null;
      const lineNo = (id: string | null): number => (id ? ['market', 'farm', 'school', 'camp', 'hospital', 'hall'].indexOf(id) : -1);
      return {
        town: t.town, mine: t.mine, free: t.free, can: t.offers.filter((o) => o.why === null).length,
        list: t.plots.map((p) => (p.line ? `${p.name}: ${p.level > 0 ? p.label : 'going up'}${p.building && p.level > 0 ? ' (next level going up)' : ''}` : 'Free plot')),
        // The same as things to draw: every plot with what stands on it, and every line that could be built or raised.
        size: t.size,
        plots: t.plots.map((p) => ({ line: p.line ?? '', no: lineNo(p.line), name: p.line ? p.name : 'Free plot', label: p.line ? p.label : '', level: p.level, building: p.building, due: p.building && p.due >= 0 ? turnToDate(p.due).label : '', does: p.does })),
        offers: t.offers.map((o) => ({ line: o.line, no: lineNo(o.line), name: o.name, label: o.label, level: o.level, has: o.has, cost: shown(o.cost), months: o.months, effect: o.effect, blurb: o.blurb, why: !humanUp(s) || onEve(s) ? 'It is not your month.' : o.why ?? '' })),
      };
    })(),
    // A fallen country's capital: whether it can come back, and whether the player taking this place would free it.
    fallenNote: fallenCapitalAt(s, r.id)?.note ?? '',
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
    // The town as a fortress (Version 54): what it adds to its defenders, and how its siege stands, in plain lines.
    siege: (() => { try { const v = townSiege(s, r.id); return v ? { town: v.town, by: v.by ? s.nations[v.by]?.name ?? v.by : '', lines: v.lines, inside: v.inside, room: v.room } : null; } catch { return null; } })(),
    // The places of the province that are prizes of their own (Version 56), in plain lines.
    places: placesOf(s, r.id),
    jobs: (() => {
      const out: { key: string; label: string; detail: string; ok: boolean; why: string }[] = [];
      if (!me) return out;
      // A siege's own orders: the assault, the garrison's sortie, and corps into the town or out of it.
      try {
        const v = townSiege(s, r.id);
        if (v) {
          const odds = (o: { exact: boolean; odds: number; lo: number; hi: number; chance: number } | null): string => !o ? ''
            : ` The staff put the odds at ${o.exact ? o.odds.toFixed(1) : `${o.lo.toFixed(1)} to ${o.hi.toFixed(1)}`} to 1, about ${Math.round(o.chance * 100)} chances in a hundred.`;
          if (v.by === me) {
            const why = whyNotStormTown(s, r.id);
            out.push({ key: 'storm', label: `Storm ${v.town}`, detail: `An assault on the town itself, once a month. It costs more men than a fight in the open.${odds(stormOutlook(s, r.id))}`, ok: why === null, why: why ?? '' });
          }
          if (v.holder === me && v.by) {
            const why = whyNotSallyOut(s, r.id);
            out.push({ key: 'sally', label: `Sally out from ${v.town}`, detail: `The garrison comes out against the army round the town: win, and the siege is raised.${odds(sallyOutlook(s, r.id))}`, ok: why === null, why: why ?? '' });
          }
          for (const c of s.corps) {
            if (c.dead || c.nat !== me || c.reg !== r.id || c.kind === 'fort') continue;
            if ((c as { town?: unknown }).town) {
              const why = whyNotLeaveTown(s, c);
              out.push({ key: `field:${c.id}`, label: `${c.name}: take the field`, detail: `It leaves ${v.town} and stands in the open country of the province again.`, ok: why === null, why: why ?? '' });
            } else if (r.ctrl === me || allied(s, r.ctrl, me)) {
              const why = whyNotGarrisonTown(s, c);
              out.push({ key: `garrison:${c.id}`, label: `${c.name}: into ${v.town}`, detail: `It goes into the town as its garrison: it fights much stronger behind the walls, and holds the province while the town holds.`, ok: why === null, why: why ?? '' });
            }
          }
        }
      } catch { /* a campaign from history has no sieges */ }
      // A reconnaissance flight over ground that is not the player's: one press, by the group the staff would send.
      if (!allied(s, r.ctrl, me)) {
        try {
          const plan = weighRaid(s, 'recon', r.id);
          if (plan.picked.length) {
            const group = s.airFleets.find((f) => f.id === plan.picked[0]);
            out.push({ key: `recon:${plan.picked[0]}`, label: 'Send a reconnaissance flight', detail: `${group?.name ?? 'An air group'} photographs ${r.name}: every army here and beside it is seen whole and identified until the month ends. ${plan.effects[0] ?? ''}`.trim(), ok: up && !plan.why, why: plan.why ?? (up ? '' : 'It is not your month.') });
          }
        } catch { /* the air war as its own game is the grand campaign's alone */ }
      }
      // A corps of a sea power standing in a foreign port can be lifted off by sea, as from the beaches of Dunkirk.
      const N = s.nations[me];
      if (N?.seaSupply && r.port && r.owner !== me && N.capital >= 0) {
        for (const c of s.corps) {
          if (c.dead || c.nat !== me || c.reg !== r.id || c.kind === 'fort') continue;
          const open = seaOpenAt(s, me, r);
          const can = up && canOrder(s, c) && open;
          out.push({ key: `evac:${c.id}`, label: `Lift ${c.name} off by sea`, detail: 'Taken off from the port to a haven or home. It leaves a quarter of its strength and its order behind, and can do no more this month.', ok: can, why: can ? '' : !up ? 'It is not your month.' : !open ? 'The enemy holds the sea off this port.' : 'That corps cannot be given orders now.' });
        }
      }
      if (!groundRules(s)) return out;
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
      // (Version 56) Wreck a prize the player's troops hold, and scorched earth for the province and the nation.
      out.push(...placeJobs(s, r.id));
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
        const hasty = (rail as { hasty?: number }).hasty ?? 0;
        lines.push(rail.line ? (rail.cut ? (hasty > 0 ? `The railway here is cut: torn up in haste, the railwaymen will have it running in about ${hasty === 1 ? 'a month' : `${hasty} months`}.` : 'The railway here is cut: it stays cut until engineers relay it.') : 'A railway runs through here.')
          : rail.road > 0 ? `No railway: ${rail.road} province${rail.road === 1 ? '' : 's'} of road from the railhead.` : 'No railway.');
        if (rail.ruined > 0) lines.push(`${rail.ruined} of its ${rail.factories} factories lie in ruins from bombing.`);
      }
      return lines;
    })(),
  };
}

// ---- Prizes outside the town, and scorched earth (Version 56) ----------------------------------
const WRECK_WORD: Record<PrizeKind, string> = { oil: 'Fire the wells', works: 'Blow up the works', airfield: 'Plough up the airfield', harbour: 'Block the harbour', junction: 'Blow the junction' };
const WRECKED_WORD: Record<PrizeKind, string> = { oil: 'The wells are fired.', works: 'The works are blown up.', airfield: 'The airfield is ploughed up.', harbour: 'The harbour is blocked.', junction: 'The junction is blown.' };
const POLICY_WORD: Record<ScorchPolicy, string> = { never: 'never', conquered: 'on conquered ground', everywhere: 'everywhere' };
const wreckTip = (k: PrizeKind): string => (k === 'junction'
  ? 'One corps spends its month on it. No train runs through for either side until engineers relay the line.'
  : `One corps spends its month on it, engineers for choice. Nobody has the use of it for ${PRIZES.wreck[k]} months, you included.`);

/**
 * The places of a province that are prizes of their own: its oilfield, factory district, airfield, harbour and
 * great rail junction, each with who holds it and how it stands, and what the player's troops do if they are
 * driven off. In plain lines for the province panel; null where there are none (and outside the grand campaign).
 */
function placesOf(s: Game, region: number): { head: string; lines: string[]; seized: boolean; burns: boolean } | null {
  let v: ReturnType<typeof prizesOf> = null;
  try { v = prizesOf(s, region); } catch { return null; }
  if (!v || !v.prizes.length) return null;
  const r = s.regions[region];
  const me = s.human;
  const town = townSiege(s, region)?.town ?? r.name;
  const under = v.prizes.some((p) => p.holder !== r.ctrl);
  const lines: string[] = [];
  if (under) lines.push(`The army round ${town} holds what lies outside its walls, and has the use of it after a month.`);
  for (const p of v.prizes) {
    const held = p.holder !== r.ctrl ? ` Held by ${s.nations[p.holder]?.name ?? p.holder}.` : '';
    // (where the detail already says which side of the walls it lies, it is not said twice)
    const where = /inside|outside/.test(p.detail) ? '' : p.inside ? ' (inside the defences)' : ' (outside the walls)';
    lines.push(`${p.name}${where}: ${p.detail}. ${p.state}${held}`);
  }
  const mine = !!me && s.nations[me].alive && (r.ctrl === me || v.prizes.some((p) => p.holder === me));
  if (mine) {
    const odds = Math.round(PRIZES.burn.chance * 100);
    lines.push(`If your troops are driven off ${r.name}: ${v.burns
      ? `they burn what they hold as they go (each place about ${odds} times in 100; a burned place is out of use for ${PRIZES.burn.months.airfield} to ${PRIZES.burn.months.oil} months, whoever holds it).`
      : 'they leave everything whole, for the enemy to use.'} ${v.order ? 'That is your order for this province.' : `That follows your scorched earth policy: ${POLICY_WORD[v.policy]}.`}`);
  }
  return { head: 'PLACES OF THEIR OWN', lines, seized: under, burns: v.burns };
}

/** The orders a province offers over its prizes: wreck one the player's troops hold, and scorched earth. */
function placeJobs(s: Game, region: number): { key: string; label: string; detail: string; ok: boolean; why: string }[] {
  const out: { key: string; label: string; detail: string; ok: boolean; why: string }[] = [];
  const me = s.human;
  if (!me || !s.nations[me]?.alive) return out;
  let v: ReturnType<typeof prizesOf> = null;
  try { v = prizesOf(s, region); } catch { return out; }
  if (!v) return out;
  const r = s.regions[region];
  const up = humanUp(s) && !onEve(s);
  for (const p of v.prizes) {
    if (p.holder !== me || p.out > 0) continue;
    const why = up ? whyNotWreckPrize(s, region, p.kind) : 'It is not your month.';
    out.push({ key: `wreck:${p.kind}`, label: `${WRECK_WORD[p.kind]}`, detail: `${p.name}. ${wreckTip(p.kind)}`, ok: why === null, why: why ?? '' });
  }
  if (r.ctrl === me || v.prizes.some((p) => p.holder === me)) {
    const why = up ? whyNotBurnOrder(s, region) : 'It is not your month.';
    // (only the orders that would change something are offered: the one in force is told in the panel's own lines)
    const seg = (val: number, label: string, detail: string): void => { if (v!.order !== val) out.push({ key: `burn:${val}`, label, detail, ok: why === null, why: why ?? '' }); };
    seg(1, `If ${r.name} is lost: burn it`, 'Scorched earth here, whatever your policy says.');
    seg(-1, `If ${r.name} is lost: leave it whole`, 'Nothing is burned here, whatever your policy says.');
    seg(0, `If ${r.name} is lost: follow the policy`, `This province follows your scorched earth policy again: ${POLICY_WORD[v.policy]}.`);
    const pol = (pp: ScorchPolicy, label: string, detail: string): void => { if (v!.policy !== pp) out.push({ key: `scorch:${pp}`, label, detail, ok: up, why: up ? '' : 'It is not your month.' }); };
    pol('never', 'Scorched earth policy: never', 'For every province without an order of its own: your troops burn nothing they leave.');
    pol('conquered', 'Scorched earth policy: conquered ground', 'For every province without an order of its own: your troops burn what they leave on ground that is not your own or an ally\'s.');
    pol('everywhere', 'Scorched earth policy: everywhere', 'For every province without an order of its own: your troops burn what they leave, at home too.');
  }
  return out;
}

/** The wreck order the player has been asked to confirm: province, kind and month. */
let wreckAsked = '';

/** Carries out one of the works a province offers (see province().jobs). */
function work(key: string, region: number) {
  const s = game();
  const [what, a, b] = key.split(':');
  let ok = false;
  let note = 'That cannot be done now.';
  const flies: Flight[] = [];
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
  } else if (what === 'evac') {
    const c = s.corps.find((x) => x.id === Number(a));
    const h0 = s.headlines.length;
    ok = !!c && orderEvacuate(s, c);
    if (ok && c) note = s.headlines.slice(h0).map((h) => h.text).join(' ') || `${c.name} is lifted off by sea to ${s.regions[c.reg].name}.`;
  } else if (what === 'storm' || what === 'sally') {
    const why = what === 'storm' ? whyNotStormTown(s, region) : whyNotSallyOut(s, region);
    const report = why ? null : what === 'storm' ? orderStorm(s, region) : orderSally(s, region);
    ok = !!report;
    note = report ? battleNote(s, report) : why ?? note;
  } else if (what === 'garrison' || what === 'field') {
    const c = s.corps.find((x) => x.id === Number(a));
    const why = !c ? 'That corps is not there.' : what === 'garrison' ? whyNotGarrisonTown(s, c) : whyNotLeaveTown(s, c);
    ok = !!c && !why && (what === 'garrison' ? orderGarrison(s, c) : orderTakeField(s, c));
    const town = townSiege(s, region)?.town ?? s.regions[region]?.name ?? 'the town';
    note = ok && c ? (what === 'garrison' ? `${c.name} goes into ${town} as its garrison.` : `${c.name} leaves ${town} and takes the field.`) : why ?? note;
  } else if (what === 'wreck') {
    const kind = a as PrizeKind;
    const why = whyNotWreckPrize(s, region, kind);
    // (it cannot be undone, so it is asked twice: the first press says what it will cost, the second does it)
    const asking = `${region}:${kind}:${s.turn}`;
    if (!why && wreckAsked !== asking) {
      wreckAsked = asking;
      const place = prizesOf(s, region)?.prizes.find((x) => x.kind === kind)?.name ?? 'It';
      return { ok: false, note: `${WRECK_WORD[kind]} at ${s.regions[region]?.name ?? 'this place'}? ${place} cannot be un-wrecked: ${kind === 'junction' ? 'no train runs through, for either side, until engineers relay the line' : `nobody has the use of it for ${PRIZES.wreck[kind as Exclude<PrizeKind, 'junction'>]} months, you included`}. Press it again to give the order.`, flies, view: view() };
    }
    wreckAsked = '';
    ok = !why && orderWreck(s, region, kind);
    note = ok ? `${WRECKED_WORD[kind] ?? 'It is wrecked.'} ${kind === 'junction' ? 'No train runs through until engineers relay the line.' : `Nobody has the use of it for ${PRIZES.wreck[kind as Exclude<PrizeKind, 'junction'>]} months.`}` : why ?? note;
  } else if (what === 'burn') {
    const why = whyNotBurnOrder(s, region);
    ok = !why && orderBurn(s, region, Number(a));
    const name = s.regions[region]?.name ?? 'the province';
    note = ok ? (Number(a) === 1 ? `If ${name} is lost, your troops burn what they hold as they go.` : Number(a) === -1 ? `If ${name} is lost, your troops leave it whole.` : `${name} follows your scorched earth policy again.`) : why ?? note;
  } else if (what === 'scorch') {
    ok = orderScorchPolicy(s, a as ScorchPolicy);
    if (ok) note = `Scorched earth policy: ${POLICY_WORD[a as ScorchPolicy] ?? a}. A province's own order still overrides it.`;
  } else if (what === 'recon') {
    const group = s.airFleets.find((f) => f.id === Number(a));
    const flown = sendScout(s, Number(a), region);
    ok = flown.ok;
    note = flown.text || (ok ? 'The flight is back with its photographs.' : 'No flight can be sent there now.');
    // The flight is seen to go out and come back: a few fighters from the group's own airfield.
    if (ok && group && s.regions[region]) flies.push({ from: group.base, x: s.regions[region].x, y: s.regions[region].y, nat: group.nat, bombers: false, escort: false, region: -1, aa: 0, meet: '', meetFrom: -1, loss: 0 });
  }
  return { ok, note, flies, view: view() };
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
    const seen = fleetsInSight(s).find((x) => x.id === id);
    if (!seen) return null;
    if (seen.known !== 'exact') {
      const where = seen.atSea ? `In the ${s.seas[f.zone]?.name ?? 'sea'}` : 'In harbour';
      return { name: seen.name, nat: f.nat, mine: false, lines: [`${seen.size}, not identified. ${where}.`, 'A report and no more: its ships are not counted. Search this sea from the air, or with a carrier\'s aircraft, to find out what it is.'], options: [] };
    }
    const parts: string[] = [];
    const count = (n: number, one: string, many: string) => { if (n > 0) parts.push(`${n} ${n === 1 ? one : many}`); };
    count(f.ships.bb, 'battleship', 'battleships');
    count(f.ships.cv, 'carrier', 'carriers');
    count(f.ships.ca, 'cruiser', 'cruisers');
    count(f.ships.dd, 'destroyer', 'destroyers');
    count(f.ships.ss, 'submarine', 'submarines');
    const sea = s.seas[f.zone]?.name ?? 'the sea';
    const doing = `${fleetView(s, f.id)?.doing ?? (f.stance === 'port' ? `In harbour at ${f.port}` : f.stance === 'raid' ? `Raiding shipping in the ${sea}` : `On patrol in the ${sea}`)}.`;
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
  else if (what === 'fleetbase') ok = orderRebase(s, Number(id), Number(a));
  else if (what === 'air') ok = orderAir(s, Number(id), a as AirMission, Number(b));
  else if (what === 'airbase') ok = orderAirBase(s, Number(id), Number(a));
  return { ok, note: ok ? 'The order is given.' : 'That order cannot be given now.', view: view() };
}

/** Every province's chief town with what the province pays a month and how that ranks (0 to 5), for the map's labels and its wealth view. */
function townsList() {
  const s = game();
  return { towns: townsOfMap(s).map((t) => ({ region: t.region, name: t.name, province: t.province, size: t.size, worth: t.worth, rank: t.rank, signs: t.signs })) };
}

/** Asks every ally's corps in a province that can to march with the player's army there. */
function joinArmyAt(region: number) {
  const s = game();
  const n = orderJoinAll(s, region);
  const why = n ? '' : joinOffers(s, region).map((o) => o.why).find(Boolean) ?? 'No ally\'s corps here can join.';
  return { ok: n > 0, note: n ? `${n} allied corps ${n === 1 ? 'marches' : 'march'} with your army: ${n === 1 ? 'it takes' : 'they take'} your orders, and ${n === 1 ? 'goes' : 'go'} home if left a month without your troops beside ${n === 1 ? 'it' : 'them'}.` : why, view: view() };
}

/** Sends an ally's corps that marches with the player's army back to its own command. */
function sendBack(id: number) {
  const s = game();
  const c = s.corps.find((x) => x.id === id);
  const ok = !!c && orderSendBack(s, c);
  return { ok, note: ok && c ? `${c.name} goes back to ${s.nations[c.nat]?.adj ?? 'its own'} command.` : 'It cannot be sent back now.', view: view() };
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
  if (lastMonth) out.push({ id: 'reports', label: "THE MONTH'S REPORT" });
  if (s.stories.awards.some((a) => a.nat === s.human && !a.old)) out.push({ id: 'heroes', label: 'HEROES AT HOME' });
  if (secretService(s)) out.push({ id: 'spies', label: 'THE SECRET SERVICE' });
  // The government and its war cabinet (Version 57, the grand campaign).
  try { if (governmentOf(s)?.mine) out.push({ id: 'cabinet', label: 'THE CABINET' }); } catch { /* no government in this game */ }
  // Towns to build up and trade agreements (the grand campaign).
  if (tradeLedger(s)) out.push({ id: 'trade', label: 'TRADE AGREEMENTS' });
  if (s.nations[s.human] && townBuildings(s, s.nations[s.human].capital)) out.push({ id: 'towns', label: 'TOWNS TO BUILD UP' });
  const act = lastAct(s);
  if (act && (act.decided || act.on)) out.push({ id: 'lastact', label: act.on ? 'THE WORLD STANDINGS' : 'THE LAST ACT' });
  return out;
}

/**
 * What is still worth doing before the month is ended, besides moving the armies: each a line for a chip under the
 * END THE MONTH button, the screen that deals with it, and a word of explanation. Nothing here has to be done.
 */
function todoOf(s: Game): { k: string; t: string; tip: string }[] {
  const out: { k: string; t: string; tip: string }[] = [];
  const me = s.human && s.nations[s.human]?.alive ? s.nations[s.human] : null;
  if (!me || !humanUp(s) || onEve(s)) return out;
  // A general waiting for a command while a corps he may take has nobody.
  const open = vacant(s, me.id).filter((c) => !c.dead);
  const free = open.length ? waiting(s, me.id).filter((x) => open.some((c) => !whyNotAssign(s, x, c))) : [];
  if (free.length) {
    out.push({
      k: 'generals', t: free.length === 1 ? 'A GENERAL AWAITS A COMMAND' : `${free.length} GENERALS AWAIT A COMMAND`,
      tip: `${free.length === 1 ? 'A general is' : `${free.length} generals are`} waiting for a command, and ${open.length === 1 ? 'a corps has' : `${open.length} corps have`} nobody. A corps with no general fights by the book. Open GENERALS.`,
    });
  }
  // Design teams with nothing to do: they never choose their own work.
  const idle = idleTeams(s, me.id);
  if (idle > 0) {
    out.push({
      k: 'research', t: idle === 1 ? 'A DESIGN TEAM IS IDLE' : `${idle} DESIGN TEAMS ARE IDLE`,
      tip: 'A design team with nothing to do designs nothing. Open RESEARCH and give it work.',
    });
  }
  // The cabinet (Version 57): a directive newly set or nearly due, or a cabinet about to fall.
  try {
    const gov = governmentOf(s);
    if (gov?.mine) {
      const d = gov.directive;
      if (gov.low > 0 && gov.trust <= gov.fallsAt) {
        out.push({ k: 'page:cabinet', t: 'THE CABINET IS ABOUT TO FALL', tip: `The cabinet's trust is ${Math.round(gov.trust)} of 100. At ${gov.fallsAt} or under for ${gov.crisisMonths} months running, it falls; it has been there for ${gov.low}. Open THE CABINET.` });
      } else if (d && d.issued === s.turn) {
        out.push({ k: 'page:cabinet', t: 'A NEW DIRECTIVE', tip: `${d.by}: ${d.title}. ${d.text} Open THE CABINET.` });
      } else if (d && d.left <= 2) {
        out.push({ k: 'page:cabinet', t: d.left <= 1 ? 'A DIRECTIVE IS DUE THIS MONTH' : 'A DIRECTIVE IS DUE NEXT MONTH', tip: `${d.title}. ${d.progress} Open THE CABINET.` });
      }
    }
  } catch { /* no government in this game */ }
  // Sieges: a town of the player's shut in, and towns his armies stand round.
  try {
    const all = siegesInSight(s).filter((v) => v.by);
    const mine = all.filter((v) => v.holder === me.id), theirs = all.filter((v) => v.by === me.id);
    if (mine.length) out.push({ k: `goto:${mine[0].region}`, t: mine.length === 1 ? `${mine[0].town.toUpperCase()} IS BESIEGED` : `${mine.length} OF YOUR TOWNS ARE BESIEGED`, tip: `${mine.map((v) => v.town).slice(0, 4).join(', ')}: ${mine[0].lines[mine[0].lines.length - 1]} Attack the army round it to raise the siege, or let the garrison sally out.` });
    if (theirs.length) out.push({ k: `goto:${theirs[0].region}`, t: theirs.length === 1 ? `YOU BESIEGE ${theirs[0].town.toUpperCase()}` : `YOU BESIEGE ${theirs.length} TOWNS`, tip: `${theirs.map((v) => v.town).slice(0, 4).join(', ')}. Starve it out, or storm it: pick the province and look under ORDERS.` });
  } catch { /* a campaign from history has no sieges */ }
  // Trade: a route cut, or a trading house idle with a country ready to sign.
  const book = tradeLedger(s);
  if (book) {
    const cut = book.deals.filter((d) => d.state === 'cut').length;
    if (cut) out.push({ k: 'page:trade', t: cut === 1 ? 'A TRADE ROUTE IS CUT' : `${cut} TRADE ROUTES ARE CUT`, tip: 'An agreement pays nothing while its route is cut. Open TRADE AGREEMENTS to see which, and why.' });
    else if (book.houses > book.used && book.pc >= book.cost && book.offers.some((o) => o.why === null)) {
      out.push({ k: 'page:trade', t: 'A TRADING HOUSE STANDS IDLE', tip: 'A country is ready to sign a trade agreement, and you have a trading house free for it. It would pay into the treasury every month.' });
    }
  }
  // Nothing on order at all, with money in the treasury for something the arsenal offers.
  if (me.queue.length === 0 && arsenalOffers(s, -1).some((o) => !o.why)) {
    out.push({
      k: 'arsenal', t: 'NOTHING IS ON ORDER',
      tip: `Nothing is being raised or built, and there is ${inHand(me.pp).toLocaleString('en-US')} in the treasury. Open the ARSENAL to order corps, aircraft, ships or works.`,
    });
  }
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

/** What the last month brought, kept so that it can be read again after it has been watched: each nation's doings and where. */
/** The month the last newsreel was shown in, so that they do not come too often. */
let lastReel = -9;

let lastMonth: { date: string; rows: { nat: string; note: string; region: number; mine: boolean }[]; news: string[] } | null = null;

/** The month's report: what every nation did while the player waited, each with a way to go and look. */
function reportsPage(s: Game): Page | null {
  if (!lastMonth) return null;
  const row = (r: { nat: string; note: string; region: number }): PageRow => ({
    nat: r.nat, title: s.nations[r.nat]?.name ?? r.nat, lines: [r.note],
    buttons: r.region >= 0 ? [{ label: `GO TO ${s.regions[r.region].name.toUpperCase()}`, act: `goto:region:${r.region}`, ok: true, tip: 'Looks at the place on the map.' }] : [],
  });
  const mine = lastMonth.rows.filter((r) => r.mine).map(row), rest = lastMonth.rows.filter((r) => !r.mine).map(row);
  return {
    id: 'reports', title: `THE MONTH'S REPORT  \u00B7  ${lastMonth.date.toUpperCase()}`,
    headline: lastMonth.rows.length ? `${mine.length ? `${mine.length} report${mine.length === 1 ? '' : 's'} from your own fronts` : 'Nothing touched your own fronts'}, ${rest.length} from elsewhere` : 'A quiet month',
    note: 'What every nation did while your month was closed, as it was shown on the table. Your own fronts come first.',
    top: [],
    sections: [
      { heading: 'YOUR FRONTS', text: mine.length ? '' : 'No enemy moved against you, and none of your ground changed hands.', rows: mine },
      { heading: 'ELSEWHERE', text: rest.length ? '' : 'Nothing was fought for elsewhere.', rows: rest },
      { heading: 'THE NEWS', text: lastMonth.news.join('\n'), rows: [] },
    ].filter((x) => x.rows.length || x.text),
  };
}

/** The honours the player's units have won and what can be made of each at home: a bond tour, or instructors for the schools. */
function heroesPage(s: Game): Page | null {
  const me = s.human;
  if (!me) return null;
  const up = humanUp(s) && !onEve(s);
  const mine = s.stories.awards.filter((a) => a.nat === me && !a.old);
  // To open the room is to have read the citations.
  for (const a of mine) orderSeen(s, a.id);
  const rows: PageRow[] = mine.slice(-30).reverse().map((a) => {
    const fame = fameOf(a);
    const button = (how: 'bonds' | 'school', label: string, what: string): PageButton => {
      const why = whyNotCash(s, a, how);
      return { label, act: `cash:${a.id}:${how}`, ok: up && !why, tip: why || what };
    };
    return {
      nat: me, title: `${awardTitle(s, a)}  \u00B7  ${a.name}`,
      lines: [citation(s, a), a.cash ? (a.cash === 'bonds' ? 'Its heroes have been home on a bond tour.' : 'The men who earned it are instructors at the schools.') : `Worth ${fame} at home. Until it is cashed in, the honour gives its unit an edge at the front.`],
      buttons: a.cash ? [] : [
        button('bonds', 'A BOND TOUR', `Its heroes go home to sell war bonds: money for the treasury and political capital, ${fame} time${fame === 1 ? '' : 's'} over. The honour stops giving its edge at the front, and a corps is out of the line for two months.`),
        button('school', 'INSTRUCTORS', 'The men who earned it go to the schools: every later draft reaches the front better trained. The honour stops giving its edge at the front, and a corps is out of the line for two months.'),
      ],
    };
  });
  const open = mine.filter((a) => !a.cash).length;
  return {
    id: 'heroes', title: 'HEROES AT HOME', headline: mine.length ? `${mine.length} honour${mine.length === 1 ? '' : 's'} won, ${open} not yet cashed in` : 'No honours have been won yet',
    note: 'An honour gives its unit an edge at the front. It can be cashed in once instead: a bond tour brings money and political capital; instructors make every later draft better trained.',
    top: [], sections: [{ heading: 'THE HONOURS', text: mine.length ? '' : 'Honours are won in battle: for ground taken against the odds, for standing fast, for a raid pressed home.', rows }],
  };
}

// ---- The navy as pieces (the grand campaign) --------------------------------------------------------------

const SHIP_WORD: Record<string, [string, string]> = { bb: ['battleship', 'battleships'], cv: ['carrier', 'carriers'], ca: ['cruiser', 'cruisers'], dd: ['destroyer', 'destroyers'], ss: ['submarine', 'submarines'] };

/** One of the player's fleets in hand: its card, and everything it could be told to do this month. Null where the rules keep no such navy. */
function fleetInHand(id: number) {
  const s = game();
  try {
    const card = fleetView(s, id);
    if (!card || card.nat !== s.human) return null;
    // (the navy as pieces is the grand campaign's: a historical scenario keeps its fleets to patrol, raid and harbour)
    const grand = s.flags.open === 1;
    const o = grand ? fleetOptions(s, id) : null;
    return {
      ok: true, grand, id, name: card.name, admiral: card.admiral ? `${card.admiral.name}, level ${card.admiral.level}` : '', doing: card.doing, order: card.order, stance: card.stance,
      ready: card.ready, guns: Math.round(card.guns), screen: card.screen.gap > 0 ? `${cap1(card.screen.word)}: ${card.screen.have} destroyers where ${card.screen.need} are needed.` : '', shelled: card.shelled,
      cards: card.cards.map((c) => ({ cls: c.cls, label: c.label, count: c.count, lost: c.lost, role: c.role, brief: c.brief })),
      seas: (o?.seas ?? []).map((z) => ({ zone: z.zone, name: z.name, surface: z.surface, subs: z.subs, meet: z.meet, hunt: z.hunt, torpedo: z.torpedo, reported: z.reported ?? 0, search: z.search ?? '' })),
      coasts: o?.coasts ?? [], merge: o?.merge ?? [], split: !!o?.split, canOrder: !!o,
      wings: wingsOfFleet(s, id).map((w) => ({ id: w.id, label: w.label, aircraft: Math.round(w.aircraft), doing: w.doing, flew: w.flew })),
    };
  } catch {
    return null;
  }
}
const cap1 = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);

/** A fleet's orders, all in one room: what it is, the seas in its reach and what it would meet, lanes to escort, coasts to shell, splitting and joining. */
function fleetPage(s: Game, id: number): Page | null {
  const card = s.flags.open === 1 ? fleetView(s, id) : null;
  if (!card || card.nat !== s.human) return null;
  const o = fleetOptions(s, id);
  const up = !!o;
  const share = (x: number): string => `${Math.round(x * 100)}%`;
  const ships: PageRow[] = card.cards.map((c) => {
    const buttons: PageButton[] = [];
    if (o?.split) {
      const all = { [c.cls]: c.count } as Record<string, number>;
      const whyAll = whyNotSplit(s, id, all);
      buttons.push({ label: c.count === 1 ? 'SPLIT IT OFF' : 'SPLIT THEM ALL OFF', act: `fleetsplit:${id}:${c.cls}:${c.count}`, ok: up && !whyAll, tip: whyAll ?? 'They leave as a squadron of their own, in the same sea or harbour, which can be given its own orders.' });
      if (c.count >= 2) {
        const half = Math.floor(c.count / 2);
        const whyHalf = whyNotSplit(s, id, { [c.cls]: half } as Record<string, number>);
        buttons.push({ label: `SPLIT OFF ${half}`, act: `fleetsplit:${id}:${c.cls}:${half}`, ok: up && !whyHalf, tip: whyHalf ?? 'Half of them leave as a squadron of their own.' });
      }
    }
    return { nat: card.nat, title: `${c.count} ${SHIP_WORD[c.cls]?.[c.count === 1 ? 0 : 1] ?? c.label.toLowerCase()}`, lines: [c.role, c.lost ? `${c.lost} lost since the war began.` : ''].filter(Boolean), buttons };
  });
  const seas: PageRow[] = (o?.seas ?? []).map((z) => {
    const meet = z.meet === 'clear' ? 'A patrol would meet no enemy fleet.' : z.meet === 'battle' ? 'A patrol would bring the enemy fleet to battle.' : 'The enemy there is too strong: a patrol would not face him.';
    const enemy = z.surface || z.subs ? `Enemy there: ${[z.surface ? `${z.surface} surface warship${z.surface === 1 ? '' : 's'}` : '', z.subs ? `${z.subs} submarine${z.subs === 1 ? '' : 's'}` : ''].filter(Boolean).join(' and ')}.` : 'No enemy ships are known there.';
    const hunt = z.subs ? `Hunting would sink about ${share(z.hunt)} of the submarines in a month${z.huntNow > 0 ? ` (${share(z.huntNow)} as things stand)` : ''}.` : '';
    const risk = z.torpedo >= 0.005 ? `Chance of losing a heavy ship to a torpedo: ${share(z.torpedo)}.` : '';
    const at = (st: string): boolean => z.here && card.stance === st;
    return {
      nat: '', title: `${z.name}${z.here ? '  \u00B7  it is here' : ''}`, lines: [`${enemy} ${meet}`, [hunt, risk].filter(Boolean).join(' ')].filter(Boolean),
      buttons: [
        { label: 'PATROL', act: `fleet:${id}:${z.zone}:patrol`, ok: up && !at('patrol'), tip: 'Seeks out the enemy\'s warships there and guards the convoys that cross it.' },
        { label: 'RAID SHIPPING', act: `fleet:${id}:${z.zone}:raid`, ok: up && !at('raid'), tip: 'Goes for merchant ships and keeps clear of a fight with the enemy fleet.' },
        { label: 'HUNT SUBMARINES', act: `fleet:${id}:${z.zone}:hunt`, ok: up && !at('hunt') && card.hunters > 0, tip: card.hunters > 0 ? 'Its destroyers sweep the sea for submarines: the more destroyers, the more it sinks.' : 'It has no destroyers to hunt with.' },
      ],
    };
  });
  const lanes: PageRow[] = (o?.lanes ?? []).map((l) => ({
    nat: l.to, title: l.name,
    lines: [l.why ?? `Cargoes lost on it now: ${share(l.now)}. With this fleet as its escort: ${share(l.then)}.`, l.bound ? 'This fleet is its escort now.' : ''].filter(Boolean),
    buttons: [{ label: l.bound ? 'ITS ESCORT NOW' : 'ESCORT THIS LANE', act: `fleetescort:${id}:${l.lane}`, ok: up && !l.why && !l.bound, tip: l.why ?? 'The fleet is bound to the lane and sails with its convoys: fewer cargoes are sunk, and it fights only for them.' }],
  }));
  const coasts: PageRow[] = (o?.coasts ?? []).map((r) => {
    const t = previewBombard(s, id, r, 'troops'), c = previewBombard(s, id, r, 'city');
    const lines: string[] = [];
    if (t?.can.troops) lines.push(`The troops there (${t.corps} corps, ${t.divs} division${t.divs === 1 ? '' : 's'}) would lose about ${share(t.cut)} of their order.`);
    if (c?.can.city) lines.push(`Its city would lose about ${share(c.output)} of its output for a month.`);
    const reply = t ?? c;
    if (reply && reply.fort > 0) lines.push(`A fortress of level ${reply.fort} answers: it is likely to sink about ${share(reply.reply)} of the cruisers and destroyers.`);
    return {
      nat: s.regions[r].ctrl, title: s.regions[r].name, lines,
      buttons: [
        { label: 'SHELL THE TROOPS', act: `fleetshell:${id}:${r}:troops`, ok: up && !!t?.can.troops && !t.why, tip: t?.why ?? 'The guns fall on the troops in the open: they fight the weaker for it until they have rested.' },
        { label: 'SHELL THE CITY', act: `fleetshell:${id}:${r}:city`, ok: up && !!c?.can.city && !c.why, tip: c?.why ?? 'The guns fall on its docks and works: part of its output is stopped for a month.' },
      ],
    };
  });
  const joins: PageRow[] = (o?.merge ?? []).map((other) => {
    const f = fleetView(s, other);
    return { nat: card.nat, title: f?.name ?? 'A fleet', lines: [f ? `${f.ships} ships. ${f.doing}.` : ''], buttons: [{ label: 'JOIN IT', act: `fleetmerge:${id}:${other}`, ok: up, tip: 'The two become one fleet under the other\'s flag and orders.' }] };
  });
  const gap = card.screen.gap > 0 ? ` ${cap1(card.screen.word)}: it has ${card.screen.have} destroyers where its heavy ships need ${card.screen.need}.` : '';
  return {
    id: `fleet:${id}`, title: card.name.toUpperCase(), headline: `${card.doing}`,
    note: `${card.admiral ? `${card.admiral.name} commands (level ${card.admiral.level}). ` : ''}${card.ships} ships, readiness ${share(card.ready)}.${gap}${card.shelled ? ' Its guns have spoken this month.' : ''} A fleet reaches three seas from its base, and can be given one order a month.`,
    top: [
      { label: 'RETURN TO HARBOUR', act: `fleet:${id}:${s.fleets.find((f) => f.id === id)?.home ?? card.zone}:port`, ok: up && card.stance !== 'port', tip: 'Safe from the enemy fleet, and takes no part at sea.' },
      ...(wingsOfFleet(s, id).length ? [{ label: 'CARRIER AIRCRAFT', act: `open:wings:${id}`, ok: true, tip: 'The fighters and strike aircraft its carriers bring: search a sea for the enemy, strike a fleet, or raid a harbour.' }] : []),
    ],
    sections: [
      { heading: 'ITS SHIPS', text: '', rows: ships },
      { heading: 'SEAS IN ITS REACH', text: seas.length ? '' : up ? 'The country is at peace: the fleet keeps its station.' : 'It is not your month.', rows: seas },
      { heading: 'LANES TO ESCORT', text: '', rows: lanes },
      { heading: 'COASTS TO SHELL', text: '', rows: coasts },
      { heading: 'FLEETS TO JOIN', text: '', rows: joins },
    ].filter((x) => x.rows.length || x.text),
  };
}

/** The question before a coast is shelled: what the guns would do to the troops there, or to the city, and what the shore would answer. */
function shellPage(s: Game, id: number, region: number): Page | null {
  const card = s.flags.open === 1 ? fleetView(s, id) : null;
  const r = s.regions[region];
  if (!card || card.nat !== s.human || !r) return null;
  const share = (x: number): string => `${Math.round(x * 100)}%`;
  const rows: PageRow[] = [];
  for (const what of ['troops', 'city'] as const) {
    const plan = previewBombard(s, id, region, what);
    if (!plan || !plan.can[what]) continue;
    const does = what === 'troops' ? `The troops there (${plan.corps} corps, ${plan.divs} division${plan.divs === 1 ? '' : 's'}) would lose about ${share(plan.cut)} of their order, and fight the weaker for it until they have rested.`
      : `About ${share(plan.output)} of ${r.name}'s output would be stopped for a month: ${(plan.nation * 100).toFixed(1)}% of ${s.nations[r.ctrl]?.adj ?? 'the enemy\'s'} production.`;
    rows.push({
      nat: r.ctrl, title: what === 'troops' ? 'The troops on the coast' : 'The city and its works',
      lines: [does, plan.fort > 0 ? `A fortress of level ${plan.fort} answers: it is likely to sink about ${share(plan.reply)} of the cruisers and destroyers that stand in.` : 'No fortress answers from the shore.', ...plan.lines.map((l) => `${l.label}: ${l.value}${l.effect ? ` (${l.effect})` : ''}`)],
      buttons: [{ label: what === 'troops' ? 'SHELL THE TROOPS' : 'SHELL THE CITY', act: `fleetshell:${id}:${region}:${what}`, ok: !plan.why, tip: plan.why ?? 'The fleet stands in and opens fire. Its guns speak once a month.' }],
    });
  }
  return {
    id: `shell:${id}:${region}`, title: `SHELL ${r.name.toUpperCase()}?`, headline: `${card.name}, with guns of weight ${Math.round(card.guns)}`,
    note: 'A fleet with heavy guns can stand in to an enemy coast and shell it: the troops in the open there, or the docks and works of its city. Battleships and cruisers do the work; a fortress on the coast shoots back.',
    top: [{ label: 'THE FLEET\'S OTHER ORDERS', act: `open:fleet:${id}`, ok: true, tip: '' }],
    sections: [{ heading: 'WHAT TO SHELL', text: rows.length ? '' : card.shelled ? 'Its guns have spoken this month: they can speak again next month.' : 'There is nothing there the guns can reach.', rows }],
  };
}

/** What winning means: the player's short campaign and his long one, each thing they ask for ticked or not, and how long is left. */
function goalsPage(s: Game): Page | null {
  const me = s.human;
  if (!me) return null;
  const date = (turn: number): string => turnToDate(turn).label;
  const sections: Page['sections'] = [];
  let open = 0, wonAny = 0;
  for (const key of GOAL_KEYS) {
    const goal = GOALS[me]?.[key];
    if (!goal) continue;
    const state = goalState(s, key);
    const checks = goalChecks(s, me, goal);
    const done = checks.filter((c) => c.done).length;
    const left = goalEnds(goal) - s.turn;
    if (state === 'open') open++;
    if (state === 'won') wonAny++;
    const standing = state === 'won' ? 'Won.' : state === 'lost' ? 'Lost: its time ran out, or the country fell.' : state === 'out' ? 'Not in play in this campaign.'
      : `${done} of ${checks.length} done, ${goalWhen(goal, date)}${goal.outlast === undefined && left >= 0 ? ` (${left} month${left === 1 ? '' : 's'} left)` : ''}.`;
    sections.push({
      heading: `${key === 'short' ? 'THE SHORT CAMPAIGN' : 'THE LONG CAMPAIGN'}  \u00B7  ${goal.title.toUpperCase()}`,
      text: `${goal.text}\n${standing}`,
      rows: checks.map((c) => {
        const r = s.byName[c.label];
        return {
          nat: r ? r.ctrl : '', title: `${c.done ? '\u2713' : '\u25CB'}  ${c.label}`,
          lines: [r ? (c.done ? `Held by ${s.nations[r.ctrl]?.name ?? r.ctrl}.` : `In the hands of ${s.nations[r.ctrl]?.name ?? r.ctrl}: it must be yours or an ally's.`) : c.done ? 'Done.' : 'Not yet.'],
          buttons: r ? [{ label: 'GO THERE', act: `goto:region:${r.id}`, ok: true, tip: 'Looks at the place on the map.' }] : [],
        };
      }),
    });
  }
  if (!sections.length) return null;
  return {
    id: 'goals', title: 'YOUR OBJECTIVES', headline: wonAny ? `${wonAny} campaign${wonAny === 1 ? '' : 's'} won${open ? `, ${open} still to win` : ''}` : open ? `${open} campaign${open === 1 ? '' : 's'} to win` : 'Nothing left to win',
    note: 'What winning means for your country. Goals judge the war and never change it: you can play on after they are decided.',
    top: [], sections,
  };
}

/** Every force the player commands, each with a way to go to it: his armies by where they stand, his fleets, his airfields. */
function forcesPage(s: Game): Page | null {
  const me = s.human;
  if (!me) return null;
  const up = humanUp(s) && !onEve(s);
  const num = (n: number): string => Math.round(n).toLocaleString('en-US');
  const places = new Map<number, Corps[]>();
  for (const c of s.corps) if (!c.dead && c.nat === me) places.set(c.reg, [...(places.get(c.reg) ?? []), c]);
  let canMove = 0;
  const armies: PageRow[] = [...places.entries()].map(([reg, cs]) => {
    const moving = cs.filter((c) => up && canOrder(s, c) && c.ap > 0);
    const field = cs.filter((c) => c.kind !== 'fort');
    if (moving.length) canMove++;
    const gen = cs.map((c) => generalOf(s, c)?.name).find(Boolean);
    const kinds = new Map<string, number>();
    for (const c of field) kinds.set(KIND[c.kind]?.label ?? c.kind, (kinds.get(KIND[c.kind]?.label ?? c.kind) ?? 0) + c.divs);
    const what = [...kinds.entries()].map(([k, n]) => `${n} ${k.toLowerCase()}`).join(', ');
    return {
      sort: (moving.length ? 0 : 1) * 1e6 - cs.reduce((n, c) => n + men(c), 0) / 1000,
      row: {
        nat: me, title: `${s.regions[reg].name}${gen ? `  ·  ${gen}` : ''}`,
        lines: [`${cs.length} corps, ${num(cs.reduce((n, c) => n + men(c), 0))} men${what ? `: ${what} divisions` : ''}.`, moving.length ? `${moving.length} corps can still move this month.` : up ? 'It has used its month, or holds its ground.' : ''].filter(Boolean),
        buttons: [{ label: 'GO THERE', act: `goto:army:${reg}`, ok: true, tip: 'Looks at the army and picks it up.' }],
      },
    };
  }).sort((a, b) => a.sort - b.sort).map((x) => x.row);
  const fleets: PageRow[] = s.fleets.filter((f) => f.nat === me && f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd + f.ships.ss > 0).map((f) => {
    const parts = [[f.ships.bb, 'battleships'], [f.ships.cv, 'carriers'], [f.ships.ca, 'cruisers'], [f.ships.dd, 'destroyers'], [f.ships.ss, 'submarines']].filter((x) => (x[0] as number) > 0).map((x) => `${x[0]} ${x[1]}`).join(', ');
    const sea = s.seas[f.zone]?.name ?? 'the sea';
    return {
      nat: me, title: f.name,
      lines: [`${parts}.`, `${fleetView(s, f.id)?.doing ?? (f.stance === 'port' ? `In harbour at ${f.port}` : f.stance === 'raid' ? `Raiding shipping in the ${sea}` : `On patrol in the ${sea}`)}.`],
      buttons: [{ label: 'GO THERE', act: `goto:fleet:${f.id}`, ok: true, tip: 'Looks at the fleet and picks it up.' }],
    };
  });
  const bases = new Map<number, Game['airFleets']>();
  for (const f of s.airFleets) if (f.nat === me && f.ftr + f.bmr >= 1) bases.set(f.base, [...(bases.get(f.base) ?? []), f]);
  const fields: PageRow[] = [...bases.entries()].map(([reg, fs]) => ({
    nat: me, title: s.regions[reg]?.name ?? 'An airfield',
    lines: [`${fs.length} air group${fs.length === 1 ? '' : 's'}: ${num(fs.reduce((n, f) => n + f.ftr, 0))} fighters, ${num(fs.reduce((n, f) => n + f.bmr, 0))} bombers.`, fs.map((f) => f.name).slice(0, 6).join(', ') + (fs.length > 6 ? ` and ${fs.length - 6} more` : '') + '.'],
    buttons: [{ label: 'OPEN THE AIRFIELD', act: `goto:field:${reg}`, ok: true, tip: 'Looks at the airfield and opens it.' }],
  }));
  return {
    id: 'forces', title: 'YOUR FORCES', headline: `${armies.length} arm${armies.length === 1 ? 'y' : 'ies'}, ${fleets.length} fleet${fleets.length === 1 ? '' : 's'}, ${fields.length} airfield${fields.length === 1 ? '' : 's'} with aircraft`,
    note: up ? (canMove ? `${canMove} of your armies can still move this month. They are listed first.` : 'Every army has used its month.') : 'It is not your month.',
    top: [],
    sections: [
      { heading: 'ARMIES', text: armies.length ? '' : 'You have no army in the field.', rows: armies },
      { heading: 'FLEETS', text: fleets.length ? '' : 'You have no fleet.', rows: fleets },
      { heading: 'AIRFIELDS', text: fields.length ? '' : 'You have no aircraft.', rows: fields },
    ],
  };
}

/** One of the rooms the general panel shows. region is the province selected on the map, or -1. */
const moneyWords = (pp: number): string => shown(pp).toLocaleString('en-US');
const monthsWords = (n: number): string => `${n} month${n === 1 ? '' : 's'}`;

/**
 * The situation as a campaign opens, told as a flight over the map: the player's capital, each enemy's frontier,
 * his friends, what he is short of, and his first objective. Each stop has a place to look at, how close, which way
 * the eye is turned, a heading and a few plain sentences. Only what the player may know is said.
 */
function briefing() {
  const s = game();
  const me = s.human && s.nations[s.human]?.alive ? s.nations[s.human] : null;
  if (!me) return { ok: false };
  const seen = sight(s);
  const cards: { region: number; zoom: number; turn: number; title: string; text: string }[] = [];
  const cap = me.capital >= 0 ? me.capital : s.regions.find((r) => r.ctrl === me.id)?.id ?? -1;
  const num = (n: number): string => Math.round(n).toLocaleString('en-US');
  const count = (n: number, one: string, many: string): string => `${num(n)} ${n === 1 ? one : many}`;
  // What answers to him.
  const mine = s.corps.filter((c) => !c.dead && c.nat === me.id);
  const places = new Set(mine.filter((c) => c.kind !== 'fort').map((c) => c.reg)).size;
  const divs = mine.reduce((n, c) => n + c.divs, 0);
  const ships = s.fleets.filter((f) => f.nat === me.id).reduce((n, f) => n + f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd + f.ships.ss, 0);
  const planes = s.airFleets.filter((f) => f.nat === me.id).reduce((n, f) => n + f.ftr + f.bmr, 0);
  const forces = [count(divs, 'division', 'divisions') + ` in ${count(places, 'army', 'armies')}`, ships ? count(ships, 'warship', 'warships') : '', planes >= 1 ? `${num(planes)} aircraft` : ''].filter(Boolean);
  cards.push({
    region: cap, zoom: 0.4, turn: 0, title: `${me.name.toUpperCase()}  \u00B7  ${turnToDate(s.turn).label.toUpperCase()}`,
    text: `You lead ${theName(s, me.id)}${s.regions[cap] ? ` from ${townIn(s, cap)?.name ?? s.regions[cap].name}` : ''}. ${listOf(forces)} answer to you.${budgeted(s) ? ` The treasury holds ${inHand(me.pp).toLocaleString('en-US')}.` : ''}`,
  });
  // His enemies: each where its ground meets his, or at its capital if the sea lies between.
  const foes = me.atWar.filter((id) => s.nations[id]?.alive).sort((a, b) => fieldArmy(s, b) - fieldArmy(s, a));
  let turn = -18;
  for (const foe of foes.slice(0, 3)) {
    const them = s.nations[foe];
    let best = -1, bestSeen = -1;
    for (const r of s.regions) {
      if (r.ctrl !== foe || !r.nb.some((n) => s.regions[n].ctrl === me.id)) continue;
      const there = seen[r.id] > 0 ? s.corps.filter((c) => !c.dead && c.reg === r.id && c.nat === foe).reduce((n, c) => n + c.divs, 0) : 0;
      if (there > bestSeen) { bestSeen = there; best = r.id; }
    }
    const theirCap = them.capital >= 0 ? s.regions[them.capital] : null;
    if (best >= 0) {
      const inSight = s.regions.filter((r) => r.ctrl === foe && seen[r.id] > 1).reduce((n, r) => n + s.corps.filter((c) => !c.dead && c.reg === r.id && c.nat === foe).reduce((m, c) => m + c.divs, 0), 0);
      cards.push({
        region: best, zoom: 0.33, turn, title: `AT WAR WITH ${theName(s, foe).toUpperCase()}`,
        text: `${cap1(theName(s, foe))} lies across this frontier.${inSight ? ` ${count(inSight, 'division', 'divisions')} of its army ${inSight === 1 ? 'is' : 'are'} in sight.` : ' What it has behind the frontier is not known.'}${theirCap ? ` Its capital is ${townIn(s, theirCap.id)?.name ?? theirCap.name}.` : ''}`,
      });
    } else if (theirCap) {
      cards.push({
        region: theirCap.id, zoom: 0.5, turn, title: `AT WAR WITH ${theName(s, foe).toUpperCase()}`,
        text: `${cap1(theName(s, foe))} has no frontier with you: the sea, or another country, lies between. Its fleets and its bombers can still reach you, and yours can reach it.`,
      });
    }
    turn = -turn;
  }
  if (!foes.length) {
    // At peace: the strongest power on his borders.
    const near = [...new Set(s.regions.filter((r) => r.ctrl === me.id).flatMap((r) => r.nb.map((n) => s.regions[n].ctrl)).filter((id) => id !== me.id && s.nations[id]?.alive))].sort((a, b) => fieldArmy(s, b) - fieldArmy(s, a));
    const big = near[0];
    const at = big ? s.regions.find((r) => r.ctrl === big && r.nb.some((n) => s.regions[n].ctrl === me.id))?.id ?? cap : cap;
    cards.push({
      region: at, zoom: 0.4, turn: 14, title: 'AT PEACE',
      text: `Nobody is at war with you.${big ? ` ${cap1(theName(s, big))} is the strongest power on your borders, with ${count(fieldArmy(s, big), 'division', 'divisions')}${near.length > 1 ? `; ${listOf(near.slice(1, 4).map((id) => theName(s, id)))} ${near.length === 2 ? 'is' : 'are'} your other neighbour${near.length === 2 ? '' : 's'}` : ''}.` : ''} The war will come to your borders or it will not: how you stand toward it is yours to decide.`,
    });
  }
  // His friends.
  const friends = Object.values(s.nations).filter((n) => n.alive && n.id !== me.id && allied(s, n.id, me.id)).sort((a, b) => fieldArmy(s, b.id) - fieldArmy(s, a.id));
  if (friends.length) {
    const first = friends[0];
    cards.push({
      region: first.capital >= 0 ? first.capital : cap, zoom: 0.5, turn: 10, title: 'YOUR ALLIES',
      text: `${cap1(listOf(friends.slice(0, 4).map((n) => theName(s, n.id))))} ${friends.length === 1 ? 'stands' : 'stand'} with you${friends.length > 4 ? `, and ${friends.length - 4} more` : ''}. ${cap1(theName(s, first.id))} is the strongest of them, with ${count(fieldArmy(s, first.id), 'division', 'divisions')}.`,
    });
  } else if (foes.length) {
    cards.push({ region: cap, zoom: 0.62, turn: 0, title: 'YOU STAND ALONE', text: 'No country fights beside you. DIPLOMACY shows where every neutral stands, and which of them might be won over.' });
  }
  // What he is short of.
  const st = storesOf(s);
  if (st) {
    const line = (word: string, b: { months: number; net: number; short: boolean }): string => (b.short ? `${word} is already short: the army and the factories are going without.`
      : b.net < 0 && b.months >= 0 && b.months <= 18 ? `${word} will last ${count(b.months, 'month', 'months')} at this rate.` : '');
    const said = [line('Oil', st.oil), line('Supply', st.sup)].filter(Boolean);
    if (said.length) {
      const oilAt = s.regions.find((r) => r.oil && r.ctrl === me.id)?.id ?? cap;
      cards.push({ region: st.oil.short || (st.oil.net < 0 && st.oil.months >= 0 && st.oil.months <= 18) ? oilAt : cap, zoom: 0.45, turn: -8, title: 'WHAT YOU ARE SHORT OF', text: `${said.join(' ')} The bars under the treasury show both, and what feeds them.` });
    }
  }
  // His first objective.
  for (const key of GOAL_KEYS) {
    const goal = GOALS[me.id]?.[key];
    if (!goal || goalState(s, key) !== 'open') continue;
    const next = goalChecks(s, me.id, goal).find((c) => !c.done && s.byName[c.label]);
    const r = next ? s.byName[next.label] : null;
    cards.push({
      region: r ? r.id : cap, zoom: 0.4, turn: 6, title: `YOUR OBJECTIVE  \u00B7  ${goal.title.toUpperCase()}`,
      text: `${goal.text}${r ? ` Begin with ${r.name}, which ${theName(s, r.ctrl)} holds.` : ''} The O key shows how it stands at any time.`,
    });
    break;
  }
  return { ok: true, date: turnToDate(s.turn).label, nation: me.name, cards: cards.filter((c) => c.region >= 0).slice(0, 7) };
}

/** The town plan of one province: its plots, what stands on them, and every line of building that could go up. */
function townPage(s: Game, region: number): Page | null {
  const t = townBuildings(s, region);
  const r = s.regions[region];
  if (!t || !r) return null;
  const up = humanUp(s) && !onEve(s);
  const date = (turn: number): string => turnToDate(turn).label;
  const n = t.plots.length;
  const plots: PageRow[] = t.plots.map((p, i) => p.line
    ? {
      nat: '', title: `Plot ${i + 1}  \u00B7  ${p.name}: ${p.level > 0 ? p.label : 'being built'}${p.level > 0 ? `  (level ${p.level} of 3)` : ''}`,
      lines: [p.does, p.building ? `${p.level > 0 ? 'The next level is going up' : 'Going up'}: due ${date(p.due)}.` : ''].filter(Boolean), buttons: [],
    }
    : { nat: '', title: `Plot ${i + 1}  \u00B7  Free`, lines: [t.mine ? 'Choose a building for it below.' : 'Nothing built.'], buttons: [] });
  const offers: PageRow[] = t.offers.map((o) => {
    const p = t.plots.find((x) => x.line === o.line);
    const going = !!p?.building, top = o.has >= 3 && !going;
    const lines = top ? [`Complete: ${o.hasLabel}.`, o.blurb]
      : going ? [`${o.label} is going up: due ${date(p!.due)}.`, o.blurb]
        : [`${o.has ? 'Next' : 'First'}: ${o.label}  \u00B7  ${moneyWords(o.cost)}  \u00B7  ${monthsWords(o.months)}`, o.effect, o.blurb];
    return {
      nat: '', title: `${o.name}${o.has ? `  \u00B7  ${o.hasLabel}` : ''}`, lines,
      buttons: top || going ? [] : [{ label: o.has ? 'RAISE' : 'BUILD', act: `civic:${region}:${o.line}`, ok: up && o.why === null, tip: !up ? 'It is not your month.' : o.why ?? `${o.label}: ${o.effect}` }],
    };
  });
  const sections: Page['sections'] = [{ heading: 'THE PLOTS', text: `${r.name}${r.owner === r.ctrl ? '' : ', conquered ground'}.`, rows: plots }];
  if (t.mine) sections.push({ heading: 'WHAT CAN BE BUILT HERE', text: 'One building goes up at a time in a town.', rows: offers });
  return {
    id: `town:${region}`, title: `THE TOWN PLAN  \u00B7  ${t.town.toUpperCase()}`,
    headline: `${t.size === 'capital' ? 'The capital' : `A ${t.size}`}: ${n} building plot${n === 1 ? '' : 's'}, ${t.free ? `${t.free} free` : 'all taken'}`,
    note: 'A town is a high street with a few empty lots: a village has one, a town two, a city three and the capital four. Each lot takes one kind of building, and each building can be raised two more levels. A lot once built on stays that kind.',
    top: [
      { label: 'SHOW ON THE MAP', act: `goto:region:${region}`, ok: true, tip: 'Looks at the place on the map.' },
      { label: 'TOWNS TO BUILD UP', act: 'open:towns', ok: true, tip: 'The best place for each kind of building now.' },
      { label: 'TRADE AGREEMENTS', act: 'open:trade', ok: !!tradeLedger(s), tip: 'What the commerce of your towns is traded for.' },
    ],
    sections,
  };
}

/** The buildings worth ordering next: for each kind, the richest of the player's places that can take it now. */
function townsPage(s: Game): Page | null {
  const me = s.human ? s.nations[s.human] : null;
  if (!me || !townBuildings(s, me.capital)) return null;
  const up = humanUp(s) && !onEve(s);
  const tips = buildingTips(s, 8);
  const going = me.queue.filter((b) => (b as { what?: string }).what === 'civic');
  const rows: PageRow[] = tips.map((tip) => {
    const o = tip.offer;
    return {
      nat: '', title: `${o.name}: ${o.label}  \u00B7  ${tip.town}${tip.town === tip.province ? '' : `, ${tip.province}`}`,
      lines: [`${moneyWords(o.cost)}  \u00B7  ${monthsWords(o.months)}`, o.effect],
      buttons: [
        { label: o.has ? 'RAISE' : 'BUILD', act: `civic:${tip.region}:${o.line}`, ok: up && o.why === null, tip: !up ? 'It is not your month.' : o.why ?? o.blurb },
        { label: 'TOWN PLAN', act: `open:town:${tip.region}`, ok: true, tip: 'Every plot of this town and everything that could be built there.' },
      ],
    };
  });
  const sections: Page['sections'] = [{ heading: 'THE BEST PLACE FOR EACH KIND OF BUILDING NOW', text: tips.length ? '' : 'Nothing more can be built in your towns this month.', rows }];
  if (going.length) {
    sections.push({
      heading: 'GOING UP', text: '',
      rows: going.map((b) => ({ nat: '', title: orderLine(s, b).name, lines: [`Due ${turnToDate(b.eta).label}.`], buttons: [{ label: 'TOWN PLAN', act: `open:town:${b.region}`, ok: true, tip: 'Every plot of this town.' }] })),
    });
  }
  return {
    id: 'towns', title: 'TOWNS TO BUILD UP', headline: `${tips.length} building${tips.length === 1 ? '' : 's'} worth ordering${going.length ? `, ${going.length} going up` : ''}`,
    note: 'Commerce pays into the treasury, farming fills the depots, learning quickens research, training and medicine look after the army, and administration brings in tax and political capital. Pick any province of yours on the map and press TOWN PLAN to see its plots.',
    top: [{ label: 'TRADE AGREEMENTS', act: 'open:trade', ok: !!tradeLedger(s), tip: 'What the commerce of your towns is traded for.' }],
    sections,
  };
}

/** The player's trade: agreements in force and how the goods travel, and every country he could sign with. */
function tradePage(s: Game): Page | null {
  const b = tradeLedger(s);
  if (!b) return null;
  const up = humanUp(s) && !onEve(s);
  const free = b.houses - b.used;
  const deals: PageRow[] = b.deals.map((d) => ({
    nat: d.nat, title: `${d.name}  \u00B7  +${moneyWords(d.pays)} a month${d.state === 'raided' ? ` of ${moneyWords(d.full)}` : ''}`,
    lines: [
      `${d.route}. Signed ${turnToDate(d.since).label}.`,
      d.state === 'cut' ? 'CUT: nothing gets through, and it pays nothing until the way is open again.' : d.state === 'raided' ? `RAIDED: ${Math.round(d.lost * 100)}% of the cargoes are being sunk.` : '',
    ].filter(Boolean),
    buttons: [{ label: 'END IT', act: `trade:end:${d.nat}`, ok: up, tip: up ? 'Ends the agreement. The trading house is free for another.' : 'It is not your month.', danger: true }],
  }));
  // (those that could be signed now first, then those wanting only a trading house or political capital, then the rest)
  const rank = (o: { why: string | null; pays: number; poor: boolean; full: boolean; way: string }): number => (o.why === null ? 3e9 : o.poor || o.full ? 2e9 : o.way !== 'none' ? 1e9 : 0) + o.pays;
  const offers: PageRow[] = b.offers.slice().sort((x, y) => rank(y) - rank(x)).slice(0, 16).map((o) => ({
    nat: o.nat, title: `${o.name}  \u00B7  +${moneyWords(o.pays)} a month`,
    lines: [o.route, o.why ?? ''].filter(Boolean),
    buttons: [{ label: 'SIGN', act: `trade:sign:${o.nat}`, ok: up && o.why === null, tip: !up ? 'It is not your month.' : o.why ?? `Costs ${b.cost} political capital and one trading house on each side.` }],
  }));
  return {
    id: 'trade', title: 'TRADE AGREEMENTS',
    headline: `${b.used} of ${b.houses} trading house${b.houses === 1 ? '' : 's'} in use  \u00B7  +${moneyWords(b.income)} a month`,
    note: `An agreement takes one trading house on each side and costs ${b.cost} political capital to sign; you have ${Math.floor(b.pc)}. Goods go overland or by sea, and a sea route can be raided or cut. Commerce buildings in your towns give you more trading houses (${b.most} at most)${b.boost > 0 ? ` and add ${Math.round(b.boost * 100)}% to what every agreement pays` : ''}.`,
    top: [{ label: 'TOWNS TO BUILD UP', act: 'open:towns', ok: true, tip: 'Where commerce buildings could go up.' }],
    sections: [
      { heading: 'AGREEMENTS IN FORCE', text: deals.length ? '' : 'None yet.', rows: deals },
      { heading: 'COUNTRIES YOU COULD SIGN WITH', text: free > 0 ? `${free} trading house${free === 1 ? '' : 's'} free.` : 'Every trading house is in use: end an agreement or build more commerce to sign another.', rows: offers },
    ],
  };
}

/**
 * The government and its war cabinet (Version 57): the cabinet's trust, the directive in hand with its clock, the
 * four offices and who could hold each, the two ladders of decrees, what it all comes to, and the minutes.
 */
function cabinetPage(s: Game): Page | null {
  let v: ReturnType<typeof governmentOf> = null;
  try { v = governmentOf(s); } catch { return null; }
  if (!v || !v.mine) return null;
  const up = humanUp(s) && !onEve(s);
  const notNow = 'It is not your month.';
  const months = (n: number): string => `${n} month${n === 1 ? '' : 's'}`;
  const cap = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
  const d = v.directive;
  const directive: PageRow[] = d ? [{
    nat: s.nations[d.from] ? d.from : v.nat, title: `${d.title}  \u00B7  ${d.left <= 1 ? 'due this month' : `${months(d.left)} left`}`,
    lines: [d.text, `Set by ${d.by.replace(/^The /, 'the ')}, to be done by the end of ${turnToDate(d.due).label}.`, d.progress, `Reward: ${d.reward}.`.replace(/\.\.$/, '.')],
    buttons: [
      ...(d.region !== undefined && d.region >= 0 ? [{ label: 'SHOW ON THE MAP', act: `goto:${d.region}`, ok: true, tip: `Goes to ${s.regions[d.region]?.name ?? 'the place'}.` }] : []),
      (() => { const why = up ? whyNotSetAsideDirective(s) : notNow; return { label: 'SET IT ASIDE', act: 'cabinet:aside', ok: why === null, tip: why ?? `Tells the cabinet it will not be done. Costs ${d.price} of its trust at once, the same as letting it lapse, and the next directive comes sooner.`, danger: true }; })(),
    ],
  }] : [];
  const offices: PageRow[] = v.seats.map((seat) => ({
    nat: v!.nat, title: `${seat.name}  \u00B7  ${seat.minister.name}`,
    lines: [cap(seat.minister.plus), cap(seat.minister.minus)].filter(Boolean),
    buttons: seat.options.map((o) => ({
      label: o.name.toUpperCase(), act: `cabinet:appoint:${seat.id}:${o.id}`, ok: up && o.why === null,
      tip: !up ? notNow : o.why ?? `${cap(o.plus)}. ${cap(o.minus)}. The appointment costs ${v!.appointCost} political capital.`.replace(/\.\./g, '.'),
    })),
  }));
  const decrees: PageRow[] = v.tracks.map((t) => ({
    nat: v!.nat, title: `${t.name}  \u00B7  ${t.steps[t.at]?.name ?? ''}`,
    lines: [t.steps[t.at]?.text ?? ''].filter(Boolean),
    buttons: t.steps.map((st, i) => ({ st, i })).filter((x) => !x.st.on).map(({ st, i }) => ({
      label: st.name.toUpperCase(), act: `cabinet:decree:${t.id}:${i}`, ok: up && st.why === null,
      tip: !up ? notNow : st.why ?? `${st.text}${i > t.at && st.cost > 0 ? ` Costs ${st.cost} political capital.` : i < t.at ? ' Stepping down is free.' : ''}`,
    })),
  }));
  const falling = v.trust <= v.fallsAt;
  return {
    id: 'cabinet', title: 'THE CABINET',
    // (the headline is one short line: the screen cuts a long one off)
    headline: `Trust ${Math.round(v.trust)} of 100  \u00B7  ${v.trustWord}`,
    note: `${v.form.name}. Political capital ${Math.floor(v.capital)} of ${v.capitalMost}, +${v.capitalMonth.toFixed(1)} a month. ${v.form.blurb} ${falling ? `The cabinet's trust is at ${v.fallsAt} or under: if it stays there for ${months(v.crisisMonths)} running the cabinet falls (it has been there for ${months(v.low)}).` : `With no news the cabinet's trust drifts back toward ${Math.round(v.rest)}. At ${v.fallsAt} or under for ${months(v.crisisMonths)} running, the cabinet falls.`} Directives carried out: ${v.done}. Missed or set aside: ${v.missed}.`,
    top: [],
    sections: [
      { heading: 'THE DIRECTIVE', text: d ? '' : `No directive stands. ${v.nextIn > 0 ? `The cabinet sets the next in ${months(v.nextIn)}.` : 'The cabinet is weighing the next.'}`, rows: directive },
      { heading: 'THE FOUR OFFICES', text: `A new minister costs ${v.appointCost} political capital. Each brings something and costs something.`, rows: offices },
      { heading: 'DECREES', text: 'One step up at a time, for political capital. A step down is free.', rows: decrees },
      { heading: 'WHAT IT ALL COMES TO', text: v.sums.length ? v.sums.join('  \u00B7  ') : 'Nothing either way.', rows: [] },
      { heading: "WHAT MOVED THE CABINET'S TRUST", text: v.moved.length ? v.moved.map((m) => `${m.label} ${m.amount > 0 ? '+' : ''}${m.amount.toFixed(1)}`).join('  \u00B7  ') : 'Nothing since it last read the news.', rows: [] },
      { heading: 'MINUTES', text: v.log.length ? v.log.slice(-8).reverse().map((l) => `${turnToDate(l.turn).label}: ${l.text}`).join('\n') : 'Nothing minuted yet.', rows: [] },
    ],
  };
}

function page(id: string, region = -1) {
  const s = game();
  const part = id.split(':');
  const p = id === 'cabinet' ? cabinetPage(s) : id === 'trade' ? tradePage(s) : id === 'towns' ? townsPage(s) : part[0] === 'town' ? townPage(s, Number(part[1]))
    : id === 'spies' ? spiesPage(s, region) : id === 'lastact' ? lastActPage(s) : id === 'forces' ? forcesPage(s) : id === 'reports' ? reportsPage(s) : id === 'goals' ? goalsPage(s) : id === 'heroes' ? heroesPage(s) : part[0] === 'fleet' ? fleetPage(s, Number(part[1])) : part[0] === 'shell' ? shellPage(s, Number(part[1]), Number(part[2]))
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
    // A search of a sea is seen to go out too: a few aircraft from the group's airfield, or from the ships.
    if (air.ok && part[0] === 'search') {
      const grp = s.airFleets.find((x) => x.id === Number(part[1])) as (typeof s.airFleets[number] & { ship?: number }) | undefined;
      const sea = s.seas[Number(part[2])];
      const ship = grp?.ship !== undefined ? s.fleets.find((f) => f.id === grp.ship) : undefined;
      const from = ship ? s.seas[ship.zone] : undefined;
      if (grp && sea) {
        // (a search of the fleet's own waters still flies out and back: a sweep to one side of the ships)
        const own = !!ship && ship.zone === sea.id;
        flies = [{
          from: ship ? -1 : grp.base, ...(ship && from ? { fromX: from.x, fromY: from.y, fleet: ship.id } : {}), x: sea.x + (own ? 46 : 0), y: sea.y - (own ? 30 : 0), nat: grp.nat,
          bombers: false, escort: false, region: -1, aa: 0, meet: '', meetFrom: -1, loss: 0,
        }];
      }
    }
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'civic') {
    // A building ordered in one of the player's towns.
    const at = Number(part[1]);
    const before = townBuildings(s, at)?.offers.find((o) => o.line === part[2]);
    const b = orderBuilding(s, at, part[2]);
    ok = !!b;
    note = b ? `${before?.label ?? 'The building'} ordered at ${townBuildings(s, at)?.town ?? s.regions[at]?.name ?? 'the town'}. Due ${turnToDate(b.eta).label}.` : before?.why ?? 'That cannot be built now.';
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'cabinet') {
    // The government (Version 57): a minister appointed, a decree, or the directive set aside.
    const v = governmentOf(s);
    if (part[1] === 'appoint') {
      const why = whyNotAppointMinister(s, part[2], part[3]);
      const seat = v?.seats.find((x) => x.id === part[2]);
      const who = seat?.options.find((o) => o.id === part[3])?.name ?? 'The new minister';
      ok = !why && orderAppoint(s, part[2], part[3]);
      note = ok ? `${who} takes office${seat ? ` as ${seat.name}` : ''}.` : why ?? 'That appointment cannot be made now.';
    } else if (part[1] === 'decree') {
      const step = Number(part[3]);
      const why = whyNotOrderDecree(s, part[2], step);
      const track = v?.tracks.find((x) => x.id === part[2]);
      ok = !why && orderDecree(s, part[2], step);
      note = ok ? `Decreed: ${track?.steps[step]?.name ?? 'the new measure'}${track ? ` (${track.name})` : ''}.` : why ?? 'That cannot be decreed now.';
    } else if (part[1] === 'aside') {
      const why = whyNotSetAsideDirective(s);
      const title = v?.directive?.title ?? 'The directive';
      ok = !why && orderSetAside(s);
      note = ok ? `${title}: set aside. The cabinet takes note.` : why ?? 'There is no directive to set aside.';
    }
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'trade') {
    const who = s.nations[part[2]]?.name ?? part[2];
    if (part[1] === 'sign') {
      const why = tradeLedger(s)?.offers.find((o) => o.nat === part[2])?.why ?? null;
      ok = orderTrade(s, part[2]);
      note = ok ? `A trade agreement is signed with ${who}.` : why ?? 'That agreement cannot be signed now.';
    } else if (part[1] === 'end') {
      ok = orderEndTrade(s, part[2]);
      note = ok ? `The trade agreement with ${who} is ended.` : 'That cannot be done now.';
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
  } else if (part[0] === 'fleet') {
    ok = orderFleet(s, Number(part[1]), Number(part[2]), part[3] as Stance);
    note = ok ? `${fleetView(s, Number(part[1]))?.doing ?? 'The order is given'}.` : 'That order cannot be given now.';
    return { ok, note, fly, flies: [], open: `fleet:${part[1]}`, view: view() };
  } else if (part[0] === 'fleetescort') {
    ok = orderEscort(s, Number(part[1]), part.slice(2).join(':'));
    note = ok ? `${fleetView(s, Number(part[1]))?.doing ?? 'The fleet is bound to the lane'}.` : 'It cannot escort that lane now.';
    return { ok, note, fly, flies: [], open: `fleet:${part[1]}`, view: view() };
  } else if (part[0] === 'fleetshell') {
    const rep2 = orderBombard(s, Number(part[1]), Number(part[2]), part[3] as 'troops' | 'city');
    ok = !!rep2;
    note = rep2 ? rep2.result : previewBombard(s, Number(part[1]), Number(part[2]), part[3] as 'troops' | 'city')?.why ?? 'The coast cannot be shelled now.';
    // (the map shows the guns falling on the coast)
    return { ok, note, fly, flies: [], open: '', shell: ok ? Number(part[2]) : -1, view: view() };
  } else if (part[0] === 'fleetsplit') {
    const made = orderSplit(s, Number(part[1]), { [part[2]]: Number(part[3]) } as Record<string, number>);
    ok = made >= 0;
    note = ok ? `${fleetView(s, made)?.name ?? 'A new squadron'} is formed from ${fleetView(s, Number(part[1]))?.name ?? 'the fleet'}.` : whyNotSplit(s, Number(part[1]), { [part[2]]: Number(part[3]) } as Record<string, number>) ?? 'Those ships cannot be split off now.';
    return { ok, note, fly, flies: [], open: `fleet:${part[1]}`, made, view: view() };
  } else if (part[0] === 'fleetmerge') {
    ok = orderMerge(s, Number(part[1]), Number(part[2]));
    note = ok ? `The fleets are joined as ${fleetView(s, Number(part[2]))?.name ?? 'one'}.` : 'They cannot be joined now.';
    return { ok, note, fly, flies: [], open: ok ? `fleet:${part[2]}` : `fleet:${part[1]}`, joined: ok ? Number(part[2]) : -1, view: view() };
  } else if (part[0] === 'cash') {
    const award = s.stories.awards[Number(part[1]) - 1];
    const why = award ? whyNotCash(s, award, part[2] as 'bonds' | 'school') : 'There is no such honour.';
    ok = orderCash(s, Number(part[1]), part[2] as 'bonds' | 'school');
    note = ok ? '' : why || 'That cannot be done now.';
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
      // Dismissed for good. A popular general's going is taken badly by the army.
      sack: up && x.status === 'active', popular: popular(x),
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

/** Dismisses a general for good. */
function sackGeneral(general: number) {
  const s = game();
  const h0 = s.headlines.length;
  const ok = humanUp(s) && orderSack(s, general);
  return { ok, note: s.headlines.slice(h0).map((h) => h.text).join(' '), view: view() };
}

/** The picked corps' standing order about drafts: take them as they come, or keep the veterans together and take none. */
function draftsFor(ids: number[], take: boolean) {
  const s = game();
  let n = 0;
  for (const c of corpsById(s, ids)) if (orderDrafts(s, c, take)) n++;
  return { ok: n > 0, note: n ? (take ? 'They take drafts as they come: their ranks fill, and the new men water down their experience.' : 'They keep their veterans together and take no drafts: they stay seasoned, and do not recover their strength.') : 'That order cannot be given to them.', view: view() };
}

/** Places the army as the staff advises, on the eve of war. */
function evePlanGo() {
  const s = game();
  const n = orderEvePlan(s);
  return { ok: n > 0, note: n ? `The staff's plan is carried out: ${n} corps ${n === 1 ? 'is' : 'are'} moved by rail to ${n === 1 ? 'its place' : 'their places'}.` : 'The army already stands where the staff would have it, or cannot be moved.', view: view() };
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
  return { sea: s.seaReports.map((r) => [r.id, r.turn, r.zoneName]), battles: s.flags.airBattles ?? 0, reports: s.airReports.length, turns: [...new Set(s.airReports.map((r) => r.turn))], raids: flownRaids(s, turn ?? s.turn) };
}

(globalThis as Record<string, unknown>).TC = {
  raidsFlown, fleetInHand, briefing, sack: sackGeneral, drafts: draftsFor, evePlanGo, towns: townsList, join: joinArmyAt, sendBack, seaAt: (x: number, y: number) => ({ sea: seaUnder(x, y) }),
  land, work, force, forceOrder, seaReach, ship, landingPlan, landOn, record, roll, stance, railReach, rail, province, raise, fortify, cancel, declare, warPlan, diplomacy, court: courtNation, path: takePathOf, invite: inviteNation, page, act, abilities, research, tech, techStart, techStop, teams, bureau, generals, assign, promote: promoteIn, relieve: relieveOf, skill: skillUp, ledger, arsenal, order, starts, grand, start, save, peek, load, view, reach: reachOf, move, previewAttack: preview, attack, endMonth };
