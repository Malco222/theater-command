// A port cut off by sea tells of its stores (release 2; the UI director's words of 26 November 2026, 18:46Z,
// ui-director/port-stores-shut-sea-UI-MARKS.md, after the design director's rulings of 18:44Z). Behind its own switch
// (stage.json 'portStores', the flag 'portStores', the Options key 'portstores'): off, nothing here is written, read or
// shown, and every war is the war without it. Words only, on as off: nothing here changes the war.
//
// A fortified port overseas keeps stores (game.ts portKeep): with the sea behind it shut, the supply picture feeds its
// garrison from them for a while (computeSupply's window), and then the garrison goes hungry and is struck off (game.ts
// startPhase). Until now nothing said so: Malta's corps starved with no warning (rules-builder/release2/true-reach,
// gate96-1126 READ, "Malta"). One function, portCutOff, reads the facts from the rules' own clock, so that every month
// it names is the month the rule acts; the port's card, the news and the corps card all call it, and nobody copies it.
// A port in its holder's pocket is left to the pocket's own lines, as a besieged port is left to the siege's (portOfKind).
//
// Its own flags (numbers only, saved with the game, written only with the switch on and a player at the table):
//   portTold:<id>     the month the first cut-off news of the port was told to the player: the relief is told only after
//                     it ("Relief, after the first line was told"). Cleared at the relief, when the port changes hands,
//                     is left to a siege's lines or its holder's pocket's (watched), or the player has no part in it any
//                     more, when the switch is turned off, and when its holder or the player falls (portsDone).
//   seaShut:<z>:<nat> the month sea z became shut to nation nat, read at every nation's month as it opens: the start of
//                     the hold the shutting side sees on the port's card ("held ... since"). -1 (WATCHED_OPEN) while
//                     the switch watches the sea open to it; the month it became shut only where the switch watched it
//                     open before; 0 where the sea was already shut when the switch first watched it (its first look,
//                     as it is turned on or at the first month it reads the seas; a port newly of the kind; a nation
//                     newly holding one): a hold whose start the switch did not see, which gives no month (the UI
//                     director's 23:50Z, item 5). Kept for every nation but the player that holds a port of this kind
//                     on that sea (in its own pocket or not: keepsStores), whatever its wars and the player's, so that a
//                     hold begun before the player's war with the holder is seen to begin. Cleared when no port of the
//                     kind asks for it, when the switch is turned off, when the nation falls, and when the player falls
//                     (so that a player given his seat back is watched afresh).
//   portLost:<id>:<corps>:<nat>:<name>
//                     the month a corps of the port's garrison starved there, kept from its nation's reckoning to the
//                     player's own, so that the corps a port loses in one month are told in one line ("Several"),
//                     whichever nations' corps they were. Only where the player hears of the port. Cleared as the line is
//                     told, when the switch is turned off, and when the player falls. (The corps' name is in the key: a
//                     corps struck off leaves the roster, and a flag holds only a number.)
//   portHad:<id>      the month the player's corps stood in the port as his month opened: a nation with corps there as the
//                     month opened hears of the corps the port loses that month (ruling 2). Set afresh at each of his
//                     months; cleared when the switch is turned off and when the player falls.
//   portCrew:<id>:<corps>:<nat>:<months>:<raised>[:<seen>]
//                     a fortress crew of the port's side lost there (the fortress rows), whatever took it: starved,
//                     beaten in battle, ordered out, gone with its country (the UI director's 01:14Z, item 3, and 03:13Z,
//                     item 2). The month of the last draw of the supply picture that saw it living, with its months of
//                     stores, its nation and the month it was raised in the key, and the nation whose draw that was where
//                     it is not the crew's own (`seen`; a crew that starves goes in its own nation's reckoning, after its
//                     own draw): the rule reads the stores of the crews living at each draw (game.ts portKeep), so that
//                     the months it fed the garrison on the crew's stores are still counted once the crew is gone, and
//                     the draws after it are counted without it. Written in any port of the kind (in its holder's pocket
//                     or not: keepsStores) as the crew is struck off (game.ts removeCorps). Cleared once the port's stores
//                     are filled again, it changes hands or it is no longer of the kind, when the switch is turned off,
//                     when its holder falls, and when the player falls.
// None begins with "stores:", so a load never touches them (save.ts keeps the supply picture's own marks by that prefix).
import { turnToDate } from '../core/ticket';
import { AIR } from './air';
import { theUnit } from './bombers';
import { seaFigure, seaMeasure } from './carriers';
import { hospitalNear, hospitalWorth } from './buildings';
import { columnShare } from './columns';
import { cmdRally, cmdWinter } from './commanders';
import { BUDGET, budgeted } from './economy';
import { DESERT, EASTERN, UNFED, allied, atWar, enemyAshore, fortRow, fortRows, frozen, headline, keptStoresHis, lineStrength, placeName, portKeep, theWords, trueReach, worksKept } from './game';
import { crewStores, storesEaten } from './fortrows';
import { SIGHT, sightOf } from './intel';
import { MORALE, moraleRules } from './morale';
import { NAVY, SeaPicture, canPass, closedTo, fleetReach, inPassage, openSeas, seaSide, seaStarts, shipCount } from './navy';
import { skiesOver } from './pacific';
import { seaWhat } from './sea-words';
import { fieldCtrl, pocketNat, siegeRules } from './siege';
import { witherAt } from './wither';
import { farOn } from './world';
import type { Corps, Game, Region } from './types';

// ---- The Options row and the switch ----------------------------------------------------------------------------------

/**
 * The Options row's name and line (the UI director's words of 18:46Z, section 4). For a screen that lists the release 2
 * switches; the 3D's own row carries the same words (CampaignPlayerController.cpp). The browser has no such screen: a
 * grand campaign there takes the switch from stage.json as it opens, or from the page's address (?portstores).
 */
export const PORTSTORES_ROW = {
  id: 'portstores',
  name: 'Ports cut off by sea tell you of their stores',
  line: 'When the sea round one of your ports is shut, its news, its card and the cards of the corps there say whose ships or aircraft shut it, the month its stores run out, the month its garrison would be lost, and what opens the sea again. The side that shuts it sees on the port\'s card how long it has held that sea. Off: none of this is said, as before. Words only: no result changes.',
} as const;

/** The switch as everything here reads it: a grand campaign with the flag 'portStores'. */
export const portStoresOn = (g: Game): boolean => g.flags.open === 1 && g.flags.portStores === 1;

const toldKey = (id: number): string => `portTold:${id}`;
const shutKey = (z: number, nat: string): string => `seaShut:${z}:${nat}`;
const lostKey = (id: number, c: Corps): string => `portLost:${id}:${c.id}:${c.nat}:${c.name}`;
const hadKey = (id: number): string => `portHad:${id}`;
const crewKey = (id: number, c: Corps, months: number, seen: string): string => `portCrew:${id}:${c.id}:${c.nat}:${months}:${c.born ?? 1}${seen === c.nat ? '' : `:${seen}`}`;
/**
 * The last draw of the supply picture whose reckoning reads it, per game: the month and the nation whose month it
 * opened (portsMonth reads straight after each nation's draw, game.ts startPhase). Not saved: a game loaded, or one whose
 * telling has just been switched on, is in a nation's month after its draw (lastDraw). Let go whenever a reading is
 * missed, so that it never names a draw older than the last.
 */
const reading = new WeakMap<Game, { turn: number; nat: string }>();
const lastDraw = (g: Game): { turn: number; nat: string } => reading.get(g) ?? { turn: g.turn, nat: g.order[Math.min(g.phase, g.order.length - 1)] };
/** A sea's record while the switch watches it open (`seaShut:`): a hold that begins after it is one the switch saw begin. */
const WATCHED_OPEN = -1;
/** The telling's own flags, by their prefixes. */
const PORT_FLAGS = ['portTold:', 'seaShut:', 'portLost:', 'portHad:', 'portCrew:'];
const portFlag = (k: string): boolean => PORT_FLAGS.some((p) => k.startsWith(p));

