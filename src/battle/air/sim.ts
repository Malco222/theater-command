// The air battle: air groups as blocks on a 100 km board round the target, wearing each other down while in contact.
//
// Counted, not flown (book.ts). Each tick: the computer gives its orders; groups climb or dive between the three
// heights and move toward what their job asks; fighters burn fuel; groups that touch are in contact and lose
// aircraft at the rates the campaign's own numbers give; the guns round the target wear down whatever flies through
// them; bombers that hold their run over the target drop; morale breaks groups that bleed too much. When a group's
// count drops by one, an 'air:lost' event says how the 3D side shows that aircraft going down.

import { Rng } from '../../core/rng';
import { dist, bearing } from '../core/geometry';
import { fieldRatio } from '../core/result';
import type { BattleSim, BattleView, Command, FieldEvent, FieldResult, FieldTicket, Side, UnitState, Zone } from '../core/types';
import { DT } from '../core/types';
import { SKY, type Band } from './book';
import { drawFall, fallWords } from './falls';
import type { AirJob, AirOutcome, AirPlace, AirUnit } from './types';
import { afterWords, staffHow } from '../../campaign/airfought';
import type { VictoryLevel } from '../../core/ticket';
export { peopleOf } from '../../campaign/airfought';

const C = SKY.cues;

const clamp01 = (v: number): number => (v < 0 ? 0 : v > 1 ? 1 : v);
const off = (u: AirUnit): boolean => u.state === 'gone' || u.state === 'withdrawn';
const flying = (u: AirUnit): boolean => !off(u) && u.job.do !== 'wait';
/** A group that has broken off or turned for home: it runs, and does not fight back. */
const away = (u: AirUnit): boolean => u.job.do === 'home' || u.state === 'routing';

/**
 * A group's name as the subject of a dispatch, and whether it takes a plural verb (the UI director, air review 10):
 * "Jagdgeschwader 4 is in a fight", but "The Home Fleet's carrier fighters are in a fight". 'whom' is the name inside
 * a sentence.
 */
export function subjectOf(name: string): { who: string; whom: string; many: boolean } {
  const m = /^(.+?) (carrier [a-z]+s)$/.exec(name);
  if (m) return { who: `The ${m[1]}'s ${m[2]}`, whom: `the ${m[1]}'s ${m[2]}`, many: true };
  return { who: name, whom: name, many: /(^| )[a-z]+s$/.test(name) };
}

/** A line about a group, its verbs agreeing with the name: told(name, '{N} [is|are] in a fight.'). */
export function told(name: string, text: string): string {
  const s = subjectOf(name);
  return text.replace(/\{N\}/g, s.who).replace(/\[([^|\]]*)\|([^\]]*)\]/g, (_, one: string, many: string) => (s.many ? many : one));
}

export class AirSim implements BattleSim<AirUnit> {
  readonly domain = 'air' as const;
  readonly ticket: FieldTicket<AirPlace>;
  readonly place: AirPlace;
  t = 0;
  units: AirUnit[] = [];
  over: FieldResult | null = null;
  ai: Record<Side, boolean>;
  private rng: Rng;
  private fallRng: Rng;
  /** Whether a share of an aircraft owed this tick falls (fall). */
  private lossRng: Rng;
  private events: FieldEvent[] = [];
  private log: FieldEvent[] = [];
  private zones: Zone[];
  private thinkAt = 0;
  private quietFor = 0;
  private scrambled = false;
  private gaveUp: Side | null = null;
  private toStaff = false;
  /** Each side's losses when the player handed the rest to the staff, for the after card. */
  private before: { att: { ftr: number; bmr: number }; def: { ftr: number; bmr: number } } | null = null;
  /** The staff's reckoning of what the raid would cost each pool, at the start (see expect), and what is handed back at the press. */
  private start = { foe: 0, ftr: 0, bmr: 0 };
  private staff: AirOutcome['staff'];
  /** Each group's morale at the start, for how near its side is to breaking off; the sides already warned of it. */
  private mor0 = new Map<number, number>();
  private warned = new Set<Side>();
  /** Whether any group has met an enemy yet (the first contact). */
  fired = false;
  /** The bombers the staff's reckoning expected through at the start (see throughNow). */
  private through0 = 0;
  private flakT: number;
  private flakIn: number;
  /** The campaign's bomber-loss law as the raid card weighed it: the defenders' weight, the escort's weight per bomber, the bombers' own guns. */
  private law = { D: 0, B: 0, ePer: 0, guns: 0, met: 0 };

  constructor(ticket: FieldTicket<AirPlace>) {
    this.ticket = ticket;
    this.place = ticket.place;
    this.rng = new Rng(ticket.seed);
    this.fallRng = this.rng.fork(7);
    this.lossRng = this.rng.fork(11);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    const T = SKY.target;
    this.zones = [
      { id: 1, kind: 'air:target', x: T.x, y: T.y, r: T.r, until: Infinity },
      { id: 2, kind: 'air:flak', x: T.x, y: T.y, r: SKY.flakR, until: Infinity, side: 'def' },
    ];
    // The time a box spends in the flak ring on the staff's run, in to where it drops (it holds the run from the edge
    // of the target ring). The guns' share of the bombers is spread over it: the raid card counts every bomber the
    // guns bring down as one that never bombed, so they fire at the boxes on the run in. Escorts are under the guns
    // in and out.
    this.flakIn = (SKY.flakR - SKY.target.r) / SKY.speed.bmr + SKY.run;
    this.flakT = (2 * (SKY.flakR - SKY.target.r)) / SKY.speed.bmr + 2 * SKY.run;
    let id = 1;
    for (const side of ['att', 'def'] as Side[]) {
      for (const f of ticket[side].forces) {
        const s = this.place.starts[f.corps];
        if (!s) continue;
        const kind = f.kind === 'bmr' ? 'bmr' : 'ftr';
        const mor = clamp01(SKY.morale.base + SKY.morale.ready * f.org);
        const u: AirUnit = {
          id: id++, side, corps: f.corps, nat: f.nat, kind, name: f.name,
          x: s.x + (kind === 'bmr' ? 0 : 1_500 * (side === 'att' ? 1 : 0)), y: s.y,
          face: side === 'att' ? 0 : bearing(s.x, s.y, T.x, T.y),
          str: 1, mor, state: 'steady', men0: f.men, ...this.drawn(kind, f.men), act: s.wait ? 'idle' : 'march', speed: 0,
          air: kind, n: f.men, n0: f.men, band: s.band, to: s.band, alt: s.wait ? 0 : SKY.bands.alt[s.band], climbT: 0,
          fuel: kind === 'ftr' ? s.fuel : SKY.fuel.home, fuel0: kind === 'ftr' ? s.fuel : SKY.fuel.home,
          job: s.wait ? { do: 'wait' } : side === 'att' && kind === 'bmr' ? { do: 'route' } : { do: 'patrol', x: s.x, y: s.y },
          q: kind === 'bmr' ? 1 : this.place.worth[side], bounce: 0, run: 0, dropped: 0, jettisoned: false, shot: 0, owed: 0, back: 0,
          contacts: [], vig: 1, kit: { ftr: 0, bmr: 0 }, kit0: 0, luck: 1, lane: 0, homeX: s.x, homeY: side === 'att' ? -5_000 : s.y,
        };
        this.units.push(u);
      }
    }
    // The raid comes on as one stream: its bomber boxes side by side on one line to the target, the escorts with them.
    const raid = this.units.filter((u) => u.side === 'att');
    if (raid.length) {
      const entry = raid.reduce((s, u) => s + u.x, 0) / raid.length;
      const boxes = raid.filter((u) => u.air === 'bmr');
      boxes.forEach((u, k) => { u.lane = (k - (boxes.length - 1) / 2) * SKY.stream; u.x = entry + u.lane; u.homeX = u.x; });
      raid.filter((u) => u.air === 'ftr').forEach((u, k) => { u.x = entry + (k % 2 ? 1 : -1) * (SKY.escort.close + Math.floor(k / 2) * 600); u.homeX = u.x; });
    }
    this.arm();
    // Both sides start on the staff's plan; the player changes what he likes before or after he starts the clock.
    this.think('att', true);
    this.think('def', true);
    this.start = this.expect();
    for (const u of this.units) this.mor0.set(u.id, u.mor);
    this.through0 = this.throughNow(this.place.forecast.bmr);
  }

