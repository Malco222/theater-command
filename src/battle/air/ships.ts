// Strikes on ships in the air battle (ruling 238, the design director, 19 November 2026): a raid on a harbour and a
// strike on a fleet at sea, fought out like any raid. Which ships go down is drawn from the campaign's own law
// (shipHits) as each box of bombers drops, against throws the battle makes once at the start, and the campaign books
// exactly what the battle drew (b397's law, as for the railway and the factory). A carrier's strike aircraft fly as
// torpedo, dive and level bombers by the record's mix for that nation and year (ruling 238(2)): for show and for how
// they fly, so who flies low and who flies high may change who is lost, inside the cap. Land bomber groups stay level
// bombers (ruling 59). Nothing here moves a count the campaign does not already make.

import { CARRIERS, deckBred, foundShare, shipHitsBy, shipLaw, type AirAssess, type ShipLaw } from '../../campaign/air';
import { techMul } from '../../campaign/research';
import type { AirMission, Game, ShipClass } from '../../campaign/types';
import type { Rng } from '../../core/rng';
import { SKY, type Band } from './book';
import { fighterOf, type When } from './doctrine';
import { faceOnBoard, onBoard, recordMonth, stageOf, STAGES, type Pt, type StagePlace } from './stages';
import type { AirUnit } from './types';

const CLASSES: ShipClass[] = ['bb', 'cv', 'ca', 'dd', 'ss'];
const COMMONWEALTH = new Set(['UK', 'CAN', 'AUS', 'NZL', 'SAF', 'IND']);
const at = (w: When): number => w.year + w.month / 12;

/** How a carrier's strike aircraft attack: a torpedo run low over the water, a dive from height, or level from height. */
export type Strike = 'torpedo' | 'dive' | 'level';

/** One ship on the field: drawn, hit and sunk one by one; the campaign counts its class in its fleet. */
export interface ShipPiece {
  id: number;
  fleet: number;
  nat: string;
  adj: string;
  cls: ShipClass;
  /** The record's name: only at the record's own berth or station, in the record's month, of the record's class (ruling 238(4)). */
  name: string | null;
  /** The stage's berth it lies at, if any. */
  berth: string | null;
  x: number;
  y: number;
  face: number;
  /** How open it lies to a torpedo run, 0 (inboard, in dock) to 1 (outboard, at sea). */
  open: number;
  /** In a dry dock: no torpedo reaches it. */
  dock?: boolean;
}

/** A carrier air group's aircraft as the 3D game draws them, and how its bombers split by kind. */
export interface WingSplit { corps: number; ftr: string; parts: { strike: Strike; share: number; model: string; band: Band }[] }

/** What a strike on ships carries on its ticket. */
export interface ShipPlace {
  /** The campaign's law (shipLaw), and what fly() multiplies the bombers that drop by before it (the bombers' research). */
  law: ShipLaw;
  mul: number;
  /** The set piece it is fought on, if any. */
  stage: StagePlace | null;
  pieces: ShipPiece[];
  wings: WingSplit[];
  /** A harbour caught unready in the month war came: its guns come up as the raid goes on (shown only, ruling 238(1)). */
  surprise: boolean;
}

/**
 * The record's mix of a carrier strike by kind, for the historian: Pearl Harbor's two waves (40 torpedo, 103 level and
 * 129 dive bombers), the carrier battles of 1942 for strikes at sea, the Fleet Air Arm's Swordfish at Taranto.
 */
export function deckMix(nat: string, w: When, m: 'harbour' | 'fleet'): [Strike, number][] {
  const d = at(w);
  if (nat === 'JAP' || nat === 'JPN') return m === 'harbour' ? [['torpedo', 0.15], ['level', 0.38], ['dive', 0.47]] : [['torpedo', 0.45], ['dive', 0.55]];
  if (nat === 'USA') return d < 1943.5 ? [['torpedo', 0.25], ['dive', 0.75]] : [['torpedo', 0.35], ['dive', 0.65]];
  if (COMMONWEALTH.has(nat)) return m === 'harbour' ? [['torpedo', 0.55], ['dive', 0.45]] : [['torpedo', 0.8], ['dive', 0.2]];
  return [['torpedo', 0.4], ['dive', 0.6]];
}

