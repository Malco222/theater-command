// A fleet sent straight at an enemy fleet (the 1.5.1 patch; Brian, 23 November 2026 22:23Z: "choosing to attack someone by
// dragging my unit on top of theirs", as an army attacks).
//
// Before this, fleets met only at the month's end, and only where an admiral of the side that moves thought the odds
// good enough (navy.json battle.seek, 1.15 to 1). The player who sent his fleet at a stronger one saw nothing happen:
// his admiral declined, and said nothing. Here the player gives the order himself, in his own month, and it is fought
// at once, at any odds, on the figures every fleet action uses (navy.ts fleetAction). He is told the odds first
// (planFleetAttack, read-only, as planSeaBattle is), on what his side has found of the enemy and nothing more.
//
// At sea the enemy is attacked where he is, with every fleet of his side patrolling that sea; ships that are not
// patrolling (raiders, escorts, hunters) have to be found first (battle.catchRaider), and an enemy outweighed by
// battle.evadeAt or more may make for port (battle.evade). In harbour every fleet of his side in that port fights, at
// the share of its weight that ships at anchor fight with, and the port's fortress guns add to the defence where its
// works serve the side that holds it (data/campaign/fleetattack.json). A fleet attacks once a month and then stays
// where the battle leaves it until the next.
//
// In the 1.5.1 patch (Brian, 24 November 2026 19:47Z: "yes to all") it is on in every grand campaign, new or saved, and
// the player can switch it off in OPTIONS ("Fleets attack fleets"): the grand campaign's flag 'fleetAttack' is 0 then.
// Off, nothing here is read, no die is thrown, and every war is the war without it.

import data from '../../data/campaign/fleetattack.json';
import type { TicketLine } from '../core/ticket';
import { fleetKnown, fleetsSeen, seaPicture, type SeaKnown } from './carriers';
import { allied, atWar, headline, worksFor } from './game';
import { fogOn } from './intel';
import { CLASSES, NAVY, airOver, atSea, cutOff, fleetAction, fleetName, fleetPower, fleetReach, sail, seaRng, shipCount, surfaceShips } from './navy';
import { lossAt, shares, type SeaPlanFleet, type SeaPlanSide } from './seaplan';
import type { Fleet, Game, SeaReport, Ships } from './types';

export const FLEET_ATTACK = data as unknown as { atAnchor: number; gunsPerLevel: number };

/** True where this campaign lets the player send a fleet straight at an enemy fleet. */
export const fleetAttackOn = (g: Game): boolean => g.flags.open === 1 && g.flags.fleetAttack !== 0;

/** Turns the direct fleet attack on or off. A battle already fought stays fought either way. */
export function setFleetAttack(g: Game, on: boolean): void {
  if (on) delete g.flags.fleetAttack;
  else g.flags.fleetAttack = 0;
}

const cap = (s: string): string => s[0].toUpperCase() + s.slice(1);
/** A sea named in a sentence, as the 1.5 patch's other sea sentences name it. */
const seaWhere = (name: string): string => `in the ${name}`;
const zero = (): Ships => ({ bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 });
const pct = (x: number): string => `${Math.round(x * 100)}%`;
const oneDp = (x: number): string => (x >= 9.95 ? 'more than 10' : x.toFixed(1));
const sum = (g: Game, fs: Fleet[]): number => fs.reduce((s, x) => s + fleetPower(g, x), 0);
const shipsIn = (fs: Fleet[]): number => fs.reduce((s, x) => s + shipCount(x.ships), 0);
const listNames = (fs: Fleet[]): string => { const l = fs.map(fleetName); return l.length === 1 ? l[0] : `${l.slice(0, -1).join(', ')} and ${l[l.length - 1]}`; };
/**
 * (1.5.3, playtest #25) Enemy fleets in a sentence, as the viewer's side knows them: a fleet it has identified by its
 * name, and one only reported as the map has it, "those British ships" (once a nation), with no name and no port.
 * `one` is a single fleet, named: it takes "is", "it" and "declines".
 */
