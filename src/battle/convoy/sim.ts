// A wolf pack's night against a convoy, fought out on the field (release 2; ruling 241 D2, the design director's rider
// of 24 November 12:34Z under Units decide; Brian's asks of 20 November 23:29Z and 23 November 06:36Z: wolf packs,
// destroyers chasing them with depth charges, boats forced up that fight with the deck gun, dive again or are given up).
//
// The convoy steams north in columns through one night, sunset to sunrise. Its escorts keep a screen around it. The
// boats come up through the night, homed from ashore or alone as their navy's habits are, close on the surface in the
// dark, fire a salvo into the columns and turn away to reload and come again. An escort that finds a boat (sea-search
// radar or its lookouts on the surface, sonar below) runs her down, drives her under and attacks with patterns of
// depth charges or ahead-throwing weapons until she is killed, slips away or the escort must go back to the screen.
// A boat hurt enough is forced up: her crew fight with the deck gun, dive again when they can, or abandon and scuttle
// her. Every boat, escort and merchant lost is counted for the campaign by its fleet.
//
// Abstract by design (ruling 217(1)): contacts, hits and kills are rolls on the battle's own stream at convoy.json's
// rates, and a kill's chance is the card's own (convoycause.json through navy.ts incidentRate), by the hunter's kit.
// Nothing is aimed; the courses, depths and looks act it out. The field's clock is worked once a battle second; the
// screens step it by the core's DT. Same ticket, same orders, same night.

import { Rng } from '../../core/rng';
import { bearing, clamp, dist, turn, DT, TAU } from '../core';
import type { BattleSim, BattleView, Command, FieldEvent, FieldResult, Side, Zone } from '../core/types';
import { attackKills, CONVOY, gunMmOf } from './numbers';
import type { BoatDoing, ConvoyClass, ConvoyCommand, ConvoyCounts, ConvoyFleet, ConvoyKit, ConvoyTicket, ConvoyUnit, Dive, Echo, EscortDoing } from './types';

const B = CONVOY.boat, E = CONVOY.escort, D = CONVOY.detect, T = CONVOY.torpedo, H = CONVOY.hunt, S = CONVOY.surface;
/** A number from 0 to under 1 fixed by a whole number: looks only, never the battle's dice (as the fleet action's mix). */
export const mix = (n: number): number => { let h = n >>> 0; h ^= h >>> 16; h = Math.imul(h, 0x85ebca6b); h ^= h >>> 13; h = Math.imul(h, 0xc2b2ae35); h ^= h >>> 16; return (h >>> 0) / 4294967296; };
const chanceIn = (mean: number, dt: number): number => 1 - Math.exp(-dt / Math.max(1e-6, mean));

interface Boat {
  u: ConvoyUnit; fleet: number; nat: string;
  doing: BoatDoing; arrive: number; from: number; homed: boolean;
  dive: Dive; keel: number; keelTo: number; under: number;
  tubes: number; load: number; reloadAt: number;
  harm: number; doomed: boolean; repairAt: number; scuttleAt: number; struck: number; nextGun: number; gunOn: number;
  side: number; ahead: number; until: number; lastHeard: number;
  sees: Set<number>; order: 'attack' | 'under' | 'dive' | 'deep' | 'surface' | 'evade' | 'gun' | 'leave' | null; target: number;
}
interface Escort {
  u: ConvoyUnit; fleet: number; nat: string; kit: ConvoyKit;
  doing: EscortDoing; sx: number; sy: number;
  holds: number; echo: Echo | null; seenAt: number; huntFrom: number; dx: number; dy: number;
  openTo: number; nextGun: number; nextShell: number; harm: number;
  order: 'hunt' | 'screen' | 'rescue' | 'move' | null; target: number; tx: number; ty: number; until: number;
  attacks: number;
}
interface Merchant { u: ConvoyUnit; dx: number; dy: number; armed: boolean; nextGun: number; lost: boolean }
interface Torp { by: number; at: number; aims: number[]; escort: boolean; x: number; y: number }
interface Burst { at: number; by: number; on: number; x: number; y: number; ahead: boolean; group: boolean; kit: ConvoyKit }
interface Later { at: number; run: () => void }

