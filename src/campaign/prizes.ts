// Prizes outside the town (the grand campaign).
//
// The town is the key to its province, but not everything a province is worth lies inside its walls. Its oilfield,
// its factory district, its airfield, its harbour and its great rail junction are places of their own. Those that
// lie outside the town go with the countryside: the army that wins the field has the use of them once it has held
// them a month, while the town still holds. A harbour lies inside the defences of a city, a capital or a fortress,
// and goes with the town; the quays of a smaller place lie outside it.
//
// A prize can be wrecked by the army that holds it: by order, which is a corps' work for the month and a thorough
// job, or as that army is driven off the ground, if its nation has ordered scorched earth there. A wrecked prize
// gives nobody anything for months. Time mends it, and engineers standing in the province mend it faster. A junction
// is cut only by order: when ground is lost the railway keeps the rule it always had (rails.ts).
//
// The numbers are in data/campaign/prizes.json. The historical scenarios have none of this. Both interfaces read
// prizesAt() for a province's panel and give the orders through turn.ts.

import data from '../../data/campaign/prizes.json';
import { Rng, hashString } from '../core/rng';
import { KIND, type Corps, type Game, type Region } from './types';
import { airWar, airfieldsHold } from './air';
import { BUDGET, PRODUCTION } from './economy';
import { allied, atWar, headline, theName } from './game';
import { armyOf } from './joint';
import { cutRail, railCut, railsAt, ruinedAt } from './rails';
import { fieldOut, fireField } from './resources';
import { besieged, fieldCtrl, garrisonOf, siegeRules } from './siege';
import { townName, townSize, type TownSize } from './towns';

export type PrizeKind = 'oil' | 'works' | 'airfield' | 'harbour' | 'junction';
export const PRIZE_KINDS: PrizeKind[] = ['oil', 'works', 'airfield', 'harbour', 'junction'];
/** Scorched earth: never, on ground that is not the nation's own or an ally's, or everywhere. */
export type ScorchPolicy = 'never' | 'conquered' | 'everywhere';
const POLICIES: ScorchPolicy[] = ['never', 'conquered', 'everywhere'];

export const PRIZES = data as unknown as {
  names: Record<PrizeKind, string>;
  works: { least: number; outside: number };
  junction: { lines: number };
  harbourInside: TownSize[];
  settle: number;
  ap: number;
  wreck: Record<Exclude<PrizeKind, 'junction'>, number>;
  burn: { chance: number; unguarded: number; fastCaptor: number; months: Record<Exclude<PrizeKind, 'junction'>, number> };
  mend: number;
  ai: { policy: Record<string, ScorchPolicy> };
};

/** True when the prizes are in force: the grand campaign, where the town is the key to its province. */
export const prizeRules = (g: Game): boolean => g.flags.open === 1 && siegeRules(g);

// ---- What a province has, and where it lies ----------------------------------------------------

/** How many of a province's factories stand outside the walls of its town. */
export const worksOutside = (r: Region): number => (r.fac >= PRIZES.works.least ? Math.floor(r.fac * PRIZES.works.outside) : 0);

/** True where a province has a prize of this kind. */
export function hasPrize(g: Game, r: Region, kind: PrizeKind): boolean {
  if (!prizeRules(g)) return false;
  if (kind === 'oil') return r.oil;
  if (kind === 'works') return worksOutside(r) > 0;
  if (kind === 'airfield') return airWar(g) && airfieldsHold(g, r.id) > 0;
  if (kind === 'harbour') return r.port && r.seas.length > 0;
  return railsAt(r.id).length >= PRIZES.junction.lines;
}

/** True for a prize that lies inside the town's defences, and goes with the town: the harbour of a city, a capital or a fortress. */
export const prizeInside = (g: Game, r: Region, kind: PrizeKind): boolean => kind === 'harbour' && (!!r.fort || PRIZES.harbourInside.includes(townSize(g, r)));

/** The month a wrecked prize is back in use, by kind. A junction's line has no such month: engineers relay it. */
const outKey = (r: Region, kind: PrizeKind): string => `pzo:${r.id}:${kind}`;