function foeWords(g: Game, fs: Fleet[], named: (x: Fleet) => boolean): { text: string; one: boolean } {
  const known = fs.filter(named).map(fleetName);
  const unknown = [...new Set(fs.filter((x) => !named(x)).map((x) => `those ${g.nations[x.nat].adj} ships`))];
  const l = [...known, ...unknown];
  return { text: l.length === 1 ? l[0] : `${l.slice(0, -1).join(', ')} and ${l[l.length - 1]}`, one: l.length === 1 && !unknown.length };
}
const identified = (g: Game, viewer: string | null) => (x: Fleet): boolean => fleetKnown(g, x, viewer) === 'exact';

/** The swing of the dice on a fleet action's odds, either way (navy.ts fleetAction), and the steps the forecast walks it in. */
const SWING: [number, number] = [0.8, 1.2];
const STEPS = 40;
const VERDICT: [number, string][] = [[0.8, 'Very likely to win'], [0.6, 'Likely to win'], [0.4, 'It could go either way'], [0.2, 'Likely to lose'], [0, 'Very likely to lose']];

/** Where the fight would be: the sea, and the harbour when the enemy lies in one. */
interface Ground { harbour: boolean; zone: number; port: string | null; shore: number; anchor: number }

/** The sea a harbour attack goes in from: the harbour's own sea where the fleet reaches it, or else the first of its coast's seas it reaches. */
function groundOf(g: Game, f: Fleet, t: Fleet): Ground | null {
  const reach = fleetReach(g, f);
  if (atSea(t)) return reach.includes(t.zone) ? { harbour: false, zone: t.zone, port: null, shore: 0, anchor: 1 } : null;
  if (t.base < 0) return null;
  const r = g.regions[t.base];
  const zone = reach.includes(t.home) ? t.home : r.seas.find((z) => reach.includes(z));
  if (zone === undefined) return null;
  const lv = r.fort && worksFor(g, r, r.ctrl) ? r.fort.lv : 0;
  return { harbour: true, zone, port: t.port, shore: lv * FLEET_ATTACK.gunsPerLevel, anchor: FLEET_ATTACK.atAnchor };
}

/** The player's own fleet, in his own month, or null. */
const ownFleet = (g: Game, id: number): Fleet | null => {
  const f = g.fleets.find((x) => x.id === id);
  return f && f.nat === g.human && g.order[g.phase] === f.nat ? f : null;
};

/** Why fleet `id` cannot be sent at enemy fleet `target` now, or null if it can. Read-only. */
export function whyNotFleetAttack(g: Game, id: number, target: number): string | null {
  if (!fleetAttackOn(g)) return 'Fleets are sent straight at the enemy only where that is switched on.';
  const f = ownFleet(g, id);
  if (!f) return 'Only your own fleet can be given the order, in your own month.';
  if (!shipCount(f.ships)) return `The ${f.name} has no ships.`;
  if (surfaceShips(f) <= 0) return 'Submarines fight no fleet action: they raid shipping, and escorts hunt them.';
  if (f.attacked === g.turn) return `The ${f.name} has already been sent at the enemy this month.`;
  // (1.5.2) Cut off at sea beyond a strait shut to it, it reaches nothing until it has a harbour on its side.
  if (cutOff(g, f)) return `The ${f.name} is cut off beyond a shut strait: send it to harbour on its side first.`;
  const t = g.fleets.find((x) => x.id === target);
  if (!t || !shipCount(t.ships) || fleetKnown(g, t, f.nat) === 'none') return 'No such enemy fleet is known.';
  if (!atWar(g, f.nat, t.nat)) return `${g.nations[t.nat]?.name ?? t.nat} is not at war with ${g.nations[f.nat].name}.`;
  if (surfaceShips(t) <= 0) return 'Submarines are not brought to a fleet action: send ships to hunt them in their sea.';
  if (!groundOf(g, f, t)) {
    // (1.5.3, playtest #25) A fleet only reported keeps its name and its port to itself: the sea it is in, or that it is in harbour.
    const foe = foeWords(g, [t], identified(g, f.nat));
    if (!foe.one) return `${cap(foe.text)} are ${atSea(t) ? seaWhere(g.seas[t.zone].name) : 'in harbour'}, out of reach of ${fleetName(f)} from ${f.port}.`;
    return atSea(t)
      ? `${cap(fleetName(t))} is ${seaWhere(g.seas[t.zone].name)}, out of reach of ${fleetName(f)} from ${f.port}.`
      : `${t.port} is out of reach of ${fleetName(f)} from ${f.port}.`;
  }
  return null;
}

