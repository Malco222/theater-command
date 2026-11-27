// The monthly turn: each nation in order gets its month, the computer plays the nations
// the player does not, and the player's orders go through the same rules.

import type { FoughtAir } from './airfought';
import { KIND, MEN_PER_DIV, STACK, type Agent, type AirFleet, type AirMission, type AirReport, type BattleReport, type Build, type Cash, type Corps, type CrewOrder, type Field, type Fleet, type Game, type General, type Headline, type Ships, type ShoreReport, type Kind, type Mission, type Posture, type Region, type Skill, type Stance } from './types';
import { plansView, type PlanItem } from './plans';
import { airBattles, airBases, airTargets, crowdedNews, moveTargets, overOcean, previewAir, rebaseAir, setMission, shipAir, spreadAir, type AirAssess } from './air';
import { raidFogOn, rememberSkies } from './airfog';
import { fleetKnown, fleetsSeen, flyAboard, flyAshore, plannedStrike, searchOptions, searchSea, seaPicture, syncWings, wingCards, type DeckOption, type FleetSeen, type SeaPicture, type SearchOption, type WingCard } from './carriers';
import { planSeaBattle, type SeaPlan } from './seaplan';
import { attackTargets, fleetAttackOn, orderFleetAttack, planFleetAttack, whyNotFleetAttack, type FleetAttackPlan, type FleetAttackResult } from './fleetattack';
import {
  airGroups, airfieldCard, airfields, dayOrNight, groupBases, groupDecks, groupJobs, launchRaid, planRaid, raidKinds, raidKindsOne, raidReplays, raidTargets, scout, setGroupJob,
  type AirfieldCard, type AirfieldRow, type BaseOption, type DayNight, type GroupJob, type GroupRow, type RaidKind, type RaidKindView, type RaidPlan, type RaidReplay, type RaidTarget,
} from './airwar';
import { NAVY, adrift, baseTargets, canPass, cutOff, fleetName, inPassage, laneNear, laneSteps, laneStepsFrom, passageWhy, scuttleFleet, turnBack, crossingsOf, gateHolder, gateShutSince, gatesTick, setGate, landCorps, landingTargets, planCrossing, planLanding, rebase, sail, seaBattles, canShip, type CrossingOutlook, type Landing, type LandingPlan } from './navy';
import { DIFFICULTY, levelOf, setLevel, whyNotLevel, type Level } from './difficulty';
import { assignGeneral, promote, relieve, sack, spendPoint } from './commanders';
import {
  DEFENCE, aloneLine, allied, assess, atWar, battleCost, bloc, chanceOn, corpsBase, declareWar, ensureSupply, evacuate, evenOdds, fights, friendlyUnits, groundWorks, gunsMayFollow, gunsRule, hasRoom, headline, holdCallUp, hostileUnits, limitAttackers,
  endPhase, foldNews, sidesAtPeace, standingCallUp, moveCorps, moveCost, openTheatres, probeCost, probeLine, probeRules, probeWeight, railMove, sameSide, swapCorps, whyNoSwap, resolveBattle, shipMove, sovietEvent, startPhase, theName, whyNoProbe, worksOf,
  type Assessment, type CostOf, type FieldBill, type GroundWorks, type ProbeReport,
  worksMonth,
  declineOffer, rowOffers, takeOffer, type RowOffer,
} from './game';
import { foldChoices, foldCorps, type FoldView } from './fold';
import { atItsPost } from './pacific';
import { stationBars, whyStationBars } from './far';
import { BUDGET, cancelBuild, orderCorps, orderFort } from './economy';
import { liftView, offers, orderOffer, smallAirMonth, type LiftView, type Offer } from './arsenal';
import { cityOrders, ordersAt, type CityOrders, type OrderLine } from './orders';
import { blocked, blowBridges, crossingOf, whyNotBlow } from './rivers';
import { cancelJob, jobsFor, orderJob, type JobKind, type JobOffer } from './engineers';
import { tearUp, whyNotCutRail } from './rails';
import { foundBureau, setTeams } from './research';
import { startResearch, stopResearch, techView, whyNotResearch, type TechView } from './tech';
import { keepsStores, seaLanes, stores, type Lane, type StoresView } from './resources';
import {
  bombard, bombardTargets, detach, escortLane, escortLanes, fleetCard, merge, mergeTargets, planBombard, planEscort, seaOptions, whyNotDetach, whyNotMerge,
  type EscortLane, type EscortPlan, type FleetCard, type SeaOption, type ShorePlan, type ShoreTarget,
} from './fleets';
import {
  INTEL, SIGHT, armiesSeen, arrest, booksOf, cityReport, corpsKnown, fogOn, oddsBand, reconnoitre, recruit, retire, sendAgent, setFog, setMission as setAgentMission, sightOf, sortKnown, spyRoom,
  turnFoe, whyNotArrest, whyNotMission, whyNotRecruit, whyNotSend, whyNotTurn, type ArmySeen, type Books, type CityReport, type Known, type SpyRoom,
} from './intel';
import { turnToDate, type TicketLine } from '../core/ticket';
import { couldJoin, joinArmy, leaveArmy, lendLimit, lentBy, whyNotJoin } from './joint';
import { phaseFor, runAI } from './ai';
import { RELATIONS, closeDiplomacy, court, majorsOf, monthlyDiplomacy, sideName, soreAtShut, soreForget, soreList, warForecast, wouldResent, type WarForecast } from './diplomacy';
import { yieldMonth } from './yield';
import { acrossTheWater, farOn, standWithAllies } from './world';
import { founders, invite, invites, pathOptions, takePath, whyNotInvite, type Invite, type PathId, type PathOption } from './league';
import { awardOf, cashHonour, whyNoSchool } from './stories';
import { takeWinterPeace, whyNotWinterPeace, winterMonth } from './winter';
import { setCrewOrder } from './bombers';
import { sendHero } from './valour';
import { contenders, dominating, keepPeace, startDomination, warDecided, whyNotDomination, worldBoard, worldWinner, type Decided, type WorldBoard } from './domination';
import { fallenNations, liberationMonth, passingCard, passingsIn, standDown, type FallenRow, type PassingCard } from './liberation';
import { armyCard, corpsCard, corpsSeen, type ArmyCard, type CorpsCard } from './armies';
export { FORT_NO_ORDERS, garrisonLine, whyNotOrder, whyNotOrderGarrison } from './armies';
export { gunsLeftAlone } from './siege';
import { corpsMood, type Mood } from './morale';
import { prizeOf, prizesInReach, townAt, townName, townSize, townsOf, type Prize, type TownSize, type TownView } from './towns';
import { CIVIC, civicOffers, civicRules, orderCivic, plotsAt, type CivicOffer, type PlotView } from './buildings';
import { damageRules, openDamage } from './citydamage';
import { flakFog, isFlakLine } from './citydefences';
import { FIELD, fieldRepairView as fieldRepairViewOf, fieldRules as fieldRulesOf, fieldView, fieldsDamaged, fieldsMonth, openFields, orderFieldRepair as repairFieldFor, undoFieldRepair as undoFieldFor, type FieldRepairView, type FieldView } from './fieldstate';
import { damagedTowns, repairAll, repairAllPlan, repairCity, repairView, townsDamaged, undoRepair, type RepairAll, type RepairView } from './repairs';
import { endTrade, gateTrade, refreshTrade, signTrade, tradeBook, type GateTradeRow, type TradeBook } from './trade';
import { gateViews, type GateView } from './gates';
import { SIEGE, apOutOfTown, fieldCtrl, garrison, garrisonOf, guardMen, pocketView, previewSally, previewStorm, sally, siegeRules, siegeView, siegesNow, storm, takeField, townHeld, whyNotGarrison, whyNotSally, whyNotStorm, whyNotTakeField, type PocketView, type SiegeView } from './siege';
import { loadGame, saveGame } from './save';
import { RIVALS, characterOf, effortsOf, lodgment, ops, plansKnown, quietNews, rearMonth, rivalRules, stalemates, type Character } from './staff';
import {
  orderWreckPrize, prizeIncome, prizeMarks, prizesAt, scorchPolicy, setBurnOrder, setScorchPolicy, whyNotWreck,
  type PrizeIncome, type PrizeKind, type PrizeMark, type PrizesView, type ScorchPolicy,
} from './prizes';
import {
  appoint, cabinetBill, cabinetOff, decree, govMul, government, headAsked, headGiven, headOf, noteReadings, officeSecure, promise, promiseTargets, promiseTerms, setAside, setCabinet, setCanFall, standBy, takeBack,
  whyNotAppoint, whyNotDecree, whyNotPromise, whyNotSetAside, whyNotStandBy, whyNotTakeBack, type GovView, type PromiseTarget, type PromiseTerms,
} from './government';
import { footingView, orderFooting, whyNotFooting, type FootingId, type FootingView } from './footing';
import { MANPOWER, callEarly, classGapUntil, classNow, earlyClass, earlyGap, manpower, menWords, raisable, whyNotEarly, type MenView } from './manpower';
import { commandView, crossingWord, handOver, setDepth, setStaffCap, staffMonth, type CommandView, type CrossingWord } from './desks';
import { dropBehind, pressQueue } from './townqueue';
import { HISTORY, dialView, setStop, whyNotStop, type DialView, type Stop } from './history';
import { claimsView, historyMonth, type ClaimView } from './claims';
import { finishMonthEnd, finishMonthOpen } from './finish';
import { staffRuns, type DeskId } from './depth';
import { makeTask, releaseTask, retask, taskForcesMonth, whyNotTask } from './taskforce';
import { aiShadows, orderShadow, shadowOn, shadowTargets, shadowsFollow, stopShadow, whyNotShadow } from './shadow';
import { logVoyages } from './voyage';
import { ATOM, PACES, aiAtom, armStrike, atomFly, atomOn, atomView, beginPlants, chooseSite, disarmStrike, pausePlants, setAtom, setMaking, setPace, whyNotPlants, whyNotStrike, type AtomView } from './atom';
import type { AtomPace } from './types';
import { aiMines, mineSeas, minesMonth, minesStrike, orderMines, seaMinesOn, whyNotMines } from './mines';
import { churnMonth } from './churned';
import { orderCarrierStrike, type CarrierStrikeResult } from './harbourstrike';
import { galeNews } from './heavyseas';
import { orderRearguard, standDown as standRearDown, whyNotRearguard } from './rearguard';
import { minesKnown, whatYouFace } from './facing';
import { SUPPORT, type FaceLine } from './support';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import EVE from '../../data/campaign/eve.json';
import { airWarMonth } from './aircampaigns';
import { cancelFerry, ferryMonth, ferryOn, ferryRoutes, orderFerry, type FerryRoute } from './ferry';
import { bargainsFirstDay, bargainsMonth, placeBid, chooseBuyer } from './bargains';
import { legionsMonth, recallLegion, sendLegion } from './legions';

export const current = (g: Game): string => g.order[g.phase];

function closePhase(g: Game, nat: string): void {
  // The month's air campaigns and fleet actions are fought once the nation's orders are all given.
  airBattles(g, nat);
  seaBattles(g, nat);
  // Carriers lost in the month's fleet actions take their aircraft with them (the grand campaign).
  syncWings(g);
  // A computer staff leaves no airfield of its own crowded at its month's end (b437).
  spreadAir(g, nat);
  // The routine of the month, in one line (the grand campaign).
  foldNews(g, nat);
  endPhase(g, nat);
  if (nat === 'GER') sovietEvent(g);
  g.phase++;
  if (g.phase >= g.order.length) {
    // A fallen country whose capital its friends hold, with no enemy corps in or next to it, returns at the month's
    // end, dated the month that ends (ruling 229, part 2; the grand campaign).
    liberationMonth(g);
    // History's events that are read as a month closes (the far side: America's entry follows Japan's blow, ruling 197).
    closeDiplomacy(g);
    // An alliance finishes its war together (ruling 115): Vichy ends with a France that is free again, and a great
    // power's fall this month turns its conquerors on the great powers their allies still fight.
    finishMonthEnd(g);
    // (And no country is left at war with its own side's members, whatever path took it to that side: ruling 229, 3.)
    sidesAtPeace(g);
    // Works held outright from one month's end to the next are taken over by the side that holds them.
    worksMonth(g);
    // Each fleet's voyage log has the month's line (release 2, voyage.ts; nothing with that switch off).
    logVoyages(g);
    // Minefields wear and are swept (release 2, mines.ts; nothing with that switch off).
    minesMonth(g);
    // Churned ground's record for the 3D map, never a rule (release 2, churned.ts; nothing with that switch off).
    churnMonth(g);
    g.phase = 0; g.turn++; g.week = 1;
    // Airfields as targets: the repairs in hand do the month's work (fieldstate.ts; nothing with the switch off).
    fieldsMonth(g);
    // A gate that has changed hands is no longer shut by the order of the country that lost it.
    gatesTick(g);
    openTheatres(g);
    // A smaller power's aircraft from abroad, and the month its own works open (ruling 213(d), the grand campaign).
    smallAirMonth(g);
    // Air campaigns (ruling 236.2): the bombs' damage is repaired, held skies kept, and the book's campaigns open and close.
    airWarMonth(g);
    monthlyDiplomacy(g);
    // A neutral's bargains (release 2, switched off in release 1): terms end, bids open and close, patience moves.
    bargainsMonth(g);
    // Volunteer legions (release 2, switched off in release 1): they reach the line, come home, or are sent.
    legionsMonth(g);
    // The Winter War: the peace of Moscow, or the offer of it to a player on either side (ruling 228, 1b).
    winterMonth(g);
    // The history dial's month: the book's marks, and the neutrals' ambitions (the grand campaign, off the middle stop).
    historyMonth(g);
    // The wars an alliance gave notice of when its enemy fell, now due (ruling 115).
    finishMonthOpen(g);
    // Wars nobody can fight any longer end in an armistice (the grand campaign).
    stalemates(g);
  }
}

/** Plays one computer-run nation's month. */
export function stepAI(g: Game): string {
  const nat = current(g);
  g.week = 1;
  startPhase(g, nat);
  // Its fleets at sea in an enemy's minefield may strike a mine (release 2, mines.ts; nothing with that switch off).
  minesStrike(g, nat);
  // Its air groups on their way to another field fly the month's hops (release 2, ferry.ts; nothing with that switch off).
  ferryMonth(g, nat);
  // Every shadow follows its quarry as a country's month opens (release 2, shadow.ts; nothing with that switch off).
  shadowsFollow(g, nat);
  // (The yield form, behind its switch: a beaten country with no relief in reach yields as its month opens.)
  yieldMonth(g, nat);
  if (g.nations[nat].alive) {
    // Its atomic project, before its other orders, so that a bomb in hand gets the bomber group it needs (release 2, atom.ts).
    aiAtom(g, nat);
    runAI(g, nat);
    // The computer's admirals set their shadows, after their other orders (release 2, shadow.ts).
    aiShadows(g, nat);
    // And lay their mines (release 2, mines.ts).
    aiMines(g, nat);
  }
  closePhase(g, nat);
  // Supply is clean whenever the rules hand the month on (release 2, the supply window): a step that ends with a
  // province taken, a fleet sailed or a bridge blown leaves the supply picture stale, and until now it stayed stale
  // until whoever read it next drew it afresh. On the 3D table that could be a picture, so when a picture was drawn
  // decided when supply was refreshed. Now the step itself ends by making it fresh, the same in a war nobody plays,
  // on the table and in the browser.
  ensureSupply(g);
  return nat;
}

/** Opens the player's month: reinforcements, replacements, fresh action points. */
export function beginHuman(g: Game): void {
  g.week = 1;
  startPhase(g, current(g));
  // His fleets at sea in an enemy's minefield may strike a mine (release 2, mines.ts; nothing with that switch off).
  minesStrike(g, current(g));
  // His air groups on their way to another field fly the month's hops (release 2, ferry.ts; nothing with that switch off).
  ferryMonth(g, current(g));
  // The month's gales over his seas, in a line of news (release 2, heavyseas.ts; nothing with that switch off).
  galeNews(g, current(g));
  // Every shadow follows its quarry, and his report where it is (release 2, shadow.ts; nothing with that switch off).
  shadowsFollow(g, current(g));
  // (The far side: the rear garrison of a port he took last month comes to a player as to a computer's staff, staff.ts rearMonth.)
  rearMonth(g, current(g));
  // (The yield form: the player's country is told the month the cause is met, and yields at the next opening.)
  yieldMonth(g, current(g));
  // The month's reports come in: what the player's agents have in view is identified afresh.
  reconnoitre(g);
  // Fog on raids (airfog.ts, its switch on): and what his side sees of the enemy's air defence is noted, as every staff's is.
  if (raidFogOn(g)) rememberSkies(g, g.human);
  // And the cabinet notes what the attack card reads against the towns he could promise, the way his page now reckons it.
  noteReadings(g);
  quietNews(g);
  // A field of his that has become crowded since last month: the month card says so once (b448).
  crowdedNews(g);
  // His task forces come in to refit or go back to their jobs (release 2, taskforce.ts): before his admirals' month.
  taskForcesMonth(g, current(g));
  // Last, the desks he has handed to his staff do their month's work, each with one line of news (the grand campaign).
  staffMonth(g);
  // And then, as his own presses would, the buildings he queued for his towns (release 2, townqueue.ts; nothing with that switch off).
  pressQueue(g);
  // (The supply window: his month opens on a fresh supply picture, whatever his desks and his queue did.)
  ensureSupply(g);
}

export function endHuman(g: Game): void {
  entrench(g, current(g));
  closePhase(g, current(g));
  // (The supply window, as stepAI: the player's month hands on a fresh supply picture.)
  ensureSupply(g);
}

/**
 * Troops under orders to dig in who spent the whole month where they stood are entrenched from now on. Anyone
 * who marched, went by rail or attacked is not, and has to sit a month out before the trenches are ready again.
 */
function entrench(g: Game, nat: string): void {
  // In the grand campaign nobody has to be told: whoever stood the month digs in as it closes (endPhase).
  if (g.flags.open === 1 && DEFENCE.entrench.auto) return;
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead) continue;
    if (c.digs && c.kind !== 'fort' && c.ap >= c.apMax && !c.fought) c.dug = true;
    else if (c.dug) delete c.dug;
  }
}

// ---- The eve of war ------------------------------------------------------------------------

/** The nation history's plan sends against the player before the player's first month comes round, if any. */
export function eveAttacker(g: Game): string | null {
  const me = g.human;
  if (!me || g.turn !== 1) return null;
  return g.order.slice(0, g.order.indexOf(me)).find((n) => {
    const ph = phaseFor(g, n);
    return ph?.posture === 'attack' && (!!ph.declare?.includes(me) || (atWar(g, n, me) && !!ph.enemies?.includes(me)));
  }) ?? null;
}

/** True while the player is placing an army on the eve of war. */
export const onEve = (g: Game): boolean => g.flags.eve === 1;

/**
 * The eve of war. A player whose country is attacked in the opening month before its own turn comes round would
 * otherwise watch it fall without giving an order, so that player places the army first: marches and rail moves
 * on friendly ground, on the corps' opening action points. No shot is fired, no month passes, nothing is
 * produced and nobody else moves.
 */
export function beginEve(g: Game): boolean {
  if (g.phase !== 0 || g.flags.eve !== undefined || !eveAttacker(g)) return false;
  g.flags.eve = 1;
  g.phase = g.order.indexOf(g.human!);
  g.week = 1;
  return true;
}

export interface EvePlan { title: string; advice: string[]; place: [string, number][] }

/** What the staff advises on the eve of war, if the player's nation has such advice and the eve is not over. */
export function evePlan(g: Game): EvePlan | null {
  if (!onEve(g) || !g.human) return null;
  return (EVE as unknown as Record<string, EvePlan | undefined>)[g.human] ?? null;
}

/**
 * Places the army as the staff advises, by rail, strongest corps first. Only corps that have not moved yet can go,
 * so it does the most when it is the first thing done. Returns how many corps it moved.
 */
export function orderEvePlan(g: Game): number {
  const plan = evePlan(g);
  if (!plan) return 0;
  const army = g.corps.filter((c) => ownOrder(g, c)).sort((a, b) => b.divs * b.str - a.divs * a.str || a.id - b.id);
  const want = new Map<number, number>();
  let k = 0;
  for (const [name, n] of plan.place) {
    const r = g.byName[name];
    for (let i = 0; i < n && k < army.length; i++, k++) if (r) want.set(army[k].id, r.id);
  }
  let moved = 0;
  // Several passes: a corps can only go where there is room, and some have to leave first.
  for (let pass = 0; pass < 6; pass++) {
    for (const c of army) {
      const to = want.get(c.id);
      if (to !== undefined && c.reg !== to && orderRail(g, c, to)) moved++;
    }
  }
  return moved;
}

/** Ends the eve of war: the month begins with whoever moves first. */
export function endEve(g: Game): void {
  if (!onEve(g)) return;
  // An army left in the positions it held all summer has had time to dig them.
  entrench(g, g.human!);
  g.flags.eve = 0;
  g.phase = 0;
  g.week = 1;
}

/** The levels a grand campaign can be played at, easiest first, and the one this game is on. */
export const levels = (): Level[] => DIFFICULTY.levels;
export const levelNow = (g: Game): Level => levelOf(g);

/** Why the player cannot put the campaign on this level, or null if he can. */
export function whyNotSetLevel(g: Game, id: string): string | null {
  if (g.human === null) return 'Nobody is playing this war.';
  return whyNotLevel(g, id);
}

/** Puts the campaign on another level. It can be done in any month and takes hold at once. */
export function orderLevel(g: Game, id: string): boolean {
  return whyNotSetLevel(g, id) === null && setLevel(g, id);
}

// ---- The history dial (the grand campaign) -----------------------------------------------------

