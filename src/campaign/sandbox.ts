// The sandbox at sea (Brian, 19 November 2026: "a sandbox to play and make your own history, not just play out the
// real one"; design paper https://claude.ai/artifact/LV5ZA8vXKtyuDvjRbazxhc). With the switch on (classes.ts), the
// Navy column of the arsenal offers, beside each kind's standard ship, the classes a nation may build of that kind:
// the what-if classes it has researched and the classes it has drawn itself. Every ship laid down to one of them
// joins her fleet's list by name (shiplist.ts). A ship in harbour can be refitted. And the drawing board prices a
// design of the player's own from six dials, so that no design beats the standard ship of its kind for the money
// by more than a tenth.
//
// With the switch off none of this is offered and no fleet ever has a list: the war is the one it always was.
//
// The numbers are in data/campaign/sandbox-ships.json and in the ship rows of classes.json.

import { ARSENAL, isCity, joinsFleet, slipSite, yardLevel, type Hull } from './arsenal';
import { BUDGET, isHome } from './economy';
import { CLASS_ROWS, classById, classLocked, classRule, classesFor, computerDesigns, sandbox, stampSteel, whatifShare, type ClassRow } from './classes';
import { headline } from './game';
import { listed, standard, trim, writeFallen } from './shiplist';
import { NAVY, foundFleet } from './navy';
import { onPlans, whyNoPlans } from './plans';
import { SANDBOX_SHIPS, type RefitDef, type Style } from './sandbox-data';
import { locked, techById, techOf } from './tech';
import { shipNick } from './reputation';
import { priceOf } from './traits';
import { Rng, hashString } from '../core/rng';
import type { Build, Fleet, Game, ShipClass, ShipHull } from './types';

const S = SANDBOX_SHIPS;
const KINDS: ShipClass[] = ['dd', 'ss', 'ca', 'cv', 'bb'];
type Stat = 'power' | 'tough' | 'escort' | 'raid';
const STATS: Stat[] = ['power', 'tough', 'escort', 'raid'];
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;
const num = (x: unknown, d = 0): number => (typeof x === 'number' && Number.isFinite(x) ? x : d);

/** The ship classes of a nation: those of classes.json (what-if) and its own designs, of one kind or of all. */
export const shipClassRows = (g: Game, nat: string, kind?: ShipClass): ClassRow[] =>
  (kind ? classesFor(g, nat, kind) : KINDS.flatMap((k) => classesFor(g, nat, k)));

/** What a ship of a class costs, takes, holds and fights with, her kind's standard ship filling what the row leaves out. */
export function classSpec(row: ClassRow): { kind: ShipClass; cost: number; months: number; yard: number; count: number } & Record<Stat, number> & { deck?: number; aa?: number; eyes?: number; oil?: number } {
  const kind = row.of as ShipClass;
  const std = ARSENAL.ships[kind as Hull], fight = NAVY.classes[kind];
  return {
    kind, cost: num(row.cost, std.cost), months: num(row.months, std.months), yard: num(row.yard, std.yard), count: num(row.count, std.count),
    power: num(row.power, fight.power), tough: num(row.tough, fight.tough), escort: num(row.escort, fight.escort), raid: num(row.raid, fight.raid),
    ...(row.deck !== undefined ? { deck: num(row.deck) } : {}), ...(row.aa !== undefined ? { aa: num(row.aa) } : {}),
    ...(row.eyes !== undefined ? { eyes: num(row.eyes) } : {}), ...(row.oil !== undefined ? { oil: num(row.oil) } : {}),
  };
}

// ---- The arsenal's Navy column -------------------------------------------------------------------------------

/** An offer of the arsenal, as arsenal.ts lists them (its type, without the import cycle). */
export interface ClassOffer {
  key: string; group: 'navy'; cls: string; model?: string; label: string; blurb: string; size: string; cost: number; months: number; upkeep: number; at: number; note: string; why: string | null; tech?: string;
}

/**
 * The Navy column's lines for the classes of a nation (the sandbox): one a class, keyed 'ship:<kind>:<class>', under
 * its kind's standard line. A what-if class waits on its technology; a small nation's battleship and carrier
 * classes wait on plans (ruling 235), as its standard ones do.
 */
