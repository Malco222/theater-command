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
// A province is under siege exactly when troops at war with its holder stand in its field. Nothing more is stored
// about who besieges whom: only the month the town was invested, the months it has been closely shut in, how much
// of its works have been broken down and how much of its guard is out of action, all in the flags.

import data from '../../data/campaign/siege.json';
import { clamp } from '../core/math';
import { allied, armySize, assess, atWar, capture, corpsBase, headline, men, removeCorps, resolveBattle, sendHome, type Assessment } from './game';
import { armyOf } from './joint';
import { seaOpenAt } from './navy';
import { storyFallen } from './stories';
import { TOWNS, townName, townSize, type TownSize } from './towns';
import { KIND, MEN_PER_DIV, STACK, type BattleReport, type Corps, type Game, type Region } from './types';

export const SIEGE = data as unknown as {
  room: Record<TownSize, number>; fortRoom: number;
  bonus: Record<TownSize, number>; fortBonus: number[];
  guard: Record<TownSize, number>; guardShare: number; guardOrg: number;
  stores: Record<TownSize, number>; fortStores: number;
  ring: number; ringNear: number; summons: number; summonsNot: TownSize[]; stand: number;
  assault: { ap: number; loss: number; armour: number; hasty: Record<TownSize, number>; guardHit: number; dice: [number, number] };
  breach: { gun: number; raid: number; shell: number; most: number; mend: number };
  bombard: { raidFull: number; shellFull: number; org: number; floor: number; guard: number; intensity: number; value: number };
  sortie: number; pay: number;
  capital: number; capitalYear: number;
  ai: { hasty: number; assault: number; late: number; last: { after: number; odds: number }; sally: number; raid: number; invest: number; relief: { capital: number; other: number }; hold: Partial<Record<TownSize, number>> };
};

/** True where towns are garrisoned and besieged: the grand campaign. A historical scenario is fought as it always was. */
export const siegeRules = (g: Game): boolean => g.flags.sieges === 1;

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

/** The share of the town's works that bombardment has broken down, 0 to 1. */
export const breachOf = (g: Game, r: Region): number => clamp((g.flags[`breach:${r.id}`] ?? 0) / 100, 0, 1);

/** Breaks down more of the works, by so many percent of the whole. Returns what is now broken, 0 to 1. */
export function breach(g: Game, r: Region, pct: number): number {
  const now = Math.min(SIEGE.breach.most, (g.flags[`breach:${r.id}`] ?? 0) + Math.max(0, pct));
  if (now > 0) g.flags[`breach:${r.id}`] = Math.round(now * 10) / 10;
  return now / 100;
}

/** What the town adds to its defenders before any bombardment: so much above 1. */
const wallsOf = (g: Game, r: Region): number => SIEGE.bonus[townSize(g, r)] - 1 + (r.fort ? SIEGE.fortBonus[r.fort.lv] ?? 0 : 0);

/**
 * How much stronger the defenders fight inside the town. `eased` is the share of the works the guns and engineers
 * of this assault put out of account, on top of what earlier bombardment has broken down.
 */
export const townBonus = (g: Game, r: Region, eased = 0): number => 1 + wallsOf(g, r) * (1 - breachOf(g, r)) * (1 - clamp(eased, 0, 1));

/**
 * The town's own guard, in divisions' worth: depot troops, police and militia. It stands only while the place is
 * held by the country it belongs to, and a small country's towns have small guards.
 */
export function guardOf(g: Game, r: Region): number {
  if (g.flags.sieges !== 1 || r.ctrl !== r.owner) return 0;
  const n = g.nations[r.owner];
  if (!n || !n.alive) return 0;
  const full = Math.min(SIEGE.guard[townSize(g, r)], SIEGE.guardShare * Math.max(n.armyPeak, armySize(g, n.id)));
  const left = full * (1 - clamp((g.flags[`guard:${r.id}`] ?? 0) / 100, 0, 1));
  return left > 0.02 ? left : 0;
}