/** The five stops of the history dial, from By the book to Anything goes: number, name, a word under it, and what it does in plain lines. */
export const historyStops = (): Stop[] => HISTORY.stops;
/** The dial as it stands in this campaign: the stop, the loosest stop it has been played at, and where the book has been closed. Null outside the grand campaign. */
export const dialOf = (g: Game): DialView | null => dialView(g);
/** Why the player cannot turn the dial to this stop, or null if he can. */
export function whyNotSetHistory(g: Game, stop: number): string | null {
  if (g.human === null) return 'Nobody is playing this war.';
  return whyNotStop(g, stop);
}
/**
 * Turns the history dial. It can be done in any month and bears on what the computer's staffs and the neutral
 * world decide from the next month on. `fresh` is for a campaign that starts at this stop.
 */
export function orderHistory(g: Game, stop: number, fresh = false): boolean {
  const ok = (fresh ? whyNotStop(g, stop) : whyNotSetHistory(g, stop)) === null && setStop(g, stop, fresh);
  // (A new campaign's neutral bargains are opened before its stop is set: the record's first-day buyers are read again
  // at the stop it starts at. Release 2; nothing while the switch is off.)
  if (ok && fresh) bargainsFirstDay(g);
  return ok;
}
/** The claims on named ground that can be read now (stops 4 and 5): who claims what from whom and why, and whether it is being pressed or fought for. */
export const claimsOf = (g: Game): ClaimView[] => claimsView(g);

// ---- How deep the player plays (the grand campaign) --------------------------------------------

/** The depths, the desks with who runs each and what it did this month, the spending cap and the three off switches. Null outside the grand campaign. */
export const commandOf = (g: Game): CommandView | null => commandView(g);

/** True while the player's staff runs this desk for him. */
export const staffRunsDesk = (g: Game, desk: DeskId): boolean => staffRuns(g, g.human, desk);

/** Hands a desk to the staff, or takes it back. In the player's own month a staff given a desk goes to work on it at once. */
export function orderDesk(g: Game, desk: DeskId, staff: boolean): boolean {
  return g.human !== null && handOver(g, desk, staff, humanUp(g) && !onEve(g));
}

/** Puts the campaign at one of the three depths: every desk, the cabinet, whether it can fall, and the fog of war. It can be done in any month. */
export function orderDepth(g: Game, id: string): boolean {
  return g.human !== null && setDepth(g, id, humanUp(g) && !onEve(g));
}

/** Sets the share of the treasury the staff may spend in a month: 25, 50, 75 or 100. */
export function orderStaffCap(g: Game, pct: number): boolean {
  return g.human !== null && setStaffCap(g, pct);
}

/** True when the nation up next is the player's and still in the war. */
export function humanUp(g: Game): boolean {
  return g.human !== null && current(g) === g.human && g.nations[g.human].alive;
}

/** Runs a whole month with every nation on the computer. */
export function runTurnAI(g: Game): void {
  const t = g.turn;
  while (g.turn === t) stepAI(g);
}

// ---- The player's orders -----------------------------------------------------------------

export function canOrder(g: Game, c: Corps): boolean {
  // A corps sent home on a bond tour or to the schools takes no orders until it is back; nor does one at the battle schools this month.
  if (c.dead || g.human === null || current(g) !== g.human || c.kind === 'fort' || c.leave !== undefined || c.school) return false;
  // (The far side: Vichy's corps that stayed at their posts in Indochina on the demand take no orders from Japan, pacific.ts atItsPost.)
  if (atItsPost(c)) return false;
  // An ally's corps that marches with the player's army takes his orders too (the grand campaign).
  return c.nat === g.human || (c.joint === g.human && allied(g, c.nat, g.human));
}

/** True for a corps of the player's own nation that can be given orders: what an ally has lent does not sail, drop or dig for him. */
const ownOrder = (g: Game, c: Corps): boolean => c.nat === g.human && canOrder(g, c);

export interface Reach { cost: number; prev: number; attack: boolean }

/** Every region this corps could get to this month, and the enemy-held ones it could attack. */
export function reach(g: Game, c: Corps): Map<number, Reach> {
  const out = new Map<number, Reach>();
  if (c.kind === 'fort' || c.ap <= 0) return out;
  out.set(c.reg, { cost: 0, prev: -1, attack: false });
  // Guns and engineers follow a fighting corps onto enemy-held ground, they never lead (ruling 211(a), the grand
  // campaign). Such a corps reaches enemy-held ground only where a fighting corps of its side stands already, or
  // where one stands with it that still has a move this month and can go first (marchOrder sends it first).
  const led = !gunsRule(g) || fights(c) || g.regions[c.reg].units.some((u) => u !== c && !u.dead && u.ap > 0 && fights(u) && allied(g, u.nat, c.nat));
  const open = [c.reg];
  while (open.length) {
    open.sort((a, b) => out.get(a)!.cost - out.get(b)!.cost);
    const a = open.shift()!;
    const ca = out.get(a)!.cost;
    for (const j of g.regions[a].nb) {
      const r = g.regions[j];
      const friendly = allied(g, c.nat, r.ctrl);
      if (!friendly && (onEve(g) || !atWar(g, c.nat, r.ctrl))) continue; // on the eve of war nobody crosses a frontier
      if (blocked(g, c, r, a)) continue; // tanks and the steepest ground, wheels and a river with its bridges down
      // An attack on the province next door takes one move of the month, whatever the ground would cost to march
      // over: the battle charges one, and the attack's card and its order ask only for a move left. So the gate
      // below is for a march alone, and an army that has come up with a move in hand is shown its attack (Version
      // 84.2.3: Brian's find of 2026-11-17, traced by the code checker. Before it, such an army had no red on
      // the map and no word of an attack under the pointer, though a click still opened the card).
      if (hostileUnits(g, r, c.nat).length) {
        if (a === c.reg && !out.has(j)) out.set(j, { cost: 1, prev: a, attack: true });
        continue;
      }
      if (!friendly && !led && !gunsMayFollow(g, c, r)) continue;
      const step = moveCost(g, c, r, a);
      const left = c.ap - ca;
      if (left <= 0 || left < Math.min(step, 4)) continue;
      const cost = ca + step;
      if (!hasRoom(g, r, c)) continue;
      const old = out.get(j);
      if (old && old.cost <= cost) continue;
      out.set(j, { cost, prev: a, attack: false });
      // An army that comes up to a town the enemy holds stops there for the month (the grand campaign).
      if (!friendly && siegeRules(g) && townHeld(g, r)) continue;
      open.push(j);
    }
  }
  return out;
}

/** Marches a corps to a region it can reach, one region at a time. Returns how far it got. */
export function orderMove(g: Game, c: Corps, to: number): number {
  if (!canOrder(g, c)) return 0;
  const m = reach(g, c);
  const end = m.get(to);
  if (!end || end.attack) return 0;
  const path: number[] = [];
  for (let at = to; at !== c.reg && at >= 0; at = m.get(at)!.prev) path.unshift(at);
  let n = 0;
  for (const j of path) { if (!moveCorps(g, c, g.regions[j])) break; n++; }
  return n;
}

/**
 * The order the corps of an army are marched in (both games): the fighting corps first, then the guns and the
 * engineers, who follow them onto enemy-held ground and never lead (ruling 211(a), the grand campaign). Elsewhere
 * the order given.
 */
export const marchOrder = (g: Game, corps: Corps[]): Corps[] => (gunsRule(g) ? [...corps].sort((a, b) => Number(fights(b)) - Number(fights(a))) : corps);

/**
 * Why these corps cannot go forward onto this ground by themselves, or null if they can (ruling 211(a), the grand
 * campaign; both games say the sentence). A force with no fighting corps in it (infantry, cavalry, mountain troops,
 * motorized troops or tanks) opens no attack and marches onto no enemy-held ground: guns and engineers go in beside
 * a fighting corps, or follow one onto ground it stands on. Asked of the corps picked for an attack or a march.
 */
export function whyNotLead(g: Game, corps: Corps[], to: number): string | null {
  const r = g.regions[to];
  if (!gunsRule(g) || !r) return null;
  const able = corps.filter((c) => !c.dead && c.kind !== 'fort' && !blocked(g, c, r));
  if (!able.length || able.some(fights)) return null;
  const c = able[0];
  if (allied(g, c.nat, r.ctrl) || !atWar(g, c.nat, r.ctrl) || gunsMayFollow(g, c, r)) return null;
  return aloneLine(able);
}

/** The player's corps that could join an attack on a region. */
export function candidates(g: Game, target: Region): Corps[] {
  if (g.human === null) return [];
  return g.corps.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id));
}

export function previewAttack(g: Game, corps: Corps[], target: Region): Assessment | null {
  // Only as many corps as the ground lets in will fight: the forecast counts the same force the battle will.
  const going = jointForce(g, limitAttackers(g, jointForce(g, corps, target), target), target);
  if (!going.length || !hostileUnits(g, target, going[0].nat).length || whyNotAttack(g, target, going[0].nat)) return null;
  return assess(g, going, target);
}

/**
 * Why troops may not be attacked where they stand, or '' if they may (the grand campaign). Enemy troops on the
 * ground of a country the attacker is neither allied to nor at war with are out of reach: marching in there is an
 * act of war on that country, and it has to be declared first (orderDeclare). So are an enemy's troops stationed by
 * agreement on the ground of a country the attacker is at peace with (far.ts `stationBars`).
 */
export function whyNotAttack(g: Game, target: Region, nat: string | null = g.human): string {
  if (g.flags.open !== 1 || nat === null) return '';
  if (allied(g, nat, target.ctrl)) return '';
  if (atWar(g, nat, target.ctrl)) return whyStationBars(g, nat, target) ?? '';
  const host = g.nations[target.ctrl];
  return host ? `${target.name} belongs to ${theName(host)}, and you are not at war with it. Declare war on ${theName(host)} first, or leave the troops there alone.` : '';
}

/**
 * The corps of a force that can take part in an attack on a region, the player's own first. An ally's corps in a
 * joint army fights only an enemy its own country is at war with: against anyone else it stays where it is.
 */
export function jointForce(g: Game, corps: Corps[], target: Region): Corps[] {
  if (!corps.some((c) => c.joint !== undefined)) return corps;
  const going = corps.filter((c) => c.joint === undefined || hostileUnits(g, target, c.nat).length > 0);
  return going.sort((a, b) => (a.joint === undefined ? 0 : 1) - (b.joint === undefined ? 0 : 1));
}

/**
 * What the player's staff can say of an attack before it is ordered. Where every defender has been identified this
 * month the odds are the odds ('exact'). Otherwise the defence is partly a guess, and the odds are a band that holds
 * the truth: 'lo' to 'hi', with the staff's best guess 'odds' between them. 'chance' is the chance of taking the
 * ground at those odds, and 'lines' is the Battle Ticket with what is not known of the defenders left out.
 */
export interface Outlook {
  exact: boolean;
  odds: number; lo: number; hi: number;
  chance: number; chanceLo: number; chanceHi: number;
  /** The odds at which it is an even chance: 1.2 to 1 in the open, more for an assault on a town or an attack on a prepared defence, where luck runs against the attacker. */
  even: number;
  /** Defending corps not identified, and those known only from a stale report. */
  unidentified: number;
  stale: number;
  /** One sentence on why the odds are a band, and how to narrow it. Empty where they are exact. */
  note: string;
  /** True where the band of chances is too wide to plan on: more than 40 points from one end to the other. */
  wide: boolean;
  /** The staff's best guess at the chance where the odds are a band: the chance averaged over the band ('chance' is the chance at the middle of it). */
  guess: number;
  /** What would narrow the band, and by how much, in one sentence. Empty where the odds are exact. */
  narrow: string;
  /**
   * What the fight is likely to cost in men if it carries the ground and if it is thrown back (null where that
   * cannot happen): 'ours' is exact, 'share' is the defenders' loss as a share of their men, and 'theirs' is
   * their loss in men where every defender is identified, null where the defence is partly a guess.
   */
  cost: { won: CostSeen | null; lost: CostSeen | null };
  lines: TicketLine[];
  /** WHAT YOU FACE, as his reports know it (the grand campaign; null in a history campaign): a sign and a sentence a line, four at most. */
  face: FaceLine[] | null;
}
/** One result's bill as the player's staff can state it before the fight. */
export interface CostSeen { ours: number; theirs: number | null; share: number }

export function attackOutlook(g: Game, corps: Corps[], target: Region): Outlook | null {
  const a = previewAttack(g, corps, target);
  if (!a) return null;
  // The same force the battle will count: the bill is reckoned on it.
  const going = jointForce(g, limitAttackers(g, jointForce(g, corps, target), target), target);
  return outlookOf(g, a, target, going);
}

/**
 * The force an attack card opens with (both interfaces): the corps the player picked, unless everyone the roads
 * will carry makes a better attack of it, in which case everyone in reach, for the player to take corps out of.
 * 'widened' says the staff brought more in than were picked.
 */
export function openingForce(g: Game, picked: Corps[], target: Region): { corps: Corps[]; widened: boolean } {
  const all = candidates(g, target);
  if (all.length <= picked.length) return { corps: picked, widened: false };
  const mine = previewAttack(g, picked, target), full = previewAttack(g, all, target);
  if (full && (!mine || full.odds > mine.odds * 1.02)) return { corps: all, widened: true };
  return { corps: picked, widened: false };
}

/** An assessment as the player's staff can report it: exact where every defender is identified, a band where not. */
function outlookOf(g: Game, a: Assessment, target: Region, going: Corps[] = []): Outlook {
  // The run of the day's luck comes with the assessment: wider in the streets of a town, and on a line that is dug in and fed.
  const dice = a.dice;
  const chanceAt = (odds: number): number => chanceOn(odds, dice);
  const even = evenOdds(dice);
  const band = oddsBand(g, a.defenders, a.odds, target.id, g.human);
  const k = sortKnown(g, a.defenders, g.human);
  // The bill: in men for both sides where the defence is known, and the defenders' as a share of their men where it is not.
  // (Where the staff's own estimate rules a result out but the band does not, that result is billed at the end of the band that allows it.)
  const billAt = (odds: number): { won: CostOf | null; lost: CostOf | null } => (going.length ? battleCost(g, a, going, target, {}, odds) : { won: null, lost: null });
  const bill = billAt(band.exact ? a.odds : band.mid);
  if (!band.exact && !bill.won) bill.won = billAt(band.hi).won;
  if (!band.exact && !bill.lost) bill.lost = billAt(band.lo).lost;
  const seen = (c: CostOf | null): CostSeen | null => (c ? { ours: c.ours, theirs: band.exact ? c.theirs : null, share: c.share } : null);
  const cost = { won: seen(bill.won), lost: seen(bill.lost) };
  const face = whatYouFace(g, a, target, g.human);
  if (band.exact) return { exact: true, odds: a.odds, lo: a.odds, hi: a.odds, chance: chanceAt(a.odds), chanceLo: chanceAt(a.odds), chanceHi: chanceAt(a.odds), even, unidentified: 0, stale: 0, note: '', wide: false, guess: chanceAt(a.odds), narrow: '', cost, lines: a.lines, face };
  const n = a.defenders.length;
  const corpsWord = (x: number): string => `${x} corps`;
  // What the ticket says of the defenders' numbers, spirit and armour is left out where it is not known.
  const lines = a.lines.map((l): TicketLine => {
    // (The town's guard fighting beside them (ruling 232) is no secret: its part of the line stays.)
    if (l.label === 'Forces') { const guard = a.guard ? / and the town guard of about [\d,]+$/.exec(l.value)?.[0] : undefined; return { ...l, value: `${l.value.split(', against ')[0]}, against ${corpsWord(n)}${k.none.length ? `, ${k.none.length === n ? 'none' : `${n - k.none.length}`} of them identified` : ', known from old reports'}${guard ? `,${guard}` : ''}` }; }
    if (l.label === 'Morale') {
      const ours = l.value.split(', defenders')[0];
      return { ...l, value: /^Attackers/.test(ours) ? `${ours}; the defenders' spirit is not known` : "The defenders' spirit is not known", effect: l.effect ? l.effect.split(', defence')[0].replace(/^Defence.*$/, '') || undefined : undefined };
    }
    if (l.label === 'Troops') return { label: l.label, value: 'How good the defenders are is not known', effect: 'Training and experience count for as much as numbers: identify them first' };
    if (l.label === 'Armor' && /without armor/.test(l.value) && k.none.length) return { ...l, value: 'No tanks have been seen among the defenders', effect: 'Tank attack up 25% if there are none' };
    // (The grand campaign: the guns of a defence that is not identified are not known, and neither is what they take from the tanks.)
    if (l.label === 'Armor' && /^Tank attack up/.test(l.value) && k.none.length) return { label: l.label, value: 'Tank attack up by as much as 25%: their guns are not known' };
    // (City defences: the town's heavy guns against the tanks are not known while the garrison is not identified; the line
    // says the most they could take, as the line above does for the guns of a defence in the field.)
    if (isFlakLine(l) && k.none.length) return flakFog();
    return l;
  // (Guns alone, ruling 211(a): before the battle the row is left out while any defender is not identified, and the
  // odds speak alone; the fog wins, the design director's log b357. After it they have been seen.)
  }).filter((l) => !k.none.length || !(l.label === 'Machine guns' || l.label === 'Tanks alone' || l.label === 'Guns alone' || l.label === 'Who pays' || (l.label === 'Minefields' && !minesKnown(g, target, g.human))));
  // Tanks with no infantry beside them, against guns that are not known: the most they could take (ruling 164(c)).
  const loneMost = k.none.length && a.faced?.antiTank && a.faced.shock !== null ? SUPPORT.antiTank.alone.cut * (1 - a.faced.antiTank.cover) : 0;
  if (loneMost >= 0.005) {
    const at = lines.findIndex((l) => l.label === 'Armor');
    lines.splice(at < 0 ? lines.length : at + 1, 0, { label: 'Tanks alone', value: `Tank attack cut by as much as ${Math.round(loneMost * 100)}%: their guns are not known` });
  }
  const nn = k.none.length, old = k.report.length;
  const count = (x: number): string => ['No', 'One', 'Two', 'Three', 'Four', 'Five', 'Six'][x] ?? `${x}`;
  const dark = !nn ? '' : nn < n ? `${count(nn)} of the ${n} defending corps ${nn === 1 ? 'is' : 'are'} not identified`
    : n === 1 ? 'The defending corps is not identified' : n === 2 ? 'Neither of the two defending corps is identified' : `None of the ${n} defending corps is identified`;
  const dated = !old ? '' : nn ? `${count(old).toLowerCase()} ${old === 1 ? 'is' : 'are'} known only from ${old === 1 ? 'an old report' : 'old reports'}`
    : old === n ? (n === 1 ? 'The defending corps is known only from an old report' : 'The defending corps are known only from old reports')
    : `${count(old)} of the ${n} defending corps ${old === 1 ? 'is' : 'are'} known only from ${old === 1 ? 'an old report' : 'old reports'}`;
  const what = [dark, dated].filter(Boolean).join(', and ');
  // What would narrow the band, and by how much: with the unidentified corps identified only the old reports are in doubt.
  const R = INTEL.reports;
  let w = 0, dOld = 0;
  for (const c of a.defenders) { const x = c.divs * Math.max(0.05, c.str); w += x; if (k.report.includes(c)) dOld += x * Math.min(R.widest, R.drift * corpsKnown(g, c, g.human).age); }
  dOld = w > 0 ? dOld / w : 0;
  const part = band.doubt > 0 ? Math.round((dOld / band.doubt) * 10) * 10 : 0;
  const them = (x: number): string => (x === 1 ? 'the one corps' : `the ${count(x).toLowerCase()} corps`);
  const narrow = nn && !old ? `Identify ${them(nn)} the staff cannot see, and it can give one figure in place of this band.`
    : nn ? `Identify ${them(nn)} the staff cannot see, and the band closes to about ${Math.max(10, Math.min(90, part))}% of its width; fresh reports on the rest close it altogether.`
    : `Fresh reports on ${them(old)} known from old ones would turn this band into one figure.`;
  return {
    exact: false, odds: band.mid, lo: band.lo, hi: band.hi, chance: chanceAt(band.mid), chanceLo: chanceAt(band.lo), chanceHi: chanceAt(band.hi), even,
    unidentified: k.none.length, stale: k.report.length,
    note: `${what}, so the odds are the staff's estimate. ${probeRules(g) ? 'A probe by one corps' : 'A battle'}, an agent on watch or a reconnaissance flight over ${target.name} would show what is there.`,
    wide: chanceAt(band.hi) - chanceAt(band.lo) > 0.4, guess: Array.from({ length: 21 }, (_, i) => chanceAt(band.lo + ((band.hi - band.lo) * i) / 20)).reduce((s, x) => s + x, 0) / 21, narrow, cost,
    lines, face,
  };
}

export function orderAttack(g: Game, corps: Corps[], target: Region, fought?: number, field?: FieldBill): BattleReport | null {
  if (onEve(g)) return null;
  const ok = jointForce(g, limitAttackers(g, jointForce(g, corps.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id)), target), target), target);
  if (!ok.length || !hostileUnits(g, target, ok[0].nat).length || whyNotAttack(g, target, ok[0].nat)) return null;
  // (src/battle) A battle the player fought out hands back the ratio it came to, in place of the dice.
  return resolveBattle(g, ok, target, fought === undefined ? {} : { fought, ...(field ? { field } : {}) });
}

// ---- Softening a line, handing it over, and folding corps together (the grand campaign) -------------------------
//
// Before an attack on troops who are dug in, the player can soften the line: one corps probes it (orderProbe), or
// the bombers go for its fieldworks (the raid planner's 'line': previewRaid and orderRaid). softenOptions() says
// what each would do, for one row on the attack card. Fieldworks a corps leaves behind stay on the ground for the
// next corps of its side to march in (fieldworksLeft). Two worn corps standing together can be made into one
// (foldChoicesAt, orderFold): the corps emptied becomes a cadre, raised again from the arsenal (key 'cadre:<id>').

/** One of the player's corps probes the enemy line in the province beside it: who is there, and a month off their works. */
export function orderProbe(g: Game, c: Corps, target: Region): ProbeReport | null {
  if (onEve(g) || !canOrder(g, c) || whyNotAttack(g, target, c.nat)) return null;
  return probeLine(g, c, target);
}

