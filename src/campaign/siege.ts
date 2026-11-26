// Cities, towns and sieges (the grand campaign). The town is the key to its province.
//
// Troops in a province stand either in the field or inside its town as the garrison. The armies in the field are
// the region's `units`, as they always were: they march, fight in the open and retreat. The garrison is the region's
// `garr` list: it takes no part in the battle outside, and nothing that moves armies ever sees it.
//
// An army that beats the field army, or marches into a province nobody defends in the open, has won the countryside
// and nothing more. While the town is held (by a garrison, or by its own guard of depot troops and police) the
// province still belongs to its holder: the army camps round the town and the province is under siege. The town
// falls by assault, which is quick and bloody, or by hunger, which is slow. A relieving army that beats the
// besiegers in the field raises the siege. A town nobody holds falls to whoever walks in.
//
// Ruling 232 (Brian's ask of 18 November: "if i drive the army out of a city then i should conquer it"), behind the
// switch `driveOut`: a field army beaten outside its town never falls back into it. Where no garrison stands in the
// town, its guard fights in the same battle as the field army, behind its walls, and the town falls with the field.
// Only a garrison ordered in beforehand holds the town after the field is lost, to be stormed or starved.
//
// A province is under siege exactly when troops at war with its holder stand in its field. Nothing more is stored
// about who besieges whom: only the month the town was invested, the months it has been closely shut in, how much
// of its works have been broken down and how much of its guard is out of action, all in the flags.

import './far-rows'; // the far side's rows are laid over the data before anything reads it (off unless stage.json says so)
import data from '../../data/campaign/siege.json';
import { clamp } from '../core/math';
import { GUNS_ALONE, addCorps, aloneLine, allied, armySize, assess, atWar, capture, corpsBase, estAt, takenInWarThisMonth, fights, headline, keptStoresRun, men, natIndex, onPortStores, relocate, removeCorps, resolveBattle, retreatTarget, sendHome, theName, trueReach, worksFor, worksKept, fortRow, fortRows, roomless, type Assessment } from './game';
import { farGuard, farGuardRow, type FarGuardRow } from './economy';
import { crewStores, portStores, storesLeft } from './fortrows';
import { seat } from './commanders';
import { bookFed } from './history';
import { drawMen } from './manpower';
import { armyOf } from './joint';
import { closedTo, openSeas, seaOpenAt } from './navy';
import { storyFallen } from './stories';
import { TOWNS, townName, townSize, type TownSize } from './towns';
import { PRIZES, harbourOpen, prizesOnLoss, willBurn } from './prizes';
import { cityHit } from './repairs';
import { KIND, MEN_PER_DIV, STACK, type BattleReport, type Corps, type Game, type Region } from './types';
import { seaWhatList } from './sea-words';
import { flagName } from './flagnames';

export const SIEGE = data as unknown as {
  room: Record<TownSize, number>; fortRoom: number;
  bonus: Record<TownSize, number>; fortBonus: number[];
  guard: Record<TownSize, number>; guardShare: number; guardOrg: number;
  /** The far chart's town guard, land by land (far-guards.json; laid only with the far side's switch on). */
  farGuard?: FarGuardRow[];
  stores: Record<TownSize, number>; fortStores: number;
  ring: number; ringNear: number; pocket: { least: number; share: number }; summons: number; summonsNot: TownSize[]; stand: number;
  assault: { ap: number; loss: number; armour: number; hasty: Record<TownSize, number>; guardHit: number; dice: [number, number] };
  breach: { gun: number; raid: number; shell: number; most: number; air: number; mend: number };
  bombard: { raidFull: number; shellFull: number; org: number; floor: number; guard: number; intensity: number; value: number };
  sortie: number; pay: number;
  capital: number; capitalAfter: number; capitalYear: number;
  driveOut?: boolean;
  startIn?: { nat: string; corps: string; name: string; gen?: string; trait?: string; xp?: number; str?: number; fortress?: boolean; town?: string; divs?: number }[];
  rings?: { nat: string; town: string; corps: string; org: number }[];
  ai: { hasty: number; assault: number; late: number; last: { after: number; odds: number }; sea: { after: number; blockade: number }; relieved: { months: number; odds: number }; sally: number; raid: number; invest: number; relief: { capital: number; other: number }; hold: Partial<Record<TownSize, number>>; great?: { works: number; on?: boolean; field?: number }; lateGarrison?: { stops: number[]; str: number; org: number } };
};

/** True where towns are garrisoned and besieged: the grand campaign. A historical scenario is fought as it always was. */
export const siegeRules = (g: Game): boolean => g.flags.sieges === 1;

// (A run outside the browser, or a test, can set the switch by TC_DRIVE=1 or 0, read as the modules load.)
const G = globalThis as { TC_DRIVE?: unknown; process?: { env?: Record<string, string | undefined> } };
const forcedDrive = G.TC_DRIVE ?? G.process?.env?.TC_DRIVE;
/** The switch of ruling 232 (siege.json, 'driveOut'), as the game reads it: a test may turn it for its own file. */
export const DRIVE = { on: forcedDrive !== undefined ? !!forcedDrive && forcedDrive !== '0' : SIEGE.driveOut === true };

/**
 * True where ruling 232 holds (the grand campaign, with its switch on): beat the army, take the city. A beaten field
 * army never goes into the town, and a town with no garrison falls with the field.
 */
export const driveOut = (g: Game): boolean => DRIVE.on && siegeRules(g);

/** Turns the rules on for a grand campaign: a new one, or one saved before there were sieges. */
export function openSieges(g: Game): void {
  if (g.flags.open === 1) g.flags.sieges = 1;
}

const NONE: Corps[] = [];

/** Keeps count of what the sieges of a war came to, for the balance runs and for a summing-up at the end: `sgn:<what>`. */
export const tallySiege = (g: Game, what: string, by = 1): void => { g.flags[`sgn:${what}`] = (g.flags[`sgn:${what}`] ?? 0) + by; };

// ---- Who stands where ---------------------------------------------------------------------------

/** The corps inside the town. */
export const garrisonOf = (r: Region): Corps[] => r.garr ?? NONE;
/** The corps inside that take a place of the town's room: fortress crews, under the fortress rows, take none (06:15Z (b)). */
export const housed = (g: Game, r: Region): number => (r.garr ? r.garr.filter((c) => !roomless(g, c)).length : 0);

/** The corps in the field of a province that are at war with whoever holds its town. */
export function besiegers(g: Game, r: Region): Corps[] {
  if (g.flags.sieges !== 1 || !r.units.length) return NONE;
  const out = r.units.filter((u) => !u.dead && atWar(g, u.nat, r.ctrl));
  return out.length ? out : NONE;
}

/** True while an enemy army stands in the field of a province whose town it has not taken. */
export const besieged = (g: Game, r: Region): boolean => g.flags.sieges === 1 && r.units.length > 0 && r.units.some((u) => atWar(g, u.nat, r.ctrl));

/**
 * The nation whose army holds the countryside of a province: whoever holds its town, unless an enemy army stands
 * round it. Supply and the marching of armies go by this; whose province it is goes by `ctrl`, as it always did.
 */
export function fieldCtrl(g: Game, r: Region): string {
  if (g.flags.sieges !== 1 || !r.units.length) return r.ctrl;
  const foe = r.units.find((u) => atWar(g, u.nat, r.ctrl));
  return foe ? armyOf(foe) : r.ctrl;
}

/** The nation besieging a town, or null. */
export function besiegerOf(g: Game, r: Region): string | null {
  const foes = besiegers(g, r);
  return foes.length ? armyOf(foes[0]) : null;
}

// ---- The town: its room, its walls, its guard, its stores ---------------------------------------

/** How many corps the town can hold. */
export const roomIn = (g: Game, r: Region): number => SIEGE.room[townSize(g, r)] + (r.fort ? SIEGE.fortRoom : 0);

// Ruling 139: a staff holds its great cities. (A run outside the browser can turn it off by TC_GREAT=0, read as the modules load.)
const GG = globalThis as { TC_GREAT?: unknown; process?: { env?: Record<string, string | undefined> } };
const forcedGreat = GG.TC_GREAT ?? GG.process?.env?.TC_GREAT;
export const GREAT = { on: forcedGreat !== undefined ? !!forcedGreat && forcedGreat !== '0' : !!SIEGE.ai.great?.on };

/**
 * A great city (ruling 139): a capital, or a city whose works stand at the level named in siege.json (ai.great.works)
 * or higher. Its staff fills it before it leaves a corps in the field, and goes to its relief at the capital's odds.
 */
export function greatCity(g: Game, r: Region): boolean {
  if (!GREAT.on || !SIEGE.ai.great || !siegeRules(g)) return false;
  const size = townSize(g, r);
  return size === 'capital' || (size === 'city' && !!r.fort && r.fort.lv >= SIEGE.ai.great.works);
}

/**
 * A capital, or a city whose works stand at the great-city level (siege.json, ai.great.works), whether or not ruling
 * 139's own switch is on: the places ruling 232's late garrison (rider b587) looks after.
 */
export function greatPlace(g: Game, r: Region): boolean {
  if (!SIEGE.ai.great || !siegeRules(g)) return false;
  const size = townSize(g, r);
  return size === 'capital' || (size === 'city' && !!r.fort && r.fort.lv >= SIEGE.ai.great.works);
}

/** The share of the town's works that bombardment has broken down, 0 to 1. */
export const breachOf = (g: Game, r: Region): number => clamp((g.flags[`breach:${r.id}`] ?? 0) / 100, 0, 1);

/** Breaks down more of the works, by so many percent of the whole. Returns what is now broken, 0 to 1. */
export function breach(g: Game, r: Region, pct: number): number {
  const now = Math.min(SIEGE.breach.most, (g.flags[`breach:${r.id}`] ?? 0) + Math.max(0, pct));
  if (now > 0) g.flags[`breach:${r.id}`] = Math.round(now * 10) / 10;
  return now / 100;
}

/** What the town adds to its defenders before any bombardment: so much above 1. */
const wallsOf = (g: Game, r: Region, works = worksFor(g, r, r.ctrl)): number => SIEGE.bonus[townSize(g, r)] - 1 + (r.fort && works ? SIEGE.fortBonus[r.fort.lv] ?? 0 : 0);

/**
 * How much stronger the defenders fight inside the town. `eased` is the share of the works the guns and engineers
 * of this assault put out of account, on top of what earlier bombardment has broken down.
 */