  /** How a group of n aircraft is drawn: how many models, one for every 'per' aircraft, in a box of w by d metres. */
  private drawn(kind: 'ftr' | 'bmr', n: number): { figures0: number; figures: number; per: number; w: number; d: number } {
    const L = SKY.look, per = Math.max(1, Math.ceil(n / L.most[kind])), figures = Math.max(1, Math.ceil(n / per));
    const rows = Math.min(L.rows[kind], figures), cols = Math.ceil(figures / rows);
    return { figures0: figures, figures, per, w: cols * L.gap[kind], d: rows * L.gap[kind] };
  }

  /**
   * A group's name in a dispatch: his own bare, as the tray has them; the enemy's with its country word first, as every
   * enemy named on the field (the UI director's ruling 226(7), carried to the air: "French Groupe de Chasse 3 is in a
   * fight."). With nobody's hand on either side, both bare.
   */
  called(u: AirUnit): string {
    const t = this.ticket;
    return t[u.side].human || !(t.att.human || t.def.human) ? u.name : `${t[u.side].adj} ${u.name}`;
  }

  private say(what: FieldEvent['what'], u: AirUnit | null, text: string, extra?: Record<string, unknown>, cue?: string): void {
    const at = extra && typeof extra.x === 'number' ? { x: extra.x as number, y: extra.y as number } : u ? { x: u.x, y: u.y } : {};
    const e: FieldEvent = { t: this.t, what, text, ...(u ? { unit: u.id, side: u.side } : {}), ...at, ...(cue ? { cue } : {}), ...(extra ? { extra } : {}) };
    this.events.push(e);
    this.log.push(e);
  }

  unit(id: number): AirUnit | undefined { return this.units.find((u) => u.id === id); }

  // ---------------------------------------------------------------------------------------------------------------
  // The computer's orders. It plans for its own side every few seconds; for the player's side only once, at the start.

  private nearest(u: AirUnit, pred: (v: AirUnit) => boolean): AirUnit | null {
    let best: AirUnit | null = null, bd = Infinity;
    for (const v of this.units) {
      if (v.side === u.side || off(v) || !flying(v) || !pred(v) || !this.sees(u.side, v)) continue;
      const d = dist(u.x, u.y, v.x, v.y);
      if (d < bd) { bd = d; best = v; }
    }
    return best;
  }

  private valid(u: AirUnit, job: AirJob): boolean {
    if (job.do === 'escort') { const b = this.unit(job.on); return !!b && !off(b) && b.side === u.side; }
    if (job.do === 'intercept') { const e = this.unit(job.on); return !!e && flying(e) && e.side !== u.side; }
    return true;
  }

  private think(side: Side, first = false): void {
    for (const u of this.units) {
      if (u.side !== side || off(u) || u.job.do === 'home' || u.job.do === 'wait' || u.state === 'routing') continue;
      if (u.air === 'bmr') { if (u.job.do !== 'route') u.job = { do: 'route' }; continue; }
      if (side === 'att') {
        if (!first && this.valid(u, u.job) && u.job.do !== 'patrol') continue;
        if (this.place.mission === 'superiority') { u.job = { do: 'hunt' }; continue; }
        // Close escort, shared out over the bomber groups by their size.
        const boxes = this.units.filter((b) => b.side === 'att' && b.air === 'bmr' && !off(b));
        if (!boxes.length) { u.job = { do: 'home' }; continue; }
        const load = (b: AirUnit): number => this.units.filter((f) => f.job.do === 'escort' && f.job.on === b.id).reduce((s, f) => s + f.n, 0) / b.n;
        const b = [...boxes].sort((x, y) => load(x) - load(y) || x.id - y.id)[0];
        u.job = { do: 'escort', on: b.id, close: true };
        u.to = b.band;
      } else {
        // The defenders go for the bombers that have yet to drop; failing those, any raider still coming on.
        const loaded = (v: AirUnit): boolean => !away(v) && v.air === 'bmr' && !v.dropped && !v.jettisoned;
        const cur = u.job.do === 'intercept' ? this.unit(u.job.on) : undefined;
        if (!first && cur && this.valid(u, u.job) && loaded(cur)) continue;
        const want = u.kit.bmr > 0 || u.kit0 <= 0;
        const e = (want ? this.nearest(u, loaded) : null) ?? (cur && this.valid(u, u.job) && !away(cur) ? cur : null)
          ?? this.nearest(u, (v) => !away(v) && (v.air === 'ftr' ? u.kit.ftr > 0 || u.kit0 <= 0 : want));
        if (e) u.job = { do: 'intercept', on: e.id };
        else u.job = { do: 'patrol', x: SKY.target.x, y: SKY.target.y };
        // The staff meets the raid at its own height. Height is the player's to play: a group sent above the enemy dives
        // on it (book.ts edge.bounce); the raid card's odds already weigh radar and a late scramble.
        if (first) u.to = Math.max(u.to, 1) as Band;
      }
    }
  }