/** The corps in a neighbouring province that this corps of the player's could change places with in one order (the grand campaign). */
export function swapChoices(g: Game, c: Corps, to: number): Corps[] {
  if (g.human === null || !humanUp(g) || onEve(g) || !canOrder(g, c)) return [];
  return g.regions[to].units.filter((b) => canOrder(g, b) && whyNoSwap(g, c, b) === null);
}

/** The player has two corps in neighbouring provinces change places. False if they cannot. */
export function orderSwap(g: Game, a: Corps, b: Corps): boolean {
  if (onEve(g) || !canOrder(g, a) || !canOrder(g, b)) return false;
  return swapCorps(g, a, b);
}

/** What softening a line first would do, for the attack card. */
export interface SoftenView {
  /** The most months of fieldworks any defender there stands in; 0 where nobody is dug in. */
  works: number;
  /** A probe by one corps: which corps the staff would send, what it costs it, what it finds, or why it cannot be done. */
  /** 'weight' is how much of a full probe the corps has the men for (0 to 1), 'full' the men a full one takes. */
  probe: { corps: number; name: string; why: string | null; ap: number; men: number; wear: number; fresh: number; weight: number; full: number };
  /** Bombing the line, where the air war is played: the groups the staff would send and the months they should take off, or why not. */
  bomb: { why: string | null; groups: number; bombers: number; wear: number; verdict: string } | null;
}

/**
 * The two ways to soften the line in a province before attacking it with these corps, or null where there is no
 * enemy army in the field there. The staff picks the corps it can best spare for the probe: the weakest of the
 * force that still has the moves for it.
 */
export function softenOptions(g: Game, corps: Corps[], target: Region): SoftenView | null {
  if (!probeRules(g) || g.human === null || !humanUp(g)) return null;
  const foes = hostileUnits(g, target, g.human);
  if (!foes.length || whyNotAttack(g, target, g.human)) return null;
  const P = DEFENCE.probe!;
  const works = Math.round(Math.max(0, ...foes.map(worksOf)) * 10) / 10;
  const pool = [...new Set([...corps, ...candidates(g, target)])].filter((c) => canOrder(g, c));
  // Who goes: a corps with the weight for a full probe, and of those the one the attack will miss least.
  const can = pool.filter((c) => whyNoProbe(g, c, target) === null).sort((a, b) => probeWeight(b) - probeWeight(a) || corpsBase(g, a) * KIND[a.kind].atk - corpsBase(g, b) * KIND[b.kind].atk || a.id - b.id);
  const who = can[0] ?? pool[0];
  const fresh = foes.filter((d) => corpsKnown(g, d, g.human).level !== 'exact').length;
  const probe = who
    ? { corps: who.id, name: who.name, why: whyNoProbe(g, who, target), ap: P.ap, men: probeCost(g, who), wear: Math.round(Math.min(P.wear * probeWeight(who), works) * 10) / 10, fresh, weight: probeWeight(who), full: P.weight ?? 0 }
    : { corps: -1, name: '', why: 'No corps of yours stands beside that line with moves left.', ap: P.ap, men: 0, wear: Math.min(P.wear, works), fresh, weight: 1, full: P.weight ?? 0 };
  let bomb: SoftenView['bomb'] = null;
  if (raidKinds(g, g.human).some((k) => k.key === 'line')) {
    if (works <= 0) bomb = { why: 'Nobody there is dug in: there are no fieldworks to bomb.', groups: 0, bombers: 0, wear: 0, verdict: '' };
    else {
      const plan = planRaid(g, 'line', target.id, undefined, g.human);
      bomb = { why: plan.why, groups: plan.picked.length, bombers: plan.bombers, wear: Math.min(plan.wear ?? 0, works), verdict: plan.verdict };
    }
  }
  return { works, probe, bomb };
}

/** Fieldworks standing empty in a province that the player's side could take over by marching in, or null. */
export function fieldworksLeft(g: Game, reg: number): GroundWorks | null {
  const left = groundWorks(g, reg);
  return left && g.human !== null && allied(g, g.human, left.nat) ? left : null;
}

/** Every way two of the player's corps in a province could be folded into one this month: each allowed pair, both ways round. */
export function foldChoicesAt(g: Game, reg: number): FoldView[] {
  return g.human !== null && humanUp(g) ? foldChoices(g, reg, g.human).filter((v) => { const c = g.corps.find((x) => x.id === v.keep); return !!c && canOrder(g, c); }) : [];
}

/** Folds one of the player's corps into another: 'keep' takes the men of 'gone', which goes home as a cadre. */
export function orderFold(g: Game, keep: number, gone: number): boolean {
  const a = g.corps.find((c) => c.id === keep), b = g.corps.find((c) => c.id === gone);
  if (!a || !b || !canOrder(g, a) || !canOrder(g, b)) return false;
  return foldCorps(g, a, b);
}

// ---- Cities, towns and sieges (the grand campaign) -------------------------------------------------------------
//
// The town is the key to its province. Troops stand either in the field (the province's armies, as always) or
// inside the town as its garrison. Winning the field wins the country round the town and puts it under siege; the
// province changes hands only when the town is stormed, starved out or opens its gates. An interface shows a
// province's town with townSiege(), lists every siege with siegesInSight(), and gives the orders below: into the
// town and out of it, the assault, and the garrison's sortie. A siege is raised by an ordinary attack on the army
// round the town (orderAttack): the garrison joins in.

/** True for a corps of the player's own in a garrison, or in the field, that he may order into or out of a town this month. */
const townOrder = (g: Game, c: Corps): boolean => !c.dead && g.human !== null && current(g) === g.human && c.nat === g.human && c.leave === undefined;

// ---- Prizes outside the town, and scorched earth (the grand campaign) -------------------------

/**
 * The prizes of a province: its oilfield, factory district, airfield, harbour and great rail junction, each with who
 * holds it, who has the use of it this month, and how long it is out of use if wrecked; and the player's scorched
 * earth order for the province. Null outside the grand campaign and where the province has none.
 */
export const prizesOf = (g: Game, region: number): PrizesView | null => prizesAt(g, region, g.human);

/** Provinces to mark on the map: a prize wrecked there (burning), or in the hands of an army that has not taken the town (seized). Under the fog, only ground the player can see. 'seen' is the player's sightOf where the caller has it already (the 3D table works it out once a picture). */
export function prizesOnMap(g: Game, seen?: Uint8Array): PrizeMark[] {
  const all = prizeMarks(g);
  if (!all.length || g.human === null || !fogOn(g, g.human)) return all;
  const sight = seen ?? sightOf(g, g.human);
  return all.filter((m) => sight[m.region] !== SIGHT.none || allied(g, g.human!, g.regions[m.region].ctrl));
}

/** What the player's armies earn from prizes they hold outside towns they have not yet taken. */
export const myPrizeIncome = (g: Game): PrizeIncome | null => (g.human === null ? null : prizeIncome(g, g.human));

/** Why the player cannot wreck a prize by order, or null if he can: it is a corps' work for the month. */
export function whyNotWreckPrize(g: Game, region: number, kind: PrizeKind): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotWreck(g, g.human, region, kind);
}

/** The player's order to wreck a prize his troops hold: a thorough job, and nobody has the use of it for months. */
export function orderWreck(g: Game, region: number, kind: PrizeKind): boolean {
  return g.human !== null && humanUp(g) && orderWreckPrize(g, g.human, region, kind);
}

/** Why the player cannot give a scorched earth order for a province, or null if he can: he must hold its town or the country round it. */
export function whyNotBurnOrder(g: Game, region: number): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  const r = g.regions[region];
  if (!r || !prizesAt(g, region, g.human)) return 'There is nothing there to burn.';
  if (r.ctrl !== g.human && fieldCtrl(g, r) !== g.human) return `${r.name} is not in your hands.`;
  return null;
}

/**
 * The player's standing order for one province if his troops are driven off it: 1 burn its prizes, -1 leave them
 * whole, 0 follow the nation's policy.
 */
export function orderBurn(g: Game, region: number, order: number): boolean {
  if (whyNotBurnOrder(g, region)) return false;
  setBurnOrder(g, g.human!, g.regions[region], order);
  return true;
}

/** The player's scorched earth policy: never, on conquered ground only, or everywhere. */
export const scorchPolicyNow = (g: Game): ScorchPolicy => (g.human === null ? 'never' : scorchPolicy(g, g.human));

/** The player sets his nation's scorched earth policy. A province's own order overrides it. */
export function orderScorchPolicy(g: Game, policy: ScorchPolicy): boolean {
  if (g.human === null || !humanUp(g) || g.flags.open !== 1) return false;
  setScorchPolicy(g, g.human, policy);
  return true;
}

// ---- Government and the war cabinet (the grand campaign) --------------------------------------

/**
 * A nation's government: its form, and for the player his war cabinet (four seats, the minister in each and who
 * else could hold it), his decrees, political capital, the cabinet's trust in him and the directive it has set.
 * With no nation named, the player's own. Null outside the grand campaign.
 */
export const governmentOf = (g: Game, nat: string | null = g.human): GovView | null => government(g, nat);

/**
 * A nation's reserve of men fit to bear arms (the grand campaign): what stands in it, what it opened with, the class
 * that comes of age each month under the government's call-up, and one word and one sentence for how it stands.
 * Replacements and every new corps are drawn from it. With no nation named, the player's own. Null outside the grand campaign.
 */
export const manpowerOf = (g: Game, nat: string | null = g.human): MenView | null => manpower(g, nat, nat ? govMul(g, nat, 'men') : 1);

/**
 * The player's say over the call-up (the grand campaign), for the panel that shows his reserve of men. `early`: what
 * calling the next class up early would bring today, in men, the months no class would then come of age, and why it
 * cannot be done now, or null if it can. `callUp`: his country's standing call-up, where it has one and is at war:
 * whether he holds it, the corps the next wave would raise at most, and `drafts`, the men a month that go to the
 * corps in the field in their place while it is held. Null outside the grand campaign.
 */
export interface CallUpView {
  early: { men: number; months: number; gap: number; why: string | null; text: string };
  callUp: { held: boolean; corps: number; divs: number; drafts: number; text: string } | null;
}
export function callUpOf(g: Game): CallUpView | null {
  const nat = g.human;
  if (nat === null || g.flags.open !== 1 || !g.nations[nat]) return null;
  const E = MANPOWER.early, mul = govMul(g, nat, 'men'), come = earlyClass(g, nat, mul), men = Math.round(come * MEN_PER_DIV);
  const why = !humanUp(g) ? 'It is not your month.' : whyNotEarly(g, nat);
  // What comes is as many months of classes as the reserve has room for, and the months without a class after it are as many.
  const one = classNow(g, nat, mul), months = one > 0 ? Math.max(1, Math.min(E.months, Math.round(come / one))) : E.months, gap = earlyGap(g, nat, come, mul);
  const mo = (k: number): string => (k === 1 ? 'a month' : `${k} months`);
  const wave = standingCallUp(g, nat);
  const drafts = wave ? Math.round(Math.min(wave.corps, raisable(g, nat, wave.divs)) * wave.divs * MEN_PER_DIV) : 0;
  const notRaised = wave ? `${wave.corps === 1 ? 'one new corps' : `${wave.corps} new corps`} a month ${wave.corps === 1 ? 'is' : 'are'} not raised` : '';
  return {
    early: { men, months, gap, why, text: men < 1000
      ? 'Calling the next class up early would bring nobody now: the reserve has no room for them.'
      : `Call up the next class early: about ${menWords(men)} men join the reserve at once, ${mo(months)} of classes together. No class then comes of age for ${mo(gap)}, because those boys are with the colours already: the same men, sooner.` },
    callUp: wave ? { ...wave, drafts, text: wave.held
      ? `The standing call-up is held: ${notRaised}, and the waves wait until you let it run. Their men go to the corps in the field instead: up to ${menWords(drafts)} a month over and above what the depots send, at no cost to the treasury. Once the corps are full the men wait in the reserve, and the army is no bigger for it.`
      : `The standing call-up is running: next month it raises up to ${wave.corps} new corps of ${wave.divs} divisions from the reserve, if the men are there. Hold it, and ${notRaised}: their men go as drafts to the corps in the field instead, over and above what the depots send. An army kept small that way can be outnumbered.` } : null,
  };
}

/** Why the player cannot call the next class up early, or null if he can. */
export function whyNotOrderEarlyClass(g: Game): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotEarly(g, g.human);
}

/** The player calls the next class up early: months of classes join the reserve now, and none comes of age for a while after. */
export function orderEarlyClass(g: Game): boolean {
  if (g.human === null || !humanUp(g)) return false;
  const come = callEarly(g, g.human, govMul(g, g.human, 'men'));
  if (come <= 0) return false;
  const gap = Math.max(1, classGapUntil(g, g.human) - g.turn);
  headline(g, 'info', `The next class is called up early: ${menWords(come * MEN_PER_DIV)} men join the reserve. No class comes of age for the next ${gap === 1 ? 'month' : `${gap} months`}.`, g.human);
  return true;
}

/** The player holds his country's standing call-up, or lets it run again. It takes hold from next month's wave. */
export function orderCallUp(g: Game, hold: boolean): boolean {
  if (g.human === null || !holdCallUp(g, g.human, hold)) return false;
  headline(g, 'info', hold ? 'The standing call-up is held: no new corps are raised until you let it run again, and the men of each wave go to the corps in the field as drafts instead.' : 'The standing call-up runs again: new corps are raised from the reserve each month.', g.human);
  return true;
}

/** True when the player has switched his war cabinet off (the grand campaign): governmentOf is then null, and no screen for it should show. */
export const cabinetIsOff = (g: Game): boolean => cabinetOff(g);

/**
 * Switches the player's war cabinet off or on, for a player who wants the war without the politics. It can be done
 * in any month and takes hold at once. Off: no ministers, decrees, trust or directives, and nothing they moved is
 * moved. On again: the cabinet sits as it was left and sets its next directive some months on.
 */
export function orderCabinet(g: Game, on: boolean): boolean {
  return g.human !== null && setCabinet(g, on);
}

/** True when the player has made his office secure (the grand campaign): his government cannot fall, whatever the cabinet's trust. */
export const officeIsSecure = (g: Game): boolean => officeSecure(g);

/**
 * Says whether the player's government can fall: a setting apart from the cabinet's switch, for a player who wants
 * the cabinet, its ministers and its directives but never to be turned out. It can be changed in any month. With
 * `canFall` false the cabinet's trust still moves and a cabinet that doubts him still withholds money and political
 * capital, but it never falls.
 */
export function orderOffice(g: Game, canFall: boolean): boolean {
  return g.human !== null && setCanFall(g, canFall);
}

/**
 * What switching the war cabinet off would settle today, line by line, with the trust each costs: a directive set
 * aside, a promise taken back, a general stood by. Show it before the player confirms. Empty where nothing is open.
 */
export const cabinetBillOf = (g: Game): { text: string; trust: number }[] => cabinetBill(g);

/** Why the player cannot put this minister in this seat of his war cabinet, or null if he can. */
export function whyNotAppointMinister(g: Game, seat: string, minister: string): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotAppoint(g, g.human, seat, minister);
}

/** The player appoints a minister: it costs political capital, and the seat cannot change again for some months. */
export function orderAppoint(g: Game, seat: string, minister: string): boolean {
  return g.human !== null && humanUp(g) && appoint(g, g.human, seat, minister);
}

/** Why the player cannot move a decree track to this step (one step up, or one down), or null if he can. */
export function whyNotOrderDecree(g: Game, track: string, step: number): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotDecree(g, g.human, track, step);
}

/** The player carries a decree, one step up for its price in political capital, or relaxes one, free. */
export function orderDecree(g: Game, track: string, step: number): boolean {
  return g.human !== null && humanUp(g) && decree(g, g.human, track, step);
}

// ---- The nation's footing (ruling 172; the grand campaign with the lever switched on) ----------------------------

/**
 * A country's footing as both games show it: the ledger's line, the four footings as a tally with their locks, and
 * the one move the card asks about (one step up, or back down). With no nation named, the player's own. Null with the
 * lever switched off.
 */
export const footingOf = (g: Game, nat: string | null = g.human): FootingView | null => footingView(g, nat);

/** Why the player cannot put his country on this footing now, or null if he can. */
export function whyNotOrderFooting(g: Game, to: FootingId): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotFooting(g, g.human, to);
}

/** The player puts his country on a footing: one step up, for its price in political capital, or back down, at once and free. */
export function orderFootingTo(g: Game, to: FootingId): boolean {
  return g.human !== null && humanUp(g) && orderFooting(g, g.human, to);
}

/** Why the player cannot set his directive aside, or null if he can. */
export function whyNotSetAsideDirective(g: Game): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotSetAside(g, g.human);
}

/** The player tells the cabinet its directive will not be carried out: the trust is lost at once, and the next one is set sooner. */
export function orderSetAside(g: Game): boolean {
  return g.human !== null && humanUp(g) && setAside(g, g.human);
}

// ---- Formations offered out of the player's own corps (ruling B8 of 25 November 2026; whole world only) -------------
// A dated row that would take corps of his off the map is an offer, never a silent removal (game.ts `rowOffers`). The
// month's offers are one list; he takes all, takes some, or declines, in his own month. None with the far side off.
// The end of a loan is in the same list (an offer with `loan`): a lent regiment its division awaits goes home only if
// he takes it.

/** The player's open offers: what each would form and where, and which of his corps it would take. */
export const formationOffers = (g: Game): RowOffer[] => rowOffers(g);
export type { RowOffer };

/** Why the player cannot answer an offer now, or null if he can. */
export function whyNotAnswerOffers(g: Game): string | null {
  return g.human === null || !humanUp(g) || onEve(g) ? 'It is not your month.' : null;
}

/** The player takes an offer: the formation is made as the record made it, out of the corps it names (a loan's end: the lent regiment is let go). Null where it was done, else the plain reason it was not; the offer then stands. */
export function orderTakeFormation(g: Game, id: number): string | null {
  return whyNotAnswerOffers(g) ?? takeOffer(g, id);
}

/** The player declines an offer: the formation never comes, and his corps stay as they are (a loan's end: the lent regiment stays his for good). */
export function orderDeclineFormation(g: Game, id: number): boolean {
  return whyNotAnswerOffers(g) === null && declineOffer(g, id);
}

/**
 * Call your shot. The enemy towns the player could promise his war cabinet today, the richest first, each with the
 * terms for every length of time he may allow himself: the odds on the day, how bold the cabinet reckons the promise,
 * what keeping it earns and what breaking it costs. Empty when he cannot make a promise now (governmentOf().promiseWhy says why).
 */
export const promiseTargetsOf = (g: Game, most = 8): PromiseTarget[] => promiseTargets(g, g.human, most);

/** The terms of promising one town in so many months, or null where no such promise can be reckoned: for a town picked on the map. */
export const promiseTermsOf = (g: Game, region: number, months: number): PromiseTerms | null => (g.human ? promiseTerms(g, g.human, region, months) : null);

/** Why the player cannot go to the cabinet with a promise now (or with this town, or in this time), or null if he can. */
export function whyNotOrderPromise(g: Game, region?: number, months?: number): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotPromise(g, g.human, region, months);
}

/** The player gives the cabinet his word: this town in his side's hands within so many months. One promise at a time. */
export function orderPromise(g: Game, region: number, months: number): boolean {
  return g.human !== null && humanUp(g) && promise(g, g.human, region, months);
}

/** Why the player cannot take his promise back, or null if he can. */
export function whyNotTakeBackPromise(g: Game): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotTakeBack(g, g.human);
}

/** The player takes his promise back before its day, for a share of what breaking it would cost. */
export function orderTakeBack(g: Game): boolean {
  return g.human !== null && humanUp(g) && takeBack(g, g.human);
}

/** The general the war cabinet is asking the player to dismiss, by id, or null (governmentOf().head has the rest). */
export const headWantedOf = (g: Game): number | null => headOf(g);

/** Why the player cannot stand by a general, or null while the cabinet is asking for one's dismissal. */
export function whyNotStandByGeneral(g: Game): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotStandBy(g, g.human);
}

/**
 * The player refuses the cabinet the general it wants dismissed (governmentOf().head). Its trust falls, and his
 * next battle settles it: won, he is vindicated for the rest of the war; lost, the cabinet was right. To give the
 * cabinet its head instead, dismiss him with orderSack.
 */
export function orderStandBy(g: Game): boolean {
  return g.human !== null && humanUp(g) && standBy(g, g.human);
}

/**
 * A province cut off from the rest of its country (the grand campaign): whose troops are shut in, since when, the
 * town whose stores feed them and the months those will last. Null where the province lies in no pocket.
 */
export function pocketAt(g: Game, region: number): PocketView | null {
  return pocketView(g, region);
}
export type { PocketView };

/** The town of a province as a fortress: what it holds, what it adds to its defenders, and how its siege stands. Null outside the grand campaign. */
export function townSiege(g: Game, region: number): SiegeView | null {
  const v = siegeView(g, region);
  if (!v) return v;
  // Under the fog an enemy's garrison is counted only where the player's side stands round the town or has the province in full view.
  const r = g.regions[region];
  const seen = g.human === null || !fogOn(g, g.human) || allied(g, g.human, r.ctrl) || (!!v.by && allied(g, g.human, v.by)) || sightOf(g, g.human)[region] === SIGHT.full;
  if (!seen) return { ...v, inside: -1 };
  // A town with no army in it, none in the country round it and nobody at its gates is held by its guard only: said first, with the men (ruling 175(c)).
  if (v.inside !== 0 || v.guard <= 0 || v.by || r.units.some((c) => !c.dead)) return v;
  const foe = g.human !== null && atWar(g, g.human, r.ctrl);
  return { ...v, lines: [`No army holds ${v.town}: only its town guard, about ${menWord(guardMen(g, r))} men.${foe ? ' An army you march in goes at the town as it arrives.' : ''}`, ...v.lines] };
}

/** The forces line of a province whose town no army holds (ruling 175(c), as the UI director placed it): a head, the
 *  guard's strength, and for an enemy's town what a march in meets. Null where an army is in the town or the country
 *  round it, where somebody stands at its gates, where the town has no guard, and where the fog hides who is inside. */
