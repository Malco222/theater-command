// How far the grand campaign's chart reaches (data/campaign/stage.json).
//
// The far side of the world (the Pacific and the globe, step 13 and the globe's stage) is built on its own branch
// while the versions before it ship. Everything of it is keyed on this one constant, read as the modules load: the
// chart files the game reads, and the rows of the countries that live out there. While it is false the game is,
// byte for byte, the game without them.
//
// A test of the far side turns it on for its own file, before anything of the game is imported:
//
//   vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
//
// (Vitest gives every test file its own copy of the modules, so one run of the suite plays the older tests on the
// Americas and the far side's on the whole chart.) The browser page can be opened on the far chart the same way, by
// setting TC_FAR before the bundle loads. This file imports nothing but the switch, so any module may read it first.

import stage from '../../data/campaign/stage.json';

// A run outside the browser can be given the switch by its environment too: TC_FAR=1 node ... (the hash checks, a seat's trial).
const G = globalThis as { TC_FAR?: unknown; process?: { env?: Record<string, string | undefined> } };
const forced = G.TC_FAR ?? G.process?.env?.TC_FAR;

/** True where the grand campaign plays on the whole world: the far side's chart is read and its countries exist. */
export const FAR_STAGE: boolean = forced !== undefined ? !!forced && forced !== '0' : (stage as { far: boolean }).far === true;

// The wolf pack and convoy battle (release 2, ruling 241): the grand campaigns opened while this is true carry the flag
// 'convoyBattle' (navy.ts convoyRules). A run outside the browser can be given it by its environment: TC_CONVOY=1 node ...
const C = globalThis as { TC_CONVOY?: unknown; process?: { env?: Record<string, string | undefined> } };
const convoyForced = C.TC_CONVOY ?? C.process?.env?.TC_CONVOY;

/** True where new grand campaigns open with the convoy battle (data/campaign/stage.json 'convoyBattle'). */
export const CONVOY_STAGE: boolean = convoyForced !== undefined ? !!convoyForced && convoyForced !== '0' : (stage as { convoyBattle?: boolean }).convoyBattle === true;

// The month's submarine scene (release 2, ruling 241 D1) is a viewing setting, kept outside the rules' save: the
// screen sets navy.ts subScene. A run outside the browser can start with it on: TC_SUBSCENE=1 node ...
const subForced = C.process?.env?.TC_SUBSCENE ?? (globalThis as { TC_SUBSCENE?: unknown }).TC_SUBSCENE;

/** True where a run outside the browser starts with the submarine scene's record on. */
export const SUBSCENE_ENV: boolean = subForced !== undefined && !!subForced && subForced !== '0';

// The fortress rows (data/campaign/stage.json 'fortRows'): forced by TC_FORTS, as the far side's switch is by TC_FAR.
const forts = (globalThis as { TC_FORTS?: unknown }).TC_FORTS ?? G.process?.env?.TC_FORTS;
/** True where a grand campaign opens with the fortress rows (the flag is set as it opens: game.ts fortRows). */
export const FORTS_STAGE: boolean = forts !== undefined ? !!forts && forts !== '0' : (stage as { fortRows?: boolean }).fortRows === true;

// The yield form (data/campaign/stage.json 'yieldRule'; the design director's 07:20Z of 22 November 2026): forced by
// TC_YIELD, as the far side's switch is by TC_FAR. Off, nothing of the form is read.
const yields = (globalThis as { TC_YIELD?: unknown }).TC_YIELD ?? G.process?.env?.TC_YIELD;
/** True where a beaten country yields by cause, and at stops 1-2 by the record's month (src/campaign/yield.ts). */
export const YIELD_STAGE: boolean = yields !== undefined ? !!yields && yields !== '0' : (stage as { yieldRule?: boolean }).yieldRule === true;

// A neutral's bargains (release 2): the grand campaigns opened while this is true carry the flag 'bargains'
// (bargains.ts). A run outside the browser can be given it by its environment: TC_BARGAINS=1 node ...
const B = globalThis as { TC_BARGAINS?: unknown; TC_LEGIONS?: unknown; process?: { env?: Record<string, string | undefined> } };
const bargainsForced = B.TC_BARGAINS ?? B.process?.env?.TC_BARGAINS;

/** True where new grand campaigns open with a neutral's bargains (data/campaign/stage.json 'bargains'). */
export const BARGAINS_STAGE: boolean = bargainsForced !== undefined ? !!bargainsForced && bargainsForced !== '0' : (stage as { bargains?: boolean }).bargains === true;

