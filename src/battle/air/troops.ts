// The army under a raid on troops fought out as an air battle (release 2).
//
// Brian, 24 November 2026: "i want every air battle playable"; "if a bomber group is bombing troops instead of a factory
// or town and they get intercepted than i want that playable". The design director, 22:28Z: the fought raid books through
// the staff's own law for a raid on troops (the organisation cut, from the bombers that drop and the fighters that
// strafe); no choice is offered that the rules cannot book, so there is no picking of columns, guns or armour, for the
// campaign keeps no stock of them for a corps.
//
// Two things live here. COUNTED: nothing but the law (airfought.ts TroopLaw), which is fly()'s own. SHOWN: the part of
// the army under the aiming point as pieces on the board (place.marks, the 3D thread's contract,
// /mnt/project-files/air-battles/every-kind/CONTRACT.md), and a book that says which piece each bomb pattern and each
// strafing pass is seen to hit. The wreckage it shows never passes the men the law books lost.

import data from '../../../data/battle/air-troops.json';
import { troopLaw, type AirAssess } from '../../campaign/air';
import { troopCutOf, troopWorth, type TroopLaw } from '../../campaign/airfought';
import { atWar, men } from '../../campaign/game';
import type { AirMission, Corps, Game } from '../../campaign/types';
import { Rng } from '../../core/rng';
import { vehicleOf } from '../land/armour';
import type { Kind } from '../land/rules';
import { SKY } from './book';

type Pt = [number, number];

/** What a piece is: the 3D game draws each from its own kit. The last six are fieldworks and a town's defences (the line and the town, to come). */
export type MarkWhat = 'men' | 'wagon' | 'lorry' | 'halftrack' | 'car' | 'gun' | 'aa' | 'tank' | 'trench' | 'bunker' | 'pit' | 'wire' | 'block' | 'post';
/** How the troops stand: on the road in column, in bivouac off it, in fieldworks; 'works' and 'town' are the line's and the town's. */
export type MarkPosture = 'march' | 'camp' | 'dug' | 'works' | 'town';

export interface MarkPiece {
  id: number;
  /** The campaign corps it belongs to. */
  corps: number;
  what: MarkWhat;
  /** The battle kit's model where the rules know one (a tank: land/armour.ts vehicleOf); '' for the nation's plain one. */
  model: string;
  nat: string;
  /** Board metres, in the frame of place.ground; face in radians clockwise from north. */
  x: number; y: number; face: number;
  /** The men, vehicles, guns or teams it stands for. */
  n: number;
  posture: MarkPosture;
}

/**
 * A corps under the raid, as the ticket carries it. (No strength or organisation: the ticket goes to the raider's
 * screen, and his side knows an enemy corps's only as far as its sight and reports go, armies.ts corpsSeen.)
 */
export interface MarkCorps { id: number; nat: string; name: string; kind: string; divs: number; men: number; posture: MarkPosture; works: number }

export interface MarkPlace {
  /** Whose they mostly are. */
  nat: string;
  posture: MarkPosture;
  /** A column's road through the aim; null where nothing is on the march. */
  road: Pt[] | null;
  /** What stands in the province (for words), and the men one drawn figure stands for in the slice under the raid. */
  divs: number; men: number; per: number;
  pieces: MarkPiece[];
  corps: MarkCorps[];
  /** The campaign's own law for the raid: the battle books by it; the 3D game does not read it. */
  law: TroopLaw;
}

interface TroopData {
  slice: { share: number; least: number; most: number };
  pieces: number; figures: number;
  unit: Record<string, number>; worth: Record<string, number>;
  mix: Record<string, Record<string, number>>;
  lorried: Record<string, number>; lorriedElse: number; motor: string[];
  strafe: { passes: number; half: number; width: number };
}
export const TROOPS = data as unknown as TroopData;

const bearing = (dx: number, dy: number): number => Math.atan2(dx, dy);

/** How a corps stands under a raid: dug in; on the road, if it has marched or fought this month; else in bivouac. */
function postureOf(c: Corps): MarkPosture {
  if (c.dug) return 'dug';
  return c.ap < c.apMax || c.fought ? 'march' : 'camp';
}

