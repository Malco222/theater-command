// Airfields as targets (Brian, 22 November 2026; the design director's ruling of 22 Nov 05:55Z on the design in
// air-battles/airfields/airfield-targets-design.md). Bombers can go for an airfield: enough bombs shut its runway, and
// no aircraft based there flies until its holder repairs it; aircraft caught on the ground are lost; heavier raids
// burn the hangars, workshops and fuel, and the field serves fewer aircraft until it is rebuilt. Fields are field
// strips (fighters only) or concrete airfields (every group; the only ones where jets keep their edge).
//
// ONE state per province's airfield, kept here and written through damageField() only: by a raid on the field, by a
// fight for the sky whose bombers hit it, by the spies (an agent's or saboteurs' wreck), by an army ploughing it up
// (prizes.ts), and by Repair. Everything else reads it: fieldShut() grounds (airversion.ts wrecked()), the room the
// field serves (air.ts airfieldsHold), basing, prizes, the staffs and the cards.
//
// The grand campaign's air war only, behind its own switch (g.flags.fields; damage.json field.on, false as shipped).
// With it off nothing here writes anything, and fieldShut() is release 1's wrecked-field test to the letter.

import data from '../../data/campaign/damage.json';
import type { AirFleet, Game, Region } from './types';
import { airPool, airRange, airfieldsHold, fieldClass, km, poolIn } from './air';
import { prizeHolder, prizeRules, hasPrize } from './prizes';
import { besieged } from './siege';
import { PRODUCTION } from './economy';
import { allied } from './game';
import { agentAt, sightOf } from './intel';

export const FIELD = (data as unknown as {
  field: {
    on: boolean; shut: number; level: number; levels: number; room: number; ground: number; strafe: number; bomber: number; surprise: number; mostGround: number;
    price: { runway: { strip: number; full: number }; level: { strip: number; full: number }; least: number; derived: boolean };
    plough: { order: number; burn: number }; worth: { shut: number; level: number }; ai: { quiet: number };
  };
}).field;

/** True where airfields are targets: a grand campaign's air war with the switch on. */
export const fieldRules = (g: Game): boolean => g.flags.fields === 1 && g.flags.open === 1 && g.flags.airwar === 1;

/** Turns airfields as targets on or off for a grand campaign (the interfaces, when a new one begins). Nothing outside one. */
export function openFields(g: Game, on = true): void {
  if (g.flags.open !== 1) return;
  if (on) g.flags.fields = 1; else delete g.flags.fields;
}

// The keys, kept as strings: fieldShut() is asked for every group in every fight.
const K: Record<string, string[]> = { fsh: [], flv: [], fht: [], frp: [], frc: [], afc: [] };
const key = (k: 'fsh' | 'flv' | 'fht' | 'frp' | 'frc' | 'afc', id: number): string => (K[k][id] ??= `${k}:${id}`);
export const fieldKey = key;

/**
 * True when no aircraft can take off from a province's field. With the switch on: its runway is shut. Off: release 1's
 * test, a field saboteurs wrecked this month in a campaign with the field piece (airversion.ts, ruling 151(b)).
 */
export function fieldShut(g: Game, region: number): boolean {
  if (g.flags.fields === 1 && fieldRules(g)) return g.flags[key('fsh', region)] !== undefined;
  return g.flags['av:field'] === 1 && g.flags[`wreck:${region}`] === g.turn;
}

/** Levels burned at a field (hangars, workshops, fuel): 0 for a whole one, and always 0 with the switch off. */
export const fieldLost = (g: Game, region: number): number => (fieldRules(g) ? g.flags[key('flv', region)] ?? 0 : 0);

/** The month a field was last hit, or undefined. */
export const fieldHit = (g: Game, region: number): number | undefined => g.flags[key('fht', region)];

/** The month a repair in hand was ordered, or undefined. */
export const fieldRepairing = (g: Game, region: number): number | undefined => g.flags[key('frp', region)];

