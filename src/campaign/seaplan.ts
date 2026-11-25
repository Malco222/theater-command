// A fleet action weighed up before it is fought (Version 80). Display only.
//
// `planSeaBattle` reads the game and changes nothing: it rolls no dice, moves no ship and writes no flag, so a
// game, a save and a history hash are the same whether it is called or not. It answers the question a player
// asks before he sends a fleet to a sea where an enemy is known to be: what do I have there, what does he have
// as far as my side has found it, what are the odds, and what is likely to go down on each side.
//
// It is made from the parts the fight itself uses (navy.ts, `seaBattles`): a fleet's weight, the aircraft that
// reach from the coasts, the odds an admiral wants before he offers battle, the odds at which the weaker side
// may decline it, the swing of the dice, and the share of each class that a winner and a loser lose. The one
// difference is what it may know. The fight is settled on what is really there; the forecast counts only what the
// viewer's side has identified (carriers.ts, `fleetKnown`). A force that is only reported is listed with the word
// its report gives it and is not in the figures, and a force nobody has found is not mentioned at all.

import { clamp } from '../core/math';
import type { TicketLine } from '../core/ticket';
import { fleetKnown, fleetsSeen, seaPicture, type SeaKnown } from './carriers';
import { allied, atWar } from './game';
import { fogOn } from './intel';
import { CLASSES, NAVY, airOver, atSea, fleetPower, shipCount, surfaceShips } from './navy';
import type { Fleet, Game, ShipClass, Ships } from './types';

/** One fleet on a forecast, as the viewer knows it. */
export interface SeaPlanFleet {
  id: number;
  nat: string;
  /** Its name, or what a report calls it: "Italian ships reported". */
  name: string;
  known: SeaKnown;
  /** Its ships by class, or null where they are not identified. */
  ships: Ships | null;
  /** How many ships, or -1 where that is not known, and the report's word for a force of that size. */
  count: number;
  size: string;
  /** Its weight in a fleet action, or null where it is not identified. */
  power: number | null;
  /** Its fitness for sea, 0 to 1: the viewer's own side only. */
  ready: number | null;
  /** Its admiral: the viewer's own side only. */
  admiral: string | null;
  /** Whether it is in the figures. A force only reported is not, and nor are submarines, which fight no fleet action. */
  counted: boolean;
}

/** One side of a forecast: its fleets, their weight, and the weight of the aircraft that reach from its coasts. */
export interface SeaPlanSide { fleets: SeaPlanFleet[]; ships: number; air: number; total: number }

/** A fleet action weighed up before it is fought. */
export interface SeaPlan {
  fleet: number;
  zone: number;
  zoneName: string;
  /** Why no action can be forecast (no enemy known there, a fleet of submarines), or null. */
  why: string | null;
  ours: SeaPlanSide;
  theirs: SeaPlanSide;
  /** Enemy forces reported in this sea and not identified: their strength is not in the figures. */
  reported: number;
  /** How well the viewer's side has this sea searched, in words (carriers.ts, `seaPicture`). */
  search: string;
  /** Our strength to his: the worst the dice can make it, as it stands, and the best. Null with no enemy to weigh. */
  odds: { low: number; mid: number; high: number } | null;
  /** 0 to 1: the chance the action is won if it is fought, and that in words. */
  win: number;
  verdict: string;
  /** Whether the viewer's admiral will offer battle at these odds, and the odds he wants. */
  offers: boolean;
  seekAt: number;
  /** Who may decline battle and make for port, being outweighed, and how likely that is (0 to 1). */
  declines: { who: 'ours' | 'theirs'; chance: number } | null;
  /** The ships each side is likely to lose, by class, if the action is fought. */
  likely: { ours: Ships; theirs: Ships };
  /** The fewest and the most each side should lose across the swing of the dice. */
  range: { ours: [Ships, Ships]; theirs: [Ships, Ships] };
  /** The forecast as a ticket, in the lines the battle's own report uses. */
  lines: TicketLine[];
  /** What else the player should know before he sails, a sentence each. */
  notes: string[];
}