/** The pieces a corps's share of the slice is drawn as: [what, how many it stands for] each. */
function sample(c: Corps, slice: number, grow: number): [MarkWhat, number][] {
  const mix = TROOPS.mix[c.kind] ?? TROOPS.mix.inf;
  const lorried = TROOPS.motor.includes(c.kind) ? 1 : TROOPS.lorried[c.nat] ?? TROOPS.lorriedElse;
  const shares: [MarkWhat, number][] = [];
  for (const [k, v] of Object.entries(mix)) {
    if (k === 'haul') { shares.push(['lorry', v * lorried], ['wagon', v * (1 - lorried)]); continue; }
    shares.push([k as MarkWhat, v]);
  }
  const out: [MarkWhat, number][] = [];
  for (const [what, share] of shares) {
    const units = (slice * share) / TROOPS.worth[what];
    if (units < 0.5) continue;
    const size = TROOPS.unit[what] * grow, k = Math.max(1, Math.round(units / size)), n = Math.max(1, Math.round(units / k));
    for (let i = 0; i < k; i++) out.push([what, n]);
  }
  return out;
}

/**
 * The army under a raid on troops, for the ticket: null for any other raid, or where no enemy stands there. Laid on a
 * stream of its own from the battle's seed (fork 67), which no count reads: the same battle lays the same column. A
 * tank piece takes the battle kit's model for its nation that month.
 */