  /** Whether a side can see that group: radar shows the raid to the defender; otherwise only what is within sight. */
  sees(side: Side, v: AirUnit): boolean {
    if (v.side === side) return true;
    if (side === 'def' && this.place.radar && !this.place.night) return true;
    const r = this.place.night ? SKY.sight.night : SKY.sight.day;
    if (side === 'def' && dist(v.x, v.y, SKY.target.x, SKY.target.y) <= r) return true;
    return this.units.some((u) => u.side === side && flying(u) && dist(u.x, u.y, v.x, v.y) <= r);
  }

  // ---------------------------------------------------------------------------------------------------------------

  step(): void {
    if (this.over) return;
    const dt = DT * SKY.scale;
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    for (const side of ['att', 'def'] as Side[]) if (this.ai[side] && this.t >= this.thinkAt) this.think(side);
    if (this.t >= this.thinkAt) this.thinkAt = this.t + SKY.think;
    this.scramble();
    for (const u of this.units) if (!off(u)) this.fly(u, dt);
    this.touch();
    this.wear(dt);
    for (const u of this.units) if (!off(u)) this.settle(u, dt);
    this.warn();
    this.end(dt);
  }

  /**
   * How near a side's formations are to breaking off, from 0 (all fresh) to 1 (every one broken off), weighted by the
   * aircraft each sent up (ruling 226, mark 3: no hidden thresholds). A group is as near as the further of two marks:
   * its morale on the way from where it started down to routing, and, for a fighter group, its fight spent on the way
   * down to the share at which it breaks off for home. A group lost, routing or fought out has broken off.
   */
  nearBreak(side: Side): number {
    let w = 0, all = 0;
    for (const u of this.units) {
      if (u.side !== side) continue;
      all += u.n0;
      if (u.state === 'gone' || u.state === 'routing' || (u.air === 'ftr' && u.kit0 > 0 && u.vig < SKY.spent)) { w += u.n0; continue; }
      const m0 = this.mor0.get(u.id) ?? u.mor;
      const byMor = m0 > SKY.morale.routing ? clamp01((m0 - u.mor) / (m0 - SKY.morale.routing)) : 0;
      const byFight = u.air === 'ftr' && u.kit0 > 0 ? clamp01((1 - u.vig) / (1 - SKY.spent)) : 0;
      w += u.n0 * Math.max(byMor, byFight);
    }
    return all > 0 ? w / all : 0;
  }

  /**
   * A side three quarters of the way to breaking off: one line, once (ruling 226, mark 3, in the UI director's words).
   * The player's own reads "Your raid is near breaking off"; the enemy's "The French fighters are near breaking off".
   */
  private warn(): void {
    for (const side of ['att', 'def'] as Side[]) {
      if (this.warned.has(side) || this.nearBreak(side) < SKY.warn) continue;
      this.warned.add(side);
      const raid = side === 'att' && this.units.some((u) => u.side === 'att' && u.air === 'bmr');
      const who = this.ticket[side].human ? `Your ${raid ? 'raid is' : 'fighters are'}` : `The ${this.ticket[side].adj} ${raid ? 'raid is' : 'fighters are'}`;
      this.say('air:near', null, `${who} near breaking off.`, { side });
    }
  }

  /** Defenders with no radar warning wait on the ground until the raid comes near, then climb to meet it. */
  private scramble(): void {
    if (this.scrambled) return;
    const near = this.units.some((u) => u.side === 'att' && flying(u) && dist(u.x, u.y, SKY.target.x, SKY.target.y) <= SKY.scramble);
    if (!near) return;
    this.scrambled = true;
    for (const u of this.units) {
      if (u.job.do !== 'wait') continue;
      u.job = { do: 'patrol', x: SKY.target.x, y: SKY.target.y };
      u.band = 0; u.to = 1; u.alt = SKY.bands.alt[0];
      this.say('air:scramble', u, told(this.called(u), '{N} [is|are] scrambled late and [climbs|climb] to meet the raid.'), undefined, C.scramble);
    }
    this.think('def', true);
  }

  /** Where the job sends a group this tick. */
  private aim(u: AirUnit): [number, number] {
    const T = SKY.target;
    const j = u.job;
    // In a fight, a group free to pick its enemy turns on the one it is fighting; an interceptor keeps after its quarry
    // and a close escort keeps its place by its bombers, fighting whatever comes at them.
    const free = j.do === 'hunt' || j.do === 'patrol' || (j.do === 'escort' && !j.close);
    if (u.air === 'ftr' && u.contacts.length && free) { const e = this.unit(u.contacts[0])!; return [e.x, e.y]; }
    switch (j.do) {
      case 'route': return u.dropped || u.jettisoned ? [u.x, -5_000] : [T.x + u.lane, T.y];
      case 'home': return [u.homeX, u.homeY];
      case 'wait': return [u.x, u.y];
      case 'patrol': {
        const e = this.nearest(u, (v) => dist(v.x, v.y, j.x, j.y) <= SKY.escort.reach);
        return e ? [e.x, e.y] : [j.x, j.y];
      }
      case 'intercept': { const e = this.unit(j.on); return e && this.valid(u, j) ? [e.x, e.y] : [u.x, u.y]; }
      case 'hunt': {
        const e = this.nearest(u, (v) => v.air === 'ftr') ?? this.nearest(u, () => true);
        return e ? [e.x, e.y] : [T.x, T.y];
      }
      case 'escort': {
        const b = this.unit(j.on);
        if (!b || off(b)) return [u.x, u.y];
        if (j.close) {
          // A close escort turns on any enemy fighter that comes at its bombers, and otherwise keeps its place by them.
          const e = this.nearest(u, (v) => v.air === 'ftr' && v.job.do !== 'home' && v.state !== 'routing' && dist(v.x, v.y, b.x, b.y) <= SKY.escort.near);
          if (e) return [e.x, e.y];
          const k = this.units.filter((f) => f.job.do === 'escort' && f.job.on === b.id).indexOf(u);
          const side = k % 2 ? 1 : -1, row = Math.floor(k / 2);
          return [b.x + side * SKY.escort.close * (1 + row * 0.5), b.y - row * 600];
        }
        // A loose escort sweeps ahead of its bombers and goes for enemy fighters that come near them.
        const e = this.nearest(u, (v) => v.air === 'ftr' && dist(v.x, v.y, b.x, b.y) <= SKY.escort.reach);
        if (e) return [e.x, e.y];
        const hd = bearing(b.x, b.y, T.x, T.y);
        return [b.x + Math.sin(hd) * SKY.escort.ahead, b.y + Math.cos(hd) * SKY.escort.ahead];
      }
    }
  }