/**
 * Switches the telling on or off in any month of a grand campaign; outside one it does nothing and answers false. Off
 * deletes the flag and every flag of the telling's own, so that a war switched on and off holds nothing of it. On, with
 * a player at the table, the switch takes its first look at the seas there and then (holdsMonth): a sea already shut
 * to its holder is a hold whose start it did not see, and one open is watched from then.
 */
export function setPortStores(g: Game, on: boolean): boolean {
  if (g.flags.open !== 1) return false;
  reading.delete(g);
  if (on) {
    const was = portStoresOn(g);
    g.flags.portStores = 1;
    if (!was && g.human && g.nations[g.human]?.alive) holdsMonth(g, false);
    return true;
  }
  delete g.flags.portStores;
  for (const k of Object.keys(g.flags)) if (portFlag(k)) delete g.flags[k];
  return true;
}

/**
 * A nation leaves the war for good (game.ts `capitulate`; domination.ts `leave` and `layDown`): the telling's flags
 * that are about it go, so no dead data stays in a lost war. Called while its ports are still its own. Nothing where
 * none was ever written.
 */
export function portsDone(g: Game, nat: string): void {
  // (With the switch off there are none: turning it off takes them all, setPortStores.)
  if (!portStoresOn(g)) return;
  for (let z = 0; z < g.seas.length; z++) delete g.flags[shutKey(z, nat)];
  for (const r of g.regions) if (g.flags[toldKey(r.id)] !== undefined && (r.ctrl === nat || nat === g.human)) delete g.flags[toldKey(r.id)];
  // (The lines kept for the player's month, and the ports he had corps in, are his alone: they go when he falls. So do
  // the holds the switch watched for him: given his seat back, he is watched afresh, and a sea already shut then is a
  // hold whose start the switch did not see. The crews lost in a port go with its holder, and with the player, whose
  // months no longer read them or clear them.)
  for (const k of Object.keys(g.flags)) {
    if (nat === g.human ? k.startsWith('portLost:') || k.startsWith('portHad:') || k.startsWith('seaShut:') || k.startsWith('portCrew:') : k.startsWith('portCrew:') && g.regions[+k.split(':')[1]]?.ctrl === nat) delete g.flags[k];
  }
}

// ---- The facts -----------------------------------------------------------------------------------------------------

/** The forces that shut one of the port's seas to its holder: the nations named, most first, and the arms each holds it with. */
export interface SeaHeld {
  sea: number;
  by: { nat: string; fleets: boolean; aircraft: boolean }[];
}

/**
 * One corps of the garrison and the month the rule strikes it off if no ship gets in; `by` where that month is only the
 * latest it can come, something the count leaves out as the month stands being able to bring it sooner (lostMonth).
 * `fed`: the last month the port's stores feed it (null where they feed none), by the keep the rule reads at its own
 * nation's draw each month (fedTo).
 */
export interface GarrisonCount { id: number; lost: number; by: boolean; fed: number | null }

/** What portCutOff answers. Months are game turns (turn 1 is September 1939). */
export interface PortCut {
  region: number;
  holder: string;
  /** How the viewer stands: the holder's side (the holder, or a nation with corps there), or the side that shuts the sea. */
  side: 'holder' | 'shutting';
  /**
   * The months the port's stores feed its garrison after the last fill (game.ts portKeep). For the shutting side, as
   * its own sight gives them (sightKeep): the port's own two (DESERT.stores) with the fortress rows off; under them, the
   * stores of the row the port holds only where it sees the port whole (with those of a crew lost there since its
   * side's hold began: the keep the stores were filled with), and null where it does not.
   */
  keep: number | null;
  /** The port's seas shut to the holder this month, and who holds each. Empty where none is. */
  shut: SeaHeld[];
  // ---- The holder's side (null for the shutting side, which never reads the holder's stores) ----
  /**
   * 'filled': a ship got in this month and the stores are full. 'stores': none has, and the garrison lives on the
   * port's stores. 'hungry': none has, and the stores are spent.
   */
  state: 'filled' | 'stores' | 'hungry' | null;
  /** True where the sea is shut to the holder and no ship has got in this month: the lines of a port cut off by sea. */
  cut: boolean;
  /** The month the stores were last filled for the holder (null where they never were, or the larder is not his). */
  lastFilled: number | null;
  /**
   * The last month they feed the garrison (null where they feed none): the soonest of its corps' (GarrisonCount.fed),
   * or with no garrison the holder's own, by the rule's clock (fedTo).
   */
  feedsTo: number | null;
  /** The month the garrison goes hungry: the first month the stores feed it no longer. */
  hungry: number | null;
  /**
   * The last month a fill of the stores this month would feed the garrison through, by the rule's clock as it stands
   * (the remedy line's month): under the fortress rows a crew whose nation has had this month's reckoning has eaten its
   * month already, and a fill after it leaves its count running, so that it starves when it would have; and a corps of a
   * nation whose month comes after a crew's is drawn without it in the month it starves (refillTo). The soonest of the
   * garrison's nations, or with no garrison the holder's own.
   */
  refill: number | null;
  /** The month the first corps of the garrison is struck off if no ship gets in (null with no garrison that starves). */
  lost: number | null;
  /**
   * True where `lost` is only the latest month the first loss can come: something the count leaves out as the month
   * stands (a treasury in debt, a corps that cannot count its month as rested) can bring it sooner. Its lines then say
   * "by the start of", and otherwise "at the start of" (the UI director's 23:50Z, item 6). Null for the shutting side,
   * whose count is always only the latest (it cannot see the garrison's order): its line says "by".
   */
  lostBy: boolean | null;
  /** Each corps of the garrison that the rule would strike off from hunger, with its month. */
  garrison: GarrisonCount[];
  /** Whether a fleet of the holder's can reach one of the shut seas this month. */
  fleetInReach: boolean | null;
  // ---- The shutting side, from its own sight only (null for the holder's side) ----
  /** The month its side's hold on the port's seas began, as its own fleets and aircraft know it. */
  heldSince: number | null;
  /**
   * Counting from then: the latest month the garrison is hungry by, and the latest month it is lost by. Bounds at both
   * ends (the UI director's 01:14Z, item 2): a hold first seen at a month's turn may have begun after the holder's last
   * fill of the month before, so that the hunger, and the loss, come a month sooner than counted.
   */
  heldHungry: number | null;
  heldLost: number | null;
}

/**
 * A port whose stores the rules keep, as the supply picture's window reads it (game.ts computeSupply), and whose lines
 * these are: a fortified port overseas, held by a nation alive and at war, with no army of the enemy's round the town (a
 * siege runs on the siege's own clock: siege.ts), no friendly ground beside it by land (where the roads may feed the
 * garrison whatever the stores), and not in its holder's pocket (portOfKind).
 */
function watched(g: Game, r: Region): boolean {
  return portOfKind(g, r) && !!g.nations[r.ctrl].atWar.length;
}
/**
 * The same port, its holder's wars aside. A port in a pocket is left to the pocket's own lines, as a besieged port is
 * left to the siege's (the UI director's 05:03Z, item 1): cut off by land as well as by sea (siege.ts pocketMonth), its
 * holder's army there lives on the pocket's stores on the pocket's own clock, and the pocket tells the player both
 * roads and ships ("{Gibraltar} is surrounded: no road and no ship reaches it from the rest of {Britain}. ..."). While
 * the holder's pocket record stands for the port (`pkt:`, siege.ts pocketNat), the port has no card, news or corps line
 * here, for any reader, the shutting side too; its lines come back the month the record goes.
 */
function portOfKind(g: Game, r: Region): boolean {
  return keepsStores(g, r) && pocketNat(g, r) !== r.ctrl;
}
/**
 * The same port, a pocket or not: one whose records the telling keeps (the holds, holdsMonth; a crew lost there,
 * portCrewGone), so that its lines come back on the rule's own clock the month the pocket's record goes. The records
 * are numbers and no line.
 */