export type FieldTier = 'none' | 'strip' | 'full';

/**
 * A field's tier: none where it serves nobody; full (concrete) where the war began with an airfield or an air base
 * there, or a concrete level has been built; else a strip (a landing ground, and what engineers and the strip offer
 * lay). Damage never changes a tier. With the switch off every field reads full: nothing asks.
 */
export function fieldTier(g: Game, region: number): FieldTier {
  if (airfieldsHold(g, region) <= 0) return 'none';
  if (!fieldRules(g)) return 'full';
  return fieldClass(g, region) >= 2 || g.flags[key('afc', region)] === 1 ? 'full' : 'strip';
}

/** The room a field's burned levels take off, in aircraft (air.ts airfieldsHold subtracts it). */
export const burnedRoom = (g: Game, region: number): number => {
  const lv = g.flags[key('flv', region)];
  return lv ? lv * FIELD.room : 0;
};

/** A field's room as built, before anything was burned. */
export const roomBuilt = (g: Game, region: number): number => airfieldsHold(g, region) + (fieldRules(g) ? burnedRoom(g, region) : 0);

// ---- A hit -----------------------------------------------------------------------------------------------------

export type FieldHow = 'raid' | 'sky' | 'agent' | 'saboteurs' | 'plough' | 'burn';

/** What a raid brings to the field: bombers through (after losses, as fly() counts them), escort fighters through, the radar's ground factor, surprise, and who is an enemy. */
export interface FieldBlow {
  through: number;
  strafe?: number;
  /** The holder's radar chain's 'ground' factor where it has one (Chain Home 0.3), else 1. */
  radar?: number;
  /** True in the month a war opens between the raiders and the holder. */
  surprise?: boolean;
  /** Which nations' groups on the field are the raiders' enemies. */
  foe: (nat: string) => boolean;
  /** Each defending group's share that went up to meet the raid (by group id): the rest of its fighters were parked. */
  up?: Map<number, number>;
}

export interface FieldHit {
  region: number;
  how: FieldHow;
  /** True if the runway is shut now (newly or still). */
  shut: boolean;
  /** True if this hit shut it. */
  closed: boolean;
  /** Levels burned by this hit. */
  levels: number;
  /** True where the hit undoes a repair in hand: it shut the runway, or would have, or burned or ploughed. */
  cancels: boolean;
  /** Aircraft destroyed on the ground, and how they fell by group. */
  ground: number;
  losses: { id: number; ftr: number; bmr: number }[];
  /** Aircraft grounded by the shut runway (those left on the field). */
  grounded: number;
  /** The field's room after the hit, and as built. */
  room: number;
  built: number;
  /** What the hit is worth to the raiders, in aircraft (damage.json field.worth). */
  worth: number;
  words: string;
}

/** For probes and the gate's readings: told of every hit and every repair ordered. It moves nothing (as ai.ts landingWatch). */
export const fieldWatch: { tell: ((e: { kind: 'hit'; hit: FieldHit; nat: (id: number) => string } | { kind: 'repair'; nat: string; region: number; cost: number; free: boolean }) => void) | null } = { tell: null };

const groupsOn = (g: Game, region: number, foe: (nat: string) => boolean): AirFleet[] =>
  g.airFleets.filter((f) => f.base === region && f.ship === undefined && f.ftr + f.bmr > 0 && foe(f.nat));

const num = (n: number): string => Math.round(n).toLocaleString('en-US');

