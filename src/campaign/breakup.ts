// Breaking a corps up into its divisions, and putting it back together (release 2; Brian, 24 November 2026 18:02Z:
// "when I click on a unit stack, I want to be able to see each individual unit apart of it. that way i can break it up
// if needed"). Behind its own switch (stage.json 'breakUp', the flag 'breakUp'): off, nothing here is read.
//
// The player picks divisions of one of his own corps standing in the field and splits them off as a corps of their
// own, in the same place: like lifting a few cards off a stack, nothing is lost or gained. The divisions picked take
// their own share of the men and the experience (the shares the corps' division cards show, armies.ts divisionsOf)
// and their own numbers and names; morale, rest, fieldworks and the moves left this month are the corps'. The general
// stays with the corps. A part rejoins the corps it came from, or another part of the same corps, where they stand
// together, and never makes a corps bigger than the corps was before it was broken up: so a stack can be broken up and
// put back, but no road is ever packed with a corps bigger than the one that was raised. No computer nation does either.

import data from '../../data/campaign/breakup.json';
import { men, removeCorps } from './game';
import { carryBattle } from './drawn';
import { divisionsOf } from './armies';
import { riflesJoin, riflesSplit } from './rifles';
import { FAR_STAGE } from './stage';
import { type Corps, type Game } from './types';

export const BREAKUP = data as unknown as { detached: string };

/** True where the player may break his corps up: the grand campaign, with the switch on. */
export const breakUpOn = (g: Game): boolean => g.flags.open === 1 && g.flags.breakUp === 1;

/** Turns breaking corps up on or off in a campaign under way. */
export function setBreakUp(g: Game, on: boolean): void {
  if (on) g.flags.breakUp = 1;
  else delete g.flags.breakUp;
}

/** Whose month it is, with the player at the table. */
const hisMonth = (g: Game): boolean => g.human !== null && g.order[g.phase] === g.human;

/** Why this corps cannot have these divisions (by their place in it, from 0) split off now, or null when it can. */
export function whyNotBreakUp(g: Game, c: Corps | undefined, slots: number[]): string | null {
  if (!breakUpOn(g)) return 'Corps are broken up only where that is switched on.';
  if (!c || c.dead) return 'There is no such corps.';
  if (c.nat !== g.human || !hisMonth(g)) return 'Only your own corps can be broken up, in your own month.';
  if (c.kind === 'fort') return 'Fortress troops stay together in their works.';
  if (c.divs < 2) return 'A single division cannot be broken up any further.';
  if (c.town !== undefined) return 'A garrison is broken up in the field: order it out of the town first.';
  if (c.joint !== undefined) return "It marches with an ally's army, and takes that army's orders.";
  if (c.leave !== undefined) return 'It is at home, out of the line.';
  if (c.school) return 'It is at the battle schools this month.';
  const picked = new Set(slots);
  if (picked.size === 0 || [...picked].some((i) => !Number.isInteger(i) || i < 0 || i >= c.divs)) return 'Pick the divisions to split off.';
  if (picked.size >= c.divs) return 'Some divisions have to stay with the corps.';
  return null;
}

/**
 * Splits the divisions picked (by their place in the corps, from 0) off as a corps of their own, standing where the
 * corps stands, and returns it; null where it cannot be done (whyNotBreakUp says why). Nothing is drawn from the dice.
 */
