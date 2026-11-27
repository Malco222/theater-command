// Supply columns (release 2; Brian's yes of 23 November 2026, idea 3): every month lorry columns drive from each
// nation's railhead along the roads to its corps beyond it, and enemy aircraft or partisans can ambush them on the
// way. What is lost on the road never reaches the front: the corps it was bound for take in fewer drafts and rest the
// worse for it. That is the whole price of a loss, paid once. A nation's lorries and wagons are counted afresh every
// month (from the divisions to be fed, or with the lorry orders on from its industry and fuel, logistics.ts lorryPool),
// so the next month's count has already made good what was burned, and the treasury is not charged for it a second time
// (the design director, 18:20Z 26 November 2026: price once; card and result agree, 217). The columns never touch the
// treasury: only a forward dump, ordered and paid for like any other order, does (logistics.ts orderDump).
//
// Everything comes from what is on the ground and in the air (the core theme, Brian 23 November 07:13Z): the enemy
// aircraft in reach and the orders they fly, the side's own fighters over the road, the troops standing along it, the
// ground the road runs through and how long its people have been occupied. No number is keyed to a nation or a date.
// The record only weighs the causes (data/campaign/columns.json).
//
// The grand campaign's flag 'columns', set as a campaign opens where stage.json switches it on. Off, nothing here is
// read, no die is thrown and every war is the war without it.

import data from '../../data/campaign/columns.json';
import { Rng, hashString } from '../core/rng';
import { allied, atWar, headline } from './game';
import { railheads } from './rails';
import { airRange, cover, crowding, kmTo, skyOver } from './air';
import { wrecked } from './airversion';
import { grounded, skyAt, weatherOn } from './weather';
import { SIGHT, sightOf } from './intel';
import { aiDumps, clearDumps, dumpsOn, roadWear, shareOutLorries, tendDumps, wagonsOn } from './logistics';
import { noteFed } from './dumpfires';
import type { Corps, Game } from './types';

export const COLUMNS = data as unknown as {
  lorries: { perDiv: number };
  floor: number;
  air: { most: number; half: number; cover: number; support: number; ground: number; line: number; close: number; sky: number; flak: number; flakMost: number };
  partisans: { most: number; terrain: Record<string, number>; rampMonths: number; leaderless: number; quiet: number; garrisonHalf: number };
  roll: [number, number];
  hitAt: number;
  newsAt: number;
};

/** What ambushed a column on one stretch of road. */
export type Ambusher = 'air' | 'partisans';

/** One month's column from a nation's railhead to one province where its corps stand beyond it. */
export interface ColumnRun {
  /** The road, province by province: the railhead first, the province its corps stand in last. */
  path: number[];
  /** The divisions it feeds. */
  divs: number;
  /** The lorries that set out, and those lost on the way. */
  lorries: number;
  lost: number;
  /** The share of what was sent that reached the front: 0 to 1. */
  share: number;
  /** Where it was caught, by whom, and how many lorries were lost there. */
  hits: [number, Ambusher, number][];
  // The three supply orders (logistics.ts), each absent where its switch is off:
  /** With the nation's lorries counted: the lorries the run needed ('lorries' is then those it got). */
  need?: number;
  /** Carried by horse-drawn wagon, for want of lorries. */
  horse?: number;
  /** Not carried at all, for want of lorries (the wagons off). */
  short?: number;
  /** Held up on the road by mud, snow, sand or the wagons' slower pace: arrives too late for the month. */
  worn?: number;
  /** On the front the nation sends its lorries to first. */
  first?: boolean;
  /** Fed from this forward dump, not from the railhead (the road starts at the dump). */
  dump?: number;
  /** Stocking a forward dump (the road ends at the dump): 'divs' is then the division-months it carries. */
  fill?: boolean;
}

/** True where this campaign runs supply columns. */
export const columnsOn = (g: Game): boolean => g.flags.open === 1 && g.flags.columns === 1;

/**
 * Whether the switch can still be thrown: in the campaign's opening month only (turn 1, or the month a later start
 * opens in). A nation that has already moved that month sends its first columns the month after.
 */
export const columnsSettable = (g: Game): boolean => g.flags.open === 1 && g.turn === (g.flags.openFrom ?? 1);