/**
 * Ours: the fleet, and every fleet of its side already patrolling that sea that is at war with the enemy's side and has
 * not fought in an attack this month. (1.5.1, the playtester, 25 November 2026: a fleet that joined one attack joined the
 * next as well, so the enemy could be fought five times in a month by five destroyers split off it; a fleet that fights
 * in an attack has attacked, orderFleetAttack.)
 */
function oursIn(g: Game, f: Fleet, zone: number, foe: string): Fleet[] {
  return [f, ...g.fleets.filter((x) => x.id !== f.id && x.zone === zone && x.stance === 'patrol' && surfaceShips(x) > 0
    && x.attacked !== g.turn && (x.nat === f.nat || allied(g, x.nat, f.nat)) && atWar(g, x.nat, foe))];
}

/** Every fleet of ours that fought in the attack has attacked this month: it stays where the battle left it until the next. */
function foughtIn(g: Game, mine: Fleet[]): void {
  for (const x of mine) x.attacked = g.turn;
}

/** His: the target, and every fleet of his side that stands with it (patrolling that sea, or lying in that harbour). */
function theirsAt(g: Game, f: Fleet, t: Fleet, at: Ground): Fleet[] {
  const side = (x: Fleet): boolean => x.id !== t.id && (x.nat === t.nat || allied(g, x.nat, t.nat)) && atWar(g, x.nat, f.nat) && shipCount(x.ships) > 0;
  const rest = at.harbour
    ? g.fleets.filter((x) => side(x) && !atSea(x) && x.base === t.base)
    : g.fleets.filter((x) => side(x) && atSea(x) && x.zone === t.zone && x.stance === 'patrol');
  return [t, ...rest];
}

/** An attack weighed up before it is ordered. */
export interface FleetAttackPlan {
  fleet: number;
  target: number;
  /** At sea, or in the enemy's harbour. */
  where: 'sea' | 'harbour';
  /** The sea the fight is in (for a harbour, the sea the attack goes in from), and the harbour's name. */
  zone: number;
  zoneName: string;
  port: string | null;
  /** Why it cannot be ordered, or null. */
  why: string | null;
  ours: SeaPlanSide;
  theirs: SeaPlanSide;
  /** In harbour: what the port's batteries add to the defence, and the share of their weight ships at anchor fight with. */
  shore: number;
  anchor: number;
  /** Enemy forces with the target not identified: not in the figures. */
  reported: number;
  /** Our strength to his: the worst the dice can make it, as it stands, and the best. Null with nothing of his known. */
  odds: { low: number; mid: number; high: number } | null;
  /** 0 to 1: the chance the action is won if it is fought, and that in words. */
  win: number;
  verdict: string;
  /** 0 to 1: the chance he is found at all (1 for a fleet patrolling or in harbour). */
  found: number;
  /** 0 to 1: the chance he makes for port rather than fight, outweighed at sea. */
  slips: number;
  /** What the computer's admirals want before they offer battle, and whether these odds would have done. */
  seekAt: number;
  enough: boolean;
  likely: { ours: Ships; theirs: Ships };
  range: { ours: [Ships, Ships]; theirs: [Ships, Ships] };
  lines: TicketLine[];
  notes: string[];
}

/**
 * Fleet `id` sent at enemy fleet `target`, weighed up for the player's side before the order is given: the odds, the
 * chance it is won, what each side is likely to lose, and the chances he is not found or slips away. It counts only
 * what the viewer's side has found. Null where there is no such fleet. Read-only: it rolls no dice and writes nothing.
 */