function keepsStores(g: Game, r: Region): boolean {
  if (!r.overseas || !r.port || !worksKept(r)) return false;
  if (!g.nations[r.ctrl]?.alive) return false;
  if (siegeRules(g) && fieldCtrl(g, r) !== r.ctrl) return false;
  return !r.nb.some((j) => allied(g, g.regions[j].ctrl, r.ctrl));
}

/**
 * Who shuts a sea to the holder, split by nation from the sums closedTo makes (navy.ts SeaPicture.state, seaSide and
 * airOver's own terms). Whether it is shut is closedTo's answer alone; this only names. An arm is named where its
 * strength alone would shut the sea, and both where neither alone would; the nations are named, most first, until those
 * named would shut it by themselves.
 */
function holdersOf(g: Game, pic: SeaPicture, z: number, holder: string): SeaHeld['by'] {
  const st = pic.state(z);
  if (!st) return [];
  const ships = new Map<string, number>(), air = new Map<string, number>();
  const add = (m: Map<string, number>, nat: string, v: number): void => { if (v > 0) m.set(nat, (m.get(nat) ?? 0) + v); };
  for (const f of st.fleets) if (atWar(g, f.nat, holder)) add(ships, f.nat, f.power);
  const zone = g.seas[z];
  if (st.skies) {
    const S = skiesOver(g, z);
    if (S) for (const x of st.groups) if (atWar(g, x.nat, holder)) add(air, x.nat, (x.n / S.per) * zone.air * NAVY.air.weight);
  } else if (zone.coast.length && st.wx !== 0) {
    const per = (zone.air * st.wx * NAVY.air.weight) / zone.coast.length;
    for (const c of st.coasts) {
      if (atWar(g, c.ctrl, holder)) add(air, c.ctrl, c.air * (c.sky ? AIR.sky.seaFoe : 1) * per);
      else if (c.sky && atWar(g, c.sky, holder)) add(air, c.sky, c.skyAir * AIR.sky.seaOwn * per);
    }
  }
  const need = Math.max(NAVY.closed.min, NAVY.closed.ratio * seaSide(g, z, holder, false).total);
  const sum = (m: Map<string, number>): number => [...m.values()].reduce((s, v) => s + v, 0);
  let byShips = sum(ships) >= need, byAir = sum(air) >= need;
  if (!byShips && !byAir) { byShips = ships.size > 0; byAir = air.size > 0; }
  const weight = (n: string): number => (byShips ? ships.get(n) ?? 0 : 0) + (byAir ? air.get(n) ?? 0 : 0);
  const nats = [...new Set([...ships.keys(), ...air.keys()])].filter((n) => weight(n) > 0).sort((a, b) => weight(b) - weight(a) || g.order.indexOf(a) - g.order.indexOf(b));
  const out: SeaHeld['by'] = [];
  let got = 0;
  for (const n of nats) {
    out.push({ nat: n, fleets: byShips && (ships.get(n) ?? 0) > 0, aircraft: byAir && (air.get(n) ?? 0) > 0 });
    got += weight(n);
    if (got >= need) break;
  }
  return out;
}

/** True where the nation's corps have had this month's reckoning (game.ts startPhase, in the order of the nations). */
const charged = (g: Game, nat: string, uncharged?: string): boolean => nat !== uncharged && g.order.indexOf(nat) <= g.phase;

/**
 * The rest a corps gets back in a month it is fed (game.ts startPhase, the fed branch): from a month it fought in, or
 * not; with a whole month out of the line's own rest and its hospital's, or without; its line, its column, the winter
 * and its general as the month stands. The treasury is taken to pay.
 */
function fedRest(g: Game, c: Corps, fought: boolean, rested: boolean): number {
  const quiet = rested && moraleRules(g) && c.kind !== 'fort';
  const extra = quiet ? MORALE.rested.org : 0;
  const ward = quiet ? hospitalWorth(hospitalNear(g, c, (h) => h === c.nat || allied(g, h, c.nat))).org : 0;
  const bite = frozen(g, c.nat, g.regions[c.reg]) ? cmdWinter(g, c) : 0;
  return ((fought ? 0.25 : 0.45) + extra + ward) * lineStrength(g, c).mul * columnShare(g, c) * (1 - (1 - EASTERN.winter.rest) * bite) * cmdRally(g, c);
}
/** The rest a corps left where it stands gets back in a month it is fed: not fought, a whole month out of the line. */
const quietRest = (g: Game, c: Corps): number => fedRest(g, c, false, true);

/**
 * The month the rule strikes a corps off if no ship gets in: fed from the stores through `feedsTo` (resting as it
 * stands), then charged its hunger month by month (game.ts startPhase: UNFED from its order, struck off once nothing
 * is left), each in its own nation's month. The same sums the rule makes, so the month named is the month it acts.
 *
 * `worst`: the same count with what it leaves out as the month stands taken at its worst (the UI director's 23:50Z,
 * item 6; game.ts startPhase). Each fed month's rest is the treasury's to pay, and a treasury in debt pays for less of
 * it (`owed`, at most BUDGET.debt.rest of it lost). The count takes the corps to rest each month as one out of the line;
 * one that has fought or marched this month, or was raised in the month it is next charged, does not count its next
 * month as rested (morale.ts restedMonth), and one that fought gets back a fought month's rest; a town's garrison, or a
 * corps with an ally's army, is held where it stands every month (its moves are taken away as each month opens), and
 * counts none of them as rested. The hungry months take UNFED whatever else, so nothing is left out of them.
 */
function lostMonth(g: Game, c: Corps, feedsTo: number | null, uncharged?: string, worst = false): number {
  let m = charged(g, c.nat, uncharged) ? g.turn + 1 : g.turn;
  let o = c.org;
  if (feedsTo !== null && m <= feedsTo) {
    const rest = quietRest(g, c);
    if (!worst) for (; m <= feedsTo; m++) o = Math.min(1, o + rest);
    else {
      const n = g.nations[c.nat];
      const pay = budgeted(g) && n && n.pp < 0 ? 1 - BUDGET.debt.rest : 1;
      const held = !!c.town || c.joint !== undefined;
      const next = c.fought ? fedRest(g, c, true, false) : held || c.ap < c.apMax || c.born === m ? fedRest(g, c, false, false) : rest;
      const after = held ? fedRest(g, c, false, false) : rest;
      for (let k = m; m <= feedsTo; m++) o = Math.min(1, o + (m === k ? next : after) * pay);
    }
  }
  for (let i = 0; i < 50; i++, m++) {
    o -= UNFED;
    if (o <= 0) return m;
  }
  return m;
}

/** The months a full corps holds out hungry: the rule's own subtraction from full order (three). */
const hungryRun = (): number => { let o = 1, k = 0; do { o -= UNFED; k++; } while (o > 0 && k < 50); return k; };

/**
 * The corps of the holder's side in the port that the rule would strike off from hunger (no fortress crew, which keeps
 * its own clock, and no island garrison of the whole world's table, which withers). An ally's corps whose own ships
 * still reach the port this month is fed by them (game.ts computeSupply, its own lanes' quay), and is none of it: its
 * nation's draw reads the stores' keep in its own month, so that counting it as starving would name a month the rule
 * never acts in.
 */
function garrisonOf(g: Game, r: Region): Corps[] {
  const seen = new Set<Corps>();
  const own = new Map<string, boolean>();
  const shipped = (nat: string): boolean => {
    if (!own.has(nat)) { const n = g.nations[nat]; own.set(nat, !!n && (n.seaSupply || n.sealift > 0) && ((lanes) => r.seas.some((z) => lanes[z] === 1))(openSeas(g, nat))); }
    return own.get(nat)!;
  };
  for (const c of [...r.units, ...(r.garr ?? [])]) {
    if (c.dead || seen.has(c) || !allied(g, c.nat, r.ctrl) || !g.nations[c.nat]?.atWar.length || fortRow(g, c)) continue;
    if (farOn(g) && witherAt(g, c, enemyAshore(g, c))) continue;
    if (c.nat !== r.ctrl && shipped(c.nat)) continue;
    seen.add(c);
  }
  return [...seen];
}