export interface GuardOnly { town: string; men: number; head: string; strength: string; march: string }
export function guardOnly(g: Game, region: number): GuardOnly | null {
  const v = townSiege(g, region), r = g.regions[region];
  if (!v || !r || v.inside !== 0 || v.guard <= 0 || v.by || r.units.some((c) => !c.dead)) return null;
  const men = guardMen(g, r);
  const foe = g.human !== null && atWar(g, g.human, r.ctrl);
  return { town: v.town, men, head: 'Held by its town guard only', strength: `About ${menWord(men)} men, and no army.`, march: foe ? 'An army you march in goes at the town as it arrives.' : '' };
}

/** Every siege the player knows of: his own, his allies', and any other on ground he can see. 'seen' as for prizesOnMap. */
export function siegesInSight(g: Game, seen?: Uint8Array): SiegeView[] {
  const all = siegesNow(g);
  if (!all.length || g.human === null || !fogOn(g, g.human)) return all;
  const sight = seen ?? sightOf(g, g.human);
  return all.filter((v) => allied(g, g.human!, v.holder) || (v.by !== null && allied(g, g.human!, v.by)) || sight[v.region] !== SIGHT.none);
}

/** The garrison inside a province's town, one card for each nation with corps in it, as the player knows them. */
export function garrisonsAt(g: Game, region: number): ArmyCard[] {
  const r = g.regions[region];
  if (!r || !r.garr) return [];
  const nats = [...new Set(r.garr.filter((c) => !c.dead).map((c) => c.joint ?? c.nat))];
  return nats.map((n) => armyCard(g, region, n, g.human, true)).filter((a): a is ArmyCard => a !== null);
}

/** The player's own corps inside a province's town. */
export const myGarrison = (g: Game, region: number): Corps[] => (g.human === null ? [] : garrisonOf(g.regions[region]).filter((c) => c.nat === g.human && !c.dead));

/** Why the player cannot send this corps into its province's town, or null if he can. */
export function whyNotGarrisonTown(g: Game, c: Corps): string | null {
  if (!townOrder(g, c)) return 'That corps takes no orders from you this month.';
  return whyNotGarrison(g, c);
}

/** Sends a corps into the town of the province it stands in. It does nothing more this month. */
export function orderGarrison(g: Game, c: Corps): boolean {
  return whyNotGarrisonTown(g, c) === null && garrison(g, c);
}

/** Why the player cannot bring this corps out of its town into the field, or null if he can. */
export function whyNotLeaveTown(g: Game, c: Corps): string | null {
  if (!townOrder(g, c)) return 'That corps takes no orders from you this month.';
  return whyNotTakeField(g, c);
}

/** Brings a corps of the garrison out into the field. */
export function orderTakeField(g: Game, c: Corps): boolean {
  return whyNotLeaveTown(g, c) === null && takeField(g, c);
}

/**
 * Where a corps of the player's garrison could go this month if it came out of its town first (release 2, Brian, 24
 * November 2026: "if a unit is dug in and i click them to attack and move I want them to over ride their orders
 * insted of me having to press a button"). The 3D game sends such a corps straight out of the town where it is
 * ordered to march or attack: this is its reach from the open country of its province, with the move that coming
 * out costs. Empty where it may not come out (shut in by a siege, no room outside, or not the player's to order).
 */
export function reachOutOfTown(g: Game, c: Corps): Map<number, Reach> {
  if (c.town === undefined || whyNotLeaveTown(g, c) !== null) return new Map();
  return reach(g, { ...c, town: undefined, ap: apOutOfTown(g, c) });
}

/** The moves a corps of the player's would have for an order this month: a garrison that may come out has what it keeps once out. */
export const apForOrders = (g: Game, c: Corps): number => (c.town !== undefined && whyNotLeaveTown(g, c) === null ? apOutOfTown(g, c) : c.ap);

/**
 * The player's corps round a town that could go into an assault this month. A town is assaulted once a month, and
 * every corps round it can go in, however much of its month it has used: troops that have just won the country
 * round the town may follow the beaten defenders straight to the walls.
 */
export function stormCandidates(g: Game, region: number): Corps[] {
  const r = g.regions[region];
  if (!r || g.human === null) return [];
  return r.units.filter((c) => canOrder(g, c) && !c.town && atWar(g, c.nat, r.ctrl));
}

/** Why the player cannot storm the town of this province, or null if he can. */
export function whyNotStormTown(g: Game, region: number): string | null {
  const r = g.regions[region];
  if (!r) return 'Pick a province.';
  if (g.human === null || current(g) !== g.human || onEve(g)) return 'It is not your month.';
  return whyNotStorm(g, r, g.human) ?? (stormCandidates(g, region).length ? null : 'None of your corps round the town takes your orders this month.');
}

/**
 * What the staff can say of an assault on the town before it is ordered: the odds (a band where the garrison is
 * not identified), the chance of taking it, and the Battle Ticket. `corps` are the player's corps to send, all that
 * can go if left out.
 */
export function stormOutlook(g: Game, region: number, corps?: Corps[]): Outlook | null {
  const r = g.regions[region];
  if (!r || whyNotStormTown(g, region)) return null;
  const a = previewStorm(g, corps ? corps.filter((c) => canOrder(g, c)) : stormCandidates(g, region), r);
  return a ? outlookOf(g, a, r) : null;
}

/** Storms the town: once a month for each town. It costs more men than a fight in the open, and an assault the month the army comes up is a hasty one. */
export function orderStorm(g: Game, region: number, corps?: Corps[]): BattleReport | null {
  const r = g.regions[region];
  if (!r || whyNotStormTown(g, region)) return null;
  return storm(g, corps ? corps.filter((c) => canOrder(g, c)) : stormCandidates(g, region), r);
}

// ---- Marching in is the attack (ruling 175(c)) ----------------------------------------------------

/** What an army ordered into an enemy province with no enemy corps in its field will meet there (the grand campaign). */
export interface MarchOutlook {
  region: number;
  /** The town, and the size of the place in a word. */
  town: string; size: TownSize; sizeWord: string;
  /**
   * 'open': nothing holds the place, and it falls with the move. 'gates': only its own guard holds it, and gives it
   * up to this force without a fight: it falls with the move. 'guard': only its guard holds it, and will fight: the
   * army goes at the town as it arrives. 'garrison': corps hold the town: the march closes round it and a siege
   * begins. 'unknown': the player's reports do not say what is inside: the march finds out.
   */
  meets: 'open' | 'gates' | 'guard' | 'garrison' | 'unknown';
  /** The town's own guard, in divisions' worth and in men: 0 where it has none. */
  guard: number; guardMen: number;
  /** The corps that make the march this month, of those offered. */
  going: number[];
  /** What it comes to, in a sentence. */
  line: string;
  /** Where the guard will fight: the assault as it will stand when the army arrives, off the march (a hasty one). Null otherwise. */
  outlook: Outlook | null;
  /** The same assault made ready: as it would stand next month, with the army round the town and nothing else changed. Null otherwise. */
  prepared: Outlook | null;
  /** Which of the two orders the card lights where the guard fights: the storm on arrival when its forecast is favoured, else the siege (ruling 180). */
  favours: 'storm' | 'siege' | null;
  /** Where the guard fights, the sentence that stands under each order's forecast: what a failed storm costs, and what laying siege first comes to. '' otherwise. */
  ifStorm: string;
  ifSiege: string;
}
/** The chance from which a forecast's verdict reads "The odds favour you". The cards print their verdicts from this figure, and whatever a verdict lights moves with it (ruling 180). */
export const FAVOURED = 0.6;
/**
 * The staff's verdict on a forecast, in the words every card prints: one function, so that the browser's cards and
 * the 3D table's never differ. `band` is the verdict as a key, `word` the line, and `tone` how it is coloured
 * ('win', '' or 'loss'). `of` is what is forecast: an attack in the field, the storm of a town, or a garrison's
 * sortie. Where the defence is not fully known the verdict is the staff's, of the band.
 */
export interface Verdict { band: 'certain' | 'favoured' | 'even' | 'long' | 'none'; word: string; tone: 'win' | '' | 'loss' }
export function verdictOf(look: Outlook, of: 'attack' | 'storm' | 'sortie' = 'attack'): Verdict {
  if (look.exact) return verdictAt(look.chance, of);
  const p = look.chance;
  const tone = p >= FAVOURED ? 'win' : p >= 0.3 ? '' : 'loss';
  if (look.chanceLo >= 0.95) return { band: 'certain', word: `${of === 'sortie' ? 'Breaking out' : 'Victory'} looks certain`, tone };
  if (look.chanceHi <= 0) return { band: 'none', word: `${of === 'attack' ? 'This attack' : 'This'} looks hopeless`, tone };
  if (p >= FAVOURED) return { band: 'favoured', word: 'The staff thinks the odds favour you', tone };
  return p >= 0.3 ? { band: 'even', word: 'The staff cannot call it', tone } : { band: 'long', word: 'The staff thinks it a long shot', tone };
}
/** The verdict on a chance that is known exactly (every defender identified). */
export function verdictAt(p: number, of: 'attack' | 'storm' | 'sortie' = 'attack'): Verdict {
  const tone = p >= FAVOURED ? 'win' : p >= 0.3 ? '' : 'loss';
  if (p >= 0.95) return { band: 'certain', word: `${of === 'sortie' ? 'Breaking out' : 'Victory'} is all but certain`, tone };
  if (p >= FAVOURED) return { band: 'favoured', word: 'The odds favour you', tone };
  if (p >= 0.3) return { band: 'even', word: 'It could go either way', tone };
  return p > 0 ? { band: 'long', word: 'A long shot', tone } : { band: 'none', word: `${of === 'attack' ? 'This attack' : 'This'} will fail`, tone };
}
/** True where a forecast's verdict reads "The odds favour you" or better, as its card prints it. */
export function favoured(look: Outlook): boolean {
  const b = verdictOf(look).band;
  return b === 'certain' || b === 'favoured';
}
/** How far over the forecast's own figure an attacker's losses may run and the result still count as forecast (ruling 204): half as much again. */
export const AS_FORECAST_LOSS = 1.5;
/**
 * Ruling 204: a won battle that went as forecast is told on the map and opens no card. Four things together: the
 * attack won; the verdict on the order's card was "certain" or "the odds favour you"; it was not a Pyrrhic
 * victory; and the attacker's losses were no more than half as much again as the card said a win would cost.
 * `look` is the forecast the order's card showed, taken before the order was given. One function, so that the
 * browser's page and the 3D table never differ on which results are news.
 */
export function asForecast(look: Outlook | null | undefined, rep: BattleReport, of: 'attack' | 'storm' | 'sortie' = 'attack'): boolean {
  if (!look || !rep.taken) return false;
  const band = verdictOf(look, of).band;
  if (band !== 'certain' && band !== 'favoured') return false;
  if (rep.level === 'Pyrrhic victory') return false;
  const said = look.cost.won;
  if (!said) return false;
  const lost = Math.max(rep.attLost, rep.attackers.reduce((s, u) => s + u.lost, 0));
  return lost <= AS_FORECAST_LOSS * said.ours;
}

/**
 * Men after "about", rounded (the UI director's HUD read, 22 November: "about 5,000 men", never "about 4,973 men"): to
 * the nearest thousand from 1,000, the nearest hundred under it. Every line that uses it says "about" before it.
 */
export const menWord = (n: number): string => (n >= 1000 ? Math.round(n / 1000) * 1000 : n >= 100 ? Math.round(n / 100) * 100 : Math.round(n)).toLocaleString('en-US');

/**
 * A forecast read off a copy of the campaign, kept with the save the copy was made from. The pointer resting over a
 * town asks the same question again and again, and the card asks it once more when the army is sent: the copy is
 * the dear part (a whole load of the campaign). The copy is made from the save alone, so the same save and the same
 * question make the same copy and the same answer, and a kept answer is that answer. Only the last one is kept, and
 * each asking is handed its own copy of it.
 */
let kept: { text: string; ask: string; out: unknown } | null = null;
const plainCopy = <T>(x: T): T => {
  if (Array.isArray(x)) return x.map(plainCopy) as T;
  if (x && typeof x === 'object') { const o: Record<string, unknown> = {}; for (const k of Object.keys(x)) o[k] = plainCopy((x as Record<string, unknown>)[k]); return o as T; }
  return x;
};
function offCopy<T>(g: Game, ask: string, read: (h: Game) => T): T | undefined {
  const text = saveGame(g);
  if (kept && kept.ask === ask && kept.text === text) return plainCopy(kept.out as T);
  const h = loadGame(text);
  if (!h) return undefined;
  const out = read(h);
  kept = { text, ask, out: plainCopy(out) };
  return out;
}

/**
 * What marching these corps into a province will meet, before anything is ordered: ordering an army onto enemy
 * ground where no enemy corps stands is the order to take the place. Null where the march is no such thing: the
 * ground is a friend's or a neutral's, an enemy army stands in its field (that is an attack), an army already
 * stands round its town (the corps only join the siege), or none of the corps can get there this month. Where only
 * the town's guard will fight, the forecast is read off a copy of the campaign on which the march has been made, so
 * it is the assault exactly as it will be fought. Not for every square a pointer crosses: for the card.
 */
export function marchOutlook(g: Game, corps: Corps[], to: number): MarchOutlook | null {
  const r = g.regions[to], me = g.human;
  if (!r || me === null || !siegeRules(g) || current(g) !== me || onEve(g)) return null;
  if (!atWar(g, me, r.ctrl) || hostileUnits(g, r, me).length || r.units.some((c) => !c.dead && atWar(g, c.nat, r.ctrl))) return null;
  const able = corps.filter((c) => { if (!canOrder(g, c) || c.reg === to) return false; const e = reach(g, c).get(to); return !!e && !e.attack; });
  if (!able.length) return null;
  const v = townSiege(g, to);
  if (!v) return null;
  const base = { region: to, town: v.town, size: v.size, sizeWord: v.sizeWord, guard: v.guard, guardMen: guardMen(g, r), going: able.map((c) => c.id), outlook: null, prepared: null, favours: null, ifStorm: '', ifSiege: '' };
  const guardWords = `its town guard, about ${menWord(base.guardMen)} men`;
  if (v.inside < 0) return { ...base, meets: 'unknown', line: `Your reports do not say what holds ${v.town}${v.guard > 0 ? ` beside ${guardWords}` : ''}. Your army marches up to it and finds out.` };
  if (v.inside > 0) return { ...base, meets: 'garrison', line: `${v.inside === 1 ? 'A corps holds' : `${v.inside} corps hold`} ${v.town}: your army closes round it and the siege begins.` };
  if (!townHeld(g, r)) return { ...base, meets: 'open', line: `Nothing holds ${v.town}: ${r.name} is yours as your army marches in.` };
  // Only the guard: the march is made on a copy of the campaign, and the copy says what follows.
  const order = marchOrder(g, able).map((c) => c.id);
  const read = offCopy(g, `march ${to} ${order.join(',')}`, (h) => {
    const there: number[] = [];
    for (const id of order) { const hc = h.corps.find((x) => x.id === id); if (hc && orderMove(h, hc, to) > 0 && hc.reg === to) there.push(id); }
    if (!there.length || allied(h, me, h.regions[to].ctrl)) return { there, gates: there.length > 0, outlook: null, prepared: null };
    const outlook = stormOutlook(h, to);
    // Made ready: the same army round the same town, with the month it came up behind it.
    h.flags[`siege:${to}`] = h.turn - 1;
    return { there, gates: false, outlook, prepared: stormOutlook(h, to) };
  });
  if (!read || !read.there.length) return null;
  const { there, outlook, prepared } = read;
  if (read.gates) return { ...base, going: there, meets: 'gates', line: `Only ${guardWords}, holds ${v.town}, and against this force it opens its gates: ${r.name} is yours as your army marches in.` };
  return {
    ...base, going: there, meets: 'guard', outlook, prepared, favours: outlook && favoured(outlook) ? 'storm' : 'siege',
    ifStorm: IF_STORM,
    ifSiege: 'Nothing is risked this month. The storm is then yours to order from the siege: next month it goes in made ready, if nothing else changes.',
    line: `Only ${guardWords}, holds ${v.town}, and it will fight: your army goes at the town as it arrives.` };
}

/** What an order to take a place came to. */
/**
 * The storm of a town an army already stands round, as the choice its card puts (ruling 182): the storm now with
 * its forecast, or keeping up the siege. `hasty` is true where the army only came up this month; `prepared` is then
 * the same storm as it would stand next month made ready, with nothing else changed (asked of a copy of the game:
 * nothing is changed here). `favours` is the order the card lights, by the verdict of the storm's own forecast.
 * `ifStorm` and `ifWait` are the sentences that stand under the two orders; `ifWait` is '' where the rules have
 * nothing to say of waiting. Null where no storm can be ordered (whyNotStormTown says why).
 */
export interface StormChoice {
  region: number;
  outlook: Outlook;
  hasty: boolean;
  prepared: Outlook | null;
  favours: 'storm' | 'siege';
  ifStorm: string;
  ifWait: string;
}
const IF_STORM = 'If the storm fails, your corps pay more dearly than they would in the open, and the town guard is thinned too.';
const carryWord = (o: Outlook): string => `${o.exact ? '' : 'about '}${o.odds.toFixed(2)} to 1, about ${Math.min(99, Math.max(1, Math.round(o.chance * 100)))}% to carry it`;
export function stormChoice(g: Game, region: number): StormChoice | null {
  const outlook = stormOutlook(g, region);
  if (!outlook) return null;
  const hasty = g.flags[`siege:${region}`] === g.turn && outlook.lines.some((l) => l.label === 'A hasty assault');
  let prepared: Outlook | null = null;
  if (hasty) prepared = offCopy(g, `storm ${region}`, (h) => { h.flags[`siege:${region}`] = h.turn - 1; return stormOutlook(h, region); }) ?? null;
  const v = townSiege(g, region);
  // Made ready, the wait is a better storm. Otherwise it is what the siege itself is doing: the stores, the ring, the sea.
  const ifWait = prepared ? `Made ready, a month later: ${carryWord(prepared)}, if nothing else changes.` : v && v.by ? v.lines[v.guard > 0 ? 2 : 1] ?? '' : '';
  return { region, outlook, hasty, prepared, favours: favoured(outlook) ? 'storm' : 'siege', ifStorm: IF_STORM, ifWait };
}

export interface TakeResult {
  /** How many of the corps marched. */
  moved: number;
  /** The province is the player's side's: it fell with the move, or to the assault. */
  fell: boolean;
  /** The assault on the town, where one was made. */
  report: BattleReport | null;
  /** What still holds the town now that the army stands round it: null where the place has fallen, or nobody marched. */
  stands: 'guard' | 'garrison' | null;
}

/**
 * The order to take a place: marches the corps in, and where only the town's guard holds out, storms the town as
 * they arrive (all the player's corps round it go in, as an assault's card shows). With `assault` false the army
 * only marches in and lays the siege: the assault is then made ready, and can go in next month at full weight.
 */
export function orderTake(g: Game, corps: Corps[], to: number, assault = true): TakeResult {
  const r = g.regions[to], me = g.human;
  const none: TakeResult = { moved: 0, fell: false, report: null, stands: null };
  if (!r || me === null) return none;
  let moved = 0, arrived = 0;
  for (const c of marchOrder(g, corps)) if (canOrder(g, c) && orderMove(g, c, to) > 0) { moved++; if (c.reg === to) arrived++; }
  if (!moved) return none;
  if (!arrived) return { moved, fell: false, report: null, stands: null };
  if (allied(g, me, r.ctrl)) return { moved, fell: true, report: null, stands: null };
  const stands = garrisonOf(r).length ? 'garrison' : 'guard';
  if (!assault || stands !== 'guard' || !siegeRules(g) || whyNotStormTown(g, to)) return { moved, fell: false, report: null, stands };
  const report = orderStorm(g, to);
  const fell = allied(g, me, r.ctrl);
  return { moved, fell, report, stands: fell ? null : 'guard' };
}

/** Why the player's garrison cannot sally out against the army round its town, or null if it can. */
export function whyNotSallyOut(g: Game, region: number): string | null {
  const r = g.regions[region];
  if (!r) return 'Pick a province.';
  if (g.human === null || current(g) !== g.human || onEve(g)) return 'It is not your month.';
  return whyNotSally(g, r, g.human);
}

/** What the staff can say of a sortie before it is ordered. */
export function sallyOutlook(g: Game, region: number): Outlook | null {
  const r = g.regions[region];
  if (!r || whyNotSallyOut(g, region)) return null;
  const a = previewSally(g, r, g.human!);
  return a ? outlookOf(g, a, r) : null;
}

/** The garrison comes out against the army round the town: win, and the siege is raised. One sortie a month from each town. */
export function orderSally(g: Game, region: number): BattleReport | null {
  const r = g.regions[region];
  if (!r || whyNotSallyOut(g, region)) return null;
  return sally(g, r, g.human!);
}

// ---- Joint armies: an ally's corps marching with the player's (the grand campaign) ---------------------------

/** An ally's field corps standing in a province with the player's army: whether it could join, and why not if it cannot. */
export interface JoinOffer { corps: number; nat: string; name: string; kind: Corps['kind']; divs: number; why: string | null }

/** Why the player cannot ask this corps to join his army now, or null if he can. */
export function whyNotJoinArmy(g: Game, c: Corps): string | null {
  if (!humanUp(g)) return 'It is not your month.';
  return whyNotJoin(g, c, g.human);
}

/** Every ally's field corps in a province that is not already with the player's army, with whether each could join it. */
export function joinOffers(g: Game, region: number): JoinOffer[] {
  const me = g.human, r = g.regions[region];
  if (me === null || !r || g.flags.open !== 1 || !r.units.some((u) => u.nat === me && u.kind !== 'fort' && !u.dead)) return [];
  return r.units.filter((c) => !c.dead && c.nat !== me && c.joint === undefined && c.kind !== 'fort' && allied(g, c.nat, me))
    .map((c) => ({ corps: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, why: whyNotJoinArmy(g, c) }));
}