/**
 * Months a prize is still out of use: 0 for one at work. A junction whose line is cut says 99: it stays cut until
 * engineers relay it.
 */
export function prizeOut(g: Game, r: Region, kind: PrizeKind): number {
  if (!hasPrize(g, r, kind)) return 0;
  if (kind === 'junction') return railCut(g, r.id) ? 99 : 0;
  if (kind === 'oil') return fieldOut(g, r) > 0 ? Math.max(1, (g.flags[`oilout:${r.id}`] ?? 0) - g.turn) : 0;
  return Math.max(0, (g.flags[outKey(r, kind)] ?? 0) - g.turn);
}

/** The factories of a province's district that stand idle because the district was wrecked. */
export function worksBurnt(g: Game, r: Region): number {
  if ((g.flags[`pzo:${r.id}:works`] ?? 0) <= g.turn) return 0;
  return Math.max(0, Math.min(worksOutside(r), r.fac - ruinedAt(g, r)));
}

/** The nation whose army holds a prize now: the town's holder, or whoever holds the field round it for a prize outside the walls. */
export function prizeHolder(g: Game, r: Region, kind: PrizeKind): string {
  return prizeRules(g) && besieged(g, r) && !prizeInside(g, r, kind) ? fieldCtrl(g, r) : r.ctrl;
}

/** True while a prize outside the walls has just changed hands with the field, and works for nobody yet. */
export function prizeSettling(g: Game, r: Region, kind: PrizeKind): boolean {
  if (!prizeRules(g) || !besieged(g, r) || prizeInside(g, r, kind)) return false;
  const since = g.flags[`siege:${r.id}`];
  return since === undefined || g.turn - since < PRIZES.settle;
}

/**
 * The nation that has the use of a prize this month, or null while nobody has: it is wrecked, or the army that took
 * the field has not yet held it long enough. Outside the grand campaign everything goes with the province.
 */
export function prizeUser(g: Game, r: Region, kind: PrizeKind): string | null {
  if (!prizeRules(g)) return r.ctrl;
  if (!hasPrize(g, r, kind) || prizeOut(g, r, kind) > 0 || prizeSettling(g, r, kind)) return null;
  return prizeHolder(g, r, kind);
}

/**
 * The nation a prize answers to once it has settled, wrecked or not: the town's holder, or the army that has held
 * the field round it long enough. Null in the month it changes hands. What it yields while wrecked is another matter.
 */
export const prizeTaker = (g: Game, r: Region, kind: PrizeKind): string | null => (prizeSettling(g, r, kind) ? null : prizeHolder(g, r, kind));

/** True where this nation, or an ally, has the use of a province's airfield: its own ground with the roads open, or a field its side holds round an enemy town. */
export function airfieldOpenTo(g: Game, nat: string, r: Region): boolean {
  if (!hasPrize(g, r, 'airfield')) return allied(g, nat, r.ctrl) && !besieged(g, r);
  const user = prizeUser(g, r, 'airfield');
  return user !== null && allied(g, nat, user);
}

/** True where this nation's trains run through a province under siege: its side holds the field and the great junction outside the town. */
export function junctionOpenTo(g: Game, nat: string, r: Region): boolean {
  const user = prizeRules(g) && besieged(g, r) ? prizeUser(g, r, 'junction') : null;
  return user !== null && allied(g, nat, user);
}

/** True while ships can use a province's harbour for its holder: it has one, it is not wrecked, and it is not in an enemy's hands. */
export function harbourOpen(g: Game, r: Region): boolean {
  if (!r.port) return false;
  if (!hasPrize(g, r, 'harbour')) return true;
  return prizeUser(g, r, 'harbour') === r.ctrl;
}

// ---- What a prize is worth ---------------------------------------------------------------------

/** What a prize outside a besieged town pays the army that holds it, in money a month. */
function prizeMoney(g: Game, r: Region, kind: PrizeKind, nat: string): number {
  const own = r.owner === nat;
  const k = own ? 1 : BUDGET.occupied;
  if (kind === 'works') return Math.max(0, Math.min(worksOutside(r), r.fac - ruinedAt(g, r))) * (own ? 1 : PRODUCTION.occupiedIncome);
  if (kind === 'oil') return BUDGET.oil * k;
  if (kind === 'harbour') return BUDGET.port * k;
  return 0;
}