/** The aircraft the 3D game draws for a carrier group, by nation and date (for the historian; the plain model where it has none). */
export function deckModels(nat: string, w: When): { ftr: string; torpedo: string; dive: string; level: string } {
  const d = at(w);
  if (nat === 'JAP' || nat === 'JPN') return { ftr: 'Zero', torpedo: d < 1944 ? 'B5N' : 'B6N', level: d < 1944 ? 'B5N' : 'B6N', dive: d < 1944 ? 'D3A' : 'D4Y' };
  // Midway was the Devastator's last fight (June 1942); the Hellcat from August 1943; the Helldiver from November 1943.
  if (nat === 'USA') return { ftr: d < 1943 + 7 / 12 ? 'Wildcat' : 'Hellcat', torpedo: d < 1942.5 ? 'TBD' : 'TBF', level: d < 1942.5 ? 'TBD' : 'TBF', dive: d < 1943 + 10 / 12 ? 'SBD' : 'SB2C' };
  if (COMMONWEALTH.has(nat)) return { ftr: d < 1942 ? 'Fulmar' : 'Seafire', torpedo: d < 1943.5 ? 'Swordfish' : 'Barracuda', level: d < 1943.5 ? 'Swordfish' : 'Barracuda', dive: d < 1941.5 ? 'Skua' : d < 1943.5 ? 'Swordfish' : 'Barracuda' };
  return { ftr: fighterOf(nat, at(w)), torpedo: `Torpedo:${nat}`, level: `Torpedo:${nat}`, dive: `Dive:${nat}` };
}

/**
 * The height a kind of strike is counted at: the dive bombers high, the torpedo and level bombers in the middle. The
 * torpedo bombers' run in low over the water is the 3D game's to act (the box's 'strike' and 'run' in its extra): flown
 * low on the counts the whole way in, the torpedo boxes moved a raid's losses a fifth past the card's, beyond ruling
 * 238's plus or minus 15 per cent, while the dive boxes' height moves them a tenth (who flies high still decides who is lost).
 */
const BAND: Record<Strike, Band> = { torpedo: 1, level: 1, dive: 2 };

/**
 * A strike on ships for the ticket: the campaign's law, the ships each fleet has as pieces on the board, the stage
 * (by place) and how each carrier group splits. T is the board's target, 'turn' the board's own turn (airTicket's).
 */
export function shipPlace(g: Game, a: AirAssess, m: AirMission, target: number, nat: string, w: When, T: { x: number; y: number }, turn: number): ShipPlace | null {
  if (m !== 'harbour' && m !== 'fleet') return null;
  const law = shipLaw(a, m, foundShare(g, a, nat));
  const quarry = m === 'fleet' ? a.ships[0] : null;
  const key = stageOf(m, m === 'harbour' ? g.regions[target]?.name ?? null : null, quarry ? g.seas[quarry.zone]?.key ?? null : null);
  const stage: StagePlace | null = key ? { key, aim: STAGES[key].aim[m]!, turn } : null;
  const pieces = m === 'harbour' ? moored(g, law, stage, T, w) : atSeaPieces(g, law, stage, T, w);
  const wings: WingSplit[] = [];
  for (const x of a.att) {
    if (!deckBred(x.f) || wings.some((v) => v.corps === x.f.id)) continue;
    const md = deckModels(x.f.nat, w);
    wings.push({ corps: x.f.id, ftr: md.ftr, parts: deckMix(x.f.nat, w, m).map(([strike, share]) => ({ strike, share, model: md[strike], band: BAND[strike] })) });
  }
  return { law, mul: techMul(g, nat, 'bombers'), stage, pieces, wings, surprise: a.surprise };
}

/** The ships as pieces on the board, in harbour or at sea (shipPlace's own layout, for the tests and the browser's air page). */
export const layShips = (g: Game, law: ShipLaw, m: 'harbour' | 'fleet', stage: StagePlace | null, T: { x: number; y: number }, w: When): ShipPiece[] =>
  (m === 'harbour' ? moored(g, law, stage, T, w) : atSeaPieces(g, law, stage, T, w));