export function shipClassOffers(g: Game, nat: string, sel: number, only?: ShipClass): ClassOffer[] {
  if (!sandbox(g)) return [];
  const n = g.nations[nat];
  const out: ClassOffer[] = [];
  for (const kind of only ? [only] : KINDS) {
    for (const row of classesFor(g, nat, kind)) {
      const c = classSpec(row);
      const at = slipSite(g, nat, c.yard, sel);
      const fleet = at >= 0 ? joinsFleet(g, nat, kind, at) : null;
      const cost = priceOf(g, nat, `ship:${kind}`, c.cost);
      // A class waits on its own technology, and on its kind's (a carrier class on carriers), as the standard line does.
      const plans = whyNoPlans(g, nat, kind);
      const held = classLocked(g, nat, row) ?? (onPlans(g, nat, kind) ? null : locked(g, nat, `ship:${kind}`));
      const one = row.label.toLowerCase();
      out.push({
        key: `ship:${kind}:${row.id}`, group: 'navy', cls: row.id, model: row.model, label: row.label, blurb: `${row.blurb ?? ''} ${classRule(row)}`.trim(),
        size: c.count === 1 ? `one ${one}` : `a flotilla of ${plural(c.count, one)}`, cost, months: c.months,
        upkeep: c.count * BUDGET.upkeep.ships[kind], at,
        note: at < 0 ? '' : `Laid down at ${g.regions[at].name}. ${fleet ? `Joins the ${fleet.name}.` : 'Forms a new fleet there.'} She counts as a ${NAVY.classes[kind].one}.`,
        why: plans ?? (held ? `Needs the technology ${held.name}. Open Research to begin it.` : null) ?? (at < 0 ? `Needs a shipyard of level ${c.yard} in one of your home ports.` : n.pp < cost ? 'The treasury cannot cover it.' : null),
        ...(held ? { tech: held.id } : {}),
      });
    }
  }
  return out;
}

/** Fills in an order of the arsenal for a ship of a class: how many, her class, and her name. Called by orderOffer. */
export function classOrder(g: Game, nat: string, b: Build, cls: string, name?: string): void {
  const row = classById(g, nat, cls);
  if (!row) return;
  const c = classSpec(row);
  b.count = c.count;
  b.cls = cls;
  b.ship = name?.trim() || newName(g, nat, row, c.count);
  b.name = `${c.count === 1 ? `The ${b.ship}, ${row.label.toLowerCase()},` : `The ${b.ship}`} at ${g.regions[b.region].name}`;
}

/** Renames a ship on order: the player's own name for her (the sandbox). */
export function nameOrder(g: Game, nat: string, id: number, name: string): boolean {
  const b = g.nations[nat]?.queue.find((x) => x.id === id && x.what === 'ship' && x.cls !== undefined);
  const want = name.trim().slice(0, 40);
  if (!b || !want || takenNames(g, nat).has(want)) return false;
  const row = classById(g, nat, b.cls!);
  b.ship = want;
  b.name = `${(b.count ?? 1) === 1 ? `The ${want}, ${(row?.label ?? 'ship').toLowerCase()},` : `The ${want}`} at ${g.regions[b.region].name}`;
  return true;
}

/** A ship of a class is ready: her line on her fleet's list (the counts have been raised by deliverArsenal). Returns the line. */
export function classJoins(g: Game, nat: string, b: Build, f: Fleet): ShipHull | null {
  const row = classById(g, nat, b.cls!);
  if (!row) return null;
  const c = classSpec(row);
  const h: ShipHull = {
    cls: row.id, kind: c.kind, label: row.label, name: b.ship ?? newName(g, nat, row, c.count), n: b.count ?? c.count, born: g.turn,
    power: c.power, tough: c.tough, escort: c.escort, raid: c.raid,
    ...(c.deck !== undefined ? { deck: c.deck } : {}), ...(c.aa !== undefined ? { aa: c.aa } : {}), ...(c.eyes !== undefined ? { eyes: c.eyes } : {}), ...(c.oil !== undefined ? { oil: c.oil } : {}),
  };
  (f.hulls ??= []).push(h);
  trim(f);
  return h;
}

// ---- Names ---------------------------------------------------------------------------------------------------

/** Every name a nation's ships have had or will have: afloat, lost, on order and in the yards. */
function takenNames(g: Game, nat: string): Set<string> {
  const out = new Set<string>();
  for (const f of g.fleets) if (f.nat === nat) for (const h of f.hulls ?? []) out.add(h.name);
  for (const m of g.nations[nat]?.memorial ?? []) out.add(m.name);
  for (const b of g.nations[nat]?.queue ?? []) { if (b.ship) out.add(b.ship); if (b.refit) out.add(b.refit.hull.name); }
  for (const f of g.fleets) if (f.nat === nat) out.add(f.name);
  return out;
}