export interface PrizeIncome { works: number; oil: number; harbour: number; total: number; count: number }

/** What a nation earns from the prizes its armies hold outside towns they have not yet taken. */
export function prizeIncome(g: Game, nat: string): PrizeIncome {
  const out: PrizeIncome = { works: 0, oil: 0, harbour: 0, total: 0, count: 0 };
  if (!prizeRules(g)) return out;
  for (const r of g.regions) {
    if (r.ctrl === nat || !r.units.length || !besieged(g, r) || fieldCtrl(g, r) !== nat) continue;
    for (const kind of ['works', 'oil', 'harbour'] as const) {
      if (prizeUser(g, r, kind) !== nat) continue;
      const m = prizeMoney(g, r, kind, nat);
      out[kind] += m;
      out.total += m;
      out.count++;
    }
  }
  return out;
}

// ---- Wrecking ----------------------------------------------------------------------------------

const monthsWord = (n: number): string => (n <= 1 ? 'a month' : `${n} months`);

/** Puts a prize out of use for so many months. False where there is none, or it is out already. */
export function wreckPrize(g: Game, r: Region, kind: PrizeKind, months: number): boolean {
  if (!hasPrize(g, r, kind) || prizeOut(g, r, kind) > 0) return false;
  if (kind === 'oil') return fireField(g, r, months, 1);
  if (kind === 'junction') return cutRail(g, r.id);
  g.flags[outKey(r, kind)] = g.turn + months;
  g.supplyDirty = true;
  return true;
}

/** The corps of a nation that stand where a prize is: in the field for one outside the walls, in the town or its field for one inside. */
function handsAt(g: Game, nat: string, r: Region, kind: PrizeKind): Corps[] {
  const field = r.units.filter((u) => !u.dead && u.nat === nat);
  return prizeInside(g, r, kind) || !besieged(g, r) ? [...field, ...garrisonOf(r).filter((u) => !u.dead && u.nat === nat)] : field;
}

/** Why this nation cannot wreck a prize by order, or null if it can. */
export function whyNotWreck(g: Game, nat: string, region: number, kind: PrizeKind): string | null {
  if (!prizeRules(g)) return 'Prizes are wrecked only in the grand campaign.';
  const r = g.regions[region];
  if (!r) return 'Pick a province.';
  const name = PRIZES.names[kind].toLowerCase();
  if (!hasPrize(g, r, kind)) return `${r.name} has no ${name}.`;
  if (prizeOut(g, r, kind) > 0) return kind === 'junction' ? `The line at ${r.name} is already cut.` : `The ${name} at ${r.name} is already wrecked.`;
  if (!g.nations[nat]?.atWar.length) return 'Nobody wrecks what they hold in peacetime.';
  if (!allied(g, nat, prizeHolder(g, r, kind))) return `The ${name} at ${r.name} is not in your hands.`;
  const hands = handsAt(g, nat, r, kind);
  if (!hands.length) return `None of your troops stand at the ${name} of ${r.name}.`;
  if (!hands.some((c) => c.ap >= PRIZES.ap && c.leave === undefined)) return 'The troops there have no marching time left this month: it is a month\'s work.';
  return null;
}

const wreckWords = (g: Game, r: Region, kind: PrizeKind, months: number): string => (kind === 'oil' ? `set the oilfields of ${r.name} alight: they will burn for ${months} months`
  : kind === 'works' ? `blow up the factory district of ${r.name}: its ${worksOutside(r) === 1 ? 'factory stands' : `${worksOutside(r)} factories stand`} idle for ${months} months`
  : kind === 'airfield' ? `plough up the airfield at ${r.name}: no aircraft will fly from it for ${months} months`
  : kind === 'harbour' ? `block the harbour of ${townName(r)} and blow up its quays: no ship will use it for ${months} months`
  : `blow up the rail junction at ${r.name}: no train runs through until engineers relay the line`);