export const townBonus = (g: Game, r: Region, eased = 0, works = worksFor(g, r, r.ctrl)): number => 1 + wallsOf(g, r, works) * (1 - breachOf(g, r)) * (1 - clamp(eased, 0, 1));

/**
 * The town's own guard, in divisions' worth: depot troops, police and militia. It stands only while the place is
 * held by the country it belongs to, and a small country's towns have small guards. (On the far side's colonial
 * ground the guard is a tenth, and none at a post whose record garrison is its whole defence: economy.ts farGuard.)
 * (On the far side's whole world a town beyond the old chart whose land has a row in the guard table stands at the
 * row's figure for its class, or its own where the row names it: the record's size, so no cap by the army is laid
 * on it, and the record's figure for the month where it changed. A village has none, and the post rule stays first.
 * economy.ts farGuardRow; far-guards.json.)
 */
export function guardOf(g: Game, r: Region): number {
  if (g.flags.sieges !== 1 || r.ctrl !== r.owner) return 0;
  const n = g.nations[r.owner];
  if (!n || !n.alive) return 0;
  const row = farGuardRow(g, r), size = townSize(g, r);
  const full = row
    ? (farGuard(r) === 0 ? 0 : estAt(row.named?.[r.name] ?? (size === 'village' ? 0 : row[size]), g.turn) ?? 0)
    : Math.min(SIEGE.guard[size], SIEGE.guardShare * Math.max(n.armyPeak, armySize(g, n.id))) * farGuard(r);
  const left = full * (1 - clamp((g.flags[`guard:${r.id}`] ?? 0) / 100, 0, 1));
  return left > 0.02 ? left : 0;
}

/** What the guard is worth in a fight for the town, before the town's bonus. */
export const guardPower = (g: Game, r: Region): number => {
  const d = guardOf(g, r);
  return d > 0 ? d * KIND.inf.def * g.nations[r.owner].quality * Math.sqrt(0.15 + 0.85 * SIEGE.guardOrg) : 0;
};

/**
 * The id a town's guard goes by on a battle field (ruling 232: the guard that fights beside the field army stands on the
 * field as a block of its own, the coordinator's ruling of 22 November): below every corps id, one to a town, so the
 * field's bill for it comes back apart from the corps'.
 */
export const guardForce = (r: Region): number => -1 - r.id;

/** The men in the guard, for a report. */
export const guardMen = (g: Game, r: Region): number => Math.round(guardOf(g, r) * MEN_PER_DIV);

/** Puts part of the guard out of action: so many percent of what it was at full strength. */
function hitGuard(g: Game, r: Region, pct: number): void {
  const now = Math.min(100, (g.flags[`guard:${r.id}`] ?? 0) + Math.max(0, pct));
  if (now > 0) g.flags[`guard:${r.id}`] = Math.round(now);
}

/**
 * The town's guard that fights beside its field army against an attack by `nat` (ruling 232), in divisions' worth:
 * where no garrison stands in the town, the guard is in the same battle as the army outside, and falls with it.
 * 0 where the rule is off, where a garrison holds the town (it is a separate fight), or where the attack is not an
 * enemy's (a relief, a sortie).
 */
export function guardInField(g: Game, r: Region, nat: string, force?: Corps[]): number {
  // (Shut in by an enemy already in the field, the guard is not beside the army there: a third power that beats the
  // besiegers takes the siege over, as before.)
  if (!driveOut(g) || garrisonOf(r).length || besieged(g, r) || !atWar(g, nat, r.ctrl)) return 0;
  // (The design director's b531: a guard that would open its gates to this army at the first summons, a town's or a
  // village's, does not stand in the field beside the army either. It gives itself up with the field, as it gave the
  // town up after the field before ruling 232: only a city's or a capital's guard, which never opens at a summons,
  // fights the field battle behind its walls.)
  if (force?.length && opensTo(g, r, force)) return 0;
  // (The book's dated attack fights the part of each side that met in the record's fight and no more: rider b580 to
  // ruling 234. The town's guard was no part of that line, so it stays behind its walls and holds the town after.)
  if (force?.some((c) => c.bookPart)) return 0;
  return guardOf(g, r);
}

/**
 * True where the town's guard opens its gates to this force at the first summons, without a fight: a town's or a
 * village's guard, against odds of SIEGE.summons or better. A city's or a capital's never does.
 */
export const opensTo = (g: Game, r: Region, force: Corps[]): boolean =>
  guardOf(g, r) > 0 && !SIEGE.summonsNot.includes(townSize(g, r)) && assess(g, wholeOf(force), r, { town: true }).odds >= SIEGE.summons;
/**
 * The army at the gates, whole. (By the book only a share of a corps may be in a dated fight, stations.ts, rider b580:
 * the guard reads the whole of each corps that stands before it, as it did before. Nobody else has a share.)
 */
const wholeOf = (force: Corps[]): Corps[] => (force.some((c) => c.bookPart) ? force.map((c) => (c.bookPart ? { ...c, divs: c.divs / c.bookPart[0] } : c)) : force);

/** A guard's size in words: "1 division's worth", "2.4 divisions' worth", and below one, "part of a division's worth" (never 0). */
export const guardWorth = (d: number): string => d >= 0.95 && d < 1.05 ? "1 division's worth" : d >= 1.05 ? `${(Math.round(d * 10) / 10).toFixed(1)} divisions' worth` : "part of a division's worth";

/**
 * How much stronger the guard fights behind the town's walls in a battle for the province (ruling 232), as it would in
 * a storm. Fortress works count for the guard only against the share of the attack (`open`, 0 to 1) they do not
 * already slow in the field, so they are never counted twice.
 */
export const fieldWalls = (g: Game, r: Region, eased: number, open: number): number =>
  1 + (SIEGE.bonus[townSize(g, r)] - 1 + (r.fort && worksFor(g, r, r.ctrl) ? (SIEGE.fortBonus[r.fort.lv] ?? 0) * clamp(open, 0, 1) : 0)) * (1 - breachOf(g, r)) * (1 - clamp(eased, 0, 1));

/** True while anybody holds the town: a garrison, or its guard. */
export const townHeld = (g: Game, r: Region): boolean => garrisonOf(r).length > 0 || guardOf(g, r) > 0;

/** Months the town feeds its defenders once it is closely invested. */
// (A place's works set aside for a fight still keep its stores: game.ts worksKept, rider b628 to ruling 234.)
export const storesOf = (g: Game, r: Region): number => rowStores(g, r) ?? SIEGE.stores[townSize(g, r)] + (worksKept(r)?.lv ?? 0) * SIEGE.fortStores;

/**
 * One figure for one larder (the design director's ruling of 22:08Z, 25 November 2026, on his 07:12Z of 22 November:
 * "one stores"). Under true reach and the fortress rows, a fortified port overseas that holds a fortress row with a
 * stores clock reads the row's months (fortrows.ts portStores) for the town's stores, the figure the supply picture's
 * window already keeps (game.ts portKeep): the guard's cap and the siege's opening stores (invest), the months left and
 * the starving (garrisonFed), the card (siegeView), a computer staff's patience (ai.ts) and a pocket living on the town
 * all read it through storesOf. Null elsewhere, and with either switch off: the town keeps its own figure.
 * At such a port each corps eats from one larder only. A fortress crew eats its own stores on its own clock
 * (fortrows.ts crewMonth, `eaten:<corps id>`), the row's months once, whether it lives on the port's stores while the
 * sea is shut (game.ts onPortStores) or is shut in by a siege (siegeSupply: a crew eats none of the town's stores). The
 * rest of the garrison eats the place's one stores: the window's months while the sea is shut and no enemy stands round
 * the town, and once the siege opens, the siege's clock (`sgm:`) against the same figure, with the kept months already
 * run charged at the opening (keptAtOpening).
 * A siege keeps the figure it opened with (`sgs:`, written by invest and cleared with the siege's other flags): the
 * figure is read from the living rows, and a crew killed or starved out in mid siege would otherwise drop it to the
 * town's own and starve the garrison at once. (The window, read only while no siege stands, still follows the living
 * rows.)
 */
function rowStores(g: Game, r: Region): number | null {
  if (!r.overseas || !r.port || !worksKept(r) || !fortRows(g) || !trueReach(g)) return null;
  return g.flags[`sgs:${r.id}`] ?? portStores(g, r);
}

/** Months the town has been closely invested. */
export const monthsShut = (g: Game, r: Region): number => g.flags[`sgm:${r.id}`] ?? 0;

/** The month the town was invested, or -1. */
export const investedOn = (g: Game, r: Region): number => g.flags[`siege:${r.id}`] ?? -1;

/** The nations whose ships could feed this town: its holder and whoever has troops inside, if they have ships to send and the quays to land at. */
export function feeders(g: Game, r: Region): string[] {
  if (!r.port || !r.seas.length) return [];
  // The quays of a small place lie outside the town: lost with the field, or wrecked, they land nothing (prizes.ts).
  if (!harbourOpen(g, r)) return [];
  // Ships have to sail from somewhere: a country down to its last harbour has nothing to send into it.
  return [...new Set([r.ctrl, ...garrisonOf(r).map((c) => c.nat)])].filter((id) => { const n = g.nations[id]; return !!n && n.alive && (n.seaSupply || n.sealift > 0) && portElsewhere(g, id, [r.id]); });
}

/**
 * True if a nation has a harbour to send ships from other than the places listed: a port of its own or of an ally
 * that no enemy army stands round, or a base off the map.
 */
export function portElsewhere(g: Game, nat: string, not: number[]): boolean {
  const n = g.nations[nat];
  if (!n) return false;
  if (n.offmap || g.fleets.some((f) => f.nat === nat && f.base < 0)) return true;
  for (const x of g.regions) {
    if (!x.port || !x.seas.length || not.includes(x.id) || !allied(g, nat, x.ctrl)) continue;
    if (allied(g, nat, fieldCtrl(g, x))) return true;
  }
  return false;
}

/**
 * The seas nation x's ships reach carrying nation forNat's stores (true reach), one bit a sea, as the supply picture's
 * own lanes give them: x's own lanes where forNat is x, and otherwise x's lanes walked with every sea closed to forNat
 * closed to them as well (navy.ts openSeas, its `carrying`). The supply picture hands the siege its kept answers
 * (game.ts computeSupply); every other reader takes seaReach, a fresh walk.
 */
export type SeaReach = (x: string, forNat: string) => Uint8Array;