// ---- The keep the rule feeds by -------------------------------------------------------------------------------------
//
// Under the fortress rows the port's stores are its crew's (game.ts portKeep, fortrows.ts portStores): the rule reads
// them afresh at every draw of the supply picture, from the crews living then, and the port's own two where none is. So
// the rule itself changes the keep mid-way: a crew that starves takes its months with it from the next draw on. In the
// play of the UI director's 01:14Z, item 3 (a crew fed at the last fill, its three months eaten with the sea shut), the
// crew starves in the reckoning of the stores' last month, after the draw that fed the garrison for it: a garrison of
// the crew's own nation, or of a nation whose month comes before it, is fed for all three months, the keep the stores
// were filled with, and the next month is hungry by either keep. A corps of a nation whose month comes after the crew's
// is drawn, in that last month, with the crew gone: the rule feeds it one month less, and so does the count.

/**
 * A fortress crew of the port's side whose stores the rule reads as the port's: its months, its nation, the month it
 * was raised (a crew never leaves its works) and the last month a draw sees it: for one living, the month the rule
 * starves it if no ship gets in (Infinity where it does not: on an island of the whole world's table it withers and
 * holds on, game.ts startPhase); for one gone, the month of the last draw that saw it (`portCrew:`). `seen`: the nation
 * whose draw in that month is the last to see it (its own, for a crew that starves in its own nation's reckoning).
 */
interface PortCrew { months: number; nat: string; from: number; to: number; seen: string }

/**
 * The port's crews as the rule's draws read them, if no ship gets in: those living, each to the reckoning that eats
 * the last of its stores (fortrows.ts crewMonth: a month a fill comes before its reckoning feeds it and its count
 * starts again; every other month eats one), and those lost there since the stores were last filled (`portCrew:`).
 * None with the fortress rows off.
 */
function crewsOf(g: Game, r: Region, last: number | null, uncharged?: string): PortCrew[] {
  if (!fortRows(g)) return [];
  const out: PortCrew[] = [];
  const far = farOn(g);
  for (const c of [...r.units, ...(r.garr ?? [])]) {
    if (c.dead || c.kind !== 'fort' || !allied(g, c.nat, r.ctrl)) continue;
    const months = crewStores(c);
    if (months === null) continue;
    const done = charged(g, c.nat, uncharged);
    const next = done || last === g.turn ? g.turn + 1 : g.turn;
    const eaten = !done && last === g.turn ? 0 : storesEaten(g, c);
    out.push({ months, nat: c.nat, from: c.born ?? 1, to: far && witherAt(g, c, enemyAshore(g, c)) ? Infinity : next + Math.max(1, Math.ceil(months - eaten - 1e-9)) - 1, seen: c.nat });
  }
  return out.concat(lostHere(g, r));
}

/**
 * The crews lost in the port since its stores were last filled, whatever took them, as the record keeps them
 * (`portCrew:`). None the port has changed hands since: its stores are no longer the side's that lost the crew.
 */
function lostHere(g: Game, r: Region): PortCrew[] {
  const out: PortCrew[] = [];
  const head = `portCrew:${r.id}:`;
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith(head) || r.since >= g.flags[k]) continue;
    const [, , , nat, months, from, seen] = k.split(':');
    out.push({ months: +months, nat, from: +from, to: g.flags[k], seen: seen ?? nat });
  }
  return out;
}

/**
 * The keep the rule reads at the draw of nation `nat`'s month m (game.ts computeSupply, as each nation's month opens):
 * the stores of the largest crew there then, or the port's own two. A crew is there from the draw of its own nation's
 * month in the month it was raised, and through the last draw that sees it: for a crew that starves, its own nation's
 * in the month it starves; for one lost otherwise, the last draw made before it was lost (`seen`).
 */
function keepAt(g: Game, crews: PortCrew[], m: number, nat: string): number {
  const at = g.order.indexOf(nat);
  let most: number | null = null;
  for (const x of crews) {
    const o = g.order.indexOf(x.nat), s = g.order.indexOf(x.seen);
    if ((m > x.from || (m === x.from && at >= o)) && (m < x.to || (m === x.to && at <= s)) && (most === null || x.months > most)) most = x.months;
  }
  return most ?? DESERT.stores;
}

/**
 * The last month the port's stores feed nation `nat`'s corps after the fill of month `last` (the fill's own month
 * where they feed none after it): each month the window feeds while the months since the fill are no more than the
 * keep its draw reads (game.ts computeSupply), month by month as the rule reads it (keepAt).
 */
function fedTo(g: Game, crews: PortCrew[], last: number, nat: string): number {
  let to = last;
  const most = Math.max(DESERT.stores, ...crews.map((x) => x.months));
  for (let m = last + 1; m - last <= most; m++) if (m - last <= keepAt(g, crews, m, nat)) to = m;
  return to;
}

/**
 * The last month a fill of the port's stores this month would feed these nations' corps through, the soonest of them
 * (the remedy line, the UI director's 03:13Z, item 1): the window counts from the fill (game.ts computeSupply), and the
 * keep its draws read is the crews' as they would be after it (crewsOf, read as filled this month). The fill comes in
 * the month as it stands, after the reckoning of every nation already charged: its crews have eaten this month's stores
 * and the fill does not restock them (a crew is restocked only in a reckoning fed all month, game.ts startPhase), so
 * that each still starves when its own count runs out; a crew of a nation not yet charged is fed at its reckoning and
 * its count starts again. With no crew, the fill's month and the port's own two.
 */
function refillTo(g: Game, r: Region, nats: string[], uncharged?: string): number {
  const crews = crewsOf(g, r, g.turn, uncharged);
  return Math.min(...nats.map((nat) => fedTo(g, crews, g.turn, nat)));
}

/**
 * The months the shutting side counts the port's stores for, from its own sight (ruling 1: never read from the holder's
 * stores): with the fortress rows off, the port's own two. Under them, a fortress crew's stores (portKeep) only where
 * it sees the port whole, and the crew with it; where it does not, null: a crew it cannot see may keep the stores
 * longer than the port's own two (Malta's three, Gibraltar's 16), so that neither the month the garrison goes hungry
 * nor the latest month it is lost can be given, and the count waits (the review of the 23:50Z build, finding 2). Seen
 * whole, a crew lost there since its side's hold began counts too, whatever took it: the stores were filled with its
 * keep, and the rule fed the garrison on it until the crew was gone (the UI director's 01:14Z, item 3, and 03:13Z, item
 * 2). The months counted from the hold are the rule's clock as that sight gives it (heldFed).
 */
function sightKeep(g: Game, r: Region, sight: number, since: number | null): number | null {
  if (sight < SIGHT.full) return fortRows(g) ? null : DESERT.stores;
  let keep = portKeep(g, r);
  for (const x of lostHere(g, r)) if (since === null || x.to >= since) keep = Math.max(keep, x.months);
  return keep;
}

/**
 * The latest month the stores can feed the garrison through, counted from the shutting side's hold (`since`, the latest
 * the holder's last fill can have come) by the rule's own clock, as its own sight gives it (the UI director's 03:13Z,
 * item 2): with the fortress rows off, or no crew, the port's own two from then. Seen whole under them, the keep each
 * draw reads (keepAt): each crew living there for the whole of its stores from the fill, the latest it can starve; and a
 * crew lost since the hold began, whatever took it, for the draws that saw it (`portCrew:`), and not after. Each nation
 * of the garrison by its own draw, the soonest of them: the garrison is hungry by the month after.
 */