/** Sets the switch. Only in the campaign's opening month; returns whether it was set. */
export function setColumns(g: Game, on: boolean): boolean {
  if (!columnsSettable(g)) return false;
  if (on) g.flags.columns = 1;
  else { delete g.flags.columns; delete g.columns; }
  return true;
}

const friendOf = (g: Game, a: string, b: string): boolean => a === b || allied(g, a, b);

/** The divisions of a nation and its allies standing in a province: they guard the road there and shoot at what comes over it. */
function guardsIn(g: Game, nat: string, p: number): number {
  let n = 0;
  for (const c of g.regions[p].units) if (!c.dead && friendOf(g, c.nat, nat)) n += c.divs;
  return n;
}

/** What a stretch of road holds for one nation's lorries this month. */
interface Stretch {
  /** How hard the enemy's aircraft press on it: 0 to 1. */
  air: number;
  /** The player's share of the aircraft hunting there (for his news): 0 to 1. */
  mine: number;
  /** How strong the partisans are there: 0 to 1. */
  partisans: number;
  /** What the troops standing on it take off an air attack: 0 to flakMost. */
  flak: number;
}

/**
 * The enemy aircraft hunting over a province, against the side's own fighters there (columns.json '_air'): the share
 * of an unguarded road's worst that they bring to bear, 0 to 1, and the player's share of the hunters.
 */
export function airOver(g: Game, nat: string, p: number): { air: number; mine: number } {
  const A = COLUMNS.air;
  if (weatherOn(g) && skyAt(g, p) === 'closed') return { air: 0, mine: 0 };
  let hunt = 0, mine = 0, guard = 0;
  for (const f of g.airFleets) {
    if (f.ftr + f.bmr <= 0) continue;
    const foe = atWar(g, f.nat, nat);
    if (!foe && !friendOf(g, f.nat, nat)) continue;
    if (grounded(g, f) || wrecked(g, f)) continue;
    const d = kmTo(g, f, p);
    if (!Number.isFinite(d)) continue;
    const R = airRange(f, g);
    if (foe) {
      const w = f.close ? A.close : f.mission === 'ground' ? A.ground : f.mission === 'line' ? A.line : f.mission === 'support' ? A.support : 0;
      if (!w) continue;
      const n = w * (f.ftr * cover(d, R.ftr) + f.bmr * cover(d, R.bmr)) * crowding(g, f);
      hunt += n;
      if (f.nat === g.human) mine += n;
    } else if (f.mission === 'support' && !f.close && f.ftr > 0) {
      guard += f.ftr * cover(d, R.ftr) * crowding(g, f);
    }
  }
  if (hunt <= 0) return { air: 0, mine: 0 };
  const sky = skyOver(g, p);
  const pressed = hunt * (sky && atWar(g, sky, nat) ? A.sky : 1);
  return { air: Math.min(1, pressed / (pressed + A.cover * guard + A.half)), mine: mine / hunt };
}

/**
 * How strong the partisans are on a province's roads for one nation's lorries (columns.json '_partisans'): 0 where its
 * people are the nation's own or an ally's, otherwise from the ground, the months of occupation, whether the people's
 * own country still fights, and the troops holding the province down.
 */
export function partisansAt(g: Game, nat: string, p: number): number {
  const P = COLUMNS.partisans, r = g.regions[p];
  const people = r.owner;
  if (!people || friendOf(g, people, nat)) return 0;
  const ramp = Math.min(1, Math.max(0, g.turn - r.since) / P.rampMonths);
  if (ramp <= 0) return 0;
  const home = g.nations[people];
  const fights = !home?.alive ? P.leaderless : atWar(g, people, nat) ? 1 : P.quiet;
  const held = guardsIn(g, nat, p);
  return (P.terrain[r.terrain] ?? P.terrain.plains) * ramp * fights * (1 - held / (held + P.garrisonHalf));
}

/** The road back from a province to the railhead, a province at a time, by the safest of the shortest ways. */
function roadFrom(g: Game, road: Int16Array, to: number, danger: (p: number) => number): number[] {
  const path = [to];
  let at = to;
  while (road[at] > 0 && path.length < 64) {
    let best = -1, worst = Infinity;
    for (const j of g.regions[at].nb) {
      if (road[j] !== road[at] - 1) continue;
      const d = danger(j);
      if (d < worst || (d === worst && j < best)) { best = j; worst = d; }
    }
    if (best < 0) break;
    path.push(best);
    at = best;
  }
  return path.reverse();
}

