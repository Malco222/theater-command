// Free play ("open history"). The war starts as it did on 1 September 1939, and from then on no
// computer-run nation follows history's plan. Each has a staff that looks at the map every month:
// where it is in touch with an enemy, how strong each side is there, and whether an offensive is
// worth opening, pressing or calling off. The great decisions history took (the attack in the west,
// Italy's entry, the attack on the Soviet Union) are choices the staff may or may not make, and when.
// What comes out is an ordinary plan phase, so the armies carry it out by the same rules as in
// Historical mode. The numbers and the list of choices live in data/campaign/open.json.

import { raiseForts, trainCrews } from './fortrows';
import { FORTS_STAGE } from './stage';
import { YIELD_STAGE } from './stage';
import { deterred } from './league';
import { openTraits } from './traits';
import { openTech, openUnitsRows } from './tech';
import { openStores } from './resources';
import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import openData from '../../data/campaign/open.json';
import rivalsData from '../../data/campaign/rivals.json';
import { hashString, Rng } from '../core/rng';
import { turnToDate } from '../core/ticket';
import { holds, leavings, majorsOf } from './diplomacy';
import { concertFor, landingsFor, nerveOf, seat, type Landings } from './difficulty';
import { DOMINATION, dominating } from './domination';
import { bookIf, leanFor, leash, moodMul, stopDef, stopOf } from './history';
import { lineOwes, lineShut } from './line';
import { blocked } from './rivers';
import { openAirWar, openPilots, openWorldAir, skyDenies } from './air';
import { isCity, openSmallAir } from './arsenal';
import { openCarriers } from './carriers';
import { fieldCtrl, openSieges, raiseGarrisons, startGarrisons } from './siege';
import { agentsOf, atWork, knowsBooks } from './intel';
import { startEngineers } from './engineers';
import { openMorale } from './morale';
import { numberDivisions } from './armies';
import { STRENGTH, reckon, type Reckoning } from './ashore';
import { NAVY, canPass, closedTo, seaPath, seaRisk } from './navy';
import { openEscorts } from './fleets';
import { openTrade } from './trade';
import { openGovernment } from './government';
import { openFooting } from './footing';
import { openWeather } from './weather';
import { openAirVersion } from './airversion';
import { openSecretWar } from './secretwar';
import { drawMen, menLeft, menRules, openManpower } from './manpower';
import { WORLD_FIRST, WORLD_MAP, acrossOcean, attachWorld, beyondEurope, farOn, neighbourByLand, oceanShares, overseasOf, shoreOf } from './world';
import { patronOfClient, stationed, tieWar } from './far';
import { inTheWay, pacificGround, runQuays } from './pacific';
import { EASTERN, addCorps, allied, assess, atWar, bloc, carriedRows, collapseScore, computeSupply, corpsBase, friendlyUnits, frozen, headline, hostileUnits, joinedSide, limitAttackers, lineOwners, theName, weather, worksAgainst, worksFor, placed, roomless } from './game';
import { openWinter, winterOver } from './winter';
import { ATOM_STAGE, BREAKUP_STAGE, CARRIERSTRIKE_STAGE, COLUMNS_STAGE, CONVOY_STAGE, DUMPS_STAGE, FLEETATTACK_STAGE, FLEETWHY_STAGE, HEAVYSEAS_STAGE, LORRYSHIFT_STAGE, MARKED_STAGE, NICKNAMES_STAGE, REARBRIDGES_STAGE, REARGUARD_STAGE, SEAMINES_STAGE, SEEKONCE_STAGE, SETTLEDTOWNS_STAGE, SHADOWRELAY_STAGE, SHADOW_STAGE, TASKFORCE_STAGE, TOWNQUEUE_STAGE, TRUEREACH_STAGE, UNITS_STAGE, VOYAGELOG_STAGE, WAGONS_STAGE, WINCHANCE_STAGE, WINGBYDATE_STAGE, AIRNAMES_STAGE, FERRY_STAGE } from './stage';
import { FORT_MUL, KIND, STACK, TERRAIN_DEF, type Corps, type Game, type PlanPhase, type Region } from './types';
import { openBargains } from './bargains';
import { openLegions } from './legions';
import { openQuality } from './units';
import type { PlanFront } from './planfronts';

interface Temper { attackAt: number; persist: number; minOdds: number }

/** One of history's great decisions, written as a choice a staff may take up. */
export interface Op {
  id: string; name: string;
  declare?: string[]; if?: string[]; from?: number; to?: number;
  /** The odds in prospect it takes to go, and to keep preparing. */
  ratio?: number;
  /** Months spent bringing the army up before war is declared. */
  prep?: number;
  /** The chance, each month the conditions hold, that the staff takes the decision. */
  chance: number;
  /** The chance, each month the army is up and the season right, that the blow is put off another month: weather, a plan compromised, nerves. Never past the season's last month. */
  hold?: number;
  /** Months of the year in which the blow may fall. */
  season?: number[];
  /** Months the offensive is pressed whatever the odds, once begun. */
  push?: number;
  /** A side operation: no build-up, and the main army is not taken for it. */
  side?: boolean;
  /** Off the book its month is drawn elsewhere, by the test that raises the flag it waits on (readiness.ts): once its conditions hold it goes, with no second draw. */
  sure?: boolean;
  landings?: PlanPhase['landings'];
  /** Harbours its country holds without owning them, from which these landings may sail as from a home port. */
  quays?: string[];
  /** Corps that come off hold the month the choice goes: the plan's own reserves, named (far-pacific-war.json: the Guards for Malaya, the naval landing forces for the Indies). */
  frees?: string[];
  /** So many months after it goes, the corps it named for its own landings that still wait on a quay come off hold too: they are the staff's own from then. */
  freesAfter?: number;
  /** Strikes its carriers fly at an enemy harbour in the month it goes (ai.ts and carriers.ts, `plannedStrike`). */
  strikes?: PlanPhase['strikes'];
  /** The fronts of a plan taken from the record: whose each row is, where each front ends, and the follow-ons by month (planfronts.ts, rider b633 to ruling 234). Read only where no book dates the rows. */
  fronts?: PlanFront[];
  staging?: Record<string, number>;
  join?: 'allies' | 'axis';
  /**
   * A war for a strip of frontier (the Winter War): while it is the staff's only war, only the troops within this many
   * marches of the front take part, and the rest of the army stays where it stands.
   */
  near?: number;
  /** The ground such a war is fought for: while it is the staff's only war, the army attacks and steps only there. */
  aims?: string[];
  /** The news that follows the declaration, in the same line: the month card tells one story. */
  news?: string;
  /** The works and the peace of the Winter War (winter.ts). */
  works?: Record<string, [number, ...string[]]>;
  peace?: { cede: string[]; from: number; by: number; after: number; chance: number };
  /**
   * By the book (a stop that dates this choice), the front it is meant for opens no general offensive while it waits:
   * not out of impatience, not on a staff's temper row (ruling 215(b): Japan's offensive of 1944 in China).
   */
  wait?: boolean;
}

/**
 * History's shores for one staff (open.json, 'descent.planned'). 'open': the countries on whose own shores it may open
 * a front by a landing of its own; 'join': those on whose ground it goes by ship to a front its side holds (left out:
 * anywhere); 'with': the build-up. Version 79: 'from' gives a mainland shore the month it opens and 'lapse' the
 * country whose invasion ends the wait; 'book' keeps the whole list for the stops where history's dates hold;
 * 'behind', 'base' and 'joinFrom' are for a staff that waits on its base behind the allies named.
 */
interface Planned { open: string[]; join?: string[]; with?: number; from?: Record<string, number>; lapse?: Record<string, string>; not?: string[]; book?: boolean; behind?: string[]; base?: string[]; joinFrom?: number }
const OPEN = openData as unknown as {
  mood: number; major: number; rest: number; stall: number; headOn: number; commit: number; bound: number; boundOverseas: number; thrust: [number, number]; weak: number; foothold: number; general: number; guard: number; guardWithin: number;
  temper: Record<string, Partial<Temper>>;
  ops: Record<string, Op[]>;
  /** A staff grows bolder the longer its front is quiet: months of quiet, and what each stage does. */
  impatience: { after: number[]; ratio: number[]; odds: number[]; effort: number; floor: number; any: number; local: number[]; locals: number };
  /** The second front: when a staff with shipping and no land front crosses the sea, and with what. */
  descent: Landings & { after: number; abroad?: number; lead?: number; day?: boolean; wait?: boolean; afresh?: boolean; room?: boolean; joint?: boolean; stands?: boolean; hold?: number; feed?: number; strength?: unknown; heed?: { mainland: boolean; island: boolean; wave: boolean }; front?: number; door?: string[]; planned?: Record<string, Planned> };
  /** The coast watch: the share of its field corps a staff will keep in the harbours of its homeland, the fewest it keeps, and the most in one harbour. */
  coast?: { share: number; least: number; each: number; works: number; dig?: { orders: number; top: number; capital: number; keep: number } };  /** Ruling 233, rider b612: the ward. The stops of the dial at which a computer staff guards the capital of an ally whose leaving is in play, and the corps it keeps there. */
  ward?: { stops: number[]; corps: number };
  /** Rider C of 20:45Z (20 November 2026) to ruling 234: ground taken has to be held. The garrison a province of taken ground asks, in divisions' worth, and the stops of the dial at which a computer staff keeps it (garrisonMonth). Far-side rows alone give it. */
  garrison?: { each: number; stops: number[]; by?: { name: string; each: number; places: string[]; own?: boolean }[] };
  /**
   * The rear garrison of a taken port (the design director's rulings of 16:36Z, 16:44Z and 16:46Z of 21 November 2026;
   * rearMonth): the stops at which it comes by cause, and each owner's own table: the countries whose ground its
   * record covers (`of`), what a port the table does not name gets (`each`), the row's name there, and the record's
   * towns, each [divisions' worth, the row's name]. Far-side rows alone give it, and a country with no table of its
   * own gets nothing.
   */
  rear?: { stops: number[]; xp?: number; says?: [one: string, several: string]; by: Record<string, { each: number; name: string; says?: [one: string, several: string]; of: string[]; towns: Record<string, [worth: number, name?: string]> }> };
};

/** The organisation a corps needs before a staff counts it fit to send into battle (the staffs' own fit line; the yield form reads it too). */
export const FIT_ORG = 0.35;

/** True in a free-play war: the computer's nations make their own decisions. */
export const isOpen = (g: Game): boolean => g.flags.open === 1;

/**
 * Turns a game into a free-play war from this month on. In September 1939 history sets the start: Germany's
 * attack on Poland is already under way. Later in the war each staff finds its fronts as history left them.
 */
export function startOpen(g: Game, wide = true): Game {
  // (Ruling 232, rider b606: the garrisons that are the ruling's own join the roster before anything is counted from it.)
  raiseGarrisons(g);
  g.flags.open = 1;
  // The fortress rows (stage.ts FORTS_STAGE): the switch is set as the campaign opens, and the book's crews of the
  // first day join the roster with the garrisons, before anything is counted from it. (A test may set the flag on the
  // game it opens.)
  if (FORTS_STAGE || g.flags.fortRows === 1) { g.flags.fortRows = 1; trainCrews(g); raiseForts(g); }
  // The yield form (stage.ts YIELD_STAGE): the switch is set as the campaign opens. (A test may set the flag on the
  // game it opens.)
  if (YIELD_STAGE || g.flags.yieldRule === 1) g.flags.yieldRule = 1;
  // Units decide (release 2, src/campaign/units.ts): set at a campaign's start only, where stage.json switches it on,
  // and before the opening research, so a later start's staffs take its research rows in their catch-up months.
  if (UNITS_STAGE) g.flags.unitsDecide = 1;
  // Earned nicknames and supply columns (release 2, nicknames.ts and columns.ts): set as the campaign opens, where stage.json switches them on.
  if (NICKNAMES_STAGE) g.flags.nicknames = 1;
  if (COLUMNS_STAGE) g.flags.columns = 1;
  // The three supply orders on top of the columns (release 2, logistics.ts), each its own switch.
  if (LORRYSHIFT_STAGE) g.flags.lorryShift = 1;
  if (WAGONS_STAGE) g.flags.wagons = 1;
  if (DUMPS_STAGE) g.flags.dumps = 1;
  // A fleet sent straight at an enemy fleet (release 2, fleetattack.ts).
  if (FLEETATTACK_STAGE) g.flags.fleetAttack = 1;
  // An air group sent to any friendly airfield with one order, by stages (release 2, ferry.ts).
  if (FERRY_STAGE) g.flags.ferry = 1;
  // Breaking a corps up into its divisions (release 2, breakup.ts).
  if (BREAKUP_STAGE) g.flags.breakUp = 1;
  // The marked army (release 2, marked.ts): the corps that takes a town of the player's is marked until he beats it.
  if (MARKED_STAGE) g.flags.marked = 1;
  // Task forces: a fleet with a standing job (release 2, taskforce.ts).
  if (TASKFORCE_STAGE) g.flags.taskForce = 1;
  // The admirals' reasons for each fleet they order (release 2, fleetwhy.ts).
  if (FLEETWHY_STAGE) g.flags.fleetWhy = 1;
  // Rearguards: a small corps holds a province a month to slow the enemy (release 2, rearguard.ts).
  if (REARGUARD_STAGE) g.flags.rearguard = 1;
  // Conquered towns settle (release 2, buildings.ts).
  if (SETTLEDTOWNS_STAGE) g.flags.settledTowns = 1;
  // Shadowing, the voyage log, and rearguards that blow the bridges behind them (release 2; shadow.ts, voyage.ts, rearguard.ts).
  if (SHADOW_STAGE) g.flags.shadow = 1;
  if (VOYAGELOG_STAGE) g.flags.voyageLog = 1;
  if (REARBRIDGES_STAGE) g.flags.rearBridges = 1;
  // Mines at sea, heavy seas and the shadow relay (release 2; mines.ts, heavyseas.ts, shadow.ts).
  if (SEAMINES_STAGE) g.flags.seaMines = 1;
  if (HEAVYSEAS_STAGE) g.flags.heavySeas = 1;
  if (SHADOWRELAY_STAGE) g.flags.shadowRelay = 1;
  if (SEEKONCE_STAGE) g.flags.seekOnce = 1;
  // A queue for a town (release 2; townqueue.ts).
  if (TOWNQUEUE_STAGE) g.flags.townQueue = 1;
  // The atomic project (release 2; atom.ts).
  if (ATOM_STAGE) g.flags.atom = 1;
  // Carrier strikes on harbours by the player's own hand (release 2; harbourstrike.ts).
  if (CARRIERSTRIKE_STAGE) g.flags.carrierStrike = 1;
  if (WINGBYDATE_STAGE) g.flags.wingByDate = 1;
  // Better air unit names (release 2, air.ts groupName): set before the air war opens, so the first day's groups take them.
  if (AIRNAMES_STAGE) g.flags.airNames = 1;
  // The grand campaign is played on the wider world: it is attached before anything else is set up for it. (One
  // kept to the map of Europe is what a campaign saved before the world was drawn plays on as.)
  if (wide) attachWorld(g);
  // The Mannerheim Line across the Karelian isthmus, while Finland holds it (ruling 228, 1b).
  openWinter(g);
  if (g.turn === 1) {
    g.flags['st:on:GER:POL'] = 1;
    g.flags['st:gain:GER'] = 1;
  }
  // (Begun after the first day: the rows whose men history's corps already brought do not come again. The month it
  // opens is kept: its navies, the player's included, open with the record's ships of that month, rider b639.)
  else {
    carriedRows(g);
    g.flags.openFrom = g.turn;
  }
  startEngineers(g);
  openTraits(g);
  // Units decide's start list (units.json startRows; DD 24 Nov 15:53Z, #2 form): rows held on the first day under that
  // switch only, laid before the opening research so a later start's catch-up counts each once. Off, nothing.
  openUnitsRows(g);
  openTech(g);
  // America's air commands stand on their fields before the depots are counted: the stores open with what they burn.
  openWorldAir(g);
  openStores(g);
  openAirWar(g);
  openSmallAir(g);
  openCarriers(g);
  // Units decide's quality store (units.ts; DD 24 Nov 15:53Z, #2 form Part 2): seeded once the wider world, the
  // opening research, the world's air commands and the carriers are in place. Off, nothing.
  openQuality(g);
  // (And every air group's pilots from it: their hours and experience, under the switch only.)
  openPilots(g);
  openSieges(g);
  openMorale(g);
  openEscorts(g);
  openTrade(g);
  // The convoy battle (release 2, ruling 241): set at a campaign's start only, where stage.json switches it on.
  if (CONVOY_STAGE) g.flags.convoyBattle = 1;
  // The honest win chance on land (release 2, auto-resolve fix 1): set at a campaign's start only, where stage.json switches it on.
  if (WINCHANCE_STAGE) g.flags.winChance = 1;
  // True reach on the old chart (game.ts trueReach): a step of Units decide, set at a campaign's start only, where stage.json switches both on.
  if (UNITS_STAGE && TRUEREACH_STAGE) g.flags.trueReach = 1;
  // A neutral's bargains (release 2): only where stage.json switches them on.
  openBargains(g);
  openGovernment(g);
  openFooting(g);
  openWeather(g);
  openAirVersion(g, majorsOf(g));
  openSecretWar(g);
  openManpower(g);
  numberDivisions(g);
  // (After the numbers: a garrison keeps its divisions' numbers under the record's name for the command. Ruling 232, b566.)
  startGarrisons(g);
  // Volunteer legions (release 2): only where stage.json switches them on. (After the reserves of men are opened: the
  // record's legions in the line on the first day are raised from them.)
  openLegions(g);
  // On the wider map the picture is drawn once here, so that a campaign saved on its first day comes back as it was.
  if (wide) computeSupply(g);
  // The grand campaign counts its railheads with the supply picture: draw it afresh under these rules.
  g.supplyDirty = true;
  return g;
}

/** A draw between 0 and 1 that depends only on the war's seed and what is being decided, never on the battle dice. */
const draw = (g: Game, ...key: (string | number)[]): number => hashString(`${g.seed}:${key.join(':')}`) / 4294967296;
/**
 * A chance taken month by month. The plain hash above barely moves when only the month at the end of its key
 * changes, so a staff that drew badly in one month drew badly for years; this one is stirred first, and each
 * month is a throw of its own.
 */
export const monthly = (g: Game, ...key: (string | number)[]): number => new Rng(hashString(`${g.seed}:${key.join(':')}`)).next();

/** The character of a staff: how readily it attacks, and when it names a main effort. */
export type Character = 'bold' | 'cautious' | 'opportunist';
export interface CharacterRule { label: string; says: string; attackAt: number; minOdds: number; quiet: number; strength: number; local: number }

/** Rival staffs: the characters, the main effort, the will to finish. The numbers are in data/campaign/rivals.json. */
export const RIVALS = rivalsData as unknown as {
  characters: Record<Character, CharacterRule>;
  lean: Record<string, [number, number, number]>;
  effort: { months: number; rest: number; mass: number; sector: number; busy: number; patience?: { step: number; strength: number; odds: number } };
  press: { ratio: number; after: number; step: number; floor: number; waves: number; again: number };
  soften: { works: number; most: number; from: number; raid: number };
  fold: { worn: number; cadres: number };
  /** A staff that will not be ringed: who, how few ways out, how well one must be held to count as safe, how far the corps go. */
  ring?: { staffs: string[]; exits: number; safe: number; steps: number; reach: number; odds: number };
  relief?: { staffs: string[]; worn: number; fit: number; months: number };
  exhaust: { after: number; per: number };
  stalemate?: { after: number };
};

/** True where the staffs have characters and name main efforts: the grand campaign. */
export const rivalRules = (g: Game): boolean => isOpen(g) && !!RIVALS.characters;

/** The character of a nation's staff in this war: drawn once from the war's seed, with a national lean. */
export function characterOf(g: Game, nat: string): Character {
  // (The history dial: by the book each staff has its real counterpart's character, and higher up the lean is flattened.)
  const w = leanFor(g, RIVALS.lean[nat] ?? RIVALS.lean._default);
  const x = monthly(g, nat, 'character') * (w[0] + w[1] + w[2]);
  return x < w[0] ? 'bold' : x < w[0] + w[1] ? 'cautious' : 'opportunist';
}

/** A nation's character in this war: its nerve differs a little from one war to the next. */
export function temper(g: Game, nat: string): Temper {
  const t = { attackAt: 99, persist: 0.85, minOdds: 1.4, ...OPEN.temper._default, ...OPEN.temper[nat] } as Temper;
  // The staff's character (the grand campaign): a bold one goes on a smaller advantage, a cautious one waits for more.
  if (rivalRules(g)) { const C = RIVALS.characters[characterOf(g, nat)]; t.attackAt *= C.attackAt; t.minOdds += C.minOdds; }
  const mood = 1 + OPEN.mood * moodMul(g) * (2 * draw(g, nat, 'mood') - 1);
  // The level of the campaign steadies or stiffens the nerve of the player's rivals: at the fair level, neither.
  const lv = nerveOf(g, nat);
  // In the last act there is nobody to wait for: a staff strikes at shorter odds than it would in an ordinary war.
  const last = dominating(g) ? DOMINATION.nerve : { attackAt: 1, persist: 1 };
  return { attackAt: t.attackAt * mood * lv.nerve * last.attackAt, persist: t.persist * mood * lv.nerve * last.persist, minOdds: t.minOdds * lv.odds };
}

/** A stretch of country where this nation's side is in touch with an enemy: who is there, how strong, and the way in. */
export interface Front {
  /** The enemy who holds most of the country behind it. */
  key: string;
  enemies: string[];
  /** Friendly regions on it, and the enemy regions they face. */
  ours: number[];
  edge: number[];
  /** Steps into enemy country, 1 on the edge, counted from the stretches not covered by strong works. -1 elsewhere. */
  depth: Int16Array;
  /** The friendly land areas that touch it. */
  areas: Set<number>;
  /** Attack strength that can be brought to it, the enemy's defence near it, and the attack he could make from it. */
  atk: number; def: number; threat: number;
  /** Defence already standing on it in fixed positions: garrisons and fortress troops. */
  fixed: number;
  /**
   * atk, less what other fronts need to stay safe, over def. On a front whose whole edge stands on the far side: its
   * members over def, and no other front's corps (one membership: see `membership`); for a great choice weighing what
   * it could gather, its members and what the pool spares of the corps that are not its members, over def.
   */
  ratio: number;
  /** The attack strength of the front's members: set only for a front whose whole edge stands on the far side. */
  members?: number;
  /**
   * The same against the enemy on the line and one province behind it only. A main effort is a push at one place
   * for a season: his reserves deep in the country do not meet it, and a line held in depth is no reason to let it sleep.
   */
  near: number; defNear: number;
}

const atkP = (g: Game, c: Corps): number => corpsBase(g, c) * KIND[c.kind].atk * (g.nations[c.nat].slowCommand ? 0.85 : 1);
const defP = (g: Game, c: Corps, r: Region): number => corpsBase(g, c) * KIND[c.kind].def * TERRAIN_DEF[r.terrain];

export interface FrontOpts {
  /** Nations counted as enemies though no war has been declared yet: what a staff weighs before it decides. */
  extra?: Set<string>;
  /** Only this ground is looked at: an army overseas weighs its own theatre. */
  scope?: Set<number> | null;
  /** Ground left out: a side theatre that has a staff of its own. */
  skip?: (r: Region) => boolean;
  /** True for a front where an offensive is already under way: the troops committed to it are not free for another. */
  busy?: (f: Front) => boolean;
  /**
   * A great choice weighing what it could gather: the corps on garrison in taken ground are counted as they were before
   * they were named (garrisonMonth), and a far-side front reads its members and what the pool spares from outside them.
   */
  gather?: boolean;
  /** For a test or a reading: enemy ground on the far side is joined whoever holds it, as it was before each country's was a front of its own. Nothing in the game asks for it. */
  joined?: boolean;
  /** For a test or a reading: a far-side front reads what the pool spares, as it did before one membership. Nothing in the game asks for it. */
  pooled?: boolean;
}

/**
 * Ruling 221 (S): the long line one province past the ground this side holds beside a province on a front's edge,
 * as a staff reckons it: the shortest line among its own provinces there, and a fresh step on.
 *
 * Rider A of 20:45Z (20 November 2026) to ruling 234. The table of lines holds a nought for ground the supply table
 * does not feed at all, and the staff read that nought as the shortest line there is: in one war a single cut-off
 * province beside four provinces of a five-province edge read them at full strength, and opened the offensive that
 * ended China. Ground that is not fed gives no line and is passed over; where no fed ground stands beside the
 * province the answer is null, and that stretch of the edge is cut off.
 */
export function edgeLine(g: Game, nat: string, ours: { has(id: number): boolean }, id: number): number | null {
  const reach = g.reach[nat], fed = g.supply[nat];
  let best = Infinity;
  for (const j of g.regions[id].nb) if (ours.has(j) && (!fed || fed[j])) best = Math.min(best, reach ? reach[j] : 0);
  return best === Infinity ? null : best + EASTERN.stepFresh;
}