function heldFed(g: Game, r: Region, since: number, keep: number): number {
  if (!fortRows(g)) return since + keep;
  const far = farOn(g);
  const crews: PortCrew[] = [];
  for (const c of [...r.units, ...(r.garr ?? [])]) {
    if (c.dead || c.kind !== 'fort' || !allied(g, c.nat, r.ctrl)) continue;
    const months = crewStores(c);
    if (months !== null) crews.push({ months, nat: c.nat, from: c.born ?? 1, to: far && witherAt(g, c, enemyAshore(g, c)) ? Infinity : since + Math.max(1, Math.ceil(months - 1e-9)), seen: c.nat });
  }
  for (const x of lostHere(g, r)) if (x.to >= since) crews.push(x);
  const nats = [...new Set(garrisonOf(g, r).map((c) => c.nat))];
  return Math.min(...(nats.length ? nats : [r.ctrl]).map((nat) => fedTo(g, crews, since, nat)));
}

/**
 * True where the viewer's own sight of the port shows no garrison there for the rule to starve: seen whole, none of
 * garrisonOf's corps; glimpsed (a flag and a count), no corps of the holder's side counted there; out of sight, never.
 */
function seenEmpty(g: Game, r: Region, sight: number): boolean {
  if (sight >= SIGHT.full) return !garrisonOf(g, r).length;
  if (sight >= SIGHT.glimpse) return !r.units.some((c) => !c.dead && allied(g, c.nat, r.ctrl));
  return false;
}

/**
 * True where, with true reach off, the port's stores are filled at every draw by ships other than the holder's (game.ts
 * computeSupply, `if (!truly) g.flags[storesKey(r.id)] = g.turn`, whoever's ships make the quay): the draw of a nation
 * with corps on the map that takes the port for a friend's, its own ships' lanes reaching one of the port's seas; or, in
 * a side theatre, an ally of that nation's with ships, whose lanes reach one (the shared shipping). The quay test the
 * rule makes there, so that the rule's clock never starts while it holds. Never with true reach on, where only ships
 * that could carry the holder's stores fill them (computeSupply, forHolder).
 */
function filledByOthers(g: Game, r: Region): boolean {
  if (trueReach(g)) return false;
  const lanes = new Map<string, boolean>();
  const reaches = (nat: string): boolean => {
    if (!lanes.has(nat)) { const n = g.nations[nat]; lanes.set(nat, !!n && (n.seaSupply || n.sealift > 0) && ((o) => r.seas.some((z) => o[z] === 1))(openSeas(g, nat))); }
    return lanes.get(nat)!;
  };
  const shipping = Object.values(g.nations).filter((n) => n.alive && (n.seaSupply || n.sealift > 0)).map((n) => n.id);
  for (const id of new Set(g.corps.map((c) => c.nat))) {
    if (!allied(g, id, r.ctrl)) continue;
    if (reaches(id) || (r.theatre >= 0 && shipping.some((a) => a !== id && allied(g, id, a) && reaches(a)))) return true;
  }
  return false;
}

/** True where a fleet of the nation's, with ships and free to sail, can reach one of these seas this month (navy.ts sail). */
function fleetFor(g: Game, nat: string, seas: number[]): boolean {
  return g.fleets.some((f) => f.nat === nat && shipCount(f.ships) > 0 && !inPassage(f) && f.attacked !== g.turn && fleetReach(g, f).some((z) => seas.includes(z)));
}

/**
 * The facts of a fortified port overseas whose stores the rules keep, as `viewer` may know them: null for any other
 * province, and for a viewer who is neither on the holder's side there (the holder, or a nation with corps in the port)
 * nor on the side whose forces shut its sea. A null viewer reads the holder's side (the rule's own view).
 *
 * The holder's side reads the rules' own clock: the month the stores were last filled (`stores:<id>`, whose they are
 * under true reach as the window reads it, keptStoresHis), the months they feed (portKeep), and each corps' order. The
 * shutting side reads only its own sight: how long its side has held the port's seas (`seaShut:`), and the rule's count
 * from the start of that hold for a garrison at full order; never the holder's stores, nor a fortress crew it does not
 * see (sightKeep). It has a count only where every sea of the port is shut to the holder, each held by its own side:
 * with one sea open the stores are filled and the clock never starts; so too, with true reach off, where another
 * nation's ships make a quay there (filledByOthers). Reads only: nothing is drawn or written. `uncharged` names a nation whose corps have not yet had this month's reckoning (the news, read as that
 * nation's month opens).
 */
export function portCutOff(g: Game, regionId: number, viewer: string | null, uncharged?: string): PortCut | null {
  const r = g.regions[regionId];
  if (!r || !watched(g, r)) return null;
  const holder = r.ctrl;
  const pic = new SeaPicture(g);
  const shut: SeaHeld[] = r.seas.filter((z) => closedTo(g, z, holder)).map((z) => ({ sea: z, by: holdersOf(g, pic, z, holder) }));
  const ours = viewer === null || viewer === holder || (allied(g, viewer, holder) && [...r.units, ...(r.garr ?? [])].some((c) => !c.dead && c.nat === viewer));
  if (!ours) {
    if (!viewer || !g.nations[viewer] || !atWar(g, viewer, holder)) return null;
    // The rule's clock runs only while every sea of the port is shut to the holder: one sea still open to him is a quay,
    // and the stores are filled (game.ts computeSupply, `r.seas.some((z) => open[z])`). And the count is its own side's
    // only where its own side holds every one of them: what it has held, and since when, is what its own forces know.
    if (shut.length !== r.seas.length || !shut.every((s) => s.by.some((b) => allied(g, b.nat, viewer)))) return null;
    // (Nor does it run where, with true reach off, another nation's ships make a quay there: their fill is the holder's
    // too, at every draw, whatever holds the sea against him. The UI director's 23:50Z: the shutting side's count only
    // where the rule's clock runs.)
    if (filledByOthers(g, r)) return null;
    // (The clock starts when the last of the port's seas was shut. A sea already shut when the switch first watched it
    // keeps 0: shut before any hold whose start the switch saw, so the last such start is the clock's; with no such
    // start there is no month to give, and the count waits: the UI director's 23:50Z, item 5. A sea still watched open
    // at the last reading, or never read, gives none either.)
    const starts = r.seas.map((z) => g.flags[shutKey(z, holder)]);
    const last = starts.some((x) => x === undefined || x === WATCHED_OPEN) ? null : Math.max(...(starts as number[]));
    const since = last === 0 ? null : last;
    const keep = sightKeep(g, r, sightOf(g, viewer)[r.id], since);
    // (The latest month the garrison is hungry by, and lost by: the UI director's 01:14Z, item 2. The record dates the
    // hold by the first reading that saw the sea shut, as a nation's month opens, and the holder's last fill was the
    // last draw that saw it open. A hold begun mid-month is seen the month it began, after the fill: the count is the
    // rule's own. One begun at a month's turn (a fleet put to sea in the last nation's month, the weather turning as the
    // month opens) is first seen in the new month, after a fill of the old one, and the rule acts a month sooner than
    // counted. Which of the two it was, the shutting side cannot tell from its own sight: only the holder's draws, and
    // whether his lanes reached the sea at them, say when his ships last got in.)
    const heldHungry = since === null || keep === null ? null : heldFed(g, r, since, keep) + 1;
    return {
      region: r.id, holder, keep, shut, side: 'shutting', state: null, cut: false, lastFilled: null, feedsTo: null, hungry: null, refill: null, lost: null, lostBy: null, garrison: [], fleetInReach: null,
      heldSince: since, heldHungry, heldLost: heldHungry === null ? null : heldHungry + hungryRun() - 1,
    };
  }
  const keep = portKeep(g, r);
  const base = { region: r.id, holder, keep, shut };
  // The window's own reading (game.ts computeSupply): the stores feed while the month is no later than the fill's
  // month and portKeep, and under true reach only where they are the present holder's.
  const flag = g.flags[`stores:${r.id}`];
  const his = flag !== undefined && (!trueReach(g) || keptStoresHis(g, r));
  const last = his ? flag : null;
  // (The months they feed each nation's corps, by the keep the rule reads at that nation's draw each month: the keep
  // they were filled with while the crew whose stores they are lives, and after it starves, the keep it leaves. The UI
  // director's 01:14Z, item 3. With the fortress rows off, or no crew, the port's own two: the fill's month and two.)
  const crews = last !== null ? crewsOf(g, r, last, uncharged) : [];
  const feeds = new Map<string, number | null>();
  const fedFor = (nat: string): number | null => {
    if (!feeds.has(nat)) feeds.set(nat, last === null ? null : fedTo(g, crews, last, nat));
    return feeds.get(nat)!;
  };
  // (Each corps' month, and the soonest it can come: the first loss is "by" its month where any corps of the garrison
  // can be lost before it.)
  const corps = garrisonOf(g, r);
  const counts = corps.map((c) => { const fed = fedFor(c.nat); return { id: c.id, fed, lost: lostMonth(g, c, fed, uncharged), soonest: lostMonth(g, c, fed, uncharged, true) }; });
  // (The garrison lives on the stores through the last month they feed all of it.)
  const feedsTo = last === null ? null : counts.length ? Math.min(...counts.map((x) => x.fed!)) : fedFor(holder);
  const state = last === g.turn ? 'filled' : feedsTo !== null && feedsTo >= g.turn ? 'stores' : 'hungry';
  const lost = counts.length ? Math.min(...counts.map((x) => x.lost)) : null;
  return {
    ...base, side: 'holder', state, cut: state !== 'filled' && shut.length > 0, lastFilled: last, feedsTo,
    hungry: feedsTo !== null ? feedsTo + 1 : g.turn,
    refill: refillTo(g, r, corps.length ? [...new Set(corps.map((c) => c.nat))] : [holder], uncharged),
    lost,
    lostBy: lost !== null && counts.some((x) => x.soonest < lost),
    garrison: counts.map((x) => ({ id: x.id, lost: x.lost, by: x.soonest < x.lost, fed: x.fed })),
    fleetInReach: shut.length ? fleetFor(g, holder, shut.map((s) => s.sea)) : false,
    heldSince: null, heldHungry: null, heldLost: null,
  };
}