const live = (u: ConvoyUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

export class ConvoySim implements BattleSim<ConvoyUnit> {
  readonly domain = 'sea' as const;
  readonly ticket: ConvoyTicket;
  t = 0;
  units: ConvoyUnit[] = [];
  over: FieldResult | null = null;
  ai: Record<Side, boolean>;
  /** The night's length in battle seconds: dawn ends it. */
  readonly limit: number;
  /** What the night has cost each side so far, by fleet: what the campaign books (calibrate.ts). */
  readonly counts: ConvoyCounts = { ss: {}, dd: {}, mv: 0 };
  /** Measured only, for the tables (nothing reads them back): attacks made on boats, incidents begun, salvoes fired, torpedoes that hit. */
  readonly tally = { attacks: 0, hunts: 0, salvoes: 0, hits: 0, forcedUp: 0, escaped: 0, scuttled: 0, rammed: 0, shelled: 0, crushed: 0, captured: 0 };

  private readonly rng: Rng;
  private sec = 0;
  private cx = 30000;
  private cy = 20000;
  private readonly halfW: number;
  private readonly halfL: number;
  private readonly boats: Boat[] = [];
  private readonly escorts: Escort[] = [];
  private readonly merchants: Merchant[] = [];
  private readonly byId = new Map<number, ConvoyUnit>();
  private readonly boatOf = new Map<number, Boat>();
  private readonly escortOf = new Map<number, Escort>();
  private torps: Torp[] = [];
  private bursts: Burst[] = [];
  private later: Later[] = [];
  private events: FieldEvent[] = [];
  private zones: Zone[] = [];
  private zoneId = 0;
  private contactAt: number | null = null;
  /** The boats each side's pieces know where they are now (the player sees only his own side's contacts, ruling 241 Q6). */
  private readonly known: Record<Side, Set<number>> = { att: new Set(), def: new Set() };
  private handed = false;
  /** The rest of the night as the staff would settle it, from the computer's own nights (calibrate.ts), for 'dice'. */
  rest: ((sec: number) => ConvoyCounts) | null = null;

  constructor(ticket: ConvoyTicket) {
    this.ticket = ticket;
    this.rng = new Rng(ticket.seed);
    this.limit = Math.round(ticket.place.light.hours * 3600);
    this.ai = { att: !ticket.att.human, def: !ticket.def.human };
    const p = ticket.place;
    // The convoy in columns, ships one hull each.
    const n = Math.max(1, Math.min(CONVOY.convoy.most, p.ships));
    const cols = Math.max(1, Math.ceil(n / CONVOY.convoy.perColumn)), rows = Math.ceil(n / cols);
    this.halfW = ((cols - 1) / 2) * CONVOY.convoy.columnGap;
    this.halfL = ((rows - 1) / 2) * CONVOY.convoy.shipGap;
    let id = 1;
    const defNat = ticket.def.nat;
    for (let k = 0; k < n; k++) {
      const c = k % cols, r = Math.floor(k / cols);
      const dx = (c - (cols - 1) / 2) * CONVOY.convoy.columnGap, dy = ((rows - 1) / 2 - r) * CONVOY.convoy.shipGap;
      const u = this.unit(id++, 'def', 'mv', `Merchant ${k + 1}`, defNat, 0, 'Convoy', 1, this.cx + dx, this.cy + dy);
      u.extra.column = c;
      this.merchants.push({ u, dx, dy, armed: mix(ticket.seed ^ (k * 7919)) < S.merchantGuns, nextGun: 0, lost: false });
    }
    // The escorts: one piece a ship up to sixteen, several to a piece in a bigger guard (the card says how many).
    const escortFleets = p.fleets.filter((f) => f.side === 'def' && f.n > 0);
    const ships = escortFleets.reduce((s, f) => s + f.n, 0);
    const pieces = Math.min(16, ships), per = Math.max(1, Math.ceil(ships / Math.max(1, pieces)));
    const slots = this.shares(escortFleets, pieces, per);
    let k = 0;
    for (const [f, count] of slots) {
      for (let j = 0; j < count.length; j++) {
        const th = (2 * Math.PI * k) / Math.max(1, pieces);
        const sx = Math.sin(th) * (this.halfW + E.station), sy = Math.cos(th) * (this.halfL + E.station);
        const u = this.unit(id++, 'def', 'dd', `Escort ${k + 1}`, f.nat, f.id, f.name, count[j], this.cx + sx, this.cy + sy);
        const e: Escort = { u, fleet: f.id, nat: f.nat, kit: p.kit[f.nat] ?? { asdic: true, radar: false, hedgehog: false, groups: false }, doing: 'screen', sx, sy,
          holds: -1, echo: null, seenAt: -1, huntFrom: -1, dx: 0, dy: 0, openTo: 0, nextGun: 0, nextShell: 0, harm: 0, order: null, target: -1, tx: 0, ty: 0, until: 0, attacks: 0 };
        this.escorts.push(e);
        this.escortOf.set(u.id, e);
        k++;
      }
    }
    // The boats: one a piece, up to twenty, shared over the raiders by their boats.
    const raiders = p.fleets.filter((f) => f.side === 'att' && f.n > 0);
    let b = 0;
    for (const f of raiders) {
      const habits = p.habits[f.nat] ?? { home: false, warships: false };
      for (let j = 0; j < f.n && b < 20; j++, b++) {
        const homed = habits.home;
        // When she comes up and from where: homed boats in the night's first half (the first, the shadower, at once);
        // a boat alone finds the convoy at the 'alone' share, at any time in the night's first 85%.
        const finds = homed || this.rng.chance(CONVOY.arrive.alone);
        const arrive = !finds ? Infinity : homed ? (b === 0 ? 0 : this.rng.range(0, CONVOY.arrive.home) * this.limit) : this.rng.range(0, 0.85) * this.limit;
        const from = this.rng.range(-2.6, 2.6);
        const u = this.unit(id++, 'att', 'ss', `Boat ${b + 1}`, f.nat, f.id, f.name, 1, 0, 0);
        u.state = 'withdrawn';
        const boat: Boat = { u, fleet: f.id, nat: f.nat, doing: 'out', arrive, from, homed,
          dive: 'surfaced', keel: 0, keelTo: 0, under: 0, tubes: B.tubes, load: B.load, reloadAt: Infinity,
          harm: 0, doomed: false, repairAt: Infinity, scuttleAt: Infinity, struck: -1, nextGun: 0, gunOn: -1,
          side: Math.sin(from) >= 0 ? 1 : -1, ahead: this.rng.range(-0.3, 0.7), until: 0, lastHeard: -Infinity, sees: new Set(), order: null, target: -1 };
        u.extra.dive = 'surfaced'; u.extra.keel = 0; u.extra.scope = false; u.extra.tubes = B.tubes; u.extra.heard = []; u.extra.gunMm = gunMmOf(f.nat);
        this.boats.push(boat);
        this.boatOf.set(u.id, boat);
      }
    }
    // Every merchant the boats can see once one is in touch; the convoy's own see nothing of the boats until found.
  }

  /** The escort pieces each fleet gets, and the ships in each, by its share of the guard. */
  private shares(fleets: ConvoyFleet[], pieces: number, per: number): [ConvoyFleet, number[]][] {
    const total = fleets.reduce((s, f) => s + f.n, 0);
    const out: [ConvoyFleet, number[]][] = [];
    let given = 0;
    fleets.forEach((f, i) => {
      const want = i === fleets.length - 1 ? pieces - given : Math.min(pieces - given, Math.max(1, Math.round((f.n / Math.max(1, total)) * pieces)));
      const list: number[] = [];
      let left = f.n;
      for (let k = 0; k < want; k++) { const s = Math.max(1, Math.min(per, Math.ceil(left / (want - k)))); list.push(s); left -= s; }
      if (want > 0) out.push([f, list]);
      given += want;
    });
    return out;
  }

  private unit(id: number, side: Side, cls: ConvoyClass, name: string, nat: string, corps: number, fleet: string, ships: number, x: number, y: number): ConvoyUnit {
    const len = cls === 'ss' ? B.len : cls === 'dd' ? E.len : 135, beam = cls === 'ss' ? B.beam : cls === 'dd' ? E.beam : 17.5;
    const u: ConvoyUnit = {
      id, side, corps, nat, kind: cls, name, x, y, face: 0, str: 1, mor: 1, state: 'steady', men0: cls === 'ss' ? CONVOY.crew.ss : cls === 'mv' ? CONVOY.crew.mv : 150 * ships,
      figures0: ships, figures: ships, w: beam, d: len, act: 'march', speed: cls === 'mv' ? CONVOY.convoy.speed : 0, engaged: -1,
      extra: { cls, ships0: ships, ships, sunk: 0, fleet },
    };
    this.units.push(u);
    this.byId.set(id, u);
    return u;
  }

  // ---- The clock --------------------------------------------------------------------------------------------------

  step(): void {
    if (this.over) return;
    this.t = Math.round((this.t + DT) * 1000) / 1000;
    while (!this.over && this.sec + CONVOY.tick <= this.t + 1e-9) this.tick();
  }

  /** Runs the night to its end at once (the computer's own nights, tests). */
  runOut(): FieldResult {
    while (!this.over) { this.t = this.sec + CONVOY.tick; this.tick(); }
    return this.over!;
  }

  /** Works one battle second at once (the computer's own nights, the tables). */
  advance(): void { if (!this.over) { this.t = this.sec + CONVOY.tick; this.tick(); } }

  /** The battle seconds worked so far. */
  get worked(): number { return this.sec; }

  private tick(): void {
    this.quiet();
    this.sec += CONVOY.tick;
    const dt = CONVOY.tick, now = this.sec;
    this.cy += CONVOY.convoy.speed * dt;
    for (const m of this.merchants) if (!m.lost) { m.u.x = this.cx + m.dx; m.u.y = this.cy + m.dy; }
    for (const b of this.boats) if (b.doing === 'out' && b.arrive <= now) this.comeUp(b);
    const checking = now % CONVOY.check < CONVOY.tick;
    for (const e of this.escorts) if (e.doing !== 'lost') this.escortHelm(e, dt, checking);
    for (const b of this.boats) if (b.doing !== 'out' && b.doing !== 'left' && b.doing !== 'lost') this.boatHelm(b, dt, checking);
    if (checking) { this.seek(); this.look(); }
    this.landTorps();
    this.burstCharges();
    if (now % S.gunEvery < CONVOY.tick) this.guns();
    if (this.later.length) {
      const due = this.later.filter((l) => l.at <= now);
      if (due.length) { this.later = this.later.filter((l) => l.at > now); for (const l of due) l.run(); }
    }
    if (this.zones.length && this.zones[0].until <= now) this.zones = this.zones.filter((z) => z.until > now);
    const pack = this.boats.some((b) => b.doing !== 'left' && b.doing !== 'lost' && b.arrive !== Infinity);
    if (now >= this.limit || !pack) this.finish(now >= this.limit ? 'time' : 'withdrew');
  }

  /**
   * While no boat is up with the convoy and nothing is in the water, the convoy and its screen only steam on: the clock
   * runs on to a second before the next boat comes up (no roll is thrown in such seconds, so nothing moves but time).
   */
  private quiet(): void {
    if (this.torps.length || this.bursts.length || this.later.length) return;
    let next = Infinity;
    for (const b of this.boats) {
      if (b.doing === 'out') { if (b.arrive < next) next = b.arrive; } else if (b.doing !== 'left' && b.doing !== 'lost') return;
    }
    for (const e of this.escorts) if (e.doing !== 'screen' && e.doing !== 'lost') return;
    const to = Math.min(next === Infinity ? this.limit : Math.ceil(next), this.limit) - 1;
    const gap = to - this.sec;
    if (gap < 10) return;
    const shift = CONVOY.convoy.speed * gap;
    this.cy += shift;
    for (const e of this.escorts) if (e.doing !== 'lost') e.u.y += shift;
    this.sec = to;
    if (this.t < this.sec) this.t = this.sec;
  }

  // ---- The boats ---------------------------------------------------------------------------------------------------

  private comeUp(b: Boat): void {
    const r = B.home;
    b.u.x = this.cx + Math.sin(b.from) * r;
    b.u.y = this.cy + Math.cos(b.from) * r;
    b.u.state = 'steady';
    b.doing = 'close';
    if (b.homed && !this.boats.some((o) => o !== b && o.doing !== 'out' && o.homed && o.nat === b.nat)) {
      const near = this.nearestMerchant(b.u.x, b.u.y);
      this.say('sea:report', b.u, `${b.u.name} reports the convoy.`, { at: near?.u.id ?? -1 });
    }
  }

  private setDive(b: Boat, dive: Dive, why: string): void {
    if (b.dive === dive) return;
    b.dive = dive;
    b.keelTo = dive === 'surfaced' ? 0 : dive === 'periscope' ? B.scope : B.deep;
    if (dive !== 'surfaced' && b.under === 0) b.under = this.sec;
    if (dive === 'surfaced') b.under = 0;
    this.say('sea:dive', b.u, dive === 'surfaced' ? `${b.u.name} surfaces.` : dive === 'deep' ? `${b.u.name} goes deep.` : `${b.u.name} dives.`, { dive, keel: b.keelTo, why });
  }

  /** Whether she is under far enough that radar and lookouts no longer find her: half her crash dive. */
  private under(b: Boat): boolean { return b.dive !== 'surfaced' && this.sec - b.under >= B.diveTime / 2; }

  private boatHelm(b: Boat, dt: number, checking: boolean): void {
    const u = b.u, now = this.sec;
    // Keel toward her depth (a look: a crash dive to periscope depth in diveTime, deeper at a metre a second).
    const rate = b.keelTo > b.keel ? (b.keel < B.scope ? B.scope / B.diveTime : 1) : 2;
    b.keel = b.keel < b.keelTo ? Math.min(b.keelTo, b.keel + rate * dt) : Math.max(b.keelTo, b.keel - rate * dt);
    if (b.tubes < B.tubes && b.load > 0 && now >= b.reloadAt) { const k = Math.min(B.tubes - b.tubes, b.load); b.tubes += k; b.load -= k; b.reloadAt = b.load > 0 && b.tubes < B.tubes ? now + B.reload : Infinity; }
    const heard = this.firstHeard(b);
    if (heard >= 0) b.lastHeard = now;
    let goX = u.x, goY = u.y, speed = 0;
    switch (b.doing) {
      case 'close': {
        if (this.boatWantsDown(b, heard)) break;
        const under = b.order === 'under';
        const gx = this.cx + b.side * (this.halfW + B.fireAt * 0.9), gy = this.cy + b.ahead * this.halfL + 400;
        const far = dist(u.x, u.y, gx, gy);
        if (under && far < 5000 && b.dive === 'surfaced') this.setDive(b, 'periscope', 'attack');
        goX = gx; goY = gy;
        speed = b.dive !== 'surfaced' ? B.dived : far > 4000 ? B.surfaced : B.cruise;
        // (The nearest ship is sought only once she is within firing reach of the convoy's box: the night's own cost.)
        const bx = Math.max(0, Math.abs(u.x - this.cx) - this.halfW), by = Math.max(0, Math.abs(u.y - this.cy) - this.halfL);
        const m = bx * bx + by * by <= (B.fireAt + 300) * (B.fireAt + 300) || b.order === 'attack' || b.order === 'under' || this.ticket.place.habits[b.nat]?.warships ? this.targetFor(b) : null;
        if (m && dist(u.x, u.y, m.x, m.y) <= B.fireAt && b.tubes > 0) this.fire(b, m);
        else if (b.tubes === 0 && b.load === 0) this.leave(b, 'spent');
        break;
      }
      case 'away': {
        const a = bearing(this.cx, this.cy, u.x, u.y);
        goX = u.x + Math.sin(a) * 1000; goY = u.y + Math.cos(a) * 1000 + 500;
        speed = b.dive === 'surfaced' ? B.surfaced : B.dived;
        if (this.boatWantsDown(b, heard)) break;
        if (now >= b.until) {
          if (b.tubes === 0 && b.load === 0) this.leave(b, 'spent');
          else if (b.tubes > 0) b.doing = 'close';
        }
        break;
      }
      case 'evade':
      case 'deep': {
        const near = this.nearestEscort(u.x, u.y);
        if (near) { const a = bearing(near.u.x, near.u.y, u.x, u.y); goX = u.x + Math.sin(a) * 500; goY = u.y + Math.cos(a) * 500; }
        speed = b.doing === 'deep' ? B.creep : B.dived;
        if (heard >= 0 && b.doing === 'evade') { b.doing = 'deep'; this.setDive(b, 'deep', 'evade'); }
        const quiet = now - b.lastHeard;
        if (b.order === 'dive' || b.order === 'deep') break;
        if (quiet >= E.lostFor && now >= b.until) {
          if (b.harm >= 0.5 || (b.tubes === 0 && b.load === 0)) { this.leave(b, b.harm >= 0.5 ? 'hurt' : 'spent'); break; }
          this.setDive(b, 'surfaced', 'attack');
          b.doing = b.tubes > 0 ? 'close' : 'away';
          b.until = now + 600;
        }
        break;
      }
      case 'up':
      case 'gun': {
        speed = b.doomed ? 0 : 2;
        const a = bearing(this.cx, this.cy, u.x, u.y); goX = u.x + Math.sin(a) * 1000; goY = u.y + Math.cos(a) * 1000;
        if (!b.doomed && now >= b.repairAt) {
          const near = this.nearestEscort(u.x, u.y);
          if (b.harm < 2 && (!near || dist(near.u.x, near.u.y, u.x, u.y) > S.escape)) {
            this.setDive(b, 'deep', 'evade');
            this.tally.escaped++;
            this.leave(b, 'hurt');
            break;
          }
          b.repairAt = now + S.repair / 2;
        }
        break;
      }
      default:
        break;
    }
    // Player orders that override the captain's course.
    if (b.order === 'leave' && b.doing !== 'up' && b.doing !== 'gun') { this.leave(b, 'ordered'); return; }
    this.steer(u, goX, goY, speed, dt);
    void checking;
  }

  /** A boat on the surface that sees an escort closing on her, or is held, goes under (her captain's call). */
  private boatWantsDown(b: Boat, heard: number): boolean {
    if (b.dive !== 'surfaced') return false;
    if (b.order === 'surface') return false;
    const u = b.u;
    let why: string | null = heard >= 0 ? (this.escortOf.get(heard)!.echo?.kind === 'radar' ? 'radar' : 'evade') : null;
    if (!why) {
      for (const id of b.sees) {
        const e = this.escortOf.get(id);
        if (!e || e.doing === 'lost') continue;
        const d = dist(e.u.x, e.u.y, u.x, u.y);
        const closing = Math.abs(turn(e.u.face, bearing(e.u.x, e.u.y, u.x, u.y))) < 0.6 && e.u.speed > E.screen * 0.9;
        if (d < this.ticket.place.light.vis * B.lookout * 0.8 && closing) { why = 'evade'; break; }
      }
    }
    if (b.order === 'dive' || b.order === 'deep') why = 'ordered';
    if (!why) return false;
    this.setDive(b, b.order === 'deep' ? 'deep' : 'periscope', why);
    b.doing = 'evade';
    b.until = this.sec + 300;
    return true;
  }

  private leave(b: Boat, why: 'spent' | 'hurt' | 'ordered'): void {
    if (b.doing === 'left' || b.doing === 'lost') return;
    b.doing = 'left';
    b.u.state = 'withdrawn';
    b.u.act = 'idle';
    this.dropContacts(b.u.id);
    this.say('withdrawn', b.u, why === 'spent' ? `${b.u.name} has fired her last torpedo and turns for home.` : why === 'hurt' ? `${b.u.name} slips away, damaged.` : `${b.u.name} breaks off.`, { why });
  }

  /** The merchant (or escort, by her navy's habit) a boat would fire at now: the nearest she can reach. */
  private targetFor(b: Boat): ConvoyUnit | null {
    const u = b.u;
    if (b.order === 'attack' || b.order === 'under') { const t = this.byId.get(b.target); if (t && live(t) && t.side === 'def') return t; }
    const habits = this.ticket.place.habits[b.nat];
    if (habits?.warships) {
      let best: ConvoyUnit | null = null, bd = B.fireAt * 1.3;
      for (const id of b.sees) { const e = this.escortOf.get(id); if (!e || e.doing === 'lost') continue; const d = dist(e.u.x, e.u.y, u.x, u.y); if (d < bd) { bd = d; best = e.u; } }
      if (best) return best;
    }
    return this.nearestMerchant(u.x, u.y)?.u ?? null;
  }

  private fire(b: Boat, target: ConvoyUnit): void {
    const n = Math.min(b.tubes, B.salvo);
    if (n <= 0) return;
    b.tubes -= n;
    if (b.reloadAt === Infinity && b.load > 0) b.reloadAt = this.sec + B.reload;
    const range = dist(b.u.x, b.u.y, target.x, target.y);
    const running = Math.max(10, Math.round(range / T.speed));
    // Where the salvo meets the target's way: ahead of her by her speed over the run.
    const ax = target.x, ay = target.y + (target.kind === 'mv' ? CONVOY.convoy.speed * running : 0);
    const escort = target.kind === 'dd';
    const aims: number[] = [];
    for (let k = 0; k < n; k++) aims.push(target.id);
    this.torps.push({ by: b.u.id, at: this.sec + running, aims, escort, x: ax, y: ay });
    this.tally.salvoes++;
    this.noteContact();
    this.say('sea:torpedo', b.u, `${b.u.name} fires a salvo at ${escort ? target.name : 'the convoy'}.`, {
      target: target.id, x: Math.round(ax), y: Math.round(ay), running, fish: n, spread: 10, wakeless: CONVOY.wakeless.includes(b.nat), dive: b.dive,
    }, 'torpedo_launch');
    b.doing = 'away';
    b.until = this.sec + 600;
    b.order = b.order === 'attack' || b.order === 'under' ? null : b.order;
  }

  private landTorps(): void {
    if (!this.torps.length) return;
    const now = this.sec, due = this.torps.filter((t) => t.at <= now);
    if (!due.length) return;
    this.torps = this.torps.filter((t) => t.at > now);
    for (const t of due) {
      const by = this.byId.get(t.by)!;
      const struck = new Set<number>();
      for (let k = 0; k < t.aims.length; k++) {
        if (t.escort) {
          const e = this.escortOf.get(t.aims[k]);
          if (!e || e.doing === 'lost' || struck.has(e.u.id) || !this.rng.chance(T.escortHit)) continue;
          struck.add(e.u.id);
          this.tally.hits++;
          this.say('sea:torpedoHit', e.u, `A torpedo hits ${e.u.name}.`, { from: t.by }, 'torpedo_hit', 'att');
          if (this.rng.chance(T.escortSinks)) this.escortShipLost(e, 'torpedo');
          else { e.harm = Math.max(e.harm, 0.6); this.say('sea:burning', e.u, `${e.u.name} is hit and falls out, burning.`, { cls: 'dd', fleet: e.fleet, hull: 0.4, limp: { course: Math.PI, speed: 2 } }); }
          continue;
        }
        // Into the columns: each fish that runs true takes any ship within its reach of the aim.
        if (!this.rng.chance(T.hit)) continue;
        const near = this.merchants.filter((m) => !m.lost && !struck.has(m.u.id) && dist(m.u.x, m.u.y, t.x, t.y + (k - (t.aims.length - 1) / 2) * 120) <= T.column);
        if (!near.length) continue;
        near.sort((a, b2) => dist(a.u.x, a.u.y, t.x, t.y) - dist(b2.u.x, b2.u.y, t.x, t.y) || a.u.id - b2.u.id);
        const m = near[0];
        struck.add(m.u.id);
        this.tally.hits++;
        this.merchantLost(m, t.by);
      }
      void by;
    }
  }

  private merchantLost(m: Merchant, by: number): void {
    m.lost = true;
    this.counts.mv++;
    this.say('sea:torpedoHit', m.u, `A torpedo hits ${m.u.name}.`, { from: by }, 'torpedo_hit', 'att');
    const how = ['settle', 'break', 'burn', 'capsize'][Math.floor(mix(this.ticket.seed ^ (m.u.id * 104729)) * 4)];
    const u = m.u;
    u.state = 'gone';
    u.figures = 0; u.str = 0;
    u.extra.ships = 0; u.extra.sunk = 1;
    u.speed = 0;
    const at = this.sec + 60 + Math.round(mix(m.u.id * 31 + this.ticket.seed) * 240);
    this.later.push({ at, run: () => this.say('sea:sunk', u, `${u.name} goes down.`, { cls: 'mv', how, ship: 1 }, 'ship_sinking', 'def') });
    this.later.push({ at: at - 30, run: () => this.say('sea:men', u, '', { ship: 1, men: CONVOY.crew.mv, dead: Math.round(CONVOY.crew.mv * mix(u.id * 13 + 7) * 0.6) }, undefined, 'def') });
  }

  // ---- The escorts ------------------------------------------------------------------------------------------------

  /** Whether the staff commands this piece: its side is the computer's, or it is an ally's under the player's side (he commands only his own, ruling 241 Q4). */
  private staffRuns(u: ConvoyUnit): boolean { return this.ai[u.side] || (this.ticket.place.mine !== undefined && u.nat !== this.ticket.place.mine); }

  private awayCount(): number { let n = 0; for (const e of this.escorts) if (e.doing !== 'screen' && e.doing !== 'lost') n++; return n; }

  private escortHelm(e: Escort, dt: number, checking: boolean): void {
    const u = e.u, now = this.sec;
    const groupsKit = e.kit.groups;
    let goX = this.cx + e.sx, goY = this.cy + e.sy, speed = E.screen;
    const target = e.holds >= 0 ? this.boatOf.get(e.holds) : undefined;
    if (target && (target.doing === 'lost' || target.doing === 'left')) this.drop(e, 'faded');
    const huntCap = groupsKit ? E.groupFor : E.huntFor;
    switch (e.doing) {
      case 'screen': {
        const off = Math.sin(now / 240 + u.id) * 400;
        goX += off; goY += Math.cos(now / 300 + u.id) * 200;
        const want = dist(u.x, u.y, goX, goY);
        speed = clamp(CONVOY.convoy.speed + want / 60, 0, E.top);
        if (this.staffRuns(u) && e.holds >= 0 && (this.awayCount() < Math.ceil(E.keepAway * this.escorts.length) || e.u.extra.ships > 1)) this.beginHunt(e);
        break;
      }
      case 'run': {
        const b = target;
        if (!b) { e.doing = 'search'; break; }
        goX = e.echo && e.echo.kind !== 'asdic' && b.dive === 'surfaced' ? b.u.x : e.dx; goY = e.echo && e.echo.kind !== 'asdic' && b.dive === 'surfaced' ? b.u.y : e.dy;
        speed = E.top;
        const d = dist(u.x, u.y, goX, goY);
        if ((b.doing === 'up' || b.doing === 'gun') && d <= E.ram && checking && this.staffRuns(u) && b.struck < 0 && this.rng.chance(S.ramShare)) { this.ram(e, b); break; }
        if (b.dive !== 'surfaced' && d < 600) { e.doing = 'search'; e.seenAt = now; }
        if (now - e.huntFrom > huntCap) this.goBack(e);
        break;
      }
      case 'search': {
        // Round the datum, widening, at the speed the sonar can hear at.
        const r = 300 + Math.min(1500, (now - e.seenAt) * 2);
        const a = now / 90 + u.id;
        goX = e.dx + Math.sin(a) * r; goY = e.dy + Math.cos(a) * r;
        speed = E.hunt;
        if (e.holds >= 0 && e.echo?.kind === 'asdic') e.doing = 'attack';
        else if (target && target.dive === 'surfaced' && e.echo && e.echo.kind !== 'asdic') e.doing = 'run';
        if (now - e.seenAt > E.lostFor || now - e.huntFrom > huntCap) this.goBack(e);
        break;
      }
      case 'attack': {
        const b = target;
        if (!b || !e.echo) { e.doing = 'search'; break; }
        goX = b.u.x; goY = b.u.y; speed = E.hunt;
        const d = dist(u.x, u.y, b.u.x, b.u.y);
        if (b.dive === 'surfaced') { e.doing = 'run'; break; }
        if (e.kit.hedgehog && d <= E.ahead && d > 120) this.chargeAhead(e, b);
        else if (d <= 60) this.chargePattern(e, b);
        if (now - e.huntFrom > huntCap) this.goBack(e);
        break;
      }
      case 'open': {
        goX = u.x + Math.sin(u.face) * 500; goY = u.y + Math.cos(u.face) * 500;
        speed = E.hunt;
        if (dist(u.x, u.y, e.dx, e.dy) >= e.openTo) e.doing = e.holds >= 0 && e.echo?.kind === 'asdic' ? 'attack' : 'search';
        if (now - e.huntFrom > huntCap) this.goBack(e);
        break;
      }
      case 'back': {
        speed = E.top;
        if (dist(u.x, u.y, goX, goY) < 400) { e.doing = 'screen'; e.order = null; }
        break;
      }
      case 'rescue': {
        goX = e.tx; goY = e.ty; speed = dist(u.x, u.y, goX, goY) < 300 ? 0 : E.top;
        if (speed === 0 && now >= e.until) this.goBack(e);
        break;
      }
      case 'move': {
        goX = e.tx; goY = e.ty; speed = E.top;
        if (dist(u.x, u.y, goX, goY) < 300) { e.dx = e.tx; e.dy = e.ty; e.doing = 'search'; e.seenAt = now; e.huntFrom = now; }
        break;
      }
      default:
        break;
    }
    if (e.harm >= 0.6) speed = Math.min(speed, 3);
    this.steer(u, goX, goY, speed, dt);
  }

  private beginHunt(e: Escort, by: 'staff' | 'order' = 'staff'): void {
    const b = this.boatOf.get(e.holds);
    if (!b) return;
    e.doing = b.dive === 'surfaced' && e.echo?.kind !== 'asdic' ? 'run' : 'search';
    e.huntFrom = this.sec;
    e.seenAt = this.sec;
    e.dx = b.u.x; e.dy = b.u.y;
    e.attacks = 0;
    this.tally.hunts++;
    // A group trained to hunt sends a second ship to work the contact with the first (escortgroups): the creeping attack.
    if (by === 'staff' && e.kit.groups && this.staffRuns(e.u)) {
      const mate = this.escorts.filter((o) => o !== e && o.doing === 'screen' && o.kit.groups && o.holds < 0)
        .sort((a, c) => dist(a.u.x, a.u.y, b.u.x, b.u.y) - dist(c.u.x, c.u.y, b.u.x, b.u.y) || a.u.id - c.u.id)[0];
      if (mate && this.awayCount() < Math.ceil(E.keepAway * this.escorts.length)) {
        mate.holds = b.u.id; mate.echo = e.echo ? { ...e.echo } : null; mate.doing = 'search'; mate.huntFrom = this.sec; mate.seenAt = this.sec; mate.dx = b.u.x; mate.dy = b.u.y; mate.attacks = 0;
      }
    }
  }

  private goBack(e: Escort): void {
    if (e.holds >= 0) this.drop(e, 'faded');
    e.doing = 'back';
    e.order = null;
  }

  /** The escorts hunting a boat now (a group where two or more work her, or one piece of several ships). */
  private huntersOf(b: Boat): Escort[] { return this.escorts.filter((e) => e.holds === b.u.id && e.doing !== 'lost' && e.doing !== 'screen' && e.doing !== 'back'); }

  private chargePattern(e: Escort, b: Boat): void {
    const group = this.huntersOf(b).length >= 2 || e.u.extra.ships > 1;
    const set = Math.max(15, Math.round(b.keel + (mix(this.sec * 7 + e.u.id) - 0.5) * 30));
    const sinking = Math.round((set / 3) * 10) / 10;
    const x = Math.round(b.u.x), y = Math.round(b.u.y);
    this.say('sea:depthCharge', e.u, `${e.u.name} drops a pattern of ${E.pattern} on ${b.u.name}.`, { on: b.u.id, x, y, n: E.pattern, set, sinking, spread: E.spread, kind: 'pattern' }, 'depth_charge', 'def', x, y);
    this.zones.push({ id: ++this.zoneId, kind: 'pattern', x, y, r: E.spread / 2, until: this.sec + sinking + 30, side: 'def', unit: e.u.id });
    this.bursts.push({ at: this.sec + Math.ceil(sinking), by: e.u.id, on: b.u.id, x, y, ahead: false, group, kit: e.kit });
    this.attacked(e);
  }

  private chargeAhead(e: Escort, b: Boat): void {
    const group = this.huntersOf(b).length >= 2 || e.u.extra.ships > 1;
    const x = Math.round(b.u.x), y = Math.round(b.u.y);
    const sinking = Math.round((8 + b.keel / 7) * 10) / 10;
    this.say('sea:depthCharge', e.u, `${e.u.name} fires its ahead-throwers at ${b.u.name}.`, { on: b.u.id, x, y, n: 24, set: Math.round(b.keel), sinking, spread: 40, kind: 'ahead' }, 'ahead_thrower', 'def', x, y);
    this.bursts.push({ at: this.sec + Math.ceil(sinking), by: e.u.id, on: b.u.id, x, y, ahead: true, group, kit: e.kit });
    this.attacked(e);
  }

  private attacked(e: Escort): void {
    e.attacks++;
    this.tally.attacks++;
    e.doing = 'open';
    e.dx = e.u.x; e.dy = e.u.y;
    e.openTo = E.open;
    if (e.echo) e.u.extra.huntPhase = 'open';
  }

  private burstCharges(): void {
    if (!this.bursts.length) return;
    const now = this.sec, due = this.bursts.filter((x) => x.at <= now);
    if (!due.length) return;
    this.bursts = this.bursts.filter((x) => x.at > now);
    for (const x of due) {
      const b = this.boatOf.get(x.on);
      if (!b || b.doing === 'lost' || b.doing === 'left') continue;
      // A boat that has run out of the pattern's reach since it was laid is missed.
      if (dist(b.u.x, b.u.y, x.x, x.y) > E.spread + 60) continue;
      const kit: ConvoyKit = x.ahead ? x.kit : { ...x.kit, hedgehog: false };
      const p = attackKills(kit, x.group);
      const u = this.rng.next();
      if (u < p) {
        this.say('sea:depthChargeHit', b.u, `${b.u.name} is hit hard.`, { from: x.by, harm: 1, keel: Math.round(b.keel) }, undefined, 'def');
        if (this.rng.chance(H.forced)) this.forcedUp(b, true, x.by);
        else this.boatLost(b, 'crushed', x.by);
      } else if (!x.ahead && u < p + H.hurt) {
        b.harm += H.harm;
        this.say('sea:depthChargeHit', b.u, `${b.u.name} is damaged.`, { from: x.by, harm: Math.round(H.harm * 100) / 100, keel: Math.round(b.keel) }, undefined, 'def');
        if (b.harm >= 1) this.forcedUp(b, false, x.by);
      }
    }
  }

  /** Forced to the surface: doomed (her crew will give her up), or hurt and trying to make good her damage. */
  private forcedUp(b: Boat, doomed: boolean, by: number): void {
    this.tally.forcedUp++;
    b.doing = 'up';
    b.doomed = doomed;
    this.setDive(b, 'surfaced', 'forced');
    b.repairAt = doomed ? Infinity : this.sec + S.repair;
    b.u.act = 'idle';
    const near = this.nearestEscort(b.u.x, b.u.y);
    const close = near ? dist(near.u.x, near.u.y, b.u.x, b.u.y) : Infinity;
    if (!doomed) return;
    // Her crew's call: fight with the deck gun while she floats, strike to an escort (subSurrender), or abandon her.
    if (this.ticket.place.surrender && near && close <= 1500 && this.rng.chance(S.strike)) {
      b.struck = near.u.id;
      this.say('sea:surrender', b.u, `${b.u.name}'s crew give her up.`, { ship: 1, how: 'scuttle', by: near.u.id }, undefined, 'att');
    } else if (near && close <= S.gunAt && this.rng.chance(S.fight)) {
      b.doing = 'gun'; b.gunOn = near.u.id;
    }
    const [lo, hi] = S.scuttleIn;
    b.scuttleAt = this.sec + lo + Math.round(this.rng.range(0, hi - lo));
    const at = b.scuttleAt;
    this.later.push({ at: at - 50, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.say('sea:abandon', b.u, `${b.u.name}'s crew abandon her.`, { ship: 1 }, undefined, 'att'); b.gunOn = -1; b.doing = 'up'; } });
    this.later.push({ at: at - 45, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.say('sea:men', b.u, '', { ship: 1, men: CONVOY.crew.ss, dead: Math.round(CONVOY.crew.ss * mix(b.u.id * 17 + this.ticket.seed) * 0.4) }, undefined, 'att'); } });
    this.later.push({ at, run: () => { if (b.doing === 'lost' || b.doing === 'left') return; this.boatLost(b, b.struck >= 0 && this.boarded(b) && this.rng.chance(S.taken) ? 'captured' : 'scuttled', by); } });
  }

  /** A struck boat is taken where an escort lies within boarding distance of her as she would have gone down. */
  private boarded(b: Boat): boolean {
    const e = this.escortOf.get(b.struck);
    return !!e && e.doing !== 'lost' && dist(e.u.x, e.u.y, b.u.x, b.u.y) <= S.board;
  }

  private ram(e: Escort, b: Boat): void {
    e.harm = Math.max(e.harm, 0.3);
    this.say('sea:burning', e.u, `${e.u.name} rams ${b.u.name}.`, { cls: 'dd', fleet: e.fleet, hull: 0.7 }, undefined, 'def');
    this.boatLost(b, 'rammed', e.u.id);
  }

  private boatLost(b: Boat, how: 'crushed' | 'scuttled' | 'shelled' | 'rammed' | 'captured', by: number): void {
    if (b.doing === 'lost') return;
    b.doing = 'lost';
    this.counts.ss[b.fleet] = (this.counts.ss[b.fleet] ?? 0) + 1;
    this.tally[how]++;
    const u = b.u;
    u.state = 'gone'; u.figures = 0; u.str = 0; u.speed = 0; u.act = 'down';
    u.extra.ships = 0; u.extra.sunk = 1; u.extra.gun = undefined;
    this.dropContacts(u.id);
    const x = Math.round(u.x), y = Math.round(u.y);
    if (how === 'captured') {
      this.say('sea:surrender', u, `${u.name} is taken.`, { ship: 1, how: 'captured', by: b.struck }, undefined, 'att');
      return;
    }
    const words = how === 'crushed' ? `${u.name} is destroyed.` : how === 'scuttled' ? `${u.name} is scuttled by her crew.` : how === 'shelled' ? `${u.name} goes down under gunfire.` : `${u.name} is rammed and sunk.`;
    this.say('sea:sunk', u, words, { cls: 'ss', fleet: b.fleet, how, ship: 1, from: by }, 'ship_sinking', 'att', x, y);
    this.zones.push({ id: ++this.zoneId, kind: 'oil', x, y, r: 200, until: this.sec + 1800, side: 'att', unit: u.id });
    if (how === 'crushed') this.say('sea:oil', u, '', { r: 200, until: this.sec + 1800 }, undefined, 'att', x, y);
  }

  private escortShipLost(e: Escort, why: 'torpedo' | 'gun'): void {
    this.counts.dd[e.fleet] = (this.counts.dd[e.fleet] ?? 0) + 1;
    const u = e.u;
    u.extra.ships = Math.max(0, (u.extra.ships as number) - 1);
    u.extra.sunk = (u.extra.sunk as number) + 1;
    u.figures = u.extra.ships as number;
    u.str = u.figures / Math.max(1, u.figures0);
    this.say('sea:sunk', u, why === 'torpedo' ? `${u.name} is torpedoed and sinks.` : `${u.name} sinks under the boat's gunfire.`, { cls: 'dd', fleet: e.fleet, how: why === 'torpedo' ? 'break' : 'shelled', ship: u.extra.sunk }, 'ship_sinking', 'att');
    if (u.figures === 0) {
      e.doing = 'lost';
      if (e.holds >= 0) this.drop(e, 'faded');
      u.state = 'gone'; u.speed = 0;
    } else {
      e.harm = 0;
    }
  }

  // ---- Guns ---------------------------------------------------------------------------------------------------------

  private guns(): void {
    // A boat on the surface fighting with her deck gun, or ordered to.
    for (const b of this.boats) {
      if (b.doing === 'lost' || b.doing === 'left' || b.doing === 'out' || b.dive !== 'surfaced') continue;
      let on = b.doing === 'gun' ? b.gunOn : b.order === 'gun' ? b.target : -1;
      const t = on >= 0 ? this.byId.get(on) : undefined;
      if (!t || !live(t) || dist(b.u.x, b.u.y, t.x, t.y) > S.gunAt) { b.u.extra.gun = undefined; continue; }
      if (b.u.extra.gun !== t.id) this.say('sea:gun', b.u, `${b.u.name} opens fire on ${t.name} with her deck gun.`, { on: t.id, why: b.doing === 'gun' ? 'fight' : 'attack' }, 'deck_gun', 'att');
      b.u.extra.gun = t.id;
      on = t.id;
      if (!this.rng.chance(S.boatHits)) continue;
      this.say('sea:gunHit', t, `A shell from ${b.u.name}'s deck gun hits ${t.name}.`, { n: 1, from: b.u.id }, undefined, 'att');
      if (t.kind === 'dd') {
        const e = this.escortOf.get(t.id)!;
        e.harm += S.boatHarm;
        if (e.harm >= 1) this.escortShipLost(e, 'gun');
      } else if (t.kind === 'mv') {
        const m = this.merchants.find((x) => x.u.id === t.id)!;
        const h = ((m.u.extra.harm as number | undefined) ?? 0) + S.boatHarm;
        m.u.extra.harm = h;
        if (h >= 1 && !m.lost) this.merchantShelled(m, b.u.id);
      }
    }
    // Escorts' guns on a surfaced boat they hold in reach; armed merchants on one the convoy has found.
    for (const e of this.escorts) {
      if (e.doing === 'lost' || e.holds < 0) { e.u.extra.gun = undefined; continue; }
      const b = this.boatOf.get(e.holds)!;
      if (b.dive !== 'surfaced' || dist(e.u.x, e.u.y, b.u.x, b.u.y) > S.gunAt || b.struck >= 0) { e.u.extra.gun = undefined; continue; }
      if (e.u.extra.gun !== b.u.id) this.say('sea:gun', e.u, `${e.u.name} opens fire on ${b.u.name}.`, { on: b.u.id, why: 'attack' }, 'naval_gun', 'def');
      e.u.extra.gun = b.u.id;
      if (this.rng.chance(S.escortHits)) this.shellBoat(b, e.u.id);
    }
    for (const m of this.merchants) {
      if (m.lost || !m.armed) continue;
      let best: Boat | null = null, bd = 2000;
      for (const id of this.known.def) { const b = this.boatOf.get(id); if (!b || b.dive !== 'surfaced' || b.doing === 'lost' || b.doing === 'left') continue; const d = dist(m.u.x, m.u.y, b.u.x, b.u.y); if (d < bd) { bd = d; best = b; } }
      if (!best) { m.u.extra.gun = undefined; continue; }
      if (m.u.extra.gun !== best.u.id) this.say('sea:gun', m.u, `${m.u.name}'s gunners open fire.`, { on: best.u.id, why: 'defend' }, 'deck_gun', 'def');
      m.u.extra.gun = best.u.id;
      if (this.rng.chance(S.merchantHits)) this.shellBoat(best, m.u.id);
    }
  }

  private shellBoat(b: Boat, by: number): void {
    b.harm += S.escortHarm;
    this.say('sea:gunHit', b.u, `A shell hits ${b.u.name}.`, { n: 1, from: by }, undefined, 'def');
    if (b.harm >= 2) { this.boatLost(b, 'shelled', by); return; }
    if (b.harm >= 1 && b.doing !== 'up' && b.doing !== 'gun') this.forcedUp(b, false, by);
  }

  private merchantShelled(m: Merchant, by: number): void {
    m.lost = true;
    this.counts.mv++;
    const u = m.u;
    u.state = 'gone'; u.figures = 0; u.str = 0; u.extra.ships = 0; u.extra.sunk = 1; u.speed = 0;
    this.later.push({ at: this.sec + 120, run: () => this.say('sea:sunk', u, `${u.name} goes down under the boat's gunfire.`, { cls: 'mv', how: 'shelled', ship: 1, from: by }, 'ship_sinking', 'att') });
  }

  // ---- Contacts -----------------------------------------------------------------------------------------------------

  /** The first escort that holds a boat now, or -1. */
  private firstHeard(b: Boat): number { for (const e of this.escorts) if (e.holds === b.u.id && e.doing !== 'lost') return e.u.id; return -1; }

  /** Who hears a boat now: the escorts that hold her. */
  private heardBy(b: Boat): number[] { return this.escorts.filter((e) => e.holds === b.u.id && e.doing !== 'lost').map((e) => e.u.id); }

  /** The escorts' search: radar and lookouts on a surfaced boat, sonar on a dived one (convoy.json detect, kit by navy). */
  private seek(): void {
    const C = CONVOY.check, vis = this.ticket.place.light.vis;
    for (const e of this.escorts) {
      if (e.doing === 'lost') continue;
      const u = e.u;
      // A held contact: kept while she stays within the sense that holds her, slipping at 'slip' a check.
      if (e.holds >= 0) {
        const b = this.boatOf.get(e.holds)!;
        const d = dist(u.x, u.y, b.u.x, b.u.y);
        const kind = this.senses(e, b, d, vis);
        if (!kind) { this.drop(e, b.dive !== 'surfaced' && e.echo?.kind !== 'asdic' ? 'deep' : 'faded'); continue; }
        if (this.rng.chance(D.slip * (b.dive === 'deep' ? 1.5 : 1))) { this.drop(e, 'faded'); continue; }
        this.hold(e, b, kind, d, false);
        continue;
      }
      for (const b of this.boats) {
        if (b.doing === 'out' || b.doing === 'left' || b.doing === 'lost') continue;
        const d = dist(u.x, u.y, b.u.x, b.u.y);
        if (d > Math.max(E.radar, vis * 2, E.asdic)) continue;
        const kind = this.senses(e, b, d, vis);
        if (!kind) continue;
        const mean = kind === 'radar' ? D.radar : kind === 'sight' ? D.sight : D.asdic;
        const p = chanceIn(mean, C) * (kind === 'asdic' && b.dive === 'deep' ? D.deep : 1);
        if (!this.rng.chance(p)) continue;
        this.hold(e, b, kind, d, true);
        // The player's own escorts on the screen report it and wait for his order; the staff's go after her.
        break;
      }
    }
  }

  /** The sense an escort could hold this boat by, now: radar or lookouts on the surface, sonar below. */
  private senses(e: Escort, b: Boat, d: number, vis: number): Echo['kind'] | null {
    if (!this.under(b)) {
      if (e.kit.radar && d <= E.radar) return 'radar';
      const lit = this.zones.some((z) => z.kind === 'starshell' && dist(z.x, z.y, b.u.x, b.u.y) <= z.r);
      if (d <= vis * (lit ? 2 : 1)) return 'sight';
      return null;
    }
    if (!e.kit.asdic || d > E.asdic || e.u.speed > E.deaf) return null;
    if (Math.abs(turn(e.u.face, bearing(e.u.x, e.u.y, b.u.x, b.u.y))) > E.arc && d > E.blind) return null;
    return 'asdic';
  }

  private hold(e: Escort, b: Boat, kind: Echo['kind'], d: number, gained: boolean): void {
    // (clockwise from north, 0 to under a full turn, as the scene reads a contact's bearing)
    const br = Math.round((((bearing(e.u.x, e.u.y, b.u.x, b.u.y) % TAU) + TAU) % TAU) * 1000) / 1000;
    const err = Math.round((mix(this.sec * 131 + e.u.id * 7 + b.u.id) - 0.5) * (kind === 'asdic' ? 0.1 : 0.04) * 1000) / 1000;
    const was = e.echo?.kind;
    e.holds = b.u.id;
    e.echo = { on: b.u.id, kind, range: Math.round(d), bearing: br, err };
    e.seenAt = this.sec;
    e.dx = b.u.x; e.dy = b.u.y;
    this.known.def.add(b.u.id);
    if (gained || was !== kind) {
      this.noteContact();
      this.say('sea:sonar', e.u, gained ? `${e.u.name} has ${kind === 'asdic' ? 'a sonar' : kind === 'radar' ? 'a radar' : 'a sighting of a boat'}${kind === 'sight' ? '' : ' contact'}.` : '', { on: b.u.id, kind, range: Math.round(d), bearing: br, err }, undefined, 'def');
      // Star shell over a boat seen by eye where the night is dark: a look, and it lights her for the other lookouts.
      if (kind === 'sight' && this.ticket.place.light.moon.lit < 0.6 && this.sec >= e.nextShell) {
        e.nextShell = this.sec + 180;
        const x = Math.round(b.u.x), y = Math.round(b.u.y);
        this.say('sea:starshell', e.u, `${e.u.name} fires star shell.`, { by: e.u.id, x, y }, 'starshell', 'def', x, y);
        this.zones.push({ id: ++this.zoneId, kind: 'starshell', x, y, r: 800, until: this.sec + 60, side: 'def', unit: e.u.id });
      }
    }
  }

  private drop(e: Escort, why: 'close' | 'deep' | 'wake' | 'faded'): void {
    if (e.holds < 0) return;
    const on = e.holds;
    e.holds = -1;
    const kind = e.echo?.kind ?? 'asdic';
    e.echo = null;
    this.say('sea:sonar', e.u, '', { on, kind, lost: why }, undefined, 'def');
    if (!this.escorts.some((o) => o.holds === on)) this.known.def.delete(on);
    if (e.doing === 'attack' || e.doing === 'open' || e.doing === 'run') e.doing = 'search';
  }

  private dropContacts(id: number): void { for (const e of this.escorts) if (e.holds === id) this.drop(e, 'faded'); this.known.def.delete(id); }

  /** The boats' lookouts: escorts within their reach are picked up in 'boat' seconds on average; lost once out of it. */
  private look(): void {
    const reach = this.ticket.place.light.vis * B.lookout;
    const p = chanceIn(D.boat, CONVOY.check);
    for (const b of this.boats) {
      if (b.doing === 'out' || b.doing === 'left' || b.doing === 'lost') continue;
      const up = b.dive !== 'deep';
      for (const e of this.escorts) {
        if (e.doing === 'lost') continue;
        const d = dist(b.u.x, b.u.y, e.u.x, e.u.y);
        if (!up || d > reach * 1.2) { b.sees.delete(e.u.id); continue; }
        if (b.sees.has(e.u.id) || d > reach) continue;
        if (this.rng.chance(p)) b.sees.add(e.u.id);
      }
    }
    this.known.att.clear();
    for (const b of this.boats) if (b.doing !== 'out' && b.doing !== 'left' && b.doing !== 'lost') for (const id of b.sees) this.known.att.add(id);
  }

  // ---- Helpers --------------------------------------------------------------------------------------------------

  private nearestMerchant(x: number, y: number): Merchant | null {
    let best: Merchant | null = null, bd = Infinity;
    for (const m of this.merchants) { if (m.lost) continue; const d = dist(x, y, m.u.x, m.u.y); if (d < bd) { bd = d; best = m; } }
    return best;
  }

  private nearestEscort(x: number, y: number): Escort | null {
    let best: Escort | null = null, bd = Infinity;
    for (const e of this.escorts) { if (e.doing === 'lost') continue; const d = dist(x, y, e.u.x, e.u.y); if (d < bd) { bd = d; best = e; } }
    return best;
  }

  private steer(u: ConvoyUnit, x: number, y: number, speed: number, dt: number): void {
    const d = dist(u.x, u.y, x, y);
    if (d > 1) u.face = bearing(u.x, u.y, x, y);
    const step = Math.min(d, speed * dt);
    u.x += Math.sin(u.face) * step;
    u.y += Math.cos(u.face) * step;
    u.speed = Math.round((step / dt) * 100) / 100;
    u.act = u.speed > 0.2 ? 'march' : 'idle';
  }

  private noteContact(): void { if (this.contactAt === null) { this.contactAt = this.sec; this.events.push({ t: this.sec, what: 'contact', text: 'The convoy is under attack.' }); } }

  private say(what: FieldEvent['what'], u: ConvoyUnit, text: string, extra: Record<string, unknown>, cue?: string, side?: Side, x?: number, y?: number): void {
    const e: FieldEvent = { t: this.sec, what, unit: u.id, side: side ?? u.side, text, extra };
    if (cue) e.cue = cue;
    if (x !== undefined && y !== undefined) { e.x = x; e.y = y; }
    this.events.push(e);
  }

  /** What the scene reads off each piece each view (a boat's depth and hunters, an escort's echo and hunt). */
  private showLooks(): void {
    for (const b of this.boats) {
      const x = b.u.extra;
      x.dive = b.dive; x.keel = Math.round(b.keel * 10) / 10; x.scope = b.dive === 'periscope' && b.keel <= B.scope + 0.5; x.tubes = b.tubes;
      x.heard = this.heardBy(b);
      if (b.doing === 'up' || b.doing === 'gun') b.u.act = 'idle';
    }
    for (const e of this.escorts) {
      const x = e.u.extra;
      x.echo = e.echo ?? undefined;
      x.huntPhase = e.doing === 'search' ? 'search' : e.doing === 'run' || e.doing === 'attack' ? (e.echo && e.echo.kind === 'asdic' && e.echo.range <= E.blind ? 'blind' : 'run') : e.doing === 'open' ? 'open' : undefined;
      x.huntUntil = x.huntPhase ? e.huntFrom + (e.kit.groups ? E.groupFor : E.huntFor) : undefined;
    }
  }

  // ---- Orders -------------------------------------------------------------------------------------------------------

  command(c: Command | ConvoyCommand, by: Side): boolean {
    if (this.over) return false;
    if (c.do === 'auto') { this.ai[c.side] = c.on; return true; }
    if (c.do === 'dice') { this.handToStaff(); return true; }
    const units = 'units' in c && Array.isArray(c.units) ? (c.units as number[]) : [];
    let ok = false;
    for (const id of units) {
      const u = this.byId.get(id);
      if (!u || u.side !== by || !live(u) || (this.ticket.place.mine !== undefined && u.nat !== this.ticket.place.mine)) continue;
      if (u.kind === 'dd') ok = this.orderEscort(this.escortOf.get(id)!, c) || ok;
      else if (u.kind === 'ss') ok = this.orderBoat(this.boatOf.get(id)!, c) || ok;
    }
    return ok;
  }

  private orderEscort(e: Escort, c: Command | ConvoyCommand): boolean {
    if (e.doing === 'lost') return false;
    switch (c.do) {
      case 'convoy:hunt': case 'attack': {
        const b = this.boatOf.get(c.target);
        if (!b || !this.known.def.has(b.u.id)) return false;
        if (e.holds !== b.u.id) { if (e.holds >= 0) this.drop(e, 'faded'); e.holds = b.u.id; e.echo = null; }
        e.dx = b.u.x; e.dy = b.u.y; e.order = 'hunt';
        this.beginHunt(e, 'order');
        return true;
      }
      case 'convoy:screen': case 'hold': this.goBack(e); return true;
      case 'convoy:rescue': {
        const t = this.byId.get(c.target);
        if (!t || t.state !== 'gone') return false;
        this.goBack(e); e.doing = 'rescue'; e.order = 'rescue'; e.tx = t.x; e.ty = t.y; e.until = this.sec + 600 + Math.round(dist(e.u.x, e.u.y, t.x, t.y) / E.top);
        this.later.push({ at: e.until - 300, run: () => { if (e.doing === 'rescue') this.say('sea:rescue', e.u, `${e.u.name} picks up survivors.`, { on: t.id, by: e.u.id, wreck: { unit: t.id, ship: 1 } }, undefined, 'def', Math.round(e.u.x), Math.round(e.u.y)); } });
        return true;
      }
      case 'convoy:starshell': {
        if (this.sec < e.nextShell) return false;
        e.nextShell = this.sec + 180;
        this.say('sea:starshell', e.u, `${e.u.name} fires star shell.`, { by: e.u.id, x: Math.round(c.x), y: Math.round(c.y) }, 'starshell', 'def', Math.round(c.x), Math.round(c.y));
        this.zones.push({ id: ++this.zoneId, kind: 'starshell', x: c.x, y: c.y, r: 800, until: this.sec + 60, side: 'def', unit: e.u.id });
        return true;
      }
      case 'move': { if (e.holds >= 0) this.drop(e, 'faded'); e.doing = 'move'; e.order = 'move'; e.tx = c.x; e.ty = c.y; return true; }
      default: return false;
    }
  }

  private orderBoat(b: Boat, c: Command | ConvoyCommand): boolean {
    if (b.doing === 'out' || b.doing === 'left' || b.doing === 'lost') return false;
    switch (c.do) {
      case 'convoy:attack': case 'attack': {
        const target = 'target' in c && c.target !== undefined ? this.byId.get(c.target) : undefined;
        if (target && (target.side !== 'def' || !live(target))) return false;
        b.order = 'under' in c && c.under ? 'under' : 'attack'; b.target = target?.id ?? -1;
        if (b.doing !== 'up' && b.doing !== 'gun') { if (b.dive === 'deep') this.setDive(b, 'periscope', 'attack'); b.doing = 'close'; }
        return true;
      }
      case 'convoy:dive': {
        if (b.doing === 'up' && (b.doomed || b.harm >= 1)) return false;
        b.order = c.deep ? 'deep' : 'dive'; this.setDive(b, c.deep ? 'deep' : 'periscope', 'ordered'); b.doing = c.deep ? 'deep' : 'evade'; b.until = this.sec + 300; return true;
      }
      case 'convoy:surface': {
        if (b.doing === 'up' || b.doing === 'gun') return false;
        b.order = 'surface'; this.setDive(b, 'surfaced', 'ordered'); b.doing = b.tubes > 0 ? 'close' : 'away'; return true;
      }
      case 'convoy:evade': { b.order = 'evade'; if (b.dive === 'surfaced' && b.doing !== 'up' && b.doing !== 'gun') this.setDive(b, 'periscope', 'ordered'); if (b.doing !== 'up' && b.doing !== 'gun') { b.doing = 'evade'; b.until = this.sec + 600; } return true; }
      case 'convoy:gun': {
        const t = this.byId.get(c.target);
        if (!t || t.side !== 'def' || !live(t) || b.dive !== 'surfaced') return false;
        b.order = 'gun'; b.target = t.id; return true;
      }
      case 'convoy:leave': case 'withdraw': { if (b.doing === 'up' || b.doing === 'gun') return false; this.leave(b, 'ordered'); return true; }
      default: return false;
    }
  }

  /**
   * Hand it to the staff (ruling 217(4), b348): the rest of the night is settled at once, as the computer's own nights
   * for the card settle their rest from this hour on (calibrate.ts), rolled on the battle's stream.
   */
  private handToStaff(): void {
    this.handed = true;
    const rest = this.rest?.(this.sec);
    if (rest) {
      for (const [k, v] of Object.entries(rest.ss)) { const n = this.rng.roll(v); if (n) this.counts.ss[+k] = (this.counts.ss[+k] ?? 0) + n; }
      for (const [k, v] of Object.entries(rest.dd)) { const n = this.rng.roll(v); if (n) this.counts.dd[+k] = (this.counts.dd[+k] ?? 0) + n; }
      this.counts.mv += this.rng.roll(rest.mv);
    }
    this.finish('withdrew');
  }

  /** Dawn, or no boat left in it: the night ends. A hunt still alive at dawn ends in one last attack's roll. */
  private finish(how: 'time' | 'withdrew'): void {
    if (this.over) return;
    if (how === 'time' && !this.handed) {
      for (const b of this.boats) {
        if (b.doing === 'lost' || b.doing === 'left' || b.doing === 'out') continue;
        const hunters = this.huntersOf(b).filter((e) => e.echo?.kind === 'asdic');
        if (!hunters.length) continue;
        const e = hunters[0];
        if (this.rng.chance(attackKills({ ...e.kit, hedgehog: false }, hunters.length >= 2 || e.u.extra.ships > 1))) this.boatLost(b, 'crushed', e.u.id);
      }
    }
    const lostB = Object.values(this.counts.ss).reduce((s, n) => s + n, 0), lostE = Object.values(this.counts.dd).reduce((s, n) => s + n, 0);
    // A plain verdict for the screens; what the campaign books is the night's rank (calibrate.ts).
    const winner: Side = this.counts.mv > 3 * lostB + 2 * lostE ? 'att' : 'def';
    this.over = {
      winner, how, seconds: this.sec, R: winner === 'att' ? 1.2 : 0.9, // (words only: the night books by its ranks, calibrate.ts)
      lost: { att: lostB / Math.max(1, this.boats.length), def: (this.counts.mv + lostE) / Math.max(1, this.merchants.length + this.escorts.length) },
      men: { att: lostB * CONVOY.crew.ss, def: this.counts.mv * CONVOY.crew.mv },
      level: winner === 'att' ? 'Close victory' : 'Close defeat',
      lines: this.closing(lostB, lostE),
      forces: this.ticket.place.fleets.map((f) => ({ corps: f.id, side: f.side, lost: f.side === 'att' ? this.counts.ss[f.id] ?? 0 : this.counts.dd[f.id] ?? 0 })),
      extra: { counts: this.counts, tally: this.tally, handed: this.handed },
    };
    this.events.push({ t: this.sec, what: 'end', text: how === 'time' ? 'Dawn. The pack draws off.' : 'The night is over.' });
  }

  /** The after card's words: what the night cost each side (words only). */
  private closing(lostB: number, lostE: number): string[] {
    const n = (k: number, one: string, many: string): string => `${k === 0 ? 'No' : k} ${k === 1 ? one : many}`;
    const sunk = this.counts.mv, ships = this.merchants.length;
    const lines = [sunk === 0 ? 'The convoy comes through the night without loss.' : `${n(sunk, 'merchant', 'merchants')} of ${ships} sunk.`];
    lines.push(`${n(lostB, 'boat', 'boats')} lost of ${this.boats.length}${this.tally.scuttled ? `, ${this.tally.scuttled} scuttled by ${this.tally.scuttled === 1 ? 'her crew' : 'their crews'}` : ''}.`);
    if (lostE) lines.push(`${n(lostE, 'escort', 'escorts')} lost.`);
    if (this.handed) lines.push('The rest of the night was left to the staff.');
    return lines;
  }

  // ---- The picture --------------------------------------------------------------------------------------------------

  view(as?: Side): BattleView<ConvoyUnit> {
    this.showLooks();
    const events = this.events;
    this.events = [];
    const seen = (u: ConvoyUnit): boolean => {
      if (as === undefined || u.side === as) return true;
      if (u.kind === 'mv') return true; // the convoy is found: it is what the night is about
      return this.known[as].has(u.id) || u.state === 'gone';
    };
    const units = this.units.filter((u) => u.state !== 'withdrawn' || u.kind !== 'ss' || this.boatOf.get(u.id)!.doing === 'left').filter(seen).map((u) => ({ ...u, extra: { ...u.extra } }));
    const afloat = this.merchants.filter((m) => !m.lost).length / Math.max(1, this.merchants.length);
    return {
      t: this.t, limit: this.limit, units, balance: { att: 1 - afloat, def: afloat },
      events: as === undefined ? events : events.filter((e) => e.side === as || e.what === 'contact' || e.what === 'end' || e.what === 'sea:torpedoHit' || e.what === 'sea:sunk' || (e.unit !== undefined && this.byId.get(e.unit)?.side === as)),
      zones: as === undefined ? this.zones.slice() : this.zones.filter((z) => z.side === as || z.kind === 'oil' || z.kind === 'starshell'),
      ai: { ...this.ai }, over: this.over, contact: this.contactAt,
    };
  }
}