export function troopPlace(g: Game, a: AirAssess, m: AirMission, target: number, nat: string, seed: number): MarkPlace | null {
  const law = troopLaw(g, a, m, target);
  if (!law) return null;
  const r = g.regions[target];
  // (To do with the fog ruling of 22:10Z: the corps as the raider's side last saw them. A raid on troops is flown only at troops in sight.)
  const foes = r.units.filter((c) => !c.dead && atWar(g, nat, c.nat));
  const all = foes.reduce((s, c) => s + men(c), 0);
  if (!foes.length || all <= 0) return null;
  const rng = new Rng(seed).fork(67);
  const slice = Math.min(all, Math.max(TROOPS.slice.least, Math.min(TROOPS.slice.most, all * TROOPS.slice.share)));
  // Larger pieces rather than more of them, where the sample would pass the most a ticket carries.
  let grow = 1, drawn: { c: Corps; what: MarkWhat; n: number }[] = [];
  for (let k = 0; k < 8; k++) {
    drawn = foes.flatMap((c) => sample(c, slice * (men(c) / all), grow).map(([what, n]) => ({ c, what, n })));
    if (drawn.length <= TROOPS.pieces) break;
    grow *= Math.max(1.15, drawn.length / TROOPS.pieces);
  }
  drawn = drawn.slice(0, TROOPS.pieces);
  const T = SKY.target, R = T.r * 0.92;
  const lead = [...foes].sort((x, y) => men(y) - men(x) || x.id - y.id)[0];
  const posture = postureOf(lead);
  // The road: through the aim on a heading of its own, with one easy bend.
  const hd = rng.range(0, Math.PI), bend = rng.range(-0.08, 0.08) * R;
  const dx = Math.sin(hd), dy = Math.cos(hd);
  const onRoad = (s: number): Pt => [T.x + dx * s + dy * bend * Math.sin((Math.PI * s) / R), T.y + dy * s - dx * bend * Math.sin((Math.PI * s) / R)];
  const marching = foes.some((c) => postureOf(c) === 'march');
  const road: Pt[] | null = marching ? Array.from({ length: 13 }, (_, i) => onRoad(-R + (i * R) / 6)) : null;
  const pieces: MarkPiece[] = [];
  const turn = g.turn;
  const put = (c: Corps, what: MarkWhat, n: number, x: number, y: number, face: number): void => {
    // Never outside the release ring: a piece laid past it is brought in to its rim.
    const d = Math.hypot(x - T.x, y - T.y), k = d > R ? R / d : 1;
    pieces.push({
      id: pieces.length + 1, corps: c.id, what, model: what === 'tank' ? vehicleOf('arm' satisfies Kind, c.nat, turn) ?? '' : '', nat: c.nat,
      x: Math.round(T.x + (x - T.x) * k), y: Math.round(T.y + (y - T.y) * k), face: Math.round(face * 1000) / 1000, n, posture: postureOf(c),
    });
  };
  // The column: every marching corps's pieces nose to tail along the road, the gaps closed up until it fits the ring.
  const column = drawn.filter((p) => postureOf(p.c) === 'march');
  const span = (p: { what: MarkWhat; n: number }): number => (p.what === 'men' ? 60 + p.n * 0.8 : 40 + p.n * 28);
  const long = column.reduce((s, p) => s + span(p) + 60, 0), fit = Math.min(1, (2 * R * 0.94) / Math.max(1, long));
  let at = -Math.min(R * 0.94, long / 2);
  for (const p of column) {
    const len = (span(p) + 60) * fit, mid = at + len / 2;
    const [x, y] = onRoad(mid), [ax, ay] = onRoad(mid + 30);
    put(p.c, p.what, p.n, x, y, bearing(ax - x, ay - y));
    at += len;
  }
  // In bivouac: each corps round a centre of its own off the road, its vehicles parked in rows, its men in their lines.
  // Dug in: the men along a line of works through the aim, the guns behind it, the transport further back.
  const face0 = rng.range(0, 2 * Math.PI);
  foes.forEach((c, ci) => {
    const how = postureOf(c);
    if (how === 'march') return;
    const mine = drawn.filter((p) => p.c === c);
    if (how === 'camp') {
      const cx = T.x + rng.range(-0.45, 0.45) * R, cy = T.y + rng.range(-0.45, 0.45) * R;
      mine.forEach((p, i) => {
        const ring = 250 + 260 * Math.floor(i / 6), th = (i % 6) * (Math.PI / 3) + ci + rng.range(-0.25, 0.25);
        put(c, p.what, p.n, cx + Math.sin(th) * ring, cy + Math.cos(th) * ring, rng.range(0, 2 * Math.PI));
      });
      return;
    }
    // (A line faces its enemy on the ground, whom the air battle does not know: one heading for the whole position.)
    const fx = Math.sin(face0), fy = Math.cos(face0), lx = fy, ly = -fx;
    const front = mine.filter((p) => p.what === 'men'), guns = mine.filter((p) => p.what === 'gun' || p.what === 'aa'), rear = mine.filter((p) => p.what !== 'men' && p.what !== 'gun' && p.what !== 'aa');
    const along = (k: number, n: number, wide: number): number => (n > 1 ? (k / (n - 1) - 0.5) * wide : 0);
    const off = (ci - (foes.length - 1) / 2) * 500;
    front.forEach((p, i) => { const s = along(i, front.length, 1.5 * R); put(c, p.what, p.n, T.x + lx * s + fx * (off + (i % 2 ? 90 : -90)), T.y + ly * s + fy * (off + (i % 2 ? 90 : -90)), face0); });
    guns.forEach((p, i) => { const s = along(i, guns.length, 1.1 * R); put(c, p.what, p.n, T.x + lx * s - fx * (700 - off), T.y + ly * s - fy * (700 - off), face0); });
    rear.forEach((p, i) => { const s = along(i, rear.length, 0.9 * R); put(c, p.what, p.n, T.x + lx * s - fx * (1_500 - off), T.y + ly * s - fy * (1_500 - off), face0 + Math.PI / 2); });
  });
  const shownMen = pieces.reduce((s, p) => s + (p.what === 'men' ? p.n : 0), 0);
  return {
    nat: lead.nat, posture, road,
    divs: Math.round(foes.reduce((s, c) => s + c.divs, 0) * 10) / 10, men: all, per: Math.max(1, Math.ceil(shownMen / TROOPS.figures)),
    pieces,
    corps: foes.map((c) => ({ id: c.id, nat: c.nat, name: c.name, kind: c.kind, divs: c.divs, men: men(c), posture: postureOf(c), works: c.dug ? Math.round((c.dugFor ?? 0) * 10) / 10 : 0 })),
    law,
  };
}

/** One hit as the 3D game shows it ('air:mark-hit'): shown, never counted. */
export interface MarkNews { piece: number; strike: 'bomb' | 'strafe'; how: 'burn' | 'wreck' | 'blast' | 'scatter' | 'near'; men: number; big: boolean; x: number; y: number }
export type MarkState = 'ok' | 'hit' | 'burn' | 'out';

/**
 * The troops under a raid as the battle goes: what the law has earned so far, and which pieces are seen hit. The cut is
 * the campaign's own from the bombers that have dropped and the fighters that have strafed; the hits are drawn from a
 * stream of their own and move no count.
 */
export class TroopBook {
  private out = new Map<number, number>();
  private state = new Map<number, MarkState>();
  private changed = new Set<number>();
  /** The men (and what each wrecked vehicle or gun is counted as) shown put out of action so far. */
  private shown = 0;
  private now = { dropped: 0, strafed: 0 };

  constructor(private P: MarkPlace, private looks: Rng) {}