// ---- The words ------------------------------------------------------------------------------------------------------

const WORDS = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];
/** Numbers in words to ten, figures from 11 (the marks' preface). */
const counted = (n: number): string => (n >= 0 && n <= 10 ? WORDS[n] : String(n));
const months = (n: number): string => (n === 1 ? 'one month' : `${counted(n)} months`);
/** Names joined whole: "A", "A and B", "A, B and C" (or, with `or`, "A or B", "A, B or C"). */
const join = (xs: string[], word = 'and'): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} ${word} ${xs[xs.length - 1]}`);
/** A month named alone in the present game year ("March"), with its year otherwise ("January 1942"). */
export function monthSaid(g: Game, turn: number): string {
  const d = turnToDate(turn);
  return d.year === turnToDate(g.turn).year ? d.label.split(' ')[0] : d.label;
}
const portOf = (r: Region): string => placeName(r.name);
/** "at the start of" a month the count names exactly, "by the start of" one it names as the latest (the UI director's 23:50Z, item 6). */
const startOf = (by: boolean | null): string => (by ? 'by the start of' : 'at the start of');
const PortOf = (r: Region): string => placeName(r.name, true);
/** "on" an island with no ground beside it by land, "in" any other province (the marks' preface: provinces take "in"). */
const onOrIn = (r: Region): string => (r.nb.length ? 'in' : 'on');
const seasSaid = (g: Game, seas: number[], word = 'and'): string => join(seas.map((z) => seaWhat(g.seas[z].name)), word);

/**
 * Who holds the seas, as a sentence's head and verb: "Italian fleets hold the Central Mediterranean", "German aircraft
 * hold the sky over ...", "Italian fleets and aircraft hold ...", "Italian and German fleets hold ...", "... hold the
 * Central Mediterranean and the Ionian Sea". Seas held alike are said together; seas held by others each in a clause
 * of their own. `perfect` gives "have held". The viewer's own nation reads "Your" where it is the only nation the whole
 * line names ("your" after the first clause); with others, every nation by its adjective (the marks, section 2).
 */
function heldSaid(g: Game, shut: SeaHeld[], viewer: string | null, perfect: boolean): string {
  const verb = perfect ? 'have held' : 'hold';
  const groups: { key: string; by: SeaHeld['by']; seas: number[] }[] = [];
  for (const s of shut) {
    const key = s.by.map((b) => `${b.nat}:${b.fleets ? 1 : 0}${b.aircraft ? 1 : 0}`).join(',');
    const at = groups.find((x) => x.key === key);
    if (at) at.seas.push(s.sea); else groups.push({ key, by: s.by, seas: [s.sea] });
  }
  const named = new Set(shut.flatMap((s) => s.by.map((b) => b.nat)));
  const alone = viewer !== null && named.size === 1 && named.has(viewer);
  return groups.map(({ by, seas }, i) => {
    const who = (nats: string[]): string => (alone ? (i ? 'your' : 'Your') : join(nats.map((n) => g.nations[n]?.adj ?? n)));
    const fleets = by.filter((b) => b.fleets).map((b) => b.nat), aircraft = by.filter((b) => b.aircraft).map((b) => b.nat);
    const where = seasSaid(g, seas);
    if (!aircraft.length) return `${who(fleets)} fleets ${verb} ${where}`;
    if (!fleets.length) return `${who(aircraft)} aircraft ${verb} the sky over ${where}`;
    if (fleets.length === by.length && aircraft.length === by.length) return `${who(fleets)} fleets and aircraft ${verb} ${where}`;
    return `${who(fleets)} fleets and ${who(aircraft)} aircraft ${verb} ${where}`;
  }).join(', and ');
}

/** A line of the port's card: its words, and how it is set out (a warning, bad news, or a plain note). */
export interface PortLine { text: string; tone: 'warn' | 'bad' | 'note' }

/**
 * The port's card for the viewer (the UI director's section 2), each month the sea is shut: to the holder and every
 * nation with corps there, the stores left or spent, and what opens the sea; with the sea open, what the stores would
 * feed; to the side that shuts it, how long it has held the sea and the rule's count from then. Empty with the switch
 * off, with no viewer (a war watched with nobody at the table), and for every other province or viewer.
 *
 * `measure` (the browser's card): the remedy line is followed by the measure its sea card gives this seat, a sentence
 * for each sea the line names (the UI director's 23:50Z, item 3). The 3D game's sea card shows no strengths, so its
 * card leaves the sentence out.
 */
export function portCardLines(g: Game, regionId: number, viewer: string | null, opts: { measure?: boolean } = {}): PortLine[] {
  // (A war watched with nobody at the table has no viewer: the card is nobody's. The rules' own readings, the news and
  // the corps struck off, read portCutOff's null viewer themselves.)
  if (!portStoresOn(g) || viewer === null) return [];
  const f = portCutOff(g, regionId, viewer);
  if (!f) return [];
  const r = g.regions[regionId];
  if (f.side === 'shutting') {
    // (From its own sight only: no line where it sees the port and no garrison in it; where it does not see the port,
    // the line stands whatever is there, so that it tells nothing of what is out of sight.)
    if (f.heldSince === null || f.heldLost === null || f.heldHungry === null || g.turn > f.heldLost || seenEmpty(g, r, sightOf(g, viewer)[r.id])) return [];
    return [{ text: theWords(`${heldSaid(g, f.shut, viewer, true)} since ${monthSaid(g, f.heldSince)}. Counting from then, the garrison of ${portOf(r)} is hungry by ${monthSaid(g, f.heldHungry)} and lost by the start of ${monthSaid(g, f.heldLost)}, unless ships get through.`), tone: 'note' }];
  }
  if (f.lost === null) return [];
  const keep = f.keep!;
  if (f.state === 'filled') return [{ text: theWords(`Cut off by sea, its stores would feed the garrison for ${months(keep)}.`), tone: 'note' }];
  if (!f.cut) return [];
  const out: PortLine[] = [];
  if (f.state === 'stores') out.push({ text: theWords(`Cut off by sea: ${heldSaid(g, f.shut, viewer, false)}, and no ship reaches the harbour. The garrison lives on the port's stores through ${monthSaid(g, f.feedsTo!)}; unless the sea is opened, it then goes hungry and is lost ${startOf(f.lostBy)} ${monthSaid(g, f.lost)}.`), tone: 'warn' });
  else out.push({ text: theWords(`Cut off by sea, and the port's stores are spent: the garrison goes hungry, losing order every month, and is lost ${startOf(f.lostBy)} ${monthSaid(g, f.lost)} unless the sea is opened.`), tone: 'bad' });
  // (One sea opened is enough: the quay is open where any sea of the port is in the holder's lanes. With several shut,
  // the seas whose opening joins his lanes are named, joined by "or".)
  const open = openers(g, r, f.shut.map((s) => s.sea));
  // (Ruling 4: "strong enough" shows only the measure the game's own sea card already shows that seat. The browser's
  // sea card shows each side's strength there and no threshold, so the line is followed by those two figures, a
  // sentence for each sea it names, and no figure of the strength needed: the UI director's 23:50Z, item 3.)
  const measured = opts.measure ? open.map((z) => measureSaid(g, z, viewer)).filter((x): x is string => x !== null) : [];
  // (What one such month feeds: the keep, counted from this month. Where a fill this month may feed the garrison less
  // long than that, a fortress crew's clock running on through it (PortCut.refill), the line counts the shorter and says
  // it is the least: the UI director's 03:13Z, item 1. Everywhere else, as laid.)
  const least = f.refill !== null && f.refill < g.turn + keep;
  const feeds = least ? `at least through ${monthSaid(g, f.refill!)}` : `through ${monthSaid(g, g.turn + keep)}`;
  out.push({ text: theWords(`A fleet of yours at sea there, strong enough to open ${seasSaid(g, open, 'or')}, lets the ships in and fills the stores: one such month feeds the garrison ${feeds}.${measured.map((x) => ` ${x}`).join('')}`), tone: 'note' });
  return out;
}