/** The fresh walk of SeaReach: x's lanes from where its ships set out this month, carrying forNat's stores. */
export const seaReach = (g: Game, x: string, forNat: string): Uint8Array => openSeas(g, x, undefined, undefined, undefined, undefined, x === forNat ? undefined : forNat);

/**
 * Fresh walks kept for one reading only (one siegeView or siegesNow call): each (feeder, holder) is walked once however
 * many towns or questions ask it, and nothing is kept past the call.
 */
const walkedOnce = (g: Game): SeaReach => {
  const seen = new Map<string, Uint8Array>();
  return (x, forNat) => {
    const k = `${x} ${forNat}`;
    let o = seen.get(k);
    if (!o) seen.set(k, o = seaReach(g, x, forNat));
    return o;
  };
};

/**
 * True where the defenders' ships can still bring food into the harbour: such a town is never starved.
 *
 * Under true reach (game.ts trueReach; the design director's rulings of 13:58Z, 14:26Z and 17:35Z, 25 November 2026)
 * one town has one larder and one reach, and the reach is the holder's: the town is fed when the ships of one of its
 * feeders, carrying the holder's stores, reach a sea off the harbour by the supply picture's own lanes. Paper reach is
 * not reach: a sea off the harbour open to the feeder and the holder feeds nothing while the feeder's ships cannot sail
 * there. (A sea closed to the feeder or to the holder is never open in that walk, so the town is never fed through it.)
 * Every corps inside eats from the town, whoever's it is, and the holder needs no corps of its own inside. `reach`, if
 * given, answers the walks (the supply picture's kept lanes); absent, each is walked afresh. Off, a sea off the harbour
 * not closed to a feeder is enough.
 */
export function fedBySea(g: Game, r: Region, reach?: SeaReach): boolean {
  if (!trueReach(g)) return feeders(g, r).some((id) => seaOpenAt(g, id, r));
  const by = reach ?? ((x: string, id: string) => seaReach(g, x, id));
  return feeders(g, r).some((x) => { const o = by(x, r.ctrl); return r.seas.some((z) => o[z] === 1); });
}

/** True where the town's stores do not run down however closely it is ringed: ships still reach it, or the book feeds it (history.json 'restraint'). */
export const fed = (g: Game, r: Region, reach?: SeaReach): boolean => fedBySea(g, r, reach) || fedByBook(g, r);
const fedFrom = fed;

/** True for a town the book feeds against the ring round it now: Leningrad against a computer Germany, at stops 1 and 2. */
// (Each besieger by the army it fights in: a corps lent to a player stands in the player's ring.)
export const fedByBook = (g: Game, r: Region): boolean => besieged(g, r) && bookFed(g, r.name, r.ctrl, [...new Set(besiegers(g, r).map(armyOf))]);

/**
 * The seas the town's food comes in by: those off its harbour that are still open to someone who feeds it. Close them
 * all and it starves like any other. Under true reach, those some feeder's ships reach carrying the holder's stores
 * (fedBySea); `reach` as there.
 */
export function feedingSeas(g: Game, r: Region, reach?: SeaReach): number[] {
  const who = feeders(g, r);
  if (!trueReach(g)) return r.seas.filter((z) => who.some((id) => !closedTo(g, z, id)));
  const by = reach ?? ((x: string, id: string) => seaReach(g, x, id));
  const open = who.map((x) => by(x, r.ctrl));
  return r.seas.filter((z) => open.some((o) => o[z] === 1));
}

const strength = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);

/**
 * True when the town is shut in: only then do its stores run down. The army round it is strong enough by itself,
 * or with its side's troops in the country next door (they count for part of their weight), or every road out of
 * the place already runs into ground the enemy holds, so that nothing reaches it whatever stands at the gates.
 */
export function ringClosed(g: Game, r: Region): boolean {
  const out = strength(besiegers(g, r));
  if (out <= 0) return false;
  const need = SIEGE.ring * (strength(garrisonOf(r)) + guardOf(g, r));
  if (out >= need) return true;
  const by = besiegerOf(g, r);
  let near = 0, road = false;
  for (const j of r.nb) {
    const x = g.regions[j];
    if (allied(g, r.ctrl, fieldCtrl(g, x))) { road = true; continue; }
    if (by) for (const u of x.units) if (!u.dead && allied(g, u.nat, by)) near += u.divs * u.str;
  }
  return !road || out + SIEGE.ringNear * near >= need;
}

/** True while the garrison still eats: the stores last, or ships still come in. */
export const garrisonFed = (g: Game, r: Region, reach?: SeaReach): boolean => !besieged(g, r) || monthsShut(g, r) <= storesOf(g, r) || fed(g, r, reach);

// ---- A pocket: part of a country cut off from the rest of it ------------------------------------
//
// An army is fed from its country's depots: the capital, the great industrial regions. That is right while the
// country stands behind them. A capital with one province left to it and the enemy on every road is no country:
// the army round it lives on what the town has in store, and when that is eaten it goes hungry like any army cut
// off in the open. Four flags for each province of a pocket: `pkt:` the month it was cut off, `pkte:` the stores
// eaten so far in tenths of a month, `pktn:` whose it was, and `pktx:` once the stores are spent.

/** The provinces joined to one through ground a side holds in the field, or null if they come to more than `most`: that is a country, not a pocket. */
function joined(g: Game, nat: string, from: number, most: number): number[] | null {
  const out = [from];
  for (let i = 0; i < out.length; i++) {
    for (const j of g.regions[out[i]].nb) {
      if (out.includes(j) || !allied(g, nat, fieldCtrl(g, g.regions[j]))) continue;
      out.push(j);
      if (out.length > most) return null;
    }
  }
  return out;
}

/** True where a nation's army is fed from a province by right: its capital, its great industrial regions, a depot it has built. */
const depotOf = (g: Game, nat: string, r: Region): boolean => r.ctrl === nat && r.owner === nat && (r.cap || r.fac >= 3 || !!g.flags[`depot:${r.id}`]);

/**
 * The pocket a province of this nation lies in, if it is cut off from its country: a few provinces, a small part
 * of the home ground, every road out of them running into ground an enemy holds, and no ship able to come in from
 * another port. Null otherwise. `home` is the count of the nation's home provinces.
 */
function cutOffAt(g: Game, nat: string, r: Region, home: number): number[] | null {
  const P = SIEGE.pocket;
  const most = Math.min(P.least - 1, Math.ceil(P.share * home) - 1);
  if (most < 1 || !allied(g, nat, fieldCtrl(g, r))) return null;
  const pocket = joined(g, nat, r.id, most);
  if (!pocket) return null;
  let roads = 0;
  for (const id of pocket) {
    for (const j of g.regions[id].nb) {
      if (pocket.includes(j)) continue;
      // A neutral neighbour is no enemy: the ring is not closed.
      if (!atWar(g, nat, fieldCtrl(g, g.regions[j]))) return null;
      roads++;
    }
  }
  if (!roads) return null;
  const n = g.nations[nat];
  const ships = n.seaSupply || n.sealift > 0;
  if (ships && pocket.some((id) => { const x = g.regions[id]; return x.port && x.seas.length > 0 && allied(g, nat, x.ctrl) && harbourOpen(g, x) && seaOpenAt(g, nat, x); }) && portElsewhere(g, nat, pocket)) return null;
  return pocket;
}

/** The town a pocket lives on: the one with the most in store among those its side holds. */
function larder(g: Game, nat: string, pocket: number[]): Region | null {
  let best: Region | null = null;
  for (const id of pocket) {
    const x = g.regions[id];
    if (!allied(g, nat, x.ctrl)) continue;
    if (!best || storesOf(g, x) > storesOf(g, best)) best = x;
  }
  return best;
}

/** The nation whose troops are cut off in a province, or null if it lies in no pocket. */
export function pocketNat(g: Game, r: Region): string | null {
  if (g.flags.sieges !== 1 || g.flags[`pkt:${r.id}`] === undefined) return null;
  return Object.keys(g.nations)[g.flags[`pktn:${r.id}`] ?? -1] ?? null;
}

/** True for a corps cut off with its country's last depot: it eats the town's stores, and no drafts reach it. */
export const inPocket = (g: Game, c: Corps): boolean => g.flags[`pkt:${c.reg}`] !== undefined && pocketNat(g, g.regions[c.reg]) === c.nat;

/** True where a pocket's stores are spent: the province feeds nobody of the nation cut off in it (game.ts, the supply). */
export const pocketDry = (g: Game, r: Region, nat: string): boolean => g.flags[flagName('pktx:', r.id)] === 1 && pocketNat(g, r) === nat;

function clearPocket(g: Game, r: Region): void {
  delete g.flags[`pkt:${r.id}`];
  delete g.flags[`pkte:${r.id}`];
  delete g.flags[`pktn:${r.id}`];
  delete g.flags[`pktx:${r.id}`];
}

/** How fast a pocket's troops eat the stores: a month's worth each month for as many corps as the town has room for, and more in proportion. */
function mouths(g: Game, nat: string, pocket: number[], town: Region): number {
  let corps = 0;
  for (const id of pocket) {
    const x = g.regions[id];
    for (const c of x.units) if (c.nat === nat && !c.dead && c.kind !== 'fort') corps++;
    for (const c of garrisonOf(x)) if (c.nat === nat && !c.dead && !roomless(g, c)) corps++;
  }
  return Math.max(1, corps / Math.max(1, roomIn(g, town)));
}

/**
 * The start of a nation's month, before its supply is drawn (the grand campaign): which of its provinces are cut
 * off from the country, what they have eaten, and whether a ring has been broken.
 */