/** The ships of each fleet in harbour: at the record's berths for their class on a stage, then its other moorings; in rows off the quays elsewhere. */
function moored(g: Game, law: ShipLaw, stage: StagePlace | null, T: { x: number; y: number }, w: When): ShipPiece[] {
  const out: ShipPiece[] = [];
  const S = stage ? STAGES[stage.key] : null;
  const named = !!S && recordMonth(stage!.key, w);
  const used = new Set<string>(), spotsUsed = new Set<number>();
  for (const f of law.fleets) {
    const adj = g.nations[f.nat]?.adj ?? f.nat;
    for (const c of CLASSES) {
      for (let k = 0; k < f.ships[c]; k++) {
        const id = out.length + 1;
        if (S) {
          const b = S.berths.find((x) => x.cls === c && !used.has(x.id));
          if (b) {
            used.add(b.id);
            const [x, y] = onBoard(stage!, b.at, T);
            out.push({ id, fleet: f.id, nat: f.nat, adj, cls: c, name: named && (!S.record.nat || S.record.nat === f.nat) ? b.name : null, berth: b.id, x, y, face: faceOnBoard(stage!, b.face), open: b.open, ...(b.dock ? { dock: true } : {}) });
            continue;
          }
          // Destroyers nest at the destroyer buoys first; everything else takes the nearest free mooring to the aim.
          let j = c === 'dd' ? S.spots.findIndex((s, i) => s.nest && !spotsUsed.has(i)) : -1;
          if (j < 0) {
            let best = Infinity;
            S.spots.forEach((s, i) => { if (spotsUsed.has(i) || s.nest) return; const d = Math.hypot(s.at[0] - stage!.aim[0], s.at[1] - stage!.aim[1]); if (d < best) { best = d; j = i; } });
          }
          if (j >= 0) {
            spotsUsed.add(j);
            const s = S.spots[j], [x, y] = onBoard(stage!, s.at, T);
            out.push({ id, fleet: f.id, nat: f.nat, adj, cls: c, name: null, berth: null, x, y, face: faceOnBoard(stage!, s.face), open: 0.7 });
            continue;
          }
        }
        // Rows across the board just past the target's centre, the big ships in front, each row 250 m deep.
        const row = Math.floor(out.length / 8), col = out.length % 8;
        out.push({ id, fleet: f.id, nat: f.nat, adj, cls: c, name: null, berth: null, x: T.x + (col - 3.5) * 260, y: T.y + 300 + row * 250, face: Math.PI / 2, open: col % 2 ? 0.4 : 0.9 });
      }
    }
  }
  return out;
}

/**
 * The ships of a fleet at sea in its cruising order: the carriers in a box at the centre, the battleships and cruisers
 * in a ring round them, the destroyers in a wider screen; a second fleet some way off. On a stage in the record's
 * month, the record's names by nation and class in the record's order.
 */
function atSeaPieces(g: Game, law: ShipLaw, stage: StagePlace | null, T: { x: number; y: number }, w: When): ShipPiece[] {
  const out: ShipPiece[] = [];
  const S = stage ? STAGES[stage.key] : null;
  const named = !!S && recordMonth(stage!.key, w);
  const given = new Map<string, number>();
  const course = stage ? faceOnBoard(stage, 135) : 2.3;
  law.fleets.forEach((f, fi) => {
    const adj = g.nations[f.nat]?.adj ?? f.nat;
    const cx = T.x + fi * 9_000, cy = T.y + fi * 2_000;
    const ring = (r: number, n: number, k: number, turn = 0): Pt => { const t = turn + (2 * Math.PI * k) / Math.max(1, n); return [cx + r * Math.sin(t), cy + r * Math.cos(t)]; };
    const cv = f.ships.cv, mid = f.ships.bb + f.ships.ca, dd = f.ships.dd + f.ships.ss;
    let iCv = 0, iMid = 0, iDd = 0;
    for (const c of CLASSES) {
      for (let k = 0; k < f.ships[c]; k++) {
        let p: Pt;
        if (c === 'cv') { const cols = Math.ceil(Math.sqrt(cv)); p = [cx + ((iCv % cols) - (cols - 1) / 2) * 2_500, cy + (Math.floor(iCv / cols) - (Math.ceil(cv / cols) - 1) / 2) * 2_500]; iCv++; }
        else if (c === 'bb' || c === 'ca') p = ring(cv ? 4_000 : 1_500 * Math.min(1, iMid), mid, iMid++, 0.4);
        else p = ring(6_500, dd, iDd++, 0.1);
        const list = named ? S!.fleets?.[f.nat]?.[c] : undefined;
        const n = given.get(`${f.nat}:${c}`) ?? 0;
        const name = list && n < list.length ? list[n] : null;
        if (name) given.set(`${f.nat}:${c}`, n + 1);
        out.push({ id: out.length + 1, fleet: f.id, nat: f.nat, adj, cls: c, name, berth: null, x: p[0], y: p[1], face: course, open: 1 });
      }
    }
  });
  return out;
}