  private fly(u: AirUnit, dt: number): void {
    if (u.job.do === 'wait') return;
    // Height: climbing takes time, diving little.
    if (u.band !== u.to) {
      u.climbT += dt;
      const up = u.to > u.band, need = up ? SKY.bands.climb : SKY.bands.dive;
      const a0 = SKY.bands.alt[u.band], a1 = SKY.bands.alt[(u.band + (up ? 1 : -1)) as Band];
      u.alt = a0 + (a1 - a0) * Math.min(1, u.climbT / need);
      if (u.climbT >= need) { u.band = (u.band + (up ? 1 : -1)) as Band; u.climbT = 0; u.alt = SKY.bands.alt[u.band]; }
    } else u.alt = SKY.bands.alt[u.band];
    const [tx, ty] = this.aim(u);
    const d = dist(u.x, u.y, tx, ty);
    const climbing = u.to > u.band;
    const v = (u.air === 'bmr' ? SKY.speed.bmr : SKY.speed.ftr) * (climbing ? SKY.speed.climbing : 1) * dt;
    u.speed = 0;
    if (d > 1) {
      u.face = bearing(u.x, u.y, tx, ty);
      const k = Math.min(1, v / d);
      u.x += (tx - u.x) * k; u.y += (ty - u.y) * k;
      u.speed = (d * k) / DT;
    }
    // Fuel: only the raid's fighters are far from home.
    if (u.air === 'ftr' && u.fuel0 < SKY.fuel.home) {
      u.fuel -= dt * (u.contacts.length ? SKY.fuel.fight : 1);
      if (u.fuel <= 0 && u.job.do !== 'home') {
        u.fuel = 0; u.job = { do: 'home' };
        this.say('air:bingo', u, told(this.called(u), '{N} [is|are] short of fuel and [turns|turn] for home.'), undefined, C.bingo);
      }
    }
    // The bombing run.
    if (u.air === 'bmr' && u.side === 'att' && u.job.do === 'route' && !u.dropped && !u.jettisoned) {
      if (dist(u.x, u.y, SKY.target.x, SKY.target.y) <= SKY.target.r) {
        u.run += dt;
        if (u.run >= SKY.run) {
          u.dropped = u.n;
          this.say('air:bombs', u, `${told(this.called(u), '{N} [bombs|bomb]')} ${this.place.name}: ${u.n} bombers over the target.`, { n: u.n }, C.bombs);
        }
      }
    }
  }

  /** Who is in contact with whom: hostile groups close together and no more than one height apart. A dive from above is a bounce. */
  private touch(): void {
    const r = this.place.night ? SKY.contact.night : SKY.contact.day;
    const live = this.units.filter(flying);
    const before = new Map(live.map((u) => [u.id, new Set(u.contacts)]));
    for (const u of live) u.contacts = [];
    for (const a of live) {
      if (a.side !== 'att') continue;
      for (const d of live) {
        if (d.side !== 'def' || d.air !== 'ftr') continue;
        // A group that has broken off or turned for home runs and does not fight back; no new fight starts with it,
        // but an enemy already on it can stay on it as it goes.
        const aw = away(a), dw = away(d);
        if (aw && dw) continue;
        if (aw || dw) {
          const [run, chase] = aw ? [a, d] : [d, a];
          if (!before.get(a.id)?.has(d.id) || chase.kit[run.air] <= SKY.spent * chase.kit0 * 0.25) continue;
        }
        if (Math.abs(a.band - d.band) > 1 || dist(a.x, a.y, d.x, d.y) > r) continue;
        a.contacts.push(d.id); d.contacts.push(a.id);
        this.fired = true;
        if (before.get(a.id)?.has(d.id)) continue;
        // A new contact: the higher group dives on the lower one and strikes first.
        const hi = a.band > d.band ? a : d.band > a.band ? d : null;
        if (hi) {
          const lo = hi === a ? d : a;
          hi.bounce = SKY.edge.bounceFor; hi.band = lo.band; hi.to = lo.band; hi.alt = SKY.bands.alt[lo.band];
          lo.mor = clamp01(lo.mor - SKY.morale.bounced);
          this.say('air:bounce', hi, `${told(this.called(hi), '{N} [dives|dive] on')} ${subjectOf(this.called(lo)).whom} from above.`, { on: lo.id });
        }
        for (const x of [a, d]) if (!(before.get(x.id)?.size)) {
          x.luck = this.rng.range(1 - SKY.wear.luck, 1 + SKY.wear.luck);
          this.say('contact', x, told(this.called(x), '{N} [is|are] in a fight.'), undefined, x.air === 'bmr' ? C.onBombers : C.dogfight);
        }
      }
    }
  }

  /** Where a group stands in the fight: diving from above, or caught climbing. Counts for its blows and against the enemy's. */
  private place_(u: AirUnit): number {
    return (u.bounce > 0 ? SKY.edge.bounce : 1) * (u.to > u.band ? SKY.edge.climbing : 1);
  }

  /** How hard a group hits: where it stands, its nerve, and its luck. */
  private edge(u: AirUnit): number {
    return this.place_(u) * (u.state === 'wavering' ? SKY.edge.wavering : 1) * u.luck;
  }

  /**
   * What each group can do to the enemy over the whole battle, from the raid card's own forecast: the defending
   * fighters share out the escort and bomber losses the card expects, the escorts the defending fighters it expects
   * to fall, each by its weight in the fight (aircraft x worth; an escort by how long its fuel lets it stay). A
   * group spends this while in contact and the enemies it touches pay it, so the battle's sums are the card's. A
   * group breaks off once all but SKY.spent of it is gone, so it carries that much more than its share.
   */
  private arm(): void {
    const P = this.place, F = P.forecast;
    const att = this.units.filter((u) => u.side === 'att' && u.air === 'ftr'), def = this.units.filter((u) => u.side === 'def' && u.air === 'ftr');
    const A = att.reduce((s, u) => s + this.weight(u, u.n0), 0), D = def.reduce((s, u) => s + this.weight(u, u.n0), 0);
    const B = this.units.filter((u) => u.side === 'att' && u.air === 'bmr').reduce((s, u) => s + u.n0, 0);
    const full = 1 / (1 - SKY.spent);
    // The guns take their own share of the bombers; the fighters the rest.
    const onBombers = Math.max(0, F.bmr - B * P.gunLoss);
    for (const u of att) { const k = A > 0 ? this.weight(u, u.n0) / A : 0; u.kit = { ftr: F.foe * k * full, bmr: 0 }; }
    for (const u of def) { const k = D > 0 ? this.weight(u, u.n0) / D : 0; u.kit = { ftr: F.ftr * k * full, bmr: onBombers * k * full }; }
    for (const u of this.units) u.kit0 = u.kit.ftr + u.kit.bmr;
    const met = D / Math.max(1e-9, D + A + B * P.bomberDefence);
    this.law = { D, B, ePer: B > 0 ? A / B : 0, guns: P.bomberDefence, met };
  }