export function pocketMonth(g: Game, nat: string): void {
  if (!siegeRules(g)) return;
  const n = g.nations[nat];
  if (!n || !n.alive) return;
  let home = -1;
  const seen = new Set<number>();
  for (const r of g.regions) {
    if (r.ctrl !== nat || seen.has(r.id)) continue;
    const was = g.flags[`pkt:${r.id}`];
    // The enemy holds the country round the town: the siege has its own count of the stores (invest, above).
    if (besieged(g, r)) { if (was !== undefined) clearPocket(g, r); continue; }
    if (home < 0) home = g.regions.reduce((s, x) => s + (x.owner === nat ? 1 : 0), 0);
    const pocket = n.atWar.length ? cutOffAt(g, nat, r, home) : null;
    const town = pocket ? larder(g, nat, pocket) : null;
    // A pocket with no depot and no harbour in it has nothing to live on: its troops are out of supply at once, as they always were.
    const fed = !!pocket && pocket.some((id) => { const x = g.regions[id]; return depotOf(g, nat, x) || (x.port && allied(g, nat, x.ctrl)); });
    if (!pocket || !town || !fed) {
      if (was !== undefined) {
        const mine = pocketNat(g, r) === nat;
        const dry = g.flags[`pktx:${r.id}`] === 1;
        clearPocket(g, r);
        if (mine && allied(g, nat, fieldCtrl(g, r))) tell(g, r, `The ring round ${townName(r)} is broken: ${dry ? 'supplies reach' : 'the roads are open to'} the ${n.adj} troops there again.`, nat, [nat], { won: nat });
      }
      continue;
    }
    const mine = pocket.filter((id) => g.regions[id].ctrl === nat);
    for (const id of mine) seen.add(id);
    const olds = mine.map((id) => (pocketNat(g, g.regions[id]) === nat ? g.flags[`pkt:${id}`] : undefined)).filter((t): t is number => t !== undefined);
    const since = olds.length ? Math.min(...olds) : g.turn;
    const stores = storesOf(g, town);
    // What was eaten last month, by the mouths there are now.
    let eaten = Math.max(0, ...mine.map((id) => (pocketNat(g, g.regions[id]) === nat ? g.flags[`pkte:${id}`] ?? 0 : 0)));
    if (olds.length) eaten = Math.min(10 * stores, eaten + Math.round(10 * mouths(g, nat, pocket, town)));
    const dry = eaten >= 10 * stores;
    for (const id of mine) {
      g.flags[`pkt:${id}`] = since;
      g.flags[`pkte:${id}`] = eaten;
      g.flags[`pktn:${id}`] = natIndex(g, nat);
      if (dry) g.flags[`pktx:${id}`] = 1; else delete g.flags[`pktx:${id}`];
    }
    if (!olds.length) {
      const by = [...new Set(pocket.flatMap((id) => g.regions[id].nb).filter((j) => !pocket.includes(j)).map((j) => fieldCtrl(g, g.regions[j])))];
      // (Reckoned here and not read off the town's own view: the town that feeds a pocket may be an ally's, with no count kept on it.)
      const left = dry ? 0 : Math.max(1, Math.ceil((stores - eaten / 10) / mouths(g, nat, pocket, town)));
      const corps = pocket.reduce((s, id) => s + g.regions[id].units.filter((c) => c.nat === nat && !c.dead).length + garrisonOf(g.regions[id]).filter((c) => c.nat === nat).length, 0);
      const said = corps ? `${corps === 1 ? `One ${n.adj} corps is` : `${corps} ${n.adj} corps are`} cut off there, with stores for ${months(left)}; after that the troops go hungry.`
        : `The town has stores for ${months(left)}, and any troops who fall back on it will live on them.`;
      // Fed from an ally's town, the news is of the army and the province it holds: the town is not this nation's to lose.
      const own = town.ctrl !== nat && mine.length ? g.regions[mine[0]] : null;
      const lead = own ? `The ${n.adj} troops round ${townName(own)} are surrounded, with their allies in ${townName(town)}: no road and no ship reaches them` : `${townName(town)} is surrounded: no road and no ship reaches it`;
      tell(g, own ?? town, `${lead} from the rest of ${theName(n)}. ${said}`, nat, [nat, ...by], { lost: nat });
    }
  }
}

/** A pocket as the player reads it. */
export interface PocketView {
  /** Whose troops are cut off. */
  nat: string;
  /** The month the ring closed. */
  since: number;
  /** The town whose stores feed the pocket, and how many months they were laid in for. */
  town: string; stores: number;
  /** Months of stores left at the rate they are being eaten (0 once they are spent). */
  left: number;
  spent: boolean;
  text: string;
}

/** The pocket a province lies in, or null. */
export function pocketView(g: Game, region: number): PocketView | null {
  const r = g.regions[region];
  const nat = r ? pocketNat(g, r) : null;
  if (!r || !nat) return null;
  const n = g.nations[nat];
  const pocket = joined(g, nat, r.id, SIEGE.pocket.least) ?? [r.id];
  const town = larder(g, nat, pocket) ?? r;
  const stores = storesOf(g, town);
  const eaten = (g.flags[`pkte:${r.id}`] ?? 0) / 10;
  const spent = g.flags[`pktx:${r.id}`] === 1;
  const left = spent ? 0 : Math.max(1, Math.ceil((stores - eaten) / mouths(g, nat, pocket, town)));
  const text = spent
    ? `Cut off from the rest of ${theName(n)}, and the stores of ${townName(town)} are spent. The troops here are out of supply: they lose men and heart each month and lay down their arms unless the ring is broken.`
    : `Cut off from the rest of ${theName(n)}: no road and no ship reaches it. The troops here live on the stores of ${townName(town)}, enough for ${months(left)} more at the rate they are eaten, and no drafts reach them. A bigger army eats them faster.`;
  return { nat, since: g.flags[`pkt:${r.id}`], town: townName(town), stores, left, spent, text };
}

// ---- Into the town and out of it ----------------------------------------------------------------

/** Puts a corps on the town's list. The caller has already taken it off whatever list it was on. */
export function joinTown(g: Game, r: Region, c: Corps): void {
  (r.garr ??= []).push(c);
  c.reg = r.id;
  c.town = g.turn;
  c.ap = 0;
  if (c.dug) delete c.dug;
  if (c.digs) delete c.digs;
  if (c.yields) delete c.yields;
  if (c.job) delete c.job;
}

/**
 * Rider b606's garrisons are ruling 232's own and no part of the roster: a SIEGE.startIn row that carries 'town' and
 * 'divs'. `startOpen` calls this first of all, on the first day of a 1939 grand campaign with the ruling's switch on:
 * the corps is raised in that town's province, held there, as a row of the roster is, so that the campaign's opening
 * counts (the depots, the numbers its divisions carry) take it as one; `startGarrisons` then takes it inside at the
 * record's size. With the switch off, and in the history campaign, which never opens this way, there is no such corps
 * and that war's roster is what it was.
 */
export function raiseGarrisons(g: Game): void {
  if (g.turn !== 1 || !DRIVE.on) return;
  for (const s of SIEGE.startIn ?? []) {
    const r = s.town ? g.byName[s.town] : undefined;
    if (!s.divs || !r || r.ctrl !== s.nat || r.owner !== s.nat || g.corps.some((x) => x.nat === s.nat && x.name === s.corps)) continue;
    addCorps(g, s.nat, s.corps, 'inf', s.divs, r.id, { hold: 1 });
  }
}

/**
 * Ruling 232's mend for a city whose record garrison stood inside before any field army fell back (the design
 * director's b566 and b567: Warsaw, September 1939). At the start of a 1939 grand campaign with the ruling on, each
 * corps SIEGE.startIn names stands inside its own town, under the record's name, commander and experience. Its
 * divisions are the row's own, so nothing is counted twice; the rest of a siege's garrison comes by play, under 232(d).
 */
export function startGarrisons(g: Game): void {
  if (g.turn !== 1 || !driveOut(g)) return;
  for (const s of SIEGE.startIn ?? []) {
    const c = g.corps.find((x) => x.nat === s.nat && x.name === s.corps && !x.dead && x.town === undefined);
    const r = c ? g.regions[c.reg] : undefined;
    if (!c || !r || r.ctrl !== s.nat || r.owner !== s.nat || housed(g, r) >= roomIn(g, r)) continue;
    r.units.splice(r.units.indexOf(c), 1);
    joinTown(g, r, c);
    c.name = s.name;
    if (s.xp !== undefined) c.xp = s.xp;
    if (s.gen) seat(g, c, s.gen, s.trait);
    // (Rider b606: a row the record keeps small stands at its own size from the first day, and its army's count is taken again.)
    if (s.str !== undefined) { c.str = c.full = s.str; g.nations[s.nat].armyPeak = armySize(g, s.nat); }
    if (s.fortress) c.fortress = true;
  }
}

/**
 * Ruling 232's rider b606: a ring the record manned in the month its country was first attacked (Kiev, July 1941). The
 * first time anyone declares war on a nation SIEGE.rings names, the corps the row names, if it still stands inside
 * its town, fills to its full divisions before the attacker's battles that month: new men from the reserve, as a
 * call-up wave's are, as far as the reserve allows. Once; no staff reads it.
 */
export function manRings(g: Game, nat: string): void {
  if (!siegeRules(g) || !driveOut(g)) return;
  for (const s of SIEGE.rings ?? []) {
    const key = `ring:${s.town}`;
    if (s.nat !== nat || g.flags[key] !== undefined) continue;
    g.flags[key] = g.turn;
    const r = g.byName[s.town];
    const c = r && r.ctrl === nat ? garrisonOf(r).find((x) => x.nat === nat && x.name === s.corps && !x.dead) : undefined;
    if (!r || !c || c.str >= 1) continue;
    const men = drawMen(g, nat, (1 - c.str) * c.divs);
    c.str = Math.min(1, c.str + men / c.divs);
    delete c.full;
    c.org = Math.min(c.org, s.org);
    headline(g, 'event', `The forts round ${townName(r)} are manned.`, nat, r.id);
  }
}

/**
 * Where a corps bound for a province goes when the enemy holds the country round its town and the town is full:
 * the nearest ground its own side still holds in the field with room for one more corps, or the nearest such
 * ground at all if every province is full. Null if the town has room, or there is no such ground.
 */
export function musterNear(g: Game, nat: string, r: Region): Region | null {
  if (housed(g, r) < roomIn(g, r)) return null;
  const seen = new Uint8Array(g.regions.length);
  const q = [r.id];
  seen[r.id] = 1;
  let crowded: Region | null = null;
  for (let i = 0; i < q.length; i++) {
    const x = g.regions[q[i]];
    if (x !== r && allied(g, nat, x.ctrl) && allied(g, nat, fieldCtrl(g, x))) {
      if (x.units.filter((u) => u.kind !== 'eng' && allied(g, u.nat, nat) && !roomless(g, u)).length < STACK) return x;
      crowded ??= x;
    }
    for (const j of x.nb) if (!seen[j]) { seen[j] = 1; q.push(j); }
  }
  return crowded;
}

/** Takes a corps off the town's list, and does nothing else with it. */
export function quitTown(g: Game, c: Corps): void {
  const r = g.regions[c.reg];
  const k = r.garr ? r.garr.indexOf(c) : -1;
  if (k >= 0) r.garr!.splice(k, 1);
  if (r.garr && !r.garr.length) delete r.garr;
  delete c.town;
}

