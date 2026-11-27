// Churned ground's record (release 2; the campaign map look's "fronts that feel alive", piece 1). Where two lines have
// stood dug in facing each other for months, the 3D map beats the ground between them into a band of broken earth, and
// once the front moves on the ground heals where it lies. The table used to remember that itself, so a war taken up
// again showed nothing healing from before it was saved. Now the rules keep the record at the month's end and the
// table reads it. (The design director's rulings of 27 November 2026, 19:36Z and 21:05Z, on the short form
// map-looks/fronts-alive/churn-healing-row-FORM.md; the owners' yes of 21:07Z to 21:09Z.)
//
// A RECORD, NEVER A RULE. Nothing in the war reads it: no plan, no battle, no price. Only this file and save.ts touch
// g.churned, and the bridge's view reads it through churnedView. Every reader it calls writes nothing (fieldCtrl,
// atWar, allied, sightOf, corpsKnown). With its switch off it writes nothing at all, so such a war is saved byte for
// byte as before.
//
// The switch is the map's own (tc.Map.FrontChurn, the release 2 ship list's row frontchurn): the table lays this one
// from it as a war is set up or read in (TC.churnSwitch), so the two are one switch with one setter. Off after on
// deletes the record, so no field is left that nothing reads.
import { allied, atWar } from './game';
import { SIGHT, corpsKnown, fogOn, sightOf } from './intel';
import { fieldCtrl } from './siege';
import type { Game } from './types';

type ChurnRow = NonNullable<Game['churned']>[number];

export const CHURN = {
  /**
   * The art director's healing year (her sheet art-director/fronts-alive/fronts-alive-look.md, 27 November 2026): the
   * ground between two lines heals over twelve months once the front moves on. Hers, not the rules': here only as the
   * record's storage bound (a scar is dropped once it is healed). The bridge sends it with the list and the table draws
   * its healing over it, so a change to her sheet is this one line. Her other figure, the band whole at the sixth month,
   * is the table's drawing alone: the record's months are facts and are never capped.
   */
  year: 12,
  /** At most so many entries in the bridge's list (four numbers each). */
  sent: 600,
} as const;

/** The record is kept (the grand campaign only). */
export const churnedOn = (g: Game): boolean => g.flags.open === 1 && g.flags.churned === 1;

/** The one setter. On: the record is kept from the next month's end. Off: the record and the switch are gone. */
export function setChurned(g: Game, on: boolean): boolean {
  if (on && g.flags.open === 1) g.flags.churned = 1;
  else {
    delete g.flags.churned;
    delete g.churned;
  }
  return churnedOn(g);
}

/**
 * At the month's end (turn.ts, once, after the mines): each pair of neighbouring provinces whose holders are at war.
 *
 * - The foe test is fronts()' own: whose army holds each province (fieldCtrl), at war with the other's.
 * - A side is dug in where a living corps that is not a fort stands dug in there, at war with the other side's holder.
 * - ma, mb: the months each side has stood dug in facing the other, as facts; a month end without it sets it to 0.
 * - la, lb: the holders at the last month's end. A change of either is the front moving on.
 * - pm: the months the stand has stood in the player's own picture, from what he can know at each month's end and
 *   nothing more (the design director's 19:36Z and 21:05Z rulings, Q2 and Q3):
 *     his own front (one side his or his ally's): his side's months; where he knows the other side dug in as it is
 *     today, the lesser of those and the works months his cards show for it; it ends when his side stops digging, the
 *     other is known not to be dug in, or the front ends (who holds the ground, and who is at war, are always known);
 *     a front between two other countries: the months he has watched both sides dug in (mv), counted only where he
 *     knows each side's dig as his own cards know it (sight in full and the corps known as it is today); a month he
 *     cannot see adds nothing and ends nothing; it ends where a side is known not to be dug in, or the front ends;
 *     nobody seated, or the fog lifted: the lesser of ma and mb, while both stand dug in.
 *   Where the fog hides the other side, nothing about it moves pm: not its dig, not its months.
 * - When pm's stand ends, its months become a scar with the month it was left; a scar is dropped once healed.
 *
 * The first month's end that keeps the record starts each count from the works months the rules already keep
 * (dugFor), so a stand dug in before the switch does not start again from nothing; his picture's counts start from
 * what his cards show.
 */