/**
 * The sea card's measure for the seat, in a sentence (the UI director's 23:50Z, item 3, and 01:14Z, item 1): "At sea in
 * the Central Mediterranean today: yours 11, the enemy's 131, as found." Its figures are the sea card's own, printed as
 * it prints them (carriers.ts seaMeasure, seaFigure: a strength is a measure, so figures under 11 stay figures), and
 * "as found" after the enemy's where the card says so. Null where the card gives none.
 */
function measureSaid(g: Game, z: number, viewer: string): string | null {
  const m = seaMeasure(g, z, viewer);
  return m && `At sea in ${seaWhat(g.seas[z].name)} today: yours ${seaFigure(m.ours)}, the enemy's ${seaFigure(m.theirs)}${m.found ? ', as found' : ''}.`;
}

/**
 * Of the port's seas shut to its holder, those whose opening alone would let his ships in: a sea his ships set out from
 * (navy.ts seaStarts), or one beside a sea his lanes reach this month (openSeas), as the supply picture's walk takes
 * them. All of them, in the port's order, where none would (a lane shut further out).
 */
function openers(g: Game, r: Region, shut: number[]): number[] {
  if (shut.length < 2) return shut;
  const nat = r.ctrl, lanes = openSeas(g, nat), starts = new Set(seaStarts(g, nat));
  const near = shut.filter((z) => starts.has(z) || g.seas.some((s, y) => lanes[y] === 1 && s.adj.includes(z) && canPass(g, y, z, nat)));
  return near.length ? near : shut;
}

/**
 * A corps' own line for its card (the UI director's section 3): living on the port's stores, through the month they
 * last; or hungry, with the month the rule strikes it off. Null with the switch off, for a corps not in such a port, and
 * where the sea is not shut or a ship got in this month. A corps of the garrison in a port cut off always has its line
 * (the UI director's 03:13Z, item 3).
 */
export function portCorpsLine(g: Game, c: Corps): string | null {
  // (Its own player's card: none in a war watched with nobody at the table.)
  if (!portStoresOn(g) || c.dead || g.human === null) return null;
  const f = portCutOff(g, c.reg, c.nat);
  if (!f || f.side !== 'holder' || !f.cut) return null;
  const mine = f.garrison.find((x) => x.id === c.id);
  if (!mine) return null;
  const port = portOf(g.regions[c.reg]);
  // (Its own months on the stores, by the keep the rule reads at its own nation's draw: GarrisonCount.fed. The line reads
  // the rule's clock as it stands when the card is read, and never the picture of supply as last drawn, which may be
  // another nation's draw of this month and change within it: in March of the Canadian play a corps whose nation's draw
  // comes after a crew starves is still drawn as fed by Britain's, with the crew living, and its own draw will not feed
  // it. Hungry by the clock, it reads hungry.)
  const fed = mine.fed !== null && mine.fed >= g.turn;
  return fed
    ? theWords(`Living on ${port}'s stores through ${monthSaid(g, mine.fed!)}.`)
    : theWords(`Hungry: ${port}'s stores are spent. Lost ${startOf(mine.by)} ${monthSaid(g, mine.lost)} unless ships get through.`);
}

// ---- The news -------------------------------------------------------------------------------------------------------

/** True where the player hears of the port: he holds it, or has corps there (the design director's ruling 2). */
const involved = (g: Game, r: Region, extra: Corps[] = []): boolean => {
  const me = g.human;
  return !!me && (r.ctrl === me || [...r.units, ...(r.garr ?? []), ...extra].some((c) => c.nat === me && (!c.dead || extra.includes(c))));
};

/**
 * The record of each hold, at every nation's month as it opens (after its draw of the supply picture): every sea of
 * every port of the kind (keepsStores, a pocket or not) held by a nation other than the player, whatever its wars and
 * the player's, so that a hold begun before the player's war with the holder is seen to begin. A sea open to the holder
 * keeps WATCHED_OPEN; one shut keeps the month it became shut. At the player's own month, records no port of the kind
 * still asks for go.
 *
 * Only a hold the switch saw begin has a month (the UI director's 23:50Z, item 5): a sea shut to its holder keeps the
 * month only where the reading before watched it open. A sea already shut the first time a reading watches it (the
 * switch's first look, as it is turned on, setPortStores, or at the first month it reads the seas; a port newly of the
 * kind; a nation newly holding one) keeps 0, and keeps it while it stays shut; once open and shut again, it keeps that
 * month.
 */
function holdsMonth(g: Game, sweep: boolean): void {
  const me = g.human!;
  const want = new Set<string>();
  for (const r of g.regions) {
    // (His own ports he never shuts: a hold on them is nobody's at his table. A port in its holder's pocket is watched
    // still: its lines come back when the pocket's record goes, from the hold the switch saw begin.)
    if (r.ctrl === me || !keepsStores(g, r)) continue;
    for (const z of r.seas) {
      const k = shutKey(z, r.ctrl);
      if (want.has(k)) continue;
      want.add(k);
      const was = g.flags[k];
      g.flags[k] = !closedTo(g, z, r.ctrl) ? WATCHED_OPEN : was === undefined ? 0 : was === WATCHED_OPEN ? g.turn : was;
    }
  }
  if (sweep) for (const k of Object.keys(g.flags)) if (k.startsWith('seaShut:') && !want.has(k)) delete g.flags[k];
}

/**
 * The news of ports cut off by sea, as a nation's month opens, straight after its draw of the supply picture (game.ts
 * startPhase): the record of the holds; the relief of a port whose cut-off was told; and at the player's own month,
 * before his corps are charged, the first month no ship gets in and the month the stores run out. Once each: the first
 * by the clock (the month after the last fill), the second by the clock too (the first month the stores feed none),
 * or, where the larder was never the holder's, the first month it is told. Only to a player who holds the port or has
 * corps there. Nothing with the switch off, or with nobody at the table.
 */