/** What the guard is worth in a fight for the town, before the town's bonus. */
export const guardPower = (g: Game, r: Region): number => {
  const d = guardOf(g, r);
  return d > 0 ? d * KIND.inf.def * g.nations[r.owner].quality * Math.sqrt(0.15 + 0.85 * SIEGE.guardOrg) : 0;
};

/** The men in the guard, for a report. */
export const guardMen = (g: Game, r: Region): number => Math.round(guardOf(g, r) * MEN_PER_DIV);

/** Puts part of the guard out of action: so many percent of what it was at full strength. */
function hitGuard(g: Game, r: Region, pct: number): void {
  const now = Math.min(100, (g.flags[`guard:${r.id}`] ?? 0) + Math.max(0, pct));
  if (now > 0) g.flags[`guard:${r.id}`] = Math.round(now);
}

/** True while anybody holds the town: a garrison, or its guard. */
export const townHeld = (g: Game, r: Region): boolean => garrisonOf(r).length > 0 || guardOf(g, r) > 0;

/** Months the town feeds its defenders once it is closely invested. */
export const storesOf = (g: Game, r: Region): number => SIEGE.stores[townSize(g, r)] + (r.fort ? SIEGE.fortStores * r.fort.lv : 0);

/** Months the town has been closely invested. */
export const monthsShut = (g: Game, r: Region): number => g.flags[`sgm:${r.id}`] ?? 0;

/** The month the town was invested, or -1. */
export const investedOn = (g: Game, r: Region): number => g.flags[`siege:${r.id}`] ?? -1;