/** News of a prize wrecked is for the player when it is his, his ally's, or his enemy's doing against him; a great prize is news to everyone. */
function tellPrize(g: Game, r: Region, text: string, nat: string, parties: string[]): void {
  const mine = g.human !== null && parties.some((p) => p === g.human || allied(g, p, g.human!));
  if (mine || r.oil || r.cap || r.fac >= 4) headline(g, 'battle', text, nat, r.id);
}

/** Wrecks a prize by order of the nation whose troops hold it: a corps' work for the month. */
export function orderWreckPrize(g: Game, nat: string, region: number, kind: PrizeKind): boolean {
  if (whyNotWreck(g, nat, region, kind)) return false;
  const r = g.regions[region];
  const months = kind === 'junction' ? 0 : PRIZES.wreck[kind];
  // Engineers do it for choice; otherwise the corps with the most of its month left.
  const c = handsAt(g, nat, r, kind).filter((u) => u.ap >= PRIZES.ap && u.leave === undefined).sort((a, b) => (b.kind === 'eng' ? 1 : 0) - (a.kind === 'eng' ? 1 : 0) || b.ap - a.ap)[0];
  if (!wreckPrize(g, r, kind, months)) return false;
  c.ap = 0;
  tellPrize(g, r, `${g.nations[nat].adj} troops ${wreckWords(g, r, kind, months)}.`, nat, [nat, r.ctrl, r.owner]);
  return true;
}

// ---- Scorched earth ----------------------------------------------------------------------------

/** A nation's scorched earth policy. The player's is 'never' until he says otherwise; a computer nation's is its own. */
export function scorchPolicy(g: Game, nat: string): ScorchPolicy {
  const set = g.flags[`scorch:${nat}`];
  if (set !== undefined) return POLICIES[set] ?? 'never';
  return nat === g.human ? 'never' : PRIZES.ai.policy[nat] ?? PRIZES.ai.policy._default;
}

export function setScorchPolicy(g: Game, nat: string, policy: ScorchPolicy): void {
  g.flags[`scorch:${nat}`] = POLICIES.indexOf(policy);
}

/** The order a nation has given for one province: 1 burn it if it is lost, -1 leave it whole, 0 follow the policy. */
export const burnOrder = (g: Game, nat: string, r: Region): number => g.flags[`burn:${r.id}:${nat}`] ?? 0;

export function setBurnOrder(g: Game, nat: string, r: Region, order: number): void {
  if (order) g.flags[`burn:${r.id}:${nat}`] = order > 0 ? 1 : -1; else delete g.flags[`burn:${r.id}:${nat}`];
}

/** True when this nation's troops will burn the prizes of a province as they are driven off it. */
export function willBurn(g: Game, nat: string, r: Region): boolean {
  if (!prizeRules(g)) return false;
  const order = burnOrder(g, nat, r);
  if (order) return order > 0;
  const p = scorchPolicy(g, nat);
  return p === 'everywhere' || (p === 'conquered' && r.owner !== nat && !allied(g, nat, r.owner));
}

/** The chance each prize is burned as the ground is lost: whether the loser still has troops at hand, and who took it. */
export function burnChance(g: Game, r: Region, by: string, from: string): number {
  const B = PRIZES.burn;
  const near = garrisonOf(r).some((c) => !c.dead && allied(g, c.nat, from))
    || r.nb.some((j) => g.regions[j].units.some((u) => !u.dead && allied(g, u.nat, from) && atWar(g, u.nat, by)));
  let p = near ? B.chance : B.unguarded;
  // Fast troops and airborne troops are on the wells and the quays before the demolition parties have finished.
  if (r.units.some((u) => !u.dead && allied(g, armyOf(u), by) && (KIND[u.kind].mobile || u.airborne))) p -= B.fastCaptor;
  return Math.max(0.05, Math.min(0.95, p));
}

/**
 * Ground has been lost in war: the field of a province whose town still holds ('outside'), a town that falls after
 * a siege ('inside': what lay outside went with the field), or a whole province at once ('all'). If the side driven
 * off has ordered scorched earth there, it burns what it can as it goes. The dice are the province's own for the
 * month, so nothing else in the war shifts.
 */