/** What a hit would do, without doing it (the raid card and the staffs' forecast read this; damageField applies it). */
export function planFieldHit(g: Game, region: number, how: FieldHow, blow?: FieldBlow): FieldHit | null {
  if (!fieldRules(g)) return null;
  const r = g.regions[region];
  const room = airfieldsHold(g, region);
  if (!r || room <= 0) return null;
  const built = room + burnedRoom(g, region);
  const wasShut = g.flags[key('fsh', region)] !== undefined;
  const had = g.flags[key('flv', region)] ?? 0;
  // A field never burns below one level of room.
  const canBurn = Math.max(0, Math.floor(built / FIELD.room) - 1 - had);
  let closed = false, levels = 0, cancels = false;
  const losses: FieldHit['losses'] = [];
  let ground = 0;
  if (how === 'agent' || how === 'saboteurs') {
    if (wasShut) return null;
    closed = true;
  } else if (how === 'plough' || how === 'burn') {
    closed = !wasShut;
    cancels = true;
    levels = Math.min(canBurn, how === 'plough' ? FIELD.plough.order : FIELD.plough.burn);
  } else {
    const b = blow!;
    const through = Math.max(0, b.through);
    closed = !wasShut && through >= FIELD.shut * room;
    levels = Math.min(canBurn, FIELD.levels, Math.floor(through / FIELD.level));
    // A raid heavy enough to shut a runway undoes the crews' work on one already shut.
    cancels = through >= FIELD.shut * room || levels > 0;
    // The aircraft on the ground: every bomber based there, and the fighters that did not go up.
    const on = groupsOn(g, region, b.foe);
    const parked = on.map((f) => ({ f, ftr: f.ftr * (1 - Math.min(1, Math.max(0, b.up?.get(f.id) ?? 0))), bmr: f.bmr }));
    const all = parked.reduce((s, p) => s + p.ftr + p.bmr, 0);
    const weight = parked.reduce((s, p) => s + p.ftr + p.bmr * FIELD.bomber, 0);
    const blows = (through + FIELD.strafe * Math.max(0, b.strafe ?? 0)) * FIELD.ground * (b.radar ?? 1) * (b.surprise ? FIELD.surprise : 1);
    const lost = Math.min(FIELD.mostGround * all, blows);
    if (weight > 0 && lost > 0) {
      for (const p of parked) {
        const ftr = Math.min(Math.floor(p.ftr), Math.round((lost * p.ftr) / weight));
        const bmr = Math.min(p.bmr, Math.round((lost * p.bmr * FIELD.bomber) / weight));
        if (ftr + bmr > 0) { losses.push({ id: p.f.id, ftr, bmr }); ground += ftr + bmr; }
      }
    }
  }
  const shut = wasShut || closed;
  const left = groupsOn(g, region, blow?.foe ?? (() => true)).reduce((s, f) => s + f.ftr + f.bmr, 0) - ground;
  const grounded = closed ? Math.max(0, left) : 0;
  const after = room - levels * FIELD.room;
  const worth = ground + FIELD.worth.shut * grounded + FIELD.worth.level * levels;
  const place = r.name;
  const parts: string[] = [];
  if (how === 'agent' || how === 'saboteurs') parts.push(`The runway at ${place} is cratered: no aircraft can take off there until its crews have it open next month.`);
  else if (closed) parts.push(`The field at ${place} is shut: no aircraft can take off there until it is repaired.`);
  else if (wasShut) parts.push(`The field at ${place} stays shut.`);
  if (levels) parts.push(`${levels === 1 ? 'One level' : `${levels} levels`} burned: it serves ${num(after)} of its ${num(built)} aircraft until rebuilt.`);
  if (ground) parts.push(`${num(ground)} aircraft destroyed on the ground.`);
  if (!parts.length && (how === 'raid' || how === 'sky')) parts.push(`The bombs on the field at ${place} were filled in within the day.`);
  return { region, how, shut, closed, levels, cancels: cancels || closed, ground, losses, grounded, room: after, built, worth, words: parts.join(' ') };
}

/**
 * The one writer of a field's damage: a raid, a fight for the sky's bombers, an agent or saboteurs (the spies), or an
 * army ploughing it up. Returns what it did, or null where it struck nothing (the switch off, no field there, or a
 * field already shut by an agent's kind of hit). An agent's or saboteurs' hit is the light one: the runway shut, a
 * free repair in hand by the field's own crews, open again next month. A double agent's "success" never calls this.
 * A hit that shuts or burns cancels any repair in hand: its unspent money is lost with the bombs.
 */
