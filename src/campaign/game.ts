// The campaign rules: the map, who is at war, how armies move and are supplied, how a
// battle is auto-resolved, and how a nation falls. The same battle maths will sit under
// the playable 3D battles later, so that skipping a battle and fighting it are the same fight.

import { FORT_XP, crewFed, crewMonth, fortsMonth, portStores } from './fortrows';
import capData from '../../data/campaign/capitulations.json';
import { chartDist, chartDist2 } from './wrap';
import { AFV_PER_DIV } from '../battle/land/wrecks';
import mapData from '../generated/campaign-map.json';
import nationsData from '../../data/campaign/nations.json';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import setup from '../../data/campaign/setup-1939.json';
import { flagName } from './flagnames';
import { Rng, hashString } from '../core/rng';
import { clamp } from '../core/math';
import { turnToDate, type TicketLine, type VictoryLevel } from '../core/ticket';
import { BUDGET, PRODUCTION, budgeted, canBuildYet, corpsName, deliver, farShare, firstCorpsNumber, gunsWanted, income, specOf, upkeepTotal } from './economy';
import { DIPLOMACY, RELATIONS, alarm, alarmed, homeOpinion, leavingThreat, majorsOf, pcIncome, sneakOn, warDiplomacy, wouldSurprise } from './diplomacy';
import { ownPath } from './league';
import { afterBattle, cmdAp, cmdAttack, cmdCutOff, cmdDefence, cmdLine, cmdLoss, cmdRally, cmdSteady, cmdWinter, commandersIn, generalOf, monthlyCommand, seat } from './commanders';
import { AIR, airOnFall, airUpkeep, bookCloseAir, closeEdge, closeOf, closeRules, closeStrikes, ratedAir, setupAir, skyByNumbers, skyOnLand, skyOver, type CloseAir } from './air';
import { syncWings } from './carriers';
import { researchTick, setupResearch, techMul, techNotes } from './research';
import { locked, techTick, winterReady } from './tech';
import { liftDates } from './lift';
import { RES, fuelAir, fuelMet, fuelSlows, hungerLost, resourcesTick, shortAttack, shortDefence, supDrafts, suppliesMet, willLost } from './resources';
import { crown, warGoesOn } from './domination';
import { afterFall, recordFall, rememberWars, retaken, standsFreed } from './liberation';
import { honour } from './guarantees';
import { MORALE, forceMorale, heart, moraleBattle, moraleCapital, moraleMonth, moraleWord, nerve, raiseMorale, restedDrafts, restedMonth, restedOrg } from './morale';
import { civicDrafts, civicSum, handedOver, holdersNow, hospitalNear, hospitalWorth } from './buildings';
import { tradeTick } from './trade';
import { growCorps, numberCorps } from './armies';
import { INTEL, corpsKnown, identify, intelAttack, intelTick, seeCuts } from './intel';
import { plansOnCapture } from './plans';
import { armyOf, jointHold, jointMonth } from './joint';
import { SeaPicture, canPass, closedLine, coastReport, fleetsOnFall, navalUpkeep, openSeas, openingStations, opensAt, roadsHome, sailFirst, sameSeaState, seaOpenAt, seaSide, seaStarts, setupNavy, shipCorps, type SeaState } from './navy';
import { drafted, newStories, schoolMonth, wearMonth, storyAttack, storyBattle, storyDefence, storyFallen, storyMonth, storyWalk } from './stories';
import eastData from '../../data/campaign/east.json';
import arsenalData from '../../data/campaign/arsenal.json';
import placesData from '../../data/campaign/places.json';
import defenceData from '../../data/campaign/defence.json';
import winData from '../../data/campaign/winchance.json';
import pursuitData from '../../data/campaign/pursuit.json';
import { unitsDecide } from './units';
import { columnShare, columnsMonth } from './columns';
import rivalsData from '../../data/campaign/rivals.json';
import { barred, combinedArms, groundAttack, groundDefence, groundLine, groundMarch, groundRules, groundSays, isFoot, whyBarred } from './ground';
import { SUPPORT, antiTank, assaultFire, footShare, machineGuns, mortars, snipedIn, tollFor, tollOf, whoPaid, type Faced, type Facing } from './support';
import { isTeam, TEAMS, teamsIn } from './teams';
import { ENGINEERING, blocked, bridgeDown, bridgesOnCapture, downBetween, noCrossing, riverBetween, riverName, worksName } from './rivers';
import { engineersMonth, liftMines, minesAt, sappers } from './engineers';
import { CAUSE_WORDS, cutCause, cutOn, hastyLeft, lineSince, onNet, railCut, railOnCapture, railheadOf, railsAt, resetRailheads } from './rails';
import { junctionOpenTo, mendPrizes, railOnScorch, tellBurnt, tellSettled } from './prizes';
import { answersOf, beyondEurope, farOn, patronOf, pressData, protectorOf, shipsItsOwn, shoreOf, worldOn } from './world';
import { FAR_STAGE } from './stage';
import { govCapital, govMonth, govMul, judgeDirective } from './government';
import { footHold } from './footing';
import { MANPOWER, classArrives, drawMen, menLeft, menRules, menSpare, menWarning, menWords, poolAtStart, raisable } from './manpower';
import { legionDrafts } from './legions';
import { draftsMul } from './difficulty';
import { traitAttack, traitDefence, traitDrafts, traitLevy, traitNames } from './traits';
import { SIEGE, arrive, besieged, besiegerOf, breachOf, canShelter, driveOut, dropSieges, fall, fieldCtrl, fieldWalls, garrisonOf, guardAfter, guardForce, guardInField, guardMen, guardWorth, guardOf, guardPower, inPocket, joinTown, manRings, musterNear, pocketDry, pocketMonth, pocketView, quitTown, ringClosed, shutIn, siegeEmpties, siegeFalls, siegeMonth, siegeRules, siegeSupply, sortiePower, tallySiege, townBonus, townHeld } from './siege';
import { battleHit } from './repairs';
import { TOWNS, townName, townSize, worthOf } from './towns';
import { WEATHER, skyAt, skyBattleLine, skySupport, weatherOn, zoneOfRegion } from './weather';
import desertData from '../../data/campaign/desert.json';
import farSandData from '../../data/campaign/far-sand.json';
import theatreData from '../../data/campaign/theatres.json';
import { raisedQuality, stopDef, stopOf } from './history';
import { chargeDelay, isRearguard, rearguardWeeks, rearguardsAt, rearguardsLapse, screenWeeks, weeksWords } from './rearguard';
import { clientWar, tiedPair } from './far';
import { atomFallen, atomMonth, atomPress } from './atom';
import { demandInstead, keepersOf, keptForces, pacificWar } from './pacific';
import { engRoom, islandOf, roomAt, roomRefusal, roomSays } from './islands';
import { frontOfRow, onLiveFront } from './planfronts';
import { bookKept } from './stations';
import { floorWords, witherAt, witherDrop, witherFed, witherMonth, withered } from './wither';

/** Rules for campaigning deep inside the Soviet Union. See data/campaign/east.json. */
export const EASTERN = eastData as unknown as {
  deepOwners: string[];
  /**
   * Ruling 221 (the Pacific planner's paper 20). 'lineOwners': owners whose ground takes the long line as the Soviet
   * Union's does (its drag, its thin drafts and rest, no railheads of the invader's own), counted from the invader's
   * frontier and from the harbours it holds on that ground, with none of the Russian winter; read only at the dial's
   * stops in 'lineStops' (every stop if it is not given). 'lineReckon': a staff reckons that line in its sums of
   * strength, as it reckons the winter. 'nearRefuge': troops lifted off the coast of a country their own is fighting
   * are landed at the nearest harbour their country holds on that country's ground, not taken home.
   */
  lineOwners?: string[]; lineStops?: number[]; lineReckon?: boolean; nearRefuge?: boolean;
  mud: number[]; lineDefence: number; stepFresh: number; stepConverted: number; convertAfter: number; badWeather: number;
  bands: [number, number][]; drag: [number, number][]; roads: number; winter: { months: number[]; attack: number; defence: number; attrition: number; rest: number; hardy: string[] };
  mobilisation: Record<string, { perMonth: number[]; after: number; divs: number; kind: Kind; xp: number; name: string; trim?: { stops: number[]; cut: number; from?: number }; works?: [number, string, number][] }>;
  works?: [number, string, number, string[], string, { taken?: boolean; port?: boolean; vs?: string[] }?][];
};
/** Rules for campaigning in the desert. See data/campaign/desert.json. */
export const DESERT = desertData as unknown as {
  terrain: string; step: number; port: number; ports: Record<string, number>; bands: [number, number][]; lineDefence: number; open: number; digLate: number; box: number; stores: number; retreat: [number, number]; replace: number; drag: [number, number][];
  route: { id: string; feeds: string[]; perTenth: number };
  works?: [number, string, number, string[], string][];
};
import {
  FORT_MUL, KIND, MEN_PER_DIV, STACK, TERRAIN_DEF,
  type BattleReport, type Corps, type Field, type Game, type Headline, type Kind, type Nation, type Region, type ReportUnit, type Voyage,
} from './types';

const ORDER = ['GER', 'ITA', 'SVK', 'HUN', 'SOV', 'POL', 'FRA', 'UK', 'USA', 'BEL', 'NED', 'LUX', 'DEN', 'NOR'];
const EAST = new Set(['POL', 'SOV', 'LIT', 'LAT', 'EST', 'ROM', 'FIN']);

/** A province as a game holds it, from its row in a map file (the map of Europe, or the wider world's: world.ts). */
export function regionOf(r: any): Region {
  // Which side theatre each region belongs to. A theatre with an opening month is ordinary ground until then (see openTheatres).
  const k = THEATRE_OF.get(r.name) ?? -1;
  return {
    id: r.id, name: r.name, owner: r.owner, ctrl: r.owner, first: r.owner, terrain: r.terrain, vp: r.vp, fac: r.fac,
    // A depot on the coast is a harbour too. One inland feeds an army but takes no ships.
    cap: !!r.flags.cap, port: !!r.flags.port || (!!r.flags.depot && (r.seas ?? []).length > 0),
    fort: r.flags.fort ? { lv: r.flags.fort[0], vs: r.flags.fort.slice(1), by: r.owner } : null,
    vichy: !!r.flags.vichy, sov: !!r.flags.sov, oil: !!r.flags.oil, overseas: !!r.flags.col, theatre: k >= 0 && THEATRE_LIST[k].opens === undefined ? k : -1,
    x: r.x, y: r.y, nb: r.nb, polys: r.polys, units: [], seas: r.seas ?? [], since: 0, prev: null,
  };
}

/** A nation as a game holds it on its first day, from its row in the data: at peace, its books empty. */
export function nationOf(id: string, d: any, capital: number): Nation {
  return {
    id, name: d.name, adj: d.adj, color: d.color, quality: d.quality, air: d.air, resilience: d.resilience, align: d.align,
    blitz: !!d.blitz, slowCommand: !!d.slowCommand, maxAttack: d.maxAttack ?? 0, seaSupply: !!d.seaSupply, sealift: d.sealift ?? 0,
    atWar: [], alive: !d.dormant, dormant: !!d.dormant, offmap: d.offmap ?? null, capital, pp: 0, queue: [], corpsNo: firstCorpsNumber(id),
    opinion: homeOpinion(id, d.align), home: homeOpinion(id, d.align), pc: 0, armyPeak: 0, offStart: {}, lastFight: {}, lab: null,
  };
}

/** How many provinces lie between each province and a nation's capital, by land (999 where there is no way). */
export function capitalDistances(g: Game, capital: number): Int16Array {
  const d = new Int16Array(g.regions.length).fill(999);
  d[capital] = 0;
  const q = [capital];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] === 999) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}

export function createGame(seed: number, human: string | null): Game {
  const md = mapData as any;
  const regions: Region[] = md.regions.map(regionOf);
  const byName: Record<string, Region> = {};
  for (const r of regions) byName[r.name] = r;
  const nations: Record<string, Nation> = {};
  for (const [id, d] of Object.entries(nationsData as Record<string, any>)) {
    if (id.startsWith('_')) continue;
    const cap = regions.find((r) => r.owner === id && r.cap);
    nations[id] = nationOf(id, d, cap ? cap.id : -1);
  }
  const order = [...ORDER, ...Object.keys(nations).filter((n) => !ORDER.includes(n))];
  const g: Game = {
    seed, rng: new Rng(seed), turn: 1, phase: 0, order, nations, regions, byName, corps: [], generals: [], nextCorps: 1, nextBuild: 1, human,
    reports: [], headlines: [], flags: {}, supplyDirty: true, supply: {}, reach: {}, railhead: {}, capDist: {}, week: 1,
    seas: [], gates: [], fleets: [], nextFleet: 1, seaReports: [], shoreReports: [], trade: [], convoys: {},
    airFleets: [], nextAir: 1, airReports: [], airPool: {}, sky: {}, raids: {}, stories: newStories(), intel: { agents: [], next: 1, notes: [], heat: {} }, passings: [],
    links: md.links, x0: 0, width: md.width, height: md.height, land: md.land,
  };
  for (const row of (setup as any).corps) addCorps(g, row[0], row[1], row[2], row[3], row[4], row[5] ?? {});
  setupNavy(g);
  setupAir(g);
  setupResearch(g);
  // Depots: home regions that can feed an army even when cut off from the capital.
  (md.regions as any[]).forEach((r) => { if (r.flags.depot) g.flags['depot:' + r.id] = 1; });
  for (const n of Object.values(nations)) if (n.capital >= 0) g.capDist[n.id] = capitalDistances(g, n.capital);
  // 1 September 1939.
  setWar(g, 'GER', 'POL');
  setWar(g, 'SVK', 'POL');
  setWar(g, 'UK', 'GER');
  setWar(g, 'FRA', 'GER');
  nations.GER.offStart.POL = 1;
  headline(g, 'war', 'Germany invades Poland. Britain and France declare war on Germany.', 'GER');
  for (const n of Object.values(nations)) n.armyPeak = armySize(g, n.id);
  openingStations(g);
  computeSupply(g);
  return g;
}

/**
 * A row's establishment in a given month. A number is the fullest the row ever gets. A list of [month, worth] is the
 * record's size by date (rider b614 to ruling 234: Japan's island garrisons by half-year, the historian's table):
 * the last line whose month has come, and the first line before any has.
 */
export function estAt(est: number | [number, number][] | undefined, turn: number): number | undefined {
  if (!Array.isArray(est)) return est;
  let now = est[0]?.[1];
  for (const [m, w] of est) if (m <= turn) now = w;
  return now;
}

/**
 * A dated establishment's month (`estAt`): the row, while it stands on the ground its row put it on, takes the
 * record's size for the month. The men of an increase arrive with it, as any row's do; a smaller size sends the
 * rest home. A garrison that has fallen, or been shipped off its island, is not followed. No row has a dated
 * establishment with the far side off.
 */
function estMonth(g: Game, nat: string, row: any[]): void {
  const est = row[6]?.est;
  if (!Array.isArray(est)) return;
  const c = g.corps.find((x) => x.nat === nat && x.name === row[2] && !x.dead);
  if (!c || g.regions[c.reg].name !== row[5]) return;
  const was = c.full ?? 1, now = estAt(est, g.turn) ?? 1;
  if (now === was) return;
  c.str = now > was ? Math.min(now, c.str + (now - was)) : Math.min(c.str, now);
  if (now < 1) c.full = now; else delete c.full;
}

export function addCorps(g: Game, nat: string, name: string, kind: Kind, divs: number, region: string | number, o: any): Corps {
  const reg = typeof region === 'number' ? g.regions[region] : g.byName[region];
  const c: Corps = {
    id: g.nextCorps++, nat, name, kind, divs, str: 1, org: 1, xp: o.xp ?? 0.15, reg: reg.id, ap: 4, apMax: 4,
    general: null, hold: !!o.hold, dyle: o.dyle ?? null, supplied: true, fought: false, dead: false,
  };
  if (o.airborne) c.airborne = true;
  if (o.stay) c.stay = true;
  // A fortress crew is trained, never green, every nation's alike (fortrows.json crewXp; the fortress rows' switch).
  if (kind === 'fort' && fortRows(g)) c.xp = Math.max(c.xp, FORT_XP);
  // The fullest the row ever gets (the planner's paper 18, ruled 16:32Z on 17 November 2026): a garrison of a brigade
  // is a row of one division that never fills past its own size, for player and computer alike.
  // (A row whose record changes its size by date gives a list of month and worth, as the air rows do: `estAt`.)
  const est = estAt(o.est, g.turn);
  if (est !== undefined && est < 1) c.full = est;
  // (The far side: a row's own works and will, from the record: ruling 234's note of 22:05Z of 20 November 2026, item
  // 5. `works` is [province, months]: the works it has where the record's landing came. No older row gives either.)
  if (Array.isArray(o.works) && g.byName[o.works[0]]) c.ownWorks = [g.byName[o.works[0]].id, o.works[1]];
  if (o.will !== undefined) c.will = o.will;
  // Where and when it was raised: the first lines of its record.
  c.born = g.turn;
  if (reg.owner === nat) c.home = reg.id;
  // The grand campaign: its morale, and the numbers its divisions carry.
  raiseMorale(g, c);
  numberCorps(g, c);
  g.corps.push(c);
  // Raised in a town the enemy stands round (the grand campaign): it joins the garrison.
  // If the town is full it forms up on the nearest ground its side still holds.
  if (siegeRules(g) && reg.units.some((u) => atWar(g, u.nat, nat))) {
    const alt = musterNear(g, nat, reg);
    if (alt) { c.reg = alt.id; alt.units.push(c); } else joinTown(g, reg, c);
  }
  else reg.units.push(c);
  if (o.gen) seat(g, c, o.gen, o.trait);
  return c;
}

/** A country's name as it reads in a sentence: "the United States", "Germany". */
export function theName(n: Nation, start = false): string {
  return /^(United|Soviet|Netherlands|Dominican Republic)/.test(n.name) ? `${start ? 'The' : 'the'} ${n.name}` : n.name;
}

/**
 * The countries whose names take their article wherever they follow another word, and "The" at the head of a line
 * ("against the United States", "The United States against Canada": the UI director's words for ruling 211(f)).
 * Every line of news passes through it, so no line has to remember; "a Soviet Union that ..." is left as written.
 */
const ARTICLED = /(^|[^A-Za-z])(?<![Tt]he )(?<!\b[Aa]n? )(United States|United Kingdom|Soviet Union|Netherlands|Dominican Republic)\b/g;
/**
 * And the places whose names take "the" (data/campaign/places.json, the UI director's list and rule): "the Ruhr", "The
 * Ruhr" at the head of a line, "the Ruhr's output". A name inside a longer one keeps that one's form: one that runs on
 * into a capital word ("Azores Waters", "Terek Cossacks"), one after a title or a word that makes a name ("Army Group
 * Kuban", "General Mackenzie", "Mount Lebanon"), and the longer places the file names ("Rio Grande do Sul").
 */
const PLACES = placesData as { the: string[]; within?: Record<string, string[]> };
const reEsc = (s: string): string => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const NAME_WORDS = 'Army|Group|Corps|Front|Fleet|Force|Command|Division|Brigade|Detachment|Legion|Operation|General|Admiral|Marshal|Colonel|Major|Captain|Lord|Sir|President|Minister|King|Mount|Lake|Cape|Port|Fort|River|Gulf|Bay|Isle|Saint|St|New|North|South|East|West|Upper|Lower|Old|Great';
const PLACED = new RegExp(`(^|[^A-Za-z])(?<![Tt]he )(?<!\\b[Aa]n? )(?<!\\b(?:${NAME_WORDS}) )(${[...PLACES.the].sort((a, b) => b.length - a.length).map((n) => {
  const longer = PLACES.within?.[n] ?? [];
  const after = longer.filter((l) => l.startsWith(n)).map((l) => `(?!${reEsc(l.slice(n.length))})`).join('');
  const before = longer.filter((l) => l.endsWith(n) && l !== n).map((l) => `(?<!${reEsc(l.slice(0, l.length - n.length))})`).join('');
  return `${before}${reEsc(n)}${after}`;
}).join('|')})\\b(?! [A-Z])`, 'g');
/** A plural name owns with the apostrophe alone: "the Midlands' output", never "Midlands's" (the third playtester, 84.3.1). */
const PLURAL_OWN = new RegExp(`\\b(${[...PLACES.the.filter((n) => /s$/.test(n)), 'Netherlands', 'United States'].sort((a, b) => b.length - a.length).map(reEsc).join('|')})'s\\b`, 'g');
/**
 * A line of running text with its articles. `before` is the text that runs before it in the same sentence or
 * paragraph, where the line is one piece of a longer one (a page's text, piece by piece): it decides "The" or "the"
 * and is not itself changed.
 */
export function theWords(text: string, before = ''): string {
  const cut = before.length;
  const put = (s: string, re: RegExp): string => s.replace(re, (m: string, pre: string, name: string, at: number) => {
    if (at + pre.length < cut) return m;
    const head = /(^|[.!?]\s+)$/.test(s.slice(0, at + pre.length));
    return `${pre}${head ? 'The' : 'the'} ${name}`;
  });
  return put(put(before + text, ARTICLED), PLACED).slice(cut).replace(PLURAL_OWN, "$1'");
}
/** A place's name as it reads in running text: "the Ruhr", "Warsaw"; `start` at the head of a sentence. */
export function placeName(name: string, start = false): string {
  return PLACES.the.includes(name) ? `${start ? 'The' : 'the'} ${name}` : name;
}

export function headline(g: Game, kind: Headline['kind'], text: string, nat?: string, region?: number, tag?: NewsTag): void {
  g.headlines.push({ turn: g.turn, week: newsWeek(g), text: theWords(text), kind, nat, region, ...tag });
}

/**
 * The week a line of news is dated (display only: no rule reads it). Each nation's month counts its weeks from the
 * first, so a nation that moves after the player dated its news before his own battles of the same month: Canada's
 * guns closing round Cleveland read week 2, after the relief of Pittsburgh in week 4 (ruling 211(f)). Once the
 * player's month is over, news is dated no earlier than the last week his own news reached. g.week, the battles'
 * weeks and the week a country fell, which the rules read, are left as they were.
 */
export function newsWeek(g: Game): number {
  const me = g.human, at = me ? g.order.indexOf(me) : -1, reached = g.flags.newsweek ?? 0, month = (g.turn - 1) * 4;
  // The player's own month: his news keeps its own weeks, and the furthest of them is noted (saved with the game).
  if (at >= 0 && g.phase === at && g.flags.eve !== 1) { g.flags.newsweek = Math.max(reached > month ? reached : 0, month + g.week); return g.week; }
  if (at < 0 || g.phase <= at) return g.week;
  // After it: the furthest week his news reached, or his battles did.
  let floor = reached > month ? reached - month : 0;
  for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn === g.turn; i--) if (g.reports[i].attNat === me) floor = Math.max(floor, g.reports[i].week);
  return Math.max(g.week, floor);
}

/** The two tags on news of fighting (Version 80, display only): the arm that fought, and who had the better and the worse of it. */
export type NewsTag = Pick<Headline, 'arm' | 'won' | 'lost' | 'ready' | 'unit'>;

/** News the player must not miss: a warning with a short title, set out as a card of its own in the month's news. */
export function warning(g: Game, title: string, text: string, nat?: string, region?: number, about?: string): void {
  g.headlines.push({ turn: g.turn, week: newsWeek(g), text: theWords(text), kind: 'event', nat, region, warn: title, ...(about ? { about } : {}) });
}

/** Routine news of a computer-run nation's month, kept back for one line when the month closes. */
const routineKept = new WeakMap<Game, { nat: string; brief: string }[]>();
/** Months a routine thing may go unseen before it is news again. */
const ROUTINE_GAP = 3;

/**
 * News of something that happens month after month: the same raid on the same town, the same escorts sinking
 * submarines in the same sea. In the grand campaign it is a headline of its own the first time, and again when
 * it has not happened for a few months. In between only 'brief' is kept, for one line when that nation's month
 * closes (foldNews), so that the feed does not say the same thing for years. What the player himself ordered is
 * always told in full, and so is everything outside the grand campaign.
 */
export function routineNews(g: Game, key: string, kind: Headline['kind'], text: string, brief: string, nat: string, region?: number, tag?: NewsTag): void {
  if (g.flags.open !== 1 || nat === g.human) { headline(g, kind, text, nat, region, tag); return; }
  const k = `told:${key}`;
  const last = g.flags[k] ?? 0;
  g.flags[k] = g.turn;
  if (!last || g.turn - last > ROUTINE_GAP) { headline(g, kind, text, nat, region, tag); return; }
  let kept = routineKept.get(g);
  if (!kept) routineKept.set(g, (kept = []));
  kept.push({ nat, brief });
}

/** The one line that stands for a nation's routine news of the month, written as its month closes. */
export function foldNews(g: Game, nat: string): void {
  const kept = routineKept.get(g);
  if (!kept?.length) return;
  const mine = kept.filter((x) => x.nat === nat).map((x) => x.brief);
  routineKept.set(g, kept.filter((x) => x.nat !== nat));
  if (!mine.length) return;
  const shown = mine.slice(0, 6);
  const more = mine.length - shown.length;
  headline(g, 'info', `The month in the air and at sea, ${theName(g.nations[nat])}: ${shown.join('; ')}${more > 0 ? `; and ${more} more of the same` : ''}.`, nat);
}

// ---- Who stands where ---------------------------------------------------------------

/** The alliance a nation belongs to: one of the two blocs, or the League of smaller nations that stands apart from both. */
export type Bloc = 'allies' | 'axis' | 'league';

export function bloc(n: Nation): Bloc | null {
  if (n.league) return 'league';
  if (n.align === 'allies' || n.align === 'allies_cobelligerent') return 'allies';
  if (n.align === 'axis' || n.align === 'axis_cobelligerent') return 'axis';
  return null;
}

/** (Supply keeps its friend tables, and its lanes' answers, while friendsKeyOf is the same: anything new read here must be
 * named there too.) */
export function allied(g: Game, a: string, b: string): boolean {
  return sameSide(g, a, b) || tiedPair(g, a, b);
}

/**
 * True for one country, or two members of one alliance. 'allied' is this and, on the far side of the world, a client
 * and its patron while their tie holds (far.ts): the two stand on each other's ground as allies do, and are members
 * of no alliance together, so what asks about membership (who may declare war on whom) asks this.
 */
export function sameSide(g: Game, a: string, b: string): boolean {
  if (a === b) return true;
  const A = g.nations[a], B = g.nations[b];
  if (!A || !B) return false;
  const ba = bloc(A);
  return ba !== null && ba === bloc(B);
}

/** (Supply keeps its lanes' answers while friendsKeyOf is the same: anything new read here must be named there too.) */
export function atWar(g: Game, a: string, b: string): boolean {
  return g.nations[a].atWar.includes(b);
}

/** Puts two nations at war and says nothing about it: the declaration, the surprise and the news are declareWar's business. */
export function setWar(g: Game, a: string, b: string): void {
  const A = g.nations[a], B = g.nations[b];
  if (!A.atWar.includes(b)) A.atWar.push(b);
  if (!B.atWar.includes(a)) B.atWar.push(a);
}

/**
 * The grand campaign: a country that has taken a side is at peace with that side's members. A guarantee kept against
 * a country outside both blocs leaves it at war with the guarantors (guarantees.ts, honour), and if it later joins
 * their side, as an attacked neutral does, that war ends with no news of its own (ruling 229, part 3).
 */
export function joinedSide(g: Game, id: string): void {
  const N = g.nations[id];
  // (The last act has no sides: every power fights for itself.)
  if (g.flags.open !== 1 || g.flags.dom || !N) return;
  for (const x of [...N.atWar]) if (allied(g, id, x)) {
    N.atWar = N.atWar.filter((y) => y !== x);
    if (g.nations[x]) g.nations[x].atWar = g.nations[x].atWar.filter((y) => y !== id);
    g.supplyDirty = true;
  }
}

/** The month's end (the grand campaign): every country at peace with its own side's members, whatever path took it there. */
export function sidesAtPeace(g: Game): void {
  if (g.flags.open !== 1) return;
  for (const id of Object.keys(g.nations).sort()) if (g.nations[id].atWar.length) joinedSide(g, id);
}

export function declareWar(g: Game, a: string, b: string): void {
  const A = g.nations[a], B = g.nations[b];
  if (!A.alive || !B.alive || atWar(g, a, b) || sameSide(g, a, b)) return;
  // The far side (ruling 199(e)): Japan's declaration on Vichy France while it holds Saigon is answered by the handing-over of Indochina, and there is no war.
  if (demandInstead(g, a, b)) return;
  const tied = tiedPair(g, a, b); // a client turning on its patron, or the patron on it (far.ts): read before the war is set
  // The grand campaign: a blow against a country at peace is a surprise this month, and when the player strikes a
  // country outside both blocs the neutral world takes note. Both are read off before anything changes.
  const sneak = wouldSurprise(g, a, b);
  const shock = alarmed(g, a, b);
  // (The alliance the country stood in before the blow, for its guarantees: an attacked neutral takes a side below.)
  const sideWas = bloc(B);
  // A country outside every alliance that is struck down is a fright to its friends: the month such pacts are made.
  if (g.flags.open === 1 && bloc(B) === null) g.flags[`shock:${b}`] = g.turn;
  setWar(g, a, b);
  // (Ruling 232, rider b606: a ring the record manned in the month its country was first attacked is manned before the attacker's battles.)
  manRings(g, b);
  if (sneak) g.flags[`sneak:${a}:${b}`] = g.turn;
  A.offStart[b] = g.turn;
  g.flags[`war:${a}:${b}`] = g.turn;
  // A neutral that leans toward a bloc and attacks that bloc's enemy becomes its co-belligerent.
  if (A.align === 'leaning_axis' || A.align === 'leaning_allies') {
    const side = A.align === 'leaning_axis' ? 'axis' : 'allies';
    if (B.atWar.some((x) => x !== a && bloc(g.nations[x]) === side)) {
      A.align = side === 'axis' ? 'axis_cobelligerent' : 'allies_cobelligerent';
      A.opinion = A.home = side === 'axis' ? 100 : -100;
      g.supplyDirty = true;
      joinedSide(g, a);
    }
  }
  // A neutral that is attacked joins the other side. A small country in the player's hands chooses for itself.
  const ba = bloc(A);
  if (bloc(B) === null && ba && ba !== 'league' && !ownPath(g, b)) {
    B.align = ba === 'axis' ? 'allies' : 'axis_cobelligerent';
    B.opinion = B.home = ba === 'axis' ? -100 : 100;
    joinedSide(g, b);
  }
  headline(g, 'war', `${theName(A, true)} declares war on ${theName(B)}.`, a);
  // War on one member of the League is war on them all.
  if (B.league && !A.league) {
    const stood: string[] = [];
    for (const m of Object.values(g.nations)) {
      if (!m.league || !m.alive || m.id === b || atWar(g, m.id, a)) continue;
      setWar(g, m.id, a);
      g.flags[`war:${m.id}:${a}`] = g.turn;
      stood.push(m.name);
    }
    if (stood.length) headline(g, 'war', `The League stands by ${B.name}: ${stood.length === 1 ? stood[0] : `${stood.slice(0, -1).join(', ')} and ${stood[stood.length - 1]}`} ${stood.length === 1 ? 'is' : 'are'} at war with ${theName(A)} too.`, b);
  }
  // Britain may stand by its dominions, one way only (ruling 211(b)): six times in ten she goes to war the same month.
  standsBy(g, a, b);
  clientWar(g, a, b, tied); // the far side: war on a client is war on its patron, and a tie broken parts the two
  // A guarantee the card names is kept (ruling 229, part 3): its computer-run guarantors declare war the same day.
  honour(g, a, b, sideWas);
  // The far side: the dominions and the Dutch are at war with Japan the day Britain is (ruling 197; pacific.ts).
  pacificWar(g, a, b);
  // The events that are read the day a war is declared (the far side's: Siam beside Japan, ruling 200(b)).
  warDiplomacy(g);
  if (A.blitz && !g.flags[`tempo:${a}:${g.turn}`]) {
    g.flags[`tempo:${a}:${g.turn}`] = 1;
    for (const c of g.corps) if (c.nat === a && KIND[c.kind].mobile && g.order[g.phase] === a) { c.ap += 1; c.apMax += 1; }
  }
  g.supplyDirty = true;
  reactionMoves(g, a, b);
  alarm(g, shock, b);
}

/** The short name news gives a country: Britain for the United Kingdom. */
export const newsName = (n: Nation): string => (n.id === 'UK' ? 'Britain' : n.name);

/**
 * The mother country that may go to war with `a` for making war on `b`, or null (ruling 211(b) as Brian changed
 * it on 17 November 2026; the grand campaign's wider world). Britain may stand by Canada, Australia, New Zealand
 * and South Africa, one way only, when a country at peace with her makes war on one of them while the two stand
 * in the same alliance: she answers as far as her own war in Europe allows (ruling 222, patronPress: eight, six, four
 * or two times in ten; world.json, 'dominions'), at every stop of the dial and at every difficulty. A war a dominion starts is not hers, and war with any other
 * member of an alliance is war with that member alone. Not in World Domination, where a dominion follows nobody
 * either.
 */
export function standsByWhom(g: Game, a: string, b: string): string | null {
  const p = patronOf(b);
  if (!p || p === a || g.flags.open !== 1 || !worldOn(g) || (g.flags.dom ?? 0) > 0) return null;
  const P = g.nations[p], A = g.nations[a], B = g.nations[b];
  if (!P?.alive || !A?.alive || !B?.alive || atWar(g, p, a) || allied(g, p, a) || !allied(g, p, b)) return null;
  return p;
}

/**
 * How hard a mother country's own war in Europe presses her (ruling 222, Brian's ask of 17 November 2026: "it depends
 * on how they are doing with the war in europe"), the step her answer for a dominion is rolled at. Counting from the
 * bottom, the first that holds: 4, she is in danger at home, with an enemy army on her home ground or a third of her
 * will to fight gone; 3, she is at war with a great power of Europe and France is not fighting beside her (`fallen`
 * when France has fallen); 2, she is at war there with France fighting beside her, a France freed and back in the
 * war as much as one that never fell; 1, she is at war with no great power of Europe, as when all her European
 * enemies have fallen or made peace. `answers` is the share of that step (world.json, 'dominions').
 */
export interface PatronPress { step: 1 | 2 | 3 | 4; answers: number; fallen: boolean }

export function patronPress(g: Game, p: string): PatronPress {
  const P = g.nations[p], { europe, beside, willGone } = pressData(p);
  const at = (step: PatronPress['step'], fallen = false): PatronPress => ({ step, answers: answersOf(p, step), fallen });
  if (!P?.alive) return at(1);
  const foe = (u: Corps): boolean => !u.dead && u.kind !== 'fort' && atWar(g, u.nat, p);
  // (In the field, or inside a town of hers it has taken, as the home alarm reads it.)
  const invaded = g.regions.some((r) => r.owner === p && !r.overseas && (r.units.some(foe) || (!allied(g, p, r.ctrl) && (r.garr ?? []).some(foe))));
  if (invaded || (P.resilience > 0 && collapseScore(g, p) >= willGone * P.resilience)) return at(4);
  const enemies = europe.filter((e) => e !== p && g.nations[e]?.alive && atWar(g, p, e));
  if (!enemies.length) return at(1);
  const F = beside ? g.nations[beside] : undefined;
  if (F?.alive && !F.dormant && F.atWar.some((e) => enemies.includes(e))) return at(2);
  return at(3, !F?.alive);
}

/**
 * Whether the mother country `p` answers for the war `a` made on her dominion `b` in the month `war`: the one roll
 * of that declaration, at the step her war in Europe stands at that day (patronPress). It is made from the
 * campaign's seed, the month and the countries, so it moves no other dice, both games agree on it, and the same
 * month loaded again gives the same answer. A war made in an earlier month than this one (a campaign saved before
 * the roll was kept, rolled as a month ends) is rolled at the one share the rule had before ruling 222, six in ten:
 * the state of Europe the day it was made is not kept, and a roll already made stands.
 */
export const dominionAnswers = (g: Game, p: string, a: string, b: string, war: number): boolean =>
  hashString(`dominion:${g.seed}:${war}:${p}:${a}:${b}`) / 4294967296 < (war < g.turn ? answersOf(p) : patronPress(g, p).answers);

/**
 * The mother country answers, or does not, for the war `a` made on her dominion `b` (standsByWhom): one roll for
 * each such war, and both answers are news the same month. `since` is the month that war was made, for a campaign
 * saved before the rule, where the roll is made as a month ends. True if she went to war. She declares nothing
 * else by it: her allies stay as they are, and a country outside both blocs stays outside. Where she stays out
 * there is no second roll while that war lasts (the flag `domroll` keeps the month of the war it was made for);
 * her own staff stays as free as any staff.
 */
export function standsBy(g: Game, a: string, b: string, since?: number): boolean {
  const p = standsByWhom(g, a, b);
  if (!p) return false;
  const war = since ?? g.turn, key = `domroll:${a}:${b}`;
  if (g.flags[key] === war) return false;
  g.flags[key] = war;
  const P = g.nations[p], A = g.nations[a], B = g.nations[b];
  const was = since !== undefined && since < g.turn ? `${theName(A, true)} has been at war with ${B.name} since ${turnToDate(since).label}. ` : '';
  if (!dominionAnswers(g, p, a, b, war)) {
    g.headlines.push({ turn: g.turn, week: g.week, kind: 'info', nat: p, head: `${newsName(P)} stays out of ${B.name}'s war`, text: `${was}${newsName(P)} does not declare war on ${theName(A)}: ${B.name} fights this war without it.` });
    return false;
  }
  setWar(g, p, a);
  g.flags[`war:${p}:${a}`] = g.turn;
  g.headlines.push({ turn: g.turn, week: g.week, kind: 'war', nat: p, head: `${newsName(P)} stands by ${B.name}`, text: `${was}${newsName(P)}${was ? ' now' : ''} declares war on ${theName(A)}: it answers for its dominion.` });
  g.supplyDirty = true;
  return true;
}

/** Armies with standing orders to go to a neighbour's aid march the day that neighbour is attacked. */
function reactionMoves(g: Game, aggressor: string, victim: string): void {
  let moved = 0;
  for (const c of g.corps) {
    if (!c.dyle || c.nat === g.human || !atWar(g, c.nat, aggressor)) continue;
    const to = g.byName[c.dyle];
    if (!to || to.owner !== victim || !allied(g, c.nat, to.ctrl)) continue;
    if (hostileUnits(g, to, c.nat).length || placed(g, to, c.nat).length >= roomAt(g, to, c.nat)) continue;
    place(g, c, to);
    c.dyle = null;
    moved++;
  }
  if (moved) headline(g, 'event', `Allied armies cross the frontier into ${g.nations[victim].name} to meet the attack.`, victim);
}

export const hostileUnits = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => atWar(g, u.nat, nat));
/**
 * True where an enemy of this corps stands on its own province: an army round the town it holds, or a town held
 * against the field it stands in. (The far side's islands ask it: ruling 173(e) is for an island with no enemy ashore.)
 */
export const enemyAshore = (g: Game, c: Corps): boolean => { const r = g.regions[c.reg]; return atWar(g, c.nat, r.ctrl) || r.units.some((u) => !u.dead && atWar(g, u.nat, c.nat)); };

/** Who dug a place's works: the nation that built them; the works on the map from the first day are their country's. */
export const worksBy = (r: Region): string => r.fort?.by ?? r.owner;
/**
 * True where a place's works count for `nat` (ruling 232 rider b624): works count only for the nation that dug them
 * and its side, never for its enemy against that nation's own attacks. A ring that has taken the ground behind a
 * city's lines does not stand in them backwards against the city's relief. Every nation, every stop.
 */
export const worksFor = (g: Game, r: Region, nat: string): boolean => !!r.fort && (sideOf(g, nat, worksBy(r)) || (!!r.fort.held && sideOf(g, nat, r.fort.held)));
const sideOf = (g: Game, nat: string, of: string): boolean => nat === of || allied(g, nat, of);
/** True where a place's works count for whoever stands against `attNat` there: the army in its field, or with none, its holder. */
/**
 * Works taken over (the variant put to the design director beside b624): a side that has held a place outright from one
 * month's end to the next (the place in its hands, no enemy army standing in it, no siege) takes over the works there
 * too (fort.held): Tobruk's Italian perimeter held by Britain in 1941, the French Mareth Line held by the Axis in 1943.
 * The works still count for the side that dug them, the day it takes them back (Kiev and Zhitomir in 1943). A ring
 * in a city's field never holds the place outright while the city's own garrison holds the town, so it never takes the
 * city's lines over (the Moscow case).
 */
export function worksMonth(g: Game): void {
  for (const r of g.regions) {
    const key = `wkh:${r.id}`;
    const holder = r.ctrl;
    const outright = !!r.fort && !worksFor(g, r, holder) && !r.units.some((u) => !u.dead && atWar(g, u.nat, holder)) && !besieged(g, r);
    if (!outright) { if (g.flags[key] !== undefined) delete g.flags[key]; continue; }
    if (g.flags[key] === undefined) { g.flags[key] = g.turn; continue; }
    if (g.turn > g.flags[key]) { r.fort = { ...r.fort!, held: holder }; delete g.flags[key]; }
  }
}
export function worksAgainst(g: Game, r: Region, attNat: string): boolean {
  if (!r.fort) return false;
  const foes = hostileUnits(g, r, attNat);
  return foes.length ? foes.some((u) => worksFor(g, r, u.nat)) : worksFor(g, r, r.ctrl);
}
export const friendlyUnits = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => allied(g, u.nat, nat));
/**
 * The fortress rows (the design director's 05:38Z, 06:12Z and 06:15Z): the book's fortress crews by date, their works
 * holding all round and kept when beaten. On with their switch (stage.ts FORTS_STAGE), set as a grand campaign opens;
 * off, the game is release 1's.
 */
export const fortRows = (g: Game): boolean => g.flags.open === 1 && g.flags.fortRows === 1;
/** True for a fortress row under the fortress rows' rules: any corps of the fortress kind, every nation's, the book's rows and release 1's alike. */
export const fortRow = (g: Game, u: Corps): boolean => u.kind === 'fort' && g.flags.fortRows === 1 && g.flags.open === 1;
/** True for a corps that takes no place among the five a province holds: fortress troops, as engineers do not (06:12Z). */
export const roomless = fortRow;
/**
 * The fortress rows' share of a defence (06:15Z (a)), by their base strength against everyone's: their works face
 * every attacker, those come from behind as well, while the field corps in the intervals keep the outflanked rule.
 */
/**
 * How many months a fortified port overseas feeds its garrison after the sea behind it closes: its own two, or under
 * the fortress rows the stores of the row it holds (the design director's 07:12Z: the historian's figure is the place's
 * whole stores, so it is counted once).
 */
export function portKeep(g: Game, r: Region): number {
  return (fortRows(g) ? portStores(g, r) : null) ?? DESERT.stores;
}
/**
 * A fortress crew in a fortified port overseas living on the port's stores this month (07:12Z): the sea behind it is
 * shut and the port still feeds it from what it holds. Its own clock runs from the month the sea closed: it eats a
 * month of its stores as a crew cut off does (09:06Z: counted apart from its organisation, which it holds), it neither rests
 * nor takes in drafts, and it is gone in the month the stores run out.
 */
export function onPortStores(g: Game, c: Corps): boolean {
  if (!fortRow(g, c) || !c.supplied) return false;
  const r = g.regions[c.reg], last = g.flags[`stores:${r.id}`];
  return r.overseas && !!r.port && !!r.fort && last !== undefined && last < g.turn && g.turn - last <= portKeep(g, r);
}
export function fortShare(g: Game, defenders: Corps[]): number {
  let all = 0, forts = 0;
  for (const d of defenders) {
    const p = corpsBase(g, d) * KIND[d.kind].def;
    all += p;
    if (fortRow(g, d)) forts += p;
  }
  return all > 0 ? forts / all : 0;
}
/** The corps of `nat`'s side in a province that take a place among the five. */
export const placed = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => allied(g, u.nat, nat) && !roomless(g, u));

/** The army at home and on the Continent, in divisions. Troops in the colonies are not there to defend the homeland, and do not count. */
export function armySize(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat && !g.regions[c.reg].overseas) s += c.divs * c.str;
  return s;
}

/** Every division a nation has in the field, wherever it stands. */
export function fieldArmy(g: Game, nat: string): number {
  let s = 0;
  for (const c of g.corps) if (c.nat === nat) s += c.divs * c.str;
  // (Corps at sea on a long passage are still the nation's army: ruling 229, part 1.)
  for (const v of g.atSea ?? []) if (v.nat === nat) s += v.corps.divs * v.corps.str;
  return s;
}

/**
 * The Western Desert's own rules (desert.json: supply counted from the port, every bound shorter, no railway) are
 * for the desert of the map of Europe alone. Desert ground on the wider world (ruling 212, the cities version) is the
 * ground's own row: it moves, fights and digs in as desert, but its supply is counted as anywhere else, and a
 * railway drawn across it runs. Keyed by the chart, so nothing on Europe's map moves.
 */
export const sandRules = (r: Region): boolean => r.terrain === DESERT.terrain && (!beyondEurope(r) || !!FAR_SAND?.has(r.name));
/**
 * The far side (the design director's rulings of 21:55Z to 22:20Z of 21 November 2026; data/campaign/far-sand.json),
 * amending ruling 212 for the fifty provinces named there. As keyed by the chart alone, the sand the old chart's
 * desert runs on into followed another rule than the sand it left: a beaten corps at Kufra walked four desert
 * provinces of Britain's in a month and paid no drag, and its line back to Tripoli read no length. It was also free
 * ground round the old chart's edge, so with the far side on seven of the old chart's own lines read short on the
 * first day (Siwa 1 for 3, Jalo and Jarabub 2 for 4, Mersa Matruh 1 for 2, Sidi Barrani 2 for 3, El Agheila 2 for
 * 3, Ouargla 1 for 2). The added provinces of desert ground joined to the old chart's desert by unbroken desert
 * ground (`sand`) read the desert's rules as the old chart's desert does: the line, the drag and the bands. The old
 * chart's desert then reads the same lines with the far side on as off. Those of the fifty the record had on a
 * working railway or a river line (`rail`) keep the trains the chart draws there (212's "a drawn railway runs"),
 * and no more: the open sand round the strip pays like the rest. The desert that stands apart takes nothing. With
 * the far switch off neither list is read.
 */
const FAR_SAND: Set<string> | null = FAR_STAGE ? new Set((farSandData as unknown as { sand: string[] }).sand) : null;
const FAR_SAND_RAIL: Set<string> | null = FAR_STAGE ? new Set((farSandData as unknown as { rail: string[] }).rail) : null;
/** "No railway crosses the desert": the desert's rule for trains. On the far side a province named for its railway or river line keeps the trains the chart draws. */
export const sandNoRail = (r: Region): boolean => sandRules(r) && !(beyondEurope(r) && !!FAR_SAND_RAIL?.has(r.name));
/** True where the desert's supply rules apply: the desert of Europe's map, and the overseas coast that feeds it. */
export const inDesert = (r: Region): boolean => r.overseas || sandRules(r);

export function industry(g: Game, nat: string): number {
  let s = 0;
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat) s += r.fac;
  return s;
}

export function freshOffensive(g: Game, nat: string, enemy?: string): boolean {
  const n = g.nations[nat];
  if (!n.blitz) return false;
  const list = enemy ? [enemy] : n.atWar;
  return list.some((e) => n.offStart[e] !== undefined && g.turn - n.offStart[e] <= 1);
}

/**
 * The ground's weather where a land battle is fought, in the 3D game's words (release 2, looks only): 'mud' in the
 * east's thaw, 'snow' in a month its weather zone lies under snow, else with the skies on 'rain' under a closed sky and
 * 'clear' under a clear one; '' where nothing is known. The east by the weather zones' map (northern and southern
 * Russia), so a field keeps its seasons whoever holds it.
 */
export function groundWeather(g: Game, r: Region): string {
  const zone = WEATHER.zones[zoneOfRegion(g, r.id)];
  const m = turnToDate(g.turn).month;
  if (zone && (zone.key === 'nrus' || zone.key === 'srus') && weather(g).mud) return 'mud';
  if (zone && zone.snow.includes(m + 1)) return 'snow';
  if (!weatherOn(g)) return '';
  const sky = skyAt(g, r.id);
  return sky === 'closed' ? 'rain' : sky === 'clear' ? 'clear' : '';
}

/**
 * Captured kit (release 2, looks only): after a fought battle won heavily over an army with tanks, the winner keeps a
 * few of them (1 to 3 tank blocks to be drawn as the loser's model in the 3D game), at even odds for a heroic win or
 * a rout of the attack, and a third for a decisive win; kept six months, the three latest for each nation.
 */
function capturedKit(g: Game, rep: BattleReport, target: Region): void {
  const attWon = rep.level.endsWith('victory');
  const heavy = rep.level === 'Heroic victory' || rep.level === 'Decisive defeat' ? 50 : rep.level === 'Decisive victory' ? 33 : 0;
  if (!heavy) return;
  const losers = (attWon ? rep.defenders : rep.attackers).filter((u) => u.kind === 'arm' && u.divs > 0);
  if (!losers.length) return;
  const throwOf = (rep.id * 7919 + target.id * 104729 + g.turn * 31) % 100;
  if (throwOf >= heavy) return;
  const winner = attWon ? rep.attNat : rep.defNat;
  const from = losers.reduce((a, b) => (b.divs > a.divs ? b : a)).nat;
  if (from === winner) return;
  const n = Math.min(3, Math.max(1, Math.round(losers.reduce((s2, u) => s2 + u.divs, 0))));
  const book = (g.captured ??= {});
  book[winner] = [...(book[winner] ?? []).filter((k) => g.turn - k.turn < 6).slice(-2), { from, n, turn: g.turn }];
}

/**
 * Old wrecks (release 2, looks only; Brian, 23 November 20:09Z): the armoured vehicles a land battle leaves burnt out or
 * knocked out on its field, for the next battle there. Estimated from the losses the battle booked, so no die is thrown:
 * an armoured division fields about 'per.arm' armoured vehicles and a motorised one about 'per.mot' (battle/land/wrecks.ts),
 * and a corps loses them in the share of its men it lost. The side that holds the field after it tows 'towed' of its own
 * away to be mended; the side that leaves it leaves all of its. A ticket shows at most 'most' (the 3D game's own cap). Nothing in any fight reads it.
 */
export const WRECKS = { per: AFV_PER_DIV as Partial<Record<Kind, number>>, towed: 0.5, most: 60 } as const;

/** One battle's wrecks, by nation, booked on its region (and the month of the last battle that left any). */
export function wreckBump(g: Game, rep: BattleReport): void {
  if (rep.yielded) return;
  const attHolds = rep.level.endsWith('victory');
  const left: Record<string, number> = {};
  const side = (us: ReportUnit[], holds: boolean) => {
    for (const u of us) {
      const per = WRECKS.per[u.kind] ?? 0;
      if (!per || u.guard || u.divs <= 0 || u.men <= 0 || u.lost <= 0) continue;
      left[u.nat] = (left[u.nat] ?? 0) + per * u.divs * Math.min(1, u.lost / u.men) * (holds ? 1 - WRECKS.towed : 1);
    }
  };
  side(rep.attackers, attHolds);
  side(rep.defenders, !attHolds);
  const whole = Object.entries(left).map(([nat, n]) => [nat, Math.round(n)] as const).filter(([, n]) => n > 0);
  if (!whole.length) return;
  const was = (g.wrecks ??= {})[rep.region];
  const n = { ...(was?.n ?? {}) };
  for (const [nat, k] of whole) n[nat] = (n[nat] ?? 0) + k;
  g.wrecks[rep.region] = { n, last: Math.max(was?.last ?? rep.turn, rep.turn) };
}

/**
 * A region's old wrecks as the battle ticket carries them: both sides' together, at most WRECKS.most, each nation's share
 * of that (largest remainders, so they add up to the count), and the months since the last battle that left any.
 */
export function wreckLook(w: { n: Record<string, number>; last: number } | undefined, turn: number): { count: number; months: number; nats: Record<string, number> } | null {
  if (!w) return null;
  const rows = Object.entries(w.n).filter(([, k]) => k > 0).sort((a, b) => b[1] - a[1] || (a[0] < b[0] ? -1 : 1));
  const all = rows.reduce((s2, [, k]) => s2 + k, 0);
  if (all <= 0) return null;
  const count = Math.min(WRECKS.most, Math.round(all));
  const exact = rows.map(([nat, k]) => [nat, (k * count) / all] as const);
  const nats: Record<string, number> = {};
  let given = 0;
  for (const [nat, x] of exact) { nats[nat] = Math.floor(x); given += nats[nat]; }
  for (const [nat, x] of [...exact].sort((a, b) => (b[1] - Math.floor(b[1])) - (a[1] - Math.floor(a[1])) || (a[0] < b[0] ? -1 : 1))) {
    if (given >= count) break;
    nats[nat] += 1; given += 1;
  }
  for (const nat of Object.keys(nats)) if (nats[nat] <= 0) delete nats[nat];
  return { count, months: Math.max(0, turn - w.last), nats };
}

export function weather(g: Game): { mud: boolean; winter: boolean; text: string } {
  const m = turnToDate(g.turn).month;
  if (EASTERN.mud.includes(m)) return { mud: true, winter: false, text: 'Mud in the east slows every army there.' };
  if (EASTERN.winter.months.includes(m)) return { mud: false, winter: true, text: 'Winter in the east: armies without winter kit fight and march badly there.' };
  return { mud: false, winter: false, text: 'Clear campaigning weather.' };
}

/**
 * Whose ground takes the long line in this war: the Soviet Union's at every stop, and that of the owners in east.json
 * 'lineOwners' at the dial's stops in 'lineStops' (ruling 221, L). The Russian winter stays the Soviet Union's alone.
 */
export function lineOwners(g: Game): string[] {
  const more = EASTERN.lineOwners ?? [];
  if (!more.length || (EASTERN.lineStops && !EASTERN.lineStops.includes(stopOf(g)))) return EASTERN.deepOwners;
  return [...EASTERN.deepOwners, ...more];
}
export const lineOwner = (g: Game, o: string): boolean => lineOwners(g).includes(o);

/** True when this corps is campaigning on Soviet ground in winter without the equipment for it. */
export function frozen(g: Game, nat: string, r: Region): boolean {
  return weather(g).winter && EASTERN.deepOwners.includes(r.owner) && !EASTERN.winter.hardy.includes(nat) && !winterReady(g, nat);
}

/**
 * Extra movement cost for pushing on beyond the railheads: fuel and shells arrive slower than the
 * tanks can drive. It is paid on enemy ground and ground taken this month; marching up behind
 * the front through country already held is an ordinary march.
 */
export function lineDrag(g: Game, c: Corps, to: Region): number {
  if (sandRules(to)) {
    // In the desert the same holds for everyone: the further from the port, the shorter each bound.
    if (allied(g, c.nat, to.ctrl) && to.since < g.turn) return 0;
    ensureSupply(g);
    const reach = g.reach[c.nat];
    if (!reach) return 0;
    const length = allied(g, c.nat, to.ctrl) ? reach[to.id] : reach[c.reg] + DESERT.step;
    for (const [max, extra] of DESERT.drag) if (length <= max) return extra;
    return 0;
  }
  const long = lineOwners(g);
  if (!long.includes(to.owner) || long.includes(c.nat)) return 0;
  if (allied(g, c.nat, to.ctrl) && to.since < g.turn) return 0;
  ensureSupply(g);
  const reach = g.reach[c.nat];
  if (!reach) return EASTERN.roads;
  let length = reach[to.id];
  const roads = EASTERN.roads;
  if (!allied(g, c.nat, to.ctrl)) {
    const w = weather(g);
    length = reach[c.reg] + EASTERN.stepFresh * (w.mud || w.winter ? EASTERN.badWeather : 1);
  }
  for (const [max, extra] of EASTERN.drag) if (length <= max) return roads + extra;
  return roads;
}

/** How strong an attack is at the end of this corps' supply line: 1 at the frontier, less deep inside the Soviet Union. */
export function lineStrength(g: Game, c: Corps): { mul: number; length: number } {
  const length = g.reach[c.nat]?.[c.reg] ?? 0;
  // In the desert the line is counted from the nearest working port, and has its own scale.
  for (const [max, mul] of inDesert(g.regions[c.reg]) ? DESERT.bands : EASTERN.bands) if (length <= max) return { mul: cmdLine(g, c, mul), length };
  return { mul: 1, length };
}

export function moveCost(g: Game, c: Corps, to: Region, from = c.reg): number {
  // Marching through country that has been friendly since before this month is quick for anyone.
  const road = allied(g, c.nat, to.ctrl) && to.since < g.turn;
  let cost = road ? 1 : KIND[c.kind].move;
  // Off the roads every arm has ground it cannot cross quickly (the grand campaign's ground rules).
  if (!road) cost += groundMarch(g, c.kind, to);
  if (groundRules(g)) {
    // A river whose bridges are down is crossed in boats, and an enemy's minefields are felt for yard by yard.
    if (downBetween(g, from, to.id)) cost += ENGINEERING.blown.march;
    if (!allied(g, c.nat, to.ctrl) && minesAt(g, to)) cost += ENGINEERING.mines.march;
  }
  if (to.terrain === 'mountain' && c.kind !== 'mtn') cost += 1;
  if (to.terrain === 'marsh') cost += 1;
  if (weather(g).mud && EAST.has(to.owner)) cost += 1;
  if (frozen(g, c.nat, to) && cmdWinter(g, c) >= 1) cost += 1;
  cost += lineDrag(g, c, to);
  return cost;
}

// ---- Supply -------------------------------------------------------------------------

/**
 * Each country's last supply picture, kept with what it was drawn from (the map-speed lane's month-end profile, 24 Nov
 * 2026: the picture was drawn for every country at every country's turn, and almost every time came out the same). A
 * picture is drawn again only when something it is drawn from has changed: which provinces its roads may run through,
 * where its stores are and the head start each gives in the desert, and, for a picture reaching into the east, how the
 * railways there stand. The stores of the ports overseas, which corps are fed, the garrisons and the railheads are worked
 * out at every call as before. Nothing it gives differs from drawing every picture afresh: TC_SUPPLY_CHECK=1 draws every
 * kept picture again and stops the run on any difference.
 * - Each picture keeps the railways it was drawn with, not the last call's: a country with no corps for a while is not
 *   looked at, and its picture must not outlive a railway that changed meanwhile.
 * - The railways and the desert are worked out once a call, and the very arrays that key the kept pictures are the ones
 *   the pictures are drawn from (reachInto, desertLines), so a rule that moves a step cannot miss the key.
 * - The roads between provinces are not keyed: they change only when the wider world is attached (attachWorld), which
 *   also adds provinces, and a new number of provinces starts the book afresh.
 * - A kept picture is handed out again as the same arrays: nothing may write into g.supply or g.reach in place.
 *
 * Which provinces a country's roads may pass is a province's holder and whether that holder is the country's friend
 * (the far chart's profile, 24 Nov 2026: with 65 countries and 2,067 provinces, the provinces were laid out and compared
 * afresh for every country at every call). So the holders are numbered once for the whole campaign, every call writes
 * down each province's holder by number, and the book logs the provinces whose holder changed since the call before. A
 * kept picture remembers the friends it was last found good with and how far the log had got; it still holds when the
 * friends are the same and every province logged since passes as it did. Both are read afresh at every call: a capture,
 * a surrender, a hand-over or a change of alliance anywhere is in them at the next call. When the friends differ the
 * provinces are laid out and compared in full, as before. (Nothing here reads the towns' hand:<id> stamps.)
 *
 * The seas each country's ships can use (openSeas) are kept too: with the seas they set out from (seaStarts, worked out
 * at every call), whether each strait would let them through (canPass, asked at every call), and the answer to every
 * "is this sea closed to them" the walk asked. While those are all the same, the walk would ask the same and find the
 * same. A sea a kept answer was had in is looked at whenever that answer is (SeaPicture.state: its fleets at sea and
 * their weight, the coasts that put aircraft over it, its sky) and stamped with the call that found it changed; a kept
 * answer is asked again when its sea has changed since it was last found good, or when anything allied() or atWar()
 * reads has changed since (friendsKeyOf, which names every country's alliance, life and wars). A country that loses its
 * last port, or the shipyard ground it sails from, sets out from other seas or none, and so walks again.
 * TC_SUPPLY_CHECK=1 walks the seas afresh as well and stops on any difference.
 */
/** The eastern railways as they stand this call: what each Soviet-owned step of the line costs, and whose home it is. */
interface RailNow { step: Float64Array; home: Uint8Array }
/**
 * What a country's roads may pass (mask: 1 where the holder is its friend), with the friends it was last found good
 * with (by the holders' numbers) and the book's log as far as it had got then.
 */
interface RoadsSeen { mask: Uint8Array; friend: Uint8Array; at: number }
interface SupplyDrawn extends RoadsSeen { seeds: number[]; heads: number[]; ok: Uint8Array; reach: Float32Array; east: boolean; rail: RailNow | null }
/**
 * A country's sea lanes, with what its walk set out from, every answer it had on the way, and the call at which those
 * answers were last found good, with every country's friends and wars then (fkey, friendsKeyOf).
 */
interface LanesKept { starts: number[]; gates: Uint8Array; asked: Int16Array; shut: Uint8Array; open: Uint8Array; at: number; fkey: string }
/** The provinces a country's roads join to its capital (roadsHome), by holders (mask: 1 where a friend holds it). */
interface HomeKept extends RoadsSeen { cap: number; linked: Uint8Array }
interface SupplyBook {
  size: number; wide: boolean; long: string; seas: Game['seas']; gates: Game['gates']; deep: Uint8Array; sand: Uint8Array; rail: RailNow; drawn: Map<string, SupplyDrawn>;
  /** Each province's holder at the last call: on the field (road, as the pictures go) and in the books (own, ctrl). */
  road: Int16Array; own: Int16Array;
  /** The provinces whose holder changed from one call to the next, in turn; log[0] is entry number logBase. */
  log: number[]; logBase: number;
  lanes: Map<string, LanesKept>; home: Map<string, HomeKept>;
  /** The calls made on this book, counted; each sea's state when last looked at, and the call that found it changed. */
  calls: number; seaSeen: (SeaState | null | undefined)[]; seaStamp: Int32Array;
  /** Each country's friends among every holder numbered (friendsFor of them), good while friendsKey (friendsKeyOf) is the same. */
  friends: Map<string, Uint8Array>; friendsKey: string; friendsFor: number;
}
const supplyBooks = new WeakMap<Game, SupplyBook>();
/** Every holder a campaign's supply has seen, numbered in the order seen: the numbers never change. */
const supplyHolders = new WeakMap<Game, { no: Map<string, number>; list: string[] }>();
const supplyCheckEnv = (globalThis as { TC_SUPPLY_CHECK?: unknown }).TC_SUPPLY_CHECK ?? (globalThis as { process?: { env?: Record<string, string | undefined> } }).process?.env?.TC_SUPPLY_CHECK;
const supplyCheck = supplyCheckEnv !== undefined && !!supplyCheckEnv && supplyCheckEnv !== '0';
/**
 * Everything allied() and atWar() read of the war, for every country: its alliance (bloc), whether it is alive and whom
 * it is at war with (far.ts tiedPair's tie, and atWar itself), and whether the far side is on. While this is the same,
 * so is every answer the two give (a change to either must be followed here; tests/friends-key-r2 records their reads,
 * and TC_SUPPLY_CHECK asks every kept answer afresh).
 */
export const friendsKeyOf = (g: Game): string => {
  let key = farOn(g) ? 'far' : 'near';
  for (const id in g.nations) { const n = g.nations[id]; key += `|${id}:${bloc(n)}:${n.alive ? 1 : 0}:${n.atWar.join(',')}`; }
  return key;
};
const sameBytes = (a: ArrayLike<number>, b: ArrayLike<number>): boolean => {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) if (a[i] !== b[i]) return false;
  return true;
};
/** The flags' names a call asks after for every province, made once (the same names, without building them again). */
const depotKeys: string[] = [], storesKeys: string[] = [];
const depotKey = (id: number): string => (depotKeys[id] ??= 'depot:' + id);
const storesKey = (id: number): string => (storesKeys[id] ??= `stores:${id}`);

/** One country's supply picture from its stores outward over the provinces its roads may pass, and its lines' lengths. */
function drawSupply(g: Game, id: string, pass: Uint8Array, seeds: number[], heads: number[], wide: boolean, long: string[], deep: Uint8Array, sand: Uint8Array, rail: RailNow): { ok: Uint8Array; reach: Float32Array; east: boolean } {
  const size = g.regions.length;
  const ok = new Uint8Array(size);
  const q: number[] = [];
  let east = false, desert = false;
  for (const at of seeds) {
    // In the grand campaign the table also remembers where the stores are: the railheads are counted from there.
    ok[at] = wide ? 2 : 1; q.push(at);
    if (deep[at]) east = true;
    if (sand[at]) desert = true;
  }
  for (let i = 0; i < q.length; i++) {
    for (const j of g.regions[q[i]].nb) {
      if (ok[j] || !pass[j]) continue;
      ok[j] = 1; q.push(j);
      if (deep[j]) east = true;
      if (sand[j]) desert = true;
    }
  }
  // A country with no supplied ground inside the Soviet Union has no line there to measure, and one with none in
  // the desert or overseas has none there: both tables stay empty, as the long way round would leave them.
  const reach = east ? reachInto(g, id, ok, long, deep, rail) : new Float32Array(size);
  if (desert) {
    const head = new Float32Array(size).fill(-1);
    for (let i = 0; i < heads.length; i += 2) head[heads[i]] = heads[i + 1];
    desertLines(g, ok, head, sand, reach);
  }
  return { ok, reach, east };
}

export function computeSupply(g: Game): void {
  const active = new Set(g.corps.map((c) => c.nat));
  const withShips = Object.values(g.nations).filter((n) => n.alive && (n.seaSupply || n.sealift > 0)).map((n) => n.id);
  const shippers = (id: string): string[] => withShips.filter((a) => a !== id && allied(g, id, a));
  const wide = groundRules(g);
  // The grand campaign: where an army stands round a town it has not taken, the roads are that army's. Its supply
  // passes through; the town's holder draws none there and sends none on.
  const field = siegeRules(g) ? g.regions.map((r) => fieldCtrl(g, r)) : null;
  // Worked out once for every country's picture: whose roads each province's are (the holders numbered, so that
  // "is he my friend" is asked once a holder and not once a province), and where the long lines of the east and of
  // the desert can run at all. With fifty countries on the chart the picture is drawn fifty times over.
  const size = g.regions.length;
  let named = supplyHolders.get(g);
  if (!named) supplyHolders.set(g, (named = { no: new Map(), list: [] }));
  const holderNo = named.no, holders = named.list;
  // (here: which holders hold anything this call; only theirs is asked after below)
  const here: number[] = [];
  let lastWho = '', lastNo = -1;
  const numberOf = (who: string): number => {
    if (who === lastWho) return lastNo;
    let k = holderNo.get(who);
    if (k === undefined) { k = holders.length; holderNo.set(who, k); holders.push(who); }
    if (here[k] !== 1) here[k] = 1;
    lastWho = who; lastNo = k;
    return k;
  };
  const road = new Int16Array(size), own = field ? new Int16Array(size) : road, deep = new Uint8Array(size), sand = new Uint8Array(size);
  const long = lineOwners(g);
  // The provinces that can be anybody's source of supply at all, in order, and whose roads each is on: a port, or a
  // capital, a big industrial province or a depot standing on its owner's own ground. Every other province only carries the line.
  const sources: number[] = [], sourceRoad: number[] = [], sourcesBy: number[][] = [];
  for (const r of g.regions) {
    const k = numberOf(field ? field[r.id] : r.ctrl);
    road[r.id] = k;
    if (field) own[r.id] = numberOf(r.ctrl);
    if (long.includes(r.owner)) deep[r.id] = 1;
    // Desert ground (2) costs a step of the line; the coast overseas (1) only carries it. Either is the desert's table.
    sand[r.id] = (r.overseas ? 1 : 0) | (sandRules(r) ? 2 : 0);
    if (r.port || (r.ctrl === r.owner && (r.cap || r.fac >= 3 || !!g.flags[depotKey(r.id)]))) { sources.push(r.id); sourceRoad.push(k); (sourcesBy[k] ??= []).push(r.id); }
  }
  // What each step of the long line costs this month, on Soviet-owned ground: a region held long enough has had its
  // track relaid (in the grand campaign, unless the line there is torn up), and mud and winter lengthen every step.
  const w = weather(g), bad = w.mud || w.winter ? EASTERN.badWeather : 1;
  const step = new Float64Array(size), home = new Uint8Array(size);
  for (let j = 0; j < size; j++) {
    if (!deep[j]) continue;
    const r = g.regions[j];
    if (EASTERN.deepOwners.includes(r.owner)) home[j] = 1;
    step[j] = (g.turn - lineSince(g, j, r.since) >= EASTERN.convertAfter && !railCut(g, j) ? EASTERN.stepConverted : EASTERN.stepFresh) * bad;
  }
  // The pictures kept from the last call hold while the chart, the rules, the long line's owners and the desert are the
  // same; those reaching into the east hold only while every eastern step costs what it did when they were drawn.
  let book = supplyBooks.get(g);
  const longKey = `${long.join('|')}/${DESERT.step}`;
  if (!book || book.size !== size || book.wide !== wide || book.long !== longKey || book.seas !== g.seas || book.gates !== g.gates || !sameBytes(book.deep, deep) || !sameBytes(book.sand, sand)) {
    book = { size, wide, long: longKey, seas: g.seas, gates: g.gates, deep, sand, rail: { step, home }, drawn: new Map(), road, own, log: [], logBase: 0, lanes: new Map(), home: new Map(), calls: 0, seaSeen: [], seaStamp: new Int32Array(g.seas.length), friends: new Map(), friendsKey: '', friendsFor: -1 };
    supplyBooks.set(g, book);
  } else {
    // The provinces whose holder has changed since the last call, on the field or in the books.
    const was = book.road, wasOwn = book.own;
    for (let j = 0; j < size; j++) if (was[j] !== road[j] || wasOwn[j] !== own[j]) book.log.push(j);
    book.road = road; book.own = own;
    // A long log is let go: what was kept from before it is then compared in full, once.
    if (book.log.length > 4 * size) { book.logBase += book.log.length; book.log = []; }
  }
  const kept0 = book;
  const logEnd = book.logBase + book.log.length;
  const callNo = ++book.calls;
  // The railways now: the same object as the last call's while nothing on them has changed, so most checks are one look.
  let rail = book.rail;
  if (!sameBytes(rail.step, step) || !sameBytes(rail.home, home)) book.rail = rail = { step, home };
  const railHolds = (then: RailNow | null): boolean => !then || then === rail || (sameBytes(then.step, rail.step) && sameBytes(then.home, rail.home));
  // Each country's friends among every holder numbered, asked once a country while what allied() reads of the war is
  // the same as when they were asked (friendsKeyOf: an alliance, a surrender, a war or the far side's ties change it,
  // and the tables are asked afresh) and no new holder has been numbered. (Only this call's holders are looked at by
  // number below; the others' entries are never read.)
  const friendsKey = friendsKeyOf(g);
  if (book.friendsKey !== friendsKey || book.friendsFor !== holders.length) { book.friends = new Map(); book.friendsKey = friendsKey; book.friendsFor = holders.length; }
  const friends = book.friends;
  const friendOf = (id: string): Uint8Array => {
    let friend = friends.get(id);
    if (!friend) {
      friend = new Uint8Array(holders.length);
      for (let k = 0; k < holders.length; k++) if (allied(g, id, holders[k])) friend[k] = 1;
      friends.set(id, friend);
    } else if (supplyCheck) {
      for (let k = 0; k < holders.length; k++) if (friend[k] !== (allied(g, id, holders[k]) ? 1 : 0)) throw new Error(`friends kept for ${id} in month ${g.turn} differ from fresh ones (${holders[k]})`);
    }
    return friend;
  };
  const maskOf = (friend: Uint8Array, by: Int16Array): Uint8Array => {
    const mask = new Uint8Array(size);
    for (let j = 0; j < size; j++) mask[j] = friend[by[j]];
    return mask;
  };
  // True while what a country's roads may pass is what it was: the same friends among this call's holders, and every
  // province logged since passing as it did (any other province has the same holder as then, so one of this call's);
  // or, the friends being changed, every province compared. Kept now as good for this call.
  const roadsHold = (seen: RoadsSeen, friend: Uint8Array, by: Int16Array): boolean => {
    let same = seen.at >= kept0.logBase && seen.friend.length <= friend.length;
    for (let k = 0; same && k < seen.friend.length; k++) if (here[k] === 1 && seen.friend[k] !== friend[k]) same = false;
    if (same) {
      const log = kept0.log;
      for (let i = seen.at - kept0.logBase; i < log.length; i++) { const j = log[i]; if (friend[by[j]] !== seen.mask[j]) return false; }
    } else {
      for (let j = 0; j < size; j++) if (friend[by[j]] !== seen.mask[j]) return false;
    }
    seen.friend = friend; seen.at = logEnd;
    return true;
  };
  // The provinces a country's roads join to its capital, for the harbours it has taken (seaStarts): walked again only
  // when its capital moves or a province changes from friend's to not or back.
  const homeOf = (nat: string): Uint8Array => {
    const cap = g.nations[nat].capital, friend = friendOf(nat);
    const kept = kept0.home.get(nat);
    if (kept && kept.cap === cap && roadsHold(kept, friend, own)) return kept.linked;
    const mask = maskOf(friend, own);
    const linked = roadsHome(g, nat, mask);
    kept0.home.set(nat, { cap, mask, friend, at: logEnd, linked });
    return linked;
  };
  // The seas each nation's supply ships can still use, worked out once a call: an ally's overseas ports feed its friends too.
  const lanes = new Map<string, Uint8Array | null>();
  const seasNow = new SeaPicture(g);
  // Each sea as it stands this call, looked at once and set beside its state when last looked at: the call that found it
  // changed is the sea's stamp. A kept answer about a sea not changed since it was last found good, while every
  // country's friends and enemies are the same, is the answer closed() would give now (SeaPicture.state). A sea is
  // looked at in every call that takes a kept answer about it, or makes one, so its stamp is later than an answer's
  // call whenever the sea has been seen to change since.
  const looked = new Uint8Array(g.seas.length);
  const look = (z: number): void => {
    if (looked[z]) return;
    looked[z] = 1;
    const now = seasNow.state(z), was = kept0.seaSeen[z];
    if (was === undefined || !sameSeaState(was, now)) { kept0.seaSeen[z] = now; kept0.seaStamp[z] = callNo; }
  };
  const lanesKept = (id: string): Uint8Array => {
    const starts = seaStarts(g, id, seasNow, homeOf);
    const gates = new Uint8Array(g.gates.length);
    for (let i = 0; i < gates.length; i++) if (canPass(g, g.gates[i].a, g.gates[i].b, id)) gates[i] = 1;
    const kept = kept0.lanes.get(id);
    let open: Uint8Array | null = null;
    if (kept && sameBytes(kept.starts, starts) && sameBytes(kept.gates, gates)) {
      // (closed() asks atWar() and allied() of the nations in a sea: both read only what friendsKeyOf names.)
      const rels = kept.fkey === friendsKey;
      open = kept.open;
      for (let i = 0; i < kept.asked.length; i++) {
        const z = kept.asked[i];
        look(z);
        if ((!rels || kept0.seaStamp[z] > kept.at) && seasNow.closed(z, id) !== (kept.shut[i] === 1)) { open = null; break; }
      }
      if (open) { kept.at = callNo; kept.fkey = friendsKey; }
    }
    if (!open) {
      const asked = new Int8Array(g.seas.length).fill(-1);
      open = openSeas(g, id, seasNow, undefined, asked, starts);
      const at: number[] = [];
      for (let z = 0; z < asked.length; z++) if (asked[z] >= 0) { at.push(z); look(z); }
      kept0.lanes.set(id, { starts, gates, asked: Int16Array.from(at), shut: Uint8Array.from(at, (z) => asked[z]), open, at: callNo, fkey: friendsKey });
    }
    if (supplyCheck && !sameBytes(openSeas(g, id, seasNow), open)) throw new Error(`sea lanes kept for ${id} in month ${g.turn} differ from fresh ones`);
    return open;
  };
  const lanesOf = (id: string): Uint8Array | null => {
    if (!lanes.has(id)) { const n = g.nations[id]; lanes.set(id, n && (n.seaSupply || n.sealift > 0) ? lanesKept(id) : null); }
    return lanes.get(id)!;
  };
  // The sources on a country's friends' roads, in order: one friend's own list when only one has any.
  const sourcesFor = (friend: Uint8Array): number[] => {
    let one: number[] | null = null;
    for (let k = 0; k < sourcesBy.length; k++) {
      if (!sourcesBy[k] || !friend[k]) continue;
      if (one) {
        const out: number[] = [];
        for (let i = 0; i < sources.length; i++) if (friend[sourceRoad[i]]) out.push(sources[i]);
        return out;
      }
      one = sourcesBy[k];
    }
    return one ?? [];
  };
  for (const id of active) {
    const friend = friendOf(id);
    // Where the supply comes from, and in the desert how much of a head start each base gives: [province, start].
    const seeds: number[] = [];
    const heads: number[] = [];
    const sunk = convoyDrag(g, id);
    // A nation with ships can also be fed through any friendly port its supply ships can still reach.
    const open = lanesOf(id);
    let mates: string[] | null = null;
    for (const at of sourcesFor(friend)) {
      const r = g.regions[at];
      // An army round a town it has not taken holds the beaches beside the harbour and no more: ships can still
      // land its stores there, but the place is no depot of anybody's while the siege lasts.
      const shut = !!field && field[r.id] !== r.ctrl;
      // An army is fed from its own country: its capital, its big industrial regions, its depots.
      const home = !shut && r.ctrl === id && r.owner === id;
      const depot = home && (r.cap || r.fac >= 3 || !!g.flags[depotKey(r.id)]);
      let quay = !!open && r.port && r.seas.some((z) => open[z]);
      // Overseas, an ally's working port feeds the troops who came to fight beside him.
      if (!quay && !shut && r.overseas && r.port && r.ctrl !== id) { const theirs = lanesOf(r.ctrl); quay = !!theirs && r.seas.some((z) => theirs[z]); }
      // In a side theatre the allies share their shipping: a port any of them can still reach feeds them all.
      if (!quay && r.theatre >= 0 && r.port) quay = (mates ??= shippers(id)).some((a) => { const theirs = lanesOf(a); return !!theirs && r.seas.some((z) => theirs[z]); });
      // A fortified port overseas keeps stores: with the sea behind it closed, it feeds its garrison a while longer.
      // (Under the fortress rows, a port that holds a row keeps that row's stores: one stores, not two. 07:12Z.)
      if (!shut && r.overseas && r.port && worksKept(r)) {
        if (quay) g.flags[storesKey(r.id)] = g.turn;
        else if (g.turn - (g.flags[storesKey(r.id)] ?? -99) <= portKeep(g, r)) quay = true;
      }
      if (!depot && !quay) continue;
      // A pocket that has eaten its stores feeds nobody (the grand campaign): its depot is a name, and no ship comes in.
      if (field && r.ctrl === id && pocketDry(g, r, id)) continue;
      seeds.push(r.id);
      heads.push(r.id, depot || !r.overseas ? 0 : (DESERT.ports[r.name] ?? DESERT.port) + sunk);
    }
    // Its picture: the one kept, if nothing it was drawn from moved (the provinces its roads may pass are laid out only
    // to draw it again, or when its friends have changed).
    const kept = book.drawn.get(id);
    if (kept && sameBytes(kept.seeds, seeds) && sameBytes(kept.heads, heads) && railHolds(kept.rail) && roadsHold(kept, friend, road)) {
      if (supplyCheck) {
        const pass = maskOf(friend, road);
        if (!sameBytes(pass, kept.mask)) throw new Error(`roads kept for ${id} in month ${g.turn} differ from fresh ones`);
        const fresh = drawSupply(g, id, pass, seeds, heads, wide, long, deep, sand, rail);
        if (!sameBytes(fresh.ok, kept.ok) || !sameBytes(fresh.reach, kept.reach)) throw new Error(`supply picture kept for ${id} in month ${g.turn} differs from a fresh one`);
      }
      g.supply[id] = kept.ok;
      g.reach[id] = kept.reach;
      continue;
    }
    const pass = maskOf(friend, road);
    const drawn = drawSupply(g, id, pass, seeds, heads, wide, long, deep, sand, rail);
    g.supply[id] = drawn.ok;
    g.reach[id] = drawn.reach;
    book.drawn.set(id, { mask: pass, friend, at: logEnd, seeds, heads, ...drawn, rail: drawn.east ? rail : null });
  }
  // An army at peace is fed by its own country wherever it stands.
  for (const c of g.corps) c.supplied = !g.nations[c.nat].atWar.length || !!g.supply[c.nat]?.[c.reg];
  // A garrison shut in its town lives on the town's stores while they last.
  if (field) siegeSupply(g);
  g.supplyDirty = false;
  resetRailheads(g);
}

/** How much longer the desert supply line has grown for a nation fed across the sea, from the cargoes sunk on the way last month. */
export function convoyDrag(g: Game, id: string): number {
  if (!DESERT.route.feeds.includes(id)) return 0;
  const m = g.convoys[DESERT.route.id];
  return m && g.turn - m.turn <= 1 ? DESERT.route.perTenth * 10 * (m.loss ?? 0) : 0;
}

/**
 * The length of the supply line in the desert and along the overseas coast: counted from the
 * nearest working base, a step for every region of desert on the way. Written into the same
 * table as the line inside the Soviet Union; the two never overlap.
 */
function desertLines(g: Game, ok: Uint8Array, head: Float32Array, sand: Uint8Array, out: Float32Array): void {
  const d = new Float32Array(g.regions.length).fill(1e6);
  const q: number[] = [];
  for (let i = 0; i < head.length; i++) if (head[i] >= 0) { d[i] = head[i]; q.push(i); }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    for (const j of g.regions[a].nb) {
      if (!ok[j]) continue;
      const far = d[a] + (sand[j] & 2 ? DESERT.step : 0);
      if (far < d[j] - 1e-6) { d[j] = far; q.push(j); }
    }
  }
  for (let i = 0; i < d.length; i++) if (sand[i] && ok[i] && d[i] < 1e6) out[i] = d[i];
}

/**
 * The length of a nation's supply line in every region it can reach inside the Soviet Union,
 * counted from the frontier railheads. Regions held long enough to relay the track count for less.
 * (Ruling 221, L: on the ground of the other owners of the long line it is counted from the harbours the invader
 * holds there as well, where its ships bring the stores in.)
 */
function reachInto(g: Game, id: string, ok: Uint8Array, long: string[], deep: Uint8Array, rail: RailNow): Float32Array {
  const d = new Float32Array(g.regions.length);
  if (long.includes(id)) return d; // at home on their own railways
  // Off Soviet ground (ruling 221, L), a harbour the invader holds on the long line's ground is a railhead too.
  const quay = (j: number): boolean => ok[j] === 2 && !rail.home[j];
  d.fill(1e6);
  const q: number[] = [];
  for (let j = 0; j < d.length; j++) if (ok[j] && (!deep[j] || quay(j))) { d[j] = 0; q.push(j); }
  for (let i = 0; i < q.length; i++) {
    const a = q[i];
    for (const j of g.regions[a].nb) {
      if (!ok[j] || !deep[j]) continue;
      // The step's cost (computeSupply): in the grand campaign a line the defenders tore up is not relaid by time
      // alone, engineers must do it; and a line burned under a scorched earth order is converted only from the month
      // it was made good.
      const step = rail.step[j];
      if (d[a] + step < d[j] - 1e-6) { d[j] = d[a] + step; q.push(j); }
    }
  }
  for (let i = 0; i < d.length; i++) if (d[i] >= 1e6) d[i] = 0;
  return d;
}

/**
 * A place's works set aside for a fight being resolved (the book's record fights: stations.ts, rider b580). The
 * set-aside holds for the fight only, never for supply or stores, at every place and stop (rider b628 to ruling 234):
 * what feeds a garrison reads the works through `worksKept`, so a supply picture drawn in the middle of the fight
 * still finds the stores of a fortified port behind them.
 */
let asideAt = -1, asideWorks: Region['fort'] = null;
export function withWorksAside<T>(r: Region, f: () => T): T {
  const fort = r.fort, wasAt = asideAt, wasWorks = asideWorks;
  if (fort) { asideAt = r.id; asideWorks = fort; }
  r.fort = null;
  try { return f(); } finally { r.fort = fort; asideAt = wasAt; asideWorks = wasWorks; }
}
/** A place's works as its stores and its supply know them: its own, set aside for a fight or not. */
export const worksKept = (r: Region): Region['fort'] => r.fort ?? (r.id === asideAt ? asideWorks : null);

export function ensureSupply(g: Game): void {
  if (g.supplyDirty) computeSupply(g);
}

// ---- Battles ------------------------------------------------------------------------

/** How many corps can attack a region from each side: there is only so much road and front. */
export function frontage(r: Region): number {
  return r.terrain === 'plains' ? 4 : r.terrain === 'mountain' ? 2 : 3;
}

/**
 * A fighting corps: infantry, cavalry, mountain troops, motorized troops or tanks (the probe's own line). Guns,
 * engineers and fortress troops are not. In the grand campaign a force with no fighting corps opens no attack,
 * storms no town, keeps up no siege and marches onto no enemy-held ground, computer and player alike (ruling 211(a),
 * which brings ruling 146's second fix forward and widens it: in Brian's own campaign one Canadian artillery corps
 * stormed Buffalo and Pittsburgh and took Washington by itself).
 */
/**
 * A corps' name with its country's word in front, as the battle card, the ticket and the battle report give every
 * corps on them (the UI director's words for b357, item 8): "Canadian XII Engineer Corps" against "American XII
 * Engineer Corps", always, so the form never flickers. A name that already says a country before any bracket
 * ("1st Canadian Division", "French Army B: ...") is left as it is. The army tray keeps the player's own names bare.
 */
export function corpsNamed(g: Game, nat: string, name: string): string {
  const adj = g.nations[nat]?.adj;
  if (!adj) return name;
  const head = name.split('(')[0];
  for (const n of Object.values(g.nations)) if (n.adj && new RegExp(`(^|[^A-Za-z])${n.adj}([^A-Za-z]|$)`).test(head)) return name;
  return `${adj} ${name}`;
}
/**
 * Ground of a country of the Americas that joined the Axis by its courting (ruling 223(c), diplomacy.ts): it keeps
 * its own small army, and no computer-run country of its side ships a corps to its harbours or bases an air group on
 * its fields. Its enemies are not barred from what they take of it.
 */
export const thornGround = (g: Game, r: Region, nat: string): boolean => nat !== r.owner && nat !== g.human && g.flags[`americas:in:${r.owner}`] !== undefined && allied(g, nat, r.owner);

// The support teams (release 2, teams.ts) are no fighting corps either: they go in beside one.
export const fights = (c: Corps): boolean => c.kind !== 'art' && c.kind !== 'eng' && c.kind !== 'fort' && !isTeam(c);
/** The sentence wherever guns are sent forward alone (ruling 146). */
export const GUNS_ALONE = 'Guns cannot take ground alone. Send infantry or tanks in with them.';
/** The same for a force of engineers with no guns among them. */
export const ENGINEERS_ALONE = 'Engineers cannot take ground alone. Send infantry or tanks in with them.';
/** And for one of guns and engineers together. */
export const GUNS_ENGINEERS_ALONE = 'Guns and engineers cannot take ground alone. Send infantry or tanks in with them.';
/** And for one of support teams alone (release 2). */
export const TEAMS_ALONE = 'Machine-gun and mortar brigades cannot take ground alone. Send infantry or tanks in with them.';
/** The sentence for a force with no fighting corps in it. */
export function aloneLine(corps: Corps[]): string {
  const guns = corps.some((c) => c.kind === 'art'), sappers = corps.some((c) => c.kind === 'eng');
  if (!guns && !sappers && corps.some(isTeam)) return TEAMS_ALONE;
  return guns && sappers ? GUNS_ENGINEERS_ALONE : sappers ? ENGINEERS_ALONE : GUNS_ALONE;
}
/** Where the rule that guns never go forward alone holds: the grand campaign, which alone raises guns and engineers. */
export const gunsRule = (g: Game): boolean => g.flags.open === 1;
/** True if a corps that is not a fighting corps may come onto this enemy-held ground: a fighting corps of its side stands in its field already. */
export const gunsMayFollow = (g: Game, c: Corps, to: Region): boolean => to.units.some((u) => !u.dead && fights(u) && allied(g, u.nat, c.nat));

/** Trims an attacking force to what the roads into the target can carry, strongest first. */
export function limitAttackers(g: Game, corps: Corps[], target: Region): Corps[] {
  let cap = frontage(target);
  // A staff that cannot yet run a big battle brings fewer corps into it, however many stand ready.
  const most = corps.length ? g.nations[corps[0].nat].maxAttack || 99 : 99;
  const ranked = [...corps].sort((x, y) => corpsBase(g, y) * KIND[y.kind].atk - corpsBase(g, x) * KIND[x.kind].atk);
  const guns: Corps[] = [];
  // Support teams (release 2) fire from behind the line: a few ride in beside the corps that fight, as guns do.
  const teams: Corps[] = [];
  if (g.flags.open === 1 && corps.length) {
    // Attacked from several sides at once a defence cannot cover every road: each side's roads carry more (the grand campaign).
    if (new Set(corps.filter((c) => !blocked(g, c, target)).map((c) => c.reg)).size >= DEFENCE.frontage.sides) cap += DEFENCE.frontage.extra;
    // Guns fire over the heads of the infantry: against works, a few artillery corps join without taking room on the roads.
    if (worksAgainst(g, target, armyOf(corps[0])) || hostileUnits(g, target, armyOf(corps[0])).some((u) => u.dug)) {
      for (const c of ranked) if (c.kind === 'art' && guns.length < DEFENCE.frontage.guns && !blocked(g, c, target)) guns.push(c);
    }
    for (const c of ranked) if (isTeam(c) && teams.length < TEAMS.frontage && !blocked(g, c, target)) teams.push(c);
  }
  const fill = (over: Corps[]): Corps[] => {
    const used: Record<number, number> = {};
    const out: Corps[] = [];
    for (const c of ranked) {
      if (out.length >= most) break;
      if (over.includes(c)) continue;
      if (blocked(g, c, target)) continue; // tanks have no way up the steepest ground, and nothing on wheels crosses a river whose bridges are down
      if ((used[c.reg] ?? 0) >= cap) continue;
      used[c.reg] = (used[c.reg] ?? 0) + 1;
      out.push(c);
    }
    return out;
  };
  const behind = teams.length ? [...guns, ...teams] : guns;
  const out = fill(behind);
  // The guns support somebody: with nobody else to go in, they take the roads themselves like any other corps.
  // (The teams ride only beside a fighting corps.)
  const force = out.length ? (teams.length && out.some(fights) ? [...out, ...behind] : [...out, ...guns]) : fill([]);
  if (!gunsRule(g) || force.some(fights)) return force;
  // In the grand campaign guns never go forward alone (ruling 211(a)): where the roads filled up with guns and
  // engineers, the fighting corps to hand take them instead, and with no fighting corps there is no attack.
  const line = fill(corps.filter((c) => !fights(c)));
  return line.length ? (teams.length ? [...line, ...guns, ...teams] : [...line, ...guns]) : [];
}

/** Which week of the month a corps reaches after spending this many more action points. */
export function weekOf(c: Corps, cost: number): number {
  return clamp(Math.ceil(((c.apMax - c.ap + cost) / Math.max(1, c.apMax)) * 4), 1, 4);
}

/**
 * What a corps' army is worth man for man: the figure of the country it answers to, or by the book that of the army
 * that raised it, whoever's flag it stands under (history.ts raisedQuality; rider b626 to ruling 234).
 */
const armyQuality = (g: Game, c: Corps): number => raisedQuality(g, c) ?? g.nations[c.nat].quality;

export function corpsBase(g: Game, c: Corps): number {
  // Tanks and lorries, or rifles and guns: a corps is as good as the designs it has been issued.
  const kit = techMul(g, c.nat, c.kind === 'arm' || c.kind === 'mot' ? 'armour' : 'infantry');
  return c.divs * c.str * Math.sqrt(0.15 + 0.85 * c.org) * armyQuality(g, c) * (0.85 + 0.4 * c.xp) * (c.supplied || fortRow(g, c) ? 1 : cmdCutOff(g, c)) * kit;
}

/** Share of the usual losses a defender takes when the attack is beaten off. */
export const HELD_LOSS = 0.6;
/** The share of its strength an attack loses at the ratio R, before who pays and the works (the battle's bill). */
export const attackLoss = (R: number): number => clamp(0.14 / R, 0.02, 0.2);
/** The share of its strength a defence loses at the ratio R, before the push of tanks, the desert and holding its ground. */
export const defenceLoss = (R: number): number => clamp(0.07 + 0.1 * R, 0.06, 0.45);
/** How much stronger troops defend from trenches they have had a month to dig. */
export const DUG_IN = 1.25;
/** Prepared defences (the grand campaign): who digs in, and how the day's luck runs against an attack on a line that is ready for it. */
export const DEFENCE = defenceData as unknown as {
  entrench: { auto: boolean; months: number; share: number[] };
  handover?: { fade: number };
  /** Probing the line: see the note in the data. */
  probe?: { ap: number; wear: number; weight?: number; loss: number; org: number };
  wear: { perAttack: number; fullAt: number; guns: number; regrow: number };
  tired: { per: number; most: number };
  hammer: { first: number; second: number };
  frontage: { sides: number; extra: number; guns: number };
  prepared: { dice: [number, number]; spentAt: number; surprise: number; surpriseMonths: number; openingMonths: number; armour: number; losses: { attacker: number; defender: number } };
  staff: { odds: number };
};
/** The run of the day's luck in the open: it takes 1.2 to 1 on the day to carry the ground. */
export const FIELD_DICE: [number, number] = [0.8, 1.2];
/** The chance that an attack at these odds carries the ground, with the day's luck running between the two numbers. */
export const chanceOn = (odds: number, dice: [number, number]): number => Math.max(0, Math.min(1, (dice[1] - 1.2 / Math.max(1e-6, odds)) / (dice[1] - dice[0])));
const listAnd = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
/** The odds at which an attack is an even chance. */
export const evenOdds = (dice: [number, number]): number => 2.4 / (dice[0] + dice[1]);
/** The honest win chance on land (release 2, auto-resolve fix 1; data/campaign/winchance.json): the record's luck in the field. */
const WIN = winData as unknown as { field: [number, number]; prepared: number };
const PURSUIT = pursuitData as unknown as { share: number };
/**
 * True in a grand campaign opened with the honest win chance on land: its own step inside Units decide (the design
 * director, 23 Nov 2026 10:14Z), so it needs both flags, and the other steps can go on without it.
 */
export const winChanceOn = (g: Game): boolean => unitsDecide(g) && g.flags.winChance === 1;
/**
 * The run of the day's luck an attack in the field is decided by, with the honest win chance on: the record's, fitted
 * over combat power, so that the card's odds (which count training, doctrine, supply and tech) carry the ground as often
 * as like odds did in the record. A line dug in and fed shrinks the whole run by the prepared share.
 */
/** What the luck's low end says of sure odds, in words: with the record's luck no odds make an attack sure. */
export const sureWords = (dice: [number, number]): string => (dice[0] > 0 ? `nothing is sure under ${(1.2 / dice[0]).toFixed(1)} to 1` : 'no odds make it sure');
export const luckOf = (prepared: number): [number, number] => {
  const f = 1 - WIN.prepared * Math.max(0, Math.min(1, prepared));
  return [WIN.field[0] * f, WIN.field[1] * f];
};
/** True on open desert with no fort (ruling 165): rock under a thin layer of sand, so fieldworks there come a month late. */
export const digsLate = (r: Region): boolean => r.terrain === DESERT.terrain && !r.fort;
/** Months of works a dug-in corps has to its name, up to the months they take to finish (the grand campaign). Attacks wear them down. */
export const worksOf = (c: Corps): number => (c.dug ? Math.max(0, Math.min(DEFENCE.entrench.months, c.dugFor ?? 1)) : 0);
/** What works that many months old are worth, as a share of finished ones: the first month's digging does the most. */
export function worksShare(months: number): number {
  const s = DEFENCE.entrench.share;
  if (months <= 0) return 0;
  const at = (k: number): number => (k <= 0 ? 0 : s[Math.min(k, s.length) - 1]);
  const i = Math.floor(months);
  return at(i) + (at(i + 1) - at(i)) * (months - i);
}
/**
 * True while a new offensive still has the defenders off balance (the grand campaign): an army trained to it
 * (Germany's) takes most of the worth out of a prepared line for the first month of each new offensive, and for
 * longer in the first one it ever makes on a country. A blow after three quiet months is a new offensive.
 */
function surprised(g: Game, nat: string, foes: Iterable<string>): boolean {
  const n = g.nations[nat];
  if (!n?.blitz) return false;
  for (const d of foes) {
    const last = n.lastFight[d], start = n.offStart[d];
    // The first blow ever struck at them, or the first after a lull: the offensive begins with this battle.
    if (last === undefined || g.turn - last >= 3) { if (atWar(g, nat, d)) return true; continue; }
    if (start === undefined) continue;
    const months = g.flags[`opening:${nat}:${d}`] === start ? DEFENCE.prepared.openingMonths : DEFENCE.prepared.surpriseMonths;
    if (g.turn - start <= months - 1) return true;
  }
  return false;
}
/** What artillery does in a land battle (the grand campaign's arsenal). */
export const GUNS = (arsenalData as unknown as { guns: { soften: number; softenMost: number; thicken: number; thickenMost: number } }).guns;
/** The weight of guns in a force: its artillery divisions, at the strength they stand at. */
const gunsIn = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);

export const men = (c: Corps): number => Math.round(c.divs * c.str * MEN_PER_DIV);

/** What a month cut off from its supplies costs a corps in order: it lays down its arms when none is left. */
const UNFED = 0.4;
/** How many months a corps in this order holds out on what it carries once it is cut off (a fresh one: three). */
export const monthsUnfed = (org = 1): number => Math.max(1, Math.ceil(org / UNFED));

function airOf(g: Game, nats: Set<string>, enemies: Set<string>): number {
  let s = 0;
  // An air force short of fuel flies fewer sorties (the grand campaign).
  // (A country whose air force the fight back raised as air groups fights with them, and its rating no longer counts: air.ts ratedAir.)
  for (const id of nats) s += ratedAir(g, id) * fuelAir(g, id);
  for (const n of Object.values(g.nations)) {
    // An ally at war with the same enemy lends a share of its air force, if it has airfields on the map to fly from.
    if (!n.alive || nats.has(n.id) || n.capital < 0) continue;
    if ([...nats].some((a) => allied(g, a, n.id)) && [...enemies].some((e) => atWar(g, n.id, e))) s += ratedAir(g, n.id) * 0.25;
  }
  return s;
}

export interface Assessment {
  A: number;
  D: number;
  odds: number;
  lines: TicketLine[];
  defenders: Corps[];
  /** The run of the day's luck: 0.8 to 1.2 in the open, wider and against the attacker in the streets of a town or on a prepared defence. */
  dice: [number, number];
  /** With the honest win chance (release 2): today's run of luck, read at the same share of the way as the day's throw, for what the day costs. Absent, the dice are today's. */
  luck?: [number, number];
  /** The share of the defence that is dug in and in supply, less what the attacker's guns have broken up (the grand campaign): 0 to 1. */
  prepared: number;
  /** How old the works of the dug-in defenders are, in months, up to the months they take to finish (the grand campaign; 0 where nobody is dug in). */
  works: number;
  /** The months this attack takes off those works whether it carries the ground or not (the grand campaign). */
  wear: number;
  /**
   * The weight of the blow (the grand campaign): 1 at wear.fullAt to 1 or better on the ticket, in proportion
   * below that. The defenders' losses, the wear on their works and how much the fight tires them all go by it.
   */
  weight: number;
  /** The attacks the defenders have already stood against this month, by weight and averaged over the defence (the grand campaign). */
  tired: number;
  /** True where the defenders are guns with no fighting corps in front of them, caught in the field (ruling 146, the grand campaign): they are overrun, and the odds say so. */
  overrun?: boolean;
  /**
   * The town's guard fighting beside the field army, in divisions' worth (ruling 232, the grand campaign): where no
   * garrison stands in the town, the guard is in this battle behind its walls, and the town falls with the field.
   */
  guard?: number;
  /**
   * What the attack faces (the grand campaign; null everywhere else): the defence's anti-tank strength and what it is
   * made of, who pays (what each arm in the attack loses, as a multiple of the usual share), and the facts the battle
   * card tells before the attack and after.
   */
  faced: Faced | null;
  /**
   * Close air support (the grand campaign, air.json 'close'): the air edge after it, and each side's on-call air, its
   * share of that side's strength, its air strikes and the groups that fly them. Unset where nobody is on call.
   */
  air?: CloseAir;
}

/** How an attack arrives: over land, or (sea) from landing craft at that share of its strength. */
export interface AttackOpts {
  sea?: number;
  surprise?: boolean;
  /** The landing comes from the air, not from ships. */
  air?: boolean;
  /** The most corps that follow up into the ground won. The rest stay where they attacked from. */
  occupy?: number;
  /** The fight is for the town itself (the grand campaign): the defenders are its garrison and its guard, behind its walls. */
  town?: boolean;
  /** The garrison of a besieged town comes out against the army round it (the grand campaign): the attackers are the corps inside. */
  sally?: boolean;
  /**
   * The attack is reckoned as it would stand after an earlier blow this month that has not been struck yet (the
   * grand campaign): the defenders that much more tired, their works worn by that many months. A staff planning two waves reads it.
   */
  after?: { blows: number; wear: number };
  /**
   * Close air support flown over a battle fought on the field (the land field's bill): the air strikes each group
   * flew, by group id. Unset, each group flies the strikes the battle card gave its side.
   */
  closeSorties?: Record<number, number>;
  /**
   * The battle was fought out on the field by the player (src/battle): the ratio it came to stands in for the day's
   * luck, and the rules apply their own losses, retreats and prisoners from it. Absent, the dice roll as always.
   */
  fought?: number;
  /** With 'fought': what the field cost each corps and each side, so the bill speaks as the field did (ruling 226(6)). */
  field?: FieldBill;
  /** Units decide (#7 4(b), 12:48Z): a beaten army breaking out through the enemy on its road out. No pursuit follows it. */
  breakout?: boolean;
}

/**
 * What a battle fought on the field cost, for the campaign's bill (ruling 226(6)). 'corps': the share of its strength
 * each corps lost on the field, by corps id: each side's bill falls on its corps as the field had it. 'att' and 'def':
 * the share each side lost on the field. 'whole': the battle was fought to its end; handed to the staff, only the part
 * before is the field's, and the bill for the whole day is never less than it. 'broke': the side whose army broke on
 * the field (routed or driven from it). Where that side lost the day, the rules settle the pursuit (b464): it pays
 * what the rules charge for the same result at the staff's roll, the odds SEND THEM IN draws (b476(2)), or the field's
 * loss where that is larger.
 * 'pulled': the side that withdrew its army after the armies met, and how near breaking it stood (its bar, 0 to 1).
 * Where that side lost the day it pays the rules' bill for that defeat (at the staff's roll) times the bar squared, or
 * its field loss where that is larger (b474): an army that pulls out early gets away for little more than it had lost; one on the point of
 * breaking could not get away at all, and pays the whole.
 */
export interface FieldBill {
  corps: Record<number, number>; att: number; def: number; whole: boolean; broke?: 'att' | 'def'; pulled?: { side: 'att' | 'def'; bar: number };
  /** Close air support flown on the field: the strikes each group flew, by group id, booked as AttackOpts.closeSorties. */
  air?: Record<number, number>;
}

/**
 * How far a side's bill may stand from what the field cost it: the after card's headline and its field line are one
 * measure and must agree within a fifth (the design director, after the UI director's slice 3 review). Held a little
 * inside the fifth, so the whole numbers the card prints agree too.
 */
export const FIELD_HOLD = 1.15;

/** Scales a side's planned loss shares so its whole bill stands within FIELD_HOLD of the field's (1 with no field). */
function fieldScale(cs: Corps[], planned: number[], share: number | undefined, whole: boolean): number {
  if (share === undefined) return 1;
  const m = cs.map((c) => men(c)), tot = m.reduce((a, b) => a + b, 0);
  const now = tot > 0 ? cs.reduce((a, _c, i) => a + m[i] * planned[i], 0) / tot : 0;
  if (now <= 1e-9) return 1;
  const want = Math.max(share / FIELD_HOLD, whole ? Math.min(now, share * FIELD_HOLD) : now);
  return want / now;
}

/**
 * The scale for a side whose army broke on the field and lost the day (b464): the field settles the clash, the rules the
 * pursuit. Its whole bill is the rules' own as SEND THEM IN charges it, at the staff's roll ('plain': each corps' share
 * with no field split, b476(2)), or the field's ('share') where that is larger. It still falls on its corps as the field had it ('part': each
 * corps' share with the split, before the scale), each held to 'cap' and then times 'mul': where the corps the field hit
 * hardest reaches its cap, the rest of the bill falls on the others, so the side's whole bill is still the rules'.
 */
function pursuitScale(cs: Corps[], part: number[], plain: number[], cap: number, mul: number, share: number, of = 1): number {
  const m = cs.map((c) => men(c)), tot = m.reduce((a, b) => a + b, 0);
  if (tot <= 0 || mul <= 0) return 1;
  // 'of': the part of the rules' bill it pays; all of it for a pursuit, the bar squared for a withdrawal (b474).
  const want = Math.max(share / mul, of * (cs.reduce((a, _c, i) => a + m[i] * Math.min(cap, plain[i]), 0) / tot)) * tot;
  // The bill rises with the scale until each corps reaches its cap: a straight line between caps, so a few steps find it.
  let k = 1;
  for (let n = 0; n <= cs.length + 1; n++) {
    let held = 0, free = 0;
    cs.forEach((_c, i) => { if (k * part[i] >= cap) held += m[i] * cap; else free += m[i] * part[i]; });
    if (free <= 1e-12) break;
    const next = Math.max(0, (want - held) / free);
    if (Math.abs(next - k) < 1e-9) break;
    k = next;
  }
  return k;
}

/**
 * Each corps' part of its side's bill as the field had it: its field loss over the side's, weighted by men, so the
 * side's total is unchanged. 1 for every corps where the field gave nothing to go on.
 */
function splitWeights(cs: Corps[], split: Record<number, number> | undefined): number[] {
  if (!split) return cs.map(() => 1);
  const m = cs.map((c) => men(c)), tot = m.reduce((a, b) => a + b, 0);
  const mean = tot > 0 ? cs.reduce((a, c, i) => a + m[i] * (split[c.id] ?? 0), 0) / tot : 0;
  return cs.map((c) => (mean > 1e-6 && split[c.id] !== undefined ? Math.min(3, split[c.id] / mean) : 1));
}

/** Works out both sides' strength for an attack. Auto-resolve and the odds preview share it. */
export function assess(g: Game, attackers: Corps[], target: Region, opts: AttackOpts = {}): Assessment {
  ensureSupply(g);
  // A joint army attacks under the flag of the nation that commands it.
  const attNat = armyOf(attackers[0]);
  // An assault on a town (the grand campaign) is fought against its garrison, not against an army in the field.
  const town = !!opts.town;
  const defenders = town ? [...garrisonOf(target)] : hostileUnits(g, target, attNat);
  const lines: TicketLine[] = [];
  const aN = new Set(attackers.map((c) => c.nat)), dN = new Set(defenders.map((c) => c.nat));
  if (town && !dN.size) dN.add(target.ctrl);
  // (Ruling 232 rider b624: a place's works count only where they stand with the side that dug them.)
  const served = !!target.fort && (dN.size ? [...dN].some((d) => worksFor(g, target, d)) : worksFor(g, target, target.ctrl));
  const defHasArmor = defenders.some((d) => d.kind === 'arm');
  // Open desert has no flanks: infantry on foot outside prepared works can be driven round by anything on wheels or tracks.
  const openDesert = !town && target.terrain === DESERT.terrain && !target.fort && attackers.some((c) => KIND[c.kind].mobile);
  let turned = false, turnedTo = 1, fewGuns = false;
  const tempo = [...dN].some((d) => freshOffensive(g, attNat, d)) || freshOffensive(g, attNat, target.ctrl);
  // (pAll and pFaced: the attack's weight, and the part of it the fortress works face, for the town guard's walls.)
  let pAll = 0, pFaced = 0;
  // The fortress rows (06:15Z (a)): the share of the defence whose works stand all round, against those the works do not face too.
  const allRound = served && !town && !opts.sally && fortRows(g) ? fortShare(g, defenders) : 0;
  let roundFaced = 0;
  let A = 0, fortFaced = 0, armorShock = false, armorStuck = false, armorAlone = 0, aloneOver = 1, longest = 0, lineMul = 1, cold = false, coldDef = false, dugIn = 0;
  const sources = new Set<number>();
  for (const c of attackers) sources.add(c.reg);
  // Defenders with nowhere to fall back to: every way out is enemy ground or the attack itself.
  const pocket = !town && defenders.length > 0 && !target.nb.some((j) => !sources.has(j) && allied(g, defenders[0].nat, g.regions[j].ctrl) && !hostileUnits(g, g.regions[j], defenders[0].nat).length);
  let cmdA = 0, cmdD = 0, rawA = 0, rawD = 0, trap = false, proudA = 1, proudD = 1;
  // Massed guns: in the attack they break up works and trenches, in the defence they thicken the line they stand in.
  // Only the grand campaign raises artillery, so a battle without any is worked out exactly as it always was.
  const gunsA = gunsIn(attackers), gunsD = gunsIn(defenders);
  const soften = gunsA ? Math.min(GUNS.softenMost, GUNS.soften * gunsA) : 0;
  const thicken = gunsD ? Math.min(GUNS.thickenMost, GUNS.thicken * gunsD) : 0;
  // Support teams (release 2, teams.ts): the defence's machine-gun brigades against men on foot, the attack's mortar
  // brigades against fieldworks. None exist unless the teams are switched on, so both are 0 and nothing below moves.
  const mgD = teamsIn(defenders, 'mg'), morA = teamsIn(attackers, 'mor');
  const mgUp = mgD ? Math.min(TEAMS.card.mg.most, TEAMS.card.mg.perDiv * mgD) : 0;
  const morUp = morA ? Math.min(TEAMS.card.mor.most, TEAMS.card.mor.perDiv * morA) : 0;
  // The grand campaign's ground rules: each arm on the ground that suits it or does not, and tanks and infantry
  // worth more together than apart. A historical scenario has none of this and is worked out as it always was.
  const wide = groundRules(g);
  const land = opts.sea === undefined;
  // Tanks that cannot get over a river whose bridges are down are no help to the infantry that does.
  const armsA = wide ? combinedArms(g, land ? attackers.filter((c) => !noCrossing(g, c.kind, c.reg, target.id)) : attackers, target, false) : null, armsD = wide && !town ? combinedArms(g, defenders, target, true) : null;
  let tooSteep = 0, noBridge = 0, overBridges = 0, inBoats = 0, sneaked = false, giftA = 1, giftD = 1, shortA = 1, shortD = 1, spied = '', streets = false;
  // (Brian, 22 November, release 2: 'Busier, mostly watchers'.) An enemy watcher on the player's ground that mapped its defences.
  let watched: { nat: string; by: string; found: boolean; mul: number } | null = null;
  // Armies that have outrun their railhead: how far the worst of them is from it, and what it is worth there.
  let roadA = 0, railA = 1, roadD = 0, railD = 1;
  const rivers = new Set<string>();
  // Engineers in the assault breach the works, clear lanes through the mines and put the infantry over a bridgeless river.
  const sap = wide ? sappers(attackers.filter((c) => !land || !blocked(g, c, target))) : 0;
  const breach = sap ? Math.min(ENGINEERING.breach.most, ENGINEERING.breach.perDiv * sap) : 0;
  const mineLv = wide ? minesAt(g, target) : 0;
  const mined = mineLv ? 1 - ENGINEERING.mines.attack * mineLv * (1 - Math.min(1, sap)) : 1;
  // A staff weighing a second wave reckons the works as the first will have left them.
  const wornBy = wide && !town ? opts.after?.wear ?? 0 : 0;
  const worksNow = (d: Corps): number => Math.max(0, worksOf(d) - wornBy);
  // What the attack faces (the grand campaign, in the field): the defence's anti-tank strength takes that share off
  // the tanks' edge, its machine guns tell against men on foot, and the attack's mortars work on its fieldworks.
  const facing: Facing | null = wide && !town ? {
    works: (d) => (d.dug && (d.supplied || fortRow(g, d)) ? worksShare(worksNow(d)) : 0),
    guns: gunsD, mines: mineLv, sappers: sap,
    fort: served && !opts.sally && attackers.some((c) => target.fort!.vs.includes(g.regions[c.reg].owner)) ? target.fort!.lv : 0,
    foot: armsA ? armsA.foot : 0, tanks: armsA ? armsA.tanks : 0,
  } : null;
  const at = facing ? antiTank(g, attackers, defenders, target, facing) : null;
  // A desert box is its guns (ruling 170, the grand campaign): works keep the men from being driven round only where
  // the defence's anti-tank strength against this attack is two thirds or more. (The mark is met at its printed value.)
  const boxed = !wide || (!!at && Math.round(at.strength * 1e6) / 1e6 >= DESERT.box);
  const onFoot = facing ? footShare(attackers) : 0;
  const mortar = facing ? (morUp ? 1 - (1 - mortars(g, attackers)) * (1 - morUp) : mortars(g, attackers)) : 0;
  // An assault the month the army comes up to the town goes in off the march, with nothing made ready.
  const rushed = town && g.flags[`siege:${target.id}`] === g.turn ? SIEGE.assault.hasty[townSize(g, target)] : 1;
  const hasty = rushed < 1;
  for (const c of attackers) {
    if (wide && barred(g, c, target)) { tooSteep++; continue; }
    if (wide && land && noCrossing(g, c.kind, c.reg, target.id)) { noBridge++; continue; }
    let p = corpsBase(g, c) * KIND[c.kind].atk;
    if (hasty) p *= rushed;
    if (wide) {
      const suits = groundAttack(g, c.kind, target);
      if (suits !== 1) p *= suits;
      if (armsA) p *= c.kind === 'arm' ? armsA.armour : isFoot(c) ? armsA.infantry : 1;
      const x = land ? riverBetween(g, c.reg, target.id) : null;
      if (x) {
        rivers.add(riverName(x));
        if (bridgeDown(g, x)) { p *= sap ? ENGINEERING.blown.withEngineers : ENGINEERING.blown.attack; inBoats++; }
        else { p *= ENGINEERING.river.attack; overBridges++; }
      }
      if (mined !== 1) p *= mined;
    }
    if (c.kind === 'arm') {
      // In the streets of a town a tank is a target: there is no room to manoeuvre.
      if (town) { p *= SIEGE.assault.armour; streets = true; }
      else if (target.terrain === 'mountain' || target.terrain === 'urban' || target.terrain === 'marsh') { p *= 0.6; armorStuck = true; }
      // Tanks against a defence that cannot stop them. In the grand campaign the edge shrinks by its anti-tank strength.
      else {
        if (at ? at.strength < 1 : !defHasArmor) { p *= 1 + 0.25 * (at ? 1 - at.strength : 1); armorShock = true; }
        // Tanks that come on without infantry beside them are picked off by the guns (the grand campaign).
        // Never twice (ruling 164(b)): where the old cut for tanks alone in close country is the larger, it stands and this adds nothing.
        if (at && at.alone > 0) {
          const old = armsA ? armsA.alone : 1;
          if (1 - at.alone < old) { p *= (1 - at.alone) / old; armorAlone = at.alone; aloneOver = old; }
        }
      }
    }
    const lead = cmdAttack(g, c, pocket);
    if (pocket && lead > cmdAttack(g, c, false)) trap = true;
    if (tempo && KIND[c.kind].mobile) p *= 1.2;
    if (facing) p *= assaultFire(g, c);
    // The month war is declared on a country at peace, its defenders are caught unready (the grand campaign).
    if (wide && sneakOn(g, c.nat, target.ctrl)) { p *= RELATIONS.sneak.attack; sneaked = true; }
    // What its nation does best, and what it is itself if it is one of the nation's special formations (the grand campaign).
    if (wide) { const gift = traitAttack(g, c, target, !land); if (gift !== 1) { p *= gift; giftA = Math.max(giftA, gift); } }
    // Tanks without fuel and troops without shells: a nation whose depots are empty fights weaker (the grand campaign).
    if (wide) { const short = shortAttack(g, c); if (short !== 1) { p *= short; shortA = Math.min(shortA, short); } }
    // An agent at work in the province has mapped its defences for the attackers (the grand campaign).
    if (wide) { const spy = intelAttack(g, c.nat, target); if (spy) { p *= spy.mul; if (spy.foe) { if (!watched || (watched.by !== spy.foe && c.nat === spy.foe)) watched = { nat: spy.foe, by: c.nat, found: !!spy.found, mul: spy.mul }; } else spied = spy.agent; } }
    // Troops in good heart hit harder, and shaken ones do not (the grand campaign: nobody else carries morale).
    p *= heart(c);
    if (g.nations[c.nat].slowCommand) p *= 0.85;
    p *= techMul(g, c.nat, 'doctrine');
    if (opts.sea !== undefined) p *= opts.sea;
    const line = lineStrength(g, c);
    if (line.mul < 1) { p *= line.mul; longest = Math.max(longest, line.length); lineMul = Math.min(lineMul, line.mul); }
    if (wide) {
      const head = railheadOf(g, c);
      if (head.attack < 1) { p *= head.attack; roadA = Math.max(roadA, head.road); railA = Math.min(railA, head.attack); }
    }
    if (frozen(g, c.nat, target)) { p *= 1 - (1 - EASTERN.winter.attack) * cmdWinter(g, c); cold = true; }
    const from = g.regions[c.reg];
    // What the guns and the engineers between them leave of the works. With neither, the works count in full, as they always did.
    const eased = breach ? 1 - (1 - soften) * (1 - breach) : soften;
    // (In an assault on the town itself the works ring it: they are counted with the town, for its defenders.)
    pAll += p;
    if (served && target.fort && !town && !opts.sally && target.fort.vs.includes(from.owner)) { pFaced += p; p /= eased ? 1 + (FORT_MUL[target.fort.lv] - 1) * (1 - eased) : FORT_MUL[target.fort.lv]; fortFaced++; }
    // (From behind, the fortress troops' share of the line still meets the attack from its works: 1 + (W - 1) x s.)
    else if (allRound > 0 && target.fort) { const W = eased ? 1 + (FORT_MUL[target.fort.lv] - 1) * (1 - eased) : FORT_MUL[target.fort.lv]; p /= 1 + (W - 1) * allRound; roundFaced++; }
    // The player's honoured corps, a corps fighting for the place it was raised in, and one that has its old enemy in front of it.
    const proud = storyAttack(g, c, target, defenders);
    if (proud !== 1) { p *= proud; proudA = Math.max(proudA, proud); }
    rawA += p;
    cmdA += p * lead;
    A += p * lead;
  }
  // A relieving army (the grand campaign): the garrison of the town it has come to free sallies out to meet it.
  const sally = town || opts.sally ? { power: 0, corps: 0 } : sortiePower(g, target, attNat);
  if (sally.power > 0) A += sally.power;
  const flank = Math.min(1.3, 1 + 0.1 * (sources.size - 1));
  A *= flank;
  // What the town is worth to those inside it, after the guns and the engineers of this assault have done their work.
  const walls = town ? townBonus(g, target, breach ? 1 - (1 - soften) * (1 - breach) : soften, served) : 1;
  // The fortress rows (the design director's 07:36Z, form (b) amended): in a storm of a town that holds a fortress crew,
  // the crew's own share fights from its works at the field's weight, FORT_MUL by level, eased as the town's walls are
  // by the bombardment's breach and by this assault's guns and engineers, in place of the town's walls. The rest of the
  // garrison keeps the walls; no share has both (b418).
  const crewWalls = town && served && target.fort && fortRows(g)
    ? 1 + (FORT_MUL[target.fort.lv] - 1) * (1 - breachOf(g, target)) * (1 - clamp(breach ? 1 - (1 - soften) * (1 - breach) : soften, 0, 1)) : 0;
  let crewP = 0, townP = 0;
  let D = 0, thin = 1, readyD = 0, dugD = 0, worksD = 0, tiredD = 0, tiredMost = 0;
  let coastOut = false;
  for (const d of defenders) {
    // (By the book the record's figure for a place holds its fortress troops: they man the coast works, and add
    // nothing to a field fight that comes round those works. Rider b579; stations.ts. None with the far side off.)
    if (d.bookCoast && !town && opts.sea === undefined && target.fort && !fortFaced) { coastOut = true; continue; }
    // In the streets the country round about counts for nothing: the town is the ground.
    const crew = town && crewWalls > 0 && fortRow(g, d);
    const behind = !town ? TERRAIN_DEF[target.terrain] : crew ? crewWalls : walls;
    if (town) { const b = corpsBase(g, d) * KIND[d.kind].def; townP += b; if (crew) crewP += b; }
    let p = corpsBase(g, d) * KIND[d.kind].def * behind;
    if (wide && !town) {
      const suits = groundDefence(g, d.kind, target);
      if (suits !== 1) p *= suits;
      if (armsD) p *= d.kind === 'arm' ? armsD.armour : isFoot(d, true) ? armsD.infantry : 1;
    }
    // Trenches worn under a month's worth by attack after attack give less cover (the grand campaign).
    if (d.dug) { const dug = wide ? 1 + (DUG_IN - 1) * Math.min(1, Math.max(0, (d.dugFor ?? 1) - wornBy)) : DUG_IN; const left = (1 - soften) * (1 - mortar); p *= left < 1 ? 1 + (dug - 1) * left : dug; dugIn++; }
    // Machine guns tell against men on foot, most from finished fieldworks (the grand campaign).
    if (facing) p *= machineGuns(g, d, onFoot, facing.works(d));
    // The machine-gun brigades stiffen the line they stand behind (release 2): its corps that fight, not themselves.
    if (facing && mgUp && fights(d)) p *= 1 + mgUp * onFoot * (SUPPORT.machineGuns.open + (1 - SUPPORT.machineGuns.open) * facing.works(d));
    // Men who have stood off an attack already this month meet the next one tired (the grand campaign).
    const blows = wide && !town ? Math.min(DEFENCE.tired.most, (d.blows ?? 0) + (opts.after?.blows ?? 0)) : 0;
    if (blows > 0) p *= 1 - DEFENCE.tired.per * blows;
    if (thicken && d.kind !== 'art') p *= 1 + thicken;
    if (frozen(g, d.nat, target)) { p *= 1 - (1 - EASTERN.winter.defence) * cmdWinter(g, d); coldDef = true; }
    // A dug-in box is not driven round (ruling 146, the Historian's third fix): the cut falls only on men on foot who
    // have not dug in, and in the grand campaign it fades as their works are finished, where the guns make it a box.
    // (A position driven round for want of guns is judged no help in strength, so its works do not turn the day's
    // luck against the attack either: they would count twice. The design director's b420, the historian's b418.)
    let unboxed = false;
    if (openDesert && !KIND[d.kind].mobile && d.kind !== 'fort') {
      const box = !d.dug ? 0 : !wide ? 1 : boxed ? worksShare(worksNow(d)) : 0;
      if (d.dug && !boxed) { fewGuns = true; unboxed = true; }
      if (box < 1) { const cut = DESERT.open + (1 - DESERT.open) * box; p *= cut; turned = true; turnedTo = Math.min(turnedTo, cut); }
    }
    const dl = lineStrength(g, d).mul;
    const share = inDesert(target) ? DESERT.lineDefence : EASTERN.lineDefence;
    if (dl < 1) { p *= 1 - (1 - dl) * share; thin = Math.min(thin, 1 - (1 - dl) * share); }
    if (wide) {
      const head = railheadOf(g, d);
      if (head.defence < 1) { p *= head.defence; roadD = Math.max(roadD, head.road); railD = Math.min(railD, head.defence); }
    }
    if (wide) { const gift = traitDefence(g, d, target); if (gift !== 1) { p *= gift; giftD = Math.max(giftD, gift); } }
    if (wide) { const short = shortDefence(g, d); if (short !== 1) { p *= short; shortD = Math.min(shortD, short); } }
    p *= heart(d);
    const proud = storyDefence(g, d, target, attackers);
    if (proud !== 1) { p *= proud; proudD = Math.max(proudD, proud); }
    const lead = cmdDefence(g, d);
    rawD += p;
    cmdD += p * lead;
    D += p * lead;
    // Works grow with the months spent on them: a line held since the spring is not one occupied last month.
    if (d.dug && (d.supplied || fortRow(g, d)) && !unboxed) readyD += p * lead * worksShare(worksNow(d));
    if (d.dug) { dugD += p * lead; worksD += p * lead * worksNow(d); }
    if (blows > 0) { tiredD += p * lead * blows; tiredMost = Math.max(tiredMost, blows); }
  }
  // A line that is dug in and fed (the grand campaign): the attacker's luck runs worse against it, by the share of the defence that is ready.
  // Massed guns break it up, and the first blows of a new offensive by an army trained to it catch it off balance.
  const caught = wide && !town && surprised(g, attNat, new Set([...dN, target.ctrl])) ? DEFENCE.prepared.surprise : 0;
  // And tanks break into a line that has no armour of its own to meet them.
  const pierced = armorShock ? DEFENCE.prepared.armour * (at ? 1 - at.strength : 1) : 0;
  const prepared = wide && !town && D > 0 ? Math.min(1, readyD / D) * (1 - soften) * (1 - mortar) * (1 - caught) * (1 - pierced) : 0;
  const today: [number, number] = town ? [SIEGE.assault.dice[0], SIEGE.assault.dice[1]]
    : prepared > 0 ? [FIELD_DICE[0] + (DEFENCE.prepared.dice[0] - FIELD_DICE[0]) * prepared, FIELD_DICE[1] + (DEFENCE.prepared.dice[1] - FIELD_DICE[1]) * prepared] : FIELD_DICE;
  // With the honest win chance (release 2), an attack in the field is decided by the record's wider luck; today's run
  // still sets what the day costs (resolveBattle). Towns keep today's luck, and so do guns caught alone (below).
  let dice: [number, number] = winChanceOn(g) && !town ? luckOf(prepared) : today;
  // The town's own guard stands with whoever is inside. Where nobody is inside, it fights beside the army in the field,
  // behind its walls, and the town falls with the field (ruling 232): the fortress works, where they already stand
  // against every attacker, are not counted for it twice.
  const guarded = !town && !opts.sally && defenders.length > 0 ? guardInField(g, target, attNat, attackers) : 0;
  const guardWalls = guarded > 0 ? fieldWalls(g, target, breach ? 1 - (1 - soften) * (1 - breach) : soften, pAll > 0 ? 1 - pFaced / pAll : 1) : 1;
  const watch = town ? guardPower(g, target) * walls : guarded > 0 ? guardPower(g, target) * guardWalls : 0;
  if (watch > 0) { rawD += watch; cmdD += watch; D += watch; }
  const aAir = airOf(g, aN, dN), dAir = airOf(g, dN, aN);
  let airEdge = (aAir - dAir) / (aAir + dAir + 1);
  // Whoever has won the sky over this ground in the air war has it in the land battle too. In the grand campaign's air
  // war it counts for whichever side holds it, and the whole air effect is fitted to the record (ruling 227, b433).
  const sky = skyOver(g, target.id), S = skyOnLand(g);
  // (And in the air war a side with overwhelming air over the battle holds its sky, unless it is fought for: b444.)
  const byNumbers = sky === null && S.both ? skyByNumbers(g, aN, dN, target.id) : null;
  const skyWon = byNumbers === 'att' || (sky !== null && [...aN].some((n) => allied(g, n, sky)));
  const skyLost = byNumbers === 'def' || (S.both && sky !== null && !skyWon && [...dN].some((n) => allied(g, n, sky)));
  if (skyWon) airEdge = Math.min(1, airEdge + S.land);
  if (skyLost) airEdge = Math.max(-1, airEdge - S.land);
  // Weather (ruling 127): a closed sky over the battle grounds both air forces, and cloud halves what they lend.
  const wxSky = weatherOn(g) ? skyAt(g, target.id) : 'clear';
  if (wxSky !== 'clear') airEdge *= skySupport(wxSky);
  // Close air support (Brian's ask of 2026-11-18): each side's fighter groups on ground support within reach of the
  // battle add to its side of the air edge. Only this term reaches past the sky's clamp at 1 (the design director's
  // b536): ground attack paid most where the sky was already won. Off, or with nobody on call, nothing here changes.
  let close: CloseAir | undefined;
  // (Who holds the sky is said from the air war alone: the groups on call over the battle do not change it.)
  const skyEdge = airEdge;
  if (closeRules(g)) {
    const att = closeOf(g, aN, dN, target.id, skyLost), def = closeOf(g, dN, aN, target.id, skyWon);
    if (att.groups.length || def.groups.length) {
      const cA = closeEdge(att.W), cD = closeEdge(def.W);
      const e = airEdge + cA - cD;
      // (Each side's share is what its own on-call air adds to its strength, against the edge without it.)
      att.share = cA > 0 ? Math.max(0, 1 - (1 + S.attack * (e - cA)) / (1 + S.attack * e)) : 0;
      def.share = cD > 0 ? Math.max(0, 1 - (1 - S.defence * (e + cD)) / (1 - S.defence * e)) : 0;
      att.strikes = closeStrikes(att);
      def.strikes = closeStrikes(def);
      airEdge = e;
      close = { edge: e, att, def };
    }
  }
  A *= 1 + S.attack * airEdge;
  D *= 1 - S.defence * airEdge;
  D = Math.max(D, 0.01);

  const sum = (cs: Corps[]) => cs.reduce((s, c) => s + men(c), 0).toLocaleString('en-US');
  if (town) {
    const broken = breachOf(g, target), place = townName(target);
    lines.push({ label: 'Forces', value: `${attackers.length} corps, ${sum(attackers)} men, against ${defenders.length ? `${defenders.length} corps, ${sum(defenders)} men` : watch > 0 ? 'no army' : '0 corps, 0 men'}${watch > 0 ? `${defenders.length ? ', and' : ':'} the town guard of about ${guardMen(g, target).toLocaleString('en-US')}` : ''}` });
    lines.push({
      // ("Panama City, Panama's capital", not "Capital of Panama City"; "Lodz, a city", not "City of Lodz": the UI director,
      // builds 1668 and 1678.)
      label: 'The town', value: `${townSize(g, target) === 'capital' ? `${place}, ${theWords(`${theName(g.nations[target.owner])}'s`)} capital` : `${place}, a ${TOWNS.words[townSize(g, target)].toLowerCase()}`}${target.fort ? `, with fortress works of level ${target.fort.lv}${served ? '' : `, dug by ${theName(g.nations[worksBy(target)])}: they give its defenders no cover`}` : ''}`,
      effect: `Its defenders fight ${Math.round((walls - 1) * 100)}% stronger in the streets${broken > 0 ? `: bombardment has already broken down ${Math.round(broken * 100)}% of its works` : ''}`,
    });
    // (Which share stands behind which: the design director's 07:36Z.)
    if (crewP > 0) {
      const crews = defenders.filter((d) => fortRow(g, d)).map((d) => d.name).join(', ');
      const rest = townP > crewP ? `; the other ${Math.round((1 - crewP / townP) * 100)}% of the garrison behind the town's walls, ${Math.round((walls - 1) * 100)}% stronger` : '';
      lines.push({ label: 'Fortress crew', value: `${crews}: ${Math.round((crewP / townP) * 100)}% of the garrison`, effect: `It fights from its own works of level ${target.fort!.lv}, ${Math.round((crewWalls - 1) * 100)}% stronger, in place of the town's walls${rest}` });
    }
  } else {
    lines.push({ label: 'Forces', value: `${attackers.length} corps, ${sum(attackers)} men, against ${defenders.length} corps, ${sum(defenders)} men${guarded > 0 ? `, and the town guard of about ${guardMen(g, target).toLocaleString('en-US')}` : ''}` });
    lines.push({ label: 'Terrain', value: target.terrain[0].toUpperCase() + target.terrain.slice(1), effect: TERRAIN_DEF[target.terrain] > 1 ? `Defenders fight ${Math.round((TERRAIN_DEF[target.terrain] - 1) * 100)}% stronger` : 'Open ground, no help to the defender' });
    // The grand campaign: what winning here does and does not win.
    if (opts.sally) lines.push({ label: 'The sortie', value: `The garrison of ${townName(target)} comes out against the army round it`, effect: 'Win, and the siege is raised. Lose, and the garrison goes back inside the weaker for it' });
    else if (sally.power > 0) lines.push({ label: 'The garrison', value: `The garrison of ${townName(target)} sallies out as the relief comes up: ${sally.corps} corps`, effect: `It adds ${Math.round(SIEGE.sortie * 100)}% of its strength to the attack` });
    else if (guarded > 0) lines.push({
      label: 'The town', value: `${townName(target)} is held by its own guard: ${guardWorth(guarded)}, behind its walls`,
      effect: `The guard fights beside the army, ${Math.round((guardWalls - 1) * 100)}% stronger behind the walls. Win, and the town falls with the country round it`,
    });
    else if (siegeRules(g) && !allied(g, attNat, target.ctrl) && townHeld(g, target)) lines.push({
      label: 'The town', value: garrisonOf(target).length ? `${townName(target)} is garrisoned` : `${townName(target)} is held by its own guard`,
      effect: driveOut(g) && garrisonOf(target).length ? 'Winning here wins the country round it. The garrison holds the town after: it must then be stormed or starved out'
        : 'Winning here wins the country round it. The town must then be stormed or starved out, unless it opens its gates',
    });
  }
  if (wide && defenders.length) {
    // Training and experience decide as many fights as numbers do: said in words, division for division.
    const grade = (cs: Corps[]): { worth: number; army: number; xp: number } => {
      let w = 0, worth = 0, army = 0, xp = 0;
      for (const c of cs) { const k = c.divs * c.str; w += k; worth += k * armyQuality(g, c) * (0.85 + 0.4 * c.xp); army += k * armyQuality(g, c); xp += k * c.xp; }
      return w > 0 ? { worth: worth / w, army: army / w, xp: xp / w } : { worth: 1, army: 1, xp: 0 };
    };
    const ours = grade(attackers), theirs = grade(defenders);
    const ratio = theirs.worth / Math.max(0.01, ours.worth);
    const word = (xp: number): string => (xp < 0.2 ? 'green' : xp < 0.45 ? 'trained' : xp < 0.7 ? 'seasoned' : 'veteran');
    if (ratio >= 1.1 || ratio <= 1 / 1.1) {
      const better = ratio > 1 ? 'defending' : 'attacking', worse = ratio > 1 ? 'attacking' : 'defending';
      const k = ratio > 1 ? ratio : 1 / ratio;
      const armyK = ratio > 1 ? theirs.army / ours.army : ours.army / theirs.army, xpK = k / armyK;
      const why = [armyK >= 1.04 ? 'the better trained and led army' : '', xpK >= 1.04 ? 'the more experienced troops' : ''].filter(Boolean);
      lines.push({
        label: 'Troops', value: `${word(ours.xp)[0].toUpperCase() + word(ours.xp).slice(1)} attackers against ${word(theirs.xp)} defenders`,
        effect: `Division for division the ${better} side is the stronger${why.length ? `, as ${listAnd(why)}` : ''}: each ${better} division counts for about ${k.toFixed(1)} ${worse} ones`,
      });
    }
  }
  if (wide) {
    const a = groundLine(g, attackers.filter((c) => !barred(g, c, target)), target, false), d = town ? '' : groundLine(g, defenders, target, true);
    const fx = [tooSteep ? `${tooSteep === 1 ? 'one armoured corps' : `${tooSteep} armoured corps`} cannot climb here and ${tooSteep === 1 ? 'takes' : 'take'} no part` : '', a, d].filter(Boolean).join('; ');
    if (fx) lines.push({ label: 'Ground', value: groundSays(g, target), effect: fx[0].toUpperCase() + fx.slice(1) });
    const pc = (m: number): number => Math.round(Math.abs(m - 1) * 100);
    if (armsA) {
      const lone = armsA.alone < 1, close = target.terrain === 'urban' ? 'city streets' : 'forest';
      const together = armsA.foot > 0;
      const tank = armsA.armour !== 1 ? `tank attack ${armsA.armour > 1 ? 'up' : 'down'} ${pc(armsA.armour)}%` : '';
      const foot = together ? `infantry attack up ${pc(armsA.infantry)}%` : '';
      const fx2 = [foot, tank].filter(Boolean).join(', ');
      lines.push({
        label: 'Combined arms',
        value: !together ? (lone ? `Tanks attacking into ${close} with no infantry beside them` : 'Tanks attacking without infantry')
          : armsA.cover < 1 ? 'Tanks and infantry attack together, but the infantry is too thin to cover all the armour'
          : armsA.share < 1 ? 'Tanks and infantry attack together, with tanks enough for part of the infantry' : 'Tanks and infantry attack together',
        effect: fx2 ? fx2[0].toUpperCase() + fx2.slice(1) : 'Infantry with the tanks would make both stronger',
      });
    }
    if (armsD && (armsD.foot > 0 || armsD.alone < 1)) lines.push({
      label: 'Combined arms',
      value: armsD.foot > 0 ? 'The defenders have tanks in reserve behind their infantry' : `Tanks holding ${target.terrain === 'urban' ? 'city streets' : 'forest'} with no infantry`,
      effect: [armsD.foot > 0 ? `Infantry defence up ${pc(armsD.infantry)}%` : '', armsD.alone < 1 ? `${armsD.foot > 0 ? 'tank' : 'Tank'} defence down ${pc(armsD.alone)}%` : ''].filter(Boolean).join(', '),
    });
  }
  if (wide && (rivers.size || noBridge)) {
    const name = rivers.size ? [...rivers].join(' and the ') : riverName(riverBetween(g, attackers.find((c) => noCrossing(g, c.kind, c.reg, target.id))!.reg, target.id)!);
    const crossing = overBridges + inBoats, all = attackers.length - tooSteep;
    const fx = [
      overBridges ? `over the bridges the attack is cut to ${Math.round(ENGINEERING.river.attack * 100)}%` : '',
      inBoats ? `infantry crossing in boats fights at ${Math.round((sap ? ENGINEERING.blown.withEngineers : ENGINEERING.blown.attack) * 100)}%${sap ? ', with engineers to get it over' : ''}` : '',
      noBridge ? `${noBridge === 1 ? 'one corps of tanks, lorries or guns' : `${noBridge} corps of tanks, lorries or guns`} cannot cross and ${noBridge === 1 ? 'takes' : 'take'} no part` : '',
    ].filter(Boolean).join('; ');
    lines.push({
      label: 'River',
      value: inBoats || noBridge ? `The bridges over the ${name} are down` : crossing < all ? `${crossing} of the ${all} attacking corps cross the ${name}` : `The attack crosses the ${name}`,
      effect: fx[0].toUpperCase() + fx.slice(1),
    });
  }
  if (mineLv) lines.push({
    label: 'Minefields', value: mineLv > 1 ? 'Two belts of minefields and booby traps on every road in' : 'Minefields and booby traps on every road in',
    effect: mined === 1 ? 'The engineers clear lanes: the mines cost the attack nothing' : `Attack cut to ${Math.round(mined * 100)}%${sap ? ', the engineers clearing what lanes they can' : ''}`,
  });
  if (breach && target.fort && (fortFaced || roundFaced)) lines.push({ label: 'Engineers', value: 'Assault engineers go in with the attack', effect: `They breach the works: the fortifications count for ${Math.round(breach * 100)}% less` });
  if (breach && town) lines.push({ label: 'Engineers', value: 'Assault engineers go in with the attack', effect: `They blow in the barricades: the town counts for ${Math.round(breach * 100)}% less` });
  if (target.fort && !town) {
    if (!served) lines.push({ label: 'Fortifications', value: `Level ${target.fort.lv}: ${worksName(target.fort.lv)}, dug by ${theName(g.nations[worksBy(target)])}`, effect: 'Works count only for the side that dug them: no help to the defence here' });
    else lines.push({ label: 'Fortifications', value: `Level ${target.fort.lv}: ${worksName(target.fort.lv)}`, effect: fortFaced === attackers.length ? `Attacked head-on: the attack is cut to ${Math.round(100 / FORT_MUL[target.fort.lv])}% strength` : fortFaced > 0 ? (roundFaced > 0 ? 'Partly outflanked: the fortress troops\' share keeps its works all round' : 'Partly outflanked: only the corps attacking head-on are slowed') : roundFaced > 0 ? 'Taken from behind: the fortress troops\' share keeps its works all round' : 'Outflanked: the works face the other way' });
  }
  if (coastOut) lines.push({ label: 'Coast defences', value: 'The fortress troops man the works that face the sea', effect: 'They take no part in this fight' });
  // (The guns thicken the line of the corps beside them: where they stand alone there is nobody to say it of.)
  const beside = thicken > 0 && defenders.some((d) => d.kind !== 'art');
  if (soften || beside) lines.push({
    label: 'Artillery',
    value: soften && beside ? 'Massed guns on both sides' : soften ? 'Massed guns support the attack' : 'Massed guns stand in the defence',
    effect: [
      soften ? town ? `The bombardment takes ${Math.round(soften * 100)}% off the worth of the town to its defenders` : (target.fort && (fortFaced || roundFaced)) || dugIn ? `The bombardment takes ${Math.round(soften * 100)}% off the worth of the ${target.fort && (fortFaced || roundFaced) ? (dugIn ? 'fortifications and fieldworks' : 'fortifications') : 'fieldworks'}` : 'There are no fortifications or fieldworks here for the guns to break' : '',
      beside ? `${soften ? 'the' : 'The'} other defenders fight ${Math.round(thicken * 100)}% stronger under their own guns` : '',
    ].filter(Boolean).join('; '),
  });
  if (dugIn && !wide) lines.push({ label: 'Dug in', value: dugIn === defenders.length ? 'The defenders have had a month to dig in' : `${dugIn} of the ${defenders.length} defending corps have had a month to dig in`, effect: `Troops in fieldworks defend ${Math.round((DUG_IN - 1) * 100)}% stronger` });
  // How old the works are, and what this blow takes off them whether it carries the ground or not (the grand campaign).
  const works = wide && !town && dugD > 0 ? worksD / dugD : 0;
  // The weight of the blow: a real attack counts in full, a jab by one corps at a great stack for next to nothing.
  const weight = wide && !town ? Math.min(1, (A / D) / DEFENCE.wear.fullAt) : 1;
  const wear = works > 0 ? Math.min(works, DEFENCE.wear.perAttack * weight * (1 + DEFENCE.wear.guns * soften)) : 0;
  const tired = tiredD > 0 && cmdD > 0 ? Math.min(DEFENCE.tired.most, tiredD / cmdD) : 0;
  if (tiredMost >= 0.05) {
    const pct = Math.round(DEFENCE.tired.per * tiredMost * 100);
    const n = Math.max(1, Math.round(tiredMost));
    lines.push({
      label: 'Earlier attacks',
      value: opts.after ? 'The defenders as the first wave will leave them' : tiredMost < 0.5 ? 'The defenders have been attacked lightly already this month' : `The defenders have already stood off ${n === 1 ? 'an attack' : `${n} attacks`} this month`,
      effect: `Tired men: defence down ${pct ? `as much as ${pct}%` : 'a little'} for the corps that fought. They are fresh again when their own month begins`,
    });
  }
  if (wide && !town && defenders.length && weight < 0.95) lines.push({
    label: 'Weight of attack',
    value: weight < 0.35 ? 'A jab: too light to engage more than a part of the defence' : 'A light attack for a defence of this size',
    effect: `Win or lose, the defenders lose about ${Math.round(weight * 100)}% of the men and the order a full attack would cost them, and it tires them and wears their works in the same proportion`,
  });
  if (wide && dugIn) {
    const full = DEFENCE.entrench.months, age = Math.round(works * 10) / 10;
    const built = works >= full - 0.05 ? 'their works are finished' : works < 0.05 ? 'their works have been battered flat' : `their works are ${age === 1 ? 'a month' : `${age} months`} old of the ${full} they take to finish`;
    const who = dugIn === defenders.length ? 'The defenders are dug in' : `${dugIn} of the ${defenders.length} defending corps are dug in`;
    // (The first of them names the line, the rest call it "it".)
    // (In the grand campaign the tanks break in only as far as the line's anti-tank strength lets them, and mortars work the trenches over with the guns.)
    const worked = soften && mortar ? 'the guns and mortars have' : mortar ? 'the mortars have' : 'the guns have';
    const broke = !at ? 'LINE has no armour to meet the tanks' : at.strength < 0.1 ? 'LINE has little to stop the tanks' : 'the tanks break part of the way into LINE';
    const lost = [(soften || mortar) && prepared < 1 ? `${worked} worked LINE over` : '', caught ? 'the new offensive has caught LINE off balance' : '', pierced ? broke : ''].filter(Boolean).map((t, i) => t.replace('LINE', i ? 'it' : 'the line'));
    const cover = dugD > 0 ? Math.round((DUG_IN - 1) * Math.min(1, works) * 100) : 0;
    const luck = prepared > 0.005
      ? `The day runs against the attack: an even chance takes about ${evenOdds(dice).toFixed(1)} to 1, and ${sureWords(dice)}. The attackers lose ${Math.round((DEFENCE.prepared.losses.attacker - 1) * prepared * 100)}% more men and the defenders ${Math.round((1 - DEFENCE.prepared.losses.defender) * prepared * 100)}% fewer`
      : defenders.some((d) => d.dug && !d.supplied && !fortRow(g, d)) && !defenders.some((d) => d.dug && (d.supplied || fortRow(g, d))) ? 'Cut off from supply, the line is no better than open ground on the day' : 'On the day the line is no better than open ground';
    const after = wear >= 0.05 ? `. Won or lost, this attack takes ${wear >= 0.95 && wear < 1.05 ? 'a month' : `${Math.round(wear * 10) / 10} months`} off the works${wear < works - 0.05 ? '' : ', which leaves nothing of them'}` : '';
    lines.push({
      label: 'Dug in',
      value: `${who}: ${built}${lost.length ? `, but ${listAnd(lost)}` : ''}`,
      effect: `${cover ? `Troops in fieldworks defend ${cover}% stronger. ` : ''}${luck}${after}`,
    });
  }
  const aUns = attackers.filter((c) => !c.supplied).length, dUns = defenders.filter((c) => !c.supplied).length;
  lines.push({ label: 'Supply', value: `${aUns ? `${aUns} attacking corps cut off` : 'Attackers in supply'}; ${dUns ? `${dUns} defending corps cut off` : 'defenders in supply'}`, effect: aUns || dUns ? (defenders.some((c) => !c.supplied && fortRow(g, c)) ? (defenders.every((c) => c.supplied || fortRow(g, c)) && !aUns ? 'Fortress troops cut off fight on from their works\' own stores, at full strength while those last' : 'Corps that are cut off fight at half strength; fortress troops fight on from their works\' own stores, at full strength while those last') : 'Corps that are cut off fight at half strength') : undefined });
  const wxLine = wxSky !== 'clear' ? skyBattleLine(g, zoneOfRegion(g, target.id), target.name) : null;
  if (wxLine) lines.push({ label: 'The sky', value: wxLine });
  const onCall = (n: number, who: string): string => (n ? `; ${n === 1 ? 'an air strike' : `${n} air strikes`} on call for the ${who}` : '');
  const closeWords = close ? onCall(close.att.strikes, 'attack') + onCall(close.def.strikes, 'defence') : '';
  if (wxSky !== 'closed') lines.push({ label: 'Air', value: (byNumbers ? `The ${byNumbers === 'att' ? 'attacker' : 'defender'}'s air force holds the sky here by its numbers` : skyWon || skyLost ? `The ${g.nations[sky!].adj} air force has won the sky here` : skyEdge > 0.15 ? 'Attacker holds the sky' : skyEdge < -0.15 ? 'Defender holds the sky' : 'Contested') + closeWords, effect: Math.abs(airEdge) > 0.05 ? `${airEdge > 0 ? 'Attack' : 'Defence'} strengthened by about ${Math.round(Math.abs(airEdge) * S.attack * 100)}%` : undefined });
  const gens = commandersIn(g, attackers), dgens = commandersIn(g, defenders);
  if (gens || dgens) {
    const up = rawA > 0 ? Math.round((cmdA / rawA - 1) * 100) : 0, dup = rawD > 0 ? Math.round((cmdD / rawD - 1) * 100) : 0;
    const fx = [up ? `Attack ${up > 0 ? 'up' : 'down'} ${Math.abs(up)}%` : '', dup ? `defence ${dup > 0 ? 'up' : 'down'} ${Math.abs(dup)}%` : ''].filter(Boolean).join(', ');
    lines.push({ label: 'Commanders', value: `${gens || 'No general named'} against ${dgens || 'no general named'}`, effect: fx ? fx[0].toUpperCase() + fx.slice(1) : undefined });
  }
  // Designs ahead of or behind their time, on either side.
  const kitOf = (cs: Corps[]): Field[] => [...new Set(cs.map((c): Field => (c.kind === 'arm' || c.kind === 'mot' ? 'armour' : 'infantry')))];
  const kitA = [...techNotes(g, aN, kitOf(attackers)), ...techNotes(g, aN, ['doctrine'])], kitD = techNotes(g, dN, kitOf(defenders));
  if (kitA.length || kitD.length) {
    const up = (k: number): string => `${k > 0 ? 'up' : 'down'} ${Math.abs(k)}%`;
    lines.push({
      label: 'Equipment', value: [...kitA, ...kitD].map((x) => x.text).join('; '),
      effect: [...kitA.map((x) => `Attack ${up(x.pct)}`), ...kitD.map((x) => `Defence ${up(x.pct)}`)].join(', '),
    });
  }
  if (proudA > 1 || proudD > 1) lines.push({
    label: 'Honours', value: proudA > 1 && proudD > 1 ? 'Honoured corps on both sides of this fight' : `Corps that carry honours, are fighting for their home ground or face an old enemy are ${proudA > 1 ? 'attacking' : 'defending'} here`,
    effect: [proudA > 1 ? `Attack up as much as ${Math.round((proudA - 1) * 100)}% for those corps` : '', proudD > 1 ? `${proudA > 1 ? 'defence' : 'Defence'} up as much as ${Math.round((proudD - 1) * 100)}% for those corps` : ''].filter(Boolean).join(', '),
  });
  if (opts.sea !== undefined) lines.push(opts.air
    ? { label: 'Airborne assault', value: 'By parachute and glider, without heavy weapons', effect: `Attack cut to ${Math.round(opts.sea * 100)}%` }
    : opts.surprise
      ? { label: 'Landing', value: 'From the sea, on the day war is declared', effect: 'The defenders are caught unready: the landing fights at full strength' }
      : { label: 'Landing', value: 'From the sea, against a defended shore', effect: `Attack cut to ${Math.round(opts.sea * 100)}%` });
  if (trap) lines.push({ label: 'Encirclement', value: 'The defenders have no line of retreat', effect: 'A general who knows how to close a pocket hits 15% harder' });
  // Units decide (#7 4(b)): what a beaten defence would lose to the chase, read by both sides before the battle (129(d)).
  if (unitsDecide(g) && !town && !opts.breakout && defenders.some(fights)) {
    const ours = g.human !== null && dN.has(g.human);
    const banned = new Set(attackers.map((c) => c.reg)), blocks = new Map<number, Corps>(), cutAt = new Set<number>();
    let chasedBy: Corps | null = null;
    for (const c of defenders) {
      if (c.str < 0.15 || !afoot(c)) continue;
      for (const j of roadsCut(g, c, target, attNat, banned, blocks, false)) cutAt.add(j);
      for (const j of target.nb) {
        const r = g.regions[j];
        if (!fallsOn(g, c, r, banned)) continue;
        const by = attackers.find((u) => chaser(u) && chasePace(g, u, c, r) < pace(g, c, r));
        if (by && (!chasedBy || KIND[by.kind].mobile && !KIND[chasedBy.kind].mobile)) chasedBy = by;
      }
    }
    const arm = (u: Corps | undefined): string => (u?.kind === 'arm' ? 'armour' : u?.kind === 'mot' ? 'lorried troops' : u?.kind === 'cav' ? 'cavalry' : 'troops');
    if (cutAt.size) {
      const at = listAnd([...cutAt].map((j) => g.regions[j].name));
      lines.push({ label: 'Pursuit', value: `${ours ? 'Our' : 'Their'} road out is cut at ${at}`, effect: `If the line breaks, ${ours ? 'our' : 'their'} men on foot who can go no other way are taken whole by the ${arm(blocks.get([...cutAt][0]))} already there` });
      // The breakout comes first: against the weakest of the enemy on those roads, by the card's own defence worth there.
      const breakers = defenders.filter((c) => c.str >= 0.15 && c.kind !== 'fort');
      let road: Region | null = null, worst = Infinity;
      for (const j of cutAt) {
        const u = blocks.get(j), r = g.regions[j];
        if (!u) continue;
        const D = onTheRoad(g, u, r, () => assess(g, breakers, r, { breakout: true }).D);
        if (D < worst) { worst = D; road = r; }
      }
      if (road) lines.push({ label: 'Breakout', value: `Breaking out at ${road.name}`, effect: `If the line breaks, ${ours ? 'our' : 'their'} beaten army first attacks the enemy on its road out there; if that fails, ${ours ? 'our' : 'their'} men on foot who can go no other way are taken whole` });
    } else if (chasedBy) {
      lines.push({ label: 'Pursuit', value: `${ours ? 'Our foot cannot outrun their' : 'Their foot cannot outrun our'} ${arm(chasedBy)}`, effect: `If the line breaks, about ${Math.round(PURSUIT.share * 100)}% of ${ours ? 'our' : 'their'} men on foot are taken as they fall back` });
    }
  }
  if (lineMul < 1) lines.push(attackers.some((c) => inDesert(g.regions[c.reg]))
    ? { label: 'Supply line', value: `Stretched over ${Math.round(longest)} regions of desert from the nearest working port`, effect: `Attack cut to as little as ${Math.round(lineMul * 100)}% until a port nearer the front is working` }
    : { label: 'Supply line', value: `Stretched over ${Math.round(longest)} regions of Soviet railway`, effect: `Attack cut to as little as ${Math.round(lineMul * 100)}% until the track behind is relaid` });
  if (railA < 1) lines.push({ label: 'Railhead', value: `Attackers as far as ${roadA} provinces by road beyond their nearest working railway`, effect: `Attack cut to as little as ${Math.round(railA * 100)}% until engineers bring the line forward` });
  if (railD < 1) lines.push({ label: 'Railhead', value: `Defenders as far as ${roadD} provinces by road beyond their nearest working railway`, effect: `Defence cut to as little as ${Math.round(railD * 100)}% until their line is relaid` });
  if (turned) lines.push(fewGuns
    ? { label: 'Open desert', value: 'Dug in, but with too few anti-tank guns the position can be driven round', effect: `Foot infantry defends at ${Math.round(DESERT.open * 100)}% against tanks and lorries` }
    : turnedTo > DESERT.open
    ? { label: 'Open desert', value: 'Infantry on foot whose works are not finished, against tanks and lorried troops', effect: `Foot infantry defends at as little as ${Math.round(turnedTo * 100)}% until the box is dug` }
    : { label: 'Open desert', value: 'Infantry on foot with no works to anchor on, against tanks and lorried troops', effect: `Foot infantry defends at ${Math.round(DESERT.open * 100)}%` });
  if (cold) lines.push({ label: 'Winter', value: 'Attackers without winter equipment', effect: `Attack cut to ${Math.round(EASTERN.winter.attack * 100)}%` });
  if (thin < 1) lines.push({ label: 'Supply line', value: 'Defenders at the end of a long supply line', effect: `Defence cut to as little as ${Math.round(thin * 100)}%` });
  if (coldDef) lines.push({ label: 'Winter', value: 'Defenders without winter equipment', effect: `Defence cut to ${Math.round(EASTERN.winter.defence * 100)}%` });
  if (tempo) lines.push({ label: 'Tempo', value: 'New offensive', effect: 'Fast troops hit 20% harder in the first two turns' });
  if (giftA !== 1 || giftD !== 1) {
    const a = giftA !== 1 ? traitNames(g, attackers, target, 'attack', !land) : [], d = giftD !== 1 ? traitNames(g, defenders, target, 'defence') : [];
    const say = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
    lines.push({
      label: 'Abilities and technology', value: [a.length ? `Attacking: ${say(a)}` : '', d.length ? `Defending: ${say(d)}` : ''].filter(Boolean).join('. '),
      effect: [giftA !== 1 ? `Attack up as much as ${Math.round((giftA - 1) * 100)}% for the corps it covers` : '', giftD !== 1 ? `${giftA !== 1 ? 'defence' : 'Defence'} up as much as ${Math.round((giftD - 1) * 100)}% for the corps it covers` : ''].filter(Boolean).join(', '),
    });
  }
  if (shortA < 1 || shortD < 1) {
    // Who is short of what: the nation on each side whose depots are emptiest.
    const worst = (cs: Corps[]): string => {
      let fuel = 1, sup = 1;
      for (const c of cs) { fuel = Math.min(fuel, c.kind === 'arm' || c.kind === 'mot' ? fuelMet(g, c.nat) : 1); sup = Math.min(sup, suppliesMet(g, c.nat)); }
      return [fuel < 1 ? `fuel for ${Math.round(fuel * 100)}% of what their tanks and lorries need` : '', sup < 1 ? `${Math.round(sup * 100)}% of the supplies they need` : ''].filter(Boolean).join(' and ');
    };
    lines.push({
      label: 'Oil and supplies', value: [shortA < 1 ? `Attackers have ${worst(attackers)}` : '', shortD < 1 ? `Defenders have ${worst(defenders)}` : ''].filter(Boolean).join('. '),
      effect: [shortA < 1 ? `Attack cut to as little as ${Math.round(shortA * 100)}%` : '', shortD < 1 ? `${shortA < 1 ? 'defence' : 'Defence'} cut to as little as ${Math.round(shortD * 100)}%` : ''].filter(Boolean).join(', '),
    });
  }
  const heartA = forceMorale(attackers), heartD = forceMorale(defenders);
  if (heartA !== null || heartD !== null) {
    const say = (m: number | null, who: string): string => (m === null ? '' : `${who} ${moraleWord(m).toLowerCase()} (${Math.round(m * 100)}%)`);
    const fx = (m: number | null, what: string): string => {
      const pct = m === null ? 0 : Math.round((m - 0.5) * MORALE.fight.span * 100);
      return pct ? `${what} ${pct > 0 ? 'up' : 'down'} ${Math.abs(pct)}%` : '';
    };
    const value = [say(heartA, 'Attackers'), say(heartD, heartA === null ? 'Defenders' : 'defenders')].filter(Boolean).join(', ');
    const effect = [fx(heartA, 'Attack'), fx(heartD, 'defence')].filter(Boolean).join(', ');
    lines.push({ label: 'Morale', value, effect: effect ? effect[0].toUpperCase() + effect.slice(1) : undefined });
  }
  if (spied) lines.push({ label: 'Intelligence', value: `Agent ${spied} has mapped the defences of ${target.name}`, effect: `Attack up ${Math.round(INTEL.battle.attack * 100)}%` });
  // (The tell for a hidden watcher: the battle shows that the enemy knew the defences, and so where to hunt.)
  // (It names the army that attacks, and the agent's own country where that is an ally of the attacker's.)
  if (watched) {
    const adj = g.nations[watched.nat]?.adj ?? 'enemy', by = g.nations[watched.by]?.adj ?? 'enemy', own = watched.by === watched.nat;
    const an = /^[AEIOU]/.test(adj) && !/^(Uru|Ukr|Uni|Eu)/.test(adj) ? 'an' : 'a';
    const value = watched.found
      ? `The ${adj} agent uncovered at ${target.name} has mapped its defences${own ? '' : ` for the ${by} army`}`
      : `The ${by} army knew the defences of ${target.name}: ${own ? 'one of its agents' : `${an} ${adj} agent`} is at work there`;
    lines.push({ label: 'Enemy intelligence', value, effect: `Attack up ${Math.round((watched.mul - 1) * 100)}%` });
  }
  if (sneaked) lines.push({ label: 'Surprise', value: 'War was declared this month on a country at peace', effect: `The defenders are caught unready: attack up ${Math.round((RELATIONS.sneak.attack - 1) * 100)}%` });
  // (In the grand campaign WHAT YOU FACE says why, and the ticket keeps the figure alone: ruling 149(e).)
  if (armorShock) lines.push(at ? { label: 'Armor', value: `Tank attack up ${Math.round(25 * (1 - at.strength))}%` } : { label: 'Armor', value: 'Tanks against a defence without armor', effect: 'Tank attack up 25%' });
  if (armorStuck) lines.push({ label: 'Armor', value: 'Tanks in close country', effect: 'Tank attack cut to 60%' });
  if (streets) lines.push({ label: 'Armor', value: 'Tanks in the streets of the town', effect: `Tank attack cut to ${Math.round(SIEGE.assault.armour * 100)}%` });
  if (hasty) lines.push({ label: 'A hasty assault', value: 'The army has only just come up to the town: it goes in off the march', effect: `Attack cut to ${Math.round(rushed * 100)}%. Next month the assault is made ready and goes in at full weight` });
  if (flank > 1) lines.push({ label: 'Direction', value: `Attack from ${sources.size} sides`, effect: `Attack up ${Math.round((flank - 1) * 100)}%` });
  // What the battle card tells of it: who will pay, how much of the tanks' shock is left, and what he brings against the works.
  let faced: Faced | null = null;
  if (wide) {
    const sniped = snipedIn(target, town);
    const toll = tollOf(g, attackers, defenders, at, facing, sniped, mgUp);
    const tanksIn = attackers.some((c) => c.kind === 'arm');
    const dugNow = facing ? defenders.some((d) => facing.works(d) > 0) : false;
    faced = {
      antiTank: at, toll, shock: at && tanksIn && !armorStuck ? 1 - at.strength : null, alone: armorAlone,
      machineGuns: toll.why.machineGuns > 0 && onFoot > 0, mgDug: facing ? defenders.some((d) => machineGuns(g, d, 1, 0) > 1 && facing.works(d) > 0) : false,
      worksCut: dugNow ? 1 - (1 - soften) * (1 - mortar) : 0, mortars: mortar > 0, gunsIn: soften > 0, snipers: onFoot > 0 ? sniped : null,
    };
    // The figures, for the ticket: the words are on the card.
    if (facing) {
      let w = 0, up = 0;
      for (const d of defenders) { const k = d.divs * d.str; w += k; up += k * (machineGuns(g, d, onFoot, facing.works(d)) - 1); }
      if (armorAlone >= 0.005) lines.push({ label: 'Tanks alone', value: `Tank attack cut by ${Math.round(armorAlone * 100)}%`, ...(aloneOver < 1 ? { effect: `This takes the place of the ${Math.round((1 - aloneOver) * 100)}% cut for tanks alone in close country` } : {}) });
      if (w > 0 && up / w >= 0.005) lines.push({ label: 'Machine guns', value: `Defence up ${Math.round((up / w) * 100)}% against the men on foot` });
      if (mortar >= 0.005 && dugNow) lines.push({ label: 'Mortars', value: `The fieldworks count for ${Math.round(mortar * 100)}% less` });
      if (mgUp && onFoot > 0) lines.push({ label: 'Machine-gun brigades', value: `Defence up to ${Math.round(mgUp * onFoot * 100)}% more against the men on foot` });
      if (attackers.some((c) => assaultFire(g, c) > 1)) lines.push({ label: 'Assault rifles', value: `Infantry attack up ${Math.round(SUPPORT.assault.attack * 100)}%` });
    }
    const share = (x: number): string => `${Math.round(x * 100)}%`;
    // (Only the arms that go in, and no row where each of them pays the usual share.)
    const arms: [string, number][] = [];
    if (tanksIn) arms.push(['tanks', toll.tanks]);
    if (attackers.some((c) => c.kind !== 'arm' && c.kind !== 'art' && !isTeam(c))) arms.push(['men on foot', toll.foot]);
    if (attackers.some((c) => c.kind === 'art' || isTeam(c))) arms.push(['guns', toll.guns]);
    if (arms.some((x) => Math.abs(x[1] - 1) >= 0.005)) lines.push({ label: 'Who pays', value: `Of the usual share of losses: ${listAnd(arms.map((x) => `${x[0]} ${share(x[1])}`))}` });
  }
  // Guns caught alone in the field are overrun (ruling 146, the grand campaign): with no infantry or tanks in front
  // of them they make no stand, whatever the figures say. The odds are at least what the worst of the day's luck
  // still makes a decisive win, so the forecast is certain and the battle is fought on the same ticket.
  // (The town's guard fighting beside them (ruling 232) stands in front of them. Support teams caught alone, release 2,
  // are overrun as guns are.)
  const overrun = gunsRule(g) && !town && !guarded && defenders.some((c) => c.kind === 'art' || isTeam(c)) && defenders.every((c) => c.kind === 'art' || c.kind === 'eng' || isTeam(c));
  if (overrun) lines.push(GUNS_OVERRUN);
  if (overrun) dice = today;
  return { A, D, odds: overrun ? Math.max(A / D, OVERRUN_ODDS / dice[0]) : A / D, lines, defenders, dice, ...(dice !== today ? { luck: today } : {}), prepared, works, wear, weight, tired, faced, ...(overrun ? { overrun } : {}), ...(guarded > 0 ? { guard: guarded } : {}), ...(close ? { air: close } : {}) };
}

function reportUnit(c: Corps): ReportUnit {
  return { name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: men(c), lost: 0, fate: '' };
}

export function removeCorps(g: Game, c: Corps): void {
  c.dead = true;
  // (The whole-world chart: a withered island garrison's count goes with it. Ruling 173(e), wither.ts.)
  if (farOn(g)) witherDrop(g, c, true);
  const gen = generalOf(g, c);
  if (gen) { gen.corps = -1; c.general = null; }
  const r = g.regions[c.reg];
  if (c.town) quitTown(g, c); else r.units.splice(r.units.indexOf(c), 1);
  g.corps.splice(g.corps.indexOf(c), 1);
}

// ---- Fieldworks left on the ground (the grand campaign) ------------------------------
// Works belong to the ground, not to the men who dug them. A dug-in corps that marches off, entrains, or goes
// forward after a battle won leaves its positions where they are: a corps of its own side standing there without
// works takes them over at once, and otherwise they wait on the ground for whoever marches in next. Empty, they
// lose a month's work with every month, and they are gone the moment the enemy sets foot there. Troops driven
// off their ground leave nothing a relief could use, and troops falling back never take works over: nobody
// retires into a ready line. Kept in the flags, since a region is saved as a fixed row: `fw:` the months of work
// in each position, `fwk:` how many corps they hold, `fwn:` whose they are.

/** Fieldworks standing empty in a province: how many months' work, room for how many corps, and whose. */
export interface GroundWorks { months: number; corps: number; nat: string }

const handover = (g: Game): boolean => g.flags.open === 1 && DEFENCE.entrench.auto && !!DEFENCE.handover;

/** The fieldworks left empty on a province's ground, if any (the grand campaign): for both interfaces. */
export function groundWorks(g: Game, reg: number): GroundWorks | null {
  const months = g.flags[`fw:${reg}`];
  if (months === undefined || months <= 0) return null;
  const nat = Object.keys(g.nations)[g.flags[`fwn:${reg}`] ?? -1];
  return nat ? { months, corps: g.flags[`fwk:${reg}`] ?? 1, nat } : null;
}

function clearWorks(g: Game, reg: number): void {
  if (g.flags[`fw:${reg}`] === undefined) return;
  delete g.flags[`fw:${reg}`]; delete g.flags[`fwk:${reg}`]; delete g.flags[`fwn:${reg}`];
}

/** True if this corps could man works found or handed over where it stands: in the field, and not under orders to give ground. */
const mansWorks = (c: Corps): boolean => !c.dead && c.kind !== 'fort' && c.town === undefined && !c.yields && !c.dug;

/** Called before a corps leaves its ground in good order: its works stay behind it. */
function leaveWorks(g: Game, c: Corps): void {
  if (!handover(g) || !c.dug || c.town !== undefined || c.kind === 'fort') return;
  const months = Math.round(worksOf(c) * 100) / 100;
  if (months <= 0) return;
  const from = g.regions[c.reg];
  // Hand over the line: troops of its own side already standing there without works take these as their own.
  const heir = friendlyUnits(g, from, c.nat).find((u) => u !== c && mansWorks(u));
  if (heir) { heir.dug = true; heir.dugFor = months; heir.relief = true; return; }
  const old = groundWorks(g, from.id);
  const n = old ? old.corps : 0;
  g.flags[`fw:${from.id}`] = Math.round(((old ? old.months * n : 0) + months) / (n + 1) * 100) / 100;
  // (Works hold no more corps than the ground has room for: five, or an island's own room. islands.ts.)
  g.flags[`fwk:${from.id}`] = Math.min(islandOf(g, from)?.room ?? STACK, n + 1);
  g.flags[`fwn:${from.id}`] = Object.keys(g.nations).indexOf(c.nat);
}

/** Called when a corps has marched or come by rail onto ground its side holds: it takes over works standing empty there. */
function takeWorks(g: Game, c: Corps, to: Region): void {
  if (!handover(g) || c.reg !== to.id || !mansWorks(c)) return;
  const left = groundWorks(g, to.id);
  if (!left || !allied(g, c.nat, left.nat)) return;
  c.dug = true; c.dugFor = left.months; c.relief = true;
  if (left.corps <= 1) clearWorks(g, to.id); else g.flags[`fwk:${to.id}`] = left.corps - 1;
}

/** Works nobody mans fall in: a month's work gone with each month of their owner's (the start of his month). */
function fadeWorks(g: Game, nat: string): void {
  if (g.flags.open !== 1) return;
  const idx = Object.keys(g.nations).indexOf(nat), fade = DEFENCE.handover?.fade ?? 1;
  for (const r of g.regions) {
    const key = flagName('fw:', r.id);
    const m = g.flags[key];
    if (m === undefined || g.flags[flagName('fwn:', r.id)] !== idx) continue;
    if (m - fade <= 0.001) clearWorks(g, r.id); else g.flags[key] = Math.round((m - fade) * 100) / 100;
  }
}

/** Puts a corps down in another region without a march: rail, ship, or an order from the capital. */
export function relocate(g: Game, c: Corps, to: Region): void { place(g, c, to); }

function place(g: Game, c: Corps, to: Region): void {
  const r = g.regions[c.reg];
  // A corps of a garrison comes off the town's list, not the field's (the grand campaign).
  if (c.town) quitTown(g, c); else r.units.splice(r.units.indexOf(c), 1);
  // Engineers driven off their work, or marched away from it, have done nothing: the job is dropped there and then.
  if (c.job && c.job.reg !== to.id) delete c.job;
  c.reg = to.id;
  // Put down where the enemy holds the country round a town, it goes into the town: the two never share a field.
  // A full town takes no more: the corps is set down on the nearest ground its side still holds.
  if (siegeRules(g) && to.units.some((u) => atWar(g, u.nat, c.nat))) {
    const alt = musterNear(g, c.nat, to);
    if (!alt) { joinTown(g, to, c); return; }
    to = alt;
    c.reg = to.id;
  }
  to.units.push(c);
  // Trenches stay where they were dug.
  if (c.dug) delete c.dug;
  if (c.relief) delete c.relief;
  // Works left empty here by the other side are overrun as its enemy sets foot on the ground (the grand campaign).
  if (g.flags[`fw:${to.id}`] !== undefined) { const left = groundWorks(g, to.id); if (!left || atWar(g, c.nat, left.nat)) clearWorks(g, to.id); }
}

/** Sends a corps to the nearest ground its side holds, or strikes it off if there is none: for troops left where they can no longer stand. */
export function sendHome(g: Game, c: Corps): void {
  const to = nearestFriendly(g, c);
  if (to) { place(g, c, to); c.ap = 0; } else removeCorps(g, c);
}

/**
 * True if a province would still be fed for a nation with the ground `lost` no longer its side's: joined to a source
 * of its supply through ground that is fed now, the lost ground left out. Read from the supply picture as it stands
 * (the grand campaign marks the sources on it); nothing is drawn again.
 */
function fedWithout(g: Game, nat: string, at: Region, lost: Region): boolean {
  const ok = g.supply[nat];
  if (!ok?.[at.id]) return false;
  if (!groundRules(g) || ok[at.id] === 2) return true;
  const seen = new Uint8Array(g.regions.length);
  seen[at.id] = 1; seen[lost.id] = 1;
  const q = [at.id];
  for (let i = 0; i < q.length; i++) {
    for (const j of g.regions[q[i]].nb) {
      if (seen[j] || !ok[j]) continue;
      if (ok[j] === 2) return true;
      seen[j] = 1; q.push(j);
    }
  }
  return false;
}

export function retreatTarget(g: Game, c: Corps, from: Region, banned: Set<number>): Region | null {
  if (c.kind === 'fort') return null;
  let best: Region | null = null, bs = 1e9;
  for (const j of from.nb) {
    const r = g.regions[j];
    // (In the grand campaign an army also falls back on the lines of its own side round a town it is besieging.)
    if (!fallsOn(g, c, r, banned)) continue;
    // (An island of the Pacific takes a beaten army two over its room, as any ground takes two over its five: islands.ts.)
    if (placed(g, r, c.nat).length >= roomAt(g, r, c.nat) + 2) continue;
    const s = fallScore(g, c, from, r, banned);
    if (s < bs) { bs = s; best = r; }
  }
  return best;
}

/** True where a beaten corps can fall back onto this ground: its side's, held by nobody against it, open to it. */
function fallsOn(g: Game, c: Corps, r: Region, banned: Set<number>): boolean {
  return !banned.has(r.id) && allied(g, c.nat, fieldCtrl(g, r)) && !hostileUnits(g, r, c.nat).length && !barred(g, c, r);
}

/** How badly this ground serves a corps falling back onto it from there: the lower, the better. (`lost`: the field it was beaten on, where that is not `from`.) */
function fallScore(g: Game, c: Corps, from: Region, r: Region, banned: Set<number>, lost: Region = from): number {
  const d = g.capDist[c.nat];
  const j = r.id;
  const abroad = g.nations[c.nat].seaSupply && r.owner !== c.nat;
  // In the desert an army falls back along its own supply line, toward the port that feeds it.
  // It also keeps out of a dead end: a place with one way out, or none, is a trap for a beaten force.
  // (An old-chart candidate counts its old-chart neighbours only: with Sebha on the board Hon read one way out and
  // the army fell back there, to starve one province nearer, where with the far side off it goes to Tripoli. The
  // design director's 11:53Z of 22 November 2026: the far chart does not change how an old-chart retreat reads.)
  const old = farOn(g) && shoreOf(r) === 0;
  const ways = inDesert(r) ? r.nb.filter((k) => k !== from.id && !banned.has(k) && allied(g, c.nat, g.regions[k].ctrl) && !(old && shoreOf(k) !== 0)).length : 2;
  const back = (inDesert(r) ? g.reach[c.nat]?.[j] ?? 0 : abroad ? (r.port ? 0 : 3) : d ? d[j] : 0) + (ways === 0 ? 4 : ways === 1 ? 1.5 : 0);
  // (Far-side ground is weighed by whether its feed survives the retreat: fed there means fed once the ground the
  // corps is leaving is lost. Beaten at Nalut with Misrata gone, Italy's 5th Army fell back on Sebha, whose line ran
  // through Nalut, and starved there in every one of six wars; with the far side off there is no Sebha. The design
  // director's 11:40Z of 22 November 2026 to ruling 234: far-side ground only, the old chart's scoring as it was.)
  const fed = farOn(g) && shoreOf(r) === 2 ? fedWithout(g, c.nat, r, lost) : !!g.supply[c.nat]?.[j];
  return back + (fed ? 0 : 6) + r.units.length * 0.3;
}

/**
 * Where a corps beaten in the field goes when no ground of its side next to the field has room for it (Units decide,
 * audit #7; the design director's ruling of 23 November 2026 10:20Z). A full rear is not an encirclement:
 *   1. it falls back through a full friendly neighbour to the next ground of its side with room, as one retreat;
 *   2. failing that, it stands in the full friendly neighbour over the stack for the month, under the stack's own rules,
 *      for its staff or its player to clear the next.
 * Null where no ground of its side touches the field at all: then it is taken whole, as ruling 232 has it. Off (and in
 * release 1 and 1.5), always null, and the corps is taken as before.
 */
function fallThrough(g: Game, c: Corps, from: Region, banned: Set<number>): Region | null {
  if (!unitsDecide(g) || c.kind === 'fort') return null;
  const full = from.nb.map((j) => g.regions[j]).filter((r) => fallsOn(g, c, r, banned));
  let best: Region | null = null, bs = 1e9;
  for (const n of full) {
    for (const k of n.nb) {
      const m = g.regions[k];
      if (k === from.id || !fallsOn(g, c, m, banned) || placed(g, m, c.nat).length >= roomAt(g, m, c.nat) + 2) continue;
      const s = fallScore(g, c, n, m, banned, from);
      if (s < bs) { bs = s; best = m; }
    }
  }
  if (best) return best;
  for (const n of full) {
    const s = fallScore(g, c, from, n, banned);
    if (s < bs) { bs = s; best = n; }
  }
  return best;
}

// ---- The pursuit (Units decide, #7 step 4(b); the design director's rulings of 24 Nov 12:41Z and 12:43Z) ---------
// A beaten army is chased by whoever is faster over the ground it falls back across. Speed is the ground's own, read
// as moveCost reads it but without the march road, which both sides would share: on open ground tanks and lorries
// outrun men on foot; in forest tanks do not, though lorries do; in mountain and marsh nobody outruns anybody, and
// mountain troops outrun tanks in their mountains. The record (historian/prisoner-share-1124.md) only sizes it.

/** How hard this corps finds it to get into a province in a pursuit: the ground's cost to its arm, off the roads. */
export function pace(g: Game, c: Corps, to: Region, kind: Kind = c.kind): number {
  let cost = KIND[kind].move + groundMarch(g, kind, to);
  if (to.terrain === 'mountain' && kind !== 'mtn') cost += 1;
  if (to.terrain === 'marsh') cost += 1;
  if (weather(g).mud && EAST.has(to.owner)) cost += 1;
  if (frozen(g, c.nat, to) && cmdWinter(g, c) >= 1) cost += 1;
  return cost;
}

/** The share of a corps that rides: lorries, half-tracks or carriers. By its arm until a corps carries its own lorry share. */
export const lifted = (c: Corps): number => (c.kind === 'mot' || c.kind === 'arm' ? 1 : 0);
/** Men on foot: what a pursuit can catch. Horsemen are faster than foot, and tanks and lorries ride. */
const afoot = (c: Corps): boolean => !KIND[c.kind].mobile && c.kind !== 'fort';
/** Fit to chase: a fighting arm, fed (no fuel, no chase), at 15 in 100 of its size or more. */
const chaser = (c: Corps): boolean => fights(c) && c.supplied && c.str >= 0.15;
/**
 * A pursuer's pace after a corps it chases: its own, or a man's on foot when it came out of the fight more
 * disorganised than the men it chases (the design director's 13:10Z: a winner bled to a standstill does not run).
 */
const chasePace = (g: Game, u: Corps, c: Corps, to: Region): number => (u.org < c.org && KIND[u.kind].mobile ? pace(g, u, to, 'inf') : pace(g, u, to));

/**
 * The ways out of the field this beaten corps has lost to a faster enemy (a cut road): a province of its side next
 * to the field with none of its side standing in it, where a motor corps of the winning side standing next to it
 * gets there at less cost than this corps does. Each such corps cuts one road and pays the action points; a road
 * already cut stays cut for every corps it outruns. Off, or nothing cut, an empty set.
 */
function roadsCut(g: Game, c: Corps, from: Region, winner: string, banned: Set<number>, blocks: Map<number, Corps>, pay = true, open?: Set<number>): Set<number> {
  const cut = new Set<number>();
  if (!unitsDecide(g) || !afoot(c) && lifted(c) >= 1) return cut;
  for (const j of from.nb) {
    const r = g.regions[j];
    if (open?.has(j) || !fallsOn(g, c, r, banned) || r.units.some((u) => allied(g, u.nat, c.nat)) || garrisonOf(r).some((u) => allied(g, u.nat, c.nat))) continue;
    const mine = pace(g, c, r);
    const held = blocks.get(j);
    if (held) { if (chasePace(g, held, c, r) < mine) cut.add(j); continue; }
    const taken = new Set([...blocks.values()].map((u) => u.id));
    let by: Corps | null = null;
    for (const k of r.nb) {
      if (k === from.id) continue;
      for (const u of g.regions[k].units) {
        if (taken.has(u.id) || (u.kind !== 'mot' && u.kind !== 'arm') || !allied(g, u.nat, winner) || !atWar(g, u.nat, c.nat) || !chaser(u)) continue;
        const cost = chasePace(g, u, c, r);
        if (cost < mine && u.ap >= cost && (!by || cost < chasePace(g, by, c, r))) by = u;
      }
    }
    if (by) { blocks.set(j, by); if (pay) by.ap -= chasePace(g, by, c, r); cut.add(j); }
  }
  return cut;
}

/**
 * The men a faster pursuit takes from a corps falling back into this province: its foot that does not ride, a share.
 * (Release 2, rearguard.ts; the design director's 19:13Z and 19:22Z) Rearguards act on the race through time only,
 * never by more than the pursuers lose: 'lost', the time the attackers already lost to rearguards in this fight, as
 * the fight decided it (half where they broke; not for a rearguard's own pursuers); and a rearguard of its side holding
 * the province it falls back into, unfought, costs the pursuers pressing the chase its delay, each paying it once a
 * battle ('paid').
 */
function pursuitTakes(g: Game, c: Corps, to: Region, chasers: Corps[], lost = 0, paid?: Set<number>): number {
  if (!unitsDecide(g) || !afoot(c)) return 0;
  const mine = pace(g, c, to);
  const fast = chasers.filter((u) => chaser(u) && chasePace(g, u, c, to) < mine);
  if (!fast.length) return 0;
  const held = screenWeeks(g, c, to, (nat) => nat === c.nat || allied(g, nat, c.nat));
  if (held > 0) for (const u of fast) if (!paid?.has(u.id)) { paid?.add(u.id); u.ap = Math.max(0, u.ap - held); }
  const slow = lost + held;
  if (slow > 0 && !fast.some((u) => chasePace(g, u, c, to) + slow < mine)) return 0;
  return PURSUIT.share * (1 - lifted(c));
}

/**
 * A cutting corps stands on the road it cut while a breakout is fought against it there, out of any works; it goes
 * back to where it came from, as it was, if the fight leaves it standing on that road. (Moved by hand, not marched:
 * it never left its province on the map, and nothing else about it changes.)
 */
function onTheRoad<T>(g: Game, u: Corps, road: Region, fight: () => T): T {
  const home = g.regions[u.reg], at = home.units.indexOf(u), dug = u.dug;
  home.units.splice(at, 1); road.units.push(u); u.reg = road.id; if (dug !== undefined) u.dug = false;
  try { return fight(); }
  finally {
    // (Put back in its own place in the list: whoever was reading the list round it reads it unchanged.)
    if (!u.dead && u.reg === road.id && road.units.includes(u)) {
      road.units.splice(road.units.indexOf(u), 1); home.units.splice(Math.min(at, home.units.length), 0, u); u.reg = home.id; if (dug !== undefined) u.dug = dug;
    }
  }
}

/**
 * The breakout (Units decide, #7 4(b); the design director's 12:48Z and 12:49Z): before a cut road takes any corps
 * whole, the beaten stack's corps still at 15 in 100 or more attack the weakest cutting corps, by the card's defence
 * worth on the road it cut, once, as an ordinary battle on the card. Won, that road is open again for the whole
 * stack; lost, the cut stands. Null where no cut would take anybody. The pursuit is off inside it.
 */
function breakOut(g: Game, stack: Corps[], from: Region, winner: string, banned: Set<number>, open: Set<number>): BattleReport | null {
  const dry = new Map<number, Corps>();
  let doomed = false;
  for (const c of stack) {
    if (c.dead || c.reg !== from.id || c.str < 0.15 || !afoot(c)) continue;
    const outs = from.nb.filter((j) => fallsOn(g, c, g.regions[j], banned));
    if (!outs.length) continue;
    const cut = roadsCut(g, c, from, winner, banned, dry, false, open);
    if (outs.every((j) => cut.has(j))) doomed = true;
  }
  if (!doomed) return null;
  const breakers = stack.filter((c) => !c.dead && c.reg === from.id && c.str >= 0.15 && c.kind !== 'fort');
  if (!breakers.some(fights)) return null;
  let road: Region | null = null, block: Corps | null = null, worst = Infinity;
  for (const [j, u] of dry) {
    const r = g.regions[j];
    const D = onTheRoad(g, u, r, () => assess(g, breakers, r, { breakout: true }).D);
    if (D < worst) { worst = D; road = r; block = u; }
  }
  if (!road || !block) return null;
  const at = road;
  const goers = limitAttackers(g, breakers, at);
  const rep = onTheRoad(g, block, at, () => resolveBattle(g, goers.length ? goers : breakers, at, { breakout: true }));
  if (rep.taken) open.add(at.id);
  return rep;
}

/** The winners move in, fastest first. */
/**
 * An army that closes round a held town has the rest of its month's work there (the grand campaign): the corps that
 * came up march no further. They can still storm the town, which asks no marching of them.
 */
function closeRound(g: Game, r: Region, corps: Corps[]): void {
  for (const c of corps) if (!c.dead && c.reg === r.id && !c.town) c.ap = 0;
}

function moveIn(g: Game, attackers: Corps[], target: Region, opts: AttackOpts, attU: ReportUnit[]): void {
  // (The grand campaign: the fighting corps go first, and guns and engineers follow only onto ground one of them stands on: ruling 211(a).)
  const lead = gunsRule(g);
  const order = attackers.map((c, i) => ({ c, i })).sort((x, y) => (lead ? Number(fights(y.c)) - Number(fights(x.c)) : 0) || KIND[x.c.kind].move - KIND[y.c.kind].move || corpsBase(g, y.c) - corpsBase(g, x.c));
  let moved = 0;
  // (An island of the Pacific takes no more of the winners than it has room for, those of their side already on it
  // counted, as a march onto it would be: rulings 173(b) and 179(s), islands.ts. The rest stay where they came from.)
  const isle = islandOf(g, target) !== null;
  for (const { c, i } of order) {
    // Troops who stormed a town from the lines round it are already there.
    if (c.reg === target.id) continue;
    if (lead && !fights(c) && !gunsMayFollow(g, c, target)) { c.ap = Math.max(0, c.ap - 1); continue; }
    // A force that came by sea all goes ashore together.
    const cost = opts.sea !== undefined ? 0 : moveCost(g, c, target);
    if (moved >= (opts.occupy ?? STACK) || (isle && !hasRoom(g, target, c)) || (moved > 0 && c.ap < cost) || (opts.sea === undefined ? blocked(g, c, target) : barred(g, c, target))) { c.ap = Math.max(0, c.ap - 1); continue; }
    leaveWorks(g, c);
    place(g, c, target);
    c.ap = Math.max(0, c.ap - cost);
    attU[i].fate = 'advanced';
    moved++;
  }
}

/** The odds at which troops under orders to give ground do so: the odds that carry a position. */
export const YIELD_ODDS = 1.2;
/** What slipping away costs a corps in cohesion. */
const YIELD_ORG = 0.15;

/**
 * Troops the player has ordered to give ground rather than be broken on it. When a blow that would carry the
 * position is about to land they slip away to the best ground behind them, whole but shaken. A corps does this
 * once a month: caught again before its own month comes round, it stands. Fortress troops never leave their works,
 * and a capital defended by its own army is never given up this way.
 */
function giveWay(g: Game, a: Assessment, attackers: Corps[], target: Region): ReportUnit[] {
  const gone: ReportUnit[] = [];
  if (a.odds < YIELD_ODDS) return gone;
  // Nobody gives up their own capital without a fight, whatever the standing order says.
  if (target.cap && a.defenders.some((c) => c.nat === target.owner)) return gone;
  const banned = new Set(attackers.map((c) => c.reg));
  for (const c of [...a.defenders]) {
    // (A rearguard holds where it was ordered to: release 2, rearguard.ts.)
    if (!c.yields || c.gaveWay || isRearguard(g, c)) continue;
    const to = retreatTarget(g, c, target, banned);
    if (!to) continue;
    const unit = reportUnit(c);
    place(g, c, to);
    c.ap = 0;
    c.gaveWay = true;
    c.org = clamp(c.org - YIELD_ORG, 0, 1);
    unit.fate = 'yielded';
    gone.push(unit);
  }
  return gone;
}

/** The attackers take ground the defenders have given up without a fight. It is filed with the battles so the player can see what happened. */
function walkIn(g: Game, attackers: Corps[], target: Region, opts: AttackOpts, a: Assessment, gone: ReportUnit[]): BattleReport {
  const attNat = armyOf(attackers[0]);
  const AN = g.nations[attNat];
  // The offensive is counted as under way all the same: the attackers came on and the ground changed hands.
  for (const d of new Set(gone.map((u) => u.nat))) {
    const first = AN.lastFight[d] === undefined;
    if (first || g.turn - AN.lastFight[d] >= 3) AN.offStart[d] = AN.offStart[d] !== undefined && g.turn - AN.offStart[d] <= 1 ? AN.offStart[d] : g.turn;
    // The first offensive an army ever makes on a country keeps its surprise longer (the grand campaign).
    if (first && g.flags.open === 1) g.flags[`opening:${attNat}:${d}`] = AN.offStart[d];
    AN.lastFight[d] = g.turn;
    if (g.flags.open === 1) g.flags[`ff:${attNat}:${d}`] = g.turn;
  }
  const week = Math.max(...attackers.map((c) => weekOf(c, 1)));
  g.week = week;
  const attU = attackers.map(reportUnit);
  moveIn(g, attackers, target, opts, attU);
  // In the grand campaign the country is won, and the town with it only if nobody holds it.
  const got = arrive(g, target, attNat);
  if (!got) closeRound(g, target, attackers);
  // (Ruling 232: an army that gave up the field without a fight left its guard behind the walls, to be stormed or starved.)
  const lines = a.guard && !got ? a.lines.map((l) => l.label === 'The town' ? { label: 'The town', value: `${townName(target)} is held by its own guard`, effect: 'The army gave up the field without a fight, so the guard never fought beside it: the town must now be stormed or starved out' }
    : l.label === 'Forces' ? { ...l, value: l.value.replace(/, and the town guard of about [\d,]+$/, '') } : l) : a.lines;
  g.flags.battles = Math.max(g.flags.battles ?? 0, g.reports.length) + 1;
  const rep: BattleReport = {
    id: g.flags.battles, turn: g.turn, week, region: target.id, regionName: target.name, attNat, defNat: gone[0].nat,
    attackers: attU, defenders: gone, lines: [...lines, { label: 'Orders', value: 'The defenders gave ground before the attack', effect: 'No battle was fought' }],
    aPower: a.A, dPower: a.D, odds: a.odds, level: 'Close victory', taken: true, attLost: 0, defLost: 0, prisoners: 0, yielded: true,
  };
  if (!got) rep.siege = true;
  g.reports.push(rep);
  // Ground walked into in a theatre that opened late is kept out of everyone else's count of battles.
  if (lateTheatre(target)) { g.flags.lateBattles = (g.flags.lateBattles ?? 0) + 1; g.flags.lateWalks = (g.flags.lateWalks ?? 0) + 1; }
  storyWalk(g, attackers, target);
  for (const d of new Set(gone.map((u) => u.nat))) checkCollapse(g, d, attNat);
  return rep;
}

/** One side of the bill for a fight: the men each side loses in it, and the defenders' loss as a share of their men. */
export interface CostOf { ours: number; theirs: number; share: number }
/**
 * What a fight is likely to cost in men, read from its assessment before it is fought: the bill if the attack carries
 * the ground ('won') and the bill if it is thrown back ('lost'), each at the middle of the luck that gives that
 * result. Null where the result cannot happen at these odds. It counts the fight itself: prisoners, and men lost
 * in a retreat with nowhere to go, come on top of a victory. 'odds' is for a staff that does not know the defence
 * exactly and reckons at its own estimate.
 */
export interface BattleCost { chance: number; won: CostOf | null; lost: CostOf | null }
export function battleCost(g: Game, a: Assessment, attackers: Corps[], target: Region, opts: AttackOpts = {}, odds = a.odds): BattleCost {
  const town = !!opts.town;
  const ready = town ? 0 : a.prepared;
  const mobile = attackers.some((c) => KIND[c.kind].mobile);
  const weight = odds === a.odds ? a.weight : g.flags.open === 1 && !town ? Math.min(1, odds / DEFENCE.wear.fullAt) : 1;
  const all = a.defenders.reduce((s, c) => s + men(c), 0);
  const at = (R: number, win: boolean): CostOf => {
    const attFrac = attackLoss(R) * (town ? SIEGE.assault.loss : 1) * (1 + (DEFENCE.prepared.losses.attacker - 1) * ready);
    const defFrac = defenceLoss(R);
    const stood = win ? 1 : HELD_LOSS;
    const caught = win && mobile && target.terrain === DESERT.terrain && !target.fort && !unitsDecide(g) ? DESERT.retreat[R >= 2 ? 1 : 0] : 1;
    let ours = 0, theirs = 0;
    for (const c of attackers) ours += men(c) * Math.min(0.6, attFrac * tollFor(a.faced?.toll, c) * cmdLoss(g, c));
    for (const c of a.defenders) theirs += men(c) * Math.min(0.8, defFrac * (win && mobile ? 1.3 : 1) * (KIND[c.kind].mobile ? 1 : caught) * stood * cmdLoss(g, c) * (1 - (1 - DEFENCE.prepared.losses.defender) * ready)) * weight;
    return { ours: Math.round(ours), theirs: Math.round(theirs), share: all > 0 ? theirs / all : 0 };
  };
  if (a.luck) {
    // The honest win chance (release 2): the day is won on the wide run's share past the bar, and billed at the same share of today's run.
    const [d0, d1] = a.dice, [l0, l1] = a.luck;
    const bar = Math.max(0, Math.min(1, (1.2 / Math.max(1e-6, odds) - d0) / (d1 - d0)));
    const billAt = (s: number): number => Math.max(1e-6, odds * (l0 + (l1 - l0) * s));
    return { chance: chanceOn(odds, a.dice), won: bar < 1 ? at(billAt((bar + 1) / 2), true) : null, lost: bar > 0 ? at(billAt(bar / 2), false) : null };
  }
  const lo = Math.max(1e-6, odds * a.dice[0]), hi = Math.max(lo, odds * a.dice[1]);
  return {
    chance: chanceOn(odds, a.dice),
    won: hi >= 1.2 ? at((Math.max(1.2, lo) + hi) / 2, true) : null,
    lost: lo < 1.2 ? at((lo + Math.min(1.2, hi)) / 2, false) : null,
  };
}

/** The ticket's line where guns stand alone in the field (ruling 146): before the battle, and as it reads after it. */
const GUNS_OVERRUN: TicketLine = { label: 'Guns alone', value: 'The guns stand with no infantry or tanks in front of them', effect: 'An attack overruns them: every gun is taken' };
const GUNS_OVERRUN_AFTER: TicketLine = { label: 'Guns alone', value: 'The guns stood with no infantry or tanks in front of them', effect: 'They were overrun: every gun was taken' };
/** The odds guns alone are overrun at, at the least: a decisive win on the worst of the day's luck. */
const OVERRUN_ODDS = 3;

/** A Battle Ticket as it reads once the battle has been fought: what it said would happen, it now says did (the grand campaign's lines). */
function ticketAfter(lines: TicketLine[]): TicketLine[] {
  return lines.map((l) => (l === GUNS_OVERRUN ? GUNS_OVERRUN_AFTER : l.effect && (l.label === 'Dug in' || l.label === 'Weight of attack') ? {
    ...l,
    effect: l.effect.replace('Won or lost, this attack takes ', 'Won or lost, the attack took ').replace(', which leaves nothing of them', ', which left nothing of them')
      .replace('the defenders lose about ', 'the defenders lost about ').replace('would cost them, and it tires them and wears their works', 'would have cost them, and it tired them and wore their works'),
  } : l));
}

export function resolveBattle(g: Game, attackers: Corps[], target: Region, opts: AttackOpts = {}): BattleReport {
  let a = assess(g, attackers, target, opts);
  // The grand campaign: the fight may be for the town itself, or to raise the siege of a town of the attackers' own side.
  const town = !!opts.town, sieges = siegeRules(g);
  // (Release 2, rearguard.ts) The rearguards holding the field against this attack: they cost the attackers time.
  const rears = town || opts.breakout ? [] : rearguardsAt(g, target, (nat) => atWar(g, nat, armyOf(attackers[0])));
  const gone = town ? [] : giveWay(g, a, attackers, target);
  if (gone.length) {
    if (!hostileUnits(g, target, armyOf(attackers[0])).length) return walkIn(g, attackers, target, opts, a, gone);
    // Whoever stayed behind, in the works or under other orders, fights alone.
    a = assess(g, attackers, target, opts);
  }
  const defenders = a.defenders;
  // (The fortress rows, 07:03Z: each fortress crew's organisation as the battle begins, which it takes back into its works.)
  const orgAt = new Map<number, number>();
  if (!town && fortRows(g)) for (const c of defenders) if (fortRow(g, c)) orgAt.set(c.id, c.org);
  const attNat = armyOf(attackers[0]);
  const defNat = defenders[0]?.nat ?? target.ctrl;
  const AN = g.nations[attNat];
  // An enemy's capital, for the records of the corps that take it: known before the fight, since its fall may end the war with him.
  // (Where its town is held, winning the field outside it does not take the capital: storming the town does. Where
  // only its guard holds it and the guard fights in this battle (ruling 232), the field takes it.)
  const inField = !town && (a.guard ?? 0) > 0;
  const enemyCapital = target.cap && atWar(g, attNat, target.owner) && (town || inField || !sieges || !townHeld(g, target));
  const relief = sieges && !town && allied(g, attNat, target.ctrl);
  const watchMen = town || inField ? guardMen(g, target) : 0, watch = town || inField ? guardOf(g, target) : 0;
  for (const d of new Set(defenders.map((c) => c.nat))) {
    const first = AN.lastFight[d] === undefined;
    if (first || g.turn - AN.lastFight[d] >= 3) AN.offStart[d] = AN.offStart[d] !== undefined && g.turn - AN.offStart[d] <= 1 ? AN.offStart[d] : g.turn;
    // The first offensive an army ever makes on a country keeps its surprise longer (the grand campaign).
    if (first && g.flags.open === 1) g.flags[`opening:${attNat}:${d}`] = AN.offStart[d];
    AN.lastFight[d] = g.turn;
    // The staffs' quiet clock for a front counts battles in the field, not a town stormed behind the line (the grand campaign).
    if (g.flags.open === 1 && !town) g.flags[`ff:${attNat}:${d}`] = g.turn;
  }
  let week = Math.max(...attackers.map((c) => weekOf(c, 1)));
  // A later battle of the same army's month never reads an earlier week than the one before it (the grand campaign).
  if (g.flags.open === 1) for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn === g.turn; i--) if (g.reports[i].attNat === attNat) { week = Math.max(week, g.reports[i].week); break; }
  g.week = week;
  // Battles overseas and in a side theatre roll their own dice, so that a fight in the desert or the Balkans never changes the luck of the main fronts.
  const aside = target.overseas || target.theatre >= 0;
  const late = lateTheatre(target);
  const count = late ? `t${target.theatre}:${g.flags[`fought:${target.theatre}`] ?? 0}` : `${(g.flags.battles ?? 0) - (g.flags.lateBattles ?? 0)}`;
  const dice = aside ? new Rng(hashString(`land:${g.seed}:${g.turn}:${target.id}:${attNat}:${count}`)) : g.rng;
  // Luck runs wider in the streets of a town than in the open, and mostly against the attacker.
  // Against a line that is dug in and fed it runs wider still (the grand campaign): the assessment carries the run.
  // A lone gun corps attacked is overrun (ruling 146, the grand campaign): guns with nothing in front of them make
  // no stand in the field. The dice are thrown all the same, so the luck of the battles after it is what it was.
  const overrun = !!a.overrun;
  // The staff's roll is thrown on the field too: it sets the odds an army beaten on the field is billed at (b476(2)), and
  // the luck of the battles after it is what it would have been had the staff settled this one.
  // With the honest win chance (release 2) the throw is a share of the way between the wide run's ends, and the same
  // share of today's run sets what the day costs (losses, order, morale, the level of the result): the bill at given
  // odds is what it was, and only who carries the ground follows the record.
  const throwAt = a.luck ? dice.range(0, 1) : 0;
  const rolled = a.luck ? a.odds * (a.dice[0] + (a.dice[1] - a.dice[0]) * throwAt) : a.odds * dice.range(a.dice[0], a.dice[1]);
  const billed = a.luck ? a.odds * (a.luck[0] + (a.luck[1] - a.luck[0]) * throwAt) : rolled;
  const R = opts.fought ?? rolled;
  // (A field fought out is billed at its own result's share of the way, read in today's run.)
  const RL = opts.fought === undefined ? billed
    : a.luck ? a.odds * (a.luck[0] + (a.luck[1] - a.luck[0]) * clamp((opts.fought / Math.max(1e-6, a.odds) - a.dice[0]) / (a.dice[1] - a.dice[0]), 0, 1)) : opts.fought;
  const attU = attackers.map(reportUnit), defU = defenders.map(reportUnit);
  const attGens = attackers.map((c) => generalOf(g, c)), defGens = defenders.map((c) => generalOf(g, c));
  const mobile = attackers.some((c) => KIND[c.kind].mobile);
  const defFrac = defenceLoss(RL);
  // Fighting from house to house costs the attackers more.
  // Men who go at trenches pay for it, and the men in them are harder to hit (the grand campaign).
  const ready = town ? 0 : a.prepared;
  const attFrac = attackLoss(RL) * (town ? SIEGE.assault.loss : 1) * (1 + (DEFENCE.prepared.losses.attacker - 1) * ready);
  let attLost = 0, defLost = 0, prisoners = 0;
  // True if the field is won but the town holds out (the grand campaign), and if it gave itself up at the summons.
  let held = false, gates = false;
  // The share of its men each corps loses in this fight: what its morale makes of the day.
  const bled = new Map<number, number>();
  const attW = splitWeights(attackers, opts.field?.corps), defW = splitWeights(defenders, opts.field?.corps);
  // Fought on the field, each side's whole bill is held near what the field cost it (ruling 226(6)); an army that broke
  // on the field and lost the day pays the rules' bill for that result, or the field's if larger (b464).
  const attPart = (c: Corps, i: number, w = attW[i]): number => attFrac * w * tollFor(a.faced?.toll, c) * cmdLoss(g, c);
  // The bill an army beaten on the field pays is the rules' own at the staff's roll, the odds SEND THEM IN uses, never
  // the field's: the dice set the odds and the player's hand the result (the design director, b476(2)).
  const attPlain = (c: Corps): number => attackLoss(billed) * (town ? SIEGE.assault.loss : 1) * (1 + (DEFENCE.prepared.losses.attacker - 1) * ready) * tollFor(a.faced?.toll, c) * cmdLoss(g, c);
  // An army that pulled out after contact and lost the day pays that bill times its bar squared, or its field loss (b474).
  const pulled = (s: 'att' | 'def'): number | null => (opts.field?.pulled?.side === s ? Math.min(1, Math.max(0, opts.field.pulled.bar)) ** 2 : null);
  const attOut = pulled('att');
  const attK = !opts.field ? 1
    : opts.field.broke === 'att' && R < 1.2 ? pursuitScale(attackers, attackers.map((c, i) => attPart(c, i)), attackers.map((c) => attPlain(c)), 0.6, 1, opts.field.att)
      : attOut !== null && R < 1.2 ? pursuitScale(attackers, attackers.map((c, i) => attPart(c, i)), attackers.map((c) => attPlain(c)), 0.6, 1, opts.field.att, attOut)
      : fieldScale(attackers, attackers.map((c, i) => Math.min(0.6, attFrac * attW[i] * tollFor(a.faced?.toll, c) * cmdLoss(g, c))), opts.field.att, opts.field.whole);
  attackers.forEach((c, i) => {
    const before = men(c);
    c.str *= 1 - Math.min(0.6, attFrac * attW[i] * attK * tollFor(a.faced?.toll, c) * cmdLoss(g, c));
    c.org = clamp(c.org - (0.1 + 0.08 / RL) * cmdSteady(g, c) * nerve(c), 0, 1);
    c.xp = Math.min(1, c.xp + 0.05);
    c.fought = true;
    attU[i].lost = before - men(c);
    attLost += attU[i].lost;
    if (before > 0) bled.set(c.id, attU[i].lost / before);
  });
  const win = R >= 1.2;
  // Troops who hold their ground come out of the fight in better shape than troops driven off it.
  const stood = win ? 1 : HELD_LOSS;
  // Men on foot driven out of open desert by a mobile enemy do not all get away.
  const caught = win && mobile && target.terrain === DESERT.terrain && !target.fort && !unitsDecide(g) ? DESERT.retreat[RL >= 2 ? 1 : 0] : 1;
  const defPart = (c: Corps, i: number, w = defW[i]): number => defFrac * w * (win && mobile ? 1.3 : 1) * (KIND[c.kind].mobile ? 1 : caught) * stood * cmdLoss(g, c) * (1 - (1 - DEFENCE.prepared.losses.defender) * ready);
  // The same result at the staff's roll (b476(2)).
  const caughtRolled = win && mobile && target.terrain === DESERT.terrain && !target.fort && !unitsDecide(g) ? DESERT.retreat[billed >= 2 ? 1 : 0] : 1;
  const defPlain = (c: Corps): number => defenceLoss(billed) * (win && mobile ? 1.3 : 1) * (KIND[c.kind].mobile ? 1 : caughtRolled) * stood * cmdLoss(g, c) * (1 - (1 - DEFENCE.prepared.losses.defender) * ready);
  const defOut = pulled('def');
  const defK = !opts.field ? 1
    : opts.field.broke === 'def' && win ? pursuitScale(defenders, defenders.map((c, i) => defPart(c, i)), defenders.map((c) => defPlain(c)), 0.8, a.weight, opts.field.def)
      : defOut !== null && win ? pursuitScale(defenders, defenders.map((c, i) => defPart(c, i)), defenders.map((c) => defPlain(c)), 0.8, a.weight, opts.field.def, defOut)
      : fieldScale(defenders, defenders.map((c, i) => Math.min(0.8, defPart(c, i)) * a.weight), opts.field.def, opts.field.whole);
  defenders.forEach((c, i) => {
    const before = men(c);
    // (The grand campaign: a light blow costs the defence in proportion to its weight. Everywhere else the weight is 1.)
    c.str *= 1 - Math.min(0.8, defK * defFrac * defW[i] * (win && mobile ? 1.3 : 1) * (KIND[c.kind].mobile ? 1 : caught) * stood * cmdLoss(g, c) * (1 - (1 - DEFENCE.prepared.losses.defender) * ready)) * a.weight;
    c.org = clamp(c.org - clamp(0.18 + 0.14 * RL, 0.2, 0.75) * stood * cmdSteady(g, c) * nerve(c) * a.weight, 0, 1);
    c.xp = Math.min(1, c.xp + 0.04);
    c.fought = true;
    defU[i].lost = before - men(c);
    defLost += defU[i].lost;
    if (before > 0) bled.set(c.id, defU[i].lost / before);
  });
  // Works wear down under attack (the grand campaign): whoever is still in them next month has less to stand behind,
  // and troops who were attacked dig slower that month, or not at all.
  if (!town && g.flags.open === 1) {
    for (const c of defenders) {
      if (c.dug && a.wear > 0) c.dugFor = Math.max(0, worksOf(c) - a.wear);
      c.pressed = true;
      // And they meet the next blow of the month tired.
      if (a.weight > 0) c.blows = Math.min(DEFENCE.tired.most, (c.blows ?? 0) + a.weight);
    }
  }
  moraleBattle(g, attackers, defenders, RL, win, bled, enemyCapital);
  // Whoever fights an army learns what it is made of: each side has identified the other's corps.
  identify(g, defenders, attNat);
  for (const nat of new Set(defenders.map((c) => c.nat))) identify(g, attackers, nat);

  // (Who carried the ground is the throw's; how far, today's run's. Off, they are one and the same.)
  const level: VictoryLevel = win ? (RL >= 3 ? 'Heroic victory' : RL >= 2 ? 'Decisive victory' : attLost > defLost ? 'Pyrrhic victory' : 'Close victory') : RL >= 0.8 ? 'Close defeat' : 'Decisive defeat';
  const affected = new Set(defenders.map((c) => c.nat));
  // (Release 2, rearguard.ts; the design director's 19:18Z) The attackers fought the rearguards in one fight and share
  // its delay, each losing it once: in full where the rearguards held, half where they were broken. The time is lost
  // here, before the pursuit is run.
  const rearWeeks = rears.length ? chargeDelay(attackers, rearguardWeeks(rears, win)) : 0;
  if (win) {
    const banned = new Set(attackers.map((c) => c.reg));
    let intoWorks = false;
    let back: Region | null = null;
    // (Units decide, the pursuit: the roads a faster enemy has cut, and by whom; and what the chase takes on the way.)
    const blocks = new Map<number, Corps>();
    // (release 2, rearguard.ts: the pursuers who have paid a rearguard's delay in this battle, once each)
    const paidRear = new Set<number>();
    const chased = (c: Corps, to: Region, i: number): void => {
      const share = opts.breakout ? 0 : pursuitTakes(g, c, to, attackers, rears.includes(c) ? 0 : rearWeeks, paidRear);
      if (share <= 0) return;
      const before = men(c);
      c.str *= 1 - share;
      const took = before - men(c);
      defU[i].lost += took; defU[i].chased = took; prisoners += took;
    };
    const reopened = new Set<number>();
    const breakout = !opts.breakout && unitsDecide(g) && !town && !overrun ? breakOut(g, defenders, target, attNat, banned, reopened) : null;
    defenders.forEach((c, i) => {
      // (Units decide: a corps that broke out through the enemy on its road out is already on it, and is chased there.)
      if (breakout && !c.dead && c.reg !== target.id) {
        defU[i].fate = 'retreated'; if (driveOut(g)) defU[i].to = c.reg; c.ap = 0;
        chased(c, g.regions[c.reg], i);
        return;
      }
      // A corps too far gone to fall back as a formation. (By the book only a share of a corps may be in a dated fight,
      // and it is judged whole: stations.ts, rider b580. Nobody else has a share.)
      const whole = c.bookPart ? c.bookPart[1] - c.bookPart[0] * (c.bookPart[1] - c.str) : c.str;
      // (The far side: read as a share of the row's own full size, as a fortress crew's is. A small unit is a row of one
      // division that never fills past its own size (`full`), and at a strength under 0.15 it read as broken from the
      // day it was placed: 45 far rows could never fall back when beaten, the 2/15th Punjab at Kuching among them with
      // British ground next door. The design director's ruling of 07:26Z, 22 November 2026, every nation; the 0.15
      // is unchanged, and a corps of full size reads as it did.)
      const shattered = (farOn(g) ? whole / (c.full ?? 1) : whole) < 0.15;
      // The fortress rows (06:15Z (b)): a beaten fortress crew stays in its works as the town's garrison, taking no room
      // there, until it is stormed or starves, on its own stores and not the town's. Shattered, at under 15 in 100 of its
      // own size, or beaten in the storm of the town itself, it is taken as before.
      // (07:03Z: it goes in with the organisation it had when the battle began. Its men lost in the battle stay lost;
      // only the cohesion comes back, because the crew fought from its casemates and what broke was the field around it.)
      if (!town && sieges && fortRow(g, c) && allied(g, c.nat, target.ctrl) && c.str >= 0.15 * (c.full ?? 1)) {
        target.units.splice(target.units.indexOf(c), 1);
        c.org = Math.max(c.org, orgAt.get(c.id) ?? c.org);
        joinTown(g, target, c);
        defU[i].fate = 'sheltered';
        tallySiege(g, 'works');
        intoWorks = true;
        return;
      }
      // A garrison has nowhere to go.
      const cut = town || overrun || shattered || opts.breakout ? null : roadsCut(g, c, target, attNat, banned, blocks, true, reopened);
      if (cut?.size) defU[i].roadsCut = [...cut];
      const outs = cut?.size ? new Set([...banned, ...cut]) : banned;
      const to = town || overrun || shattered ? null : retreatTarget(g, c, target, outs);
      const N = g.nations[c.nat];
      // (The far side: and only with ground of its own side's to be landed on: `refugeHeld`.)
      const dest = !overrun && !shattered && N.seaSupply && target.port && target.owner !== c.nat && N.capital >= 0 && seaOpenAt(g, c.nat, target) ? refugeHeld(g, c, target) : null;
      const away = !!dest;
      // The grand campaign: beaten in the field with no way out, a corps falls back into the town if there is room,
      // and nobody leaves his own capital while it can still be held. Under ruling 232 a beaten field army never goes
      // into the town: it falls back to open country, or with nowhere to go and no ship to lift it, lays down its arms.
      if (!town && !overrun && sieges && !driveOut(g) && !shattered && (to ? target.cap && c.nat === target.owner : !away) && canShelter(g, c, target)) {
        target.units.splice(target.units.indexOf(c), 1);
        joinTown(g, target, c);
        defU[i].fate = 'sheltered';
        tallySiege(g, 'sheltered');
      }
      else if (to) { place(g, c, to); c.ap = 0; defU[i].fate = 'retreated'; if (driveOut(g)) defU[i].to = to.id; chased(c, to, i); }
      else if (away) {
        // Backs to the sea: the navy takes off what it can.
        place(g, c, dest!);
        c.str *= 0.75; c.org = 0.2; c.ap = 0;
        liftedOut(c, target, dest);
        defU[i].fate = 'evacuated';
        if (!g.flags[`evac:${g.turn}`]) { g.flags[`evac:${g.turn}`] = 1; headline(g, 'event', `${N.adj} troops are lifted off the beaches at ${target.name} under fire${dest!.cap ? '' : ` and taken to ${dest!.name}`}.`, c.nat, target.id, { arm: 'land', lost: c.nat }); }
      } else if ((back = !town && !overrun && !shattered ? fallThrough(g, c, target, outs) : null)) {
        // (Units decide: its side's ground next to the field is full, not lost. It falls back through it, or into it.)
        place(g, c, back); c.ap = 0; defU[i].fate = 'retreated'; if (driveOut(g)) defU[i].to = back.id; chased(c, back, i);
      } else {
        const left = men(c);
        // (A fortress crew is read against its own size under the fortress rows: a small one taken whole surrenders.
        // Every other corps as the far side reads it above: against its own size there, whole elsewhere.)
        const gone = fortRow(g, c) ? c.str < 0.15 * (c.full ?? 1) : shattered;
        defU[i].fate = gone ? 'destroyed' : 'surrendered';
        // (Ruling 232: with no ground of its side to go to and no ship to lift it, a beaten field army lays down its arms.)
        if (driveOut(g) && !town && !overrun && !gone) defU[i].cut = true;
        defU[i].lost += left;
        prisoners += left;
        storyFallen(g, c, gone ? 'destroyed' : 'surrendered', attNat, target.id);
        removeCorps(g, c);
      }
    });
    moveIn(g, attackers, target, opts, attU);
    if (town) {
      // The town is stormed: its guard is taken with it, and the province changes hands.
      if (watch > 0) { prisoners += watchMen; defU.push({ name: `Town guard of ${townName(target)}`, nat: target.ctrl, kind: 'inf', divs: Math.round(watch * 10) / 10, men: watchMen, lost: watchMen, fate: 'surrendered', guard: true }); }
      for (const c of attackers) c.ap = Math.max(0, c.ap - SIEGE.assault.ap);
      fall(g, target, attNat, 'storm');
    }
    // (The fortress rows: where a fortress crew went into its works, the guard goes in with it and the town holds.)
    else if (inField && !intoWorks) {
      // Ruling 232: the guard fought beside the field army and is taken with it. The town falls, and the province
      // changes hands, in this one order.
      prisoners += watchMen;
      defU.push({ name: `Town guard of ${townName(target)}`, nat: target.ctrl, kind: 'inf', divs: Math.max(0.1, Math.round(watch * 10) / 10), men: watchMen, lost: watchMen, fate: 'surrendered', guard: true });
      fall(g, target, attNat, 'field');
    }
    else {
      // A town with only its guard may open its gates to the army that has just won the field outside.
      const walls = sieges && !relief && townHeld(g, target);
      held = !arrive(g, target, attNat);
      if (held) closeRound(g, target, attackers); else gates = walls;
    }
  } else {
    // Thrown back from a prepared line, an attack has shot its bolt: those corps do nothing more this month.
    const spent = !town && a.prepared >= DEFENCE.prepared.spentAt && a.prepared > 0;
    for (const c of attackers) c.ap = spent ? 0 : Math.max(0, c.ap - (town ? SIEGE.assault.ap : 1));
    if (town) tallySiege(g, 'repulsed');
    if ((town || inField) && watch > 0) {
      // The guard has its losses too, and they are not made good while the siege lasts. (Beside the field army, ruling
      // 232, it pays as it would for a storm thrown back. Where the battle was fought on the field, the guard stood on it
      // as a block of its own (the coordinator's ruling, 22 November): it pays the share its side lost there, as the corps
      // beside it are held to (ruling 226(6)), so the after card's headline and its field line agree (b464).)
      const onField = inField && opts.field ? (opts.field.corps[guardForce(target)] !== undefined ? opts.field.def : null) : null;
      const share = onField !== null ? clamp(onField, 0, 1) : clamp((SIEGE.assault.guardHit / 100) * Math.min(1.2, RL), 0.05, 0.6);
      const lost = Math.round(watchMen * share);
      guardAfter(g, target, share);
      defLost += lost;
      defU.push({ name: `Town guard of ${townName(target)}`, nat: target.ctrl, kind: 'inf', divs: inField ? Math.max(0.1, Math.round(watch * 10) / 10) : Math.round(watch * 10) / 10, men: watchMen, lost, fate: '', guard: true });
    }
  }
  // Battles are numbered through the whole war, so a saved game carries the count on.
  g.flags.battles = Math.max(g.flags.battles ?? 0, g.reports.length) + 1;
  const rep: BattleReport = {
    id: g.flags.battles, turn: g.turn, week, region: target.id, regionName: target.name, attNat, defNat,
    attackers: attU, defenders: gone.length ? [...defU, ...gone] : defU, lines: ticketAfter(a.lines), aPower: a.A, dPower: a.D, odds: R, level, taken: win,
    attLost, defLost, prisoners,
  };
  if (town) rep.town = true;
  if (inField) rep.guarded = true;
  if (held) rep.siege = true;
  if (gates) rep.gates = true;
  if (relief) rep.relief = true;
  if (opts.sally) rep.sally = true;
  if (opts.breakout) rep.breakout = true;
  if (a.prepared > 0) rep.prepared = Math.round(a.prepared * 100) / 100;
  if (!win && !town && a.wear > 0) { rep.worn = Math.round(a.wear * 10) / 10; rep.works = Math.round(Math.max(0, a.works - a.wear) * 10) / 10; }
  // Who paid, and to what (the grand campaign): the result's line, as the attacker reads it and as the defender does.
  if (a.faced) {
    const lost = { foot: 0, tanks: 0, guns: 0 };
    for (const u of attU) lost[u.kind === 'arm' ? 'tanks' : u.kind === 'art' ? 'guns' : 'foot'] += u.lost;
    // Fought on the field, how the cost fell is read from each corps' share of its own men, as its row gives it, never
    // from men lost alone (ruling 226(6), b464(6)). "Evenly" only where no corps lost more than a quarter again the share
    // another did; a corps is named only where its share stands a quarter clear of the next and it lost the most men
    // too, so "paid most of it" holds either way. Otherwise, as with one corps on a side (the UI director, slice 3), the
    // line says nothing of how the cost fell.
    const fell = ((us: typeof attU): { most?: string; quiet: boolean } => {
      if (!opts.field) return { quiet: false };
      if (us.length < 2) return { quiet: true };
      const of = (u: (typeof us)[number]): number => (u.men > 0 ? u.lost / u.men : 0);
      const by = [...us].sort((x, y) => of(y) - of(x)), top = by[0];
      if (of(top) <= 0) return { quiet: true };
      if (of(top) <= 1.25 * of(by[by.length - 1])) return { quiet: false };
      return of(top) >= 1.25 * of(by[1]) && us.every((u) => u === top || u.lost < top.lost) ? { most: top.name, quiet: false } : { quiet: true };
    })(attU);
    const said = whoPaid(a.faced, lost, true, fell.most, fell.quiet);
    rep.paid = { att: said.text, def: whoPaid(a.faced, lost, false, fell.most, fell.quiet).text, ...(said.sign ? { sign: said.sign } : {}) };
    // The first time a line of the player's, or one he attacks, takes a third or more out of the tanks' shock, his staff says what did it.
    if (a.faced.shock !== null && a.faced.shock <= 2 / 3 && (attNat === g.human || defNat === g.human) && !g.flags['tip:antitank']) {
      g.flags['tip:antitank'] = g.turn;
      headline(g, 'info', 'Your staff on the fighting at ' + target.name + ': anti-tank guns, mines, ditches and artillery stop tanks as well as tanks do. The battle card shows how much of the tanks\' shock is left.', g.human!, target.id, { arm: 'land' });
    }
  }
  // The first time a dug-in line of the player's is broken with no guns in it, his staff says what would have helped.
  if (win && !town && a.prepared > 0 && defNat === g.human && g.flags.open === 1 && !g.flags['tip:guns'] && !defenders.some((c) => c.kind === 'art')) {
    g.flags['tip:guns'] = g.turn;
    headline(g, 'info', `Your staff on the loss of ${target.name}: the line was dug in and it still broke. Artillery standing in a line makes every corps beside it fight up to ${Math.round(GUNS.thickenMost * 100)}% stronger, and the enemy's staff thinks twice before attacking guns. An artillery corps behind each threatened sector would help the next one hold.`, defNat, target.id);
  }
  // Close air support: the groups that flew over the battle pay for it, once (air.json 'close').
  if (a.air) { bookCloseAir(g, a.air, opts.closeSorties, target.id); rep.air = { att: a.air.att.strikes, def: a.air.def.strikes }; }
  // Damaged cities (the grand campaign with its switch): a battle in the province leaves its town damaged, the more so
  // when it is fought for the town itself or at its gates, and the attackers' guns add to it.
  const fight = town ? (win ? 'storm' : 'repulsed') : inField || relief || opts.sally ? 'streets' : 'field';
  const hurt = battleHit(g, target, fight, [...attU, ...defU].reduce((s, u) => s + u.divs, 0), attU.reduce((s, u) => s + (u.kind === 'art' ? u.divs : 0), 0));
  if (hurt) rep.city = { region: hurt.region, added: hurt.added, now: hurt.now };
  // (Release 2, rearguard.ts) Held up by a rearguard, held or broken: each attacking corps lost time of its month
  // (charged before the pursuit above), and cannot press on as far behind the army that pulled back.
  if (rearWeeks > 0) {
    const who = rears.map((c) => c.name).join(' and '), lost = weeksWords(rearWeeks);
    const how = win ? 'was broken, but held the attack up' : 'held the attack up';
    rep.lines.push({ label: 'Rearguard', value: `${who} ${how}`, effect: `Each attacking corps lost ${lost} of its month` });
    rep.rearguard = rearWeeks;
    headline(g, 'info', `The rearguard at ${target.name} ${win ? 'was broken, but held the enemy up' : 'held the enemy up'}: each corps of his that attacked there lost ${lost} of its month.`, rears[0].nat, target.id);
    headline(g, 'info', `An enemy rearguard at ${target.name} held our attack up${win ? ' before it broke' : ''}: each corps that attacked there lost ${lost} of its month.`, armyOf(attackers[0]), target.id);
  }
  g.reports.push(rep);
  // (scarred ground: one more battle fought over this region, this month; a walk-in where the defenders gave ground
  // fought none, so it leaves none)
  const scar = (g.scars ??= {})[target.id];
  const muddy = groundWeather(g, target) === 'mud';
  g.scars[target.id] = { n: (scar?.n ?? 0) + 1, last: g.turn, ...(scar?.mudN ? { mudN: scar.mudN, mudLast: scar.mudLast } : {}) };
  // (mud memory: a battle fought in the thaw leaves its ruts on the field for the next battle there)
  if (muddy) { g.scars[target.id].mudN = (scar?.mudN ?? 0) + 1; g.scars[target.id].mudLast = g.turn; }
  // (old wrecks: the armoured vehicles it left burnt out on the field, by nation, for the next battle there)
  wreckBump(g, rep);
  // (captured kit, looks only: a heavy win over an army with tanks leaves some of them in the winner's hands, a few of
  // them to be fielded in its paint; a fixed throw of the battle's own numbers, so no die of the war's is spent)
  capturedKit(g, rep, target);
  // The generals' luck on the main fronts is counted off the main fronts' own battles, for the same reason.
  if (aside) g.flags.asideBattles = (g.flags.asideBattles ?? 0) + 1;
  if (late) { g.flags.lateBattles = (g.flags.lateBattles ?? 0) + 1; g.flags[`fought:${target.theatre}`] = (g.flags[`fought:${target.theatre}`] ?? 0) + 1; }
  const luck = late ? `late:${target.theatre}:${g.flags[`fought:${target.theatre}`]}` : aside ? `aside:${rep.id - (g.flags.lateBattles ?? 0)}` : `${rep.id - (g.flags.asideBattles ?? 0) - (g.flags.lateWalks ?? 0)}`;
  afterBattle(g, rep, target, a.odds, attackers, attGens, defenders, defGens, luck, evenOdds(a.dice));
  storyBattle(g, rep, target, a.odds, attackers, defenders, { sea: opts.sea, air: opts.air, capital: enemyCapital });
  if (prisoners >= 40000) {
    const who = new Set(defU.filter((u) => u.fate === 'surrendered' || u.fate === 'destroyed').map((u) => u.nat));
    const adj = who.size === 1 ? g.nations[[...who][0]].adj : bloc(g.nations[defNat]) === 'allies' ? 'Allied' : bloc(g.nations[defNat]) === 'league' ? 'League' : 'Axis';
    headline(g, 'battle', `${prisoners.toLocaleString('en-US')} ${adj} troops surrender at ${target.name}.`, defNat, target.id, { arm: 'land', won: attackers[0]?.nat, lost: defNat });
  }
  for (const d of affected) checkCollapse(g, d, attNat);
  return rep;
}

// ---- Probing the line (the grand campaign) ------------------------------------------
// One corps feels out the enemy line in the province beside it: a fight of patrols and a few battalions, not a
// battle. It names every corps that stands there, takes a month off their fieldworks, and costs the corps that
// probes a few men, some of its order and half its month. Nothing changes hands, no battle is written in the
// file, and a line is probed once a month at most (`probed:` in the flags). No computer staff probes.

/** What a probe found and what it cost. */
export interface ProbeReport {
  turn: number; region: number; nat: string; corps: number; name: string;
  /** Men the probing corps lost. */
  lost: number;
  /** The corps it found there, and how many of them the staff had not identified before. */
  found: string[]; fresh: number;
  /** The months it took off their fieldworks, and the most that is left of them. */
  wear: number; works: number;
}

export const probeRules = (g: Game): boolean => g.flags.open === 1 && !!DEFENCE.probe;

/** Why this corps cannot probe the enemy line in that province now, or null if it can. */
export function whyNoProbe(g: Game, c: Corps, target: Region): string | null {
  const P = DEFENCE.probe;
  if (!probeRules(g) || !P) return 'A line is probed only in the grand campaign.';
  if (c.dead || c.kind === 'fort' || c.kind === 'art' || c.kind === 'eng' || isTeam(c)) return 'Only a fighting corps probes: infantry, cavalry, mountain troops, motorized troops or tanks.';
  if (c.town !== undefined) return 'A garrison does not leave its town to probe.';
  if (c.leave !== undefined) return 'The corps is at home.';
  if (!g.regions[c.reg].nb.includes(target.id) || blocked(g, c, target)) return 'It cannot reach that line from where it stands.';
  if (!hostileUnits(g, target, armyOf(c)).length) return 'No enemy army stands there in the field.';
  if (g.flags[`probed:${target.id}`] === g.turn) return 'That line has been probed this month already.';
  if (c.ap < P.ap) return `A probe takes ${P.ap} of a corps' moves, and it has ${c.ap} left.`;
  return null;
}

/** How much of a probe's full effect on the works this corps has the weight for, 0 to 1: a probe in strength takes men. */
export const probeWeight = (c: Corps): number => { const w = DEFENCE.probe?.weight ?? 0; return w > 0 ? Math.min(1, men(c) / w) : 1; };

/** The men a probe would cost this corps. */
export const probeCost = (g: Game, c: Corps): number => Math.round(men(c) * Math.min(0.6, (DEFENCE.probe?.loss ?? 0) * cmdLoss(g, c)));

/** One corps probes the enemy line in the province beside it. Null if it cannot. */
export function probeLine(g: Game, c: Corps, target: Region): ProbeReport | null {
  if (whyNoProbe(g, c, target) !== null) return null;
  const P = DEFENCE.probe!, nat = armyOf(c);
  const defenders = hostileUnits(g, target, nat);
  const lost = probeCost(g, c);
  // The works are worn by the weight that comes at them: reckoned before the probe's own losses.
  const reach = P.wear * probeWeight(c);
  const fresh = defenders.filter((d) => corpsKnown(g, d, nat).level !== 'exact').length;
  c.str *= 1 - Math.min(0.6, P.loss * cmdLoss(g, c));
  c.org = clamp(c.org - P.org, 0, 1);
  c.ap = Math.max(0, c.ap - P.ap);
  c.fought = true;
  let works = 0, left = 0;
  for (const d of defenders) {
    if (d.dug) { const w = worksOf(d); works = Math.max(works, w); d.dugFor = Math.max(0, w - reach); left = Math.max(left, worksOf(d)); }
    d.pressed = true;
  }
  const wear = Math.round(Math.min(reach, works) * 10) / 10;
  // Whoever is felt out is known, and so is the corps that came to feel.
  identify(g, defenders, nat);
  for (const d of new Set(defenders.map((x) => x.nat))) identify(g, [c], d);
  g.flags[`probed:${target.id}`] = g.turn;
  const rep: ProbeReport = { turn: g.turn, region: target.id, nat, corps: c.id, name: c.name, lost, found: defenders.map((d) => d.name), fresh, wear, works: Math.round(left * 10) / 10 };
  const months = (m: number): string => (m === 1 ? 'a month' : `${m} months`);
  const worksNews = works <= 0 ? 'They are not dug in.' : `Their fieldworks are ${months(wear)} the worse for it${rep.works < 0.05 ? ', and nothing is left of them' : `: ${months(rep.works)} of work ${rep.works === 1 ? 'is' : 'are'} left in them`}.`;
  if (nat === g.human) headline(g, 'battle', `${c.name} probes the line at ${target.name} and finds ${listAnd(rep.found)}. ${worksNews} It cost ${lost.toLocaleString('en-US')} men.`, nat, target.id, { arm: 'land' });
  const mine = defenders.find((d) => d.nat === g.human);
  if (mine) headline(g, 'battle', `${g.nations[nat].adj} troops probe your line at ${target.name} in strength. They know now who holds it${works > 0 ? `, and your fieldworks there are ${months(wear)} the worse` : ''}.`, mine.nat, target.id, { arm: 'land' });
  return rep;
}

// ---- Moving, taking ground, and falling ---------------------------------------------

export function capture(g: Game, r: Region, nat: string): void {
  const old = r.ctrl;
  if (old === nat) return;
  const owner = g.nations[r.owner];
  r.ctrl = owner.alive && allied(g, nat, r.owner) ? r.owner : nat;
  r.since = g.turn;
  // And in whose turn of the month it fell, so that the alarm can tell a fall the player has seen from one since he
  // last ended his month (danger.ts homeAlarms; the design director's ruling of 17 November, by the order of play).
  // Read by nothing that moves a die.
  g.flags[`fellP:${r.id}`] = g.phase;
  moraleCapital(g, r, old);
  r.prev = old;
  g.supplyDirty = true;
  // Works standing empty go with the ground: whoever takes it from their owner finds nothing he can use.
  if (g.flags[`fw:${r.id}`] !== undefined) { const left = groundWorks(g, r.id); if (!left || !allied(g, r.ctrl, left.nat)) clearWorks(g, r.id); }
  if (groundRules(g)) {
    // The minefields are lifted by whoever takes the ground, and the side driven off it blows the bridges behind it.
    liftMines(g, r);
    if (atWar(g, nat, old)) { bridgesOnCapture(g, r, nat); railOnCapture(g, r, nat, old); railOnScorch(g, r, nat, old); }
  }
  if (r.cap && r.owner === old) headline(g, 'fall', `${r.name} falls to ${g.nations[nat].adj} forces.`, old, r.id, { arm: 'land', won: nat, lost: old });
  // Overseas the prizes are the ports: whoever holds them feeds the army that fights there.
  else if (r.overseas && r.port && r.vp > 0 && atWar(g, nat, old)) headline(g, 'battle', `${r.name} falls to ${g.nations[nat].adj} forces.`, old, r.id, { arm: 'land', won: nat, lost: old });
  // (Brian, 18 November: ground a great power owns, taken from it in war, gives a smaller taker its capital-ship plans.)
  plansOnCapture(g, r, nat, old);
  checkCollapse(g, old, nat);
  // The grand campaign: ground of France's taken back from Vichy goes to a freed France. (A fallen country whose
  // capital its conqueror's enemies now hold comes back at the month's end, ruling 229 part 2: turn.ts.)
  retaken(g, r);
  // A third country's corps at peace with the new holder goes home (ruling b341(b), the grand campaign).
  if (g.flags.open === 1) goHome(g, r);
}

/**
 * The corps of a third country standing on ground that is not its own, at peace with `who`, by nation (rulings b341
 * and b343): they take no part in a fight there, and go home if the province changes hands.
 */
export function standersAt(g: Game, r: Region, who: string): [string, Corps[]][] {
  const by = new Map<string, Corps[]>();
  for (const c of r.units) {
    if (c.dead || c.kind === 'fort' || c.nat === r.ctrl || c.nat === who || allied(g, c.nat, who) || atWar(g, c.nat, who)) continue;
    (by.get(c.nat) ?? by.set(c.nat, []).get(c.nat)!).push(c);
  }
  return [...by];
}

/** "A" or "An" before a nation's adjective. */
const anA = (adj: string): string => (/^[AEIO]|^U(?![rkgn])/.test(adj) ? 'An' : 'A');

/**
 * The one sentence the cards and the hover box read for a third country's corps at peace with the player, one for
 * each nation (the UI director's words, log b343). The grand campaign only, where the rule stands.
 */
export function standersLine(g: Game, r: Region, who: string): string[] {
  if (g.flags.open !== 1) return [];
  return standersAt(g, r, who).map(([nat, cs]) => {
    const adj = g.nations[nat].adj;
    return cs.length === 1
      ? `${anA(adj)} ${adj} corps stands at ${r.name} too, at peace with you: it takes no part in the fight, and goes home if the province changes hands.`
      : `${cs.length} ${adj} corps stand at ${r.name} too, at peace with you: they take no part in the fight, and go home if the province changes hands.`;
  });
}

/**
 * Ground that has changed hands: every corps on it of a country at peace with the new holder, and not its ally, goes
 * home to its own nearest ground, its month spent, with one plain line of news told to the player whoever took it
 * (rulings b341(b) and b343). A corps whose country holds no ground with room stays where it is.
 */
export function goHome(g: Game, r: Region): void {
  const by = new Map<string, Corps[]>();
  for (const c of r.units) {
    if (c.dead || c.kind === 'fort' || c.nat === r.ctrl || allied(g, c.nat, r.ctrl) || atWar(g, c.nat, r.ctrl)) continue;
    (by.get(c.nat) ?? by.set(c.nat, []).get(c.nat)!).push(c);
  }
  for (const [nat, cs] of by) {
    let went = 0;
    for (const c of cs) {
      const to = ownNearest(g, c);
      if (!to) continue;
      place(g, c, to);
      c.ap = 0;
      went++;
    }
    if (!went) continue;
    const adj = g.nations[nat].adj;
    headline(g, 'info', went === 1 ? `The ${adj} corps at ${r.name} goes home: the province has changed hands.` : `The ${went} ${adj} corps at ${r.name} go home: the province has changed hands.`, g.human ?? nat, r.id);
  }
}

/** A corps' own nearest ground with room for it: by land where there is a way, else the nearest across the sea. */
function ownNearest(g: Game, c: Corps): Region | null {
  const mine = (r: Region): boolean => r.id !== c.reg && r.ctrl === c.nat && hasRoom(g, r, c);
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    const r = g.regions[q[i]];
    if (mine(r)) return r;
    for (const j of r.nb) if (!seen[j]) { seen[j] = 1; q.push(j); }
  }
  const from = g.regions[c.reg];
  let best: Region | null = null, bd = Infinity;
  for (const r of g.regions) if (mine(r)) { const d = chartDist2(g, r, from); if (d < bd) { bd = d; best = r; } }
  return best;
}

/**
 * Room for this corps in a region. Engineers work among the other troops: they are not counted against the
 * corps a province holds, and it takes two engineer corps at most. (How many corps a province holds is `roomAt`'s
 * to say: five, or on the whole-world chart an island's own room. islands.ts.)
 */
export function hasRoom(g: Game, to: Region, c: Corps): boolean {
  const here = placed(g, to, c.nat);
  const eng = here.filter((u) => u.kind === 'eng').length;
  return c.kind === 'eng' ? eng < engRoom(g, to) : here.length - eng < roomAt(g, to, c.nat);
}

/**
 * How many more corps of a side an island of the Pacific takes ashore: its room less the side's corps already on it,
 * engineers apart as `hasRoom` counts them, and never less than none (rulings 173(b) and 179(s): a landing force for
 * an atoll is one corps). Infinity for ground that is no island of the table, and with the far side off, where a
 * landing is counted as it always was.
 */
export function shoreRoom(g: Game, to: Region, nat: string): number {
  return islandOf(g, to) ? Math.max(0, roomAt(g, to, nat) - placed(g, to, nat).filter((u) => u.kind !== 'eng').length) : Infinity;
}

/**
 * Why a corps cannot march into a province next door for want of room there, in the player's words, or null where
 * room is not what stops it. Only an island of the Pacific says so ("Hong Kong has room for one corps."): other
 * ground that is full says nothing here, as before, and the two corps may change places.
 */
export function whyNoRoom(g: Game, c: Corps, to: Region): string | null {
  return to.id !== c.reg && allied(g, c.nat, to.ctrl) && !hostileUnits(g, to, c.nat).length && !hasRoom(g, to, c) ? roomRefusal(g, to, [c]) ?? roomSays(g, to) : null;
}

/** Moves one corps one region. Returns false if the move is not allowed. */
export function moveCorps(g: Game, c: Corps, to: Region): boolean {
  const from = g.regions[c.reg];
  if (!from.nb.includes(to.id) || c.kind === 'fort' || blocked(g, c, to)) return false;
  const cost = moveCost(g, c, to);
  if (c.ap < Math.min(cost, 4) || c.ap <= 0) return false;
  const friendly = allied(g, c.nat, to.ctrl);
  if (!friendly && !atWar(g, c.nat, to.ctrl)) return false;
  if (hostileUnits(g, to, c.nat).length) return false;
  // Guns and engineers follow onto enemy-held ground, they never lead (the grand campaign, ruling 211(a)).
  if (!friendly && gunsRule(g) && !fights(c) && !gunsMayFollow(g, c, to)) return false;
  if (!hasRoom(g, to, c)) return false;
  g.week = weekOf(c, cost);
  leaveWorks(g, c);
  place(g, c, to);
  c.ap = Math.max(0, c.ap - cost);
  // A relief in place: works standing empty on ground its side holds are its own from the moment it marches in.
  if (friendly) takeWorks(g, c, to);
  // Ground a corps walks into is taken for the army it marches with: the province too, unless its town is held
  // (the grand campaign), in which case the army has only the country round it and the town is under siege.
  if (!friendly && !arrive(g, to, armyOf(c))) c.ap = 0; // the town holds: closing round it is the rest of the month's work
  return true;
}

/**
 * Why these two corps cannot change places in one order, or null if they can (the grand campaign, Version 65).
 * Two corps of one side in neighbouring provinces their side holds march at the same moment, so it does not matter
 * that both provinces are full: a worn corps in the line and a fresh one behind it are exchanged, which a march
 * one after the other cannot do on a full front.
 */
export function whyNoSwap(g: Game, a: Corps, b: Corps): string | null {
  if (g.flags.open !== 1) return 'Two corps change places in one order only in the grand campaign.';
  if (a === b || a.dead || b.dead) return 'It takes two corps.';
  for (const c of [a, b]) if (c.kind === 'fort' || c.town !== undefined || c.leave !== undefined) return `${c.name} is not in the field.`;
  if (!allied(g, a.nat, b.nat)) return 'They are not of one side.';
  const ra = g.regions[a.reg], rb = g.regions[b.reg];
  if (ra === rb || !ra.nb.includes(rb.id)) return 'They do not stand in neighbouring provinces.';
  if (!allied(g, a.nat, ra.ctrl) || !allied(g, a.nat, rb.ctrl) || hostileUnits(g, ra, a.nat).length || hostileUnits(g, rb, a.nat).length) return 'Both provinces must be in your side\'s hands, with no enemy in them.';
  if (blocked(g, a, rb) || blocked(g, b, ra)) return 'The way between them is closed to one of them.';
  if ((a.kind === 'eng') !== (b.kind === 'eng')) return 'Engineers change places only with engineers.';
  for (const [c, to] of [[a, rb], [b, ra]] as [Corps, Region][]) { const cost = moveCost(g, c, to); if (c.ap < Math.min(cost, 4) || c.ap <= 0) return `${c.name} has not the moves left for the march.`; }
  return null;
}

/** Two corps change places in one order: each leaves its works behind it and takes over what the other left. False if they cannot. */
export function swapCorps(g: Game, a: Corps, b: Corps): boolean {
  if (whyNoSwap(g, a, b) !== null) return false;
  const ra = g.regions[a.reg], rb = g.regions[b.reg];
  const ca = moveCost(g, a, rb), cb = moveCost(g, b, ra);
  g.week = weekOf(a, ca);
  // The works go from hand to hand: the corps that comes in has first claim on what the one going out stood in,
  // before anyone else there. Works nobody can man are left as a march would leave them.
  const wa = handover(g) && a.dug ? Math.round(worksOf(a) * 100) / 100 : 0, wb = handover(g) && b.dug ? Math.round(worksOf(b) * 100) / 100 : 0;
  if (wa > 0 && b.yields) leaveWorks(g, a);
  if (wb > 0 && a.yields) leaveWorks(g, b);
  place(g, a, rb); place(g, b, ra);
  a.ap = Math.max(0, a.ap - ca); b.ap = Math.max(0, b.ap - cb);
  if (wa > 0 && mansWorks(b)) { b.dug = true; b.dugFor = wa; b.relief = true; }
  if (wb > 0 && mansWorks(a)) { a.dug = true; a.dugFor = wb; a.relief = true; }
  takeWorks(g, a, rb); takeWorks(g, b, ra);
  return true;
}

/**
 * Where a corps could go by rail this month: any friendly region joined to its own by friendly track. No railway
 * crosses the desert. In the grand campaign the trains keep to the lines on the map, and none runs into, out of
 * or through a province whose line is cut; only engineers can be carried to the break itself.
 */
export function railReach(g: Game, c: Corps): Set<number> {
  return railWalk(g, c).out;
}

/**
 * The walk railReach makes: where the corps can get down (`out`), every province its train can run through, its own
 * first (`through`), and the cut provinces a works train carries engineers up to (`ends`).
 */
function railWalk(g: Game, c: Corps): { out: Set<number>; through: number[]; ends: number[] } {
  const out = new Set<number>();
  // No train leaves a province under siege, unless the army round the town holds a great junction outside it (prizes.ts).
  if (c.kind === 'fort' || c.ap < 4 || sandNoRail(g.regions[c.reg]) || (besieged(g, g.regions[c.reg]) && !junctionOpenTo(g, c.nat, g.regions[c.reg]))) return { out, through: [], ends: [] };
  const net = groundRules(g);
  if (net && railCut(g, c.reg)) return { out, through: [], ends: [] };
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg], ends: number[] = [];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    for (const j of net ? railsAt(q[i]) : g.regions[q[i]].nb) {
      const r = g.regions[j];
      if (seen[j] || sandNoRail(r)) continue;
      if ((!allied(g, c.nat, r.ctrl) || besieged(g, r)) && !junctionOpenTo(g, c.nat, r)) continue;
      if (downBetween(g, q[i], j)) continue; // no train crosses a river whose bridges are down
      if (net && railCut(g, j)) {
        // Engineers alone ride the works train to the break itself. Nothing goes on through it.
        if (c.kind === 'eng') { seen[j] = 1; out.add(j); ends.push(j); }
        continue;
      }
      seen[j] = 1; q.push(j);
      if (!barred(g, c, r)) out.add(j); // the trains run through the high passes, but tanks are not unloaded there
    }
  }
  return { out, through: q, ends };
}

/**
 * The lit railway (the art director's railway brief, sections 3 and 11): every working track a train could carry the
 * army in hand along this month, from its own province outward, track inside its march reach among it. Each is a
 * pair of provinces a line joins, the lower id first. With engineers in hand the light runs up to a cut they can be
 * carried to. Empty where the army cannot ride, and where the trains do not keep to lines on the map.
 */
export function litTracks(g: Game, corps: Corps[]): [number, number][] {
  const ride = corps.filter((c) => !c.dead);
  if (!ride.length || !groundRules(g) || whyNoTrainFor(g, ride)) return [];
  type Track = [number, number];
  let lit: Map<number, Track> | null = null;
  for (const c of ride) {
    const w = railWalk(g, c), through = new Set(w.through), ends = new Set(w.ends);
    const mine = new Map<number, Track>(), was: Map<number, Track> | null = lit;
    for (const a of w.through) for (const b of railsAt(a)) {
      if (!(through.has(b) ? a < b : ends.has(b)) || downBetween(g, a, b)) continue;
      const lo = Math.min(a, b), hi = Math.max(a, b);
      mine.set(lo * g.regions.length + hi, [lo, hi]);
    }
    lit = was ? new Map([...was].filter((e: [number, Track]) => mine.has(e[0]))) : mine;
  }
  return [...(lit?.values() ?? [])];
}

/** Rail move: anywhere through friendly territory, for the whole month. */
export function railMove(g: Game, c: Corps, to: Region): boolean {
  const friendly = (allied(g, c.nat, to.ctrl) && !besieged(g, to)) || junctionOpenTo(g, c.nat, to);
  if (c.kind === 'fort' || c.ap < 4 || !friendly || !hasRoom(g, to, c) || barred(g, c, to) || c.town) return false;
  if (to.id !== c.reg && !railReach(g, c).has(to.id)) return false;
  if (to.id !== c.reg) leaveWorks(g, c);
  place(g, c, to);
  c.ap = 0;
  takeWorks(g, c, to);
  return true;
}

/** Why a corps cannot go by train: the kind of reason, the sentence, and the province the trouble is at, where there is one. */
export interface NoTrain {
  code: 'fort' | 'town' | 'spent' | 'desert' | 'siege' | 'cut' | 'noline' | 'foe' | 'steep' | 'room' | 'broken' | 'unjoined';
  line: string;
  /**
   * Set where the reason is a break he can act on (a cut, a siege, bridges down): the opening of `line` that takes
   * the box's emphasis. Both builds then print the reason first and the kept march's line after it, dim. Every other
   * reason is an aside: it follows the march's line in plain type (the art director's railway brief, section 8).
   */
  strong?: string;
  at?: number;
}

/** A reason that is news he can act on: its opening takes the emphasis. */
const news = (code: NoTrain['code'], strong: string, rest: string, at: number): NoTrain => ({ code, line: `${strong} ${rest}`, strong, at });

/** A cut on friendly ground, said in passing: " (bombed, June 1941)", or the month alone where no cause was kept. */
function cutAside(g: Game, r: number): string {
  const why = cutCause(g, r), on = cutOn(g, r);
  return ` (${why ? `${CAUSE_WORDS[why]}, ` : 'since '}${turnToDate(on ?? g.turn).label})`;
}

/**
 * Why no train will take a corps, or take it to one province (the design director's ruling 112(b)): one function for
 * the browser and the 3D table, so that the two never say different things. With no province named it answers for
 * the corps alone. Null where the train runs. Where the line does not reach, it names the place the line is broken:
 * the first break on the way with the fewest. Enemy ground is named as enemy ground and never as cut, so nothing here
 * tells the player what the fog hides.
 */
export function whyNoTrain(g: Game, c: Corps, to?: number): NoTrain | null {
  const from = g.regions[c.reg], net = groundRules(g);
  const open = (r: Region): boolean => (allied(g, c.nat, r.ctrl) && !besieged(g, r)) || junctionOpenTo(g, c.nat, r);
  const mended = (r: number): string => (hastyLeft(g, r) > 0 ? `the railwaymen have it running, in ${turnToDate(g.turn + hastyLeft(g, r)).label}, or engineers relay it sooner` : 'engineers relay the track');
  if (c.kind === 'fort') return { code: 'fort', line: 'Fortress troops do not go by train.' };
  // (A garrison in a town nobody besieges is not shut in: it takes the field first, and rides from the country outside.)
  if (c.town) return { code: 'town', line: besieged(g, from) ? `${c.name} is shut in the town at ${from.name}. No train leaves a siege.` : 'A garrison does not take the train: TAKE THE FIELD first.', at: from.id };
  // The test is four weeks of movement in hand. A corps that has moved, fought or worked has spent part of its month;
  // one slowed for want of fuel opens its month with three and has spent nothing (ruling 118(c); the art director's
  // lines). A fast corps with five may march a week and still ride.
  if (c.ap < 4) return {
    code: 'spent',
    line: c.ap < c.apMax ? `No train for ${c.name} this month: it has spent part of its month, and the railway takes a whole one.`
      : fuelSlows(g, c) ? `No train for ${c.name} this month: short of fuel, it has less than the whole month a train takes.`
      : `No train for ${c.name} this month: the railway takes a whole month of movement, and it has less.`,
  };
  if (sandNoRail(from)) return { code: 'desert', line: 'No railway crosses the desert.', at: from.id };
  if (besieged(g, from) && !junctionOpenTo(g, c.nat, from)) return news('siege', `${from.name} is under siege.`, 'No train leaves it.', from.id);
  if (net && !onNet(from.id)) return { code: 'noline', line: `No railway runs through ${from.name}: ${c.name} goes by road.`, at: from.id };
  if (net && railCut(g, from.id)) return news('cut', `The railway at ${from.name} is cut${cutAside(g, from.id)}.`, `No train leaves until ${mended(from.id)}.`, from.id);
  if (to === undefined || to === c.reg || !g.regions[to]) return null;
  const dest = g.regions[to];
  if (sandNoRail(dest)) return { code: 'desert', line: 'No railway crosses the desert.', at: to };
  if (!allied(g, c.nat, dest.ctrl) && !junctionOpenTo(g, c.nat, dest)) return { code: 'foe', line: `${dest.name} is not in friendly hands. Trains run through friendly ground only.`, at: to };
  if (besieged(g, dest) && !junctionOpenTo(g, c.nat, dest)) return news('siege', `${dest.name} is under siege.`, 'No train runs into it.', to);
  if (net && !onNet(to)) return { code: 'noline', line: `No railway runs to ${dest.name}.`, at: to };
  if (barred(g, c, dest)) return { code: 'steep', line: whyBarred(g, c, dest)!, at: to };
  if (railReach(g, c).has(to)) return hasRoom(g, dest, c) ? null : { code: 'room', line: roomRefusal(g, dest, [c]) ?? roomSays(g, dest) ?? `${dest.name} has no room for another corps.`, at: to };
  if (net && railCut(g, to)) return news('cut', `The railway at ${dest.name} is cut${cutAside(g, to)}.`, `No train runs into it until ${mended(to)}.`, to);
  // Where the line is broken: the way there over the map's lines that meets the fewest breaks, and the first of them.
  // A break on his own side's ground (a cut, a siege, bridges down) is a smaller thing than ground he does not hold,
  // so he is told of the cut on his own line before he is told of a way round through another country.
  const INF = 1e9;
  const cost = new Int32Array(g.regions.length).fill(INF), prev = new Int32Array(g.regions.length).fill(-1);
  const weigh = (a: number, b: number): number => { const r = g.regions[b]; return !allied(g, c.nat, r.ctrl) && !junctionOpenTo(g, c.nat, r) ? 4 : !open(r) || (net && railCut(g, b)) || downBetween(g, a, b) ? 1 : 0; };
  cost[c.reg] = 0;
  const dq: number[] = [c.reg];
  while (dq.length) {
    const a = dq.shift()!;
    for (const b of net ? railsAt(a) : g.regions[a].nb) {
      if (sandNoRail(g.regions[b])) continue;
      const w = weigh(a, b);
      if (cost[a] + w >= cost[b]) continue;
      cost[b] = cost[a] + w; prev[b] = a;
      dq.push(b);
    }
  }
  if (cost[to] >= INF) return { code: 'unjoined', line: `No railway joins ${from.name} to ${dest.name}.` };
  const way: number[] = [];
  for (let at = to; at >= 0 && at !== c.reg; at = prev[at]) way.unshift(at);
  let a = c.reg;
  for (const b of way) {
    const r = g.regions[b];
    if (!allied(g, c.nat, r.ctrl) && !junctionOpenTo(g, c.nat, r)) return { code: 'broken', line: `The line to ${dest.name} runs through ${r.name}, which is not in friendly hands.`, at: b };
    if (besieged(g, r) && !junctionOpenTo(g, c.nat, r)) return news('broken', `The line to ${dest.name} is broken at ${r.name}:`, 'the town is under siege, and no train runs through.', b);
    if (net && railCut(g, b)) return news('broken', `The line to ${dest.name} is broken at ${r.name}:`, `the railway there is cut${cutAside(g, b)}.`, b);
    const x = riverBetween(g, a, b);
    if (x && bridgeDown(g, x)) return news('broken', `The line to ${dest.name} is broken between ${g.regions[a].name} and ${r.name}:`, `the bridges over the ${riverName(x)} are down.`, b);
    a = b;
  }
  return { code: 'unjoined', line: `No train can reach ${dest.name} from ${from.name} this month.` };
}

/** Room in a region for all of these corps at once: engineers apart, as `hasRoom` counts them. */
export function roomForAll(g: Game, to: Region, corps: Corps[]): boolean {
  if (!corps.length) return true;
  const here = placed(g, to, corps[0].nat).filter((u) => !corps.includes(u));
  const eng = here.filter((u) => u.kind === 'eng').length, riding = corps.filter((c) => c.kind === 'eng').length;
  // (Engineers apart: two, or on an island of the Pacific the lesser of two and its room. Engineers already there over
  // that, the record's or a beaten army's, bar more engineers and nobody else. islands.ts engRoom.)
  return (riding === 0 ? eng <= Math.max(2, engRoom(g, to)) : eng + riding <= engRoom(g, to)) && here.length - eng + (corps.length - riding) <= roomAt(g, to, corps[0].nat);
}

/**
 * How an order's words name the corps it moves (the UI director's "one voice", 84.2.6): a lone corps by its name;
 * every corps of its nation in the field of the province where the first stands as its army ("the American army",
 * or "Patton's army" where a general commands one of them, the senior first); a part of it as "2 corps of the
 * American army". `many` is for a plural verb.
 */
export function forceWords(g: Game, corps: Corps[]): { who: string; many: boolean } {
  if (corps.length === 1) return { who: corps[0].name, many: false };
  const lead = corps[0], adj = g.nations[lead.nat]?.adj ?? '';
  const all = g.regions[lead.reg].units.filter((c) => !c.dead && c.kind !== 'fort' && c.nat === lead.nat);
  const gens = corps.map((c) => generalOf(g, c)).filter((x): x is NonNullable<typeof x> => !!x && x.status === 'active').sort((a, b) => b.level - a.level || a.id - b.id);
  const army = gens.length ? `${gens[0].name.split(' ').pop()}'s army` : `the ${adj} army`;
  return all.every((c) => corps.includes(c)) ? { who: army, many: false } : { who: `${corps.length} corps of ${gens.length ? army : `the ${adj} army`}`, many: true };
}

/**
 * Why no train will take an army in hand to a province (the design director's ruling 116): an army is never split by
 * surprise, so if one corps cannot ride, nobody rides. Null where every corps can and there is room for them all.
 * Where some can and some cannot, the sentence names the corps that cannot, so that the player can leave them behind
 * himself and send the rest.
 */
export function whyNoTrainFor(g: Game, corps: Corps[], to?: number): NoTrain | null {
  const whys = corps.map((c) => whyNoTrain(g, c, to));
  const first = whys.findIndex(Boolean);
  if (first < 0) {
    const dest = to === undefined ? undefined : g.regions[to];
    return dest && !roomForAll(g, dest, corps) ? { code: 'room', line: roomRefusal(g, dest, corps) ?? roomSays(g, dest) ?? `${dest.name} has no room for ${corps.length === 1 ? 'another corps' : `${corps.length} more corps`}.`, at: dest.id } : null;
  }
  const no = whys[first]!;
  if (whys.every(Boolean)) {
    // (Where none can ride for want of a whole month, it is said once of the army: "No train for the American army
    // this month: it has spent part of its month, ...", the one voice of 84.2.6.)
    if (corps.length < 2 || !whys.every((w) => w!.code === 'spent' && w!.line.startsWith('No train for '))) return no;
    const { who, many } = forceWords(g, corps);
    const rest = no.line.slice(no.line.indexOf(' this month:'));
    return { ...no, line: `No train for ${who}${many ? rest.replace(': it has spent part of its month', ': they have spent part of their month').replace(': short of fuel, it has', ': short of fuel, they have').replace(', and it has less', ', and they have less') : rest}` };
  }
  const names = corps.filter((_, i) => whys[i]).map((c) => c.name);
  const who = names.length === 1 ? names[0] : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
  // (Its own reason follows as an aside: the sentence that leads is about the army.)
  return { code: no.code, line: `${who} cannot go by rail, so nobody rides. ${no.line}`, ...(no.at !== undefined ? { at: no.at } : {}) };
}

/**
 * The words under the pointer on a province an army in hand can ride to (the art director's railway brief, section
 * 8, as ruled at 15:45 UTC on Version 82.2's pictures): the one line, in the rail's own blue, naming the press that
 * sends it (the build's own: 'Right-click'); and engineers pointed at a cut they can reach are told what they can do
 * there, and that it is the works train that takes them (Version 82.4, the art director's mend).
 */
export function rideLines(g: Game, corps: Corps[], to: number, press: string): { line: string; relay: string } {
  const works = corps.length > 0 && corps.every((c) => c.kind === 'eng') && railCut(g, to);
  return {
    line: `${press}: by ${works ? 'works train' : 'rail'} to ${g.regions[to].name}, there this month. It takes the whole month.`,
    relay: works ? 'Engineers can relay the track here.' : '',
  };
}

/** Where an army in hand could go by rail this month: what every corps of it can reach, less where it stands and where there is no room for all. */
export function railReachFor(g: Game, corps: Corps[]): Set<number> {
  const out = new Set<number>();
  if (!corps.length) return out;
  let common: number[] | null = null;
  for (const c of corps) {
    const m = railReach(g, c);
    common = common ? common.filter((id) => m.has(id)) : [...m];
    if (!common.length) return out;
  }
  for (const id of common ?? []) if (corps.every((c) => c.reg !== id) && roomForAll(g, g.regions[id], corps)) out.add(id);
  return out;
}

/**
 * Where the army in hand can go by train and not by road this month, the map's quieter tint (ruling 110(b)) and
 * FIRST COMMAND's lesson 8: ground every corps of it can ride to, with room for all, that none of them can march to
 * (`goes`). Empty unless every corps has its whole month, since a train takes all of it.
 */
export function trainReachFor(g: Game, corps: Corps[], goes: { has(id: number): boolean }): Set<number> {
  const out = new Set<number>();
  if (!corps.length || corps.some((c) => c.ap < 4)) return out;
  for (const id of railReachFor(g, corps)) if (!goes.has(id)) out.add(id);
  return out;
}

/**
 * The break nearest an army on its own side's lines that stops its trains (a cut, a siege, bridges down), in the
 * words whyNoTrain has for it, or null where nothing of the kind bounds its reach.
 */
export function nearestBreak(g: Game, corps: Corps[]): NoTrain | null {
  const c = corps[0];
  if (!c) return null;
  const net = groundRules(g), reach = railReach(g, c), seen = new Set<number>([c.reg]), q = [c.reg];
  for (let i = 0; i < q.length; i++) {
    for (const j of net ? railsAt(q[i]) : g.regions[q[i]].nb) {
      if (seen.has(j)) continue;
      seen.add(j);
      const r = g.regions[j];
      if (sandNoRail(r)) continue;
      const open = !((!allied(g, c.nat, r.ctrl) || besieged(g, r)) && !junctionOpenTo(g, c.nat, r)) && !downBetween(g, q[i], j) && !(net && railCut(g, j));
      if (open) { q.push(j); continue; }
      if (reach.has(j) || !allied(g, c.nat, r.ctrl)) continue;
      const why = whyNoTrain(g, c, j);
      if (why?.strong) return why;
    }
  }
  return null;
}

/**
 * The one line on the army's panel that says what the trains will do for the army in hand this month (ruling
 * 118(b); the art director's railway brief, section 2a), where the switch used to stand. `beyond` is how many
 * places a train would take it to that its march does not reach, which the build knows from the reaches it draws:
 * the line counts them, so he looks for them. `where` names the build's own sign for such a place, of one and of
 * several ({ one: 'the station dot', many: 'a station dot' }; { one: 'the blue one', many: 'a blue one' }), and
 * `press` its press. Null where nothing in hand could ever ride (no field corps). Given the places themselves in
 * place of their count (province ids, the nearest by rail first, as `railReachFor` hands them), the line names them
 * where there are three or fewer (Version 82.4, ruling 123(3)): "By rail this month to Tyrol, Styria and Slovakia:
 * right-click a blue one. It takes the whole month."; of one, "right-click it".
 */
export interface RailSaid {
  can: boolean;
  /** The opening that takes the rail's own colour, where it can ride: 'By rail this month to 3 more places:'. */
  strong: string;
  line: string;
}
export function railSays(g: Game, corps: Corps[], beyond: number | number[], press: string, where: { one: string; many: string }): RailSaid | null {
  const ride = corps.filter((c) => !c.dead);
  if (!ride.length) return null;
  const no = whyNoTrainFor(g, ride);
  if (no) return { can: false, strong: '', line: no.line };
  if (typeof beyond !== 'number') {
    const names = beyond.map((id) => g.regions[id].name);
    if (names.length > 0 && names.length <= 3) {
      const strong = `By rail this month to ${names.length === 1 ? names[0] : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`}:`;
      return { can: true, strong, line: `${strong} ${press} ${names.length === 1 ? 'it' : where.many}. It takes the whole month.` };
    }
    beyond = names.length;
  }
  if (beyond > 0) {
    const strong = `By rail this month to ${beyond} more place${beyond === 1 ? '' : 's'}:`;
    return { can: true, strong, line: `${strong} ${press} ${beyond === 1 ? where.one : where.many}. It takes the whole month.` };
  }
  const brk = nearestBreak(g, ride);
  return { can: true, strong: '', line: `By rail this month, but a train reaches no farther than it can march.${brk ? ` ${brk.line}` : ''}` };
}

/**
 * Whether a province is overseas as the score reads its home: the map's mark, and with the yield form on its
 * corrections (capitulations.json 'overseas' and 'home': the design director's 09:05Z and 11:50Z, on the historian's
 * overseas-marks-home-chart.md). Home is ground under the state's ordinary home regime, islands included; ground under
 * a regime of its own is overseas. Off, the map's mark alone, as release 1 has it.
 */
export const overseasRead = (g: Game, r: Region): boolean =>
  yieldMarks(g) ? OUT_FIX.has(r.name) || (!!r.overseas && !IN_FIX.has(r.name)) : !!r.overseas;
const CAP_MARKS = capData as unknown as { overseas?: string[]; home?: string[]; annexed?: { by: string; from: number; regions: string[] }[] };
const OUT_FIX = new Set<string>(CAP_MARKS.overseas ?? []);
const IN_FIX = new Set<string>(CAP_MARKS.home ?? []);
const yieldMarks = (g: Game): boolean => g.flags.open === 1 && g.flags.yieldRule === 1;
/** The record's formal annexations, by province (capitulations.json 'annexed'): who annexed it, and from which month. */
const ANNEXED = new Map<string, { by: string; from: number }>();
for (const a of CAP_MARKS.annexed ?? []) for (const name of a.regions) ANNEXED.set(name, { by: a.by, from: a.from });

/**
 * Whose home ground a province is, as the score and the yield form read it: none overseas (overseasRead), and with the
 * yield form on, the annexing state's from the month a formal annexation of the record was fixed (annexMonth), while
 * the country it belongs to stays fallen. Otherwise its owner. Off, exactly release 1's 'owner and not overseas'.
 */
export function homeOf(g: Game, r: Region): string | null {
  if (overseasRead(g, r)) return null;
  if (yieldMarks(g)) {
    const a = ANNEXED.get(r.name);
    if (a && g.flags[`annex:${r.id}`] !== undefined && !g.nations[r.owner]?.alive) return a.by;
  }
  return r.owner;
}

/**
 * The month's formal annexations (the yield form's month, yield.ts): a province of the record's list becomes its
 * annexer's home from its date, the first month on or after it in which the annexer holds it and the country it
 * belongs to has fallen. Fixed once (the flag 'annex:<id>', the month).
 */
export function annexMonth(g: Game): void {
  if (!yieldMarks(g)) return;
  for (const [name, a] of ANNEXED) {
    const r = g.byName[name];
    if (!r || g.turn < a.from || g.flags[`annex:${r.id}`] !== undefined) continue;
    if (r.ctrl === a.by && g.nations[a.by]?.alive && !g.nations[r.owner]?.alive) g.flags[`annex:${r.id}`] = g.turn;
  }
}

/** The share of a nation's home country, by worth, that is in enemy hands. */
export function homeLost(g: Game, nat: string): number {
  let vp = 0, held = 0;
  for (const r of g.regions) if (homeOf(g, r) === nat) { vp += r.vp; if (allied(g, nat, r.ctrl)) held += r.vp; }
  return vp ? 1 - held / vp : 0;
}

/** What each loss weighs when a country reckons whether to give in: its capital, its home country by worth, and its army. */
export const COLLAPSE = { capital: 0.45, home: 0.35, army: 0.2 };

/**
 * How far a nation's capital counts as lost, 0 to 1. `ahead` reads it as it will stand that many months on if
 * nothing changes (a siege that much older): the warnings ask that; the rule itself asks with none.
 */
export function capitalLost(g: Game, nat: string, ahead = 0): number {
  const n = g.nations[nat];
  // Shut in its capital by an army strong enough to close the ring, a government counts the city part lost already
  // (the grand campaign): a country overrun and beaten in the field does not fight on for months from inside one town.
  const seat = g.regions[n.capital];
  // And a government shut in loses its hold on the country month by month, until the capital counts as lost outright.
  const shut = siegeRules(g) && besieged(g, seat) && ringClosed(g, seat);
  const since = g.flags[`siege:${seat.id}`];
  const sat = (since === undefined ? 0 : Math.max(0, g.turn - since)) + ahead;
  // (Nothing in the month the ring closes: a capital stands a siege before its country gives in.)
  const after = SIEGE.capitalAfter;
  const hold = sat < after ? SIEGE.capital * sat / after : SIEGE.capital + (1 - SIEGE.capital) * (sat - after) / Math.max(1, SIEGE.capitalYear - after);
  return !allied(g, nat, seat.ctrl) ? 1 : shut ? Math.min(1, hold) : 0;
}

export function collapseScore(g: Game, nat: string): number {
  const n = g.nations[nat];
  if (n.capital < 0) return 0;
  const capLost = capitalLost(g, nat);
  // The colonies are fought over like any other ground, but a nation stands or falls by its home country.
  const army = n.armyPeak > 0 ? clamp(armySize(g, nat) / n.armyPeak, 0, 1) : 1;
  const score = COLLAPSE.capital * capLost + COLLAPSE.home * homeLost(g, nat) + COLLAPSE.army * (1 - army) + exhausted(g, nat);
  // A people gone short for months will bear less (the grand campaign): this is how a blockade chokes a country.
  const worn = willLost(g, nat);
  return worn > 0 ? score + worn : score;
}

const EXHAUST = (rivalsData as unknown as { exhaust?: { after: number; per: number } }).exhaust;

/**
 * A finish for a war that is won (the grand campaign): a computer nation whose capital has been in enemy hands for
 * more than a year wears out, a little more each month, until it asks for terms however much army it has left.
 * The player's own country is never worn out this way: he decides when he is beaten.
 */
export function exhausted(g: Game, nat: string): number {
  const out = g.flags[`capOut:${nat}`];
  if (g.flags.open !== 1 || !EXHAUST || nat === g.human || out === undefined) return 0;
  return Math.max(0, g.turn - out - EXHAUST.after) * EXHAUST.per;
}

/** Notes the month a nation's capital passed into enemy hands, and forgets it when the capital is its own again. */
function capitalWatch(g: Game, nat: string): void {
  const n = g.nations[nat];
  if (g.flags.open !== 1 || n.capital < 0) return;
  const key = `capOut:${nat}`;
  if (allied(g, nat, g.regions[n.capital].ctrl)) delete g.flags[key];
  else if (g.flags[key] === undefined) g.flags[key] = g.turn;
}

export function checkCollapse(g: Game, nat: string, by: string): void {
  const n = g.nations[nat];
  if (!n || !n.alive || n.capital < 0) return;
  // A country that has been liberated stands for as long as its capital does (the grand campaign).
  if (standsFreed(g, n)) return;
  if (collapseScore(g, nat) < n.resilience) return;
  // The grand campaign: a country gives in to the enemy at its throat, not to whoever's month it happens to be, and
  // to nobody while no enemy holds any of its home ground or stands on its border.
  if (g.flags.open === 1) {
    // Nor does it give in while its capital stands free and most of the country is still its own: hunger, a beaten
    // army and a lost province or two do not bring a government to the table by themselves.
    // (The atomic project, release 2, atom.ts: for some months after an atomic strike a computer-run country may ask for
    // terms all the same, to the enemy at its throat or else to the one that struck it. The will test above is unchanged.)
    const seat = g.regions[n.capital];
    const press = atomPress(g, nat);
    if (!press && siegeRules(g) && allied(g, nat, seat.ctrl) && !besieged(g, seat) && homeLost(g, nat) < SIEGE.stand) return;
    const to = victorOf(g, nat) ?? press;
    if (!to) return;
    by = to;
  }
  capitulate(g, nat, by);
}

/** The share of a nation's home country, by worth, that an enemy holds. With `also`, as it would stand with that province taken too. */
export function homeTaken(g: Game, nat: string, also?: Region): number {
  let vp = 0, taken = 0;
  // (Home ground as the score and the yield form read it, `homeOf`: with that form off, the owner's ground that is not overseas, as before.)
  for (const r of g.regions) if (homeOf(g, r) === nat) { vp += r.vp; if (r === also || atWar(g, nat, r.ctrl)) taken += r.vp; }
  return vp ? taken / vp : 0;
}

/**
 * The two conditions on which a nation with a row of terms asks for them (ruling 196; resources.json 'will.terms',
 * where Japan's is the only row, laid with the far side). The war has reached its home ground: an enemy holds the
 * row's share of it by worth. And its people can bear no more: privation has worn all it can off its will to fight.
 * Null for a nation with no row.
 */
export function termsOf(g: Game, nat: string): { ground: boolean; privation: boolean; taken: number; share: number } | null {
  const row = RES.will.terms?.[nat];
  if (!row || g.flags.open !== 1 || !g.nations[nat]) return null;
  const taken = homeTaken(g, nat);
  return { ground: taken >= row.home - 1e-9, privation: hungerLost(g, nat) >= RES.will.most - 1e-9, taken, share: row.home };
}

/**
 * A computer nation with a row of terms asks for them at the opening of its month when both conditions hold (ruling
 * 196): it gives in as any country does, to the enemy `victorOf` names, and its armies lay down their arms. No date
 * decides it. A player's own country is spared, as a player's Italy is: he decides when he is beaten.
 */
export function checkTerms(g: Game, nat: string): boolean {
  const n = g.nations[nat];
  if (!n?.alive || n.capital < 0 || nat === g.human) return false;
  const t = termsOf(g, nat);
  if (!t) return false;
  if (!t.ground || !t.privation) { termsNews(g, nat, t); return false; }
  const to = victorOf(g, nat);
  if (!to) return false;
  capitulate(g, nat, to, `${theName(n, true)} accepts terms and is out of the war.`);
  return true;
}

/**
 * The news of the month each condition first holds with the other not yet (ruling 196, the planner's two lines; the
 * UI director's sheet 25, box 5): said once each, under the country's own flag. The first carries its line for the
 * month card, where the whole sentence is a letter or two too long for a row. The month both hold the news is a
 * fall's, and none of this is said of a player's own country (checkTerms reads nothing for him).
 */
function termsNews(g: Game, nat: string, t: { ground: boolean; privation: boolean }): void {
  const n = g.nations[nat], name = theName(n), Name = theName(n, true);
  const once = (key: string, text: string, card?: string): void => {
    if (g.flags[key]) return;
    g.flags[key] = g.turn;
    g.headlines.push({ turn: g.turn, week: newsWeek(g), text: theWords(text), kind: 'event', nat, ...(card ? { card } : {}) });
  };
  if (t.ground) once(`terms:ground:${nat}`, `The war has reached ${name}'s home ground. ${Name} asks for terms when its people can bear no more.`, `War has reached ${name}'s home ground.`);
  if (t.privation) once(`terms:privation:${nat}`, `${Name}'s people can bear no more. It asks for terms once an enemy holds a part of its home ground.`);
}

/**
 * Who a beaten country gives in to (the grand campaign): the enemy that holds its capital or stands round it, or
 * else the one with most of its home ground and most troops on its soil and at its borders. Null if no enemy is
 * anywhere near: hunger and a lost fleet alone bring nobody to the table.
 */
export function victorOf(g: Game, nat: string): string | null {
  const n = g.nations[nat];
  const seat = g.regions[n.capital];
  if (atWar(g, nat, seat.ctrl) && g.nations[seat.ctrl]?.alive) return seat.ctrl;
  const ring = siegeRules(g) && besieged(g, seat) ? besiegerOf(g, seat) : null;
  if (ring && atWar(g, nat, ring)) return ring;
  const score: Record<string, number> = {};
  const add = (who: string, v: number): void => { if (atWar(g, nat, who) && g.nations[who]?.alive) score[who] = (score[who] ?? 0) + v; };
  for (const r of g.regions) {
    if (homeOf(g, r) !== nat) continue;
    add(r.ctrl, 2 + r.vp);
    for (const u of r.units) if (!u.dead) add(u.nat, 0.5 * u.divs * u.str);
    if (allied(g, nat, r.ctrl)) for (const j of r.nb) { const x = g.regions[j]; if (x.owner !== nat) for (const u of x.units) if (!u.dead) add(u.nat, 0.1 * u.divs * u.str); }
  }
  let best: string | null = null;
  for (const who of Object.keys(score).sort()) if (best === null || score[who] > score[best]) best = who;
  return best;
}

function nearestFriendly(g: Game, c: Corps): Region | null {
  const seen = new Uint8Array(g.regions.length);
  const q = [c.reg];
  seen[c.reg] = 1;
  for (let i = 0; i < q.length; i++) {
    const r = g.regions[q[i]];
    if (allied(g, c.nat, fieldCtrl(g, r)) && q[i] !== c.reg) return r;
    for (const j of r.nb) if (!seen[j]) { seen[j] = 1; q.push(j); }
  }
  return null;
}

/** `said`: the news of the fall in other words than "capitulates to" (a country that asks for terms, ruling 196). */
export function capitulate(g: Game, nat: string, by: string, said?: string): void {
  const X = g.nations[nat];
  if (!X.alive) return;
  const was = holdersNow(g); // settled towns (release 2): ground handed over here starts settling this month
  X.alive = false;
  X.fellOn = { turn: g.turn, week: g.week, to: by, how: 'beaten' };
  recordFall(g, nat, by, 'beaten');
  // (Who it was at war with is kept: a freed country takes those wars up again, and its conquerors' allies finish theirs.)
  rememberWars(g, nat);
  // In the last act of the grand campaign nobody signs an armistice: a fallen France is simply conquered. So is a
  // France that had been liberated and falls a second time.
  const armistice = nat === 'FRA' && !g.flags.dom && !X.freedOn;
  delete X.freedOn;
  // The far side (ruling 199(b)): its forces on far ground that passes into keeping stay at their posts and answer to the keeper.
  const keeping = keptForces(g, nat, by, armistice ? (r) => r.vichy : undefined);
  for (const c of g.corps.filter((c) => c.nat === nat)) {
    const at = g.regions[c.reg];
    // The Army of Africa stays at its posts and answers to the new government.
    if (armistice && at.vichy && at.overseas && at.ctrl === 'FRA') { c.nat = 'VIC'; c.hold = true; continue; }
    removeCorps(g, c);
  }
  endVoyages(g, nat);
  fleetsOnFall(g, nat);
  airOnFall(g, nat);
  const pact = g.nations.SOV.alive && !atWar(g, 'SOV', by);
  // Its sieges end with it: nobody is left to hold the towns, or to stand round them.
  dropSieges(g, nat);
  const kept: Region[] = [];
  for (const r of g.regions) {
    if (r.owner === nat) {
      if (armistice && r.vichy && r.ctrl === 'FRA') { r.owner = 'VIC'; r.ctrl = 'VIC'; }
      else if (nat === 'POL' && r.sov && pact) r.ctrl = 'SOV';
      else if (r.ctrl === nat) {
        // Ground still held by the fallen nation's allies stays in their hands until it is taken.
        // (An ally's garrison holds its town the same way.)
        // (The far side: ground whose garrison has just passed to its keeper is in that keeper's keeping.)
        const inKeeping = keeping.get(r.id);
        const holder = inKeeping ? undefined : r.units.find((u) => u.nat !== nat && atWar(g, u.nat, by)) ?? garrisonOf(r).find((u) => u.nat !== nat && atWar(g, u.nat, by));
        // An island or colony out in the ocean, beyond the old chart, is not the victor's for the asking: a sea power
        // still fighting him takes it into its keeping (the wider world).
        const keeper = inKeeping ?? (!holder && r.overseas && beyondEurope(r) ? protectorOf(g, by, r) : null);
        if (keeper) { r.ctrl = keeper; r.since = g.turn; kept.push(r); }
        else r.ctrl = holder ? holder.nat : by;
      }
    } else if (r.ctrl === nat) {
      // Foreign ground the fallen country held goes back to its owner, unless an army of the fallen country's side
      // still stands on it (the grand campaign): then it is that army's until it is taken. An army at war with the
      // victor counts as much as one at war with the owner: ground in a country long dead is not handed to an enemy
      // over the heads of the troops who hold it.
      const keeps = (u: Corps): boolean => u.nat !== nat && !u.dead && (atWar(g, u.nat, r.owner) || atWar(g, u.nat, by));
      const stays = g.flags.open === 1 ? r.units.find(keeps) ?? garrisonOf(r).find(keeps) : undefined;
      // (The far side, ruling 199(c): ground a fallen keeper only kept passes on to the next keeper, as its own ground does.)
      const next = !stays && !g.nations[r.owner].alive && r.overseas && keepersOf(g, r) ? protectorOf(g, by, r) : null;
      r.ctrl = stays ? stays.nat : g.nations[r.owner].alive ? r.owner : next ?? by;
      if (next) kept.push(r);
    }
  }
  if (armistice) {
    const V = g.nations.VIC;
    V.alive = true; V.dormant = false;
    const cap = g.regions.find((r) => r.owner === 'VIC' && r.name === 'Auvergne') ?? g.regions.find((r) => r.owner === 'VIC');
    // (Its distances are counted the month it has a capital, as every other country's are on the day it is drawn:
    // a beaten Vichy corps falls back toward Auvergne whatever year the campaign began in.)
    if (cap) { cap.cap = true; V.capital = cap.id; g.capDist.VIC = capitalDistances(g, cap.id); }
    headline(g, 'fall', 'France signs an armistice. The north and the Atlantic coast are occupied; a new government at Vichy keeps the south.', 'FRA', undefined, { arm: 'land', won: by, lost: 'FRA' });
  } else {
    headline(g, 'fall', said ?? `${theName(X, true)} capitulates to ${theName(g.nations[by])}.`, nat, undefined, { arm: 'land', won: by, lost: nat });
  }
  // (Ruling 201, point 4: the fall's own line of news, kept so that its second line can be written once the fall
  // has brought down whoever it brings down.)
  const told = g.headlines[g.headlines.length - 1];
  if (kept.length) {
    const K = g.nations[kept[0].ctrl];
    const names = kept.map((r) => r.name);
    // (A whole colonial empire is not read out place by place: the three of most worth, and how many more.)
    const chief = (): string[] => [...kept].sort((x, y) => worthOf(g, y) - worthOf(g, x) || x.id - y.id).slice(0, 3).map((r) => r.name);
    const told = names.length > 6 ? `${chief().join(', ')} and ${names.length - 3} other places` : names.length === 1 ? names[0] : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
    headline(g, 'event', `${K.adj} forces take ${told} into their keeping: ${theName(X)} has fallen, and ${names.length === 1 ? 'the place is' : 'they are'} not to be ${g.nations[by].adj} for the asking.`, K.id, kept[0].id);
  }
  for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((e) => e !== nat);
  X.atWar = [];
  g.supplyDirty = true;
  // Anyone left standing on ground that is no longer theirs to stand on.
  for (const c of [...g.corps]) {
    const r = g.regions[c.reg];
    if (allied(g, c.nat, r.ctrl)) continue;
    if (atWar(g, c.nat, r.ctrl) && hostileUnits(g, r, c.nat).length === 0) continue; // still occupying enemy ground
    const N = g.nations[c.nat];
    // (The far side: home only while its own side holds the capital: `refugeHeld`. With the capital the enemy's the
    // corps goes to the nearest ground its side holds, or with none lays down its arms where it stands.)
    if (N.seaSupply && N.capital >= 0 && (!r.seas.length || seaOpenAt(g, c.nat, r)) && !(farOn(g) && !allied(g, c.nat, g.regions[N.capital].ctrl))) {
      place(g, c, g.regions[N.capital]);
      c.str *= 0.7; c.org = 0.3; c.ap = 0;
      if (!g.flags[`evac:${g.turn}`]) { g.flags[`evac:${g.turn}`] = 1; headline(g, 'event', `${N.adj} troops are evacuated from the Continent, leaving their heavy equipment behind.`, c.nat, undefined, { arm: 'land', lost: c.nat }); }
    } else {
      const to = nearestFriendly(g, c);
      if (to) { place(g, c, to); c.ap = 0; } else removeCorps(g, c);
    }
  }
  handedOver(g, was);
  // (Release 2, atom.ts) Its atomic project ends with it, and whoever holds the plants' ground takes the papers.
  atomFallen(g, nat, by);
  // The fall of one nation can bring down another.
  for (const n of Object.values(g.nations)) if (n.alive && n.atWar.includes(by)) checkCollapse(g, n.id, by);
  // The grand campaign: this fall may have put another country's capital into the hands of that country's friends.
  afterFall(g, nat);
  // The last act of the grand campaign: was that the last but one?
  crown(g);
  // (Ruling 201, point 4, the whole-world chart: a great power's fall that does not decide the war says so on the
  // second line of its own band, naming the powers still at war. Read last, when every fall this one brought on has
  // been counted. Null, and nothing said, on any narrower chart, where the fall decides the war, and in the last act.)
  const on = warGoesOn(g, nat);
  if (on) told.text += ` ${on.news}`;
}

/** Sea transport: a nation with shipping can send a corps from one friendly port to another. It takes the whole month, and the enemy may find the convoy. */
export function shipMove(g: Game, c: Corps, to: Region, rides = false): boolean {
  if (shipCorps(g, c, to, rides)) { delete c.sail; return true; }
  // (The far side's ship move, the design director's 13:56Z and 18:23Z of 22 November 2026: a port further off than a
  // month's sailing is reached by ports its side or an ally holds, a month a leg. A computer staff's corps goes the
  // first leg now and sails on from there next month, `sailOn`; the player ships his own leg by leg. Nothing on the
  // old chart, where `sailFirst` finds no leg.)
  if (c.nat === g.human) return false;
  const leg = sailFirst(g, c, to, rides);
  if (leg && shipCorps(g, c, leg, rides)) c.sail = to.id;
  return false;
}

/**
 * The far side's ship move: a computer staff's corps bound by sea for a port further off than a month's sailing
 * sail their next leg, before the staff moves anyone. An order that cannot go on this month lapses, and the corps
 * is the staff's to use where it stands.
 */
export function sailOn(g: Game, nat: string): void {
  if (nat === g.human) return;
  for (const c of [...g.corps]) {
    if (c.nat !== nat || c.sail === undefined) continue;
    const to = g.regions[c.sail];
    delete c.sail;
    if (to && to.id !== c.reg) shipMove(g, c, to);
  }
}

/** Lifts a corps off the Continent from a port: to a haven of its theatre if there is one to go to, or else home. */
export function evacuate(g: Game, c: Corps): boolean {
  const r = g.regions[c.reg];
  const N = g.nations[c.nat];
  // (Ruling 173(e), the whole-world chart: a withered island garrison does not sail. It waits for a ship of its side.)
  if (withered(g, c)) return false;
  if (!N.seaSupply || !r.port || r.owner === c.nat || N.capital < 0 || !seaOpenAt(g, c.nat, r)) return false;
  const dest = refugeHeld(g, c, r);
  if (!dest) return false;
  place(g, c, dest);
  c.str *= 0.75; c.org = 0.3; c.ap = 0;
  liftedOut(c, r, dest);
  if (!g.flags[`evac:${g.turn}`]) {
    g.flags[`evac:${g.turn}`] = 1;
    headline(g, 'event', dest.cap ? `${N.adj} troops are lifted off the beaches at ${r.name}.` : `${N.adj} troops are lifted off the beaches at ${r.name} and taken to ${dest.name}, without their heavy equipment.`, c.nat, r.id, { arm: 'land', lost: c.nat });
  }
  return true;
}

/**
 * Why a corps cannot be lifted off by sea from where it stands, in the tests evacuate() makes and in their order;
 * null when it can. Words only: the page shows the order off with this sentence instead of lit and silent.
 */
export function whyNotEvacuate(g: Game, c: Corps): string | null {
  const r = g.regions[c.reg];
  const N = g.nations[c.nat];
  if (withered(g, c)) return `${c.name} is cut off and out of stores on ${placeName(r.name)}: it cannot be lifted off until stores have reached it by sea.`;
  if (!N.seaSupply) return `${N.name} has no ships to lift an army off a beach.`;
  if (!r.port) return `${r.name} has no port to lift an army from.`;
  if (r.owner === c.nat) return `${r.name} is home ground: there is nowhere to be lifted off to.`;
  if (N.capital < 0) return `${N.name} has no home left to sail for.`;
  if (!seaOpenAt(g, c.nat, r)) {
    // Every sea at this port is closed to the troopships: name the one nearest to opening, in the landing's own sentence (ruling 207).
    const gap = (z: number): number => opensAt(g, z, c.nat) - seaSide(g, z, c.nat, false).total;
    const z = [...r.seas].sort((a, b) => gap(a) - gap(b))[0];
    return z === undefined ? `${r.name} has no sea to sail from.` : closedLine(g, z, c.nat);
  }
  if (!refugeHeld(g, c, r)) return `${g.regions[N.capital].name} is in enemy hands, and no harbour of ${N.name}'s side is near: there is nowhere to be lifted off to.`;
  return null;
}

// ---- The start of a nation's month ---------------------------------------------------

const ordinal = (n: number): string => `${n}${n % 100 >= 11 && n % 100 <= 13 ? 'th' : ['th', 'st', 'nd', 'rd', 'th', 'th', 'th', 'th', 'th', 'th'][n % 10]}`;

/** How sharply the standing call-up falls with the share of home industry lost (the grand campaign): 1 is in step with it. */
const OPEN_CALLUP = 1;

/** True while the player holds his country's standing call-up (the grand campaign): no new corps are raised from the reserve, and the waves wait. */
export const callUpHeld = (g: Game, nat: string): boolean => g.flags.open === 1 && nat === g.human && g.flags[`mph:${nat}`] === 1;

/**
 * True while a nation is at war with a great power: what starts a call-up and keeps it running (ruling 228, 1a). A war
 * with a small country, Finland's in the winter of 1939, calls up nobody.
 */
const greatWar = (g: Game, nat: string): boolean => { const big = majorsOf(g); return g.nations[nat].atWar.some((e) => big.includes(e)); };

/**
 * A country's standing call-up as it stands (the grand campaign): whether the player holds it, and the corps the next
 * wave would raise at most, of how many divisions each. Null for a country that has none, or is not yet at war.
 */
export function standingCallUp(g: Game, nat: string): { held: boolean; corps: number; divs: number } | null {
  const plan = EASTERN.mobilisation[nat], n = g.nations[nat];
  if (g.flags.open !== 1 || !plan || !n?.alive || !greatWar(g, nat)) return null;
  const began = g.flags[`mobilised:${nat}`];
  const wave = began === undefined ? 0 : g.turn + 1 - began - (g.flags[`mphn:${nat}`] ?? 0);
  return { held: callUpHeld(g, nat), corps: Math.floor((plan.perMonth[wave] ?? plan.after) * (1 - callUpCut(g, nat)) + (g.flags[`mtr:${nat}`] ?? 0) + 1e-9), divs: plan.divs };
}

/** Watches the call-up (measures only; moves nothing): each month, the corps the plan and the reserve allowed before the trim, and those raised. */
export const callUpWatch: { tell: ((e: { nat: string; turn: number; wave: number; want: number; raised: number; divs: number }) => void) | null } = { tell: null };

/**
 * The one trim of the free call-up (ruling 228, rider b481; sized on the tree that ships, b627): the share of the corps
 * a nation's call-up would raise each month that are not raised, at the dial's stops its plan names (the grand
 * campaign only). Their men stay in the reserve, where the depots' drafts can still reach them. The call-up only: the
 * factories are never touched. 0 where there is no trim, the stop is not one of its, or the month is before its 'from'.
 */
export function callUpCut(g: Game, nat: string): number {
  const t = EASTERN.mobilisation[nat]?.trim;
  return g.flags.open === 1 && t && t.cut > 0 && t.stops.includes(stopOf(g)) && g.turn >= (t.from ?? 0) ? Math.min(1, t.cut) : 0;
}

/**
 * The men a held call-up sends to the corps in the field each month, in divisions' worth (the grand campaign): the
 * wave that was not raised, as far as the reserve could have filled it. They go out over and above what the depots
 * send and cost the treasury nothing. 0 where the call-up is running, or there is none.
 */
export const heldDrafts = (g: Game, nat: string): number => (callUpHeld(g, nat) ? g.flags[`mphw:${nat}`] ?? 0 : 0);

/** The player holds his standing call-up, or lets it run again. False where there is none to hold or nothing changes. */
export function holdCallUp(g: Game, nat: string, hold: boolean): boolean {
  if (nat !== g.human || !standingCallUp(g, nat) || callUpHeld(g, nat) === hold) return false;
  if (hold) g.flags[`mph:${nat}`] = 1; else { delete g.flags[`mph:${nat}`]; delete g.flags[`mphw:${nat}`]; }
  return true;
}

/**
 * The share of its home industry a nation still holds, as the standing call-up reckons it (the grand campaign): 1 with
 * every home factory in hand, 0 with none, or for a nation that never had one. Home ground on the far side of the
 * world counts at its share here as it does for money and for lines (ruling b475, economy.ts farShare): the Soviet
 * Union's works beyond the old chart are never lost, and counted whole they held a wave at 0.81 to 0.84 of its size
 * where the old chart's reckoning cut it to 0.61 to 0.68.
 */
export function industryHeld(g: Game, nat: string): number {
  let own = 0, held = 0;
  for (const r of g.regions) if (r.owner === nat && !r.overseas) { const f = r.fac * farShare(r); own += f; if (r.ctrl === nat) held += f; }
  return own > 0 ? held / own : 0;
}

/** A nation with a mobilisation plan raises a wave of new corps every month once it is at war with a great power. */
function mobilise(g: Game, nat: string): void {
  const plan = EASTERN.mobilisation[nat];
  const n = g.nations[nat];
  if (!plan || !greatWar(g, nat)) return;
  const key = `mobilised:${nat}`;
  if (g.flags[key] === undefined) g.flags[key] = g.turn;
  const month = g.turn - g.flags[key];
  // The player's say over the call-up (the grand campaign): held, it raises nothing, and the wave waits its turn.
  const held = callUpHeld(g, nat), waited = g.flags.open === 1 ? g.flags[`mphn:${nat}`] ?? 0 : 0, wave = month - waited;
  let count = plan.perMonth[wave] ?? plan.after;
  if (held) g.flags[`mphn:${nat}`] = waited + 1;
  // The grand campaign: once the first waves are in, the standing call-up is only as large as the industry the
  // nation still holds can arm. A country that has lost half its factories raises half the corps.
  if (g.flags.open === 1 && wave >= plan.perMonth.length) count = Math.round(count * Math.pow(industryHeld(g, nat), OPEN_CALLUP));
  // The grand campaign: every corps of the call-up is filled from the reserve of men, and when that is spent the waves stop.
  // The first waves take whatever there is; the standing call-up after them takes only what the armies in the field do not need.
  if (g.flags.open === 1) count = Math.min(count, wave < plan.perMonth.length ? raisable(g, nat, plan.divs) : Math.max(0, Math.floor(menSpare(g, nat) / plan.divs)));
  // The one trim (b481, b627): a share of the corps the month would raise are not raised, the fraction carried to the next month.
  const want = count;
  const cut = held ? 0 : callUpCut(g, nat);
  if (cut > 0 && count > 0) { const want = count * (1 - cut) + (g.flags[`mtr:${nat}`] ?? 0); count = Math.floor(want + 1e-9); g.flags[`mtr:${nat}`] = Math.max(0, want - count); }
  // Held, the wave is not raised: its men go to the corps in the field as this month's extra drafts (startPhase).
  if (held) { g.flags[`mphw:${nat}`] = Math.min(plan.perMonth[wave] ?? plan.after, raisable(g, nat, plan.divs)) * plan.divs; count = 0; }
  else if (g.flags[`mphw:${nat}`] !== undefined) delete g.flags[`mphw:${nat}`];
  const enemyNear = (r: Region): boolean => r.nb.some((j) => atWar(g, nat, g.regions[j].ctrl));
  // The grand campaign: a computer's staff whose line has met dug-in troops forms one corps of each month's wave as
  // massed guns, until it has its share of them (the same share its factories would be asked for).
  const guns = g.flags.open === 1 && nat !== g.human && count > 0 && !locked(g, nat, 'corps:art') && gunsWanted(g, nat) ? specOf(g, 'art') : null;
  let raisedNow = 0;
  for (let k = 0; k < count; k++) {
    // Form up in the industrial towns still held, emptiest first, away from the fighting.
    let best: Region | null = null, bs = 1e9;
    for (const r of g.regions) {
      if (r.owner !== nat || r.ctrl !== nat || r.fac < 1 || r.units.filter((u) => !roomless(g, u)).length >= roomAt(g, r, nat) || enemyNear(r) || besieged(g, r)) continue;
      const s = r.units.length * 10 - r.fac - (r.cap ? 3 : 0);
      if (s < bs) { bs = s; best = r; }
    }
    if (!best) break;
    const asGuns = guns && k === 0;
    const c = asGuns ? addCorps(g, nat, corpsName(nat, 'art', n.corpsNo++), 'art', guns.divs, best.id, { xp: plan.xp })
      : addCorps(g, nat, `${ordinal(n.corpsNo++)} ${plan.name}`, plan.kind, plan.divs, best.id, { xp: plan.xp });
    c.org = 0.7;
    raisedNow++;
    drawMen(g, nat, asGuns ? guns.divs : plan.divs);
  }
  callUpWatch.tell?.({ nat, turn: g.turn, wave, want, raised: raisedNow, divs: plan.divs });
  if (count > 0 && month === 0) headline(g, 'event', `${n.name} orders general mobilisation.`, nat);
  // Behind the front, the people dig: anti-tank ditches and gun lines around the great cities.
  // In free play nobody waits for history's calendar: the lines round a city are dug as soon as the enemy is three marches from it.
  const close = (r: Region): boolean => {
    const seen = new Set([r.id]);
    let ring = [r.id];
    for (let d = 0; d < 3; d++) {
      const next: number[] = [];
      for (const id of ring) for (const j of g.regions[id].nb) if (!seen.has(j)) { seen.add(j); next.push(j); if (atWar(g, nat, g.regions[j].ctrl)) return true; }
      ring = next;
    }
    return false;
  };
  for (const [when, place, lv] of plan.works ?? []) {
    const r = g.byName[place];
    const due = when === month || (g.flags.open === 1 && month >= 1 && month < when && !!r && close(r));
    if (!due || !r || r.ctrl !== nat || (r.fort?.lv ?? 0) >= lv) continue;
    r.fort = { lv, vs: r.fort?.vs ?? [nat], by: nat };
    headline(g, 'event', `Defence lines are dug around ${r.name}.`, nat, r.id);
  }
}

const THEATRE_LIST = Object.values(theatreData as Record<string, any>).filter((x) => typeof x !== 'string') as { name: string; regions: string[]; havens?: string[]; scripted?: boolean; opens?: number; works?: [number, string, number, string[], string, { taken?: boolean; port?: boolean; vs?: string[] }?][] }[];
/** The theatre each region is listed under, by name, whether or not that theatre has opened yet. */
const THEATRE_OF = new Map<string, number>();
THEATRE_LIST.forEach((t, k) => { for (const nm of t.regions) THEATRE_OF.set(nm, k); });

/**
 * A theatre with an opening month is ordinary ground until then: its regions are part of the main fronts and their
 * battles roll the main dice, exactly as if the theatre were not listed. From that month on it is a war apart like
 * the others. In free play it never opens: the war there stays the main staff's business.
 */
export function openTheatres(g: Game): void {
  if (g.flags.open === 1) return;
  THEATRE_LIST.forEach((t, k) => {
    if (t.opens === undefined || g.turn < t.opens) return;
    for (const nm of t.regions) { const r = g.byName[nm]; if (r && r.theatre !== k) { r.theatre = k; g.supplyDirty = true; } }
  });
}

/** True for ground in a theatre that opened late in the war. Its battles keep a count of their own, so that they never change the luck of the older theatres or of the main fronts. */
const lateTheatre = (r: Region): boolean => r.theatre >= 0 && THEATRE_LIST[r.theatre]?.opens !== undefined;

/** The name of the side theatre a region belongs to, as data/campaign/theatres.json gives it. */
export function theatreName(r: Region): string | null {
  return r.theatre >= 0 ? THEATRE_LIST[r.theatre]?.name ?? null : null;
}

/** The places troops lifted off this region's coast are taken to before anywhere else: its theatre's havens. */
export function havensOf(r: Region): string[] {
  return r.theatre >= 0 ? THEATRE_LIST[r.theatre]?.havens ?? [] : [];
}

/**
 * A corps lifted off out of the theatre it stood in holds nothing in place where it lands: the place it was to hold is
 * gone. (Kept, Creforce and W Force stood held in London for the rest of the war, playtest war 17.) Taken to one of
 * the theatre's own havens it holds on there, as W Force did on Crete.
 */
function liftedOut(c: Corps, from: Region, to: Region): void {
  if (to.theatre !== from.theatre || to.theatre < 0) c.hold = false;
}

/** Where troops lifted off a coast are landed: a haven of the same theatre still in friendly hands, with room and ships able to reach it, or else home. */
export function refuge(g: Game, c: Corps, from: Region): Region {
  for (const nm of havensOf(from)) {
    const r = g.byName[nm];
    if (r && r.id !== from.id && allied(g, c.nat, r.ctrl) && !hostileUnits(g, r, c.nat).length && placed(g, r, c.nat).length < roomAt(g, r, c.nat) && seaOpenAt(g, c.nat, r)) return r;
  }
  // Ruling 221 (F): troops lifted off the coast of a country their own is fighting are landed at the nearest harbour
  // their country holds on that country's ground, with no enemy in it, room and the sea open: the war they came for
  // goes on. Without it they are taken home, and from an island home nothing ever ships them back (paper 20, finding 3).
  // It is a rule of the grand campaign, at every stop of its dial: history's campaigns lift their troops off as they
  // were measured to (with the far side's switch on the row is laid for every game, and four wars in 24 of the 1939
  // campaign played differently from May 1941 until this asked: tests/far-history-same.test.ts).
  if (EASTERN.nearRefuge && g.flags.open === 1 && from.owner !== c.nat && atWar(g, c.nat, from.owner)) {
    const near = nearHarbour(g, c.nat, from);
    if (near) return near;
  }
  return g.regions[g.nations[c.nat].capital];
}

/**
 * (The far side: troops lifted off a coast are landed only on ground their own side holds. The havens and the near
 * harbour are asked that already; the capital, the last place tried, was not, and Japanese corps were set down inside
 * a Tokyo the United States held in 9 of the playtester's 31 far wars of 22 November 2026. With no such ground there
 * is nowhere to be lifted to, and a beaten corps with its back to the sea lays down its arms as one cut off (ruling
 * 232(b)): the design director's ruling B1 of 07:22Z, 22 November 2026, every nation. With the far side off, as it was.)
 */
export function refugeHeld(g: Game, c: Corps, from: Region): Region | null {
  const dest = refuge(g, c, from);
  return farOn(g) && !allied(g, c.nat, dest.ctrl) ? null : dest;
}

/**
 * The nearest harbour a nation holds on the ground of the country it stands in, other than where it stands, with no
 * enemy in it, room for one more corps and the sea open to its ships (ruling 221, F); 'fits' narrows the choice.
 */
export function nearHarbour(g: Game, nat: string, from: Region, fits: (r: Region) => boolean = () => true): Region | null {
  return g.regions
    .filter((r) => r.port && r.id !== from.id && r.owner === from.owner && r.ctrl === nat && !hostileUnits(g, r, nat).length && placed(g, r, nat).length < roomAt(g, r, nat) && seaOpenAt(g, nat, r) && fits(r))
    .sort((a, b) => chartDist(g, a, from) - chartDist(g, b, from) || a.id - b.id)[0] ?? null;
}

/**
 * A corps with no road back to its own side is taken out by sea (the director's ruling of 18:59Z, under 221 F): to
 * the harbour given, at an evacuation's cost, with the news. The line's march home calls it (ai.ts lineHome).
 */
export function takeOut(g: Game, c: Corps, to: Region): void {
  const from = g.regions[c.reg], N = g.nations[c.nat];
  place(g, c, to);
  c.str *= 0.75; c.org = Math.min(c.org, 0.3); c.ap = 0; c.hold = false;
  if (!g.flags[`evac:${g.turn}`]) {
    g.flags[`evac:${g.turn}`] = 1;
    headline(g, 'event', `${N.adj} troops at ${from.name}, with no road back to their own lines, are taken out by sea to ${to.name}, without their heavy equipment.`, c.nat, from.id, { arm: 'land', lost: c.nat });
  }
}

/**
 * A formation put together from an army already overseas ('from' names its theatre) takes its divisions out of
 * that army: the largest corps there give them up, and a corps left with none is struck off. Garrisons are not touched.
 */
function draw(g: Game, nat: string, theatre: string, divs: number, made?: Corps): void {
  const k = THEATRE_LIST.findIndex((t) => t.name === theatre);
  for (let need = divs; k >= 0 && need > 0;) {
    const c = g.corps.filter((x) => x !== made && x.nat === nat && !x.dead && !x.hold && x.kind !== 'fort' && g.regions[x.reg].theatre === k && !(made && g.flags[`formed:${x.id}`])).sort((a, b) => b.divs - a.divs || a.id - b.id)[0];
    if (!c) break;
    const take = Math.min(need, c.divs);
    if (take >= c.divs) removeCorps(g, c); else c.divs -= take;
    need -= take;
  }
  g.supplyDirty = true;
}

/**
 * Whether a lent row's time with its campaign is up: its month (`until`), by the book and wherever no more is said.
 *
 * The far side, at the stops that keep no book (the design director's rulings of 22:55Z, 21 November, and 03:49Z,
 * 22 November 2026; no number): a lent row leaves when the place it was lent for is decided.
 *   takes   a row lent to take a place leaves when that place is its side's, or when the front of its staff's plan
 *           that lists the row has ended with the front's own place untaken. While that front stands, and after it
 *           has taken its own place with the row's still to take (the luzon front's is Manila, and the guns and the
 *           7th Tank Regiment were lent for Bataan), the row stays, past the book's month. Where no front of a plan
 *           lists the row (the plan never taken up; the player's own Japan) the place taken is all that decides it
 *           (his ruling of 04:26Z): at these stops nothing ends by date alone.
 *   holds   a row lent to hold places leaves when the last of them is lost, and stays while one is held.
 */
function lentOver(g: Game, nat: string, row: any[], o: any): boolean {
  const due = g.turn >= o.until;
  if (!farOn(g) || stopDef(g).book || (!o.takes && !o.holds)) return due;
  const mine = (nm: string): boolean => { const r = g.byName[nm]; return !!r && allied(g, nat, r.ctrl); };
  if (o.holds) return !(o.holds as string[]).some(mine);
  if (mine(o.takes)) return true;
  const F = frontOfRow(g, nat, row[2]);
  return !!F && !F.live && !F.taken;
}

/**
 * The far side: a division made up of regiments, some of them lent to a campaign elsewhere when its row forms (the
 * design director's rulings of 03:49Z and 04:01Z, 22 November 2026, on the historian's paper
 * japan-tank-divisions-1942.md; no number). The division's row names the regiments on the board it is formed from
 * (`from`) and the lent ones it waits for (`awaits`); the row stands by date at every stop. What changes is the
 * make-up:
 *   the share    a lent regiment's share of its division is the regiment's own fullest size by its own row. While
 *                the regiment is lent the division's fullest size is that much short, and the drafts do not fill it.
 *   coming home  a regiment that leaves its place alive (`lentOver`) brings the worth it stands at into its
 *                division, one for one, and its share is the division's again: never more than the share, so no tank
 *                is lost, and the regiment's corps leaves the board that month, so none is counted twice. One that
 *                comes home before its division has formed is remembered at that worth.
 *   destroyed    a regiment destroyed while lent holds no share: the drafts fill it, as the record replaced losses.
 *   formation    the row's own figure counts the regiments the book has with the division in its month: those it
 *                is formed from, and each lent one whose book month has come by then, every one an equal part. A
 *                counted regiment that is not there (lent still, or destroyed) leaves the division that part
 *                short; one the figure does not count that is home already adds the worth it came home at.
 * Each regiment the row awaits: its place in the list of dated rows, its book month, its share in the division's
 * own divisions, whether it is lent still, and the worth it came home at (null where it has not come home).
 */
function awaited(g: Game, nat: string, o: any, divs: number): { j: number; until: number; share: number; lent: boolean; home: number | null }[] {
  const rows = ((setup as any).expeditions as any[]) ?? [];
  const out: { j: number; until: number; share: number; lent: boolean; home: number | null }[] = [];
  for (const name of o.awaits as string[]) {
    const j = rows.findIndex((r) => r[1] === nat && r[2] === name && r[6]?.until);
    if (j < 0) continue;
    const id = g.flags[`lent:${j}`] ?? 0, came = g.flags[`home:${j}`];
    const home = came === undefined ? null : (came - 1) / 1000 / divs;
    out.push({ j, until: rows[j][6].until, share: ((estAt(rows[j][6].est, g.turn) ?? 1) * rows[j][4]) / divs, lent: home === null && id > 0 && g.corps.some((x) => x.id === id && x.nat === nat && !x.dead), home });
  }
  return out;
}

/** The fullest size of a division that awaits lent regiments (`awaited`): its row's, less the share of each one lent still. It takes nothing from what the division already has. */
function makeUp(g: Game, nat: string, row: any[], o: any): void {
  const d = g.corps.find((x) => x.nat === nat && x.name === row[2] && !x.dead);
  if (!d || d.divs !== row[4]) return;
  const full = Math.max(d.str, (estAt(o.est, g.turn) ?? 1) - awaited(g, nat, o, d.divs).reduce((sum, r) => sum + (r.lent ? r.share : 0), 0));
  if (full < 1) d.full = full; else delete d.full;
}

/** A division that awaits lent regiments, as its row forms it (`awaited`, formation): its figure less the part of each counted regiment that is not there, and with the worth of each uncounted one that is home already. */
function formUp(g: Game, nat: string, row: any[], o: any, c: Corps): void {
  const A = awaited(g, nat, o, c.divs);
  const counted = (Array.isArray(o.from) ? o.from.length : 0) + A.filter((r) => r.until <= row[0]).length;
  for (const r of A) {
    if (r.until <= row[0]) { if (r.home === null) c.str -= o.str / counted; }
    else if (r.home !== null) c.str += r.home;
  }
  makeUp(g, nat, row, o);
}

/** A lent regiment that leaves its place alive comes home to the division whose row awaits it (`awaited`, coming home). */
function comeHome(g: Game, nat: string, i: number, row: any[], c: Corps): void {
  if (!farOn(g) || c.dead) return;
  const rows = ((setup as any).expeditions as any[]) ?? [];
  const k = rows.findIndex((r) => r[1] === nat && Array.isArray(r[6]?.awaits) && r[6].awaits.includes(row[2]));
  if (k < 0) return;
  const worth = c.str * c.divs;
  g.flags[`home:${i}`] = 1 + Math.round(worth * 1000);
  const d = g.flags[`sent:${k}`] ? g.corps.find((x) => x.nat === nat && x.name === rows[k][2] && !x.dead) : undefined;
  if (!d || d.divs !== rows[k][4]) return;
  d.str += worth / d.divs;
  makeUp(g, nat, rows[k], rows[k][6]);
}

/**
 * Formations history sent to a war away from the main fronts. Unlike the home mobilisation they need a
 * friendly place to arrive at: one that has been lost, or is full, puts the move off, and after half a year
 * it is cancelled. Formations only lent to a campaign ('until') go back to the main army when their time is up.
 * Divisions sent to strengthen a corps already at the front ('joins' names it) go into that corps wherever it
 * stands: it keeps its general and what it has learned, and takes the new name. A formation made by breaking
 * up others ('from' lists them by name) strikes those corps off as it forms. One that waits on a defeat ('ifLost')
 * forms only once the enemy holds every place named.
 */
/** True if a nation and a friend of it have an enemy in common: the friend is in the nation's war, not standing by. */
const sharesWar = (g: Game, nat: string, friend: string): boolean => !!g.nations[friend]?.atWar.some((e) => atWar(g, nat, e));

/**
 * Free play: where an army history dated is really put down. The place meant for it if there is room there; if
 * not, the nearest ground in the nation's own hands with room for one more corps, across the enemy's lines if
 * the place is a pocket. Nobody stacks a sixth corps in a province because a calendar said so. Null if every such
 * place is full.
 *
 * (The whole-world chart: the record's own place is counted against five as it always was, `record`, though it be an
 * island of the Pacific with room for fewer. The record was written before the islands' table, and where its rows
 * stand more corps on an island than the island's room they are put down all the same and nobody is sent away or
 * struck for it: the historian mends the rows (form B3 of 25 November 2026, (c)). The ground a row is sent on to is
 * counted by its own room, an island's among it, as is a corps come in from the sea.)
 */
function roomNear(g: Game, nat: string, at: Region, record = false): Region | null {
  const full = (r: Region, most = roomAt(g, r, nat)): boolean => placed(g, r, nat).filter((u) => u.kind !== 'eng').length >= most;
  if (!full(at, record ? STACK : undefined)) return at;
  const seen = new Uint8Array(g.regions.length);
  const q = [at.id];
  seen[at.id] = 1;
  for (let i = 0; i < q.length; i++) {
    for (const j of g.regions[q[i]].nb) {
      if (seen[j]) continue;
      seen[j] = 1;
      q.push(j);
      const r = g.regions[j];
      if (r.ctrl === nat && fieldCtrl(g, r) === nat && !full(r)) return r;
    }
  }
  return null;
}

/**
 * Ruling b423 (a rider on 221): a dated row that serves only an operation of the book comes held and stayed where the
 * war does not follow the book, at the stops its list names ('bookOnly' in setup-1939.json; the far side lays the
 * list, china-stop3.json). A hold binds a staff, never a player's own corps.
 */
function bookOnly(g: Game, nat: string, name: string): boolean {
  const list = (setup as { bookOnly?: { stops: number[]; rows: [string, string][] } }).bookOnly;
  return !!list && nat !== g.human && list.stops.includes(stopOf(g)) && list.rows.some(([n, nm]) => n === nat && nm === name);
}

/**
 * Ruling 233, rider b612 (4): the book's rows. At the book's stops of the history dial a computer's staff is sent the
 * roster's dated formations for a scripted theatre's ground where the roster's 'book' list names its country and the
 * ground's owner (Germany's for Italy, 1943: the moves into Italy are history's own, and at those stops the staff
 * makes history's moves). Free play at the looser stops sends none of them, as before, and the player never: his army
 * is his own business. By the record the moves answered the landing (Sicily, and Mussolini's fall): each row waits
 * until the ground's owner has had its leaving in play with an enemy on one of the grounds its leaving event names
 * (the ward's trigger, staff.ts), and then comes at its date, or at once if the date has passed. A quiet ally draws
 * no corps from the other fronts.
 */
type BookRows = { stops: number[]; scripted: [string, string][]; from?: Record<string, string[]> };
const BOOK = (setup as unknown as { book?: BookRows }).book;
function bookRow(g: Game, nat: string, reg: Region | undefined): boolean {
  return !!BOOK && !!reg && nat !== g.human && BOOK.stops.includes(stopOf(g)) && BOOK.scripted.some(([n, owner]) => n === nat && owner === reg.owner);
}
/** True while a book row still waits: the enemy has never yet stood on a ground its ally's leaving event names. The month he first does is kept. */
function bookWaits(g: Game, owner: string): boolean {
  const key = `book:${owner}`;
  if (g.flags[key] === undefined && leavingThreat(g, owner)) g.flags[key] = g.turn;
  return g.flags[key] === undefined;
}

/** True where the war an expedition row names is on: '*' as the second name is a war with anyone. */
const warOn = (g: Game, w: string[]): boolean => (w[1] === '*' ? (g.nations[w[0]]?.atWar.length ?? 0) > 0 : atWar(g, w[0], w[1]));
/** The month the war a row names began, by its declaration (the earliest, for '*'): this month where none was declared. */
function warBegan(g: Game, w: string[]): number {
  const foes = w[1] === '*' ? g.nations[w[0]]?.atWar ?? [] : [w[1]];
  let first = Infinity;
  for (const f of foes) for (const k of [`war:${w[0]}:${f}`, `war:${f}:${w[0]}`]) if (g.flags[k] !== undefined) first = Math.min(first, g.flags[k]);
  return first === Infinity ? g.turn : first;
}

/**
 * The far side: whether a dated row is one the far side's rule for dated rows reads (ruling B7's reach, `postedFor`):
 * its posting lies beyond the old chart, and no book dates its country's rows (stations.ts `bookKept`). Never with
 * the far side off, and never a row posted on the old chart.
 */
const farRow = (g: Game, nat: string, row: any[]): boolean => {
  const posting = g.byName[row[5]];
  return farOn(g) && !!posting && beyondEurope(posting) && !bookKept(g, nat);
};

/**
 * True where the war a row names has begun and is over: it is not on now, and the row has seen it on (`due:N`, kept
 * the month its war first let it come), or one side declared it on the other. A war not on now that nobody has
 * declared is a war not yet begun. (A war on from the first day was declared by nobody: a row that never saw it on
 * reads it as not yet begun.)
 */
function warOver(g: Game, i: number, w: string[]): boolean {
  if (warOn(g, w)) return false;
  if (g.flags[`due:${i}`] !== undefined) return true;
  return Object.keys(g.flags).some((k) => {
    const p = k.split(':');
    if (p.length !== 3 || p[0] !== 'war') return false;
    return w[1] === '*' ? p[1] === w[0] || p[2] === w[0] : (p[1] === w[0] && p[2] === w[1]) || (p[1] === w[1] && p[2] === w[0]);
  });
}

/**
 * The month a dated row's half year is counted from, as the rule of dated rows reads it (`expeditions`): its date,
 * or in the grand campaign the month its war let it come (`due:N`), and never before the month after the last month
 * it was made to wait (`wait:N`: for its war by ruling B7, for a live front, for an offer it is made from).
 */
function rowDue(g: Game, i: number, row: any[], o: any): number {
  const due = g.flags.open === 1 && o.war && g.flags[`due:${i}`] !== undefined ? g.flags[`due:${i}`] : row[0];
  const waited = g.flags[`wait:${i}`];
  return waited !== undefined ? Math.max(due, waited + 1) : due;
}

/**
 * The allies a dated row sent a corps to fight beside: the countries its 'war' names other than its own (Creforce and
 * W Force, for Greece). ai.ts wantsOut reads them: on such a country's ground the corps is its guest.
 */
const AID = new Map<string, string[]>(((setup as any).expeditions as any[] ?? []).filter((row) => Array.isArray(row[6]?.war))
  .map((row) => [`${row[1]}|${row[2]}`, (row[6].war as string[]).filter((x) => x !== row[1] && x !== '*')]));
export const aidOf = (c: Corps): string[] => AID.get(`${c.nat}|${c.name}`) ?? [];

/** The rows another row's 'unless' names: the month each of them lands is kept (the grand campaign, ruling 229, 1c). */
const UNLESS = new Set<string>(((setup as any).expeditions as any[] ?? []).map((row) => row[6]?.unless).filter((x): x is string => !!x));
/**
 * True once the row of that name has landed in this game: in the grand campaign (its month kept), or before it began,
 * under history (a row settled is marked 1; one the grand campaign gave up on, or never sent, 2).
 */
const rowLanded = (g: Game, name: string): boolean => ((setup as any).expeditions as any[] ?? []).some((row, i) => row[2] === name && (g.flags[`came:${i}`] !== undefined || g.flags[`sent:${i}`] === 1));

/**
 * A grand campaign begun after the first day (staff.ts, startOpen): a row whose men history's full-size corps already
 * brought ('carried', the corps row's name: the South Africans and New Zealanders of XIII and XXX Corps, ruling 229,
 * 1c) does not come as well, once that corps has landed under history.
 */
export function carriedRows(g: Game): void {
  const rows = (setup as any).expeditions as any[] ?? [];
  rows.forEach((row, i) => {
    const o = row[6] ?? {};
    if (!o.carried || g.flags[`sent:${i}`]) return;
    if (rows.some((r, j) => r[2] === o.carried && r[1] === row[1] && g.flags[`sent:${j}`] === 1)) g.flags[`sent:${i}`] = 2;
  });
}

/**
 * The grand campaign: where a dated formation lands when its side no longer holds the place meant for it (ruling 229,
 * 1c, as ruling 221's F lands troops lifted off a coast): the nearest harbour its side holds on that front, clear of
 * the enemy and with room, in the same side theatre. Null where there is none, or the place is on no side theatre.
 */
function fallbackHarbour(g: Game, nat: string, at: Region): Region | null {
  const k = THEATRE_OF.get(at.name) ?? -1;
  // (A place on no named front has no fall-back: its row waits for it as before. 1c is the desert's.)
  if (k < 0) return null;
  let best: Region | null = null, bd = Infinity;
  for (const r of g.regions) {
    if (!r.port || r.id === at.id || THEATRE_OF.get(r.name) !== k) continue;
    if (!allied(g, nat, r.ctrl) || hostileUnits(g, r, nat).length || placed(g, r, nat).filter((u) => u.kind !== 'eng').length >= roomAt(g, r, nat)) continue;
    const d = chartDist(g, r, at);
    if (d < bd || (d === bd && best && r.id < best.id)) { bd = d; best = r; }
  }
  return best;
}

/** The corps a country has at sea on a long passage. */
export const voyagesOf = (g: Game, nat: string): Voyage[] => (g.atSea ?? []).filter((v) => v.nat === nat);

/** A country that falls loses its corps at sea with the rest of its army: none of them lands if it is freed later. */
export function endVoyages(g: Game, nat: string): void {
  if (!g.atSea?.length) return;
  g.atSea = g.atSea.filter((v) => v.nat !== nat);
  if (!g.atSea.length) delete g.atSea;
}

/**
 * Puts a corps to sea on a long passage (ruling 229, part 1; the design director's b460: the Cape): it leaves the map
 * from where it stands in its own country, with no loss on the way, and lands in `months` months at `to`, by the
 * arrival rows' own code (landVoyages). Its general stays behind to wait for a command, as for a corps struck off.
 * Who may sail, and when, is its staff's business (ai.ts, reinforce): this only takes it off the map.
 */
export function putToSea(g: Game, c: Corps, to: Region, months: number): void {
  const gen = generalOf(g, c);
  if (gen) { gen.corps = -1; c.general = null; }
  const r = g.regions[c.reg];
  if (c.town) quitTown(g, c); else r.units.splice(r.units.indexOf(c), 1);
  g.corps.splice(g.corps.indexOf(c), 1);
  if (c.job) delete c.job;
  if (c.dug) delete c.dug;
  if (c.relief) delete c.relief;
  (g.atSea ??= []).push({ nat: c.nat, corps: c, from: r.id, to: to.id, due: g.turn + months });
  g.supplyDirty = true;
}

/**
 * Lands a country's corps whose passage ends this month, as its phase opens and after its arrival rows: at the
 * harbour it sailed for, or, where the enemy has taken that, the nearest its side holds on that front (ruling 229,
 * 1c); a full one sends it on to the nearest ground with room. One that can land nowhere stays at sea a month more:
 * it is never lost on the way.
 */
function landVoyages(g: Game, nat: string): void {
  if (!g.atSea?.length) return;
  const n = g.nations[nat];
  for (const v of [...g.atSea]) {
    if (v.nat !== nat || v.due > g.turn) continue;
    let reg: Region | null = g.regions[v.to];
    if (!allied(g, nat, reg.ctrl) || hostileUnits(g, reg, nat).length) reg = atWar(g, nat, reg.ctrl) || allied(g, nat, reg.ctrl) ? fallbackHarbour(g, nat, reg) : null;
    if (reg) reg = roomNear(g, nat, reg) ?? reg;
    if (!reg || !allied(g, nat, reg.ctrl) || hostileUnits(g, reg, nat).length || placed(g, reg, nat).filter((u) => u.kind !== 'eng').length >= roomAt(g, reg, nat)) continue;
    g.atSea = g.atSea.filter((x) => x !== v);
    const c = v.corps;
    c.reg = reg.id;
    reg.units.push(c);
    g.corps.push(c);
    g.supplyDirty = true;
    if (g.human !== null && (allied(g, g.human, nat) || atWar(g, g.human, nat)) && allied(g, g.human, reg.ctrl)) headline(g, 'info', `${c.name} of the ${n.adj} army has landed at ${reg.name}.`, nat, reg.id);
  }
  if (!g.atSea.length) delete g.atSea;
}

/**
 * The far side: a division raised at home and posted abroad is not struck for a lost posting (rider b642 part 2 to
 * ruling 234, the design director's of 15:52Z and 17:10Z of 19 November 2026). A row is a fact of the division's
 * raising: where its posting, beyond the old chart, is not its side's on the date, it comes ashore at its country's
 * nearest held port to the posting, clear of the enemy and with room, and the staff posts it: it holds nothing
 * there. In twenty-four wars the old rule struck Japan a hundred and ten rows for want of the place, the 65th
 * Brigade and the 21st Division's detachment for Luzon in every one. Returns the port, and the worth that lands.
 *
 * A row the record raised in the place itself (`levy`, the historian's mark) keeps the old rule: it waits its half
 * year for the place and is then struck. So does a row that came by land or by air from a country of its own and
 * is not its carrier's to land from the sea (`overland`: China's armies for Burma). A row with no home but one
 * ground comes ashore only at a port of that ground its side holds (`shore`: the KNIL, the Netherlands being
 * occupied). And a row that takes corps already on the board (`from`) keeps the old rule for them: they stay where
 * they stand under their own names, and only what the row adds from home (`adds`, its worth) comes ashore, so that
 * nobody is counted twice. The 18th Division, all of it in Malaya, is not brought to Rangoon's coast; the 56th's
 * body comes from home and the Sakaguchi Detachment stays as it is. A fort, a row that joins a standing corps and a
 * row made out of a defeat are not read. Null with the far side off, and for every row posted on the old chart.
 */
export function ashoreFor(g: Game, nat: string, row: any[], reg: Region | undefined): { at: Region; adds: number | null } | null {
  const o = row[6] ?? {}, posting = g.byName[row[5]];
  if (!farOn(g) || !reg || !posting || reg.id !== posting.id || !beyondEurope(posting) || allied(g, nat, posting.ctrl)) return null;
  if (o.levy || o.overland || o.joins || o.join || o.ifLost || row[3] === 'fort' || (o.from !== undefined && !(Array.isArray(o.from) && o.adds > 0))) return null;
  const mine = (r: Region): boolean => (o.shore ? r.owner === o.shore && allied(g, nat, r.ctrl) : r.ctrl === nat);
  let best: Region | null = null, bd = Infinity;
  for (const r of g.regions) {
    // (Where the book dates this country's rows, stops 1 and 2 of the dial, the record's hand stands over an island's
    // room: its port is counted against five, as it always was. The design director, 13:58Z 25 November 2026, island
    // room 1. Elsewhere the island's own room binds the row, as it binds everyone.)
    if (!r.port || !mine(r) || hostileUnits(g, r, nat).length || friendlyUnits(g, r, nat).filter((u) => u.kind !== 'eng').length >= (bookKept(g, nat) ? STACK : roomAt(g, r, nat))) continue;
    const d = chartDist2(g, r, posting);
    if (d < bd) { bd = d; best = r; }
  }
  return best ? { at: best, adds: o.from !== undefined ? o.adds : null } : null;
}

/**
 * The far side: a row the record raised in the place itself (`levy`) is its own country's, and changes flag with the
 * rest of that ground's corps on its cause, the country's fall, never on a date (ruling 234's note of 22:05Z of 20
 * November 2026, item 6, with the historian's rows 7.3 and 8.4: the KNIL's militia served the Dutch government to the
 * surrender of March 1942). While its country stands the row comes to it, and `keptForces` hands it to the ground's
 * keeper with the others when the country falls. Where the country has fallen before the row's date and the place
 * is that country's own far ground in this country's keeping, the row comes under the keeper's flag, marked with the
 * country that raised it as the kept corps are. True for such a row, read for the keeper.
 */
function keptLevy(g: Game, nat: string, row: any[]): boolean {
  const own = g.nations[row[1]], reg = g.byName[row[5]];
  return farOn(g) && !!row[6]?.levy && !!own && !own.alive && !!reg && reg.owner === row[1] && reg.ctrl === nat && !!keepersOf(g, reg)?.includes(nat);
}

/**
 * The stretch of a country's ground a province stands on: every province of the same owner joined to it by land.
 * It is what "the same theatre" means beyond the old chart, where no side theatre is drawn (theatres.json names the
 * old chart's four): the far side's fronts are read the same way, each country's ground a front of its own, Burma's
 * apart from Malaya's (staff.ts `fronts`), and the near harbour of ruling 221 (F) is asked on the ground of one
 * country too (`nearHarbour`). Luzon and Mindanao are one stretch, the Philippines; Malaya is one, Burma with India
 * another, British Borneo a third; Guam is its own.
 */
function stretchOf(g: Game, at: Region): Set<number> {
  const seen = new Set<number>([at.id]);
  const q = [at.id];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!seen.has(j) && g.regions[j].owner === at.owner) { seen.add(j); q.push(j); }
  return seen;
}

/** The corps on the board a dated row would break up to find its men: those of its country, living, that its `from` list names. */
const rowTakes = (g: Game, nat: string, o: any): Corps[] => (Array.isArray(o?.from) ? g.corps.filter((x) => x.nat === nat && !x.dead && o.from.includes(x.name)) : []);

/**
 * The far side: a dated row posted on ground its country's side does not hold (the design director's ruling B7 of
 * 25 November 2026, design-director/pacific-japan-1125.md; one rule for every nation under the far switch).
 *
 *   where   It forms at the harbour its country holds nearest the posting in the same theatre (`stretchOf`: the
 *           posting's country's ground, joined by land), with no enemy in it, room and the sea open (`nearHarbour`,
 *           ruling 221 F's own asking), else at its capital, and whoever leads it ships it from there. It holds
 *           nothing in place where it forms: the place it was to hold is not its side's.
 *   when    A row the record raised for an operation forms when that war has begun, never on its date. A row that
 *           names its war (`war`) waits for it as it always did. A row that names none, posted on the ground of a
 *           country its own is at peace with, was raised to take that ground: it waits until its country is at war
 *           with whoever holds the posting, and its half year is counted from that month (`waits`). As built the 65th
 *           Brigade for Lingayen came in January 1942 with America at peace.
 *
 * As built a player whose capital lies beyond the old chart had every row posted on ground his country did not OWN
 * formed at his capital: for Japan 57 rows of 86 at Tokyo, 26 of them posted on ground in China that Japan held, with
 * a sea lift of four corps a month. A computer's rows came ashore at its nearest held port anywhere on the chart
 * (`ashoreFor`, rider b642 part 2), on their dates.
 *
 * Who is read. Every country's rows posted beyond the old chart, the player's and the computer's, at stops 3 to 5
 * of the history dial; at stops 1 and 2 only where no book dates that country's rows (stations.ts `bookKept`: the
 * player's own country, any country in a war where the player is the book's invader, and every country once the
 * book is closed in the theatre). Where the book dates a computer's rows they come as the book had them: `ashoreFor`
 * stands, for the dated operations that sail them (ruling B7's own words: the record's book stays the book).
 *
 * Who is not read, and keeps the rule it had: a posting on the old chart (ruling 229, 1c: the side theatres' own
 * near harbour); a posting its side holds, its own or an ally's or a client's (it forms there, or at home where the
 * friend is at peace, as before); a local levy, a row that came overland, a row with no home but one ground
 * (`shore`), a fort, a row that joins a standing corps, a row made out of a defeat. A computer's row that takes corps
 * already on the board (`from`) moves no living corps off to an unheld posting: they stay where they stand, and only
 * what the row adds from home (`adds`) comes; the player's is an offer (ruling B8, `rowOffers`), and the offer forms
 * by this rule.
 *
 * (That is B7's own reading, not a departure from it: the design director's ruling of 13:39Z, 25 November 2026, on
 * the batch 1 readings, point 1 of the dated rows: "B7 says where a row FORMS. A row that moves a living corps moves
 * it only to ground its side holds; to unheld ground the corps stays where it stands." A row made of corps already
 * on the board forms nothing new where it is posted: it moves the corps that are its men, and to a posting its side
 * does not hold they are not moved. The earlier rulings pinned by tests stand on the same reading: the 18th Division
 * is not brought to Rangoon's coast (17:10Z, 19 November); with Hsuchow lost the garrison stays whole and its
 * detachment waits its half year and is struck (17:23Z); a row let go by a live front has its half year for its
 * posting from that month, and its corps stays as it is (05:35Z, 21 November). Read the other way, a computer's
 * 18th Division left the south for Japan's home islands the month after Singapore fell, with Rangoon still
 * Britain's. The player's offer forms by the harbour rule because taking it is his own order: he may take it at home
 * at once or wait for the posting and take it there, which is every move the computer has and one more.)
 *
 * Returns null where the row is not read. Else `waits` (its war has not begun), or the place (`at`, null where its
 * country holds neither a harbour in the theatre nor its capital), the worth that comes where that is not the whole
 * row (`adds`), and `takes` where it is the player's row that would take his corps.
 */
export function postedFor(g: Game, nat: string, row: any[], reg: Region | undefined): { at: Region | null; adds: number | null; waits: boolean; takes: boolean } | null {
  const o = row[6] ?? {}, posting = g.byName[row[5]];
  if (!farOn(g) || !reg || !posting || reg.id !== posting.id || !beyondEurope(posting) || allied(g, nat, posting.ctrl)) return null;
  if (o.levy || o.overland || o.shore || o.joins || o.join || o.ifLost || row[3] === 'fort' || bookKept(g, nat)) return null;
  const body = Array.isArray(o.from) && o.adds > 0;
  const takes = o.from !== undefined && !body;
  if (takes && !(nat === g.human && rowTakes(g, nat, o).length > 0)) return null;
  if (!o.war && !atWar(g, nat, posting.ctrl)) return { at: null, adds: null, waits: true, takes };
  const ground = stretchOf(g, posting), cap = g.nations[nat].capital;
  const home = cap >= 0 && g.regions[cap].ctrl === nat ? roomNear(g, nat, g.regions[cap]) ?? g.regions[cap] : null;
  return { at: nearHarbour(g, nat, posting, (r) => ground.has(r.id)) ?? home, adds: body ? o.adds : null, waits: false, takes };
}

/**
 * Free play: where a dated row is put down this month, read with nothing changed. `at` is the place its row names;
 * the answer is the place it forms at (`reg`), whether that is somewhere other than a posting lost (`fell`: it holds
 * nothing in place there), the port a row is brought ashore at for a lost posting and the worth that lands
 * (`ashore`), and, on the far side, whether its war has not begun (`waits`) and whether it is the player's row that
 * takes his corps, formed away from its posting (`shifted`): `postedFor`.
 */
function rowPlace(g: Game, nat: string, row: any[], o: any, at: Region): { reg: Region; fell: boolean; ashore: { at: Region; adds: number | null } | null; waits: boolean; shifted: boolean } {
  let reg = at, fell = false, shifted = false;
  let ashore: { at: Region; adds: number | null } | null = null;
  // A country's own army, raised on its own soil, forms up at the capital if the place meant for it is already lost.
  const cap = g.nations[nat].capital;
  if (reg && reg.owner === nat && !allied(g, nat, reg.ctrl) && cap >= 0) reg = g.regions[cap];
  // Nor is it put down in a friendly country that is at peace: it forms at home instead.
  if (reg && reg.ctrl !== nat && allied(g, nat, reg.ctrl) && !sharesWar(g, nat, reg.ctrl) && cap >= 0) reg = g.regions[cap];
  // A place its side has lost to the enemy: it lands at the nearest harbour its side holds on that front, and is
  // not cancelled for the loss (ruling 229, 1c). It holds nothing in place there: the place it was to hold is gone.
  // (A place held by a country it is not at war with, Vichy's or a neutral's, is waited for as before.)
  if (reg && !allied(g, nat, reg.ctrl) && atWar(g, nat, reg.ctrl) && !o.joins && !o.join && row[3] !== 'fort') {
    const h = fallbackHarbour(g, nat, reg);
    if (h) { reg = h; fell = true; }
  }
  // (The far side: a row posted on ground its side does not hold forms at its country's nearest held harbour in the
  // same theatre, else at its capital, and waits for its war: ruling B7, `postedFor`. Never with the far side off.)
  const posted = fell ? null : postedFor(g, nat, row, reg);
  if (posted) {
    if (posted.waits) return { reg, fell, ashore, waits: true, shifted };
    if (posted.at && posted.takes) { reg = posted.at; fell = true; shifted = true; }
    else if (posted.at) { ashore = { at: posted.at, adds: posted.adds }; reg = posted.at; fell = true; }
    return { reg: reg && !o.join && !o.joins && row[3] !== 'fort' && allied(g, nat, reg.ctrl) ? roomNear(g, nat, reg, true) ?? reg : reg, fell, ashore, waits: false, shifted };
  }
  // The wider world: a player whose homeland lies across the ocean has such an army form at home, and shipping
  // it is his business. (A computer's staff ships the corps it raises itself, and is sent none of these: above.)
  // (The far side: never a row posted on ground he holds himself, whoever owns it: it forms there. Ruling B7: Japan's
  // rows for Canton and Hainan, which stood at Tokyo. An ally's ground is as it was: his army is his to ship.)
  if (worldOn(g) && nat === g.human && cap >= 0 && beyondEurope(cap) && g.regions[cap].ctrl === nat && reg && reg.owner !== nat && !(reg.ctrl === nat && farOn(g)) && !o.from && !o.joins && row[3] !== 'fort')
    reg = roomNear(g, nat, g.regions[cap]) ?? g.regions[cap];
  // (The far side: a division raised at home and posted abroad is not struck for a lost posting. On its date it
  // comes ashore at its country's nearest held port to the posting and the staff posts it: rider b642 part 2 to
  // ruling 234, `ashoreFor`. Nobody is brought ashore with the far side off. Since ruling B7 it is the rule of the
  // rows a book dates alone: `postedFor` reads every other.)
  if (!fell) {
    ashore = ashoreFor(g, nat, row, reg);
    if (ashore) { reg = ashore.at; fell = true; }
  }
  // A full province sends it on to the nearest ground with room. (The record's own place is counted against five on
  // an island of the Pacific too, `roomNear`'s `record`: form B3 (c). So is it above, for a row `postedFor` places.)
  if (reg && !o.join && !o.joins && row[3] !== 'fort' && allied(g, nat, reg.ctrl)) reg = roomNear(g, nat, reg, true) ?? reg;
  return { reg, fell, ashore, waits: false, shifted };
}

/** For the measuring harness: told of each dated row struck off unsent when its half year is out. Nothing in the game listens. */
export const struckWatch: { tell: ((e: { turn: number; due: number; nat: string; name: string; kind: string; divs: number; place: string; owner: string | null; holder: string | null; to: string | null; toHolder: string | null; war: boolean; until: boolean; ifLost: boolean; joins: string | null; hold: boolean }) => void) | null } = { tell: null };

// ---- Rows that take a corps from the player: an offer, never a silent removal (ruling B8) ---------------------------
//
// The design director's ruling B8 of 25 November 2026 (design-director/pacific-japan-1125.md), whole world only. A
// dated row made from corps on the board (`from`: the 25th Army's 5th Division out of the 21st Army's) struck those
// corps off wherever they stood as it formed, the player's as a computer's: 65 of Japan's 152 dated rows. A human
// seat is never moved automatically. So for the player's own country such a row is an OFFER:
//   made     in the month the rule would have formed the row, with everything else it waits for met (its date, its
//            war, a place with room), and only while a corps it would take is on the board. Nothing is taken.
//   open     it stands until he answers, through the rest of the row's own half year: the half year a computer's
//            row has to form before it is struck (`rowDue`, counted from its date, or from the month its war let it
//            come). The month's offers are one list on the month card (the 3D game's TC.offers), take all or pick,
//            never a card each. (As first built no half year ran against an offer: it stood until he answered. The
//            design director's ruling of 13:39Z, 25 November 2026, on the batch 1 readings, point 5 of the dated
//            rows: "The half-year wait is only for a war not yet begun, and holds for both: the player's offer stays
//            in the list for the same half year.")
//   taken    it does what the rule did before: the corps it names are struck off and the row forms, where the rule
//            puts it that month (`rowPlace`; away from an unheld posting by ruling B7). Where it cannot form that
//            month (no room) he is told why and the offer stands.
//   declined the row never comes, and his corps stay as they are under their own names.
//   lapses   when a corps it needed is gone from the board, when the war it was raised for is over, or when its
//            last month is past (its row's own `until`, or the end of its half year, whichever comes first:
//            `offerLast`): he is told. (An offer whose war was over stood on the card for good and could never be
//            taken: the mending of 25 November 2026.) A war that is over strikes the computer's row in the same month
//            and on the same reach: one rule for both seats (the same ruling, point 5: "A row whose war is over lapses
//            at once, the player's offer and the computer's row alike"; `warOver`).
//   sisters  a row whose men another row releases by cutting a corps (`cut`), or which is itself made from the
//            formation another row would make (`from`: the 18th Division for Rangoon out of the 25th Army's 18th
//            Division), waits on his answer while that other row stands as an open offer, and no half year runs
//            against it for the months he takes (`sourceAsked`). Where he declines that other row, or its offer
//            lapses, and the corps it would have been made from are still his, the row made from it never comes:
//            its men are the corps he kept, and nobody is counted twice (`menKept`). The offer says which later
//            rows he gives up by declining (`then`).
// A computer's rows are as they were. The state is three flags a row: `offer:N` the month it was made, `offer:n:N`
// how many corps it named, and `unmade:N` the month he declined it or it lapsed. With the far side off no offer is
// ever made and none of them is written or read.
//
// The end of a loan is the same case where a division awaits the lent corps (the ruling's own words: "rows that take
// a corps from the player ... never a silent removal"; the reviewer's finding of 25 November 2026). A regiment only
// lent to a campaign (`until`) left the map when its time was up (`lentOver`) and went home to its division
// (`comeHome`), the player's as a computer's: Japan's 1st and 6th Tank Regiments the month Singapore was his, the
// 7th with Bataan. For the player's own country that is an offer in the same list:
//   made     in the month the loan is over, by the rule that always decided it (`lentOver`). Nothing leaves.
//   taken    the regiment goes home as the rule had it go: off the map, its worth into its division one for one,
//            or remembered for a division still to be formed (`comeHome`).
//   declined it stays his for good under its own name, and is never asked for again. Its division stays short of
//            its share while the regiment lives, as while it was lent (`awaited`), so nobody is counted twice.
//   gone     where the regiment is destroyed while the offer stands there is nothing left to ask about.
// Two flags a row: `leave:N` the month it was made, `kept:N` once he has declined. A computer's loans end as they did.
//
// Every other loan ends as it did, for the player too, and that is put to the design director with nothing built: a
// corps the main army's order of battle counts elsewhere (the South Africans lent to East Africa, the panzer
// divisions lent to the Balkans) cannot be kept without being counted twice, and a garrison whose record ends in a
// surrender (`ends`) was never a loan. Only where a division's row awaits the regiment does the game know where its
// men are counted, and there the choice is a real one.

const offerKey = (i: number): string => `offer:${i}`;
const offerCount = (i: number): string => `offer:n:${i}`;
const offerOpen = (g: Game, nat: string, i: number): boolean => farOn(g) && nat === g.human && g.flags[offerKey(i)] !== undefined;
const offerShut = (g: Game, i: number): void => { delete g.flags[offerKey(i)]; delete g.flags[offerCount(i)]; };
/**
 * A row of the formations list (startPhase, `formRow`) has its number in the one list of offers after the dated
 * rows, so that an offer's id, its flags and the 3D game's calls name either list alike: Oahu's split is offered by
 * the same machinery (the design director's ruling of 13:39Z, 25 November 2026, point 3).
 */
const formId = (k: number): number => (((setup as any).expeditions as any[]) ?? []).length + k;
/** The row an offer's id names: a dated row, or a row of the formations list; and which. */
function offerRow(id: number): { row: any[]; form: boolean } | null {
  const E = ((setup as any).expeditions as any[]) ?? [];
  if (!Number.isInteger(id) || id < 0) return null;
  if (id < E.length) return { row: E[id], form: false };
  const f = ((setup as any).formations as any[] ?? [])[id - E.length];
  return f ? { row: f, form: true } : null;
}

/** The month the player declined a row offered to him, or its offer lapsed: the formation was never made, by his answer or for want of one. */
const unmadeKey = (i: number): string => `unmade:${i}`;
/** The places in the list of dated rows of a country's rows that would make a formation of one of these names. */
const makersOf = (nat: string, names: string[]): number[] => {
  const out: number[] = [];
  ((setup as any).expeditions as any[] ?? []).forEach((r, j) => { if (r[1] === nat && names.includes(r[2])) out.push(j); });
  return out;
};

/**
 * True while a formation this row waits on stands as an offer the player has not answered: the row that would make
 * its cut (`cut`), with no row having made it; or, for a row made from formations none of which is on the board
 * (`from`, with no body of its own), the row that would make one of them. Such a row waits on his answer and no half
 * year runs against it. As built the 21st Division's detachment for Bataan was struck with the offer to cut
 * Hsuchow's garrison unanswered, and the 18th Division formed whole at Rangoon, taking nobody, with the offer to
 * form the 25th Army's 18th Division still open. Never with the far side off, and never for a computer.
 */
function sourceAsked(g: Game, nat: string, o: any): boolean {
  if (!farOn(g) || nat !== g.human) return false;
  const cut = o.cut ? (Array.isArray(o.cut) ? (o.cut as string[]) : [o.cut as string]) : [];
  if (cut.length && !cut.some((nm) => rowLanded(g, nm)) && makersOf(nat, cut).some((j) => offerOpen(g, nat, j))) return true;
  return Array.isArray(o.from) && !(o.adds > 0) && rowTakes(g, nat, o).length === 0 && makersOf(nat, o.from).some((j) => offerOpen(g, nat, j));
}

/**
 * True for the player's row made from formations none of which is on the board, where one of them was offered to
 * him and never made (declined, or its offer lapsed: `unmade`) and the corps IT would have been made from still
 * stand on his board, however many such links back. The row's men are those corps, kept under their own names: the
 * row never comes (ruling B8: declined, his corps stay as they are; and nobody is counted twice). As built it came
 * whole and took nobody: declining the 25th Army's 18th Division and then holding Rangoon gave him the 18th Division
 * twice over. Where the kept corps are gone too, the row is as it always was. Never with the far side off, and never
 * for a computer.
 */
function menKept(g: Game, nat: string, o: any, deep = 0): boolean {
  if (!farOn(g) || nat !== g.human || !Array.isArray(o?.from) || o.adds > 0 || rowTakes(g, nat, o).length > 0) return false;
  const rows = (setup as any).expeditions as any[];
  return makersOf(nat, o.from).some((j) => g.flags[unmadeKey(j)] !== undefined && !rowLanded(g, rows[j][2])
    && (rowTakes(g, nat, rows[j][6]).length > 0 || (deep < 8 && menKept(g, nat, rows[j][6], deep + 1))));
}

/** The offer is made: the month, and how many of his corps it names. */
function offerMade(g: Game, nat: string, i: number, o: any): void {
  g.flags[offerKey(i)] = g.turn;
  g.flags[offerCount(i)] = rowTakes(g, nat, o).length;
}

/**
 * The last month in which an offer may be answered: the month before its row's own last month (`until`, the month
 * the rule stops reading the row), or the last month of the row's half year (`rowDue`: a computer's row not formed
 * by then is struck), whichever comes first. The rule's own reading of when an offer lapses for time, not a second
 * date: `offerGone` reads this, and the offers page shows it.
 *
 * (The half year runs against an offer on the far side's rule's own reach and no further: a dated row `farRow` reads
 * (ruling B7's reach: posted beyond the old chart, no book dating its country's rows), and a row of the formations
 * list (Oahu's split). The design director's ruling of 13:39Z, 25 November 2026, point 5 of the dated rows, is built
 * "for every dated row the far side's rule reaches": an offer of a row posted on the old chart (the Fifth Army's II
 * Corps at Naples, the First Parachute Army in Gelderland) keeps what it had, its row's own `until` where it has one,
 * and otherwise no last month: it stands until he answers. As first mended the half year reached those too: the
 * skeptic's review of 25 November 2026.)
 */
function offerLast(g: Game, nat: string, row: any[], i: number): number | null {
  const o = row[6] ?? {};
  if (!offerRow(i)?.form && !farRow(g, nat, row)) return o.until ? o.until - 1 : null;
  const half = rowDue(g, i, row, o) + 6;
  return o.until ? Math.min(o.until - 1, half) : half;
}

/**
 * Why an open offer has lapsed, in plain words: its last month is past (`offerLast`), the war it was raised for is
 * over, or a corps it needed is gone from the map (said by the count: "the corps" where it named one, "one of the
 * 3 corps", "2 of the 3 corps", "the 3 corps"). Null while it stands.
 */
function offerGone(g: Game, nat: string, row: any[], i: number): string | null {
  const o = row[6] ?? {}, there = rowTakes(g, nat, o).length, named = g.flags[offerCount(i)] ?? 0, last = offerLast(g, nat, row, i);
  if (last !== null && g.turn > last) return `The offer to form ${row[2]} has lapsed: its time has passed.`;
  if (o.war && !warOn(g, o.war)) return `The offer to form ${row[2]} has lapsed: the war it was raised for is over.`;
  // (A row of the formations list takes its strength out of the corps it names, in their order, and does not come
  // only where none of them is left (`formRow`): its offer lapses there and nowhere sooner. As first built it lapsed
  // when any one of them was gone, so that a player's America who took the 24th Infantry Division first, emptying
  // the Hawaiian Division, lost the offer of the 25th, though the 298th Infantry still stood to form it as the rule
  // would have: the skeptic's review of 25 November 2026.)
  if (offerRow(i)?.form) return there > 0 || named === 0 ? null : `The offer to form ${row[2]} has lapsed: ${named === 1 ? 'the corps' : `the ${named} corps`} it was to be formed from ${named === 1 ? 'is' : 'are'} no longer on the map.`;
  if (there >= named) return null;
  const lost = named - there;
  const which = named === 1 ? 'the corps' : `${lost === named ? 'the' : lost === 1 ? 'one of the' : `${lost} of the`} ${named} corps`;
  return `The offer to form ${row[2]} has lapsed: ${which} it was to be formed from ${lost === 1 ? 'is' : 'are'} no longer on the map.`;
}

/** An open offer's month: it lapses where `offerGone` says so, and he is told; else it stands, with any more of his corps it now names counted in. */
function offerStands(g: Game, nat: string, row: any[], i: number): string | null {
  const gone = offerGone(g, nat, row, i);
  if (!gone) { g.flags[offerCount(i)] = Math.max(g.flags[offerCount(i)] ?? 0, rowTakes(g, nat, row[6] ?? {}).length); return null; }
  offerShut(g, i);
  g.flags[`sent:${i}`] = 2;
  g.flags[unmadeKey(i)] = g.turn;
  headline(g, 'info', gone, nat);
  return gone;
}

/** One of the player's corps an offer would take: where it stands and its size. */
export interface OfferTakes { id: number; name: string; kind: Kind; region: number; divs: number; men: number }
/**
 * An offer as either interface shows it: the row's place in the list of dated rows (`id`), what would form and how
 * big, the month it was made, where it would form this month (`region`, -1 where nowhere; `joins` the corps it would
 * strengthen where it forms no corps of its own), the corps it would take, and why it cannot be taken this month
 * (`why`, null where it can). `then` names his later rows that are made from this formation or cut from it, which he
 * gives up by declining it (`menKept`). The end of a loan is in the same list (a regiment its division awaits,
 * `loanStays`): `loan` says in plain words what it was lent for and what taking and declining do (null for a
 * formation offered); its name, size and place are the lent regiment's own, and that regiment is the one it takes.
 * `last` is the last month in which it may be answered (`offerLast`): null for the end of a loan, and for an offer
 * of a row on the old chart with no `until` of its own, which stand until he answers.
 */
export interface RowOffer {
  id: number; name: string; kind: Kind; divs: number; men: number; since: number; last: number | null; region: number; joins: string | null; takes: OfferTakes[]; why: string | null; then: string[]; loan: string | null;
  /**
   * What becomes of the men of the corps it takes beyond the formation's own (the offers page's line for them):
   * 'lost' where the corps taken are struck off whole and the row forms at its own size (`expeditions`: nothing goes
   * back to the depots), 'stay' where the row takes only its own strength out of them and the rest stay in the last
   * corps it draws on (the formations list, `formRow`). Null for the end of a loan.
   */
  rest: 'lost' | 'stay' | null;
  /**
   * The end of a loan, from the row's own data: what the regiment was lent to do (`take Singapore`, `hold ...`), the
   * division it goes back into, and how: 'into' the division on the map, 'later' remembered for a division still to be
   * formed, 'lost' where that division never came or is gone. Null for a formation offered.
   */
  lent: { to: string; division: string; goes: 'into' | 'later' | 'lost' } | null;
}

/** One open offer of the player's, read with nothing changed. */
function offerView(g: Game, nat: string, row: any[], i: number): RowOffer {
  const o = row[6] ?? {}, posting = g.byName[row[5]];
  const divs = g.flags.open === 1 && o.openDivs !== undefined ? o.openDivs : row[4];
  const takes = rowTakes(g, nat, o).map((c) => ({ id: c.id, name: c.name, kind: c.kind, region: c.reg, divs: c.divs, men: men(c) }));
  const old = o.joins ? g.corps.find((x) => x.nat === nat && x.name === o.joins && !x.dead) : undefined;
  const P = rowPlace(g, nat, row, o, posting);
  const reg = old ? g.regions[old.reg] : !P.waits && P.reg && allied(g, nat, P.reg.ctrl) ? P.reg : null;
  // Why it cannot be formed this month: the gates the row's own reading keeps (`expeditions`), in its order.
  const why = ((): string | null => {
    const gone = offerGone(g, nat, row, i);
    if (gone) return gone;
    if (o.stops && !(o.stops as number[]).includes(stopOf(g))) return 'It is not formed at this setting of the history dial.';
    if (P.waits) return `It forms once you are at war with ${theName(g.nations[posting.ctrl])}, which holds ${posting.name}.`;
    if (o.cut && !(Array.isArray(o.cut) ? (o.cut as string[]) : [o.cut as string]).some((nm) => rowLanded(g, nm))) return 'The formation its men are released from has not been formed.';
    if (old) return null;
    if (!reg) return `There is nowhere for it to form: ${posting?.name ?? row[5]} is not yours, and you hold no harbour on that ground, nor your capital.`;
    if (!o.join && placed(g, reg, nat).filter((u) => u.kind !== 'eng').length >= STACK) return `There is no room for another corps at ${reg.name}, or near it.`;
    return null;
  })();
  // His rows still to come at this stop of the dial that are made out of this formation: cut from it, or made from
  // it with no other corps of theirs on the board (one that names a corps he keeps by declining is offered still).
  const names = (x: unknown): string[] => (Array.isArray(x) ? (x as string[]) : typeof x === 'string' ? [x] : []);
  const then = [...new Set((((setup as any).expeditions as any[]) ?? []).filter((r, j) => r[1] === nat && j !== i && r[2] !== row[2] && !g.flags[`sent:${j}`]
    && (!r[6]?.stops || (r[6].stops as number[]).includes(stopOf(g)))
    && (names(r[6]?.cut).includes(row[2]) || (Array.isArray(r[6]?.from) && !(r[6].adds > 0) && r[6].from.includes(row[2])
      && rowTakes(g, nat, { from: (r[6].from as string[]).filter((nm) => nm !== row[2]) }).length === 0))).map((r) => r[2] as string))];
  return { id: i, name: row[2], kind: row[3], divs, men: Math.round(divs * (o.str ?? 1) * MEN_PER_DIV), since: g.flags[offerKey(i)], last: offerLast(g, nat, row, i), region: reg ? reg.id : -1, joins: old ? old.name : null, takes, why, then, loan: null, rest: 'lost', lent: null };
}

const leaveKey = (i: number): string => `leave:${i}`;
const keptKey = (i: number): string => `kept:${i}`;
const leaveOpen = (g: Game, nat: string, i: number): boolean => farOn(g) && nat === g.human && g.flags[leaveKey(i)] !== undefined;
/** The player's corps a lent row put on the map, while it lives. */
const lentCorps = (g: Game, nat: string, i: number): Corps | undefined => { const id = g.flags[`lent:${i}`]; return id > 0 ? g.corps.find((x) => x.id === id && x.nat === nat && !x.dead) : undefined; };

/** The place in the list of dated rows of the division whose row awaits a lent corps of that name (`awaits`), or -1 where none does. */
const awaitedBy = (nat: string, name: string): number => (((setup as any).expeditions as any[]) ?? []).findIndex((r) => r[1] === nat && Array.isArray(r[6]?.awaits) && r[6].awaits.includes(name));

/**
 * The far side: whether the corps of a lent row stays on the map this month for the player's sake (ruling B8: the
 * end of a loan takes no corps of his off the map; it is an offer). Only a regiment a division's row awaits is
 * asked about: there the game knows where its men are counted, so keeping it counts nobody twice. False for every
 * other lent row, for a computer's row, with the far side off, where the corps is no longer on the map (nothing is
 * left to ask about), and where he has just said it may go (`taken`): the loan then ends as it always did. True
 * where he has declined (`kept`), and where the loan is over and the offer is made or stands; the month it is first
 * over is the month it is made.
 */
function loanStays(g: Game, nat: string, row: any[], o: any, i: number, taken: boolean): boolean {
  if (!farOn(g) || nat !== g.human || awaitedBy(nat, row[2]) < 0) return false;
  if (!lentCorps(g, nat, i)) { delete g.flags[leaveKey(i)]; return false; }
  if (g.flags[keptKey(i)]) return true;
  if (taken) return false;
  if (g.flags[leaveKey(i)] === undefined && lentOver(g, nat, row, o)) g.flags[leaveKey(i)] = g.turn;
  return true;
}

/**
 * The end of a loan as the list shows it: the lent regiment, where it stands, and what letting it go and keeping it
 * do. What letting it go does is said as `comeHome` will do it: into its division where that is on the map,
 * remembered for it where its row has still to form, and lost to it where that row never came or the division is
 * gone. Null where the regiment is gone.
 */
function loanView(g: Game, nat: string, row: any[], i: number): RowOffer | null {
  const o = row[6] ?? {}, c = lentCorps(g, nat, i), k = awaitedBy(nat, row[2]);
  if (!c || k < 0) return null;
  const home = ((setup as any).expeditions as any[])[k];
  const there = g.flags[`sent:${k}`] === 1 && g.corps.some((x) => x.nat === nat && x.name === home[2] && !x.dead && x.divs === home[4]);
  const list = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
  const to = typeof o.takes === 'string' ? `take ${o.takes}` : Array.isArray(o.holds) ? `hold ${list(o.holds as string[])}` : 'a campaign';
  const lent = `It was lent to ${to}, and its loan is over`;
  const goes = there ? `it goes home to its division, ${home[2]}`
    : g.flags[`sent:${k}`] === undefined ? `it goes home, and joins its division, ${home[2]}, when that is formed`
      : `it leaves the map, for its division, ${home[2]}, is not there to take it`;
  const mine = { id: c.id, name: c.name, kind: c.kind, region: c.reg, divs: c.divs, men: men(c) };
  return { id: i, name: c.name, kind: c.kind, divs: c.divs, men: men(c), since: g.flags[leaveKey(i)], last: null, region: c.reg, joins: null, takes: [mine], why: null, then: [], loan: `${lent}. Let go, ${goes}; kept, it stays under your command, and its division stays short of it.`,
    rest: null, lent: { to, division: home[2], goes: there ? 'into' : g.flags[`sent:${k}`] === undefined ? 'later' : 'lost' } };
}

/**
 * One open offer of a row of the formations list (Oahu's split), read with nothing changed: what would form, where
 * (`formPlace`, the formations list's own placing) and which of his corps it would take, in the order the row takes
 * them. It cannot be taken only where it has lapsed or has nowhere to form; it names no later rows (`then`).
 */
function formView(g: Game, nat: string, row: any[], id: number): RowOffer {
  const o = row[6] ?? {}, made = formMade(g, nat, row) ?? [];
  const reg = formPlace(g, nat, row, made);
  const takes = made.map((c) => ({ id: c.id, name: c.name, kind: c.kind, region: c.reg, divs: c.divs, men: men(c) }));
  const why = offerGone(g, nat, row, id) ?? (reg ? null : `There is nowhere for it to form: ${row[5]} is not yours.`);
  // (Its size is what `formRow` would make of it this month: its own strength, and never more than the corps it
  // names now hold. As first built it said the row's size, more than those corps held: the skeptic's review.)
  const held = made.reduce((t, x) => t + x.divs * x.str, 0);
  return { id, name: row[2], kind: row[3], divs: row[4], men: Math.round(Math.min(row[4] * (o.str ?? 1), held) * MEN_PER_DIV), since: g.flags[offerKey(id)], last: offerLast(g, nat, row, id), region: reg ? reg.id : -1, joins: null, takes, why, then: [], loan: null, rest: 'stay', lent: null };
}

/** The player's open offers, oldest first and then in the order of the rows: the month's one list (ruling B8). None with the far side off. */
export function rowOffers(g: Game): RowOffer[] {
  const nat = g.human;
  if (nat === null || !farOn(g) || !g.nations[nat]?.alive) return [];
  const out: RowOffer[] = [];
  ((setup as any).expeditions as any[] ?? []).forEach((row, i) => {
    if (row[1] !== nat) return;
    if (offerOpen(g, nat, i)) out.push(offerView(g, nat, row, i));
    else if (leaveOpen(g, nat, i)) { const v = loanView(g, nat, row, i); if (v) out.push(v); }
  });
  ((setup as any).formations as any[] ?? []).forEach((row, k) => { if (row[1] === nat && offerOpen(g, nat, formId(k))) out.push(formView(g, nat, row, formId(k))); });
  return out.sort((a, b) => a.since - b.since || a.id - b.id);
}

/**
 * For the offers page (the design director's ruling of 13:39Z, 25 November 2026, point 2 of the dated rows: "Until
 * then they leave as today, and the offers page says so"): the player's corps lent to a campaign whose loan's end is
 * not asked of him (`loanStays`: no division's row awaits it), which leave his command by themselves when the loan is
 * over (`lentOver`). Each with what it was lent to do and when it leaves: by its month (`until`) where the date
 * decides it, else when the place it was lent to take is his side's (`takes`) or the last place it was lent to hold is
 * lost (`holds`). Read with nothing changed; none with the far side off.
 */
export function loansEnding(g: Game): { id: number; name: string; until: number | null; takes: string | null; holds: string[] | null }[] {
  const nat = g.human;
  if (nat === null || !farOn(g) || !g.nations[nat]?.alive) return [];
  const out: { id: number; name: string; until: number | null; takes: string | null; holds: string[] | null }[] = [];
  ((setup as any).expeditions as any[] ?? []).forEach((row, i) => {
    const o = row[6] ?? {};
    if (row[1] !== nat || !o.until || awaitedBy(nat, row[2]) >= 0 || !lentCorps(g, nat, i)) return;
    // (By date where `lentOver` reads the date: the book's stops, or a row lent for no place.)
    const byDate = stopDef(g).book || (!o.takes && !o.holds);
    out.push({ id: i, name: lentCorps(g, nat, i)!.name, until: byDate ? o.until : null, takes: !byDate && !o.holds && typeof o.takes === 'string' ? o.takes : null, holds: !byDate && Array.isArray(o.holds) ? o.holds : null });
  });
  return out;
}

/**
 * For the offers page (the same ruling, point 7: "One line on the offers page for a row waiting for its war: yes"):
 * the player's dated rows that have come due and wait for their war to begin, by ruling B7. A row that names its war
 * (`war`) and finds it not yet begun, or one posted on ground a country his is at peace with holds (`postedFor`).
 * `with` is who he must be at war with: the posting's holder, or the other of the two its war names ('*' for any war);
 * `pair` is set where its war is between two others (an ally's war it was raised to join). Read with nothing changed;
 * none with the far side off.
 */
export function rowsAwaitingWar(g: Game): { id: number; name: string; with: string; pair: [string, string] | null; place: string }[] {
  const nat = g.human;
  if (nat === null || !farOn(g) || g.flags.open !== 1 || !g.nations[nat]?.alive || shipsItsOwn(g, nat)) return [];
  const out: { id: number; name: string; with: string; pair: [string, string] | null; place: string }[] = [];
  ((setup as any).expeditions as any[] ?? []).forEach((row, i) => {
    const o = row[6] ?? {};
    if (row[1] !== nat || g.flags[`sent:${i}`] || offerOpen(g, nat, i) || leaveOpen(g, nat, i)) return;
    if ((o.stops && !(o.stops as number[]).includes(stopOf(g))) || (g.turn < row[0] && !(o.war && o.after !== undefined))) return;
    if (o.war) {
      const w = o.war as string[];
      if (warOn(g, w) || warOver(g, i, w)) return;
      const mine = w.includes(nat);
      out.push({ id: i, name: row[2], with: mine ? (w[0] === nat ? w[1] : w[0]) : w[1], pair: mine ? null : [w[0], w[1]], place: row[5] });
      return;
    }
    const posting = g.byName[row[5]];
    if (posting && rowPlace(g, nat, row, o, posting).waits) out.push({ id: i, name: row[2], with: posting.ctrl, pair: null, place: row[5] });
  });
  return out;
}

/**
 * The player says yes to an offer (ruling B8): the row forms as the rule always formed it, the corps it names struck
 * off; or, at the end of a loan, the lent regiment goes home as the rule always had it go. Null where it was done;
 * else the plain reason it could not be this month, with the offer left standing.
 */
export function takeOffer(g: Game, id: number): string | null {
  const nat = g.human, at = offerRow(id);
  // (A row of the formations list, Oahu's split: it forms as that list always formed it, in the month he says yes.)
  if (at?.form) {
    if (nat === null || at.row[1] !== nat || !offerOpen(g, nat, id)) return 'There is no such offer.';
    const gone = offerStands(g, nat, at.row, id);
    if (gone) return gone;
    const why = formView(g, nat, at.row, id).why;
    if (why) return why;
    if (!formRow(g, nat, at.row, id, true)) return 'It cannot be formed this month.';
    offerShut(g, id);
    return null;
  }
  const row = at?.row;
  // (The end of a loan: he lets the regiment go, and it leaves as the rule always had it leave.)
  if (nat !== null && row && row[1] === nat && leaveOpen(g, nat, id)) {
    if (!lentCorps(g, nat, id)) { delete g.flags[leaveKey(id)]; return 'There is no such offer.'; }
    expeditions(g, nat, id);
    return g.flags[`lent:${id}`] === -1 ? null : 'It cannot leave this month.';
  }
  if (nat === null || !row || row[1] !== nat || !offerOpen(g, nat, id)) return 'There is no such offer.';
  const gone = offerStands(g, nat, row, id);
  if (gone) return gone;
  const why = offerView(g, nat, row, id).why;
  if (why) return why;
  expeditions(g, nat, id);
  if (g.flags[`sent:${id}`] !== 1) return 'It cannot be formed this month.';
  offerShut(g, id);
  g.supplyDirty = true;
  return null;
}

/** The player says no to an offer (ruling B8): the row never comes, and his corps stay as they are; or, at the end of a loan, the lent regiment stays his. False where there is no such offer. */
export function declineOffer(g: Game, id: number): boolean {
  const nat = g.human, at = offerRow(id), row = at?.row;
  // (The end of a loan: he keeps the regiment, for good.)
  if (nat !== null && row && !at!.form && row[1] === nat && leaveOpen(g, nat, id)) {
    delete g.flags[leaveKey(id)];
    if (!lentCorps(g, nat, id)) return false;
    g.flags[keptKey(id)] = 1;
    return true;
  }
  if (nat === null || !row || row[1] !== nat || !offerOpen(g, nat, id)) return false;
  offerShut(g, id);
  g.flags[`sent:${id}`] = 2;
  g.flags[unmadeKey(id)] = g.turn;
  return true;
}

/**
 * `take` is the one row the player has just said yes to (ruling B8, `takeOffer`): that row alone is read, and it
 * forms as the rule always formed it. Every other reading gives none, and reads every row.
 */
function expeditions(g: Game, nat: string, take?: number): void {
  const gone: Corps[] = [], down: Corps[] = [];
  ((setup as any).expeditions as any[] ?? []).forEach((row, i) => {
    if (take !== undefined && i !== take) return;
    const booked = g.flags.open === 1 && row[1] === nat && bookRow(g, nat, g.byName[row[5]]);
    // (Nothing is counted twice, b609: a division the roster already names elsewhere is not added again. The corps that
    // holds it, where it still lives, is broken up into the row: the book's 'from' list, read as a row's own.)
    const o0 = booked && BOOK!.from?.[row[2]] ? { ...(row[6] ?? {}), from: [...(row[6]?.from ?? []), ...BOOK!.from[row[2]]] } : row[6] ?? {};
    const o = bookOnly(g, nat, row[2]) ? { ...o0, hold: 1, stay: 1 } : o0;
    // (A country's own call-up counted from its war's start waits for no book month in the grand campaign: ruling 229, part 3.)
    const fromWar = g.flags.open === 1 && !!o.war && o.after !== undefined;
    // (The far side: a fallen country's row raised in the place itself comes under its ground's keeper: `keptLevy`.)
    const kept = row[1] !== nat && keptLevy(g, nat, row);
    if ((row[1] !== nat && !kept) || (!fromWar && g.turn < row[0])) return;
    // A row marked 'open' belongs to the grand campaign alone: history's campaigns keep the order of battle they were measured with.
    if (o.open && g.flags.open !== 1) return;
    // A row marked with stops is a row of the book's own record (ruling 234, riders b579 and b581): it comes only at those stops.
    if (o.stops && !(o.stops as number[]).includes(stopOf(g))) return;
    if (g.flags[`sent:${i}`]) {
      // (A row whose record changes its size by date takes the new size in its month: rider b614 to ruling 234.)
      if (g.flags[`sent:${i}`] === 1) estMonth(g, nat, row);
      // The main army's order of battle already counts these formations: the campaign only had the loan of them.
      const id = g.flags[`lent:${i}`];
      // (The far side: the end of a loan takes no regiment of the player's that its division awaits off the map: it
      // is an offer, and the regiment stays until he lets it go. Ruling B8, `loanStays`. A computer's loan ends as
      // it did, and so does everybody's with the far side off.)
      if (o.until && id > 0 && !loanStays(g, nat, row, o, i, take === i) && (take === i || lentOver(g, nat, row, o))) {
        const c = g.corps.find((x) => x.id === id && x.nat === nat);
        // (The far side: a regiment its division awaits comes home to it: `comeHome`. A row whose record ends in a
        // surrender, `ends`, is told as one: `down`, below.)
        if (c) { (o.ends === 'surrender' && farOn(g) ? down : gone).push(c); comeHome(g, nat, i, row, c); }
        g.flags[`lent:${i}`] = -1;
        if (take === i) delete g.flags[leaveKey(i)];
      }
      // (The far side: a division that awaits lent regiments keeps their shares for them while they are lent: `makeUp`.)
      if (o.awaits && farOn(g)) makeUp(g, nat, row, o);
      return;
    }
    // (The far side: a row the player has been offered stands open until he answers, and nothing is read of it but
    // whether the corps it needs are still there: ruling B8, `offerStands`.)
    if (take !== i && offerOpen(g, nat, i)) { offerStands(g, nat, row, i); return; }
    let reg = g.byName[row[5]];
    let due = row[0];
    let fell = false, shifted = false;
    let ashore: { at: Region; adds: number | null } | null = null;
    // The wider world: a computer-run country that ships its own army across the ocean (world.ts, 'overseas') is sent
    // none of history's dated formations as well: the corps it raises and ships take their place.
    if (shipsItsOwn(g, nat)) return;
    if (g.flags.open === 1) {
      // Free play. An invasion force history put together for a landing of its own planning is nobody's to send.
      // A formation raised for a war waits for that war, however late it comes, and then has its half year.
      const k = reg ? THEATRE_OF.get(reg.name) ?? -1 : -1;
      if (k >= 0 && THEATRE_LIST[k].scripted && row[0] >= (THEATRE_LIST[k].opens ?? 0) && (!booked || bookWaits(g, reg!.owner))) return;
      if (o.war) {
        // (The far side: a row whose war is over lapses at once, the player's and the computer's alike, and never
        // comes: the design director's ruling of 13:39Z, 25 November 2026, on the batch 1 readings, point 5 of the
        // dated rows, "one rule for both sides". As built a computer's row waited here for the war to come again and
        // was then struck for its half year, while the player's offer of it lapsed at once. A war not yet begun is
        // waited for as before, by both. Read on the far side's rows alone, `farRow`: nothing on the old chart.)
        if (!warOn(g, o.war)) {
          if (farRow(g, nat, row) && warOver(g, i, o.war)) { g.flags[`sent:${i}`] = 2; if (nat === g.human && row[1] === nat) g.flags[unmadeKey(i)] = g.turn; }
          return;
        }
        // And it waits for its own country to be in that war: no army turns up in a war its country is not fighting.
        if (!o.war.includes(nat) && !atWar(g, nat, o.war[0]) && !atWar(g, nat, o.war[1])) return;
        // (A call-up is counted from the war's start, but never from before the month the row is first seen: a game
        // saved before ruling 229 counts it from the month it is loaded, and loses none of it for being late. A
        // country overrun before its call-up was ever counted never had it: freed, it returns with its liberation
        // corps alone, ruling 229, 2b.)
        if (fromWar && g.flags[`due:${i}`] === undefined && g.nations[nat].fellOn) { g.flags[`sent:${i}`] = 2; return; }
        due = g.flags[`due:${i}`] ??= fromWar ? Math.max(warBegan(g, o.war), g.turn - 1) + o.after : g.turn;
        if (g.turn < due) return;
      }
      // Where it is put down: the place meant for it, home where that is lost or a friend's at peace, the near
      // harbour of a front lost to the enemy, the port a posted division comes ashore at (`rowPlace`).
      const P = rowPlace(g, nat, row, o, reg);
      // (The far side: a row raised to take the ground of a country its own is at peace with waits for that war, and
      // has its half year from the month it begins: ruling B7, `postedFor`. Never with the far side off.)
      if (P.waits) { g.flags[`wait:${i}`] = g.turn; return; }
      reg = P.reg; fell = P.fell; ashore = P.ashore; shifted = P.shifted;
    }
    // (The far side: a dated row takes no corps standing on a live front of its staff's plan (planfronts.ts): the
    // design director's rulings of 05:30Z and 05:35Z of 21 November 2026 to ruling 234. As built the 18th Division's
    // row formed at Rangoon in its month and took the 25th Army's 18th Division out of Malaya with Singapore still
    // standing, and the 56th's took the Sakaguchi Detachment off the Indies' road. A row with a body of its own
    // (`adds`, the 56th's) comes at that worth and takes nobody, as one brought ashore for a lost posting does. A row
    // with none waits for the front, and its half year is counted from the month the front lets its corps go.)
    let own = false;
    if (Array.isArray(o.from) && !ashore && row[1] === nat && farOn(g) && g.corps.some((x) => x.nat === nat && !x.dead && o.from.includes(x.name) && onLiveFront(g, nat, x))) {
      if (o.adds > 0) own = true;
      else { g.flags[`wait:${i}`] = g.turn; return; }
    }
    // (The far side: a row that waits on a formation the player has been offered and has not answered, the one that
    // makes its cut or the one it is itself made from, waits on his answer and has its half year from the month he
    // gives it: he is never struck a row for the months he took, nor given its men twice. And a row made from a
    // formation he did not make, whose corps he kept, never comes. Ruling B8; the mending of 25 November 2026:
    // `sourceAsked`, `menKept`.)
    if (take !== i && sourceAsked(g, nat, o)) { g.flags[`wait:${i}`] = g.turn; return; }
    if (take !== i && row[1] === nat && menKept(g, nat, o)) { g.flags[`sent:${i}`] = 2; g.flags[unmadeKey(i)] = g.turn; return; }
    const waited = g.flags[`wait:${i}`];
    if (waited !== undefined) due = Math.max(due, waited + 1);
    // (An offer the player takes is not struck for the months he took to answer: it stood open. Ruling B8.)
    const late = g.turn - due > 6 && take !== i;
    if (late || (o.war && !warOn(g, o.war)) || (o.until && g.turn >= o.until)) {
      if (late) {
        g.flags[`sent:${i}`] = g.flags.open === 1 ? 2 : 1;
        const at = g.byName[row[5]];
        struckWatch.tell?.({ turn: g.turn, due, nat, name: row[2], kind: row[3], divs: row[4], place: row[5], owner: at?.owner ?? null, holder: at?.ctrl ?? null, to: reg?.name ?? null, toHolder: reg?.ctrl ?? null, war: !!o.war && !warOn(g, o.war), until: !!o.until && g.turn >= o.until, ifLost: !!o.ifLost, joins: o.joins ?? null, hold: !!o.hold });
      }
      return;
    }
    // (The far side: a detachment whose men a sister row releases, by cutting a corps on the board, comes only once
    // that row has made the cut, at its posting or ashore, so that its men are never counted twice: the design
    // director's of 17:23Z of 19 November 2026 on rider b642 part 2. The 21st Division's detachment for Bataan waits
    // for the row that cuts Hsuchow's garrison; with Hsuchow lost it waits its half year and is struck.)
    // (A detachment cut from a division the record moved at some stops and not at others names each row that may
    // make the cut, and comes once any of them has: the 21st Division's for Bataan, cut at Hsuchow where it stays
    // (stops 3 to 5) or freed when the 17th Division takes over its stretch (the book's stops, far-shores.json '_21st').)
    if (o.cut && farOn(g) && !(Array.isArray(o.cut) ? (o.cut as string[]) : [o.cut as string]).some((nm) => rowLanded(g, nm))) return;
    // An army put together out of a defeat forms only once the ground is really lost: while the nation or an ally
    // still holds any place named, the formations it would be made from fight on where they stand.
    if (o.ifLost && (o.ifLost as string[]).some((nm) => { const r = g.byName[nm]; return !r || allied(g, nat, r.ctrl); })) return;
    // (A row that stands in for another, in a war where that other has already landed, never comes: ruling 229, 1c.)
    if (o.unless && g.flags.open === 1 && rowLanded(g, o.unless)) { g.flags[`sent:${i}`] = 2; return; }
    // (The far side: a row that would take corps of the player's off the board is an offer, never a silent removal:
    // ruling B8 of 25 November 2026. It is made in the month the rule would have formed the row, stands open until he
    // answers, and taken (`take`) it does what the rule always did. A computer's rows are as they were.)
    const asks = farOn(g) && nat === g.human && take !== i && !ashore && !own && rowTakes(g, nat, o).length > 0;
    const old = o.joins ? g.corps.find((x) => x.nat === nat && x.name === o.joins && !x.dead) : undefined;
    if (old) {
      if (asks) { offerMade(g, nat, i, o); return; }
      g.flags[`sent:${i}`] = 1;
      // Corps broken up to find the divisions are struck off, and a corps only lent to the campaign is remembered.
      if (Array.isArray(o.from) && !own) for (const x of g.corps.filter((x) => x.nat === nat && !x.dead && o.from.includes(x.name))) removeCorps(g, x);
      if (o.until) g.flags[`lent:${i}`] = old.id;
      // The new divisions come at full strength; the corps they join is as rested as it was.
      old.str = (old.str * old.divs + row[4]) / (old.divs + row[4]);
      // (They carry numbers of their own, never those of the corps beside it: armies.ts.)
      growCorps(g, old, row[4], row[3]);
      old.name = row[2];
      g.supplyDirty = true;
      if (o.news) headline(g, 'event', o.news, nat, old.reg);
      return;
    }
    // Troops sent to take over a stretch of an ally's line ('join') go in whatever already stands there.
    // (Room is counted as everywhere else: engineers take none. The record's own place is counted against five on an
    // island of the Pacific too, as `roomNear` counts it: a row is not kept waiting, and so struck, by the islands' table.)
    if (!reg || !allied(g, nat, reg.ctrl) || (!o.join && placed(g, reg, nat).filter((u) => u.kind !== 'eng').length >= STACK)) return;
    if (asks) { offerMade(g, nat, i, o); return; }
    g.flags[`sent:${i}`] = 1;
    // (The body of a row brought ashore for a lost posting takes nobody off the board: the corps it would have
    // taken stay where they stand under their own names. Rider b642 part 2. Nor does the body of a row whose corps
    // stand on a live front: `own`, above.)
    if (Array.isArray(o.from) && !ashore && !own) {
      for (const x of g.corps.filter((x) => x.nat === nat && !x.dead && o.from.includes(x.name))) removeCorps(g, x);
      g.supplyDirty = true;
    }
    // (The player's row formed away from its posting with the corps it takes holds nothing in place there either: `shifted`.)
    const as = ashore ? { ...o, hold: undefined, stay: undefined, est: ashore.adds ?? o.est } : shifted ? { ...o, hold: undefined, stay: undefined } : fell ? { ...o, hold: undefined } : o;
    const c = addCorps(g, nat, row[2], row[3], g.flags.open === 1 && o.openDivs !== undefined ? o.openDivs : row[4], reg.id, own ? { ...as, est: o.adds } : as);
    if (o.str !== undefined) c.str = o.str;
    // (The far side: a division that awaits lent regiments forms without the ones that are not there: `formUp`.)
    if (o.awaits && o.str !== undefined && farOn(g)) formUp(g, nat, row, o, c);
    if (kept) c.raised = row[1];
    if (ashore?.adds) c.str = Math.min(c.str, ashore.adds);
    if (own) c.str = Math.min(c.str, o.adds);
    // (A row that takes part of a division already landed leaves that division the weaker by it, in the grand campaign:
    // W Force's 1st Armoured Brigade came out of the 2nd Armoured Division in Egypt, and was counted in both. A
    // division not landed yet comes the weaker when it does.)
    if (g.flags.open === 1 && g.flags[`cut:${i}`] !== undefined) { c.str *= 1 - g.flags[`cut:${i}`]; delete g.flags[`cut:${i}`]; }
    if (g.flags.open === 1 && o.takes) {
      const x = g.corps.find((u) => u.nat === nat && !u.dead && u.name === o.takes.name);
      const j = ((setup as any).expeditions as any[]).findIndex((r2) => r2[1] === nat && r2[2] === o.takes.name);
      if (x) {
        x.str *= 1 - o.takes.share;
        if (nat === g.human && o.takes.leaves) headline(g, 'event', o.takes.leaves.replace('{place}', g.regions[x.reg].name), nat, reg.id);
      } else if (j >= 0 && !g.flags[`sent:${j}`]) g.flags[`cut:${j}`] = o.takes.share;
    }
    if (g.flags.open === 1 && UNLESS.has(row[2])) {
      g.flags[`came:${i}`] = g.turn;
      // (A row standing in for this one, landed already, goes with it: the same men cannot stand in two places.)
      ((setup as any).expeditions as any[] ?? []).forEach((r2, j) => {
        if (r2[6]?.unless !== row[2] || r2[1] !== nat) return;
        if (!g.flags[`sent:${j}`]) g.flags[`sent:${j}`] = 2;
        const x = g.corps.find((u) => u.nat === nat && !u.dead && u.name === r2[2]);
        if (!x) return;
        // (The player is told where his corps has gone, in the row's own words: the UI director's.)
        if (nat === g.human && r2[6].leaves) headline(g, 'event', r2[6].leaves.replace('{place}', g.regions[x.reg].name), nat, reg.id);
        removeCorps(g, x);
        g.supplyDirty = true;
      });
    }
    if (o.from && !Array.isArray(o.from)) {
      // A formation put together where it stands, out of the army around it, is not itself broken up for the next one.
      const here = theatreName(reg) === o.from;
      if (here) g.flags[`formed:${c.id}`] = 1;
      draw(g, o.of ?? nat, o.from, row[4], here ? c : undefined);
    }
    if (o.until) g.flags[`lent:${i}`] = c.id;
    const the = /^(the |[A-Z] Force|Creforce)/i.test(c.name) ? '' : 'the ';
    // Troops come ashore at a port across the sea: a colony's, or a foreign one for a nation that lives by its ships.
    // An army that marched there, or is forming up at home, simply arrives.
    const bySea = reg.port && (reg.overseas || (g.nations[nat].seaSupply && reg.owner !== nat));
    // A row that tells its own news says more than the bare arrival would.
    if (o.news) headline(g, 'event', o.news, nat, reg.id);
    else if (o.gen) headline(g, 'event', `${o.gen} ${bySea ? 'lands at' : 'arrives in'} ${reg.name} with ${the}${c.name}.`, nat, reg.id);
    else if (nat === g.human) headline(g, 'info', `${c.name} has ${bySea ? 'landed at' : 'arrived in'} ${reg.name}.`, nat, reg.id);
  });
  if (gone.length) {
    const where = theatreName(g.regions[gone[0].reg]);
    const names = gone.map((c) => c.name);
    for (const c of gone) removeCorps(g, c);
    g.supplyDirty = true;
    headline(g, 'event', `${g.nations[nat].adj} formations lent to the campaign${where ? ` in ${where}` : ''} leave for other fronts: ${names.join('; ')}.`, nat);
  }
  // (The far side: a garrison whose record ends in a surrender, a row with `ends`: its months over, it lays down its
  // arms where it stands, and the card says that and not that it left for another front. The design director's
  // acceptance of 13:09Z, 22 November 2026, of the historian's East Africa rows: far-africa.json. No older row has the word.)
  if (down.length) {
    const names = down.map((c) => c.name);
    const at = down[0].reg;
    for (const c of down) removeCorps(g, c);
    g.supplyDirty = true;
    headline(g, 'event', `${g.nations[nat].adj} forces lay down their arms: ${names.join('; ')}.`, nat, at);
  }
}

/** A theatre that opens in its own month digs nothing before it, and in free play, where it never opens, nothing at all. */
const worksDue = (g: Game, t: { opens?: number }): boolean => t.opens === undefined || (g.flags.open !== 1 && g.turn >= t.opens);

/**
 * Field works in the desert, and on the eastern front where history is followed, by history's calendar: in the month named, whoever of the nations listed holds the
 * place digs it in to that level, or strips its defences down to it. The works face the open desert: an attack
 * from ground of the same colony meets them. Works marked 'taken' are a beachhead's: they are dug as the month
 * closes by a holder who won the place in that month, where the others are dug as his month opens.
 */
function desertWorks(g: Game, nat: string, closing = false): void {
  // A theatre outside the desert can list works of its own in theatres.json: they are dug the same way.
  const rows: [number, string, number, string[], string, { taken?: boolean; port?: boolean; vs?: string[] }?][] = [...(DESERT.works ?? []), ...(g.flags.open !== 1 ? EASTERN.works ?? [] : []), ...THEATRE_LIST.flatMap((t) => (worksDue(g, t) ? t.works ?? [] : []))];
  for (const [when, place, lv, holders, news, o] of rows) {
    const r = g.byName[place];
    if (closing && !o?.taken) continue;
    if (when !== g.turn || !r || r.ctrl !== nat || !holders.includes(nat)) continue;
    // Nothing to do where the works already stand as the row wants them: the same level, facing the same way, the harbour built.
    const faces = o?.vs ?? r.fort?.vs ?? [r.owner];
    if ((r.fort?.lv ?? 0) === lv && (!o?.vs || (r.fort?.vs ?? []).join() === o.vs.join()) && (!o?.port || r.port)) continue;
    r.fort = lv > 0 ? { lv, vs: [...faces], by: nat } : null;
    // A harbour built off an open beach: from now on ships can land troops and stores there.
    if (o?.port && !r.port) { r.port = true; g.flags[`harbour:${r.id}`] = g.turn; g.supplyDirty = true; }
    headline(g, 'event', news, nat, r.id);
  }
}

/**
 * An army newly cut off from its supply is news the month it happens, whoever closed the ring. It is
 * told once: again only if the army is relieved and then cut off a second time.
 */
function cutOffNews(g: Game): void {
  const rings = new Map<number, { by: Record<string, number>; corps: number; men: number }>();
  for (const c of g.corps) {
    if (c.supplied || c.kind === 'fort') continue;
    const ring = rings.get(c.reg) ?? { by: {}, corps: 0, men: 0 };
    ring.by[c.nat] = (ring.by[c.nat] ?? 0) + 1;
    ring.corps++;
    ring.men += men(c);
    rings.set(c.reg, ring);
  }
  for (const [reg, ring] of rings) {
    const key = `cut:${reg}`;
    const fresh = (g.flags[key] ?? -99) < g.turn - 1;
    g.flags[key] = g.turn;
    if (!fresh || ring.corps < 3 || ring.men < 60000) continue;
    g.flags[`ring:${reg}`] = g.turn;
    const who = Object.keys(ring.by).sort((a, b) => ring.by[b] - ring.by[a]);
    const adj = who.map((id) => g.nations[id].adj).join(' and ');
    // An army that has been living on a surrounded town's stores was told of the ring when it closed.
    const spent = g.flags[`pktx:${reg}`] === 1;
    headline(g, 'battle', spent
      ? `The stores of ${pocketView(g, reg)?.town ?? townName(g.regions[reg])} are spent: ${ring.corps} ${adj} corps, ${ring.men.toLocaleString('en-US')} men, are out of supply and cannot hold out long.`
      : `The ring closes at ${g.regions[reg].name}: ${ring.corps} ${adj} corps, ${ring.men.toLocaleString('en-US')} men, are cut off from all supply.`, who[0], reg, { arm: 'land', lost: who[0] });
  }
}

/** The last thing in a nation's month: a beachhead won in it is dug in before the enemy's turn comes round. */
/**
 * The grand campaign: whoever stood the whole month where he was, without marching or fighting, is dug in from now
 * on, the computer's armies as well as the player's. Troops in a town have its walls, and troops under orders to
 * give ground do not dig. A corps lent to an ally's army keeps that army's month.
 */
function digIn(g: Game, nat: string): void {
  if (g.flags.open !== 1 || !DEFENCE.entrench.auto) return;
  for (const c of g.corps) {
    if (c.dead || c.kind === 'fort' || armyOf(c) !== nat) continue;
    // A corps that took works over this month keeps them as it found them: it marched, so it dug nothing of its own.
    if (c.relief) {
      delete c.relief;
      if (c.dug && c.town === undefined && !c.yields && !c.fought) { if (c.pressed) delete c.pressed; continue; }
    }
    // (A withered island garrison has no moves to spend and marched nowhere: it keeps the works it stood in as they
    // are. It digs no more of them, and none where it had none. Ruling 173(e), the whole-world chart.)
    if (c.town === undefined && !c.yields && !c.fought && withered(g, c)) { if (c.pressed) delete c.pressed; continue; }
    if (c.town === undefined && !c.yields && c.ap >= c.apMax && !c.fought) {
      // A month in which the line was attacked is a month's digging lost: the works are what the attack left of them.
      const gain = c.pressed ? DEFENCE.wear.regrow : 1;
      const late = digsLate(g.regions[c.reg]) ? DESERT.digLate : 0;
      // Works standing empty where it stood the month are its own (Version 65): a corps raised beside a ready line
      // does not dig from nothing while the line falls in.
      if (!c.dug) {
        takeWorks(g, c, g.regions[c.reg]);
        if (c.relief) { delete c.relief; c.dugFor = Math.max(c.dugFor ?? 0, (c.digging ?? 0) + gain - late); if (c.digging !== undefined) delete c.digging; if (c.pressed) delete c.pressed; continue; }
      }
      if (c.dug) c.dugFor = (c.dugFor ?? 1) + gain;
      else if (gain > 0) {
        // In the open desert the first month's digging shows no works (ruling 165): half after two whole months.
        const begun = (c.digging ?? 0) + gain;
        if (begun > late) { c.dugFor = begun - late; c.dug = true; if (c.digging !== undefined) delete c.digging; } else c.digging = begun;
      }
    }
    else { if (c.dug) delete c.dug; if (c.dugFor !== undefined) delete c.dugFor; if (c.digging !== undefined) delete c.digging; }
    if (c.pressed) delete c.pressed;
  }
  // (A corps the book has stationed keeps the record's works where it stands and digs no more: stations.ts, ruling
  // 234's rider b568. No corps carries the mark with the far side off.)
  // (And a row with works of its own keeps the record's where its row names them, wherever the book does not have it:
  // the rule that every corps left alone digs in whole gives way to the row's own flag. Ruling 234's note of 22:05Z of
  // 20 November 2026, item 5. No corps carries either mark with the far side off.)
  for (const c of g.corps) {
    const most = c.bookWorks ?? (c.ownWorks && c.reg === c.ownWorks[0] ? c.ownWorks[1] : undefined);
    if (most === undefined || c.dead || armyOf(c) !== nat) continue;
    if (most <= 0) { if (c.dug) delete c.dug; if (c.dugFor !== undefined) delete c.dugFor; if (c.digging !== undefined) delete c.digging; }
    else if (c.dug && (c.dugFor ?? 1) > most) c.dugFor = most;
  }
}

export function endPhase(g: Game, nat: string): void {
  // And again as it closes, before the other staffs move: what he saw this month he goes on knowing.
  if (nat === g.human) seeCuts(g);
  digIn(g, nat);
  if (g.nations[nat].alive) desertWorks(g, nat, true);
  siegeEmpties(g);
  mendPrizes(g, nat);
  // What was burned in this month's fighting is told once, in one line for each army driven off.
  tellBurnt(g);
  // The player's directive is looked at as his month closes: a town taken in it is a job done (the grand campaign).
  judgeDirective(g, nat);
  // The coast watch: troops and shipping left standing on a quay are seen from across the water (the grand campaign).
  if (g.nations[nat].alive) coastReport(g, nat);
  // The last act of the grand campaign: has this month's ground given the last great power its share of the world?
  crown(g);
}

/**
 * Where a row of the reinforcements or formations list is put down this month, read with nothing changed: its place,
 * the capital where that is not its side's, and in free play home rather than a friend's ground at peace, and the
 * nearest ground with room. Null where it does not come (no such ground on this chart, or nowhere its side holds),
 * and for a row made from corps on the board (`made`, far side) where none of them is left or its place is lost.
 */
function formPlace(g: Game, nat: string, row: any[], made: Corps[] | null): Region | null {
  const n = g.nations[nat];
  let reg = g.byName[row[5]];
  // A row for ground this campaign's chart does not carry (a far-side row on the older chart) never comes.
  if (!reg) return null;
  if (made && (!made.length || !allied(g, nat, reg.ctrl))) return null;
  if (!allied(g, nat, reg.ctrl)) reg = g.regions[n.capital];
  if (!reg || !allied(g, nat, reg.ctrl)) return null;
  if (g.flags.open === 1 && row[3] !== 'fort') {
    // Free play: not on the ground of a friend who is at peace, and never a sixth corps in one province.
    if (reg.ctrl !== nat && !sharesWar(g, nat, reg.ctrl) && n.capital >= 0 && g.regions[n.capital].ctrl === nat) reg = g.regions[n.capital];
    // (The record's own hand: a row by date is counted against five, an island's room or no: form B3, (c).)
    reg = roomNear(g, nat, reg, true) ?? reg;
  }
  return reg;
}

/**
 * The corps on the board a row of the formations list is made from (far side: `from`), in the order it names them;
 * null where it names none, or with the far side off.
 */
const formMade = (g: Game, nat: string, row: any[]): Corps[] | null => (farOn(g) && Array.isArray(row[6]?.from)
  ? (row[6].from as string[]).flatMap((nm) => g.corps.filter((x) => x.nat === nat && !x.dead && x.name === nm)) : null);

/**
 * A row of the reinforcements or formations list in its month. `id` is a formations row's number in the one list of
 * offers (`formId`), else -1; `take` is true where the player has just said yes to it (`takeOffer`). True where the
 * row formed.
 *
 * (The far side: a row of the formations list made from the player's own corps on the board is an offer, made in its
 * month, never a silent removal: the design director's ruling of 13:39Z, 25 November 2026, on the batch 1 readings,
 * point 3 of the dated rows, "Oahu's split: an offer too, yes. A reorganisation of the player's own forces is the
 * player's choice." It is made, stands, lapses and is answered as ruling B8's offers are (`rowOffers`, `takeOffer`,
 * `declineOffer`, the same flags); taken, the row forms as it always did; declined, his corps stay whole and the row
 * never comes. As built a player's America had the 24th and 25th Infantry Divisions made out of the Hawaiian
 * Division's corps in October 1941 without a word, as a computer's has. A computer's rows are as they were.)
 */
function formRow(g: Game, nat: string, row: any[], id: number, take = false): boolean {
  // A row marked 'open' is the open grand campaign's alone (ruling 233, rider b609(a)): the history campaign's roster stays the book's.
  if (row[6]?.open && g.flags.open !== 1) return false;
  // (The far side: a row made from corps on the board, `from`, as a dated expedition's is: the design director's
  // ruling of 06:21Z of 21 November 2026 to ruling 234, on Oahu's split. A computer staff that ships its own army
  // is sent no dated expedition (world.ts shipsItsOwn), so America's reorganisations stand in this list, for every
  // country alike. A reorganisation makes nothing from nothing: the row takes its strength out of the corps it
  // names, in their order, and never more than they hold; a corps emptied is struck off; with none of them left
  // the row does not come, and with its own place lost it is never formed at the capital instead.)
  const made = formMade(g, nat, row);
  const reg = formPlace(g, nat, row, made);
  if (!reg) return false;
  if (made && id >= 0 && nat === g.human && !take) { offerMade(g, nat, id, row[6]); return false; }
  const c = addCorps(g, nat, row[2], row[3], row[4], reg.id, row[6] ?? {});
  // A row by date reads its strength as a first-day row does (ruled 16:32Z, 17 November 2026; no row of Europe's book says one).
  if (row[6]?.str !== undefined) c.str = row[6].str;
  if (made) {
    const want = c.divs * c.str;
    let got = 0;
    for (const x of made) {
      const has = x.divs * x.str, part = Math.min(has, want - got);
      if (part <= 0) continue;
      got += part;
      if (has - part < 1e-9) removeCorps(g, x); else x.str = (has - part) / x.divs;
    }
    c.str = got / c.divs;
    g.supplyDirty = true;
  }
  return true;
}

export function startPhase(g: Game, nat: string): void {
  const n = g.nations[nat];
  fadeWorks(g, nat);
  // (Release 2, rearguard.ts) The nation's rearguards of last month stand down, the player's and the staffs' alike.
  rearguardsLapse(g, nat);
  if (!n.alive) return;
  // Shipping by the record's dates (ruling 234, rider b630).
  liftDates(g, nat);
  // Reinforcements from the pre-war mobilisation, and the armies and fronts formed later from men already under arms.
  // (A row of the formations list is numbered in the one list of offers after the dated rows: `formId`.)
  const early = (setup as any).reinforcements as any[];
  [...early, ...((setup as any).formations ?? [])].forEach((row: any[], j: number) => {
    if (row[0] !== g.turn || row[1] !== nat) return;
    formRow(g, nat, row, j >= early.length ? formId(j - early.length) : -1);
  });
  // (The far side: the player's reorganisations offered and not yet answered stand or lapse as any offer does: ruling
  // B8 and the design director's ruling of 13:39Z, 25 November 2026, point 3, `formRow`. Never for a computer.)
  if (farOn(g) && nat === g.human) ((setup as any).formations ?? []).forEach((row: any[], k: number) => { if (offerOpen(g, nat, formId(k))) offerStands(g, nat, row, formId(k)); });
  // The fortress rows of this month, and those whose orders take them out of their works (the fortress rows' switch on).
  fortsMonth(g, nat);
  expeditions(g, nat);
  // Corps whose long passage ends this month come ashore after the month's arrival rows (ruling 229, part 1).
  landVoyages(g, nat);
  desertWorks(g, nat);
  mobilise(g, nat);
  monthlyCommand(g, nat);
  navalUpkeep(g, nat);
  // Carriers that joined or went down this month: their wings follow (the grand campaign).
  syncWings(g);
  airUpkeep(g, nat);
  researchTick(g, nat);
  techTick(g, nat);
  // Oil and supplies come in and go out before the month's money is counted: idle factories earn less (the grand campaign).
  resourcesTick(g, nat);
  // The secret war: the player's agents report, strike or are caught, and the other side's saboteurs come calling (the grand campaign).
  intelTick(g, nat);
  // What the player can see of cut railways is noted as his month opens (the fog on a cut line, rulings 113 and 114).
  if (nat === g.human) seeCuts(g);
  // The towns this nation holds that are under siege: another month of bombardment and of stores eaten (the grand campaign).
  siegeMonth(g, nat);
  // Its provinces cut off from the rest of the country eat another month of their stores.
  pocketMonth(g, nat);
  // The places outside a town this nation's army has now held long enough: they start to work for it.
  tellSettled(g, nat);
  computeSupply(g);
  cutOffNews(g);
  // Supply columns (release 2, columns.ts): the month's lorries go out from the railhead to the corps beyond it, and some are caught on the road.
  columnsMonth(g, nat);
  // The month's production comes in, and in the grand campaign the forces are paid out of it. Losses are made good
  // from what is left. A treasury that is empty pays nobody: no drafts go out, the armies rest badly and men drift away.
  // Trade agreements are looked over first: what war has ended, what the sea lets through (the grand campaign).
  tradeTick(g, nat);
  // The player's war cabinet reads the month's news, and sets or judges its directive (the grand campaign).
  govMonth(g, nat);
  // The atomic project (release 2, atom.ts): the plants draw their share of this month's earnings, and the bombs are made.
  atomMonth(g, nat);
  const earned = income(g, nat);
  n.pp += earned - upkeepTotal(g, nat);
  const credit = BUDGET.debt.floor * Math.max(1, earned);
  const owed = budgeted(g) && n.pp < 0 ? Math.min(1, -n.pp / credit) : 0;
  // What the player's decrees take to keep can outrun what comes in (the grand campaign): the capital in hand pays for it.
  // (And, with the nation's footing switched on, what holding rearmament or all-out war takes: ruling 172.)
  const pcBase = pcIncome(nat, g) + civicSum(g, nat).pc + govCapital(g, nat), pcNet = pcBase + footHold(g, nat, pcBase);
  n.pc = Math.max(0, Math.min(Math.max(n.pc, DIPLOMACY.pcCap), n.pc + (g.flags.open === 1 ? pcNet : Math.max(0, pcNet))));
  const perDiv = PRODUCTION.replacementCost;
  const tempo = freshOffensive(g, nat);
  const army = g.corps.filter((c) => c.nat === nat);
  // Men, tanks and rest reach a corps only as well as its supply line allows. The depots can
  // send out only so many trained men a month, and what there is is shared along the front.
  // Overseas, every man and tank has to come by sea first: the drafts arrive slower.
  // A corps the player has told to keep its veterans together asks for no drafts.
  // The level of the campaign quickens or slows the drafts, for the player or for his rivals: at the fair level, not at all.
  // A nation whose supplies have run out cannot equip the drafts it trains (the grand campaign).
  // Training grounds at home turn out replacements faster (the grand campaign).
  // The player's ministers and his decrees on the call-up quicken them, or slow them (the grand campaign).
  const drafts = draftsMul(g, nat) * traitDrafts(g, nat) * supDrafts(g, nat) * civicDrafts(g, nat) * govMul(g, nat, 'drafts');
  // A hospital in a resting corps' province, or beside it, sends more of its men back and rests it better.
  const ward = (c: Corps): { drafts: number; org: number } => (restedMonth(g, c) ? hospitalWorth(hospitalNear(g, c, (h) => h === nat || allied(g, h, nat))) : { drafts: 1, org: 0 });
  // A corps that has spent the whole month out of the line takes in its drafts faster (the grand campaign).
  // (A fortress crew living on its port's stores is cut off for its drafts and its rest, 07:12Z.)
  const stored = new Set(army.filter((c) => onPortStores(g, c)));
  const wantOf = (c: Corps): number => (c.supplied && !stored.has(c) && !(c.lean && nat === g.human) && !shutIn(g, c) && !inPocket(g, c) ? Math.min(Math.max(0, (c.full ?? 1) - c.str) * c.divs, 0.2 * drafts * restedDrafts(g, c) * ward(c).drafts * c.divs * lineStrength(g, c).mul * columnShare(g, c) * (g.regions[c.reg].overseas ? DESERT.replace : 1)) : 0);
  // A volunteer legion's drafts come from its own home's reserve, not the patron's (legions.ts; release 2, switched off in release 1).
  const want = army.map((c) => (c.vol ? 0 : wantOf(c)));
  const volWant = army.map((c) => (c.vol ? wantOf(c) : 0));
  const asked = want.reduce((s, x) => s + x, 0);
  // The month's class comes of age and joins the reserve, and the men the armies ask for are taken from it (the
  // grand campaign): a nation whose reserve is spent makes good only what the class allows.
  if (n.alive) classArrives(g, nat, govMul(g, nat, 'men'));
  // The last levy of a nation whose army is being worn away (Version 68; traits.ts).
  if (n.alive) traitLevy(g, nat);
  // The player's say over the call-up (the grand campaign): while he holds the standing call-up, the men of the wave
  // that was not raised go out as drafts over and above what the depots send, and like the wave they cost the treasury nothing.
  const heldMen = heldDrafts(g, nat);
  const share = asked > 0 ? Math.min(1, (Math.max(0, n.pp) / perDiv + heldMen) / asked, (PRODUCTION.manpower * drafts * Math.max(n.armyPeak, armySize(g, nat)) + heldMen) / asked, menLeft(g, nat) / asked) : 0;
  let drafted_ = 0, freeLeft = heldMen;
  // Corps starved into surrender this month, by where they stood: an army lost together is one piece of news.
  const starved = new Map<number, { names: string[]; men: number }>();
  // (Ruling 173(e), the whole-world chart alone: on an island of the historian's table a cut-off garrison withers to
  // a floor and does not lay down its arms from hunger. wither.ts; with the far side off nothing of it is read.)
  const far = farOn(g);
  army.forEach((c, i) => {
    // What it knew before the month's drafts: a corps back from the battle schools is a grade better than when it went.
    // And its strength before them: the drafts come in new uniforms (wearMonth).
    const knew = c.xp, had = c.str;
    // Its morale for the month: where it stands, who stands with it, whether it has had any rest (the grand campaign).
    moraleMonth(g, c, { owed, cold: c.supplied && frozen(g, nat, g.regions[c.reg]) });
    // (A count carried off every island of the table is struck off: the rule does not answer there.)
    if (far) witherDrop(g, c);
    if (c.supplied && !stored.has(c)) {
      // (A fortress crew fed all month is restocked: its own stores' count starts again, 09:06Z (b).)
      if (fortRow(g, c)) crewFed(g, c);
      // (A withered island garrison mends when a ship of its side can reach it again: fed from outside, its count is
      // struck off. A town's own stores under siege, or a pocket's, are not a ship.)
      if (far && !shutIn(g, c) && !inPocket(g, c)) witherFed(g, c);
      const give = want[i] * share;
      if (give > 0) {
        // Green drafts thin out what a corps of the player's has learned.
        drafted(g, c, c.str, give / c.divs);
        const free = Math.min(freeLeft, give);
        freeLeft -= free;
        c.str += give / c.divs; n.pp -= (give - free) * perDiv;
        drafted_ += give;
      }
      if (volWant[i] > 0) legionDrafts(g, c, volWant[i]);
      // A Russian winter without winter clothing costs men to frostbite and leaves little rest.
      const cold = frozen(g, nat, g.regions[c.reg]);
      const bite = cold ? cmdWinter(g, c) : 0;
      if (cold) c.str *= 1 - EASTERN.winter.attrition * bite;
      const rest = ((c.fought ? 0.25 : 0.45) + restedOrg(g, c) + ward(c).org) * lineStrength(g, c).mul * columnShare(g, c) * (1 - (1 - EASTERN.winter.rest) * bite) * cmdRally(g, c);
      // (A garrison still withered here eats a besieged town's stores or a pocket's, which are no ship: it gets no
      // rest from them, and stays at its floor. An enemy who lands beside it does not mend it.)
      if (!(far && withered(g, c))) c.org = Math.min(1, c.org + (owed ? rest * (1 - BUDGET.debt.rest * owed) : rest));
      if (owed) c.str *= 1 - BUDGET.debt.desert * owed;
    } else {
      // (A fortress crew under the fortress rows lives on its works' own stores, counted apart from its organisation
      // (09:06Z): it holds its organisation and its strength while they last, and goes in the month they run out; a
      // row with no clock yields only with its army.)
      // (The whole-world chart: the island of the table this corps is cut off on, where its rule holds this month, and
      // what the corps was before the month's hunger. A crew already withered has no stores left to count.)
      const isle = far ? witherAt(g, c, enemyAshore(g, c)) : null;
      const was = isle ? { org: c.org, str: c.str } : null;
      const crew = fortRow(g, c) ? (isle && withered(g, c) ? 'held' : crewMonth(g, c)) : 'fed';
      if (crew === 'fed') { c.org -= UNFED; c.str *= 0.85; }
      if (isle && was) {
        // Hunger takes it to the island's floor and no further; the month its stores are gone, its own player is told.
        if (witherMonth(g, c, isle, was, crew === 'fed' ? monthsUnfed(was.org) : crew) === 'withers' && nat === g.human) {
          // (The UI director's words, 14:00Z 25 November 2026. Bad news to its own player, `lost`, in the month it withers only.)
          headline(g, 'info', `Cut off and out of stores, ${c.name} withers on ${placeName(g.regions[c.reg].name)}. It holds on at ${floorWords(isle)} of its strength and will not lay down its arms, but it can no longer attack. It recovers once stores reach it by sea.`, nat, c.reg, { lost: nat });
        }
      } else if (crew === 'fed' ? c.org <= 0 : crew === 'starved') {
        const lost = starved.get(c.reg) ?? { names: [], men: 0 };
        lost.names.push(c.name);
        lost.men += men(c);
        starved.set(c.reg, lost);
        storyFallen(g, c, 'starved', n.atWar[0] ?? nat, c.reg);
        removeCorps(g, c);
        return;
      }
    }
    // Its month at the battle schools is over (release 2): back a grade better, or cut short.
    const schooled = schoolMonth(g, c, knew);
    // How worn its uniforms and kit look after the month gone (release 2, looks only): a whole term at school is a refit.
    wearMonth(g, c, had, schooled);
    c.fought = false;
    if (c.blows !== undefined) delete c.blows;
    if (c.gaveWay) delete c.gaveWay;
    c.ap = c.apMax = 4 + (tempo && KIND[c.kind].mobile ? 1 : 0) + cmdAp(g, c) - (fuelSlows(g, c) ? 1 : 0);
    // A corps with an ally's army marches in that army's month, not in its own (the grand campaign).
    if (jointHold(g, c)) c.ap = 0;
    // A garrison keeps to its town until it is ordered out.
    if (c.town) c.ap = 0;
    // A withered island garrison only defends: it neither attacks, marches nor sails (ruling 173(e), rider (o)(3)).
    if (far && withered(g, c)) c.ap = 0;
  });
  if (drafted_ > 0) drawMen(g, nat, drafted_);
  if (menRules(g) && nat === g.human) {
    const level = menWarning(g, nat), left = menWords(menLeft(g, nat) * MEN_PER_DIV), start = menWords(poolAtStart(g, nat) * MEN_PER_DIV);
    // What he can do about it is said with the warning: the say he has over the call-up is in the ledger.
    const say = `In the ledger you can call the next class up early${standingCallUp(g, nat) ? ', or hold the standing call-up so that the men go to the corps in the field' : ''}.`;
    if (level === 1) headline(g, 'info', `The reserve of men is thinning: ${left} are left of the ${start} the war began with. Every loss made good and every new corps is taken from it. ${say}`, nat);
    else if (level === 2) headline(g, 'info', `The reserve of men is running short: ${left} are left, under ${Math.round(MANPOWER.warn[1] * 100)}% of what the war began with. Spare the armies, or the day comes when losses cannot be made good. ${say}`, nat);
    else if (level === 3) headline(g, 'info', 'The reserve of men is spent. From now on only the class that comes of age each month reaches the armies, and no new corps can be raised.', nat);
  }
  // A town whose last defenders laid down their arms this month falls.
  siegeFalls(g, nat);
  for (const [reg, lost] of starved) {
    // Small parties are not news, unless they are the end of an army the newsreel has already said was surrounded.
    if (lost.men <= 30000 && g.flags[`ring:${reg}`] === undefined) continue;
    const place = g.regions[reg].name;
    if (lost.names.length === 1) headline(g, 'battle', `Cut off and out of supply, ${lost.names[0]} lays down its arms at ${place}.`, nat, reg, { arm: 'land', lost: nat });
    else headline(g, 'battle', `Cut off and out of supply, the ${n.adj} army at ${place} lays down its arms: ${lost.names.length} corps, ${lost.men.toLocaleString('en-US')} men.`, nat, reg, { arm: 'land', lost: nat });
  }
  // Replacements can spend the treasury to the last point: a rounding error below nothing is not a debt.
  n.pp = budgeted(g) ? (n.pp > -1e-9 ? Math.max(0, n.pp) : Math.max(-credit, n.pp)) : Math.max(0, n.pp);
  if (budgeted(g)) {
    // The first month in the red is news, and so is the month the books balance again.
    const key = `debt:${nat}`;
    if (n.pp < 0 && g.flags[key] === undefined) {
      g.flags[key] = g.turn;
      if (nat === g.human) headline(g, 'info', `The treasury is empty. Until the books balance again no replacements go out to the armies, nothing new can be ordered, and unpaid troops rest badly and begin to drift away. Open the ledger to see where the money goes.`, nat);
    } else if (n.pp >= 0 && g.flags[key] !== undefined) {
      delete g.flags[key];
      if (nat === g.human) headline(g, 'info', `The books balance again: the treasury is out of debt.`, nat);
    }
  }
  deliver(g, nat);
  engineersMonth(g, nat);
  if (nat !== g.human) n.pp = Math.min(n.pp, PRODUCTION.aiBank[canBuildYet(g, nat) ? 1 : 0]);
  n.armyPeak = Math.max(n.armyPeak, armySize(g, nat));
  if (tempo) g.flags[`tempo:${nat}:${g.turn}`] = 1;
  storyMonth(g, nat);
  jointMonth(g, nat);
  capitalWatch(g, nat);
  checkCollapse(g, nat, n.atWar[0] ?? nat);
  // (Ruling 196: a nation with a row of terms asks for them at its month's opening. No row, nothing read.)
  if (RES.will.terms?.[nat]) checkTerms(g, nat);
}

/**
 * The Red Army moves into eastern Poland once the Polish front is breaking, as the 1939 pact arranged. In the grand
 * campaign, if Poland has already fallen and the pact has handed its zone over, the Red Army marches into it all the
 * same (the design director's b502): the same corps to the same six places, so the zone is never handed over empty.
 * (The historical scenarios are left as they were.)
 */
export function sovietEvent(g: Game): void {
  if (g.flags.sovietMove || !g.nations.SOV.alive || g.turn > 3 || atWar(g, 'SOV', 'GER')) return;
  const fallen = !g.nations.POL.alive;
  if (fallen) {
    if (g.flags.open !== 1 || !g.regions.some((r) => r.sov && r.owner === 'POL' && r.ctrl === 'SOV')) return;
  } else {
    if (!atWar(g, 'GER', 'POL')) return;
    const lost = g.regions.filter((r) => r.owner === 'POL' && r.ctrl !== 'POL').length;
    if (lost < 5) return;
  }
  const was = holdersNow(g); // settled towns (release 2): the pact's ground starts settling this month
  g.flags.sovietMove = 1;
  g.week = 3;
  for (const r of g.regions) {
    if (!r.sov || r.ctrl !== 'POL') continue;
    for (const c of [...r.units]) if (c.nat === 'POL') removeCorps(g, c);
    r.ctrl = 'SOV';
  }
  // In the grand campaign, with Poland still standing, the pact's line holds all the same: ground east of it that the
  // German army has already overrun is handed over as the Red Army comes in, and the Germans there fall back west of
  // the line, as they gave up Brest and the approaches to Lwow in September 1939. (Ruling 232 lets the German army
  // sweep the whole zone in its first month while Warsaw holds out, rider b566; without this the Red Army found its
  // six places in German hands and stayed at home, and the Winter War was never fought in earnest.) The design
  // director's b571: only where a computer-run power keeps the pact. A player's ground and a player's corps are never
  // moved by it: with a player's Germany the zone stays as he left it.
  if (!fallen && g.flags.open === 1) {
    const mine = (r: Region): boolean => g.human !== null && (r.ctrl === g.human || [...r.units, ...garrisonOf(r)].some((c) => c.nat === g.human));
    const over = g.regions.filter((r) => r.sov && r.owner === 'POL' && r.ctrl !== 'SOV' && r.ctrl !== 'POL' && !atWar(g, 'SOV', r.ctrl) && !mine(r));
    for (const r of over) {
      for (const c of [...r.units, ...garrisonOf(r)]) if (c.nat === 'POL') removeCorps(g, c);
      r.ctrl = 'SOV';
    }
    for (const r of over) for (const c of [...r.units, ...garrisonOf(r)]) if (c.nat !== 'SOV') sendHome(g, c);
  }
  const fronts = g.corps.filter((c) => c.nat === 'SOV' && !c.hold);
  const dest = ['Wilno', 'Brest', 'Lwow', 'Volhynia', 'Bialystok', 'Polesie'];
  fronts.forEach((c, i) => { const r = g.byName[dest[i % dest.length]]; if (r.ctrl === 'SOV') place(g, c, r); });
  g.supplyDirty = true;
  headline(g, 'event', 'The Red Army crosses into eastern Poland under the pact with Germany.', 'SOV');
  g.week = 4;
  handedOver(g, was);
  if (!fallen) checkCollapse(g, 'POL', 'GER');
}