const ORD = (n: number): string => { const t = n % 100, u = n % 10; return `${n}${t >= 11 && t <= 13 ? 'th' : u === 1 ? 'st' : u === 2 ? 'nd' : u === 3 ? 'rd' : 'th'}`; };

/**
 * The next name for a new ship of a class: capital ships and carriers after the nation's home provinces, cruisers and
 * small carriers after its cities, a single submarine or destroyer by her class and number, and a flotilla by number.
 */
export function newName(g: Game, nat: string, row: ClassRow, count = 1): string {
  const taken = takenNames(g, nat);
  if (count > 1) {
    for (let k = 1; ; k++) { const s = S.names.flotilla.replace('{n}', ORD(k)).replace('{label}', row.label); if (!taken.has(s)) return s; }
  }
  const kind = row.of as ShipClass;
  const big = kind === 'bb' || (kind === 'cv' && num(row.deck, 1) >= 0.5);
  const home = g.regions.filter((r) => isHome(g, nat, r) && !r.overseas);
  const pool = kind === 'dd' || kind === 'ss' ? [] : (big ? home : home.filter(isCity)).map((r) => r.name);
  // The capital's province is kept for the fleet's flagship-to-be: the largest first, by its worth.
  const order = [...new Set(pool)].sort();
  const rng = new Rng(hashString(`name:${g.seed}:${nat}:${row.id}`));
  for (let i = order.length - 1; i > 0; i--) { const j = rng.int(i + 1); [order[i], order[j]] = [order[j], order[i]]; }
  for (const s of order) if (!taken.has(s)) return s;
  for (let k = 1; ; k++) { const s = `${row.label} ${k}`; if (!taken.has(s)) return s; }
}

// ---- Refits --------------------------------------------------------------------------------------------------

/** A ship of a fleet a refit can be given to: one of its list by name, or one of a kind's standard ships ('std:bb'). */
export interface RefitTarget { key: string; label: string; kind: ShipClass; hull?: ShipHull }

/** The ships of a fleet that could go to a yard: each line of its list, and one of each kind's standard ships. */
export function refitTargets(f: Fleet): RefitTarget[] {
  const out: RefitTarget[] = [];
  for (const h of f.hulls ?? []) if (h.n > 0) out.push({ key: h.name, label: `The ${h.name} (${h.label.toLowerCase()})`, kind: h.kind, hull: h });
  for (const k of KINDS) if (standard(f, k) > 0) out.push({ key: `std:${k}`, label: `A ${NAVY.classes[k].one} of the standard pattern`, kind: k });
  return out;
}

const hasTech = (g: Game, nat: string, id: string | undefined): boolean => !id || !techById(id) || !!techOf(g, nat)?.done.includes(id);

/** A refit's price: its share of what a new ship of her class costs (her kind's standard ship, one hull of it). */
function refitCost(g: Game, nat: string, t: RefitTarget, r: RefitDef): number {
  const row = t.hull ? classById(g, nat, t.hull.cls) : undefined;
  const std = ARSENAL.ships[t.kind as Hull];
  const one = row ? classSpec(row).cost / classSpec(row).count : std.cost / std.count;
  return priceOf(g, nat, `ship:${t.kind}`, Math.round(one * r.share * 10) / 10);
}

/** Why a ship cannot be given a refit now, or null. */
export function whyNoRefit(g: Game, nat: string, f: Fleet, t: RefitTarget, r: RefitDef): string | null {
  if (!sandbox(g)) return 'Refits are part of the sandbox.';
  if (f.nat !== nat) return 'That is not your fleet.';
  if (!r.kinds.includes(t.kind)) return `A ${NAVY.classes[t.kind].one} cannot be given that refit.`;
  if (r.once && t.hull?.refits?.includes(r.id)) return 'She has had that refit already.';
  if (r.to && t.hull && t.hull.cls === r.to) return 'She is that class already.';
  if (t.kind === 'cv' && r.to === 'cv') return 'She is a carrier already.';
  if (r.needs && !hasTech(g, nat, r.needs)) return `Needs the technology ${techById(r.needs)?.name ?? r.needs}. Open Research to begin it.`;
  if (r.to && r.to !== 'cv' && !classById(g, nat, r.to)) return 'No plans of that class.';
  if (f.stance !== 'port' || f.base < 0) return `The ${f.name} must be in harbour.`;
  const port = g.regions[f.base];
  const need = ARSENAL.ships[t.kind as Hull].yard;
  if (!isHome(g, nat, port) || yardLevel(g, port) < need) return `The ${f.name}'s port at ${port.name} has no yard that can take her: she needs a shipyard of level ${need} in a home port.`;
  if (g.nations[nat].pp < refitCost(g, nat, t, r)) return 'The treasury cannot cover it.';
  return null;
}