export function breakUp(g: Game, c: Corps, slots: number[]): Corps | null {
  if (whyNotBreakUp(g, c, slots) !== null) return null;
  const cards = divisionsOf(g, c);
  const picked = new Set(slots);
  const go = cards.filter((d) => picked.has(d.slot)), stay = cards.filter((d) => !picked.has(d.slot));
  const sum = (ds: typeof cards, f: (d: (typeof cards)[number]) => number): number => ds.reduce((n, d) => n + f(d), 0);
  // The men each part takes, as the division cards show them; together exactly the corps' own.
  let goMen = sum(go, (d) => d.str), stayMen = c.str * c.divs - goMen;
  let goXp = sum(go, (d) => d.xp * d.str) / Math.max(1e-9, goMen);
  // A corps its row keeps small (its establishment, 'full', below 1; under the far switch): no part stands above its
  // full, the design director's ruling 3 of 06:04Z, 26 November 2026: "Rifles follow the men who carry them and no part
  // stands above its full: a part's full is its share of the parent's establishment; men and rifles past it stay with
  // the part left behind." The division cards spread around the corps' strength, so the cards picked can hold more men
  // than the part's full: those stay with the corps. Where the cards left behind would stand above theirs, the part
  // takes the men past it, up to its own full (they bring their experience). Rifles follow in riflesSplit.
  if (FAR_STAGE && c.full !== undefined && c.full < 1) {
    const all = c.str * c.divs, to = Math.min(c.full * go.length, Math.max(goMen, all - c.full * stay.length));
    if (to > goMen && stayMen > 1e-9) goXp = (goXp * goMen + ((c.xp * all - goXp * goMen) / stayMen) * (to - goMen)) / to;
    goMen = to;
    stayMen = all - goMen;
  }
  const stayXp = stayMen > 1e-9 ? (c.xp * c.str * c.divs - goXp * goMen) / stayMen : c.xp;
  // Names: the divisions' own, kept with them wherever they go.
  const named = cards.map((d) => [d.name, d.short] as [string, string]);
  const whole = c.whole ?? cards.map((d) => d.no);
  const name = go.length === 1 ? go[0].name : `${c.name} ${BREAKUP.detached}`;
  // Made by hand, not raised (game.ts addCorps would number it afresh, give it a raw morale and a new birth): it is
  // the same men, and the record of the corps they came from is theirs.
  const p: Corps = {
    id: g.nextCorps++, nat: c.nat, name, kind: c.kind, divs: go.length, str: 1, org: c.org, xp: goXp, reg: c.reg, ap: c.ap, apMax: c.apMax,
    general: null, hold: c.hold, dyle: c.dyle, supplied: c.supplied, fought: c.fought, dead: false,
  };
  g.corps.push(p);
  carryBattle(g, c.id, p.id);
  // Its numbers are the divisions' own: never fresh ones.
  const nos = go.map((d) => d.no);
  p.dn = nos[0];
  if (nos.some((n, i) => n !== nos[0] + i)) p.nos = nos; else delete p.nos;
  const left = stay.map((d) => d.no);
  c.dn = left[0];
  if (left.some((n, i) => n !== left[0] + i)) c.nos = left; else delete c.nos;
  p.names = go.map((d) => named[d.slot]);
  c.names = stay.map((d) => named[d.slot]);
  // The men, the experience and everything the corps is doing this month.
  p.str = Math.max(0, Math.min(1, goMen / go.length));
  c.divs = stay.length;
  c.str = Math.max(0, Math.min(1, stayMen / stay.length));
  c.xp = Math.max(0, Math.min(1, stayXp));
  p.xp = Math.max(0, Math.min(1, goXp));
  for (const k of ['born', 'home', 'mor', 'full', 'dug', 'dugFor', 'digging', 'pressed', 'relief', 'blows', 'digs', 'yields', 'gaveWay', 'lean', 'line', 'wear', 'fresh', 'elite', 'airborne', 'vol', 'stay'] as const) {
    if (c[k] !== undefined) (p as unknown as Record<string, unknown>)[k] = c[k];
  }
  p.part = c.part ?? c.id;
  p.whole = whole;
  c.whole = whole;
  // It stands next to the corps it came from, in the province's order.
  const r = g.regions[c.reg];
  r.units.splice(r.units.indexOf(c) + 1, 0, p);
  riflesSplit(g, c, p);
  g.supplyDirty = true;
  return p;
}

/** The corps a part could rejoin here: the one it came from, or another part of the same corps, standing with it. */
export function rejoinTargets(g: Game, c: Corps): Corps[] {
  if (!breakUpOn(g) || c.dead) return [];
  const root = c.part ?? c.id;
  return g.regions[c.reg].units.filter((o) => o !== c && !o.dead && (o.id === root || o.part === root) && whyNotRejoin(g, c, o) === null);
}

