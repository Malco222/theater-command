/**
 * Veteran crews (release 2; Brian's pick of 21 November 2026). The crews of a fleet and the pilots and aircrews of
 * an air group learn from every fight they come through, wear a rank for it (the same five a corps wears: Green,
 * Trained, Seasoned, Veteran, Elite) and fight a little better for it. Ships and aircraft that join afterwards bring
 * men fresh from the schools, who thin what the old hands know. A corps keeps its own experience already (stories.ts);
 * with this switch on, every nation's drafts thin it, not only the player's.
 *
 * One switch, g.flags.crews (setCrews), off in the rules: with it off nothing here is read or written and every
 * multiplier is exactly 1, so history's wars, the exit tests and every hash are as they were. The interfaces turn it
 * on when a grand campaign is started. It applies to every nation alike (Brian's standing rule of 21 November: a
 * battle mechanic is for all nations), and draws on no dice.
 *
 * Numbers are in data/campaign/crews.json, whose `_about` block explains each one.
 */
import data from '../../data/campaign/crews.json';
import { headline } from './game';
import type { AirFleet, Crew, Fleet, Game, ShipClass } from './types';

type Row = [turn: number, sea: number, air: number];
interface CrewData {
  pivot: number;
  edge: { sea: number; air: number; aim: number; close: number };
  gain: { sea: { won: number; lost: number; month: number }; air: { won: number; lost: number }; close: number };
  most: number;
  dilute: number;
  news: number;
  weight: Record<ShipClass | 'bomber', number>;
  start: Record<string, Row[]>;
  train: Record<string, Row[]>;
}
export const CREWS = data as unknown as CrewData;

/** The five ranks, from what the crews know: the same steps a corps' grades take (stories.json 'grades'). */
export const RANKS: [from: number, name: string][] = [[0, 'Green'], [0.2, 'Trained'], [0.4, 'Seasoned'], [0.6, 'Veteran'], [0.8, 'Elite']];

export type Arm = 'sea' | 'air';

/** True with veteran crews switched on. */
export const crewRules = (g: Game): boolean => g.flags.crews === 1;

/** Turns veteran crews on or off (a new grand campaign, or the test kitchen). Off leaves no trace in the game. */
export function setCrews(g: Game, on: boolean): void {
  if (on) g.flags.crews = 1;
  else delete g.flags.crews;
}

/** The rank's step, 0 (Green) to 4 (Elite). Trained wears one chevron, Seasoned two, Veteran three, Elite three and a star. */
export function rankIndex(xp: number): number {
  let k = 0;
  RANKS.forEach(([from], i) => { if (xp >= from - 1e-9) k = i; });
  return k;
}
export const rankName = (xp: number): string => RANKS[rankIndex(xp)][1];

const rowOf = (table: Record<string, Row[]>, nat: string, turn: number): Row => {
  const rows = table[nat] ?? table._default;
  let out = rows[0];
  for (const r of rows) if (r[0] <= turn) out = r;
  return out;
};
const pick = (r: Row, arm: Arm): number => (arm === 'sea' ? r[1] : r[2]);

/** What a nation's crews know when the switch first sees them, by the month. */
export const startXp = (g: Game, nat: string, arm: Arm): number => pick(rowOf(CREWS.start, nat, g.turn), arm);
/** How well trained the men are who fill a nation's gaps and man its new ships and aircraft this month. */
export const trainXp = (g: Game, nat: string, arm: Arm): number => pick(rowOf(CREWS.train, nat, g.turn), arm);

/** The size of a fleet in crew weight, or of an air group in aircrews. */
export function crewSize(u: Fleet | AirFleet, arm: Arm): number {
  if (arm === 'sea') {
    const f = u as Fleet;
    let n = 0;
    for (const c of Object.keys(f.ships) as ShipClass[]) n += (f.ships[c] ?? 0) * (CREWS.weight[c] ?? 0);
    return n;
  }
  const a = u as AirFleet;
  return Math.max(0, a.ftr) + Math.max(0, a.bmr) * CREWS.weight.bomber;
}

const round4 = (x: number): number => Math.round(x * 10000) / 10000;