export function planFleetAttack(g: Game, id: number, target: number, viewer: string | null = g.human): FleetAttackPlan | null {
  const f = g.fleets.find((x) => x.id === id), t = g.fleets.find((x) => x.id === target);
  if (!f || !t || !shipCount(f.ships)) return null;
  if (viewer !== null && viewer !== f.nat && !allied(g, viewer, f.nat)) return null;
  const B = NAVY.battle;
  const at = groundOf(g, f, t) ?? { harbour: !atSea(t), zone: atSea(t) ? t.zone : t.home, port: atSea(t) ? null : t.port, shore: 0, anchor: atSea(t) ? 1 : FLEET_ATTACK.atAnchor };
  const sea = g.seas[at.zone];
  const seen = new Map(fleetsSeen(g, viewer).map((x) => [x.id, x]));
  const mine = oursIn(g, f, at.zone, t.nat);
  const foes = theirsAt(g, f, t, at).map((x) => ({ f: x, known: fleetKnown(g, x, viewer) as SeaKnown })).filter((x) => x.known !== 'none');
  const met = foes.filter((x) => x.known === 'exact' && surfaceShips(x.f) > 0).map((x) => x.f);
  const reported = foes.filter((x) => x.known === 'report').length;
  const row = (x: Fleet, known: SeaKnown, own: boolean, counted: boolean): SeaPlanFleet => {
    const s = seen.get(x.id), exact = known === 'exact';
    return {
      id: x.id, nat: x.nat, name: exact ? x.name : s?.name ?? `${g.nations[x.nat].adj} ships reported`, known,
      ships: exact ? { ...x.ships } : null, count: exact ? shipCount(x.ships) : -1, size: s?.size ?? '',
      power: exact ? fleetPower(g, x) : null, ready: own ? x.ready : null, admiral: own ? x.admiral?.name ?? null : null, counted,
    };
  };
  const airA = airOver(g, at.zone, f.nat) * NAVY.air.weight, airD = airOver(g, at.zone, f.nat, true) * NAVY.air.weight;
  const ours: SeaPlanSide = { fleets: mine.map((x) => row(x, 'exact', true, true)), ships: sum(g, mine), air: airA, total: sum(g, mine) + airA };
  const theirs: SeaPlanSide = { fleets: foes.map((x) => row(x.f, x.known, false, met.includes(x.f))), ships: sum(g, met), air: airD, total: sum(g, met) * at.anchor + at.shore + airD };
  const plan: FleetAttackPlan = {
    fleet: f.id, target: t.id, where: at.harbour ? 'harbour' : 'sea', zone: at.zone, zoneName: sea.name, port: at.port,
    why: whyNotFleetAttack(g, id, target), ours, theirs, shore: at.shore, anchor: at.anchor, reported, odds: null, win: 0, verdict: '',
    found: at.harbour || t.stance === 'patrol' ? 1 : B.catchRaider, slips: 0, seekAt: B.seek, enough: false,
    likely: { ours: zero(), theirs: zero() }, range: { ours: [zero(), zero()], theirs: [zero(), zero()] }, lines: [], notes: [],
  };
  const unseen = `${reported === 1 ? 'One force is' : `${reported} forces are`} reported with it and not identified: ${reported === 1 ? 'its' : 'their'} strength is not known.`;
  if (!met.length) {
    plan.notes.push(`${unseen} What your side has not found is not in these figures.`);
    return plan;
  }

  // The odds as the fight works them out, on what is known: the attacker's strength to the defender's, the dice
  // swinging them by a fifth either way. The attacker gave the order, so an even fight is his (navy.ts fleetAction).
  const A = ours.total, D = Math.max(1, theirs.total);
  const mid = A / D;
  plan.odds = { low: mid * SWING[0], mid, high: mid * SWING[1] };
  plan.enough = A >= B.seek * D;
  if (!at.harbour && A >= B.evadeAt * D) plan.slips = B.evade;
  let wins = 0;
  const lo = zero(), lt = zero();
  for (let k = 0; k < STEPS; k++) {
    const r = mid * (SWING[0] + ((k + 0.5) / STEPS) * (SWING[1] - SWING[0]));
    if (r >= 1) wins++;
    const s = shares(r);
    const a = lossAt(mine, s.ours), d = lossAt(met, s.theirs);
    for (const c of CLASSES) { lo[c] += a[c] / STEPS; lt[c] += d[c] / STEPS; }
  }
  plan.win = wins / STEPS;
  plan.verdict = VERDICT.find(([v]) => plan.win >= v)![1];
  const each = (s: Ships): Ships => { const out = zero(); for (const c of CLASSES) out[c] = Math.round(s[c]); return out; };
  plan.likely = { ours: each(lo), theirs: each(lt) };
  const worst = shares(plan.odds.low), best = shares(plan.odds.high);
  plan.range = {
    ours: [lossAt(mine, best.ours, Math.floor), lossAt(mine, worst.ours, Math.ceil)],
    theirs: [lossAt(met, worst.theirs, Math.floor), lossAt(met, best.theirs, Math.ceil)],
  };

  plan.lines.push({ label: 'Forces', value: `${shipsIn(mine)} ships against ${shipsIn(met)}`, effect: `Fleet strength ${Math.round(ours.ships)} against ${Math.round(theirs.ships)}` });
  if (at.harbour) {
    plan.lines.push({ label: 'In harbour', value: `${cap(listNames(met))} at anchor at ${at.port}`, effect: `Ships at anchor fight at ${pct(at.anchor)} of their weight` });
    if (at.shore > 0) plan.lines.push({ label: 'Shore batteries', value: `The guns of ${at.port}`, effect: `Worth ${Math.round(at.shore)} to the defence` });
  }
  if (airA > 0.5 || airD > 0.5) plan.lines.push({ label: 'Air', value: airA > airD ? 'You have the stronger air cover from the coast' : airD > airA ? 'The enemy has the stronger air cover from the coast' : 'Even', effect: `Worth ${Math.round(airA)} against ${Math.round(airD)}` });
  plan.lines.push({ label: 'Odds', value: `${oneDp(mid)} to 1`, effect: `Between ${oneDp(plan.odds.low)} and ${oneDp(plan.odds.high)} to 1 as the day goes` });
  if (reported) plan.lines.push({ label: 'Not identified', value: unseen, effect: 'Not in these figures' });
  if (!at.harbour) plan.lines.push({ label: 'Search', value: seaPicture(g, at.zone, f.nat).word });
  const worn = mine.filter((x) => x.ready < 0.8);
  if (worn.length) plan.lines.push({ label: 'Readiness', value: `${worn.map((x) => x.name).join(', ')} not fully fit for sea`, effect: 'A fleet in need of refit fights below its strength' });

  plan.notes.push(plan.enough
    ? `Your admiral would have offered battle at these odds anyway: this fights it now, not at the month's end.`
    : `Your admiral would not offer battle at these odds (he wants ${B.seek} to 1 and has ${oneDp(mid)}): it is your order, and it will be carried out.`);
  if (plan.found < 1) {
    const foe = foeWords(g, [t], identified(g, viewer));
    plan.notes.push(`${cap(foe.text)} ${foe.one ? 'is' : 'are'} not patrolling, so ${foe.one ? 'it has' : 'they have'} to be found first: about ${pct(plan.found)} of the time. Not found, your fleet patrols ${seaWhere(sea.name)}.`);
  }
  if (plan.slips > 0) plan.notes.push(`Outweighed ${B.evadeAt} to 1 or more, the enemy makes for port about ${pct(plan.slips)} of the time, which leaves you the sea.`);
  if (at.harbour) plan.notes.push('A fleet in harbour cannot make for port: it is there, and it fights where it lies.');
  if (mine.length > 1) plan.notes.push(`${cap(listNames(mine.slice(1)))}, already patrolling ${seaWhere(sea.name)}, ${mine.length === 2 ? 'joins' : 'join'} the attack.`);
  plan.notes.push('The side that loses breaks off and makes for its home port. A fleet attacks once a month, and stays where the battle leaves it until the next.');
  if (viewer !== null && fogOn(g, viewer)) plan.notes.push('What your side has not found is not in these figures.');
  return plan;
}