/** Every front this nation has, with the balance of strength on each. */
export function fronts(g: Game, nat: string, o: FrontOpts = {}): Front[] {
  const N = g.regions.length;
  const foe = (id: string): boolean => atWar(g, nat, id) || !!o.extra?.has(id);
  const inPlay = (r: Region): boolean => (o.scope ? o.scope.has(r.id) : !o.skip?.(r));
  // 1 friendly ground, 2 enemy ground.
  const side = new Int8Array(N);
  // Whose army holds the country counts here, not whose town it is: a town shut in by the enemy lies behind his front.
  const lord = g.regions.map((r) => fieldCtrl(g, r));
  // (Ground its holder stands on by agreement with its owner is no enemy ground to a country at peace with that
  // owner: no front forms over it. far.ts `stationBars`; the design director's ruling of 21:08Z of 21 November 2026.)
  const kept = (r: Region): boolean => stationed(g, r) && lord[r.id] === r.ctrl && r.owner !== nat && !foe(r.owner);
  for (const r of g.regions) if (inPlay(r)) side[r.id] = foe(lord[r.id]) && !kept(r) ? 2 : allied(g, nat, lord[r.id]) ? 1 : 0;
  const label = new Int16Array(N).fill(-1);
  let groups = 0;
  // In the last act every neighbour is an enemy and none of them is another's friend: each country is a front of
  // its own, to be weighed and taken on one at a time, not one wall of enemies right round the frontier.
  const each = dominating(g);
  // On the far side of the world each enemy country's ground is a front of its own from the first day, as the
  // record's commands stood: the Kwantung Army against the Soviet and Mongolian frontier, the armies in China against
  // China, Burma's apart from both. Joined, a Soviet war made one front of that frontier, Mongolia, all of China,
  // Burma and India, keyed to the Red Army, and a dated choice in Hunan was weighed against it. Far-side ground
  // only: where either of two neighbouring provinces stands on the old chart or in the Americas they are joined as
  // they were, so every front wholly on the old chart stands as it did. It holds for every staff: in Africa, which
  // the far chart holds, Italy's front against France's Tunisia and its front against Britain's Egypt are two, as its
  // Fifth and Tenth Armies were. What a front may use is still what the others spare (below).
  // (The design director's rulings of 05:10Z, 05:45Z and 13:06Z of 21 November 2026, form (a). Behind the far switch.)
  const apart = farOn(g) && !o.joined;
  for (const r of g.regions) {
    if (!side[r.id] || label[r.id] >= 0) continue;
    const q = [r.id];
    label[r.id] = groups;
    for (let i = 0; i < q.length; i++) {
      for (const j of g.regions[q[i]].nb) {
        if (label[j] >= 0 || side[j] !== side[r.id] || (each && side[j] === 2 && lord[j] !== lord[r.id])) continue;
        if (apart && side[j] === 2 && lord[j] !== lord[q[i]] && shoreOf(j) === 2 && shoreOf(q[i]) === 2) continue;
        label[j] = groups; q.push(j);
      }
    }
    groups++;
  }
  // Friendly areas where some of this nation's army stands: only from those can it reach a front.
  const manned = new Uint8Array(groups);
  for (const c of g.corps) if (c.nat === nat && !c.dead && side[c.reg] === 1) manned[label[c.reg]] = 1;

  const found = new Map<number, { ours: Set<number>; edge: Set<number>; areas: Set<number> }>();
  for (const r of g.regions) {
    if (side[r.id] !== 2) continue;
    for (const j of r.nb) {
      if (side[j] !== 1 || !manned[label[j]]) continue;
      // An ally's country is a way in only if that ally is in this war too: nobody attacks out of a neutral's garden.
      const host = lord[j];
      if (host !== nat && !atWar(g, host, lord[r.id])) continue;
      let f = found.get(label[r.id]);
      if (!f) found.set(label[r.id], (f = { ours: new Set(), edge: new Set(), areas: new Set() }));
      f.ours.add(j); f.edge.add(r.id); f.areas.add(label[j]);
    }
  }

  const wx = weather(g);
  const long = lineOwners(g);
  const out: Front[] = [];
  // (One membership, far side: what each corps counted toward a front brings, kept until the fronts are all found.
  // A great choice weighing what it could gather reads its front's members too, and beside them what the pool
  // spares of the corps that are not its members: its redeploy does bring those up. See the prospect, below.)
  const one = farOn(g) && !o.pooled;
  const brought = new Map<Front, [Corps, number][]>();
  for (const [comp, f] of found) {
    // Works only stop an attack that comes from the country they were built against: the best way in is what counts.
    const works = (e: Region): number => {
      if (!e.fort) return 1;
      let best = 99;
      if (!worksAgainst(g, e, nat)) return 1;
      for (const j of e.nb) if (f.ours.has(j)) best = Math.min(best, e.fort.vs.includes(g.regions[j].owner) ? FORT_MUL[e.fort.lv] : 1);
      return best === 99 ? 1 : best;
    };
    const edge = [...f.edge];
    const soft = edge.filter((id) => works(g.regions[id]) < 1.5);
    const depth = new Int16Array(N).fill(-1);
    const q = soft.length ? [...soft] : [...edge];
    for (const id of q) depth[id] = 1;
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (depth[j] < 0 && label[j] === comp && side[j] === 2) { depth[j] = depth[q[i]] + 1; q.push(j); }

    let def = 0, defNear = 0, threat = 0;
    const share: Record<string, number> = {};
    const enemies = new Set<string>();
    for (const id of q) {
      const r = g.regions[id], d = depth[id];
      enemies.add(lord[id]);
      const near = d === 1 ? 1 : d === 2 ? 0.6 : d === 3 ? 0.35 : 0.12;
      for (const u of r.units) {
        if (!foe(u.nat)) continue;
        enemies.add(u.nat);
        const still = u.hold || u.kind === 'fort';
        // An enemy caught by the Russian winter without the kit for it is weaker than he looks on the map.
        const cold = frozen(g, u.nat, r);
        const p = defP(g, u, r) * (d === 1 ? works(r) : 1) * near * (still && d > 1 ? 0.3 : 1) * (cold ? EASTERN.winter.defence : 1);
        def += p;
        if (d <= 2) defNear += p;
        share[u.nat] = (share[u.nat] ?? 0) + p;
        if (!still && d <= 2) threat += atkP(g, u) * (d === 1 ? 1 : 0.6) * (cold ? EASTERN.winter.attack : 1);
      }
    }
    // A stretch of the edge behind strong works is still a place the enemy can come out of.
    for (const id of edge) if (depth[id] > 2) for (const u of g.regions[id].units) if (foe(u.nat) && !u.hold && u.kind !== 'fort') threat += atkP(g, u) * 0.6;

    // Whoever on this side is at war with someone there, and can get there by land, counts toward the attack.
    const ours = [...f.ours];
    const close = new Uint8Array(N);
    for (const id of ours) { close[id] = 1; for (const j of g.regions[id].nb) if (side[j] === 1) close[j] = 1; }
    const fights = (id: string): boolean => id === nat || [...enemies].some((e) => atWar(g, id, e));
    let atk = 0, fixed = 0;
    const tally: [Corps, number][] | null = one && edge.every((id) => shoreOf(id) === 2) ? [] : null;
    // Deep in Russia in the bad months an army that has not learned the country attacks at a fraction of its strength.
    const deep = edge.filter((id) => EASTERN.deepOwners.includes(g.regions[id].owner)).length * 2 >= edge.length;
    let season = deep && !EASTERN.winter.hardy.includes(nat) ? (wx.winter ? EASTERN.winter.attack : wx.mud ? 0.8 : 1) : 1;
    // Ruling 221 (S): where the long line runs on ground the Russian winter does not reach, a staff reckons it as it
    // reckons the winter: the strength its corps would have one province past the line they hold, averaged along the edge.
    if (EASTERN.lineReckon && !long.includes(nat)) {
      const reach = g.reach[nat];
      const far = edge.filter((id) => long.includes(g.regions[id].owner) && !EASTERN.deepOwners.includes(g.regions[id].owner));
      if (reach && far.length) {
        // (Rider A of 20:45Z, 20 November 2026, to ruling 234: a stretch with no fed ground beside it is cut off, and
        // reads the longest line the bands know. See edgeLine.)
        const cutOff = EASTERN.bands[EASTERN.bands.length - 1][1];
        let sum = 0;
        for (const id of far) {
          const len = edgeLine(g, nat, f.ours, id);
          sum += len === null ? cutOff : (EASTERN.bands.find(([max]) => len <= max) ?? EASTERN.bands[EASTERN.bands.length - 1])[1];
        }
        season *= sum / far.length;
      }
    }
    for (const c of g.corps) {
      if (c.dead || side[c.reg] !== 1 || !f.areas.has(label[c.reg]) || !allied(g, nat, c.nat) || !fights(c.nat)) continue;
      const r = g.regions[c.reg];
      // (Rider C of 20:45Z, 20 November 2026, to ruling 234: a corps on garrison in ground taken is not free, though
      // it stands on the front itself. Only a great choice weighing what it could gather counts it. See garrisonMonth.)
      const posted = c.garr !== undefined && !o.gather;
      if (c.kind === 'fort' || c.hold || posted) {
        if (f.ours.has(c.reg)) fixed += defP(g, c, r) * (r.fort && worksFor(g, r, c.nat) ? FORT_MUL[r.fort.lv] : 1);
        // A garrison on the front itself would go in with the rest; one further back stays where it is.
        if (c.kind === 'fort' || c.nat !== nat || !close[c.reg] || posted) continue;
      }
      atk += atkP(g, c) * season;
      // (A row that holds and stays is the front's defence and never its attack: the staff cannot move it. A corps
      // named to a garrison holds and stays by that name alone: a great choice weighing what it could gather counts
      // it as it was before it was named, rider C.)
      if (tally && !(c.hold && c.stay && !(o.gather && c.garr !== undefined))) tally.push([c, atkP(g, c) * season]);
    }
    // The air forces: whoever is stronger in the air has that much more on the ground.
    const key = Object.keys(share).sort((a, b) => share[b] - share[a])[0] ?? g.regions[edge[0]].ctrl;
    const mine = g.nations[nat].air, his = g.nations[key]?.air ?? 0;
    const airEdge = (mine - his) / (mine + his + 1);
    atk *= 1 + 0.3 * airEdge;
    if (tally) for (const t of tally) t[1] *= 1 + 0.3 * airEdge;
    def *= 1 - 0.2 * airEdge;
    defNear *= 1 - 0.2 * airEdge;
    // With no way round, the works have to be stormed head-on, and that takes far more than the sums say.
    if (!soft.length) { def *= OPEN.headOn; defNear *= OPEN.headOn; }
    out.push({ key, enemies: [...enemies], ours, edge, depth, areas: f.areas, atk, def: Math.max(def, 0.01), threat, fixed, ratio: 0, near: 0, defNear: Math.max(defNear, 0.01) });
    if (tally) brought.set(out[out.length - 1], tally);
  }
  // What a front can be given is what the others, reached from the same country, can spare: an offensive already
  // under way elsewhere keeps the force it needs, and a quiet front keeps enough to hold it.
  //
  // One membership (the design director's rulings of 14:08Z and 14:31Z of 21 November 2026): on a front whose whole
  // edge stands on the far side there is no pool. A corps on a front's own province counts to that front; a corps
  // behind the lines is a member of the front nearest it; and the front reads its members over the defence, whatever
  // the others are doing: a thin front reads thin however full the country is. Corps move between fronts by the
  // plan's rows, a great choice's assembly, the railways and the garrison rule, never by the reading. A front with a
  // province of the old chart or the Americas on its edge reads as it did.
  //
  // The prospect (the design director's 19:52Z of 21 November 2026): a great choice weighs what it will move. On a
  // far-side front that is the front's own members, who are already there, and what its redeploy would bring from
  // outside the front: the corps of the pool that are not its members, less what the other fronts keep. One map, and
  // no corps counted twice. Before it a great choice kept the whole pool, the other fronts' cover came off the
  // front's own members too, and with a great war on beside it (the Soviet Union's in 1945) a front that read well
  // over one to one on its members weighed nothing.
  const M = brought.size ? membership(g, nat, out, (id) => side[id] === 1) : null;
  const keeps = new Map<Front, number>();
  for (const f of out) {
    const far = !!M && brought.has(f);
    if (far && !o.gather) continue;
    let kept = 0;
    for (const x of out) {
      if (x === f || ![...x.areas].some((a) => f.areas.has(a))) continue;
      const cover = Math.max(0, 0.55 * x.threat - 0.7 * x.fixed);
      kept += o.busy?.(x) ? Math.max(cover, Math.min(x.atk, OPEN.commit * x.def)) : cover;
    }
    if (far) { keeps.set(f, kept); continue; }
    f.ratio = Math.max(0, f.atk - kept) / f.def;
    f.near = Math.max(0, f.atk - kept) / f.defNear;
  }
  out.sort((a, b) => b.def - a.def);
  // (The members are read in the fronts' own order: a front with an offensive on speaks for the corps on its line.)
  if (M) {
    for (const f of out) READ.set(f, { map: M, brought: brought.get(f) ?? null });
    readMembers(out, o.busy);
    for (const [f, kept] of keeps) {
      let outside = 0;
      for (const [c, v] of brought.get(f)!) if (!M.counts(c, f)) outside += v;
      const sum = (f.members ?? 0) + Math.max(0, outside - kept);
      f.ratio = sum / f.def;
      f.near = sum / f.defNear;
    }
  }
  return out;
}

/** What a staff's far-side fronts were read from: the one map, and what each counted corps brings to this front (null for a front that reads the pool). */
const READ = new WeakMap<Front, { map: Membership; brought: [Corps, number][] | null }>();

/**
 * The membership a staff's far-side fronts were summed by: the one map, for whoever reads the fronts after fronts()
 * (posture, what a front can spare). It is known to every front fronts() found beside a far-side one. Undefined
 * where no front of the list reads its members: the far side off, and a staff whose fronts all have a province of
 * the old chart or the Americas on their edge. (The fronts a great choice weighs know it too: the prospect.)
 */
export const membershipOf = (f: Front | undefined): Membership | undefined => (f ? READ.get(f)?.map : undefined);

/** Told, as a front's members are read, of each corps it did not count because a front before it, its offensive on, had spoken for it: taken `by` that front `from` this one. For tests and readings. */
export const membersWatch: { tell: ((e: { by: Front; from: Front; corps: Corps }) => void) | null } = { tell: null };

/**
 * Reads the members of each far-side front of a list again: its `members`, `ratio` and `near`. The list is fronts()'
 * own, in its order or a part of it in that order.
 *
 * Spoken for (the design director's 14:31Z of 21 November 2026, condition 1): a corps that stands on two fronts'
 * own provinces is weighed by both while both are quiet, but it fights for one. A front with an offensive on (`on`)
 * takes the corps that stand on its own provinces, and a front after it in the order no longer counts them. A
 * staff's month reads its fronts in that order and calls this again as each decides, so no corps carries two
 * offensives at once.
 */
export function readMembers(list: Front[], on?: (f: Front) => boolean): void {
  const taken = new Map<Corps, Front>();
  for (const f of list) {
    const R = READ.get(f);
    if (!R?.brought) continue;
    let sum = 0;
    for (const [c, v] of R.brought) {
      if (!R.map.counts(c, f)) continue;
      const by = taken.get(c);
      if (by) membersWatch.tell?.({ by, from: f, corps: c }); else sum += v;
    }
    f.members = sum;
    f.ratio = sum / f.def;
    f.near = sum / f.defNear;
    if (on?.(f)) for (const [c] of R.brought) if (f.ours.includes(c.reg) && !taken.has(c)) taken.set(c, f);
  }
}

/** Whose corps are whose front's: see `membership`. */
export interface Membership {
  /** The front the corps of this nation's that stand on a province belong to: null where no front is reached by land. */
  at(id: number): Front | null;
  /** The marches from a province to that front: 0 on the front itself, -1 where none is reached. */
  marches: Int16Array;
  /** The front a corps belongs to: this nation's by where it stands, an ally's by the fronts its country fights on. */
  of(c: Corps): Front | null;
  /**
   * True where a corps counts toward a front's strength: the front is its own, or the corps stands on the front
   * itself, on one of its provinces and in touch with its enemy's ground (an ally's corps only where its country
   * fights that enemy). A corps on two lines at once counts on both: nothing has to move for either to use it.
   */
  counts(c: Corps, f: Front): boolean;
}

/**
 * One membership (the design director's ruling of 14:08Z of 21 November 2026, far side). A corps is a member of one
 * front and no more: the front nearest it by the follow-through's marches through its side's ground, a march a
 * province, counted from the front's own provinces (`ours`). Where two fronts are as near, the one with the larger
 * defence has it; where those are equal too, the one that comes first in the list.
 *
 * As pooled, every corps in a land area counted toward every front that area touched, less what the others kept,
 * and the follow-through then sent each corps to the front nearest it: one map was weighed and another fought. A
 * front before Hong Kong with no corps of its own read five to one off the army in China, none of which the
 * follow-through would ever bring.
 *
 * The list is the fronts as fronts() found them, in any order; fronts() is not called here. `friendly` says which
 * ground is this side's as fronts() reads it (its `side` of 1); left out, it is every province where the army in
 * the field is this nation's or an ally's. An ally's corps fights only where its country is at war: its front is
 * the nearest of the fronts it fights on, by the same marches.
 *
 * A province may stand on two fronts at once (Tungkun, before Hong Kong and before Waichow), and the tie gives it
 * to one of them: a front whose every province stands on a larger front too would never have a member, whatever
 * stood there. So a front's strength counts its members and, with them, every corps that stands on the front itself
 * (`counts`): such a corps is in touch with both enemies and nothing has to move for either front to use it.
 *
 * This is the reading only: who is left out of a front's sum (fortress troops, held rows away from the line,
 * garrisons named this month) is for the sum to say, corps by corps, as fronts() does.
 */
export function membership(g: Game, nat: string, found: Front[], friendly?: (id: number) => boolean): Membership {
  const N = g.regions.length;
  // (Its own copy of the list: the map knows a front by its place in it, and the caller may sort his afterwards.)
  const list = [...found];
  const ok = friendly ?? ((id: number): boolean => allied(g, nat, fieldCtrl(g, g.regions[id])));
  const order = list.map((_, k) => k).sort((a, b) => list[b].def - list[a].def || a - b);
  const read = (among: (f: Front) => boolean): { of: Int16Array; marches: Int16Array } => {
    const of = new Int16Array(N).fill(-1), marches = new Int16Array(N).fill(-1);
    const q: number[] = [];
    // (Laid down in the order of the tie: a queue of marches then hands each province to the first front that is
    // nearest, the larger defence before the smaller.)
    for (const k of order) {
      if (!among(list[k])) continue;
      for (const id of list[k].ours) if (of[id] < 0) { of[id] = k; marches[id] = 0; q.push(id); }
    }
    for (let i = 0; i < q.length; i++) {
      const a = q[i];
      for (const j of g.regions[a].nb) if (of[j] < 0 && ok(j)) { of[j] = of[a]; marches[j] = marches[a] + 1; q.push(j); }
    }
    return { of, marches };
  };
  const mine = read(() => true);
  const allies = new Map<string, Int16Array>();
  const front = (k: number): Front | null => (k >= 0 ? list[k] : null);
  const of = (c: Corps): Front | null => {
    if (c.nat === nat) return front(mine.of[c.reg]);
    let m = allies.get(c.nat);
    if (!m) allies.set(c.nat, (m = read((f) => f.enemies.some((e) => atWar(g, c.nat, e))).of));
    return front(m[c.reg]);
  };
  const lines = new Map<Front, Set<number>>();
  const standsOn = (c: Corps, f: Front): boolean => {
    let line = lines.get(f);
    if (!line) lines.set(f, (line = new Set(f.ours)));
    return line.has(c.reg) && (c.nat === nat || f.enemies.some((e) => atWar(g, c.nat, e)));
  };
  return { at: (id) => front(mine.of[id]), marches: mine.marches, of, counts: (c, f) => of(c) === f || standsOn(c, f) };
}

// ---- Ground taken has to be held (rider C of 20:45Z, 20 November 2026, to ruling 234) ------------------------
//
// After Wuhan Japan's army in China held its gains as points and lines, about three quarters of it on garrison
// (the historian, japan-china-garrison-1939-41.md; the director's note of 20:50Z). As built a staff's sums counted
// every corps its row did not hold as free for the offensive, and left nobody behind for fifty provinces taken in
// eight months.
//
// Every province a country holds of ground neither its own nor an ally's, beyond the old chart, asks a garrison of
// so much of a division's worth (divisions by strength) that stands in that ground: one sum for each piece of taken
// ground that is one piece by land. What stands there held already counts first, as it stands: fortress troops,
// rows held by their rows or by a plan, corps inside a town, a client's corps on its patron's ground. Then the
// staff names its own free corps there: one it named last month before any other, the foot before the fast troops,
// the rear first (the farthest from ground an enemy holds, then the farthest from his capital, so that the corps
// nearest it stay free for the blow), then the weakest, then the table's order. Armour is never named. A corps is
// taken while it brings the garrison nearer the sum asked, so the garrison is as near it as whole corps allow. A
// corps the staff names is held and marked to stay for as long as it is named, by the hold rule's form: no general
// offensive's release takes it. A staff's sums of strength count none of them (fronts).
//
// A garrison is a choice and not a vow. A great choice in preparation or being pressed draws on the garrison of the
// ground its enemies own: that ground asks nothing while it is, and the choice weighs beforehand what it could
// gather, garrisons and all (prospect). The rows a plan keeps for itself are the plan's and are never named. Ground
// left thin may be walked back into (ruling 216(d)); nothing else follows from it.
//
// It binds a computer staff, at the stops its row names (the far side's rows, china-stop3.json). A hold never binds
// a player's corps, and none of his is named. Nobody's are with the far side off.

/** A corps' worth as a garrison: its divisions by its strength. */
const garrisonWorth = (c: Corps): number => c.divs * c.str;

/** True while a great choice of this staff's draws on the garrison of ground this country owns: one it is preparing against it, or one it is pressing. */
function drawsOn(g: Game, nat: string, owner: string): boolean {
  if (g.turn <= flag(g, `st:push:${nat}`) && flag(g, `st:pushed:${nat}:${owner}`)) return true;
  const op = ops(nat)[flag(g, `st:op:${nat}`) - 1];
  return !!op && (targets(g, nat, op).includes(owner) || inWar(g, nat, op).includes(owner));
}

/** A city's province (a province is its own), on the wider chart. */
const provinceOf = (id: number): number => (id >= WORLD_FIRST ? (WORLD_MAP.regions[id - WORLD_FIRST] as { of?: number } | undefined)?.of ?? id : id);
let garrisonBy: Map<string, number> | null = null;
let garrisonOwn: Map<string, number> | null = null;
let provinceParts: Map<number, number> | null = null;
/**
 * What one province of taken ground asks (rider of 22:30Z, 20 November 2026, to ruling 234, and the note of 22:28Z on
 * the historian's paper, historian/japan-south-garrison-1942.md): the record's figure for its region where the row
 * gives one (`by`: the Philippines, Malaya, Sumatra, Java, Borneo and the eastern islands, Burma), and the row's
 * `each`, sized from China's record, everywhere else. The historian's figures are the record's men over the
 * provinces of the far side's first chart; where the chart cuts one of those finer, its parts share the figure, so
 * that a region asks the men the record gives it and no more.
 *
 * A row marked `own` names its places as the chart is cut, and each asks the figure itself: the record's sum for a
 * region spread over the ground a country held of it on a day (the design director's 05:14Z of 21 November 2026:
 * North China asks 13 across the 23 places Japan held on the first day and the centre about 7 across its 11). The
 * other parts of those provinces, taken later, ask as before.
 */
export function garrisonAsk(g: Game, r: Region): number {
  const C = OPEN.garrison;
  if (!C) return 0;
  if (!garrisonBy || !garrisonOwn) { garrisonBy = new Map(); garrisonOwn = new Map(); for (const row of C.by ?? []) for (const nm of row.places) (row.own ? garrisonOwn : garrisonBy).set(nm, row.each); }
  const own = garrisonOwn.get(r.name);
  if (own !== undefined) return own;
  const of = provinceOf(r.id), fig = garrisonBy.get(g.regions[of]?.name ?? '');
  if (fig === undefined) return C.each;
  if (!provinceParts) { provinceParts = new Map(); for (const q of g.regions) { const p = provinceOf(q.id); provinceParts.set(p, (provinceParts.get(p) ?? 0) + 1); } }
  return fig / (provinceParts.get(of) ?? 1);
}

/**
 * Each piece of ground this country has taken, with the garrison it asks this month. On the Pacific's ground as the
 * landing rules read it (pacific.ts, `pacificGround`: the far side's first slice with its cities, and the coasts of
 * the Bay of Bengal's seas), and no farther: Africa, Arabia, Iran, inner Russia, Afghanistan and inland India wait
 * for the rule's general form, each with a record figure of its own (rider of 22:30Z, 1).
 */
export function takenGround(g: Game, nat: string): { ids: number[]; ask: number }[] {
  const C = OPEN.garrison;
  if (!C || !farOn(g)) return [];
  // (Ground held by agreement with its owner is not taken ground: the agreement's own troops stand on it, and the
  // owner's garrison keeps its posts. far.ts `stationed`; the design director's form 2 of 21:08Z, 21 November 2026.)
  const taken = (r: Region): boolean => r.ctrl === nat && r.owner !== nat && pacificGround(g, r) && !allied(g, nat, r.owner) && !stationed(g, r);
  const seen = new Uint8Array(g.regions.length);
  const out: { ids: number[]; ask: number }[] = [];
  for (const r0 of g.regions) {
    if (seen[r0.id] || !taken(r0)) continue;
    const ids = [r0.id];
    seen[r0.id] = 1;
    for (let i = 0; i < ids.length; i++) for (const j of g.regions[ids[i]].nb) if (!seen[j] && taken(g.regions[j])) { seen[j] = 1; ids.push(j); }
    // (To two places, as the record's figures are.)
    out.push({ ids, ask: Math.round(ids.reduce((s, id) => s + (drawsOn(g, nat, g.regions[id].owner) ? 0 : garrisonAsk(g, g.regions[id])), 0) * 100) / 100 });
  }
  return out;
}

/**
 * A computer staff's garrisons for the month, named before it reads its fronts. `plans` is true for a row a plan
 * keeps for itself (ai.ts passes the plan's fronts): such a row is never named.
 */