/** Asks an ally's corps to march with the player's army in its province. */
export function orderJoin(g: Game, c: Corps): boolean {
  return humanUp(g) && joinArmy(g, c, g.human);
}

/** Asks every ally's corps in a province that can to join the player's army there. Returns how many did. */
export function orderJoinAll(g: Game, region: number): number {
  if (!humanUp(g)) return 0;
  let n = 0;
  // One at a time: each that joins counts against what its country can still spare.
  for (const c of [...(g.regions[region]?.units ?? [])].sort((a, b) => b.divs * b.str - a.divs * a.str || a.id - b.id)) if (joinArmy(g, c, g.human)) n++;
  return n;
}

/** Sends an ally's corps back from the player's army to its own command. */
export function orderSendBack(g: Game, c: Corps): boolean {
  return humanUp(g) && c.joint === g.human && leaveArmy(g, c);
}

/** What each ally has lent the player and how much more it could: for a list of the joint armies. */
export function lending(g: Game): { nat: string; lent: number; limit: number; corps: number[] }[] {
  const me = g.human;
  if (me === null || g.flags.open !== 1) return [];
  return Object.values(g.nations).filter((n) => n.alive && n.id !== me && allied(g, n.id, me))
    .map((n) => { const cs = lentBy(g, n.id).filter((c) => c.joint === me); return { nat: n.id, lent: cs.length, limit: lendLimit(g, n.id), corps: cs.map((c) => c.id) }; })
    .filter((x) => x.lent > 0 || x.limit > 0);
}

/** The ally's corps in a province that would join the player's army if asked. */
export const joinable = (g: Game, region: number): Corps[] => (humanUp(g) ? couldJoin(g, region, g.human) : []);

/**
 * The player's standing order for a corps that is attacked: stand and fight, dig in where it is, or give ground
 * when the blow would break it. Digging takes a month spent on the spot, so the order pays from the month after.
 */
export function orderStance(g: Game, c: Corps, posture: Posture): boolean {
  if (!canOrder(g, c)) return false;
  if (posture === 'back') c.yields = true; else delete c.yields;
  // (In the grand campaign whoever stands a month digs in unasked: only an order to give ground costs an army its trenches.)
  if (posture === 'dig') c.digs = true; else { delete c.digs; if (posture === 'back' || !(g.flags.open === 1 && DEFENCE.entrench.auto)) delete c.dug; }
  return true;
}

/** The standing order a corps is under. */
export const postureOf = (c: Corps): Posture => (c.yields ? 'back' : c.digs ? 'dig' : 'stand');

/** What the player's engineer corps could do where it stands this month (the grand campaign). */
export function engineerJobs(g: Game, c: Corps): JobOffer[] {
  return c.nat === g.human ? jobsFor(g, c) : [];
}

/** Sets one of the player's engineer corps to work for the month. */
export function orderEngineers(g: Game, c: Corps, job: JobKind): boolean {
  return ownOrder(g, c) && orderJob(g, c, job);
}

/** Takes one of the player's engineer corps off the work it was given this month. */
export function orderStopWork(g: Game, c: Corps): boolean {
  return c.nat === g.human && humanUp(g) && cancelJob(g, c);
}

/** Why the player cannot blow the bridges between two regions from the first of them, or null if they can. */
export function whyNotBlowBridges(g: Game, from: number, to: number): string | null {
  const x = crossingOf(from, to);
  if (!x) return 'There is no river between those two.';
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotBlow(g, g.human, x, from);
}

/** The player's order to blow the bridges between a region their troops stand in and its neighbour across the river. */
export function orderBlowBridges(g: Game, from: number, to: number): boolean {
  const x = crossingOf(from, to);
  return !!x && g.human !== null && humanUp(g) && blowBridges(g, g.human, x, from);
}

/** Why the player cannot tear up the railway of a province, or null if they can (the grand campaign). */
export function whyNotCutRailway(g: Game, region: number): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotCutRail(g, g.human, region);
}

/** The player's order to tear up the railway of a province their troops stand in. Only engineers can relay it. */
export function orderCutRailway(g: Game, region: number): boolean {
  return g.human !== null && humanUp(g) && tearUp(g, g.human, region);
}

export function orderRail(g: Game, c: Corps, to: number): boolean {
  return canOrder(g, c) && railMove(g, c, g.regions[to]);
}

/** Friendly ports this corps could be shipped to this month. */
export function shipTargets(g: Game, c: Corps): Region[] {
  const from = g.regions[c.reg];
  if (g.nations[c.nat].sealift <= 0 || !from.port || c.kind === 'fort' || c.ap < Math.min(4, c.apMax)) return [];
  return g.regions.filter((r) => r.port && r.id !== from.id && canShip(g, c, r));
}

/** Enemy coasts the player's chosen corps could land on from the port they stand in. */
export function landTargets(g: Game, cs: Corps[]): Region[] {
  return cs.length && !onEve(g) && cs.every((c) => ownOrder(g, c)) ? landingTargets(g, cs) : [];
}

/** The odds and the dangers of a landing, before the order is given. */
export function previewLanding(g: Game, cs: Corps[], to: number): LandingPlan {
  return planLanding(g, cs, g.regions[to]);
}

/** Sends the chosen corps across the sea to land on an enemy coast. */
export function orderLand(g: Game, cs: Corps[], to: number): Landing {
  if (!cs.length || onEve(g) || !cs.every((c) => ownOrder(g, c))) return { ok: false, why: cs.some((c) => c.joint !== undefined) ? 'An ally\'s corps does not sail under your orders: send the landing with your own.' : 'Those corps cannot be given orders now.' };
  const beach = g.regions[to], nat = cs[0].nat;
  const res = landCorps(g, cs, beach);
  // A harbour the player takes from the sea is a second front his allies' staffs know of: one that ships its army
  // across an ocean puts its corps into that harbour beside his (staff.ts, `frontAshore`). The first stands while it lasts.
  if (res.ok && res.taken && !res.air && beach.port && g.flags.open === 1 && !lodgment(g, nat)) g.flags[`st:lodged:${nat}`] = beach.id + 1;
  return res;
}

/**
 * The crossings open to the player (the grand campaign): for each shore an enemy holds that his army cannot reach
 * by land, the safest way over the water, what bars it today, why it is as dangerous as it is and what would make
 * it safer. The one he has named as his plan comes first.
 */
export function crossings(g: Game, short = false): CrossingOutlook[] {
  return g.human && g.nations[g.human].alive ? crossingsOf(g, g.human, { short }) : [];
}

/**
 * What the player's staff says of the crossing he has named as his plan: still preparing it and what is wanting,
 * prepared but held back by the landing card's warning (the order to sail under it is his own), or ready. Null
 * with no crossing named.
 */
export function crossingStaffWord(g: Game): CrossingWord | null {
  return g.human && g.nations[g.human].alive ? crossingWord(g, g.human) : null;
}

/**
 * Names a crossing as the player's plan: his admirals stand a fleet off that beach and his air staff fights for the
 * sky over it, where he has handed them the navy and the air war. With no beach, the plan is dropped. The landing
 * itself is always his own order.
 */
export function orderCrossingPlan(g: Game, from: number | null, to: number | null): boolean {
  const me = g.human;
  if (!me || onEve(g) || current(g) !== me) return false;
  return planCrossing(g, me, from === null ? null : g.regions[from] ?? null, to === null ? null : g.regions[to] ?? null);
}

/** The player's shipping for armies: corps a month, what is left this month, the troopships on the slips and when they are ready. */
export function liftOf(g: Game): LiftView | null {
  return g.human && g.nations[g.human].alive ? liftView(g, g.human) : null;
}

const myFleet = (g: Game, id: number): Fleet | null => {
  const f = g.fleets.find((x) => x.id === id);
  return f && f.nat === g.human && current(g) === f.nat ? f : null;
};

/**
 * A fleet the player has given an order himself is his from then on: where his admirals run the navy for him they
 * leave it where he sent it, and do not order it back to its old beat next month (1.5.1; Brian, 24 November 2026
 * 18:32Z: "when i move my navy they go right back to where they were before the next turn").
 */
const hisOrder = (f: Fleet, ok: boolean): boolean => {
  if (ok) f.ordered = true;
  return ok;
};

/** Sends one of the player's fleets to a sea to patrol or raid, or back to port. */
export function orderFleet(g: Game, id: number, zone: number, stance: Stance): boolean {
  const f = myFleet(g, id);
  // (Release 2, the far chart: a fleet in passage to a new base sent back to harbour turns back, to the one it left.)
  const ok = !!f && !heldAfterAttack(g, f) && hisOrder(f, f && inPassage(f) ? stance === 'port' && turnBack(g, f) : sail(g, f, zone, stance));
  // A task force given a new job keeps that one as its standing order; sent into harbour, it is released (release 2).
  if (ok) retask(g, f!);
  // A new order ends a shadow (release 2, shadow.ts).
  if (ok) stopShadow(f!);
  return ok;
}

/** Moves one of the player's fleets to a new home port. */
export function orderRebase(g: Game, id: number, region: number): boolean {
  const f = myFleet(g, id);
  // (A fleet in passage is turned back, not sent on elsewhere: release 2, the far chart. One waiting at sea with no
  // harbour it could reach makes for one he names, once one opens to it: the ruling of 15:48Z, 3.)
  const ok = !!f && !heldAfterAttack(g, f) && (!inPassage(f) || adrift(f)) && hisOrder(f, rebase(g, f, g.regions[region]));
  if (ok) stopShadow(f!);
  // (Moved where the staff's book wanted it, the move that waited for him is made: navy.ts, bookWaits.)
  if (ok && f!.planBase?.base === g.regions[region].name) delete f!.planBase;
  return ok;
}

/**
 * Scuttles one of the player's fleets that waits at sea with no harbour it can reach (release 2, the far chart; the
 * design director's ruling of 15:48Z, 25 November 2026, 3: "he may scuttle it"). False for any other fleet.
 */
export function orderScuttle(g: Game, id: number): boolean {
  const f = myFleet(g, id);
  return !!f && scuttleFleet(g, f);
}

/** True for one of the player's fleets that he has ordered himself, which his admirals leave where he sent it. */
export const fleetKeptByHand = (g: Game, id: number): boolean => g.fleets.some((f) => f.id === id && f.nat === g.human && f.ordered === true);

/** Hands one of the player's fleets back to his admirals: they sail it again from their next month's orders. A task force is released. */
export function handBackFleet(g: Game, id: number): boolean {
  const f = myFleet(g, id);
  if (!f || !f.ordered) return false;
  delete f.ordered;
  delete f.task;
  delete f.shadow;
  return true;
}

/** Why one of the player's fleets cannot be set to shadow an enemy fleet now, or null (release 2, shadow.ts). */
export const whyNotShadowFleet = (g: Game, id: number, target: number): string | null => whyNotShadow(g, id, target);

/** Sets one of the player's fleets to shadow an enemy fleet: it follows it and reports where it goes (release 2, shadow.ts). */
export function orderShadowFleet(g: Game, id: number, target: number): boolean {
  const f = myFleet(g, id);
  if (!f || heldAfterAttack(g, f)) return false;
  const ok = orderShadow(g, id, target);
  if (ok) delete f.task;
  return ok;
}

/** Why one of the player's fleets cannot lay a field of mines in sea `zone` now, or null (release 2, mines.ts). */
export const whyNotMinesFleet = (g: Game, id: number, zone: number): string | null => whyNotMines(g, id, zone);

/** One of the player's fleets sails to sea `zone` and lays a field of mines there (release 2, mines.ts). A new order: it ends a shadow. */
export function orderMinesFleet(g: Game, id: number, zone: number): boolean {
  const f = myFleet(g, id);
  if (!f || heldAfterAttack(g, f)) return false;
  const ok = orderMines(g, id, zone);
  if (ok) { delete f.task; stopShadow(f); }
  return ok;
}

/**
 * One of the player's carrier fleets sails to strike an enemy harbour from the air (release 2, harbourstrike.ts): to the
 * nearest sea from which its aircraft reach it, its wings sent at the harbour to fly with the month's air war. A new
 * order of his own: the fleet is his from then on, and it ends a shadow and a task force's standing job.
 */
export function orderCarrierStrikeFleet(g: Game, id: number, region: number): CarrierStrikeResult | null {
  const f = myFleet(g, id);
  if (!f || heldAfterAttack(g, f)) return null;
  const done = orderCarrierStrike(g, id, region);
  if (done) { hisOrder(f, true); delete f.task; stopShadow(f); }
  return done;
}

/** Ends one of the player's fleets' shadow: it stays where it is, under his orders (release 2, shadow.ts). */
export function stopShadowFleet(g: Game, id: number): boolean {
  const f = myFleet(g, id);
  return !!f && stopShadow(f);
}

/** Why one of the player's corps cannot be ordered to hold as a rearguard now, or null (release 2, rearguard.ts). */
export const whyNotRearguardOf = (g: Game, id: number): string | null => whyNotRearguard(g, g.corps.find((c) => c.id === id && !c.dead));

/** Orders one of the player's corps to hold where it stands as a rearguard until his next month (release 2, rearguard.ts). */
export function orderHoldRear(g: Game, id: number): boolean {
  const c = g.corps.find((x) => x.id === id && !x.dead);
  return !!c && orderRearguard(g, c);
}

/** Stands one of the player's rearguards down: in the month it was ordered, it has its marching back (release 2). */
export function orderStandDown(g: Game, id: number): boolean {
  const c = g.corps.find((x) => x.id === id && !x.dead && x.nat === g.human);
  return !!c && current(g) === c.nat && standRearDown(g, c);
}

/** Why one of the player's fleets cannot be made a task force now, or null (release 2, taskforce.ts). */
export const whyNotTaskForce = (g: Game, id: number): string | null => whyNotTask(g, g.fleets.find((f) => f.id === id));

/** Makes one of the player's fleets a task force on the job it is doing now (release 2, taskforce.ts). */
export function orderTaskForce(g: Game, id: number): boolean {
  const f = myFleet(g, id);
  return !!f && makeTask(g, f);
}

/** Releases one of the player's task forces: it stays where it is, under his orders (release 2, taskforce.ts). */
export function releaseTaskForce(g: Game, id: number): boolean {
  const f = myFleet(g, id);
  return !!f && releaseTask(g, f);
}

/** A fleet sent at the enemy this month stays where the battle left it until the next (release 2, fleetattack.ts). */
const heldAfterAttack = (g: Game, f: Fleet): boolean => f.attacked === g.turn;

/** Why one of the player's fleets cannot be sent at an enemy fleet now, or null if it can (release 2, fleetattack.ts). */
export const whyNotAttackFleet = (g: Game, id: number, target: number): string | null => whyNotFleetAttack(g, id, target);

/** One of the player's fleets sent at an enemy fleet, weighed up before the order: odds, win chance, likely losses (read-only). */
export const previewFleetAttack = (g: Game, id: number, target: number): FleetAttackPlan | null => (myFleet(g, id) ? planFleetAttack(g, id, target) : null);

/** Sends one of the player's fleets straight at an enemy fleet, fought at once (release 2, fleetattack.ts). */
export function orderFleetAttackNow(g: Game, id: number, target: number): FleetAttackResult | null {
  const out = orderFleetAttack(g, id, target);
  // His order too: the admirals leave it where the battle left it, and do not send it back to its old beat (1.5.1).
  const f = out ? g.fleets.find((x) => x.id === id && x.nat === g.human) : undefined;
  if (f) f.ordered = true;
  return out;
}

// ---- The navy as pieces (the grand campaign): cards, escorts on a lane, shelling a coast, splitting and joining ----

/**
 * The great power whose plans each country builds its battleships, carriers, four-engined bombers, jet fighters,
 * rocket artillery and guided weapons to (Brian, 18 November; ruling 235 and its rider b542; plans.ts):
 * `{ ESP: { bb: 'GER', cv: null, heavybombers: null, jets: 'GER', rockets: null, guided: null }, UK: { bb: 'UK', ... } }`.
 * A great power names itself; a dominion Britain, and Vichy and Free France France (for a technology, once that power
 * has it); a smaller country the power whose plans it gained last, or null when it holds none. The 3D game draws a
 * smaller country's battleship, carrier or aircraft as that power's model, in its own colours and under its own flag.
 */
export const capitalPlans = (g: Game): Record<string, Record<PlanItem, string | null>> => plansView(g);

/** Any fleet as a card: its admiral, where it is, what it is doing, and its ships by class. */
export function fleetView(g: Game, id: number): FleetCard | null {
  const f = g.fleets.find((x) => x.id === id);
  // A fleet the player's side has not identified has no card to show: fleetsInSight() says what is known of it.
  return f && fleetKnown(g, f, g.human) === 'exact' ? fleetCard(g, f) : null;
}

/** The player's fleets as cards, strongest first. */
export function myFleetCards(g: Game): FleetCard[] {
  if (!g.human) return [];
  return g.fleets.filter((f) => f.nat === g.human && Object.values(f.ships).some((n) => n > 0)).map((f) => fleetCard(g, f)).sort((a, b) => b.power - a.power);
}

export interface FleetOptions {
  /** The seas it can be sent to, with what it would meet in each. */
  seas: SeaOption[];
  /** Each lane it has a stake in, with what binding it to that lane would do for the cargoes (and why it cannot, if it cannot). */
  lanes: EscortPlan[];
  /** Enemy coasts it could shell from where it is, and friendly ports it could move its base to. */
  coasts: number[];
  ports: number[];
  /** Fleets it could join where it is. */
  merge: number[];
  /** True when it has more than one ship, so that some can be split off. */
  split: boolean;
  /** Enemy fleets it can be sent straight at now (release 2, fleetattack.ts): there only where that is switched on. */
  attack?: number[];
  /** Why it stays where it is this month, having been sent at the enemy (release 2): there only then. */
  held?: string;
  /** Enemy fleets it can be set to shadow now (release 2, shadow.ts): there only where that is switched on. */
  shadow?: number[];
  /** Seas it can lay a field of mines in now (release 2, mines.ts): there only where that is switched on. */
  mines?: number[];
  /** True where it waits at sea with no harbour it could reach, and can be scuttled (release 2, the far chart: navy.ts goAdrift). */
  scuttle?: boolean;
}

/** A fleet's lanes nearest first on the whole world (release 2: the design director's ruling of 15:48Z, 4); as they come elsewhere. */
function nearestFirst<T extends { zones: number[] }>(g: Game, f: Fleet, lanes: T[]): T[] {
  if (!farOn(g)) return lanes;
  const d = laneStepsFrom(g, f);
  return lanes.map((l, i) => ({ l, i, k: laneSteps(g, f, l, d) })).sort((a, b) => a.k - b.k || a.i - b.i).map((x) => x.l);
}

/** Everything one of the player's fleets could be told to do this month. Null if it is not theirs to order now. */
export function fleetOptions(g: Game, id: number): FleetOptions | null {
  const f = myFleet(g, id);
  if (!f) return null;
  const war = g.nations[f.nat].atWar.length > 0;
  // (Release 2, the far chart: a fleet in passage to a new base is held to its way. It can only be turned back.)
  const held = heldAfterAttack(g, f) || inPassage(f);
  // (1.5.2) Cut off at sea beyond a shut strait, every sea and lane in its reach lies beyond the strait: none is offered
  // only to be refused (sail() turns them all down). Its way out is to harbour, on its side.
  const cut = cutOff(g, f);
  const out: FleetOptions = {
    seas: war && !held && !cut ? seaOptions(g, f, f.nat) : [],
    // (Release 2, the far chart: only the lanes within two months' steaming of it, and its own, until the Pacific lanes
    // are laid; then every lane. Nearest first: laneNear, laneSteps.)
    lanes: war && !held && !cut ? nearestFirst(g, f, escortLanes(g, f.nat).filter((l) => laneNear(g, f, l))).map((l) => planEscort(g, f, l.id)) : [],
    coasts: war ? bombardTargets(g, f, f.nat).map((r) => r.id) : [],
    // (Waiting at sea with no harbour it could reach, the harbours open to it now, where any is: release 2, the far chart.)
    ports: held && !adrift(f) ? [] : baseTargets(g, f).map((r) => r.id),
    merge: mergeTargets(g, f).map((x) => x.id),
    split: g.flags.open === 1 && !inPassage(f) && Object.values(f.ships).reduce((s, n) => s + n, 0) > 1,
  };
  if (fleetAttackOn(g)) out.attack = war ? attackTargets(g, f.id) : [];
  // (Shadowing and mines sail the fleet to a sea as well (sail()), so a cut-off fleet is offered neither: 1.5.2's rule above.)
  if (shadowOn(g)) out.shadow = war && !cut ? shadowTargets(g, f.id) : [];
  if (seaMinesOn(g)) out.mines = war && !held && !cut ? mineSeas(g, f.id) : [];
  if (adrift(f)) { out.held = `${passageWhy(g, f)} ${out.ports.length ? 'A harbour is open to it now: it can make for one.' : 'It can be scuttled.'}`; out.scuttle = true; }
  else if (inPassage(f)) out.held = `${passageWhy(g, f)} It can be turned back to ${f.passage?.fromPort ?? 'the harbour it left'}.`;
  else if (held) {
    // (1.5.3) "The Home Fleet was sent", "Force H was sent": the fleet named as the attack card names it.
    const named = fleetName(f);
    out.held = `${named[0].toUpperCase()}${named.slice(1)} was sent at the enemy this month: it stays where the battle left it until next month.`;
  }
  return out;
}

/** The shipping lanes the player has a stake in guarding, with the escorts bound to each. */
export function lanesToGuard(g: Game): EscortLane[] {
  return g.human ? escortLanes(g, g.human) : [];
}

/** What binding one of the player's fleets to a lane would do for its cargoes. */
export function previewEscort(g: Game, id: number, lane: string): EscortPlan | null {
  const f = myFleet(g, id);
  return f ? planEscort(g, f, lane) : null;
}

/** Binds one of the player's fleets to a shipping lane as its escort. */
export function orderEscort(g: Game, id: number, lane: string): boolean {
  const f = myFleet(g, id);
  const ok = !!f && !heldAfterAttack(g, f) && hisOrder(f, escortLane(g, f, lane));
  if (ok) retask(g, f!);
  if (ok) stopShadow(f!);
  return ok;
}