  /** A fighter group's weight in the fight: aircraft x worth, and for the raid's escorts, how long its fuel lets it stay. */
  private weight(u: AirUnit, n = u.n): number {
    return n * u.q * (u.side === 'att' ? Math.min(1, u.fuel0 / SKY.fuel.full) : 1);
  }

  /**
   * How much harder than the raid card expects a bomber box is hit, for the escort it has with it now: the campaign's
   * own law (bombers lost go as the defenders' share of the fighting weight, to the power L.power), with this box's
   * escort per bomber in place of the raid's. A box with its fair share of the escort is hit as the card says; one
   * left alone, harder; one with more, less.
   */
  private cover(b: AirUnit, live: AirUnit[]): number {
    const L = this.law, P = this.place;
    if (L.met <= 0) return 1;
    const by = (f: AirUnit): boolean => f.side === 'att' && f.job.do !== 'home' && Math.abs(f.band - b.band) <= 1 && dist(f.x, f.y, b.x, b.y) <= SKY.escort.near;
    // The escort per bomber round this box: the fighters near it over the bombers near it (boxes in a stream share theirs).
    const E = live.filter((f) => f.air === 'ftr' && by(f)).reduce((s, f) => s + this.weight(f), 0);
    const per = E / Math.max(1, live.filter((f) => f.air === 'bmr' && by(f)).reduce((s, f) => s + f.n, 0));
    const met = L.D / Math.max(1e-9, L.D + L.B * (per + L.guns));
    return Math.max(SKY.escort.cover[0], Math.min(SKY.escort.cover[1], Math.pow(met / L.met, P.law.power)));
  }

  /** The wear: what contact and the guns cost each group this tick, owed until a whole aircraft is lost. */
  private wear(dt: number): void {
    const P = this.place;
    const live = this.units.filter(flying);
    const byId = new Map(live.map((u) => [u.id, u]));
    const foes = (u: AirUnit, k: 'ftr' | 'bmr'): AirUnit[] => u.contacts.map((i) => byId.get(i)!).filter((v) => v && v.air === k);
    const sum = (xs: AirUnit[]): number => xs.reduce((s, v) => s + v.n, 0);
    const burn = 1 - Math.exp(-dt / SKY.tire);
    const covers = new Map<number, number>();
    for (const u of live) {
      if (u.air !== 'ftr' || !u.contacts.length || u.kit0 <= 0 || away(u)) continue;
      const F = foes(u, 'ftr'), Bs = foes(u, 'bmr');
      // On fighters: the more for coming down from above, the less for climbing or wavering.
      if (F.length && u.kit.ftr > 0) {
        const spend = u.kit.ftr * burn;
        u.kit.ftr -= spend;
        const nF = sum(F);
        for (const v of F) v.owed += spend * (v.n / Math.max(1, nF)) * Math.min(2, this.edge(u) / this.place_(v));
      }
      // On bombers: the same, and harder on a box with less than its share of the escort by it.
      if (Bs.length && u.kit.bmr > 0) {
        const spend = u.kit.bmr * burn;
        u.kit.bmr -= spend;
        const nB = sum(Bs);
        for (const b of Bs) {
          const most = P.law.most * b.n0 - (b.n0 - b.n) - b.owed;
          if (most <= 0) continue;
          const cover = covers.get(b.id) ?? this.cover(b, live);
          covers.set(b.id, cover);
          b.owed += Math.min(most, spend * (b.n / Math.max(1, nB)) * cover * Math.min(2, this.edge(u)));
          if (cover > 1.2) b.mor = clamp01(b.mor - SKY.morale.alone * dt);
        }
      }
      u.vig = u.kit0 > 0 ? (u.kit.ftr + u.kit.bmr) / u.kit0 : 1;
      if (u.vig < SKY.spent && u.job.do !== 'home') {
        u.job = { do: 'home' };
        this.say('air:spent', u, told(this.called(u), '{N} [has|have] fought [itself|themselves] out and [breaks|break] off for home.'));
      }
    }
    // The guns round the target.
    if (P.gunLoss > 0) for (const u of live) {
      if (u.side !== 'att' || dist(u.x, u.y, SKY.target.x, SKY.target.y) > SKY.flakR) continue;
      if (u.air === 'bmr') {
        if (u.dropped || u.jettisoned) continue;
        u.owed += (u.n * P.gunLoss * SKY.flak.band[u.band] * dt) / this.flakIn;
        u.shot = Math.min(1, u.shot + dt / this.flakIn);
      } else if (SKY.flak.fighters > 0) u.owed += (u.n * P.gunLoss * SKY.flak.fighters * SKY.flak.band[u.band] * dt) / this.flakT;
    }
    // Whole aircraft fall.
    this.fall(live);
  }

  /**
   * Whole aircraft fall from what each group was owed this tick, each with how it goes down. A share of an aircraft
   * falls by chance, so a fight's losses scatter round what it owed as a run of raids scatters (ruling 226, mark 4:
   * the obvious order wins about as often as the raid card says, not every time the expected count tips its way).
   */
  private fall(live: AirUnit[]): void {
    const P = this.place;
    for (const u of live) {
      let k = u.owed;
      u.owed = 0;
      for (; k > 0 && u.n > 0; k -= 1) {
        if (k < 1 && this.lossRng.next() >= k) break;
        u.n--;
        u.mor = clamp01(u.mor - SKY.morale.perLoss / u.n0);
        const over = u.side === 'att' ? 'enemy' : 'own';
        const fall = drawFall(this.fallRng, u.air, over, P.saved[u.side]);
        if (fall.back) u.back++;
        this.say('air:lost', u, fallWords(this.called(u), fall), { ...fall, air: u.air, alt: u.alt, ...(fall.chutes ? { chuteCue: C.chutes } : {}) }, fall.kind === 'explode' ? C.explode : C.down);
        u.figures = Math.max(u.n > 0 ? 1 : 0, Math.ceil(u.n / u.per));
      }
    }
  }