export function garrisonMonth(g: Game, nat: string, plans?: (c: Corps) => boolean): void {
  const C = OPEN.garrison, n = g.nations[nat];
  if (!C || !isOpen(g) || !farOn(g) || !C.stops.includes(stopOf(g)) || nat === g.human || !n?.alive) return;
  // What stood there held last month is counted afresh.
  for (const c of g.corps) if (c.garr === 0 && (c.nat === nat || patronOfClient(c.nat) === nat)) delete c.garr;
  // Marches by land from ground an enemy holds: the rear is where that is farthest, or where no road leads at all.
  const N = g.regions.length, dist = new Int16Array(N).fill(-1), q: number[] = [];
  for (const r of g.regions) if (atWar(g, nat, r.ctrl)) { dist[r.id] = 0; q.push(r.id); }
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (dist[j] < 0) { dist[j] = dist[q[i]] + 1; q.push(j); }
  const rear = (c: Corps): number => (dist[c.reg] < 0 ? N : dist[c.reg]);
  // And marches from the capitals of the enemies it is at war with: of two corps as far to the rear, the one farther
  // from all of them is the garrison, and the one nearer stays free for the blow.
  const goal = new Int16Array(N).fill(-1), qc: number[] = [];
  for (const e of n.atWar) { const cap = g.nations[e]?.alive ? g.nations[e].capital : -1; if (cap >= 0 && goal[cap] < 0) { goal[cap] = 0; qc.push(cap); } }
  for (let i = 0; i < qc.length; i++) for (const j of g.regions[qc[i]].nb) if (goal[j] < 0) { goal[j] = goal[qc[i]] + 1; qc.push(j); }
  const away = (c: Corps): number => (goal[c.reg] < 0 ? N : goal[c.reg]);
  const order = (a: Corps, b: Corps): number => Number(KIND[a.kind].mobile) - Number(KIND[b.kind].mobile) || rear(b) - rear(a) || away(b) - away(a) || garrisonWorth(a) - garrisonWorth(b) || a.id - b.id;
  const named = new Set<Corps>();
  for (const piece of takenGround(g, nat)) {
    let sum = 0;
    // A whole corps is taken while it brings the garrison nearer the sum asked.
    const nearer = (c: Corps): boolean => garrisonWorth(c) > 0 && garrisonWorth(c) / 2 < piece.ask - sum;
    const stand = piece.ids.flatMap((id) => g.regions[id].units.filter((c) => !c.dead && c.kind !== 'eng' && c.joint === undefined && c.leave === undefined));
    const standing = (c: Corps): boolean => (c.nat === nat ? c.garr !== 1 && (c.kind === 'fort' || c.hold || c.town !== undefined) : patronOfClient(c.nat) === nat);
    for (const c of stand.filter(standing).sort(order)) if (nearer(c)) { sum += garrisonWorth(c); c.garr = 0; }
    // (Armour is never named to hold ground: armies held ground with their infantry and kept their armour with the
    // field army. If the rest leave the sum short, it stays short. The design director's 05:51Z of 21 November 2026.)
    const own = stand.filter((c) => c.nat === nat && c.kind !== 'fort' && c.kind !== 'arm' && c.town === undefined && (c.garr === 1 || !c.hold) && !plans?.(c));
    for (const c of own.sort((a, b) => Number(b.garr === 1) - Number(a.garr === 1) || order(a, b))) if (nearer(c)) { sum += garrisonWorth(c); named.add(c); }
  }
  const mine = [...g.corps, ...(g.atSea ?? []).map((v) => v.corps)];
  for (const c of mine) {
    if (c.nat !== nat) continue;
    if (named.has(c)) { c.garr = 1; c.hold = true; c.stay = true; }
    else if (c.garr === 1) { c.hold = false; delete c.stay; delete c.garr; }
  }
}

/** For the measuring harness and the tests: told of each rear garrison as it comes. Nothing in the game listens. */
export const rearWatch: { tell: ((e: { nat: string; at: Region; corps: Corps; worth: number }) => void) | null } = { tell: null };

/**
 * The rear garrison of a taken port (the design director's ruling of 16:36Z of 21 November 2026, with 16:44Z and
 * 16:46Z, on the trace of "offensives against nothing": pacific-r2/membership/READ-STOOD-12.md).
 *
 * As measured on 12 wars, the empty ground Japan's fronts had orders for stood empty with a corps beside it for 688
 * province-months, and for 400 of them what kept that corps was the last-guard rule (ai.ts `cityBound`): a landing
 * force alone in the port it had taken, Rabaul's in every war and every month. The rule worked; what was missing was
 * the record's rear. No taken port was left empty: a navy base force in the navy's ground, garrison and
 * line-of-communication troops in the army's, took each port over within weeks, and the landing force went on
 * (historian/japan-taken-towns-1942.md, sections 3 to 6).
 *
 * So a port a country has taken gets that country's rear-garrison row (a city the last guard binds as a port is
 * read with the ports: Mandalay). Here, by cause, at the stops the table names: the month after the port is taken
 * and only while it is still held, never by a date alone; once for each taking. On ground taken from the countries
 * the owner's table covers, and no other: the record behind Japan's table is the south's, and China's ground keeps
 * the garrison its own record gives it. Its size is the record's figure for the town at its middle and, for a port
 * the table does not name, the table's `each`. Its weight is drawn from its owner's reserve of men as it comes, and
 * no row comes while the reserve cannot give it: the last guard then stays, as before. The row holds and stays. At
 * the book's stops the record's own dated rows hold the ports and nothing comes from here. The freed corps is its
 * staff's or its plan's as before: no row is pointed anywhere by this.
 *
 * It is one rule for everybody (the director's 19:44Z, on the read of what a player can do): a player's country
 * gets the same row at the same cost, men from his reserve and no points, as his month opens (turn.ts beginHuman),
 * and his month's report says so in one line (the table's 'says', one port or several). A player can raise only whole corps, and only at
 * home, and can part none: without this he had less than the computer.
 *
 * The form is by owner and no one country's (the director's 19:24Z and 19:40Z): each country reads its own table
 * and nobody another's, Japan its base forces and garrison troops, the United States and Britain a base command at
 * the middle of the record's 0.05 to 0.1 (historian/rear-garrisons-us-britain-1941-45.md); a country with no table
 * gets no row. "Taken" is taken in war: the port's last holder was an enemy the country still fights. A living
 * ally's port goes back to that ally as it is freed (game.ts capture), and the row is then that ally's own, from
 * its own table (Tarawa: Britain's, whoever freed it); so is a country's own port that it takes back. A fallen
 * ally's port stays the taker's, and gets the taker's row. Ground that passed into a country's keeping as its owner
 * fell was not taken.
 */
export function rearMonth(g: Game, nat: string): void {
  const C = OPEN.rear, T = C?.by?.[nat], n = g.nations[nat];
  if (!C || !T || !isOpen(g) || !farOn(g) || !C.stops.includes(stopOf(g)) || !n?.alive) return;
  const told: Region[] = [];
  for (const r of g.regions) {
    // (A place the last guard could bind, a port or a city (ai.ts `cityBound`), that the country holds on the
    // Pacific's ground: its own, freed, or ground its table covers.)
    if (!(r.port || r.vp > 0 || isCity(r)) || r.ctrl !== nat || !(r.owner === nat || T.of.includes(r.owner)) || !pacificGround(g, r)) continue;
    // (A living ally's ground is that ally's to hold. And the port was taken from an enemy still fought: ground that
    // came into a country's keeping as its owner fell, game.ts capitulate, was not taken, and has its kept garrison.)
    if ((r.owner !== nat && allied(g, nat, r.owner) && g.nations[r.owner]?.alive) || !r.prev || !atWar(g, nat, r.prev)) continue;
    const since = r.since ?? 0;
    if (since <= 0 || g.turn <= since || g.flags[`rear:${r.id}`] === since) continue;
    if (hostileUnits(g, r, nat).length || friendlyUnits(g, r, nat).filter((u) => u.kind !== 'eng').length >= STACK) continue;
    const row = T.towns[r.name], worth = row?.[0] ?? T.each;
    if (!(worth > 0)) { g.flags[`rear:${r.id}`] = since; continue; }
    if (menRules(g) && menLeft(g, nat) + 1e-9 < worth) continue;
    drawMen(g, nat, worth);
    const c = addCorps(g, nat, row?.[1] ?? T.name.replace('{place}', r.name), 'inf', 1, r.id, { xp: C.xp ?? 0.1, est: worth, hold: 1, stay: 1 });
    c.str = worth;
    g.flags[`rear:${r.id}`] = since;
    g.supplyDirty = true;
    if (nat === g.human) told.push(r);
    rearWatch.tell?.({ nat, at: r, corps: c, worth });
  }
  // (A player sees them come: one line in his month's report however many ports got their guard, the table's own
  // words, the first three by name and then how many more. The UI director's of 19:48Z of 21 November 2026,
  // ui-director/words-far-guards-indochina.md: a line for each port would fill the report in Japan's first months.)
  const says = T.says ?? C.says;
  if (told.length && says) {
    const names = told.slice(0, 3).map((r) => r.name), more = told.length - names.length;
    const places = more > 0 ? `${names.join(', ')} and ${more} more` : names.length > 1 ? `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}` : names[0];
    headline(g, 'info', says[told.length > 1 ? 1 : 0].replace('{places}', places), nat, told[0].id);
  }
}

/** The places an offensive on this front is aimed at: the enemy's capitals first, then what is worth having near the front. */
function objectives(g: Game, f: Front): string[] {
  const inside = g.regions.filter((r) => f.depth[r.id] >= 1);
  const caps = inside.filter((r) => r.cap && f.depth[r.id] <= 7 && g.nations[r.owner]?.alive && f.enemies.includes(r.owner)).sort((a, b) => f.depth[a.id] - f.depth[b.id]);
  const worth = inside.filter((r) => f.depth[r.id] <= 4 && (r.vp >= 1 || r.port)).sort((a, b) => f.depth[a.id] - f.depth[b.id] || b.vp - a.vp || a.id - b.id);
  const near = inside.filter((r) => f.depth[r.id] <= 2).sort((a, b) => f.depth[a.id] - f.depth[b.id] || a.id - b.id);
  return [...new Set([...caps, ...worth, ...near].map((r) => r.name))].slice(0, 30);
}

const GOING: Record<string, number> = { plains: 0, desert: 0.3, forest: 0.5, hills: 0.6, urban: 0.8, marsh: 1.5, mountain: 2 };

/** The line the fast troops are sent along: the easiest road from the front to the nearest enemy capital, round the works and the mountains. */
function axis(g: Game, nat: string, f: Front, placed = false): string[] {
  const N = g.regions.length;
  const cost = new Float32Array(N).fill(1e9);
  const from = new Int16Array(N).fill(-1);
  const step = (r: Region): number => 1 + (GOING[r.terrain] ?? 0) + (r.fort && r.fort.vs.includes(nat) && worksAgainst(g, r, nat) ? 2.5 * r.fort.lv : 0) + 0.2 * r.units.length;
  const open: number[] = [];
  const doors = f.edge.filter((id) => f.depth[id] === 1);
  // With the army in the field, the road starts where the army is: a thrust cannot be launched out of a corner
  // of the front held by a corps or two, however short the way from there.
  const push = (id: number): number => {
    let sum = 0;
    for (const j of g.regions[id].nb) if (allied(g, nat, fieldCtrl(g, g.regions[j]))) for (const u of g.regions[j].units) if (u.nat === nat && u.kind !== 'fort' && !u.dead) sum += atkP(g, u);
    return sum;
  };
  const force = placed ? doors.map(push) : [];
  const floor = OPEN.foothold * Math.max(0, ...force);
  doors.forEach((id, k) => { if (!placed || force[k] >= floor) { cost[id] = step(g.regions[id]); open.push(id); } });
  // Few regions: a plain search, cheapest first.
  const done = new Uint8Array(N);
  for (;;) {
    let at = -1;
    for (const id of open) if (!done[id] && (at < 0 || cost[id] < cost[at])) at = id;
    if (at < 0) break;
    done[at] = 1;
    for (const j of g.regions[at].nb) {
      if (f.depth[j] < 1 || done[j]) continue;
      const c = cost[at] + step(g.regions[j]);
      if (c < cost[j]) { if (cost[j] === 1e9) open.push(j); cost[j] = c; from[j] = at; }
    }
  }
  let goal: Region | null = null;
  for (const r of g.regions) {
    if (cost[r.id] >= 1e9 || !r.cap || !g.nations[r.owner]?.alive || !f.enemies.includes(r.owner)) continue;
    if (!goal || cost[r.id] < cost[goal.id]) goal = r;
  }
  if (!goal) for (const r of g.regions) if (cost[r.id] < 1e9 && f.depth[r.id] <= 5 && r.vp >= 2 && (!goal || r.vp > goal.vp || (r.vp === goal.vp && cost[r.id] < cost[goal.id]))) goal = r;
  if (!goal) return [];
  const path: string[] = [];
  for (let id = goal.id; id >= 0; id = from[id]) path.unshift(g.regions[id].name);
  return path;
}

/**
 * The month's bound: how much of the enemy's front the staff lets the offensive take on before the line is put in
 * order and supply brought up. No army attacks everywhere at once. The main thrust goes in along its road (five
 * regions deep in the opening month of a nation that makes war by speed, two after that) with the ground on either
 * side of it; on the rest of the front only the prizes and the weak places are attacked. A front with no thrust,
 * a side-show or a theatre overseas, is taken on whole, to the depth given.
 */
function bound(g: Game, nat: string, f: Front, way: string[], opening: boolean, reach = OPEN.bound): string[] {
  const out = new Set<string>();
  const thrust = way.slice(0, opening && g.nations[nat].blitz ? OPEN.thrust[0] : OPEN.thrust[1]).map((nm) => g.byName[nm]).filter(Boolean);
  // An army with overwhelming strength does not wait on one road: it presses the whole of the enemy's front.
  if (thrust.length && f.ratio >= OPEN.general) for (const id of f.edge) out.add(g.regions[id].name);
  if (!thrust.length) {
    for (const r of g.regions) if (f.depth[r.id] >= 1 && f.depth[r.id] <= reach) out.add(r.name);
    for (const id of f.edge) out.add(g.regions[id].name);
    return [...out];
  }
  // The thrust and its shoulders: enemy ground beside the road, and the enemy's front within two regions of it.
  const near = new Int8Array(g.regions.length).fill(-1);
  const q = thrust.map((r) => r.id);
  for (const id of q) near[id] = 0;
  for (let i = 0; i < q.length; i++) if (near[q[i]] < 2) for (const j of g.regions[q[i]].nb) if (near[j] < 0) { near[j] = near[q[i]] + 1; q.push(j); }
  for (const r of g.regions) {
    if (f.depth[r.id] < 1) continue;
    if (near[r.id] === 0 || near[r.id] === 1 || (near[r.id] === 2 && f.depth[r.id] === 1)) out.add(r.name);
  }
  // Elsewhere on the front: what is worth a battle, and what will fall to a push.
  for (const id of f.edge) {
    const r = g.regions[id];
    const held = r.units.reduce((s, u) => s + (atWar(g, nat, u.nat) ? u.divs * u.str : 0), 0);
    // A bulge in the line, with more of this army's ground around it than the enemy's, is a base for a stroke into
    // the flank of the advance. It is cleared: nothing is left standing behind the front.
    const round = r.nb.map((j) => g.regions[j]);
    const bulge = round.filter((x) => allied(g, nat, fieldCtrl(g, x))).length > round.filter((x) => atWar(g, nat, fieldCtrl(g, x))).length;
    if (r.cap || r.vp >= 2 || held <= OPEN.weak || bulge) out.add(r.name);
  }
  return [...out];
}

/** True if ground held by someone this nation is at war with lies within so many regions of a place. */
function nearEnemy(g: Game, nat: string, at: Region, within: number): boolean {
  const seen = new Set([at.id]);
  let ring = [at.id];
  for (let d = 0; d < within; d++) {
    const next: number[] = [];
    for (const id of ring) for (const j of g.regions[id].nb) if (!seen.has(j)) { seen.add(j); next.push(j); if (atWar(g, nat, fieldCtrl(g, g.regions[j]))) return true; }
    ring = next;
  }
  return false;
}

/** The friendly ground an offensive on this front starts from: garrisons there join in. Those facing strong works stay in their own. */
function jumpOff(g: Game, nat: string, f: Front): string[] {
  const cap = g.nations[nat].capital;
  return f.ours.filter((id) => id !== cap && g.regions[id].nb.some((j) => f.depth[j] === 1)).map((id) => g.regions[id].name);
}

// ---- What the staff remembers from month to month, kept with the game -----------------------------------

const flag = (g: Game, k: string): number => g.flags[k] ?? 0;
const onKey = (nat: string, e: string, where: string): string => `st:on:${nat}:${e}${where}`;
/**
 * The word of the far-side fronts against an enemy, kept under a key of its own (the design director's 11:40Z of 22
 * November 2026 to ruling 234). Each far-side front decides for itself and remembers on its own ground; the word by
 * country is kept for those who read by country (the air staff, the engineers, the player's view of a rival). Written
 * under the old chart's key it was also read by every old-chart front with that enemy among its own: a far-side front
 * opened against empty ground in East Africa turned on France's whole front at home. An old-chart front reads only
 * what its own decision, or a plan, wrote. Never written with the far side off.
 */
const farWordKey = (nat: string, e: string): string => `st:on:${nat}:${e}:far`;
/** The word by country as its readers hear it: the old chart's fronts' or a plan's, or else the far-side fronts'. */
const wordOn = (g: Game, nat: string, e: string): number => flag(g, onKey(nat, e, '')) || flag(g, farWordKey(nat, e));

/** The month this nation's offensive against the enemies on a front began, or 0 if there is none. */
/**
 * Whether this nation's staff is on the attack against `e` in a war it began itself, its offensive against `e` still
 * on (ruling 139, the design director's b519(m)). A war is its own when the last declaration between the two was its.
 */
export function onTheAttack(g: Game, nat: string, e: string): boolean {
  const mine = flag(g, `war:${nat}:${e}`);
  return mine > 0 && mine >= flag(g, `war:${e}:${nat}`) && wordOn(g, nat, e) > 0;
}

function onSince(g: Game, nat: string, f: Front, where = ''): number {
  if (!where && ownFront(g, f)) return frontSince(g, nat, f);
  let t = 0;
  for (const e of f.enemies) { const v = flag(g, onKey(nat, e, where)); if (v && (!t || v < t)) t = v; }
  return t;
}

/** (A far-side front never comes here: its month is `ownMonth`'s, and what it remembers is written on its ground.) */
function setOn(g: Game, nat: string, f: Front, turn: number, where = ''): void {
  for (const e of f.enemies) { if (turn) g.flags[onKey(nat, e, where)] = turn; else delete g.flags[onKey(nat, e, where)]; }
}

/**
 * True when the army on a front has had its rest since its last offensive there was called off. (A far-side front
 * rests ground by ground: it is read as resting while half its edge or more still is.)
 */
const rested = (g: Game, nat: string, f: Front, where = ''): boolean => (!where && ownFront(g, f)
  ? 2 * f.edge.filter((id) => g.turn < markAt(g, nat, MARK.rest, id)).length < f.edge.length
  : f.enemies.every((e) => g.turn >= flag(g, `st:rest:${nat}:${e}${where}`)));

// ---- The offensive held by front (rider b646 to ruling 234, and its rider of 19:25Z, 20 November 2026) -------
//
// On the old chart a staff remembers its offensive by enemy country: on against Poland, resting against France.
// Beyond the old chart one country's ground lies in many pieces, and a piece nobody stands on reads a strength
// ratio in the thousands: remembered by country, it opened the offensive against the whole country, and the main
// front's corps fought under it though their own front had never earned one. So on the far side each front opens,
// keeps and calls off its own offensive by its own reading, and rests by itself; the orders are bounded to the
// ground an offensive is on (build, below), so only that is attacked. No number is new.
//
// What is remembered is written on the ground, province by province, and a front is on where its ground is on
// and nowhere else (the rider of 19:25Z, item 2). So a front that has moved or split in two is still the same
// front; and when two pieces of enemy ground join into one front, each keeps what it had: the piece that was on
// is on, the piece that was called off rests its two months out, and a piece that was quiet opens only when the
// front's reading opens it. A front is kept, called off and opened on its one reading, piece by piece.
// A great choice or a march of the staff's own opens, for its months, the ground the enemies it names hold, and no
// other ground of a front they share with someone else (item 1).
// None of this is read with the far side's switch off, and no front with a province of its edge on the old chart
// or in the Americas reads it.

const MARK = { on: 'st:fo', rest: 'st:fr', idle: 'st:fi' } as const;
const markAt = (g: Game, nat: string, kind: string, id: number): number => g.flags[`${kind}:${nat}:${id}`] ?? 0;
function mark(g: Game, nat: string, kind: string, ids: Iterable<number>, value: number): void {
  for (const id of ids) { if (value) g.flags[`${kind}:${nat}:${id}`] = value; else delete g.flags[`${kind}:${nat}:${id}`]; }
}

/** True for a front that keeps its own offensive: the far side's switch on, and every province of its edge on the far side of the chart. */
export const ownFront = (g: Game, f: Pick<Front, 'edge'>): boolean => farOn(g) && f.edge.length > 0 && f.edge.every((id) => shoreOf(id) === 2);

const grounds = new WeakMap<Int16Array, number[]>();
/** The provinces of a front's ground. */
function groundOf(f: Pick<Front, 'depth'>): number[] {
  let out = grounds.get(f.depth);
  if (!out) { out = []; for (let id = 0; id < f.depth.length; id++) if (f.depth[id] >= 1) out.push(id); grounds.set(f.depth, out); }
  return out;
}

/** The month the offensive on this ground began: what most of it says (the commonest, then the oldest), or 0 where none is on. */
function sinceOf(g: Game, nat: string, ids: number[]): number {
  const tally = new Map<number, number>();
  for (const id of ids) { const v = markAt(g, nat, MARK.on, id); if (v) tally.set(v, (tally.get(v) ?? 0) + 1); }
  let best = 0, most = 0;
  for (const [v, k] of tally) if (k > most || (k === most && v < best)) { best = v; most = k; }
  return best;
}

/** The enemies met on some ground: who holds it in the field, and whose troops stand on it (as `fronts` names a front's). */
function foesOn(g: Game, nat: string, ids: number[]): string[] {
  const out = new Set<string>();
  for (const id of ids) { const r = g.regions[id]; out.add(fieldCtrl(g, r)); for (const u of r.units) if (!u.dead && atWar(g, nat, u.nat)) out.add(u.nat); }
  return [...out];
}

/**
 * The plan's word on a province of a far-side front: while a great choice or a march is being pressed (`st:push`),
 * the month its offensive against the enemies it named began, for ground one of them holds; 0 otherwise. The push
 * opens the stretch that faces the ground of the enemies it names and no more (the rider of 19:25Z, item 1): on a
 * front that joins Burma to China, the strike south opens Britain's provinces, and China's wait on the front's reading.
 */
function planSince(g: Game, nat: string, id: number): number {
  if (g.turn > flag(g, `st:push:${nat}`)) return 0;
  const e = fieldCtrl(g, g.regions[id]);
  return flag(g, `st:pushed:${nat}:${e}`) ? wordOn(g, nat, e) : 0;
}

/** The month a far-side front's offensive began, or 0: what its ground remembers first, then the plan's word. */
function frontSince(g: Game, nat: string, f: Front): number {
  const G = groundOf(f);
  const own = sinceOf(g, nat, G);
  if (own || g.turn > flag(g, `st:push:${nat}`)) return own;
  let t = 0;
  for (const id of G) { const v = planSince(g, nat, id); if (v && (!t || v < t)) t = v; }
  return t;
}

/**
 * Whose corps a front on the attack counts (the design director's 21:18Z of 21 November 2026, on the trace of the
 * book's stop in China in 1944): true for a corps that a far-side front of this staff's, its offensive on, counts
 * toward its strength by the one membership the staff's fronts are read by. A staff never weighs by one map and
 * fights by another: what a front on the attack counts is not somebody else's reserve (ai.ts `defend`). Null with
 * the far side off, and where no far-side front of the staff's has an offensive on with a target this month (`aimed`).
 * The fronts are read as they stand when it is asked: a front moves with the ground it takes, and its members with it.
 */
export function attackMembers(g: Game, nat: string, skip?: FrontOpts['skip']): ((c: Corps) => boolean) | null {
  if (!farOn(g)) return null;
  const own = fronts(g, nat, { skip }).filter((f) => ownFront(g, f));
  const M = membershipOf(own[0]);
  if (!M) return null;
  const live = own.filter((f) => onSince(g, nat, f) > 0);
  const on = live.filter((f) => aimed(g, nat, f, M));
  // (By the book a front between two of the record's places keeps what stands on it: `waits`.)
  const idle = stopDef(g).book ? live.filter((f) => !on.includes(f) && waits(g, nat, f)) : [];
  if (!on.length && !idle.length) return null;
  const stood = idle.map((f) => new Set(f.ours));
  return (c) => on.some((f) => M.counts(c, f)) || idle.some((f, k) => stood[k].has(c.reg) && M.counts(c, f));
}

/**
 * A front between offensives (the design director's 11:41Z of 22 November 2026, on Hengyang's fall in month 67 of
 * one war in six): by the book, a front whose offensive is on, that the record's line shuts for the month but still
 * owes a place, keeps what stands on its own ground. The record's 11th Army paused after Changsha before Hengyang;
 * with Changsha taken in month 58 and Hengyang's month 60, the front had no target for two months, kept nobody, and
 * the reserve rule took its rows north to Shasi and Linfen.
 *
 * Owed by its own operation: the place it waits for is given by a span whose kind is a dated choice of the book's
 * that has gone (Ichigo's places, from the month Ichigo goes). A span of no operation (a raid, a lasting gain: the
 * small front before Yencheng, shut from 1939 and owed Yencheng in 1941) is waited for by nobody, and such a front
 * keeps nobody, as ruled at 22:12Z of 21 November 2026.
 */
function waits(g: Game, nat: string, f: Front): boolean {
  const gone = ops(nat).filter((op) => flag(g, `st:done:${nat}:${op.id}`) > 0).map((op) => op.id);
  if (!gone.length) return false;
  const u = underOrders(g, nat, f, []);
  return !!u && u.edge.some((id) => lineOwes(g, nat, g.regions[id], gone));
}

/**
 * A front with a target this month (the design director's 22:12Z of 21 November 2026, on the count of the places the
 * reserve rule left unanswered). As laid, a front whose offensive was on kept its members whether or not it could
 * attack anything: by the book the one-province front before Yencheng was on from 1939 while the record's line shut
 * Yencheng until 1941, and the front of Macheng and Fowyang from 1944 with no span at all, and Hankow went twenty
 * months with a threat beside it and every reserve it had kept. A front with no target this month attacks nothing,
 * and keeps nobody.
 *
 * The bars are the ones the month's orders and the attack read, and no other: of the ground the front's orders would
 * name (`underOrders`: where its offensive is on, or the plan's word runs), a province on the enemy's edge that the
 * record's line leaves open this month (line.ts) and that a corps the front counts may enter from this side's ground
 * beside it (rivers.ts `blocked`: the bar every way between two provinces is asked). That bar is asked of a corps:
 * one of each country and arm among those the front counts answers for the rest.
 */
