// The monthly turn: each nation in order gets its month, the computer plays the nations
// the player does not, and the player's orders go through the same rules.

import { KIND, MEN_PER_DIV, STACK, type Agent, type AirFleet, type AirMission, type AirReport, type BattleReport, type Build, type Cash, type Corps, type Field, type Fleet, type Game, type General, type Ships, type ShoreReport, type Kind, type Mission, type Posture, type Region, type Skill, type Stance } from './types';
import { airBattles, airBases, airTargets, previewAir, rebaseAir, setMission, type AirAssess } from './air';
import { fleetKnown, fleetsSeen, searchOptions, searchSea, seaPicture, syncWings, wingCards, type FleetSeen, type SeaPicture, type SearchOption, type WingCard } from './carriers';
import {
  airGroups, airfieldCard, airfields, groupBases, groupJobs, launchRaid, planRaid, raidKinds, raidReplays, raidTargets, scout, setGroupJob,
  type AirfieldCard, type AirfieldRow, type BaseOption, type GroupJob, type GroupRow, type RaidKind, type RaidKindView, type RaidPlan, type RaidReplay, type RaidTarget,
} from './airwar';
import { NAVY, baseTargets, canPass, landCorps, landingTargets, planLanding, rebase, sail, seaBattles, whyNotShip, type Landing, type LandingPlan } from './navy';
import { DIFFICULTY, levelOf, setLevel, whyNotLevel, type Level } from './difficulty';
import { assignGeneral, promote, relieve, sack, spendPoint } from './commanders';
import {
  DEFENCE, allied, assess, atWar, battleCost, chanceOn, corpsBase, declareWar, ensureSupply, evacuate, evenOdds, friendlyUnits, groundWorks, hasRoom, headline, holdCallUp, hostileUnits, limitAttackers,
  endPhase, foldNews, standingCallUp, moveCorps, moveCost, openTheatres, probeCost, probeLine, probeRules, probeWeight, railMove, swapCorps, whyNoSwap, resolveBattle, shipMove, sovietEvent, startPhase, theName, whyNoProbe, worksOf,
  type Assessment, type CostOf, type GroundWorks, type ProbeReport,
} from './game';
import { foldChoices, foldCorps, type FoldView } from './fold';
import { cancelBuild, orderCorps, orderFort } from './economy';
import { offers, orderOffer, type Offer } from './arsenal';
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
  INTEL, SIGHT, armiesSeen, booksOf, cityReport, corpsKnown, fogOn, oddsBand, reconnoitre, recruit, retire, sendAgent, setFog, setMission as setAgentMission, sightOf, sortKnown, spyRoom,
  whyNotMission, whyNotRecruit, whyNotSend, type ArmySeen, type Books, type CityReport, type Known, type SpyRoom,
} from './intel';
import { turnToDate, type TicketLine } from '../core/ticket';
import { couldJoin, joinArmy, leaveArmy, lendLimit, lentBy, whyNotJoin } from './joint';
import { phaseFor, runAI } from './ai';
import { court, monthlyDiplomacy, warForecast, type WarForecast } from './diplomacy';
import { founders, invite, invites, pathOptions, takePath, whyNotInvite, type Invite, type PathId, type PathOption } from './league';
import { awardOf, cashHonour } from './stories';
import { contenders, dominating, keepPeace, startDomination, warDecided, whyNotDomination, worldBoard, worldWinner, type Decided, type WorldBoard } from './domination';
import { fallenNations, passingCard, passingsIn, standDown, type FallenRow, type PassingCard } from './liberation';
import { armyCard, corpsCard, corpsSeen, type ArmyCard, type CorpsCard } from './armies';
import { corpsMood, type Mood } from './morale';
import { prizeOf, prizesInReach, townAt, townName, townSize, townsOf, type Prize, type TownSize, type TownView } from './towns';
import { CIVIC, civicOffers, civicRules, orderCivic, plotsAt, type CivicOffer, type PlotView } from './buildings';
import { endTrade, signTrade, tradeBook, type TradeBook } from './trade';
import { SIEGE, fieldCtrl, garrison, garrisonOf, pocketView, previewSally, previewStorm, sally, siegeRules, siegeView, siegesNow, storm, takeField, townHeld, whyNotGarrison, whyNotSally, whyNotStorm, whyNotTakeField, type PocketView, type SiegeView } from './siege';
import { RIVALS, characterOf, effortsOf, plansKnown, quietNews, rivalRules, type Character } from './staff';
import {
  orderWreckPrize, prizeIncome, prizeMarks, prizesAt, scorchPolicy, setBurnOrder, setScorchPolicy, whyNotWreck,
  type PrizeIncome, type PrizeKind, type PrizeMark, type PrizesView, type ScorchPolicy,
} from './prizes';
import {
  appoint, cabinetBill, cabinetOff, decree, govMul, government, headAsked, headGiven, headOf, noteReadings, officeSecure, promise, promiseTargets, promiseTerms, setAside, setCabinet, setCanFall, standBy, takeBack,
  whyNotAppoint, whyNotDecree, whyNotPromise, whyNotSetAside, whyNotStandBy, whyNotTakeBack, type GovView, type PromiseTarget, type PromiseTerms,
} from './government';
import { MANPOWER, callEarly, classGapUntil, classNow, earlyClass, earlyGap, manpower, menWords, raisable, whyNotEarly, type MenView } from './manpower';
import EVE from '../../data/campaign/eve.json';