/** What came of an attack: fought (with its report), not found, or declined by an enemy who made for port. */
export interface FleetAttackResult {
  outcome: 'fought' | 'missed' | 'declined';
  report: SeaReport | null;
  /** One sentence for the card that follows the order. */
  text: string;
}

/**
 * Sends the player's fleet `id` straight at enemy fleet `target`, and fights it at once. The fleet sails to the sea
 * (patrolling it), and every fleet of its side already patrolling there joins in; the enemy stands with every fleet
 * of his side in that sea, or in that harbour. Null where the order cannot be given (whyNotFleetAttack).
 */
export function orderFleetAttack(g: Game, id: number, target: number): FleetAttackResult | null {
  if (whyNotFleetAttack(g, id, target)) return null;
  const f = ownFleet(g, id)!, t = g.fleets.find((x) => x.id === target)!;
  const at = groundOf(g, f, t)!;
  // (1.5.3, playtest #25) The enemy as the player's side knew him when the order was given: a fleet only reported stays unnamed.
  const named = new Set(theirsAt(g, f, t, at).filter(identified(g, f.nat)).map((x) => x.id));
  const foes = (fs: Fleet[]): { text: string; one: boolean } => foeWords(g, fs, (x) => named.has(x.id));
  if (!sail(g, f, at.zone, 'patrol')) return null;
  f.attacked = g.turn;
  const news = g.headlines.length;
  const told = (): string => g.headlines.slice(news).reverse().find((h) => h.kind === 'battle')?.text ?? '';
  const zone = g.seas[at.zone];
  const rng = seaRng(g, `attack:${f.id}:${t.id}`);
  const mine = oursIn(g, f, at.zone, t.nat);
  let met = theirsAt(g, f, t, at).filter((x) => surfaceShips(x) > 0);
  if (!at.harbour) {
    // Ships that are not patrolling have to be found first; those that are stand with him.
    if (t.stance !== 'patrol' && !rng.chance(NAVY.battle.catchRaider)) {
      const foe = foes([t]);
      const text = `${cap(fleetName(f))} searched ${seaWhere(zone.name)} for ${foe.text} and did not find ${foe.one ? 'it' : 'them'}.`;
      headline(g, 'event', text, f.nat);
      return { outcome: 'missed', report: null, text };
    }
    const airA = airOver(g, at.zone, f.nat) * NAVY.air.weight, airD = airOver(g, at.zone, f.nat, true) * NAVY.air.weight;
    const A = sum(g, mine) + airA, D = Math.max(1, sum(g, met) + airD);
    if (A >= NAVY.battle.evadeAt * D && rng.chance(NAVY.battle.evade)) {
      for (const x of met) sail(g, x, x.home, 'port');
      const foe = foes(met);
      const text = `${cap(foe.text)} ${foe.one ? 'declines' : 'decline'} battle ${seaWhere(zone.name)} and ${foe.one ? 'makes' : 'make'} for port.`;
      headline(g, 'event', text, met[0].nat, undefined, { arm: 'sea', won: f.nat, lost: met[0].nat });
      return { outcome: 'declined', report: null, text };
    }
    foughtIn(g, mine);
    const report = fleetAction(g, zone, f.nat, mine, met, rng, airA, airD);
    return { outcome: 'fought', report, text: told() };
  }
  met = met.filter((x) => !atSea(x));
  const airA = airOver(g, at.zone, f.nat) * NAVY.air.weight, airD = airOver(g, at.zone, f.nat, true) * NAVY.air.weight;
  foughtIn(g, mine);
  const report = fleetAction(g, zone, f.nat, mine, met, rng, airA, airD, { port: at.port!, shore: at.shore, anchor: at.anchor });
  return { outcome: 'fought', report, text: told() };
}

/** The enemy fleets the player's fleet `id` can be sent at now, nearest first: for the map to light up when he picks it up. */
export function attackTargets(g: Game, id: number): number[] {
  const f = ownFleet(g, id);
  if (!f || !fleetAttackOn(g)) return [];
  return g.fleets.filter((t) => t.nat !== f.nat && whyNotFleetAttack(g, id, t.id) === null).map((t) => t.id);
}