function aimed(g: Game, nat: string, f: Front, M: Membership): boolean {
  const u = underOrders(g, nat, f, []);
  if (!u) return false;
  const ours = new Set(f.ours);
  let reps: Corps[] | null = null;
  for (const id of u.edge) {
    const t = g.regions[id];
    if (lineShut(g, nat, t)) continue;
    if (!reps) {
      const seen = new Set<string>();
      reps = g.corps.filter((c) => !c.dead && M.counts(c, f) && !seen.has(`${c.nat}:${c.kind}`) && !!seen.add(`${c.nat}:${c.kind}`));
    }
    for (const j of t.nb) if (ours.has(j) && reps.some((c) => !blocked(g, c, t, j))) return true;
  }
  return false;
}

/** Told what a staff decides on each of its fronts as the month opens: for tests and readings. `part` is the number of provinces the word is about, on a far-side front. */
export const offensiveWatch: { tell: ((e: { nat: string; front: Front; own: boolean; was: number; now: number; how: 'opened' | 'eager' | 'pushed' | 'fresh' | 'kept' | 'off' | 'idle' | 'quiet'; marks: Temper; part?: number }) => void) | null } = { tell: null };

/**
 * The month on a far-side front, decided piece by piece on the front's one reading. Ground the plan's word covers
 * is on with the plan. Ground an offensive is on keeps it while the front reads at or over the mark to keep (new,
 * it is not asked), or is called off and rests, or is called off as an offensive in name only. Ground no offensive
 * is on, its rest over, opens one when the front reads the mark to open. Ground that is resting waits its rest out,
 * whatever it has joined.
 */
function ownMonth(g: Game, nat: string, f: Front, T: Temper, stalled: boolean, q: { planned: (f: Front) => boolean; waits: (f: Front) => boolean }): void {
  const G = groundOf(f);
  const was = frontSince(g, nat, f);
  let said = false;
  const tell = (how: 'opened' | 'eager' | 'pushed' | 'fresh' | 'kept' | 'off' | 'idle' | 'quiet', ids: number[]): void => { said = true; offensiveWatch.tell?.({ nat, front: f, own: true, was, now: frontSince(g, nat, f), how, marks: T, part: ids.length }); };
  const lord = (id: number): string => fieldCtrl(g, g.regions[id]);
  // The plan's word.
  const plan = new Set<number>();
  for (const id of G) {
    const v = planSince(g, nat, id);
    if (!v) continue;
    plan.add(id);
    if (!markAt(g, nat, MARK.on, id)) { g.flags[`${MARK.on}:${nat}:${id}`] = v; delete g.flags[`${MARK.rest}:${nat}:${id}`]; }
  }
  if (plan.size) tell('pushed', [...plan]);
  // The ground an offensive of the front's own is on: each offensive (the ground that went on in one month) by itself.
  const begun = new Map<number, number[]>();
  for (const id of G) {
    const v = plan.has(id) ? 0 : markAt(g, nat, MARK.on, id);
    if (v) { const at = begun.get(v); if (at) at.push(id); else begun.set(v, [id]); }
  }
  const off = new Set<number>();
  for (const [since, own] of [...begun].sort((x, y) => x[0] - y[0])) {
    const piece: Front = { ...f, enemies: foesOn(g, nat, own) };
    // An offensive is not asked what it has gained in its first two months. (On the old chart one opened out of
    // impatience is given its whole season, and so is every other front of the country while that season lasts: on
    // the far side an impatient season may open a front and never keeps one, the rider of 19:25Z, item 3.)
    const fresh = g.turn - since < 2;
    if (!fresh && inNameOnly(g, nat, piece, since)) {
      mark(g, nat, MARK.on, own, 0); mark(g, nat, MARK.idle, own, g.turn);
      for (const id of own) off.add(id);
      tell('idle', own);
    } else if (fresh || (f.ratio >= T.persist && (!stalled || f.ratio >= T.attackAt))) {
      // (Ground lost to the same enemy since the offensive began is fought over under it.)
      const lords = new Set(own.map(lord));
      for (const id of G) {
        if (plan.has(id) || off.has(id) || markAt(g, nat, MARK.on, id) || g.turn < markAt(g, nat, MARK.rest, id)) continue;
        const r = g.regions[id];
        if (r.since >= since && !!r.prev && allied(g, nat, r.prev) && lords.has(lord(id))) g.flags[`${MARK.on}:${nat}:${id}`] = since;
      }
      tell(fresh ? 'fresh' : 'kept', own);
    } else {
      mark(g, nat, MARK.on, own, 0); mark(g, nat, MARK.rest, own, g.turn + OPEN.rest);
      for (const id of own) off.add(id);
      tell('off', own);
    }
  }
  // The ground no offensive is on, its rest over.
  const idle = G.filter((id) => !off.has(id) && !markAt(g, nat, MARK.on, id) && g.turn >= markAt(g, nat, MARK.rest, id));
  if (idle.length) {
    const piece: Front = { ...f, enemies: foesOn(g, nat, idle) };
    const usual = T.attackAt * inConcert(g, nat, piece);
    // The longer the front has been quiet, the smaller the advantage a staff waits for: a nation with no
    // offensive of its own in it finds one at last, and nobody attacks at hopeless odds.
    const I = OPEN.impatience;
    const k0 = boldness(g, nat, piece);
    const k = k0 < (I?.after.length ?? 0) && q.planned(piece) ? 0 : k0;
    const none = T.attackAt >= 50;
    const eager = !k ? usual : none ? (k >= I.after.length ? I.any : usual) : Math.max(I.floor, usual * I.ratio[k - 1]);
    // (Ground where the last offensive was one in name only waits for more strength, or for patience to run out.)
    const timid = !k && rivalRules(g) && f.ratio < RIVALS.press.ratio;
    const open = timid ? idle.filter((id) => { const at = markAt(g, nat, MARK.idle, id); return !(at > 0 && g.turn - at < RIVALS.press.again); }) : idle;
    if (open.length && f.ratio >= eager && !q.waits(piece)) {
      mark(g, nat, MARK.on, open, g.turn);
      g.flags[`st:gain:${nat}`] = g.turn;
      if (f.ratio < usual) {
        g.flags[`st:bold:${nat}`] = g.turn + I.effort - 1;
        g.flags[`st:boldK:${nat}`] = k;
      }
      tell(f.ratio < usual ? 'eager' : 'opened', open);
    }
  }
  if (!said) tell('quiet', []);
}

/**
 * A far-side front as the orders see it: its ground where an offensive is on, or the plan's word runs, or that an
 * enemy war is being declared on this month holds, and nothing else. The front itself where that is all of it; null
 * where it is none. The reading stays the whole front's.
 */
function underOrders(g: Game, nat: string, f: Front, declare: string[]): Front | null {
  if (!ownFront(g, f)) return f;
  const G = groundOf(f);
  const ids = G.filter((id) => markAt(g, nat, MARK.on, id) > 0 || planSince(g, nat, id) > 0 || (declare.length > 0 && declare.includes(fieldCtrl(g, g.regions[id]))));
  if (ids.length === G.length) return f;
  const depth = new Int16Array(f.depth.length).fill(-1);
  for (const id of ids) depth[id] = f.depth[id];
  const edge = f.edge.filter((id) => depth[id] >= 1);
  // (Ground under an offensive that touches the line nowhere is given no orders.)
  if (!edge.length) return null;
  const enemies = foesOn(g, nat, ids).filter((e) => f.enemies.includes(e));
  return { ...f, key: enemies.includes(f.key) ? f.key : enemies[0] ?? f.key, enemies, edge, depth };
}

/**
 * With the month's fronts decided: the by-country word of an offensive is kept true for those who read it (the
 * air staff, the engineers, the player's view of a rival), on against an enemy while far-side ground of his is
 * under an offensive; and what was remembered of ground that is on no far-side front now is forgotten.
 */
function frontsKept(g: Game, nat: string, real: Front[]): void {
  if (!farOn(g)) return;
  const on = new Map<string, number>(), seen = new Set<string>(), elsewhere = new Set<string>();
  const ground = new Uint8Array(g.regions.length);
  for (const f of real) {
    if (!ownFront(g, f)) { for (const e of f.enemies) elsewhere.add(e); continue; }
    for (const e of f.enemies) seen.add(e);
    const G = groundOf(f);
    for (const id of G) ground[id] = 1;
    const under = G.filter((id) => markAt(g, nat, MARK.on, id) > 0);
    const since = sinceOf(g, nat, under);
    for (const e of foesOn(g, nat, under)) if (!on.get(e) || since < on.get(e)!) on.set(e, since);
  }
  for (const e of seen) {
    const k = onKey(nat, e, ''), far = farWordKey(nat, e);
    // (The far-side fronts' word goes under its own key: an old-chart front against the same enemy reads only its own
    // decision. An enemy met on the old chart too: the old chart's word is its fronts' to give and to take away. And
    // the word a plan gave against an enemy it named stands for the plan's months, whether or not a front has reached
    // his ground yet; with its months over and none of his far-side ground under an offensive it is forgotten.)
    if (on.has(e)) { if (!flag(g, far)) g.flags[far] = on.get(e)!; }
    else {
      delete g.flags[far];
      if (!elsewhere.has(e) && !(g.turn <= flag(g, `st:push:${nat}`) && flag(g, `st:pushed:${nat}:${e}`))) delete g.flags[k];
    }
  }
  // (And the far-side word against an enemy who stands on no far-side front of this nation's now is forgotten. One walk
  // over the flags for this and the marks below: the two kinds of key share no key, and neither reads the other's.)
  const word = `st:on:${nat}:`;
  const mine = [`${MARK.on}:${nat}:`, `${MARK.rest}:${nat}:`, `${MARK.idle}:${nat}:`];
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('st:')) continue;
    if (k.startsWith(word)) {
      if (k.endsWith(':far') && !seen.has(k.slice(word.length, -4))) delete g.flags[k];
      continue;
    }
    const pre = mine.find((m) => k.startsWith(m));
    if (!pre) continue;
    // (A rest lasts its months whatever becomes of the front, and is forgotten when it is over.)
    if (pre === mine[1] ? g.flags[k] <= g.turn : !ground[Number(k.slice(pre.length))] || (pre === mine[2] && g.turn - g.flags[k] >= (RIVALS.press?.again ?? 99))) delete g.flags[k];
  }
}

/** Months since this nation and an enemy last fought on land, either way round, or since their war began. */
export function quietWith(g: Game, nat: string, e: string): number {
  const last = Math.max(g.nations[nat]?.lastFight[e] ?? 0, g.nations[e]?.lastFight[nat] ?? 0, flag(g, `war:${nat}:${e}`), flag(g, `war:${e}:${nat}`), 1);
  return g.turn - last;
}

/**
 * Months since the two last fought in the open field, either way round (the grand campaign): a town stormed behind
 * the line is not a battle on the front, and does not wake a staff that is waiting for the front to go quiet. A
 * game saved before the field was counted apart reads the older clock.
 */
export function quietInField(g: Game, nat: string, e: string): number {
  const field = Math.max(flag(g, `ff:${nat}:${e}`), flag(g, `ff:${e}:${nat}`));
  if (!field) return quietWith(g, nat, e);
  return g.turn - Math.max(field, flag(g, `war:${nat}:${e}`), flag(g, `war:${e}:${nat}`), 1);
}

/**
 * How far a staff's patience has run out on a front: 0 while there has been fighting on it lately, then a step for
 * each term of quiet in open.json that has gone by. A war nobody is fighting makes bolder generals.
 */
export function boldness(g: Game, nat: string, f: Pick<Front, 'enemies'>): number {
  const I = OPEN.impatience;
  if (!I || !f.enemies.length) return 0;
  const quiet = Math.min(...f.enemies.map((e) => quietWith(g, nat, e)));
  return I.after.filter((m) => quiet >= m).length;
}

/** The step of boldness a nation's present offensive was opened at, while its main effort lasts. 0 otherwise. */
export const boldNow = (g: Game, nat: string): number => (g.turn <= flag(g, `st:bold:${nat}`) ? flag(g, `st:boldK:${nat}`) : 0);

/**
 * The local attacks a staff standing on the defensive will make this month: against which enemies, at what odds or
 * better, and how many. A quiet front is not a truce. Once the first term of quiet has gone by it takes a weak
 * point where it sees one, and it keeps at it for as long as the last such attack is fresh. Null when it makes none.
 */
export function probing(g: Game, nat: string): { enemies: Set<string>; odds: number; most: number; step: number } | null {
  const I = OPEN.impatience;
  const n = g.nations[nat];
  if (!isOpen(g) || !I?.local || !n?.alive || n.dormant || nat === g.human) return null;
  const enemies = new Set<string>();
  let k = 0;
  for (const e of n.atWar) {
    if (!g.nations[e]?.alive) continue;
    const quiet = quietWith(g, nat, e);
    const at = flag(g, `st:probe:${nat}:${e}`);
    const step = at && g.turn - at <= I.effort ? flag(g, `st:probeK:${nat}:${e}`) : I.after.filter((m) => quiet >= m).length;
    if (step) { enemies.add(e); k = Math.max(k, step); }
  }
  return k ? { enemies, odds: I.local[Math.min(k, I.local.length) - 1], most: I.locals, step: k } : null;
}

/**
 * Told to the player once a year while his war is silent: nobody has fought, and what that will bring. A war does
 * not end by going quiet, and the player should know the other side's patience is running out as well.
 */
export function quietNews(g: Game): void {
  const me = g.human;
  if (!isOpen(g) || me === null) return;
  const n = g.nations[me];
  if (!n?.alive) return;
  const foes = n.atWar.filter((e) => g.nations[e]?.alive && !g.nations[e].dormant);
  if (!foes.length) return;
  const quiet = Math.min(...foes.map((e) => quietWith(g, me, e)));
  if (quiet < 12 || g.turn - flag(g, 'st:quietTold') < 12) return;
  g.flags['st:quietTold'] = g.turn;
  const span = quiet >= 24 ? `${Math.floor(quiet / 12)} years` : 'a year';
  const next = foes.some((e) => inTouch(g, me, e) || inTouch(g, e, me))
    ? 'A quiet front makes bolder generals: your enemies will now attack on a smaller advantage than they waited for before, and strike at any weak point in your line.'
    : 'With no front on land between you, the war will be carried over the sea: watch your coasts, or put an army ashore on theirs.';
  headline(g, 'info', `No battle has been fought between you and your enemies for ${span}. ${next}`, me);
}

/** Notes a local attack made on an enemy, so that the staff keeps at it while it is fresh. */
export function probed(g: Game, nat: string, e: string, step: number): void {
  g.flags[`st:probe:${nat}:${e}`] = g.turn;
  g.flags[`st:probeK:${nat}:${e}`] = step;
}

/**
 * On the hardest level the player's rivals time their blows together: while an ally of this nation has an offensive
 * under way against the player, it opens its own against him at a smaller ratio than it would alone. 1 otherwise.
 */
export function inConcert(g: Game, nat: string, f: Pick<Front, 'enemies'>): number {
  const k = concertFor(g, nat);
  const you = seat(g);
  if (k === 1 || you === null || !f.enemies.includes(you)) return 1;
  for (const a of Object.keys(g.nations)) if (a !== nat && g.nations[a].alive && allied(g, a, nat) && wordOn(g, a, you)) return k;
  return 1;
}

// ---- The main effort (the grand campaign) ---------------------------------------------------------------
//
// A staff that stands on the defensive on a front is not asleep on it. When the front has been quiet long enough
// for its character, the balance of strength is not hopeless, and somewhere on the line the force it could mass
// in a month would go in at good odds, it names a main effort: one enemy province, attacked for a season with
// everything that can be brought opposite it and nothing anywhere else. The army's orders for those months are an
// ordinary attack phase bounded to that sector; the bombers and a probing corps soften the line first (air.ts, ai.ts).

/** Odds no attack reaches: the orders of a month spent massing. */
const HOLD_ODDS = 99;

const SEASONS = ['winter', 'winter', 'spring', 'spring', 'spring', 'summer', 'summer', 'summer', 'autumn', 'autumn', 'autumn', 'winter'];
const effKey = (k: string, nat: string, where: string): string => `st:${k}:${nat}${where}`;

/** A staff's main effort: the province it is aimed at now, the one it was named for, the months it runs, and whom it is against. */
export interface Effort { nat: string; target: number; first: number; from: number; to: number; enemy: string; name: string; won: number; /** The odds it strikes at, where patience has shortened them. */ odds?: number }

/** The main effort a nation's staff has under way, on its main fronts or in a theatre overseas ('where'), or null. */
export function effortOf(g: Game, nat: string, where = ''): Effort | null {
  const t = flag(g, effKey('eff', nat, where));
  if (!t || !rivalRules(g) || !g.regions[t - 1]) return null;
  const first = flag(g, effKey('eff0', nat, where)) || t, from = flag(g, effKey('effAt', nat, where));
  const enemy = Object.keys(g.nations)[flag(g, effKey('effE', nat, where)) - 1] ?? g.regions[t - 1].ctrl;
  return {
    nat, target: t - 1, first: first - 1, from, to: flag(g, effKey('effTo', nat, where)), enemy, won: flag(g, effKey('effWon', nat, where)),
    ...(flag(g, effKey('effOdds', nat, where)) ? { odds: flag(g, effKey('effOdds', nat, where)) / 100 } : {}),
    name: `the ${SEASONS[turnToDate(from).month]} offensive towards ${g.regions[first - 1].name}`,
  };
}

/** Every main effort a nation has under way: on its main fronts and in each theatre overseas. */
export function effortsOf(g: Game, nat: string): Effort[] {
  if (!rivalRules(g)) return [];
  const base = `st:eff:${nat}`;
  const out: Effort[] = [];
  for (const k of Object.keys(g.flags)) {
    if (k !== base && !k.startsWith(`${base}:t`)) continue;
    const e = effortOf(g, nat, k.slice(base.length));
    if (e) out.push(e);
  }
  return out;
}

/** True for an enemy who has his hands full: he has fought someone else on land within the last months. */
const busyElsewhere = (g: Game, e: string, nat: string): boolean => Object.keys(g.nations).some((x) =>
  x !== nat && x !== e && Math.max(g.nations[e]?.lastFight[x] ?? -99, g.nations[x].lastFight[e] ?? -99) >= g.turn - RIVALS.effort.busy);

/**
 * The best place for a main effort on a front: the enemy province where the force that could be massed in a month
 * (the field corps beside it, and those a province further back as far as there is room to bring them up) has the
 * best prospect, worth counted. Only places where that force would go in at 'least' odds or better; 'beside' keeps
 * the choice next to a province just taken, when an effort is carried on.
 */
function sector(g: Game, nat: string, f: Front, least: number, beside?: Region): { target: Region; odds: number } | null {
  const ours = new Set(f.ours);
  const prize = objectives(g, f).slice(0, 8);
  const fit = (c: Corps): boolean => c.nat === nat && !c.dead && c.kind !== 'fort' && !c.hold && c.town === undefined && c.leave === undefined && c.org >= FIT_ORG;
  let best: { target: Region; odds: number; score: number } | null = null;
  for (const id of f.edge) {
    const t = g.regions[id];
    // Behind strong works there is no way in; a place nobody holds is walked into, not attacked.
    if (f.depth[id] !== 1 || (beside && !beside.nb.includes(id)) || !atWar(g, nat, fieldCtrl(g, t)) || !hostileUnits(g, t, nat).length) continue;
    // (By the book, no main effort goes for ground beyond the record's line: line.ts.)
    if (lineShut(g, nat, t)) continue;
    const doors = t.nb.filter((j) => (ours.has(j) || (beside && j === beside.id)) && allied(g, nat, fieldCtrl(g, g.regions[j])));
    if (!doors.length) continue;
    const front: Corps[] = [], back: Corps[] = [];
    const seen = new Set(doors);
    let room = 0;
    for (const j of doors) {
      const r = g.regions[j];
      for (const c of r.units) if (fit(c)) front.push(c);
      room += Math.max(0, STACK - r.units.filter((u) => allied(g, nat, u.nat) && !roomless(g, u)).length);
    }
    for (const j of doors) for (const k of g.regions[j].nb) {
      if (seen.has(k)) continue;
      seen.add(k);
      const r = g.regions[k];
      if (allied(g, nat, fieldCtrl(g, r))) for (const c of r.units) if (fit(c)) back.push(c);
    }
    back.sort((a, b) => atkP(g, b) - atkP(g, a) || a.id - b.id);
    const force = limitAttackers(g, [...front, ...back.slice(0, room)], t);
    if (!force.length) continue;
    const odds = assess(g, force, t).odds;
    if (odds < least) continue;
    const score = Math.min(odds, 3) + t.vp * 2 + (t.cap ? 8 : 0) + (prize.includes(t.name) ? 2 : 0);
    if (!best || score > best.score || (score === best.score && t.id < best.target.id)) best = { target: t, odds, score };
  }
  return best;
}

/**
 * How a viewer comes to know a staff's plans: an ally tells him ('ally'), or an agent of his is reading its books in
 * its capital ('agent'). Null where he does not know them. With no player at the table everything is known.
 */
export function plansKnown(g: Game, nat: string, viewer: string | null = g.human): 'ally' | 'agent' | null {
  if (viewer === null || !g.nations[viewer]?.alive || allied(g, nat, viewer)) return 'ally';
  return knowsBooks(g, nat, viewer) ? 'agent' : null;
}

/** What the player is told when a staff whose plans he knows names its main effort: an ally's, or an enemy's whose books his agent reads. */
function effortNamed(g: Game, e: Effort): void {
  const me = g.human;
  if (me === null || !g.nations[me]?.alive || e.nat === me) return;
  const how = plansKnown(g, e.nat, me);
  if (!how) return;
  const who = g.nations[e.nat]?.name ?? e.nat;
  const until = turnToDate(e.to).label;
  const name = e.name.replace(/^the /, '');
  if (how === 'ally') {
    if (atWar(g, me, e.enemy) || allied(g, me, e.enemy)) headline(g, 'info', `${who}'s staff has named its main effort: ${e.name}, against ${g.nations[e.enemy]?.name ?? e.enemy}, until ${until}.`, e.nat, e.first);
    return;
  }
  const cap = g.nations[e.nat].capital;
  const agent = agentsOf(g, me).find((a) => a.reg === cap && a.mission === 'books' && atWork(g, a));
  const mine = e.enemy === me || allied(g, me, e.enemy);
  headline(g, mine ? 'event' : 'info', `${agent ? `Agent ${agent.name} reports from ${g.regions[cap].name}` : 'Your secret service reports'}: ${who}'s staff has named its main effort, the ${name}. It will mass opposite ${g.regions[e.first].name} and attack there until ${until}${mine ? '. You have the month it takes them to mass.' : '.'}`, e.nat, e.first);
}

/** What the player is told when a main effort that concerned him is over: one against him or his allies, or an ally's. */
function effortOver(g: Game, e: Effort): void {
  const me = g.human;
  if (me === null || !g.nations[me]?.alive) return;
  const who = g.nations[e.nat]?.name ?? e.nat;
  const got = e.won === 1 ? 'one province' : `${e.won} provinces`;
  const name = e.name.replace(/^the /, '');
  if (atWar(g, me, e.nat) && (e.enemy === me || allied(g, me, e.enemy))) {
    headline(g, 'event', `${who}'s ${name} is over. ${e.won ? `It took ${got}, and the staff that planned it will be looking for the next place.` : 'The line held, and the staff that planned it will be looking for another place.'}`, e.nat, e.first, { arm: 'land', ...(e.won ? { won: e.nat, lost: e.enemy } : { won: e.enemy, lost: e.nat }) });
  } else if (allied(g, me, e.nat)) {
    headline(g, 'info', `${who}'s ${name} is over: ${e.won ? `it took ${got}` : 'it gained no ground'}.`, e.nat, e.first, { arm: 'land', ...(e.won ? { won: e.nat, lost: e.enemy } : { won: e.enemy, lost: e.nat }) });
  }
}

/**
 * The month's decision on a main effort, taken with the staff's others: one under way is carried on, moved to the
 * next province when its target has fallen, or ended when its months are up; and with none under way one is named
 * where a front calls for it. 'spoken' is true for a front the staff has a greater plan against already.
 */