/**
 * A carrier group's bombers split by kind (ruling 238(2)): each kind a box of its own of the same campaign group, at
 * its own height, with the model of its kind; its fighters drawn as the carrier fighter. Returns the units and what
 * the guns' toll is scaled by so the raid as a whole pays the campaign's (a torpedo run low meets more of the guns,
 * a dive bomber's approach high fewer, but the toll is the campaign's sum).
 */
export function splitWings(units: AirUnit[], P: ShipPlace, drawn: (n: number) => Pick<AirUnit, 'figures0' | 'figures' | 'per' | 'w' | 'd'>, nextId: () => number): { units: AirUnit[]; flakNorm: number } {
  const out: AirUnit[] = [];
  for (const u of units) {
    const wing = u.side === 'att' ? P.wings.find((x) => x.corps === u.corps) : undefined;
    if (!wing) { out.push(u); continue; }
    if (u.air === 'ftr') { u.model = wing.ftr; out.push(u); continue; }
    // Largest remainder, so the parts add up to the group.
    const raw = wing.parts.map((p) => u.n * p.share);
    const n = raw.map(Math.floor);
    let left = u.n - n.reduce((s, x) => s + x, 0);
    raw.map((r, i) => [r - Math.floor(r), i] as const).sort((a, b) => b[0] - a[0] || a[1] - b[1]).forEach(([, i]) => { if (left > 0) { n[i]++; left--; } });
    let first = true;
    wing.parts.forEach((p, i) => {
      if (n[i] <= 0) return;
      const v: AirUnit = first ? u : { ...u, id: nextId(), contacts: [], fire: [], strag: [], kit: { ...u.kit }, job: { ...u.job } };
      first = false;
      Object.assign(v, drawn(n[i]), { n: n[i], n0: n[i], men0: n[i], band: p.band, to: p.band, alt: SKY.bands.alt[p.band], model: p.model, strike: p.strike });
      out.push(v);
    });
  }
  const raid = out.filter((u) => u.side === 'att' && u.air === 'bmr');
  const all = raid.reduce((s, u) => s + u.n, 0), weighed = raid.reduce((s, u) => s + u.n * SKY.flak.band[u.band], 0);
  return { units: out, flakNorm: P.wings.length && weighed > 0 ? all / weighed : 1 };
}

/** How a ship goes down, as the 3D game acts it out (shown only): by its class, and whether it lies in harbour or in dock. */
function howDown(p: ShipPiece, harbour: boolean): [string, number][] {
  if (p.dock) return [['burn', 0.6], ['explode', 0.4]];
  if (p.cls === 'cv') return [['burn', 0.7], ['explode', 0.3]];
  if (p.cls === 'dd') return [['break', 0.4], ['explode', 0.3], [harbour ? 'settle' : 'sink', 0.3]];
  if (p.cls === 'ss') return [[harbour ? 'settle' : 'sink', 1]];
  return [[harbour ? 'settle' : 'sink', 0.5], ['capsize', 0.3], ['explode', 0.2]];
}
const WORD: Record<ShipClass, string> = { bb: 'battleship', cv: 'carrier', ca: 'cruiser', dd: 'destroyer', ss: 'submarine' };
const WORDS: Record<ShipClass, string> = { bb: 'battleships', cv: 'carriers', ca: 'cruisers', dd: 'destroyers', ss: 'submarines' };
/** How a ship goes down, in the UI director's words (03:43Z): a ship that burns is lost, as the campaign counts it. */
const VERB: Record<string, string> = { settle: 'settles on the bottom', sink: 'goes down', capsize: 'rolls over', explode: 'blows up', burn: 'is ablaze from end to end and cannot be saved', break: 'breaks in two' };

/**
 * The dispatch for a ship going down (the UI director, 03:43Z): a named ship by its class and name, never a navy's
 * prefix ("The battleship Arizona blows up."); the enemy's unnamed by nation ("An Italian destroyer breaks in two.");
 * the player's own as every other dispatch says them ("One of your cruisers goes down."). The name rides in the
 * event's extra too, for the 3D to set in italics.
 */