/** Every refit a fleet's ships could be given, with its price and its reason when not. */
export function refitOffers(g: Game, nat: string, f: Fleet): { key: string; target: string; refit: string; label: string; blurb: string; cost: number; months: number; why: string | null }[] {
  if (!sandbox(g)) return [];
  const out = [];
  for (const t of refitTargets(f)) for (const r of S.refits) {
    if (!r.kinds.includes(t.kind)) continue;
    out.push({ key: `refit:${f.id}:${t.key}:${r.id}`, target: t.label, refit: r.id, label: r.label, blurb: r.blurb, cost: refitCost(g, nat, t, r), months: r.months, why: whyNoRefit(g, nat, f, t, r) });
  }
  return out;
}

/** The ship as she stands, as a line of the list: a standard ship gets her kind's numbers and a name of her own. */
function asHull(g: Game, nat: string, t: RefitTarget): ShipHull {
  if (t.hull) return { ...t.hull, n: 1 };
  const fight = NAVY.classes[t.kind];
  const row: ClassRow = { id: t.kind, of: t.kind, label: NAVY.classes[t.kind].one[0].toUpperCase() + NAVY.classes[t.kind].one.slice(1) };
  return { cls: t.kind, kind: t.kind, label: row.label, name: newName(g, nat, row), n: 1, born: g.turn, power: fight.power, tough: fight.tough, escort: fight.escort, raid: fight.raid };
}

/** Sends a ship to the yard for a refit: she leaves her fleet now and comes back to it when the months are up. */
export function orderRefit(g: Game, nat: string, fleetId: number, target: string, refit: string): Build | null {
  const n = g.nations[nat];
  const f = g.fleets.find((x) => x.id === fleetId);
  const r = S.refits.find((x) => x.id === refit);
  const t = f ? refitTargets(f).find((x) => x.key === target) : undefined;
  if (!n?.alive || !f || !r || !t || whyNoRefit(g, nat, f, t, r)) return null;
  const cost = refitCost(g, nat, t, r);
  const hull = asHull(g, nat, t);
  // She leaves the fleet: one off its count and, if she was on its list, off her line.
  f.ships[t.kind] -= 1;
  if (t.hull) { t.hull.n -= 1; f.hulls = f.hulls!.filter((h) => h.n > 0); if (!f.hulls.length) delete f.hulls; }
  n.pp -= cost;
  const b: Build = {
    id: g.nextBuild++, what: 'ship', item: t.kind, count: 1, name: `The ${hull.name}: ${r.label.toLowerCase()} at ${g.regions[f.base].name}`, region: f.base,
    ordered: g.turn, eta: g.turn + r.months, cost, refit: { id: r.id, fleet: f.id, hull },
  };
  // A refit draws its steel as a new ship does (classes.ts).
  stampSteel(g, b, 'ships');
  n.queue.push(b);
  return b;
}

/** Gives a ship her refit: her new numbers, or her new class. */
export function applyRefit(g: Game, nat: string, h: ShipHull, r: RefitDef): ShipHull {
  const out: ShipHull = { ...h, refits: [...(h.refits ?? []), r.id] };
  if (r.to) {
    const row = r.to === 'cv' ? undefined : classById(g, nat, r.to);
    if (row) {
      const c = classSpec(row);
      Object.assign(out, { cls: row.id, kind: c.kind, label: row.label, power: c.power, tough: c.tough, escort: c.escort, raid: c.raid });
      for (const k of ['deck', 'aa', 'eyes', 'oil'] as const) { if (c[k] !== undefined) out[k] = c[k]; else delete out[k]; }
    } else if (h.kind === 'ca' && r.light) {
      Object.assign(out, { cls: 'lightcv', kind: 'cv', label: r.light.label, power: num(r.light.power), tough: num(r.light.tough), escort: num(r.light.escort), raid: num(r.light.raid), deck: num(r.light.deck, 1) });
    } else {
      const fight = NAVY.classes.cv;
      Object.assign(out, { cls: 'cv', kind: 'cv', label: 'Carrier', power: fight.power, tough: fight.tough, escort: fight.escort, raid: fight.raid });
      delete out.deck;
    }
    return out;
  }
  for (const k of STATS) {
    if (r.mul?.[k] !== undefined) out[k] = Math.round(out[k] * r.mul[k]! * 1000) / 1000;
    if (r.add?.[k] !== undefined) out[k] = Math.round((out[k] + r.add[k]!) * 1000) / 1000;
  }
  for (const k of ['aa', 'eyes', 'deck', 'oil'] as const) if (r.add?.[k] !== undefined) out[k] = Math.round(((out[k] ?? 0) + r.add[k]!) * 1000) / 1000;
  return out;
}