function mainEffort(g: Game, nat: string, real: Front[], T: Temper, where: string, spoken: (f: Front) => boolean): void {
  if (!rivalRules(g) || nat === g.human) return;
  const E = RIVALS.effort, who = characterOf(g, nat), C = RIVALS.characters[who];
  const key = (k: string): string => effKey(k, nat, where);
  const stop = (rest: number): void => {
    for (const k of ['eff', 'eff0', 'effAt', 'effTo', 'effE', 'effWon', 'effOdds']) delete g.flags[key(k)];
    if (rest) g.flags[key('effRest')] = g.turn + rest;
  };
  const cur = effortOf(g, nat, where);
  if (cur) {
    const at = g.regions[cur.target];
    const f = real.find((x) => x.enemies.includes(cur.enemy)) ?? null;
    const taken = allied(g, nat, fieldCtrl(g, at));
    if (taken) { g.flags[key('effWon')] = cur.won + 1; cur.won++; }
    const gone = !f || !g.nations[cur.enemy]?.alive || !atWar(g, nat, cur.enemy);
    // A general offensive opened on the same front takes the effort over.
    // (On the far side that is the front the effort's own target stands on: another front's offensive is not this one's.)
    const same = (!where && farOn(g) ? real.find((x) => ownFront(g, x) && x.depth[cur.target] >= 1) : undefined) ?? f;
    const general = !!same && (!where && ownFront(g, same) && same.depth[cur.target] >= 1 ? markAt(g, nat, MARK.on, cur.target) > 0 || planSince(g, nat, cur.target) > 0 : onSince(g, nat, same, where) > 0);
    const live = !gone && !general && g.turn <= cur.to;
    // The place is taken: the effort is carried on to the next one beside it while its months last.
    const next = live && taken ? sector(g, nat, f!, T.minOdds, at) : null;
    if (next) { g.flags[key('eff')] = next.target.id + 1; return; }
    if (live && !taken) return;
    if (!general) effortOver(g, cur);
    stop(gone || general ? 0 : E.rest);
    return;
  }
  if (g.turn < flag(g, key('effRest')) || flag(g, `st:op:${nat}`)) return;
  for (const f of real) {
    if (onSince(g, nat, f, where) || spoken(f)) continue;
    // An offensive just called off: the army rests before anything new is asked of it.
    if (!rested(g, nat, f, where)) continue;
    // The strength is weighed against the enemy on the line and close behind it, and the quiet is the quiet of the field.
    const quiet = Math.min(...f.enemies.map((e) => quietInField(g, nat, e)));
    const waited = quiet >= C.quiet;
    if (!waited && !(who === 'opportunist' && f.enemies.some((e) => busyElsewhere(g, e, nat)))) continue;
    // Patience runs out: for every month the front has lain quiet beyond the staff's own wait it asks a step less
    // of its strength and of the odds at the place it would strike, down to a floor. No front sleeps for ever.
    const P = E.patience, over = P && waited ? quiet - C.quiet : 0;
    const strength = P ? Math.max(Math.min(P.strength, C.strength), C.strength - P.step * over) : C.strength;
    const local = P ? Math.max(Math.min(P.odds, C.local), C.local - P.step * over) : C.local;
    if (f.near < strength) continue;
    const s = sector(g, nat, f, local);
    if (!s) continue;
    g.flags[key('eff')] = g.flags[key('eff0')] = s.target.id + 1;
    g.flags[key('effAt')] = g.turn;
    // The month it is named the army masses and the line is softened; the blows fall in the months after.
    g.flags[key('effTo')] = g.turn + E.months;
    g.flags[key('effE')] = Object.keys(g.nations).indexOf(fieldCtrl(g, s.target)) + 1;
    delete g.flags[key('effWon')];
    // An effort named on shortened odds strikes at them: a staff out of patience does not mass and then sit.
    if (local < C.local) g.flags[key('effOdds')] = Math.round(local * 100); else delete g.flags[key('effOdds')];
    const named = effortOf(g, nat, where);
    if (named) effortNamed(g, named);
    return;
  }
}

/**
 * The orders a main effort gives the army: attack the target and the enemy provinces beside it that touch the same
 * jump-off ground, mass in the provinces next to the target, and let the garrisons there join. Null if the effort
 * has no front left to be fought on.
 */
function effortOrders(g: Game, nat: string, e: Effort, real: Front[], T: Temper): Partial<PlanPhase> | null {
  const E = RIVALS.effort;
  const at = g.regions[e.target];
  const f = real.find((x) => x.depth[at.id] >= 1) ?? real.find((x) => x.enemies.includes(e.enemy));
  if (!f) return null;
  const doors = at.nb.filter((j) => allied(g, nat, fieldCtrl(g, g.regions[j])));
  // The sector: the target, then the enemy provinces beside it that the same jump-off ground touches.
  const around = at.nb
    .filter((j) => f.depth[j] >= 1 && atWar(g, nat, fieldCtrl(g, g.regions[j])) && g.regions[j].nb.some((k) => doors.includes(k)))
    .sort((a, b) => f.depth[a] - f.depth[b] || g.regions[b].vp - g.regions[a].vp || a - b)
    .slice(0, Math.max(0, E.sector - 1));
  const staging: Record<string, number> = {};
  for (const j of doors) if (j !== g.nations[nat].capital) staging[g.regions[j].name] = Math.min(STACK, E.mass);
  return {
    posture: 'attack', name: e.name.replace(/^t/, 'T'), from: e.from, effort: at.id,
    enemies: f.enemies, objectives: [at.name, ...around.map((j) => g.regions[j].name)],
    targets: [at.name, ...around.map((j) => g.regions[j].name)],
    // No attack goes in the month the effort is named: the word of it runs ahead of the blow.
    minOdds: g.turn === e.from ? HOLD_ODDS : e.odds ? Math.min(T.minOdds, e.odds) : T.minOdds, ...(g.turn === e.from ? { massing: true } : {}), staging, release: doors.filter((j) => j !== g.nations[nat].capital).map((j) => g.regions[j].name),
  };
}

/**
 * The will to finish (the grand campaign): the odds a staff asks battle by battle on a front where its general
 * offensive has the numbers and has gained no ground for months, and how many waves it will send at one place.
 * Null where it fights by its usual rules.
 */
export function pressing(g: Game, nat: string, f: Pick<Front, 'ratio'>, minOdds: number, where = ''): { minOdds: number; waves: number } | null {
  const P = RIVALS.press;
  if (!rivalRules(g) || !P || f.ratio < P.ratio) return null;
  const idle = g.turn - flag(g, `st:gain:${nat}${where}`);
  if (idle < P.after) return null;
  return { minOdds: Math.max(Math.min(P.floor, minOdds), minOdds - P.step * (idle - P.after + 1)), waves: P.waves };
}

/**
 * An offensive in name only (the grand campaign): one that has been on for months without the strength to press it
 * home, and on which nobody has fought a battle either way. The staff calls it off and looks for a main effort.
 */
function inNameOnly(g: Game, nat: string, f: Front, since: number): boolean {
  const P = RIVALS.press;
  return rivalRules(g) && nat !== g.human && g.turn - since >= P.after && f.ratio < P.ratio && Math.min(...f.enemies.map((e) => quietWith(g, nat, e))) >= P.after;
}
/** Called off as an offensive in name only: the front is marked, so that the next general offensive there waits for more strength. */
function calledOffIdle(g: Game, nat: string, f: Front, where = ''): void {
  setOn(g, nat, f, 0, where);
  for (const e of f.enemies) g.flags[`st:idle:${nat}:${e}${where}`] = g.turn;
}
/** True while a front's last offensive in name only is fresh in the staff's mind and it has not the strength to do better. */
const shy = (g: Game, nat: string, f: Front, where = ''): boolean => rivalRules(g) && f.ratio < RIVALS.press.ratio
  && f.enemies.map((e) => flag(g, `st:idle:${nat}:${e}${where}`)).some((at) => at > 0 && g.turn - at < RIVALS.press.again);

/** One of the staff's own conditions, or one that reads as in diplomacy.json. */
function cond(g: Game, nat: string, c: string, real: Front[]): boolean {
  // (One of several will do where they are written with a bar between them.)
  if (c.includes('|')) return c.split('|').some((x) => cond(g, nat, x, real));
  const [key, rest = ''] = c.split(/:(.*)/s);
  const a = rest.split(',');
  if (key === 'quiet') {
    // No serious land front: nothing the enemy could do there that the troops standing on it could not answer.
    const total = g.corps.reduce((s, x) => s + (x.nat === nat && !x.dead && x.kind !== 'fort' ? atkP(g, x) : 0), 0);
    return real.every((f) => f.def < OPEN.major * total);
  }
  if (key === 'collapsing') { const n = g.nations[a[0]]; return !!n && (n.alive ? collapseScore(g, a[0]) >= Number(a[1]) : !!n.fellOn); }
  if (key === 'month') return a.map(Number).includes(turnToDate(g.turn).month + 1);
  return holds(g, c);
}

/**
 * A great choice as it goes in this war. Where its enemies are those that stand in the way rather than history's list
 * (pacific.ts inTheWay: the strike south at stops 3 to 5), it declares on them, and its strikes and landings are cut
 * to ground held by countries it is at war with or declares on (`declaring`, this month's declarations; left out, all
 * those in the way). Every other choice, and this one at stops 1 and 2, goes exactly as written.
 */
export function asGoes(g: Game, nat: string, op: Op, declaring?: string[]): Op {
  const way = inTheWay(g, nat, op);
  if (!way) return op;
  const now = declaring ?? way;
  const foe = (nm: string): boolean => { const r = g.byName[nm]; return !!r && (atWar(g, nat, r.ctrl) || now.includes(r.ctrl)); };
  const strikes = op.strikes?.filter((s) => foe(s.harbour)), landings = op.landings?.filter((L) => L.to.some(foe));
  return { ...op, declare: way, strikes: strikes?.length ? strikes : undefined, landings: landings?.length ? landings : undefined };
}

/** Whom an operation would still add to this nation's enemies: the living, not on its own side, not yet at war with it. */
const targets = (g: Game, nat: string, op: Op): string[] => (asGoes(g, nat, op).declare ?? []).filter((e) => g.nations[e]?.alive && !g.nations[e].dormant && !allied(g, nat, e) && !atWar(g, nat, e));

/**
 * A great offensive against an enemy the nation is at war with already (it declares on nobody and is no side-show):
 * the enemies it names in its own conditions ('war:ME,HIM') who are still fighting. Empty for every other choice.
 */
const inWar = (g: Game, nat: string, op: Op): string[] => (op.side || (op.declare ?? []).length ? [] : (op.if ?? [])
  .filter((c) => c.startsWith(`war:${nat},`)).map((c) => c.slice(nat.length + 5)).filter((e) => g.nations[e]?.alive && atWar(g, nat, e)));

/**
 * The front a new war would open, weighed before it is declared; for a great offensive in a war already on, its front as it stands.
 * (A great choice weighs what it could gather: the corps on garrison in taken ground are counted, rider C to ruling 234.
 * On a far-side front it weighs the front's members and what the pool spares of the corps that are not its members:
 * the design director's 19:52Z of 21 November 2026, in fronts().)
 */
export function prospect(g: Game, nat: string, op: Op, skip: FrontOpts['skip']): Front | null {
  const extra = new Set(targets(g, nat, op));
  if (!extra.size) {
    const foes = inWar(g, nat, op);
    return foes.length ? fronts(g, nat, { skip, busy: (f) => onSince(g, nat, f) > 0, gather: true }).find((f) => f.enemies.some((e) => foes.includes(e))) ?? null : null;
  }
  return fronts(g, nat, { extra, skip, busy: (f) => onSince(g, nat, f) > 0, gather: true }).find((f) => f.enemies.some((e) => extra.has(e))) ?? null;
}

const inSeason = (g: Game, op: Op, turn = g.turn): boolean => !op.season || op.season.includes(turnToDate(turn).month + 1);

export interface Decision {
  /** 0 stand on the defensive, 1 attack, 2 bring the army up for a war not yet declared. */
  posture: 0 | 1 | 2;
  /** Wars declared this month, and the operation they belong to. */
  declare: string[];
  op: Op | null;
}

/** The great choices open to a nation's staff. */
export const ops = (nat: string): Op[] => OPEN.ops[nat] ?? [];

/**
 * Opens an offensive against the given enemies and presses it for at least `push` months: what a staff does when
 * it marches of its own accord, on no plan of history's (a neutral with an ambition, the history dial).
 */
export function launch(g: Game, nat: string, foes: string[], push: number): void {
  g.flags[`st:push:${nat}`] = g.turn + push - 1;
  g.flags[`st:gain:${nat}`] = g.turn;
  for (const e of foes) { g.flags[onKey(nat, e, '')] = g.turn; g.flags[`st:pushed:${nat}:${e}`] = 1; }
}

/**
 * The month's decisions for one computer-run nation, taken once before its army moves: which offensives go on,
 * which stop, whether a new one opens, and whether one of history's great choices is taken up or carried out.
 */
export function decide(g: Game, nat: string, skip?: FrontOpts['skip']): void {
  const n = g.nations[nat];
  if (!n.alive || n.dormant) return;
  memo.get(g)?.delete(nat);
  const T = temper(g, nat);
  const real = fronts(g, nat, { skip, busy: (f) => onSince(g, nat, f) > 0 });
  // Ground gained since last month keeps an offensive alive; three months without any and it is called off.
  const held = g.regions.reduce((s, r) => s + (r.ctrl === nat ? 1 : 0), 0);
  if (held > flag(g, `st:n:${nat}`)) g.flags[`st:gain:${nat}`] = g.turn;
  g.flags[`st:n:${nat}`] = held;
  const stalled = g.turn - flag(g, `st:gain:${nat}`) >= OPEN.stall;
  const pushed = g.turn <= flag(g, `st:push:${nat}`);
  const list = ops(nat);
  // A great choice still open against an enemy this nation is already at war with: the way its staff means to
  // deal with him. While it stands, a quiet front does not tempt the staff into a head-on attack: it hurries the
  // plan along.
  const meant = (op: Op, f: Pick<Front, 'enemies'>): boolean => !op.side && f.enemies.some((e) => (op.if ?? []).includes(`war:${nat},${e}`));
  const planned = (f: Front): boolean => list.some((op) => meant(op, f) && !flag(g, `st:done:${nat}:${op.id}`) && g.turn <= (op.to ?? 999)
    && (op.if ?? []).filter((c) => c !== 'quiet').every((c) => cond(g, nat, c, real)));
  const hurried = (op: Op): boolean => real.some((f) => meant(op, f) && boldness(g, nat, f) > 0);
  // By the book, a front that waits on a dated choice carrying 'wait' opens no general offensive (ruling 215(b)).
  const waits = (f: Front): boolean => list.some((op) => op.wait && meant(op, f) && !flag(g, `st:done:${nat}:${op.id}`) && g.turn <= (op.to ?? 999)
    && leash(g, nat, op.id).kind === 'dated' && (bookIf(nat, op.id) ?? op.if ?? []).filter((c) => c !== 'quiet').every((c) => cond(g, nat, c, real)));

  for (const f of real) {
    // (The far side: the front's month is its own, piece by piece.)
    // (A front that opens or keeps its offensive takes the corps on its own provinces: those after it read again.)
    if (ownFront(g, f)) { ownMonth(g, nat, f, T, stalled, { planned, waits }); readMembers(real, (x) => onSince(g, nat, x) > 0); continue; }
    const since = onSince(g, nat, f);
    // (Told to whoever is reading the staff's month: tests and readings. Nothing in the game hears it.)
    const told = (how: 'opened' | 'eager' | 'pushed' | 'fresh' | 'kept' | 'off' | 'idle' | 'quiet'): void => offensiveWatch.tell?.({ nat, front: f, own: false, was: since, now: onSince(g, nat, f), how, marks: T });
    if (since) {
      // A main effort opened out of impatience is given its season before anyone asks what it has gained.
      const byPlan = pushed && f.enemies.some((e) => flag(g, `st:pushed:${nat}:${e}`));
      const fresh = g.turn - since < 2 || byPlan || boldNow(g, nat) > 0;
      if (!fresh && inNameOnly(g, nat, f, since)) { calledOffIdle(g, nat, f); told('idle'); }
      else if (fresh || (f.ratio >= T.persist && (!stalled || f.ratio >= T.attackAt))) { setOn(g, nat, f, since); told(byPlan ? 'pushed' : fresh ? 'fresh' : 'kept'); }
      else { setOn(g, nat, f, 0); for (const e of f.enemies) g.flags[`st:rest:${nat}:${e}`] = g.turn + OPEN.rest; told('off'); }
    } else if (rested(g, nat, f)) {
      const usual = T.attackAt * inConcert(g, nat, f);
      // The longer the front has been quiet, the smaller the advantage a staff waits for: a nation with no
      // offensive of its own in it finds one at last, and nobody attacks at hopeless odds.
      const I = OPEN.impatience;
      const k0 = boldness(g, nat, f);
      const k = k0 < (I?.after.length ?? 0) && planned(f) ? 0 : k0;
      const none = T.attackAt >= 50;
      const eager = !k ? usual : none ? (k >= I.after.length ? I.any : usual) : Math.max(I.floor, usual * I.ratio[k - 1]);
      // (A front where the last offensive was one in name only waits for more strength, or for patience to run out.)
      if (f.ratio >= eager && !(!k && shy(g, nat, f)) && !waits(f)) {
        setOn(g, nat, f, g.turn);
        g.flags[`st:gain:${nat}`] = g.turn;
        if (f.ratio < usual) {
          g.flags[`st:bold:${nat}`] = g.turn + I.effort - 1;
          g.flags[`st:boldK:${nat}`] = k;
        }
        told(f.ratio < usual ? 'eager' : 'opened');
      } else told('quiet');
    } else told('quiet');
  }
  // (The far side: the by-country word follows the fronts, before a great choice of this month adds its own.)
  frontsKept(g, nat, real);

  // Where no general offensive is on, a front that has gone quiet may still be given a main effort for the season.
  mainEffort(g, nat, real, T, '', planned);

  // History's great choices. One is prepared at a time, and none while a great offensive is still being fought.
  const busy = real.some((f) => onSince(g, nat, f) && !cond(g, nat, 'quiet', [f]) && g.turn - flag(g, `st:gain:${nat}`) <= 1);
  let cur = flag(g, `st:op:${nat}`);
  // The history dial: at a stop that keeps the book a side-show history dated is launched beside the army being
  // brought up for the next great blow (Norway in April, the west in May), and its landings are given their second month.
  const book = !!stopDef(g).book;
  const go = (k: number, beside = false): void => {
    const op = list[k];
    // (A great offensive in a war already on is opened against the enemies it names.)
    const who = (op.declare ?? []).length ? targets(g, nat, op) : inWar(g, nat, op);
    const last = list[flag(g, `st:goOp:${nat}`) - 1];
    if (book && last?.landings && !op.landings && g.turn - flag(g, `st:go:${nat}`) === 1) { g.flags[`st:landAt:${nat}`] = flag(g, `st:go:${nat}`); g.flags[`st:landOp:${nat}`] = flag(g, `st:goOp:${nat}`); }
    g.flags[`st:done:${nat}:${op.id}`] = g.turn;
    g.flags[`st:go:${nat}`] = g.turn;
    g.flags[`st:goOp:${nat}`] = k + 1;
    if (!beside) delete g.flags[`st:op:${nat}`];
    if (who.length) {
      g.flags[`st:push:${nat}`] = g.turn + (op.push ?? 2) - 1;
      g.flags[`st:gain:${nat}`] = g.turn;
      for (const e of who) { g.flags[onKey(nat, e, '')] = g.turn; g.flags[`st:pushed:${nat}:${e}`] = 1; }
    }
  };
  if (cur) {
    const op = list[cur - 1];
    const p = op ? prospect(g, nat, op, skip) : null;
    const still = !!op && (op.if ?? []).filter((c) => c !== 'quiet').every((c) => cond(g, nat, c, real));
    // The history dial: a blow the book has dated goes in on shorter odds, and is not put off once the army is up.
    const L = op ? leash(g, nat, op.id) : null;
    const dated = L?.kind === 'dated';
    const need = op?.ratio !== undefined ? op.ratio * (dated ? L!.nerve : 1) : 0;
    if (!op || !p || !still || (op.ratio !== undefined && p.ratio < 0.8 * need)) {
      // The moment has passed: stand the army down and think again in half a year.
      delete g.flags[`st:op:${nat}`];
      if (op) g.flags[`st:cool:${nat}:${op.id}`] = g.turn + 6;
      cur = 0;
    } else if (g.turn - flag(g, `st:opAt:${nat}`) >= (op.prep ?? 0) && inSeason(g, op) && (op.ratio === undefined || p.ratio >= need)
      && !(op.hold && !dated && !hurried(op) && inSeason(g, op, g.turn + 1) && monthly(g, nat, op.id, 'hold', g.turn) < op.hold)) {
      go(cur - 1);
      cur = 0;
    }
  }
  // One great choice a month: a second one launched in the same month would take the declaration of war away
  // from the first (the orders carry one operation), and the first would be marked done without ever happening.
  if ((!cur || book) && flag(g, `st:go:${nat}`) !== g.turn) {
    for (let k = 0; k < list.length; k++) {
      const op = list[k];
      // The history dial: by the book a choice history dated waits for its month and is then taken without a draw,
      // one that followed an ally into a war follows at once, and one history never took is not taken at all.
      const L = leash(g, nat, op.id);
      if (L.kind === 'never') continue;
      // While an army is being brought up for one great choice, only a side-show the book has dated goes in beside it.
      // So does a great choice of the book's that is dated no later than the one the army is coming up for: the
      // Balkan campaign of April 1941 was fought while the army for the east assembled. It goes in in its own
      // month, or the month before the greater blow if the two are dated together.
      const upL = cur ? leash(g, nat, list[cur - 1].id) : null;
      const first = !!cur && k !== cur - 1 && !op.side && L.kind === 'dated' && upL?.kind === 'dated' && L.date! <= upL.date!;
      if (cur && !(L.kind === 'dated' && (op.side || first))) continue;
      if (flag(g, `st:done:${nat}:${op.id}`) || g.turn < (op.from ?? 1) - L.early || g.turn > (op.to ?? 999) || g.turn < flag(g, `st:cool:${nat}:${op.id}`)) continue;
      if (L.kind === 'dated' && g.turn < (first ? Math.min(L.date!, upL!.date! - 1) : L.date! - (op.side ? 0 : op.prep ?? 0))) continue;
      if (!op.side && busy) continue;
      // (By the book a dated choice may wait on the book's own conditions in place of its staff's: history.json, 'bookIf'.)
      if (!((L.kind === 'dated' ? bookIf(nat, op.id) : null) ?? op.if ?? []).every((c) => cond(g, nat, c, real))) continue;
      const who = targets(g, nat, op);
      if ((op.declare ?? []).length && !who.length) { g.flags[`st:done:${nat}:${op.id}`] = -1; continue; }
      if (op.ratio !== undefined) { const p = prospect(g, nat, op, skip); if (!p || p.ratio < 0.9 * op.ratio * L.nerve) continue; }
      // The blow has to be able to fall in season once the army is up.
      // (A blow the book has dated falls in its month, or the month after it is taken up if that is later.)
      const falls = L.kind === 'dated' ? Math.max(g.turn + 1, L.date!) : g.turn + (op.prep ?? 0);
      if (!op.side && op.season && ![0, 1, 2].some((late) => inSeason(g, op, falls + late))) continue;
      // A staff thinks twice before a blow that would bring the whole League down on it.
      // (A front that has been quiet too long settles it: the plan is taken up without more waiting.)
      // (Not one whose month another test has already drawn: 'sure'.)
      if (L.kind === 'free' && !op.sure && !hurried(op) && monthly(g, nat, op.id, g.turn) >= op.chance * L.chance * deterred(g, nat, who)) continue;
      if (op.side || !(op.prep ?? 0) || first) {
        if (inSeason(g, op)) go(k, !!cur);
      } else {
        g.flags[`st:op:${nat}`] = k + 1;
        // (By the book the army has been coming up since the month history began to move it.)
        g.flags[`st:opAt:${nat}`] = L.kind === 'dated' ? Math.min(g.turn, L.date! - (op.prep ?? 0)) : g.turn;
      }
      break;
    }
  }
}

// ---- The plan phase the armies are given ------------------------------------------------------------------

const memo = new WeakMap<Game, Map<string, { turn: number; phase: PlanPhase | null }>>();

/** This month's orders for a computer-run nation's main army, as its staff has decided them. */
export function staffPhase(g: Game, nat: string, skip?: FrontOpts['skip']): PlanPhase | null {
  if (nat === g.human) return null;
  let m = memo.get(g);
  if (!m) memo.set(g, (m = new Map()));
  const hit = m.get(nat);
  if (hit && hit.turn === g.turn) return hit.phase;
  const phase = build(g, nat, skip);
  m.set(nat, { turn: g.turn, phase });
  return phase;
}

