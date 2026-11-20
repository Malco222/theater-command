// The readiness test: the month Japan strikes where no book dates it (ruling 200(m), and rider b586 to ruling 234;
// the Pacific planner's paper 14, section 6; the figures are data/campaign/pacific.json, 'ready').
//
// At the stops of the history dial that keep the book the strike south is dated: December 1941. Where no book dates
// it (a looser stop, or the Pacific's book closed) ruling 197 gives Japan's staff a window, not before the third
// month after the freeze and not after the sixth, and leaves the month inside it to the staff. This file is that
// month. Nothing here happens while the far side's switch is off, and a player's Japan is bound by none of it.
//
//   NO FREEZE, NO STRIKE. A Japan that is still sold its oil has no reason to fight three great powers.
//   THE FIRST MONTH is the staff's character: a bold staff may strike in the third month after the freeze, a
//     cautious one in the fourth, an opportunist in the fourth, or the third while Britain or the Soviet Union has
//     fought Germany on land within the staffs' own months for a busy enemy (rivals.json, 'effort.busy').
//   READY is three things, read as the staff's month opens:
//     1. the striking fleet (the fleet the choice's strike names) is fit for sea, by the admirals' own figure, and
//        the harbour it means to strike lies in its reach: the seas of its base and two beyond;
//     2. landing forces stand on quays within a landing's range of the first month's beaches, as many corps as the
//        sea lift carries: the corps the choice names for its landings, each in a harbour its side holds;
//     3. the test of strength passes at the beaches that lead to Singapore and to Manila: for each, at the first of
//        its beaches in enemy hands, with the choice's own corps for that beach and what the ships can bring in after
//        them. The reckoning counts as enemies the countries the choice declares on and those that follow them
//        (ashore.ts, `also`): at peace it would otherwise find nobody to count.
//   THE DRAW. From its first month, in each month it is ready, the staff strikes with a chance of one half, drawn
//     from the war's seed as its other great choices are. In the window's last month it strikes, ready or not: its
//     oil is running down.
//   THE STRIKE is the flag the great choice waits on (far-pacific-war.json, 'flag:pac:strike'); staff.ts takes the
//     choice that month, with no second draw ('sure').
//   THE GATHERING. By the book a dated row moves the carrier striking force to its northern anchorage the month
//     before the blow. Off the book the staff gives the same order on the freeze's clock, two months after the
//     freeze, so that the fleet lies in reach as the window opens (and again in any month it is found elsewhere).
//   AT STOPS 4 AND 5 (the sandbox playtest of 19 November): the window has no last month, so a staff that is never
//     ready never strikes. From what was the last month on, its oil running down, the staff no longer waits for the
//     odds: the fleet fit and the army on its quays make it ready, and it strikes on the same draw. (Without this the
//     test of strength at Manila, which in the wars measured never passed, kept a staff with America in its way at
//     home for good.) And the strike goes against those that stand in Japan's way that month (pacific.ts
//     inTheWay), with its strikes and landings cut to their ground (staff.ts asGoes). Everything above reads that
//     choice: a strike that leaves America out asks no striking fleet and gathers none, and nobody in the way is no
//     strike. Stops 1 to 3 read history's choice exactly as before.

import type { Corps, Game, Region } from './types';
import { PACIFIC, freezeAt, readyRules, type ReadyRules } from './pacific';
import { RIVALS, asGoes, characterOf, monthly, ops, strengthAsked, type Op } from './staff';
import { leash, stopDef } from './history';
import { FLEETS, NAVY, fleetReach, fleetsOf, orderedTo, seaPath, shipCount } from './navy';
import { followUp, reckon } from './ashore';
import { WORLD } from './world';
import { allied, atWar } from './game';

/** What the test reads in one month. */
export interface Readiness {
  /** Months since the freeze, and the first of them in which this staff may strike. */
  month: number; first: number;
  /** The striking fleet fit for sea with its harbour in reach. */
  fleet: boolean;
  /** Landing forces on their quays, and how many the test asks for. */
  forces: number; asked: number;
  /** The test of strength for each place the opening is for: null where it is not asked (no beach of it in enemy hands). */
  strength: Record<string, boolean | null>;
  ready: boolean;
}

/** For the measuring harness: told what the test read each month of the window, and whether the staff struck. Nothing in the game listens. */
export const readyWatch: { tell: ((e: Readiness & { nat: string; turn: number; struck: boolean; why: string }) => void) | null } = { tell: null };

/**
 * The test's figures and the choice it names the month of, while that month is the staff's own to choose: the choice
 * as it would go this month (at stops 4 and 5 against those in Japan's way, staff.ts asGoes).
 */
function testFor(g: Game, nat: string): { R: ReadyRules; op: Op } | null {
  const R = readyRules(g);
  if (!R || nat !== PACIFIC!.power || nat === g.human || !g.nations[nat]?.alive) return null;
  const op = ops(nat).find((o) => o.id === R.choice);
  if (!op || g.flags[`st:done:${nat}:${op.id}`] || g.flags[R.flag] !== undefined) return null;
  // (Where the book dates the choice, the book's month stands.)
  return leash(g, nat, op.id).kind === 'free' ? { R, op: asGoes(g, nat, op) } : null;
}

/** The countries the choice would bring to war with its own: those it declares on, their dominions and the countries the Pacific's ties name with them. */
function meant(op: Op): Set<string> {
  const out = new Set(op.declare ?? []);
  for (const [id, of] of Object.entries(WORLD.follow)) if (out.has(of)) out.add(id);
  for (const lead of op.declare ?? []) for (const id of PACIFIC!.with[lead] ?? []) out.add(id);
  return out;
}