const word = (n: number): string => Math.round(n).toLocaleString('en-US');

/**
 * A nation's columns for the month: one from its railhead to every province where its corps stand beyond it, each
 * caught or not on every stretch of the road. Kept on the game for the month (g.columns), read by the drafts and the
 * rest its corps get (columnShare), and drawn on the map. Lorries lost cost the front what they carried, and nothing
 * more: next month's lorries are counted afresh, and the treasury is not charged for them (price once).
 */
export function columnsMonth(g: Game, nat: string): void {
  if (!columnsOn(g)) return;
  const book = (g.columns ??= {});
  delete book[nat];
  const N = g.nations[nat];
  if (!N?.alive || !N.atWar.length) return;
  const road = railheads(g, nat);
  if (!road) return;
  // Forward dumps (logistics.ts): one whose ground is lost is burned, one under enemy aircraft loses part of its
  // stock; those stocked and ready feed the corps nearer to them than to the railhead. None where the switch is off.
  const ready = tendDumps(g, nat);
  // Where its corps stand beyond the railhead, and the divisions there.
  const fed = new Map<number, number>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort' || !c.supplied || !(road[c.reg] >= 1)) continue;
    fed.set(c.reg, (fed.get(c.reg) ?? 0) + c.divs);
  }
  // The dumps ordered last month or before and not yet stocked: from the railhead by a column, or straight off the
  // train where the railway has since come up to them.
  const filling = dumpsOn(g) ? (g.dumps ?? []).filter((d) => d.nat === nat && d.fill > 0 && d.turn < g.turn && road[d.region] >= 0) : [];
  for (const d of filling) if (road[d.region] === 0) { d.stock += d.fill; d.fill = 0; }
  filling.splice(0, filling.length, ...filling.filter((d) => d.fill > 0));
  if (!fed.size && !filling.length) return;
  const C = COLUMNS;
  const seen = new Map<number, Stretch>();
  const stretch = (p: number): Stretch => {
    let s = seen.get(p);
    if (!s) {
      const a = airOver(g, nat, p);
      s = { air: a.air, mine: a.mine, partisans: partisansAt(g, nat, p), flak: Math.min(C.air.flakMost, guardsIn(g, nat, p) * C.air.flak) };
      seen.set(p, s);
    }
    return s;
  };
  const danger = (p: number): number => { const s = stretch(p); return C.air.most * s.air * (1 - s.flak) + C.partisans.most * s.partisans; };
  const runs: ColumnRun[] = [];
  for (const [to, divs] of [...fed].sort((a, b) => a[0] - b[0])) {
    // The nearest ready dump with stock for these divisions, where it is nearer than the railhead. A corps standing
    // on its dump takes its supply there and sends no column.
    let from: { dump: (typeof ready)[number]['dump']; road: Int16Array } | null = null;
    for (const r of ready) {
      const d = r.road[to];
      if (d < 0 || d >= road[to] || r.dump.stock < divs) continue;
      if (!from || d < from.road[to] || (d === from.road[to] && r.dump.region < from.dump.region)) from = r;
    }
    if (from) {
      from.dump.stock -= divs;
      // (Release 2, dumpfires.ts) Where that dump was set burning this month, the corps drawing on it are noted.
      noteFed(g, nat, from.dump.region, to);
      if (from.road[to] === 0) continue;
    }
    const path = roadFrom(g, from ? from.road : road, to, danger);
    if (path.length < 2) continue;
    const lorries = Math.max(1, Math.round(divs * C.lorries.perDiv * (path.length - 1)));
    const run: ColumnRun = { path, divs, lorries, lost: 0, share: 1, hits: [] };
    if (from) run.dump = from.dump.region;
    runs.push(run);
  }
  // The columns stocking a dump ordered last month, from the railhead: paid for when it was ordered, outside the pool.
  for (const d of filling) {
    const path = roadFrom(g, road, d.region, danger);
    if (path.length < 2) continue;
    runs.push({ path, divs: d.fill, lorries: Math.max(1, Math.round(d.fill * C.lorries.perDiv * (path.length - 1))), lost: 0, share: 1, hits: [], fill: true });
  }
  // The nation's lorries shared out among its columns (logistics.ts): the front it names first. Where the switches
  // are off every column has all the lorries it needs.
  shareOutLorries(g, nat, runs);
  let sent = 0, lostAll = 0, horseLost = 0, byAir = 0, byBands = 0, mineLost = 0;
  let worst = { p: -1, n: 0 };
  for (const run of runs) {
    const path = run.path, to = path[path.length - 1];
    const lorries = run.lorries;
    // Lorries ('m') and wagons ('h') still on the road, and those held up by the ground.
    let m = lorries, h = run.horse ?? 0, mWorn = 0, hWorn = 0;
    for (let k = 1; k < path.length; k++) {
      const p = path[k], s = stretch(p), wear = roadWear(g, p);
      if (s.air <= 0 && s.partisans <= 0 && wear.lorry <= 0 && wear.horse <= 0) continue;
      if (s.air > 0 || s.partisans > 0) {
        const rng = new Rng(hashString(`${run.fill ? 'dump' : 'col'}:${g.seed}:${g.turn}:${nat}:${to}:${p}${run.dump !== undefined ? `:${run.dump}` : ''}`));
        const roll = (): number => C.roll[0] + (C.roll[1] - C.roll[0]) * rng.next();
        const air = Math.min(0.95, C.air.most * s.air * (1 - s.flak) * roll());
        const bands = Math.min(0.95, C.partisans.most * s.partisans * roll());
        const mAir = m * air, mBands = (m - mAir) * bands;
        const hAir = h * air, hBands = (h - hAir) * bands;
        const fromAir = mAir + hAir, fromBands = mBands + hBands;
        if (air >= C.hitAt) run.hits.push([p, 'air', Math.round(fromAir)]);
        if (bands >= C.hitAt) run.hits.push([p, 'partisans', Math.round(fromBands)]);
        byAir += fromAir; byBands += fromBands; mineLost += fromAir * s.mine;
        if (fromAir + fromBands > worst.n) worst = { p, n: fromAir + fromBands };
        m -= mAir + mBands;
        h -= hAir + hBands;
      }
      // The ground and the pace (the wagons on): what is held up does not reach the front this month.
      if (wear.lorry > 0) { const w = m * Math.min(0.95, wear.lorry); mWorn += w; m -= w; }
      if (wear.horse > 0 && h > 0) { const w = h * Math.min(0.95, wear.horse); hWorn += w; h -= w; }
    }
    const lostM = lorries - m - mWorn, lostH = (run.horse ?? 0) - h - hWorn;
    run.lost = Math.round(lostM + lostH);
    if (mWorn + hWorn > 0) run.worn = Math.round(mWorn + hWorn);
    const need = run.need ?? lorries;
    run.share = Math.max(0, Math.min(1, (m + h) / need));
    // A dump's column puts what got through into the dump.
    if (run.fill) {
      const d = g.dumps?.find((x) => x.nat === nat && x.region === to);
      if (d) { d.stock += d.fill * run.share; d.fill = 0; }
    }
    sent += lorries + (run.horse ?? 0); lostAll += lostM; horseLost += lostH;
  }
  // Used-up dumps are taken off the map, and a computer nation stocks a new one where it would save most road.
  clearDumps(g, nat);
  aiDumps(g, nat, runs);
  if (!runs.length) return;
  book[nat] = runs;
  // The player hears of it when his own columns bleed, and when his aircraft burn the enemy's.
  const me = g.human;
  if (!me) return;
  const lostSent = lostAll + horseLost;
  if (nat === me && sent > 0 && lostSent / sent >= C.newsAt) {
    const who = byAir >= 2 * byBands ? 'Enemy aircraft caught' : byBands >= 2 * byAir ? 'Partisans ambushed' : 'Enemy aircraft and partisans caught';
    const got = Math.round((1 - lostSent / sent) * 100);
    const what = horseLost > 0 ? 'lorries and wagons' : 'lorries';
    headline(g, 'info', `${who} our supply columns on the roads to the front: ${word(lostSent)} of ${word(sent)} ${what} lost${worst.p >= 0 ? `, the worst of it near ${g.regions[worst.p].name}` : ''}. The corps beyond the railhead got ${got}% of what was sent, and take in fewer drafts and less rest for it.`, me, worst.p >= 0 ? worst.p : undefined, { arm: 'land', lost: me });
  } else if (atWar(g, me, nat) && mineLost >= 20) {
    headline(g, 'info', `Our aircraft caught ${g.nations[nat].adj} ${wagonsOn(g) ? 'supply' : 'lorry'} columns on the roads behind their front: about ${word(mineLost)} ${wagonsOn(g) ? 'lorries and wagons' : 'lorries'} burned${worst.p >= 0 ? `, the most near ${g.regions[worst.p].name}` : ''}. What they carried never reached the front.`, me, worst.p >= 0 ? worst.p : undefined, { arm: 'air' });
  }
}