function build(g: Game, nat: string, skip?: FrontOpts['skip']): PlanPhase | null {
  const n = g.nations[nat];
  if (!n.alive || n.dormant) return null;
  const T = temper(g, nat);
  const list = ops(nat);
  const prep = list[flag(g, `st:op:${nat}`) - 1] ?? null;
  // The operation launched this month or last: its landings may take two months to get ashore.
  const wentAs = g.turn - flag(g, `st:go:${nat}`) <= 1 && flag(g, `st:go:${nat}`) ? list[flag(g, `st:goOp:${nat}`) - 1] ?? null : null;
  const declare = wentAs && flag(g, `st:go:${nat}`) === g.turn ? targets(g, nat, wentAs) : [];
  // (Its strikes and landings as it goes: at stops 3 to 5 the strike south's are cut to those it fights. asGoes.)
  const went = wentAs ? asGoes(g, nat, wentAs, declare) : null;
  // The fronts as they will stand once this month's declarations are made.
  const real = fronts(g, nat, { extra: new Set(declare), skip });
  // (One membership: the month's orders read what the month decided. A front whose offensive is on has spoken for
  // the corps on its own provinces. Busy is not handed to fronts() here: an old-chart front would keep otherwise.)
  readMembers(real, (x) => onSince(g, nat, x) > 0);
  // (A far-side front is given orders only for the ground its offensive is on: `underOrders`.)
  const live = real.filter((f) => onSince(g, nat, f) > 0 || f.enemies.some((e) => declare.includes(e)))
    .map((f) => underOrders(g, nat, f, declare)).filter((f): f is Front => !!f);
  const main = live[0] ?? null;
  const phase: PlanPhase = { name: '', from: g.turn, to: g.turn, posture: 'defend' } as PlanPhase;
  if (declare.length) phase.declare = declare;
  if (went?.strikes && flag(g, `st:go:${nat}`) === g.turn) phase.strikes = went.strikes;
  if (went?.landings) { phase.landings = went.landings; phase.to = flag(g, `st:go:${nat}`) + 6; if (went.quays) phase.quays = went.quays; }
  // (By the book: the landings of a side-show launched the month before the great blow fell are still going ashore.)
  const ashore = !went?.landings && flag(g, `st:landAt:${nat}`) && g.turn - flag(g, `st:landAt:${nat}`) <= 1 ? list[flag(g, `st:landOp:${nat}`) - 1] ?? null : null;
  if (ashore?.landings) { phase.landings = ashore.landings; phase.to = flag(g, `st:landAt:${nat}`) + 6; if (ashore.quays) phase.quays = ashore.quays; }
  if (live.length) {
    phase.enemies = [...new Set(live.flatMap((f) => f.enemies))];
    phase.objectives = [...new Set(live.flatMap((f) => objectives(g, f)))];
    // In a main effort opened out of impatience the staff accepts shorter odds battle by battle.
    const bold = boldNow(g, nat);
    phase.minOdds = bold ? Math.min(T.minOdds, OPEN.impatience.odds[bold - 1]) : T.minOdds;
    phase.release = [...new Set(live.flatMap((f) => jumpOff(g, nat, f)))];
    phase.targets = [...new Set(live.flatMap((f) => bound(g, nat, f, [], false)))];
  }
  if (prep) {
    const p = prospect(g, nat, prep, skip);
    phase.posture = 'redeploy';
    phase.name = `Building up: ${prep.name}`;
    phase.from = flag(g, `st:opAt:${nat}`);
    if (p) {
      const first = p.ours.filter((id) => allied(g, nat, g.regions[id].ctrl));
      const way = axis(g, nat, p);
      const door = way.length ? g.byName[way[0]] : null;
      const second = new Set<number>();
      for (const id of first) for (const j of g.regions[id].nb) if (g.regions[j].ctrl === nat && !first.includes(j) && j !== n.capital) second.add(j);
      phase.assembly = [...first, ...second].map((id) => g.regions[id].name);
      // The fast troops gather opposite the way in.
      const at = door ? first.filter((id) => door.nb.includes(id)) : [];
      const beside = first.filter((id) => !at.includes(id) && at.some((a) => g.regions[a].nb.includes(id)));
      phase.armour = [...at, ...beside].slice(0, 3).map((id) => g.regions[id].name);
      // Occupation troops stay in the great cities of beaten countries, away from where the army is gathering.
      const gather = new Set([...first, ...second]);
      phase.garrison = g.regions
        .filter((r) => r.ctrl === nat && r.owner !== nat && !g.nations[r.owner]?.alive && (r.cap || r.vp >= 2) && !gather.has(r.id))
        .sort((a, b) => b.vp - a.vp || a.id - b.id).slice(0, 6).map((r) => r.name);
    }
  } else if (main) {
    phase.posture = 'attack';
    phase.name = went && declare.length ? went.name : `The offensive against ${g.nations[main.key]?.name ?? main.key}`;
    phase.from = onSince(g, nat, main) || g.turn;
    const way = axis(g, nat, main, true);
    const opening = phase.from === g.turn;
    phase.axis = way.slice(0, opening && n.blitz ? OPEN.thrust[0] : OPEN.thrust[1]);
    const blow = bound(g, nat, main, way, opening);
    phase.targets = [...new Set(live.flatMap((f) => (f === main ? blow : bound(g, nat, f, [], opening))))];
    phase.main = blow;
    // No army takes a capital deep in enemy country in its first bound: the opening month goes to breaking the front.
    if (opening) phase.haltOrder = g.regions.filter((r) => r.cap && main.depth[r.id] >= 3 && main.enemies.includes(r.owner)).map((r) => r.name);
    // The will to finish: with the numbers and no ground gained for months, shorter odds and a third wave.
    const press = pressing(g, nat, main, phase.minOdds ?? T.minOdds);
    if (press) { phase.minOdds = press.minOdds; phase.waves = press.waves; }
  } else {
    // No general offensive: the season's main effort, if the staff has named one.
    const eff = effortOf(g, nat);
    const orders = eff ? effortOrders(g, nat, eff, real, T) : null;
    if (orders) Object.assign(phase, orders);
    else if (went) phase.name = went.name;
  }
  // With the enemy a few marches from the capital, the staff keeps a force in it whatever else is going on.
  const cap = n.capital >= 0 ? g.regions[n.capital] : null;
  if (cap && cap.ctrl === nat && nearEnemy(g, nat, cap, OPEN.guardWithin)) phase.staging = { ...(phase.staging ?? {}), [cap.name]: OPEN.guard };
  // On the hardest level a rival with shipping keeps a force by the quay for a descent on the player's coast.
  // The coast watch: the harbours of the homeland an enemy's ships could put an army into are not left empty.
  const watch = coastWatch(g, nat);
  for (const [name, count] of Object.entries(watch)) phase.staging = { ...(phase.staging ?? {}), [name]: Math.max(phase.staging?.[name] ?? 0, count) };
  if (Object.keys(watch).length) phase.watch = Object.keys(watch);
  // Ruling 233, riders b612 and b627: the ward. The capital of an ally whose leaving is in play is guarded by a corps
  // of the field army, named once and kept until the ward ends (ai.ts, keepWards): it is no watched harbour, which is
  // sent a fresh corps each month and only where a train can carry it the whole way into a town with room.
  const ward = wards(g, nat);
  if (ward.length) phase.ward = Object.fromEntries(ward.map((name) => [name, OPEN.ward!.corps]));
  // So does a staff opening a second front, at home and on a far shore: and while its men ashore still have a front, all its ships can carry.
  for (const slot of [0, 1]) {
    const quay = seaBase(g, nat, slot);
    if (!quay) continue;
    const sea = seaFoe(g, nat, slot)!;
    const lod = sea.second ? lodgment(g, nat, slot) : null;
    // (With its men ashore on its own side of the ocean and not short, a staff keeps nobody more at that quay.)
    if (!slot && lod && !lod.short && neighbourAcrossTheWater(g, nat, sea.foe)) continue;
    // (And where only its strength kept its force from every beach last month, it builds up: the quay is filled,
    // and the shipping desk builds the ships to lift what stands there. Version 83, the test of strength.)
    const more = !!STRENGTH && flag(g, `st:more:${nat}${SLOT[slot]}`) > 0 && g.turn - flag(g, `st:more:${nat}${SLOT[slot]}`) <= 2;
    const want = lod?.short ? Math.max(sea.L.force, Math.min(n.sealift, STACK)) : more ? Math.max(sea.L.force, Math.min(STRENGTH!.wave, STACK)) : sea.L.force;
    phase.staging = { ...(phase.staging ?? {}), [quay.name]: Math.max(phase.staging?.[quay.name] ?? 0, want) };
  }
  // A war for a strip of frontier (an op with 'near', the Winter War): while it is the staff's only war, only the troops
  // near the front take part. The army facing the great powers stays where it stands. Only that war: once its story is
  // over (the peace made, or the great war has swallowed it), a later war with the same country is fought in full.
  const wars = new Set([...n.atWar, ...declare]);
  const small = wars.size ? list.find((op) => op.near !== undefined && flag(g, `st:done:${nat}:${op.id}`) > 0 && !(op.peace && winterOver(g)) && [...wars].every((e) => (op.declare ?? []).includes(e))) : undefined;
  if (small) {
    phase.near = small.near;
    const aims = (small.aims ?? []).filter((nm) => g.byName[nm] && wars.has(g.byName[nm].ctrl));
    // (Its own ground the enemy has taken is won back all the same: the aims limit how far into his country it goes.)
    const home = g.regions.filter((r) => r.owner === nat && wars.has(r.ctrl)).map((r) => r.name);
    if (small.aims) { phase.targets = [...new Set([...aims, ...home])]; phase.main = aims.length ? aims : home; phase.strict = true; delete phase.haltOrder; }
  }
  // A force kept ready for a stroke still to come.
  for (const op of list) {
    if (!op.staging || flag(g, `st:done:${nat}:${op.id}`) || g.turn > (op.to ?? 999)) continue;
    phase.staging = { ...(phase.staging ?? {}), ...op.staging };
  }
  // By the book, ground beyond the record's line is named in none of the staff's orders (line.ts).
  const open = (nm: string): boolean => !g.byName[nm] || !lineShut(g, nat, g.byName[nm]);
  for (const k of ['targets', 'objectives', 'axis', 'haltOrder'] as const) if (phase[k]?.some((nm) => !open(nm))) phase[k] = phase[k]!.filter(open);
  return phase;
}

/**
 * True where this nation's field troops stand next to ground an enemy holds: a land front with him. With `among`,
 * only the troops standing on that ground count (the army at home, say, and not an expedition overseas).
 */
function inTouch(g: Game, nat: string, e: string, among?: Uint8Array): boolean {
  for (const r of g.regions) {
    if (r.ctrl !== e) continue;
    for (const j of r.nb) if ((!among || among[j]) && g.regions[j].units.some((u) => u.nat === nat && !u.dead && u.kind !== 'fort')) return true;
    if (r.units.some((u) => u.nat === nat && !u.dead)) return true;
  }
  return false;
}

/**
 * Ruling 233, rider b612: the ward. A computer staff at the dial's looser stops guards the capital of an ally whose own
 * leaving is in play: an enemy holds or stands on a place that ally's leaving event names (diplomacy.ts, leavings), and
 * the capital can be reached overland from this staff's own through its side's ground. That is the record's reason
 * (Italy in the summer of 1943). One guard, not one from every partner: it is kept by the strongest army among the
 * ally's partners on the Continent that are run by a computer or not, and if that is the player the choice is his.
 * Returns the capitals this staff guards.
 */
export function wards(g: Game, nat: string): string[] {
  const W = OPEN.ward, n = g.nations[nat];
  if (!W || !isOpen(g) || !W.stops.includes(stopOf(g)) || nat === g.human || !n?.alive || n.capital < 0 || n.seaSupply || !n.atWar.length) return [];
  const out: string[] = [];
  let on: Uint8Array | null = null;
  for (const L of leavings(g)) {
    const A = g.nations[L.nation];
    if (!A?.alive || A.id === nat || A.capital < 0 || !allied(g, nat, A.id)) continue;
    const cap = g.regions[A.capital];
    if (cap.ctrl !== A.id || out.includes(cap.name)) continue;
    if (!L.grounds.some((nm) => { const r = g.byName[nm]; return !!r && (atWar(g, A.id, r.ctrl) || hostileUnits(g, r, A.id).length > 0); })) continue;
    const keeper = Object.values(g.nations)
      .filter((x) => x.id !== A.id && x.alive && !x.dormant && !x.seaSupply && x.capital >= 0 && x.atWar.length > 0 && allied(g, x.id, A.id))
      .sort((a, b) => b.armyPeak - a.armyPeak || (a.id < b.id ? -1 : 1))[0];
    if (keeper?.id !== nat) continue;
    if (!on) {
      on = new Uint8Array(g.regions.length);
      const list = [n.capital];
      on[n.capital] = 1;
      for (let i = 0; i < list.length; i++) for (const j of g.regions[list[i]].nb) if (!on[j] && (g.regions[j].ctrl === nat || allied(g, nat, g.regions[j].ctrl))) { on[j] = 1; list.push(j); }
    }
    if (on[cap.id]) out.push(cap.name);
  }
  return out;
}

/** The ground a nation holds in one piece with its capital, nearest first, and a mark on each province of it. Nothing for a capital off the map. */
function homeGround(g: Game, nat: string): { list: number[]; on: Uint8Array } {
  const on = new Uint8Array(g.regions.length);
  const cap = g.nations[nat].capital;
  if (cap < 0) return { list: [], on };
  const list = [cap];
  on[cap] = 1;
  for (let i = 0; i < list.length; i++) for (const j of g.regions[list[i]].nb) if (!on[j] && g.regions[j].ctrl === nat) { on[j] = 1; list.push(j); }
  return { list, on };
}

/** The two forces a staff may keep for the sea: 0 the army at home, 1 an army standing idle on a far shore. Each has its own flags. */
const SLOT = ['', ':o'];

/**
 * The harbour a nation's second front began at (0 the landing from home, 1 the one from a far shore), while the
 * nation still holds it (or the country round it, with its town under siege) and the ground won from it still
 * touches the enemy, with every port on that ground whose quays are in friendly hands (the first harbour first:
 * none while its town still holds out and no other has been taken). `short` while the men ashore are still too
 * few: fewer than 'hold' divisions, or than 'feed' times the enemy army that stands next to their ground. Null
 * once the harbour is lost or the front has gone.
 */
export function lodgment(g: Game, nat: string, slot = 0): { at: Region; ports: Region[]; short: boolean } | null {
  const at = g.regions[flag(g, `st:lodged:${nat}${SLOT[slot]}`) - 1];
  if (!at || !at.port || (at.ctrl !== nat && fieldCtrl(g, at) !== nat)) return null;
  const D = OPEN.descent;
  const seen = new Uint8Array(g.regions.length);
  const q = [at.id];
  seen[at.id] = 1;
  let front = false, mine = 0, foes = 0;
  for (let i = 0; i < q.length; i++) {
    const r = g.regions[q[i]];
    // A town on this ground that still holds out is a front of its own.
    if (atWar(g, nat, r.ctrl)) front = true;
    for (const u of r.units) if (u.nat === nat && !u.dead && u.kind !== 'fort') mine += u.divs * u.str;
    for (const j of r.nb) {
      if (seen[j]) continue;
      const c = fieldCtrl(g, g.regions[j]);
      if (allied(g, nat, c)) { seen[j] = 1; q.push(j); continue; }
      if (!atWar(g, nat, c)) continue;
      front = true;
      seen[j] = 2;
      for (const u of g.regions[j].units) if (!u.dead && u.kind !== 'fort' && atWar(g, nat, u.nat)) foes += u.divs * u.str;
    }
  }
  if (!front) return null;
  // (A force put ashore for an enemy on the staff's own side of the ocean is the force its forecast asks for and no
  // more: it is short only against the enemy army next to its ground, never against the fifteen divisions a
  // lodgment across the water is built up to. Version 81, ruling 80.)
  const hold = keepsDoor(g, nat) && beyondEurope(at.id) ? 0 : D.hold ?? 15;
  return { at, ports: q.map((id) => g.regions[id]).filter((r) => r.port && r.seas.length > 0 && allied(g, nat, r.ctrl)), short: mine < Math.max(hold, (D.feed ?? 2) * foes) };
}

/** The seas a nation's troopships can reach from a quay within a landing's range, over seas not closed to them: the seas crossed to each, -1 where they cannot go. */
function lanes(g: Game, nat: string, from: Region, shut: boolean[]): Int16Array {
  const d = new Int16Array(g.seas.length).fill(-1);
  const q: number[] = [];
  for (const z of from.seas) if (!shut[z] && d[z] < 0) { d[z] = 1; q.push(z); }
  for (let i = 0; i < q.length; i++) {
    if (d[q[i]] >= NAVY.landing.range) continue;
    for (const j of g.seas[q[i]].adj) if (d[j] < 0 && !shut[j] && canPass(g, q[i], j, nat)) { d[j] = d[q[i]] + 1; q.push(j); }
  }
  return d;
}

/**
 * What a quay is worth for a descent on some shores: over the harbours its ships can reach, how lightly each is
 * held (1 for an empty harbour with no army within a march, less as that army grows, next to nothing under a sky
 * the enemy's fighters still hold), and the fewest seas to any of them. Worth nothing when no lane is open.
 */
function quayWorth(g: Game, nat: string, from: Region, shores: Region[], shut: boolean[]): { worth: number; seas: number } {
  const d = lanes(g, nat, from, shut);
  let worth = 0, seas = Infinity;
  for (const s of shores) {
    let k = Infinity;
    for (const z of s.seas) if (d[z] > 0 && d[z] < k) k = d[z];
    if (k === Infinity) continue;
    seas = Math.min(seas, k);
    if (!allied(g, nat, s.ctrl) && skyDenies(g, nat, s.id)) { worth += 0.02; continue; }
    let near = 0;
    for (const j of [s.id, ...s.nb]) for (const u of hostileUnits(g, g.regions[j], nat)) if (u.kind !== 'fort') near += u.divs * u.str;
    worth += 1 / (1 + near / 4);
  }
  return { worth, seas };
}

/**
 * True where a staff heeds its own forecast of the sea behind a beach (open.json, 'descent.heed'): it lands nobody
 * where that sea could be closed. On the mainland it does; for a landing that takes a whole island (a province with
 * no land frontier) it does not; and a further wave to men already ashore (`wave`) has a switch of its own.
 */
export function heedsCard(to: Region, wave = false): boolean {
  const H = OPEN.descent?.heed;
  return !!H && (wave ? H.wave : to.nb.length ? H.mainland : H.island);
}
/**
 * The corps a nation's great choices take off hold this month, by name: those a choice frees the month it goes
 * ('frees'), and, so many months after ('freesAfter'), the ones it named for its own landings.
 */
export function freed(g: Game, nat: string): string[] {
  const out: string[] = [];
  for (const o of ops(nat)) {
    if (!o.frees && o.freesAfter === undefined) continue;
    const at = flag(g, `st:done:${nat}:${o.id}`);
    if (!at) continue;
    if (at === g.turn) out.push(...(o.frees ?? []));
    if (o.freesAfter !== undefined && g.turn - at === o.freesAfter) out.push(...(o.landings ?? []).flatMap((L) => [L.corps, ...(L.with ?? [])]));
  }
  return out;
}
/** What a staff's own landing asks (open.json, 'descent'): the odds on a defended beach, and the most risk it takes on the crossing. Null where no staff lands of its own accord. */
export const descentAsks = (): { odds: number; risk: number } | null => (OPEN.descent ? { odds: OPEN.descent.odds, risk: OPEN.descent.risk } : null);
/** How many corps a staff sends ashore in a landing (open.json, 'descent.force'). */
export const landingForce = (): number => OPEN.descent?.force ?? 3;

/** The harbours a nation holds. */
const harbours = (g: Game, e: string): Region[] => g.regions.filter((r) => r.ctrl === e && r.port && r.seas.length > 0);

/**
 * The shores history planned for a staff's second front (open.json, 'descent.planned'), for a staff that has such a
 * list: 'open' names the countries on whose own shores it may open a front by a landing of its own, 'join' those on
 * whose ground it goes by ship to a front its side already holds. Colonies and islands overseas are open to it
 * always. A staff with no list goes where it judges best, and so does every staff at the stop of the history dial
 * where nobody has read the books.
 */
const planned = (g: Game, nat: string): Planned | null => {
  if (stopDef(g).bold) return null;
  const P = OPEN.descent?.planned?.[nat] ?? null;
  // (A list kept by the book only is put away at the stops where neutrals have ambitions.)
  return P && P.book && !dated(g) ? null : P;
};
/** True at the stops of the history dial where history's dates hold for a staff's own landings: all but the two loosest. */
const dated = (g: Game): boolean => !stopDef(g).ambitions;
/** True once an enemy of a country stands on its home ground, or holds any of it. */
const entered = (g: Game, id: string): boolean => g.regions.some((r) => r.owner === id && !r.overseas && (atWar(g, id, r.ctrl) || r.units.some((u) => !u.dead && atWar(g, id, u.nat))));
/**
 * True where a staff may open a second front by a landing of its own (see `planned`). History's dates (Version 79,
 * the design director's ruling 60): at the stops where they hold, a mainland shore with a date under 'from' is not
 * opened before that month. An island is open always, and a date lapses for as long as an enemy of the country
 * named for it under 'lapse' stands on that country's home ground (France's, once Germany itself is entered). A
 * province named under 'not' is no part of the list, whoever owns it (Albania is not the Italian mainland).
 */
export function opensOn(g: Game, nat: string, r: Region): boolean {
  const P = planned(g, nat);
  // (A staff that waits behind its allies opens no front of its own on a mainland at any stop: Version 83, ruling 89(e).)
  if (!P && waitsUndated(g, nat) && !r.overseas && r.nb.length > 0 && !ownShore(g, nat, r)) return false;
  if (!P || r.overseas || ownShore(g, nat, r)) return true;
  if (!P.open.includes(r.owner) || P.not?.includes(r.name)) return false;
  const from = P.from?.[r.owner];
  if (from === undefined || !dated(g) || !r.nb.length || g.turn >= from) return true;
  const lapse = P.lapse?.[r.owner];
  return !!lapse && entered(g, lapse);
}
/**
 * True for a mainland harbour of a staff's list whose month has not come and comes within 'lead' months (open.json,
 * 'descent.lead'; ruling 233(q)). The staff plans for it: the enemy across the sea is chosen for it, the quay, the
 * force gathered there, the beach named and its strength tried. It lands not before the month (`opensOn`). By the
 * book the invasion army gathered in Britain through the first half of 1944.
 */
export function opensSoon(g: Game, nat: string, r: Region): boolean {
  const lead = OPEN.descent?.lead ?? 0;
  const P = lead > 0 ? planned(g, nat) : null;
  const from = P?.from?.[r.owner];
  if (!P || from === undefined || !dated(g) || r.overseas || !r.nb.length || ownShore(g, nat, r)) return false;
  return P.open.includes(r.owner) && !P.not?.includes(r.name) && g.turn < from && g.turn >= from - lead && !opensOn(g, nat, r);
}
/** True for a mainland harbour of a staff's list in the very month history's list opens it (at the stops where the dates hold). */
export function opensNow(g: Game, nat: string, r: Region): boolean {
  const P = planned(g, nat);
  return !!P && dated(g) && !r.overseas && r.nb.length > 0 && P.open.includes(r.owner) && !P.not?.includes(r.name) && P.from?.[r.owner] === g.turn;
}
/**
 * True in the month history's list opens a mainland shore this enemy holds, for a staff that has planned for it since
 * the first month of the lead (open.json, 'descent.day'; ruling 233(u)): the list picked the day, and the staff lands
 * that month without the monthly dice where every test passes. Asked once a month for each army of a staff with that
 * enemy across the sea (`sfx` names the army): the month the lead begins it is noted that the plan stood.
 */
export function onTheDay(g: Game, nat: string, you: string, sfx: string): boolean {
  const D = OPEN.descent, lead = D?.lead ?? 0;
  const P = D?.day && lead > 0 && dated(g) ? planned(g, nat) : null;
  const key = `st:led:${nat}${sfx}`;
  let m = 0;
  if (P?.from) for (const r of harbours(g, you)) {
    const f = P.from[r.owner];
    if (f === undefined || r.overseas || !r.nb.length || !P.open.includes(r.owner) || P.not?.includes(r.name) || ownShore(g, nat, r)) continue;
    if (g.turn >= f - lead && g.turn <= f && (!m || f < m)) m = f;
  }
  if (m && g.turn === m - lead) g.flags[key] = m;
  else if (flag(g, key) && flag(g, key) < g.turn) delete g.flags[key];
  return m === g.turn && flag(g, key) === m;
}
/**
 * The build-up (open.json, 'descent.planned', 'with'): a staff with history's list opens no front of its own on a
 * mainland until this many of its field corps stand across the ocean: the landing force, and as many again to
 * follow it in before the enemy's reserves arrive. An island taken whole and a colony overseas need no such army.
 * True where the staff may land today.
 */
export function builtUp(g: Game, nat: string, r: Region): boolean {
  const need = planned(g, nat)?.with;
  return !need || !!r.overseas || !r.nb.length || ownShore(g, nat, r) || armyAcross(g, nat) >= need;
}
/** The field corps a nation has across its ocean. */
function armyAcross(g: Game, nat: string): number {
  let across = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && !acrossHome(g, nat, c.reg)) across++;
  return across;
}
/**
 * True while a staff with history's list is still building its army up across the sea for the front planned there:
 * it has corps on the far shore, and fewer than it waits for. Its war is not yet one nobody can fight (`stalemates`).
 * Nor is it while the staff waits for history's date for a shore on its list (Version 79): it will come that month.
 */
export function massing(g: Game, nat: string): boolean {
  const P = planned(g, nat);
  if (!P) return false;
  const across = P.with ? armyAcross(g, nat) : -1;
  if (P.with && across <= 0) return false;
  return (!!P.with && across < P.with) || awaiting(g, nat, P);
}
/** True while a staff with history's dates waits for one: an enemy holds a harbour on a mainland shore of its list whose month has not come. */
function awaiting(g: Game, nat: string, P: Planned): boolean {
  const from = P.from;
  if (!from || !dated(g)) return false;
  return g.regions.some((r) => r.port && r.seas.length > 0 && !r.overseas && r.nb.length > 0 && g.turn < (from[r.owner] ?? 0)
    && P.open.includes(r.owner) && !P.not?.includes(r.name) && atWar(g, nat, r.ctrl) && !opensOn(g, nat, r));
}
/** True for ground on a nation's own side of the ocean: beyond the old chart for a country whose capital is, on it for the others. */
const acrossHome = (g: Game, nat: string, reg: number): boolean => { const cap = g.nations[nat].capital; return cap < 0 || beyondEurope(cap) === beyondEurope(reg); };
/**
 * True for ground on a staff's own side of the ocean, where its capital stands beyond the old chart (Version 81, the
 * design director's rulings 75 and 76). History's list of shores was written for the far side of the water: on its
 * own side a staff has no list and no build-up figure, and goes where it judges best.
 */
export const ownShore = (g: Game, nat: string, r: Region): boolean => keepsDoor(g, nat) && beyondEurope(r.id);
/** True for a staff the rule is written for (open.json, 'descent.door'): its capital stands beyond the old chart. */
export const keepsDoor = (g: Game, nat: string): boolean => { const cap = g.nations[nat]?.capital ?? -1; return cap >= 0 && beyondEurope(cap) && (OPEN.descent?.door?.includes(nat) ?? false); };
/**
 * True for an enemy whose home is on a staff's own side of the ocean (Version 81): its own capital stands beyond the
 * old chart, and so does his. He is the staff's first business: it does not wait a quiet year to cross to him.
 */
export const neighbourAcrossTheWater = (g: Game, nat: string, e: string): boolean => {
  const his = g.nations[e]?.capital ?? -1;
  // (A neighbour by the map is met by land, as he was: Mexico is no business for the ships. Ruling 76b. One who
  // touches only ground the staff's country owns across the sea is not one: Panama is reached by ship.)
  return his >= 0 && beyondEurope(his) && keepsDoor(g, nat) && !neighbourByLand(g, nat, e);
};
/**
 * True where a staff sends its corps by ship to a front its side already holds (see `planned`); `by` names the ally
 * whose landing the harbour is, where it is one. A staff that waits behind its allies (Version 79, ruling 60:
 * Canada, the island's guard) goes on from its base not before 'joinFrom', and only behind one of the allies named
 * under 'behind': into that ally's landing, a harbour he holds, or one where his field corps stand in it or next to
 * it. That holds at the stops of the history dial where history's dates do, and there, with the whole world on the
 * chart, a colony across an ocean is shut to such a staff for good (ruling 231, 3, and its rider b462).
 */