export const current = (g: Game): string => g.order[g.phase];

function closePhase(g: Game, nat: string): void {
  // The month's air campaigns and fleet actions are fought once the nation's orders are all given.
  airBattles(g, nat);
  seaBattles(g, nat);
  // Carriers lost in the month's fleet actions take their aircraft with them (the grand campaign).
  syncWings(g);
  // The routine of the month, in one line (the grand campaign).
  foldNews(g, nat);
  endPhase(g, nat);
  if (nat === 'GER') sovietEvent(g);
  g.phase++;
  if (g.phase >= g.order.length) {
    g.phase = 0; g.turn++; g.week = 1;
    openTheatres(g);
    monthlyDiplomacy(g);
  }
}

/** Plays one computer-run nation's month. */
export function stepAI(g: Game): string {
  const nat = current(g);
  g.week = 1;
  startPhase(g, nat);
  if (g.nations[nat].alive) runAI(g, nat);
  closePhase(g, nat);
  return nat;
}

/** Opens the player's month: reinforcements, replacements, fresh action points. */
export function beginHuman(g: Game): void {
  g.week = 1;
  startPhase(g, current(g));
  // The month's reports come in: what the player's agents have in view is identified afresh.
  reconnoitre(g);
  // And the cabinet notes what the attack card reads against the towns he could promise, the way his page now reckons it.
  noteReadings(g);
  quietNews(g);
}

export function endHuman(g: Game): void {
  entrench(g, current(g));
  closePhase(g, current(g));
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
  // A corps sent home on a bond tour or to the schools takes no orders until it is back.
  if (c.dead || g.human === null || current(g) !== g.human || c.kind === 'fort' || c.leave !== undefined) return false;
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
      const step = moveCost(g, c, r, a);
      const left = c.ap - ca;
      if (left <= 0 || left < Math.min(step, 4)) continue;
      const cost = ca + step;
      if (hostileUnits(g, r, c.nat).length) {
        if (a === c.reg && !out.has(j)) out.set(j, { cost: 1, prev: a, attack: true });
        continue;
      }
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
 * act of war on that country, and it has to be declared first (orderDeclare).
 */
export function whyNotAttack(g: Game, target: Region, nat: string | null = g.human): string {
  if (g.flags.open !== 1 || nat === null) return '';
  if (allied(g, nat, target.ctrl) || atWar(g, nat, target.ctrl)) return '';
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
  if (band.exact) return { exact: true, odds: a.odds, lo: a.odds, hi: a.odds, chance: chanceAt(a.odds), chanceLo: chanceAt(a.odds), chanceHi: chanceAt(a.odds), even, unidentified: 0, stale: 0, note: '', wide: false, guess: chanceAt(a.odds), narrow: '', cost, lines: a.lines };
  const n = a.defenders.length;
  const corpsWord = (x: number): string => `${x} corps`;
  // What the ticket says of the defenders' numbers, spirit and armour is left out where it is not known.
  const lines = a.lines.map((l): TicketLine => {
    if (l.label === 'Forces') return { ...l, value: `${l.value.split(', against ')[0]}, against ${corpsWord(n)}${k.none.length ? `, ${k.none.length === n ? 'none' : `${n - k.none.length}`} of them identified` : ', known from old reports'}` };
    if (l.label === 'Morale') {
      const ours = l.value.split(', defenders')[0];
      return { ...l, value: /^Attackers/.test(ours) ? `${ours}; the defenders' spirit is not known` : "The defenders' spirit is not known", effect: l.effect ? l.effect.split(', defence')[0].replace(/^Defence.*$/, '') || undefined : undefined };
    }
    if (l.label === 'Troops') return { label: l.label, value: 'How good the defenders are is not known', effect: 'Training and experience count for as much as numbers: identify them first' };
    if (l.label === 'Armor' && /without armor/.test(l.value) && k.none.length) return { ...l, value: 'No tanks have been seen among the defenders', effect: 'Tank attack up 25% if there are none' };
    return l;
  });
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
    lines,
  };
}