export function damageField(g: Game, region: number, how: FieldHow, blow?: FieldBlow): FieldHit | null {
  const hit = planFieldHit(g, region, how, blow);
  if (!hit) return null;
  const strike = hit.closed || hit.levels > 0 || hit.ground > 0 || how === 'raid' || how === 'sky';
  if (strike) g.flags[key('fht', region)] = g.turn;
  if (hit.cancels) { delete g.flags[key('frp', region)]; delete g.flags[key('frc', region)]; }
  if (hit.closed) g.flags[key('fsh', region)] = g.turn;
  if (hit.levels) g.flags[key('flv', region)] = (g.flags[key('flv', region)] ?? 0) + hit.levels;
  // The field's own crews mend an agent's work by next month at no price.
  if (how === 'agent' || how === 'saboteurs') g.flags[key('frp', region)] = g.turn;
  if (fieldWatch.tell) { const by = new Map(g.airFleets.map((f) => [f.id, f.nat])); fieldWatch.tell({ kind: 'hit', hit, nat: (id) => by.get(id) ?? '' }); }
  for (const x of hit.losses) {
    const f = g.airFleets.find((y) => y.id === x.id);
    if (!f) continue;
    const p = airPool(g, f.nat);
    f.ftr -= x.ftr; f.bmr -= x.bmr; f.lost.ftr += x.ftr; f.lost.bmr += x.bmr;
    // Pilots of fighters lost on the ground go back to the pool, as release 1's fight for the sky has it.
    p.lostFtr += x.ftr; p.lostBmr += x.bmr; poolIn(g, f.nat, x.ftr, f);
  }
  return hit;
}

// ---- Repair ----------------------------------------------------------------------------------------------------

/** The nation that has the use of a field, and so repairs it: the province's holder, or the army round a besieged town. */
export function fieldHolder(g: Game, r: Region): string {
  return prizeRules(g) && hasPrize(g, r, 'airfield') ? prizeHolder(g, r, 'airfield') : r.ctrl;
}

/** True where a field is damaged: shut, or levels burned. */
export const fieldDamaged = (g: Game, region: number): boolean =>
  fieldRules(g) && (g.flags[key('fsh', region)] !== undefined || (g.flags[key('flv', region)] ?? 0) > 0);

/** The price of repairing a field now: the runway if it is shut, and each burned level, at a strip's or a concrete field's rate; never below PR 19's least. */
export function fieldRepairCost(g: Game, region: number): number {
  const P = FIELD.price;
  const t = fieldTier(g, region) === 'strip' ? 'strip' : 'full';
  const shut = g.flags[key('fsh', region)] !== undefined ? P.runway[t] : 0;
  const lv = (g.flags[key('flv', region)] ?? 0) * P.level[t];
  return Math.max(P.least, Math.round((shut + lv) * 10) / 10);
}

/** Why a nation cannot order this field repaired now, or null. */
export function whyNotFieldRepair(g: Game, nat: string, region: number): string | null {
  if (!fieldRules(g)) return 'Airfields are repaired only with airfields as targets.';
  const r = g.regions[region];
  if (!r) return 'Pick a province.';
  if (!fieldDamaged(g, region)) return `The field at ${r.name} is whole.`;
  if (fieldHolder(g, r) !== nat) return `The field at ${r.name} is not in your hands.`;
  if (besieged(g, r) && r.ctrl === nat && fieldHolder(g, r) !== nat) return `${r.name} is under siege: no work can be done on its field.`;
  if (g.flags[key('frp', region)] !== undefined) return inHandWords(g, region);
  const cost = fieldRepairCost(g, region);
  if ((g.nations[nat]?.pp ?? 0) < cost) return `Not enough production points: the repair costs ${cost}.`;
  return null;
}