/** A refit is done: she rejoins her fleet, or the nearest fleet of her kind, or forms one at the yard. */
export function refitDone(g: Game, nat: string, b: Build, back = false): void {
  const R = b.refit!;
  const r = S.refits.find((x) => x.id === R.id);
  const h = back || !r ? R.hull : applyRefit(g, nat, R.hull, r);
  const port = g.regions[b.region];
  let f = g.fleets.find((x) => x.id === R.fleet && x.nat === nat) ?? joinsFleet(g, nat, h.kind, port.id);
  if (!f && isHome(g, nat, port) && port.port && port.seas.length) f = foundFleet(g, nat, port, `${port.name} Fleet`);
  if (!f) {
    if (nat === g.human) headline(g, 'info', `With no port left to take her, the ${h.name} is lost in the yard at ${port.name}.`, nat, port.id);
    (g.nations[nat].memorial ??= []).push({ name: h.name, label: h.label, kind: h.kind, turn: g.turn, sea: -1, stars: h.stars ?? 0, how: 'in the yard, with no port left' });
    return;
  }
  f.ships[h.kind] += 1;
  // A standard ship whose refit was cancelled goes back as one of the count, as she was.
  if (back && h.cls === h.kind && !h.refits?.length) return;
  (f.hulls ??= []).push(h);
  trim(f);
  if (!back && nat === g.human) headline(g, 'info', `The ${h.name} leaves the yard at ${port.name}${r ? ` with her refit done: ${r.label.toLowerCase()}` : ''}, and rejoins the ${f.name}.`, nat, port.id);
}

// ---- The month -----------------------------------------------------------------------------------------------

/** The sandbox's month at sea for a nation: lists kept inside counts, the lost written on the memorial and told, and the computer's designs. */
export function shipsMonth(g: Game, nat: string): void {
  if (!sandbox(g)) return;
  for (const f of g.fleets) if (f.nat === nat) { trim(f); writeFallen(g, f); }
  // A ship that keeps coming through her battles is given a name by her crew (reputation.ts), once and for good.
  for (const f of g.fleets) if (f.nat === nat) for (const h of f.hulls ?? []) {
    if (h.nick) continue;
    const nick = shipNick(g, nat, h.name, h.stars ?? 0);
    if (!nick) continue;
    h.nick = nick;
    if (nat === g.human) headline(g, 'battle', `After ${plural(h.stars ?? 0, 'battle')} the crew of the ${h.name} have a name for her: "${nick}".`, nat, undefined, { arm: 'sea' });
  }
  const n = g.nations[nat];
  if (nat === g.human) for (const m of n.memorial ?? []) {
    if (m.told || m.turn < g.turn - 1) continue;
    m.told = 1;
    const sea = m.sea >= 0 ? ` in the ${g.seas[m.sea]?.name ?? 'open sea'}` : '';
    headline(g, 'battle', `The ${m.name}, ${m.label.toLowerCase()}, is lost${sea}${m.how ? ` ${m.how}` : ''}.${m.stars ? ` She had ${plural(m.stars, 'battle star')}.` : ''}`, nat, undefined, { arm: 'sea', lost: nat });
  }
  if (nat !== g.human && computerDesigns(g)) aiDesign(g, nat);
}

// ---- The drawing board ---------------------------------------------------------------------------------------

/** A surface ship's dials: each a step of its row in sandbox-ships.json board (turrets 0 to 4 with 'guns' above none). */
export interface Dials { hull: number; guns: number; turrets: number; armour: number; speed: number; aa: number; air: number }
/** A submarine's own small card. */
export interface SubDials { size: number; tubes: number; range: number; hangar: number }

