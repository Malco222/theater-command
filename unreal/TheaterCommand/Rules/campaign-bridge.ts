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
//   TC.reach([corpsId, ...])            -> { regions: [{ id, cost, attack, prev }], danger: [regionId, ...] }
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

import { DEFENCE, allied, atWar, bloc, collapseScore, createGame, fieldArmy, frontage, hostileUnits, limitAttackers, men, moveCost, railReach as railReachOf, litTracks, railReachFor, railSays, rideLines, whyNoTrain, whyNoTrainFor, weather as weatherNow, worksOf } from '../../../src/campaign/game';
import { FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, inService, lead, monthsAhead, nextStep, stepsOf, teamsFree, teamsTotal } from '../../../src/campaign/research';
import type { Field, Skill } from '../../../src/campaign/types';
import { ARSENAL as arsenalBook, factoriesBuilt, workLevel, yardLevel } from '../../../src/campaign/arsenal';
import { TECH as TECH_TREE, hasTech, idleTeams, techOf } from '../../../src/campaign/tech';
import { eliteOf, traitRule, traitRules, traitsOf, eliteRule } from '../../../src/campaign/traits';
import { commandOf, orderDepth, orderDesk, orderStaffCap, staffRunsDesk } from '../../../src/campaign/turn';
import * as everyRule from '../../../src/campaign/turn';
// (the air war's rules by name too: a rule a later version adds is used when the rules have it, and not missed when they have not)
import * as everyAirRule from '../../../src/campaign/air';
import { gunsAlone, homeAlarms, type HomeAlarm } from '../../../src/campaign/danger';
import { DEPTH, deskDef, type DeskId } from '../../../src/campaign/depth';
import { LEAGUE, leagueFounded, leagueMembers, type PathId } from '../../../src/campaign/league';
import { DIPLOMACY, TRACK, TRACK_LABEL, TRACK_PHRASE, canCourt, courtable, friendsOf, majorsOf, momentum, pcIncome, receptive, step, tradePartners } from '../../../src/campaign/diplomacy';
import { startGunsOnLoad } from '../../../src/campaign/airversion';
import {
  arsenalOffers, beginEve, beginHuman, canOrder, candidates, current, declarable, endEve, endHuman, humanUp, onEve, orderRebase, orderAttack, orderBuildCorps, orderArsenal, orderDeclare,
  orderBuildFort, orderCancelBuild, orderMove, orderRail, orderStance, postureOf, previewAttack, reach, runTurnAI, stepAI,
  orderAir, orderAirBase, orderFleet, fleetKeptByHand, handBackFleet, levelNow, orderLevel, orderLand, orderShip, previewLanding,
  depots, shippingLanes, previewAirOrder,
  townBuildings, orderBuilding, buildingTips, tradeLedger, orderTrade, orderEndTrade,
  fleetsInSight, wingsOfFleet,
  prizesOf, prizesOnMap, whyNotWreckPrize, orderWreck, whyNotBurnOrder, orderBurn, orderScorchPolicy,
  governmentOf, whyNotAppointMinister, orderAppoint, whyNotOrderDecree, orderDecree, whyNotSetAsideDirective, orderSetAside,
  manpowerOf, cabinetIsOff, orderCabinet, cabinetBillOf, callUpOf, officeIsSecure, orderOffice, orderEarlyClass, orderCallUp,
  promiseTargetsOf, promiseTermsOf, whyNotOrderPromise, orderPromise, whyNotTakeBackPromise, orderTakeBack, whyNotStandByGeneral, orderStandBy, orderSack,
  townSiege, siegesInSight, whyNotGarrisonTown, orderGarrison, whyNotLeaveTown, orderTakeField, whyNotStormTown, stormOutlook, orderStorm, marchOutlook, orderTake, stormCandidates, stormChoice, verdictOf, whyNotGate, gateQuestion, orderGate, gateLeft, whyNotOrder, whyNotOrderGarrison, garrisonLine, whyNotSallyOut, sallyOutlook, orderSally,
  sight, secretService, orderRecruitAgent, orderSendAgent, orderAgentMission, orderRetireAgent, orderFog, whyNotSendAgent,
  lastAct, standings, orderDomination, orderKeepPeace, whyNotOrderDomination,
  orderBureau, orderTeams, techTree, orderResearch, orderStopResearch, whyNotOrderResearch, orderAssign, orderPromote, orderRelieve, orderSkill,
  invasions, previewDeclare, orderCourt, leagueInvites, leaguePartners, orderLeagueInvite, orderPath, paths, whyNotLeagueInvite,
  engineerJobs, orderBlowBridges, orderCutRailway, orderEngineers, orderStopWork, whyNotBlowBridges, whyNotCutRailway,
} from '../../../src/campaign/turn';
import { jobLine, type JobKind } from '../../../src/campaign/engineers';
import { BUDGET, PRODUCTION, budgeted, canRaise, corpsUpkeep, fortCost, income, isHome, ledger as ledgerOf, needsOf, orderCorps, provinceYield } from '../../../src/campaign/economy';
import { PRIZES, type PrizeKind, type ScorchPolicy } from '../../../src/campaign/prizes';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, rankOf, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../../../src/campaign/commanders';
import { STORIES, awardTitle, citation, designation, diaryText, fameOf, grade, honourDef, honoursOf, isGuards, stat, takeOver } from '../../../src/campaign/stories';
import { KIND, TERRAIN_DEF } from '../../../src/campaign/types';
import { MANPOWER } from '../../../src/campaign/manpower';
import { GOALS, GOAL_KEYS, goalChecks, goalEnds, goalState, goalWhen, judgeGoals, openGoals } from '../../../src/campaign/goals';
import { isOpen, startOpen } from '../../../src/campaign/staff';
// (release 1.5) The general's ground battle: the campaign half of FIGHT IT YOURSELF (battle-bridge.ts).
import { battleCalls } from './battle-bridge';
import type { FoughtAir } from '../../../src/campaign/airfought';
import type { FieldBill } from '../../../src/campaign/game';
import { loadGame, peekSave, saveGame } from '../../../src/campaign/save';
import { ashoreSays } from '../../../src/campaign/ashore';
import { guardSays } from '../../../src/campaign/guard';
import { carried } from '../../../src/campaign/support';
import { briefTicket } from '../../../src/campaign/facing';
import { guardMen, previewSally, salliers } from '../../../src/campaign/siege';
import { GROUND, groundRules, groundSays } from '../../../src/campaign/ground';
import { blocked, bridgeDown, crossingsOf, riverBetween, riverName, riverNotes, riversOf } from '../../../src/campaign/rivers';
import { cutRail, railCut, railLinks, railNote, railsAt, workingFactories, type RailCause } from '../../../src/campaign/rails';
import { INTEL as SERVICE, cutSays, cutsKnown } from '../../../src/campaign/intel';
import { baseTargets, cutOff, cutOffLine, fleetReach, harbourFor, landingTargets, seaAt as seaUnder, whyNotLand, whyNotShip } from '../../../src/campaign/navy';
import { gateBar, gateSpot, gateViews } from '../../../src/campaign/gates';
import { armyRound, armyStands, frontIdle, nextArmy, roundCount, roundQuestion, roundWords, settleStands, standArmy } from '../../../src/campaign/round';
import { AIR, MISSION, SHIP_AIR_TIP, airRange, airTargets, forecastAir, gunsAt as gunsRound, km as airKm, moveTargets, overOcean, whyNotMission } from '../../../src/campaign/air';
import type { AirMission, Stance } from '../../../src/campaign/types';
import { DIFFICULTY } from '../../../src/campaign/difficulty';
import { airAct, airPage } from '../../../src/campaign/airpages';
import { fleetOptions, fleetView, orderBombard, orderEscort, orderFleetAttackNow, orderMerge, orderSplit, previewBombard, previewFleetAttack, whyNotSplit } from '../../../src/campaign/turn';
import { breakUp as breakCorps, breakUpOn, rejoin as rejoinCorps, rejoinTargets, setBreakUp, whyNotBreakUp, whyNotRejoin } from '../../../src/campaign/breakup';
import { fleetAttackOn, setFleetAttack } from '../../../src/campaign/fleetattack';
import { raidMenu as raidKinds, raidTargetsFor as raidStopsFor } from '../../../src/campaign/turn';
import { cityOrders as ordersBook, inMonths as readyWhen, type CityOrders as CityBook } from '../../../src/campaign/orders';
import { townName } from '../../../src/campaign/towns';
import { aboutMonths, advanceMarches, enemyReach, farGround, farRoad, forgetMarch, marchMonths, marchOf, marchState, orderMarch, planMarch } from '../../../src/campaign/marches';
import { markFor as markOfRules, monthCard as cardOfRules, monthMark, monthOrders, type CardFigure as RuleFigure, type CardTone, type MonthCard as RuleCard, type MonthMark } from '../../../src/campaign/monthcard';
import { keepsStores } from '../../../src/campaign/resources';
import { newsSide } from '../../../src/campaign/turn';
import type { RaidKind, RaidPlan, RaidWay } from '../../../src/campaign/airwar';
import { orderScout as sendScout, previewRaid as weighRaid, previewDayNight as weighDayNight, joinOffers, orderJoinAll, orderSendBack, orderEvacuate, evePlan, orderEvePlan, orderCash, orderSeen, orderDrafts } from '../../../src/campaign/turn';
import { whyNotCash } from '../../../src/campaign/stories';
import { popular } from '../../../src/campaign/commanders';
import { seaOpenAt } from '../../../src/campaign/navy';
import { NAVY } from '../../../src/campaign/navy';
import { chanceOn, headline } from '../../../src/campaign/game';
// (Rules 84.2.6) Britain by the name the news gives her, every name's article where it follows a word, and a dominion's mother country.
import { ENGINEERS_ALONE, GUNS_ALONE, corpsNamed, gunsRule, newsName, theWords } from '../../../src/campaign/game';
import { answersOf, motherLine, patronOf } from '../../../src/campaign/world';
import { atSea as fleetAtSea, shipCount as shipsIn } from '../../../src/campaign/navy';
import { fleetKnown as fleetKnownTo, seaPicture as seaPictureOf } from '../../../src/campaign/carriers';
import { airfieldsSeen, announcements, attackOutlook, openingForce, pocketAt, softenOptions, orderProbe, foldChoicesAt, orderFold, fieldworksLeft, rivalStaff, swapChoices, orderSwap, jointForce, whyNotAttack, corpsDetail, corpsKnownAs, fallenCapitalAt, flownRaids, knowledgeOf, nationsFallen, playerFell, prize as prizeOfPlace, townIn, towns as townsOfMap } from '../../../src/campaign/turn';
import { STARTS } from './starts';
import grandData from '../../../data/campaign/grand.json';
import { turnToDate } from '../../../src/core/ticket';
import { TRADE as TRADE_RULES, dealsOf as tradeDealsOf, housesOf as tradeHousesOf, tradePath as tradePathOf, tradeRules as tradeIsOn, whyNoTrade as whyNoTradeWith } from '../../../src/campaign/trade';
import { civicSum as civicSumOf } from '../../../src/campaign/buildings';
// (Rules 84.3.3) FIRST COMMAND: the first hour's ten staff notes, one list for both games.
import {
  BOMBING, CLOSING, FIRST_COMMAND, LESSONS, LESSON_DOOR, LESSON_GROUPS, LESSON_NAMES, LESSON_SETTING, TIPS, allDone, bringBack, closingOwed, currentLesson, doneCount,
  firstCommand, isDone, lessonDeed, lessonWords, lessonsAgain, lessonsOn, namedWords, pickupPlace, putAway, readNotebook, showAgain, siegeOf, waitingCount,
  type Deed, type Lesson, type Moment, type Notebook,
} from '../../../src/campaign/lessons';
import { cutThroughGuns } from '../../../src/campaign/rails';
import type { BattleReport, Build, Corps, Game, Kind, Posture, Region } from '../../../src/campaign/types';

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

/**
 * (release 1) What each arm is, for the bigger unit card: its weight in attack and in defence, what it is for, and
 * the ground it is at its best and its worst on. Every figure is read from the rules' own tables, so the card can
 * never promise what a battle will not deliver.
 */
function armsTable(s: Game): { kind: string; label: string; atk: number; def: number; role: string; good: string; poor: string }[] {
  const out: { kind: string; label: string; atk: number; def: number; role: string; good: string; poor: string }[] = [];
  try {
    const grand = groundRules(s), alone = gunsRule(s);
    const GROUNDS: [string, string][] = [['plains', 'open country'], ['desert', 'desert'], ['forest', 'forest'], ['hills', 'hills'], ['mountain', 'mountains'], ['marsh', 'marsh'], ['urban', 'cities']];
    const by = (m: number): string => `${m > 1 ? '+' : '−'}${Math.round(Math.abs(m - 1) * 100)}%`;
    const upTo = (x: number): string => `${Math.round(x * 100)}%`;
    const A = GROUND.arms;
    const roles: Record<string, string> = {
      inf: grand ? `Holds ground better than it takes it. With tanks beside it, it is worth up to ${upTo(A.infantry)} more.` : 'Holds ground better than it takes it.',
      mot: grand ? 'Infantry on wheels: quick off the roads, and it counts as the infantry tanks need beside them.' : 'Infantry on wheels: quick off the roads.',
      arm: grand ? `The hardest hitter there is. With infantry beside it, it is worth up to ${upTo(A.armour)} more; alone in a city or a forest it is worth less.` : 'The hardest hitter there is.',
      cav: 'Quick and light: for scouting and pursuit, not for a stand-up fight.',
      mtn: 'At home in hills and mountains, where everyone else struggles.',
      fort: 'A fortress garrison: it holds its works and does not march.',
      // (Rules 84.2.6; the design director's log b357.) Where guns never go forward alone, the card says so in the rules' sentence.
      art: grand ? `Massed guns. Against forts and dug-in troops, up to ${DEFENCE.frontage.guns} artillery corps join an attack without taking room on the roads, and their fire wears the works down.${alone ? ` ${GUNS_ALONE.split('. ')[0]}.` : ''}` : 'Massed guns.',
      eng: `Builds and breaches: bridges, fieldworks, minefields and railways. It is not there for its rifles.${alone ? ` ${ENGINEERS_ALONE.split('. ')[0]}.` : ''}`,
    };
    for (const kind of Object.keys(KIND) as (keyof typeof KIND)[]) {
      const k = KIND[kind];
      const good: string[] = [], poor: string[] = [];
      for (const [t, word] of GROUNDS) {
        let a = grand ? (GROUND.attack[kind] as Record<string, number> | undefined)?.[t] ?? 1 : 1;
        const d = grand ? (GROUND.defence[kind] as Record<string, number> | undefined)?.[t] ?? 1 : 1;
        const m = grand ? (GROUND.march[kind] as Record<string, number> | undefined)?.[t] ?? 0 : 0;
        // (the classic rule, in every campaign: tanks attack at 60% in mountain, city and marsh)
        if (kind === 'arm' && (t === 'mountain' || t === 'urban' || t === 'marsh')) a *= 0.6;
        if (a === 1 && d === 1 && !m) continue;
        const bits = [a !== 1 ? `attack ${by(a)}` : '', d !== 1 ? `defence ${by(d)}` : '', m ? 'slow to enter' : ''].filter(Boolean).join(', ');
        (a > 1 || d > 1 ? good : poor).push(`${word} (${bits})`);
      }
      if (kind === 'arm') good.unshift('open country and desert');
      if (grand && GROUND.steep.barred.includes(kind)) poor.push('the highest mountains (cannot enter)');
      out.push({ kind, label: k.label, atk: k.atk, def: k.def, role: roles[kind] ?? '', good: good.join('; '), poor: poor.join('; ') });
    }
  } catch { /* a card without its notes is still a card */ }
  return out;
}

/**
 * (release 1) The admiral's chart: how well the player's side has each sea searched (0 not searched, 1 thinly: a
 * fleet there would be reported, 2 well: it would be identified, 3 in full view, where this game keeps no fog at
 * sea), and old sightings: enemy ships that were found at sea in the last three months and are lost again, each
 * with the sea they were last seen in and how many months ago. The last sighting of each enemy fleet is kept with
 * the game, as a flag.
 */
function chartOf(s: Game): { seas: number[]; old: { zone: number; nat: string; months: number }[] } {
  const seas: number[] = [], old: { zone: number; nat: string; months: number }[] = [];
  try {
    const me = s.human;
    if (!me || !s.nations[me]?.alive) return { seas, old };
    for (const z of s.seas) {
      const p = seaPictureOf(s, z.id, me);
      seas[z.id] = p.word.startsWith('In full view') ? 3 : p.search >= p.needFind ? 2 : p.search >= p.needReport ? 1 : 0;
    }
    const live = new Set<string>();
    for (const f of s.fleets) {
      const key = `sight:${f.id}`;
      if (!shipsIn(f.ships) || !atWar(s, f.nat, me)) continue;
      live.add(key);
      const known = fleetKnownTo(s, f, me);
      if (known !== 'none') {
        // (found: at sea it is on the chart as a piece; in harbour there is nothing to look for)
        if (fleetAtSea(f)) s.flags[key] = s.turn * 1000 + f.zone; else delete s.flags[key];
        continue;
      }
      const at = s.flags[key];
      if (at === undefined) continue;
      const months = s.turn - Math.floor(at / 1000);
      if (months > 3) { delete s.flags[key]; continue; }
      if (months >= 1 && s.seas[at % 1000]) old.push({ zone: at % 1000, nat: f.nat, months });
    }
    // (a fleet that is no more, or no longer an enemy's, leaves no sighting)
    for (const key of Object.keys(s.flags)) if (key.startsWith('sight:') && !live.has(key)) delete s.flags[key];
  } catch { /* a chart that cannot be drawn is left blank */ }
  return { seas, old };
}

/** What taking a place is worth, in a sentence; nothing where it is worth nothing (bare ground, or a scenario that keeps no money). */
function prizeLine(s: Game, region: number): string {
  const z = prizeOfPlace(s, region);
  return z && (z.income > 0 || z.incomeNow > 0 || z.vp > 0 || z.factories > 0 || z.oil > 0 || z.capitalOf || z.frees) ? z.line : '';
}

/** The picture of the war the game draws. */
/**
 * (Rulings 153, 159, 163 and 169.) The player's agents as pieces on the map, each with what its card says: where it
 * is, what it is doing, the chance it is caught this month, the armies it has in sight, and the work it can be given
 * there with the odds and the reason for any it cannot. Every sentence is the rules' own. `stuck` is the rules' reason
 * why it cannot be sent anywhere now (in training, on the road, not his month), or empty. `marks` are the places where
 * one of his agents was caught this month: the secret war is settled as his month opens, so the note carries this month.
 */
function agentsOnMap(s: Game, up: boolean) {
  type Work = { id: string; label: string; text: string; why: string; ok: boolean; now: boolean; caught: number; success: number; act: string };
  type Seen = { region: number; name: string; nat: string; corps: number; divs: number };
  type Piece = { id: number; name: string; rank: string; skill: number; ranks: number; xp: number; next: number; toward: string; region: number; where: string; host: string; hostName: string;
    abroad: boolean; atWork: boolean; wait: number; mission: string; doing: string; caught: number; rest: number; stuck: string; home: string; will: string; work: Work[]; sees: Seen[] };
  const none = { agents: [] as Piece[], agentMarks: [] as { region: number; text: string }[] };
  try {
    const room = secretService(s);
    if (!room) return none;
    const live = up && !onEve(s);
    const pct100 = (x: number) => Math.round(x * 100);
    const home = room.threat.foes.length ? `This month the chance of an enemy strike at home is ${pct(room.threat.chance)}, with ${room.threat.guards} of your agents on guard.` : 'No enemy service is working against you at home.';
    return {
      agents: room.agents.map((a): Piece => ({
        id: a.id, name: a.name, rank: a.rank, skill: a.skill, ranks: SERVICE.agents.ranks.length, xp: a.xp, next: a.next ?? 0, toward: a.next !== null ? SERVICE.agents.ranks[a.skill] ?? '' : '',
        region: a.reg, where: a.where, host: a.host, hostName: s.nations[a.host]?.name ?? a.host,
        abroad: a.abroad, atWork: a.atWork, wait: a.wait, mission: a.mission, doing: a.doing, caught: pct100(a.caught), rest: a.rest,
        stuck: !live || !a.atWork ? whyNotSendAgent(s, a.id, a.reg) ?? '' : '',
        home: a.abroad ? '' : home,
        // (what the work he will take up is, in the rules' words: for the card of an agent still on the road)
        will: a.missions.find((m) => m.id === a.mission)?.text ?? '',
        work: a.missions.map((m) => ({ id: m.id, label: m.label, text: m.text, why: m.why, ok: live && m.ok && !m.now, now: m.now, caught: pct100(m.caught), success: m.success === null ? -1 : pct100(m.success), act: `spy:mission:${a.id}:${m.id}` })),
        sees: a.sees.map((x) => ({ region: x.reg, name: s.regions[x.reg]?.name ?? '', nat: x.nat, corps: x.corps, divs: Math.round(x.divs) })),
      })),
      agentMarks: room.notes.filter((n) => n.what === 'caught' && n.turn === s.turn).map((n) => ({ region: n.reg, text: n.text })),
    };
  } catch { return none; }
}

/** (Ruling 163.) Why the player's agent cannot be sent to this province, in the rules' words; empty where he can go. */
function agentWhy(id: number, region: number) {
  try { return { why: whyNotSendAgent(game(), id, region) ?? '' }; } catch { return { why: '' }; }
}

/**
 * (For the photographer only: no start has an agent, and the piece needs its pictures.) Recruits an agent by the
 * rules' own order and sets him at work in a province at once, watching abroad or on guard at home; with `mission`
 * his work there is set by the rules' own order where they allow it. Where the treasury cannot pay, he is written
 * straight into the service's list.
 */
function stageAgent(region: number, mission: string) {
  const s = game();
  const r = s.regions[region];
  if (!r || !s.human) return { ok: false, id: -1, view: view() };
  let a = orderRecruitAgent(s);
  if (!a) {
    a = { id: s.intel.next++, nat: s.human, name: ['Halberd', 'Chalice', 'Lantern', 'Anvil'][s.intel.agents.length % 4], xp: 0, reg: region, ready: s.turn, mission: 'watch', born: s.turn - 2 };
    s.intel.agents.push(a);
  }
  a.reg = region;
  a.ready = mission === 'road' ? s.turn + 1 : s.turn;
  a.months = 1;
  a.mission = allied(s, r.ctrl, s.human) ? 'guard' : 'watch';
  if (mission && mission !== 'road') orderAgentMission(s, a.id, mission as Parameters<typeof orderAgentMission>[2]);
  return { ok: true, id: a.id, view: view() };
}

/** (For the photographer only.) Writes into the service's file that an agent of his was caught in a province this month, in the rules' own sentence, for a picture of the mark left there. */
function stageCaught(region: number) {
  const s = game();
  const r = s.regions[region];
  if (!r || !s.human) return { ok: false, view: view() };
  const host = s.nations[r.ctrl];
  s.intel.notes.push({ turn: s.turn, agent: 'Halberd', nat: s.human, reg: region, what: 'caught', text: `Agent Halberd is caught at ${r.name} by ${host?.adj ?? 'enemy'} counter-intelligence. Nothing more is heard.`, good: false });
  return { ok: true, view: view() };
}

/**
 * (Version 84; rulings 146 and 156.) The weapons a corps carries, for the row of signs on its card and for the weapons
 * that stand on its ring: its anti-tank guns' level (0 to 3), then machine guns, mortars and snipers as 1 or 0. Null
 * for a corps the rule counts none for (tanks, guns, engineers), and outside the grand campaign.
 */
function carriedBy(s: Game, c: Corps): number[] | null {
  try { const k = carried(s, c); return k ? [k.antiTank, k.machineGuns ? 1 : 0, k.mortars ? 1 : 0, k.snipers ? 1 : 0] : null; } catch { return null; }
}

// (Speed, 17 November 2026.) One picture of the war asked for the player's trade book three times over (for the
// lanes on the table, for the pages, and for what wants doing), and the book was the dearest thing in it.
//
// (Speed, second round.) Of the book, the picture uses only the player's own agreements (the gold lanes on the
// table, and a route cut) and, when a trading house stands idle with the political capital to fill it, whether any
// country is ready to sign. The rest of the book, every country's offer with its route and what it would pay, was
// half of the picture's time, and the picture threw it away. So the picture now works out the agreements as the book
// does, line for line (tradeBook, in src/campaign/trade.ts: keep the two the same), and asks after the offers only when
// the list of what wants doing needs to know, stopping at the first country ready. The trade page still gets the whole
// book. While a picture is being drawn up nothing in the war changes, so this is worked out once a picture.
interface PictureTrade {
  used: number;
  houses: number;
  pc: number;
  cost: number;
  deals: { nat: string; pays: number; full: number; state: 'open' | 'raided' | 'cut'; path: [number, number][] }[];
  /** Whether a country would sign now: the book's `offers.some((o) => o.why === null)`. */
  anyReady: () => boolean;
}
let pictureDepth = 0;
let pictureTradeKept: { s: Game; trade: PictureTrade | null } | null = null;
function pictureTrade(s: Game): PictureTrade | null {
  if (pictureDepth > 0 && pictureTradeKept && pictureTradeKept.s === s) return pictureTradeKept.trade;
  const trade = workOutPictureTrade(s);
  if (pictureDepth > 0) pictureTradeKept = { s, trade };
  return trade;
}
function workOutPictureTrade(s: Game): PictureTrade | null {
  // (null exactly when tradeLedger(s) is: no player, or not the grand campaign)
  const nat = s.human;
  const n = nat ? s.nations[nat] : undefined;
  if (!nat || !n || !tradeIsOn(s)) return null;
  const boost = civicSumOf(s, nat).trade;
  const deals = tradeDealsOf(s, nat).map((d) => {
    const o = d.a === nat ? d.b : d.a;
    return { nat: o, pays: d.state === 'cut' ? 0 : d.worth * (1 - d.lost) * (1 + boost), full: d.worth * (1 + boost), state: d.state, path: tradePathOf(s, d) };
  }).sort((x, y) => y.full - x.full || (x.nat < y.nat ? -1 : 1));
  let ready: boolean | null = null;
  const houses = tradeHousesOf(s, nat);
  return {
    used: deals.length, houses, pc: n.pc, cost: TRADE_RULES.sign.pc, deals,
    // (An offer's `why` is null when the rules find nothing in the way and the player has the political capital. A
    // country the book leaves out of the offers, the player's own, one fallen or asleep, one already trading, always
    // has something in the way, so asking every country is the same as asking the offers.)
    // (Speed, b414 mark 2.) whyNoTrade asks after the player's own free trading house for every country it is asked
    // about, and each time works out what his towns add to his houses. Here that is already known (deals.length and
    // houses above, the same dealsOf and housesOf it uses), so it is asked once and whyNoTrade is told to skip it: the
    // same countries come out ready.
    anyReady: () => {
      if (ready === null) {
        const ctx = new Map();
        ready = n.pc >= TRADE_RULES.sign.pc && deals.length < houses && Object.values(s.nations).some((b) => whyNoTradeWith(s, nat, b.id, ctx, false) === null);
      }
      return ready;
    },
  };
}

// (Speed, b414 mark 2.) The depth and the six desks, likewise worked out once a picture: the frame's brief asked
// commandOf once, then once more for each desk's bar, and the list of pages asked for the brief again, fourteen
// times in all. Outside a picture (a desk's own screen, the staff's room) each still asks afresh.
let pictureCommandKept: { s: Game; v: ReturnType<typeof commandOf>; failed: boolean; why: unknown } | null = null;
function pictureCommand(s: Game): ReturnType<typeof commandOf> {
  if (pictureDepth > 0 && pictureCommandKept && pictureCommandKept.s === s) {
    if (pictureCommandKept.failed) throw pictureCommandKept.why;
    return pictureCommandKept.v;
  }
  let v: ReturnType<typeof commandOf>;
  try { v = commandOf(s); } catch (why) {
    if (pictureDepth > 0) pictureCommandKept = { s, v: null, failed: true, why };
    throw why;
  }
  if (pictureDepth > 0) pictureCommandKept = { s, v, failed: false, why: null };
  return v;
}
let pictureBriefKept: { s: Game; brief: ReturnType<typeof workOutCommandBrief> } | null = null;

function view() {
  pictureDepth++;
  try {
    return drawView();
  } finally {
    if (--pictureDepth === 0) { pictureTradeKept = null; pictureCommandKept = null; pictureBriefKept = null; }
  }
}

/**
 * (Release 1.5.1; Brian, 24 November 2026 18:30Z: "let me know when a unit has been delivered".) The units handed over
 * to the player as his new month opened (a nation's builds are handed over as its month begins, startPhase; the rules
 * tag that news 'ready'), for the month's end to tell: what each is, its line of news, where it stands now and, for a
 * corps, which one (one lost since is left out).
 */
function readyOf(s: Game, from: number): { what: string; text: string; region: number; corps: number; name: string }[] {
  if (!s.human) return [];
  const out: { what: string; text: string; region: number; corps: number; name: string }[] = [];
  for (const h of s.headlines.slice(from)) {
    if (!h.ready || h.nat !== s.human) continue;
    const c = h.ready === 'corps' ? s.corps.find((x) => x.id === h.unit && !x.dead && x.nat === s.human) : undefined;
    if (h.ready === 'corps' && !c) continue;
    out.push({ what: h.ready, text: h.text, region: c ? c.reg : h.region ?? -1, corps: c ? c.id : -1, name: c ? c.name : '' });
  }
  return out;
}

function drawView() {
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
    // (Version 64) Where two of the player's corps standing together are worn thin and could be folded into one: the province, and their names.
    folds: s.human && up ? [...new Set(s.corps.filter((c) => !c.dead && c.nat === s.human && c.kind !== 'fort').map((c) => c.reg))].map((reg) => {
      let vs: ReturnType<typeof foldChoicesAt> = [];
      try { vs = foldChoicesAt(s, reg).filter((v) => !v.why); } catch { vs = []; }
      return { region: reg, names: [...new Set(vs.map((v) => v.keepName))] };
    }).filter((x) => x.names.length > 0) : [],
    // (Version 64) Fieldworks standing empty on ground of the player's side: the province, tenths of a month of work, and room for how many corps.
    worksLeft: (() => {
      const out: number[] = [];
      if (s.flags.open !== 1) return out;
      try { for (const r of s.regions) { const v = fieldworksLeft(s, r.id); if (v) out.push(r.id, Math.round(v.months * 10), v.corps); } } catch { /* none */ }
      return out;
    })(),
    // Works going up from the arsenal, for the table to show as a builder's yard in scaffolding: the province, and
    // what is being built there as bits (1 a factory, 2 a shipyard, 4 anti-aircraft guns, 8 an airfield, 16
    // fortifications). Only where the player can know it.
    cityOrders: (() => { try { return cityOrdersOf(s); } catch { return []; } })(),
    going: (() => {
      const at = new Map<number, number>();
      try {
        for (const n of Object.values(s.nations)) {
          for (const b of n.queue) {
            const q = b as unknown as { what?: string; item?: string; region?: number };
            if (q.region === undefined || !s.regions[q.region]) continue;
            const bit = q.what === 'fort' ? 16 : q.what === 'work' ? ({ factory: 1, yard: 2, aa: 4, field: 8 } as Record<string, number>)[q.item ?? ''] ?? 0 : 0;
            if (!bit) continue;
            const r = s.regions[q.region];
            const known = !s.human || r.ctrl === s.human || allied(s, r.ctrl, s.human) || seen[r.id] > 0;
            if (known) at.set(r.id, (at.get(r.id) ?? 0) | bit);
          }
        }
      } catch { /* nothing on order */ }
      return [...at.entries()].flat();
    })(),
    // Factories that are not working (bombed, sabotaged or burnt by their own side), for the table to show as
    // wrecks with cold chimneys: the province and how many. Only where the player can know it: on his own side's
    // ground, or ground he has eyes on.
    wrecked: (() => {
      const out: number[] = [];
      try {
        for (const r of s.regions) {
          if (r.fac <= 0) continue;
          const lost = r.fac - workingFactories(s, r);
          if (lost <= 0) continue;
          const known = !s.human || r.ctrl === s.human || allied(s, r.ctrl, s.human) || seen[r.id] > 0;
          if (known) out.push(r.id, lost);
        }
      } catch { /* no such rule in this game */ }
      return out;
    })(),
    // Cold chimneys (Brian's pick, the idle half): from his second month on, when the player has nothing at all on
    // order and the arsenal has something he could order (the very state the NOTHING IS ON ORDER chip tells of,
    // whoever runs the desk), the stacks of every factory town he holds stand cold, so
    // idle industry shows at a glance. (Orders here are placed with the country's arsenal, not with one factory:
    // counted town by town, all but two or three of a country's towns would stand cold in every month of the war.
    // Other countries' order books are their own secret, so their chimneys say nothing about them.)
    cold: (() => {
      try {
        const me = s.human ? s.nations[s.human] : null;
        if (!me || !me.alive || me.queue.length > 0) return [];
        // (and only where there is an arsenal with something he could order: a game without one has no idle industry to show)
        if (!arsenalOffers(s, -1).some((o) => !o.why)) return [];
        // (the design director's ruling: a new campaign opens on a living map, so not in the player's first month)
        const began = (s.flags as Record<string, number>).began;
        if (began !== undefined && s.turn <= began) return [];
        return s.regions.filter((r) => r.fac > 0 && r.ctrl === me.id).map((r) => r.id);
      } catch { return []; }
    })(),
    // Ground the player has no eyes on (where the fog of war is kept): the map dims it.
    unseen: s.human ? s.regions.filter((r) => seen[r.id] === 0).map((r) => r.id) : [],
    // (Ruling 154.) Ground he only glimpses (the rules' sight 1): the haze stays over it, and an army there is a flag and no more.
    glimpsed: s.human ? s.regions.filter((r) => seen[r.id] === 1).map((r) => r.id) : [],
    // (Rulings 153 and 159.) His agents, each a piece on the map that he alone sees, with everything its card says; and where one of his was caught this month.
    ...agentsOnMap(s, up),
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
    // (Version 66) How deep the player plays: the depth's name, and each desk with who runs it and a line for a bar on
    // its own screen (the staff's news of the month where it runs the desk, an offer to hand it over where it does not).
    command: commandBrief(s),
    fallen: fallenNation,
    // The player's treasury: production points in hand, what the factories earn a month, and what is on order.
    treasury: s.human ? treasuryOf(s, s.human) : null,
    // The countries the player could declare war on this month: those at peace with him that he borders or can reach by sea.
    declare: up ? declarable(s) : [],
    // Bridges that are down (by their place in the list of crossings) and provinces whose railway is cut.
    down: groundRules(s) ? crossingsOf(s).map((c, i) => (bridgeDown(s, c) ? i : -1)).filter((i) => i >= 0) : [],
    // (Rulings 113 and 114: the cuts drawn are the cuts he knows of, with how he knows each and the rules' words for it.)
    ...((): { cut: number[]; cuts: { region: number; how: string; head: string; line: string; short: string }[] } => {
      if (!groundRules(s)) return { cut: [], cuts: [] };
      try {
        const known = [...cutsKnown(s).entries()];
        return {
          cut: known.map(([id]) => id),
          cuts: known.map(([id, k]) => { const said = cutSays(s, id); return { region: id, how: k.how, head: said?.head ?? '', line: said?.line ?? '', short: said?.short ?? '' }; }),
        };
      } catch { return { cut: s.regions.filter((r) => railCut(s, r.id)).map((r) => r.id), cuts: [] }; }
    })(),
    // Each region: who holds it, and the works built there since the war began that the table shows as models:
    // [factories built, shipyard level, anti-aircraft level, airfield level]. Left out where there are none.
    regions: s.regions.map((r) => {
      const w = [factoriesBuilt(s, r.id), yardLevel(s, r), workLevel(s, 'aa', r.id), fields.length ? fieldSize.get(r.id) ?? 0 : Math.max(workLevel(s, 'field', r.id), based.has(r.id) ? 1 : 0)];
      // (The ownership paint: where the holder is not the lawful owner, the owner is named too, for the stripes of occupied ground.)
      const own = r.owner && r.owner !== r.ctrl ? { own: r.owner } : {};
      return w.some((x) => x > 0) ? { id: r.id, ctrl: r.ctrl, stand: standingOf(s, r.ctrl), w, ...own } : { id: r.id, ctrl: r.ctrl, stand: standingOf(s, r.ctrl), ...own };
    }),
    // (The ownership paint: every siege the player knows of, for the besieger's colour creeping over the field. Who
    // stands round the town, how far the siege has gone (the works broken down, or the stores eaten, whichever is
    // further: 0 when it is laid, 1 when the town is about to fall), whether the ring is closed, and the
    // neighbouring provinces the besieger's side holds, which are the sides his colour comes in from.)
    siegePaint: (() => {
      try {
        return siegesInSight(s).filter((v) => v.by).map((v) => {
          const from = (s.regions[v.region]?.nb ?? []).filter((n) => { const c = s.regions[n]?.ctrl; return !!c && (c === v.by || allied(s, c, v.by!)); });
          const done = Math.max(v.broken ?? 0, v.stores > 0 ? 1 - v.left / v.stores : 0);
          return { r: v.region, by: v.by, p: Math.max(0, Math.min(1, done)), closed: v.closed ? 1 : 0, from };
        });
      } catch { return []; }
    })(),
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
        return { id: c.id, nat: c.nat, name: c.name, kind: c.kind, elite: '', divs: c.divs, str: mid, org: 0.5, reg: c.reg, ap: 0, apMax: 0, gen: null, posture: postureOf(c), canOrder: false, supplied: true, men: Math.round(c.divs * 15000 * mid), xp: 0, grade: '', nick: null, honours: [], dim: false, stale: kn.age, strLo: lo, strHi: hi, carry: carriedBy(s, c) };
      }
      return ({
      id: c.id, nat: s.human && c.joint === s.human ? s.human : c.nat, ally: s.human && c.joint === s.human ? c.nat : '', name: c.name, kind: c.kind, elite: c.elite ?? '', divs: c.divs, str: c.str, org: c.org, reg: c.reg,
      ap: c.ap, apMax: c.apMax, gen: generalOf(s, c)?.name ?? null, posture: postureOf(c),
      // (release 1) the province it is marching to on a standing order, or -1, and about how many months it has still to go
      marchTo: marchOf(s, c), marchMonths: marchState(s, c)?.months ?? 0,
      // (One voice after a press, 17 November) and whether that march is still to set off, as the next month opens
      marchNext: marchOf(s, c) >= 0 && s.flags[`marchLegs:${c.id}`] === 0,
      canOrder: up && canOrder(s, c), noOrders: whyNotOrder(s, c) ?? '', supplied: c.supplied, dug: !!c.dug, dugFor: (c as { dugFor?: number }).dugFor ?? 0,
      // (Version 61) how far its works have got, in months, where months are counted: attacks wear them down. -1 where they are not.
      works: c.dug && s.flags.open === 1 && DEFENCE.entrench.auto ? Math.round(worksOf(c) * 10) / 10 : -1, town: !!(c as { town?: unknown }).town, men: men(c),
      // (Rules 84.2.4; the UI director, 17 November.) A corps of his own inside a town: the rules' refusal, naming the town, for its card.
      garrison: (c as { town?: unknown }).town && c.nat === s.human ? (() => { try { return garrisonLine(s, [c]); } catch { return ''; } })() : '',
      // Its story so far: how seasoned it is, the name the army gives it, and the honours on its colours.
      xp: Math.round(c.xp * 100), grade: grade(c.xp), nick: c.nick ?? null,
      honours: honoursOf(s, c).filter((a) => !a.old).map((a) => ({ title: awardTitle(s, a), ribbon: honourDef(a.hon)?.ribbon ?? ['#b08d57', '#f2ead6'] })),
      dim: false,
      // (Version 84) the weapons it carries; and whether it has begun to dig where the first month's digging shows no works (the open desert)
      carry: carriedBy(s, c), digging: !c.dug && ((c as { digging?: number }).digging ?? 0) > 0,
      // The player's own corps as its card reads: its divisions by name, its morale and its rest, each with why.
      lean: !!c.lean,
      // (The 1.5 patch, breakup.ts; Brian 24 November 2026 18:02Z) Where breaking corps up is on: why this corps cannot have
      // divisions split off now ('' if it can), and the corps it could rejoin here, the first of them.
      ...(c.nat === s.human && breakUpOn(s) ? (() => {
        const to = up ? rejoinTargets(s, c)[0] : undefined;
        return { breakWhy: up ? whyNotBreakUp(s, c, [0]) ?? '' : 'It is not your month.', rejoinId: to?.id ?? -1, rejoinName: to?.name ?? '' };
      })() : {}),
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
      // (1.5.2) A fleet cut off beyond a shut strait reaches no sea: only a harbour on its side (bases).
      reach: f.nat === s.human && up && !cutOff(s, f) ? fleetReach(s, f) : [],
      // (Ruling 86.) And the seas it would reach but for a gate shut to it, each with that gate: sea, gate, sea, gate.
      barred: f.nat === s.human && up ? barredSeas(s, f) : [],
      // And the friendly ports it could make its new home. (1.5.1) None for a fleet held where its battle left it this
      // month: the rules refuse it a new base, so no port is tinted to offer one (its seas still say why when hovered).
      bases: f.nat === s.human && up && !heldWhy(s, f) ? baseTargets(s, f).map((r) => r.id) : [],
      };
    }),
    // (Ruling 86.) The sea gates: where each mark stands, who holds it, and whether the player's ships may pass.
    gates: gatesOf(s),
    // (Rulings 88, 101 and 102.) The round of armies, counted by the rules for both builds.
    round: roundOf(s),
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
        // (With medium tanks held the year is 1942 at once: the game shows the medium tank from that month, and no
        // country's own models change between 1939 and 1942.)
        const armour = !hasTech(s, n.id, 'mediums') ? 1939 : !hasTech(s, n.id, 'heavies') ? 1942 : Math.max(year, 1943);
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
    // (Rules 84.2.6; ruling 211(c).) The alarm that names the cure: the red row first in the tray. Empty until the rules have it.
    alarms: alarmsOf(s),
    // (Rules 84.2.6.) His own guns left alone round an enemy town: the siege's row in the rules' words. Empty until the rules have it.
    alone: gunsAloneOf(s),
    // For the night: the nations at war, whose towns are blacked out; and the fronts, where armies at war stand in
    // provinces side by side, as far as the player sees them (pairs of regions), for the guns that flicker there.
    // (release 1) What each arm is, for the bigger unit card.
    arms: armsTable(s),
    // (Version 70) The history dial as it stands: null outside the grand campaign, and where the rules have none.
    dial: (() => { const d = dialNow(s); return d ? { stop: d.stop, name: d.name, under: d.under } : null; })(),
    // (Version 72) The sea lift in a few words, for the heading of the fleet card: empty where the rules have none.
    lift: (() => { try { const l = seaCalls.lift ? seaCalls.lift(s) : null; return l ? (l.lift > 0 ? `sea lift for ${l.lift} corps a month, ${l.left} left` : 'no sea lift') : ''; } catch { return ''; } })(),
    // (release 1) The admiral's chart: each sea by how well it is searched, and old sightings.
    chart: chartOf(s),
    // (release 1) Wrecks on the chart: where the big ships went down.
    wrecks: wrecksOf(s),
    dark: Object.values(s.nations).filter((n) => n.alive && n.atWar.length > 0).map((n) => n.id),
    // For the drawn front line (release 1): every pair of living nations at war, tag by tag, two by two. The table
    // draws the line itself along every frontier between two such holders, so it moves the moment ground changes hands.
    wars: (() => {
      const out: string[] = [];
      for (const n of Object.values(s.nations)) {
        if (!n.alive) continue;
        for (const b of n.atWar) if (n.id < b && s.nations[b]?.alive) out.push(n.id, b);
      }
      return out.slice(0, 400);
    })(),
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
    // (Version 59) The town a directive of the war cabinet names, and the months left: for a small seal on the map.
    seal: (() => { try { const d = governmentOf(s)?.directive; return d && d.region !== undefined && d.region >= 0 ? { region: d.region, left: d.left } : null; } catch { return null; } })(),
    // (Version 60) And the town the player has promised the cabinet, with the months left: a second seal.
    vow: (() => { try { const pr = (governmentOf(s) as { promise?: { region?: number; left: number } | null } | null)?.promise; return pr && pr.region !== undefined && pr.region >= 0 ? { region: pr.region, left: pr.left } : null; } catch { return null; } })(),
    // Towns under siege that the player knows of (Version 54): region, 1 if the ring is closed, months of stores left.
    // (Brian's ask of 16 November 2026: a city or target that is bombed stands under black smoke through the next
    // month.) The provinces bombed this month or last, where the player can know of it: his own side's raid, a raid
    // on his own side, or ground he has eyes on.
    bombed: (() => {
      try {
        // (pairs: the province, and what the raids on it were worth on the rules' own scale, where one aircraft is 1.
        // The art director's brief: smoke only where a raid did damage. The design director's ruling 66: it shows
        // nothing the player could not know already.)
        const out = new Map<number, number>();
        for (const r of s.airReports) {
          const reg = s.regions[r.region];
          if (!reg || r.turn < s.turn - 1 || !['bomb', 'night', 'town', 'harbour', 'ground', 'line'].includes(r.mission as string)) continue;
          const known = !s.human || r.attNat === s.human || r.defNat === s.human || allied(s, r.attNat, s.human) || allied(s, r.defNat, s.human) || reg.ctrl === s.human || seen[reg.id] > 0;
          const worth = r.worth ?? (r.won ? 1 : 0);
          if (known && worth > 0) out.set(reg.id, (out.get(reg.id) ?? 0) + worth);
        }
        return [...out].flat();
      } catch { return []; }
    })(),
    sieges: (() => { try { return siegesInSight(s).filter((v) => v.by).flatMap((v) => [v.region, v.closed ? 1 : 0, v.left]); } catch { return []; } })(),
    // (Damaged cities) Each damaged town the player can know: pairs of the region and the percent of it in ruins
    // (to a whole percent), for the table to unroof that share of its houses. Empty in rules without them.
    damaged: (() => {
      try {
        return (repairCalls.marks?.(s) ?? []).filter((t) => !s.human || allied(s, t.holder, s.human) || seen[t.region] > 0).flatMap((t) => [t.region, Math.max(1, Math.round(t.damage * 100))]);
      } catch { return []; }
    })(),
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
    // (Version 59) The reserve of men: how much of what the war opened with is left, in a word and in a sentence.
    men: (() => {
      try {
        const m = manpowerOf(s);
        if (!m) return null;
        const words = (n: number): string => (n >= 1000000 ? `${(n / 1000000).toFixed(1).replace(/\.0$/, '')} million` : Math.round(n / 1000) * 1000 >= 1000 ? `${(Math.round(n / 1000) * 1000).toLocaleString('en-US')}` : `${Math.round(n)}`);
        return {
          share: Math.max(0, Math.min(1, m.share)), word: m.word, short: m.word === 'Short' || m.word === 'Spent',
          tip: [m.line, `That is enough for about ${Math.floor(m.divs)} division${Math.floor(m.divs) === 1 ? '' : 's'}.`,
            `Each month ${words(m.classMen)} more come of age${m.home < 0.995 ? `, from the ${Math.round(m.home * 100)}% of the homeland still in your hands` : ''}.`,
            `Drawn since the campaign opened: ${words(m.drawn)}.`, 'Every new corps and every replacement comes out of this reserve. When it is spent, nothing new can be raised.', 'Click for the count, and your say over the call-up.'].join('\n'),
        };
      } catch { return null; }
    })(),
    // (Version 59) The war cabinet can be switched off by a player who wants the war without the politics.
    // The countries whose armies fight beside the player's: a province crowded with friendly flags shows one, with a count.
    allied: s.human ? Object.keys(s.nations).filter((n) => n !== s.human && s.nations[n].alive && allied(s, n, s.human!)) : [],
    cabinet: (() => { try { const off = cabinetIsOff(s); return s.human && s.flags.open === 1 ? { can: true, off, secure: officeIsSecure(s) } : { can: false, off: false, secure: false }; } catch { return { can: false, off: false, secure: false }; } })(),
    lanes: s.flags.open === 1 ? [
      ...shippingLanes(s).filter((l) => l.kind === 'sea' && l.path.length > 1 && l.state !== 'idle').map((l) => ({ path: l.path.flat(), state: l.state, flow: l.flow })),
      // The player's trade routes, over land and sea, in gold: red where one is cut.
      ...(pictureTrade(s)?.deals ?? []).filter((d) => d.path.length > 1).map((d) => ({ path: d.path.flat(), state: d.state === 'open' ? 'trade' : `trade-${d.state}`, flow: d.full > 0 ? d.pays / d.full : 1 })),
    ] : [],
    // (The 1.5 patch, breakup.ts) Whether the player can split divisions off his corps, and put them back.
    breakUp: breakUpOn(s),
    // (The 1.5 patch) A grand campaign, where the OPTIONS row for it is shown (on or off).
    breakUpCan: s.flags.open === 1,
    // (The 1.5.1 patch, fleetattack.ts) Whether the player's fleets can be dropped on an enemy fleet to attack it at once.
    fleetAttack: fleetAttackOn(s),
    // (The 1.5.1 patch) A grand campaign, where the OPTIONS row for it is shown (on or off).
    fleetAttackCan: s.flags.open === 1,
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
    orders: ((book) => s.nations[nat].queue.map((b) => orderLine(s, b, book)))(ordersBookOf(s)),
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
  const orders = ((book) => s.nations[me].queue.map((b) => ({ ...orderLine(s, b, book), cost: shown(b.cost) })))(ordersBookOf(s));
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

// ---- What each city has on order (Brian's ask of 15 November 2026; the design director's rulings 46 and 57) ----
// (The rules' own shared call of Version 78, `cityOrders` in orders.ts: one working-out for the browser page and for
// this game. The badge, the pointer's list and the city card all read it through here. Ruling 57: the sign is the
// unit ready soonest; the count is the orders there of exactly what the sign shows; the plus says other kinds of
// unit are on order there too; works take no badge.)

/** The rules' book of what is on order place by place, as the player may know it. */
function ordersBookOf(s: Game): CityBook[] {
  const me = s.human;
  return me && s.nations[me]?.alive ? ordersBook(s, me) : [];
}

/**
 * Every place with something on order that the player may know of, for its one badge and for the pointer's list:
 * his own, and another country's where an agent of his reports it. A place with only works under way has no sign.
 */
function cityOrdersOf(s: Game) {
  return ordersBookOf(s).map((c) => ({
    region: c.region, sign: c.badge?.mark ?? '', star: !!c.badge?.starred, count: c.badge?.count ?? 0, plus: !!c.badge?.plus,
    months: c.badge?.months ?? c.months, total: c.units, spied: c.spied,
    head: `${c.spied ? 'Your agent reports: ' : ''}${c.summary}`,
    lines: c.lines.map((l) => `${upFirst(l.label)}, ready ${readyWhen(l.months)}`),
  }));
}

/** Something on order, as a line: what, where, when it is due, and whether the order can still be taken back. */
function orderLine(s: Game, b: Build, book: CityBook[] = ordersBookOf(s)) {
  const months = Math.max(0, b.eta - s.turn);
  // (for the city card's "Forming here": its sign, what it is, and how far along it is, as the rules have them)
  const shownAs = book.find((c) => c.region === b.region && !c.spied)?.lines.find((l) => l.id === b.id);
  return {
    id: b.id, what: b.what, name: b.name, region: b.region, where: s.regions[b.region]?.name ?? '',
    due: turnToDate(b.eta).label, months, cost: b.cost, canCancel: b.ordered === s.turn && humanUp(s),
    sign: shownAs?.mark ?? '', star: !!shownAs?.elite, words: upFirst(shownAs?.label ?? b.name),
    share: shownAs ? Math.min(1, Math.max(0, shownAs.done / shownAs.need)) : b.eta > b.ordered ? Math.min(1, Math.max(0, (s.turn - b.ordered) / (b.eta - b.ordered))) : 1,
  };
}

const corpsById = (s: Game, ids: number[]): Corps[] =>
  ids.map((id) => s.corps.find((c) => c.id === id)).filter((c): c is Corps => !!c && !c.dead);

// ---- A garrison sent somewhere comes out of its town on the way (release 2, and the 1.5.1 patch) ----------------
// (Brian, 24 November 2026: "if a unit is dug in and i click them to attack and move I want them to over ride their
// orders insted of me having to press a button".) The corps of the player's hand that are inside a town come out as
// part of the order's own call, as TAKE THE FIELD would bring them (one move), and each stays out only if it went: a
// corps that after the call has not moved, spent a move, fallen or been given a march to keep (a refusal, or one left
// behind by the order, as one that went in this month and has no move once out) is put back inside, with nothing
// spent. The calls that only look (the reach, the march's plan, the odds, the march onto empty ground, guns alone, the
// general's battle) see them as if out and always put them back, so the 3D game can show and ask about an order
// before anything is carried out (the 1.5 release lead, 18:42Z: a No at a card, the war question's No or a refusal
// must leave them inside). The corps inside are judged together: where they cannot all come out (room outside for one
// of two), none does, and the call is made with them inside, as before, so that it says the rules' own sentence (the
// city glance thread's pre-check, 18:42Z). Putting one back touches that corps and nothing else (the release lead,
// 18:56Z): it is taken out of its province's field by identity and set back at its old place in the town, its town and
// moves as they were, and the corps' and the province's keys in their old order, so that a look leaves the save the
// same to the byte and an order leaves whatever else it did as it did it.
/** A corps of the garrison as it stood before it came out. */
type Inside = { c: Corps; r: Region; at: number; town: number; ap: number; keys: string[]; regionKeys: string[]; march: unknown };
const marchKey = (c: Corps): string => `march:${c.id}`;
const insideNow = (s: Game, c: Corps): Inside => {
  const r = s.regions[c.reg];
  return { c, r, at: r.garr ? r.garr.indexOf(c) : -1, town: c.town!, ap: c.ap, keys: Object.keys(c), regionKeys: Object.keys(r), march: s.flags[marchKey(c)] };
};
/** An object's keys set back in an earlier order (keys since gone stay gone, new ones come after), its values as they are now. */
const keysInOrder = (o: object, keys: string[]): void => {
  const r = o as Record<string, unknown>, now = { ...r };
  for (const k of Object.keys(r)) delete r[k];
  for (const k of keys) if (k in now) r[k] = now[k];
  for (const k of Object.keys(now)) if (!(k in r)) r[k] = now[k];
};
/** Puts corps that came out back inside their towns, each at its old place, touching nothing else. */
const backInside = (xs: Inside[]): void => {
  for (const x of [...xs].sort((p, q) => p.at - q.at)) {
    const { c, r } = x;
    const k = r.units.indexOf(c);
    if (k >= 0) r.units.splice(k, 1);
    const garr = (r.garr ??= []);
    if (!garr.includes(c)) garr.splice(Math.min(Math.max(x.at, 0), garr.length), 0, c);
    c.reg = r.id;
    c.town = x.town;
    c.ap = x.ap;
    keysInOrder(c, x.keys);
    keysInOrder(r, x.regionKeys);
  }
};
/** True where a corps that came out went with the order: it moved, spent a move, fell, went back in, or was given a march to keep. */
const wentOut = (s: Game, x: Inside, out: { reg: number; ap: number }): boolean =>
  x.c.dead || x.c.reg !== out.reg || x.c.ap !== out.ap || x.c.town !== undefined || s.flags[marchKey(x.c)] !== x.march;
/** The player's corps among these ids that stand inside a town. */
const insideOf = (s: Game, ids: unknown): Corps[] => {
  try { return Array.isArray(ids) && s.human ? corpsById(s, ids as number[]).filter((c) => c.town !== undefined && c.nat === s.human) : []; } catch { return []; }
};
/**
 * Makes a call with the garrison in hand out of its town. 'looks' calls always put it back; after an order, each corps
 * that did not go is put back. Anything not a garrison, or a garrison that cannot all come out, is called as it stands.
 */
function comeOut<R>(ids: unknown, call: () => R, looks: boolean): R {
  const s = game();
  const inside = insideOf(s, ids).map((c) => insideNow(s, c));
  if (!inside.length) return call();
  const done: Inside[] = [];
  for (const x of inside) {
    if (!orderTakeField(s, x.c)) { backInside(done); return call(); }
    done.push(x);
  }
  const out = inside.map((x) => ({ reg: x.c.reg, ap: x.c.ap }));
  const settle = (): number => {
    const back = looks ? inside : inside.filter((x, i) => !wentOut(s, x, out[i]));
    backInside(back);
    return back.length;
  };
  let result: R;
  try { result = call(); } catch (e) { settle(); throw e; }
  const put = settle();
  // (The 1.5 pre-check, 24 Nov 19:11Z.) An order's answer carries the picture of the war drawn inside the call, with
  // the garrison still out; the 3D game takes it as the map's (TakeView). Where any corps was put back (a refusal, or
  // one the order left behind), the picture is drawn again, with it inside its town and its moves as they were.
  if (!looks && put > 0 && result && typeof result === 'object' && 'view' in result) (result as { view: unknown }).view = view();
  return result;
}
/** A call that only looks, made with the garrison in hand as if out: put back after. */
const lookOut = <A extends unknown[], R>(fn: (...a: A) => R) => (...a: A): R => comeOut(a[0], () => fn(...a), true);
/** An order, made with the garrison in hand out of its town: it stays out if the order went. */
const goOut = <A extends unknown[], R>(fn: (...a: A) => R) => (...a: A): R => comeOut(a[0], () => fn(...a), false);


/** One formation seen to fly a raid: where from, where to, whose, what flies, and what it meets. */
interface Flight { from: number; x: number; y: number; nat: string; bombers: boolean; escort: boolean; region: number; aa: number; meet: string; meetFrom: number; loss: number; fromX?: number; fromY?: number; fleet?: number; /** (Ruling 107.) Flown by night: the place raided shows its searchlights. */ night?: boolean }

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
          night: (r as { mission?: string }).mission === 'night',
        });
        first = false;
      }
      if (first) continue;
      // (every raid takes its slot, as in 1.5.1, empty or not: the callers drop the empty ones after taking their two;
      // 1.5.2, playtest #38: trimmed, so a joined note never opens with a space)
      notes.push(r.result.trim());
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
function seaFightsSince(s: Game, since: number): { zones: number[]; notes: string[]; mine: boolean; sunk: { zone: number; nat: string; cls: string; n: number; ships?: { name: string | null; cls: string; crew: number; lost: number; pair?: boolean; type?: 'battlecruiser' }[] }[] } {
  const fresh = s.seaReports.filter((r) => r.id > since);
  const adj = (nat: string): string => s.nations[nat]?.adj ?? nat;
  // (release 1) Ships seen to sink: in a fight the player's side took part in he knows what went down, on both
  // sides. Each loss is listed in the fight's own note and shown on the table, and the biggest ship a side lost
  // (a battleship, a carrier or a cruiser) leaves a wreck on the chart, kept with the game as a flag.
  const me = s.human;
  const ours = (nat: string): boolean => !!me && (nat === me || allied(s, nat, me));
  const nats = Object.keys(s.nations);
  const sunk: { zone: number; nat: string; cls: string; n: number; ships?: { name: string | null; cls: string; crew: number; lost: number; pair?: boolean; type?: 'battlecruiser' }[] }[] = [];
  const lostIn = new Map<number, string>();
  try {
    for (const r of fresh) {
      const sides = [...r.att, ...r.def];
      if (!sides.some((x) => ours(x.nat))) continue;
      const said: string[] = [];
      sides.forEach((side, i) => {
        const parts: string[] = [];
        for (const cls of ['bb', 'cv', 'ca', 'dd', 'ss']) {
          const n = (side.sunk as unknown as Record<string, number>)[cls] ?? 0;
          if (n <= 0) continue;
          // (Brian's memorial card) The battleships and carriers lost, by name, with their crews and the men lost (the rules' 'hulls').
          const ships = cls === 'bb' || cls === 'cv' ? (side.hulls ?? []).filter((h) => h.cls === cls) : [];
          sunk.push({ zone: r.zone, nat: side.nat, cls, n, ...(ships.length ? { ships } : {}) });
          parts.push(`${n} ${SHIP_WORD[cls][n === 1 ? 0 : 1]}`);
        }
        if (parts.length) said.push(`${adj(side.nat)} ships sunk: ${parts.join(', ')}.`);
        const big = ['bb', 'cv', 'ca'].find((cls) => ((side.sunk as unknown as Record<string, number>)[cls] ?? 0) > 0);
        const who = nats.indexOf(side.nat);
        if (big && who >= 0) s.flags[`wreck:${r.id}:${i}`] = ((r.turn * 100 + who) * 10 + (big === 'bb' ? 1 : big === 'cv' ? 2 : 3)) * 1000 + r.zone;
      });
      if (said.length) lostIn.set(r.id, said.join(' '));
    }
  } catch { /* a fight whose losses cannot be read is told without them */ }
  return {
    zones: [...new Set(fresh.map((r) => r.zone))],
    notes: fresh.map((r) => `${adj(r.attNat)} and ${adj(r.defNat)} ships fight in the ${r.zoneName}: ${r.won ? `the ${adj(r.attNat)} fleet has the better of it` : `the ${adj(r.defNat)} fleet holds the sea`}.${lostIn.has(r.id) ? ` ${lostIn.get(r.id)}` : ''}`),
    mine: fresh.some((r) => r.attNat === s.human || r.defNat === s.human),
    sunk,
  };
}

/** (Brian's memorial card) The ships sunk by strikes from the air since a report (by its id): carrier battles and raids on a harbour, where the player's side was in it, as the sea fights give them (a harbour's by its province, 'region', with 'zone' -1). */
function strikeLossesSince(s: Game, since: number): { zone: number; region?: number; nat: string; cls: string; n: number; ships?: { name: string | null; cls: string; crew: number; lost: number; pair?: boolean; type?: 'battlecruiser' }[] }[] {
  const me = s.human;
  const ours = (nat: string): boolean => !!me && (nat === me || allied(s, nat, me));
  const out: { zone: number; region?: number; nat: string; cls: string; n: number; ships?: { name: string | null; cls: string; crew: number; lost: number; pair?: boolean; type?: 'battlecruiser' }[] }[] = [];
  try {
    for (const r of s.airReports) {
      if (r.id <= since || !r.ships?.length || !(ours(r.attNat) || r.ships.some((x) => ours(x.nat)))) continue;
      for (const side of r.ships) {
        for (const cls of ['bb', 'cv', 'ca', 'dd', 'ss']) {
          const n = (side.sunk as unknown as Record<string, number>)[cls] ?? 0;
          if (n <= 0) continue;
          const ships = cls === 'bb' || cls === 'cv' ? (side.hulls ?? []).filter((h) => h.cls === cls) : [];
          out.push({ zone: r.zone, ...(r.zone < 0 ? { region: r.region } : {}), nat: side.nat, cls, n, ...(ships.length ? { ships } : {}) });
        }
      }
    }
  } catch { /* a strike whose losses cannot be read is shown without them */ }
  return out;
}

/** (release 1) The wrecks on the chart: where a battleship, a carrier or a cruiser went down in a fight the player's side was in. The latest forty. */
function wrecksOf(s: Game): { zone: number; nat: string; cls: string; turn: number; seed: number }[] {
  const out: { zone: number; nat: string; cls: string; turn: number; seed: number }[] = [];
  try {
    const nats = Object.keys(s.nations);
    for (const key of Object.keys(s.flags)) {
      if (!key.startsWith('wreck:')) continue;
      const v = s.flags[key];
      const zone = v % 1000, a = Math.floor(v / 1000), cls = a % 10, b = Math.floor(a / 10), nat = nats[b % 100], turn = Math.floor(b / 100);
      const bits = key.split(':');
      if (!nat || !s.seas[zone]) continue;
      out.push({ zone, nat, cls: cls === 1 ? 'bb' : cls === 2 ? 'cv' : 'ca', turn, seed: Number(bits[1]) * 2 + Number(bits[2]) });
    }
  } catch { /* none to show */ }
  return out.sort((x, y) => y.turn - x.turn || y.seed - x.seed).slice(0, 40);
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
    const raidNote = air.notes.slice(0, 2).filter(Boolean).join(' ');
    const sea = seaFightsSince(s, sea0);
    const shore = shellingSince(s, shore0);
    const sunk = [...sea.sunk, ...strikeLossesSince(s, air0)];
    const seaNote = [...sea.notes.slice(0, 2), ...shore.notes.slice(0, 1)].join(' ');
    const taken = s.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = s.reports.slice(r0);
    if (!taken.length && !reps.length) {
      // Nothing was fought for or taken, but armies the player can see may have marched: that is shown too.
      const seen = sight(s);
      const to = [...new Set(s.corps.filter((c) => !c.dead && c.nat === nat && stood.has(c.id) && stood.get(c.id) !== c.reg && (seen[c.reg] > 0 || seen[stood.get(c.id)!] > 0)).map((c) => c.reg))];
      if (air.flights.length) steps.push({ nat, note: [raidNote, seaNote].filter(Boolean).join(' '), taken: [], battles: [], focus: air.focus.length ? air.focus : air.flights.map((f) => f.from), mine: air.mine || sea.mine || shore.mine, raids: air.flights, seas: sea.zones, sunk, shelled: shore.regions, view: view() });
      else if (sea.zones.length || shore.regions.length) steps.push({ nat, note: seaNote, taken: [], battles: [], focus: shore.regions, mine: sea.mine || shore.mine, seas: sea.zones, sunk: sea.sunk, shelled: shore.regions, view: view() });
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
    // (Version 63) Where the player's own troops fought, the price of the costliest battle: men lost on each side, and the share of those sent in.
    const own = reps.filter((r) => !r.yielded && !!s.human && (r.defenders.some((u) => u.nat === s.human) || r.attackers.some((u) => u.nat === s.human)))
      .sort((a, b) => b.attLost + b.defLost - (a.attLost + a.defLost));
    const price = own.length ? ` At ${own[0].regionName} ${priceLine(s, own[0]).replace(/^I/, 'i')}` : '';
    steps.push({
      nat,
      note: `${s.nations[nat].adj} forces ${said.join(', ')}.${price}${shore.notes.length ? ` ${shore.notes[0]}` : ''}${raidNote ? ` ${raidNote}` : ''}`,
      taken,
      raids: air.flights,
      seas: sea.zones,
      sunk,
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
  // (The code checker, 17 November.) A country can fall in what its own month's opening settles: caught here too, as
  // the browser catches it, and handled as a fall before it is.
  if (!lost && s.human && !s.nations[s.human].alive) {
    const late = s.human;
    fallenNation = late;
    if (!playerFell(s)) s.human = null;
    while (s.phase > 0 && !s.human) stepAI(s);
    return late;
  }
  return lost;
}

/** The places a campaign can begin, with each playable nation's aims for the campaigns that start in September 1939. */
function starts() {
  return {
    years: GRAND.years.map((y) => ({ year: String(y.year), title: y.date.replace(/^1 /, ''), from: y.turn, note: y.text })),
    // How hard a grand campaign can be made, easiest first, and the level it is on unless another is chosen.
    levels: DIFFICULTY.levels.map((l) => ({ id: l.id, name: l.name, blurb: l.blurb, lines: l.lines })),
    level: DIFFICULTY.start,
    // (Version 66) How deep a grand campaign is played: which desks the player's staff runs for him. The one offered first.
    depths: DEPTH.depths.map((d) => ({ id: d.id, name: d.name, under: d.under, blurb: d.blurb })),
    depth: DEPTH.start,
    // (Version 70) The history dial's five stops, and the one a campaign opens on: none where the rules have no dial.
    histories: dialStops().map((x) => ({ stop: x.stop, id: x.id, name: x.name, under: x.under, blurb: x.blurb, lines: x.lines })),
    history: 3,
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
/**
 * Whether the grand campaign is played on the wider world (rules version 74: North America and its seas) or kept to
 * the map of Europe. The 3D table says which, when a campaign is started: it is the wider world only where the table
 * has that world's provinces laid on it.
 */
let wideWorld = false;

function grandGame(from: number, seed: number, human: string | null): Game {
  // September 1939 is free play from the first day. A later year is history up to the day the player takes
  // over, and from that day no nation follows history's plan: each one's staff decides for itself.
  if (from <= 1) return startOpen(createGame(seed, human), wideWorld);
  return startOpen(historyTo(from, seed), wideWorld);
}

/**
 * Who can be led in the grand campaign from a month: every country still standing. The great powers named in
 * the shared data come first, each with the data's word about what the player is taking on that year, and
 * are offered only in the years the data gives them. Every other country is offered whenever it is still there.
 */
function grand(from = 1, seed = 1939, world = false) {
  wideWorld = world === true;
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
        standing: foes.length ? theWords(`At war with ${listNames(foes.map((f) => h.nations[f].name))}`) : 'At peace',
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
function start(options?: { human?: string | null; from?: number; view?: string; seed?: number; level?: string; secure?: boolean; depth?: string; history?: number; world?: boolean; first?: boolean }) {
  wideWorld = options?.world === true;
  const human = options?.human === undefined ? 'GER' : options.human;
  const from = options?.from ?? 1;
  const seed = options?.seed ?? 1939;
  swapAsked = ''; wreckAsked = ''; // (nothing asked in the campaign being left is remembered in this one)
  fallenNation = null;
  lastMonth = null;
  lastReel = -9;
  monthBook = null;
  lastCard = null;
  // Air campaigns are set right after startOpen, as the war runners set them (TC_AC=1), so a war started here is the
  // war the gates measured.
  if (options?.view === 'grand') {
    g = grandGame(from, seed, human);
    openAirCampaigns(g);
  } else {
    g = createGame(seed, from > 1 ? null : human);
    if (options?.view === 'open') { startOpen(g, false); openAirCampaigns(g); }
    while (g.turn < from) runTurnAI(g);
  }
  if (options?.view === 'grand' || options?.view === 'open') { try { repairCalls.on?.(g, true); } catch { /* rules without damaged cities */ } }
  g.human = human;
  // How hard the war is to be (the grand campaign only): set once the player has his seat.
  if (human && options?.level) orderLevel(g, options.level);
  // (Version 62) A government that cannot fall, chosen as the campaign is set up.
  if (human && options?.secure) { try { orderOffice(g, false); } catch { /* no government in this game */ } }
  // (Version 66) How deep he plays: a depth sets the desks his staff runs, the cabinet, whether it can fall, and the fog of
  // war. The staff goes to work when his first month opens. A game started with no depth is the full game.
  if (human && options?.view === 'grand' && options?.depth) { try { orderDepth(g, options.depth); } catch { /* no depths in this game */ } }
  // (Version 70) The history dial, for a campaign that starts at a stop other than the third: a watched war included.
  if (options?.view === 'grand' && options?.history && options.history !== 3) { try { dialCalls.order?.(g, options.history, true); } catch { /* no dial in these rules */ } }
  // The month the player took his seat, kept with the game (the chimneys of an idle country stand cold only from his second month).
  if (human) (g.flags as Record<string, number>).began = g.turn;
  // (Rules 84.3.3) The campaign FIRST COMMAND opened, marked as the browser marks it: the notes teach in any campaign
  // while they are on, and the Guide lists them by its months in this one.
  if (human && options?.first) (g.flags as Record<string, number>).firstCommand = 1;
  lessonsBegin(!!human && !!options?.first);
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
  // (Release 1.5.1, new units ready; development only) "-TheaterReadyDemo" sets TC_READY_DEMO: the player's month
  // opens with one infantry corps on order at his capital, paid for as usual and due next month, so that the Mac's
  // check can see it handed over.
  if (human && (globalThis as { TC_READY_DEMO?: unknown }).TC_READY_DEMO && humanUp(g)) {
    const n = g.nations[human];
    const b = n && n.capital >= 0 ? orderCorps(g, human, 'inf' as Kind, n.capital) : null;
    if (b) b.eta = g.turn + 1;
  }
  try { openMonthBook(g); } catch { monthBook = null; }
  return view();
}

/** The whole campaign as text, to be kept and picked up again later. */
function save() {
  const s = game();
  // (1.5.2, playtest #32.) The month's report is the table's, not the rules': it is written into the save beside the
  // rules' flags only while the save is made, and taken out again as the save is loaded, so the war itself never
  // carries it and a save and load gives back the same text.
  if (!lastMonth && !lastCard) return { text: saveGame(s) };
  const flags = s.flags as Record<string, unknown>;
  if (lastMonth) flags.monthReport = lastMonth;
  if (lastCard) flags.monthCard = lastCard;
  try { return { text: saveGame(s) }; } finally { delete flags.monthReport; delete flags.monthCard; }
}

/** The month's report and its card a save carries (see save), each null where the save has none or it does not read right. */
function reportOfSave(s: Game): { report: typeof lastMonth; card: MonthCard | null } {
  const flags = s.flags as Record<string, unknown>;
  const m = flags.monthReport as typeof lastMonth | undefined;
  const c = flags.monthCard as MonthCard | undefined;
  delete flags.monthReport;
  delete flags.monthCard;
  const fine = !!m && typeof m.date === 'string' && Array.isArray(m.rows) && Array.isArray(m.news)
    && m.rows.every((r) => r && typeof r.nat === 'string' && typeof r.note === 'string' && typeof r.region === 'number' && r.region < s.regions.length)
    && m.news.every((x) => typeof x === 'string');
  const card = !!c && typeof c === 'object' && typeof c.date === 'string' && Array.isArray(c.staff);
  return { report: fine ? m! : null, card: card ? c! : null };
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
  airCampaignsOnLoad(g);
  startGunsLoaded(g);
  lessonsBegin(false);
  swapAsked = ''; wreckAsked = ''; // (nothing asked in the campaign being left is remembered in this one)
  fallenNation = null;
  const kept = reportOfSave(g);
  lastMonth = kept.report;
  lastReel = -9;
  monthBook = null;
  lastCard = kept.card;
  if (g.human && !humanUp(g)) {
    runOthers(g, null);
    arrive(g);
  }
  return view();
}

// ---- The history dial (Version 70) ---------------------------------------------------------------
// One setting of the grand campaign with five stops, from "By the book" to "Anything goes": how far the computer's
// countries may move away from what history did. Its calls are asked for by name, so that this bridge builds on
// the rules before Version 70 as well: there the dial is simply not there.

interface DialStop { stop: number; id: string; name: string; under: string; blurb: string; lines: string[] }
interface DialNow { stop: number; name: string; under: string; blurb: string; lines: string[]; loosest: number; loosestName: string; closed: { theatre: string; name: string; turn: number }[]; locked: string | null; expects?: { theatre: string; name: string; turn: number; text: string }[] }
interface ClaimRow { id: string; nat: string; on: string; name: string; why: string; ground: string[]; state: 'claims' | 'pressing' | 'marched'; since: number; onYou: boolean }
const rulesByName: Record<string, unknown> = { ...everyRule };
const dialCalls = {
  stops: rulesByName['historyStops'] as (() => DialStop[]) | undefined,
  order: rulesByName['orderHistory'] as ((g: Game, stop: number, atStart?: boolean) => boolean) | undefined,
  now: rulesByName['dialOf'] as ((g: Game) => DialNow | null) | undefined,
  whyNot: rulesByName['whyNotSetHistory'] as ((g: Game, stop: number) => string | null) | undefined,
  claims: rulesByName['claimsOf'] as ((g: Game) => ClaimRow[]) | undefined,
};
const dialStops = (): DialStop[] => { try { return dialCalls.stops ? dialCalls.stops() : []; } catch { return []; } };

// (Rules 84.2.5; ruling 211(a).) Guns and engineers never go forward alone: the rules' sentence where a force has no
// fighting corps in it, the order an army marches in (the fighting corps first), and the paper's line where his order
// leaves guns alone round a town. Asked for by name like the dial's, so that this bridge builds on the rules before
// that version too: there nothing is refused here and the corps march in the order given, as the rules then have it.
const leadCalls = {
  whyNot: rulesByName['whyNotLead'] as ((g: Game, corps: Corps[], to: number) => string | null) | undefined,
  order: rulesByName['marchOrder'] as ((g: Game, corps: Corps[]) => Corps[]) | undefined,
  leftAlone: rulesByName['gunsLeftAlone'] as ((g: Game, region: number) => string | null) | undefined,
};
const leadWhy = (g: Game, corps: Corps[], to: number): string => { try { return leadCalls.whyNot?.(g, corps, to) ?? ''; } catch { return ''; } };
const inMarchOrder = (g: Game, corps: Corps[]): Corps[] => { try { return leadCalls.order ? leadCalls.order(g, corps) : corps; } catch { return corps; } };

// Damaged cities (Brian, 20 November 2026: "when you click the cities there should be a repair button to press to
// repair the damaged buildings from artillery, bomb runs, and attacks"). Bombs, shells and the fighting leave a town
// partly in ruins, and it pays that much less until its holder pays to repair it: paid and done at once, and taken
// back with the money in the month it was ordered. Asked for by name, so that this bridge builds on rules without
// damaged cities too: there no town is ever damaged and nothing here shows.
interface RepairLike { region: number; town: string; province: string; damage: number; pct: number; holder: string; lost: number; cost: number; months: number; hit: boolean; why: string | null; poor: boolean; undo: boolean }
interface RepairAllLike { towns: RepairLike[]; pick: RepairLike[]; cost: number; gain: number; all: number; short: number }
const repairCalls = {
  on: rulesByName['setCityDamage'] as ((g: Game, on: boolean) => boolean) | undefined,
  view: rulesByName['cityDamage'] as ((g: Game, region: number) => RepairLike | null) | undefined,
  mine: rulesByName['damagedCities'] as ((g: Game) => RepairLike[]) | undefined,
  plan: rulesByName['repairAllOutlook'] as ((g: Game) => RepairAllLike | null) | undefined,
  marks: rulesByName['damageMarks'] as ((g: Game) => { region: number; town: string; damage: number; holder: string; hit: boolean }[]) | undefined,
  order: rulesByName['orderRepair'] as ((g: Game, region: number) => boolean) | undefined,
  all: rulesByName['orderRepairAll'] as ((g: Game) => string[]) | undefined,
  undo: rulesByName['orderUndoRepair'] as ((g: Game, region: number) => boolean) | undefined,
};

// Air campaigns (ruling 236.2): flown in a new grand campaign as shipped (aircampaigns.json `on`; the design director,
// 22 November 12:28Z, b623 and b636). Asked for by name, so that this bridge builds on rules without the switch too:
// there the war is played as before. A saved game keeps the switch it was started with.
const acCalls = {
  shipped: rulesByName['airCampaignsShipped'] as (() => boolean) | undefined,
  on: rulesByName['setAirCampaigns'] as ((g: Game, on: boolean) => void) | undefined,
};
function openAirCampaigns(g: Game): void {
  try { if (acCalls.shipped?.()) acCalls.on?.(g, true); } catch { /* rules without air campaigns */ }
}
/**
 * A grand campaign saved before air campaigns were flown (Brian, 23 November: "i want this in release 1 as its a major
 * part") flies them from the month it is loaded, as a new one does from its first, with one line in the news. Only a
 * grand campaign (startOpen's flag): a scenario or a history campaign is never open, so it is never touched.
 */
function airCampaignsOnLoad(g: Game): void {
  if (!isOpen(g) || g.flags.ac) return;
  openAirCampaigns(g);
  if (g.flags.ac) headline(g, 'info', 'Air campaigns are flown in this war from this month: the air forces fight standing campaigns for the sky and bomb to a plan.', g.human ?? undefined);
}
/**
 * A grand campaign saved before the start guns were on lays them as it is loaded (1.5.1; Brian, 24 November 22:31Z: "Yes,
 * when I load a saved war"): once, for every great power, with one line in the news. airversion.ts startGunsOnLoad.
 */
function startGunsLoaded(g: Game): void {
  if (startGunsOnLoad(g, majorsOf(g))) headline(g, 'info', "Anti-aircraft guns now stand round each great power's capital and its two chief factory towns.", g.human ?? undefined);
}
const repairView = (g: Game, region: number): RepairLike | null => { try { return repairCalls.view?.(g, region) ?? null; } catch { return null; } };
const damagedOfMine = (g: Game): RepairLike[] => { try { return repairCalls.mine?.(g) ?? []; } catch { return []; } };
const leftAloneAt = (g: Game, region: number): string => { try { return leadCalls.leftAlone?.(g, region) ?? ''; } catch { return ''; } };

/** (Rules 84.2.5.) Why the corps picked cannot go forward onto that ground by themselves, in the rules' words; '' where they can. */
function leadOf(ids: number[], to: number) {
  const s = game(), r = s.regions[to];
  if (!r) return { why: '' };
  const picked = corpsById(s, ids);
  const first = leadWhy(s, picked, to);
  // (Rules 84.2.6; the design director's log b357.) Where enemy troops stand next door, the right-click opens the attack
  // card on the staff's best force, which may bring infantry or tanks in beside the guns: the refusal stands only where
  // that force is guns alone too, as the browser's hover reads it. (Not next door, the order is the march up, and the
  // guns would still go forward alone.)
  if (!first || !s.human || !hostileUnits(s, r, s.human).length) return { why: first };
  const near = picked.filter((c) => s.regions[c.reg]?.nb.includes(to));
  if (!near.length) return { why: first };
  try { return { why: leadWhy(s, openingForce(s, near, r).corps, to) }; } catch { return { why: first }; }
}

// ---- The alarm that names the cure (Rules 84.2.6; ruling 211(c); the UI director's sheet 35) -------------------------
// One rules reading that both games draw: homeAlarms(g, nat) in danger.ts, nearest his capital first, each with the
// row's three sentences (the second left out where nothing of his is in reach), where the nearest help stands, what a
// press picks up (`help.corps`), the province to right-click (`help.to`), and the paper's line after it (`hand`, ''
// where no army can come).
function alarmsOf(s: Game) {
  if (!s.human || !humanUp(s)) return [];
  let all: HomeAlarm[] = [];
  try { all = homeAlarms(s, s.human) ?? []; } catch { return []; }
  return all.map((a) => ({
    case: a.case, region: a.region, by: a.by, lines: (a.lines ?? []).filter((t) => !!t),
    help: a.help ? a.help.region : -1, pick: a.help?.corps ?? [], to: a.help?.to ?? a.region, hand: a.hand ?? '',
  }));
}

// (Rules 84.2.6; ruling 211(a) as passed at 16:54 UTC.) His own guns left alone round an enemy town: his siege's own row
// in the tray, red while the cure is in his hands, plain where nothing of his can reach them. (Rules 84.3: a press on a
// red row picks up the infantry or tanks that can come, `help`, from where they stand, `from`; a plain one centres the town.)
function gunsAloneOf(s: Game) {
  if (!s.human || !humanUp(s)) return [];
  try { return (gunsAlone(s, s.human) ?? []).filter((a) => !!a.line).map((a) => ({ region: a.region, what: a.what ?? '', red: !!a.red, line: a.line, help: a.help ?? [], from: a.from ?? -1 })); } catch { return []; }
}

// ---- Crossing a defended sea (Version 72) ---------------------------------------------------------
// The danger of a landing's crossing can be read, prepared for and brought down: the reckoning line by line, what
// would make it safer with the danger as it would then be, the list of crossings open to the player, the one he
// names as his plan, and the shipping that carries his armies.

// (Version 73) Whether the sea behind a beach stays open to the supply ships once the force is ashore: a ticket shaped like the reckoning.
interface SeaHoldView { sea: number; open: boolean; port: boolean; lines: { label?: string; value?: string; effect?: string }[]; contested?: boolean; island?: boolean; months?: number }
interface CrossAccount { risk: number; loss: number; sky: string; worst: number; skyWon: number | null; lines: { label?: string; value?: string; effect?: string }[]; helps: { id: string; text: string; risk: number }[]; hold?: SeaHoldView }
interface CrossingRow { foe: string; owner: string; home: boolean; from: { id: number; name: string }; to: { id: number; name: string }; path: number[]; bar: string | null; barred: string | null; account: CrossAccount; planned: boolean; hope?: boolean }
const seaCalls = {
  crossings: rulesByName['crossings'] as ((g: Game) => CrossingRow[]) | undefined,
  plan: rulesByName['orderCrossingPlan'] as ((g: Game, from: number | null, to: number | null) => boolean) | undefined,
  lift: rulesByName['liftOf'] as ((g: Game) => { lift: number; left: number; building: number[]; room: number } | null) | undefined,
};

/** The player's shipping for armies, in a line: what it carries a month, what is left this month, and the troopships on the slips. */
function liftLine(s: Game): string {
  try {
    const l = seaCalls.lift ? seaCalls.lift(s) : null;
    if (!l) return '';
    const slips = (l.building ?? []).length;
    return `Sea lift: your shipping carries ${l.lift} corps a month, and ${l.left} ${l.left === 1 ? 'is' : 'are'} left this month.${slips ? ` ${slips} lot${slips === 1 ? '' : 's'} of troopships on the slips, the first ready in ${turnToDate(Math.min(...l.building)).label}.` : ''}`;
  } catch { return ''; }
}

/** (Version 73) The holding of the sea behind a beach as lines to read, under a heading of its own; nothing where the rules have none. */
function holdLines(h: SeaHoldView | undefined | null): string[] {
  if (!h || !(h.lines ?? []).length) return [];
  return ['ONCE ASHORE: HOLDING THE SEA BEHIND THE BEACH', ...h.lines.map((l) => `\u00B7 ${l.label ?? ''}${l.value ? `: ${l.value}` : ''}${l.effect ? `  (${l.effect})` : ''}`)];
}
/** (Version 73) The warning where the enemy could close that sea behind the force, in the browser's own words; nothing where he could not. */
function holdWarning(s: Game, h: SeaHoldView | undefined | null, landing: boolean): string {
  if (!h) return '';
  const sea = s.seas[h.sea]?.name ?? 'sea';
  // (Version 76: a whole island that can be cut off, where the force lives a while on what it carries, and a sea that is contested)
  if (!h.open && h.island) return `Once ashore the force is on its own: the enemy can close the ${sea} behind it. Cut off, it can hold out for about ${h.months ?? 0} months on what it carries.`;
  if (h.open) return h.contested ? `Once ashore, the ${sea} behind the force is contested: yours today, but the enemy can bring ships enough to fight you for it.` : '';
  return landing
    ? `As things stand the enemy can close the ${sea} behind the force, and the troops would be cut off from their supplies: beat his fleets, or bring more of your own ships and aircraft into that sea first.`
    : `Once ashore, the enemy can close the ${sea} behind the force: he can bring more than twice your strength into it.`;
}

/** The reckoning of a crossing as lines to read: each line of the account, then what would make it safer and the danger as it would then be. */
function accountLines(a: CrossAccount | undefined | null): string[] {
  if (!a) return [];
  const out: string[] = [];
  for (const l of a.lines ?? []) out.push(`${l.label ?? ''}${l.value ? `: ${l.value}` : ''}${l.effect ? `  (${l.effect})` : ''}`);
  if ((a.helps ?? []).length) {
    out.push('What would make it safer:');
    for (const h of a.helps) out.push(`${h.text.replace(/[.\s]+$/, '')}: the danger would then be about ${Math.round(h.risk * 100)} in 100.`);
  }
  return out.filter(Boolean);
}

/** The crossings open to the player: for each shore an enemy holds that his army cannot march to, the safest way over, what bars it, why, and what would help. */
function crossingsPage(s: Game): Page | null {
  if (!seaCalls.crossings || !s.human) return null;
  const up = humanUp(s) && !onEve(s);
  let list: CrossingRow[] = [];
  try { list = seaCalls.crossings(s); } catch { list = []; }
  const rows: PageRow[] = list.map((c) => {
    const risk = Math.round((c.account?.risk ?? 0) * 100);
    return {
      nat: c.owner, title: `${c.from.name} to ${c.to.name}${c.planned ? '  \u00B7  YOUR PLAN' : ''}`,
      lines: [c.bar ? `Barred today: ${c.bar}` : `Open today. About ${risk} in 100 of the force would be caught at sea.`,
        ...(c.hope ? ['It is listed because it would be the safest way to this shore once the sky over the beach was won.'] : []), ...accountLines(c.account), ...holdLines(c.account?.hold)],
      // (Version 73: set out in red where the enemy could close the sea behind the force)
      alarm: holdWarning(s, c.account?.hold, false),
      buttons: [c.planned
        ? { label: 'DROP THE PLAN', act: 'crossing:none', ok: up, tip: 'Your admirals and your air staff go back to their own judgement.' }
        : { label: 'NAME THIS THE PLAN', act: `crossing:${c.from.id}:${c.to.id}`, ok: up, tip: 'Where you have handed them the navy and the air war, your admirals stand a fleet off this beach and your air staff fights for the sky over it. The landing itself is always your own order.' }],
    };
  });
  return {
    id: 'crossings', title: 'SEA CROSSINGS',
    headline: rows.length ? `${rows.length} shore${rows.length === 1 ? '' : 's'} across the water` : 'No crossing to plan',
    note: liftLine(s) || 'You have no shipping for an army.', top: [],
    sections: [{
      heading: 'THE CROSSINGS',
      text: rows.length ? 'For each shore an enemy holds that your army cannot march to: the safest way over from one of your ports, what bars it today, why it is as dangerous as it is, and what would make it safer. Each figure is exact: do the thing and the danger is that number.'
        : 'Every enemy shore can be reached by land, or you are at war with nobody across the sea.',
      rows,
    }],
  };
}
const dialNow = (s: Game): DialNow | null => { try { return dialCalls.now ? dialCalls.now(s) : null; } catch { return null; } };

/**
 * Claims and ambitions for the Powers view, worded as the browser words them: who claims what ground from whom, and
 * how far it has gone. Claims on the player's ground come first. Null where there is nothing to say (no dial, or
 * a dial that has never stood loose enough for a claim).
 */
function claimsPane(s: Game): { lead: string; rows: { title: string; from: string; tag: string; state: number; onYou: boolean; region: number }[] } | null {
  try {
    const d = dialNow(s);
    if (!d) return null;
    const list = dialCalls.claims ? dialCalls.claims(s) : [];
    if (!list.length && d.stop <= 3) return null;
    const rows = list.map((c) => {
      const when = turnToDate(c.since).label, at = s.byName[c.ground[0]];
      return {
        title: `${upFirst(theName(s, c.nat))} claims ${c.name}`,
        from: `from ${theName(s, c.on)}${c.why ? `: ${c.why}` : ''}.`,
        tag: c.state === 'marched' ? `At war for it since ${when}` : c.state === 'pressing' ? `Pressing its claim since ${when}: it may march` : 'A claim it has not pressed',
        state: c.state === 'marched' ? 2 : c.state === 'pressing' ? 1 : 0,
        onYou: !!c.onYou,
        region: at ? at.id : -1,
      };
    });
    const lead = d.stop > 3
      ? `With the history dial at ${d.name}, a country outside the war may press a claim on ground a neighbour holds and march for it when its moment comes. It always gives warning first, and every claim that could be pressed is listed here.${rows.length ? '' : ' No country holds a claim it could press today.'}`
      : 'Raised while the history dial stood looser than it does now. A war begun for a claim is fought to its end.';
    return { lead, rows };
  } catch { return null; }
}

/** The dial's part of the staff's room: where it stands, the five stops with a button each, and where history's book is closed. */
function dialSection(s: Game): { heading: string; text: string; rows: PageRow[] } | null {
  const d = dialNow(s), stops = dialStops();
  if (!d || !stops.length) return null;
  const up = humanUp(s) && !onEve(s);
  const closed = d.closed.map((c) => `History's book is closed in ${c.name} since ${turnToDate(c.turn).label}.`);
  return {
    heading: 'THE HISTORY DIAL',
    text: [`How far the other countries may move away from what history did. It stands at ${d.name} (${d.under}). ${d.blurb}`,
      d.locked ? d.locked : 'The dial leashes the world the computer runs, never you: your own orders are free at every stop.',
      d.loosest !== d.stop ? `This campaign has been played as loose as ${d.loosestName}.` : '', ...closed,
      // (Version 72) What history's book still expects, soonest first.
      ...((d.expects ?? []).length ? ["History's book still expects:", ...(d.expects ?? []).slice(0, 4).map((e) => `${turnToDate(e.turn).label}, ${e.name}: ${e.text}`)] : [])].filter(Boolean).join('\n'),
    rows: stops.map((x) => {
      const on = x.stop === d.stop;
      let why: string | null = null;
      try { why = on ? null : dialCalls.whyNot ? dialCalls.whyNot(s, x.stop) : null; } catch { why = null; }
      return {
        nat: '', title: `${x.stop}  ·  ${x.name}  ·  ${x.under}${on ? '  ·  where it stands' : ''}`, lines: [x.blurb, ...x.lines],
        // (The UI director, 11:19 on build 1638, as for the depth played: the stop the dial stands at has no button.
        // Its title already says where it stands.)
        buttons: on ? [] : [{ label: `TURN THE DIAL TO ${x.name.toUpperCase()}`, act: `history:${x.stop}`, ok: up && !why,
          tip: why ?? "The change reaches the other countries' decisions from next month. It can be turned again in any month." }],
      };
    }),
  };
}

// ---- Marches that carry on ------------------------------------------------------------------------
// (Rules Version 82; the design director's rulings 87, 93 and 94.) A march to friendly ground is a standing order:
// the corps goes as far as it can at once, and on again as each of the player's months opens, until it arrives. A
// corps with no moves left sets off as the next month opens, to the province next door as to any other. Its first
// leg always marches; after that it halts only for news (the enemy can now reach its next halt, it was attacked, or
// the way is closed), and says which. Any other order ends it. All of it is the rules' own (src/campaign/marches.ts),
// kept with the game as flags, so that a saved game reads the same in the browser and here.
const monthsOf = marchMonths;
const inMonths = aboutMonths;

/** Where a picked-up army could go: anywhere any one of its corps could, and what it could attack. */
function reachOf(ids: number[]) {
  const s = game();
  const merged = new Map<number, { id: number; cost: number; attack: boolean; prev: number; war?: string; take?: boolean }>();
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
    // (Rules 84.1, ruling 175(c).) Enemy ground with no army in its field: marching there is the order to take the
    // place. The game asks takeOutlook what the march will meet before it sends anybody.
    try {
      const me = s.human;
      if (me) for (const e of merged.values()) if (!e.attack && !e.war && atWar(s, me, s.regions[e.id].ctrl)) e.take = true;
    } catch { /* (a reach without the marks is still a reach) */ }
  }
  // Enemy reach (release 1): how far the enemy corps the player can see could strike next month. Kept to the ground
  // that matters to the army in hand: his side's own, and wherever this army could go.
  let danger = new Set<number>();
  try {
    const me = s.human;
    if (me && humanUp(s)) danger = enemyReach(s, (id) => { const r = s.regions[id]; return r.ctrl === me || allied(s, r.ctrl, me) || merged.has(id); });
  } catch { danger = new Set<number>(); }
  // Friendly ground beyond this month's reach (release 1, marches that carry on): each such province with about how
  // many months the march would take, two by two; and the road still ahead of a corps already on a standing march.
  const far: number[] = [];
  let standing: number[] = [];
  let standingTo = -1;
  // (Ruling 87.) True when every corps in hand that takes orders has used its moves: an order is kept for next month.
  let spent = false;
  try {
    if (humanUp(s) && !onEve(s)) {
      const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
      const lead = cs[0];
      spent = cs.length > 0 && cs.every((c) => c.ap <= 0);
      if (lead) {
        for (const [id, g] of farGround(s, lead)) {
          // (a province next door that cannot be entered this month is full: that is a matter of changing places, not of a long march)
          // (Ruling 87: but a corps with no moves left is another matter. Its march next door is kept like any other, and sets off as the next month opens.)
          if (id === lead.reg || merged.has(id) || (lead.ap > 0 && s.regions[lead.reg].nb.includes(id)) || far.length >= 1200) continue;
          far.push(id, monthsOf(lead, g.cost));
        }
        const on = cs.find((c) => marchOf(s, c) >= 0);
        if (on) { standingTo = marchOf(s, on); standing = farRoad(s, on, standingTo)?.road ?? []; }
      }
    }
  } catch { far.length = 0; standing = []; standingTo = -1; spent = false; }
  return { regions: [...merged.values()], danger: [...danger], far, standing, standingTo, spent };
}

/**
 * (Rulings 88, 101 and 102(b).) The round of armies as the rules count it (round.ts), so that both builds show the
 * same numbers: `bright` on NEXT ARMY's disc (while it is above nothing END THE MONTH stays dark), `quiet` for the
 * rear, `front` for how many of the bright stand at the front, `all` for every army Tab still goes to, and `idle`
 * for the question at the month's end.
 */
function roundOf(s: Game) {
  try {
    const c = roundCount(s);
    // (rulings 111(f) and 116: the bright count in the rules' own words, the same beside NEXT ARMY and in the question)
    return { bright: c.bright, quiet: c.quiet, front: c.front, all: armyRound(s).length, idle: frontIdle(s).length, words: roundWords(c), question: c.bright > 0 ? roundQuestion(c) : '' };
  } catch { return { bright: 0, quiet: 0, front: 0, all: 0, idle: 0, words: '', question: '' }; }
}

/** The army Tab goes to next, given those already visited this round and where the eye is (the rules' nextArmy). */
function nextArmyOf(seen: number[], x: number, y: number) {
  try {
    const n = nextArmy(game(), seen, Number.isFinite(x) && Number.isFinite(y) ? { x, y } : null);
    return { reg: n.army ? n.army.reg : -1, seen: n.seen, again: n.again };
  } catch { return { reg: -1, seen: [] as number[], again: false }; }
}

/** STAND THIS MONTH for the army in a province, or taking it back: kept in the game itself, so in its saves. */
function standOf(reg: number, on: boolean) {
  const s = game();
  let changed = 0;
  try { changed = standArmy(s, reg, on); } catch { changed = 0; }
  let stands = '';
  try { stands = armyStands(s, reg) ?? ''; } catch { stands = ''; }
  return { changed, stands, view: view() };
}

/**
 * (For the photographer only.) Cuts a province's railway by the rules' own cutRail, with a cause; or, with `how`
 * "report" or "seen", writes down only that the player knows of a cut there (for the hollow sign of a cut he cannot see).
 */
function stageCut(region: number, cause: string, how: string) {
  const s = game();
  if (!s.regions[region]) return { ok: false, view: view() };
  const why = (['bombed', 'agent', 'saboteurs', 'retreat', 'hasty', 'order'].includes(cause) ? cause : 'bombed') as RailCause;
  if (how === 'report' || how === 'seen') {
    (s.intel.cuts ??= {})[region] = { t: Math.max(1, s.turn - 1), how, why };
    return { ok: true, view: view() };
  }
  const done = cutRail(s, region, why);
  return { ok: done, view: view() };
}

/**
 * (For the photographer only: no start has a place with anti-aircraft guns, and the raid card's defences need a
 * picture.) Gives a province guns of a level, as if they had been built there.
 */
function stageGuns(region: number, level: number) {
  const s = game();
  if (s.regions[region]) s.flags[`wk:aa:${region}`] = Math.max(0, Math.min(3, Math.round(level)));
  return { ok: !!s.regions[region], view: view() };
}

/** (For the photographer only.) Flies a reconnaissance flight over a province by the rules' own sendScout, with the first of the player's air groups that can go. */
function stageRecon(region: number) {
  const s = game();
  let said = '';
  try {
    for (const f of s.airFleets) {
      if (f.nat !== s.human) continue;
      const flown = sendScout(s, f.id, region);
      if (flown.ok) { said = flown.text || 'flown'; break; }
    }
  } catch { /* none could go */ }
  return { ok: !!said, said, view: view() };
}

/** How much of the army in a province stands this month: 'all', 'some', 'none', or '' where it has nothing to stand. */
function standsOf(reg: number) {
  try { return { stands: armyStands(game(), reg) ?? '' }; } catch { return { stands: '' }; }
}

/**
 * (Ruling 86.) Where each gate's mark stands: on the water at its narrowest, by latitude and longitude. A gate the
 * rules add later, and not named here, stands on the province that holds it until it is given a place.
 */
const GATE_AT: Record<string, [number, number]> = {
  'the Panama Canal': [9.62, -79.92],
  'the Danish straits': [55.35, 10.95],
  'the Strait of Gibraltar': [35.95, -5.6],
  'the Turkish straits': [40.2, 26.4],
};

/**
 * (Rulings 86 and 90b.) The sea gates as the player is shown them. Who holds each, whether his ships may pass and
 * the words for it are the rules' own (gates.ts, which reads canPass and each gate's own rule), so that both builds
 * say the same. The box marks the words that answer the question in a colour of their own, so the line is handed
 * over in three parts: what comes before "Open to your ships." or "Shut to your ships:", those words, and the rest.
 * A gate with no place given above stands where the rules put it (gateSpot).
 */
function gatesOf(s: Game) {
  try {
    return gateViews(s, s.human ?? null).map((v) => {
      const gt = s.gates[v.id];
      const at = GATE_AT[gt.name];
      const spot = at ? { x: (at[1] + 11) * 25.7115, y: (71.4 - at[0]) * 40 } : gateSpot(s, gt);
      const open = v.open !== false;
      const key = open ? 'Open to your ships' : 'Shut to your ships:';
      const i = v.line.indexOf(key);
      const mark = i < 0 ? '' : open && v.line[i + key.length] === '.' ? `${key}.` : key;
      const lead = i < 0 ? v.line : v.line.slice(0, i), rest = i < 0 ? '' : v.line.slice(i + mark.length);
      // (for a fleet's route that is refused: the gate named, with the same reason, as gateBar words it)
      const shut = open ? '' : `${v.theName} ${/s$/.test(v.name) ? 'are' : 'is'} closed to your ships: ${v.why}.`;
      return { id: v.id, name: v.name, x: spot.x, y: spot.y, holder: v.holder, open, lead, mark, rest, shut, order: v.order ?? '', orderLine: v.orderLine, shutLine: v.shutLine };
    });
  } catch { return []; }
}

/** (Ruling 86.) The seas a fleet would reach this month but for a gate shut to it, each with that gate: sea, gate, sea, gate. */
function barredSeas(s: Game, f: Game['fleets'][number]): number[] {
  const out: number[] = [];
  try {
    const now = new Set(fleetReach(s, f));
    for (let z = 0; z < s.seas.length; z++) {
      if (now.has(z)) continue;
      const bar = gateBar(s, f, z);
      if (bar) out.push(z, bar.gate.id);
    }
  } catch { out.length = 0; }
  return out;
}

/**
 * (Ruling 87.) Before the order is given: could the corps in hand be marched to that place on a standing order, about
 * how long it would take, the road, and the line for the pointer. All of it is the rules' own (marches.ts); for more
 * than one corps the line speaks of them together. Nothing in the game is changed by asking.
 */
function marchPlanOf(ids: number[], to: number, army = '', armyIds: number[] = []) {
  const none = { ok: false, spent: false, now: false, months: 0, road: [] as number[], line: '' };
  try {
    const s = game();
    const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
    const lead = cs[0];
    if (!lead || !s.regions[to]) return none;
    const p = planMarch(s, lead, to);
    let line = p.line.split(lead.name).join(shortName(lead.name)), months = p.months;
    if (p.ok && !p.now && cs.length > 1) {
      for (const c of cs.slice(1)) { const q = planMarch(s, c, to); if (q.ok && !q.now) months = Math.max(months, q.months); }
      const all = cs.every((c) => c.ap <= 0);
      // (One voice after a press) named as the order's own sentence will name them: the army, or so many corps of it
      const { who, many } = whoOf(cs, army, armyIds);
      line = all ? `${leading(who)} ${many ? 'march' : 'marches'} here from next month, ${inMonths(months)}.`
        : `${leading(who)} ${many ? 'march' : 'marches'} here over ${inMonths(months)}, going on by ${many ? 'themselves' : 'itself'} as each month opens.`;
    }
    return { ok: p.ok, spent: p.spent, now: p.now, months, road: p.road, line };
  } catch { return none; }
}

/** (Version 65) The swap last offered: asked once, done when the same march is ordered again. */
let swapAsked = '';

/** A corps' own name, without the divisions in brackets that some names in the data still carry ("First Army (1st Division)" is First Army). */
const shortName = (name: string): string => name.replace(/\s*\([^()]*Division[^()]*\)\s*$/, '');
/** A sentence's first letter made a capital ("the American army" leading a sentence). */
const leading = (text: string): string => text.charAt(0).toUpperCase() + text.slice(1);

/**
 * (One voice after a press; the UI director's brief, ruled by the design director at 17:16 UTC on 17 November.) Who
 * an order speaks of: the whole army in hand by the army's own name, as the tray's head names it ("the American
 * army", "Heinz Guderian's army"); part of it as "2 corps of the American army"; one corps alone by its own name.
 * `army` and `armyIds` are the table's: the army's name and the corps it has in the field there.
 */
function whoOf(cs: Corps[], army: string, armyIds: number[]): { who: string; many: boolean } {
  if (cs.length === 1) return { who: shortName(cs[0].name), many: false };
  const has = new Set(cs.map((c) => c.id));
  if (army && armyIds.length > 0 && armyIds.every((id) => has.has(id))) return { who: army, many: false };
  return { who: army ? `${cs.length} corps of ${army}` : `${cs.length} corps`, many: true };
}

function move(ids: number[], to: number, army = '', armyIds: number[] = []) {
  const s = game();
  const moved: { id: number; path: number[] }[] = [];
  let note = '';
  // (One voice after a press) the corps whose march is kept, said of together once they are all ordered
  const kept: Corps[] = [];
  let keptMonths = 0, setOff = false;
  // (Version 65) A friendly province next door that is full: the corps can change places with one that stands
  // there. It is asked once, and done when the same march is ordered again.
  const trySwap = (c: Corps): void => {
    let others: Corps[] = [];
    try { others = s.regions[c.reg]?.nb.includes(to) ? swapChoices(s, c, to) : []; } catch { others = []; }
    if (!others.length || moved.length) return;
    const other = others[0], from = c.reg, key = `${c.id}:${to}:${s.turn}`;
    if (swapAsked === key) {
      swapAsked = '';
      if (orderSwap(s, c, other)) {
        moved.push({ id: c.id, path: [to] }, { id: other.id, path: [from] });
        note = `${c.name} and ${other.name} have changed places.`;
      }
    } else {
      swapAsked = key;
      note = `${s.regions[to].name} is full. Order the march again and ${c.name} changes places with ${other.name}, which comes back to ${s.regions[from].name}.`;
    }
  };
  // (Rules 84.2.5) the fighting corps first: guns and engineers follow them onto enemy-held ground. (The code checker, 17
  // November) and those that can get there this month before those that cannot, so that a corps kept for a later
  // month never offers a swap, or takes a place, ahead of one that arrives.
  const order = inMarchOrder(s, corpsById(s, ids));
  const gets = (c: Corps): boolean => { const e = reach(s, c).get(to); return !!e && !e.attack && to !== c.reg; };
  const sent = [...order.filter(gets), ...order.filter((c) => !gets(c))];
  const from = new Set(sent.map((c) => c.reg));
  for (const c of sent) {
    const m = reach(s, c);
    const end = m.get(to);
    if (to === c.reg || end?.attack) continue;
    if (!end) {
      // (Rules Version 82.) Friendly ground it cannot reach this month: a march that carries on. The corps sets off at
      // once with the moves it has; with none left it sets off as the next month opens, to the province next door
      // as to any other. (A province next door that is only full is another matter: there the two change places.)
      const full = c.ap > 0 && s.regions[c.reg].nb.includes(to);
      const plan = full ? null : planMarch(s, c, to);
      if (plan && plan.ok && !plan.now) {
        const done = orderMarch(s, c, to);
        if (done.moved > 0) moved.push({ id: c.id, path: plan.road.slice(0, done.moved) });
        if (done.kept) {
          kept.push(c);
          keptMonths = Math.max(keptMonths, done.months);
          if (done.moved > 0) setOff = true;
        }
        continue;
      }
      trySwap(c);
      continue;
    }
    forgetMarch(s, [c.id]); // (a new order ends a standing march)
    const path: number[] = [];
    for (let at = to; at !== c.reg && at >= 0; at = m.get(at)!.prev) path.unshift(at);
    const n = orderMove(s, c, to);
    if (n > 0) moved.push({ id: c.id, path: path.slice(0, n) });
    else trySwap(c);
  }
  if (kept.length) {
    // One sentence answers the press, of everyone whose march is kept. (a) It set off this month and goes on; (b) it
    // was laid for next month, nobody having stirred yet. A march that arrives says nothing: the piece moving is the answer.
    const { who, many } = whoOf(kept, army, armyIds), place = s.regions[to].name;
    const said = setOff
      ? `${leading(who)} ${many ? 'are' : 'is'} on the march to ${place}: ${inMonths(keptMonths)} more, going on as each month opens. Any other order ends the march.`
      : `${leading(who)} will march to ${place} from next month, ${inMonths(keptMonths)}. Any other order ends the march.`;
    note = note ? `${said} ${note}` : said;
  }
  // (Rules 84.2.5; ruling 211(a).) His order has left only guns round a town he was besieging: the rules' line, as the infantry leave.
  for (const r of from) {
    const alone = r !== to && moved.length ? leftAloneAt(s, r) : '';
    if (alone) note = note ? `${note} ${alone}` : alone;
  }
  return { moved, note, view: view() };
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
  // (Version 61) Attacked from three sides or more, each side's roads carry one corps more; and against works a
  // few artillery corps go in over and above the roads. The line says so where it happened.
  const sent = attackers(s, ids, to);
  const sides = new Set(sent.map((c) => c.reg)).size;
  const wider = s.flags.open === 1 && sides >= DEFENCE.frontage.sides;
  const cap = frontage(s.regions[to]) + (wider ? DEFENCE.frontage.extra : 0);
  const gunsOver = s.flags.open === 1 && sent.some((c) => c.kind === 'art') && (!!s.regions[to].fort || s.regions[to].units.some((u) => !u.dead && u.dug && !!s.human && atWar(s, s.human, u.nat)));
  return {
    label: 'The front', value: `${going.size} of ${all.length} corps go in`,
    effect: `Only ${cap} corps can attack ${s.regions[to].terrain === 'plains' ? 'open country' : s.regions[to].terrain === 'mountain' ? 'mountains' : 'this ground'} from one side${wider ? ` (one more than usual, as the attack comes in from ${sides} sides)` : ''}, the strongest first.${gunsOver ? ` Against works up to ${DEFENCE.frontage.guns} artillery corps fire over their heads and take no room on the roads.` : ''} ${listOf(left.slice(0, 3).map((c) => c.name))}${left.length > 3 ? ` and ${left.length - 3} more` : ''} ${left.length === 1 ? 'waits' : 'wait'} behind.`,
  };
}

/** A chance as a whole percentage that never rounds a near thing up to a sure one, or a faint hope down to none. */
const oddsPct = (p: number): number => (p >= 0.9995 ? 100 : p <= 0.0005 ? 0 : Math.min(99, Math.max(1, Math.round(p * 100))));

type Look = NonNullable<ReturnType<typeof attackOutlook>>;
/**
 * (Version 63) A chance or a band of chances in words: one figure where the staff can give one, its best guess with
 * the limits where the band is too wide to plan on.
 */
const chanceWords = (look: Look): string => (look.exact || Math.round(look.chanceLo * 100) === Math.round(look.chanceHi * 100) ? `about ${oddsPct(look.chanceLo)}%`
  : look.wide ? `a best guess of ${oddsPct(look.guess)}%, anywhere from ${oddsPct(look.chanceLo)} to ${oddsPct(look.chanceHi)}%` : `${oddsPct(look.chanceLo)} to ${oddsPct(look.chanceHi)}%`);
/**
 * (The UI director, 17 November, on Version 84.3.) The chance of taking a place, said: at the ends of the scale in
 * words, never "about 100%" or "about 0%". The card's own odds line says it the same way (ChanceTo in the controller).
 */
// (Rules 84.3.1: the browser's endSaid, word for word.) `named` where the line has already named the staff.
const endWords = (words: string, named = false): string => (words === 'about 100%' ? (named ? 'they are sure of it' : 'your staff is sure of it')
  : words === 'about 0%' ? (named ? 'they give it no chance' : 'your staff gives it no chance') : '');
const chanceTo = (look: Look, place: string): string => {
  const words = chanceWords(look);
  return endWords(words) || `${words} to take ${place}`;
};
/** Men to the nearest hundred, for a forecast that is not exact. */
const roughMen = (n: number): string => (n < 100 ? 'under 100' : (Math.round(n / 100) * 100).toLocaleString('en'));
/**
 * (Version 63) The bill before the order: what the fight is likely to cost each side in men, if it carries the
 * ground and if it is thrown back. The enemy's loss is a count where every defender is identified, and a share of
 * his men where the defence is partly a guess.
 */
function costLines(look: Look | null): string[] {
  if (!look?.cost) return [];
  const row = (c: Look['cost']['won'], head: string): string => {
    if (!c) return '';
    const theirs = c.theirs !== null ? `${roughMen(c.theirs)} men` : `${Math.max(1, Math.round(c.share * 100))}% of their men`;
    return `${head}: you lose about ${roughMen(c.ours)} men, they lose about ${theirs}.`;
  };
  return [row(look.cost.won, look.cost.lost ? 'If it carries' : 'It should carry'), row(look.cost.lost, look.cost.won ? 'If it is thrown back' : 'Thrown back, as it will be')].filter(Boolean);
}

/**
 * (Version 61) Everyone the roads will carry, where more stand in reach than were picked: what the attack could
 * be, said in a sentence, with the corps to pick to make it so.
 */
function massNote(s: Game, corps: Corps[], to: number, odds: number | null): { note: string; ids: number[] } | null {
  const target = s.regions[to];
  const cands = candidates(s, target);
  const everyone = jointForce(s, limitAttackers(s, jointForce(s, cands, target), target), target);
  const inSet = new Set(corps.map((c) => c.id));
  const extra = everyone.filter((c) => !inSet.has(c.id)).length;
  const massed = extra ? attackOutlook(s, everyone, target) : null;
  if (!massed || (odds !== null && massed.odds <= odds * 1.02)) return null;
  return {
    note: `${extra === 1 ? 'One more corps stands' : `${extra} more corps stand`} within reach. Send everyone the roads will carry, ${everyone.length} corps in all, and the odds ${odds !== null ? `go from ${odds.toFixed(2)} to ${massed.odds.toFixed(2)} to 1` : `are ${massed.odds.toFixed(2)} to 1`}: ${chanceTo(massed, target.name)}.`,
    ids: cands.map((c) => c.id),
  };
}

/** (Version 64) The last probe of a line this month: who went, what was found, what it cost. Said on the attack card afterwards. */
let lastProbe: { turn: number; region: number; name: string; found: string[]; wear: number; lost: number; was: string; now: string } | null = null;

/**
 * (Version 64) The row on the attack card that offers to soften the line first: a probe by one corps, or the
 * bombers on its fieldworks. Null where there is nothing to soften and nothing more to find.
 */
function softenRow(s: Game, corps: Corps[], to: number) {
  let v: ReturnType<typeof softenOptions> = null;
  try { v = softenOptions(s, corps, s.regions[to]); } catch { v = null; }
  const done = lastProbe && lastProbe.turn === s.turn && lastProbe.region === to ? lastProbe : null;
  if (!v || (v.works <= 0 && v.probe.fresh <= 0 && !done)) return null;
  const mo = (m: number): string => { const x = Math.round(m * 10) / 10; return x === 1 ? 'a month' : `${x} months`; };
  const num = (n: number): string => Math.round(n).toLocaleString('en');
  const gains = [v.probe.fresh ? `name the ${v.probe.fresh === 1 ? 'corps' : `${v.probe.fresh} corps`} the staff cannot see` : '', v.probe.wear > 0 ? `take ${mo(v.probe.wear)} off their fieldworks` : ''].filter(Boolean).join(' and ');
  // (Version 65) A corps too thin for a full probe does less: said, with what a full one takes.
  const pw = v.probe as typeof v.probe & { weight?: number; full?: number };
  const thin = pw.weight !== undefined && pw.weight < 0.995 ? ` It has the men for ${Math.max(1, Math.round(pw.weight * 100))}% of a full probe${pw.full ? `, which takes ${num(pw.full)} men` : ''}.` : '';
  const probeText = v.probe.why ?? (gains ? `It would ${gains}, for about ${num(v.probe.men)} men and ${v.probe.ap} of its moves. It is not a battle: nothing changes hands.${thin}` : 'There is nothing more a probe could find here.');
  return {
    head: v.works > 0 ? `The defenders stand in fieldworks of ${mo(v.works)}${v.works >= DEFENCE.entrench.months ? ', finished' : ''}.` : 'The defenders are not dug in.',
    done: done ? `${done.name} has probed this line: it found ${listOf(done.found)}${done.wear > 0 ? `, and their fieldworks are ${mo(done.wear)} the worse` : ''}. It cost ${num(done.lost)} men.${done.was && done.now && done.was !== done.now ? ` Before it the staff gave ${done.was} to take the place; now ${endWords(done.now, true) || `it reads ${done.now}`}.` : ''}` : '',
    probe: { label: `PROBE${v.probe.name ? ` WITH ${v.probe.name.toUpperCase()}` : ' THE LINE'}`, text: probeText, ok: !v.probe.why && !!gains, corps: v.probe.corps },
    bomb: v.bomb ? {
      label: 'BOMB THE LINE', ok: !v.bomb.why,
      // (the raid planner, on this line, with the groups the staff would send)
      open: v.bomb.why ? '' : `plan:line:${to}:${(() => { try { return weighRaid(s, 'line', to).picked.join('.'); } catch { return ''; } })()}`,
      text: v.bomb.why ?? `${v.bomb.groups === 1 ? 'One air group' : `${v.bomb.groups} air groups`} with ${num(v.bomb.bombers)} bombers can reach it, and should take about ${mo(v.bomb.wear)} off the fieldworks. ${v.bomb.verdict}. The planner shows what will meet them.`,
    } : null,
  };
}

/**
 * (The UI director, 17 November, on the 3D replay of v8425 C.) The card always shows exactly what the player picked,
 * never the whole army. Where that pick cannot go (guns or engineers alone, ruling 211(a)), the attacking side shows
 * the corps picked and no men to take the ground, there is no forecast, `alone` is the rules' sentence for the foot,
 * and the row of everyone in reach stays as the way to mend the pick.
 */
function barredCard(s: Game, ids: number[], to: number, alone: string) {
  const target = s.regions[to];
  const picked = wouldAttack(s, ids, to);
  const mass = massNote(s, picked, to, null);
  const foes = s.human ? hostileUnits(s, target, s.human).filter((u) => !u.dead) : [];
  const named = (c: Corps) => {
    const kn = c.nat !== s.human && s.human && !allied(s, c.nat, s.human) ? knowledgeOf(s, c) : { level: 'exact' as const, age: 0 };
    return kn.level === 'none' ? { id: c.id, nat: c.nat, name: 'A corps not identified', kind: '', divs: 0, men: 0, gen: null }
      : kn.level === 'report' ? { id: c.id, nat: c.nat, name: `${corpsNamed(s, c.nat, c.name)} (a report ${kn.age} month${kn.age === 1 ? '' : 's'} old)`, kind: c.kind, divs: c.divs, men: 0, gen: null }
        : { id: c.id, nat: c.nat, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null };
  };
  return {
    region: to, alone, exact: true, attack: 0, defence: 0, odds: 0, lo: 0, hi: 0, even: 1.2, note: '', lines: [], face: [], cost: [], opened: '',
    hasChance: false, chanceWords: '', tone: '', verdict: '', soften: null, prize: prizeLine(s, to),
    allies: [...new Set(picked.filter((c) => c.nat !== s.human).map((c) => s.nations[c.nat]?.name ?? c.nat))],
    mass: mass?.note ?? '', massIds: mass?.ids ?? [], ids: picked.map((c) => c.id), pickedIds: [] as number[],
    attackers: picked.map((c) => ({ id: c.id, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: foes.map(named),
  };
}

// (Speed, second round.) `glance` is the pointer's tip, which reads only the odds: it leaves out the row that offers to
// soften the line first, which asks after every air group that could fly at it and was nineteen parts in twenty of the
// time this took. The attack card never asks with `glance`, and gets the whole answer as before.
function preview(ids: number[], to: number, open = false, glance = false) {
  const s = game();
  // (Version 63) The card opens on the best attack to be had: everyone the roads will carry, where that beats the
  // corps picked. It says so, and gives back the corps that were picked so one button can return to them.
  let pickedIds: number[] = [];
  if (open) {
    const mine = wouldAttack(s, ids, to);
    const best = mine.length ? openingForce(s, mine, s.regions[to]) : null;
    if (best?.widened) { pickedIds = mine.map((c) => c.id); ids = best.corps.map((c) => c.id); }
  }
  // (Rules 84.2.5; ruling 211(a).) Guns open no attack alone: the rules' sentence, where no fighting corps goes in with
  // them. Where the order opens the card (or the pointer asks for the odds) that is all there is to say.
  const alone = leadWhy(s, wouldAttack(s, ids, to), to);
  if (alone && (open || glance)) return { region: to, refused: alone };
  // (Version 55) Enemy troops standing on the ground of a country the player is not at war with cannot be attacked
  // until war is declared on that country: the rules say why, and the game tells the player instead of the odds.
  const foeThere = !!s.human && s.regions[to].units.some((u) => !u.dead && atWar(s, s.human!, u.nat));
  const refused = foeThere ? whyNotAttack(s, s.regions[to]) : '';
  if (refused) return { region: to, refused };
  if (alone) return barredCard(s, ids, to, alone);
  const corps = attackers(s, ids, to);
  const a = previewAttack(s, corps, s.regions[to]);
  if (!a) return null;
  // Where the defenders are not all identified, the odds are a band and the ticket leaves out what is not known.
  const look = attackOutlook(s, corps, s.regions[to]);
  // (Version 55) The odds at which it is an even chance: 1.2 to 1 in the open, more for an assault on a town.
  const even = look?.even ?? 1.2;
  // (Version 58: an attack on a line that is dug in and fed needs better odds too, and the rules' own line says so:
  // it is not said twice. Since Version 61 that is the one "Dug in" line, which carries the works, the day's luck
  // and what the attack takes off them.)
  const saidAlready = [...(look?.lines ?? []), ...a.lines].some((l) => l.label === 'Prepared defence' || (l.label === 'Dug in' && /even chance/.test(l.effect ?? '')));
  const mass = massNote(s, corps, to, look?.odds ?? a.odds);
  // (Version 63) What was brought in over the pick, in a sentence.
  let opened = '';
  if (pickedIds.length && look) {
    const mine = corpsById(s, pickedIds), mineLook = mine.length ? attackOutlook(s, mine, s.regions[to]) : null;
    opened = `The staff has brought in every corps the roads will carry. ${corps.length === 1 ? 'One corps goes' : `${corps.length} corps go`} in at ${look.odds.toFixed(2)} to 1${mineLook ? `, where the ${mine.length === 1 ? 'corps' : `${mine.length} corps`} you picked ${mine.length === 1 ? 'reads' : 'read'} ${mineLook.odds.toFixed(2)} to 1` : ''}.`;
  }
  const pct = {
    chancePct: oddsPct(look?.chance ?? 0), chancePctLo: oddsPct(look?.chanceLo ?? look?.chance ?? 0), chancePctHi: oddsPct(look?.chanceHi ?? look?.chance ?? 0), hasChance: !!look, mass: mass?.note ?? '', massIds: mass?.ids ?? [],
    // (Version 63) the chance in words, the likely cost in men either way, and the corps the card opened with
    chanceWords: look ? chanceWords(look) : '', cost: costLines(look), ids, opened, pickedIds,
    tone: look ? verdictOf(look).tone : '', verdict: look ? verdictOf(look).word : '',
    // (Version 64) soften the line first: a probe by one corps, or the bombers on its fieldworks
    soften: glance ? null : softenRow(s, corps, to),
    // (Version 84; rulings 149(d), 164(f) and 174) WHAT YOU FACE: a sign and a sentence for each thing the defence will
    // do to what he brings, four at most, and one more for what is not scouted. Empty in a history campaign.
    face: (look?.face ?? []).map((l) => ({ sign: l.sign, text: l.text, unknown: !!l.unknown })),
  };
  const evenLine = even > 1.21 && !saidAlready ? [{ label: 'An even chance', value: `${even.toFixed(1)} to 1`, effect: 'Luck runs wider in the streets of a town than in the open, and mostly against the attacker: it takes better odds here than in the field to make it an even chance.' }] : [];
  const allies = [...new Set(corps.filter((c) => c.nat !== s.human).map((c) => s.nations[c.nat]?.name ?? c.nat))];
  const named = (c: Corps) => {
    const kn = c.nat !== s.human && s.human && !allied(s, c.nat, s.human) ? knowledgeOf(s, c) : { level: 'exact' as const, age: 0 };
    return kn.level === 'none' ? { id: c.id, nat: c.nat, name: 'A corps not identified', kind: '', divs: 0, men: 0, gen: null }
      : kn.level === 'report' ? { id: c.id, nat: c.nat, name: `${corpsNamed(s, c.nat, c.name)} (a report ${kn.age} month${kn.age === 1 ? '' : 's'} old)`, kind: c.kind, divs: c.divs, men: 0, gen: null }
        : { id: c.id, nat: c.nat, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null };
  };
  if (look && !look.exact) {
    return {
      region: to, allies, prize: prizeLine(s, to), attack: look.odds, defence: 1, odds: look.odds, ...pct,
      exact: false, lo: look.lo, hi: look.hi, chance: look.chance, chanceLo: look.chanceLo, chanceHi: look.chanceHi, note: `${look.note}${look.wide && look.narrow ? ` ${look.narrow}` : ''}`, even,
      lines: briefTicket([...look.lines, ...(frontLine(s, ids, to) ? [frontLine(s, ids, to)!] : []), ...evenLine], look.face),
      attackers: corps.map((c) => ({ id: c.id, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
      defenders: a.defenders.map(named),
    };
  }
  return {
    ...pct,
    allies, exact: true, lo: a.odds, hi: a.odds, chance: look?.chance ?? 0, chanceLo: look?.chance ?? 0, chanceHi: look?.chance ?? 0, note: '', even,
    region: to,
    // What taking the place is worth, in a sentence, for the screen that asks whether to attack.
    prize: prizeLine(s, to),
    attack: a.A,
    defence: a.D,
    odds: a.odds,
    lines: briefTicket([...a.lines, ...(frontLine(s, ids, to) ? [frontLine(s, ids, to)!] : []), ...evenLine], look?.face),
    attackers: corps.map((c) => ({ id: c.id, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: a.defenders.map((c) => ({ id: c.id, nat: c.nat, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
  };
}

/**
 * (Rules 84.3; the UI director's words for b357, item 8.) The battle result as the card shows it: every corps on it
 * carries its country's word ("German XIX Corps" against "Polish Pomorze Corps"), as the rules' corpsNamed gives it.
 * A copy: the rules' own report is left as it is.
 */
function namedReport<R extends { attackers: { nat: string; name: string }[]; defenders: { nat: string; name: string }[] }>(s: Game, r: R | null | undefined): R | null {
  if (!r) return null;
  const named = <U extends { nat: string; name: string }>(u: U): U => ({ ...u, name: corpsNamed(s, u.nat, u.name) });
  return { ...r, attackers: r.attackers.map(named), defenders: r.defenders.map(named) };
}

/** What a battle came to, in a sentence. */
function battleNote(s: Game, r: BattleReport): string {
  if (r.yielded) return `The ${s.nations[r.defNat].adj} troops fell back from ${r.regionName} without a fight.`;
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
  // (Version 63) The price: men lost on each side, and the share of those sent in.
  return `${r.level} at ${r.regionName}: ${came}${prisoners}. ${priceLine(s, r)}${wornNote(s, r)}`;
}

/** (Version 63) What one side lost in a battle, in men and as a share of those engaged: the fight itself and whoever laid down his arms after it. */
function lostIn(us: BattleReport['attackers'], fought: number): { lost: number; pct: number } {
  const total = us.reduce((n, u) => n + u.men, 0);
  const lost = Math.max(fought, us.reduce((n, u) => n + u.lost, 0));
  return { lost, pct: total > 0 && lost > 0 ? Math.max(1, Math.round(100 * Math.min(1, lost / total))) : 0 };
}
/** (Version 63) The price of a battle in one sentence, to whichever side reads its report. */
function priceLine(s: Game, r: BattleReport): string {
  if (r.yielded) return '';
  const a = lostIn(r.attackers, r.attLost), d = lostIn(r.defenders, r.defLost);
  const num = (n: number): string => n.toLocaleString('en');
  // (a figure and its share are kept together where the line folds)
  const of = (x: { lost: number; pct: number }): string => `${num(x.lost)}${x.pct ? `\u00A0(${x.pct}%)` : ''}`;
  if (s.human && (r.attNat === s.human || r.attackers.some((u) => u.nat === s.human))) return `It cost you ${num(a.lost)} men${a.pct ? `, ${a.pct}% of those sent in,` : ''} against ${of(d)} of theirs.`;
  if (s.human && r.defenders.some((u) => u.nat === s.human)) return `It cost the attackers ${num(a.lost)} men${a.pct ? `, ${a.pct}% of those sent in,` : ''} against ${of(d)} of yours.`;
  return `The attackers lost ${of(a)} men, the defenders ${of(d)}.`;
}

/** (Version 61) An attack thrown back from dug-in troops still wears their works down: said to whichever side reads it. */
function wornNote(s: Game, r: BattleReport): string {
  const x = r as BattleReport & { worn?: number; works?: number };
  if (!x.worn) return '';
  const full = DEFENCE.entrench.months, left = x.works ?? 0;
  const took = x.worn >= 0.95 && x.worn < 1.05 ? 'a month' : `${x.worn} months`;
  const now = left < 0.05 ? 'nothing is left of them' : `they stand at ${left} of ${full} months`;
  const regrow = DEFENCE.wear.regrow;
  const pace = regrow <= 0 ? 'do no digging' : regrow >= 1 ? 'dig as usual' : regrow === 0.5 ? 'dig at half pace' : `get ${Math.round(regrow * 100)}% of their digging done`;
  return r.defNat === s.human
    ? ` The line held, but its works are worn: the attack took ${took} off them, and ${now}. Your troops there ${pace} this month. A quiet month builds the works up again.`
    : ` Thrown back, but not for nothing: the attack took ${took} off the defenders' works, and ${now}. The defenders ${pace} this month, so the next blow meets less. Guns in the attack wear works down faster.`;
}

/** (The design director, 17 November 2026: on a town no army holds, the ticket's forces line reads "against no army: the town guard of about 30,000", never "0 corps, 0 men". The rules' line is mended here until the rules word it so themselves.) */
const noArmy = <T extends { label: string; value: string }>(lines: T[]): T[] => lines.map((l) => (l.label === 'Forces' ? { ...l, value: l.value.replace('against 0 corps, 0 men: ', 'against no army: ') } : l));

/**
 * (Rules 84.1; rulings 175(c), 177 and 178.) What marching these corps onto enemy ground with no army in its field
 * will meet: the place falls with the move ('open', 'gates'), its town guard fights ('guard': the forecast of the
 * assault as the army arrives, and of the same assault made ready next month), corps hold the town ('garrison': a
 * siege begins), or the reports do not say ('unknown'). Null where the march is a plain move or a field battle.
 * For the card, never for every province the pointer crosses: the rules read the forecast off a copy of the campaign.
 */
/**
 * (The design director, 17 November 2026, on Brian's find of the same day: "if an enemy army is out of a city i
 * couldnt attack them".) An enemy army that is not next door: the march up and the attack are planned as one order,
 * and shown before anything is carried out. The rows are the provinces beside the enemy that the army in hand can
 * march to this month: the soonest first; among equals no river between it and the enemy, then the better ground to
 * stand on, then friendly ground before enemy ground. Each row says what is left on arrival, and its river and
 * ground in a word. A province whose town guard would fight for it has a card of its own and is no approach; nor is
 * one from which nothing in hand could go in. Where nothing beside the enemy can be reached this month, `none` says
 * so and names the nearest. The choice is never made silently: the game shows the first row and up to three more.
 */
// (Rules 84.5; the design director's ruling of 15:50 UTC and sheet 34.) The rules' reading of the approaches, read by
// name: the staff's pick first, each { region, left, river, ground }. Until it is bundled the bridge reckons them by
// the same rule and with the same mend: an approach is ground the army can stand on this month without a fight.
interface ApproachRule { region: number; left: number; river: boolean; ground: unknown }
const approachRule = rulesByName['approaches'] as ((g: Game, corps: Corps[], target: Region) => ApproachRule[]) | undefined;
type ArmySeen = { name: string; known: string; corpsCount: number; divs: number; men: number; line: string };
const armyAtRule = rulesByName['armyAt'] as ((g: Game, region: number, nat: string) => ArmySeen | null) | undefined;

function approach(ids: number[], to: number) {
  const s = game();
  const target = s.regions[to];
  type Row = { id: number; name: string; cost: number; left: number; attack: boolean; river: string; riverWords: string; down: boolean; ground: string; enemy: boolean; arrive: string; how: string; says: string; road: number[]; going: number[] };
  // (sheet 34) and the card's two sides: where the road starts, the hand in numbers, and the enemy as far as the fog shows
  const out = { ok: false, name: target?.name ?? '', rows: [] as Row[], none: '', from: '', ours: '', foe: '', foeName: '', foeLine: '' };
  const me = s.human;
  try {
    if (!target || !me || !humanUp(s) || onEve(s)) return out;
    if (!hostileUnits(s, target, me).length || whyNotAttack(s, target, me)) return out;
    const cs = corpsById(s, ids).filter((c) => canOrder(s, c) && !(c as { town?: unknown }).town);
    if (!cs.length || cs.some((c) => c.ap > 0 && s.regions[c.reg].nb.includes(to))) return out; // (next door already: the attack card is the order)
    const reaches = cs.map((c) => ({ c, m: reach(s, c) }));
    // One row: a province beside the enemy, the corps of the hand that reach it this month, and what is left on arrival.
    const rowFor = (j: number, ruled?: number): Row | null => {
      const r = s.regions[j];
      const going = reaches.filter(({ m }) => { const e = m.get(j); return !!e && !e.attack; });
      if (!going.length || going.every(({ c }) => blocked(s, c, target, j))) return null; // (nothing in hand could ever go in from there)
      const cost = Math.min(...going.map(({ m }) => m.get(j)!.cost));
      const lefts = going.map(({ c, m }) => ({ c, left: Math.max(0, c.ap - m.get(j)!.cost) }));
      const left = ruled ?? Math.max(0, ...lefts.map((x) => x.left));
      const attack = left > 0 && lefts.some(({ c, left: l }) => l > 0 && !blocked(s, c, target, j));
      const cross = riverBetween(s, j, to);
      const river = cross ? riverName(cross) : '';
      const down = !!cross && bridgeDown(s, cross);
      const arrive = attack ? `${left} move${left === 1 ? '' : 's'} left on arrival: you can attack this month`
        : left > 0 ? `${left} move${left === 1 ? '' : 's'} left on arrival, but the way in from there is barred to this army this month`
          : 'No move left on arrival: the attack waits for next month';
      // (its river in words, as sheet 34 has it; the ground goes by its key, and each game prints its own label for it)
      const riverWords = river ? `across the ${river.replace(/^the /i, '')}${down ? ', bridges down' : ''}` : 'no river to cross';
      const how = `${riverWords} \u00B7 ${r.terrain}`;
      // The road there, as the first corps that can go would march it.
      const first = going.slice().sort((a, b) => a.m.get(j)!.cost - b.m.get(j)!.cost)[0];
      const road: number[] = [];
      for (let at = j; at !== first.c.reg && at >= 0 && road.length < 32; at = first.m.get(at)?.prev ?? -1) road.unshift(at);
      // (The code checker, 17 November.) The corps of the hand that can reach it this month: they alone are sent by this row.
      return { id: j, name: r.name, cost, left, attack, river, riverWords, down, ground: r.terrain, enemy: false, arrive, how, says: '', road, going: going.map(({ c }) => c.id) };
    };
    let rows: Row[] = [];
    let ruled: ApproachRule[] | null = null;
    try { ruled = approachRule ? approachRule(s, cs, target) : null; } catch { ruled = null; }
    if (ruled) {
      // (Rules 84.5.) The rules' own approaches, in their order: the staff's pick first, and up to three more.
      rows = ruled.slice(0, 4).map((a) => rowFor(a.region, a.left)).filter((x): x is Row => x !== null);
    } else {
      for (const j of target.nb) {
        // (Sheet 34's mend, passed at 16:23 UTC.) Ground in enemy hands is never a row: marching there is an attack of its own.
        if (!allied(s, me, s.regions[j].ctrl)) continue;
        const row = rowFor(j);
        if (row) rows.push(row);
      }
      // The staff's pick: reached soonest; among equals no river between it and the enemy; then the better ground.
      rows.sort((a, b) => a.cost - b.cost || (a.river ? 1 : 0) - (b.river ? 1 : 0) || (TERRAIN_DEF[s.regions[b.id].terrain] ?? 1) - (TERRAIN_DEF[s.regions[a.id].terrain] ?? 1) || a.id - b.id);
      rows = rows.slice(0, 4);
    }
    if (rows.length) {
      // The card's two sides (sheet 34): the hand in numbers, and the enemy as far as the fog shows.
      const num = (n: number): string => Math.round(n).toLocaleString('en-US');
      const divs = (n: number): string => `${num(n)} division${Math.round(n) === 1 ? '' : 's'}`;
      out.from = s.regions[cs[0].reg]?.name ?? '';
      out.ours = `${cs.length} corps \u00B7 ${divs(cs.reduce((t, c) => t + c.divs, 0))} \u00B7 ${num(cs.reduce((t, c) => t + men(c), 0))} men`;
      const foe = hostileUnits(s, target, me)[0]?.nat ?? '';
      let seen: ArmySeen | null = null;
      try { seen = foe && armyAtRule ? armyAtRule(s, to, foe) : null; } catch { seen = null; }
      let siege: { by: string | null; town: string } | null = null;
      try { siege = townSiege(s, to); } catch { siege = null; }
      out.foe = foe;
      out.foeName = siege && siege.by === foe ? `Round ${siege.town}` : seen?.name ?? '';
      out.foeLine = !seen ? '' : seen.known === 'full' ? `${seen.corpsCount} corps \u00B7 ${divs(seen.divs)} \u00B7 ${num(seen.men)} men` : seen.line || `${seen.corpsCount} corps not identified`;
      return { ...out, ok: true, rows };
    }
    // Nothing beside the enemy can be reached this month: the fact first, then the cure (sheet 34), the nearest
    // ground of his side beside it by the kept march's own reckoning.
    let best: { name: string; months: number } | null = null;
    for (const j of target.nb) {
      if (!allied(s, me, s.regions[j].ctrl)) continue;
      for (const c of cs) {
        let q: ReturnType<typeof planMarch> | null = null;
        try { q = planMarch(s, c, j); } catch { q = null; }
        if (q && q.ok && !q.now && (!best || q.months < best.months)) best = { name: s.regions[j].name, months: q.months };
      }
    }
    out.none = best
      ? `No ground beside the army round ${target.name} can be reached this month. The nearest is ${best.name}, ${inMonths(best.months)}' march: right-click it to set off.`.replace("month' march", "month's march")
      : `No ground beside the army round ${target.name} can be reached this month.`;
    return out;
  } catch { return out; }
}

/**
 * (The design director, 17 November 2026: no press fights a battle whose odds have not been shown.) The garrison's
 * sortie against the army round its town, for the attack card: the same fields an attack's forecast has, the
 * garrison as the attackers and the besiegers as far as they are known. `refused` where the rules will not have a
 * sortie now (one a month from each town, nobody fit to go, no army round it); null where there is nothing to ask.
 */
function sallyLook(region: number) {
  const s = game();
  const r = s.regions[region];
  const me = s.human;
  if (!r || !me) return null;
  const why = whyNotSallyOut(s, region);
  if (why) return { region, refused: why };
  const corps = salliers(s, r, me);
  const a = corps.length ? previewSally(s, r, me) : null;
  const look = a ? sallyOutlook(s, region) : null;
  if (!a || !look) return null;
  let town = r.name;
  try { town = townSiege(s, region)?.town ?? r.name; } catch { town = r.name; }
  const v = verdictOf(look, 'sortie');
  const named = (c: Corps) => {
    const kn = c.nat !== me && !allied(s, c.nat, me) ? knowledgeOf(s, c) : { level: 'exact' as const, age: 0 };
    return kn.level === 'none' ? { id: c.id, nat: c.nat, name: 'A corps not identified', kind: '', divs: 0, men: 0, gen: null }
      : kn.level === 'report' ? { id: c.id, nat: c.nat, name: `${corpsNamed(s, c.nat, c.name)} (a report ${kn.age} month${kn.age === 1 ? '' : 's'} old)`, kind: c.kind, divs: c.divs, men: 0, gen: null }
        : { id: c.id, nat: c.nat, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null };
  };
  const ids = corps.map((c) => c.id);
  return {
    region, sortie: true, town,
    chancePct: oddsPct(look.exact ? look.chance : look.guess), chancePctLo: oddsPct(look.chanceLo), chancePctHi: oddsPct(look.chanceHi), hasChance: true, mass: '', massIds: [] as number[],
    chanceWords: chanceWords(look), cost: costLines(look), ids, opened: '', pickedIds: [] as number[],
    tone: v.tone, verdict: v.word,
    face: (look.face ?? []).map((l) => ({ sign: l.sign, text: l.text, unknown: !!l.unknown })),
    allies: [] as string[], exact: look.exact, lo: look.lo, hi: look.hi, chance: look.chance, chanceLo: look.chanceLo, chanceHi: look.chanceHi,
    note: look.exact ? '' : `${look.note}${look.wide && look.narrow ? ` ${look.narrow}` : ''}`, even: look.even,
    prize: `Win, and the army round ${town} is driven off: the siege is raised. The garrison goes back inside either way.`,
    attack: look.exact ? a.A : look.odds, defence: look.exact ? a.D : 1, odds: look.exact ? a.odds : look.odds,
    lines: briefTicket(look.exact ? a.lines : look.lines, look.face),
    attackers: corps.map((c) => ({ id: c.id, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: a.defenders.map(named),
  };
}

/** The sortie ordered from its card. The answer reads as an attack's does. */
function sally(region: number) {
  const s = game();
  const r = s.regions[region];
  const why = r ? whyNotSallyOut(s, region) : 'Pick a province.';
  const report = why ? null : orderSally(s, region);
  const paid = report?.paid && !report.yielded ? { text: report.paid.att, sign: report.paid.sign ?? '' } : null;
  return { report: namedReport(s, report), siege: false, fell: false, stands: '', marched: 0, note: report ? battleNote(s, report) : why ?? '', price: report ? priceLine(s, report) : '', paid, moved: [] as { id: number; path: number[] }[], view: view() };
}

/** (Rules 84.2.4.) Why the player's corps inside a province's town take no marching orders, in the rules' own sentence; '' where he has none there. */
function garrisonWhy(region: number) {
  try { return { why: whyNotOrderGarrison(game(), region) ?? '' }; } catch { return { why: '' }; }
}

function takeOutlook(ids: number[], to: number) {
  const s = game();
  let m: ReturnType<typeof marchOutlook> = null;
  try { m = marchOutlook(s, corpsById(s, ids), to); } catch { m = null; }
  if (!m) return null;
  const shape = (look: Look | null) => (!look ? null : {
    odds: look.odds, lo: look.lo, hi: look.hi, exact: look.exact, even: look.even,
    chance: look.exact ? look.chance : look.guess, chancePct: oddsPct(look.exact ? look.chance : look.guess), chancePctLo: oddsPct(look.chanceLo), chancePctHi: oddsPct(look.chanceHi),
    chanceWords: chanceWords(look), cost: costLines(look), note: look.exact ? '' : `${look.note}${look.wide && look.narrow ? ` ${look.narrow}` : ''}`,
    lines: noArmy(briefTicket(look.lines, look.face)),
    face: (look.face ?? []).map((l) => ({ sign: l.sign, text: l.text, unknown: !!l.unknown })),
  });
  const going = corpsById(s, m.going);
  const now = shape(m.outlook), prepared = shape(m.prepared);
  // (Rules 84.1: the verdict, the order the card lights and the sentence under each order are the rules' own.)
  const favours = m.favours ?? 'siege';
  const r = s.regions[to];
  const size = m.sizeWord.toLowerCase();
  const divisions = `${m.guard.toFixed(1)} division${m.guard === 1 ? '' : 's'}`;
  const guardMen = `about ${Math.round(m.guardMen).toLocaleString('en-US')} men`;
  // The walls, as the ticket states them: the first line of WHAT YOU FACE on the card.
  const wall = m.outlook?.lines.find((l) => l.label === 'The town');
  const walls = wall?.effect ? [{ sign: 'defence', text: `The walls of a ${size}: ${wall.effect.charAt(0).toLowerCase()}${wall.effect.slice(1)}.`, unknown: false }] : [];
  return {
    region: to, forecast: !!now, meets: m.meets, town: m.town, sizeWord: m.sizeWord, guard: m.guard, guardMen: m.guardMen, line: m.line, going: m.going, favours,
    // (the slip's lines, and the card's)
    forces: m.guard > 0 && (m.meets === 'guard' || m.meets === 'gates') ? `Town guard only: ${divisions}, ${guardMen}` : m.meets === 'open' ? `No troops here, and a ${size} has no guard` : '',
    order: m.meets === 'open' || m.meets === 'gates' ? 'March in and take it' : m.meets === 'guard' ? `March on the ${size}` : m.meets === 'garrison' ? 'March in and lay siege' : 'March up to it',
    guardName: `Town guard of ${m.town}`, guardLine: `${divisions} · ${guardMen}`, guardWho: 'Depot troops, police and militia · no army inside',
    nowLabel: 'SEND THEM IN  \u00B7  A HASTY ASSAULT AS YOU ARRIVE', laterLabel: 'LAY SIEGE FIRST  \u00B7  THE ASSAULT A MONTH LATER, MADE READY', waitSays: '',
    nowVerdict: m.outlook ? verdictOf(m.outlook, 'storm').word : '',
    nowSays: m.outlook ? `${verdictOf(m.outlook, 'storm').word}. ${m.ifStorm}` : '',
    laterVerdict: m.prepared ? verdictOf(m.prepared, 'storm').word : '',
    laterSays: m.prepared ? `${verdictOf(m.prepared, 'storm').word}. ${m.ifSiege}` : '',
    holder: r.ctrl, holderName: s.nations[r.ctrl]?.name ?? r.ctrl, prize: prizeLine(s, to), allies: [] as string[],
    attackers: going.map((c) => ({ id: c.id, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: [] as never[],
    // (where the guard will fight: the assault as the army arrives, in the fields an attack's forecast is read from)
    ...(now ? {
      attack: now.odds, defence: 1, odds: now.odds, exact: now.exact, lo: now.lo, hi: now.hi, even: now.even, note: now.note,
      hasChance: true, chancePct: now.chancePct, chancePctLo: now.chancePctLo, chancePctHi: now.chancePctHi, chanceWords: now.chanceWords,
      cost: now.cost, lines: now.lines, face: [...walls, ...now.face], ids: m.going, opened: '', pickedIds: [] as number[], mass: '', massIds: [] as number[],
    } : {}),
    prepared: prepared ? { odds: prepared.odds, exact: prepared.exact, lo: prepared.lo, hi: prepared.hi, chancePct: prepared.chancePct, chanceWords: prepared.chanceWords } : null,
  };
}

/**
 * (Rules 84.1.) The order to take a place: the corps march in, and where only the town's guard holds out they go at
 * the town as they arrive (`assault` false: they only close round it, and the assault can go in next month made
 * ready). The answer reads as an attack's does, with whether the place fell and what still holds the town.
 */
function take(ids: number[], to: number, assault = true) {
  const s = game();
  const cs = corpsById(s, ids);
  // The roads, read before anybody moves.
  const roads = new Map<number, number[]>();
  for (const c of cs) {
    const m = reach(s, c), end = m.get(to), path: number[] = [];
    if (end && !end.attack) for (let at = to; at !== c.reg && at >= 0; at = m.get(at)?.prev ?? -1) path.unshift(at);
    roads.set(c.id, path);
  }
  const before = new Map(cs.map((c) => [c.id, c.reg]));
  const res = orderTake(s, cs, to, assault);
  const moved = cs.filter((c) => !c.dead && c.reg !== before.get(c.id)).map((c) => {
    const road = roads.get(c.id) ?? [], at = road.indexOf(c.reg);
    return { id: c.id, path: at >= 0 ? road.slice(0, at + 1) : [c.reg] };
  });
  forgetMarch(s, moved.map((m) => m.id)); // (an order carried out ends a standing march; one refused leaves it standing)
  const report = res.report;
  const r = s.regions[to];
  const town = townSiege(s, to)?.town ?? r.name;
  const note = report ? battleNote(s, report)
    : !res.moved ? ''
      : res.fell ? `${r.name} is yours: your army has marched in.`
        : res.stands === 'garrison' ? `Your army has closed round ${town}: the siege begins.`
          : res.stands === 'guard' ? `Your army stands round ${town}, and its town guard holds out. The assault can go in next month made ready.`
            : '';
  const paid = report?.paid && !report.yielded ? { text: report.paid.att, sign: report.paid.sign ?? '' } : null;
  return { report: namedReport(s, report), siege: false, fell: res.fell, stands: res.stands ?? '', marched: res.moved, note, price: report ? priceLine(s, report) : '', paid, moved, view: view() };
}

/**
 * (Rules 84.1; ruling 182; the UI director's sheet 21.) The assault on a town the player's army already stands
 * round, for its card: no storm is fought without it. One forecast, in an attack's own fields; what waiting brings,
 * in the rules' own sentence where they have one; and which button the verdict lights. `refused` where the rules
 * will not have a storm this month (one assault a town a month); null where there is nothing to storm.
 */
function stormLook(region: number) {
  const s = game();
  const r = s.regions[region];
  if (!r || !s.human) return null;
  let v: ReturnType<typeof townSiege> = null;
  try { v = townSiege(s, region); } catch { v = null; }
  if (!v || !v.by || !allied(s, s.human, v.by) || !atWar(s, s.human, r.ctrl)) return null;
  const why = whyNotStormTown(s, region);
  if (why) return { region, refused: why };
  // (the rules' own choice: the storm now, hasty or made ready; the same storm a month later where it is hasty; the
  // order the verdict lights; and the sentence under each order)
  const choice = stormChoice(s, region);
  if (!choice) return null;
  const look = choice.outlook;
  const going = stormCandidates(s, region);
  const verdict = verdictOf(look, 'storm').word;
  const size = v.sizeWord.toLowerCase();
  // (The UI director, 18 November 2026: "0.0 divisions" reads as none. A guard under a division is "part of a division", as the province's card says.)
  const divisions = v.guard < 0.95 ? 'part of a division' : `${v.guard.toFixed(1)} division${v.guard.toFixed(1) === '1.0' ? '' : 's'}`;
  const menOfGuard = v.guard > 0 ? Math.round(guardMen(s, r)) : 0;
  const guardOnly = v.inside === 0;
  const wall = look.lines.find((l) => l.label === 'The town');
  const walls = wall?.effect ? [{ sign: 'defence', text: `The walls of a ${size}: ${wall.effect.charAt(0).toLowerCase()}${wall.effect.slice(1)}.`, unknown: false }] : [];
  const forces = look.lines.find((l) => l.label === 'Forces');
  const against = forces?.value && / against /.test(forces.value) ? forces.value.split(' against ')[1] : '';
  return {
    region, forecast: true, meets: guardOnly ? 'guard' : 'garrison', town: v.town, sizeWord: v.sizeWord, guard: v.guard, guardMen: menOfGuard, line: '', going: going.map((c) => c.id),
    favours: choice.favours, forces: '', order: '',
    guardName: guardOnly ? `Town guard of ${v.town}` : `Garrison of ${v.town}`,
    guardLine: guardOnly ? `${divisions} \u00B7 about ${menOfGuard.toLocaleString('en-US')} men` : `${v.inside < 0 ? 'strength not known' : `${v.inside} corps inside`}${v.guard > 0 ? ` \u00B7 and its town guard, ${divisions}` : ''}`,
    guardWho: guardOnly ? 'Depot troops, police and militia \u00B7 no army inside' : against ? `${against.charAt(0).toUpperCase()}${against.slice(1)}` : '',
    nowLabel: choice.hasty ? 'SEND THEM IN  \u00B7  A HASTY ASSAULT' : 'SEND THEM IN  \u00B7  THE ASSAULT, MADE READY',
    laterLabel: choice.prepared ? 'KEEP UP THE SIEGE  \u00B7  THE ASSAULT A MONTH LATER, MADE READY' : choice.ifWait ? 'KEEP UP THE SIEGE  \u00B7  WHAT WAITING BRINGS' : '',
    waitSays: choice.prepared ? '' : choice.ifWait,
    nowVerdict: verdict, nowSays: `${verdict}. ${choice.ifStorm}`,
    laterVerdict: choice.prepared ? verdictOf(choice.prepared, 'storm').word : '', laterSays: choice.prepared ? `${verdictOf(choice.prepared, 'storm').word}.` : '',
    holder: r.ctrl, holderName: s.nations[r.ctrl]?.name ?? r.ctrl, prize: prizeLine(s, region), allies: [] as string[],
    attackers: going.map((c) => ({ id: c.id, name: corpsNamed(s, c.nat, c.name), kind: c.kind, divs: c.divs, men: men(c), gen: generalOf(s, c)?.name ?? null })),
    defenders: [] as never[],
    attack: look.odds, defence: 1, odds: look.odds, exact: look.exact, lo: look.lo, hi: look.hi, even: look.even, note: look.exact ? '' : `${look.note}${look.wide && look.narrow ? ` ${look.narrow}` : ''}`,
    hasChance: true, chancePct: oddsPct(look.exact ? look.chance : look.guess), chancePctLo: oddsPct(look.chanceLo), chancePctHi: oddsPct(look.chanceHi), chanceWords: chanceWords(look),
    cost: costLines(look), lines: noArmy(briefTicket(look.lines, look.face)), face: [...walls, ...(look.face ?? []).map((l) => ({ sign: l.sign, text: l.text, unknown: !!l.unknown }))],
    ids: going.map((c) => c.id), opened: '', pickedIds: [] as number[], mass: '', massIds: [] as number[],
    prepared: choice.prepared ? { odds: choice.prepared.odds, exact: choice.prepared.exact, lo: choice.prepared.lo, hi: choice.prepared.hi, chancePct: oddsPct(choice.prepared.exact ? choice.prepared.chance : choice.prepared.guess), chanceWords: chanceWords(choice.prepared) } : null,
  };
}

/** (Rules 84.1; ruling 182.) The storm ordered from its card: every corps of the player's round the town goes in. The answer reads as an attack's does. */
function storm(region: number) {
  const s = game();
  const r = s.regions[region];
  const why = r ? whyNotStormTown(s, region) : 'Pick a province.';
  const report = why ? null : orderStorm(s, region);
  const fell = !!report && !!s.human && !!r && allied(s, s.human, r.ctrl);
  let inside = 0;
  try { inside = townSiege(s, region)?.inside ?? 0; } catch { inside = 0; }
  const paid = report?.paid && !report.yielded ? { text: report.paid.att, sign: report.paid.sign ?? '' } : null;
  return { report: namedReport(s, report), siege: false, fell, stands: fell || !report ? '' : inside > 0 ? 'garrison' : 'guard', marched: 0, note: report ? battleNote(s, report) : why ?? '', price: report ? priceLine(s, report) : '', paid, moved: [] as { id: number; path: number[] }[], view: view() };
}

/** (Rules 84.1; ruling 176(e).) The gate's question for its holder: what the order does and whose trade it costs, as rows, before he gives it. */
function gateAsk(gate: number) {
  const s = game();
  const why = whyNotGate(s, gate);
  const q = why ? null : gateQuestion(s, gate);
  if (!q) return { ok: false, why: why ?? '' };
  // (the loss is in the rules' money: shown as every other sum is, by the budget's scale. Rules 84.2: his own row
  // stands first, always; the last row of the tally is the others' alone, and it is the others' rows added up.)
  const fig = (n: number): string => `${shown(n).toLocaleString('en-US')} a month`;
  const others = q.rows.filter((r) => !r.own);
  return {
    ok: true, why: '', order: q.order, label: q.label, question: q.question, lead: q.lead, heading: q.heading, holder: q.gate.holder, name: q.gate.name,
    rows: q.rows.map((r) => ({ nat: r.nat, name: r.own ? 'You' : r.name, what: r.what, loss: shown(r.loss) > 0 ? `\u2212${fig(r.loss)}` : '' })),
    total: others.length > 1 ? `${q.sumLabel}: \u2212${others.reduce((n, r) => n + shown(r.loss), 0).toLocaleString('en-US')} a month` : '',
    none: others.length ? '' : q.none,
    // (ruling 177(a)) who resents it, a row each, and what that comes to
    resentHeading: q.resent.length ? q.resentHeading : '', resentNote: q.resentNote, resent: q.resent.map((r) => `${r.name}: ${r.line}`),
    yes: q.yes, no: q.no,
  };
}

/** (Rules 84.2.) The warning before the last corps leaves a gate's province: "If they go, the Panama Canal is left to its town guard: about 4,800 men, and no army." Empty otherwise. */
function gateLeftOf(ids: number[]) {
  const s = game();
  let line = '';
  try { line = gateLeft(s, corpsById(s, ids))?.ifLine ?? ''; } catch { line = ''; }
  return { line };
}

/** (Rules 84.1.) The holder's order: shuts his gate to all ships but his own and his allies', or opens it again. */
function gateOrder(gate: number, shut: boolean) {
  const s = game();
  const why = whyNotGate(s, gate);
  const ok = !why && orderGate(s, gate, shut);
  let note = why ?? '';
  try { if (ok) note = gateViews(s, s.human ?? null)[gate]?.line ?? ''; } catch { /* (the order stands without its line) */ }
  return { ok, note, view: view() };
}

/**
 * The player's attack. Fought out on the field (release 1.5: the land rules' battle, battle-bridge.ts battleSettleLand),
 * 'fought' is the field's R in place of the dice and 'field' what the field cost each corps and side (ruling 226(6));
 * without them the dice roll as always.
 */
function attack(ids: number[], to: number, fought?: number, field?: FieldBill) {
  const s = game();
  const before = new Map(s.corps.map((c) => [c.id, c.reg]));
  const sent = attackers(s, ids, to);
  const report = orderAttack(s, sent, s.regions[to], fought, field);
  if (report) forgetMarch(s, sent.map((c) => c.id)); // (an order carried out ends a standing march; one refused leaves it standing)
  // Corps that followed up into the ground they won, for the game to march across.
  const moved = report
    ? s.corps.filter((c) => !c.dead && (c.nat === s.human || (!!s.human && c.joint === s.human)) && c.reg === to && before.get(c.id) !== to).map((c) => ({ id: c.id, path: [to] }))
    : [];
  // (where the town still holds, the province has not changed hands: the screen shows no capture)
  const siege = !!(report as (BattleReport & { siege?: boolean }) | null)?.siege;
  // (Version 63) The price in a sentence, for the result card; the report's own ticket reads in the past tense.
  // (Version 84) Who paid and to what, under the price: one sentence or two, as the attacker reads it.
  const paid = report?.paid && !report.yielded ? { text: report.paid.att, sign: report.paid.sign ?? '' } : null;
  return { report: namedReport(s, report && siege ? { ...report, taken: false } : report), siege, note: report ? battleNote(s, report) : '', price: report ? priceLine(s, report) : '', paid, moved, view: view() };
}

// ---- The month on one card ---------------------------------------------------------------------------
// (Brian, 16 November 2026: "i dont love the turn update UI telling you what happend. its very plain and hard to
// read". The design director's rulings 64, 65, 84 and 95 set what is on the card, in what order and by what weight;
// the art director's brief sets its look.) The card is the rules' own (src/campaign/monthcard.ts, Version 82): the
// same card, in the same order, as the browser's. The bridge keeps the mark the rules ask an interface to keep (as
// the player's month opens, and what he has on order as he ends it) and hands the card to the screen in the shape
// the screen reads. Display only.

interface CardFigure { value: string; label: string }
interface MonthCard {
  /** (Rules 84.3.3; ruling 123(b).) The card that opens a campaign: headed by the moment, "As September 1939 opens". */
  opening?: boolean;
  date: string;
  bands: { kind: string; flags: string[]; text: string; tip: string; week: number; when: string; region: number; title: string; sub: string; note: string; good: boolean; spent: boolean; alarm: boolean }[];
  score: { label: string; sign: string; figure: string; sub: string; tone: number }[];
  lead: null | { tag: string; stamp: string; tone: number; headline: string; a: string; aName: string; b: string; bName: string; picture: string; line: string; figures: CardFigure[]; region: number; report: number };
  rows: { picture: string; sign: string; place: string; tag: string; tone: number; a: string; b: string; join: string; joinName: string; text: string; figures: CardFigure[]; region: number; sea: number }[];
  none: string; foot: string; count: string;
  home: { sign: string; count: string; text: string; where: string; grey: boolean; tip: string }[];
  staff: { text: string; tip: string; desk: string; act: string }[];
  elsewhere: { nat: string; text: string; tip: string }[];
  /** What is said at the card's foot of the news that found no line: "3 more lines of news are in the Newsreel." */
  rest: string;
}

/** The mark kept as the player's month opened: what the card that closes it is counted from. */
let monthBook: MonthMark | null = null;
/** The last month's card, kept so that the report can be opened again. */
let lastCard: MonthCard | null = null;

function openMonthBook(s: Game): void {
  // (the rules' own mark: in a campaign's first month it reaches back to the war's opening, so that the declaration
  // that began it is on the first card)
  monthBook = s.human && s.nations[s.human] ? markOfRules(s) : null;
}

const cardNum = (n: number): string => Math.round(n).toLocaleString('en-US');

/** A mark for a month whose opening was not seen (a game picked up from its file): everything of that month counts, and nothing of who held what before. */
function markBackTo(s: Game, month0: number): MonthMark {
  const before = <T extends { turn: number }>(list: T[]): number => { const i = list.findIndex((x) => x.turn >= month0); return i < 0 ? list.length : i; };
  return { ...monthMark(s), turn: month0, h: before(s.headlines), r: before(s.reports), s: before(s.seaReports), a: before(s.airReports), shore: before(s.shoreReports ?? []), orders: [] };
}

/** The rules' card in the shape the screen reads. */
function shownCard(s: Game, c: RuleCard): MonthCard {
  const tone = (x: CardTone): number => (x === 'good' ? 1 : x === 'bad' ? -1 : x === 'warn' ? 2 : 0);
  const figures = (fs: RuleFigure[]): CardFigure[] => fs.map((f) => ({ value: f.n, label: f.label }));
  const name = (nat: string | null): string => (nat ? s.nations[nat]?.name ?? nat : '');
  const boxSign: Record<string, string> = { provinces: 'flag', battles: '', men: 'inf', sea: 'hull', air: 'ftr', treasury: '' };
  const lead = c.lead;
  // (The art director's brief, "What the browser build taught".) Each foot box holds three lines beside a lead story
  // and five on a quieter card; what is over is counted, not lost. At home: what is ready first, then research,
  // then what next month brings, then the other news of home. Deliveries that find no line are counted in one;
  // news that finds none is in the Newsreel.
  const lines = lead ? 3 : 5;
  type HomeLine = { sign: string; count: string; text: string; where: string; grey: boolean; tip: string; ready: number; next: number };
  const homeAll: HomeLine[] = [
    ...c.home.filter((x) => !x.next).map((x) => ({ sign: x.sign, count: String(x.count), text: x.text, where: x.place, grey: false, tip: '', ready: x.count, next: 0 })),
    ...c.research.map((text) => ({ sign: 'star', count: '', text: `Research complete: ${text}`, where: '', grey: false, tip: '', ready: 0, next: 0 })),
    ...c.home.filter((x) => x.next).map((x) => ({ sign: x.sign, count: String(x.count), text: x.text, where: x.place, grey: true, tip: '', ready: 0, next: x.count })),
    ...c.also.map((text, i) => ({ sign: '', count: '', text, where: '', grey: false, tip: c.alsoFull[i] ?? text, ready: 0, next: 0 })),
  ];
  let home = homeAll.slice(0, lines), homeOver = 0;
  if (homeAll.length > lines) {
    const hid = homeAll.slice(lines - 1);
    const ready = hid.reduce((n, x) => n + x.ready, 0), next = hid.reduce((n, x) => n + x.next, 0);
    const said = [ready ? `${ready} more ready` : '', next ? `${next} due next month` : ''].filter(Boolean).join('  \u00B7  ');
    if (said) home = [...homeAll.slice(0, lines - 1), { sign: '', count: '', text: said, where: '', grey: true, tip: '', ready: 0, next: 0 }];
    homeOver = (said ? hid : homeAll.slice(lines)).filter((x) => !x.ready && !x.next).length;
  }
  // (A desk's first line carries its button, and the first lines of the desks come first.)
  const firsts = c.staff.filter((x, i) => c.staff.findIndex((y) => y.desk === x.desk) === i);
  const staffAll = [...firsts, ...c.staff.filter((x) => !firsts.includes(x))];
  const over = c.more + homeOver + Math.max(0, staffAll.length - lines) + Math.max(0, c.elsewhere.length - lines) + c.letters.length;
  return {
    date: c.month,
    // (Ruling 102(a): the rules give each item its date in words, "week 3" or "as July opens"; nothing else is printed for a date.)
    // (Version 83.1: a band's own title, the second line it must be read with, the sentence that says what to do, and how its edge is worn.)
    bands: c.bands.map((x) => ({ kind: x.word, flags: x.nats.slice(0, 3), text: x.head, tip: x.text, week: x.week, when: x.when ?? '', region: x.region ?? -1,
      title: x.title ?? '', sub: x.sub ?? '', note: x.note ?? '', good: !!x.good, spent: !!x.spent,
      // (Rules 84.2.6; ruling 211(c).) A warning that ends with the help of his home ground's first alarm: its SHOW ON MAP is a press on the red row.
      alarm: !!x.alarm })),
    score: c.boxes.map((x) => ({ label: x.label.toUpperCase(), sign: boxSign[x.key] ?? '', figure: x.figure, sub: x.sub, tone: x.tone === 'good' ? 1 : x.tone === 'bad' ? -1 : 0 })),
    lead: lead ? {
      tag: lead.when ? `${lead.kind.toUpperCase()}  ·  ${lead.when.toUpperCase()}` : lead.kind.toUpperCase(), stamp: lead.tag, tone: tone(lead.tone), headline: lead.head,
      a: lead.ours ?? '', aName: name(lead.ours), b: lead.theirs ?? '', bName: name(lead.theirs), picture: lead.picture ?? '',
      line: lead.follows || lead.line, figures: figures(lead.facts), region: lead.region ?? -1, report: lead.report?.type === 'land' ? lead.report.id : -1,
    } : null,
    // (A province has a place on the map and a sea has one on the chart: either way the row carries the pin.)
    // (Rules 84.2.6.) The word that goes with the tag and the other side's name: "WON from", "LOST to", "HELD against", "BESIEGED by".
    rows: c.rows.map((x) => ({ picture: x.picture ?? '', sign: x.sign, place: x.place, tag: x.tag, tone: tone(x.tone), a: x.ours ?? '', b: x.theirs ?? '', join: x.join?.word ?? '', joinName: x.join?.name ?? '', text: x.line, figures: figures(x.figures), region: x.region ?? -1, sea: x.sea ?? -1 })),
    none: c.quiet, foot: c.foot, count: c.count,
    home: home.map((x) => ({ sign: x.sign, count: x.count, text: x.text, where: x.where, grey: x.grey, tip: x.tip })),
    staff: staffAll.slice(0, lines).map((x) => ({ text: x.short || x.text, tip: x.text, desk: String(x.desk), act: firsts.includes(x) ? `desk:${x.desk}:own` : '' })),
    elsewhere: c.elsewhere.slice(0, lines).map((x) => ({ nat: x.nat, text: x.text, tip: x.full ?? x.text })),
    rest: over > 0 ? `${over} more ${over === 1 ? 'line' : 'lines'} of news ${over === 1 ? 'is' : 'are'} in the Newsreel.` : "Every figure comes from the month's own battle reports.",
  };
}

function endMonth() {
  const s = game();
  const h0 = s.headlines.length;
  // Who stands as the month ends, to tell afterwards who has been struck from the map and who has come back.
  const stoodUp = new Set(Object.values(s.nations).filter((n) => n.alive && !n.dormant).map((n) => n.id));
  const wasDown = new Set(Object.values(s.nations).filter((n) => !n.alive && !n.dormant).map((n) => n.id));
  const steps: unknown[] = [];
  let fallen: string | null = null;
  let marched: { note: string; region: number }[] = [];
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
      try { if (monthBook) monthBook.orders = monthOrders(s); } catch { /* the card tells of home without it */ }
      // The raids to read next month are those filed from here on (airpages.ts raidsToRead): his own side's as his
      // month closes, the other nations', and those he orders then.
      s.flags.raidsFrom = air0;
      endHuman(s);
    }
    const air = raidsSince(s, air0, [month, s.turn]);
    if (air.flights.length) steps.push({ nat: me, note: air.notes.slice(0, 2).filter(Boolean).join(' '), taken: [], battles: [], focus: air.focus.length ? air.focus : air.flights.map((f) => f.from), mine: true, raids: air.flights, sunk: strikeLossesSince(s, air0), view: view() });
    const sea = seaFightsSince(s, sea00);
    if (sea.zones.length) steps.push({ nat: me, note: sea.notes.slice(0, 2).join(' '), taken: [], battles: [], focus: [], mine: true, seas: sea.zones, sunk: sea.sunk, view: view() });
    runOthers(s, steps);
    fallen = arrive(s);
    // (release 1) The player's armies on a standing march go on as his month opens: shown as the month's last move.
    if (!fallen && s.human) {
      // (as his month opens, last month's orders to stand are forgotten)
      try { settleStands(s); } catch { /* the marks are dated: an old one counts for nothing */ }
      const out = advanceMarches(s);
      marched = out.ended;
      const said = [out.went.length ? `${out.went.length === 1 ? 'One of your corps is' : `${out.went.length} of your corps are`} on the march.` : '', ...out.ended.slice(0, 2).map((x) => x.note)].filter(Boolean).join(' ');
      if (said) steps.push({ nat: s.human, note: said, taken: [], battles: [], focus: [...out.ended.map((x) => x.region), ...out.went].slice(0, 6), mine: false, moved: true, view: view() });
    }
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
        nat: late[0].attNat, taken: [], battles: [], focus: [], seas: zones, sunk: all.sunk.filter((x) => zones.includes(x.zone)), mine: late.some((r) => r.attNat === s.human || r.defNat === s.human),
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
    // (Rules 84.2.5; ruling 211(b).) A line of news with a title of its own: "Britain stands by Canada".
    head: (h as { head?: string }).head ?? '',
  }));
  // The few lines worth telling first: declarations of war, countries falling, events, honours won and letters home.
  const weight: Record<string, number> = { fall: 0, war: 1, goal: 2, honour: 3, event: 4, letter: 5 };
  // (news with a title of its own is told first beside the declarations of war: Britain stands by Canada, or stays out)
  const rank = (h: { kind: string; head: string }): number => (h.head ? 1.5 : weight[h.kind]);
  const first = headlines.filter((h) => h.kind in weight || !!h.head).sort((a, b) => rank(a) - rank(b));
  const events = (first.length ? first : headlines).slice(0, 12).map((h) => ({ kind: h.kind, text: h.text }));
  // The great moments of the month, each told on a screen of its own: a nation eliminated, a nation liberated.
  const said = (nat: string): string => [...headlines].reverse().find((h) => h.nat === nat && (h.kind === 'fall' || h.kind === 'event' || h.kind === 'war'))?.text ?? '';
  const announce: { nat: string; title: string; text: string; good: boolean; kicker?: string; more?: string; facts?: { label: string; value: string }[]; region?: number; ending?: boolean }[] = [];
  const capitalOfNat = (nat: string): number => s.regions.find((r) => r.cap && r.owner === nat)?.id ?? -1;
  // (Version 72) A headline may carry a warning addressed to the player, with a short title: each is set out as a
  // card of its own, with the place to look at where it names one (the coast the shipping is gathering against).
  for (const h of s.headlines.slice(h0) as { nat?: string; text: string; warn?: string; region?: number }[]) {
    if (h.warn && s.human && h.nat === s.human) announce.push({ nat: s.human, title: h.warn.toUpperCase(), text: h.text, good: false, kicker: 'A WARNING', region: typeof h.region === 'number' ? h.region : undefined });
  }
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
    if (stoodUp.has(n.id) && !n.alive) announce.push({ nat: n.id, title: theWords(`${n.name} has fallen`).toUpperCase(), text: said(n.id) || `${upFirst(theName(s, n.id))} is eliminated from the war.`, good: foe });
    else if (wasDown.has(n.id) && n.alive) announce.push({ nat: n.id, title: theWords(`${n.name} is liberated`).toUpperCase(), text: said(n.id) || `${upFirst(theName(s, n.id))} is free again, and back in the war.`, good: mine || !foe });
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
        region: capitalOfNat(me0), good: now === 'won', ending: true,
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
  // (Rules 84.2.5) a declaration with a title of its own is told under that title: BRITAIN STANDS BY CANADA
  for (const h of [...wars.filter(touches), ...wars.filter((x) => !touches(x))].slice(0, 2)) reel.push({ nat: h.nat ?? '', title: h.head ? h.head.toUpperCase() : 'WAR IS DECLARED', text: short(h.text), region: capitalOf(h.nat) });
  // and news with a title of its own that is no war, where it touches him (Britain stays out of Canada's war)
  for (const h of headlines.filter((x) => !!x.head && x.kind !== 'war' && x.kind !== 'fall' && touches(x)).slice(0, 1)) reel.push({ nat: h.nat ?? '', title: h.head.toUpperCase(), text: short(h.text), region: capitalOf(h.nat) });
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
    rows: (steps as { nat: string; note: string; focus?: number[]; mine?: boolean; moved?: boolean }[]).filter((x) => x.note?.trim() && !x.moved).map((x) => ({ nat: x.nat, note: x.note.trim(), region: x.focus?.[0] ?? -1, mine: !!x.mine })),
    news: events.map((e) => e.text),
  };
  // (every march that ended is told with the player's own fronts, with the way to go and look at the army)
  if (s.human) for (const x of marched) lastMonth.rows.push({ nat: s.human, note: x.note, region: x.region, mine: true });
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
  // (Rulings 64 and 65.) The month on one card; and the book is opened for the month that now begins.
  let month: MonthCard | null = null;
  try {
    const freed = cards.filter((c) => c.kind === 'freed').map((c) => ({ nat: c.nat, text: c.text, region: typeof c.region === 'number' ? c.region : -1 }));
    void freed;
    // (The UI director's review of build 1628: no sentence is cut with three dots. The card's rows and boxes wrap and
    // grow here, so the rules give every line whole.)
    month = fallen || !s.human ? null : shownCard(s, cardOfRules(s, monthBook ?? markBackTo(s, month0), s.human, { whole: true }));
  } catch { month = null; }
  lastCard = month;
  try { openMonthBook(s); } catch { monthBook = null; }
  // (Release 1.5.1; Brian, 24 November 2026 18:30Z) The units handed over to him this month, for the table to say plainly.
  const ready = fallen ? [] : readyOf(s, h0);
  return { steps, headlines, events, fallen, announce, newsreel, year, month, ready, view: view() };
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
  const asked = corpsById(s, ids).filter((c) => c.reg !== to);
  const went: string[] = [];
  for (const c of asked) if (orderRail(s, c, to)) { forgetMarch(s, [c.id]); moved.push({ id: c.id, path: [to] }); went.push(c.name); } // (an order carried out ends a standing march)
  // (the message line, as the browser says it)
  const place = s.regions[to]?.name ?? '', left = asked.length - went.length;
  const said = went.length ? `${went.length === 1 ? `${went[0]} goes` : `${went.length} corps go`} to ${place} by rail: there this month, the whole month spent.${left ? ` ${left === 1 ? 'One corps stays' : `${left} corps stay`} behind: ${place} has no room for more.` : ''}` : '';
  return { moved, said, view: view() };
}

/**
 * (Brian, 16 November 2026; rulings 110, 112, 116, 118; the art director's railway brief.) What the railway shows
 * for the army in hand, all of it from the rules' shared functions: whether it can ride and the one line that says
 * so; every working track a train could carry it along; where a train would take it beyond its march (`march` is the
 * provinces it can walk to this month, which the table has from the march's reach); the provinces that take a station
 * dot (none on the steepest ground for armour, and none on a cut, which has its lamp); the lit lines that stop at a
 * cut; and the cuts engineers in hand can be carried to.
 */
/**
 * (One voice after a press: the design director's note of 17 November.) The tray's rail line names whom it is true
 * of. Where no corps in hand can ride, each for the same reason, it speaks of them together ("No train for the
 * American army this month: ..."), never of one corps of three; where it names one corps, that corps goes by its own
 * name. Any other line stands as the rules give it.
 */
function trainWho(s: Game, cs: Corps[], line: string, army: string, armyIds: number[]): string {
  const head = 'No train for \u0000 this month:';
  if (cs.length > 1) {
    const lines = cs.map((c) => { const no = whyNoTrain(s, c); return no ? no.line.split(c.name).join('\u0000') : null; });
    const first = lines[0];
    if (first && first.startsWith(head) && lines.every((l) => l === first)) {
      const { who, many } = whoOf(cs, army, armyIds);
      const rest = first.slice(head.length);
      return `No train for ${who} this month:${many ? rest.replace(/\bit has\b/g, 'they have').replace(/\bits month\b/g, 'their month') : rest}`;
    }
  }
  let out = line;
  for (const c of cs) if (shortName(c.name) !== c.name) out = out.split(c.name).join(shortName(c.name));
  return out;
}

function railShow(ids: number[], march: number[], army = '', armyIds: number[] = []) {
  const s = game();
  const none = { can: false, strong: '', line: '', tracks: [] as number[][], reach: [] as number[], dots: [] as number[], tint: [] as number[], bars: [] as number[][], ringed: [] as number[] };
  try {
    if (!humanUp(s) || onEve(s)) return none;
    const cs = corpsById(s, ids).filter((c) => !c.dead && canOrder(s, c));
    if (!cs.length) return none;
    const walked = new Set<number>(march);
    const reach = [...railReachFor(s, cs)];
    const beyond = reach.filter((id) => !walked.has(id));
    const said = railSays(s, cs, beyond.length, 'right-click', { one: 'the station dot', many: 'a station dot' });
    const tracks = litTracks(s, cs);
    const cuts = cutsKnown(s);
    const steep = new Set<string>(GROUND.steep.regions);
    const barred = cs.some((c) => GROUND.steep.barred.includes(c.kind));
    const lit = new Set<number>(), pair = new Set<string>();
    for (const [a, b] of tracks) { lit.add(a); lit.add(b); pair.add(`${Math.min(a, b)}:${Math.max(a, b)}`); }
    const bars: number[][] = [];
    for (const a of lit) {
      if (cuts.has(a)) continue;
      for (const b of railsAt(a)) if (cuts.has(b) && !pair.has(`${Math.min(a, b)}:${Math.max(a, b)}`)) bars.push([a, b]);
    }
    // (The UI director, 18 November 2026.) Corps that are a town's garrison by choice were told of a siege that is not
    // there ("... is shut in the town at New York. No train leaves a siege."). Until the rules' own new line arrives
    // (the rules builder's 3c897ba), a garrison of a town nobody besieges is told what it is and how it comes out.
    let line = said ? trainWho(s, cs, said.line, army, armyIds) : '';
    try {
      const seat = townSiege(s, cs[0].reg);
      const inTown = cs.every((c) => !!(c as { town?: boolean }).town || !!(c as { garrison?: unknown }).garrison);
      if (said && !said.can && seat && !seat.by && inTown && / is shut in the town /.test(said.line)) {
        // (one corps by its own name, as the banner that sent it in names it; several as the army in hand is named)
        const named = whoOf(cs, army, armyIds);
        const who = named.who.charAt(0).toUpperCase() + named.who.slice(1);
        const stronger = /fights? (\d+)% stronger/.exec(seat.lines.join(' '))?.[1];
        line = `${who} ${named.many ? 'are' : 'is'} ${seat.town}'s garrison${stronger ? `: inside the town ${named.many ? 'they fight' : 'it fights'} ${stronger}% stronger` : ''}. TAKE THE FIELD brings ${named.many ? 'them' : 'it'} out.`;
      }
    } catch { /* the rules' own line stands */ }
    return {
      can: !!said?.can, strong: said?.strong ?? '', line, tracks, reach,
      dots: beyond.filter((id) => !cuts.has(id) && !(barred && steep.has(s.regions[id].name))),
      tint: beyond, bars, ringed: reach.filter((id) => cuts.has(id)),
    };
  } catch { return none; }
}

/**
 * The words under the pointer for a province beyond the march of the army in hand (the art director's railway brief,
 * section 8): the rules' one line where a train would take it there; or why none will, with the opening that takes
 * the emphasis where the reason is a break he can act on, and whether that reason is a cut at this very province (so
 * that the cut is said once).
 */
function railTip(ids: number[], to: number) {
  const s = game();
  const none = { ride: '', relay: '', no: '', strong: '', cutHere: false };
  try {
    const cs = corpsById(s, ids).filter((c) => !c.dead && canOrder(s, c) && c.reg !== to);
    if (!cs.length || !s.regions[to] || !humanUp(s)) return none;
    if (railReachFor(s, cs).has(to)) {
      const said = rideLines(s, cs, to, 'Right-click');
      return { ...none, ride: said.line, relay: said.relay };
    }
    const no = whyNoTrainFor(s, cs, to);
    return no ? { ...none, no: no.line, strong: no.strong ?? '', cutHere: no.code === 'cut' && no.at === to } : none;
  } catch { return none; }
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

/**
 * (The UI director, 18 November 2026; ruling 204.) Why the chosen corps cannot go by sea to a place the by-sea offer
 * leaves out: the first test the place fails, in the rules' own words (to friendly ground, the first corps that
 * cannot be shipped there; to any other, the landing). Empty where the rules have nothing against it.
 */
function bySeaWhy(ids: number[], to: number): { why: string } {
  const s = game();
  const r = s.regions[to];
  const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
  if (!r || !cs.length) return { why: '' };
  if (allied(s, cs[0].nat, r.ctrl)) {
    for (const c of cs) { const why = whyNotShip(s, c, r); if (why) return { why }; }
    return { why: '' };
  }
  return { why: whyNotLand(s, cs, r) ?? '' };
}

/** Ships the chosen corps to a friendly port. */
function ship(ids: number[], to: number) {
  const s = game();
  const moved: { id: number; path: number[] }[] = [];
  for (const c of corpsById(s, ids)) if (c.reg !== to && orderShip(s, c, to)) { forgetMarch(s, [c.id]); moved.push({ id: c.id, path: [to] }); } // (an order carried out ends a standing march)
  return { moved, view: view() };
}

/** What a landing on an enemy coast would meet, before the order is given. */
function landingPlan(ids: number[], to: number) {
  const s = game();
  const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
  const p = previewLanding(s, cs, to);
  const lines: string[] = [];
  if (!p.why) {
    // (Versions 74 and 76, the design director's rulings 24, 36 and 37.) The headline speaks for the whole venture and
    // is never more cheerful than the worst line on the card. What applies, worst first: a mainland beach that can
    // be cut off (red); a crossing that would lose one man in four or more at sea; a whole island that can be cut
    // off, where the force lives a while on what it carries; a contested sea (these three amber). The worst leads,
    // the next is the line under it, and the fight for the beach then has a line of its own. A fight that is itself
    // all but lost keeps the headline, with the worst warning under it.
    const holdNow = p.air ? null : ((p as { hold?: SeaHoldView }).hold ?? null);
    const seaBehind = holdNow ? s.seas[holdNow.sea]?.name ?? 'sea' : 'sea';
    const place = s.regions[to].name;
    const lossAtSea = p.air ? 0 : Math.round(p.risk * NAVY.intercept.loss * 100);
    const months = ['no', 'one', 'two', 'three', 'four', 'five', 'six'][holdNow?.months ?? 0] ?? String(holdNow?.months ?? 0);
    const warns: { tone: 'red' | 'amber'; head: string; text: string; also: string }[] = [];
    if (holdNow && !holdNow.open && !holdNow.island) warns.push({ tone: 'red', head: 'Ashore, then cut off', text: `The force can land, but the enemy can close the ${seaBehind} behind it and the troops would be cut off from their supplies.`, also: `And a force that did get ashore could be cut off: the enemy can close the ${seaBehind} behind it.` });
    // (Version 83.1, ruling 134(d).) The test of strength's own head, in the rules' words: red, third in the order,
    // after the fight's own and ASHORE, THEN CUT OFF. The order to sail stays his.
    const said = ashoreSays(s, cs, s.regions[to]);
    if (said?.warn) warns.push({ tone: 'red', head: said.warn.head, text: said.warn.text, also: said.warn.also });
    if (!p.surprise && !p.air && lossAtSea >= 25) warns.push({ tone: 'amber', head: 'A costly crossing', text: `About ${lossAtSea}% of the force would be lost at sea before it lands.`, also: `And about ${lossAtSea}% of the force would be lost at sea before it lands.` });
    if (holdNow && !holdNow.open && holdNow.island) warns.push({ tone: 'amber', head: 'Ashore, but on its own', text: `The force can take ${place}, but the enemy can close the ${seaBehind} behind it. Cut off, it can hold out for about ${months} months on what it carries.`, also: `And a force that took ${place} would be on its own: the enemy can close the ${seaBehind} behind it, and cut off it can hold out for about ${months} months on what it carries.` });
    if (holdNow && holdNow.contested) warns.push({ tone: 'amber', head: 'Ashore, on a contested sea', text: `The force can land and the ${seaBehind} behind it is yours today. But the enemy can bring ships enough to fight you for it, and if he wins the troops are cut off.`, also: `And the ${seaBehind} behind the force is contested: yours today, but the enemy can bring ships enough to fight you for it.` });
    const fought = p.defended && p.assess ? p.assess : null;
    const chance = fought ? chanceOn(fought.odds, fought.dice) : 1;
    const hopeless = !!fought && chance < 0.3;
    const fight = !fought ? 'The coast is undefended' : chance >= 0.95 ? 'Victory is all but certain' : chance >= 0.6 ? 'The odds favour you' : chance >= 0.3 ? 'It could go either way' : chance > 0 ? 'A long shot' : p.air ? 'This assault will fail' : 'This landing will fail';
    const fightTone = !fought || chance >= 0.6 ? 'good' : chance >= 0.3 ? '' : 'red';
    const lead = hopeless ? null : warns[0] ?? null;
    const next = hopeless ? warns[0] ?? null : warns[1] ?? null;
    const verdict = lead ? lead.head : fight;
    const tone = lead ? lead.tone : fightTone;
    const under = lead ? `${lead.text}${next ? `\n${next.text}` : ''}` : next ? next.also : '';
    const underTone = lead ? (lead.tone === 'red' || next?.tone === 'red' ? 'red' : 'amber') : next ? next.tone : '';
    if (lead) lines.push(p.air ? 'THE FIGHT ON THE GROUND' : 'THE FIGHT FOR THE BEACH', `\u00B7 ${fight}${fought ? `: odds of ${fought.odds.toFixed(2)} to 1, about ${Math.round(chance * 100)} in 100 to take ${place}` : ''}`, '');
    lines.push(p.air ? 'The force goes in by air.' : p.surprise ? 'War was declared this month: the defenders are caught unready, and no fleet can stop the crossing.'
      // (Version 72: the danger is the share of the force caught at sea; the reckoning says what share of the men that costs.)
      : p.risk > 0.005 ? ((p as { account?: CrossAccount }).account
        ? `${p.risk >= 0.2 ? 'The crossing is dangerous' : 'The crossing carries some risk'}: about ${Math.max(1, Math.round(p.risk * 100))} in 100 of the force would be caught at sea by enemy ships and aircraft.`
        : `The crossing is dangerous: about ${Math.round(p.risk * 100)} in 100 of the men could be lost to enemy ships and aircraft on the way.`) : 'The crossing is safe.');
    if (p.sortie > 0 && !p.surprise && !p.air) lines.push('Enemy warships within reach will put to sea against it.');
    lines.push(p.sky ? 'You hold the sky over the beach.' : 'You do not hold the sky over the beach.');
    lines.push(p.defended ? `The coast is defended: the troops must fight their way ashore${p.assess ? `, at odds of ${p.assess.odds.toFixed(1)} to 1` : ''}.` : 'The coast is undefended: they land unopposed.');
    // (Version 72) Why the crossing is as dangerous as it is, and what would make it safer. (Absent in a surprise landing.)
    // (Set apart under a heading of its own, each line marked, so that the card reads as a list and not as a block.)
    const reckoning = accountLines((p as { account?: CrossAccount }).account);
    if (reckoning.length) lines.push('', 'THE RECKONING', ...reckoning.map((l) => (l.endsWith(':') ? l : `\u00B7 ${l}`)));
    // (Version 73) And whether the sea behind the beach stays open once they are ashore.
    const hold = (p as { hold?: SeaHoldView }).hold;
    const holding = holdLines(hold);
    if (holding.length) lines.push('', ...holding);
    if (hold && hold.open && !p.air) lines.push('', `As things stand the enemy cannot close the ${s.seas[hold.sea]?.name ?? 'sea'} behind it.`);
    // (the long warning in red stays under the card where the headline does not already say it)
    // (the headline and the line under it now say what the long warning in red said)
    // (Version 83, ruling 89(f).) The test of strength, as the card's last note: the reckoning his rivals' staffs make
    // before they sail, for his own force at that beach. The rules' own words; it never stops him.
    return { ok: !p.why, why: p.why ?? '', name: s.regions[to].name, lines, warn: '', verdict, tone, under, underTone, ashore: said ? { head: said.head, line: said.line, tone: said.tone } : null };
  }
  return { ok: !p.why, why: p.why ?? '', name: s.regions[to].name, lines, warn: '', verdict: '', tone: '', under: '', underTone: '', ashore: null };
}

/** Lands the chosen corps on an enemy coast. */
function landOn(ids: number[], to: number) {
  const s = game();
  const cs = corpsById(s, ids).filter((c) => canOrder(s, c));
  const before = new Map(cs.map((c) => [c.id, c.reg] as const));
  const r = orderLand(s, cs, to);
  const moved = cs.filter((c) => !c.dead && c.reg !== before.get(c.id)).map((c) => ({ id: c.id, path: [c.reg] }));
  forgetMarch(s, moved.map((m) => m.id)); // (an order carried out ends a standing march; one refused leaves it standing)
  const bits: string[] = [];
  if (r.ok) {
    bits.push(r.taken ? `The landing at ${s.regions[to].name} has succeeded: the troops are ashore.` : `The landing at ${s.regions[to].name} was thrown back into the sea.`);
    if (r.lostAtSea && r.lostAtSea >= 100) bits.push(`${r.lostAtSea.toLocaleString('en-US')} men were lost on the crossing.`);
    // (The UI director, 18 November 2026: a notice holds three lines at most. The landing's own sentence and what it
    // cost; what the attack did to the defenders' works is the battle report's to say.)
    if (r.report) { const price = priceLine(s, r.report); if (price) bits.push(price); }
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
    // (the line through the rules' own pass for names: "the United States' 51", never "the United States's")
    prize: (() => { const z = prizeOfPlace(s, r.id); return z && prizeLine(s, r.id) ? { kind: z.kind, line: theWords(z.line), items: z.items.map((i) => `${i.label}: ${i.text}`) } : null; })(),
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
    // (1.5.2, playtest #29: the price and the yes or no are the rules' own, the arsenal's offer for this province, so a
    // button shown open is never refused and one the treasury can pay for is never shut: the nation's own price, the
    // technology, the men, and whether this province can form up a corps of the kind at all.)
    raise: raiseHere ? (() => {
      const offers = new Map(arsenalOffers(s, id).map((o) => [o.key, o]));
      return (Object.keys(PRODUCTION.builds) as Kind[]).map((kind) => {
        const spec = PRODUCTION.builds[kind];
        const o = offers.get(`corps:${kind}`);
        const price = o?.cost ?? spec.cost;
        const here = !!o && o.at === id;
        const need = needsOf(s, kind);
        const short = r.fac < need ? `${r.name} has not the ${need === 1 ? '1 factory' : `${need} factories`} it needs` : `${r.name} is too steep for tanks to form up`;
        const why = !up ? 'It is not your month.'
          : !o ? 'It cannot be ordered now.'
            : o.poor && n ? `Needs ${shown(price).toLocaleString('en-US')}; the treasury has ${inHand(n.pp).toLocaleString('en-US')}.`
              : o.why ? o.why
                : !here ? `${short}.${o.at >= 0 ? ` Raise it at ${s.regions[o.at].name}.` : ''}` : '';
        return {
          kind, label: spec.label, divs: spec.divs, cost: shown(price), months: spec.months, blurb: spec.blurb,
          // What the new corps will cost to keep each month once it has arrived (nothing in the scenarios).
          keep: budgeted(s) ? shown(corpsUpkeep(kind, spec.divs, 1)) : 0,
          ok: up && !!o && !o.why && here,
          why,
        };
      });
    })() : [],
    raiseNote: raiseHere ? '' : home ? 'New corps are raised in the home country and sent out by sea.' : '',
    // Fortifications: the next level, if there is one.
    fort: home && cost !== null ? {
      level: level + 1, cost: shown(cost), months: PRODUCTION.fort.months,
      ok: up && !!n && n.pp >= cost,
      why: !up ? 'It is not your month.' : n && n.pp < cost ? `Needs ${shown(cost).toLocaleString('en-US')}; the treasury has ${inHand(n.pp).toLocaleString('en-US')}.` : '',
    } : null,
    ...((book) => ({
      forming: me ? s.nations[me].queue.filter((b) => b.region === r.id).sort((a, b) => a.eta - b.eta).map((b) => orderLine(s, b, book)) : [],
      formingHead: book.find((c) => c.region === r.id && !c.spied)?.summary ?? '',
    }))((() => { try { return ordersBookOf(s); } catch { return []; } })()),
    // What kind of country it is for an army, the rivers on its borders and their bridges, and its railway
    // (all only in the grand campaign, where those rules apply).
    // What can be done to the country itself this month: bridges blown, the railway torn up, engineers set to work.
    // The town as a fortress (Version 54): what it adds to its defenders, and how its siege stands, in plain lines.
    // (The UI director, 18 November 2026: the besieger's name takes its article, "BESIEGED BY THE UNITED STATES"; and `own` says the town is the player's own, the one case in which the head is red.)
    siege: (() => { try { const v = townSiege(s, r.id); return v ? { town: v.town, by: v.by ? theWords(`by ${s.nations[v.by]?.name ?? v.by}`).slice(3) : '', own: !!me && r.ctrl === me, lines: v.lines, inside: v.inside, room: v.room } : null; } catch { return null; } })(),
    // (Version 63) A province cut off from the rest of its country: what its troops live on, and for how long.
    pocket: (() => {
      try {
        const v = pocketAt(s, r.id);
        if (!v) return null;
        const mine = v.nat === s.human, foe = !!s.human && atWar(s, s.human, v.nat);
        const tail = mine ? (v.spent ? ' Break the ring from outside, or attack out of it while the corps can still fight.' : ' Break the ring from outside before the stores are gone.')
          : foe ? (v.spent ? ' An attack now meets hungry troops whose works count for nothing.' : ' Wait, and they starve. Attack, and you meet them at full strength.') : '';
        return {
          title: v.spent ? 'CUT OFF, AND THE STORES ARE SPENT' : `CUT OFF SINCE ${turnToDate(v.since).label.toUpperCase()}  \u00B7  STORES FOR ${v.left} MONTH${v.left === 1 ? '' : 'S'}`,
          text: `${v.text}${tail}`, spent: v.spent, mine,
        };
      } catch { return null; }
    })(),
    // (Version 64) Fieldworks standing empty here, which a corps of the player's side takes over by marching in.
    leftWorks: (() => {
      try {
        const v = fieldworksLeft(s, r.id);
        if (!v) return null;
        const m = Math.round(v.months * 10) / 10;
        return {
          title: `FIELDWORKS STAND EMPTY HERE  \u00B7  ROOM FOR ${v.corps === 1 ? 'ONE CORPS' : `${v.corps} CORPS`}  \u00B7  ${m === 1 ? 'A MONTH' : `${m} MONTHS`} OF WORK`,
          text: 'A corps of yours that marches or rides in takes them over at once and fights from them, as if it had dug them itself. Empty, they lose a month of work every month.',
        };
      } catch { return null; }
    })(),
    // (Version 59) A directive of the war cabinet that names this province: said on its panel, with the months left.
    directive: (() => {
      try {
        const gov = governmentOf(s);
        const d = gov?.directive;
        const pr = (gov as { promise?: { region?: number; town: string; left: number; progress: string } | null } | null)?.promise;
        return [
          d && d.region === r.id ? `The war cabinet's directive: ${d.title}. ${d.left <= 1 ? 'It is due this month.' : `${d.left} months left.`} ${d.progress}` : '',
          pr && pr.region === r.id ? `Your word to the cabinet: ${pr.town} ${pr.left <= 1 ? 'this month' : `within ${pr.left} months`}. ${pr.progress}` : '',
        ].filter(Boolean).join('\n');
      } catch { return ''; }
    })(),
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
      // (Damaged cities) Repair the town, or take back this month's repair.
      const ruin = repairView(s, r.id);
      if (ruin && ruin.holder === me) {
        if (ruin.undo && ruin.damage <= 0) out.push({ key: 'unrepair', label: `Take back the repair of ${ruin.town}`, detail: 'Repaired this month. Taken back, the town is as it was and the money comes back.', ok: up, why: up ? '' : 'It is not your month.' });
        else out.push({ key: 'repair', label: `Repair ${ruin.town} \u00B7 ${moneyWords(ruin.cost)}`, detail: `${ruin.pct}% of the town lies in ruins: it pays ${moneyWords(ruin.lost)} a month less. Paid and done at once; it pays for itself in ${monthsWords(ruin.months)}.`, ok: up && ruin.why === null, why: !up ? 'It is not your month.' : ruin.why ?? '' });
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
      // (Version 60) Call your shot: an enemy town within reach can be promised to the cabinet from its own panel.
      try {
        if (r.ctrl !== me && governmentOf(s)?.mine && whyNotOrderPromise(s) === null) {
          for (const months of [2, 3, 4, 6]) {
            if (whyNotOrderPromise(s, r.id, months) !== null) continue;
            const terms = promiseTermsOf(s, r.id, months);
            if (terms) out.push({ key: `promise:${months}`, label: `Promise ${terms.town} to the cabinet in ${months} months`, detail: terms.text, ok: up, why: up ? '' : 'It is not your month.' });
          }
        }
      } catch { /* no cabinet in this game */ }
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
      // (Damaged cities) Only where the player could know it: his own side's town, or one he has eyes on.
      const ruin = repairView(s, r.id);
      if (ruin && ruin.damage > 0 && (!me || allied(s, r.ctrl, me) || sight(s)[r.id] > 0)) lines.push(`${ruin.town} is ${ruin.pct}% in ruins${ruin.hit ? ', hit this month' : ''}: it pays ${r.ctrl === me ? 'you' : 'its holder'} that much less until it is repaired.`);
      for (const n of riverNotes(s, r.id)) {
        lines.push(`The ${n.river} lies toward ${s.regions[n.far].name}: ${n.down ? 'the bridges are down' : 'the bridges stand'}.`);
      }
      const rail = railNote(s, r.id);
      if (rail) {
        const hasty = (rail as { hasty?: number }).hasty ?? 0;
        lines.push(rail.line ? (rail.cut ? (hasty > 0 ? `The railway here is cut: torn up in haste, the railwaymen will have it running in about ${hasty === 1 ? 'a month' : `${hasty} months`}.` : 'The railway here is cut: it stays cut until engineers relay it.') : 'A railway runs through here.')
          : rail.road > 0 ? `No railway: ${rail.road} province${rail.road === 1 ? '' : 's'} of road from the railhead.` : 'No railway.');
        // (Version 83.1, ruling 125: the guns that guard the line, and the order the railway's row offers, in the rules' own words.)
        const guard = rail.line ? guardSays(s, r.id) : null;
        if (guard) {
          lines.push(guard.line);
          if (guard.order) lines.push(`${guard.order.head}: ${guard.order.text} \u00B7 ${inHand(guard.order.cost).toLocaleString('en-US')} \u00B7 ${guard.order.months} month${guard.order.months === 1 ? '' : 's'}${guard.order.why ? `. ${guard.order.why}` : ''}`);
        }
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
    const held = p.holder !== r.ctrl ? ` Held by ${theName(s, p.holder)}.` : '';
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
/** The appointment or decree the player has been asked to confirm, and the month. */
let cabinetAsked = '';
/** The page of the cabinet room that is open (Version 62): its business, the ministers, the decrees or the minutes. Back to its business as each month opens. */
let cabinetTab: 'now' | 'table' | 'decrees' | 'minutes' = 'now';
let cabinetTabTurn = -1;

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
  } else if (what === 'promise') {
    // (Version 60) The player's word to the cabinet, given from the town's own panel: asked twice.
    const months = Number(a);
    const why = whyNotOrderPromise(s, region, months);
    const terms = why ? null : promiseTermsOf(s, region, months);
    const asking = `promise:${region}:${months}:${s.turn}`;
    if (!why && terms && wreckAsked !== asking) {
      wreckAsked = asking;
      return { ok: false, note: `Promise ${terms.town} to the cabinet within ${months} months? ${terms.bandName}: kept, ${terms.reward}; broken, its trust falls ${terms.price}. Press it again to give your word.`, flies, view: view() };
    }
    wreckAsked = '';
    ok = !why && orderPromise(s, region, months);
    note = ok ? `Your word is given: ${terms?.town ?? 'the town'} by the end of ${terms ? turnToDate(terms.due).label : 'the time named'}.` : why ?? note;
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
  } else if (what === 'repair' || what === 'unrepair') {
    // (Damaged cities) Paid and done at once; taken back in its month with the money.
    const v = repairView(s, region);
    try { ok = what === 'repair' ? !!repairCalls.order?.(s, region) : !!repairCalls.undo?.(s, region); } catch { ok = false; }
    note = ok ? (what === 'repair' ? `${v?.town ?? 'The town'} is repaired for ${moneyWords(v?.cost ?? 0)}: it pays in full again from this month.` : `The repair of ${v?.town ?? 'the town'} is taken back, and the money with it.`) : v?.why ?? note;
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
      // (1.5.1; the playtester, 25 November) A fleet sent at the enemy this month stays where the battle left it: its
      // seas and its harbour are shown, refused, with the reason, and not offered only to be turned down.
      const held = heldWhy(s, f);
      // (1.5.2) Cut off at sea beyond a shut strait, it can go to no sea in its reach: only to a harbour on its side.
      const cut = cutOff(s, f), haven = cut ? harbourFor(s, f) : null;
      const reach = cut ? [] : fleetReach(s, f).slice().sort((a, b) => (a === f.zone ? -1 : b === f.zone ? 1 : 0));
      for (const z of reach) {
        const here = f.zone === z && f.stance === 'patrol';
        options.push({ key: `fleet:${id}:${z}:patrol`, label: `Patrol the ${s.seas[z].name}`, detail: 'Seeks out enemy warships there and guards the convoys.', ok: up && !here && !held, why: here ? 'It is doing so now.' : notNow || held });
      }
      for (const z of reach) {
        const here = f.zone === z && f.stance === 'raid';
        options.push({ key: `fleet:${id}:${z}:raid`, label: `Raid shipping in the ${s.seas[z].name}`, detail: 'Goes for merchant ships and avoids a fight with the enemy fleet.', ok: up && !here && !held, why: here ? 'It is doing so now.' : notNow || held });
      }
      options.push({ key: `fleet:${id}:${f.home}:port`, label: `Return to harbour at ${haven?.name ?? f.port}`,
        detail: cut && haven ? `It is cut off beyond a shut strait: it makes for ${haven.name}, the nearest friendly harbour it can reach, which becomes its base.` : 'Safe from the enemy fleet, and takes no part at sea.',
        ok: up && f.stance !== 'port' && !held && (!cut || !!haven),
        why: f.stance === 'port' ? 'It is in harbour now.' : notNow || held || (cut && !haven ? 'It is cut off beyond a shut strait, and no friendly harbour on its side can take it.' : '') });
      if (staffRunsDesk(s, 'navy') && fleetKeptByHand(s, id)) options.push({ key: `fleet:${id}:staff`, label: 'Hand it back to your admirals', detail: HAND_BACK_TIP, ok: up, why: notNow });
    }
    const who = mine ? whoSails(s, id) : '';
    // (1.5.2) A cut-off fleet's own card says so for as long as it is (the DD, 25 Nov 04:05Z).
    const cut = mine ? cutOffLine(s, f) : '';
    return { name: f.name, nat: f.nat, mine, lines: [parts.join(', ') || 'No ships left', doing, ...(cut ? [cut] : []), ...(who ? [who] : [])], options };
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
    // (85.1: the player's list is moveTargets, as in the browser: the far islands of the wider world by sea included. airBases is the staffs'.)
    const bases = moveTargets(s, f).sort((a, b) => Math.hypot(a.x - base.x, a.y - base.y) - Math.hypot(b.x - base.x, b.y - base.y)).slice(0, 5);
    for (const r of bases) {
      options.push({ key: `airbase:${id}:${r.id}`, label: `Move to airfields in ${r.name}`, detail: overOcean(s, f, r) ? SHIP_AIR_TIP : 'Settling in costs it some readiness for a month.', ok: up, why: notNow });
    }
  }
  return {
    name: f.name, nat: f.nat, mine,
    lines: [`${Math.round(f.ftr)} fighters, ${Math.round(f.bmr)} bombers`, `Based in ${base?.name ?? 'the field'}.`, doing], options,
  };
}

/**
 * (1.5.1; the playtester, 25 November 2026) A fleet sent at the enemy this month stays where the battle left it until
 * next month (turn.ts refuses its sailing, harbour, new base and escort orders). Its panel and its page show those
 * orders dimmed with the reason, and an order sent from the map anyway is answered with it, in the rules' own words
 * (fleetOptions().held), not "That order cannot be given now." The map's ring is left as it was: its tip over a sea
 * already gives the reason (FleetHand.Held), and a right-click there comes here for the answer.
 */
function heldWhy(s: Game, f: { id: number; nat: string; attacked?: number }): string {
  return f.nat === s.human && f.attacked === s.turn ? fleetOptions(s, f.id)?.held ?? '' : '';
}
function heldWhyId(s: Game, id: number): string {
  const f = s.fleets.find((x) => x.id === id);
  return f ? heldWhy(s, f) : '';
}
/** (1.5.2) Why one of the player's fleets was refused an order: held where this month's battle left it, or cut off beyond a shut strait (its card's line). Empty for neither. */
function fleetWhyId(s: Game, id: number): string {
  const f = s.fleets.find((x) => x.id === id);
  return !f || f.nat !== s.human ? '' : heldWhy(s, f) || cutOffLine(s, f);
}

/**
 * (1.5.1; Brian, 24 November 2026 18:32Z: "when i move my navy they go right back to where they were before the next
 * turn") Who sails one of the player's fleets while his admirals run the navy: a line for its panel and its page. Empty
 * where he runs the navy himself, when every fleet is his anyway.
 */
function whoSails(s: Game, id: number): string {
  if (!staffRunsDesk(s, 'navy')) return '';
  return fleetKeptByHand(s, id)
    ? 'Under your own orders: your admirals leave it to you, wherever it is.'
    : 'Your admirals sail it each month. Give it an order yourself and it stays where you send it.';
}
const HAND_BACK_TIP = 'Your admirals sail it again from next month, with the rest of the navy.';

/** Gives a fleet or an air fleet one of the orders its panel offered. */
function forceOrder(key: string) {
  const s = game();
  const [what, id, a, b] = key.split(':');
  let ok = false;
  // (The panel's 'fleet:<id>:staff' hands a fleet back to the admirals: the map sends every 'fleet:' key here.)
  if (what === 'fleet' && a === 'staff') ok = handBackFleet(s, Number(id));
  else if (what === 'fleet') ok = orderFleet(s, Number(id), Number(a), b as Stance);
  else if (what === 'fleetbase') ok = orderRebase(s, Number(id), Number(a));
  else if (what === 'air') ok = orderAir(s, Number(id), a as AirMission, Number(b));
  else if (what === 'airbase') ok = orderAirBase(s, Number(id), Number(a));
  return { ok, note: ok ? 'The order is given.' : (((what === 'fleet' && a !== 'staff') || what === 'fleetbase') && fleetWhyId(s, Number(id))) || 'That order cannot be given now.', view: view() };
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
    // (rules version 74: on the wider chart, its rivers, crossings and lines follow Europe's, which keep their numbers)
    // (`into`: what the wider chart's file says each river runs into, another river or a sea; the game joins a
    // tributary's ribbon only to the river it names)
    rivers: riversOf(s).map((r) => ({ name: r.name, pts: r.pts, into: (r as { into?: string }).into ?? '' })),
    crossings: crossingsOf(s).map((c) => [c.a, c.b, c.river, c.x, c.y]),
    rails: railLinks(s).map((l) => [l.a, l.b, l.x, l.y]),
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
      key: o.key, group: o.group, label: o.label, blurb: o.blurb, size: o.size, months: o.months, kind: (o as { kind?: string }).kind ?? '',
      cost: shown(o.cost), upkeep: shown(o.upkeep), at: o.at, where: s.regions[o.at]?.name ?? '',
      // (Version 59: what it takes from the reserve of men, said with its note)
      note: [o.note, (o as { men?: number }).men ? `Takes ${Math.round((o as { men?: number }).men!).toLocaleString('en-US')} men from the reserve.` : ''].filter(Boolean).join(' '),
      // (Versions 65 and 67) where money is the only thing in the way, how far short the treasury is and what it holds
      why: up ? (o.poor && shown(o.cost) > inHand(s.nations[me].pp) ? `${(shown(o.cost) - inHand(s.nations[me].pp)).toLocaleString('en')} short: the treasury holds ${inHand(s.nations[me].pp).toLocaleString('en')}.` : o.why) : 'It is not your month.', poor: o.poor,
    })),
    orders: ((book) => s.nations[me].queue.map((b) => ({ ...orderLine(s, b, book), cost: shown(b.cost) })))(ordersBookOf(s)),
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
  const h0 = s.headlines.length;
  const ok = humanUp(s) && orderDeclare(s, on);
  // (Rules 84.2.5; the UI director, 17 November: one voice after a press.) What the declaration brought at once, as the
  // rules head it ("Britain stands by Canada", "Britain stays out of Canada's war"): the paper answers the press with it.
  const note = s.headlines.slice(h0).filter((h) => !!(h as { head?: string }).head).map((h) => `${(h as { head?: string }).head}. ${h.text}`).join(' ');
  return { ok, note, view: view() };
}

/** A nation's name as it reads inside a sentence: "the United Kingdom", "Germany". */
const theName = (s: Game, id: string): string => { const nm = s.nations[id]?.name ?? id; return /^(United|Soviet|Netherlands)/.test(nm) ? `the ${nm}` : nm; };
const upFirst = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
const listOf = (names: string[]): string => (names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`);
/** (Rules 84.2.6.) A dominion's mother country, alive, in the open campaign outside World Domination: first among its friends. */
const motherOf = (s: Game, nat: string): string | null => { const p = isOpen(s) && !s.flags.dom ? patronOf(nat) : null; return p && s.nations[p]?.alive ? p : null; };

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
  // (whole divisions, as the browser's card counts them: "Canada and its 2 divisions", never "1.6 divisions")
  const divsOf = (n: number): string => `${Math.round(n).toLocaleString('en-US')} division${Math.round(n) === 1 ? '' : 's'}`;
  const divs = divsOf(fieldArmy(s, on));
  const lines: string[] = [];
  lines.push(f.joins ? `${The} joins ${blocOf(f.joins)} the moment it is attacked, and its ${divs} ${Math.round(fieldArmy(s, on)) === 1 ? 'fights' : 'fight'} for them.` : `${The} and its ${divs} are at war with you from today.`);
  if (f.league.length) lines.push(`The League stands by it. ${The} belongs to ${LEAGUE.name}, and war on one member is war on all: ${listOf(f.league.map((x) => theName(s, x)))} ${f.league.length === 1 ? 'goes' : 'go'} to war with you the same day, with ${divsOf(f.league.reduce((sum, id) => sum + fieldArmy(s, id), 0))} between them.`);
  // (Rules 84.2.5; ruling 211(b).) A mother country that may stand by it: the odds in plain words, never a promise.
  const patron = (f as { patronSays?: [string, string] | null }).patronSays;
  if (patron) lines.push(`${patron[0]} ${patron[1]}`);
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
  // (Rules 84.2.6.) A dominion's mother country stands first, by name, with the year of the tie: "Britain (the mother
  // country: Canada has been a dominion since 1867)". Display only: the tie moves no opinion.
  const mother = motherOf(s, on);
  const ties = new Map<string, string[]>();
  for (const x of f.friends) if (x.nat !== mother) ties.set(x.why, [...(ties.get(x.why) ?? []), theName(s, x.nat)]);
  const said = [
    ...(mother ? [`${newsName(s.nations[mother])} (${motherLine(mother, on, n.name).replace(/^The /, 'the ').replace(/\.$/, '')})`] : []),
    ...[...ties].map(([why, who]) => `${listOf(who)} (${why})`),
  ];
  const friendsLine = said.length ? `Its friends: ${said.join('; ')}.` : `${The} has no close friends among the nations.`;
  return { ok: true, name: theName(s, on), lead, friends: friendsLine, lines };
}

// ---- Rooms drawn by one general panel: the bridge writes the words and the buttons, the game draws them ----------

interface PageButton { label: string; act: string; ok: boolean; tip: string; danger?: boolean }
interface PageRow { nat: string; title: string; lines: string[]; buttons: PageButton[]; meter?: number; picture?: string; alarm?: string }
interface Page { id: string; title: string; headline: string; note: string; top: PageButton[]; sections: { heading: string; text: string; rows: PageRow[] }[] }

/** A question about World Domination waiting for its second and third yes. */
let domAsked = 0;

function pagesOf(s: Game): { id: string; label: string }[] {
  const out: { id: string; label: string }[] = [];
  if (!s.human) return out;
  if (lastMonth) out.push({ id: 'reports', label: "THE MONTH'S REPORT" });
  if (s.stories.awards.some((a) => a.nat === s.human && !a.old)) out.push({ id: 'heroes', label: 'HEROES AT HOME' });
  if (commandBrief(s)) out.push({ id: 'staff', label: 'YOUR STAFF' });
  if (secretService(s)) out.push({ id: 'spies', label: 'THE SECRET SERVICE' });
  // The government and its war cabinet (Version 57, the grand campaign).
  try { if (governmentOf(s)?.mine) out.push({ id: 'cabinet', label: 'THE CABINET' }); } catch { /* no government in this game */ }
  // Towns to build up and trade agreements (the grand campaign).
  if (pictureTrade(s)) out.push({ id: 'trade', label: 'TRADE AGREEMENTS' });
  if (s.nations[s.human] && townBuildings(s, s.nations[s.human].capital)) out.push({ id: 'towns', label: 'TOWNS TO BUILD UP' });
  if (damagedOfMine(s).length) out.push({ id: 'repairs', label: 'DAMAGED TOWNS' });
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
      } else if ((gov as { head?: { name: string } | null }).head) {
        out.push({ k: 'page:cabinet', t: 'THE CABINET WANTS A HEAD', tip: `The cabinet asks you to dismiss ${(gov as { head?: { name: string } | null }).head!.name}. Open THE CABINET to dismiss him or stand by him.` });
      } else if ((gov as { promise?: { town: string; left: number } | null }).promise && (gov as { promise?: { town: string; left: number } | null }).promise!.left <= 2) {
        const pr = (gov as { promise?: { town: string; left: number } | null }).promise!;
        out.push({ k: 'page:cabinet', t: pr.left <= 1 ? 'YOUR PROMISE IS DUE THIS MONTH' : 'YOUR PROMISE IS DUE NEXT MONTH', tip: `You promised the cabinet ${pr.town}. Open THE CABINET.` });
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
    // (Rules 84.2.6.) A siege whose guns are left alone has its own row, in the rules' words: it is not said twice.
    const lone = new Set(gunsAloneOf(s).map((a) => a.region));
    const mine = all.filter((v) => v.holder === me.id), theirs = all.filter((v) => v.by === me.id && !lone.has(v.region));
    if (mine.length) out.push({ k: `goto:${mine[0].region}`, t: mine.length === 1 ? `${mine[0].town.toUpperCase()} IS BESIEGED` : `${mine.length} OF YOUR TOWNS ARE BESIEGED`, tip: `${mine.map((v) => v.town).slice(0, 4).join(', ')}: ${mine[0].lines[mine[0].lines.length - 1]} Attack the army round it to raise the siege, or let the garrison sally out.` });
    // (Brian, 18 November 2026: he had beaten an army into its town and found no way to attack it again. A press takes the
    // besieging army in hand at the town, where its own card carries STORM <TOWN>.)
    if (theirs.length) out.push({ k: `siege:${theirs[0].region}`, t: theirs.length === 1 ? `YOU BESIEGE ${theirs[0].town.toUpperCase()}` : `YOU BESIEGE ${theirs.length} TOWNS`, tip: `${theirs.map((v) => v.town).slice(0, 4).join(', ')}. Starve it out, or storm it: a press takes your army there in hand, and STORM on its card attacks the town.` });
  } catch { /* a campaign from history has no sieges */ }
  // Trade: a route cut, or a trading house idle with a country ready to sign.
  const book = pictureTrade(s);
  if (book) {
    const cut = book.deals.filter((d) => d.state === 'cut').length;
    if (cut) out.push({ k: 'page:trade', t: cut === 1 ? 'A TRADE ROUTE IS CUT' : `${cut} TRADE ROUTES ARE CUT`, tip: 'An agreement pays nothing while its route is cut. Open TRADE AGREEMENTS to see which, and why.' });
    else if (book.houses > book.used && book.pc >= book.cost && book.anyReady()) {
      out.push({ k: 'page:trade', t: 'A TRADING HOUSE STANDS IDLE', tip: 'A country is ready to sign a trade agreement, and you have a trading house free for it. It would pay into the treasury every month.' });
    }
  }
  // (Version 86, the design director's b448; the UI director, 18 November 2026.) His crowded airfields, in one row:
  // a single field names its place ("OREL'S AIRFIELD IS CROWDED"), with "AN AIRFIELD IS CROWDED" only where the name
  // will not fit; several are counted. The rules' own sentence is its hover, and a press opens the worst field.
  try {
    const air = everyAirRule as Record<string, unknown>;
    const fieldsOf = air['crowdedFields'] as ((g: Game, nat: string | null) => { region: number }[]) | undefined;
    const rowOf = air['crowdedRow'] as ((g: Game, cs: { region: number }[]) => string) | undefined;
    const crowded = fieldsOf ? fieldsOf(s, me.id) : [];
    if (crowded.length && rowOf) {
      const named = `${(s.regions[crowded[0].region]?.name ?? '').toUpperCase()}'S AIRFIELD IS CROWDED`;
      out.push({
        k: `field:${crowded[0].region}`,
        t: crowded.length > 1 ? `${crowded.length} AIRFIELDS ARE CROWDED` : named.length <= 34 ? named : 'AN AIRFIELD IS CROWDED',
        tip: ((row: string) => (/[.!?]$/.test(row) ? row : `${row}.`))(rowOf(s, crowded)),
      });
    }
  } catch { /* rules before Version 86 */ }
  // (Damaged cities) Towns of his in ruins that the treasury can repair now.
  const ruins = damagedOfMine(s).filter((t) => t.why === null);
  if (ruins.length) {
    const one = `${ruins[0].town.toUpperCase()} IS IN RUINS`;
    out.push({ k: 'page:repairs', t: ruins.length > 1 ? `${ruins.length} TOWNS IN RUINS` : one.length <= 34 ? one : 'A TOWN IS IN RUINS', tip: `${listNames(ruins.map((t) => `${t.town} (${t.pct}%)`))}: ${ruins.length > 1 ? 'they pay' : 'it pays'} that much less until repaired. Open DAMAGED TOWNS to repair them.` });
  }
  // Nothing on order at all, with money in the treasury for something the arsenal offers.
  if (me.queue.length === 0 && arsenalOffers(s, -1).some((o) => !o.why)) {
    out.push({
      k: 'arsenal', t: 'NOTHING IS ON ORDER',
      tip: `Nothing is being raised or built, and there is ${inHand(me.pp).toLocaleString('en-US')} in the treasury. Open the ARSENAL to order corps, aircraft, ships or works.`,
    });
  }
  // (Version 66) What belongs to a desk the staff runs is the staff's to deal with: it does not ask for the player's attention.
  const theirs: Record<string, DeskId> = { research: 'research', arsenal: 'build', 'page:trade': 'build', 'page:spies': 'spies', 'page:repairs': 'build' };
  // (a crowded field is the air desk's, as in the browser: its row is not shown while the staff runs the air war)
  return out.filter((t) => { const desk = t.k.startsWith('field:') ? ('air' as DeskId) : theirs[t.k]; try { return !(desk && staffRunsDesk(s, desk)); } catch { return true; } });
}

function alertsOf(s: Game): string[] {
  const out: string[] = [];
  if (!s.human) return out;
  const act = lastAct(s);
  if (act?.winner) out.push(`${upFirst(theName(s, act.winner))} has won the world.`);
  else if (act?.open && !act.peace) out.push('The war between the alliances is decided. Open DIPLOMACY and choose THE LAST ACT.');
  // (Version 59) The reserve of men running low, and a cabinet that has begun to doubt the player.
  try {
    const m = manpowerOf(s);
    if (m && (m.word === 'Short' || m.word === 'Spent')) out.push(m.word === 'Spent' ? 'The reserve of men is spent: nothing new can be raised until more come of age.' : `The reserve of men is short. ${m.line}`);
    const gov = governmentOf(s);
    if (gov?.mine && gov.doubts) out.push(`The cabinet doubts you.${gov.withheld ? ` ${gov.withheld}` : ''} Open DIPLOMACY, THE CABINET.`);
    const talk = gov as (typeof gov & { head?: { name: string; left: number } | null; promise?: { town: string; left: number } | null }) | null;
    if (talk?.mine && talk.head) out.push(`The cabinet wants ${talk.head.name} dismissed, and an answer ${talk.head.left <= 1 ? 'this month' : `within ${talk.head.left} months`}. Open DIPLOMACY, THE CABINET.`);
    if (talk?.mine && talk.promise && talk.promise.left <= 1) out.push(`Your promise of ${talk.promise.town} falls due this month.`);
  } catch { /* neither rule in this game */ }
  return out;
}

const pct = (x: number): string => `${Math.round(x * 100)}%`;

function spiesPage(s: Game, region: number): Page | null {
  const room = secretService(s);
  if (!room) return null;
  const up = humanUp(s) && !onEve(s);
  const here = s.regions[region];
  const rows: PageRow[] = room.agents.map((a) => {
    const whySend = here ? whyNotSendAgent(s, a.id, region) : 'Pick the agent up on the map and right-click where he should go; or click a province on the map and open the room again.';
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
      { heading: 'YOUR AGENTS', text: rows.length ? (here ? `Your agents stand on the map: click one to pick him up, then right-click where he should go. The province selected on the map is ${here.name}: an agent can also be sent there from here.` : 'Your agents stand on the map: click one to pick him up, then right-click where he should go, or drag him there.') : 'The service has no agents yet. Recruit one above: he stands on the map at home, and can be picked up there.', rows },
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
  // (Version 71) Brian's rule for winning the world: the last great power standing takes it, once it holds a set share
  // of the map's worth; the small countries left then lay down their arms. The board says which great powers still
  // stand, the share it takes, and how many laid down their arms. (Asked for by name: older rules have none of it.)
  const world = board as typeof board & { great?: string[]; need?: number; laid?: number };
  const need = world.need ?? 0, laid = world.laid ?? 0;
  const mine = board.rows.find((r) => r.you);
  const rivals = (world.great ?? []).filter((id) => id !== s.human);
  const needWords = Math.abs(need - 0.75) < 1e-9 ? 'three quarters' : Math.abs(need - 2 / 3) < 1e-3 ? 'two thirds' : Math.abs(need - 0.5) < 1e-9 ? 'half' : `${Math.round(need * 100)}%`;
  const laidWords = laid === 1 ? 'the last country still standing has laid down its arms' : `the last ${laid} countries still standing have laid down their arms`;
  if (act.winner) {
    const name = upFirst(theName(s, act.winner)), top1 = board.rows[0];
    note = need > 0
      ? (act.winner === s.human
        ? `No great power is left to fight you. You hold ${pct(top1?.share ?? 1)} of the world${laid ? `, and ${laidWords}` : ''}. The world is won.`
        : `${name} is the last great power standing and holds ${pct(top1?.share ?? 1)} of the world${laid ? `, and ${laidWords}` : ''}. ${name} stands alone.`)
      : `${name} is the last nation standing. The world is won.`;
  } else if (act.on) note = need > 0
    ? `The last act began in ${turnToDate(act.since).label}. Every alliance is dissolved and every nation still standing is at war with every other. ${act.left} are left. The world goes to the last great power standing, once it holds ${needWords} of it: the countries still standing then lay down their arms.`
    : `The last act began in ${turnToDate(act.since).label}. Every alliance is dissolved and every nation still standing is at war with every other. ${act.left} are left. The last one standing wins the world.`;
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
      // (Version 71) The two tests, as they stand for the player: the great powers still against him, and his share of the world.
      ...(act.on && !act.winner && mine && need > 0 ? [{
        heading: 'THE TWO TESTS FOR WINNING THE WORLD', text: '',
        rows: [
          { nat: '', title: rivals.length ? `${rivals.length === 1 ? 'One great power' : `${rivals.length} great powers`} still to beat` : 'No great power stands against you',
            lines: [rivals.length ? `${upFirst(listOf(rivals.map((id) => theName(s, id))))}.` : 'The first test is met.'], buttons: [] },
          { nat: '', title: `${pct(mine.share)} of the world is yours`, lines: [`It takes ${Math.round(need * 100)}%.${mine.share >= need ? ' The second test is met.' : ''}`], buttons: [],
            meter: Math.min(1, mine.share / need) },
        ],
      }] : []),
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
    // (the group's own aircraft, large, beside its orders: the fighter, or the bomber where it is mostly bombers)
    return { nat: f.nat, title: f.name, lines, buttons, picture: `${f.bmr > f.ftr ? 'air:bmr' : 'air:ftr'}:${f.nat}` };
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
  const rows: PageRow[] = moveTargets(s, f).sort((a, b) => Math.hypot(a.x - base.x, a.y - base.y) - Math.hypot(b.x - base.x, b.y - base.y)).slice(0, 14).map((r) => ({
    nat: r.ctrl, title: r.name, lines: [`${workLevel(s, 'field', r.id) > 0 ? `Airfields of level ${workLevel(s, 'field', r.id)}` : 'Rough landing grounds'}.${s.airFleets.some((x) => x.base === r.id && x.id !== f.id && x.ftr + x.bmr > 0) ? ' Other groups are based there already.' : ''}`],
    buttons: [{ label: 'MOVE HERE', act: `airbase:${f.id}:${r.id}`, ok: up, tip: overOcean(s, f, r) ? SHIP_AIR_TIP : 'Settling in costs it some readiness for a month.' }],
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

/** The month's card as it is shown now: a desk the player has since taken back no longer offers to be taken. */
function monthShown(s: Game, card: MonthCard) {
  let run: string[] = [];
  try { run = (commandOf(s)?.desks ?? []).filter((d) => d.staff).map((d) => String(d.id)); } catch { run = []; }
  return { ...card, staff: card.staff.map((x) => ({ ...x, ok: run.includes(x.desk) })) };
}

/** A battle's report set out to be read: who fought, what each lost, and what the map fed into the fight. */
function battleReportPage(s: Game, id: number): Page | null {
  const r = s.reports.find((x) => x.id === id);
  if (!r) return null;
  const nameOf = (nat: string): string => upFirst(theName(s, nat));
  const side = (units: BattleReport['attackers']): PageRow[] => units.map((u) => ({
    // (Rules 84.3.) Every corps on the report carries its country's word: "German XIX Corps".
    nat: u.nat, title: `${corpsNamed(s, u.nat, u.name)}${u.guard ? '' : `  \u00B7  ${u.divs} division${u.divs === 1 ? '' : 's'}`}`,
    lines: [`${cardNum(u.men)} men, ${cardNum(u.lost)} lost${u.fate ? `  \u00B7  ${u.fate}` : ''}`], buttons: [],
  }));
  return {
    id: `battle:${id}`, title: `THE BATTLE REPORT  \u00B7  ${r.regionName.toUpperCase()}`,
    headline: r.yielded ? 'The defenders gave ground without a fight' : r.taken ? `${nameOf(r.attNat)} takes ${r.regionName}` : r.siege ? `${r.regionName} is besieged` : `${nameOf(r.defNat)} holds ${r.regionName}`,
    // (Version 84: and who paid, to what, as whichever side reads it)
    note: `${turnToDate(r.turn).label}, week ${r.week}. Odds ${r.odds.toFixed(1)} to 1. The attack lost ${cardNum(r.attLost)} men and the defence ${cardNum(r.defLost)}${r.prisoners ? `, with ${cardNum(r.prisoners)} taken prisoner` : ''}.${r.paid && !r.yielded ? ` ${s.human && !(r.attNat === s.human || r.attackers.some((u) => u.nat === s.human)) && r.defenders.some((u) => u.nat === s.human) ? r.paid.def : r.paid.att}` : ''}`,
    top: [{ label: "BACK TO THE MONTH'S REPORT", act: 'open:reports', ok: true, tip: '' }, { label: `GO TO ${r.regionName.toUpperCase()}`, act: `goto:region:${r.region}`, ok: true, tip: 'Looks at the place on the map.' }],
    sections: [
      { heading: 'THE ATTACK', text: '', rows: side(r.attackers) },
      { heading: 'THE DEFENCE', text: '', rows: side(r.defenders) },
      { heading: 'WHAT THE MAP FED INTO THE FIGHT', text: r.lines.map((l) => `${l.label}: ${l.value}${l.effect ? `  (${l.effect})` : ''}`).join('\n'), rows: [] },
    ].filter((x) => x.rows.length || x.text),
  };
}

/** The month's report: what every nation did while the player waited, each with a way to go and look. */
function reportsPage(s: Game): Page | null {
  if (!lastMonth) return null;
  const row = (r: { nat: string; note: string; region: number }): PageRow => ({
    nat: r.nat, title: s.nations[r.nat]?.name ?? r.nat, lines: [r.note],
    buttons: r.region >= 0 ? [{ label: `GO TO ${s.regions[r.region].name.toUpperCase()}`, act: `goto:region:${r.region}`, ok: true, tip: 'Looks at the place on the map.' }] : [],
  });
  const mine = lastMonth.rows.filter((r) => r.mine).map(row), rest = lastMonth.rows.filter((r) => !r.mine).map(row);
  // What the staff did at the desks it runs, each with the way to take the desk back (Version 66: a staff's
  // headline is never read without the button that undoes the arrangement beside it).
  let staffed: PageRow[] = [];
  try {
    const v = commandOf(s);
    if (v && s.human && s.nations[s.human]?.alive) {
      staffed = v.desks.filter((d) => d.staff && d.news).map((d) => ({
        nat: '', title: `${d.name}  \u00B7  your staff runs it`, lines: [d.news as string],
        buttons: [{ label: 'TAKE IT BACK', act: `desk:${d.id}:own`, ok: true, tip: `${deskDef(d.id)?.hand ?? ''} What your staff ordered this month stands.` }],
      }));
    }
  } catch { staffed = []; }
  return {
    id: 'reports', title: `THE MONTH'S REPORT  \u00B7  ${lastMonth.date.toUpperCase()}`,
    headline: lastMonth.rows.length ? `${mine.length ? `${mine.length} report${mine.length === 1 ? '' : 's'} from your own fronts` : 'Nothing touched your own fronts'}, ${rest.length} from elsewhere` : 'A quiet month',
    note: 'What every nation did while your month was closed, as it was shown on the table. Your own fronts come first.',
    top: [],
    sections: [
      { heading: 'YOUR FRONTS', text: mine.length ? '' : 'No enemy moved against you, and none of your ground changed hands.', rows: mine },
      { heading: 'WHAT YOUR STAFF DID', text: '', rows: staffed },
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
      // (The 1.5.1 patch, fleetattack.ts) The enemy fleets it can be dropped on to attack now, and why it stays put this
      // month once it has attacked. Empty while the switch is off.
      fleetAttack: fleetAttackOn(s), attack: o?.attack ?? [], held: o?.held ?? '',
      // (1.5.2) Cut off beyond a shut strait: its card's line, and the harbour on its side it would make for (empty with none).
      ...((): { cut: string; haven: string } => {
        const f = s.fleets.find((x) => x.id === id), cut = f ? cutOffLine(s, f) : '';
        return { cut, haven: f && cut ? harbourFor(s, f)?.name ?? '' : '' };
      })(),
      wings: wingsOfFleet(s, id).map((w) => ({ id: w.id, label: w.label, aircraft: Math.round(w.aircraft), doing: w.doing, flew: w.flew })),
    };
  } catch {
    return null;
  }
}
const cap1 = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);

/** A fleet's orders, all in one room: what it is, the seas in its reach and what it would meet, lanes to escort, coasts to shell, splitting and joining. */
/** (1.5.2) The cut-off line at the head of the fleet page's note, with a space after it; empty when it is not cut off. */
function cutOffNote(s: Game, id: number): string {
  const f = s.fleets.find((x) => x.id === id), line = f ? cutOffLine(s, f) : '';
  return line ? `${line} ` : '';
}

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
      buttons.push({ label: c.count === 1 ? 'SPLIT IT OFF' : 'SPLIT THEM ALL OFF', act: `fleetsplit:${id}:${c.cls}:${c.count}`, ok: up && !whyAll, tip: whyAll ?? 'They leave as a fleet of their own, in the same sea or harbour, which can be given its own orders.' });
      if (c.count >= 2) {
        const half = Math.floor(c.count / 2);
        const whyHalf = whyNotSplit(s, id, { [c.cls]: half } as Record<string, number>);
        buttons.push({ label: `SPLIT OFF ${half}`, act: `fleetsplit:${id}:${c.cls}:${half}`, ok: up && !whyHalf, tip: whyHalf ?? 'Half of them leave as a fleet of their own.' });
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
        // (1.5.2, playtest #37: a button shut because it is the fleet's order now says so, as the side panel does.)
        { label: 'PATROL', act: `fleet:${id}:${z.zone}:patrol`, ok: up && !at('patrol'), tip: `${at('patrol') ? 'It is doing so now. ' : ''}Seeks out the enemy's warships there and guards the convoys that cross it.` },
        { label: 'RAID SHIPPING', act: `fleet:${id}:${z.zone}:raid`, ok: up && !at('raid'), tip: `${at('raid') ? 'It is doing so now. ' : ''}Goes for merchant ships and keeps clear of a fight with the enemy fleet.` },
        { label: 'HUNT SUBMARINES', act: `fleet:${id}:${z.zone}:hunt`, ok: up && !at('hunt') && card.hunters > 0, tip: card.hunters > 0 ? `${at('hunt') ? 'It is doing so now. ' : ''}Its destroyers sweep the sea for submarines: the more destroyers, the more it sinks.` : 'It has no destroyers to hunt with.' },
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
  // (The 1.5.1 patch, fleetattack.ts) The enemy fleets in its reach it can be sent straight at, each with its odds.
  const targets: PageRow[] = (o?.attack ?? []).map((t) => {
    const p = previewFleetAttack(s, id, t);
    const them = p?.theirs.fleets.find((x) => x.id === t);
    const where = p ? (p.where === 'harbour' ? `In harbour at ${p.port}.` : `At sea, in the ${p.zoneName}.`) : '';
    return {
      nat: them?.nat ?? '', title: them?.name ?? 'An enemy fleet',
      lines: [where, p?.odds ? `${p.verdict}: ${p.odds.mid >= 9.95 ? 'more than 10' : p.odds.mid.toFixed(1)} to 1.` : 'Its strength is not known.'].filter(Boolean),
      buttons: [{ label: 'ATTACK', act: `open:fleetattack:${id}:${t}`, ok: up, tip: 'Shows the odds first. Or drop the fleet on theirs on the map.' }],
    };
  });
  const joins: PageRow[] = (o?.merge ?? []).map((other) => {
    const f = fleetView(s, other);
    return { nat: card.nat, title: f?.name ?? 'A fleet', lines: [f ? `${f.ships} ships. ${f.doing}.` : ''], buttons: [{ label: 'JOIN IT', act: `fleetmerge:${id}:${other}`, ok: up, tip: 'The two become one fleet under the other\'s flag and orders.' }] };
  });
  const gap = card.screen.gap > 0 ? ` ${cap1(card.screen.word)}: it has ${card.screen.have} destroyers where its heavy ships need ${card.screen.need}.` : '';
  const who = whoSails(s, id);
  return {
    id: `fleet:${id}`, title: card.name.toUpperCase(), headline: `${card.doing}`,
    note: `${cutOffNote(s, id)}${card.admiral ? `${card.admiral.name} commands (level ${card.admiral.level}). ` : ''}${card.ships} ships, readiness ${share(card.ready)}.${gap}${card.shelled ? ' Its guns have spoken this month.' : ''} A fleet reaches three seas from its base, and can be given one order a month.${who ? ` ${who}` : ''}`,
    top: [
      ...(() => {
        // (1.5.2) Cut off at sea beyond a shut strait, it makes for the nearest friendly harbour on its side, or has none.
        // (1.5.1) A fleet held where this month's battle left it is refused, with the reason, before either.
        const f = s.fleets.find((x) => x.id === id), cut = !!f && cutOff(s, f), haven = cut ? harbourFor(s, f!) : null;
        return [{ label: 'RETURN TO HARBOUR', act: `fleet:${id}:${f?.home ?? card.zone}:port`, ok: up && card.stance !== 'port' && !o?.held && (!cut || !!haven),
          tip: card.stance === 'port' ? 'It is in harbour now. Safe from the enemy fleet, and takes no part at sea.'
            : o?.held || (cut ? (haven ? `Cut off beyond a shut strait: it makes for ${haven.name}, which becomes its base.` : 'Cut off beyond a shut strait, and no friendly harbour on its side can take it.') : 'Safe from the enemy fleet, and takes no part at sea.') }];
      })(),
      ...(staffRunsDesk(s, 'navy') && fleetKeptByHand(s, id) ? [{ label: 'HAND BACK TO YOUR ADMIRALS', act: `fleetstaff:${id}`, ok: up, tip: HAND_BACK_TIP }] : []),
      ...(wingsOfFleet(s, id).length ? [{ label: 'CARRIER AIRCRAFT', act: `open:wings:${id}`, ok: true, tip: 'The fighters and strike aircraft its carriers bring: search a sea for the enemy, strike a fleet, or raid a harbour.' }] : []),
    ],
    sections: [
      { heading: 'ITS SHIPS', text: '', rows: ships },
      { heading: 'ENEMY FLEETS TO ATTACK', text: '', rows: targets },
      { heading: 'SEAS IN ITS REACH', text: seas.length ? '' : o?.held ? o.held : up ? 'The country is at peace: the fleet keeps its station.' : 'It is not your month.', rows: seas },
      { heading: 'LANES TO ESCORT', text: '', rows: lanes },
      { heading: 'COASTS TO SHELL', text: '', rows: coasts },
      { heading: 'FLEETS TO JOIN', text: '', rows: joins },
    ].filter((x) => x.rows.length || x.text),
  };
}

/**
 * (The 1.5.1 patch, fleetattack.ts; Brian 23 November 2026 22:23Z) The question before a fleet is sent straight at an enemy
 * fleet, as an army's face-off card asks it before an attack: the two sides, the odds, the chance it is won, what each
 * side is likely to lose, and the chances he is not found or slips away. SEND THEM IN fights it at once.
 */
function fleetAttackPage(s: Game, id: number, target: number): Page | null {
  const card = s.flags.open === 1 ? fleetView(s, id) : null;
  if (!card || card.nat !== s.human || !fleetAttackOn(s)) return null;
  const p = previewFleetAttack(s, id, target);
  if (!p) return null;
  const share = (x: number): string => `${Math.round(x * 100)}%`;
  const odds = (x: number): string => (x >= 9.95 ? 'more than 10' : x.toFixed(1));
  const lost = (x: Record<string, number>): string => {
    const parts = ['bb', 'cv', 'ca', 'dd', 'ss'].filter((c) => (x[c] ?? 0) > 0).map((c) => `${x[c]} ${SHIP_WORD[c][x[c] === 1 ? 0 : 1]}`);
    return parts.length ? parts.join(', ') : 'nothing';
  };
  const them = p.theirs.fleets.find((x) => x.id === target);
  const name = them?.name ?? 'the enemy fleet';
  const where = p.where === 'harbour' ? `in harbour at ${p.port}` : `in the ${p.zoneName}`;
  const side = (fs: typeof p.ours.fleets, own: boolean): PageRow[] => fs.map((x) => ({
    nat: x.nat, title: `${x.name}${x.count >= 0 ? `  \u00B7  ${x.count} ships` : x.size ? `  \u00B7  ${x.size.toLowerCase()}` : ''}`,
    lines: [
      x.ships ? ['bb', 'cv', 'ca', 'dd', 'ss'].filter((c) => (x.ships as unknown as Record<string, number>)[c] > 0).map((c) => `${(x.ships as unknown as Record<string, number>)[c]} ${SHIP_WORD[c][(x.ships as unknown as Record<string, number>)[c] === 1 ? 0 : 1]}`).join(', ') : 'Reported, not identified: its ships and strength are not known.',
      own ? `${x.admiral ? `${x.admiral} commands. ` : ''}Readiness ${share(x.ready ?? 1)}.` : '',
      x.power !== null ? `Fleet strength ${Math.round(x.power)}.` : '',
    ].filter(Boolean),
    buttons: [],
  }));
  const ticket: PageRow[] = p.lines.map((l) => ({ nat: '', title: l.label, lines: [l.value, l.effect ?? ''].filter(Boolean), buttons: [] }));
  const losses: PageRow[] = p.odds ? [
    { nat: card.nat, title: 'You are likely to lose', lines: [lost(p.likely.ours as unknown as Record<string, number>), `Between ${lost(p.range.ours[0] as unknown as Record<string, number>)} and ${lost(p.range.ours[1] as unknown as Record<string, number>)} as the day goes.`], buttons: [] },
    { nat: them?.nat ?? '', title: 'He is likely to lose', lines: [lost(p.likely.theirs as unknown as Record<string, number>), `Between ${lost(p.range.theirs[0] as unknown as Record<string, number>)} and ${lost(p.range.theirs[1] as unknown as Record<string, number>)} as the day goes.`], buttons: [] },
  ] : [];
  return {
    id: `fleetattack:${id}:${target}`, title: `ATTACK ${name.toUpperCase()}?`,
    headline: p.why ? p.why : p.odds ? `${p.verdict}: ${odds(p.odds.mid)} to 1, about ${share(p.win)} to win` : 'Its strength is not known',
    note: `${card.name} against ${name}, ${where}. ${p.where === 'harbour' ? 'Ships at anchor cannot manoeuvre, and the harbour\'s guns fire on the attackers.' : 'Fought at once, on the ships, guns, air cover, training and readiness of both sides.'}`,
    top: [
      { label: 'SEND THEM IN', act: `fleetattack:${id}:${target}`, ok: !p.why, tip: p.why ?? 'The fleet sails at once and the battle is fought now. The side that loses makes for its home port.', danger: !p.why && p.win < 0.4 },
      { label: 'CALL IT OFF', act: `goto:fleet:${id}`, ok: true, tip: 'The fleet stays where it is, in your hand.' },
    ],
    sections: [
      { heading: 'YOUR SHIPS', text: '', rows: side(p.ours.fleets, true) },
      { heading: 'HIS SHIPS, AS FAR AS YOU KNOW', text: '', rows: side(p.theirs.fleets, false) },
      { heading: 'THE ODDS', text: p.odds ? '' : 'Nothing of his strength is known: the odds cannot be given.', rows: ticket },
      { heading: 'IF IT IS FOUGHT', text: '', rows: losses },
      { heading: 'WORTH KNOWING', text: p.notes.join('\n'), rows: [] },
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
          lines: [r ? (c.done ? `Held by ${theName(s, r.ctrl)}.` : `In the hands of ${theName(s, r.ctrl)}: it must be yours or an ally's.`) : c.done ? 'Done.' : 'Not yet.'],
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
    // (1.5.2, playtest #36: each kind with its own count and noun, "1 infantry division, 2 engineer divisions", never
    // "1 infantry divisions"; the kinds' words as a division is named, "armoured" and "engineer".)
    const DIV_WORD: Record<string, string> = { Armor: 'armoured', Engineers: 'engineer', Fortress: 'fortress' };
    const what = [...kinds.entries()].map(([k, n]) => `${n} ${DIV_WORD[k] ?? k.toLowerCase()} division${n === 1 ? '' : 's'}`).join(', ');
    return {
      sort: (moving.length ? 0 : 1) * 1e6 - cs.reduce((n, c) => n + men(c), 0) / 1000,
      row: {
        nat: me, title: `${s.regions[reg].name}${gen ? `  ·  ${gen}` : ''}`,
        lines: [`${cs.length} corps, ${num(cs.reduce((n, c) => n + men(c), 0))} men${what ? `: ${what}` : ''}.`, moving.length ? `${moving.length} corps can still move this month.` : up ? 'It has used its month, or holds its ground.' : ''].filter(Boolean),
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
    // (Version 72) The crossings of a defended sea, and the plan the player names among them: the grand campaign only.
    top: seaCalls.crossings && s.flags.open === 1 ? [{ label: 'SEA CROSSINGS', act: 'open:crossings', ok: true, tip: 'Each enemy shore your army cannot march to: the safest way over, what bars it, and what would make it safer. Name one as your plan.' }] : [],
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
/** (Damaged cities) The player's damaged towns, the ones whose damage costs most first, and REPAIR ALL. */
function repairsPage(s: Game): Page | null {
  const me = s.human ? s.nations[s.human] : null;
  let plan: RepairAllLike | null = null;
  try { plan = repairCalls.plan?.(s) ?? null; } catch { plan = null; }
  if (!me || !plan) return null;
  const up = humanUp(s) && !onEve(s);
  const lost = plan.towns.reduce((t, x) => t + x.lost, 0);
  const rows: PageRow[] = plan.towns.map((t) => ({
    nat: '', title: `${t.town}  \u00B7  ${t.pct}% IN RUINS${t.hit ? '  \u00B7  HIT THIS MONTH' : ''}`,
    lines: [`It pays ${moneyWords(t.lost)} a month less.  \u00B7  Repair ${moneyWords(t.cost)}, done at once.  \u00B7  Pays for itself in ${monthsWords(t.months)}.`, ...(t.why && !t.poor ? [t.why] : [])],
    buttons: [
      { label: `REPAIR \u00B7 ${moneyWords(t.cost)}`, act: `repair:${t.region}`, ok: up && t.why === null, tip: !up ? 'It is not your month.' : t.why ?? 'The town pays in full again from this month.' },
      { label: 'GO THERE', act: `goto:region:${t.region}`, ok: true, tip: 'Looks at the place on the map.' },
    ],
  }));
  return {
    id: 'repairs', title: 'DAMAGED TOWNS',
    headline: plan.towns.length ? `${plan.towns.length} town${plan.towns.length === 1 ? '' : 's'} in ruins, costing ${moneyWords(lost)} a month` : 'Every one of your towns stands whole',
    note: 'Bombs, shells and the fighting leave a town partly in ruins, and it pays that much less until it is repaired: its factories, land, harbour and buildings. A repair costs a few months of what the damage costs, is paid and done at once, and can be taken back in the month it was ordered.',
    top: [{ label: `REPAIR ALL YOU CAN AFFORD \u00B7 ${moneyWords(plan.cost)}`, act: 'repair:all', ok: up && plan.pick.length > 0, tip: plan.pick.length ? `Repairs ${plan.pick.length} town${plan.pick.length === 1 ? '' : 's'}, the ones whose damage costs you most first: ${moneyWords(plan.gain)} a month more.${plan.short ? ` ${plan.short} more must wait for money.` : ''}` : 'The treasury cannot cover a repair now.' }],
    sections: [{ heading: 'THE TOWNS, THE ONES THAT COST YOU MOST FIRST', text: rows.length ? '' : 'Nothing to repair.', rows }],
  };
}

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
  if (cabinetTabTurn !== s.turn) { cabinetTabTurn = s.turn; cabinetTab = 'now'; }
  const d = v.directive;
  // (Version 62) A directive that names no town is carried out in another room: a door to it.
  const doorOf: Record<string, { label: string; act: string; tip: string }> = {
    build: { label: 'OPEN THE BUILDING LIST', act: 'open:towns', tip: 'Your towns and what can be built in them.' },
    agent: { label: 'OPEN THE SECRET SERVICE', act: 'open:spies', tip: 'Your agents, and recruiting more.' },
    trade: { label: 'OPEN TRADE AGREEMENTS', act: 'open:trade', tip: 'Your trade agreements, and who would sign another.' },
  };
  const directive: PageRow[] = d ? [{
    nat: s.nations[d.from] ? d.from : v.nat, title: `${d.title}  \u00B7  ${d.left <= 1 ? 'due this month' : `${months(d.left)} left`}`,
    lines: [d.text, `Set by ${d.by.replace(/^The /, 'the ')}, to be done by the end of ${turnToDate(d.due).label}.`, d.progress, (d as { oddsLine?: string }).oddsLine ?? '', `Reward: ${d.reward}.`.replace(/\.\.$/, '.'),
      // (Version 63) What ignoring it costs: a building or an agent is a small matter, with a flat price that is not counted with the directives ignored.
      (d as { small?: boolean }).small ? `Ignored: a small matter. The cabinet's trust falls by ${d.price} and no further, and it is not counted with the directives you have ignored.`
        : `Ignored: the cabinet's trust falls by ${d.price}, and settles lower until a directive is carried out.`,
      (d as { hard?: boolean }).hard ? 'The cabinet knows the place is strongly held: it has given more time and a larger reward.' : '',
      (d as { again?: boolean }).again ? 'The cabinet has asked for this before.' : ''].filter(Boolean),
    buttons: [
      ...(d.region !== undefined && d.region >= 0 ? [{ label: 'SHOW ON THE MAP', act: `goto:region:${d.region}`, ok: true, tip: `Goes to ${s.regions[d.region]?.name ?? 'the place'}.` }] : []),
      ...(doorOf[d.kind as string] ? [{ ...doorOf[d.kind as string], ok: true }] : []),
      (() => {
        const why = up ? whyNotSetAsideDirective(s) : notNow;
        const aside = (d as { asidePrice?: number }).asidePrice ?? d.price, next = (d as { asideNext?: number }).asideNext;
        return { label: 'SET IT ASIDE', act: 'cabinet:aside', ok: why === null, danger: true,
          tip: why ?? `Tells the cabinet it will not be done. Costs ${aside} of its trust at once${aside < d.price ? `, where letting it lapse would cost ${d.price}` : ''}${next !== undefined ? `. The next directive would then be set in ${turnToDate(next).label}` : ''}.` };
      })(),
    ],
  }] : [];
  const offices: PageRow[] = v.seats.map((seat) => ({
    nat: v!.nat, title: `${seat.name}  \u00B7  ${seat.minister.name}`,
    lines: [cap(seat.minister.plus), cap(seat.minister.minus)].filter(Boolean),
    buttons: seat.options.map((o) => ({
      label: o.name.toUpperCase(), act: `cabinet:appoint:${seat.id}:${o.id}`, ok: up && o.why === null,
      tip: !up ? notNow : o.why ?? `${(o as { wing?: string | null }).wing === 'hawk' ? 'A hawk. ' : (o as { wing?: string | null }).wing === 'dove' ? 'A dove. ' : ''}${cap(o.plus)}. ${cap(o.minus)}. The appointment costs ${v!.appointCost} political capital.`.replace(/\.\./g, '.'),
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
  // (Version 60: the cabinet talks back.) Everything below is read with care: an older rules bundle has none of it.
  const x = v as typeof v & {
    wings?: { hawks: number; doves: number; lean: number; word: string; text: string; effects: string[] } | null;
    promise?: { town: string; bandName: string; left: number; progress: string; reward: string; back: number; region?: number } | null;
    promiseWhy?: string | null; kept?: number; broken?: number;
    head?: { general: number; name: string; rank: string; command: string; why: string; left: number; gain: number; cost: number; vindicated: number; wrong: number; loved: boolean; fought?: number; won?: number; battles?: { text: string }[] } | null;
    secure?: boolean;
    stoodBy?: { general: number; name: string; left: number } | null;
  };
  const wingOf = (m: { wing?: string | null }): string => (m.wing === 'hawk' ? ' (a hawk)' : m.wing === 'dove' ? ' (a dove)' : '');
  for (const seat of v.seats) {
    const row = offices[v.seats.indexOf(seat)];
    if (row) row.title = `${seat.name}  \u00B7  ${seat.minister.name}${wingOf(seat.minister as { wing?: string | null })}`;
  }
  // Call your shot: the player's word to the cabinet, or the towns he could give it for.
  let word: { heading: string; text: string; rows: PageRow[] } | null = null;
  try {
    if (x.promise) {
      const pw = up ? whyNotTakeBackPromise(s) : notNow;
      word = { heading: 'YOUR WORD TO THE CABINET', text: `Promises kept: ${x.kept ?? 0}. Broken: ${x.broken ?? 0}.`, rows: [{
        nat: v.nat, title: `${x.promise.town}  \u00B7  ${x.promise.left <= 1 ? 'due this month' : `${months(x.promise.left)} left`}`,
        lines: [`${x.promise.bandName}. ${x.promise.progress}`, `Kept: ${x.promise.reward}.`],
        buttons: [
          ...(x.promise.region !== undefined && x.promise.region >= 0 ? [{ label: 'SHOW ON THE MAP', act: `goto:region:${x.promise.region}`, ok: true, tip: 'Goes to the town you promised.' }] : []),
          { label: 'TAKE IT BACK', act: 'cabinet:takeback', ok: pw === null, danger: true, tip: pw ?? `You tell the cabinet it cannot be done. Costs ${x.promise.back} of its trust, half of what breaking your word would.` },
        ],
      }] };
    } else if (x.promiseWhy !== undefined) {
      const targets = x.promiseWhy === null ? promiseTargetsOf(s, 6) : [];
      word = { heading: 'CALL YOUR SHOT', text: x.promiseWhy ?? (targets.length ? `Give the cabinet your word that a town will be in your side's hands by a day you name. Keep it and its trust rises; break it and it remembers. Promises kept: ${x.kept ?? 0}. Broken: ${x.broken ?? 0}. Any other enemy town within reach can be promised from its own province panel.` : 'No enemy town is within reach of your ground or your troops to promise.'),
        rows: targets.map((t) => ({
          nat: t.holder, title: `${t.town}  \u00B7  ${s.nations[t.holder]?.name ?? t.holder}  \u00B7  odds ${t.odds >= 10 ? 'better than 10' : t.odds.toFixed(1)} to 1`,
          lines: [t.depth <= 1 ? 'Beside your ground or your troops.' : `${t.depth} provinces from your ground and your troops.`],
          buttons: t.terms.map((term) => ({ label: `IN ${term.months} MONTHS`, act: `cabinet:promise:${t.region}:${term.months}`, ok: up, tip: !up ? notNow : term.text })),
        })) };
    }
  } catch { word = null; }
  // The cabinet wants a head: a general it asks the player to dismiss.
  let head: { heading: string; text: string; rows: PageRow[] } | null = null;
  if (x.head) {
    const h = x.head;
    const sw = up ? whyNotStandByGeneral(s) : notNow;
    head = { heading: 'THE CABINET WANTS A HEAD', text: '', rows: [{
      nat: v.nat, title: `${h.rank} ${h.name}  \u00B7  ${h.left <= 1 ? 'answer this month' : `${months(h.left)} to answer`}`,
      lines: [h.why, h.command ? `He commands ${h.command}.${h.loved ? ' The army thinks well of him: his corps will take his going badly.' : ''}` : 'He holds no command.',
        // (Version 62) His record, and the battles the cabinet holds against him.
        h.fought !== undefined ? `His record: ${h.fought} battle${h.fought === 1 ? '' : 's'} fought, ${h.won ?? 0} won.` : '',
        ...(h.battles?.length ? ['What the cabinet holds against him:', ...h.battles.map((b) => `  ${b.text}`)] : []),
        'Unanswered, the cabinet takes it as a refusal.'].filter(Boolean),
      buttons: [
        { label: 'DISMISS HIM', act: `cabinet:sack:${h.general}`, ok: up, danger: true, tip: !up ? notNow : `The cabinet has its head: its trust rises ${h.gain}. He leaves the army for good.` },
        { label: 'STAND BY HIM', act: 'cabinet:standby', ok: sw === null, tip: sw ?? `You refuse. The cabinet's trust falls ${h.cost} at once, and his next battle settles it: won, its trust rises ${h.vindicated} and he is vindicated for the rest of the war; lost, it falls ${h.wrong} more.` },
      ],
    }] };
  } else if (x.stoodBy) {
    head = { heading: 'THE CABINET WANTS A HEAD', text: `You stood by ${x.stoodBy.name}. His next battle settles it${x.stoodBy.left > 0 ? `, within ${months(x.stoodBy.left)}` : ''}.`, rows: [] };
  }
  const wings = x.wings ? { heading: 'HAWKS AND DOVES', text: [`${x.wings.hawks} hawk${x.wings.hawks === 1 ? '' : 's'} and ${x.wings.doves} dove${x.wings.doves === 1 ? '' : 's'} at the table: ${x.wings.word.toLowerCase()}. ${x.wings.text}`, ...x.wings.effects].join('\n'), rows: [] as PageRow[] } : null;
  // (Version 62) Four pages, so that the cabinet's business is all on the first: the directive, a question about a
  // general, and the player's own promise; then the ministers, the decrees, and the minutes.
  const filled = v.seats.filter((seat) => seat.minister.id !== 'caretaker').length, inForce = v.tracks.filter((t) => t.at > 0).length;
  const tabs: { id: typeof cabinetTab; label: string; tip: string }[] = [
    { id: 'now', label: `BUSINESS${x.head ? '  \u00B7  A QUESTION' : d && d.left <= 2 ? '  \u00B7  DUE' : ''}`, tip: 'The directive, a general the cabinet asks about, and your own word to it.' },
    { id: 'table', label: `MINISTERS  \u00B7  ${filled} OF ${v.seats.length}`, tip: 'The four offices, who holds them, and the hawks and doves at the table.' },
    { id: 'decrees', label: `DECREES${inForce ? `  \u00B7  ${inForce} IN FORCE` : ''}`, tip: 'The measures your government has decreed, and the next step of each.' },
    { id: 'minutes', label: 'MINUTES', tip: "What moved the cabinet's trust, and what it has minuted." },
  ];
  const comes = { heading: 'WHAT IT ALL COMES TO', text: v.sums.length ? v.sums.join('  \u00B7  ') : 'Nothing either way.', rows: [] as PageRow[] };
  const shown = cabinetTab === 'table' ? [
    { heading: 'THE FOUR OFFICES', text: `A new minister costs ${v.appointCost} political capital. Each brings something and costs something.`, rows: offices },
    ...(wings ? [wings] : []), comes,
  ] : cabinetTab === 'decrees' ? [
    { heading: 'DECREES', text: 'One step up at a time, for political capital. A step down is free.', rows: decrees }, comes,
  ] : cabinetTab === 'minutes' ? [
    { heading: "WHAT MOVED THE CABINET'S TRUST", text: v.moved.length ? v.moved.map((m) => { const x = Math.round(m.amount * 10) / 10; return `${m.label} ${x > 0 ? '+' : x < 0 ? '\u2212' : ''}${Math.abs(x).toFixed(1)}`; }).join('  \u00B7  ') : 'Nothing since it last read the news.', rows: [] as PageRow[] },
    { heading: 'MINUTES', text: v.log.length ? v.log.slice(-12).reverse().map((l) => `${turnToDate(l.turn).label}: ${l.text}`).join('\n') : 'Nothing minuted yet.', rows: [] as PageRow[] },
  ] : [
    { heading: 'THE DIRECTIVE', text: d ? '' : `No directive stands. ${v.nextIn > 0 ? `The cabinet sets the next in ${months(v.nextIn)}.` : 'The cabinet is weighing the next.'}`, rows: directive },
    ...(head ? [head] : []),
    ...(word ? [word] : []),
  ];
  const secure = !!x.secure;
  const standing = secure && falling
    ? `The cabinet has no trust left in you. Your office is secure, so it cannot bring you down, but it still withholds. ${(v as { withheld?: string | null }).withheld ?? ''}`
    : secure ? `With no news the cabinet's trust drifts back toward ${Math.round(v.rest)}. Your office is secure: the cabinet cannot fall.`
      : falling ? `The cabinet's trust is at ${v.fallsAt} or under: if it stays there for ${months(v.crisisMonths)} running the cabinet falls (it has been there for ${months(v.low)}).`
        : `With no news the cabinet's trust drifts back toward ${Math.round(v.rest)}. At ${v.fallsAt} or under for ${months(v.crisisMonths)} running, the cabinet falls.`;
  return {
    id: 'cabinet', title: 'THE CABINET',
    // (the headline is one short line: the screen cuts a long one off. Trust is shown rounded down, as the cabinet reckons it.)
    headline: `Trust ${Math.floor(v.trust)} of 100  \u00B7  ${v.trustWord}`,
    note: `${v.form.name}${(v as { since?: number }).since !== undefined ? `, in office since ${turnToDate((v as { since?: number }).since!).label}` : ''}. Political capital ${Math.floor(v.capital)} of ${v.capitalMost}, +${v.capitalMonth.toFixed(1)} a month. ${(v as { doubts?: boolean }).doubts && !(secure && falling) ? `THE CABINET DOUBTS YOU. ${(v as { withheld?: string | null }).withheld ?? ''} ` : ''}${cabinetTab === 'now' ? `${v.form.blurb} ` : ''}${standing} Directives carried out: ${v.done}. Missed or set aside: ${v.missed}.`,
    top: [
      ...tabs.map((t) => ({ label: t.label, act: `cabinet:tab:${t.id}`, ok: t.id !== cabinetTab, tip: t.id === cabinetTab ? 'This page is open.' : t.tip })),
      secure
        ? { label: 'YOUR OFFICE IS SECURE', act: 'cabinet:office:fall', ok: true, tip: 'The cabinet never falls and you are never turned out of office, though a cabinet that doubts you still holds back money and political capital. Press to let it bring you down again: a cabinet left with no trust in you for months running then falls.' }
        : { label: 'YOU CAN BE BROUGHT DOWN', act: 'cabinet:office:secure', ok: true, tip: `A cabinet left with no trust in you for ${months(v.crisisMonths)} running falls. Press to make your office secure: the cabinet, its ministers, directives and trust all work as they do, and it still withholds when it doubts you, but it never falls.` },
      { label: 'PLAY WITHOUT THE CABINET', act: 'cabinet:switch:off', ok: true, danger: true, tip: 'Switches the war cabinet off: no ministers, decrees, trust or directives, and nothing they moved is moved. What stands open is settled first, each at its usual price, and you are shown the bill before it is done. You can switch it on again in any month, from MENU, OPTIONS.' },
    ],
    sections: shown,
  };
}

/** (Version 62) The reserve of men as a sum, and the player's two orders over the call-up. */
function menPage(s: Game): Page | null {
  let m: ReturnType<typeof manpowerOf> = null;
  try { m = manpowerOf(s); } catch { return null; }
  if (!m || !s.human) return null;
  const up = humanUp(s) && !onEve(s);
  const short = (n: number): string => (Math.abs(n) >= 1e6 ? `${(n / 1e6).toFixed(Math.abs(n) >= 1e7 ? 0 : 1)} million` : Math.round(n).toLocaleString('en'));
  const x = m as typeof m & { came?: number; gap?: number; start: number; drawn: number; classMen: number; men: number; line: string; word: string };
  const start = Math.round(x.men - (x.came ?? 0) + x.drawn);
  const c = callUpOf(s);
  const rows: PageRow[] = [];
  if (c) {
    const e = c.early;
    rows.push({ nat: s.human, title: 'The next class, early', lines: [e.text, e.why ?? ''].filter(Boolean),
      buttons: [{ label: 'CALL THEM UP NOW', act: 'men:early', ok: !e.why, danger: true, tip: e.why ?? `About ${short(e.men)} men join the reserve at once, ${e.months === 1 ? 'a month' : `${e.months} months`} of classes together. No class then comes of age for ${e.gap === 1 ? 'a month' : `${e.gap} months`}. They are the same men, sooner. It cannot be undone, and you are asked twice.` }] });
    if (c.callUp) {
      rows.push({ nat: s.human, title: `The standing call-up${c.callUp.held ? '  \u00B7  held' : ''}`, lines: [c.callUp.text, c.callUp.held && c.callUp.drafts > 0 ? `Going to the corps in the field while it is held: up to ${short(c.callUp.drafts)} men a month.` : ''].filter(Boolean),
        buttons: [{ label: c.callUp.held ? 'LET IT RUN' : 'HOLD IT', act: `men:hold:${c.callUp.held ? 0 : 1}`, ok: up, tip: up ? (c.callUp.held ? 'New corps are raised from the reserve again, from next month.' : 'No new corps are raised from the reserve: the men stay for the corps in the field. It takes hold from next month.') : 'It is not your month.' }] });
    }
  }
  return {
    id: 'men', title: 'THE RESERVE OF MEN',
    headline: `${short(x.men)} men in reserve  \u00B7  ${x.word}`,
    note: `${x.line}${(x.gap ?? 0) > 0 ? ` A class was called up early: none comes of age for ${x.gap} more month${x.gap === 1 ? '' : 's'}.` : ''}`,
    top: [],
    sections: [
      { heading: 'THE COUNT', text: [`At the start: ${short(start)}`, `Come of age since: +${short(x.came ?? 0)}`, `Called up so far: \u2212${short(x.drawn)}`, `In reserve today: ${short(x.men)}`, `Come of age each month: +${short(x.classMen)}`].join('\n'), rows: [] },
      ...(rows.length ? [{ heading: 'YOUR SAY OVER THE CALL-UP', text: '', rows }] : []),
    ],
  };
}

/**
 * (Version 64) The card that folds two worn corps into one: every pair that can be folded, either way round, and
 * what each makes of them. Nothing is done until one of its buttons is pressed.
 */
function foldPage(s: Game, reg: number): Page | null {
  const r = s.regions[reg];
  if (!r || !s.human) return null;
  let vs: ReturnType<typeof foldChoicesAt> = [];
  try { vs = foldChoicesAt(s, reg); } catch { vs = []; }
  const pct = (x: number): string => `${Math.round(x * 100)}%`;
  const num = (n: number): string => Math.round(n).toLocaleString('en');
  const corpsOfId = (id: number): Corps | undefined => s.corps.find((c) => c.id === id);
  const pairs = new Map<string, typeof vs>();
  for (const v of vs) { const k = [v.keep, v.gone].sort((a, b) => a - b).join(':'); (pairs.get(k) ?? pairs.set(k, []).get(k)!).push(v); }
  const sections = [...pairs.values()].map((p) => {
    const a = corpsOfId(p[0].keep), b = corpsOfId(p[0].gone);
    return {
      heading: a && b ? `${a.name.toUpperCase()} AND ${b.name.toUpperCase()}` : 'TWO CORPS', text: a && b ? `${a.name}: ${num(men(a))} men, ${pct(a.str)} of its strength. ${b.name}: ${num(men(b))} men, ${pct(b.str)}.` : '',
      rows: p.map((v): PageRow => ({
        nat: s.human!, title: `Keep ${v.keepName}`,
        lines: [
          `${num(v.men)} men in one corps at ${pct(v.str)} of its strength, ${grade(v.xp).toLowerCase()}.`,
          v.general ? `${v.general} commands${v.stepsDown ? `; ${v.stepsDown} goes to the list of generals waiting for a corps` : ''}.` : 'No general commands it.',
          v.back > 0 ? `${num(v.back)} men over go back to the reserve.` : '',
          v.works !== undefined ? `It takes over the fieldworks ${v.goneName} stands in.` : '',
          `${v.goneName} goes home as a cadre: it can be raised again from the arsenal for ${shown(v.cadreCost).toLocaleString('en')} from the treasury and ${num(v.cadreMen)} men from the reserve, in ${v.cadreMonths} month${v.cadreMonths === 1 ? '' : 's'}.`,
          v.why ?? '',
        ].filter(Boolean),
        buttons: [{ label: `FOLD INTO ${v.keepName.toUpperCase()}`, act: `fold:${v.keep}:${v.gone}`, ok: !v.why, danger: true, tip: v.why ?? `${v.keepName} takes in the men of ${v.goneName}. It cannot be undone.` }],
      })),
    };
  });
  return {
    id: `fold:${reg}`, title: 'FOLD TWO WORN CORPS INTO ONE', headline: `The army at ${r.name}  \u00B7  ${turnToDate(s.turn).label}`,
    note: `Two corps of the same kind, both under ${Math.round(MANPOWER.fold.worn * 100)}% of their strength, can be made into one that can fight. The corps that is kept loses a little of its order while the men shake down together, and has the fewer moves of the two left this month. The other leaves the map with its name, its record and its honours, and waits in the arsenal to be raised again at ${Math.round(MANPOWER.fold.price * 100)}% of the price of a new corps.`,
    top: [],
    sections: sections.length ? sections : [{ heading: 'NOTHING TO FOLD', text: 'No two corps here can be folded together this month.', rows: [] }],
  };
}

/** What kind of picture a raid's target wants: the thing the bombs are meant for. */
function raidSort(s: Game, kind: string, target: number): string {
  if (kind === 'fleet' || kind === 'strike') return 'fleet';
  if (kind === 'harbour') return 'harbour';
  if (kind === 'superiority') return 'airfield';
  if (kind === 'ground') return 'troops';
  if (kind === 'line') return 'works';
  if (kind === 'town') return 'town';
  return (s.regions[target]?.fac ?? 0) > 0 ? 'factories' : 'rail';
}

/**
 * (Brian's ask of 15 November 2026, ruling 54.) A raid being planned, as a card: the target and the others in reach,
 * every air group as a seat with its aircraft and its readiness, and the forecast as numbers for bars. All of it is
 * the rules' own preview of the raid: nothing is reckoned here. (Morale comes when the rules have it.)
 */
function raidCardOf(s: Game, kind: string, target: number, picked: number[]) {
  try {
    const k = kind as RaidKind;
    const p: RaidPlan = weighRaid(s, k, target, picked);
    const me = s.human ?? '';
    const sea = kind === 'fleet' || kind === 'strike';
    const link = (t: number, list: number[]): string => `open:plan:${kind}:${t}:${list.join('.')}`;
    // (Ruling 107(d), the art director's brief of 16 November.) A bombing raid is weighed by day and by night, side
    // by side: the rules' own two forecasts for the same groups. Fighters asked for go by day and stay at home by
    // night, so every link on the night card carries them still and a change of mind drops nobody.
    const dn = kind === 'bomb' || kind === 'night' ? (() => { try { return weighDayNight(s, k, target, picked); } catch { return null; } })() : null;
    const keep = (list: number[]): number[] => (dn && kind === 'night' ? [...list, ...dn.home.filter((id) => !list.includes(id))] : list);
    const carry = keep(p.picked);
    const picture = (t: number, holder: string): string => `Target:${raidSort(s, kind, t)}:${sea ? -1 : t}:${holder || 'X'}`;
    // (The art director's defences brief: a place's anti-aircraft guns, 0 to 3, which the rules never hide; -1 for a
    // target that is not a place.)
    // (The art director's round 38: the guns round a place take part in the bombing of its industry, its town and its
    // harbour, by day or by night. In any other raid they take none, and the targets carry no pips.)
    const gunsOf = (t: number): number => (sea || !s.regions[t] || !['bomb', 'night', 'town', 'harbour'].includes(kind) ? -1 : gunsRound(s, t));
    const stops = raidStopsFor(s, k, -1);
    const here = stops.find((t) => t.id === target);
    const lead = p.seats.find((x) => x.picked && x.kind !== 'ftr') ?? p.seats.find((x) => x.picked);
    // (The art director's round 32: a fleet is titled by its sea, "Western Approaches", and whose ships they are
    // goes on the line below; every other target is titled by its own name.)
    const titled = (t: { name: string; zone?: number } | undefined, fallback: string): { title: string; who: string } => {
      const name = t?.name ?? fallback;
      if (kind !== 'fleet') return { title: name, who: '' };
      const sea = (s.seas[t?.zone ?? -1]?.name ?? '').replace(/^the /i, '');
      const cut = name.indexOf(' in the ');
      return { title: sea || name, who: cut > 0 ? name.slice(0, cut) : name };
    };
    const card = {
      kind, kindLabel: raidKinds(s).find((x) => x.key === kind)?.label ?? '', why: p.why ?? '',
      target: { id: target, name: here?.name ?? p.place, km: Math.round(here?.km ?? 0), holder: here?.holder ?? '', sort: raidSort(s, kind, target), act: '', now: true, picture: picture(target, here?.holder ?? ''), ...titled(here, p.place) },
      // (about twenty words of prose on the card at most: the first sentence of what a hit would do; the rest is on the pointer)
      what: here?.what ?? '', effect: p.why ? '' : ((t: string): string => { const i = t.indexOf('. '); return i > 0 && t.length > 90 ? t.slice(0, i + 1) : t; })(p.effects[0] ?? ''),
      // (Release 1.5.2: a fight for the sky sent with too few fighters to hold one says so, after what the fight should do.)
      effects: p.why ? '' : [...p.effects, p.tooFew ?? ''].filter(Boolean).join(' '),
      // (The art director's card: up to three boxes, each one number and a few words on what a hit does. The rules
      // give these as sentences today, so the figures are read out of the rules' own words; a sentence that fits
      // none of these gets no box, and the whole of it is on the pointer.)
      hits: p.why ? [] : ((all: string): { n: string; words: string }[] => {
        const out: { n: string; words: string }[] = [];
        const take = (re: RegExp, n: (m: RegExpMatchArray) => string, words: string | ((m: RegExpMatchArray) => string)): void => {
          const m = all.match(re);
          if (m && out.length < 3) out.push({ n: n(m), words: typeof words === 'string' ? words : words(m) });
        };
        take(/stop about (\d+(?:\.\d+)?)% of [^.]*output/, (m) => `\u2212${m[1]}%`, 'output, for a month');
        take(/(\d+)% chance of cutting the railway/, (m) => `${m[1]}%`, 'chance the railway is cut');
        take(/(\d+)% chance of wrecking a factory/, (m) => `${m[1]}%`, 'chance a factory is wrecked');
        take(/lose about (\d+)% of their organisation/, (m) => `\u2212${m[1]}%`, 'of the troops\' organisation');
        take(/break down about (\d+)% of the works/, (m) => `\u2212${m[1]}%`, 'of the town\'s works');
        take(/(\d+)% of the town guard/, (m) => `\u2212${m[1]}%`, 'of the town guard');
        take(/Fighter odds (\d+(?:\.\d+)?) to 1/, (m) => `${m[1]} to 1`, 'fighter odds going in');
        // (1.5.2, the UI director 04:55Z and 05:38Z: the box says what winning a sky takes, both figures, never a bare
        // multiple; figure and words read on as one sentence, and "of yours" keeps the 50 from reading as the defenders'.)
        take(/with (\d+(?:\.\d+)?) times what the defender/, (m) => `${m[1]} to 1`, `against the defenders' fighters wins the sky, with ${AIR.sky.least} of yours still over the target at the end`);
        take(/lose up to (\d+)% of their number/, (m) => `${m[1]}%`, 'of warships caught, at most');
        take(/About (\d+(?:\.\d+)?) bombs and torpedoes should hit/, (m) => m[1], 'bombs and torpedoes should hit');
        take(/chance of (\d+)% for (an? [a-z ]+?)[.,]/, (m) => `${m[1]}%`, (m) => `chance ${m[2]} goes down`);
        take(/takes? about (\d+(?:\.\d+)?) months? off/, (m) => `\u2212${m[1]}`, 'months of fieldworks');
        return out;
      })(p.effects.join(' ')),
      from: lead?.baseName ?? '', holderName: s.nations[here?.holder ?? '']?.name ?? '', inReach: stops.length,
      allAct: `open:raid:${kind}`, otherAct: 'open:raids',
      strip: stops.slice(0, 10).map((t) => {
        // (the same groups against the next target where they can fly it; the rules' own pick where they cannot)
        let q = weighRaid(s, k, t.id, p.picked);
        if (q.why || !q.picked.length) q = weighRaid(s, k, t.id);
        return { id: t.id, name: t.name, km: Math.round(t.km), holder: t.holder, sort: raidSort(s, kind, t.id), act: link(t.id, keep(q.picked)), now: t.id === target, picture: picture(t.id, t.holder), guns: gunsOf(t.id), ...titled(t, t.name) };
      }),
      seats: p.seats.map((x) => {
        // (by night: an escort that was picked stays on the card and stays at home; one that could join by day says so)
        const stays = !!dn && kind === 'night' && dn.home.includes(x.id);
        const dayOnly = !!dn && kind === 'night' && !stays && dn.barred.includes(x.id);
        // (A carrier wing whose fleet's station is out of reach of the target has no distance to fly, which the rules
        // give as Infinity: it has no km, its distance in words is 'Out of reach', and its reason says why. Findings 9
        // and 34 of 1.5.1; the UI director's words of 05:10Z.)
        const out = !Number.isFinite(x.km);
        return {
          id: x.id, name: x.name, kind: x.kind, aircraft: Math.round(x.aircraft), base: x.baseName, afloat: x.ship >= 0, ...(out ? {} : { km: Math.round(x.km) }), ready: x.ready,
          far: out ? 'Out of reach' : `${Math.round(x.km).toLocaleString('en-US')} km`,
          picked: x.picked || stays, locked: x.locked, lead: !!lead && x.id === lead.id, why: stays ? '' : dayOnly ? 'No escort by night' : out && x.why ? 'Its aircraft cannot reach it from the fleet\'s station.' : x.why ?? '', note: x.note, nat: me,
          stays, dayOnly,
          act: stays ? link(target, carry.filter((id) => id !== x.id)) : x.why ? '' : x.picked ? link(target, carry.filter((id) => id !== x.id)) : link(target, [...carry, x.id]),
        };
      }),
      ways: dn ? ((): unknown => {
        // (a line that one column has, the other prints too, dim, with its own figure: the rules' own since 82.2)
        const one = (w: RaidWay) => ({
          label: w.label, night: w.kind === 'night', chosen: w.kind === kind,
          why: w.why ? `Cannot fly ${w.kind === 'night' ? 'by night' : 'by day'}: ${w.why.charAt(0).toLowerCase()}${w.why.slice(1)}` : '',
          success: w.success, verdict: w.verdict, lost: w.lost, lostShare: w.bomberLoss, fightersLost: w.fightersLost, fighterShare: w.fighterLoss,
          hits: w.why ? [] : w.hits.map((h) => ({ n: h.n, words: h.words, dim: !!h.dim })),
          // (the guns' part: the line under the bombers lost, and the note on the WHAT A HIT DOES line; the 3D card's
          // column is narrower than the browser's, so the note stops at the figure)
          toGuns: w.toGuns, gunsWords: w.gunsWords, stopNote: w.stopNote.replace(/ of the bombing$/, ''),
          foot: w.foot,
          act: w.kind === kind ? '' : `open:plan:${w.kind}:${target}:${carry.join('.')}`,
          tip: w.kind === kind ? `The raid is planned ${w.kind === 'night' ? 'by night' : 'by day'}.` : `Plan it ${w.kind === 'night' ? 'by night' : 'by day'} instead. Nobody you picked is dropped.`,
        });
        const foes = Math.round(dn.foes);
        return {
          label: 'THE FORECAST  \u00B7  BY DAY OR BY NIGHT', bombers: Math.round(dn.bombers),
          foes: foes > 0 ? `${foes.toLocaleString('en-US')}` : 'No', foesWords: foes > 0 ? 'enemy fighters in reach' : 'fighters in reach',
          // (the guns round the target, common to both ways: the level for the pips, and Brian's own words for them)
          guns: dn.guns, gunsFigure: dn.guns > 0 ? `Level ${dn.guns}` : 'No', gunsNoun: 'AA guns', gunsTail: dn.gunsTail ? `\u00B7 ${dn.gunsTail}` : '', gunsTip: `${dn.gunsLabel}. Each level stops a share of the bombing and brings down more bombers.`,
          day: one(dn.day), night: one(dn.night),
        };
      })() : null,
      success: p.success, verdict: p.verdict, bombers: Math.round(p.bombers), bomberLoss: Math.round(p.bomberLossN), bomberShare: p.bomberLoss,
      fighters: Math.round(p.fighters), fighterLoss: Math.round(p.fighterLossN), foeLoss: Math.round(p.foeLossN),
      meets: p.meets.map((m) => ({ nat: m.nat, name: m.name, base: m.baseName, ftr: Math.round(m.ftr), able: Math.round(m.able) })),
      massed: p.massed, offPatrol: p.offPatrol, ticket: p.lines.map((l) => `${l.label}: ${l.value}${l.effect ? ` (${l.effect})` : ''}`).join('\n'),
      // (any raid: the guns' part of the bombers lost, for the one line under them on the single forecast)
      gunsWords: p.why || p.bombers <= 0 ? '' : p.guns.level ? 'of them to flak and guns' : 'of them to light flak', toGuns: p.why || p.bombers <= 0 ? '' : `about ${Math.round(p.guns.lossN).toLocaleString('en-US')}`,
      fly: { label: dn ? (kind === 'night' ? 'FLY THE RAID BY NIGHT' : 'FLY THE RAID BY DAY') : 'FLY THE RAID', act: `raid:${kind}:${target}:${p.picked.join('.')}`, ok: humanUp(s) && !onEve(s) && !p.why, tip: p.why ?? 'Flown at once. Every group that goes is away for the rest of the month.' },
    };
    // (the same round: the one line of prose is dropped when it only repeats a lone box)
    if (card.hits.length === 1 && card.effect.includes(` ${card.hits[0].n.replace(/[^0-9.]/g, '')}`)) card.effect = '';
    return card;
  } catch { return null; }
}

function page(id: string, region = -1) {
  const s = game();
  const part = id.split(':');
  const p = id === 'repairs' ? repairsPage(s) : id === 'staff' ? staffPage(s) : id === 'cabinet' ? cabinetPage(s) : id === 'men' ? menPage(s) : id === 'trade' ? tradePage(s) : id === 'towns' ? townsPage(s) : part[0] === 'town' ? townPage(s, Number(part[1]))
    : id === 'spies' ? spiesPage(s, region) : id === 'crossings' ? crossingsPage(s) : id === 'lastact' ? lastActPage(s) : id === 'forces' ? forcesPage(s) : id === 'reports' ? reportsPage(s) : part[0] === 'battle' ? battleReportPage(s, Number(part[1])) : id === 'goals' ? goalsPage(s) : id === 'heroes' ? heroesPage(s) : part[0] === 'fleet' ? fleetPage(s, Number(part[1])) : part[0] === 'fleetattack' ? fleetAttackPage(s, Number(part[1]), Number(part[2])) : part[0] === 'shell' ? shellPage(s, Number(part[1]), Number(part[2]))
    : airPage(s, id, region) ? airPage(s, id, region)
    : id === 'airfield' ? airfieldPage(s, region)
    : part[0] === 'raid' ? raidPage(s, part[1] as AirMission, Number(part[2]))
    : part[0] === 'rebase' ? rebasePage(s, Number(part[1])) : part[0] === 'fold' ? foldPage(s, Number(part[1])) : null;
  // An air group's row carries a large picture of its own machine, wherever the row is of an air group by name:
  // the fighter, or the bomber where the group is mostly bombers. (The fine model of an aircraft is seen here.)
  if (p) {
    for (const section of p.sections) {
      for (const row of section.rows as PageRow[]) {
        if (row.picture || !row.nat) continue;
        const f = s.airFleets.find((x) => x.name === row.title && x.nat === row.nat);
        if (f) row.picture = `${f.bmr > f.ftr ? 'air:bmr' : 'air:ftr'}:${f.nat}`;
      }
    }
  }
  // (Version 66) A room that belongs to a desk the staff runs says so at its head, with the staff's line for the month
  // and a way to take the desk back; one the player runs offers, at its head, to hand it over.
  const desk = p ? deskOfPage(id) : null;
  if (p && desk) {
    const bar = deskBar(s, desk);
    if (bar) {
      p.note = bar.staff ? `${bar.line}${p.note ? `  ${p.note}` : ''}` : p.note;
      p.top = [...p.top, { label: bar.label, act: bar.act, ok: true, tip: bar.tip }];
    }
  }
  // (Ruling 54.) A raid being planned is set out as a card: the words give way to it, and the whole list of
  // targets stays one button away.
  const card = p && part[0] === 'plan' ? raidCardOf(s, part[1], Number(part[2]), (part[3] ?? '').split('.').filter(Boolean).map(Number).filter((n) => Number.isFinite(n))) : null;
  if (p && card) {
    return { ok: true, ...p, headline: '', note: '', top: [], card };
  }
  // (Rules 84.3.3, trap 1.) The card that opens a war played from its first month has no month of reports behind it:
  // the page is the card alone, for lesson 1 to lie on.
  if (!p && id === 'reports' && lastCard?.opening) {
    try { return { ok: true, id: 'reports', title: `THE MONTH'S REPORT  \u00B7  ${lastCard.date.toUpperCase()}`, headline: '', note: '', top: [], sections: [], month: monthShown(s, lastCard) }; } catch { /* no card, then */ }
  }
  // (Ruling 64.) The month's report is one card: the page's own lists stay behind it for a game with none.
  if (p && id === 'reports' && lastCard) {
    try { return { ok: true, ...p, month: monthShown(s, lastCard) }; } catch { /* the page as it was */ }
  }
  return p ? { ok: true, ...p } : { ok: false };
}

// ---- How deep the player plays (Version 66) ----------------------------------------------------

const lowFirst = (t: string): string => (/^The /.test(t) ? `the ${t.slice(4)}` : t.charAt(0).toLowerCase() + t.slice(1));
/** "Your admirals run the navy and convoys." (and of the secret service, which is its own staff, that it runs itself) */
const runsLine = (lead: string, desk: string): string => (lead.toLowerCase().endsWith(lowFirst(desk).replace(/^the /, ''))
  ? `${lead} runs itself for you.` : `${lead} run${/s$/.test(lead) ? '' : 's'} ${lowFirst(desk)}.`);

/** The desk a room belongs to, or null: the secret service's room, trade and the towns, the airfield's, a fleet's. (The rooms that only ask a question, such as where a raid is to go, carry no bar.) */
function deskOfPage(id: string): DeskId | null {
  const head = id.split(':')[0];
  return head === 'spies' ? 'spies' : head === 'trade' || head === 'towns' ? 'build' : head === 'airfield' ? 'air' : head === 'fleet' ? 'navy' : null;
}

/** For a bar on a desk's own screen: who runs it, a line to read, and the button that changes hands. Null outside the grand campaign. */
function deskBar(s: Game, desk: DeskId): { staff: boolean; line: string; label: string; act: string; tip: string } | null {
  let v: ReturnType<typeof commandOf> = null;
  try { v = pictureCommand(s); } catch { v = null; }
  const d = v?.desks.find((x) => x.id === desk), def = deskDef(desk);
  if (!v || !d || !def || !s.human || !s.nations[s.human]?.alive) return null;
  return d.staff
    ? { staff: true, line: `${runsLine(def.lead, d.name)} ${d.news ?? 'Nothing to report this month.'} What you order here by hand still goes out.`, label: 'TAKE IT BACK', act: `desk:${desk}:own`, tip: `${def.hand} You can hand it back to your staff in any month.` }
    : { staff: false, line: '', label: `LET MY STAFF RUN ${d.name.toUpperCase()}`, act: `desk:${desk}:staff`, tip: `${def.runs} You can take it back in any month.` };
}

/** The depth and the desks, for the frame: null outside the grand campaign. */
function commandBrief(s: Game): ReturnType<typeof workOutCommandBrief> {
  if (pictureDepth > 0 && pictureBriefKept && pictureBriefKept.s === s) return pictureBriefKept.brief;
  const brief = workOutCommandBrief(s);
  if (pictureDepth > 0) pictureBriefKept = { s, brief };
  return brief;
}
function workOutCommandBrief(s: Game): { name: string; under: string; depth: string; staffed: number; desks: { id: string; name: string; staff: boolean; line: string; label: string; act: string; tip: string }[] } | null {
  try {
    const v = pictureCommand(s);
    if (!v || !s.human || !s.nations[s.human]?.alive) return null;
    return {
      name: v.name, under: v.under, depth: v.depth ?? '', staffed: v.desks.filter((d) => d.staff).length,
      desks: v.desks.map((d) => { const bar = deskBar(s, d.id)!; return { id: d.id, name: d.name, staff: d.staff, line: bar.line, label: bar.label, act: bar.act, tip: bar.tip }; }),
    };
  } catch { return null; }
}

/** The staff's room: the three depths, the six desks with who runs each, and what the staff may spend. */
function staffPage(s: Game): Page | null {
  let v: ReturnType<typeof commandOf> = null;
  try { v = commandOf(s); } catch { v = null; }
  if (!v) return null;
  const list = (names: string[]): string => (names.length ? names.join(', ') : 'nothing');
  return {
    id: 'staff', title: 'YOUR STAFF',
    headline: v.depth ? `You are playing at ${v.name} (${v.under}).` : 'You are playing a mix of your own.',
    note: 'The armies and their battles are always yours, and so are declaring war and landings. Any desk below can be handed to your staff or taken back in any month. How deep you play is separate from how hard the war is: the rival staffs fight the same at every depth.',
    top: [],
    sections: [
      {
        heading: 'HOW DEEP YOU PLAY', text: v.depth ? 'Pick a depth to set every switch at once.' : 'You have made a mix of your own from the desks below. Pick a depth to set them all at once.',
        rows: v.depths.map((d) => ({
          nat: '', title: `${d.name}  \u00B7  ${d.under}${d.on ? '  \u00B7  where you are' : ''}`,
          lines: [d.blurb, `Yours: ${list(d.yours)}.`, `Your staff's: ${list(d.staff)}.`],
          // (The UI director, 10:57 on build 1638: the depth played has no button, which read as a dead one in faint ink.
          // Its title already says where you are.)
          buttons: d.on ? [] : [{ label: `GO TO ${d.name.toUpperCase()}`, act: `depth:${d.id}`, ok: true, tip: `${d.blurb} It can be changed again in any month.` }],
        })),
      },
      ...(() => { const d = dialSection(s); return d ? [d] : []; })(),
      {
        heading: 'THE SIX DESKS', text: '',
        rows: v.desks.map((d) => ({
          nat: '', title: `${d.name}  \u00B7  ${d.staff ? 'your staff runs it' : 'you run it'}`,
          lines: [d.text, d.state, d.staff ? (d.news ? `This month: ${d.news}` : 'This month: nothing to report.') : ''].filter(Boolean),
          buttons: [{ label: d.staff ? 'TAKE IT BACK' : 'LET MY STAFF RUN IT', act: `desk:${d.id}:${d.staff ? 'own' : 'staff'}`, ok: true, tip: d.staff ? `${deskDef(d.id)?.hand ?? ''} What your staff ordered this month stands.` : `${deskDef(d.id)?.runs ?? ''} Handed over in your own month, the staff goes to work at once.` }],
        })),
      },
      {
        heading: 'WHAT YOUR STAFF MAY SPEND',
        text: `Your staff may spend ${v.cap}% of the treasury in a month on what it builds. ${shown(v.left).toLocaleString('en-US')} is left of this month's allowance${v.spent > 0 ? `, and ${shown(v.spent).toLocaleString('en-US')} has been spent` : ''}.${(v as { capNext?: boolean }).capNext ? ' The new share takes hold from next month: this month\'s allowance was already drawn.' : ''}`,
        rows: [{
          nat: '', title: 'The share of the treasury', lines: [],
          buttons: v.caps.map((c) => ({ label: c === v!.cap ? `${c}% (NOW)` : `${c}%`, act: `cap:${c}`, ok: c !== v!.cap, tip: `Let the staff spend up to ${c}% of the treasury in a month.` })),
        }],
      },
    ],
  };
}

/**
 * Something pressed in one of those rooms. Says what came of it; the game then draws the room again. A raid fought out
 * as an air battle (battle-bridge.ts battleEnd; release 1.6) comes with 'fought', what the battle reports, paid in place
 * of the dice, and says its after card ('after').
 */
function act(action: string, region = -1, fought?: FoughtAir) {
  const s = game();
  const h0 = s.headlines.length;
  const part = action.split(':');
  let ok = false, note = '';
  // Aircraft seen leaving for their target when a raid is ordered: where from, where to, whose, and what flies.
  let fly: Flight | null = null;
  let flies: Flight[] = [];
  let open = '';
  const air0 = lastAirReport(s);
  const air = airAct(s, action, fought);
  if (air) {
    // The air war as its own game (the grand campaign): a raid is flown the moment it is ordered, by every group picked.
    ok = air.ok; note = air.note; open = air.ok ? air.open ?? '' : '';
    if (air.ok && air.report !== undefined) {
      const flown = raidsSince(s, air0, [s.turn]);
      flies = flown.flights;
      // What came of it, in the report's own words first, kept short enough to read while the raid is flown.
      const told = flown.notes.filter(Boolean).join(' ') || note;
      note = told.length > 190 ? `${told.slice(0, told.lastIndexOf('.', 190) + 1) || told.slice(0, 190)}` : told;
      // (Brian, 24 November 2026: a raid "didnt't tell me the results of the battle".) A raid his crews flew: the page
      // that comes next is its report (airpages.ts), which the 3D game opens once the aircraft have been seen to fly
      // it. A raid he flew himself has its after card in the air battle, and keeps the page it had; so does a raid
      // with no aircraft to be seen flying it, which has nothing to wait for (its report is in the month's raids).
      if (!fought && flies.length > 0 && airPage(s, `airreport:${air.report}`)) open = `airreport:${air.report}`;
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
    return { ok, note, fly, flies, open, ...(air.after ? { after: air.after } : {}), view: view() };
  }
  if (part[0] === 'probe') {
    // (Version 64) One corps feels out the line: the attack card is read again with what it found.
    const who = s.corps.find((c) => c.id === Number(part[1])), target = s.regions[Number(part[2])];
    // (Version 65) The card's chance before the probe, where it was a guess, to set against what it reads after.
    const force = (part[3] ?? '').split('.').map(Number).filter((n) => Number.isFinite(n));
    const chanceOf = (): { words: string; exact: boolean } => {
      try { const look = target && force.length ? attackOutlook(s, attackers(s, force, target.id), target) : null; return look ? { words: chanceWords(look), exact: look.exact } : { words: '', exact: true }; } catch { return { words: '', exact: true }; }
    };
    const before = chanceOf();
    const rep = who && target ? orderProbe(s, who, target) : null;
    ok = !!rep;
    if (rep) {
      const after = chanceOf();
      lastProbe = { turn: s.turn, region: target.id, name: rep.name, found: rep.found, wear: rep.wear, lost: rep.lost, was: before.exact ? '' : before.words, now: after.words };
      note = `${rep.name} has probed the line at ${target.name}: it found ${listOf(rep.found)}${rep.wear > 0 ? ', and wore their fieldworks down' : ''}. It cost ${rep.lost.toLocaleString('en')} men.`;
    } else note = 'The line cannot be probed now.';
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'fold') {
    // (Version 64) Two worn corps made into one: the corps that is emptied goes home as a cadre.
    const keep = Number(part[1]), gone = Number(part[2]);
    const a = s.corps.find((c) => c.id === keep), b = s.corps.find((c) => c.id === gone);
    const names = a && b ? [a.name, b.name] : null;
    ok = !!names && orderFold(s, keep, gone);
    note = ok && names ? `${names[1]} is folded into ${names[0]}. Its cadre waits in the arsenal.` : 'They cannot be folded together now.';
    return { ok, note, fly, flies, open: ok ? 'close' : open, view: view() };
  }
  if (part[0] === 'repair') {
    // (Damaged cities) One town repaired, or every one the treasury covers.
    if (part[1] === 'all') {
      let done: string[] = [];
      try { done = repairCalls.all?.(s) ?? []; } catch { done = []; }
      ok = done.length > 0;
      note = ok ? `Repaired: ${listNames(done)}.` : 'Nothing could be repaired.';
    } else {
      const at = Number(part[1]);
      const v = repairView(s, at);
      try { ok = !!repairCalls.order?.(s, at); } catch { ok = false; }
      note = ok ? `${v?.town ?? 'The town'} is repaired: it pays in full again from this month.` : v?.why ?? 'It cannot be repaired now.';
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
  if (part[0] === 'men') {
    // (Version 62) The player's say over the call-up: the next class early (asked twice), or the standing call-up held or let run.
    if (part[1] === 'early') {
      const c = callUpOf(s);
      const why = c ? c.early.why : 'The reserve of men is counted in the grand campaign.';
      const asking = `${action}:${s.turn}`;
      if (!why && c && cabinetAsked !== asking) {
        cabinetAsked = asking;
        return { ok: false, note: `Call up the next class early? About ${Math.round(c.early.men).toLocaleString('en')} men join the reserve at once, ${c.early.months === 1 ? 'a month' : `${c.early.months} months`} of classes together, and no class then comes of age for ${c.early.gap === 1 ? 'a month' : `${c.early.gap} months`}. They are the same men, sooner, and it cannot be undone. Press it again to call them up.`, fly, flies, open, view: view() };
      }
      cabinetAsked = '';
      ok = !why && orderEarlyClass(s);
      note = ok ? 'The next class is called up early.' : why ?? 'They cannot be called up now.';
    } else if (part[1] === 'hold') {
      const hold = part[2] === '1';
      ok = orderCallUp(s, hold);
      note = ok ? (hold ? 'The standing call-up is held: no new corps are raised from the reserve until you let it run again.' : 'The standing call-up runs again.') : 'That cannot be changed now.';
    }
    return { ok, note, fly, flies, open, view: view() };
  }
  if (action === 'open:crossings') return { ok: true, note: '', fly, flies, open: 'crossings', view: view() };
  if (part[0] === 'crossing') {
    // (Version 72) One crossing named as the plan, or the plan dropped.
    try { ok = !!seaCalls.plan && (part[1] === 'none' ? seaCalls.plan(s, null, null) : seaCalls.plan(s, Number(part[1]), Number(part[2]))); } catch { ok = false; }
    note = ok ? (part[1] === 'none' ? 'The plan is dropped.' : `The crossing to ${s.regions[Number(part[2])]?.name ?? 'that shore'} is your plan. Staffs you have handed the navy or the air war now prepare it.`) : 'It cannot be changed now.';
    return { ok, note, fly, flies, open: 'crossings', view: view() };
  }
  if (part[0] === 'history') {
    // (Version 70) The history dial turned to another stop.
    const stop = Number(part[1]), to = dialStops().find((x) => x.stop === stop);
    let why: string | null = dialStops().length ? 'There is no such stop on the dial.' : 'These rules have no history dial.';
    try { why = dialCalls.whyNot && to ? dialCalls.whyNot(s, stop) : why; } catch { /* as it was */ }
    try { ok = !why && !!dialCalls.order && dialCalls.order(s, stop); } catch { ok = false; }
    note = ok ? `The history dial stands at ${to?.name ?? stop} now. The other countries feel it from next month.` : why ?? 'The dial cannot be turned now.';
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'depth' || part[0] === 'desk' || part[0] === 'cap') {
    // (Version 66) How deep the player plays: a depth, one desk handed over or taken back, or what the staff may spend.
    if (part[0] === 'depth') {
      const to = DEPTH.depths.find((d) => d.id === part[1]);
      // A depth with no cabinet, picked while the cabinet has business open, is asked twice: the bill is shown first.
      let bill: { text: string; trust: number }[] = [];
      try { bill = to && !to.cabinet && !cabinetIsOff(s) ? cabinetBillOf(s) : []; } catch { bill = []; }
      const asking = `${action}:${s.turn}`;
      if (to && bill.length && cabinetAsked !== asking) {
        cabinetAsked = asking;
        const sum = Math.round(bill.reduce((t, b) => t + b.trust, 0) * 10) / 10;
        return { ok: false, note: `${to.name} has no war cabinet. Its open business is settled first: ${bill.map((b) => `${b.text} (trust \u2212${b.trust})`).join('; ')}. In all its trust falls by ${sum}, and it is kept at that should you want it back. Press it again to go to ${to.name}.`, fly, flies, open, view: view() };
      }
      cabinetAsked = '';
      let stands = false;
      try { stands = commandOf(s)?.depth === part[1]; } catch { stands = false; }
      ok = !stands && orderDepth(s, part[1]);
      note = ok ? `You play at ${to?.name ?? 'that depth'} now.${to && to.staff.length ? ` Your staff runs ${to.staff.map((k) => lowFirst(deskDef(k)?.name ?? k)).join(', ')}.` : ' Everything is on your own desk.'}` : 'The campaign stands at that depth already.';
    } else if (part[0] === 'desk') {
      const def = deskDef(part[1]), staff = part[2] === 'staff';
      ok = !!def && orderDesk(s, part[1] as DeskId, staff);
      note = !def ? '' : ok ? (staff ? `${runsLine(def.lead, def.name).replace(/\.$/, '')} from now on.` : `${def.name} is on your own desk again. What your staff ordered this month stands.`)
        : (staff ? `Your staff runs ${lowFirst(def.name)} already.` : `${def.name} is on your own desk already.`);
    } else {
      ok = orderStaffCap(s, Number(part[1]));
      note = ok ? `Your staff may spend up to ${Number(part[1])}% of the treasury in a month.` : '';
    }
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'breakuprule') {
    // (The 1.5 patch, breakup.ts) Breaking corps up, switched by the player in any month.
    setBreakUp(s, part[1] === 'on');
    ok = true;
    note = breakUpOn(s)
      ? 'Breaking corps up is on: pick divisions on a corps\' card in the tray and SPLIT OFF makes them a corps of their own; REJOIN puts them back.'
      : 'Breaking corps up is off: a corps moves and fights whole.';
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'fleetattackrule') {
    // (The 1.5.1 patch, fleetattack.ts) Fleets sent straight at the enemy, switched by the player in any month.
    setFleetAttack(s, part[1] === 'on');
    ok = true;
    note = fleetAttackOn(s)
      ? 'Fleet attacks are on: drop one of your fleets on an enemy fleet in reach, and the battle is fought at once, at any odds.'
      : 'Fleet attacks are off: fleets meet only at the month\'s end, where an admiral thinks the odds good enough.';
    return { ok, note, fly, flies, open, view: view() };
  }
  if (part[0] === 'breakup') {
    // (Release 2, breakup.ts; Brian 24 November 2026 18:02Z) 'breakup:<corps>:<slot,slot>': the divisions picked on the
    // corps' card, by their place in it from 0, split off as a corps of their own where it stands.
    const c = s.corps.find((x) => x.id === Number(part[1]) && !x.dead);
    const slots = (part[2] ?? '').split(',').filter((x) => x !== '').map(Number);
    const why = whyNotBreakUp(s, c, slots);
    const made = !why && c ? breakCorps(s, c, slots) : null;
    ok = !!made;
    note = made && c ? `${made.name} ${made.divs === 1 ? 'is' : 'are'} split off from ${c.name}, and stand${made.divs === 1 ? 's' : ''} at ${s.regions[c.reg]?.name ?? 'the same place'} as a corps of ${made.divs === 1 ? 'its' : 'their'} own.` : why ?? 'Those divisions cannot be split off now.';
    return { ok, note, fly, flies: [], open: '', made: made?.id ?? -1, view: view() };
  }
  if (part[0] === 'rejoin') {
    // (The 1.5 patch, breakup.ts) 'rejoin:<part>:<into>': a part goes back into the corps it was split from, or another part of it.
    const a = s.corps.find((x) => x.id === Number(part[1]) && !x.dead), b = s.corps.find((x) => x.id === Number(part[2]) && !x.dead);
    const why = whyNotRejoin(s, a, b);
    const name = a?.name ?? '';
    ok = !why && !!a && !!b && rejoinCorps(s, a, b);
    note = ok && b ? `${name} rejoins ${b.name}: ${b.divs} division${b.divs === 1 ? '' : 's'} together again.` : why ?? 'They cannot be put back together now.';
    return { ok, note, fly, flies: [], open: '', joined: ok ? Number(part[2]) : -1, view: view() };
  }
  if (part[0] === 'cabinet') {
    // The government (Version 57): a minister appointed, a decree, or the directive set aside.
    const v = governmentOf(s);
    if (part[1] === 'tab') {
      cabinetTab = part[2] === 'table' || part[2] === 'decrees' || part[2] === 'minutes' ? part[2] : 'now';
      return { ok: true, note: '', fly, flies, open, view: view() };
    }
    if (part[1] === 'office') {
      // (Version 62) Whether the player's government can fall: a setting of its own beside the cabinet's switch.
      const canFall = part[2] === 'fall';
      ok = orderOffice(s, canFall);
      note = ok ? (canFall ? 'Your office is no longer secure: a cabinet left with no trust in you for months running will fall.' : 'Your office is secure: the cabinet still doubts and withholds, but it never falls.') : (canFall ? 'Your office can be brought down already.' : 'Your office is secure already.');
      return { ok, note, fly, flies, open, view: view() };
    }
    if (part[1] === 'switch') {
      // The cabinet switched off or on (Version 59): it takes hold at once, in any month.
      const on = part[2] === 'on';
      if (!on) {
        // (Version 62) What stands open is settled first, each at its usual price: the bill is shown, and it is asked twice.
        let bill: { text: string; trust: number }[] = [];
        try { bill = cabinetIsOff(s) ? [] : cabinetBillOf(s); } catch { bill = []; }
        const asking = `${action}:${s.turn}`;
        if (bill.length && cabinetAsked !== asking) {
          cabinetAsked = asking;
          const sum = Math.round(bill.reduce((t, b) => t + b.trust, 0) * 10) / 10;
          return { ok: false, note: `Switch the war cabinet off? Its open business is settled first: ${bill.map((b) => `${b.text} (trust \u2212${b.trust})`).join('; ')}. In all its trust falls by ${sum}, and it is kept at that should you want it back. Press it again to switch it off.`, fly, flies, open, view: view() };
        }
        cabinetAsked = '';
      }
      ok = orderCabinet(s, on);
      note = ok ? (on ? 'The war cabinet sits again, as it was left. It will set its next directive in a few months.' : 'The war cabinet is switched off. Switch it on again from MENU, OPTIONS.') : (on ? 'The war cabinet is sitting already.' : 'The war cabinet is switched off already.');
      return { ok, note, fly, flies, open, view: view() };
    }
    if (part[1] === 'promise') {
      // Call your shot (Version 60): the player's word that a town will be his side's within so many months. Asked twice.
      const region = Number(part[2]), months = Number(part[3]);
      const why = whyNotOrderPromise(s, region, months);
      const terms = why ? null : promiseTermsOf(s, region, months);
      const asking = `${action}:${s.turn}`;
      if (!why && terms && cabinetAsked !== asking) {
        cabinetAsked = asking;
        return { ok: false, note: `Promise ${terms.town} to the cabinet within ${months} months? ${terms.bandName}: kept, ${terms.reward}; broken, its trust falls ${terms.price}. Press it again to give your word.`, fly, flies, open, view: view() };
      }
      cabinetAsked = '';
      ok = !why && orderPromise(s, region, months);
      note = ok ? `Your word is given: ${terms?.town ?? 'the town'} by the end of ${terms ? turnToDate(terms.due).label : 'the time named'}.` : why ?? 'That promise cannot be made now.';
      return { ok, note, fly, flies, open, view: view() };
    }
    if (part[1] === 'takeback') {
      const why = whyNotTakeBackPromise(s);
      ok = !why && orderTakeBack(s);
      note = ok ? 'You have taken your word back. The cabinet makes a note of it.' : why ?? 'There is no promise to take back.';
      return { ok, note, fly, flies, open, view: view() };
    }
    if (part[1] === 'sack' || part[1] === 'standby') {
      // The cabinet wants a head (Version 60): dismiss the general it names, or stand by him. Dismissal is asked twice.
      const name = (v as { head?: { name: string } | null } | null)?.head?.name ?? 'the general';
      if (part[1] === 'sack') {
        const asking = `${action}:${s.turn}`;
        if (cabinetAsked !== asking) {
          cabinetAsked = asking;
          return { ok: false, note: `Dismiss ${name}? He leaves the army for good. Press it again to do it.`, fly, flies, open, view: view() };
        }
        cabinetAsked = '';
        ok = orderSack(s, Number(part[2]));
        note = ok ? `${name} is dismissed. The cabinet has its head.` : 'He cannot be dismissed now.';
      } else {
        const why = whyNotStandByGeneral(s);
        ok = !why && orderStandBy(s);
        note = ok ? `You stand by ${name}. His next battle settles it.` : why ?? 'The cabinet is asking for nobody.';
      }
      return { ok, note, fly, flies, open, view: view() };
    }
    // (an appointment or a decree that costs political capital is asked twice: the first press says what it spends)
    const spends = part[1] === 'appoint' ? (v?.appointCost ?? 0) : part[1] === 'decree' ? ((): number => { const t = v?.tracks.find((x) => x.id === part[2]); const i = Number(part[3]); return t && i > t.at ? t.steps[i]?.cost ?? 0 : 0; })() : 0;
    const refused = part[1] === 'appoint' ? whyNotAppointMinister(s, part[2], part[3]) : part[1] === 'decree' ? whyNotOrderDecree(s, part[2], Number(part[3])) : 'x';
    const asking = `${action}:${s.turn}`;
    if (spends > 0 && !refused && cabinetAsked !== asking) {
      cabinetAsked = asking;
      return { ok: false, note: `That spends ${spends} political capital of the ${Math.floor(v?.capital ?? 0)} you hold. Press it again to carry it.`, fly, flies, open, view: view() };
    }
    cabinetAsked = '';
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
    if (ok && f && t >= 0) { const p = m === 'strike' ? s.seas[t] : s.regions[t]; fly = { from: f.base, x: p.x, y: p.y, nat: f.nat, bombers: m !== 'superiority' && f.bmr >= 1, escort: m !== 'night' && f.ftr >= 1, region: m === 'strike' ? -1 : t, meetFrom: -1, night: m === 'night', ...meets }; }
  } else if (part[0] === 'airbase') {
    ok = orderAirBase(s, Number(part[1]), Number(part[2]));
    note = ok ? `The group moves to ${s.regions[Number(part[2])]?.name ?? 'its new airfield'}.` : 'It cannot move there now.';
  } else if (part[0] === 'fleet') {
    ok = orderFleet(s, Number(part[1]), Number(part[2]), part[3] as Stance);
    note = ok ? `${fleetView(s, Number(part[1]))?.doing ?? 'The order is given'}.` : fleetWhyId(s, Number(part[1])) || 'That order cannot be given now.';
    return { ok, note, fly, flies: [], open: `fleet:${part[1]}`, view: view() };
  } else if (part[0] === 'fleetstaff') {
    ok = handBackFleet(s, Number(part[1]));
    note = ok ? `${fleetView(s, Number(part[1]))?.name ?? 'The fleet'} is your admirals' to sail again from next month.` : 'It cannot be handed back now.';
    return { ok, note, fly, flies: [], open: `fleet:${part[1]}`, view: view() };
  } else if (part[0] === 'fleetescort') {
    ok = orderEscort(s, Number(part[1]), part.slice(2).join(':'));
    note = ok ? `${fleetView(s, Number(part[1]))?.doing ?? 'The fleet is bound to the lane'}.` : fleetWhyId(s, Number(part[1])) || 'It cannot escort that lane now.';
    return { ok, note, fly, flies: [], open: `fleet:${part[1]}`, view: view() };
  } else if (part[0] === 'fleetshell') {
    const rep2 = orderBombard(s, Number(part[1]), Number(part[2]), part[3] as 'troops' | 'city');
    ok = !!rep2;
    note = rep2 ? rep2.result : previewBombard(s, Number(part[1]), Number(part[2]), part[3] as 'troops' | 'city')?.why ?? 'The coast cannot be shelled now.';
    // (the map shows the guns falling on the coast)
    return { ok, note, fly, flies: [], open: '', shell: ok ? Number(part[2]) : -1, view: view() };
  } else if (part[0] === 'fleetattack') {
    // (The 1.5.1 patch, fleetattack.ts) The fleet sent straight at an enemy fleet: fought at once. The map shows the guns over
    // the water and the ships that went down, as it does for the month's fights at sea.
    const sea0 = lastSeaReport(s);
    const out = orderFleetAttackNow(s, Number(part[1]), Number(part[2]));
    ok = !!out;
    note = out ? out.text : previewFleetAttack(s, Number(part[1]), Number(part[2]))?.why ?? 'That fleet cannot be attacked now.';
    const fought = ok ? seaFightsSince(s, sea0) : null;
    const zone = out?.report?.zone ?? (out ? s.fleets.find((f) => f.id === Number(part[1]))?.zone ?? -1 : -1);
    return { ok, note, fly, flies: [], open: '', seaFight: ok ? zone : -1, outcome: out?.outcome ?? '', sunk: fought?.sunk ?? [], view: view() };
  } else if (part[0] === 'fleetsplit') {
    const made = orderSplit(s, Number(part[1]), { [part[2]]: Number(part[3]) } as Record<string, number>);
    ok = made >= 0;
    note = ok ? `${fleetView(s, made)?.name ?? 'A new fleet'} is formed from ${fleetView(s, Number(part[1]))?.name ?? 'the fleet'}.` : whyNotSplit(s, Number(part[1]), { [part[2]]: Number(part[3]) } as Record<string, number>) ?? 'Those ships cannot be split off now.';
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
function abilityLines(nat: string, s?: Game): string[] {
  const out = traitsOf(nat).map((t) => `${t.name}. ${traitRule(t, BUDGET.scale)}`);
  const u = eliteOf(nat);
  if (u) out.push(`Special formation, from the arsenal: ${u.label}. ${eliteRule(u)}`);
  // (Rules 84.2.6.) A dominion: how often war on it brings its mother country in, the browser's own words. In the open
  // campaign outside World Domination only, as the roll itself.
  const patron = s && isOpen(s) && !s.flags.dom ? patronOf(nat) : null, odds = patron ? Math.round(answersOf(patron) * 10) : 0;
  const TIMES = ['never', 'once', 'twice', 'three times', 'four times', 'five times', 'six times', 'seven times', 'eight times', 'nine times'];
  if (s && patron && odds > 0 && s.nations[patron]) out.push(`Dominion. War on it brings ${newsName(s.nations[patron])} in ${odds >= 10 ? 'every time' : `${TIMES[odds]} in ten`}, the same month.`);
  return out;
}

/** The abilities of the player's nation, for the page that says how to play. They count in the grand campaign only. */
function abilities() {
  const s = game();
  const me = s.human ? s.nations[s.human] : null;
  return { nation: me?.name ?? '', active: traitRules(s), lines: me ? abilityLines(me.id, s) : [] };
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
      // (Version 65) Its general staff as the player knows it: its character, what it is doing in a sentence, and
      // whether a main effort of its is aimed at the player or his allies, with the place it is aimed at.
      staff: (() => {
        try {
          const v = rivalStaff(s, n.id);
          if (!v) return null;
          const hot = v.efforts.find((e) => e.atYou && !e.won) ?? null;
          return { label: v.label, line: v.line, says: v.says, threat: !!hot, target: hot ? hot.target : -1, targetName: hot ? hot.targetName : '' };
        } catch { return null; }
      })(),
    }));
  const rows = all.filter((n) => n.alive && !n.atWar.length && !n.league && n.id !== me.id).sort((a, b) => a.opinion - b.opinion).map((n) => {
    const open = (mine === 'axis' || mine === 'allies') && courtable(n);
    const c = open ? canCourt(s, me.id, n.id) : { ok: false, why: '' };
    const k = open ? receptive(n, mine as 'axis' | 'allies') : 0;
    // (Rules 84.2.6: the mother country named first, as the declare card has it.)
    const mother = motherOf(s, n.id);
    const frs = [...(mother ? [newsName(s.nations[mother])] : []), ...friendsOf(n.id).filter((f) => f.nat !== mother && s.nations[f.nat]?.alive).map((f) => theName(s, f.nat))];
    return {
      nat: n.id, name: n.name, label: TRACK_LABEL[n.align], step: step(n), opinion: Math.round(n.opinion),
      divs: fieldArmy(s, n.id),
      court: open && up, canCourt: up && c.ok, why: open && up && !c.ok ? c.why : '',
      word: !open ? '' : k >= 1.3 ? 'Receptive' : k >= 0.8 ? 'Open to offers' : k >= 0.5 ? 'Wary' : 'Very hard to move',
      friends: frs.length ? `Friends: ${listOf(frs)}` : '',
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
    // (Version 70) What one country claims of another's ground, at the dial's two loosest stops: those on the player first.
    claims: claimsPane(s),
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
      note: `${upFirst(theName(s, me.id))} has no design offices of its own and buys its weapons abroad. The weapons of every great power improve as the war goes on, each new design arriving in the month history brought it into service.`,
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
      // (The research tree's chart.) Its year and its months in its own time; every need, met or not; and its tag.
      year: n.year, base: n.base,
      all: n.needs.map((x) => ({ id: x.id, name: x.name, done: x.done })),
      tag: ((raw) => (raw?.unlock?.length ? 'NEW UNIT' : raw?.slots ? `+${raw.slots} TEAM${raw.slots === 1 ? '' : 'S'}` : ''))(TECH_TREE.techs.find((x) => x.id === n.id) as { unlock?: string[]; slots?: number } | undefined),
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

// ---- FIRST COMMAND: the first hour's ten staff notes (rules 84.3.3) -------------------------------------------
// The lessons, their words and the tests of their deeds are the shared rules' (src/campaign/lessons.ts, read from
// data/campaign/lessons.json): the game keeps nothing of its own but where the note is drawn. The notebook (which
// lessons are done, put away or shown) is the player's, not the campaign's: the game hands it in from its own small
// file beside the options with TC.notebook(kept), and writes it back whenever an answer carries `book`.
//
// Deeds are read across the player's own orders (the handoff, first-hour/HANDOFF-3D.md, section 3): what a march, a
// train, an attack, a storm, a raid or an arsenal order brought about is read off the war as the call returns, as the
// browser reads it off the page between two drawings. Nothing his staff does is read: a desk or a depth handed over
// in the middle of his month (where the staff works at once, trap 4) and the other nations' moves at the month's end
// (trap 2) go through calls that read no deeds. The two deeds only the screen sees, the month card put down ('card')
// and a line of it followed ('line'), the game reports by name with TC.lessonDeed.

/**
 * What only the screen can see: whether the month card is open, the corps of the army in hand, and whether the page a
 * lesson sends the player to is open (the ARSENAL, lesson 10's: its note then says its 'there' words).
 */
type LessonMoment = { card?: boolean; inHand?: number[]; open?: boolean };

let lessonBook: Notebook = readNotebook(null);
/** The notebook changed since the game last wrote it to the player's file. */
let lessonBookChanged = false;
/** Lessons stamped DONE since the game last asked, in order: the note it shows takes the stamp. */
let lessonStamps: string[] = [];
/** The tip on screen, the month it came in, and the orders given since: it goes at the next order or the month's end. */
let lessonTip: { id: string; turn: number; acts: number } | null = null;
/** How many of his lines were cut through no guns when his month was last open (-1: not yet counted in this campaign). */
let lessonCuts = -1;

/** A campaign begun or read in: nothing of the last one's notes stands on the desk. */
function lessonsBegin(first: boolean): void {
  lessonStamps = []; lessonTip = null; lessonCuts = -1;
  if (first) {
    if (allDone(lessonBook)) lessonsAgain(lessonBook);
    lessonBook.on = true;
    lessonBookChanged = true;
  }
}

/** True while it is his own month to give orders in, as the notes read it (no eve of war, nobody else moving). */
const lessonUp = (s: Game): boolean => !!s.human && humanUp(s) && !onEve(s);

/** Stamps the lesson a deed earns, if its turn has come. */
function lessonDid(s: Game, deed: Deed, m: LessonMoment = {}): void {
  const got = lessonDeed(s, lessonBook, deed, m);
  if (!got.length) return;
  lessonBookChanged = true;
  for (const l of got) if (!lessonStamps.includes(l.id)) lessonStamps.push(l.id);
}

/** A tip is shown once, the first time its moment comes; the march's only once the march lesson is done. */
function lessonTipDue(s: Game, id: string): void {
  if (lessonBook.tips.includes(id) || !lessonsOn(s, lessonBook) || !TIPS.some((t) => t.id === id)) return;
  if (id === 'kept' && !isDone(lessonBook, 'march')) return;
  lessonBook.tips.push(id);
  lessonBookChanged = true;
  lessonTip = { id, turn: s.turn, acts: 0 };
}

/** His lines cut by bombers where no guns stood: the moment of the tip that says so. */
const lessonCutCount = (s: Game): number => (s.human ? s.regions.filter((r) => r.ctrl === s.human && cutThroughGuns(s, r.id) === 0).length : 0);

/** The war as an order of his finds it, to read his deeds against once it is carried out. */
interface DeedMark { turn: number; reports: number; air: number; queue: Set<number>; kept: Set<string>; hand: { id: number; reg: number }[] }
const keptMarches = (s: Game): Set<string> => new Set(Object.keys(s.flags).filter((k) => k.startsWith('march:')));
function deedMark(s: Game, ids: number[]): DeedMark | null {
  const me = s.human;
  if (!me || !lessonUp(s) || !lessonsOn(s, lessonBook)) return null;
  return {
    turn: s.turn, reports: s.reports.length, air: s.airReports.length, queue: new Set((s.nations[me]?.queue ?? []).map((b) => b.id)), kept: keptMarches(s),
    hand: ids.map((id) => s.corps.find((c) => c.id === id && !c.dead)).filter((c): c is Corps => !!c).map((c) => ({ id: c.id, reg: c.reg })),
  };
}
/** What his order brought about, read as the browser reads it between two drawings (firsthour.ts, readDeeds). */
function deedsSince(was: DeedMark, how: 'march' | 'rail' | ''): void {
  const s = game();
  const me = s.human;
  if (!me || s.turn !== was.turn || !lessonUp(s)) return;
  const fought = s.reports.slice(was.reports).filter((r) => r.attNat === me);
  if (fought.some((r) => r.town)) lessonDid(s, 'siege');
  if (fought.length) lessonDid(s, 'attack');
  const flew = s.airReports.slice(was.air);
  if (flew.some((r) => r.attNat === me && BOMBING.includes(r.mission))) lessonDid(s, 'raid');
  const queue = s.nations[me]?.queue ?? [];
  const ordered = queue.some((b) => !was.queue.has(b.id));
  if (ordered) lessonDid(s, 'order');
  // A march kept from month to month (trap 3: even one whose first leg rides the railway is a march), or the army
  // in hand moved: by a march, or by the train it was ordered onto.
  const kept = [...keptMarches(s)].filter((k) => !was.kept.has(k));
  const moved = was.hand.some((h) => { const c = s.corps.find((x) => x.id === h.id); return !!c && !c.dead && c.reg !== h.reg; });
  // (Any order he gives puts away a tip that came before it.)
  if (lessonTip && (fought.length || moved || kept.length || flew.length || ordered || queue.length !== was.queue.size)) lessonTip.acts++;
  if (fought.length) return;
  if (kept.length) lessonTipDue(s, 'kept');
  if (kept.length || (moved && how === 'march')) lessonDid(s, 'march');
  if (moved && how === 'rail' && !kept.length) lessonDid(s, 'rail');
}

/**
 * One of the player's own orders, with its deeds read. `how` says what moving the army in hand means (a march or a
 * train); `counts` leaves out an order that is not his own doing (a desk or a depth handed to the staff).
 */
function playerOrder<A extends unknown[], R>(fn: (...a: A) => R, how: 'march' | 'rail' | '' = '', counts: (...a: A) => boolean = () => true): (...a: A) => R {
  return (...a: A): R => {
    let was: DeedMark | null = null;
    try { was = g && counts(...a) ? deedMark(g, how && Array.isArray(a[0]) ? (a[0] as number[]) : []) : null; } catch { was = null; }
    const out = fn(...a);
    if (was) { try { deedsSince(was, how); } catch { /* (the notes never stop an order) */ } }
    return out;
  };
}
/** The calls of the page actions that are the staff's work, not his: a depth, a desk or the staff's purse. */
const staffAct = (action: unknown): boolean => typeof action === 'string' && /^(depth|desk|cap):/.test(action);

/** The month handed over: a siege of his standing as he hands it over earns lesson 7, and the month's end lesson 5. */
function endMonthWithLessons() {
  const s = game();
  const up = lessonUp(s) && lessonsOn(s, lessonBook);
  let siegeUp = false;
  try { siegeUp = up && !isDone(lessonBook, 'city') && siegeOf(s) !== null; } catch { siegeUp = false; }
  const t0 = s.turn;
  const out = endMonth();
  const t = game();
  if (up && t.turn > t0) {
    if (siegeUp) lessonDid(t, 'siege');
    lessonDid(t, 'end');
  }
  if (lessonTip && lessonTip.turn < t.turn) lessonTip = null;
  return out;
}

/** The 3D game's own words for a control, where they are not the list's: lessons.json `names.unreal`. */
const LESSON_GAME = 'unreal';
/**
 * (Ruling b425, the design director, 18 November 2026.) The 3D game's swaps, lent here until lessons.json carries
 * them all as names.unreal: YOUR STAFF, opened from the red line under the date, where the browser has a STAFF tab; the
 * raid planned from the aircraft's own airfield; the month card's lines followed by their pins; the GUIDE on the
 * top bar. Each keeps its lesson inside its word limit (lesson 9 stays at 36).
 */
const LESSON_NAMES_3D: [string, string][] = [
  ['Open STAFF and take the air war into your own hands. In the planner choose bombers, a raid and a target.', 'In YOUR STAFF, take the air war yourself. Click your aircraft on the map: choose bombers, a raid and a target.'],
  ['STAFF hands you each desk when you want it', 'YOUR STAFF hands you each desk when you want it'],
  ['Click a line to go to its place', "Click a line's pin to go to its place"],
  ['Click one line of the card.', 'Click the pin on one line of the card.'],
  ['in the Guide', 'in the GUIDE'],
];
// (Where the list already carries names.unreal, only the pairs it lacks are added. The closing note's YOUR STAFF comes
// with the list itself, beside ruling b429's cut that keeps that note at 29: lent here, it would make the older note 31.)
try {
  const have = LESSON_NAMES[LESSON_GAME] ?? (LESSON_NAMES[LESSON_GAME] = []);
  for (const pair of LESSON_NAMES_3D) if (!have.some(([was]) => was === pair[0])) have.push(pair);
} catch { /* (the list's own words, then) */ }

/**
 * A lesson as the screen shows it: its number, its words with the campaign's places filled in, and its deed line.
 * `open`: the page it sends the player to is open, and a lesson with words for that says them (lesson 10 in the
 * ARSENAL asks for the artillery corps: a note never asks for what is already done, the UI director, build 1640).
 */
function lessonShown(s: Game, l: Lesson, open = false) {
  const w = lessonWords(s, l, LESSON_GAME, '', open);
  return { kind: 'lesson', id: l.id, n: l.n, of: LESSONS.length, head: w.head, text: w.text, deed: w.deed, does: l.does, due: l.due };
}

/** The army lesson 4 points at when none is in hand: one of his with a move left beside the enemy (the browser's flag). */
function lessonFoeFlag(s: Game, inHand: number[]): number {
  const me = s.human;
  if (!me) return -1;
  const beside = s.regions.find((r) => r.units.some((c) => c.nat === me && c.ap > 0 && !c.dead) && r.nb.some((k) => s.regions[k].units.some((u) => s.nations[me].atWar.includes(u.nat))));
  if (beside) return beside.id;
  const held = s.corps.find((c) => inHand.includes(c.id) && !c.dead);
  return held ? held.reg : -1;
}

/**
 * The note on the desk now, for the moment the screen describes: the lesson due, a tip, or the closing note; lesson
 * 1 laid on the month card while it is open; the lessons stamped since last asked; and the notebook when it changed.
 * The army in hand earns lesson 2 (its deed is picking one up). `region` is where the pointer stands on the map for
 * a lesson that points at a flag: the army to pick up, one beside the enemy, the province under siege.
 */
function lessonNow(moment: LessonMoment = {}) {
  const s = game();
  const m: Moment = { card: !!moment.card, inHand: (moment.inHand ?? []).filter((id) => Number.isFinite(id)) };
  const on = lessonsOn(s, lessonBook), up = lessonUp(s);
  let note: Record<string, unknown> | null = null;
  let laid: Record<string, unknown> | null = null;
  if (on && up) {
    if (m.inHand!.length && s.corps.some((c) => m.inHand!.includes(c.id) && !c.dead && c.nat === s.human && canOrder(s, c))) lessonDid(s, 'pick', m);
    if (!lessonBook.tips.includes('cut')) {
      const cuts = lessonCutCount(s);
      if (lessonCuts >= 0 && cuts > lessonCuts) lessonTipDue(s, 'cut');
      lessonCuts = cuts;
    }
    const l = currentLesson(s, lessonBook, m);
    if (lessonTip && !(lessonTip.acts < 1 && lessonTip.turn === s.turn)) lessonTip = null;
    // (A lesson that comes only with the month card goes before a tip, which waits until the card is put down.)
    if (lessonTip && !(l?.due === 'card' && m.card)) {
      const t = TIPS.find((x) => x.id === lessonTip!.id);
      if (t) note = { kind: 'tip', id: t.id, n: 0, of: LESSONS.length, head: '', text: namedWords(t.text, LESSON_GAME), deed: '', does: '', due: t.when };
    } else if (l && l.id === 'month') {
      // (Lesson 1 lies on the month card itself and nowhere else.)
      laid = lessonShown(s, l);
    } else if (l) {
      note = lessonShown(s, l, !!moment.open);
      const at = l.id === 'pickup' ? pickupPlace(s) : l.id === 'attack' ? lessonFoeFlag(s, m.inHand!) : l.id === 'city' ? siegeOf(s)?.region ?? -1 : -1;
      note.region = at;
    } else if (closingOwed(s, lessonBook)) {
      const w = lessonWords(s, { ...LESSONS[0], text: CLOSING.text, deed: '' }, LESSON_GAME);
      note = { kind: 'closing', id: CLOSING.id, n: 0, of: LESSONS.length, head: CLOSING.head, text: w.text, deed: '', does: '', due: '' };
    }
  }
  const stamped = lessonStamps;
  lessonStamps = [];
  const out = { on, first: firstCommand(s), note, laid, stamped, waiting: waitingCount(lessonBook), done: doneCount(lessonBook), ...(lessonBookChanged ? { book: lessonBook } : {}) };
  lessonBookChanged = false;
  return out;
}

/** A deed only the screen sees, reported by name: 'card' (the month card lesson 1 lay on, put down) or 'line' (a line of the month card followed). */
function lessonDeedOf(deed: string, moment: LessonMoment = {}) {
  const s = game();
  if ((deed === 'card' || deed === 'line') && lessonUp(s)) lessonDid(s, deed, { card: !!moment.card, inHand: moment.inHand ?? [] });
  return lessonNow(moment);
}

/**
 * What the player does with the notes: 'later' puts the note away (it waits in the Guide), 'back:<id>' brings one
 * back, 'again:<id>' arms a lesson done for its next moment (SHOW ME AGAIN), 'all' the ten from the first, 'on' and
 * 'off' the setting, 'close' the closing note, and 'tip' puts the tip away. Answers as lessonNow does.
 */
function lessonAct(action: string, moment: LessonMoment = {}) {
  const s = game();
  const [what, id = ''] = action.split(':');
  if (what === 'later') { const l = lessonsOn(s, lessonBook) ? currentLesson(s, lessonBook, { card: !!moment.card, inHand: moment.inHand ?? [] }) : null; if (l && l.id !== 'month') putAway(lessonBook, l.id); }
  else if (what === 'back') bringBack(lessonBook, id);
  else if (what === 'again') showAgain(lessonBook, id);
  else if (what === 'all') lessonsAgain(lessonBook);
  else if (what === 'on' || what === 'off') lessonBook.on = what === 'on';
  else if (what === 'close') { if (!lessonBook.done.includes(CLOSING.id)) lessonBook.done.push(CLOSING.id); }
  else if (what === 'tip') { lessonTip = null; return lessonNow(moment); }
  else return lessonNow(moment);
  lessonBookChanged = true;
  return lessonNow(moment);
}

/** The notebook from the player's file (any shape: what is missing is a new player's), or as it stands. */
function lessonNotebook(kept?: unknown) {
  if (kept !== undefined) { lessonBook = readNotebook(kept); lessonBookChanged = false; }
  return { book: lessonBook, door: { ...LESSON_DOOR, done: allDone(lessonBook), line: allDone(lessonBook) ? LESSON_DOOR.again : LESSON_DOOR.fresh }, campaign: FIRST_COMMAND, setting: LESSON_SETTING };
}

const WORD_COUNT = ['None', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'];
/** The Guide's first page: the ten lessons, each done, waiting or to come, under its month in FIRST COMMAND. */
function lessonGuide(moment: LessonMoment = {}) {
  const s = g;
  const first = !!s && firstCommand(s);
  let cur = '';
  try { cur = s && lessonsOn(s, lessonBook) ? currentLesson(s, lessonBook, { card: !!moment.card, inHand: moment.inHand ?? [] })?.id ?? '' : ''; } catch { cur = ''; }
  const waiting = LESSONS.filter((l) => !isDone(lessonBook, l.id) && (lessonBook.later.includes(l.id) || l.id === cur));
  const done = doneCount(lessonBook);
  const state = `${done ? `${WORD_COUNT[done]} done.` : 'None done yet.'}${waiting.length ? ` ${waiting.length === 1 ? 'One is' : `${WORD_COUNT[waiting.length]} are`} waiting for you.` : ''}`;
  let lit = false;
  const row = (l: Lesson) => {
    if (isDone(lessonBook, l.id)) return { id: l.id, n: l.n, head: l.head, state: 'done', lit: false };
    if (waiting.includes(l)) { const glow = !lit; lit = true; return { id: l.id, n: l.n, head: l.head, state: 'now', lit: glow }; }
    return { id: l.id, n: l.n, head: l.head, state: 'later', lit: false };
  };
  const groups = LESSON_GROUPS.map((gr, k) => ({ label: first ? gr.label : gr.plain, rows: LESSONS.slice(gr.from, LESSON_GROUPS[k + 1]?.from ?? LESSONS.length).map(row) }));
  return { first, label: first ? 'THE GUIDE · FIRST COMMAND' : 'THE GUIDE', head: 'The ten lessons', state, groups, on: lessonBook.on, setting: LESSON_SETTING, waiting: waitingCount(lessonBook) };
}

/**
 * The card that opens a war played from its first month, for lesson 1 to lie on (trap 1: a war from September 1939
 * has no card until its second month). Kept as the month's card, so that the reports page shows it as it shows any
 * month's; `opening` heads it by the moment, "As September 1939 opens" (ruling 123(b)). False where there is none to open.
 */
function openingCard() {
  const s = game();
  if (!s.human || !s.nations[s.human]) return { ok: false };
  try { lastCard = { ...shownCard(s, cardOfRules(s, markOfRules(s), s.human, { opening: true, whole: true })), opening: true }; } catch { return { ok: false }; }
  return { ok: true };
}

// ---- The playtester's click-through (a development tool: Brian, 17 November 2026, "i want the playtester to really
// click everything to make sure it works in the 3d game too") --------------------------------------------------

/**
 * The digest a step is logged with, before and after: the same fields, worked out the same way, as the browser's
 * runner writes, every one from the rules' Game alone. `stamp` is djb2 over the code units of the saved game, so
 * equal stamps mean both games hold the same game.
 */
function digest(region = -1, ids: number[] = [], fleet = -1) {
  const s = game();
  const me = s.human;
  const text = saveGame(s);
  let h = 5381;
  for (let i = 0; i < text.length; i++) h = ((h << 5) + h + text.charCodeAt(i)) | 0;
  const mine = me ? s.corps.filter((c) => !c.dead && c.nat === me) : [];
  const groups = me ? s.airFleets.filter((f) => f.nat === me && f.ftr + f.bmr > 0) : [];
  const hand: (number | string)[][] = [];
  for (const id of ids) { const c = s.corps.find((x) => x.id === id && !x.dead); if (c) hand.push([c.id, s.regions[c.reg]?.name ?? '', c.ap]); }
  const f = fleet >= 0 ? s.fleets.find((x) => x.id === fleet) : undefined;
  if (f) hand.push([f.id, f.stance === 'port' && f.base >= 0 ? s.regions[f.base]?.name ?? '' : s.seas[f.zone]?.name ?? '']);
  const fields = {
    month: turnToDate(s.turn).label, turn: s.turn,
    holder: region >= 0 && s.regions[region] ? s.regions[region].ctrl : '',
    treasury: me ? Math.round(s.nations[me].pp * 100) / 100 : 0,
    wars: me ? [...s.nations[me].atWar].sort() : [],
    corps: mine.filter((c) => c.kind !== 'fort').length, forts: mine.filter((c) => c.kind === 'fort').length,
    fleets: me ? s.fleets.filter((x) => x.nat === me && x.ships.bb + x.ships.cv + x.ships.ca + x.ships.dd + x.ships.ss > 0).length : 0,
    groups: groups.length, wings: groups.filter((x) => x.ship !== undefined).length,
    onOrder: me ? s.nations[me].queue.length : 0,
    hand, stamp: h,
  };
  // (the line as it is logged, written here so that both games print it the same way)
  return { ...fields, text: JSON.stringify(fields) };
}

/** A unit or a place by its name, for a step that names one: a province, a sea, a corps, a fleet, an air group. Names are matched whole first, then by how they begin, without regard to case. */
function find(name: string) {
  const s = game();
  const want = name.trim().toLowerCase();
  const pick = <T>(list: T[], of: (x: T) => string): T | undefined => list.find((x) => of(x).toLowerCase() === want) ?? list.find((x) => of(x).toLowerCase().startsWith(want));
  const me = s.human;
  const corps = pick(s.corps.filter((c) => !c.dead && (!me || c.nat === me)), (c) => c.name);
  const fleet = pick(s.fleets.filter((f) => !me || f.nat === me), (f) => f.name);
  const group = pick(s.airFleets.filter((f) => (!me || f.nat === me) && f.ftr + f.bmr > 0), (f) => f.name);
  const region = pick(s.regions, (r) => r.name);
  const sea = pick(s.seas, (z) => z.name);
  // (a corps of another country, by the name the player knows it by: for a right-click on its figure)
  let seenId = -1, seenReg = -1;
  if (!corps && !fleet && !group && !region && !sea && me) { // (the whole picture is read only where nothing else answers to the name)
    try {
      const seen = pick((view() as unknown as { corps: { id: number; nat: string; name: string; reg: number }[] }).corps.filter((c) => c.nat !== me), (c) => c.name);
      if (seen) { seenId = seen.id; seenReg = seen.reg; }
    } catch { /* (nothing seen of that name) */ }
  }

  return {
    region: region ? region.id : -1, sea: sea ? s.seas.indexOf(sea) : -1,
    corps: corps ? corps.id : seenId, corpsRegion: corps ? corps.reg : seenReg,
    fleet: fleet ? fleet.id : -1, group: group ? group.id : -1, groupShip: group && group.ship !== undefined ? group.ship : -1, groupBase: group ? group.base : -1,
  };
}

const battleTable = battleCalls({ game, attack: playerOrder(attack), raid: (k, t, ids, f) => playerOrder(act)(`raid:${k}:${t}:${ids.join('.')}`, -1, f) });
(globalThis as Record<string, unknown>).TC = {
  digest, find,
  raidsFlown, fleetInHand, briefing, sack: sackGeneral, drafts: draftsFor, evePlanGo, towns: townsList, join: joinArmyAt, sendBack, seaAt: (x: number, y: number) => { try { return { sea: seaUnder(x, y, game()) }; } catch { return { sea: seaUnder(x, y) }; } },
  land, work: playerOrder(work), force, forceOrder: playerOrder(forceOrder), seaReach, bySeaWhy, ship, landingPlan, landOn: playerOrder(landOn, 'march'), record, roll, stance, railReach, rail: playerOrder(rail, 'rail'), railShow, railTip, province, raise: playerOrder(raise), fortify: playerOrder(fortify), cancel, declare, warPlan, diplomacy, court: courtNation, path: takePathOf, invite: inviteNation, page, act: playerOrder(act, '', (action) => !staffAct(action)), abilities, research, tech, techStart, techStop, teams, bureau, generals, assign, promote: promoteIn, relieve: relieveOf, skill: skillUp, ledger, arsenal, order: playerOrder(order), starts, grand, start, save, peek, load, view, reach: lookOut(reachOf), marchPlan: lookOut(marchPlanOf), nextArmy: nextArmyOf, stand: standOf, stands: standsOf, stageGuns, stageCut, stageRecon, stageAgent, stageCaught, agentWhy, move: playerOrder(goOut(move), 'march'), previewAttack: lookOut(preview), attack: playerOrder(goOut(attack)), takeOutlook: lookOut(takeOutlook), take: playerOrder(goOut(take), 'march'), stormLook, storm: playerOrder(storm), gateAsk, gateOrder, gateLeft: gateLeftOf, approach, garrisonWhy, sallyLook, sally: playerOrder(sally), endMonth: endMonthWithLessons, lead: lookOut(leadOf),
  // (Rules 84.3.3) FIRST COMMAND's notes, the Guide's list, the notebook, and the card lesson 1 lies on.
  lessonNow, lessonDeed: lessonDeedOf, lessonAct, notebook: lessonNotebook, guide: lessonGuide, openingCard,
  // (release 1.5) The general's ground battle (battle-bridge.ts): TC.battleLook and TC.battleSettleLand.
  ...battleTable,
  // (release 2 and the 1.5.1 patch: the general's battle of a garrison in hand is looked at, and settled, with it out of its town)
  battleLook: lookOut(battleTable.battleLook), battleSettleLand: goOut(battleTable.battleSettleLand) };
// (The 1.5 patch, breakup.ts) Breaking corps up: TC.breakUp(on) at any time; the options page uses act('breakuprule:on' / ':off').
((globalThis as Record<string, unknown>).TC as Record<string, unknown>).breakUp = (on: boolean) => { const s = game(); setBreakUp(s, !!on); return { on: breakUpOn(s) }; };
// (The 1.5.1 patch, fleetattack.ts) Fleets sent straight at the enemy: TC.fleetAttack(on) at any time; the options page uses act('fleetattackrule:on' / ':off').
((globalThis as Record<string, unknown>).TC as Record<string, unknown>).fleetAttack = (on: boolean) => { const s = game(); setFleetAttack(s, !!on); return { on: fleetAttackOn(s) }; };