/** Orders a field repaired, paid at once. The runway opens at the next month's start; each burned level a month after. */
export function orderFieldRepair(g: Game, nat: string, region: number, free = false): boolean {
  if (!free && whyNotFieldRepair(g, nat, region)) return false;
  if (free && (!fieldDamaged(g, region) || g.flags[key('frp', region)] !== undefined)) return false;
  const cost = free ? 0 : fieldRepairCost(g, region);
  if (cost > 0) g.nations[nat].pp -= cost;
  g.flags[key('frp', region)] = g.turn;
  if (nat === g.human && cost > 0) g.flags[key('frc', region)] = cost;
  fieldWatch.tell?.({ kind: 'repair', nat, region, cost, free });
  return true;
}

/** Takes back a repair the player ordered this month, money and all (PR 19's take-back). False after the month, or once the field was hit since. */
export function undoFieldRepair(g: Game, nat: string, region: number): boolean {
  const paid = g.flags[key('frc', region)];
  if (nat !== g.human || paid === undefined || g.flags[key('frp', region)] !== g.turn) return false;
  g.nations[nat].pp += paid;
  delete g.flags[key('frp', region)];
  delete g.flags[key('frc', region)];
  return true;
}

/**
 * The month's work on the fields, at the month's start (turn.ts): where a repair is in hand from an earlier month, the
 * runway opens, or, with it open, one burned level comes back; a field whole again is off the books. A repair in hand
 * is dropped when the province changed hands since it was ordered: the crews were the loser's.
 */
export function fieldsMonth(g: Game): void {
  if (!fieldRules(g)) return;
  for (const k of Object.keys(g.flags)) {
    if (!k.startsWith('frp:')) continue;
    const id = +k.slice(4), at = g.flags[k];
    delete g.flags[key('frc', id)];
    if (at >= g.turn) continue;
    const r = g.regions[id];
    if (!r || (r.since ?? 0) >= at) { delete g.flags[k]; continue; }
    if (g.flags[key('fsh', id)] !== undefined) delete g.flags[key('fsh', id)];
    else if ((g.flags[key('flv', id)] ?? 0) > 0) { const v = g.flags[key('flv', id)] - 1; if (v > 0) g.flags[key('flv', id)] = v; else delete g.flags[key('flv', id)]; }
    if (g.flags[key('fsh', id)] === undefined && !g.flags[key('flv', id)]) delete g.flags[k];
  }
}

/**
 * A computer staff's repairs (and a player's staff with towns and trade), before new builds: a damaged field with
 * any of its own or its allies' groups based there at once, front line included; an empty one when it has not been hit
 * in the last 'ai.quiet' months and lies within its fighters' reach of a front. Paid from the treasury, keeping the
 * computer's reserve in hand. Returns the fields repaired, by name.
 */
export function aiFieldRepair(g: Game, nat: string, friends: (n: string) => boolean): string[] {
  if (!fieldRules(g)) return [];
  const n = g.nations[nat];
  if (!n?.alive) return [];
  const done: string[] = [];
  const range = airRange({ nat, name: '' }, g).ftr;
  let foes: Region[] | null = null;
  const near = (r: Region): boolean => {
    foes ??= g.regions.filter((x) => n.atWar.includes(x.ctrl));
    return foes.some((x) => km(g, r.id, x.id) <= range);
  };
  const ids = Object.keys(g.flags).filter((k) => k.startsWith('fsh:') || k.startsWith('flv:')).map((k) => +k.slice(4));
  for (const id of [...new Set(ids)].sort((a, b) => a - b)) {
    const r = g.regions[id];
    if (!r || fieldHolder(g, r) !== nat || g.flags[key('frp', id)] !== undefined || !fieldDamaged(g, id)) continue;
    const based = g.airFleets.some((f) => f.base === id && f.ship === undefined && f.ftr + f.bmr > 0 && friends(f.nat));
    if (!based) {
      const hit = g.flags[key('fht', id)];
      if (hit !== undefined && g.turn - hit < FIELD.ai.quiet) continue;
      if (!near(r)) continue;
    }
    const cost = fieldRepairCost(g, id);
    if (n.pp < cost + PRODUCTION.aiReserve) continue;
    if (orderFieldRepair(g, nat, id)) done.push(r.name);
  }
  return done;
}