export function prizesOnLoss(g: Game, r: Region, by: string, from: string, which: 'outside' | 'inside' | 'all'): PrizeKind[] {
  const burnt: PrizeKind[] = [];
  if (!prizeRules(g) || !g.nations[from] || !atWar(g, by, from) || !willBurn(g, from, r)) return burnt;
  const p = burnChance(g, r, by, from);
  for (const kind of PRIZE_KINDS) {
    if (!hasPrize(g, r, kind) || prizeOut(g, r, kind) > 0) continue;
    const inside = prizeInside(g, r, kind);
    if ((which === 'outside' && inside) || (which === 'inside' && !inside)) continue;
    // The railway keeps the rule it always had: torn up, or not, as the province falls (rails.ts), or by order.
    if (kind === 'junction') continue;
    const roll = new Rng(hashString(`prize:${g.seed}:${g.turn}:${r.id}:${kind}`)).next();
    if (roll < p && wreckPrize(g, r, kind, PRIZES.burn.months[kind])) burnt.push(kind);
  }
  if (burnt.length) {
    const list = listed(burnt);
    tellPrize(g, r, `Scorched earth: ${g.nations[from].adj} troops wreck ${list} at ${r.name} as they are driven off. ${theName(g.nations[by], true)} will get nothing from ${burnt.length === 1 && burnt[0] !== 'oil' ? 'it' : 'them'} for months.`, from, [by, from, r.owner]);
  }
  return burnt;
}

const PLACE_WORDS: Record<PrizeKind, string> = { oil: 'the oilfields', works: 'the factory district', airfield: 'the airfield', harbour: 'the harbour', junction: 'the rail junction' };
const listed = (kinds: PrizeKind[]): string => { const w = kinds.map((k) => PLACE_WORDS[k]); return w.length === 1 ? w[0] : `${w.slice(0, -1).join(', ')} and ${w[w.length - 1]}`; };

/**
 * News for the player when the places outside a town change hands whole with the field: his army's gain, his
 * ally's, or his own loss. An airfield alone is not news: nearly every province has one.
 */
export function tellSeized(g: Game, r: Region, by: string): void {
  if (!prizeRules(g) || g.human === null || !g.nations[by]) return;
  if (![by, r.ctrl].some((p) => p === g.human || allied(g, p, g.human!))) return;
  const got = PRIZE_KINDS.filter((k) => hasPrize(g, r, k) && !prizeInside(g, r, k) && prizeOut(g, r, k) <= 0);
  if (!got.some((k) => k !== 'airfield')) return;
  const words = listed(got);
  const many = got.length > 1 || got[0] === 'oil';
  headline(g, 'info', `${words[0].toUpperCase()}${words.slice(1)} outside ${townName(r)} ${many ? 'are' : 'is'} in ${g.nations[by].adj} hands, though the town holds. ${many ? 'They work' : 'It works'} for ${theName(g.nations[by])} after ${monthsWord(PRIZES.settle)}.`, by, r.id);
}

/** Engineers standing in a province mend its wrecked prizes faster: called as their nation's month closes. */
export function mendPrizes(g: Game, nat: string): void {
  if (!prizeRules(g) || PRIZES.mend <= 0) return;
  for (const r of g.regions) {
    if (!allied(g, nat, fieldCtrl(g, r)) || !r.units.some((u) => !u.dead && u.nat === nat && u.kind === 'eng' && u.leave === undefined)) continue;
    for (const kind of ['works', 'airfield', 'harbour'] as const) {
      const key = outKey(r, kind);
      if ((g.flags[key] ?? 0) > g.turn + 1) g.flags[key] -= PRIZES.mend;
    }
    if ((g.flags[`oilout:${r.id}`] ?? 0) > g.turn + 1) g.flags[`oilout:${r.id}`] -= PRIZES.mend;
  }
}

// ---- What a panel reads ------------------------------------------------------------------------