export function orderAttack(g: Game, corps: Corps[], target: Region): BattleReport | null {
  if (onEve(g)) return null;
  const ok = jointForce(g, limitAttackers(g, jointForce(g, corps.filter((c) => canOrder(g, c) && c.ap > 0 && g.regions[c.reg].nb.includes(target.id)), target), target), target);
  if (!ok.length || !hostileUnits(g, target, ok[0].nat).length || whyNotAttack(g, target, ok[0].nat)) return null;
  return resolveBattle(g, ok, target);
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

/** Provinces to mark on the map: a prize wrecked there (burning), or in the hands of an army that has not taken the town (seized). Under the fog, only ground the player can see. */
export function prizesOnMap(g: Game): PrizeMark[] {
  const all = prizeMarks(g);
  if (g.human === null || !fogOn(g, g.human)) return all;
  const sight = sightOf(g, g.human);
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

/** Why the player cannot set his directive aside, or null if he can. */
export function whyNotSetAsideDirective(g: Game): string | null {
  if (g.human === null || !humanUp(g)) return 'It is not your month.';
  return whyNotSetAside(g, g.human);
}

/** The player tells the cabinet its directive will not be carried out: the trust is lost at once, and the next one is set sooner. */
export function orderSetAside(g: Game): boolean {
  return g.human !== null && humanUp(g) && setAside(g, g.human);
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
  if (!v || g.human === null || !fogOn(g, g.human)) return v;
  // Under the fog an enemy's garrison is counted only where the player's side stands round the town or has the province in full view.
  const r = g.regions[region];
  if (allied(g, g.human, r.ctrl) || (v.by && allied(g, g.human, v.by)) || sightOf(g, g.human)[region] === SIGHT.full) return v;
  return { ...v, inside: -1 };
}

/** Every siege the player knows of: his own, his allies', and any other on ground he can see. */
export function siegesInSight(g: Game): SiegeView[] {
  const all = siegesNow(g);
  if (g.human === null || !fogOn(g, g.human)) return all;
  const sight = sightOf(g, g.human);
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
  return g.regions.filter((r) => r.port && r.id !== from.id && whyNotShip(g, c, r) === null);
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
  return landCorps(g, cs, g.regions[to]);
}

const myFleet = (g: Game, id: number): Fleet | null => {
  const f = g.fleets.find((x) => x.id === id);
  return f && f.nat === g.human && current(g) === f.nat ? f : null;
};

/** Sends one of the player's fleets to a sea to patrol or raid, or back to port. */
export function orderFleet(g: Game, id: number, zone: number, stance: Stance): boolean {
  const f = myFleet(g, id);
  return !!f && sail(g, f, zone, stance);
}

/** Moves one of the player's fleets to a new home port. */
export function orderRebase(g: Game, id: number, region: number): boolean {
  const f = myFleet(g, id);
  return !!f && rebase(g, f, g.regions[region]);
}

// ---- The navy as pieces (the grand campaign): cards, escorts on a lane, shelling a coast, splitting and joining ----

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
}

/** Everything one of the player's fleets could be told to do this month. Null if it is not theirs to order now. */
export function fleetOptions(g: Game, id: number): FleetOptions | null {
  const f = myFleet(g, id);
  if (!f) return null;
  const war = g.nations[f.nat].atWar.length > 0;
  return {
    seas: war ? seaOptions(g, f, f.nat) : [],
    lanes: war ? escortLanes(g, f.nat).map((l) => planEscort(g, f, l.id)) : [],
    coasts: war ? bombardTargets(g, f, f.nat).map((r) => r.id) : [],
    ports: baseTargets(g, f).map((r) => r.id),
    merge: mergeTargets(g, f).map((x) => x.id),
    split: g.flags.open === 1 && Object.values(f.ships).reduce((s, n) => s + n, 0) > 1,
  };
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
  return !!f && escortLane(g, f, lane);
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
  return (f && detach(g, f, ships, name)?.id) || -1;
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

/** Moves one of the player's air fleets to airfields in another friendly region. */
export function orderAirBase(g: Game, id: number, region: number): boolean {
  const f = myAir(g, id);
  return !!f && rebaseAir(g, f, g.regions[region]);
}

/** Where one of the player's air fleets could be sent on a mission, and where it could be based. */
export function airOptions(g: Game, id: number, mission: AirMission): { targets: number[]; bases: number[] } {
  const f = myAir(g, id);
  return f ? { targets: airTargets(g, f, mission), bases: airBases(g, f).map((r) => r.id) } : { targets: [], bases: [] };
}

// ---- The air war as its own game (the grand campaign): airfields, air groups, and raids planned from their target ----

/** Every airfield on the map as the player knows it, for a map to draw. Empty in a historical scenario. */
export const airfieldsSeen = (g: Game): AirfieldRow[] => airfields(g, g.human);

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

/** The raid planner's menu: each kind of raid and how many targets are in reach this month. */
export const raidMenu = (g: Game): RaidKindView[] => raidKinds(g, g.human);

/** Every target for one kind of raid, nearest first. With `lead`, only those that one of the player's groups can fly against. */
export const raidTargetsFor = (g: Game, kind: RaidKind, lead = -1): RaidTarget[] => raidTargets(g, kind, g.human, lead);

/**
 * A raid weighed up before it is ordered: who could fly it, who is going, what will meet it, the chance of success,
 * the losses to expect and what meeting up is worth. Leave `ids` out for the staff's proposal.
 */
export const previewRaid = (g: Game, kind: RaidKind, target: number, ids?: number[]): RaidPlan => planRaid(g, kind, target, ids, g.human);

/** Flies a planned raid at once and files its report. Null if it cannot be flown as picked. */
export function orderRaid(g: Game, kind: RaidKind, target: number, ids: number[]): AirReport | null {
  return g.human !== null && humanUp(g) ? launchRaid(g, kind, target, ids, g.human) : null;
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

// ---- Carriers and the air war at sea (the grand campaign) ----
//
// A carrier wing is an air group like any other: its id (WingCard.id, GroupRow.id with ship >= 0) goes to the raid
// planner (raidTargetsFor, previewRaid, orderRaid with the kinds 'fleet' and 'harbour'), to airGroupJobs and
// orderGroupJob, and to the search below. It has no airfield: it flies from wherever its fleet is.

/** Every fleet the player can put on the map: his own side's whole, and others' as far as his side has found them. A fleet only reported has a flag and a rough size and no more; one not found is not in the list. */
export const fleetsInSight = (g: Game): FleetSeen[] => fleetsSeen(g, g.human);

/** One sea as the player knows it: each side's known strength, enemy forces only reported, and how well his side has it searched. Null with nobody at the table. */
export const seaPictureOf = (g: Game, zone: number): SeaPicture | null => (g.human && g.seas[zone] ? seaPicture(g, zone, g.human) : null);

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

/** The player has read a citation. */
export function orderSeen(g: Game, award: number): void {
  const a = awardOf(g, award);
  if (a && a.nat === g.human) a.seen = true;
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
  return humanUp(g) && cancelBuild(g, g.human!, id);
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
      const since = Object.keys(g.flags).filter((k) => k === `st:on:${nat}:${e}` || k.startsWith(`st:on:${nat}:${e}:t`)).reduce((s, k) => Math.max(s, g.flags[k]), 0);
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
    if (N && N.alive && !allied(g, me, o) && !atWar(g, me, o)) out.add(o);
  };
  for (const r of g.regions) {
    if (!allied(g, me, r.ctrl)) continue;
    for (const j of r.nb) add(g.regions[j].ctrl);
  }
  // A nation with troopships can also reach any coast within landing range of its own ports.
  if (g.nations[me].sealift > 0) {
    const depth = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (const r of g.regions) if (r.port && r.ctrl === me) for (const z of r.seas) if (depth[z] < 0) { depth[z] = 1; q.push(z); }
    for (let i = 0; i < q.length; i++) {
      for (const id of g.seas[q[i]].coast) add(g.regions[id].ctrl);
      if (depth[q[i]] >= NAVY.landing.range) continue;
      for (const j of g.seas[q[i]].adj) if (depth[j] < 0 && canPass(g, q[i], j, me)) { depth[j] = depth[q[i]] + 1; q.push(j); }
    }
  }
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
  declareWar(g, g.human, on);
  ensureSupply(g);
  return true;
}

/** Spends the player's political capital to pull a neutral toward the player's bloc. */
export function orderCourt(g: Game, target: string): boolean {
  return humanUp(g) && court(g, g.human!, target);
}

export { KIND };
