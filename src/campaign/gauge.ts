// Railway gauge (release 2, batch 3).
//
// Brian, 27 November 2026 17:10Z, "Yes to all" to the idea; the design director's rulings of 17:15Z and 17:48Z on
// r2-three-ideas/FORM-rails-rivers-1127.md, part A, and of 28 November 01:12Z. Behind the grand campaign's flag
// 'gauge', OFF until its gate.
//
// 1. Each province's track has a gauge, a fact of the ground: at the war's start, the table by province
//    (data/campaign/gauge.json 'track', read at the start month by the one start-key reader, citydefences.ts
//    startRowsAt; no row is standard), and after that whatever the last railway troops to re-lay it laid (flag
//    'ga:<id>'). The owner of a province never changes its track: ground handed over by a treaty, a claim or a
//    liberation keeps the track it had. Which track a nation's trains run on is its home gauge, by the nation
//    (gauge.json 'home'; every nation not listed is standard). Never a date.
// 2. A nation's trains run only on track of its own gauge. One reading, in rails.ts: the working net a nation's
//    railheads are counted over (railheads), its trains' walk (game.ts railWalk, whyNoTrain, nearestBreak), and the
//    long line in the east (game.ts reachInto). So supply, rail moves, the railhead's worth and every staff read it.
// 3. Each month, before its supply is drawn (game.ts startPhase), a nation's railway troops re-lay up to 'relay'
//    provinces of captured track to its own gauge: provinces it holds, whose line is not cut, joined by a working line
//    to its working net; main lines first (more lines through the province), then the nearer to its depots by rail,
//    then the lower id. No draw. A capacity, the same for every nation.
// 4. The eastern long line's gauge half goes (the DD, 17:48Z, (a)): no months to convert, and the line is counted from
//    the invader's own railheads, by the track's own gauge. Its roads half stays: every step on that ground costs
//    'stepConverted', times the weather.
// With the switch off nothing here is written, and every answer is the answer without it.

import data from '../../data/campaign/gauge.json';
import { startRowsAt } from './citydefences';
import { headline } from './game';
import { groundRules } from './ground';
import type { Game } from './types';

/** A row of the track's table: a province by name, the start months it reads at, its gauge, and why (gauge.json 'track'). */
export interface TrackRow { region: string; turns: number[]; gauge: string; lay: boolean; why: string }
export const GAUGE = data as unknown as { names: Record<string, string>; home: Record<string, string>; relay: number; track: TrackRow[] };

/** The switch: a grand campaign's own, off unless the release 2 ship list or the player turns it on. */
export const gaugeOn = (g: Game): boolean => g.flags.open === 1 && g.flags.gauge === 1 && groundRules(g);

// Gauges by number in the flags: 0 standard, then the others in the order of 'names'.
const CODES = Object.keys(GAUGE.names);
const codeOf = (name: string | undefined): number => Math.max(0, CODES.indexOf(name ?? 'standard'));
const HOME = new Map<string, number>(Object.entries(GAUGE.home).map(([n, k]) => [n, codeOf(k)]));
/** A nation's own gauge, by number. */
export const homeGauge = (nat: string): number => HOME.get(nat) ?? 0;

const KEYS: string[] = [];
const key = (r: number): string => (KEYS[r] ??= `ga:${r}`);
// The track at the war's start, by province: the table's rows at the start month (no row: standard). Worked out once a
// game and start month: it reads the table and the provinces' names, which do not change.
const STARTS = new WeakMap<Game, { from: number; rows: TrackRow[]; at: Int8Array }>();
/** The gauge of every province's track at the war's start, by number. `rows` is for tests. */
export function startTrack(g: Game, rows: TrackRow[] = GAUGE.track): Int8Array {
  const from = g.flags.openFrom ?? 1;
  const kept = STARTS.get(g);
  if (kept && kept.from === from && kept.rows === rows && kept.at.length === g.regions.length) return kept.at;
  const at = new Int8Array(g.regions.length);
  const byName = new Map(g.regions.map((r) => [r.name, r.id]));
  for (const row of startRowsAt(rows, from)) {
    const id = byName.get(row.region);
    if (id !== undefined) at[id] = codeOf(row.gauge);
  }
  STARTS.set(g, { from, rows, at });
  return at;
}
/** The gauge of a province's track, by number: what its last railway troops laid, or the table's at the war's start. */
export const trackGauge = (g: Game, r: number): number => g.flags[key(r)] ?? startTrack(g)[r];
/** The gauge's name, for the words: "broad", "standard", "Iberian". */
export const gaugeName = (code: number): string => GAUGE.names[CODES[code] ?? 'standard'];