/** What shelling a coast with one of the player's fleets would do: the troops there, or the works of its city. */
export function previewBombard(g: Game, id: number, region: number, what?: ShoreTarget): ShorePlan | null {
  const f = myFleet(g, id);
  return f ? planBombard(g, f, g.regions[region], what, f.nat) : null;
}

/** Stands one of the player's fleets in to an enemy coast to shell it. */
export function orderBombard(g: Game, id: number, region: number, what: ShoreTarget): ShoreReport | null {
  const f = myFleet(g, id);
  return f ? bombard(g, f, g.regions[region], what, f.nat) : null;
}

/** Why these ships cannot be split off from one of the player's fleets, or null. */
export function whyNotSplit(g: Game, id: number, ships: Partial<Ships>): string | null {
  const f = myFleet(g, id);
  return f ? whyNotDetach(g, f, ships) : 'That fleet cannot be given orders now.';
}

/** Splits ships off from one of the player's fleets as a squadron of their own. Returns the new fleet's id, or -1. */
export function orderSplit(g: Game, id: number, ships: Partial<Ships>, name?: string): number {
  const f = myFleet(g, id);
  const sq = f ? detach(g, f, ships, name) : null;
  // The squadron is of his own making: it stays where it was split off until he orders it (1.5.1).
  if (sq) sq.ordered = true;
  return sq?.id || -1;
}

/** Joins one of the player's fleets to another in the same harbour or the same sea. */
export function orderMerge(g: Game, id: number, into: number): boolean {
  const a = myFleet(g, id), b = myFleet(g, into);
  if (!a || !b) return false;
  return whyNotMerge(g, a, b) === null && merge(g, a, b);
}

export function orderShip(g: Game, c: Corps, to: number): boolean {
  return ownOrder(g, c) && shipMove(g, c, g.regions[to]);
}

const myAir = (g: Game, id: number): AirFleet | null => {
  const f = g.airFleets.find((x) => x.id === id);
  return f && f.nat === g.human && current(g) === f.nat ? f : null;
};

/** Gives one of the player's air fleets its mission for the month: a region to fight over or bomb, or a sea to strike. */
export function orderAir(g: Game, id: number, mission: AirMission, target = -1): boolean {
  const f = myAir(g, id);
  return !!f && setMission(g, f, mission, target);
}

/**
 * Moves one of the player's air fleets to airfields in another friendly region. For a carrier wing aboard its
 * fleet this is the order to fly ashore (ruling 175(b)): to the airfield of the harbour its fleet lies in, or one on
 * the sea it is in. airGroupBases() lists where; a wing's GroupRow.wingMove says why not when it cannot.
 */
// ---- The gate's order (Version 84.1; ruling 176(e), on Brian's words of 2026-11-17: "you should be able to open or
// close the panama canal"). The holder may shut his gate to all ships but his own and his allies', and open it
// again. The player's order only: no staff gives it. The question says whose trade it stops before he orders. ----

export interface GateRow {
  nat: string;
  name: string;
  /** The holder's own row: it stands first. */
  own: boolean;
  /** What happens to its trade, as a sentence. */
  what: string;
  /** What it loses a month, rounded to the money the page shows (budget.json 'scale'), so that the rows add up to the totals. */
  loss: number;
}
/** A country that resents the shut gate (ruling 177(a)): how far its opinion has moved for this shutting, of the most it can. */
export interface GateSoreRow { nat: string; name: string; so: number; most: number; line: string }
export interface GateQuestion {
  gate: GateView;
  /** The order the box offers: to shut the gate, or to open it again. */
  order: 'shut' | 'open';
  label: string;
  question: string;
  lead: string;
  /**
   * The price as rows: whose trade stops or goes the long way round, with what each loses a month. The holder's own
   * row always stands first, even when none of his trade runs through the gate (it then says so, and its loss is 0).
   */
  heading: string;
  rows: GateRow[];
  /** All the rows together; and the other countries' alone, with the words for that last row of the tally. */
  total: number;
  others: number;
  sumLabel: string;
  /** Said under the holder's own row where nobody else's trade runs through the gate. */
  none: string;
  /**
   * Who resents it (ruling 177(a)): the countries at peace and outside both blocs whose trade it turns back, each a
   * point of opinion a month away from the holder's side, to a most for one shutting. Empty where nobody will.
   */
  resentHeading: string;
  resentNote: string;
  resent: GateSoreRow[];
  /** The two buttons: the order, and leaving things as they are. */
  yes: string;
  no: string;
}
/**
 * What ordering these corps away would leave a sea gate to (ruling 176(d)): when they are the last of the player's and
 * his allies' corps in the province of a gate he holds, the sentence for the move's card. Null otherwise. Nothing is changed.
 */
export interface GateLeft {
  gate: number; name: string; region: number;
  /** The town's own guard that stays, in men: 0 where the place has none. */
  men: number;
  /** For the move's card, before the order: "If they go, the Panama Canal is left to its town guard: about 4,800 men, and no army." */
  ifLine: string;
  /** Once they have gone: "The Panama Canal is left to its town guard: about 4,800 men, and no army." */
  line: string;
}
export function gateLeft(g: Game, corps: Corps[]): GateLeft | null {
  if (g.human === null || !corps.length) return null;
  const going = new Set(corps.map((c) => c.id));
  for (const from of new Set(corps.map((c) => c.reg))) {
    const i = g.gates.findIndex((gt) => gt.region === from);
    const r = g.regions[from];
    if (i < 0 || !r || r.ctrl !== g.human) continue;
    const stays = [...r.units, ...(r.garr ?? [])].some((c) => !c.dead && !going.has(c.id) && allied(g, g.human!, c.nat));
    if (stays) continue;
    const gt = g.gates[i], many = /s$/.test(gt.name), men = guardMen(g, r);
    const rest = men > 0 ? `${many ? 'are' : 'is'} left to ${many ? 'their' : 'its'} town guard: about ${menWord(men)} men, and no army.` : `${many ? 'are' : 'is'} left with nobody to hold ${many ? 'them' : 'it'}.`;
    return { gate: i, name: gt.name, region: from, men, ifLine: `If they go, ${gt.name} ${rest}`, line: `${gt.name[0].toUpperCase()}${gt.name.slice(1)} ${rest}` };
  }
  return null;
}

/** Why the player cannot give a gate's order, or null when he can. */
export function whyNotGate(g: Game, gate: number): string | null {
  const gt = g.gates[gate];
  if (!gt) return 'There is no such gate.';
  if (g.human === null || !g.nations[g.human]?.alive || gateHolder(g, gt) !== g.human) return `Only its holder can ${gateShutSince(g, gate) < 0 ? 'shut' : 'open'} it.`;
  if (!humanUp(g) || onEve(g)) return 'It is not your month.';
  return null;
}
const namesOf = (g: Game, ids: string[]): string => {
  const ns = ids.map((id) => theName(g.nations[id]));
  return ns.length <= 1 ? ns.join('') : `${ns.slice(0, -1).join(', ')} and ${ns[ns.length - 1]}`;
};
/** The gate's question for its holder: what the order does, and what it costs whom, before he gives it. Null where he cannot give it. */
export function gateQuestion(g: Game, gate: number): GateQuestion | null {
  if (whyNotGate(g, gate)) return null;
  const me = g.human!, gt = g.gates[gate];
  const v = gateViews(g, me)[gate];
  const isShut = v.shutSince >= 0;
  const many = /s$/.test(gt.name), it = many ? 'they' : 'it', them = many ? 'them' : 'it';
  // The same game with the gate the other way, as an exact copy: the price is read off the two, and nothing changes.
  const other = loadGame(saveGame(g));
  let list: GateTradeRow[] = [];
  if (other) {
    setGate(other, gate, !isShut);
    list = isShut ? gateTrade(other, g, gate) : gateTrade(g, other, gate);
  }
  const what = (r: GateTradeRow, own: boolean): string => {
    const parts: string[] = [];
    if (r.stops.length) parts.push(`${own ? 'Your' : 'Its'} trade with ${namesOf(g, r.stops)} ${isShut ? 'is stopped' : 'stops'}`);
    if (r.round.length) parts.push(`${parts.length ? (own ? 'your' : 'its') : own ? 'Your' : 'Its'} trade with ${namesOf(g, r.round)} goes the long way round`);
    return `${parts.join('; ')}.`;
  };
  // (Each figure is rounded to the money the page shows, and the totals are the rows added up: a sum that is one out reads as a fault.)
  const shown = (x: number): number => Math.round(x * BUDGET.scale) / BUDGET.scale;
  const rows: GateRow[] = list.map((r) => ({ nat: r.nat, name: g.nations[r.nat].name, own: r.nat === me, what: what(r, r.nat === me), loss: shown(r.loss) }))
    .sort((x, y) => Number(y.own) - Number(x.own));
  // His own row first, always: he must not read the others' bill as his own.
  if (!rows.length || !rows[0].own) rows.unshift({ nat: me, name: g.nations[me].name, own: true, what: 'None of your own trade runs through it.', loss: 0 });
  const others = rows.filter((r) => !r.own);
  // Who resents it: fixed at the shutting for a gate already shut, else those of the rows who would.
  const G = RELATIONS.gate, side = sideName(g, me);
  const sore = isShut ? soreList(g, gate) : others.filter((r) => wouldResent(g, me, r.nat)).map((r) => ({ nat: r.nat, so: 0 }));
  const resent: GateSoreRow[] = sore.map((x) => ({
    nat: x.nat, name: g.nations[x.nat].name, so: x.so, most: G.most,
    line: isShut ? `${x.so} of ${G.most} points so far` : `${G.month} point of opinion a month, ${G.most} at most`,
  })).sort((x, y) => x.name.localeCompare(y.name));
  return {
    gate: v, order: isShut ? 'open' : 'shut', label: 'Sea gate · you hold it',
    question: isShut ? `Open ${gt.name} again?` : `Shut ${gt.name}?`,
    lead: isShut
      ? `You shut ${them} in ${turnToDate(v.shutSince).label}. Open, ${it} ${many ? 'stand' : 'stands'} as before: ${gt.closed ? 'in wartime only your allies may pass, and a country at war with nobody' : 'every country may pass that is not at war with you'}.`
      : `Shut, ${it} ${many ? 'let' : 'lets'} through only your own ships and your allies'. Every other country's ships are turned back: its warships, its supply ships and the trade it carries, your own trade with it included. You can open ${them} again whenever you choose.`,
    heading: isShut ? 'What it is costing each month' : 'What it costs each month',
    rows, total: shown(rows.reduce((s, r) => s + r.loss, 0)), others: shown(others.reduce((s, r) => s + r.loss, 0)), sumLabel: 'All told, to others',
    none: isShut ? 'No agreement is held up by it today.' : 'No trade runs through it today: shutting it costs nobody anything yet.',
    resentHeading: isShut ? 'Who resents it' : 'Who will resent it',
    resentNote: !resent.length ? '' : isShut
      ? `Each has moved away from ${side} in public opinion since you shut ${them}. Open, they move no further.`
      : `Each moves away from ${side} in public opinion for as long as ${it} ${many ? 'stay' : 'stays'} shut.`,
    resent,
    yes: isShut ? 'Open the gate' : 'Shut the gate', no: isShut ? 'Leave it shut' : 'Leave it open',
  };
}
/** The holder's order: shuts his gate to all ships but his own and his allies', or opens it again. */
export function orderGate(g: Game, gate: number, shut: boolean): boolean {
  if (whyNotGate(g, gate) || (gateShutSince(g, gate) >= 0) === shut) return false;
  // Who resents it is fixed at the shutting: the countries whose trade it turns back this month (ruling 177(a)).
  const sore = shut ? gateQuestion(g, gate)?.resent.map((r) => r.nat) ?? [] : [];
  setGate(g, gate, shut);
  if (shut) soreAtShut(g, gate, g.human!, sore); else soreForget(g, gate);
  // His own agreements find the gate so at once; every other country's do in its own month.
  refreshTrade(g, g.human!);
  return true;
}

export function orderAirBase(g: Game, id: number, region: number): boolean {
  const f = myAir(g, id), r = g.regions[region];
  if (!f || !r) return false;
  // A group on its way by stages (ferry.ts) is sent here instead: its journey is called off first, and stands if the move cannot be made.
  const was = f.way;
  if (was) delete f.way;
  // (Across an ocean a land group goes by sea, one a month: ruling 133(b).)
  const ok = f.ship !== undefined ? flyAshore(g, f, r) : overOcean(g, f, r) ? shipAir(g, f, r) : rebaseAir(g, f, r);
  if (!ok && was) f.way = was;
  return ok;
}

/**
 * Every airfield of his side's one of the player's land groups could be sent to by stages (release 2, ferry.ts), each
 * with its way there in plain words: the turns, the fields on the way, and the enemy fighters his side knows of within
 * reach of it; or crated by sea, or why not. Empty with the switch off.
 */
export function airFerryRoutes(g: Game, id: number): FerryRoute[] {
  const f = g.airFleets.find((x) => x.id === id && x.nat === g.human);
  return f && g.human && ferryOn(g) ? [...ferryRoutes(g, f, g.human).values()] : [];
}

/** Sends one of the player's land groups to a friendly airfield by stages: it sets off at once. False only where there is no way there (ferry.ts). */
export function orderFerryTo(g: Game, id: number, region: number): FerryRoute | null {
  const f = myAir(g, id);
  return f && ferryOn(g) ? orderFerry(g, f, region) : null;
}

/** Calls off one of the player's groups' journey: it stays at the field it has reached. */
export function orderFerryOff(g: Game, id: number): boolean {
  const f = myAir(g, id);
  return !!f && ferryOn(g) && cancelFerry(g, f);
}

/** The fleets one of the player's carrier wings ashore could go back aboard, those it can board now first: each with where it lies, the deck room standing empty, and why not now. */
export const airGroupDecks = (g: Game, id: number): DeckOption[] => groupDecks(g, id, g.human);

/** Sends one of the player's carrier wings ashore back aboard a fleet of his navy that has deck room standing empty for it there. */
export function orderAboard(g: Game, id: number, fleet: number): boolean {
  const f = myAir(g, id), c = g.fleets.find((x) => x.id === fleet);
  return !!f && !!c && flyAboard(g, f, c);
}

/** Where one of the player's air fleets could be sent on a mission, and where it could be based. */
export function airOptions(g: Game, id: number, mission: AirMission): { targets: number[]; bases: number[] } {
  const f = myAir(g, id);
  return f ? { targets: airTargets(g, f, mission), bases: moveTargets(g, f).map((r) => r.id) } : { targets: [], bases: [] };
}

// ---- The air war as its own game (the grand campaign): airfields, air groups, and raids planned from their target ----

/** Every airfield on the map as the player knows it, for a map to draw. Empty in a historical scenario. */
export const airfieldsSeen = (g: Game, seen?: Uint8Array): AirfieldRow[] => airfields(g, g.human, seen);

/** One airfield opened: its size and the air groups based there. Null outside the grand campaign. */
export const airfieldAt = (g: Game, region: number): AirfieldCard | null => airfieldCard(g, region, g.human);

/** The player's air groups, as cards. */
export const myAirGroups = (g: Game): GroupRow[] => (g.human ? airGroups(g, g.human) : []);

/** What one of the player's air groups can be left doing between raids, each with what it would mean. */
export const airGroupJobs = (g: Game, id: number): GroupJob[] => groupJobs(g, id, g.human);

/** Gives one of the player's air groups its standing order: 'patrol', 'escort:<bomber group id>' or 'rest'. */
export function orderGroupJob(g: Game, id: number, key: string): boolean {
  return !!myAir(g, id) && setGroupJob(g, id, key, g.human);
}

/** The airfields one of the player's air groups could move to, nearest first. orderAirBase() moves it. */
export const airGroupBases = (g: Game, id: number): BaseOption[] => groupBases(g, id, g.human);

/**
 * The eve of war has no shot fired, so the player's planner has no raid to offer on it: every kind but the scouts'
 * photographs is refused with this sentence, in the menu, in the targets, in the plan and in the order itself.
 * (Only a player has an eve, so no war the computer fights alone is touched.)
 */
export const EVE_NO_RAID = 'It is the eve of war: no raid flies until the first shot is fired.';
const eveRaid = (g: Game, kind: RaidKind): boolean => onEve(g) && kind !== 'recon';
const eveMenu = (g: Game, menu: RaidKindView[]): RaidKindView[] => (onEve(g) ? menu.map((k) => (k.key === 'recon' ? k : { ...k, targets: 0, why: EVE_NO_RAID })) : menu);

/** The raid planner's menu: each kind of raid and how many targets are in reach this month. */
export const raidMenu = (g: Game): RaidKindView[] => eveMenu(g, raidKinds(g, g.human));

/** The same menu with a bombing raid named once, by day or by night (the design director's ruling 117, point 6). */
export const raidMenuOne = (g: Game): RaidKindView[] => eveMenu(g, raidKindsOne(g, g.human));

/** Every target for one kind of raid, nearest first. With `lead`, only those that one of the player's groups can fly against. */
export const raidTargetsFor = (g: Game, kind: RaidKind, lead = -1): RaidTarget[] => (eveRaid(g, kind) ? [] : raidTargets(g, kind, g.human, lead));

/**
 * A raid weighed up before it is ordered: who could fly it, who is going, what will meet it, the chance of success,
 * the losses to expect and what meeting up is worth. Leave `ids` out for the staff's proposal.
 */
export const previewRaid = (g: Game, kind: RaidKind, target: number, ids?: number[]): RaidPlan => {
  const p = planRaid(g, kind, target, ids, g.human);
  return eveRaid(g, kind) ? { ...p, why: EVE_NO_RAID } : p;
};

/** A bombing raid by day and by night, side by side, for the raid card (ruling 107(d)). Null for any other kind of raid. */
export const previewDayNight = (g: Game, kind: RaidKind, target: number, ids?: number[]): DayNight | null => dayOrNight(g, kind, target, ids, g.human);

/** Flies a planned raid at once and files its report. Null if it cannot be flown as picked. */
export function orderRaid(g: Game, kind: RaidKind, target: number, ids: number[], fought?: FoughtAir): AirReport | null {
  return g.human !== null && humanUp(g) && !eveRaid(g, kind) ? launchRaid(g, kind, target, ids, g.human, fought) : null;
}

/**
 * The raids flown in a month (this month unless told otherwise), as the player may know them: his side's and those
 * flown against it. Each gives the target, every group that flew with the airfield it took off from, the fighters
 * that rose and from where, and what each lost, so an interface can show the raid again.
 */
export const flownRaids = (g: Game, turn = g.turn): RaidReplay[] => raidReplays(g, turn, g.human);

/** Sends a few aircraft of one of the player's groups to photograph a province: its armies are seen whole until the month ends. */
export function orderScout(g: Game, id: number, target: number): { ok: boolean; text: string; lost: number } {
  if (!myAir(g, id)) return { ok: false, text: 'It is not your month.', lost: 0 };
  const flown = scout(g, id, target, g.human);
  // The photographs name every corps in the province and around it.
  if (flown.ok) reconnoitre(g);
  return flown;
}

// ---- Air campaigns (aircampaigns.ts; the grand campaign's air war, ruling 236.2) ---------------------------------

// On in a new grand campaign as shipped (aircampaigns.json `on`; the design director, 22 November 12:28Z, b623 and
// b636). The browser's newGame and the campaign bridge's start turn it on right after startOpen, where the war runners
// set g.flags.ac with TC_AC=1. The bridge asks for both by name, so it builds on rules without the switch too.
export { airCampaignsShipped, setAirCampaigns } from './aircampaigns';

// ---- Carriers and the air war at sea (the grand campaign) ----
//
// A carrier wing is an air group like any other: its id (WingCard.id, GroupRow.id with ship >= 0) goes to the raid
// planner (raidTargetsFor, previewRaid, orderRaid with the kinds 'fleet' and 'harbour'), to airGroupJobs and
// orderGroupJob, and to the search below. It has no airfield: it flies from wherever its fleet is.

/** Every fleet the player can put on the map: his own side's whole, and others' as far as his side has found them. A fleet only reported has a flag and a rough size and no more; one not found is not in the list. */
export const fleetsInSight = (g: Game): FleetSeen[] => fleetsSeen(g, g.human);

/** One sea as the player knows it: each side's known strength, enemy forces only reported, and how well his side has it searched. Null with nobody at the table. */
export const seaPictureOf = (g: Game, zone: number): SeaPicture | null => (g.human && g.seas[zone] ? seaPicture(g, zone, g.human) : null);

/**
 * A fleet action weighed up before it is fought (Version 80; display only, it changes nothing): one of the player's
 * fleets on patrol in a sea, with whatever else of his side patrols there, against the enemy as far as his side has
 * found him. Each side's fleets and their weight, the aircraft that reach from the coasts, the odds as a band, the
 * chance of winning in words, whether his admiral will offer battle, who may decline it, and the ships each side is
 * likely to lose by class. A force only reported is listed and not counted; one not found is not there. `why` says
 * so when there is nothing to forecast. Null with nobody at the table or for a fleet that is not his side's.
 * After the action the result is the last entry of `g.seaReports` for that sea.
 */
export const seaForecast = (g: Game, id: number, zone: number): SeaPlan | null => (g.human ? planSeaBattle(g, id, zone, g.human) : null);

/**
 * How a piece of news reads to whoever is looking (Version 80; display only): 'won' when his side had the better of
 * the fighting or his enemy the worse, 'lost' the other way round, and null for news with no winner or loser, or of
 * a fight that was none of his. Which arm fought is on the line itself (`arm`: land, sea or air).
 */
export function newsSide(g: Game, h: Headline, viewer: string | null = g.human): 'won' | 'lost' | null {
  if (viewer === null || !g.nations[viewer]) return null;
  const ours = (id?: string): boolean => id !== undefined && (id === viewer || allied(g, id, viewer));
  const foe = (id?: string): boolean => id !== undefined && atWar(g, id, viewer);
  if (ours(h.won)) return 'won';
  if (ours(h.lost)) return 'lost';
  // A line that names only one side: an enemy's loss is good news, and an enemy's success bad.
  if (h.won === undefined && foe(h.lost)) return 'won';
  if (h.lost === undefined && foe(h.won)) return 'lost';
  return null;
}