/** Why a corps cannot go into its province's town as garrison, or null if it can. */
export function whyNotGarrison(g: Game, c: Corps): string | null {
  if (!siegeRules(g)) return 'Towns are garrisoned only in the grand campaign.';
  if (c.dead) return 'That corps is no more.';
  const r = g.regions[c.reg], name = townName(r);
  if (c.town) return `It is already in ${name}.`;
  if (c.kind === 'fort') return 'Fortress troops stay in their works.';
  if (c.kind === 'eng') return 'Engineers work outside the town.';
  if (c.joint !== undefined) return 'A corps marching with an ally\'s army stays with it.';
  if (c.leave !== undefined) return 'It is out of the line this month.';
  if (!allied(g, c.nat, r.ctrl)) return `${name} is not in friendly hands.`;
  const room = roomIn(g, r);
  if (housed(g, r) >= room) return `${name} has room for ${room === 1 ? 'one corps' : `${room} corps`} only.`;
  return null;
}

/** Sends a corps into the town. It does nothing more this month. */
export function garrison(g: Game, c: Corps): boolean {
  if (whyNotGarrison(g, c)) return false;
  const r = g.regions[c.reg];
  r.units.splice(r.units.indexOf(c), 1);
  joinTown(g, r, c);
  return true;
}

/** Why a corps of the garrison cannot take the field, or null if it can. */
export function whyNotTakeField(g: Game, c: Corps): string | null {
  if (!c.town || c.dead) return 'It is not in a garrison.';
  const r = g.regions[c.reg];
  if (besieged(g, r)) return `The enemy holds the country round ${townName(r)}: the garrison can come out only if the siege is raised.`;
  const here = r.units.filter((u) => allied(g, u.nat, c.nat) && u.kind !== 'eng' && !roomless(g, u)).length;
  if (here >= STACK) return `There is no room in the field at ${r.name} for another corps.`;
  return null;
}

/** The moves a corps of the garrison has left once it is out in the field: coming out costs one, and one that went in this month has none. */
export const apOutOfTown = (g: Game, c: Corps): number => (c.town === g.turn ? 0 : Math.max(0, c.apMax - 1));

/** Brings a corps out of the town into the field. One that went in this month has no marching time left. */
export function takeField(g: Game, c: Corps): boolean {
  if (whyNotTakeField(g, c)) return false;
  const r = g.regions[c.reg];
  const ap = apOutOfTown(g, c);
  quitTown(g, c);
  r.units.push(c);
  c.ap = ap;
  return true;
}

/** True if a beaten corps with nowhere to go can fall back into the town instead of laying down its arms. */
export function canShelter(g: Game, c: Corps, r: Region): boolean {
  return siegeRules(g) && c.kind !== 'fort' && c.kind !== 'eng' && allied(g, c.nat, r.ctrl) && housed(g, r) < roomIn(g, r);
}

// ---- News ---------------------------------------------------------------------------------------

/** Sieges of cities are news to everyone; of smaller places, only to the player if his side is in it. */
function tell(g: Game, r: Region, text: string, nat: string, parties: string[], out: { won?: string; lost?: string } = {}): void {
  const size = townSize(g, r);
  const mine = g.human !== null && parties.some((p) => p === g.human || allied(g, p, g.human!));
  if (size === 'capital' || size === 'city' || mine) headline(g, 'battle', text, nat, r.id, { arm: 'land', ...out });
}

const placeWords = (g: Game, r: Region): string => (r.cap && r.owner === r.ctrl ? `the capital of ${theName(g.nations[r.owner])}` : `the ${TOWNS.words[townSize(g, r)].toLowerCase()}`);
const months = (n: number): string => (n <= 1 ? 'a month' : `${n} months`);
/** Months in words up to ten and in digits above, for the card of a port under true reach (the UI director, 22:08Z). */
const COUNTED = ['no', 'one', 'two', 'three', 'four', 'five', 'six', 'seven', 'eight', 'nine', 'ten'];
const monthsSaid = (n: number): string => (n <= 1 ? 'a month' : `${n <= 10 ? COUNTED[n] : n} months`);

// ---- Investing a town, raising a siege, and the fall --------------------------------------------

/**
 * The months of its kept stores a fortified port overseas has already eaten, for a siege that opens round it now
 * (game.ts keptStoresRun; the design director's ruling of 19:01Z, 25 November 2026). Opened at the start of the
 * holder's own month (siegeMonth, `monthOpen`), or in a nation's month that comes before the holder's, the siege's
 * clock charges the present month. Opened later in the month, the holder's month has already been fed from the kept
 * stores, and that month is charged here. Nothing off true reach.
 */
export function keptAtOpening(g: Game, r: Region, monthOpen = false): number {
  if (!trueReach(g)) return 0;
  return keptStoresRun(g, r, !monthOpen && g.order.indexOf(r.ctrl) <= g.phase);
}

/** The countryside is lost and the town holds: the siege begins. */
export function invest(g: Game, r: Region, by: string, monthOpen = false): void {
  const key = `siege:${r.id}`;
  if (g.flags[key] !== undefined) return;
  g.flags[key] = g.turn;
  // Who closed round it: so that an army driven off again can be told to burn what it held outside the walls.
  g.flags[`sgby:${r.id}`] = Object.keys(g.nations).indexOf(by);
  delete g.flags[`sgm:${r.id}`];
  // (Under true reach and the fortress rows, the row's figure for the town's stores, kept for the siege: rowStores.)
  delete g.flags[`sgs:${r.id}`];
  const row = rowStores(g, r);
  if (row !== null) g.flags[`sgs:${r.id}`] = row;
  // A town that has been feeding an army cut off round it starts its siege with those stores already eaten.
  let eaten = 0;
  if (g.flags[`pkt:${r.id}`] !== undefined) {
    const pocket = Math.min(storesOf(g, r), Math.floor((g.flags[`pkte:${r.id}`] ?? 0) / 10));
    if (pocket > 0 && pocketNat(g, r) === r.ctrl) eaten = pocket;
    clearPocket(g, r);
  }
  // Under true reach a fortified port overseas that has been living on its kept stores starts its siege with those
  // months eaten too (the design director's ruling of 19:01Z, 25 November 2026): one larder, so the larger of the two
  // clocks, never their sum, as both count the same eating, and never more than the town holds.
  // (A siege raised and closed again in one nation's month, with no draw of the supply picture between, reads the
  // stores' month as the siege left it. The picture is drawn between any two nations' months, and that order was not
  // seen in the recorded wars.)
  if (trueReach(g)) eaten = Math.min(storesOf(g, r), Math.max(eaten, keptAtOpening(g, r, monthOpen)));
  if (eaten > 0) g.flags[`sgm:${r.id}`] = eaten;
  g.supplyDirty = true;
  tallySiege(g, 'begun');
  const inside = garrisonOf(r);
  const who = inside.length ? `${inside.length === 1 ? 'one corps' : `${inside.length} corps`} inside` : 'only its guard inside';
  tell(g, r, `${g.nations[by].adj} troops close round ${townName(r)}: ${placeWords(g, r)} is under siege, with ${who}.`, r.ctrl, [by, r.ctrl, ...inside.map((c) => c.nat)], { won: by, lost: r.ctrl });
  // What lies outside the walls goes with the field: the side driven off it burns what it was ordered to. What
  // changed hands whole is news in the month it starts to work for its new holder (prizes.ts).
  // (Thrown for already this month: its town was relieved and the relieving army's foes threw for the same ground, or
  // the province was taken in war, whole, from a side that burns. Its own dice then; game.ts takenInWarThisMonth.)
  prizesOnLoss(g, r, by, r.ctrl, 'outside', g.flags[`relieved:${r.id}`] === g.turn || (takenInWarThisMonth(g, r) && willBurn(g, r.prev!, r)));
}

/** The besiegers are gone: the siege is raised. What was broken is mended month by month. */
export function lift(g: Game, r: Region, by: string | null = null): void {
  const key = `siege:${r.id}`;
  if (g.flags[key] === undefined) return;
  const lasted = g.turn - g.flags[key];
  const was = Object.keys(g.nations)[g.flags[`sgby:${r.id}`] ?? -1];
  delete g.flags[key];
  delete g.flags[`sgby:${r.id}`];
  delete g.flags[`sgm:${r.id}`];
  delete g.flags[`sgs:${r.id}`];
  // Raised by a relieving army: a staff that has just been driven off a town leaves it alone for a season (ai.ts).
  if (by) g.flags[`relieved:${r.id}`] = g.turn;
  g.supplyDirty = true;
  tallySiege(g, 'raised');
  tell(g, r, `The siege of ${townName(r)} is raised${lasted >= 1 ? ` after ${months(lasted)}` : ''}${by ? ` by ${g.nations[by].adj} troops` : ''}.`, r.ctrl, [r.ctrl, ...(by ? [by] : []), ...garrisonOf(r).map((c) => c.nat)], { won: r.ctrl, ...(was && g.nations[was] ? { lost: was } : {}) });
  // Driven off by a relieving army, the besiegers burn what they held outside the walls, if that is their order.
  // (Always the month's first throw for what lies outside: a siege that has lasted a month was begun before this
  // month, and nothing else throws for the field while it stands; a siege begun again this month has not lasted.)
  if (by && was && lasted >= PRIZES.settle) prizesOnLoss(g, r, by, was, 'outside');
}

/** How a town fell: stormed, opened to a summons, starved, empty, or with its guard in the battle for the province (ruling 232). */
export type Fell = 'storm' | 'gates' | 'hunger' | 'empty' | 'field';

