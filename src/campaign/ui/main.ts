// The campaign screen: a staff map of Europe, the armies on it, and the paperwork a
// commander would see. All of the rules live in ../game, ../ai and ../turn; this file
// only draws the state of the game and turns clicks into orders.

import { flagDefs, flagIcon, flagUse } from './flags';
import grandData from '../../../data/campaign/grand.json';
import {
  DEFENCE, DESERT, allied, atWar, chanceOn, collapseScore, convoyDrag, createGame, ensureSupply, evenOdds, fieldArmy, frozen, hostileUnits, inDesert, limitAttackers, lineStrength, men, railReach, refuge, weather,
} from '../game';
import {
  beginEve, beginHuman, canOrder, candidates, declarable, endEve, endHuman, eveAttacker, evePlan, humanUp, landTargets, onEve, orderAttack, orderBuildCorps, orderBuildFort, orderEvePlan,
  orderAir, orderAirBase, orderAssign, orderBureau, orderCancelBuild, orderCourt, orderDeclare, orderEvacuate, orderFleet, orderLand, orderMove, orderPromote, orderRail, orderRebase, orderRelieve, orderSack, orderShip, orderSkill, orderStance, orderTeams, postureOf,
  pocketAt, previewAirOrder, previewAttack, previewLanding, reach, runTurnAI, shipTargets, stepAI, type Reach,
  airGroupBases, airGroupJobs, airfieldAt, airfieldsSeen, myAirGroups, orderGroupJob, orderRaid, orderScout, previewRaid, raidMenu, raidTargetsFor,
  fleetsInSight, orderSearch, searchOptionsFor, seaPictureOf, wingsOfFleet,
  orderCash, orderDrafts, orderSeen, arsenalOffers, orderArsenal, engineerJobs, orderBlowBridges, orderEngineers, orderStopWork, whyNotBlowBridges, orderCutRailway, whyNotCutRailway,
  invasions, previewDeclare, announcement, fallenCapitalAt, nationsFallen, playerFell, playerLost,
  armyAt, corpsDetail, moodOf, prize, prizesNear, townIn, towns, attackOutlook, openingForce, corpsKnownAs, knowledgeOf,
  fieldworksLeft, foldChoicesAt, orderFold, orderProbe, softenOptions,
  joinOffers, jointForce, lending, orderJoin, orderJoinAll, orderSendBack,
  fleetOptions, fleetView, lanesToGuard, orderBombard, orderEscort, orderMerge, orderSplit, previewBombard, whyNotSplit,
  cabinetBillOf, cabinetIsOff, callUpOf, governmentOf, headWantedOf, manpowerOf, officeIsSecure, orderAppoint, orderCabinet, orderCallUp, orderDecree, orderEarlyClass, orderOffice, orderPromise, orderSetAside, orderStandBy, orderTakeBack, promiseTargetsOf, promiseTermsOf, whyNotOrderPromise, buildingTips, orderBuilding, orderEndTrade, orderTrade, townBuildings, tradeLedger, type BuildingTip, type TownBuildings,
  leagueInvites, leaguePartners, orderLeagueInvite, orderPath, paths, whyNotLeagueInvite,
  orderResearch, orderStopResearch,
  garrisonsAt, myGarrison, orderGarrison, orderSally, orderStorm, orderTakeField, sallyOutlook, siegesInSight, stormCandidates, stormOutlook, townSiege, whyNotGarrisonTown, whyNotLeaveTown, whyNotSallyOut, whyNotStormTown, type Outlook,
  orderBurn, orderScorchPolicy, orderWreck, prizesOf, prizesOnMap, whyNotWreckPrize,
  rivalStaff, rivalStaffs, type StaffView,
  agentReport, foreignBooks, orderAgentMission, orderFog, orderRecruitAgent, orderRetireAgent, orderSendAgent, lastAct, orderDomination, orderKeepPeace, secretService, standings, sight, whyNotRecruitAgent, whyNotSendAgent,
} from '../turn';
import { INTEL, armyKnown, fogOn, homeBase, intelRules, knowsBooks, type AgentView, type Known, type SpyRoom } from '../intel';
import { armyOf } from '../joint';
import { SIEGE, besiegers, garrisonOf, guardMen, salliers, siegeRules, stormForce, townHitText } from '../siege';
import { PRIZES, type PrizeKind, type ScorchPolicy } from '../prizes';
import { GOV, type GovView, type PromiseTarget, type PromiseTerms } from '../government';
import { MANPOWER, menWords } from '../manpower';
import { CIVIC, civicSum, type CivicOffer, type PlotView } from '../buildings';
import { dealsOf, housesOf, tradePath, tradeRules, type TradeOffer, type TradeRow } from '../trade';
import { TECH, idleTeams, techView, type TechNode } from '../tech';
import { GOOD_NAME, RES, fieldYield, keepsStores, resRules, seaLanes, standing as depotState, stores, type Book, type Lane, type ResLine } from '../resources';
import { LEAGUE, leagueFounded, leagueMembers, ownPath, small as isSmall, type PathId, type PathOption } from '../league';
import { ARSENAL, canStation, factoriesBuilt, workLevel, yardLevel, type Hull, type Offer } from '../arsenal';
import { TRAITS, eliteCount, eliteIn, eliteOf, eliteRule, traitRule, traitsOf } from '../traits';
import { barred, groundAttack, groundDefence, groundNotes, groundRules, groundSays, steep } from '../ground';
import { CROSSINGS, ENGINEERING, RIVERS, blocked, bridgeDown, noCrossing, riverNotes, whyBlocked } from '../rivers';
import { jobLine, minesAt, type JobKind } from '../engineers';
import { RAILWAYS, RAIL_LINKS, railCut, railNote } from '../rails';
import { groupRow, type AirfieldRow, type GroupRow, type RaidKind, type RaidPlan } from '../airwar';
import type { FleetSeen, WingCard } from '../carriers';
import { DIFFICULTY, FAIR, levelIndex, levelOf, setLevel, type Level } from '../difficulty';
import {
  AIR, CARRIERS, MISSION, carrierRules, groupAt, airBases, airDeliveries, airFleetsOf, airIncomeLoss, airPlanFor, airPool, airRange, airTargets, airWar, cover, crowding, forecastAir, isOffensive,
  km as airKm, seaKm, skyOver, whyNotBase, whyNotMission, type AirOrder,
} from '../air';
import {
  FLEETS, NAVY, SEA_BORDERS, airborne as byAir, assaultStrength, atSea, baseTargets, canPass, closedTo, fleetPower, fleetReach, fleetsOf, liftLeft, oreRoute, seaAt, seaRisk, seaSide, seaWar, shipCount, shipList, surfaceShips,
} from '../navy';
import { bombard, fleetDoing, squadronName, type FleetCard, type ShipCard } from '../fleets';
import { phaseFor, theatrePhases } from '../ai';
import { SIDE_NAME } from '../domination';
import { GOALS, GOAL_KEYS, goalChecks, goalState, goalWhen, judgeGoals, openGoals, weeksLasted } from '../goals';
import { RIVALS, fronts, isOpen, startOpen, type Front } from '../staff';
import { liberate } from '../liberation';
import { BUDGET, PRODUCTION, budgeted, canRaise, corpsCost, corpsUpkeep, fortCost, isHome, ledger, needsOf, provinceYield, raiseSite, specOf, type Ledger, type LedgerLine } from '../economy';
import { FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, inService, lead, monthsAhead, nextStep, researches, stepsOf, teamsFree, teamsTotal } from '../research';
import { loadGame, peekSave, saveGame, staleSave } from '../save';
import { COMMAND, SKILLS, corpsOf, fit, generalOf, popular, rankOf, serving, vacant, waiting, whyNotAssign, whyNotPromote, xpToNext } from '../commanders';
import { DIVISIONS, ordinal, type DivisionCard } from '../armies';
import { TOWNS, townName, townSize, type Prize, type TownView } from '../towns';
import type { Mood } from '../morale';
import { STORIES, awardOf, awardTitle, cashWorth, citation, designation, diaryText, fallenOf, fameOf, grade, honourDef, honoursOf, isGuards, raisedXp, revivable, rivalryOf, stat, takeOver, training, unitOf, unseen, whyNotCash } from '../stories';
import { DIPLOMACY, RELATIONS, TRACK, TRACK_LABEL, TRACK_PHRASE, canCourt, courtable, friendsOf, momentum, pcIncome, receptive, standing, step, tradePartners, type Standing } from '../diplomacy';
import { bloc, worksOf, worksShare } from '../game';
import { KIND, MEN_PER_DIV, type AirFleet, type AirMission, type AirReport, type AirSide, type Award, type BattleReport, type Cash, type Corps, type DiaryLine, type Fallen, type Storied, type UnitKind, type Field, type Fleet, type ShipClass, type Ships, type ShoreReport, type Game, type General, type Kind, type Mission, type Nation, type Posture, type Region, type SeaReport, type SeaSide, type Skill, type Stance } from '../types';
import { turnToDate } from '../../core/ticket';

let g: Game;
/** The month after the last one the historical plans cover: February 1945, when the Bulge has been closed. */
const STORY_ENDS = 66;
/** The first month in which only some fronts still follow history: January 1944. */
const STORY_THINS = 53;
/** A free-play war has no story to run out of: left on Play, it is stopped at the end of 1947 so that it does not run for ever. */
const FREE_ENDS = 100;
const ui = {
  sel: -1,
  picked: new Set<number>(),
  rail: false,
  ship: false,
  /** Choosing an enemy coast for the picked corps to land on. */
  land: false,
  /** The sea being looked at, when no region is. */
  sea: -1,
  /** The fleet being given orders under the Navy tab. */
  fleet: -1,
  stance: 'patrol' as Stance,
  /** Choosing a new home port for that fleet. */
  rebase: false,
  seenSea: 0,
  /** Where the list of shore bombardments stood when the player's last month ended: coasts of his shelled since are this month's. */
  shoreFrom: -1,
  /** True once the player has asked to see every line of a Battle Ticket on a narrow screen, where it opens folded to the lines that matter most. */
  ticketAll: false,
  /** The directive the player has last looked at in his Cabinet card, by its id. */
  govSeen: -1,
  /** The page of the cabinet room that is open: its business, the ministers, the decrees or the minutes. */
  govTab: 'now' as 'now' | 'table' | 'decrees' | 'minutes',
  // The wreck order the player has been asked to confirm (province:kind), and whether the country's scorched earth policy is unfolded.
  wreckAsk: '',
  polOpen: false,
  /** The air fleet being given orders under the Air tab. */
  air: -1,
  /** What a click on the map means for that air fleet: a new base, a province to photograph, or a target for a mission. */
  airMode: '' as '' | 'base' | 'scout' | 'search' | AirMission,
  /** The grand campaign's air war: the airfield opened under the Air tab, the raid planner's step (its menu, or the kind of raid whose targets the map shows), and the group the raid is being planned for. */
  field: -1,
  raid: '' as '' | 'menu' | RaidKind,
  lead: -1,
  seenAir: 0,
  /** Where the list of raids stood when the player's last month ended: those flown against him since are this month's. */
  raidsFrom: -1,
  /** How far down the roll of fallen and freed nations the player has been shown. */
  seenPass: 0,
  /** A one-line answer to the last click, shown where the hint is. */
  msg: '',
  /** False until a game has been started or resumed, so the title screen never overwrites a saved game. */
  live: false,
  tab: 'region' as 'region' | 'build' | 'research' | 'generals' | 'honours' | 'navy' | 'air' | 'spies' | 'reel' | 'battles' | 'powers',
  /** The agent being given orders under the Spies tab, whether the next click on the map sends it there, and one waiting for a yes to its retirement. */
  spy: -1,
  spySend: false,
  spyAsk: -1,
  /** The month whose secret reports the player has looked at. */
  spySeen: 0,
  view: { x: 380, y: 300, w: 620 },
  /** The map's lens: with 'worth' every province is tinted by what it pays, and every town shows its figures. */
  lens: '' as '' | 'worth',
  unread: 0,
  seenReports: 0,
  flash: [] as number[],
  auto: 0 as number,
  modal: null as null | { kind: string; [k: string]: any },
  /** The start screen: the grand campaign's front door, or the list of historical scenarios behind it. */
  menu: 'grand' as 'grand' | 'scenarios',
  /** Which shelf of the arsenal the Build tab shows in the grand campaign. */
  arm: 'army' as 'army' | 'air' | 'navy' | 'works' | 'towns',
  /** The country and the year picked for a grand campaign. An empty country watches. */
  gcNat: 'GER',
  gcYear: 1939,
  /** The level picked for it: Cadet to Field Marshal. */
  gcLevel: DIFFICULTY.start,
  /** Whether the grand campaign being set up has a war cabinet. */
  gcCabinet: true,
  /** Whether the campaign being set up has a government that cannot fall. */
  gcSecure: false,
};

const $ = (s: string): HTMLElement => document.querySelector(s) as HTMLElement;
const esc = (s: string): string => s.replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]!));
const num = (n: number): string => Math.round(n).toLocaleString('en-US');
/** Money as the interface shows it: a price or what is in hand, rounded down so nothing is promised that is not there. */
const money = (pp: number): string => Math.floor(pp * BUDGET.scale + 1e-6).toLocaleString('en-US');
/** A flow of money in the books, rounded to the nearest whole number, with the sign written out when asked. */
const cash = (pp: number, signed = false): string => { const v = Math.round(pp * BUDGET.scale); return `${v < 0 ? '−' : signed && v > 0 ? '+' : ''}${Math.abs(v).toLocaleString('en-US')}`; };
/** What is in the treasury: rounded down in credit, so it never shows more than can be spent, and as a debt when it is one. */
const inHand = (pp: number): string => (pp >= 0 ? money(pp) : cash(pp));
/** "1 division", "12 divisions". */
const divsOf = (n: number): string => `${num(n)} division${Math.round(n) === 1 ? '' : 's'}`;
const natName = (id: string): string => g.nations[id]?.name ?? id;
/** A nation's name as it reads inside a sentence: "the United Kingdom", "Germany". */
const theName = (id: string): string => { const nm = natName(id); return /^(United|Soviet|Netherlands)/.test(nm) ? `the ${nm}` : nm; };
const upFirst = (s: string): string => (s ? s[0].toUpperCase() + s.slice(1) : s);
/** "Germany", "Germany and Italy", "Germany, Italy, Hungary and 2 more". */
function nameList(ids: string[], most = 3): string {
  const names = ids.slice(0, most).map(theName);
  if (ids.length > most) names.push(`${ids.length - most} more`);
  return names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
}

/** The nation the player is running, while it is still in the war: the one whose friends and enemies the map is read by. */
const viewer = (): string | null => (g.human && g.nations[g.human]?.alive ? g.human : null);

/**
 * What a nation is to the player: one word for the badge, and the class that colours it. With nobody at the
 * controls there are no friends or enemies, so the word is the side the nation is on.
 */
function relationWord(nat: string): { kind: Standing | 'axis' | 'allies' | 'league'; word: string } {
  const me = viewer();
  const side = bloc(g.nations[nat]);
  if (me) {
    const kind = standing(g, me, nat);
    // A member of the League that is neither the player's ally nor his enemy is no ordinary neutral: say so.
    if (kind === 'neutral' && side === 'league') return { kind: 'league', word: 'League' };
    return { kind, word: kind === 'own' ? 'Yours' : kind === 'ally' ? 'Ally' : kind === 'enemy' ? 'Enemy' : 'Neutral' };
  }
  return side ? { kind: side, word: side === 'axis' ? 'Axis' : side === 'league' ? 'League' : 'Allies' } : { kind: 'neutral', word: 'Neutral' };
}

/** Where a nation stands, in a word or two and as a phrase: on the track between the blocs, or in the League. */
const standLabel = (n: Nation): string => (n.league ? 'League member' : TRACK_LABEL[n.align]);
const standPhrase = (n: Nation): string => (n.league ? `a member of ${LEAGUE.name}` : TRACK_PHRASE[n.align]);

/** The banner at the head of the region panel: whose ground this is to the player, in a word and a sentence. */
function standingNote(r: Region): string {
  const me = viewer();
  const h = r.ctrl, o = r.owner, n = g.nations[h];
  const { kind, word } = relationWord(h);
  const N = upFirst(theName(h));
  const foes = n.atWar.filter((x) => x !== me && g.nations[x]?.alive);
  const wars = foes.length ? `At war with ${nameList(foes)}.` : 'Not at war with anyone.';
  let line: string;
  if (!me) line = `${N} is ${standPhrase(n)}. ${wars}`;
  else if (kind === 'own') line = o === me ? 'Your own ground.' : 'Held by your troops.';
  else if (kind === 'enemy') line = `${N} is at war with you.${foes.length ? ` Also at war with ${nameList(foes)}.` : ''}`;
  else if (kind === 'ally') line = n.atWar.some((x) => g.nations[me].atWar.includes(x)) ? `${N} is on your side and fighting the same enemy.` : `${N} is on your side. ${wars}`;
  else if (bloc(n)) line = foes.length ? `${N} is ${standPhrase(n)}, but not at war with you. ${wars}` : `${N} is ${standPhrase(n)}, but is not at war with anyone.`;
  else line = `${N} is ${standPhrase(n)}. ${wars}`;
  // Occupied ground: say where the nation it was taken from stands as well.
  if (o !== h && g.nations[o]) {
    const O = upFirst(theName(o));
    if (!g.nations[o].alive) line += ` ${O}, whose ground this was, has fallen.`;
    else if (me && o !== me) { const k = standing(g, me, o); line += ` ${O}, whose ground this is, is ${k === 'ally' ? 'your ally' : k === 'enemy' ? 'your enemy too' : 'neutral'}.`; }
    else if (me) line += ' This is your own ground, in their hands.';
    else line += ` The ground is ${g.nations[o].adj}.`;
  }
  return `<div class="rel ${kind}" style="--side:${n.color}"><b>${word}</b><span>${esc(line)}</span></div>`;
}
const date = (t: number): string => turnToDate(t).label;
const capital = (t: string): string => (t ? t[0].toUpperCase() + t.slice(1) : t);
/** Enemy coasts the picked corps could land on, worked out once each time the screen is drawn. */
let lands: Region[] = [];
/** What each picked corps can reach this month, kept from the last redraw for drawing its route under the cursor. */
let reaches: { from: number; ap: number; map: Map<number, Reach> }[] = [];
/** The army whose general and units the tray shows. */
let trayArmy: { reg: number; nat: string } | null = null;
/** The enemy coasts the fleet that is picked up could shell from where it is. */
let shellAt: number[] = [];
/** The piece kept off the map while a copy of it is still being slid along its road, and the march being drawn. */
let marchHide: { reg: number; nat: string } | null = null;
/** What the player can see of each province this moment: 2 an army in full, 1 a flag and a count, 0 nothing (the fog of war). */
let seen: Uint8Array = new Uint8Array(0);
/** True for the player's own nation and its allies, whose armies are never hidden. With nobody at the table, for everyone. */
const friendOf = (nat: string): boolean => !g.human || !g.nations[g.human].alive || allied(g, nat, g.human);
/** True if everything of this nation's that stands in a province is known to the player. */
const inFull = (r: Region, nat: string): boolean => (seen[r.id] ?? 2) >= 2 || friendOf(nat);
/** How well the player knows a corps: the player's own and allies' always exactly, a foreign one only as far as it has been identified. */
const kn = (c: Corps): Known => (friendOf(c.nat) ? 'exact' : knowledgeOf(g, c).level);
const allKnown = (cs: Corps[]): boolean => cs.every((c) => kn(c) === 'exact');
/** The staff's estimate of an army in words: "about 7 to 12 divisions". */
const estWords = (e: { lo: number; hi: number } | null): string => {
  if (!e) return 'strength not known';
  const lo = Math.max(1, Math.round(e.lo)), hi = Math.max(lo, Math.round(e.hi));
  return lo === hi ? `about ${lo} division${lo === 1 ? '' : 's'}` : `about ${lo} to ${hi} divisions`;
};
/** A band in words: "3 to 6", or the one figure where both ends are the same. */
const span = (lo: number, hi: number): string => (lo === hi ? `about ${num(lo)}` : `${num(lo)} to ${num(hi)}`);
const ICON_CAMERA = '<svg class="cam" viewBox="0 0 16 12" width="15" height="11" aria-hidden="true"><path d="M1.5 3h3l1-1.6h5l1 1.6h3v8h-13z" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"/><circle cx="8" cy="6.8" r="2.3" fill="none" stroke="currentColor" stroke-width="1.2"/></svg>';
const HOW_TO_KNOW = 'A battle, an agent on watch or a reconnaissance flight would identify them.';
let marchToken = 0;
/** The region whose route is drawn under the cursor. */
let routeShown = -1;
// Until this moment (performance.now), a click off the map is the echo of a tap on the map and is ignored.
let ghostUntil = 0;

function mix(a: string, b: string, t: number): string {
  const p = (h: string, i: number) => parseInt(h.slice(1 + 2 * i, 3 + 2 * i), 16);
  const c = [0, 1, 2].map((i) => Math.round(p(a, i) * (1 - t) + p(b, i) * t));
  return `rgb(${c[0]},${c[1]},${c[2]})`;
}

function pathOf(polys: [number, number][][][]): string {
  let d = '';
  for (const poly of polys) for (const ring of poly) d += 'M' + ring.map((p) => `${p[0]} ${p[1]}`).join('L') + 'Z';
  return d;
}

// ---- Unit symbols (standard military map symbols, drawn in a w x h box) -------------------

function symbol(kind: Kind, w: number, h: number): string {
  const X = `<path class="sym" d="M0 0L${w} ${h}M0 ${h}L${w} 0"/>`;
  switch (kind) {
    case 'inf': return X;
    case 'mot': return X + `<circle class="symf" cx="${w * 0.3}" cy="${h + 1.7}" r="1.3"/><circle class="symf" cx="${w * 0.7}" cy="${h + 1.7}" r="1.3"/>`;
    case 'arm': return `<ellipse class="sym" cx="${w / 2}" cy="${h / 2}" rx="${w * 0.33}" ry="${h * 0.27}"/>`;
    case 'cav': return `<path class="sym" d="M0 ${h}L${w} 0"/>`;
    case 'mtn': return X + `<path class="symf" d="M${w / 2 - 3} ${h}L${w / 2} ${h - 4.5}L${w / 2 + 3} ${h}Z"/>`;
    case 'fort': return `<path class="sym" d="M${w * 0.3} ${h * 0.75}V${h * 0.3}H${w * 0.42}V${h * 0.45}H${w * 0.58}V${h * 0.3}H${w * 0.7}V${h * 0.75}Z"/>`;
    case 'art': return `<circle class="symf" cx="${w / 2}" cy="${h / 2}" r="${h * 0.24}"/>`;
    // Engineers: a bridge span on its piers.
    case 'eng': return `<path class="sym" d="M${w * 0.22} ${h * 0.72}V${h * 0.32}H${w * 0.78}V${h * 0.72}M${w / 2} ${h * 0.32}V${h * 0.6}"/>`;
  }
}

/**
 * The branch of an army's strongest corps, as a small plate on the corner of its flag. Infantry, the usual
 * case, carries none, so that the flag is seen whole.
 */
function badge(kind: Kind): string {
  const w = 9, h = 6.4;
  let s = '';
  switch (kind) {
    case 'inf': return '';
    case 'arm': s = `<ellipse class="bsym" cx="${w / 2}" cy="${h / 2}" rx="2.7" ry="1.6"/>`; break;
    case 'mot': s = `<path class="bsym" d="M1.5 1.2L${w - 1.5} ${h - 2.2}M1.5 ${h - 2.2}L${w - 1.5} 1.2"/><circle class="bsymf" cx="2.9" cy="${h - 1}" r=".7"/><circle class="bsymf" cx="${w - 2.9}" cy="${h - 1}" r=".7"/>`; break;
    case 'cav': s = `<path class="bsym" d="M1.5 ${h - 1.3}L${w - 1.5} 1.3"/>`; break;
    case 'mtn': s = `<path class="bsymf" d="M${w / 2 - 2.6} ${h - 1.2}L${w / 2} 1.2L${w / 2 + 2.6} ${h - 1.2}Z"/>`; break;
    case 'fort': s = `<path class="bsym" d="M2 ${h - 1.3}V1.7H3.6V2.9H5.4V1.7H7V${h - 1.3}Z"/>`; break;
    case 'art': s = `<circle class="bsymf" cx="${w / 2}" cy="${h / 2}" r="1.7"/>`; break;
    case 'eng': s = `<path class="bsym" d="M2 ${h - 1.4}V2H${w - 2}V${h - 1.4}M${w / 2} 2V${h - 2.2}"/>`; break;
  }
  return `<g class="badge" transform="translate(14.4 8.2)"><rect width="${w}" height="${h}" rx="1"/>${s}</g>`;
}

/** An army's standard: its nation's flag, edged, with the branch plate on its corner. */
const standard = (nat: string, kind: Kind): string => `${flagUse(nat, g.nations[nat].color)}<rect class="box" width="22" height="14" rx=".7" fill="none"/>${badge(kind)}`;

/** A nation's flag beside its name in a panel. */
const chip = (nat: string): string => flagIcon(nat, g.nations[nat]?.color ?? '#888', 20, 'chip');

/** The general an army on the map marches under: the most senior of those commanding its corps. */
function leaderOf(cs: Corps[]): General | null {
  let best: General | null = null, size = 0;
  for (const c of cs) {
    const x = generalOf(g, c);
    if (!x) continue;
    const s = c.divs * c.str;
    if (!best || x.level > best.level || (x.level === best.level && s > size)) { best = x; size = s; }
  }
  return best;
}

/** The name a general goes by on the map: his surname. */
const surname = (name: string): string => name.replace(/\s*\(.*\)\s*$/, '').trim().split(/\s+/).pop() ?? name;
/** The share of its full strength an army still has. */
const fitness = (cs: Corps[]): number => { const full = cs.reduce((n, c) => n + c.divs, 0); return full > 0 ? sumDivs(cs) / full : 0; };

function icon(c: Corps): string {
  return `<svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[c.nat].color}"/>${symbol(c.kind, 24, 15)}</svg>`;
}

// ---- Fleet symbols: a warship in profile, or a submarine for a flotilla with no surface ships

const HULL = 'M-12 .5L-10 4H10L12.5 .5H6V-2H3V-4.5H-1V-2H-6V.5Z';
/** A pair of peaks: the mark of ground too steep for tanks. */
const PEAKS = 'M-5.2 2.8L-1.6-3.6 1 .6 2.6-1.6 5.2 2.8Z';

// ---- Cities and towns on the map ----------------------------------------------------------------

/** A place as it is drawn on the map, each standing on the line y = 3.4: a cottage, a few roofs round a spire, a skyline, and a skyline round a domed hall. */
const TOWN_SHAPE: Record<TownView['size'], string> = {
  village: 'M-2.3 3.4V1L0-1.1 2.3 1V3.4Z',
  town: 'M-4.7 3.4V1L-3-.6-1.3 1V3.4ZM-1.3 3.4V-1.4L0-3.9 1.3-1.4V3.4ZM1.3 3.4V.6L3-1 4.7.6V3.4Z',
  city: 'M-6 3.4V-.4H-4.2V-2.6H-2.3V-.4H-1.5V-5L-.3-6.5.9-5V-1.6H2.6V-3.7H4.5V-.7H6V3.4Z',
  capital: 'M-7 3.4V-.3H-5.3V-2.5H-3.5V-.3H-2.9V-3A2.9 2.9 0 0 1 2.9-3V-.3H3.5V-3.5H5.3V-.3H7V3.4Z',
};
const STAR = 'M0-3.2L.9-1 3.2-1 1.4.5 2 2.8 0 1.4-2 2.8-1.4.5-3.2-1-.9-1Z';
/** A town's glyph about its own centre: gold by how rich the province is (rank 0 to 5), with a star over a capital (hollow if the government has fallen). */
function townGlyph(size: TownView['size'], rank: number, fallen = false): string {
  const star = size === 'capital' || fallen ? `<path class="twstar${fallen ? ' gone' : ''}" transform="translate(0 ${size === 'capital' ? -8.6 : -9.6}) scale(.82)" d="${STAR}"/>` : '';
  return `<g class="twn ${size} k${Math.min(5, Math.max(0, rank))}"><path d="${TOWN_SHAPE[size]}"/>${star}</g>`;
}

/** Small signs for the line under a town's name: a coin, a factory, an anchor, an oil derrick. Each is drawn about its own centre, four units wide. */
const SIGN = {
  coin: '<circle class="sgn coin" r="1.75"/>',
  factory: '<path class="sgn solid" d="M-2.2 1.7V-.5L-.7.5V-.5L.8.5V-2.2H2.2V1.7Z"/>',
  harbour: '<path class="sgn" d="M0-1.1V2M-1.2-.2H1.2M-2 .5C-1.7 1.7-.8 2.1 0 2.1S1.7 1.7 2 .5"/><circle class="sgn" cy="-1.6" r=".55"/>',
  oil: '<path class="sgn" d="M-1.6 2L0-2.2 1.6 2M-1.05.5H1.05M-.55-.8H.55"/>',
};

/**
 * The line under a province's name at close zoom: its chief town where the province is not named for it, what it is
 * worth a month, and signs for factories, harbour and oilfield. Laid out about the centre from the widths of its parts.
 */
function townFigures(t: TownView, cx: number, y: number): string {
  const parts: { w: number; draw: (x: number) => string }[] = [];
  const text = (s: string, cls: string, per: number): void => { const w = s.length * per; parts.push({ w, draw: (x) => `<text class="lbl2 ${cls}" x="${x.toFixed(1)}" y="${y}" textLength="${w.toFixed(1)}" lengthAdjust="spacingAndGlyphs">${esc(s)}</text>` }); };
  const sign = (body: string): void => { parts.push({ w: 4.4, draw: (x) => `<g class="lbl2" transform="translate(${(x + 2.2).toFixed(1)} ${y - 1.7})">${body}</g>` }); };
  if (t.name !== t.province) text(t.name, 'tn', 2.75);
  if (t.worth > 0) { sign(SIGN.coin); text(String(t.worth), 'tw', 2.9); }
  if (t.factories > 0) { sign(SIGN.factory); if (t.factories > 1) text(String(t.factories), 'tf', 2.9); }
  if (t.signs.includes('harbour')) sign(SIGN.harbour);
  if (t.signs.includes('oil')) sign(SIGN.oil);
  if (!parts.length) return '';
  const gap = 1.5;
  let x = cx - (parts.reduce((s, p) => s + p.w, 0) + gap * (parts.length - 1)) / 2;
  let out = '';
  for (const p of parts) { out += p.draw(x); x += p.w + gap; }
  return out;
}
const SUB = 'M-11 2.2Q-11 4.2-8 4.2H8Q11.5 4.2 11.5 2.2Q11.5 .2 8 .2H2.5V-3H-1.5V.2H-8Q-11 .2-11 2.2Z';

function hullIcon(f: Fleet): string {
  return `<svg width="34" height="24" viewBox="-14 -9 28 18" class="ctr"><path class="hull" d="${surfaceShips(f) ? HULL : SUB}" fill="${g.nations[f.nat].color}"/></svg>`;
}

/** The fleets on the map: one marker for each navy in each sea, and a smaller one for ships lying in harbour. */
function fleetMarks(): string {
  // In the grand campaign the player sees his own side's fleets, and anybody else's only as far as they have been found.
  const fog = carrierRules(g) && !!g.human;
  const seen = new Map<number, FleetSeen>(fog ? fleetsInSight(g).map((s) => [s.id, s]) : []);
  const groups = new Map<string, Fleet[]>();
  for (const f of g.fleets) {
    if (!shipCount(f.ships)) continue;
    const s = seen.get(f.id);
    if (fog && !s) continue;
    // Ships that are only reported are a marker of their own: a flag and a question mark.
    const q = s && s.known !== 'exact';
    const key = atSea(f) ? `s:${f.zone}:${f.nat}:${q ? 'q' : f.stance}` : f.base >= 0 ? `p:${f.base}:${f.nat}:${q ? 'q' : ''}` : `h:${f.home}:${f.nat}:${q ? 'q' : ''}`;
    (groups.get(key) ?? groups.set(key, []).get(key)!).push(f);
  }
  // While a strike on a fleet is being planned, the enemy fleets that can be struck are pieces to click.
  const aim = new Set(ui.tab === 'air' && ui.raid === 'fleet' && g.human && humanUp(g) ? raidTargetsFor(g, 'fleet', ui.lead).map((t) => t.id) : []);
  const rows = new Map<string, number>();
  for (const key of groups.keys()) { const k = key.split(':').slice(0, 2).join(':'); rows.set(k, (rows.get(k) ?? 0) + 1); }
  const used = new Map<string, number>();
  let out = '';
  for (const [key, fs] of groups) {
    const [where, idText, , tail] = key.split(':');
    const id = Number(idText), slot = `${where}:${id}`;
    const i = used.get(slot) ?? 0;
    used.set(slot, i + 1);
    const vague = tail === 'q';
    const n = fs.reduce((t, f) => t + shipCount(f.ships), 0);
    const shape = fs.some((f) => surfaceShips(f) > 0) ? HULL : SUB;
    const picked = fs.some((f) => f.id === ui.fleet) && ui.tab === 'navy';
    // The player's own fleets are pieces: a click on one picks it up.
    const own = fs[0].nat === g.human;
    const badge = vague ? '' : fs[0].stance === 'escort' ? 'E' : fs[0].stance === 'hunt' ? 'H' : '';
    const adj = g.nations[fs[0].nat].adj;
    const targets = fs.filter((f) => aim.has(f.id)).map((f) => f.id);
    const tipText = own ? `${fs.map((f) => `${f.name}: ${fleetDoing(g, f)}`).join(' · ')}. Click to pick ${fs.length > 1 ? 'one' : 'it'} up.`
      : vague ? `${adj} ships reported ${where === 's' ? `in the ${g.seas[id].name}` : 'in harbour'}: ${fs.map((f) => (seen.get(f.id)?.size ?? 'ships').toLowerCase()).join(', and ')}. Not identified: search the sea with aircraft, or bring them to battle, to learn what they are.${targets.length ? ' Click to plan a strike on them.' : ''}`
      : fog ? `${fs.map((f) => `${f.name}: ${shipList(f.ships)}`).join(' · ')}.${targets.length ? ' Click to plan a strike.' : ''}` : '';
    // Carriers show their aircraft: a small aeroplane on the marker, where the fleet is one's own or identified.
    const decks = carrierRules(g) && !vague && fs.some((f) => f.ships.cv > 0);
    let x: number, y: number, small = false;
    if (where === 'p') { const r = g.regions[id]; x = r.x + 16 + i * 12; y = r.y + 12; small = true; }
    else { const z = g.seas[id]; x = z.x + (i - (rows.get(slot)! - 1) / 2) * 30; y = z.y + (where === 'h' ? 37 : 19); small = where === 'h'; }
    out += `<g class="ctr flt${!vague && fs[0].stance === 'raid' ? ' raid' : ''}${small ? ' port' : ''}${vague ? ' vague' : ''}${targets.length ? ' aim' : ''}${tipText && where !== 'p' ? ' ftip' : ''}"${own ? ` data-fleets="${fs.map((f) => f.id).join(',')}"` : targets.length ? ` data-foe="${targets.join(',')}"` : ''} transform="translate(${x.toFixed(1)} ${y.toFixed(1)})${small ? ' scale(.68)' : ''}">${tipText ? `<title>${esc(tipText)}</title><circle class="fhit" r="15"/>` : ''}${picked ? '<circle class="fsel" r="17"/>' : ''}${targets.length ? '<circle class="faim" r="16"/>' : ''}<path class="hull" d="${shape}" fill="${g.nations[fs[0].nat].color}"/><g class="jack" transform="translate(-5 -12.4)">${flagUse(fs[0].nat, g.nations[fs[0].nat].color, 10, 6.4)}<rect width="10" height="6.4" fill="none"/></g><text x="0" y="11.5">${vague ? '?' : n}</text>${badge ? `<g class="fbadge"><circle cx="11" cy="-4.5" r="4.4"/><text x="11" y="-2.3">${badge}</text></g>` : ''}${decks ? `<g class="fbadge cvb"><circle cx="-11" cy="-4.5" r="4.4"/><path d="${PLANE}" transform="translate(-11 -4.5) scale(.36)"/></g>` : ''}</g>`;
  }
  return out;
}

// ---- Air fleets: an aircraft seen from above, beside the region it is based in ---------------

const PLANE = 'M0-8.5L1.6-3 9.5 1.2V3.2L1.6 1.6 1.1 6 3.6 8V9.2L0 8.2-3.6 9.2V8L-1.1 6-1.6 1.6-9.5 3.2V1.2L-1.6-3Z';

function planeIcon(f: AirFleet): string {
  return `<svg width="34" height="24" viewBox="-14 -10.5 28 21" class="ctr"><path class="hull" d="${PLANE}" fill="${g.nations[f.nat].color}"/></svg>`;
}

/** Air fleets at their bases, and a line from each one that is flying a mission to where it is flying it. */
function airBaseMarks(): string {
  let out = airWar(g) ? airfieldMarks() : '';
  for (const f of g.airFleets) {
    if (!isOffensive(f.mission) || f.ftr + f.bmr <= 0) continue;
    const b = g.regions[f.base];
    const t = f.mission === 'strike' ? g.seas[f.target] : g.regions[f.target];
    if (!t) continue;
    const ty = f.mission === 'strike' ? t.y + 9 : t.y;
    const c = g.nations[f.nat].color;
    out += `<path class="airline under" d="M${b.x} ${b.y}L${t.x} ${ty}"/><path class="airline${f.mission === 'night' ? ' night' : ''}" d="M${b.x} ${b.y}L${t.x} ${ty}" stroke="${c}"/><circle class="airhit" cx="${t.x}" cy="${ty}" r="3.4" fill="${c}"/>`;
  }
  // Under the fog of war an airfield is seen but not what flies from it.
  const hidden = new Set(airWar(g) && g.human ? airfieldsSeen(g).filter((a) => a.groups < 0).map((a) => a.region) : []);
  const groups = new Map<string, AirFleet[]>();
  for (const f of g.airFleets) {
    if (f.ftr + f.bmr <= 0 || hidden.has(f.base)) continue;
    const key = `${f.base}:${f.nat}`;
    (groups.get(key) ?? groups.set(key, []).get(key)!).push(f);
  }
  const used = new Map<number, number>();
  for (const [key, fs] of groups) {
    const base = Number(key.split(':')[0]);
    const r = g.regions[base];
    const i = used.get(base) ?? 0;
    used.set(base, i + 1);
    const n = fs.reduce((t, f) => t + f.ftr + f.bmr, 0);
    const picked = ui.tab === 'air' && fs.some((f) => f.id === ui.air);
    // In the air war the aircraft stand on their airfield's pad: smaller, with the count under the pad.
    const aw = airWar(g);
    out += `<g class="ctr flt air${aw ? ' onpad' : ''}" transform="translate(${(r.x - 17 - i * (aw ? 16 : 13)).toFixed(1)} ${(r.y + 11).toFixed(1)}) scale(${aw ? '.5' : '.62'})">${picked ? `<circle class="fsel" r="${aw ? 22 : 17}"/>` : ''}<path class="hull" d="${PLANE}" fill="${g.nations[fs[0].nat].color}"/><g class="jack" transform="translate(${aw ? '9 -17' : '4.5 -12'})">${flagUse(fs[0].nat, g.nations[fs[0].nat].color, 10, 6.4)}<rect width="10" height="6.4" fill="none"/></g><text x="0" y="${aw ? 24 : 17.5}">${n}</text></g>`;
  }
  return out;
}

/** The reach of the picked air fleet, and where its next order could send it. */
function airMarks(f: AirFleet): string {
  const b = g.regions[f.base];
  const R = airRange(f);
  const u = AIR.kmPerUnit;
  let out = '';
  if (ui.airMode === 'base') for (const r of airBases(g, f)) out += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
  else if (ui.airMode === 'strike') for (const z of airTargets(g, f, 'strike')) out += `<circle class="mk sea${f.mission === 'strike' && f.target === z ? ' at' : ''}" cx="${g.seas[z].x}" cy="${g.seas[z].y + 9}" r="29"/>`;
  else if (ui.airMode && ui.airMode !== 'scout' && ui.airMode !== 'search') for (const id of airTargets(g, f, ui.airMode)) out += `<path class="mk hit" d="${pathOf(g.regions[id].polys)}"/>`;
  if (f.bmr > 0) out += `<circle class="mk ring bmr" cx="${b.x}" cy="${b.y}" r="${(R.bmr / u).toFixed(1)}"/>`;
  if (f.ftr > 0) out += `<circle class="mk ring ftr" cx="${b.x}" cy="${b.y}" r="${(R.ftr / u).toFixed(1)}"/><circle class="mk ring full" cx="${b.x}" cy="${b.y}" r="${((R.ftr * AIR.range.fullAt) / u).toFixed(1)}"/>`;
  return out;
}

// ---- The page skeleton --------------------------------------------------------------------

function mount(): void {
  $('#app').innerHTML = `
    <div class="bar">
      <div class="brand">Theater Command<small>1939 to 1945 · campaign prototype</small></div>
      <div class="when" id="when"></div>
      <span id="purse"></span>
      <span class="spacer"></span>
      <span id="barbtns" style="display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end"></span>
    </div>
    <div class="main">
      <div class="mapwrap" id="mapwrap">
        <svg id="map" preserveAspectRatio="xMidYMid slice">
          <defs>${flagDefs()}<pattern id="hatch" width="5" height="5" patternUnits="userSpaceOnUse" patternTransform="rotate(45)"><rect width="1.6" height="5" fill="rgba(20,22,24,.3)"/></pattern></defs>
          <g id="seas"></g><g id="lanes"></g><g id="land"></g><g id="regs"></g><g id="occ"></g><g id="riches"></g><g id="fog"></g><g id="fx"></g><g id="marks"></g><g id="rails"></g><g id="rivers"></g><g id="towns"></g><g id="labels"></g><g id="sieges"></g><g id="bridges"></g><g id="fleets"></g><g id="units"></g><g id="orders"></g><g id="route"></g><g id="ghost"></g>
        </svg>
        <div class="hint" id="hint"></div>
        <div class="passing" id="passing"></div>
        <div class="alerts" id="alerts"></div>
        <div class="tray" id="tray"></div>
        <div class="legend"><span><i style="background:rgba(92,170,96,.5);border-color:#2f6b39"></i>Can march here</span><span><i style="background:rgba(207,90,65,.55);border-color:#a22d18"></i>Can attack</span><span id="legwar" hidden><i style="background:rgba(224,170,60,.5);border-color:#b07a12"></i>Neutral: marching in means war</span><span id="legleague" hidden><i style="background:rgba(42,157,143,.6);border-color:#1c6b61"></i>The League</span><span><i style="background:repeating-linear-gradient(45deg,#8a8a80 0 2px,#cfc9b8 2px 4px);border-color:#555"></i>Occupied</span><span id="legfree" hidden><svg width="14" height="14" viewBox="-7 -7 14 14" style="vertical-align:-3px;margin-right:4px"><circle r="5.2" fill="none" stroke="#e8c15a" stroke-width="1.6" stroke-dasharray="2.6 1.8"/></svg>Fallen capital: take it to free the country</span><span id="legdir" hidden><svg width="13" height="14" viewBox="-6.5 -5.4 13 11.6" style="vertical-align:-3px;margin-right:4px"><path d="M-5.6 -4.4H5.6V2.4L0 5.4-5.6 2.4Z" fill="#d8b25a" stroke="#15120f" stroke-width=".9"/></svg>The cabinet's directive, months left</span><span id="legvow" hidden><svg width="13" height="13" viewBox="-6.5 -6.5 13 13" style="vertical-align:-2px;margin-right:4px"><circle r="5.2" fill="#9cc0e6" stroke="#15120f" stroke-width=".9"/></svg>Your promise, months left</span><span id="legsiege" hidden><svg width="15" height="15" viewBox="-12 -12 24 24" style="vertical-align:-3px;margin-right:4px"><circle r="8.6" fill="none" stroke="#e2664d" stroke-width="3" stroke-dasharray="4.4 2.8"/><circle r="3" fill="#efe6cf"/></svg>Town under siege</span><span id="legprize" hidden><svg width="11" height="14" viewBox="-5 -6.6 10 12.2" style="vertical-align:-2px;margin-right:4px"><path d="M0 5C-3 5-4.6 3-4.2 .6-3.9-1.2-2.5-2-2.2-4-1-3.2-.6-2.3-.4-1.2.6-2.6.7-4.4-.2-6 2.6-4.8 4.6-2.2 4.4 .9 4.2 3.3 2.6 5 0 5Z" fill="#e8622c" stroke="#15120f" stroke-width=".9"/></svg>Wrecked: out of use</span><span id="legfog" hidden><i style="background:rgba(16,20,26,.62);border-color:#2a2f36"></i>Unseen ground: send an agent</span><span id="legsteep" hidden><svg width="15" height="10" viewBox="-6 -4.6 12 8" style="vertical-align:-1px;margin-right:4px"><path d="${PEAKS}" fill="none" stroke="#cfc9b8" stroke-width="1.1" stroke-linejoin="round"/></svg>Too steep for tanks</span><span id="legbridge" hidden><svg width="16" height="10" viewBox="-5.5 -3.4 11 6.8" style="vertical-align:-1px;margin-right:4px"><path d="M-3.6 -1.2H-1.1M1.1 -1.2H3.6M-3.6 1.2H-1.1M1.1 1.2H3.6M-1.5 -2.3L1.5 2.3M1.5 -2.3L-1.5 2.3" fill="none" stroke="#e2664d" stroke-width="1" stroke-linecap="round"/></svg>Bridges down</span><span id="legtrade" hidden><svg width="18" height="8" viewBox="0 0 18 8" style="vertical-align:0;margin-right:4px"><path d="M1 4H17" stroke="#15120f" stroke-width="3.2" stroke-linecap="round"/><path d="M1 4H17" stroke="#e8c15a" stroke-width="1.6" stroke-dasharray="4 2.4" stroke-linecap="round"/></svg>Your trade routes</span><span id="legrail" hidden><svg width="18" height="8" viewBox="0 0 18 8" style="vertical-align:0;margin-right:4px"><path d="M1 4H17" stroke="#15120f" stroke-width="3" stroke-linecap="round"/><path d="M1 4H17" stroke="#f1e9d4" stroke-width="1.5" stroke-dasharray="2.4 2.4"/></svg>Railway</span><span id="legrailcut" hidden><svg width="18" height="8" viewBox="0 0 18 8" style="vertical-align:0;margin-right:4px"><path d="M1 4H17" stroke="#e2664d" stroke-width="1.8" stroke-dasharray="1.4 2.6" stroke-linecap="round"/></svg>Line cut</span><span><svg width="44" height="13" viewBox="-8 -9.4 49 14" style="vertical-align:-2px;margin-right:4px">${townGlyph('capital', 4)}<g transform="translate(15 0)">${townGlyph('city', 2)}</g><g transform="translate(28 0)">${townGlyph('town', 0)}</g><g transform="translate(37 0)">${townGlyph('village', 0)}</g></svg>Capital, city, town, village: the more gold, the richer</span><span><svg width="22" height="10" viewBox="-13 -5.5 26 10.5" style="vertical-align:-1px;margin-right:4px"><path d="${HULL}" fill="#8f98a0" stroke="#15181b" stroke-width=".8"/></svg>Fleet at sea</span><span><svg width="13" height="12" viewBox="-10.5 -9.5 21 19.5" style="vertical-align:-2px;margin-right:4px"><path d="${PLANE}" fill="#8f98a0" stroke="#15181b" stroke-width=".8"/></svg><span id="legair">Air fleet</span></span><span id="legfield" hidden><svg width="19" height="14" viewBox="-9.5 -7 19 14" style="vertical-align:-3px;margin-right:4px"><rect x="-8.5" y="-6" width="17" height="12" rx="2" fill="#66733f" stroke="#e9dfc4" stroke-width=".9"/><path d="M-6.3 2.4H6.3M0 -4.3L-4.6 4M0 -4.3L4.6 4" fill="none" stroke="#f1e9d4" stroke-width="1.3" stroke-linecap="round"/></svg>Airfield: click to open</span></div>
        <div class="zoom"><button class="btn" id="lensbtn" data-act="lens" aria-label="Show what every province is worth" title="Show what every province is worth: the richer the ground, the more gold on it."><svg width="18" height="18" viewBox="-9 -9 18 18"><circle r="6.6" fill="none" stroke="currentColor" stroke-width="1.6"/><path d="M0 -3.9V3.9M-2.3 -1.6Q-2.3 -3 0 -3T2.3 -1.6 0 0 -2.3 1.6 0 3 2.3 1.6" fill="none" stroke="currentColor" stroke-width="1.3" stroke-linecap="round"/></svg></button><button class="btn" data-act="zin" aria-label="Zoom in">+</button><button class="btn" data-act="zout" aria-label="Zoom out">−</button></div>
        <div class="tip" id="tip"></div>
      </div>
      <div class="side">
        <div class="tabs" id="tabs"></div>
        <div class="pane" id="pane"></div>
      </div>
    </div>
    <div class="veil" id="veil"><div class="card" id="card"></div></div>`;
  // A tap on the map can put a panel under the finger, and the browser's follow-up click would then press whatever
  // appeared there. For a moment after such a tap, a click that lands off the map is swallowed.
  document.addEventListener('click', (e) => {
    if (performance.now() < ghostUntil && !(e.target as Element | null)?.closest?.('#map')) { e.stopPropagation(); e.preventDefault(); }
  }, true);
  document.addEventListener('click', onClick);
  document.addEventListener('keydown', onKey);
  wireMap();
  window.addEventListener('resize', applyView);
  window.addEventListener('resize', drawTechLines);
}

function drawStatic(): void {
  $('#seas').innerHTML = SEA_BORDERS.map((b) => `<path class="seaborder" d="M${b.map((p) => `${p[0]} ${p[1]}`).join('L')}"/>`).join('')
    + g.seas.map((z) => {
      // Long names, and names in tight water, go on two lines.
      const cut = z.narrow || z.name.length > 12 ? z.name.lastIndexOf(' ') : -1;
      const lines = cut > 0 ? [z.name.slice(0, cut), z.name.slice(cut + 1)] : [z.name];
      const step = z.narrow ? 9 : 11;
      return `<text class="sealbl${z.narrow ? ' narrow' : ''}" y="${z.y - (lines.length - 1) * step / 2}">${lines.map((t, i) => `<tspan x="${z.x}" dy="${i ? step : 0}">${esc(t)}</tspan>`).join('')}</text>`;
    }).join('');
  $('#land').innerHTML = `<path class="land" d="${pathOf(g.land)}"/>`;
  $('#regs').innerHTML = g.regions.map((r) => `<path class="reg" data-id="${r.id}" d="${pathOf(r.polys)}"/>`).join('');
  // The great rivers, each a smooth line with its name set along its middle reach.
  $('#rivers').innerHTML = RIVERS.map((rv) => {
    const p = rv.pts;
    let d = `M${p[0][0]} ${p[0][1]}`;
    for (let i = 0; i < p.length - 1; i++) {
      const a = p[i - 1] ?? p[i], b = p[i], c = p[i + 1], e = p[i + 2] ?? c;
      d += `C${(b[0] + (c[0] - a[0]) / 6).toFixed(1)} ${(b[1] + (c[1] - a[1]) / 6).toFixed(1)} ${(c[0] - (e[0] - b[0]) / 6).toFixed(1)} ${(c[1] - (e[1] - b[1]) / 6).toFixed(1)} ${c[0]} ${c[1]}`;
    }
    // The name goes on the longest reach, turned to run along it and never upside down.
    let k = 0, best = 0;
    for (let i = 0; i < p.length - 1; i++) { const len = Math.hypot(p[i + 1][0] - p[i][0], p[i + 1][1] - p[i][1]); if (len > best) { best = len; k = i; } }
    let ang = (Math.atan2(p[k + 1][1] - p[k][1], p[k + 1][0] - p[k][0]) * 180) / Math.PI;
    if (ang > 90) ang -= 180; else if (ang < -90) ang += 180;
    const mx = (p[k][0] + p[k + 1][0]) / 2, my = (p[k][1] + p[k + 1][1]) / 2;
    return `<path class="rivercase" d="${d}"/><path class="river" d="${d}"/>${best > 26 ? `<text class="rivlbl" transform="translate(${mx.toFixed(1)} ${my.toFixed(1)}) rotate(${ang.toFixed(0)})" y="-2.2">${esc(rv.name)}</text>` : ''}`;
  }).join('');
}

/**
 * The railways (the grand campaign). Each line runs from one province's centre to the next by the point where it
 * crosses the border, so each province owns its half: a province whose line is cut has its halves in broken red.
 */
function railMarks(): string {
  if (!groundRules(g)) return '';
  let ok = '', cut = '';
  for (const l of RAIL_LINKS) {
    const a = g.regions[l.a], b = g.regions[l.b];
    const ha = `M${a.x} ${a.y}L${l.x} ${l.y}`, hb = `M${l.x} ${l.y}L${b.x} ${b.y}`;
    if (railCut(g, l.a)) cut += ha; else ok += ha;
    if (railCut(g, l.b)) cut += hb; else ok += hb;
  }
  return `<path class="railbed" d="${ok}"/><path class="railtie" d="${ok}"/>${cut ? `<path class="railcutbed" d="${cut}"/><path class="railcut" d="${cut}"/>` : ''}`;
}

/** The bridges over the rivers: a span where they stand, a broken one in red where they are down (the grand campaign). */
function bridgeMarks(): string {
  if (!groundRules(g)) return '';
  return CROSSINGS.map((x) => {
    const a = g.regions[x.a], b = g.regions[x.b];
    const ang = (Math.atan2(b.y - a.y, b.x - a.x) * 180) / Math.PI;
    const down = bridgeDown(g, x);
    return `<g class="bridge${down ? ' down' : ''}" transform="translate(${x.x} ${x.y}) rotate(${ang.toFixed(0)})">${down
      ? '<path class="deck" d="M-3.6 -1.2H-1.1M1.1 -1.2H3.6M-3.6 1.2H-1.1M1.1 1.2H3.6"/><path class="wing" d="M-3.6 -1.2l-1.1 -1.1M-3.6 1.2l-1.1 1.1M3.6 -1.2l1.1 -1.1M3.6 1.2l1.1 1.1"/><path class="gap" d="M-1.5 -2.3L1.5 2.3M1.5 -2.3L-1.5 2.3"/>'
      : '<rect class="road" x="-3.6" y="-1.2" width="7.2" height="2.4"/><path class="deck" d="M-3.6 -1.2H3.6M-3.6 1.2H3.6"/><path class="wing" d="M-3.6 -1.2l-1.1 -1.1M-3.6 1.2l-1.1 1.1M3.6 -1.2l1.1 -1.1M3.6 1.2l1.1 1.1"/>'}</g>`;
  }).join('');
}

function drawLabels(): void {
  // Cities and towns: each stands at its own fixed place in its province, drawn by its size and gilded by its worth.
  const list = towns(g);
  $('#towns').innerHTML = list.map((t) => `<g transform="translate(${t.x} ${t.y})${t.size === 'capital' || t.size === 'city' ? ' scale(1.2)' : ''}">${townGlyph(t.size, t.rank, t.size !== 'capital' && !!g.regions[t.region].cap)}</g>`).join('');
  $('#labels').innerHTML = g.regions.map((r) => {
    const busy = r.units.length > 0;
    const t = list[r.id];
    const star = '';
    const fort = r.fort ? `<path class="fortmark" transform="translate(${r.x + (busy ? 17 : 0)} ${r.y + (busy ? 3 : -7)})" d="M-3 2.5V-2.5H-1.2V-.8H1.2V-2.5H3V2.5"/>` : '';
    // The steepest ground carries a pair of peaks: no tanks go there.
    const high = steep(g, r) ? `<path class="steepmark" transform="translate(${r.x + (busy ? 17 : r.fort ? 9 : 0)} ${r.y + (busy ? (r.fort ? -6 : 3) : -7.5)})" d="${PEAKS}"/>` : '';
    const ly = r.y + (busy ? 26.5 : 3);
    return `${star}${fort}${high}<text class="lbl${r.cap ? ' cap' : t.size === 'city' ? ' big' : ''}" x="${r.x}" y="${ly}">${esc(r.name)}</text><g class="figs ${t.size}">${townFigures(t, r.x, ly + 6.3)}</g>`;
  }).join('');
}

// ---- Drawing the changing parts -----------------------------------------------------------

function refresh(): void {
  ensureSupply(g);
  const me = g.human;
  const myTurn = humanUp(g);
  // Region colours: who holds it.
  const regs = $('#regs').children;
  let occ = '';
  for (const r of g.regions) {
    const n = g.nations[r.ctrl];
    // The League shows in one colour of its own, as the third alliance it is.
    (regs[r.id] as SVGPathElement).setAttribute('fill', mix(n.league ? LEAGUE.color : n.color, '#e9e2cf', n.atWar.length ? 0.4 : n.league ? 0.5 : 0.63));
    if (r.ctrl !== r.owner) occ += `<path class="occ" d="${pathOf(r.polys)}"/>`;
  }
  $('#occ').innerHTML = occ;
  // The worth lens: every province tinted by what it pays, the poor ground dimmed and the rich in gold.
  $('#riches').innerHTML = ui.lens === 'worth' ? towns(g).map((t) => `<path class="rich k${t.rank}" d="${pathOf(g.regions[t.region].polys)}"/>`).join('') + tradeMarks() : '';
  ($('#legtrade') as HTMLElement).hidden = !(ui.lens === 'worth' && tradeRules(g) && g.trade.length > 0);
  $('#map').classList.toggle('lensworth', ui.lens === 'worth');
  $('#lensbtn').classList.toggle('on', ui.lens === 'worth');
  // The fog of war: ground the player's side cannot see into is veiled, lightly where an army shows as a flag and a count.
  seen = sight(g);
  const foggy = fogOn(g);
  let veil = '';
  if (foggy) for (const r of g.regions) if (seen[r.id] < 2) veil += `<path class="fog ${seen[r.id] ? 'dim' : 'dark'}" d="${pathOf(r.polys)}"/>`;
  $('#fog').innerHTML = veil;
  ($('#legfog') as HTMLElement).hidden = !foggy;
  // Where an air force has won the sky, the region is ringed in its colour.
  let skies = '';
  for (const k of Object.keys(g.sky)) { const who = skyOver(g, +k); if (who) skies += `<path class="skywon" d="${pathOf(g.regions[+k].polys)}" stroke="${g.nations[who].color}"/>`; }
  // The capital of a fallen country the player could free by taking it is ringed in gold.
  let freeable = '';
  if (me && isOpen(g)) for (const f of nationsFallen(g)) if (f.yours && f.capital >= 0 && !allied(g, me, f.holder)) freeable += `<circle class="freeme" cx="${g.regions[f.capital].x}" cy="${g.regions[f.capital].y}" r="15"/>`;
  ($('#legfree') as HTMLElement).hidden = !freeable;
  // Sieges (the grand campaign): the country round a besieged town in the besieger's colour, the town ringed, a shield on a garrison.
  const sg = siegeMarks();
  $('#fx').innerHTML = ui.flash.map((id) => `<path class="flash" d="${pathOf(g.regions[id].polys)}"/>`).join('') + skies + freeable + sg.under;
  $('#sieges').innerHTML = sg.over + placeMarks();
  // The cabinet's directive is drawn over the armies, so a flag beside the town cannot hide it.
  $('#orders').innerHTML = directiveMark();

  // Where the picked corps can go.
  let marks = '';
  const picked = pickedCorps();
  reaches = [];
  $('#route').innerHTML = '';
  routeShown = -1;
  trayArmy = armyShown();
  lands = picked.length && (g.regions[picked[0].reg].port || byAir(picked)) && g.nations[picked[0].nat].atWar.length ? landTargets(g, picked) : [];
  if (!lands.length) ui.land = false;
  const fl = ui.tab === 'navy' ? myFleet() : null;
  if (!fl) ui.rebase = false;
  // The coasts the picked fleet's guns can reach: red on the map, and a click on one plans a bombardment.
  shellAt = fl && seaWar(g) && g.nations[fl.nat].atWar.length ? fleetOptions(g, fl.id)?.coasts ?? [] : [];
  const af = ui.tab === 'air' ? myAir() : null;
  if (!af) ui.airMode = '';
  const raiding = ui.tab === 'air' && airWar(g) && !!g.human && humanUp(g);
  if (!raiding) { ui.raid = ''; ui.lead = -1; }
  if (raiding && (af || ui.raid)) marks += airWarMarks();
  else if (af) marks += airMarks(af);
  else if (fl) {
    if (ui.rebase) for (const r of baseTargets(g, fl)) marks += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
    else {
      for (const z of fleetReach(g, fl)) marks += `<circle class="mk sea${atSea(fl) && fl.zone === z ? ' at' : ''}" cx="${g.seas[z].x}" cy="${g.seas[z].y + 9}" r="29"/>`;
      for (const id of shellAt) marks += `<path class="mk hit" d="${pathOf(g.regions[id].polys)}"/>`;
    }
  } else if (ui.rail && picked.length) {
    for (const r of railTargets(picked[0])) marks += `<path class="mk rail" d="${pathOf(r.polys)}"/>`;
  } else if (ui.land && picked.length) {
    for (const r of lands) marks += `<path class="mk hit" d="${pathOf(r.polys)}"/>`;
  } else if (ui.ship && picked.length) {
    const ports = new Set<number>();
    for (const c of picked) for (const r of shipTargets(g, c)) ports.add(r.id);
    for (const id of ports) marks += `<path class="mk rail" d="${pathOf(g.regions[id].polys)}"/>`;
  } else {
    const goes = new Map<number, boolean>();
    for (const c of picked) { const m = reach(g, c); reaches.push({ from: c.reg, ap: c.ap, map: m }); for (const [id, rc] of m) if (id !== c.reg) goes.set(id, (goes.get(id) ?? false) || rc.attack); }
    for (const [id, attack] of goes) marks += `<path class="mk ${attack ? 'hit' : 'move'}" d="${pathOf(g.regions[id].polys)}"/>`;
    // Neutral ground next to the picked armies: marching in means war, and the game asks first.
    for (const id of invasions(g, picked).keys()) if (!goes.has(id)) marks += `<path class="mk war" d="${pathOf(g.regions[id].polys)}"/>`;
    // Ground the picked tanks cannot enter, where they stand next to it or to somewhere they can reach.
    if (groundRules(g)) {
      const near = new Set<number>([...goes.keys(), ...picked.map((c) => c.reg)]);
      for (const r of g.regions) if (picked.some((c) => barred(g, c, r) || (g.regions[c.reg].nb.includes(r.id) && noCrossing(g, c.kind, c.reg, r.id))) && r.nb.some((j) => near.has(j))) marks += `<path class="mk nogo" d="${pathOf(r.polys)}"/>`;
    }
  }
  if (ui.sel >= 0) marks += `<path class="mk sel" d="${pathOf(g.regions[ui.sel].polys)}"/>`;
  else if (ui.sea >= 0 && ui.tab === 'region') marks += `<circle class="mk seasel" cx="${g.seas[ui.sea].x}" cy="${g.seas[ui.sea].y + 9}" r="33"/>`;
  $('#marks').innerHTML = marks;
  ($('#legwar') as HTMLElement).hidden = !marks.includes('mk war');
  ($('#legleague') as HTMLElement).hidden = !leagueMembers(g).length;
  $('#rails').innerHTML = railMarks();
  $('#bridges').innerHTML = bridgeMarks();
  $('#lanes').innerHTML = laneMarks();
  $('#fleets').innerHTML = airBaseMarks() + fleetMarks();

  // Armies. Each nation's force in a region is one piece on the map: its colours on a standard with the branch
  // of its strongest corps, its commander's name over it, and under it a bar for the strength it has left.
  let units = '';
  for (const r of g.regions) {
    if (!r.units.length) continue;
    // An army the player cannot see is not on the map at all, and one only glimpsed is a flag and a count of corps.
    const s = seen[r.id];
    const by = new Map<string, Corps[]>();
    // A corps that marches with an ally's army stands under that army's flag.
    for (const c of r.units) if (s > 0 || friendOf(c.nat)) (by.get(armyOf(c)) ?? by.set(armyOf(c), []).get(armyOf(c))!).push(c);
    if (!by.size) continue;
    const inSight = [...by.values()].reduce((n, cs) => n + cs.length, 0);
    const groups = [...by.entries()].sort((a, b) => sumDivs(a[1]) - sumDivs(b[1]));
    groups.forEach(([nat, cs], i) => {
      const k = groups.length - 1 - i;
      if (!inFull(r, nat)) {
        units += `<g class="ctr army dimly" data-reg="${r.id}" data-nat="${nat}" transform="translate(${r.x - 11 + k * 4.5} ${r.y - 7 - k * 4})"><title>${esc(g.nations[nat].adj)} troops, ${cs.length} corps. Their kinds and strength are not known.</title>
        <ellipse class="base" cx="11" cy="16.2" rx="13" ry="3.4" fill="${g.nations[nat].color}"/><path class="pole" d="M-.6 -1.5V16.2"/>
        ${flagUse(nat, g.nations[nat].color)}<rect class="box" width="22" height="14" rx=".7" fill="none"/>
        ${k === 0 ? `<text x="11" y="24.6">?</text><circle class="more" cx="22" cy="0" r="4"/><text class="moret" x="22" y="2">${inSight}</text>` : ''}</g>`;
        return;
      }
      if (!friendOf(nat) && !allKnown(cs)) {
        // In view but not identified, or known only from old reports: the flag, the count, and the staff's estimate.
        const idd = cs.filter((c) => kn(c) !== 'none');
        const top = idd.length ? [...idd].sort((a, b) => KIND[b.kind].atk * b.divs - KIND[a.kind].atk * a.divs)[0] : null;
        const e = armyAt(g, r.id, nat)?.est ?? null;
        const picked = r.id === ui.sel && trayArmy?.reg === r.id && trayArmy.nat === nat;
        units += `<g class="ctr army unsure${picked ? ' chosen' : ''}" data-reg="${r.id}" data-nat="${nat}" transform="translate(${r.x - 11 + k * 4.5} ${r.y - 7 - k * 4})"><title>${esc(g.nations[nat].adj)} army, ${cs.length} corps, ${idd.length ? `${idd.length} identified` : 'none identified'}: ${estWords(e)}. ${HOW_TO_KNOW}</title>
        <ellipse class="base" cx="11" cy="16.2" rx="13" ry="3.4" fill="${g.nations[nat].color}"/><path class="pole" d="M-.6 -1.5V16.2"/>
        ${worksMark(cs)}${top ? standard(nat, top.kind) : `${flagUse(nat, g.nations[nat].color)}<rect class="box" width="22" height="14" rx=".7" fill="none"/>`}
        ${k === 0 ? `<text x="11" y="24.6">${e ? `${Math.max(1, Math.round((e.lo + e.hi) / 2))}?` : '?'}</text><text class="size" x="11" y="-1.6">${cs.length > 1 ? 'XXXX' : 'XXX'}</text>` : ''}
        ${k === 0 && inSight > 1 ? `<circle class="more" cx="22" cy="0" r="4"/><text class="moret" x="22" y="2">${inSight}</text>` : ''}</g>`;
        return;
      }
      const lead = [...cs].sort((a, b) => KIND[b.kind].atk * b.divs - KIND[a.kind].atk * a.divs)[0];
      const ready = myTurn && nat === me && cs.some((c) => c.ap > 0 && c.kind !== 'fort');
      const spent = myTurn && nat === me && !ready;
      const chosen = r.id === ui.sel && (cs.some((c) => ui.picked.has(c.id)) || (trayArmy?.reg === r.id && trayArmy.nat === nat));
      const who = leaderOf(cs);
      const fit = fitness(cs);
      const x = r.x - 11 + k * 4.5, y = r.y - 7 - k * 4;
      // A joint army flies its allies' colours in the corner of its standard.
      const guests = [...new Set(cs.filter((c) => c.nat !== nat).map((c) => c.nat))].slice(0, 2);
      const pennants = guests.map((o, i) => `<g class="pennant" transform="translate(${13.4 - i * 8.4} 8.2) scale(.4)">${flagUse(o, g.nations[o].color)}<rect class="box" width="22" height="14" rx=".7" fill="none"/></g>`).join('');
      units += `<g class="ctr army${ready ? ' ready' : ''}${spent ? ' spent' : ''}${chosen ? ' chosen' : ''}${who ? ' led' : ''}" data-reg="${r.id}" data-nat="${nat}" transform="translate(${x} ${y})"${marchHide && marchHide.reg === r.id && marchHide.nat === nat ? ' style="visibility:hidden"' : ''}>
        <ellipse class="base" cx="11" cy="16.2" rx="13" ry="3.4" fill="${g.nations[nat].color}"/><path class="pole" d="M-.6 -1.5V16.2"/>
        ${worksMark(cs)}${standard(nat, lead.kind)}${pennants}
        ${k === 0 ? `<rect class="strbg" x="0" y="15.2" width="22" height="2.2" rx=".6"/><rect class="str${fit < 0.5 ? ' low' : fit < 0.75 ? ' mid' : ''}" x="0" y="15.2" width="${(22 * Math.min(1, fit)).toFixed(1)}" height="2.2" rx=".6"/>
        <text x="11" y="24.6">${num(sumDivs(cs))}</text><text class="size" x="11" y="-1.6">${cs.length > 1 ? 'XXXX' : 'XXX'}</text>${who ? `<text class="cmdr" x="11" y="-2">${esc(surname(who.name))}</text>` : ''}` : ''}
        ${k === 0 && inSight > 1 ? `<circle class="more" cx="22" cy="0" r="4"/><text class="moret" x="22" y="2">${inSight}</text>` : ''}</g>`;
    });
  }
  // Fieldworks standing empty on ground the player's side holds: a small dotted bank, under whatever stands there.
  let left = '';
  if (groundRules(g)) for (const r of g.regions) {
    const v = fieldworksLeft(g, r.id);
    if (v) left += `<g class="leftworks" transform="translate(${r.x - 11} ${r.y - 7})"><path class="u" d="M-2.8 17A13.8 4.2 0 0 0 24.8 17"/><path d="M-2.8 17A13.8 4.2 0 0 0 24.8 17"/><title>Fieldworks standing empty: room for ${v.corps === 1 ? 'one corps' : `${v.corps} corps`}, ${Math.round(v.months * 10) / 10} months of work. A corps of yours that marches in takes them over.</title></g>`;
  }
  $('#units').innerHTML = left + units + agentMarks();
  drawLabels();

  drawBar();
  drawTabs();
  drawPane();
  drawTray();
  ($('#legsteep') as HTMLElement).hidden = !groundRules(g);
  ($('#legfield') as HTMLElement).hidden = !airWar(g);
  $('#legair').textContent = airWar(g) ? 'Aircraft based here' : 'Air fleet';
  ($('#legbridge') as HTMLElement).hidden = !groundRules(g) || !CROSSINGS.some((x) => bridgeDown(g, x));
  ($('#legrail') as HTMLElement).hidden = !groundRules(g);
  ($('#legrailcut') as HTMLElement).hidden = !groundRules(g) || !g.regions.some((r) => railCut(g, r.id));
  $('#hint').textContent = hintText();
  $('#alerts').innerHTML = alertButtons();
  const pass = $('#passing');
  if (passing) {
    const n = passing.nat ? g.nations[passing.nat] : null;
    pass.innerHTML = `<div class="prow">${n ? `${chip(passing.nat)}<b>${esc(natName(passing.nat))}</b>` : '<b>The month goes on</b>'}<span class="grow">${esc(passing.note || 'The other nations give their orders.')}</span><button class="btn small" data-act="skip">Skip ▸▸</button></div><div class="pbar"><i style="width:${Math.round(100 * Math.min(1, passing.done / Math.max(1, passing.total)))}%"></i></div>`;
    pass.classList.add('open');
  } else { pass.classList.remove('open'); pass.innerHTML = ''; }
  queueSave();
}

// ---- Saved games: the campaign is kept in this browser so it can be picked up later -------

const SAVE_KEY = 'theater-command-save-v1';
let saveTimer = 0;

function queueSave(): void {
  if (!ui.live || saveTimer) return;
  saveTimer = window.setTimeout(() => {
    saveTimer = 0;
    try { localStorage.setItem(SAVE_KEY, saveGame(g)); } catch { /* storage is off or full: the game still plays */ }
  }, 500);
}

function readSave(): string | null {
  try { return localStorage.getItem(SAVE_KEY); } catch { return null; }
}

const sumDivs = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
/** Divisions as the cards of a face-off count them: every division on the roll, whatever its strength, as the corps cards beneath do. */
const rollDivs = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs, 0);

function pickedCorps(): Corps[] {
  if (!humanUp(g)) return [];
  return g.corps.filter((c) => ui.picked.has(c.id) && canOrder(g, c) && c.ap > 0);
}

/** What an air mission does, with what the grand campaign adds to bombing. */
function missionWhat(m: keyof typeof MISSION): string {
  const more = groundRules(g) && (m === 'bomb' || m === 'night');
  return MISSION[m].what + (more ? ' The bombs fall on the rail yards too and can cut the railway through the province, and a heavy raid can wreck a factory outright. A province with a railway and no industry can be bombed as well.' : '');
}

function railTargets(c: Corps): Region[] {
  return [...railReach(g, c)].map((id) => g.regions[id]);
}

function hintText(): string {
  if (!g.human) return '';
  if (!humanUp(g)) return '';
  if (ui.msg) return ui.msg;
  if (onEve(g)) return `The eve of war. ${natName(eveAttacker(g) ?? 'GER')} strikes before your first month comes round: place your army now, by road or by rail, give each army its orders for the day it is attacked, then finish. No month passes.`;
  if (ui.tab === 'build') return budgeted(g) ? 'The arsenal: pick Army, Air, Navy, Works or Towns, and click one of your own provinces on the map to raise, station or build there.' : 'Planning production: click one of your own home regions to raise troops or build works there.';
  if (ui.tab === 'research') return isOpen(g) ? 'Research: open the technology tree and put each free team to work. A technology takes months, and what it gives lasts for the rest of the war.' : g.human && g.nations[g.human].lab ? 'Research: use − and + to move design teams between branches. The new pace starts next month.' : '';
  if (ui.tab === 'spies') {
    const a = ui.spySend ? secretService(g)?.agents.find((x) => x.id === ui.spy) : null;
    return a ? `Click any province on the map to send agent ${a.name} there. The journey takes a month.` : 'The secret service: recruit agents, send them abroad and give each its work. An agent is at work a month after it sets out.';
  }
  if (ui.tab === 'navy') {
    const f = myFleet();
    if (f && ui.rebase) return `New base for the ${f.name}: click any blue port. The fleet spends the month in harbour there.`;
    return f ? `Click a ringed sea to send the ${f.name} there.` : 'Pick one of your fleets in the list to give it orders.';
  }
  if (ui.tab === 'air' && airWar(g) && g.human) {
    const f = myAir();
    if (ui.raid && ui.raid !== 'menu') return ui.raid === 'strike' ? 'Shipping strike: click a ringed sea to plan it.' : ui.raid === 'fleet' ? 'Strike on a fleet: click a ringed enemy fleet, or its sea, to plan it. Only fleets that have been found can be struck.' : `${raidMenu(g).find((k) => k.key === ui.raid)?.label ?? 'Raid'}: click a red province to plan it. You see the odds before you commit.`;
    if (f && ui.airMode === 'search') return `A search by the ${f.name}: click a ringed sea. Enemy ships there may be reported, or identified.`;
    if (f && f.ship !== undefined && !ui.airMode) return `The ${f.name} flies from its carriers: the rings show its own sea and the seas it can reach next door. Plan a strike with it, or send it to search a sea.`;
    if (f && ui.airMode === 'base') return `A new airfield for the ${f.name}: click a blue province. Aircraft can be based only where there is an airfield.`;
    if (f && ui.airMode === 'scout') return `Reconnaissance by the ${f.name}: click a marked province to photograph it.`;
    if (f) return `The rings show how far the ${f.name} reaches. Give it standing orders, plan a raid with it, or move it.`;
    return 'Click an airfield on the map, or one of your air groups in the list. Plan a raid from its target.';
  }
  if (ui.tab === 'air') {
    const f = myAir();
    if (!f) return 'Pick one of your air fleets in the list to give it orders.';
    if (ui.airMode === 'scout' || ui.airMode === 'search') return '';
    if (ui.airMode === 'base') return `New airfields for ${f.name}: click any blue region.`;
    if (ui.airMode === 'strike') return `Click a ringed sea to send ${f.name} against the shipping there.`;
    if (ui.airMode) return `${MISSION[ui.airMode].label}: click a red region to see the odds.`;
    return `The rings show how far ${f.name} reaches: fighters inside the gold ring, bombers inside the pale one.`;
  }
  if (ui.land) return byAir(pickedCorps()) ? 'Airborne assault: click a red coast to see the odds.' : 'Landing from the sea: click a red coast to see the odds.';
  if (ui.rail) return 'Rail move: click any blue region. It takes the whole month.';
  if (ui.ship) return 'Sea transport: click any blue port. It takes the whole month.';
  if (pickedCorps().length) return invasions(g, pickedCorps()).size
    ? 'Right-click a green region to march there, or a red one to plan an attack. Amber ground is a country at peace with you: send an army there and you will be asked whether you mean war.'
    : 'Right-click a green region to march there, or a red one to plan an attack. On a touch screen, tap it, or press and hold.';
  const ready = g.corps.some((c) => c.nat === g.human && c.ap > 0 && c.kind !== 'fort');
  return ready ? 'Click one of your armies (gold outline) to pick it up. End the month when you are done.' : 'All of your armies have used their month. End the month to continue.';
}

/** The treasury, always in sight on the top bar: what is in hand and what the month will add or take. Click for the ledger. */
function purse(me: string): string {
  const L = ledger(g, me);
  const red = L.treasury < 0;
  const tip = L.budget
    ? `Income ${cash(L.gross)} a month, upkeep ${cash(L.upkeepTotal)}. ${red ? 'The treasury is in debt: no replacements, no new orders, and the armies suffer for it. ' : ''}Click to open the ledger.`
    : `Income ${cash(L.gross)} a month. Click to open the ledger.`;
  return `<button class="purse${red ? ' debt' : ''}" data-act="ledger" title="${esc(tip)}"><span class="pl">Treasury</span><b>${inHand(L.treasury)}</b><i class="${L.net >= 0 ? 'up' : 'down'}">${cash(L.net, true)} a month</i></button>`;
}

function drawBar(): void {
  const me = g.human;
  const w = weather(g);
  const eve = onEve(g);
  const who = passing ? 'The other nations are moving' : me ? (g.nations[me].alive ? (eve ? `The eve of war: place the army of ${natName(me)}` : `Your orders: ${natName(me)}${isOpen(g) ? (lastAct(g)?.winner === me ? ' · the world is yours' : lastAct(g)?.on ? ` · World Domination, ${lastAct(g)!.left} left` : ' · free play') : ''}`) : `${natName(me)} has fallen`) : isOpen(g) ? 'Watching a free-play war: every nation decides for itself' : 'Watching history unfold: every nation on its historical plan';
  const when = eve ? '31 August 1939' : me || g.turn === 1 ? (g.turn === 1 && !me ? '1 September 1939' : date(g.turn)) : `End of ${date(g.turn - 1)}`;
  // The level of a grand campaign is always in sight beside the date: a click shows what it does and lets it be changed.
  const rank = me && isOpen(g) ? `<button class="rank" data-act="level" title="The level of this campaign. Click to see what it does, to change it, or to switch the war cabinet ${cabinetIsOff(g) ? 'on' : 'off'}.">${pips(levelIndex(g))}${esc(levelOf(g).name)}</button>` : '';
  $('#when').innerHTML = `${when}${rank}<small>${esc(who)}${me ? ` · ${esc(w.text)}` : ''}</small>`;
  $('#purse').innerHTML = me && g.nations[me].alive && !eve ? purse(me) + gauges(me) + menGauge(me) : '';
  let b = '';
  if (passing) b += `<button class="btn go" data-act="skip" title="Play the rest of the month at once">Skip ▸▸</button>`;
  else if (me && humanUp(g)) {
    // A small country chooses its own path: the card is one click away, and flagged while it stands alone under attack.
    if (!eve && paths(g).length) { const alone = !bloc(g.nations[me]) && g.nations[me].atWar.length > 0; b += `<button class="btn${alone ? ' flag' : ''}" data-act="path" title="${alone ? 'You are at war and stand alone. See who would take you in.' : 'Join the Axis, join the Allies, stand alone, or found a League of your own.'}">Your path${alone ? '<i class="dotn"></i>' : ''}</button>`; }
    if (!eve && isOpen(g) && !cabinetIsOff(g)) { const fl = cabinetFlag(); b += `<button class="btn${fl ? ' flag' : ''}" data-act="cabinet" title="${esc(fl || 'Your government: the war cabinet and its trust in you, your ministers, your decrees and the directive.')}">Cabinet${fl ? '<i class="dotn"></i>' : ''}</button>`; }
    if (!eve && isOpen(g) && hasTraits(me)) b += `<button class="btn wideonly" data-act="traits" title="What ${esc(natName(me))} does best: its abilities and its special formation.">Abilities</button>`;
    if (declarable(g).length) b += `<button class="btn warn" data-act="declare">Declare war</button>`;
    b += `<button class="btn go" data-act="end">${eve ? 'Finish deployment ▸' : 'End month ▸'}</button>`;
  } else {
    b += `<button class="btn" data-act="auto">${ui.auto ? 'Pause' : 'Play ▸▸'}</button><button class="btn go" data-act="next">Next month ▸</button>`;
  }
  b += `<button class="btn" data-act="new">New game</button>`;
  $('#barbtns').innerHTML = b;
}

function drawTabs(): void {
  const newB = g.reports.length - ui.seenReports + Math.max(0, g.seaReports.length - ui.seenSea) + Math.max(0, g.airReports.length - ui.seenAir);
  const t = (id: string, label: string, n = 0) => `<button class="tab${ui.tab === id ? ' on' : ''}" data-act="tab" data-tab="${id}">${label}${n ? `<span class="n">${n}</span>` : ''}</button>`;
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const build = me ? t('build', 'Build') : '';
  // Generals with no corps, when there is a corps for them: a decision waiting for the player.
  const idle = me && vacant(g, me).length ? waiting(g, me).length : 0;
  $('#tabs').innerHTML = t('region', 'Region') + build + t('research', 'Research', me && isOpen(g) ? idleTeams(g, me) : 0) + t('generals', 'Generals', idle) + t('honours', 'Honours', unseen(g).length) + t('navy', 'Navy') + t('air', 'Air') + (me && isOpen(g) ? t('spies', 'Spies', spyNews()) : '') + t('reel', 'News', ui.unread) + t('battles', 'Battles', newB) + t('powers', 'Powers');
}

/** Free play: nobody hands the player a plan. The note says so, and gives his own staff's reading of each front. */
function freeAdvice(me: string): string {
  const word = (r: number): string => (r >= 1.6 ? 'strongly in your favour' : r >= 1.15 ? 'in your favour' : r >= 0.85 ? 'about even' : r >= 0.55 ? 'against you' : 'heavily against you');
  const list = fronts(g, me).sort((a, b) => b.ratio - a.ratio).slice(0, 4);
  const whoOf = (f: Front): string => f.enemies.slice(0, 3).map(natName).join(', ') + (f.enemies.length > 3 ? ' and others' : '');
  // Two fronts against the same enemy are told apart by the place each faces.
  const facing = (f: Front): string => { const r = [...f.edge].map((i) => g.regions[i]).sort((a, b) => b.vp - a.vp || b.fac - a.fac || a.id - b.id)[0]; return r ? `, on the front facing ${townName(r)}` : ''; };
  const rows = list.map((f) => {
    const who = whoOf(f), twice = list.filter((x) => whoOf(x) === who).length > 1;
    return `<li>Against ${esc(who)}${twice ? esc(facing(f)) : ''}: the odds for an offensive are ${word(f.ratio)}, ${f.ratio >= 10 ? 'better than 10 to 1' : `about ${f.ratio.toFixed(1)} to 1`}.</li>`;
  });
  const body = rows.length
    ? `Your staff's reading of the fronts, counting your attack against their defence near the line with forts, terrain and season weighed in:<ul>${rows.join('')}</ul>Other nations' staffs open an offensive at about 1 to 1 if they are bold and 2 to 1 if they are cautious.`
    : g.nations[me].atWar.length ? 'You are at war, but no enemy army stands on your borders.' : 'You are at peace. Build up, court your neighbours, and declare war when you choose, or never.';
  return `<div class="note"><b>Free play: there is no plan to follow.</b> You decide who to fight and when. So does every other nation: its staff weighs its fronts each month and makes up its own mind, so this war will not go the way history did. ${body}</div>`;
}

/** The staff's advice on the eve of war: what to do with the army before the blow falls, and a button that does the placing. */
function eveAdvice(): string {
  const plan = evePlan(g);
  if (!plan) return '';
  const where = plan.place.map(([name, n]) => `${n} corps at ${name}`);
  return `<div class="note eve"><b>Your staff's advice: ${esc(plan.title)}.</b><ul>${plan.advice.map((t) => `<li>${esc(t)}</li>`).join('')}</ul>
    <button class="btn small" data-act="eveplan" title="Sends the army by rail, strongest corps first: ${esc(where.join(', '))}. Corps you have already moved stay where you put them. The orders each army is given are left to you.">Place the army as the staff advises</button></div>`;
}

function advice(): string {
  const me = g.human;
  if (!me || !g.nations[me].alive) return '';
  if (onEve(g)) return eveAdvice();
  if (isOpen(g)) return freeAdvice(me);
  const list = (a?: string[]) => (a ?? []).slice(0, 7).join(', ');
  // Only what is still to do: wars not yet declared, objectives still in enemy hands.
  const todo = (a?: string[]) => (a ?? []).filter((nm) => { const r = g.byName[nm]; return r && atWar(g, me, r.ctrl); });
  // The plan's standing orders beyond its objectives: forces to gather or keep in place, and sectors left to allies.
  // An army overseas has no railway and a port to fall back on, so its orders are worded for that.
  const orders = (ph: NonNullable<ReturnType<typeof phaseFor>>, base?: string | null): string => {
    const away = base !== undefined;
    const out: string[] = [];
    const held = (nm: string): boolean => { const r = g.byName[nm]; return !!r && allied(g, me, r.ctrl); };
    const fast = Object.entries(ph.concentrate ?? {}).filter(([nm]) => held(nm));
    if (fast.length) out.push(away
      ? `Bring your tanks and lorried troops up for the stroke to come: ${esc(fast.map(([nm, k]) => `${k} corps at ${nm}`).join(', '))}. March infantry out if there is no room.`
      : `Take the tanks out of the quiet parts of the line and gather them by rail for the summer: ${esc(fast.map(([nm, k]) => `${k} corps at ${nm}`).join(', '))}.`);
    const keep = ph.posture === 'redeploy' ? [] : Object.entries(ph.staging ?? {}).filter(([nm]) => held(nm));
    if (keep.length) {
      const each = keep.map(([nm, k]) => `${k} corps at ${nm}`);
      out.push(`Keep ${esc(each.length <= 2 ? each.join(' and ') : `${each.slice(0, -1).join(', ')} and ${each[each.length - 1]}`)}.`);
    }
    const lent = Object.entries(ph.entrust ?? {}).flatMap(([ally, places]) => places.filter((nm) => held(nm) && g.byName[nm].units.filter((u) => u.nat === ally).length >= 2));
    if (lent.length) out.push(`Your allies now hold ${esc(list(lent))}: history left one corps to stiffen each and sent the rest on.`);
    const out2 = (ph.abandon ?? []).filter((nm) => held(nm) && g.byName[nm].units.some((u) => u.nat === me && u.kind !== 'fort'));
    if (out2.length) out.push(`Give up ${esc(list(out2))}: march the corps there back toward ${away && base ? esc(base) : 'home'} before the road behind them is cut.`);
    const dear = Object.entries(ph.oddsFor ?? {}).filter(([nm, o]) => o < (ph.minOdds ?? 1.3) && !held(nm)).map(([nm]) => nm);
    if (dear.length) out.push(`History threw its army at ${esc(list(dear))} against the odds. You need not.`);
    if (ph.posture === 'attack' && ph.renew === false) out.push('History made this assault once and did not send the army in again when it was held.');
    return out.length ? ` ${out.join(' ')}` : '';
  };
  const main = (): string => {
    const ph = phaseFor(g, me);
    if (!ph) return '';
    if (ph.posture === 'attack') {
      const declare = (ph.declare ?? []).filter((e) => g.nations[e].alive && !atWar(g, me, e));
      // Once the wars the plan calls for are on, only the objectives still in enemy hands are left to name.
      const left = declare.length ? ph.objectives ?? [] : todo(ph.objectives);
      if (!left.length && !declare.length) return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> The plan at home is carried out. Hold what you have.${orders(ph)}</div>`;
      const aims = left.length ? `Objectives, in order: ${esc(list(left))}.` : '';
      return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> ${declare.length ? `Declare war on ${declare.map(natName).join(', ')}. ` : ''}${aims}${ph.landings?.length ? ` From the sea: ${esc(ph.landings.map((l) => `${l.corps} to ${l.to[0]}`).join(', '))}.` : ''}${orders(ph)} You are free to do something else.</div>`;
    }
    if (ph.posture === 'redeploy') return `<div class="note"><b>History's plan this month.</b> Move the army west by rail and wait for spring. Assembly areas: ${esc(list(ph.assembly))}.</div>`;
    return `<div class="note"><b>History's plan this month${ph.name ? `: ${esc(ph.name)}` : ''}.</b> ${ph.quiet ? 'Stand on the defensive and risk nothing: the army is being saved for a later stroke.' : 'Hold the line and keep a reserve near the capital.'}${ph.landings?.length ? ` By sea: ${esc(ph.landings.map((l) => `${l.corps} to ${l.to[0]}`).join(', '))}.` : ''}${orders(ph)}</div>`;
  };
  // An army overseas has a plan of its own.
  const away = theatrePhases(g, me).map(({ theatre, phase: ph, base }) => {
    const head = `<b>History's plan in ${esc(theatre)}${ph?.name ? `: ${esc(ph.name)}` : ''}.</b>`;
    if (ph?.posture === 'attack') {
      const declare = (ph.declare ?? []).filter((e) => g.nations[e].alive && !atWar(g, me, e));
      // Until the wars the plan calls for are declared, every objective is still to be won.
      const left = declare.length ? (ph.objectives ?? []).filter((nm) => { const r = g.byName[nm]; return r && !allied(g, me, r.ctrl); }) : todo(ph.objectives);
      const drops = (ph.landings ?? []).filter((l) => l.to.some((nm) => { const r = g.byName[nm]; return r && !allied(g, me, r.ctrl); }));
      return `<div class="note">${head} ${declare.length ? `Declare war on ${declare.map(natName).join(', ')}. ` : ''}${left.length ? `Objectives, in order: ${esc(list(left))}.` : 'Its objectives are taken.'}${ph.siege?.length ? ` Do not storm ${esc(list(ph.siege))}: starve it out.` : ''}${drops.length ? ` Across the water: ${esc(drops.map((l) => `${l.corps}${l.with?.length ? ` with ${list(l.with)}` : ''} to ${l.to[0]}${l.to.length > 1 ? ` (or ${l.to.slice(1).join(' or ')} if that shore is too strongly held)` : ''}`).join('; '))}.` : ''}${orders(ph, base)} You are free to do something else.</div>`;
    }
    if (ph?.withdraw) {
      const lost = ph.withdraw.when.filter((nm) => { const r = g.byName[nm]; return r && atWar(g, me, r.ctrl); });
      return `<div class="note">${head} ${lost.length ? `${esc(list(lost))} has fallen. Get the expedition to a port and lift it off before it is lost.` : `Stand beside your ally, and be ready to leave: if ${esc(list(ph.withdraw.when))} falls, get the expedition to a port and lift it off.`}</div>`;
    }
    return `<div class="note">${head} ${ph?.quiet ? 'Stand on the defensive and risk nothing: the army is being saved for a later stroke.' : 'Stand on the defensive and build up.'}${ph ? orders(ph, base) : ''}</div>`;
  }).join('');
  return main() + away;
}

function scopeNote(): string {
  if (isOpen(g)) return g.human ? '' : `<div class="note"><b>Free play.</b> No nation is following history's plan. Each one's staff weighs its fronts every month and decides for itself whether to attack, hold or wait, so every war you watch comes out differently.</div>`;
  if (g.turn < STORY_THINS) return '';
  if (g.turn < STORY_ENDS) return `<div class="note"><b>The last winter is still being written.</b> History's plans cover every front to January 1945: Italy to the Gothic Line, the West to the Ardennes, and the eastern front from Leningrad to the siege of Budapest. After that the computer's nations only hold what they have.</div>`;
  return `<div class="note"><b>This is as far as the prototype's story goes.</b> History's plans cover the land war from September 1939 to the end of 1943, and Italy and the West to the end of 1944. You can play on, but the computer's nations will only hold what they have. Shipbuilding and the later campaigns come in later phases.</div>`;
}

/** A meter for a figure that is only an estimate: the band it lies in, hatched. */
const bandMeter = (lo: number, hi: number): string => `<span class="meter"><i class="band" style="margin-left:${Math.round(lo * 100)}%;width:${Math.max(4, Math.round((hi - lo) * 100))}%"></i></span>`;
const pctBand = (lo: number, hi: number): string => (Math.round(lo * 100) === Math.round(hi * 100) ? `${Math.round(lo * 100)}%` : `${Math.round(lo * 100)} to ${Math.round(hi * 100)}%`);
const reportAge = (age: number): string => (age <= 1 ? 'Report from last month' : `Report ${age} months old`);
/** Where a report came from, in a few words. */
const SOURCE_WORD: Record<string, string> = { battle: 'met in battle', agent: "an agent's report", air: 'air photographs' };
const sourceWord = (s: string | null): string => (s ? SOURCE_WORD[s] ?? '' : '');
/** A flag with a question mark: a corps that is there, and nothing more is known of it. */
const unknownIcon = (nat: string): string => `<svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[nat].color}" stroke-dasharray="2.4 1.6"/><text class="qm" x="12" y="11.6">?</text></svg>`;

/** A foreign corps known from a report gone stale: its kind and divisions, an estimate of its strength, and how old the report is. */
function reportRow(c: Corps): string {
  const card = corpsKnownAs(g, c);
  if (!card) return '';
  const lo = Math.round(c.divs * card.strLo * 15) * 1000, hi = Math.round(c.divs * card.strHi * 15) * 1000;
  return `<div class="unit"><div class="corps stale">${icon(c)}<span class="nm">${esc(c.name)}</span><span></span>
    <span class="meta">${g.nations[c.nat].adj} ${KIND[c.kind].label.toLowerCase()} · ${c.divs} division${c.divs > 1 ? 's' : ''} · about ${lo === hi ? num(lo) : `${num(lo)} to ${num(hi)}`} men · ${esc(card.grade)}</span>
    <span class="m3 wide"><span class="mrow" title="An estimate from a report ${card.age} month${card.age > 1 ? 's' : ''} old: the older the report, the wider the band."><small>Strength</small>${bandMeter(card.strLo, card.strHi)}<em>${pctBand(card.strLo, card.strHi)}</em></span><span class="mrow"><small>Rest</small><span class="meter"></span><em>Not known</em></span><span class="mrow"><small>Morale</small><span class="meter"></span><em>Not known</em></span></span>
    <span class="tags"><span class="tag" title="This corps was last identified ${card.age} month${card.age > 1 ? 's' : ''} ago. ${HOW_TO_KNOW}">${reportAge(card.age)}${card.source ? ` · ${sourceWord(card.source)}` : ''}</span></span></div></div>`;
}

/** Foreign corps that are seen and counted but not identified: one row for each nation's. */
function unknownRow(r: Region, nat: string, cs: Corps[]): string {
  const e = seen[r.id] >= 2 ? armyAt(g, r.id, nat)?.est ?? null : null;
  const all = r.units.filter((c) => c.nat === nat).length === cs.length;
  return `<div class="unit"><div class="corps unknown">${unknownIcon(nat)}<span class="nm">${cs.length} ${esc(g.nations[nat].adj)} corps not identified</span><span></span>
    <span class="meta">Their kinds and their strength are not known.${e ? ` The staff counts ${estWords(e)} in ${all ? 'this army' : 'the whole army here'}.` : ''} ${HOW_TO_KNOW}</span></div></div>`;
}

/** The corps in a province as the player knows them: its own and its allies' in full, a foreign army's as far as it has been identified. */
function knownRows(r: Region, cs: Corps[]): string {
  let out = '';
  const dark = new Map<string, Corps[]>();
  for (const c of cs) {
    const k = kn(c);
    if (k === 'exact') out += corpsRow(c, canOrder(g, c));
    else if (k === 'report') out += reportRow(c);
    else (dark.get(c.nat) ?? dark.set(c.nat, []).get(c.nat)!).push(c);
  }
  for (const [nat, xs] of dark) out += unknownRow(r, nat, xs);
  if (cs.some((c) => kn(c) !== 'exact')) out += scoutButton(r);
  return out;
}

/** A button that sends the nearest of the player's air groups able to photograph a province; nothing where none can fly it. */
function scoutButton(r: Region): string {
  if (!humanUp(g) || !airWar(g)) return '';
  const p = previewRaid(g, 'recon', r.id);
  const f = !p.why && p.picked.length ? g.airFleets.find((x) => x.id === p.picked[0]) : null;
  if (!f) return '';
  return `<button class="btn small scoutnow" data-act="scoutnow" data-id="${r.id}" data-fleet="${f.id}" title="${esc(p.verdict)}. ${esc(p.effects[0] ?? '')}">${ICON_CAMERA} Send a reconnaissance flight <small>${esc(f.name)}</small></button>`;
}

function corpsRow(c: Corps, mine: boolean): string {
  const k = KIND[c.kind];
  const pick = ui.picked.has(c.id);
  const tags: string[] = [];
  if (!c.supplied) tags.push('<span class="tag bad">Cut off</span>');
  if (c.fought) tags.push('<span class="tag">Fought this month</span>');
  // A foreign corps is shown whole only because it has been identified this month: the row says how.
  const src = mine || friendOf(c.nat) ? null : corpsKnownAs(g, c)?.source ?? null;
  if (src) tags.push(`<span class="tag good" title="Exact this month. From next month the report goes stale: its strength becomes an estimate, and its rest and morale are no longer known.">Identified · ${sourceWord(src)}</span>`);
  const line = lineStrength(g, c);
  if (line.mul < 1) tags.push(inDesert(g.regions[c.reg])
    ? `<span class="tag bad" title="Counted from the nearest working port, a step for every region of desert. Take a port nearer the front, or fall back toward one.">${Math.round(line.length)} regions from port: attacks at ${Math.round(line.mul * 100)}%</span>`
    : `<span class="tag bad">Long supply line: attacks at ${Math.round(line.mul * 100)}%</span>`);
  if (frozen(g, c.nat, g.regions[c.reg])) tags.push('<span class="tag bad">No winter kit</span>');
  if (groundRules(g) && c.kind !== 'fort') {
    const here = g.regions[c.reg], d = groundDefence(g, c.kind, here), a = groundAttack(g, c.kind, here);
    const pc = (m: number): string => `${Math.round(Math.abs(m - 1) * 100)}%`;
    if (d > 1) tags.push(`<span class="tag good" title="Mountain troops are trained and equipped for this country. In mountains and hills they attack and defend more strongly than anyone else.">At home on this ground: defence up ${pc(d)}</span>`);
    else if (d < 1) tags.push(`<span class="tag bad" title="This is poor country for ${esc(k.label.toLowerCase())}. Open ground suits them better.">Poor ground for ${c.kind === 'arm' ? 'tanks' : c.kind === 'mot' ? 'lorried troops' : 'cavalry'}: defence down ${pc(d)}</span>`);
    else if (a < 1 && c.kind === 'cav') tags.push('<span class="tag bad">No ground for cavalry</span>');
  }
  const gen = generalOf(g, c);
  for (const t of gen?.traits ?? []) {
    const d = COMMAND.traits[t];
    if (d) tags.push(`<span class="tag ${d.good ? 'good' : 'bad'}" title="${esc(d.effect)}">${esc(d.label)}</span>`);
  }
  const special = eliteIn(c);
  if (special) tags.push(`<span class="tag good" title="${esc(special.text)} ${esc(eliteRule(special))}">★ ${esc(special.label)}</span>`);
  tags.push(...storyTags(c));
  if (c.job) tags.push(`<span class="tag good" title="${esc(jobLine(g, c))}">At work</span>`);
  else if (c.kind === 'eng' && c.nat === g.human && groundRules(g)) tags.push('<span class="tag" title="Give it work from the list under the forces: bridges, railways, bombed factories, fortifications, minefields or demolition.">Engineers: ready for work</span>');
  if (c.airborne) tags.push(`<span class="tag good" title="Parachute and glider troops. Once a month they can be flown from a friendly coast to an enemy one across a single sea, with no ships and whoever holds the water. They fight there at ${Math.round(NAVY.landing.airborne * 100)}%, without heavy weapons, and not under a sky the enemy's fighters hold.">Airborne</span>`);
  if (c.hold && !mine) tags.push('<span class="tag">Garrison</span>');
  if (c.yields && c.nat === g.human) tags.push(`<span class="tag" title="${esc(STANCE_BACK)}">Falls back if attacked</span>`);
  if (c.dug) tags.push(`<span class="tag good${autoDig() && worksOf(c) < 0.95 ? ' worn' : ''}" title="${esc(autoDig() ? `${worksWords(c)} ${STANCE_HOLD}` : STANCE_DIG)}">${autoDig() ? `Dug in · ${worksTag(c)}` : 'Dug in'}</span>`);
  else if (autoDig() ? mine && c.kind !== 'fort' && c.town === undefined && !c.yields && !c.fought && c.ap >= c.apMax : c.digs && c.nat === g.human) tags.push(`<span class="tag" title="${esc(autoDig() ? STANCE_HOLD : STANCE_DIG)}">Digging in: ready next month if it stays put</span>`);
  const pips = c.kind === 'fort' ? '' : `<span class="pips" title="Moves left this month">${Array.from({ length: Math.max(c.apMax, c.ap) }, (_, i) => `<i class="${i < c.ap ? '' : 'off'}"></i>`).join('')}</span>`;
  const el = mine && c.kind !== 'fort' ? 'button' : 'div';
  const card = corpsDetail(g, c);
  if (card.mood.resting && c.nat === g.human && (c.str < 0.97 || c.org < 0.9 || (card.mood.morale ?? 1) < 0.7)) tags.push(`<span class="tag good" title="${esc(REST_RULE)}">Resting</span>`);
  return `<div class="unit"><${el} class="corps${pick ? ' pick' : ''}" ${el === 'button' ? `data-act="pick" data-id="${c.id}"` : ''}>
    ${icon(c)}<span class="nm">${esc(c.name)}${c.nick ? ` <i class="nick">${esc(c.nick)}</i>` : ''}${gen ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(gen.name)} <span class="lvl" title="Level ${gen.level} general">${gen.level}</span></span>` : ''}</span>${pips}
    <span class="meta">${g.nations[c.nat].adj} ${k.label.toLowerCase()} · ${c.divs} division${c.divs > 1 ? 's' : ''} · ${num(men(c))} men · ${esc(card.service)}</span>
    <span class="chits wide">${card.divisions.map((d) => chit(d, true)).join('')}</span>
    ${moodMeters(c, card.mood, true)}${moodWhy(card.mood)}
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>${jointStrip(c)}${recStrip('corps', c)}</div>`;
}

/** Under an ally's corps in the province panel: the button that asks it to join the player's army, or sends it back. */
function jointStrip(c: Corps): string {
  const me = g.human;
  if (!me || c.nat === me || !friendOf(c.nat) || c.kind === 'fort' || g.flags.open !== 1) return '';
  const adj = esc(g.nations[c.nat].adj);
  if (c.joint === me) {
    return `<div class="jstrip on">${chip(c.nat)}<span class="grow">Marching with your army. It takes your orders, and the ground it helps to take is yours.</span><button class="btn small" data-act="sendback" data-id="${c.id}" ${humanUp(g) ? '' : 'disabled'} title="It goes back to ${adj} command and does not march again this month.">Send back</button></div>`;
  }
  const offer = joinOffers(g, c.reg).find((o) => o.corps === c.id);
  if (!offer) return '';
  return `<div class="jstrip">${chip(c.nat)}<span class="grow">${offer.why ? esc(offer.why) : `${adj} troops beside yours: they can march and attack with your army.`}</span><button class="btn small" data-act="joinarmy" data-id="${c.id}" ${offer.why ? 'disabled' : ''} title="${offer.why ? esc(offer.why) : `It keeps its own flag, pay and supply, takes your orders in your month, and goes back to ${adj} command when you send it or leave it behind.`}">Join my army</button></div>`;
}

/** In the tray of the player's army: who marches with it, and the allies standing beside it who could. */
function jointBar(r: Region): string {
  const me = g.human;
  if (!me || g.flags.open !== 1) return '';
  const with_ = r.units.filter((c) => c.joint === me && !c.dead);
  const offers = joinOffers(g, r.id);
  const can = offers.filter((o) => !o.why);
  if (!with_.length && !offers.length) return '';
  const nats = (ids: string[]): string => [...new Set(ids)].map((n) => `${chip(n)} ${esc(g.nations[n].adj)}`).join(', ');
  const up = humanUp(g);
  const spare = lending(g);
  const room = (n: string): string => { const x = spare.find((s) => s.nat === n); return x ? `${g.nations[n].name} has lent you ${x.lent} of the ${x.limit} corps it can spare.` : ''; };
  const here = with_.length ? `<small title="${esc([...new Set(with_.map((c) => c.nat))].map(room).join(' '))}">Joint army · with ${nats(with_.map((c) => c.nat))} troops (${with_.length} corps)</small>` : '';
  const ask = can.length
    ? `<button class="btn small" data-act="joinall" ${up ? '' : 'disabled'} title="They keep their own flags, pay and supply, march and attack with your army under your orders, and go back to their own command when you send them or leave them behind.">Ask ${can.length === 1 ? `the ${esc(g.nations[can[0].nat].adj)} corps` : `${can.length} allied corps`} here to join</button>`
    : offers.length && !with_.length ? `<small title="${esc(offers[0].why ?? '')}">${nats(offers.map((o) => o.nat))} troops stand here too: ${esc(offers[0].why ?? '')}</small>` : '';
  const back = with_.length ? `<button class="btn small quiet" data-act="sendbackall" ${up ? '' : 'disabled'} title="Every allied corps with this army goes back to its own command.">Send them back</button>` : '';
  return `<div class="jointbar">${here}${ask}${back}</div>`;
}

// ---- The army tray: the general and his units, laid out as cards along the foot of the map ----

/** The earth bank round the foot of an army's standard: at least half of it has had a month to dig in. */
const worksMark = (cs: Corps[]): string => {
  const field = cs.filter((c) => c.kind !== 'fort');
  const d = 'M-2.8 17A13.8 4.2 0 0 0 24.8 17';
  return field.length && field.filter((c) => c.dug).length * 2 >= field.length ? `<path class="worksbed" d="${d}"/><path class="works" d="${d}"><title>Dug in</title></path>` : '';
};
const STANCE_STAND = 'Stand and fight wherever they are attacked.';
/** The grand campaign: nobody has to be told to dig. */
const autoDig = (): boolean => groundRules(g) && DEFENCE.entrench.auto;
/** How much digging troops get done in a month in which they were attacked (the grand campaign), in words. */
const pressedPace = (): string => { const r = DEFENCE.wear.regrow; return r <= 0 ? 'do no digging' : r >= 1 ? 'dig as usual' : r === 0.5 ? 'dig at half pace' : `get ${Math.round(r * 100)}% of their digging done`; };
/** How far a dug-in corps has got with its works (the grand campaign): for its tag, and for the tip behind it. */
const worksTag = (c: Corps): string => { const w = Math.round(worksOf(c) * 10) / 10, full = DEFENCE.entrench.months; return w >= full ? 'works finished' : w < 0.05 ? 'works battered flat' : `works ${w} of ${full} months`; };
const worksWords = (c: Corps): string => { const w = Math.round(worksOf(c) * 10) / 10, full = DEFENCE.entrench.months; return w >= full ? 'Its works are finished.' : `Its works are ${w === 1 ? 'a month' : `${w} months`} old of the ${full} they take to finish, and count for ${Math.round(worksShare(worksOf(c)) * 100)}% of finished ones.`; };
const STANCE_HOLD = `Stand and fight. An army that spends a whole month where it stands, without marching or attacking, digs in: from then on it defends 25% stronger, and while it is in supply the day's luck runs against whoever attacks it, so that odds which would be safe in the open become a gamble (an even chance takes about ${evenOdds(DEFENCE.prepared.dice).toFixed(1)} to 1, and nothing is sure under ${(1.2 / DEFENCE.prepared.dice[0]).toFixed(1)} to 1). An attack thrown back from such a line does nothing more that month. Fieldworks (foxholes, gun pits, anti-tank ditches and mines) belong to the ground: an army that marches off leaves them behind, a corps of yours that marches in has them at once, and standing empty they lose a month of work every month. Guns in the attack break them up, bombers can be sent against them, and a probe by one corps takes a month off them. Works take ${DEFENCE.entrench.months} months to finish, and the first month does half the job. Every real attack on a line, won or lost, takes a month off its works, more with guns, and troops who were attacked ${pressedPace()} that month: steady pressure wears a line away, and a quiet month builds it back.`;
const STANCE_DIG = 'Dig in. An army that spends a whole month where it stands, without marching or attacking, fights from fieldworks from then on (foxholes, gun pits, anti-tank ditches and mines) and defends 25% stronger. It keeps them for as long as it stays put, and loses them the moment it leaves the ground.';
const STANCE_BACK = 'Give ground instead of fighting when the enemy comes on with odds of 1.2 to 1 or better, which is what it takes to carry a position. The troops slip away whole to the ground behind them, at a cost in cohesion and the region itself. Caught again in the same month, they stand. Fortress troops always stand, and nobody gives up their own capital without a fight.';

/** The army the tray shows for the selected region: the player's own if he has one there, otherwise the largest. */
function armyShown(): { reg: number; nat: string } | null {
  if (ui.sel < 0 || ui.tab !== 'region') return null;
  const r = g.regions[ui.sel];
  if (!r.units.length) return null;
  const size = new Map<string, number>();
  // The tray never opens on an army the fog hides.
  for (const c of r.units) if (inFull(r, c.nat)) size.set(armyOf(c), (size.get(armyOf(c)) ?? 0) + c.divs * c.str);
  if (!size.size) return null;
  const me = g.human;
  return { reg: r.id, nat: me && size.has(me) ? me : [...size].sort((a, b) => b[1] - a[1])[0][0] };
}

const REST_RULE = 'A corps that spends a whole month in supply without marching or fighting rests: its drafts arrive half as fast again, it gets back more of its rest, and its morale recovers.';

/** One division as a chit: its arm in its nation's colour, a stripe for each grade of experience, its number, and its strength along the foot. */
function chit(d: DivisionCard, named = false): string {
  const pct = Math.round(d.str * 100);
  const stripes = `<span class="stripes" aria-hidden="true">${'<i></i>'.repeat(d.stripes)}</span>`;
  return `<span class="chit${d.hardened ? ' hard' : ''}${d.str < 0.5 ? ' weak' : ''}" title="${esc(d.name)} · ${pct}% strength, ${num(d.men)} men · ${esc(d.grade)}${d.hardened ? ', battle-hardened' : ''}">
    <span class="ctop"><svg width="26" height="19" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[d.nat].color}"/>${symbol(d.kind, 24, 15)}</svg>${stripes}</span>
    <b>${esc(named ? d.short : ordinal(d.no))}</b>${named ? `<small>${esc(d.grade)}</small>` : ''}<span class="meter"><i class="${d.str < 0.5 ? 'low' : ''}" style="width:${pct}%"></i></span></span>`;
}

/** Strength, rest and morale as three meters. Each says in a tooltip where it stands and why. */
function moodMeters(c: Corps, m: Mood = moodOf(g, c), wide = false): string {
  const row = (label: string, v: number, cls: string, title: string, word = '', extra = ''): string =>
    `<span class="mrow" title="${esc(title)}"><small>${label}</small><span class="meter"><i class="${cls}" style="width:${Math.round(v * 100)}%"></i></span>${wide ? `<em>${esc(word)}</em>` : ''}${extra}</span>`;
  const s = Math.round(c.str * 100);
  let out = row('Strength', c.str, c.str < 0.5 ? 'low' : '', `Strength ${s}%: ${num(men(c))} men`, `${s}%`)
    + row('Rest', m.rest, `org${m.rest < 0.4 ? ' low' : ''}`, `Rest ${Math.round(m.rest * 100)}%. ${m.restLine}`, m.restWord);
  if (m.morale !== null) out += row('Morale', m.morale, `mor${m.morale < 0.4 ? ' low' : ''}`, `Morale ${Math.round(m.morale * 100)}%. ${m.line}`, m.moraleWord, `<i class="trend ${m.trend}" title="Morale ${m.trend}"></i>`);
  return `<span class="m3${wide ? ' wide' : ''}">${out}</span>`;
}

/** Why a corps' morale stands where it does: a sentence, then each reason with what it was worth. */
function moodWhy(m: Mood): string {
  if (m.morale === null) return '';
  const pts = (d: number): string => `${d > 0 ? '+' : '−'}${Math.max(1, Math.round(Math.abs(d) * 100))}`;
  const why = m.why.map((w) => `<i class="${w.delta > 0 ? 'up' : 'dn'}${w.earlier ? ' old' : ''}"${w.earlier ? ' title="From the month before"' : ''}>${esc(w.text)} ${pts(w.delta)}</i>`).join('');
  return `<span class="moodl"><b>Morale:</b> ${esc(m.line)}${why ? `<span class="why">${why}</span>` : ''}<span class="restl"><b>Rest:</b> ${esc(m.restLine)}</span></span>`;
}

/** One corps of the army as a card, with its divisions as chits. The player's own are buttons: a click leaves the corps behind or takes it along. */
function unitCard(c: Corps, mine: boolean): string {
  const can = mine && c.kind !== 'fort';
  const el = can ? 'button' : 'div';
  const pips = c.kind === 'fort' ? '' : `<span class="pips" title="Moves left this month">${Array.from({ length: Math.max(c.apMax, c.ap) }, (_, i) => `<i class="${i < c.ap ? '' : 'off'}"></i>`).join('')}</span>`;
  const gen = generalOf(g, c);
  const special = eliteIn(c);
  const card = corpsDetail(g, c);
  const resting = card.mood.resting && c.nat === g.human && (c.str < 0.97 || c.org < 0.9 || (card.mood.morale ?? 1) < 0.7) ? `<span class="tag good" title="${esc(REST_RULE)}">Resting</span>` : '';
  const flags = `${resting}${special ? `<span class="tag good" title="${esc(special.label)}: ${esc(eliteRule(special))}">★ ${esc(special.word)}</span>` : ''}${c.supplied ? '' : '<span class="tag bad">Cut off</span>'}${c.fought ? '<span class="tag">Fought</span>' : ''}${c.yields && c.nat === g.human ? '<span class="tag" title="Under orders to give ground when the enemy attacks in strength">Falls back</span>' : ''}${c.dug ? '<span class="tag good" title="In fieldworks: defends 25% stronger while it stands on this ground">Dug in</span>' : c.digs && c.nat === g.human ? '<span class="tag" title="Under orders to dig in: the fieldworks are ready next month if it stays put">Digging</span>' : ''}`;
  return `<${el} class="ucard corpsbox${ui.picked.has(c.id) ? ' pick' : ''}${can && c.ap <= 0 ? ' spent' : ''}" style="--n:${c.divs}" ${can ? `data-act="pick" data-id="${c.id}"` : ''} title="${esc(c.name)}${c.nick ? `, ${esc(c.nick)}` : ''}${gen ? ` · ${esc(gen.name)}` : ''} · ${c.divs} ${esc(DIVISIONS.kinds[c.kind].word)} division${c.divs > 1 ? 's' : ''}, ${num(men(c))} men">
    <span class="utop"><b>${esc(c.name)}</b>${pips}</span>
    <span class="um vet">${c.joint ? `${chip(c.nat)} ${esc(g.nations[c.nat].adj)} · ` : ''}${esc(card.service)}${gen ? ` · ${esc(surname(gen.name))}` : ''}${c.leave !== undefined ? ' · at home' : ''} ${ribbons(c, 3)}</span>
    <span class="chits">${card.divisions.map((d) => chit(d)).join('')}</span>
    ${moodMeters(c, card.mood)}${flags ? `<span class="tags">${flags}</span>` : ''}</${el}>`;
}

/**
 * A foreign corps the player does not know in full, as a card: one known from a stale report shows its kind, its
 * divisions and an estimate of its strength; one not identified is a flag and a question mark.
 */
function foeCard(c: Corps, tray = false): string {
  const card = corpsKnownAs(g, c);
  const cls = tray ? 'ucard corpsbox' : 'ucard';
  if (!card) {
    return `<div class="${cls} unknown" style="--n:2" title="A corps that has not been identified: its kind and its strength are not known. ${HOW_TO_KNOW}"><span class="utop">${tray ? '<b>Not identified</b>' : unknownIcon(c.nat)}</span>${tray ? `<span class="um vet">${esc(g.nations[c.nat].adj)} corps</span><span class="chits">${unknownIcon(c.nat)}</span>` : `<b>Not identified</b><span class="um">${esc(g.nations[c.nat].adj)} corps</span>`}
      <span class="um">Kind and strength not known</span></div>`;
  }
  const title = `${esc(c.name)} · ${c.divs} ${esc(DIVISIONS.kinds[c.kind].word)} division${c.divs > 1 ? 's' : ''} · strength ${pctBand(card.strLo, card.strHi)}, from a report ${card.age} month${card.age > 1 ? 's' : ''} old`;
  return `<div class="${cls} stale" style="--n:${c.divs}" title="${title}"><span class="utop">${tray ? `<b>${esc(c.name)}</b>` : icon(c)}</span>${tray ? '' : `<b>${esc(c.name)}</b>`}
    <span class="um${tray ? ' vet' : ''}">${c.divs} ${esc(tray ? DIVISIONS.kinds[c.kind].word : KIND[c.kind].label.toLowerCase())} div${c.divs > 1 ? 's' : ''} · ${esc(card.grade)}</span>${tray ? `<span class="chits">${icon(c)}</span>` : ''}
    ${bandMeter(card.strLo, card.strHi)}<span class="um">Strength ${pctBand(card.strLo, card.strHi)}</span>
    <span class="tags"><span class="tag" title="${card.source ? `From ${sourceWord(card.source)}. ` : ''}${HOW_TO_KNOW}">${reportAge(card.age)}</span></span></div>`;
}

/** The player's fleet that is picked up under the Navy tab: its tray is open even in a month that is not his to give orders in. */
function fleetShown(): Fleet | null {
  if (ui.tab !== 'navy' || ui.fleet < 0 || !g.human) return null;
  const f = g.fleets.find((x) => x.id === ui.fleet);
  return f && f.nat === g.human && shipCount(f.ships) > 0 ? f : null;
}

/** A ship of one class on a plate in its navy's colour, as the arsenal draws it. */
function shipPlate(nat: string, cls: string, w = 22, h = 16): string {
  const [d, k] = HULL_OF[cls];
  return `<svg width="${w}" height="${h}" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[nat].color}"/><path class="symf" d="${d}" transform="translate(12 7.5) scale(${k})"/></svg>`;
}

/** One class of a fleet's ships as a card: how many, a plate for each of the first few, and what they are for. */
function shipCardHtml(card: FleetCard, c: ShipCard): string {
  const shown = Math.min(c.count, 10);
  return `<div class="ucard corpsbox shipbox" style="--n:3" title="${esc(c.label)}: ${esc(c.role)}">
    <span class="utop"><b>${esc(c.label)}</b><span class="shipn">${c.count}</span></span>
    <span class="um vet">${esc(c.brief)}</span>
    <span class="hulls">${shipPlate(card.nat, c.cls, 26, 19).repeat(shown)}${c.count > shown ? `<i>+${c.count - shown}</i>` : ''}</span>
    ${c.lost ? `<span class="tags"><span class="tag bad" title="Sunk since the war began">${c.lost} lost</span></span>` : ''}</div>`;
}

/** A fighter or a strike aircraft on a plate in its navy's colour. */
function planePlate(nat: string, kind: 'ftr' | 'bmr'): string {
  return `<svg width="26" height="19" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[nat].color}"/><path class="symf" d="${kind === 'bmr' ? BOMBER : PLANE}" transform="translate(12 7.6) scale(${kind === 'bmr' ? '.56' : '.62'})"/></svg>`;
}

/** One of a fleet's carrier wings as a small card in its tray: its aircraft, and the way to their orders. The two wings stand one above the other beside the carriers. */
function wingCardHtml(card: FleetCard, w: WingCard): string {
  const short = Math.round(w.est - w.aircraft);
  const can = humanUp(g) && g.nations[card.nat].atWar.length > 0 && w.aircraft > 0;
  const tags = [
    w.flew ? '<span class="tag">Flew this month</span>' : can ? `<span class="tag good">${w.kind === 'bmr' ? 'Plan a strike' : 'Orders'}</span>` : '',
    short > w.est * 0.1 ? `<span class="tag bad" title="${esc(w.note)}">${num(short)} short</span>` : '',
  ].join('');
  return `<button class="ucard wingmini" data-act="wing" data-id="${w.id}" title="${esc(`${w.name}. ${w.doing}. Reach ${num(w.range)} km. ${w.note}${w.lost > 0 ? ` ${num(w.lost)} lost since the war began.` : ''}`)}">
    <span class="utop">${planePlate(card.nat, w.kind)}<b>${esc(w.label)}</b><span class="shipn">${num(w.aircraft)}</span></span>
    <span class="um">${w.aircraft <= 0 ? 'No aircraft aboard' : esc(w.brief)}</span>
    <span class="tags">${tags}</span></button>`;
}

/** The order a fleet is under, or (in harbour) the one it will sail under when a sea is clicked. */
const fleetStance = (f: Fleet): Stance => (atSea(f) ? f.stance : ui.stance);

/** The tray of a fleet that has been picked up: its admiral, its ships as cards by class, and its orders. */
function drawFleetTray(f: Fleet): void {
  const el = $('#tray');
  const card = fleetView(g, f.id)!;
  const n = g.nations[f.nat];
  const adm = card.admiral;
  const can = !!myFleet() && n.atWar.length > 0;
  const pieces = seaWar(g);
  const head = `<div class="tgen"><span class="seal${adm ? '' : ' none'}" style="background:${n.color}">${adm ? esc(initialsOf(adm.name)) : ''}</span><span class="grow"><b>${esc(card.name)}</b><small>${adm ? `Admiral ${esc(surname(adm.name))}, level ${adm.level} · ` : ''}${esc(card.doing)}</small></span></div>`;
  const bar = (label: string, v: number, cls: string, word: string, title: string): string => `<span class="mrow" title="${esc(title)}"><small>${label}</small><span class="meter"><i class="${cls}" style="width:${Math.round(v * 100)}%"></i></span><em>${esc(word)}</em></span>`;
  const heavy = f.ships.bb + f.ships.cv + f.ships.ca;
  const screen = pieces && heavy > 0
    ? bar('Screen', 1 - card.screen.gap, card.screen.gap > 0.4 ? 'low' : 'org', card.screen.word, `${card.screen.have} destroyers for ${heavy} heavy ships; ${card.screen.need} would screen them fully. A fleet short of destroyers can lose a heavy ship to a submarine${card.screen.risk >= 0.01 ? `: about ${Math.round(card.screen.risk * 100)}% this month where it is` : ''}.`)
    : '';
  const tally = `<div class="tsum"><small class="tcount">${card.ships} ship${card.ships === 1 ? '' : 's'} · fleet strength ${num(card.power)}${pieces && card.guns >= FLEETS.bombard.least ? ` · guns ${num(card.guns)}` : ''}</small>
    <span class="m3 wide">${bar('Ready', card.ready, card.ready < 0.5 ? 'low' : '', `${Math.round(card.ready * 100)}%`, 'How fit for sea the ships are. A fleet in harbour refits quickly.')}${screen}${card.wings.length ? bar('Aircraft', card.decks, card.decks < 0.5 ? 'low' : 'mor', `${num(card.wings.reduce((t, w) => t + w.aircraft, 0))} of ${num(card.wings.reduce((t, w) => t + w.est, 0))}`, 'The aircraft aboard its carriers against what the decks can carry. A wing is made up to strength only while the fleet lies in harbour, and a carrier with empty decks is worth far less in a fight.') : ''}</span></div>`;
  const now = fleetStance(f);
  const ord = (s: Stance, label: string): string => `<button class="${now === s ? 'on' : ''}" data-act="stance" data-stance="${s}" title="${esc(FLEETS.orders[s].blurb)}">${label}</button>`;
  const orders = can
    ? `<div class="tstance fleetord" role="group" aria-label="Orders for this fleet"><small>Orders</small>${ord('patrol', 'Patrol')}${ord('raid', 'Raid')}${pieces ? `${ord('hunt', 'Hunt subs')}<button class="${f.stance === 'escort' ? 'on' : ''}" data-act="fleetsec" data-sec="flanes" title="${esc(FLEETS.orders.escort.blurb)}">Escort</button>` : ''}<button data-act="toport" ${atSea(f) ? '' : 'disabled'} title="${esc(FLEETS.orders.port.blurb)}">Port</button>${pieces ? `<button data-act="fleetsplit" ${fleetOptions(g, f.id)?.split ? '' : 'disabled'} title="Send some of its ships off as a squadron of their own.">Split</button>` : ''}</div>`
    : `<small class="tcount">${n.atWar.length ? 'Orders can be given in your own month.' : 'At peace: the fleet stays in harbour.'}</small>`;
  // The wings stand beside the carriers they fly from.
  const wingCol = card.wings.length ? `<div class="wingcol">${card.wings.map((w) => wingCardHtml(card, w)).join('')}</div>` : '';
  const withCv = card.cards.some((c) => c.cls === 'cv');
  const cards = card.cards.map((c) => shipCardHtml(card, c) + (c.cls === 'cv' ? wingCol : '')).join('') + (withCv ? '' : wingCol);
  el.innerHTML = `<div class="thead">${head}${tally}${orders}</div><div class="cards${card.wings.length ? ' haswings' : ''}">${cards}</div>`;
  el.classList.add('open');
}

function drawTray(): void {
  const el = $('#tray');
  const a = trayArmy;
  const fleet = a ? null : fleetShown();
  if (fleet) { drawFleetTray(fleet); return; }
  if (!a) { el.classList.remove('open'); el.innerHTML = ''; return; }
  const r = g.regions[a.reg];
  const last = (c: Corps): number => (c.kind === 'fort' ? 1 : 0);
  // The player's own corps first, then the allies' that march with them.
  const cs = r.units.filter((c) => armyOf(c) === a.nat).sort((x, y) => last(x) - last(y) || (x.nat === a.nat ? 0 : 1) - (y.nat === a.nat ? 0 : 1) || y.divs * y.str - x.divs * x.str);
  const mine = a.nat === g.human && humanUp(g);
  const who = leaderOf(cs);
  const n = g.nations[a.nat];
  const initials = who ? initialsOf(who.name) : '';
  const others = [...new Set(r.units.filter((c) => armyOf(c) !== a.nat).map((c) => c.nat))];
  const head = who
    ? `<button class="tgen" data-act="general" data-id="${who.id}" title="Open this general's record"><span class="seal" style="background:${n.color}">${esc(initials)}</span><span class="grow"><b>${esc(who.name)}</b><small>${esc(n.adj)} army at ${esc(r.name)} · ${esc(rankOf(who))}, level ${who.level}</small>${who.traits.length ? `<span class="tags">${traitTags(who)}</span>` : ''}</span></button>`
    : `<div class="tgen"><span class="seal none" style="background:${n.color}"></span><span class="grow"><b>${esc(n.adj)} army at ${esc(r.name)}</b><small>No general in command</small></span></div>`;
  // What the army is made of, arm by arm, and how it stands as a whole.
  const army = armyAt(g, a.reg, a.nat);
  const bar = (label: string, v: number, cls: string, word: string): string => `<span class="mrow" title="${label} ${Math.round(v * 100)}%"><small>${label}</small><span class="meter"><i class="${cls}" style="width:${Math.round(v * 100)}%"></i></span><em>${esc(word)}</em></span>`;
  const tally = army && army.known === 'full'
    ? `<div class="tsum"><div class="tmix" title="${esc(army.line)}">${army.mix.map((m) => `<span class="mixc"><svg width="22" height="16" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${n.color}"/>${symbol(m.kind, 24, 15)}</svg><b>${m.divs}</b><small>${esc(m.word)}</small></span>`).join('')}</div>
      <small class="tcount">${army.divs} division${army.divs === 1 ? '' : 's'} · ${num(army.men)} men${others.length ? ` · also here: ${others.map((o) => esc(g.nations[o].adj)).join(', ')}` : ''}</small>
      <span class="m3 wide">${bar('Strength', army.str, army.str < 0.5 ? 'low' : '', `${Math.round(army.str * 100)}%`)}${bar('Rest', army.rest, `org${army.rest < 0.4 ? ' low' : ''}`, army.restWord)}${army.morale !== null ? bar('Morale', army.morale, `mor${army.morale < 0.4 ? ' low' : ''}`, army.moraleWord) : ''}</span></div>`
    : army && army.known === 'partial'
    ? `<div class="tsum"><div class="tmix" title="${esc(army.line)}">${army.mix.map((m) => `<span class="mixc"><svg width="22" height="16" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${n.color}"/>${symbol(m.kind, 24, 15)}</svg><b>${m.divs}</b><small>${esc(m.word)}</small></span>`).join('')}${army.unidentified ? `<span class="mixc">${unknownIcon(a.nat).replace('width="34" height="24"', 'width="22" height="16"')}<b>${army.unidentified}</b><small>corps not identified</small></span>` : ''}</div>
      <small class="tcount">${army.corpsCount} corps · ${army.est ? `the staff counts ${estWords(army.est)}` : 'strength not known'}${army.oldest ? ` · oldest report ${army.oldest} month${army.oldest > 1 ? 's' : ''} old` : ''}</small>
      <small class="tcount fogline">${HOW_TO_KNOW}</small></div>`
    : `<div class="ttally"><b>${num(sumDivs(cs))}</b><small>divisions</small><b>${Math.round(fitness(cs) * 100)}%</b><small>strength</small>${others.length ? `<small class="also">Also here: ${others.map((o) => esc(g.nations[o].adj)).join(', ')}</small>` : ''}</div>`;
  // The standing order for the day the enemy comes: the whole army in the tray takes it together.
  const field = cs.filter((c) => c.kind !== 'fort');
  const under = new Set(field.map(postureOf));
  const all = (p: Posture): string => (under.size === 1 && under.has(p) ? 'on' : '');
  const stance = mine && field.length
    ? `<div class="tstance" role="group" aria-label="Orders if this army is attacked"><small>If attacked${under.size > 1 ? ' · mixed' : ''}</small>${autoDig()
      ? `<button class="${!under.has('back') ? 'on' : ''}" data-act="ifattacked" data-v="stand" title="${esc(STANCE_HOLD)}">Stand and dig in</button>`
      : `<button class="${all('stand')}" data-act="ifattacked" data-v="stand" title="${esc(STANCE_STAND)}">Stand</button><button class="${all('dig')}" data-act="ifattacked" data-v="dig" title="${esc(STANCE_DIG)}">Dig in</button>`}<button class="${all('back')}" data-act="ifattacked" data-v="back" title="${esc(STANCE_BACK)}">Fall back</button></div>`
    : '';
  el.innerHTML = `<div class="thead">${head}${tally}${stance}${a.nat === g.human ? jointBar(r) + siegeBar(r) + foldBar(r) : ''}</div><div class="cards">${cs.map((c) => (kn(c) === 'exact' ? unitCard(c, mine) : foeCard(c, true))).join('')}</div>`;
  el.classList.add('open');
}

// ---- Orders on the map: the road an army would take, and the march along it ------------------

/** The road the picked army would take to a region: the centres of the regions on the way, from where it stands. */
function routeTo(id: number): { pts: [number, number][]; cost: number; ap: number; attack: boolean } | null {
  let best: { pts: [number, number][]; cost: number; ap: number; attack: boolean } | null = null;
  for (const rc of reaches) {
    const end = rc.map.get(id);
    if (!end || id === rc.from) continue;
    const ids: number[] = [];
    for (let at = id; at >= 0 && at !== rc.from; at = rc.map.get(at)?.prev ?? -1) ids.unshift(at);
    ids.unshift(rc.from);
    // The slowest corps sets the pace: its road is the one shown.
    if (!best || end.cost > best.cost) best = { pts: ids.map((k) => [g.regions[k].x, g.regions[k].y]), cost: end.cost, ap: rc.ap, attack: end.attack };
  }
  return best;
}

/** Draws that road on the map while the cursor is over a region the picked army can reach. */
function drawRoute(id: number): void {
  if (id === routeShown) return;
  routeShown = id;
  const way = id >= 0 && !ui.rail && !ui.ship && !ui.land && ui.tab === 'region' ? routeTo(id) : null;
  if (!way) { $('#route').innerHTML = ''; return; }
  const [ax, ay] = way.pts[way.pts.length - 2], [bx, by] = way.pts[way.pts.length - 1];
  const turn = Math.atan2(by - ay, bx - ax) * 180 / Math.PI;
  const cls = way.attack ? ' strike' : '';
  $('#route').innerHTML = `<path class="road under" d="M${way.pts.map((p) => `${p[0]} ${p[1]}`).join('L')}"/><path class="road${cls}" d="M${way.pts.map((p) => `${p[0]} ${p[1]}`).join('L')}"/>
    <path class="roadhead${cls}" transform="translate(${bx} ${by}) rotate(${turn.toFixed(1)})" d="M4 0L-5 -4.6L-2.6 0L-5 4.6Z"/>`;
}

/** Slides a copy of the piece along the road it took, so the eye can follow the move. */
function march(pts: [number, number][], nat: string, kind: Kind, to: number): void {
  const ghost = $('#ghost');
  const done = (): void => {
    ghost.innerHTML = '';
    marchHide = null;
    const real = document.querySelector<SVGGElement>(`#units .army[data-reg="${to}"][data-nat="${nat}"]`);
    if (real) real.style.visibility = '';
  };
  if (pts.length < 2 || typeof requestAnimationFrame !== 'function') { done(); return; }
  ghost.innerHTML = `<g class="ctr army chosen"><ellipse class="base" cx="11" cy="16.2" rx="13" ry="3.4" fill="${g.nations[nat].color}"/><path class="pole" d="M-.6 -1.5V16.2"/>${standard(nat, kind)}</g>`;
  const piece = ghost.firstElementChild as SVGGElement;
  const legs = pts.length - 1, total = Math.min(720, 190 * legs + 140);
  const t0 = performance.now();
  const token = ++marchToken;
  const step = (now: number): void => {
    if (token !== marchToken) return;
    const f = Math.min(1, (now - t0) / total);
    const e = f < 0.5 ? 2 * f * f : 1 - 2 * (1 - f) * (1 - f);
    const at = e * legs, i = Math.min(legs - 1, Math.floor(at)), u = at - i;
    piece.setAttribute('transform', `translate(${(pts[i][0] + (pts[i + 1][0] - pts[i][0]) * u - 11).toFixed(1)} ${(pts[i][1] + (pts[i + 1][1] - pts[i][1]) * u - 7).toFixed(1)})`);
    if (f < 1) requestAnimationFrame(step); else done();
  };
  step(t0);
  requestAnimationFrame(step);
}

/**
 * Gives the picked corps their order at a region: a march there, or the plan of an attack on it.
 * False if there is no such order to give, and the click means something else.
 */
function orderAt(id: number): boolean {
  const picked = pickedCorps();
  if (!picked.length || id === ui.sel || ui.land || ui.rail || ui.ship || ui.tab === 'build' || ui.tab === 'navy' || ui.tab === 'air') return false;
  const r = g.regions[id];
  if (hostileUnits(g, r, g.human!).length) {
    const near = picked.filter((c) => g.regions[c.reg].nb.includes(id));
    if (!near.length) return false;
    // The card opens on the best attack to be had: everyone the roads will carry, where that beats the army picked.
    const open = openingForce(g, near, r);
    openModal({ kind: 'attack', target: id, chosen: new Set(open.corps.map((c) => c.id)), picked: open.widened ? near.map((c) => c.id) : null });
    return true;
  }
  // A country at peace with the player: sending an army over its frontier is a declaration of war, so ask first.
  if (!atWar(g, g.human!, r.ctrl) && !allied(g, g.human!, r.ctrl)) {
    const on = invasions(g, picked).get(id);
    if (!on) return false;
    openModal({ kind: 'invade', on, target: id });
    return true;
  }
  const h = g.headlines.length;
  const before = g.regions.map((x) => x.ctrl);
  const way = routeTo(id);
  const lead = [...picked].sort((a, b) => KIND[b.kind].atk * b.divs - KIND[a.kind].atk * a.divs)[0];
  const nat = lead.nat, kind = lead.kind;
  let moved = 0;
  for (const c of picked) if (orderMove(g, c, id) > 0) moved++;
  if (!moved) return false;
  ui.unread += g.headlines.length - h;
  ui.flash = g.regions.filter((x, i) => x.ctrl !== before[i]).map((x) => x.id);
  ui.sel = id;
  ui.picked = new Set(picked.filter((c) => c.reg === id && c.ap > 0).map((c) => c.id));
  ui.tab = 'region';
  const arrived = picked.some((c) => c.reg === id);
  marchHide = arrived && way ? { reg: id, nat } : null;
  refresh();
  if (arrived && way) march(way.pts, nat, kind, id);
  // A march that took a fallen country's capital, or brought a nation down, is announced as soon as it is made.
  if (!ui.modal) { const next = nextCard(); if (next) openModal(next); }
  return true;
}

let paneKey = '';

function drawPane(): void {
  const pane = $('#pane');
  // A new subject starts at the top of the panel; a redraw of the same one keeps its place.
  const key = `${ui.tab}:${ui.tab === 'region' ? `${ui.sel}:${ui.sea}` : ''}`;
  if (key !== paneKey) { paneKey = key; pane.scrollTop = 0; }
  if (ui.tab === 'build' && !(g.human && g.nations[g.human].alive)) ui.tab = 'region';
  if (ui.tab === 'spies' && !(g.human && g.nations[g.human].alive && isOpen(g))) ui.tab = 'region';
  if (ui.tab !== 'spies') ui.spySend = false;
  if (ui.tab === 'region') pane.innerHTML = regionPane();
  else if (ui.tab === 'build') pane.innerHTML = buildPane();
  else if (ui.tab === 'research') pane.innerHTML = researchPane();
  else if (ui.tab === 'generals') pane.innerHTML = generalsPane();
  else if (ui.tab === 'honours') pane.innerHTML = honoursPane();
  else if (ui.tab === 'navy') pane.innerHTML = navyPane();
  else if (ui.tab === 'air') pane.innerHTML = airPane();
  else if (ui.tab === 'spies') pane.innerHTML = spiesPane();
  else if (ui.tab === 'reel') pane.innerHTML = reelPane();
  else if (ui.tab === 'battles') pane.innerHTML = battlesPane();
  else pane.innerHTML = powersPane();
}

// ---- What wants the player's attention this month --------------------------------------------

interface Alert { kind: string; text: string; /** Regions to visit in turn, for an alert about places. */ regs?: number[]; tab?: string }

function alertList(): Alert[] {
  const me = g.human;
  if (!me || !humanUp(g) || onEve(g) || passing) return [];
  const n = g.nations[me];
  const out: Alert[] = [];
  const cut = [...new Set(g.corps.filter((c) => c.nat === me && !c.dead && !c.supplied).map((c) => c.reg))];
  if (cut.length) out.push({ kind: 'cut', text: `${cut.length} ${cut.length === 1 ? 'army is' : 'armies are'} cut off from supply`, regs: cut });
  if (n.atWar.length) {
    // An army under orders to dig in is doing its work by staying where it is.
    const idle = g.regions.filter((r) => r.units.some((c) => canOrder(g, c) && c.ap >= c.apMax && !c.fought && !c.digs) && r.nb.some((j) => atWar(g, me, g.regions[j].ctrl))).map((r) => r.id);
    if (idle.length) out.push({ kind: 'front', text: `${idle.length} ${idle.length === 1 ? 'army' : 'armies'} at the front ${idle.length === 1 ? 'has' : 'have'} not moved`, regs: idle });
  }
  const cheapest = Math.min(...Object.values(PRODUCTION.builds).map((b) => b.cost));
  if (n.pp < 0) out.push({ kind: 'debt', text: 'The treasury is in debt: open the ledger' });
  if (Math.floor(n.pp) >= cheapest) out.push({ kind: 'build', text: `${money(n.pp)} in the treasury to spend`, tab: 'build' });
  const depots = stores(g, me);
  for (const b of depots ? [depots.oil, depots.sup] : []) {
    const name = GOOD_NAME[b.good];
    if (b.met < 0.995) out.push({ kind: `short ${b.good}`, text: `Short of ${name.toLowerCase()}: ${Math.round(b.met * 100)}% of the need met` });
    else if (b.net < -0.04 && b.months <= 6) out.push({ kind: `low ${b.good}`, text: `${name} for ${monthsWordOf(b.months)}` });
  }
  // The reserve of men: running short, or spent.
  const mv = manpowerOf(g, me);
  if (mv && (mv.word === 'Short' || mv.word === 'Spent')) out.push({ kind: 'men', text: mv.word === 'Spent' ? 'The reserve of men is spent: no new corps can be raised' : `The reserve of men is running short: ${menWords(mv.men)} left` });
  const stars = g.generals.filter((x) => x.nat === me && x.points > 0 && serving(x)).length;
  if (stars) out.push({ kind: 'generals', text: `${stars} general${stars === 1 ? ' has' : 's have'} skill points to spend`, tab: 'generals' });
  if (isOpen(g)) { const idle = idleTeams(g, me); if (idle) out.push({ kind: 'research', text: idle === 1 ? 'A research team has no work' : `${idle} research teams have no work`, tab: 'research' }); }
  else if (n.lab && teamsFree(n.lab) > 0) out.push({ kind: 'research', text: 'A design team has no work', tab: 'research' });
  const fresh = secretService(g)?.agents.filter((a) => a.atWork && !a.abroad && g.intel.agents.find((x) => x.id === a.id)?.ready === g.turn) ?? [];
  if (fresh.length) out.push({ kind: 'agent', text: fresh.length === 1 ? `Agent ${fresh[0].name} is ready for orders` : `${fresh.length} agents are ready for orders`, tab: 'spies' });
  // Enemy raids since the player's last month: the places hit, to visit in turn.
  const raided = ui.raidsFrom < 0 ? [] : [...new Set(g.airReports.slice(ui.raidsFrom).filter((r) => r.region >= 0 && atWar(g, r.attNat, me) && (r.defNat === me || g.regions[r.region].ctrl === me)).map((r) => r.region))];
  if (raided.length) out.push({ kind: 'raided', text: raided.length === 1 ? `Enemy aircraft raided ${g.regions[raided[0]].name}` : `Enemy aircraft raided ${raided.length} of your provinces`, regs: raided });
  // Sieges: the player's towns shut in, and the towns his armies stand round with an assault still in hand.
  if (siegeRules(g)) {
    const sg = siegesInSight(g);
    const shutIn = sg.filter((v) => v.holder === me);
    if (shutIn.length) out.push({ kind: 'besieged', text: shutIn.length === 1 ? `${shutIn[0].town} is under siege` : `${shutIn.length} of your towns are under siege`, regs: shutIn.map((v) => v.region) });
    const round = sg.filter((v) => atWar(g, me, v.holder) && !whyNotStormTown(g, v.region));
    if (round.length) out.push({ kind: 'siege', text: round.length === 1 ? `Your army stands round ${round[0].town}: storm it or sit it out` : `Your armies stand round ${round.length} towns`, regs: round.map((v) => v.region) });
  }
  // Coasts of the player's shelled from the sea since his last month.
  const shelled = ui.shoreFrom < 0 ? [] : [...new Set(g.shoreReports.slice(ui.shoreFrom).filter((r) => atWar(g, r.nat, me) && (r.defNat === me || g.regions[r.region].ctrl === me || g.regions[r.region].units.some((c) => c.nat === me))).map((r) => r.region))];
  if (shelled.length) out.push({ kind: 'bombarded', text: shelled.length === 1 ? `Enemy warships shelled ${g.regions[shelled[0]].name}` : `Enemy warships shelled ${shelled.length} of your coasts`, regs: shelled });
  // A lane of the player's own that raiders are bleeding with no fleet bound to it.
  if (n.atWar.length && seaWar(g)) {
    const bare = lanesToGuard(g).filter((l) => l.to === me && l.state === 'raided' && l.loss >= 0.05 && !l.escorts.length).sort((a, b) => b.loss - a.loss)[0];
    if (bare) out.push({ kind: 'unescorted', text: `${upFirst(bare.name)} has no escort: ${pc100(bare.loss)} of its cargoes are being sunk`, tab: 'navy' });
  }
  // Trade: an agreement nothing is getting through, or a trading house idle with somebody willing to sign.
  if (tradeRules(g)) {
    const gone = dealsOf(g, me).filter((d) => d.state === 'cut');
    if (gone.length) out.push({ kind: 'tradecut', text: gone.length === 1 ? `Your trade with ${theName(gone[0].a === me ? gone[0].b : gone[0].a)} is cut: nothing is getting through` : `${gone.length} of your trade agreements are cut` });
    else if (housesOf(g, me) > dealsOf(g, me).length && n.pc >= 2 && tradeLedger(g)?.offers.some((o) => o.why === null)) out.push({ kind: 'tradeidle', text: 'A trading house stands idle: sign an agreement' });
  }
  // The government: a cabinet about to fall, a directive new or running out, a cabinet nobody has yet been appointed to.
  const gv = governmentOf(g);
  if (gv?.mine) {
    const d = gv.directive;
    if (gv.secure && gv.trust <= gv.fallsAt) out.push({ kind: 'cabinetdoubt', text: `The war cabinet has no trust left in you. Your office is secure, but it holds back ${Math.round(GOV.capital.lowIncome * 100)}% of your income and ${Math.round(GOV.capital.lowShare * 100)}% of your political capital` });
    else if (gv.low > 0 || gv.trust <= gv.fallsAt) out.push({ kind: 'cabinetfall', text: 'The war cabinet has no trust left in you, and will fall' });
    else if (gv.doubts) out.push({ kind: 'cabinetdoubt', text: `The war cabinet doubts you: it holds back ${Math.round(GOV.capital.lowIncome * 100)}% of your income and ${Math.round(GOV.capital.lowShare * 100)}% of your political capital` });
    if (d && d.left <= 2) out.push({ kind: 'directive', text: `${d.left <= 1 ? 'Last month' : 'Two months left'}: ${d.title}` });
    else if (d && ui.govSeen !== d.id) out.push({ kind: 'directive', text: `A directive from ${d.from === me ? 'the war cabinet' : theName(d.from)}: ${d.title}` });
    if (g.turn - gv.since <= 2 && gv.seats.every((s) => s.minister.id === 'caretaker') && gv.seats.some((s) => s.options.some((o) => o.id !== 'caretaker' && o.why === null))) out.push({ kind: 'cabinetseat', text: 'Your war cabinet is all caretakers: appoint a minister' });
    if (gv.head) out.push({ kind: 'cabinethead', text: `The war cabinet wants ${gv.head.name} dismissed: ${gv.head.left <= 1 ? 'it wants your answer this month' : 'it awaits your answer'}` });
    if (gv.promise && gv.promise.left <= 1) out.push({ kind: 'promise', text: `The last month for your promise: ${gv.promise.title}` });
  }
  const cited = unseen(g).length;
  if (cited) out.push({ kind: 'honours', text: `${cited} honour${cited === 1 ? '' : 's'} earned: read the citation${cited === 1 ? '' : 's'}` });
  // The war between the alliances is decided and the player has not yet said what comes next.
  const act = lastAct(g);
  if (act?.open && !act.peace) out.unshift({ kind: 'lastact', text: 'The war is decided: take the victory, or World Domination' });
  return out;
}

const alertButtons = (): string => alertList().map((a) => `<button class="alert ${a.kind}" data-act="alert" data-kind="${a.kind}">${esc(a.text)}</button>`).join('');

/** Goes where an alert points: the next place on its list, or the tab that deals with it. */
function followAlert(kind: string): void {
  const a = alertList().find((x) => x.kind === kind);
  if (!a) return;
  if (kind === 'honours') { const next = unseen(g)[0]; if (next) openModal({ kind: 'award', id: next.id }); return; }
  if (kind === 'debt') { openModal({ kind: 'ledger' }); return; }
  if (kind === 'tradecut' || kind === 'tradeidle') { openModal({ kind: 'trade' }); return; }
  if (kind === 'lastact') { openModal({ kind: 'lastact' }); return; }
  if (kind === 'directive' || kind === 'cabinetfall' || kind === 'cabinetseat' || kind === 'cabinetdoubt' || kind === 'cabinethead' || kind === 'promise') { openModal({ kind: 'cabinet' }); refresh(); return; }
  if (kind.startsWith('short ') || kind.startsWith('low ')) { openModal({ kind: 'stores', nat: g.human || '' }); return; }
  if (kind === 'men') { openModal({ kind: 'ledger', nat: g.human || '' }); document.querySelector('#card #lmen')?.scrollIntoView({ block: 'start' }); return; }
  if (a.tab) { ui.tab = a.tab as typeof ui.tab; ui.rail = ui.ship = ui.land = ui.rebase = false; refresh(); return; }
  const regs = a.regs ?? [];
  if (!regs.length) return;
  const at = regs.indexOf(ui.sel);
  const id = regs[(at + 1) % regs.length];
  ui.tab = 'region';
  selectRegion(id);
  refresh();
  const svg = $('#map');
  const tall = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.min(ui.view.w, 700);
  glide({ x: g.regions[id].x - w / 2, y: g.regions[id].y - (w * tall) / 2, w });
}

/** The player's short and long campaign, with what each one asks for ticked off as it is done. */
function goalsBlock(): string {
  const me = g.human;
  if (!me || !GOALS[me]) return '';
  const rows = GOAL_KEYS.map((key) => {
    const goal = GOALS[me]![key];
    const st = goalState(g, key);
    if (!goal || st === 'out') return '';
    const when = goalWhen(goal, date);
    return `<div class="goal ${st}"><span class="gk">${key === 'short' ? 'Short campaign' : 'Long campaign'} · ${when}</span><b>${esc(goal.title)}${st === 'won' ? ' <span class="tag good">Won</span>' : st === 'lost' ? ' <span class="tag bad">Lost</span>' : ''}</b><small>${esc(goal.text)}</small><span class="gchecks">${goalChecks(g, me, goal).map((c) => `<span class="${c.done ? 'done' : ''}">${c.done ? '✓' : '○'} ${esc(c.label)}</span>`).join('')}</span></div>`;
  }).join('');
  return rows ? `<h3>Campaign goals</h3>${rows}` : '';
}

// ---- The province: what a region holds, laid out as slots, and what can be raised or built there ----

const GLYPH = {
  factory: '<path d="M1.5 16.5V8l5.5 3V8l5.5 3V8l5 2.6V2.5h3.5v14Z"/>',
  works: '<path d="M3 16.5V5h4v3.2h3V5h4v3.2h3V5h4v11.5Z"/>',
  harbour: '<path class="line" d="M12 6.2V16M7.5 9.3h9M4.5 11.5c.8 3.3 4 4.6 7.5 4.6s6.7-1.3 7.5-4.6"/><circle class="line" cx="12" cy="4.2" r="1.9"/>',
};
/** An anti-aircraft gun on its mount, barrel raised. */
const GUN = '<path d="M4.5 16.5h15v-2.2h-4.6l-1-2.7 7.1-7.5-1.5-1.5-7.7 6.9-2.6-.5-1.6 5.3H4.5Z"/>';
const slotIcon = (body: string, box = '0 0 24 18'): string => `<svg class="sglyph" width="30" height="22" viewBox="${box}">${body}</svg>`;
const pipsOf = (have: number, coming: number, of: number): string => `<span class="lvpips">${Array.from({ length: of }, (_, i) => `<i class="${i < have ? 'on' : i < have + coming ? 'due' : ''}"></i>`).join('')}</span>`;

// ---- The arsenal as the player sees it: one drawing of an offer for the Build tab, one for the province ----

const BOMBER = 'M0-9L1.7-4 11.5-.6V1.6L1.7.9 1.2 6.2 4 8.2V9.4L0 8.4-4 9.4V8.2L-1.2 6.2-1.7.9-11.5 1.6V-.6L-1.7-4ZM-6.6-3.6h1.7v3.2h-1.7ZM4.9-3.6h1.7v3.2H4.9Z';
const LINER = 'M-12.5.6L-10.5 4H10.5L12.5.6H8V-2.2H-7.5V.6ZM-3.6-5.8h2.3v3.6h-2.3ZM1.3-5.8h2.3v3.6H1.3Z';
const FLATTOP = 'M-13-.8L-11.2 4H10.8L13-.8ZM2.6-4h3.8v3.2H2.6Z';
const HULL_OF: Record<string, [string, number]> = { dd: [HULL, 0.78], ca: [HULL, 0.9], bb: [HULL, 1.08], cv: [FLATTOP, 1], ss: [SUB, 0.9], tp: [LINER, 0.95] };
const workGlyph = (item: string): [string, string] => (item === 'factory' ? [GLYPH.factory, '0 0 24 18'] : item === 'yard' ? [GLYPH.harbour, '0 0 24 18'] : item === 'aa' ? [GUN, '0 0 24 18'] : item === 'field' ? [`<path d="${PLANE}"/>`, '-10.5 -9.5 21 19.5'] : [GLYPH.works, '0 0 24 18']);
const months = (n: number): string => `${n} month${n === 1 ? '' : 's'}`;

function offerIcon(o: Offer): string {
  const me = g.human!;
  const c = g.nations[me].color;
  const [group, item] = o.key.split(':');
  if (group === 'corps') return icon({ nat: me, kind: item } as Corps);
  if (group === 'elite') return `<span class="starred">${icon({ nat: me, kind: eliteOf(me)!.kind } as Corps)}<i>★</i></span>`;
  if (group === 'para') return icon({ nat: me, kind: 'inf' } as Corps);
  if (group === 'cadre') return icon({ nat: me, kind: o.kind ?? 'inf' } as Corps);
  // Aircraft and ships are drawn like the corps: a plate in the nation's colour with the silhouette on it.
  const plate = (d: string, k: number, dy = 0): string => `<svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${c}"/><path class="symf" d="${d}" transform="translate(12 ${7.5 + dy}) scale(${k})"/></svg>`;
  if (group === 'air') return plate(item === 'bmr' ? BOMBER : PLANE, item === 'bmr' ? 0.68 : 0.7, -0.2);
  if (group === 'ship') { const [d, k] = HULL_OF[item]; return plate(d, k * 0.8, 0.6); }
  const [body, box] = workGlyph(item);
  return `<svg class="wglyph" width="34" height="24" viewBox="${box}">${body}</svg>`;
}

const offerLabel = (o: Offer): string => (o.key === 'corps:arm' ? `${PRODUCTION.armourName[g.human!] ?? PRODUCTION.armourName._default} corps` : o.label);
/** What an offer would cost to keep each month as things stand: forces cost half at peace, works the same always. */
const offerKeep = (o: Offer): number => o.upkeep * (o.group !== 'works' && g.human && !g.nations[g.human].atWar.length ? BUDGET.upkeep.peace : 1);

/** An offer as a full-width row in the Build tab: what it is, what it does, where it goes, and why not when it cannot be ordered. */
function offerButton(o: Offer, quiet = false): string {
  const ok = humanUp(g) && o.why === null;
  const keep = offerKeep(o);
  const line = [o.size, o.men ? `${menWords(o.men)} men` : '', `ready in ${months(o.months)}`, keep > 0 ? `upkeep ${cash(keep)} a month` : ''].filter(Boolean).join(' · ');
  const facts = line[0].toUpperCase() + line.slice(1);
  // An order the technology tree still holds back opens the tree at the technology it waits on.
  const act = o.tech ? `data-act="tech" data-id="${o.tech}"` : `data-act="arsenal" data-key="${o.key}" ${ok ? '' : 'disabled'}`;
  return `<button class="buy offer${o.tech ? ' held' : ''}" ${act}>${offerIcon(o)}<span class="grow"><b>${o.tech ? LOCK : ''}${esc(offerLabel(o))}</b><small>${facts}</small><small>${esc(o.blurb)}</small>${o.note ? `<small class="where">${esc(o.note)}</small>` : ''}${o.why && !o.poor && !quiet ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span><span class="cost${o.poor ? ' short' : ''}">${money(o.cost)}</span></button>`;
}

/** An offer as a small card in a province: it can be ordered there only if this is where it would go. */
function offerCard(o: Offer, r: Region): string {
  const here = o.at === r.id;
  const ok = humanUp(g) && here && o.why === null;
  const [group, key] = o.key.split(':');
  // The nation's special formation is raised like a corps of its kind.
  const unit = group === 'elite' ? eliteOf(g.human!) : group === 'para' ? { kind: 'inf' as Kind, divs: TECH.units.para.divs } : group === 'cadre' ? { kind: o.kind ?? ('inf' as Kind), divs: parseInt(o.size, 10) || 1 } : null;
  const item = unit ? unit.kind : key;
  const need = here ? '' : group === 'corps' || unit ? `Needs ${needsOf(g, item)} factor${needsOf(g, item) === 1 ? 'y' : 'ies'} here` : group === 'ship' ? `Needs a level ${ARSENAL.ships[item as Hull].yard} shipyard` : o.at < 0 ? 'No aircraft industry' : 'No airfields here';
  const keep = offerKeep(o);
  const count = group === 'ship' && item !== 'tp' ? ARSENAL.ships[item as Hull].count : 0;
  // A card is narrow: the count goes in as a multiple, and the title on hover says the rest.
  const size = unit ? `${unit.divs} divs` : group === 'corps' ? `${specOf(g, item)!.divs} divs` : group === 'ship' ? (count > 1 ? `×${count}` : '') : `×${ARSENAL.air[item as 'ftr' | 'bmr'].count}`;
  const name = offerLabel(o).replace(/, a cadre$/, '').replace(/ corps$/i, '').replace(/ (wing|flotilla)$/i, 's').replace(/^Aircraft carrier$/, 'Carrier');
  const why = o.tech ? 'Needs research' : !here ? need : o.why && !o.poor ? o.why : '';
  return `<button class="rcard${o.tech ? ' held' : ''}" ${o.tech ? `data-act="tech" data-id="${o.tech}"` : `data-act="arsenal" data-key="${o.key}" ${ok ? '' : 'disabled'}`} title="${esc(offerLabel(o))}, ${esc(o.size)}: ${esc(o.blurb)}${o.men ? ` Takes ${menWords(o.men)} men from the reserve.` : ''}${keep > 0 ? ` Upkeep ${cash(keep)} a month.` : ''}${why ? ` ${esc(why)}.` : ''}">${offerIcon(o)}<b>${esc(name)}</b><small>${why ? esc(why) : [size, months(o.months)].filter(Boolean).join(' · ')}</small><span class="cost${here && !o.poor && o.why === null ? '' : ' short'}">${money(o.cost)}</span></button>`;
}

/** A region's holdings as slots: industry, works, harbour, airfields and air defence. In the grand campaign each slot the player can build on is a button. */
function provinceSlots(r: Region): string {
  const me = g.human;
  const mine = !!me && humanUp(g) && isHome(g, me, r);
  const occupied = r.ctrl !== r.owner;
  const facs = r.fac ? `${r.fac} factor${r.fac === 1 ? 'y' : 'ies'}${occupied ? `, worth ${Math.round(PRODUCTION.occupiedIncome * 100)}% under occupation` : ''}` : 'No industry';
  const lv = r.fort?.lv ?? 0;
  const queued = me ? g.nations[me].queue.filter((b) => b.what === 'fort' && b.region === r.id).length : 0;
  const state = `${lv ? `Level ${lv}` : 'No works'}${queued ? `, level ${lv + queued} building` : ''}`;
  const sea = r.port ? 'A port: troops and supplies can come and go by sea' : r.seas.length ? 'A coast with no port' : 'Inland';
  const wings = g.airFleets.filter((f) => f.base === r.id && f.ftr + f.bmr > 0);
  const flying = wings.length ? `${wings.length} air fleet${wings.length > 1 ? 's' : ''}, ${num(wings.reduce((t, f) => t + f.ftr + f.bmr, 0))} aircraft` : 'No air fleet based here';
  const plane = slotIcon(`<path d="${PLANE}"/>`, '-10.5 -9.5 21 19.5');
  if (!budgeted(g)) {
    const industry = `<div class="slot${r.fac ? ' has' : ''}">${slotIcon(GLYPH.factory)}<b>Industry</b><small>${facs}</small></div>`;
    const cost = mine ? fortCost(g, me!, r) : null;
    const have = me ? Math.floor(g.nations[me].pp) : 0;
    const works = cost !== null
      ? `<button class="slot act${lv ? ' has' : ''}" data-act="fortify" ${have >= cost ? '' : 'disabled'} title="Defenders behind works fight far harder. Ready in ${PRODUCTION.fort.months} months, and the works face every direction.">${slotIcon(GLYPH.works)}<b>Works</b>${pipsOf(lv, queued, 3)}<small>${state}</small><small class="do">Build level ${lv + queued + 1} · ${money(cost)}</small></button>`
      : `<div class="slot${lv ? ' has' : ''}">${slotIcon(GLYPH.works)}<b>Works</b>${pipsOf(lv, queued, 3)}<small>${state}${mine && lv + queued >= 3 ? ', as strong as they can be' : ''}</small></div>`;
    const harbour = `<div class="slot${r.port ? ' has' : ''}">${slotIcon(GLYPH.harbour)}<b>Harbour</b><small>${sea}</small></div>`;
    const field = `<div class="slot${wings.length ? ' has' : ''}">${plane}<b>Airfields</b><small>${flying}</small></div>`;
    return `<h3>The province</h3><div class="slots">${industry}${works}${harbour}${field}</div>`;
  }
  // The grand campaign: what the treasury can build here.
  const list = me && g.nations[me].alive && r.ctrl === me ? arsenalOffers(g, r.id) : [];
  const coming = (item: string): number => (me ? g.nations[me].queue.filter((b) => b.what === 'work' && b.item === item && b.region === r.id).length : 0);
  const slot = (key: string, glyph: string, title: string, has: boolean, pips: string, text: string, verb: string, wide = false): string => {
    const o = list.find((x) => x.key === key);
    const cls = `slot${has ? ' has' : ''}${wide ? ' wide' : ''}`;
    if (o && o.at === r.id && (o.why === null || o.poor)) {
      return `<button class="${cls} act" data-act="arsenal" data-key="${key}" ${humanUp(g) && o.why === null ? '' : 'disabled'} title="${esc(o.blurb)} Ready in ${months(o.months)}.${o.upkeep > 0 ? ` Each level costs ${cash(o.upkeep)} a month to man.` : ''}">${glyph}<b>${title}</b>${pips}<small>${text}</small><small class="do">${verb} · ${money(o.cost)}</small></button>`;
    }
    return `<div class="${cls}"${o?.why ? ` title="${esc(o.why)}"` : ''}>${glyph}<b>${title}</b>${pips}<small>${text}</small></div>`;
  };
  const built = factoriesBuilt(g, r.id), facComing = coming('factory');
  const room = r.terrain === 'urban' || r.cap ? ARSENAL.works.factory.most.city : ARSENAL.works.factory.most.town;
  const city = list.some((o) => o.key === 'work:factory' && (o.why === null || o.poor)) || built + facComing > 0;
  const industry = slot('work:factory', slotIcon(GLYPH.factory), 'Industry', r.fac > 0, city ? pipsOf(built, facComing, room) : '', `${facs}${facComing ? `, ${facComing} more building` : ''}`, 'Build a factory', true);
  const works = slot('work:fort', slotIcon(GLYPH.works), 'Works', lv > 0, pipsOf(lv, queued, 3), state, `Build level ${lv + queued + 1}`);
  const yard = yardLevel(g, r), yardComing = coming('yard');
  const YARD = ['', 'builds destroyers, submarines and troopships', 'builds every ship up to cruisers', 'builds every class of ship'];
  const harbour = slot('work:yard', slotIcon(GLYPH.harbour), 'Harbour', r.port, r.port ? pipsOf(yard, yardComing, 3) : '', r.port ? (yard ? `Shipyard level ${yard}: ${YARD[yard]}` : 'A port with no shipyard') : sea, `${yard + yardComing ? 'Enlarge' : 'Build'} the shipyard`);
  const fl = workLevel(g, 'field', r.id), flComing = coming('field');
  const afc = airfieldAt(g, r.id);
  const field = afc
    ? slot('work:field', plane, 'Airfield', !!afc.field, pipsOf(fl, flComing, 3), afc.field ? `${afc.field.word}: serves ${num(afc.field.holds)} aircraft. ${afc.hidden ? 'What flies from here is not known' : afc.rows.length ? `${afc.rows.length} air group${afc.rows.length > 1 ? 's' : ''}, ${num(afc.rows.reduce((t, x) => t + x.aircraft, 0))} aircraft` : 'No air group based here'}` : 'No airfield: no aircraft can be based here. Engineers can lay one out', afc.field ? `Enlarge it by ${num(ARSENAL.works.field.holds)} aircraft` : 'Build an airfield')
    : slot('work:field', plane, 'Airfields', wings.length > 0 || fl > 0, pipsOf(fl, flComing, 3), `${flying}${fl ? `. Level ${fl} fields: room for ${num(AIR.airfields.holds + fl * ARSENAL.works.field.holds)} aircraft` : ''}`, `Build level ${fl + flComing + 1}`);
  const aa = workLevel(g, 'aa', r.id), aaComing = coming('aa');
  const guns = slot('work:aa', slotIcon(GUN), 'Air defence', aa > 0, pipsOf(aa, aaComing, 3), aa ? `Level ${aa} guns: they stop ${Math.round(aa * ARSENAL.works.aa.cut * 100)}% of any bombing here` : 'No anti-aircraft guns', `Build level ${aa + aaComing + 1}`);
  return `<h3>The province</h3><div class="slots">${industry}${works}${harbour}${field}${guns}</div>${afc?.field ? `<div class="acts"><button class="btn small" data-act="airfield" data-id="${r.id}" title="See the air groups based here and give them their orders.">Open the airfield</button></div>` : ''}`;
}

/** What the player can raise, station or lay down in this region, and what is already on order there. */
function provinceMuster(r: Region): string {
  const me = g.human;
  if (!me || !g.nations[me].alive) return '';
  const n = g.nations[me];
  const mine = humanUp(g);
  const list = arsenalOffers(g, r.id);
  const shelf = (k: Offer['group']): string => `<div class="recruit">${list.filter((o) => o.group === k).map((o) => offerCard(o, r)).join('')}</div>`;
  let first = true;
  const head = (title: string): string => { const h = `<h3>${title}${first ? ` <span class="inhand">${money(n.pp)} in the treasury</span>` : ''}</h3>`; first = false; return h; };
  let out = '';
  if (canRaise(g, me, r)) out += head('Raise troops here') + shelf('army');
  if (budgeted(g)) {
    if (canStation(g, me, r)) out += head('Station aircraft here') + shelf('air');
    if (isHome(g, me, r) && yardLevel(g, r) > 0) out += head('Lay down ships here') + shelf('navy');
  }
  const here = n.queue.filter((b) => b.region === r.id).sort((a, c) => a.eta - c.eta);
  if (here.length) {
    out += `<h3>${budgeted(g) ? 'On order here' : 'Forming here'}</h3>`;
    for (const b of here) {
      const left = b.eta - g.turn;
      out += `<div class="order"><span class="grow"><b>${esc(b.name)}</b><small>Ready ${date(b.eta)}${left > 0 ? ` (${left} month${left > 1 ? 's' : ''})` : ''}</small></span>${b.ordered === g.turn && mine ? `<button class="btn small" data-act="cancelbuild" data-id="${b.id}">Cancel</button>` : `<span class="cost">${money(b.cost)}<small>paid</small></span>`}</div>`;
    }
  }
  return out;
}

/** What a new corps will go on costing once it is raised, for the buttons that order one. Nothing in a historical scenario. */
const keepNote = (kind: Kind, divs: number): string => (budgeted(g) ? ` Upkeep ${cash(corpsUpkeep(kind, divs) * (g.human && !g.nations[g.human].atWar.length ? BUDGET.upkeep.peace : 1))} a month.` : '');

/** What a province is worth each month to whoever holds it, and what it would be worth to the player. */
function provinceWorth(r: Region): string {
  const me = g.human;
  const y = provinceYield(g, r);
  if (y.total <= 0) return '';
  const parts: string[] = [];
  if (y.land) parts.push(`land ${cash(y.land)}`);
  if (y.port) parts.push(`harbour ${cash(y.port)}`);
  if (y.oil) parts.push(`oil ${cash(y.oil)}`);
  if (y.fac) parts.push(`${r.fac} factor${r.fac === 1 ? 'y' : 'ies'} ${cash(y.fac)}`);
  if (y.capital) parts.push(`the capital ${cash(y.capital)}`);
  const mine = !!me && r.ctrl === me;
  const less = y.siege ? 'Under siege: it pays nothing until the siege is raised.' : r.ctrl !== r.owner ? 'Occupied ground pays half.' : r.overseas && budgeted(g) ? 'Overseas ground pays half.' : '';
  return `<button class="worth" data-act="ledger" data-nat="${r.ctrl}" title="Open the ledger of ${esc(natName(r.ctrl))}"><span>${mine ? 'Pays into your treasury each month' : `Worth to ${esc(natName(r.ctrl))} each month`}</span><b>${cash(y.total)}</b><small>${parts.join(' · ')}${less ? ` · ${less}` : ''}</small></button>`;
}

/** A town's glyph as a small picture for a panel or a card. */
const townIcon = (t: TownView, px = 26): string => `<svg class="twicon" width="${px}" height="${Math.round(px * 0.78)}" viewBox="-8 -12.6 16 16.6">${townGlyph(t.size, t.rank, t.size !== 'capital' && !!g.regions[t.region].cap)}</svg>`;

/** Five coins for how rich a province is, as many gilded as its rank. */
const rankCoins = (t: TownView): string => `<span class="rankpips" title="How rich this ground is, out of five: it pays its own country ${num(t.worth)} a month.">${Array.from({ length: 5 }, (_, i) => `<i class="${i < t.rank ? 'on' : ''}"></i>`).join('')}</span>`;

/** The chief place of the province under its name: the town, how big it is, and how rich the ground is. */
function townLine(r: Region): string {
  const t = townIn(g, r.id);
  if (!t) return '';
  return `<div class="townline">${townIcon(t)}<span class="grow"><b>${esc(t.name)}</b> · ${esc(t.sizeWord)}${t.name !== t.province ? `, chief town of ${esc(t.province)}` : ''}</span>${rankCoins(t)}</div>`;
}

const PRIZE_SIGN: Record<string, string> = {
  income: `<svg viewBox="-3 -3 6 6">${SIGN.coin}</svg>`, factory: `<svg viewBox="-3 -3 6 6">${SIGN.factory}</svg>`, harbour: `<svg viewBox="-3 -3 6 6">${SIGN.harbour}</svg>`, oil: `<svg viewBox="-3 -3 6 6">${SIGN.oil}</svg>`,
  airfield: `<svg viewBox="-10.5 -9.5 21 19.5"><path class="sgn solid" d="${PLANE}"/></svg>`, rail: '<svg viewBox="-3 -3 6 6"><path class="sgn" d="M-2.4 -1H2.4M-2.4 1H2.4M-1.5 -1.9V1.9M0 -1.9V1.9M1.5 -1.9V1.9"/></svg>',
  vp: `<svg viewBox="-3.4 -3.4 6.8 6.8"><path class="sgn solid" d="${STAR}"/></svg>`, capital: `<svg viewBox="-3.4 -3.4 6.8 6.8"><path class="sgn solid" d="${STAR}"/></svg>`,
  frees: '<svg viewBox="-3 -3 6 6"><circle class="sgn" r="2" stroke-dasharray="1.2 .8"/></svg>', fort: '<svg viewBox="-3.6 -3.2 7.2 6.4"><path class="sgn" d="M-3 2.5V-2.5H-1.2V-.8H1.2V-2.5H3V2.5"/></svg>',
};

/** The heading over a prize: whose it is, or what taking it would mean. */
function prizeHead(p: Prize): string {
  const r = g.regions[p.region];
  if (p.kind === 'yours') return `What ${p.town} gives you`;
  if (p.kind === 'ally') return `What ${p.town} gives ${theName(p.to)}`;
  if (p.kind === 'restored') return `Take ${p.town} and it goes back to ${theName(p.to)}`;
  return g.human && atWar(g, g.human, r.ctrl) ? `The prize: what taking ${p.town} gives you` : `What ${p.town} would be worth to you`;
}

/** What a province gives whoever holds it, and what taking it would bring the player: the sentence, then the items one by one. */
function prizeBlock(r: Region): string {
  const p = prize(g, r.id);
  if (!p || !p.items.length) return '';
  const figure = p.kind === 'taken' ? `+${num(p.income)}` : p.kind === 'restored' ? `−${num(p.incomeNow)}` : num(p.kind === 'ally' ? p.incomeNow : p.income);
  const under = p.kind === 'taken' ? 'a month to you' : p.kind === 'restored' ? `a month to ${esc(natName(r.ctrl))}` : 'a month';
  return `<div class="prize ${p.kind}"><div class="phead"><span>${esc(prizeHead(p))}</span><b>${figure}</b><small>${under}</small></div>
    <p>${esc(p.line)}</p><div class="pzrows">${p.items.map((it) => `<div class="pzrow">${PRIZE_SIGN[it.key] ?? ''}<span><b>${esc(it.label)}</b> ${esc(it.text)}.</span></div>`).join('')}</div></div>`;
}

const PLACE_SIGN: Record<PrizeKind, string> = { oil: PRIZE_SIGN.oil, works: PRIZE_SIGN.factory, airfield: PRIZE_SIGN.airfield, harbour: PRIZE_SIGN.harbour, junction: PRIZE_SIGN.rail };
const WRECK_WORD: Record<PrizeKind, string> = { oil: 'Fire the wells', works: 'Blow up the works', airfield: 'Plough it up', harbour: 'Block the harbour', junction: 'Blow the junction' };
const POLICY_WORD: Record<ScorchPolicy, string> = { never: 'Never', conquered: 'Conquered ground', everywhere: 'Everywhere' };
const wreckTip = (k: PrizeKind): string => (k === 'junction'
  ? 'One corps spends its month on it. No train runs through for either side until engineers relay the line.'
  : `One corps spends its month on it, engineers for choice. Nobody has the use of it for ${PRIZES.wreck[k]} months, you included.`);

/**
 * The places of a province that are prizes of their own (the grand campaign): its oilfield, factory district,
 * airfield, harbour and great rail junction, each with who holds it and how it stands, the order to wreck one the
 * player's troops hold, and what they do if they are driven off: scorched earth.
 */
function placesBlock(r: Region): string {
  const v = prizesOf(g, r.id);
  if (!v) return '';
  const me = g.human, up = humanUp(g);
  const alive = !!me && g.nations[me].alive;
  const under = v.prizes.some((p) => p.holder !== r.ctrl);
  const rows = v.prizes.map((p) => {
    const mine = alive && p.holder === me;
    const why = mine && up ? whyNotWreckPrize(g, r.id, p.kind) : null;
    // Wrecking cannot be undone and costs a corps its month: the order is asked for twice, and a greyed one says why in words.
    const asked = ui.wreckAsk === `${r.id}:${p.kind}` && !why;
    const btn = !(mine && up && p.out <= 0) ? ''
      : asked ? `<span class="wreckask"><button class="btn small danger" data-act="wreck" data-id="${r.id}" data-kind="${p.kind}" title="${esc(wreckTip(p.kind))}">Yes, ${WRECK_WORD[p.kind].toLowerCase()}</button><button class="btn small" data-act="wreckno">Keep it</button></span>`
      : `<button class="btn small danger" data-act="wreckask" data-id="${r.id}" data-kind="${p.kind}" ${why ? `disabled title="${esc(why)}"` : `title="${esc(wreckTip(p.kind))}"`}>${WRECK_WORD[p.kind]}</button>`;
    const said = mine && up && p.out <= 0 ? (asked ? `<small class="whynot ask">${esc(wreckTip(p.kind))} It cannot be undone.</small>` : why ? `<small class="whynot">${esc(why)}</small>` : '') : '';
    const cls = p.out > 0 ? ' down' : p.holder !== r.ctrl ? ' seized' : '';
    const where = p.inside ? 'Inside the defences' : 'Outside the walls';
    return `<div class="rrow place${cls}"${p.holder !== r.ctrl ? ` style="border-left-color:${g.nations[p.holder].color}"` : ''}><span>${PLACE_SIGN[p.kind]}<b>${esc(p.name)}</b> <i>${where}</i></span><small>${esc(p.detail[0].toUpperCase() + p.detail.slice(1))}. ${esc(p.state)}</small>${btn}${said}</div>`;
  }).join('');
  // Scorched earth: the player's order for this province, where he holds the town or the country round it.
  let order = '';
  if (alive && (r.ctrl === me || v.prizes.some((p) => p.holder === me))) {
    const seg = (val: number, label: string, tip: string): string => `<button class="btn small${v.order === val ? ' on' : ''}" data-act="burnorder" data-id="${r.id}" data-v="${val}" ${up ? `title="${esc(tip)}"` : 'disabled title="It is not your month."'}>${label}</button>`;
    const pol = (p: ScorchPolicy, tip: string): string => `<button class="btn small${v.policy === p ? ' on' : ''}" data-act="scorch" data-v="${p}" ${up ? `title="${esc(tip)}"` : 'disabled title="It is not your month."'}>${POLICY_WORD[p]}</button>`;
    const odds = Math.round(PRIZES.burn.chance * 100);
    order = `<div class="rrow scorch${v.burns ? ' burns' : ''}"><span><b>If your troops are driven off ${esc(r.name)}</b></span>
      <small title="${esc(`Each place burns about ${odds} times in 100, less often if nobody is left near to do it or the enemy comes on fast. A burned place is out of use for ${PRIZES.burn.months.airfield} to ${PRIZES.burn.months.oil} months, whoever holds it. The railway is torn up on the same chance when the province falls: no train runs there for about ${PRIZES.burn.months.rail} months, unless engineers relay it sooner, and the enemy's supply is the thinner for it.`)}">${v.burns ? `They burn what they hold as they go, about ${odds} times in 100 for each place, and tear up the railway behind them.` : 'They leave everything whole, for the enemy to use.'} ${v.order ? 'That is your order for this province.' : `That follows your policy: ${POLICY_WORD[v.policy].toLowerCase()}.`}</small>
      <div class="acts">${seg(1, 'Burn it', 'Scorched earth here, whatever the policy says.')}${seg(-1, 'Leave it whole', 'Nothing is burned here, whatever the policy says.')}${seg(0, 'By policy', 'This province follows your policy.')}<button class="btn small quiet" data-act="polopen" title="The scorched earth policy for every province without an order of its own.">${ui.polOpen ? 'Hide the policy' : `Policy: ${POLICY_WORD[v.policy].toLowerCase()}`}</button></div>
      ${ui.polOpen ? `<small class="polline">Scorched earth policy, for every province without an order of its own:</small>
      <div class="acts">${pol('never', 'Your troops burn nothing they leave.')}${pol('conquered', 'Your troops burn what they leave on ground that is not your own or an ally\'s.')}${pol('everywhere', 'Your troops burn what they leave, at home too.')}</div>` : ''}</div>`;
  }
  return `<div class="rivers places"><div class="placehead"><b>Places of their own</b><small>${under ? `The army round ${esc(townName(r))} holds what lies outside its walls, and has the use of it after a month.` : `What lies outside the walls of ${esc(townName(r))} goes with the country round it: an army that wins the field holds it, though the town holds.`}</small></div>${rows}${order}</div>`;
}

/**
 * Prizes on the map (the grand campaign): a flame over a town where one is wrecked and still out of use, and a
 * pennant in the colour of the army that holds prizes outside a town it has not taken.
 */
/** The directive that names a town, if the player's cabinet has one standing. */
function directiveHere(): NonNullable<GovView['directive']> | null {
  const v = g.human && isOpen(g) ? governmentOf(g) : null;
  const d = v?.mine ? v.directive : null;
  return d && d.region !== undefined ? d : null;
}

/** The town the cabinet's directive names: a brass ring round it and a seal with the months left, so the order is in sight on the map. */
function directiveMark(): string {
  const d = directiveHere();
  const leg = $('#legdir') as HTMLElement;
  const t = d ? towns(g)[d.region!] : null;
  leg.hidden = !t;
  // The town the player promised of his own accord carries his own seal: round, and in another colour.
  const p = promiseHere(), pt = p ? towns(g)[p.region] : null;
  ($('#legvow') as HTMLElement).hidden = !pt;
  const mine = p && pt ? `<g class="dirmark vow${p.left <= 1 ? ' late' : ''}" transform="translate(${pt.x} ${pt.y})"><circle r="9.4"/><g transform="translate(0 -15.5)"><circle class="dseal" r="5.2"/><text y="2.2">${p.left}</text></g></g>` : '';
  if (!d || !t) return mine;
  return `<g class="dirmark${d.left <= 2 ? ' late' : ''}" transform="translate(${t.x} ${t.y})"><circle r="9.4"/><g transform="translate(0 -15.5)"><path class="dseal" d="M-5.6 -4.4H5.6V2.4L0 5.4-5.6 2.4Z"/><text y="1.7">${d.left}</text></g></g>${mine}`;
}

/** The player's own promise to his cabinet, if one stands. */
function promiseHere(): NonNullable<GovView['promise']> | null {
  const v = g.human && isOpen(g) ? governmentOf(g) : null;
  return v?.mine ? v.promise : null;
}

/** In the province the player promised: his word and how it stands. In an enemy province he could promise: the way to the cabinet room. */
function vowLine(r: Region): string {
  if (!g.human || !isOpen(g) || cabinetIsOff(g)) return '';
  const p = promiseHere();
  if (p && p.region === r.id) return `<button class="dirline vow${p.left <= 1 ? ' late' : ''}" data-act="cabinet" title="Open the cabinet room"><i class="dseal">${p.left}</i><span class="grow"><b>Your word to the cabinet: ${esc(p.title)}</b><small>${p.left === 1 ? 'This is the last month' : `${p.left} months left`}. ${esc(p.progress)}</small></span></button>`;
  if (p || !humanUp(g) || whyNotOrderPromise(g, r.id) !== null) return '';
  return `<button class="btn small quiet vowbtn" data-act="govpromise" data-id="${r.id}" title="Go to your war cabinet and promise it ${esc(townName(r))} by a day you name. The odds today set what keeping your word earns and what breaking it costs.">Promise ${esc(townName(r))} to the cabinet</button>`;
}

/** One line in the province the directive names: what is asked, by when, and how it stands. A click opens the cabinet room. */
function dirLine(r: Region): string {
  const d = directiveHere();
  if (!d || d.region !== r.id) return '';
  return `<button class="dirline${d.left <= 2 ? ' late' : ''}" data-act="cabinet" title="Open the cabinet room"><i class="dseal">${d.left}</i><span class="grow"><b>${d.from === g.human ? 'The war cabinet directs' : `${esc(d.by)} asks`}: ${esc(d.title)}</b><small>${d.left === 1 ? 'This is the last month' : `${d.left} months left`}, to the end of ${date(d.due)}. ${esc(d.progress)}</small></span></button>`;
}

function placeMarks(): string {
  const leg = $('#legprize') as HTMLElement;
  const marks = prizesOnMap(g);
  let out = '', flames = 0;
  const list = marks.length ? towns(g) : [];
  for (const m of marks) {
    const t = list[m.region];
    // The airfield alone is not worth a pennant: nearly every province has one.
    const worth = m.seized.filter((k) => k !== 'airfield');
    if (m.by && worth.length) out += `<g class="pzflag" transform="translate(${t.x + 11.5} ${t.y + 6})"><path class="pole" d="M0 4.6V-4.8"/><path d="M0 -4.8L6.2 -2.9 0 -1Z" fill="${g.nations[m.by].color}"/></g>`;
    if (m.burning.length) { flames++; out += `<g class="pzfire" transform="translate(${t.x - 10.5} ${t.y - 8.5})"><path class="o" d="${FLAME}"/><path class="i" d="M0 4.3C-1.7 4.3-2.3 3-2 1.9-1.6.8-.5.3 0-1.2 1.1.2 2.2 1 2.1 2.4 2 3.5 1.2 4.3 0 4.3Z"/></g>`; }
  }
  leg.hidden = !flames;
  return out;
}
const FLAME = 'M0 5C-3 5-4.6 3-4.2 .6-3.9-1.2-2.5-2-2.2-4-1-3.2-.6-2.3-.4-1.2.6-2.6.7-4.4-.2-6 2.6-4.8 4.6-2.2 4.4 .9 4.2 3.3 2.6 5 0 5Z';

/** The richest enemy ground the player could strike at next, each a button that shows it on the map. */
function prizesBlock(): string {
  if (!g.human || !g.nations[g.human].alive || !g.nations[g.human].atWar.length) return '';
  const list = prizesNear(g, 5);
  if (!list.length) return '';
  return `<h3>The richest prizes within reach</h3><p class="sub" style="margin:0 0 7px">Enemy ground beside your own or beside your armies, the most valuable first. Click one to see it on the map.</p><div class="targets">${list.map((p) => {
    const t = townIn(g, p.region)!;
    const r = g.regions[p.region];
    const marks = [p.frees ? `frees ${esc(g.nations[p.frees].name)}` : p.capitalOf ? `capital of ${esc(theName(p.capitalOf))}` : '', p.factories ? `${p.factories} factor${p.factories === 1 ? 'y' : 'ies'}` : '', p.oil > 0 || r.oil ? 'oilfield' : '', p.harbour ? 'harbour' : '', p.vp ? `${p.vp} victory point${p.vp === 1 ? '' : 's'}` : ''].filter(Boolean);
    return `<button class="tgt" data-act="showmap" data-id="${p.region}">${townIcon(t, 24)}<span class="grow"><b>${esc(t.name)}</b>${t.name !== t.province ? ` <i>${esc(t.province)}</i>` : ''}<small>${chip(r.ctrl)} ${marks.join(' · ') || esc(t.sizeWord)}</small></span><span class="cost">${p.kind === 'taken' ? `+${num(p.income)}<small>a month</small>` : `−${num(p.incomeNow)}<small>to them</small>`}</span></button>`;
  }).join('')}</div>`;
}

/** The capital of a fallen country: who it fell to, and what taking it would do. */
function freeNote(r: Region): string {
  const f = fallenCapitalAt(g, r.id);
  if (!f) return '';
  return `<div class="note${f.yours ? ' good' : ''} freenote">${flagIcon(f.nat, g.nations[f.nat].color, 30)}<span><b>The capital of ${esc(theName(f.nat))}, which fell to ${esc(theName(f.fell.to))} in ${date(f.fell.turn)}.</b> ${esc(f.note)}</span></div>`;
}

// ---- Cities, towns and sieges (the grand campaign) ------------------------------------------

/** A shield: the mark of troops inside a town. */
const SHIELD = 'M-4.2 -4.6H4.2V.6C4.2 3.4 0 5.2 0 5.2C0 5.2 -4.2 3.4 -4.2 .6Z';
/** A ring of siege lines round a town, as a small picture for a panel or the legend. */
const ringIcon = (color: string, px = 22): string => `<svg class="sgicon" width="${px}" height="${px}" viewBox="-12 -12 24 24"><circle r="9.6" fill="none" stroke="#15120f" stroke-width="4"/><circle r="9.6" fill="none" stroke="${color}" stroke-width="2.2" stroke-dasharray="4 2.6"/><path d="M-3.4 3V-2.6H-1.5V-.9H1.5V-2.6H3.4V3Z" fill="#efe6cf" stroke="#15120f" stroke-width=".9" stroke-linejoin="round"/></svg>`;

/**
 * The sieges on the map. The country round a besieged town is tinted in the colour of the army that holds it, the
 * town keeps a disc in its holder's colour and is ringed by the siege lines, and a town with troops inside carries
 * a shield with their number. `under` goes beneath the towns, `over` above them.
 */
function siegeMarks(): { under: string; over: string } {
  const leg = $('#legsiege') as HTMLElement;
  leg.hidden = true;
  if (!siegeRules(g)) return { under: '', over: '' };
  const list = towns(g);
  let under = '', over = '';
  const sg = siegesInSight(g).filter((v) => v.by !== null);
  for (const v of sg) {
    const r = g.regions[v.region], t = list[v.region];
    const by = g.nations[v.by!].color;
    under += `<path class="sieged" d="${pathOf(r.polys)}" fill="${by}"/><circle class="sgdisc" cx="${t.x}" cy="${t.y}" r="7.6" fill="${g.nations[v.holder].color}"/>`;
    over += `<g transform="translate(${t.x} ${t.y})"><circle class="sgo" r="10.4"/><circle class="sgi" r="10.4" stroke="${by}"/></g>`;
  }
  for (const r of g.regions) {
    if (!r.garr || !r.garr.length) continue;
    const v = townSiege(g, r.id);
    if (!v || v.inside <= 0) continue;
    const t = list[r.id];
    over += `<g class="garrb" transform="translate(${t.x + 9.5} ${t.y - 9})"><path d="${SHIELD}" fill="${g.nations[r.ctrl].color}"/><text y="2.5">${v.inside}</text></g>`;
  }
  leg.hidden = !sg.length;
  return { under, over };
}

/** On the army tray: the siege this army is laying, with the assault one click away; or the town it could go into. */
/** Two worn corps of the player's standing together (the grand campaign): the order that makes one corps of them. */
function foldBar(r: Region): string {
  const vs = foldChoicesAt(g, r.id);
  if (!vs.length) return '';
  const names = [...new Set(vs.map((v) => v.keepName))];
  return `<div class="jointbar foldbar"><small>${names.length === 2 ? `${esc(names[0])} and ${esc(names[1])} are` : `${names.length} corps here are`} worn thin</small><button class="btn small" data-act="foldcard" data-id="${r.id}" title="The men of both fill one corps, the better of the two generals commands it, and the corps that is emptied goes home as a cadre, to be raised again from the arsenal at ${Math.round(MANPOWER.fold.price * 100)}% of the price of a new corps. Nothing is done until you have seen what it makes of them.">Fold two worn corps into one</button></div>`;
}

/** The card that folds two worn corps into one: every pair that can be folded, either way round, and what each makes of them. */
function foldCard(reg: number): string {
  const r = g.regions[reg];
  const vs = foldChoicesAt(g, reg);
  const pct = (x: number): string => `${Math.round(x * 100)}%`;
  const row = (v: (typeof vs)[number]): string => {
    const bits = [
      `<b>${num(v.men)} men</b> in one corps at ${pct(v.str)} of its strength, ${esc(grade(v.xp).toLowerCase())}.`,
      v.general ? `${esc(v.general)} commands${v.stepsDown ? `; ${esc(v.stepsDown)} goes to the list of generals waiting for a corps` : ''}.` : 'No general commands it.',
      v.back > 0 ? `${num(v.back)} men over go back to the reserve.` : '',
      v.works !== undefined ? `It takes over the fieldworks ${esc(v.goneName)} stands in.` : '',
      `${esc(v.goneName)} goes home as a cadre: it can be raised again from the arsenal for ${money(v.cadreCost)} from the treasury and ${num(v.cadreMen)} men from the reserve, in ${monthsWord(v.cadreMonths)}.`,
    ].filter(Boolean).join(' ');
    return `<div class="foldopt"><span class="grow"><b class="foldkeep">Keep ${esc(v.keepName)}</b><small>${bits}</small></span><button class="btn small go" data-act="fold" data-keep="${v.keep}" data-gone="${v.gone}" title="${esc(v.keepName)} takes in the men of ${esc(v.goneName)}. It cannot be undone.">Fold into ${esc(v.keepName)}</button></div>`;
  };
  const corps = (id: number): Corps | undefined => g.corps.find((c) => c.id === id);
  const pairs = new Map<string, typeof vs>();
  for (const v of vs) { const k = [v.keep, v.gone].sort((a, b) => a - b).join(':'); (pairs.get(k) ?? pairs.set(k, []).get(k)!).push(v); }
  const body = [...pairs.values()].map((p) => {
    const a = corps(p[0].keep), b = corps(p[0].gone);
    const head = a && b ? `${esc(a.name)} (${num(men(a))} men, ${pct(a.str)}) and ${esc(b.name)} (${num(men(b))} men, ${pct(b.str)})` : '';
    return `<div class="foldpair"><h3>${head}</h3>${p.map(row).join('')}</div>`;
  }).join('');
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The army at ${esc(r.name)} · ${date(g.turn)}</div><h2>Fold two worn corps into one</h2></header>
    <div class="body"><p class="sub" style="margin:0 0 10px">Two corps of the same kind, both under ${Math.round(MANPOWER.fold.worn * 100)}% of their strength, can be made into one that can fight. The corps that is kept loses a little of its order while the men shake down together, and has the fewer moves of the two left this month. The other leaves the map with its name, its record and its honours, and waits in the arsenal to be raised again.</p>${body || '<div class="empty">No two corps here can be folded together now.</div>'}</div>
    <footer><button class="btn" data-act="close">Leave them as they are</button></footer>`;
}

/** Fieldworks standing empty in a province of the player's side (the grand campaign): what is there, and who can use it. */
function leftWorksNote(r: Region): string {
  const v = fieldworksLeft(g, r.id);
  if (!v) return '';
  const m = Math.round(v.months * 10) / 10;
  return `<div class="note leftworks"><b>Fieldworks stand empty here: room for ${v.corps === 1 ? 'one corps' : `${v.corps} corps`}, ${m === 1 ? 'a month' : `${m} months`} of work.</b> A corps of yours that marches or rides in takes them over at once and fights from them, as if it had dug them itself. Empty, they lose a month of work every month.</div>`;
}

/** The row on the attack card that offers to soften the line first: a probe by one corps, or the bombers on its fieldworks. */
function softenRow(target: Region, chosen: Corps[]): string {
  const s = softenOptions(g, chosen, target);
  const done = ui.modal?.probe as { name: string; found: string[]; wear: number; lost: number } | undefined;
  if (!s || (s.works <= 0 && s.probe.fresh <= 0 && !done)) return '';
  const mo = (m: number): string => { const v = Math.round(m * 10) / 10; return v === 1 ? 'a month' : `${v} months`; };
  const gains = [s.probe.fresh ? `name the ${s.probe.fresh === 1 ? 'corps' : `${s.probe.fresh} corps`} the staff cannot see` : '', s.probe.wear > 0 ? `take ${mo(s.probe.wear)} off their fieldworks` : ''].filter(Boolean).join(' and ');
  const probeText = s.probe.why ?? (gains ? `It would ${gains}, for about ${num(s.probe.men)} men and ${s.probe.ap} of its moves. It is not a battle: nothing changes hands.` : 'There is nothing more a probe could find here.');
  const probe = `<div class="softopt"><button class="btn small" data-act="probe" data-id="${s.probe.corps}" ${s.probe.why || !gains ? 'disabled' : ''}>Probe${s.probe.name ? ` with ${esc(s.probe.name)}` : ' the line'}</button><small>${esc(probeText)}</small></div>`;
  const bomb = s.bomb ? `<div class="softopt"><button class="btn small" data-act="bombline" ${s.bomb.why ? 'disabled' : ''}>Bomb the line</button><small>${esc(s.bomb.why ?? `${s.bomb.groups === 1 ? 'One air group' : `${s.bomb.groups} air groups`} with ${num(s.bomb.bombers)} bombers can reach it, and should take about ${mo(s.bomb.wear)} off the fieldworks. ${s.bomb.verdict}. The planner shows what will meet them.`)}</small></div>` : '';
  const found = done ? `<small class="softdone">${esc(done.name)} has probed this line: it found ${esc(listOf(done.found))}${done.wear > 0 ? `, and their fieldworks are ${mo(done.wear)} the worse` : ''}. It cost ${num(done.lost)} men.</small>` : '';
  const head = s.works > 0 ? `The defenders stand in fieldworks of ${mo(s.works)}${s.works >= DEFENCE.entrench.months ? ', finished' : ''}.` : 'The defenders are not dug in.';
  return `<div class="note soften"><span><b>Soften the line first.</b> ${head}</span>${found}${probe}${bomb}</div>`;
}

function siegeBar(r: Region): string {
  const me = g.human;
  const v = me ? townSiege(g, r.id) : null;
  if (!v || !me) return '';
  const up = humanUp(g);
  if (v.by && atWar(g, me, r.ctrl) && stormCandidates(g, r.id).length) {
    const why = whyNotStormTown(g, r.id);
    return `<div class="jointbar siegebar"><small title="${esc(v.lines.join(' '))}">${ringIcon(g.nations[me].color, 20)}Siege of ${esc(v.town)} · walls +${Math.round((v.bonus - 1) * 100)}%${v.broken > 0 ? `, ${Math.round(v.broken * 100)}% broken` : ''} · ${v.bySea ? 'fed by sea' : v.closed ? (v.left > 0 ? `stores for ${monthsWord(v.left)}` : 'stores gone') : 'ring not closed'}</small><button class="btn small go" data-act="storm" data-id="${r.id}" ${why ? `disabled title="${esc(why)}"` : 'title="See the odds of an assault before anything is ordered."'}>Storm the town</button></div>`;
  }
  if (r.ctrl === me && v.by && v.inside > 0) {
    const why = whyNotSallyOut(g, r.id);
    return `<div class="jointbar siegebar"><small>${ringIcon(g.nations[v.by].color, 20)}${esc(v.town)} is under siege</small><button class="btn small go" data-act="sally" data-id="${r.id}" ${why ? `disabled title="${esc(why)}"` : ''}>Sally out</button></div>`;
  }
  if (r.ctrl === me && up && !v.by && v.inside < v.room) {
    const picked = pickedCorps().filter((c) => c.reg === r.id && c.nat === me && !whyNotGarrisonTown(g, c));
    if (picked.length === 1) return `<div class="jointbar siegebar"><small>${esc(v.town)}: walls +${Math.round((v.bonus - 1) * 100)}%, room for ${v.room - v.inside} more corps</small><button class="btn small" data-act="garrison" data-id="${picked[0].id}" title="${esc(picked[0].name)} goes inside the walls. It does nothing more this month.">Into the town</button></div>`;
  }
  return '';
}

/** A corps inside a town, as a row: what it is, how it stands, and the order that brings it out. */
function garrisonRow(c: Corps, order: boolean): string {
  const why = order ? whyNotLeaveTown(g, c) : null;
  const btn = order ? `<button class="btn small" data-act="takefield" data-id="${c.id}" ${why ? `disabled title="${esc(why)}"` : 'title="Bring it out of the town into the country round it. Outside it marches and fights like any army, without the walls."'}>Take the field</button>` : '';
  return `<div class="sgrow">${icon(c)}<span class="grow"><b>${esc(c.name)}</b><small>${c.nat !== g.human ? `${esc(g.nations[c.nat].adj)} · ` : ''}${c.divs} ${KIND[c.kind].label.toLowerCase()} div${c.divs > 1 ? 's' : ''} · strength ${Math.round(c.str * 100)}% · rest ${Math.round(c.org * 100)}%${c.supplied ? '' : ' · hungry'}</small></span>${btn}</div>`;
}

/**
 * The town of a province as a fortress (the grand campaign): what it adds to its defenders, who is inside, how its
 * siege stands, and the orders: into the town and out of it, the assault, the sortie, and softening it first.
 */
/** A province cut off from the rest of its country: what its troops live on, and for how long. */
function pocketNote(r: Region): string {
  const v = pocketAt(g, r.id);
  if (!v) return '';
  const mine = v.nat === g.human;
  const foe = g.human !== null && atWar(g, g.human, v.nat);
  const tail = mine ? (v.spent ? ' Break the ring from outside, or attack out of it while the corps can still fight.' : ' Break the ring from outside before the stores are gone.')
    : foe ? (v.spent ? ' An attack now meets hungry troops whose works count for nothing.' : ' Wait, and they starve. Attack, and you meet them at full strength.') : '';
  return `<div class="note ${v.spent ? 'bad' : 'warn'} pocketnote"><b>${v.spent ? 'Cut off, and the stores are spent' : `Cut off since ${date(v.since)}: stores for ${monthsWord(v.left)}`}.</b> ${esc(v.text)}${tail}</div>`;
}

function siegeBlock(r: Region): string {
  const v = townSiege(g, r.id);
  if (!v) return '';
  const me = g.human, up = humanUp(g);
  const alive = !!me && g.nations[me].alive;
  const mine = alive && r.ctrl === me, foe = alive && atWar(g, me!, r.ctrl);
  const pct = Math.round((v.bonus - 1) * 100);
  const fact = (label: string, value: string, title: string, cls = ''): string => `<span class="sgfact${cls}" title="${esc(title)}"><small>${label}</small><b>${value}</b></span>`;
  const facts = fact('Walls', `+${pct}%`, `Troops inside ${v.town} fight ${pct}% stronger than in the open.${v.broken > 0 ? ` Bombardment has broken down ${Math.round(v.broken * 100)}% of its works.` : ''} Bombs, ships' guns and siege artillery break the works down, and engineers blow in the barricades.`, v.broken > 0 ? ' hurt' : '')
    + fact('Garrison', `${v.inside < 0 ? '?' : v.inside} of ${v.room}`, `${v.town} has room for ${v.room === 1 ? 'one corps' : `${v.room} corps`} inside.${v.inside < 0 ? ' How many are inside is not known: look with an agent or a reconnaissance flight.' : ''}`)
    + fact('Town guard', v.guard > 0 ? `${v.guard.toFixed(1)} div` : 'None', v.guard > 0
      ? `Depot troops, police and militia who hold ${v.town} while no army does. ${SIEGE.summonsNot.includes(v.size) ? `A ${v.size === 'capital' ? 'capital' : 'city'} never opens its gates at a summons: its guard has to be beaten in the streets or starved out.` : `A town held only by its guard opens its gates to a force with odds of ${SIEGE.summons} to 1 or better.`}`
      : `${v.town} has no guard of its own${r.ctrl !== r.owner ? ': it is held by a foreign army' : ''}. With no troops inside, it falls to whoever wins the country round it.`)
    + fact('Stores', v.by ? (v.bySea ? 'By sea' : `${v.left} of ${v.stores} mo`) : `${v.stores} mo`, v.by && v.bySea ? 'Ships still reach the harbour: the town cannot be starved while they do.' : `Closely shut in, ${v.town} feeds its defenders for ${monthsWord(v.stores)}. After that they go hungry, and a hungry garrison gives in.`, v.by && !v.bySea && v.left === 0 ? ' hurt' : '');
  let head: string;
  if (v.by) {
    const ours = alive && allied(g, me!, v.by);
    head = `<div class="sghead">${ringIcon(g.nations[v.by].color, 30)}<span class="grow"><b>${esc(v.town)} is under siege</b><small>${ours && v.by === me ? 'Your army holds' : `${esc(g.nations[v.by].adj)} troops hold`} the country round it${v.since >= 0 ? ` since ${date(v.since)}` : ''}. ${mine ? 'You still hold the town, and the province with it.' : `${esc(g.nations[v.holder].adj)} troops still hold the town, and the province with it.`}</small></span></div>`;
  } else {
    const say = foe ? `Beat the army in the field and you hold the country round ${esc(v.town)}. The province is yours only when the town itself is stormed, starved out or opens its gates.`
      : mine ? `Whoever holds ${esc(v.town)} holds the province. Troops you put inside fight from behind its walls, and an enemy who beats your army in the field must still take the town.`
      : `Whoever holds ${esc(v.town)} holds the province.`;
    head = `<div class="sghead plain"><span class="grow"><b>The key to ${esc(r.name)}</b><small>${say}</small></span></div>`;
  }
  const lines = v.by ? v.lines.slice(1).map((l) => `<p>${esc(l)}</p>`).join('') : '';
  // Who is inside: the player's own corps one by one, anyone else's as an army.
  let inside = '';
  if (mine) inside += myGarrison(g, r.id).map((c) => garrisonRow(c, up)).join('');
  for (const a of garrisonsAt(g, r.id)) {
    if (mine && a.nat === me) continue;
    inside += a.known === 'count' || !a.corps.length
      ? `<div class="sgrow">${flagIcon(a.nat, g.nations[a.nat].color, 26)}<span class="grow"><b>${esc(a.name)}</b><small>${a.corpsCount} corps, not identified</small></span></div>`
      : `<div class="sgrow">${flagIcon(a.nat, g.nations[a.nat].color, 26)}<span class="grow"><b>${esc(a.name)}</b><small>${esc(a.line)} · ${num(a.men)} men${a.known === 'partial' && a.unidentified ? ` · ${a.unidentified} corps not identified` : ''}</small></span></div>`;
  }
  if (v.inside < 0) inside += `<div class="sgrow dim"><span class="grow"><small>What is inside ${esc(v.town)} is not known. An agent, a reconnaissance flight or a battle will tell you.</small></span></div>`;
  // The orders.
  let acts = '';
  if (mine && up) {
    const field = r.units.filter((c) => c.nat === me && canOrder(g, c) && c.kind !== 'fort' && c.kind !== 'eng');
    const into = field.slice(0, 6).map((c) => {
      const why = whyNotGarrisonTown(g, c);
      return `<button class="btn small" data-act="garrison" data-id="${c.id}" ${why ? `disabled title="${esc(why)}"` : `title="${esc(c.name)} goes inside the walls and fights ${pct}% stronger there. It does nothing more this month."`}>Into the town: ${esc(c.name)}</button>`;
    }).join('');
    if (into) acts += `<div class="acts">${into}</div>`;
    if (v.by && v.inside > 0) {
      const why = whyNotSallyOut(g, r.id);
      acts += `<div class="acts"><button class="btn small go" data-act="sally" data-id="${r.id}" ${why ? `disabled title="${esc(why)}"` : 'title="The garrison comes out against the army round the town. Win, and the siege is raised."'}>Sally out</button></div><p class="sghint">To raise the siege from outside, attack the army round ${esc(v.town)} from a neighbouring province. The garrison joins in.</p>`;
    } else if (v.by) acts += `<p class="sghint">To raise the siege, attack the army round ${esc(v.town)} from a neighbouring province.</p>`;
  } else if (foe && v.by && stormCandidates(g, r.id).length) {
    const why = whyNotStormTown(g, r.id);
    const air = airWar(g) && up && raidTargetsFor(g, 'town', -1).some((x) => !x.sea && x.id === r.id);
    acts += `<div class="acts"><button class="btn small go" data-act="storm" data-id="${r.id}" ${why ? `disabled title="${esc(why)}"` : 'title="Send the army round the town against its walls. You see the odds before anything is ordered."'}>Storm the town</button>${air ? `<button class="btn small" data-act="raidtown" data-id="${r.id}" title="Bombers break down the works and shake the garrison, so the assault goes in easier.">Bomb it first</button>` : ''}</div>
      <p class="sghint">Three ways in. <b>Storm it</b>, once a month: costly, and quick if it works. <b>Soften it</b> with bombers${r.seas.length ? ", a fleet's guns" : ''} and artillery, then storm. Or <b>starve it</b>: keep an army round it${v.bySea ? ' and close the sea' : ''} until its stores are gone.</p>`;
  } else if (foe && v.by && alive && allied(g, me!, v.by)) acts += `<p class="sghint">Your ally's army stands round ${esc(v.town)}. March a corps in beside it to take a hand in the siege.</p>`;
  return `<div class="siegebox${v.by ? ' under' : ''}">${head}<div class="sgfacts">${facts}</div>${lines ? `<div class="sglines">${lines}</div>` : ''}${inside}${acts}</div>`;
}

/** What the staff says of a battle before it is ordered: the verdict, the balance (a band where the enemy is not fully known) and the Battle Ticket. */
function outlookBlock(look: Outlook, mineColor: string, foeColor: string, what: string, sure: string, scout: Region | null): string {
  const at = (odds: number): number => (100 * odds) / (1 + odds);
  const need = at(look.even).toFixed(1);
  const takes = look.even.toFixed(look.even === 1.2 ? 1 : 2);
  const p = look.chance;
  if (!look.exact) {
    const word = look.chanceLo >= 0.95 ? `${sure} looks certain` : look.chanceHi <= 0 ? 'This looks hopeless' : p >= 0.6 ? 'The staff thinks the odds favour you' : p >= 0.3 ? 'The staff cannot call it' : 'The staff thinks it a long shot';
    return `<div class="balance"><div class="bhead"><span class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}">${word}</span><span class="sub">Odds between ${look.lo.toFixed(1)} and ${look.hi.toFixed(1)} to 1 · ${Math.round(look.chanceLo * 100) === Math.round(look.chanceHi * 100) ? `about ${oddsPct(look.chanceLo)}%` : `${oddsPct(look.chanceLo)} to ${oddsPct(look.chanceHi)}%`} chance ${what}</span></div>
      <div class="btrack"><div class="bal"><i style="width:${at(look.odds).toFixed(1)}%;background:${mineColor}"></i><i style="flex:1;background:${foeColor}"></i></div><span class="doubt" style="left:${at(look.lo).toFixed(1)}%;width:${Math.max(1, at(look.hi) - at(look.lo)).toFixed(1)}%" title="The balance lies somewhere in this band"></span><span class="need" style="left:${need}%" title="An even chance: roughly ${takes} to 1"></span></div>
      <div class="ends"><span>Your strength on the day</span><span>The white mark is an even chance; the hatched band is what the staff cannot see</span><span>Theirs</span></div></div>
      <div class="note fogged" style="margin:10px 0 2px"><b>The defence is not fully known.</b> ${esc(look.note)}${scout ? scoutButton(scout) : ''}</div>
      ${ticketBlock('What the campaign map feeds into this battle', look.lines)}`;
  }
  const word = p >= 0.95 ? `${sure} is all but certain` : p >= 0.6 ? 'The odds favour you' : p >= 0.3 ? 'It could go either way' : p > 0 ? 'A long shot' : 'This will fail';
  return `<div class="balance"><div class="bhead"><span class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}">${word}</span><span class="sub">Odds ${look.odds.toFixed(2)} to 1 · about ${oddsPct(p)}% chance ${what}</span></div>
    <div class="btrack"><div class="bal"><i style="width:${at(look.odds).toFixed(1)}%;background:${mineColor}"></i><i style="flex:1;background:${foeColor}"></i></div><span class="need" style="left:${need}%" title="An even chance: roughly ${takes} to 1"></span></div>
    <div class="ends"><span>Your strength on the day</span><span>The white mark is an even chance: about ${takes} to 1</span><span>Theirs</span></div></div>
    ${ticketBlock('What the campaign map feeds into this battle', look.lines)}`;
}

/** The card that plans an assault on a town, or a garrison's sortie against the army round it: the two sides face to face, the odds, and what follows either way. */
function siegeCard(): string {
  const m = ui.modal!;
  const r = g.regions[m.target];
  const me = g.human!;
  const out = m.mode === 'sally';
  const v = townSiege(g, r.id);
  const look = out ? sallyOutlook(g, r.id) : stormOutlook(g, r.id);
  const shut = `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${out ? 'The sortie' : 'The assault'} · ${date(g.turn)}</div><h2>${esc(out ? `Sortie from ${townName(r)}` : `Assault on ${townName(r)}`)}</h2></header>`;
  if (!v || !look) return `${shut}<div class="body"><div class="empty">${esc((out ? whyNotSallyOut(g, r.id) : whyNotStormTown(g, r.id)) ?? 'Nothing can be ordered here now.')}</div></div><footer><button class="btn go" data-act="close">Close</button></footer>`;
  const cands = out ? salliers(g, r, me) : stormCandidates(g, r.id);
  const going = out ? cands : stormForce(g, cands, r);
  const inSet = new Set(going.map((c) => c.id));
  const foes = out ? besiegers(g, r) : garrisonOf(r).filter((c) => !c.dead);
  const guard = !out && v.guard > 0 ? `<div class="ucard" title="Depot troops, police and militia: no match for an army, but behind walls they must still be beaten."><span class="utop">${ringIcon(g.nations[r.ctrl].color, 30)}</span><b>Town guard</b><span class="um">${v.guard.toFixed(1)} div · about ${num(guardMen(g, r))} men</span></div>` : '';
  const ours: Side = {
    nats: [me, ...new Set(going.filter((c) => c.nat !== me).map((c) => c.nat))], role: out ? 'coming out' : 'storming', who: leaderOf(going), men: going.reduce((s, c) => s + men(c), 0), corps: going.length, divs: rollDivs(going),
    cards: cands.map((c) => battleCard(c, { note: inSet.has(c.id) ? '' : 'Staff at its limit' })).join(''),
    under: `<small class="fhelp">${out ? 'Every corps of the garrison fit to fight comes out.' : 'Every corps round the town goes in, from every side at once.'}</small>`,
  };
  const foeNat = foes[0]?.nat ?? r.ctrl;
  const theirs: Side = {
    nats: foes.length ? [...new Set(foes.map((c) => c.nat))] : [r.ctrl], role: out ? 'round the town' : 'inside the town', who: leaderOf(foes), men: foes.reduce((s, c) => s + men(c), 0) + (out ? 0 : guardMen(g, r)), corps: foes.length, divs: rollDivs(foes) + (out ? 0 : Math.round(v.guard * 10) / 10),
    ...(look.exact ? {} : { menText: '?', divsText: '?' }),
    cards: foes.map((c) => (kn(c) === 'exact' ? battleCard(c) : foeCard(c))).join('') + guard,
    under: out ? '' : `<small class="fhelp">Behind the walls of ${esc(v.town)}: ${Math.round((v.bonus - 1) * 100)}% stronger${v.broken > 0 ? `, with ${Math.round(v.broken * 100)}% of the works broken down` : ''}.</small>`,
  };
  const hasty = !out && look.lines.some((l) => l.label === 'A hasty assault');
  const note = out
    ? '<div class="note" style="margin-top:14px"><b>What follows.</b> Win, and the army round the town is driven off and the siege is raised. Lose, and the garrison goes back inside the weaker for it. One sortie a month from each town.</div>'
    : `<div class="note" style="margin-top:14px"><b>What follows.</b> Carry the town and ${esc(r.name)} is yours, with everyone inside taken prisoner. Fail, and your corps pay more dearly than in the open, though the town guard is thinned too. Luck runs wider in the streets than in the field, and mostly against the attacker: an assault is an even chance at about ${look.even.toFixed(2)} to 1. A town can be assaulted once a month.${hasty ? ` <b>This is a hasty assault:</b> your army has only just come up, and goes in at ${Math.round(SIEGE.assault.hasty[v.size] * 100)}% of its weight. Next month it goes in at full weight, and anything your bombers, ships and guns break down meanwhile makes it easier.` : ''}</div>`;
  return `${shut}
    <div class="body"><div class="faceoff">${sidePanel(ours, 'att')}<div class="fmid"><span class="vsmark">VS</span><small>${esc(v.sizeWord[0].toUpperCase() + v.sizeWord.slice(1))}</small></div>${sidePanel(theirs, 'def')}</div>${out ? '' : prizeStrip(r)}
      ${outlookBlock(look, g.nations[me].color, g.nations[foeNat].color, out ? 'to break the siege' : `to carry ${esc(v.town)}`, out ? 'Breaking out' : 'Victory', out ? null : r)}${note}</div>
    <footer><button class="btn" data-act="close" title="Call it off. Nothing is spent.">${out ? 'Stay inside' : 'Keep up the siege'}</button><button class="btn" disabled title="Fighting a battle yourself in real time comes with the 3D build of the game. Until then every battle is settled from the Battle Ticket.">Fight it yourself · 3D build</button><button class="btn go" data-act="dosiege" title="Settle it at once from the Battle Ticket">${out ? 'Sally out' : 'Storm the town'}</button></footer>`;
}

function regionPane(): string {
  if (ui.sel < 0 && ui.sea >= 0) return seaPane();
  if (ui.sel < 0) {
    return `<h2>${g.human && !g.nations[g.human].alive ? `${esc(natName(g.human))} has fallen` : g.human ? 'Your orders' : 'The map'}</h2><p class="sub">${g.human && !g.nations[g.human].alive ? 'Your country has laid down its arms. End the month and the war goes on without you.' : g.human ? 'Click a region to see who holds it and which armies are there.' : 'Click any region to inspect it. Use Next month or Play to watch the war unfold.'}</p>${alertList().length ? `<div class="alerts inpane">${alertButtons()}</div>` : ''}${scopeNote()}${advice()}${prizesBlock()}${goalsBlock()}${g.human ? `<p class="sub pref">${watchPref() ? 'When you end the month, the map follows the other nations as they move.' : 'When you end the month, the other nations move at once.'} <button class="btn small" data-act="watchpref">${watchPref() ? 'Play them at once' : 'Show their moves'}</button></p>` : ''}
      <h3>How a month works</h3><ol class="steps"><li>Each army gets four moves a month, shown as gold dots. Tanks and motorized troops in a fresh offensive get one or two more.</li><li>Marching through friendly ground costs one move. Entering enemy ground costs foot infantry two.</li><li>An attack is settled at once from the Battle Ticket: forces, terrain, forts, supply, air, commanders.</li>${autoDig() ? '<li>An army left where it stands for a whole month digs in, yours and the enemy\'s alike: an earth bank shows under its flag, and the works take three months to finish. Against a line that is dug in and supplied, luck runs against the attacker: odds that are safe in the open become a gamble, the attack costs more men, and one thrown back is spent for the month. Massed guns, the first two months of a German offensive, and tanks against a line with no armour all weaken it. The attack card shows the chance before you commit.</li>' : ''}<li>A nation asks for terms when it has lost too much of its capital, its territory and its army.</li><li>Fleets hold the seas. A sea the enemy commands is closed to troopships and supply ships. Click any sea to see who holds it.</li>${airWar(g) ? '<li>Aircraft fly from airfields, the round pads on the map: click one to see the air groups based there. Fighters patrol their sky to catch raids, stick with a bomber group as its escort, or rest. Any group flies one raid a month. Open <b>Air</b> and plan it from its target: you see the chance of success and the losses to expect, then pick who goes. Bombers from several airfields can meet up for one raid, and the more that go together the smaller the share they lose. Engineers lay out new airfields.</li>' : '<li>Air fleets fly from airfields. Left alone they support the armies and defend their own sky. Given a mission they fight for the sky, bomb industry or strike at shipping, as far as their range allows.</li>'}${g.turn >= 9 ? '<li>In the desert an army is only as strong as the port behind it. Every region of desert between a corps and its nearest working port weakens its attacks and slows its advance, and cargoes sunk on the way to Libya lengthen the Axis line. Infantry on foot in open desert is easy meat for tanks unless it stands in a fortress.</li>' : ''}${g.corps.some((c) => c.airborne) ? '<li>Airborne troops need no ships. Once a month a corps marked Airborne can be dropped on an enemy coast across one sea, whoever holds the water, as long as the enemy\'s fighters do not hold the sky there. It fights without heavy weapons, so pick a weakly held target.</li>' : ''}${g.turn >= 20 ? '<li>Deep inside the Soviet Union an invader outruns his supplies: attacks weaken and marches slow until the railways behind are relaid. Mud and winter make it worse.</li>' : ''}${g.human ? '<li>Every province you hold pays into your treasury each month: factories most, then cities, farmland, harbours and oilfields. Your armies, fleets and air fleets draw their upkeep from it, and what is left buys ' + (budgeted(g) ? 'the arsenal: corps of every kind, aircraft by the wing, warships, and works for your provinces' : 'new corps and works') + '. The treasury is on the top bar: click it for the ledger. Click one of your own regions to raise corps or build works there, or open <b>Build</b> to see everything on order.</li>' + (budgeted(g) ? '<li>The town is the key to its province. Beat the army in the field and you hold only the country round the town: the province is yours when the town itself falls. A town held by nobody but its guard opens its gates to a strong army. A town with troops inside must be <b>stormed</b> (once a month, costly, and weaker the month you arrive), <b>softened</b> first by bombers, ships\' guns and artillery, or <b>starved</b> by keeping an army round it until its stores are gone. An army that closes round a held town marches no further that month. Put your own corps inside a town (click the province, then <b>Into the town</b>) and they fight from behind its walls; a garrison shut in can <b>sally out</b>, and an attack from a neighbouring province can raise the siege. A province under siege pays nothing, raises nothing and runs no trains. A country that is overrun, with its capital ringed, asks for terms.</li><li>Towns grow by what you build in them. The chief place of every province has building plots, one in a village and four in the capital: click a province and look under <b>The town</b>. Commerce pays money, farming feeds the depots, learning quickens research, training and medicine serve the army, and administration brings tax and makes conquered ground pay. Trade agreements pay you and your partner every month: open <b>Build</b>, <b>Towns</b>, <b>Trade agreements</b>. Each needs a trading house, and goods that go by sea can be sunk.</li><li>Every arm has its ground. Tanks are at their best in open country, weak in mountains, cities, marsh and forest, and cannot enter the steepest mountains at all (marked with a pair of peaks). Mountain troops are at home in mountains and hills. Lorries and guns bog down in marsh and mountain. Click any province to see how its ground treats each arm.</li><li>Tanks and infantry are worth more together. Infantry attacking with armour beside it hits up to 15% harder, and the tanks 10% harder with infantry to cover them. One armoured division supports about three of infantry. Tanks sent into a city or a forest alone are easy to pick off.</li><li>The great rivers are on the map, with a bridge wherever a road crosses one. An attack over standing bridges is a little weaker. Blow the bridges (click your province on the bank) and nothing on wheels or tracks can cross, in either direction: tanks, lorries and guns wait while infantry goes over in boats at a heavy cost. Troops driven off a river bank blow the bridges behind them unless fast troops seize them first. Nothing rebuilds itself: bring up an engineer corps.</li><li>The railways are on the map. Troops go by rail only along a working line, and an army is fed from its railhead: the nearest province joined by working track to one of your depots or supplied ports. More than two provinces of road beyond it, your troops fight weaker. Troops driven out of a province often tear up the track behind them, you can order your own to do it, and bombers can cut a line from the air. A cut line stays cut until engineers relay it, so an advance is only as fast as the railway behind it.</li><li>Engineers are raised on the Army shelf. Click the province an engineer corps stands in to give it a month of work there: rebuild bridges, relay a cut railway, rebuild a bombed factory, build field works, lay minefields and booby traps, or demolish fortifications. Sent in with an attack, engineers breach enemy works, clear lanes through his mines and help the infantry across a bridgeless river.</li><li>Heavy equipment needs industry. Tanks, artillery and motorized troops are raised only in a province with factories; aircraft need an aircraft industry and airfields to fly from; ships need a shipyard in a home port. Build a factory in a city and it also pays into the treasury every month. Ring your industry with anti-aircraft guns against bombing, and build airfields where you want fighter patrols.</li><li>A grand campaign is fought at one of five levels, from Cadet to Field Marshal, shown as a badge beside the date. Colonel is the fair fight. Below it your treasury and replacements get a helping hand and rival staffs are slower to attack; above it your rivals are richer and bolder, their bombers come for your factories and railways, and at Field Marshal they land on thinly held coasts and attack together. Battles are worked out the same way at every level. Click the badge to change the level in any month.</li>' : '') + (budgeted(g) ? '<li>Your government is under <b>Cabinet</b> on the top bar. The war cabinet has four offices: put a minister in one, for political capital, and he brings a gain and a price. Decrees put the economy and the call-up on a war footing, one step at a time. The cabinet\'s trust in you rises with ground and battles won and falls with home ground lost and debt. A cabinet that doubts you withholds political capital and a share of the treasury\'s income, and one with no trust left for three months running falls and takes your ministers with it. Every few months the cabinet, or an ally, sets a directive: a job with a last month and a reward. It is yours to carry out or to ignore, but each one ignored in a row costs more trust than the last, and setting one aside early costs half. The town a directive names carries a brass seal on the map with the months left. The cabinet also talks back. Every minister sits with the <b>hawks</b> or the <b>doves</b>: two more of one wing than the other and that wing shapes what the cabinet asks for and what it takes hard. You can <b>call your shot</b>: promise the cabinet an enemy town by a day you name, from the cabinet room or from the province the town stands in, and the odds on the day set what keeping your word earns and breaking it costs. The cabinet reckons a town the way the attack card does, with your corps in reach brought up, and it remembers how each month opened, so marching back a province before you promise changes nothing. And after defeats that could have been won the cabinet may <b>ask for the head of a general</b>, and shows the battles it holds against him: dismiss him and its trust recovers, or stand by him and let his next battle settle who was right. If you would sooner fight the war without the politics, click the level badge beside the date and switch the war cabinet off: what stands open is settled first, at its usual price. Or keep the cabinet and make <b>your office secure</b> on the same card: ministers, directives and trust all work as before, and a cabinet that doubts you still holds money back, but it never falls and you are never turned out.</li>' : '') + '<li>Neutral countries drift toward whoever is winning. Open <b>Powers</b> to court them with political capital, and to see who each one counts as a friend.</li><li>To invade a neutral, pick up an army beside its frontier and send it across. The ground shows in amber, and you are asked whether you mean war before anything moves.' + (budgeted(g) ? ' Striking a country at peace catches it unready: your attacks on its ground hit ' + Math.round((RELATIONS.sneak.attack - 1) * 100) + '% harder that month. The price is paid elsewhere. Every neutral grows warier of you, the victim\'s friends most of all, and some will stop trading with you or lean toward your enemies.' : '') + '</li>' + (budgeted(g) ? (paths(g).length ? '<li>Yours is a small country, and its path is its own. Open <b>Your path</b> on the top bar to join the Axis, join the Allies, stand alone, or found a League with a willing neighbour. No treaty signs your land away and no courting moves you; if you are attacked while you stand alone, you fight alone until you choose a side.</li>' : '') + '<li>Once in a while a few smaller nations found the League, a third alliance that belongs to neither the Axis nor the Allies and shows on the map in a colour of its own. Its members defend one another: declare war on one and you are at war with them all. The declaration prompt names them, and <b>Powers</b> lists its members.</li>' : '') + '<li>In the grand campaign every country has abilities of its own: Germany\'s tanks hit harder in open country, the Soviet Union makes good its losses faster and moves its factories out of reach, Switzerland is a fortress in its mountains and a bank while at peace. Many countries can also raise a special formation from the <b>Build</b> tab, marked with a star: Guards, Alpine troops, heavy tanks, ski troops. Open <b>Abilities</b> on the top bar, or from <b>Build</b> or <b>Powers</b>, to read any country\'s list. A battle\'s ticket names the abilities that count in it.</li><li>Generals gain experience in battle and earn traits from how they fight. Open <b>Generals</b> to give them commands and spend their skill points.</li><li>Every unit keeps a record, and one that has done enough is honoured: a breakthrough, a stand against the odds, a capital entered, a full tour of missions. Keep the honour at the front and the unit fights harder, or send its heroes home to sell war bonds or train the next drafts. Open <b>Honours</b>, or the <b>Record</b> strip under any unit.</li><li>Open <b>Navy</b> to send your fleets out. A corps standing in a port can be shipped to a friendly port or landed on an enemy coast.</li>' + (budgeted(g) ? '<li>Fleets are pieces on the map, handled like armies. Click one of your fleets to pick it up: its ships are laid out as cards by class, and the seas it can reach are ringed. Give it one of five orders. <b>Patrol</b> holds a sea and brings enemy fleets to battle. <b>Raid</b> goes after merchant ships. <b>Escort</b> binds it to one shipping lane and guards the convoys the whole way. <b>Hunt</b> sends its destroyers and carrier aircraft after submarines, which nothing else can sink. <b>Port</b> brings it home to refit. Battleships and cruisers standing off an enemy coast can shell the troops or the factories there once a month: shell first, attack after. Heavy ships need destroyers round them, or a submarine may get one. <b>Split off ships</b> makes an escort group or a bombardment force out of a big fleet.</li><li>A carrier is a floating airfield. Every fleet with carriers brings a wing of fighters and a wing of strike aircraft, laid out as cards beside its ships. At sea they reach every coast of the sea they are in, inland as far as their range allows, and the seas next door. The fighters keep enemy bombers off the fleet and rise to meet any raid on it. The strike aircraft go for a fleet at sea, for ships lying in a harbour, or for anything ashore: click a wing, or open <b>Air</b>, and plan the strike like any raid. Bombers on land can strike ships too, though few crews are trained for it. A fleet has to be found before it can be struck. You see only the enemy ships your side has identified or had reported, so keep the seas searched: ships and carriers search the sea they are in, aircraft search from your coasts, and any group can fly a search of one sea a month. Wings are made up to strength only in harbour, and a carrier with empty decks is worth little in a fight. A harbour raided in the month war is declared is caught unready.</li>' : '') + '<li>Open <b>Air</b> to move your air fleets and give them missions. The month\'s air campaigns are flown when you end the month.</li>' + (budgeted(g) ? '<li>Research is a tree of technologies in five branches: land, air, naval, industry and doctrine. Each of your research teams works on one technology at a time, and a technology opens when the ones before it are done. What you finish lasts for the rest of the war: stronger tanks and infantry weapons, better fighters, cheaper ships, more teams. Some things cannot be ordered at all until their technology is researched: armoured and motorised corps, aircraft carriers, airborne troops. A technology begun before its year takes longer. Open <b>Research</b>, then the tree; a locked order under <b>Build</b> takes you straight to the technology it waits on.</li><li>Money builds an army, oil moves it, and supplies keep it fighting. The six great powers keep a stock of each, shown beside the treasury on the top bar: every month what they make at home and bring in from abroad goes in, and what their tanks, aircraft, ships and people need comes out. When a stock runs dry the forces get only what arrived that month: tanks and lorried troops attack weaker and lose a move, aircraft and fleets are worth less, fewer replacements go out, factories stand idle, and after months of it the nation loses the will to fight. Oil comes from oilfields (click one on the map), from deals with neighbours, and across the sea along the shipping lanes drawn on the map. A lane in amber is losing cargoes to raiders and one in red is cut. Guard your own with an escort; send submarines into an enemy\'s. Synthetic fuel in the technology tree makes oil at home that no raider can reach. Smaller countries buy what they need abroad and are never short.</li><li>An army is made of men, and no country has them without end. Every nation has a reserve of men fit to bear arms, shown as <b>Men</b> on the top bar. Replacements for worn corps and every new corps are drawn from it, and each month a class of young men comes of age and joins it: a larger class in a great country, a smaller one as home ground is lost. When the reserve is spent only the month\'s class reaches the armies and no new corps can be raised, so a war of attrition is lost by the side that runs out of men first. The call-up decrees in the Cabinet enlarge the class. The count is in the ledger, and so is your say over the call-up: take the next class early, which is men now and none for some months after, or hold the standing call-up so that the men go to the corps in the field.</li><li>Allies can fight as one army. Where an ally\'s corps stands in a province with yours, the tray and the province panel offer <b>Join my army</b>. A corps that joins keeps its own flag, pay and supply, but takes your orders in your month: it marches and attacks with your corps, and the ground you take together is yours. Its country lends only a share of its army, and only against enemies it is itself at war with. Send it back from the same place, or leave it behind for a month and it goes home by itself.</li><li>The fog of war hides what your side cannot see. Beside your own lines a foreign army is a flag, a count of its corps and your staff\'s estimate of its strength; a province further off it is a flag and a count; deeper than that the map is dark. What an army is made of is known only once its corps are identified: by fighting them, by an agent with them in view, or by a reconnaissance flight over their province. A report is exact in the month it is made and then goes stale: the strength becomes an estimate that widens every month, morale and rest are no longer known, and after ' + INTEL.reports.keep + ' months the corps is unidentified again. Where the defence is not known, the card that plans an attack gives the odds as a band. Open <b>Spies</b> to recruit agents and send them anywhere on the map. An agent identifies every army in its province and around it, reads a government\'s books in its capital, and can wreck a factory, blow a railway or fire an oilfield in a country you are at war with. Every month abroad an agent may be caught. Keep one at home on guard, because the other side has saboteurs too. The fog can be lifted under <b>Spies</b> if you would rather see everything.</li><li>When a great power has fallen and the great powers have stopped fighting each other, the war is decided and a card gives you the choice: take the victory, or <b>World Domination</b>. That dissolves every alliance and sets every nation still standing against every other, yesterday\'s allies among them, until one is left. It cannot be undone. The Powers tab then shows the standings.</li>' : '<li>Weapons improve on history\'s clock. Open <b>Research</b> to put extra design teams on the branches you want early; a branch left without a team falls behind.</li>') + '' : ''}</ol>`;
  }
  const r = g.regions[ui.sel];
  const me = g.human;
  const held = r.ctrl === r.owner ? `${g.nations[r.owner].adj} territory` : `${g.nations[r.owner].adj} territory, held by ${natName(r.ctrl)}`;
  const tags: string[] = [];
  if (r.cap) tags.push('<span class="tag good">Capital</span>');
  if (r.port) tags.push('<span class="tag">Port</span>');
  if (r.seas.length) tags.push(`<span class="tag">Coast: ${r.seas.map((z) => esc(g.seas[z].name)).join(', ')}</span>`);
  if (r.fort) tags.push(`<span class="tag">Fortified, level ${r.fort.lv}, facing ${r.fort.vs.map((v) => g.nations[v]?.adj ?? v).join(' and ')} ground</span>`);
  if (me === 'POL' && r.sov && r.ctrl === 'POL' && !g.flags.sovietMove && g.turn <= 3) tags.push('<span class="tag bad" title="Once Poland\'s front breaks, the Red Army crosses the border under its pact with Germany and takes this province. Troops left here are lost.">East of the pact line</span>');
  if (me && g.nations[me].alive && allied(g, me, r.ctrl)) tags.push(g.supply[me]?.[r.id] ? '<span class="tag good">In your supply net</span>' : '<span class="tag bad">No supply line to here</span>');
  const sky = skyOver(g, r.id);
  if (sky) tags.push(`<span class="tag ${me && allied(g, me, sky) ? 'good' : 'bad'}">The ${g.nations[sky].adj} air force holds the sky here</span>`);
  const based = g.airFleets.filter((f) => f.base === r.id && f.ftr + f.bmr > 0);
  const mineHere = r.units.filter((c) => canOrder(g, c));
  let acts = '';
  const picked = pickedCorps();
  if (picked.length) {
    const canRail = picked.some((c) => railReach(g, c).size > 0);
    // An ally's corps in a joint army does not sail under the player's orders.
    const canShip = picked.some((c) => c.nat === me && shipTargets(g, c).length > 0);
    const canEvac = picked.some((c) => c.nat === me && g.nations[c.nat].seaSupply && r.port && r.owner !== c.nat);
    const evacTo = canEvac ? refuge(g, picked[0], r) : null;
    acts = `<div class="acts">${canRail ? `<button class="btn small${ui.rail ? ' on' : ''}" data-act="rail">Rail move</button>` : ''}${canShip ? `<button class="btn small${ui.ship ? ' on' : ''}" data-act="ship">Ship by sea</button>` : ''}${lands.length ? (byAir(picked) ? `<button class="btn small${ui.land ? ' on' : ''}" data-act="land" title="Drop these airborne troops on an enemy coast across one sea. They need no ships, and fight without heavy weapons.">Airborne assault</button>` : `<button class="btn small${ui.land ? ' on' : ''}" data-act="land" title="Land these corps on an enemy coast.">Invade by sea</button>`) : ''}${canEvac ? `<button class="btn small" data-act="evac" title="An emergency lift ${evacTo && !evacTo.cap ? `to ${esc(evacTo.name)}, the nearest haven still in friendly hands` : 'home'}. The corps leaves a quarter of its strength behind.">Evacuate by sea</button>` : ''}${picked.length === 1 ? (picked[0].general !== null ? `<button class="btn small" data-act="general" data-id="${picked[0].general}">General's record</button>` : `<button class="btn small" data-act="appoint" data-id="${picked[0].id}">Appoint a general</button>`) : ''}<button class="btn small" data-act="unpick">Stand down</button></div>`;
  } else if (mineHere.some((c) => c.ap > 0)) {
    acts = `<div class="acts"><button class="btn small" data-act="pickall">Select all here</button></div>`;
  }
  // The fog of war: what the player's side cannot see is not listed, and what it only glimpses is a count.
  const inView = r.units.filter((c) => inFull(r, c.nat));
  const veiled = r.units.length - inView.length;
  const units = (inView.length ? knownRows(r, [...inView].sort((a, b) => (a.nat === me ? 0 : 1) - (b.nat === me ? 0 : 1))) : veiled || (fogOn(g) && seen[r.id] === 0) ? '' : '<div class="empty">No armies here.</div>') + fogNote(r);
  return `<h2>${esc(r.name)}</h2><div class="sub">${held}</div>${townLine(r)}${dirLine(r)}${vowLine(r)}${pocketNote(r)}${leftWorksNote(r)}${siegeBlock(r)}${freeNote(r)}${standingNote(r)}
    <div class="facts"><div class="fact"><span>Terrain</span><b>${r.terrain[0].toUpperCase() + r.terrain.slice(1)}</b></div><div class="fact"><span>Victory pts</span><b>${r.vp}</b></div><div class="fact"><span>Industry</span><b>${r.fac}${railNote(g, r.id)?.ruined ? ` <small style="color:#f0b3a5;font-weight:500">${railNote(g, r.id)!.ruined} in ruins</small>` : ''}</b></div></div>
    ${tags.length ? `<div class="tags" style="display:flex;gap:5px;flex-wrap:wrap;margin-top:8px">${tags.join('')}</div>` : ''}
    ${prizeBlock(r)}${placesBlock(r)}${groundBlock(r)}${riverBlock(r)}${railBlock(r)}${provinceWorth(r)}${oilNote(r)}${provinceSlots(r)}${townPlots(r)}
    <h3>Forces here</h3>${acts}${units}${jobBlock(r)}${spyBlock(r)}
    ${based.length ? `<h3>On the airfields</h3>${based.map((f) => airRow(f, false, true)).join('')}` : ''}${provinceMuster(r)}${ui.sel >= 0 && me ? advice() : ''}`;
}

/** How this province's ground suits each arm (the grand campaign's ground rules). */
function groundBlock(r: Region): string {
  const says = groundSays(g, r);
  if (!says) return '';
  const notes = groundNotes(g, r);
  return `<div class="ground${steep(g, r) ? ' steep' : ''}">${r.terrain === 'mountain' || r.terrain === 'hills' ? `<svg viewBox="-6 -4.6 12 8" aria-hidden="true"><path d="${PEAKS}"/></svg>` : ''}<div><b>${esc(says)}</b>${notes.length ? `<ul>${notes.map((n) => `<li>${esc(n)}</li>`).join('')}</ul>` : ''}</div></div>`;
}

/** The rivers on this province's borders: which bridges stand, which are down, and the order to blow them. */
function riverBlock(r: Region): string {
  const notes = riverNotes(g, r.id);
  const mines = minesAt(g, r);
  if (!notes.length && !mines) return '';
  const mine = humanUp(g);
  const rows = notes.map((n) => {
    const far = g.regions[n.far];
    if (n.down) return `<div class="rrow down"><span><b>${esc(n.river)}</b>, toward ${esc(far.name)}</span><small>Bridges down since ${date(n.since ?? g.turn)}. Tanks, lorries and guns cannot cross, and infantry crosses in boats. Engineers on either bank can rebuild them.</small></div>`;
    const why = mine ? whyNotBlowBridges(g, r.id, far.id) : 'It is not your month.';
    const yours = g.human && allied(g, g.human, r.ctrl);
    return `<div class="rrow"><span><b>${esc(n.river)}</b>, toward ${esc(far.name)}</span><small>Bridges standing. An attack across them is cut to ${Math.round(ENGINEERING.river.attack * 100)}%.</small>${yours ? `<button class="btn small" data-act="blow" data-from="${r.id}" data-to="${far.id}" ${why ? `disabled title="${esc(why)}"` : 'title="Blow the bridges. Nothing on wheels or tracks crosses here, in either direction, until engineers rebuild them."'}>Blow the bridges</button>` : ''}</div>`;
  }).join('');
  const mined = mines ? `<div class="rrow mined"><span><b>Minefields</b>${mines > 1 ? ', two belts' : ''}</span><small>Mines and booby traps on every road in: an attack on ${esc(r.name)} is cut by ${Math.round(ENGINEERING.mines.attack * mines * 100)}% and costs the enemy a move more to enter, unless his engineers clear lanes.</small></div>` : '';
  return `<div class="rivers">${rows}${mined}</div>`;
}

/** The railway through this province: working or cut, how far the player's troops here are from their railhead, and the order to tear it up. */
function railBlock(r: Region): string {
  const n = railNote(g, r.id);
  if (!n) return '';
  const me = g.human;
  const yours = !!me && g.nations[me].alive && allied(g, me, r.ctrl);
  const pct = (m: number): number => Math.round((1 - m) * 100);
  const rows: string[] = [];
  if (n.line) {
    const to = n.to.map((j) => g.regions[j].name);
    const lines = to.length > 4 ? `${to.slice(0, 4).join(', ')} and ${to.length - 4} more` : to.join(', ');
    if (n.cut) rows.push(`<div class="rrow down"><span><b>Railway cut</b></span><small>Torn up since ${date(n.since ?? g.turn)}. No train runs into, out of or through ${esc(r.name)} until ${n.hasty > 0 ? `the railwaymen have made it good, in about ${n.hasty === 1 ? 'a month' : `${n.hasty} months`}, or engineers relay the track sooner` : 'engineers relay the track'}. Only engineers can be carried up to the break.</small></div>`);
    else {
      const why = whyNotCutRailway(g, r.id);
      rows.push(`<div class="rrow rail"><span><b>Railway</b>, to ${esc(lines)}</span><small>The line is working.</small>${yours ? `<button class="btn small" data-act="cutrail" data-id="${r.id}" ${why ? `disabled title="${esc(why)}"` : 'title="Tear up the track. No train runs into, out of or through this province, for either side, until engineers relay it."'}>Tear up the railway</button>` : ''}</div>`);
    }
  } else if (r.terrain !== 'desert' && !r.overseas) rows.push('<div class="rrow rail"><span><b>No railway</b></span><small>No line runs to this province: everything here comes and goes by road or by sea.</small></div>');
  if (yours && n.road > 0) {
    const far = n.attack < 1;
    rows.push(`<div class="rrow ${far ? 'down' : 'rail'}"><span><b>${n.road} ${n.road === 1 ? 'province' : 'provinces'} by road from your railhead</b></span><small>${far
      ? `Your troops here have outrun their supplies: they attack ${pct(n.attack)}% weaker and defend ${pct(n.defence)}% weaker until engineers bring the working line nearer.`
      : `Near enough to be fed in full. Beyond ${RAILWAYS.railhead[0][0]} provinces of road an army fights weaker.`}</small></div>`);
  }
  if (n.ruined) rows.push(`<div class="rrow down"><span><b>${n.ruined} of ${n.factories} ${n.factories === 1 ? 'factory' : 'factories'} in ruins</b></span><small>Wrecked by bombing or by saboteurs. A ruined factory earns nothing until engineers rebuild it, one a month.</small></div>`);
  return rows.length ? `<div class="rivers">${rows.join('')}</div>` : '';
}

/** The player's engineers in a province: what those at work are doing, and the jobs the first idle corps can be given. */
function jobBlock(r: Region): string {
  if (!groundRules(g) || !humanUp(g)) return '';
  const mine = r.units.filter((c) => c.kind === 'eng' && canOrder(g, c));
  if (!mine.length) return '';
  const busy = mine.filter((c) => c.job).map((c) => {
    const fresh = c.job!.eta === g.turn + 1;
    return `<div class="note"><b>${esc(c.name)} is at work.</b> ${esc(jobLine(g, c))}.${fresh ? ` <button class="btn small" data-act="stopjob" data-id="${c.id}">Stop the work</button>` : ''}</div>`;
  }).join('');
  const c = mine.find((x) => !x.job);
  if (!c) return `<div class="jobs">${busy}</div>`;
  // What the corps can start now comes first, and what it cannot follows with the reason.
  const offers = engineerJobs(g, c).map((o, i) => ({ o, i })).sort((p, q) => Number(!!p.o.why) - Number(!!q.o.why) || p.i - q.i).map((x) => x.o);
  return `<div class="jobs">${busy}<h3>Work for ${esc(c.name)}</h3><p class="sub" style="margin:0 0 7px">A job takes the corps the rest of the month where it stands, and is done when your next month begins. Sent in with an attack instead, engineers breach fortifications, clear mines and get infantry over a river whose bridges are down.</p>${offers.map((o) => `<button class="buy offer" data-act="job" data-id="${c.id}" data-key="${o.key}" ${o.why ? 'disabled' : ''}><span class="grow"><b>${esc(o.label)}</b><small>${esc(o.blurb)}</small>${o.why ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span></button>`).join('')}</div>`;
}

const SHELF = { army: 'Army', air: 'Air force', navy: 'Navy', works: 'Works', towns: 'Towns' } as const;

function buildPane(): string {
  const me = g.human!;
  const n = g.nations[me];
  const mine = humanUp(g);
  const have = Math.floor(n.pp);
  const L = ledger(g, me);
  const list = arsenalOffers(g, ui.sel);
  const sel = ui.sel >= 0 ? g.regions[ui.sel] : null;
  const at = (text: string): string => `<b style="color:var(--ink)">${esc(text)}</b>`;
  const perDiv = cash(PRODUCTION.replacementCost), depots = Math.round(PRODUCTION.manpower * 100);
  let out = `<h2>${L.budget ? 'The arsenal' : 'War production'}</h2><p class="sub">${L.budget
    ? `Every province you hold pays into the treasury, and every corps, ship and aircraft draws its upkeep from it. Losses in the field are made good next, at ${perDiv} a division, as fast as the depots can train men: about ${depots}% of the army a month. What is left is yours to spend.`
    : `Your factories pay into the treasury every month. Losses in the field are made good first, at ${perDiv} a division, as fast as the depots can train men: about ${depots}% of the army a month. What is left is yours to spend.`}</p>
    <div class="facts"><div class="fact"><span>In the treasury</span><b${L.treasury < 0 ? ' style="color:var(--red)"' : ''}>${inHand(L.treasury)}</b></div><div class="fact"><span>${L.budget ? 'After upkeep, a month' : 'Each month'}</span><b${L.net < 0 ? ' style="color:var(--red)"' : ''}>${cash(L.net, true)}</b></div><div class="fact"><span>On order</span><b>${n.queue.length}</b></div></div>
    <div class="acts"><button class="btn small" data-act="ledger">Open the ledger</button>${L.budget && hasTraits(me) ? '<button class="btn small" data-act="traits">National abilities</button>' : ''}</div>`;
  if (L.treasury < 0) out += `<div class="note"><b>The treasury is in debt.</b> Nothing new can be ordered and no replacements go out until the books balance. Unpaid troops rest badly and men drift away.</div>`;
  if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> Industry runs at half speed until the nation is at war${L.budget ? ', and the forces cost half as much to keep' : ''}.</div>`;

  const shelf = (k: Offer['group'], quiet = false): string => list.filter((o) => o.group === k).map((o) => offerButton(o, quiet)).join('');
  const army = (): string => {
    let s = `<h3>Raise a new corps</h3>`;
    const any = list.filter((o) => o.group === 'army');
    if (!any.some((o) => o.at >= 0)) return `${s}<div class="empty">There is no home ground left to raise troops on.</div>`;
    s += `<p class="sub" style="margin:0 0 7px">${L.budget ? 'Infantry and mountain troops can be raised in any home province. Heavier equipment is built where there is industry: motorized troops and artillery need a factory in the province, tanks need two. ' : ''}${sel && canRaise(g, me, sel) ? `Raising at ${at(sel.name)} where it can be.` : 'Click one of your own home regions on the map to raise troops there.'}</p>${shelf('army')}`;
    const lostOnes = any.filter((o) => o.key.startsWith('corps:')).flatMap((o) => revivable(g, me, o.key.split(':')[1])).sort((a, b) => b.turn - a.turn).slice(0, 6);
    if (lostOnes.length) {
      s += `<h3>Raise a lost corps again</h3><p class="sub" style="margin:0 0 7px">A formation on the memorial wall can be raised again under its old name. It costs what any new corps costs, and it carries the old colours, the name the army gave it and part of what the old corps knew.</p>`;
      for (const f of lostOnes) {
        const b = specOf(g, f.kind)!;
        const site = raiseSite(g, me, f.kind, ui.sel);
        if (site < 0) continue;
        const price = corpsCost(g, me, f.kind);
        s += `<button class="buy" data-act="revive" data-id="${f.id}" ${mine && have >= price ? '' : 'disabled'}>${icon({ nat: me, kind: f.kind } as Corps)}<span class="grow"><b>${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}</b><small>${fallenLine(f)} ${ribbons(f)}</small><small>${b.divs} divisions · ready in ${b.months} months · forms up at ${esc(g.regions[site].name)}</small></span><span class="cost${have >= price ? '' : ' short'}">${money(price)}</span></button>`;
      }
    }
    return s;
  };
  const fortify = (): string => {
    // A historical scenario builds one kind of works: fortifications, on home ground.
    const s = `<h3>Fortify</h3>`;
    if (!sel || !isHome(g, me, sel)) return `${s}<div class="empty">Click one of your own home regions on the map to fortify it.</div>`;
    return s + (shelf('works') || `<div class="empty">${esc(sel.name)} is already fortified as heavily as it can be.</div>`);
  };
  if (!L.budget) out += army() + fortify();
  else {
    out += `<div class="seg shelves">${(Object.keys(SHELF) as (keyof typeof SHELF)[]).map((k) => `<button class="btn small${ui.arm === k ? ' on' : ''}" data-act="arm" data-arm="${k}">${SHELF[k]}</button>`).join('')}</div>`;
    if (ui.arm === 'army') out += army();
    else if (ui.arm === 'air') {
      out += `<h3>Order aircraft</h3><p class="sub" style="margin:0 0 7px">${airWar(g) ? 'Aircraft come by the wing from the nation\'s aircraft industry, and are stationed on an airfield of yours, the round pads on the map. A new wing joins a group of its kind there if one has room, or forms a new air group.' : 'Aircraft come by the wing from the nation\'s aircraft industry, and are stationed on airfields: a province where one of your air fleets is based, or one where you have built airfields.'} ${sel && canStation(g, me, sel) ? `Stationing at ${at(sel.name)}.` : 'Click such a province on the map to station them there.'}</p>${shelf('air')}`;
    } else if (ui.arm === 'navy') {
      out += `<h3>Lay down ships</h3><p class="sub" style="margin:0 0 7px">Ships are laid down in a home port whose shipyard is big enough, and they take a long time. A level 1 yard builds destroyers, submarines and troopships, level 2 cruisers, level 3 carriers and battleships. ${sel && isHome(g, me, sel) && yardLevel(g, sel) > 0 ? `Building at ${at(sel.name)} where its yard will do.` : 'Click one of your ports on the map to build there.'}</p>${shelf('navy')}`;
    } else if (ui.arm === 'towns') out += townsShelf(sel);
    else {
      const own = !!sel && sel.ctrl === me;
      out += `<h3>Build works</h3><p class="sub" style="margin:0 0 7px">${own ? `Building at ${at(sel!.name)}.` : '<b style="color:var(--brass)">Click one of your own provinces on the map to build there.</b>'} Factories go up in cities and add to your income every month. Shipyards go in ports. Anti-aircraft guns and airfields can go on any ground you hold.</p>${shelf('works', !own)}`;
    }
  }

  out += `<h3>On order</h3>`;
  if (!n.queue.length) out += `<div class="empty">Nothing on order.</div>`;
  for (const b of [...n.queue].sort((a, c) => a.eta - c.eta)) {
    const left = b.eta - g.turn;
    const where = g.regions[b.region]?.name ?? '';
    out += `<div class="order"><span class="grow"><b>${esc(b.name)}</b><small>${b.what === 'corps' ? `Forming at ${esc(where)} · ready` : 'Ready'} ${date(b.eta)}${left > 0 ? ` (${left} month${left > 1 ? 's' : ''})` : ''}</small></span>${b.ordered === g.turn && mine ? `<button class="btn small" data-act="cancelbuild" data-id="${b.id}">Cancel</button>` : `<span class="cost">${money(b.cost)}<small>paid</small></span>`}</div>`;
  }
  return out;
}

// ---- Towns and trade (the grand campaign) -------------------------------------------------------------

/** Our own signs for the six lines of building: a market awning, a barn and silo, a scholar's cap, a tent, a ward and a hall with columns. */
const CIVIC_GLYPH: Record<string, string> = {
  market: '<path d="M2.5 7.6 5 3h14l2.5 4.6v1a2.4 2.4 0 0 1-4.75 0 2.4 2.4 0 0 1-4.75 0 2.4 2.4 0 0 1-4.75 0 2.4 2.4 0 0 1-4.75 0ZM4.6 12.2h14.8v4.3H4.6Z"/>',
  farm: '<path d="M2.5 16.5V9l4.25-4.6L11 9v7.5H8.4v-4H5.1v4ZM13.6 16.5V6.4a3 3 0 0 1 6 0v10.1Z"/>',
  school: '<path d="M12 3 1.5 7.6 12 12.2l8-3.5v5h1.6V8ZM5.6 11.2v3.2c1.6 1.5 3.9 2.2 6.4 2.2s4.8-.7 6.4-2.2v-3.2L12 14Z"/>',
  camp: '<path d="M12 2.5l1 1.8 8.5 12.2h-6.8L12 10.8l-2.7 5.7H2.5L11 4.3Z"/>',
  hospital: '<path fill-rule="evenodd" d="M3 16.5V7.2l9-4.7 9 4.7v9.3ZM10.7 7.4v2.4H8.3v2.6h2.4v2.4h2.6v-2.4h2.4V9.8h-2.4V7.4Z"/>',
  hall: '<path d="M2 7.4 12 2.5l10 4.9v1.5H2ZM4.2 10h2.6v4.6H4.2ZM10.7 10h2.6v4.6h-2.6ZM17.2 10h2.6v4.6h-2.6ZM2 15.4h20V17H2Z"/>',
};
/** An empty lot: a pegged-out square. */
const PLOT_GLYPH = '<path class="line" stroke-dasharray="2.2 2.2" d="M5 3.5h14v11H5Z"/><path class="line" d="M12 6.5v5M9.5 9h5"/>';
const civicGlyph = (line: string, cls = 'wglyph'): string => `<svg class="${cls}" width="30" height="22" viewBox="0 0 24 18">${CIVIC_GLYPH[line] ?? ''}</svg>`;

/** One building plot as a tile: what stands on it and what it does, the next level as a button, or a free lot to build on. */
function plotCell(t: TownBuildings, p: PlotView, inCard = false): string {
  const id = t.region;
  if (!p.line) {
    return t.mine && !inCard
      ? `<button class="slot act vacant" data-act="town" data-id="${id}" title="Choose what to build on this plot.">${slotIcon(PLOT_GLYPH)}<b>Free plot</b><small class="do">Choose a building</small></button>`
      : `<div class="slot vacant">${slotIcon(PLOT_GLYPH)}<b>Free plot</b><small>${t.mine ? 'Choose a building below' : 'Nothing built'}</small></div>`;
  }
  const o = t.offers.find((x) => x.line === p.line);
  const glyph = slotIcon(CIVIC_GLYPH[p.line] ?? '');
  const pips = pipsOf(p.level, p.building ? 1 : 0, 3);
  const text = p.level ? `${p.label}. ${p.does}` : `${p.label}: the builders are at work`;
  const due = p.building ? `<small class="do">${p.level && o ? `${esc(o.label)} ready` : 'Ready'} ${date(p.due)}</small>` : '';
  if (o && !p.building && (o.why === null || o.poor)) {
    return `<button class="slot act has" data-act="civic" data-id="${id}" data-line="${p.line}" ${humanUp(g) && o.why === null ? '' : 'disabled'} title="${esc(o.effect)} Ready in ${months(o.months)}.${o.poor ? ' The treasury cannot cover it yet.' : ''}">${glyph}<b>${esc(p.name)}</b>${pips}<small>${esc(text)}</small><small class="do">Raise to ${esc(o.label.toLowerCase())} · ${money(o.cost)}</small></button>`;
  }
  return `<div class="slot${p.level ? ' has' : ''}"${o?.why && !p.building ? ` title="${esc(o.why)}"` : ''}>${glyph}<b>${esc(p.name)}</b>${pips}<small>${esc(text)}</small>${due}</div>`;
}

/** The building plots of a province's chief place, under the province's works. Other countries' towns show only what stands in them. */
function townPlots(r: Region): string {
  const t = townBuildings(g, r.id);
  if (!t || (!t.mine && t.free === t.plots.length)) return '';
  const n = t.plots.length;
  return `<h3>The town <span class="inhand">${esc(t.town)} · ${n} building plot${n === 1 ? '' : 's'}${t.free ? `, ${t.free} free` : ', all taken'}</span></h3><div class="slots">${t.plots.map((p) => plotCell(t, p)).join('')}</div>${t.mine ? `<div class="acts"><button class="btn small" data-act="town" data-id="${r.id}" title="Everything that can be built at ${esc(t.town)}: what it costs, how long it takes and what it does.">The town plan</button></div>` : ''}`;
}

/** The town plan: the plots of one place, and every line of building with its three levels, price, time and effect. */
function townCard(id: number): string {
  const t = townBuildings(g, id);
  const r = g.regions[id];
  if (!t || !r) return '';
  const tv = townIn(g, id);
  const me = g.human;
  const have = me ? g.nations[me].pp : 0;
  const own = r.owner === r.ctrl;
  const row = (o: CivicOffer): string => {
    const p = t.plots.find((x) => x.line === o.line);
    const going = !!p?.building, top = o.has >= 3 && !going, can = o.why === null;
    const levels = CIVIC.lines.find((l) => l.id === o.line)?.levels ?? [];
    const names = levels.map((nm, i) => `<i class="${i < o.has ? 'lit' : i === o.has ? (going ? 'due' : 'nx') : ''}">${esc(nm)}</i>`).join('<u>›</u>');
    const btn = top ? '<span class="tag good">Complete</span>'
      : going ? `<span class="tag">Building · ${date(p!.due)}</span>`
        : `<button class="btn small${can ? ' go' : ''}" data-act="civic" data-id="${id}" data-line="${o.line}" ${can && humanUp(g) ? '' : 'disabled'}>${o.has ? 'Raise' : 'Build'} · ${money(o.cost)}<small>${months(o.months)}</small></button>`;
    return `<div class="civrow${o.has ? ' has' : ''}${!can && !going && !top && !o.poor ? ' off' : ''}">${civicGlyph(o.line)}<span class="grow"><b>${esc(o.name)}</b><span class="civlv">${names}</span><small>${esc(o.blurb)}</small>${top ? '' : `<small class="eff"><b>${esc(o.label)}:</b> ${esc(o.effect)}</small>`}${o.why && !going && !top ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span>${btn}</div>`;
  };
  const story = `Think of a town as a high street with a few empty lots. A village has one, a town two, a city three and the capital four. Each lot takes one kind of building, and each building can be raised two more levels. A lot once built on keeps its trade, so decide what each place is for: money, food, learning, soldiers, healing or government.${own ? '' : ' On conquered ground only administration and medicine can be set up.'}`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The town plan · ${date(g.turn)}</div><h2>${tv ? townIcon(tv, 30) : ''} ${esc(t.town)}</h2></header>
    <div class="body ledger townplan">
      <div class="lfacts"><div class="lfact"><span>Size</span><b>${esc(upFirst(tv?.sizeWord ?? t.size))}</b></div><div class="lfact"><span>Building plots</span><b>${t.plots.length}</b></div><div class="lfact"><span>Free plots</span><b>${t.free}</b></div><div class="lfact net"><span>In the treasury</span><b class="${have < 0 ? 'neg' : ''}">${inHand(have)}</b></div></div>
      <p class="sub">${story}</p>
      <h3>The plots <span class="inhand">${esc(r.name)}${own ? '' : ' · conquered ground'}</span></h3><div class="slots">${t.plots.map((p) => plotCell(t, p, true)).join('')}</div>
      ${t.mine ? `<h3>What can be built here <span class="inhand">one building goes up at a time</span></h3><div class="civrows">${t.offers.map(row).join('')}</div>` : ''}
    </div>
    <footer><button class="btn" data-act="ledgergo" data-id="${id}">Show on the map</button>${t.mine ? '<button class="btn" data-act="trade">Trade agreements</button>' : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

/** The Build tab's shelf for towns: what all the player's buildings bring in, and the best place for each kind of building now. */
function townsShelf(sel: Region | null): string {
  const me = g.human!;
  const keeps = keepsStores(g, me);
  const s = civicSum(g, me, keeps);
  const own = !!sel && sel.ctrl === me;
  const pct = (x: number): string => `+${Math.round(x * 1000) / 10}%`;
  const brings: string[] = [];
  if (s.commerce + s.farms + s.tax > 0) brings.push(`<div class="fact"><span>Money a month</span><b>${cash(s.commerce + s.farms + s.tax, true)}</b></div>`);
  if (s.sup > 0) brings.push(`<div class="fact"><span>Supplies a month</span><b>+${s.sup.toFixed(1)}</b></div>`);
  if (s.research > 0) brings.push(`<div class="fact"><span>Research</span><b>${pct(s.research)}</b></div>`);
  if (s.drafts > 0) brings.push(`<div class="fact"><span>Replacements</span><b>${pct(s.drafts)}</b></div>`);
  if (s.trade > 0) brings.push(`<div class="fact"><span>Worth of trade</span><b>${pct(s.trade)}</b></div>`);
  if (s.pc > 0) brings.push(`<div class="fact"><span>Political capital</span><b>+${s.pc.toFixed(2)}</b></div>`);
  const tips = buildingTips(g, 6);
  const tip = (x: BuildingTip): string => {
    const o = x.offer, can = o.why === null && humanUp(g);
    return `<button class="buy offer" data-act="civic" data-id="${x.region}" data-line="${o.line}" ${can ? '' : 'disabled'} title="${esc(o.blurb)}">${civicGlyph(o.line)}<span class="grow"><b>${esc(o.label)} at ${esc(x.town)}</b><small>${esc(o.effect)}</small><small class="where">${esc(o.name)}, level ${o.level} of 3 · ${months(o.months)}${x.town !== x.province ? ` · ${esc(x.province)}` : ''}</small>${o.why ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span><span class="cost${o.poor ? ' short' : ''}">${money(o.cost)}</span></button>`;
  };
  return `<h3>Build up your towns</h3><p class="sub" style="margin:0 0 7px">Every province has a chief place with building plots: one in a village, two in a town, three in a city, four in the capital. ${own ? `Open the town plan of <b style="color:var(--ink)">${esc(townIn(g, sel!.id)?.name ?? sel!.name)}</b> to choose for yourself, or take one of the staff's picks below.` : 'Click one of your provinces on the map and look under <b style="color:var(--ink)">The town</b>, or take one of the staff\'s picks below.'}</p>
    ${own ? `<div class="acts" style="margin-bottom:8px"><button class="btn small" data-act="town" data-id="${sel!.id}">The town plan of ${esc(townIn(g, sel!.id)?.name ?? sel!.name)}</button><button class="btn small" data-act="trade">Trade agreements</button></div>` : '<div class="acts" style="margin-bottom:8px"><button class="btn small" data-act="trade">Trade agreements</button></div>'}
    ${brings.length ? `<h3>What your ${s.count} building${s.count === 1 ? '' : 's'} bring${s.count === 1 ? 's' : ''}</h3><div class="facts brings">${brings.join('')}</div>` : ''}
    <h3>The staff's picks <span class="inhand">the best place for each kind</span></h3>${tips.map(tip).join('') || '<div class="empty">Every plot you hold is built on, or its builders are busy.</div>'}`;
}

/** The trade office: the agreements in force and what each pays, the trading houses, and every country that could be signed with. */
function tradeCard(nat: string): string {
  const b = tradeLedger(g, nat);
  const n = g.nations[nat];
  if (!b || !n) return '';
  const mine = nat === g.human;
  const up = mine && humanUp(g);
  const free = b.houses - b.used;
  const houses = `<span class="thouses">${Array.from({ length: b.houses }, (_, i) => `<i class="${i < b.used ? 'lit' : ''}"></i>`).join('')}</span>`;
  const deal = (d: TradeRow): string => {
    const ask = ui.modal?.ask === d.nat;
    const state = d.state === 'cut' ? ' <span class="tag bad">Cut</span>' : d.state === 'raided' ? ` <span class="tag bad">Raided: ${pc100(d.lost)} sunk</span>` : '';
    const btn = !mine ? '' : ask
      ? `<span class="tyes"><button class="btn small warn" data-act="tradeend" data-nat="${d.nat}">End it</button><button class="btn small" data-act="tradeask" data-nat="">Keep it</button></span>`
      : `<button class="btn small" data-act="tradeask" data-nat="${d.nat}" ${up ? '' : 'disabled'}>End</button>`;
    return `<div class="tdeal ts-${d.state}">${chip(d.nat)}<span class="grow"><b>${esc(d.name)}</b>${state}<small>${esc(d.route)} · signed ${date(d.since)}</small></span><span class="tpay"><b>${cash(d.pays, true)}</b><small>a month${d.state === 'raided' ? ` of ${cash(d.full)}` : ''}</small></span>${btn}</div>`;
  };
  const offer = (o: TradeOffer): string => {
    const can = o.why === null;
    return `<div class="tdeal${can || o.poor || o.full ? '' : ' off'}">${chip(o.nat)}<span class="grow"><b>${esc(o.name)}</b><small>${esc(o.route)}</small>${o.why && !o.full ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span><span class="tpay"><b>${cash(o.pays, true)}</b><small>a month</small></span><button class="btn small${can ? ' go' : ''}" data-act="tradesign" data-nat="${o.nat}" ${can && up ? '' : 'disabled'} title="Signing costs ${b.cost} political capital.">Sign</button></div>`;
  };
  const open = b.offers.filter((o) => o.why === null || o.poor || o.full);
  const shut = b.offers.filter((o) => !(o.why === null || o.poor || o.full));
  const all = !!ui.modal?.all;
  const note = !mine ? '' : free <= 0
    ? `<div class="note"><b>Every trading house is in use.</b> End an agreement to sign a better one from the list below, or raise a merchant quarter in a harbour town or in the capital: each one opens another trading house, up to ${b.most} in all.</div>`
    : open.some((o) => o.why === null) ? `<div class="note good"><b>${free} trading house${free === 1 ? ' stands' : 's stand'} idle.</b> Sign with the partner that pays most: it costs ${b.cost} political capital, and you have ${Math.floor(b.pc)}.</div>` : '';
  const story = 'Think of each trading house as a shop counter that serves one partner. A rich partner close by is worth more than a poor one far away, and both countries are paid every month. Goods that go by sea can be sunk on the way: a route raiders are working pays less, and one with no open sea pays nothing until the way is clear.';
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The trade office · ${date(g.turn)}</div><h2>${flagIcon(nat, n.color, 34, 'inh')} ${esc(mine ? 'Your trade agreements' : `The trade of ${theName(nat)}`)}</h2></header>
    <div class="body ledger tradebook">
      <div class="lfacts"><div class="lfact"><span>Trading houses in use</span><b>${b.used} of ${b.houses} ${houses}</b></div><div class="lfact net"><span>Trade pays, a month</span><b class="pos">${cash(b.income, true)}</b></div><div class="lfact"><span>From your commerce</span><b>+${Math.round(b.boost * 100)}%</b></div><div class="lfact"><span>Political capital</span><b>${Math.floor(b.pc)}</b></div></div>
      <p class="sub">${story}</p>${note}
      <h3>Agreements in force <span class="inhand">${b.deals.length ? 'the best first' : ''}</span></h3>
      <div class="tdeals">${b.deals.map(deal).join('') || '<div class="empty">No agreements are in force.</div>'}</div>
      ${mine ? `<h3>Countries that would sign <span class="inhand">${open.length ? `signing costs ${b.cost} political capital` : ''}</span></h3>
      <div class="tdeals">${open.map(offer).join('') || '<div class="empty">Nobody will sign just now.</div>'}</div>
      ${shut.length ? `<div class="acts"><button class="btn small" data-act="tradeall">${all ? 'Hide the countries that will not sign' : `Show the ${shut.length} that will not sign now, and why`}</button></div>${all ? `<div class="tdeals">${shut.map(offer).join('')}</div>` : ''}` : ''}` : ''}
      <p class="sub" style="margin-top:8px">Your routes are drawn on the map in the wealth view, the coin button beside the zoom.</p>
    </div>
    <footer><button class="btn" data-act="ledger"${mine ? '' : ` data-nat="${nat}"`}>Open the ledger</button><button class="btn go" data-act="close">Close</button></footer>`;
}

/** A line through points, eased into a curve. */
function easedPath(p: [number, number][]): string {
  let d = `M${p[0][0].toFixed(1)} ${p[0][1].toFixed(1)}`;
  for (let i = 0; i < p.length - 1; i++) {
    const a = p[i - 1] ?? p[i], b = p[i], c = p[i + 1], e = p[i + 2] ?? c;
    d += `C${(b[0] + (c[0] - a[0]) / 6).toFixed(1)} ${(b[1] + (c[1] - a[1]) / 6).toFixed(1)} ${(c[0] - (e[0] - b[0]) / 6).toFixed(1)} ${(c[1] - (e[1] - b[1]) / 6).toFixed(1)} ${c[0].toFixed(1)} ${c[1].toFixed(1)}`;
  }
  return d;
}

/** Trade routes for the wealth view: the player's agreements as gold lines from town to town, amber where raiders are sinking the goods. */
function tradeMarks(): string {
  if (!tradeRules(g)) return '';
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  let out = '';
  for (const d of g.trade) {
    if (me && d.a !== me && d.b !== me) continue;
    const p = tradePath(g, d);
    if (p.length < 2) continue;
    const path = easedPath(p);
    const k = Math.floor((p.length - 1) / 2);
    const lx = (p[k][0] + p[k + 1][0]) / 2, ly = (p[k][1] + p[k + 1][1]) / 2;
    const them = me ? (d.a === me ? d.b : d.a) : '';
    const label = me ? `<text class="troutelbl" x="${lx.toFixed(1)}" y="${(ly - 7).toFixed(1)}">${esc(natName(them))} ${cash(d.worth * (1 - d.lost), true)}</text>` : '';
    const ends = [p[0], p[p.length - 1]].map(([x, y]) => `<circle class="trouteend" cx="${x.toFixed(1)}" cy="${y.toFixed(1)}" r="3.4"/>`).join('');
    out += `<g class="troute ts-${d.state}${d.way === 'land' ? ' byland' : ''}"><path class="troutebed" d="${path}"/><path class="trouteline" d="${path}"/>${ends}${label}</g>`;
  }
  return out;
}

// ---- Research -----------------------------------------------------------------------------

/** The last month the research timelines run to: December 1945. */
const WAR_ENDS = 76;
const PACE = ['Nobody at work: this branch stands still', 'One team: keeps to history\'s schedule', 'Two teams: half as fast again', 'Three teams: nearly twice the pace'];

/** One branch as a strip of the whole war: history's dates as marks, the designers' progress as a gold bar, today as a white line. */
function labTrack(nat: string, f: Field): string {
  const lab = g.nations[nat].lab!;
  const pos = (t: number): string => `${(Math.max(0, Math.min(WAR_ENDS, t)) / WAR_ENDS * 100).toFixed(1)}%`;
  const ticks = stepsOf(nat, f).filter((st) => st.at > 0).map((st) => `<i class="${st.at <= lab.clock[f] ? 'got' : ''}" style="left:${pos(st.at)}" title="${esc(st.name)}: ${date(st.at)} in history"></i>`).join('');
  return `<div class="labtrack"><span class="fill" style="width:${pos(lab.clock[f])}"></span>${ticks}<span class="now" style="left:${pos(lab.months)}" title="History today: ${date(Math.max(1, lab.months))}"></span></div>`;
}

function labRow(nat: string, f: Field, mine: boolean): string {
  const lab = g.nations[nat].lab!;
  const spec = RESEARCH.fields[f];
  const cur = inService(nat, lab, f), next = nextStep(nat, lab, f);
  const ahead = Math.round(monthsAhead(lab, f));
  const tag = ahead > 0 ? `<span class="tag good">${ahead} month${ahead === 1 ? '' : 's'} ahead</span>` : ahead < 0 ? `<span class="tag bad">${-ahead} month${ahead === -1 ? '' : 's'} behind</span>` : `<span class="tag">On history's schedule</span>`;
  const eta = arrival(g, nat, f);
  const k = lead(g, nat, f);
  const pct = Math.round(spec.per * k * 100);
  const n = lab.teams[f];
  const free = teamsFree(lab);
  let out = `<div class="lab"><div class="lab-h"><b>${esc(f === 'doctrine' ? `${spec.label}: ${doctrineOf(nat)}` : spec.label)}</b>${tag}</div>
    <div class="lab-now">In service: <b>${esc(cur?.name ?? 'nothing yet')}</b></div>
    <div class="lab-next">${next ? `Next: ${esc(next.name)}. History had it in ${date(next.at)}; ${eta === null ? 'at this pace it never comes' : `yours is due ${date(eta)}`}.` : 'The designers have nothing further on the drawing board.'}</div>
    ${labTrack(nat, f)}`;
  if (k) out += `<div class="lab-fx ${k > 0 ? 'good' : 'bad'}">${esc(spec.who)}: ${pct > 0 ? '+' : ''}${pct}% while these designs are ${Math.abs(k) === 1 ? 'a generation' : `${Math.abs(k)} generations`} ${k > 0 ? 'ahead of' : 'behind'} history.</div>`;
  out += `<div class="lab-t"><button class="btn small" data-act="teams" data-field="${f}" data-n="${n - 1}" ${mine && n > 0 ? '' : 'disabled'} aria-label="One team fewer on ${esc(spec.label)}">−</button><span class="teampips">${Array.from({ length: MAX_TEAMS }, (_, i) => `<i class="${i < n ? 'on' : ''}"></i>`).join('')}</span><button class="btn small" data-act="teams" data-field="${f}" data-n="${n + 1}" ${mine && n < MAX_TEAMS && free > 0 ? '' : 'disabled'} aria-label="One team more on ${esc(spec.label)}">+</button><small>${PACE[n]}</small></div></div>`;
  return out;
}

/** What one power has in service, branch by branch. */
function labSummary(nat: string): string {
  const lab = g.nations[nat].lab!;
  return `<table class="ticket">${FIELDS.map((f) => `<tr><td>${esc(RESEARCH.fields[f].label)}</td><td>${esc(inService(nat, lab, f)?.name ?? '')}</td></tr>`).join('')}</table>`;
}

function researchPane(): string {
  // The grand campaign steers research through the technology tree.
  if (isOpen(g)) return techPane();
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const majors = Object.values(g.nations).filter((n) => n.lab && n.alive && n.id !== me).sort((a, b) => fieldArmy(g, b.id) - fieldArmy(g, a.id));
  const others = majors.map((n) => `<h3>${esc(n.name)}${doctrineOf(n.id) ? ` · ${esc(doctrineOf(n.id))}` : ''}</h3>${labSummary(n.id)}`).join('');
  if (!me || !g.nations[me].lab) {
    return `<h2>Research</h2><p class="sub">The weapons of every great power improve as the war goes on, each new design arriving in the month history brought it into service. ${me ? `${esc(natName(me))} has no design offices of its own and buys its weapons abroad.` : 'A power that pushes one branch hard gets its designs early; one that neglects a branch falls behind.'}</p>${others}`;
  }
  const n = g.nations[me], lab = n.lab!;
  const mine = humanUp(g);
  const free = teamsFree(lab);
  const cost = bureauCost(n);
  let out = `<h2>Research</h2><p class="sub">Every design team keeps one branch of the armed forces on history's schedule. Put a second team on a branch and its next designs arrive early; take the team away and the branch falls behind. In battle a design counts for as long as it is ahead of its time.</p>
    <div class="facts"><div class="fact"><span>Design teams</span><b>${teamsTotal(lab)}</b></div><div class="fact"><span>Unassigned</span><b>${free}</b></div><div class="fact"><span>Doctrine</span><b style="font-size:13px">${esc(doctrineOf(me))}</b></div></div>
    <div class="trackkey"><span><i class="k fill"></i>Your designers</span><span><i class="k now"></i>History today</span><span><i class="k tick"></i>A design's date in history</span></div>`;
  if (free > 0) out += `<div class="note"><b>${free} design team${free === 1 ? ' is' : 's are'} idle.</b> Put ${free === 1 ? 'it' : 'them'} to work with a + button below.</div>`;
  for (const f of FIELDS) out += labRow(me, f, mine);
  out += `<h3>More design teams</h3>`;
  out += cost === null ? `<div class="empty">The nation has all the design teams its engineers can staff.</div>`
    : `<button class="buy" data-act="bureau" ${mine && n.pp >= cost ? '' : 'disabled'}><span class="grow"><b>Found a new design team</b><small>One more team to put on any branch, for the rest of the war.</small><small>Paid for from the treasury: ${money(n.pp)} in hand.</small></span><span class="cost${n.pp >= cost ? '' : ' short'}">${money(cost)}</span></button>`;
  if (others) out += `<h3 style="margin-top:20px">What the other powers have in service</h3>${others.replace(/<h3>/g, '<h3 style="margin-top:10px">')}`;
  return out;
}

// ---- The technology tree (the grand campaign) ------------------------------------------------------

const TIER = ['', 'I', 'II', 'III', 'IV'];
const LOCK = '<svg class="tlock" width="10" height="12" viewBox="0 0 10 12" aria-hidden="true"><path d="M2.5 5V3.5a2.5 2.5 0 0 1 5 0V5" fill="none" stroke="currentColor" stroke-width="1.4"/><rect x="1" y="5" width="8" height="6" rx="1" fill="currentColor"/></svg>';
/** A small sign for each branch of the tree, drawn in the ink of whatever it sits on. */
const BRANCH_SIGN: Record<string, string> = {
  land: '<path d="M2 11h14l2-3-2-3H6L4.5 2.5H2Z M5 13.5h9" />',
  air: '<path d="M10 1.5c1 0 1.4 1.2 1.4 3v2.2l6.6 4v1.6l-6.6-2v3.4l2 1.5v1.3L10 15.6l-3.4.9v-1.3l2-1.5v-3.4l-6.6 2v-1.6l6.6-4V4.5c0-1.8.4-3 1.4-3Z"/>',
  naval: '<path d="M1.5 10.5h17l-2.5 5h-12Z M6 10V6.5h3V4h2.5v2.5H14V10"/>',
  industry: '<path d="M2 16.5V8l5 3V8l5 3V3.5h4.5v13Z"/>',
  doctrine: '<path d="M10 2 12.4 7.6 18.5 8 13.8 11.9 15.3 17.8 10 14.6 4.7 17.8 6.2 11.9 1.5 8 7.6 7.6Z"/>',
};
const branchSign = (id: string): string => `<svg class="bsign" width="18" height="18" viewBox="0 0 20 20" aria-hidden="true">${BRANCH_SIGN[id] ?? ''}</svg>`;
const monthsWord = (k: number): string => `${k} month${k === 1 ? '' : 's'}`;

/** What a technology's card says under its name. */
function nodeLine(x: TechNode): string {
  if (x.state === 'done') return 'Researched';
  if (x.state === 'work') return `${monthsWord(x.left)} to go`;
  if (x.state === 'locked') return `Needs ${x.needs.filter((y) => !y.done).map((y) => y.name).join(' and ')}`;
  return x.months > x.base ? `${monthsWord(x.months)} · ahead of its time` : monthsWord(x.months);
}

/** The Research tab in the grand campaign: what the teams are doing, what they could begin, and the door to the tree. */
function techPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const others = Object.values(g.nations).filter((n) => n.alive && n.id !== me && (TECH.slots[n.id] ?? 0) >= 2).sort((a, b) => fieldArmy(g, b.id) - fieldArmy(g, a.id));
  const table = others.length ? `<table class="ticket techtable"><tr><th></th>${TECH.branches.map((b) => `<th title="${esc(b.label)}">${branchSign(b.id)}</th>`).join('')}</tr>${others.map((n) => {
    const v = techView(g, n.id)!;
    return `<tr><td>${flagIcon(n.id, n.color, 18)} ${esc(n.name)}</td>${v.branches.map((b) => `<td>${b.done}<small>/${b.total}</small></td>`).join('')}</tr>`;
  }).join('')}</table>` : '';
  const v = me ? techView(g, me) : null;
  if (!v) return `<h2>Research</h2><p class="sub">Every nation's research teams work through a tree of technologies in five branches: land, air, naval, industry and doctrine. What a nation finishes lasts for the rest of the war.</p><div class="acts" style="margin-top:8px"><button class="btn small" data-act="tech">See the technology tree</button></div><h3 style="margin-top:14px">What the great powers have researched</h3>${table}`;
  const mine = humanUp(g);
  const free = v.teams - v.busy;
  const work = v.nodes.filter((x) => x.state === 'work');
  const ready = v.nodes.filter((x) => x.state === 'ready');
  const lane = (id: string): string => TECH.branches.find((b) => b.id === id)?.label ?? id;
  let out = `<h2>Research</h2><p class="sub">Each research team works on one technology at a time. What it finishes lasts for the rest of the war: better weapons, new kinds of formation, cheaper ships, more teams.</p>
    <div class="facts"><div class="fact"><span>Research teams</span><b>${v.teams}</b></div><div class="fact"><span>At work</span><b>${v.busy}</b></div><div class="fact"><span>Researched</span><b>${v.done} of ${v.total}</b></div></div>`;
  if (free > 0 && ready.length) out += `<div class="note"><b>${free === 1 ? 'One research team has' : `${free} research teams have`} nothing to do.</b> Pick ${free === 1 ? 'its' : 'their'} next work from the list below, or from the tree.</div>`;
  out += `<div class="acts" style="margin-top:10px"><button class="btn go" data-act="tech">Open the technology tree</button></div>`;
  out += `<h3 style="margin-top:14px">In the laboratories</h3>`;
  out += work.length ? work.map((x) => `<button class="buy techrow" data-act="tech" data-id="${x.id}">${branchSign(x.branch)}<span class="grow"><b>${esc(x.name)}</b><small>${esc(x.rule)}</small><span class="tbar"><i style="width:${Math.round((1 - x.left / Math.max(1, x.of)) * 100)}%"></i></span></span><span class="cost">${monthsWord(x.left)}</span></button>`).join('') : `<div class="empty">No team is at work.</div>`;
  const row = (x: TechNode): string => `<button class="buy techrow" data-act="tech" data-id="${x.id}" ${mine ? '' : 'disabled'}>${branchSign(x.branch)}<span class="grow"><b>${esc(x.name)}</b><small>${esc(lane(x.branch))} · step ${TIER[x.tier]}${x.months > x.base ? ` · ${monthsWord(x.base)} from ${x.year}` : ''}</small><small>${esc(x.rule)}</small></span><span class="cost">${monthsWord(x.months)}</span></button>`;
  const now = ready.filter((x) => x.months <= x.base), early = ready.filter((x) => x.months > x.base);
  if (now.length) out += `<h3 style="margin-top:14px">Open to begin</h3>${now.map(row).join('')}`;
  if (early.length) out += `<h3 style="margin-top:14px">Ahead of their time</h3><p class="sub">These can be begun now, but each takes half as long again for every year it is ahead of its time.</p>${early.map(row).join('')}`;
  if (table) out += `<h3 style="margin-top:18px">What the other powers have researched</h3>${table}`;
  return out;
}

/** The whole tree on one card: a lane for each branch, a column for each step, and the chosen technology spelled out underneath. */
function techCard(pick?: string): string {
  const nat = g.human && g.nations[g.human] ? g.human : 'GER';
  const v = techView(g, nat);
  if (!v) return '';
  const n = g.nations[nat];
  const mine = nat === g.human && humanUp(g);
  const free = v.teams - v.busy;
  const sel = v.nodes.find((x) => x.id === pick) ?? null;
  const node = (x: TechNode): string => `<button class="tnode ${x.state}${x.state === 'ready' && x.months > x.base ? ' early' : ''}${sel && sel.id === x.id ? ' sel' : ''}" id="tn-${x.id}" data-act="tech" data-id="${x.id}" aria-pressed="${!!sel && sel.id === x.id}"><b>${x.state === 'done' ? '<i class="tick">✓</i>' : x.state === 'locked' ? LOCK : ''}${esc(x.name)}</b><small>${esc(nodeLine(x))}</small>${x.state === 'work' ? `<span class="tbar"><i style="width:${Math.round((1 - x.left / Math.max(1, x.of)) * 100)}%"></i></span>` : ''}</button>`;
  const lanes = v.branches.map((b) => `<section class="tlane"><div class="tlanehead">${branchSign(b.id)}<b>${esc(b.label)}</b><small>${esc(b.text)}</small><span>${b.done} of ${b.total}</span></div><div class="tcols">${[1, 2, 3, 4].map((tier) => `<div class="tcol">${v.nodes.filter((x) => x.branch === b.id && x.tier === tier).map(node).join('')}</div>`).join('')}</div></section>`).join('');
  const eras = `<div class="tcols teras">${[1, 2, 3, 4].map((tier) => `<div><b>${TIER[tier]}</b> ${esc(v.eras[tier])}</div>`).join('')}</div>`;
  let foot: string;
  if (!sel) {
    foot = `<div class="techsel"><div class="grow"><small>Click a technology to see what it does and what it needs. ${g.human ? (free > 0 ? `${free === 1 ? 'One team is' : `${free} teams are`} free for new work.` : 'Every team is at work.') : ''}</small></div><button class="btn" data-act="close">Close</button></div>`;
  } else {
    const lane = v.branches.find((b) => b.id === sel.branch)!;
    const needs = sel.needs.length ? `<small class="tneeds">Needs ${sel.needs.map((y) => `<span class="${y.done ? 'got' : 'not'}">${y.done ? '✓' : '✗'} ${esc(y.name)}</span>`).join(' ')}</small>` : '';
    const time = sel.months > sel.base ? `${monthsWord(sel.months)} now: it is ahead of its time. ${monthsWord(sel.base)} from ${sel.year}.` : `${monthsWord(sel.months)} of one team's work.`;
    let act = '';
    if (sel.state === 'done') act = `<span class="tag good">Researched</span>`;
    else if (sel.state === 'work') act = `<span class="tag">${monthsWord(sel.left)} to go</span>${mine ? `<button class="btn small warn" data-act="stopresearch" data-id="${sel.id}" title="The team is free at once. The work done so far is lost.">Stop this work</button>` : ''}`;
    else if (g.human && nat === g.human) act = `<button class="btn go" data-act="research" data-id="${sel.id}" ${mine && sel.why === null ? '' : 'disabled'}>Research · ${monthsWord(sel.months)}</button>`;
    const why = sel.state !== 'done' && sel.state !== 'work' && sel.why && g.human ? `<small class="whyno">${esc(sel.why)}</small>` : '';
    foot = `<div class="techsel"><div class="grow"><b>${esc(sel.name)}</b> <span class="tag">${esc(lane.label)} · step ${TIER[sel.tier]}</span><p class="flavor">${esc(sel.text)}</p><p class="rule">${esc(sel.rule)}</p>${needs}${sel.state === 'ready' || sel.state === 'locked' ? `<small>${esc(time)}</small>` : ''}${why}</div><div class="techacts">${act}<button class="btn small" data-act="close">Close</button></div></div>`;
  }
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Research</div><h2>${flagIcon(nat, n.color, 30)} The technology tree</h2><p class="sub">${v.teams} research team${v.teams === 1 ? '' : 's'}, ${v.busy} at work · ${v.done} of ${v.total} technologies researched. A technology opens when the ones before it are done; lines show what leads to what.</p></header>
    <div class="body techbody"><div class="techscroll"><div class="techgrid"><svg class="techlines" aria-hidden="true"></svg>${eras}${lanes}</div></div></div>
    <footer class="techfoot">${foot}</footer>`;
}

/** Draws the lines of the tree, from each technology to the ones it opens, once the cards are laid out. */
function drawTechLines(): void {
  const grid = document.querySelector('.techgrid') as HTMLElement | null;
  const svg = grid?.querySelector('svg.techlines') as SVGSVGElement | null;
  const nat = g.human && g.nations[g.human] ? g.human : 'GER';
  const v = techView(g, nat);
  if (!grid || !svg || !v || ui.modal?.kind !== 'tech') return;
  const base = grid.getBoundingClientRect();
  const sel = ui.modal.id as string | undefined;
  const byId = new Map(v.nodes.map((x) => [x.id, x]));
  const at = (id: string): DOMRect | null => (grid.querySelector(`#tn-${id}`) as HTMLElement | null)?.getBoundingClientRect() ?? null;
  const paths: string[] = [];
  for (const x of v.nodes) {
    for (const y of x.needs) {
      const a = at(y.id), b = at(x.id);
      if (!a || !b) continue;
      const x1 = a.right - base.left, y1 = a.top + a.height / 2 - base.top, x2 = b.left - base.left, y2 = b.top + b.height / 2 - base.top;
      const k = Math.max(10, Math.min(36, (x2 - x1) / 2));
      const cls = [sel && (sel === x.id || sel === y.id) ? 'hot' : '', y.done && x.state !== 'locked' ? 'open' : '', byId.get(y.id)?.branch !== x.branch ? 'cross' : ''].filter(Boolean).join(' ');
      paths.push(`<path class="${cls}" d="M${x1.toFixed(1)} ${y1.toFixed(1)}C${(x1 + k).toFixed(1)} ${y1.toFixed(1)} ${(x2 - k).toFixed(1)} ${y2.toFixed(1)} ${x2.toFixed(1)} ${y2.toFixed(1)}"/>`);
    }
  }
  svg.setAttribute('width', String(grid.scrollWidth));
  svg.setAttribute('height', String(grid.scrollHeight));
  // The chosen technology's lines are drawn last, on top of the rest.
  svg.innerHTML = paths.filter((p) => !p.includes('hot')).join('') + paths.filter((p) => p.includes('hot')).join('');
}

/** Opens the tree, or redraws it where it stands when it is already open, so a click does not throw the page back to the top. */
function showTech(id?: string): void {
  const open = ui.modal?.kind === 'tech';
  const sc = document.querySelector('.techscroll') as HTMLElement | null;
  const keep = open ? { x: sc?.scrollLeft ?? 0, y: $('#card').scrollTop } : null;
  let pick = id;
  if (!pick && !open && g.human) {
    // Opened from the top: start on what a free team could begin, else on what is in work.
    const v = techView(g, g.human);
    const free = v ? v.teams - v.busy : 0;
    const ready = v?.nodes.filter((x) => x.state === 'ready') ?? [];
    pick = (free > 0 ? ready.find((x) => x.months <= x.base) ?? ready[0] : undefined)?.id ?? v?.nodes.find((x) => x.state === 'work')?.id;
  }
  openModal({ kind: 'tech', id: pick });
  const now = document.querySelector('.techscroll') as HTMLElement | null;
  if (keep) { $('#card').scrollTop = keep.y; if (now) now.scrollLeft = keep.x; }
  else if (pick) (document.querySelector(`#tn-${pick}`) as HTMLElement | null)?.scrollIntoView({ block: 'center', inline: 'center' });
  drawTechLines();
}

// ---- Generals -----------------------------------------------------------------------------

const GONE: Record<string, string> = { captured: 'Prisoner of war', dead: 'Killed in action', sacked: 'Dismissed' };

function genWhere(x: General): string {
  if (x.status === 'wounded') return `Wounded. Fit again in ${date(x.backOn)}`;
  if (x.status !== 'active') return GONE[x.status];
  const c = corpsOf(g, x);
  return c ? `${esc(c.name)} · ${KIND[c.kind].label.toLowerCase()} · ${esc(g.regions[c.reg].name)}` : 'Waiting for a command';
}

function traitTags(x: General): string {
  return x.traits.map((t) => {
    const d = COMMAND.traits[t];
    return d ? `<span class="tag ${d.good ? 'good' : 'bad'}" title="${esc(d.effect)}">${esc(d.label)}</span>` : '';
  }).join('');
}

function genRow(x: General, act = 'general', extra = '', off = ''): string {
  const wanted = x.nat === g.human && headWantedOf(g) === x.id ? '<span class="tag bad" title="The war cabinet asks you to dismiss him. Answer it in the cabinet room.">The cabinet wants him dismissed</span>' : '';
  const pts = (x.nat === g.human && x.points > 0 && serving(x) ? `<span class="tag pts">${x.points} skill point${x.points > 1 ? 's' : ''}</span>` : '') + wanted;
  const whose = x.nat === g.human ? '' : `${g.nations[x.nat].adj} · `;
  return `<button class="gen${serving(x) ? '' : ' gone'}" data-act="${act}" data-id="${x.id}" ${extra} ${off ? `disabled title="${esc(off)}"` : ''}><span class="lv">${x.level}</span><span class="grow"><b>${esc(x.name)}</b><small>${whose}${esc(rankOf(x))} · ${genWhere(x)}</small>${x.traits.length || pts ? `<span class="tags">${pts}${traitTags(x)}</span>` : ''}</span></button>`;
}

function generalsPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const rank = (a: General, b: General): number => b.level - a.level || b.rec.won - a.rec.won || a.id - b.id;
  let out = '';
  if (me) {
    const mine = g.generals.filter((x) => x.nat === me);
    const free = waiting(g, me);
    const led = mine.filter((x) => x.status === 'active' && x.corps >= 0).sort(rank);
    const gone = mine.filter((x) => x.status !== 'active').sort((a, b) => (serving(b) ? 1 : 0) - (serving(a) ? 1 : 0) || rank(a, b));
    out += `<h2>Your generals</h2><p class="sub">Generals gain experience in battle, and every level brings a skill point to spend. Traits are earned from how a general actually fights. Open one to see his service record or give him a command.</p>
      <div class="facts"><div class="fact"><span>In command</span><b>${led.length}</b></div><div class="fact"><span>Waiting</span><b>${free.length}</b></div><div class="fact"><span>Corps without</span><b>${vacant(g, me).length}</b></div></div>`;
    if (free.length) out += `<h3>Waiting for a command</h3>${free.map((x) => genRow(x)).join('')}`;
    out += `<h3>In command</h3>${led.length ? led.map((x) => genRow(x)).join('') : '<div class="empty">No corps has a general yet.</div>'}`;
    if (gone.length) out += `<h3>Out of action</h3>${gone.map((x) => genRow(x)).join('')}`;
    out += `<div class="note"><b>A corps with no general fights by the book.</b> Pick one of your corps on the map and press <b>Appoint a general</b> to give it one who is waiting, or to promote an unknown officer from its ranks for ${COMMAND.promote.pc} political capital.</div>`;
  }
  const others = g.generals.filter((x) => x.nat !== me && serving(x) && (x.rec.battles > 0 || x.level >= 3)).sort(rank).slice(0, me ? 8 : 40);
  out += me ? '<h3>Other commanders of the war</h3>' : '<h2>Commanders of the war</h2><p class="sub">The generals who have made a name so far. They gain experience in battle, earn traits from how they fight, and can be wounded, captured or killed. Open one to see his service record.</p><div style="height:8px"></div>';
  out += others.length ? others.map((x) => genRow(x)).join('') : '<div class="empty">No one has made a name yet.</div>';
  return out;
}

/** A general's service record, and the decisions the player can take about him. */
function generalCard(x: General): string {
  const m = ui.modal!;
  const n = g.nations[x.nat];
  const mine = x.nat === g.human && humanUp(g);
  const c = corpsOf(g, x);
  const need = xpToNext(x);
  const times = (k: number): string => (k === 1 ? 'once' : k === 2 ? 'twice' : `${k} times`);
  const traits = x.traits.map((t) => {
    const d = COMMAND.traits[t];
    return d ? `<li class="${d.good ? 'plus' : 'minus'}"><b>${d.good ? '+' : '−'} ${esc(d.label)}</b><span>${esc(d.effect)}</span><small>${esc(d.earned)}</small></li>` : '';
  }).join('');
  const skills = SKILLS.map((k) => {
    const d = COMMAND.skills[k], v = x.skills[k];
    const can = mine && serving(x) && x.points > 0 && v < COMMAND.skillMax;
    const dots = Array.from({ length: COMMAND.skillMax }, (_, i) => `<i class="${i < v ? '' : 'off'}"></i>`).join('');
    return `<div class="skill"><b>${d.label}</b><span class="dots" title="${v} of ${COMMAND.skillMax}">${dots}</span>${can ? `<button class="btn small" data-act="skill" data-id="${x.id}" data-skill="${k}" title="Spend a skill point on ${d.label.toLowerCase()}">+ 1</button>` : '<span></span>'}<small>${esc(d.blurb)}</small></div>`;
  }).join('');
  let orders = '';
  if (mine && x.status === 'active') {
    const one = pickedCorps().length === 1 ? pickedCorps()[0] : null;
    const open = vacant(g, x.nat).sort((a, b) => fit(g, x, b) - fit(g, x, a));
    const list = [...(one && one.id !== x.corps && !open.includes(one) ? [one] : []), ...open].slice(0, 6);
    const row = (t: Corps): string => {
      const why = whyNotAssign(g, x, t);
      const sitting = generalOf(g, t);
      return `<button class="pickrow" data-act="assign" data-id="${x.id}" data-corps="${t.id}" ${why ? `disabled title="${esc(why)}"` : ''}>${icon(t)}<span class="grow"><b>${esc(t.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${KIND[t.kind].label} · ${t.divs} division${t.divs > 1 ? 's' : ''} · at ${esc(g.regions[t.reg].name)}${sitting ? ` · takes over from ${esc(sitting.name)}` : ''}</span></span></button>`;
    };
    if (x.moved === g.turn) orders = `<h3>${c ? 'Another command' : 'A command'}</h3><div class="empty">He took up a new command this month. He can be moved again next month.</div>`;
    else orders = `<h3>${c ? 'Give him another command' : 'Give him a command'}</h3>${list.length ? list.map(row).join('') : '<div class="empty">Every corps has a general. Pick one corps on the map first to put him in another man\'s place.</div>'}`;
  }
  const state = x.status === 'active' ? (c ? `Assigned: ${esc(c.name)}, ${esc(g.regions[c.reg].name)}` : 'Waiting for a command') : genWhere(x);
  const hv = mine && headWantedOf(g) === x.id ? governmentOf(g)?.head ?? null : null;
  const confirm = m.confirm
    ? `<div class="note" style="border-left-color:var(--red)"><b>Dismiss ${esc(x.name)} for good?</b> ${hv ? `The war cabinet asked for it: its trust in you rises by ${hv.gain}. ${popular(x) ? `The army thinks well of him${c ? ', and his corps will lose cohesion' : ''}; the cabinet carries the political cost. ` : ''}He will not come back.` : popular(x) ? `The army thinks well of him. ${c ? 'His corps will lose cohesion, and it' : 'It'} will cost ${COMMAND.sack.pc} political capital.` : 'He will not come back.'}</div>`
    : hv ? `<div class="note warn"><b>The war cabinet wants him dismissed.</b> ${esc(hv.why)} Dismiss him and its trust rises by ${hv.gain}; stand by him and it falls by ${hv.cost}, unless his next battle proves the cabinet wrong. <button class="btn small" data-act="cabinet">Answer in the cabinet room</button></div>` : '';
  const foot = m.confirm
    ? `<button class="btn" data-act="keep" data-id="${x.id}">Keep him</button><button class="btn go" data-act="sack" data-id="${x.id}">Dismiss him</button>`
    : `${mine && serving(x) ? `<button class="btn" data-act="sack" data-id="${x.id}">Dismiss</button>` : ''}${mine && x.status === 'active' && c ? `<button class="btn" data-act="relieve" data-id="${x.id}" title="Take him off his corps and put him back on the list">Relieve of command</button>` : ''}${c ? `<button class="btn" data-act="showmap" data-id="${c.reg}">Show on map</button>` : ''}<button class="btn go" data-act="close">Close</button>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${n.adj} army · ${esc(rankOf(x))} · ${x.historical ? 'Historical' : 'Rose from the ranks'}</div><h2>${esc(x.name)}</h2></header>
    <div class="body">
      <div class="record"><div class="lvbig"><b>${x.level}</b><span>LEVEL</span></div><div class="who"><b>${state}</b>
        <div class="xp"><i style="width:${need === Infinity ? 100 : Math.round((x.xp / need) * 100)}%"></i></div>
        <small>${need === Infinity ? 'At the highest level' : `XP ${num(x.xp)} / ${num(need)} · Next level brings a skill point`}${x.points > 0 && serving(x) ? ` · <b style="color:var(--brass)">${x.points} to spend</b>` : ''}</small></div></div>
      <h3>Traits</h3>${traits ? `<ul class="traits">${traits}</ul>` : '<div class="empty">None yet. Traits are earned from how a general fights: closing pockets, holding against the odds, costly assaults.</div>'}
      <h3>Skills</h3>${skills}
      <div class="tally">Battles ${x.rec.battles} · Won ${x.rec.won}${x.rec.wounds ? ` · Wounded ${times(x.rec.wounds)}` : ''}</div>
      ${rankLadder(x)}
      ${orders}${confirm}
    </div><footer>${foot}</footer>`;
}

/** Choosing a general for one corps: a man from the waiting list, or an unknown officer from its own ranks. */
function appointCard(c: Corps): string {
  const free = waiting(g, c.nat).sort((a, b) => fit(g, b, c) - fit(g, a, c) || b.level - a.level);
  const why = whyNotPromote(g, c);
  const pc = Math.floor(g.nations[c.nat].pc);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Command</div><h2>A general for ${esc(c.name)}</h2></header>
    <div class="body"><p class="lead">${g.nations[c.nat].adj} ${KIND[c.kind].label.toLowerCase()}, ${c.divs} division${c.divs > 1 ? 's' : ''}, at ${esc(g.regions[c.reg].name)}. A general adds his skills and traits to every battle this corps fights.</p>
      <h3>Generals waiting for a command</h3>
      ${free.length ? free.map((x) => genRow(x, 'assign', `data-corps="${c.id}"`, whyNotAssign(g, x, c))).join('') : '<div class="empty">No general is waiting for a command.</div>'}
      <h3>Promote from the ranks</h3>
      <button class="buy" data-act="promote" data-id="${c.id}" ${why ? `disabled title="${esc(why)}"` : ''}><span class="grow"><b>Promote an officer of this corps</b><small>An unknown officer takes command at level 1. He is a rising star and learns faster than the old guard.</small><small>You have ${pc} political capital.</small></span><span class="cost${why ? ' short' : ''}">${COMMAND.promote.pc}<small>capital</small></span></button>
    </div><footer><button class="btn go" data-act="close">Close</button></footer>`;
}

// ---- Unit stories: ribbons, service records, honours and the memorial wall -------------------

type AnyUnit = (Corps | AirFleet | Fleet) & Storied;

const andList = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const serviceOf = (kind: UnitKind): string => (kind === 'corps' ? 'army' : kind === 'air' ? 'air force' : 'navy');
const placeOf = (kind: UnitKind, at: number): string => (at < 0 ? '' : kind === 'fleet' ? g.seas[at]?.name ?? '' : g.regions[at]?.name ?? '');

/** What has become of an honour, in a few words. */
function awardState(a: Award): string {
  if (a.old) return 'carried from the corps that bore the name before';
  if (a.cash === 'bonds') return `sent home on a bond tour, ${date(a.cashed ?? a.turn)}`;
  if (a.cash === 'school') return `its veterans went to the schools, ${date(a.cashed ?? a.turn)}`;
  return 'carried at the front';
}

/** One honour as a ribbon: two colours, as on a medal bar. Faded once the fame has been spent at home. */
function ribbonOf(a: Award, big = false): string {
  const d = honourDef(a.hon);
  if (!d) return '';
  return `<i class="rib${big ? ' big' : ''}${a.old ? ' old' : a.cash ? ' home' : ''}" style="--a:${d.ribbon[0]};--b:${d.ribbon[1]}" title="${esc(awardTitle(g, a))}, ${date(a.turn)}: ${esc(awardState(a))}"></i>`;
}

/** A unit's ribbons in a row, newest last. */
function ribbons(u: Storied, most = 8): string {
  const hs = honoursOf(g, u);
  if (!hs.length) return '';
  return `<span class="ribs">${hs.slice(-most).map((a) => ribbonOf(a)).join('')}${hs.length > most ? `<small>+${hs.length - most}</small>` : ''}</span>`;
}

/** What an honour adds in battle while it is carried at the front. */
function edgeText(a: Award): string {
  const e = honourDef(a.hon)?.edge;
  const pc = (v: number): number => Math.round(v * 100);
  const parts: string[] = [];
  if (e?.attack) parts.push(`attacks ${pc(e.attack)}% stronger`);
  if (e?.defence) parts.push(`defends ${pc(e.defence)}% stronger`);
  if (e?.all) parts.push(`fights ${pc(e.all)}% stronger`);
  return parts.length ? andList(parts) : '';
}

/** The tags a corps wears for its story: its designation, leave at home, drafts refused, an old enemy, a home in enemy hands. */
function storyTags(c: Corps): string[] {
  const out: string[] = [];
  const mine = c.nat === g.human;
  if (isGuards(g, c)) out.push(`<span class="tag hon" title="Named in the order of the day: the highest honour a formation can earn.">${esc(designation(c.nat))}</span>`);
  if (c.leave !== undefined) out.push(`<span class="tag" title="At home with its fame. It cannot be given orders until it is back.">At home until ${date(c.leave)}</span>`);
  if (c.lean && mine) out.push('<span class="tag" title="This corps takes no drafts. It stays as small as its losses leave it, and keeps what it knows.">Veterans only</span>');
  const r = rivalryOf(g, c.rival);
  if (r && !r.over && mine) out.push(`<span class="tag hon" title="The two have met ${r.met} times. This corps fights ${Math.round(STORIES.rivals.edge * 100)}% harder in any battle its old enemy is in.">Old enemy: ${esc(r.theirsName)}</span>`);
  if (mine && c.rec?.exile && c.home !== undefined) out.push(`<span class="tag bad" title="The place this corps was raised in is in enemy hands.">${esc(g.regions[c.home].name)} lost</span>`);
  return out;
}

/** A unit's record in one line. */
function statLine(kind: UnitKind, u: AnyUnit): string {
  const v = (k: string): number => u.rec?.[k] ?? 0;
  if (kind === 'corps') {
    const c = u as Corps;
    if (c.kind === 'fort') return v('battles') ? `${v('battles')} ${v('battles') === 1 ? 'battle' : 'battles'}, ${v('won')} won` : 'No battles yet';
    return `${grade(c.xp)} · ${v('battles') ? `${v('battles')} ${v('battles') === 1 ? 'battle' : 'battles'}, ${v('won')} won` : 'no battles yet'}`;
  }
  if (kind === 'air') {
    const ops = v('flown') + v('defended');
    return ops ? `${ops} ${ops === 1 ? 'operation' : 'operations'}${v('missions') ? ` · ${v('missions')} bombing missions` : ''}` : 'No operations yet';
  }
  const parts: string[] = [];
  if (v('actions')) parts.push(`${v('actions')} ${v('actions') === 1 ? 'action' : 'actions'} won`);
  if (v('tons')) parts.push(`${num(v('tons'))} tons sunk`);
  if (v('escort')) parts.push(`${v('escort')} month${v('escort') === 1 ? '' : 's'} on escort`);
  return parts.length ? parts.join(' · ') : 'No actions yet';
}

/** The strip under a unit's row: its ribbons, its record in a line, and the way in to the whole of it. */
function recStrip(kind: UnitKind, u: AnyUnit): string {
  return `<button class="recstrip" data-act="record" data-type="${kind}" data-id="${u.id}" title="Open the service record of ${esc(u.name)}">${ribbons(u, 6)}<span class="grow">${esc(statLine(kind, u))}</span><b>Record ›</b></button>`;
}

/** How and where a formation on the memorial wall was lost. */
function fallenLine(f: Fallen): string {
  const how = f.how === 'destroyed' ? 'Destroyed' : f.how === 'surrendered' ? 'Laid down its arms' : 'Starved out';
  return `${how} at ${esc(g.regions[f.reg]?.name ?? 'the front')}, ${date(f.turn)}`;
}

/** The ranks of a general's army, with the one he holds picked out. */
function rankLadder(x: General): string {
  const rows = COMMAND.ranks[x.nat] ?? COMMAND.ranks._default;
  return `<h3>Rank</h3><div class="ladder">${rows.map(([from, title], i) => {
    const next = rows[i + 1]?.[0] ?? Infinity;
    return `<span class="${x.level >= from ? 'got' : ''}${x.level >= from && x.level < next ? ' here' : ''}"><b>${esc(title)}</b><small>${x.level >= from ? 'Held' : `At level ${from}`}</small></span>`;
  }).join('')}</div>`;
}

/** One honour set out in full on a record: ribbon, name, when and where, and what became of it. */
function honourBlock(a: Award, open: boolean): string {
  const d = honourDef(a.hon);
  const el = open ? 'button' : 'div';
  const todo = open && a.nat === g.human && !a.cash && !a.old && !d?.last;
  const edge = !a.cash && !a.old ? edgeText(a) : '';
  return `<${el} class="hon" ${open ? `data-act="award" data-id="${a.id}"` : ''}>${ribbonOf(a, true)}<span class="grow"><b>${esc(awardTitle(g, a))}</b><small>${esc(d?.name ?? '')} · ${date(a.turn)}${a.first ? ` · first of the ${esc(g.nations[a.nat].adj)} ${serviceOf(a.kind)}` : ''}</small><small>${capital(esc(awardState(a)))}${edge ? `: ${edge}` : ''}</small></span>${todo ? '<span class="go">Decide ›</span>' : open ? '<span class="go">Citation ›</span>' : ''}</${el}>`;
}

const STAT_LABELS: Record<UnitKind, [string, string][]> = {
  corps: [['battles', 'Battles'], ['won', 'Won'], ['taken', 'Regions taken'], ['stormed', 'Breakthroughs'], ['stood', 'Stood against odds'], ['pris', 'Prisoners taken'], ['lost', 'Men lost'], ['caps', 'Capitals entered'], ['landings', 'Landings'], ['drops', 'Airborne drops'], ['cut', 'Months cut off'], ['winter', 'Winter battles'], ['desert', 'Desert battles'], ['scores', 'Old scores settled']],
  air: [['flown', 'Operations flown'], ['missions', 'Bombing missions'], ['nights', 'Night raids'], ['skies', 'Skies won'], ['kills', 'Enemy aircraft downed'], ['sunk', 'Ships sunk'], ['defended', 'Raids met'], ['saved', 'Raiders shot down']],
  fleet: [['actions', 'Actions won'], ['capital', 'Capital ships sunk'], ['tons', 'Tons of shipping sunk'], ['escort', 'Months on escort']],
};

function statGrid(kind: UnitKind, u: Storied, always: number): string {
  const cells = STAT_LABELS[kind].filter(([k], i) => i < always || (u.rec?.[k] ?? 0) > 0).map(([k, label]) => `<div class="fact"><span>${label}</span><b>${num(u.rec?.[k] ?? 0)}</b></div>`);
  return cells.length ? `<div class="facts">${cells.join('')}</div>` : '';
}

function diaryBlock(u: { log?: DiaryLine[] }, shape: { kind?: unknown; ships?: unknown; ftr?: unknown }): string {
  const log = u.log ?? [];
  if (!log.length) return '<div class="empty">Nothing written yet.</div>';
  let out = '<div class="reel diary">', t = -1;
  for (let i = log.length - 1; i >= 0; i--) {
    const line = log[i];
    if (line[0] !== t) { t = line[0]; out += `<div class="mo">${date(t)}</div>`; }
    out += `<p class="${line[1] === 'honour' ? 'honour' : ''}">${esc(diaryText(g, shape, line))}</p>`;
  }
  return out + '</div>';
}

/** The letters written about a unit. Air fleets and fleets are filed under their own numbers. */
function lettersOf(kind: UnitKind, id: number): Game['headlines'] {
  const key = kind === 'corps' ? id : kind === 'air' ? -id : -1000 - id;
  return g.headlines.filter((h) => h.kind === 'letter' && h.unit === key);
}

/** A unit's whole story: what it is, what it has earned, what it has done, and what the player can decide about it. */
function unitRecordCard(kind: UnitKind, u: AnyUnit): string {
  const n = g.nations[u.nat];
  const mine = u.nat === g.human;
  const up = mine && humanUp(g);
  const hs = honoursOf(g, u);
  const v = (k: string): number => u.rec?.[k] ?? 0;
  let badge: string, who: string, extra = '', where = -1;
  if (kind === 'corps') {
    const c = u as Corps;
    where = c.reg;
    const gen = generalOf(g, c);
    const guards = isGuards(g, c);
    badge = `<div class="lvbig"><b>${Math.round(c.xp * 100)}</b><span>${esc(grade(c.xp).toUpperCase())}</span></div>`;
    who = `<b>${n.adj} ${KIND[c.kind].label.toLowerCase()}${guards ? ` · ${esc(designation(c.nat))}` : ''} · ${c.divs} division${c.divs > 1 ? 's' : ''}</b>
      <small>${c.leave !== undefined ? `At home in ${esc(g.regions[c.reg].name)} until ${date(c.leave)}` : `At ${esc(g.regions[c.reg].name)}`}${gen ? ` · ${esc(rankOf(gen))} ${esc(gen.name)}` : ''}</small>
      <small>Raised ${c.heir !== undefined ? 'again ' : ''}${date(c.born ?? 1)}${c.home !== undefined ? ` at ${esc(g.regions[c.home].name)}` : ''} · ${stat(g, c, 'service')} months in service</small>`;
    // Home ground.
    if (c.home !== undefined && mine && c.kind !== 'fort') {
      const home = g.regions[c.home];
      extra += `<div class="note"><b>Home: ${esc(home.name)}.</b> ${c.rec?.exile ? `It is in enemy hands. The corps attacks ${Math.round(STORIES.home.attack * 100)}% stronger to win it back, and will take heart the day it is freed.` : `The corps defends it ${Math.round(STORIES.home.defence * 100)}% stronger than any other ground. If it falls to the enemy the corps loses heart.`}</div>`;
    }
    // An old enemy.
    const all = g.stories.rivals.filter((r) => r.mine === c.id);
    for (const r of all.slice(-2)) {
      const adj = g.nations[r.theirsNat]?.adj ?? '';
      extra += r.over
        ? `<div class="note"><b>Old enemy: ${esc(adj)} ${esc(r.theirsName)}.</b> ${r.over === 'won' ? `Destroyed by this corps in ${date(r.ended ?? r.since)}, after ${r.met} meetings.` : r.over === 'lost' ? `It had the better of this corps in the end, ${date(r.ended ?? r.since)}.` : `Lost elsewhere in ${date(r.ended ?? r.since)}. The score was never settled.`}</div>`
        : `<div class="note"><b>Old enemy: ${esc(adj)} ${esc(r.theirsName)}.</b> They first met at ${esc(g.regions[r.first]?.name ?? 'the front')} and have met ${r.met} times${r.won + r.lost ? `; since the men began to keep count, ${r.won} won and ${r.lost} lost` : ''}. This corps fights ${Math.round(STORIES.rivals.edge * 100)}% harder in any battle the other is in. Destroy it in battle and the corps gains cohesion, experience, an honour, and ${STORIES.rivals.pc} political capital for you.</div>`;
    }
    // Replacements.
    if (mine && c.kind !== 'fort') {
      const t = Math.round(training(g, c.nat) * 100);
      extra += `<h3>Replacements</h3><p class="sub" style="margin:0 0 7px">Drafts bring a corps back to strength, but men who have never been under fire thin out what it knows. Drafts reach the front at experience ${t} (${esc(grade(t / 100).toLowerCase())}); this corps stands at ${Math.round(c.xp * 100)}. A corps kept to veterans only stays as small as its losses leave it and keeps its edge.</p>
        <div class="seg"><button class="btn small${c.lean ? '' : ' on'}" data-act="drafts" data-id="${c.id}" data-v="take" ${up ? '' : 'disabled'}>Take drafts</button><button class="btn small${c.lean ? ' on' : ''}" data-act="drafts" data-id="${c.id}" data-v="lean" ${up ? '' : 'disabled'}>Veterans only</button></div>`;
    }
  } else if (kind === 'air') {
    const f = u as AirFleet;
    where = f.base;
    const ops = v('flown') + v('defended');
    badge = `<div class="lvbig"><b>${ops}</b><span>${ops === 1 ? 'OPERATION' : 'OPERATIONS'}</span></div>`;
    const need = honourDef('tour')?.needs[1] ?? 25;
    who = `<b>${n.adj} air fleet · ${esc(craft(f.ftr, f.bmr))}</b><small>Based at ${esc(g.regions[f.base].name)}${f.leader ? ` · ${esc(f.leader.name)}` : ''}</small>
      ${f.bmr > 0 && !hs.some((a) => a.hon === 'tour') ? `<small>Tour of duty: ${Math.min(need, v('missions'))} of ${need} bombing missions flown</small><div class="xp"><i style="width:${Math.min(100, Math.round((v('missions') / need) * 100))}%"></i></div>` : ''}`;
  } else {
    const f = u as Fleet;
    badge = `<div class="lvbig"><b>${v('actions')}</b><span>${v('actions') === 1 ? 'ACTION WON' : 'ACTIONS WON'}</span></div>`;
    who = `<b>${n.adj} fleet · ${esc(shipList(f.ships))}</b><small>${whereFleet(f)}${f.admiral ? ` · ${esc(f.admiral.name)}` : ''}</small>`;
  }
  const letters = lettersOf(kind, u.id);
  const waiting = hs.filter((a) => mine && !a.cash && !a.old && !honourDef(a.hon)?.last).length;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${n.adj} ${serviceOf(kind)} · Service record</div><h2>${esc(u.name)}</h2>${u.nick ? `<div class="aka">${esc(capital(u.nick))}</div>` : ''}</header>
    <div class="body">
      <div class="record">${badge}<div class="who">${who}</div></div>
      <h3>Honours</h3>${hs.length ? `${hs.map((a) => honourBlock(a, true)).join('')}${waiting ? `<p class="sub">An honour carried at the front makes the unit fight harder. Open one to send its heroes home instead: on a bond tour, or to the schools as instructors.</p>` : ''}` : `<div class="empty">None yet. Honours are earned from the record: ${kind === 'corps' ? 'breakthroughs, stands against the odds, capitals entered, sieges outlasted, long service' : kind === 'air' ? 'a full tour of twenty-five bombing missions, a sky won, raids broken up, ships sunk' : 'a great ship sunk in a fleet action, a year on convoy escort, tonnage sunk on the trade routes'}.</div>`}
      <h3>Record</h3>${statGrid(kind, u, kind === 'corps' ? 3 : 1)}
      ${extra}
      <h3>Diary</h3>${diaryBlock(u, u)}
      ${letters.length ? `<h3>Letters</h3>${letters.slice(-4).reverse().map(letterBlock).join('')}` : ''}
    </div><footer>${where >= 0 ? `<button class="btn" data-act="showmap" data-id="${where}">Show on map</button>` : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

/** A formation on the memorial wall. */
function fallenCard(f: Fallen): string {
  const n = g.nations[f.nat];
  const mine = f.nat === g.human;
  const hs = honoursOf(g, f);
  const by = g.nations[f.by];
  const heir = f.heir !== undefined ? g.corps.find((c) => c.id === f.heir) : undefined;
  const queued = n.queue.some((b) => b.revive === f.id);
  const site = mine ? raiseSite(g, f.nat, f.kind, ui.sel) : -1;
  const at = site >= 0 ? g.regions[site] : null;
  const b = specOf(g, f.kind) ?? PRODUCTION.builds.inf;
  const have = Math.floor(n.pp);
  let again = '';
  if (heir) again = `<div class="note"><b>Raised again.</b> A new ${esc(f.name)} carries the old colours. <button class="link" data-act="record" data-type="corps" data-id="${heir.id}">Its record ›</button></div>`;
  else if (f.heir !== undefined) again = '<div class="note"><b>Raised again, and lost again.</b> The name has been carried twice.</div>';
  else if (queued) again = '<div class="note"><b>Being raised again.</b> A new corps is forming under the old name.</div>';
  else if (mine && b && at) again = `<h3>Raise it again</h3><button class="buy" data-act="revive" data-id="${f.id}" ${humanUp(g) && have >= b.cost ? '' : 'disabled'}>${icon({ nat: f.nat, kind: f.kind } as Corps)}<span class="grow"><b>Raise ${esc(f.name)} again</b><small>${b.divs} divisions · ready in ${b.months} months · forms up at ${esc(at.name)}</small><small>The new corps carries the old colours${f.nick ? ', the name the army gave it' : ''} and ${Math.round(STORIES.legacy.xp * 100)}% of what the old corps knew above a green one.</small></span><span class="cost${have >= b.cost ? '' : ' short'}">${money(b.cost)}</span></button>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Memorial wall · ${n.adj} army</div><h2>${esc(f.name)}</h2>${f.nick ? `<div class="aka">${esc(capital(f.nick))}</div>` : ''}</header>
    <div class="body">
      <p class="lead"><b>${fallenLine(f)}</b>${by && by.id !== f.nat ? `, ${f.how === 'destroyed' ? 'by' : f.how === 'surrendered' ? 'to' : 'cut off by'} the ${esc(by.adj)} army` : ''}. ${f.divs} ${KIND[f.kind].label.toLowerCase()} division${f.divs > 1 ? 's' : ''}, raised ${date(f.born)}${f.home >= 0 ? ` at ${esc(g.regions[f.home].name)}` : ''}; ${esc(grade(f.xp).toLowerCase())} troops at the end.${f.general ? ` ${esc(f.general)} was with it.` : ''}</p>
      ${hs.length ? `<h3>Honours</h3>${hs.map((a) => honourBlock(a, true)).join('')}` : ''}
      <h3>Record</h3>${statGrid('corps', f, 3)}
      ${again}
      ${f.log?.length ? `<h3>Diary</h3>${diaryBlock(f, { kind: f.kind })}` : ''}
    </div><footer><button class="btn" data-act="showmap" data-id="${f.reg}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

/** The citation for an honour, and the choice that comes with it: keep the fame at the front, or spend it at home. */
function awardCard(a: Award): string {
  const d = honourDef(a.hon);
  const n = g.nations[a.nat];
  const mine = a.nat === g.human;
  const u = unitOf(g, a.kind, a.unit);
  const up = humanUp(g);
  const C = STORIES.cash, fame = fameOf(a);
  const edge = edgeText(a);
  const where = a.kind === 'fleet' ? -1 : a.at >= 0 ? a.at : a.kind === 'corps' && u ? (u as Corps).reg : a.kind === 'air' && u ? (u as AirFleet).base : -1;
  const away = a.kind === 'corps'
    ? `The corps goes home for ${C.months} months and cannot be given orders.`
    : `Its best crews go home, and the ${a.kind === 'air' ? 'air fleet' : 'fleet'} loses ${Math.round(C.ready * 100)} points of readiness.`;
  let choice = '';
  if (d?.last) choice = '<div class="note"><b>A last honour.</b> It is carried on the memorial wall, and by any corps raised again in the name.</div>';
  else if (a.old) choice = '<div class="note"><b>Carried from the old corps.</b> The formation that bore this name before earned it. The new one wears the ribbon, and has its own name to make.</div>';
  else if (!mine) choice = '';
  else if (!u) choice = '<div class="note"><b>The unit that earned this honour is gone.</b> It is written on the memorial wall.</div>';
  else if (a.cash) choice = `<div class="note"><b>${a.cash === 'bonds' ? 'Sent home on a bond tour' : 'Its veterans went to the schools'}, ${date(a.cashed ?? a.turn)}.</b> The ribbon stays on the colours. The edge it gave at the front has been spent at home.</div>`;
  else {
    const opt = (how: Cash, head: string, blurb: string): string => {
      const why = whyNotCash(g, a, how) || (up ? '' : 'Wait for your turn.');
      return `<button class="buy choice" data-act="cash" data-id="${a.id}" data-how="${how}" ${why ? `disabled title="${esc(why)}"` : ''}><span class="grow"><b>${head}</b><small class="gain">${esc(capital(cashWorth(g, a, how)))}</small><small>${blurb}</small>${why && up ? `<small class="why">${esc(why)}</small>` : ''}</span></button>`;
    };
    const lose = a.kind === 'corps' ? ` It also gives up ${Math.round(C.schoolXp * fame * 100)} points of its own experience with the men who leave.` : '';
    choice = `<h3>What will you do with the fame?</h3>
      <div class="buy choice pick"><span class="grow"><b>Keep it at the front</b><small class="gain">${edge ? capital(`${esc(u.name)} ${edge}`) : 'The honour stays on the colours'}</small><small>This is how things stand now, and you can change your mind in any later month. Honours add up, to at most ${Math.round(STORIES.edgeCap * 100)}% for one unit.</small></span></div>
      ${opt('bonds', 'Send it home on a bond tour', `The heroes are paraded through the cities and the country digs into its pockets. ${away} The honour stops giving its edge.${n.pc >= DIPLOMACY.pcCap ? ' Your political capital is already as high as it can go, so only the production points would count.' : ''}`)}
      ${opt('school', 'Send its veterans to the schools', `The men who did it teach the next drafts how. ${away}${lose} The honour stops giving its edge.`)}`;
  }
  const more = unseen(g).filter((x) => x.id !== a.id).length;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${mine ? 'Honour' : `${n.adj} honour`} · ${date(a.turn)}${a.first ? ` · first of the ${n.adj} ${serviceOf(a.kind)}` : ''}</div><h2>${esc(awardTitle(g, a))}</h2></header>
    <div class="body">
      <div class="cite">${ribbonOf(a, true)}<div><b>${esc(a.name)}</b>${u?.nick ? ` <i class="nick">${esc(u.nick)}</i>` : ''}<small>${esc(d?.name ?? '')}${placeOf(a.kind, a.at) ? ` · ${esc(placeOf(a.kind, a.at))}` : ''}</small></div></div>
      <p class="lead citation">${esc(citation(g, a))}</p>
      ${d?.real ? `<div class="note real"><b>In the real war.</b> ${esc(d.real)}</div>` : ''}
      ${choice}
    </div><footer>${u ? `<button class="btn" data-act="record" data-type="${a.kind}" data-id="${a.unit}">Service record</button>` : ''}${where >= 0 ? `<button class="btn" data-act="showmap" data-id="${where}">Show on map</button>` : ''}<button class="btn go" data-act="nextaward">${more ? `Next honour (${more})` : 'Close'}</button></footer>`;
}

/** The Honours tab: the player's honoured units, their old enemies, the memorial wall, and the roll of honour of the war. */
function honoursPane(): string {
  const me = g.human;
  const S = g.stories;
  const unitRow = (kind: UnitKind, u: AnyUnit, whose = false): string => {
    const at = kind === 'corps' ? esc(g.regions[(u as Corps).reg].name) : kind === 'air' ? esc(g.regions[(u as AirFleet).base].name) : esc(g.seas[(u as Fleet).zone]?.name ?? (u as Fleet).port);
    return `<button class="gen" data-act="record" data-type="${kind}" data-id="${u.id}"><span class="grow"><b>${esc(u.name)}${u.nick ? ` <i class="nick">${esc(u.nick)}</i>` : ''}</b><small>${whose ? `${g.nations[u.nat].adj} · ` : ''}${esc(statLine(kind, u))} · ${at}</small></span>${ribbons(u, 6)}</button>`;
  };
  const wallRow = (f: Fallen, whose = false): string => `<button class="gen gone" data-act="fallen" data-id="${f.id}"><span class="grow"><b>${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}</b><small>${whose ? `${g.nations[f.nat].adj} · ` : ''}${fallenLine(f)}${f.heir !== undefined ? ' · raised again' : ''}</small></span>${ribbons(f, 6)}</button>`;
  const rollRow = (a: Award): string => `<button class="gen" data-act="award" data-id="${a.id}">${ribbonOf(a, true)}<span class="grow"><b>${esc(a.name)}</b><small>${g.nations[a.nat].adj} · ${esc(awardTitle(g, a))} · ${date(a.turn)}</small></span></button>`;
  const units = (nat: string | null): [UnitKind, AnyUnit][] => [
    ...g.corps.filter((c) => !c.dead && (!nat || c.nat === nat) && c.hon?.length).map((c): [UnitKind, AnyUnit] => ['corps', c]),
    ...g.airFleets.filter((f) => (!nat || f.nat === nat) && f.hon?.length).map((f): [UnitKind, AnyUnit] => ['air', f]),
    ...g.fleets.filter((f) => (!nat || f.nat === nat) && f.hon?.length).map((f): [UnitKind, AnyUnit] => ['fleet', f]),
  ].sort((x, y) => (y[1].hon?.length ?? 0) - (x[1].hon?.length ?? 0));

  if (!me) {
    const roll = S.awards.filter((a) => !a.old).slice(-40).reverse();
    const famous = units(null).slice(0, 12);
    const wall = S.fallen.slice(-25).reverse();
    return `<h2>Honours of the war</h2><p class="sub">Every unit keeps a record of what it has done, and when it has done enough it is honoured: a breakthrough, a stand against the odds, a capital entered, a full tour of missions. Open any of them to read its story.</p>
      <div class="facts"><div class="fact"><span>Honours given</span><b>${S.awards.filter((a) => !a.old).length}</b></div><div class="fact"><span>Units honoured</span><b>${units(null).length}</b></div><div class="fact"><span>On the wall</span><b>${S.fallen.length}</b></div></div>
      <h3>The most honoured units</h3>${famous.length ? famous.map(([k, u]) => unitRow(k, u, true)).join('') : '<div class="empty">No unit has made a name yet.</div>'}
      <h3>Roll of honour</h3>${roll.length ? roll.map(rollRow).join('') : '<div class="empty">No honours yet.</div>'}
      <h3>Memorial wall</h3>${wall.length ? wall.map((f) => wallRow(f, true)).join('') : '<div class="empty">No formation has been destroyed yet.</div>'}`;
  }

  const mine = units(me);
  const held = S.awards.filter((a) => a.nat === me && !a.old);
  const waiting = unseen(g);
  const wall = S.fallen.filter((f) => f.nat === me).reverse();
  const feuds = S.rivals.filter((r) => !r.over);
  const settled = S.rivals.filter((r) => r.over === 'won');
  const schools = S.schools[me] ?? 0;
  const t = Math.round(training(g, me) * 100);
  const others = S.awards.filter((a) => a.nat !== me && !a.old && (a.first || fameOf(a) >= 3)).slice(-12).reverse();
  let out = `<h2>Honours</h2><p class="sub">Every unit keeps a record of what it has done. When it has done enough it is honoured, and what you do with the fame is yours to decide: keep it at the front, where the honour makes the unit fight harder, or send its heroes home to sell war bonds or train the next drafts.</p>
    <div class="facts"><div class="fact"><span>Honours held</span><b>${held.length}</b></div><div class="fact"><span>Cadres at the schools</span><b>${schools} / ${STORIES.veterans.maxSchools}</b></div><div class="fact"><span>On the wall</span><b>${wall.length}</b></div></div>
    <p class="sub">Drafts reach the front at experience ${t} (${esc(grade(t / 100).toLowerCase())}), and a new corps starts at ${Math.round(raisedXp(g, me) * 100)}. Every cadre of veterans sent to the schools raises both.</p>`;
  if (waiting.length) out += `<div class="note"><b>${waiting.length} ${waiting.length === 1 ? 'citation is' : 'citations are'} waiting to be read.</b> <button class="btn small" data-act="nextaward" style="margin-left:6px">Read ${waiting.length === 1 ? 'it' : 'them'}</button></div>`;
  out += `<h3>Your honoured units</h3>${mine.length ? mine.map(([k, u]) => unitRow(k, u)).join('') : '<div class="empty">None yet. Honours are earned from the record: breakthroughs, stands against the odds, capitals entered, sieges outlasted, long service, a full tour of bombing missions. Every unit has a <b>Record</b> strip under its name; open it to see how it stands.</div>'}`;
  if (feuds.length || settled.length) {
    out += '<h3>Old enemies</h3>';
    for (const r of [...feuds, ...settled.slice(-3)]) {
      const c = g.corps.find((x) => x.id === r.mine && !x.dead);
      const adj = g.nations[r.theirsNat]?.adj ?? '';
      const text = r.over ? `Settled ${date(r.ended ?? r.since)} after ${r.met} meetings` : `Met ${r.met} times, first at ${esc(g.regions[r.first]?.name ?? 'the front')}${r.won + r.lost ? ` · ${r.won} won, ${r.lost} lost since` : ''}`;
      out += `<${c ? 'button' : 'div'} class="gen${r.over ? ' gone' : ''}" ${c ? `data-act="record" data-type="corps" data-id="${c.id}"` : ''}><span class="grow"><b>${esc(r.mineName)} <span style="font-weight:400;color:var(--ink-3)">against</span> ${esc(adj)} ${esc(r.theirsName)}</b><small>${text}</small></span></${c ? 'button' : 'div'}>`;
    }
  }
  out += `<h3>Memorial wall</h3>${wall.length ? `${wall.slice(0, 30).map((f) => wallRow(f)).join('')}<p class="sub">A formation on the wall can be raised again under its old name from <b>Build</b>. It carries the old colours and part of what the old corps knew.</p>` : '<div class="empty">No formation of yours has been destroyed.</div>'}`;
  out += `<h3>Honours of the other armies</h3>${others.length ? others.map(rollRow).join('') : '<div class="empty">None of note yet.</div>'}`;
  return out;
}

function reelPane(): string {
  if (!g.headlines.length) return '<div class="empty">No news yet.</div>';
  let out = '<h2>Newsreel</h2><div class="reel">';
  let t = -1;
  for (let i = g.headlines.length - 1; i >= 0; i--) {
    const h = g.headlines[i];
    if (h.turn !== t) { t = h.turn; out += `<div class="mo">${date(t)}</div>`; }
    out += h.kind === 'letter' ? letterBlock(h) : `<p class="${h.kind}"><span class="wk">Week ${h.week}</span>${esc(h.text)}</p>`;
  }
  return out + '</div>';
}

function verdictFor(rep: BattleReport): { text: string; cls: string } {
  const me = g.human;
  const defSide = me !== null && rep.defenders.some((u) => u.nat === me) && !rep.attackers.some((u) => u.nat === me) && rep.attNat !== me;
  // Ground given up under orders is neither a victory nor a defeat for the troops who left it.
  if (rep.yielded) return defSide ? { text: 'Fell back', cls: '' } : { text: 'Taken unopposed', cls: 'win' };
  if (!defSide) return { text: rep.level, cls: rep.taken ? 'win' : 'loss' };
  // Seen from the defender's side of the hill.
  const flip: Record<string, string> = {
    'Heroic victory': 'Crushing defeat', 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Pyrrhic victory': 'Close defeat',
    'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory', 'Crushing defeat': 'Heroic victory',
  };
  return { text: flip[rep.level] ?? rep.level, cls: rep.taken ? 'loss' : 'win' };
}

function seaVerdict(r: SeaReport): { text: string; cls: string } {
  const me = g.human;
  const defSide = me !== null && r.def.some((u) => u.nat === me) && !r.att.some((u) => u.nat === me);
  if (!defSide) return { text: r.level, cls: r.won ? 'win' : 'loss' };
  const flip: Record<string, string> = { 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory' };
  return { text: flip[r.level] ?? r.level, cls: r.won ? 'loss' : 'win' };
}

function seaRow(r: SeaReport): string {
  const v = seaVerdict(r);
  return `<button class="brow" data-act="seareport" data-id="${r.id}"><b>${esc(r.zoneName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)} · fleet action, ${g.nations[r.attNat].adj} against ${g.nations[r.defNat].adj} ships</span></button>`;
}

function airVerdict(r: AirReport): { text: string; cls: string } {
  const me = g.human;
  // A raid is the player's to win when he or an ally flew it, and his to lose when it fell on him or an ally,
  // whether or not a fighter rose to meet it. Anybody else's raid is reported as its attacker saw it.
  const ours = (nat: string): boolean => me !== null && !!nat && (nat === me || allied(g, nat, me));
  const attSide = ours(r.attNat) || r.att.some((u) => u.nat === me);
  const defSide = !attSide && (ours(r.defNat) || r.def.some((u) => ours(u.nat)));
  // A raid that no fighter rose to meet was no battle: it got through, and is called that, not a victory or a defeat.
  // A raid is judged by what it did: it got through or it was beaten off. Only a fight for the sky is won or lost outright.
  if (airWar(g) && r.mission !== 'superiority') {
    const cls = me !== null && !attSide && !defSide ? '' : r.won === defSide ? 'loss' : 'win';
    if (!r.def.length) return { text: defSide ? 'Not intercepted' : 'Unopposed', cls };
    return { text: r.won ? (defSide ? 'Raid got through' : 'Raid succeeded') : 'Raid beaten off', cls };
  }
  if (!defSide) return { text: r.level, cls: me !== null && !attSide ? '' : r.won ? 'win' : 'loss' };
  const flip: Record<string, string> = { 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory' };
  return { text: flip[r.level] ?? r.level, cls: r.won ? 'loss' : 'win' };
}

function airTitle(r: AirReport): string {
  if (r.mission === 'fleet') return `Air strike on the ${r.ships?.[0]?.fleet ?? 'fleet'}`;
  if (r.mission === 'harbour') return `Raid on the harbour of ${r.place}`;
  if (r.mission === 'town') return `Bombing of the defences of ${r.region >= 0 ? townName(g.regions[r.region]) : r.place}`;
  return r.mission === 'superiority' ? `The sky over ${r.place}` : r.mission === 'bomb' ? `Bombing of ${r.place}` : r.mission === 'night' ? `Night raids on ${r.place}` : r.mission === 'ground' ? `Raid on the troops at ${r.place}` : r.mission === 'line' ? `Bombing of the line at ${r.place}` : `Air strikes, ${r.place.replace(/^the /, '')}`;
}

function airBrow(r: AirReport): string {
  const v = airVerdict(r);
  return `<button class="brow" data-act="airreport" data-id="${r.id}"><b>${esc(airTitle(r))}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)} · ${airWar(g) ? 'air raid' : 'air campaign'}, ${g.nations[r.attNat].adj} against ${r.defNat ? g.nations[r.defNat].adj : 'enemy'} ${r.mission === 'strike' ? 'shipping' : r.mission === 'fleet' ? `ships in ${esc(r.place)}` : r.mission === 'harbour' ? 'ships in harbour' : r.mission === 'town' ? 'a town' : r.mission === 'superiority' ? 'fighters' : r.mission === 'ground' ? 'troops' : 'industry'}</span></button>`;
}

function landRow(r: BattleReport): string {
  const v = verdictFor(r);
  return `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week} · ${g.nations[r.attNat].adj} attack on ${g.nations[r.defNat].adj} forces</span></button>`;
}

function battlesPane(): string {
  if (!g.reports.length && !g.seaReports.length && !g.airReports.length && !g.shoreReports.length) return '<h2>Battles</h2><div class="empty">No battles yet. Every battle is settled from its Battle Ticket and filed here.</div>';
  // Newest first. Within a month the land battles are fought first, then the air campaigns, then the fleet actions.
  const rows: { turn: number; rank: number; i: number; html: () => string }[] = [];
  g.reports.forEach((r, i) => rows.push({ turn: r.turn, rank: 0, i, html: () => landRow(r) }));
  g.airReports.forEach((r, i) => rows.push({ turn: r.turn, rank: 1, i, html: () => airBrow(r) }));
  g.seaReports.forEach((r, i) => rows.push({ turn: r.turn, rank: 2, i, html: () => seaRow(r) }));
  g.shoreReports.forEach((r, i) => rows.push({ turn: r.turn, rank: 3, i, html: () => shoreRow(r) }));
  rows.sort((a, b) => b.turn - a.turn || b.rank - a.rank || b.i - a.i);
  return '<h2>Battles</h2><p class="sub">Open any battle, on land, at sea or in the air, to see its Battle Ticket: what the campaign map fed into the fight.</p><div style="height:8px"></div>'
    + rows.slice(0, 90).map((x) => x.html()).join('');
}

// ---- Spies and the fog of war (the grand campaign) -----------------------------------------------

const EYE = 'M-3.4 0Q0 -3.1 3.4 0Q0 3.1 -3.4 0Z';
const chance = (x: number): string => (x > 0 && x < 0.005 ? 'under 1%' : `${Math.round(x * 100)}%`);

/** This month's secret reports that the player has not yet opened the Spies tab on. */
function spyNews(): number {
  if (!g.human || ui.spySeen === g.turn || ui.tab === 'spies') return 0;
  return g.intel.notes.filter((n) => n.turn === g.turn && n.nat === g.human && n.what !== 'recruit' && n.what !== 'retire').length;
}

/** The player's agents on the map: an eye in a dark seal beside the province's armies. A click opens the agent's orders. */
function agentMarks(): string {
  const room = g.human ? secretService(g) : null;
  if (!room) return '';
  const at = new Map<number, number>();
  return room.agents.map((a) => {
    const r = g.regions[a.reg];
    const k = at.get(a.reg) ?? 0;
    at.set(a.reg, k + 1);
    return `<g class="spy${a.abroad ? ' away' : ''}${a.atWork ? '' : ' road'}${ui.tab === 'spies' && ui.spy === a.id ? ' sel' : ''}" data-act="spypick" data-agent="${a.id}" transform="translate(${r.x + 15 + k * 10} ${r.y + 13})"><title>Agent ${esc(a.name)}, ${esc(a.rank.toLowerCase())}. ${esc(a.doing)}.</title><circle r="5.8"/><path d="${EYE}"/><circle class="pupil" r="1.2"/></g>`;
  }).join('');
}

/** What the fog of war hides in a province, said in the province panel. */
function fogNote(r: Region): string {
  if (!fogOn(g) || seen[r.id] >= 2) return '';
  if (seen[r.id] === 0) return `<div class="note fogged"><b>Unseen ground.</b> ${esc(r.name)} is too far from your lines for anything to be known of what stands here. An agent sent to the province would report every army in it and around it.</div>`;
  const by = new Map<string, number>();
  for (const c of r.units) if (!inFull(r, c.nat)) by.set(c.nat, (by.get(c.nat) ?? 0) + 1);
  if (!by.size) return `<div class="note fogged"><b>Glimpsed from a distance.</b> Your patrols see no army at ${esc(r.name)}.</div>`;
  const rows = [...by].map(([nat, n]) => `${n} ${esc(g.nations[nat].adj)} corps`);
  return `<div class="note fogged"><b>Glimpsed from a distance.</b> Your patrols count ${rows.join(' and ')} at ${esc(r.name)}. Their kinds and their strength are not known: your own troops beside the province would count them better, and a battle, an agent on watch or a reconnaissance flight would identify them.</div>`;
}

/** The secret service in a province: the player's agent there and what it reports, and the agents that could be sent. */
function spyBlock(r: Region): string {
  const room = g.human && g.nations[g.human].alive ? secretService(g) : null;
  if (!room) return '';
  const here = room.agents.filter((a) => a.reg === r.id);
  let out = '';
  for (const a of here) out += `<div class="note spyhere"><b>Agent ${esc(a.name)}.</b> ${esc(a.doing)}.${a.atWork && a.abroad ? ` Risk of being caught this month: ${chance(a.caught)}.` : ''} <button class="btn small" data-act="spypick" data-id="${a.id}">Orders</button></div>`;
  const rep = agentReport(g, r.id);
  if (rep) {
    const air = rep.air.map((f) => `${esc(f.name)} (${f.ftr} fighters, ${f.bmr} bombers)`);
    out += `<div class="note spyrep"><b>${esc(rep.agent)} reports from ${esc(r.name)}.</b><ul><li>${rep.orders.length ? `On order here: ${rep.orders.map((o) => `${esc(o.name)}, ready ${date(o.eta)}`).join('; ')}.` : 'Nothing is on order here.'}</li><li>${air.length ? `On the airfields: ${air.join('; ')}.` : 'No air fleet is based here.'}</li>${rep.factories ? `<li>${rep.factories - rep.ruined} of ${rep.factories} ${rep.factories === 1 ? 'factory' : 'factories'} at work.</li>` : ''}</ul>Your armies attack ${esc(r.name)} ${Math.round(INTEL.battle.attack * 100)}% harder for knowing its defences.</div>`;
  }
  // Agents that could be sent here, for a province that is not the player's own side's.
  if (humanUp(g) && !onEve(g)) {
    const free = room.agents.filter((a) => a.reg !== r.id && whyNotSendAgent(g, a.id, r.id) === null).slice(0, 4);
    if (free.length && !friendOf(r.ctrl)) out += `<div class="acts spysend">${free.map((a) => `<button class="btn small" data-act="spygo" data-id="${a.id}" data-to="${r.id}" title="A month on the road, then at work here: watching, reading the books or sabotage.">Send agent ${esc(a.name)} here</button>`).join('')}</div>`;
  }
  return out;
}

/** One agent on the Spies tab: who, where, doing what, and under it (when picked) the work it can be given. */
function agentCard(a: AgentView, can: boolean): string {
  const open = ui.spy === a.id;
  const pips = [1, 2, 3].map((k) => `<i class="${k <= a.skill ? 'on' : ''}"></i>`).join('');
  const host = g.regions[a.reg].ctrl;
  let body = '';
  if (open) {
    const sees = a.sees.slice().sort((x, y) => (x.reg === a.reg ? 0 : 1) - (y.reg === a.reg ? 0 : 1) || y.divs - x.divs);
    const sight = a.atWork && a.abroad
      ? `<div class="asees"><b>In sight</b>${sees.length ? `<ul>${sees.slice(0, 6).map((s) => `<li>${chip(s.nat)} ${s.corps} ${esc(g.nations[s.nat].adj)} corps at ${esc(g.regions[s.reg].name)}, ${num(s.divs)} ${Math.round(s.divs) === 1 ? 'division' : 'divisions'}</li>`).join('')}${sees.length > 6 ? `<li>and ${sees.length - 6} more armies</li>` : ''}</ul>` : '<p>No army in the province or around it.</p>'}</div>`
      : '';
    const here = g.regions[a.reg];
    const offers = a.atWork ? a.missions.filter((m) => m.now || !((m.id === 'oil' && !here.oil) || (m.id === 'books' && !here.cap))).map((m) => {
      const odds = m.success !== null ? `Comes off ${chance(m.success)} of the time · caught ${chance(m.caught)}` : a.abroad ? `Caught ${chance(m.caught)} a month` : 'No risk at home';
      return `<button class="buy offer amis${m.now ? ' now' : ''}" data-act="spymission" data-id="${a.id}" data-m="${m.id}" ${m.ok && can && !m.now ? '' : 'disabled'}><span class="grow"><b>${esc(m.label)}${m.now ? ' <span class="tag good">Current work</span>' : ''}</b><small>${esc(m.text)}</small><small class="odds">${odds}</small>${m.ok ? '' : `<small class="whyno">${esc(m.why)}</small>`}</span></button>`;
    }).join('') : `<p class="sub">${esc(a.doing)}. Orders can be given once the agent is at work.</p>`;
    const home = homeBase(g, a.nat);
    const acts = `<div class="acts">${a.atWork ? `<button class="btn small${ui.spySend ? ' on' : ''}" data-act="spysend" data-id="${a.id}" ${can ? '' : 'disabled'} title="Then click any province on the map. The journey takes a month.">${ui.spySend ? 'Click a province on the map…' : 'Send to a province'}</button>` : ''}${a.atWork && a.abroad && home >= 0 ? `<button class="btn small" data-act="spygo" data-id="${a.id}" data-to="${home}" ${can ? '' : 'disabled'} title="Back to ${esc(g.regions[home].name)}, to stand guard against saboteurs.">Recall home</button>` : ''}${ui.spyAsk === a.id ? `<span class="pathask"><b>Retire ${esc(a.name)} for good?</b><button class="btn small warn" data-act="spyretire" data-id="${a.id}">Yes, retire</button><button class="btn small" data-act="spypick" data-id="${a.id}">No</button></span>` : `<button class="btn small" data-act="spyretire" data-id="${a.id}" ${can ? '' : 'disabled'} title="Frees a place in the service for a new recruit.">Retire</button>`}</div>`;
    body = `<div class="abody">${a.rest > 0 ? `<div class="note warn">Lying low for ${a.rest === 1 ? 'another month' : `${a.rest} more months`} after the last attempt: watching goes on, sabotage must wait.</div>` : ''}${sight}${offers}${acts}</div>`;
  }
  return `<div class="agent${open ? ' sel' : ''}${a.abroad ? ' away' : ''}${a.atWork ? '' : ' road'}">
    <button class="ahead" data-act="spypick" data-id="${a.id}"><svg class="aseal" viewBox="-7 -7 14 14" aria-hidden="true"><circle r="5.8"/><path d="${EYE}"/><circle class="pupil" r="1.2"/></svg><span class="grow"><b>${esc(a.name)} <span class="apips" title="${esc(a.rank)}${a.next !== null ? `: ${a.xp} of ${a.next} experience toward the next rank` : ''}">${pips}</span></b><small>${esc(a.rank)} · ${esc(a.doing)}</small></span><span class="awhere">${chip(host)}${a.atWork && a.abroad ? `<small class="arisk${a.caught >= 0.1 ? ' hot' : ''}" title="The chance this month that the agent is caught">${chance(a.caught)} risk</small>` : `<small class="arisk safe">${a.abroad ? 'on the way' : 'at home'}</small>`}</span></button>${body}</div>`;
}

/** The Spies tab: the secret service's room. */
function spiesPane(): string {
  const room = secretService(g);
  if (!room) return '<h2>Secret service</h2><p class="sub">Spies and the fog of war belong to the grand campaign.</p>';
  const can = humanUp(g) && !onEve(g) && !passing;
  if (room.agents.length && !room.agents.some((a) => a.id === ui.spy)) { ui.spy = -1; ui.spySend = false; }
  const why = whyNotRecruitAgent(g);
  const T = room.threat;
  const threat = T.foes.length
    ? `<div class="note ${T.guards ? '' : 'warn'}"><b>Saboteurs at home.</b> ${esc(listOf(T.foes.map(natName)))} ${T.foes.length === 1 ? 'has' : 'have'} agents of ${T.foes.length === 1 ? 'its' : 'their'} own. Each month there is about a ${chance(T.chance)} chance that one of them wrecks a factory or blows a railway on your home ground. ${T.guards ? `You have ${T.guards} ${T.guards === 1 ? 'agent' : 'agents'} on guard: a guard in or beside the province stops a plot ${chance(INTEL.enemy.near)} of the time, and is some help everywhere else.` : 'Nobody is on guard. An agent kept at home stops most plots in its own province and the ones beside it, and is some help everywhere else.'}</div>`
    : '';
  const books = room.books.length ? `<h3>Books you are reading</h3><div class="acts">${room.books.map((id) => `<button class="btn small" data-act="books" data-nat="${id}">${chip(id)} ${esc(natName(id))}</button>`).join('')}</div>` : '';
  const file = room.notes.slice(0, 14).map((n) => `<div class="snote ${n.good ? 'good' : 'bad'}"><small>${date(n.turn)}</small><span>${esc(n.text)}</span></div>`).join('');
  const list = room.agents.length ? room.agents.map((a) => agentCard(a, can)).join('') : '<div class="empty">You have no agents. Recruit one: it trains for a month at home, and then you can send it anywhere on the map.</div>';
  return `<h2>Secret service</h2>
    <p class="sub">${room.fog
    ? 'You see a foreign army whole only on your own side\'s ground, beside your troops, and one province beyond. A province further off it is a flag and a count. Deeper than that the map is dark. Agents are your eyes where your armies cannot reach.'
    : 'The fog of war is lifted: every army on the map is in sight. Agents can still read a government\'s books and wreck its factories, railways and oilfields.'}</p>
    <div class="facts"><div class="fact"><span>Agents</span><b>${room.agents.length} of ${room.most}</b></div><div class="fact"><span>A new agent</span><b>${num(room.cost)}</b></div><div class="fact"><span>Fog of war</span><b>${room.fog ? 'On' : 'Off'}</b></div></div>
    <div class="acts" style="margin-top:8px"><button class="btn" data-act="spyrecruit" ${can && !why ? '' : `disabled title="${esc(can ? why ?? '' : 'Orders are given in your own month.')}"`}>Recruit an agent · ${num(room.cost)}</button><button class="btn small" data-act="fog" title="${room.fog ? 'Shows every army on the map, as before spies were in the game.' : 'Hides the armies your side cannot see.'}">${room.fog ? 'Lift the fog of war' : 'Lay the fog of war again'}</button></div>
    ${can && why && room.agents.length < room.most ? `<p class="sub whyno">${esc(why)}</p>` : ''}
    <h3>Your agents</h3>${list}
    ${books}${threat}
    <details class="spyhow"><summary>How the secret war works</summary><ul>
      <li>An agent sent to a province is on the road for a month. From then on it reports every army there and in the provinces around it, and your armies attack that province ${Math.round(INTEL.battle.attack * 100)}% harder.</li>
      <li>In a capital an agent can read the government's books: its treasury, its true strength, what it is building and researching, and what is in its depots.</li>
      <li>Against a country you are at war with, an agent can wreck a factory, blow a railway or fire an oilfield. The attempt is made as the month turns, and afterwards the agent must lie low for ${INTEL.risk.lieLow} months.</li>
      <li>Every month abroad an agent may be caught, and a caught agent is gone for good. Capitals, countries at war with you and provinces on alert after sabotage are the most dangerous. A neutral that catches your spy thinks the worse of your side.</li>
      <li>Work that comes off earns experience. An ${esc(INTEL.agents.ranks[1].toLowerCase())} and then a ${esc(INTEL.agents.ranks[2].toLowerCase())} are harder to catch and surer in sabotage.</li>
    </ul></details>
    ${file ? `<h3>The service's file</h3><div class="sfile">${file}</div>` : ''}`;
}

/** A foreign government's books, as the player's agent in its capital reads them. */
function booksCard(nat: string): string {
  const b = foreignBooks(g, nat)!;
  const n = g.nations[nat];
  const cap = n.capital >= 0 ? g.regions[n.capital].name : 'its capital';
  const what: Record<string, string> = { corps: 'Army', fort: 'Fortification', air: 'Aircraft', ship: 'Ships', work: 'Works' };
  const queue = b.queue.length ? b.queue.slice(0, 16).map((q) => `<div class="lrow"><span>${esc(q.name)} <small>${what[q.what] ?? ''}${g.regions[q.region] ? ` · ${esc(g.regions[q.region].name)}` : ''}</small></span><b>${date(q.eta)}</b></div>`).join('') + (b.queue.length > 16 ? `<div class="lrow"><span>and ${b.queue.length - 16} more orders</span><b></b></div>` : '') : '<div class="empty">Nothing is on order.</div>';
  const research = b.research.length ? b.research.map((w) => `<div class="lrow"><span>${esc(w.name)}</span><b>${w.left} ${w.left === 1 ? 'month' : 'months'}</b></div>`).join('') : '<div class="empty">No team is at work.</div>';
  const S = b.stores;
  const staff = rivalStaff(g, nat);
  const depot = S ? `<div class="lfacts two"><div class="lfact"><span>Oil in the depots</span><b>${amt(S.oil.stock)} <small>${monthsWordOf(S.oil.months)}</small></b></div><div class="lfact"><span>Supplies in the depots</span><b>${amt(S.sup.stock)} <small>${monthsWordOf(S.sup.months)}</small></b></div></div>` : `<p class="sub">${esc(upFirst(theName(nat)))} keeps no stocks of oil and supplies.</p>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Secret intelligence · ${date(g.turn)}</div><h2>${flagIcon(nat, n.color, 34, 'inh')} The books of ${esc(theName(nat))}</h2></header>
    <div class="body ledger books"><p class="sub">${b.agent ? `Read by agent ${esc(b.agent)} in ${esc(cap)}. For as long as the agent is not caught, these figures are the true ones, month by month.` : `${esc(upFirst(theName(nat)))} is your ally: its books are open to you.`}</p>
    <div class="lfacts"><div class="lfact"><span>Treasury</span><b>${inHand(b.treasury)}</b></div><div class="lfact"><span>Income a month</span><b>${cash(b.gross)}</b></div><div class="lfact"><span>Upkeep a month</span><b>${cash(b.upkeep)}</b></div><div class="lfact"><span>Left over</span><b class="${b.net < 0 ? 'neg' : ''}">${cash(b.net, true)}</b></div></div>
    <div class="lfacts"><div class="lfact"><span>Army</span><b>${num(b.divisions)} <small>divisions</small></b></div><div class="lfact"><span>Formations</span><b>${b.corps} <small>corps</small></b></div><div class="lfact"><span>Technologies</span><b>${b.researched} <small>researched</small></b></div><div class="lfact"><span>On order</span><b>${b.queue.length}</b></div></div>
    ${staff ? `<h3>Its general staff</h3>${staffRow(staff)}` : ''}
    <div class="lcols"><div><h3>On order</h3>${queue}</div><div><h3>In the laboratories</h3>${research}<h3>Depots</h3>${depot}</div></div></div>
    <footer>${S ? `<button class="btn" data-act="stores" data-nat="${nat}">Its depots in full</button>` : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

// ---- World Domination: the last act ------------------------------------------------------------------

/** "The Axis is beaten", "The Allies are beaten", or both. */
function beatenLine(beaten: ('axis' | 'allies')[]): string {
  if (beaten.length > 1) return 'Both alliances are broken';
  return beaten[0] === 'axis' ? 'The Axis is beaten' : 'The Allies are beaten';
}

/** A card that is waiting its turn: the war just decided, or the world just won. Each is shown unasked only once. */
function nextCard(): typeof ui.modal {
  const act = lastAct(g);
  const me = g.human;
  // A nation that has fallen, or one that has come back: each is announced once, in the order it happened.
  if (me && !passing && ui.seenPass < g.passings.length) {
    // A country set free by its conqueror's fall is told on the conqueror's card, unless it is the player's own.
    const ids = g.passings.slice(ui.seenPass).filter((p) => !(p.kind === 'freed' && p.after && !p.player && g.passings.some((q) => q.kind === 'fell' && q.nat === p.after && q.turn === p.turn))).map((p) => p.id);
    ui.seenPass = g.passings.length;
    if (ids.length) return { kind: 'passing', ids };
  }
  if (!act || !me || !g.nations[me].alive || passing) return null;
  if (act.winner === me && !g.flags['dom:hail']) { g.flags['dom:hail'] = g.turn; return { kind: 'worldwon' }; }
  if (act.open && humanUp(g) && !onEve(g) && !g.flags['dom:seen']) { g.flags['dom:seen'] = g.turn; return { kind: 'lastact' }; }
  return null;
}

/** An army's size in words, as well as it is known: "35 divisions", "about 640 divisions", "a handful of divisions". */
const armyWords = (a: { divs: number; exact: boolean; few?: boolean }): string => a.few ? 'a handful of divisions' : `${a.exact ? '' : 'about '}${num(a.divs)} division${a.exact && Math.round(a.divs) === 1 ? '' : 's'}`;

/** One row of the world's standings. */
function boardRow(r: ReturnType<typeof standings>['rows'][number], rank: number, scale: number): string {
  const pct = r.share * 100;
  const share = pct >= 9.5 ? Math.round(pct) : Math.round(pct * 10) / 10;
  const army = armyWords({ divs: r.divisions, exact: r.exact, few: r.few });
  const income = r.income !== null ? ` · ${num(Math.round(r.income * scale))} a month` : '';
  return `<div class="wrow${r.you ? ' you' : ''}"><span class="wrank">${rank}</span>${chip(r.nat)}<span class="grow"><b>${esc(r.name)}</b>${r.you ? '<i class="tag pts">You</i>' : ''}<small${r.exact ? '' : ' title="Your staff\'s estimate through the fog of war."'}>${r.provinces} province${r.provinces === 1 ? '' : 's'} · ${army}${income}</small><div class="meter" title="How much fight it has left"><i class="${r.will < 0.35 ? 'low' : ''}" style="width:${Math.round(r.will * 100)}%"></i></div></span><span class="wshare"><b>${share}%</b><small>of the world</small></span></div>`;
}

/** The standings of the last act, on the Powers screen: who still stands and how much of the world each holds. */
function worldPane(): string {
  const B = standings(g);
  const me = g.human;
  const winner = B.winner ? g.nations[B.winner] : null;
  const head = winner
    ? `<p class="sub"><b>${esc(winner.name)} stands alone.</b> The last war began in ${date(B.since)} with ${B.began} nations and ended in ${date(g.flags['dom:won'] ?? g.turn)}.</p>`
    : `<p class="sub">Every nation for itself since ${date(B.since)}. <b>${B.left} of ${B.began}</b> still stand, every one of them at war with all the rest, and the last one standing takes the world.</p>`;
  const mine = B.rows.find((r) => r.you);
  const facts = mine && !winner ? `<div class="facts"><div class="fact"><span>Still standing</span><b>${B.left}</b></div><div class="fact"><span>Your share</span><b>${Math.round(mine.share * 100)}%</b></div><div class="fact"><span>Your place</span><b>${B.rows.indexOf(mine) + 1}${['st', 'nd', 'rd'][B.rows.indexOf(mine)] ?? 'th'}</b></div></div>` : '';
  const since = B.fallen.filter((r) => r.fell!.turn >= B.since);
  const fell = (r: typeof B.fallen[number]): string => `<div class="wrow gone">${chip(r.nat)}<span class="grow"><b>${esc(r.name)}</b><small>fell in ${date(r.fell!.turn)} to ${esc(theName(r.fell!.to))}${r.fell!.to === me ? ' <i class="tag good">Yours</i>' : ''}</small></span></div>`;
  return `<h2>World Domination</h2>${head}${facts}
    <div class="wboard">${B.rows.map((r, i) => boardRow(r, i + 1, B.scale)).join('')}</div>
    ${since.length ? `<h3>Fallen in the last war</h3><div class="wboard">${since.map(fell).join('')}</div>` : ''}
    <div class="acts" style="margin-top:10px"><button class="btn small" data-act="traits">National abilities: what each country does best</button></div>`;
}

/** The choice at the end of the great war: take the victory, or dissolve every alliance and fight for the world. */
function lastActCard(ask: boolean): string {
  const act = lastAct(g)!;
  const d = act.decided!;
  const me = g.nations[g.human!];
  const mine = bloc(me);
  const B = standings(g);
  const you = B.rows.find((r) => r.you);
  const rival = B.rows.find((r) => !r.you);
  // The great powers of the beaten alliances: one that fell on the winning side is not part of this sentence.
  const lost = d.fallen.filter((id) => { const s = bloc(g.nations[id]); return (s === 'axis' || s === 'allies') && d.beaten.includes(s); });
  const fallen = listOf((lost.length ? lost : d.fallen).map(natName));
  const has = (lost.length ? lost : d.fallen).length === 1 ? 'has' : 'have';
  const side = mine === 'axis' || mine === 'allies' ? mine : null;
  const yours = side && d.victor === side ? ' Your alliance has won the war it went to fight.'
    : side && d.beaten.includes(side) ? ` You fought on the losing side, and ${esc(theName(me.id))} still stands.`
      : ` ${esc(upFirst(theName(me.id)))} has come through it standing.`;
  const can = act.open && humanUp(g);
  const peace = act.peace
    ? `<div class="pathrow now"><div class="pathhead"><b>The victory is yours</b><span class="tag good">Taken in ${date(act.peace)}</span></div><p>The alliances stand and your campaign goes on as it is. World Domination stays open to you for as long as you want to leave it.</p></div>`
    : `<div class="pathrow"><div class="pathhead"><b>Take the victory</b><button class="btn" data-act="keeppeace" ${can ? '' : 'disabled'}>Take the victory</button></div><p>The alliances stand and the peace holds. Your campaign goes on as it is and nothing is lost. World Domination stays open: you will find it under Powers whenever you want it.</p></div>`;
  const btn = ask
    ? `<span class="pathask"><b>Every nation becomes your enemy today. This cannot be undone.</b><button class="btn warn" data-act="domination" ${can ? '' : 'disabled'}>Begin the last war</button><button class="btn" data-act="lastact">Not yet</button></span>`
    : `<button class="btn warn" data-act="lastact" data-ask="1" ${can ? '' : 'disabled'}>World Domination ▸</button>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The grand campaign · ${date(g.turn)}</div><h2>The war is decided</h2></header>
    <div class="body path lastact"><p class="lead"><b>${beatenLine(d.beaten)}.</b> ${esc(upFirst(fallen))} ${has} fallen, and no two great powers are still at war with each other.${yours}</p>
    <p class="lead quiet">The guns fall silent, and the victors look at one another across the table. An alliance made against a common enemy has no enemy left, and every general knows how many divisions his friends still have in the field.</p>
    <div class="facts"><div class="fact"><span>Nations still standing</span><b>${B.left}</b></div><div class="fact"><span>Your share of the world</span><b>${you ? Math.round(you.share * 100) : 0}%</b></div>${rival ? `<div class="fact"><span>The strongest of the others</span><b>${esc(rival.name)} <small>${armyWords({ divs: rival.divisions, exact: rival.exact, few: rival.few })}</small></b></div>` : ''}</div>
    ${peace}
    <div class="pathrow domrow${ask ? ' now' : ''}"><div class="pathhead"><b>World Domination</b>${btn}</div><p>Every alliance is dissolved. All ${B.left} nations still standing, yesterday's allies among them, are at war with one another from today. Troops on a former ally's soil go home under safe conduct. Nobody makes peace again: <b>the last nation standing has the world</b>.</p></div>
    ${can ? '' : `<div class="note">${esc(act.why ?? 'Orders are given in your own month.')}</div>`}</div>
    <footer><button class="btn go" data-act="close">Decide later</button></footer>`;
}

/** The end of the last act, for a player who is the last one standing. */
/** The announcement of a nation that has fallen or been freed: its flag, what happened, and what follows from it. */
function passingCardHtml(a: NonNullable<ReturnType<typeof announcement>>): string {
  const X = g.nations[a.nat], B = g.nations[a.by];
  const freed = a.kind === 'freed';
  const banner = `<div class="passflags ${freed ? 'freed' : 'fell'}"><div class="pflag">${flagIcon(X.id, X.color, 132)}<span>${esc(X.name)}</span></div><div class="parrow">${freed ? 'freed by' : a.kicker.startsWith('Annexed') ? 'annexed by' : 'fell to'}</div><div class="pflag by">${flagIcon(B.id, B.color, 74)}<span>${esc(B.name)}</span></div></div>`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${esc(a.kicker)}</div><h2>${esc(a.title)}</h2></header>
    <div class="body path natcard">${banner}<p class="lead">${esc(a.text)}</p>
    <div class="facts">${a.facts.map((f) => `<div class="fact"><span>${esc(f.label)}</span><b>${esc(f.value)}</b></div>`).join('')}</div>
    ${freedStrip(a)}
    ${a.more ? `<div class="note${freed ? ' good' : ''}" style="margin-top:12px">${esc(a.more)}</div>` : ''}</div>
    <footer>${a.region >= 0 ? `<button class="btn" data-act="showmap" data-id="${a.region}">Show on map</button>` : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

/** The countries a conqueror's fall set free the same day, as a row of flags. */
function freedStrip(a: NonNullable<ReturnType<typeof announcement>>): string {
  if (!a.freed.length) return '';
  return `<div class="pfreed"><span>Free again with ${esc(theName(a.nat))} beaten</span><div>${a.freed.map((id) => `<b>${flagIcon(id, g.nations[id].color, 30)}${esc(natName(id))}</b>`).join('')}</div></div>`;
}

const COUNT_WORDS = ['', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten', 'Eleven', 'Twelve'];

/** Several nations fallen or freed in the same month: one card, a row for each, in the order it happened. */
function passingListHtml(list: NonNullable<ReturnType<typeof announcement>>[]): string {
  const fell = list.filter((a) => a.kind === 'fell').length, free = list.length - fell;
  const n = COUNT_WORDS[list.length] ?? String(list.length);
  const title = !free ? `${n} nations have fallen` : !fell ? `${n} nations are free` : 'The map is redrawn';
  const rows = list.map((a) => {
    const X = g.nations[a.nat];
    return `<div class="prow2 ${a.kind}"${a.region >= 0 ? ` data-act="showmap" data-id="${a.region}" title="Show on the map"` : ''}>${flagIcon(X.id, X.color, 58)}<div><div class="kick">${esc(a.kicker)}</div><h3>${esc(a.title)}</h3><p>${esc(a.text)}</p>${freedStrip(a)}${a.more ? `<small>${esc(a.more)}</small>` : ''}</div></div>`;
  }).join('');
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The roll of nations · ${date(g.turn)}</div><h2>${title}</h2></header>
    <div class="body path natcard natlist">${rows}</div>
    <footer><button class="btn go" data-act="close">Close</button></footer>`;
}

function worldWonCard(): string {
  const B = standings(g);
  const me = g.nations[g.human!];
  const cap = me.capital >= 0 ? g.regions[me.capital].name : me.name;
  const months = Math.max(1, (g.flags['dom:won'] ?? g.turn) - B.since);
  const since = B.fallen.filter((r) => r.fell!.turn >= B.since);
  const yours = since.filter((r) => r.fell!.to === me.id);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">World Domination · ${date(g.flags['dom:won'] ?? g.turn)}</div><h2>${flagIcon(me.id, me.color, 34, 'inh')} ${esc(me.name)} stands alone</h2></header>
    <div class="body path lastact"><p class="lead"><b>There is nobody left to fight.</b> Every capital on the map answers to ${esc(cap)}, ${months} ${months === 1 ? 'month' : 'months'} after the alliances were dissolved. Of the ${B.began} nations that began the last war, one is left, and it is yours.</p>
    <div class="facts"><div class="fact"><span>The last war</span><b>${months} <small>${months === 1 ? 'month' : 'months'}</small></b></div><div class="fact"><span>Nations you brought down</span><b>${yours.length} <small>of ${since.length}</small></b></div><div class="fact"><span>Your army today</span><b>${num(B.rows[0]?.divisions ?? 0)} <small>divisions</small></b></div></div>
    ${since.length ? `<h3>How the world fell</h3><div class="wboard">${since.map((r) => `<div class="wrow gone">${chip(r.nat)}<span class="grow"><b>${esc(r.name)}</b><small>${date(r.fell!.turn)}, to ${esc(theName(r.fell!.to))}</small></span></div>`).join('')}</div>` : ''}</div>
    <footer><button class="btn" data-act="new">New game</button><button class="btn go" data-act="close">Look at your world</button></footer>`;
}

/**
 * A computer staff on the Powers tab and the Books card: its character in a word, and what it is doing where the
 * player may know it (an ally's plans, or an enemy's whose books an agent of his is reading).
 */
function staffRow(v: StaffView): string {
  const aim = v.efforts.find((e) => e.atYou) ?? v.efforts[0];
  const show = aim ? ` <button class="btn tiny" data-act="showmap" data-id="${aim.target}" title="Show ${esc(aim.targetName)} on the map">Show</button>` : '';
  const how = v.source === 'agent' ? ' title="Read from its books by your agent in its capital."' : v.source === 'ally' ? '' : ' title="Its plans are secret."';
  return `<div class="staff${aim?.atYou ? ' atyou' : ''}"><button class="tag chr ${v.character}" data-act="staffs" title="${esc(v.says)}">${esc(v.label)}</button><small${how}>${esc(v.line)}${show}</small></div>`;
}

/** The card behind a staff's character: what the three characters mean, what a main effort is, and every staff at war as the player knows it. */
function staffsCard(): string {
  const all = rivalStaffs(g).filter((v) => g.nations[v.nat].atWar.length);
  const kinds = (['bold', 'cautious', 'opportunist'] as const).map((k) => `<div class="stkind"><i class="tag chr ${k}">${esc(RIVALS.characters[k].label)}</i><span>${esc(RIVALS.characters[k].says)}</span></div>`).join('');
  const rows = all.map((v) => `<div class="strow">${flagIcon(v.nat, g.nations[v.nat].color, 22)}<b>${esc(natName(v.nat))}</b>${staffRow(v)}</div>`).join('');
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The general staffs · ${date(g.turn)}</div><h2>Who you are up against</h2></header>
    <div class="body staffs"><p class="lead">Every country the computer leads has a general staff with a character of its own in each war. The character decides how readily it attacks, and when it names a main effort.</p>
    ${kinds}
    <h3>The main effort</h3><p class="sub">A staff that is not strong enough for a general offensive does not sit still. When a front has gone quiet it picks one stretch of the line, masses its corps opposite it for a month, softens the fieldworks with a probe and its bombers, and attacks there and nowhere else for a season. An ally tells you where its main effort is going. An enemy keeps it secret: an agent reading its books in its capital learns the place and the months, and you are told the moment it is named.</p>
    <p class="sub">A staff that is winning does not let go either. With twice its enemy\'s strength and no ground gained for three months it accepts shorter odds and sends a third wave. A country whose capital has been in enemy hands for more than a year wears out month by month until it asks for terms.</p>
    <h3>The staffs at war</h3>${rows || '<div class="empty">No country the computer leads is at war.</div>'}</div>
    <footer><button class="btn go" data-act="close">Close</button></footer>`;
}

function powersPane(): string {
  const act = lastAct(g);
  if (act?.on) return worldPane();
  const at = Object.values(g.nations).filter((n) => !n.dormant && (n.atWar.length || n.fellOn));
  const neutral = Object.values(g.nations).filter((n) => n.alive && !n.atWar.length && !n.dormant);
  const offer = act?.open && act.decided
    ? `<div class="note lastnote"><b>${beatenLine(act.decided.beaten)}: the war is decided.</b> ${act.peace ? `You took the victory in ${date(act.peace)}. ` : ''}World Domination is open to you: every alliance dissolved, every nation for itself, the last one standing takes the world.<div class="acts" style="margin-top:7px"><button class="btn small warn" data-act="lastact">The last act ▸</button></div></div>`
    : '';
  let out = `${offer}<h2>Powers at war</h2><p class="sub">The bar shows how much fight a nation has left. It drains as the capital, territory and army are lost.${isOpen(g) ? ' The word under a country is the character of its general staff in this war: press it to read what it means.' : ''}</p>${isOpen(g) ? `<div class="acts" style="margin-top:8px"><button class="btn small" data-act="traits">National abilities: what each country does best</button>${g.human && tradeRules(g) ? '<button class="btn small" data-act="trade">Trade agreements</button>' : ''}</div>` : ''}<div style="height:10px"></div>`;
  const fallen = nationsFallen(g);
  for (const n of at.sort((a, b) => (b.alive ? 1 : 0) - (a.alive ? 1 : 0) || b.armyPeak - a.armyPeak)) {
    if (n.alive) {
      const will = Math.max(0, 1 - collapseScore(g, n.id) / n.resilience);
      const army = armyKnown(g, n.id);
      const books = isOpen(g) && g.human && !friendOf(n.id) && knowsBooks(g, n.id) ? ` <button class="btn tiny" data-act="books" data-nat="${n.id}" title="Your agent in its capital is reading its books.">Books</button>` : '';
      // Its form of government, in a word: a click opens the card.
      const gv = isOpen(g) ? governmentOf(g, n.id) : null;
      const form = gv ? ` <button class="btn tiny" data-act="cabinet" data-nat="${n.id}" title="${esc(gv.form.name)}. ${esc(gv.form.blurb)}">${FORM_WORD[gv.form.id]}</button>` : '';
      // Its general staff: the character it has in this war, and its main effort where the player may know it.
      const staff = rivalStaff(g, n.id);
      out += `<div class="nat">${chip(n.id)}<b>${esc(n.name)}${n.freedOn ? ` <i class="tag good" title="Liberated by ${esc(natName(n.freedOn.by))}. It stands for as long as its capital does.">Freed ${date(n.freedOn.turn)}</i>` : ''}</b><small${army.exact ? '' : ' title="Your staff\'s estimate. An agent reading its books in its capital would give the true figure."'}>${armyWords(army)}${books}${form}</small><div class="will"><div class="meter"><i class="${will < 0.35 ? 'low' : ''}" style="width:${Math.round(will * 100)}%"></i></div><small>At war with ${n.atWar.map(natName).join(', ')}</small></div>${staff ? staffRow(staff) : ''}</div>`;
    } else {
      const f = fallen.find((x) => x.nat === n.id);
      const how = f?.fell.how === 'annexed' ? `Annexed by ${theName(n.fellOn!.to)}` : f?.fell.how === 'terms' ? `Left the war by treaty with ${natName(n.fellOn!.to)}` : `Asked ${natName(n.fellOn!.to)} for terms in week ${n.fellOn!.week}`;
      out += `<div class="nat gone${f?.yours ? ' freeable' : ''}"${f && f.capital >= 0 ? ` data-act="showmap" data-id="${f.capital}" title="Show ${esc(f.capitalName)} on the map"` : ''}>${chip(n.id)}<b>${esc(n.name)}</b><small>fell ${date(n.fellOn!.turn)}${f?.yours ? ' <i class="tag good">You can free it</i>' : ''}</small><div class="will"><small>${how}${f && isOpen(g) ? `. ${esc(f.note)}` : ''}</small></div></div>`;
    }
  }
  out += leaguePane();
  out += trackPane(neutral);
  return out;
}

/** One country's answer to an invitation, with the button that sends it when the player may. */
function inviteRow(x: { nat: string; ok: boolean; why: string }, act: string, label: string, can: boolean, extra = ''): string {
  return `<div class="lgrow${x.ok ? '' : ' no'}">${chip(x.nat)}<span class="grow"><b>${esc(natName(x.nat))}</b><small>${esc(x.why)}</small></span>${x.ok ? `<button class="btn small" data-act="${act}" data-nat="${x.nat}" ${extra} ${can ? '' : 'disabled'}>${label}</button>` : '<small class="stand">Would refuse</small>'}</div>`;
}

/** The countries a member of the League may invite, willing ones first. */
function inviteList(): string {
  const me = g.nations[g.human!];
  const list = leagueInvites(g);
  const yes = list.filter((x) => x.ok), no = list.filter((x) => !x.ok);
  const can = (id: string): boolean => whyNotLeagueInvite(g, id) === null;
  return `<div class="facts"><div class="fact"><span>Political capital</span><b>${Math.floor(me.pc)}</b></div><div class="fact"><span>Each month</span><b>+${pcIncome(me.id)}</b></div><div class="fact"><span>An invitation</span><b>${LEAGUE.invite.cost}</b></div></div>
    ${yes.length ? `<div class="lgrows">${yes.map((x) => inviteRow(x, 'leagueinvite', `Invite · ${LEAGUE.invite.cost}`, can(x.nat))).join('')}</div>` : '<p class="sub">No country is ready to join today. A country says yes when it is a friend of a member or has a nation at war on its border.</p>'}
    ${no.length ? `<details class="lgno"><summary>${no.length} ${no.length === 1 ? 'country' : 'countries'} would refuse today</summary><div class="lgrows">${no.map((x) => inviteRow(x, '', '', false)).join('')}</div></details>` : ''}`;
}

/** The League on the Powers screen: who belongs to it, and for a member in the player's hands, whom to invite. */
function leaguePane(): string {
  if (!isOpen(g)) return '';
  const members = leagueMembers(g);
  const me = g.human && g.nations[g.human].alive ? g.nations[g.human] : null;
  const mine = !!me?.league;
  if (!leagueFounded(g)) {
    return me && paths(g).length ? `<h3 class="lgh"><i></i>A third alliance</h3><p class="sub">No League exists yet. As a small country you could found one with a willing neighbour: an alliance apart from the Axis and the Allies whose members defend one another.</p><button class="btn small" data-act="path">Your path ▸</button>` : '';
  }
  let out = `<h3 class="lgh"><i></i>${esc(upFirst(LEAGUE.full))}</h3><p class="sub">A third alliance, of smaller nations, that belongs to neither the Axis nor the Allies. Founded ${date(g.flags['league:born'])}. War on one member is war on them all.</p>`;
  if (!members.length) return out + '<p class="sub">Every member has fallen. The League is history.</p>';
  for (const n of members) out += `<div class="nat lg">${chip(n.id)}<b>${esc(n.name)}${n.id === g.human ? ' <span class="tag good">You</span>' : ''}</b><small>${divsOf(fieldArmy(g, n.id))}</small><div class="will"><small>${n.atWar.length ? `At war with ${n.atWar.map(natName).join(', ')}` : 'At peace'} · member since ${date(g.flags[`league:${n.id}`] ?? g.flags['league:born'])}</small></div></div>`;
  if (mine && humanUp(g)) out += `<h3>Invite a country</h3>${inviteList()}`;
  else if (me && paths(g).some((o) => o.id === 'join' && o.ok)) out += `<button class="btn small" data-act="path">Ask to join ▸</button>`;
  return out;
}

const PATH_TEXT: Record<PathId, { title: string; text: string }> = {
  axis: { title: 'Join the Axis', text: 'Germany\'s side. Axis armies may cross your land and yours theirs, you share their supply lines, and you can go to war beside them. You are at war with nobody until you declare it or the Allies do.' },
  allies: { title: 'Join the Allies', text: 'Britain\'s side. Allied armies may cross your land and yours theirs, you share their supply lines, and you can go to war beside them. You are at war with nobody until you declare it or the Axis does.' },
  alone: { title: 'Stand alone', text: 'You are at nobody\'s side and nobody\'s quarrel is yours. If you are attacked you fight alone, unless you choose a side then.' },
  found: { title: 'Found the League', text: 'A third alliance, of smaller nations, apart from the Axis and the Allies. Its members are allies of one another, and war on one is war on them all. Pick the country that founds it with you; as a member you can invite more.' },
  join: { title: 'Join the League', text: 'The third alliance, of smaller nations, apart from the Axis and the Allies. Its members are allies of one another, and war on one is war on them all.' },
};

/** The card where a small country in the player's hands chooses its path. 'ask' is a path waiting for the player's yes. */
function pathCard(ask: string): string {
  const me = g.nations[g.human!];
  const opts = paths(g);
  const mine = bloc(me);
  const can = humanUp(g);
  const members = leagueMembers(g).filter((n) => n.id !== me.id);
  const foes = me.atWar.filter((x) => g.nations[x]?.alive);
  const where = mine === 'league' ? `a member of ${LEAGUE.name}${members.length ? `, with ${listOf(members.map((n) => n.name))}` : ''}` : mine ? `a member of ${mine === 'axis' ? 'the Axis' : 'the Allies'}` : standPhrase(me);
  const row = (o: PathOption): string => {
    const T = PATH_TEXT[o.id];
    let text = T.text, body = '';
    if (o.id === 'alone' && !o.now && o.ok) text = `Step back to strict neutrality: you stop leaning toward ${step(me) > 0 ? 'the Axis' : 'the Allies'}, and answer to nobody.`;
    if (o.id === 'join' && members.length) text += ` Its members today: ${listOf(members.map((n) => n.name))}.`;
    if (o.id === 'found' && o.ok) {
      const list = leaguePartners(g);
      const yes = list.filter((x) => x.ok), no = list.filter((x) => !x.ok);
      body = `<div class="lgrows">${yes.map((x) => inviteRow(x, 'takepath', 'Found it together', can, 'data-path="found"')).join('')}</div>${no.length ? `<details class="lgno"><summary>${no.length} ${no.length === 1 ? 'country' : 'countries'} would refuse today</summary><div class="lgrows">${no.map((x) => inviteRow(x, '', '', false)).join('')}</div></details>` : ''}`;
    }
    if (o.now && o.id === 'join' && can) body = `<h4>Invite a country</h4>${inviteList()}`;
    const final = o.id === 'axis' || o.id === 'allies' || o.id === 'join';
    const btn = o.now ? '<span class="tag good">Your path today</span>'
      : !o.ok ? `<small class="pathno">${esc(o.why)}</small>`
        : o.id === 'found' ? ''
          : ask === o.id ? `<span class="pathask"><b>${final ? 'An alliance, once joined, is not left.' : 'Are you sure?'}</b><button class="btn warn" data-act="takepath" data-path="${o.id}" ${can ? '' : 'disabled'}>Yes, ${T.title[0].toLowerCase()}${T.title.slice(1)}</button><button class="btn" data-act="path">No</button></span>`
            : `<button class="btn" data-act="path" data-ask="${o.id}" ${can ? '' : 'disabled'}>${T.title} ▸</button>`;
    const title = !o.now ? T.title : o.id === 'join' ? upFirst(LEAGUE.full) : o.id === 'axis' ? 'The Axis' : o.id === 'allies' ? 'The Allies' : T.title;
    return `<div class="pathrow ${o.id}${o.now ? ' now' : ''}${o.ok || o.now ? '' : ' off'}"><div class="pathhead"><b>${esc(title)}</b>${btn}</div>${o.ok || o.now ? `<p>${esc(text)}</p>` : ''}${body}</div>`;
  };
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Your country's path · ${date(g.turn)}</div><h2>${flagIcon(me.id, me.color, 34, 'inh')} Where ${esc(theName(me.id))} stands</h2></header>
    <div class="body path"><p class="lead">${esc(upFirst(theName(me.id)))} is ${esc(where)}.${foes.length ? ` You are at war with ${esc(nameList(foes))}.` : ' You are at peace.'} A small country chooses its own path, and nobody chooses it for you: no treaty signs your land away, and no courting moves you.</p>
    ${opts.map(row).join('')}</div>
    <footer><button class="btn go" data-act="close">Close</button></footer>`;
}

/** The alignment track: where every country at peace stands between the two blocs, and which way it is drifting. */
function trackPane(list: Nation[]): string {
  const me = g.human && g.nations[g.human].alive ? g.nations[g.human] : null;
  const side = me ? bloc(me) : null;
  // Only the two blocs court neutrals: the League invites, and a country alone has nobody to court for.
  const mine = side === 'axis' || side === 'allies' ? side : null;
  const m = momentum(g);
  const wind = Math.abs(m) < 0.15 ? 'Neither side has the upper hand, so neutrals are sitting tight.' : `${m > 0 ? 'The Axis is' : 'The Allies are'} winning, and neutrals ${Math.abs(m) > 0.6 ? 'are bending' : 'are starting to bend'} that way.`;
  let out = `<h3>The alignment track</h3><p class="sub">Every country stands somewhere between the two blocs. The dot is its public opinion. ${wind}</p>`;
  if (me && mine) {
    const agreed = tradeRules(g);
    const partners = agreed ? dealsOf(g, me.id).length : tradePartners(g, me.id).length;
    out += `<div class="facts"><div class="fact"><span>Political capital</span><b>${Math.floor(me.pc)}</b></div><div class="fact"><span>Each month</span><b>+${pcIncome(me.id)}</b></div><div class="fact"><span>Trade partners</span><b>${partners}</b></div></div>
      <p class="sub" style="margin-top:6px">Courting a neutral costs ${DIPLOMACY.court.cost} and pulls it toward ${mine === 'axis' ? 'the Axis' : 'the Allies'}. ${agreed ? 'Trade is by agreement now, with any country at peace with you whose people can stand your alliance' : `A country that leans your way trades with you${DIPLOMACY.majors.includes(me.id) ? `, worth ${money(DIPLOMACY.tradeIncome)} a month to your treasury` : ''}`}; one that is fully won over joins your bloc.</p>`;
  }
  out += `<div class="trackhead"><span>◂ Allies</span><span>Independent</span><span>Axis ▸</span></div>`;
  const rows = list.filter((n) => !n.league && !ownPath(g, n.id) && (Math.abs(step(n)) <= 1 || !n.atWar.length)).sort((a, b) => a.opinion - b.opinion);
  for (const n of rows) {
    const pos = 50 + Math.max(-100, Math.min(100, n.opinion)) / 2;
    let btn = '';
    if (me && mine && courtable(n) && humanUp(g)) {
      const c = canCourt(g, me.id, n.id);
      const k = receptive(n, mine);
      const word = k >= 1.3 ? 'Receptive' : k >= 0.8 ? 'Open to offers' : k >= 0.5 ? 'Wary' : 'Very hard to move';
      btn = `<button class="btn small" data-act="court" data-nat="${n.id}" ${c.ok ? '' : 'disabled'} title="${esc(c.ok ? word : c.why)}">Court</button>`;
    }
    const frs = friendsOf(n.id).filter((f) => g.nations[f.nat]?.alive);
    out += `<div class="trk"${frs.length ? ` title="Friends of ${esc(n.name)}: ${esc(frs.map((f) => `${natName(f.nat)} (${f.why})`).join('; '))}"` : ''}>${chip(n.id)}<b>${esc(n.name)}</b><small>${standLabel(n)}</small>${btn}${frs.length ? `<small class="frs">Friends: ${esc(listOf(frs.map((f) => natName(f.nat))))}</small>` : ''}
      <div class="track"><i class="t" style="left:15%"></i><i class="t" style="left:35%"></i><i class="t mid" style="left:50%"></i><i class="t" style="left:65%"></i><i class="t" style="left:85%"></i><i class="dot" style="left:${pos.toFixed(1)}%"></i></div></div>`;
  }
  out += `<p class="sub" style="margin-top:8px">The inner marks are where a country starts to lean; the outer marks are where it asks to join.</p>`;
  return out;
}

// ---- The war at sea -----------------------------------------------------------------------

function whereFleet(f: Fleet): string {
  if (!atSea(f)) return `In harbour at ${esc(f.port)}`;
  return `${esc(fleetDoing(g, f))} from ${esc(f.port)}`;
}

const pc100 = (x: number): string => `${Math.round(x * 100)}%`;
const SHIP_CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];

/** Ships that are reported and not identified, as a row: whose they are, where, and roughly how many. */
function shipsReportRow(s: FleetSeen): string {
  return `<div class="corps fleet vague"><svg width="34" height="24" viewBox="-14 -9 28 18" class="ctr"><path class="hull" d="${HULL}" fill="${g.nations[s.nat].color}" opacity=".45"/><text x="0" y="3.2" class="qm">?</text></svg><span class="nm">${esc(s.name)}</span><span></span>
    <span class="meta">${s.atSea ? `At sea in the ${esc(g.seas[s.zone].name)}` : `In harbour at ${esc(g.regions[s.base]?.name ?? 'their base')}`}</span>
    <span class="meta">${esc(s.size)}. Not identified.</span></div>`;
}

const HOW_TO_FIND = 'A fleet is identified by fighting it, by ships and aircraft that search its sea well, or lying in a harbour in full view. A thin search gives only a report: ships are there, not what they are. Send aircraft to search a sea from the Air tab.';

function fleetRow(f: Fleet, mine: boolean, whose = false): string {
  const adm = f.admiral ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(f.admiral.name)} <span class="lvl" title="Level ${f.admiral.level} admiral">${f.admiral.level}</span></span>` : '';
  const lost = shipCount(f.lost);
  const el = mine ? 'button' : 'div';
  const wings = f.ships.cv > 0 && carrierRules(g) ? wingsOfFleet(g, f.id) : [];
  const aboard = wings.length ? ` · ${num(wings.reduce((t, w) => t + w.aircraft, 0))} aircraft aboard` : '';
  return `<${el} class="corps fleet${mine && ui.fleet === f.id ? ' pick' : ''}" ${mine ? `data-act="fleet" data-id="${f.id}"` : ''}>
    ${hullIcon(f)}<span class="nm">${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}${adm}</span><span></span>
    <span class="meta">${whose ? `${g.nations[f.nat].adj} · ` : ''}${whereFleet(f)}</span>
    <span class="meta">${esc(capital(shipList(f.ships)))}${aboard}${lost ? ` · lost so far: ${esc(shipList(f.lost))}` : ''}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${f.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(f.ready * 100)}%"></i></span></span></${el}>${recStrip('fleet', f)}`;
}

/** The orders the player can give the fleet that is picked. */
function fleetOrders(f: Fleet): string {
  const o = fleetOptions(g, f.id);
  if (!o) return '';
  const pieces = seaWar(g);
  const now = fleetStance(f);
  const small = (t: string): string => `<span style="font-size:11.5px;color:var(--ink-2)">${t}</span>`;
  const rows = o.seas.map((s) => {
    const them = seaSide(g, s.zone, f.nat, true);
    const foe = s.theirs >= 1 ? `Enemy strength ${num(s.theirs)}${them.air > s.theirs - them.air ? ', mostly aircraft' : ''}` : 'No enemy seen';
    const more: string[] = [];
    if (s.reported > 0) more.push(`${s.reported} enemy force${s.reported === 1 ? '' : 's'} reported, not identified`);
    if (carrierRules(g) && s.search && s.search !== 'In full view') more.push(esc(s.search.split(':')[0].toLowerCase()));
    if (s.subs > 0) {
      const sunk = s.subs * s.hunt;
      more.push(`${s.subs} enemy submarine${s.subs === 1 ? '' : 's'} at work${pieces ? `: a hunt here would sink ${sunk < 0.5 ? 'less than one' : `about ${Math.round(sunk)}`} a month` : ''}`);
    }
    if (s.torpedo >= 0.02) more.push(`<span style="color:var(--red)">${pc100(s.torpedo)} chance of a heavy ship torpedoed: too few destroyers</span>`);
    if (now === 'patrol' && s.meet !== 'clear') more.push(s.meet === 'battle' ? 'on patrol it would bring the enemy fleet to battle' : 'the enemy fleet is too strong here to offer battle');
    return `<button class="pickrow${s.here ? ' pick' : ''}" data-act="sail" data-zone="${s.zone}"><span class="grow"><b>${esc(s.name)}</b><br>${small(`${foe} · your side ${num(s.ours)}${more.length ? `<br>${more.join(' · ')}` : ''}`)}</span>${s.here ? '<span class="tag good">Here now</span>' : ''}</button>`;
  }).join('');
  const st = (s: Stance, label: string): string => `<button class="btn small${now === s ? ' on' : ''}" data-act="stance" data-stance="${s}" title="${esc(FLEETS.orders[s].blurb)}">${label}</button>`;
  const will = now === 'raid' ? 'raid shipping' : now === 'hunt' ? 'hunt submarines' : 'patrol';
  let out = `<div class="orders">
    <div class="acts">${st('patrol', 'Patrol')}${st('raid', 'Raid shipping')}${pieces ? st('hunt', 'Hunt submarines') : ''}<button class="btn small" data-act="toport" ${atSea(f) ? '' : 'disabled'} title="A fleet in harbour refits quickly.">Return to port</button><button class="btn small${ui.rebase ? ' on' : ''}" data-act="rebase">Change base</button>${o.split ? '<button class="btn small" data-act="fleetsplit" title="Send some of its ships off as a squadron of their own: an escort group for a lane, a hunting group, a bombardment force.">Split off ships</button>' : ''}</div>`;
  const wings = wingsOfFleet(g, f.id);
  if (wings.length) {
    out += `<h3>Its aircraft</h3><p class="sub" style="margin:0 0 6px">A carrier is a floating airfield. At sea its aircraft reach every coast of the sea they are in, inland as far as their range allows, and the seas next door. Fighters keep enemy bombers off the fleet; strike aircraft go for ships, harbours and targets ashore. They are made up to strength only in harbour.</p>${wings.map((w) => `<button class="pickrow" data-act="wing" data-id="${w.id}">${groupIcon(w.kind, f.nat)}<span class="grow"><b>${esc(w.label)}</b> · ${num(w.aircraft)} of ${num(w.est)}<br>${small(esc(`${w.doing}. Reach ${num(w.range)} km.${w.aircraft < w.est ? ` ${w.note}` : ''}`))}</span><span class="tag${w.flew ? '' : ' good'}">${w.flew ? 'Flew' : w.kind === 'bmr' ? 'Plan a strike' : 'Orders'}</span></button>`).join('')}`;
  }
  if (pieces && o.lanes.length) {
    out += `<h3 id="flanes">Escort a shipping lane</h3><p class="sub" style="margin:0 0 6px">An escort sails with the convoys the whole length of the lane. Destroyers count most, carriers and cruisers help, and battleships guard nothing.</p>${o.lanes.map((p) => `<div class="pickrow${p.bound ? ' pick' : ''}">${flagIcon(p.to, g.nations[p.to].color, 18, 'inh')}<span class="grow"><b>${esc(upFirst(p.name))}</b><br>${small(p.bound ? `This fleet is its escort. ${pc100(p.now)} of the cargoes are being sunk.` : p.why ? esc(p.why) : p.raid <= 0 ? 'No raiders are on it now.' : `Cargoes sunk: ${pc100(p.now)} now, ${pc100(p.then)} with this fleet as its escort.`)}</span>${p.bound ? '<span class="tag good">Escorting</span>' : `<button class="btn small go" data-act="escort" data-lane="${p.lane}" ${p.why ? 'disabled' : ''}>Escort</button>`}</div>`).join('')}`;
  }
  if (pieces) {
    const card = fleetView(g, f.id)!;
    const none = !atSea(f) ? 'In harbour. Send the fleet to sea off an enemy coast first.' : card.guns < FLEETS.bombard.least * 0.5 || f.ships.bb + f.ships.ca <= 0 ? 'Too few heavy guns: shelling a coast is work for battleships and cruisers.' : card.shelled ? 'Its guns have already spoken this month.' : 'No enemy coast on this sea has troops in sight or industry to hit.';
    out += `<h3>Coasts under its guns</h3>${o.coasts.length ? o.coasts.map((id) => {
      const r = g.regions[id];
      const p = previewBombard(g, f.id, id)!;
      const bits = [p.can.troops ? `${p.corps} corps, ${p.divs} division${p.divs === 1 ? '' : 's'}: organisation cut by about ${pc100(p.cut)}` : '', p.can.city ? `${r.fac} factor${r.fac === 1 ? 'y' : 'ies'}: ${pc100(p.output)} of its output stopped` : ''].filter(Boolean);
      return `<button class="pickrow" data-act="shell" data-id="${id}"><span class="grow"><b>${esc(r.name)}</b><br>${small(bits.join(' · '))}</span><span class="tag bad">Shell</span></button>`;
    }).join('') : `<div class="empty">${none}</div>`}`;
  }
  if (o.merge.length) {
    out += `<h3>Join another fleet here</h3>${o.merge.map((id) => { const x = g.fleets.find((q) => q.id === id)!; return `<div class="pickrow">${hullIcon(x)}<span class="grow"><b>${esc(x.name)}</b><br>${small(esc(capital(shipList(x.ships))))}</span><button class="btn small" data-act="fleetjoin" data-id="${id}" title="The ships of the ${esc(f.name)} sail under the ${esc(x.name)}'s flag from now on.">Join it</button></div>`; }).join('')}`;
  }
  out += `<h3>Seas in reach</h3><p class="sub" style="margin:0 0 6px">Fleet strength ${num(fleetPower(g, f))}. It can reach the seas within ${NAVY.range} of ${esc(f.port)}: click a ringed sea on the map, or pick one here. It will ${will} there.</p>${rows}</div>`;
  return out;
}

/** The plan for a bombardment before the guns open: what it would do to the troops or the factories, and what it costs the fleet. */
function shellCard(): string {
  const m = ui.modal!;
  const f = g.fleets.find((x) => x.id === m.fleet);
  const r = g.regions[m.target];
  const p = f ? previewBombard(g, f.id, r.id, m.what) : null;
  if (!f || !p) return `<header><button class="x" data-act="close" aria-label="Close">×</button><h2>${esc(r.name)}</h2></header><div class="body"><div class="empty">That fleet cannot be given orders now.</div></div><footer><button class="btn go" data-act="close">Close</button></footer>`;
  const adj = g.nations[r.ctrl].adj;
  const aims = ([['troops', 'Shell the troops'], ['town', "Shell the town's defences"], ['city', 'Shell the factories']] as const).filter(([k]) => p.can[k]);
  const pick = aims.length > 1
    ? `<div class="acts" style="margin-bottom:8px">${aims.map(([k, label]) => `<button class="btn small${p.what === k ? ' on' : ''}" data-act="shellwhat" data-what="${k}">${label}</button>`).join('')}</div>`
    : '';
  let head: string;
  if (p.why) head = `<div class="empty">${esc(p.why)}</div>`;
  else if (p.what === 'troops') {
    head = `<div class="verdict ${p.cut >= 0.2 ? 'win' : ''}" style="font-size:24px">${p.cut >= 0.2 ? 'A heavy bombardment' : p.cut >= 0.1 ? 'A useful bombardment' : 'A light bombardment'}</div>
      <div class="sub">The ${adj} troops at ${esc(r.name)} should lose about ${pc100(p.cut)} of their organisation, and some of their men and guns. They go into their next battle the weaker for it, so shell first and attack after.${p.reply > 0 ? ` The coast is fortified and will answer.` : ''}</div>`;
  } else if (p.what === 'town' && p.town) {
    head = `<div class="verdict ${p.town.works >= 15 ? 'win' : ''}" style="font-size:24px">${p.town.works >= 25 ? 'The walls under heavy fire' : p.town.works >= 8 ? 'The walls under fire' : 'A light shelling'}</div>
      <div class="sub">${esc(townHitText(g, r, p.town, 'shells', false))} Shell first and storm after.${p.reply > 0 ? ` The coast is fortified and will answer.` : ''}</div>`;
  } else {
    head = `<div class="verdict ${p.output >= 0.3 ? 'win' : ''}" style="font-size:24px">${p.output >= 0.3 ? 'The works under heavy fire' : 'The works under fire'}</div>
      <div class="sub">The shelling should stop about ${pc100(p.output)} of ${esc(r.name)}'s output for a month: ${(p.nation * 100).toFixed(1)}% of ${adj} production.${p.reply > 0 ? ` The coast is fortified and will answer.` : ''}</div>`;
  }
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Shore bombardment · ${esc(f.name)} · ${date(g.turn)}</div><h2>${esc(p.what === 'city' ? `Shelling ${r.name}` : p.what === 'town' ? `Shelling the defences of ${townName(r)}` : `Shelling the troops at ${r.name}`)}</h2></header>
    <div class="body">${pick}${head}${p.why ? '' : `<h3>Battle Ticket</h3>${ticketTable(p.lines)}<div class="note"><b>Fired at once.</b> A fleet's guns speak once a month, and a coast is shelled once a month.</div>`}</div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="doshell" ${p.why ? 'disabled' : ''}>Open fire</button></footer>`;
}

/** How a bombardment went, for whoever fired it and whoever was under it. */
function shoreVerdict(r: ShoreReport): { text: string; cls: string } {
  const ours = !!g.human && allied(g, r.nat, g.human), theirs = !!g.human && allied(g, r.defNat, g.human);
  const heavy = r.what === 'troops' ? r.cut >= 0.2 : r.what === 'town' ? (r.works ?? 0) >= 25 : r.output >= 0.3;
  return { text: heavy ? 'Heavy bombardment' : 'Bombardment', cls: ours ? 'win' : theirs ? 'loss' : '' };
}

function shoreRow(r: ShoreReport): string {
  const v = shoreVerdict(r);
  return `<button class="brow" data-act="shorereport" data-id="${r.id}"><b>${esc(r.place)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)} · shelled from the sea by the ${esc(r.fleet)}</span></button>`;
}

function shoreReportCard(r: ShoreReport): string {
  const v = shoreVerdict(r);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Battle Ticket · shore bombardment · ${date(r.turn)}</div><h2>${esc(r.place)} shelled from the ${esc(g.seas[r.zone]?.name ?? 'sea')}</h2></header>
    <div class="body"><div class="verdict ${v.cls}" style="font-size:24px">${v.text}</div><div class="sub">${esc(r.result)}</div>
    <h3>Battle Ticket</h3>${ticketTable(r.lines)}
    <h3>The ships that stood in</h3><ul class="ulist"><li><span>${esc(r.fleet)} <span style="color:var(--ink-3)">(${g.nations[r.nat].adj}${r.admiral ? `, ${esc(r.admiral)}` : ''})</span><br><span style="color:var(--ink-2)">${esc(capital(shipList(r.ships)))}</span></span><span>${shipCount(r.sunk) ? `lost ${esc(shipList(r.sunk))}` : 'no ships lost'}</span></li></ul></div>
    <footer><button class="btn" data-act="showmap" data-id="${r.region}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

/** Choosing the ships to send off from a fleet as a squadron of their own. */
function splitCard(): string {
  const m = ui.modal!;
  const f = g.fleets.find((x) => x.id === m.fleet);
  if (!f) return `<header><button class="x" data-act="close" aria-label="Close">×</button><h2>Split off ships</h2></header><div class="body"><div class="empty">That fleet is gone.</div></div><footer><button class="btn go" data-act="close">Close</button></footer>`;
  const take = m.take as Ships;
  const why = whyNotSplit(g, f.id, take);
  const n = SHIP_CLASSES.reduce((s, c) => s + take[c], 0);
  const rows = SHIP_CLASSES.filter((c) => f.ships[c] > 0).map((c) => {
    const step = (d: number, label: string): string => `<button class="btn small" data-act="splitadj" data-cls="${c}" data-d="${d}" ${(d < 0 ? take[c] <= 0 : take[c] >= f.ships[c]) ? 'disabled' : ''}>${label}</button>`;
    return `<div class="pickrow splitrow">${shipPlate(f.nat, c, 30, 21)}<span class="grow"><b>${esc(NAVY.classes[c].label)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${esc(FLEETS.brief[c])} · ${f.ships[c]} in the fleet</span></span><span class="stepper">${step(-5, '−5')}${step(-1, '−')}<b>${take[c]}</b>${step(1, '+')}${step(5, '+5')}</span></div>`;
  }).join('');
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${esc(f.name)} · ${esc(fleetDoing(g, f))}</div><h2>Split off ships</h2></header>
    <div class="body"><p class="sub" style="margin-top:0">The ships you choose form a squadron of their own, in the same place and under the same order. Give it work of its own afterwards: destroyers to escort a lane or hunt submarines, battleships and cruisers to shell a coast.</p>${rows}
    <div class="note">${why ? esc(why) : `<b>${esc(squadronName(g, f.nat, take))}</b> will form with ${esc(shipList(take))}. ${shipCount(f.ships) - n} ship${shipCount(f.ships) - n === 1 ? '' : 's'} stay with the ${esc(f.name)}.`}</div></div>
    <footer><button class="btn" data-act="close">Cancel</button><button class="btn go" data-act="dosplit" ${why ? 'disabled' : ''}>Form the squadron</button></footer>`;
}

/** What a route that feeds an army overseas means for that army this month. */
function desertNote(route: (typeof NAVY.convoys)[number]): string {
  if (route.id !== DESERT.route.id) return '';
  const extra = convoyDrag(g, route.to);
  const line = extra > 0.05 ? `Cargoes sunk make the Axis supply line in Libya ${extra.toFixed(1)} regions longer this month.` : 'The Axis army in Libya is getting what it is sent.';
  const b = route.base ? g.byName[route.base.region] : null;
  if (!b) return ` ${line}`;
  if (!atWar(g, b.ctrl, route.to)) return ` ${line} ${esc(b.name)} no longer threatens the route.`;
  const starved = !!g.supply[b.ctrl] && !g.supply[b.ctrl][b.id];
  return ` ${line} ${esc(b.name)} is hunting the convoys${starved ? ', but is itself cut off and short of everything' : ''}. In Axis hands the route would be safe.`;
}

/** The convoy war and the ore traffic, for whoever has a stake in them. */
function seaNotes(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  let out = '';
  for (const route of NAVY.convoys) {
    const m = g.convoys[route.id];
    if (!m || (me && me !== route.to && !atWar(g, me, route.to))) continue;
    const share = m.raid + m.guard > 0 ? m.raid / (m.raid + m.guard * NAVY.convoy.escortWorth) : 0;
    out += `<div class="note"><b>${esc(capital(route.name))}.</b> ${num(m.tons)} tons of shipping sunk ${m.turn === g.turn ? 'this month' : 'last month'}, cutting ${g.nations[route.to].adj} production by ${(m.share * 100).toFixed(m.share < 0.095 ? 1 : 0)}%. ${m.subsLost} submarine${m.subsLost === 1 ? '' : 's'} and ${m.escortsLost} escort${m.escortsLost === 1 ? '' : 's'} lost. ${num(m.total)} tons since the war began.
      <div class="oddsbar" title="Raiders against escorts"><i style="width:${Math.round(share * 100)}%;background:var(--red)"></i><i style="flex:1;background:#6f9fd0"></i></div><span style="font-size:11.5px">Raiders ${num(m.raid)} against escorts ${num(m.guard)}. ${seaWar(g) ? 'Fleets ordered to escort the route cut the losses most; destroyers sent to hunt in its seas sink the submarines.' : 'More escorts and patrolling fleets on the route cut the losses.'}${desertNote(route)}</span></div>`;
  }
  const ore = oreRoute(g);
  const to = NAVY.ore.to;
  if (ore !== 'none' && (!me || me === to || atWar(g, me, to))) {
    const text = ore === 'baltic' ? `Swedish iron ore is reaching ${natName(to)} across the Baltic.` : ore === 'narvik' ? `The Baltic ports are frozen. Swedish iron ore is reaching ${natName(to)} through ${NAVY.ore.winterPort}.` : `Swedish iron ore is not getting through. ${natName(to)} loses ${Math.round(NAVY.ore.share * 100)}% of its production until the route is open again.`;
    out += `<div class="note"${ore === 'cut' ? ' style="border-left-color:var(--red)"' : ''}><b>The ore traffic.</b> ${esc(text)}</div>`;
  }
  return out;
}

function navyPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const afloat = (f: Fleet): boolean => shipCount(f.ships) > 0;
  const size = (a: Fleet, b: Fleet): number => fleetPower(g, b) - fleetPower(g, a);
  let out = '';
  if (me) {
    const mine = fleetsOf(g, me).filter(afloat);
    const n = g.nations[me];
    const order = humanUp(g);
    out += `<h2>The ${n.adj} navy</h2><p class="sub">${seaWar(g) ? 'Your fleets are pieces on the map: click one there, or below, to pick it up and give it orders. A fleet holds a sea by patrolling it, guards a shipping lane by escorting it, and sinks submarines only by hunting them with destroyers and aircraft. Battleships and cruisers can shell an enemy coast. Ships at sea wear down, so bring a fleet home to refit.' : 'A fleet holds a sea by patrolling it. Where one side is much the stronger, that sea is closed to the other\'s troopships and supply ships. Ships at sea wear down, so bring a fleet home to refit after a fight.'}</p>
      <div class="facts"><div class="fact"><span>Ships</span><b>${num(mine.reduce((t, f) => t + shipCount(f.ships), 0))}</b></div><div class="fact"><span>Fleets at sea</span><b>${mine.filter(atSea).length} of ${mine.length}</b></div><div class="fact"><span>Sea lift left</span><b>${n.sealift > 0 ? `${liftLeft(g, me)} corps` : 'None'}</b></div></div>${budgeted(g) ? '<div class="acts"><button class="btn small" data-act="armgo" data-arm="navy" title="Lay down warships and troopships in your shipyards.">Lay down more ships</button></div>' : ''}`;
    if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> The fleet stays in harbour until the nation is at war.</div>`;
    out += seaNotes();
    out += laneNotes();
    out += `<h3>Your fleets</h3>${mine.length ? mine.map((f) => fleetRow(f, order && n.atWar.length > 0) + (order && ui.fleet === f.id && n.atWar.length ? fleetOrders(f) : '')).join('') : '<div class="empty">No ships under your flag.</div>'}`;
    const friends = g.fleets.filter((f) => f.nat !== me && afloat(f) && allied(g, f.nat, me)).sort(size);
    const foes = g.fleets.filter((f) => afloat(f) && atWar(g, f.nat, me)).sort(size);
    if (friends.length) out += `<h3>Allied fleets</h3>${friends.map((f) => fleetRow(f, false, true)).join('')}`;
    if (carrierRules(g)) {
      // Only what has been found: identified fleets in full, reports as reports, and no count of what is still hidden.
      const seen = new Map(fleetsInSight(g).filter((s) => s.side === 'foe').map((s) => [s.id, s]));
      const known = foes.filter((f) => seen.get(f.id)?.known === 'exact');
      const told = [...seen.values()].filter((s) => s.known === 'report');
      if (n.atWar.length) out += `<h3>Enemy fleets found</h3>${known.map((f) => fleetRow(f, false, true)).join('')}${told.map(shipsReportRow).join('')}${known.length + told.length ? '' : '<div class="empty">No enemy fleet has been found.</div>'}<p class="sub" style="margin-top:6px">You see only the enemy ships your side has found. ${HOW_TO_FIND}</p>`;
    } else if (foes.length) out += `<h3>Enemy fleets</h3>${foes.map((f) => fleetRow(f, false, true)).join('')}`;
    return out;
  }
  out += `<h2>The war at sea</h2><p class="sub">A fleet holds a sea by patrolling it. Where one side is much the stronger, that sea is closed to the other's troopships and supply ships. Click any sea on the map to see who holds it.</p>${seaNotes()}${laneNotes()}`;
  const war = g.fleets.filter((f) => afloat(f) && g.nations[f.nat].atWar.length > 0);
  const nats = [...new Set(war.map((f) => f.nat))].sort((a, b) => war.filter((f) => f.nat === b).reduce((t, f) => t + fleetPower(g, f), 0) - war.filter((f) => f.nat === a).reduce((t, f) => t + fleetPower(g, f), 0));
  for (const nat of nats) out += `<h3>${esc(natName(nat))}</h3>${war.filter((f) => f.nat === nat).map((f) => fleetRow(f, false)).join('')}`;
  const peace = g.fleets.filter((f) => afloat(f) && !g.nations[f.nat].atWar.length).sort(size);
  if (peace.length) out += `<h3>Navies at peace</h3>${peace.map((f) => fleetRow(f, false, true)).join('')}`;
  return out;
}

/** One sea: who holds it, which fleets are in it, and what it would cost to cross. */
function seaPane(): string {
  const z = g.seas[ui.sea];
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const afloat = (f: Fleet): boolean => shipCount(f.ships) > 0;
  // In the grand campaign a sea shows only the fleets the player's side has found.
  const pic = me && carrierRules(g) ? seaPictureOf(g, z.id) : null;
  const seen = new Map<number, FleetSeen>(pic ? fleetsInSight(g).map((s) => [s.id, s]) : []);
  const shown = (f: Fleet): boolean => !pic || seen.has(f.id);
  const rowOf = (f: Fleet): string => (pic && seen.get(f.id)!.known !== 'exact' ? shipsReportRow(seen.get(f.id)!) : fleetRow(f, false, true));
  const here = g.fleets.filter((f) => f.zone === z.id && atSea(f) && afloat(f) && shown(f));
  const docked = g.fleets.filter((f) => f.home === z.id && !atSea(f) && afloat(f) && shown(f));
  let head: string;
  if (me && g.nations[me].atWar.length) {
    const us = seaSide(g, z.id, me, false), them = seaSide(g, z.id, me, true);
    const theirs = pic ? pic.theirs : them.total;
    const loss = Math.round(seaRisk(g, z.id, me) * NAVY.intercept.loss * 100);
    head = `<div class="sub">${closedTo(g, z.id, me) ? 'Closed to your troopships and supply ships: the enemy is much the stronger here.' : theirs >= 1 ? 'Contested, but still open to your shipping.' : 'Open to your shipping.'}</div>
      <div class="facts"><div class="fact"><span>Your side</span><b>${num(us.total)}</b></div><div class="fact"><span>The enemy${pic ? ', as found' : ''}</span><b>${num(theirs)}</b></div><div class="fact"><span>Lost crossing</span><b>${loss ? `about ${loss}%` : 'None'}</b></div></div>
      <p class="sub" style="margin-top:6px">Strength counts the fleets at sea here and the aircraft that can reach this sea from the coasts each side holds${us.air + them.air >= 1 ? `: yours ${num(us.air)}, the enemy's ${num(them.air)}` : ''}. The last figure is the share of a corps likely to be lost if it crossed this sea now.</p>
      ${pic ? `<div class="note"><b>The search.</b> ${esc(pic.word)}.${pic.reported ? ` ${pic.reported} enemy force${pic.reported === 1 ? ' is' : 's are'} reported here and not identified: ${pic.reported === 1 ? 'its' : 'their'} strength is not counted above.` : ''} Ships and carriers here, carriers in the seas next door and aircraft from your coasts all search this sea. A group or a carrier wing can fly a search of it from the Air tab.</div>` : ''}`;
  } else head = `<div class="sub">${z.coast.length} coastal region${z.coast.length === 1 ? '' : 's'} on this sea.</div>`;
  let straits = '';
  for (const gt of g.gates) {
    if (gt.a !== z.id && gt.b !== z.id) continue;
    const other = g.seas[gt.a === z.id ? gt.b : gt.a];
    const holder = gt.region >= 0 ? g.regions[gt.region].ctrl : gt.nation!;
    straits += `<div class="note"><b>${esc(capital(gt.name))}.</b> The way to the ${esc(other.name)}, held by ${esc(natName(holder))}.${me ? ` ${canPass(g, gt.a, gt.b, me) ? 'Open to your ships.' : 'Closed to your ships.'}` : ''}</div>`;
  }
  return `<h2>${esc(z.name)}</h2>${head}${straits}
    <h3>Fleets at sea here</h3>${here.length ? here.map(rowOf).join('') : `<div class="empty">${pic ? 'No fleet has been found at sea here.' : 'No fleet at sea here.'}</div>`}
    ${docked.length ? `<h3>In harbour on this sea</h3>${docked.map(rowOf).join('')}` : ''}
    ${me && humanUp(g) ? `<p class="sub" style="margin-top:10px">${seaWar(g) ? 'To send a fleet here, click the fleet on the map to pick it up, then click this sea.' : 'To send a fleet here, open <b style="color:var(--ink)">Navy</b>, pick the fleet, then click this sea.'}</p>` : ''}`;
}

function seaReportCard(r: SeaReport): string {
  const v = seaVerdict(r);
  const count = (ss: SeaSide[], k: 'ships' | 'sunk'): number => ss.reduce((n, x) => n + shipCount(x[k]), 0);
  const side = (ss: SeaSide[]): string => `<div class="big">${count(ss, 'ships')}</div><small>ships in ${ss.length} fleet${ss.length > 1 ? 's' : ''} · lost ${count(ss, 'sunk')}</small>`;
  const list = (ss: SeaSide[]): string => `<ul class="ulist">${ss.map((x) => `<li><span>${esc(x.fleet)} <span style="color:var(--ink-3)">(${g.nations[x.nat].adj}${x.admiral ? `, ${esc(x.admiral)}` : ''})</span><br><span style="color:var(--ink-2)">${esc(capital(shipList(x.ships)))}</span></span><span>${shipCount(x.sunk) ? `lost ${esc(shipList(x.sunk))}` : 'no ships lost'}</span></li>`).join('')}</ul>`;
  const beaten = g.nations[r.won ? r.defNat : r.attNat].adj;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Battle Ticket · fleet action · ${date(r.turn)}</div><h2>Battle of the ${esc(r.zoneName)}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">The ${beaten} ships broke off and made for port. Final odds ${r.odds.toFixed(2)} to 1.</div>
      <div class="vs"><div><h4>${chip(r.attNat)} ${natName(r.attNat)} offering battle</h4>${side(r.att)}</div><div class="mid">vs</div><div class="r"><h4>${natName(r.defNat)} ${chip(r.defNat)}</h4>${side(r.def)}</div></div>
      <h3>What the campaign map fed into the battle</h3>
      <table class="ticket">${r.lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>
      <h3>Offering battle</h3>${list(r.att)}<h3>Brought to battle</h3>${list(r.def)}
      <div class="note" style="margin-top:14px"><b>Auto-resolved.</b> A fleet action is settled from its ticket in the same way as a land battle, so the full game can hand it to a 3D sea battle and take the same kind of result back.</div>
    </div><footer><button class="btn" data-act="showsea" data-id="${r.zone}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

/** The plan for a landing from the sea: the crossing, the odds on the beach, and the order to go. */
function landingCard(): string {
  const m = ui.modal!;
  const target = g.regions[m.target];
  const cs = g.corps.filter((c) => m.corps.has(c.id));
  const plan = previewLanding(g, cs, target.id);
  let body: string;
  if (plan.why) body = `<div class="empty">${esc(plan.why)}</div>`;
  else {
    const a = plan.assess;
    const air = !!plan.air;
    const route = plan.path.map((z) => g.seas[z].name).join(', then the ');
    const loss = Math.round(plan.risk * NAVY.intercept.loss * 100);
    const crossing = plan.surprise
      ? `War has only just been declared, so the enemy is caught unready: no fleet can stop the crossing this month, and the troops go ashore at full strength.`
      : loss > 0 ? `${plan.sortie >= 1 ? 'The enemy fleet will put to sea against the invasion' : 'Enemy ships and aircraft are at work on the way'}${plan.sky ? ', but your air force holds the sky over the beach and will keep most of it off the troopships' : ''}. Expect to lose about ${loss}% of the force at sea.` : 'No enemy ship or aircraft is in a position to stop the crossing.';
    let odds = `<div class="verdict win" style="font-size:24px">The coast is undefended</div><div class="sub">The force walks ashore and takes ${esc(target.name)}.</div>`;
    if (a) {
      const p = chanceOn(a.odds, a.dice);
      const share = a.A / (a.A + a.D);
      const word = p >= 0.95 ? 'Victory is all but certain' : p >= 0.6 ? 'The odds favour you' : p >= 0.3 ? 'It could go either way' : p > 0 ? 'A long shot' : air ? 'This assault will fail' : 'This landing will fail';
      odds = `<div class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}" style="font-size:24px">${word}</div>
        <div class="oddsbar"><i style="width:${Math.round(share * 100)}%;background:${g.nations[g.human!].color}"></i><i style="flex:1;background:${g.nations[a.defenders[0].nat].color}"></i></div>
        <div class="sub">Odds ${a.odds.toFixed(2)} to 1 · about ${oddsPct(p)}% chance to take ${esc(target.name)}${air ? ` · airborne troops without their heavy weapons count for ${Math.round(NAVY.landing.airborne * 100)}%` : plan.surprise ? '' : ` · troops fighting their way off the beach count for ${Math.round(assaultStrength(g, cs[0].nat) * 100)}%`}</div>`;
    }
    body = `${odds}
      ${air ? `<h3>The flight</h3><p class="lead" style="font-size:13px">Transport aircraft and gliders carry the force across the water. It needs no ships and no fleet can stop it, but the transports can mount only one assault a month${plan.sky ? ', and your air force holds the sky over the target' : ''}.</p>` : `<h3>The crossing</h3><p class="lead" style="font-size:13px">By way of the ${esc(route)}. ${crossing}</p>`}
      ${a ? `<h3>Battle Ticket</h3><table class="ticket">${a.lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>` : ''}
      <h3>${air ? 'Going in' : 'Going ashore'}</h3>${cs.map((c) => `<div class="pickrow pick">${icon(c)}<span class="grow"><b>${esc(c.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">from ${esc(g.regions[c.reg].name)} · ${c.divs} division${c.divs > 1 ? 's' : ''} · ${num(men(c))} men</span></span></div>`).join('')}
      <div class="note"><b>Once ${air ? 'down' : 'ashore'}</b> the force is supplied through a port you hold on a sea the enemy has not closed. If your navy loses that sea, the troops are cut off.</div>`;
  }
  const fromAir = byAir(cs);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${fromAir ? 'Airborne assault' : 'Landing from the sea'} · ${date(g.turn)}</div><h2>${fromAir ? 'Assault on' : 'Landing at'} ${esc(target.name)}</h2></header>
    <div class="body">${body}</div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="doland" ${plan.why ? 'disabled' : ''}>${fromAir ? 'Drop the troops' : 'Land the troops'}</button></footer>`;
}

// ---- The war in the air -------------------------------------------------------------------

const craft = (ftr: number, bmr: number): string => [ftr ? `${num(ftr)} fighter${ftr === 1 ? '' : 's'}` : '', bmr ? `${num(bmr)} bomber${bmr === 1 ? '' : 's'}` : ''].filter(Boolean).join(' and ') || 'no aircraft';

/** The player's air fleet being given orders, if it is still theirs to order. */
function myAir(): AirFleet | null {
  if (ui.air < 0 || !humanUp(g)) return null;
  const f = g.airFleets.find((x) => x.id === ui.air);
  return f && f.nat === g.human ? f : null;
}

function missionText(f: AirFleet): string {
  const where = f.mission === 'strike' ? `the ${g.seas[f.target]?.name ?? 'sea'}` : f.mission === 'fleet' ? `the ${g.fleets.find((x) => x.id === f.target)?.name ?? 'fleet'}` : g.regions[f.target]?.name ?? '';
  switch (f.mission) {
    case 'support': return 'supporting the armies and defending its sky';
    case 'rest': return 'resting and refitting';
    case 'superiority': return `fighting for the sky over ${where}`;
    case 'bomb': return `bombing ${where} by day`;
    case 'night': return `bombing ${where} by night`;
    case 'strike': return `striking at shipping in ${where}`;
    case 'ground': return `raiding the troops at ${where}`;
    case 'line': return `bombing the fieldworks at ${where}`;
    case 'fleet': return `striking ${where}`;
    case 'harbour': return `raiding the harbour of ${where}`;
    case 'town': return `bombing the defences of ${g.regions[f.target] ? townName(g.regions[f.target]) : where}`;
  }
}

function airRow(f: AirFleet, mine: boolean, whose = false): string {
  const ldr = f.leader ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(f.leader.name)} <span class="lvl" title="Level ${f.leader.level} air commander">${f.leader.level}</span></span>` : '';
  const short = Math.max(0, f.est.ftr - f.ftr) + Math.max(0, f.est.bmr - f.bmr);
  const crowd = crowding(g, f);
  const tags: string[] = [];
  if (crowd < 1) tags.push(`<span class="tag bad" title="More aircraft are based in ${esc(g.regions[f.base].name)} than its airfields can serve. Move a fleet to a neighbouring region.">Crowded airfields: flies ${Math.round(crowd * 100)}%</span>`);
  if (short > (f.est.ftr + f.est.bmr) * 0.2) tags.push(`<span class="tag bad">${num(short)} aircraft short</span>`);
  if (f.moved === g.turn) tags.push('<span class="tag">Moved this month</span>');
  const lost = f.lost.ftr + f.lost.bmr;
  const el = mine ? 'button' : 'div';
  return `<${el} class="corps fleet${mine && ui.air === f.id ? ' pick' : ''}" ${mine ? `data-act="airfleet" data-id="${f.id}"` : ''}>
    ${planeIcon(f)}<span class="nm">${esc(f.name)}${f.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}${ldr}</span><span></span>
    <span class="meta">${whose ? `${g.nations[f.nat].adj} · ` : ''}${esc(g.regions[f.base].name)}, ${esc(missionText(f))}</span>
    <span class="meta">${esc(capital(craft(f.ftr, f.bmr)))}${lost ? ` · lost so far: ${esc(craft(f.lost.ftr, f.lost.bmr))}` : ''}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${f.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(f.ready * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>${recStrip('air', f)}`;
}

/** The orders the player can give the air fleet that is picked. */
function airOrders(f: AirFleet): string {
  const R = airRange(f);
  const war = g.nations[f.nat].atWar.length > 0;
  const btn = (m: AirMission): string => {
    const off = isOffensive(m);
    const ok = !off || airTargets(g, f, m).length > 0;
    const why = !war ? 'The nation is at peace.' : m !== 'superiority' && !f.bmr ? 'It has no bombers.' : 'Nothing in reach to fly this against.';
    const on = off ? ui.airMode === m || (!ui.airMode && f.mission === m) : !ui.airMode && f.mission === m;
    return `<button class="btn small${on ? ' on' : ''}" data-act="mission" data-mission="${m}" ${ok ? '' : 'disabled'} title="${esc(ok ? missionWhat(m) : why)}">${MISSION[m].label}</button>`;
  };
  const moved = f.moved === g.turn;
  const btns = (['support', 'superiority', 'bomb', 'night', 'strike', 'rest'] as AirMission[]).map(btn).join('')
    + `<button class="btn small${ui.airMode === 'base' ? ' on' : ''}" data-act="airbase" ${moved ? 'disabled' : ''} title="${moved ? 'It has already changed base this month.' : 'Move to airfields in another friendly region. Settling in costs readiness.'}">Change base</button>`;
  let body: string;
  if (ui.airMode === 'scout' || ui.airMode === 'search') body = '';
  else if (ui.airMode === 'base') {
    body = `<p class="sub" style="margin:8px 0 6px">Click any blue region on the map to move ${esc(f.name)} there. Settling in costs it ${Math.round(AIR.ready.move * 100)}% readiness, which a month of rest more than makes up. The airfields of one region serve about ${num(AIR.airfields.holds)} aircraft: pack in more and they cannot all fly.</p>`;
  } else if (ui.airMode) {
    const m = ui.airMode;
    const sea = m === 'strike';
    const ts = airTargets(g, f, m).map((t) => ({ t, d: sea ? seaKm(g, f.base, t) : airKm(g, f.base, t) })).sort((a, b) => a.d - b.d);
    const rows = ts.slice(0, 8).map(({ t, d }) => {
      const name = sea ? g.seas[t].name : g.regions[t].name;
      const c = cover(d, R.ftr);
      const esc2 = m === 'night' || !f.ftr ? '' : c >= 1 ? 'well inside fighter range' : c > 0 ? `fighters at ${Math.round(c * 100)}%` : 'beyond the fighters';
      const ind = !sea && m !== 'superiority' ? `industry ${g.regions[t].fac}` : '';
      const on = f.mission === m && f.target === t;
      return `<button class="pickrow${on ? ' pick' : ''}" data-act="airtarget" data-target="${t}"><span class="grow"><b>${esc(name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${[`${num(d)} km`, esc2, ind].filter(Boolean).join(' · ')}</span></span>${on ? '<span class="tag good">Ordered</span>' : ''}</button>`;
    }).join('');
    body = `<p class="sub" style="margin:8px 0 6px">${esc(missionWhat(m))} Click a ${sea ? 'ringed sea' : 'red region'} on the map, or pick one of the nearest here, to see the odds before you commit.</p>${rows}${ts.length > 8 ? `<p class="sub">${ts.length - 8} more in reach: see the map.</p>` : ''}`;
  } else {
    body = `<p class="sub" style="margin:8px 0 6px">${esc(capital(missionText(f)))}. ${esc(missionWhat(f.mission))} ${f.ftr ? `Its fighters reach ${num(R.ftr)} km and are at full effect to ${num(R.ftr * AIR.range.fullAt)} km. ` : ''}${f.bmr ? `Its bombers reach ${num(R.bmr)} km. ` : ''}The rings on the map show both.</p>`;
  }
  return `<div class="orders"><div class="acts">${btns}</div>${body}</div>`;
}

function airOrderText(o: AirOrder): string {
  const from = o.base?.length ? ` from ${o.base[0]}` : '';
  const sea = o.sea ? g.seas.find((z) => z.key === o.sea)?.name ?? 'sea' : '';
  const what = o.mission === 'superiority' ? `fights for the sky over ${o.region}` : o.mission === 'bomb' ? `bombs ${o.region} by day` : o.mission === 'night' ? `bombs ${o.region} by night` : o.mission === 'strike' ? `strikes at shipping in the ${sea}` : o.mission === 'rest' ? 'rests' : 'supports the armies';
  return `${o.fleet}${from} ${what}.`;
}

function airPane(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const size = (a: AirFleet, b: AirFleet): number => b.ftr + b.bmr - (a.ftr + a.bmr);
  const held = g.regions.filter((r) => skyOver(g, r.id));
  const skyNote = held.length ? `<div class="note"><b>The sky.</b> ${held.map((r) => `The ${g.nations[skyOver(g, r.id)!].adj} air force holds the sky over ${esc(r.name)}.`).join(' ')} An army attacking there has the air on its side, and on a coast the holder's aircraft command the sea. It has to be won again every month.</div>` : '';
  const plan = (nat: string, mine: boolean): string => {
    // A player sees the month he is about to play; a spectator the month that has just been flown.
    const ph = airPlanFor(g, nat, mine ? g.turn : Math.max(1, g.turn - 1));
    return ph ? `<div class="note"><b>${mine ? "History's plan this month" : `${esc(natName(nat))}, on history's plan`}: ${esc(ph.name)}.</b> ${esc(ph.orders.map(airOrderText).join(' '))}${mine ? ' You are free to do something else.' : ''}</div>` : '';
  };
  let out = '';
  if (me && airWar(g)) return airWarPane(me);
  if (me) {
    const n = g.nations[me];
    const mine = airFleetsOf(g, me);
    const p = airPool(g, me), d = airDeliveries(g, me);
    const order = humanUp(g);
    out += `<h2>The ${n.adj} air force</h2><p class="sub">An air fleet left alone supports the armies and defends the sky within reach of its fighters. Give it a mission and it flies a month-long air campaign, settled from a Battle Ticket when you end the month.</p>
      <div class="facts"><div class="fact"><span>Fighters</span><b>${num(mine.reduce((t, f) => t + f.ftr, 0))}</b></div><div class="fact"><span>Bombers</span><b>${num(mine.reduce((t, f) => t + f.bmr, 0))}</b></div><div class="fact"><span>Spare pilots</span><b>${num(p.pilots)}</b></div></div>
      <p class="sub" style="margin-top:6px">In reserve: ${craft(p.ftr, p.bmr)}. Each month the factories deliver ${craft(d.ftr, d.bmr)} and the flying schools ${num(d.pilots)} pilots. A new fighter needs a pilot, and resting fleets are made up first.${p.lostFtr + p.lostBmr ? ` Lost since the war began: ${craft(p.lostFtr, p.lostBmr)}, and ${num(p.lostPilots)} fighter pilots.` : ''}</p>${budgeted(g) ? '<div class="acts"><button class="btn small" data-act="armgo" data-arm="air" title="Buy fighter and bomber wings from the arsenal.">Order more aircraft</button></div>' : ''}`;
    if (!n.atWar.length) out += `<div class="note"><b>At peace.</b> The air force flies no missions until the nation is at war.</div>`;
    out += plan(me, true);
    const chain = AIR.radar.chains.find((c) => c.nat === me && c.from <= g.turn);
    if (chain) out += `<div class="note"><b>${esc(capital(chain.name))}.</b> Radar warns of raids on your home ground: every fighter in reach gets up in time, and few aircraft are caught on the ground. An enemy without it meets raids with only ${Math.round(AIR.radar.without * 100)}% of his fighters.</div>`;
    const cut = airIncomeLoss(g, me);
    if (cut > 0) out += `<div class="note" style="border-left-color:var(--red)"><b>Under the bombs.</b> Bombing and attacks on coastal shipping stopped ${(cut * 100).toFixed(1)}% of your production this month.</div>`;
    out += skyNote;
    out += `<h3>Your air fleets</h3>${mine.length ? mine.map((f) => airRow(f, order) + (order && ui.air === f.id ? airOrders(f) : '')).join('') : '<div class="empty">No air fleets under your flag.</div>'}`;
    const friends = g.airFleets.filter((f) => f.nat !== me && allied(g, f.nat, me)).sort(size);
    const foes = g.airFleets.filter((f) => atWar(g, f.nat, me)).sort(size);
    if (friends.length) out += `<h3>Allied air forces</h3>${friends.map((f) => airRow(f, false, true)).join('')}`;
    if (foes.length) out += `<h3>Enemy air forces</h3>${foes.map((f) => airRow(f, false, true)).join('')}`;
    return out;
  }
  out += `<h2>The war in the air</h2><p class="sub">Air fleets fly from the airfields of a region. Left alone they support the armies and defend their sky. Given a mission they fight a month-long campaign for the sky, against industry or against shipping, as far as their range allows. The lines on the map show who is flying against what.</p>`;
  const war = g.airFleets.filter((f) => g.nations[f.nat].atWar.length > 0);
  const nats = [...new Set(war.map((f) => f.nat))].sort((a, b) => war.filter((f) => f.nat === b).reduce((t, f) => t + f.ftr + f.bmr, 0) - war.filter((f) => f.nat === a).reduce((t, f) => t + f.ftr + f.bmr, 0));
  for (const nat of nats) out += plan(nat, false);
  out += skyNote;
  for (const nat of nats) {
    const p = airPool(g, nat);
    out += `<h3>${esc(natName(nat))}</h3>${p.lostFtr + p.lostBmr ? `<p class="sub" style="margin:-2px 0 6px">Lost so far: ${craft(p.lostFtr, p.lostBmr)}, and ${num(p.lostPilots)} fighter pilots.</p>` : ''}${war.filter((f) => f.nat === nat).map((f) => airRow(f, false)).join('')}`;
  }
  const peace = g.airFleets.filter((f) => !g.nations[f.nat].atWar.length).sort(size);
  if (peace.length) out += `<h3>Air forces at peace</h3>${peace.map((f) => airRow(f, false, true)).join('')}`;
  return out;
}

// ---- The air war as its own game (the grand campaign): airfields, air groups, the raid planner ----

const pctOf = (x: number): string => `${Math.round(x * 100)}%`;
const kindWord = (k: 'ftr' | 'bmr' | 'mixed', n: number): string => `${num(n)} ${k === 'ftr' ? 'fighter' : k === 'bmr' ? 'bomber' : 'aircraft'}${k === 'mixed' || Math.round(n) === 1 ? '' : 's'}`;

/** A fighter or a bomber seen from above, in its nation's colour. */
function groupIcon(kind: 'ftr' | 'bmr' | 'mixed', nat: string): string {
  return kind === 'bmr'
    ? `<svg width="36" height="26" viewBox="-14 -11 28 22" class="ctr"><path class="hull" d="${BOMBER}" fill="${g.nations[nat].color}"/></svg>`
    : `<svg width="34" height="24" viewBox="-14 -10.5 28 21" class="ctr"><path class="hull" d="${PLANE}" fill="${g.nations[nat].color}"/></svg>`;
}

/** Every airfield on the map: a patch of grass with its runways, larger for a larger field, rimmed in its holder's colours. A click opens it. */
function airfieldMarks(): string {
  let out = '';
  for (const a of airfieldsSeen(g)) {
    const k = Math.max(1, Math.min(4, a.size));
    const w = [0, 11, 14, 17, 19][k], h = [0, 8, 10, 12, 13.5][k];
    const on = ui.tab === 'air' && ui.field === a.region;
    // One strip for a landing ground, a second across it for an airfield, the triangle of three for an air base.
    const strips = k === 1 ? `M${(-w * 0.32).toFixed(1)} ${(h * 0.2).toFixed(1)}L${(w * 0.32).toFixed(1)} ${(-h * 0.2).toFixed(1)}`
      : k === 2 ? `M${(-w * 0.36).toFixed(1)} ${(h * 0.16).toFixed(1)}H${(w * 0.36).toFixed(1)}M${(-w * 0.16).toFixed(1)} ${(-h * 0.32).toFixed(1)}L${(w * 0.2).toFixed(1)} ${(h * 0.32).toFixed(1)}`
      : `M${(-w * 0.37).toFixed(1)} ${(h * 0.2).toFixed(1)}H${(w * 0.37).toFixed(1)}M0 ${(-h * 0.36).toFixed(1)}L${(-w * 0.27).toFixed(1)} ${(h * 0.33).toFixed(1)}M0 ${(-h * 0.36).toFixed(1)}L${(w * 0.27).toFixed(1)} ${(h * 0.33).toFixed(1)}`;
    out += `<g class="afm ${a.side}${on ? ' on' : ''}" data-id="${a.region}" transform="translate(${(a.x - 17).toFixed(1)} ${(a.y + 11).toFixed(1)})">${on ? `<rect class="afsel" x="${(-w / 2 - 2.6).toFixed(1)}" y="${(-h / 2 - 2.6).toFixed(1)}" width="${(w + 5.2).toFixed(1)}" height="${(h + 5.2).toFixed(1)}" rx="3.6"/>` : ''}<rect class="afpad" x="${(-w / 2).toFixed(1)}" y="${(-h / 2).toFixed(1)}" width="${w}" height="${h}" rx="2"/><path class="afrun" d="${strips}"/></g>`;
  }
  return out;
}

/** What the map shows for the air war's orders: a raid's targets, where a group could move or scout, and the reach of the group picked. */
function airWarMarks(): string {
  let out = '';
  const f = myAir();
  if (ui.raid && ui.raid !== 'menu' && humanUp(g)) {
    // A sea is ringed once, however many fleets in it could be struck.
    const ringed = new Set<number>();
    for (const t of raidTargetsFor(g, ui.raid, ui.lead)) {
      if (!t.sea) out += `<path class="mk hit" d="${pathOf(g.regions[t.id].polys)}"/>`;
      else if (!ringed.has(t.zone)) { ringed.add(t.zone); out += `<circle class="mk sea${ui.raid === 'fleet' ? ' strike' : ''}" cx="${t.x}" cy="${t.y + 9}" r="29"/>`; }
    }
  } else if (f && ui.airMode === 'search') {
    for (const s of searchOptionsFor(g, f.id)) out += `<circle class="mk sea find${s.searched ? ' at' : ''}" cx="${s.x}" cy="${s.y + 9}" r="29"/>`;
  } else if (f && ui.airMode === 'base') {
    for (const b of airGroupBases(g, f.id)) if (!b.why) out += `<path class="mk rail" d="${pathOf(g.regions[b.region].polys)}"/>`;
  } else if (f && ui.airMode === 'scout') {
    for (const t of raidTargetsFor(g, 'recon', f.id)) out += `<path class="mk scout" d="${pathOf(g.regions[t.id].polys)}"/>`;
  }
  if (f) {
    const at = groupAt(g, f), R = airRange(f), u = AIR.kmPerUnit;
    if (at.sea >= 0) {
      // A carrier wing at sea is anywhere in its sea: that sea is ringed, and the seas next door that it can reach.
      const z = g.seas[at.sea];
      out += `<circle class="mk ring ${f.bmr > 0 ? 'bmr' : 'ftr'}" cx="${z.x}" cy="${z.y + 9}" r="35"/>`;
      for (const j of z.adj) out += `<circle class="mk ring full" cx="${g.seas[j].x}" cy="${g.seas[j].y + 9}" r="31"/>`;
    } else {
      if (f.bmr > 0) out += `<circle class="mk ring bmr" cx="${at.x}" cy="${at.y}" r="${(R.bmr / u).toFixed(1)}"/>`;
      if (f.ftr > 0) out += `<circle class="mk ring ftr" cx="${at.x}" cy="${at.y}" r="${(R.ftr / u).toFixed(1)}"/><circle class="mk ring full" cx="${at.x}" cy="${at.y}" r="${((R.ftr * AIR.range.fullAt) / u).toFixed(1)}"/>`;
    }
  }
  return out;
}

/** One air group as a row: its kind, its name and command, where it is, what it is doing, and how fit it is. */
function groupRowHtml(x: GroupRow, canOrder: boolean, whose = false): string {
  const tags: string[] = [];
  if (x.flew) tags.push('<span class="tag">Flew its raid</span>');
  if (x.scouted) tags.push('<span class="tag">Scouted</span>');
  if (x.crowd < 1) tags.push(`<span class="tag bad" title="More aircraft are based here than the airfield serves. Move a group to another airfield, or have engineers enlarge this one.">Crowded: flies ${pctOf(x.crowd)}</span>`);
  if (x.est - x.aircraft > x.est * 0.2) tags.push(`<span class="tag bad">${num(x.est - x.aircraft)} aircraft short</span>`);
  const f = g.airFleets.find((y) => y.id === x.id);
  if (f && f.moved === g.turn) tags.push('<span class="tag">Moved this month</span>');
  const el = canOrder ? 'button' : 'div';
  const ldr = x.leader ? ` <span style="font-weight:400;color:var(--ink-2)">· ${esc(x.leader)} <span class="lvl" title="Level ${x.level} air commander">${x.level}</span></span>` : '';
  return `<${el} class="corps fleet grp${canOrder && ui.air === x.id ? ' pick' : ''}" ${canOrder ? `data-act="airfleet" data-id="${x.id}"` : ''}>
    ${groupIcon(x.kind, x.nat)}<span class="nm">${esc(x.name)}${f?.nick ? ` <i class="nick">${esc(f.nick)}</i>` : ''}${ldr}</span><span></span>
    <span class="meta">${whose ? `${g.nations[x.nat].adj} · ` : ''}${esc(capital(kindWord(x.kind, x.aircraft)))}${x.command ? ` · ${esc(x.command)}` : ''}${x.ship >= 0 && whose ? ` · aboard the ${esc(x.baseName)}` : ''} · reach ${num(x.range)} km</span>
    <span class="meta">${esc(x.doing)}</span>
    <span class="bars"><span>Readiness</span><span class="meter"><i class="${x.ready < 0.5 ? 'low' : ''}" style="width:${Math.round(x.ready * 100)}%"></i></span></span>
    ${tags.length ? `<span class="tags">${tags.join('')}</span>` : ''}</${el}>${f && (ui.air === x.id || Object.keys(f.rec ?? {}).length > 0 || (f.hon?.length ?? 0) > 0) ? recStrip('air', f) : ''}`;
}

/** The orders for the air group that is picked: what it does between raids, each with what it would mean, and the raids, scouting and moves it can make. */
function groupOrders(x: GroupRow): string {
  const jobs = airGroupJobs(g, x.id).map((j) => `<button class="jobrow${j.on ? ' on' : ''}" data-act="groupjob" data-key="${esc(j.key)}" ${j.why || j.on ? 'disabled' : ''}><b>${esc(j.label)}${j.on ? ' <span class="tag good">Its order now</span>' : ''}</b><span>${esc(j.why ?? j.forecast)}</span></button>`).join('');
  const f = g.airFleets.find((y) => y.id === x.id)!;
  const moved = f.moved === g.turn;
  const peace = !g.nations[x.nat].atWar.length;
  const deck = x.ship >= 0;
  const acts = `<div class="acts"><button class="btn small go${ui.raid && ui.lead === x.id ? ' on' : ''}" data-act="raidmenu" data-id="${x.id}" ${x.flew || peace ? 'disabled' : ''} title="${x.flew ? 'It has flown its raid this month.' : peace ? 'The nation is at peace.' : 'Pick the kind of raid and its target. You see the chance of success and the losses to expect, then choose who else flies it.'}">${deck ? 'Plan a strike with it' : 'Plan a raid with it'}</button>
    <button class="btn small${ui.airMode === 'scout' ? ' on' : ''}" data-act="scoutmode" ${x.scouted || peace ? 'disabled' : ''} title="${x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft photograph one province: every army there and beside it is identified, corps by corps, with its strength, its rest and its morale. The report is exact this month and goes stale after. It keeps its patrol.'}">Scout from the air</button>
    ${carrierRules(g) ? `<button class="btn small${ui.airMode === 'search' ? ' on' : ''}" data-act="searchmode" ${x.scouted || peace ? 'disabled' : ''} title="${x.scouted ? 'It has flown its reconnaissance this month.' : 'A few aircraft quarter one sea for the month. Enemy ships there may be reported, or identified: a fleet has to be found before it can be struck. It keeps its patrol.'}">Search a sea</button>` : ''}
    ${deck ? `<button class="btn small" data-act="fleetgo" data-id="${x.ship}" title="Its aircraft go where its fleet goes: pick the fleet up to move them.">Go to its fleet</button>` : `<button class="btn small${ui.airMode === 'base' ? ' on' : ''}" data-act="airbase" ${moved ? 'disabled' : ''} title="${moved ? 'It has already changed base this month.' : 'Move to another airfield. Settling in costs readiness.'}">Move to another airfield</button>`}</div>`;
  let body = '';
  if (ui.airMode === 'search') {
    const ss = searchOptionsFor(g, x.id).slice(0, 8);
    body = `<p class="sub" style="margin:8px 0 6px">Click a ringed sea on the map, or pick one here. A search adds to what your ships and coastal aircraft already see there. It is this group's one reconnaissance flight of the month.</p>${ss.map((o) => `<button class="pickrow" data-act="searchpick" data-zone="${o.zone}"><span class="grow"><b>${esc(o.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${o.km > 0 ? `${num(o.km)} km · ` : ''}${esc(o.word)}${o.reported ? ` · ${o.reported} force${o.reported === 1 ? '' : 's'} reported, not identified` : ''}</span></span>${o.searched ? '<span class="tag">Searched this month</span>' : ''}</button>`).join('') || '<div class="empty">No sea is in its reach.</div>'}`;
  } else if (ui.airMode === 'base') {
    const bases = airGroupBases(g, x.id).filter((b) => !b.why).slice(0, 8);
    body = `<p class="sub" style="margin:8px 0 6px">Click a blue province on the map, or pick one of the nearest airfields. Settling in costs ${pctOf(AIR.ready.move)} readiness. Where there is no airfield, an engineer corps can lay one out in a month.</p>${bases.map((b) => `<button class="pickrow" data-act="basepick" data-id="${b.region}"><span class="grow"><b>${esc(b.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${esc(b.word)} · ${num(b.km)} km · room for ${num(b.free)} of ${num(b.holds)}${b.free < x.aircraft ? ' · it would be crowded' : ''}</span></span></button>`).join('') || '<div class="empty">No other airfield is in reach.</div>'}`;
  } else if (ui.airMode === 'scout') {
    const ts = raidTargetsFor(g, 'recon', x.id).slice(0, 8);
    body = `<p class="sub" style="margin:8px 0 6px">Click a marked province on the map, or pick one of the nearest. ${AIRWAR_RECON} aircraft go; fighters on patrol there may bring some down.</p>${ts.map((t) => `<button class="pickrow" data-act="scoutpick" data-id="${t.id}"><span class="grow"><b>${esc(t.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${num(t.km)} km · ${esc(t.what)}</span></span></button>`).join('') || '<div class="empty">Nothing in its reach is hidden from you.</div>'}`;
  }
  return `<div class="orders"><p class="sub" style="margin:0 0 5px">Between raids:</p><div class="jobs">${jobs}</div>${acts}${body}</div>`;
}

const AIRWAR_RECON = 12;

/** The raid planner's first steps, in the pane: the kind of raid, then its target. The map marks the targets too. */
function raidPicker(): string {
  const menu = raidMenu(g).filter((k) => k.key !== 'recon');
  const lead = ui.lead >= 0 ? myAirGroups(g).find((x) => x.id === ui.lead) : undefined;
  const kinds = menu.map((k) => {
    const n = lead ? raidTargetsFor(g, k.key, lead.id).length : k.targets;
    return `<button class="btn small${ui.raid === k.key ? ' on' : ''}" data-act="raidkind" data-kind="${k.key}" ${n ? '' : 'disabled'} title="${esc(n ? k.blurb : k.why ?? 'Nothing in reach.')}">${esc(k.label)} <span class="cnt">${n}</span></button>`;
  }).join('');
  let body = `<p class="sub" style="margin:8px 0 6px">Pick the kind of raid${lead ? ` for the ${esc(lead.name)}` : ''}. The number is how many targets are in reach this month.</p>`;
  if (ui.raid && ui.raid !== 'menu') {
    const k = menu.find((x) => x.key === ui.raid)!;
    const ts = raidTargetsFor(g, ui.raid, ui.lead);
    const rows = ts.slice(0, 10).map((t) => `<button class="pickrow" data-act="raidtarget" data-target="${t.id}"><span class="grow"><b>${esc(upFirst(t.name))}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${[`${num(t.km)} km`, esc(t.what), `${t.groups} group${t.groups === 1 ? '' : 's'} can reach it`].filter(Boolean).join(' · ')}</span></span></button>`).join('');
    const none = !ts.length ? `<div class="empty">${ui.raid === 'fleet' ? 'No enemy fleet at sea has been found in reach. Search a sea first: a fleet has to be found before it can be struck.' : ui.raid === 'harbour' ? 'No enemy warships lie in a harbour in reach.' : ui.raid === 'town' ? 'No enemy town that your armies stand round or beside is in reach of a bomber group that can still fly.' : 'Nothing is in reach.'}</div>` : '';
    body = `<p class="sub" style="margin:8px 0 6px">${esc(k.blurb)} Click a ${ui.raid === 'strike' ? 'ringed sea' : ui.raid === 'fleet' ? 'ringed fleet or sea' : 'red province'} on the map, or pick one of the nearest here: you see the chance of success and the losses before anything flies.</p>${rows}${none}${ts.length > 10 ? `<p class="sub">${ts.length - 10} more in reach: see the map.</p>` : ''}`;
  }
  return `<div class="orders planner"><div class="acts">${kinds}<button class="btn small" data-act="raidmenu" data-id="-1">Close</button></div>${body}</div>`;
}

/** The Air tab in the grand campaign: the air force by airfield, and the way into the raid planner. */
function airWarPane(me: string): string {
  const n = g.nations[me];
  const order = humanUp(g);
  const groups = myAirGroups(g);
  const fields = airfieldsSeen(g);
  const byRegion = new Map(fields.map((a) => [a.region, a]));
  const p = airPool(g, me), d = airDeliveries(g, me);
  const peace = !n.atWar.length;
  let out = `<h2>The ${n.adj} air force</h2><p class="sub">Aircraft fly from airfields, the pads marked on the map. An air group is fighters or bombers. Between raids fighters patrol their sky, stick with a bomber group as its escort, or rest. Any group flies one raid a month: you plan it from its target, and it is flown at once.</p>
    <div class="facts"><div class="fact"><span>Fighters</span><b>${num(groups.reduce((t, x) => t + x.ftr, 0))}</b></div><div class="fact"><span>Bombers</span><b>${num(groups.reduce((t, x) => t + x.bmr, 0))}</b></div><div class="fact"><span>Airfields</span><b>${fields.filter((a) => a.side === 'own').length}</b></div></div>
    <p class="sub" style="margin-top:6px">In reserve: ${craft(p.ftr, p.bmr)}, and ${num(p.pilots)} spare pilots. Each month the factories deliver ${craft(d.ftr, d.bmr)} and the flying schools ${num(d.pilots)} pilots. Resting groups are made up first.${p.lostFtr + p.lostBmr ? ` Lost since the war began: ${craft(p.lostFtr, p.lostBmr)}, and ${num(p.lostPilots)} fighter pilots.` : ''}</p>
    <div class="acts"><button class="btn small go${ui.raid && ui.lead < 0 ? ' on' : ''}" data-act="raidmenu" data-id="-1" ${order && !peace ? '' : 'disabled'} title="${peace ? 'The nation is at peace.' : 'Pick the kind of raid, then the target: you see the chance of success and the losses to expect, then choose which groups fly it together.'}">Plan a raid</button>${budgeted(g) ? '<button class="btn small" data-act="armgo" data-arm="air" title="Buy fighter and bomber wings from the arsenal.">Order more aircraft</button>' : ''}</div>`;
  if (ui.raid && order) out += raidPicker();
  if (peace) out += `<div class="note"><b>At peace.</b> The air force flies no raids until the nation is at war.</div>`;
  const chain = AIR.radar.chains.find((c) => c.nat === me && c.from <= g.turn);
  if (chain) out += `<div class="note"><b>${esc(capital(chain.name))}.</b> Radar warns of raids on your home ground: every fighter on patrol in reach gets up in time. An enemy without it meets raids with only ${Math.round(AIR.radar.without * 100)}% of his fighters.</div>`;
  const cut = airIncomeLoss(g, me);
  if (cut > 0) out += `<div class="note" style="border-left-color:var(--red)"><b>Under the bombs.</b> Bombing and attacks on coastal shipping stopped ${(cut * 100).toFixed(1)}% of your production this month.</div>`;
  const held = g.regions.filter((r) => skyOver(g, r.id));
  if (held.length) out += `<div class="note"><b>The sky.</b> ${held.map((r) => `The ${g.nations[skyOver(g, r.id)!].adj} air force holds the sky over ${esc(r.name)}.`).join(' ')} An army attacking there has the air on its side. It has to be won again every month.</div>`;

  // An airfield opened that is not one of the player's own: what is known of it.
  const open = ui.field >= 0 ? airfieldAt(g, ui.field) : null;
  if (open && (!open.field || open.field.side !== 'own' || !groups.some((x) => x.base === ui.field))) {
    const own = open.field?.side === 'own';
    out += `<h3>${esc(open.name)}${open.field ? ` · ${natName(open.field.holder)}` : ''}</h3><div class="afield pick"><p class="sub" style="margin:0 0 6px">${esc(open.note)}${!open.field || own ? ` ${esc(open.build)}` : ''}${own ? ' To base a group here, pick the group and choose Move to another airfield.' : ''}</p>${open.rows.map((x) => groupRowHtml(x, false, true)).join('')}</div>`;
  }

  const flown = g.airReports.filter((r) => r.turn === g.turn && r.attNat === me);
  if (flown.length) out += `<h3>Raids flown this month</h3>${flown.map((r) => airBrow(r)).join('')}`;

  // Carrier wings fly from their fleets: each fleet with carriers is a floating airfield of its own.
  const decks = [...new Set(groups.filter((x) => x.ship >= 0).map((x) => x.ship))];
  if (decks.length) {
    out += '<h3>Your carriers</h3><p class="sub" style="margin:-2px 0 6px">A carrier is a floating airfield. Its fighters keep enemy bombers off the fleet; its strike aircraft go for ships at sea, ships in harbour and targets ashore, wherever the fleet carries them. A fleet has to be found before it can be struck: ships and aircraft search the seas they are in, and a wing can fly a search of its own.</p>';
    for (const id of decks) {
      const fl = g.fleets.find((q) => q.id === id);
      if (!fl) continue;
      const here = groups.filter((x) => x.ship === id);
      out += `<div class="afield deck${here.some((x) => x.id === ui.air) ? ' pick' : ''}"><button class="afhead" data-act="fleetgo" data-id="${id}" title="Pick the fleet up: its aircraft go where it goes."><b>${esc(fl.name)}</b><span>${fl.ships.cv} carrier${fl.ships.cv === 1 ? '' : 's'} · ${atSea(fl) ? `at sea in the ${esc(g.seas[fl.zone].name)}` : `in harbour at ${esc(fl.port)}`} · ${num(here.reduce((t, x) => t + x.aircraft, 0))} of ${num(here.reduce((t, x) => t + x.est, 0))} aircraft</span></button>
        ${here.map((x) => groupRowHtml(x, order) + (order && ui.air === x.id ? groupOrders(x) : '')).join('')}</div>`;
    }
  }

  out += '<h3>Your airfields</h3>';
  const bases = [...new Set(groups.filter((x) => x.ship < 0).map((x) => x.base))];
  if (!bases.length) out += '<div class="empty">No air group flies under your flag.</div>';
  for (const b of bases) {
    const a = byRegion.get(b);
    const here = groups.filter((x) => x.base === b && x.ship < 0);
    const used = here.reduce((t, x) => t + x.aircraft, 0);
    out += `<div class="afield${ui.field === b ? ' pick' : ''}"><button class="afhead" data-act="airfield" data-id="${b}"><b>${esc(g.regions[b].name)}</b><span>${a ? `${esc(a.word)} · ${num(used)} of ${num(a.holds)} aircraft` : 'No airfield'}</span>${a?.crowded ? '<span class="tag bad">Crowded</span>' : ''}</button>
      ${here.map((x) => groupRowHtml(x, order) + (order && ui.air === x.id ? groupOrders(x) : '')).join('')}</div>`;
  }
  const empty = fields.filter((a) => a.side === 'own' && !bases.includes(a.region));
  if (empty.length) out += `<p class="sub" style="margin-top:8px">Airfields with nobody based there: ${empty.map((a) => `<button class="lnk" data-act="airfield" data-id="${a.region}">${esc(a.name)}</button> (${esc(a.word.toLowerCase())}, ${num(a.holds)})`).join(', ')}. Engineers lay out new airfields and enlarge small ones; the treasury builds larger.</p>`;

  // An enemy carrier's aircraft are known when its fleet is identified; a group on land when its airfield is in view.
  const found = new Set(carrierRules(g) ? fleetsInSight(g).filter((s) => s.known === 'exact').map((s) => s.id) : []);
  const known = (f: AirFleet): boolean => (f.ship !== undefined ? found.has(f.ship) : (byRegion.get(f.base)?.groups ?? -1) >= 0);
  const size = (a: AirFleet, b: AirFleet): number => b.ftr + b.bmr - (a.ftr + a.bmr);
  const friends = g.airFleets.filter((f) => f.nat !== me && allied(g, f.nat, me) && f.ftr + f.bmr > 0).sort(size);
  const foes = g.airFleets.filter((f) => atWar(g, f.nat, me) && f.ftr + f.bmr > 0);
  const seen = foes.filter(known).sort(size);
  if (friends.length) out += `<h3>Allied air forces</h3>${friends.map((f) => groupRowHtml(myGroupRow(f), false, true)).join('')}`;
  if (foes.length) out += `<h3>Enemy air forces</h3>${seen.map((f) => groupRowHtml(myGroupRow(f), false, true)).join('')}${seen.length < foes.length ? `<p class="sub">${seen.length ? 'More fly' : 'They fly'} from airfields${carrierRules(g) ? ' and carriers' : ''} the fog of war hides. Scout an airfield from the air, or put an agent on watch there, to see what is based on it.</p>` : ''}`;
  return out;
}

/** Any air group as a row for a list, whoever it belongs to. */
const myGroupRow = (f: AirFleet): GroupRow => (f.ship !== undefined ? groupRow(g, f) : airfieldAt(g, f.base)?.rows.find((x) => x.id === f.id) ?? {
  id: f.id, nat: f.nat, name: f.name, command: '', kind: f.bmr <= 0 ? 'ftr' : f.ftr <= 0 ? 'bmr' : 'mixed', ftr: f.ftr, bmr: f.bmr, aircraft: f.ftr + f.bmr, est: f.est.ftr + f.est.bmr, ready: f.ready,
  leader: f.leader?.name ?? null, level: f.leader?.level ?? 0, base: f.base, baseName: g.regions[f.base].name, ship: f.ship ?? -1, sea: -1, x: g.regions[f.base].x, y: g.regions[f.base].y, doing: `Based at ${g.regions[f.base].name}`, job: 'patrol', escorts: -1, flew: false, scouted: false, range: 0, crowd: 1, mine: false,
});

/** The raid planner's card: the target, the chance of success and the losses to expect, and who flies it. Ticking groups works the odds out again. */
function raidCard(): string {
  const m = ui.modal!;
  const kind = m.raid as RaidKind;
  const p: RaidPlan = previewRaid(g, kind, m.target, m.ids);
  m.ids = p.picked;
  const me = g.human!;
  const going = p.seats.filter((s) => s.picked), could = p.seats.filter((s) => !s.picked && !s.why), cannot = p.seats.filter((s) => s.why);
  const seat = (s: RaidPlan['seats'][number]): string => `<button class="pickrow seat${s.picked ? ' pick' : ''}" data-act="raidtick" data-id="${s.id}" ${s.locked ? 'disabled title="It goes because it is ordered to stick with its bombers. Change its standing order to take it off escort."' : ''}>
    <span class="tick">${s.picked ? '✓' : ''}</span>${groupIcon(s.kind, me)}<span class="grow"><b>${esc(s.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${esc(kindWord(s.kind, s.aircraft))} · ${esc(s.baseName)} · ${esc(s.note)}${s.ready < 0.8 ? ` · readiness ${pctOf(s.ready)}` : ''}${s.locked ? ' · escort, sticks with its bombers' : ''}</span></span>
    <span class="tag${s.picked ? ' good' : ''}">${s.picked ? 'Going' : s.kind === 'ftr' ? 'Add as escort' : 'Add'}</span></button>`;
  let head: string;
  if (p.why && !going.length) head = `<div class="empty">${esc(p.why)}</div>`;
  else if (p.why) head = `<div class="verdict loss" style="font-size:22px">${esc(p.why)}</div>`;
  else {
    const cls = p.success >= 0.6 ? 'win' : p.success >= 0.4 ? '' : 'loss';
    const loss = [p.bombers ? `<div class="fact"><span>Bombers likely lost</span><b>${num(p.bomberLossN)} of ${num(p.bombers)}</b><small>${pctOf(p.bomberLoss)}</small></div>` : '', p.fighters ? `<div class="fact"><span>Fighters likely lost</span><b>${num(p.fighterLossN)} of ${num(p.fighters)}</b></div>` : '', `<div class="fact"><span>Enemy fighters likely lost</span><b>${num(p.foeLossN)}</b></div>`].join('');
    head = `<div class="verdict ${cls}" style="font-size:24px">${esc(p.verdict)}</div>
      <div class="oddsbar" title="The chance the raid costs the enemy more than it costs you"><i style="width:${Math.round(p.success * 100)}%;background:${g.nations[me].color}"></i><i style="flex:1;background:var(--panel-3)"></i></div>
      <div class="facts raidfacts"><div class="fact"><span>Chance of success</span><b>${pctOf(p.success)}</b></div>${loss}</div>
      <div class="sub">${p.effects.map(esc).join(' ')}</div>
      ${p.massed ? `<div class="note good"><b>Meeting up.</b> ${esc(p.massed.replace(/^Meeting up pays: /, ''))}</div>` : ''}
      ${p.offPatrol ? `<div class="note" style="border-left-color:var(--red)"><b>Off patrol.</b> ${esc(upFirst(p.offPatrol))}</div>` : ''}`;
  }
  const meets = p.meets.length ? p.meets.map((x) => `<div class="pickrow">${groupIcon('ftr', x.nat)}<span class="grow"><b>${esc(x.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${g.nations[x.nat].adj}, ${x.afloat ? 'from the carriers of the' : 'from'} ${esc(x.baseName)} · ${num(x.ftr)} fighters, about ${num(x.able)} able to meet the raid</span></span></div>`).join('') : '<div class="empty">No enemy fighters are in reach.</div>';
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Raid planner · ${date(g.turn)}</div><h2>${esc(p.title)}</h2></header>
    <div class="body">${head}
      <h3>Flying the raid</h3>${going.map(seat).join('') || '<div class="empty">Nobody yet: tick a group below.</div>'}
      ${could.length ? `<h3>Could join it</h3><p class="sub" style="margin:-2px 0 6px">Bombers from other airfields meet up on the way: together they lose a smaller share. Fighters in range go as escort.</p>${could.map(seat).join('')}` : ''}
      ${p.why ? '' : `<h3>Rising to meet them</h3>${meets}<h3>Battle Ticket</h3>${ticketTable(p.lines)}`}
      ${cannot.length ? `<details class="cannot"><summary>${cannot.length} group${cannot.length === 1 ? '' : 's'} cannot go</summary>${cannot.map((s) => `<div class="pickrow">${groupIcon(s.kind, me)}<span class="grow"><b>${esc(s.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${esc(s.baseName)} · ${esc(s.why ?? '')}</span></span></div>`).join('')}</details>` : ''}
      ${kind === 'fleet' || kind === 'harbour' ? `<div class="note"><b>Ships are sunk whole.</b> The ticket counts hits; a ship goes down when it has taken enough of them, so a strike that is likely to succeed can still come home with nothing sunk. ${kind === 'fleet' ? 'A fleet that is only reported is harder to find: part of the strike never sees it.' : 'Ships in harbour cannot manoeuvre, and a harbour raided in the month war is declared is caught unready.'}</div>` : ''}
      <div class="note"><b>Flown at once.</b> Every group that goes is away for the rest of the month: it flies no second raid and meets no raid on its own sky. It is back at its airfield${going.some((s) => s.ship >= 0) ? ' or aboard its carrier' : ''}, on patrol as before, next month.</div>
    </div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="doraid" ${p.why ? 'disabled' : ''}>Fly the raid</button></footer>`;
}

const ticketTable = (lines: { label: string; value: string; effect?: string }[]): string => `<table class="ticket">${lines.map((l) => `<tr><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>`;

/** The look at an air mission before the order is given: the odds, the ticket, who flies and who will meet them. */
function airPlanCard(): string {
  const m = ui.modal!;
  const f = g.airFleets.find((x) => x.id === m.fleet)!;
  const mission = m.mission as AirMission;
  const sea = mission === 'strike';
  const place = sea ? `the ${g.seas[m.target].name}` : g.regions[m.target].name;
  const title = mission === 'superiority' ? `The sky over ${place}` : mission === 'bomb' ? `Bombing ${place}` : mission === 'night' ? `Night raids on ${place}` : `Strikes on shipping in ${place}`;
  const a = previewAirOrder(g, f.id, mission, m.target);
  const why = whyNotMission(g, f, mission, m.target) ?? a?.why ?? (a ? null : 'Nothing can reach the target.');
  let body: string;
  if (why || !a) body = `<div class="empty">${esc(why ?? '')}</div>`;
  else {
    const fc = forecastAir(g, a, mission, m.target);
    const lossPct = Math.round(fc.bomberLoss * 100);
    const defNat = a.def[0]?.f.nat ?? (sea ? '' : g.regions[m.target].ctrl);
    let head: string;
    if (mission === 'superiority') {
      const word = a.D <= 0 ? 'No fighters will come up' : fc.odds >= AIR.sky.win * 1.25 ? 'The sky is within reach' : fc.odds >= AIR.sky.win ? 'The sky can be won, at a price' : fc.odds >= 1.2 ? 'A hard fight that wears them down' : fc.odds >= 0.8 ? 'An even fight' : 'The defenders have the edge';
      const share = a.Epow + a.Dpow > 0 ? a.Epow / (a.Epow + a.Dpow) : 1;
      head = `<div class="verdict ${fc.odds >= AIR.sky.win ? 'win' : fc.odds >= 1.2 ? '' : 'loss'}" style="font-size:24px">${word}</div>
        <div class="oddsbar"><i style="width:${Math.round(share * 100)}%;background:${g.nations[f.nat].color}"></i><i style="flex:1;background:${defNat ? g.nations[defNat].color : 'var(--panel-3)'}"></i></div>
        <div class="sub">Fighter odds ${fc.odds.toFixed(2)} to 1 going in. The sky is won when your fighters end the month with ${AIR.sky.win} times what the defender can still put up once his losses are made good.${a.B > 0 ? ` About ${lossPct}% of the bombers are likely to be lost.` : ''}${fc.cut > 0 ? ` Those that get through should cut the readiness of the fighters based there by about ${Math.round(fc.cut * 100)}%, and that carries over to next month.` : ''}</div>`;
    } else if (sea) {
      head = `<div class="verdict ${lossPct <= 10 ? 'win' : lossPct <= 25 ? '' : 'loss'}" style="font-size:24px">${lossPct <= 10 ? 'Light opposition' : lossPct <= 25 ? 'The fighters will take a toll' : 'Heavy losses are likely'}</div>
        <div class="sub">About ${lossPct}% of the bombers are likely to be lost. Warships caught at sea there could lose up to ${Math.round(fc.cut * 100)}% of their number, and merchant ships in these waters will be sunk.</div>`;
    } else {
      head = `<div class="verdict ${lossPct <= 10 ? 'win' : lossPct <= 25 ? '' : 'loss'}" style="font-size:24px">${lossPct <= 10 ? 'Light losses expected' : lossPct <= 25 ? 'The fighters will take a toll' : 'Heavy losses are likely'}</div>
        <div class="sub">About ${lossPct}% of the bombers are likely to be lost. ${g.regions[m.target].fac > 0 ? `The raids should stop about ${Math.round(fc.cut * 100)}% of ${esc(place)}'s output for a month: ${(fc.nation * 100).toFixed(1)}% of ${g.nations[g.regions[m.target].ctrl].adj} production.` : `There is no industry at ${esc(place)}: the bombers go for its rail yards and junctions.`}${fc.rail ? ` There is about a ${Math.round(fc.rail * 100)}% chance of cutting the railway through ${esc(place)}, which stays cut until engineers relay it.` : ''}${fc.ruin ? ` There is about a ${Math.round(fc.ruin * 100)}% chance of wrecking a factory outright.` : ''}</div>`;
    }
    const flying = a.att.map((x) => `<div class="pickrow pick">${planeIcon(x.f)}<span class="grow"><b>${esc(x.f.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">from ${esc(g.regions[x.f.base].name)}, ${num(x.d)} km · ${esc(craft(mission === 'night' ? 0 : x.f.ftr, x.bmr > 0 ? x.f.bmr : 0))}${x.crowd < 1 ? ` · crowded airfields, ${Math.round(x.crowd * 100)}% can fly` : ''}</span></span></div>`).join('');
    const meeting = a.def.length ? a.def.map((x) => `<div class="pickrow">${planeIcon(x.f)}<span class="grow"><b>${esc(x.f.name)}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${g.nations[x.f.nat].adj}, from ${esc(g.regions[x.f.base].name)} · ${num(x.f.ftr)} fighters, about ${num(x.f.ftr * x.w)} able to meet the raids</span></span></div>`).join('') : '<div class="empty">No enemy fighters are in reach.</div>';
    body = `${head}<h3>Battle Ticket</h3>${ticketTable(a.lines)}<h3>Flying the mission</h3>${flying}<h3>Rising to meet them</h3>${meeting}
      <div class="note"><b>Flown when you end the month.</b> Other fleets given the same mission against the same target fly it together. A fleet on a mission does not defend its own sky that month.</div>`;
  }
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Air mission · ${esc(f.name)} · ${date(g.turn)}</div><h2>${esc(title)}</h2></header>
    <div class="body">${body}</div>
    <footer><button class="btn" data-act="close">Call it off</button><button class="btn go" data-act="doair" ${why || !a ? 'disabled' : ''}>Give the order</button></footer>`;
}

function airReportCard(r: AirReport): string {
  const v = airVerdict(r);
  const sum = (ss: AirSide[], k: 'ftr' | 'bmr' | 'lostFtr' | 'lostBmr'): number => ss.reduce((n, x) => n + x[k], 0);
  const side = (ss: AirSide[]): string => ss.length ? `<div class="big">${num(sum(ss, 'ftr') + sum(ss, 'bmr'))}</div><small>aircraft in ${ss.length} air ${airWar(g) ? 'group' : 'fleet'}${ss.length > 1 ? 's' : ''} · lost ${num(sum(ss, 'lostFtr') + sum(ss, 'lostBmr'))}</small>` : '<div class="big">0</div><small>no fighters came up</small>';
  const list = (ss: AirSide[]): string => ss.length ? `<ul class="ulist">${ss.map((x) => `<li><span>${esc(x.fleet)} <span style="color:var(--ink-3)">(${g.nations[x.nat].adj}${x.leader ? `, ${esc(x.leader)}` : ''})</span><br><span style="color:var(--ink-2)">${esc(capital(craft(x.ftr, x.bmr)))}</span></span><span>${x.lostFtr + x.lostBmr ? `lost ${esc(craft(x.lostFtr, x.lostBmr))}` : 'no losses'}</span></li>`).join('')}</ul>` : '<div class="empty">None.</div>';
  const sky = r.mission === 'superiority' ? (r.sky ? ` The ${g.nations[r.attNat].adj} air force won the sky over ${esc(r.place)}: its fighters ended the month with ${r.hold >= 9 ? 'no opposition left' : `${r.hold.toFixed(2)} times what the defenders could still put up`}.` : ` The attackers ended the month with ${r.hold.toFixed(2)} times the fighters the defenders could still put up. It takes ${AIR.sky.win} to win the sky.`) : '';
  const what = r.mission === 'superiority' ? 'air superiority' : r.mission === 'bomb' ? 'day bombing' : r.mission === 'night' ? 'night bombing' : r.mission === 'ground' ? 'a raid on troops' : r.mission === 'line' ? 'bombing the line' : r.mission === 'fleet' ? 'a strike on a fleet' : r.mission === 'harbour' ? 'a raid on a harbour' : r.mission === 'town' ? 'a raid on a town' : 'strikes on shipping';
  const aw = airWar(g);
  // The ships under the bombs, fleet by fleet, as they were before the strike and with what each lost.
  const ships = r.ships?.length
    ? `<h3>The ships under attack</h3><ul class="ulist">${r.ships.map((x) => `<li><span>${esc(x.fleet)} <span style="color:var(--ink-3)">(${g.nations[x.nat].adj}${x.admiral ? `, ${esc(x.admiral)}` : ''})</span><br><span style="color:var(--ink-2)">${esc(capital(shipList(x.ships)))}</span></span><span>${shipCount(x.sunk) ? `lost ${esc(shipList(x.sunk))}` : 'no ships lost'}</span></li>`).join('')}</ul>${r.surprise ? '<div class="note" style="border-left-color:var(--red)"><b>Caught unready.</b> War was declared this month: the harbour had no warning, its fighters and guns were not manned, and the ships lay at their moorings.</div>' : ''}`
    : '';
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Battle Ticket · ${aw ? 'air raid' : 'air campaign'}, ${what} · ${date(r.turn)}</div><h2>${esc(airTitle(r))}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">${esc(r.result)}${sky}</div>
      <div class="vs"><div><h4>${chip(r.attNat)} ${natName(r.attNat)} attacking</h4>${side(r.att)}</div><div class="mid">vs</div><div class="r"><h4>${r.defNat ? natName(r.defNat) : 'No defenders'} ${r.defNat ? chip(r.defNat) : ''}</h4>${side(r.def)}</div></div>
      <h3>What the campaign map fed into the battle</h3>
      ${ticketTable(r.lines)}
      ${ships}<h3>Flying the mission</h3>${list(r.att)}<h3>Rising to meet them</h3>${list(r.def)}
      <div class="note" style="margin-top:14px">${aw ? '<b>Flown and settled.</b> The raid is settled from its ticket the moment it is ordered. The groups that flew it are back on their airfields next month.' : '<b>Auto-resolved.</b> A month of air fighting is settled from its ticket. In the full game its big days can be flown as 3D air battles, with the same ticket going in and the same kind of result coming back.'}</div>
    </div><footer><button class="btn" data-act="${r.zone >= 0 ? 'showsea' : 'showmap'}" data-id="${r.zone >= 0 ? r.zone : r.region}">Show on map</button><button class="btn go" data-act="close">Close</button></footer>`;
}

// ---- Modals -------------------------------------------------------------------------------

function openModal(m: typeof ui.modal): void {
  ui.modal = m;
  const veil = $('#veil');
  if (!m) {
    veil.classList.remove('open');
    // A card that was waiting for this one to be put down: the war just decided, or the world just won.
    const next = nextCard();
    if (next) openModal(next);
    return;
  }
  veil.classList.add('open');
  const card = $('#card');
  card.classList.toggle('wide', m.kind === 'attack' || m.kind === 'siege' || m.kind === 'report' || m.kind === 'ledger' || m.kind === 'town' || m.kind === 'trade' || m.kind === 'cabinet' || m.kind === 'tech' || m.kind === 'stores' || m.kind === 'books' || m.kind === 'raid');
  card.classList.toggle('techcard', m.kind === 'tech');
  if (m.kind === 'start') card.innerHTML = startCard();
  else if (m.kind === 'report') {
    const rep = g.reports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = reportCard(rep);
  }
  else if (m.kind === 'seareport') {
    const rep = g.seaReports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = seaReportCard(rep);
  }
  else if (m.kind === 'airreport') {
    const rep = g.airReports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = airReportCard(rep);
  }
  else if (m.kind === 'shell') card.innerHTML = shellCard();
  else if (m.kind === 'split') card.innerHTML = splitCard();
  else if (m.kind === 'shorereport') {
    const rep = g.shoreReports.find((r) => r.id === m.id);
    if (!rep) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = shoreReportCard(rep);
  }
  else if (m.kind === 'airplan') card.innerHTML = airPlanCard();
  else if (m.kind === 'raid') card.innerHTML = raidCard();
  else if (m.kind === 'landing') card.innerHTML = landingCard();
  else if (m.kind === 'attack') card.innerHTML = attackCard();
  else if (m.kind === 'siege') card.innerHTML = siegeCard();
  else if (m.kind === 'fold') card.innerHTML = foldCard(m.reg);
  else if (m.kind === 'news') card.innerHTML = newsCard(m.from, m.reports, m.sea ?? g.seaReports.length, m.air ?? g.airReports.length);
  else if (m.kind === 'declare') card.innerHTML = declareCard();
  else if (m.kind === 'invade') {
    if (!previewDeclare(g, m.on)) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = invadeCard(m.on, m.target ?? -1);
  }
  else if (m.kind === 'level') card.innerHTML = levelCard();
  else if (m.kind === 'lastact') {
    if (!lastAct(g)?.decided || lastAct(g)!.on) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = lastActCard(!!m.ask);
  }
  else if (m.kind === 'worldwon') {
    if (lastAct(g)?.winner !== g.human || !g.human) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = worldWonCard();
  }
  else if (m.kind === 'passing') {
    const list = ((m.ids ?? []) as number[]).map((id) => announcement(g, id)).filter((a): a is NonNullable<typeof a> => !!a);
    if (!list.length) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = list.length === 1 ? passingCardHtml(list[0]) : passingListHtml(list);
  }
  else if (m.kind === 'tech') card.innerHTML = techCard(m.id);
  else if (m.kind === 'traits') card.innerHTML = traitsCard(m.nat && g.nations[m.nat] ? m.nat : g.human ?? 'GER');
  else if (m.kind === 'path') {
    if (!paths(g).length) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = pathCard(m.ask ?? '');
  }
  else if (m.kind === 'ledger') {
    const nat = m.nat || g.human;
    if (!nat || !g.nations[nat]) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = ledgerCard(nat);
  }
  else if (m.kind === 'town') {
    const html = townCard(m.id);
    if (!html) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = html;
  }
  else if (m.kind === 'cabinet') {
    const html = cabinetCard(m.nat || g.human || '');
    if (!html) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = html;
    // The directive has been read: the bar and the alerts stop flagging it as new.
    const d = (m.nat || g.human) === g.human ? governmentOf(g)?.directive : null;
    if (d) ui.govSeen = d.id;
  }
  else if (m.kind === 'trade') {
    const html = tradeCard(m.nat || g.human || '');
    if (!html) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = html;
  }
  else if (m.kind === 'stores') {
    const nat = m.nat || g.human;
    if (!nat || !g.nations[nat] || !resRules(g)) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = storesCard(nat);
  }
  else if (m.kind === 'staffs') card.innerHTML = staffsCard();
  else if (m.kind === 'books') {
    const b = foreignBooks(g, m.nat);
    if (!b) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = booksCard(m.nat);
  }
  else if (m.kind === 'general') {
    const x = g.generals[m.id];
    if (!x) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = generalCard(x);
  }
  else if (m.kind === 'appoint') {
    const c = g.corps.find((k) => k.id === m.corps);
    if (!c) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = appointCard(c);
  }
  else if (m.kind === 'unit') {
    const u = unitOf(g, m.type, m.id);
    if (!u) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = unitRecordCard(m.type, u);
  }
  else if (m.kind === 'fallen') {
    const f = fallenOf(g, m.id);
    if (!f) { ui.modal = null; veil.classList.remove('open'); return; }
    card.innerHTML = fallenCard(f);
  }
  else if (m.kind === 'award') {
    const a = awardOf(g, m.id);
    if (!a) { ui.modal = null; veil.classList.remove('open'); return; }
    orderSeen(g, a.id);
    card.innerHTML = awardCard(a);
  }
  card.scrollTop = 0;
}

// ---- The start screen: the grand campaign at the front door, the historical scenarios behind it ----

interface GrandYear { year: number; turn: number; date: string; text: string }
const GRAND = grandData as unknown as { years: GrandYear[]; nations: { id: string; small?: boolean; years: Record<string, string> }[]; watch: Record<string, string> };

/** The years a country can begin a grand campaign in. Nobody at the controls can begin in any. */
const grandYears = (nat: string): number[] => (nat ? Object.keys(GRAND.nations.find((n) => n.id === nat)?.years ?? {}).map(Number) : GRAND.years.map((y) => y.year));

/** Reads back the country and year picked last time, and keeps the pick to what is on offer. */
function grandPick(): { nat: string; year: GrandYear; text: string } {
  if (ui.gcNat && !GRAND.nations.some((n) => n.id === ui.gcNat)) ui.gcNat = 'GER';
  const years = grandYears(ui.gcNat);
  if (!years.includes(ui.gcYear)) ui.gcYear = years.filter((y) => y <= ui.gcYear).pop() ?? years[0];
  const year = GRAND.years.find((y) => y.year === ui.gcYear) ?? GRAND.years[0];
  const text = ui.gcNat ? GRAND.nations.find((n) => n.id === ui.gcNat)!.years[String(year.year)] : GRAND.watch[String(year.year)] ?? GRAND.watch.later;
  return { nat: ui.gcNat, year, text };
}

function rememberPick(): void {
  try { localStorage.setItem('tc-grand', `${ui.gcNat}:${ui.gcYear}:${ui.gcLevel}:${ui.gcCabinet ? 1 : 0}:${ui.gcSecure ? 1 : 0}`); } catch { /* the pick is only a convenience */ }
}

function recallPick(): void {
  try {
    const [nat, year, level, cabinet, secure] = (localStorage.getItem('tc-grand') ?? '').split(':');
    ui.gcCabinet = cabinet !== '0';
    ui.gcSecure = secure === '1';
    if (year) { ui.gcNat = nat; ui.gcYear = Number(year) || 1939; }
    if (level && DIFFICULTY.levels.some((l) => l.id === level)) ui.gcLevel = level;
  } catch { /* nothing kept */ }
}

const startHead = (title: string): string => `<header><div class="kick">Grand strategy · 1939 to 1945</div><h2>${title}</h2></header>`;
const STEPS = `<ol class="steps"><li>Click one of your armies, then a green region to march or a red one to attack.</li><li>Press <b>End month</b> and the other nations take their turns.</li><li>Spend your factories' output under <b>Build</b>, steer your designers under <b>Research</b>, and send your fleets out under <b>Navy</b> and plan your air raids under <b>Air</b>. The game saves itself as you play.</li><li>Drag the map to move it. Scroll or pinch to zoom.</li></ol>`;

const levelById = (id: string): Level => DIFFICULTY.levels.find((l) => l.id === id) ?? DIFFICULTY.levels[FAIR];
/** A level's badge of rank: one pip for the easiest, five for the hardest. */
const pips = (k: number): string => `<span class="pips" aria-hidden="true">${'<i></i>'.repeat(k + 1)}</span>`;

/** The five levels side by side, the chosen one lit, and under them what the chosen one does. */
function levelPicker(chosen: string, act: string, full = false): string {
  const on = levelById(chosen);
  return `<div class="gclevels">${DIFFICULTY.levels.map((l, k) => `<button class="gclevel${l.id === on.id ? ' on' : ''}" data-act="${act}" data-level="${l.id}" aria-pressed="${l.id === on.id}">${pips(k)}<b>${esc(l.name)}</b>${k === FAIR ? '<span>The fair fight</span>' : k < FAIR ? '<span>Easier</span>' : '<span>Harder</span>'}</button>`).join('')}</div>
    <div class="gclevelnote"><p>${esc(on.blurb)}</p><ul>${on.lines.map((x) => `<li>${esc(x)}</li>`).join('')}</ul>${full ? '<small>Battles are worked out the same way at every level. A level changes what each side can afford and how the rival staffs think, never the dice.</small>' : ''}</div>`;
}

/** The switch for the war cabinet: with one, or without. */
function cabinetPicker(on: boolean, act: string, live = false): string {
  const note = on
    ? 'Your government sits behind you. You appoint ministers and pass decrees with political capital, keep the trust of a war cabinet that reads the news each month, and every few months it sets you a directive with a reward.'
    : `No ministers, no decrees, no trust to keep and no directives: you run the war alone and nothing the cabinet would move is moved. ${live ? 'What stands open is settled first, each at its usual price: a directive is set aside, a promise is taken back, and a general the cabinet wants dismissed is stood by. The cabinet is kept as you left it should you want it back.' : 'You can switch it on in any month from the level badge beside the date.'}`;
  return `<div class="gclevels two"><button class="gclevel${on ? ' on' : ''}" data-act="${act}" data-on="1" aria-pressed="${on}"><b>With a war cabinet</b><span>Ministers, decrees, directives</span></button><button class="gclevel${on ? '' : ' on'}" data-act="${act}" data-on="0" aria-pressed="${!on}"><b>No cabinet</b><span>The war without the politics</span></button></div>
    <div class="gclevelnote"><p>${note}</p></div>`;
}

/** Whether the player's government can fall: two buttons and what each means. A setting of its own beside the cabinet's switch. */
function officePicker(secure: boolean, act: string): string {
  const note = secure
    ? 'The cabinet, its ministers, its directives and its trust all work as they do. A cabinet that doubts you still holds back money and political capital. But it never falls and you are never turned out of office, however the war goes.'
    : `A cabinet left with no trust in you for ${GOV.trust.crisisMonths} months running falls: caretakers take every office, your newest decrees are withdrawn and your political capital is spent.`;
  return `<div class="gclevels two"><button class="gclevel${secure ? '' : ' on'}" data-act="${act}" data-secure="0" aria-pressed="${!secure}"><b>You can be brought down</b><span>Lose its trust and the cabinet falls</span></button><button class="gclevel${secure ? ' on' : ''}" data-act="${act}" data-secure="1" aria-pressed="${secure}"><b>Your office is secure</b><span>The cabinet never falls</span></button></div>
    <div class="gclevelnote"><p>${note}</p></div>`;
}

/** True for a nation with abilities or a special formation of its own. */
const hasTraits = (nat: string): boolean => traitsOf(nat).length > 0 || !!eliteOf(nat);

/**
 * A nation's abilities and its special formation. Short: the name and what it does. In full: the line of history too.
 * What it does is worked out from the rule's own numbers, so the card cannot promise what the rules do not do.
 */
function traitList(nat: string, full = false): string {
  const u = eliteOf(nat);
  const row = (name: string, text: string, rule: string, star = false): string => `<li${star ? ' class="unit"' : ''}><b>${star ? '<i>★</i> ' : ''}${esc(name)}</b>${full && text ? `<span>${esc(text)}</span>` : ''}${rule ? `<em>${esc(rule)}</em>` : ''}</li>`;
  return `<ul class="traits${full ? ' full' : ''}">${traitsOf(nat).map((t) => row(t.name, t.text, traitRule(t, BUDGET.scale))).join('')}${u ? row(u.label, u.text, `Special formation. ${eliteRule(u)}`, true) : ''}</ul>`;
}

/** What a country does best, with every other country's abilities a click away. */
function traitsCard(nat: string): string {
  const n = g.nations[nat];
  const u = eliteOf(nat);
  const mine = nat === g.human && isOpen(g);
  const all = Object.keys(TRAITS.nations).filter((id) => g.nations[id]);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">National abilities</div><h2>${flagIcon(nat, n.color, 30)} ${esc(n.name)}</h2></header>
    <div class="body"><p class="lead">Every country fights, builds and earns in its own way. An ability works the same whether you or the computer leads the country, and a battle's ticket names the ones that count in it.</p>
    ${hasTraits(nat) ? traitList(nat, true) : '<div class="empty">This country has no abilities of its own.</div>'}
    ${u && mine ? `<p class="sub" style="margin-top:8px">You have <b style="color:var(--ink)">${eliteCount(g, nat)} of ${u.most}</b> ${esc(u.label.toLowerCase())} in the field or on order. Order more from the <b style="color:var(--ink)">Build</b> tab, under Army.</p>` : ''}
    <h3>Every country</h3><div class="trnats">${all.map((id) => `<button class="trnat${id === nat ? ' on' : ''}" data-act="traits" data-nat="${id}" aria-pressed="${id === nat}">${flagIcon(id, g.nations[id].color, 22)}<span>${esc(natName(id))}</span></button>`).join('')}</div></div>
    <footer><button class="btn go" data-act="close">Close</button></footer>`;
}

// ---- The war cabinet (the grand campaign) -------------------------------------------------------

const signed = (x: number): string => `${x > 0 ? '+' : '−'}${Math.abs(x)}`;
const FORM_WORD: Record<GovView['form']['id'], string> = { democracy: 'Democracy', oneparty: 'One party', authoritarian: 'Strong man' };

/** Where a directive that names no town is carried out: the button that takes the player there. */
const GOV_GO: Partial<Record<string, string>> = { build: 'Open the building list', agent: 'Open the secret service', raise: 'Open the army list', trade: 'Open trade agreements' };

/** Why the Cabinet button is flagged, or '' if nothing there wants the player. */
function cabinetFlag(): string {
  const v = g.human ? governmentOf(g) : null;
  if (!v?.mine) return '';
  if (v.low > 0 || v.trust <= v.fallsAt) return 'The war cabinet has no trust left in you.';
  if (v.head) return `The war cabinet wants ${v.head.name} dismissed, and awaits your answer.`;
  const d = v.directive;
  if (d && d.left <= 2) return `${d.left <= 1 ? 'The last month' : 'Two months left'} for the directive: ${d.title}.`;
  if (d && ui.govSeen !== d.id) return `A new directive: ${d.title}.`;
  if (v.doubts) return `The war cabinet doubts you. ${v.withheld ?? ''}`;
  return '';
}

/** A government: its form, and for the player the cabinet's trust in him, his political capital, the directive, the four offices and the decrees. */
function cabinetCard(nat: string): string {
  const v: GovView | null = governmentOf(g, nat);
  const n = g.nations[nat];
  if (!v || !n) return '';
  const head = `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${v.mine ? 'Your government' : 'Government'}</div><h2>${flagIcon(nat, n.color, 30)} ${esc(n.name)}</h2></header>`;
  const form = `<p class="lead"><b>${esc(v.form.name)}.</b> ${esc(v.form.blurb)}</p>`;
  const foot = `<footer>${v.mine ? '<button class="btn quiet" data-act="level" data-part="cabinet" title="Play on without ministers, decrees, trust or directives. It can be switched back on.">Play without a cabinet</button>' : ''}<button class="btn go" data-act="close">Close</button></footer>`;
  if (!v.mine) return `${head}<div class="body gov">${form}<div class="note">A government opens its cabinet room only to the one who leads it.${g.human ? ' Yours is under <b>Cabinet</b> on the top bar.' : ''}</div></div>${foot}`;
  const up = humanUp(g);
  const C = GOV.capital;
  const months = (k: number): string => (k === 1 ? 'a month' : `${k} months`);
  // The meter: where the cabinet's trust stands, the ground on which it falls, doubts and believes, and the level it settles at.
  const meter = `<div class="gvmeter" role="img" aria-label="The cabinet's trust in you is ${v.trust} of 100: ${v.trustWord.toLowerCase()}. ${v.secure ? 'Your office is secure: the cabinet cannot fall.' : `At ${v.fallsAt} or under for ${v.crisisMonths} months running the cabinet falls.`}">
      <i class="z fall" style="width:${v.fallsAt}%"></i><i class="z doubt" style="left:${v.fallsAt}%;width:${C.lowTrust - v.fallsAt}%"></i><i class="z firm" style="left:${C.highTrust}%;width:${100 - C.highTrust}%"></i>
      <i class="rest" style="left:${v.rest}%" title="With no news the cabinet's trust settles here, at ${v.rest}."></i><i class="pin" style="left:${v.trust}%"></i></div>
    <div class="gvscale"><span style="left:${v.fallsAt / 2}%">${v.secure ? 'None' : 'Falls'}</span><span style="left:${(v.fallsAt + C.lowTrust) / 2}%">Doubts</span><span style="left:${(C.lowTrust + C.highTrust) / 2}%">Follows</span><span style="left:${(C.highTrust + 100) / 2}%">Believes</span></div>`;
  const moved = v.moved.length ? `<div class="gvmoved"><small>Since the cabinet last met</small>${v.moved.map((c) => `<span class="${c.amount > 0 ? 'up' : 'down'}"><b>${signed(c.amount)}</b> ${esc(c.label)}</span>`).join('')}</div>` : '';
  const state = v.secure && v.trust <= v.fallsAt
    ? `<div class="note bad"><b>The cabinet has no trust left in you.</b> Your office is secure, so it cannot bring you down. ${esc(v.withheld ?? '')} Ground won, a directive carried out or a paymaster at the Ministry of Finance would steady it.</div>`
    : v.low > 0 || v.trust <= v.fallsAt
    ? `<div class="note bad"><b>The cabinet has no trust left in you.</b> ${v.low >= v.crisisMonths - 1 ? 'Another month of this' : `${upFirst(months(v.crisisMonths - v.low))} more of this`} and it falls: caretakers take every office, your newest decrees are withdrawn and your political capital is spent. Ground won, a directive carried out or a paymaster at the Ministry of Finance would steady it.</div>`
    : v.trust < C.lowTrust ? `<div class="note warn"><b>The cabinet doubts you.</b> ${esc(v.withheld ?? '')}</div>`
    : v.trust >= C.highTrust ? `<div class="note good"><b>The cabinet believes in you.</b> It adds ${C.highAdd} political capital a month while its trust stays at ${C.highTrust} or over.</div>` : '';
  const ignored = v.missed > 0 ? `<p class="sub">${v.missed === 1 ? 'One directive has been' : `${v.missed} directives in a row have been`} ignored, so the cabinet's trust now settles at ${v.rest}. Carry one out and that is forgotten.</p>` : '';
  const trust = `<div class="gvtrust"><div class="gvnum"><b>${v.trust}</b><span>${esc(v.trustWord)}</span><small>The war cabinet's trust in you</small></div><div class="gvbar">${meter}</div></div>${moved}${state}${ignored}
    <div class="facts"><div class="fact"><span>Political capital</span><b>${v.capital} <small>of ${v.capitalMost}</small></b></div><div class="fact"><span>Each month</span><b${v.capitalMonth < 0 ? ' class="neg"' : ''}>${v.capitalMonth < 0 ? '−' : '+'}${Math.abs(Math.round(v.capitalMonth * 10) / 10)}</b></div><div class="fact"><span>Directives carried out</span><b>${v.done}</b></div><div class="fact"><span>Ignored in a row</span><b>${v.missed}</b></div></div>`;
  // The directive.
  const d = v.directive;
  const asking = ui.modal?.ask === 'aside';
  const dir = d
    ? `<div class="gvdir${d.left <= 2 ? ' late' : ''}"><div class="gvdhead">${flagIcon(d.from, g.nations[d.from].color, 26)}<span class="grow"><small>${d.from === nat ? 'The war cabinet directs' : `${esc(d.by)} asks`}</small><b>${esc(d.title)}</b>${d.hard ? ' <span class="tag warn" title="The cabinet knows the place is strongly held, so it gives more months and a larger reward.">Strongly held</span>' : ''}${d.again ? ' <span class="tag" title="The cabinet has asked for this town before in this war.">Asked again</span>' : ''}</span><span class="gvclock" title="The job must be done by the end of ${date(d.due)}."><b>${d.left}</b><small>month${d.left === 1 ? '' : 's'} left</small></span></div>
        <p>${esc(d.text)}</p><p class="gvprog">${esc(d.progress)}</p>${d.oddsLine ? `<p class="gvodds">${esc(d.oddsLine)}</p>` : ''}
        <div class="gvterms"><div><span class="tag good">Carried out</span> ${esc(upFirst(d.reward))}, and the cabinet's trust rises.</div><div><span class="tag bad">Ignored</span> ${d.small ? `A small matter: the cabinet's trust falls by ${d.price} and no further. It is not counted with the directives you have ignored.` : `The cabinet's trust falls by ${d.price}, and settles lower until a directive is carried out.`}</div>${d.kind === 'hold' && !d.tested ? '<div><span class="tag">Not attacked</span> If the enemy never comes for it the order is withdrawn, with no reward and no blame.</div>' : ''}<div><span class="tag">Set aside</span> The cabinet's trust falls by ${d.asidePrice}, and the next directive comes no sooner for it.</div></div>
        ${asking ? `<div class="note warn"><b>Set this directive aside?</b> The cabinet's trust falls by ${d.asidePrice} at once: that is less than the ${d.price} it would cost to let the order run out undone. The next directive is set in ${date(d.asideNext)}, when it would have come anyway.</div><div class="acts"><button class="btn small warn" data-act="govaside" ${up ? '' : 'disabled'}>Yes, set it aside</button><button class="btn small" data-act="govkeep">Keep it</button></div>`
          : `<div class="acts">${d.region !== undefined ? `<button class="btn small go" data-act="govshow" data-id="${d.region}">Show it on the map</button>` : ''}${GOV_GO[d.kind] ? `<button class="btn small go" data-act="govgo" data-to="${d.kind}">${GOV_GO[d.kind]}</button>` : ''}<button class="btn small" data-act="govaside" ${up ? '' : 'disabled title="It is not your month."'}>Set it aside</button></div>`}</div>`
    : `<div class="gvdir none"><p>No directive stands. ${v.nextIn > 0 ? `The cabinet is expected to set one in ${months(v.nextIn)}.` : 'The cabinet sets one as your next month opens.'} A directive is a job with a last month and a reward, chosen from the state of the war. It is yours to carry out or to ignore.</p></div>`;
  const ask = String(ui.modal?.ask ?? '').split(':');
  const clock = (left: number, due: number, what: string): string => `<span class="gvclock" title="${what} the end of ${date(due)}."><b>${left}</b><small>month${left === 1 ? '' : 's'} left</small></span>`;
  // The cabinet wants a head: a general it asks to see dismissed, and what each answer does.
  const hd = v.head;
  let wants = '';
  if (hd) {
    const q = ask[0] === 'head' ? ask[1] : '';
    const body = q === 'sack'
      ? `<div class="note warn"><b>Dismiss ${esc(hd.name)}?</b> The cabinet's trust rises by ${hd.gain}. He goes for good, and what he knew goes with him.${hd.loved ? ' The army thinks well of him: his corps will lose cohesion, though the cabinet carries the political cost.' : ''}</div><div class="acts"><button class="btn small go" data-act="govhead" data-how="sack" data-sure="1" ${up ? '' : 'disabled'}>Yes, dismiss him</button><button class="btn small" data-act="govkeep">Not now</button></div>`
      : q === 'stand'
        ? `<div class="note warn"><b>Stand by ${esc(hd.name)}?</b> The cabinet's trust falls by ${hd.cost} at once, and for ${months(GOV.head.watch)} it watches him. If he wins a battle in that time he is vindicated: its trust rises by ${hd.vindicated}, it never asks for him again, and he carries a mark that makes his corps 4% stronger for the rest of the war. If he loses his next battle first, its trust falls by ${hd.wrong} more.</div><div class="acts"><button class="btn small go" data-act="govhead" data-how="stand" data-sure="1" ${up ? '' : 'disabled'}>Yes, stand by him</button><button class="btn small" data-act="govkeep">Not now</button></div>`
        : `<div class="acts"><button class="btn small" data-act="govhead" data-how="sack" ${up ? '' : 'disabled title="It is not your month."'}>Dismiss him · trust +${hd.gain}</button><button class="btn small" data-act="govhead" data-how="stand" ${up ? '' : 'disabled title="It is not your month."'}>Stand by him · trust −${hd.cost}</button><button class="btn small" data-act="general" data-id="${hd.general}">See his record</button></div>`;
    wants = `<h3 id="gvhead">The cabinet wants a head</h3><div class="gvdir head${hd.left <= 1 ? ' late' : ''}"><div class="gvdhead"><span class="grow"><small>The war cabinet asks you to dismiss</small><b>${esc(hd.name)}</b></span>${clock(hd.left, g.turn + hd.left - 1, 'The cabinet wants its answer by')}</div>
        <p>${esc(hd.rank)}${hd.command ? `, commanding ${esc(hd.command)}` : ', without a command'}. ${esc(hd.why)}</p>
        <div class="gvrecord"><small>His record</small><b>${hd.fought} battle${hd.fought === 1 ? '' : 's'} fought, ${hd.won} won</b>${hd.battles.length ? `<small>What the cabinet holds against him</small><ul>${hd.battles.map((b) => `<li>${esc(b.text)}</li>`).join('')}</ul>` : ''}</div>
        <p class="gvprog">${hd.left <= 1 ? 'It wants your answer this month' : `It wants your answer within ${months(hd.left)}`}. No answer is taken as standing by him.</p>${body}</div>`;
  } else if (v.stoodBy) {
    wants = `<p class="sub gvstood">You stood by <b>${esc(v.stoodBy.name)}</b>. The cabinet watches him for ${months(v.stoodBy.left)} more: a battle won vindicates him, a battle lost first proves the cabinet right.</p>`;
  }
  // Call your shot: the player's own promise, or the way to make one.
  const BAND: Record<string, string> = { safe: 'Safe', fair: 'Fair', bold: 'Bold', daring: 'Daring' };
  const TOWN_SIZE: Record<string, string> = { village: 'Village', town: 'Town', city: 'City', capital: 'Capital' };
  const pv = v.promise;
  let vow: string;
  if (pv) {
    vow = `<div class="gvdir vow${pv.left <= 1 ? ' late' : ''}"><div class="gvdhead"><span class="grow"><small>Your word to the cabinet</small><b>${esc(pv.title)}</b> <span class="tag band ${pv.band}" title="How bold the cabinet reckoned it on the day you gave your word: your troops stood at ${pv.odds} to 1 against the town.">${esc(pv.bandName)}</span></span>${clock(pv.left, pv.due, 'Your word is good to')}</div>
        <p class="gvprog">${esc(pv.progress)}</p>
        <div class="gvterms"><div><span class="tag good">Kept</span> ${esc(upFirst(pv.reward))}.</div><div><span class="tag bad">Broken</span> The cabinet's trust falls by ${pv.price}.</div><div><span class="tag">Taken back</span> The cabinet's trust falls by ${pv.back}.</div></div>
        ${ask[0] === 'back' ? `<div class="note warn"><b>Take your word back?</b> The cabinet's trust falls by ${pv.back} at once: that is less than the ${pv.price} a broken promise would cost.</div><div class="acts"><button class="btn small go" data-act="govback" data-sure="1" ${up ? '' : 'disabled'}>Yes, take it back</button><button class="btn small" data-act="govkeep">Not now</button></div>`
          : `<div class="acts"><button class="btn small go" data-act="govshow" data-id="${pv.region}">Show it on the map</button><button class="btn small" data-act="govback" ${up ? '' : 'disabled title="It is not your month."'}>Take it back</button></div>`}</div>`;
  } else if (ask[0] === 'promise' && !v.promiseWhy) {
    const reg = ask[1] ? Number(ask[1]) : -1, mo = Number(ask[2] ?? 0);
    const picked: PromiseTerms | null = reg >= 0 && mo ? promiseTermsOf(g, reg, mo) : null;
    if (picked) {
      vow = `<div class="gvdir vow asking"><div class="note warn"><b>Give the cabinet your word?</b> ${esc(picked.text)} You may take it back before its day for ${picked.back} of its trust, and the cabinet hears one promise every ${months(GOV.promise.every)}.</div>
          <div class="acts"><button class="btn small go" data-act="govpromise" data-id="${reg}" data-months="${mo}" data-sure="1" ${up ? '' : 'disabled'}>Yes, I give my word</button><button class="btn small" data-act="govpromise">Choose another</button><button class="btn small" data-act="govkeep">Not now</button></div></div>`;
    } else {
      let list: PromiseTarget[] = promiseTargetsOf(g, 8);
      // A town picked on the map heads the list, whether or not the cabinet would have suggested it.
      if (reg >= 0) {
        const own = GOV.promise.months.map((k) => promiseTermsOf(g, reg, k)).filter((x): x is PromiseTerms => !!x);
        const r0 = g.regions[reg];
        if (own.length && r0 && whyNotOrderPromise(g, reg) === null) list = [{ region: reg, town: own[0].town, size: townSize(g, r0), holder: own[0].holder, odds: own[0].odds, depth: own[0].depth, terms: own }, ...list.filter((x) => x.region !== reg)].slice(0, 8);
      }
      const row = (x: PromiseTarget): string => `<div class="vowrow${x.region === reg ? ' pick' : ''}"><span class="grow"><b>${esc(x.town)}</b><small>${TOWN_SIZE[x.size] ?? 'Town'} · held by ${esc(theName(x.holder))} · your troops ${x.odds >= 10 ? 'better than 10' : x.odds >= 1 ? Math.round(x.odds * 10) / 10 : x.odds} to 1 · ${x.depth <= 1 ? 'beside you' : `${x.depth} provinces off`}</small></span><span class="vowopts">${x.terms.map((t) => `<button class="vowopt ${t.band}" data-act="govpromise" data-id="${x.region}" data-months="${t.months}" ${up ? '' : 'disabled'} title="${esc(t.text)}"><b>${t.months} mo</b><small>${BAND[t.band] ?? ''}</small></button>`).join('')}</span></div>`;
      vow = `<div class="gvdir vow asking"><p>Name the town and the time you allow yourself. The cabinet reads the odds as they stand today: the weaker your troops within reach, the deeper the town and the shorter the time, the bolder the promise. A safe promise earns little. A bold one earns a great deal if it is kept, and costs you dearly if it is not.</p>
          ${list.length ? `<div class="vowlist">${list.map(row).join('')}</div><p class="sub">Any other enemy town within ${GOV.promise.reach} provinces can be promised from its province on the map.</p>` : '<div class="empty">No enemy town lies within reach of your ground or your troops.</div>'}
          <div class="acts"><button class="btn small" data-act="govkeep">Not now</button></div></div>`;
    }
  } else {
    vow = `<div class="gvdir none">${v.promiseWhy ? `<p>${esc(v.promiseWhy)}</p>` : `<p>You may go to the cabinet with a promise of your own: an enemy town, by a day you name. The odds on the day set the stakes. Keep your word and the cabinet's trust rises, with political capital and money for a bold one; break it and its trust falls.</p><div class="acts"><button class="btn small go" data-act="govpromise" ${up ? '' : 'disabled title="It is not your month."'}>Go to the cabinet with a promise</button></div>`}${v.kept || v.broken ? `<p class="sub">Promises kept: ${v.kept}. Broken: ${v.broken}.</p>` : ''}</div>`;
  }
  // Hawks and doves: how the table leans.
  const w = v.wings;
  const table = `<div class="gvwings"><div class="gvwbar" role="img" aria-label="${w.doves} of your ministers sit with the doves and ${w.hawks} with the hawks: ${esc(w.word.toLowerCase())}."><span class="lab">Doves <b>${w.doves}</b></span><div class="track"><i class="z dove" style="width:${((4 - w.at) / 8) * 100}%"></i><i class="z hawk" style="width:${((4 - w.at) / 8) * 100}%"></i><i class="mid"></i><i class="pin" style="left:${((w.lean + 4) / 8) * 100}%"></i></div><span class="lab r"><b>${w.hawks}</b> Hawks</span></div>
      <p><b>${esc(w.word)}.</b> ${esc(w.text)}</p>${w.effects.length ? `<div class="gvsums">${w.effects.map((e) => `<span class="tag">${esc(e)}</span>`).join('')}</div>` : ''}</div>`;
  const wingTag = (m: { wing: 'hawk' | 'dove' | null }): string => (m.wing ? ` <span class="tag wing ${m.wing}" title="He sits with the ${m.wing}s, and tilts the table their way.">${m.wing === 'hawk' ? 'Hawk' : 'Dove'}</span>` : '');
  // The four offices.
  const seats = v.seats.map((s) => {
    const m = s.minister, empty = m.id === 'caretaker';
    const pick = ask[0] === 'appoint' && ask[1] === s.id ? s.options.find((o) => o.id === ask[2] && !o.why) : undefined;
    if (pick) {
      const out = pick.id === 'caretaker';
      const q = out
        ? `<div class="note warn"><b>Dismiss ${esc(m.name.toLowerCase())}?</b> A caretaker takes the office at no cost. What the minister gave ends, and so does what he cost. The office then stays as it is for ${months(GOV.appoint.wait)}.</div>`
        : `<div class="note warn"><b>Appoint ${esc(pick.name.toLowerCase())} as ${esc(s.name)}?</b> It costs ${v.appointCost} of your ${v.capital} political capital. <span class="plus">${esc(pick.plus)}.</span> <span class="minus">${esc(pick.minus)}.</span> The office then stays as it is for ${months(GOV.appoint.wait)}: he cannot be dismissed before ${date(g.turn + GOV.appoint.wait)}.</div>`;
      return `<div class="gvseat asking"><div class="gvsh"><small>${esc(s.name)}</small><b>${esc(m.name)}</b></div>${q}<div class="acts"><button class="btn small go" data-act="govappoint" data-seat="${s.id}" data-min="${pick.id}" data-sure="1" ${up ? '' : 'disabled'}>${out ? 'Yes, dismiss him' : 'Yes, appoint him'}</button><button class="btn small" data-act="govkeep">Not now</button></div></div>`;
    }
    const opts = s.options.filter((o) => o.id !== 'caretaker').map((o) => `<button class="buy offer" data-act="govappoint" data-seat="${s.id}" data-min="${o.id}" ${o.why || !up ? 'disabled' : ''}><span class="grow"><b>${esc(o.name)}${wingTag(o)}</b><small class="plus">${esc(o.plus)}</small><small class="minus">${esc(o.minus)}</small>${o.why ? `<small class="whyno">${esc(o.why)}</small>` : ''}</span><span class="cost${o.why ? ' short' : ''}">${v.appointCost}<small>capital</small></span></button>`).join('');
    const back = empty ? '' : (() => { const c = s.options.find((o) => o.id === 'caretaker'); return c ? `<button class="btn small" data-act="govappoint" data-seat="${s.id}" data-min="caretaker" ${c.why || !up ? `disabled title="${esc(c.why ?? 'It is not your month.')}"` : 'title="He leaves office and a caretaker takes it, at no cost."'}>Dismiss him</button>` : ''; })();
    return `<div class="gvseat${empty ? ' empty' : ''}"><div class="gvsh"><small>${esc(s.name)}</small><b>${esc(m.name)}${wingTag(m)}</b>${back}</div>${empty ? '<p class="gvpm">The office runs itself and changes nothing.</p>' : `<p class="gvpm"><span class="plus">${esc(m.plus)}.</span> <span class="minus">${esc(m.minus)}.</span></p>`}${opts}</div>`;
  }).join('');
  // The decrees, each a ladder of steps.
  const sure = (t: GovView['tracks'][number]): string => {
    if (ask[0] !== 'decree' || ask[1] !== t.id) return '';
    const i = Number(ask[2]), s = t.steps[i];
    if (!s || s.why || (i !== t.at + 1 && i !== t.at - 1)) return '';
    const upStep = i > t.at;
    const q = upStep
      ? `<b>Decree ${esc(s.name.toLowerCase())}?</b> It costs ${s.cost} of your ${v.capital} political capital. ${esc(s.text)} It can be relaxed later at no cost, but not tightened again for ${months(GOV.decree.wait)} after that.`
      : `<b>Relax to ${esc(s.name.toLowerCase())}?</b> It costs nothing, and what ${esc(t.steps[t.at].name.toLowerCase())} gave and cost ends at once. It cannot be tightened again for ${months(GOV.decree.wait)}.`;
    return `<div class="note warn">${q}</div><div class="acts"><button class="btn small go" data-act="govdecree" data-track="${t.id}" data-step="${i}" data-sure="1" ${up ? '' : 'disabled'}>${upStep ? 'Yes, decree it' : 'Yes, relax it'}</button><button class="btn small" data-act="govkeep">Not now</button></div>`;
  };
  const tracks = v.tracks.map((t) => `<div class="gvtrack"><h4>${esc(t.name)}</h4><div class="gvsteps">${t.steps.map((s, i) => {
    const btn = s.on ? '<span class="tag good">In force</span>'
      : i === t.at + 1 ? `<button class="btn small${s.why ? '' : ' go'}" data-act="govdecree" data-track="${t.id}" data-step="${i}" ${s.why || !up ? 'disabled' : ''}>Decree · ${s.cost} capital</button>${s.why ? `<small class="whyno">${esc(s.why)}</small>` : ''}`
      : i === t.at - 1 ? `<button class="btn small" data-act="govdecree" data-track="${t.id}" data-step="${i}" ${s.why || !up ? 'disabled' : ''} title="Free. It cannot be tightened again for ${months(GOV.decree.wait)}.">Relax to this</button>` : '';
    return `<div class="gvstep${s.on ? ' on' : i < t.at ? ' past' : ''}"><b>${esc(s.name)}</b><small>${esc(s.text)}</small>${btn}</div>`;
  }).join('<i class="gvarrow">▸</i>')}</div>${sure(t)}</div>`).join('');
  const sums = v.sums.length ? `<div class="gvsums">${v.sums.map((s) => `<span class="tag">${esc(s)}</span>`).join('')}</div>` : '<p class="sub">Caretakers in every office and no decree in force: nothing is changed.</p>';
  const log = v.log.length ? `<h3>The cabinet's minutes</h3><ul class="gvlog">${v.log.map((l) => `<li><small>${date(l.turn)}</small>${esc(l.text)}</li>`).join('')}</ul>` : '';
  // Four pages, so that the cabinet's business (its trust, the directive, a question about a general, your promise) is all on the first screen.
  const tab = ask[0] === 'appoint' ? 'table' : ask[0] === 'decree' ? 'decrees' : ask[0] ? 'now' : ui.govTab;
  const filled = v.seats.filter((s) => s.minister.id !== 'caretaker').length, inForce = v.tracks.filter((t) => t.at > 0).length;
  const page = (id: typeof ui.govTab, label: string, n = ''): string => `<button class="btn small${tab === id ? ' on' : ''}" data-act="govtab" data-tab="${id}" aria-pressed="${tab === id}">${label}${n ? ` <small>${n}</small>` : ''}</button>`;
  const tabs = `<div class="seg gvtabs">${page('now', 'Business', hd ? 'a question' : d && d.left <= 2 ? 'due' : '')}${page('table', 'Ministers', `${filled} of ${v.seats.length}`)}${page('decrees', 'Decrees', inForce ? `${inForce} in force` : '')}${page('minutes', 'Minutes')}</div>`;
  const comes = `<h3>What your government comes to</h3>${sums}`;
  const shown = tab === 'table' ? `<h3>The war cabinet <span class="inhand">an appointment costs ${v.appointCost} political capital, and the office then stays ${months(GOV.appoint.wait)}</span></h3>${table}<div class="gvseats">${seats}</div>${comes}`
    : tab === 'decrees' ? `<h3>Decrees <span class="inhand">paid for in political capital, one step at a time</span></h3>${tracks}${comes}`
    : tab === 'minutes' ? (log || '<h3>The cabinet\'s minutes</h3><p class="sub">Nothing has been minuted yet.</p>')
    : `<h3>The directive</h3>${dir}${wants}
    <h3 id="gvvow">Your word to the cabinet <span class="inhand">a promise of your own, once every ${months(GOV.promise.every)}</span></h3>${vow}`;
  return `${head}<div class="body gov">${form}${trust}${tabs}${shown}</div>${foot}`;
}

/** Puts a province in the middle of the map and opens its panel. */
function showRegion(id: number): void {
  ui.tab = 'region';
  selectRegion(id);
  refresh();
  const svg = $('#map');
  const tall = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.min(ui.view.w, 700);
  glide({ x: g.regions[id].x - w / 2, y: g.regions[id].y - (w * tall) / 2, w });
}

/** Asked before the cabinet is switched off with business open: what is settled on the way out, and what each item costs. */
function cabinetOffAsk(): string {
  if (ui.modal?.ask !== 'caboff' || cabinetIsOff(g)) return '';
  const bill = cabinetBillOf(g), sum = Math.round(bill.reduce((s, b) => s + b.trust, 0) * 10) / 10;
  return `<div class="note warn"><b>Switch the war cabinet off?</b> Its open business is settled first:<ul class="gvbill">${bill.map((b) => `<li><span>${esc(b.text)}</span><b>trust −${b.trust}</b></li>`).join('')}</ul>In all the cabinet's trust falls by ${sum}, and it is kept at that should you want it back.</div>
    <div class="acts"><button class="btn small warn" data-act="setcabinet" data-on="0" data-sure="1">Yes, switch it off</button><button class="btn small" data-act="govkeep">Not now</button></div>`;
}

/** The level of the campaign under way, with the chance to change it. */
function levelCard(): string {
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The grand campaign</div><h2>Campaign settings</h2></header>
    <div class="body"><p class="lead">This campaign is being fought at <b>${esc(levelOf(g).name)}</b>, ${cabinetIsOff(g) ? 'with the war cabinet switched off' : `with a war cabinet${officeIsSecure(g) ? ' that cannot bring you down' : ''}`}. You can change any of it in any month, and it takes hold at once.</p>
    <h3>How hard a war</h3>${levelPicker(levelOf(g).id, 'setlevel', true)}
    <h3 id="setcab">The war cabinet</h3>${cabinetPicker(!cabinetIsOff(g), 'setcabinet', true)}${cabinetOffAsk()}
    <h3 id="setoffice">Your office</h3>${cabinetIsOff(g) ? '<p class="sub">With no war cabinet there is nobody to bring you down. Switch the cabinet on and you can say here whether it may.</p>' : officePicker(officeIsSecure(g), 'setoffice')}</div>
    <footer><button class="btn go" data-act="close">Close</button></footer>`;
}

function startCard(): string {
  if (ui.menu === 'scenarios') return scenarioCard();
  const saved = peekSave(readSave() ?? '');
  const resume = saved ? `<button class="sidebtn wide resume" data-act="continue"><b>Continue your campaign</b><span>${saved.human ? `${esc(natName(saved.human))}, ${date(saved.turn)}` : `${saved.open ? 'Watching a war nobody has seen' : 'Watching history'}, ${date(saved.turn)}`}${saved.human && saved.open ? `, grand campaign${saved.level !== null && DIFFICULTY.levels[saved.level] ? `, ${esc(DIFFICULTY.levels[saved.level].name)}` : ''}` : ''}. Saved in this browser.</span></button>` : '';
  const pick = grandPick();
  const years = grandYears(pick.nat);
  const tile = (id: string, name: string, size = 54): string => `<button class="gcnat${id === pick.nat ? ' on' : ''}" data-act="gcnat" data-nat="${id}" aria-pressed="${id === pick.nat}">${id ? flagIcon(id, g.nations[id].color, size) : '<span class="gcwatch">◉</span>'}<b>${esc(name)}</b></button>`;
  const smallPick = GRAND.nations.some((n) => n.small && n.id === pick.nat);
  const goals = pick.nat && pick.year.turn === 1 ? GOAL_KEYS.map((key) => { const goal = GOALS[pick.nat]?.[key]; return goal ? `<span class="sgoal"><i>${key === 'short' ? 'Short campaign' : 'Long campaign'}</i>${esc(goal.text)}</span>` : ''; }).join('') : '';
  return `${startHead('Theater Command')}
    <div class="body"><p class="lead">Pick a country and a year, then <b>make your own history</b>. From the day you take over nobody follows the history books: you decide where your armies, fleets and air fleets go, and so does the staff of every other nation.</p>
    ${resume ? `<div class="sides">${resume}</div>` : staleSave(readSave() ?? '') ? '<div class="note">The campaign saved in this browser was played on an earlier, smaller map and cannot be continued in this version. Starting a new game replaces it.</div>' : ''}
    <h3>The grand campaign · choose your country</h3>
    <div class="gcnats">${GRAND.nations.filter((n) => !n.small).map((n) => tile(n.id, g.nations[n.id].name)).join('')}${tile('', 'Watch')}</div>
    <h3>Or a smaller nation · choose your own path</h3>
    <div class="gcnats small">${GRAND.nations.filter((n) => n.small).map((n) => tile(n.id, g.nations[n.id].name, 38)).join('')}</div>
    <p class="gcsmallnote">A small country decides where it stands: join the Axis, join the Allies, stand alone, or found a League of smaller nations with its neighbours.</p>
    <h3>Choose the year you take over</h3>
    <div class="gcyears">${GRAND.years.map((y) => `<button class="gcyear${y.year === pick.year.year ? ' on' : ''}" data-act="gcyear" data-year="${y.year}" ${years.includes(y.year) ? '' : 'disabled'} aria-pressed="${y.year === pick.year.year}"><b>${y.year}</b><span>${y.turn === 1 ? 'The war begins' : esc(date(y.turn))}</span></button>`).join('')}</div>
    ${pick.nat ? `<h3>Choose how hard a war</h3>${levelPicker(ui.gcLevel, 'gclevel')}<h3>The war cabinet</h3>${cabinetPicker(ui.gcCabinet, 'gccabinet')}${ui.gcCabinet ? `<h3>Your office</h3>${officePicker(ui.gcSecure, 'gcsecure')}` : ''}` : ''}
    <div class="gcbrief">${pick.nat ? flagIcon(pick.nat, g.nations[pick.nat].color, 88) : ''}<div class="grow"><div class="kick">${esc(pick.year.date)}${pick.nat ? ` · ${esc(levelById(ui.gcLevel).name)}` : ''}</div><h4>${pick.nat ? esc(g.nations[pick.nat].name) : 'A war nobody has seen'}</h4>
      <p>${esc(pick.year.text)}</p><p class="you">${esc(pick.text)}${smallPick ? ' Open <b>Your path</b> on the top bar to choose where you stand.' : ''}</p>${goals ? `<div class="gcgoals">${goals}</div>` : ''}
      ${pick.nat && hasTraits(pick.nat) ? `<div class="gctraits"><h5>What ${esc(theName(pick.nat))} does best</h5>${traitList(pick.nat)}</div>` : ''}
      ${pick.year.turn > 1 ? `<small class="gcwait">The computer plays the first ${pick.year.turn - 1} months of the war first. That takes a few seconds.</small>` : ''}</div></div>
    <h3>Historical scenarios</h3>
    <div class="sides"><button class="sidebtn wide" data-act="menu" data-menu="scenarios"><b>Play a turning point of the war</b><span>Fourteen shorter campaigns, from the Battle of Britain and Barbarossa to D-Day and Operation Bagration. Each starts at a turning point with the other nations following history's plan, or you can sit back and watch history unfold.</span></button></div>
    ${STEPS}</div>
    <footer class="gcfoot"><span class="grow">${pick.nat ? flagIcon(pick.nat, g.nations[pick.nat].color, 30) : ''}<span><b>${pick.nat ? esc(g.nations[pick.nat].name) : 'A war nobody has seen'}</b><small>${esc(pick.year.date)}${pick.nat ? ` · ${esc(levelById(ui.gcLevel).name)}` : ''}</small></span></span><button class="btn go gcgo" data-act="start" data-nat="${pick.nat}" data-from="${pick.year.turn}" data-view="grand" data-level="${pick.nat ? ui.gcLevel : ''}" data-cabinet="${ui.gcCabinet ? 1 : 0}" data-secure="${ui.gcSecure ? 1 : 0}">${pick.nat ? 'Begin the campaign' : 'Watch it unfold'} ▸</button></footer>`;
}

function scenarioCard(): string {
  const aims = (id: string | null, from: number): string => (id && from === 1 ? GOAL_KEYS.map((key) => { const goal = GOALS[id]?.[key]; return goal ? `<span class="sgoal"><i>${key === 'short' ? 'Short campaign' : 'Long campaign'}</i>${esc(goal.text)}</span>` : ''; }).join('') : '');
  const side = (id: string | null, name: string, blurb: string, wide = false, from = 1, view = '') => `<button class="sidebtn${wide ? ' wide' : ''}" data-act="start" data-nat="${id ?? ''}" data-from="${from}" data-view="${view}">${id ? `<b>${flagIcon(id, g.nations[id].color, 26)} ${name}</b>` : `<b>${name}</b>`}<span>${blurb}</span>${aims(id, from)}</button>`;
  return `${startHead('Historical scenarios')}
    <div class="body"><p class="lead"><button class="btn small" data-act="menu" data-menu="grand">◂ Back to the grand campaign</button></p>
    <p class="lead">Each scenario starts at a turning point of the war. The computer plays history up to that day, and the nations you do not play go on following <b>history's plan</b>, so the war around you unfolds as it did unless you change it.</p>
    <h3>September 1939 · The war as it was fought</h3>
    <div class="sides">
      ${side(null, 'Watch history unfold', 'Every nation follows its plan. Press Play and watch Poland, then the West, then the East.', true)}
      ${side('GER', 'Germany', 'On the attack from the first day. The hardest plan to pull off on schedule.')}
      ${side('FRA', 'France', 'The Maginot Line, a large army, and a slow command. Can you keep Paris?')}
      ${side('UK', 'United Kingdom', 'A small army, the largest navy in the world, and convoys to keep alive.')}
      ${side('POL', 'Poland', 'Outnumbered and nearly surrounded. You place your army on the eve of war, then hold as long as you can.')}
    </div>
    <h3>July 1940 · The Battle of Britain</h3>
    <div class="sides">
      ${side(null, 'Watch the Battle of Britain', 'France has fallen. The computer flies history\'s plan: the Channel, the airfields, London, then the night Blitz.', true, 11)}
      ${side('GER', 'Germany', 'Five air fleets and a summer of flying weather. History split its effort and lost. Can you win the sky over Kent?', false, 11)}
      ${side('UK', 'United Kingdom', 'Radar, four fighter groups and the Channel. Keep Fighter Command in being until the autumn.', false, 11)}
    </div>
    <h3>September 1940 · The desert war</h3>
    <div class="sides">
      ${side(null, 'Watch the desert war', 'One road, no water, and a front that swings a thousand kilometres each way: Egypt, Operation Compass, Rommel, Tobruk.', true, 13)}
      ${side('UK', 'United Kingdom', 'Two divisions against an army. Throw the Italians out of Egypt, then decide how far from Alexandria you dare to chase them.', false, 13)}
      ${side('ITA', 'Italy', 'A large army on foot, a long way from Tripoli. Take Egypt before the British tanks find your open flank.', false, 13)}
    </div>
    <h3>October 1940 · Greece and the Balkans</h3>
    <div class="sides">
      ${side(null, 'Watch the Balkans', 'Italy marches into the Greek mountains in the autumn rain. In April the panzers come through Bulgaria, and in May the parachutes open over Crete.', true, 14)}
      ${side('GRE', 'Greece', 'A small army that knows its mountains. Hold the Italians in Epirus all winter, then decide what can be saved when Germany comes.', false, 14)}
      ${side('ITA', 'Italy', 'Mussolini expects Athens in two weeks. You have a mountain wall in front of you, and an army in Libya that needs the same replacements.', false, 14)}
    </div>
    <h3>June 1941 · Barbarossa</h3>
    <div class="sides">
      ${side(null, 'Watch Barbarossa unfold', 'The computer plays the first 21 months, then the invasion of the Soviet Union and the first winter.', true, 22)}
      ${side('GER', 'Germany', 'Three army groups on the frontier. Reach Moscow before the snow, or outrun your supplies trying.', false, 22)}
      ${side('SOV', 'Soviet Union', 'The frontier armies are about to be hit. Trade space for time until the reserves and the winter arrive.', false, 22)}
    </div>
    <h3>May 1942 · Gazala and El Alamein</h3>
    <div class="sides">
      ${side(null, 'Watch the desert in 1942', 'The computer plays the first 32 months, then Gazala, the fall of Tobruk, the stand at El Alamein and the long pursuit back across Libya.', true, 33)}
      ${side('UK', 'United Kingdom', 'Rommel strikes at the Gazala line as you take over, and Tobruk is not the fortress it was. Save the army, hold El Alamein, and strike back when the armour arrives.', false, 33)}
      ${side('ITA', 'Italy', 'Most of the Axis army in Africa is yours, and so is the convoy route that feeds it. Tobruk is in reach. Decide how far past it you dare to go.', false, 33)}
    </div>
    <h3>June 1942 · Case Blue and Stalingrad</h3>
    <div class="sides">
      ${side(null, 'Watch the road to Stalingrad', 'The computer plays the first 33 months, then the drive for the Don, the Volga and the Caucasus oil, and the winter that turned the war.', true, 34)}
      ${side('GER', 'Germany', 'Your tanks are gathered in the south and your allies will hold the Don behind you. The oil and the Volga are both in reach. Decide how much of the army goes into the city.', false, 34)}
      ${side('SOV', 'Soviet Union', 'Give ground all summer and save the army. Hold Stalingrad, watch who is guarding the Don, and strike when the snow comes.', false, 34)}
    </div>
    <h3>November 1942 · Torch and Tunisia</h3>
    <div class="sides">
      ${side(null, 'Watch Torch and Tunisia', 'The computer plays the first 38 months, then the American and British landings in Morocco and Algeria, the race for Tunis, Kasserine, the Mareth Line and the end in Africa.', true, 39)}
      ${side('UK', 'United Kingdom', 'Montgomery is chasing Rommel across Libya, and a second army waits at Gibraltar beside the Americans. Get it ashore in Algeria, try for Tunis before the Germans do, and close the trap from both ends.', false, 39)}
      ${side('ITA', 'Italy', 'Your army is falling back from El Alamein and a new enemy is about to land behind it. Hold Tunisia, bring the army home to it, and keep the sea road to Sicily open for as long as you can.', false, 39)}
    </div>
    <h3>July 1943 · Kursk and the Dnieper</h3>
    <div class="sides">
      ${side(null, 'Watch Kursk and the road to the Dnieper', 'The computer plays the first 46 months, then the last German summer offensive in the east, the Soviet strokes at Orel and Kharkov, the race for the Dnieper and the freeing of Kiev.', true, 47)}
      ${side('GER', 'Germany', 'Your tanks are gathered on both shoulders of the Kursk salient, and the Red Army has had three months to dig in there. Strike or hold them back: either way the weight is about to fall on Orel and Kharkov, and the Dnieper is a long way behind you.', false, 47)}
      ${side('SOV', 'Soviet Union', 'You know where the blow will fall and you have dug in to meet it. Break the panzers on the lines round Kursk, then send the reserve fronts at Orel and Kharkov and do not stop until you are over the Dnieper.', false, 47)}
    </div>
    <h3>July 1943 · Sicily and Italy</h3>
    <div class="sides">
      ${side(null, 'Watch Sicily and Italy', 'The computer plays the first 46 months, then the landing in Sicily, the fall of Mussolini, the Italian armistice, Salerno and Naples, and the German winter line across the mountains south of Rome.', true, 47, 'italy')}
      ${side('UK', 'United Kingdom', 'Two armies wait at Malta and your air force has spent a month over Sicily. Keep the sky, put the Eighth Army ashore, and follow it across the strait before the Germans have dug in across the mountains.', false, 47, 'italy')}
      ${side('ITA', 'Italy', 'Africa is gone, the army has lost heart and the invasion fleet is gathering at Malta. If Sicily falls the King will dismiss Mussolini, and an armistice follows. Hold the island and Italy stays in the war.', false, 47, 'italy')}
    </div>
    <h3>January 1944 · The eastern front, from Leningrad to the Danube</h3>
    <div class="sides">
      ${side(null, 'Watch the Red Army\'s year', 'The computer plays the first 52 months, then the relief of Leningrad, the spring offensive through the mud of the Ukraine, the Crimea, the destruction of Army Group Centre in the summer, Romania and Finland leaving the war, and the road to Budapest.', true, 53, 'east')}
      ${side('SOV', 'Soviet Union', 'You have the men, the guns and the initiative on every part of the front. History freed Leningrad in January, the Ukraine by April, Minsk in July and Bucharest in August. Choose where each blow falls, and do not let the armies you cut off get away.', false, 53, 'east')}
      ${side('GER', 'Germany', 'A front of two thousand kilometres, too few divisions to hold it, and orders to give up nothing. Italy has to be held as well, and the landing in France is coming in the summer. Decide what to give up while it is still yours to give.', false, 53, 'east')}
    </div>
    <h3>January 1944 · Anzio and the road to Rome</h3>
    <div class="sides">
      ${side(null, 'Watch Anzio, Cassino and Rome', 'The computer plays the first 52 months, then the landing at Anzio, the four battles for Monte Cassino, the fall of Rome in June, the pursuit to Florence and the autumn battles on the Gothic Line.', true, 53, 'italy')}
      ${side('UK', 'United Kingdom', 'The Eighth Army holds the Adriatic end of a front that has not moved since October, and the Americans are about to land behind it at Anzio. Monte Cassino bars the one good road to Rome. Decide when to break yourself against it, and when to wait for the spring.', false, 53, 'italy')}
    </div>
    <h3>May 1944 · Overlord and the liberation of France</h3>
    <div class="sides">
      ${side(null, 'Watch D-Day and the drive to the Rhine', 'The computer plays the first 56 months, then the air battle over the beaches, D-Day in June, the breakout and the liberation of Paris in August, Brussels and Antwerp, Market Garden, the West Wall, and the Ardennes offensive in December.', true, 57, 'west')}
      ${side('UK', 'United Kingdom', 'Two corps wait on the quays in Hampshire and the Americans beside you have a beach of their own. Win the sky over Normandy in May, land in June, and hold the German armour on your front while the Americans break out. Then see how far a single road to Arnhem will take you.', false, 57, 'west')}
      ${side('GER', 'Germany', 'Some fifty divisions stand behind the Atlantic Wall and nobody can tell you which beach. The panzers can wait close to the coast or well back from it, not both. And every division kept in France is one the eastern front will not have when the Soviet summer offensive opens.', false, 57, 'west')}
    </div>
    <h3>June 1944 · Operation Bagration</h3>
    <div class="sides">
      ${side(null, 'Watch Bagration', 'The computer plays the first 57 months, then the blow that destroyed Army Group Centre: Vitebsk and Minsk in June, the Bug and the Vistula in July, the Warsaw rising, and Romania changing sides as the summer ends.', true, 58, 'east')}
      ${side('SOV', 'Soviet Union', 'The landing in France is days away and the German tanks have gone south, where they expect you. Four fronts are ready in Byelorussia. Break the fortified places at Vitebsk and Mogilev, close the ring at Minsk, and see how far the summer will carry you.', false, 58, 'east')}
      ${side('GER', 'Germany', 'Army Group Centre holds a great bulge round Minsk, its tanks have been sent south, and the fortified places are to be held to the last man. The Allies are about to land in France. You have a few weeks to decide what can be saved.', false, 58, 'east')}
    </div>
    ${STEPS}</div>`;
}


// ---- The face-off: two armies drawn up against each other, before the battle and after it ----

interface Side {
  /** Said in place of the numbers of men and divisions where they are only an estimate. */
  menText?: string;
  divsText?: string;
  nats: string[];
  /** 'attacking' or 'defending'. */
  role: string;
  /** The senior general, null when the army has none, left out when nobody recorded it. */
  who?: General | null;
  men: number;
  corps: number;
  divs: number;
  cards: string;
  /** A line under the numbers, such as the losses. */
  under?: string;
}

const adjs = (nats: string[]): string => nats.map((n) => g.nations[n].adj).join(' and ');
const initialsOf = (name: string): string => name.replace(/\s*\(.*\)\s*$/, '').split(/\s+/).map((w) => w[0]).filter((ch) => /[A-Z]/.test(ch)).slice(-2).join('');

/** One army in a face-off: its colours, who leads it, what it brings, and its corps as cards. */
function sidePanel(o: Side, cls: string): string {
  const n = g.nations[o.nats[0]];
  const head = o.who
    ? `<span class="seal" style="background:${n.color}">${esc(initialsOf(o.who.name))}</span><span class="grow"><b>${esc(o.who.name)}</b><small>${esc(adjs(o.nats))} army ${o.role} · level ${o.who.level} general</small>${o.who.traits.length ? `<span class="tags">${traitTags(o.who)}</span>` : ''}</span>`
    : `<span class="seal none" style="background:${n.color}"></span><span class="grow"><b>${esc(adjs(o.nats))} army</b><small>${o.role[0].toUpperCase() + o.role.slice(1)}${o.who === null && o.corps ? ' · no general in command' : ''}</small></span>`;
  return `<div class="fside ${cls}" style="--side:${n.color}"><div class="fhead">${head}</div>
    <div class="fnum"><span><b>${o.menText ?? num(o.men)}</b><small>men</small></span><span><b>${o.divsText ?? num(o.divs)}</b><small>division${o.divs === 1 && !o.divsText ? '' : 's'}</small></span><span><b>${o.corps}</b><small>corps</small></span></div>${o.under ?? ''}
    <div class="fcards">${o.cards}</div></div>`;
}

/** A corps in a face-off. One the player may send or hold back is a button. */
function battleCard(c: Corps, opts: { act?: boolean; on?: boolean; note?: string; from?: boolean } = {}): string {
  const el = opts.act ? 'button' : 'div';
  const gen = generalOf(g, c);
  const where = opts.from ? ` · ${esc(g.regions[c.reg].name)}` : gen ? ` · ${esc(surname(gen.name))}` : '';
  return `<${el} class="ucard${opts.act ? (opts.on ? ' pick' : ' out') : ''}" ${opts.act ? `data-act="atkpick" data-id="${c.id}"` : ''} title="${esc(c.name)}${gen ? ` · ${esc(gen.name)}` : ''} · ${num(men(c))} men${opts.from ? ` · from ${esc(g.regions[c.reg].name)}, ${c.ap} move${c.ap === 1 ? '' : 's'} left` : ''}">
    <span class="utop">${icon(c)}${gen ? `<span class="lvl" title="${esc(gen.name)}, level ${gen.level} general">${gen.level}</span>` : ''}</span><b>${esc(c.name)}</b>
    <span class="um">${c.joint ? `${chip(c.nat)} ${esc(g.nations[c.nat].adj)} · ` : ''}${c.divs} ${KIND[c.kind].label.toLowerCase()} div${c.divs > 1 ? 's' : ''}${where}</span>
    <span class="meter" title="Strength ${Math.round(c.str * 100)}%"><i class="${c.str < 0.5 ? 'low' : ''}" style="width:${Math.round(c.str * 100)}%"></i></span>
    <span class="meter" title="Rest ${Math.round(c.org * 100)}%"><i class="org${c.org < 0.4 ? ' low' : ''}" style="width:${Math.round(c.org * 100)}%"></i></span>${c.mor !== undefined ? `<span class="meter" title="Morale ${Math.round(c.mor * 100)}%: ${esc(moodOf(g, c).moraleWord.toLowerCase())}"><i class="mor${c.mor < 0.4 ? ' low' : ''}" style="width:${Math.round(c.mor * 100)}%"></i></span>` : ''}${opts.note ? `<span class="tags"><span class="tag bad">${esc(opts.note)}</span></span>` : ''}</${el}>`;
}

const FATE: Record<string, string> = { '': 'held its ground', retreated: 'retreated', surrendered: 'surrendered', destroyed: 'destroyed', advanced: 'advanced', evacuated: 'evacuated by sea', yielded: 'gave ground under orders', sheltered: 'fell back into the town' };

/** A corps after the battle: what it lost and what became of it. */
function fateCard(u: BattleReport['attackers'][number], won: boolean): string {
  const left = u.men > 0 ? Math.max(0, 1 - u.lost / u.men) : 0;
  const gone = u.fate === 'surrendered' || u.fate === 'destroyed';
  const fate = u.fate === '' && !won ? 'fell back' : FATE[u.fate];
  return `<div class="ucard${gone ? ' gone' : ''}" title="${esc(u.name)} · ${num(u.men)} men, lost ${num(u.lost)}">
    <span class="utop"><svg width="34" height="24" viewBox="-1 -1 26 19" class="ctr"><rect class="box" x="0" y="0" width="24" height="15" rx="1.5" fill="${g.nations[u.nat].color}"/>${symbol(u.kind, 24, 15)}</svg></span><b>${esc(u.name)}</b>
    <span class="um">${u.divs} ${KIND[u.kind].label.toLowerCase()} div${u.divs > 1 ? 's' : ''} · lost ${num(u.lost)}</span>
    <span class="meter" title="${Math.round(left * 100)}% of its men left"><i class="${left < 0.5 ? 'low' : ''}" style="width:${Math.round(left * 100)}%"></i></span>
    <span class="tags"><span class="tag ${gone || u.fate === 'retreated' ? 'bad' : u.fate === 'advanced' ? 'good' : ''}">${fate}</span></span></div>`;
}

/** An attack thrown back from dug-in troops still wears their works down (the grand campaign): said on its report, to whichever side reads it. */
function wornNote(r: BattleReport): string {
  if (!r.worn) return '';
  const full = DEFENCE.entrench.months, left = r.works ?? 0;
  const took = r.worn >= 0.95 && r.worn < 1.05 ? 'a month' : `${r.worn} months`;
  const now = left < 0.05 ? 'nothing is left of them' : `they stand at ${left} of ${full} months`;
  const ours = r.defNat === g.human;
  const paid = lostIn(r.attackers, r.attLost);
  return `<div class="note wornnote"><b>${ours ? 'The line held, but its works are worn.' : 'Thrown back, but not for nothing.'}</b> The attack took ${took} off the ${ours ? '' : "defenders' "}works at ${esc(r.regionName)}${ours ? '' : `, at the price of ${num(paid.lost)} men${paid.pct ? `, ${paid.pct}% of those sent in` : ''}`}: ${now}. ${ours ? `Your troops there ${pressedPace()} this month. A quiet month builds the works up again; another attack takes off more.` : `The defenders ${pressedPace()} this month, so the next blow meets less. Guns in the attack wear works down faster.`}</div>`;
}
/** What one side lost in a battle, in men and as a share of those engaged: the fight itself and whoever laid down his arms after it. */
function lostIn(us: BattleReport['attackers'], fought: number): { lost: number; pct: number } {
  const total = us.reduce((s, u) => s + u.men, 0);
  const lost = Math.max(fought, us.reduce((s, u) => s + u.lost, 0));
  return { lost, pct: total > 0 && lost > 0 ? Math.max(1, Math.round(100 * Math.min(1, lost / total))) : 0 };
}
/** The price of a battle in one sentence, to whichever side reads its report. */
function priceLine(r: BattleReport): string {
  if (r.yielded) return '';
  const a = lostIn(r.attackers, r.attLost), d = lostIn(r.defenders, r.defLost);
  const of = (x: { lost: number; pct: number }): string => `${num(x.lost)}${x.pct ? ` (${x.pct}%)` : ''}`;
  if (g.human && (r.attNat === g.human || r.attackers.some((u) => u.nat === g.human))) return `It cost you ${num(a.lost)} men${a.pct ? `, ${a.pct}% of those sent in,` : ''} against ${of(d)} of theirs.`;
  if (g.human && r.defenders.some((u) => u.nat === g.human)) return `It cost the attackers ${num(a.lost)} men${a.pct ? `, ${a.pct}% of those sent in,` : ''} against ${of(d)} of yours.`;
  return `The attackers lost ${of(a)} men, the defenders ${of(d)}.`;
}
/** A chance as a whole percentage that never rounds a near thing up to a sure one, or a faint hope down to none. */
const oddsPct = (p: number): number => (p >= 0.9995 ? 100 : p <= 0.0005 ? 0 : Math.min(99, Math.max(1, Math.round(p * 100))));
/** The lines of a Battle Ticket that matter most: on a narrow screen the ticket opens folded to these, with a button for the rest. */
const TICKET_KEY = new Set(['Forces', 'Troops', 'Dug in', 'Supply', 'Earlier attacks', 'Weight of attack']);
const ticketBlock = (title: string, lines: BattleReport['lines']): string => {
  const more = lines.filter((l) => !TICKET_KEY.has(l.label)).length;
  const fold = more > 0 && !ui.ticketAll;
  return `<h3>${title}</h3><table class="ticket${fold ? ' fold' : ''}">${lines.map((l) => `<tr${TICKET_KEY.has(l.label) ? '' : ' class="tx"'}><td>${esc(l.label)}</td><td>${esc(l.value)}</td><td class="fx">${esc(l.effect ?? '')}</td></tr>`).join('')}</table>${fold ? `<button class="btn small ticketall" data-act="ticketall">Show all ${lines.length} lines</button>` : ''}`;
};
/** Men to the nearest hundred, for a forecast that is not exact. */
const roughMen = (n: number): string => (n < 100 ? 'under 100' : num(Math.round(n / 100) * 100));
/** A chance or a band of chances in words: one figure where the staff can give one, its best guess with the limits where the band is too wide to plan on. */
const chanceWords = (look: Outlook): string => (look.exact || Math.round(look.chanceLo * 100) === Math.round(look.chanceHi * 100) ? `about ${oddsPct(look.chanceLo)}%`
  : look.wide ? `a best guess of ${oddsPct(look.guess)}%, anywhere from ${oddsPct(look.chanceLo)} to ${oddsPct(look.chanceHi)}%` : `${oddsPct(look.chanceLo)} to ${oddsPct(look.chanceHi)}%`);
/** The bill before the order: what the fight is likely to cost each side in men, if it carries the ground and if it is thrown back. */
function costBlock(look: Outlook): string {
  const row = (c: Outlook['cost']['won'], head: string): string => {
    if (!c) return '';
    const theirs = c.theirs !== null ? `<b>${roughMen(c.theirs)}</b> men` : `<b>${Math.max(1, Math.round(c.share * 100))}%</b> of their men`;
    return `<div class="costrow"><b>${head}:</b><span>you lose about <b>${roughMen(c.ours)}</b> men, they lose about ${theirs}</span></div>`;
  };
  const won = row(look.cost.won, look.cost.lost ? 'If it carries' : 'It should carry');
  const lost = row(look.cost.lost, look.cost.won ? 'If it is thrown back' : 'Thrown back, as it will be');
  if (!won && !lost) return '';
  return `<div class="costline" title="Each side's losses in the fight itself, at the middle of the luck that gives that result. Prisoners, and men lost in a retreat with nowhere to go, come on top of a victory.">${won}${lost}</div>`;
}

function reportCard(r: BattleReport): string {
  const v = verdictFor(r);
  const side = (us: BattleReport['attackers'], fought: number, role: string, won: boolean): Side => {
    const total = us.reduce((s, u) => s + u.men, 0);
    // What the cards beneath add up to: the men lost in the fight, and those who laid down their arms after it.
    const gave = us.reduce((s, u) => s + (u.fate === 'surrendered' || u.fate === 'destroyed' ? u.lost : 0), 0);
    const lost = Math.max(fought, us.reduce((s, u) => s + u.lost, 0));
    const share = total > 0 ? Math.min(1, lost / total) : 0;
    const line = gave > 0 && gave < lost ? `Lost ${num(lost)} men, ${Math.round(share * 100)}% of those engaged: ${num(lost - gave)} in the fight, ${num(gave)} taken or scattered after it`
      : gave > 0 ? `Lost ${num(lost)} men, ${Math.round(share * 100)}% of those engaged: all taken or scattered` : `Lost ${num(lost)} men, ${Math.round(share * 100)}% of those engaged`;
    return {
      // A joint army attacks under the flag of the nation that commands it.
      nats: [...new Set(role === 'attacking' ? [r.attNat, ...us.map((u) => u.nat)] : us.map((u) => u.nat))], role, men: total, corps: us.filter((u) => !u.guard).length, divs: us.reduce((s, u) => s + u.divs, 0),
      under: `<div class="floss"><span class="meter"><i class="low" style="width:${Math.round(share * 100)}%"></i></span><small>${line}</small></div>`,
      cards: us.map((u) => fateCard(u, won)).join(''),
    };
  };
  const share = r.aPower / Math.max(1e-9, r.aPower + r.dPower);
  // What the ground just won is worth, while the player still holds it.
  const won = r.taken && r.turn === g.turn && g.human === r.attNat ? prize(g, r.region) : null;
  const spoils = won && (won.kind === 'yours' || won.kind === 'ally') ? `<div class="prizeline won">${townIcon(townIn(g, r.region)!, 30)}<span class="grow"><b>${won.kind === 'yours' ? 'The prize is yours.' : 'Freed.'}</b> ${esc(won.line)}</span></div>` : '';
  // The grand campaign: a fight for a town, or for the country round one, says which it was and what follows.
  const place = g.regions[r.region], town = esc(townName(place));
  const kick = r.yielded ? 'Ground given up' : r.town ? 'Assault on a town' : r.sally ? 'Sortie' : r.relief ? 'Relief of a siege' : 'Battle result';
  const title = r.yielded ? `${esc(r.regionName)} given up` : r.town ? `Assault on ${town}` : r.sally ? `Sortie from ${town}` : r.relief ? `Relief of ${town}` : `Battle of ${esc(r.regionName)}`;
  const told = r.town ? (r.taken ? `${town} was stormed: ${esc(r.regionName)} has fallen.` : `The assault on ${town} was thrown back. The siege goes on.`)
    : r.sally ? (r.taken ? `The garrison of ${town} drove off the army round it. The siege is raised.` : `The sortie from ${town} was beaten back inside the walls. The siege goes on.`)
    : r.relief ? (r.taken ? `The army round ${town} was driven off. The siege is raised.` : `The attempt to relieve ${town} failed. The siege goes on.`)
    : r.siege ? `The country round ${town} was won, but the town holds out. ${esc(r.regionName)} does not change hands until the town is stormed, starved out or opens its gates.`
    : r.gates ? `The army outside was beaten, and ${town} opened its gates without a fight. ${esc(r.regionName)} has fallen.`
    : r.taken ? `${esc(r.regionName)} was taken.` : `The attack on ${esc(r.regionName)} was thrown back.`;
  const mineNow = !!r.siege && r.turn === g.turn && humanUp(g) && (r.attNat === g.human || r.attackers.some((u) => u.nat === g.human));
  const stormWhy = mineNow ? whyNotStormTown(g, r.region) : 'no';
  const next = r.siege && mineNow ? `<div class="note siegenote">${ringIcon(g.nations[r.attNat].color, 26)}<span><b>${town} is under siege.</b> Your army has closed round it and marches no further this month. You can storm it at once, off the march, or wait: next month the assault goes in at full weight, and anything your bombers, ships and guns break down meanwhile makes it easier. Keep the ring closed and hunger will do the work in the end.</span></div>`
    : r.siege ? `<div class="note siegenote">${ringIcon(g.nations[r.attNat].color, 26)}<span><b>${town} is under siege.</b> ${g.human && allied(g, g.human, place.ctrl) ? 'To raise it, attack the army round the town from a neighbouring province, or sally out with the garrison.' : 'The town must be stormed or starved out before the province changes hands.'}</span></div>` : '';
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">${kick} · ${date(r.turn)}, week ${r.week}</div><h2>${title}</h2></header>
    <div class="body">
      <div class="verdict ${v.cls}">${v.text}</div>
      <div class="sub">${r.yielded ? `The defenders were under orders to give ground and slipped away before the blow fell. ${esc(r.regionName)} was given up without a fight. The odds against them were ${r.odds.toFixed(2)} to 1.` : `${told} ${priceLine(r)} Final odds ${r.odds.toFixed(2)} to 1${r.prisoners ? ` · ${num(r.prisoners)} prisoners` : ''}.`}</div>
      ${spoils}${next}${wornNote(r)}
      <div class="faceoff">${sidePanel(side(r.attackers, r.attLost, 'attacking', r.taken), 'att')}<div class="fmid"><span class="vsmark">VS</span></div>${sidePanel(side(r.defenders, r.defLost, 'defending', !r.taken), 'def')}</div>
      <div class="balance"><div class="btrack"><div class="bal"><i style="width:${Math.round(share * 100)}%;background:${g.nations[r.attNat].color}"></i><i style="flex:1;background:${g.nations[r.defNat].color}"></i></div><span class="need" style="left:${(100 * 1.2 / 2.2).toFixed(1)}%" title="What it takes to carry the ground: roughly 1.2 to 1"></span></div>
        <div class="ends"><span>${esc(g.nations[r.attNat].adj)} strength on the day</span><span>${esc(g.nations[r.defNat].adj)} strength on the day</span></div></div>
      ${ticketBlock('What the campaign map fed into the battle', r.lines)}
      ${r.yielded ? '<div class="note" style="margin-top:14px"><b>No battle was fought.</b> Troops ordered to fall back keep their strength and lose some cohesion and the ground. They stand if they are caught again before their own month comes round.</div>' : '<div class="note" style="margin-top:14px"><b>Auto-resolved.</b> In the full game you can choose to fight this battle yourself in 3D. The same ticket goes in and the same kind of result comes back, so the campaign never needs to know which one happened.</div>'}
    </div><footer><button class="btn" data-act="showmap" data-id="${r.region}">Show on map</button>${mineNow && !stormWhy ? `<button class="btn" data-act="storm" data-id="${r.region}" title="See the odds of an assault on the town before anything is ordered.">Storm the town</button>` : ''}<button class="btn go" data-act="close">Continue</button></footer>`;
}

/** What is at stake in a battle, said in one line on the card that plans it. */
function prizeStrip(target: Region): string {
  const p = prize(g, target.id);
  const t = townIn(g, target.id);
  if (!p || !t || p.kind === 'yours' || p.kind === 'ally') return '';
  const chips = p.items.filter((it) => it.key !== 'income' && it.key !== 'fort').map((it) => `<span class="pzchip">${PRIZE_SIGN[it.key] ?? ''}${esc(it.label)}</span>`).join('');
  return `<div class="prizeline">${townIcon(t, 30)}<span class="grow"><b>The prize.</b> ${esc(p.line)}${chips ? `<span class="pzchips">${chips}</span>` : ''}</span><span class="cost">${p.kind === 'taken' ? `+${num(p.income)}` : `−${num(p.incomeNow)}`}<small>${p.kind === 'taken' ? 'a month to you' : 'a month to them'}</small></span></div>`;
}

function attackCard(): string {
  const m = ui.modal!;
  const target = g.regions[m.target];
  const me = g.human!;
  const cands = candidates(g, target);
  // An ally's corps in a joint army goes in only against an enemy its own country is at war with.
  const chosen = jointForce(g, limitAttackers(g, jointForce(g, cands.filter((c) => m.chosen.has(c.id)), target), target), target);
  const inSet = new Set(chosen.map((c) => c.id));
  const a = previewAttack(g, chosen, target);
  const look = a ? attackOutlook(g, chosen, target) : null;
  // Everyone the roads will carry, if more stand in reach than were picked: what the attack could be.
  const everyone = jointForce(g, limitAttackers(g, jointForce(g, cands, target), target), target);
  const extra = everyone.filter((c) => !inSet.has(c.id)).length;
  const massed = extra ? attackOutlook(g, everyone, target) : null;
  // The card opened on everyone in reach because that beat the army picked: said, with the way back to the pick.
  const mine: Corps[] = m.picked ? cands.filter((c) => (m.picked as number[]).includes(c.id)) : [];
  const mineLook = mine.length ? attackOutlook(g, mine, target) : null;
  const openNote = m.picked && look && !extra && chosen.length > mine.length
    ? `<div class="note massnote opennote"><span><b>The staff has brought in every corps the roads will carry.</b> ${chosen.length} corps go in at ${look.odds.toFixed(2)} to 1${mineLook ? `, where the ${mine.length === 1 ? 'corps' : `${mine.length} corps`} you picked ${mine.length === 1 ? 'reads' : 'read'} ${mineLook.odds.toFixed(2)} to 1` : ''}. Click a corps to hold it back.</span><button class="btn small" data-act="atkmine" title="Send only the corps that were picked when the attack was ordered.">Only the corps I picked</button></div>` : '';
  const massNote = openNote ? openNote : massed && (!look || massed.odds > look.odds * 1.02)
    ? `<div class="note massnote"><span><b>${extra === 1 ? 'One more corps stands' : `${extra} more corps stand`} within reach.</b> Send everyone the roads will carry, ${everyone.length} corps in all, and the odds ${look ? `go from ${look.odds.toFixed(2)} to ${massed.odds.toFixed(2)} to 1` : `are ${massed.odds.toFixed(2)} to 1`}: ${chanceWords(massed)} to take ${esc(target.name)}.</span><button class="btn small go" data-act="atkall" title="Pick every corps in reach. The staff sends the strongest the roads will carry, and any guns behind them.">Send everyone who can reach</button></div>` : '';
  const foes = a ? a.defenders : hostileUnits(g, target, me);
  // What is known of the defence: everything, or the staff's estimate of it.
  const sure = allKnown(foes);
  const foeEst = sure ? null : armyAt(g, target.id, foes[0]?.nat ?? target.ctrl)?.est ?? null;
  const why = (c: Corps): string => (c.joint && !hostileUnits(g, target, c.nat).length ? `${g.nations[c.nat].name} is not at war with them` : barred(g, c, target) ? 'Too steep for tanks' : blocked(g, c, target) ? 'Bridges down' : m.chosen.has(c.id) && !inSet.has(c.id) ? (g.nations[c.nat].maxAttack && chosen.length >= g.nations[c.nat].maxAttack ? 'Staff at its limit' : 'No room on the roads') : '');
  const ours: Side = {
    nats: [me, ...new Set(chosen.filter((c) => c.nat !== me).map((c) => c.nat))], role: 'attacking', who: leaderOf(chosen), men: chosen.reduce((s, c) => s + men(c), 0), corps: chosen.length, divs: rollDivs(chosen),
    cards: cands.map((c) => battleCard(c, { act: true, on: inSet.has(c.id), note: why(c), from: true })).join(''),
    under: `<small class="fhelp">${cands.length > 1 ? 'Click a corps to send it in or hold it back.' : 'The one corps in reach.'}</small>`,
  };
  const theirs: Side = {
    nats: [...new Set(foes.map((c) => c.nat))], role: 'defending', who: leaderOf(foes), men: foes.reduce((s, c) => s + men(c), 0), corps: foes.length, divs: rollDivs(foes),
    ...(sure ? {} : { menText: foeEst ? span(Math.round(foeEst.lo * 15) * 1000, Math.round(foeEst.hi * 15) * 1000) : '?', divsText: foeEst ? span(Math.max(1, Math.round(foeEst.lo)), Math.max(1, Math.round(foeEst.hi))) : '?' }),
    cards: foes.map((c) => (kn(c) === 'exact' ? battleCard(c) : foeCard(c))).join(''),
    under: target.fort ? `<small class="fhelp">Dug in behind level ${target.fort.lv} fortifications${target.fort.vs.includes(me) ? '' : ', which do not face you'}.</small>` : '',
  };
  let body = '';
  if (a && look && !look.exact) {
    // The defence is partly a guess: the odds are a band, and the bar shows where in it the truth may lie.
    const at = (odds: number): number => (100 * odds) / (1 + odds);
    const p = look.chance;
    const word = look.chanceLo >= 0.95 ? 'Victory looks certain' : look.chanceHi <= 0 ? 'This attack looks hopeless' : p >= 0.6 ? 'The staff thinks the odds favour you' : p >= 0.3 ? 'The staff cannot call it' : 'The staff thinks it a long shot';
    body = `<div class="balance"><div class="bhead"><span class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}">${word}</span><span class="sub">Odds between ${look.lo.toFixed(1)} and ${look.hi.toFixed(1)} to 1 · ${chanceWords(look)} chance to take ${esc(target.name)}</span></div>
      <div class="btrack"><div class="bal"><i style="width:${at(look.odds).toFixed(1)}%;background:${g.nations[me].color}"></i><i style="flex:1;background:${g.nations[foes[0].nat].color}"></i></div><span class="doubt" style="left:${at(look.lo).toFixed(1)}%;width:${Math.max(1, at(look.hi) - at(look.lo)).toFixed(1)}%" title="The balance lies somewhere in this band"></span><span class="need" style="left:${(100 * look.even / (1 + look.even)).toFixed(1)}%" title="An even chance: roughly ${look.even.toFixed(1)} to 1${a.prepared > 0 ? ', against a line that is dug in and fed' : ''}"></span></div>
      <div class="ends"><span>Your strength on the day</span><span>The white mark is an even chance; the hatched band is what the staff cannot see</span><span>Theirs</span></div></div>
      ${costBlock(look)}
      <div class="note fogged" style="margin:10px 0 2px"><b>The defence is not fully known.</b> ${esc(look.note)}${look.wide ? ` <b>${esc(look.narrow)}</b>` : ''}${scoutButton(target)}</div>
      ${ticketBlock('What the campaign map feeds into this battle', look.lines)}`;
  } else if (a) {
    const p = chanceOn(a.odds, a.dice);
    const share = a.A / (a.A + a.D);
    const word = p >= 0.95 ? 'Victory is all but certain' : p >= 0.6 ? 'The odds favour you' : p >= 0.3 ? 'It could go either way' : p > 0 ? 'A long shot' : 'This attack will fail';
    body = `<div class="balance"><div class="bhead"><span class="verdict ${p >= 0.6 ? 'win' : p >= 0.3 ? '' : 'loss'}">${word}</span><span class="sub">Odds ${a.odds.toFixed(2)} to 1 · about ${oddsPct(p)}% chance to take ${esc(target.name)}</span></div>
      <div class="btrack"><div class="bal"><i style="width:${Math.round(share * 100)}%;background:${g.nations[me].color}"></i><i style="flex:1;background:${g.nations[foes[0].nat].color}"></i></div><span class="need" style="left:${(100 * evenOdds(a.dice) / (1 + evenOdds(a.dice))).toFixed(1)}%" title="An even chance: roughly ${evenOdds(a.dice).toFixed(1)} to 1${a.prepared > 0 ? ', against a line that is dug in and fed' : ''}"></span></div>
      <div class="ends"><span>Your strength on the day</span><span>The white mark is an even chance: about ${evenOdds(a.dice).toFixed(1)} to 1${a.prepared > 0 ? ' against prepared positions' : ''}</span><span>Theirs</span></div></div>
      ${look ? costBlock(look) : ''}
      ${ticketBlock('What the campaign map feeds into this battle', a.lines)}`;
  } else body = cands.length && cands.every((c) => barred(g, c, target))
    ? `<div class="empty">${esc(target.name)} is too steep for tanks. Bring infantry or mountain troops up beside it to attack.</div>`
    : cands.length && cands.every((c) => blocked(g, c, target))
    ? `<div class="empty">${esc(whyBlocked(g, cands[0], target) ?? '')} Infantry can still cross in boats.</div>`
    : '<div class="empty">Choose at least one corps to attack with.</div>';
  const ground = `${target.terrain[0].toUpperCase() + target.terrain.slice(1)}${steep(g, target) ? ', too steep for tanks' : ''}`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The armies meet · ${date(g.turn)}</div><h2>Battle for ${esc(target.name)}</h2></header>
    <div class="body"><div class="faceoff">${sidePanel(ours, 'att')}<div class="fmid"><span class="vsmark">VS</span><small>${esc(ground)}</small></div>${sidePanel(theirs, 'def')}</div>${massNote}${prizeStrip(target)}${a ? softenRow(target, chosen) : ''}${body}</div>
    <footer><button class="btn" data-act="close" title="Call the attack off. Nothing is spent.">Withdraw</button><button class="btn" disabled title="Fighting a battle yourself in real time comes with the 3D build of the game. Until then every battle is settled from the Battle Ticket.">Fight it yourself · 3D build</button><button class="btn go" data-act="attack" ${a ? '' : 'disabled'} title="Settle the battle at once from the Battle Ticket">Auto-resolve</button></footer>`;
}

/** The news that changes the war, a declaration or a fall, set out as a card of its own. */
function eventCard(h: Game['headlines'][number]): string {
  const n = h.nat ? g.nations[h.nat] : undefined;
  if (h.kind === 'honour') {
    const a = h.award ? awardOf(g, h.award) : undefined;
    const mine = !!a && a.nat === g.human;
    return `<div class="evcard honour" style="--side:${n?.color ?? 'var(--brass)'}"><span class="ek">${mine ? 'Honour' : `${esc(n?.adj ?? '')} honour`} · ${date(h.turn).split(' ')[0]}${a ? ` ${ribbonOf(a)}` : ''}</span><p>${esc(h.text)}</p>${a && mine ? `<button class="btn small" data-act="award" data-id="${a.id}">${a.cash || a.kind === 'corps' && !unitOf(g, a.kind, a.unit) ? 'Read the citation' : 'Read the citation and decide'}</button>` : h.region !== undefined ? `<button class="btn small" data-act="showmap" data-id="${h.region}">Show on map</button>` : ''}</div>`;
  }
  return `<div class="evcard ${h.kind}" style="--side:${n?.color ?? 'var(--brass)'}"><span class="ek">${h.kind === 'war' ? 'War' : h.kind === 'goal' ? 'Campaign goal' : 'Fallen'} · ${date(h.turn).split(' ')[0]}, week ${h.week}</span><p>${esc(h.text)}</p>${h.region !== undefined ? `<button class="btn small" data-act="showmap" data-id="${h.region}">Show on map</button>` : ''}</div>`;
}

/** A letter from the front, set apart from the news: somebody wrote it by hand. */
function letterBlock(h: Game['headlines'][number]): string {
  return `<blockquote class="letter"><p>${esc(h.text)}</p><cite>${esc(h.by ?? 'A soldier')}${h.unit !== undefined && h.unit >= 0 && g.corps.some((c) => c.id === h.unit) ? ` <button class="link" data-act="record" data-type="corps" data-id="${h.unit}">Record ›</button>` : ''}</cite></blockquote>`;
}

function newsCard(from: number, repFrom: number, seaFrom: number, airFrom: number): string {
  const hs = g.headlines.slice(from);
  // The player's own honours are cards; another army's are a line of news.
  const cited = (h: Game['headlines'][number]): boolean => h.kind === 'honour' && h.nat === g.human;
  const big = hs.filter((h) => h.kind === 'war' || h.kind === 'fall' || h.kind === 'goal' || cited(h));
  const letters = hs.filter((h) => h.kind === 'letter');
  const rest = hs.filter((h) => !big.includes(h) && h.kind !== 'letter');
  const me = g.human;
  // What waits behind the dispatches: nations fallen or freed this month, told on a card of their own.
  const due = me && !passing ? g.passings.slice(ui.seenPass) : [];
  const waiting = !due.length ? '' : due.length === 1 ? `${natName(due[0].nat)} ${due[0].kind === 'freed' ? 'is free again' : 'has fallen'}` : `${due.length} nations fallen or freed`;
  const reps = g.reports.slice(repFrom).filter((r) => me && (r.attNat === me || r.attackers.some((u) => u.nat === me) || r.defenders.some((u) => u.nat === me)));
  const seas = g.seaReports.slice(seaFrom).filter((r) => me && (r.att.some((u) => u.nat === me) || r.def.some((u) => u.nat === me)));
  const airs = g.airReports.slice(airFrom).filter((r) => me && (r.attNat === me || r.defNat === me || r.att.some((u) => u.nat === me) || r.def.some((u) => u.nat === me)));
  // Coasts shelled from the sea this month, by the player's ships or on the player's ground.
  const shores = !me || ui.shoreFrom < 0 ? [] : g.shoreReports.slice(ui.shoreFrom).filter((r) => r.turn >= g.turn - 1 && (r.nat === me || r.defNat === me || g.regions[r.region].units.some((c) => c.nat === me)));
  const all = g.reports.length - repFrom + (g.seaReports.length - seaFrom) + (g.airReports.length - airFrom);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Dispatches</div><h2>${date(g.turn)}</h2></header>
    <div class="body">${big.length ? `<div class="evcards">${big.map(eventCard).join('')}</div>` : ''}${rest.length ? `<div class="reel">${rest.map((h) => `<p class="${h.kind}"><span class="wk">${date(h.turn).split(' ')[0]} wk ${h.week}</span>${esc(h.text)}</p>`).join('')}</div>` : hs.length ? '' : '<p class="lead">A quiet month. No news of note.</p>'}
    ${letters.length ? `<h3>Letters from the front</h3>${letters.map(letterBlock).join('')}` : ''}
    ${reps.length || seas.length || airs.length || shores.length ? `<h3>Battles your forces fought</h3>${airs.map(airBrow).join('')}${shores.map(shoreRow).join('')}${seas.map(seaRow).join('')}${reps.map((r) => { const v = verdictFor(r); return `<button class="brow" data-act="report" data-id="${r.id}"><b>${esc(r.regionName)}</b><span class="res ${v.cls}">${v.text}</span><span>${date(r.turn)}, week ${r.week}</span></button>`; }).join('')}` : ''}
    ${all ? `<p class="sub" style="margin-top:10px">${all} battle${all > 1 ? 's were' : ' was'} fought across the map. They are filed under Battles.</p>` : ''}</div>
    <footer>${waiting ? `<small class="nextup">Next: ${esc(waiting)}</small>` : ''}<button class="btn go" data-act="close">${waiting ? 'Next ▸' : me && humanUp(g) ? 'Give orders' : 'Continue'}</button></footer>`;
}

// ---- Oil and supplies: the gauges on the top bar, the books, and the shipping lanes on the map ----

/** An amount of oil or supplies: whole numbers once they are big enough to read that way. */
const amt = (x: number): string => (Math.abs(x) >= 9.95 ? num(x) : (Math.round(x * 10) / 10).toString());
const signedAmt = (x: number): string => `${x < -0.04 ? '−' : '+'}${amt(Math.abs(x))}`;
const monthsWordOf = (m: number): string => { const k = Math.max(1, Math.round(m)); return `${k} month${k === 1 ? '' : 's'}`; };
const STANDING: Record<ReturnType<typeof depotState>, string> = { ample: 'Ample', steady: 'Steady', tightening: 'Running down', short: 'Short', dry: 'Run dry' };

/** One of the two gauges beside the treasury: what is in the depots, and whether it is rising, falling or gone. */
function gauge(b: Book): string {
  const name = GOOD_NAME[b.good];
  const short = b.met < 0.995, falling = b.net < -0.04, low = !short && falling && b.months <= 6;
  const line = short ? `${Math.round(b.met * 100)}% of need met` : falling && b.months < 36 ? `${monthsWordOf(b.months)} left` : `${signedAmt(b.net)} a month`;
  const tip = short
    ? `The depots are empty and only ${Math.round(b.met * 100)}% of the need came in this month. ${b.effects.join('. ')}. Click to see why.`
    : `${amt(b.made)} comes in each month and ${amt(b.need)} goes out. ${falling ? `At this rate the depots are empty in ${monthsWordOf(b.months)}. ` : ''}Click to see where it comes from.`;
  return `<button class="purse store ${b.good}${short ? ' debt' : low ? ' low' : ''}" data-act="stores" data-good="${b.good}" title="${esc(tip)}"><span class="pl">${name}</span><b>${num(Math.floor(b.stock + 1e-6))}</b><i class="${short || falling ? 'down' : 'up'}">${line}</i></button>`;
}

const menShort = (men: number): string => (men < 500 ? '0' : men >= 999500 ? `${(men / 1e6).toFixed(men >= 9.95e6 ? 0 : 1).replace(/\.0$/, '')}m` : `${Math.round(men / 1000)}k`);

/** The reserve of men beside the stocks on the top bar (the grand campaign): what stands in it and the class that comes of age each month. Click for the ledger. */
function menGauge(me: string): string {
  const v = manpowerOf(g, me);
  if (!v) return '';
  const spent = v.word === 'Spent', short = v.word === 'Short';
  const tip = `${v.line} ${menWords(v.classMen)} men come of age each month${v.home < 0.995 ? `, from the ${Math.round(v.home * 100)}% of the homeland you still hold` : ''}. Replacements and every new corps are drawn from the reserve. Click to see the count.`;
  return `<button class="purse store men${spent ? ' debt' : short ? ' low' : ''}" data-act="ledger" data-part="men" title="${esc(tip)}"><span class="pl">Men</span><b>${menShort(v.men)}</b><i class="${spent || short ? 'down' : 'up'}">${spent ? 'reserve spent' : `+${menShort(v.classMen)} a month`}</i></button>`;
}

/** The player's two orders over the call-up, in the ledger: the next class early, and the standing call-up held or let run. */
function menOrders(nat: string): string {
  const c = nat === g.human ? callUpOf(g) : null;
  if (!c) return '';
  const e = c.early, up = humanUp(g);
  const early = ui.modal?.ask === 'early' && !e.why
    ? `<div class="menorder asking"><div class="note warn"><b>Call up the next class early?</b> About ${menWords(e.men)} men join the reserve at once, ${e.months === 1 ? 'a month' : `${e.months} months`} of classes together. No class then comes of age for ${e.gap === 1 ? 'a month' : `${e.gap} months`}, because those boys are with the colours already. They are the same men, sooner, and it cannot be undone.</div><div class="acts"><button class="btn small go" data-act="menearly" data-sure="1">Yes, call them up</button><button class="btn small" data-act="govkeep">Not now</button></div></div>`
    : `<div class="menorder"><span class="grow"><b>The next class, early</b><small>${esc(e.text)}</small>${e.why ? `<small class="whyno">${esc(e.why)}</small>` : ''}</span><button class="btn small" data-act="menearly" ${e.why ? 'disabled' : ''}>Call them up now</button></div>`;
  const w = c.callUp;
  const wave = w ? `<div class="menorder"><span class="grow"><b>The standing call-up${w.held ? ' <span class="tag warn">Held</span>' : ''}</b><small>${esc(w.text)}</small></span><button class="btn small" data-act="menhold" data-hold="${w.held ? 0 : 1}" ${up ? '' : 'disabled title="It is not your month."'}>${w.held ? 'Let it run' : 'Hold it'}</button></div>` : '';
  return `<div class="menorders"><small>Your say over the call-up</small>${early}${wave}</div>`;
}

/** The ledger's count of men: the reserve as a bar against what it opened with, the month's class, and what has been called up. */
function menBlock(nat: string): string {
  const v = manpowerOf(g, nat);
  if (!v) return '';
  const fill = Math.max(0, Math.min(100, v.share * 100));
  const inf = specOf(g, 'inf')?.divs ?? 3;
  const months = v.classMen > 0 ? Math.ceil((inf * MEN_PER_DIV) / v.classMen) : 0;
  const home = v.home < 0.995 ? ` You hold ${Math.round(v.home * 100)}% of the homeland, counted by the worth of its provinces, so the class is that much smaller.` : '';
  return `<h3 id="lmen">Men fit to bear arms <span class="inhand">${esc(v.word.toLowerCase())}</span></h3>
      <div class="menbar ${v.word.toLowerCase()}" role="img" aria-label="${Math.round(v.share * 100)}% of the reserve this campaign opened with is still in hand."><i style="width:${fill.toFixed(1)}%"></i><u style="left:${MANPOWER.warn[0] * 100}%"></u><u style="left:${MANPOWER.warn[1] * 100}%"></u></div>
      <div class="lfacts men sum"><div class="lfact"><span>At the start</span><b>${menShort(v.start * MEN_PER_DIV)}</b></div><div class="lfact"><span>Come of age since</span><b class="pos">+${menShort(v.came)}</b></div><div class="lfact"><span>Called up so far</span><b class="neg">−${menShort(v.drawn)}</b></div><div class="lfact eq"><span>In reserve today</span><b>${menShort(v.men)}</b></div></div>
      <p class="sub mennow">${v.gap > 0 ? `No class comes of age for ${v.gap === 1 ? 'another month' : `another ${v.gap} months`}: it was called up early.` : `${upFirst(menWords(v.classMen))} men come of age each month.`}</p>${menOrders(nat)}
      <p class="sub">${esc(v.line)} Think of it as a reservoir behind a dam. Replacements for worn corps and every new corps are drawn off it, and each month's class of young men is the river that refills it.${home} A new infantry corps takes ${menWords(inf * MEN_PER_DIV)} men${months > 1 ? `, about ${months} months of the class` : ''}. With the reserve spent, only the month's class reaches the armies and no new corps can be raised.</p>`;
}

/** The gauges for a power that keeps stocks; nothing for a small country, which buys what it needs abroad. */
function gauges(me: string): string {
  const v = stores(g, me);
  return v ? gauge(v.oil) + gauge(v.sup) : '';
}

/** One side of the books: a bar for the depots, then what comes in and what goes out. */
function bookBlock(b: Book): string {
  const name = GOOD_NAME[b.good], lower = name.toLowerCase();
  const fill = Math.max(0, Math.min(100, (b.stock / Math.max(b.cap, 1e-9)) * 100));
  const short = b.met < 0.995, falling = b.net < -0.04;
  const state = short
    ? `<div class="note bad"><b>Short of ${lower}.</b> The depots are empty and only ${Math.round(b.met * 100)}% of the need came in this month.<ul>${b.effects.map((e) => `<li>${esc(e)}</li>`).join('')}</ul></div>`
    : falling && b.months <= 12
      ? `<div class="note warn"><b>${upFirst(monthsWordOf(b.months))} left.</b> More is going out than coming in. Once the depots are empty the forces get only what arrives that month.</div>`
      : '';
  const row = (l: ResLine, sign: 1 | -1): string => {
    const unit = l.count === undefined || l.lane ? '' : l.key === 'fast' || l.key === 'foot' || l.key === 'army' ? `${num(l.count)} division${l.count === 1 ? '' : 's'}` : l.key === 'air' ? `${num(l.count)} aircraft` : l.key === 'sea' || l.key === 'port' ? `${num(l.count)} ship${l.count === 1 ? '' : 's'}` : l.key === 'fields' || l.key === 'taken' ? `${l.count} field${l.count === 1 ? '' : 's'}` : l.key === 'fac' || l.key === 'tech' ? `${num(l.count)} working factor${l.count === 1 ? 'y' : 'ies'}` : `${l.count} province${l.count === 1 ? '' : 's'}`;
    const stopped = l.lane !== undefined && Math.abs(l.amount) < 1e-9;
    return `<tr${stopped ? ' class="stopped"' : ''}><td>${esc(l.label)}${unit ? ` <small>${unit}</small>` : ''}${l.note ? `<small class="lnote">${esc(l.note)}</small>` : ''}</td><td class="${l.amount * sign < -1e-9 ? 'neg' : ''}">${stopped ? '0' : signedAmt(l.amount * sign)}</td></tr>`;
  };
  return `<section class="sbook ${b.good}">
    <h3>${name} <span class="inhand">${num(Math.floor(b.stock + 1e-6))} in the depots · room for ${num(b.cap)}</span></h3>
    <div class="sbar${short ? ' dry' : falling && b.months <= 6 ? ' low' : ''}" role="img" aria-label="${name}: the depots are ${Math.round(fill)}% full"><i style="width:${fill.toFixed(1)}%"></i></div>
    <div class="sfacts"><span>Comes in <b>${signedAmt(b.made)}</b></span><span>Goes out <b>${signedAmt(-b.need)}</b></span><span class="${b.net < -0.04 ? 'neg' : 'pos'}">${b.net < -0.04 ? 'Short each month' : 'Left over each month'} <b>${signedAmt(b.net)}</b></span></div>
    ${state}
    <table class="ltab">${b.income.map((l) => row(l, 1)).join('')}<tr class="sum"><td>Comes in each month</td><td>${signedAmt(b.made)}</td></tr></table>
    <table class="ltab" style="margin-top:10px">${b.spend.map((l) => row(l, -1)).join('') || '<tr><td>Nothing needs any</td><td>0</td></tr>'}<tr class="sum"><td>Goes out each month</td><td>${signedAmt(-b.need)}</td></tr></table>
  </section>`;
}

const seaNames = (zones: number[]): string => { const xs = zones.map((z) => `the ${g.seas[z].name}`); return xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`; };

/** Who is escorting a lane, in a sentence for its row. */
function laneGuards(id: string, own: boolean): string {
  const fs = g.fleets.filter((f) => f.stance === 'escort' && f.lane === id && atSea(f) && shipCount(f.ships) > 0);
  if (fs.length) { const names = fs.map((f) => `the ${esc(f.name)}`); return ` Escorted by ${names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`}.`; }
  return own ? ' <b>No fleet is escorting it.</b> Pick up a fleet with destroyers and order it to escort this lane; a fleet that only patrols these seas gives a quarter of the cover.' : ' No fleet is escorting it.';
}

/** One lane or overland deal: what it carries, how much is getting through, and what to do about it. */
function laneRow(l: Lane, me: string | null): string {
  const pct = Math.round(l.flow * 100);
  const mine = !!me && (l.to === me || allied(g, l.to, me)), foe = !!me && atWar(g, me, l.to);
  const carries = [l.full.oil > 0.05 ? `oil ${amt(l.oil)} of ${amt(l.full.oil)}` : '', l.full.sup > 0.05 ? `supplies ${amt(l.sup)} of ${amt(l.full.sup)}` : ''].filter(Boolean).join(' · ');
  const status = l.state === 'cut' ? 'Cut' : l.state === 'idle' ? 'Not running' : `${pct}% getting through`;
  const fight = l.kind === 'sea' && l.zones.length && l.state !== 'cut' && l.state !== 'idle' && g.nations[l.to].atWar.length
    ? ` Raiders ${num(l.raid)} against escorts ${num(l.guard)} in ${seaNames(l.zones)}.` : '';
  const advice = l.kind !== 'sea' || !l.zones.length || l.state === 'idle' ? ''
    : mine && l.state !== 'cut' ? (seaWar(g) ? laneGuards(l.id, l.to === me) : ` Fleets on patrol in those seas, and aircraft on the coasts, keep it open.`)
    : foe && l.state !== 'cut' ? ` Submarines and raiders sent into those seas bleed it.` : '';
  return `<div class="lane ${l.state}">
    <div class="lhead">${flagIcon(l.to, g.nations[l.to].color, 18, 'inh')}<b>${esc(upFirst(l.name))}</b><span class="tag">${l.kind === 'sea' ? 'By sea' : 'Overland'}</span><span class="lflow">${status}</span></div>
    <div class="oddsbar" title="${status}"><i style="width:${pct}%"></i></div>
    <small>${esc(l.text)}${carries ? ` This month: ${carries}.` : ''}${l.why ? ` <b>${esc(l.why)}</b>` : ''}${esc(fight)}${advice}</small>
    ${l.path.length ? `<div class="acts"><button class="btn small" data-act="lanego" data-id="${l.id}">Show on the map</button></div>` : ''}
  </div>`;
}

/** How the other powers stand, as far as anyone outside can judge: a word for each, never a number. */
function standingTable(skip: string): string {
  const rows = RES.nations.filter((id) => id !== skip && g.nations[id]?.alive).map((id) => {
    const v = stores(g, id);
    if (!v) return '';
    const word = (b: Book): string => { const s = depotState(b); return `<td class="st ${s}">${STANDING[s]}</td>`; };
    return `<tr><td>${flagIcon(id, g.nations[id].color, 18, 'inh')} ${esc(natName(id))}</td>${word(v.oil)}${word(v.sup)}</tr>`;
  }).join('');
  return rows ? `<table class="ltab sttab"><tr><th></th><th>Oil</th><th>Supplies</th></tr>${rows}</table>` : '';
}

/** The books for oil and supplies: the player's own in full, an ally's in full, anyone else's only as far as it shows from outside. */
function storesCard(nat: string): string {
  const n = g.nations[nat];
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  // An agent reading a government's books in its capital opens its depots too.
  const mine = nat === me, open = !me || mine || allied(g, nat, me) || knowsBooks(g, nat, me);
  const v = stores(g, nat);
  const head = `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Oil and supplies · ${date(g.turn)}</div><h2>${flagIcon(nat, n.color, 34, 'inh')} ${esc(mine ? 'Your depots' : `The depots of ${theName(nat)}`)}</h2></header>`;
  const foot = `<footer>${mine ? '<button class="btn" data-act="ledger">Open the ledger</button>' : me && keepsStores(g, me) ? `<button class="btn" data-act="stores" data-nat="${me}">Your own depots</button>` : ''}<button class="btn go" data-act="close">Close</button></footer>`;
  const story = `Money builds an army, oil moves it, and supplies keep it fighting. Think of a lorry: money buys the lorry, oil is the fuel in its tank, and supplies are the load it carries and the driver's dinner. A great power makes some of each at home, buys some from its neighbours, and brings the rest in by sea. Whoever cuts those sea lanes chokes it.`;
  if (!v) {
    return `${head}<div class="body ledger stores"><p class="sub">${story}</p>
      <div class="note"><b>${esc(upFirst(theName(nat)))} keeps no stocks.</b> A small country buys the little oil and the supplies it needs abroad, month by month, and is never short. Only the great powers have fleets, air forces and tank armies large enough to run dry.</div>
      <h3>How the great powers stand</h3>${standingTable('') || '<div class="empty">No great power is in the field.</div>'}
      <p class="sub" style="margin-top:8px">The shipping lanes are drawn on the map. A lane in amber is losing cargoes to raiders; one in red is cut.</p></div>${foot}`;
  }
  const lanes = v.lanes.filter((l) => l.state !== 'idle' || l.kind === 'sea');
  const laneList = lanes.length ? lanes.map((l) => laneRow(l, me)).join('') : `<div class="empty">${esc(upFirst(theName(nat)))} lives on what it makes at home.</div>`;
  if (!open) {
    const word = (b: Book): string => { const s = depotState(b); return `<div class="lfact"><span>${GOOD_NAME[b.good]}</span><b class="st ${s}">${STANDING[s]}</b></div>`; };
    return `${head}<div class="body ledger stores"><p class="sub">${story}</p>
      <div class="lfacts two">${word(v.oil)}${word(v.sup)}</div>
      <p class="sub">What ${esc(theName(nat))} holds in its depots is its own secret. This is how it looks from outside: what its lanes carry, and what is getting through.</p>
      <h3>Its lanes and deals</h3><div class="lanes">${laneList}</div></div>${foot}`;
  }
  return `${head}<div class="body ledger stores">
    <p class="sub">${story}</p>
    <div class="lcols">${bookBlock(v.oil)}${bookBlock(v.sup)}</div>
    ${v.willLost > 0.005 ? `<div class="note bad"><b>Privation at home.</b> Months of empty depots have worn away ${Math.round(v.willLost * 100)}% of the nation's will to fight. It counts toward surrender beside lost ground and a broken army, and it mends only slowly once the depots are filled again.</div>` : ''}
    <h3>Shipping lanes and deals <span class="inhand">what comes from abroad</span></h3>
    <div class="lanes">${laneList}</div>
    <h3>How the other powers stand <span class="inhand">as far as it shows from outside</span></h3>
    ${standingTable(nat) || '<div class="empty">No other great power is in the field.</div>'}
  </div>${foot}`;
}

/** The shipping lanes on the map: a line through each lane's seas, pale while cargoes pass, amber while raiders bleed it, red and broken when it is cut. */
function laneMarks(): string {
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  let out = '';
  for (const l of seaLanes(g)) {
    if (l.state === 'idle' || l.path.length < 2) continue;
    const p = l.path;
    let d = `M${p[0][0]} ${p[0][1]}`;
    for (let i = 0; i < p.length - 1; i++) {
      const a = p[i - 1] ?? p[i], b = p[i], c = p[i + 1], e = p[i + 2] ?? c;
      d += `C${(b[0] + (c[0] - a[0]) / 6).toFixed(1)} ${(b[1] + (c[1] - a[1]) / 6).toFixed(1)} ${(c[0] - (e[0] - b[0]) / 6).toFixed(1)} ${(c[1] - (e[1] - b[1]) / 6).toFixed(1)} ${c[0]} ${c[1]}`;
    }
    const side = me ? (l.to === me || allied(g, l.to, me) ? ' ours' : atWar(g, me, l.to) ? ' theirs' : '') : '';
    // The label sits on the longest stretch of open water, a little above the line.
    let k = 0, best = 0;
    for (let i = 0; i < p.length - 1; i++) { const len = Math.hypot(p[i + 1][0] - p[i][0], p[i + 1][1] - p[i][1]); if (len > best) { best = len; k = i; } }
    const lx = (p[k][0] + p[k + 1][0]) / 2, ly = (p[k][1] + p[k + 1][1]) / 2;
    const word = l.state === 'cut' ? 'cut' : `${Math.round(l.flow * 100)}%`;
    const tip = `${upFirst(l.name)}: feeds ${theName(l.to)}. ${l.state === 'cut' ? l.why : `${Math.round(l.flow * 100)}% of its cargoes are getting through.`}`;
    out += `<g class="lane ${l.state}${side}" data-act="stores" data-nat="${l.to}"><title>${esc(tip)}</title><path class="lanebed" d="${d}"/><path class="laneline" d="${d}"/><path class="lanehit" d="${d}"/><text class="lanelbl" x="${lx.toFixed(1)}" y="${(ly - 7).toFixed(1)}">${esc(upFirst(l.name.replace(/^the /, '')))} · ${word}</text></g>`;
  }
  return out;
}

/** The lanes that matter to the player, for the Navy tab: his own and his allies' to guard, his enemies' to raid. */
function laneNotes(): string {
  if (!resRules(g)) return '';
  const me = g.human && g.nations[g.human].alive ? g.human : null;
  const rows = seaLanes(g).filter((l) => l.state !== 'idle' && (!me || l.to === me || allied(g, l.to, me) || atWar(g, me, l.to)));
  if (!rows.length) return '';
  return `<h3>Shipping lanes</h3><p class="sub">${seaWar(g) ? 'Oil and supplies come to the great powers by sea. Give each of your own lanes an escort: pick up a fleet and choose <b style="color:var(--ink)">Escort</b>. Send raiders into an enemy\'s.' : 'Oil and supplies come to the great powers by sea. Guard your own lanes with fleets on patrol; send raiders into an enemy\'s.'}</p><div class="lanes">${rows.map((l) => laneRow(l, me)).join('')}</div>`;
}

/** What an oilfield is worth in oil, for the province panel. */
function oilNote(r: Region): string {
  if (!r.oil || !resRules(g)) return '';
  const own = fieldYield(g, r, r.owner), held = fieldYield(g, r);
  const buyers = RES.deals.filter((d) => d.field === r.name && d.oil > 0 && stores(g, d.to)?.lanes.some((l) => l.id === d.id && l.state === 'open'));
  const who = keepsStores(g, r.ctrl)
    ? `It gives ${esc(theName(r.ctrl))} ${amt(held)} oil a month${r.ctrl !== r.owner ? `, less than the ${amt(own)} it gave its owner` : ''}.`
    : `It yields ${amt(own)} oil a month.${buyers.length ? ` ${buyers.map((d) => `${natName(d.to)} buys ${amt(d.oil)}`).join(' and ')}.` : ''}`;
  return `<div class="note"><b>Oilfield.</b> ${who} A conqueror gets ${Math.round(RES.oil.occupied * 100)}% of what the owner did.</div>`;
}

// ---- The ledger: where the money comes from, what the forces cost, and what every province pays ----

type Arm = 'army' | 'navy' | 'air' | 'works';
const ARMS: Arm[] = ['army', 'navy', 'air', 'works'];
const ARM_OF = (key: string): Arm => (key.startsWith('corps:') ? 'army' : key.startsWith('ships:') ? 'navy' : key.startsWith('works:') ? 'works' : 'air');
const ARM_NAME = { army: 'Army', navy: 'Navy', air: 'Air force', works: 'Guns and airfields' } as const;

function ledgerRows(lines: LedgerLine[], sign: 1 | -1, unit: (l: LedgerLine) => string): string {
  return lines.map((l) => `<tr${sign < 0 ? ' class="sub"' : ''}><td>${esc(l.label)}${unit(l) ? ` <small>${unit(l)}</small>` : ''}</td><td class="${l.amount * sign < 0 ? 'neg' : ''}">${cash(l.amount * sign, sign < 0 || l.amount < 0)}</td></tr>`).join('');
}

function ledgerCard(nat: string): string {
  const n = g.nations[nat];
  const L: Ledger = ledger(g, nat);
  const mine = nat === g.human;
  const by: Record<Arm, number> = { army: 0, navy: 0, air: 0, works: 0 };
  // The three services always show; the guns and airfields only once some have been built.
  const arm3 = ARMS.filter((k) => k !== 'works' || by.works > 0 || L.upkeep.some((l) => ARM_OF(l.key) === 'works'));
  for (const l of L.upkeep) by[ARM_OF(l.key)] += l.amount;
  // The picture: one bar for the month's income, cut into what each arm takes and what is left to spend.
  const whole = Math.max(L.gross, L.upkeepTotal, 1e-9);
  const pct = (x: number): string => `${Math.max(0, (x / whole) * 100).toFixed(1)}%`;
  const share = (x: number): string => `${Math.round((x / Math.max(L.gross, 1e-9)) * 100)}%`;
  const left = Math.max(0, L.net);
  const bar = L.budget
    ? `<div class="lbar" role="img" aria-label="Of each month's income the army takes ${share(by.army)}, the navy ${share(by.navy)}, the air force ${share(by.air)}${by.works > 0 ? `, the guns and airfields ${share(by.works)}` : ''}, and ${share(left)} is left to spend.">${arm3.map((k) => (by[k] > 0 ? `<i class="${k}" style="width:${pct(by[k])}" title="${ARM_NAME[k]}: ${cash(by[k])} a month"></i>` : '')).join('')}${left > 0 ? `<i class="free" style="width:${pct(left)}" title="Left to spend: ${cash(left)} a month"></i>` : ''}</div>
      <div class="lkey">${arm3.map((k) => `<span><i class="${k}"></i>${ARM_NAME[k]} ${cash(by[k])} <small>${share(by[k])}</small></span>`).join('')}<span><i class="free"></i>${L.net >= 0 ? `Left to spend ${cash(left)} <small>${share(left)}</small>` : `Short by ${cash(-L.net)} a month`}</span></div>`
    : '';
  const unitIn = (l: LedgerLine): string => (l.count === undefined ? '' : l.key === 'land' ? `${l.count} province${l.count === 1 ? '' : 's'}` : l.key === 'trade' ? `${l.count} partner${l.count === 1 ? '' : 's'}` : l.key === 'capital' ? '' : `${num(l.count)}`);
  const unitOut = (l: LedgerLine): string => (l.count === undefined ? '' : l.key.startsWith('corps:') ? `${num(l.count)} division${l.count === 1 ? '' : 's'}` : l.key.startsWith('air:') ? `${num(l.count)} aircraft` : `${num(l.count)}`);
  const moneyIn = `<table class="ltab">${ledgerRows(L.income, 1, unitIn)}${ledgerRows(L.changes, 1, () => '')}<tr class="sum"><td>Income each month</td><td>${cash(L.gross, true)}</td></tr></table>`;
  const arms = ARMS.filter((k) => by[k] > 0).map((k) => `<tr class="arm"><td><i class="dot ${k}"></i>${ARM_NAME[k]}</td><td>${cash(-by[k], true)}</td></tr>${ledgerRows(L.upkeep.filter((l) => ARM_OF(l.key) === k), -1, unitOut)}`).join('');
  const moneyOut = L.budget
    ? `<table class="ltab">${arms || '<tr><td>No forces under arms</td><td>0</td></tr>'}<tr class="sum"><td>Upkeep each month</td><td>${cash(-L.upkeepTotal, true)}</td></tr></table>${n.atWar.length ? '' : `<p class="sub" style="margin-top:6px">At peace the forces are on a peacetime footing and cost ${Math.round(BUDGET.upkeep.peace * 100)}% of this in war.</p>`}`
    : `<p class="sub">In a historical scenario the forces are kept by the state as history kept them: no upkeep is charged, so that history's timetable holds. In the grand campaign every corps, ship and aircraft draws its keep from the treasury.</p>`;
  const all = !!ui.modal?.all;
  const shown = all ? L.provinces : L.provinces.slice(0, 10);
  const prov = shown.map((p) => {
    const r = g.regions[p.id];
    const bits: string[] = [];
    if (p.capital) bits.push(`capital ${cash(p.capital)}`);
    if (p.fac) bits.push(`${r.fac} factor${r.fac === 1 ? 'y' : 'ies'} ${cash(p.fac)}`);
    if (p.oil) bits.push(`oil ${cash(p.oil)}`);
    if (p.port) bits.push(`harbour ${cash(p.port)}`);
    if (p.land) bits.push(`${r.terrain} ${cash(p.land)}`);
    return `<button class="lprov" data-act="ledgergo" data-id="${p.id}" title="Show ${esc(p.name)} on the map"><span class="grow"><b>${esc(p.name)}</b>${r.owner !== nat ? ' <span class="tag">Occupied</span>' : r.overseas ? ' <span class="tag">Overseas</span>' : ''}<small>${bits.join(' · ')}</small></span><span class="lbarx"><i style="width:${Math.max(2, (p.total / Math.max(L.provinces[0]?.total ?? 1, 1e-9)) * 100).toFixed(0)}%"></i></span><b class="amt">${cash(p.total)}</b></button>`;
  }).join('');
  const debt = L.treasury < 0
    ? `<div class="note" style="border-left-color:var(--red)"><b>In debt.</b> While the treasury is empty no replacements go out and nothing new can be ordered. Unpaid troops recover up to ${Math.round(BUDGET.debt.rest * 100)}% less cohesion each month and up to ${Math.round(BUDGET.debt.desert * 100)}% of every corps drifts away, worse the deeper the debt. Creditors stop lending at ${BUDGET.debt.floor} months of income.</div>`
    : '';
  const story = L.budget
    ? `Think of it as a household budget. Every province is a pay cheque, large or small. The armed forces are the standing bills, due every month whether they fight or not. What is left after the bills replaces losses first and then buys new forces, and a new corps is not only its price: it is another bill every month after.`
    : `Every month the factories pay in, losses in the field are made good, and what is left buys new corps and works.`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">The ledger · ${date(g.turn)}</div><h2>${flagIcon(nat, n.color, 34, 'inh')} ${esc(mine ? 'Your treasury' : `The treasury of ${natName(nat)}`)}</h2></header>
    <div class="body ledger">
      <div class="lfacts"><div class="lfact"><span>In the treasury</span><b class="${L.treasury < 0 ? 'neg' : ''}">${inHand(L.treasury)}</b></div><div class="lfact"><span>Income a month</span><b>${cash(L.gross, true)}</b></div><div class="lfact"><span>Upkeep a month</span><b>${L.budget ? cash(-L.upkeepTotal, true) : '0'}</b></div><div class="lfact net"><span>${L.net >= 0 ? 'Left to spend a month' : 'Short each month'}</span><b class="${L.net < 0 ? 'neg' : 'pos'}">${cash(L.net, true)}</b></div></div>
      ${bar}${debt}
      <p class="sub">${story}</p>
      <div class="lcols"><div><h3>Money in</h3>${moneyIn}</div><div><h3>Money out</h3>${moneyOut}</div></div>
      ${menBlock(nat)}
      <h3>What the provinces pay <span class="inhand">${L.provinces.length} held · richest first</span></h3>
      <div class="lprovs">${prov || '<div class="empty">No provinces held.</div>'}</div>
      ${L.provinces.length > 10 ? `<div class="acts"><button class="btn small" data-act="ledgerall">${all ? 'Show the ten richest' : `Show all ${L.provinces.length} provinces`}</button></div>` : ''}
    </div>
    <footer>${mine ? '<button class="btn" data-act="ledgerbuild">Raise forces</button>' : ''}${resRules(g) ? `<button class="btn" data-act="stores" data-nat="${nat}">Oil and supplies</button>` : ''}${tradeRules(g) ? `<button class="btn" data-act="trade" data-nat="${nat}">Trade</button>` : ''}<button class="btn go" data-act="close">Close</button></footer>`;
}

const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);

/**
 * The question asked before the player's army crosses a neutral frontier, or when he picks a country to declare war
 * on: who it is, who its friends are, and everything that follows from saying yes. Saying no leaves the army where it was.
 */
function invadeCard(on: string, target: number): string {
  const me = g.human!;
  const f = previewDeclare(g, on)!;
  const n = g.nations[on];
  const r = target >= 0 ? g.regions[target] : null;
  const mine = bloc(g.nations[me]);
  const blocOf = (b: 'allies' | 'axis'): string => (b === 'axis' ? 'the Axis' : 'the Allies');
  const friendRows = f.friends.map((x) => `<div class="frow">${chip(x.nat)}<span class="grow"><b>${esc(natName(x.nat))}</b><small>${esc(upFirst(x.why))}</small></span><small class="stand">${esc(standLabel(g.nations[x.nat]))}</small></div>`).join('');
  const what: string[] = [];
  const The = esc(upFirst(theName(on)));
  what.push(f.joins ? `<b>${The} joins ${blocOf(f.joins)}</b> the moment it is attacked, and its ${divsOf(fieldArmy(g, on))} fight${Math.round(fieldArmy(g, on)) === 1 ? 's' : ''} for them.` : `<b>${The}</b> and its ${divsOf(fieldArmy(g, on))} are at war with you from today.`);
  if (f.league.length) what.push(`<b>The League stands by it.</b> ${The} belongs to ${LEAGUE.name}, and war on one member is war on all: ${esc(listOf(f.league.map(natName)))} ${f.league.length === 1 ? 'goes' : 'go'} to war with you the same day, with ${divsOf(f.league.reduce((sum, id) => sum + fieldArmy(g, id), 0))} between them.`);
  if (f.trade) what.push(`<b>Its trade ends.</b> ${The} trades with you today, worth ${money(DIPLOMACY.tradeIncome)} a month to your treasury.`);
  if (f.sneak > 1) what.push(`<b>Surprise.</b> It is at peace and not expecting you: this month your attacks on its ground are <b>${Math.round((f.sneak - 1) * 100)}% stronger</b>. Next month the bonus is gone.`);
  else what.push(f.ready === 'war' ? 'No surprise: it is already at war with your side and its army is ready.'
    : f.ready === 'alert' ? `No surprise: ${esc(traitsOf(on).find((t) => t.alert)?.name.toLowerCase() ?? 'its intelligence service')} has read the signs, and its army is ready.`
    : 'No surprise: its army is watching the frontier.');
  // The player's own allies are not bound to follow him into a war of his choosing.
  const hang = Object.values(g.nations).filter((x) => x.id !== me && x.alive && !x.dormant && allied(g, me, x.id) && !atWar(g, x.id, on)).map((x) => x.id);
  if (hang.length) what.push(`<b>Your allies do not follow at once.</b> ${esc(upFirst(listOf(hang.map(theName))))} ${hang.length === 1 ? 'stays' : 'stay'} at peace with ${esc(theName(on))}: each comes in when its own government judges the moment right, and that may be months away.`);
  const moved = f.alarm.filter((a) => a.from !== a.to);
  const friends = f.alarm.filter((a) => a.friend);
  if (f.alarm.length) {
    if (friends.length) what.push(`<b>Its friends turn against you.</b> Public opinion in ${esc(listOf(friends.map((a) => natName(a.nat))))} swings hard toward ${mine === 'axis' ? 'the Allies' : 'the Axis'}, and part of that never comes back.`);
    what.push(`<b>The neutral world grows wary.</b> ${f.alarm.length - friends.length} other ${f.alarm.length - friends.length === 1 ? 'neutral moves' : 'neutrals move'} a little away from ${mine === 'axis' ? 'the Axis' : 'the Allies'}. Every attack on a neutral adds to it.`);
    for (const a of moved) {
      const was = TRACK.indexOf(a.from) - 3, now = TRACK.indexOf(a.to) - 3;
      const how = now === 0 ? `no longer leans toward ${blocOf(was > 0 ? 'axis' : 'allies')}` : Math.abs(now) === 1 ? `begins to lean toward ${blocOf(now > 0 ? 'axis' : 'allies')}` : `throws in its lot with ${blocOf(now > 0 ? 'axis' : 'allies')}`;
      what.push(`<b>${esc(upFirst(theName(a.nat)))}</b> <b>${how}</b>${a.trade ? `, and <b>stops trading with you</b> (${money(DIPLOMACY.tradeIncome)} a month lost)` : ''}.`);
    }
    if (!moved.length) what.push('Nobody changes sides over this one attack, but the next may tip them.');
  } else if (f.joins === null && bloc(n) === 'league') what.push('The neutrals on the track between the blocs take no side in a quarrel with the League.');
  else if (f.joins === null && bloc(n) !== null) what.push(`${The} already belongs to ${blocOf(bloc(n) as 'allies' | 'axis')}: the neutral world expects this war and takes no fright at it.`);
  else if (!mine) what.push('Your country belongs to neither bloc, so the neutrals have no side to turn away from. They will remember it all the same.');
  const at = TRACK.indexOf(n.align) - 3;
  const stands = n.league ? `is a member of ${LEAGUE.name} and at peace with you` : at === 0 ? 'is neutral and at peace with you' : Math.abs(at) === 1 ? `is neutral, leans toward ${blocOf(at > 0 ? 'axis' : 'allies')}, and is at peace with you` : `is ${esc(TRACK_PHRASE[n.align])}, but at peace with you`;
  const lead = r
    ? `Your army is about to cross the frontier into <b>${esc(r.name)}</b>. ${The} ${stands}. Marching in means war.`
    : `${The} ${stands}.`;
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Diplomacy · ${date(g.turn)}</div><h2>${flagIcon(on, n.color, 34, 'inh')} Declare war on ${esc(theName(on))}?</h2></header>
    <div class="body invade"><p class="lead">${lead}</p>
    ${friendRows ? `<h3>Its friends</h3><div class="frows">${friendRows}</div>` : `<p class="sub">${The} has no close friends among the nations.</p>`}
    <h3>What follows if you say yes</h3><ul class="follows">${what.map((x) => `<li>${x}</li>`).join('')}</ul></div>
    <footer><button class="btn" data-act="close">Not now</button><button class="btn warn" data-act="dodeclare" data-nat="${on}" data-target="${target}">${r ? 'Declare war and march ▸' : 'Declare war'}</button></footer>`;
}

function declareCard(): string {
  const list = declarable(g);
  return `<header><button class="x" data-act="close" aria-label="Close">×</button><div class="kick">Diplomacy</div><h2>Declare war</h2></header>
    <div class="body"><p class="lead">A neutral that is attacked joins the other side, and its friends may march to help it at once.</p>
    ${list.map((id) => `<button class="pickrow" data-act="askdeclare" data-nat="${id}">${chip(id)}<span class="grow"><b>${esc(natName(id))}</b><br><span style="font-size:11.5px;color:var(--ink-2)">${divsOf(fieldArmy(g, id))} · ${esc(standLabel(g.nations[id]))}${friendsOf(id).some((f) => g.nations[f.nat]?.alive) ? ` · friends: ${esc(listOf(friendsOf(id).filter((f) => g.nations[f.nat]?.alive).map((f) => natName(f.nat))))}` : ''}</span></span></button>`).join('')}
    <p class="sub" style="margin-top:8px">Pick a country to see what declaring war on it would mean. Nothing happens until you confirm.</p></div>
    <footer><button class="btn" data-act="close">Not now</button></footer>`;
}

// ---- Running the game ---------------------------------------------------------------------

/** Starts a campaign. With a later starting month, the computer plays history up to it and then hands over. */
function newGame(human: string | null, seed?: number, from = 1, view = '', level: string = DIFFICULTY.start, cabinet = true, secure = false): void {
  stopAuto();
  dropPassing();
  g = createGame(seed ?? (Date.now() % 900000) + 1, from > 1 ? null : human);
  // A grand campaign: history is played to the day the player takes over, and from then on nobody follows it.
  const grand = view === 'grand' || view === 'open';
  if (grand && from === 1) startOpen(g);
  while (g.turn < from) runTurnAI(g);
  if (grand && from > 1) startOpen(g);
  g.human = human;
  // The level the player chose for his grand campaign. With nobody at the table every nation plays the fair game.
  if (grand && human) setLevel(g, level);
  // A player who wants the war without the politics plays with the war cabinet switched off.
  if (grand && human && !cabinet) orderCabinet(g, false);
  if (grand && human && secure) orderOffice(g, false);
  // Honours earned before the player took over are his to carry; their citations are old news.
  if (from > 1) takeOver(g);
  if (human) openGoals(g, human, from);
  ui.live = true;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false; ui.tab = 'region'; ui.unread = 0; ui.seenReports = from > 1 ? g.reports.length : 0; ui.flash = [];
  ui.sea = ui.fleet = -1; ui.land = ui.rebase = false; ui.msg = ''; ui.seenSea = from > 1 ? g.seaReports.length : 0;
  ui.air = -1; ui.airMode = ''; ui.seenAir = from > 1 ? g.airReports.length : 0; ui.seenPass = g.passings.length;
  drawStatic();
  const italy = view === 'italy', west = view === 'west', east = view === 'east';
  const britain = from === 11, desert = from === 13 || from === 33, balkans = from === 14, blue = from === 34, torch = from === 39, kursk = from === 47 && !italy;
  if (britain) ui.tab = 'air';
  const focus = britain ? g.byName['Calais'] : desert ? g.byName['Tobruk'] : torch ? g.byName['Constantine'] : balkans ? g.byName['Thessaly'] : blue ? g.byName[human === 'SOV' ? 'Stalingrad' : 'Kharkov'] : kursk ? g.byName['Kursk'] : east ? g.byName[from >= 58 ? 'Minsk' : 'Kiev'] : west ? g.byName['Rouen'] : italy ? g.byName[from >= 53 ? 'Rome' : 'Calabria'] : from > 1 ? g.byName[human === 'SOV' ? 'Smolensk' : 'Brest'] : human ? g.regions[g.nations[human].capital] : g.byName['Poznan'];
  ui.view = { x: focus.x - (torch ? 420 : 300), y: focus.y - (torch ? 250 : 230), w: torch ? 760 : desert || balkans ? 580 : human === 'GER' || from > 1 ? 640 : 600 };
  // Italy in 1944 is fought over a hundred miles of mountain: start close in.
  if (italy && from >= 53) ui.view = { x: focus.x - 215, y: focus.y - 185, w: 440 };
  // The West in 1944: the south of England, the invasion coast, and the roads to Paris and the Rhine.
  if (west) ui.view = { x: focus.x - 215, y: focus.y - 200, w: 520 };
  // The eastern front of 1944 runs from the Gulf of Finland to the Black Sea: start with all of it in view.
  if (east) ui.view = { x: focus.x - 400, y: focus.y - (from >= 58 ? 300 : 330), w: 800 };
  applyView();
  // A country that is attacked before its first month comes round places its army on the eve of war.
  if (human && from === 1 && beginEve(g)) refresh();
  else if (human) advanceToHuman(from > 1 && human !== 'GER'); else { ui.unread = from > 1 ? 0 : g.headlines.length; refresh(); }
}

/** Picks up the campaign saved in this browser. Returns false if there is none that can be read. */
function continueGame(): boolean {
  const loaded = loadGame(readSave() ?? '');
  if (!loaded) return false;
  stopAuto();
  dropPassing();
  g = loaded;
  ui.live = true;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = false; ui.tab = 'region'; ui.unread = 0; ui.seenReports = g.reports.length; ui.flash = [];
  ui.sea = ui.fleet = -1; ui.land = ui.rebase = false; ui.msg = ''; ui.seenSea = g.seaReports.length;
  ui.air = -1; ui.airMode = ''; ui.seenAir = g.airReports.length; ui.seenPass = g.passings.length;
  drawStatic();
  const me = g.human;
  const cap = me ? g.nations[me].capital : -1;
  const focus = cap >= 0 ? g.regions[cap] : g.byName['Paris'];
  ui.view = { x: focus.x - 300, y: focus.y - 230, w: 620 };
  applyView();
  if (me && !humanUp(g)) advanceToHuman(false); else refresh();
  return true;
}

/** Lets the computer play until it is the player's month again. */
type Since = { h: number; r: number; s: number; a: number; shore?: number };

/** The other nations' months being played out in front of the player, one nation at a time. */
let passing: { since: Since; before: string[]; show: boolean; view: { x: number; y: number; w: number }; timer: number; nat: string; note: string; done: number; total: number } | null = null;
/** How long the map rests on a nation that has done something, in milliseconds. */
const WATCH_PAUSE = 700;

function watchPref(): boolean {
  try { return localStorage.getItem('tc-watch') !== '0'; } catch { return true; }
}

function advanceToHuman(show: boolean, since?: Since): void {
  const from: Since = since ?? { h: g.headlines.length, r: g.reports.length, s: g.seaReports.length, a: g.airReports.length, shore: g.shoreReports.length };
  const before = g.regions.map((r) => r.ctrl);
  if (show && g.human && watchPref() && !humanUp(g)) {
    passing = { since: from, before, show, view: { ...ui.view }, timer: 0, nat: '', note: '', done: 0, total: g.order.length - 1 };
    ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = ui.rebase = false; ui.msg = ''; ui.airMode = ''; ui.flash = [];
    refresh();
    passStep();
    return;
  }
  runOthers();
  arrive(show, from, before);
}

/** Every computer nation's month until the player is up again, or has fallen. */
function runOthers(): void {
  let guard = 0;
  while (!humanUp(g) && guard++ < 80) {
    if (g.human && !g.nations[g.human].alive) break;
    stepOther();
  }
}

/** One computer nation's month. A goal that asks what stands when a month opens is judged the moment it opens. */
function stepOther(): string {
  const month = g.turn;
  const nat = stepAI(g);
  if (g.human && g.turn !== month) judgeGoals(g, g.human);
  return nat;
}

const some = (names: string[]): string => (names.length <= 3 ? names.join(', ') : `${names.slice(0, 3).join(', ')} and ${names.length - 3} more`);

/** Plays computer nations until one of them has done something worth seeing, shows it, and waits a moment. */
function passStep(): void {
  const p = passing;
  if (!p) return;
  p.timer = 0;
  for (let guard = 0; guard < 80; guard++) {
    if (humanUp(g) || !g.human || !g.nations[g.human].alive) break;
    const ctrl = g.regions.map((r) => r.ctrl);
    const r0 = g.reports.length;
    const nat = stepOther();
    p.done++;
    const taken = g.regions.filter((r, i) => r.ctrl !== ctrl[i]).map((r) => r.id);
    const reps = g.reports.slice(r0);
    if (!taken.length && !reps.length) continue;
    // A battle won round a town that still holds out is a siege begun, and a siege raised is not ground taken.
    const won = reps.filter((r) => r.taken && !r.siege && !r.sally && !r.relief).map((r) => r.regionName);
    const laid = reps.filter((r) => r.siege).map((r) => townName(g.regions[r.region]));
    const freed = reps.filter((r) => r.taken && (r.sally || r.relief)).map((r) => townName(g.regions[r.region]));
    const held = reps.filter((r) => !r.taken).map((r) => r.regionName);
    const walked = taken.filter((id) => !reps.some((r) => r.taken && r.region === id)).map((id) => g.regions[id].name);
    const said = [won.length ? `took ${some(won)}` : '', laid.length ? `laid siege to ${some(laid)}` : '', freed.length ? `raised the siege of ${some(freed)}` : '', walked.length ? `moved into ${some(walked)}` : '', held.length ? `${won.length || laid.length || freed.length || walked.length ? 'and were' : 'were'} thrown back at ${some(held)}` : ''].filter(Boolean);
    p.nat = nat;
    p.note = `${g.nations[nat].adj} forces ${said.join(', ')}.`;
    ui.flash = taken;
    const mine = reps.some((r) => r.defNat === g.human || r.defenders.some((u) => u.nat === g.human)) || taken.some((id) => p.before[id] === g.human);
    refresh();
    glide(viewFor([...new Set([...taken, ...reps.map((r) => r.region)])]));
    p.timer = window.setTimeout(passStep, WATCH_PAUSE + (mine ? 500 : 0) + Math.min(400, 100 * (reps.length - 1)));
    return;
  }
  endPassing();
}

/** The watching is over, or cut short: whatever is left is played at once and the player's month opens. */
function endPassing(): void {
  const p = passing;
  if (!p) return;
  clearTimeout(p.timer);
  passing = null;
  runOthers();
  glide(p.view, 260);
  arrive(p.show, p.since, p.before);
}

/** Drops a watched month without finishing it: a new game is about to replace this one. */
function dropPassing(): void {
  if (passing) clearTimeout(passing.timer);
  passing = null;
  glideToken++;
}

function arrive(show: boolean, from: Since, before: string[]): void {
  const hFrom = from.h, rFrom = from.r, sFrom = from.s, aFrom = from.a;
  if (g.human) judgeGoals(g, g.human);
  // The month the country fell in is played out to its end, so the watcher's calendar starts on a month's last day.
  // The seat is remembered: if the country is liberated, the player is given it back.
  const fell = (): string | null => {
    const who = g.human && !g.nations[g.human].alive ? g.human : null;
    if (who) { playerFell(g); while (g.phase > 0 && !g.human) stepAI(g); ui.seenPass = g.passings.length; }
    return who;
  };
  let lost = fell();
  // A country can also fall as its own month opens, starved out or with its capital lost to hunger: the same is done then.
  if (humanUp(g)) { beginHuman(g); lost = lost ?? fell(); }
  ui.raidsFrom = aFrom;
  ui.shoreFrom = from.shore ?? -1;
  ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
  ui.unread += g.headlines.length - hFrom;
  ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = ui.rebase = false; ui.msg = ''; ui.airMode = '';
  refresh();
  if (lost) {
    openModal({ kind: 'news', from: hFrom, reports: rFrom, sea: sFrom, air: aFrom });
    // A nation whose goals are about holding out is told how long it held.
    const weeks = Object.values(GOALS[lost] ?? {}).some((x) => x?.outlast !== undefined) ? weeksLasted(g, lost) : null;
    const nth = ['', 'first', 'second', 'third', 'fourth', 'fifth', 'sixth', 'seventh', 'eighth', 'ninth', 'tenth', 'eleventh', 'twelfth'];
    const hope = nationsFallen(g).find((f) => f.nat === lost);
    const back = hope?.canReturn ? ` If ${natName(hope.fell.to)}'s enemies take ${hope.capitalName}, ${natName(lost)} is liberated and you lead it again.` : '';
    $('#card').querySelector('.kick')!.textContent = (weeks !== null && nth[weeks]
      ? `${natName(lost)} has fallen in the ${nth[weeks]} week of the war. The war goes on without you.`
      : `${natName(lost)} has fallen. The war goes on without you.`) + back;
    return;
  }
  if (show && (g.headlines.length > hFrom || g.reports.length > rFrom)) openModal({ kind: 'news', from: hFrom, reports: rFrom, sea: sFrom, air: aFrom });
  else { const next = nextCard(); if (next) openModal(next); }
}

function nextMonth(): void {
  const before = g.regions.map((r) => r.ctrl);
  const hFrom = g.headlines.length, rFrom = g.reports.length;
  runTurnAI(g);
  if (g.human) {
    // The country the player lost has been liberated: the player is given it back, and the announcement says so.
    stopAuto();
    const mine = g.passings.findIndex((p, i) => i >= ui.seenPass && p.kind === 'freed' && p.nat === g.human);
    ui.seenPass = mine >= 0 ? mine : g.passings.length;
    ui.unread += g.headlines.length - hFrom;
    const cap = g.nations[g.human].capital;
    if (cap >= 0) focusOn([cap]);
    advanceToHuman(false);
    return;
  }
  ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
  ui.unread += g.headlines.length - hFrom;
  // Follow the action.
  const hot = new Set<number>(ui.flash);
  for (const r of g.reports.slice(rFrom)) hot.add(r.region);
  if (!hot.size) for (const r of g.airReports) if (r.turn === g.turn - 1 && r.region >= 0) hot.add(r.region);
  if (hot.size) focusOn([...hot]);
  if (ui.tab === 'reel') ui.unread = 0;
  refresh();
  if (g.turn > (isOpen(g) ? FREE_ENDS : STORY_ENDS)) stopAuto();
}

/** The view that takes in these regions with some room round them. */
function viewFor(ids: number[]): { x: number; y: number; w: number } {
  let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
  for (const id of ids) { const r = g.regions[id]; x0 = Math.min(x0, r.x); y0 = Math.min(y0, r.y); x1 = Math.max(x1, r.x); y1 = Math.max(y1, r.y); }
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.max(420, x1 - x0 + 260, (y1 - y0 + 240) / a);
  return { x: (x0 + x1) / 2 - w / 2, y: (y0 + y1) / 2 - (w * a) / 2, w };
}

function focusOn(ids: number[]): void {
  glideToken++;
  ui.view = viewFor(ids);
  applyView();
}

let glideToken = 0;

/** Moves the map to a new view smoothly, as a camera would travel, instead of jumping there. */
function glide(to: { x: number; y: number; w: number }, ms = 340): void {
  const from = { ...ui.view };
  const token = ++glideToken;
  const t0 = performance.now();
  const step = (now: number): void => {
    if (token !== glideToken) return;
    const k = Math.min(1, (now - t0) / ms);
    const e = k < 0.5 ? 2 * k * k : 1 - (-2 * k + 2) ** 2 / 2;
    ui.view = { x: from.x + (to.x - from.x) * e, y: from.y + (to.y - from.y) * e, w: from.w + (to.w - from.w) * e };
    applyView();
    if (k < 1) requestAnimationFrame(step);
  };
  requestAnimationFrame(step);
}

function focusOnPoint(x: number, y: number): void {
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const w = Math.max(ui.view.w, 520);
  ui.view = { x: x - w / 2, y: y - (w * a) / 2, w };
  applyView();
}

/** Where on the map a point of the screen falls. */
function mapPoint(cx: number, cy: number): { x: number; y: number } {
  const rect = $('#map').getBoundingClientRect();
  const k = ui.view.w / Math.max(1, rect.width);
  return { x: ui.view.x + (cx - rect.left) * k, y: ui.view.y + (cy - rect.top) * k };
}

/** The order a fleet will sail under when it is sent to a sea: the work it is doing, or its usual duty. Escorting is given by lane, not by sea. */
const sailingAs = (f: Fleet): Stance => (atSea(f) && f.stance !== 'escort' ? f.stance : f.duty === 'raid' ? 'raid' : 'patrol');

/** Picks up one of the player's fleets: the Navy tab opens on it, its tray is laid out, and the map shows where it can go. */
function pickFleet(id: number): void {
  const f = g.fleets.find((x) => x.id === id);
  if (!f) return;
  ui.tab = 'navy'; ui.fleet = id; ui.sel = -1; ui.sea = -1; ui.picked.clear();
  ui.rail = ui.ship = ui.land = ui.rebase = false; ui.spySend = false;
  ui.stance = sailingAs(f);
  refresh();
  document.querySelector('#pane .fleet.pick')?.scrollIntoView({ block: 'nearest' });
}

/** A click on a fleet marker: picks the fleet up, moves on to the next one under the same marker, or puts the only one down. */
function pickFleetMark(ids: number[]): void {
  ui.flash = [];
  ui.msg = '';
  const at = ui.tab === 'navy' ? ids.indexOf(ui.fleet) : -1;
  if (at >= 0 && ids.length === 1) { ui.fleet = -1; ui.rebase = false; refresh(); return; }
  pickFleet(ids[(at + 1) % ids.length]);
}

/** The player's fleet being given orders, if it is still theirs to order. */
function myFleet(): Fleet | null {
  if (ui.fleet < 0 || !humanUp(g)) return null;
  const f = g.fleets.find((x) => x.id === ui.fleet);
  return f && f.nat === g.human && shipCount(f.ships) > 0 ? f : null;
}

/** Opens an airfield under the Air tab: the groups based there and what each is doing. */
function openAirfield(id: number): void {
  ui.tab = 'air'; ui.field = id; ui.sel = -1; ui.sea = -1; ui.picked.clear();
  ui.rail = ui.ship = ui.land = ui.rebase = false; ui.spySend = false;
  const f = g.airFleets.find((x) => x.id === ui.air);
  if (!f || f.ship !== undefined || f.base !== id) { ui.air = -1; ui.airMode = ''; }
  refresh();
  document.querySelector('#pane .afield.pick')?.scrollIntoView({ block: 'nearest' });
}

function onSea(z: number): void {
  ui.flash = [];
  ui.msg = '';
  if (ui.tab === 'air' && airWar(g) && humanUp(g) && ui.raid && ui.raid !== 'menu') {
    if (ui.raid === 'fleet') {
      const ts = raidTargetsFor(g, 'fleet', ui.lead).filter((x) => x.zone === z);
      if (ts.length === 1) { openModal({ kind: 'raid', raid: 'fleet', target: ts[0].id, ids: ui.lead >= 0 ? [ui.lead] : undefined }); return; }
      ui.msg = ts.length ? `${ts.length} enemy forces have been found in the ${g.seas[z].name}: click the one to strike, or pick it from the list.` : `The ${g.seas[z].name}: no enemy fleet has been found there in reach. A fleet has to be found before it can be struck: search the sea first.`;
    } else if (ui.raid !== 'strike') ui.msg = ui.raid === 'harbour' ? 'A raid on a harbour needs a port: click one of the red provinces.' : 'This raid needs a province: click one of the red ones.';
    else if (raidTargetsFor(g, 'strike', ui.lead).some((x) => x.sea && x.id === z)) { openModal({ kind: 'raid', raid: 'strike', target: z, ids: ui.lead >= 0 ? [ui.lead] : undefined }); return; }
    else ui.msg = `The ${g.seas[z].name}: ${previewRaid(g, 'strike', z, ui.lead >= 0 ? [ui.lead] : undefined).why || 'no enemy shipping is known there, or no bomber reaches it.'}`;
    refresh();
    return;
  }
  const af = ui.tab === 'air' ? myAir() : null;
  if (af && ui.airMode === 'search') {
    const res = orderSearch(g, af.id, z);
    ui.msg = res.text;
    if (res.ok) ui.airMode = '';
    refresh();
    return;
  }
  if (af && ui.airMode) {
    if (ui.airMode === 'strike') {
      const why = whyNotMission(g, af, 'strike', z);
      if (!why) { openModal({ kind: 'airplan', fleet: af.id, mission: 'strike', target: z }); return; }
      ui.msg = `The ${g.seas[z].name}: ${why}`;
    } else ui.msg = ui.airMode === 'base' ? 'Airfields are on land: click one of the blue regions.' : 'This mission needs a region: click one of the red ones.';
    refresh();
    return;
  }
  const fl = ui.tab === 'navy' ? myFleet() : null;
  if (fl) {
    if (ui.rebase) ui.msg = 'A base has to be a port: click one of the blue regions.';
    else if (!g.nations[fl.nat].atWar.length) ui.msg = 'At peace the fleet stays in harbour.';
    else if (!orderFleet(g, fl.id, z, ui.stance)) ui.msg = `The ${g.seas[z].name} is out of reach of the ${fl.name} from ${fl.port}.`;
    refresh();
    return;
  }
  ui.sea = z; ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = false;
  ui.tab = 'region';
  refresh();
}

function stopAuto(): void {
  if (ui.auto) { clearInterval(ui.auto); ui.auto = 0; }
}

// ---- Clicks -------------------------------------------------------------------------------

function onClick(e: MouseEvent): void {
  const t = (e.target as HTMLElement).closest('[data-act]') as HTMLElement | null;
  if (!t) { if (e.target === $('#veil') && ui.modal && ui.modal.kind !== 'start') openModal(null); return; }
  const act = t.dataset.act!;
  const id = Number(t.dataset.id ?? t.dataset.agent);
  ui.msg = '';
  switch (act) {
    case 'start': if (t.dataset.view === 'grand') rememberPick(); openModal(null); newGame(t.dataset.nat || null, undefined, Number(t.dataset.from || 1), t.dataset.view || '', t.dataset.level || DIFFICULTY.start, t.dataset.cabinet !== '0', t.dataset.secure === '1'); break;
    case 'menu': ui.menu = t.dataset.menu === 'scenarios' ? 'scenarios' : 'grand'; openModal({ kind: 'start' }); break;
    case 'gcnat': case 'gcyear': case 'gclevel': case 'gccabinet': case 'gcsecure': {
      if (act === 'gcnat') ui.gcNat = t.dataset.nat ?? ''; else if (act === 'gclevel') ui.gcLevel = t.dataset.level ?? DIFFICULTY.start; else if (act === 'gccabinet') ui.gcCabinet = t.dataset.on === '1'; else if (act === 'gcsecure') ui.gcSecure = t.dataset.secure === '1'; else ui.gcYear = Number(t.dataset.year) || 1939;
      // Repainting the card must not throw the reader back to its top.
      const keep = $('#card').scrollTop;
      openModal({ kind: 'start' });
      $('#card').scrollTop = keep;
      break;
    }
    case 'continue': if (continueGame()) openModal(null); else openModal({ kind: 'start' }); break;
    case 'new': stopAuto(); endPassing(); ui.menu = 'grand'; openModal({ kind: 'start' }); break;
    case 'skip': endPassing(); break;
    case 'alert': followAlert(t.dataset.kind!); break;
    case 'ledger': openModal({ kind: 'ledger', nat: t.dataset.nat || g.human || '' }); if (t.dataset.part === 'men') document.querySelector('#card #lmen')?.scrollIntoView({ block: 'start' }); break;
    case 'stores': {
      openModal({ kind: 'stores', nat: t.dataset.nat || g.human || '' });
      // Asked for by one of the two gauges, the card opens at that side of the books.
      const side = t.dataset.good ? document.querySelector(`#card .sbook.${t.dataset.good}`) : null;
      if (side && window.innerWidth < 760) side.scrollIntoView({ block: 'start' });
      break;
    }
    case 'books': openModal({ kind: 'books', nat: t.dataset.nat || '' }); break;
    case 'staffs': openModal({ kind: 'staffs' }); break;
    case 'spyrecruit': {
      const why = whyNotRecruitAgent(g);
      const a = why ? null : orderRecruitAgent(g);
      if (a) { ui.spy = a.id; ui.msg = `Agent ${a.name} is recruited. Training takes a month: orders can be given from ${date(a.ready)}.`; } else ui.msg = why ?? '';
      ui.spySend = false; ui.spyAsk = -1;
      refresh();
      break;
    }
    case 'spypick': {
      const a = secretService(g)?.agents.find((x) => x.id === id);
      const again = ui.tab === 'spies' && ui.spy === id;
      ui.spy = again && !t.closest('svg') ? -1 : id; ui.spySend = false; ui.spyAsk = -1; ui.tab = 'spies';
      refresh();
      if (a && !again) focusOn([a.reg]);
      break;
    }
    case 'spysend': ui.spy = id; ui.spySend = !ui.spySend; ui.spyAsk = -1; ui.tab = 'spies'; refresh(); break;
    case 'spygo': {
      const to = Number(t.dataset.to);
      const why = whyNotSendAgent(g, id, to);
      const a = secretService(g)?.agents.find((x) => x.id === id);
      if (why) ui.msg = why;
      else if (orderSendAgent(g, id, to) && a) ui.msg = `Agent ${a.name} sets out for ${g.regions[to].name} and will be at work there next month.`;
      ui.spySend = false; ui.spy = id;
      refresh();
      break;
    }
    case 'spymission': {
      const m = t.dataset.m as Mission;
      const a = secretService(g)?.agents.find((x) => x.id === id);
      if (orderAgentMission(g, id, m) && a) ui.msg = m === 'factory' || m === 'rail' || m === 'oil' ? `Agent ${a.name} will make the attempt as the month turns.` : '';
      ui.spySend = false;
      refresh();
      break;
    }
    case 'spyretire': {
      if (ui.spyAsk !== id) { ui.spyAsk = id; refresh(); break; }
      orderRetireAgent(g, id);
      ui.spyAsk = -1; ui.spy = -1; ui.spySend = false;
      refresh();
      break;
    }
    case 'fog': orderFog(g, !fogOn(g)); refresh(); break;
    case 'lanego': {
      const l = seaLanes(g).find((x) => x.id === t.dataset.id);
      openModal(null);
      if (l && l.path.length) {
        ui.tab = 'navy';
        refresh();
        const xs = l.path.map((p) => p[0]), ys = l.path.map((p) => p[1]);
        focusOnPoint((Math.min(...xs) + Math.max(...xs)) / 2, (Math.min(...ys) + Math.max(...ys)) / 2);
      }
      break;
    }
    case 'ledgerall': { const top = $('#card').scrollTop; openModal({ ...ui.modal!, all: !ui.modal?.all }); $('#card').scrollTop = top; break; }
    case 'ledgergo': { openModal(null); ui.tab = 'region'; selectRegion(id); refresh(); focusOn([id]); break; }
    case 'ledgerbuild': openModal(null); ui.tab = 'build'; refresh(); break;
    case 'town': openModal({ kind: 'town', id }); break;
    case 'civic': {
      const b = orderBuilding(g, id, t.dataset.line!);
      if (b) { ui.msg = `Ordered: ${b.name}. Ready ${date(b.eta)}.`; queueSave(); }
      refresh();
      if (ui.modal?.kind === 'town') openModal(ui.modal);
      break;
    }
    case 'trade': openModal({ kind: 'trade', nat: t.dataset.nat || '' }); break;
    case 'tradeall': if (ui.modal) { ui.modal.all = !ui.modal.all; openModal(ui.modal); } break;
    case 'tradeask': if (ui.modal) { ui.modal.ask = t.dataset.nat || ''; openModal(ui.modal); } break;
    case 'tradesign': case 'tradeend': {
      const nat = t.dataset.nat!, h = g.headlines.length;
      const ok = act === 'tradesign' ? orderTrade(g, nat) : orderEndTrade(g, nat);
      ui.unread += g.headlines.length - h;
      if (ok) { ui.msg = act === 'tradesign' ? `A trade agreement is signed with ${theName(nat)}.` : `The trade agreement with ${theName(nat)} is ended.`; queueSave(); }
      if (ui.modal) ui.modal.ask = '';
      refresh();
      if (ui.modal?.kind === 'trade') openModal(ui.modal);
      break;
    }
    case 'cabinet': ui.govTab = 'now'; openModal({ kind: 'cabinet', nat: t.dataset.nat || g.human || '' }); refresh(); break;
    case 'govappoint': case 'govdecree': case 'govaside': {
      // Setting a directive aside is asked twice: it costs the cabinet's trust at once.
      if (act === 'govaside' && ui.modal && ui.modal.ask !== 'aside') { ui.modal.ask = 'aside'; openModal(ui.modal); break; }
      // So is an appointment or a decree: it spends political capital and fixes the office or the law for months.
      if (act !== 'govaside' && ui.modal && t.dataset.sure !== '1') {
        ui.modal.ask = act === 'govappoint' ? `appoint:${t.dataset.seat}:${t.dataset.min}` : `decree:${t.dataset.track}:${t.dataset.step}`;
        openModal(ui.modal);
        document.querySelector('#card .gvseat.asking, #card .gvtrack .note.warn')?.scrollIntoView({ block: 'nearest' });
        break;
      }
      const h = g.headlines.length;
      const ok = act === 'govappoint' ? orderAppoint(g, t.dataset.seat!, t.dataset.min!) : act === 'govdecree' ? orderDecree(g, t.dataset.track!, Number(t.dataset.step)) : orderSetAside(g);
      ui.unread += g.headlines.length - h;
      if (ok) queueSave();
      if (ui.modal) ui.modal.ask = '';
      refresh();
      if (ui.modal?.kind === 'cabinet') openModal(ui.modal);
      break;
    }
    case 'govkeep': if (ui.modal) { ui.modal.ask = ''; openModal(ui.modal); } break;
    case 'govpromise': {
      // Call your shot: from a province, or from the cabinet room, to the list of towns, to the terms, to the word given.
      const reg = t.dataset.id ?? '', mo = t.dataset.months ?? '';
      if (t.dataset.sure !== '1') {
        const next = `promise${reg ? `:${reg}` : ''}${reg && mo ? `:${mo}` : ''}`;
        if (ui.modal?.kind === 'cabinet') { ui.modal.ask = next; openModal(ui.modal); } else { openModal({ kind: 'cabinet', nat: g.human || '', ask: next }); refresh(); }
        document.querySelector('#card #gvvow')?.scrollIntoView({ block: 'start' });
        break;
      }
      const h = g.headlines.length;
      if (orderPromise(g, Number(reg), Number(mo))) queueSave();
      ui.unread += g.headlines.length - h;
      if (ui.modal) ui.modal.ask = '';
      refresh();
      if (ui.modal?.kind === 'cabinet') { openModal(ui.modal); document.querySelector('#card #gvvow')?.scrollIntoView({ block: 'start' }); }
      break;
    }
    case 'govback': case 'govhead': {
      // Taking a promise back, dismissing the general the cabinet named or standing by him: each is asked twice.
      if (ui.modal && t.dataset.sure !== '1') {
        ui.modal.ask = act === 'govback' ? 'back' : `head:${t.dataset.how}`;
        openModal(ui.modal);
        document.querySelector(act === 'govback' ? '#card #gvvow' : '#card #gvhead')?.scrollIntoView({ block: 'start' });
        break;
      }
      const h = g.headlines.length, asked = governmentOf(g)?.head;
      const ok = act === 'govback' ? orderTakeBack(g) : t.dataset.how === 'sack' ? !!asked && orderSack(g, asked.general) : orderStandBy(g);
      ui.unread += g.headlines.length - h;
      if (ok) queueSave();
      if (ui.modal) ui.modal.ask = '';
      refresh();
      if (ui.modal?.kind === 'cabinet') openModal(ui.modal);
      break;
    }
    case 'govshow': openModal(null); showRegion(id); break;
    case 'record': openModal({ kind: 'unit', type: t.dataset.type as UnitKind, id }); break;
    case 'fallen': openModal({ kind: 'fallen', id }); break;
    case 'award': openModal({ kind: 'award', id }); refresh(); break;
    case 'nextaward': { const next = unseen(g)[0]; openModal(next ? { kind: 'award', id: next.id } : null); refresh(); break; }
    case 'cash': {
      const a = awardOf(g, id);
      if (a && orderCash(g, id, t.dataset.how as Cash)) { ui.picked.clear(); queueSave(); }
      refresh();
      openModal({ kind: 'award', id });
      break;
    }
    case 'drafts': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderDrafts(g, c, t.dataset.v === 'take')) queueSave();
      refresh();
      openModal({ kind: 'unit', type: 'corps', id });
      break;
    }
    case 'revive': {
      const f = fallenOf(g, id), at = f ? raiseSite(g, g.human!, f.kind, ui.sel) : -1;
      if (f && at >= 0 && orderBuildCorps(g, f.kind, at, f.id)) { ui.msg = `${f.name} will be raised again at ${g.regions[at].name}.`; queueSave(); if (ui.modal?.kind === 'fallen') openModal(null); }
      refresh();
      break;
    }
    case 'watchpref': { try { localStorage.setItem('tc-watch', watchPref() ? '0' : '1'); } catch { /* storage is off: the choice lasts as long as the default */ } refresh(); break; }
    case 'close': openModal(null); refresh(); break;
    case 'tab':
      ui.tab = t.dataset.tab as typeof ui.tab;
      if (ui.tab === 'reel') ui.unread = 0;
      if (ui.tab === 'spies') ui.spySeen = g.turn;
      ui.spySend = false; ui.spyAsk = -1;
      if (ui.tab === 'battles') { ui.seenReports = g.reports.length; ui.seenSea = g.seaReports.length; ui.seenAir = g.airReports.length; }
      ui.rail = ui.ship = ui.land = ui.rebase = false; ui.airMode = ''; ui.raid = ''; ui.lead = -1;
      refresh();
      break;
    case 'lens': ui.lens = ui.lens === 'worth' ? '' : 'worth'; refresh(); break;
    case 'zin': zoomBy(0.7); break;
    case 'zout': zoomBy(1.4); break;
    case 'next': nextMonth(); break;
    case 'auto':
      if (ui.auto) stopAuto(); else { nextMonth(); ui.auto = window.setInterval(nextMonth, 1500); }
      drawBar();
      break;
    case 'end': {
      // The player's own fleet actions are fought as the month closes, so the dispatches start from here.
      const since = { h: g.headlines.length, r: g.reports.length, s: g.seaReports.length, a: g.airReports.length, shore: g.shoreReports.length };
      if (onEve(g)) endEve(g); else { judgeGoals(g, g.human!); endHuman(g); }
      advanceToHuman(true, since);
      break;
    }
    case 'pick': {
      if (ui.picked.has(id)) ui.picked.delete(id); else ui.picked.add(id);
      ui.rail = ui.ship = ui.land = false; refresh(); break;
    }
    case 'pickall': selectRegion(ui.sel); refresh(); break;
    case 'joinarmy': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderJoin(g, c)) { ui.msg = `${c.name} marches with your army at ${g.regions[c.reg].name}.`; selectRegion(c.reg); queueSave(); }
      refresh(); break;
    }
    case 'joinall': {
      const n = ui.sel >= 0 ? orderJoinAll(g, ui.sel) : 0;
      if (n) { ui.msg = `${n} allied corps ${n === 1 ? 'joins' : 'join'} your army at ${g.regions[ui.sel].name}.`; selectRegion(ui.sel); queueSave(); }
      refresh(); break;
    }
    case 'sendback': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderSendBack(g, c)) { ui.picked.delete(c.id); ui.msg = `${c.name} goes back to ${g.nations[c.nat].adj} command.`; queueSave(); }
      refresh(); break;
    }
    case 'sendbackall': {
      let n = 0;
      if (ui.sel >= 0) for (const c of [...g.regions[ui.sel].units]) if (c.joint === g.human && orderSendBack(g, c)) { ui.picked.delete(c.id); n++; }
      if (n) { ui.msg = `${n} allied corps ${n === 1 ? 'goes' : 'go'} back to ${n === 1 ? 'its' : 'their'} own command.`; queueSave(); }
      refresh(); break;
    }
    case 'job': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderEngineers(g, c, t.dataset.key as JobKind)) { ui.msg = `${c.name}: ${jobLine(g, c).replace(/^./, (ch) => ch.toLowerCase())}.`; queueSave(); }
      refresh(); break;
    }
    case 'stopjob': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderStopWork(g, c)) { ui.msg = `${c.name} has stopped work and has its moves back.`; queueSave(); }
      refresh(); break;
    }
    case 'blow': {
      const from = Number(t.dataset.from), to = Number(t.dataset.to);
      if (orderBlowBridges(g, from, to)) { ui.msg = `The bridges between ${g.regions[from].name} and ${g.regions[to].name} are blown.`; queueSave(); }
      else ui.msg = whyNotBlowBridges(g, from, to) ?? '';
      refresh(); break;
    }
    case 'wreckask': ui.wreckAsk = `${id}:${t.dataset.kind}`; refresh(); break;
    case 'wreckno': ui.wreckAsk = ''; refresh(); break;
    case 'polopen': ui.polOpen = !ui.polOpen; refresh(); break;
    case 'wreck': {
      const kind = t.dataset.kind as PrizeKind, why = whyNotWreckPrize(g, id, kind);
      ui.wreckAsk = '';
      if (orderWreck(g, id, kind)) { ui.msg = `${PRIZES.names[kind]} at ${g.regions[id].name}: wrecked. ${kind === 'junction' ? 'No train runs through until engineers relay the line.' : `It is out of use for ${PRIZES.wreck[kind]} months.`}`; queueSave(); }
      else ui.msg = why ?? '';
      refresh(); break;
    }
    case 'burnorder': {
      const v = Number(t.dataset.v);
      if (orderBurn(g, id, v)) { ui.msg = v > 0 ? `If ${g.regions[id].name} is lost, your troops burn what they hold there.` : v < 0 ? `If ${g.regions[id].name} is lost, your troops leave it whole.` : `${g.regions[id].name} follows your scorched earth policy.`; queueSave(); }
      refresh(); break;
    }
    case 'scorch': {
      const p = t.dataset.v as ScorchPolicy;
      if (orderScorchPolicy(g, p)) { ui.msg = p === 'never' ? 'Scorched earth policy: your troops burn nothing they leave.' : p === 'conquered' ? 'Scorched earth policy: your troops burn what they leave on conquered ground.' : 'Scorched earth policy: your troops burn what they leave, at home too.'; queueSave(); }
      refresh(); break;
    }
    case 'cutrail': {
      if (orderCutRailway(g, id)) { ui.msg = `The railway at ${g.regions[id].name} is torn up.`; queueSave(); }
      else ui.msg = whyNotCutRailway(g, id) ?? '';
      refresh(); break;
    }
    case 'eveplan': {
      const moved = orderEvePlan(g);
      ui.sel = -1; ui.picked.clear(); ui.rail = ui.ship = ui.land = false;
      ui.msg = moved
        ? `${moved} corps sent by rail as the staff advised. Now give each army its orders: click an army, then Stand, Dig in or Fall back.`
        : 'The army is already where the staff wants it, or has made its move for the eve.';
      refresh(); break;
    }
    case 'ifattacked': {
      const a = trayArmy;
      if (!a || a.nat !== g.human) break;
      const posture = t.dataset.v as Posture;
      const at = g.regions[a.reg].name;
      const told = g.regions[a.reg].units.filter((c) => c.nat === a.nat && orderStance(g, c, posture)).length;
      if (told) ui.msg = posture === 'back'
        ? `The army at ${at} will give ground if the enemy attacks with the odds to carry the position.`
        : posture === 'dig'
          ? `The army at ${at} is digging in. Leave it where it is this month and it fights from fieldworks from the next.`
          : autoDig() ? `The army at ${at} will stand and fight where it is. Left there for the month, it digs in.` : `The army at ${at} will stand and fight where it is.`;
      refresh(); break;
    }
    case 'unpick': ui.picked.clear(); ui.rail = ui.ship = ui.land = false; refresh(); break;
    case 'rail': ui.rail = !ui.rail; ui.ship = ui.land = false; refresh(); break;
    case 'ship': ui.ship = !ui.ship; ui.rail = ui.land = false; refresh(); break;
    case 'land': ui.land = !ui.land; ui.rail = ui.ship = false; refresh(); break;
    case 'fleet': {
      const f = g.fleets.find((x) => x.id === id);
      ui.fleet = ui.fleet === id ? -1 : id;
      ui.rebase = false;
      if (f && ui.fleet === id) { ui.stance = sailingAs(f); ui.sel = -1; ui.sea = -1; ui.picked.clear(); }
      refresh(); break;
    }
    case 'stance': {
      ui.stance = t.dataset.stance as Stance;
      const f = myFleet();
      // A fleet at sea changes its work where it is. One in harbour, or bound to a lane, waits to be told which sea.
      if (f && atSea(f) && f.stance !== 'escort') orderFleet(g, f.id, f.zone, ui.stance);
      else if (f) ui.msg = `Click a ringed sea: the ${f.name} will ${ui.stance === 'raid' ? 'raid shipping' : ui.stance === 'hunt' ? 'hunt submarines' : 'patrol'} there.`;
      refresh(); break;
    }
    case 'fleetsec': {
      ui.tab = 'navy';
      refresh();
      document.getElementById(t.dataset.sec ?? '')?.scrollIntoView({ block: 'start' });
      break;
    }
    case 'escort': {
      const f = myFleet();
      const lane = t.dataset.lane ?? '';
      if (f && orderEscort(g, f.id, lane)) ui.msg = `The ${f.name} sails with the convoys on ${lanesToGuard(g).find((l) => l.id === lane)?.name ?? 'the lane'}.`;
      else if (f) ui.msg = `The ${f.name} cannot escort that lane.`;
      ui.rebase = false;
      refresh(); break;
    }
    case 'shell': { const f = myFleet(); if (f) openModal({ kind: 'shell', fleet: f.id, target: id }); break; }
    case 'shellwhat': { const m = ui.modal; if (m && m.kind === 'shell') openModal({ ...m, what: t.dataset.what }); break; }
    case 'doshell': {
      const m = ui.modal;
      if (!m || m.kind !== 'shell') break;
      const plan = previewBombard(g, m.fleet, m.target, m.what);
      const rep = plan && !plan.why ? orderBombard(g, m.fleet, m.target, plan.what) : null;
      refresh();
      if (rep) openModal({ kind: 'shorereport', id: rep.id }); else { ui.msg = 'The bombardment could not be made.'; openModal(null); refresh(); }
      break;
    }
    case 'shorereport': openModal({ kind: 'shorereport', id }); break;
    case 'fleetsplit': {
      const f = myFleet();
      if (f) openModal({ kind: 'split', fleet: f.id, take: { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 } });
      break;
    }
    case 'splitadj': {
      const m = ui.modal;
      const f = m && m.kind === 'split' ? g.fleets.find((x) => x.id === m.fleet) : null;
      if (!m || !f) break;
      const c = t.dataset.cls as ShipClass;
      const take = { ...(m.take as Ships) };
      take[c] = Math.max(0, Math.min(f.ships[c], take[c] + Number(t.dataset.d)));
      openModal({ ...m, take });
      break;
    }
    case 'dosplit': {
      const m = ui.modal;
      if (!m || m.kind !== 'split') break;
      const made = orderSplit(g, m.fleet, m.take as Ships);
      openModal(null);
      if (made >= 0) { pickFleet(made); ui.msg = `${g.fleets.find((x) => x.id === made)?.name ?? 'The squadron'} forms. Give it its orders.`; }
      refresh(); break;
    }
    case 'fleetjoin': {
      const f = myFleet();
      const into = g.fleets.find((x) => x.id === id);
      if (f && into && orderMerge(g, f.id, id)) { ui.msg = `The ships of the ${f.name} join the ${into.name}.`; pickFleet(id); }
      refresh(); break;
    }
    case 'sail': {
      const f = myFleet();
      if (f && !orderFleet(g, f.id, Number(t.dataset.zone), ui.stance)) ui.msg = `The ${f.name} cannot reach that sea.`;
      ui.rebase = false;
      refresh(); break;
    }
    case 'toport': { const f = myFleet(); if (f) orderFleet(g, f.id, f.home, 'port'); ui.rebase = false; refresh(); break; }
    case 'rebase': ui.rebase = !ui.rebase; refresh(); break;
    case 'showsea': openModal(null); ui.sea = id; ui.sel = -1; ui.picked.clear(); ui.tab = 'region'; focusOnPoint(g.seas[id].x, g.seas[id].y); refresh(); break;
    case 'seareport': openModal({ kind: 'seareport', id }); break;
    case 'airfleet': {
      ui.air = ui.air === id ? -1 : id;
      ui.airMode = ''; ui.raid = ''; ui.lead = -1;
      const f = g.airFleets.find((x) => x.id === ui.air);
      if (f) { ui.sel = -1; ui.sea = -1; ui.picked.clear(); if (airWar(g)) ui.field = f.ship !== undefined ? -1 : f.base; }
      refresh(); break;
    }
    case 'wing': {
      // From a fleet's tray or its orders: the Air tab opens on that wing, with the strike planner ready where it can fly.
      const w = g.airFleets.find((x) => x.id === id);
      if (!w) break;
      ui.tab = 'air'; ui.air = id; ui.field = -1; ui.airMode = ''; ui.sel = -1; ui.sea = -1; ui.picked.clear(); ui.rebase = false;
      const can = humanUp(g) && w.bmr > 0 && w.flew !== g.turn && g.nations[w.nat].atWar.length > 0;
      ui.raid = can ? 'menu' : ''; ui.lead = can ? id : -1;
      refresh();
      document.querySelector(can ? '#pane .planner' : '#pane .grp.pick')?.scrollIntoView({ block: 'nearest' });
      break;
    }
    case 'fleetgo': {
      const f = g.fleets.find((x) => x.id === id);
      if (!f) break;
      pickFleet(id);
      const at = atSea(f) ? g.seas[f.zone] : g.regions[f.base] ?? g.seas[f.home];
      if (at) focusOnPoint(at.x, at.y);
      break;
    }
    case 'searchmode': ui.airMode = ui.airMode === 'search' ? '' : 'search'; ui.raid = ''; ui.lead = -1; refresh(); break;
    case 'searchpick': {
      const f = myAir();
      if (!f) break;
      const res = orderSearch(g, f.id, Number(t.dataset.zone));
      ui.msg = res.text;
      if (res.ok) ui.airMode = '';
      refresh(); break;
    }
    case 'airfield': openModal(null); openAirfield(id); focusOnPoint(g.regions[id].x, g.regions[id].y); break;
    case 'groupjob': {
      const f = myAir();
      if (f && !orderGroupJob(g, f.id, t.dataset.key ?? '')) ui.msg = 'That order could not be given.';
      refresh(); break;
    }
    case 'raidmenu': {
      const open = ui.raid && ui.lead === id;
      ui.raid = open ? '' : 'menu'; ui.lead = open ? -1 : id; ui.airMode = '';
      if (id >= 0 && !open) ui.air = id;
      refresh();
      if (!open) document.querySelector('#pane .planner')?.scrollIntoView({ block: 'nearest' });
      break;
    }
    case 'raidkind': { const k = t.dataset.kind as RaidKind; ui.raid = ui.raid === k ? 'menu' : k; ui.airMode = ''; refresh(); break; }
    case 'raidtarget': if (ui.raid && ui.raid !== 'menu') openModal({ kind: 'raid', raid: ui.raid, target: Number(t.dataset.target), ids: ui.lead >= 0 ? [ui.lead] : undefined }); break;
    case 'raidtick': {
      const m = ui.modal;
      if (!m || m.kind !== 'raid') break;
      const ids: number[] = m.ids ?? [];
      m.ids = ids.includes(id) ? ids.filter((x) => x !== id) : [...ids, id];
      const at = $('#card').scrollTop;
      openModal(m);
      $('#card').scrollTop = at;
      break;
    }
    case 'doraid': {
      const m = ui.modal;
      if (!m || m.kind !== 'raid') break;
      const rep = orderRaid(g, m.raid, m.target, m.ids ?? []);
      if (!rep) { ui.msg = 'The raid could not be flown as picked.'; openModal(null); refresh(); break; }
      ui.raid = ''; ui.lead = -1; ui.airMode = '';
      ui.seenAir = g.airReports.length;
      openModal({ kind: 'airreport', id: rep.id });
      refresh(); break;
    }
    case 'scoutmode': ui.airMode = ui.airMode === 'scout' ? '' : 'scout'; ui.raid = ''; ui.lead = -1; refresh(); break;
    case 'scoutnow': {
      // Straight from the province panel or the card that plans an attack: the nearest group flies it.
      const res = orderScout(g, Number(t.dataset.fleet), id);
      ui.msg = res.text;
      const top = $('#card').scrollTop, open = ui.modal;
      refresh();
      if (open?.kind === 'attack') { openModal(open); $('#card').scrollTop = top; }
      break;
    }
    case 'scoutpick': {
      const f = myAir();
      if (!f) break;
      const res = orderScout(g, f.id, id);
      ui.msg = res.text;
      if (res.ok) ui.airMode = '';
      refresh(); break;
    }
    case 'basepick': {
      const f = myAir();
      if (!f) break;
      const why = whyNotBase(g, f, g.regions[id]);
      if (why) ui.msg = `${f.name} cannot move to ${g.regions[id].name}. ${why}`;
      else { orderAirBase(g, f.id, id); ui.airMode = ''; ui.field = id; }
      refresh(); break;
    }
    case 'mission': {
      const f = myAir();
      const m = t.dataset.mission as AirMission;
      if (!f) break;
      if (isOffensive(m)) ui.airMode = ui.airMode === m ? '' : m;
      else { orderAir(g, f.id, m); ui.airMode = ''; }
      refresh(); break;
    }
    case 'airbase': ui.airMode = ui.airMode === 'base' ? '' : 'base'; ui.raid = ''; ui.lead = -1; refresh(); break;
    case 'airtarget': {
      const f = myAir();
      if (f && ui.airMode && ui.airMode !== 'base') openModal({ kind: 'airplan', fleet: f.id, mission: ui.airMode, target: Number(t.dataset.target) });
      break;
    }
    case 'doair': {
      const m = ui.modal!;
      if (orderAir(g, m.fleet, m.mission, m.target)) ui.airMode = ''; else ui.msg = 'That order could not be given.';
      openModal(null); refresh(); break;
    }
    case 'airreport': openModal({ kind: 'airreport', id }); break;
    case 'doland': {
      const m = ui.modal!;
      const h = g.headlines.length;
      const before = g.regions.map((r) => r.ctrl);
      const res = orderLand(g, g.corps.filter((c) => m.corps.has(c.id)), m.target);
      ui.unread += g.headlines.length - h;
      ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
      ui.land = false;
      if (!res.ok) { ui.msg = res.why ?? ''; openModal(null); refresh(); break; }
      ui.picked.clear();
      ui.sel = m.target; ui.sea = -1;
      if (res.report) { ui.seenReports = g.reports.length; refresh(); openModal({ kind: 'report', id: res.report.id }); }
      else { openModal(null); refresh(); }
      break;
    }
    case 'arm': ui.arm = t.dataset.arm as typeof ui.arm; refresh(); break;
    case 'armgo': ui.arm = t.dataset.arm as typeof ui.arm; ui.tab = 'build'; refresh(); break;
    case 'arsenal': {
      const b = orderArsenal(g, t.dataset.key!, ui.sel);
      if (b) { ui.msg = `Ordered: ${b.name}. Ready ${date(b.eta)}.`; queueSave(); }
      refresh();
      break;
    }
    case 'fortify': if (ui.sel >= 0) orderBuildFort(g, ui.sel); refresh(); break;
    case 'cancelbuild': orderCancelBuild(g, id); refresh(); break;
    case 'teams': orderTeams(g, t.dataset.field as Field, Number(t.dataset.n)); refresh(); break;
    case 'bureau': orderBureau(g); refresh(); break;
    case 'court': { const h = g.headlines.length; orderCourt(g, t.dataset.nat!); ui.unread += g.headlines.length - h; refresh(); break; }
    case 'evac': {
      for (const c of pickedCorps()) orderEvacuate(g, c);
      ui.picked.clear(); refresh(); break;
    }
    case 'report': openModal({ kind: 'report', id }); break;
    case 'general': openModal({ kind: 'general', id }); break;
    case 'appoint': openModal({ kind: 'appoint', corps: id }); break;
    case 'assign': orderAssign(g, id, Number(t.dataset.corps)); refresh(); openModal({ kind: 'general', id }); break;
    case 'relieve': orderRelieve(g, id); refresh(); openModal({ kind: 'general', id }); break;
    case 'skill': orderSkill(g, id, t.dataset.skill as Skill); refresh(); openModal({ kind: 'general', id }); break;
    case 'keep': openModal({ kind: 'general', id }); break;
    case 'sack': {
      if (!ui.modal?.confirm) { openModal({ kind: 'general', id, confirm: true }); break; }
      const h = g.headlines.length;
      orderSack(g, id);
      ui.unread += g.headlines.length - h;
      refresh();
      openModal({ kind: 'general', id });
      break;
    }
    case 'promote': {
      const x = orderPromote(g, id);
      refresh();
      if (x) openModal({ kind: 'general', id: x.id });
      break;
    }
    case 'showmap': openModal(null); ui.sel = id; ui.tab = 'region'; focusOn([id]); refresh(); break;
    case 'declare': openModal({ kind: 'declare' }); break;
    case 'level': openModal({ kind: 'level' }); if (t.dataset.part === 'cabinet') document.querySelector('#card #setcab')?.scrollIntoView({ block: 'start' }); break;
    case 'setcabinet': {
      // Switching it off with business open is asked twice: the bill is shown first.
      if (g.human && t.dataset.on !== '1' && t.dataset.sure !== '1' && !cabinetIsOff(g) && cabinetBillOf(g).length) { openModal({ kind: 'level', ask: 'caboff' }); document.querySelector('#card #setcab')?.scrollIntoView({ block: 'start' }); break; }
      if (g.human && orderCabinet(g, t.dataset.on === '1')) { queueSave(); const keep = $('#card').scrollTop; refresh(); openModal({ kind: 'level' }); $('#card').scrollTop = keep; }
      break;
    }
    case 'setoffice': {
      if (g.human && orderOffice(g, t.dataset.secure !== '1')) { queueSave(); const keep = $('#card').scrollTop; refresh(); openModal({ kind: 'level' }); $('#card').scrollTop = keep; }
      break;
    }
    case 'govtab': ui.govTab = (t.dataset.tab as typeof ui.govTab) ?? 'now'; if (ui.modal) { ui.modal.ask = ''; openModal(ui.modal); } break;
    case 'govgo': {
      // From a directive that names no town to the place where the job is done.
      const to = t.dataset.to;
      if (to === 'trade') { openModal({ kind: 'trade' }); break; }
      openModal(null);
      if (to === 'agent') ui.tab = 'spies'; else { ui.arm = to === 'build' ? 'towns' : 'army'; ui.tab = 'build'; }
      refresh();
      break;
    }
    case 'menearly': {
      // Calling a class up early is a trade, and is asked twice.
      if (t.dataset.sure !== '1') { if (ui.modal) { ui.modal.ask = 'early'; openModal(ui.modal); document.querySelector('#card #lmen')?.scrollIntoView({ block: 'start' }); } break; }
      const h = g.headlines.length;
      if (orderEarlyClass(g)) queueSave();
      ui.unread += g.headlines.length - h;
      if (ui.modal) ui.modal.ask = '';
      refresh();
      if (ui.modal) { openModal(ui.modal); document.querySelector('#card #lmen')?.scrollIntoView({ block: 'start' }); }
      break;
    }
    case 'menhold': {
      const h = g.headlines.length;
      if (orderCallUp(g, t.dataset.hold === '1')) queueSave();
      ui.unread += g.headlines.length - h;
      refresh();
      if (ui.modal) { openModal(ui.modal); document.querySelector('#card #lmen')?.scrollIntoView({ block: 'start' }); }
      break;
    }
    case 'traits': openModal({ kind: 'traits', nat: t.dataset.nat ?? g.human ?? '' }); break;
    case 'tech': showTech(t.dataset.id); break;
    case 'research': if (orderResearch(g, t.dataset.id ?? '')) { refresh(); showTech(t.dataset.id); } break;
    case 'stopresearch': if (orderStopResearch(g, t.dataset.id ?? '')) { refresh(); showTech(t.dataset.id); } break;
    case 'setlevel': { if (g.human && setLevel(g, t.dataset.level ?? '')) { const keep = $('#card').scrollTop; refresh(); openModal({ kind: 'level' }); $('#card').scrollTop = keep; } break; }
    case 'askdeclare': openModal({ kind: 'invade', on: t.dataset.nat!, target: -1 }); break;
    case 'path': openModal({ kind: 'path', ask: t.dataset.ask ?? '' }); break;
    case 'lastact': g.flags['dom:seen'] ??= g.turn; openModal({ kind: 'lastact', ask: t.dataset.ask === '1' }); break;
    case 'keeppeace': if (orderKeepPeace(g)) { refresh(); openModal({ kind: 'lastact' }); } break;
    case 'domination': {
      const h = g.headlines.length;
      if (orderDomination(g)) {
        ui.unread += g.headlines.length - h;
        ui.tab = 'powers'; ui.sel = -1; ui.picked.clear();
        ui.msg = 'Every alliance is dissolved. Every nation still standing is your enemy, and you are theirs.';
        openModal(null);
        refresh();
      }
      break;
    }
    case 'takepath': {
      const h = g.headlines.length;
      if (orderPath(g, t.dataset.path as PathId, t.dataset.nat || undefined)) { ui.unread += g.headlines.length - h; refresh(); openModal({ kind: 'path' }); }
      break;
    }
    case 'leagueinvite': {
      const h = g.headlines.length;
      const keep = $('#card').scrollTop;
      if (orderLeagueInvite(g, t.dataset.nat!)) { ui.unread += g.headlines.length - h; refresh(); if (ui.modal?.kind === 'path') { openModal({ kind: 'path' }); $('#card').scrollTop = keep; } }
      break;
    }
    case 'dodeclare': {
      const h = g.headlines.length;
      const to = Number(t.dataset.target ?? -1);
      const done = orderDeclare(g, t.dataset.nat!);
      ui.unread += g.headlines.length - h;
      openModal(null);
      refresh();
      // The army that was sent over the frontier goes now: into the attack planner if the ground is held, or straight in.
      if (done && to >= 0) orderAt(to);
      break;
    }
    case 'atkpick': {
      const m = ui.modal!;
      if (m.chosen.has(id)) m.chosen.delete(id); else m.chosen.add(id);
      $('#card').innerHTML = attackCard(); break;
    }
    case 'probe': {
      // One corps feels out the line: the card is read again with what it found.
      const m = ui.modal!;
      const target = g.regions[m.target];
      const who = g.corps.find((x) => x.id === id);
      const h = g.headlines.length;
      const rep = who ? orderProbe(g, who, target) : null;
      if (rep) { m.probe = rep; ui.unread += g.headlines.length - h; queueSave(); refresh(); }
      $('#card').innerHTML = attackCard(); break;
    }
    case 'bombline': {
      // The raid planner, on this line: the attack is ordered again when the bombers are back.
      const m = ui.modal!;
      openModal({ kind: 'raid', raid: 'line', target: m.target });
      break;
    }
    case 'foldcard': {
      if (g.regions[id]) openModal({ kind: 'fold', reg: id });
      break;
    }
    case 'fold': {
      const keep = Number(t.dataset.keep), gone = Number(t.dataset.gone);
      const a = g.corps.find((x) => x.id === keep), b = g.corps.find((x) => x.id === gone);
      const h = g.headlines.length;
      if (a && b && orderFold(g, keep, gone)) {
        ui.msg = `${b.name} is folded into ${a.name}. Its cadre waits in the arsenal.`;
        ui.unread += g.headlines.length - h;
        ui.picked.delete(gone);
        queueSave();
      }
      openModal(null);
      refresh(); break;
    }
    case 'atkmine': {
      // Back to the corps that were picked when the attack was ordered.
      const m = ui.modal!;
      if (m.picked) m.chosen = new Set(m.picked as number[]);
      m.picked = null;
      $('#card').innerHTML = attackCard(); break;
    }
    case 'ticketall': {
      // Every line of the Battle Ticket, on a screen where it opens folded.
      ui.ticketAll = true;
      document.querySelectorAll('table.ticket.fold').forEach((t) => t.classList.remove('fold'));
      document.querySelectorAll('.ticketall').forEach((b) => b.remove());
      break;
    }
    case 'atkall': {
      // Everyone in reach: the staff trims the force to what the roads will carry.
      const m = ui.modal!;
      m.chosen = new Set(candidates(g, g.regions[m.target]).map((c) => c.id));
      $('#card').innerHTML = attackCard(); break;
    }
    case 'storm': case 'sally': {
      // The card that plans it comes first: nothing is ordered until the player has seen the odds.
      if (g.regions[id]) openModal({ kind: 'siege', mode: act, target: id });
      break;
    }
    case 'dosiege': {
      const m = ui.modal;
      if (!m || m.kind !== 'siege') break;
      const target = g.regions[m.target];
      const h = g.headlines.length;
      const before = g.regions.map((r) => r.ctrl);
      const rep = m.mode === 'sally' ? orderSally(g, target.id) : orderStorm(g, target.id);
      ui.unread += g.headlines.length - h;
      ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
      if (rep) {
        ui.seenReports = g.reports.length;
        selectRegion(target.id);
        queueSave();
        refresh();
        openModal({ kind: 'report', id: rep.id });
      } else { refresh(); openModal(null); }
      break;
    }
    case 'garrison': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderGarrison(g, c)) { ui.picked.delete(c.id); ui.msg = `${c.name} goes into ${townName(g.regions[c.reg])}. It fights from behind the walls until you order it out.`; queueSave(); }
      else if (c) ui.msg = whyNotGarrisonTown(g, c) ?? '';
      refresh(); break;
    }
    case 'takefield': {
      const c = g.corps.find((x) => x.id === id);
      if (c && orderTakeField(g, c)) { ui.msg = `${c.name} comes out of ${townName(g.regions[c.reg])} and takes the field.`; queueSave(); }
      else if (c) ui.msg = whyNotLeaveTown(g, c) ?? '';
      refresh(); break;
    }
    case 'raidtown': {
      if (g.regions[id]) openModal({ kind: 'raid', raid: 'town', target: id });
      break;
    }
    case 'attack': {
      const m = ui.modal!;
      const target = g.regions[m.target];
      const h = g.headlines.length;
      const before = g.regions.map((r) => r.ctrl);
      const rep = orderAttack(g, g.corps.filter((c) => m.chosen.has(c.id)), target);
      ui.unread += g.headlines.length - h;
      ui.flash = g.regions.filter((r, i) => r.ctrl !== before[i]).map((r) => r.id);
      if (rep) {
        ui.seenReports = g.reports.length;
        if (rep.taken) { ui.sel = target.id; ui.picked = new Set(target.units.filter((c) => canOrder(g, c) && c.ap > 0).map((c) => c.id)); }
        refresh();
        openModal({ kind: 'report', id: rep.id });
      } else openModal(null);
      break;
    }
  }
}

/** The keyboard: Escape backs out of whatever is open, Enter ends the month, Space skips the watching. */
function onKey(e: KeyboardEvent): void {
  const el = e.target as HTMLElement | null;
  if (e.ctrlKey || e.metaKey || e.altKey || (el && /^(INPUT|TEXTAREA|SELECT)$/.test(el.tagName))) return;
  if (e.key === 'Escape') {
    if (ui.modal) { if (ui.modal.kind !== 'start' || ui.live) { openModal(null); refresh(); } }
    else if (ui.picked.size || ui.sel >= 0) { ui.picked.clear(); ui.sel = -1; ui.rail = ui.ship = ui.land = false; ui.msg = ''; refresh(); }
    return;
  }
  // A key pressed on a button belongs to that button.
  if (el && el.tagName === 'BUTTON') return;
  if (e.key === ' ' && passing) { e.preventDefault(); endPassing(); }
  else if (e.key === 'Enter' && !ui.modal && !passing && g.human && humanUp(g)) { e.preventDefault(); ($('#barbtns').querySelector('[data-act="end"]') as HTMLElement | null)?.click(); }
}

function selectRegion(id: number): void {
  ui.sel = id;
  ui.sea = -1;
  ui.rail = ui.ship = ui.land = false;
  ui.picked = new Set(humanUp(g) ? g.regions[id].units.filter((c) => canOrder(g, c) && c.ap > 0).map((c) => c.id) : []);
  ui.tab = 'region';
}

function onRegion(id: number): void {
  const r = g.regions[id];
  ui.flash = [];
  ui.msg = '';
  // The raid planner is open on a kind of raid: a click on the map is its target.
  if (ui.tab === 'air' && airWar(g) && humanUp(g) && ui.raid && ui.raid !== 'menu') {
    const lead = ui.lead >= 0 ? [ui.lead] : undefined;
    if (ui.raid === 'strike') ui.msg = 'A strike on shipping needs a sea: click one of the ringed seas.';
    else if (ui.raid === 'fleet') ui.msg = 'A strike on a fleet needs the fleet: click a ringed enemy fleet or its sea, or pick it from the list.';
    else if (raidTargetsFor(g, ui.raid, ui.lead).some((x) => !x.sea && x.id === id)) { openModal({ kind: 'raid', raid: ui.raid, target: id, ids: lead }); return; }
    else ui.msg = `${r.name}: ${previewRaid(g, ui.raid, id, lead).why || 'there is nothing there for this raid.'}`;
    refresh();
    return;
  }
  // Under the Air tab with a fleet picked and an order half given, a click on the map finishes it.
  const af = ui.tab === 'air' ? myAir() : null;
  if (af && ui.airMode) {
    if (ui.airMode === 'base') {
      const why = whyNotBase(g, af, r);
      if (why) ui.msg = `${af.name} cannot move to ${r.name}. ${why}`;
      else { orderAirBase(g, af.id, id); ui.airMode = ''; if (airWar(g)) ui.field = id; }
    } else if (ui.airMode === 'strike') ui.msg = 'A strike on shipping needs a sea: click one of the ringed seas.';
    else if (ui.airMode === 'search') ui.msg = 'A search needs a sea: click one of the ringed seas.';
    else if (ui.airMode === 'scout') {
      const res = orderScout(g, af.id, id);
      ui.msg = res.text;
      if (res.ok) { ui.airMode = ''; ui.sel = id; }
    } else {
      const why = whyNotMission(g, af, ui.airMode, id);
      if (!why) { openModal({ kind: 'airplan', fleet: af.id, mission: ui.airMode, target: id }); return; }
      ui.msg = `${r.name}: ${why}`;
    }
    refresh();
    return;
  }
  // Under the Air tab a province with an airfield opens the airfield.
  if (ui.tab === 'air' && airWar(g) && g.human && airfieldsSeen(g).some((a) => a.region === id)) { openAirfield(id); return; }
  // Under the Spies tab with an agent waiting to be sent, a click on the map is where it goes.
  if (ui.tab === 'spies' && ui.spySend && ui.spy >= 0) {
    const a = secretService(g)?.agents.find((x) => x.id === ui.spy);
    const why = whyNotSendAgent(g, ui.spy, id);
    if (why) ui.msg = why;
    else if (orderSendAgent(g, ui.spy, id)) { ui.spySend = false; ui.msg = a ? `Agent ${a.name} sets out for ${r.name} and will be at work there next month.` : ''; }
    refresh();
    return;
  }
  // Under the Navy tab with a fleet picked, a click on a port is a change of base.
  const fl = ui.tab === 'navy' ? myFleet() : null;
  if (fl && ui.rebase) {
    if (orderRebase(g, fl.id, id)) ui.rebase = false;
    else ui.msg = `${r.name} cannot be the fleet's base: it has to be a friendly port the ships can steam to.`;
    refresh();
    return;
  }
  // With a fleet picked up, a click on a coast under its guns plans a bombardment.
  if (fl && shellAt.includes(id)) { openModal({ kind: 'shell', fleet: fl.id, target: id }); return; }
  // While planning production the map only chooses where to build. No army moves by accident.
  if (ui.tab === 'build') {
    ui.sel = id; ui.sea = -1; ui.picked.clear(); ui.rail = ui.ship = false;
    refresh();
    return;
  }
  const picked = pickedCorps();
  if (picked.length && ui.land) {
    const plan = previewLanding(g, picked, id);
    if (plan.why) { ui.msg = plan.why; refresh(); return; }
    openModal({ kind: 'landing', target: id, corps: new Set(picked.map((c) => c.id)) });
    return;
  }
  if (picked.length && (ui.rail || ui.ship)) {
    for (const c of picked) { if (ui.rail) orderRail(g, c, id); else orderShip(g, c, id); }
    selectRegion(id);
    ui.picked.clear();
    ui.tab = 'region';
    refresh();
    return;
  }
  // A click on another of the player's armies picks that one up: sending this one to join it takes a right-click.
  if (picked.length && id !== ui.sel && !r.units.some((c) => canOrder(g, c) && c.ap > 0) && orderAt(id)) return;
  selectRegion(id);
  refresh();
}

// ---- Moving around the map ----------------------------------------------------------------

function applyView(): void {
  const svg = $('#map');
  const a = svg.clientHeight / Math.max(1, svg.clientWidth);
  const v = ui.view;
  v.w = Math.max(150, Math.min(2100, v.w));
  const h = v.w * a;
  v.x = Math.max(-200, Math.min(g.width + 200 - v.w, v.x));
  // The map ends in the Sahara: there is nothing to scroll to below it.
  v.y = Math.max(-200, Math.min(g.height - h, v.y));
  svg.setAttribute('viewBox', `${v.x.toFixed(1)} ${v.y.toFixed(1)} ${v.w.toFixed(1)} ${h.toFixed(1)}`);
  svg.classList.toggle('near', v.w < 600);
  svg.classList.toggle('mid', v.w < 760);
}

function zoomBy(f: number, cx?: number, cy?: number): void {
  const svg = $('#map');
  const rect = svg.getBoundingClientRect();
  const a = rect.height / Math.max(1, rect.width);
  const v = ui.view;
  const px = cx === undefined ? 0.5 : (cx - rect.left) / rect.width;
  const py = cy === undefined ? 0.5 : (cy - rect.top) / rect.height;
  const mx = v.x + px * v.w, my = v.y + py * v.w * a;
  v.w = Math.max(150, Math.min(2100, v.w * f));
  v.x = mx - px * v.w; v.y = my - py * v.w * a;
  applyView();
}

function wireMap(): void {
  const svg = $('#map');
  const pts = new Map<number, { x: number; y: number }>();
  let start: { x: number; y: number; vx: number; vy: number } | null = null;
  let moved = false, pinch = 0;
  // On a touch screen there is no right button: pressing and holding a region gives the order instead.
  let hold = 0, held = false;
  const regionAt = (x: number, y: number): number => {
    const el = document.elementFromPoint(x, y)?.closest('.reg') as HTMLElement | null;
    return el ? Number(el.dataset.id) : -1;
  };
  svg.addEventListener('contextmenu', (e) => {
    e.preventDefault();
    const id = regionAt(e.clientX, e.clientY);
    if (id >= 0 && humanUp(g)) { ui.msg = ''; if (!orderAt(id) && pickedCorps().length && id !== ui.sel) { ui.msg = pickedCorps().map((c) => (g.regions[c.reg].nb.includes(id) ? whyBlocked(g, c, g.regions[id]) : barred(g, c, g.regions[id]) ? whyBlocked(g, c, g.regions[id]) : null)).find(Boolean) ?? `${g.regions[id].name} is out of reach for the army you have picked up this month.`; refresh(); } }
  });
  svg.addEventListener('pointerdown', (e) => {
    if (e.pointerType === 'mouse' && e.button !== 0) return;
    held = false;
    clearTimeout(hold);
    if (e.pointerType !== 'mouse') {
      const x = e.clientX, y = e.clientY;
      hold = window.setTimeout(() => {
        if (moved || pts.size !== 1 || !humanUp(g)) return;
        const id = regionAt(x, y);
        if (id >= 0 && orderAt(id)) held = true;
      }, 520);
    }
    pts.set(e.pointerId, { x: e.clientX, y: e.clientY });
    try { svg.setPointerCapture(e.pointerId); } catch { /* not all browsers */ }
    if (pts.size === 1) { start = { x: e.clientX, y: e.clientY, vx: ui.view.x, vy: ui.view.y }; moved = false; }
    else if (pts.size === 2) { const [p, q] = [...pts.values()]; pinch = Math.hypot(p.x - q.x, p.y - q.y); moved = true; }
  });
  svg.addEventListener('pointermove', (e) => {
    const rect = svg.getBoundingClientRect();
    if (pts.has(e.pointerId)) pts.set(e.pointerId, { x: e.clientX, y: e.clientY });
    if (pts.size === 2) {
      const [p, q] = [...pts.values()];
      const d = Math.hypot(p.x - q.x, p.y - q.y);
      if (pinch > 0 && d > 0) zoomBy(pinch / d, (p.x + q.x) / 2, (p.y + q.y) / 2);
      pinch = d;
      return;
    }
    if (pts.size === 1 && start) {
      const dx = e.clientX - start.x, dy = e.clientY - start.y;
      if (Math.abs(dx) + Math.abs(dy) > 5) { moved = true; clearTimeout(hold); svg.classList.add('drag'); }
      if (moved) { const k = ui.view.w / rect.width; ui.view.x = start.vx - dx * k; ui.view.y = start.vy - dy * k; applyView(); }
      return;
    }
    // Hover: say what is under the cursor.
    const el = (e.target as Element).closest?.('[data-id]') as HTMLElement | null;
    const tip = $('#tip');
    if (e.pointerType !== 'mouse') { tip.style.display = 'none'; return; }
    drawRoute(el && el.classList.contains('reg') ? Number(el.dataset.id) : -1);
    if (!el) {
      const p = mapPoint(e.clientX, e.clientY);
      const z = seaAt(p.x, p.y);
      if (z < 0) { tip.style.display = 'none'; return; }
      const by = new Map<string, number>();
      for (const f of g.fleets) if (f.zone === z && atSea(f)) by.set(f.nat, (by.get(f.nat) ?? 0) + shipCount(f.ships));
      tip.innerHTML = `<b>${esc(g.seas[z].name)}</b><br>${by.size ? [...by].map(([nat, n]) => `${g.nations[nat].adj}: ${n} ships`).join(' · ') : 'No fleet at sea here'}`;
    } else {
    const r = g.regions[Number(el.dataset.id)];
    const armies = new Map<string, Corps[]>();
    for (const c of r.units) (armies.get(armyOf(c)) ?? armies.set(armyOf(c), []).get(armyOf(c))!).push(c);
    // The fog of war: an army glimpsed is a count, and one unseen is not spoken of.
    const here = [...armies].map(([nat, cs]) => {
      if (!inFull(r, nat)) return seen[r.id] === 1 ? `<br>${esc(g.nations[nat].adj)} troops: ${cs.length} corps, strength unknown` : '';
      if (!friendOf(nat) && !allKnown(cs)) {
        const idd = cs.filter((c) => kn(c) !== 'none').length;
        return `<br>${esc(g.nations[nat].adj)} army: ${cs.length} corps, ${estWords(armyAt(g, r.id, nat)?.est ?? null)} <span class="tipfog">· ${idd ? `${idd} identified` : 'not identified'}</span>`;
      }
      const who = leaderOf(cs);
      const guests = [...new Set(cs.filter((c) => c.nat !== nat).map((c) => g.nations[c.nat].adj))];
      return `<br>${esc(g.nations[nat].adj)} army${who ? ` under ${esc(surname(who.name))}` : ''}: ${cs.length} corps, ${num(sumDivs(cs))} divisions${guests.length ? ` <span class="tipgo">· joint army with ${esc(guests.join(' and '))} troops</span>` : ''}`;
    }).join('') + (fogOn(g) && seen[r.id] === 0 ? '<br><span class="tipfog">Unseen ground: nothing is known of what stands here</span>' : '');
    const way = routeShown === r.id ? routeTo(r.id) : null;
    const order = way ? `<br><span class="tipgo">${way.attack ? 'Right-click to plan an attack' : `Right-click to march: ${way.cost} of ${way.ap} moves`}</span>` : '';
    const rel = relationWord(r.ctrl);
    // The town, and what the ground is worth: to the player if it is the player's own, or as a prize.
    const t = townIn(g, r.id)!;
    const pz = prize(g, r.id);
    const has = t.signs.filter((s) => s !== 'fort').map((s) => (s === 'factory' ? `${t.factories} factor${t.factories === 1 ? 'y' : 'ies'}` : TOWNS.words[s].toLowerCase()));
    const stake = !pz ? (t.worth ? `Worth ${num(t.worth)} a month` : '') : pz.kind === 'yours' ? `Pays you ${num(pz.income)} a month` : pz.kind === 'ally' ? `Pays ${esc(natName(pz.to))} ${num(pz.incomeNow)} a month` : pz.kind === 'restored' ? `Taken, it goes back to ${esc(natName(pz.to))}` : `Worth ${num(pz.income)} a month to you if taken`;
    const town = `<br><span class="tiptown">${esc(t.sizeWord)}${t.name !== t.province ? ` of ${esc(t.name)}` : ''}${has.length ? ` · ${esc(has.join(', '))}` : ''}</span>${stake ? `<br><span class="tiptown gold">${stake}</span>` : ''}`;
    tip.innerHTML = `<b>${esc(r.name)}</b><br>${natName(r.ctrl)} <span class="tiprel ${rel.kind}">${rel.word}</span>${r.ctrl !== r.owner ? ` (occupied ${g.nations[r.owner].adj} ground)` : ''} · ${r.terrain}${steep(g, r) ? ', too steep for tanks' : ''}${town}${here}${order}`;
    }
    tip.style.display = 'block';
    const wrap = $('#mapwrap').getBoundingClientRect();
    tip.style.left = Math.min(wrap.width - 250, e.clientX - wrap.left + 14) + 'px';
    tip.style.top = Math.min(wrap.height - 100, e.clientY - wrap.top + 14) + 'px';
  });
  const up = (e: PointerEvent) => {
    const was = pts.size;
    pts.delete(e.pointerId);
    clearTimeout(hold);
    svg.classList.remove('drag');
    if (was === 1 && !moved && !held && e.type === 'pointerup') {
      $('#tip').style.display = 'none';
      if (e.pointerType !== 'mouse') ghostUntil = performance.now() + 420;
      const under = document.elementFromPoint(e.clientX, e.clientY);
      const el = under?.closest('[data-id]') as HTMLElement | null;
      // One of the player's fleets is a piece: a click picks it up, unless the map is waiting for a place to finish another order.
      const piece = under?.closest('[data-fleets]') as HTMLElement | null;
      const busy = ui.rail || ui.ship || ui.land || ui.spySend || ui.tab === 'build' || (ui.tab === 'air' && (!!ui.airMode || (!!ui.raid && ui.raid !== 'menu')));
      const foe = under?.closest('[data-foe]') as HTMLElement | null;
      if (foe && ui.tab === 'air' && ui.raid === 'fleet') {
        // An enemy fleet that can be struck: one under the marker opens the planner, several are picked from the list.
        const ids = (foe.dataset.foe ?? '').split(',').map(Number);
        if (ids.length === 1) openModal({ kind: 'raid', raid: 'fleet', target: ids[0], ids: ui.lead >= 0 ? [ui.lead] : undefined });
        else { ui.msg = `${ids.length} enemy forces are under that marker: pick the one to strike from the list.`; refresh(); }
      }
      else if (piece && !busy) pickFleetMark((piece.dataset.fleets ?? '').split(',').map(Number));
      else if (el && el.classList.contains('reg')) onRegion(Number(el.dataset.id));
      else if (el && el.classList.contains('afm')) {
        // An airfield's pad opens the airfield, unless the map is waiting for a province to finish an order.
        const id = Number(el.dataset.id);
        const aiming = ui.rail || ui.ship || ui.land || ui.rebase || ui.spySend || ui.tab === 'build' || (ui.tab === 'air' && (!!ui.airMode || (!!ui.raid && ui.raid !== 'menu')));
        if (aiming) onRegion(id); else openAirfield(id);
      }
      else if (!el) {
        const p = mapPoint(e.clientX, e.clientY);
        const z = seaAt(p.x, p.y);
        if (z >= 0) onSea(z);
      }
    }
    if (pts.size === 0) start = null;
    else if (pts.size === 1) { const p = [...pts.values()][0]; start = { x: p.x, y: p.y, vx: ui.view.x, vy: ui.view.y }; moved = true; }
  };
  svg.addEventListener('pointerup', up);
  svg.addEventListener('pointercancel', up);
  svg.addEventListener('pointerleave', () => { $('#tip').style.display = 'none'; drawRoute(-1); });
  svg.addEventListener('wheel', (e) => { e.preventDefault(); zoomBy(Math.exp(e.deltaY * 0.0016), e.clientX, e.clientY); }, { passive: false });
}

// ---- Start --------------------------------------------------------------------------------

g = createGame(1939, null);
mount();
drawStatic();
applyView();
refresh();
recallPick();
openModal({ kind: 'start' });

// A handle for automated checks and for curious players in the browser console.
(window as any).theater = { game: () => g, ui, newGame, nextMonth, onRegion, onSea, refresh, applyView, continueGame, save: () => saveGame(g), liberate: (nat: string, by: string) => liberate(g, nat, by), shell: (fleet: number, region: number, what: 'troops' | 'city') => { const f = g.fleets.find((x) => x.id === fleet); return f ? bombard(g, f, g.regions[region], what) : null; } };