  /** Morale, leaving the board, and the end of a group. */
  private settle(u: AirUnit, dt: number): void {
    if (u.bounce > 0) u.bounce = Math.max(0, u.bounce - dt);
    if (u.n <= 0) { this.state(u, 'gone', told(this.called(u), '{N} [is|are] wiped out.')); return; }
    u.str = u.n / u.n0;
    if (!u.contacts.length) u.mor = Math.min(SKY.morale.base + SKY.morale.ready, u.mor + SKY.morale.mend * dt);
    const was = u.state;
    if (u.mor < SKY.morale.routing && was !== 'routing') {
      // One notice for one event (the UI director, 18:28Z): the break says the bombs went too.
      const dump = u.air === 'bmr' && !u.dropped && !u.jettisoned;
      if (dump) u.jettisoned = true;
      this.state(u, 'routing', dump ? told(this.called(u), '{N} [breaks|break]: [it jettisons its|they jettison their] bombs and [turns|turn] for home.') : told(this.called(u), '{N} [breaks|break] off and [heads|head] for home.'));
      u.job = { do: 'home' };
    } else if (was !== 'routing' && u.mor < SKY.morale.wavering && was !== 'wavering') this.state(u, 'wavering', told(this.called(u), '{N} [is|are] wavering.'));
    else if (was === 'wavering' && u.mor >= SKY.morale.wavering + 0.05) this.state(u, 'steady', told(this.called(u), '{N} [steadies|steady].'), 'rallied');
    // For the models: what it is doing, and whom it is fighting.
    u.engaged = u.contacts[0];
    u.act = u.job.do === 'wait' ? 'idle' : u.state === 'routing' ? 'rout' : u.contacts.length ? 'fight' : 'march';
    // Off the board: a raider past its own edge, a defender down at its airfield.
    if (u.job.do === 'home' || (u.job.do === 'route' && (u.dropped || u.jettisoned))) {
      const gone = u.side === 'att' ? u.y <= 0 : dist(u.x, u.y, u.homeX, u.homeY) <= 1_500;
      if (gone) this.state(u, 'withdrawn', u.side === 'att' ? told(this.called(u), '{N} [is|are] clear of the fight and [heads|head] for home.') : told(this.called(u), '{N} [lands|land].'));
    }
  }

  private state(u: AirUnit, s: UnitState, text: string, what?: FieldEvent['what']): void {
    if (u.state === s) return;
    u.state = s;
    if (s === 'gone' || s === 'withdrawn') { u.act = 'down'; u.speed = 0; u.engaged = undefined; u.contacts = []; if (s === 'gone') u.figures = 0; }
    this.say(what ?? (s === 'steady' ? 'rallied' : s), u, text);
  }

  /** Every bomber has bombed or turned back (with no bombers, every raider is on its way home), and nobody is in a fight. */
  winding(): boolean {
    const att = this.units.filter((u) => u.side === 'att');
    const fighting = this.units.some((u) => flying(u) && u.contacts.length > 0);
    const done = (u: AirUnit): boolean => off(u) || u.job.do === 'home' || u.state === 'routing' || (u.air === 'bmr' && (u.dropped > 0 || u.jettisoned));
    const bombers = att.filter((u) => u.air === 'bmr');
    return !fighting && (bombers.length ? bombers.every(done) : att.every(done));
  }

  private end(dt: number): void {
    const att = this.units.filter((u) => u.side === 'att');
    if (att.every(off)) return this.finish('rout');
    if (this.gaveUp) return this.finish('withdrew');
    if (this.t >= this.ticket.limit) return this.finish('time');
    this.quietFor = this.winding() ? this.quietFor + dt : 0;
    if (this.quietFor >= SKY.quiet) this.finish('objective');
  }

  // ---------------------------------------------------------------------------------------------------------------

  command(c: Command, by: Side): boolean {
    if (this.over) return false;
    const mine = (ids?: number[]): AirUnit[] => (ids ?? []).map((i) => this.unit(i)!).filter((u) => u && u.side === by && !off(u) && u.state !== 'routing');
    const fighters = (ids?: number[]): AirUnit[] => mine(ids).filter((u) => u.air === 'ftr' && u.job.do !== 'wait');
    switch (c.do) {
      case 'auto': this.ai[c.side] = c.on; if (c.on) this.think(c.side, true); return true;
      // Hand it to the staff (ruling 217(4), the design director 18:27Z): the rest is settled at once from where each
      // side stands, and what has been fought stays fought. The rules then hold it inside the dice's run as always.
      case 'dice': this.settleNow(); return true;
      case 'withdrawAll': {
        if (c.side !== by) return false;
        this.gaveUp = by;
        for (const u of this.units) if (u.side === by && !off(u)) this.goHome(u);
        this.end(0);
        return true;
      }
      case 'withdraw': { const us = mine(c.units); us.forEach((u) => this.goHome(u)); return us.length > 0; }
      case 'move': case 'hold': {
        const us = fighters(c.units);
        for (const u of us) u.job = c.do === 'move' ? { do: 'patrol', x: c.x, y: c.y } : { do: 'patrol', x: u.x, y: u.y };
        return us.length > 0;
      }
      case 'attack': {
        const e = this.unit(c.target);
        if (!e || e.side === by || !flying(e)) return false;
        const us = fighters(c.units);
        for (const u of us) u.job = { do: 'intercept', on: e.id };
        return us.length > 0;
      }
      case 'air:escort': {
        const b = this.unit(Number(c.target));
        if (!b || b.side !== by || b.air !== 'bmr' || off(b)) return false;
        const us = fighters(c.units);
        for (const u of us) { u.job = { do: 'escort', on: b.id, close: c.close !== false }; u.to = c.close !== false ? b.band : u.to; }
        return us.length > 0;
      }
      case 'air:hunt': { const us = fighters(c.units); us.forEach((u) => { u.job = { do: 'hunt' }; }); return us.length > 0; }
      case 'air:band': {
        const b = Number(c.band);
        if (b !== 0 && b !== 1 && b !== 2) return false;
        const us = mine(c.units).filter((u) => u.job.do !== 'wait');
        us.forEach((u) => { u.to = b as Band; u.climbT = 0; });
        return us.length > 0;
      }
      case 'air:jettison': {
        const us = mine(c.units).filter((u) => u.air === 'bmr' && !u.dropped && !u.jettisoned);
        for (const u of us) { u.jettisoned = true; u.job = { do: 'home' }; this.say('air:jettison', u, told(this.called(u), '{N} [jettisons its|jettison their] bombs and [turns|turn] for home.'), { n: u.n }); }
        return us.length > 0;
      }
      default: return false;
    }
  }