/** The words for a repair in hand: why Repair cannot be pressed again. */
export function inHandWords(g: Game, region: number): string {
  const left = (g.flags[key('fsh', region)] !== undefined ? 1 : 0) + (g.flags[key('flv', region)] ?? 0);
  return g.flags[key('fsh', region)] !== undefined ? `Repair in hand: open next month${left > 1 ? `, whole in ${left} months` : ''}.` : `Repair in hand: whole in ${left <= 1 ? 'a month' : `${left} months`}.`;
}

// ---- Who knows what (the design director, 22 Nov 06:21Z) -------------------------------------------------------

/**
 * How well a province must be seen for its field's state to be known (intel.ts SIGHT: 1 a glimpse, 2 a full look).
 * The one constant every reader uses alike: the bridge's fieldRow, the glance, the crater list, and the staffs where
 * they choose a field to raid or raid again.
 */
export const FIELD_SEEN = 2;

/** True where a viewer knows a field's state: its own or an ally's field, one where its agent is placed, or one seen at FIELD_SEEN. */
export function fieldKnown(g: Game, viewer: string | null, region: number, sight?: Uint8Array): boolean {
  const r = g.regions[region];
  if (!viewer || !r) return false;
  if (allied(g, viewer, r.ctrl) || allied(g, viewer, fieldHolder(g, r))) return true;
  if (agentAt(g, region, viewer)) return true;
  return (sight ?? sightOf(g, viewer))[region] >= FIELD_SEEN;
}

/** The city glance's and the airfield card's reader (by name, from turn.ts): null with the switch off or no field. Shut, levels lost, room now and crowding only where the viewer knows them (false, 0, -1 and false otherwise). */
export interface FieldRepairView {
  shut: boolean;
  lost: number;
  tier: FieldTier;
  /** The room the field serves now (-1 where not known); the room as built is public. */
  roomNow: number;
  built: number;
  crowded: boolean;
  known: boolean;
  /** The price of Repair now, and why it cannot be pressed (a repair in hand says so: never null while one is). */
  cost: number;
  why: string | null;
  /** True where the viewer may take back a repair ordered this month. */
  undo: boolean;
  /** Months until the field is whole: with a repair in hand, from now; else what a repair ordered now would take. 0 when whole. */
  months: number;
  /** True with a repair in hand. */
  repairing: boolean;
}

export function fieldRepairView(g: Game, region: number, viewer: string | null = g.human, sight?: Uint8Array): FieldRepairView | null {
  if (!fieldRules(g)) return null;
  const r = g.regions[region];
  if (!r || airfieldsHold(g, region) <= 0) return null;
  const known = fieldKnown(g, viewer, region, sight);
  const room = airfieldsHold(g, region), lost = g.flags[key('flv', region)] ?? 0;
  const shut = g.flags[key('fsh', region)] !== undefined;
  const there = g.airFleets.reduce((s, f) => s + (f.base === region && f.ship === undefined ? f.ftr + f.bmr : 0), 0);
  const months = (shut ? 1 : 0) + lost;
  const damaged = shut || lost > 0;
  return {
    shut: known && shut, lost: known ? lost : 0, tier: fieldTier(g, region), roomNow: known ? room : -1, built: room + lost * FIELD.room,
    crowded: known && there > room, known,
    cost: known && damaged ? fieldRepairCost(g, region) : 0,
    why: !viewer ? 'Nobody to repair it.' : known ? whyNotFieldRepair(g, viewer, region) : `The field at ${r.name} is not in your hands.`,
    undo: known && viewer !== null && viewer === g.human && g.flags[key('frc', region)] !== undefined && g.flags[key('frp', region)] === g.turn,
    months: known ? months : 0,
    repairing: known && g.flags[key('frp', region)] !== undefined,
  };
}