export interface DesignCard {
  row: ClassRow & { cost: number; months: number; yard: number; count: number } & Record<Stat, number>;
  kind: ShipClass;
  room: { used: number; has: number };
  /** True when its price was raised to keep it within a tenth of its kind's standard ship for the money. */
  capped: boolean;
  why: string | null;
}

const B = () => S.board;
const r1 = (x: number): number => Math.round(x * 10) / 10;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** How much a ship is worth for her price, the measure the board holds every design to: her power, escort and raid, steadied by how hard she is to sink. */
function valueOf(x: Record<Stat, number>, cost: number, toughStd: number): number {
  const W = B().weights;
  return ((x.power * W.power + x.escort * W.escort + x.raid * W.raid) * Math.sqrt(toughStd / Math.max(0.05, x.tough))) / Math.max(0.1, cost);
}

/** The standard ship of a kind, per hull, as the board measures it. */
function stdValue(kind: ShipClass): number {
  const s = ARSENAL.ships[kind as Hull], f = NAVY.classes[kind];
  return valueOf({ power: f.power, tough: f.tough, escort: f.escort, raid: f.raid }, s.cost / s.count, f.tough);
}

/** What a class is worth for her price against her kind's standard ship (1 = the same; the board holds designs to 'value'). */
export function againstStandard(row: { of: string; cost?: number; count?: number } & Partial<Record<Stat, number>>): number {
  const kind = row.of as ShipClass, f = NAVY.classes[kind];
  const x = { power: row.power ?? f.power, tough: row.tough ?? f.tough, escort: row.escort ?? f.escort, raid: row.raid ?? f.raid };
  const per = (row.cost ?? ARSENAL.ships[kind as Hull].cost) / (row.count ?? ARSENAL.ships[kind as Hull].count);
  return valueOf(x, per, f.tough) / stdValue(kind);
}

const needsWhy = (g: Game, nat: string, id: string | undefined): string | null => (hasTech(g, nat, id) ? null : `Needs the technology ${techById(id!)?.name ?? id}.`);

/** Prices a surface ship from her dials, as the board shows it before she is signed for. */
export function designCard(g: Game, nat: string, d: Dials, label = 'Own design'): DesignCard {
  const b = B();
  const hull = b.hull[d.hull], gun = b.guns[d.guns], arm = b.armour[d.armour], spd = b.speed[d.speed], aa = b.aa[d.aa], air = b.air[d.air];
  const bad = !hull || !gun || !arm || !spd || !aa || !air || d.turrets < 0 || d.turrets > b.turrets[1] || (d.guns > 0 && d.turrets < b.turrets[0]);
  const turrets = d.guns > 0 ? d.turrets : 0;
  const has = hull?.room ?? 0;
  const roomAir = air ? (air.roomShare !== undefined ? air.roomShare * has : num(air.room)) : 0;
  const used = bad ? 0 : turrets * gun.room + arm.room * has + spd.room * has + aa.room + roomAir;
  let cost = bad ? 0 : hull.cost + turrets * gun.cost + arm.cost * hull.cost + spd.cost * hull.cost + aa.cost + (air.costShare !== undefined ? air.costShare * hull.cost : num(air.cost));
  const months = bad ? 0 : Math.round(hull.months + turrets * gun.months + num(arm.months) + air.months);
  const deck = bad ? 0 : air.deck * hull.deck;
  const gunPower = bad ? 0 : turrets * gun.power;
  const x: Record<Stat, number> = bad ? { power: 0, tough: 1, escort: 0, raid: 0 } : {
    power: gunPower + aa.power + deck * b.deck.power,
    tough: hull.tough * arm.tough,
    escort: hull.escort + spd.escort + aa.escort + air.escort + deck * b.deck.escort,
    raid: gunPower * b.gunsRaid + spd.raid + deck * b.deck.raid,
  };
  const kind: ShipClass = deck >= 0.33 ? 'cv' : d.guns >= 3 && d.armour >= 1 && d.hull >= 2 ? 'bb' : d.hull === 0 ? 'dd' : 'ca';
  const count = bad ? 1 : hull.count;
  // No better than a tenth over the standard ship of her kind for the money: above that, the price rises.
  let capped = false;
  if (!bad) {
    const most = stdValue(kind) * b.value;
    const v = valueOf(x, cost, NAVY.classes[kind].tough);
    if (v > most) { cost = cost * (v / most); capped = true; }
  }
  const why = bad ? 'Those dials do not make a ship.' : used > has + 1e-9 ? `She will not fit: her guns, armour, engines and the rest take ${r1(used)} of the ${has} her hull has room for.`
    : d.guns === 0 && deck <= 0 && d.air <= 1 && kind !== 'dd' ? 'A ship needs guns or a flight deck.'
      : needsWhy(g, nat, hull.needs) ?? needsWhy(g, nat, gun.needs) ?? needsWhy(g, nat, air.needs) ?? whyNoPlans(g, nat, kind);
  const row = {
    id: '', of: kind, label, own: true, cost: r1(cost * count), months: Math.max(4, months), yard: bad ? 1 : hull.yard, count,
    power: r3(x.power), tough: r3(x.tough), escort: r3(x.escort), raid: r3(x.raid),
    ...(deck > 0 ? { deck: r3(deck) } : {}), ...(aa.aa > 0 ? { aa: aa.aa } : {}), oil: bad ? 1 : r3(hull.oil * spd.oil),
    dials: { ...d },
  } as DesignCard['row'];
  return { row, kind, room: { used: r1(used), has }, capped, why };
}