/** Whether a nation's trains run on a province's track: always with the switch off. */
export const fitsGauge = (g: Game, nat: string, r: number): boolean => !gaugeOn(g) || trackGauge(g, r) === homeGauge(nat);

/** Turns the switch on or off at any time. On again while on, nothing changes. Off forgets every track re-laid. The supply picture is drawn afresh. */
export function setGauge(g: Game, on: boolean): boolean {
  if (on) {
    if (g.flags.open !== 1) return false;
    if (g.flags.gauge === 1) return true;
    g.flags.gauge = 1;
  } else {
    delete g.flags.gauge;
    for (const k of Object.keys(g.flags)) if (k.startsWith('ga:')) delete g.flags[k];
  }
  g.supplyDirty = true;
  return true;
}

/** Re-lays a province's track to a nation's gauge; the gauge it had at the war's start again is no flag at all. */
export function relay(g: Game, r: number, nat: string): void {
  const k = homeGauge(nat);
  if (k === startTrack(g)[r]) delete g.flags[key(r)];
  else g.flags[key(r)] = k;
  g.supplyDirty = true;
}

/**
 * The provinces a nation's railway troops re-lay this month, in order (point 3), from its working net as the supply
 * picture now stands (`net`: 0 on it, rails.ts railheads) and its depots (`depot`: the stores, supply value 2). Pure:
 * the caller writes them. `links` gives a province's lines; `whole(a, b)` whether the line between two is working
 * (neither cut, the bridges standing).
 */
export function relayChoice(g: Game, nat: string, net: Int16Array, depot: (r: number) => boolean, links: (r: number) => number[], whole: (a: number, b: number) => boolean, cut: (r: number) => boolean): number[] {
  if (!gaugeOn(g)) return [];
  const own = homeGauge(nat);
  // How far each province of the working net is from its depots by rail.
  const far = new Int32Array(g.regions.length).fill(-1);
  const q: number[] = [];
  for (let i = 0; i < net.length; i++) if (net[i] === 0 && depot(i)) { far[i] = 0; q.push(i); }
  for (let i = 0; i < q.length; i++) for (const j of links(q[i])) if (far[j] < 0 && net[j] === 0 && whole(q[i], j)) { far[j] = far[q[i]] + 1; q.push(j); }
  const cands: { r: number; lines: number; near: number }[] = [];
  for (const r of g.regions) {
    if (r.ctrl !== nat || cut(r.id) || trackGauge(g, r.id) === own) continue;
    let near = Infinity;
    for (const j of links(r.id)) if (net[j] === 0 && far[j] >= 0 && whole(r.id, j)) near = Math.min(near, far[j] + 1);
    if (near === Infinity) continue;
    cands.push({ r: r.id, lines: links(r.id).length, near });
  }
  cands.sort((a, b) => b.lines - a.lines || a.near - b.near || a.r - b.r);
  return cands.slice(0, Math.max(0, Math.floor(GAUGE.relay))).map((c) => c.r);
}

/** Tells the player what his railway troops re-laid this month. */
export function tellRelay(g: Game, nat: string, done: number[]): void {
  if (!done.length || nat !== g.human) return;
  const names = done.map((r) => g.regions[r].name);
  const list = names.length === 1 ? names[0] : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
  headline(g, 'info', `Your railway troops re-lay the track at ${list} to your own ${gaugeName(homeGauge(nat))} gauge. Your trains run there from now on.`, nat, done[0], { arm: 'land' });
}