/** The town falls, and the province with it. Anyone still inside lays down his arms. */
export function fall(g: Game, r: Region, by: string, how: Fell): void {
  const since = g.flags[`siege:${r.id}`];
  const lasted = since === undefined ? 0 : g.turn - since;
  const old = r.ctrl;
  // (Read before this fall's capture: a town taken in war already this month from a side that burns (r.prev) was
  // thrown for, so it throws its own dice (game.ts takenInWarThisMonth). Not the side driven out now: prizesOnLoss
  // throws only where it burns, so it would salt the month's first throw too. Where the town was relieved
  // this month and the place is lost whole, what lay outside was thrown for by the besiegers driven off: those kinds
  // only take the side, and what lies inside keeps its key.)
  const again = (takenInWarThisMonth(g, r) && willBurn(g, r.prev!, r)) || (since === undefined && g.flags[`relieved:${r.id}`] === g.turn ? 'outside' : false);
  tallySiege(g, how);
  if (how !== 'empty') tallySiege(g, 'months', lasted);
  tallySiege(g, 'taken', garrisonOf(r).length);
  for (const c of [...garrisonOf(r)]) {
    storyFallen(g, c, 'surrendered', by, r.id);
    removeCorps(g, c);
  }
  delete g.flags[`siege:${r.id}`];
  delete g.flags[`sgby:${r.id}`];
  delete g.flags[`sgm:${r.id}`];
  delete g.flags[`sgs:${r.id}`];
  delete g.flags[`guard:${r.id}`];
  delete g.flags[`storm:${r.id}`];
  delete g.flags[`sally:${r.id}`];
  clearPocket(g, r);
  const adj = g.nations[by].adj;
  // A capital's fall is told by the rules that take it; the rest is told here.
  const cap = r.cap && r.owner === old;
  // Scorched earth: what was not already lost with the field is burned, if that is the order, as the place falls.
  prizesOnLoss(g, r, by, old, since === undefined ? 'all' : 'inside', again);
  // (A town stormed, or taken with the field (ruling 232), is told by the battle's own report.)
  if (!cap && how !== 'empty' && how !== 'storm' && how !== 'field') {
    const text = how === 'gates' ? `${townName(r)} opens its gates to ${adj} troops without a fight.`
      : `Starved out after a siege of ${months(lasted)}, ${townName(r)} surrenders to ${adj} forces.`;
    tell(g, r, text, old, [by, old], { won: by, lost: old });
  }
  capture(g, r, by);
  g.supplyDirty = true;
}

/**
 * A town held only by its guard is summoned by the army round it: against odds heavy enough it opens its gates
 * without a fight. Returns true if it did.
 */
export function summon(g: Game, r: Region, by: string): boolean {
  if (garrisonOf(r).length) return false;
  const force = besiegers(g, r);
  if (!force.length) return false;
  // A great city does not give itself up at the first summons, whatever stands outside: its guard has to be beaten.
  if (guardOf(g, r) > 0 && !opensTo(g, r, force)) return false;
  fall(g, r, by, guardOf(g, r) > 0 ? 'gates' : 'empty');
  return true;
}

/**
 * An army has come into the field of a province: by winning the battle there or by marching in unopposed. Returns
 * true if the province is now in its side's hands, false if the town holds out and a siege has begun.
 */
export function arrive(g: Game, r: Region, nat: string): boolean {
  if (!siegeRules(g)) { capture(g, r, nat); return true; }
  // A relieving army: the ground was never lost, only overrun.
  if (allied(g, nat, r.ctrl)) { lift(g, r, nat); return true; }
  if (!townHeld(g, r)) { fall(g, r, nat, 'empty'); return true; }
  invest(g, r, nat);
  return summon(g, r, nat);
}

// ---- Storming the town --------------------------------------------------------------------------

/** The corps of an army round a town that can still go into an assault this month. */
export function stormers(g: Game, r: Region, nat: string): Corps[] {
  if (!besieged(g, r)) return NONE;
  return r.units.filter((c) => !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && armyOf(c) === nat && atWar(g, c.nat, r.ctrl));
}

/**
 * Why an army cannot storm the town it stands round, or null if it can. A town is assaulted once a month, by every
 * corps round it that is fit to go in: troops that have just won the country round it may follow the beaten
 * defenders straight to the walls, however far they have marched.
 */
export function whyNotStorm(g: Game, r: Region, nat: string): string | null {
  if (!siegeRules(g)) return 'Towns are stormed only in the grand campaign.';
  if (!besieged(g, r) || besiegerOf(g, r) === null) return `${townName(r)} is not under siege.`;
  if (!r.units.some((c) => armyOf(c) === nat && atWar(g, c.nat, r.ctrl))) return `You have no army round ${townName(r)}.`;
  if (g.flags[`storm:${r.id}`] === g.turn) return `${townName(r)} has been assaulted this month. The next assault can go in next month.`;
  const fit = stormers(g, r, nat);
  if (!fit.length) return 'No corps round the town is fit to storm it.';
  // Guns storm no town alone (ruling 211(a)).
  if (!fit.some(fights)) return aloneLine(fit);
  return null;
}

/**
 * The corps that go into an assault, out of those offered. An army round a town comes at it from every side, so
 * the width of the ground does not hold any back: only the size of battle its staff can run does. Engineers go in
 * with the infantry to breach the works.
 */
export function stormForce(g: Game, corps: Corps[], r: Region, lead?: string): Corps[] {
  const fit = corps.filter((c) => !c.dead && c.reg === r.id && !c.town && c.kind !== 'fort' && atWar(g, c.nat, r.ctrl));
  // Where allies storm together, the troops of the nation that leads go in first and the town falls to it.
  const own = (c: Corps): number => (lead && c.nat === lead ? 1 : 0);
  const line = fit.filter((c) => c.kind !== 'eng').sort((x, y) => own(y) - own(x) || corpsBase(g, y) * KIND[y.kind].atk - corpsBase(g, x) * KIND[x.kind].atk);
  // (Guns go in behind a fighting corps or not at all: ruling 211(a).)
  if (!line.some(fights)) return [];
  const most = g.nations[lead ?? line[0].nat].maxAttack || 99;
  const first = line.slice(0, most);
  if (!first.some(fights)) first[first.length - 1] = line.find(fights)!;
  return [...first, ...fit.filter((c) => c.kind === 'eng')];
}

/**
 * The months until hunger takes the fortress crews that hold a town (the fortress rows; the design director's 13:05Z):
 * the most of the stores left among the crews in its garrison, the game's own clock (fortrows.ts storesLeft). Null where
 * hunger does not take them: no crew there, a crew fed (in supply and not living on a port's stores), or a row with no
 * clock. Off, null.
 */
export function crewClock(g: Game, r: Region): number | null {
  if (!fortRows(g)) return null;
  const crews = garrisonOf(r).filter((c) => !c.dead && fortRow(g, c));
  if (!crews.length) return null;
  let most = 0;
  for (const c of crews) {
    if (c.supplied && !onPortStores(g, c)) return null;
    const left = storesLeft(g, c);
    if (left === null) return null;
    most = Math.max(most, left);
  }
  return most;
}

/**
 * The odds a computer staff asks before it storms a town it has stood round for `sat` months (siege.json 'ai'): off
 * the march only what it is sure to carry; after a month round the place what it probably will; when the siege has
 * dragged on, less, and at last a gamble. Under the fortress rows (the design director's 13:05Z) the bar does not fall
 * while hunger would take the town's fortress crews before the siege comes to its gamble: the staff storms only what
 * it is sure to carry, or waits for the hunger. Every staff, every crew-held town; no number of its own.
 */
export function stormBar(g: Game, r: Region, sat: number): number {
  const clock = crewClock(g, r);
  if (clock !== null && sat + clock <= SIEGE.ai.last.after) return SIEGE.ai.hasty;
  return sat >= SIEGE.ai.last.after ? SIEGE.ai.last.odds : sat >= 2 ? SIEGE.ai.late : sat >= 1 ? SIEGE.ai.assault : SIEGE.ai.hasty;
}

/** The odds of an assault, with its Battle Ticket. */
export function previewStorm(g: Game, corps: Corps[], r: Region): Assessment | null {
  const going = stormForce(g, corps, r);
  if (!going.length || !besieged(g, r)) return null;
  return assess(g, going, r, { town: true });
}

/** Storms the town with the corps given. */
export function storm(g: Game, corps: Corps[], r: Region): BattleReport | null {
  const going = stormForce(g, corps, r);
  if (!going.length || !besieged(g, r) || g.flags[`storm:${r.id}`] === g.turn) return null;
  g.flags[`storm:${r.id}`] = g.turn;
  return resolveBattle(g, going, r, { town: true });
}

// ---- The sortie ---------------------------------------------------------------------------------

/** The corps of a garrison fit to come out against the army round the town. */
export function salliers(g: Game, r: Region, nat: string): Corps[] {
  if (!besieged(g, r)) return NONE;
  return garrisonOf(r).filter((c) => !c.dead && armyOf(c) === nat && c.kind !== 'fort' && c.org >= 0.25);
}

/** Why a garrison cannot sally out, or null if it can. One sortie a month from each town. */
export function whyNotSally(g: Game, r: Region, nat: string): string | null {
  if (!siegeRules(g)) return 'Garrisons sally out only in the grand campaign.';
  if (!besieged(g, r)) return `${townName(r)} is not under siege.`;
  if (!garrisonOf(r).some((c) => armyOf(c) === nat)) return `You have no garrison in ${townName(r)}.`;
  if (g.flags[`sally:${r.id}`] === g.turn) return `The garrison of ${townName(r)} has made its sortie this month.`;
  const out = salliers(g, r, nat);
  if (!out.length) return 'The garrison is too shaken to come out.';
  // A garrison of guns alone makes no sortie (ruling 211(a)).
  if (!out.some(fights)) return aloneLine(out);
  return null;
}

/** The odds of a sortie, with its Battle Ticket. */
export function previewSally(g: Game, r: Region, nat: string): Assessment | null {
  const going = salliers(g, r, nat);
  return going.some(fights) ? assess(g, going, r, { sally: true }) : null;
}

/**
 * The garrison comes out against the army round the town. If it wins, the besiegers are driven off and the siege
 * is raised; the garrison goes back inside either way, and may take the field once the country is clear.
 */
export function sally(g: Game, r: Region, nat: string): BattleReport | null {
  if (whyNotSally(g, r, nat)) return null;
  const going = salliers(g, r, nat);
  g.flags[`sally:${r.id}`] = g.turn;
  tallySiege(g, 'sortie');
  return resolveBattle(g, going, r, { sally: true });
}

/** After an assault that failed: the guard has its losses too. `share` is the share of the defenders' strength lost. */
export function guardAfter(g: Game, r: Region, share: number): void {
  if (guardOf(g, r) <= 0) return;
  const gone = clamp((g.flags[`guard:${r.id}`] ?? 0) / 100, 0, 1);
  hitGuard(g, r, clamp(share, 0, 1) * (1 - gone) * 100);
}

/** A nation has left the war: the sieges of its towns are over, with no news of their being raised. */
export function dropSieges(g: Game, nat: string): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    delete g.flags[`siege:${r.id}`];
    delete g.flags[`sgby:${r.id}`];
    delete g.flags[`sgm:${r.id}`];
    delete g.flags[`sgs:${r.id}`];
    delete g.flags[`guard:${r.id}`];
  }
}