export interface PrizeView {
  kind: PrizeKind;
  name: string;
  /** True for a prize inside the town's defences: it goes with the town. */
  inside: boolean;
  /** The nation whose army holds it now. */
  holder: string;
  /** The nation that has the use of it this month, or null. */
  user: string | null;
  /** Months it is still out of use; 99 for a junction whose line is cut. */
  out: number;
  /** True while it has just changed hands with the field and works for nobody yet. */
  settling: boolean;
  /** What it is, in a few words: "2 of 4 factories outside the walls", "room for 600 aircraft". */
  detail: string;
  /** One sentence on how it stands. */
  state: string;
}

export interface PrizesView {
  region: number;
  prizes: PrizeView[];
  /** What the province's holder has ordered if it is lost: 1 burn, -1 leave whole, 0 by policy; and what that comes to. */
  order: number;
  burns: boolean;
  policy: ScorchPolicy;
}

/** The prizes of a province, for a panel. Null outside the grand campaign or where the province has none. `nat` is whose orders are shown. */
export function prizesAt(g: Game, region: number, nat: string | null = g.human): PrizesView | null {
  const r = g.regions[region];
  if (!r || !prizeRules(g)) return null;
  const prizes: PrizeView[] = [];
  for (const kind of PRIZE_KINDS) {
    if (!hasPrize(g, r, kind)) continue;
    const inside = prizeInside(g, r, kind), holder = prizeHolder(g, r, kind), out = prizeOut(g, r, kind), settling = prizeSettling(g, r, kind);
    const n = worksOutside(r);
    const detail = kind === 'works' ? `${n} of ${r.fac} ${r.fac === 1 ? 'factory' : 'factories'} outside the walls`
      : kind === 'airfield' ? `room for ${airfieldsHold(g, r.id).toLocaleString('en-US')} aircraft`
      : kind === 'junction' ? `${railsAt(r.id).length} lines meet here`
      : kind === 'harbour' ? (inside ? 'inside the defences: it goes with the town' : 'the quays lie outside the town')
      : 'wells and refineries';
    const who = g.nations[holder]?.adj ?? holder;
    const state = out >= 99 ? 'The line is cut until engineers relay it.'
      : out > 0 ? `${kind === 'oil' ? 'Burning' : 'Wrecked'}: out of use for ${monthsWord(out)} more.`
      : settling ? `Taken by ${who} troops this month: it works for them from next month.`
      : holder !== r.ctrl ? `In ${who} hands with the country round the town.`
      : besieged(g, r) && inside ? 'Held with the town, and still in use.'
      : 'At work.';
    prizes.push({ kind, name: PRIZES.names[kind], inside, holder, user: prizeUser(g, r, kind), out, settling, detail, state });
  }
  if (!prizes.length) return null;
  const who = nat ?? r.ctrl;
  return { region, prizes, order: burnOrder(g, who, r), burns: willBurn(g, who, r), policy: scorchPolicy(g, who) };
}

export interface PrizeMark {
  region: number;
  /** The army that holds prizes outside a town it has not taken, or null. */
  by: string | null;
  /** The prizes there that are wrecked and still out of use. A junction whose line is cut is not listed: a map draws its cut railway. */
  burning: PrizeKind[];
  /** The prizes there in the hands of the army round the town. */
  seized: PrizeKind[];
}

/** Every province where a prize is wrecked or in the hands of an army that has not taken the town: for a map's marks. */
export function prizeMarks(g: Game): PrizeMark[] {
  const out: PrizeMark[] = [];
  if (!prizeRules(g)) return out;
  for (const r of g.regions) {
    const burning: PrizeKind[] = [], seized: PrizeKind[] = [];
    for (const kind of PRIZE_KINDS) {
      if (!hasPrize(g, r, kind)) continue;
      if (kind !== 'junction' && prizeOut(g, r, kind) > 0) burning.push(kind);
      if (prizeHolder(g, r, kind) !== r.ctrl) seized.push(kind);
    }
    if (burning.length || seized.length) out.push({ region: r.id, by: seized.length ? fieldCtrl(g, r) : null, burning, seized });
  }
  return out;
}