  private goHome(u: AirUnit): void {
    if (u.air === 'bmr' && !u.dropped && !u.jettisoned && u.side === 'att') { u.jettisoned = true; this.say('air:jettison', u, told(this.called(u), '{N} [jettisons its|jettison their] bombs and [turns|turn] for home.'), { n: u.n }); }
    u.job = { do: 'home' };
  }

  /** Each side's fighting strength still in the air, as a share of what both sent up (the view's balance). */
  balance(): Record<Side, number> {
    const power = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n * u.q, 0);
    const p0 = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n0 * u.q, 0);
    const all = p0('att') + p0('def') || 1;
    return { att: power('att') / all, def: power('def') / all };
  }

  view(as?: Side): BattleView<AirUnit> {
    const units = this.units.map((u) => ({ ...u, spotted: { att: this.sees('att', u), def: this.sees('def', u) } }))
      .filter((u) => !as || u.side === as || u.spotted[as]);
    const events = this.events;
    this.events = [];
    return { t: this.t, limit: this.ticket.limit, units, balance: this.balance(), events, zones: this.zones, ai: { ...this.ai }, over: this.over };
  }

  /** Every event so far, for the after-action card and replays. */
  history(): FieldEvent[] { return [...this.log]; }

  // ---------------------------------------------------------------------------------------------------------------

  /**
   * The rest of the raid, settled at once from where each side stands (ruling 217(4)). Each fighter group still in the
   * fight deals what it has left before it would break off, at the strength it has now, to the enemies still in play:
   * a group the player drove off deals nothing more, a battered one less, and a box left without its escort pays for
   * it, as the board would have made it pay. Bombers still coming on get through; those that turned back stay turned.
   */
  /**
   * What the rest of the raid would still cost each pool from where each side stands, at even luck: the defending
   * fighters ('foe'), the raid's fighters ('ftr') and its bombers ('bmr'). Each fighter group's fight left, at its
   * strength now, against the enemy still in play, and the guns' run each box has still to fly. Groups on the ground
   * count: they would come up.
   */
  private expect(): { foe: number; ftr: number; bmr: number } {
    const live = this.units.filter((u) => !off(u));
    const inPlay = (v: AirUnit): boolean => !away(v);
    const out = { foe: 0, ftr: 0, bmr: 0 };
    const took = new Map<number, number>();
    for (const u of live) {
      if (u.air !== 'ftr' || away(u) || u.kit0 <= 0) continue;
      const vig = (u.kit.ftr + u.kit.bmr) / u.kit0;
      if (vig <= SKY.spent) continue;
      const k = ((vig - SKY.spent) / vig) * (u.n / u.n0) * (u.state === 'wavering' ? SKY.edge.wavering : 1);
      const F = live.filter((v) => v.side !== u.side && v.air === 'ftr' && inPlay(v)), Bs = live.filter((v) => v.side !== u.side && v.air === 'bmr' && inPlay(v));
      if (F.length) out[u.side === 'att' ? 'foe' : 'ftr'] += u.kit.ftr * k;
      const nB = Bs.reduce((s, v) => s + v.n, 0);
      for (const b of Bs) {
        const most = this.place.law.most * b.n0 - (b.n0 - b.n) - (took.get(b.id) ?? 0);
        const hit = most > 0 ? Math.min(most, u.kit.bmr * k * (b.n / Math.max(1, nB)) * this.cover(b, live)) : 0;
        took.set(b.id, (took.get(b.id) ?? 0) + hit);
        out.bmr += hit;
      }
    }
    for (const b of live) if (b.air === 'bmr' && b.side === 'att' && inPlay(b)) out.bmr += b.n * this.place.gunLoss * SKY.flak.band[b.band] * (1 - b.shot);
    return out;
  }

  /**
   * HAND IT TO THE STAFF: what was fought stays fought, and the rest goes to the rules' own dice (the design director,
   * log b368). The battle ends here and hands back what each group had lost, with each pool's share of the raid's cost
   * still to come from where each side stands (the staff's reckoning now against the same reckoning at the start).
   * The campaign rolls the raid exactly as it does for any raid and books that share of its roll on top. Pressed at
   * once, the share is 1 and nothing is lost yet: it books exactly what the campaign would have rolled.
   */
  private settleNow(): void {
    if (this.over) return;
    const lost = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    this.before = { att: { ftr: lost('att', 'ftr'), bmr: lost('att', 'bmr') }, def: { ftr: lost('def', 'ftr'), bmr: 0 } };
    this.staff = { ...this.restNow(), chance: this.staffChance(), fired: this.fired };
    this.toStaff = true;
    this.finish('objective');
  }

  /**
   * What the staff would settle from where the battle stands: each pool's share of the raid's cost still to come (the
   * staff's reckoning now against the same reckoning at the start), and the bombers in the air that had turned back
   * without bombing, which do not bomb when the staff settle it.
   */
  restNow(): NonNullable<AirOutcome['staff']> {
    const now = this.expect(), S = this.start;
    // A pool the reckoning never weighed (no escort, no bombers) is left to the dice whole: they give it what they give it.
    const share = (k: 'foe' | 'ftr' | 'bmr'): number => (S[k] > 0 ? Math.min(2, now[k] / S[k]) : 1);
    const turned = this.units.filter((b) => b.air === 'bmr' && b.side === 'att' && b.n > 0 && !b.dropped && (b.jettisoned || away(b)));
    return { rest: { foe: share('foe'), ftr: share('ftr'), bmr: share('bmr') }, away: turned.reduce((t, b) => t + b.n, 0) };
  }

  /** The bombers the staff would expect over the target from here: those that have bombed, and those still on their way less the bombers still to fall. */
  private throughNow(bmrToCome: number): number {
    const bs = this.units.filter((u) => u.side === 'att' && u.air === 'bmr');
    const done = bs.reduce((t, b) => t + b.dropped, 0);
    const coming = bs.filter((b) => !off(b) && !b.dropped && !b.jettisoned && !away(b)).reduce((t, b) => t + b.n, 0);
    return done + Math.max(0, coming - bmrToCome);
  }

  /**
   * The raider's chance of success if the staff settled the rest now (ruling 226, mark 5: HAND IT TO THE STAFF says
   * the staff's chance now), weighed as forecastAir weighs the raid card's: the aircraft each side has lost and is
   * still expected to lose, and what the bombers still getting through should achieve. Before anything has happened
   * it is the raid card's own chance.
   */
  staffChance(): number {
    const P = this.place, R = P.reckon, F = P.forecast, { rest } = this.restNow();
    const lost = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    const foe = lost('def', 'ftr') + rest.foe * F.foe, ftr = lost('att', 'ftr') + rest.ftr * F.ftr, bmrToCome = rest.bmr * F.bmr;
    const bmr = lost('att', 'bmr') + bmrToCome;
    const share = this.through0 > 0 ? Math.min(1.5, this.throughNow(bmrToCome) / this.through0) : 1;
    const pr = Math.min(1, R.rail * share), pf = Math.min(1, R.ruin * share), value = R.value * share;
    const beats = (worth: number): number => { const x = Math.pow((foe + worth + 10) / (ftr + bmr + 10), R.k); return x / (1 + x); };
    return (1 - pr) * (1 - pf) * beats(value) + pr * (1 - pf) * beats(value + 300) + (1 - pr) * pf * beats(value + 300) + pr * pf * beats(value + 600);
  }

  outcome(): AirOutcome {
    const groups = new Map<number, AirOutcome['groups'][number]>();
    for (const u of this.units) {
      const row = groups.get(u.corps) ?? { id: u.corps, side: u.side, lostFtr: 0, lostBmr: 0, back: 0, flewFtr: 0, flewBmr: 0 };
      if (u.air === 'ftr') { row.lostFtr += u.n0 - u.n; row.flewFtr += u.n0; } else { row.lostBmr += u.n0 - u.n; row.flewBmr += u.n0; }
      row.back += u.back;
      groups.set(u.corps, row);
    }
    const att = this.units.filter((u) => u.side === 'att' && u.air === 'bmr');
    // A box that bombed lost u.n0 - u.dropped on the way in and u.dropped - u.n after; one that never bombed lost all its losses on the way.
    const after = att.reduce((s, u) => s + (u.dropped ? u.dropped - u.n : 0), 0);
    return {
      groups: [...groups.values()], chance: this.place.chance,
      dropped: att.reduce((s, u) => s + u.dropped, 0), jettisoned: att.reduce((s, u) => s + (u.jettisoned ? u.n : 0), 0),
      lostBefore: att.reduce((s, u) => s + u.n0 - u.n, 0) - after, lostAfter: after,
      ...(this.gaveUp ? { gaveUp: this.gaveUp } : {}),
      ...(this.staff ? { staff: this.staff } : {}),
    };
  }

  /**
   * The field's verdict by the campaign's own measure (fly(): the aircraft the defender lost, and what the bombs were
   * worth, against the aircraft the raider lost, each with 10 added). What the bombs broke beyond the output they
   * stopped (a railway cut, a factory wrecked) is the campaign's to roll when it books the raid, so this is the field's
   * verdict before it: fly() judges it again from the field's own losses with what the bombs did, and the after card
   * says that (the design director, b384(a); ruling 226 mark 6). A side that broke off everything lost; where its
   * losses say otherwise, it is a close defeat. As the raider reads it, on the campaign's air scale.
   */
  private judge(): { won: boolean; level: VictoryLevel; ratio: number } {
    const R = this.place.reckon;
    const count = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    const dropped = this.units.filter((u) => u.side === 'att' && u.air === 'bmr').reduce((t, u) => t + u.dropped, 0);
    const worth = this.place.mission === 'superiority' || this.through0 <= 0 ? 0 : R.value * Math.min(1.5, dropped / this.through0);
    const ratio = (count('def', 'ftr') + count('def', 'bmr') + worth + 10) / (count('att', 'ftr') + count('att', 'bmr') + 10);
    const won = this.gaveUp ? this.gaveUp === 'def' : ratio >= 1;
    const level: VictoryLevel = won ? (ratio >= 2 ? 'Decisive victory' : 'Close victory') : ratio < 0.5 ? 'Decisive defeat' : 'Close defeat';
    return { won, level, ratio };
  }

  private finish(how: FieldResult['how']): void {
    if (this.over) return;
    const out = this.outcome();
    const p0 = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + u.n0 * u.q, 0) || 1;
    const pl = (s: Side): number => this.units.filter((u) => u.side === s).reduce((t, u) => t + (u.n0 - u.n) * u.q, 0);
    const lost = { att: pl('att') / p0('att'), def: pl('def') / p0('def') };
    const bombers0 = this.units.filter((u) => u.side === 'att' && u.air === 'bmr').reduce((s, u) => s + u.n0, 0);
    // The verdict by the campaign's own measure. Handed to the staff, the field has none of its own: the staff's dice
    // settle it, and the after card says what they made of it.
    const v = this.judge();
    const winner: Side = v.won ? 'att' : 'def';
    const R = fieldRatio(winner, lost);
    const count = (s: Side, k: 'ftr' | 'bmr'): number => this.units.filter((u) => u.side === s && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
    // The after card in the UI director's words (airfought.ts afterWords). Handed to the staff, the battle does not
    // know the bill (the campaign's dice settle the rest), so its own card leaves the bill and the bombers line out;
    // the campaign's raid order writes the whole card from what it books (airpages.ts airAct).
    const me: Side = this.ticket.def.human && !this.ticket.att.human ? 'def' : 'att';
    const bill = (s: Side) => ({ ftr: count(s, 'ftr'), bmr: count(s, 'bmr') });
    const lines = afterWords({
      place: this.place.name, sky: this.place.mission === 'superiority', me, adj: { att: this.ticket.att.adj, def: this.ticket.def.adj },
      bombers: this.toStaff ? 0 : bombers0, dropped: out.dropped, jettisoned: Math.max(0, bombers0 - out.dropped - out.lostBefore), lostBefore: out.lostBefore, staff: this.toStaff,
      bill: this.toStaff ? null : { att: bill('att'), def: bill('def') }, before: this.before,
      ...(this.staff ? { how: staffHow(me, this.staff, this.place.chance) } : {}),
    });
    this.over = {
      winner, how, seconds: this.t, lost,
      men: { att: count('att', 'ftr') + count('att', 'bmr'), def: count('def', 'ftr') },
      R, level: v.level, lines,
      forces: out.groups.map((gr) => {
        const us = this.units.filter((u) => u.corps === gr.id);
        const n0 = us.reduce((s, u) => s + u.n0, 0) || 1;
        return { corps: gr.id, side: gr.side, lost: us.reduce((s, u) => s + u.n0 - u.n, 0) / n0 };
      }),
      extra: { air: out },
    };
    // The end is told without the bill: the campaign books it (the after card, airfought.ts), and a dispatch with the
    // battle's own count beside the card's would read as a second bill (the design director, 18:39Z).
    this.say('end', null, `${this.ticket.title} is over.`);
  }
}

/** A battle from a ticket. */
export const airBattle = (t: FieldTicket<AirPlace>): AirSim => new AirSim(t);