/** The aircraft one fleet's carriers bring, as cards: fighters first. Empty for a fleet without carriers, one that is not identified, and outside the grand campaign. */
export function wingsOfFleet(g: Game, id: number): WingCard[] {
  const f = g.fleets.find((x) => x.id === id);
  return f && fleetKnown(g, f, g.human) === 'exact' ? wingCards(g, f) : [];
}

/** The seas one of the player's air groups (a carrier wing, or a group on an airfield) could search this month, nearest first. */
export const searchOptionsFor = (g: Game, id: number): SearchOption[] => searchOptions(g, id, g.human);

/** Sends a few aircraft of one of the player's groups to search a sea: enemy ships there may be reported, or identified. One flight a month for each group, beside its raid. */
export function orderSearch(g: Game, id: number, zone: number): { ok: boolean; text: string; found: number } {
  if (!myAir(g, id)) return { ok: false, text: 'It is not your month.', found: 0 };
  return searchSea(g, id, zone, g.human);
}

/** The odds and the dangers of an air mission, before the order is given. */
export function previewAirOrder(g: Game, id: number, mission: AirMission, target: number): AirAssess | null {
  const f = g.airFleets.find((x) => x.id === id);
  return f ? previewAir(g, f, mission, target) : null;
}

export function orderBuildCorps(g: Game, kind: Kind, region: number, revive?: number): Build | null {
  return humanUp(g) ? orderCorps(g, g.human!, kind, region, revive) : null;
}

/** Cashes in one of the player's honours: its heroes go home on a bond tour, or to the schools as instructors. */
export function orderCash(g: Game, award: number, how: Cash): boolean {
  return humanUp(g) && cashHonour(g, award, how);
}

/** The player's standing order for bomber crews who finish their tour: the schools, a war bond tour, or one and then the other (bombers.ts). */
export function orderCrews(g: Game, how: CrewOrder): boolean {
  return humanUp(g) && setCrewOrder(g, how);
}

/** The player sends a holder of the highest award home: on a war bond tour, or to train recruits (valour.ts). */
export function orderHero(g: Game, id: number, how: Cash): boolean {
  return humanUp(g) && sendHero(g, id, how);
}

/** The player has read a citation. */
export function orderSeen(g: Game, award: number): void {
  const a = awardOf(g, award);
  if (a && a.nat === g.human) a.seen = true;
}

/**
 * The player sends a corps to the battle schools for this month, or calls it back before the month is out (release 2).
 * See whyNoSchool for who can go. A corps called back has had no term: it is free to march and fight again.
 */
export function orderSchool(g: Game, c: Corps, go: boolean): boolean {
  if (g.human === null || current(g) !== g.human || c.nat !== g.human || c.dead) return false;
  if (!go) {
    if (!c.school) return false;
    delete c.school;
    return true;
  }
  if (whyNoSchool(g, c) !== null) return false;
  c.school = true;
  return true;
}

/** The player's standing order for a corps: take drafts as they come, or keep the veterans together and take none. */
export function orderDrafts(g: Game, c: Corps, take: boolean): boolean {
  if (c.dead || c.nat !== g.human || c.kind === 'fort') return false;
  if (take) delete c.lean; else c.lean = true;
  return true;
}

/**
 * Everything the player's nation could order from its arsenal, with one province in mind (the one picked on the
 * map, or -1): corps, aircraft, ships and works, each with its price, its upkeep, where it would go and, when it
 * cannot be ordered, why not. Both interfaces draw their build screens from this one list.
 */
export function arsenalOffers(g: Game, region = -1): Offer[] {
  return g.human ? offers(g, g.human, region) : [];
}

/** Places one of those orders: the key of the offer ('corps:arm', 'air:ftr', 'ship:dd', 'work:aa') and the province in mind. */
export function orderArsenal(g: Game, key: string, region = -1): Build | null {
  return humanUp(g) ? orderOffer(g, g.human!, key, region) : null;
}

export function orderBuildFort(g: Game, region: number): Build | null {
  return humanUp(g) ? orderFort(g, g.human!, region) : null;
}

export function orderCancelBuild(g: Game, id: number): boolean {
  if (!humanUp(g)) return false;
  const b = g.nations[g.human!]?.queue.find((x) => x.id === id);
  if (!cancelBuild(g, g.human!, id)) return false;
  // (Release 2, townqueue.ts) A building taken back takes with it what was queued behind it in its line and needs it:
  // nothing had been paid. (Nothing is queued with that switch off.)
  if (b?.what === 'civic' && b.item) dropBehind(g, g.human!, b.region, b.item);
  return true;
}

/**
 * The technology tree as the player's nation sees it (the grand campaign): every technology with its branch and
 * step, what it does in words, what it needs, whether it is researched, in work, open or locked, and why it
 * cannot be started now. Null in a historical scenario, or with nobody at the controls.
 */
export function techTree(g: Game): TechView | null {
  return g.human ? techView(g, g.human) : null;
}

/**
 * Oil and supplies (the grand campaign): the books of the player's nation, or of any power that keeps stocks. For
 * each of the two: what is in the depots and the most they hold, every source and every use for the month, months
 * left at this rate, the share of the need that was met, and what a shortage is costing in words; and every lane and
 * overland deal that feeds the nation, with the share getting through. Null in a historical scenario, and for a
 * small country, which buys what it needs abroad and is never short.
 */
export function depots(g: Game, nat: string | null = g.human): StoresView | null {
  return nat ? stores(g, nat) : null;
}

/**
 * Every shipping lane on the map this month, whoever it feeds: the points to draw it through, the seas where it can
 * be raided or guarded, the share of its cargoes getting through, and whether it is open, raided or cut.
 */
export function shippingLanes(g: Game): Lane[] {
  return seaLanes(g);
}

// ---- Towns and trade (the grand campaign) --------------------------------------------------------------

export interface TownBuildings {
  region: number;
  /** The chief place of the province and how big it is: the size sets the number of plots. */
  town: string;
  size: TownSize;
  /** Every plot in order: what stands on it, what is being built, or free (line null). */
  plots: PlotView[];
  free: number;
  /** True when the province is the player's to build in. */
  mine: boolean;
  /** Every line of building as an offer here: the next level, its cost, months and effect, or why not. */
  offers: CivicOffer[];
}

/** The buildings of a province's chief place, and what the player could build there. Null in a history campaign. */
export function townBuildings(g: Game, region: number): TownBuildings | null {
  const r = g.regions[region];
  if (!r || !civicRules(g)) return null;
  const plots = plotsAt(g, region, keepsStores(g, r.ctrl));
  const me = g.human;
  const mine = !!me && r.ctrl === me && g.nations[me].alive;
  return { region, town: townName(r), size: townSize(g, r), plots, free: plots.filter((p) => p.line === null).length, mine, offers: mine ? civicOffers(g, me!, region, keepsStores(g, me!)) : [] };
}

/** Orders the next level of a line of building ('market', 'farm', 'school', 'camp', 'hospital', 'hall') in one of the player's provinces. */
export function orderBuilding(g: Game, region: number, line: string): Build | null {
  return humanUp(g) ? orderCivic(g, g.human!, region, line) : null;
}

// ---- The atomic project (release 2; Brian 25 November 2026; atom.ts) ------------------------------------------------

// A staff's armed strike flies at once as an ordinary bombing raid: the staff's proposal of bombers and escort, or else
// the group it picked (airwar.ts launchRaid, through air.ts fly and the air rules' own losses).
atomFly.raid = (g, nat, region, groups) => {
  const plan = planRaid(g, 'bomb', region, undefined, nat);
  const ids = !plan.why && plan.picked.some((id) => groups.includes(id) || (g.airFleets.find((f) => f.id === id)?.bmr ?? 0) > 0) ? plan.picked : groups;
  return !!launchRaid(g, 'bomb', region, ids, nat);
};

/** True in a grand campaign with the atomic project switched on. */
export const atomBombOn = (g: Game): boolean => atomOn(g);

/** Switches the atomic project on or off for this grand campaign, in any month (OPTIONS). */
export const setAtomBomb = (g: Game, on: boolean): boolean => setAtom(g, on);

/** The player's atomic project as his page shows it, or null outside the rules. */
export const atomPage = (g: Game): AtomView | null => (g.human ? atomView(g, g.human) : null);

/** The paces the plants can draw at, with the share of the earnings each takes. */
export const atomPaces = (): { pace: AtomPace; share: number }[] => PACES.map((pace) => ({ pace, share: ATOM.plants.pace[pace] }));

/** Why the player cannot begin the atomic plants now (at `site`, or his chosen or the staff's), or null. */
export const whyNotAtomPlants = (g: Game, site?: number): string | null => (g.human ? whyNotPlants(g, g.human, site) : 'There is no player.');

/** Begins the player's atomic plants, at a site and a pace (his chosen, or the staff's, where none is given). */
export const orderAtomPlants = (g: Game, site?: number, pace?: AtomPace): boolean => g.human !== null && humanUp(g) && beginPlants(g, g.human, site, pace);

/** Chooses the province the player's plants will stand in, before they are begun. */
export const orderAtomSite = (g: Game, region: number): boolean => g.human !== null && humanUp(g) && chooseSite(g, g.human, region);

/** Sets the pace the player's plants draw at. */
export const orderAtomPace = (g: Game, pace: AtomPace): boolean => g.human !== null && humanUp(g) && setPace(g, g.human, pace);

/** Stops the player's plants drawing, or lets them draw again. */
export const orderAtomPause = (g: Game, paused: boolean): boolean => g.human !== null && humanUp(g) && pausePlants(g, g.human, paused);

/** Stops the player's finished plants making more bombs, or lets them. */
export const orderAtomMaking = (g: Game, on: boolean): boolean => g.human !== null && humanUp(g) && setMaking(g, g.human, on);

/** Why the player cannot arm a strike on this city now, or null. */
export const whyNotAtomStrike = (g: Game, region: number): string | null => (g.human ? whyNotStrike(g, g.human, region) : 'There is no player.');

/** Arms a strike: the player's next bombing raid on that city this month carries the bomb. */
export const orderAtomStrike = (g: Game, region: number): boolean => g.human !== null && humanUp(g) && armStrike(g, g.human, region);

/** Takes the player's armed strike back before the raid flies. */
export const cancelAtomStrike = (g: Game): boolean => g.human !== null && humanUp(g) && disarmStrike(g, g.human);

// ---- Damaged cities (the grand campaign with its switch: citydamage.ts, repairs.ts) ----------------------

/** True when towns take damage and are repaired in this war. */
export const cityDamageOn = (g: Game): boolean => damageRules(g);

/**
 * Turns damaged cities on or off for a grand campaign (the interfaces turn it on when a new one begins). Off, the
 * war is played exactly as it was before there were damaged cities; damage already done stays on the books but
 * neither costs nor shows anything until it is turned on again.
 */
export function setCityDamage(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1) return false;
  openDamage(g, on);
  return damageRules(g) === on;
}

// ---- Air campaigns (aircampaigns.ts; the grand campaign's air war, ruling 236.2) ---------------------------------

// Fog and reconnaissance on air raids (airfog.ts; off as shipped, airwar.json fog.on): a war has it with g.flags.rfog,
// set by setRaidFog right after startOpen. The bridge can ask for both by name.
export { raidFogShipped, setRaidFog } from './airfog';

/** A town's damage as the player sees it on clicking it: how badly, what it costs him, the price of a repair and why not. Null where it stands whole. */
export function cityDamage(g: Game, region: number): RepairView | null {
  return damageRules(g) ? repairView(g, region, g.human) : null;
}

/** Every damaged town the player holds, the ones whose damage costs him most first. */
export function damagedCities(g: Game): RepairView[] {
  return g.human && damageRules(g) ? damagedTowns(g, g.human) : [];
}

/** What 'Repair all' would do now: the towns the treasury can pay for, best first, their price and what they bring back each month. */
export function repairAllOutlook(g: Game): RepairAll | null {
  return g.human && damageRules(g) ? repairAllPlan(g, g.human) : null;
}

/** Every damaged town on the map, for the map's marks: where and how badly. */
export function damageMarks(g: Game): ReturnType<typeof townsDamaged> {
  return townsDamaged(g);
}

/** Repairs one of the player's towns, paid at once. True if it was done. */
export function orderRepair(g: Game, region: number): boolean {
  return humanUp(g) && repairCity(g, g.human!, region);
}

/** Repairs every one of the player's damaged towns the treasury can pay for, best first. Returns the towns repaired. */
export function orderRepairAll(g: Game): string[] {
  return humanUp(g) ? repairAll(g, g.human!) : [];
}

/** Takes back a repair the player ordered this month: the town as it was, and the money back. */
export function orderUndoRepair(g: Game, region: number): boolean {
  return humanUp(g) && undoRepair(g, g.human!, region);
}

// ---- Airfields as targets (fieldstate.ts; the grand campaign's air war with its switch) ----------------------------

/** True when airfields are targets in this war. (The bridge looks this up by name: fieldRules.) */
export const airfieldTargetsOn = (g: Game): boolean => fieldRulesOf(g);
export const fieldRules = (g: Game): boolean => fieldRulesOf(g);

/** True when new grand campaigns are opened with airfields as targets (damage.json field.on; false as shipped). */
export const airfieldTargetsShipped = (): boolean => FIELD.on === true;

/** Turns airfields as targets on or off for a grand campaign. Off, the war plays as release 1 did. */
export function setAirfieldTargets(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1) return false;
  openFields(g, on);
  return (g.flags.fields === 1) === on;
}

/** The bridge's name for setAirfieldTargets: a new grand campaign, behind its option. */
export const setFieldRules = (g: Game, on: boolean): boolean => setAirfieldTargets(g, on);

/** A province's field as the glance, the province job and the airfield card read it (null with the switch off, or no field). Keys: 'work:fieldfix:<id>' (glance tile), 'fieldfix:<id>' and 'fieldfix-undo:<id>' (province job), act 'fieldfix:<id>' (card). */
export function fieldRepairView(g: Game, region: number, viewer: string | null = g.human): FieldRepairView | null {
  return fieldRepairViewOf(g, region, viewer);
}

/** Repairs one of the player's fields, paid at once (the bridge's name). */
export const orderFieldRepair = (g: Game, region: number): boolean => humanUp(g) && repairFieldFor(g, g.human!, region);

/** Takes back the player's field repair of this month, with his money (the bridge's name). */
export const undoFieldRepair = (g: Game, region: number): boolean => humanUp(g) && undoFieldFor(g, g.human!, region);

/** A province's airfield as the city glance and the airfield card show it: tier, room, shut, levels burned, the repair, its price and why not. Null without a field or with the switch off. */
export function airfieldState(g: Game, region: number): FieldView | null {
  return fieldRulesOf(g) ? fieldView(g, region, g.human) : null;
}

/** Every damaged field on the map, for the map's marks (the 3D table shows only those the player may know of). */
export const airfieldMarks = (g: Game): ReturnType<typeof fieldsDamaged> => fieldsDamaged(g);

/** Repairs one of the player's fields, paid at once: the runway opens next month, a burned level each month after. */
export function orderAirfieldRepair(g: Game, region: number): boolean {
  return orderFieldRepair(g, region);
}

/** Takes back a field repair the player ordered this month, with his money. */
export function orderUndoAirfieldRepair(g: Game, region: number): boolean {
  return undoFieldRepair(g, region);
}

export interface BuildingTip { region: number; town: string; province: string; offer: CivicOffer }

/** The buildings worth ordering next, for a list on the Build tab: for each line, the richest of the player's places that can take it now. */
export function buildingTips(g: Game, most = 6): BuildingTip[] {
  const me = g.human;
  if (!me || !civicRules(g) || !g.nations[me].alive) return [];
  const worth = (r: Region): number => (g.nations[me].capital === r.id ? 100 : 0) + r.vp * 3 + r.fac * 2 + (r.terrain === 'urban' ? 3 : r.terrain === 'plains' ? 2 : 1) + (r.port ? 1 : 0);
  const home = g.regions.filter((r) => r.ctrl === me && r.owner === me).sort((a, b) => worth(b) - worth(a) || a.id - b.id);
  const keeps = keepsStores(g, me);
  const out: BuildingTip[] = [];
  for (const l of CIVIC.lines) {
    for (const r of home) {
      const o = civicOffers(g, me, r.id, keeps).find((x) => x.line === l.id);
      if (o && (o.why === null || o.poor)) { out.push({ region: r.id, town: townName(r), province: r.name, offer: o }); break; }
    }
  }
  return out.slice(0, most);
}

/** The player's trade: agreements in force, what each pays and how the goods go, and every country he could sign with. Null in a history campaign. */
export function tradeLedger(g: Game, nat: string | null = g.human): TradeBook | null {
  return nat ? tradeBook(g, nat) : null;
}

/** Signs a trade agreement with another country. It costs political capital. */
export function orderTrade(g: Game, nat: string): boolean {
  return humanUp(g) && !!signTrade(g, g.human!, nat);
}

/** Ends the player's trade agreement with another country. */
export function orderEndTrade(g: Game, nat: string): boolean {
  return humanUp(g) && endTrade(g, g.human!, nat);
}

/** The player bids for a neutral's bargain whose round is open (release 2). Returns why not, or null when the bid stands. */
export function orderBid(g: Game, id: string, pp: number): string | null {
  return humanUp(g) ? placeBid(g, g.human!, id, pp) : 'It is not your month.';
}

/** The player, as the seller, chooses a buyer for a bargain from the bids that stand, or nobody (release 2). */
export function orderSellTo(g: Game, id: string, buyer: string | null): string | null {
  return humanUp(g) ? chooseBuyer(g, g.human!, id, buyer) : 'It is not your month.';
}

/** The player, at peace outside both sides, sends a legion of volunteers to a patron against its enemy (release 2). Returns why not, or null when it is sent. */
export function orderLegion(g: Game, patron: string, against: string): string | null {
  return humanUp(g) ? sendLegion(g, g.human!, patron, against) : 'It is not your month.';
}

/** The player calls his legion home: it leaves at the next month's start (release 2). Returns why not, or null. */
export function orderRecall(g: Game): string | null {
  return humanUp(g) ? recallLegion(g, g.human!) : 'It is not your month.';
}

// ---- Spies and the fog of war (the grand campaign) -----------------------------------------------------

/**
 * What the player can see of every province, by region id: 2 where an army is seen whole, 1 where only its flag
 * and the number of its corps are known, 0 where nothing is known. Who holds the ground is always known. Everything
 * is 2 in a historical scenario, with nobody at the controls, or once the player has lifted the fog.
 */
export function sight(g: Game): Uint8Array {
  return sightOf(g, g.human);
}

/**
 * Every army the player knows of, one row for each nation's force in a province: the number of corps, and the
 * divisions it has left where it is seen whole (null where only glimpsed). Armies in unseen ground are not listed:
 * a map drawn from this list shows exactly what the player is allowed to know.
 */
export function visibleArmies(g: Game): ArmySeen[] {
  return armiesSeen(g, g.human);
}

/**
 * The secret service's room: every agent with its rank, where it is, what it is doing, the chance of being caught
 * this month, the work it can be given there (with the reason for any it cannot) and the armies it has in sight;
 * how many agents the service can run and what one costs; the service's file of what has happened; the nations
 * whose books are being read; and the chance of the other side's saboteurs striking at home. Null outside the grand campaign.
 */
export function secretService(g: Game): SpyRoom | null {
  return spyRoom(g, g.human);
}

/** Why the player cannot recruit an agent now, or null if he can. */
export function whyNotRecruitAgent(g: Game): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotRecruit(g, g.human);
}

/** Recruits an agent for the player: paid for at once, trained at home, ready for orders next month. */
export function orderRecruitAgent(g: Game): Agent | null {
  return whyNotRecruitAgent(g) ? null : recruit(g, g.human!);
}

/** Why the player's agent cannot be sent to a province, or null if it can. */
export function whyNotSendAgent(g: Game, id: number, region: number): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotSend(g, g.human, id, region);
}

/** Sends the player's agent to any province on the map: a month on the road, then watching abroad or on guard at home. */
export function orderSendAgent(g: Game, id: number, region: number): boolean {
  return whyNotSendAgent(g, id, region) === null && sendAgent(g, g.human!, id, region);
}

/** Why the player's agent cannot be given this work where it stands, or null if it can. */
export function whyNotAgentMission(g: Game, id: number, mission: Mission): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotMission(g, g.human, id, mission);
}

/** Gives the player's agent its work: 'watch', 'books', 'factory', 'rail', 'oil' or 'guard'. */
export function orderAgentMission(g: Game, id: number, mission: Mission): boolean {
  return whyNotAgentMission(g, id, mission) === null && setAgentMission(g, g.human!, id, mission);
}

/** Why the player cannot arrest this uncovered enemy agent now, or null (ruling 151(c)). */
export function whyNotArrestAgent(g: Game, id: number): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotArrest(g, g.human, id);
}

/** ARREST: ends an enemy agent the player's hunters uncovered (ruling 151(c)). */
export function orderArrest(g: Game, id: number): boolean {
  return whyNotArrestAgent(g, id) === null && arrest(g, g.human!, id);
}

/** Why the player cannot turn this uncovered enemy agent now, or null (double agents, Brian, 22 November, release 2). */
export function whyNotTurnAgent(g: Game, id: number): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotTurn(g, g.human, id);
}

/** TURN: an enemy agent the player's hunters uncovered goes back to its service as his double agent. */
export function orderTurn(g: Game, id: number): boolean {
  return whyNotTurnAgent(g, id) === null && turnFoe(g, g.human!, id) !== null;
}

/** Retires one of the player's agents, which frees a place in the service. */
export function orderRetireAgent(g: Game, id: number): boolean {
  return !!g.human && humanUp(g) && retire(g, g.human, id);
}