export function portsMonth(g: Game, nat: string): void {
  if (!portStoresOn(g) || !g.human || !g.nations[g.human]?.alive) { reading.delete(g); return; }
  const me = g.human, mine = nat === me;
  // (The draw just made: a crew lost from now until the next was last seen by it. portCrewGone.)
  reading.set(g, { turn: g.turn, nat });
  holdsMonth(g, mine);
  // (A crew lost in a port counts only until the port's stores are filled again, and only while it is of the kind, a
  // pocket or not.)
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('portCrew:')) continue;
    const r = g.regions[+k.split(':')[1]];
    if (!r || !keepsStores(g, r) || (g.flags[`stores:${r.id}`] ?? -1) >= g.flags[k] || r.since >= g.flags[k]) delete g.flags[k];
  }
  for (const r of g.regions) {
    const k = toldKey(r.id);
    const told = g.flags[k];
    if (told !== undefined) {
      // (A port lost since, or one the player no longer has any part in, keeps no mark.)
      if (!watched(g, r) || r.since > told || !involved(g, r)) { delete g.flags[k]; continue; }
      const flag = g.flags[`stores:${r.id}`];
      if (flag !== undefined && flag >= told && (!trueReach(g) || keptStoresHis(g, r))) {
        headline(g, 'info', `Ships reach ${portOf(r)} again: its stores are filled.`, r.ctrl, r.id, { won: r.ctrl });
        delete g.flags[k];
        continue;
      }
    }
    if (!mine || !involved(g, r)) continue;
    const f = portCutOff(g, r.id, null, me);
    if (!f || !f.cut || f.lost === null) continue;
    // (The mark keeps the month the first of the port's cut-off news was told: the relief is read from it.)
    if (f.state === 'stores' && f.lastFilled === g.turn - 1) {
      headline(g, 'info', `No ship reaches ${portOf(r)}: ${heldSaid(g, f.shut, null, false)}. The garrison lives on the port's stores through ${monthSaid(g, f.feedsTo!)}; unless the sea is opened, it then goes hungry and is lost ${startOf(f.lostBy)} ${monthSaid(g, f.lost)}.`, r.ctrl, r.id, { lost: r.ctrl });
      g.flags[k] ??= g.turn;
    } else if (f.state === 'hungry' && (f.feedsTo !== null ? f.feedsTo + 1 === g.turn : told === undefined)) {
      headline(g, 'info', `${PortOf(r)}'s stores are spent and no ship has got through: the garrison goes hungry, and is lost ${startOf(f.lostBy)} ${monthSaid(g, f.lost)} unless the sea is opened.`, r.ctrl, r.id, { lost: r.ctrl });
      g.flags[k] ??= g.turn;
    }
  }
}

/**
 * A corps struck off the roster, whatever took it (game.ts removeCorps: starved in its reckoning, beaten in battle,
 * ordered out of its works, gone with its country): a fortress crew of the port's side in a port of the kind is kept
 * for the count (`portCrew:`), with the month and the nation of the last draw that saw it living (the UI director's
 * 03:13Z, item 2). The rule reads the crews living at each draw (game.ts portKeep, computeSupply): every draw made
 * before the loss fed the garrison on the crew's stores, and every draw after it reads the stores without it, whatever
 * took it. So the counts read it as the rule's draws did (keepAt). Called as the corps goes, while it still stands in
 * its port. Nothing with the switch off, or with nobody at the table.
 */
export function portCrewGone(g: Game, c: Corps): void {
  if (!portStoresOn(g) || c.dead || !fortRow(g, c) || !g.human || !g.nations[g.human]?.alive) return;
  const r = g.regions[c.reg];
  // (In a port of the kind whose holder is at war, a pocket or not: the count reads it once the pocket's record goes.)
  if (!r || !keepsStores(g, r) || !g.nations[r.ctrl].atWar.length || !allied(g, c.nat, r.ctrl)) return;
  const months = crewStores(c);
  if (months === null) return;
  const at = lastDraw(g);
  g.flags[crewKey(r.id, c, months, at.nat)] = at.turn;
}

/**
 * The corps of a nation standing in ports of the kind as its month's reckoning begins (game.ts startPhase), so that
 * those the reckoning strikes off are told after it. Null with the switch off, or with no player at the table.
 */
export function portsWatched(g: Game, army: Corps[]): Corps[] | null {
  if (!portStoresOn(g) || !g.human || !g.nations[g.human]?.alive) return null;
  return army.filter((c) => !c.dead && watched(g, g.regions[c.reg]));
}

/**
 * The month it is struck off (the UI director's section 1): the corps a port cut off by sea has lost, one line a port,
 * "Several" where it lost more than one, whichever nations' corps they were. Each nation's corps are charged in its own
 * month (game.ts startPhase), so a corps struck off is kept (`portLost:`) from its nation's reckoning to the player's,
 * and his reckoning tells, after his own corps are charged, every corps his ports have lost since his last: those of
 * the nations before him this month with his own, in one line. Kept only where he hears of the port (ruling 2): he
 * holds it, has corps there, or had corps there as his month opened (`portHad:`).
 *
 * Two lines for one event (the UI director's 23:50Z, item 4): where the rule's own line tells this nation's corps laid
 * down their arms in the port this month ("Cut off and out of supply, the British army at Malta lays down its arms:
 * ...", game.ts startPhase; `surrendered`, the provinces it was told for), that line stands unchanged and these corps
 * are not kept for this one. The rule's line is a headline of the one news the table reads, told to the player
 * whatever his part in the port, in the same dispatch as this line would be; the corps it does not tell (another
 * nation's, a smaller party's) keep this line.
 */
export function portsStarved(g: Game, nat: string, before: Corps[] | null, surrendered?: ReadonlySet<number>): void {
  if (!before) return;
  const me = g.human!;
  const by = new Map<number, Corps[]>();
  for (const c of before) if (c.dead) { const l = by.get(c.reg); if (l) l.push(c); else by.set(c.reg, [c]); }
  for (const [reg, lost] of by) {
    // (A fortress crew starved in the port was kept for the count as it was struck off, whoever hears of it: portCrewGone.)
    const r = g.regions[reg];
    if (surrendered?.has(reg)) continue;
    if (!involved(g, r, lost) && g.flags[hadKey(reg)] !== g.turn) continue;
    const f = portCutOff(g, reg, null);
    if (!f || !f.cut) continue;
    // (A garrison's corps starves once the stores are spent; a fortress crew by its own clock, in the reckoning that eats
    // the last of its stores, which may be the month the stores still feed the rest of the garrison.)
    for (const c of lost) if (f.state === 'hungry' || fortRow(g, c)) g.flags[lostKey(reg, c)] = g.turn;
  }
  if (nat !== me) return;
  // His own month: one line a port, the corps in the order they fell.
  const told = new Map<number, { nat: string; name: string }[]>();
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('portLost:')) continue;
    const [, reg, , who, ...name] = k.split(':');
    const l = told.get(+reg);
    if (l) l.push({ nat: who, name: name.join(':') }); else told.set(+reg, [{ nat: who, name: name.join(':') }]);
    delete g.flags[k];
  }
  for (const [reg, lost] of told) {
    const r = g.regions[reg];
    const names = lost.map((c) => theUnit(c.name));
    headline(g, 'info', lost.length === 1
      ? `Cut off ${onOrIn(r)} ${portOf(r)}, ${names[0]} has starved and is lost.`
      : `Cut off ${onOrIn(r)} ${portOf(r)}, ${counted(lost.length)} corps have starved and are lost: ${join(names)}.`, lost[0].nat, reg, { lost: lost[0].nat });
  }
  // The ports his corps stand in as his month opens (before its reckoning): he hears of what they lose in it.
  for (const k of Object.keys(g.flags)) if (k.startsWith('portHad:')) delete g.flags[k];
  for (const c of before) if (c.nat === me) g.flags[hadKey(c.reg)] = g.turn;
}