/**
 * Brings a unit's crews up to date and hands them back: the first time the switch sees it they know their nation's
 * starting level; ships or aircraft that have joined since it was last counted bring men trained to this month's
 * level, who count for 'dilute' of their numbers. Losses take men away and leave what the rest know as it was.
 */
export function crewOf(g: Game, u: Fleet | AirFleet, arm: Arm): Crew {
  const now = crewSize(u, arm);
  if (!u.crew) {
    u.crew = { xp: startXp(g, u.nat, arm), n: round4(now), fought: 0, won: 0 };
    return u.crew;
  }
  const c = u.crew;
  if (now > c.n + 1e-6) {
    const add = (now - c.n) * CREWS.dilute;
    // A unit that was all but gone and is filled again is new men: it starts where the schools leave them.
    c.xp = round4(c.n > 0 ? (c.xp * c.n + trainXp(g, u.nat, arm) * add) / (c.n + add) : trainXp(g, u.nat, arm));
  }
  c.n = round4(now);
  return c;
}

/** What the crews know, without counting anything: their record if they have one, else their nation's starting level. */
export const crewXp = (g: Game, u: Fleet | AirFleet, arm: Arm): number => u.crew?.xp ?? startXp(g, u.nat, arm);

/**
 * How much better or worse than its nation's standard a unit fights for what its crews know: exactly 1 with the
 * switch off. `which` picks the edge: the fleet's whole fighting ('sea'), an air group's fighters ('air'), its
 * bombers' aim ('aim'), or its fighters over a land battle on call ('close').
 */
export function crewMul(g: Game, u: Fleet | AirFleet, which: 'sea' | 'air' | 'aim' | 'close'): number {
  if (!crewRules(g)) return 1;
  const xp = crewXp(g, u, which === 'sea' ? 'sea' : 'air');
  return 1 + CREWS.edge[which] * (xp - CREWS.pivot);
}

/** The bombers' aim of the groups on a raid together: each group's edge, weighed by its bombers. Exactly 1 with the switch off. */
export function crewAim(g: Game, groups: AirFleet[]): number {
  if (!crewRules(g)) return 1;
  let w = 0, s = 0;
  for (const f of groups) { const b = Math.max(0, f.bmr); w += b; s += b * crewMul(g, f, 'aim'); }
  return w > 0 ? s / w : 1;
}

/**
 * A unit has come through a fight: its men learn from it, more from a fight won. `lesson`, where given, is what the
 * fight teaches in place of a won or lost fight's (a group that only flew over a land battle on call), and is not
 * counted as a fight of its own. A unit of the player's that reaches Veteran or Elite is named in the home news.
 */
export function crewFought(g: Game, u: Fleet | AirFleet, arm: Arm, won: boolean, lesson?: number): void {
  if (!crewRules(g)) return;
  const c = crewOf(g, u, arm);
  const before = rankIndex(c.xp);
  const G = arm === 'sea' ? CREWS.gain.sea : CREWS.gain.air;
  const step = (lesson ?? (won ? G.won : G.lost)) * (1 - c.xp);
  c.xp = round4(Math.min(CREWS.most, c.xp + Math.max(0, step)));
  if (lesson === undefined) { c.fought++; if (won) c.won++; }
  const rank = rankIndex(c.xp);
  if (rank > before && rank >= CREWS.news && u.nat === g.human) {
    const who = arm === 'sea' ? `The crews of ${u.name}` : `The aircrews of ${u.name}`;
    headline(g, 'info', `${who} are ${RANKS[rank][1].toLowerCase()} now: ${c.fought} ${arm === 'sea' ? 'actions' : 'fights'} behind them. They wear ${rank === 4 ? 'the elite star over three chevrons' : 'three chevrons'}.`, u.nat);
  }
}

