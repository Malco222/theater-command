// Units decide: an army's command at the month's end (release 2; the design director's ruling of 24 Nov 2026 15:53Z on
// the rules builder's #2 form, rules-builder/release2/soviet/form.md, Part 1 2b and section 1). Command c is Field
// radios' share plus what the army has earned in war (units.ts commandOf). What it earns is read here, once a month,
// from the month's own battles, by one test for the player's nation and every computer's alike: it rises for a month
// in which the army carries an offensive through, and falls for each of its corps taken whole, since staff officers
// go into the bag with their corps. Then the reforms whose text a cause tells are told, once, when it passes its mark.
// Off, nothing here runs: no store, no news.

import type { Game } from './types';
import { UNITS, armsOf, commandOf, earnCommand, qualityStore } from './units';
import { fights, headline } from './game';
import { DIPLOMACY, holds } from './diplomacy';

/**
 * The provinces a nation's own attacks took from an enemy this month (#2 form Part 1 2b): this month's battle reports
 * with it as the attacker and the ground taken, counted by distinct province. A walk-in after the defenders gave
 * ground counts; a field won round a town that holds out (siege), the relief of a town of its own side and a sally
 * from a besieged town do not.
 */
export function provincesTaken(g: Game, nat: string): number {
  const got = new Set<number>();
  for (let i = g.reports.length - 1; i >= 0 && g.reports[i].turn === g.turn; i--) {
    const r = g.reports[i];
    if (r.attNat === nat && r.taken && !r.siege && !r.relief && !r.sally) got.add(r.region);
  }
  return got.size;
}

/**
 * A nation's corps taken whole this month (#2 form Part 1 2b): written on the roll of the fallen as surrendered
 * (beaten with no way out, under ruling 232 or on a cut road, or inside a town that fell) or starved (cut off and out
 * of supply). A corps shattered in battle was not taken whole.
 */
export function corpsTakenWhole(g: Game, nat: string): number {
  const F = g.stories.fallen;
  let n = 0;
  for (let i = F.length - 1; i >= 0 && F[i].turn === g.turn; i--) if (F[i].nat === nat && F[i].how !== 'destroyed') n++;
  return n;
}

/** The mean experience of a nation's fighting corps, or null where it has none. */
function meanXp(g: Game, nat: string): number | null {
  let s = 0, n = 0;
  for (const c of g.corps) if (c.nat === nat && !c.dead && fights(c)) { s += c.xp; n++; }
  return n ? s / n : null;
}

/** Room given a cause built up by steps of 0.01 when it is read against a mark. */
const EPS = 1e-9;

/**
 * The reforms told by a cause (units.json land.reforms.news; #2 form Part 1 section 1): no reform remakes an army under
 * Units decide (diplomacy.ts), and its text is told once, as news, the first month's end at which a cause of the army
 * it names has passed the mark from the other side of it, while the event's own conditions hold: the Red Army's when
 * its command reaches 0.5, the German army's when its mean corps experience, having stood at 0.5 or more, falls under
 * it. The store's news keeps the month each was told, so it is told once.
 */
function reformNews(g: Game): void {
  const qs = qualityStore(g)!;
  const L = UNITS.land;
  for (const r of L.reforms.news) {
    const e = DIPLOMACY.events.find((x) => x.id === r.event);
    if (!e?.reform) continue;
    const mark = r.above ?? r.below;
    if (mark === undefined) continue;
    // The armies the reform would have remade: those of its nations whose part names training, command or most corps.
    for (const [nat, to] of Object.entries(e.reform)) {
      if (!g.nations[nat] || !Object.keys(to).some((k) => L.reforms.land.includes(k))) continue;
      const key = `${nat}:${r.cause}:${mark}`;
      if (qs.news[key] !== undefined) continue;
      const v = r.cause === 'command' ? commandOf(g, nat) : meanXp(g, nat);
      if (v === null) continue;
      const past = r.above !== undefined ? v >= mark - EPS : v < mark - EPS;
      if (!past) { qs.news[`${key}:near`] ??= g.turn; continue; }
      if (qs.news[`${key}:near`] === undefined || !(e.if ?? []).every((c) => holds(g, c))) continue;
      qs.news[key] = g.turn;
      headline(g, 'event', e.text);
    }
  }
}

/**
 * The month's end under Units decide (turn.ts, before the month turns): each army's earned command moves by the month's
 * own battles, +0.01 for a month in which its attacks took at least two provinces and -0.01 for each corps taken
 * whole, the month's change kept between 0 and 1 (#2 form Part 1 2b). The same test for the player's nation and every
 * computer's: the staff's phase is not the test, since the player's nation has none. Then the reforms' news. Off,
 * nothing.
 */
export function commandMonth(g: Game): void {
  if (!qualityStore(g)) return;
  const E = UNITS.land.earned;
  for (const n of Object.values(g.nations)) {
    const by = (provincesTaken(g, n.id) >= E.provinces ? E.month : 0) + E.corps * corpsTakenWhole(g, n.id);
    if (by && armsOf(g, n.id)) earnCommand(g, n.id, by);
  }
  reformNews(g);
}