/** The plan's landing forces as they stand: each corps the choice names that is alive, with the first of its beaches in enemy hands, and whether it stands on a quay with that beach in a landing's range. */
function forces(g: Game, nat: string, op: Op, foe: (id: string) => boolean): { c: Corps; beach: Region; quay: boolean }[] {
  const out: { c: Corps; beach: Region; quay: boolean }[] = [];
  for (const L of op.landings ?? []) {
    const c = g.corps.find((x) => x.nat === nat && x.name === L.corps && !x.dead);
    const beach = L.to.map((nm) => g.byName[nm]).find((r) => !!r && foe(r.ctrl));
    if (!c || !beach) continue;
    const q = g.regions[c.reg];
    const quay = q.port && q.seas.length > 0 && allied(g, nat, q.ctrl) && !!seaPath(g, nat, q, beach, { max: NAVY.landing.range, surprise: true });
    out.push({ c, beach, quay });
  }
  return out;
}

/** What the readiness test reads this month for a staff whose month it is to choose; null for every other. */
export function readiness(g: Game, nat: string): Readiness | null {
  const X = testFor(g, nat), freeze = freezeAt(g);
  if (!X || !freeze) return null;
  const { R, op } = X;
  const also = meant(op);
  const foe = (id: string): boolean => atWar(g, nat, id) || also.has(id);
  // The first month, by the staff's character.
  const who = characterOf(g, nat);
  const fought = (a: string, b: string): number => Math.max(g.nations[a]?.lastFight[b] ?? -99, g.nations[b]?.lastFight[a] ?? -99);
  const busy = R.busy.of.some((e) => fought(e, R.busy.with) >= g.turn - RIVALS.effort.busy);
  const first = who === 'opportunist' && busy ? Math.min(R.first[who] ?? R.window[0], R.busy.first) : R.first[who] ?? R.window[0];
  // 1. The striking fleet. (A choice that orders no strike asks for none.)
  const s = op.strikes?.[0];
  const f = s ? fleetsOf(g, nat).find((x) => x.name === s.fleet) : undefined;
  const target = s ? g.byName[s.harbour] : undefined;
  const fleet = !s || (!!f && !!target && shipCount(f.ships) > 0 && f.ready >= FLEETS.ai.ready && fleetReach(g, f).some((z) => target.seas.includes(z)));
  // 2. The landing forces on their quays.
  const F = forces(g, nat, op, foe);
  const onQuays = F.filter((x) => x.quay);
  const asked = Math.min(g.nations[nat].sealift, (op.landings ?? []).length);
  // 3. The test of strength at the beaches that lead to the places the opening is for.
  const ask = strengthAsked(g, nat);
  const strength: Record<string, boolean | null> = {};
  for (const [place, beaches] of Object.entries(R.leads)) {
    const p = g.byName[place];
    const b = p && foe(p.ctrl) ? beaches.map((nm) => g.byName[nm]).find((r) => !!r && foe(r.ctrl)) : undefined;
    if (!b) { strength[place] = null; continue; }
    const wave = onQuays.filter((x) => x.beach === b).map((x) => x.c);
    if (!wave.length) { strength[place] = false; continue; }
    const K = ask === null ? null : reckon(g, nat, b, wave, followUp(g, nat, g.regions[wave[0].reg], wave, b), undefined, also);
    strength[place] = !K || K.chance >= ask!;
  }
  // (At stops 4 and 5, from the window's last month, its oil running down, the staff no longer waits for the odds:
  // the fleet fit and the army on its quays are enough. Nothing forces the blow there; see readinessMonth.)
  const month = g.turn - freeze, pressed = !!stopDef(g).ambitions && month >= R.window[1];
  const ready = fleet && onQuays.length >= asked && (pressed || Object.values(strength).every((v) => v !== false));
  return { month, first, fleet, forces: onQuays.length, asked, strength, ready };
}

/**
 * The staff's month (ai.ts, before its great choices are read): the gathering, then the test. Raises the flag the
 * great choice waits on in the month the staff strikes.
 */
export function readinessMonth(g: Game, nat: string): void {
  const X = testFor(g, nat), freeze = freezeAt(g);
  if (!X || !freeze) return;
  const { R, op } = X;
  const m = g.turn - freeze;
  // The gathering: the striking fleet to the anchorage its strike is flown from. (At stops 4 and 5 only while a strike
  // it flies is still meant: one that leaves America out flies none.)
  if (m >= R.gather.after && (!stopDef(g).ambitions || (op.strikes ?? []).some((s) => s.fleet === R.gather.fleet))) {
    const f = fleetsOf(g, nat).find((x) => x.name === R.gather.fleet);
    if (f) orderedTo(g, f, R.gather);
  }
  if (m < R.window[0]) return;
  const now = readiness(g, nat);
  if (!now) return;
  let struck = false, why: string;
  // (At stops 4 and 5 there is no last month, and nobody in the way is no strike: the sandbox playtest of 19 November.)
  const loose = !!stopDef(g).ambitions;
  if (m >= R.window[1] && !loose) { struck = true; why = 'the last month of the window'; }
  else if (loose && !(op.declare ?? []).length) why = 'nobody in its way';
  else if (m < now.first) why = 'before its first month';
  else if (!now.ready) why = 'not ready';
  else { struck = monthly(g, nat, R.choice, 'ready', g.turn) < R.chance; why = struck ? 'ready, and the draw for it' : 'ready, and the draw against it'; }
  if (struck) g.flags[R.flag] = g.turn;
  readyWatch.tell?.({ ...now, nat, turn: g.turn, struck, why });
}