export function sinkLine(p: ShipPiece, how: string, own: string | null): string {
  const who = p.name ? `The ${WORD[p.cls]} ${p.name}` : p.nat === own ? `One of your ${WORDS[p.cls]}` : `${/^[AEIOU]/.test(p.adj) ? 'An' : 'A'} ${p.adj} ${WORD[p.cls]}`;
  return `${who} ${VERB[how]}.`;
}

/** One ship's state on the field, for the 3D game. */
export interface ShipShown extends ShipPiece { hits: number; sunk: { t: number; how: string } | null }

/** What the field says happened to a ship as a box dropped. */
export interface ShipNews {
  what: 'air:ship-hit' | 'air:ship-sunk';
  piece: ShipShown;
  text: string;
  extra: Record<string, unknown>;
  /** A hit not drawn but given to a ship that goes down without one, shown just before it sinks (never in the event). */
  given?: true;
}

/**
 * The ships under attack through the battle. The throws are made once, at the start, one for each class in each
 * fleet; as each box drops, the ships of a class that go down are those whose throw the campaign's expected sinkings
 * (shipHits on the bombers dropped so far) have passed: so a sinking, once seen, stays, and over many raids each class
 * sinks as often as fly()'s own dice sink it. Which piece of the class goes down, and the hits drawn on the others, are
 * for show (their own stream), and the same with the showing on or off.
 */
export class ShipBook {
  readonly pieces: ShipShown[];
  /** Each wing's quarry (S2): the campaign group whose boxes fly together, and the piece they go for (null: none left). */
  private quarries = new Map<number, number | null>();
  private leads = new Map<number, Strike | 'bomb'>();
  private parts = new Map<number, { strike: Strike | 'bomb'; n: number }[]>();
  private throws = new Map<string, number>();
  private down = new Map<string, number>();
  /** Ruling 240 (C3), only where the law carries `tough`: what the plain law (no one's damage control) sinks on the same throws. */
  private plainDown: Map<string, number> | null;
  private dropped = 0;
  private hitsSoFar = 0;
  private big = false;
  private readonly harbour: boolean;
  private readonly bombers0: number;

  /** own: the player's nation where the ships under attack are the player's, for "One of your cruisers". */
  constructor(private P: ShipPlace, throws: Rng, private shown: Rng, bombers0: number, private own: string | null = null) {
    this.harbour = P.law.harbour;
    this.bombers0 = Math.max(1, bombers0);
    this.pieces = P.pieces.map((p) => ({ ...p, hits: 0, sunk: null }));
    for (const f of P.law.fleets) for (const c of CLASSES) if (f.ships[c] > 0) { this.throws.set(`${f.id}:${c}`, throws.next()); this.down.set(`${f.id}:${c}`, 0); }
    this.plainDown = P.law.tough ? new Map([...this.down.keys()].map((k) => [k, 0])) : null;
  }

  /**
   * Each wing takes its quarry (the tactics panel's S2): the afloat ship its lead kind (its largest part) weighs most
   * as pickHit weighs it, the law's aim by how open the ship lies, so at sea the carriers first, then the battleships,
   * the cruisers and the destroyers, by the campaign's own table; a torpedo never takes a ship in dock. Among ships of
   * equal weight, the one fewest wings have taken; then the one the wing as a whole weighs most, so that in harbour a
   * wing led by its dive bombers takes a ship its torpedo bombers can reach, outboard; then the nearest the target, then
   * the first. A wing is a carrier group's boxes together, or a land group's single box; boxes in the order the battle
   * has them. Nothing is drawn.
   */
  aim(boxes: { corps: number; strike?: Strike; n: number }[]): void {
    const big = new Map<number, number>();
    for (const b of boxes) {
      if (!this.leads.has(b.corps) || b.n > big.get(b.corps)!) { this.leads.set(b.corps, b.strike ?? 'bomb'); big.set(b.corps, b.n); }
      this.parts.set(b.corps, [...(this.parts.get(b.corps) ?? []), { strike: b.strike ?? 'bomb', n: b.n }]);
    }
    for (const corps of this.leads.keys()) this.retake(corps);
  }

  /** A wing's quarry: the piece its boxes go for, null when no ship it could hit is afloat (undefined: not a wing). */
  quarryOf(corps: number): number | null | undefined { return this.quarries.get(corps); }