// Volunteer legions (release 2): the grand campaigns opened while this is true carry the flag 'legions' (legions.ts).
// A run outside the browser can be given it by its environment: TC_LEGIONS=1 node ...
const legionsForced = B.TC_LEGIONS ?? B.process?.env?.TC_LEGIONS;

/** True where new grand campaigns open with volunteer legions (data/campaign/stage.json 'legions'). */
export const LEGIONS_STAGE: boolean = legionsForced !== undefined ? !!legionsForced && legionsForced !== '0' : (stage as { legions?: boolean }).legions === true;

// The honest win chance on land (release 2, auto-resolve fix 1): the grand campaigns opened while this is true carry the
// flag 'winChance' (game.ts luckOf; data/campaign/winchance.json). A run outside the browser can be given it by its
// environment: TC_WINCHANCE=1 node ...
const winForced = (globalThis as { TC_WINCHANCE?: unknown }).TC_WINCHANCE ?? G.process?.env?.TC_WINCHANCE;

/** True where new grand campaigns open with the honest win chance on land (data/campaign/stage.json 'winChance'). */
export const WINCHANCE_STAGE: boolean = winForced !== undefined ? !!winForced && winForced !== '0' : (stage as { winChance?: boolean }).winChance === true;

// True reach on the old chart (audit item 41; game.ts trueReach): the grand campaigns opened while this and Units decide
// are true carry the flag 'trueReach'. A run outside the browser can be given it by its environment:
// TC_UNITS=1 TC_TRUEREACH=1 node ... (On the far side the rule needs no flag: it is on with the far switch.)
const reachForced = (globalThis as { TC_TRUEREACH?: unknown }).TC_TRUEREACH ?? G.process?.env?.TC_TRUEREACH;
/** True where new grand campaigns with Units decide open with true reach (data/campaign/stage.json 'trueReach'). */
export const TRUEREACH_STAGE: boolean = reachForced !== undefined ? !!reachForced && reachForced !== '0' : (stage as { trueReach?: boolean }).trueReach === true;

// Units decide (release 2; src/campaign/units.ts): the grand campaigns opened while this is true carry the flag
// 'unitsDecide'. Forced by TC_UNITS, as the far side's switch is by TC_FAR.
const units = (globalThis as { TC_UNITS?: unknown }).TC_UNITS ?? G.process?.env?.TC_UNITS;
/** True where new grand campaigns open with Units decide (data/campaign/stage.json 'unitsDecide'). */
export const UNITS_STAGE: boolean = units !== undefined ? !!units && units !== '0' : (stage as { unitsDecide?: boolean }).unitsDecide === true;

// Earned nicknames and supply columns (release 2; Brian's yes of 23 November 2026 12:05Z): the grand campaigns opened
// while these are true carry the flags 'nicknames' (nicknames.ts) and 'columns' (columns.ts). Forced by TC_NICKNAMES
// and TC_COLUMNS, as the far side's switch is by TC_FAR.
const nicks = (globalThis as { TC_NICKNAMES?: unknown }).TC_NICKNAMES ?? G.process?.env?.TC_NICKNAMES;
/** True where new grand campaigns open with earned nicknames (data/campaign/stage.json 'nicknames'). */
export const NICKNAMES_STAGE: boolean = nicks !== undefined ? !!nicks && nicks !== '0' : (stage as { nicknames?: boolean }).nicknames === true;
const cols = (globalThis as { TC_COLUMNS?: unknown }).TC_COLUMNS ?? G.process?.env?.TC_COLUMNS;
/** True where new grand campaigns open with supply columns (data/campaign/stage.json 'columns'). */
export const COLUMNS_STAGE: boolean = cols !== undefined ? !!cols && cols !== '0' : (stage as { columns?: boolean }).columns === true;