  get law(): TroopLaw { return this.P.law; }
  cut(): number { return troopCutOf(this.P.law, this.now.dropped, this.now.strafed); }
  /** What the troops have cost the enemy so far on the campaign's scale (aircraft), as fly() will value it. */
  value(dropped = this.now.dropped, strafed = this.now.strafed): number { return troopWorth(this.P.law, dropped, strafed); }
  /** The men the law books lost at that cut: each corps's own, less where it is dug in. */
  private booked(cut: number): number {
    return this.P.corps.reduce((s, c) => s + c.men * cut * (c.posture === 'dug' ? this.P.law.dug : 1) * this.P.law.str, 0);
  }

  /**
   * A bomb pattern or a strafing pass has come down at (x, y), with that many bombers dropped and fighters strafed in
   * all: the pieces it is seen to hit, nearest first, as far as the men the law now books allow; where they allow none,
   * one near miss, so every release shows on the ground it fell on.
   */
  land(dropped: number, strafed: number, strike: 'bomb' | 'strafe', x: number, y: number): MarkNews[] {
    this.now = { dropped: Math.max(this.now.dropped, dropped), strafed: Math.max(this.now.strafed, strafed) };
    const left = (p: MarkPiece): number => p.n - (this.out.get(p.id) ?? 0);
    const live = this.P.pieces.filter((p) => left(p) > 0).sort((a, b) => Math.hypot(a.x - x, a.y - y) - Math.hypot(b.x - x, b.y - y) || a.id - b.id);
    const news: MarkNews[] = [];
    let due = this.booked(this.cut()) - this.shown;
    const most = strike === 'bomb' ? 5 : 3;
    for (let k = 0; k < most && live.length; k++) {
      // One of the nearest few, so a pattern does not walk down the column in order.
      const p = live.splice(Math.min(live.length - 1, this.looks.int(Math.min(3, live.length))), 1)[0];
      const worth = TROOPS.worth[p.what] ?? 1;
      const share = due / (most - k);
      const n = Math.min(left(p), Math.floor(Math.max(share, Math.min(due, worth)) / worth));
      if (n <= 0) continue;
      this.out.set(p.id, (this.out.get(p.id) ?? 0) + n);
      this.shown += n * worth; due -= n * worth;
      const gone = left(p) <= 0, soft = p.what === 'men', motor = p.what === 'tank' || p.what === 'lorry' || p.what === 'halftrack' || p.what === 'car';
      const how: MarkNews['how'] = soft ? (strike === 'bomb' && n >= 10 ? 'blast' : 'scatter') : motor && this.looks.chance(0.55) ? 'burn' : strike === 'bomb' && !motor ? 'blast' : 'wreck';
      this.state.set(p.id, gone ? 'out' : how === 'burn' ? 'burn' : 'hit');
      this.changed.add(p.id);
      news.push({ piece: p.id, strike, how, men: n * worth, big: n * 2 >= p.n, x: p.x, y: p.y });
    }
    if (!news.length) {
      const p = this.P.pieces.slice().sort((a, b) => Math.hypot(a.x - x, a.y - y) - Math.hypot(b.x - x, b.y - y) || a.id - b.id)[0];
      if (p) news.push({ piece: p.id, strike, how: 'near', men: 0, big: false, x: p.x, y: p.y });
    }
    return news;
  }

  /** The share of the army's own anti-aircraft pieces still firing (0 where it has none). */
  fire(): number {
    const aa = this.P.pieces.filter((p) => p.what === 'aa'), all = aa.reduce((s, p) => s + p.n, 0);
    return all > 0 ? Math.round((aa.reduce((s, p) => s + p.n - (this.out.get(p.id) ?? 0), 0) / all) * 100) / 100 : 0;
  }

  /** For each view: the cut earned so far, the guns' fire, and the pieces changed since the last view. */
  view(): { fire: number; cut: number; pieces: { id: number; state: MarkState; men: number }[] } {
    const pieces = [...this.changed].sort((a, b) => a - b).map((id) => {
      const p = this.P.pieces.find((q) => q.id === id)!;
      return { id, state: this.state.get(id) ?? 'ok', men: p.n - (this.out.get(id) ?? 0) };
    });
    this.changed.clear();
    return { fire: this.fire(), cut: Math.round(this.cut() * 10000) / 10000, pieces };
  }

  /** The men shown put out of action so far, and the men the law books at the cut earned so far (the first never passes the second). */
  tally(): { shown: number; booked: number } { return { shown: this.shown, booked: this.booked(this.cut()) }; }
}