export function joinsOn(g: Game, nat: string, r: Region, by?: string): boolean {
  const P = planned(g, nat);
  // (Version 83, ruling 89(e): at the stops where history's dates do not hold, a staff that waits behind its allies
  // has no date to wait for, and goes on from its base only into a landing of theirs that is ashore to stay.)
  const W = waitsUndated(g, nat);
  if (W) {
    if (r.overseas || ownShore(g, nat, r) || based(W, r)) return true;
    if (P?.join && !P.join.includes(r.owner)) return false;
    return behindToStay(g, nat, r, W.behind ?? []);
  }
  // A colony overseas is open always (ruling 60(b)1). Not to a staff that waits behind its allies, once the whole
  // world is on the chart (ruling 231, 3, and its rider b462): the island's guard holds against it at the stops where
  // history's dates hold, and a colony across an ocean stays shut to it for good. Canada's corps cross only to
  // Britain, and go on from it as below, to the continent, not before 1943 and behind a British or an American
  // landing; her army never went to Africa as a formation. (On the older chart a colony near an enemy was a rare
  // thing. With Italy's Horn of Africa on it there is one from June 1940, and Canada's army sailed for Tripoli and
  // Alexandria.) A colony on its own side of the ocean is open to it as ever: no ocean is crossed for it.
  const guard = !!P?.behind && dated(g) && farOn(g);
  if (!P || ownShore(g, nat, r)) return true;
  if (r.overseas) return !guard || shoreOf(r) === shoreOf(g.nations[nat].capital);
  if (P.join && !P.join.includes(r.owner)) return false;
  if (!P.behind || !dated(g) || based(P, r)) return true;
  const B = P.behind;
  if (g.turn < (P.joinFrom ?? 0)) return false;
  return (by !== undefined && B.includes(by)) || B.includes(r.ctrl)
    || [r.id, ...r.nb].some((j) => g.regions[j].units.some((u) => !u.dead && u.kind !== 'fort' && B.includes(u.nat)));
}
/** True for the home ground of a country a staff is based on while it waits behind its allies (open.json, 'base'). */
const based = (P: Planned, r: Region): boolean => !!P.base?.includes(r.owner) && !r.overseas;
/**
 * The list of a staff that waits behind its allies, at the stops of the history dial where history's dates do not
 * hold (Version 83, the design director's ruling 89(e): Canada at the two loosest stops). Null for every other
 * staff, at the other stops, and where the rules have no test of strength.
 */
const waitsUndated = (g: Game, nat: string): Planned | null => {
  if (!STRENGTH || dated(g)) return null;
  const P = OPEN.descent?.planned?.[nat];
  return P?.behind ? P : null;
};
/**
 * True for a harbour on the ground of a landing that one of the allies named has made and that is ashore to stay:
 * the men on that ground pass the test of strength as they stand, with nobody more sent to them.
 */
function behindToStay(g: Game, nat: string, r: Region, behind: string[]): boolean {
  for (const b of behind) {
    if (!g.nations[b]?.alive) continue;
    for (const slot of [0, 1]) {
      const l = lodgment(g, b, slot);
      if (l && (l.at === r || l.ports.includes(r)) && staysAshore(g, nat, b, l.at)) return true;
    }
  }
  return false;
}

/**
 * The chance a computer staff asks for before it puts a force across the sea (Version 83, the test of strength;
 * open.json, 'descent.strength', and ashore.ts): 'sure', or 'bold' for a staff of bold character, and never less
 * than an even chance. Null where no test is made: in history's campaigns, and for the player's own country.
 */
export function strengthAsked(g: Game, nat: string): number | null {
  const S = STRENGTH;
  if (!S || !isOpen(g) || nat === g.human) return null;
  return Math.max(S.even, rivalRules(g) && characterOf(g, nat) === 'bold' ? S.bold : S.sure);
}
/**
 * True where a force put down at a place passes the staff's test of strength: against what the defender can bring
 * there within three months, its chance of being still ashore is what the staff asks for or better. `follow` is the
 * strength its ships can bring in after it within the time (ashore.ts, `followUp`). True wherever no test is made.
 */
export function holdsAshore(g: Game, nat: string, at: Region, force: Corps[], follow = 0): boolean {
  const ask = strengthAsked(g, nat);
  if (ask === null) return true;
  const R = reckon(g, nat, at, force, follow);
  if (R) strengthWatch.tell?.({ nat, at, force, ask, R });
  return !R || R.chance >= ask;
}
/**
 * The main front first (Version 83, the design director's ruling 98(e); open.json, 'descent.strength.first'): for the
 * staffs named there, of the harbours of a front their side holds, those on the home ground named come before every
 * other, a harbour gained by a treaty among them, each in the order it had. Every one is still put to the test of
 * strength in its turn. For any other staff the list is as it was.
 */
export function mainFirst(nat: string, ports: Region[]): Region[] {
  const F = STRENGTH?.first;
  if (!F || !F.for.includes(nat)) return ports;
  const main = (p: Region): boolean => F.ground.includes(p.owner) && !p.overseas;
  return ports.some(main) ? [...ports.filter(main), ...ports.filter((p) => !main(p))] : ports;
}

/** For the measuring harness: told of every reckoning a staff makes before it sails, and what it asked for. Nothing in the game listens. */
export const strengthWatch: { tell: ((e: { nat: string; at: Region; force: Corps[]; ask: number; R: Reckoning }) => void) | null } = { tell: null };
/** True where the men an ally has on the ground round a place are ashore to stay, as the asking staff reckons it: they pass the test with nobody more sent. */
function staysAshore(g: Game, nat: string, ally: string, at: Region): boolean {
  const S = STRENGTH;
  if (!S) return true;
  const R = reckon(g, ally, at, []);
  return !R || R.chance >= (strengthAsked(g, nat) ?? S.sure);
}

/**
 * Whom a staff with shipping means to cross the sea against, and the rules it lands by. On the hardest level a
 * rival goes for the player's coasts. On every level a nation that has been at war with an enemy for a year and
 * more with no land front against him at home and no fighting opens a second front, against an enemy whose
 * harbours its home ports can reach: the choice stands for a year, and after that for as long as his coast is
 * still in reach and no front has opened at home, or the men it put ashore still have a front to hold. A nation
 * with the enemy on its own soil sends nobody. The second force (slot 1) is an army standing idle on a far
 * shore: see `abroad`. Null for the player and for a nation with no ships.
 */
export function seaFoe(g: Game, nat: string, slot = 0): { foe: string; L: Landings; second?: boolean } | null {
  const n = g.nations[nat];
  if (!n || !n.alive || n.dormant || n.sealift <= 0 || nat === g.human) return null;
  const D = OPEN.descent;
  if (slot) {
    const far = D && isOpen(g) ? abroad(g, nat) : null;
    return far ? { foe: far.foe, L: D, second: true } : null;
  }
  if (n.capital < 0) return null;
  const L = landingsFor(g, nat), you = seat(g);
  if (L && you !== null && atWar(g, nat, you)) return { foe: you, L };
  if (!D || !isOpen(g)) return null;
  if (g.regions.some((r) => r.owner === nat && !r.overseas && atWar(g, nat, r.ctrl))) return null;
  const ids = Object.keys(g.nations);
  const key = `st:foe:${nat}`, at = `st:foeAt:${nat}`;
  const home = homeGround(g, nat);
  const from = [...new Set(home.list.map((id) => g.regions[id]).filter((r) => r.port && r.owner === nat && !r.overseas).flatMap((r) => r.seas))];
  // (A staff with history's list of shores looks only at those of his harbours it may open on this month, or within the lead: `opensSoon`.)
  const inReach = (e: string): boolean => from.length > 0 && seaHops(g, from, harbours(g, e).filter((r) => opensOn(g, nat, r) || opensSoon(g, nat, r))) <= NAVY.landing.range;
  const kept = ids[flag(g, key) - 1];
  // (History's list has an order, ruling 233(j): when the month comes for a later shore of the list, the home army
  // turns to the enemy who holds it nearest the home ports, unless it has men ashore with a front to hold. France
  // in June 1944, though Italy still stands; and Germany on the Channel, not Italy at Marseille. It turns 'lead'
  // months before the shore's month, ruling 233(q): the invasion army gathered in Britain through the first half of
  // 1944, and a landing takes a season to mount. It lands not before the month (`opensOn`). The army abroad keeps
  // the first: see `abroad`.)
  const P = planned(g, nat);
  const dueOf = new Map<string, number>();
  /** The month of the latest shore of the list an enemy holds a harbour on, within a landing of the home ports, with the hops to it taken off as a fraction: later first, then nearer. Nothing until 'lead' months before it. */
  const due = (e: string): number => {
    if (!P?.from || !dated(g) || !from.length) return 0;
    let m = dueOf.get(e);
    if (m !== undefined) return m;
    m = 0;
    for (const r of harbours(g, e)) {
      const f = P.from[r.owner];
      if (f === undefined || f < Math.floor(m) || g.turn < f - (D.lead ?? 0) || r.overseas || !r.nb.length || !P.open.includes(r.owner) || P.not?.includes(r.name)) continue;
      const hops = seaHops(g, from, [r]);
      if (hops <= NAVY.landing.range) m = Math.max(m, f + 1 - (hops + 1) / (NAVY.landing.range + 2));
    }
    dueOf.set(e, m);
    return m;
  };
  const turnsTo = (e: string): boolean => { const x = g.nations[e]; return e !== kept && !!x?.alive && x.capital >= 0 && due(e) > (kept ? due(kept) : 0) && !inTouch(g, nat, e, home.on); };
  const later = !!kept && !!P?.from && !lodgment(g, nat) && n.atWar.some(turnsTo);
  if (kept && g.nations[kept].alive && atWar(g, nat, kept) && !later) {
    if (g.turn - flag(g, at) < 12 || lodgment(g, nat)) return { foe: kept, L: D, second: true };
    if (inReach(kept) && !inTouch(g, nat, kept, home.on)) { g.flags[at] = g.turn; return { foe: kept, L: D, second: true }; }
  }
  let best: string | null = null, bq = D.after - 1;
  // (A country that went to war for ground across the sea does not wait a year to cross it: the history dial's ambitions.)
  const claim = ids[flag(g, `st:claim:${nat}`) - 1];
  for (const e of [...n.atWar].sort()) {
    const x = g.nations[e];
    if (!x?.alive || x.capital < 0) continue;
    // (Nor does a staff beyond the old chart wait a year for an enemy on its own side of the ocean: he is its first business. Version 81.)
    const q = e === claim ? 999 : later && turnsTo(e) ? 900 + due(e) : neighbourAcrossTheWater(g, nat, e) ? 500 + quietWith(g, nat, e) : quietWith(g, nat, e);
    if (q > bq && !inTouch(g, nat, e, home.on) && inReach(e)) { bq = q; best = e; }
  }
  if (!best) { delete g.flags[key]; return null; }
  g.flags[key] = ids.indexOf(best) + 1;
  g.flags[at] = g.turn;
  return { foe: best, L: D, second: true };
}

/**
 * The army overseas. A nation with shipping whose troops stand idle on a far shore (friendly ground not in one
 * piece with its capital, with no enemy on it or next to it) crosses from a port there against an enemy it has not
 * fought for a season. The quay and the enemy are chosen together, by how lightly the harbours in reach are held,
 * from the ports with a landing force of its own within three marches (of two that are equal, the one it stands at:
 * ruling 233(y)), and looked at again every three months.
 * While the men it put ashore still have a front to hold, the same quay feeds them. With no lane open from any of
 * those ports it waits where it was, or at the port nearest his harbours (open.json, 'descent.wait').
 */