/** Why 'part' cannot rejoin 'into' now, or null when it can. */
export function whyNotRejoin(g: Game, part: Corps | undefined, into: Corps | undefined): string | null {
  if (!breakUpOn(g)) return 'Corps are put back together only where that is switched on.';
  if (!part || !into || part.dead || into.dead || part === into) return 'There is nothing to put back together.';
  if (part.nat !== g.human || into.nat !== g.human || !hisMonth(g)) return 'Only your own corps, in your own month.';
  const root = part.part ?? part.id;
  if (!(into.id === root || into.part === root || (into.part ?? into.id) === root)) return 'A part rejoins only the corps it was split from, or another part of it.';
  if (part.reg !== into.reg || part.town !== undefined || into.town !== undefined) return 'The two must stand together in the field, in the same province.';
  if (part.kind !== into.kind) return 'The two must be of the same kind.';
  if (part.joint !== undefined || into.joint !== undefined) return "A corps marching with an ally's army cannot be put back together.";
  if (part.leave !== undefined || into.leave !== undefined || part.school || into.school) return 'Both must be in the line.';
  const most = Math.max(part.whole?.length ?? part.divs, into.whole?.length ?? into.divs);
  if (part.divs + into.divs > most) return `Together they would be bigger than the corps was: ${most} divisions at most.`;
  return null;
}

/**
 * Puts a part back into the corps it was split from (or another part of it): the divisions, the men and the
 * experience go back in, the moves left are the fewer of the two, and the part leaves the map. False where it cannot
 * be done (whyNotRejoin says why).
 */
export function rejoin(g: Game, part: Corps, into: Corps): boolean {
  if (whyNotRejoin(g, part, into) !== null) return false;
  const a = divisionsOf(g, into), b = divisionsOf(g, part);
  const all = [...a, ...b];
  const men = into.str * into.divs + part.str * part.divs;
  const xp = men > 1e-9 ? (into.xp * into.str * into.divs + part.xp * part.str * part.divs) / men : into.xp;
  const mine = into.str * into.divs, theirs = part.str * part.divs;
  if (into.mor !== undefined && part.mor !== undefined && men > 1e-9) into.mor = (into.mor * mine + part.mor * theirs) / men;
  into.org = men > 1e-9 ? (into.org * mine + part.org * theirs) / men : into.org;
  // The divisions in the order the corps had them before it was broken up, each keeping its own name.
  const seq = into.whole ?? part.whole ?? [];
  const at = (no: number): number => { const i = seq.indexOf(no); return i < 0 ? seq.length + no : i; };
  all.sort((x, y) => at(x.no) - at(y.no));
  const nos = all.map((d) => d.no);
  riflesJoin(g, part, into);
  into.divs = all.length;
  into.str = Math.max(0, Math.min(1, men / into.divs));
  into.xp = Math.max(0, Math.min(1, xp));
  into.ap = Math.min(into.ap, part.ap);
  into.dn = nos[0];
  if (nos.some((n, i) => n !== nos[0] + i)) into.nos = nos; else delete into.nos;
  into.names = all.map((d) => [d.name, d.short] as [string, string]);
  if (part.fought) into.fought = true;
  carryBattle(g, part.id, into.id);
  if (part.pressed) into.pressed = true;
  if ((part.blows ?? 0) > (into.blows ?? 0)) into.blows = part.blows;
  if ((part.whole?.length ?? 0) > (into.whole?.length ?? 0)) into.whole = part.whole;
  // Whole again: the corps it all came from needs no record of its parts, nor its divisions' names written out
  // where its own name gives them. (A corps a dated row laid may hold other divisions than its name lists, some of
  // them standing with the corps that had them already: a division is laid once, drawn.ts. Those names it keeps.)
  if (into.part === undefined && into.whole !== undefined && into.divs >= into.whole.length && !g.corps.some((o) => o !== part && !o.dead && o.part === into.id)) {
    delete into.whole;
    const written = into.names;
    delete into.names;
    if (divisionsOf(g, into).some((d, i) => d.name !== written[i]?.[0])) into.names = written;
  }
  removeCorps(g, part);
  g.supplyDirty = true;
  return true;
}

/** (For the men a card counts: what the two parts add up to.) */
/** A corps' men: the corps card's own measure, game.ts `men` (one measure of a corps' men, never a second). */
export const menOf = (c: Corps): number => men(c);