// The three supply orders on top of the columns (release 2; Brian's yes of 23 November 2026 13:06Z, logistics.ts): the
// grand campaigns opened while these are true carry the flags 'lorryShift', 'wagons' and 'dumps'. Each does nothing
// unless the columns run too. Forced by TC_LORRYSHIFT, TC_WAGONS and TC_DUMPS.
const forcedBy = (name: string): unknown => (globalThis as Record<string, unknown>)[name] ?? G.process?.env?.[name];
const staged = (env: string, key: string): boolean => {
  const v = forcedBy(env);
  return v !== undefined ? !!v && v !== '0' : (stage as Record<string, unknown>)[key] === true;
};
/** True where new grand campaigns open with the marked army (release 2, marked.ts; stage.json 'marked'). Forced by TC_MARKED. */
export const MARKED_STAGE: boolean = staged('TC_MARKED', 'marked');
// A row that takes a named corps whole waits while that corps is in contact, on the old chart (release 2; the design
// director's ruling of 19:47Z, 25 November 2026; game.ts `expeditions`): the grand campaigns opened while this is true
// carry the flag 'fromWait'. Forced by TC_FROMWAIT.
/** True where new grand campaigns open with the old chart's "from" rows waiting on contact (stage.json 'fromWait'). */
export const FROMWAIT_STAGE: boolean = staged('TC_FROMWAIT', 'fromWait');
/** True where new grand campaigns open with the lorries moved to the front named first (stage.json 'lorryShift'). */
export const LORRYSHIFT_STAGE: boolean = staged('TC_LORRYSHIFT', 'lorryShift');
/** True where new grand campaigns open with horse-drawn wagons where lorries are short (stage.json 'wagons'). */
export const WAGONS_STAGE: boolean = staged('TC_WAGONS', 'wagons');
/** True where new grand campaigns open with forward dumps (stage.json 'dumps'). */
export const DUMPS_STAGE: boolean = staged('TC_DUMPS', 'dumps');

// A fleet sent straight at an enemy fleet (release 2; Brian 23 November 2026 22:23Z, fleetattack.ts): the grand
// campaigns opened while this is true carry the flag 'fleetAttack'. Forced by TC_FLEETATTACK.
/** True where new grand campaigns open with the player's fleets able to attack an enemy fleet at once (stage.json 'fleetAttack'). */
export const FLEETATTACK_STAGE: boolean = staged('TC_FLEETATTACK', 'fleetAttack');

// Breaking a corps up (release 2; Brian 24 November 2026 18:02Z, breakup.ts): the grand campaigns opened while this
// is true carry the flag 'breakUp'. Forced by TC_BREAKUP.
/** True where new grand campaigns open with the player able to split divisions off a corps (stage.json 'breakUp'). */
export const BREAKUP_STAGE: boolean = staged('TC_BREAKUP', 'breakUp');

// Task forces (release 2; Brian 24 November 2026 18:24Z, taskforce.ts): the grand campaigns opened while this is true
// carry the flag 'taskForce'. Forced by TC_TASKFORCE.
/** True where new grand campaigns open with the player able to make task forces of his fleets (stage.json 'taskForce'). */
export const TASKFORCE_STAGE: boolean = staged('TC_TASKFORCE', 'taskForce');

// The admirals' reasons (release 2; fleetwhy.ts): the grand campaigns opened while this is true carry the flag
// 'fleetWhy'. Forced by TC_FLEETWHY.
/** True where new grand campaigns open with the admirals giving a reason for each fleet they order (stage.json 'fleetWhy'). */
export const FLEETWHY_STAGE: boolean = staged('TC_FLEETWHY', 'fleetWhy');

// Rearguards (release 2; Brian 24 November 2026 18:24Z, rearguard.ts): the grand campaigns opened while this is true
// carry the flag 'rearguard'. Forced by TC_REARGUARD.
/** True where new grand campaigns open with the player able to order rearguards (stage.json 'rearguard'). */
export const REARGUARD_STAGE: boolean = staged('TC_REARGUARD', 'rearguard');

// Conquered towns settle (release 2; Brian 24 November 2026 18:13Z, buildings.ts): the grand campaigns opened while
// this is true carry the flag 'settledTowns'. Forced by TC_SETTLEDTOWNS.
/** True where new grand campaigns open with conquered towns able to build everything once settled (stage.json 'settledTowns'). */
export const SETTLEDTOWNS_STAGE: boolean = staged('TC_SETTLEDTOWNS', 'settledTowns');

// Shadowing, the voyage log, and rearguards that blow the bridges (release 2; Brian 24 November 2026 19:47Z, "yes to
// all"; shadow.ts, voyage.ts, rearguard.ts): the grand campaigns opened while each is true carry its flag. Forced by
// TC_SHADOW, TC_VOYAGELOG and TC_REARBRIDGES.
/** True where new grand campaigns open with fleets able to shadow an enemy fleet (stage.json 'shadow'). */
export const SHADOW_STAGE: boolean = staged('TC_SHADOW', 'shadow');
/** True where new grand campaigns open with each fleet keeping a voyage log (stage.json 'voyageLog'). */
export const VOYAGELOG_STAGE: boolean = staged('TC_VOYAGELOG', 'voyageLog');
/** True where new grand campaigns open with rearguards blowing the bridges behind them (stage.json 'rearBridges'). */
export const REARBRIDGES_STAGE: boolean = staged('TC_REARBRIDGES', 'rearBridges');