/** Lifts the fog of war for this campaign, or lays it again. Agents and sabotage go on either way. */
export function orderFog(g: Game, on: boolean): boolean {
  return !!g.human && setFog(g, on);
}

/** A foreign government's books as the player's agent in its capital reads them, or null where none is reading. An ally's are always open. */
export function foreignBooks(g: Game, nat: string): Books | null {
  return g.human ? booksOf(g, nat, g.human) : null;
}

/** What the player's agent in a province reports of it beyond its armies: what is on order there and the air fleets based there. Null without one. */
export function agentReport(g: Game, region: number): CityReport | null {
  return g.human ? cityReport(g, region, g.human) : null;
}

// ---- What is on order, place by place (Version 78; orders.ts) ----

/**
 * Every place where the player has something on order: what, how many of each, the months left for each and where
 * it will stand when ready. Places with units forming come first, the soonest first. Also what his agents report
 * of other countries' orders, marked `spied`. Display only: one call for the browser page and the 3D game.
 */
export function ordersByCity(g: Game): CityOrders[] {
  return g.human ? cityOrders(g, g.human) : [];
}

/** The same for one province, or null where the player has nothing on order there and no agent's report. */
export function ordersIn(g: Game, region: number): CityOrders | null {
  return g.human ? ordersAt(g, region, g.human) : null;
}
export type { CityOrders, OrderLine };

// ---- Rival staffs: the character of each computer staff, and its main effort where the player may know it ----

/** One main effort of a staff, as the player is allowed to know it. */
export interface EffortView {
  /** "the summer offensive towards Smolensk". */
  name: string;
  /** The province it is aimed at this month, and the one it was named for. */
  target: number; targetName: string; first: number; firstName: string;
  /** Whom it is against, the months it runs (turn numbers), and the provinces it has taken so far. */
  enemy: string; from: number; to: number; won: number;
  /** True when it is aimed at the player or one of his allies. */
  atYou: boolean;
}

/** A computer nation's general staff as the player knows it. */
export interface StaffView {
  nat: string;
  /** Its character in this war, the word for it and the sentence that says how it fights. Always known. */
  character: Character; label: string; says: string;
  /** How the player knows its plans: an ally tells him, or his agent reads its books in its capital. Null: he does not. */
  source: 'ally' | 'agent' | null;
  /** Its main efforts under way, where the player knows its plans. Empty when he does not, or when it has none. */
  efforts: EffortView[];
  /** The general offensives it has under way, where the player knows its plans: against whom and since when. */
  offensives: { enemy: string; since: number }[];
  /** One sentence on what it is doing, ready to show under its name. */
  line: string;
}

/** The staff of one computer-run nation as the player knows it, or null: the player's own, a fallen one, or outside the grand campaign. */
export function rivalStaff(g: Game, nat: string): StaffView | null {
  const n = g.nations[nat];
  if (!rivalRules(g) || !n?.alive || n.dormant || nat === g.human) return null;
  const character = characterOf(g, nat);
  const C = RIVALS.characters[character];
  const source = plansKnown(g, nat);
  const me = g.human;
  const name = (id: string): string => g.nations[id]?.name ?? id;
  const efforts: EffortView[] = !source ? [] : effortsOf(g, nat).map((e) => ({
    name: e.name, target: e.target, targetName: g.regions[e.target].name, first: e.first, firstName: g.regions[e.first].name,
    enemy: e.enemy, from: e.from, to: e.to, won: e.won, atYou: me !== null && (e.enemy === me || allied(g, me, e.enemy)),
  }));
  const offensives: StaffView['offensives'] = [];
  if (source) {
    for (const e of n.atWar) {
      // (The far-side fronts' word is kept under a key of its own, staff.ts `farWordKey`: never written with the far side off.)
      const since = Object.keys(g.flags).filter((k) => k === `st:on:${nat}:${e}` || k === `st:on:${nat}:${e}:far` || k.startsWith(`st:on:${nat}:${e}:t`)).reduce((s, k) => Math.max(s, g.flags[k]), 0);
      // (Only one that is being fought: opened lately, or with a battle of its own in the last three months.)
      if (since && g.nations[e]?.alive && (g.turn - since <= 3 || g.turn - (n.lastFight[e] ?? -99) <= 3)) offensives.push({ enemy: e, since });
    }
  }
  const cap = n.capital >= 0 ? g.regions[n.capital].name : 'its capital';
  const line = !n.atWar.length ? 'At peace: its staff has no plan of attack.'
    : !source ? `Where its main effort will fall is not known. An agent reading its books in ${cap} would learn it.`
    : efforts.length ? efforts.map((e) => `Main effort: ${e.name}, against ${name(e.enemy)}, until ${turnToDate(e.to).label}${e.target !== e.first ? `. It has moved on to ${e.targetName}` : ''}.`).join(' ')
    : offensives.length ? `A general offensive against ${offensives.map((o) => name(o.enemy)).join(' and ')}, under way since ${turnToDate(Math.min(...offensives.map((o) => o.since))).label}.`
    : 'It stands on the defensive: no main effort is named.';
  return { nat, character, label: C.label, says: C.says, source, efforts, offensives, line };
}

/** Every computer-run nation's staff as the player knows it: the nations at war first, strongest first. */
export function rivalStaffs(g: Game): StaffView[] {
  return Object.values(g.nations)
    .sort((a, b) => (b.atWar.length ? 1 : 0) - (a.atWar.length ? 1 : 0) || b.armyPeak - a.armyPeak || (a.id < b.id ? -1 : 1))
    .map((n) => rivalStaff(g, n.id))
    .filter((v): v is StaffView => v !== null);
}

// ---- World Domination: the last act of the grand campaign ------------------------------------------------

/** Where the campaign stands with its last act: whether the war between the alliances is decided, and what the player has chosen. */
export interface LastAct {
  /** How the war between the alliances was decided, or null while it is still being fought. */
  decided: Decided | null;
  /** True while the player may still choose World Domination: the war is decided and the last act has not begun. */
  open: boolean;
  /** Why it cannot be chosen today, in words, or null. */
  why: string | null;
  /** The month the player took the victory and left the world as it is, or 0. The last act stays open after that. */
  peace: number;
  /** True once the last act has begun, and the month it did. */
  on: boolean;
  since: number;
  /** How many nations still stand. */
  left: number;
  /** The nation that has won the last act, or null. */
  winner: string | null;
}

/** The state of the campaign's last act, for an interface. Null outside the grand campaign or with nobody at the controls. */
export function lastAct(g: Game): LastAct | null {
  if (g.flags.open !== 1 || !g.human) return null;
  const why = whyNotDomination(g, g.human);
  return {
    decided: warDecided(g), open: why === null, why,
    peace: g.flags['dom:peace'] ?? 0,
    on: dominating(g), since: g.flags.dom ?? 0,
    left: contenders(g).length,
    winner: worldWinner(g),
  };
}

/** Why the player cannot open the last act now, or null when he can. */
export function whyNotOrderDomination(g: Game): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  if (!humanUp(g)) return 'Orders are given in your own month.';
  return whyNotDomination(g, g.human);
}

/** The player chooses World Domination: every alliance is dissolved and every nation still standing is at war with every other. It cannot be undone. */
export function orderDomination(g: Game): boolean {
  return whyNotOrderDomination(g) === null && startDomination(g, g.human);
}

/** The player takes the victory and leaves the world as it is. World Domination can still be chosen in a later month. */
export function orderKeepPeace(g: Game): boolean {
  return !!g.human && humanUp(g) && keepPeace(g);
}

// ---- The Winter War's peace (ruling 228, 1b) -------------------------------------------------------------------
// The card and the standing offer for a Soviet or a Finnish player: winterPeace() gives the words for both builds,
// winterCardDue() says when the card comes up at the month's start (and marks it seen), and the order takes the peace
// on this month's terms. Fighting on needs no order: the offer stands while the war lasts.
export { winterPeace, winterCardDue, type WinterPeace } from './winter';

export function whyNotOrderWinterPeace(g: Game): string | null {
  return whyNotWinterPeace(g, humanUp(g));
}

export function orderWinterPeace(g: Game): boolean {
  return takeWinterPeace(g, humanUp(g));
}

/** The standings of the world: who still stands, how much of the map each holds, and who has fallen to whom. Foreign armies are as well known as the fog allows. */
export function standings(g: Game): WorldBoard {
  return worldBoard(g, g.human);
}

/** Why the player cannot begin this technology now, or null when a free research team can start on it. */
export function whyNotOrderResearch(g: Game, id: string): string | null {
  if (!g.human) return 'Nobody is at the controls.';
  return whyNotResearch(g, g.human, id);
}

/** Puts one of the player's free research teams to work on a technology. */
export function orderResearch(g: Game, id: string): boolean {
  return humanUp(g) && startResearch(g, g.human!, id);
}

/** Takes the player's team off a technology. The work done on it is lost. */
export function orderStopResearch(g: Game, id: string): boolean {
  return humanUp(g) && stopResearch(g, g.human!, id);
}

/** Puts this many of the player's design teams on a branch of research. */
export function orderTeams(g: Game, field: Field, teams: number): boolean {
  return humanUp(g) && setTeams(g, g.human!, field, teams);
}

/** Founds a new design team for the player, paid for from production. */
export function orderBureau(g: Game): boolean {
  return humanUp(g) && foundBureau(g, g.human!);
}

/** Ends the player's month and lets the computer play until the player is up again. Returns false if the player's nation has fallen. */
export function passMonth(g: Game): boolean {
  if (humanUp(g)) endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 80) {
    if (g.human === null || !g.nations[g.human].alive) return false;
    stepAI(g);
  }
  if (!humanUp(g)) return false;
  beginHuman(g);
  return true;
}

export function orderEvacuate(g: Game, c: Corps): boolean {
  return ownOrder(g, c) && evacuate(g, c);
}

/** Nations the player could declare war on: neighbours by land that are not already friends or enemies. */
// ---- The fall of nations and their return ------------------------------------------------------------

/**
 * The announcements both interfaces make: every nation that has left the war and every one that has come back into
 * it, in words, oldest first. Give a month to have only that month's. Each carries the nation, who did it, and the
 * capital to show on the map.
 */
export function announcements(g: Game, turn?: number): PassingCard[] {
  return passingsIn(g, turn).map((p) => passingCard(g, p));
}

/** One announcement by its number on the roll, or null. */
export function announcement(g: Game, id: number): PassingCard | null {
  const p = g.passings.find((x) => x.id === id);
  return p ? passingCard(g, p) : null;
}

/**
 * Every fallen country as the player sees its chances: when it fell, to whom, who holds its capital, whether it can
 * come back at all, and whether the player taking that capital would free it.
 */
export function nationsFallen(g: Game): FallenRow[] {
  return fallenNations(g, g.human);
}

/** The fallen country whose capital this province is, or null: what the province panel says about freeing it. */
export function fallenCapitalAt(g: Game, region: number): FallenRow | null {
  return fallenNations(g, g.human).find((f) => f.capital === region) ?? null;
}

/**
 * The player's country has fallen: the player steps back and watches the war go on. The seat is remembered, and if
 * the country is liberated the player is given it back (g.human is that country again). Returns the country lost.
 */
export function playerFell(g: Game): string | null {
  return g.human && !g.nations[g.human].alive ? standDown(g) : null;
}

/** The country the player lost and may yet be given back, or null. */
export function playerLost(g: Game): string | null {
  if (g.human) return null;
  const k = Object.keys(g.flags).find((x) => x.startsWith('was:'));
  return k ? k.slice(4) : null;
}

// ---- Armies opened up: divisions, morale and rest ---------------------------------------------------

/**
 * One nation's army in a province as the player knows it: what it is made of arm by arm ("5 infantry, 4 armoured
 * and 2 artillery divisions"), and each corps with its divisions as cards, its experience, its morale and its rest.
 * The player's own armies and their allies' are known in full. An enemy's is known in full only where the fog
 * allows; with a glimpse, only how many corps stand under the flag ('known' is 'count'); unseen, null.
 */
export const armyAt = (g: Game, region: number, nat: string): ArmyCard | null => armyCard(g, region, nat, g.human);

/** Every army in a province that the player knows anything of, the player's own first and the largest after. */
export function armiesAt(g: Game, region: number): ArmyCard[] {
  const r = g.regions[region];
  if (!r) return [];
  const nats = [...new Set(r.units.filter((c) => !c.dead).map((c) => c.nat))];
  const cards = nats.map((n) => armyCard(g, region, n, g.human)).filter((a): a is ArmyCard => a !== null);
  return cards.sort((a, b) => Number(b.nat === g.human) - Number(a.nat === g.human) || b.divs - a.divs || b.corpsCount - a.corpsCount);
}

/** One corps opened up: its divisions, its record, its morale and its rest. For the player's own corps and allies': a foreign corps goes through corpsKnownAs. */
export const corpsDetail = (g: Game, c: Corps): CorpsCard => corpsCard(g, c);

/**
 * How well the player knows a corps: 'exact' (its own, an ally's, or a foreign corps identified this month), 'report'
 * (identified 'age' months ago: a dated report gone stale) or 'none' (seen, but not identified). A foreign corps is
 * identified by fighting it, by an agent at work with it in view, or by a reconnaissance flight over its province.
 */
export const knowledgeOf = (g: Game, c: Corps): { level: Known; age: number } => corpsKnown(g, c, g.human);

/** A corps as the player knows it: its card where it is identified (masked where the report is stale), null where it is not. */
export const corpsKnownAs = (g: Game, c: Corps): CorpsCard | null => corpsSeen(g, c, g.human);

/**
 * How a corps feels: morale and rest, each 0 to 1, each with its word ("Confident", "Rested"), which way morale is
 * going, a sentence on why, and the reasons one by one. Morale is null in a historical scenario, which keeps none.
 */
export const moodOf = (g: Game, c: Corps): Mood => corpsMood(g, c);

// ---- Cities and towns: where the valuable places are, and what taking one earns ---------------------

/**
 * Every province's chief place for a map to draw: its name, its size ('capital', 'city', 'town' or 'village'), where it
 * stands (x, y on the campaign map, a fixed place beside the province's centre), the signs it carries (factory, harbour,
 * oil, fort, yard, airfield), what it pays its own country a month, and that worth ranked 0 to 5.
 */
export const towns = (g: Game): TownView[] => townsOf(g);

/** The chief place of one province. */
export const townIn = (g: Game, region: number): TownView | null => townAt(g, region);

/**
 * What a province is worth to the player: what it gives if it is the player's own ('yours'), what it gives the ally who
 * holds it ('ally'), what taking it would bring ('taken'), or what freeing it for its owner would do ('restored'). A
 * sentence to show before an attack ('line'), and the items one by one: money, industry, oil, harbour, airfield,
 * railway junction, victory points, a capital, a country to be freed, a fortress in the way.
 */
export const prize = (g: Game, region: number): Prize | null => prizeOf(g, region, g.human);

/** The richest prizes the player could take next: enemy ground beside the player's own or beside the player's troops. */
export const prizesNear = (g: Game, most = 5): Prize[] => prizesInReach(g, g.human, most);

// ---- The player's generals ----------------------------------------------------------------

const myGeneral = (g: Game, id: number): General | null => {
  const gen = g.generals[id];
  return gen && gen.nat === g.human && current(g) === gen.nat ? gen : null;
};

/** Gives one of the player's generals a corps. */
export function orderAssign(g: Game, general: number, corps: number): boolean {
  const gen = myGeneral(g, general);
  const c = g.corps.find((x) => x.id === corps);
  return !!gen && !!c && c.nat === g.human && assignGeneral(g, gen, c);
}

export function orderRelieve(g: Game, general: number): boolean {
  const gen = myGeneral(g, general);
  return !!gen && relieve(g, gen);
}

export function orderSack(g: Game, general: number): boolean {
  const gen = myGeneral(g, general);
  if (!gen) return false;
  // A general the war cabinet asked to see dismissed: its trust recovers, and it carries what his popularity would have cost.
  const asked = headAsked(g, gen), before = g.nations[gen.nat].pc;
  if (!sack(g, gen)) return false;
  if (asked) headGiven(g, gen, before - g.nations[gen.nat].pc);
  return true;
}

/** Promotes an unknown officer to command one of the player's corps. */
export function orderPromote(g: Game, corps: number): General | null {
  const c = g.corps.find((x) => x.id === corps);
  return c && ownOrder(g, c) ? promote(g, c) : null;
}

export function orderSkill(g: Game, general: number, skill: Skill): boolean {
  const gen = myGeneral(g, general);
  return !!gen && spendPoint(gen, skill);
}

export function declarable(g: Game): string[] {
  const me = g.human;
  if (!me || !g.nations[me].alive || onEve(g)) return [];
  const out = new Set<string>();
  const add = (o: string): void => {
    const N = g.nations[o];
    if (N && N.alive && !sameSide(g, me, o) && !atWar(g, me, o)) out.add(o);
  };
  // (And the owner of ground an enemy's troops stand on by agreement, which is shut to his army while he is at peace
  // with that owner: far.ts `stationBars`. The refusal tells him to declare war on the owner, so he must be able to.)
  const reached = (r: Region): void => { add(r.ctrl); if (stationBars(g, me, r)) add(r.owner); };
  for (const r of g.regions) {
    if (!allied(g, me, r.ctrl)) continue;
    for (const j of r.nb) reached(g.regions[j]);
  }
  // A nation with troopships can also reach any coast within landing range of its own ports.
  if (g.nations[me].sealift > 0) {
    const depth = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (const r of g.regions) if (r.port && r.ctrl === me) for (const z of r.seas) if (depth[z] < 0) { depth[z] = 1; q.push(z); }
    for (let i = 0; i < q.length; i++) {
      for (const id of g.seas[q[i]].coast) reached(g.regions[id]);
      if (depth[q[i]] >= NAVY.landing.range) continue;
      for (const j of g.seas[q[i]].adj) if (depth[j] < 0 && canPass(g, q[i], j, me)) { depth[j] = depth[q[i]] + 1; q.push(j); }
    }
  }
  // A country that ships its army across the ocean reaches its enemies through its friends' ports (ruling 31).
  for (const id of acrossTheWater(g, me, majorsOf(g))) add(id);
  return [...out];
}

/**
 * Ground the player's corps can march into only by declaring war: the land of a country he is at peace with, next
 * to where one of the corps stands. Each region is given with the country that holds it. Either interface shows
 * the declaration prompt (previewDeclare) when a corps is sent there, and nothing moves unless the player says yes.
 */
export function invasions(g: Game, cs: Corps[]): Map<number, string> {
  const out = new Map<number, string>();
  if (!humanUp(g) || onEve(g)) return out;
  const can = new Set(declarable(g));
  if (!can.size) return out;
  for (const c of cs) {
    // War is declared by the player's own corps crossing the frontier, not by an ally's.
    if (!ownOrder(g, c) || c.ap <= 0) continue;
    for (const j of g.regions[c.reg].nb) {
      const r = g.regions[j];
      if (can.has(r.ctrl) && !blocked(g, c, r, c.reg)) out.set(j, r.ctrl);
    }
  }
  return out;
}

/** What declaring war on a country would mean: who it joins, whether the blow is a surprise, its friends, and how the neutrals take it. */
export function previewDeclare(g: Game, on: string): WarForecast | null {
  return g.human && declarable(g).includes(on) ? warForecast(g, g.human, on) : null;
}

// ---- A small country's path, and the League ------------------------------------------------------------------

/**
 * The paths open to a small country in the player's hands: join the Axis, join the Allies, stand alone, found the
 * League or join it, each with whether it can be taken today and why not. Empty for a great power, for a watcher,
 * and outside the grand campaign. Either interface shows them on one card.
 */
export function paths(g: Game): PathOption[] {
  return g.human ? pathOptions(g, g.human) : [];
}

/** The countries the player could found the League with, each with its answer and the reason. */
export function leaguePartners(g: Game): Invite[] {
  return g.human && paths(g).length ? founders(g, g.human) : [];
}

/** Takes a path. Joining an alliance cannot be undone. Founding the League needs the partner who founds it with the player. */
export function orderPath(g: Game, id: PathId, partner?: string): boolean {
  return humanUp(g) && !onEve(g) && takePath(g, g.human!, id, partner);
}

/** The countries the player, as a member of the League, could invite, each with its answer and the reason. */
export function leagueInvites(g: Game): Invite[] {
  return g.human ? invites(g, g.human) : [];
}

/** Why the player cannot invite this country into the League today, or null if he can. */
export function whyNotLeagueInvite(g: Game, target: string): string | null {
  if (!g.human || !humanUp(g) || onEve(g)) return 'Not in your month.';
  return whyNotInvite(g, g.human, target);
}

/** Invites a willing country into the League, for political capital. */
export function orderLeagueInvite(g: Game, target: string): boolean {
  return whyNotLeagueInvite(g, target) === null && invite(g, g.human!, target);
}

export function orderDeclare(g: Game, on: string): boolean {
  if (!g.human || current(g) !== g.human || !declarable(g).includes(on)) return false;
  const was = bloc(g.nations[g.human]) ?? 'none';
  declareWar(g, g.human, on);
  // At war with an Axis power, a country across the ocean stands with the Allies (ruling 32).
  standWithAllies(g, g.human, was);
  // The plan's strike falls in the month of the declaration (the far side, ruling 200(c)). A computer's staff flies
  // it as its great choice is taken (ai.ts); a player's staff, where he has handed it the navy, flies it as he
  // declares, at each harbour of the country declared on that his country's plan names. A harbour out of the
  // fleet's reach, or with no ship of the enemy's in it, is not struck (carriers.ts, `plannedStrike`).
  if (staffRuns(g, g.human, 'navy')) for (const op of ops(g.human)) for (const st of op.strikes ?? []) if (g.byName[st.harbour]?.ctrl === on) plannedStrike(g, g.human, st.fleet, st.harbour, st.home);
  ensureSupply(g);
  return true;
}

/** Spends the player's political capital to pull a neutral toward the player's bloc. */
export function orderCourt(g: Game, target: string): boolean {
  return humanUp(g) && court(g, g.human!, target);
}

export { KIND };