const VERDICT: [number, string][] = [[0.8, 'Very likely to win'], [0.6, 'Likely to win'], [0.4, 'It could go either way'], [0.2, 'Likely to lose'], [0, 'Very likely to lose']];
/** The dice swing a fleet action's odds by this much either way (navy.ts, `seaBattles`). */
const SWING: [number, number] = [0.8, 1.2];
const STEPS = 40;
const zero = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
const pct = (x: number): string => `${Math.round(x * 100)}%`;

/**
 * Odds to one decimal place, as a sentence says them: "2.4", "more than 10", and (1.5.3) "less than 0.1" where they
 * round to nothing, not "0.0". The month's sea forecast and the attack card (fleetattack.ts) both say them so.
 */
export const oddsWord = (x: number): string => (x >= 9.95 ? 'more than 10' : x < 0.05 ? 'less than 0.1' : x.toFixed(1));

/**
 * (1.5.3, playtest #33) The swing of the dice on the odds, from the worst to the best, in one sentence; said once where
 * both ends read the same: "More than 10 to 1 all day", "About 0.4 to 1 all day", "Less than 0.1 to 1 all day". Where
 * only the worse end rounds to nothing, the better end is the most there is: "No better than 0.1 to 1 all day".
 */
export function oddsSwing(low: number, high: number): string {
  const worst = oddsWord(low), best = oddsWord(high);
  if (worst === best) return `${/^\d/.test(worst) ? `About ${worst}` : `${worst[0].toUpperCase()}${worst.slice(1)}`} to 1 all day`;
  if (worst === 'less than 0.1') return `No better than ${best} to 1 all day`;
  return `Between ${worst} and ${best} to 1 as the day goes`;
}

/** The share of its ships a side loses when the two sides' strengths stand at `r` after the dice (ours to his). */
export function shares(r: number): { ours: number; theirs: number } {
  const won = r >= 1, edge = won ? r : 1 / r;
  const hard = clamp(0.14 + 0.08 * edge, 0.18, 0.5), light = clamp(0.16 / edge, 0.03, 0.14);
  return won ? { ours: light, theirs: hard } : { ours: hard, theirs: light };
}

/**
 * The ships of each class a side should lose at that share. The fight rolls for each fleet and class about this
 * figure, to the whole number below it or above: `to` rounds each fleet's figure the same way, for the fewest and the most.
 */
export function lossAt(fleets: Fleet[], share: number, to: (x: number) => number = (x) => x): Ships {
  const out = zero();
  for (const f of fleets) for (const c of CLASSES) out[c] += Math.min(f.ships[c], to(f.ships[c] * share * NAVY.classes[c].tough));
  return out;
}

const each = (s: Ships, fn: (x: number) => number): Ships => { const out = zero(); for (const c of CLASSES) out[c] = fn(s[c]); return out; };

/**
 * A fleet action in one sea, weighed up for the fleet's own side before it is fought: `id` on patrol in sea `zone`
 * with whatever else of its side patrols there, against the enemy as far as the viewer's side has found him.
 * Null when there is no such fleet or sea, or the viewer is not on the fleet's side. With nobody at the table, or
 * with the player's fog of war lifted, everything is known. Read-only.
 */