  private retake(corps: number): void {
    const lead = this.leads.get(corps)!, T = SKY.target, eps = 1e-9;
    const taken = (p: ShipShown): number => [...this.quarries].filter(([c, q]) => c !== corps && q === p.id).length;
    const whole = (p: ShipShown): number => (this.parts.get(corps) ?? []).reduce((t, x) => t + x.n * this.weigh(p, x.strike), 0);
    let best: ShipShown | null = null, bw = 0;
    for (const p of this.pieces) {
      const w = this.weigh(p, lead);
      if (w <= 0) continue;
      if (!best || w > bw + eps) { best = p; bw = w; continue; }
      if (w < bw - eps) continue;
      const k = taken(p) - taken(best), o = whole(p) - whole(best), d = Math.hypot(p.x - T.x, p.y - T.y) - Math.hypot(best.x - T.x, best.y - T.y);
      if (k < 0 || (k === 0 && (o > eps || (Math.abs(o) <= eps && (d < -eps || (Math.abs(d) <= eps && p.id < best.id)))))) best = p;
    }
    this.quarries.set(corps, best ? best.id : null);
  }

  /** 2: the wing's own quarry; 1: another wing's; 0: nobody's. */
  private aimed(p: ShipShown, corps: number | undefined): number {
    if (corps !== undefined && this.quarries.get(corps) === p.id) return 2;
    return [...this.quarries.values()].includes(p.id) ? 1 : 0;
  }

  /** How much a box of this kind goes for a ship: the law's aim by how open it lies (pickHit's own weight). */
  private weigh(p: ShipShown, strike: Strike | 'bomb'): number {
    return p.sunk || (strike === 'torpedo' && p.dock) ? 0 : CARRIERS.strike.aim[p.cls] * (0.25 + (strike === 'torpedo' ? p.open : 0.5));
  }

  /**
   * The bombers have dropped so far 'dropped' in all, the last of them 'strike' bombers of the campaign group 'corps':
   * the ships hit and sunk. A hit drawn in the class of the wing's quarry lands on the quarry while it floats (the
   * tactics panel's S3): the hits by class, the sinkings and the stream they are drawn from are as before.
   */
  drop(dropped: number, strike: Strike | 'bomb', t: number, corps?: number): ShipNews[] {
    if (dropped <= this.dropped) return [];
    this.dropped = dropped;
    const L = this.P.law, exp = shipHitsBy(L, dropped * this.P.mul), news: ShipNews[] = [];
    // The hits drawn on the ships, for show: as many more as the law expects from these bombs.
    const more = this.shown.roll(Math.max(0, exp.hits - this.hitsSoFar));
    this.hitsSoFar = Math.max(this.hitsSoFar, exp.hits);
    for (let k = 0; k < Math.min(more, 3 * this.pieces.length); k++) {
      const drawn = this.pickHit(strike);
      if (!drawn) break;
      const q = corps === undefined ? null : this.pieces.find((x) => x.id === this.quarries.get(corps)) ?? null;
      const p = q && q.cls === drawn.cls && this.weigh(q, strike) > 0 ? q : drawn;
      p.hits++;
      news.push({ what: 'air:ship-hit', piece: p, text: '', extra: { ship: p.id, strike, x: p.x, y: p.y } });
    }
    // The sinkings, by the campaign's law against the battle's throws. Ruling 240 (C3), where some fleet's damage control
    // is not 1: each fleet sinks at its own eased share (shipHitsBy's `by`, what fly() rolls each fleet's dice on), not a
    // head count's part of the eased total, so a navy beside another in one harbour keeps its own figure (the design
    // director, 13:38Z on step 9). Without `by` (the switch off, or nobody's figure other than 1) the head count's part,
    // as before.
    L.fleets.forEach((f, i) => {
      for (const c of CLASSES) {
        const key = `${f.id}:${c}`, u = this.throws.get(key);
        if (u === undefined || L.have[c] <= 0) continue;
        const e = exp.by ? exp.by[i][c] : exp.sunk[c] * (f.ships[c] / L.have[c]);
        const n = e > u ? Math.min(f.ships[c], Math.floor(e - u) + 1) : 0;
        // Ruling 240: the plain law's sinkings on the same throw (no new draw), for the ships damage control kept afloat
        // (kept()). A larger expectation never gives the smaller count, so these are never fewer than n.
        if (this.plainDown) {
          const e0 = exp.plain ? exp.plain[c] * (f.ships[c] / L.have[c]) : e;
          this.plainDown.set(key, Math.max(n, e0 > u ? Math.min(f.ships[c], Math.floor(e0 - u) + 1) : 0));
        }
        for (let k = this.down.get(key)!; k < n; k++) {
          // The most hit goes down; among those as often hit, the dropping wing's quarry, then another wing's (S3).
          const p = this.pieces.filter((x) => x.fleet === f.id && x.cls === c && !x.sunk).sort((a, b) => b.hits - a.hits || this.aimed(b, corps) - this.aimed(a, corps) || b.open - a.open || a.id - b.id)[0];
          if (!p) break;
          const how = this.pick(howDown(p, this.harbour));
          // A ship that goes down took a hit: one the draw gave none is shown taking it first, from the wing that sank it
          // with that wing's weapon (the design director on K4a, 20 November 03:52Z). Shown only: no rule reads it (b418).
          if (p.hits === 0) {
            p.hits = 1;
            news.push({ what: 'air:ship-hit', piece: p, text: '', extra: { ship: p.id, strike, x: p.x, y: p.y }, given: true });
          }
          p.sunk = { t, how };
          const big = !this.big && how === 'explode' && (c === 'bb' || c === 'cv');
          if (big) this.big = true;
          news.push({ what: 'air:ship-sunk', piece: p, text: sinkLine(p, how, this.own), extra: { ship: p.id, how, strike, x: p.x, y: p.y, ...(p.name ? { name: p.name } : {}), ...(big ? { big: true } : {}) } });
        }
        this.down.set(key, n);
      }
    });
    // A wing whose quarry has gone down takes the next by the same rule (S2).
    for (const [c, q] of this.quarries) if (q !== null && this.pieces.find((x) => x.id === q)!.sunk) this.retake(c);
    return news;
  }