/** Prices a submarine from her card: always a submarine; a coastal or ocean-going boat comes as a flotilla. */
export function subCard(g: Game, nat: string, d: SubDials, label = 'Own design'): DesignCard {
  const b = B().sub;
  const sz = b.size[d.size], tb = b.tubes[d.tubes], rg = b.range[d.range], hg = b.hangar[d.hangar];
  const bad = !sz || !tb || !rg || !hg;
  const per = bad ? 0 : sz.cost + tb.cost + rg.cost + hg.cost;
  const x: Record<Stat, number> = bad ? { power: 0, tough: 1, escort: 0, raid: 0 } : { power: sz.power, tough: sz.tough, escort: 0, raid: Math.max(0, sz.raid + tb.raid + rg.raid) };
  let cost = per, capped = false;
  const most = stdValue('ss') * B().value, v = bad ? 0 : valueOf(x, per, NAVY.classes.ss.tough);
  if (!bad && v > most) { cost = per * (v / most); capped = true; }
  const why = bad ? 'Those dials do not make a boat.' : hg.size !== undefined && d.size + 1 < hg.size ? 'Only a cruiser submarine has room for a hangar.' : needsWhy(g, nat, hg.needs);
  const count = bad ? 1 : sz.count;
  const row = {
    id: '', of: 'ss', label, own: true, cost: r1(cost * count), months: bad ? 6 : Math.round(sz.months + num(hg.months)), yard: bad ? 1 : sz.yard, count,
    power: x.power, tough: x.tough, escort: 0, raid: r3(x.raid), oil: bad ? 1 : rg.oil, ...(hg && hg.eyes > 0 ? { eyes: hg.eyes } : {}), sub: { ...d },
  } as DesignCard['row'];
  return { row, kind: 'ss', room: { used: 0, has: 0 }, capped, why };
}

/** The dials that come nearest each what-if class, to open on the board and change. */
export const PRESETS: Record<string, Dials | SubDials> = {
  superbb: { hull: 3, guns: 4, turrets: 4, armour: 2, speed: 0, aa: 1, air: 0 },
  battlecv: { hull: 2, guns: 3, turrets: 2, armour: 1, speed: 1, aa: 1, air: 2 },
  raider: { hull: 1, guns: 2, turrets: 4, armour: 0, speed: 2, aa: 0, air: 1 },
  aacruiser: { hull: 1, guns: 1, turrets: 4, armour: 0, speed: 2, aa: 2, air: 0 },
  escortcv: { hull: 2, guns: 0, turrets: 0, armour: 0, speed: 0, aa: 1, air: 3 },
  scoutsub: { size: 2, tubes: 1, range: 2, hangar: 1 },
  bb: { hull: 2, guns: 3, turrets: 3, armour: 2, speed: 1, aa: 1, air: 1 },
  cv: { hull: 2, guns: 0, turrets: 0, armour: 0, speed: 2, aa: 1, air: 3 },
  ca: { hull: 1, guns: 2, turrets: 4, armour: 0, speed: 2, aa: 0, air: 1 },
  dd: { hull: 0, guns: 1, turrets: 3, armour: 0, speed: 2, aa: 0, air: 0 },
};

const isSub = (d: Dials | SubDials): d is SubDials => (d as SubDials).size !== undefined;