/**
 * What a corps' column brought it this month, as a share of its drafts and its rest: 1 where the columns are off or
 * none was sent (on the railhead itself the trains bring everything), down to the floor where nothing got through:
 * an army carries some stores with it and lives partly off the country.
 */
export function columnShare(g: Game, c: Corps): number {
  if (!columnsOn(g)) return 1;
  const run = g.columns?.[c.nat]?.find((r) => !r.fill && r.path[r.path.length - 1] === c.reg);
  return run ? COLUMNS.floor + (1 - COLUMNS.floor) * run.share : 1;
}

/** One column as a map draws it. */
export interface ColumnView {
  nat: string;
  /** The road, province by province (the railhead first), and its points on the map. */
  path: number[];
  points: [number, number][];
  lorries: number;
  lost: number;
  share: number;
  /** Whose it is, from where the viewer stands. */
  side: 'ours' | 'friend' | 'theirs';
  /** Where it was caught: the province, a point on the road into it (most of the way from the last province), by whom, lorries lost. */
  hits: { at: number; x: number; y: number; by: Ambusher; lost: number }[];
  /** (logistics.ts, each absent where its switch is off) Loads carried by horse-drawn wagon; a column stocking a dump; one setting out from a dump. */
  horse?: number;
  fill?: boolean;
  fromDump?: boolean;
}