  private pickHit(strike: Strike | 'bomb'): ShipShown | null {
    const w = (p: ShipShown): number => this.weigh(p, strike);
    const all = this.pieces.reduce((s, p) => s + w(p), 0);
    if (all <= 0) return null;
    let x = this.shown.next() * all;
    for (const p of this.pieces) { x -= w(p); if (x < 0 && w(p) > 0) return p; }
    return this.pieces.filter((p) => w(p) > 0).pop() ?? null;
  }

  private pick(rows: [string, number][]): string {
    const all = rows.reduce((s, [, k]) => s + k, 0);
    let x = this.shown.next() * all;
    for (const [h, k] of rows) { x -= k; if (x < 0) return h; }
    return rows[rows.length - 1][0];
  }

  /** What the ships sunk so far are worth on the campaign's scale (fly()'s own measure). */
  value(): number {
    const V = CARRIERS.strike.value;
    let v = 0;
    for (const [key, n] of this.down) v += n * V[key.split(':')[1] as ShipClass];
    return v;
  }

  /** The ships of each class sunk in each fleet, every class it has (0 included): what the campaign books (fly(), battle.sunk). */
  sunk(): Record<number, Partial<Record<ShipClass, number>>> {
    const out: Record<number, Partial<Record<ShipClass, number>>> = {};
    for (const [key, n] of this.down) { const [f, c] = key.split(':'); (out[Number(f)] ??= {})[c as ShipClass] = n; }
    return out;
  }

  /**
   * Ruling 240 (C3; the design director, 13:38Z on step 9): the ships of each class in each fleet that damage control
   * kept afloat so far, every class it has (0 included): on each fleet and class's own throw, what the plain law would
   * have sunk less what the eased law sank, as fly()'s rollBoth counts them on the dice's path. Undefined where the law
   * carries no `tough` (the switch off, or no fleet's figure other than 1): then nothing is said of it anywhere.
   */
  kept(): Record<number, Partial<Record<ShipClass, number>>> | undefined {
    if (!this.plainDown) return undefined;
    const out: Record<number, Partial<Record<ShipClass, number>>> = {};
    for (const [key, n0] of this.plainDown) { const [f, c] = key.split(':'); (out[Number(f)] ??= {})[c as ShipClass] = n0 - this.down.get(key)!; }
    return out;
  }

  /** The ships for the 3D game, and how far a harbour caught unready has its guns manned (shown only, ruling 238(1)). */
  view(): { guns: number; pieces: ShipShown[] } {
    return { guns: this.P.surprise ? Math.min(1, 0.15 + (0.85 * this.dropped) / this.bombers0) : 1, pieces: this.pieces.map((p) => ({ ...p, ...(p.sunk ? { sunk: { ...p.sunk } } : {}) })) };
  }
}