export function planSeaBattle(g: Game, id: number, zone: number, viewer: string | null = g.human): SeaPlan | null {
  const f = g.fleets.find((x) => x.id === id), sea = g.seas[zone];
  if (!f || !sea || !shipCount(f.ships)) return null;
  const nat = f.nat;
  if (viewer !== null && viewer !== nat && !allied(g, viewer, nat)) return null;
  const B = NAVY.battle;
  const seen = new Map(fleetsSeen(g, viewer).map((x) => [x.id, x]));

  // His ships at sea there, as far as they are known.
  const foes: { f: Fleet; known: SeaKnown }[] = [];
  for (const e of g.fleets) {
    if (e.zone !== zone || !atSea(e) || !atWar(g, e.nat, nat) || !shipCount(e.ships)) continue;
    const known = fleetKnown(g, e, viewer);
    if (known !== 'none') foes.push({ f: e, known });
  }
  const met = foes.filter((x) => x.known === 'exact' && surfaceShips(x.f) > 0).map((x) => x.f);
  const reported = foes.filter((x) => x.known === 'report').length;
  // Ours: the fleet itself, and every fleet of its side already on patrol there that is at war with one of them.
  const mine = [f, ...g.fleets.filter((x) => x.id !== f.id && x.zone === zone && x.stance === 'patrol' && surfaceShips(x) > 0
    && (x.nat === nat || (allied(g, x.nat, nat) && foes.some((e) => atWar(g, x.nat, e.f.nat)))))];

  const row = (x: Fleet, known: SeaKnown, own: boolean, counted: boolean): SeaPlanFleet => {
    const s = seen.get(x.id), exact = known === 'exact';
    return {
      id: x.id, nat: x.nat, name: exact ? x.name : s?.name ?? `${g.nations[x.nat].adj} ships reported`, known,
      ships: exact ? { ...x.ships } : null, count: exact ? shipCount(x.ships) : -1, size: s?.size ?? '',
      power: exact ? fleetPower(g, x) : null, ready: own ? x.ready : null, admiral: own ? x.admiral?.name ?? null : null, counted,
    };
  };
  const sum = (fs: Fleet[]): number => fs.reduce((s, x) => s + fleetPower(g, x), 0);
  const airA = airOver(g, zone, nat) * NAVY.air.weight, airD = airOver(g, zone, nat, true) * NAVY.air.weight;
  const ours: SeaPlanSide = { fleets: mine.map((x) => row(x, 'exact', true, surfaceShips(x) > 0)), ships: sum(mine), air: airA, total: sum(mine) + airA };
  const theirs: SeaPlanSide = { fleets: foes.map((x) => row(x.f, x.known, false, met.includes(x.f))), ships: sum(met), air: airD, total: sum(met) + airD };
  const search = seaPicture(g, zone, nat).word;

  const plan: SeaPlan = {
    fleet: f.id, zone, zoneName: sea.name, why: null, ours, theirs, reported, search, odds: null, win: 0, verdict: '',
    offers: false, seekAt: B.seek, declines: null, likely: { ours: zero(), theirs: zero() }, range: { ours: [zero(), zero()], theirs: [zero(), zero()] },
    lines: [], notes: [],
  };
  const unseen = `${reported === 1 ? 'One force is' : `${reported} forces are`} reported there and not identified: ${reported === 1 ? 'its' : 'their'} strength is not known.`;
  if (surfaceShips(f) <= 0) plan.why = 'Submarines fight no fleet action: they raid shipping, and escorts hunt them.';
  else if (!met.length) plan.why = `No enemy fleet is known to be at sea in the ${sea.name}.${reported ? ` ${unseen}` : ''}`;
  if (plan.why) return plan;

  // The odds as the fight works them out, on what is known: A to D, the dice swinging them by a fifth either way.
  const A = ours.total, D = Math.max(1, theirs.total);
  const mid = A / D;
  const offers = A >= B.seek * D;
  // If our admiral offers battle the dice multiply our strength; if he does not and the enemy comes to us they multiply his.
  const after = (u: number): number => (offers ? mid * u : mid / u);
  const ends = [after(SWING[0]), after(SWING[1])].sort((a, b) => a - b);
  plan.odds = { low: ends[0], mid, high: ends[1] };
  plan.offers = offers;
  if (offers && A >= B.evadeAt * D) plan.declines = { who: 'theirs', chance: B.evade };
  else if (!offers && D >= B.evadeAt * A) plan.declines = { who: 'ours', chance: B.evade };

  let wins = 0;
  const lo = zero(), lt = zero();
  for (let k = 0; k < STEPS; k++) {
    const r = after(SWING[0] + ((k + 0.5) / STEPS) * (SWING[1] - SWING[0]));
    // Equal strengths go to whoever offered battle, as in the fight.
    if (offers ? r >= 1 : r > 1) wins++;
    const s = shares(offers || r !== 1 ? r : 0.999);
    const a = lossAt(mine, s.ours), d = lossAt(met, s.theirs);
    for (const c of CLASSES) { lo[c] += a[c] / STEPS; lt[c] += d[c] / STEPS; }
  }
  plan.win = wins / STEPS;
  plan.verdict = VERDICT.find(([at]) => plan.win >= at)![1];
  plan.likely = { ours: each(lo, Math.round), theirs: each(lt, Math.round) };
  const worst = shares(ends[0]), best = shares(ends[1]);
  plan.range = {
    ours: [lossAt(mine, best.ours, Math.floor), lossAt(mine, worst.ours, Math.ceil)],
    theirs: [lossAt(met, worst.theirs, Math.floor), lossAt(met, best.theirs, Math.ceil)],
  };

  const n = (fs: Fleet[]): number => fs.reduce((s, x) => s + shipCount(x.ships), 0);
  // (1.5.3, playtest #26) "1 ship against 30", not "1 ships", as on the attack card.
  const sent = n(mine);
  plan.lines.push({ label: 'Forces', value: `${sent} ${sent === 1 ? 'ship' : 'ships'} against ${n(met)}`, effect: `Fleet strength ${Math.round(ours.ships)} against ${Math.round(theirs.ships)}` });
  if (airA > 0.5 || airD > 0.5) plan.lines.push({ label: 'Air', value: airA > airD ? 'You have the stronger air cover from the coast' : airD > airA ? 'The enemy has the stronger air cover from the coast' : 'Even', effect: `Worth ${Math.round(airA)} against ${Math.round(airD)}` });
  // (1.5.3, playtest #33) Said once where both ends read the same, as on the attack card: "More than 10 to 1 all day".
  plan.lines.push({ label: 'Odds', value: `${oddsWord(mid)} to 1`, effect: oddsSwing(ends[0], ends[1]) });
  if (reported) plan.lines.push({ label: 'Not identified', value: unseen.replace(' there', ' in this sea'), effect: 'Not in these figures' });
  plan.lines.push({ label: 'Search', value: search });
  const worn = mine.filter((x) => x.ready < 0.8);
  if (worn.length) plan.lines.push({ label: 'Readiness', value: `${worn.map((x) => x.name).join(', ')} not fully fit for sea`, effect: 'A fleet in need of refit fights below its strength' });

  plan.notes.push(offers
    ? `Your admiral will offer battle: he wants ${B.seek} to 1 and has ${oddsWord(mid)}.`
    : `At these odds your admiral will not offer battle: he wants ${B.seek} to 1 and has ${oddsWord(mid)}. An enemy who is patrolling and thinks himself strong enough will come to you.`);
  if (plan.declines) plan.notes.push(plan.declines.who === 'theirs'
    ? `Outweighed ${B.evadeAt} to 1 or more, the enemy declines battle about ${pct(B.evade)} of the time and makes for port, which leaves you the sea.`
    : `Outweighed ${B.evadeAt} to 1 or more, your fleet declines battle about ${pct(B.evade)} of the time and makes for port.`);
  plan.notes.push(`Ships that are raiding and not patrolling have to be caught first: about ${pct(B.catchRaider)} of the time each month.`);
  if (viewer !== null && fogOn(g, viewer)) plan.notes.push('What your side has not found is not in these figures.');
  return plan;
}

/** The classes a forecast or a report sets out, heaviest first, for an interface that draws them. */
export const SEA_CLASSES: ShipClass[] = CLASSES;