/**
 * The columns a viewer can see this month: his own and his allies' whole, an enemy's only along the stretch of it in
 * sight. Nothing where the columns are off.
 */
export function columnsView(g: Game, viewer: string | null = g.human): ColumnView[] {
  if (!columnsOn(g) || !g.columns) return [];
  const sight = viewer ? sightOf(g, viewer) : null;
  const out: ColumnView[] = [];
  for (const [nat, runs] of Object.entries(g.columns)) {
    const friend = !viewer || friendOf(g, nat, viewer);
    for (const run of runs) {
      let path = run.path;
      if (!friend && sight) {
        const first = path.findIndex((p) => sight[p] >= SIGHT.glimpse);
        if (first < 0) continue;
        let last = first;
        while (last + 1 < path.length && sight[path[last + 1]] >= SIGHT.glimpse) last++;
        path = path.slice(Math.max(0, first - 1), last + 1);
        if (path.length < 2) continue;
      }
      const on = new Set(path);
      out.push({
        nat, path, points: path.map((p) => [g.regions[p].x, g.regions[p].y]), lorries: run.lorries, lost: run.lost, share: run.share,
        side: !viewer ? 'friend' : nat === viewer ? 'ours' : friend ? 'friend' : 'theirs',
        hits: run.hits.filter(([p]) => on.has(p)).map(([p, by, lost]) => {
          // On the road, not on the province's middle (where its armies stand): 60% of the way in from the province before.
          const i = run.path.indexOf(p), a = g.regions[run.path[Math.max(0, i - 1)]], b = g.regions[p];
          const t = i > 0 ? 0.6 : 1;
          return { at: p, x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t, by, lost };
        }),
        ...(run.horse ? { horse: Math.round(run.horse) } : {}),
        ...(run.fill ? { fill: true } : {}),
        ...(run.dump !== undefined ? { fromDump: true } : {}),
      });
    }
  }
  return out;
}