/** Signs for a design: it joins the nation's classes, to be ordered on the Navy column like any other. Returns its row, or null with the reason. */
export function drawDesign(g: Game, nat: string, d: Dials | SubDials, name: string): { row: ClassRow | null; why: string | null } {
  if (!sandbox(g)) return { row: null, why: 'The drawing board is part of the sandbox.' };
  const n = g.nations[nat];
  const label = name.trim().slice(0, 32) || `Design ${(n.designs?.length ?? 0) + 1}`;
  if ([...CLASS_ROWS, ...(n.designs ?? [])].some((r) => r.label.toLowerCase() === label.toLowerCase())) return { row: null, why: `There is a class called ${label} already.` };
  const card = isSub(d) ? subCard(g, nat, d, label) : designCard(g, nat, d, label);
  if (card.why) return { row: null, why: card.why };
  const row: ClassRow = { ...card.row, id: `own-${nat.toLowerCase()}-${(g.flags[`design:${nat}`] ?? 0) + 1}`, label, blurb: `A class of ${n.adj ?? nat} design, drawn in ${g.turn > 0 ? 'the war' : 'peace'}.` };
  g.flags[`design:${nat}`] = (g.flags[`design:${nat}`] ?? 0) + 1;
  (n.designs ??= []).push(row);
  return { row, why: null };
}

// ---- The computer's navies -----------------------------------------------------------------------------------

/** A computer navy's style (sandbox-ships.json): how it leans when it may build classes of its own. */
export const styleOf = (nat: string): Style => S.styles[nat] ?? S.styles._default;

/**
 * What a computer staff's programme orders (arsenal.ts aiProgramme), with the sandbox on and the history dial loose
 * enough: now and then, by the dial's share, a ship it wants is ordered to a class its style likes and it may build,
 * and its style's favourite class is wanted on its own. Below the dial's share it keeps history's ships.
 */
export function aiClasses(g: Game, nat: string, wants: string[]): string[] {
  const share = whatifShare(g);
  if (share <= 0) return wants;
  const rng = new Rng(hashString(`classes:${g.seed}:${g.turn}:${nat}`));
  const likes = [...S.likes[styleOf(nat)], ...(g.nations[nat].designs ?? []).map((r) => r.id)];
  const open = (id: string): ClassRow | undefined => {
    const row = classById(g, nat, id);
    return row && !classLocked(g, nat, row) && !whyNoPlans(g, nat, row.of) && (onPlans(g, nat, row.of) || !locked(g, nat, `ship:${row.of}`)) ? row : undefined;
  };
  const out = wants.map((k) => {
    const [, kind, cls] = k.split(':');
    if (!k.startsWith('ship:') || cls || !rng.chance(share)) return k;
    const pick = likes.map(open).find((r) => r?.of === kind);
    return pick ? `ship:${kind}:${pick.id}` : k;
  });
  // Its style's favourite on its own, one of a kind on order at a time.
  if (rng.chance(share * 0.5)) {
    const fav = likes.map(open).find((r) => !!r);
    const busy = (kind: string): boolean => g.nations[nat].queue.some((b) => b.what === 'ship' && b.item === kind);
    if (fav && !busy(fav.of) && !out.some((k) => k.endsWith(`:${fav.id}`))) out.push(`ship:${fav.of}:${fav.id}`);
  }
  return out;
}

/** Once a year at the loosest stops, a great power's computer navy draws a class of its own: its style's favourite with one dial turned. */
function aiDesign(g: Game, nat: string): void {
  const n = g.nations[nat];
  // Only the navies with a style of their own (the great powers) keep a design bureau.
  if (S.styles[nat] === undefined || !n.alive || !n.atWar.length || (g.turn % 12) !== 6 || (n.designs?.length ?? 0) >= 3) return;
  const rng = new Rng(hashString(`design:${g.seed}:${g.turn}:${nat}`));
  const base = PRESETS[S.likes[styleOf(nat)][0]] ?? PRESETS.ca;
  if (isSub(base)) return;
  const keys: (keyof Dials)[] = ['guns', 'armour', 'speed', 'aa', 'turrets'];
  const k = keys[rng.int(keys.length)];
  const d = { ...base, [k]: Math.max(0, base[k] + (rng.chance(0.5) ? 1 : -1)) };
  drawDesign(g, nat, d, `${n.adj ?? nat} ${['Mark', 'Type', 'Pattern'][rng.int(3)]} ${(n.designs?.length ?? 0) + 1}`);
}

/** How many ships of a kind on a fleet's list (for the fleet cards). */
export const onList = (f: Fleet, k: ShipClass): number => listed(f, k);