export function churnMonth(g: Game): void {
  if (!churnedOn(g)) return;
  const N = g.regions.length;
  const first = g.churned === undefined;
  const rows = new Map<number, ChurnRow>();
  for (const r of g.churned ?? []) rows.set(r.a * N + r.b, r);
  const lord = g.regions.map((r) => fieldCtrl(g, r));
  const me = g.human;
  const fog = me !== null && fogOn(g, me);
  const seen = fog ? sightOf(g, me) : null;
  const war = (a: string, b: string): boolean => a !== b && !!g.nations[a] && !!g.nations[b] && atWar(g, a, b);
  const works = (x: number): number => Math.max(1, Math.round(x));

  // A side's dig as it is: the most works months of its corps dug in facing the other side's holder; -1 where none.
  const dug = (x: number, foe: string): number => {
    let most = -1;
    for (const c of g.regions[x].units) {
      if (c.dead || c.kind === 'fort' || !c.dug || !war(c.nat, foe)) continue;
      most = Math.max(most, c.dugFor ?? 0);
    }
    return most;
  };
  // A side's dig as the player knows it: 'dug' with the works months his cards show, 'not' where he sees every corps
  // there as it is today and none is dug in, 'unknown' otherwise.
  type Known = { state: 'dug' | 'not' | 'unknown'; works: number };
  const known = (x: number, foe: string): Known => {
    if (!seen || seen[x] < SIGHT.full) return { state: 'unknown', works: 0 };
    let most = -1, hidden = false;
    for (const c of g.regions[x].units) {
      if (c.dead || c.kind === 'fort' || !war(c.nat, foe)) continue;
      if (corpsKnown(g, c, me).level !== 'exact') { hidden = true; continue; }
      if (c.dug) most = Math.max(most, c.dugFor ?? 0);
    }
    return most >= 0 ? { state: 'dug', works: most } : { state: hidden ? 'unknown' : 'not', works: 0 };
  };

  const out: ChurnRow[] = [];
  const visit = (a: number, b: number, row: ChurnRow | undefined, apart = false): void => {
    const la = lord[a], lb = lord[b];
    const front = !apart && war(la, lb);
    const da = front ? dug(a, lb) : -1, db = front ? dug(b, la) : -1;
    if (!row && da < 0 && db < 0) return;
    const was = row ?? { a, b, la, lb, ma: 0, mb: 0, pm: 0 };
    const moved = was.la !== la || was.lb !== lb;
    const ma = da < 0 ? 0 : first ? works(da) : moved ? 1 : was.ma + 1;
    const mb = db < 0 ? 0 : first ? works(db) : moved ? 1 : was.mb + 1;
    const before = moved ? 0 : was.pm;
    let pm = 0;
    if (!front) pm = 0;
    else if (!fog) pm = ma > 0 && mb > 0 ? Math.min(ma, mb) : 0;
    else {
      const ownA = allied(g, la, me as string), ownB = !ownA && allied(g, lb, me as string);
      if (ownA || ownB) {
        // (The other side's months as his cards show them: the works of its corps he knows as they are today. The
        // record's own count for that side may hold a corps he cannot see, so it is not read here.)
        const mine = ownA ? ma : mb;
        const other = ownA ? known(b, la) : known(a, lb);
        pm = mine <= 0 || other.state === 'not' ? 0 : other.state === 'dug' ? Math.min(mine, works(other.works)) : mine;
      } else {
        const ka = known(a, lb), kb = known(b, la);
        if (ka.state === 'not' || kb.state === 'not') pm = 0;
        else if (ka.state === 'dug' && kb.state === 'dug') pm = first ? Math.min(works(ka.works), works(kb.works)) : before + 1;
        else pm = before;
      }
    }
    const scars = (was.scars ?? []).filter((s) => g.turn + 1 - s.left < CHURN.year);
    if (was.pm > 0 && (moved || pm === 0)) scars.push({ m: was.pm, left: g.turn });
    if (ma <= 0 && mb <= 0 && pm <= 0 && !scars.length) return;
    const next: ChurnRow = { a, b, la, lb, ma, mb, pm };
    if (scars.length) next.scars = scars;
    out.push(next);
  };

  const done = new Set<number>();
  for (const r of g.regions) {
    for (const j of r.nb) {
      if (j <= r.id || j >= N) continue;
      const key = r.id * N + j;
      done.add(key);
      visit(r.id, j, rows.get(key));
    }
  }
  // (A pair that is no longer two neighbours: its stand ends there, and its scars heal as any other's.)
  for (const [key, row] of rows) if (!done.has(key) && row.a < N && row.b < N) visit(row.a, row.b, row, true);
  out.sort((x, y) => x.a - y.a || x.b - y.b);
  g.churned = out;
}

/**
 * The bridge's list (fronts' sight rule: only where both provinces are seen): four numbers each, the two provinces,
 * the months, and -1 for a stand in his picture or the months since it was left for a scar. With the healing year, and
 * whether the record is kept yet (an older war before its first month's end with the switch on is not). Nothing
 * without the switch.
 */
export function churnedView(g: Game, seen: Uint8Array): { year: number; kept: boolean; pairs: number[] } | undefined {
  if (!churnedOn(g)) return undefined;
  const pairs: number[] = [];
  for (const r of g.churned ?? []) {
    if (pairs.length >= CHURN.sent * 4) break;
    if (!(seen[r.a] > 0) || !(seen[r.b] > 0)) continue;
    if (r.pm > 0) pairs.push(r.a, r.b, r.pm, -1);
    for (const s of r.scars ?? []) pairs.push(r.a, r.b, s.m, g.turn - s.left);
  }
  return { year: CHURN.year, kept: g.churned !== undefined, pairs };
}