// Mines at sea, heavy seas and the shadow relay (release 2; Brian 24 November 2026 20:38Z, "yes to all"; mines.ts,
// heavyseas.ts, shadow.ts): the grand campaigns opened while each is true carry its flag. Forced by TC_SEAMINES,
// TC_HEAVYSEAS and TC_SHADOWRELAY.
/** True where new grand campaigns open with fleets able to lay mines at sea (stage.json 'seaMines'). */
export const SEAMINES_STAGE: boolean = staged('TC_SEAMINES', 'seaMines');
/** True where new grand campaigns open with gales at sea (stage.json 'heavySeas'). */
export const HEAVYSEAS_STAGE: boolean = staged('TC_HEAVYSEAS', 'heavySeas');
/** True where new grand campaigns open with the shadow relay (stage.json 'shadowRelay'). */
export const SHADOWRELAY_STAGE: boolean = staged('TC_SHADOWRELAY', 'shadowRelay');

// A fleet seeks battle once a month (release 2; the design director's ruling of 25 November 2026, 02:38Z; navy.ts):
// the grand campaigns opened while it is true carry the flag 'seekOnce'. Forced by TC_SEEKONCE.
/** True where new grand campaigns open with each fleet seeking battle at most once a month (stage.json 'seekOnce'). */
export const SEEKONCE_STAGE: boolean = staged('TC_SEEKONCE', 'seekOnce');

// A queue for a town (release 2; Brian 25 November 2026 13:41Z; the design director's ruling seamless-r2-1125.md part
// B; townqueue.ts): the grand campaigns opened while it is true carry the flag 'townQueue'. Forced by TC_TOWNQUEUE.
/** True where new grand campaigns open with building queues for the player's towns (stage.json 'townQueue'). */
export const TOWNQUEUE_STAGE: boolean = staged('TC_TOWNQUEUE', 'townQueue');
// The atomic project (release 2; Brian 25 November 2026 01:14Z, "lets do number 1"; the design director's ruling of
// 01:27Z, design-director/atom-1125.md; atom.ts, data/campaign/atom.json): the grand campaigns opened while it is true
// carry the flag 'atom'. Forced by TC_ATOM.
/** True where new grand campaigns open with the atomic project (stage.json 'atomBomb'). */
export const ATOM_STAGE: boolean = staged('TC_ATOM', 'atomBomb');
// Carrier strikes on harbours by the player's own hand (release 2; Brian 24 November 2026 21:45Z, "Resume the Pacific
// for release 2"; harbourstrike.ts): the grand campaigns opened while it is true carry its flag. Forced by TC_CARRIERSTRIKE.
/** True where new grand campaigns open with the carrier strike on a harbour on a carrier fleet's page (stage.json 'carrierStrike'). */
export const CARRIERSTRIKE_STAGE: boolean = staged('TC_CARRIERSTRIKE', 'carrierStrike');

// Carrier wings by date (release 2; the rider to rulings 189 and 202; carriers.ts): the grand campaigns opened while it
// is true carry its flag. Forced by TC_WINGBYDATE.
/** True where new grand campaigns open with a navy's carrier wing by date (stage.json 'wingByDate'). */
export const WINGBYDATE_STAGE: boolean = staged('TC_WINGBYDATE', 'wingByDate');
// Moving an air group to any airfield of its side's with one order (release 2; Brian 24 November 2026 22:08Z; ferry.ts):
// the grand campaigns opened while this is true carry the flag 'ferry'. Forced by TC_FERRY.
/** True where new grand campaigns open with air groups able to go to any friendly airfield by stages (stage.json 'ferry'). */
export const FERRY_STAGE: boolean = staged('TC_FERRY', 'ferry');
// Better air unit names (release 2; the design director's ruling of 25 November 2026 03:18Z): the grand campaigns opened
// while this is true carry the flag 'airNames' (air.ts groupName). Words only. Forced by TC_AIRNAMES.
/** True where new grand campaigns open with the air arms' own numbers and names (stage.json 'airNames'). */
export const AIRNAMES_STAGE: boolean = staged('TC_AIRNAMES', 'airNames');