/**
 * Puts right what peace treaties, partitions and handovers leave behind: a garrison in a town its side no longer
 * holds marches out to the nearest friendly ground.
 */
export function siegeSweep(g: Game): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (!r.garr) continue;
    for (const c of [...r.garr]) if (!allied(g, c.nat, r.ctrl)) sendHome(g, c);
    // A town whose works were blown up, or that shrank, holds fewer than it did: the last corps in come out again,
    // into the country round it if no enemy stands there, and otherwise to the nearest ground their side holds.
    while (r.garr && housed(g, r) > roomIn(g, r)) {
      // (Fortress crews take no room, and stay in their works.)
      const c = [...r.garr].reverse().find((x) => !roomless(g, x))!;
      if (!takeField(g, c)) { const to = musterNear(g, c.nat, r); if (!to) break; relocate(g, c, to); c.ap = 0; }
      if (r.garr?.includes(c)) break;
    }
  }
}

// ---- The month ----------------------------------------------------------------------------------

/** The weight of guns round a town: the besiegers' artillery divisions. */
const guns = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);

/**
 * The player's own corps round a town of an enemy's are guns and engineers alone (his order has taken the fighting
 * corps away, or they are dead): what the paper says of it, or null (ruling 211(a); the UI director's line). The
 * siege is raised at the holder's next month unless a fighting corps joins them first.
 */
export function gunsLeftAlone(g: Game, region: number): string | null {
  const r = g.regions[region];
  if (!r || g.human === null || g.flags.open !== 1 || !siegeRules(g) || allied(g, g.human, r.ctrl)) return null;
  const round = besiegers(g, r).filter((c) => allied(g, c.nat, g.human!));
  if (!round.length || round.some(fights)) return null;
  const what = round.every((c) => c.kind === 'eng') ? 'engineers' : round.some((c) => c.kind === 'eng') ? 'guns and engineers' : 'guns';
  return `Only ${what} are left round ${townName(r)}: the siege is raised unless infantry or tanks join them this month.`;
}

/**
 * Guns and engineers left alone in the field of an enemy's province, their fighting corps dead or marched away:
 * they keep up no siege (ruling 211(a)). They fall back on ground their side holds, as a beaten army does, and
 * where there is none they are taken where they stand.
 */
function gunsFallBack(g: Game, r: Region, left: Corps[]): void {
  const by = armyOf(left[0]), town = townName(r);
  const back = new Set<string>();
  let taken = 0;
  for (const c of [...left]) {
    const to = retreatTarget(g, c, r, new Set());
    if (to) { relocate(g, c, to); c.ap = 0; back.add(to.name); }
    else { storyFallen(g, c, 'surrendered', r.ctrl, r.id); removeCorps(g, c); taken++; }
  }
  const who = `${g.nations[by].adj} guns`;
  const text = taken === left.length
    ? `The ${who} round ${town}, left with no infantry or tanks beside them and no way back, are taken where they stand. The siege is raised.`
    : `The ${who} round ${town}, left with no infantry or tanks beside them, fall back to ${[...back].join(' and ')}. The siege is raised.`;
  tell(g, r, text, r.ctrl, [by, r.ctrl, ...garrisonOf(r).map((c) => c.nat)], { won: r.ctrl, lost: by });
  g.supplyDirty = true;
}

/**
 * The start of a nation's month, for the towns it holds: sieges that have been raised are struck off and the works
 * mended; where a siege goes on, the guns play on the town, another month's stores are eaten if the ring is
 * closed, and a town with no garrison left gives in when its stores are gone. Called before supply is drawn, so
 * that a garrison whose stores have just run out goes hungry this month.
 */
export function siegeMonth(g: Game, nat: string): void {
  if (!siegeRules(g)) return;
  siegeSweep(g);
  for (const r of g.regions) {
    if (r.ctrl !== nat) continue;
    let foes = besiegers(g, r);
    // Guns left alone in an enemy field keep up no siege (ruling 211(a)): they fall back before the month's reckoning.
    if (foes.length && !foes.some(fights)) { gunsFallBack(g, r, foes); foes = besiegers(g, r); }
    if (!foes.length) {
      if (g.flags[`siege:${r.id}`] !== undefined) lift(g, r);
      // Left alone, the walls are mended and the guard fills up again.
      const b = g.flags[`breach:${r.id}`];
      if (b !== undefined) { if (b <= SIEGE.breach.mend) delete g.flags[`breach:${r.id}`]; else g.flags[`breach:${r.id}`] = b - SIEGE.breach.mend; }
      if (g.flags[`guard:${r.id}`] !== undefined) delete g.flags[`guard:${r.id}`];
      continue;
    }
    const by = armyOf(foes[0]);
    if (g.flags[`siege:${r.id}`] === undefined) invest(g, r, by, true);
    if (!townHeld(g, r)) { fall(g, r, by, 'empty'); continue; }
    const weight = guns(foes);
    if (weight > 0) {
      breach(g, r, SIEGE.breach.gun * weight);
      // Shellfire wears on the men inside too.
      for (const c of garrisonOf(r)) c.org = Math.max(0.15, c.org - 0.03 * Math.min(4, weight));
      // Damaged cities: the guns that play on its works wreck its streets as well, month after month.
      cityHit(g, r, 'guns', weight);
    }
    if (ringClosed(g, r) && !fed(g, r)) g.flags[`sgm:${r.id}`] = monthsShut(g, r) + 1;
    siegeEnds(g, r, by);
  }
}

/** A town whose stores are gone and whose garrison is gone gives in: its guard will not starve for nothing. */
function siegeEnds(g: Game, r: Region, by: string): boolean {
  if (garrisonOf(r).length) return false;
  if (guardOf(g, r) > 0 && garrisonFed(g, r)) return false;
  fall(g, r, by, 'hunger');
  return true;
}

/** The end of the same reckoning, once the hungry have been counted: towns whose last corps laid down its arms this month. */
export function siegeFalls(g: Game, nat: string): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (r.ctrl !== nat || g.flags[`siege:${r.id}`] === undefined) continue;
    const by = besiegerOf(g, r);
    if (by) siegeEnds(g, r, by);
  }
}

/**
 * The end of any nation's month: a town with nobody left to hold it, its guard broken by assault or bombardment and no
 * garrison inside, or handed to a country whose enemy already stood round it, falls to the army in its field. It does
 * not wait for its own country's month to come round.
 */
export function siegeEmpties(g: Game): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (!r.units.length) continue;
    // The army round the town is looked for first: whether the town is held counts every corps of its owner's.
    const by = besiegerOf(g, r);
    if (!by || townHeld(g, r)) continue;
    // (Guns alone take no town, even an empty one: ruling 211(a).)
    if (g.nations[by]?.alive && besiegers(g, r).some(fights)) fall(g, r, by, 'empty');
  }
}

/**
 * Marks which garrisons still eat. Called whenever the supply picture is drawn, which hands it the seas its lanes reach
 * under true reach (`reach`, SeaReach), so that the siege reads the very voyage the quays are drawn by.
 */
export function siegeSupply(g: Game, reach?: SeaReach): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (!r.garr || !r.garr.length || !besieged(g, r)) continue;
    const fed = garrisonFed(g, r, reach);
    // (A fortress crew inside eats none of the town's stores, the fortress rows' 06:15Z (b): it lives on its own, and
    // is fed only where food still reaches the town from outside, by sea or across a lake.)
    const outside = r.garr.some((c) => fortRow(g, c)) ? fedFrom(g, r, reach) : false;
    for (const c of r.garr) c.supplied = fortRow(g, c) ? outside : fed;
  }
}

/** True for a corps shut in a besieged town: no drafts reach it. */
export const shutIn = (g: Game, c: Corps): boolean => !!c.town && besieged(g, g.regions[c.reg]) && !fed(g, g.regions[c.reg]);

// ---- Softening a town: bombs and shells ---------------------------------------------------------

/** What a bombardment of a town does, or would do. */
export interface TownHit {
  /** How much of a bombardment in full this one is, 0 to 1. */
  share: number;
  /** Percent of the town's works it breaks down, and the share of them broken in all afterwards (0 to 1). */
  works: number; broken: number;
  /** How much stronger the defenders fight inside, before and after (1.45 is 45% stronger). */
  bonus: number; bonusAfter: number;
  /** What it takes off the organisation of each corps inside, and the percent of the town guard it puts out of action. */
  org: number; guard: number;
  /** Corps inside, and the divisions' worth of everything inside, guard and all. */
  corps: number; divs: number;
}

/** The share of a bombardment in full that this weight comes to: bombers that get through, or the weight of a fleet's guns. */
export const townShare = (kind: 'raid' | 'shell', weight: number): number => clamp(weight / (kind === 'raid' ? SIEGE.bombard.raidFull : SIEGE.bombard.shellFull), 0, 1);

/**
 * How far the works can be broken by bombs or by shells, in percent. Bombs stop at a fifth (ruling 139 rider b610: the
 * record's bombing never broke a great city's lines); shells, like the siege guns, go on to the most.
 */
export const breachCeiling = (kind: 'raid' | 'shell'): number => (kind === 'raid' ? Math.min(SIEGE.breach.air, SIEGE.breach.most) : SIEGE.breach.most);

/** What bombs or shells of this weight would do to a town. Nothing is changed. */
export function planTownHit(g: Game, r: Region, kind: 'raid' | 'shell', share: number): TownHit {
  const B = SIEGE.bombard, s = clamp(share, 0, 1);
  const now = g.flags[`breach:${r.id}`] ?? 0;
  const works = Math.max(0, Math.min(breachCeiling(kind) - now, SIEGE.breach[kind] * s));
  const inside = garrisonOf(r), guard = guardOf(g, r);
  return {
    share: s, works, broken: (now + works) / 100, bonus: townBonus(g, r), bonusAfter: 1 + wallsOf(g, r) * (1 - (now + works) / 100),
    org: inside.length ? B.org * s : 0, guard: guard > 0 ? B.guard * s : 0, corps: inside.length, divs: inside.reduce((t, c) => t + c.divs * c.str, 0) + guard,
  };
}