/** The month's count of every fleet and air group of a nation: the ships and aircraft that joined since it was last counted. */
export function crewsMonth(g: Game, nat: string): void {
  if (!crewRules(g)) return;
  const war = (g.nations[nat]?.atWar.length ?? 0) > 0;
  for (const f of g.fleets) {
    if (f.nat !== nat) continue;
    const c = crewOf(g, f, 'sea');
    // A month at sea in a war is a lesson of its own: watches kept, drills run, the enemy looked for.
    if (war && f.stance !== 'port' && c.n > 0) crewFought(g, f, 'sea', false, CREWS.gain.sea.month);
  }
  for (const f of g.airFleets) if (f.nat === nat) crewOf(g, f, 'air');
}

/** Two fleets become one (fleets.ts merges): the crews of both, weighed by their size. Call before the ships move. */
export function crewsJoin(g: Game, from: Fleet | AirFleet, to: Fleet | AirFleet, arm: Arm): void {
  if (!crewRules(g)) return;
  const a = crewOf(g, from, arm), b = crewOf(g, to, arm);
  const n = a.n + b.n;
  if (n > 0) b.xp = round4((a.xp * a.n + b.xp * b.n) / n);
  b.n = round4(n);
  b.fought = Math.max(a.fought, b.fought);
  b.won = Math.max(a.won, b.won);
}

/** What the interfaces show of a unit's crews: null with the switch off. */
export interface CrewView {
  /** What they know, in percent; the rank, and its step: 0 Green (bare), 1 Trained (one chevron), 2 Seasoned (two), 3 Veteran (three), 4 Elite (three and a star). */
  xp: number;
  rank: string;
  step: number;
  /** How much better or worse than their nation's standard they fight, in percent (+6, -3). */
  edge: number;
  /** The fights they have come through, and won. */
  fought: number;
  won: number;
  /** The rank above, and how far there is to go to it in percent; null at Elite. */
  next: string | null;
  toNext: number;
  /** The level new men join at this month, in percent, and its rank. */
  trained: number;
  trainedRank: string;
  /** One line for a card: "Veteran crews · 7 actions, 5 won". */
  line: string;
}

export function crewView(g: Game, u: Fleet | AirFleet, arm: Arm): CrewView | null {
  if (!crewRules(g)) return null;
  const xp = crewXp(g, u, arm);
  const k = rankIndex(xp);
  const fought = u.crew?.fought ?? 0, won = u.crew?.won ?? 0;
  const next = k < RANKS.length - 1 ? RANKS[k + 1] : null;
  const t = trainXp(g, u.nat, arm);
  const what = arm === 'sea' ? 'crews' : 'aircrews';
  const deeds = fought ? ` · ${fought} ${arm === 'sea' ? (fought === 1 ? 'action' : 'actions') : (fought === 1 ? 'fight' : 'fights')}, ${won} won` : ' · not yet in action';
  return {
    xp: Math.round(xp * 100), rank: RANKS[k][1], step: k,
    edge: Math.round((crewMul(g, u, arm === 'sea' ? 'sea' : 'air') - 1) * 100),
    fought, won, next: next ? next[1] : null, toNext: next ? Math.max(0, Math.round((next[0] - xp) * 100)) : 0,
    trained: Math.round(t * 100), trainedRank: rankName(t),
    line: `${RANKS[k][1]} ${what}${deeds}`,
  };
}

/** A line for a battle's ticket: each side's crews by rank ("Veteran against Green"), or null with the switch off. */
export function crewsLine(g: Game, a: (Fleet | AirFleet)[], b: (Fleet | AirFleet)[], arm: Arm): { label: string; value: string; effect: string } | null {
  if (!crewRules(g) || !a.length || !b.length) return null;
  const avg = (us: (Fleet | AirFleet)[]): number => {
    let w = 0, s = 0;
    for (const u of us) { const n = Math.max(1, crewSize(u, arm)); w += n; s += n * crewXp(g, u, arm); }
    return w ? s / w : CREWS.pivot;
  };
  const x = avg(a), y = avg(b);
  const pct = (v: number): string => `${v >= 0 ? '+' : ''}${Math.round(CREWS.edge[arm] * (v - CREWS.pivot) * 100)}%`;
  return {
    label: arm === 'sea' ? 'Crews' : 'Aircrews',
    value: `${rankName(x)} against ${rankName(y)}`,
    effect: `Attacker ${pct(x)}, defender ${pct(y)} for what their men have learned`,
  };
}