/** True where the defenders' ships can still bring food into the harbour: such a town is never starved. */
export function fedBySea(g: Game, r: Region): boolean {
  if (!r.port || !r.seas.length) return false;
  const nats = new Set([r.ctrl, ...garrisonOf(r).map((c) => c.nat)]);
  for (const id of nats) {
    const n = g.nations[id];
    if (n && n.alive && (n.seaSupply || n.sealift > 0) && seaOpenAt(g, id, r)) return true;
  }
  return false;
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
export const garrisonFed = (g: Game, r: Region): boolean => !besieged(g, r) || monthsShut(g, r) <= storesOf(g, r) || fedBySea(g, r);

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
 * Where a corps bound for a province goes when the enemy holds the country round its town and the town is full:
 * the nearest ground its own side still holds in the field with room for one more corps, or the nearest such
 * ground at all if every province is full. Null if the town has room, or there is no such ground.
 */
export function musterNear(g: Game, nat: string, r: Region): Region | null {
  if (garrisonOf(r).length < roomIn(g, r)) return null;
  const seen = new Uint8Array(g.regions.length);
  const q = [r.id];
  seen[r.id] = 1;
  let crowded: Region | null = null;
  for (let i = 0; i < q.length; i++) {
    const x = g.regions[q[i]];
    if (x !== r && allied(g, nat, x.ctrl) && allied(g, nat, fieldCtrl(g, x))) {
      if (x.units.filter((u) => u.kind !== 'eng' && allied(g, u.nat, nat)).length < STACK) return x;
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
  if (garrisonOf(r).length >= room) return `${name} has room for ${room === 1 ? 'one corps' : `${room} corps`} only.`;
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
  const here = r.units.filter((u) => allied(g, u.nat, c.nat) && u.kind !== 'eng').length;
  if (here >= STACK) return `There is no room in the field at ${r.name} for another corps.`;
  return null;
}

/** Brings a corps out of the town into the field. One that went in this month has no marching time left. */
export function takeField(g: Game, c: Corps): boolean {
  if (whyNotTakeField(g, c)) return false;
  const r = g.regions[c.reg];
  const fresh = c.town === g.turn;
  quitTown(g, c);
  r.units.push(c);
  c.ap = fresh ? 0 : Math.max(0, c.apMax - 1);
  return true;
}

/** True if a beaten corps with nowhere to go can fall back into the town instead of laying down its arms. */
export function canShelter(g: Game, c: Corps, r: Region): boolean {
  return siegeRules(g) && c.kind !== 'fort' && c.kind !== 'eng' && allied(g, c.nat, r.ctrl) && garrisonOf(r).length < roomIn(g, r);
}

// ---- News ---------------------------------------------------------------------------------------

/** Sieges of cities are news to everyone; of smaller places, only to the player if his side is in it. */
function tell(g: Game, r: Region, text: string, nat: string, parties: string[]): void {
  const size = townSize(g, r);
  const mine = g.human !== null && parties.some((p) => p === g.human || allied(g, p, g.human!));
  if (size === 'capital' || size === 'city' || mine) headline(g, 'battle', text, nat, r.id);
}

const placeWords = (g: Game, r: Region): string => (r.cap && r.owner === r.ctrl ? `the capital of ${g.nations[r.owner].name}` : `the ${TOWNS.words[townSize(g, r)].toLowerCase()}`);
const months = (n: number): string => (n <= 1 ? 'a month' : `${n} months`);

// ---- Investing a town, raising a siege, and the fall --------------------------------------------

/** The countryside is lost and the town holds: the siege begins. */
export function invest(g: Game, r: Region, by: string): void {
  const key = `siege:${r.id}`;
  if (g.flags[key] !== undefined) return;
  g.flags[key] = g.turn;
  delete g.flags[`sgm:${r.id}`];
  g.supplyDirty = true;
  tallySiege(g, 'begun');
  const inside = garrisonOf(r);
  const who = inside.length ? `${inside.length === 1 ? 'one corps' : `${inside.length} corps`} inside` : 'only its guard inside';
  tell(g, r, `${g.nations[by].adj} troops close round ${townName(r)}: ${placeWords(g, r)} is under siege, with ${who}.`, r.ctrl, [by, r.ctrl, ...inside.map((c) => c.nat)]);
}

/** The besiegers are gone: the siege is raised. What was broken is mended month by month. */
export function lift(g: Game, r: Region, by: string | null = null): void {
  const key = `siege:${r.id}`;
  if (g.flags[key] === undefined) return;
  const lasted = g.turn - g.flags[key];
  delete g.flags[key];
  delete g.flags[`sgm:${r.id}`];
  g.supplyDirty = true;
  tallySiege(g, 'raised');
  tell(g, r, `The siege of ${townName(r)} is raised${lasted >= 1 ? ` after ${months(lasted)}` : ''}${by ? ` by ${g.nations[by].adj} troops` : ''}.`, r.ctrl, [r.ctrl, ...(by ? [by] : []), ...garrisonOf(r).map((c) => c.nat)]);
}

export type Fell = 'storm' | 'gates' | 'hunger' | 'empty';

/** The town falls, and the province with it. Anyone still inside lays down his arms. */
export function fall(g: Game, r: Region, by: string, how: Fell): void {
  const since = g.flags[`siege:${r.id}`];
  const lasted = since === undefined ? 0 : g.turn - since;
  const old = r.ctrl;
  tallySiege(g, how);
  if (how !== 'empty') tallySiege(g, 'months', lasted);
  tallySiege(g, 'taken', garrisonOf(r).length);
  for (const c of [...garrisonOf(r)]) {
    storyFallen(g, c, 'surrendered', by, r.id);
    removeCorps(g, c);
  }
  delete g.flags[`siege:${r.id}`];
  delete g.flags[`sgm:${r.id}`];
  delete g.flags[`guard:${r.id}`];
  delete g.flags[`storm:${r.id}`];
  delete g.flags[`sally:${r.id}`];
  const adj = g.nations[by].adj;
  // A capital's fall is told by the rules that take it; the rest is told here.
  const cap = r.cap && r.owner === old;
  if (!cap && how !== 'empty' && how !== 'storm') {
    const text = how === 'gates' ? `${townName(r)} opens its gates to ${adj} troops without a fight.`
      : `Starved out after a siege of ${months(lasted)}, ${townName(r)} surrenders to ${adj} forces.`;
    tell(g, r, text, old, [by, old]);
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
  if (guardOf(g, r) > 0 && (SIEGE.summonsNot.includes(townSize(g, r)) || assess(g, force, r, { town: true }).odds < SIEGE.summons)) return false;
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
  if (!stormers(g, r, nat).length) return 'No corps round the town is fit to storm it.';
  return null;
}

/**
 * The corps that go into an assault, out of those offered. An army round a town comes at it from every side, so
 * the width of the ground does not hold any back: only the size of battle its staff can run does. Engineers go in
 * with the infantry to breach the works.
 */
export function stormForce(g: Game, corps: Corps[], r: Region): Corps[] {
  const fit = corps.filter((c) => !c.dead && c.reg === r.id && !c.town && c.kind !== 'fort' && atWar(g, c.nat, r.ctrl));
  const line = fit.filter((c) => c.kind !== 'eng').sort((x, y) => corpsBase(g, y) * KIND[y.kind].atk - corpsBase(g, x) * KIND[x.kind].atk);
  if (!line.length) return [];
  const most = g.nations[line[0].nat].maxAttack || 99;
  return [...line.slice(0, most), ...fit.filter((c) => c.kind === 'eng')];
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
  if (!salliers(g, r, nat).length) return 'The garrison is too shaken to come out.';
  return null;
}

/** The odds of a sortie, with its Battle Ticket. */
export function previewSally(g: Game, r: Region, nat: string): Assessment | null {
  const going = salliers(g, r, nat);
  return going.length ? assess(g, going, r, { sally: true }) : null;
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
    delete g.flags[`sgm:${r.id}`];
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
  }
}

// ---- The month ----------------------------------------------------------------------------------

/** The weight of guns round a town: the besiegers' artillery divisions. */
const guns = (cs: Corps[]): number => cs.reduce((s, c) => s + (c.kind === 'art' ? c.divs * c.str * (c.supplied ? 1 : 0.5) : 0), 0);

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
    const foes = besiegers(g, r);
    if (!foes.length) {
      if (g.flags[`siege:${r.id}`] !== undefined) lift(g, r);
      // Left alone, the walls are mended and the guard fills up again.
      const b = g.flags[`breach:${r.id}`];
      if (b !== undefined) { if (b <= SIEGE.breach.mend) delete g.flags[`breach:${r.id}`]; else g.flags[`breach:${r.id}`] = b - SIEGE.breach.mend; }
      if (g.flags[`guard:${r.id}`] !== undefined) delete g.flags[`guard:${r.id}`];
      continue;
    }
    const by = armyOf(foes[0]);
    if (g.flags[`siege:${r.id}`] === undefined) invest(g, r, by);
    if (!townHeld(g, r)) { fall(g, r, by, 'empty'); continue; }
    const weight = guns(foes);
    if (weight > 0) {
      breach(g, r, SIEGE.breach.gun * weight);
      // Shellfire wears on the men inside too.
      for (const c of garrisonOf(r)) c.org = Math.max(0.15, c.org - 0.03 * Math.min(4, weight));
    }
    if (ringClosed(g, r) && !fedBySea(g, r)) g.flags[`sgm:${r.id}`] = monthsShut(g, r) + 1;
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
    if (!r.units.length || townHeld(g, r)) continue;
    const by = besiegerOf(g, r);
    if (by && g.nations[by]?.alive) fall(g, r, by, 'empty');
  }
}

/** Marks which garrisons still eat. Called whenever the supply picture is drawn. */
export function siegeSupply(g: Game): void {
  if (!siegeRules(g)) return;
  for (const r of g.regions) {
    if (!r.garr || !r.garr.length || !besieged(g, r)) continue;
    const fed = garrisonFed(g, r);
    for (const c of r.garr) c.supplied = fed;
  }
}

/** True for a corps shut in a besieged town: no drafts reach it. */
export const shutIn = (g: Game, c: Corps): boolean => !!c.town && besieged(g, g.regions[c.reg]) && !fedBySea(g, g.regions[c.reg]);

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

/** What bombs or shells of this weight would do to a town. Nothing is changed. */
export function planTownHit(g: Game, r: Region, kind: 'raid' | 'shell', share: number): TownHit {
  const B = SIEGE.bombard, s = clamp(share, 0, 1);
  const now = g.flags[`breach:${r.id}`] ?? 0;
  const works = Math.max(0, Math.min(SIEGE.breach.most - now, SIEGE.breach[kind] * s));
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
  else out.push(hit.share > 0 && (g.flags[`breach:${r.id}`] ?? 0) >= SIEGE.breach.most - 0.5
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
  /** Months of stores when the gates shut, months left, whether ships still feed it, and whether the ring is closed. */
  stores: number; left: number; bySea: boolean; closed: boolean;
  /** How much stronger its defenders fight inside (1.45 is 45% stronger), and the share of its works broken down. */
  bonus: number; broken: number;
  /** Corps the town can hold and corps inside, and the town's own guard in divisions' worth. */
  room: number; inside: number; guard: number;
  /** Plain words for a panel: what the place is, who holds it, how the siege stands. */
  lines: string[];
}

/** The town of a province as a fortress: what it holds, what it adds, and how its siege stands. Null outside the grand campaign. */
export function siegeView(g: Game, region: number): SiegeView | null {
  const r = g.regions[region];
  if (!r || !siegeRules(g)) return null;
  const size = townSize(g, r), by = besiegerOf(g, r);
  const stores = storesOf(g, r), shut = monthsShut(g, r), bySea = fedBySea(g, r);
  const inside = garrisonOf(r).length, guard = guardOf(g, r), bonus = townBonus(g, r), broken = breachOf(g, r);
  const left = Math.max(0, stores - shut);
  const lines: string[] = [];
  const pct = Math.round((bonus - 1) * 100);
  lines.push(`Troops inside ${townName(r)} fight ${pct}% stronger${broken > 0 ? `: bombardment has broken down ${Math.round(broken * 100)}% of its works` : ''}. It has room for ${roomIn(g, r) === 1 ? 'one corps' : `${roomIn(g, r)} corps`}.`);
  if (guard > 0) lines.push(`Its own guard of depot troops, police and militia is worth ${guard >= 0.95 ? `${guard.toFixed(1)} divisions` : 'part of a division'}. It holds the town while no army does.`);
  if (by) {
    const closed = ringClosed(g, r);
    lines.push(bySea ? 'Ships still reach the harbour: the town cannot be starved while they do.'
      : !closed ? 'The ring is not closed: a road out is still open, and the army round it is too weak to shut it in alone. Its stores are not running down. Take the ground next door, or bring more troops up beside it.'
      : left > 0 ? `Closely invested. Its stores will feed the defenders ${left === 1 ? 'one more month' : `${left} more months`}; after that they go hungry.`
      : inside ? 'Its stores are gone: the garrison is starving, and loses men and heart every month.' : 'Its stores are gone: it is about to give in.');
    // A capital shut in: the government loses its hold on the country month by month.
    if (closed && r.cap && r.owner === r.ctrl) {
      const sat = Math.max(0, g.turn - investedOn(g, r));
      lines.push(sat >= SIEGE.capitalYear ? `Shut in for ${months(sat)}, the government has lost its hold on the country: the capital counts as lost when it reckons whether to fight on.`
        : `A government shut in its capital loses its hold on the country month by month. After ${months(SIEGE.capitalYear)} of siege the capital counts as lost, whether it has fallen or not.`);
    }
  } else {
    lines.push(`Closely invested, its stores would feed the defenders for ${months(stores)}${r.port ? ', and for as long as ships can reach its harbour' : ''}.`);
  }
  return {
    region, town: townName(r), size, sizeWord: TOWNS.words[size], holder: r.ctrl, by, since: investedOn(g, r), months: shut,
    stores, left, bySea, closed: by ? ringClosed(g, r) : false, bonus, broken, room: roomIn(g, r), inside, guard, lines,
  };
}

/** Every town under siege, for a map to mark. */
export function siegesNow(g: Game): SiegeView[] {
  if (!siegeRules(g)) return [];
  const out: SiegeView[] = [];
  for (const r of g.regions) if (besieged(g, r)) out.push(siegeView(g, r.id)!);
  return out;
}

/** The men in a garrison, for a report line. */
export const garrisonMen = (r: Region): number => garrisonOf(r).reduce((s, c) => s + men(c), 0);