// ---- What a card reads -----------------------------------------------------------------------------------------

export interface FieldView {
  region: number;
  name: string;
  tier: FieldTier;
  /** The aircraft the field serves now, and as built. */
  room: number;
  built: number;
  shut: boolean;
  /** Levels burned. */
  burned: number;
  /** A repair in hand: the month it was ordered; and when the field is whole again, in months from now. */
  repairing: number | null;
  wholeIn: number | null;
  /** The month it was last hit, or null. */
  hit: number | null;
  /** The price of a repair now, and why it cannot be ordered (null where it can). */
  price: number;
  why: string | null;
  /** True where the viewer may take back a repair ordered this month. */
  undo: boolean;
  /** One line for the card. */
  words: string;
}

/** A province's field as a card or the city glance shows it, or null where there is no field (or the switch is off). */
export function fieldView(g: Game, region: number, viewer: string | null = g.human): FieldView | null {
  if (!fieldRules(g)) return null;
  const r = g.regions[region];
  if (!r || airfieldsHold(g, region) <= 0) return null;
  const tier = fieldTier(g, region);
  const shut = g.flags[key('fsh', region)] !== undefined;
  const burned = g.flags[key('flv', region)] ?? 0;
  const rp = g.flags[key('frp', region)];
  const wholeIn = rp === undefined ? null : (shut ? 1 : 0) + burned;
  const room = airfieldsHold(g, region), built = room + burned * FIELD.room;
  const kind = tier === 'strip' ? 'Field strip' : 'Concrete airfield';
  const words = !shut && !burned ? `${kind}: serves ${num(room)} aircraft.`
    : `${kind}${shut ? ', runway shut: no aircraft can take off' : ''}${burned ? `${shut ? ';' : ','} ${burned === 1 ? 'one level' : `${burned} levels`} burned, it serves ${num(room)} of ${num(built)}` : ''}.${rp !== undefined ? (shut ? ' Repair in hand: open next month.' : ` Repair in hand: whole in ${wholeIn === 1 ? 'a month' : `${wholeIn} months`}.`) : ''}`;
  return {
    region, name: r.name, tier, room, built, shut, burned, repairing: rp ?? null, wholeIn, hit: g.flags[key('fht', region)] ?? null,
    price: fieldDamaged(g, region) ? fieldRepairCost(g, region) : 0,
    why: viewer ? whyNotFieldRepair(g, viewer, region) : 'Nobody to repair it.',
    undo: viewer !== null && viewer === g.human && g.flags[key('frc', region)] !== undefined && rp === g.turn,
    words,
  };
}

/** Every damaged field on the map, for the map's marks. */
export function fieldsDamaged(g: Game): { region: number; shut: boolean; burned: number; holder: string; repairing: boolean }[] {
  if (!fieldRules(g)) return [];
  const ids = new Set(Object.keys(g.flags).filter((k) => k.startsWith('fsh:') || k.startsWith('flv:')).map((k) => +k.slice(4)));
  return [...ids].sort((a, b) => a - b).filter((id) => g.regions[id]).map((id) => ({
    region: id, shut: g.flags[key('fsh', id)] !== undefined, burned: g.flags[key('flv', id)] ?? 0, holder: fieldHolder(g, g.regions[id]), repairing: g.flags[key('frp', id)] !== undefined,
  }));
}

/** The concrete mark a treasury-built airfield level leaves (arsenal.ts delivery of 'work:field' with the switch on). */
export const setConcrete = (g: Game, region: number): void => { if (fieldRules(g)) g.flags[key('afc', region)] = 1; };