/** Bombs or shells fall on a town: part of its works are broken down, the troops inside are shaken, and its guard thinned. */
export function hitTown(g: Game, r: Region, kind: 'raid' | 'shell', share: number): TownHit {
  const hit = planTownHit(g, r, kind, share);
  tallySiege(g, kind);
  if (hit.works > 0) breach(g, r, hit.works);
  for (const c of garrisonOf(r)) c.org = Math.max(Math.min(c.org, SIEGE.bombard.floor), c.org - hit.org);
  if (hit.guard > 0) guardAfter(g, r, hit.guard / 100);
  return hit;
}

const stronger = (bonus: number): string => `${Math.round((bonus - 1) * 100)}%`;

/** A bombardment of a town in a sentence or two: `done` for what happened, otherwise what should happen. */
export function townHitText(g: Game, r: Region, hit: TownHit, by: 'bombs' | 'shells', done: boolean): string {
  const name = townName(r);
  const out: string[] = [];
  if (hit.works >= 0.5) out.push(done
    ? `The ${by} broke down ${Math.round(hit.works)}% of the works of ${name}: troops inside now fight ${stronger(hit.bonusAfter)} stronger, not ${stronger(hit.bonus)}.`
    : `Should break down about ${Math.round(hit.works)}% of the works of ${name}: troops inside would then fight ${stronger(hit.bonusAfter)} stronger, not ${stronger(hit.bonus)}.`);
  else out.push(hit.share > 0 && (g.flags[`breach:${r.id}`] ?? 0) >= breachCeiling(by === 'bombs' ? 'raid' : 'shell') - 0.5
    ? `The works of ${name} are already broken down as far as ${by} can break them.`
    : done ? `The ${by} did little to the works of ${name}.` : `Too little weight to break down the works of ${name}.`);
  if (hit.corps && hit.org >= 0.005) out.push(done ? `The garrison lost ${Math.round(hit.org * 100)}% of its organisation.` : `The garrison should lose about ${Math.round(hit.org * 100)}% of its organisation.`);
  if (hit.guard >= 1) out.push(done ? `About ${Math.round(hit.guard)}% of the town guard is out of action.` : `About ${Math.round(hit.guard)}% of the town guard should be put out of action.`);
  if (!done && !besieged(g, r)) out.push(`Left alone, the town mends ${SIEGE.breach.mend}% of its works a month: follow the raid with the army.`);
  return out.join(' ');
}

// ---- The relief ---------------------------------------------------------------------------------

/** What the garrison adds to a relieving army's attack on the besiegers: it sallies out as the relief comes up. */
export function sortiePower(g: Game, r: Region, attNat: string): { power: number; corps: number } {
  if (!siegeRules(g) || !allied(g, attNat, r.ctrl)) return { power: 0, corps: 0 };
  const inside = garrisonOf(r).filter((c) => !c.dead && c.org >= 0.25);
  return { power: SIEGE.sortie * inside.reduce((s, c) => s + corpsBase(g, c) * KIND[c.kind].atk, 0), corps: inside.length };
}

// ---- What an interface shows --------------------------------------------------------------------

export interface SiegeView {
  region: number;
  /** The town, and the size of the place in a word. */
  town: string; size: TownSize; sizeWord: string;
  /** Who holds the town, and who stands round it (null when it is not under siege). */
  holder: string; by: string | null;
  /** The month it was invested (-1 if it is not), and the months it has been closely shut in. */
  since: number; months: number;
  /**
   * Months of stores when the gates shut, months left, whether ships still feed it, whether the book feeds it across
   * the lake (history.json 'restraint', 'fed'), and whether the ring is closed.
   */
  stores: number; left: number; bySea: boolean; byBook: boolean; closed: boolean;
  /**
   * The fortress crews' own stores (the fortress rows; the design director's 13:05Z): the months left before hunger
   * takes the crews that hold the town, and the most months they were stocked for; null where hunger does not take
   * them. Both sides read it, as the computer's staffs do (stormBar).
   */
  crew: { left: number; months: number } | null;
  /** How much stronger its defenders fight inside (1.45 is 45% stronger), and the share of its works broken down. */
  bonus: number; broken: number;
  /** Corps the town can hold and corps inside, and the town's own guard in divisions' worth. */
  room: number; inside: number; guard: number;
  /** Plain words for a panel: what the place is, who holds it, how the siege stands. */
  lines: string[];
}

/**
 * The town of a province as a fortress: what it holds, what it adds, and how its siege stands. Null outside the grand
 * campaign. Under true reach whether ships still feed it is asked only of a town under siege (as whether the book feeds
 * it is), since only there does it count, and each feeder's voyage is walked once for the call (`reach`, from siegesNow
 * for every town it reads). Off, it is asked of every town, as before.
 */
export function siegeView(g: Game, region: number, reach?: SeaReach): SiegeView | null {
  const r = g.regions[region];
  if (!r || !siegeRules(g)) return null;
  const size = townSize(g, r), by = besiegerOf(g, r);
  const walk = trueReach(g) ? reach ?? walkedOnce(g) : undefined;
  const stores = storesOf(g, r), shut = monthsShut(g, r), bySea = !walk ? fedBySea(g, r) : !!by && fedBySea(g, r, walk), byBook = fedByBook(g, r);
  const inside = garrisonOf(r).length, guard = guardOf(g, r), bonus = townBonus(g, r), broken = breachOf(g, r);
  const left = Math.max(0, stores - shut);
  const clock = by ? crewClock(g, r) : null;
  const crew = clock === null ? null : { left: clock, months: Math.max(...garrisonOf(r).filter((c) => !c.dead && fortRow(g, c)).map((c) => crewStores(c) ?? 0)) };
  const lines: string[] = [];
  const pct = Math.round((bonus - 1) * 100);
  lines.push(`Troops inside ${townName(r)} fight ${pct}% stronger${broken > 0 ? `: bombardment has broken down ${Math.round(broken * 100)}% of its works` : ''}. It has room for ${roomIn(g, r) === 1 ? 'one corps' : `${roomIn(g, r)} corps`}.`);
  if (guard > 0) lines.push(`Its own guard of depot troops, police and militia is worth ${guard >= 0.95 ? `${guard.toFixed(1)} divisions` : 'part of a division'}. ${driveOut(g) && !inside && !by && r.units.some((u) => !u.dead && allied(g, u.nat, r.ctrl)) ? 'With no garrison inside, it fights beside the army in the field, and the town falls with the field.' : 'It holds the town while no army does.'}`);
  // (Ruling 232: only a garrison ordered in beforehand holds the town once the field is lost.)
  if (driveOut(g) && inside && !by) lines.push('The garrison will hold the town after the field is lost: it must then be stormed or starved out.');
  if (by) {
    const closed = ringClosed(g, r);
    const seas = bySea ? feedingSeas(g, r, walk).map((z) => g.seas[z].name) : [];
    lines.push(bySea ? `Ships still reach the harbour${seas.length ? ` by ${seaWhatList(seas)}` : ''}: the town cannot be starved while they do.${byBook ? ' Food comes in across the lake as well, so closing the sea would not starve it.' : ` A fleet at sea there, strong enough to close ${seas.length > 1 ? 'those waters' : 'that water'} to them, starts its stores running down.`}`
      : byBook ? 'Food still comes in across the lake: however close the ring, the town is not starved while no player\'s troops stand in it.'
      : !closed ? 'The ring is not closed: a road out is still open, and the army round it is too weak to shut it in alone. Its stores are not running down. Take the ground next door, or bring more troops up beside it.'
      : left > 0 ? `Closely invested. Its stores will feed the defenders ${left === 1 ? 'one more month' : `${left} more months`}; after that they go hungry.`
      : inside ? 'Its stores are gone: the garrison is starving, and loses men and heart every month.' : 'Its stores are gone: it is about to give in.');
    // The crews' own clock, read alike by both sides and by the computer's staffs (13:05Z).
    if (crew) lines.push(`Its fortress crews live on their works' own stores: ${crew.left === 1 ? 'one month' : `${crew.left} months`} of them left. When they run out the crews give in, whatever the town's own stores.`);
    // A capital shut in: the government loses its hold on the country month by month.
    if (closed && r.cap && r.owner === r.ctrl) {
      const sat = Math.max(0, g.turn - investedOn(g, r));
      lines.push(sat >= SIEGE.capitalYear ? `Shut in for ${months(sat)}, the government has lost its hold on the country: the capital counts as lost when it reckons whether to fight on.`
        : `A government shut in its capital loses its hold on the country month by month. After ${months(SIEGE.capitalYear)} of siege the capital counts as lost, whether it has fallen or not.`);
    }
  } else if (!trueReach(g)) {
    lines.push(`Closely invested, its stores would feed the defenders for ${months(stores)}${r.port ? ', and for as long as ships can reach its harbour' : ''}.`);
  } else {
    // Under true reach a fortified port overseas living on its kept stores has already eaten some of them: the card
    // promises what a siege opened now would leave (keptAtOpening), as the siege's own line then reads, and says why
    // the promise shrank (the UI director's words of 22:08Z, 25 November 2026, the numbers in words).
    const eaten = Math.min(stores, keptAtOpening(g, r)), would = stores - eaten;
    if (would <= 0) lines.push(`Closely invested, its defenders would go hungry at once: its stores have been eaten with no ship coming in${r.port ? ', and only ships reaching its harbour could feed them' : ''}.`);
    else {
      const ate = monthsSaid(eaten);
      lines.push(`Closely invested, its stores would feed the defenders for ${monthsSaid(would)}${r.port ? ', and for as long as ships can reach its harbour' : ''}.`
        + (eaten > 0 ? ` ${ate[0].toUpperCase()}${ate.slice(1)} of its stores ${eaten === 1 ? 'has' : 'have'} been eaten with no ship coming in.` : ''));
    }
  }
  return {
    region, town: townName(r), size, sizeWord: TOWNS.words[size], holder: r.ctrl, by, since: investedOn(g, r), months: shut,
    stores, left, bySea, byBook, closed: by ? ringClosed(g, r) : false, crew, bonus, broken, room: roomIn(g, r), inside, guard, lines,
  };
}

/** Every town under siege, for a map to mark. */
export function siegesNow(g: Game): SiegeView[] {
  if (!siegeRules(g)) return [];
  const out: SiegeView[] = [], walk = trueReach(g) ? walkedOnce(g) : undefined;
  for (const r of g.regions) if (besieged(g, r)) out.push(siegeView(g, r.id, walk)!);
  return out;
}

/** The men in a garrison, for a report line. */
export const garrisonMen = (r: Region): number => garrisonOf(r).reduce((s, c) => s + men(c), 0);