function abroad(g: Game, nat: string): { foe: string; quay: Region } | null {
  const D = OPEN.descent, n = g.nations[nat];
  const ids = Object.keys(g.nations);
  const kf = `st:foe:${nat}:o`, kq = `st:quay:${nat}:o`, at = `st:quayAt:${nat}:o`, none = `st:far:${nat}`;
  const foe = ids[flag(g, kf) - 1], quay = g.regions[flag(g, kq) - 1];
  // A quay must have room for the force among whatever corps of its allies stand there: three Americans can never stand in a harbour five British corps fill.
  const room = (p: Region): boolean => STACK - placed(g, p, nat).filter((u) => u.nat !== nat).length >= D.force;
  if (foe && quay && g.nations[foe].alive && atWar(g, nat, foe) && allied(g, nat, quay.ctrl) && room(quay) && (g.turn - flag(g, at) < 3 || lodgment(g, nat, 1)?.short)) return { foe, quay };
  if (flag(g, none) === g.turn) return null;
  const drop = (): null => { delete g.flags[kf]; delete g.flags[kq]; g.flags[none] = g.turn; return null; };
  const foes = [...n.atWar].sort().filter((e) => { const x = g.nations[e]; return !!x?.alive && x.capital >= 0 && quietWith(g, nat, e) >= (D.abroad ?? 3); });
  if (!foes.length) return drop();
  // (A staff with history's list of shores looks only at those: it does not go elsewhere because elsewhere is clear.)
  const shores = foes.map((e) => harbours(g, e).filter((r) => opensOn(g, nat, r) || opensSoon(g, nat, r)));
  if (!shores.some((s) => s.length)) return drop();
  const home = homeGround(g, nat).on;
  // Each far shore in turn: a piece of friendly ground away from home. One with the enemy on it or next to it has work of its own.
  const seen = new Uint8Array(g.regions.length);
  const idle: Region[] = [];
  for (const r0 of g.regions) {
    if (seen[r0.id] || home[r0.id] || !allied(g, nat, r0.ctrl)) continue;
    const q = [r0.id];
    seen[r0.id] = 1;
    let front = false, mine = 0;
    for (let i = 0; i < q.length; i++) {
      const r = g.regions[q[i]];
      if (hostileUnits(g, r, nat).length) front = true;
      for (const u of r.units) if (u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng') mine++;
      for (const j of r.nb) {
        const c = g.regions[j].ctrl;
        if (atWar(g, nat, c)) front = true;
        else if (!seen[j] && !home[j] && allied(g, nat, c)) { seen[j] = 1; q.push(j); }
      }
    }
    if (!front && mine >= D.force) for (const id of q) if (g.regions[id].port && g.regions[id].seas.length > 0) idle.push(g.regions[id]);
  }
  if (!idle.length) return drop();
  const shut = g.seas.map((z) => closedTo(g, z.id, nat));
  let best: { foe: string; quay: Region } | null = null, bw = 0, bs = Infinity, bh = -1;
  let near: { foe: string; quay: Region } | null = null, bd = Infinity;
  for (const p of idle) {
    if (!room(p)) continue;
    // The corps that could be on this quay within the season: those there now, and those within three marches that would not leave a place empty.
    const d = new Int16Array(g.regions.length).fill(-1);
    const q = [p.id];
    d[p.id] = 0;
    let spare = 0;
    for (let i = 0; i < q.length; i++) {
      const r = g.regions[q[i]];
      const own = r.units.filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng');
      spare += r.id === p.id ? own.length : Math.min(own.filter((u) => !KIND[u.kind].mobile).length, Math.max(0, own.length - 1));
      if (d[r.id] >= 3) continue;
      for (const j of r.nb) if (d[j] < 0 && allied(g, nat, g.regions[j].ctrl)) { d[j] = d[r.id] + 1; q.push(j); }
    }
    if (spare < D.force) continue;
    // (Ruling 233(y), open.json `descent.stands`: of two quays equal in worth and in seas the army names the one it
    // stands at, the port with more of its own corps on the quay, then with more of them within the marches. Before,
    // the first port of the list won: in May 1944 America's army, four corps in Cornwall and two in Kent, named Kent;
    // the four were sent elsewhere, and for three months the two were one short of a landing force.)
    const stands = D.stands ? 100 * p.units.filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng').length + spare : 0;
    foes.forEach((e, k) => {
      const w = quayWorth(g, nat, p, shores[k], shut);
      const even = w.worth > 0 && Math.abs(w.worth - bw) <= 1e-9;
      if (w.worth > bw + 1e-9 || (even && (w.seas < bs || (w.seas === bs && stands > bh)))) { best = { foe: e, quay: p }; bw = w.worth; bs = w.seas; bh = stands; }
      // Seas between this quay and the nearest of his harbours, by the shortest way round, whoever holds them.
      const d = shores[k].length ? seaHops(g, p.seas, shores[k]) : Infinity;
      if (d < bd) { bd = d; near = { foe: e, quay: p }; }
    });
  }
  // (Ruling 233(r): with no lane open from any quay the army waits, as the home army does (`seaBase`): at the quay
  // it had, or at the one nearest his harbours by the shortest way round. Its beach is named from there and its own
  // fleets stand on the way to it, which is how a closed sea is opened. Before, the plan was dropped the month the
  // lane shut, no beach was named, and the admirals, who cover a named crossing, sent nothing.)
  if (!best && D.wait) {
    if (foe && quay && foes.includes(foe) && shores[foes.indexOf(foe)].length && idle.includes(quay) && room(quay)) return { foe, quay };
    if (near !== null && bd <= NAVY.landing.range) best = near;
  }
  if (!best) return drop();
  const got = best as { foe: string; quay: Region };
  g.flags[kf] = ids.indexOf(got.foe) + 1;
  g.flags[kq] = got.quay.id + 1;
  g.flags[at] = g.turn;
  return got;
}

/**
 * The harbour a staff keeps its landing force at. For the army at home: a home port of its own, in touch with the
 * capital by land, and the one from which the most lightly held of the enemy's harbours can be reached over seas
 * open to its ships (while it has men ashore, the one with an open lane to them). With no lane open from any home
 * port the force waits where it was, or at the port nearest his harbours by the shortest way round. The choice is
 * kept while its lane stays open and looked at again every half year. For the army on a far shore: see `abroad`.
 *
 * Ruling 233(x) (open.json `descent.afresh`, `descent.room`): the choice is made afresh the month the staff's
 * target changes (another enemy, or another shore of history's list), and a home port where its allies' corps leave
 * no room for the force is passed over while another port serves, as the army abroad's quay already is. (Before,
 * Britain kept Cornwall, its quay for Sardinia, when it turned to France, and stood there short of its force beside
 * the Americans in 13 wars of 48 on the list's day: the invasion of France was mounted from every harbour of the
 * south coast, not from one.)
 */
export function seaBase(g: Game, nat: string, slot = 0): Region | null {
  const sea = seaFoe(g, nat, slot);
  if (!sea) return null;
  if (slot) return abroad(g, nat)?.quay ?? null;
  const you = sea.foe;
  const key = `st:quay:${nat}`, at = `st:quayAt:${nat}`, tk = `st:quayFor:${nat}`;
  const lod = sea.second ? lodgment(g, nat) : null;
  // (The quay is chosen for the harbours the staff may land on, this month or within the lead: a staff with
  // history's list is not sent to face a shore that is no part of it because that shore is clear. Britain's force
  // gathered opposite Norway, once German fighters covered France. Ruling 233(q).)
  const listed = sea.second ? harbours(g, you).filter((r) => opensOn(g, nat, r) || opensSoon(g, nat, r)) : [];
  const shores = lod?.short ? [lod.at] : listed.length ? listed : harbours(g, you);
  const shut = g.seas.map((z) => closedTo(g, z.id, nat));
  const D = OPEN.descent;
  // (The target the quay was chosen for: the enemy, and the countries of the list's shores it may land on or plan for.)
  const target = D.afresh ? 1 + (hashString(`${you}:${[...new Set(listed.map((r) => r.owner))].sort().join(',')}`) % 1000000) : 0;
  const room = (p: Region): boolean => !D.room || STACK - placed(g, p, nat).filter((u) => u.nat !== nat).length >= sea.L.force;
  const kept = g.regions[flag(g, key) - 1];
  const fresh = !!kept && kept.ctrl === nat && g.turn - flag(g, at) < 6 && (!target || flag(g, tk) === target);
  if (fresh && room(kept) && quayWorth(g, nat, kept, shores, shut).worth > 0) return kept;
  let best: Region | null = null, bw = 0, bs = Infinity;
  let full: Region | null = null, fw = 0, fs = Infinity;
  let near: Region | null = null, bd = Infinity;
  for (const id of homeGround(g, nat).list) {
    const r = g.regions[id];
    if (!r.port || r.owner !== nat || r.overseas) continue;
    // Seas between this quay and the nearest of his harbours, by the shortest way round, whoever holds them.
    const d = seaHops(g, r.seas, shores);
    if (d < bd) { bd = d; near = r; }
    const w = quayWorth(g, nat, r, shores, shut);
    if (!room(r)) { if (w.worth > fw + 1e-9 || (w.worth > 0 && Math.abs(w.worth - fw) <= 1e-9 && w.seas < fs)) { full = r; fw = w.worth; fs = w.seas; } continue; }
    if (w.worth > bw + 1e-9 || (w.worth > 0 && Math.abs(w.worth - bw) <= 1e-9 && w.seas < bs)) { best = r; bw = w.worth; bs = w.seas; }
  }
  // (No port with room serves: the full one that does, as before.)
  best ??= full;
  if (!best) {
    if (fresh) return kept;
    if (!near || bd > NAVY.landing.range) { delete g.flags[key]; delete g.flags[tk]; return null; }
    best = near;
  }
  g.flags[key] = best.id + 1;
  g.flags[at] = g.turn;
  if (target) g.flags[tk] = target;
  return best;
}

/**
 * The joint crossing (ruling 233(w); open.json `descent.joint`; computer staffs only): the corps of a staff's
 * computer-run allies that stand ready at its quay with no orders of their own from it, the strongest first. Their
 * own staff has named no crossing from that harbour, for its home army or its army abroad, and they are at war with
 * the enemy across the water. Where the staff's own corps at the quay are fewer than its force they fill it up and
 * sail in it (ai.ts `crossSea`). Never a player's corps, and never for a player's staff.
 */
export function quayGuests(g: Game, nat: string, quay: Region, you: string): Corps[] {
  if (!OPEN.descent.joint || g.flags.open !== 1 || g.human === nat) return [];
  const named = (id: string): boolean => ['', ':o'].some((sfx) => flag(g, `st:quay:${id}${sfx}`) - 1 === quay.id);
  return quay.units
    .filter((u) => u.nat !== nat && u.nat !== g.human && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne && !u.hold && !u.town && u.joint === undefined && u.leave === undefined
      && u.supplied && u.str >= 0.7 && u.ap >= u.apMax && !!g.nations[u.nat]?.alive && allied(g, nat, u.nat) && atWar(g, u.nat, you) && !named(u.nat))
    .sort((a, b) => b.divs * b.str - a.divs * a.str || a.id - b.id);
}

/**
 * The quay where a staff beyond the old chart keeps the force for its first business (Version 81, rulings 75 and
 * 76): an enemy whose home is on its own side of the ocean and whom its home army cannot reach by land. The force
 * is brought there by rail and is not shipped across the ocean while he stands. Null where it has no such enemy
 * within a landing's range of its harbours, and for the player's country.
 *
 * Every staff is asked this as it masses its army, and a staff's choice of an enemy across the sea is kept for a
 * year. So the question is put only to a staff that keeps a door and has such an enemy alive: nobody else is made
 * to choose here, a part of the month before its crossing is planned. (As first built the question chose for
 * Germany as for anyone, and one war in 24 with nobody playing moved: a German landing in Norway, seed 26.)
 */
export function doorQuay(g: Game, nat: string): Region | null {
  const n = g.nations[nat];
  if (!n || !keepsDoor(g, nat) || !n.atWar.some((e) => g.nations[e]?.alive && neighbourAcrossTheWater(g, nat, e))) return null;
  const sea = seaFoe(g, nat);
  if (!sea?.second || !neighbourAcrossTheWater(g, nat, sea.foe)) return null;
  // (The force its forecast asks for and no more: once its men are ashore and not short, the quay is an ordinary
  // harbour again and the rest of the army goes about history's business across the ocean. Ruling 80.)
  const lod = lodgment(g, nat);
  return lod && !lod.short ? null : seaBase(g, nat);
}

/**
 * The coast watch (the grand campaign). A staff at war with an enemy who has shipping, and whose home army has no
 * land front against it, expects him by sea once the two have not fought for a season (or his staff has named it
 * for a descent): from then, and for a year after the last such month, it keeps a corps in each harbour of its
 * homeland that his ships can reach within a landing's range (from the ports he holds, or an ally's where his
 * troops stand), the most valuable first, up to 'share' of its corps in the field and 'each' to a harbour. The
 * corps is one of the field army, of two divisions or more: a town's standing garrison is not counted (ai.ts). The coasts of the countries it has conquered are not watched: a landing
 * there is met by the reserves, as any other front is. Its treasury fortifies the watched harbours (arsenal.ts).
 * A watcher whose own seat stands on the far side of the world does not expect by sea an enemy already at grips with
 * it on its own home ground either, wherever his capital lies (`atGrips`); a watcher of the old chart or of the
 * Americas keeps the old test alone.
 */
export function coastWatch(g: Game, nat: string): Record<string, number> {
  const C = OPEN.coast, n = g.nations[nat], D = OPEN.descent;
  const out: Record<string, number> = {};
  if (!C || !D || !isOpen(g) || !n || !n.alive || n.capital < 0 || !n.atWar.length) return out;
  const home = homeGround(g, nat);
  const mine = home.list.map((id) => g.regions[id]).filter((r) => r.port && r.seas.length > 0 && r.owner === nat && !r.overseas);
  if (!mine.length) return out;
  // Whose ships: an enemy with sea lift whose army at home is not already at grips with this nation on land. They
  // sail from the ports he holds and from an ally's where his troops stand, a landing's range and no further, and
  // not through a strait that is shut to them.
  const ids = Object.keys(g.nations);
  const want = new Uint8Array(g.seas.length);
  let fresh = false, any = false;
  for (const e of [...n.atWar].sort()) {
    const x = g.nations[e];
    if (!x?.alive || x.dormant || x.sealift <= 0) continue;
    if (x.capital >= 0 && inTouch(g, e, nat, homeGround(g, e).on)) continue;
    // (The far side: an enemy whose army stands on this country's own home ground, in touch with the ground it still
    // holds, is at grips with it on land already and is not expected by sea: the design director's ruling of 04:07Z of
    // 21 November 2026. The old test asks only whether the enemy's army at home has a land front, so China read
    // Japan, whose home army is on its islands, as coming by sea alone, and stood its field armies in its harbours.
    // Kept to a watcher whose seat stands beyond the old chart, on the far side: Europe's watches stay as they were,
    // and the general form waits for after the release. The design director's 06:05Z of the same day.)
    if (farOn(g) && shoreOf(n.capital) === 2 && atGrips(g, e, nat)) { gripsWatch.tell?.({ turn: g.turn, nat, foe: e }); continue; }
    if (quietWith(g, e, nat) >= (D.abroad ?? 3) || SLOT.some((s) => ids[flag(g, `st:foe:${e}${s}`) - 1] === nat)) fresh = true;
    const d = new Int16Array(g.seas.length).fill(-1);
    const q: number[] = [];
    for (const r of g.regions) {
      if (!r.port || !r.seas.length) continue;
      if (r.ctrl !== e && !(allied(g, e, r.ctrl) && r.units.some((u) => u.nat === e && !u.dead && u.kind !== 'fort'))) continue;
      for (const z of r.seas) if (d[z] < 0) { d[z] = 1; q.push(z); }
    }
    for (let i = 0; i < q.length; i++) {
      want[q[i]] = 1;
      any = true;
      if (d[q[i]] >= NAVY.landing.range) continue;
      for (const j of g.seas[q[i]].adj) if (d[j] < 0 && canPass(g, q[i], j, e)) { d[j] = d[q[i]] + 1; q.push(j); }
    }
  }
  if (!any) return out;
  if (fresh) g.flags[`st:watch:${nat}`] = g.turn;
  else if (!flag(g, `st:watch:${nat}`) || g.turn - flag(g, `st:watch:${nat}`) >= 12) return out;
  const open = mine.filter((r) => r.seas.some((z) => want[z])).sort((a, b) => 2 * b.vp + b.fac + (b.cap ? 8 : 0) - (2 * a.vp + a.fac + (a.cap ? 8 : 0)) || a.id - b.id);
  let left = Math.max(C.least, Math.floor(C.share * g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng').length));
  for (let k = 1; k <= C.each && left > 0; k++) for (const r of open) { if (left <= 0) break; out[r.name] = k; left--; }
  return out;
}

/**
 * True where an enemy's field troops stand on this nation's home ground (its own provinces, not its ground overseas)
 * next to ground it still holds, or in a province of it that it holds: the two are at grips on its own soil.
 */
function atGrips(g: Game, e: string, nat: string): boolean {
  for (const r of g.regions) {
    if (r.owner !== nat || r.overseas || !r.units.some((u) => u.nat === e && !u.dead && u.kind !== 'fort')) continue;
    if (r.ctrl === nat || r.nb.some((j) => g.regions[j].ctrl === nat)) return true;
  }
  return false;
}

/** Told each time a coast watch passes over an enemy because he is at grips with the watcher on its home ground: for tests and readings. Nothing in the game hears it. */
export const gripsWatch: { tell: ((e: { turn: number; nat: string; foe: string }) => void) | null } = { tell: null };

/** The coast watch's numbers, for the treasury's fortifications (arsenal.ts). */
export const coastRules = (): typeof OPEN.coast => OPEN.coast;

/** How many seas lie between a coast and the nearest of some shores: 1 when they share a sea. */
function seaHops(g: Game, from: number[], shores: Region[]): number {
  const want = new Uint8Array(g.seas.length);
  for (const r of shores) for (const z of r.seas) want[z] = 1;
  const d = new Int16Array(g.seas.length).fill(-1);
  const q = [...from];
  for (const z of from) d[z] = 1;
  for (let i = 0; i < q.length; i++) {
    if (want[q[i]]) return d[q[i]];
    for (const j of g.seas[q[i]].adj) if (d[j] < 0) { d[j] = d[q[i]] + 1; q.push(j); }
  }
  return Infinity;
}

// ---- The army sent across the ocean (the wider world) ---------------------------------------------------
//
// A country whose homeland lies an ocean away from every enemy cannot land on him from its own ports: they are far
// outside a landing's range. Its staff ships the corps it raises to a port of an ally who is in the same war, a
// share of its shipping working that ocean each month (world.json, 'overseas'). From the far shore they go the way
// every second front goes: into a harbour of a landing its side has already made, or across the narrow sea when the
// way is open (`abroad`, above). It never lands on a defended shore straight from the ocean.

/** One ocean's run this month: the quay at home, and the harbours the corps can be put down in, the best first. */
export interface OceanRun {
  ocean: string; share: number; from: Region; to: Region[]; front: boolean;
  /** How many of `to`, from the first, are harbours of a front; and the harbours farther back on that front's ground (Version 83: for a corps the test of strength keeps out of the first). */
  ahead: number;
  rear: Region[];
}

/** The ground a nation's side holds in one piece with a province, by whose army holds the country. */
function groundFrom(g: Game, nat: string, at: Region): Uint8Array {
  const on = new Uint8Array(g.regions.length);
  const q = [at.id];
  on[at.id] = 1;
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (!on[j] && allied(g, nat, fieldCtrl(g, g.regions[j]))) { on[j] = 1; q.push(j); }
  return on;
}

/**
 * The second front a nation can join by ship: one it opened itself, or else one an ally who is in the same war
 * opened and is still fighting on (a player's own landing counts, turn.ts). Its own comes first, and of two the one
 * where the men ashore are still too few. With its harbours in friendly hands and the ground it stands on. After
 * those ('lodged' of the ports), and where there is no such landing at all, every harbour its side holds near the enemy (`heldFront`): a
 * harbour handed back to the country it was freed for, one reached overland, or an ally's own that the war has
 * come to. A staff with history's list of shores joins a front only on those (`joinsOn`). Null where there is none.
 */
export function frontAshore(g: Game, nat: string): { at: Region; ports: Region[]; ground: Uint8Array; lodged: number; rear: Region[] } | null {
  const n = g.nations[nat];
  if (!n?.alive) return null;
  let best: { at: Region; ports: Region[] } | null = null, bs = -1;
  const look = (id: string, slot: number, own: boolean): void => {
    const l = lodgment(g, id, slot);
    if (!l || !l.ports.length || (!own && !joinsOn(g, nat, l.at, id))) return;
    const s = (own ? 2 : 0) + (l.short ? 1 : 0);
    if (s > bs) { bs = s; best = { at: l.at, ports: l.ports }; }
  };
  for (const slot of [1, 0]) look(nat, slot, true);
  for (const id of Object.keys(g.nations)) {
    const A = g.nations[id];
    if (id === nat || !A.alive || A.dormant || !allied(g, nat, id) || !A.atWar.some((e) => n.atWar.includes(e))) continue;
    for (const slot of [0, 1]) look(id, slot, false);
  }
  const marches = OPEN.descent?.front;
  const held = marches !== undefined && isOpen(g) ? heldFront(g, nat, marches) : null;
  if (!best) return held;
  const b = best as { at: Region; ports: Region[] };
  const ground = groundFrom(g, nat, b.at);
  if (!held) return { ...b, ground, lodged: b.ports.length, rear: [] };
  for (let i = 0; i < ground.length; i++) if (held.ground[i]) ground[i] = 1;
  return { at: b.at, ports: [...b.ports, ...held.ports.filter((p) => !b.ports.includes(p))], ground, lodged: b.ports.length, rear: held.rear.filter((p) => !b.ports.includes(p)) };
}

/**
 * The harbours a nation's side holds near the enemy (open.json, 'descent.front'): a port in the hands of an ally who
 * is in the same war, or in its own, away from its own homeland, with no enemy in it and an enemy's ground within
 * that many marches of the quay over ground its side holds. The nearest to the enemy first, and the ground they
 * stand on. Null where there is none.
 */
function heldFront(g: Game, nat: string, marches: number): { at: Region; ports: Region[]; ground: Uint8Array; lodged: number; rear: Region[] } | null {
  const n = g.nations[nat];
  const home = homeGround(g, nat).on;
  const ours = (c: string): boolean => c === nat || (allied(g, nat, c) && !!g.nations[c]?.atWar.some((e) => n.atWar.includes(e)));
  const found: { p: Region; d: number }[] = [];
  const back: Region[] = [];
  for (const p of g.regions) {
    if (!p.port || !p.seas.length || home[p.id] || !ours(p.ctrl) || !joinsOn(g, nat, p) || hostileUnits(g, p, nat).length) continue;
    const d = new Map<number, number>([[p.id, 0]]);
    const q = [p.id];
    let near = Infinity;
    for (let i = 0; i < q.length && near === Infinity; i++) {
      const k = d.get(q[i])!;
      for (const j of g.regions[q[i]].nb) {
        if (d.has(j)) continue;
        const c = fieldCtrl(g, g.regions[j]);
        if (atWar(g, nat, c)) { near = k + 1; break; }
        if (k + 1 < marches && allied(g, nat, c)) { d.set(j, k + 1); q.push(j); }
      }
    }
    if (near <= marches) found.push({ p, d: near });
    else back.push(p);
  }
  if (!found.length) return null;
  found.sort((a, b) => a.d - b.d || a.p.id - b.p.id);
  const ground = new Uint8Array(g.regions.length);
  for (const f of found) {
    if (ground[f.p.id]) continue;
    const on = groundFrom(g, nat, f.p);
    for (let i = 0; i < on.length; i++) if (on[i]) ground[i] = 1;
  }
  // (Version 83, the test of strength: the harbours farther back on the same ground, the nearest to those first. A
  // corps the staff will not put into a harbour within the enemy's reach goes into one of these and marches up.)
  let rear: Region[] = [];
  if (STRENGTH && back.some((p) => ground[p.id])) {
    const d = new Int16Array(g.regions.length).fill(-1);
    const q = found.map((f) => f.p.id);
    for (const id of q) d[id] = 0;
    for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && ground[j]) { d[j] = d[q[i]] + 1; q.push(j); }
    rear = back.filter((p) => ground[p.id] && d[p.id] > 0).sort((a, b) => d[a.id] - d[b.id] || a.id - b.id);
  }
  return { at: found[0].p, ports: found.map((f) => f.p), ground, lodged: 0, rear };
}

/**
 * The harbour across an ocean where a staff builds its army up: a port of its own side's (its own, or an ally's who
 * is in the same war) that its ships can reach without too dangerous a crossing, with no enemy in it, and the one from which the most lightly held of
 * the enemy's harbours could then be reached over a narrow sea, as a quay for a second front is chosen. With none of
 * them in reach of any, the port nearest his harbours. The choice is kept for half a year while it still serves.
 */
function oceanBase(g: Game, nat: string, ocean: string, safe: (r: Region) => boolean): Region | null {
  const n = g.nations[nat];
  const key = `st:base:${nat}:${ocean}`, at = `st:baseAt:${nat}:${ocean}`;
  // (A staff that waits behind its allies builds its army up on its base's home ground and nowhere else.)
  // (At the stops where history's dates do not hold it waits there too, with no date: Version 83, ruling 89(e).)
  const P = waitsUndated(g, nat) ?? planned(g, nat);
  const waits = !!P?.behind && !!P.base && (dated(g) || !!waitsUndated(g, nat));
  // (Nor is an army built up in a harbour the enemy could take from it: the base passes the test of strength as it stands.)
  const strong = new Map<number, boolean>();
  const holds = (p: Region): boolean => { let s = strong.get(p.id); if (s === undefined) { s = holdsAshore(g, nat, p, []); strong.set(p.id, s); } return s; };
  const serves = (p: Region): boolean => p.port && p.seas.length > 0 && acrossOcean(g, ocean, p) && allied(g, nat, p.ctrl)
    && (p.ctrl === nat || !!g.nations[p.ctrl]?.atWar.some((e) => n.atWar.includes(e))) && !hostileUnits(g, p, nat).length && safe(p)
    && (!waits || based(P!, p)) && holds(p);
  const kept = g.regions[flag(g, key) - 1];
  if (kept && serves(kept) && g.turn - flag(g, at) < 6) return kept;
  // (The shores history planned for it, where it has such a list and the enemy holds any of them: otherwise all his harbours.)
  const all = [...n.atWar].sort().filter((e) => g.nations[e]?.alive && g.nations[e].capital >= 0).flatMap((e) => harbours(g, e));
  const meant = all.filter((r) => opensOn(g, nat, r) || joinsOn(g, nat, r));
  const shores = meant.length ? meant : all;
  if (!shores.length) { delete g.flags[key]; return null; }
  const shut = g.seas.map((z) => closedTo(g, z.id, nat));
  let best: Region | null = null, bw = 0, bs = Infinity;
  let near: Region | null = null, bd = Infinity;
  for (const p of g.regions) {
    if (!serves(p)) continue;
    const d = seaHops(g, p.seas, shores);
    if (d < bd) { bd = d; near = p; }
    const w = quayWorth(g, nat, p, shores, shut);
    if (w.worth > bw + 1e-9 || (w.worth > 0 && Math.abs(w.worth - bw) <= 1e-9 && w.seas < bs)) { best = p; bw = w.worth; bs = w.seas; }
  }
  best ??= near;
  if (!best) { delete g.flags[key]; return null; }
  g.flags[key] = best.id + 1;
  g.flags[at] = g.turn;
  return best;
}

/** The danger of the safest way by sea from a set of quays to each sea, as `crossingRisk` reckons it: 1 where no way is open. */
function dangerFrom(g: Game, nat: string, quays: Region[]): Float64Array {
  const N = g.seas.length;
  const risk = g.seas.map((z) => (closedTo(g, z.id, nat) ? -1 : seaRisk(g, z.id, nat)));
  const cost = new Float64Array(N).fill(1e9), safe = new Float64Array(N), done = new Uint8Array(N);
  for (const r of quays) for (const z of r.seas) if (risk[z] >= 0) { cost[z] = 1 + 20 * risk[z]; safe[z] = 1 - risk[z]; }
  for (;;) {
    let a = -1;
    for (let i = 0; i < N; i++) if (!done[i] && cost[i] < 1e9 && (a < 0 || cost[i] < cost[a])) a = i;
    if (a < 0) break;
    done[a] = 1;
    for (const j of g.seas[a].adj) {
      if (done[j] || risk[j] < 0 || !canPass(g, a, j, nat)) continue;
      const c = cost[a] + 1 + 20 * risk[j];
      if (c < cost[j]) { cost[j] = c; safe[j] = safe[a] * (1 - risk[j]); }
    }
  }
  return safe.map((s, z) => (cost[z] < 1e9 ? 1 - s : 1));
}

/**
 * This month's sailings for a country that ships its army across an ocean (world.ts, `overseasOf`): for each ocean
 * its shipping works, the home port with the shortest open way across, and where the corps are put down. A second
 * front its side is already fighting on comes first, into its harbours; then the harbour where the army is built
 * up, and the ports within three marches of it, for when those are full. No harbour is listed that the ships cannot
 * reach without too dangerous a crossing ('risk'), and there is no run for an ocean with none.
 */
export function oceanRuns(g: Game, nat: string): OceanRun[] {
  const O = overseasOf(g, nat);
  if (!O) return [];
  const quays = homeGround(g, nat).list.map((id) => g.regions[id]).filter((r) => r.port && r.seas.length > 0 && r.owner === nat);
  if (!quays.length) return [];
  const danger = dangerFrom(g, nat, quays);
  const safe = (r: Region): boolean => r.seas.some((z) => danger[z] <= O.risk);
  const front = frontAshore(g, nat);
  const out: OceanRun[] = [];
  for (const [ocean, share] of oceanShares(g, nat, O)) {
    // (An ocean whose share has moved to the other has no run this month: ruling 190.)
    if (share <= 0) continue;
    const to = front ? front.ports.filter((p) => acrossOcean(g, ocean, p) && safe(p)) : [];
    const atFront = to.length > 0, ahead = to.length;
    const rear = front ? front.rear.filter((p) => acrossOcean(g, ocean, p) && safe(p)) : [];
    const base = oceanBase(g, nat, ocean, safe);
    if (base) {
      // The ports within three marches of it over friendly ground, the nearest first.
      const d = new Int16Array(g.regions.length).fill(-1);
      const q = [base.id];
      d[base.id] = 0;
      for (let i = 0; i < q.length; i++) {
        if (d[q[i]] >= 3) continue;
        for (const j of g.regions[q[i]].nb) if (d[j] < 0 && allied(g, nat, g.regions[j].ctrl)) { d[j] = d[q[i]] + 1; q.push(j); }
      }
      // (A run of the Pacific's puts its army down only on its own lane and side of the equator, near its base too.)
      for (const id of q) {
        const p = g.regions[id];
        if (p.port && p.seas.length > 0 && (ocean === 'atlantic' || acrossOcean(g, ocean, p)) && !hostileUnits(g, p, nat).length && safe(p) && !to.includes(p)) to.push(p);
      }
    }
    if (!to.length) continue;
    let from: Region | null = null, len = Infinity;
    // (A run of the Pacific's sails from the first of its own quays with a way open: pacific.json, 'runs.from'.)
    const own = runQuays(g, ocean);
    if (own) from = own.find((r) => quays.includes(r) && !!seaPath(g, nat, r, to[0])) ?? null;
    else for (const r of quays) {
      const path = seaPath(g, nat, r, to[0]);
      if (path && (path.length < len || (path.length === len && from !== null && r.fac > from.fac))) { from = r; len = path.length; }
    }
    if (from) out.push({ ocean, share, from, to, front: atFront, ahead, rear: rear.filter((p) => !to.includes(p)) });
  }
  return out;
}

/** What follows a declaration the staff has just made: a neutral that marches of its own accord takes its side. */
export function afterDeclaring(g: Game, nat: string): void {
  const list = ops(nat);
  const op = flag(g, `st:go:${nat}`) === g.turn ? list[flag(g, `st:goOp:${nat}`) - 1] : null;
  const n = g.nations[nat];
  // The op's own news follows its declaration in the same line, so that the month card tells one story (ruling 228, the UI director's words).
  if (op?.news) for (const e of op.declare ?? []) {
    if (g.flags[`war:${nat}:${e}`] !== g.turn || !g.nations[e]) continue;
    const said = `${theName(n, true)} declares war on ${theName(g.nations[e])}.`;
    const h = [...g.headlines].reverse().find((x) => x.turn === g.turn && x.kind === 'war' && x.nat === nat && x.text === said);
    if (h) h.text = `${h.text} ${op.news}`;
  }
  // In the last act there are no sides left to take.
  if (dominating(g)) return;
  if (!op?.join || !n.alive || n.align === 'allies' || n.align === 'axis') return;
  const others = n.atWar.some((e) => { const b = g.nations[e].align; return op.join === 'allies' ? b === 'axis' || b === 'axis_cobelligerent' : b === 'allies' || b === 'allies_cobelligerent'; });
  if (!others) return;
  n.align = op.join === 'allies' ? 'allies_cobelligerent' : 'axis_cobelligerent';
  n.opinion = n.home = op.join === 'allies' ? -100 : 100;
  g.supplyDirty = true;
  joinedSide(g, nat);
}

/** This month's orders for a computer-run nation's army in a theatre overseas that keeps a staff of its own. */
export function theatreStaff(g: Game, nat: string, scope: Set<number>, k: number): PlanPhase | null {
  const n = g.nations[nat];
  if (!n.alive || !n.atWar.length) return null;
  const T = temper(g, nat);
  const where = `:t${k}`;
  const real = fronts(g, nat, { scope, busy: (f) => onSince(g, nat, f, where) > 0 });
  const stalled = g.turn - flag(g, `st:gain:${nat}${where}`) >= OPEN.stall;
  const held = [...scope].reduce((s, id) => s + (g.regions[id].ctrl === nat ? 1 : 0), 0);
  if (flag(g, `st:nAt:${nat}${where}`) !== g.turn) {
    if (held > flag(g, `st:n:${nat}${where}`)) g.flags[`st:gain:${nat}${where}`] = g.turn;
    g.flags[`st:n:${nat}${where}`] = held;
    g.flags[`st:nAt:${nat}${where}`] = g.turn;
    for (const f of real) {
      const since = onSince(g, nat, f, where);
      if (since) {
        if (g.turn - since >= 2 && inNameOnly(g, nat, f, since)) calledOffIdle(g, nat, f, where);
        else if (g.turn - since < 2 || (f.ratio >= T.persist && (!stalled || f.ratio >= T.attackAt))) setOn(g, nat, f, since, where);
        else { setOn(g, nat, f, 0, where); for (const e of f.enemies) g.flags[`st:rest:${nat}:${e}${where}`] = g.turn + OPEN.rest; }
      } else if (f.ratio >= T.attackAt && !shy(g, nat, f, where) && f.enemies.every((e) => g.turn >= flag(g, `st:rest:${nat}:${e}${where}`))) {
        setOn(g, nat, f, g.turn, where);
        g.flags[`st:gain:${nat}${where}`] = g.turn;
      }
    }
    // A theatre that has gone quiet may be given a main effort of its own.
    mainEffort(g, nat, real, T, where, () => false);
  }
  const live = real.filter((f) => onSince(g, nat, f, where) > 0);
  if (!live.length) {
    const eff = effortOf(g, nat, where);
    const orders = eff ? effortOrders(g, nat, eff, real, T) : null;
    return { name: '', from: g.turn, to: g.turn, posture: 'defend', ...(orders ?? {}) } as PlanPhase;
  }
  const press = pressing(g, nat, live[0], T.minOdds, where);
  return {
    name: `The offensive against ${g.nations[live[0].key]?.name ?? live[0].key}`, from: onSince(g, nat, live[0], where), to: g.turn, posture: 'attack',
    enemies: [...new Set(live.flatMap((f) => f.enemies))], objectives: [...new Set(live.flatMap((f) => objectives(g, f)))], minOdds: press?.minOdds ?? T.minOdds, ...(press ? { waves: press.waves } : {}),
    targets: [...new Set(live.flatMap((f) => bound(g, nat, f, [], false, OPEN.boundOverseas)))],
    release: [...new Set(live.flatMap((f) => f.ours.map((id) => g.regions[id].name)))],
  } as PlanPhase;
}

// ---- A war nobody can fight (the grand campaign) --------------------------------------------------------
//
// A war that has been won on land does not always end: the victor holds the continent, the loser's allies sit
// behind the sea, and neither can come at the other. When two computer nations at war have no ground in touch
// anywhere, with each other or with anyone on the other's side they are fighting, and their two sides have fought
// no battle on land for a year, they sign an armistice on the lines as they stand. (Not while a staff on either side
// is still building its army up across the sea for the front history planned: Version 77.) Never a war the player is
// fighting in, and never in the last act, where every country fights for itself to the end.

/** The nations on one's side of the war: its bloc, the fallen with the standing. A nation outside both blocs stands alone. */
function sideOf(g: Game, nat: string): string[] {
  const b = bloc(g.nations[nat]);
  return b === 'axis' || b === 'allies' ? Object.keys(g.nations).filter((x) => bloc(g.nations[x]) === b) : [nat];
}

/** The month's armistices between nations that can no longer come at each other. Returns the pairs that signed. */
export function stalemates(g: Game): [string, string][] {
  const S = RIVALS.stalemate;
  if (!rivalRules(g) || !S || dominating(g)) return [];
  const me = g.human !== null && g.nations[g.human]?.alive ? g.human : null;
  const live = Object.values(g.nations).filter((n) => n.alive && !n.dormant && n.atWar.length);
  if (live.length < 2) return [];
  // Where each nation stands in the field: the ground its army holds, and wherever its corps are.
  const N = g.regions.length;
  const at = new Map<string, Uint8Array>();
  const mark = (id: string, r: number): void => { let a = at.get(id); if (!a) at.set(id, a = new Uint8Array(N)); a[r] = 1; };
  for (const r of g.regions) { mark(fieldCtrl(g, r), r.id); for (const u of r.units) if (!u.dead) mark(u.nat, r.id); }
  const touch = (a: string, b: string): boolean => {
    const A = at.get(a), B = at.get(b);
    if (!A || !B) return false;
    for (let i = 0; i < N; i++) if (A[i] && (B[i] || g.regions[i].nb.some((j) => B[j]))) return true;
    return false;
  };
  const lastBattle = (xs: string[], ys: string[]): number => {
    let t = 0;
    for (const x of xs) for (const y of ys) t = Math.max(t, g.nations[x].lastFight[y] ?? 0, g.nations[y].lastFight[x] ?? 0);
    return t;
  };
  // Neither can come at the other, nor at anyone on the other's side that it is fighting.
  const free = (x: string, side: string[]): boolean => !side.some((y) => g.nations[y].alive && atWar(g, x, y) && touch(x, y));
  // A staff still building its army up across the sea for the front history planned will come at the enemy: its
  // side's war is not one nobody can fight, and the year is counted from the month that army is ready.
  for (const n of live) if (massing(g, n.id)) g.flags[`st:massing:${n.id}`] = g.turn;
  const massed = (xs: string[], ys: string[]): number => {
    let t = 0;
    for (const x of xs) if (g.nations[x].alive && ys.some((y) => atWar(g, x, y))) t = Math.max(t, flag(g, `st:massing:${x}`));
    return t;
  };
  const signed: [string, string][] = [];
  for (const A of live) for (const b of A.atWar) {
    const B = g.nations[b];
    if (!(A.id < b) || !B?.alive || B.dormant) continue;
    if (me !== null && (A.id === me || b === me || atWar(g, me, A.id) || atWar(g, me, b))) continue;
    if (tieWar(g, A.id, b)) continue; // the far side: a client makes no separate peace while its patron fights on (far.ts)
    const sa = sideOf(g, A.id), sb = sideOf(g, b);
    const since = Math.max(lastBattle(sa, sb), massed(sa, sb), massed(sb, sa), flag(g, `war:${A.id}:${b}`), flag(g, `war:${b}:${A.id}`), 1);
    if (g.turn - since < S.after || !free(A.id, sb) || !free(b, sa)) continue;
    signed.push([A.id, b]);
  }
  for (const [a, b] of signed) {
    const A = g.nations[a], B = g.nations[b];
    A.atWar = A.atWar.filter((x) => x !== b);
    B.atWar = B.atWar.filter((x) => x !== a);
    g.flags[`arm:${a}:${b}`] = g.turn;
    headline(g, 'event', `${A.name} and ${B.name} sign an armistice. Neither could come at the other, and their armies had not met for a year. Each keeps what it holds.`, a);
  }
  return signed;
}
